project('somebar', ['c', 'cpp'],
    version: '0.1.0',
    default_options: [
        'cpp_std=c++23',
])

wayland_dep = dependency('wayland-client')
wayland_cursor_dep = dependency('wayland-cursor')
cairo_dep = dependency('cairo')
pango_dep = dependency('pango')
pangocairo_dep = dependency('pangocairo')
dbus_dep = dependency('dbus-1')
pa_dep = dependency('libpulse')

subdir('protocols')

# Default flags
cpp_args = ['-Wno-parentheses']
link_args = ['-pg']

# Change flags if it's a release build
if get_option('buildtype') == 'release'
    cpp_args = ['-Wno-parentheses', '-O3', '-lto'] # Example release flags
    link_args = [] # Example release link flags
endif

executable('somebar',
    'src/main.cpp',
    'src/shm_buffer.cpp',
    'src/file_listener.cpp',
    'src/bar.cpp',
    'src/dbus_handles.cpp',
    'src/State.cpp',
    'src/Font.cpp',
    'src/pa_vol.cpp',
    wayland_sources,
    dependencies: [
        wayland_dep,
        wayland_cursor_dep,
        cairo_dep,
        pango_dep,
        pangocairo_dep,
	dbus_dep,
	pa_dep,
    ],
    install: true,
    cpp_args: cpp_args + ['-DSOMEBAR_VERSION="@0@"'.format(meson.project_version())],
    link_args: link_args
)
