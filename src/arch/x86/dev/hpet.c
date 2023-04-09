#include "hpet.h"
#include "acpi/acpi.h"
#include "arch/map.h"
#include "arch/misc.h"
#include "mem/utils.h"

typedef struct [[gnu::packed]] {
	SdtHeader header;
	u32 event_timer_block_id;
	AcpiAddr base;
	u8 hpet_num;
	u16 min_tick;
	u8 page_protection;
} HpetHeader;

static usize base = 0;
static usize ns_in_tick = 0;

static inline void reg_write64(u32 reg, u64 value) {
	*(volatile u64*) (base + reg) = value;
}

static inline u64 reg_read64(u32 reg) {
	return *(volatile u64*) (base + reg);
}

#define REG_CAP 0
#define REG_CONF 0x10
#define REG_MAIN_COUNTER 0xF0

#define CAP_COUNTER_SIZE (1ULL << 13)
#define CONF_LEGACY_MAPPING_ENABLED (1 << 1)
#define CONF_ENABLE (1 << 0)

bool hpet_init() {
	const HpetHeader* hdr = (const HpetHeader*) acpi_get_table("HPET");
	if (!hdr) {
		return false;
	}

	base = (usize) to_virt(hdr->base.address);
	arch_map_page(KERNEL_MAP, base, hdr->base.address, PF_READ | PF_WRITE | PF_NC);

	u64 cap = reg_read64(REG_CAP);
	if ((cap & CAP_COUNTER_SIZE) == 0) {
		return false;
	}

	u32 fs_in_tick = cap >> 32;
	ns_in_tick = fs_in_tick / 1000000;

	u64 conf = reg_read64(REG_CONF);
	conf &= ~CONF_LEGACY_MAPPING_ENABLED;
	conf |= CONF_ENABLE;
	reg_write64(REG_CONF, conf);

	return true;
}

void hpet_delay(usize us) {
	u64 ticks = us * 1000 / ns_in_tick;
	u64 value = reg_read64(REG_MAIN_COUNTER) + ticks;
	while (reg_read64(REG_MAIN_COUNTER) < value) {
		arch_spinloop_hint();
	}
}