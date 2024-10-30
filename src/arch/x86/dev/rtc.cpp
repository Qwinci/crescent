#include "rtc.hpp"
#include "acpi/acpi.hpp"
#include "dev/clock.hpp"
#include "dev/date_time_provider.hpp"
#include "mem/portspace.hpp"
#include "mem/register.hpp"

namespace {
	PortSpace SPACE {0x70};

	BitRegister<u8> CMOS_SELECT {0};
	BitRegister<u8> CMOS_DATA {1};
}

namespace cmos_select {
	static constexpr BitField<u8, u8> REG_NUMBER {0, 7};
	static constexpr BitField<u8, bool> NMI_DISABLE {7, 1};
}

static u8 cmos_read(u8 reg) {
	SPACE.store(CMOS_SELECT, cmos_select::REG_NUMBER(reg));
	udelay(50);
	return SPACE.load(CMOS_DATA);
}

static void cmos_write(u8 reg, u8 value) {
	SPACE.store(CMOS_SELECT, cmos_select::REG_NUMBER(reg));
	udelay(50);
	SPACE.store(CMOS_DATA, value);
}

namespace rtc_reg {
	static constexpr u8 SECONDS = 0;
	static constexpr u8 MINUTES = 2;
	static constexpr u8 HOURS = 4;
	static constexpr u8 DAY_OF_MONTH = 7;
	static constexpr u8 MONTH = 8;
	static constexpr u8 YEAR = 9;
	static constexpr u8 STATUS_A = 0xA;
	static constexpr u8 STATUS_B = 0xB;

	static u8 CENTURY = 0;
}

namespace status_a {
	static constexpr u8 UPDATE_IN_PROGRESS = 1 << 7;
}

namespace status_b {
	static constexpr u8 ENABLE_24_HOUR = 1 << 1;
	static constexpr u8 ENABLE_BINARY = 1 << 2;
}

struct CmosRtcData {
	u8 second;
	u8 minute;
	u8 hour;
	u8 day;
	u8 month;
	u8 year;
	u8 century;

	constexpr bool operator==(const CmosRtcData& other) const {
		return memcmp(this, &other, sizeof(CmosRtcData)) == 0;
	}
};

static u8 USE_24_HOUR_FORMAT = false;
static u8 USE_BINARY = true;

static CrescentDateTime x86_read_rtc() {
	auto read_rtc = []() {
		while (cmos_read(rtc_reg::STATUS_A) & status_a::UPDATE_IN_PROGRESS);

		CmosRtcData data {};

		data.second = cmos_read(rtc_reg::SECONDS);
		data.minute = cmos_read(rtc_reg::MINUTES);
		data.hour = cmos_read(rtc_reg::HOURS);
		data.day = cmos_read(rtc_reg::DAY_OF_MONTH);
		data.month = cmos_read(rtc_reg::MONTH);
		data.year = cmos_read(rtc_reg::YEAR);
		if (rtc_reg::CENTURY) {
			data.century = cmos_read(rtc_reg::CENTURY);
		}

		return data;
	};

	IrqGuard irq_guard {};

	CmosRtcData data {};

	while (true) {
		auto first = read_rtc();
		auto second = read_rtc();
		if (first == second) {
			data = first;
			break;
		}
	}

	auto to_binary = [](u8& byte) {
		byte = (byte & 0xF) + (byte >> 4) * 10;
	};

	if (!USE_BINARY) {
		to_binary(data.second);
		to_binary(data.minute);
		data.hour = ((data.hour & 0xF) + ((data.hour >> 4 & 0b111) * 10)) |
			(data.hour & 1 << 7);
		to_binary(data.day);
		to_binary(data.month);
		to_binary(data.year);
		to_binary(data.century);
	}

	if (!USE_24_HOUR_FORMAT && (data.hour & 1 << 7)) {
		data.hour = (12 + (data.hour & ~(1 << 7))) % 24;
	}

	return {
		.year = static_cast<uint16_t>(data.year + data.century * 100),
		.month = data.month,
		.day = data.day,
		.hour = data.hour,
		.minute = data.minute,
		.second = data.second
	};
}

struct CmosRtcDateTimeProvider : public DateTimerProvider {
	int get_date(CrescentDateTime& res) override {
		res = x86_read_rtc();
		return 0;
	}
};

static CmosRtcDateTimeProvider CMOS_RTC_DATE_TIME_PROVIDER {};

void x86_rtc_init() {
	rtc_reg::CENTURY = acpi::GLOBAL_FADT->century;
	auto status = cmos_read(rtc_reg::STATUS_B);
	USE_24_HOUR_FORMAT = status & status_b::ENABLE_24_HOUR;
	USE_BINARY = status & status_b::ENABLE_BINARY;

	*DATE_TIME_PROVIDER.lock() = &CMOS_RTC_DATE_TIME_PROVIDER;
}
