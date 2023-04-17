#include "task.h"
#include "mem/page.h"

void task_add_page(Task* task, Page* page) {
	page->prev = NULL;
	page->next = task->allocated_pages;
	if (page->next) {
		page->next->prev = page;
	}
	task->allocated_pages = page;
}

void task_remove_page(Task* task, struct Page* page) {
	if (page->prev) {
		page->prev->next = page->next;
	}
	else {
		task->allocated_pages = page->next;
	}
	if (page->next) {
		page->next->prev = page->prev;
	}
}

Task* ACTIVE_INPUT_TASK = NULL;