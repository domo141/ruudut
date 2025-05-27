
// SPDX-License-Identifier: BSD 2-Clause "Simplified" License

#define _POSIX_C_SOURCE 200112L
#include "more-warnings.h"

#include <unistd.h>
#include <locale.h>
#include <time.h>
#include <err.h>
#include <signal.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#pragma GCC diagnostic pop

#include <hb.h>
#include <hb-ft.h>

#include <fontconfig/fontconfig.h>

#include "locall.h"

// (variable) block begin/end -- explicit liveness so that some variables
// that aren't to be accessed anymore cannot be accidentally referenced...
#define BB {
#define BE }

#if 0
#define df printf
#define df1 printf
#else
#define df(...) do {} while (0)
#define df1(...) do {} while (0)
#endif
#define df0(...) do {} while (0)

#if 0
void view_glyph(const char * mem, u_short w, u_short h)
{
    df("%s %dx%d\n", __func__, w, h);
    for (int y = 0; y < h; y++) {
	df("%2d ", y);
	putchar('|');
	for (int x = 0; x < w; x++)
	{
	    uint8_t c = mem[y * w + x];
	    putchar((c >= 192)? '#': (c >= 128)? '+':
		    (c >= 64)? ':': (c) ? '.': ' ');
	}
	putchar('|'); putchar('\n');
    }
}
#endif

#pragma GCC diagnostic ignored "-Waggregate-return"

struct tb { u_short t; u_short b; };
static
struct tb
glyph_tb(const char * mem, u_short w, u_short h, u_short ht, u_short hb)
{
    for (int y = 0; y < ht; y++)
	for (int x = 0; x < w; x++)
	    if (mem[y * w + x] != 0) { ht = y; goto n; }
 n:
    for (int y = h - 1; y > hb; y--)
	for (int x = 0; x < w; x++)
	    if (mem[y * w + x] != 0) { hb = y; goto o; }
 o:
    //df("/// %d %d\n", ht, hb);
    return (struct tb){.t = ht, .b = hb};
}

static
char * cpimg(char * dmem, char * smem, u_short w, u_short dw,
	     u_short tl, u_short bl)
{
    //df("cpimg(): w %u dw %u tl %u bl %u\n", w, dw, tl, bl);

    int l = (dw + 1 - w) / 2;
    int r = (dw + 0 - w) / 2;
    int x; // for code layout ;D //
    for (int y = tl; y <= bl; y++) {
	for (x = 0; x < l; x++) *dmem++ = '\0';
	for (x = 0; x < w; x++) *dmem++ = smem[w * y + x]; // me forgot memcpy?
	for (x = 0; x < r; x++) *dmem++ = '\0';
    }
    return (char*)((intptr_t)(dmem + 3) & ~3);
}

static u_short
do_glyphs(hb_font_t * hb_ft_font, hb_buffer_t * buf, FT_Face ft_face,
	  const char * txt, /*u_short h,*/ u_short bl, char * mem)
{
    hb_buffer_clear_contents(buf);

    //hb_buffer_add_utf8(buf, txt, strlen(txt), 0, strlen(txt));
    hb_buffer_add_utf8(buf, txt, -1, 0, -1);

    hb_buffer_guess_segment_properties(buf);
    hb_shape(hb_ft_font, buf, NULL, 0);

    unsigned int glyph_count;
    hb_glyph_info_t * glyph_info
	= hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t * glyph_pos
	= hb_buffer_get_glyph_positions(buf, &glyph_count);

    u_short wdth = 0;
    for (unsigned int i = 0; i < glyph_count; i++)
	wdth += (glyph_pos[i].x_advance >> 6);
    wdth += 1;
    //df("txt: '%s', w: %d\n", txt, wdth); //exit(1);

    //hb_position_t cursor_x = 0;
    //hb_position_t cursor_y = 0;

    for (unsigned int i = 0; i < glyph_count; i++) {
	hb_codepoint_t glyphid  = glyph_info[i].codepoint;

	FT_Error error = FT_Load_Glyph(ft_face, glyphid, FT_LOAD_DEFAULT);
	if (error) errx(1, "FT_Load_Glyph() failed: %d", error);

	FT_GlyphSlot slot = ft_face->glyph;
	error = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	if (error) errx(1, "FT_Render_Glyph() failed: %d", error);

	FT_Bitmap ft_bitmap = slot->bitmap;
	FT_Int x_max = ft_bitmap.width;
	FT_Int y_max = ft_bitmap.rows;

	// if rendered image is larger than expected, then overwrites
	// may happen in the same or next images. the memory buffer is
	// not (easily) overwritten (with (caught) sigsegv) though

	//hb_position_t y_advance = glyph_pos[i].y_advance;
	// so far not used...
	//hb_position_t x_offset  = glyph_pos[i].x_offset;
	//hb_position_t y_offset  = glyph_pos[i].y_offset;

	short xx = slot->bitmap_left;
	short yy = bl - slot->bitmap_top;

	//df("yy: %d, bl %u, t: %u\n", yy, bl, slot->bitmap_top); //exit(1);
	if (yy < 0)
	    errx(1, "programmer could not figure out \"baseline\" (%d)", yy);

	for (int y = 0; y < y_max; y++)
	    for (int x = 0; x < x_max; x++)
		mem[(y+yy) * wdth + x+xx] = ft_bitmap.buffer[y * x_max + x];

	mem += glyph_pos[i].x_advance >> 6;
    }
    return wdth;
}

