#include <memory>
#include <vector>
#include <pango/pangocairo.h>
#include "State.hpp"
#include "cmp_conf.hpp"

namespace state {
	std::list<Monitor> monitors {0};
	Monitor *selmon = nullptr;
	std::vector<std::string> tag_names;
	std::vector<std::string> layout_names;
	std::array<uint8_t, config::brightness::display_count> brightnesses = {0, 0};
	bool bat_is_charging   = {0};
	uint8_t bat_percentage = {0};
	std::vector<std::unique_ptr<IBarComponent>> components;
	std::string time_txt;
	wl_unique_ptr<PangoContext> pango_ctx;
	uint32_t volume = 0;
	bool is_mute = 0;
}


void state::update_time() 
{
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);

	std::tm localtime = *std::localtime(&now_c);
	std::ostringstream ss;

	ss << std::put_time(&localtime, "%H:%M");
	state::time_txt = ss.str();
}

void state::init() {

	state::pango_ctx.reset(pango_font_map_create_context(pango_cairo_font_map_get_default()));
	if (!state::pango_ctx) {
		die("pango_font_map_create_context");
	}

	state::update_time();

	/* 
	 * put components here,
	 * order matters
	 */

	const auto paddingX = config::appearence::paddingX;
	const auto paddingY = config::appearence::paddingY;

	/* right aligned */
	state::components.push_back(std::make_unique<config::components::Time>());
	state::components.push_back(std::make_unique<config::components::Battery>());
	state::components.push_back(std::make_unique<config::components::Volume>());

	std::vector<std::unique_ptr<IBarComponent>> brightness_components;
	for (size_t i = 0; i < config::brightness::display_count; i++)
		brightness_components.push_back(std::make_unique<config::components::Brightness>(i));
	state::components.push_back(std::make_unique<HAlignComponent<1>>(std::move(brightness_components)));

	/* left aligned */
	// state::components.push_back(std::make_unique<TagsComponent<0>>());
	state::components.push_back(std::make_unique<AllTagsComponent<0>>());

	/* second arg - per-monitor or global */
	state::components.push_back(std::make_unique<config::components::Layout>());
	state::components.push_back(std::make_unique<config::components::Title>());
}

void state::render() {
	for (auto &mon : monitors) {
		mon.bar.invalidate();
	}
}
