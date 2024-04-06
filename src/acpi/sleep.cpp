#include "sleep.hpp"
#include "acpi.hpp"
#include "manually_init.hpp"
#include "mem/register.hpp"
#include "qacpi/context.hpp"

namespace pm1_ctrl {
	static constexpr BitField<u64, u8> SLP_TYP {10, 3};
	static constexpr BitField<u64, bool> SLP_EN {13, 1};
}

namespace pm1_sts {
	static constexpr BitField<u64, bool> WAK_STS {15, 1};
}

namespace acpi {
	extern ManuallyInit<qacpi::Context> GLOBAL_CTX;
	extern Fadt* GLOBAL_FADT;

	void enter_sleep_state(SleepState state) {
		switch (state) {
			case SleepState::S5:
			{
				qacpi::ObjectRef arg;
				assert(arg);
				arg->data = uint64_t {5};
				auto ret = qacpi::ObjectRef::empty();
				auto status = GLOBAL_CTX->evaluate("_PTS", ret, &arg, 1);
				println("_PTS(5) returned ", qacpi::status_to_str(status));

				status = GLOBAL_CTX->evaluate("_S5_", ret);
				assert(status == qacpi::Status::Success && "failed to evaluate _S5");
				assert(ret->get<qacpi::Package>());
				auto& pkg = ret->get_unsafe<qacpi::Package>();
				assert(pkg.data->element_count >= 2);
				u8 slp_typa = pkg.data->elements[0]->get_unsafe<uint64_t>() & 0b111;
				u8 slp_typb = pkg.data->elements[1]->get_unsafe<uint64_t>() & 0b111;

				IrqGuard shutdown_guard {};
				// clear WAK_STS
				BitValue<u64> pm1_sts {pm1_sts::WAK_STS(true)};
				write_to_addr(GLOBAL_FADT->x_pm1a_evt_blk, pm1_sts);
				write_to_addr(GLOBAL_FADT->x_pm1b_evt_blk, pm1_sts);

				BitValue<u64> pm1_ctrl = {read_from_addr(GLOBAL_FADT->x_pm1a_cnt_blk) | read_from_addr(GLOBAL_FADT->x_pm1b_cnt_blk)};
				pm1_ctrl &= ~pm1_ctrl::SLP_EN;
				pm1_ctrl &= ~pm1_ctrl::SLP_TYP;
				u64 pm1a_ctrl = pm1_ctrl | pm1_ctrl::SLP_TYP(slp_typa);
				u64 pm1b_ctrl = pm1_ctrl | pm1_ctrl::SLP_TYP(slp_typb);
				write_to_addr(GLOBAL_FADT->x_pm1a_cnt_blk, pm1a_ctrl);
				write_to_addr(GLOBAL_FADT->x_pm1b_cnt_blk, pm1b_ctrl);
				pm1a_ctrl = pm1_ctrl | pm1_ctrl::SLP_TYP(slp_typa) | pm1_ctrl::SLP_EN(true);
				pm1b_ctrl = pm1_ctrl | pm1_ctrl::SLP_TYP(slp_typb) | pm1_ctrl::SLP_EN(true);
				write_to_addr(GLOBAL_FADT->x_pm1a_cnt_blk, pm1a_ctrl);
				write_to_addr(GLOBAL_FADT->x_pm1b_cnt_blk, pm1b_ctrl);
				break;
			}
		}
	}
}
