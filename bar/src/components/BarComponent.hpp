#pragma once
#include "pango/pango-layout.h"
#include "src/common.hpp"
#include <sys/types.h>

class IBarComponent {
public:
	// TODO: this should be probably renamed to 'setup'
	// align should probably be in template
	/*
	 * width, height, align(0-left, 1-right)
	 * h==-1 => full height 
	 */
	virtual std::tuple<int, int, int> dim(const Monitor &) = 0;
	virtual void render(cairo_t *, const Monitor &) const = 0;
};

class BatteryComponent : public IBarComponent {
public:
	BatteryComponent() noexcept;
	std::tuple<int, int, int> dim(const Monitor &) override;
	void render(cairo_t *painter, const Monitor &) const override;
private:
	wl_unique_ptr<PangoLayout> pango_layout;
	std::string content;
};

class BrightnessComponent : public IBarComponent {
public:
	BrightnessComponent(size_t idx) noexcept;
	std::tuple<int, int, int> dim(const Monitor &) override;
	void render(cairo_t *painter, const Monitor &) const override;
private:
	wl_unique_ptr<PangoLayout> pango_layout;
	std::string content;
	size_t idx;
};

class TagsComponent : public IBarComponent {
public:
	std::tuple<int, int, int> dim(const Monitor &) override;
	void render(cairo_t *painter, const Monitor &) const override;
private:
	std::vector<wl_unique_ptr<PangoLayout>> pango_layouts;
	int w; // since we assume tags do not change after startup
};

class TextComponent : public IBarComponent {
	std::string _text;
	int _align;
	wl_unique_ptr<PangoLayout> _pangoLayout;

public:
	TextComponent();
	explicit TextComponent(int align);

	void setText(std::string text);

	std::tuple<int, int, int> dim(const Monitor &mon) override; 
	void render(cairo_t *painter, const Monitor &mon) const override;
};
