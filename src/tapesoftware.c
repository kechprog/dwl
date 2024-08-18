#include <stdlib.h>
#include <wayland-server-core.h>
#include "globals.h"
#include "dwl.h"
#include "tapesoftware.h"
#include "config.h"
#include "client.h"
#include "net-tapesoftware-dwl-wm-unstable-v1-protocol.h"

static void
dwl_wm_monitor_handle_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
dwl_wm_monitor_handle_destroy(struct wl_resource *resource)
{
	DwlWmMonitor *mon = wl_resource_get_user_data(resource);
	if (mon) {
		wl_list_remove(&mon->link);
		free(mon);
	}
}

static void
dwl_wm_printstatus_to(Monitor *m, const DwlWmMonitor *mon)
{
	Client *c, *focused;
	int tagmask, state, numclients, focused_client;
	focused = focustop(m);
	znet_tapesoftware_dwl_wm_monitor_v1_send_selected(mon->resource, m == selmon);

	for (int tag = 0; tag<LENGTH(tags); tag++) {
		numclients = state = 0;
		focused_client = -1;
		tagmask = 1 << tag;
		if ((tagmask & m->tagset[m->seltags]) != 0)
			state = state | ZNET_TAPESOFTWARE_DWL_WM_MONITOR_V1_TAG_STATE_ACTIVE;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			if (!(c->tags & tagmask))
				continue;
			if (c == focused)
				focused_client = numclients;
			numclients++;
			if (c->isurgent)
				state = state | ZNET_TAPESOFTWARE_DWL_WM_MONITOR_V1_TAG_STATE_URGENT;
		}
		znet_tapesoftware_dwl_wm_monitor_v1_send_tag(mon->resource,
			tag, state, numclients, focused_client);
	}
	znet_tapesoftware_dwl_wm_monitor_v1_send_layout(mon->resource, m->lt[m->sellt] - layouts);
	znet_tapesoftware_dwl_wm_monitor_v1_send_title(mon->resource,
		focused ? client_get_title(focused) : "");
	znet_tapesoftware_dwl_wm_monitor_v1_send_frame(mon->resource);
	znet_tapesoftware_dwl_wm_monitor_v1_send_touchscreen(mon->resource, !m->touch ? 0 /* not supported */
																				  : m->touch->mode == TOUCH_MODE_ENABLED ? 1 /* enabled */
																				  : 2 /* disabled */);

}

void
dwl_wm_printstatus(Monitor *m)
{
	DwlWmMonitor *mon;
	wl_list_for_each(mon, &m->dwl_wm_monitor_link, link) {
		dwl_wm_printstatus_to(m, mon);
	}
}

static void
dwl_wm_monitor_handle_set_tags(struct wl_client *client, struct wl_resource *resource,
	uint32_t t, uint32_t toggle_tagset)
{
	DwlWmMonitor *mon;
	Monitor *m;
	mon = wl_resource_get_user_data(resource);
	if (!mon)
		return;
	m = mon->monitor;
	if ((t & TAGMASK) == m->tagset[m->seltags])
		return;
	if (toggle_tagset)
		m->seltags ^= 1;
	if (t & TAGMASK)
		m->tagset[m->seltags] = t & TAGMASK;

	focusclient(focustop(m), 1);
	arrange(m);
	printstatus();
}

static void
dwl_wm_monitor_handle_set_layout(struct wl_client *client, struct wl_resource *resource,
	uint32_t layout)
{
	DwlWmMonitor *mon;
	Monitor *m;
	mon = wl_resource_get_user_data(resource);
	if (!mon)
		return;
	m = mon->monitor;
	if (layout >= LENGTH(layouts))
		return;
	if (layout != m->lt[m->sellt] - layouts)
		m->sellt ^= 1;

	m->lt[m->sellt] = &layouts[layout];
	arrange(m);
	printstatus();
}

static void
dwl_wm_monitor_handle_set_client_tags(struct wl_client *client, struct wl_resource *resource,
	uint32_t and, uint32_t xor)
{
	DwlWmMonitor *mon;
	Client *sel;
	unsigned int newtags;
	mon = wl_resource_get_user_data(resource);
	if (!mon)
		return;
	sel = focustop(mon->monitor);
	if (!sel)
		return;
	newtags = (sel->tags & and) ^ xor;
	if (newtags) {
		sel->tags = newtags;
		focusclient(focustop(selmon), 1);
		arrange(selmon);
		printstatus();
	}
}

static const struct znet_tapesoftware_dwl_wm_monitor_v1_interface dwl_wm_monitor_implementation = {
	.release = dwl_wm_monitor_handle_release,
	.set_tags = dwl_wm_monitor_handle_set_tags,
	.set_layout = dwl_wm_monitor_handle_set_layout,
	.set_client_tags = dwl_wm_monitor_handle_set_client_tags,
};

/* dwl_wm_v1 */
static void
dwl_wm_handle_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
dwl_wm_handle_get_monitor(struct wl_client *client, struct wl_resource *resource,
	uint32_t id, struct wl_resource *output)
{
	DwlWmMonitor *dwl_wm_monitor;
	struct wlr_output *wlr_output = wlr_output_from_resource(output);
	struct Monitor *m = wlr_output->data;
	struct wl_resource *dwlOutputResource = wl_resource_create(client,
		&znet_tapesoftware_dwl_wm_monitor_v1_interface, wl_resource_get_version(resource), id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	dwl_wm_monitor = calloc(1, sizeof(DwlWmMonitor));
	dwl_wm_monitor->resource = dwlOutputResource;
	dwl_wm_monitor->monitor = m;
	wl_resource_set_implementation(dwlOutputResource, &dwl_wm_monitor_implementation,
	dwl_wm_monitor, dwl_wm_monitor_handle_destroy);
	wl_list_insert(&m->dwl_wm_monitor_link, &dwl_wm_monitor->link);
	dwl_wm_printstatus_to(m, dwl_wm_monitor);
}

static void
dwl_wm_handle_destroy(struct wl_resource *resource)
{
	/* no state to destroy */
}

static const struct znet_tapesoftware_dwl_wm_v1_interface dwl_wm_implementation = {
	.release = dwl_wm_handle_release,
	.get_monitor = dwl_wm_handle_get_monitor,
};

void
dwl_wm_bind(struct wl_client *client, void *data,
	uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&znet_tapesoftware_dwl_wm_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &dwl_wm_implementation, NULL, dwl_wm_handle_destroy);

	for (int i = 0; i < tagcount; i++)
		znet_tapesoftware_dwl_wm_v1_send_tag(resource, tags[i]);
	for (int i = 0; i < LENGTH(layouts); i++)
		znet_tapesoftware_dwl_wm_v1_send_layout(resource, layouts[i].symbol);
}
