#include "Popup.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <wayland-client-protocol.h>

static const struct xdg_popup_listener popup_listener = {
    .configure = [](void *data, struct xdg_popup *xdg_popup, int32_t x,
		int32_t y, int32_t width, int32_t height) 
	{
		static_cast<Popup*>(data)->configure_popup(x, y, width, height);
	},



	.popup_done = [](void *data, struct xdg_popup *xdg_popup) { },
	.repositioned = [](void *data, struct xdg_popup *xdg_popup, uint32_t token) { },
};

static const struct xdg_surface_listener surface_listener = {
	.configure = [](void *data, struct xdg_surface *xdg_surface, uint32_t serial) 
	{
		static_cast<Popup*>(data)->configure_surface(serial);
	}
};

void Popup::configure_popup(int32_t _x, int32_t _y, int32_t width, int32_t height)
{
	_bufs.emplace(width, height, WL_SHM_FORMAT_XRGB8888);

	if (_configured)
		this->render();
}

void Popup::configure_surface(uint32_t serial)
{
	xdg_surface_ack_configure(_xdg_surface.get(), serial);
	_configured = true;
	if (_bufs)
		this->render();
}

void Popup::render() 
{
	for (size_t i = 0; i < _bufs->height * _bufs->width; i++) {
		_bufs->data()[i * 4 + 0] = bg.r;
		_bufs->data()[i * 4 + 1] = bg.g;
		_bufs->data()[i * 4 + 2] = bg.b;
		_bufs->data()[i * 4 + 3] = bg.a;
	}

	wl_surface_attach(_wl_surface.get(), _bufs->buffer(), 0, 0);
	wl_surface_damage(_wl_surface.get(), 0, 0, _bufs->width, _bufs->height);
	wl_surface_commit(_wl_surface.get());
	_bufs->flip();
}

Popup::Popup(int x, int y, int w, int h, Color bg, zwlr_layer_surface_v1 *parent)
	: bg { bg }
{
	wl_unique_ptr<xdg_positioner> positioner = wl_unique_ptr<xdg_positioner> { xdg_wm_base_create_positioner(xdgWmBase) };
	xdg_positioner_set_anchor_rect(positioner.get(), x, y, w, h);
	xdg_positioner_set_size(positioner.get(), w, h);
	xdg_positioner_set_anchor(positioner.get(), XDG_POSITIONER_ANCHOR_TOP_LEFT);
	xdg_positioner_set_gravity(positioner.get(), XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);

	this->_wl_surface.reset(wl_compositor_create_surface(compositor));
	this->_xdg_surface.reset(
		xdg_wm_base_get_xdg_surface(xdgWmBase, _wl_surface.get())
	);

	this->_xdg_popup.reset(
		xdg_surface_get_popup(_xdg_surface.get(), nullptr, positioner.get())
	);

	zwlr_layer_surface_v1_get_popup(parent, _xdg_popup.get());
	xdg_popup_set_user_data(_xdg_popup.get(), this);

	xdg_popup_add_listener(_xdg_popup.get(), &popup_listener, this);
	xdg_surface_add_listener(_xdg_surface.get(), &surface_listener, this);

	wl_surface_commit(_wl_surface.get());
}
