struc Task
	.name: resb 128
	.rsp: resq 1
	.kernel_rsp: resq 1
	.next: resq 1
	.map: resq 1
	.sleep_end: resq 1
	.stack_base: resq 1
	.stack_size: resq 1
	.status: resb 1
	.task_level: resb 1
	.user: resb 1
	.reserved: resb 5
endstruc