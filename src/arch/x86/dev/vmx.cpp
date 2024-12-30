#include "vmx.hpp"
#include "arch/paging.hpp"
#include "arch/x86/cpu.hpp"
#include "array.hpp"
#include "dev/evm.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "utils/cpu_var.hpp"

namespace {
	namespace fields {
		struct Field {
			enum class Type : u64 {
				Control = 0,
				VmExitInfo = 1 << 10,
				GuestState = 2 << 10,
				HostState = 3 << 10
			};

			enum class Width : u64 {
				Bit16 = 0,
				Bit64 = 1 << 13,
				Bit32 = 2 << 13,
				Natural = 3 << 13
			};

			constexpr Field(Type type, Width width, u16 index) {
				value = static_cast<u64>(type) | static_cast<u64>(width) | index << 1;
			}

			u64 value;
		};

		constexpr Field VPID {Field::Type::Control, Field::Width::Bit16, 0};
		constexpr Field POSTED_INT_NOTIFICATION_VECTOR {
			Field::Type::Control,
			Field::Width::Bit16,
			1};
		constexpr Field EPTP_INDEX {Field::Type::Control, Field::Width::Bit16, 2};
		constexpr Field HLAT_PREFIX_SIZE {Field::Type::Control, Field::Width::Bit16, 3};
		constexpr Field LAST_PID_POINTER_INDEX {Field::Type::Control, Field::Width::Bit16, 4};

		constexpr Field GUEST_ES {Field::Type::GuestState, Field::Width::Bit16, 0};
		constexpr Field GUEST_CS {Field::Type::GuestState, Field::Width::Bit16, 1};
		constexpr Field GUEST_SS {Field::Type::GuestState, Field::Width::Bit16, 2};
		constexpr Field GUEST_DS {Field::Type::GuestState, Field::Width::Bit16, 3};
		constexpr Field GUEST_FS {Field::Type::GuestState, Field::Width::Bit16, 4};
		constexpr Field GUEST_GS {Field::Type::GuestState, Field::Width::Bit16, 5};
		constexpr Field GUEST_LDTR {Field::Type::GuestState, Field::Width::Bit16, 6};
		constexpr Field GUEST_TR {Field::Type::GuestState, Field::Width::Bit16, 7};
		constexpr Field GUEST_INT_STATUS {Field::Type::GuestState, Field::Width::Bit16, 8};
		constexpr Field GUEST_PML_INDEX {Field::Type::GuestState, Field::Width::Bit16, 9};
		constexpr Field GUEST_UINV {Field::Type::GuestState, Field::Width::Bit16, 10};

		constexpr Field HOST_ES {Field::Type::HostState, Field::Width::Bit16, 0};
		constexpr Field HOST_CS {Field::Type::HostState, Field::Width::Bit16, 1};
		constexpr Field HOST_SS {Field::Type::HostState, Field::Width::Bit16, 2};
		constexpr Field HOST_DS {Field::Type::HostState, Field::Width::Bit16, 3};
		constexpr Field HOST_FS {Field::Type::HostState, Field::Width::Bit16, 4};
		constexpr Field HOST_GS {Field::Type::HostState, Field::Width::Bit16, 5};
		constexpr Field HOST_TR {Field::Type::HostState, Field::Width::Bit16, 6};

		constexpr Field IO_BITMAP_A_ADDR {Field::Type::Control, Field::Width::Bit64, 0};
		constexpr Field IO_BITMAP_B_ADDR {Field::Type::Control, Field::Width::Bit64, 1};
		constexpr Field MSR_BITMAP_ADDR {Field::Type::Control, Field::Width::Bit64, 2};
		constexpr Field VM_EXIT_MSR_STORE_ADDR {Field::Type::Control, Field::Width::Bit64, 3};
		constexpr Field VM_EXIT_MSR_LOAD_ADDR {Field::Type::Control, Field::Width::Bit64, 4};
		constexpr Field VM_ENTRY_MSR_LOAD_ADDR {Field::Type::Control, Field::Width::Bit64, 5};
		constexpr Field EXECUTIVE_VMCS_PTR {Field::Type::Control, Field::Width::Bit64, 6};
		constexpr Field PML_ADDR {Field::Type::Control, Field::Width::Bit64, 7};
		constexpr Field TSC_OFFSET {Field::Type::Control, Field::Width::Bit64, 8};
		constexpr Field VIRT_APIC_ADDR {Field::Type::Control, Field::Width::Bit64, 9};
		constexpr Field APIC_ACCESS_ADDR {Field::Type::Control, Field::Width::Bit64, 10};
		constexpr Field POSTED_INT_DESC_ADDR {Field::Type::Control, Field::Width::Bit64, 11};
		constexpr Field VM_FUNC_CONTROLS {Field::Type::Control, Field::Width::Bit64, 12};
		constexpr Field EPT_PTR {Field::Type::Control, Field::Width::Bit64, 13};
		constexpr Field EOI_EXIT0 {Field::Type::Control, Field::Width::Bit64, 14};
		constexpr Field EOI_EXIT1 {Field::Type::Control, Field::Width::Bit64, 15};
		constexpr Field EOI_EXIT2 {Field::Type::Control, Field::Width::Bit64, 16};
		constexpr Field EOI_EXIT3 {Field::Type::Control, Field::Width::Bit64, 17};
		constexpr Field EPTP_LIST_ADDR {Field::Type::Control, Field::Width::Bit64, 18};
		constexpr Field VMREAD_BITMAP_ADDR {Field::Type::Control, Field::Width::Bit64, 19};
		constexpr Field VMWRITE_BITMAP_ADDR {Field::Type::Control, Field::Width::Bit64, 20};
		constexpr Field VIRT_EXCEPTION_INFO_ADDR {Field::Type::Control, Field::Width::Bit64, 21};
		constexpr Field XSS_EXITING_BITMAP {Field::Type::Control, Field::Width::Bit64, 22};
		constexpr Field ENCLS_EXITING_BITMAP {Field::Type::Control, Field::Width::Bit64, 23};
		constexpr Field SUB_PAGE_PERM_TABLE_PTR {Field::Type::Control, Field::Width::Bit64, 24};
		constexpr Field TSC_MULTIPLIER {Field::Type::Control, Field::Width::Bit64, 25};
		constexpr Field TERTIARY_PROCESSOR_BASED_VM_EXEC_CONTROLS {
			Field::Type::Control,
			Field::Width::Bit64,
			26};
		constexpr Field ENCLV_EXITING_BITMAP {Field::Type::Control, Field::Width::Bit64, 27};
		constexpr Field LOW_PASID_DIR_ADDR {Field::Type::Control, Field::Width::Bit64, 28};
		constexpr Field HIGH_PASID_DIR_ADDR {Field::Type::Control, Field::Width::Bit64, 29};
		constexpr Field SHARED_EPT_PTR {Field::Type::Control, Field::Width::Bit64, 30};
		constexpr Field PCONFIG_EXITING_BITMAP {Field::Type::Control, Field::Width::Bit64, 31};
		constexpr Field HLATP {Field::Type::Control, Field::Width::Bit64, 32};
		constexpr Field PID_PTR_TABLE_ADDR {Field::Type::Control, Field::Width::Bit64, 33};
		constexpr Field SECONDARY_VM_EXIT_CONTROLS {Field::Type::Control, Field::Width::Bit64, 34};
		constexpr Field IA32_SPEC_CTRL_MASK {Field::Type::Control, Field::Width::Bit64, 37};
		constexpr Field IA32_SPEC_CTRL_SHADOW {Field::Type::Control, Field::Width::Bit64, 38};

		constexpr Field GUEST_PHYS_ADDR {Field::Type::VmExitInfo, Field::Width::Bit64, 0};

		constexpr Field VMCS_LINK_PTR {Field::Type::GuestState, Field::Width::Bit64, 0};
		constexpr Field GUEST_IA32_DEBUGCTL {Field::Type::GuestState, Field::Width::Bit64, 1};
		constexpr Field GUEST_IA32_PAT {Field::Type::GuestState, Field::Width::Bit64, 2};
		constexpr Field GUEST_IA32_EFER {Field::Type::GuestState, Field::Width::Bit64, 3};
		constexpr Field GUEST_IA32_PERF_GLOBAL_CTRL {Field::Type::GuestState, Field::Width::Bit64, 4};
		constexpr Field GUEST_PDPTE0 {Field::Type::GuestState, Field::Width::Bit64, 5};
		constexpr Field GUEST_PDPTE1 {Field::Type::GuestState, Field::Width::Bit64, 6};
		constexpr Field GUEST_PDPTE2 {Field::Type::GuestState, Field::Width::Bit64, 7};
		constexpr Field GUEST_PDPTE3 {Field::Type::GuestState, Field::Width::Bit64, 8};
		constexpr Field GUEST_IA32_BNDCFGS {Field::Type::GuestState, Field::Width::Bit64, 9};
		constexpr Field GUEST_IA32_RTIT_CTL {Field::Type::GuestState, Field::Width::Bit64, 10};
		constexpr Field GUEST_IA32_LBR_CTL {Field::Type::GuestState, Field::Width::Bit64, 11};
		constexpr Field GUEST_IA32_PKRS {Field::Type::GuestState, Field::Width::Bit64, 12};

