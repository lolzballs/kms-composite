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

add_project_arguments(cc.get_supported_arguments([
	'-Wundef',
	'-Walloca',

	'-Wno-missing-braces',
	'-Wno-unused-parameter',
]), language: 'c')

drm = dependency('libdrm', include_type: 'system')

include_dirs = include_directories('include')

subdir('examples')

sources = files(
	'src/compositor.c',
	'src/main.c',
	'shared/dumb_fb.c',
)

executable(
	'kms-composite',
	sources,
	dependencies: [drm],
	include_directories: include_dirs
)
