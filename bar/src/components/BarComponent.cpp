#include "BarComponent.hpp"
#include "cairo.h"
#include "pango/pango-layout.h"
#include "pango/pangocairo.h"
#include "src/common.hpp"
#include "src/config.hpp"
#include "src/State.hpp"
#include <iostream>
#include <sstream>

static inline void setColor(cairo_t *p, Color c)
{
	const auto [r, g, b, a] = c;
	cairo_set_source_rgba(p,
		r / 255.0, g / 255.0, b / 255.0, a / 255.0);
}


/*****************************************************************************************/
/*------------------------------------BatteryComponent-----------------------------------*/
/*****************************************************************************************/
BatteryComponent::BatteryComponent() noexcept
	: pango_layout {wl_unique_ptr<PangoLayout> {pango_layout_new(state::pango_ctx.get())}} 
{
	pango_layout_set_font_description(this->pango_layout.get(), state::barfont.description);
}

std::tuple<int, int, int> BatteryComponent::dim(const Monitor &_mon)
{
	const auto status = state::bat_is_charging ? "charging" : "discharging";
	std::stringstream ss;
	int w, h;

	// ss << state::bat_percentage << "% " << status << "Hello there";
	// this->content = ss.str();
	std::cout << this->content << std::endl;

	pango_layout_set_text(this->pango_layout.get(), this->content.c_str(), this->content.size());

	pango_layout_get_size(this->pango_layout.get(), &w, &h);

	return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, -1, 1);
}

void BatteryComponent::render(cairo_t *painter, const Monitor &mon) const
{
	const auto width = cairo_image_surface_get_width(cairo_get_target(painter));
	const auto hieght = cairo_image_surface_get_height(cairo_get_target(painter));
	const auto clr_schm = colors[&mon == state::selmon];

	pango_cairo_update_layout(painter, this->pango_layout.get());

	setColor(painter, clr_schm.cmpBg);
	cairo_rectangle(painter, 0, 0, width, hieght);
	cairo_fill(painter);

	setColor(painter, clr_schm.text);
	cairo_move_to(painter, paddingX, paddingY);
	pango_cairo_show_layout(painter, this->pango_layout.get());
}

/*****************************************************************************************/
/*-----------------------------------BrightnessComponent---------------------------------*/
/*****************************************************************************************/
BrightnessComponent::BrightnessComponent(size_t idx) noexcept
	:pango_layout {wl_unique_ptr<PangoLayout> {pango_layout_new(state::pango_ctx.get())}}, 
	 idx { idx }
{
	pango_layout_set_font_description(this->pango_layout.get(), state::barfont.description);
}

std::tuple<int, int, int> BrightnessComponent::dim(const Monitor &mon) 
{
	std::stringstream ss;
	int w, h;
	ss << "Brightness: " << (int)state::brightnesses[this->idx] << "%";
	this->content = ss.str();

	pango_layout_set_text(this->pango_layout.get(), this->content.c_str(), this->content.size());

	pango_layout_get_size(this->pango_layout.get(), &w, &h);

	return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, -1, 1);
}



void BrightnessComponent::render(cairo_t *painter, const Monitor &mon) const 
{
	const auto width = cairo_image_surface_get_width(cairo_get_target(painter));
	const auto hieght = cairo_image_surface_get_height(cairo_get_target(painter));
	const auto clr_schm = colors[&mon == state::selmon];

	pango_cairo_update_layout(painter, this->pango_layout.get());

	setColor(painter, clr_schm.cmpBg);
	cairo_rectangle(painter, 0, 0, width, hieght);
	cairo_fill(painter);

	setColor(painter, clr_schm.text);
	cairo_move_to(painter, paddingX, paddingY);
	pango_cairo_show_layout(painter, this->pango_layout.get());
}

/*****************************************************************************************/
/*--------------------------------------TextComponent------------------------------------*/
/*****************************************************************************************/
TextComponent::TextComponent() {}

TextComponent::TextComponent(int align)
	  : _align {align} ,
		_pangoLayout {wl_unique_ptr<PangoLayout> {pango_layout_new(state::pango_ctx.get())}} 
{
	pango_layout_set_font_description(_pangoLayout.get(), state::barfont.description);
}	

/* h==-1 => full height */
std::tuple<int, int, int> TextComponent::dim(const Monitor &_mon) 
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

void TextComponent::render(cairo_t *painter, const Monitor &mon) const
{
	const auto width = cairo_image_surface_get_width(cairo_get_target(painter));
	const auto hieght = cairo_image_surface_get_height(cairo_get_target(painter));
	const auto clr_schm = colors[&mon == state::selmon];

	pango_cairo_update_layout(painter, _pangoLayout.get());

	setColor(painter, clr_schm.cmpBg);
	cairo_rectangle(painter, 0, 0, width, hieght);
	cairo_fill(painter);

	setColor(painter, clr_schm.text);
	cairo_move_to(painter, paddingX, paddingY);
	pango_cairo_show_layout(painter, _pangoLayout.get());
}
