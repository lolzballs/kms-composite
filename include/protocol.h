#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

struct protocol_client_state {
	int fd;
	uint32_t fb_id;
};

struct protocol_server {
	int socketfd;
	int epollfd;

	int nclients;
	struct protocol_client_state *clients;
};

int protocol_server_init(struct protocol_server *server,
		const char *socket_path, int max_clients);
int protocol_server_poll(struct protocol_server *server);

#endif
