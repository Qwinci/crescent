#ifndef CRESCENT_DEVLINK_H
#define CRESCENT_DEVLINK_H

#include "syscalls.h"
#include <stdint.h>

enum class CrescentDeviceType {
	Fb,
	Gpu,
	Max
};

enum class DevLinkRequestType {
	GetDevices,
	OpenDevice,
	Specific
};

struct DevLinkRequest {
	DevLinkRequestType type {};
	size_t size {sizeof(DevLinkRequest)};
	CrescentHandle handle {INVALID_CRESCENT_HANDLE};

	union Data {
		char dummy;
		struct {
			CrescentDeviceType type;
		} get_devices;
		struct {
			CrescentDeviceType type;
			const char* device;
			size_t device_len;
		} open_device;
	} data {.dummy {}};
};

struct DevLinkResponse {
	size_t size;
	union {
		struct {
			size_t device_count;
			const char** names;
			size_t* name_sizes;
		} get_devices;

		struct {
			CrescentHandle handle;
		} open_device;

		void* specific;
	};
};

struct DevLink {
	DevLinkRequest* request;
	union {
		char* response_buffer;
		DevLinkResponse* response;
	};
	size_t response_buf_size;
};

#define DEVLINK_BUFFER_SIZE 1024

enum class FbLinkOp {
	GetInfo,
	Map,
	Flip
};

struct FbLink {
	DevLinkRequest request {
		.type = DevLinkRequestType::Specific,
		.size = sizeof(FbLink)
	};

	FbLinkOp op {};
};

struct FbLinkResponse {
	union {
		struct {
			uintptr_t pitch;
			uint32_t width;
			uint32_t height;
			uint32_t bpp;
			uint32_t flags;
		} info;

		struct {
			void* mapping;
		} map;
	};
};

#define FB_LINK_DOUBLE_BUFFER (1 << 0)

#endif
