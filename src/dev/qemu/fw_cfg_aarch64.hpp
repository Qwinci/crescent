#pragma once
#include "types.hpp"
#include "optional.hpp"

kstd::optional<u16> qemu_fw_cfg_get_file(kstd::string_view name);
bool qemu_fw_cfg_write(u16 selector, const void* data, u32 size);
