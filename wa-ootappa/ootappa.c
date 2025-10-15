/* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*- */

// Created: Mon 03 Mar 22:00:58 EET 2025 too
// Last Modified: Wed 15 Oct 2025 21:45:49 +0300 too

#define _POSIX_C_SOURCE 200112L
#include "more-warnings.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <wayland-client.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#pragma GCC diagnostic pop

#if defined (__GNUC__) && __GNUC__ >= 4
#define UU(x) x ## _unused __attribute__ ((unused))
#else
#define UU(x) x ## _unused
#endif

// gcc -std=c2x -dM -xc -E /dev/null | grep STDC_V ;: was used to get 202000L
#if !defined (__STDC_VERSION__) || __STDC_VERSION__ < 202000L
// c23 (single argument) static assert to c11 (fyi: gcc9 knows this already)
#undef static_assert
#define static_assert(x) _Static_assert(x, #x)
#undef alignas
#define alignas _Alignas
#endif

#include "numbrle.ch"

// (variable) block begin/end -- explicit liveness...
#define BB {
#define BE }

// Shared memory support code (common "boilerplate" code like seen everyw...)
static void randname(char buf[static 7]) // using 6 first of that... //
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
	buf[i] = 'A' + (r&15) + (r&16) * 2;
	r >>= 5;
    }
}

static int create_shm_file(void)
{
    int retries = 100;
    do {
	char name[] = "/wl_shm-XXXXXX";
	randname(name + sizeof(name) - 7);
	--retries;
	// "shm_open guarantees that O_CLOEXEC is set" (if mattered)
	int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd >= 0) {
	    shm_unlink(name);
	    return fd;
	}
    } while (retries > 0 && errno == EEXIST);
    return -1;
}

static int allocate_shm_file(size_t size)
{
    int fd = create_shm_file();
    if (fd < 0)
	return -1;
    int ret;
    do {
	ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
	close(fd);
	return -1;
    }
    return fd;
}

/* --- */

/* before i forget, maeby someday
   struct {
   struct wl_shm * shm;
   struct wl_compositor * compositor;
   ....
   } Gwl;
#define Gwl_compositor_(_fn, ...) \
	wl_compositor_ ## _fn(Gwl.compositor, __VA_ARGS__)
...or...
struct wl_shm * Gwl_shm;
struct wl_compositor * Gwl_compositor;
#define Gwl_compositor_(_fn, ...) \
	wl_compositor_ ## _fn(Gwl_compositor, __VA_ARGS__)
*/

// these work with clang(1) too //
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wold-style-definition"
static void noop(/* ... */) { } // do nothing, fill in listener structs
#pragma GCC diagnostic pop


static struct wl_shm * wl_shm;
static struct wl_compositor * wl_compositor;
//static struct wl_seat * wl_seat;

static struct zwlr_layer_shell_v1 * layer_shell;

