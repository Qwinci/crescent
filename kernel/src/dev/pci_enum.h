#pragma once
#include "types.h"

void pci_init();
void* pci_get_space(u16 seg, u16 bus, u16 dev, u16 func);