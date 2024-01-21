#include <iomanip>
#include <iostream>
#include <sstream>
#include <wayland-client-protocol.h>
#include <pango/pangocairo.h>
#include <chrono>
#include "bar.hpp"
#include "cairo.h"
#include "config.hpp"
#include "pango/pango-font.h"
#include "pango/pango-fontmap.h"
#include "pango/pango-layout.h"

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

struct Font {
	PangoFontDescription* description;
	int height {0};
};

static Font getFont()
{
	auto fontMap = pango_cairo_font_map_get_default();
	if (!fontMap) {
		die("pango_cairo_font_map_get_default");
	}
	auto fontDesc = pango_font_description_from_string(font);
	if (!fontDesc) {
		die("pango_font_description_from_string");
	}
	auto tempContext = pango_font_map_create_context(fontMap);
	if (!tempContext) {
		die("pango_font_map_create_context");
	}
	auto font = pango_font_map_load_font(fontMap, tempContext, fontDesc);
	if (!font) {
		die("pango_font_map_load_font");
	}
	auto metrics = pango_font_get_metrics(font, pango_language_get_default());
	if (!metrics) {
		die("pango_font_get_metrics");
	}

	auto res = Font {};
	res.description = fontDesc;
	res.height = PANGO_PIXELS(pango_font_metrics_get_height(metrics));

	pango_font_metrics_unref(metrics);
	g_object_unref(font);
	g_object_unref(tempContext);
	return res;
}
static Font barfont = getFont();

BarComponent::BarComponent() {}

BarComponent::BarComponent(wl_unique_ptr<PangoLayout> layout)
	  : pangoLayout {std::move(layout)} {}

std::pair<int, int> BarComponent::dim()
{
	int w, h;
	pango_layout_get_size(pangoLayout.get(), &w, &h);
	return std::make_pair(PANGO_PIXELS(w) , 0);
}

void BarComponent::setText(std::string text)
{
	_text = std::move(text);
	pango_layout_set_text(pangoLayout.get(), _text.c_str(), _text.size());
}

void BarComponent::setCol(Color bg, Color fg)
{
	this->bg = bg;
	this->fg = fg;
}

Bar::Bar()
{
	pangoContext.reset(pango_font_map_create_context(pango_cairo_font_map_get_default()));
	if (!pangoContext) {
		die("pango_font_map_create_context");
	}

	for (const auto& tagName : tagNames) {
		tags.push_back({ TagState::None, 0, 0, createComponent(0, tagName) });
	}

	_timeCmp   = createComponent(1); /* creates zero initialized component */
	updateTime();
	layoutCmp = createComponent(0);
	titleCmp  = createComponent(0);
	statusCmp = createComponent(0);
	_batCmp    = createComponent(1, "BAT");

	for (auto& cmp : _brightnessCmp) {
		cmp = createComponent(1);
	}

}

const wl_surface* Bar::surface() const
{
	return _surface.get();
}

bool Bar::visible() const
{
	return _surface.get();
}

