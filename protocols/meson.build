wayland_scanner = find_program('wayland-scanner')
wayland_protos_dep = dependency('wayland-protocols')
wl_protocol_dir = wayland_protos_dep.get_variable('pkgdatadir')

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'])
wayland_scanner_server = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.h',
	arguments: ['server-header', '@INPUT@', '@OUTPUT@'])

wayland_xmls = [
    'net-tapesoftware-dwl-wm-unstable-v1.xml',
    'wlr-layer-shell-unstable-v1.xml',
    'wlr-input-inhibitor-unstable-v1.xml',
    '/usr/share/wayland-protocols/staging/cursor-shape/cursor-shape-v1.xml',
    '/usr/share/wayland-protocols/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml',
    '/usr/share/wayland-protocols/stable/tablet/tablet-v2.xml',
    '/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml'
]

wayland_sources = [
	wayland_scanner_code.process(wayland_xmls),
	wayland_scanner_server.process(wayland_xmls),
]
