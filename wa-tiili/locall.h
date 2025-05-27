
#ifndef LOCALL_H
#define LOCALL_H

#define __USE_MISC 1
#include <sys/types.h>

// size of mmap()ed blocks
#define MMSIZ (0x400 ## 000) // 4MiB

// just FYI, what is expected...
_Static_assert(sizeof (u_short) == 2, "");
_Static_assert(sizeof (u_int) == 4, "");

struct Varz {
    u_short glyh;
    u_short glywn; // 0 1 2 3 4 5 6 7 8 9
    u_short glyww; // w
    u_short glywp; // + -
    u_short glywc; // : .
    u_short glywd; // weekdays
    u_int off_nbrs[10];
    u_int off_w;
    u_int off_plus;
    u_int off_minus;
    u_int off_colon;
    u_int off_dsep;
    u_int off_wdays[7];
    u_int off_end;
};

//void view_glyph(const char * mem, u_short w, u_short h);

_Noreturn void fill_glyphs(struct Varz * mem, const char * fcname,
                           char * wds, _Bool O, const char * wl);

#endif /* LOCALL_H */
