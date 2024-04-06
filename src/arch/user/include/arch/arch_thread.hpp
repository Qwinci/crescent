#pragma once

struct Process;

struct ArchThread {
	constexpr ArchThread() = default;
	constexpr ArchThread(void (*fn)(void*), void* arg, Process* process) {}
};
