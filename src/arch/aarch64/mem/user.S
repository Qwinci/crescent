.globl mem_copy_to_user
.globl mem_copy_to_kernel
.globl atomic_load32_user

#define HANDLER_IP_OFF 16
#define HANDLER_SP_OFF 24

mem_copy_safe:
// x4 == tpidr_el1
	adrp x3, 1f
	add x3, x3, :lo12:1f
	str x3, [x4, HANDLER_IP_OFF]

	// msr pan, #0
	msr s0_0_c4_c0_4, xzr
	bl memcpy
	// msr pan, #1
	msr s0_0_c4_c1_4, xzr

	mov x0, #1
	ldp x29, x30, [sp], #16
	ret
1:
	// msr pan, #1
	msr s0_0_c4_c1_4, xzr

	mrs x1, tpidr_el1
	str xzr, [x1, HANDLER_IP_OFF]
	ldr x2, [x1, HANDLER_SP_OFF]
	mov sp, x2

	mov x0, #0
	ldp x29, x30, [sp], #16
	ret

mem_copy_unsafe:
// x4 == tpidr_el1
	adrp x3, 1f
	add x3, x3, :lo12:1f
	str x3, [x4, HANDLER_IP_OFF]

	bl memcpy

	mov x0, #1
	ldp x29, x30, [sp], #16
	ret
1:
	mrs x1, tpidr_el1
	str xzr, [x1, HANDLER_IP_OFF]
	ldr x2, [x1, HANDLER_SP_OFF]
	mov sp, x2

	mov x0, #0
	ldp x29, x30, [sp], #16
	ret

.type mem_copy_to_user, @function
mem_copy_to_user:
	stp x29, x30, [sp, #-16]!

	adrp x3, HHDM_START
	add x3, x3, :lo12:HHDM_START
	ldr x3, [x3]
	cmp x0, x3
	bhs 1f
	add x4, x0, x2
	cmp x4, x3
	bhi 1f

	mov x6, sp
	mrs x4, tpidr_el1
	str x6, [x4, HANDLER_SP_OFF]

	adrp x3, CPU_FEATURES
	add x3, x3, :lo12:CPU_FEATURES
	ldrb w3, [x3]
	cmp x3, #0
	beq mem_copy_unsafe
	b mem_copy_safe
1:
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
	bhs 1f
	add x4, x1, x2
	cmp x4, x3
	bhi 1f

	mov x6, sp
	mrs x4, tpidr_el1
	str x6, [x4, HANDLER_SP_OFF]

	adrp x3, CPU_FEATURES
	add x3, x3, :lo12:CPU_FEATURES
	ldrb w3, [x3]
	cmp x3, #0
	beq mem_copy_unsafe
	b mem_copy_safe
1:
	mov x0, #0
	ldp x29, x30, [sp], #16
	ret

atomic_load32_safe:
// x4 == tpidr_el1
	adrp x3, 1f
	add x3, x3, :lo12:1f
	str x3, [x4, HANDLER_IP_OFF]

	// msr pan, #0
	msr s0_0_c4_c0_4, xzr
	ldar w0, [x0]
	str w0, [x1]
	// msr pan, #1
	msr s0_0_c4_c1_4, xzr

	mov x0, #1
	ret
1:
	// msr pan, #1
	msr s0_0_c4_c1_4, xzr

	mrs x1, tpidr_el1
	str xzr, [x1, HANDLER_IP_OFF]
	ldr x2, [x1, HANDLER_SP_OFF]
	mov sp, x2

	mov x0, #0
	ret

atomic_load32_unsafe:
// x4 == tpidr_el1
	adrp x3, 1f
	add x3, x3, :lo12:1f
	str x3, [x4, HANDLER_IP_OFF]

	ldar w0, [x0]
	str w0, [x1]

	mov x0, #1
	ret
1:
	mrs x1, tpidr_el1
	str xzr, [x1, HANDLER_IP_OFF]
	ldr x2, [x1, HANDLER_SP_OFF]
	mov sp, x2

	mov x0, #0
	ret

.type atomic_load32_user, @function
atomic_load32_user:
	adrp x3, HHDM_START
	add x3, x3, :lo12:HHDM_START
	ldr x3, [x3]
	cmp x0, x3
	bhs 1f
	add x4, x0, #4
	cmp x4, x3
	bhi 1f

	mov x6, sp
	mrs x4, tpidr_el1
	str x6, [x4, HANDLER_SP_OFF]

	adrp x3, CPU_FEATURES
	add x3, x3, :lo12:CPU_FEATURES
	ldrb w3, [x3]
	cmp x3, #0
	beq atomic_load32_unsafe
	b atomic_load32_safe
1:
	mov x0, #0
	ret

.section .note.GNU-stack
