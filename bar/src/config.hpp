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
	/*---------|--------Bar Bg------|-------Text0------|-------Cmp Bg--------|-------Tag Sel-------|-------Tag Urg---------|----Which---*/
	ColorScheme {{91, 96, 120, 255}, {36, 39, 58, 255}, {145, 215, 227, 255}, {238, 212, 159, 255}, {238, 153, 160, 255} }, /* inactive */
	ColorScheme {{30, 32, 48 , 255}, {36, 39, 58, 255}, {139, 213, 202, 255}, {245, 169, 127, 255}, {237, 135, 150, 255} }, /*  active  */
};

constexpr const char* termcmd[]     = {"foot", nullptr};

const std::pair<std::filesystem::path, size_t> display_configs[] = {
	/*           current brightness(file)             , max brightness */
	{ "/sys/class/backlight/amdgpu_bl0/brightness",          255       },
	// {"/sys/class/leds/asus::screenpad/brightness",            255}
};

const size_t display_configs_len = sizeof(display_configs) / sizeof(display_configs[0]);


constexpr Button buttons[] = {
	{ ClkTagBar,       BTN_LEFT,   view,       {0} },
	{ ClkTagBar,       BTN_RIGHT,  tag,        {0} },
	{ ClkTagBar,       BTN_MIDDLE, toggletag,  {0} },
	{ ClkLayoutSymbol, BTN_LEFT,   setlayout,  {.ui = 0} },
	{ ClkLayoutSymbol, BTN_RIGHT,  setlayout,  {.ui = 2} },
	{ ClkStatusText,   BTN_RIGHT,  spawn,      {.v = termcmd} },
};
