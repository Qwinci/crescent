#include "console.hpp"
#include "debug.hpp"
#include "drivers/net/ethernet.hpp"
#include "drivers/net/ip.hpp"
#include "drivers/net/packet.hpp"
#include "drivers/net/udp.hpp"
#include "memory/std.hpp"

static void debug_send_packet(Nic* nic, Packet& in_packet, const char* data, u16 size, const u8 (&src)[6]) {
	Packet packet {};
	ethernet_create_hdr(nic, packet, ETH_IPV4, src);

	u16 udp_size = size;

	u16 ip_size = sizeof(UdpHeader) + udp_size;
	ipv4_create_hdr(packet, ip_size, IP_UDP, nic->ipv4.whole, in_packet.second.ipv4->src_ip_addr);
	packet.second.ipv4->serialize();
	packet.second.ipv4->update_checksum();

	u16 src_port = in_packet.third.udp->dst_port;
	u16 dst_port = in_packet.third.udp->src_port;
	udp_create_hdr(packet, src_port, dst_port, udp_size);
	packet.third.udp->serialize();

	auto p_data = packet.third.udp->data;
	memcpy(p_data, data, size);
	packet.end += udp_size;

	nic->send(packet.begin, packet.size());
}

struct GdbPacket {
	noalloc::String data {"", 0};
	u8 checksum {};
};

static char to_lower(char c) {
	return as<char>(c | 1 << 5);
}

static char hex_digits[] = "0123456789abcdef";

u8 parse_hex8(const char* s, u8 count) {
	u8 value = 0;
	for (u8 i = 0; i < count; ++i) {
		u8 val = s[i] <= '9' ? (s[i] - '0') : (10 + to_lower(s[i]) - 'a');
		value *= 16;
		value += val;
	}
	return value;
}

static GdbPacket* parse(const char* data, usize size) {
	static GdbPacket packet {};
	if (*data++ != '$') {
		return nullptr;
	}

	auto start = data;
	while (size && *data != '#') ++data, --size;
	auto end = data;

	if (size < 2) {
		return nullptr;
	}
	++data;

	packet.checksum = parse_hex8(data, 2);

	packet.data = noalloc::String {start, as<usize>(end - start)};
	return &packet;
}

static u8 checksum(const char* data, usize size) {
	usize sum = 0;
	for (usize i = 0; i < size; ++i) {
		sum += data[i];
	}
	return sum % 256;
}

static usize hex64(u64 value, char* str) {
	if (!value) {
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		*str++ = '0';
		return 16;
	}

	auto s = str;
	//s += 16 - 1;

	while (value) {
		auto digit = hex_digits[value % 16];
		//*s-- = digit;
		*s++ = digit;
		value /= 16;
	}

	/*while (s >= str) {
		*s-- = '0';
	}*/
	while (s < str + 16) {
		*s++ = '0';
	}

	return 16;
}

enum class Reg {
	Rax,
	Rbx,
	Rcx,
	Rdx,
	Rsi,
	Rdi,
	Rbp,
	Rsp,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
	Rip,
	EFlags,
	Ds,
	Es,
	Fs,
	Gs,
	Max
};

static u64 read_reg_64(Reg reg) {
	u64 value;
	switch (reg) {
		case Reg::Rax:
			asm volatile("mov %0, rax" : "=rm"(value));
			break;
		case Reg::Rbx:
			asm volatile("mov %0, rbx" : "=rm"(value));
			break;
		case Reg::Rcx:
			asm volatile("mov %0, rcx" : "=rm"(value));
			break;
		case Reg::Rdx:
			asm volatile("mov %0, rdx" : "=rm"(value));
			break;
		case Reg::Rsi:
			asm volatile("mov %0, rsi" : "=rm"(value));
			break;
		case Reg::Rdi:
			asm volatile("mov %0, rdi" : "=rm"(value));
			break;
		case Reg::Rbp:
			asm volatile("mov %0, rbp" : "=rm"(value));
			break;
		case Reg::Rsp:
			asm volatile("mov %0, rsp" : "=rm"(value));
			break;
		case Reg::R8:
			asm volatile("mov %0, r8" : "=rm"(value));
			break;
		case Reg::R9:
			asm volatile("mov %0, r9" : "=rm"(value));
			break;
		case Reg::R10:
			asm volatile("mov %0, r10" : "=rm"(value));
			break;
		case Reg::R11:
			asm volatile("mov %0, r11" : "=rm"(value));
			break;
		case Reg::R12:
			asm volatile("mov %0, r12" : "=rm"(value));
			break;
		case Reg::R13:
			asm volatile("mov %0, r13" : "=rm"(value));
			break;
		case Reg::R14:
			asm volatile("mov %0, r14" : "=rm"(value));
			break;
		case Reg::R15:
			asm volatile("mov %0, r15" : "=rm"(value));
			break;
		case Reg::Rip:
			asm volatile("lea %0, [rip]" : "=r"(value));
			break;
		case Reg::EFlags:
			value = 0;
			// todo
			break;
		default:
			value = 0;
	}
	return value;
}

