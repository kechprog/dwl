#pragma once

#include "pango/pango-types.h"
#include "config.hpp"
#include "src/main.hpp"
#include <list>
#include <memory>


namespace state {
	extern Monitor* selmon;
	extern std::list<Monitor> monitors;
	extern std::vector<std::string> tag_names;
	extern std::vector<std::string> layout_names;
	
	extern std::array<uint8_t, sizeof(display_configs) / sizeof(display_configs[0])> brightnesses;

	extern bool bat_is_charging;
	extern uint8_t bat_percentage;

	extern std::string time_txt;
	
	extern uint32_t volume;
	extern bool is_mute;
	
	/* drawing things */
	extern wl_unique_ptr<PangoContext> pango_ctx;
	extern std::vector<std::unique_ptr<IBarComponent>> components;

	void init(void);
	void render(void);
	void update_time(void);
}
