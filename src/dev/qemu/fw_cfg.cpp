#include "fw_cfg.hpp"
#include "acpi/acpi.hpp"
#include "mem/portspace.hpp"
#include "mem/register.hpp"
#include "qacpi/resources.hpp"
#include "stdio.hpp"

namespace {
	constexpr qacpi::StringView QEMU_FW_CFG_ID {"QEMU0002"};
	PortSpace SPACE {0};
	bool PRESENT = false;
}

namespace regs {
	static constexpr BasicRegister<u16> SEL {0};
	static constexpr BasicRegister<u8> DATA {1};
}

namespace sel {
	static constexpr u16 SIGNATURE = 0;
	static constexpr u16 ID = 1;
	static constexpr u16 FILE_DIR = 0x19;
}

static u32 read32() {
	u32 value = 0;
	value |= SPACE.load(regs::DATA) << 24;
	value |= SPACE.load(regs::DATA) << 16;
	value |= SPACE.load(regs::DATA) << 8;
	value |= SPACE.load(regs::DATA);
	return value;
}

bool qemu_fw_cfg_present() {
	return PRESENT;
}

kstd::optional<kstd::vector<u8>> qemu_fw_cfg_get_file(kstd::string_view name) {
	if (!PRESENT) {
		return {};
	}

	SPACE.store(regs::SEL, sel::FILE_DIR);

	u32 num_files = read32();
	for (u32 i = 0; i < num_files; ++i) {
		u32 size = read32();
		u16 select = 0;
		select |= SPACE.load(regs::DATA) << 8;
		select |= SPACE.load(regs::DATA);
		SPACE.load(regs::DATA);
		SPACE.load(regs::DATA);
		char file_name[56];
		for (auto& c : file_name) {
			c = static_cast<char>(SPACE.load(regs::DATA));
		}

		if (name == file_name) {
			SPACE.store(regs::SEL, select);

			kstd::vector<u8> data;
			data.resize(size);

			for (u32 j = 0; j < size; ++j) {
				data[j] = SPACE.load(regs::DATA);
			}

			return data;
		}
	}

	return {};
}

void qemu_fw_cfg_init() {
	qacpi::NamespaceNode* fw_node = nullptr;

	acpi::GLOBAL_CTX->discover_nodes(acpi::GLOBAL_CTX->get_root(), &QEMU_FW_CFG_ID, 1, [&](qacpi::Context& ctx, qacpi::NamespaceNode* node) {
		fw_node = node;
		return qacpi::IterDecision::Break;
	});

	if (!fw_node) {
		return;
	}

	auto res_obj = qacpi::ObjectRef::empty();
	assert(acpi::GLOBAL_CTX->evaluate(fw_node, "_CRS", res_obj) == qacpi::Status::Success);
	auto buf = res_obj->get<qacpi::Buffer>();

	size_t offset = 0;
	qacpi::Resource res;
	assert(qacpi::resource_parse(buf->data(), buf->size(), offset, res) == qacpi::Status::Success);

	auto port = res.get<qacpi::IoPortDescriptor>();
	assert(port);

	SPACE = PortSpace {port->min_base};

	SPACE.store(regs::SEL, sel::SIGNATURE);
	if (SPACE.load(regs::DATA) != 'Q' ||
		SPACE.load(regs::DATA) != 'E' ||
		SPACE.load(regs::DATA) != 'M' ||
		SPACE.load(regs::DATA) != 'U') {
		return;
	}

	PRESENT = true;
	println("[kernel][qemu]: qemu fw_cfg present");
}