static bool wl_buffer_released;
static void wl_buffer_release(void * UU(data),
			      struct wl_buffer * UU(wl_buffer))
{
    wl_buffer_released = true;
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

static struct {
    uint32_t * data;
    uint32_t * nspx;
    int width;
    int height;
    int stride;
    int size;
} B = { NULL, NULL, 512, 384, 2048, 786432 };

static struct wl_buffer * wl_buffer;
static struct wl_buffer * create_buffer(void)
{
    int fd = allocate_shm_file(B.size);
    if (fd < 0) {
	return NULL;
    }
    B.data = mmap(NULL, B.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (B.data == MAP_FAILED) {
	close(fd);
	return NULL;
    }
    B.nspx = B.data + 60 * B.width + 50;

    struct wl_shm_pool * pool = wl_shm_create_pool(wl_shm, fd, B.size);
    wl_buffer = wl_shm_pool_create_buffer(
	pool, 0, B.width, B.height, B.stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    // blank buffer (static inline later, in near proximity to draw_border) //
    //for (int i = 0; i < B.size / 4; i++) B.data[i] = 0xff004400;
    BB;
    int i;
    for (i = 0; i < B.width; i++) B.data[i] = 0xff00ffff;
    for (; i < B.size / 4 - B.width; i++) B.data[i] = 0xffff0000;
    for (; i < B.size / 4; i++) B.data[i] = 0xff00ffff;
    for (i = 0; i < B.size / 4; i += B.width) {
	B.data[i] = B.data[i + B.width - 1] = 0xff00ffff;
    }
    BE;

    wl_buffer_add_listener(wl_buffer, &wl_buffer_listener, NULL);
    return wl_buffer;
}

static void draw_bg(void)
{
    uint32_t * pos = B.nspx;
    for (int i = 0; i < 265; i++) {
	for (int j = 0; j < 410; j++) pos[j] = 0xffff0000;
	pos += B.width;
    }
}

static void draw_num(int n, int p)
{
    uint32_t * fd = gp[n];
    uint32_t * pos = B.nspx + p;

    // top blank //
    for (int i = 0; i < gw[n].t; i++) {
	for (int j = 0; j < 200; j++) pos[j] = 0xffff0000;
	pos += B.width;
    }
    //printf("-- %d %d --\n", n, gw[n].c); return;
    int x = 0;
    uint32_t rgba = 0xffff0000;
    for (int i = 0; i < gw[n].c; i++) {
	uint32_t b = fd[i];
	for (int j = 0; j < 4; j++) {
	    uint8_t l = (uint8_t)b;
	    for (int k = 0; k < l; k++) {
		pos[x++] = rgba;
		if (x == 200) {
		    x = 0;
		    pos += B.width;
		}
	    }
	    b >>= 8;
	    rgba = (j & 1)? 0xffff0000: 0xff000000;
	}
    }
    //printf("Expect %d to be 0\n", x);
    // bottom
    for (int i = 0; i < gw[n].b; i++) {
	for (int j = 0; j < 200; j++) pos[j] = 0xffff0000;
	pos += B.width;
    }
}

static void layer_surface_configure(void * data,
				    struct zwlr_layer_surface_v1 * surface,
				    uint32_t serial,
				    uint32_t UU(w), uint32_t UU(h))
{
    //printf("%s: %d\n", __func__, serial);
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    if (wl_buffer == NULL) {
	// xxx temp __auto_type test...
	__auto_type wl_surface = (struct wl_surface *)data;
	wl_buffer = create_buffer();
	wl_surface_attach(wl_surface, wl_buffer, 0, 0);
	__auto_type wl_region = wl_compositor_create_region(wl_compositor);
#if 0
	wl_region_add(wl_region, 2, 0, B.width - 4, B.height / 3);
#else
	wl_region_add(wl_region, 0, 0, B.width, B.height);
#endif
	wl_surface_set_input_region(wl_surface, wl_region);
	wl_surface_commit(wl_surface);
	wl_region_destroy(wl_region);
    }
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static void layer_surface_closed(void * UU(data),
				 struct zwlr_layer_surface_v1 * UU(surface)) {

    // maeby needs other destructions first //
    //zwlr_layer_surface_v1_destroy(surface);
    errx(111, __FUNCTION__); // __func__ not used on purpose (until more msg)
}
#pragma GCC diagnostic pop

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static unsigned char eln[1];

static void wl_pointer_enter(void * UU(data),
			     struct wl_pointer * UU(wl_pointer),
			     uint32_t UU(serial),
			     struct wl_surface * UU(surface),
			     wl_fixed_t UU(surface_x),
			     wl_fixed_t UU(surface_y))
{
    eln[0] |= 2; //printf("enter %d %d\n", surface_x_unused, surface_y_unused);
}

static void wl_pointer_leave(void * UU(data),
			     struct wl_pointer * UU(wl_pointer),
			     uint32_t UU(serial),
			     struct wl_surface * UU(surface))
{
    eln[0] |= 1; //printf("leave\n");
}

struct wl_pointer_listener wl_pointer_listener = {
    .enter = wl_pointer_enter,
    .leave = wl_pointer_leave,
    .motion = noop,
    .button = noop,
    .axis = noop,
    .frame = noop,
    .axis_source = noop,
    .axis_stop = noop,
    .axis_discrete = noop
};

static void wl_seat_capabilities(void * UU(data), struct wl_seat * wl_seat_,
				 enum wl_seat_capability caps) {
    if ((caps & WL_SEAT_CAPABILITY_POINTER)) {
	struct wl_pointer * wl_pointer = wl_seat_get_pointer(wl_seat_);
	wl_pointer_add_listener(wl_pointer, &wl_pointer_listener, NULL);
    }
}

const struct wl_seat_listener wl_seat_listener = {
    .capabilities = wl_seat_capabilities,
    .name = noop
};


static void wl_registry_global(void * UU(data),
			       struct wl_registry * wl_registry,
			       uint32_t name, const char * interface,
			       uint32_t UU(version))
{
    //printf("%p %s\n", interface, interface);

    /**/ if (strcmp(interface, wl_shm_interface.name) == 0) {
	wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, wl_compositor_interface.name) == 0) {
	wl_compositor = wl_registry_bind(wl_registry, name,
					 &wl_compositor_interface, 4);
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
	struct wl_seat *
	    wl_seat = wl_registry_bind(wl_registry, name,
				       &wl_seat_interface, 1);
	wl_seat_add_listener(wl_seat, &wl_seat_listener, NULL);
    }
    else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
	layer_shell = wl_registry_bind(wl_registry, name,
				       &zwlr_layer_shell_v1_interface, 4);
    }
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = wl_registry_global,
    .global_remove = noop,
};

#if defined(__GNUC__) && __GNUC__ >= 4
#define ATTRIBUTE __attribute__
#else
#define ATTRIBUTE(_)
#endif

char ** cmdv;
static void exit_or_cmd(void) ATTRIBUTE ((noreturn));
static void exit_or_cmd(void)
{
    // expect all relevant fd's (the one) CLOEXEC'd
    if (cmdv) execvp(cmdv[0], cmdv);
    // else //
    exit(1);
}

struct lwex { int l; int w; };

static struct lwex draw_secs(int n)
{
    static int pn1 = 0;
    static int pn2 = 12;

