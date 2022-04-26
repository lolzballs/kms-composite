#include "shared/dumb_fb.h"

#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int dumb_fb_init(struct dumb_fb *fb, int drm_fd, uint32_t format,
		uint32_t width, uint32_t height) {
	int ret;

	assert(format == DRM_FORMAT_ARGB8888 || format == DRM_FORMAT_XRGB8888);

	struct drm_mode_create_dumb create = {
		.width = width,
		.height = height,
		.bpp = 32,
		.flags = 0,
	};
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if (ret < 0) {
		return ret;
	}

	uint32_t handles[4] = { create.handle };
	uint32_t strides[4] = { create.pitch };
	uint32_t offsets[4] = { 0 };

	uint32_t fb_id;
	ret = drmModeAddFB2(drm_fd, width, height, format, handles, strides,
			    offsets, &fb_id, 0);
	if (ret < 0) {
		return ret;
	}

	fb->width = width;
	fb->height = height;
	fb->stride = create.pitch;
	fb->size = create.size;
	fb->handle = create.handle;
	fb->fb_id = fb_id;

	return 0;
}

void *dumb_fb_map(struct dumb_fb *fb, int drm_fd) {
	int ret;

	struct drm_mode_map_dumb map = { .handle = fb->handle };
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
	if (ret < 0) {
		return MAP_FAILED;
	}

	return mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd,
		    map.offset);
}

void dumb_fb_fill(struct dumb_fb *fb, int drm_fd, uint32_t color) {
	uint32_t *data;
	size_t i;

	data = dumb_fb_map(fb, drm_fd);
	if (data == MAP_FAILED) {
		return;
	}

	for (i = 0; i < fb->size / sizeof(uint32_t); i++) {
		data[i] = color;
	}

	munmap(data, fb->size);
}

void dumb_fb_draw_rect(struct dumb_fb *fb, int drm_fd, uint32_t color,
		int x, int y, int width, int height) {
	uint32_t *data = dumb_fb_map(fb, drm_fd);
	if (data == MAP_FAILED) {
		return;
	}

	for (int r = y; r < y + height; r++) {
		for (int c = x; c < x + width; c++) {
			data[r * fb->stride / sizeof(uint32_t) + c] = color;
		}
	}

	munmap(data, fb->size);
}