		constexpr Field HOST_IA32_PAT {Field::Type::HostState, Field::Width::Bit64, 0};
		constexpr Field HOST_IA32_EFER {Field::Type::HostState, Field::Width::Bit64, 1};
		constexpr Field HOST_IA32_PERF_GLOBAL_CTRL {Field::Type::HostState, Field::Width::Bit64, 2};
		constexpr Field HOST_IA32_PKRS {Field::Type::HostState, Field::Width::Bit64, 3};

		constexpr Field PIN_BASED_VM_EXEC_CONTROLS {Field::Type::Control, Field::Width::Bit32, 0};
		constexpr Field PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROLS {
			Field::Type::Control,
			Field::Width::Bit32,
			1};
		constexpr Field EXCEPTION_BITMAP {Field::Type::Control, Field::Width::Bit32, 2};
		constexpr Field PAGE_FAULT_ERR_CODE_MASK {Field::Type::Control, Field::Width::Bit32, 3};
		constexpr Field PAGE_FAULT_ERR_CODE_MATCH {Field::Type::Control, Field::Width::Bit32, 4};
		constexpr Field CR3_TARGET_COUNT {Field::Type::Control, Field::Width::Bit32, 5};
		constexpr Field PRIMARY_VM_EXIT_CONTROLS {Field::Type::Control, Field::Width::Bit32, 6};
		constexpr Field VM_EXIT_MSR_STORE_COUNT {Field::Type::Control, Field::Width::Bit32, 7};
		constexpr Field VM_EXIT_MSR_LOAD_COUNT {Field::Type::Control, Field::Width::Bit32, 8};
		constexpr Field VM_ENTRY_CONTROLS {Field::Type::Control, Field::Width::Bit32, 9};
		constexpr Field VM_ENTRY_MSR_LOAD_COUNT {Field::Type::Control, Field::Width::Bit32, 10};
		constexpr Field VM_ENTRY_INT_INFO {Field::Type::Control, Field::Width::Bit32, 11};
		constexpr Field VM_ENTRY_EXCEPTION_ERR_CODE {Field::Type::Control, Field::Width::Bit32, 12};
		constexpr Field VM_ENTRY_INST_LEN {Field::Type::Control, Field::Width::Bit32, 13};
		constexpr Field TPR_THRESHOLD {Field::Type::Control, Field::Width::Bit32, 14};
		constexpr Field SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROLS {
			Field::Type::Control,
			Field::Width::Bit32,
			15};
		constexpr Field PLE_GAP {Field::Type::Control, Field::Width::Bit32, 16};
		constexpr Field PLE_WINDOW {Field::Type::Control, Field::Width::Bit32, 17};
		constexpr Field INST_TIMEOUT_CONTROL {Field::Type::Control, Field::Width::Bit32, 18};

		constexpr Field VM_INST_ERROR {Field::Type::VmExitInfo, Field::Width::Bit32, 0};
		constexpr Field EXIT_REASON {Field::Type::VmExitInfo, Field::Width::Bit32, 1};
		constexpr Field VM_EXIT_INT_INFO {Field::Type::VmExitInfo, Field::Width::Bit32, 2};
		constexpr Field VM_EXIT_INT_ERR_CODE {Field::Type::VmExitInfo, Field::Width::Bit32, 3};
		constexpr Field IDT_VECTORING_INFO {Field::Type::VmExitInfo, Field::Width::Bit32, 4};
		constexpr Field IDT_VECTORING_ERR_CODE {Field::Type::VmExitInfo, Field::Width::Bit32, 5};
		constexpr Field VM_EXIT_INST_LEN {Field::Type::VmExitInfo, Field::Width::Bit32, 6};
		constexpr Field VM_EXIT_INST_INFO {Field::Type::VmExitInfo, Field::Width::Bit32, 7};

		constexpr Field GUEST_ES_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 0};
		constexpr Field GUEST_CS_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 1};
		constexpr Field GUEST_SS_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 2};
		constexpr Field GUEST_DS_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 3};
		constexpr Field GUEST_FS_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 4};
		constexpr Field GUEST_GS_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 5};
		constexpr Field GUEST_LDTR_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 6};
		constexpr Field GUEST_TR_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 7};
		constexpr Field GUEST_GDTR_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 8};
		constexpr Field GUEST_IDTR_LIMIT {Field::Type::GuestState, Field::Width::Bit32, 9};
		constexpr Field GUEST_ES_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 10};
		constexpr Field GUEST_CS_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 11};
		constexpr Field GUEST_SS_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 12};
		constexpr Field GUEST_DS_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 13};
		constexpr Field GUEST_FS_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 14};
		constexpr Field GUEST_GS_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 15};
		constexpr Field GUEST_LDTR_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 16};
		constexpr Field GUEST_TR_ACCESS_RIGHTS {Field::Type::GuestState, Field::Width::Bit32, 17};
		constexpr Field GUEST_INT_STATE {Field::Type::GuestState, Field::Width::Bit32, 18};
		constexpr Field GUEST_ACTIVITY_STATE {Field::Type::GuestState, Field::Width::Bit32, 19};
		constexpr Field GUEST_SMBASE {Field::Type::GuestState, Field::Width::Bit32, 20};
		constexpr Field GUEST_IA32_SYSENTER_CS {Field::Type::GuestState, Field::Width::Bit32, 21};
		constexpr Field GUEST_VMX_PREEMPTION_TIMER {Field::Type::GuestState, Field::Width::Bit32, 23};

		constexpr Field HOST_IA32_SYSENTER_CS {Field::Type::HostState, Field::Width::Bit32, 0};

		constexpr Field CR0_GUEST_HOST_MASK {Field::Type::Control, Field::Width::Natural, 0};
		constexpr Field CR4_GUEST_HOST_MASK {Field::Type::Control, Field::Width::Natural, 1};
		constexpr Field CR0_READ_SHADOW {Field::Type::Control, Field::Width::Natural, 2};
		constexpr Field CR4_READ_SHADOW {Field::Type::Control, Field::Width::Natural, 3};
		constexpr Field CR3_TARGET_VALUE0 {Field::Type::Control, Field::Width::Natural, 4};
		constexpr Field CR3_TARGET_VALUE1 {Field::Type::Control, Field::Width::Natural, 5};
		constexpr Field CR3_TARGET_VALUE2 {Field::Type::Control, Field::Width::Natural, 6};
		constexpr Field CR3_TARGET_VALUE3 {Field::Type::Control, Field::Width::Natural, 7};

		constexpr Field EXIT_QUALIFICATION {Field::Type::VmExitInfo, Field::Width::Natural, 0};
		constexpr Field IO_RCX {Field::Type::VmExitInfo, Field::Width::Natural, 1};
		constexpr Field IO_RSI {Field::Type::VmExitInfo, Field::Width::Natural, 2};
		constexpr Field IO_RDI {Field::Type::VmExitInfo, Field::Width::Natural, 3};
		constexpr Field IO_RIP {Field::Type::VmExitInfo, Field::Width::Natural, 4};
		constexpr Field GUEST_LINEAR_ADDR {Field::Type::VmExitInfo, Field::Width::Natural, 5};

		constexpr Field GUEST_CR0 {Field::Type::GuestState, Field::Width::Natural, 0};
		constexpr Field GUEST_CR3 {Field::Type::GuestState, Field::Width::Natural, 1};
		constexpr Field GUEST_CR4 {Field::Type::GuestState, Field::Width::Natural, 2};
		constexpr Field GUEST_ES_BASE {Field::Type::GuestState, Field::Width::Natural, 3};
		constexpr Field GUEST_CS_BASE {Field::Type::GuestState, Field::Width::Natural, 4};
		constexpr Field GUEST_SS_BASE {Field::Type::GuestState, Field::Width::Natural, 5};
		constexpr Field GUEST_DS_BASE {Field::Type::GuestState, Field::Width::Natural, 6};
		constexpr Field GUEST_FS_BASE {Field::Type::GuestState, Field::Width::Natural, 7};
		constexpr Field GUEST_GS_BASE {Field::Type::GuestState, Field::Width::Natural, 8};
		constexpr Field GUEST_LDTR_BASE {Field::Type::GuestState, Field::Width::Natural, 9};
		constexpr Field GUEST_TR_BASE {Field::Type::GuestState, Field::Width::Natural, 10};
		constexpr Field GUEST_GDTR_BASE {Field::Type::GuestState, Field::Width::Natural, 11};
		constexpr Field GUEST_IDTR_BASE {Field::Type::GuestState, Field::Width::Natural, 12};
		constexpr Field GUEST_DR7 {Field::Type::GuestState, Field::Width::Natural, 13};
		constexpr Field GUEST_RSP {Field::Type::GuestState, Field::Width::Natural, 14};
		constexpr Field GUEST_RIP {Field::Type::GuestState, Field::Width::Natural, 15};
		constexpr Field GUEST_RFLAGS {Field::Type::GuestState, Field::Width::Natural, 16};
		constexpr Field GUEST_PENDING_DEBUG_EXCEPTIONS {
			Field::Type::GuestState,
			Field::Width::Natural,
			17};
		constexpr Field GUEST_IA32_SYSENTER_ESP {Field::Type::GuestState, Field::Width::Natural, 18};
		constexpr Field GUEST_IA32_SYSENTER_EIP {Field::Type::GuestState, Field::Width::Natural, 19};
		constexpr Field GUEST_IA32_S_CET {Field::Type::GuestState, Field::Width::Natural, 20};
		constexpr Field GUEST_SSP {Field::Type::GuestState, Field::Width::Natural, 21};
		constexpr Field GUEST_IA32_INT_SSP_TABLE_ADDR {
			Field::Type::GuestState,
			Field::Width::Natural,
			22};

		constexpr Field HOST_CR0 {Field::Type::HostState, Field::Width::Natural, 0};
		constexpr Field HOST_CR3 {Field::Type::HostState, Field::Width::Natural, 1};
		constexpr Field HOST_CR4 {Field::Type::HostState, Field::Width::Natural, 2};
		constexpr Field HOST_FS_BASE {Field::Type::HostState, Field::Width::Natural, 3};
		constexpr Field HOST_GS_BASE {Field::Type::HostState, Field::Width::Natural, 4};
		constexpr Field HOST_TR_BASE {Field::Type::HostState, Field::Width::Natural, 5};
		constexpr Field HOST_GDTR_BASE {Field::Type::HostState, Field::Width::Natural, 6};
		constexpr Field HOST_IDTR_BASE {Field::Type::HostState, Field::Width::Natural, 7};
		constexpr Field HOST_IA32_SYSENTER_ESP {Field::Type::HostState, Field::Width::Natural, 8};
		constexpr Field HOST_IA32_SYSENTER_EIP {Field::Type::HostState, Field::Width::Natural, 9};
		constexpr Field HOST_RSP {Field::Type::HostState, Field::Width::Natural, 10};
		constexpr Field HOST_RIP {Field::Type::HostState, Field::Width::Natural, 11};
		constexpr Field HOST_IA32_S_CET {Field::Type::HostState, Field::Width::Natural, 12};
		constexpr Field HOST_SSP {Field::Type::HostState, Field::Width::Natural, 13};
		constexpr Field HOST_IA32_INT_SSP_TABLE_ADDR {Field::Type::HostState, Field::Width::Natural, 14};
	}
}

