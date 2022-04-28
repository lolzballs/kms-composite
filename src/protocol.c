#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "protocol.h"

#define MAX_EVENTS 16
#define CLIENTID_SERVER 0xFFFFFFFF
#define CLIENTID_UNKNOWNCLIENT 0xFFFFFFFE

struct event_data {
	uint32_t fd;
	uint32_t client_id;
};

static uint64_t event_data_to_u64(int fd, int client_id) {
	return (((uint64_t) fd) << 32) | (((uint64_t) client_id) & 0xFFFFFFFF);
}

static struct event_data u64_to_event_data(uint64_t u64) {
	return (struct event_data) {
		.fd = u64 >> 32,
		.client_id = u64 & 0xFFFFFFFF,
	};
}

static int accept_client(int socketfd, int epollfd) {
	int ret;

	int fd = accept4(socketfd, NULL, NULL, SOCK_NONBLOCK);
	if (fd == -1) {
		perror("accept_client: accept");
		return -1;
	}

	struct epoll_event ev = {
		.events = EPOLLIN,
		.data = {
			.u64 = event_data_to_u64(fd, CLIENTID_UNKNOWNCLIENT),
		},
	};

	ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
	if (ret == -1) {
		perror("accept_client: epoll_ctl");
		return -1;
	}

	return ret;
}

static int handle_unknown_client(struct protocol_server *server, int fd) {
	int ret;

	int client_id;
	ret = read(fd, &client_id, sizeof(uint32_t));
	if (ret != sizeof(uint32_t)) {
		fprintf(stderr, "warning: ignoring non-compliant client\n");
		close(fd);
		return -1;
	} else if (ret == -1) {
		perror("handle_unknown_client: read");
		exit(EXIT_SUCCESS);
	}

	/* sanity check the client_id */
	if (client_id >= server->nclients) {
		fprintf(stderr, "warning: ignoring client that reported a "
				"client_id >= configured nclients\n");
		close(fd);
		return -1;
	}
	if (server->clients[client_id].fd != -1) {
		fprintf(stderr, "warning: ignoring client reported a client_id "
				"which is already in use\n");
		close(fd);
		return -1;
	}

	/* update epoll data */
	struct epoll_event ev = {
		.events = EPOLLIN,
		.data = {
			.u64 = event_data_to_u64(fd, client_id),
		},
	};
	ret = epoll_ctl(server->epollfd, EPOLL_CTL_MOD, fd, &ev);
	if (ret == -1) {
		perror("handle_unknown_client: epoll_ctl");
		exit(EXIT_SUCCESS);
	}

	server->clients[client_id].fd = fd;
	server->clients[client_id].fb_id = -1;
	return 0;
}

static int handle_client_message(struct protocol_server *server,
		struct event_data *data) {
	int ret;

	int fb_id;
	ret = read(data->fd, &fb_id, sizeof(uint32_t));
	if (ret != sizeof(uint32_t)) {
		fprintf(stderr, "warning: received non-compliant message from "
				"client\n");
		close(data->fd);
		return -1;
	} else if (ret == -1) {
		perror("handle_unknown_client: read");
		exit(EXIT_SUCCESS);
	}

	assert(server->clients[data->client_id].fd != -1);
	server->clients[data->client_id].fb_id = fb_id;
	return 0;
}

int protocol_server_init(struct protocol_server *server,
		const char *socket_path, int max_clients) {
	int ret;

	/* prepare socket params */
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	strcpy(addr.sun_path, socket_path);
	unlink(socket_path);

	/* prepare socket */
	int socketfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (socketfd == -1) {
		perror("socket");
		return socketfd;
	}
	ret = bind(socketfd, (struct sockaddr*) &addr, sizeof(addr));
	if (ret == -1) {
		perror("bind");
		return ret;
	}
	ret = listen(socketfd, max_clients);
	if (ret == -1) {
		perror("listen");
		return ret;
	}

	/* setup epoll */
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		return ret;
	}
	struct epoll_event ev = {
		.events = EPOLLIN,
		.data = {
			.u64 = event_data_to_u64(socketfd, CLIENTID_SERVER),
		},
	};
	ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &ev);
	if (ret == -1) {
		perror("epoll_ctl");
		return ret;
	}

	server->socketfd = socketfd;
	server->epollfd = epollfd;
	server->nclients = max_clients;
	server->clients = calloc(max_clients, sizeof(struct protocol_client_state));
	for (int i = 0; i < max_clients; i++) {
		server->clients[i].fd = -1;
		server->clients[i].fb_id = -1;
	}

	return 0;
}

int protocol_server_poll(struct protocol_server *server) {
	int ret;
	struct epoll_event events[MAX_EVENTS];

	int nevents = epoll_wait(server->epollfd, events, MAX_EVENTS, 0);
	if (nevents == -1) {
		perror("epoll_wait");
		return nevents;
	}

	for (int i = 0; i < nevents; i++) {
		struct event_data data = u64_to_event_data(events[i].data.u64);
		/* handle clients closing gracefully */
		if (events[i].events & EPOLLHUP) {
			ret = close(data.fd);
			if (ret == -1) {
				perror("close");
			}

			server->clients[data.client_id].fd = -1;
			server->clients[data.client_id].fb_id = -1;
			continue;
		}

		switch (data.client_id) {
			case CLIENTID_SERVER:
				ret = accept_client(server->socketfd, server->epollfd);
				if (ret == -1) {
					fprintf(stderr, "fatal: error occured while "
							"accepting new connection\n");
					exit(EXIT_FAILURE);
				}
				break;
			case CLIENTID_UNKNOWNCLIENT:
				ret = handle_unknown_client(server, data.fd);
				break;
			default:
				ret = handle_client_message(server, &data);
				break;
		}
	}

	return 0;
}
