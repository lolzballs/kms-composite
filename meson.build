project(
	'kms-composite',
	'c',
	version: '0.1.0',
	license: 'GPLv3',
	meson_version: '>=0.52.0',
	default_options: [
		'c_std=c99',
		'warning_level=3',
		'werror=true',
	],
)

cc = meson.get_compiler('c')

add_project_arguments(
	cc.get_supported_arguments([
		'-Wundef',
		'-Walloca',

		'-Wno-missing-braces',
		'-Wno-unused-parameter',
	]),
	'-D_GNU_SOURCE',
	language: 'c')

drm = dependency('libdrm', include_type: 'system')

include_dirs = include_directories('include')

mpc_client_lib = library(
	'mpc-client',
	files('libmpc-client.c'),
	version: meson.project_version(),
	include_directories: include_dirs,
	install: true
)
mpc_client = declare_dependency(
	link_with: mpc_client_lib,
	include_directories: include_dirs,
)
install_headers('include/libmpc-client.h')

pkgconfig = import('pkgconfig')
pkgconfig.generate(
	mpc_client_lib,
	version: meson.project_version(),
	filebase: 'mpc-client',
	name: 'mpc-client',
	description: 'client interface to communicate with mpc (multi-plane compositor)'
)

subdir('examples')

sources = files(
	'src/compositor.c',
	'src/main.c',
	'src/protocol.c',
	'shared/dumb_fb.c',
)

executable(
	'kms-composite',
	sources,
	dependencies: [drm],
	include_directories: include_dirs,
)

