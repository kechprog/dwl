#include <algorithm>
#include <ctime>
#include <inotifytools/inotify.h>
#include <cstdlib>
#include <cstdio>
#include <list>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <utility>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <dbus-1.0/dbus/dbus.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "net-tapesoftware-dwl-wm-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "bar.hpp"
#include "file_listener.hpp"
#include "main.hpp"
#include "dbus_handles.hpp"


struct HandleGlobalHelper {
	wl_registry* registry;
	uint32_t name;
	const char* interface;

	template<typename T>
	bool handle(T& store, const wl_interface& iface, int version) {
		if (strcmp(interface, iface.name)) {
			return false;
		}
		store = static_cast<T>(wl_registry_bind(registry, name, &iface, version));
		return true;
	}
};

/* functions */
static Monitor* monitorFromSurface(const wl_surface* surface);
static void setupMonitor(uint32_t name, wl_output* output);
static void updatemon(Monitor &mon);
static void onReady();
static void onGlobalAdd(void*, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
static void onGlobalRemove(void*, wl_registry* registry, uint32_t name);
static void requireGlobal(const void* p, const char* name);
static void waylandFlush();
static void cleanup();

/* globals */
wl_display *display;
wl_compositor *compositor;
wl_shm *shm;
zwlr_layer_shell_v1 *wlrLayerShell;
znet_tapesoftware_dwl_wm_v1 *dwlWm;
std::vector<std::string> tagNames;
std::vector<std::string> layoutNames;

/* statics */
static xdg_wm_base* xdgWmBase;
static zxdg_output_manager_v1* xdgOutputManager;
static wl_surface* cursorSurface;
static wl_cursor_image* cursorImage;
static bool ready;
static std::list<Monitor> monitors;
static std::vector<std::pair<uint32_t, wl_output*>> uninitializedOutputs;
static std::list<Seat> seats;
static Monitor* selmon;
static std::string lastStatus;
static std::string statusFifoName;
static std::vector<pollfd> pollfds;
static std::array<int, 2> signalSelfPipe;
static int displayFd {-1};
static int statusFifoWriter {-1};
static bool quitting {false};
static int inotify_fd {-1};

const std::string prefixStatus = "status ";
const std::string prefixShow   = "show ";
const std::string prefixHide   = "hide ";
const std::string prefixToggle = "toggle ";
const std::string argAll       = "all";
const std::string argSelected  = "selected";

/* handalers */
static const struct xdg_wm_base_listener xdgWmBaseListener = {
	.ping = [](void*, xdg_wm_base* sender, uint32_t serial) { /* almost ping google.com */
		xdg_wm_base_pong(sender, serial);
	}
};

static const struct zxdg_output_v1_listener xdgOutputListener = {
	.logical_position = [](void*, zxdg_output_v1*, int, int) { },
	.logical_size     = [](void*, zxdg_output_v1*, int, int) { },
	.done             = [](void*, zxdg_output_v1*) { },

	.name = [](void* mp, zxdg_output_v1* xdgOutput, const char* name) {
		static_cast<Monitor*>
			(mp)->xdgName = name;
		zxdg_output_v1_destroy(xdgOutput);
	},

	.description = [](void*, zxdg_output_v1*, const char*) { },
};

static const struct wl_pointer_listener pointerListener = {
	.enter = [](void* sp, wl_pointer* pointer, uint32_t serial,
	wl_surface* surface, wl_fixed_t x, wl_fixed_t y)
	{
		auto& seat = *static_cast<Seat*>(sp);
		seat.pointer->focusedMonitor = monitorFromSurface(surface);
		if (!cursorImage) {
			auto cursorTheme = wl_cursor_theme_load(nullptr, 24, shm);
			cursorImage = wl_cursor_theme_get_cursor(cursorTheme, "left_ptr")->images[0];
			cursorSurface = wl_compositor_create_surface(compositor);
			wl_surface_attach(cursorSurface, wl_cursor_image_get_buffer(cursorImage), 0, 0);
			wl_surface_commit(cursorSurface);
		}
		wl_pointer_set_cursor(pointer, serial, cursorSurface,
			cursorImage->hotspot_x, cursorImage->hotspot_y);
	},

	.leave = [](void* sp, wl_pointer*, uint32_t serial, wl_surface*) {
		auto& seat = *static_cast<Seat*>(sp);
		seat.pointer->focusedMonitor = nullptr;
	},

	.motion = [](void* sp, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
		auto& seat = *static_cast<Seat*>(sp);
		seat.pointer->x = wl_fixed_to_int(x);
		seat.pointer->y = wl_fixed_to_int(y);
	},

	.button = [](void* sp, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t pressed) {
		auto& seat = *static_cast<Seat*>(sp);
		auto it = std::find(begin(seat.pointer->btns), end(seat.pointer->btns), button);
		if (pressed == WL_POINTER_BUTTON_STATE_PRESSED && it == end(seat.pointer->btns)) {
			seat.pointer->btns.push_back(button);
		} else if (pressed == WL_POINTER_BUTTON_STATE_RELEASED && it != end(seat.pointer->btns)) {
			seat.pointer->btns.erase(it);
		}
	},

	.axis = [](void* sp, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) { },

	.frame = [](void* sp, wl_pointer*) {
		auto& seat = *static_cast<Seat*>(sp);
		auto mon = seat.pointer->focusedMonitor;
		if (!mon) {
			return;
		}
		for (auto btn : seat.pointer->btns) {
			mon->bar.click(mon, seat.pointer->x, seat.pointer->y, btn);
		}
		seat.pointer->btns.clear();
	},

	.axis_source   = [](void*, wl_pointer*, uint32_t) { },
	.axis_stop     = [](void*, wl_pointer*, uint32_t, uint32_t) { },
	.axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) { },
};

