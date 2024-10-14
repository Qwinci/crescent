#include "utils/driver.hpp"

namespace virtio {
	struct PciCap {
		u8 id;
		u8 next;
		u8 len;
		u8 cfg_type;
		u8 bar;
		u8 index;
		u8 padding[2];
		u32 offset;
		u32 length;
	};

	static constexpr u8 TYPE_COMMON_CFG = 1;
	static constexpr u8 TYPE_NOTIFY_CFG = 2;
	static constexpr u8 TYPE_ISR_CFG = 3;
	static constexpr u8 TYPE_DEVICE_CFG = 4;
	static constexpr u8 TYPE_PCI_CFG = 5;
	static constexpr u8 TYPE_SHARED_MEM_CFG = 8;
	static constexpr u8 TYPE_VENDOR_CFG = 9;

	struct CommonCfg {
		u32 device_feature_select;
		u32 device_feature;
		u32 driver_feature_select;
		u32 driver_feature;
		u16 config_msix_vector;
		u16 num_queues;
		u8 device_status;
		u8 config_generation;
		u16 queue_select;
		u16 queue_size;
		u16 queue_msix_vector;
		u16 queue_enable;
		u16 queue_notify_off;
		u64 queue_desc;
		u64 queue_driver;
		u64 queue_device;
		u16 queue_notify_data;
		u16 queue_reset;
	};

	struct NotifyCap {
		PciCap cap;
		u32 notify_off_multiplier;
	};

	static constexpr u32 NO_VECTOR = 0xFFFF;
}

static InitStatus virtio_nic_init(pci::Device& device) {
	println("[kernel][nic]: virtio nic init");

	return InitStatus::Success;
}

static constexpr PciDriver VIRTIO_NIC_DRIVER {
	.init = virtio_nic_init,
	.match = PciMatch::Device,
	.devices {
		{.vendor = 0x1AF4, .device = 0x1041}
	}
};

PCI_DRIVER(VIRTIO_NIC_DRIVER);
