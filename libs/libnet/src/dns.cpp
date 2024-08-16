#include "net/dns.hpp"
#include <sys.hpp>
#include <cassert>
#include <string>
#include <cstring>
#include <concepts>
#include <bit>

namespace {
	struct Header {
		uint16_t transaction_id;
		uint16_t flags;
		uint16_t num_questions;
		uint16_t num_answers;
		uint16_t num_authority_rr;
		uint16_t num_additional_rr;
	};

	namespace flags {
		constexpr uint16_t RD = 1 << 0;
	}

	namespace qtype {
		constexpr uint16_t A = 1;
		constexpr uint16_t CNAME = 5;
		constexpr uint16_t PTR = 12;
		constexpr uint16_t AAAA = 28;
	}

	template<typename T>
	constexpr T to_be(T value) {
		if constexpr (std::endian::native == std::endian::little) {
			return std::byteswap(value);
		}
		else {
			return value;
		}
	}

	template<typename T>
	constexpr T to_ne_from_be(T value) {
		if constexpr (std::endian::native == std::endian::little) {
			return std::byteswap(value);
		}
		else {
			return value;
		}
	}

	struct Ptr {
		Ipv4 parse_ipv4() {
			uint32_t value;
			memcpy(&value, ptr, 4);
			ptr += 4;
			return {.value = value};
		}

		Ipv6 parse_ipv6() {
			Ipv6 addr {};
			memcpy(&addr.value, ptr, 16);
			ptr += 16;
			return addr;
		}

		std::string parse_dns_name() {
			std::string name;

			while (true) {
				auto len = static_cast<uint8_t>(*ptr++);
				if ((len & 0b11000000) == 0b11000000) {
					uint8_t str_offset = (len & 0b111111) << 8 | *ptr++;

					auto* ptr2 = base + str_offset;
					while (true) {
						auto len2 = static_cast<uint8_t>(*ptr2++);
						assert((len2 & 0b11000000) != 0b11000000);
						if (!len2) {
							break;
						}
						name += std::string_view {ptr2, len2};
						ptr2 += len2;
						if (*ptr2) {
							name += '.';
						}
					}

					break;
				}
				else {
					if (!len) {
						break;
					}
					name += std::string_view {ptr, len};
					ptr += len;
					if (*ptr) {
						name += '.';
					}
				}
			}

			return name;
		}

		const char* base;
		const char* ptr;
	};

	struct Query {
		explicit Query(uint16_t type) : type {type} {
			Header hdr {
				.transaction_id = to_be<uint16_t>(1234),
				.flags = flags::RD,
				.num_questions = to_be<uint16_t>(1),
				.num_answers = 0,
				.num_authority_rr = 0,
				.num_additional_rr = 0
			};

			req.resize(sizeof(hdr));
			memcpy(req.data(), &hdr, sizeof(hdr));
			transaction_id = hdr.transaction_id;
		}

		void add_segment(std::string_view segment) {
			assert(segment.size() <= 0xFF);
			req += static_cast<char>(segment.size());
			req += segment;
		}

		int send(void* response, size_t response_size) {
			req += '\0';

			// qtype
			req += static_cast<char>(type >> 8);
			req += static_cast<char>(type & 0xFF);
			// class in
			req += static_cast<char>(0);
			req += static_cast<char>(1);

			CrescentHandle socket_handle;
			auto res = sys_socket_create(socket_handle, SOCKET_TYPE_UDP, 0);
			assert(res == 0);
			Ipv4SocketAddress addr {
				.generic {
					.type = SOCKET_ADDRESS_TYPE_IPV4
				},
				.ipv4 = 8 | 8 << 8 | 8 << 16 | 8 << 24,
				.port = 53
			};
			res = sys_socket_send_to(socket_handle, req.data(), req.size(), addr.generic);
			assert(res == 0);

			char resp[256];
			size_t received;
			res = sys_socket_receive_from(socket_handle, resp, sizeof(resp), received, addr.generic);
			assert(res == 0);

			sys_close_handle(socket_handle);

			if (received < sizeof(Header)) {
				return ERR_INVALID_ARGUMENT;
			}

			auto* res_hdr = reinterpret_cast<Header*>(resp);
			if (res_hdr->transaction_id != transaction_id) {
				return ERR_INVALID_ARGUMENT;
			}

			if (response_size < received) {
				return ERR_BUFFER_TOO_SMALL;
			}
			memcpy(response, resp, received);
			return 0;
		}

		template<typename F> requires requires(F func, Ptr data_ptr, uint16_t qtype) {
			{ func(qtype, data_ptr) } -> std::same_as<int>;
		}
		int send(F on_answer) {
			char res[256];
			if (auto err = send(res, sizeof(res))) {
				return err;
			}

			auto* res_hdr = reinterpret_cast<Header*>(res);
			auto* ptr = reinterpret_cast<const char*>(&res_hdr[1]);
			for (int i = 0; i < to_ne_from_be(res_hdr->num_questions); ++i) {
				while (true) {
					auto len = static_cast<uint8_t>(*ptr++);
					if (len == 0) {
						break;
					}
					ptr += len;
				}
				ptr += 4;
			}

			for (int i = 0; i < to_ne_from_be(res_hdr->num_answers); ++i) {
				Ptr dns_ptr {
					.base = res,
					.ptr = ptr
				};
				dns_ptr.parse_dns_name();
				ptr = dns_ptr.ptr;
				uint16_t ans_type = ptr[0] << 8 | ptr[1];
				uint16_t rd_len = ptr[8] << 8 | ptr[9];
				ptr += 10;

				dns_ptr.ptr = ptr;
				if (auto status = on_answer(ans_type, dns_ptr)) {
					return status;
				}
				ptr += rd_len;
			}

			return 0;
		}

		std::string req;
		uint16_t type;
		uint16_t transaction_id;
	};
}

std::optional<Ipv4> dns::resolve4(std::string_view domain) {
	Query query {qtype::A};

	size_t offset = 0;
	while (true) {
		auto next_offset = domain.find('.', offset);
		auto chunk = domain.substr(offset, next_offset - offset);
		query.add_segment(chunk);
		if (next_offset == std::string_view::npos) {
			break;
		}
		else {
			offset = next_offset + 1;
		}
	}

	Ipv4 res_ip {};
	int err = query.send([&](uint16_t type, Ptr ptr) {
		assert(type == qtype::A);
		res_ip = ptr.parse_ipv4();
		return 1;
	});
	if (err == 1) {
		return res_ip;
	}
	else {
		return std::nullopt;
	}
}
