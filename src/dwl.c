#include <assert.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/util/box.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <xkbcommon/xkbcommon.h>

#include "net-tapesoftware-dwl-wm-unstable-v1-protocol.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "tapesoftware.h"
#include "globals.h"
#include "dwl.h"
#include "client.h"
#include "config.h"
#include "dbus.h"

static const char broken[] = "broken";
static struct wl_listener lock_listener = {.notify = locksession};

/* function implementations */
void
applybounds(Client *c, struct wlr_box *bbox)
{
	/* set minimum possible */
	c->geom.width = MAX(1, c->geom.width);
	c->geom.height = MAX(1, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width + 2 * c->bw <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height + 2 * c->bw <= bbox->y)
		c->geom.y = bbox->y;
}

void
applyrules(Client *c)
{
	/* rule matching */
	const char *appid, *title;
	uint32_t i, newtags = 0;
	const Rule *r;
	Monitor *mon = selmon, *m;

	c->isfloating = client_is_float_type(c);
	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (r = rules; r < END(rules); r++) {
		if ((!r->title || strstr(title, r->title))
				&& (!r->id || strstr(appid, r->id))) {
			c->isfloating = r->isfloating;
			newtags |= r->tags;
			i = 0;
			wl_list_for_each(m, &mons, link)
				if (r->monitor == i++)
					mon = m;
		}
	}
	wlr_scene_node_reparent(&c->scene->node, layers[c->isfloating ? LyrFloat : LyrTile]);
	setmon(c, mon, newtags);
}

void
arrange(Monitor *m)
{
	if (!m)
		die("Should not call arrange with NULL\n");

	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {
			wlr_scene_node_set_enabled(&c->scene->node, VISIBLEON(c, m));
			client_set_suspended(c, !VISIBLEON(c, m));
		}
	}

	wlr_scene_node_set_enabled(&m->fullscreen_bg->node,
			(c = focustop(m)) && c->isfullscreen);

	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));

	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);

	motionnotify(0);
	checkidleinhibitor(NULL);
}

void
arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive)
{
	LayerSurface *layersurface;
	struct wlr_box full_area = m->m;

	wl_list_for_each(layersurface, list, link) {
		struct wlr_layer_surface_v1 *wlr_layer_surface = layersurface->layer_surface;
		struct wlr_layer_surface_v1_state *state = &wlr_layer_surface->current;

		if (exclusive != (state->exclusive_zone > 0))
			continue;

		wlr_scene_layer_surface_v1_configure(layersurface->scene_layer, &full_area, usable_area);
		wlr_scene_node_set_position(&layersurface->popups->node,
				layersurface->scene->node.x, layersurface->scene->node.y);
		layersurface->geom.x = layersurface->scene->node.x;
		layersurface->geom.y = layersurface->scene->node.y;
	}
}

void
arrangelayers(Monitor *m)
{
	int i;
	struct wlr_box usable_area = m->m;
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	LayerSurface *layersurface;
	if (!m->wlr_output->enabled)
		return;

	/* Arrange exclusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (memcmp(&usable_area, &m->w, sizeof(struct wlr_box))) {
		m->w = usable_area;
		arrange(m);
	}

	/* Arrange non-exlusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);

	/* Find topmost keyboard interactive layer, if such a layer exists */
	for (i = 0; i < LENGTH(layers_above_shell); i++) {
		wl_list_for_each_reverse(layersurface,
				&m->layers[layers_above_shell[i]], link) {
			if (!locked && layersurface->layer_surface->current.keyboard_interactive
					&& layersurface->mapped) {
				/* Deactivate the focused client. */
				focusclient(NULL, 0);
				exclusive_focus = layersurface;
				client_notify_enter(layersurface->layer_surface->surface, wlr_seat_get_keyboard(seat));
				return;
			}
		}
	}
}

void
axisnotify(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wlr_pointer_axis_event *event = data;
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	wlr_seat_pointer_notify_axis(
		/*seat=*/ seat,
		/*time_msec=*/ event->time_msec, 
		/*orientation=*/ event->orientation, 
		/*value=*/ event->delta,
		/*value_discrete=*/ event->delta_discrete,
		/*source=*/ event->source,
		event->relative_direction
	);
}

void
buttonpress(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_button_event *event = data;
	struct wlr_keyboard *keyboard;
	uint32_t mods;
	Client *c;
	const Button *b;

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	switch (event->state) {
	case WLR_BUTTON_PRESSED:
		cursor_mode = CurPressed;
		if (locked)
			break;

		/* Change focus if the button was _pressed_ over a client */
		xytonode(cursor->x, cursor->y, NULL, &c, NULL, NULL, NULL);
		if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
			focusclient(c, 1);

		keyboard = wlr_seat_get_keyboard(seat);
		mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
		for (b = buttons; b < END(buttons); b++) {
			if (CLEANMASK(mods) == CLEANMASK(b->mod) &&
					event->button == b->button && b->func) {
				b->func(&b->arg);
				return;
			}
		}
		break;
	case WLR_BUTTON_RELEASED:
		/* If you released any buttons, we exit interactive move/resize mode. */
		/* TODO should reset to the pointer focus's current setcursor */
		if (!locked && cursor_mode != CurNormal && cursor_mode != CurPressed) {
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
			cursor_mode = CurNormal;
			/* Drop the window off on its new monitor */
			selmon = xytomon(cursor->x, cursor->y);
			setmon(grabc, selmon, 0);
			return;
		} else {
			cursor_mode = CurNormal;
		}
		break;
	}
	/* If the event wasn't handled by the compositor, notify the client with
	 * pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(seat,
			event->time_msec, event->button, event->state);
}

void
chvt(const Arg *arg)
{
	wlr_session_change_vt(session, arg->ui);
}

void
checkidleinhibitor(struct wlr_surface *exclude)
{
	int inhibited = 0, unused_lx, unused_ly;
	struct wlr_idle_inhibitor_v1 *inhibitor;
	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
		struct wlr_surface *surface = wlr_surface_get_root_surface(inhibitor->surface);
		struct wlr_scene_tree *tree = surface->data;
		if (exclude != surface && (bypass_surface_visibility || (!tree
				|| wlr_scene_node_coords(&tree->node, &unused_lx, &unused_ly)))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}

void
cleanup(void)
{
	dbus_cleanup();
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif
	wl_display_destroy_clients(dpy);
	if (child_pid > 0) {
		kill(child_pid, SIGTERM);
		waitpid(child_pid, NULL, 0);
	}
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_output_layout_destroy(output_layout);
	wl_display_destroy(dpy);
	/* Destroy after the wayland display (when the monitors are already destroyed)
	   to avoid destroying them with an invalid scene output. */
	wlr_scene_node_destroy(&scene->tree.node);
}

void
cleanupkeyboard(struct wl_listener *listener, void *data)
{
	Keyboard *kb = wl_container_of(listener, kb, destroy);

	wl_event_source_remove(kb->key_repeat_source);
	wl_list_remove(&kb->link);
	wl_list_remove(&kb->modifiers.link);
	wl_list_remove(&kb->key.link);
	wl_list_remove(&kb->destroy.link);
	free(kb);
}

void
cleanupmon(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l, *tmp;
	int i;

	for (i = 0; i <= ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY; i++)
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	wlr_scene_output_destroy(m->scene_output);
	wlr_scene_node_destroy(&m->fullscreen_bg->node);

	closemon(m);
	free(m);
}

void
closemon(Monitor *m)
{
	/* update selmon if needed and
	 * move closed monitor's clients to the focused one */
	Client *c;
	if (wl_list_empty(&mons)) {
		selmon = NULL;
	} else if (m == selmon) {
		int nmons = wl_list_length(&mons), i = 0;
		do /* don't switch to disabled mons */
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);
	}

	wl_list_for_each(c, &clients, link) {
		if (c->isfloating && c->geom.x > m->m.width)
			resize(c, (struct wlr_box){.x = c->geom.x - m->w.width, .y = c->geom.y,
				.width = c->geom.width, .height = c->geom.height}, 0);
		if (c->mon == m)
			setmon(c, selmon, c->tags);
	}
	focusclient(focustop(selmon), 1);
	printstatus();
}

void
commitlayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *layersurface = wl_container_of(listener, layersurface, surface_commit);
	struct wlr_layer_surface_v1 *wlr_layer_surface = layersurface->layer_surface;
	struct wlr_output *wlr_output = wlr_layer_surface->output;
	struct wlr_scene_tree *layer = layers[layermap[wlr_layer_surface->current.layer]];

	/* For some reason this layersurface have no monitor, this can be because
	 * its monitor has just been destroyed */
	if (!wlr_output || !(layersurface->mon = wlr_output->data))
		return;

	if (layer != layersurface->scene->node.parent) {
		wlr_scene_node_reparent(&layersurface->scene->node, layer);
		wlr_scene_node_reparent(&layersurface->popups->node, layer);
		wl_list_remove(&layersurface->link);
		wl_list_insert(&layersurface->mon->layers[wlr_layer_surface->current.layer],
				&layersurface->link);
	}
	if (wlr_layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
		wlr_scene_node_reparent(&layersurface->popups->node, layers[LyrTop]);

	if (wlr_layer_surface->current.committed == 0
			&& layersurface->mapped == wlr_layer_surface->surface->mapped)
		return;
	layersurface->mapped = wlr_layer_surface->surface->mapped;

	arrangelayers(layersurface->mon);
}

void
commitnotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, commit);

	if (client_surface(c)->mapped)
		resize(c, c->geom, (c->isfloating && !c->isfullscreen));

	/* mark a pending resize as completed */
	if (c->resize && c->resize <= c->surface.xdg->current.configure_serial)
		c->resize = 0;
}