struct VmxData {
	VmxData() {
		if (!CPU_FEATURES.vmx) {
			return;
		}

		auto features = msrs::IA32_FEATURE_CONTROL.read();
		if (!(features & 1 << 0) || !(features & 1 << 2)) {
			println("[kernel][x86]: bios has disabled vmx");
			return;
		}

		auto info = msrs::IA32_VMX_BASIC.read();
		vmcs_rev = info;
		vmcs_size = info >> 32 & 0x1FFF;
		default1_clear = info & 1UL << 55;
	}

	u32 vmcs_rev {};
	u32 vmcs_size {};
	bool default1_clear {};
	bool initialized {};
};

namespace {
	per_cpu(VmxData, VMX_DATA, [](void* ptr) {
		new (ptr) VmxData {};
	});
}

struct Ept {
	bool map(u64 guest, u64 host) {
		guest >>= 12;

		u64* level = level0;

		for (int i = 0; i < 3; ++i) {
			u64 index = (guest >> ((3 - i) * 9)) & 0x1FF;

			u64 entry = level[index];
			if (!entry) {
				u64 phys = pmalloc(1);
				if (!phys) {
					return false;
				}

				used_pages.push(Page::from_phys(phys));

				auto* virt = to_virt<u64>(phys);
				memset(virt, 0, PAGE_SIZE);

				level[index] = phys | 0b111;
				level = virt;
			}
			else {
				level = to_virt<u64>(entry & 0x000FFFFFFFFFF000);
			}
		}

		level[guest & 0x1FF] = host | 0b111;
		return true;
	}

	bool map_2mb(u64 guest, u64 host) {
		guest >>= 21;

		u64* level = level0;

		for (int i = 0; i < 2; ++i) {
			u64 index = (guest >> ((2 - i) * 9)) & 0x1FF;

			u64& entry = level[index];
			if (!entry) {
				u64 phys = pmalloc(1);
				if (!phys) {
					return false;
				}

				used_pages.push(Page::from_phys(phys));

				auto* virt = to_virt<u64>(phys);
				memset(virt, 0, PAGE_SIZE);

				entry = phys | 0b11;
				level = virt;
			}
			else {
				level = to_virt<u64>(entry & 0x000FFFFFFFFFF000);
			}
		}

		level[guest & 0x1FF] = host | 1 << 7 | 0b11;
		return true;
	}

	usize unmap(u64 guest, bool large) {
		guest >>= 12;

		u64* level = level0;

		for (int i = 0; i < 3; ++i) {
			u64 index = (guest >> ((3 - i) * 9)) & 0x1FF;

			u64 entry = level[index];
			if (!entry) {
				return 0;
			}
			else if (large && (entry & 1 << 7)) {
				auto phys = entry & 0x000FFFFFFFFFF000;
				level[index] = 0;
				return phys;
			}
			else {
				level = to_virt<u64>(entry & 0x000FFFFFFFFFF000);
			}
		}

		if (!level[guest & 0x1FF]) {
			return 0;
		}

		auto phys = level[guest & 0x1FF] & 0x000FFFFFFFFFF000;

		level[guest & 0x1FF] = 0;

		return phys;
	}

	[[nodiscard]] usize get_phys(u64 guest) const {
		u64 offset = guest & 0xFFF;
		guest >>= 12;

		u64* level = level0;

		for (int i = 0; i < 3; ++i) {
			u64 index = (guest >> ((3 - i) * 9)) & 0x1FF;

			u64 entry = level[index];
			if (!entry) {
				return 0;
			}
			else if (entry & 1 << 7) {
				auto phys = entry & 0x000FFFFFFFFFF000;
				return phys | (guest & 0x1FF) << 12 | offset;
			}
			else {
				level = to_virt<u64>(entry & 0x000FFFFFFFFFF000);
			}
		}

		if (!level[guest & 0x1FF]) {
			return 0;
		}

		auto phys = level[guest & 0x1FF] & 0x000FFFFFFFFFF000;
		return phys | offset;
	}

	~Ept() {
		for (auto& page : used_pages) {
			pfree(page.phys(), 1);
		}
		used_pages.clear();
	}

	u64* level0 {};
	DoubleList<Page, &Page::hook> used_pages {};
};

extern "C" void invalid_vm() {
	println("invalid vm");
}

