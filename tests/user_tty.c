#include "crescent/fs.h"
#include "crescent/sys.h"
#include "crescent/input.h"
#include "crescent/fb.h"
#include "crescent/dev.h"
#include <stddef.h>
#include <stdatomic.h>

size_t syscall0(size_t num) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num) : "r11", "rcx");
	return ret;
}

size_t syscall1(size_t num, size_t a0) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0) : "r11", "rcx");
	return ret;
}

size_t syscall2(size_t num, size_t a0, size_t a1) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1) : "r11", "rcx");
	return ret;
}

size_t syscall3(size_t num, size_t a0, size_t a1, size_t a2) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1), "d"(a2) : "r11", "rcx");
	return ret;
}

[[noreturn]] void sys_exit(int status) {
	syscall1(SYS_EXIT, status);
	__builtin_unreachable();
}

int sys_dprint(const char* msg, size_t len) {
	return (int) syscall2(SYS_DPRINT, (size_t) msg, len);
}

Handle sys_create_thread(void (*fn)(void*), void* arg) {
	return (Handle) syscall2(SYS_CREATE_THREAD, (size_t) fn, (size_t) arg);
}

int sys_kill_thread(Handle thread) {
	return (int) syscall1(SYS_KILL_THREAD, (size_t) thread);
}

void sys_sleep(size_t ms) {
	syscall1(SYS_SLEEP, ms);
}

int sys_wait_thread(Handle thread) {
	return (int) syscall1(SYS_WAIT_THREAD, thread);
}

int sys_wait_for_event(Event* event) {
	return (int) syscall1(SYS_WAIT_FOR_EVENT, (size_t) event);
}

int sys_poll_event(Event* event) {
	return (int) syscall1(SYS_POLL_EVENT, (size_t) event);
}

int sys_shutdown(ShutdownType type) {
	return (int) syscall1(SYS_SHUTDOWN, type);
}

bool sys_request_cap(uint32_t cap) {
	return syscall1(SYS_REQUEST_CAP, cap);
}

void* sys_mmap(size_t size, int protection) {
	return (void*) syscall2(SYS_MMAP, size, protection);
}

int sys_munmap(void* ptr, size_t size) {
	return (int) syscall2(SYS_MUNMAP, (size_t) ptr, size);
}

int sys_close(Handle handle) {
	return (int) syscall1(SYS_CLOSE, handle);
}

int sys_devmsg(Handle handle, size_t msg, void* data) {
	return (int) syscall3(SYS_DEVMSG, (size_t) handle, msg, (size_t) data);
}

int sys_devenum(DeviceType type, DeviceInfo* res, size_t* count) {
	return (int) syscall3(SYS_DEVENUM, (size_t) type, (size_t) res, (size_t) count);
}

int sys_open(const char* path, size_t path_len, Handle* ret) {
	return (int) syscall3(SYS_OPEN, (size_t) path, path_len, (size_t) ret);
}

int sys_read(Handle handle, void* buffer, size_t size) {
	return (int) syscall3(SYS_READ, handle, (size_t) buffer, size);
}

int sys_stat(Handle handle, Stat* stat) {
	return (int) syscall2(SYS_STAT, handle, (size_t) stat);
}

int sys_opendir(const char* path, size_t path_len, Dir** ret) {
	return (int) syscall3(SYS_OPENDIR, (size_t) path, path_len, (size_t) ret);
}

int sys_closedir(Dir* dir) {
	return (int) syscall1(SYS_CLOSEDIR, (size_t) dir);
}

int sys_readdir(Dir* dir, DirEntry* entry) {
	return (int) syscall2(SYS_READDIR, (size_t) dir, (size_t) entry);
}

// clang-format off

