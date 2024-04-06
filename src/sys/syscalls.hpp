#pragma once
#include "arch/arch_syscalls.hpp"

extern "C" void syscall_handler(SyscallFrame* frame);