void Bar::show(wl_output* output)
{
	if (visible()) {
		return;
	}
	_surface.reset(wl_compositor_create_surface(compositor));
	layerSurface.reset(zwlr_layer_shell_v1_get_layer_surface(wlrLayerShell,
		_surface.get(), output, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "net.tapesoftware.Somebar"));
	zwlr_layer_surface_v1_add_listener(layerSurface.get(), &_layerSurfaceListener, this);
	auto anchor = topbar ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP : ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	zwlr_layer_surface_v1_set_anchor(layerSurface.get(),
		anchor | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

	auto barSize = barfont.height + paddingY * 2;
	zwlr_layer_surface_v1_set_size(layerSurface.get(), 0, barSize);
	zwlr_layer_surface_v1_set_exclusive_zone(layerSurface.get(), barSize);
	wl_surface_commit(_surface.get());
}

void Bar::hide()
{
	if (!visible()) {
		return;
	}
	layerSurface.reset();
	_surface.reset();
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

void Bar::setBat(int perc, bool isCharging)
{
	std::stringstream ss;
	ss << "BAT " << perc << "%"; // TODO: add icons
	_batCmp.setText(ss.str());
}

void Bar::setBrightness(size_t val, size_t idx) {
	std::stringstream ss;
	val = val * 100 / displayConfigs[idx].second;
	ss << "BRIGHTNESS: " << val << "%";
	_brightnessCmp[idx].setText(ss.str());
}

void Bar::invalidate()
{
	if (invalid || !visible()) {
		return;
	}
	invalid = true;
	auto frame = wl_surface_frame(_surface.get());
	wl_callback_add_listener(frame, &_frameListener, this);
	wl_surface_commit(_surface.get());
}

void Bar::click(Monitor* mon, int x, int, int btn)
{
	Arg arg = {0};
	Arg* argp = nullptr;
	int control = ClkNone;
	if (x > statusCmp.x) {
		control = ClkStatusText;
	} else if (x > titleCmp.x) {
		control = ClkWinTitle;
	} else if (x > layoutCmp.x) {
		control = ClkLayoutSymbol;
	} else for (int tag = tags.size()-1; tag >= 0; tag--) {
		if (x > tags[tag].component.x) {
			control = ClkTagBar;
			arg.ui = 1<<tag;
			argp = &arg;
			break;
		}
	}
	for (const auto& button : buttons) {
		if (button.control == control && button.btn == btn) {
			button.func(*mon, *(argp ? argp : &button.arg));
			return;
		}
	}
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
	pango_cairo_update_context(painter, pangoContext.get());
	x_left = x_right = 0;

	updateColorScheme();

	/* bg of bar */
	setColor(colorScheme.barBg);
	cairo_rectangle(painter, 0, 0, bufs->width, bufs->height);
	cairo_fill(painter);

	renderTags();
	renderComponent(layoutCmp);
	renderComponent(titleCmp);
	renderComponent(_timeCmp);
	renderComponent(_batCmp);
	for (auto &bcmp : _brightnessCmp)
		renderComponent(bcmp);
	renderComponent(statusCmp);

	painter = nullptr;
	wl_surface_attach(_surface.get(), bufs->buffer(), 0, 0);
	wl_surface_damage(_surface.get(), 0, 0, bufs->width, bufs->height);
	wl_surface_commit(_surface.get());
	bufs->flip();
	invalid = false;
}

void Bar::renderTags()
{
	for (auto &tag : tags) {
		if (tag.state == 1) /* active */
			tag.component.bg = {255, 255, 255, 255};
		renderComponent(tag.component);
	}
}

void Bar::updateColorScheme(void) {
	this->colorScheme = colors[selected];

	layoutCmp.setCol(colorScheme.cmpBg, colorScheme.text);
	titleCmp.setCol(colorScheme.cmpBg, colorScheme.text);
	_timeCmp.setCol(colorScheme.cmpBg, colorScheme.text);
	_batCmp.setCol(colorScheme.cmpBg, colorScheme.text);
	statusCmp.setCol(colorScheme.cmpBg, colorScheme.text);
	for (auto &bcmp : _brightnessCmp)
		bcmp.setCol(colorScheme.cmpBg, colorScheme.text);
	for (auto &tag : tags) 
		tag.component.setCol(colorScheme.cmpBg, colorScheme.text);
}

void Bar::renderComponent(BarComponent& component)
{
	pango_cairo_update_layout(painter, component.pangoLayout.get());
	auto [w, h]= component.dim();
	w += paddingX*2;

	switch (component.align) {
		case 0: /* left */
			component.x = x_left;
			x_left += w;
			break;
		case 1: /* right */
			component.x = bufs->width - x_right - w;
			x_right += w;
			break;
	} 

	setColor(component.bg);
	cairo_rectangle(painter, component.x, 0, w, bufs->height);
	cairo_fill(painter);

	setColor(component.fg);
	cairo_move_to(painter, component.x+paddingX, paddingY);
	pango_cairo_show_layout(painter, component.pangoLayout.get());
}

BarComponent Bar::createComponent(const int align, const std::string &initial)
{
	auto layout = pango_layout_new(pangoContext.get());
	pango_layout_set_font_description(layout, barfont.description);
	auto res = BarComponent {wl_unique_ptr<PangoLayout> {layout}};
	res.setText(initial);
	res.align = align;
	return res;
}
