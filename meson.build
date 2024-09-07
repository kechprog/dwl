project('dwl', 'c', version: '0.4')
subdir('protocols')
cc = meson.get_compiler('c')
math = cc.find_library('m', required : false)

config_h = configuration_data()
config_h.set('VERSION', meson.project_version())

# Dependencies
wayland_dep = dependency('wayland-server')
wayland_cursor_dep = dependency('wayland-cursor')
wlroots_dep = dependency('wlroots-0.19')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput')
xcb_dep = dependency('xcb')
xcb_icccm_dep = dependency('xcb-icccm')
dbus_dep =  dependency('dbus-1')

pkg_deps = [
  wayland_dep,
  wayland_cursor_dep,
  wlroots_dep,
  xkbcommon_dep,
  libinput_dep,
  xcb_dep,
  xcb_icccm_dep,
  dbus_dep,
  math
]

cflags = [
  '-DWLR_USE_UNSTABLE',
  '-DVERSION="@0@"'.format(meson.project_version()), 
  '-D_POSIX_C_SOURCE=200809L',

  '-pedantic', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-sign-compare', '-Wshadow',
  '-Wunused-macros', '-Werror=strict-prototypes', '-Werror=implicit',
  '-Werror=return-type'
]

if get_option('buildtype') == 'debug'
  cflags += ['-g', '-O0']
else
  cflags += ['-O3', '-lto']
endif

if get_option('xwayland')
  cflags += ['-DXWAYLAND']
endif

executable(
  'dwl', 
  './src/dwl.c', 
  './src/util.c',
  './src/globals.c',
  './src/tapesoftware.c',
  './src/dbus.c',
  wayland_sources,
  dependencies: pkg_deps,
  include_directories: './src',
  c_args: cflags,
  install: true
)

