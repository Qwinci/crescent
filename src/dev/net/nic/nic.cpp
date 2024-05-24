#include "nic.hpp"

ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<Nic>>>> NICS;
