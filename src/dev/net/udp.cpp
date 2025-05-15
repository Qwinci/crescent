#include "udp.hpp"
#include "arp.hpp"
#include "cstring.hpp"
#include "dev/event.hpp"
#include "nic/nic.hpp"
#include "packet.hpp"
#include "stdio.hpp"
#include "sys/socket.hpp"

struct Udp4BufferPacket {
	DoubleListHook hook {};
	u32 ip {};
	u16 port {};
	kstd::vector<u8> data {};
	u32 data_ptr {};
};

struct Udp4Socket;

static void remove_socket(Udp4Socket* socket);

struct Udp4Socket : public Socket {
	explicit Udp4Socket(int flags, u16 port) : Socket {flags}, own_port {port} {}

	~Udp4Socket() override {
		remove_socket(this);
	}

	int connect(const AnySocketAddress&) override {
		return ERR_UNSUPPORTED;
	}

	int disconnect() override {
		return ERR_UNSUPPORTED;
	}

	int listen(uint32_t port) override {
		return ERR_UNSUPPORTED;
	}

	int accept(kstd::shared_ptr<Socket>&, int) override {
		return ERR_UNSUPPORTED;
	}

	int send(const void*, usize&) override {
		return ERR_UNSUPPORTED;
	}

	int receive(void*, usize&) override {
		return ERR_UNSUPPORTED;
	}

	int send_to(const void* data, usize& size, const AnySocketAddress& dest) override {
		if (dest.generic.type != SOCKET_ADDRESS_TYPE_IPV4) {
			return ERR_INVALID_ARGUMENT;
		}

		auto mac = arp_get_mac(dest.ipv4.ipv4);
		if (!mac) {
			return ERR_NO_ROUTE_TO_HOST;
		}

		kstd::shared_ptr<Nic> nic;

		{
			// todo allow other nics
			IrqGuard irq_guard {};
			nic = *NICS->lock()->front();
		}

		Packet packet {static_cast<u32>(sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(UdpHeader) + size)};

		packet.add_ethernet(nic->mac, mac.value(), EtherType::Ipv4);
		packet.add_ipv4(IpProtocol::Udp, sizeof(UdpHeader) + size, nic->ip, dest.ipv4.ipv4);
		packet.add_udp(own_port, dest.ipv4.port, size);
		auto* ptr = packet.add_header(size);
		memcpy(ptr, data, size);

		nic->send(packet.data, packet.size);

		return 0;
	}

	int receive_from(void* data, usize& size, AnySocketAddress& src) override {
		while (true) {
			IrqGuard irq_guard {};

			{
				auto guard = packet_list.lock();
				auto packet = guard->front();
				if (packet) {
					usize to_copy = kstd::min(packet->data.size() - packet->data_ptr, size);

					memcpy(data, packet->data.data() + packet->data_ptr, to_copy);
					packet->data_ptr += to_copy;

					size = to_copy;
					src = {
						.ipv4 {
							.generic {
								.type = SOCKET_ADDRESS_TYPE_IPV4
							},
							.ipv4 = packet->ip,
							.port = packet->port
						}
					};

					if (packet->data_ptr == packet->data.size()) {
						guard->pop_front();
						--*packet_count.lock();
						delete packet;
					}

					return 0;
				}
			}

			if (flags & SOCK_NONBLOCK) {
				return ERR_TRY_AGAIN;
			}
			else {
				event.wait();
			}
		}
	}

	int get_peer_name(AnySocketAddress&) override {
		return ERR_UNSUPPORTED;
	}

	u16 own_port {};
	Spinlock<DoubleList<Udp4BufferPacket, &Udp4BufferPacket::hook>> packet_list;
	Spinlock<usize> packet_count;
	Event event {};
};

namespace {
	ManuallyDestroy<Spinlock<kstd::vector<Udp4Socket*>>> SOCKETS;
}

static void remove_socket(Udp4Socket* socket) {
	IrqGuard irq_guard {};
	auto guard = SOCKETS->lock();

	for (usize i = 0; i < guard->size(); ++i) {
		if ((*guard)[i] == socket) {
			guard->remove(i);
			break;
		}
	}
}

kstd::shared_ptr<Socket> udp_socket_create(int flags) {
	IrqGuard irq_guard {};
	auto guard = SOCKETS->lock();
	u16 port = UINT16_MAX;
	while (true) {
		bool found = true;
		for (auto sock : *guard) {
			if (sock->own_port == port) {
				found = false;
				break;
			}
		}

		if (found) {
			break;
		}

		if (!port) {
			return nullptr;
		}
		--port;
	}

	auto socket = kstd::make_shared<Udp4Socket>(flags, port);

	guard->push(socket.data());
	return socket;
}

void udp_process_packet(Nic& nic, ReceivedPacket& packet) {
	UdpHeader hdr {};
	memcpy(&hdr, packet.layer2.raw, sizeof(UdpHeader));
	hdr.deserialize();

	IrqGuard irq_guard {};
	auto guard = SOCKETS->lock();
	for (auto& socket : *guard) {
		if (socket->own_port == hdr.dest_port) {
			auto packet_list_guard = socket->packet_list.lock();

			auto packet_count_guard = socket->packet_count.lock();
			if (packet_count_guard >= 64) {
				return;
			}
			++*packet_count_guard;

			auto* udp_buffer_packet = new Udp4BufferPacket {
				.ip = packet.layer1.ipv4.src_addr,
				.port = hdr.src_port
			};
			udp_buffer_packet->data.resize(hdr.length - sizeof(UdpHeader));
			memcpy(
				udp_buffer_packet->data.data(),
				offset(packet.layer2.raw, void*, sizeof(UdpHeader)),
				hdr.length - sizeof(UdpHeader));

			packet_list_guard->push(udp_buffer_packet);
			socket->event.signal_one();
			break;
		}
	}
}