void
createdecoration(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *dec = data;
	wlr_xdg_toplevel_decoration_v1_set_mode(dec, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void
createidleinhibitor(struct wl_listener *listener, void *data)
{
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

	checkidleinhibitor(NULL);
}

void
createkeyboard(struct wlr_keyboard *keyboard)
{
	struct xkb_context *context;
	struct xkb_keymap *keymap;
	Keyboard *kb = keyboard->data = ecalloc(1, sizeof(*kb));
	kb->wlr_keyboard = keyboard;

	/* Prepare an XKB keymap and assign it to the keyboard. */
	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	keymap = xkb_keymap_new_from_names(context, &xkb_rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(keyboard, repeat_rate, repeat_delay);

	/* Here we set up listeners for keyboard events. */
	LISTEN(&keyboard->events.modifiers, &kb->modifiers, keypressmod);
	LISTEN(&keyboard->events.key, &kb->key, keypress);
	LISTEN(&keyboard->base.events.destroy, &kb->destroy, cleanupkeyboard);

	wlr_seat_set_keyboard(seat, keyboard);

	kb->key_repeat_source = wl_event_loop_add_timer(
			wl_display_get_event_loop(dpy), keyrepeat, kb);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&keyboards, &kb->link);
}

void
createlayersurface(struct wl_listener *listener, void *data)
{
	struct wlr_layer_surface_v1 *wlr_layer_surface = data;
	LayerSurface *layersurface;
	struct wlr_layer_surface_v1_state old_state;
	struct wlr_scene_tree *l = layers[layermap[wlr_layer_surface->pending.layer]];

	if (!wlr_layer_surface->output)
		wlr_layer_surface->output = selmon ? selmon->wlr_output : NULL;

	if (!wlr_layer_surface->output) {
		wlr_layer_surface_v1_destroy(wlr_layer_surface);
		return;
	}

	wlr_layer_surface->data = layersurface = ecalloc(1, sizeof(LayerSurface));
	layersurface->type = LayerShell;
	LISTEN(&wlr_layer_surface->surface->events.commit,
			&layersurface->surface_commit, commitlayersurfacenotify);
	LISTEN(&wlr_layer_surface->events.destroy, &layersurface->destroy,
			destroylayersurfacenotify);
	LISTEN(&wlr_layer_surface->surface->events.map, &layersurface->map,
			maplayersurfacenotify);
	LISTEN(&wlr_layer_surface->surface->events.unmap, &layersurface->unmap,
			unmaplayersurfacenotify);

	layersurface->layer_surface = wlr_layer_surface;
	layersurface->mon = wlr_layer_surface->output->data;
	layersurface->scene_layer = wlr_scene_layer_surface_v1_create(l, wlr_layer_surface);
	layersurface->scene = layersurface->scene_layer->tree;
	layersurface->popups = wlr_layer_surface->surface->data = wlr_scene_tree_create(l);

	layersurface->scene->node.data = layersurface;

	wl_list_insert(&layersurface->mon->layers[wlr_layer_surface->pending.layer],
			&layersurface->link);

	/* Temporarily set the layer's current state to pending
	 * so that we can easily arrange it
	 */
	old_state = wlr_layer_surface->current;
	wlr_layer_surface->current = wlr_layer_surface->pending;
	layersurface->mapped = 1;
	arrangelayers(layersurface->mon);
	wlr_layer_surface->current = old_state;
}

void
createlocksurface(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	struct wlr_scene_tree *scene_tree = lock_surface->surface->data =
		wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);
	m->lock_surface = lock_surface;

	wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width, m->m.height);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface, destroylocksurface);

	if (m == selmon)
		client_notify_enter(lock_surface->surface, wlr_seat_get_keyboard(seat));
}

void
createmon(struct wl_listener *listener, void *data)
{
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct wlr_output *wlr_output = data;
	struct wlr_output_state wlr_output_state;
	wlr_output_state_init(&wlr_output_state);

	Touch *touch = NULL;
	Monitor *m = wlr_output->data = ecalloc(1, sizeof(*m));
	wl_list_init(&m->dwl_wm_monitor_link);

	m->wlr_output = wlr_output;
	m->touch = NULL;

	wlr_output_init_render(wlr_output, alloc, drw);

	/* Initialize monitor state using configured rules */
	for (size_t i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	m->tagset[0] = m->tagset[1] = 1;

	for (const MonitorRule *r = monrules; r < END(monrules); r++) {
		if (!r->name || strstr(wlr_output->name, r->name)) {
			m->mfact = r->mfact;
			m->nmaster = r->nmaster;
			wlr_output_state_set_scale(&wlr_output_state, r->scale);
			wlr_xcursor_manager_load(cursor_mgr, r->scale);
			m->lt[0] = m->lt[1] = r->lt;
			wlr_output_state_set_transform(&wlr_output_state, r->rr);
			m->m.x = r->x;
			m->m.y = r->y;
			m->brightness_class = r->brightness_class;
			
			/* find related touch device */
			m->touch_name = r->touch_name;
			if (m->touch_name && !wl_list_empty(&touches))
				wl_list_for_each(touch, &touches, link) {
					if (strcmp(m->touch_name, touch->touch_name) != 0)	
						continue;
					touch->m = m;
					m->touch = touch;
					break;
				}
			m->is_main = r->is_main;
		}
	}

	/* The mode is a tuple of (width, height, refresh rate), and each
	 * monitor supports only a specific set of modes. We just pick the
	 * monitor's preferred mode; a more sophisticated compositor would let
	 * the user configure it. */
	wlr_output_state_set_mode(&wlr_output_state, wlr_output_preferred_mode(wlr_output));

	/* Set up event listeners */
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state, requestmonstate);

	wlr_output_state_set_enabled(&wlr_output_state, 1);
	// wlr_output_state_set_adaptive_sync_enabled(&wlr_output_state, 1);

	if (!wlr_output_test_state(wlr_output, &wlr_output_state)) {
		die("Unable to verify state\n");	
	}
	wlr_output_commit_state(wlr_output, &wlr_output_state);
	wlr_output_state_finish(&wlr_output_state);

	wl_list_insert(&mons, &m->link);
	printstatus();

	/* The xdg-protocol specifies:
	 *
	 * If the fullscreened surface is not opaque, the compositor must make
	 * sure that other screen content not part of the same surface tree (made
	 * up of subsurfaces, popups or similarly coupled surfaces) are not
	 * visible below the fullscreened surface.
	 *
	 */
	/* updatemons() will resize and set correct position */
	m->fullscreen_bg = wlr_scene_rect_create(layers[LyrFS], 0, 0, fullscreen_bg);
	wlr_scene_node_set_enabled(&m->fullscreen_bg->node, 0);

	/* Adds this to the output layout in the order it was configured in.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	m->scene_output = wlr_scene_output_create(scene, wlr_output);
	if (m->m.x < 0 || m->m.y < 0)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));
}

void
createnotify(struct wl_listener *listener, void *data)
{
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup,
	 * or when wlr_layer_shell receives a new popup from a layer.
	 * If you want to do something tricky with popups you should check if
	 * its parent is wlr_xdg_shell or wlr_layer_shell */
	struct wlr_xdg_surface *xdg_surface = data;
	Client *c = NULL;
	LayerSurface *l = NULL;

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_box box;
		int type = toplevel_from_wlr_surface(xdg_surface->surface, &c, &l);
		if (!xdg_surface->popup->parent || type < 0)
			return;
		xdg_surface->surface->data = wlr_scene_xdg_surface_create(
				xdg_surface->popup->parent->data, xdg_surface);
		if ((l && !l->mon) || (c && !c->mon))
			return;
		box = type == LayerShell ? l->mon->m : c->mon->w;
		box.x -= (type == LayerShell ? l->geom.x : c->geom.x);
		box.y -= (type == LayerShell ? l->geom.y : c->geom.y);
		wlr_xdg_popup_unconstrain_from_box(xdg_surface->popup, &box);
		return;
	} else if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_NONE)
		return;

	/* Allocate a Client for this surface */
	c = xdg_surface->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = xdg_surface;
	c->bw = borderpx;

	wlr_xdg_toplevel_set_wm_capabilities(xdg_surface->toplevel,
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

	LISTEN(&xdg_surface->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&xdg_surface->surface->events.map, &c->map, mapnotify);
	LISTEN(&xdg_surface->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&xdg_surface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xdg_surface->toplevel->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xdg_surface->toplevel->events.request_fullscreen, &c->fullscreen,
			fullscreennotify);
	LISTEN(&xdg_surface->toplevel->events.request_maximize, &c->maximize,
			maximizenotify);
}

void
createpointer(struct wlr_pointer *pointer)
{
	if (wlr_input_device_is_libinput(&pointer->base)) {
		struct libinput_device *libinput_device = (struct libinput_device*)
			wlr_libinput_get_device_handle(&pointer->base);

		if (libinput_device_config_tap_get_finger_count(libinput_device)) {
			libinput_device_config_tap_set_enabled(libinput_device, tap_to_click);
			libinput_device_config_tap_set_drag_enabled(libinput_device, tap_and_drag);
			libinput_device_config_tap_set_drag_lock_enabled(libinput_device, drag_lock);
			libinput_device_config_tap_set_button_map(libinput_device, button_map);
		}

		if (libinput_device_config_scroll_has_natural_scroll(libinput_device))
			libinput_device_config_scroll_set_natural_scroll_enabled(libinput_device, natural_scrolling);

		if (libinput_device_config_dwt_is_available(libinput_device))
			libinput_device_config_dwt_set_enabled(libinput_device, disable_while_typing);

		if (libinput_device_config_left_handed_is_available(libinput_device))
			libinput_device_config_left_handed_set(libinput_device, left_handed);

		if (libinput_device_config_middle_emulation_is_available(libinput_device))
			libinput_device_config_middle_emulation_set_enabled(libinput_device, middle_button_emulation);

		if (libinput_device_config_scroll_get_methods(libinput_device) != LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
			libinput_device_config_scroll_set_method (libinput_device, scroll_method);

		if (libinput_device_config_click_get_methods(libinput_device) != LIBINPUT_CONFIG_CLICK_METHOD_NONE)
			libinput_device_config_click_set_method (libinput_device, click_method);

		if (libinput_device_config_send_events_get_modes(libinput_device))
			libinput_device_config_send_events_set_mode(libinput_device, send_events_mode);

		if (libinput_device_config_accel_is_available(libinput_device)) {
			libinput_device_config_accel_set_profile(libinput_device, accel_profile);
			libinput_device_config_accel_set_speed(libinput_device, accel_speed);
		}
	}

	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void
createtouch(struct wlr_touch *touch) 
{
	printf("Creating touch with name: %s\n",touch->base.name);
	const char *name = touch->base.name;
	Monitor *m = NULL;
	Touch   *t = ecalloc(1, sizeof(Touch));

	wl_list_init(&t->link);

	t->touch = touch;
	t->touch_name = ecalloc(strlen(name), sizeof(char));
	strcpy(t->touch_name, name);
	t->mode = TOUCH_MODE_ENABLED;
	t->prev_mode = TOUCH_MODE_ENABLED;
	t->last_touch = clock();
	wl_list_init(&t->track_points);

	/* events */
	t->touch_down.notify   = touch_down;
	t->touch_up.notify     = touch_up;
	t->touch_motion.notify = touch_motion;
	t->touch_frame.notify  = touch_frame;
	t->touch_cancel.notify = touch_cancel;

	wl_signal_add(&touch->events.cancel,   &t->touch_cancel);
	wl_signal_add(&touch->events.frame,    &t->touch_frame);
	wl_signal_add(&touch->events.motion,   &t->touch_motion);
	wl_signal_add(&touch->events.up,       &t->touch_up);
	wl_signal_add(&touch->events.down,     &t->touch_down);
	wlr_cursor_attach_input_device(cursor, &touch->base);

	wl_list_insert(&touches, &t->link);

	if (wl_list_empty(&mons))
		return;

	wl_list_for_each(m, &mons, link) {
		if(strcmp(t->touch_name, m->touch_name) == 0) {
			t->m = m;
			m->touch = t;
			break;
		}
	}	
}

void
createtablet(struct wlr_tablet *tablet)
{
	Tablet *tab = ecalloc(1, sizeof(Tablet));
	tab->aspect_ratio = -1;
	
	tab->tablet_tool_button    .notify = tabletbutton;
	tab->tablet_tool_proximity .notify = tabletproximity;
	tab->tablet_tool_axis      .notify = tabletaxis;
	tab->tablet_tool_tip       .notify = tablettip;

	wl_signal_add(&tablet->events.axis,      &tab->tablet_tool_axis);
	wl_signal_add(&tablet->events.proximity, &tab->tablet_tool_proximity);
	wl_signal_add(&tablet->events.button,    &tab->tablet_tool_button);
	wl_signal_add(&tablet->events.tip,       &tab->tablet_tool_tip);

	tab->tablet = tablet;
	tab->tabletv2 = wlr_tablet_create(tabletmanager, seat, &tab->tablet->base);
	wl_list_init(&tab->tools);
	wl_list_insert(&tablets, &tab->link);
}

Tool*
createtool(struct wlr_tablet_tool *tool, struct wlr_tablet_v2_tablet_tool *toolv2)
{
	Tool *t = ecalloc(1, sizeof(Tool));
	t->tool = tool;
	t->toolv2 = toolv2;
	t->tip_up = true; /* when tool is approaching the tablet it is above it, when it touches we get event */
	wl_list_init(&t->link);	

	return t;
}

void 
toolsfocus(struct wlr_surface *s) /* focuses all tools onto surface */
{
	Tablet *tab;
	Tool *t;
	wl_list_for_each(tab, &tablets, link) {
		wl_list_for_each(t, &tab->tools, link) {
			/* clear focus */
			if (!s || !wlr_surface_accepts_tablet_v2(s, tab->tabletv2)) {
				wlr_tablet_v2_tablet_tool_notify_proximity_out(t->toolv2);
				continue;
			}

			wlr_tablet_v2_tablet_tool_notify_proximity_in(t->toolv2, tab->tabletv2, s);
			t->tip_up ? wlr_tablet_v2_tablet_tool_notify_up  (t->toolv2)
					  : wlr_tablet_v2_tablet_tool_notify_down(t->toolv2);
		}
	}
}

void
cursorframe(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat);
}

void
destroydragicon(struct wl_listener *listener, void *data)
{
	/* Focus enter isn't sent during drag, so refocus the focused node. */
	focusclient(focustop(selmon), 1);
	motionnotify(0);
}

void
destroyidleinhibitor(struct wl_listener *listener, void *data)
{
	/* `data` is the wlr_surface of the idle inhibitor being destroyed,
	 * at this point the idle inhibitor is still in the list of the manager */
	checkidleinhibitor(wlr_surface_get_root_surface(data));
}

void
destroylayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *layersurface = wl_container_of(listener, layersurface, destroy);

	wl_list_remove(&layersurface->link);
	wl_list_remove(&layersurface->destroy.link);
	wl_list_remove(&layersurface->map.link);
	wl_list_remove(&layersurface->unmap.link);
	wl_list_remove(&layersurface->surface_commit.link);
	wlr_scene_node_destroy(&layersurface->scene->node);
	free(layersurface);
}