void debug_process_packet(Nic* nic, Packet& in_packet, u8* data, u16 size, const u8 (&src)[6]) {
	const char* str = cast<const char*>(data);

	char buffer[256] {};
	char* ptr = buffer;

	auto generate_reply = [&](const char* reply, usize size) {
		if (ptr + size >= buffer + 256 / 2) {
			debug_send_packet(nic, in_packet, buffer, ptr - buffer, src);
			ptr = buffer;
		}

		*ptr++ = '$';
		memcpy(ptr, reply, size);
		ptr += size;
		auto sum = checksum(reply, size);
		*ptr++ = '#';
		*ptr++ = hex_digits[sum / 16 % 16];
		*ptr++ = hex_digits[sum % 16];
	};

	while (size) {
		if (ptr >= buffer + 256 / 2) {
			debug_send_packet(nic, in_packet, buffer, ptr - buffer, src);
			ptr = buffer;
		}

		if (*str == '+') {
			println("gdb: ack");
			--size;
			++str;
		}
		else if (auto gdb = parse(str, size)) {
			println("gdb data: ", gdb->data);
			*ptr++ = '+';
			if (gdb->data == "?") {
				generate_reply("S05", 3);
			}
			else if (gdb->data == "g") {
				char reg_buf[16 * as<u16>(Reg::Max)];
				char* reg_buf_ptr = reg_buf;
				auto rax = read_reg_64(Reg::Rax);
				auto rbx = read_reg_64(Reg::Rbx);
				auto rcx = read_reg_64(Reg::Rcx);
				auto rdx = read_reg_64(Reg::Rdx);
				auto rsi = read_reg_64(Reg::Rsi);
				auto rdi = read_reg_64(Reg::Rdi);
				auto rbp = read_reg_64(Reg::Rbp);
				auto rsp = read_reg_64(Reg::Rsp);
				auto r8 = read_reg_64(Reg::R8);
				auto r9 = read_reg_64(Reg::R9);
				auto r10 = read_reg_64(Reg::R10);
				auto r11 = read_reg_64(Reg::R11);
				auto r12 = read_reg_64(Reg::R12);
				auto r13 = read_reg_64(Reg::R13);
				auto r14 = read_reg_64(Reg::R14);
				auto r15 = read_reg_64(Reg::R15);
				auto rip = read_reg_64(Reg::Rip);

				reg_buf_ptr += hex64(rax, reg_buf_ptr);

				reg_buf_ptr += hex64(rbx, reg_buf_ptr);

				reg_buf_ptr += hex64(rcx, reg_buf_ptr);

				reg_buf_ptr += hex64(rdx, reg_buf_ptr);

				reg_buf_ptr += hex64(rsi, reg_buf_ptr);

				reg_buf_ptr += hex64(rdi, reg_buf_ptr);

				reg_buf_ptr += hex64(rbp, reg_buf_ptr);

				reg_buf_ptr += hex64(rsp, reg_buf_ptr);

				reg_buf_ptr += hex64(r8, reg_buf_ptr);

				reg_buf_ptr += hex64(r9, reg_buf_ptr);

				reg_buf_ptr += hex64(r10, reg_buf_ptr);

				reg_buf_ptr += hex64(r11, reg_buf_ptr);

				reg_buf_ptr += hex64(r12, reg_buf_ptr);

				reg_buf_ptr += hex64(r13, reg_buf_ptr);

				reg_buf_ptr += hex64(r14, reg_buf_ptr);

				reg_buf_ptr += hex64(r15, reg_buf_ptr);

				reg_buf_ptr += hex64(rip, reg_buf_ptr);

				generate_reply(reg_buf, sizeof(reg_buf));
			}
			else if (gdb->data[0] == 'p') {
				auto num = parse_hex8(gdb->data.data() + 1, gdb->data.len() - 1);
				char small_buf[16] {};
				if (num < as<u16>(Reg::Max)) {
					auto reg = as<Reg>(num);
					auto value = read_reg_64(reg);
					hex64(value, small_buf);
				}

				generate_reply(small_buf, 16);
			}
			else {
				generate_reply("", 0);
			}
			size -= gdb->data.len() + 4;
			str += gdb->data.len() + 4;
		}
		else {
			println("unknown gdb data: ", noalloc::String {str, size});
			break;
		}
	}

	if (ptr != buffer) {
		debug_send_packet(nic, in_packet, buffer, ptr - buffer, src);
	}
}