static const struct wl_seat_listener seatListener = {
	.capabilities = [](void* sp, wl_seat*, uint32_t cap) {
		auto& seat = *static_cast<Seat*>(sp);
		auto hasPointer = cap & WL_SEAT_CAPABILITY_POINTER;

		if (!seat.pointer && hasPointer) {
			auto &pointer = seat.pointer.emplace();
			pointer.wlPointer = wl_unique_ptr<wl_pointer> {wl_seat_get_pointer(seat.wlSeat.get())};
			wl_pointer_add_listener(seat.pointer->wlPointer.get(), &pointerListener, &seat);
		} else if (seat.pointer && !hasPointer) {
			seat.pointer.reset();
		}
	},

	.name = [](void*, wl_seat*, const char* name) { }
};

/* only on startup */
static const struct znet_tapesoftware_dwl_wm_v1_listener dwlWmListener = {

	.tag = [](void*, znet_tapesoftware_dwl_wm_v1*, const char* name) {
		tagNames.push_back(name);
	},

	.layout = [](void*, znet_tapesoftware_dwl_wm_v1*, const char* name) {
		layoutNames.push_back(name);
	},
};

/* state update of dwl */
static const struct znet_tapesoftware_dwl_wm_monitor_v1_listener dwlWmMonitorListener {

	.selected = [](void* mv, znet_tapesoftware_dwl_wm_monitor_v1*, uint32_t selected) {
		auto mon = static_cast<Monitor*>(mv);
		if (selected) {
			selmon = mon;
		} else if (selmon == mon) {
			selmon = nullptr;
		}
		mon->bar.setSelected(selected);
	},
	
	.tag = [](void* mv, znet_tapesoftware_dwl_wm_monitor_v1*, uint32_t tag, uint32_t state, uint32_t numClients, int32_t focusedClient) {
		auto mon = static_cast<Monitor*>(mv);
		int tagState = TagState::None;
		if (state & ZNET_TAPESOFTWARE_DWL_WM_MONITOR_V1_TAG_STATE_ACTIVE)
			tagState |= TagState::Active;
		if (state & ZNET_TAPESOFTWARE_DWL_WM_MONITOR_V1_TAG_STATE_URGENT)
			tagState |= TagState::Urgent;
		mon->bar.setTag(tag, tagState, numClients, focusedClient);
		uint32_t mask = 1 << tag;
		if (tagState & TagState::Active) {
			mon->tags |= mask;
		} else {
			mon->tags &= ~mask;
		}
	},

	.layout = [](void* mv, znet_tapesoftware_dwl_wm_monitor_v1*, uint32_t layout) {
		auto mon = static_cast<Monitor*>(mv);
		mon->bar.setLayout(layoutNames[layout]);
	},

	.title = [](void* mv, znet_tapesoftware_dwl_wm_monitor_v1*, const char* title) {
		auto mon = static_cast<Monitor*>(mv);
		mon->bar.setTitle(title);
	},

	.frame = [](void* mv, znet_tapesoftware_dwl_wm_monitor_v1*) { /* issued after all events to redraw */
		auto mon = static_cast<Monitor*>(mv);
		mon->hasData = true;
		updatemon(*mon);
	}
};


