#pragma once
#include "mem/iospace.hpp"
#include "mem/register.hpp"
#include "dev/reset.hpp"
#include "span.hpp"

struct QcomResetController : public ResetController {
	constexpr QcomResetController(IoSpace space, kstd::span<BitRegister<u32>> regs)
		: space {space}, regs {regs} {}

	void set_assert(u32 id, bool assert) final;

private:
	IoSpace space;
	kstd::span<BitRegister<u32>> regs;
};
