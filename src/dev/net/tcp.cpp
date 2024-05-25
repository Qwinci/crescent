#include "tcp.hpp"
#include "arch/cpu.hpp"
#include "arp.hpp"
#include "checksum.hpp"
#include "dev/net/nic/nic.hpp"
#include "dev/random.hpp"
#include "manually_destroy.hpp"
#include "mem/register.hpp"
#include "packet.hpp"
#include "ring_buffer.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "sys/socket.hpp"
#include "unique_ptr.hpp"

namespace flags {
	static constexpr BitField<u16, bool> FIN {0, 1};
	static constexpr BitField<u16, bool> SYN {1, 1};
	static constexpr BitField<u16, bool> RST {2, 1};
	static constexpr BitField<u16, bool> PSH {3, 1};
	static constexpr BitField<u16, bool> ACK {4, 1};
	static constexpr BitField<u16, bool> URG {5, 1};
	static constexpr BitField<u16, bool> ECE {6, 1};
	static constexpr BitField<u16, bool> CWR {7, 1};
	static constexpr BitField<u16, u8> DATA_OFFSET {12, 4};
}

struct PseudoHeader {
	u32 src_ip;
	u32 dest_ip;
	u8 zero;
	IpProtocol protocol;
	u16 header_payload_size;
};

struct TcpHeader {
	u16 src_port {};
	u16 dest_port {};
	u32 sequence {};
	u32 ack_number {};
	BitValue<u16> flags {};
	u16 window_size {};
	u16 checksum {};
	u16 urgent_ptr {};

	void serialize() {
		src_port = kstd::to_be(src_port);
		dest_port = kstd::to_be(dest_port);
		sequence = kstd::to_be(sequence);
		ack_number = kstd::to_be(ack_number);
		flags.value = kstd::to_be(flags.value);
		window_size = kstd::to_be(window_size);
		urgent_ptr = kstd::to_be(urgent_ptr);
	}

	void deserialize() {
		src_port = kstd::to_ne_from_be(src_port);
		dest_port = kstd::to_ne_from_be(dest_port);
		sequence = kstd::to_ne_from_be(sequence);
		ack_number = kstd::to_ne_from_be(ack_number);
		flags.value = kstd::to_ne_from_be(flags.value);
		window_size = kstd::to_ne_from_be(window_size);
		urgent_ptr = kstd::to_ne_from_be(urgent_ptr);
	}

	void calculate_checksum(u32 src_ip, u32 dest_ip, u16 header_size, const void* data, u16 data_size) {
		checksum = 0;

		PseudoHeader pseudo {
			.src_ip = src_ip,
			.dest_ip = dest_ip,
			.zero = 0,
			.protocol = IpProtocol::Tcp,
			.header_payload_size = kstd::to_be(static_cast<u16>(header_size + data_size))
		};

		Checksum sum;
		sum.add(this, header_size);
		sum.add(data, data_size);
		sum.add(&pseudo, sizeof(PseudoHeader));
		auto res = sum.get();

		checksum = res;
	}
};

struct Tcp4Socket;

static void remove_socket(Tcp4Socket* socket);

struct Tcp4Socket : public Socket {
	explicit Tcp4Socket(int flags) : Socket {flags} {
		IrqGuard irq_guard {};
		handler_thread->cpu->scheduler.queue(handler_thread);
	}

	int disconnect_helper() {
		if (state != State::Connected) {
			return 0;
		}

		do_disconnect = true;
		send_event.signal_one();
		while (state != State::None) {
			state_change_event.wait();
		}

		return 0;
	}

	~Tcp4Socket() override {
		disconnect_helper();
		remove_socket(this);
		// todo yield
		while (!send_event.is_being_waited());

		IrqGuard irq_guard {};
		// todo this is not thread-safe
		handler_thread->exited = true;
		handler_thread->exit(0);
		handler_thread->status = Thread::Status::Blocked;
	}