/* functions */
void view(Monitor& m, const Arg& arg)
{
	znet_tapesoftware_dwl_wm_monitor_v1_set_tags(m.dwlMonitor.get(), arg.ui, 1);
}

void toggleview(Monitor& m, const Arg& arg)
{
	znet_tapesoftware_dwl_wm_monitor_v1_set_tags(m.dwlMonitor.get(), m.tags ^ arg.ui, 0);
}

void setlayout(Monitor& m, const Arg& arg)
{
	znet_tapesoftware_dwl_wm_monitor_v1_set_layout(m.dwlMonitor.get(), arg.ui);
}

void tag(Monitor& m, const Arg& arg)
{
	znet_tapesoftware_dwl_wm_monitor_v1_set_client_tags(m.dwlMonitor.get(), 0, arg.ui);
}

void toggletag(Monitor& m, const Arg& arg)
{
	znet_tapesoftware_dwl_wm_monitor_v1_set_client_tags(m.dwlMonitor.get(), ~0, arg.ui);
}

void spawn(Monitor&, const Arg& arg)
{
	if (fork() == 0) {
		auto argv = static_cast<char* const*>(arg.v);
		setsid();
		execvp(argv[0], argv);
		fprintf(stderr, "somebar: execvp %s ", argv[0]);
		perror(" failed");
		exit(1);
	}
}

Monitor* monitorFromSurface(const wl_surface* surface)
{
	auto mon = std::find_if(begin(monitors), end(monitors), [surface](const Monitor& mon) {
		return mon.bar.surface() == surface;
	});
	Monitor *ret = mon != end(monitors) ? &*mon : nullptr;
	return ret; 
}


void setupMonitor(uint32_t name, wl_output* output) {
	auto& monitor = monitors.emplace_back(Monitor {
		name, 
		{}, 
		wl_unique_ptr<wl_output> {output}
	});
	monitor.bar.setStatus(lastStatus);

	auto xdgOutput 
		= zxdg_output_manager_v1_get_xdg_output(xdgOutputManager, monitor.wlOutput.get());
	zxdg_output_v1_add_listener(xdgOutput, &xdgOutputListener, &monitor);
	monitor.dwlMonitor.reset(znet_tapesoftware_dwl_wm_v1_get_monitor(dwlWm, monitor.wlOutput.get()));
	znet_tapesoftware_dwl_wm_monitor_v1_add_listener(monitor.dwlMonitor.get(), &dwlWmMonitorListener, &monitor);
}

void updatemon(Monitor& mon)
{
	if (!mon.hasData) {
		return;
	}
	if (mon.desiredVisibility) {
		if (mon.bar.visible()) {
			mon.bar.invalidate();
		} else {
			mon.bar.show(mon.wlOutput.get());
		}
	} else if (mon.bar.visible()) {
		mon.bar.hide();
	}
}

// called after we have received the initial batch of globals
void onReady()
{
	requireGlobal(compositor,       "wl_compositor");
	requireGlobal(shm,              "wl_shm");
	requireGlobal(wlrLayerShell,    "zwlr_layer_shell_v1");
	requireGlobal(xdgOutputManager, "zxdg_output_manager_v1");
	requireGlobal(dwlWm,            "znet_tapesoftware_dwl_wm_v1");
	wl_display_roundtrip(display); // roundtrip so we receive all dwl tags etc.

	ready = true;
	for (auto output : uninitializedOutputs) {
		setupMonitor(output.first, output.second);
	}
}


