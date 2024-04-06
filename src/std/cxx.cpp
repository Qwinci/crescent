#include "stdio.hpp"

extern "C" [[noreturn]] void __cxa_pure_virtual() {
	panic("Pure virtual function called");
}
