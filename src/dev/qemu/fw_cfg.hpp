#pragma once
#include "optional.hpp"
#include "vector.hpp"

void qemu_fw_cfg_init();
bool qemu_fw_cfg_present();
kstd::optional<kstd::vector<u8>> qemu_fw_cfg_get_file(kstd::string_view name);