	int connect(AnySocketAddress& address) override {
		if (address.generic.type != SOCKET_ADDRESS_TYPE_IPV4) {
			return ERR_UNSUPPORTED;
		}
		u32 addr = address.ipv4.ipv4;
		u16 port = address.ipv4.port;

		u16 src_port = 0;
		random_generate(&src_port, 2);

		u32 new_sequence = 0;
		random_generate(&new_sequence, 4);
		sequence = new_sequence + 1;

		{
			IrqGuard irq_guard {};
			// todo support choosing the nic
			auto nic_guard = NICS->lock();

			own_ip = (*nic_guard->front())->ip;
			own_port = src_port;

			target = address.ipv4;

			Packet packet {sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader)};

			packet.add_ethernet((*(*nic_guard).front())->mac, BROADCAST_MAC, EtherType::Ipv4);
			packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader), own_ip, addr);

			auto* hdr_ptr = packet.add_header(sizeof(TcpHeader));
			TcpHeader hdr {
				.src_port = src_port,
				.dest_port = port,
				.sequence = new_sequence,
				.ack_number = 0,
				.flags {flags::SYN(true) | flags::DATA_OFFSET(5)},
				.window_size = static_cast<u16>(1024 * 64 - 1),
				.checksum = 0,
				.urgent_ptr = 0
			};
			hdr.serialize();
			hdr.calculate_checksum(own_ip, addr, sizeof(TcpHeader), nullptr, 0);
			memcpy(hdr_ptr, &hdr, sizeof(hdr));

			(*nic_guard->front())->send(packet.data, packet.size);

			state = State::SentSyn;
		}

		while (state != State::Connected) {
			state_change_event.wait();
		}

		return 0;
	}

	int disconnect() override {
		return disconnect_helper();
	}

	int listen(uint32_t port) override {
		listen_event.reset();
		own_port = port;
		state = State::Listening;
		listen_event.wait();
		state = State::None;
		return 0;
	}

	int accept(kstd::shared_ptr<Socket>& connection, int connection_flags) override {
		if (!pending_connection_valid) {
			return ERR_TRY_AGAIN;
		}

		pending_connection_valid = false;

		kstd::shared_ptr<Tcp4Socket> new_socket {tcp_socket_create(connection_flags)};
		new_socket->state = State::SynAck;
		new_socket->own_port = pending_connection.own_port;
		new_socket->own_ip = pending_connection.own_ip;
		new_socket->target.ipv4 = pending_connection.target_ip;
		new_socket->target.port = pending_connection.target_port;
		new_socket->received_sequence = pending_connection.sequence;

		new_socket->send_event.signal_one();
		while (new_socket->state != State::Connected && new_socket->state != State::None) {
			new_socket->state_change_event.wait();
		}

		connection = std::move(new_socket);

		return 0;
	}

	int send(const void* data, usize size) override {
		if (state != State::Connected) {
			return ERR_UNSUPPORTED;
		}
		send_buffer.write_block(data, size);
		send_event.signal_one();
		// todo on non-blocking socket this should return the amount of bytes actually written
		return 0;
	}

	int receive(void* data, usize& size) override {
		if (state != State::Connected) {
			return ERR_UNSUPPORTED;
		}

		size = receive_buffer.read(data, size);
		return 0;
	}

	int get_peer_name(AnySocketAddress& address) override {
		if (state != State::Connected) {
			return ERR_UNSUPPORTED;
		}
		address.ipv4 = target;
		address.generic.type = SOCKET_ADDRESS_TYPE_IPV4;
		return 0;
	}

	void process_packet(ReceivedPacket& packet, const TcpHeader& hdr) {
		if (expect_fin_ack) {
			if (!(hdr.flags & flags::ACK) || !(hdr.flags & flags::FIN)) {
				println("[kernel][tcp]: expected a fin-ack in reply to fin");
				return;
			}
			if (hdr.sequence != received_sequence + 1) {
				println("[kernel][tcp]: invalid sequence in fin reply ack");
				return;
			}
			if (hdr.ack_number != sequence) {
				println("[kernel][tcp]: invalid ack number in fin reply ack");
				return;
			}

			expect_fin_ack = false;
			return;
		}

		if (state == State::Listening) {
			if (hdr.flags & flags::SYN) {
				if (pending_connection_valid) {
					return;
				}

				println("[kernel][tcp]: new connection to port ", hdr.dest_port);

				pending_connection = {
					.sequence = hdr.sequence,
					.target_ip = packet.layer1.ipv4.src_addr,
					.own_ip = packet.layer1.ipv4.dest_addr,
					.target_port = hdr.src_port,
					.own_port = hdr.dest_port
				};
				pending_connection_valid = true;
				listen_event.signal_one();
			}
		}
		else if (state == State::SynAck) {
			if (!(hdr.flags & flags::ACK)) {
				println("[kernel][tcp]: expected an ack in reply to syn-ack");
				return;
			}
			if (hdr.sequence != received_sequence + 1) {
				println("[kernel][tcp]: invalid sequence in syn-ack reply ack");
				return;
			}
			if (hdr.ack_number != sequence) {
				println("[kernel][tcp]: invalid ack number in syn-ack reply ack");
				return;
			}
			println("[kernel][tcp]: state -> Connected");
			state = State::Connected;
			state_change_event.signal_all();
		}
		else if (state == State::SentSyn) {
			if (!(hdr.flags & flags::SYN) || !(hdr.flags & flags::ACK)) {
				println("[kernel][tcp]: expected syn-ack");
				return;
			}
			if (hdr.ack_number != sequence) {
				println("[kernel][tcp]: invalid ack number in syn-ack");
				return;
			}

			received_sequence = hdr.sequence;

			state = State::ReceivedSynAck;
			send_event.signal_one();
		}
		else if (state == State::Connected) {
			if (hdr.flags & flags::FIN) {
				state = State::ReceivedFin;
				send_event.signal_one();
				return;
			}

			if (!(hdr.flags & flags::ACK)) {
				println("[kernel][tcp]: expected ack to be set in a data packet");
				return;
			}
			if (hdr.sequence <= received_sequence) {
				println("[kernel][tcp]: ignoring already received sequence ", hdr.sequence);
				return;
			}
			else if (hdr.sequence != received_sequence + 1) {
				println("[kernel][tcp]: invalid sequence in a data packet");
				return;
			}

			if (hdr.ack_number != sequence) {
				println("[kernel][tcp]: invalid ack number in a data packet");
				return;
			}

			u16 hdr_len = (hdr.flags & flags::DATA_OFFSET) * 4;
			u16 data_len = packet.layer2_len - hdr_len;

			received_sequence += data_len;

			auto* data = offset(packet.layer2.raw, const char*, hdr_len);

			// todo out of order data
			if (receive_buffer.size() + data_len > 1024 * 64) {
				println("[kernel][tcp]: discarding packet because receive buffer is full");
				return;
			}
			receive_buffer.write_block(data, data_len);
			send_event.signal_one();
		}
		else if (state == State::SentFin) {
			if (!(hdr.flags & flags::ACK) || !(hdr.flags & flags::FIN)) {
				println("[kernel][tcp]: expected a fin-ack in reply to fin");
				return;
			}
			if (hdr.sequence != received_sequence + 1) {
				println("[kernel][tcp]: invalid sequence in fin reply ack");
				return;
			}
			if (hdr.ack_number != sequence) {
				println("[kernel][tcp]: invalid ack number in fin reply ack");
				return;
			}

			state = State::None;
			state_change_event.signal_one();
		}
		else {
			println("[kernel][tcp]: unhandled receive state");
		}
	}

	[[noreturn]] static void handler_fn(void* ptr) {
		auto* self = static_cast<Tcp4Socket*>(ptr);

		while (true) {
			self->send_event.wait();
			if (self->state == State::SynAck) {
				Packet packet {sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader)};

				auto mac = arp_get_mac(self->target.ipv4);
				if (!mac) {
					println("[kernel][tcp]: no ip->mac mapping for target");
				}

				IrqGuard irq_guard {};
				// todo support choosing the nic
				auto nic_guard = NICS->lock();

				packet.add_ethernet((*(*nic_guard).front())->mac, mac.value(), EtherType::Ipv4);
				packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader), self->own_ip, self->target.ipv4);

				u32 new_sequence = 0;
				random_generate(&new_sequence, 4);

				self->sequence = new_sequence + 1;

				auto* hdr_ptr = packet.add_header(sizeof(TcpHeader));
				TcpHeader hdr {
					.src_port = self->own_port,
					.dest_port = self->target.port,
					.sequence = new_sequence,
					.ack_number = self->received_sequence + 1,
					.flags {flags::SYN(true) | flags::ACK(true) | flags::DATA_OFFSET(5)},
					.window_size = static_cast<u16>(1024 * 64 - 1),
					.checksum = 0,
					.urgent_ptr = 0
				};
				hdr.serialize();
				hdr.calculate_checksum(self->own_ip, self->target.ipv4, sizeof(TcpHeader), nullptr, 0);
				memcpy(hdr_ptr, &hdr, sizeof(hdr));

				(*nic_guard->front())->send(packet.data, packet.size);

				println("[kernel][tcp]: sending syn-ack");
			}
			else if (self->state == State::Connected) {
				if (self->acked_sequence != self->received_sequence) {
					self->acked_sequence = self->received_sequence;

					println("[kernel][tcp]: ack sequence ", self->received_sequence);

					Packet packet {sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader)};

					auto mac = arp_get_mac(self->target.ipv4);
					if (!mac) {
						println("[kernel][tcp]: no ip->mac mapping for target");
					}

					IrqGuard irq_guard {};
					// todo support choosing the nic
					auto nic_guard = NICS->lock();

					packet.add_ethernet((*(*nic_guard).front())->mac, mac.value(), EtherType::Ipv4);
					packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader), self->own_ip, self->target.ipv4);

					auto* hdr_ptr = packet.add_header(sizeof(TcpHeader));
					TcpHeader hdr {
						.src_port = self->own_port,
						.dest_port = self->target.port,
						.sequence = self->sequence,
						.ack_number = self->received_sequence + 1,
						.flags {flags::ACK(true) | flags::DATA_OFFSET(5)},
						.window_size = static_cast<u16>(1024 * 64 - 1),
						.checksum = 0,
						.urgent_ptr = 0
					};
					hdr.serialize();
					hdr.calculate_checksum(self->own_ip, self->target.ipv4, sizeof(TcpHeader), nullptr, 0);
					memcpy(hdr_ptr, &hdr, sizeof(hdr));

					(*nic_guard->front())->send(packet.data, packet.size);
				}

				if (auto size = self->send_buffer.size()) {
					u32 to_send = kstd::min(size, usize {1024 * 64 - sizeof(Ipv4Header) - sizeof(TcpHeader)});

					kstd::vector<u8> buffer;
					buffer.resize(to_send);
					self->send_buffer.read_block(buffer.data(), to_send);

					auto mac = arp_get_mac(self->target.ipv4);
					if (!mac) {
						println("[kernel][tcp]: no ip->mac mapping for target");
					}

					IrqGuard irq_guard {};
					// todo support choosing the nic
					auto nic_guard = NICS->lock();

					u32 packet_size = sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader) + to_send;
					Packet packet {packet_size};
					packet.add_ethernet((*(*nic_guard).front())->mac, mac.value(), EtherType::Ipv4);
					packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader) + to_send, self->own_ip, self->target.ipv4);

					auto* hdr_ptr = packet.add_header(sizeof(TcpHeader));
					TcpHeader hdr {
						.src_port = self->own_port,
						.dest_port = self->target.port,
						.sequence = self->sequence,
						.ack_number = self->received_sequence + 1,
						.flags {flags::ACK(true) | flags::DATA_OFFSET(5)},
						.window_size = static_cast<u16>(1024 * 64 - 1),
						.checksum = 0,
						.urgent_ptr = 0
					};

					self->sequence += to_send;

					auto* data_ptr = packet.add_header(to_send);
					memcpy(data_ptr, buffer.data(), to_send);

					hdr.serialize();
					hdr.calculate_checksum(self->own_ip, self->target.ipv4, sizeof(TcpHeader), data_ptr, to_send);
					memcpy(hdr_ptr, &hdr, sizeof(hdr));

					(*nic_guard->front())->send(packet.data, packet.size);
				}

				if (self->do_disconnect) {
					auto mac = arp_get_mac(self->target.ipv4);
					if (!mac) {
						println("[kernel][tcp]: no ip->mac mapping for target");
					}

					IrqGuard irq_guard {};
					// todo support choosing the nic
					auto nic_guard = NICS->lock();

					Packet fin_packet {sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader)};
					fin_packet.add_ethernet((*(*nic_guard).front())->mac, mac.value(), EtherType::Ipv4);
					fin_packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader), self->own_ip, self->target.ipv4);

					auto* hdr_ptr2 = fin_packet.add_header(sizeof(TcpHeader));
					TcpHeader hdr2 {
						.src_port = self->own_port,
						.dest_port = self->target.port,
						.sequence = self->sequence,
						.ack_number = self->received_sequence + 1,
						.flags {flags::FIN(true) | flags::DATA_OFFSET(5)},
						.window_size = static_cast<u16>(1024 * 64 - 1),
						.checksum = 0,
						.urgent_ptr = 0
					};
					hdr2.serialize();
					hdr2.calculate_checksum(self->own_ip, self->target.ipv4, sizeof(TcpHeader), nullptr, 0);
					memcpy(hdr_ptr2, &hdr2, sizeof(hdr2));

					++self->sequence;

					(*nic_guard->front())->send(fin_packet.data, fin_packet.size);

					println("[kernel][tcp]: sending fin");

					self->do_disconnect = false;
					self->expect_fin_ack = true;
				}
			}
			else if (self->state == State::ReceivedFin) {
				Packet packet {sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader)};

				auto mac = arp_get_mac(self->target.ipv4);
				if (!mac) {
					println("[kernel][tcp]: no ip->mac mapping for target");
				}

				IrqGuard irq_guard {};
				// todo support choosing the nic
				auto nic_guard = NICS->lock();

				packet.add_ethernet((*(*nic_guard).front())->mac, mac.value(), EtherType::Ipv4);
				packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader), self->own_ip, self->target.ipv4);

				auto* hdr_ptr = packet.add_header(sizeof(TcpHeader));
				TcpHeader hdr {
					.src_port = self->own_port,
					.dest_port = self->target.port,
					.sequence = self->sequence,
					.ack_number = self->received_sequence + 1,
					.flags {flags::FIN(true) | flags::ACK(true) | flags::DATA_OFFSET(5)},
					.window_size = static_cast<u16>(1024 * 64 - 1),
					.checksum = 0,
					.urgent_ptr = 0
				};
				hdr.serialize();
				hdr.calculate_checksum(self->own_ip, self->target.ipv4, sizeof(TcpHeader), nullptr, 0);
				memcpy(hdr_ptr, &hdr, sizeof(hdr));

				++self->sequence;

				(*nic_guard->front())->send(packet.data, packet.size);

				println("[kernel][tcp]: sending fin-ack");

				Packet fin_packet {sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader)};
				fin_packet.add_ethernet((*(*nic_guard).front())->mac, mac.value(), EtherType::Ipv4);
				fin_packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader), self->own_ip, self->target.ipv4);

				auto* hdr_ptr2 = fin_packet.add_header(sizeof(TcpHeader));
				TcpHeader hdr2 {
					.src_port = self->own_port,
					.dest_port = self->target.port,
					.sequence = self->sequence,
					.ack_number = self->received_sequence + 1,
					.flags {flags::FIN(true) | flags::DATA_OFFSET(5)},
					.window_size = static_cast<u16>(1024 * 64 - 1),
					.checksum = 0,
					.urgent_ptr = 0
				};
				hdr2.serialize();
				hdr2.calculate_checksum(self->own_ip, self->target.ipv4, sizeof(TcpHeader), nullptr, 0);
				memcpy(hdr_ptr2, &hdr2, sizeof(hdr2));

				++self->sequence;

				(*nic_guard->front())->send(fin_packet.data, fin_packet.size);

				println("[kernel][tcp]: sending fin");

				self->state = State::SentFin;
			}
			else if (self->state == State::ReceivedSynAck) {
				Packet packet {sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader)};

				auto mac = arp_get_mac(self->target.ipv4);
				if (!mac) {
					println("[kernel][tcp]: no ip->mac mapping for target");
				}

				IrqGuard irq_guard {};
				// todo support choosing the nic
				auto nic_guard = NICS->lock();

				packet.add_ethernet((*(*nic_guard).front())->mac, mac.value(), EtherType::Ipv4);
				packet.add_ipv4(IpProtocol::Tcp, sizeof(TcpHeader), self->own_ip, self->target.ipv4);

				auto* hdr_ptr = packet.add_header(sizeof(TcpHeader));
				TcpHeader hdr {
					.src_port = self->own_port,
					.dest_port = self->target.port,
					.sequence = self->sequence,
					.ack_number = self->received_sequence + 1,
					.flags {flags::ACK(true) | flags::DATA_OFFSET(5)},
					.window_size = static_cast<u16>(1024 * 64 - 1),
					.checksum = 0,
					.urgent_ptr = 0
				};
				hdr.serialize();
				hdr.calculate_checksum(self->own_ip, self->target.ipv4, sizeof(TcpHeader), nullptr, 0);
				memcpy(hdr_ptr, &hdr, sizeof(hdr));

				(*nic_guard->front())->send(packet.data, packet.size);

				println("[kernel][tcp]: sending ack");

				self->state = State::Connected;
				self->state_change_event.signal_one();
			}
		}
	}

	struct Connection {
		u32 sequence;
		u32 target_ip;
		u32 own_ip;
		u16 target_port;
		u16 own_port;
	};
	Connection pending_connection {};
	bool pending_connection_valid {};
	bool do_disconnect {};
	bool expect_fin_ack {};

	Thread* create_handler_thread() {
		IrqGuard irq_guard {};
		auto* thread = new Thread {"tcp handler", get_current_thread()->cpu, &*KERNEL_PROCESS, handler_fn, this};
		return thread;
	}

	RingBuffer<u8> send_buffer {1024 * 64};
	RingBuffer<u8> receive_buffer {1024 * 64};
	Ipv4SocketAddress target {};
	Event listen_event {};
	Event send_event {};
	Event state_change_event {};
	Thread* handler_thread {create_handler_thread()};
	u32 own_ip {};
	u16 own_port {};
	u32 sequence {};
	u32 received_sequence {};
	u32 acked_sequence {};
	enum class State {
		None,
		SynAck,
		ReceivedFin,
		ReceivedSynAck,
		SentFin,
		SentSyn,
		Connected,
		Listening
	} state {};
};

