#include "BarComponent.hpp"

TextComponent::TextComponent() noexcept
	: pango_layout {wl_unique_ptr<PangoLayout> {pango_layout_new(state::pango_ctx.get())}} 
{
	pango_layout_set_font_description(this->pango_layout.get(), state::barfont.description);
}

void TextComponent::render(cairo_t *painter, const Monitor &mon) const
{
	const auto clr_schm = colors[&mon == state::selmon];
	const auto width = cairo_image_surface_get_width(cairo_get_target(painter));
	const auto hieght = cairo_image_surface_get_height(cairo_get_target(painter));

	if (this->pango_layout.get() == nullptr)
		die("Something is off..");

	pango_cairo_update_layout(painter, this->pango_layout.get());

	setColor(painter, clr_schm.cmp_bg);
	cairo_rectangle(painter, 0, 0, width, hieght);
	cairo_fill(painter);

	setColor(painter, clr_schm.text);
	cairo_move_to(painter, paddingX, paddingY);
	pango_cairo_show_layout(painter, this->pango_layout.get());
}

/*****************************************************************************************/
/*--------------------------------------TextComponent------------------------------------*/
/*****************************************************************************************/
ClassicComponent::ClassicComponent() {}

ClassicComponent::ClassicComponent(int align)
	  : _align {align} ,
		_pangoLayout {wl_unique_ptr<PangoLayout> {pango_layout_new(state::pango_ctx.get())}} 
{
	pango_layout_set_font_description(_pangoLayout.get(), state::barfont.description);
}	

/* h==-1 => full height */
std::tuple<int, int> ClassicComponent::dim(const Monitor &_mon) 
{
	int w, h;
	pango_layout_get_size(_pangoLayout.get(), &w, &h);
	return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX,  _align);
}

void ClassicComponent::setText(std::string text)
{
	_text = std::move(text);
	pango_layout_set_text(_pangoLayout.get(), _text.c_str(), _text.size());
}

void ClassicComponent::render(cairo_t *painter, const Monitor &mon) const
{
	const auto width = cairo_image_surface_get_width(cairo_get_target(painter));
	const auto hieght = cairo_image_surface_get_height(cairo_get_target(painter));
	const auto clr_schm = colors[&mon == state::selmon];

	pango_cairo_update_layout(painter, _pangoLayout.get());

	setColor(painter, clr_schm.cmp_bg);
	cairo_rectangle(painter, 0, 0, width, hieght);
	cairo_fill(painter);

	setColor(painter, clr_schm.text);
	cairo_move_to(painter, paddingX, paddingY);
	pango_cairo_show_layout(painter, _pangoLayout.get());
}
