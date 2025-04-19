#include "sys.h"
#include "windower/windower.hpp"
#include <cassert>
#include <stdio.h>
#include <string.h>

namespace protocol = windower::protocol;

#include "../../../include/crescent/evm.h"
#include "vga.hpp"
#include "vm.hpp"

asm(R"(
.globl some_code
.globl some_code_end
some_code:
	hlt
some_code_end:
)");

extern char some_code[];
extern char some_code_end[];

#include <cassert>
#include <cstdio>

#define CR0_PE 1u
#define CR0_MP (1U << 1)
#define CR0_EM (1U << 2)
#define CR0_TS (1U << 3)
#define CR0_ET (1U << 4)
#define CR0_NE (1U << 5)
#define CR0_WP (1U << 16)
#define CR0_AM (1U << 18)
#define CR0_NW (1U << 29)
#define CR0_CD (1U << 30)
#define CR0_PG (1U << 31)

/* CR4 bits */
#define CR4_VME 1
#define CR4_PVI (1U << 1)
#define CR4_TSD (1U << 2)
#define CR4_DE (1U << 3)
#define CR4_PSE (1U << 4)
#define CR4_PAE (1U << 5)
#define CR4_MCE (1U << 6)
#define CR4_PGE (1U << 7)
#define CR4_PCE (1U << 8)
#define CR4_OSFXSR (1U << 8)
#define CR4_OSXMMEXCPT (1U << 10)
#define CR4_UMIP (1U << 11)
#define CR4_VMXE (1U << 13)
#define CR4_SMXE (1U << 14)
#define CR4_FSGSBASE (1U << 16)
#define CR4_PCIDE (1U << 17)
#define CR4_OSXSAVE (1U << 18)
#define CR4_SMEP (1U << 20)
#define CR4_SMAP (1U << 21)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)

/* 32-bit page directory entry bits */
#define PDE32_PRESENT 1
#define PDE32_RW (1U << 1)
#define PDE32_USER (1U << 2)
#define PDE32_PS (1U << 7)

/* 64-bit page * entry bits */
#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_ACCESSED (1U << 5)
#define PDE64_DIRTY (1U << 6)
#define PDE64_PS (1U << 7)
#define PDE64_G (1U << 8)

int main() {
	CrescentHandle evm;
	assert(sys_evm_create(&evm) == 0);
	CrescentHandle vcpu;
	EvmGuestState* regs;
	assert(sys_evm_create_vcpu(evm, &vcpu, &regs) == 0);

	void* mem = nullptr;
	assert(sys_map(&mem, 0x200000, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE) == 0);
	assert(sys_evm_map(evm, 0, mem, 0x200000) == 0);

	memcpy(mem, some_code, some_code_end - some_code);

	uint64_t pml4_addr = 0x2000;
	uint64_t *pml4 = (uint64_t *)((uintptr_t)mem + pml4_addr);

	uint64_t pdpt_addr = 0x3000;
	uint64_t *pdpt = (uint64_t *)((uintptr_t)mem + pdpt_addr);

	uint64_t pd_addr = 0x4000;
	uint64_t *pd = (uint64_t *)((uintptr_t)mem + pd_addr);

	pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr;
	pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;
	pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;

	regs->cr3 = pml4_addr;
	regs->cr4 = CR4_PAE;
	regs->cr0
		= CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
	regs->efer = EFER_LME | EFER_LMA;

	regs->cs.base = 0;
	regs->cs.selector = 1 << 3;
	regs->cs.limit = 0xffffffff;

	regs->ds.base = 0;
	regs->ds.selector = 2 << 3;
	regs->es = regs->ds;
	regs->ss = regs->ds;
	regs->fs = regs->ds;
	regs->gs = regs->ds;
	regs->rflags = 2;
	regs->rip = 0;
	regs->rsp = 2 << 20;

	assert(sys_evm_vcpu_write_state(vcpu, EVM_STATE_BITS_SEG_REGS | EVM_STATE_BITS_CONTROL_REGS | EVM_STATE_BITS_EFER) == 0);
	int res = sys_evm_vcpu_run(vcpu);
	printf("res: %d\n", res);

	/*windower::Windower windower;
	if (auto status = windower::Windower::connect(windower); status != 0) {
		puts("[console]: failed to connect to window manager");
		return 1;
	}

	uint32_t width = 800;
	uint32_t height = 600;

	windower::Window window;
	if (auto status = windower.create_window(window, 0, 0, width, height); status != 0) {
		puts("[console]: failed to create window");
		return 1;
	}

	if (window.map_fb() != 0) {
		puts("[console]: failed to map fb");
		return 1;
	}
	auto* fb = static_cast<uint32_t*>(window.get_fb_mapping());

	Vm vm {fb, (width * height * 4 + 0xFFF) & ~0xFFF};
	printf("run vm\n");
	vm.run();

	vga_print_text_mem(fb);

	window.redraw();

	while (true) {
		auto event = window.wait_for_event();
		if (event.type == protocol::WindowEvent::CloseRequested) {
			window.close();
			break;
		}
		else if (event.type == protocol::WindowEvent::Key) {
			printf("[evm]: received scan %u pressed %u -> %u\n", event.key.code, event.key.prev_pressed, event.key.pressed);

			window.redraw();
		}
	}*/
}
