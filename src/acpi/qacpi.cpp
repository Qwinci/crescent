#include "acpi.hpp"
#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "arch/x86/dev/io_apic.hpp"
#include "dev/clock.hpp"
#include "dev/pci.hpp"
#include "events.hpp"
#include "manually_destroy.hpp"
#include "manually_init.hpp"
#include "mem/mem.hpp"
#include "mem/register.hpp"
#include "qacpi/event.hpp"
#include "qacpi/ns.hpp"
#include "qacpi/os.hpp"
#include "sched/mutex.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "x86/io.hpp"
#include "x86/irq.hpp"

namespace {
	struct WorkQueue {
		void queue(qacpi::Status (*fn)(void* arg), void* arg) {
			auto guard = _queue.lock();
			auto entry = new Entry {
				.fn = fn,
				.arg = arg
			};
			guard->push(entry);
			event.signal_one();
		}

		void do_work() {
			Entry* entry;
			{
				auto guard = _queue.lock();
				assert(!guard->is_empty());
				entry = guard->pop_front();
			}
			entry->fn(entry->arg);
			delete entry;
		}

		void wait_for_work() {
			event.wait();
		}

	private:
		struct Entry {
			DoubleListHook hook {};
			qacpi::Status (*fn)(void* arg) {};
			void* arg {};
		};

		IrqSpinlock<DoubleList<Entry, &Entry::hook>> _queue;
		Event event;
	};

	ManuallyDestroy<WorkQueue> ACPI_WORK_QUEUE {};

	[[noreturn]] void acpi_worker(void*) {
		while (true) {
			ACPI_WORK_QUEUE->wait_for_work();
			ACPI_WORK_QUEUE->do_work();
		}
	}
}

namespace acpi {
	ManuallyInit<qacpi::Context> GLOBAL_CTX;
	extern ManuallyDestroy<qacpi::events::Context> EVENT_CTX;

	void qacpi_init() {
		{
			IrqGuard irq_guard {};
			auto cpu = get_current_thread()->cpu;
			auto aml_worker_thread = new Thread {"acpi worker", cpu, &*KERNEL_PROCESS, acpi_worker, nullptr};
			cpu->scheduler.queue(aml_worker_thread);
		}

		assert(EVENT_CTX->init(GLOBAL_FADT) == qacpi::Status::Success);
		assert(EVENT_CTX->enable_acpi_mode(true) == qacpi::Status::Success);

		auto* dsdt_ptr = to_virt<acpi::SdtHeader>(GLOBAL_FADT->dsdt);
		if (GLOBAL_FADT->hdr.length >= sizeof(Fadt) && GLOBAL_FADT->x_dsdt) {
			dsdt_ptr = to_virt<acpi::SdtHeader>(GLOBAL_FADT->x_dsdt);
		}

		auto* dsdt_aml_ptr = offset(dsdt_ptr, const u8*, sizeof(acpi::SdtHeader));
		u32 dsdt_aml_size = dsdt_ptr->length - sizeof(acpi::SdtHeader);

		GLOBAL_CTX.initialize(dsdt_ptr->revision, qacpi::LogLevel::Error);

		if (auto status = GLOBAL_CTX->init(); status != qacpi::Status::Success) {
			panic("[kernel][acpi]: qacpi failed to init: ", qacpi::status_to_str(status));
		}

		if (auto status = GLOBAL_CTX->load_table(dsdt_aml_ptr, dsdt_aml_size); status != qacpi::Status::Success) {
			panic("[kernel][acpi]: dsdt load failed: ", qacpi::status_to_str(status));
		}
		else {
			for (u32 i = 0;; ++i) {
				auto* ssdt = static_cast<acpi::SdtHeader*>(acpi::get_table("SSDT", i));
				if (!ssdt) {
					break;
				}
				auto* ssdt_aml_ptr = offset(ssdt, const u8*, sizeof(acpi::SdtHeader));
				u32 ssdt_aml_size = ssdt->length - sizeof(acpi::SdtHeader);
				status = GLOBAL_CTX->load_table(ssdt_aml_ptr, ssdt_aml_size);
				if (status != qacpi::Status::Success) {
					panic("[kernel][acpi]: ssdt load failed: ", qacpi::status_to_str(status));
				}
			}
		}

		auto ret = qacpi::ObjectRef::empty();
		qacpi::ObjectRef arg;
		assert(arg);
		// apic mode
		arg->data = uint64_t {1};
		auto status = GLOBAL_CTX->evaluate("_PIC", ret, &arg, 1);
		println("[kernel][acpi]: _PIC returned ", qacpi::status_to_str(status));

		acpi::early_events_init();

		if (status = GLOBAL_CTX->init_namespace(); status != qacpi::Status::Success) {
			panic("[kernel][acpi]: namespace init failed: ", qacpi::status_to_str(status));
		}

		println("[kernel][acpi]: init done");
	}
}

