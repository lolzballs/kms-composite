#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <xf86drm.h>
#include <xf86drmMode.h>

struct compositor {
	int fd;

	uint32_t connector_id;
	drmModeModeInfo *mode;
	uint32_t crtc_id;
};

struct compositor *compositor_create();

#endif
