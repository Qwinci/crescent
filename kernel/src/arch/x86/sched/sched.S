// X86Task* x86_switch_task(X86Task* old_task, X86Task* new_task);
.global x86_switch_task
.type x86_switch_task, @function

.set X86Task, 0
.set X86Task.self, X86Task
.set X86Task.rsp, X86Task.self + 8

x86_switch_task:
	push %rbx
	push %rbp
	push %r12
	push %r13
	push %r14
	push %r15

	mov %rsp, X86Task.rsp(%rdi)
	mov X86Task.rsp(%rsi), %rsp

	mov %rdi, %rax

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbp
	pop %rbx
	ret

.section .note.GNU-stack
