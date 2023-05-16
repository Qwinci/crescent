#include "arch/interrupts.h"
#include "arch/x86/dev/lapic.h"
#include "stdio.h"
#include "utils/elf.h"
#include "interrupts.h"

static u32 used_ints[256 / 32 - 1] = {};

static HandlerData handlers[256] = {};

u32 arch_alloc_int(usize count, IntHandler handler, void* userdata) {
	for (u16 i = 0; i < 256 - 32; ++i) {
		if (!(used_ints[i / 32] & 1U << (i % 32))) {
			bool continue_outer = false;
			usize free_count = 1;
			for (u16 j = i + 1; j < 256 - 32 && free_count < count; ++j, ++free_count) {
				if (used_ints[j / 32] & 1U << (j % 32)) {
					continue_outer = true;
					break;
				}
			}
			if (continue_outer) {
				continue;
			}
			for (usize j = 0; j < count; ++j) {
				used_ints[(i + j) / 32] |= 1U << ((i + j) % 32);
			}
			handlers[i + 32].handler = handler;
			handlers[i + 32].userdata = userdata;
			return i + 32;
		}
	}
	return 0;
}

HandlerData arch_set_handler(u32 i, IntHandler handler, void* userdata) {
	if (i >= 32) {
		used_ints[(i - 32) / 32] |= 1 << ((i - 32) % 32);
	}
	HandlerData old = handlers[i];
	handlers[i].handler = handler;
	handlers[i].userdata = userdata;
	return old;
}

void arch_dealloc_int(usize count, u32 i) {
	for (usize j = 0; j < count; ++j) {
		used_ints[(i + j - 32) / 32] &= ~(1 << ((i + j - 32) % 32));
		handlers[i + j].handler = NULL;
		handlers[i + j].userdata = NULL;
	}
}

typedef struct Frame {
	struct Frame* rbp;
	u64 rip;
} Frame;

#include "limine/limine.h"
#include "string.h"

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
	__asm__ volatile("mov %0, rbp" : "=rm"(frame));
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

void x86_int_handler(InterruptCtx* ctx) {
	if (ctx->cs == 0x2b) {
		__asm__ volatile("swapgs");
	}

	lapic_eoi();

	HandlerData* data = &handlers[ctx->vec];

	if (data->handler) {
		data->handler(ctx, data->userdata);
	}
	else {
		kprintf("[kernel][x86]: unhandled interrupt 0x%x\n", ctx->vec);

		backtrace_display(true);
	}

	if (ctx->cs == 0x2b) {
		__asm__ volatile("swapgs");
	}
}
