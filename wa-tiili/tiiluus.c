/* -*- mode: c; c-file-style: "stroustrup"; tab-width: 8; -*- */

#define _POSIX_C_SOURCE 200112L
#include "more-warnings.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <wayland-client.h>

#ifndef USE_WLR_LAYER_SHELL
#define USE_WLR_LAYER_SHELL 1
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#if USE_WLR_LAYER_SHELL
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#endif
#include "xdg-shell-client-protocol.h"

#include "ext-workspace-v1-client-protocol.h"
#pragma GCC diagnostic pop

#if 1
#define DP printf
#define VP (void*)
#else
#define DP(...) do {} while (0);
#undef VP
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

/* Shared memory support code (common "boilerplate" code in all places...) */
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


#if ! USE_WLR_LAYER_SHELL
static void xdg_wm_base_ping(void * UU(data),
			     struct xdg_wm_base *_xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(_xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};
static struct xdg_wm_base * xdg_wm_base;
#endif

static struct wl_shm * wl_shm;
static struct wl_compositor * wl_compositor;
//static struct wl_seat * wl_seat;
#if USE_WLR_LAYER_SHELL
static struct zwlr_layer_shell_v1 * layer_shell;
#endif

static bool wl_buffer_released;
static void wl_buffer_release(void * UU(data),
			      struct wl_buffer * UU(wl_buffer))
{
    wl_buffer_released = true;
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

#include "txts.ch"

/*static*/ const char _info[] = TEXTS_CH_INFO;

static struct {
    uint32_t * data;
    int width;
    int height;
    int stride;
    int size;
} B;

static void draw_txti(const struct txtimg * ti, int xoff)
{
    uint32_t * buffer = B.data + xoff;
    const int b_width = B.width;
    const uint32_t * pixelp = ti->pixels;
    const int t_width = ti->width;
    for (int y = 0; y < TEXTHEIGHT; ++y) {
	for (int x = 0; x < t_width; x++) {
	    buffer[y * b_width + x] = *pixelp++;
	}
    }
}

// these can be removed after LOC_* defines fixed //
static_assert (TEXTWIDTH_NUM == TEXTWIDTH_COL);
static_assert (TEXTWIDTH_NUM == TEXTWIDTH_DOT);
static_assert (TEXTWIDTH_NUM == TEXTWIDTH_W);
static_assert (TEXTWIDTH_NUM == TEXTWIDTH_PLUS);
static_assert (TEXTWIDTH_NUM == TEXTWIDTH_MINUS);

// workspace
#define LOC_WS (TEXTWIDTH_NUM * 3 / 4)
#define LOC_XX (TEXTWIDTH_NUM * 1 / 4)

// mday
#define LOC_MD1 (LOC_WS + TEXTWIDTH_NUM * 13 / 6)
#define LOC_MD2 (LOC_MD1 + TEXTWIDTH_NUM)

// wday(abbrv)
#define LOC_WD (LOC_MD2 + TEXTWIDTH_NUM * 2)

// HH:MM:SS
#define LOC_H1 (LOC_WD + TEXTWIDTH_WDAYS + TEXTWIDTH_NUM)
#define LOC_H2 (LOC_H1 + TEXTWIDTH_NUM)
#define LOC_HS (LOC_H1 + TEXTWIDTH_NUM * 2)
#define LOC_M1 (LOC_H1 + TEXTWIDTH_NUM * 3)
#define LOC_M2 (LOC_H1 + TEXTWIDTH_NUM * 4)
#define LOC_MS (LOC_H1 + TEXTWIDTH_NUM * 5)
#define LOC_S1 (LOC_H1 + TEXTWIDTH_NUM * 6)
#define LOC_S2 (LOC_H1 + TEXTWIDTH_NUM * 7)

// week number
#define LOC_W0 (LOC_H1 + TEXTWIDTH_NUM * 9)
#define LOC_W1 (LOC_W0 + TEXTWIDTH_NUM * 1)
#define LOC_W2 (LOC_W0 + TEXTWIDTH_NUM * 2)

// battery
#define LOC_B0 (LOC_W2 + TEXTWIDTH_NUM * 2)
#define LOC_B1 (LOC_B0 + TEXTWIDTH_NUM * 1)
#define LOC_B2 (LOC_B0 + TEXTWIDTH_NUM * 2)
#define LOC_B3 (LOC_B0 + TEXTWIDTH_NUM * 3)
#define LOC_B4 (LOC_B0 + TEXTWIDTH_NUM * 4)

static const struct txtimg *nums[10] = {
    &ti_0, &ti_1, &ti_2, &ti_3, &ti_4,
    &ti_5, &ti_6, &ti_7, &ti_8, &ti_9
};
static const struct txtimg *wdays[8] = {
    &ti_wd_7, &ti_wd_1, &ti_wd_2, &ti_wd_3,
    &ti_wd_4, &ti_wd_5, &ti_wd_6, &ti_wd_7
};

// next 2 originally from musl libc (MIT licensed), tuned a bit
static inline int is_leap(int y)
{
    y += 1900;
    return !(y%4) && ((y%100) || !(y%400));
}

static int iso_week(int year, int yday, int wday)
{
    int val = (yday + 7 - (wday + 6) % 7) / 7;
    /* If 1 Jan is just 1-3 days past Monday,
     * the previous week is also in this year. */
    if ((wday + 371 - yday - 2) % 7 <= 2)
	val++;
    if (!val) {
	val = 52;
	/* If 31 December of prev year a Thursday,
	 * or Friday of a leap year, then the
	 * prev year has 53 weeks. */
	int dec31 = (wday + 7 - yday - 1) % 7;
	if (dec31 == 4 || (dec31 == 5 && is_leap(year - 1)))
	    val++;
    } else if (val == 53) {
	/* If 1 January is not a Thursday, and not
	 * a Wednesday of a leap year, then this
	 * year has only 52 weeks. */
	int jan1 = (wday + 371 - yday) % 7;
	if (jan1 != 4 && (jan1 != 3 || !is_leap(year)))
	    val = 1;
    }
    return val;
}

#define BpPFX "/sys/class/power_supply/BAT"
static alignas(4) char batpath[40] = BpPFX; // alignas no longer needed but...
static int batfd = -1;
enum { BATPATHPFXLEN = sizeof BpPFX - 1 };
#undef BpPFX

static void open_batfile(void)
{
    B.width = LOC_B4 + TEXTWIDTH_NUM + LOC_WS;
    B.height = TEXTHEIGHT + 1;
    B.stride = B.width * 4;
    B.size = B.stride * B.height;

    for (int i = 0; i < 9; i++) {
	batpath[BATPATHPFXLEN] = '0' + i;
	memcpy(batpath + BATPATHPFXLEN + 1, "/" "uevent", 7);
	//DP("%s\n", batpath);
	batfd = open(batpath, O_RDONLY, 0);
	if (batfd >= 0) return;
    }
    // else no batfile
    B.width = LOC_B0 - LOC_XX; // XXX ;/
    B.stride = B.width * 4;
    B.size = B.stride * B.height;
}

//STRNCMP_LITERAL in notmuch...
#define MEMCMP_LITERAL(var, lit) memcmp((var), (lit ""), sizeof (lit) - 1)

static int read_batstatus(void)
{
    // dbus someday maeby -- but needs upowerd -- is it there always...
    char buf[2052];

    // if (batfd < 0) return 0; -- checked by caller
    BB;
    int len = pread(batfd, buf, sizeof buf - 4, 0);
    if (len < 0) return 0;
    buf[len] = '\0';
    BE;
    char * status = NULL, *c_full = NULL, *c_now = NULL;
    char * p = buf - 1;
    do {
	p++;
#define LIT "POWER_SUPPLY_STATUS="
	/**/ if (MEMCMP_LITERAL(p, LIT) == 0) status = p + sizeof (LIT) - 1;
#undef LIT
#define LIT "POWER_SUPPLY_CHARGE_FULL="
	else if (MEMCMP_LITERAL(p, LIT) == 0) c_full = p + sizeof (LIT) - 1;
#undef LIT
#define LIT "POWER_SUPPLY_CHARGE_NOW="
	else if (MEMCMP_LITERAL(p, LIT) == 0) c_now = p + sizeof (LIT) - 1;
#undef LIT
    } while ((p = strchr(p, '\n')) != NULL);

    long i_full = c_full? atol(c_full): 0;
    if (i_full == 0) return 0;
static_assert (sizeof(long) >= 8);
    long i_val = ((c_now? atol(c_now): 0) * 1000) / i_full;
    if (i_val > 999) i_val = 999;
    bool neg = (status && (status[0] == 'F' || status[0] == 'C'))? 0: 1;
    //DP("%ld %ld %ld\n", i_val, atol(c_full), atol(c_now));
    return neg? -i_val: i_val;
}

/* 2 workspace vars */
static char wsfile[64];
static char wsn[2];
/* one enter/leave */
static char eln[2];

static inline void draw_border(char f)
{
    uint32_t argb = f? 0xffffffff: 0x00000000;

    int i = 0;
    for (; i < B.size / 4 - B.width; i += B.width) {
	B.data[i] = B.data[i + B.width - 1] = argb;
    }
    for (; i < B.size / 4; i++) B.data[i] = argb;
}

static bool draw_buffer(void)
{
    static time_t pt = 0;
    time_t ct = time(NULL);

    /* latest addition... */
    bool updated;
    if (eln[0] != eln[1]) {
	draw_border(eln[0]);
#define draw_border _draw_border_do_not_call_again_
	eln[1] = eln[0];
	updated = 1;
    }
    else updated = 0;

    if (wsn[0] != wsn[1]) {
	unsigned int ws = wsn[0] - '0'; if (ws > 9) ws = 0;
	draw_txti(nums[ws], LOC_WS);
	wsn[1] = wsn[0];
	// note to self: damage-x -- damage-width to retval (or x1,x2)
	if (ct == pt) return 1;
	// lisää vihje wev(1):sta kun lisää sen pointer-jutukkeen...
    }
    // it is enough to read battery every 1..5sec (i.e. not every "frame")
    else if (ct == pt) return updated;

    // if ct == pt) return updated; and updated = 1 in wsn block -- later //
    // then, //DP("%s %d\n", __func__, updated);

    if (pt / 300 != ct / 300) { // every 5 min //
	struct tm ctm; localtime_r(&ct, &ctm);

	draw_txti(nums[ctm.tm_mday / 10], LOC_MD1);
	draw_txti(nums[ctm.tm_mday % 10], LOC_MD2);

	draw_txti(wdays[ctm.tm_wday], LOC_WD);

	draw_txti(nums[ctm.tm_hour / 10], LOC_H1);
	draw_txti(nums[ctm.tm_hour % 10], LOC_H2);

	draw_txti(&ti_colon, LOC_HS);

	draw_txti(nums[ctm.tm_min / 10], LOC_M1);

	draw_txti(&ti_colon, LOC_MS);
	if (batfd >= 0) draw_txti(&ti_dot, LOC_B3);

	int week = iso_week(ctm.tm_year, ctm.tm_yday, ctm.tm_wday);

	draw_txti(&ti_w, LOC_W0);
	draw_txti(nums[week / 10], LOC_W1);
	draw_txti(nums[week % 10], LOC_W2);
    }
    uint8_t min0 = ct / 60 % 10;
    uint8_t sec1 = ct / 10 % 6;
    uint8_t sec0 = ct % 10;

    draw_txti(nums[min0], LOC_M2);
    //draw_txti(&ti_colon, LOC_MS);
    draw_txti(nums[sec1], LOC_S1);
    draw_txti(nums[sec0], LOC_S2);

    pt = ct;

    if (batfd >= 0) {
	static int prev_batstatus = 0;
	int batstatus = read_batstatus();

	if (batstatus == prev_batstatus)
	    return 1; //damage-x2

	if (batstatus > 0) {
	    draw_txti(&ti_plus, LOC_B0);
	}
	else {
	    draw_txti(&ti_minus, LOC_B0);
	    batstatus = -batstatus;
	}
	draw_txti(nums[(batstatus / 100) % 10], LOC_B1);
	draw_txti(nums[(batstatus / 10) % 10], LOC_B2);
	//draw_txti(&ti_dot, LOC_B3);
	draw_txti(nums[batstatus % 10], LOC_B4);

	prev_batstatus = batstatus;
    }
    //DP("%d %d\n", LOC_B0, LOC_B4);
    return 1;
}

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
    struct wl_shm_pool * pool = wl_shm_create_pool(wl_shm, fd, B.size);
    wl_buffer = wl_shm_pool_create_buffer(
	pool, 0, B.width, B.height, B.stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    // blank buffer (static inline later, in near proximity to draw_border) //
    //for (int i = 0; i < B.size / 4; i++) B.data[i] = 0xff004400;
    BB;
    int i;
    for (i = 0; i < B.size / 4 - B.width; i++) B.data[i] = 0xff000000;
    for (; i < B.size / 4; i++) B.data[i] = 0x00000000;
    for (i = 0; i < B.size / 4; i += B.width) {
	B.data[i] = B.data[i + B.width - 1] = 0x00000000;
    }
    BE;
    draw_buffer();

    wl_buffer_add_listener(wl_buffer, &wl_buffer_listener, NULL);
    return wl_buffer;
}

// layer_surface or xdg_surface //

#if USE_WLR_LAYER_SHELL

// layer_surface //

static void layer_surface_configure(void * data,
				    struct zwlr_layer_surface_v1 * surface,
				    uint32_t serial,
				    uint32_t UU(w), uint32_t UU(h))
{
    //DP("%s: %d\n", __func__, serial);
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    if (wl_buffer == NULL) {
	// xxx temp __auto_type test...
	__auto_type wl_surface = (struct wl_surface *)data;
	wl_buffer = create_buffer();
	wl_surface_attach(wl_surface, wl_buffer, 0, 0);
	__auto_type wl_region = wl_compositor_create_region(wl_compositor);
	wl_region_add(wl_region, 2, 0, B.width - 4, B.height / 3);
	wl_surface_set_input_region(wl_surface, wl_region);
	wl_surface_commit(wl_surface);
	wl_region_destroy(wl_region);
    }
}
static void layer_surface_closed(void * UU(data),
				 struct zwlr_layer_surface_v1 * UU(surface)) {

    // maeby needs other destructions first //
    //zwlr_layer_surface_v1_destroy(surface);
    exit(111);
}
struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

#else

// xdg_surface //

static void xdg_surface_configure(void * data,
				  struct xdg_surface * xdg_surface,
				  uint32_t serial)
{
    //DP("%s: %d\n", __func__, serial);
    xdg_surface_ack_configure(xdg_surface, serial);
    if (wl_buffer == NULL) {
	// xxx temp __auto_type test...
	__auto_type wl_surface = (struct wl_surface *)data;
	wl_buffer = create_buffer();
	wl_surface_attach(wl_surface, wl_buffer, 0, 0);
	__auto_type wl_region = wl_compositor_create_region(wl_compositor);
	wl_region_add(wl_region, 2, 0, B.width - 4, B.height / 3);
	wl_surface_set_input_region(wl_surface, wl_region);
	wl_surface_commit(wl_surface);
	wl_region_destroy(wl_region);
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

#endif

// pointer //

static void wl_pointer_enter(void * UU(data),
			     struct wl_pointer * UU(wl_pointer),
			     uint32_t UU(serial),
			     struct wl_surface * UU(surface),
			     wl_fixed_t UU(surface_x),
			     wl_fixed_t UU(surface_y))
{
    eln[0] = 1; //DP("enter %d %d\n", surface_x_unused, surface_y_unused);
}

static void wl_pointer_leave(void * UU(data),
			     struct wl_pointer * UU(wl_pointer),
			     uint32_t UU(serial),
			     struct wl_surface * UU(surface))
{
    eln[0] = 0; //DP("leave\n");
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

// seat //

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

// ext_workspace //

#if 0
const struct ext_workspace_group_handle_v1_listener
workspace_group_handle_listener = {
    .capabilities = noop,
    .output_enter = noop,
    .output_leave = noop,
    .workspace_enter = noop,
    .workspace_leave = noop,
    .removed = noop
};

static void ext_workspace_group(
    void * UU(data),
    struct ext_workspace_manager_v1 *ext_workspace_manager,
    struct ext_workspace_group_handle_v1 *workspace_group)
{
    //DP("%s %p %p\n", __func__, VP ext_workspace_manager, VP workspace_group);
    ext_workspace_group_handle_v1_add_listener(
	workspace_group, &workspace_group_handle_listener, NULL);
}
#endif

//static unsigned int wsn; // may be used later and zeroed...
static struct ext_workspace_handle_v1 *workspazes[9];

#if 0
static void ext_workspace_id(
    void * UU(data),
    struct ext_workspace_handle_v1 *ext_workspace_handle,
    const char *id)
{
    DP("%s %p %s\n", __func__, VP ext_workspace_handle, id);
}
#endif

/* .capabilities is used just to registed workspaces (if .state
 * did not get it first) -- so that .state can only care about
 * the 'active' cases */
static void ext_workspace_capabilities(
    void * UU(data),
    struct ext_workspace_handle_v1 *ext_workspace_handle,
    uint32_t UU(capabilities))
{
    //DP("%s %p %u\n", __func__, VP ext_workspace_handle, capabilities_unused);

    for (unsigned i = 0; i < sizeof workspazes / sizeof workspazes[0]; i++) {
	if (workspazes[i] == ext_workspace_handle) {
	    return;
	}
	if (workspazes[i] == NULL) {
	    workspazes[i] = ext_workspace_handle;
	    return;
	}
    }
}


static void ext_workspace_state(
    void * UU(data),
    struct ext_workspace_handle_v1 *ext_workspace_handle,
    uint32_t state)
{
    //DP("%s %p %u\n", __func__, VP ext_workspace_handle, state);
    if ((state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE) == 0)
	return;
    for (unsigned i = 0; i < sizeof workspazes / sizeof workspazes[0]; i++) {
	if (workspazes[i] == ext_workspace_handle) {
	    wsn[0] = i + '1'; // s/'1'/1/
	    return;
	}
	if (workspazes[i] == NULL) {
	    workspazes[i] = ext_workspace_handle;
	    wsn[0] = i + '1'; // s/'1'/1/
	    return;
	}
    }
    wsn[0] = 10; // wraps...
}

static void ext_workspace_removed(
    void * UU(data),
    struct ext_workspace_handle_v1 *ext_workspace_handle)
{
    // so far never called (nor tested separately so far...)
    //DP("%s %p\n", __func__, VP ext_workspace_handle);
    unsigned i;
    for (i = 0; i < sizeof workspazes / sizeof workspazes[0]; i++) {
	if (workspazes[i] == ext_workspace_handle) break;
    }
    while (++i < sizeof workspazes / sizeof workspazes[0]) {
	if (workspazes[i] == NULL) break;
	workspazes[i - 1] = workspazes[i];
    }
    workspazes[i - 1] = NULL;
}

const struct ext_workspace_handle_v1_listener workspace_handle_listener = {
    .id = noop, //ext_workspace_id,
    .name = noop, //ext_workspace_name,
    .coordinates = noop,
    .state = ext_workspace_state,
    .capabilities = ext_workspace_capabilities,
    .removed = ext_workspace_removed
};

static void ext_workspace(
    void * UU(data),
    struct ext_workspace_manager_v1 UU(*ext_workspace_manager),
    struct ext_workspace_handle_v1 *workspace)
{
    //DP("%s %p %p\n", __func__, VP ext_workspace_manager_unused, VP workspace);
    ext_workspace_handle_v1_add_listener(
	workspace, &workspace_handle_listener, NULL);
}

#if 0
static void ext_workspace_events_done(
    void * UU(data),
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1)
{
    DP("%s %p\n", __func__, VP ext_workspace_manager_v1);
}
#endif

const struct ext_workspace_manager_v1_listener ext_workspace_listener = {
    .workspace_group = noop, // ext_workspace_group,
    .workspace = ext_workspace,
    .done = noop, // ext_workspace_events_done,
    .finished = noop // should we care ? //
};

// wayland registry //

static void wl_registry_global(void * UU(data),
			       struct wl_registry * wl_registry,
			       uint32_t name, const char * interface,
			       uint32_t UU(version))
{
    //DP("%p %s\n", interface, interface);

    /**/ if (strcmp(interface, wl_shm_interface.name) == 0) {
	wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, wl_compositor_interface.name) == 0) {
	wl_compositor = wl_registry_bind(wl_registry, name,
					 &wl_compositor_interface, 4);
    }
#if ! USE_WLR_LAYER_SHELL
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
	xdg_wm_base = wl_registry_bind(wl_registry, name,
				       &xdg_wm_base_interface, 1);
	xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    }
#endif
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
	struct wl_seat *
	    wl_seat = wl_registry_bind(wl_registry, name,
				       &wl_seat_interface, 1);
	wl_seat_add_listener(wl_seat, &wl_seat_listener, NULL);
    }
#if USE_WLR_LAYER_SHELL
    else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
	layer_shell = wl_registry_bind(wl_registry, name,
				       &zwlr_layer_shell_v1_interface, 4);
    }
#endif
    else if (strcmp(interface, ext_workspace_manager_v1_interface.name) == 0) {
	struct ext_workspace_manager_v1 * workspace_manager
	    = wl_registry_bind(wl_registry, name,
			       &ext_workspace_manager_v1_interface, 1);

	ext_workspace_manager_v1_add_listener(workspace_manager,
					      &ext_workspace_listener, NULL);
    }
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = wl_registry_global,
    .global_remove = noop,
};

static int next_sec;
static int next_tout(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    if (ts.tv_sec >= next_sec) {
	do { next_sec += 5; } while (ts.tv_sec >= next_sec);
	return 0;
    }
    return (next_sec - ts.tv_sec) * 1000 - ts.tv_nsec / 1000000;
}


int main(int argc, char ** argv)
{
    if ((ulong)snprintf(wsfile, sizeof wsfile, "%s/%s,ws,",
			getenv("XDG_RUNTIME_DIR"), getenv("WAYLAND_DISPLAY"))
	/**/ >= sizeof wsfile)
	exit(111);
#if 0
    int wsfd = open(wsfile, O_RDONLY|O_NONBLOCK, 0);
    if (wsfd >= 0) {
	read(wsfd, &wsn, 1);
	close(wsfd);
	unlink(wsfile);
	mkfifo(wsfile, 0644);
	wsfd = open(wsfile, O_RDONLY|O_NONBLOCK, 0);
    }
#endif
    open_batfile();

    struct wl_display * wl_display = wl_display_connect(NULL);
    BB;
    struct wl_registry * wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &wl_registry_listener, NULL);

    wl_display_roundtrip(wl_display);
    BE;
    struct wl_surface *
	wl_surface = wl_compositor_create_surface(wl_compositor);
    BB;
#if USE_WLR_LAYER_SHELL
    if (layer_shell == NULL) exit(123);
    // wat was the type again ?
    __auto_type layer_surface = zwlr_layer_shell_v1_get_layer_surface(
#if 1
	layer_shell, wl_surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
#else
	layer_shell, wl_surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
#endif
	"wlroots-vai-tiili");
    zwlr_layer_surface_v1_set_size(layer_surface, B.width, B.height);
    BB;
    int margin = argc > 1? atoi(argv[1]): -150;
    int anchor, lm, rm;
    if (margin <= 0) {
	anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	lm = 0; rm = -margin;
    }
    else {
	anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	lm = margin; rm = 0;
    }
    anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
    //zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, exclusive_zone);
    zwlr_layer_surface_v1_set_margin(layer_surface, 0, rm, 0, lm);
    BE;
    //zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, ?);
    zwlr_layer_surface_v1_add_listener(layer_surface,
				       &layer_surface_listener, wl_surface);
    wl_surface_commit(wl_surface);
    // wl_display_roundtrip(display); -- to replace that dispatch loop ???
#else
    struct xdg_surface *
	xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, wl_surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, wl_surface);

    struct xdg_toplevel * xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    xdg_toplevel_set_title(xdg_toplevel, "tiili diila");
    wl_surface_commit(wl_surface);
