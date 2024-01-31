#include <memory>
#include <vector>
#include <pango/pangocairo.h>
#include "State.hpp"

std::list<Monitor> state::monitors {0};
Monitor *state::selmon = nullptr;

std::array<uint8_t, sizeof(display_configs) / sizeof(display_configs[0])> state::brightnesses = {0};

bool state::bat_is_charging   = {0};
uint8_t state::bat_percentage = {0};

std::vector<std::unique_ptr<IBarComponent>> state::components;


wl_unique_ptr<PangoContext> state::pango_ctx;

static Font getFont()
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

Font state::barfont;

void state::init() {

	state::pango_ctx.reset(pango_font_map_create_context(pango_cairo_font_map_get_default()));
	if (!state::pango_ctx) {
		die("pango_font_map_create_context");
	}

	state::barfont = getFont();

	/* 
	 * put components here 
	 * order matters
	 */

	/* right aligned */
	state::components.push_back(std::make_unique<BatteryComponent>());
	for (size_t i = 0; i < display_configs_len; i++)
		state::components.push_back(std::make_unique<BrightnessComponent>(i));
}

void state::render() {
	for (auto &mon : monitors) {
		mon.bar.invalidate();
	}
}
