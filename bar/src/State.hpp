#include "pango/pango-types.h"
#include "config.hpp"
#include "src/main.hpp"
#include <list>
#include <memory>

struct Font {
	PangoFontDescription* description;
	int height {0};
};

namespace state {
	extern Monitor* selmon;
	extern std::list<Monitor> monitors;
	extern std::vector<std::string> tag_names;
	
	extern std::array<uint8_t, sizeof(display_configs) / sizeof(display_configs[0])> brightnesses;

	extern bool bat_is_charging;
	extern uint8_t bat_percentage;

	extern std::vector<std::unique_ptr<IBarComponent>> components;
	
	/* drawing things */
	extern wl_unique_ptr<PangoContext> pango_ctx;
	extern Font barfont;

	void init(void);

	/* to be called on each state change */
	void render(void);
}