namespace {
	ManuallyDestroy<Spinlock<kstd::vector<Tcp4Socket*>>> sockets;
}

static void remove_socket(Tcp4Socket* socket) {
	IrqGuard irq_guard {};
	auto guard = sockets->lock();

	for (usize i = 0; i < guard->size(); ++i) {
		if ((*guard)[i] == socket) {
			guard->remove(i);
			break;
		}
	}
}

kstd::shared_ptr<Socket> tcp_socket_create(int flags) {
	auto socket = kstd::make_shared<Tcp4Socket>(flags);
	IrqGuard irq_guard {};
	auto guard = sockets->lock();
	guard->push(socket.data());
	return std::move(socket);
}

void tcp_process_packet(Nic& nic, ReceivedPacket& packet) {
	TcpHeader hdr {};
	memcpy(&hdr, packet.layer2.raw, sizeof(TcpHeader));
	hdr.deserialize();

	bool processed = false;

	IrqGuard irq_guard {};
	auto guard = sockets->lock();

	for (auto& socket : *guard) {
		if (socket->state == Tcp4Socket::State::Listening) {
			continue;
		}

		if (socket->own_ip == packet.layer1.ipv4.dest_addr &&
			socket->own_port == hdr.dest_port &&
			socket->target.ipv4 == packet.layer1.ipv4.src_addr &&
			socket->target.port == hdr.src_port) {
			socket->process_packet(packet, hdr);
			processed = true;
			break;
		}
	}

	if (!processed) {
		for (auto& socket : *guard) {
			if (socket->state == Tcp4Socket::State::Listening &&
				socket->own_port == hdr.dest_port) {
				socket->process_packet(packet, hdr);
				break;
			}
		}
	}
}
