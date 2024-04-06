#include "acpi.hpp"
#include "dev/clock.hpp"
#include "dev/pci.hpp"
#include "manually_init.hpp"
#include "mem/mem.hpp"
#include "mem/register.hpp"
#include "qacpi/ns.hpp"
#include "qacpi/os.hpp"
#include "sched/mutex.hpp"
#include "sched/sched.hpp"

namespace pm1_ctrl {
	static constexpr BitField<u64, bool> SCI_EN {0, 1};
}

namespace acpi {
	ManuallyInit<qacpi::Context> GLOBAL_CTX;
	Fadt* GLOBAL_FADT;

	void qacpi_init() {
		GLOBAL_FADT = static_cast<Fadt*>(acpi::get_table("FACP"));
		assert(GLOBAL_FADT);

		auto* dsdt_ptr = to_virt<acpi::SdtHeader>(GLOBAL_FADT->dsdt);
		auto* dsdt_aml_ptr = offset(dsdt_ptr, const u8*, sizeof(acpi::SdtHeader));
		u32 dsdt_aml_size = dsdt_ptr->length - sizeof(acpi::SdtHeader);

		GLOBAL_CTX.initialize(dsdt_ptr->revision);

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

		if (auto status = GLOBAL_CTX->init_namespace(); status != qacpi::Status::Success) {
			panic("[kernel][acpi]: namespace init failed: ", qacpi::status_to_str(status));
		}

		auto ret = qacpi::ObjectRef::empty();
		qacpi::ObjectRef arg;
		assert(arg);
		// apic mode
		arg->data = uint64_t {1};
		auto status = GLOBAL_CTX->evaluate("_PIC", ret, &arg, 1);
		println("[kernel][acpi]: _PIC returned ", qacpi::status_to_str(status));

		BitValue<u64> pm1_ctrl {read_from_addr(GLOBAL_FADT->x_pm1a_cnt_blk) | read_from_addr(GLOBAL_FADT->x_pm1b_cnt_blk)};
		if (!(pm1_ctrl & pm1_ctrl::SCI_EN)) {
			qacpi_os_io_write(GLOBAL_FADT->smi_cmd, 1, GLOBAL_FADT->acpi_enable);
			while (!(pm1_ctrl & pm1_ctrl::SCI_EN)) {
				pm1_ctrl = {read_from_addr(GLOBAL_FADT->x_pm1a_cnt_blk) | read_from_addr(GLOBAL_FADT->x_pm1b_cnt_blk)};
			}
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
	println("[kernel][qacpi]: ", kstd::string_view {str, size});
}

void* qacpi_os_get_tid() {
	return get_current_thread();
}

//static u8 STORAGE[1024 * 1024 * 500] {};
//static u8* PTR = STORAGE;

void* qacpi_os_malloc(size_t size) {
	return ALLOCATOR.alloc(size);
	/*size = ALIGNUP(size, 16);

	auto ptr = PTR;
	if (ptr + size > STORAGE + 1024 * 1024 * 500) {
		return nullptr;
	}
	PTR += size;
	return ptr;*/
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
	auto ticks_in_us = (*src)->ticks_in_us;
	return (*src)->get() / ticks_in_us / 10;
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

	println("qacpi_os_mmio_read", size, " ", Fmt::Hex, phys, " = ", res, Fmt::Reset);

	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_mmio_write(uint64_t phys, uint8_t size, uint64_t value) {
	println("qacpi_os_mmio_write", size, " ", Fmt::Hex, phys, " = ", value, Fmt::Reset);

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

static inline u8 in1(u16 port) {
	u8 value;
	asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline u16 in2(u16 port) {
	u16 value;
	asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline u32 in4(u16 port) {
	u32 value;
	asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline void out1(u16 port, u8 value) {
	asm volatile("outb %1, %0" : : "Nd"(port), "a"(value));
}

static inline void out2(u16 port, u16 value) {
	asm volatile("outw %1, %0" : : "Nd"(port), "a"(value));
}

static inline void out4(u16 port, u32 value) {
	asm volatile("outl %1, %0" : : "Nd"(port), "a"(value));
}

qacpi::Status qacpi_os_io_read(uint32_t port, uint8_t size, uint64_t& res) {
	if (size == 1) {
		res = in1(port);
	}
	else if (size == 2) {
		res = in2(port);
	}
	else if (size == 4) {
		res = in4(port);
	}
	else {
		return qacpi::Status::InvalidArgs;
	}

	println("qacpi_os_io_read", size, " ", Fmt::Hex, port, " = ", res, Fmt::Reset);

	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_io_write(uint32_t port, uint8_t size, uint64_t value) {
	println("qacpi_os_io_write", size, " ", Fmt::Hex, port, " = ", value, Fmt::Reset);

	if (size == 1) {
		out1(port, value);
	}
	else if (size == 2) {
		out2(port, value);
	}
	else if (size == 4) {
		out4(port, value);
	}
	else {
		return qacpi::Status::InvalidArgs;
	}
	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_pci_read(qacpi::PciAddress address, uint64_t offset, uint8_t size, uint64_t& res) {
	auto* space = pci::get_space(address.segment, address.bus, address.device, address.function);
	if (size == 1) {
		res = *offset(space, volatile u8*, offset);
	}
	else if (size == 2) {
		res = *offset(space, volatile u16*, offset);
	}
	else if (size == 4) {
		res = *offset(space, volatile u32*, offset);
	}
	else if (size == 8) {
		res = *offset(space, volatile u64*, offset);
	}
	else {
		return qacpi::Status::InvalidArgs;
	}
	return qacpi::Status::Success;
}

qacpi::Status qacpi_os_pci_write(qacpi::PciAddress address, uint64_t offset, uint8_t size, uint64_t value) {
	auto* space = pci::get_space(address.segment, address.bus, address.device, address.function);
	if (size == 1) {
		*offset(space, volatile u8*, offset) = value;
	}
	else if (size == 2) {
		*offset(space, volatile u16*, offset) = value;
	}
	else if (size == 4) {
		*offset(space, volatile u32*, offset) = value;
	}
	else if (size == 8) {
		*offset(space, volatile u64*, offset) = value;
	}
	else {
		return qacpi::Status::InvalidArgs;
	}
	return qacpi::Status::Success;
}

void qacpi_os_notify(qacpi::NamespaceNode* node, uint64_t value) {
	auto name = node->name();
	println("[kernel][qacpi]: os notify ", kstd::string_view {name.ptr, name.size}, " value ", value);
}
