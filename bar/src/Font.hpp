#pragma once
#include <list>
#include <pango/pango.h>
#include "src/common.hpp"

struct Font {
	wl_unique_ptr<PangoFontDescription> description;
	int height {0}; /* in pt */

	static Font* get_font(int px);
private:
	static std::list<Font> fonts;
};
