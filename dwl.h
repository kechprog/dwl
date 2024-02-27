#pragma once

#include <getopt.h>
#include <X11/XF86keysym.h>
#include <linux/input-event-codes.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "net-tapesoftware-dwl-wm-unstable-v1-protocol.h"
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#include <X11/Xlib.h>
#include <xcb/xcb_icccm.h>
#endif

#include "util.h"

/* macros */
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define CLEANMASK(mask)         (mask & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M)         ((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define END(A)                  ((A) + LENGTH(A))
#define TAGMASK                 ((1u << tagcount) - 1)
#define LISTEN(E, L, H)         wl_signal_add((E), ((L)->notify = (H), (L)))
#define LISTEN_STATIC(E, H)     do { static struct wl_listener _l = {.notify = (H)}; wl_signal_add((E), &_l); } while (0)
#define PI 3.14159265358979323846

/* enums */
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum { XDGShell, LayerShell, X11Managed, X11Unmanaged }; /* client types */
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrFS, LyrTop, LyrOverlay, LyrBlock, NUM_LAYERS }; /* scene layers */
#ifdef XWAYLAND
enum { NetWMWindowTypeDialog, NetWMWindowTypeSplash, NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility, NetLast }; /* EWMH atoms */
#endif
enum { SwipeUp, SwipeDown, SwipeLeft, SwipeRight,
	   SwipeUpRight, SwipeUpLeft, SwipeDownRight, SwipeDownLeft };

enum {TOUCH_MODE_ENABLED, TOUCH_MODE_DISABLED}; /* screen modes */
enum {TAMove1, TATap1, TATap2, TAMove2, TADrag, TAPinch}; /* trackpad actions */


typedef union {
	int i;
	uint32_t ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* XDGShell or X11* */
	struct wlr_box geom; /* layout-relative, includes border */
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4]; /* top, bottom, left, right */
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
	struct wlr_box prev; /* layout-relative, includes border */
	struct wlr_box bounds;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
#endif
	unsigned int bw;
	uint32_t tags;
	int isfloating, isurgent, isfullscreen;
	uint32_t resize; /* configure serial of a pending resize */
} Client;

typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wl_list link;
	struct wlr_keyboard *wlr_keyboard;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	uint32_t mods; /* invalid if nsyms == 0 */
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} Keyboard;

typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* LayerShell */
	struct wlr_box geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	int mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
} LayerSurface;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct {
	struct wl_list link;
	struct wl_resource *resource;
	struct Monitor *monitor;
} DwlWmMonitor;

typedef struct Touch Touch;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_rect *fullscreen_bg; /* See createmon() for info */
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m; /* monitor area, layout-relative */
	struct wlr_box w; /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface::link */
	const Layout *lt[2];
	unsigned int seltags;
	unsigned int sellt;
	uint32_t tagset[2];
	double mfact;
	int gamma_lut_changed;
	int nmaster;
	char ltsymbol[16];

	struct wl_list dwl_wm_monitor_link;
	char *touch_name; /* null if nothing is asociated, see MonitorRule */
	char *tablet_name;
	char *brightness_class;

	struct Touch *touch;
};

typedef struct {
	const char *name;
	float mfact;
	int nmaster;
	float scale;
	const Layout *lt;
	enum wl_output_transform rr;
	int x, y;

	char *touch_name;
	char *tablet_name;
	char *brightness_class;
} MonitorRule;

typedef struct {
	const char *id;
	const char *title;
	uint32_t tags;
	int isfloating;
	int monitor;
} Rule;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

typedef struct {
	uint32_t fingercount;	
	int dir;
	void (*callback)(const Arg *);
	const Arg arg;
} Swipe;

typedef struct {
	struct wl_list link;
	double ix, iy, px, py, cx, cy;
	uint32_t touch_id;
	uint32_t time_down;
} TrackPoint;