void
destroylock(SessionLock *lock, int unlock)
{
	wlr_seat_keyboard_notify_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	wlr_scene_node_set_enabled(&locked_bg->node, 0);

	focusclient(focustop(selmon), 0);
	motionnotify(0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

void
destroylocksurface(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface, *lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != seat->keyboard_state.focused_surface)
		return;

	if (locked && cur_lock && !wl_list_empty(&cur_lock->surfaces)) {
		surface = wl_container_of(cur_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface, wlr_seat_get_keyboard(seat));
	} else if (!locked) {
		focusclient(focustop(selmon), 1);
	} else {
		wlr_seat_keyboard_clear_focus(seat);
	};
}

void
destroynotify(struct wl_listener *listener, void *data)
{

	/* Called when the xdg_toplevel is destroyed. */
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else 
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}

	free(c);
}

void
destroysessionlock(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
}

void
destroysessionmgr(struct wl_listener *listener, void *data)
{
	wl_list_remove(&lock_listener.link);
	wl_list_remove(&listener->link);
}

Monitor *
dirtomon(enum wlr_direction dir)
{
	struct wlr_output *next;
	if (!wlr_output_layout_get(output_layout, selmon->wlr_output))
		return selmon;
	if ((next = wlr_output_layout_adjacent_output(output_layout,
			dir, selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	if ((next = wlr_output_layout_farthest_output(output_layout,
			dir ^ (WLR_DIRECTION_LEFT|WLR_DIRECTION_RIGHT),
			selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	return selmon;
}

void
focusclient(Client *c, int lift)
{
	struct wlr_surface *old = seat->keyboard_state.focused_surface;
	int unused_lx, unused_ly, old_client_type;
	Client *old_c = NULL;
	LayerSurface *old_l = NULL;

	if (locked)
		return;

	/* Raise client in stacking order if requested */
	if (c && lift)
		wlr_scene_node_raise_to_top(&c->scene->node);

	if (c && client_surface(c) == old)
		return;

	if ((old_client_type = toplevel_from_wlr_surface(old, &old_c, &old_l)) == XDGShell) {
		struct wlr_xdg_popup *popup, *tmp;
		wl_list_for_each_safe(popup, tmp, &old_c->surface.xdg->popups, link)
			wlr_xdg_popup_destroy(popup);
	}

	/* Put the new client atop the focus stack and select its monitor */
	if (c && !client_is_unmanaged(c)) {
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);
		selmon = c->mon;
		c->isurgent = 0;
		client_restack_surface(c);

		/* Don't change border color if there is an exclusive focus or we are
		 * handling a drag operation */
		if (!exclusive_focus && !seat->drag)
			client_set_border_color(c, focuscolor);
	}

	/* Deactivate old client if focus is changing */
	if (old && (!c || client_surface(c) != old)) {
		/* If an overlay is focused, don't focus or activate the client,
		 * but only update its position in fstack to render its border with focuscolor
		 * and focus it after the overlay is closed. */
		if (old_client_type == LayerShell && wlr_scene_node_coords(
					&old_l->scene->node, &unused_lx, &unused_ly)
				&& old_l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
			return;
		} else if (old_c && old_c == exclusive_focus && client_wants_focus(old_c)) {
			return;
		/* Don't deactivate old client if the new one wants focus, as this causes issues with winecfg
		 * and probably other clients */
		} else if (old_c && !client_is_unmanaged(old_c) && (!c || !client_wants_focus(c))) {
			client_set_border_color(old_c, bordercolor);

			client_activate_surface(old, 0);
		}
	}
	printstatus();

	if (!c) {
		/* With no client, all we have left is to clear focus */
		toolsfocus(NULL);
		wlr_seat_keyboard_notify_clear_focus(seat);
		return;
	} 

	/* Change cursor surface */
	motionnotify(0);

	/* Have a client, so focus its top-level wlr_surface */
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

	/* Activate the new client */
	client_activate_surface(client_surface(c), 1);

	/* focus tools if needed */
	toolsfocus(client_surface(c));
}

void
focusmon(const Arg *arg)
{
	int i = 0, nmons = wl_list_length(&mons);
	if (nmons)
		do /* don't switch to disabled mons */
			selmon = dirtomon(arg->i);
		while (!selmon->wlr_output->enabled && i++ < nmons);
	focusclient(focustop(selmon), 1);
}

void
focusstack(const Arg *arg)
{
	/* Focus the next or previous client (in tiling order) on selmon */
	Client *c, *sel = focustop(selmon);
	if (!sel || sel->isfullscreen)
		return;
	if (arg->i > 0) {
		wl_list_for_each(c, &sel->link, link) {
			if (&c->link == &clients)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon))
				break; /* found it */
		}
	} else {
		wl_list_for_each_reverse(c, &sel->link, link) {
			if (&c->link == &clients)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon))
				break; /* found it */
		}
	}
	/* If only one client is visible on selmon, then c == sel */
	focusclient(c, 1);
}

/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client *
focustop(Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &fstack, flink)
		if (VISIBLEON(c, m))
			return c;
	return NULL;
}

void movenext(const Arg *arg)
{
	Client *c, *cur = focustop(selmon);
	Monitor *m;

	wl_list_for_each(c, &clients, link) {
		m = wl_container_of(c->mon->link.next == &mons
		 ? c->mon->link.next->next
		 : c->mon->link.next,
		 m, link);

		if (VISIBLEON(c, c->mon))
			setmon(c, m, 0);
	}

	if (!cur)
		return;

	focusclient(cur, 1);

	/* mouse follows */
	wlr_cursor_warp_closest(cursor, NULL,
		cur->geom.x + cur->geom.width  / 2.0,
		cur->geom.y + cur->geom.height / 2.0);
}

void
fullscreennotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, fullscreen);
	setfullscreen(c, client_wants_fullscreen(c));
}

void
handlesig(int signo)
{
	if (signo == SIGCHLD) {
#ifdef XWAYLAND
		siginfo_t in;
		/* wlroots expects to reap the XWayland process itself, so we
		 * use WNOWAIT to keep the child waitable until we know it's not
		 * XWayland.
		 */
		while (!waitid(P_ALL, 0, &in, WEXITED|WNOHANG|WNOWAIT) && in.si_pid
				&& (!xwayland || in.si_pid != xwayland->server->pid))
			waitpid(in.si_pid, NULL, 0);
#else
		while (waitpid(-1, NULL, WNOHANG) > 0);
#endif
	} else if (signo == SIGINT || signo == SIGTERM) {
		quit(NULL);
	}
}


void
holdbegin(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_hold_begin_event *ev = data; 
	wlr_pointer_gestures_v1_send_hold_begin(gestures, seat, ev->time_msec, ev->fingers);
}

void
holdend(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_hold_end_event *ev = data; 
	wlr_pointer_gestures_v1_send_hold_end(gestures, seat, ev->time_msec, ev->cancelled);
}

void
incnmaster(const Arg *arg)
{
	if (!arg || !selmon)
		return;
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

void
inputdevice(struct wl_listener *listener, void *data)
{
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		createtouch(wlr_touch_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		createtablet(wlr_tablet_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TABLET:
		createtablet(wlr_tablet_from_input_device(device));
		break;
	default:
		/* TODO: handle other input device types */
		break;
	}


	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In dwl we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	/* TODO: do we actually require a cursor? */
	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	if (!wl_list_empty(&touches))
		caps |= WL_SEAT_CAPABILITY_TOUCH;
	wlr_seat_set_capabilities(seat, caps);
}

int
keybinding(uint32_t mods, uint32_t keycode)
{
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 */
	int handled = 0;
	const Key *k;
	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod) &&
				(keycode == k->keysym) && k->func) {
			k->func(&k->arg);
			handled = 1;
		}
	}
	return handled;
}

void
keypress(struct wl_listener *listener, void *data)
{
	/* This event is raised when a key is pressed or released. */
	Keyboard *kb = wl_container_of(listener, kb, key);
	struct wlr_keyboard_key_event *ev = data;

	int handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(kb->wlr_keyboard);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	/* On _press_ if there is no active screen locker,
	 * attempt to process a compositor keybinding. */

	if (!locked && ev->state == WL_KEYBOARD_KEY_STATE_PRESSED)
		handled = keybinding(mods, ev->keycode) || handled;

	if (handled && kb->wlr_keyboard->repeat_info.delay > 0) {
		kb->mods = mods;
		kb->keycode = ev->keycode;
		wl_event_source_timer_update(kb->key_repeat_source,
				kb->wlr_keyboard->repeat_info.delay);
	} else {
		kb->keycode = 0;
		wl_event_source_timer_update(kb->key_repeat_source, 0);
	}

	if (handled)
		return;

	/* Pass unhandled keycodes along to the client. */
	wlr_seat_set_keyboard(seat, kb->wlr_keyboard);
	wlr_seat_keyboard_notify_key(seat, ev->time_msec,
		ev->keycode, ev->state);
}

