#include "hpet.hpp"
#include "acpi/common.hpp"
#include "console.hpp"
#include "memory/map.hpp"
#include "utils.hpp"

struct [[gnu::packed]] AddressStructure {
	u8 addr_space_id;
	u8 reg_bit_width;
	u8 reg_bit_offset;
	u8 reserved;
	u64 address;
};

#define LEGACY_REPLACEMENT_IRQ_ROUTING_CAP (1ULL << 15)
#define COUNTER_SIZE_CAP (1ULL << 13)
#define NUM_OF_COMPARATORS (0b11111 << 8)

struct [[gnu::packed]] HpetHeader {
	SdtHeader header;
	u32 event_timer_block_id;
	AddressStructure base;
	u8 hpet_num;
	u16 min_tick;
	u8 page_protection;
};

static usize base = 0;

static inline void write_reg_64(u32 reg, u64 value) {
	*cast<volatile u64*>(base + reg) = value;
}

static inline u64 read_reg_64(u32 reg) {
	return *cast<volatile u64*>(base + reg);
}

#define REG_CAP 0
#define REG_CONF 0x10
#define REG_INT_STATUS 0x20
#define REG_MAIN_COUNTER 0xF0
#define REG_TIMER_CONF(n) (0x100 + n * 0x20)
#define REG_TIMER_COMP(n) (0x108 * n * 0x20)
#define REG_TIMER_FSB_CONF(n) (0x110 + n * 0x20)

#define CONF_LEGACY_MAPPING_ENABLED (1 << 1)
#define CONF_ENABLE (1 << 0)

static u32 ns_in_tick = 0;

void initialize_hpet(const void* hpet_ptr) {
	if (!hpet_ptr) {
		panic("tried to initialize hpet with null ptr");
	}

	auto header = cast<const HpetHeader*>(hpet_ptr);
	auto addr = PhysAddr {header->base.address};
	base = addr.to_virt().as_usize();
	get_map()->map(addr.to_virt(), addr, PageFlags::Rw | PageFlags::Huge | PageFlags::CacheDisable);

	u64 cap = read_reg_64(REG_CAP);
	if ((cap & COUNTER_SIZE_CAP) == 0) {
		panic("32bit hpet is not supported");
	}

	u32 fs_in_tick = cap >> 32;
	ns_in_tick = fs_in_tick / 1000000;

	u64 conf = read_reg_64(REG_CONF);
	conf &= ~CONF_LEGACY_MAPPING_ENABLED;
	conf |= CONF_ENABLE;
	write_reg_64(REG_CONF, conf);
}

void hpet_sleep(u64 us) {
	u64 ticks = us * 1000 / ns_in_tick;
	u64 value = read_reg_64(REG_MAIN_COUNTER) + ticks;
	while (read_reg_64(REG_MAIN_COUNTER) < value);
}

u64 hpet_get_current() {
	return read_reg_64(REG_MAIN_COUNTER);
}

u64 hpet_diff_to_ns(u64 diff) {
	return diff / ns_in_tick;
}

bool hpet_reselect_irq() {
	return false;
}