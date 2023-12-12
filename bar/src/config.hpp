// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include "common.hpp"
#include <string_view>

using namespace std::literals;

constexpr bool topbar = true;

constexpr int paddingX = 10;
constexpr int paddingY = 3;

// See https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html
constexpr const char* font = "Sans 12";

const constexpr ColorScheme colors[2] = {
	ColorScheme {.barBg = 0x6e738d, .text = 0x24273a, .cmpBg = 0xcad3f5}, /* inactive */
	ColorScheme {.barBg = 0x24273a, .text = 0x181926, .cmpBg = 0xc6a0f6}, /*  active  */
};

constexpr const char*  termcmd[]     = {"foot", nullptr};
constexpr const char*  batChargeNow  = "/sys/class/power_supply/BAT0/energy_now";
constexpr const size_t batChargeFull = 62732000;

constexpr const std::pair<std::string_view, size_t> displayConfigs[] = {
	/*           current brightness(file)             , max brightness */
	{"/sys/class/backlight/intel_backlight/brightness",     19200       },
	{"/sys/class/leds/asus::screenpad/brightness",            255}
};


constexpr Button buttons[] = {
	{ ClkTagBar,       BTN_LEFT,   view,       {0} },
	{ ClkTagBar,       BTN_RIGHT,  tag,        {0} },
	{ ClkTagBar,       BTN_MIDDLE, toggletag,  {0} },
	{ ClkLayoutSymbol, BTN_LEFT,   setlayout,  {.ui = 0} },
	{ ClkLayoutSymbol, BTN_RIGHT,  setlayout,  {.ui = 2} },
	{ ClkStatusText,   BTN_RIGHT,  spawn,      {.v = termcmd} },
};
