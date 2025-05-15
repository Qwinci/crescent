#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <crescent/syscalls.h>
#include <crescent/event.h>

typedef struct WindowerProtocolRequest {
	enum {
		WindowerProtocolRequestCreateWindow,
		WindowerProtocolRequestCloseWindow,
		WindowerProtocolRequestRedraw
	} type;
	union {
		struct {
			uint32_t x;
			uint32_t y;
			uint32_t width;
			uint32_t height;
		} create_window;

		struct {
			void* window_handle;
		} close_window;

		struct {
			void* window_handle;
		} redraw;
	};
} WindowerProtocolRequest;

typedef struct WindowerProtocolResponse {
	enum {
		WindowerProtocolResponseAck,
		WindowerProtocolResponseConnected,
		WindowerProtocolResponseWindowCreated
	} type;

	union {
		struct {
			CrescentHandle event_handle;
		} connected;

		struct {
			void* window_handle;
		} ack;

		struct {
			void* window_handle;
			CrescentHandle fb_handle;
		} window_created;
	};
} WindowerProtocolResponse;

typedef struct WindowerProtocolWindowEvent {
	enum {
		WindowerProtocolWindowEventCloseRequested,
		WindowerProtocolWindowEventMouse,
		WindowerProtocolWindowEventMouseEnter,
		WindowerProtocolWindowEventMouseLeave,
		WindowerProtocolWindowEventKey
	} type;

	void* window_handle;

	union {
		char dummy;

		struct {
			uint32_t x;
			uint32_t y;
			bool left_pressed;
			bool right_pressed;
			bool middle_pressed;
		} mouse;

		struct {
			Scancode code;
			bool prev_pressed;
			bool pressed;
		} key;
	};
} WindowerProtocolWindowEvent;
