#pragma once
#include "crescent/syscalls.h"
#include "protocol.hpp"
#include <stdint.h>

namespace windower {
	struct Window {
		~Window();

		constexpr Window() = default;
		constexpr Window(const Window&) = delete;
		constexpr Window(Window&& other) {
			width = other.width;
			height = other.height;
			owner = other.owner;
			handle = other.handle;
			fb_handle = other.fb_handle;
			fb_mapping = other.fb_mapping;
			other.owner = nullptr;
			other.handle = nullptr;
			other.fb_handle = INVALID_CRESCENT_HANDLE;
			other.fb_mapping = nullptr;
		}

		constexpr Window& operator=(const Window&) = delete;
		constexpr Window& operator=(Window&&) = delete;

		int map_fb();

		[[nodiscard]] constexpr void* get_fb_mapping() const {
			return fb_mapping;
		}

		protocol::WindowEvent wait_for_event();

		void close();

		uint32_t width {};
		uint32_t height {};
	private:
		friend struct Windower;

		struct Windower* owner {};
		void* handle {};
		CrescentHandle fb_handle = INVALID_CRESCENT_HANDLE;
		void* fb_mapping = nullptr;
	};

	struct Windower {
		~Windower();

		static int connect(Windower& res);

		int create_window(Window& res, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

	private:
		friend Window;

		CrescentHandle connection = INVALID_CRESCENT_HANDLE;
	};
}