asm(R"(
.pushsection .text
enter_vmcs:
	push %rbx
	push %rbp
	push %r12
	push %r13
	push %r14
	push %r15
	push %rdi

	mov 16(%rdi), %rbx
	mov %esi, %ebp

	mov %rsp, %rsi
	call vmcs_update_host_rsp

	test %ebp, %ebp

	mov (%rbx), %rax
	mov 16(%rbx), %rcx
	mov 24(%rbx), %rdx
	mov 32(%rbx), %rdi
	mov 40(%rbx), %rsi
	mov 48(%rbx), %rbp
	mov 64(%rbx), %r8
	mov 72(%rbx), %r9
	mov 80(%rbx), %r10
	mov 88(%rbx), %r11
	mov 96(%rbx), %r12
	mov 104(%rbx), %r13
	mov 112(%rbx), %r14
	mov 120(%rbx), %r15
	mov 8(%rbx), %rbx

	jne 0f

	vmlaunch
	jc 1f
	jz 2f
	ud2

0:
	vmresume
	jc 1f
	jz 2f

on_vm_exit:
	push %rdi
	mov 8(%rsp), %rdi
	mov 16(%rdi), %rdi

	mov %rax, (%rdi)
	mov %rbx, 8(%rdi)
	mov %rcx, 16(%rdi)
	mov %rdx, 24(%rdi)
	pop %rax
	mov %rax, 32(%rdi)
	mov %rsi, 40(%rdi)
	mov %rbp, 48(%rdi)
	mov %r8, 64(%rdi)
	mov %r9, 72(%rdi)
	mov %r10, 80(%rdi)
	mov %r11, 88(%rdi)
	mov %r12, 96(%rdi)
	mov %r13, 104(%rdi)
	mov %r14, 112(%rdi)
	mov %r15, 120(%rdi)

	pop %rdi
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbp
	pop %rbx
	xor %eax, %eax
	ret
1:
	pop %rdi
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbp
	pop %rbx
	mov $1, %eax
	ret
2:
	pop %rdi
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbp
	pop %rbx
	mov $2, %eax
	ret
.popsection
)");

extern char on_vm_exit[];

struct Vmcs;

enum class VmError : u32 {
	Success,
	VmLaunchFail,
	VmLaunchFailWithCode
};

extern "C" VmError enter_vmcs(Vmcs* vmcs, bool launched);

static bool vmclear(u64 phys) {
	bool success;
	asm volatile("vmclear %1" : "=@ccnc"(success) : "m"(phys) : "memory");
	return success;
}

static bool vmptrld(u64 phys) {
	bool success;
	asm volatile("vmptrld %1" : "=@ccnc"(success) : "m"(phys) : "memory");
	return success;
}

struct VmxVm final : public evm::Evm {
	VmxVm();

	kstd::expected<kstd::shared_ptr<evm::VirtualCpu>, int> create_vcpu() override;

	int map_page(u64 guest, u64 host) override {
		if ((guest & 0xFFF) || (host & 0xFFF)) {
			return ERR_INVALID_ARGUMENT;
		}

		auto page = Page::from_phys(host);
		assert(page);
		auto guard = page->lock.lock();
		++page->ref_count;

		if (!ept.map(guest, host)) {
			return ERR_NO_MEM;
		}

		return 0;
	}

	void unmap_page(u64 guest) override {
		auto phys = ept.unmap(guest, false);
		assert(phys);
		auto page = Page::from_phys(phys);
		assert(page);
		auto guard = page->lock.lock();
		if (--page->ref_count == 0) {
			pfree(phys, 1);
		}
	}

	Ept ept {};
	u64 entry_exit_tsc_ticks {};
};

namespace {
	namespace exit_reason {
		constexpr u16 EXT_INT = 1;
		constexpr u16 TRIPLE_FAULT = 2;
		constexpr u16 CPUID = 10;
		constexpr u16 HLT = 12;
		constexpr u16 IO_INST = 30;
		constexpr u16 APIC_ACCESS = 44;
		constexpr u16 EPT_VIOLATION = 48;
	}

	constexpr u8 REG_RAX = 0;
	constexpr u8 REG_RCX = 1;
	constexpr u8 REG_RDX = 2;
	constexpr u8 REG_RBX = 3;
	constexpr u8 REG_RSP = 4;
	constexpr u8 REG_RBP = 5;
	constexpr u8 REG_RSI = 6;
	constexpr u8 REG_RDI = 7;
}

struct Vmcs : public evm::VirtualCpu {
	explicit Vmcs(VmxVm* vm) : vm {vm} {
		IrqGuard irq_guard {};
		// todo support migration
		get_current_thread()->pin_cpu = true;

		phys = pmalloc(1);
		assert(phys);

		state_phys = pmalloc(1);
		assert(state_phys);
		state = to_virt<EvmGuestState>(state_phys);
		memset(state, 0, PAGE_SIZE);

		auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : 512;
		host_simd_state = static_cast<u8*>(KERNEL_VSPACE.alloc_backed(simd_size, PageFlags::Read | PageFlags::Write));
		guest_simd_state = static_cast<u8*>(KERNEL_VSPACE.alloc_backed(simd_size, PageFlags::Read | PageFlags::Write));
		memset(host_simd_state, 0, simd_size);
		memset(guest_simd_state, 0, simd_size);

		auto guard = VMX_DATA.get();

		vmclear(phys);

		auto* ptr = to_virt<u32>(phys);
		memset(ptr, 0, PAGE_SIZE);
		*ptr = guard->vmcs_rev;
		ptr[1] = 0;

		auto status = vmptrld(phys);
		assert(status);

		reload_control_regs();

		write(fields::GUEST_DR7, 0);
		write(fields::GUEST_RSP, 0);
		write(fields::GUEST_RIP, 0);
		write(fields::GUEST_RFLAGS, 2);

		state->cs.limit = 0xFFFF;
		state->ss.limit = 0xFFFF;
		state->ds.limit = 0xFFFF;
		state->es.limit = 0xFFFF;
		state->fs.limit = 0xFFFF;
		state->gs.limit = 0xFFFF;
		state->ldtr.limit = 0xFFFF;
		state->tr.limit = 0xFFFF;

		reload_seg_regs();

		write(fields::GUEST_CS_ACCESS_RIGHTS, 3 | 1 << 4 | 1 << 7);
		write(fields::GUEST_SS_ACCESS_RIGHTS, 3 | 1 << 4 | 1 << 7);
		write(fields::GUEST_DS_ACCESS_RIGHTS, 3 | 1 << 4 | 1 << 7);
		write(fields::GUEST_ES_ACCESS_RIGHTS, 3 | 1 << 4 | 1 << 7);
		write(fields::GUEST_FS_ACCESS_RIGHTS, 3 | 1 << 4 | 1 << 7);
		write(fields::GUEST_GS_ACCESS_RIGHTS, 3 | 1 << 4 | 1 << 7);
		write(fields::GUEST_LDTR_ACCESS_RIGHTS, 2 | 1 << 7);
		write(fields::GUEST_TR_ACCESS_RIGHTS, 3 | 1 << 7);

		write(fields::VMCS_LINK_PTR, 0xFFFFFFFFFFFFFFFF);

		// rvi == low 8 bits
		// rvi = max(rvi, new_irq_vec)
		write(fields::GUEST_INT_STATUS, 0);

		u64 cr0;
		u64 cr3;
		u64 cr4;
		asm volatile("mov %%cr0, %0" : "=r"(cr0));
		asm volatile("mov %%cr3, %0" : "=r"(cr3));
		asm volatile("mov %%cr4, %0" : "=r"(cr4));

		write(fields::HOST_CR0, cr0);
		write(fields::HOST_CR3, cr3);
		write(fields::HOST_CR4, cr4);

		write(fields::HOST_RIP, reinterpret_cast<usize>(on_vm_exit));

		write(fields::HOST_CS, 0x8);
		write(fields::HOST_SS, 0x10);
		write(fields::HOST_DS, 0x10);
		write(fields::HOST_ES, 0x10);
		write(fields::HOST_FS, 0x10);
		write(fields::HOST_GS, 0x10);
		write(fields::HOST_TR, 0x30);

		write(fields::HOST_FS_BASE, msrs::IA32_FSBASE.read());
		write(fields::HOST_GS_BASE, msrs::IA32_GSBASE.read());
		write(fields::HOST_TR_BASE, reinterpret_cast<u64>(&get_current_thread()->cpu->tss));

		struct [[gnu::packed]] Gdtr {
			u16 limit;
			u64 base;
		};
		Gdtr gdtr {};
		asm volatile("sgdt %0" : "=m"(gdtr));

		write(fields::HOST_GDTR_BASE, gdtr.base);

		asm volatile("sidt %0" : "=m"(gdtr));
		write(fields::HOST_IDTR_BASE, gdtr.base);

		write(fields::HOST_IA32_PAT, msrs::IA32_PAT.read());
		write(fields::HOST_IA32_EFER, msrs::IA32_EFER.read());

		auto restrictions = msrs::IA32_VMX_PINBASED_CTLS.read();
		u32 set = restrictions;
		u32 mask = restrictions >> 32;

		u64 pin_based_vm_controls = set & mask;

		assert(mask & 1 << 0);

		pin_based_vm_controls |= 1 << 0;
		write(fields::PIN_BASED_VM_EXEC_CONTROLS, pin_based_vm_controls);

		restrictions = msrs::IA32_VMX_PROCBASED_CTLS.read();
		set = restrictions;
		mask = restrictions >> 32;

		u64 primary_proc_based_vm_controls = set & mask;

		// exit on hlt
		assert(mask & 1 << 7);
		// exit on mwait
		assert(mask & 1 << 10);
		// don't exit on rdtsc
		assert(!(set & 1 << 12));
		// tpr shadow
		assert(mask & 1 << 21);
		// exit on io
		assert(mask & 1 << 24);
		// activate secondary controls
		assert(mask & 1 << 31);

		if (guard->default1_clear) {
			u64 true_restrictions = msrs::IA32_VMX_TRUE_PROCBASED_CTLS.read();

			// disable exit on cr3 load/store if supported
			if (!(true_restrictions & 1 << 15) && !(true_restrictions & 1 << 16)) {
				primary_proc_based_vm_controls &= ~(1 << 15);
				primary_proc_based_vm_controls &= ~(1 << 16);
			}
		}

		primary_proc_based_vm_controls |= 1 << 7;
		primary_proc_based_vm_controls |= 1 << 10;
		primary_proc_based_vm_controls |= 1 << 21;
		primary_proc_based_vm_controls |= 1 << 24;
		primary_proc_based_vm_controls |= 1 << 31;
		write(fields::PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROLS, primary_proc_based_vm_controls);

		restrictions = msrs::IA32_VMX_PROCBASED_CTLS2.read();
		set = restrictions;
		mask = restrictions >> 32;

		u64 secondary_proc_based_vm_controls = set & mask;

		// virtualize apic accesses
		assert(mask & 1 << 0);
		// enable ept
		assert(mask & 1 << 1);
		// don't exit on lgdt
		assert(!(set & 1 << 2));
		// unrestricted guest
		assert(mask & 1 << 7);
		// apic register virtualization
		assert(mask & 1 << 8);
		// virtual interrupt delivery
		assert(mask & 1 << 9);

		secondary_proc_based_vm_controls |= 1 << 0;
		secondary_proc_based_vm_controls |= 1 << 1;
		secondary_proc_based_vm_controls &= ~(1 << 2);
		secondary_proc_based_vm_controls |= 1 << 7;
		secondary_proc_based_vm_controls |= 1 << 8;
		secondary_proc_based_vm_controls |= 1 << 9;
		if (mask & 1 << 3) {
			secondary_proc_based_vm_controls |= 1 << 3;
		}

		write(fields::SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROLS, secondary_proc_based_vm_controls);

		write(fields::EXCEPTION_BITMAP, 0);

		write(fields::APIC_ACCESS_ADDR, 0xFEE00000);

		virt_lapic_phys = pmalloc(1);
		assert(virt_lapic_phys);
		virt_lapic = to_virt<u32>(virt_lapic_phys);
		memset(virt_lapic, 0, PAGE_SIZE);

		write(fields::VIRT_APIC_ADDR, virt_lapic_phys);

		write(fields::EPT_PTR, to_phys(vm->ept.level0) | 6 | 3 << 3);

		restrictions = msrs::IA32_VMX_EXIT_CTLS.read();
		set = restrictions;
		mask = restrictions >> 32;

		u64 primary_exit_controls = set & mask;

		// host address space size
		assert(mask & 1 << 9);
		// don't acknowledge interrupt on exit
		assert(!(set & 1 << 15));
		// save ia32_pat
		assert(mask & 1 << 18);
		// load ia32_pat
		assert(mask & 1 << 19);
		// save ia32_efer
		assert(mask & 1 << 20);
		// load ia32_efer
		assert(mask & 1 << 21);

		if (mask & 1 << 2) {
			primary_exit_controls |= 1 << 2;
		}

		primary_exit_controls |= 1 << 9;
		primary_exit_controls |= 1 << 18;
		primary_exit_controls |= 1 << 19;
		primary_exit_controls |= 1 << 20;
		primary_exit_controls |= 1 << 21;

		write(fields::PRIMARY_VM_EXIT_CONTROLS, primary_exit_controls);

		restrictions = msrs::IA32_VMX_ENTRY_CTLS.read();
		set = restrictions;
		mask = restrictions >> 32;

		u64 vm_entry_controls = set & mask;

		// allow non-long mode guest
		assert(!(set & 1 << 9));
		// load ia32_pat
		assert(mask & 1 << 14);
		// load ia32_efer
		assert(mask & 1 << 15);

		vm_entry_controls |= 1 << 14;
		vm_entry_controls |= 1 << 15;

		write(fields::VM_ENTRY_CONTROLS, vm_entry_controls);
	}

