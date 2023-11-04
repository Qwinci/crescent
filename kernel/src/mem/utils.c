#include "utils.h"
#include "arch/map.h"
#include "arch/cpu.h"

extern usize HHDM_END;

usize to_phys_generic(void* virt) {
	if ((usize) virt > HHDM_OFFSET && (usize) virt < HHDM_END) {
		return to_phys(virt);
	}
	else {
		return arch_virt_to_phys(arch_get_cur_task()->cpu->cur_map, (usize) virt);
	}
}