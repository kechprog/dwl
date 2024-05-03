#include "dwl.h"
#include <X11/XF86keysym.h>
#include <libinput.h>

#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

/* appearance */
static const int sloppyfocus               = 1;  /* focus follows mouse */
static const int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static const unsigned int borderpx         = 1;  /* border pixel of windows */
static const unsigned int gappx            = 8;  /* gap in pixels between windows */
static const float bordercolor[]           = COLOR(0xb8c0e0ff);
static const float focuscolor[]            = COLOR(0xed8796ff);
static const float urgentcolor[]           = COLOR(0xed8796ff);

/* trackpad emulation */
static const double clickmargin            = 0.00001; /* means if you tapped and moved finger less than 0.0001 percent of screen from start you clicked */
static const uint64_t doubleclicktimems    = 3000; /* if 2 click happen withing 100ms it is treated as double click, only for touch emulating trackpad */
static const double sens_x             = 4700; /* idk just test it */
static const double sens_y             = 7*512; /* idk just test it */

/* To conform the xdg-protocol, set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[]         = {0.1, 0.1, 0.1, 1.0};

static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* tagging - tagcount must be no greater than 31 */
static const int tagcount = sizeof(tags) / sizeof(tags[0]);

/* logging */
static int log_level = WLR_ERROR;