	void write(fields::Field field, u64 value) {
		// value can be rm but because of clang generating bad code
		// use r instead.
		// https://github.com/llvm/llvm-project/issues/20571
		asm volatile("vmwrite %0, %1" : : "r"(value), "r"(field.value));
	}

	[[nodiscard]] u64 read(fields::Field field) const {
		// value can be rm but because of clang generating bad code
		// use r instead.
		// https://github.com/llvm/llvm-project/issues/20571
		u64 value;
		asm volatile("vmread %1, %0" : "=r"(value) : "r"(field.value));
		return value;
	}

	void reload_control_regs() {
		u64 fixed0[] {
			msrs::IA32_VMX_CR0_FIXED0.read(),
			msrs::IA32_VMX_CR4_FIXED0.read()
		};
		u64 fixed1[] {
			msrs::IA32_VMX_CR0_FIXED1.read(),
			msrs::IA32_VMX_CR4_FIXED1.read()
		};

		u64 cr0 = state->cr0;
		cr0 |= fixed0[0] & ~(1 << 0 | 1 << 31);
		cr0 &= fixed1[0];

		u64 cr4 = state->cr4;
		cr4 |= fixed0[1];
		cr4 &= fixed1[1];

		state->cr0 = cr0;
		state->cr4 = cr4;

		write(fields::GUEST_CR0, cr0);
		write(fields::GUEST_CR3, state->cr3);
		write(fields::GUEST_CR4, state->cr4);
	}

	void reload_seg_regs() {
		write(fields::GUEST_CS, state->cs.selector);
		write(fields::GUEST_SS, state->ss.selector);
		write(fields::GUEST_DS, state->ds.selector);
		write(fields::GUEST_ES, state->es.selector);
		write(fields::GUEST_FS, state->fs.selector);
		write(fields::GUEST_GS, state->gs.selector);
		write(fields::GUEST_LDTR, state->ldtr.selector);
		write(fields::GUEST_TR, state->tr.selector);

		write(fields::GUEST_CS_BASE, state->cs.base);
		write(fields::GUEST_SS_BASE, state->ss.base);
		write(fields::GUEST_DS_BASE, state->ds.base);
		write(fields::GUEST_ES_BASE, state->es.base);
		write(fields::GUEST_FS_BASE, state->fs.base);
		write(fields::GUEST_GS_BASE, state->gs.base);
		write(fields::GUEST_LDTR_BASE, state->ldtr.base);
		write(fields::GUEST_TR_BASE, state->tr.base);
		write(fields::GUEST_GDTR_BASE, state->gdtr.base);
		write(fields::GUEST_IDTR_BASE, state->idtr.base);

		write(fields::GUEST_CS_LIMIT, state->cs.limit);
		write(fields::GUEST_SS_LIMIT, state->ss.limit);
		write(fields::GUEST_DS_LIMIT, state->ds.limit);
		write(fields::GUEST_ES_LIMIT, state->es.limit);
		write(fields::GUEST_FS_LIMIT, state->fs.limit);
		write(fields::GUEST_GS_LIMIT, state->gs.limit);
		write(fields::GUEST_LDTR_LIMIT, state->ldtr.limit);
		write(fields::GUEST_TR_LIMIT, state->tr.limit);
		write(fields::GUEST_GDTR_LIMIT, state->gdtr.limit);
		write(fields::GUEST_IDTR_LIMIT, state->idtr.limit);
	}

	static constexpr u64 EvmGuestState::* REG32_TABLE[] {
		&EvmGuestState::rax,
		&EvmGuestState::rcx,
		&EvmGuestState::rdx,
		&EvmGuestState::rbx,
		&EvmGuestState::rsp,
		&EvmGuestState::rbp,
		&EvmGuestState::rsi,
		&EvmGuestState::rdi
	};

