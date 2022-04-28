#include <fcntl.h>
#include <libmpc-client.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <xf86drm.h>

#include "shared/dumb_fb.h"

#define MAX_DRM_DEVICES 4

static int open_drm_device() {
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

int main(int argc, char *argv[]) {
	if (argc != 7) {
		fprintf(stderr, "usage: %s <color> <x> <y> <width> <height> "
				"<client_id>\n",
				argv[0]);
		return 1;
	}

	int drm_fd = open_drm_device();
	assert(drm_fd != -1);

	uint32_t client_id = strtoul(argv[6], NULL, 10);
	struct mpc_display *display = mpc_display_connect("/home/pi/mpc.sock",
			client_id);
	assert(display != NULL);

	uint32_t color = strtoul(argv[1], NULL, 16);

	struct dumb_fb fb;
	dumb_fb_init(&fb, drm_fd, DRM_FORMAT_ARGB8888, 720, 576);
	dumb_fb_fill(&fb, drm_fd, 0x00000000);
	printf("%x\n", color);
	dumb_fb_draw_rect(&fb, drm_fd, color, atol(argv[2]), atol(argv[3]),
			atol(argv[4]), atol(argv[5]));

	while (true) {
		assert(mpc_display_set_framebuffer(display, fb.fb_id) != -1);
		assert(mpc_display_wait_sync(display) != -1);
	}
}
