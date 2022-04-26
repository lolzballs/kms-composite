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

	ini->crtc_id = encoder->crtc_id;
	assert(ini->crtc_id != 0);

	drmModeFreeResources(resources);

	return ini;
}