#endif
    BE;
    while (wl_display_dispatch(wl_display) >= 0 && wl_buffer == NULL) {
	// wait for xdg_surface_configure() to create wl_buffer...
    }
    struct pollfd pfds[2];
    pfds[0].fd = wl_display_get_fd(wl_display);
    pfds[0].events = POLLIN;
    const int wsfd = -1;
    pfds[1].fd = wsfd;
    pfds[1].events = POLLIN;

    next_sec = time(NULL) + 4;
    next_sec = next_sec - next_sec % 5;
    wl_display_flush(wl_display);  // expect data fits to (100K+) socket buf)
    while (1) {
	int tout = next_tout();
	int nfds = poll(pfds, 2, tout);
	//DP("xxx %d %d %d\n", tout, next_sec, nfds);
	if (nfds) {
	    if (pfds[1].revents) {
		read(wsfd, &wsn, 1);
		close(wsfd);
		int nwsfd = open(wsfile, O_RDONLY|O_NONBLOCK, 0);
		if (nwsfd != wsfd) {
		    dup2(nwsfd, wsfd);
		    close(nwsfd);
		}
	    }
	    if (pfds[0].revents) {
		// fixme: check failure, print it
		if (wl_display_dispatch(wl_display) < 0) break;
	    }
	} else next_sec += 5;

	if (wl_buffer_released == 0)
	    continue;
	// hmm, would we care all-occluded windows (or lockscreen)
	// (xdg_toplevel::state seems to have this info (suspended))

	if (draw_buffer()) {
	    wl_buffer_released = 0;
	    wl_surface_attach(wl_surface, wl_buffer, 0, 0);
	    wl_surface_damage_buffer(wl_surface, 0, 0, 9876, 9876);
	    wl_surface_commit(wl_surface);
	    wl_display_flush(wl_display); // expect data fits to (100K+) soc...
	}
    }
    return 0;
}
