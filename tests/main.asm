global _start

; i64 sys_fbmn_cur_info(u32* width, u32* height, u32* depth)
; void* sys_fbmn_create(u32 width, u32 height, u32 depth)
; i64 sys_fbmn_swap(void* fb, u32 width, u32 height, u32 depth)

%define SYS_EXIT 0
%define SYS_FBMN_CUR_INFO 1
%define SYS_FBMN_CREATE 2
%define SYS_FBMN_SWAP 3

_start:
	sub rsp, 16

	; get current fb info
	mov eax, SYS_FBMN_CUR_INFO
	lea rdi, [rsp] ; width
	lea rsi, [rsp + 4] ; height
	lea rdx, [rsp + 8] ; depth
	syscall

	; create a new fb with the same info
	mov eax, SYS_FBMN_CREATE
	mov edi, dword [rsp] ; width
	mov esi, dword [rsp + 4] ; height
	mov edx, dword [rsp + 8] ; depth
	syscall

	cmp rax, 0
	jle .exit_fail

	xor ecx, ecx
	; fb in rdx
	mov rdx, rax

	mov dword [rdx], 0xFF0000

	; move fb to rdi for swap
	mov rdi, rax

	mov eax, SYS_FBMN_SWAP
	mov esi, dword [rsp] ; width
	mov edx, dword [rsp + 4] ; height
	mov r8d, dword [rsp + 8] ; depth
	syscall

	cmp rax, 0
	jl .exit_fail

	add rsp, 16

	mov eax, SYS_EXIT
	xor edi, edi
	syscall

	.exit_fail:
	mov eax, SYS_EXIT
	mov edi, 1
	syscall