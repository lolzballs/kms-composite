#include <assert.h>
#include <gbm.h>
#include <inttypes.h>
#include <libmpc-client.h>
#include <stdio.h>
#include <stdlib.h>
#include <xf86drmMode.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "shared/helper.h"

static struct gbm {
	struct gbm_device *dev;
	struct gbm_surface *surface;
	uint32_t format;
	int width, height;
} gbm;

static void init_gbm(int drm_fd, int w, int h, uint32_t format) {
	gbm.dev = gbm_create_device(drm_fd);
	assert(gbm.dev != NULL);

	gbm.format = format;
	gbm.surface = NULL;

	gbm.width = w;
	gbm.height = h;

	gbm.surface = gbm_surface_create(gbm.dev, gbm.width, gbm.height,
			gbm.format, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

	if (!gbm.surface) {
		printf("failed to create gbm surface\n");
		exit(EXIT_FAILURE);
	}
}

void drm_fb_destroy_callback(struct gbm_bo *bo, void *fb) {
	uint32_t fb_id = *((uint32_t *) fb);
	drmModeRmFB(gbm_device_get_fd(gbm.dev), fb_id);
	free(fb);
}

uint32_t drm_fb_get_from_bo(struct gbm_bo *bo) {
	int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
	uint32_t *bo_fb = gbm_bo_get_user_data(bo);
	uint32_t width, height, format,
		 strides[4] = {0}, handles[4] = {0},
		 offsets[4] = {0}, flags = 0;
	int ret = -1;

	if (bo_fb) {
		return *bo_fb;
	}
	bo_fb = malloc(sizeof(uint32_t));

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	format = gbm_bo_get_format(bo);

	uint64_t modifiers[4] = {0};
	modifiers[0] = gbm_bo_get_modifier(bo);
	const int num_planes = gbm_bo_get_plane_count(bo);
	for (int i = 0; i < num_planes; i++) {
		handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
		strides[i] = gbm_bo_get_stride_for_plane(bo, i);
		offsets[i] = gbm_bo_get_offset(bo, i);
		modifiers[i] = modifiers[0];
	}

	if (modifiers[0]) {
		flags = DRM_MODE_FB_MODIFIERS;
		printf("Using modifier %" PRIx64 "\n", modifiers[0]);
	}

	ret = drmModeAddFB2WithModifiers(drm_fd, width, height,
			format, handles, strides, offsets,
			modifiers, bo_fb, flags);

	if (ret) {
		perror("failed to create fb: %s\n");
		return -1;
	}

	gbm_bo_set_user_data(bo, bo_fb, drm_fb_destroy_callback);

	return *bo_fb;
}

static struct egl {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
} egl;

static void init_egl() {
	const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE,
	};

	const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};

	egl.display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR,
			gbm.dev, NULL);
	assert(egl.display != EGL_NO_DISPLAY);

	EGLint major, minor;
	assert(eglInitialize(egl.display, &major, &minor));
	printf("EGL %d.%d\n", major, minor);

	EGLint matched = 0;
	eglChooseConfig(egl.display, config_attribs, &egl.config, 1, &matched);
	assert(matched == 1);

	EGLint gbm_format;
	eglGetConfigAttrib(egl.display, egl.config,
			EGL_NATIVE_VISUAL_ID, &gbm_format);
	assert(gbm_format == GBM_FORMAT_ARGB8888);

	egl.context = eglCreateContext(egl.display, egl.config, NULL,
			context_attribs);
	assert(egl.context != EGL_NO_CONTEXT);

	egl.surface = eglCreateWindowSurface(egl.display, egl.config,
			gbm.surface, NULL);
	assert(egl.surface != EGL_NO_SURFACE);
}

static const char *vert_shader_text =
	"#version 100\n"
	"uniform mat4 transform;\n"
	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_Position = transform * pos;\n"
	"  v_color = color;\n"
	"}\n";
static const char *frag_shader_text =
	"#version 100\n"
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"
	"}\n";

static GLuint shader_program;
static GLint transform_uniform;

static void init_transform() {
	static const GLfloat transform[4][4] = {
		{ 1.0, 0.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0, 0.0 },
		{ 0.0, 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 0.0, 1.0 },
	};

	glUseProgram(shader_program);
	glUniformMatrix4fv(transform_uniform, 1, GL_FALSE, (GLfloat *) transform);
}

static GLuint compile_shader(const char *source, GLenum shader_type) {
	GLuint shader = glCreateShader(shader_type);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "error compiling: %.*s", len, log);
		exit(EXIT_FAILURE);
	}
	return shader;
}

void gl_render_init(void) {
	const uint8_t *version = glGetString(GL_VERSION);
	printf("%s\n", version);

	GLuint vert_shader = compile_shader(vert_shader_text, GL_VERTEX_SHADER);
	GLuint frag_shader = compile_shader(frag_shader_text, GL_FRAGMENT_SHADER);

	shader_program = glCreateProgram();
	glAttachShader(shader_program, vert_shader);
	glAttachShader(shader_program, frag_shader);
	glLinkProgram(shader_program);

	GLint status;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(shader_program, 1000, &len, log);
		fprintf(stderr, "error linking: %.*s", len, log);
		exit(EXIT_FAILURE);
	}

	transform_uniform = glGetUniformLocation(shader_program, "transform");
	init_transform();
}

void gl_render_draw(void) {
	static const GLfloat verts[4][2] = {
		{ -0.5, -0.5 },
		{ 0.5, -0.5 },
		{ -0.5, 0.5 },
		{ 0.5, 0.5 },
	};
	static const GLfloat colors[4][3] = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 },
		{ 0, 1, 1 },
	};

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(shader_program);

	GLuint pos = glGetAttribLocation(shader_program, "pos");
	GLuint color = glGetAttribLocation(shader_program, "color");

	glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(color, 3, GL_FLOAT, GL_FALSE, 0, colors);

	glEnableVertexAttribArray(pos);
	glEnableVertexAttribArray(color);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(pos);
	glDisableVertexAttribArray(color);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <client_id>\n", argv[0]);
		return 1;
	}

	uint32_t client_id = strtoul(argv[6], NULL, 10);

	struct mpc_display *mpc = mpc_display_connect("/home/pi/mpc.sock", client_id);
	int drmfd = open_drm_device();
	init_gbm(drmfd, 720, 576, GBM_FORMAT_ARGB8888);

	eglBindAPI(EGL_OPENGL_ES2_BIT);
	init_egl();

	eglMakeCurrent(egl.display, egl.surface, egl.surface, egl.context);
	gl_render_init();

	struct gbm_bo *bo = NULL;
	while (1) {
		gl_render_draw();

		eglSwapBuffers(egl.display, egl.surface);

		struct gbm_bo *next_bo = gbm_surface_lock_front_buffer(gbm.surface);
		mpc_display_set_framebuffer(mpc, drm_fb_get_from_bo(next_bo));
		mpc_display_wait_sync(mpc);

		if (bo) {
			gbm_surface_release_buffer(gbm.surface, bo);
		}
		bo = next_bo;
	}
}
