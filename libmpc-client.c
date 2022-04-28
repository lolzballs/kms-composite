#include "libmpc-client.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct mpc_display {
	int serverfd;
	uint32_t width;
	uint32_t height;
};

struct mpc_display *mpc_display_connect(const char *path, int client_id) {
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));

	int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		return NULL;
	}

	/* TODO: Implement reading display width/height from compositor */

	if (write(fd, &client_id, sizeof(uint32_t)) == -1) {
		return NULL;
	}

	struct mpc_display *ini = calloc(1, sizeof(struct mpc_display));
	ini->serverfd = fd;
	ini->width = 720;
	ini->height = 576;
	return ini;
}

int mpc_display_set_framebuffer(struct mpc_display *client, int fb_id) {
	uint32_t buf = fb_id;
	return write(client->serverfd, &buf, sizeof(uint32_t));
}

int mpc_display_wait_sync(struct mpc_display *client) {
	uint32_t sync;
	return read(client->serverfd, &sync, sizeof(uint32_t));
}