	void store_to_reg8(u8 reg) {
		switch (reg) {
			case REG_RAX:
				state->rax &= ~0xFF;
				state->rax |= state->exit_state.mmio_read.ret_value & 0xFF;
				break;
			case REG_RCX:
				state->rcx &= ~0xFF;
				state->rcx |= state->exit_state.mmio_read.ret_value & 0xFF;
				break;
			case REG_RDX:
				state->rdx &= ~0xFF;
				state->rdx |= state->exit_state.mmio_read.ret_value & 0xFF;
				break;
			case REG_RBX:
				state->rbx &= ~0xFF;
				state->rbx |= state->exit_state.mmio_read.ret_value & 0xFF;
				break;
			case REG_RSP:
				state->rax &= ~0xFF00;
				state->rax |= (state->exit_state.mmio_read.ret_value & 0xFF) << 8;
				break;
			case REG_RBP:
				state->rcx &= ~0xFF00;
				state->rcx |= (state->exit_state.mmio_read.ret_value & 0xFF) << 8;
				break;
			case REG_RSI:
				state->rdx &= ~0xFF00;
				state->rdx |= (state->exit_state.mmio_read.ret_value & 0xFF) << 8;
				break;
			case REG_RDI:
				state->rbx &= ~0xFF00;
				state->rbx |= (state->exit_state.mmio_read.ret_value & 0xFF) << 8;
				break;
			default:
				break;
		}
	}

	[[nodiscard]] u8 read_from_reg8(u8 reg) {
		u8 value = 0;
		switch (reg) {
			case REG_RAX:
				value = state->rax;
				break;
			case REG_RCX:
				value = state->rcx;
				break;
			case REG_RDX:
				value = state->rdx;
				break;
			case REG_RBX:
				value = state->rbx;
				break;
			case REG_RSP:
				value = state->rax >> 8;
				break;
			case REG_RBP:
				value = state->rcx >> 8;
				break;
			case REG_RSI:
				value = state->rdx >> 8;
				break;
			case REG_RDI:
				value = state->rbx >> 8;
				break;
			default:
				break;
		}

		return value;
	}

	void decode_inst(bool is_write, bool first) {
		auto ip = read(fields::GUEST_RIP);
		auto cr0 = read(fields::GUEST_CR0);

		enum class Mode {
			Real,
			Protected,
			Long
		};

		Mode mode {Mode::Protected};
		auto orig_ip = ip;
		if (!(cr0 & 1 << 0)) {
			mode = Mode::Real;
			ip |= read(fields::GUEST_CS) << 4;
		}

		auto fetch_at_ip = [&]() {
			auto host = vm->ept.get_phys(ip++);
			assert(host);
			return *to_virt<u8>(host);
		};

		auto fetch_at_addr = [&](u64 guest, u8& ret) {
			auto host = vm->ept.get_phys(guest);
			if (!host) {
				return false;
			}
			ret = *to_virt<u8>(host);
			return true;
		};

		auto store_to_addr = [&](u64 guest, u8 byte) {
			auto host = vm->ept.get_phys(guest);
			if (!host) {
				return false;
			}
			*to_virt<u8>(host) = byte;
			return true;
		};

		auto size = read(fields::VM_EXIT_INST_LEN);

		u8 bytes[15];
		for (usize i = 0; i < size; ++i) {
			bytes[i] = fetch_at_ip();
		}

		usize inst_start = 0;
		bool operand_size_override = false;
		bool address_size_override = false;
		bool rep = false;
		bool es_seg = false;

		while (true) {
			switch (bytes[inst_start]) {
				case 0xF0:
					assert(!"lock prefix in emulation");
				case 0xF2:
					assert(!"repne prefix in emulation");
				case 0xF3:
					rep = true;
					++inst_start;
					continue;
				case 0x2E:
					assert(!"cs segment override in emulation");
				case 0x36:
					assert(!"ss segment override in emulation");
				case 0x3E:
					assert(!"ds segment override in emulation");
				case 0x26:
					es_seg = true;
					++inst_start;
					continue;
				case 0x64:
					assert(!"fs segment override in emulation");
				case 0x65:
					assert(!"gs segment override in emulation");
				case 0x66:
					operand_size_override = true;
					++inst_start;
					continue;
				case 0x67:
					address_size_override = true;
					++inst_start;
					continue;
				default:
					break;
			}

			break;
		}

		auto addr = read(fields::GUEST_PHYS_ADDR);

		assert(mode == Mode::Real || mode == Mode::Protected);

		auto mmio_write_override = [&](u32 value) {
			if ((mode == Mode::Protected && operand_size_override) ||
				(mode == Mode::Real && !operand_size_override)) {
				state->exit_state.mmio_write = {
					.guest_phys_addr = addr,
					.value = value & 0xFFFF,
					.size = 2
				};
			}
			else {
				state->exit_state.mmio_write = {
					.guest_phys_addr = addr,
					.value = value,
					.size = 4
				};
			}
		};

		auto mmio_read_override = [&](u8 reg) {
			if ((mode == Mode::Protected && operand_size_override) ||
				(mode == Mode::Real && !operand_size_override)) {
				auto& r = state->*REG32_TABLE[reg];
				r &= ~0xFFFF;
				r |= state->exit_state.mmio_read.ret_value & 0xFFFF;
			}
			else {
				state->*REG32_TABLE[reg] = state->exit_state.mmio_read.ret_value;
			}

			if (reg == REG_RSP) {
				write(fields::GUEST_RSP, state->rsp);
			}
		};

		auto mmio_get_size_override = [&]() -> u8 {
			if ((mode == Mode::Protected && operand_size_override) ||
				(mode == Mode::Real && !operand_size_override)) {
				return 2;
			}
			else {
				return 4;
			}
		};

		auto mmio_get_addr_override = [&]() -> u8 {
			if ((mode == Mode::Protected && address_size_override) ||
				(mode == Mode::Real && !address_size_override)) {
				return 2;
			}
			else if (mode == Mode::Long && !address_size_override) {
				return 8;
			}
			else {
				return 4;
			}
		};

		u8 op = bytes[inst_start++];

		// two byte opcode
		if (op == 0xF) {
			op = bytes[inst_start++];
			switch (op) {
				// movzx r32, [r/m16]
				case 0xB7:
				{
					if (first) {
						state->exit_state.mmio_read = {
							.guest_phys_addr = addr,
							.ret_value = 0,
							.size = 2
						};
						return;
					}

					u8 modrm = bytes[inst_start];
					u8 reg = modrm >> 3 & 0b111;

					state->*REG32_TABLE[reg] = state->exit_state.mmio_read.ret_value & 0xFFFF;

					if (reg == REG_RSP) {
						write(fields::GUEST_RSP, state->rsp);
					}
					break;
				}
				default:
					panic("[kernel][x86]: unimplemented two-byte op ", Fmt::Hex, op, " in vmx emulation");
			}
		}
		// mov [r/m8], r8
		else if (op == 0x88) {
			u8 modrm = bytes[inst_start];
			u8 reg = modrm >> 3 & 0b111;

			u8 value = read_from_reg8(reg);

			state->exit_state.mmio_write = {
				.guest_phys_addr = addr,
				.value = value,
				.size = 1
			};
		}
		// mov [r/m16 | r/m32], r16 | r32
		else if (op == 0x89) {
			u8 modrm = bytes[inst_start];
			u8 reg = modrm >> 3 & 0b111;

			auto value = state->*REG32_TABLE[reg];

			mmio_write_override(value);
		}
		// mov r8, [r/m8]
		else if (op == 0x8A) {
			if (first) {
				state->exit_state.mmio_read = {
					.guest_phys_addr = addr,
					.ret_value = 0,
					.size = 1
				};
				return;
			}

			u8 modrm = bytes[inst_start];
			u8 reg = modrm >> 3 & 0b111;

			store_to_reg8(reg);
		}
		// mov r16 | r32, [r/m16 | r/m32]
		else if (op == 0x8B) {
			if (first) {
				state->exit_state.mmio_read = {
					.guest_phys_addr = addr,
					.ret_value = 0,
					.size = mmio_get_size_override()
				};
				return;
			}

			u8 modrm = bytes[inst_start];
			u8 reg = modrm >> 3 & 0b111;

			mmio_read_override(reg);
		}
		// mov ax | eax, [moffs16 | moffs32]
		else if (op == 0xA1) {
			if (first) {
				state->exit_state.mmio_read = {
					.guest_phys_addr = addr,
					.ret_value = 0,
					.size = mmio_get_size_override()
				};
				return;
			}

			auto reg_size = mmio_get_size_override();

			if (reg_size == 2) {
				state->rax &= ~0xFFFF;
				state->rax |= state->exit_state.mmio_read.ret_value & 0xFFFF;
			}
			else {
				state->rax = state->exit_state.mmio_read.ret_value;
			}
		}
		// movs m8, m8
		else if (op == 0xA4) {
			assert(rep);

			auto addr_size = mmio_get_addr_override();

			u8 byte;

			if (first) {
				if (mode == Mode::Real) {
					addr = read(fields::GUEST_DS) << 4 | (state->rsi & (UINT32_MAX >> (32 - addr_size * 8)));
				}
				else {
					addr = state->rsi & (UINT32_MAX >> (32 - addr_size * 8));
				}

				if (!fetch_at_addr(addr, byte)) {
					prev_qual = 0;
					state->exit_reason = EVM_EXIT_REASON_MMIO_READ;
					state->exit_state.mmio_read = {
						.guest_phys_addr = addr,
						.ret_value = 0,
						.size = 1
					};
					return;
				}
			}
			else {
				assert(prev_qual == 0);
				byte = state->exit_state.mmio_read.ret_value;
			}

			if (mode == Mode::Real) {
				addr = read(fields::GUEST_ES) << 4 | (state->rdi & (UINT32_MAX >> (32 - addr_size * 8)));
			}
			else {
				addr = state->rdi & (UINT32_MAX >> (32 - addr_size * 8));
			}

			if (!store_to_addr(addr, byte)) {
				if (!first) {
					skip_exec = true;
				}

				prev_qual = 1 << 1;
				state->exit_reason = EVM_EXIT_REASON_MMIO_WRITE;
				state->exit_state.mmio_write = {
					.guest_phys_addr = addr,
					.value = byte,
					.size = 1
				};
			}

			auto flags = read(fields::GUEST_RFLAGS);
			if (!(flags & 1 << 10)) {
				state->rsi += 1;
				state->rdi += 1;
			}
			else {
				state->rsi -= 1;
				state->rdi -= 1;
			}

			--state->rcx;

			if (state->rcx) {
				return;
			}
		}
		// stos m16/m32
		else if (op == 0xAB) {
			assert(rep);

			auto reg_size = mmio_get_size_override();
			mmio_write_override(state->rax);

			auto flags = read(fields::GUEST_RFLAGS);
			if (!(flags & 1 << 10)) {
				state->rdi += reg_size;
			}
			else {
				state->rdi -= reg_size;
			}

			--state->rcx;

			if (state->rcx) {
				return;
			}
		}
		else {
			panic("[kernel][x86]: unimplemented op ", Fmt::Hex, op, " in vmx emulation");
		}

		write(fields::GUEST_RIP, orig_ip + size);
	}

