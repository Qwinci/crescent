#include "kgdb.hpp"
#include "arch/cpu.hpp"
#include "dev/net/tcp.hpp"
#include "inttypes.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "sched/thread.hpp"

struct KgdbInfo {
	kstd::shared_ptr<Socket> sock;
};

void kgdb_serve(void* info_ptr) {
	{
		kstd::shared_ptr<Socket> sock;
		{
			auto* info = static_cast<KgdbInfo*>(info_ptr);
			sock = std::move(info->sock);
			delete info;
		}

		char reply_buf[512] {};
		usize reply_size = 0;

		auto generate_reply = [&](kstd::string_view data) {
			reply_buf[0] = '+';
			reply_buf[1] = '$';
			memcpy(reply_buf + 2, data.data(), data.size());

			u8 checksum = 0;
			for (auto c : data) {
				checksum += c;
			}

			static constexpr auto CHARS = "0123456789abcdef";
			reply_buf[2 + data.size()] = '#';
			reply_buf[2 + data.size() + 1] = CHARS[checksum / 16 % 16];
			reply_buf[2 + data.size() + 2] = CHARS[checksum % 16];
			reply_buf[2 + data.size() + 3] = 0;
			reply_size = data.size() + 5;
		};

		while (true) {
			char buf[512] {};
			usize size = sizeof(buf);
			int status = sock->receive(buf, size);
			if (status == ERR_CONNECTION_CLOSED) {
				println("[kernel][kgdb]: remote closed the connection");
				break;
			}
			else if (status != 0) {
				println("[kernel][kgdb]: failed to receive data: ", status);
				break;
			}

			kstd::string_view str {buf, size};

			println("[kernel][kgdb]: received packet '", buf, "'");

			usize offset = 0;
			while (offset < size) {
				if (str[offset] == '+') {
					++offset;
					continue;
				}
				else if (str[offset] == '$') {
					++offset;
					auto data_end = str.find('#', offset);
					assert(data_end != kstd::string_view::npos);
					auto data = str.substr(offset, data_end - offset);
					auto checksum_str = str.substr(data_end + 1, 2);

					auto expected_checksum = kstd::to_integer<u8>(checksum_str, 16);

					u8 checksum = 0;
					for (auto data_c : data) {
						checksum += data_c;
					}

					offset = data_end + 3;

					if (checksum != expected_checksum) {
						println("[kernel][kgdb]: discarding packet with invalid checksum");
						continue;
					}

					if (data == "?") {
						// SIGTRAP
						generate_reply("S05");
					}
					else {
						generate_reply("");
					}

					println("[kernel][kgdb]: reply with '", reply_buf, "'");
					sock->send(reply_buf, reply_size);
				}
				else {
					println("[kernel][kgdb]: received an invalid packet");
					break;
				}
			}
		}
	}

	IrqGuard irq_guard {};
	auto* cpu = get_current_thread()->cpu;
	cpu->scheduler.exit_thread(0);
}

void kgdb_listen(void*) {
	auto sock = tcp_socket_create(0);

	while (true) {
		int status = sock->listen(4321);
		assert(status == 0);

		kstd::shared_ptr<Socket> con;
		if ((status = sock->accept(con, 0)) != 0) {
			println("[kernel][kgdb]: failed to accept connection: ", status);
			continue;
		}

		auto info = new KgdbInfo {
			.sock {std::move(con)}
		};

		IrqGuard irq_guard {};
		auto* cpu = get_current_thread()->cpu;
		auto* thread = new Thread {"kgdb connection", cpu, &*KERNEL_PROCESS, kgdb_serve, info};
		cpu->scheduler.queue(thread);
		cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);
	}
}

void kgdb_init() {
	IrqGuard irq_guard {};
	// always called in kernel main
	auto* cpu = get_current_thread()->cpu;
	auto* thread = new Thread {"kgdb listener", cpu, &*KERNEL_PROCESS, kgdb_listen, nullptr};
	cpu->scheduler.queue(thread);
	cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);
}