static const Rule rules[] = {
	/* app_id     title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       1 << 3,            1,           -1 },
};

/* tablets which are to be enabled */
/* if tablet name is not presented it won't be added */
/* -1 means map one to one on curent monitor */
static const TabletRule tebletRules[] = {
	// {.name = "ELAN9008:00 04F3:2D55 Stylus"}, /*  top  */
	// {.name = "ELAN9009:00 04F3:2C1B Stylus"}, /* botom */
	{ 
	  .name = NULL,      /* matches everything(keep it last) */
	  .aspect_ratio = -1 /* map 1 to 1 on selected monitor */
	}
};

/* layout(s) */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* monitors */
static const MonitorRule monrules[] = {
	/* lenovo thinkpad x1 yoga g6 */
	/*  name  ,  mfact ,  nmaster , scale , layout      ,   rotate/reflect             ,  x  ,   y  */
	{"HDMI-A-1", 0.6, 1, 1, &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL, 0, 0, NULL, NULL},
	{ "eDP-1",   0.6   ,     1    ,   1   , &layouts[0] ,   WL_OUTPUT_TRANSFORM_NORMAL ,  0  ,   1080  
	/* |	    touch_name	     |            brightness_name         | */
	   , "Wacom HID 5276 Finger" , "sysfs/backlight/intel_backlight"  }  ,


	/* asus zenbook(ux435) */
	// { NULL ,   0.6   ,     1    ,   1   , &layouts[0] ,   WL_OUTPUT_TRANSFORM_NORMAL ,  0  ,   0  
	// /*       touch name        tablet name */
	//     , NULL, NULL, "sysfs/backlight/amdgpu_bl0"},
	
	
	/* asus zenbook duo(ux482) */
	// { "eDP-1" ,   0.6   ,     1    ,   1   , &layouts[0] ,   WL_OUTPUT_TRANSFORM_NORMAL ,  0  ,   0,
	// /*       touch name        tablet name */
	// "ELAN9008:00 04F3:2D55", NULL, "sysfs/backlight/intel_backlight"},
	// { "DP-1"  ,   0.5   ,     1    ,   1   , &layouts[0] ,   WL_OUTPUT_TRANSFORM_NORMAL ,  0  ,  1080,
	// /*       touch name        tablet name */
	// "ELAN9009:00 04F3:2C1B", "ELAN9009:00 04F3:2C1B Stylus", "sysfs/leds/asus::screenpad"}
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	/* example:
	.options = "ctrl:nocaps",
	*/
	.options = NULL,
};

static const char *kbd_layouts[] = {
	"us", /* on start up defualt */
	"ru",
};

static const int repeat_rate = 40;
static const int repeat_delay = 400;

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 1;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0229;
/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
#define MODKEY WLR_MODIFIER_ALT

#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                    KEY,                   view,            {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL,  KEY,                   toggleview,      {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY,                  tag,             {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL|WLR_MODIFIER_SHIFT, KEY, toggletag,       {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *termcmd[]           = { "kitty", NULL };
static const char *menucmd[]           = { "wofi", NULL };

static const char *volupcmd[]          = {"pamixer", "-i", "5", NULL};
static const char *voldowncmd[]        = {"pamixer", "-d", "5", NULL};
static const char *volmutetogglecmd[]  = {"pamixer", "-t", NULL};
static const char *togglemicmute[]     = {"pamixer", "--default-source", "--toggle-mute", NULL};

/* want to know a name of specific key, as they are defined in xf86 keysym ?
 * - https://github.com/jwrdegoede/wev
 */
static const Key keys[] = {
	/* Note that Shift changes certain key codes: c -> C, 2 -> at, etc. */
	/* modifier                  key                 function        argument */
	{ MODKEY,                    KEY_SPACE,      spawn,          {.v = menucmd} },
	{ MODKEY,                    KEY_ENTER,     spawn,          {.v = termcmd} },
	{ MODKEY,                    KEY_J,          focusstack,     {.i = +1} },
	{ MODKEY,                    KEY_D,          debug,		 {0} },
	{ MODKEY,                    KEY_K,          focusstack,     {.i = -1} },
	// { MODKEY,                    XKB_KEY_i,          incnmaster,     {.i = +1} },
	// { MODKEY,                    XKB_KEY_d,          incnmaster,     {.i = -1} },
	{ MODKEY,                    KEY_H,          setmfact,       {.f = -0.05} },
	{ MODKEY,                    KEY_L,          setmfact,       {.f = +0.05} },
	{ MODKEY,                    KEY_TAB,        view,           {0} },
	{ MODKEY,                    KEY_Q,          killclient,     {0} },
	{ MODKEY,                    KEY_R,          monrotate,      {0} },
	{ MODKEY,                    KEY_C,          cycle_focused_kbd, {0} },
	// { MODKEY,                    XKB_KEY_t,          setlayout,      {.v = &layouts[0]} },
	// { MODKEY,                    XKB_KEY_f,          setlayout,      {.v = &layouts[1]} },
	// { MODKEY,                    XKB_KEY_m,          setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                    KEY_T,          setlayout,      {0} }, // cycle
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY_F,          togglefloating, {0} },
	{ MODKEY,                    KEY_F,          togglefullscreen, {0} },
	{ MODKEY,                    KEY_0,          view,           {.ui = ~0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY_KPRIGHTPAREN, tag,            {.ui = ~0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY_COMMA,      focusmon,       {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY_DOT,     focusmon,       {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY_COMMA,       tagmon,         {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT, KEY_DOT,    tagmon,         {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_CTRL,  KEY_Q,          quit,           {0} },

	{ 0                        , XF86XK_Launch6,     movenext,       {0} },

	/* FUNCTION KEYS */
	{ 0, KEY_BRIGHTNESSUP,   monitorbrightness,  {.i =  5} },
	{ 0, KEY_BRIGHTNESSDOWN, monitorbrightness,  {.i = -5} },

	{ 0, KEY_VOLUMEUP,	spawn,              {.v = volupcmd}          },
	{ 0, KEY_VOLUMEDOWN,	spawn,              {.v = voldowncmd}        },
	{ 0, KEY_MUTE,		spawn,              {.v = volmutetogglecmd}  },
	{ 0, KEY_F20,		spawn,              {.v = togglemicmute}     }, /* mic mute on x1 yoga gen 6 */

	{ 0, XF86XK_Launch7,		toggletouch,        {0} },
	{ 0, KEY_SYSRQ,			spawn,		   SHCMD("grim -g \"$(slurp -d)\" - | swappy -f -") }, /* print screen */

	TAGKEYS(          KEY_1,                 0),
	TAGKEYS(          KEY_2,                 1),
	TAGKEYS(          KEY_3,                 2),
	TAGKEYS(          KEY_4,                 3),
	TAGKEYS(          KEY_5,                 4),
	TAGKEYS(          KEY_6,                 5),
	TAGKEYS(          KEY_7,                 6),
	TAGKEYS(          KEY_8,                 7),
	TAGKEYS(          KEY_9,                 8),

	/* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server */
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,KEY_F##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ MODKEY,                    BTN_LEFT,  moveresize,     {.ui = CurMove} },
	{ MODKEY,                    BTN_MIDDLE,togglefloating, {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, BTN_LEFT,  moveresize,     {.ui = CurResize} },
};

static const float corner_angle = PI/4;        /* 45 degrees */
static const float corner_wigle_angle = PI/18; /* 10 degrees */
static const Swipe swipegestures[] = {
	/*| fingercount | direction | funcion |   argument   | */
	{      3        , SwipeRight, viewnext,      {0}     , },
	{      4        , SwipeRight, viewnext,      {0}     , },
	{      3        , SwipeLeft , viewprev,      {0}     , },
	{      4        , SwipeLeft , viewprev,      {0}     , }, 
};
