#pragma once
#include "dev/evm.hpp"

bool vmx_supported();
kstd::shared_ptr<evm::Evm> vmx_create_vm();
