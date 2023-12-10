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

constexpr ColorScheme  colorInactive = {Color(0xbb, 0xbb, 0xbb), Color(0x22, 0x22, 0x22)};
constexpr ColorScheme  colorActive   = {Color(0xee, 0xee, 0xee), Color(0x00, 0x55, 0x77)};
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
