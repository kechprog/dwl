#include "globals.h"


const int layermap[] = { LyrBg, LyrBottom, LyrTop, LyrOverlay };

pid_t child_pid = -1;
int locked;
void *exclusive_focus;
struct wl_display *dpy;
struct wlr_backend *backend;
struct wlr_scene *scene;
struct wlr_scene_tree *layers[NUM_LAYERS];
struct wlr_scene_tree *drag_icon;

/* Map from ZWLR_LAYER_SHELL_* constants to Lyr* enum */
struct wlr_renderer *drw;
struct wlr_allocator *alloc;
struct wlr_compositor *compositor;
struct wlr_session *session;

struct wlr_xdg_shell *xdg_shell;
struct wlr_xdg_activation_v1 *activation;
struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
struct wl_list clients; /* tiling order */
struct wl_list fstack;  /* focus order */
struct wlr_idle_notifier_v1 *idle_notifier;
struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
struct wlr_layer_shell_v1 *layer_shell;
struct wlr_output_manager_v1 *output_mgr;
struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;

struct wlr_cursor *cursor;
struct wlr_xcursor_manager *cursor_mgr;

struct wlr_session_lock_manager_v1 *session_lock_mgr;
struct wlr_scene_rect *locked_bg;
struct wlr_session_lock_v1 *cur_lock;

struct wlr_pointer_gestures_v1 *gestures;
struct wlr_tablet_manager_v2 *tabletmanager;

struct wlr_seat *seat;
struct wl_list keyboards;
unsigned int cursor_mode;
Client *grabc;
int grabcx, grabcy; /* client-relative */

struct wlr_output_layout *output_layout;
struct wlr_box sgeom;
struct wl_list mons;
Monitor *selmon = NULL;

double swipe_dx=0, swipe_dy=0;
uint32_t swipe_fingercount=0;

struct wl_list touches;
struct wl_list tablets;

int sel_kbd_lt = 0;

#ifdef XWAYLAND
xcb_atom_t netatom[NetLast];
struct wlr_xwayland *xwayland;
#endif
