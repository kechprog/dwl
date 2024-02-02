#pragma once
#include "src/common.hpp"
#include "src/config.hpp"
#include "src/State.hpp"
#include <pango/pangocairo.h>
#include <iostream>
#include <cairo.h>
#include <sys/types.h>
#include <tuple>
#include <pango/pango-layout.h>

class IBarComponent {
public:
	// TODO: this should be probably renamed to 'setup'
	// align should probably be in template
	/*
	 * width, height, align(0-left, 1-right)
	 * h==-1 => full height 
	 */
	virtual std::tuple<int, int> dim(const Monitor &) = 0;
	virtual void render(cairo_t *, const Monitor &) const = 0;
};


/*****************************************************************************************/
/*--------------------------------------TextComponent------------------------------------*/
/*****************************************************************************************/
class TextComponent : public IBarComponent {
public:
	TextComponent() noexcept;
	void render(cairo_t *painter, const Monitor &mon) const override;
protected:
	wl_unique_ptr<PangoLayout> pango_layout;
};


/*****************************************************************************************/
/*------------------------------------BatteryComponent-----------------------------------*/
/*****************************************************************************************/
template<int align>
class BatteryComponent : public TextComponent{
public:
	BatteryComponent() noexcept : TextComponent() {}
	std::tuple<int, int> dim(const Monitor &mon) override
	{
		const auto status = state::bat_is_charging ? "charging" : "discharging";
		std::stringstream ss;
		int w, h;

		ss << status << " " << (int)state::bat_percentage << "%";
		this->content = ss.str();

		pango_layout_set_text(this->pango_layout.get(), this->content.c_str(), this->content.size());

		pango_layout_get_size(this->pango_layout.get(), &w, &h);

		return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, align);
	}
private:
	std::string content;
};


/*****************************************************************************************/
/*-----------------------------------BrightnessComponent---------------------------------*/
/*****************************************************************************************/
template<int align>
class BrightnessComponent : public TextComponent {
public:
	BrightnessComponent(size_t idx) : TextComponent(), idx { idx } {}
	std::tuple<int, int> dim(const Monitor &mon) override
	{
		std::stringstream ss;
		int w, h;
		ss << "Brightness: " << (int)state::brightnesses[this->idx] << "%";
		this->content = ss.str();

		pango_layout_set_text(this->pango_layout.get(), this->content.c_str(), this->content.size());

		pango_layout_get_size(this->pango_layout.get(), &w, &h);

		return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, align);
	}
private:
	std::string content;
	size_t idx;
};


/*****************************************************************************************/
/*--------------------------------------TimeComponent------------------------------------*/
/*****************************************************************************************/
template<int align>
class TimeComponent : public TextComponent {
public:
	TimeComponent() : TextComponent() {};
	std::tuple<int, int> dim(const Monitor &) override
	{
		int w, h;
		pango_layout_set_text(this->pango_layout.get(), state::time_txt.c_str(), state::time_txt.size());
		pango_layout_get_size(this->pango_layout.get(), &w, &h);

		return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, align);
	}
};


/*****************************************************************************************/
/*--------------------------------------LayoutComponent----------------------------------*/
/*****************************************************************************************/
template<int align>
class LayoutComponent : public TextComponent {
public:
	LayoutComponent() : TextComponent() {};
	std::tuple<int, int> dim(const Monitor &) override
	{
		int w;
		const auto &lt_name = state::layout_names[state::selmon->layout_idx];
		pango_layout_set_text(this->pango_layout.get(), lt_name.c_str(), lt_name.size());
		pango_layout_get_size(this->pango_layout.get(), &w, nullptr);

		return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, align);
	}
};

/*****************************************************************************************/
/*--------------------------------------TimeComponent------------------------------------*/
/*****************************************************************************************/
template<int align>
class TitleComponent : public TextComponent {
public:
	TitleComponent() : TextComponent() {};
	std::tuple<int, int> dim(const Monitor &mon) override
	{
		int w, h;
		pango_layout_set_text(this->pango_layout.get(), mon.title.c_str(), mon.title.size());
		pango_layout_get_size(this->pango_layout.get(), &w, &h);

		return std::make_tuple(PANGO_PIXELS(w) + 2*paddingX, align);
	}
};

template<int align>
class TagsComponent : public IBarComponent {
public:
	std::tuple<int, int> dim(const Monitor &) override
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

		return std::make_tuple(this->w, align);
	}

	void render(cairo_t *painter, const Monitor &mon) const override
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
					setColor(painter, clr_schm.cmp_bg);
				break;
				case 1: // selected
					setColor(painter, clr_schm.tag_selected);
				break;
				case 2: // urgent
					setColor(painter, clr_schm.tag_urgent);
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
private:
	std::vector<wl_unique_ptr<PangoLayout>> pango_layouts;
	int w; // since we assume tags do not change after startup
};
