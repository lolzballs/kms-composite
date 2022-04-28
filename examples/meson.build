executable(
	'dumbfb_rect',
	files(
		'../shared/helper.c',
		'../shared/dumb_fb.c',
		'dumbfb_rect.c',
	),
	dependencies: [drm, mpc_client],
	include_directories: include_dirs,
)

gbm = dependency('gbm', required: false)
egl = dependency('egl', required: false)
glesv2 = dependency('glesv2', required: false)

if gbm.found() and egl.found() and glesv2.found()
	executable(
		'egl_rect',
		files(
			'../shared/helper.c',
			'egl_rect.c',
		),
		c_args: '-DEGL_NO_X11',
		dependencies: [gbm, drm, egl, glesv2, mpc_client],
		include_directories: include_dirs,
	)
endif
