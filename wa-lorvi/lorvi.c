/* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*- */

// Created: Mon 03 Mar 22:00:58 EET 2025 too
// Last Modified: Sun 19 Oct 2025 21:29:59 +0300 too

#define _POSIX_C_SOURCE 200112L
#include "more-warnings.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <wayland-client.h>
#include <linux/input-event-codes.h> // for BTN_LEFT

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include "xdg-shell-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

#pragma GCC diagnostic pop

#if 1
#define printf(...) do {} while (0)
#endif

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

// these work with clang(1) too //
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wold-style-definition"
static void noop(/* ... */) { } // do nothing, fill in listener structs
#pragma GCC diagnostic pop

static void xdg_wm_base_ping(void * UU(data),
			     struct xdg_wm_base *_xdg_wm_base, uint32_t serial)
{
    //printf("%s(): pong %d\n", __func__, serial);
    xdg_wm_base_pong(_xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};
static struct xdg_wm_base * xdg_wm_base;

static struct wl_shm * wl_shm;
static struct wl_compositor * wl_compositor;

static struct zwp_idle_inhibit_manager_v1 * zwp_idle_inhibit_manager;

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
    uint32_t * cntr;
    int diam;
    int r;
} B = { NULL, NULL, 256, 127 };

static void draw_circle(uint32_t color) // Jesko's method to do circle
{
    const int width = B.diam;
    const int r = B.r;
    int t1 = r / 2; // tried //2, //4, //8, //16, //32 and just 0...
    int x = r;      // \- small circles look better with larger initial t1's
    int y = 0;

    while (x >= y) {
	//printf("-- %d %d\n", x, y);
	B.cntr[+x+0 + width * (y+1)] = color; // 3
	B.cntr[+x+1 + width * (y+1)] = color;

	B.cntr[+x+0 - width * (y+0)] = color; // 2
	B.cntr[+x+1 - width * (y+0)] = color;

	B.cntr[-x+0 + width * (y+1)] = color; // 6
	B.cntr[-x+1 + width * (y+1)] = color;

	B.cntr[-x+0 - width * (y+0)] = color; // 7
	B.cntr[-x+1 - width * (y+0)] = color;
	// -- //
	B.cntr[+y+1 + width * (x+0)] = color; // 4
	B.cntr[+y+1 + width * (x+1)] = color;

	B.cntr[-y+0 + width * (x+0)] = color; // 5
	B.cntr[-y+0 + width * (x+1)] = color;

	B.cntr[+y+1 - width * (x+0)] = color; // 1
	B.cntr[+y+1 - width * (x-1)] = color;

	B.cntr[-y+0 - width * (x+0)] = color; // 8
	B.cntr[-y+0 - width * (x-1)] = color;
	y = y + 1;
	t1 = t1 + y;
	int t2 = t1 - x;
	if (t2 >= 0) {
	    t1 = t2;
	    x = x - 1;
	}
    }
}

static struct wl_buffer * wl_buffer;
static struct wl_buffer * create_buffer(void)
{
    const int size = B.diam * B.diam * 4;
    int fd = allocate_shm_file(size);
    if (fd < 0) {
	return NULL;
    }
    B.data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (B.data == MAP_FAILED) {
	close(fd);
	return NULL;
    }
    //cntr = B.data + (B.diam / 2 - 1) * B.diam + (B.diam / 2 - 1)
    //B.cntr = B.data + (B.diam + 1) * (B.diam / 2 - 1);

    const int width = B.diam;
    const int r = B.r;
    B.cntr = B.data + (width + 1) * r; // deduced from above

