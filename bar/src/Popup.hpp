#pragma once

#include "src/common.hpp"
#include "src/shm_buffer.hpp"
#include <optional>
#include <wayland-client-protocol.h>

class Popup {
public:

	Popup(const Popup&)            = delete;
	Popup& operator=(const Popup&) = delete;

	Popup(int x, int y, int w, int h, Color bg, zwlr_layer_surface_v1 *parent);
	void configure(int32_t x, int32_t y, int32_t width, int32_t height);

private:
	Color bg;
	std::optional<ShmBuffer>   _bufs;
	wl_unique_ptr<wl_surface>  _wl_surface;
	wl_unique_ptr<xdg_surface> _xdg_surface;
	wl_unique_ptr<xdg_popup>   _xdg_popup;
};
