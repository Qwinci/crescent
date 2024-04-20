#include "service.hpp"

struct Service {
	DoubleListHook hook {};
	kstd::vector<kstd::string> features;
	kstd::shared_ptr<ProcessDescriptor> descriptor;
};

namespace {
	Spinlock<DoubleList<Service, &Service::hook>> SERVICES {};
}

void service_create(kstd::vector<kstd::string>&& features, Process* process) {
	auto descriptor = kstd::make_shared<ProcessDescriptor>();
	*descriptor->process.lock() = process;
	process->add_descriptor(descriptor.data());
	auto* service = new Service {
		.features {std::move(features)},
		.descriptor {std::move(descriptor)}
	};
	IrqGuard irq_guard {};
	auto guard = SERVICES.lock();
	guard->push(service);
	process->service = service;
}

void service_remove(Process* process) {
	IrqGuard irq_guard {};
	{
		auto proc_guard = process->service->descriptor->process.lock();
		*proc_guard = nullptr;
	}

	auto guard = SERVICES.lock();
	guard->remove(process->service);
	delete process->service;
	process->service = nullptr;
}

kstd::shared_ptr<ProcessDescriptor> service_get(const kstd::vector<kstd::string>& needed_features) {
	IrqGuard irq_guard {};
	auto guard = SERVICES.lock();
	for (const auto& service : *guard) {
		bool service_found = true;

		for (const auto& needed_feature : needed_features) {
			bool found = false;
			for (const auto& feature : service.features) {
				if (feature == needed_feature) {
					found = true;
					break;
				}
			}

			if (!found) {
				service_found = false;
				break;
			}
		}

		if (service_found) {
			return service.descriptor;
		}
	}

	return nullptr;
}
