target_sources(crescent PRIVATE
	ethernet.cpp
	ipv4.cpp
	udp.cpp
	tcp.cpp
	arp.cpp
	packet.cpp
	dhcp.cpp
)

add_subdirectory(nic)
