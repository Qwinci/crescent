#include "crescent/syscall.h"
#include "crescent/devlink.h"

int sys_devlink(const DevLink& dev_link) {
	return static_cast<int>(syscall(SYS_DEVLINK, &dev_link));
}

int sys_close_handle(CrescentHandle handle) {
	return static_cast<int>(syscall(SYS_CLOSE_HANDLE, handle));
}

int sys_map(void** addr, size_t size, int protection) {
	return static_cast<int>(syscall(SYS_MAP, addr, size, protection));
}

int sys_unmap(void* ptr, size_t size) {
	return static_cast<int>(syscall(SYS_UNMAP, ptr, size));
}

int main() {
	DevLinkRequest request {
		.type = DevLinkRequestType::GetDevices,
		.data {
			.get_devices {
				.type = CrescentDeviceType::Fb
			}
		}
	};
	char response[DEVLINK_BUFFER_SIZE];
	DevLink link {
		.request = &request,
		.response_buffer = response,
		.response_buf_size = DEVLINK_BUFFER_SIZE
	};
	auto status = sys_devlink(link);
	request.type = DevLinkRequestType::OpenDevice;
	request.data.open_device.device = link.response->get_devices.names[0];
	request.data.open_device.device_len = link.response->get_devices.name_sizes[0];
	status = sys_devlink(link);
	auto handle = link.response->open_device.handle;

	FbLink fb_link {
		.op = FbLinkOp::GetInfo
	};

	link.request = &fb_link.request;
	fb_link.request.handle = handle;
	status = sys_devlink(link);
	auto* fb_resp = static_cast<FbLinkResponse*>(link.response->specific);
	auto info = fb_resp->info;
	fb_link.op = FbLinkOp::Map;
	status = sys_devlink(link);
	void* mapping = fb_resp->map.mapping;
	for (int y = 0; y < 80; ++y) {
		for (int x = 0; x < 80; ++x) {
			*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(mapping) + y * info.pitch + x * (info.bpp / 8)) = 0xFF0000;
		}
	}

	sys_close_handle(handle);
	sys_unmap(mapping, info.pitch * info.height * 4);

	return 0;
}
