#include <iomanip>
#include <iostream>
#include <sstream>
#include <wayland-client-protocol.h>
#include <pango/pangocairo.h>
#include <chrono>
#include "bar.hpp"
#include "State.hpp"
#include "cairo.h"
#include "config.hpp"

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

void Bar::setColor(Color c)
{

	const auto [r, g, b, a] = c;
	cairo_set_source_rgba(painter,
		r / 255.0, g / 255.0, b / 255.0, a / 255.0);
}


Bar::Bar()
{
	for (const auto& tagName : tagNames) {
		tags.push_back({ TagState::None, 0, 0, createComponent(0, tagName) });
	}

	_timeCmp   = createComponent(1); /* creates zero initialized component */
	updateTime();
	layoutCmp = createComponent(0);
	titleCmp  = createComponent(0);
	statusCmp = createComponent(0);
}

const wl_surface* Bar::surface() const
{
	return _wl_surface.get();
}

bool Bar::visible() const
{
	return _wl_surface.get();
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
	auto anchor = topbar ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP : ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	zwlr_layer_surface_v1_set_anchor(layerSurface.get(),
		anchor | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

	auto barSize = state::barfont.height + paddingY * 2;
	zwlr_layer_surface_v1_set_size(layerSurface.get(), 0, barSize);
	zwlr_layer_surface_v1_set_exclusive_zone(layerSurface.get(), barSize);
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

void Bar::setTag(int tag, int state, int numClients, int focusedClient)
{
	auto& t = tags[tag];
	t.state = state;
	t.numClients = numClients;
	t.focusedClient = focusedClient;
}

void Bar::setSelected(bool selected)
{
	this->selected = selected;
}

void Bar::setLayout(const std::string& layout)
{
	layoutCmp.setText(layout);
}

void Bar::setTitle(const std::string& title)
{
	titleCmp.setText(title);
}

void Bar::setStatus(const std::string& status)
{
	statusCmp.setText(status);
}

void Bar::updateTime() 
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    std::tm localtime = *std::localtime(&now_c);
    std::ostringstream ss;

    ss << std::put_time(&localtime, "%H:%M");
    _timeCmp.setText(ss.str());
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
	// TODO: figure me out
	
	// Arg arg = {0};
	// Arg* argp = nullptr;
	// int control = ClkNone;
	// if (x > statusCmp.x) {
	// 	control = ClkStatusText;
	// } else if (x > titleCmp.x) {
	// 	control = ClkWinTitle;
	// } else if (x > layoutCmp.x) {
	// 	control = ClkLayoutSymbol;
	// } else for (int tag = tags.size()-1; tag >= 0; tag--) {
	// 	if (x > tags[tag].component.x) {
	// 		control = ClkTagBar;
	// 		arg.ui = 1<<tag;
	// 		argp = &arg;
	// 		break;
	// 	}
	// }
	// for (const auto& button : buttons) {
	// 	if (button.control == control && button.btn == btn) {
	// 		button.func(*mon, *(argp ? argp : &button.arg));
	// 		return;
	// 	}
	// }
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

	this->colorScheme = colors[selected];


	/* bg of bar */
	setColor(colorScheme.barBg);
	cairo_rectangle(painter, 0, 0, bufs->width, bufs->height);
	cairo_fill(painter);

	for (auto &cmp : state::components)
		renderComponent(cmp.get());
	
	renderTags();
	renderComponent(&layoutCmp);
	renderComponent(&titleCmp);
	renderComponent(&_timeCmp);
	renderComponent(&statusCmp);

	painter = nullptr;
	wl_surface_attach(_wl_surface.get(), bufs->buffer(), 0, 0);
	wl_surface_damage(_wl_surface.get(), 0, 0, bufs->width, bufs->height);
	wl_surface_commit(_wl_surface.get());
	bufs->flip();
	invalid = false;
}

void Bar::renderTags()
{
	for (auto &tag : tags) {
		// if (tag.state == 1) /* active */
		// 	tag.component.bg = {255, 255, 255, 255};
		renderComponent(&tag.component);
	}
}

// void Bar::renderComponent(TextComponent& component)
// {
// 	auto [w, h, align]= component.dim(state::monitors.front());
// 	if (h == -1) 
// 		h = bufs->height;
//
// 	auto slice_surface = wl_unique_ptr<cairo_surface_t> 
// 		{ cairo_image_surface_create(cairo_image_surface_get_format(cairo_surface), w, h) };
// 	// TODO: move me to IBarComponent::render()
// 	auto slice_painter = wl_unique_ptr<cairo_t> {cairo_create(slice_surface.get())};
//
// 	component.render(slice_painter.get(), state::monitors.front());
//
// 	int x_position;
// 	switch (align) {
// 		case 0: /* left */
// 			x_position = x_left;
// 			x_left += w;
// 			break;
// 		case 1: /* right */
// 			x_position = bufs->width - x_right - w;
// 			x_right += w;
// 			break;
// 	} 
//
// 	cairo_set_source_surface(painter, slice_surface.get(), x_position, 0);
// 	cairo_rectangle(painter, x_position, 0, w, h);
// 	cairo_fill(painter);
// }

void Bar::renderComponent(IBarComponent *cmp)
{
	auto [w, h, align] = cmp->dim(state::monitors.front());
	if (h == -1) 
		h = bufs->height;

	auto slice_surface = wl_unique_ptr<cairo_surface_t> 
		{ cairo_image_surface_create(cairo_image_surface_get_format(cairo_surface), w, h) };
	// TODO: move me to IBarComponent::render()
	auto slice_painter = wl_unique_ptr<cairo_t> {cairo_create(slice_surface.get())};

	cmp->render(slice_painter.get(), state::monitors.front());

	int x_position;
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
	cairo_rectangle(painter, x_position, 0, w, h);
	cairo_fill(painter);
}

TextComponent Bar::createComponent(const int align, const std::string &initial)
{
	auto res = TextComponent {align};
	res.setText(initial);
	return res;
}
