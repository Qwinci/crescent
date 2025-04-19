#include "timer.h"
#include "bus.h"

u8 timer_read(Timer* self, u16 addr) {
	if (addr == 0xFF04) {
		return self->div >> 8;
	}
	else if (addr == 0xFF05) {
		return self->tima;
	}
	else if (addr == 0xFF06) {
		return self->tma;
	}
	else {
		return self->tac;
	}
}

static u8 DIV_BIT_POS[] = {
	[0b00] = 9,
	[0b01] = 3,
	[0b10] = 5,
	[0b11] = 7
};

void timer_cycle(Timer* self) {
	if (self->overflowed) {
		self->overflowed = false;
		self->tima = self->tma;
		cpu_request_irq(&self->bus->cpu, IRQ_TIMER);
		self->tima_reloaded = true;
	}
	else {
		self->tima_reloaded = false;
	}

	self->div += 4;

	u8 bit = self->div >> DIV_BIT_POS[self->tac & 0b11] & 0b1;
	u8 timer_enable = self->tac >> 2 & 1;
	u8 res = bit & timer_enable;

	if (self->last_res && !res) {
		if (self->tima == 0xFF) {
			self->tima = 0;
			self->overflowed = true;
		}
		else {
			self->tima += 1;
		}
	}

	self->last_res = res;
}

void timer_write(Timer* self, u16 addr, u8 value) {
	if (addr == 0xFF04) {
		self->div = 0;
	}
	else if (addr == 0xFF05) {
		if (!self->tima_reloaded) {
			self->tima = value;
		}
		self->overflowed = false;
	}
	else if (addr == 0xFF06) {
		if (self->tima_reloaded) {
			self->tima = value;
		}
		self->tma = value;
	}
	else {
		self->tac = value;
	}
}