static void sighandler(int sig)
{
    const char * msg = (sig == SIGSEGV) ?
	"SIGSEGV - font too large or buggy code" : "SIGBUS - bug there?";
    errx(1, "kippas (%s)", msg);
}

void fill_glyphs(struct Varz * varz, const char * fcname,
		 char * wds, _Bool O, const char * wl)
{
    signal(SIGSEGV, sighandler); // with large enough glyphs 4MiB not enough
    signal(SIGBUS, sighandler); // unlikely but silent if not caught...

    if (strchr(wds, ',') != NULL) {
	int i; char * p;
	for (i = 0, p = wds; i < 6; i++, p++) {
	    p = strchr(p, ',');
	    *p++ = '\0';
	}
    }
    else {
	//char * ol = setlocale(LC_TIME, "sv_SE.UTF-8");
	setlocale(LC_TIME, wds);
	// there is ~128k space (before initial pos of char * mem) //
	char *p = (char *)(varz + 1);
	struct tm tm = { 0 };
	for (int i = 0; i < 7; i++) {
	    tm.tm_wday = i;
	    strftime(p, 256, "%a", &tm);
	    //df("wday: %s\n", p);
	    p += strlen(p) + 1;
	}
	wds = (char *)(varz + 1);
	//setlocale(LC_TIME, ol);
	//setlocale(LC_TIME, "sv_SE.UTF-8");
    }

    FT_Library ft_library;
    FT_Face ft_face;
    u_short bline, imgh;
    BB;
    FcPattern * p = FcNameParse((const FcChar8 *)fcname);
    if (p == NULL) errx(1, "cannot parse fontconfig pattern from %s\n",fcname);
    FcConfigSubstitute(0, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    // iirc there was some "fallback" code but cannot find it anymore...
    //FcPatternAddString(p, FC_FAMILY, (const FcChar8 *)"Monospace-20");
    FcResult result;
    FcPattern * fc_pat = FcFontMatch(0, p, &result);
    FcPatternDestroy(p);
    if (fc_pat == NULL) errx(1, "cannot match font from %s", fcname);

    if (getenv("FcPatternPrint") != NULL)
	FcPatternPrint(fc_pat); //exit(0);

    FT_Init_FreeType(&ft_library);

    FcChar8 * fc_file;
    if (FcPatternGetString(fc_pat, FC_FILE, 0, &fc_file) != FcResultMatch)
	errx(1, "cannot get font file for font %s", fcname);

    //df("fc_file %s\n", fc_file); // FcPatternPrint() prints it too...

    double pixsize;
    if (FcPatternGetDouble(fc_pat, FC_PIXEL_SIZE, 0, &pixsize) != FcResultMatch)
	errx(1, "cannot get pixel size for font %s", fcname);

    if (pixsize < 5)
	errx(1, "font pixel size %f suspiciously small (< 5)", pixsize);

    FT_New_Face(ft_library, (const char *)fc_file, 0, &ft_face);

    FT_Set_Pixel_Sizes(ft_face, 0, (unsigned int)pixsize);

#if 0
    df1("pix size %f\n", pixsize);

    df1("in font units: a %d d %d h %d maw %d - a*p/h %f\n",
	ft_face->ascender, ft_face->descender,
	ft_face->height, ft_face->max_advance_width,
	ft_face->ascender * pixsize / ft_face->height);

    df1("bbox xmin %ld xmax %ld  ymin %ld ymax %ld (?""?)\n",
	ft_face->bbox.xMin, ft_face->bbox.xMax,
	ft_face->bbox.yMin, ft_face->bbox.yMax);
#endif
    // baseline calc w/o understanding "font units"... // fail laterz
    // w/ some fonts, slot->bitmap_top was bigger than "baseline" counted
    // from ascender and height -- in these cases ascender +- descender
    // did not equal height -- in these cases add the gap to height...
    int ax = ft_face->descender;
    if (ax < 0) ax = -ax;
    ax = ft_face->height - ax;
    if (ax < ft_face->ascender) ax = ft_face->ascender;

    // ...ŐÖ... or -fHack-24 -dsv_SE.UTF-8 (and then -fHack-160)
    int px = (int)(pixsize / 7) + 2; // trial & err - try learn more (someday?)
    bline = (u_short)(ax * pixsize / ft_face->height) + px;
    imgh = (u_short)pixsize + px; // imgh * width + 1024 used in mem advance...

    df("ax: %d, fac: %d, ", ax, ft_face->ascender);
    df("bline %d imgh %d\n", bline, imgh);

    FcPatternDestroy(fc_pat);
    BE;

    hb_font_t * hb_ft_font = hb_ft_font_create_referenced(ft_face);
    //hb_ft_font_set_funcs(hb_ft_font); //not needed
    hb_buffer_t * buf = hb_buffer_create();

    char * mem = ((char *)varz) + (128 * 1024);

    /* rest of this fn avaits polishment (which may not happen too soon...) */

#if 0
#define ADV_MEM(w) view_glyph(mem + tb.t * w, w, tb.b - tb.t + 1); \
    mem = (char*)((intptr_t)(mem + imgh * w + 1024) & ~3); // exit(12);
#else
#define ADV_MEM(w) \
    mem = (char*)((intptr_t)(mem + imgh * w + 1024) & ~3);
#endif

    struct tb tb = { imgh, 0 };
    u_short wn_max = 0;
    u_short numwidths[10];
    void * nummems[10];
    BB;
    char n[] = "0\0""1\0""2\0""3\0""4\0""5\0""6\0""7\08\09";
    if (O) n[0] = 'O';
    for (unsigned t = 0; t < 10; t++) {
	u_short w = do_glyphs(hb_ft_font, buf, ft_face, &n[t<<1], bline, mem);
	if (w > wn_max) wn_max = w;
	numwidths[t] = w;
	nummems[t] = mem;
	tb = glyph_tb(mem, w, imgh, tb.t, tb.b);
	ADV_MEM(w);
    }
    BE;
    void * memw = mem;
    u_short ww = do_glyphs(hb_ft_font, buf, ft_face, wl, bline, mem);
    tb = glyph_tb(mem, ww, imgh, tb.t, tb.b);
    ADV_MEM(ww);

    void * memp = mem;
    u_short wp = do_glyphs(hb_ft_font, buf, ft_face, "+", bline, mem);
    tb = glyph_tb(mem, wp, imgh, tb.t, tb.b);
    ADV_MEM(wp);

    void * memm = mem;
    u_short wm = do_glyphs(hb_ft_font, buf, ft_face, "-", bline, mem);
    tb = glyph_tb(mem, wm, imgh, tb.t, tb.b);
    ADV_MEM(wm);

    void * memc = mem;
    u_short wc = do_glyphs(hb_ft_font, buf, ft_face, ":", bline, mem);
    tb = glyph_tb(mem, wc, imgh, tb.t, tb.b);
    ADV_MEM(wc);

    void * memd = mem;
    u_short wd = do_glyphs(hb_ft_font, buf, ft_face, ".", bline, mem);
    tb = glyph_tb(mem, wd, imgh, tb.t, tb.b);
    ADV_MEM(wd);

    u_short wd_max = 0;
    u_short wdwidths[7];
    void * wdmems[7];
    BB;
    char * p = wds;
    for (unsigned t = 0; t < 7; t++) {
	u_short w = do_glyphs(hb_ft_font, buf, ft_face, p, bline, mem);
	p += strlen(p) + 1;
	if (w > wd_max) wd_max = w;
	wdwidths[t] = w;
	wdmems[t] = mem;
	tb = glyph_tb(mem, w, imgh, tb.t, tb.b);
	ADV_MEM(w);
    }
    BE;
    hb_buffer_destroy(buf);
    hb_font_destroy(hb_ft_font);
    FT_Done_FreeType(ft_library);

    df("mem used: %ld\n", mem - (char *)varz);
    mem = (char *)&varz[1];
    char * msp = mem;

    for (int i = 0; i < 10; i++) {
	varz->off_nbrs[i] = mem - msp;
	mem = cpimg(mem, nummems[i], numwidths[i], wn_max, tb.t, tb.b);
    }

    varz->off_w = mem - msp;
    mem = cpimg(mem, memw, ww, ww, tb.t, tb.b);

    BB;
    u_short wx = wp > wm? wp: wm;
    varz->off_plus = mem - msp;
    mem = cpimg(mem, memp, wp, wx, tb.t, tb.b);
    varz->off_minus = mem - msp;
    mem = cpimg(mem, memm, wm, wx, tb.t, tb.b);
    varz->glywp = wp;
    BE BB;
    u_short wx = wc > wd? wc: wd;
    varz->off_colon = mem - msp;
    mem = cpimg(mem, memc, wc, wx, tb.t, tb.b);
    varz->off_dsep = mem - msp;
    mem = cpimg(mem, memd, wd, wx, tb.t, tb.b);
    varz->glywc = wc;
    BE;

    for (int i = 0; i < 7; i++) {
	varz->off_wdays[i] = mem - msp;
	mem = cpimg(mem, wdmems[i], wdwidths[i], wd_max, tb.t, tb.b);
    }
    varz->off_end = mem - msp;

    df("off end:  %ld\n", mem - msp); //exit(0);

    varz->glywn = wn_max;
    varz->glywd = wd_max;
    varz->glyww = ww;

    varz->glyh = tb.b - tb.t + 1;
    _exit(0);
}
