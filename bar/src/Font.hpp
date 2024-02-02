#pragma once
#include <pango/pango.h>

struct Font {
	PangoFontDescription* description;
	int height {0};

	static Font get_font();
};