struct Touch {
	struct wl_list link;
	struct wl_listener touch_cancel;
	struct wl_listener touch_motion;
	struct wl_listener touch_frame;
	struct wl_listener touch_up;
	struct wl_listener touch_down;
	struct wlr_touch *touch; 

	Monitor *m;
	char *touch_name;
	int mode;

	/* track mode specific */
	struct wl_list track_points;
	uint32_t pending_touches;
	clock_t last_touch;
	int action;
};

typedef struct {
	struct wl_list link;
	bool on;
	Monitor *m;

	struct wlr_tablet			*tablet;
	struct wlr_tablet_v2_tablet *tablet_v2;
	struct wl_list tools; /* current tools */

	struct wl_listener tablet_tool_axis;
	struct wl_listener tablet_tool_button;   
	struct wl_listener tablet_tool_tip;  
	struct wl_listener tablet_tool_proximity;

} Tablet;

typedef struct {
	struct wl_list link;

	Client *fclient;
	struct wlr_tablet_tool           *tool;
	struct wlr_tablet_v2_tablet_tool *toolv2;
	double x,y;
	double tilt_x, tilt_y;
} Tool;

typedef struct {
	char *name;
} TabletRule;

/* function declarations */
void applybounds(Client *c, struct wlr_box *bbox);
void applyrules(Client *c);
void arrange(Monitor *m);
void arrangelayer(Monitor *m, struct wl_list *list,
 struct wlr_box *usable_area, int exclusive);
void arrangelayers(Monitor *m);
void axisnotify(struct wl_listener *listener, void *data);
void buttonpress(struct wl_listener *listener, void *data);
void chvt(const Arg *arg);
void checkidleinhibitor(struct wlr_surface *exclude);
void cleanup(void);
void cleanupkeyboard(struct wl_listener *listener, void *data);
void cleanupmon(struct wl_listener *listener, void *data);
void closemon(Monitor *m);
void click(int btn);
void commitlayersurfacenotify(struct wl_listener *listener, void *data);
void commitnotify(struct wl_listener *listener, void *data);
void createdecoration(struct wl_listener *listener, void *data);
void createidleinhibitor(struct wl_listener *listener, void *data);
void createkeyboard(struct wlr_keyboard *keyboard);
void createlayersurface(struct wl_listener *listener, void *data);
void createlocksurface(struct wl_listener *listener, void *data);
void createmon(struct wl_listener *listener, void *data);
void createnotify(struct wl_listener *listener, void *data);
void createpointer(struct wlr_pointer *pointer);
void createtouch(struct wlr_touch *touch);
void createtablet(struct wlr_tablet *tablet_tool);
Tool* createtool(struct wlr_tablet_tool *tool, struct wlr_tablet_v2_tablet_tool *toolv2);
void cursorframe(struct wl_listener *listener, void *data);
void destroydragicon(struct wl_listener *listener, void *data);
void destroyidleinhibitor(struct wl_listener *listener, void *data);
void destroylayersurfacenotify(struct wl_listener *listener, void *data);
void destroylock(SessionLock *lock, int unlocked);
void destroylocksurface(struct wl_listener *listener, void *data);
void destroynotify(struct wl_listener *listener, void *data);
void destroysessionlock(struct wl_listener *listener, void *data);
void destroysessionmgr(struct wl_listener *listener, void *data);
Monitor *dirtomon(enum wlr_direction dir);
void focusclient(Client *c, int lift);
void focusmon(const Arg *arg);
void focusstack(const Arg *arg);
Client *focustop(Monitor *m);
void movenext(const Arg *arg);
void fullscreennotify(struct wl_listener *listener, void *data);
void handlesig(int signo);
void holdbegin(struct wl_listener *listener, void *data);
void holdend(struct wl_listener *listener, void *data);
void incnmaster(const Arg *arg);
void inputdevice(struct wl_listener *listener, void *data);
int keybinding(uint32_t mods, xkb_keysym_t sym);
void keypress(struct wl_listener *listener, void *data);
void keypressmod(struct wl_listener *listener, void *data);
int keyrepeat(void *data);
void killclient(const Arg *arg);
void locksession(struct wl_listener *listener, void *data);
void maplayersurfacenotify(struct wl_listener *listener, void *data);
void mapnotify(struct wl_listener *listener, void *data);
void maximizenotify(struct wl_listener *listener, void *data);
void monocle(Monitor *m);
void monitorbrightness(const Arg *arg);
void motionabsolute(struct wl_listener *listener, void *data);
void motionnotify(uint32_t time);
void motionrelative(struct wl_listener *listener, void *data);
void moveresize(const Arg *arg);
void outputmgrapply(struct wl_listener *listener, void *data);
void outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test);
void outputmgrtest(struct wl_listener *listener, void *data);
void pointerfocus(Client *c, struct wlr_surface *surface,
 double sx, double sy, uint32_t time);
