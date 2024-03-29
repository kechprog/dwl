#pragma once
#include "src/bar.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <wayland-client-protocol.h>

struct Monitor {
	uint32_t                 registryName;
	std::string              xdg_name;
	wl_unique_ptr<wl_output> wlOutput;
	Bar 					 bar;
	bool 				   	 desiredVisibility {false};
	bool 					 hasData;
	uint32_t 				 sel_tags;
	wl_unique_ptr<znet_tapesoftware_dwl_wm_monitor_v1> dwl_mon;
	
	std::vector<Tag> tags;
	std::string      title;
	uint32_t         layout_idx;

	void set_tag(int tag, int state, int num_clients, int focused_client);

	uint32_t touch_state;
};

struct SeatPointer {
	wl_unique_ptr<wl_pointer> wlPointer;
	Monitor* focusedMonitor;
	int x, y;
	std::vector<int> btns;
};

struct Seat {
	uint32_t name;
	wl_unique_ptr<wl_seat> wlSeat;
	std::optional<SeatPointer> pointer;
};