void
keypressmod(struct wl_listener *listener, void *data)
{
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	Keyboard *kb = wl_container_of(listener, kb, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(seat, kb->wlr_keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(seat,
		&kb->wlr_keyboard->modifiers);
}

int
keyrepeat(void *data)
{
	Keyboard *kb = data;
	if (!kb->keycode || kb->wlr_keyboard->repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(kb->key_repeat_source,
			1000 / kb->wlr_keyboard->repeat_info.rate);

	keybinding(kb->mods, kb->keycode);

	return 0;
}

void
killclient(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		client_send_close(sel);
}

void
locksession(struct wl_listener *listener, void *data)
{
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	wlr_scene_node_set_enabled(&locked_bg->node, 1);
	if (cur_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	focusclient(NULL, 0);

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	cur_lock = lock->lock = session_lock;
	locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface, createlocksurface);
	LISTEN(&session_lock->events.destroy, &lock->destroy, destroysessionlock);
	LISTEN(&session_lock->events.unlock, &lock->unlock, unlocksession);

	wlr_session_lock_v1_send_locked(session_lock);
}

void
maplayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, map);
	motionnotify(0);
}

void
mapnotify(struct wl_listener *listener, void *data)
{
	/* Called when the surface is mapped, or ready to display on-screen. */
	Client *p, *w, *c = wl_container_of(listener, c, map);
	Monitor *m;
	int i;

	/* Create scene tree for this client and its border */
	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface = c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	/* Handle unmanaged clients first so we can return prior create borders */
	if (client_is_unmanaged(c)) {
		client_get_geometry(c, &c->geom);
		/* Unmanaged clients always are floating */
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
		wlr_scene_node_set_position(&c->scene->node, c->geom.x + borderpx,
			c->geom.y + borderpx);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
		goto unset_fullscreen;
	}

	for (i = 0; i < 4; i++) {
		c->border[i] = wlr_scene_rect_create(c->scene, 0, 0, bordercolor);
		c->border[i]->node.data = c;
	}

	/* Initialize client geometry with room for border */
	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
	client_get_geometry(c, &c->geom);
	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;

	/* Insert this client into client lists. */
	wl_list_insert(&clients, &c->link);
	wl_list_insert(&fstack, &c->flink);

	/* Set initial monitor, tags, floating status, and focus:
	 * we always consider floating, clients that have parent and thus
	 * we set the same tags and monitor than its parent, if not
	 * try to apply rules for them */
	 /* TODO: https://github.com/djpohly/dwl/pull/334#issuecomment-1330166324 */
	if (c->type == XDGShell && (p = client_get_parent(c))) {
		c->isfloating = 1;
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
		setmon(c, p->mon, p->tags);
	} else {
		applyrules(c);
	}
	printstatus();

unset_fullscreen:
	m = c->mon ? c->mon : xytomon(c->geom.x, c->geom.y);
	wl_list_for_each(w, &clients, link)
		if (w != c && w->isfullscreen && m == w->mon && (w->tags & c->tags))
			setfullscreen(w, 0);
}

void
maximizenotify(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply. */
	Client *c = wl_container_of(listener, c, maximize);
	if (wl_resource_get_version(c->surface.xdg->resource)
			< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
		wlr_xdg_surface_schedule_configure(c->surface.xdg);
}

void
monocle(Monitor *m)
{
	Client *c;
	int n = 0;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		resize(c, m->w, 0);
		n++;
	}
	if (n)
		snprintf(m->ltsymbol, LENGTH(m->ltsymbol), "[%d]", n);
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

void
monrotate(const Arg *arg)
{
	if (!selmon)
		return;

	enum wl_output_transform t = (selmon->wlr_output->transform + 1) % 4;
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	wlr_output_state_set_transform(&st, t);
	wlr_output_commit_state(selmon->wlr_output, &st);
	wlr_output_state_finish(&st);
	arrange(selmon);
}


void 
monitorbrightness(const Arg *arg)
{
	char val[3];
	sprintf(val, "%d", abs(arg->i));
	const char *cmd[] = {"light", arg->i > 0 ? "-As" : "-Us", selmon->brightness_class, val, NULL};
	const Arg a = {.v = cmd};
	spawn(&a);
}

void
motionabsolute(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);
	motionnotify(event->time_msec);
}

void
motionnotify(uint32_t time)
{
	double sx = 0, sy = 0;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	int type;
	struct wlr_surface *surface = NULL;

	/* time is 0 in internal calls meant to restore pointer focus. */
	if (time) {
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

		/* Update selmon (even while dragging a window) */
		if (sloppyfocus)
			selmon = xytomon(cursor->x, cursor->y);
	}

	/* Update drag icon's position */
	wlr_scene_node_set_position(&drag_icon->node, cursor->x, cursor->y);

	/* If we are currently grabbing the mouse, handle and return */
	if (cursor_mode == CurMove) {
		/* Move the grabbed client to the new position. */
		resize(grabc, (struct wlr_box){.x = cursor->x - grabcx, .y = cursor->y - grabcy,
			.width = grabc->geom.width, .height = grabc->geom.height}, 1);
		return;
	} else if (cursor_mode == CurResize) {
		resize(grabc, (struct wlr_box){.x = grabc->geom.x, .y = grabc->geom.y,
			.width = cursor->x - grabc->geom.x, .height = cursor->y - grabc->geom.y}, 1);
		return;
	}

	/* Find the client under the pointer and send the event along. */
	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	/* continue selection even if we go out of the window */
	if (cursor_mode == CurPressed && !seat->drag
	&& (type = toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w, &l)) >= 0
	&& seat->pointer_state.focused_surface != surface) 
	{
		/* change sx and sy accordingly */
		sx = cursor->x - (type == LayerShell ? l->geom.x : w->geom.x + w->bw);
		sy = cursor->y - (type == LayerShell ? l->geom.y : w->geom.y + w->bw);
		c = w;
		surface = seat->pointer_state.focused_surface;
	}

	/* If there's no client surface under the cursor, set the cursor image to a
	 * default. This is what makes the cursor image appear when you move it
	 * off of a client or over its border. */
	if (!surface && !seat->drag)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	pointerfocus(c, surface, sx, sy, time);
}

void
motionrelative(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	wlr_cursor_move(cursor, &event->pointer->base, event->delta_x, event->delta_y);
	motionnotify(event->time_msec);
}

void
moveresize(const Arg *arg)
{
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	xytonode(cursor->x, cursor->y, NULL, &grabc, NULL, NULL, NULL);
	if (!grabc || client_is_unmanaged(grabc) || grabc->isfullscreen)
		return;

	/* Float the window and tell motionnotify to grab it */
	setfloating(grabc, 1);
	switch (cursor_mode = arg->ui) {
	case CurMove:
		grabcx = cursor->x - grabc->geom.x;
		grabcy = cursor->y - grabc->geom.y;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "fleur");
		break;
	case CurResize:
		/* Doesn't work for X11 output - the next absolute motion event
		 * returns the cursor to where it started */
		wlr_cursor_warp_closest(cursor, NULL,
				grabc->geom.x + grabc->geom.width,
				grabc->geom.y + grabc->geom.height);
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "se-resize");
		break;
	}
}

void
outputmgrapply(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

void
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test)
{
	/*
	 * Called when a client such as wlr-randr requests a change in output
	 * configuration. This is only one way that the layout can be changed,
	 * so any Monitor information should be updated by updatemons() after an
	 * output_layout.change event, not here.
	 */
	struct wlr_output_configuration_head_v1 *config_head;
	int ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		struct wlr_output_state state;
		wlr_output_state_init(&state);
		Monitor *m = wlr_output->data;

		wlr_output_state_set_enabled(&state, config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;
		if (config_head->state.mode)
			wlr_output_state_set_mode(&state, config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(&state,
					config_head->state.custom_mode.width,
					config_head->state.custom_mode.height,
					config_head->state.custom_mode.refresh);

		/* Don't move monitors if position wouldn't change, this to avoid
		 * wlroots marking the output as manually configured */
		if (m->m.x != config_head->state.x || m->m.y != config_head->state.y)
			wlr_output_layout_add(output_layout, wlr_output,
					config_head->state.x, config_head->state.y);
		wlr_output_state_set_transform(&state, config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(&state,
				config_head->state.adaptive_sync_enabled);

apply_or_test:
		if (test) {
			ok &= wlr_output_test_state(wlr_output, &state);
		} else {
			ok &= wlr_output_commit_state(wlr_output, &state);
		}
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);

	/* TODO: use a wrapper function? */
	updatemons(NULL, NULL);
}

void
outputmgrtest(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}

void
pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
		uint32_t time)
{
	struct timespec now;
	int internal_call = !time;

	if (sloppyfocus && !internal_call && c && !client_is_unmanaged(c))
		focusclient(c, 0);

	/* If surface is NULL, clear pointer focus */
	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(seat);
		return;
	}

	if (internal_call) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}

	/* Let the client know that the mouse cursor has entered one
	 * of its surfaces, and make keyboard focus follow if desired.
	 * wlroots makes this a no-op if surface is already focused */
	wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

void
printstatus(void)
{
	Monitor *m = NULL;
	// Client *c;
	// uint32_t occ, urg, sel;
	// const char *appid, *title;
	//
	wl_list_for_each(m, &mons, link) {
		// occ = urg = 0;
		// wl_list_for_each(c, &clients, link) {
		// 	if (c->mon != m)
		// 		continue;
		// 	occ |= c->tags;
		// 	if (c->isurgent)
		// 		urg |= c->tags;
		// }
		// if ((c = focustop(m))) {
		// 	title = client_get_title(c);
		// 	appid = client_get_appid(c);
		// 	printf("%s title %s\n", m->wlr_output->name, title ? title : broken);
		// 	printf("%s appid %s\n", m->wlr_output->name, appid ? appid : broken);
		// 	printf("%s fullscreen %u\n", m->wlr_output->name, c->isfullscreen);
		// 	printf("%s floating %u\n", m->wlr_output->name, c->isfloating);
		// 	sel = c->tags;
		// } else {
		// 	printf("%s title \n", m->wlr_output->name);
		// 	printf("%s appid \n", m->wlr_output->name);
		// 	printf("%s fullscreen \n", m->wlr_output->name);
		// 	printf("%s floating \n", m->wlr_output->name);
		// 	sel = 0;
		// }
		//
		// printf("%s selmon %u\n", m->wlr_output->name, m == selmon);
		// printf("%s tags %u %u %u %u\n", m->wlr_output->name, occ, m->tagset[m->seltags],
		// 		sel, urg);
		// printf("%s layout %s\n", m->wlr_output->name, m->ltsymbol);
		dwl_wm_printstatus(m);
	}
	// fflush(stdout);
}

void
pinchbegin(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_pinch_begin_event *ev = data;
	wlr_pointer_gestures_v1_send_pinch_begin(
		gestures,
		seat,
		ev->time_msec,
		ev->fingers
	);
}

