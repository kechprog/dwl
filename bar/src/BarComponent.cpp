#include "BarComponent.hpp"
#include "cairo.h"
#include "pango/pangocairo.h"
#include "src/common.hpp"
#include "src/config.hpp"
#include "src/State.hpp"
#include <cassert>

static inline void setColor(cairo_t *p, Color c)
{

	const auto [r, g, b, a] = c;
	cairo_set_source_rgba(p,
		r / 255.0, g / 255.0, b / 255.0, a / 255.0);
}

TextComponent::TextComponent() {}

TextComponent::TextComponent(int align)
	  : _align {align} ,
		_pangoLayout {wl_unique_ptr<PangoLayout> {pango_layout_new(state::pango_ctx.get())}} 
{
	pango_layout_set_font_description(_pangoLayout.get(), state::barfont.description);
}	

/* h==-1 => full height */
std::tuple<int, int, int> TextComponent::dim(Monitor &_mon) const
{
	int w, h;
	pango_layout_get_size(_pangoLayout.get(), &w, &h);
	return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, -1, _align);
}

void TextComponent::setText(std::string text)
{
	_text = std::move(text);
	pango_layout_set_text(_pangoLayout.get(), _text.c_str(), _text.size());
}

void TextComponent::setCol(Color bg, Color fg)
{
	this->bg = bg;
	this->fg = fg;
}

void TextComponent::render(cairo_t *painter, Monitor &mon) const
{
	const auto width = cairo_image_surface_get_width(cairo_get_target(painter));
	const auto hieght = cairo_image_surface_get_height(cairo_get_target(painter));
	const auto dim = this->dim(mon);
	if (std::get<1>(dim) != -1)
		assert(hieght == std::get<1>(dim));
	assert(width == std::get<0>(dim));

	pango_cairo_update_layout(painter, _pangoLayout.get());

	setColor(painter, bg);
	cairo_rectangle(painter, 0, 0, width, hieght);
	cairo_fill(painter);

	setColor(painter, fg);
	cairo_move_to(painter, paddingX, paddingY);
	pango_cairo_show_layout(painter, _pangoLayout.get());
}
