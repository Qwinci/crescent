#include "fb.h"
#include "arch/cpu.h"
#include "assert.h"
#include "fbcon.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "mem/vm.h"
#include "sys/utils.h"

int fbdev_devmsg(FbDev* self, CrescentDevMsgFb msg, __user void* data) {
	switch (msg) {
		case DEVMSG_FB_INFO:
			if (!mem_copy_to_user(data, &self->info, sizeof(CrescentFramebufferInfo))) {
				return ERR_FAULT;
			}
			return 0;
		case DEVMSG_FB_MAP:
		{
			Task* task = arch_get_cur_task();
			if (!(task->caps & CAP_DIRECT_FB_ACCESS)) {
				return ERR_NO_PERMISSIONS;
			}

			usize size = self->info.height * self->info.pitch;
			void* mem = vm_user_alloc(task->process, NULL, ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE, true);
			if (!mem) {
				return ERR_NO_MEM;
			}
			for (usize i = 0; i < size; i += PAGE_SIZE) {
				if (!arch_user_map_page(task->process, (usize) mem + i, self->phys_base + i, PF_READ | PF_WRITE | PF_WC | PF_USER)) {
					for (usize j = 0; j < i; j += PAGE_SIZE) {
						arch_user_unmap_page(task->process, (usize) mem + j, true);
					}
					vm_user_dealloc(task->process, mem, ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE, true);
					return ERR_NO_MEM;
				}
			}

			if (!mem_copy_to_user(data, &mem, sizeof(void*))) {
				for (usize i = 0; i < size; i += PAGE_SIZE) {
					arch_user_unmap_page(task->process, (usize) mem + i, true);
				}
				vm_user_dealloc(task->process, mem, ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE, true);
				return ERR_FAULT;
			}

			default_fblog_deinit();
			return 0;
		}
		default:
			__builtin_unreachable();
	}
}

void fb_set_pixel(FbDev* self, usize x, usize y, u32 color) {
	assert(self);
	assert(self->base);
	assert(x < self->info.width);
	assert(y < self->info.height);
	if (self->info.fmt == SYS_FB_FORMAT_BGRA32) {
		*offset(self->base, u32*, y * self->info.pitch + x * (self->info.bpp / 8)) = color;
	}
	else {
		return;
	}
}

u32 fb_get_pixel(FbDev* self, usize x, usize y) {
	if (self->info.fmt == SYS_FB_FORMAT_BGRA32) {
		return *offset(self->base, u32*, y * self->info.pitch + x * (self->info.bpp / 8));
	}
	else {
		return 0;
	}
}

void fb_clear(FbDev* self, u32 color) {
	for (usize y = 0; y < self->info.height; ++y) {
		for (usize x = 0; x < self->info.width; ++x) {
			fb_set_pixel(self, x, y, color);
		}
	}
}
