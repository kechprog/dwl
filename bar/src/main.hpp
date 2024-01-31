#pragma once
#include "src/bar.hpp"
#include <cstdint>
#include <string>
#include <wayland-client-protocol.h>

struct Monitor {
	uint32_t                 registryName;
	std::string              xdgName;
	wl_unique_ptr<wl_output> wlOutput;
	Bar 					 bar;
	bool 				   	 desiredVisibility {true};
	bool 					 hasData;
	uint32_t 				 sel_tags;
	wl_unique_ptr<znet_tapesoftware_dwl_wm_monitor_v1> dwlMonitor;
	
	std::vector<Tag> tags;

	void set_tag(int tag, int state, int num_clients, int focused_client);
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
