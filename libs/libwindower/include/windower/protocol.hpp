#pragma once
#include <stdint.h>
#include "crescent/syscalls.h"

namespace windower {
	namespace protocol {
		struct Request {
			enum {
				CreateWindow,
				CloseWindow
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
			};
		};

		struct Response {
			enum {
				WindowCreated
			} type;
			union {
				struct {
					void* window_handle;
					CrescentHandle fb_handle;
				} window_created;
			};
		};
	}
}