void
pinchupdate(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_pinch_update_event *ev = data;
	wlr_pointer_gestures_v1_send_pinch_update(
		gestures, 
		seat, 
		ev->time_msec, 
		ev->dx, 
		ev->dy,
		ev->scale,
		ev->rotation
	);
}

void
pinchend(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_pinch_end_event *ev = data;
	wlr_pointer_gestures_v1_send_pinch_end(
		gestures, 
		seat, 
		ev->time_msec, 
		ev->cancelled
	);
}

void
quit(const Arg *arg)
{
	wl_display_terminate(dpy);
}

void
rendermon(struct wl_listener *listener, void *data)
{
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	Monitor *m = wl_container_of(listener, m, frame);
	Client *c;
	struct wlr_output_state pending = {0};
	struct wlr_gamma_control_v1 *gamma_control;
	struct timespec now;

	/* Render if no XDG clients have an outstanding resize and are visible on
	 * this monitor. */
	wl_list_for_each(c, &clients, link)
		if (c->resize && !c->isfloating && client_is_rendered_on_mon(c, m) && !client_is_stopped(c))
			goto skip;

	/*
	 * HACK: The "correct" way to set the gamma is to commit it together with
	 * the rest of the state in one go, but to do that we would need to rewrite
	 * wlr_scene_output_commit() in order to add the gamma to the pending
	 * state before committing, instead try to commit the gamma in one frame,
	 * and commit the rest of the state in the next one (or in the same frame if
	 * the gamma can not be committed).
	 */
	if (m->gamma_lut_changed) {
		gamma_control = wlr_gamma_control_manager_v1_get_control(gamma_control_mgr, m->wlr_output);
		m->gamma_lut_changed = 0;

		if (!wlr_gamma_control_v1_apply(gamma_control, &pending))
			goto commit;

		if (!wlr_output_test_state(m->wlr_output, &pending)) {
			wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
			goto commit;
		}
		wlr_output_commit_state(m->wlr_output, &pending);
		wlr_output_schedule_frame(m->wlr_output);
	} else {
commit:
		wlr_scene_output_commit(m->scene_output, NULL);
	}

skip:
	/* Let clients know a frame has been rendered */
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);
	wlr_output_state_finish(&pending);
}

void
requeststartdrag(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin,
			event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

void
requestmonstate(struct wl_listener *listener, void *data)
{
	struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(event->output, event->state);
	updatemons(NULL, NULL);
}

void
resize(Client *c, struct wlr_box geo, int interact)
{
	struct wlr_box *bbox = interact ? &sgeom : &c->mon->w;
	struct wlr_box clip;
	client_set_bounds(c, geo.width, geo.height);
	c->geom = geo;
	applybounds(c, bbox);

	/* Update scene-graph, including borders */
	wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
	wlr_scene_rect_set_size(c->border[0], c->geom.width, c->bw);
	wlr_scene_rect_set_size(c->border[1], c->geom.width, c->bw);
	wlr_scene_rect_set_size(c->border[2], c->bw, c->geom.height - 2 * c->bw);
	wlr_scene_rect_set_size(c->border[3], c->bw, c->geom.height - 2 * c->bw);
	wlr_scene_node_set_position(&c->border[1]->node, 0, c->geom.height - c->bw);
	wlr_scene_node_set_position(&c->border[2]->node, 0, c->bw);
	wlr_scene_node_set_position(&c->border[3]->node, c->geom.width - c->bw, c->bw);

	/* this is a no-op if size hasn't changed */
	c->resize = client_set_size(c, c->geom.width - 2 * c->bw,
			c->geom.height - 2 * c->bw);
	client_get_clip(c, &clip);
	wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
}

void
run(char *startup_cmd)
{
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the startup command */
	if (startup_cmd) {
		int piperw[2];
		if (pipe(piperw) < 0)
			die("startup: pipe:");
		if ((child_pid = fork()) < 0)
			die("startup: fork:");
		if (child_pid == 0) {
			dup2(piperw[0], STDIN_FILENO);
			close(piperw[0]);
			close(piperw[1]);
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
			die("startup: execl:");
		}
		dup2(piperw[1], STDOUT_FILENO);
		close(piperw[1]);
		close(piperw[0]);
	}
	printstatus();

	/* At this point the outputs are initialized, choose initial selmon based on
	 * cursor position, and set default cursor image */
	selmon = xytomon(cursor->x, cursor->y);

	/* TODO hack to get cursor to display in its initial location (100, 100)
	 * instead of (0, 0) and then jumping. still may not be fully
	 * initialized, as the image/coordinates are not transformed for the
	 * monitor when displayed here */
	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wl_display_run(dpy);
}

void 
cycle_focused_kbd(const Arg* arg)
{
	Keyboard *kbd;
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	int next_lt = (sel_kbd_lt + 1) % LENGTH(kbd_layouts);

	struct xkb_rule_names kbd_rules = xkb_rules;
	kbd_rules.layout = kbd_layouts[next_lt];

	wl_list_for_each(kbd, &keyboards, link) {
		wlr_keyboard_set_keymap(kbd->wlr_keyboard, 
			xkb_keymap_new_from_names(context, &kbd_rules, XKB_KEYMAP_COMPILE_NO_FLAGS));

	}

	printf("current layout: %d|lt name: %s\n", next_lt, kbd_layouts[next_lt]);
	xkb_context_unref(context);
	sel_kbd_lt++;
}

void
setcursor(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	/* If we're "grabbing" the cursor, don't use the client's image, we will
	 * restore it after "grabbing" sending a leave event, followed by a enter
	 * event, which will result in the client requesting set the cursor surface */
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided surface as the cursor image. It will set the
	 * hardware cursor on the output that it's currently on and continue to
	 * do so as the cursor moves between outputs. */
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_surface(cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
}

void
setcursorshape(struct wl_listener *listener, void *data)
{
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided cursor shape. */
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
							   wlr_cursor_shape_v1_name(event->shape));
}

void
setfloating(Client *c, int floating)
{
	c->isfloating = floating;
	wlr_scene_node_reparent(&c->scene->node, layers[c->isfloating ? LyrFloat : LyrTile]);
	arrange(c->mon);
	printstatus();
}

void
setfullscreen(Client *c, int fullscreen)
{
	c->isfullscreen = fullscreen;
	if (!c->mon)
		return;
	c->bw = fullscreen ? 0 : borderpx;
	client_set_fullscreen(c, fullscreen);
	wlr_scene_node_reparent(&c->scene->node, layers[fullscreen
			? LyrFS : c->isfloating ? LyrFloat : LyrTile]);

	if (fullscreen) {
		c->prev = c->geom;
		resize(c, c->mon->m, 0);
	} else {
		/* restore previous size instead of arrange for floating windows since
		 * client positions are set by the user and cannot be recalculated */
		resize(c, c->prev, 0);
	}
	arrange(c->mon);
	printstatus();
}

void
setgamma(struct wl_listener *listener, void *data)
{
	struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
	Monitor *m = event->output->data;
	m->gamma_lut_changed = 1;
	wlr_output_schedule_frame(m->wlr_output);
}

void
setlayout(const Arg *arg)
{
	if (!selmon)
		return;
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, LENGTH(selmon->ltsymbol));
	arrange(selmon);
	printstatus();
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void
setmon(Client *c, Monitor *m, uint32_t newtags)
{
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;
	c->mon = m;
	c->prev = c->geom;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		resize(c, c->geom, 0);
		c->tags = newtags ? newtags : m->tagset[m->seltags]; /* assign tags of target monitor */
		setfullscreen(c, c->isfullscreen); /* This will call arrange(c->mon) */
	}
	focusclient(focustop(selmon), 1);
}

void
setpsel(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in dwl we always honor
	 */
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void
setsel(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in dwl we always honor
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

void
setup(void)
{
	int i, sig[] = {SIGCHLD, SIGINT, SIGTERM, SIGPIPE};
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	wlr_log_init(log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	dpy = wl_display_create();
	struct wl_event_loop *ev_loop = wl_display_get_event_loop(dpy);

	dbus_init(ev_loop);

	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
	if (!(backend = wlr_backend_autocreate(ev_loop, &session)))
		die("couldn't create backend");

	/* Initialize the scene graph used to lay out windows */
	scene = wlr_scene_create();
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	if (!(drw = wlr_renderer_autocreate(backend)))
		die("couldn't create renderer");

	/* Create shm, drm and linux_dmabuf interfaces by ourselves.
	 * The simplest way is call:
	 *      wlr_renderer_init_wl_display(drw);
	 * but we need to create manually the linux_dmabuf interface to integrate it
	 * with wlr_scene. */
	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_texture_formats(drw, WLR_BUFFER_CAP_DMABUF)) {
		wlr_drm_create(dpy, drw);
		wlr_scene_set_linux_dmabuf_v1(scene,
				wlr_linux_dmabuf_v1_create_with_renderer(dpy, 5, drw));
	}


	int drm_fd;
	if ((drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 && drw->features.timeline)
		wlr_linux_drm_syncobj_manager_v1_create(dpy, 1, drm_fd);

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the setsel() function. */
	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_subcompositor_create(dpy);
	wlr_data_device_manager_create(dpy);
	wlr_export_dmabuf_manager_v1_create(dpy);
	wlr_screencopy_manager_v1_create(dpy);
	wlr_data_control_manager_v1_create(dpy);
	wlr_primary_selection_v1_device_manager_create(dpy);
	wlr_viewporter_create(dpy);
	wlr_single_pixel_buffer_manager_v1_create(dpy);
	wlr_fractional_scale_manager_v1_create(dpy, 1);
	wlr_presentation_create(dpy, backend);

	/* Initializes the interface used to implement urgency hints */
	activation = wlr_xdg_activation_v1_create(dpy);
	LISTEN_STATIC(&activation->events.request_activate, urgent);

	gamma_control_mgr = wlr_gamma_control_manager_v1_create(dpy);
	LISTEN_STATIC(&gamma_control_mgr->events.set_gamma, setgamma);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create(dpy);
	LISTEN_STATIC(&output_layout->events.change, updatemons);
	wlr_xdg_output_manager_v1_create(dpy, output_layout);

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&mons);
	LISTEN_STATIC(&backend->events.new_output, createmon);

	/* Set up our client lists, the xdg-shell and the layer-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to the article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&clients);
	wl_list_init(&fstack);

	wl_list_init(&touches);
	wl_list_init(&tablets);

	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	LISTEN_STATIC(&xdg_shell->events.new_surface, createnotify);

	// NOTE: should probably figure out what 4(used to be 3) means
	layer_shell = wlr_layer_shell_v1_create(dpy, 4);
	LISTEN_STATIC(&layer_shell->events.new_surface, createlayersurface);

	idle_notifier = wlr_idle_notifier_v1_create(dpy);

	idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
	LISTEN_STATIC(&idle_inhibit_mgr->events.new_inhibitor, createidleinhibitor);

	session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
	wl_signal_add(&session_lock_mgr->events.new_lock, &lock_listener);
	LISTEN_STATIC(&session_lock_mgr->events.destroy, destroysessionmgr);
	locked_bg = wlr_scene_rect_create(layers[LyrBlock], sgeom.width, sgeom.height,
			(float [4]){0.1, 0.1, 0.1, 1.0});
	wlr_scene_node_set_enabled(&locked_bg->node, 0);

	/* Use decoration protocols to negotiate server-side decorations */
	wlr_server_decoration_manager_set_default_mode(
			wlr_server_decoration_manager_create(dpy),
			WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy);
	LISTEN_STATIC(&xdg_decoration_mgr->events.new_toplevel_decoration, createdecoration);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	gestures = wlr_pointer_gestures_v1_create(dpy);
	tabletmanager = wlr_tablet_v2_create(dpy);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). Scaled cursors will be loaded with each output. */
	cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	setenv("XCURSOR_SIZE", "24", 1);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	LISTEN_STATIC(&cursor->events.motion, motionrelative);
	LISTEN_STATIC(&cursor->events.motion_absolute, motionabsolute);
	LISTEN_STATIC(&cursor->events.button, buttonpress);
	LISTEN_STATIC(&cursor->events.axis, axisnotify);
	LISTEN_STATIC(&cursor->events.frame, cursorframe);

	/* gestures */
	LISTEN_STATIC(&cursor->events.swipe_begin,  swipebegin);
	LISTEN_STATIC(&cursor->events.swipe_update, swipeupdate);
	LISTEN_STATIC(&cursor->events.swipe_end,    swipeend);
	LISTEN_STATIC(&cursor->events.pinch_begin,  pinchbegin);
	LISTEN_STATIC(&cursor->events.pinch_update, pinchupdate);
	LISTEN_STATIC(&cursor->events.pinch_end,    pinchend);
	LISTEN_STATIC(&cursor->events.hold_begin,   holdbegin);
	LISTEN_STATIC(&cursor->events.hold_end,		holdend);

	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	LISTEN_STATIC(&cursor_shape_mgr->events.request_set_shape, setcursorshape);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&keyboards);
	LISTEN_STATIC(&backend->events.new_input, inputdevice);
	virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
	LISTEN_STATIC(&virtual_keyboard_mgr->events.new_virtual_keyboard, virtualkeyboard);
	seat = wlr_seat_create(dpy, "seat0");
	LISTEN_STATIC(&seat->events.request_set_cursor, setcursor);
	LISTEN_STATIC(&seat->events.request_set_selection, setsel);
	LISTEN_STATIC(&seat->events.request_set_primary_selection, setpsel);
	LISTEN_STATIC(&seat->events.request_start_drag, requeststartdrag);
	LISTEN_STATIC(&seat->events.start_drag, startdrag);

	output_mgr = wlr_output_manager_v1_create(dpy);
	LISTEN_STATIC(&output_mgr->events.apply, outputmgrapply);
	LISTEN_STATIC(&output_mgr->events.test, outputmgrtest);

	wl_global_create(dpy, &znet_tapesoftware_dwl_wm_v1_interface, 1, NULL, dwl_wm_bind);
#ifdef XWAYLAND
	/*
	 * Initialise the XWayland X server.
	 * It will be started when the first X client is started.
	 */
	xwayland = wlr_xwayland_create(dpy, compositor, 1);
	if (xwayland) {
		LISTEN_STATIC(&xwayland->events.ready, xwaylandready);
		LISTEN_STATIC(&xwayland->events.new_surface, createnotifyx11);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		fprintf(stderr, "failed to setup XWayland X server, continuing without it\n");
	}
#endif
}

