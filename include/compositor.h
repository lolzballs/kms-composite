#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <xf86drm.h>
#include <xf86drmMode.h>

#define COMPOSITOR_MAX_PLANES 8
#define COMPOSITOR_MAX_LAYERS COMPOSITOR_MAX_PLANES

struct plane {
	drmModePlane *plane;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;

	int fb;
	int zpos;
};

struct compositor {
	int fd;

	uint32_t connector_id;
	drmModeModeInfo *mode;
	uint32_t crtc_id;

	uint32_t crtc_index;

	uint32_t enabled_planes;
	uint32_t nplanes;
	struct plane planes[COMPOSITOR_MAX_PLANES];
};

struct compositor *compositor_create();
void compositor_draw(struct compositor *compositor, bool modeset);

void compositor_plane_enable(struct compositor *compositor, uint32_t idx);
void compositor_plane_disable(struct compositor *compositor, uint32_t idx);

#endif
