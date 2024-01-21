#pragma once
#include "src/common.hpp"
#include <sys/types.h>

class BarComponent {
	std::string _text;
public:
	BarComponent();
	explicit BarComponent(wl_unique_ptr<PangoLayout> layout);
	void setCol(Color bg, Color fg);
	void setText(std::string text);

	void render(wl_unique_ptr<cairo_surface_t> sf) const;

	wl_unique_ptr<PangoLayout> pangoLayout;
	std::pair<int, int> dim(); /* h==-1 => full height */
	Color bg={0, 0, 0, 0}, fg={0, 0, 0, 0};
	int x {0};
	int align {0}; /* 0: left, 1: right */
};
