#pragma once

struct Nic;
struct ReceivedPacket;

void dhcp_process_packet(Nic& nic, ReceivedPacket& packet);
void dhcp_discover(Nic* nic);
