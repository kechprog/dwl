#include "src/dwl.h"
#include <sys/wait.h>

extern const int layermap[];

extern pid_t child_pid;
extern int locked;
extern void *exclusive_focus;
extern struct wl_display *dpy;
extern struct wlr_backend *backend;
extern struct wlr_scene *scene;
extern struct wlr_scene_tree *layers[NUM_LAYERS];
extern struct wlr_scene_tree *drag_icon;

/* Map from ZWLR_LAYER_SHELL_* constants to Lyr* enum */
extern struct wlr_renderer *drw;
extern struct wlr_allocator *alloc;
extern struct wlr_compositor *compositor;
extern struct wlr_session *session;

extern struct wlr_xdg_shell *xdg_shell;
extern struct wlr_xdg_activation_v1 *activation;
extern struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
extern struct wl_list clients; /* tiling order */
extern struct wl_list fstack;  /* focus order */
extern struct wlr_idle_notifier_v1 *idle_notifier;
extern struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
extern struct wlr_layer_shell_v1 *layer_shell;
extern struct wlr_output_manager_v1 *output_mgr;
extern struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
extern struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
extern struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;

extern struct wlr_cursor *cursor;
extern struct wlr_xcursor_manager *cursor_mgr;

extern struct wlr_session_lock_manager_v1 *session_lock_mgr;
extern struct wlr_scene_rect *locked_bg;
extern struct wlr_session_lock_v1 *cur_lock;

extern struct wlr_pointer_gestures_v1 *gestures;
extern struct wlr_tablet_manager_v2 *tabletmanager;

extern struct wlr_seat *seat;
extern struct wl_list keyboards;
extern unsigned int cursor_mode;
extern Client *grabc;
extern int grabcx, grabcy; /* client-relative */

extern struct wlr_output_layout *output_layout;
extern struct wlr_box sgeom;
extern struct wl_list mons;
extern Monitor *selmon;

extern double swipe_dx, swipe_dy;
extern uint32_t swipe_fingercount;

extern struct wl_list touches;
extern struct wl_list tablets;

extern int sel_kbd_lt;

#ifdef XWAYLAND
extern xcb_atom_t netatom[NetLast];
extern struct wlr_xwayland *xwayland;
#endif