static const char* LAYOUT[SCAN_MAX] = {
	[SCAN_A]                        = "a",
	[SCAN_B]                        = "b",
	[SCAN_C]                        = "c",
	[SCAN_D]                        = "d",
	[SCAN_E]                        = "e",
	[SCAN_F]                        = "f",
	[SCAN_G]                        = "g",
	[SCAN_H]                        = "h",
	[SCAN_I]                        = "i",
	[SCAN_J]                        = "j",
	[SCAN_K]                        = "k",
	[SCAN_L]                        = "l",
	[SCAN_M]                        = "m",
	[SCAN_N]                        = "n",
	[SCAN_O]                        = "o",
	[SCAN_P]                        = "p",
	[SCAN_Q]                        = "q",
	[SCAN_R]                        = "r",
	[SCAN_S]                        = "s",
	[SCAN_T]                        = "t",
	[SCAN_U]                        = "u",
	[SCAN_V]                        = "v",
	[SCAN_W]                        = "w",
	[SCAN_X]                        = "x",
	[SCAN_Y]                        = "y",
	[SCAN_Z]                        = "z",
	[SCAN_1]                        = "1",
	[SCAN_2]                        = "2",
	[SCAN_3]                        = "3",
	[SCAN_4]                        = "4",
	[SCAN_5]                        = "5",
	[SCAN_6]                        = "6",
	[SCAN_7]                        = "7",
	[SCAN_8]                        = "8",
	[SCAN_9]                        = "9",
	[SCAN_0]                        = "0",
	[SCAN_ENTER]                    = "\n",
	[SCAN_ESC]                      = "",
	[SCAN_BACKSPACE]                = "\b",
	[SCAN_TAB]                      = "\t",
	[SCAN_SPACE]                    = " ",
	[SCAN_MINUS]                    = "+",
	[SCAN_EQUAL]                    = "´",
	[SCAN_LBRACKET]                 = "å",
	[SCAN_RBRACKET]                 = "¨",
	[SCAN_BACKSLASH]                = "'",
	[SCAN_NON_US_HASH]              = "#",
	[SCAN_SEMICOLON]                = "ä",
	[SCAN_APOSTROPHE]               = "ö",
	[SCAN_GRAVE]                    = "§",
	[SCAN_COMMA]                    = ",",
	[SCAN_DOT]                      = ".",
	[SCAN_SLASH]                    = "-",
	[SCAN_KEYPAD_SLASH]             = "/",
	[SCAN_KEYPAD_STAR]              = "*",
	[SCAN_KEYPAD_MINUS]             = "-",
	[SCAN_KEYPAD_PLUS]              = "+",
	[SCAN_KEYPAD_ENTER]             = "\n",
	[SCAN_KEYPAD_1]                 = "1",
	[SCAN_KEYPAD_2]                 = "2",
	[SCAN_KEYPAD_3]                 = "3",
	[SCAN_KEYPAD_4]                 = "4",
	[SCAN_KEYPAD_5]                 = "5",
	[SCAN_KEYPAD_6]                 = "6",
	[SCAN_KEYPAD_7]                 = "7",
	[SCAN_KEYPAD_8]                 = "8",
	[SCAN_KEYPAD_9]                 = "9",
	[SCAN_KEYPAD_0]                 = "0",
	[SCAN_KEYPAD_DOT]               = ".",
	[SCAN_NON_US_BACKSLASH_WALL]    = "<",
};

// clang-format on

size_t strlen(const char* str) {
	size_t len = 0;
	while (*str++) ++len;
	return len;
}

void puts(const char* str) {
	sys_dprint(str, strlen(str));
	sys_dprint("\n", 1);
}

void another_thread(void* arg) {
	puts("hello from another thread");
	if (arg == (void*) 0xCAFE) {
		puts("cafe received from thread 1!");
	}
	else {
		puts("didn't receive cafe from thread 1");
	}
	sys_exit(0xCAFE);
}

void* memcpy(void* restrict dest, const void* restrict src, size_t len) {
	char* dest_ptr = (char*) dest;
	const char* src_ptr = (const char*) src;
	for (size_t i = 0; i < len; ++i) {
		*dest_ptr++ = *src_ptr++;
	}
	return dest;
}

