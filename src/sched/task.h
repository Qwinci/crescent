#pragma once
#include "mem/vmem.h"
#include "types.h"

typedef enum : u8 {
	TASK_STATUS_RUNNING = 0,
	TASK_STATUS_READY = 1,
	TASK_STATUS_SLEEPING = 2,
	TASK_STATUS_EXITED = 3,
	TASK_STATUS_KILLED = 4,
	TASK_STATUS_WAITING = 5,
	TASK_STATUS_MAX = 6
} TaskStatus;

typedef struct Cpu Cpu;

typedef struct Task {
	char name[128];
	struct Task* next;
	usize sleep_end;
	Cpu* cpu;
	struct Page* allocated_pages;
	void* map;
	VMem user_vmem;
	TaskStatus status;
	u8 level;
	u8 priority;
	bool pin_level;
	bool pin_cpu;
} Task;

void task_add_page(Task* task, struct Page* page);
void task_remove_page(Task* task, struct Page* page);