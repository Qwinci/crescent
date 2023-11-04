#include "task.h"
#include "mem/page.h"

Task* ACTIVE_INPUT_TASK = NULL;
Spinlock ACTIVE_INPUT_TASK_LOCK = {};