	int run() override {
		skip_exec = false;

		switch (prev_reason) {
			case exit_reason::HLT:
			{
				auto inst_len = read(fields::VM_EXIT_INST_LEN);
				write(fields::GUEST_RIP, read(fields::GUEST_RIP) + inst_len);
				break;
			}
			case exit_reason::IO_INST:
			{
				if (prev_qual & 1 << 3) {
					u8 size = (prev_qual & 0b111) + 1;

					switch (size) {
						case 1:
							state->rax &= ~0xFF;
							state->rax |= state->exit_state.io_in.ret_value & 0xFF;
							break;
						case 2:
							state->rax &= ~0xFFFF;
							state->rax |= state->exit_state.io_in.ret_value & 0xFFFF;
							break;
						case 4:
							state->rax = state->exit_state.io_in.ret_value;
							break;
						default:
							break;
					}
				}

				auto inst_len = read(fields::VM_EXIT_INST_LEN);
				write(fields::GUEST_RIP, read(fields::GUEST_RIP) + inst_len);

				break;
			}
			case exit_reason::EPT_VIOLATION:
			{
				if (!(prev_qual & 1 << 1)) {
					decode_inst(false, false);
				}

				break;
			}
			default:
				break;
		}

		if (skip_exec) {
			return 0;
		}

		if (auto irq = pending_irq.exchange(0, kstd::memory_order::relaxed)) {
			u8 vec = irq;
			auto type = static_cast<EvmIrqType>(irq >> 8 & 0xFF);
			auto error = irq >> 16;

			auto cr0 = read(fields::GUEST_CR0);

			u32 int_info = vec | 1 << 31;
			if (type == EVM_IRQ_TYPE_EXCEPTION && (cr0 & 1 << 0)) {
				int_info |= 3 << 8;
				if (vec == 0x8 || vec == 0x13) {
					// deliver error code
					int_info |= 1 << 11;
					write(fields::VM_ENTRY_EXCEPTION_ERR_CODE, error);
				}
			}

			write(fields::VM_ENTRY_INT_INFO, int_info);
		}

		while (true) {
			asm volatile("cli");

			u64 xcr0 = 0;
			if (CPU_FEATURES.xsave) {
				xsave(host_simd_state, ~0);
				xrstor(guest_simd_state, ~0);
				xcr0 = rdxcr(0);
			}
			else {
				asm volatile("fxsaveq %0" : : "m"(*host_simd_state));
				asm volatile("fxrstorq %0" : : "m"(*guest_simd_state));
			}

			vmptrld(phys);

			auto status = enter_vmcs(this, launched);
			launched = true;

			if (CPU_FEATURES.xsave) {
				wrxcr(0, xcr0);
				xsave(guest_simd_state, ~0);
				xrstor(host_simd_state, ~0);
			}
			else {
				asm volatile("fxsaveq %0" : : "m"(*guest_simd_state));
				asm volatile("fxrstorq %0" : : "m"(*host_simd_state));
			}

			asm volatile("sti");

			switch (status) {
				case VmError::Success:
				{
					auto reason = read(fields::EXIT_REASON);

					switch (reason) {
						case exit_reason::EXT_INT:
							continue;
						case exit_reason::TRIPLE_FAULT:
							state->exit_reason = EVM_EXIT_REASON_TRIPLE_FAULT;
							break;
						case exit_reason::CPUID:
							state->exit_reason = EVM_EXIT_REASON_CPUID;
							write(fields::GUEST_RIP, read(fields::GUEST_RIP) + read(fields::VM_EXIT_INST_LEN));
							break;
						case exit_reason::HLT:
							state->exit_reason = EVM_EXIT_REASON_HALT;
							break;
						case exit_reason::IO_INST:
						{
							auto qual = read(fields::EXIT_QUALIFICATION);
							// string instruction
							assert(!(qual & 1 << 4));
							// rep prefixed
							assert(!(qual & 1 << 5));

							prev_qual = qual;

							if (qual & 1 << 3) {
								state->exit_reason = EVM_EXIT_REASON_IO_IN;
								state->exit_state.io_in = {
									.port = static_cast<u16>(qual >> 16 & 0xFFFF),
									.size = static_cast<u8>((qual & 0b111) + 1),
									.ret_value = 0
								};
							}
							else {
								u8 size = (qual & 0b111) + 1;
								u32 value = state->rax & ((u64 {1} << (size * 8)) - 1);

								state->exit_reason = EVM_EXIT_REASON_IO_OUT;
								state->exit_state.io_out = {
									.port = static_cast<u16>(qual >> 16 & 0xFFFF),
									.size = size,
									.value = value
								};
							}

							break;
						}
						case exit_reason::EPT_VIOLATION:
						{
							auto qual = read(fields::EXIT_QUALIFICATION);

							prev_qual = qual;

							if (qual & 1 << 1) {
								state->exit_reason = EVM_EXIT_REASON_MMIO_WRITE;
								decode_inst(true, true);
							}
							else {
								state->exit_reason = EVM_EXIT_REASON_MMIO_READ;
								decode_inst(false, true);
							}

							break;
						}
						case 31:
						{
							println("rdmsr is a stub");
							state->rax = 0;
							state->rdx = 0;
							write(fields::GUEST_RIP, read(fields::GUEST_RIP) + read(fields::VM_EXIT_INST_LEN));
							break;
						}
						default:
						{
							println("[kernel][x86]: vmx exit reason: ", reason);
							auto qual = read(fields::EXIT_QUALIFICATION);
							println("[kernel][x86]: vmx exit qualification: ", Fmt::Bin, qual, Fmt::Reset);
							auto addr = read(fields::GUEST_PHYS_ADDR);
							println("[kernel][x86]: vmx guest phys addr: ", Fmt::Hex, addr, Fmt::Reset);
							break;
						}
					}

					prev_reason = reason;

					break;
				}
				case VmError::VmLaunchFail:
				{
					println("[kernel][x86]: vmx vmlaunch failed");
					state->exit_reason = EVM_EXIT_REASON_VM_ENTER_FAILED;
					break;
				}
				case VmError::VmLaunchFailWithCode:
				{
					auto code = read(fields::VM_INST_ERROR);
					println("[kernel][x86]: vmx vmlaunch failed with code ", code);
					state->exit_reason = EVM_EXIT_REASON_VM_ENTER_FAILED;
					break;
				}
			}

			break;
		}

		return 0;
	}

