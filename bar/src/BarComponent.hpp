#pragma once
#include "src/common.hpp"
#include "src/config.hpp"
#include "src/State.hpp"
#include "Font.hpp"
#include <cassert>
#include <pango/pangocairo.h>
#include <iostream>
#include <cairo.h>
#include <sys/types.h>
#include <tuple>
#include <pango/pango-layout.h>

template<typename T>
T map_to_index(T *arr, size_t arr_len, uint8_t idx)
{
	assert(idx <= 100); /* uint >= 0 - always */
	if (idx == 0)
		return arr[0];
	if (idx == 100)
		return arr[arr_len - 1];

	const int category_split = 100 / arr_len;
	return arr[idx / category_split];

}

struct CmpColors {
	Color text, bg, border;
};

struct CmpStyle {
	CmpColors colors[2];
	int align;
	int border_px, padding_x, padding_y;

	static const constexpr CmpStyle defualt();
};

class IBarComponent {
public:
	/* width, align(0-left, 1-right) */
	virtual std::tuple<int, int> setup(const Monitor *, int desired_height) = 0;
	virtual void render(cairo_t *, const Monitor *) const = 0;
};


/*****************************************************************************************/
/*--------------------------------------TextComponent------------------------------------*/
/*****************************************************************************************/
// template <int align, int padding_x, int padding_y>
template<CmpStyle style = CmpStyle::defualt()>
class TextComponent : public IBarComponent {
public:
	TextComponent() : pango_layout { wl_unique_ptr<PangoLayout> { pango_layout_new(state::pango_ctx.get()) } }{};
	std::tuple<int, int> setup(const Monitor *mon, int height) override
	{
		this->update_text(mon);
		const auto *font = Font::get_font(height-2*style.padding_y);
		int w;
		pango_layout_set_font_description(this->pango_layout.get(), font->description.get());
		pango_layout_set_text(this->pango_layout.get(), this->content.c_str(), this->content.size());
		pango_layout_get_size(this->pango_layout.get(), &w, nullptr);

		return std::make_tuple(PANGO_PIXELS(w) + 2*style.padding_x, style.align);
	}

	void render(cairo_t *painter, const Monitor *mon) const override
	{
		const auto &clr_schm = style.colors[mon == state::selmon];
		const auto height = cairo_image_surface_get_height(cairo_get_target(painter));
		const auto width = cairo_image_surface_get_width(cairo_get_target(painter));
		
		setColor(painter, clr_schm.bg);
		cairo_rectangle(painter, 0, 0, width, height);
		cairo_fill(painter);

		setColor(painter, clr_schm.text);
		pango_cairo_update_layout(painter, this->pango_layout.get());
		cairo_move_to(painter, style.padding_x, style.padding_y);
		pango_cairo_show_layout(painter, this->pango_layout.get());
	}

	virtual void update_text(const Monitor *mon) = 0;
protected:
	wl_unique_ptr<PangoLayout> pango_layout;
	std::string content; /* to be updated by update_text */
};


/*****************************************************************************************/
/*------------------------------------BatteryComponent-----------------------------------*/
/*****************************************************************************************/
template<CmpStyle style>
class BatteryComponent : public TextComponent<style> {
public:
	BatteryComponent() noexcept : TextComponent<style>() {}
	void update_text(const Monitor *mon) override
	{
		const auto &icons = map_to_index(config::battery::icons, config::battery::icons_len, state::bat_percentage);
		const auto icon = state::bat_is_charging ? icons.first : icons.second;
		std::stringstream ss;

		ss << icon << ": " << (int)state::bat_percentage << "%";
		this->content = ss.str();
	}
};


/*****************************************************************************************/
/*-----------------------------------BrightnessComponent---------------------------------*/
/*****************************************************************************************/
template<CmpStyle style>
class BrightnessComponent : public TextComponent<style> {
public:
	BrightnessComponent(size_t idx) : TextComponent<style>(), idx { idx } {}
	void update_text(const Monitor *mon) override
	{
		std::stringstream ss;
		const auto icon = map_to_index(config::brightness::icons, config::brightness::icons_len, state::brightnesses[this->idx]);
		ss << icon << ": " << (int)state::brightnesses[this->idx] << "%";
		this->content = ss.str();
	}

private:
	size_t idx;
};

/*****************************************************************************************/
/*---------------------------------------VolComponent------------------------------------*/
/*****************************************************************************************/
template<CmpStyle style>
class VolComponent : public TextComponent<style> {
public:
	VolComponent() : TextComponent<style>() {};
	void update_text(const Monitor *) override
	{
		std::stringstream ss;
		if (state::is_mute)
			ss << config::volume::icon_mute;
		else {
			const auto icon = map_to_index(config::volume::icons, config::volume::icons_len, (uint8_t)state::volume);
			ss << icon;
		}
		ss << ": " << state::volume << "%";
		this->content = ss.str();
	}
};

/*****************************************************************************************/
/*--------------------------------------TimeComponent------------------------------------*/
/*****************************************************************************************/
template<CmpStyle style>
class TimeComponent : public TextComponent<style> {
public:
	TimeComponent() : TextComponent<style>() {};
	void update_text(const Monitor *) override
	{
		this->content = state::time_txt;
	}
};


/*****************************************************************************************/
/*--------------------------------------LayoutComponent----------------------------------*/
/*****************************************************************************************/
template <CmpStyle style, bool per_mon>
class LayoutComponent : public TextComponent<style> {
public:
	LayoutComponent() : TextComponent<style>() {};
	void update_text(const Monitor *mon) override
	{
		if constexpr (per_mon)
			this->content = state::layout_names[mon->layout_idx];
		else
			this->content = state::layout_names[state::selmon->layout_idx];
	}
};

