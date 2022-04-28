#include <fcntl.h>
#include <stdio.h>
#include <xf86drm.h>

#include "shared/helper.h"

#define MAX_DRM_DEVICES 4

int open_drm_device() {
	drmDevicePtr devices[MAX_DRM_DEVICES];
	int fd = -1;

	int num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
	if (num_devices < 0) {
		fprintf(stderr, "drmGetDevices2 failed\n");
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
