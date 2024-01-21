.POSIX:
.SUFFIXES:

include config.mk

# Specify custom pkg-config path for wlroots
WLROOTS_PKG_CONFIG_PATH = /usr/lib/wlroots0.16/pkgconfig
PKG_CONFIG = PKG_CONFIG_PATH=$(WLROOTS_PKG_CONFIG_PATH) pkg-config

# flags for compiling
DWLCPPFLAGS = -I. -DWLR_USE_UNSTABLE -std=c11 -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\" $(XWAYLAND)
DWLDEVCFLAGS = -g -pedantic -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wshadow -Wunused-macros\
	-Werror=strict-prototypes -Werror=implicit -Werror=return-type -Werror=incompatible-pointer-types

# CFLAGS / LDFLAGS
PKGS      = wlroots wayland-server xkbcommon libinput $(XLIBS)
DWLCFLAGS = `$(PKG_CONFIG) --cflags $(PKGS)` $(DWLCPPFLAGS) $(DWLDEVCFLAGS) $(CFLAGS)
LDLIBS    = `$(PKG_CONFIG) --libs $(PKGS)` $(LIBS) -lm

all: dwl
dwl: dwl.o util.o net-tapesoftware-dwl-wm-unstable-v1-protocol.o tablet-unstable-v2-protocol.o
	clang dwl.o util.o net-tapesoftware-dwl-wm-unstable-v1-protocol.o tablet-unstable-v2-protocol.o $(LDLIBS) $(LDFLAGS) $(DWLCFLAGS) -o $@
dwl.o: dwl.c config.mk config.h client.h xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h net-tapesoftware-dwl-wm-unstable-v1-protocol.o tablet-unstable-v2-protocol.h cursor-shape-v1-protocol.h
util.o: util.c util.h

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
WAYLAND_SCANNER   = `$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner`
WAYLAND_PROTOCOLS = `$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols`

xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@
wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-layer-shell-unstable-v1.xml $@
cursor-shape-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/staging/cursor-shape/cursor-shape-v1.xml $@

net-tapesoftware-dwl-wm-unstable-v1-protocol.h: protocols/net-tapesoftware-dwl-wm-unstable-v1.xml
	$(WAYLAND_SCANNER) server-header \
		protocols/net-tapesoftware-dwl-wm-unstable-v1.xml $@
net-tapesoftware-dwl-wm-unstable-v1-protocol.c: protocols/net-tapesoftware-dwl-wm-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code \
		protocols/net-tapesoftware-dwl-wm-unstable-v1.xml $@
net-tapesoftware-dwl-wm-unstable-v1-protocol.o: net-tapesoftware-dwl-wm-unstable-v1-protocol.h

pointer-gestures-unstable-v1-protocol.h: protocols/pointer-gestures-unstable-v1.xml
	$(WAYLAND_SCANNER) server-header \
		protocols/pointer-gestures-unstable-v1.xml $@
pointer-gestures-unstable-v1-protocol.c: protocols/pointer-gestures-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code \
		protocols/pointer-gestures-unstable-v1.xml $@
pointer-gestures-unstable-v1-protocol.o: pointer-gestures-unstable-v1-protocol.h

tablet-unstable-v2-protocol.h: protocols/tablet-unstable-v2.xml
	$(WAYLAND_SCANNER) server-header \
		protocols/tablet-unstable-v2.xml $@
tablet-unstable-v2-protocol.c: protocols/tablet-unstable-v2.xml
	$(WAYLAND_SCANNER) private-code \
		protocols/tablet-unstable-v2.xml $@
tablet-unstable-v2-protocol.o: tablet-unstable-v2-protocol.h

clean:
	rm -f dwl *.o *-protocol.*

dist: clean
	mkdir -p dwl-$(VERSION)
	cp -R LICENSE* Makefile README.md client.h config.def.h\
		config.mk protocols dwl.1 dwl.c util.c util.h\
		dwl-$(VERSION)
	tar -caf dwl-$(VERSION).tar.gz dwl-$(VERSION)
	rm -rf dwl-$(VERSION)

install: dwl
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f dwl $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dwl
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	cp -f dwl.1 $(DESTDIR)$(MANDIR)/man1
	chmod 644 $(DESTDIR)$(MANDIR)/man1/dwl.1
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dwl $(DESTDIR)$(MANDIR)/man1/dwl.1

.SUFFIXES: .c .o
.c.o:
	clang $(CPPFLAGS) $(DWLCFLAGS) -c $<
