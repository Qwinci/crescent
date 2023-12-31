#include "posix.h"
#include "arch/cpu.h"
#include "crescent/sys.h"
#include "fs.h"
#include "mem/allocator.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "mem/vm.h"

int fd_table_insert(FdTable* self, VNode* vnode) {
	if (self->len > INT32_MAX) {
		return ERR_NO_MEM;
	}

	if (self->len == self->cap) {
		usize new_cap = self->cap < 32 ? 32 : (self->cap + self->cap / 2);
		FdEntry* new_fds = (FdEntry*) krealloc(self->fds, self->cap * sizeof(FdEntry), new_cap * sizeof(FdEntry));
		if (!new_fds) {
			return ERR_NO_MEM;
		}
		self->cap = new_cap;
		self->fds = new_fds;
		memset(new_fds + self->len, 0, (self->cap - self->len) * sizeof(FdEntry));
	}
	int fd = (int) self->len++;
	self->fds[fd].vnode = vnode;
	self->fds[fd].free = false;
	self->fds[fd].offset = 0;
	return fd;
}

int sys_posix_open(__user const char* path, size_t path_len) {
	char* buf = kmalloc(path_len + 1);
	if (!buf) {
		return ERR_NO_MEM;
	}
	if (!arch_mem_copy_to_kernel(buf, path, path_len)) {
		kfree(buf, path_len + 1);
		return ERR_FAULT;
	}
	buf[path_len] = 0;

	VNode* ret_node;
	int status = kernel_fs_open_ex("posix_rootfs", sizeof("posix_rootfs") - 1, buf, &ret_node);
	kfree(buf, path_len + 1);

	if (status != 0) {
		return status;
	}

	Task* task = arch_get_cur_task();

	return fd_table_insert(&task->process->fd_table, ret_node);
}

static FdEntry* get_fd(int fd) {
	Task* task = arch_get_cur_task();
	if (fd >= (int) task->process->fd_table.len || task->process->fd_table.fds[fd].free) {
		return NULL;
	}
	return &task->process->fd_table.fds[fd];
}

int sys_posix_close(int fd) {
	FdEntry* entry = get_fd(fd);
	if (!entry) {
		return ERR_INVALID_ARG;
	}
	entry->vnode->ops.release(entry->vnode);
	entry->free = true;
	entry->vnode = NULL;
	entry->offset = 0;
	// todo reuse fd
	return 0;
}

int sys_posix_read(int fd, __user void* buffer, size_t size) {
	FdEntry* entry = get_fd(fd);
	if (!entry) {
		return ERR_INVALID_ARG;
	}

	void* buf = kmalloc(size);
	if (!buf) {
		return ERR_NO_MEM;
	}
	int status;
	if ((status = entry->vnode->ops.read(entry->vnode, buf, entry->offset, size)) != 0) {
		kfree(buf, size);
		return status;
	}
	entry->offset += size;

	if (!arch_mem_copy_to_user(buffer, buf, size)) {
		kfree(buf, size);
		return ERR_FAULT;
	}
	kfree(buf, size);

	return 0;
}

int sys_posix_write(int fd, __user const void* buffer, size_t size) {
	FdEntry* entry = get_fd(fd);
	if (!entry) {
		return ERR_INVALID_ARG;
	}

	void* buf = kmalloc(size);
	if (!buf) {
		return ERR_NO_MEM;
	}
	if (!arch_mem_copy_to_kernel(buf, buffer, size)) {
		kfree(buf, size);
		return ERR_FAULT;
	}

	int status;
	if ((status = entry->vnode->ops.write(entry->vnode, buf, entry->offset, size)) != 0) {
		kfree(buf, size);
		return status;
	}
	entry->offset += size;
	kfree(buf, size);

	return 0;
}

CrescentSeekOff sys_posix_seek(int fd, CrescentSeekType type, CrescentSeekOff offset) {
	FdEntry* entry = get_fd(fd);
	if (!entry) {
		return ERR_INVALID_ARG;
	}

	switch (type) {
		case KSEEK_SET:
			entry->offset = (usize) offset;
			break;
		case KSEEK_END:
		{
			CrescentStat s;
			int status;
			if ((status = entry->vnode->ops.stat(entry->vnode, &s)) != 0) {
				return status;
			}
			entry->offset = s.size + offset;
			break;
		}
		case KSEEK_CUR:
			entry->offset += offset;
			break;
	}

	return (CrescentSeekOff) entry->offset;
}

