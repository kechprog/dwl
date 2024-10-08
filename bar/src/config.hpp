// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include "common.hpp"
#include "src/dbus_handles.hpp"
#include <filesystem>

namespace config {
	constexpr const char* termcmd[]     = {"foot", nullptr};
	// constexpr Button buttons[] = {
	// 	{ ClkTagBar,       BTN_LEFT,   view,       {0} },
	// 	{ ClkTagBar,       BTN_RIGHT,  tag,        {0} },
	// 	{ ClkTagBar,       BTN_MIDDLE, toggletag,  {0} },
	// 	{ ClkLayoutSymbol, BTN_LEFT,   setlayout,  {.ui = 0} },
	// 	{ ClkLayoutSymbol, BTN_RIGHT,  setlayout,  {.ui = 2} },
	// 	{ ClkStatusText,   BTN_RIGHT,  spawn,      {.v = termcmd} },
	// };


	namespace appearence {
		constexpr bool topbar = true;
		const std::string displays_to_show_on[] {
			std::string("eDP-1"),
			// std::string("DP-1"),
		};
		/* ask ChatGpt if not sure what to put here */
		constexpr const double dpi = 157; 
		/* See https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html */
		constexpr const char* font = "FiraCode Nerd Font Mono";
		constexpr const int bar_size = 25; /* in px, keep it multiple of displays for AllTagsComponent */
		const constexpr ColorScheme colors[2] = {
			/*---------|--------Bar Bg------|--------Text------|-------Cmp Bg--------|-------Tag Sel-------|-------Tag Urg---------|----Which---*/
			ColorScheme {{91, 96, 120, 255}, {36, 39, 58, 255}, {145, 215, 227, 255}, {238, 212, 159, 255}, {238, 153, 160, 255} }, /* inactive */
			ColorScheme {{30, 32, 48 , 255}, {36, 39, 58, 255}, {139, 213, 202, 255}, {245, 169, 127, 255}, {237, 135, 150, 255} }, /*  active  */
		};
	}


	namespace brightness {
		const std::pair<std::filesystem::path, size_t> per_display_info[] = {
			/*                 current brightness(file)           ,  maxBright  */
			/* lenovo thinkpad x1 yoga g6 */
			{ "/sys/class/backlight/intel_backlight/brightness"   ,   38787     },

			/* asus zenbook(us435) */
			// { "/sys/class/backlight/amdgpu_bl0/brightness"     ,     255     },
	
			
			/* asus zenbook duo(ux482) */
			// { "/sys/class/backlight/intel_backlight/brightness",   19200     },
			// { "/sys/class/leds/asus::screenpad/brightness"     ,     255     }
		};
		const size_t display_count = sizeof(per_display_info) / sizeof(per_display_info[0]);
		 
		constexpr const char *icons[]    = {"󰹐", "󱩎", "󱩏", "󱩐", "󱩑", "󱩒", "󱩓", "󱩔", "󱩕", "󱩖", "󰛨"};
		constexpr const size_t icons_len = sizeof(icons) / sizeof(icons[0]); 
	}
	
	namespace battery {
		/* asus zenbook(us435) */
		// constexpr const char* dbus_obj_path = "/org/freedesktop/UPower/devices/battery_BATT";

		/* asus zenbook duo(ux482) || lenovo thinkpad x1 yoga g6 */
		static const auto batteries = std::to_array({
			std::make_pair(std::string("/org/freedesktop/UPower/devices/battery_BAT0"), BatteryType::Regular),
			std::make_pair(std::string("/org/freedesktop/UPower/devices/battery_wacom_battery_"), BatteryType::Pen),
			std::make_pair(std::string("/org/freedesktop/UPower/devices/headset_dev_"), BatteryType::Headphones),
		});

		constexpr static const char* bat_type_icons[] = {
			"", /* BatteryType::Regular */
			" ", /* BatteryType::Pen */
			" ", /* BatteryType::Headphones */
		};

		/* { discharging, charging } */
		constexpr const std::pair<const char*, const char*> icons[] = {
			{"󰢟 ", "󰂃"},
			{"󰢜 ", "󰁺"},
			{"󰂆 ", "󰁻"},
			{"󰂇 ", "󰁼"},
			{"󰂈 ", "󰁽"},
			{"󰢝 ", "󰁾"},
			{"󰂉 ", "󰁿"},
			{"󰢞 ", "󰂀"},
			{"󰂊 ", "󰂁"},
			{"󰂋 ", "󰂂"},
			{"󰂅 ", "󰁹"}
		};
		constexpr const size_t icons_len = sizeof(icons) / sizeof(icons[0]);
	}
	

	namespace volume {
		constexpr const char *mic_icons[] = {"󰍬", "󰍭"};
		constexpr const char *icons[] = {"󰕿", "󰖀", "󰕾"};
		constexpr const char *icon_mute = "󰝟";
		constexpr const size_t icons_len = sizeof(icons) / sizeof(icons[0]);
	}
	
	namespace TouchState {
		constexpr const char *icons[] = {
			"󰟢", /* No touch screen support */
			"󰈷", /* Touch screen is enabled */
			"󰺱", /* Touch screen is disabled */
		};
	}


	namespace display {
	}
}
