#pragma once

#include "src/common.hpp"
#include "src/shm_buffer.hpp"
#include <optional>
#include <wayland-client-protocol.h>

class Popup {
public:

	Popup(int x, int y, int w, int h, Color bg, zwlr_layer_surface_v1 *parent);
	bool visible() const;
	void render();

private:
	Color bg;
	bool _visible    = false; /* means compositor said we should be done */
	bool _configured = false;

	std::optional<ShmBuffer>   _bufs;
	wl_unique_ptr<wl_surface>  _wl_surface;
	wl_unique_ptr<xdg_surface> _xdg_surface;
	wl_unique_ptr<xdg_popup>   _xdg_popup;

	void configure_popup(int32_t x, int32_t y, int32_t width, int32_t height);
	void configure_surface(uint32_t serial);

	/* callbacks */
	friend void popup_configure_notify(void *data, struct xdg_popup *xdg_popup, int32_t x, int32_t y, int32_t width, int32_t height);
	friend void surface_configure_notify(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
	friend void popup_done_notify(void *data, struct xdg_popup *xdg_popup);
};
