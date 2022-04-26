#include <assert.h>
#include <stdio.h>

#include "compositor.h"

int main(int argc, char *argv[]) {
	struct compositor *compositor = compositor_create();
	assert(compositor);
}