void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwl: execvp %s failed:", ((char **)arg->v)[0]);
	}
}

void
startdrag(struct wl_listener *listener, void *data)
{
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

void
tag(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (!sel || (arg->ui & TAGMASK) == 0)
		return;

	sel->tags = arg->ui & TAGMASK;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void
tagmon(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		setmon(sel, dirtomon(arg->i), 0);
}

void 
tabletaxis(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_axis_event *ev = data;
	Tablet *tab  = wl_container_of(listener, tab, tablet_tool_axis);
	Client *c;
	LayerSurface *l;
	Tool *t;
	bool found = false, no_client;

	wl_list_for_each(t, &tab->tools, link) {
		if (t->tool == ev->tool) {
			found = true;
			break;
		}
	}

	if (!found)
		return;

	no_client = !t->toolv2->focused_surface;

	if (no_client
	&& (c = focustop(selmon)) && wlr_surface_accepts_tablet_v2(client_surface(c), tab->tabletv2))
	{
		no_client = false;	
		toolsfocus(client_surface(c));
	}


    if (!no_client && ev->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE)
        wlr_tablet_v2_tablet_tool_notify_pressure(t->toolv2, ev->pressure);

    if (!no_client && ev->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE)
        wlr_tablet_v2_tablet_tool_notify_distance(t->toolv2, ev->distance);

    if (!no_client && ev->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION)
		wlr_tablet_v2_tablet_tool_notify_rotation(t->toolv2, ev->rotation);

    if (!no_client && ev->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER)
		wlr_tablet_v2_tablet_tool_notify_slider(t->toolv2, ev->slider);

	if (!no_client && ev->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL)
		wlr_tablet_v2_tablet_tool_notify_wheel(t->toolv2, ev->wheel_delta, 0);

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_X)
		t->x = ev->x;

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_Y)
		t->y = ev->y;

	if (ev->updated_axes &
		(WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y))
	{
		double lx, ly, sx = t->x, sy = t->y, client_ar, tablet_ar, r;
		struct wlr_box *surface_box;

		pointtolocal(selmon, t->x, t->y, &lx, &ly);

		if (no_client) {
			wlr_cursor_warp_closest(cursor, NULL, lx, ly);
			motionnotify(ev->time_msec);
			return;
		}

		if (toplevel_from_wlr_surface(t->toolv2->focused_surface, &c, &l) >= 0)
			surface_box = c->type == LayerShell ? &l->geom : &c->geom;
		
		tablet_ar = tab->aspect_ratio;
		bool should_refocus = false;

		/* map 1 to 1 */
		if (tablet_ar <= 0) {
			double wlr_sx, wlr_sy;
			wlr_scene_node_at(&c->scene->node, lx, ly, &wlr_sx, &wlr_sy);

			struct wlr_box s_clip;
			client_get_clip(c, &s_clip);

			sx = lx - surface_box->x - c->bw + s_clip.x;
			sy = ly - surface_box->y - c->bw + s_clip.y;

			should_refocus = t->tip_up && !wlr_box_contains_point(&c->geom, lx, ly);
		} else {
			client_ar = (double)surface_box->width / (double)surface_box->height;

			if (tablet_ar > client_ar) {
				r = client_ar / tablet_ar;
				sx = t->x < r ? t->x : r;
				sx /= r;
			} else if (client_ar > tablet_ar) {
				r = tablet_ar / client_ar;
				sy = t->y < r ? t->y : r;
				sy /= r;
			}

			sx *= surface_box->width;
			sy *= surface_box->height;
			lx = surface_box->x + sx;
			ly = surface_box->y + sy;
		}

		wlr_cursor_warp_closest(cursor, NULL, lx, ly);
		if (should_refocus)
			motionnotify(ev->time_msec);

		wlr_tablet_v2_tablet_tool_notify_motion(t->toolv2, sx, sy);
	}

    if (!no_client && ev->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_X)
        t->tilt_x = ev->tilt_x;

    if (!no_client && ev->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_Y)
        t->tilt_y = ev->tilt_y;

    if (!no_client && ev->updated_axes &
        (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y))
        wlr_tablet_v2_tablet_tool_notify_tilt(t->toolv2, ev->tilt_x, ev->tilt_y);
	
}

void
tabletproximity(struct wl_listener *listener, void *data)
{

	struct wlr_tablet_tool_proximity_event *ev = data;
	Tablet *tab  = wl_container_of(listener, tab, tablet_tool_proximity);
	Client *c = NULL;
	Tool *t;

	if (ev->state == WLR_TABLET_TOOL_PROXIMITY_IN) 
	{
		t = createtool(
			ev->tool, 
			wlr_tablet_tool_create(tabletmanager, seat, ev->tool));
		wl_list_insert(&tab->tools, &t->link);

		if ((c = focustop(selmon)) && wlr_surface_accepts_tablet_v2(client_surface(c), tab->tabletv2))
			wlr_tablet_v2_tablet_tool_notify_proximity_in(t->toolv2, tab->tabletv2, client_surface(c));
			
	}
	else if (ev->state == WLR_TABLET_TOOL_PROXIMITY_OUT)/* OUT */ 
	{
		bool found = false;
		wl_list_for_each(t, &tab->tools, link) {
			if (t->tool == ev->tool) {
				found = true;
				break;
			}
		}	

		if (!found)
			return;

		if (t->toolv2->focused_surface)
			wlr_tablet_v2_tablet_tool_notify_proximity_out(t->toolv2);

		wl_list_remove(&t->link);
		free(t);
	}
}

void
tabletbutton(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_button_event *ev = data;
	Tablet *tab  = wl_container_of(listener, tab, tablet_tool_button);
	Tool *t;
	Client *c;
	bool found = false;

	wl_list_for_each(t, &tab->tools, link) {
		if (t->tool == ev->tool) {
			found = true;
			break;
		}
	}

	if (!found)
		return;

	if (!t->toolv2->focused_surface) {
		if (!(c = focustop(selmon)) && !wlr_surface_accepts_tablet_v2(client_surface(c), tab->tabletv2))
			return;
		
		wlr_tablet_v2_tablet_tool_notify_proximity_in(t->toolv2, tab->tabletv2, client_surface(c));
		if (t->tip_up)
			wlr_tablet_v2_tablet_tool_notify_up(t->toolv2);
		else
			 wlr_tablet_v2_tablet_tool_notify_down(t->toolv2);
	}

	wlr_tablet_v2_tablet_tool_notify_button(t->toolv2, ev->button, ev->state); /* readable code > warnings */
}

void
tablettip(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_tip_event *ev = data;
	Tablet *tab = wl_container_of(listener, tab, tablet_tool_tip);
	Tool *t;
	bool found = false;

	wl_list_for_each(t, &tab->tools, link) {
		if (t->tool == ev->tool) {
			found = true;
			break;
		}
	}

	if (!found)
		return;

	if (ev->state == WLR_TABLET_TOOL_TIP_DOWN)
		t->tip_up = false;
	else if (ev->state == WLR_TABLET_TOOL_TIP_UP)
		t->tip_up = true;

	toolsfocus(client_surface(focustop(selmon)));
}

void
tile(Monitor *m)
{
	unsigned int i, n = 0, nm;
	unsigned int mw, mh, my;
	unsigned int tw, th, ty;
	Client *c;

	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, m) && !c->isfloating && !c->isfullscreen)
			n++;
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? (m->w.width - 2*gappx) * m->mfact : 0;
	else
		mw = m->w.width - 2*gappx;
 
	nm = MIN(n, m->nmaster); /* num masters */

	i = my = ty = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;

		if (i < m->nmaster) { /* master side */
			/* if there is x masters, then there is x+1 gaps */
			mh = ( m->w.height - (nm + 1)*gappx ) / nm;
			struct wlr_box b = {
				.x = m->w.x + gappx,
				.width = mw,
				.y = m->w.y + my + gappx,
				.height = mh,
			};
			resize(c, b, 0);
			my += c->geom.height + gappx;
		} else { /* slaves */
			tw = ( m->w.width - mw - 3*gappx );
			th = ( m->w.height - (n - nm + 1)*gappx ) / (n - nm);
			struct wlr_box b = {
				.x = m->w.x + 2*gappx + mw,
				.width = tw,
				.y = m->w.y + ty + gappx,
				.height = th,
			};
			resize(c, b, 0);
			ty += c->geom.height + gappx;
		}
		i++;
	}
}

