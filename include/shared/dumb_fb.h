#ifndef SHARED_DUMB_FB_H
#define SHARED_DUMB_FB_H

#include <assert.h>
#include <drm_fourcc.h>
#include <stdint.h>

struct dumb_fb {
	uint32_t format;

	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t size;

	uint32_t handle;
	uint32_t fb_id;
};

int dumb_fb_init(struct dumb_fb *fb, int drm_fd, uint32_t format,
		uint32_t width, uint32_t height);
void *dumb_fb_map(struct dumb_fb *fb, int drm_fd);
void dumb_fb_fill(struct dumb_fb *fb, int drm_fd, uint32_t color);
void dumb_fb_draw_rect(struct dumb_fb *fb, int drm_fd, uint32_t color,
		int x, int y, int width, int height);

#endif
