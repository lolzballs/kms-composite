#ifndef LIBMPC_CLIENT_H
#define LIBMPC_CLIENT_H

struct mpc_display;

struct mpc_display *mpc_display_connect(const char *path, int client_id);
int mpc_display_set_framebuffer(struct mpc_display *display, int fb_id);
int mpc_display_wait_sync(struct mpc_display *display);

#endif