void
togglefloating(const Arg *arg)
{
	Client *sel = focustop(selmon);
	/* return if fullscreen */
	if (sel && !sel->isfullscreen)
		setfloating(sel, !sel->isfloating);
}

void
togglefullscreen(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		setfullscreen(sel, !sel->isfullscreen);
}

void
toggletag(const Arg *arg)
{
	uint32_t newtags;
	Client *sel = focustop(selmon);
	if (!sel)
		return;
	newtags = sel->tags ^ (arg->ui & TAGMASK);
	if (!newtags)
		return;

	sel->tags = newtags;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void
toggleview(const Arg *arg)
{
	uint32_t newtagset = selmon ? selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK) : 0;

	if (!newtagset)
		return;

	selmon->tagset[selmon->seltags] = newtagset;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void
toggletouch(const Arg *arg)
{
	const Monitor *m = arg->v ? arg->v : selmon;

	if (!m->touch)
		return;
	if (selmon->touch->mode == TOUCH_MODE_DISABLED)
		selmon->touch->mode = TOUCH_MODE_ENABLED;
	else
		selmon->touch->mode = TOUCH_MODE_DISABLED;
	printstatus();
}

void 
touch_cancel(struct wl_listener *listener, void *data) 
{
	struct wlr_touch_cancel_event *event = data;
	struct wlr_touch_point *p = wlr_seat_touch_get_point(seat, event->touch_id);
	Touch *touch = wl_container_of(listener, touch, touch_cancel);

	if (touch->mode != TOUCH_MODE_ENABLED)
		return;

	wlr_seat_touch_notify_cancel(seat, p->client);
}

void 
touch_frame(struct wl_listener *listener, void *data)
{

	Touch *touch = wl_container_of(listener, touch, touch_frame);
	if (touch->mode != TOUCH_MODE_ENABLED)
		return;

	wlr_seat_touch_notify_frame(seat);
}

void 
touch_down(struct wl_listener *listener, void *data)
{
	Touch *touch = wl_container_of(listener, touch, touch_down);
	struct wlr_touch_down_event *ev = data;

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

 	switch (touch->mode) {
		case TOUCH_MODE_DISABLED:
			return;

		// case SMTrack: {
		//
		// 	if (touch->pending_touches > 2) 
		// 		return;
		//
		// 	touch->pending_touches++;
		// 	TrackPoint *p = ecalloc(1, sizeof(TrackPoint));
		// 	p->touch_id = ev->touch_id;
		// 	p->cx = p->px = p->ix = ev->x;
		// 	p->cy = p->py = p->iy = ev->y;
		// 	wl_list_insert(&touch->track_points, &p->link);
		//
		// 	if (touch->pending_touches == 1) { /* assume that we want a simple click, will be changed later if needed */
		// 		// TODO: a proper support
		//
		// 		// uint64_t time_since_last = (clock() - touch->last_touch);
		// 		// if (touch->action == TATap1 && time_since_last < doubleclicktimems) // HACK: it's in clocks
		// 		// 	touch->action = TADrag;
		// 		// else
		// 			touch->action = TATap1; 
		// 	}
		// 	else  /* it is either  TATap2, TAScroll2 */
		// 		touch->action = TATap2;
		// 	break;
		// } // track

		case TOUCH_MODE_ENABLED: {
			struct wlr_surface *surface;
			Monitor *m = NULL;
			Client *c;
			double lx, ly, sx, sy;

			pointtolocal(touch->m, ev->x, ev->y, &lx, &ly);

			xytonode(lx, ly, &surface, &c, NULL, &sx, &sy);

			/* move cursor to the touch point */
			wlr_cursor_warp_closest(cursor, NULL, lx, ly);

			if (!surface || !c) { /* touched the monitor bg */
				if (!(m = xytomon(lx, ly)) || wl_list_empty(&mons) || !m->wlr_output->enabled)
					die("Something is completely wrong, go pray\n");

				selmon = m;
				focusclient(focustop(selmon), 1);

				return;
			}

			focusclient(c, 1);

			wlr_seat_touch_notify_down(seat, surface, ev->time_msec, ev->touch_id, sx, sy);

			break;
		} // touch

	}
}

void
touch_motion(struct wl_listener *listener, void *data)
{
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	Touch *touch = wl_container_of(listener, touch, touch_motion);
	struct wlr_touch_motion_event *ev = data;

	switch (touch->mode) {
		case TOUCH_MODE_DISABLED:
			return;

		// case SMTrack: {
		// 	bool found = false;	
		// 	TrackPoint *p = NULL;
		//
		// 	wl_list_for_each(p, &touch->track_points, link) {
		// 		if (p->touch_id == ev->touch_id) {
		// 			found = true;
		// 			break;
		// 		}
		// 	}
		//
		// 	if (!found)
		// 		return;
		//
		// 	p->px = p->cx;
		// 	p->py = p->cy;
		// 	p->cx = ev->x;
		// 	p->cy = ev->y;
		//
		// 	double dx   = p->cx - p->px,
		// 	       dy   = p->cy - p->py,
		// 		   dist = sqrt(pow(dx, 2) + pow(dy, 2));
		//
		// 	switch (touch->action) {
		// 		case TATap1:
		// 			if(dist > clickmargin) {
		// 				touch->action = TAMove1;
		// 			}
		// 			break;
		//
		// 		case TAMove1: {
		// 			// NOTE: only moves the cursor, evnts are not forwarded
		// 			wlr_cursor_move(cursor, NULL, dx * sens_x, dy * sens_y);
		// 			motionnotify(ev->time_msec);
		// 			break;
		// 		}
		//
		// 		case TATap2:
		// 			die("TODO 1\n");
		// 		case TAMove2:
		// 			die("TODO 2\n");
		// 		case TADrag:
		// 			die("TODO 3\n");
		// 		case TAPinch:
		// 			die("TODO 4\n");
		// 	}
		//
		// 	
		// 	break;
		// } // track

		case TOUCH_MODE_ENABLED: {
			double lx, ly, sx, sy;
			if(!wlr_seat_touch_get_point(seat, ev->touch_id))
				return;

			pointtolocal(touch->m, ev->x, ev->y, &lx, &ly);
			xytonode(lx, ly, NULL, NULL, NULL, &sx, &sy);

			wlr_seat_touch_notify_motion
				(seat, ev->time_msec, ev->touch_id, sx, sy);
			break;
		} // touch
	}
}

void 
touch_up(struct wl_listener *listener, void *data)
{
	struct wlr_touch_up_event *event = data;
	Touch *touch = wl_container_of(listener, touch, touch_up);
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);


	switch (touch->mode) {
		case TOUCH_MODE_DISABLED:
			return;

		// case SMTrack: {
		// 	touch->pending_touches--;
		// 	switch (touch->action) {
		// 		case TATap1: {
		// 			TrackPoint *p;
		// 			bool found = false;
		//
		// 			wl_list_for_each(p, &touch->track_points, link) {
		// 				if (p->touch_id == event->touch_id) {
		// 					found = true;
		// 					break;
		// 				}	
		// 			}
		// 			if (!found) return;
		//
		// 			struct wlr_pointer_button_event ev = { 
		// 				.pointer   =  NULL,	
		// 				.time_msec = p->time_down,		
		// 				.button    = BTN_LEFT,
		// 				.state     = WLR_BUTTON_PRESSED
		// 			};
		//
		// 			buttonpress(NULL, &ev);
		// 			ev.time_msec = ev.time_msec;
		// 			ev.state     = WLR_BUTTON_PRESSED;
		// 			goto cleanup;
		// 		}
		//
		// 		case TATap2:
		// 			if (touch->pending_touches != 0)
		// 				return;
		// 			click(BTN_RIGHT);
		// 			goto cleanup;
		//
		// 		case TAMove2:
		// 		case TAMove1:
		// 			goto cleanup;
		//
		// 		case TADrag:
		// 			die("Up, not implemented!, drag\n");
		// 		case TAPinch:
		// 			die("Up, not implemented!, pinch\n");
		//
		//
		// 		cleanup:
		// 		touch->pending_touches = 0;
		// 		touch->last_touch = clock();
		// 		TrackPoint *p, *tmp;
		// 		wl_list_for_each_safe(p, tmp, &touch->track_points, link) {
		// 			wl_list_remove(&p->link);
		// 			free(p);
		// 		}
		//
		// 	}
		// 	break;
		// }
						

		case TOUCH_MODE_ENABLED:
			if(!wlr_seat_touch_get_point(seat, event->touch_id))
				return;

			wlr_seat_touch_notify_up(seat, event->time_msec, event->touch_id);
			break;
	}

}


void pointtolocal(Monitor *m, double sx, double sy, double *lx, double *ly) 
{
	double tmp;

	switch (m->wlr_output->transform) {
		case WL_OUTPUT_TRANSFORM_90:
			tmp = sx;
			sx = (1 - sy);
			sy = tmp;
		break;
		case WL_OUTPUT_TRANSFORM_180:
			sx = (1 - sx);
			sy = (1 - sy);
		break;
		case WL_OUTPUT_TRANSFORM_270:
			tmp = sx;
			sx = sy;
			sy = (1 - tmp);
		break;
		default:
			// TODO:
		break;
	}

	double glb_x = sx * m->m.width  + m->m.x,
	       glb_y = sy * m->m.height + m->m.y;

	if(lx) *lx = glb_x;
	if(ly) *ly = glb_y;
}

void
swipebegin(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_swipe_begin_event *ev = data;
	swipe_fingercount = ev->fingers;
	swipe_dx = swipe_dy = 0;

	if (swipe_fingercount <= 2)
		wlr_pointer_gestures_v1_send_swipe_begin(gestures, seat, ev->time_msec, ev->fingers);
}


void
swipeupdate(struct wl_listener *listener, void *data) 
{
	struct wlr_pointer_swipe_update_event *ev = data; 
	if (swipe_fingercount <= 2) {
		wlr_pointer_gestures_v1_send_swipe_update(gestures, seat, ev->time_msec, ev->dx, ev->dy);
		return;
	}
	swipe_dx += ev->dx;
	swipe_dy += ev->dy;
}

