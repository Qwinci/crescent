global switch_task

extern ready_tasks
extern ready_tasks_end
extern sched_lock

%define STATUS_READY 1
%define STATUS_RUNNING 0

%include "sched/task.inc"

; Task* switch_task(Task* old_task, Task* new_task)
switch_task:
	push rbx
	push rbp
	push r12
	push r13
	push r14
	push r15

	mov qword [rdi + Task.rsp], rsp

	; load the new rsp
	mov rsp, [rsi + Task.rsp]

	mov rax, rdi

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx

	ret