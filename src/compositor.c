#include "compositor.h"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_DRM_DEVICES 16

static int find_drm_device() {
	drmDevicePtr devices[MAX_DRM_DEVICES];
	int fd = -1;

	int num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
	if (num_devices < 0) {
		fprintf(stderr, "drmGetDevices2 failed: %s\n",
				strerror(-num_devices));
		return -1;
	}

	for (int i = 0; i < num_devices; i++) {
		drmDevicePtr device = devices[i];

		if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY)))
			continue;
		fd = open(device->nodes[DRM_NODE_PRIMARY], O_RDWR);
	}
	drmFreeDevices(devices, num_devices);

	if (fd < 0)
		fprintf(stderr, "no drm device found!\n");
	return fd;
}

static void get_plane_info(int fd, drmModePlane *plane, struct plane *info) {
	info->plane = plane;
	info->props = drmModeObjectGetProperties(fd, plane->plane_id,
			DRM_MODE_OBJECT_PLANE);
	info->props_info =
		calloc(info->props->count_props, sizeof(drmModePropertyRes));
	for (uint32_t i = 0; i < info->props->count_props; i++) {
		info->props_info[i] =
			drmModeGetProperty(fd, info->props->props[i]);
	}
}

static int get_planes_for_crtc(int fd, uint32_t crtc, uint32_t max_planes,
		struct plane *planes) {
	drmModePlaneRes *plane_resources = drmModeGetPlaneResources(fd);
	assert(plane_resources != NULL);

	uint32_t plane_cnt = 0;
	for (uint32_t i = 0; i < plane_resources->count_planes; i++) {
		if (plane_cnt >= max_planes) {
			fprintf(stderr, "more hardware planes available than "
					"supported (%d)... ignoring extras\n",
					max_planes);
			break;
		}

		drmModePlane *plane = drmModeGetPlane(fd, plane_resources->planes[i]);
		if (plane->possible_crtcs & (1 << crtc)) {
			get_plane_info(fd, plane, &planes[plane_cnt]);
			plane_cnt++;
		}
	}

	drmModeFreePlaneResources(plane_resources);
	return plane_cnt;
}

struct compositor *compositor_create() {
	struct compositor *ini = calloc(1, sizeof(struct compositor));

	ini->fd = find_drm_device();
	assert(ini->fd > 0);

	drmModeRes *resources = drmModeGetResources(ini->fd);
	assert(resources != NULL);

	/* find a connector */
	drmModeConnector *connector = NULL;
	for (int i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(ini->fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED
				|| connector->connection == DRM_MODE_UNKNOWNCONNECTION) {
			/* found either a connector or unknown
			 * it can be unknown for connectors like composite out */
			break;
		}
		drmModeFreeConnector(connector);
	}
	ini->connector_id = connector->connector_id;
	assert(connector != NULL);

	/* find the preferred mode */
	ini->mode = NULL;
	for (int i = 0; i < connector->count_modes; i++) {
		if (connector->modes[i].flags & DRM_MODE_TYPE_PREFERRED) {
			ini->mode = &connector->modes[i];
			break;
		}
	}
	assert(ini->mode != NULL);

	/* get the encoder resource */
	drmModeEncoder *encoder = NULL;
	for (int i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(ini->fd, resources->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id)
			break;
		drmModeFreeEncoder(encoder);
	}
	assert(encoder != NULL);

	ini->crtc_id = 0;
	for (int i = 0; i < resources->count_crtcs; i++) {
		if (resources->crtcs[i] == encoder->crtc_id) {
			ini->crtc_id = encoder->crtc_id;
			ini->crtc_index = i;
		}
	}
	assert(ini->crtc_id != 0);

	drmModeFreeResources(resources);

	int ret = 0;

	ret = drmSetClientCap(ini->fd, DRM_CLIENT_CAP_ATOMIC, 1);
	if (ret < 0) {
		fprintf(stderr, "atomic modesetting is required\n");
	}

	ini->nplanes = get_planes_for_crtc(ini->fd, ini->crtc_index,
			COMPOSITOR_MAX_PLANES, ini->planes);
	printf("compositor: found %d planes\n", ini->nplanes);

	return ini;
}
