#ifndef CRESCENT_DEVLINK_H
#define CRESCENT_DEVLINK_H

#include "syscalls.h"
#include <stdint.h>

typedef enum CrescentDeviceType {
	CrescentDeviceTypeFb,
	CrescentDeviceTypeGpu,
	CrescentDeviceTypeSound,
	CrescentDeviceTypeMax
} CrescentDeviceType;

typedef enum DevLinkRequestType {
	DevLinkRequestGetDevices,
	DevLinkRequestOpenDevice,
	DevLinkRequestSpecific
} DevLinkRequestType;

typedef struct DevLinkRequest {
#ifdef __cplusplus
	DevLinkRequestType type {};
	size_t size {sizeof(DevLinkRequest)};
	CrescentHandle handle {INVALID_CRESCENT_HANDLE};
#else
	DevLinkRequestType type;
	size_t size;
	CrescentHandle handle;
#endif

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
	} data
#ifdef __cplusplus
	{.dummy {}};
#else
	;
#endif
} DevLinkRequest;

typedef struct DevLinkResponse {
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
} DevLinkResponse;

typedef struct DevLink {
	DevLinkRequest* request;
	union {
		char* response_buffer;
		DevLinkResponse* response;
	};
	size_t response_buf_size;
} DevLink;

#define DEVLINK_BUFFER_SIZE 1024

typedef enum FbLinkOp {
	FbLinkOpGetInfo,
	FbLinkOpMap,
	FbLinkOpFlip
} FbLinkOp;

typedef struct FbLink {
#ifdef __cplusplus
	DevLinkRequest request {
		.type = DevLinkRequestSpecific,
		.size = sizeof(FbLink)
	};
	FbLinkOp op {};
#else
	DevLinkRequest request;
	FbLinkOp op;

#define FB_LINK_INITIALIZER {.request = {.type = DevLinkRequestSpecific, .size = sizeof(FbLink)}}

#endif
} FbLink;

typedef struct FbLinkResponse {
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
} FbLinkResponse;

#define FB_LINK_DOUBLE_BUFFER (1 << 0)

typedef enum SoundLinkOp {
	SoundLinkOpGetInfo,
	SoundLinkOpGetOutputInfo,
	SoundLinkOpSetActiveOutput,
	SoundLinkOpSetOutputParams,
	SoundLinkOpQueueOutput,
	SoundLinkOpPlay,
	SoundLinkOpWaitUntilConsumed
} SoundLinkOp;

typedef enum SoundFormat {
	SoundFormatNone,
	SoundFormatPcmU8,
	SoundFormatPcmU16,
	SoundFormatPcmU20,
	SoundFormatPcmU24,
	SoundFormatPcmU32
} SoundFormat;

typedef struct SoundOutputParams {
	uint32_t sample_rate;
	uint32_t channels;
	SoundFormat fmt;
} SoundOutputParams;

typedef enum SoundDeviceType {
	SoundDeviceTypeHeadphone,
	SoundDeviceTypeSpeaker,
	SoundDeviceTypeLineOut,
	SoundDeviceTypeUnknown
} SoundDeviceType;

typedef struct SoundOutputInfo {
	char name[128];
	size_t name_len;
	size_t buffer_size;
	void* id;
	SoundDeviceType type;
} SoundOutputInfo;

typedef struct SoundLink {
#ifdef __cplusplus
	DevLinkRequest request {
		.type = DevLinkRequestSpecific,
		.size = sizeof(SoundLink)
	};

	SoundLinkOp op {};
#else
	DevLinkRequest request;
	SoundLinkOp op;

#define SOUND_LINK_INITIALIZER {.request = {.type = DevLinkRequestSpecific, .size = sizeof(SoundLink)}}

#endif

	union {
		struct {
			size_t index;
		} get_output_info;

		struct {
			void* id;
		} set_active_output;

		struct {
			SoundOutputParams params;
		} set_output_params;

		struct {
			const void* buffer;
			size_t len;
		} queue_output;

		struct {
			bool play;
		} play;

		struct {
			size_t trip_size;
		} wait_until_consumed;

#ifdef __cplusplus
		char dummy {};
#else
		char dummy;
#endif
	};
} SoundLink;

typedef struct SoundLinkResponse {
	union {
		struct {
			size_t output_count;
		} info;

		SoundOutputInfo output_info;

		struct {
			SoundOutputParams actual;
		} set_output_params;

		struct {
			size_t remaining;
		} wait_until_consumed;
	};
} SoundLinkResponse;

#endif
