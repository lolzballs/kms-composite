#include <assert.h>
#include <drm_fourcc.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "compositor.h"
#include "shared/dumb_fb.h"

int main(int argc, char *argv[]) {
	struct compositor *compositor = compositor_create();
	assert(compositor);

	struct dumb_fb layer0;
	dumb_fb_init(&layer0, compositor->fd, DRM_FORMAT_ARGB8888,
			compositor->mode->hdisplay, compositor->mode->vdisplay);
	dumb_fb_fill(&layer0, compositor->fd, 0xFFFFFFFF);
	compositor->planes[0].fb = layer0.fb_id;
	compositor_plane_enable(compositor, 0);

	struct dumb_fb layer1;
	dumb_fb_init(&layer1, compositor->fd, DRM_FORMAT_ARGB8888,
			compositor->mode->hdisplay, compositor->mode->vdisplay);
	dumb_fb_fill(&layer1, compositor->fd, 0x00000000);
	dumb_fb_draw_rect(&layer1, compositor->fd, 0x7FFF0000, 100, 100, 250, 300);
	compositor->planes[1].fb = layer1.fb_id;
	compositor->planes[1].zpos = 1;
	compositor_plane_enable(compositor, 1);

	struct dumb_fb layer2;
	dumb_fb_init(&layer2, compositor->fd, DRM_FORMAT_ARGB8888,
			compositor->mode->hdisplay, compositor->mode->vdisplay);
	dumb_fb_fill(&layer2, compositor->fd, 0x00000000);
	dumb_fb_draw_rect(&layer2, compositor->fd, 0x7F00FF00, 175, 200, 250, 300);
	compositor->planes[2].fb = layer2.fb_id;
	compositor->planes[2].zpos = 2;
	compositor_plane_enable(compositor, 2);

	int i = 0;
	compositor_draw(compositor, true);
	while (true) {
		if (i % 50 == 0) {
			int temp = compositor->planes[1].zpos;
			compositor->planes[1].zpos = compositor->planes[2].zpos;
			compositor->planes[2].zpos = temp;
		}
		i++;

		compositor_draw(compositor, false);
	}
}
