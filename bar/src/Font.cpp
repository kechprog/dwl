#include "Font.hpp"
#include "pango/pangocairo.h"
#include "src/config.hpp"


Font Font::get_font()
{
	auto fontMap = pango_cairo_font_map_get_default();
	auto fontDesc = pango_font_description_from_string(font);
	auto tempContext = pango_font_map_create_context(fontMap);
	auto font = pango_font_map_load_font(fontMap, tempContext, fontDesc);
	auto metrics = pango_font_get_metrics(font, pango_language_get_default());

	auto res = Font {};
	res.description = fontDesc;
	res.height = PANGO_PIXELS(pango_font_metrics_get_height(metrics));

	pango_font_metrics_unref(metrics);
	g_object_unref(font);
	g_object_unref(tempContext);
	return res;
}
