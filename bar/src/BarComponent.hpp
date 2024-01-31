#pragma once
#include "src/common.hpp"
#include <sys/types.h>

class IBarComponent {
public:
	virtual std::tuple<int, int, int> dim(Monitor &mon) const = 0;
	virtual void render(cairo_t *painter, Monitor &mon) const = 0;
};

class BatteryComponent : public IBarComponent {};

class TextComponent {
	std::string _text;
	int _align;
	wl_unique_ptr<PangoLayout> _pangoLayout;

public:
	TextComponent();
	explicit TextComponent(int align);

	void setCol(Color bg, Color fg);
	void setText(std::string text);
	void render(cairo_t *painter, Monitor &mon) const;

	/*
	 * width, height, align(0-left, 1-right)
	 * h==-1 => full height 
	 */
	std::tuple<int, int, int> dim(Monitor &mon) const; 

	Color bg={0, 0, 0, 0}, fg={0, 0, 0, 0};
	int x {0};
};
