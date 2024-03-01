#include "Popup.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <wayland-client-protocol.h>

static const struct xdg_popup_listener popup_listener = {
    .configure = [](void *data, struct xdg_popup *xdg_popup, int32_t x,
		int32_t y, int32_t width, int32_t height) 
	{
		static_cast<Popup*>(data)->configure(x, y, width, height);
	},



	.popup_done = [](void *data, struct xdg_popup *xdg_popup) { },
	.repositioned = [](void *data, struct xdg_popup *xdg_popup, uint32_t token) { },
};

void Popup::configure(int32_t _x, int32_t _y, int32_t width, int32_t height)
{
	/* needed in a xdg_surface only */
	// xdg_popup_ack_configure(this->_xdg_popup.get(), width, height);
}

Popup::Popup(int x, int y, int w, int h, Color bg, zwlr_layer_surface_v1 *parent)
	: bg { bg }
{
	struct xdg_positioner *positioner = xdg_wm_base_create_positioner(xdgWmBase);
	xdg_positioner_set_anchor_rect(positioner, 0, 0, 100, 100);

	this->_wl_surface.reset(wl_compositor_create_surface(compositor));
	this->_xdg_surface.reset(
		xdg_wm_base_get_xdg_surface(xdgWmBase, this->_wl_surface.get())
	);

	this->_xdg_popup.reset(
		xdg_surface_get_popup(this->_xdg_surface.get(), nullptr, positioner)
	);

	zwlr_layer_surface_v1_get_popup(parent, this->_xdg_popup.get());
	xdg_popup_set_user_data(this->_xdg_popup.get(), this);

	xdg_popup_add_listener(this->_xdg_popup.get(), &popup_listener, this);
	// xdg_surface_add_listener(struct xdg_surface *xdg_surface, const struct xdg_surface_listener *listener, void *data)

	wl_surface_commit(this->_wl_surface.get());
}
