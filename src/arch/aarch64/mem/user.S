.globl mem_copy_to_user
.globl mem_copy_to_kernel

.type mem_copy_to_user, @function
mem_copy_to_user:
	stp x29, x30, [sp, #-16]!

	adrp x3, HHDM_START
	add x3, x3, :lo12:HHDM_START
	ldr x3, [x3]
	cmp x0, x3
	bhs 2f
	add x4, x0, x2
	cmp x4, x3
	bhi 2f

	adrp x5, 1f
	add x5, x5, :lo12:1f
	mov x6, sp

	mrs x4, tpidr_el1
	stp x5, x6, [x4, #24]

	bl memcpy

	mrs x4, tpidr_el1
	str xzr, [x4, #24]

	mov x0, #1
	ldp x29, x30, [sp], #16
	ret

1:
	mrs x1, tpidr_el1
	str xzr, [x1, #24]
	ldr x2, [x1, #32]
	mov sp, x2
2:
	mov x0, #0
	ldp x29, x30, [sp], #16
	ret

.type mem_copy_to_kernel, @function
mem_copy_to_kernel:
	stp x29, x30, [sp, #-16]!

	adrp x3, HHDM_START
	add x3, x3, :lo12:HHDM_START
	ldr x3, [x3]
	cmp x1, x3
	bhs 2f
	add x4, x1, x2
	cmp x4, x3
	bhi 2f

	adrp x5, 1f
	add x5, x5, :lo12:1f
	mov x6, sp

	mrs x4, tpidr_el1
	stp x5, x6, [x4, #24]

	bl memcpy

	mrs x4, tpidr_el1
	str xzr, [x4, #24]

	mov x0, #1
	ldp x29, x30, [sp], #16
	ret

1:
	mrs x1, tpidr_el1
	str xzr, [x1, #24]
	ldr x2, [x1, #32]
	mov sp, x2
2:
	mov x0, #0
	ldp x29, x30, [sp], #16
	ret

.section .note.GNU-stack