    struct wl_shm_pool * pool = wl_shm_create_pool(wl_shm, fd, size);
    const int stride = B.diam * 4;
    wl_buffer = wl_shm_pool_create_buffer(
	pool, 0, B.diam, B.diam, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    // simple way to do initial *filled* circle
    const int rsq = r * r;

    for (int y = 0; y < r; y++) {
	int yy = y + 1;
	for (int x = 0; x < r; x++) {
	    int p = x * x + y * y;
	    if (p >= rsq)
		break;
	    int xx = x + 1;
	    //nt32_t col = (p + 2 * B.diam > rsq)? 0xffff0000: 0xff0000ff;
	    const uint32_t col = 0xff0000ff;
	    B.cntr[xx + width * yy] = col;
	    B.cntr[xx - width * y] = col;
	    B.cntr[-x + width * yy] = col;
	    B.cntr[-x - width * y] = col;
	}
    }
    draw_circle(0xffffffff); // outline //

    wl_buffer_add_listener(wl_buffer, &wl_buffer_listener, NULL);
    return wl_buffer;
}

// xdg_surface //

static void xdg_surface_configure(void * data,
				  struct xdg_surface * xdg_surface,
				  uint32_t serial)
{
    //printf("%s(): %d\n", __func__, serial);
    xdg_surface_ack_configure(xdg_surface, serial);
    if (wl_buffer == NULL) {
	// xxx temp __auto_type test...
	__auto_type wl_surface = (struct wl_surface *)data;
	wl_buffer = create_buffer();
	wl_surface_attach(wl_surface, wl_buffer, 0, 0);
	__auto_type wl_region = wl_compositor_create_region(wl_compositor);
	wl_region_add(wl_region, 0, 0, B.diam, B.diam);
	wl_surface_set_input_region(wl_surface, wl_region);
	wl_surface_commit(wl_surface);
	wl_region_destroy(wl_region);
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// pointer //

uint32_t ccolor[2] = { 0xffffffff, 0xffffffff };

static void wl_pointer_enter(void * UU(data),
			     struct wl_pointer * UU(wl_pointer),
			     uint32_t UU(serial),
			     struct wl_surface * UU(surface),
			     wl_fixed_t UU(surface_x),
			     wl_fixed_t UU(surface_y))
{
    printf("enter %d %d\n", surface_x_unused, surface_y_unused);
    ccolor[1] = 0xff00ff00;
}

static void wl_pointer_leave(void * UU(data),
			     struct wl_pointer * UU(wl_pointer),
			     uint32_t UU(serial),
			     struct wl_surface * UU(surface))
{
    printf("leave\n");
    ccolor[1] = 0xffffffff;
}

uint32_t pct = 0;
static void wl_pointer_button(void * UU(data),
			      struct wl_pointer * UU(wl_pointer),
			      uint32_t UU(serial), uint32_t mtime,
			      uint32_t button, uint32_t state)
{
    printf("button %x %d %d\n", button, state, mtime);
    if (button == BTN_LEFT &&
	state == WL_POINTER_BUTTON_STATE_PRESSED &&
	mtime - pct < 200) exit(0);
    pct = mtime;
}

struct wl_pointer_listener wl_pointer_listener = {
    .enter = wl_pointer_enter,
    .leave = wl_pointer_leave,
    .motion = noop,
    .button = wl_pointer_button,
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
    //printf("%s(): %p %s\n", __func__, interface, interface);

    if (strcmp(interface, wl_shm_interface.name) == 0) {
	wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
	return;
    }
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
	wl_compositor = wl_registry_bind(wl_registry, name,
					 &wl_compositor_interface, 4);
	return;
    }
    if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
	xdg_wm_base = wl_registry_bind(wl_registry, name,
				       &xdg_wm_base_interface, 1);
	xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
	return;
    }
    if (strcmp(interface, wl_seat_interface.name) == 0) {
	struct wl_seat *
	    wl_seat = wl_registry_bind(wl_registry, name,
				       &wl_seat_interface, 1);
	wl_seat_add_listener(wl_seat, &wl_seat_listener, NULL);
	return;
    }
    if (strcmp(interface,
	       zwp_idle_inhibit_manager_v1_interface.name) == 0) {
	zwp_idle_inhibit_manager = wl_registry_bind(
	    wl_registry, name, &zwp_idle_inhibit_manager_v1_interface, 1);
	return;
    }
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = wl_registry_global,
    .global_remove = noop,
};

#if 0
static int xwl_display_dispatch(struct wl_display *display)
{
    int i = wl_display_dispatch(display);
    printf("%s() -> %d\n", __func__, i);
    return i;
}
#define wl_display_dispatch xwl_display_dispatch
#endif

int main(int argc, char ** argv)
{
    if (argc > 1) {
	int s = atoi(argv[1]);
	if (s > 0) {
	    /**/ if (s < 32) s = 32;
	    else if (s > 1024) s = 1024;
	    else s = (s + 1) & ~1;
	    B.diam = s;
	    B.r = s / 2 - 1;
	}
    }
    struct wl_display * wl_display = wl_display_connect(NULL);
    BB;
    struct wl_registry * wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &wl_registry_listener, NULL);

    wl_display_roundtrip(wl_display);
    BE;
    struct wl_surface *
	wl_surface = wl_compositor_create_surface(wl_compositor);
    BB;
    struct xdg_surface *
	xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, wl_surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, wl_surface);

    struct xdg_toplevel * xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    xdg_toplevel_set_title(xdg_toplevel, "(ii)");
    xdg_toplevel_set_app_id(xdg_toplevel, "lorvi");
    wl_surface_commit(wl_surface);
    BE;
    /* now is good moment to create the idle inhibitor object on surface */
    /* make vwoc build/idle-inhibit-unstable-v1-client-protocol.h | less */
#if 0
    struct zwp_idle_inhibitor_v1 * inhibitor =
#endif
	zwp_idle_inhibit_manager_v1_create_inhibitor(zwp_idle_inhibit_manager,
						     wl_surface);

    // have the buffer available before going main loop...
    while (wl_display_dispatch(wl_display) > 0 && wl_buffer == NULL) {
	/* ^ wait for xdg_surface_configure() to create wl_buffer...^ */
    }

    // three hours for now -- to be configured later...
    alarm(10800);
    BB;
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
#undef printf
    printf("\nlorvi - versio 0.8-mvp - %02d:%02d:%02d: alarm (exit in) 3h\n",
	   tm->tm_hour,tm->tm_min, tm->tm_sec);
    BE;
    while (wl_display_dispatch(wl_display) > 0) {
	if (wl_buffer_released == 0)
	    continue;
	if (ccolor[0] != ccolor[1]) {
	    draw_circle(ccolor[1]);
	    ccolor[0] = ccolor[1];
	    wl_buffer_released = 0;
	    wl_surface_attach(wl_surface, wl_buffer, 0, 0);
	    wl_surface_damage_buffer(wl_surface, 0, 0, B.diam, B.diam);
	    wl_surface_commit(wl_surface);
	    wl_display_flush(wl_display); // expect data fits to (100K+) soc...
	}
    }

    return 0;
}
