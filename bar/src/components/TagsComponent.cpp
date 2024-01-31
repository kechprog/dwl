#include "BarComponent.hpp"
#include "../State.hpp"
#include "cairo.h"
#include "pango/pangocairo.h"

static inline void setColor(cairo_t *p, Color c)
{

	const auto [r, g, b, a] = c;
	cairo_set_source_rgba(p,
		r / 255.0, g / 255.0, b / 255.0, a / 255.0);
}

std::tuple<int, int, int> TagsComponent::dim(const Monitor &mon)
{
	if (this->pango_layouts.size() == 0) // since we need to wait for all tags to appear
	{
		int w = 0;
		for (size_t i = 0; i < state::tag_names.size(); i++)
		{
			this->pango_layouts.push_back(wl_unique_ptr<PangoLayout>{pango_layout_new(state::pango_ctx.get())});
			pango_layout_set_font_description(this->pango_layouts[i].get(), state::barfont.description);
			pango_layout_set_text(this->pango_layouts[i].get(), state::tag_names[i].c_str(), state::tag_names[i].size());
			w += PANGO_PIXELS(pango_layout_get_width(this->pango_layouts[i].get())) + 3*paddingX;
		}
		this->w = w;
	}

	return {this->w, -1, 0};
}

void TagsComponent::render(cairo_t *painter, const Monitor &mon) const
{
	size_t x = 0;
	const size_t hieght = cairo_image_surface_get_height(cairo_get_target(painter));
	for (size_t i = 0; i < state::tag_names.size(); i++)
	{
		const auto clr_schm = colors[&mon == state::selmon];
		const size_t width = PANGO_PIXELS(pango_layout_get_width(this->pango_layouts[i].get())) + 3*paddingX;
		switch (mon.tags[i].state)
		{
			case 0: // normal
				setColor(painter, clr_schm.cmpBg);
			break;
			case 1: // selected
				setColor(painter, {0, 255, 0, 255}); // TODO: make this configurable
			break;
			case 2: // urgent
				setColor(painter, {255, 0, 0, 255}); // TODO: make this configurable
			break;
		}

		pango_cairo_update_layout(painter, this->pango_layouts[i].get());
		cairo_rectangle(painter, x, 0, width, hieght);
		cairo_fill(painter);

		setColor(painter, clr_schm.text);
		cairo_move_to(painter, x + paddingX, paddingY);
		pango_cairo_show_layout(painter, this->pango_layouts[i].get());
		x += width;
	}
}
