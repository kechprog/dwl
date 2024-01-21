// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <optional>
#include <string>
#include <vector>
#include <wayland-client.h>
#include "config.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "shm_buffer.hpp"
#include "BarComponent.hpp"

struct Tag {
	int state;
	int numClients;
	int focusedClient;
	BarComponent component;
};

struct Monitor;
class Bar {
	static const zwlr_layer_surface_v1_listener _layerSurfaceListener;
	static const wl_callback_listener           _frameListener;

	wl_unique_ptr<wl_surface>            _surface;
	wl_unique_ptr<zwlr_layer_surface_v1> layerSurface;
	wl_unique_ptr<PangoContext>          pangoContext;
	std::optional<ShmBuffer>             bufs;
	std::vector<Tag> tags;
	BarComponent layoutCmp, titleCmp, statusCmp, _timeCmp, _batCmp;
	std::array<BarComponent, sizeof(displayConfigs) / sizeof(displayConfigs[0])> _brightnessCmp;
	bool selected;
	bool invalid {false};

	// only vaild during render()
	cairo_t* painter {nullptr};
	int x_left, x_right;
	ColorScheme colorScheme;

	void layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height);
	void render();
	void setColor(Color color);
	void renderTags();

	// low-level rendering
	void updateColorScheme(void);
	void renderComponent(BarComponent& component);
	BarComponent createComponent(const int align, const std::string& initial = {});

public:
	Bar();
	const wl_surface* surface() const;
	bool visible() const;
	void show(wl_output* output);
	void hide();
	/* state updating */
	void setTag      (int tag, int state, int numClients, int focusedClient);
	void setSelected (bool selected);
	void setLayout   (const std::string& layout);
	void setTitle    (const std::string& title );
	void setStatus   (const std::string& status);
	void setBat      (int perc, bool isCharging);
	void setBrightness(size_t val, size_t idx);
	void updateTime ();
	void invalidate();
	void click(Monitor* mon, int x, int y, int btn);
};
