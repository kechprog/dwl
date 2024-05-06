// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <optional>
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "shm_buffer.hpp"
#include "Popup.hpp"

/* avoid cicling dependancy of header */
class IBarComponent;

struct Tag {
	int state;
	int numClients;
	int focusedClient;
};

struct Monitor;
class Bar {
	static const zwlr_layer_surface_v1_listener _layerSurfaceListener;
	static const wl_callback_listener           _frameListener;

	wl_unique_ptr<wl_surface>            _wl_surface;
	wl_unique_ptr<zwlr_layer_surface_v1> layerSurface;
	std::optional<ShmBuffer>             bufs;
	std::optional<Popup>                 popup;
	bool invalid {false};

	/* valid during invalidate/render */
	cairo_t* painter {nullptr};
	cairo_surface_t *cairo_surface {nullptr};
	int x_left, x_right;
	ColorScheme colorScheme;

	void layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height);
	void renderComponent(IBarComponent *component);
	void render();

public:
	Bar() = default;
	const struct wl_surface* surface() const;
	bool visible() const;
	int height() const;
	void show(wl_output* output);
	void hide();

	/* state updating */
	void invalidate();
	void click(Monitor* mon, int x, int y, int btn);
};