bool qacpi_os_mutex_create(void**) {
	return true;
}

void qacpi_os_mutex_destroy(void*) {}

qacpi::Status qacpi_os_mutex_lock(void*, uint16_t) {
	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_mutex_unlock(void*) {
	return qacpi::Status::Success;
}

bool qacpi_os_event_create(void** handle) {
	*handle = new Event {};
	return true;
}

void qacpi_os_event_destroy(void* handle) {
	delete static_cast<Event*>(handle);
}

qacpi::Status qacpi_os_event_wait(void* handle, uint16_t timeout_ms) {
	auto* event = static_cast<Event*>(handle);
	if (timeout_ms == 0xFFFF) {
		event->wait();
		return qacpi::Status::Success;
	}
	auto ret = event->wait_with_timeout(timeout_ms * US_IN_MS);
	return ret ? qacpi::Status::Success : qacpi::Status::TimeOut;
}

qacpi::Status qacpi_os_event_signal(void* handle) {
	auto* event = static_cast<Event*>(handle);
	event->signal_one();
	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_event_reset(void* handle) {
	auto* event = static_cast<Event*>(handle);
	event->reset();
	return qacpi::Status::Success;
}

void qacpi_os_trace(const char* str, size_t size) {
	if (kstd::string_view {str, size}.starts_with("aml debug")) {
		return;
	}

	println("[kernel][qacpi]: ", kstd::string_view {str, size});
}

void* qacpi_os_get_tid() {
	return get_current_thread();
}

void* qacpi_os_malloc(size_t size) {
	return ALLOCATOR.alloc(size);
}

void qacpi_os_free(void* ptr, size_t size) {
	ALLOCATOR.free(ptr, size);
}

void qacpi_os_stall(uint64_t us) {
	udelay(us);
}

void qacpi_os_sleep(uint64_t ms) {
	get_current_thread()->sleep_for(ms * US_IN_MS);
}

void qacpi_os_fatal(uint8_t type, uint16_t code, uint64_t arg) {
	println("[kernel][qacpi]: fatal type ", type, " code ", code, " arg ", arg);
}

// in 100ns increments
uint64_t qacpi_os_timer() {
	auto src = CLOCK_SOURCE.lock_read();
	return (*src)->get_ns() / 100;
}

void qacpi_os_breakpoint() {}

qacpi::Status qacpi_os_mmio_read(uint64_t phys, uint8_t size, uint64_t& res) {
	if (size == 1) {
		res = *to_virt<volatile u8>(phys);
	}
	else if (size == 2) {
		res = *to_virt<volatile u16>(phys);
	}
	else if (size == 4) {
		res = *to_virt<volatile u32>(phys);
	}
	else if (size == 8) {
		res = *to_virt<volatile u64>(phys);
	}
	else {
		return qacpi::Status::InvalidArgs;
	}

#if CONFIG_TRACING
	println("qacpi_os_mmio_read", size, " ", Fmt::Hex, phys, " = ", res, Fmt::Reset);
#endif

	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_mmio_write(uint64_t phys, uint8_t size, uint64_t value) {
#if CONFIG_TRACING
	println("qacpi_os_mmio_write", size, " ", Fmt::Hex, phys, " = ", value, Fmt::Reset);
#endif

	if (size == 1) {
		*to_virt<volatile u8>(phys) = value;
	}
	else if (size == 2) {
		*to_virt<volatile u16>(phys) = value;
	}
	else if (size == 4) {
		*to_virt<volatile u32>(phys) = value;
	}
	else if (size == 8) {
		*to_virt<volatile u64>(phys) = value;
	}
	else {
		return qacpi::Status::InvalidArgs;
	}
	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_io_read(uint32_t port, uint8_t size, uint64_t& res) {
	if (size == 1) {
		res = x86::in1(port);
	}
	else if (size == 2) {
		res = x86::in2(port);
	}
	else if (size == 4) {
		res = x86::in4(port);
	}
	else {
		return qacpi::Status::InvalidArgs;
	}

#if CONFIG_TRACING
	println("qacpi_os_io_read", size, " ", Fmt::Hex, port, " = ", res, Fmt::Reset);
#endif

	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_io_write(uint32_t port, uint8_t size, uint64_t value) {
#if CONFIG_TRACING
	println("qacpi_os_io_write", size, " ", Fmt::Hex, port, " = ", value, Fmt::Reset);
#endif

	if (size == 1) {
		x86::out1(port, value);
	}
	else if (size == 2) {
		x86::out2(port, value);
	}
	else if (size == 4) {
		x86::out4(port, value);
	}
	else {
		return qacpi::Status::InvalidArgs;
	}
	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_pci_read(qacpi::PciAddress address, uint64_t offset, uint8_t size, uint64_t& res) {
	pci::PciAddress addr {
		.seg = address.segment,
		.bus = address.bus,
		.dev = static_cast<u8>(address.device),
		.func = static_cast<u8>(address.function)
	};

	if (size == 1) {
		res = pci::read8(addr, offset);
	}
	else if (size == 2) {
		res = pci::read16(addr, offset);
	}
	else if (size == 4) {
		res = pci::read32(addr, offset);
	}
	else if (size == 8) {
		res = pci::read32(addr, offset);
		res |= static_cast<u64>(pci::read32(addr, offset + 4)) << 32;
	}
	else {
		return qacpi::Status::InvalidArgs;
	}
	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_pci_write(qacpi::PciAddress address, uint64_t offset, uint8_t size, uint64_t value) {
	pci::PciAddress addr {
		.seg = address.segment,
		.bus = address.bus,
		.dev = static_cast<u8>(address.device),
		.func = static_cast<u8>(address.function)
	};

	if (size == 1) {
		pci::write8(addr, offset, value);
	}
	else if (size == 2) {
		pci::write16(addr, offset, value);
	}
	else if (size == 4) {
		pci::write32(addr, offset, value);
	}
	else if (size == 8) {
		pci::write32(addr, offset, value);
		pci::write32(addr, offset + 4, value >> 32);
	}
	else {
		return qacpi::Status::InvalidArgs;
	}
	return qacpi::Status::Success;
}

void qacpi_os_notify(void* notify_arg, qacpi::NamespaceNode* node, uint64_t value) {
	acpi::EVENT_CTX->on_notify(node, value);
}

namespace {
	bool (*QACPI_SCI_HANDLER)(void* arg);
	void* QACPI_SCI_ARG;
	ManuallyDestroy<IrqHandler> SCI_HANDLER {{
		.fn = [](IrqFrame*) {
			return QACPI_SCI_HANDLER(QACPI_SCI_ARG);
		},
		.can_be_shared = true
	}};
	u8 SCI_VEC;
}

qacpi::Status qacpi_os_install_sci_handler(uint32_t irq, bool (*handler)(void* arg), void* arg, void** handle) {
	QACPI_SCI_HANDLER = handler;
	QACPI_SCI_ARG = arg;

	SCI_VEC = x86_alloc_irq(1, true);
	assert(SCI_VEC);
	register_irq_handler(SCI_VEC, &*SCI_HANDLER);
	IO_APIC.register_isa_irq(irq, SCI_VEC, true, true);

	return qacpi::Status::Success;
}

void qacpi_os_uninstall_sci_handler(uint32_t irq, void* handle) {
	IO_APIC.deregister_isa_irq(irq);
	deregister_irq_handler(SCI_VEC, &*SCI_HANDLER);
}

qacpi::Status qacpi_os_queue_work(qacpi::Status (*fn)(void* arg), void* arg) {
	ACPI_WORK_QUEUE->queue(fn, arg);
	return qacpi::Status::Success;
}
