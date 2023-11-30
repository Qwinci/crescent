#include "serial.h"
#include "dev/log.h"
#include "io.h"

#define REG_DATA 0
#define REG_INT_ENABLE 1
#define REG_BAUD_RATE_DIV_LOW 0
#define REG_BAUD_RATE_DIV_HIGH 1
#define REG_INT_FIFO_CTRL 2
#define REG_LINE_CTRL 3
#define REG_MODEM_CTRL 4
#define REG_LINE_STS 5
#define REG_MODEM_STS 6
#define REG_SCRATCH 7

#define LINE_CTRL_DLAB (1U << 7)
#define LINE_CTRL_8BIT (0b11)
#define LINE_CTRL_STOP_1 (1 << 2)
#define LINE_CTRL_NO_PARITY (0 << 3)

#define MODEM_CTRL_DTR (1 << 0)
#define MODEM_CTRL_RTS (1 << 1)
#define MODEM_CTRL_OUT1 (1 << 2)
#define MODEM_CTRL_IRQ (1 << 3)
#define MODEM_CTRL_LOOP (1 << 4)

#define LINE_STS_DR (1 << 0)
#define LINE_STS_THRE (1 << 5)

bool serial_init(u16 port) {
	out1(port + REG_INT_ENABLE, 0);
	out1(port + REG_LINE_CTRL, LINE_CTRL_DLAB);
	out1(port + REG_BAUD_RATE_DIV_LOW, 3);
	out1(port + REG_BAUD_RATE_DIV_HIGH, 0);
	out1(port + REG_LINE_CTRL, LINE_CTRL_8BIT | LINE_CTRL_STOP_1 | LINE_CTRL_NO_PARITY);
	out1(port + REG_INT_FIFO_CTRL, 0xC7);
	out1(port + REG_MODEM_CTRL, MODEM_CTRL_DTR | MODEM_CTRL_RTS | MODEM_CTRL_IRQ);
	out1(port + REG_MODEM_CTRL, MODEM_CTRL_RTS | MODEM_CTRL_OUT1 | MODEM_CTRL_IRQ | MODEM_CTRL_LOOP);
	out1(port + REG_DATA, 0xAE);

	if (in1(port + REG_DATA) != 0xAE) {
		return false;
	}

	out1(port + REG_MODEM_CTRL, MODEM_CTRL_DTR | MODEM_CTRL_RTS | MODEM_CTRL_IRQ | MODEM_CTRL_OUT1);
	return true;
}

bool serial_can_receive(u16 port) {
	return in1(port + REG_LINE_STS) & LINE_STS_DR;
}

static inline bool serial_can_send(u16 port) {
	return in1(port + REG_LINE_STS) & LINE_STS_THRE;
}

u8 serial_receive(u16 port) {
	while (!serial_can_receive(port));
	return in1(port + REG_DATA);
}

void serial_send(u16 port, u8 value) {
	while (!serial_can_send(port));
	out1(port + REG_DATA, value);
}

typedef struct {
	LogSink common;
	u16 port;
} SerialLogSink;

static int serial_log_write(LogSink* sink, Str str) {
	u16 port = container_of(sink, SerialLogSink, common)->port;
	for (usize i = 0; i < str.len; ++i) {
		char c = str.data[i];
		if (c < 0) {
			// todo support colors
			continue;
		}

		serial_send(port, (u8) c);
	}
	return 0;
}

static SerialLogSink SERIAL_LOG_SINK = {
	.common = {
		.write = serial_log_write
	}
};

bool serial_attach_log(u16 port) {
	if (serial_init(port)) {
		SERIAL_LOG_SINK.port = port;
		log_register_sink(&SERIAL_LOG_SINK.common, false);
		return true;
	}
	else {
		return false;
	}
}
