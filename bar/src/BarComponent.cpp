#include "BarComponent.hpp"
#include "cairo.h"
#include "src/common.hpp"

static inline void setColor(cairo_t *p, Color c)
{

	const auto [r, g, b, a] = c;
	cairo_set_source_rgba(p,
		r / 255.0, g / 255.0, b / 255.0, a / 255.0);
}

void BarComponent::render(wl_unique_ptr<cairo_surface_t> sf) const
{
	auto surface = sf.get();
	wl_unique_ptr<cairo_t> painter = wl_unique_ptr<cairo_t> { cairo_create(sf.get()) };

	setColor(painter.get(), bg);
	cairo_rectangle(painter, 0, 0, sf.get(), )
	cairo_fill(painter);

	setColor(painter.get(), fg);
	cairo_move_to(painter, component.x+paddingX, paddingY);
	pango_cairo_show_layout(painter, component.pangoLayout.get());
}
