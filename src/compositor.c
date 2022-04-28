#include "compositor.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_DRM_DEVICES 16

static int set_connector_crtc(int fd, drmModeAtomicReq *req,
		uint32_t connector_id, int crtc_id) {
	drmModeObjectProperties *props = drmModeObjectGetProperties(fd,
			connector_id, DRM_MODE_OBJECT_CONNECTOR);
	int32_t prop_id = -1;
	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
		bool match = strcmp(prop->name, "CRTC_ID") == 0;
		prop_id = prop->prop_id;
		drmModeFreeProperty(prop);

		if (match) {
			break;
		} else {
			prop_id = -1;
		}
	}
	assert(prop_id != -1);

	drmModeFreeObjectProperties(props);
	return drmModeAtomicAddProperty(req, connector_id, prop_id, crtc_id);
}

static int set_crtc_property(int fd, drmModeAtomicReq *req, uint32_t crtc_id,
				const char *name, uint64_t value) {
	drmModeObjectProperties *props = drmModeObjectGetProperties(fd,
			crtc_id, DRM_MODE_OBJECT_CRTC);
	int32_t prop_id = -1;
	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
		bool match = strcmp(prop->name, name) == 0;
		prop_id = prop->prop_id;
		drmModeFreeProperty(prop);

		if (match) {
			break;
		} else {
			prop_id = -1;
		}
	}
	assert(prop_id != -1);
	drmModeFreeObjectProperties(props);

	return drmModeAtomicAddProperty(req, crtc_id, prop_id, value);
}

static int set_plane_property(struct plane *plane, drmModeAtomicReq *req,
		const char *name, uint64_t value) {
	int prop_id = -1;
	for (uint32_t i = 0; i < plane->props->count_props; i++) {
		if (strcmp(plane->props_info[i]->name, name) == 0) {
			prop_id = plane->props_info[i]->prop_id;
			break;
		}
	}


	if (prop_id < 0) {
		printf("no plane property: %s\n", name);
		return -EINVAL;
	}

	return drmModeAtomicAddProperty(req, plane->plane->plane_id, prop_id, value);
}

static int add_plane_to_req(struct plane *plane, drmModeAtomicReq *req,
		uint32_t crtc_id, drmModeModeInfo *mode) {
#define OK(val) if (val == -1) return -1;
	OK(set_plane_property(plane, req, "FB_ID", plane->fb));
	OK(set_plane_property(plane, req, "CRTC_ID", crtc_id));
	OK(set_plane_property(plane, req, "SRC_X", 0));
	OK(set_plane_property(plane, req, "SRC_Y", 0));
	OK(set_plane_property(plane, req, "SRC_W", mode->hdisplay << 16));
	OK(set_plane_property(plane, req, "SRC_H", mode->vdisplay << 16));
	OK(set_plane_property(plane, req, "CRTC_X", 0));
	OK(set_plane_property(plane, req, "CRTC_Y", 0));
	OK(set_plane_property(plane, req, "CRTC_W", mode->hdisplay));
	OK(set_plane_property(plane, req, "CRTC_H", mode->vdisplay));
	/* assume the user never sets the zpos for the 0-th plane,
	 * with is further assumed to be the primary plane */
	if (plane->zpos != 0) {
		OK(set_plane_property(plane, req, "zpos", plane->zpos));
	}
#undef OK
	return 0;
}


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
		if (connector->modes[i].flags & (DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DEFAULT)) {
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

void compositor_draw(struct compositor *compositor, bool modeset) {
	drmModeAtomicReq *req = drmModeAtomicAlloc();

	if (modeset) {
		if (set_connector_crtc(compositor->fd, req, compositor->connector_id,
					compositor->crtc_id) < 0) {
			fprintf(stderr, "could not set connector crtc\n");
			assert(0);
		}

		uint32_t mode_blob = -1;
		if (drmModeCreatePropertyBlob(compositor->fd, compositor->mode,
					sizeof(drmModeModeInfo), &mode_blob) != 0) {
			fprintf(stderr, "could not set create blob for modeset\n");
			assert(0);
		}

		if (set_crtc_property(compositor->fd, req, compositor->crtc_id,
					"MODE_ID", mode_blob) < 0) {
			fprintf(stderr, "could not set crtc mode property\n");
			assert(0);
		}

		if (set_crtc_property(compositor->fd, req, compositor->crtc_id,
					"ACTIVE", 1) < 0) {
			fprintf(stderr, "could not activate crtc\n");
			assert(0);
		}
	}

	for (int i = 0; i < COMPOSITOR_MAX_PLANES; i++) {
		if ((compositor->enabled_planes & (1 << i)) == 0) {
			continue;
		}

		if (add_plane_to_req(&compositor->planes[i], req,
				compositor->crtc_id, compositor->mode) < 0) {
			fprintf(stderr, "could not add plane properties\n");
			assert(0);
		}
	}

	uint32_t flags = 0;
	if (modeset) {
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
	}

	if (drmModeAtomicCommit(compositor->fd, req, flags, NULL) < 0) {
		fprintf(stderr, "warning: drmModeAtomicCommit failed\n");
	}

	drmModeAtomicFree(req);
}

void compositor_plane_enable(struct compositor *compositor, uint32_t idx) {
	compositor->enabled_planes |= (1 << idx);
}

void compositor_plane_disable(struct compositor *compositor, uint32_t idx) {
	compositor->enabled_planes &= ~(1 << idx);
}
