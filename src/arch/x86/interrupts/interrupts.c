#include "arch/interrupts.h"
#include "arch/x86/dev/lapic.h"
#include "interrupts.h"
#include "ipl.h"
#include "stdio.h"
#include "utils/elf.h"
#include "limine/limine.h"
#include "string.h"

static u32 used_ints[256 / 32] = {};
static u32 used_shareable_ints[256 / 32] = {};

static IrqHandler* handlers[256] = {};

IrqInstallStatus arch_irq_install(u32 irq, IrqHandler* persistent_handler, IrqInstallFlags flags) {
	persistent_handler->can_be_shared = flags & IRQ_INSTALL_SHARED;
	persistent_handler->prev = NULL;
	if (handlers[irq]) {
		if (handlers[irq] == persistent_handler || !handlers[irq]->can_be_shared || !(flags & IRQ_INSTALL_SHARED)) {
			return IRQ_INSTALL_STATUS_ALREADY_EXISTS;
		}
		persistent_handler->next = handlers[irq];
		handlers[irq]->prev = persistent_handler;
		handlers[irq] = persistent_handler;
	}
	else {
		persistent_handler->next = NULL;
		handlers[irq] = persistent_handler;
	}
	return IRQ_INSTALL_STATUS_SUCCESS;
}

IrqRemoveStatus arch_irq_remove(u32 irq, IrqHandler* persistent_handler) {
	if (!handlers[irq] || (handlers[irq] != persistent_handler && !persistent_handler->prev)) {
		return IRQ_REMOVE_STATUS_NOT_EXIST;
	}
	if (persistent_handler->prev) {
		persistent_handler->prev->next = persistent_handler->next;
	}
	else {
		handlers[irq] = persistent_handler->next;
	}
	if (persistent_handler->next) {
		persistent_handler->next->prev = persistent_handler->prev;
	}
	return IRQ_REMOVE_STATUS_SUCCESS;
}

u32 arch_irq_alloc_generic(Ipl ipl, u32 count, IrqInstallFlags flags) {
	if (!count) {
		return 0;
	}

	bool can_be_shared = flags & IRQ_INSTALL_SHARED;
	X86Ipl x86_ipl = X86_IPL_MAP[ipl];
	for (u32 i = x86_ipl * 16; i < 256 - (count - 1); ++i) {
		bool found = true;
		for (u32 j = i; j < i + count; ++j) {
			if (handlers[j] || (used_ints[j / 32] & 1U << (j % 32)) || (!can_be_shared && used_shareable_ints[j / 32] & 1U << (j % 32))) {
				found = false;
				break;
			}
		}
		if (found) {
			if (can_be_shared) {
				for (u32 j = i; j < i + count; ++j) {
					used_shareable_ints[j / 32] |= 1U << (j % 32);
				}
			}
			else {
				for (u32 j = i; j < i + count; ++j) {
					used_ints[j / 32] |= 1U << (j % 32);
				}
			}
			return i;
		}
	}
    return 0;
}

void arch_irq_dealloc_generic(u32 irq, u32 count, IrqInstallFlags flags) {
	if (flags & IRQ_INSTALL_SHARED) {
		for (u32 i = irq; i < irq + count; ++i) {
			used_shareable_ints[i / 32] &= ~(1U << (i % 32));
		}
	}
	else {
		for (u32 i = irq; i < irq + count; ++i) {
			used_ints[i / 32] &= ~(1U << (i % 32));
		}
	}
}

typedef struct Frame {
	struct Frame* rbp;
	u64 rip;
} Frame;

static volatile struct limine_kernel_file_request KERNEL_FILE_REQUEST = {
	.id = LIMINE_KERNEL_FILE_REQUEST
};

static const char* strtab = NULL;
static void* symtab = NULL;
static usize symtab_size = 0;
static usize symtab_ent_size = 0;

void debug_init() {
	const Elf64EHdr* ehdr = (const Elf64EHdr*) KERNEL_FILE_REQUEST.response->kernel_file->address;

	const Elf64SHdr* shstrtab_sect = (const Elf64SHdr*) offset(ehdr, const Elf64SHdr*, ehdr->e_shoff + ehdr->e_shstrndx * ehdr->e_shentsize);
	const char* shstrtab = (const char*) ehdr + shstrtab_sect->sh_offset;

	for (usize i = 0; i < ehdr->e_shnum; ++i) {
		const Elf64SHdr* shdr = (const Elf64SHdr*) offset(ehdr, const Elf64SHdr*, ehdr->e_shoff + i * ehdr->e_shentsize);
		const char* name = shstrtab + shdr->sh_name;
		if (strcmp(name, ".symtab") == 0) {
			symtab = (void*) ((usize) ehdr + shdr->sh_offset);
			symtab_size = shdr->sh_size;
			symtab_ent_size = shdr->sh_entsize;
			if (strtab) {
				break;
			}
		}
		else if (strcmp(name, ".strtab") == 0) {
			strtab = (const char*) ehdr + shdr->sh_offset;
			if (symtab) {
				break;
			}
		}
	}
}

const char* resolve_ip(usize ip) {
	const char* name = NULL;

	for (usize i = 0; i < symtab_size; i += symtab_ent_size) {
		Elf64Sym* sym = offset(symtab, Elf64Sym*, i);
		if (ELF64_ST_TYPE(sym->st_info) != STT_FUNC || !sym->st_size || !sym->st_name) {
			continue;
		}

		if (ip < sym->st_value || ip >= sym->st_value + sym->st_size) {
			continue;
		}

		name = strtab + sym->st_name;
		break;
	}

	return name;
}

void backtrace_display(bool lock) {
	Frame* frame;
	__asm__ volatile("mov %%rbp, %0" : "=rm"(frame));
	while (frame->rbp) {
		const char* name = resolve_ip(frame->rip);
		if (lock) {
			kprintf("\t%s <0x%x>\n", name ? name : "<unknown>", frame->rip);
		}
		else {
			kprintf_nolock("\t%s <0x%x>\n", name ? name : "<unknown>", frame->rip);
		}
		frame = frame->rbp;
	}
}

void x86_int_handler(InterruptCtx* ctx, u64 num) {
	if (ctx->cs == 0x2b) {
		__asm__ volatile("swapgs");
	}

	lapic_eoi();

	bool handled = false;
	for (IrqHandler* handler = handlers[num]; handler; handler = handler->next) {
		if (handler->fn(ctx, handler->userdata) == IRQ_ACK) {
			handled = true;
			break;
		}
	}
	if (!handled) {
		kprintf("[kernel][x86]: unhandled interrupt 0x%x\n", num);

		backtrace_display(true);
	}

	if (ctx->cs == 0x2b) {
		__asm__ volatile("swapgs");
	}
}
