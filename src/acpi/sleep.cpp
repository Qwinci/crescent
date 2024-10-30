#include "sleep.hpp"
#include "acpi.hpp"
#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "arch/sleep.hpp"
#include "dev/dev.hpp"
#include "manually_destroy.hpp"
#include "manually_init.hpp"
#include "mem/mem.hpp"
#include "qacpi/event.hpp"
#include "sched/sched.hpp"
#include "sched/thread.hpp"

extern "C" void sched_switch_thread(ArchThread* prev, ArchThread* next);
void sched_before_switch(Thread* prev, Thread* thread);

namespace acpi {
	extern ManuallyDestroy<qacpi::events::Context> EVENT_CTX;

	void reboot() {
		auto status = EVENT_CTX->prepare_for_sleep_state(*GLOBAL_CTX, qacpi::events::SleepState::S5);
		assert(status == qacpi::Status::Success);

		IrqGuard irq_guard {};
		auto cpu = get_current_thread()->cpu;

		for (usize i = 0; i < arch_get_cpu_count(); ++i) {
			if (i == cpu->number) {
				continue;
			}

			auto* other = arch_get_cpu(i);
			arch_send_ipi(Ipi::Halt, other);
			other->ipi_ack.store(false, kstd::memory_order::seq_cst);
			while (!other->ipi_ack.load(kstd::memory_order::seq_cst));
		}

		EVENT_CTX->reboot();
#ifdef __x86_64__
		asm volatile("xor %%eax, %%eax; mov %%rax, %%cr3" : : : "memory");
#endif
	}

	struct Facs {
		char signature[4];
		u32 length;
		u32 hw_signature;
		u32 fw_waking_vector;
		u32 global_lock;
		u32 flags;
		u64 x_fw_waking_vector;
		u8 version;
		u8 reserved0[3];
		u32 ospm_flags;
		u8 reserved1[24];
	};

	usize SMP_TRAMPOLINE_PHYS_ADDR = 0;
	static Facs* FACS;

	static void acpi_sleep_helper(void*) {
		dev_suspend_all();

		auto status = EVENT_CTX->prepare_for_sleep_state(*GLOBAL_CTX, qacpi::events::SleepState::S3);
		assert(status == qacpi::Status::Success);
		IrqGuard shutdown_guard {};
		EVENT_CTX->enter_sleep_state(qacpi::events::SleepState::S3);
		panic("enter sleep state returned");
	}

	void sleep_init() {
		if (GLOBAL_FADT->hdr.length < offsetof(Fadt, x_firmware_ctrl) + 8) {
			assert(GLOBAL_FADT->fw_ctrl);
			FACS = to_virt<Facs>(GLOBAL_FADT->fw_ctrl);
		}
		else {
			if (GLOBAL_FADT->x_firmware_ctrl) {
				FACS = to_virt<Facs>(GLOBAL_FADT->x_firmware_ctrl);
			}
			else {
				assert(GLOBAL_FADT->fw_ctrl);
				FACS = to_virt<Facs>(GLOBAL_FADT->fw_ctrl);
			}
		}

		arch_sleep_init();
	}

	static void set_wake_addr() {
		IrqGuard irq_guard {};

		FACS->fw_waking_vector = SMP_TRAMPOLINE_PHYS_ADDR;
		if (FACS->length < offsetof(Facs, x_fw_waking_vector) + 8) {
			return;
		}

		if (FACS->version >= 1) {
			FACS->x_fw_waking_vector = SMP_TRAMPOLINE_PHYS_ADDR;
		}
		else {
			FACS->x_fw_waking_vector = 0;
		}
	}

	static Thread* SLEEP_HELPER_THREAD;

	void enter_sleep_state(SleepState state) {
		if (!GLOBAL_CTX.is_initialized()) {
			return;
		}

		switch (state) {
			case SleepState::S3:
			{
				set_wake_addr();

				arch_prepare_for_sleep();

				auto current = get_current_thread();
				current->pin_cpu = true;
				auto cpu = current->cpu;
				SLEEP_HELPER_THREAD = new Thread {"acpi sleep helper", cpu, &*KERNEL_PROCESS, acpi_sleep_helper, nullptr};
				cpu->cpu_tick_source->reset();
				current->status = Thread::Status::Waiting;
				cpu->scheduler.queue(current);
				set_current_thread(SLEEP_HELPER_THREAD);
				sched_before_switch(current, SLEEP_HELPER_THREAD);
				sched_switch_thread(current, SLEEP_HELPER_THREAD);
				current->pin_cpu = false;

				KERNEL_PROCESS->remove_thread(SLEEP_HELPER_THREAD);
				delete SLEEP_HELPER_THREAD;

				break;
			}
			case SleepState::S5:
			{
				auto status = EVENT_CTX->prepare_for_sleep_state(*GLOBAL_CTX, qacpi::events::SleepState::S5);
				assert(status == qacpi::Status::Success);

				IrqGuard shutdown_guard {};
				auto cpu = get_current_thread()->cpu;

				for (usize i = 0; i < arch_get_cpu_count(); ++i) {
					if (i == cpu->number) {
						continue;
					}

					auto* other = arch_get_cpu(i);
					arch_send_ipi(Ipi::Halt, other);
					other->ipi_ack.store(false, kstd::memory_order::seq_cst);
					while (!other->ipi_ack.load(kstd::memory_order::seq_cst));
				}

				EVENT_CTX->enter_sleep_state(qacpi::events::SleepState::S5);
			}
		}
	}
}
