target_sources(crescent PRIVATE
	ethernet.cpp
	ipv4.cpp
	udp.cpp
	arp.cpp
	packet.cpp
)

add_subdirectory(nic)