int sys_posix_mmap(void* hint, size_t size, int prot, void* __user* window) {
	usize count = ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE;

	MappingFlags flags = MAPPING_FLAG_ZEROED;
	if (prot & KPROT_READ) {
		flags |= MAPPING_FLAG_R;
	}
	if (prot & KPROT_WRITE) {
		flags |= MAPPING_FLAG_W;
	}
	if (prot & KPROT_EXEC) {
		flags |= MAPPING_FLAG_X;
	}

	Task* task = arch_get_cur_task();
	if (hint) {
		bool high = hint >= task->process->high_vmem.base;
		void* res = vm_user_alloc_on_demand(task->process, hint, count, flags, high, NULL);
		if (!res) {
			return ERR_NO_MEM;
		}
		if (!arch_mem_copy_to_user(window, &res, sizeof(void*))) {
			vm_user_dealloc_on_demand(task->process, res, count, high, NULL);
			return ERR_FAULT;
		}
	}
	else {
		void* res = vm_user_alloc_on_demand(task->process, NULL, count, flags, true, NULL);
		if (!res) {
			return ERR_NO_MEM;
		}
		if (!arch_mem_copy_to_user(window, &res, sizeof(void*))) {
			vm_user_dealloc_on_demand(task->process, res, count, true, NULL);
			return ERR_FAULT;
		}
	}

	return 0;
}

static Mutex FUTEX_MUTEX = {};
static RbTree FUTEX_RB_TREE = {};

typedef struct {
	RbTreeNode hook;
	usize phys;
	Task* waiters;
	Mutex waiters_lock;
} FutexNode;

static void futex_rb_tree_insert(FutexNode* node) {
	if (!FUTEX_RB_TREE.root) {
		rb_tree_insert_root(&FUTEX_RB_TREE, &node->hook);
		return;
	}

	FutexNode* n = container_of(FUTEX_RB_TREE.root, FutexNode, hook);
	while (true) {
		if (node->phys < n->phys) {
			if (!n->hook.left) {
				rb_tree_insert_left(&FUTEX_RB_TREE, &n->hook, &node->hook);
				break;
			}
			n = container_of(n->hook.left, FutexNode, hook);
		}
		else {
			if (!n->hook.right) {
				rb_tree_insert_right(&FUTEX_RB_TREE, &n->hook, &node->hook);
				break;
			}
			n = container_of(n->hook.right, FutexNode, hook);
		}
	}
}

static FutexNode* futex_rb_tree_get(usize phys) {
	FutexNode* node = container_of(FUTEX_RB_TREE.root, FutexNode, hook);
	while (node) {
		if (phys < node->phys) {
			node = container_of(node->hook.left, FutexNode, hook);
		}
		else if (phys > node->phys) {
			node = container_of(node->hook.right, FutexNode, hook);
		}
		else {
			return node;
		}
	}
	return NULL;
}

int sys_posix_futex_wait(__user int* ptr, int expected) {
	int value;
	if (!arch_mem_copy_to_kernel(&value, ptr, sizeof(int))) {
		return ERR_FAULT;
	}
	if (value != expected) {
		return ERR_TRY_AGAIN;
	}

	usize phys = to_phys_generic((void*) ptr);

	mutex_lock(&FUTEX_MUTEX);
	FutexNode* node = futex_rb_tree_get(phys);

	if (!node) {
		node = kcalloc(sizeof(FutexNode));
		if (!node) {
			mutex_unlock(&FUTEX_MUTEX);
			return ERR_NO_MEM;
		}
		node->phys = phys;
		futex_rb_tree_insert(node);
	}

	mutex_lock(&node->waiters_lock);
	Task* self = arch_get_cur_task();
	self->next = node->waiters;
	node->waiters = self;
	mutex_unlock(&node->waiters_lock);

	mutex_unlock(&FUTEX_MUTEX);

	sched_block(TASK_STATUS_WAITING);

	return 0;
}

int sys_posix_futex_wake(__user int* ptr) {
	usize phys = to_phys_generic((void*) ptr);

	mutex_lock(&FUTEX_MUTEX);
	FutexNode* node = futex_rb_tree_get(phys);
	if (!node) {
		mutex_unlock(&FUTEX_MUTEX);
		return 0;
	}

	mutex_lock(&node->waiters_lock);
	Task* task = node->waiters;
	node->waiters = node->waiters->next;
	if (!node->waiters) {
		rb_tree_remove(&FUTEX_RB_TREE, &node->hook);
		kfree(node, sizeof(FutexNode));
	}
	else {
		mutex_unlock(&node->waiters_lock);
	}

	sched_unblock(task);

	mutex_unlock(&FUTEX_MUTEX);
	return 0;
}