void 
swipeend(struct wl_listener *listener, void *data)
{
	float vec_len = 0;
	int swipe_dir = -1;
	float swipe_angle;

	struct wlr_pointer_swipe_end_event *ev = data;
	if (swipe_fingercount <= 2) {
		wlr_pointer_gestures_v1_send_swipe_end(gestures, seat, ev->time_msec, ev->cancelled);
		return;
	}

	if (ev->cancelled)
		return;

	vec_len = sqrt(swipe_dx * swipe_dx + swipe_dy * swipe_dy);
	swipe_dx /= vec_len;
	swipe_dy /= vec_len;

	swipe_angle = acos(swipe_dx);
	if (swipe_angle > PI/2)
		swipe_angle = PI - swipe_angle;

	if (swipe_angle < corner_angle - corner_wigle_angle) { /* meaning that we count this as left/right */
		assert(swipe_dx != 0);
		if (swipe_dx > 0) { /* swipe right */
			swipe_dir = SwipeRight;
		} else {
			swipe_dir = SwipeLeft;
		}
	} else if (swipe_angle > corner_angle + corner_wigle_angle) { /* swipe up/down */
		assert(swipe_dy != 0);
		if (swipe_dy > 0) { /* swipe up */
			swipe_dir = SwipeUp;
		} else {
			swipe_dir = SwipeDown;
		}
	} else { /* swiping to the corner */
		if (swipe_dx > 0 && swipe_dy > 0)     /* quadrant 1 */
			swipe_dir = SwipeUpRight;
		else if(swipe_dx < 0 && swipe_dy > 0) /* quadrant 2 */
			swipe_dir = SwipeUpLeft;
		else if(swipe_dx < 0 && swipe_dy < 0) /* quadrant 3 */
			swipe_dir = SwipeDownLeft;
		else                                  /* quadrant 4 */
			swipe_dir = SwipeDownRight;
	}

	if (swipe_dir == -1)
		die("something went totally wrong in swipeend");


	for (int i = 0; i<LENGTH(swipegestures); i++) {
		Swipe s = swipegestures[i];
		if (s.dir == swipe_dir && s.fingercount == swipe_fingercount) {
			s.callback(&s.arg);
		}
	}

	swipe_fingercount = 0;
	swipe_dx = swipe_dy = 0;
}

void
unlocksession(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}

void
unmaplayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *layersurface = wl_container_of(listener, layersurface, unmap);

	layersurface->mapped = 0;
	wlr_scene_node_set_enabled(&layersurface->scene->node, 0);
	if (layersurface == exclusive_focus)
		exclusive_focus = NULL;
	if (layersurface->layer_surface->output
			&& (layersurface->mon = layersurface->layer_surface->output->data))
		arrangelayers(layersurface->mon);
	if (layersurface->layer_surface->surface ==
			seat->keyboard_state.focused_surface)
		focusclient(focustop(selmon), 1);
	motionnotify(0);
}

void
unmapnotify(struct wl_listener *listener, void *data)
{
	/* Called when the surface is unmapped, and should no longer be shown. */
	Client *c = wl_container_of(listener, c, unmap);
	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	if (client_is_unmanaged(c)) {
		if (c == exclusive_focus)
			exclusive_focus = NULL;
		if (client_surface(c) == seat->keyboard_state.focused_surface)
			focusclient(focustop(selmon), 1);
	} else {
		wl_list_remove(&c->link);
		setmon(c, NULL, 0);
		wl_list_remove(&c->flink);
	}

	wlr_scene_node_destroy(&c->scene->node);
	printstatus();
	motionnotify(0);
}

void
updatemons(struct wl_listener *listener, void *data)
{
	/*
	 * Called whenever the output layout changes: adding or removing a
	 * monitor, changing an output's mode or position, etc. This is where
	 * the change officially happens and we update geometry, window
	 * positions, focus, and the stored configuration in wlroots'
	 * output-manager implementation.
	 */
	struct wlr_output_configuration_v1 *config
			= wlr_output_configuration_v1_create();
	Client *c;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m;

	/* First remove from the layout the disabled monitors */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(config, m->wlr_output);
		config_head->state.enabled = 0;
		/* Remove this output from the layout to avoid cursor enter inside it */
		wlr_output_layout_remove(output_layout, m->wlr_output);
		closemon(m);
		m->m = m->w = (struct wlr_box){0};
	}
	/* Insert outputs that need to */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled
				&& !wlr_output_layout_get(output_layout, m->wlr_output))
			wlr_output_layout_add_auto(output_layout, m->wlr_output);
	}

	/* Now that we update the output layout we can get its box */
	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	/* Make sure the clients are hidden when dwl is locked */
	wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(config, m->wlr_output);

		/* Get the effective monitor geometry to use for surfaces */
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

		wlr_scene_node_set_position(&m->fullscreen_bg->node, m->m.x, m->m.y);
		wlr_scene_rect_set_size(m->fullscreen_bg, m->m.width, m->m.height);

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree = m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
			wlr_session_lock_surface_v1_configure(m->lock_surface, m->m.width, m->m.height);
		}

		/* Calculate the effective monitor geometry to use for clients */
		arrangelayers(m);
		/* Don't move clients to the left output when plugging monitors */
		arrange(m);
		/* make sure fullscreen clients have the right size */
		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0);

		/* Try to re-set the gamma LUT when updating monitors,
		 * it's only really needed when enabling a disabled output, but meh. */
		m->gamma_lut_changed = 1;

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;

		if (!selmon) {
			selmon = m;
		}
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped)
				setmon(c, selmon, c->tags);
		}
		focusclient(focustop(selmon), 1);
		if (selmon->lock_surface) {
			client_notify_enter(selmon->lock_surface->surface,
					wlr_seat_get_keyboard(seat));
			client_activate_surface(selmon->lock_surface->surface, 1);
		}
	}

	/* FIXME: figure out why the cursor image is at 0,0 after turning all
	 * the monitors on.
	 * Move the cursor image where it used to be. It does not generate a
	 * wl_pointer.motion event for the clients, it's only the image what it's
	 * at the wrong position after all. */
	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, config);
}

void
updatetitle(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_title);
	if (c == focustop(c->mon))
		printstatus();
}

void
urgent(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);
	if (!c || c == focustop(selmon))
		return;

	c->isurgent = 1;
	printstatus();
}

void
view(const Arg *arg)
{
	if (!selmon || (arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void
viewprev(const Arg *arg) {
	unsigned int  newtagset;

	if (selmon->tagset[selmon->seltags] == 0)
		return;

	newtagset = (selmon->tagset[selmon->seltags] << 1);
	newtagset &= TAGMASK;

	if (newtagset) {
		Arg a = {.ui = newtagset};
		view(&a);
	}
}

void
viewnext(const Arg *arg) {
	unsigned int  newtagset;

	if (selmon->tagset[selmon->seltags] == 0)
		return;

	newtagset = (selmon->tagset[selmon->seltags] >> 1);
	newtagset &= TAGMASK;

	if (newtagset) {
		Arg a = {.ui = newtagset};
		view(&a);
	}
}

void
virtualkeyboard(struct wl_listener *listener, void *data)
{
	struct wlr_virtual_keyboard_v1 *keyboard = data;
	createkeyboard(&keyboard->keyboard);
}

Monitor *
xytomon(double x, double y)
{
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : NULL;
}

void
xytonode(double x, double y, struct wlr_surface **psurface,
		Client **pc, LayerSurface **pl, double *nx, double *ny)
{
	struct wlr_scene_node *node, *pnode;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int layer;
	bool found_any = false;

	for (layer = NUM_LAYERS - 1; !surface && layer >= 0; layer--) {
		if (!(node = wlr_scene_node_at(&layers[layer]->node, x, y, nx, ny)))
			continue;

		if (node->type == WLR_SCENE_NODE_BUFFER)
			surface = wlr_scene_surface_try_from_buffer(
					wlr_scene_buffer_from_node(node))->surface;
		/* Walk the tree to find a node that knows the client */
		for (pnode = node; pnode && !c; pnode = &pnode->parent->node)
			c = pnode->data;

		if (c)
			found_any = true;

		if (c && c->type == LayerShell) {
			c = NULL;
			l = pnode->data;
		}
	}

	if (!found_any)
		*nx = *ny = 0;

	if (psurface) *psurface = surface;
	if (pc) *pc = c;
	if (pl) *pl = l;
}

void
zoom(const Arg *arg)
{
	Client *c, *sel = focustop(selmon);

	if (!sel || !selmon || !selmon->lt[selmon->sellt]->arrange || sel->isfloating)
		return;

	/* Search for the first tiled window that is not sel, marking sel as
	 * NULL if we pass it along the way */
	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, selmon) && !c->isfloating) {
			if (c != sel)
				break;
			sel = NULL;
		}

	/* Return if no other tiled window was found */
	if (&c->link == &clients)
		return;

	/* If we passed sel, move c to the front; otherwise, move sel to the
	 * front */
	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&clients, &sel->link);

	focusclient(sel, 1);
	arrange(selmon);
}

#ifdef XWAYLAND
void
activatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, activate);

	/* Only "managed" windows can be activated */
	if (c->type == X11Managed)
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
}

void
associatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}

void
configurex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	if (!c->mon)
		return;
	if (c->isfloating || c->type == X11Unmanaged)
		resize(c, (struct wlr_box){.x = event->x, .y = event->y,
				.width = event->width, .height = event->height}, 0);
	else
		arrange(c->mon);
}

void
createnotifyx11(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_surface *xsurface = data;
	Client *c;

	/* Allocate a Client for this surface */
	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = xsurface->override_redirect ? X11Unmanaged : X11Managed;
	c->bw = borderpx;

	/* Listen to the various events it can emit */
	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen, fullscreennotify);
}

void
dissociatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
}

xcb_atom_t
getatom(xcb_connection_t *xc, const char *name)
{
	xcb_atom_t atom = 0;
	xcb_intern_atom_reply_t *reply;
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xc, 0, strlen(name), name);
	if ((reply = xcb_intern_atom_reply(xc, cookie, NULL)))
		atom = reply->atom;
	free(reply);

	return atom;
}

void
sethints(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == focustop(selmon))
		return;

	c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);

	if (c->isurgent && surface && surface->mapped)
		client_set_border_color(c, urgentcolor);

	printstatus();
}

void
xwaylandready(struct wl_listener *listener, void *data)
{
	struct wlr_xcursor *xcursor;
	xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
	int err = xcb_connection_has_error(xc);
	if (err) {
		fprintf(stderr, "xcb_connect to X server failed with code %d\n. Continuing with degraded functionality.\n", err);
		return;
	}

	/* Collect atoms we are interested in. If getatom returns 0, we will
	 * not detect that window type. */
	netatom[NetWMWindowTypeDialog] = getatom(xc, "_NET_WM_WINDOW_TYPE_DIALOG");
	netatom[NetWMWindowTypeSplash] = getatom(xc, "_NET_WM_WINDOW_TYPE_SPLASH");
	netatom[NetWMWindowTypeToolbar] = getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLBAR");
	netatom[NetWMWindowTypeUtility] = getatom(xc, "_NET_WM_WINDOW_TYPE_UTILITY");

	/* assign the one and only seat */
	wlr_xwayland_set_seat(xwayland, seat);

	/* Set the default XWayland cursor to match the rest of dwl. */
	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "default", 1)))
		wlr_xwayland_set_cursor(xwayland,
				xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
				xcursor->images[0]->width, xcursor->images[0]->height,
				xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);

	xcb_disconnect(xc);
}
#endif

int
main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	int c;

	while ((c = getopt(argc, argv, "s:hv")) != -1) {
		if (c == 's')
			startup_cmd = optarg;
		else if (c == 'v')
			die("dwl " VERSION);
		else
			goto usage;
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications socket */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd);
	cleanup();
	return EXIT_SUCCESS;

usage:
	die("Usage: %s [-v] [-s startup command]", argv[0]);
}

