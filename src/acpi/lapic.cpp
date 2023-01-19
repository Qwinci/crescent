#include "lapic.hpp"
#include "utils.hpp"

usize Lapic::base = 0;

u32 Lapic::read(Reg reg) {
	return *cast<volatile u32*>(base + as<u32>(reg));
}

void Lapic::write(Reg reg, u32 value) {
	*cast<volatile u32*>(base + as<u32>(reg)) = value;
}