void onGlobalAdd(void*, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
	auto reg = HandleGlobalHelper { registry, name, interface };

	if (reg.handle(compositor, wl_compositor_interface, 4)
	||  reg.handle(shm, wl_shm_interface, 1)
	||  reg.handle(wlrLayerShell, zwlr_layer_shell_v1_interface, 4)
	||  reg.handle(xdgOutputManager, zxdg_output_manager_v1_interface, 3)) 
		return;

	if (reg.handle(xdgWmBase, xdg_wm_base_interface, 2)) {
		xdg_wm_base_add_listener(xdgWmBase, &xdgWmBaseListener, nullptr);
		return;
	}

	if (reg.handle(dwlWm, znet_tapesoftware_dwl_wm_v1_interface, 1)) {
		znet_tapesoftware_dwl_wm_v1_add_listener(dwlWm, &dwlWmListener, nullptr);
		return;
	}
	if (wl_seat* wlSeat; reg.handle(wlSeat, wl_seat_interface, 7)) {
		auto& seat = seats.emplace_back(Seat {name, wl_unique_ptr<wl_seat> {wlSeat}});
		wl_seat_add_listener(wlSeat, &seatListener, &seat);
		return;
	}
	if (wl_output* output; reg.handle(output, wl_output_interface, 1)) {
		if (ready) {
			setupMonitor(name, output);
		} else {
			uninitializedOutputs.push_back({name, output});
		}
		return;
	}
}

void onGlobalRemove(void*, wl_registry* registry, uint32_t name)
{
	monitors.remove_if([name](const Monitor &mon) { return mon.registryName == name; });
	seats.remove_if([name](const Seat &seat) { return seat.name == name; });
}

static const struct wl_registry_listener registry_listener = {
	.global = onGlobalAdd,
	.global_remove = onGlobalRemove,
};

