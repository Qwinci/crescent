#pragma once
#include "crescent/syscalls.h"
#include "protocol.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WindowerWindow {
	uint32_t width;
	uint32_t height;

	struct Windower* owner;
	void* handle;
	CrescentHandle fb_handle;
	void* fb_mapping;
} WindowerWindow;

typedef struct Windower {
	CrescentHandle control_connection;
	CrescentHandle event_pipe;
} Windower;

int windower_connect(Windower* res);
void windower_destroy(Windower* windower);
int windower_create_window(Windower* windower, WindowerWindow* res, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void windower_window_destroy(WindowerWindow* window);
int windower_window_map_fb(WindowerWindow* window);
WindowerProtocolWindowEvent windower_window_wait_for_event(WindowerWindow* window);
int windower_window_poll_event(WindowerWindow* window, WindowerProtocolWindowEvent* event);
void windower_window_redraw(WindowerWindow* window);
void windower_window_close(WindowerWindow* window);

static inline void* windower_window_get_fb_mapping(WindowerWindow* window) {
	return window->fb_mapping;
}

#ifdef __cplusplus
}
#endif
