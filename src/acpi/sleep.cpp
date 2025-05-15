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
#include "qacpi/ns.hpp"
#include "sched/sched.hpp"
#include "sched/thread.hpp"

extern "C" void sched_switch_thread(ArchThread* prev, ArchThread* next);
void sched_before_switch(Thread* prev, Thread* thread);

void reinstall_sci_handler();

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
	usize SMP_TRAMPLINE_PHYS_ENTRY16 = 0;
	usize SMP_TRAMPLINE_PHYS_ENTRY32 = 0;
	static Facs* FACS;

	static void enable_power_res(qacpi::NamespaceNode* node, bool enable) {
		uint64_t state;
		auto status = GLOBAL_CTX->evaluate_int(node, "_STA", state);
		assert(status == qacpi::Status::Success);

		if (state == enable) {
			return;
		}

		auto res = qacpi::ObjectRef::empty();
		if (enable) {
			status = GLOBAL_CTX->evaluate(node, "_ON", res);
		}
		else {
			status = GLOBAL_CTX->evaluate(node, "_OFF", res);
		}

		assert(status == qacpi::Status::Success);
	}

	static void change_power_state(qacpi::NamespaceNode* node, u32 state) {
		char ps_name[5] = "_PS0";
		ps_name[3] = static_cast<char>(ps_name[3] + state);
		auto res = qacpi::ObjectRef::empty();
		auto status = GLOBAL_CTX->evaluate(node, ps_name, res);
		if (state == 0 && status == qacpi::Status::NotFound) {
			return;
		}
		else if (status != qacpi::Status::Success) {
			auto node_name = node->name();
			kstd::string_view name {node_name.ptr, node_name.size};
			println("[kernel][acpi]: failed to execute ", ps_name, " for ", name, ": ", qacpi::status_to_str(status));
		}
		assert(status == qacpi::Status::Success);
	}

	static void acpi_sleep_helper(void*) {
		dev_suspend_all();

		auto status = EVENT_CTX->prepare_for_sleep_state(*GLOBAL_CTX, qacpi::events::SleepState::S3);
		assert(status == qacpi::Status::Success);

		GLOBAL_CTX->iterate_nodes(
			nullptr,
			[](qacpi::Context& ctx, qacpi::NamespaceNode* node) {
				if (!ctx.find_node(node, "_PRW", true)) {
					return qacpi::IterDecision::Continue;
				}

				auto res = qacpi::ObjectRef::empty();
				auto status = ctx.evaluate_package(node, "_PRW", res);
				if (status != qacpi::Status::Success) {
					return qacpi::IterDecision::Continue;
				}

				assert(res->get_unsafe<qacpi::Package>().size() >= 2);
				auto gpe_index_obj = ctx.get_pkg_element(res, 0);
				assert(gpe_index_obj->get<uint64_t>());
				auto gpe_index = gpe_index_obj->get_unsafe<uint64_t>();

				auto deepest_system_state_obj = ctx.get_pkg_element(res, 1);

				assert(deepest_system_state_obj->get<uint64_t>());
				auto deepest_system_state = deepest_system_state_obj->get_unsafe<uint64_t>();

				if (deepest_system_state < 3) {
					return qacpi::IterDecision::Continue;
				}

				uint64_t shallowest_state = 0;
				status = ctx.evaluate_int("_S3D", shallowest_state);
				if (status != qacpi::Status::Success &&
					status != qacpi::Status::NotFound) {
					return qacpi::IterDecision::Continue;
				}

				uint64_t deepest_state = shallowest_state;
				status = ctx.evaluate_int("_S3W", deepest_state);
				if (status != qacpi::Status::Success &&
					status != qacpi::Status::NotFound) {
					return qacpi::IterDecision::Continue;
				}

				qacpi::ObjectRef one {uint64_t {1}};
				qacpi::ObjectRef system_state {uint64_t {3}};
				qacpi::ObjectRef device_state {uint64_t {deepest_state}};
				if (!one || !system_state || !device_state) {
					return qacpi::IterDecision::Break;
				}

				qacpi::ObjectRef args[] {
					one,
					system_state,
					device_state
				};
				status = ctx.evaluate(node, "_DSW", res, args, 3);
				if (status == qacpi::Status::NotFound) {
					status = ctx.evaluate(node, "_PSW", res, &one, 1);
					if (status != qacpi::Status::Success &&
						status != qacpi::Status::NotFound) {
						return qacpi::IterDecision::Continue;
					}
				}
				else if (status != qacpi::Status::Success) {
					return qacpi::IterDecision::Continue;
				}

				change_power_state(node, deepest_state);

				status = EVENT_CTX->enable_gpe_for_wake(gpe_index);
				if (status == qacpi::Status::InvalidArgs) {
					println("[kernel][acpi]: failed to enable gpe ", gpe_index, " for wake");
				}
				else {
					assert(status == qacpi::Status::Success);
				}

				return qacpi::IterDecision::Continue;
			});

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

		FACS->fw_waking_vector = SMP_TRAMPLINE_PHYS_ENTRY16;
		if (FACS->length < offsetof(Facs, x_fw_waking_vector) + 8) {
			return;
		}

		if (FACS->version >= 1) {
			FACS->x_fw_waking_vector = SMP_TRAMPLINE_PHYS_ENTRY32;
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

	void wake_from_sleep() {
		reinstall_sci_handler();

		GLOBAL_CTX->iterate_nodes(
			nullptr,
			[](qacpi::Context& ctx, qacpi::NamespaceNode* node) {
				if (!ctx.find_node(node, "_PRW", true)) {
					return qacpi::IterDecision::Continue;
				}

				auto res = qacpi::ObjectRef::empty();
				auto status = ctx.evaluate_package(node, "_PRW", res);
				if (status != qacpi::Status::Success) {
					return qacpi::IterDecision::Continue;
				}

				assert(res->get_unsafe<qacpi::Package>().size() >= 2);
				auto gpe_index_obj = ctx.get_pkg_element(res, 0);
				assert(gpe_index_obj->get<uint64_t>());
				auto gpe_index = gpe_index_obj->get_unsafe<uint64_t>();

				auto deepest_system_state_obj = ctx.get_pkg_element(res, 1);
				assert(deepest_system_state_obj->get<uint64_t>());
				auto deepest_system_state = deepest_system_state_obj->get_unsafe<uint64_t>();

				if (deepest_system_state < 3) {
					return qacpi::IterDecision::Continue;
				}

				qacpi::ObjectRef zero {uint64_t {0}};
				qacpi::ObjectRef system_state {uint64_t {0}};
				qacpi::ObjectRef device_state {uint64_t {0}};
				if (!zero || !system_state || !device_state) {
					return qacpi::IterDecision::Break;
				}

				qacpi::ObjectRef args[] {
					zero,
					system_state,
					device_state
				};
				status = ctx.evaluate(node, "_DSW", res, args, 3);
				if (status == qacpi::Status::NotFound) {
					status = ctx.evaluate(node, "_PSW", res, &zero, 1);
					if (status != qacpi::Status::Success &&
						status != qacpi::Status::NotFound) {
						return qacpi::IterDecision::Continue;
					}
				}
				else if (status != qacpi::Status::Success) {
					return qacpi::IterDecision::Continue;
				}

				change_power_state(node, 0);

				status = EVENT_CTX->disable_gpe_for_wake(gpe_index);
				assert(status == qacpi::Status::Success ||
					   status == qacpi::Status::InvalidArgs);

				return qacpi::IterDecision::Continue;
			});
	}
}
