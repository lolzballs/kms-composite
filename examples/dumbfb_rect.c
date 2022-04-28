#include <fcntl.h>
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

static int open_socket(uint32_t client_id) {
	int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	struct sockaddr_un my_addr = {
		.sun_family = AF_UNIX,
		.sun_path = "/home/pi/mpc.sock",
	};
	assert(connect(fd, (struct sockaddr*) &my_addr, sizeof(my_addr)) >= 0);

	assert(write(fd, &client_id, sizeof(uint32_t)) != -1);

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
	int compositor_fd = open_socket(client_id);
	assert(compositor_fd != -1);

	uint32_t color = strtoul(argv[1], NULL, 16);

	struct dumb_fb fb;
	dumb_fb_init(&fb, drm_fd, DRM_FORMAT_ARGB8888, 720, 576);
	dumb_fb_fill(&fb, drm_fd, 0x00000000);
	printf("%x\n", color);
	dumb_fb_draw_rect(&fb, drm_fd, color, atol(argv[2]), atol(argv[3]),
			atol(argv[4]), atol(argv[5]));

	while (true) {
		assert(write(compositor_fd, &fb.fb_id, sizeof(uint32_t)) == 4);
	}
}