/*****************************************************************************************/
/*--------------------------------------TimeComponent------------------------------------*/
/*****************************************************************************************/
template <CmpStyle style, bool per_mon>
class TitleComponent : public TextComponent<style> {
public:
	TitleComponent() : TextComponent<style>() {};
	void update_text(const Monitor *mon) override
	{
		if constexpr (per_mon)
			this->content = mon->title;
		else
			this->content = state::selmon->title;
	}
};

template<int align, int padding_x, int padding_y>
class TagsComponent : public IBarComponent {
public:
	std::tuple<int, int> setup(const Monitor *, int height) override
	{
		const auto *font = Font::get_font(height-2*padding_y);
		if (this->pango_layouts.size() == 0) // since we need to wait for all tags to appear
		{
			int w = 0;
			for (size_t i = 0; i < state::tag_names.size(); i++)
			{
				int cmp_width;
				auto layout = wl_unique_ptr<PangoLayout> { pango_layout_new(state::pango_ctx.get()) };

				pango_layout_set_font_description(layout.get(), font->description.get());
				pango_layout_set_text(layout.get(), state::tag_names[i].c_str(), state::tag_names[i].size());
				pango_layout_get_size(layout.get(), &cmp_width, nullptr);

				this->pango_layouts.push_back(std::move(layout));
				w += PANGO_PIXELS(cmp_width) + 2*padding_x;
			}
			this->w = w;
		}

		return std::make_tuple(this->w, align);
	}

	void render(cairo_t *painter, const Monitor *mon) const override
	{
		size_t x = 0;
		const size_t height = cairo_image_surface_get_height(cairo_get_target(painter));

		for (size_t i = 0; i < state::tag_names.size(); i++)
		{
			const auto &clr_schm = config::appearence::colors[mon == state::selmon];
			int width; 
			pango_layout_get_size(this->pango_layouts[i].get(), &width, nullptr);
			width = PANGO_PIXELS(width) + 2*padding_x;

			switch (mon->tags[i].state)
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
			cairo_rectangle(painter, x, 0, width, height);
			cairo_fill(painter);

			setColor(painter, clr_schm.text);
			cairo_move_to(painter, x + padding_x, padding_y);
			pango_cairo_show_layout(painter, this->pango_layouts[i].get());
			x += width;
		}
		// assert(this->w == x);
	}
private:
	std::vector<wl_unique_ptr<PangoLayout>> pango_layouts;
	int w; // since we assume tags do not change after startup
};



template<int align>
class AllTagsComponent : public IBarComponent {
public:
	AllTagsComponent() = default;
	std::tuple<int, int> setup(const Monitor *mon, int height) override
	{
		if (this->all_tags.size() == 0)
		{
			const auto mon_count = state::monitors.size();
			this->mon_height = height / (double)mon_count;

			for (size_t i = 0; i < state::monitors.size(); i++) {
				this->all_tags.push_back(TagsComponent<0,5,0>()); /* alignment does not matter */
				auto [w, _align] = all_tags[i].setup(mon, mon_height);
				this->w = std::max(this->w, w);
			}

			return std::make_tuple(this->w, align);
		}

		return std::make_tuple(this->w, align);
	}

	void render(cairo_t *painter, const Monitor *mon) const override
	{
		int y = 0, i = 0;
		const auto img = cairo_get_target(painter);
		const auto width = cairo_image_surface_get_width(img);

		for (const auto &mon : state::monitors) {
			auto slice_surface = wl_unique_ptr<cairo_surface_t> 
				{ cairo_image_surface_create(cairo_image_surface_get_format(img), width, this->mon_height) };
			auto slice_painter = wl_unique_ptr<cairo_t> {cairo_create(slice_surface.get())};

			this->all_tags[i].render(slice_painter.get(), &mon);

			cairo_set_source_surface(painter, slice_surface.get(), 0, y);
			cairo_rectangle(painter, 0, y, width, this->mon_height);
			cairo_fill(painter);

			y += this->mon_height;
			i += 1;
		}
	}

private:
	std::vector<TagsComponent<0,5,0>> all_tags;
	int mon_height;
	int w;
};


template<int align>
class HAlignComponent : public IBarComponent {
public:
	HAlignComponent(std::vector<std::unique_ptr<IBarComponent>> components) : components { std::move(components) } {}
	std::tuple<int, int> setup(const Monitor *mon, int height) override
	{
		this->cmp_height = height / (double) this->components.size();
		for (auto &cmp : this->components)
		{
			auto [w, _align] = cmp->setup(mon, this->cmp_height);
			this->max_width = std::max(this->max_width, w);
		}

		return std::make_tuple(this->max_width, align);
	}

	void render(cairo_t *painter, const Monitor *mon) const override
	{
		int y = 0;
		const auto img = cairo_get_target(painter);
		const auto width = cairo_image_surface_get_width(img);
		for (const auto &cmp : this->components)
		{
			auto slice_surface = wl_unique_ptr<cairo_surface_t> 
				{ cairo_image_surface_create(cairo_image_surface_get_format(img), width, this->cmp_height) };
			auto slice_painter = wl_unique_ptr<cairo_t> {cairo_create(slice_surface.get())};
			cmp->render(slice_painter.get(), mon);
			cairo_set_source_surface(painter, slice_surface.get(), 0, y);
			cairo_rectangle(painter, 0, y, width, this->cmp_height);
			cairo_fill(painter);
			y += this->cmp_height;
		}
	}

private:
	std::vector<std::unique_ptr<IBarComponent>> components;
	int cmp_height;
	int max_width;
};
