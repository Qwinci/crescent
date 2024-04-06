#include "arch/aarch64/dtb.hpp"
#include "mem/pmalloc.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include <stdio.h>

bool IRQS_ENABLED;
namespace {
	Thread* CURRENT_THREAD;
}

Thread* get_current_thread() {
	return CURRENT_THREAD;
}

void set_current_thread(Thread* thread) {
	CURRENT_THREAD = thread;
}

extern "C" void sched_switch_thread(Thread* prev, Thread* current) {

}

class StdioLogSink : public LogSink {
public:
	void write(kstd::string_view str) override {
		printf("%.*s", static_cast<int>(str.size()), str.data());
		fflush(stdout);
	}
};
StdioLogSink LOG_SINK {};

[[noreturn]] void kmain();

extern void kernel_dtb_init(dtb::Dtb plain_dtb);

int main() {
	LOG.lock()->register_sink(&LOG_SINK);

	auto mem = malloc(1024ULL * 1024 * 1024 * 2);
	pmalloc_add_mem(reinterpret_cast<usize>(mem), 1024ULL * 1024 * 1024 * 2);

	KERNEL_PROCESS.initialize("kernel", false);
	CURRENT_THREAD = new Thread {"kernel", nullptr, &*KERNEL_PROCESS};
	return 0;
}