void printstatus(void);
void pinchbegin(struct wl_listener *listener, void *data);
void pinchupdate(struct wl_listener *listener, void *data);
void pinchend(struct wl_listener *listener, void *data);
void quit(const Arg *arg);
void rendermon(struct wl_listener *listener, void *data);
void requeststartdrag(struct wl_listener *listener, void *data);
void requestmonstate(struct wl_listener *listener, void *data);
void resize(Client *c, struct wlr_box geo, int interact);
void run(char *startup_cmd);
void setcursor(struct wl_listener *listener, void *data);
void setcursorshape(struct wl_listener *listener, void *data);
void setfloating(Client *c, int floating);
void setfullscreen(Client *c, int fullscreen);
void setgamma(struct wl_listener *listener, void *data);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void setmon(Client *c, Monitor *m, uint32_t newtags);
void setpsel(struct wl_listener *listener, void *data);
void setsel(struct wl_listener *listener, void *data);
void setup(void);
void spawn(const Arg *arg);
void startdrag(struct wl_listener *listener, void *data);
void tag(const Arg *arg);
void tagmon(const Arg *arg);
void tabletaxis(struct wl_listener *listener, void *data);
void tabletproximity(struct wl_listener *listener, void *data);
void tabletbutton(struct wl_listener *listener, void *data);
void tablettip(struct wl_listener *listener, void *data);
// void checkoraddtool(Tablet *tab, struct wlr_tablet_tool *tool);
void tile(Monitor *m);
void togglefloating(const Arg *arg);
void togglefullscreen(const Arg *arg);
void toggletag(const Arg *arg);
void toggleview(const Arg *arg);
void toggletouch(const Arg *arg);
void touch_cancel(struct wl_listener *listener, void *data);
void touch_motion(struct wl_listener *listener, void *data);
void touch_frame(struct wl_listener *listener, void *data); 
void touch_up(struct wl_listener *listener, void *data); 
void touch_down(struct wl_listener *listener, void *data); 
void pointtolocal(Monitor *m, double scrnx, double scrny, double *lx, double *ly);

void swipebegin(struct wl_listener *listener, void *data);
void swipeend(struct wl_listener *listener, void *data);
void swipeupdate(struct wl_listener *listener, void *data);
void unlocksession(struct wl_listener *listener, void *data);
void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
void unmapnotify(struct wl_listener *listener, void *data);
void updatemons(struct wl_listener *listener, void *data);
void updatetitle(struct wl_listener *listener, void *data);
void urgent(struct wl_listener *listener, void *data);
void view(const Arg *arg);
void viewnext(const Arg *arg);
void viewprev(const Arg *arg);
void virtualkeyboard(struct wl_listener *listener, void *data);
Monitor *xytomon(double x, double y);
void xytonode(double x, double y, struct wlr_surface **psurface,
 Client **pc, LayerSurface **pl, double *nx, double *ny);
void zoom(const Arg *arg);

void dwl_wm_bind(struct wl_client *client, void *data,
 uint32_t version, uint32_t id);
void dwl_wm_printstatus(Monitor *monitor);
