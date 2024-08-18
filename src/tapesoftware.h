#pragma once
#include <wayland-util.h>

typedef struct {
	struct wl_list link;
	struct wl_resource *resource;
	struct Monitor *monitor;
} DwlWmMonitor;

void dwl_wm_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void dwl_wm_printstatus(struct Monitor *monitor);
