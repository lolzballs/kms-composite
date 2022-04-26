#include <assert.h>
#include <stdio.h>

#include "compositor.h"

int main(int argc, char *argv[]) {
	struct compositor *compositor = compositor_create();
	assert(compositor);

	compositor_draw(compositor, true);
	while (true) {
		compositor_draw(compositor, false);
	}
}
