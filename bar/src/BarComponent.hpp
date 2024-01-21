#pragma once
#include "src/common.hpp"
#include <sys/types.h>

class BarComponent {
	std::string _text;
	int _align;
	wl_unique_ptr<PangoLayout> _pangoLayout;

public:
	BarComponent();
	explicit BarComponent(wl_unique_ptr<PangoLayout> layout, int align);

	void setCol(Color bg, Color fg);
	void setText(std::string text);
	void render(cairo_t *painter) const;

	/*
	 * width, height, align(0-left, 1-right)
	 * h==-1 => full height 
	 */
	std::tuple<int, int, int> dim() const; 

	Color bg={0, 0, 0, 0}, fg={0, 0, 0, 0};
	int x {0};
};