	int write_state(EvmStateBits changed_state) override {
		if (changed_state & EVM_STATE_BITS_RIP) {
			write(fields::GUEST_RIP, state->rip);
		}
		if (changed_state & EVM_STATE_BITS_RSP) {
			write(fields::GUEST_RSP, state->rsp);
		}
		if (changed_state & EVM_STATE_BITS_RFLAGS) {
			write(fields::GUEST_RFLAGS, state->rflags);
		}
		if (changed_state & EVM_STATE_BITS_SEG_REGS) {
			reload_seg_regs();
		}
		if (changed_state & EVM_STATE_BITS_CONTROL_REGS) {
			reload_control_regs();
		}

		return 0;
	}

	int read_state(EvmStateBits wanted_state) override {
		if (wanted_state & EVM_STATE_BITS_RIP) {
			state->rip = read(fields::GUEST_RIP);
		}
		if (wanted_state & EVM_STATE_BITS_RSP) {
			state->rsp = read(fields::GUEST_RSP);
		}
		if (wanted_state & EVM_STATE_BITS_RFLAGS) {
			state->rflags = read(fields::GUEST_RFLAGS);
		}
		if (wanted_state & EVM_STATE_BITS_SEG_REGS) {
			state->cs.selector = read(fields::GUEST_CS);
			state->ss.selector = read(fields::GUEST_SS);
			state->ds.selector = read(fields::GUEST_DS);
			state->es.selector = read(fields::GUEST_ES);
			state->fs.selector = read(fields::GUEST_FS);
			state->gs.selector = read(fields::GUEST_GS);
			state->ldtr.selector = read(fields::GUEST_LDTR);
			state->tr.selector = read(fields::GUEST_TR);

			state->cs.base = read(fields::GUEST_CS_BASE);
			state->ss.base = read(fields::GUEST_SS_BASE);
			state->ds.base = read(fields::GUEST_DS_BASE);
			state->es.base = read(fields::GUEST_ES_BASE);
			state->fs.base = read(fields::GUEST_FS_BASE);
			state->gs.base = read(fields::GUEST_GS_BASE);
			state->ldtr.base = read(fields::GUEST_LDTR_BASE);
			state->tr.base = read(fields::GUEST_TR_BASE);
			state->gdtr.base = read(fields::GUEST_GDTR_BASE);
			state->idtr.base = read(fields::GUEST_IDTR_BASE);

			state->cs.limit = read(fields::GUEST_CS_LIMIT);
			state->ss.limit = read(fields::GUEST_SS_LIMIT);
			state->ds.limit = read(fields::GUEST_DS_LIMIT);
			state->es.limit = read(fields::GUEST_ES_LIMIT);
			state->fs.limit = read(fields::GUEST_FS_LIMIT);
			state->gs.limit = read(fields::GUEST_GS_LIMIT);
			state->ldtr.limit = read(fields::GUEST_LDTR_LIMIT);
			state->tr.limit = read(fields::GUEST_TR_LIMIT);
			state->gdtr.limit = read(fields::GUEST_GDTR_LIMIT);
			state->idtr.limit = read(fields::GUEST_IDTR_LIMIT);
		}
		if (wanted_state & EVM_STATE_BITS_CONTROL_REGS) {
			state->cr0 = read(fields::GUEST_CR0);
			state->cr3 = read(fields::GUEST_CR3);
			state->cr4 = read(fields::GUEST_CR4);
		}

		return 0;
	}

	kstd::atomic<u32> pending_irq {};
	IrqSpinlock<void> lapic_lock {};

	int trigger_irq(const EvmIrqInfo& info) override {
		if (info.irq > 255) {
			return ERR_INVALID_ARGUMENT;
		}

		if (info.irq < 32) {
			pending_irq.store(info.irq | info.type << 8 | info.error << 16, kstd::memory_order::relaxed);
		}
		else {
			auto guard = lapic_lock.lock();
			volatile u32* reg = &virt_lapic[(0x200 | ((info.irq & 0xE0) >> 1)) / 4];
			auto value = *reg;
			value |= 1 << (info.irq % 32);
			*reg = value;

			auto status = read(fields::GUEST_INT_STATUS);
			u8 rvi = status;
			rvi = kstd::max(rvi, static_cast<u8>(info.irq));
			status &= ~0xFF;
			status |= rvi;
			write(fields::GUEST_INT_STATUS, status);
		}

		return 0;
	}

	~Vmcs() override {
		pfree(phys, 1);
		pfree(state_phys, 1);
		pfree(virt_lapic_phys, 1);

		auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : 512;

		KERNEL_VSPACE.free_backed(host_simd_state, simd_size);
		KERNEL_VSPACE.free_backed(guest_simd_state, simd_size);
	}

	usize phys;
	u8* host_simd_state {};
	u8* guest_simd_state {};
	u32* virt_lapic {};
	usize virt_lapic_phys {};
	VmxVm* vm;
	u32 prev_reason {};
	u32 prev_qual {};
	bool launched {};
	bool skip_exec {};
};

static_assert(offsetof(Vmcs, state) == 16);

VmxVm::VmxVm() {
	auto ept_pml4_phys = pmalloc(1);
	assert(ept_pml4_phys);
	ept.level0 = to_virt<u64>(ept_pml4_phys);
	memset(ept.level0, 0, PAGE_SIZE);

	auto lapic_map_result = ept.map(0xFEE00000, 0xFEE00000);
	assert(lapic_map_result);

	auto guard = VMX_DATA.get();

	if (guard->initialized) {
		return;
	}

	u64 cr4;
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= 1 << 13;
	asm volatile("mov %0, %%cr4" : : "r"(cr4));

	u64 fixed0[] {
		msrs::IA32_VMX_CR0_FIXED0.read(),
		msrs::IA32_VMX_CR4_FIXED0.read()
	};
	u64 fixed1[] {
		msrs::IA32_VMX_CR0_FIXED1.read(),
		msrs::IA32_VMX_CR4_FIXED1.read()
	};

	u64 crs[2];
	asm volatile("mov %%cr0, %0; mov %%cr4, %1" : "=r"(crs[0]), "=r"(crs[1]));

	for (int reg = 0; reg < 2; ++reg) {
		auto reg_fixed0 = fixed0[reg];
		auto reg_fixed1 = fixed1[reg];
		auto value = crs[reg];

		value |= reg_fixed0;
		value &= reg_fixed1;
		assert((value & crs[reg]) == crs[reg]);

		if (reg == 0) {
			asm volatile("mov %0, %%cr0" : : "r"(value));
		}
		else {
			asm volatile("mov %0, %%cr4" : : "r"(value));
		}
	}

	auto vmxon_phys = pmalloc(1);
	assert(vmxon_phys);
	auto* vmxon = to_virt<u32>(vmxon_phys);
	*vmxon = guard->vmcs_rev;

	bool success;
	asm volatile("vmxon %1" : "=@ccnc"(success) : "m"(vmxon_phys) : "memory");
	assert(success);

	guard->initialized = true;

	Vmcs vmcs {this};
	vmptrld(vmcs.phys);

	asm volatile("cli");
	auto start = rdtsc();
	auto status = enter_vmcs(&vmcs, false);
	auto end = rdtsc();
	asm volatile("sti");
	assert(status == VmError::Success);
	entry_exit_tsc_ticks = end - start;
}

kstd::expected<kstd::shared_ptr<evm::VirtualCpu>, int> VmxVm::create_vcpu() {
	return {kstd::make_shared<Vmcs>(this)};
}

extern "C" [[gnu::used]] void vmcs_update_host_rsp(Vmcs* vmcs, u64 rsp) {
	vmcs->write(fields::HOST_RSP, rsp);
}

bool vmx_supported() {
	if (!CPU_FEATURES.vmx) {
		return false;
	}

	auto features = msrs::IA32_FEATURE_CONTROL.read();
	if (!(features & 1 << 0) || !(features & 1 << 2)) {
		println("[kernel][x86]: bios has disabled vmx");
		return false;
	}

	return true;
}

kstd::shared_ptr<evm::Evm> vmx_create_vm() {
	assert(vmx_supported());
	return kstd::make_shared<VmxVm>();
}