int main(int argc, char* argv[])
{

	signal(SIGINT, [](int a) {_exit(0);});
	int opt;
	while ((opt = getopt(argc, argv, "chvs:")) != -1) {
		switch (opt) {
			case 's':
				statusFifoName = optarg;
				break;
			case 'h':
				printf("Usage: %s [-h] [-v] [-s path to the fifo] [-c command]\n", argv[0]);
				printf("  -h: Show this help\n");
				printf("  -v: Show somebar version\n");
				printf("  -s: Change path to the fifo (default is \"$XDG_RUNTIME_DIR/somebar-0\")\n");
				printf("  -c: Sends a command to sombar. See README for details.\n");
				printf("If any of these are specified (except -s), somebar exits after the action.\n");
				printf("Otherwise, somebar will display itself.\n");
				exit(0);
			case 'v':
				printf("somebar " SOMEBAR_VERSION "\n");
				exit(0);
			case 'c':
				if (optind >= argc) {
					die("Expected command");
				}
				if (statusFifoName.empty()) {
					statusFifoName = std::string {getenv("XDG_RUNTIME_DIR")} + "/somebar-0";
				}
				statusFifoWriter = open(statusFifoName.c_str(), O_WRONLY | O_CLOEXEC);
				if (statusFifoWriter < 0) {
					fprintf(stderr, "could not open %s: ", statusFifoName.c_str());
					perror("");
					exit(1);
				}
				auto str = std::string {};
				for (auto i = optind; i<argc; i++) {
					if (i > optind) str += " ";
					str += argv[i];
				}
				str += "\n";
				write(statusFifoWriter, str.c_str(), str.size());
				exit(0);
		}
	}
	
	if (pipe(signalSelfPipe.data()) < 0) {
		diesys("self pipe");
	}

	setCloexec(signalSelfPipe[0]);
	setCloexec(signalSelfPipe[1]);

	struct sigaction sighandler = {};
	sighandler.sa_handler = [](int) {
		if (write(signalSelfPipe[1], "0", 1) < 0) {
			diesys("write");
		}
	};
	if (sigaction(SIGTERM, &sighandler, nullptr) < 0) {
		diesys("sigaction");
	}
	if (sigaction(SIGINT, &sighandler, nullptr) < 0) {
		diesys("sigaction");
	}

	struct sigaction chld_handler = {};
	chld_handler.sa_handler = SIG_IGN;
	if (sigaction(SIGCHLD, &chld_handler, nullptr) < 0) {
		die("sigaction");
	}

	pollfds.push_back({
		.fd = signalSelfPipe[0],
		.events = POLLIN,
	});

	display = wl_display_connect(nullptr);
	if (!display) {
		die("Failed to connect to Wayland display");
	}
	displayFd = wl_display_get_fd(display);

	auto registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, nullptr); /* callback to add callbacks, and they say they have callback hell in js */
	wl_display_roundtrip(display);
	onReady(); /* check we got everything we need */

	pollfds.push_back({
		.fd = displayFd,
		.events = POLLIN,
	});

	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) < 0) {
		diesys("fcntl F_SETFL");
	}

	/* file listeners */
	inotify_fd = inotify_init();
	const auto file_listeners = setupFileListeners(monitors, inotify_fd);
	pollfds.push_back({
		.fd = inotify_fd,
		.events = POLLIN,
	});

	/*           time                */
	struct itimerspec timer_spec{{0}};
	timer_spec.it_value.tv_sec = 20;
	int timer_fd = timerfd_create(CLOCK_REALTIME, 0);
	timerfd_settime(timer_fd, 0, &timer_spec, NULL);
	pollfds.push_back({
		.fd     = timer_fd,
		.events = POLLIN
	});

	/* dbus */
	const DbusListener dbus_listener{};
	pollfds.push_back({
		.fd = dbus_listener.get_fd(),
		.events = POLLIN,
	});

	while (!quitting) {
		waylandFlush();
		if (poll(pollfds.data(), pollfds.size(), -1) < 0) {
			if (errno != EINTR) {
				diesys("poll");
			}
		} else {
			for (auto& ev : pollfds) {
				if (ev.revents & POLLNVAL) {
					die("poll revents contains POLLNVAL");
				} else if (ev.fd == displayFd) {
					if (ev.revents & POLLIN) {
						if (wl_display_dispatch(display) < 0) {
							die("wl_display_dispatch");
						}
					}
					if (ev.revents & POLLOUT) {
						ev.events = POLLIN;
						waylandFlush();
					}
				} else if (ev.fd == signalSelfPipe[0] && (ev.revents & POLLIN)) {
					quitting = true;
				} else if (ev.fd == dbus_listener.get_fd()) {
					dbus_listener(ev.revents, monitors);
				} else if (ev.fd == timer_fd && (ev.revents & POLLIN)) {
					uint64_t _x;
					read(timer_fd, &_x, sizeof(_x));
					for (auto &m : monitors) {
						m.bar.updateTime();
						m.bar.invalidate();
					}
				} else if (ev.fd == inotify_fd && (ev.revents & POLLIN)) {
					/**\
					|**|  there is no need to loop, since if somehting is left on inotify_fd,
					|**|  poll will still let us know and we will end up here again
				    \**/
					inotify_event ev;
					read(inotify_fd, &ev, sizeof(ev));
					for (auto &fl : file_listeners)
						fl(&ev);
				}
			}
		}
	}

	cleanup();
}

void requireGlobal(const void* p, const char* name)
{
	if (p) return;
	fprintf(stderr, "Wayland compositor does not export required global %s, aborting.\n", name);
	cleanup();
	exit(1);
}

void waylandFlush()
{
	wl_display_dispatch_pending(display);
	if (wl_display_flush(display) < 0 && errno == EAGAIN) {
		for (auto& ev : pollfds) {
			if (ev.fd == displayFd) {
				ev.events |= POLLOUT;
			}
		}
	}
}

void setCloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		diesys("fcntl FD_GETFD");
	}
	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
		diesys("fcntl FD_SETFD");
	}
}

void cleanup() {
	if (!statusFifoName.empty()) {
		unlink(statusFifoName.c_str());
	}
}

void die(const char* why) {
	fprintf(stderr, "error: %s failed, aborting\n", why);
	cleanup();
	exit(1);
}

void diesys(const char* why) {
	perror(why);
	cleanup();
	exit(1);
}
