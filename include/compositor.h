#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <xf86drm.h>
#include <xf86drmMode.h>

#define COMPOSITOR_MAX_PLANES 4

struct plane {
	drmModePlane *plane;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
};

struct compositor {
	int fd;

	uint32_t connector_id;
	drmModeModeInfo *mode;
	uint32_t crtc_id;

	uint32_t crtc_index;
	uint32_t nplanes;
	struct plane planes[COMPOSITOR_MAX_PLANES];
};

struct compositor *compositor_create();

#endif
