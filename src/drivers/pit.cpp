#include "pit.hpp"
#include "io.hpp"

constexpr u8 HARDWARE_ONE_SHOT = 0b1 << 1;
constexpr u8 ACCESS_LO_HI_BYTE = 0b11 << 4;
constexpr u8 CHANNEL_2 = 0b10 << 6;

void pit_prepare_sleep() {
	// Clear the state and set PIT bit
	out1(0x61, (in1(0x61) & 0xFD) | 1);
	// set mode
	out1(0x43, HARDWARE_ONE_SHOT | ACCESS_LO_HI_BYTE | CHANNEL_2);
	// set frequency to 100times/s (10ms)
	constexpr u32 frequency = 1193182 / 100;
	out1(0x42, frequency & 0xFF);
	io_wait();
	out1(0x42, frequency >> 8);
}

void pit_perform_10ms_sleep() {
	// Reset counter
	u8 init = in1(0x61) & 0xFE;
	out1(0x61, init);
	out1(0x61, init | 1);

	while (in1(0x61) & 1 << 5);
}