    if (n < 0) exit_or_cmd();

    int n2 = n % 10;
    bool same = (pn2 == n2);
    if (same) return (struct lwex){0, 0};
    pn2 = n2;
    int n1 = n / 10;
    int l, w;
    if (pn1 != n1) {
	if (n1 == 0) draw_bg();
	else draw_num(n1, 0);
	pn1 = n1;
	l = 50; w = 410;
    }
    else {
	l = (n1 == 0)? 155: 260;
	w = 200;
    }
    draw_num(n2, n1 == 0? 105: 210);
    return (struct lwex){l, w};
}

int main(int argc, char ** argv)
{
    if (argc < 2) {
	printf("\nUsage: %s seconds [-e cmd [args]]\n\n", argv[0]);
	exit(1);
    }
    int timeout = atoi(argv[1]);
    if (timeout <= 0) errx(3, "timeout %d <= 0", timeout);
    if (timeout > 99) errx(4, "timeout %d > 99", timeout);

    if (argc > 2) {
	if (argv[2][0] != '-' || argv[2][1] != 'e' || argv[2][2] != '\0')
	    errx(5, "option '%s' not '-e'", argv[2]);
	if (argc < 4)
	    errx(6, "no cmd for '-e'");
	cmdv = argv + 3;
    }

    struct wl_display * wl_display = wl_display_connect(NULL);
    BB;
    struct wl_registry * wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &wl_registry_listener, NULL);

    wl_display_roundtrip(wl_display);

    if (layer_shell == NULL)
	errx(123, "wlr-layer-shell protocol not available");
    BE;
    struct wl_surface *
	wl_surface = wl_compositor_create_surface(wl_compositor);
    BB;
    // wat was the type again ?
    __auto_type layer_surface = zwlr_layer_shell_v1_get_layer_surface(
#if 1
	layer_shell, wl_surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
#else
	layer_shell, wl_surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
#endif
	"ootappa");
    zwlr_layer_surface_v1_set_size(layer_surface, B.width, B.height);
    // centered, no anchor nor margin (nor exclusive zone)
    //zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, ?);
    zwlr_layer_surface_v1_add_listener(layer_surface,
				       &layer_surface_listener, wl_surface);
    wl_surface_commit(wl_surface);
    // wl_display_roundtrip(display); -- to replace that dispatch loop ???

    //wl_surface_commit(wl_surface);
    BE;
    while (wl_display_dispatch(wl_display) >= 0 && wl_buffer == NULL) {
	// wait for xdg_surface_configure() to create wl_buffer...
    }
    time_t prev_secs = time(NULL);
    time_t end_secs = prev_secs + timeout;

    struct pollfd pfds[1];
    pfds[0].fd = wl_display_get_fd(wl_display);
    pfds[0].events = POLLIN;

    wl_display_flush(wl_display);  // expect data fits to (100K+) socket buf)

    while (1) {
	int tout;
	BB;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if (ts.tv_sec > prev_secs) tout = 0;
	else tout = 1000 - ts.tv_nsec / 1000000;
	prev_secs = ts.tv_sec;
	BE;

	int nfds = poll(pfds, 1, tout);
	//printf("xxx %d %d %d\n", tout, next_sec, nfds);
	if (nfds) {
	    if (pfds[0].revents) {
		// fixme: check failure, print it
		if (wl_display_dispatch(wl_display) < 0) break;
	    }
	}
	// pointer enter + exit surface -- the only place to exit 0
	if (eln[0] > 2) exit(0);
	//printf("xxx\n"); fflush(stdout);

	if (wl_buffer_released == 0)
	    continue;
	// hmm, would we care all-occluded windows (or lockscreen)
	// (xdg_toplevel::state seems to have this info (suspended))

	struct lwex ex = draw_secs(end_secs - prev_secs);
	if (ex.l) {
	    wl_buffer_released = 0;
	    wl_surface_attach(wl_surface, wl_buffer, 0, 0);
	    wl_surface_damage_buffer(wl_surface, ex.l, 60, ex.w, 267);
	    wl_surface_commit(wl_surface);
	    wl_display_flush(wl_display); // expect data fits to (100K+) soc...
	}
    }
    exit_or_cmd();
}
