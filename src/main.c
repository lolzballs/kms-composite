#include <assert.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compositor.h"
#include "protocol.h"
#include "shared/dumb_fb.h"


struct mpc_options {
	const char *socket_path;
	int max_clients;

	int *client_planes;
};

int main(int argc, char *argv[]) {
	int ret;

	struct protocol_server server;
	struct mpc_options opts = {
		.socket_path = "/home/pi/mpc.sock",
		.max_clients = 2,
		.client_planes = (int[]) { 0, 1 },
	};
	ret = protocol_server_init(&server, opts.socket_path, opts.max_clients);
	assert(ret != -1);

	struct compositor *compositor = compositor_create();
	assert(compositor);
	assert(compositor->nplanes >= opts.max_clients);

	compositor_draw(compositor, true);
	while (true) {
		ret = protocol_server_poll(&server);
		assert(ret != -1);

		for (int i = 0; i < opts.max_clients; i++) {
			if (server.clients[i].fd != -1) {
				uint32_t plane = opts.client_planes[i];
				/* no fb received this frame */
				if (server.clients[i].fb_id == (uint32_t) -1) {
					compositor_plane_disable(compositor, i);
					continue;
				}

				compositor_plane_enable(compositor, plane);
				compositor->planes[plane].fb = server.clients[i].fb_id;
				server.clients[i].fb_id = -1;
			} else {
				compositor_plane_disable(compositor, i);
			}
		}

		compositor_draw(compositor, false);
		protocol_server_broadcast(&server);
	}
}
