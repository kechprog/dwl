// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include "common.hpp"
#include <filesystem>

constexpr bool topbar = true;

constexpr int paddingX = 10;
constexpr int paddingY = 3;

// See https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html
constexpr const char* font = "Sans 12";

const constexpr ColorScheme colors[2] = {
	ColorScheme {.barBg = {255, 0, 0, 0}, .text = {0x18, 0x19, 0x26, 255}, .cmpBg = {0xf5, 0xbd, 0xe6, 255} }, /* inactive */
	ColorScheme {.barBg = {255, 0, 0, 0}, .text = {0xca, 0xd3, 0xf5, 255}, .cmpBg = {0xed, 0x87, 0x96, 255} }, /*  active  */
};

constexpr const char* termcmd[]     = {"foot", nullptr};

const std::pair<std::filesystem::path, size_t> displayConfigs[] = {
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