_Noreturn void _start(void*) {
	Handle another = sys_create_thread(another_thread, (void*) 0xCAFE);
	if (another == INVALID_HANDLE) {
		puts("sys_create_thread failed to create thread");
	}
	//puts("waiting for thread 2 to exit in thread1\n");
	int status = sys_wait_thread(another);
	if (sys_close(another)) {
		puts("failed to close thread handle");
	}
	if (status == 0xCAFE) {
		//puts("thread 2 exited with status 0xCAFE\n");
	}
	else {
		puts("thread 2 didn't exit with status 0xCAFE");
	}

	if (sys_request_cap(CAP_DIRECT_FB_ACCESS)) {
		sys_dprint("user_tty got direct fb access\n", sizeof("user_tty got direct fb access\n") - 1);
	}
	else {
		sys_dprint("user_tty didn't get direct fb access\n", sizeof("user_tty didn't get direct fb access\n") - 1);
	}

	if (sys_request_cap(CAP_MANAGE_POWER)) {
		puts("user_tty got power management access");
	}
	else {
		puts("user_tty didn't get power management access, F5 isn't going to work");
	}

	DeviceInfo fb;
	size_t count = 1;
	int ret = sys_devenum(DEVICE_TYPE_FB, &fb, &count);
	if (ret == 0) {
		SysFramebufferInfo info;
		ret = sys_devmsg(fb.handle, DEVMSG_FB_INFO, &info);
		if (ret != 0) {
			puts("failed to get fb info");
		}
		void* base;
		ret = sys_devmsg(fb.handle, DEVMSG_FB_MAP, &base);
		if (ret == 0) {
			for (size_t y = 0; y < 20; ++y) {
				for (size_t x = info.width - 20; x < info.width; ++x) {
					*(uint32_t*) ((uintptr_t) base + y * info.pitch + x * (info.bpp / 8)) = 0x0000FF;
				}
			}
		}
		else if (ret == ERR_NO_PERMISSIONS) {
			puts("no permissions to map framebuffer");
		}
		else if (ret == ERR_NO_MEM) {
			puts("no memory for framebuffer mapping");
		}
		else {
			puts("unknown error while mapping framebuffer");
		}
	}
	else {
		puts("failed to enumerate framebuffers");
	}

	size_t par_count = 1;
	DeviceInfo info;
	sys_devenum(DEVICE_TYPE_PARTITION, &info, &par_count);
	sys_close(info.handle);

	Dir* dir;
	sys_opendir(info.name, strlen(info.name), &dir);

	DirEntry entry;
	while (sys_readdir(dir, &entry) == 0) {
		puts(entry.name);
		if (entry.type != FS_ENTRY_TYPE_FILE) {
			continue;
		}

		char name[256];
		memcpy(name, info.name, strlen(info.name));
		name[strlen(info.name)] = '/';
		memcpy(name + strlen(info.name) + 1, entry.name, entry.name_len);
		name[strlen(info.name) + 1 + entry.name_len] = 0;

		Handle handle;
		ret = sys_open(name, strlen(name), &handle);
		Stat stat;
		ret = sys_stat(handle, &stat);

		char* buffer = (char*) sys_mmap((stat.size + 1 + 0xFFF) & ~0xFFF, PROT_READ | PROT_WRITE);
		ret = sys_read(handle, buffer, stat.size);
		buffer[stat.size] = 0;
		puts(buffer);

		sys_close(handle);
		sys_munmap(buffer, (stat.size + 1 + 0xFFF) & ~0xFFF);
	}

	// try mapping 100gb
	void* huge_test = sys_mmap(1024ULL * 1024 * 1024 * 100, PROT_READ | PROT_WRITE);
	if (huge_test) {
		puts("100gb map successful");
		puts("trying to write to the first and last pages");
		*(int*) huge_test = 0xCAFE;
		puts("first");
		*(int*) ((uintptr_t) huge_test + 1024ULL * 1024 * 1024 * 100 - 4) = 0xCAFE;
		if (*(int*) huge_test == 0xCAFE && *(int*) ((uintptr_t) huge_test + 1024ULL * 1024 * 1024 * 100 - 4) == 0xCAFE) {
			puts("works");
		}
	}

	// num arg0 arg1 arg2 arg3 arg4 arg5
	// rdi rax  rsi  rdx  r10  r8   r9

	while (true) {
		Event event;
		sys_wait_for_event(&event);

		if (event.type == EVENT_KEY && event.key.pressed) {
			if (event.key.key == SCAN_F5) {
				if (sys_shutdown(SHUTDOWN_TYPE_REBOOT) == ERR_NO_PERMISSIONS) {
					puts("no permissions to perform the reboot");
				}
			}

			const char* text = LAYOUT[event.key.key];
			if (text) {
				sys_dprint(text, strlen(text));
			}
		}
	}
}
