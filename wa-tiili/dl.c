/*
 * $ dl.c $
 *
 * Author: Tomi Ollila -- too ät iki piste fi
 *
 *      Copyright (c) 2025 Tomi Ollila
 *          All rights reserved
 *
 * Created: Sun 11 May 2025 21:32:59 EEST too
 * Last modified: Thu 15 May 2025 21:37:15 +0300 too
 */

// SPDX-License-Identifier: Unlicense

#include "more-warnings.h"

#include <err.h>

#if 1 // set to 0 for gcc -E dl.c | less ;: to comprehend/tune x-macros

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

#endif

#include <dlfcn.h>

/* https://en.wikipedia.org/wiki/X_macro */
/* in this case TABLE w/o params is simplest */

#define TABLE \
    X(FC, "libfontconfig.so"), \
    X(FT, "libfreetype.so"), \
    X(HB, "libharfbuzz.so")

#define X(_, n) n
static const char *libnames[] = { TABLE };
#undef X

#define X(e, _) e
enum DLLIBI { TABLE /*, SZ*/ };
#undef X
#undef TABLE

static void * dllibs[sizeof libnames / sizeof libnames[0]];

//static void *(*_dlfn())(enum DLLIBI dl, const char * name)
static void *_dlfn(enum DLLIBI dl, const char * name)
{
    if (dllibs[dl] == NULL) {
	const char *libname = libnames[dl];
	dllibs[dl] = dlopen(libname, RTLD_LAZY);
	if (dllibs[dl] == NULL)
	    errx(1, "Opening \"%s\" failed: %s\n", libname, dlerror());
    }
    void * sym = dlsym(dllibs[dl], name);
    if (sym == NULL)
	errx(2, "Unable to get symbol \"%s\" from \"%s\": %s\n",
	     name, libnames[dl], dlerror());
    return sym;
}

#if 0 // i hope this is never needed, but left for ref if not so.
#define vf1(dl, fn, t1, a1) \
    void fn(t1 a1) { \
	static void (*fn ## _)(t1 a1); \
	if (fn ## _ == NULL) *(void**)&fn ## _ = _dlfn(dl, #fn); \
	fn ## _ (t1 a1); \
    }
vf1(FC, FcDefaultSubstitute, FcPattern *, p)
#endif

#define f0(dl, rt, fn) \
    rt fn(void) { \
	static rt (*fn ## _)(void); \
	if (fn ## _ == NULL) *(void**)&fn ## _ = _dlfn(dl, #fn); \
	return fn ## _ (); \
    }
#define f1(dl, rt, fn, t1, a1) \
    rt fn(t1 a1) { \
	static rt (*fn ## _)(t1 a1); \
	if (fn ## _ == NULL) *(void**)&fn ## _ = _dlfn(dl, #fn); \
	return fn ## _ (a1); \
    }
#define f2(dl, rt, fn, t1, a1, t2, a2) \
    rt fn(t1 a1, t2 a2) { \
	static rt (*fn ## _)(t1 a1, t2 a2); \
	if (fn ## _ == NULL) *(void**)&fn ## _ = _dlfn(dl, #fn); \
	return fn ## _ (a1, a2); \
    }
#define f3(dl, rt, fn, t1, a1, t2, a2, t3, a3) \
    rt fn(t1 a1, t2 a2, t3 a3) { \
	static rt (*fn ## _)(t1 a1, t2 a2, t3 a3); \
	if (fn ## _ == NULL) *(void**)&fn ## _ = _dlfn(dl, #fn); \
	return fn ## _ (a1, a2, a3); \
    }
#define f4(dl, rt, fn, t1, a1, t2, a2, t3, a3, t4, a4) \
    rt fn(t1 a1, t2 a2, t3 a3, t4 a4) { \
	static rt (*fn ## _)(t1 a1, t2 a2, t3 a3, t4 a4); \
	if (fn ## _ == NULL) *(void**)&fn ## _ = _dlfn(dl, #fn); \
	return fn ## _ (a1, a2, a3, a4); \
    }
#define f5(dl, rt, fn, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5) \
    rt fn(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) { \
	static rt (*fn ## _)(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5); \
	if (fn ## _ == NULL) *(void**)&fn ## _ = _dlfn(dl, #fn); \
	return fn ## _ (a1, a2, a3, a4, a5); \
    }

//#pragma GCC diagnostic ignored "-Wpedantic" // the void return and fn ptr //

// fontconfig/fontconfig.h //

f1(FC, FcPattern *, FcNameParse, const FcChar8 *, name)

f3(FC, FcBool, FcConfigSubstitute,
   FcConfig *, c, FcPattern *, p, FcMatchKind, k)

#define return
f1(FC, void, FcDefaultSubstitute, FcPattern *, p)
f1(FC, void, FcPatternDestroy, FcPattern *, p)
f1(FC, void, FcPatternPrint, const FcPattern *, p)
#undef return

f3(FC, FcPattern *, FcFontMatch, FcConfig *, c, FcPattern *, p, FcResult *, r)

f4(FC, FcResult, FcPatternGetString,
   const FcPattern *, p, const char *, object, int, n, FcChar8 **, s)

f4(FC, FcResult, FcPatternGetDouble,
   const FcPattern *, p, const char *, object, int, n, double *, d)

f3(FC, FcBool, FcPatternAddString,
   FcPattern *, p, const char *, object, const FcChar8 *, d)

f3(FC, FcBool, FcPatternAddDouble,
   FcPattern *, p, const char *, object, double, d)

// freetype2/freetype/freetype.h //

f3(FT, FT_Error, FT_Load_Glyph,
   FT_Face, f, FT_UInt, glyph_index, FT_Int32, load_flags)

f2(FT, FT_Error, FT_Render_Glyph, FT_GlyphSlot, s, FT_Render_Mode, m)

f1(FT, FT_Error, FT_Init_FreeType, FT_Library *, ft_library)
//fz(FT, FT_Error, FT_New_Face_From_FSRef, (FT_Library));

f4(FT, FT_Error, FT_New_Face,
   FT_Library, l, const char *, filepathname,
   FT_Long, face_index, FT_Face *, aface)

f3(FT, FT_Error, FT_Set_Pixel_Sizes,
   FT_Face, face, FT_UInt, pixel_width, FT_UInt, pixel_height)

f1(FT, FT_Error, FT_Done_FreeType, FT_Library, ft_library)

// harfbuzz/*.h //

f1(HB, hb_font_t *, hb_ft_font_create_referenced, FT_Face, ft_face)
//f1...hb_ft_font_set_funcs(hb_ft_font); //not needed

f0(HB, hb_buffer_t *, hb_buffer_create)

#define return

f5(HB, void, hb_buffer_add_utf8,
   hb_buffer_t *, buffer, const char *, text,
   int, text_length, unsigned int, item_offset, int, item_length)

f1(HB, void, hb_buffer_guess_segment_properties, hb_buffer_t *, buffer)

f4(HB, void, hb_shape,
   hb_font_t *, font, hb_buffer_t *, buffer,
   const hb_feature_t *, features, unsigned int, num_features)

f1(HB, void, hb_buffer_clear_contents, hb_buffer_t *, buf)

f1(HB, void, hb_buffer_destroy, hb_buffer_t *, buf)

f1(HB, void, hb_font_destroy, hb_font_t *, hb_ft_font)

#undef return

f2(HB, hb_glyph_info_t *, hb_buffer_get_glyph_infos,
   hb_buffer_t *, buffer, unsigned int *, length)

f2(HB, hb_glyph_position_t *, hb_buffer_get_glyph_positions,
   hb_buffer_t *, buffer, unsigned int *, length)
