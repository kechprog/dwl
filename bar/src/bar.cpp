#include <wayland-client-protocol.h>
#include <pango/pangocairo.h>
#include "bar.hpp"
#include "State.hpp"
#include "cairo.h"
#include "config.hpp"
#include "BarComponent.hpp"
const zwlr_layer_surface_v1_listener Bar::_layerSurfaceListener = {

	[](void* owner, zwlr_layer_surface_v1*, uint32_t serial, uint32_t width, uint32_t height)
	{
		static_cast<Bar*>(owner)->layerSurfaceConfigure(serial, width, height);
	}
};
const wl_callback_listener Bar::_frameListener = {
	[](void* owner, wl_callback* cb, uint32_t)
	{
		static_cast<Bar*>(owner)->render();
		wl_callback_destroy(cb);
	}
};

const wl_surface* Bar::surface() const
{
	return _wl_surface.get();
}

bool Bar::visible() const
{
	return _wl_surface.get();
}

int Bar::height() const
{
	return config::appearence::bar_size;
}

void Bar::show(wl_output* output)
{
	if (visible()) {
		return;
	}
	_wl_surface.reset(wl_compositor_create_surface(compositor));
	layerSurface.reset(zwlr_layer_shell_v1_get_layer_surface(wlrLayerShell,
		_wl_surface.get(), output, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "net.tapesoftware.Somebar"));
	zwlr_layer_surface_v1_add_listener(layerSurface.get(), &_layerSurfaceListener, this);
	const auto topbar = config::appearence::topbar;
	auto anchor = topbar ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP : ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	zwlr_layer_surface_v1_set_anchor(layerSurface.get(),
		anchor | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

	zwlr_layer_surface_v1_set_size(layerSurface.get(), 0, this->height());
	zwlr_layer_surface_v1_set_exclusive_zone(layerSurface.get(), this->height());
	wl_surface_commit(_wl_surface.get());
}

void Bar::hide()
{
	if (!visible()) {
		return;
	}
	layerSurface.reset();
	_wl_surface.reset();
	bufs.reset();
}

void Bar::invalidate()
{
	if (invalid || !visible()) {
		return;
	}

	invalid = true;
	auto frame = wl_surface_frame(_wl_surface.get());
	wl_callback_add_listener(frame, &_frameListener, this);
	wl_surface_commit(_wl_surface.get());
}

void Bar::click(Monitor* mon, int x, int, int btn)
{
	const auto &red = Color { 0xff, 0, 0, 0xff };
	if (this->popup)
		this->popup.reset();
	else 
		this->popup.emplace(0, 0, 100, 100, red, layerSurface.get());
}

void Bar::layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height)
{
	zwlr_layer_surface_v1_ack_configure(layerSurface.get(), serial);
	if (bufs && width == bufs->width && height == bufs->height) {
		return;
	}
	bufs.emplace(width, height, WL_SHM_FORMAT_XRGB8888);
	render();
}

void Bar::render()
{
	if (!bufs) {
		return;
	}
	auto img = wl_unique_ptr<cairo_surface_t> {cairo_image_surface_create_for_data(
		bufs->data(),
		CAIRO_FORMAT_ARGB32,
		bufs->width,
		bufs->height,
		bufs->stride
		)};
	auto _painter = wl_unique_ptr<cairo_t> {cairo_create(img.get())};

	painter = _painter.get();
	cairo_surface = img.get();

	pango_cairo_update_context(painter, state::pango_ctx.get());
	x_left = x_right = 0;

	/* bg of bar */
	const Monitor *mon = wl_container_of(this, mon, bar);
	const auto &clr_schm = config::appearence::colors[mon == state::selmon];
	setColor(painter, clr_schm.bar_bg);
	cairo_rectangle(painter, 0, 0, bufs->width, bufs->height);
	cairo_fill(painter);

	for (auto &cmp : state::components)
		renderComponent(cmp.get());

	painter = nullptr;
	wl_surface_attach(_wl_surface.get(), bufs->buffer(), 0, 0);
	wl_surface_damage(_wl_surface.get(), 0, 0, bufs->width, bufs->height);
	wl_surface_commit(_wl_surface.get());
	bufs->flip();
	invalid = false;
}

void Bar::renderComponent(IBarComponent *cmp)
{
	const Monitor *mon = wl_container_of(this, mon, bar);
	auto [w, align] = cmp->setup(mon, bufs->height);

	auto slice_surface = wl_unique_ptr<cairo_surface_t> 
		{ cairo_image_surface_create(cairo_image_surface_get_format(cairo_surface), w, bufs->height) };
	auto slice_painter = wl_unique_ptr<cairo_t> {cairo_create(slice_surface.get())};

	cmp->render(slice_painter.get(), mon);

	int x_position = -1;
	switch (align) {
		case 0: /* left */
			x_position = x_left;
			x_left += w;
			break;
		case 1: /* right */
			x_position = bufs->width - x_right - w;
			x_right += w;
			break;
	} 

	cairo_set_source_surface(painter, slice_surface.get(), x_position, 0);
	cairo_rectangle(painter, x_position, 0, w, bufs->height);
	cairo_fill(painter);
}
