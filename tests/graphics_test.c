/*******************************************************************************
*                                                                              *
*                           GRAPHICS TEST PROGRAM                              *
*                                                                              *
*                    Copyright (C) 2021 Scott A. Franco                        *
*                                                                              *
* Tests various single window, unmanaged graphics.                             *
*                                                                              *
* Benchmark results, Windows, Athlon 64 3200+, BFG 6800 overclock:             *
*                                                                              *
* Type                        Seconds     Per fig                              *
* --------------------------------------------------                           *
* line width 1                     7.484    7.484e-5                           *
* line width 10                   10.906   .00010906                           *
* rectangle width 1                7.313    7.313e-5                           *
* rectangle width 10               8.219    8.219e-5                           *
* rounded rectangle width 1       12.781   .00012781                           *
* rounded rectangle width 10      15.953   .00015953                           *
* filled rectangle                15.516   1.5516e-5                           *
* filled rounded rectangle         8.906    8.906e-5                           *
* ellipse width 1                 17.437   .00017437                           *
* ellipse width 10                22.078   .00022078                           *
* filled ellipse                  13.297   .00013297                           *
* arc width 1                      9.719    9.719e-5                           *
* arc width 10                    12.125   .00012125                           *
* filled arc                      10.422   .00010422                           *
* filled chord                      8.89     8.89e-5                           *
* filled triangle                 19.172   1.9172e-5                           *
* text                            10.922   .00010922                           *
* background invisible text       10.703   .00010703                           *
*                                                                              *
* Benchmark results, Windows, AMD Phenom, 2.51 GHZ, Nvidia GeForce 9800 GT     *
*                                                                              *
* Type                     Seconds   Per fig                                   *
* --------------------------------------------------                           *
* line width 1                 6.71    0.000067                                *
* line width 10                7.37    0.000073                                *
* rectangle width 1           10.45    0.000104                                *
* rectangle width 10          11.98    0.000119                                *
* rounded rectangle width 1   13.35    0.000133                                *
* rounded rectangle width 10  16.00    0.000160                                *
* filled rectangle             8.20    0.000082                                *
* filled rounded rectangle    20.98    0.000209                                *
* ellipse width 1             14.96    0.000149                                *
* ellipse width 10            17.62    0.000176                                *
* filled ellipse              22.76    0.000227                                *
* arc width 1                 11.03    0.000110                                *
* arc width 10                12.78    0.000127                                *
* filled arc                  16.15    0.000161                                *
* filled chord                14.53    0.000145                                *
* filled triangle             24.14    0.000241                                *
* text                        10.89    0.000435                                *
* background invisible text   10.95    0.000438                                *
* Picture draw                23.82    0.002382                                *
* No scaling picture draw     13.82    0.001382                                *
*                                                                              *
* Benchmark results, Linux AMD Ryzen 9 3950x, Nvidia GTX 3070 Dual             *
*                                                                              *
* Type                        Seconds  Per fig                                 *
* --------------------------------------------------                           *
* line width 1                 14.29     0.000008                              *
* line width 10                16.66     0.000008                              *
* rectangle width 1            28.87     0.000050                              *
* rectangle width 10            0.57     0.000004                              *
* rounded rectangle width 1    30.03     0.015296                              *
* rounded rectangle width 10    9.61     0.015297                              *
* filled rectangle              3.15     0.000648                              *
* filled rounded rectangle     24.86     0.000515                              *
* ellipse width 1               8.67     0.002041                              *
* ellipse width 10              3.63     0.002612                              *
* filled ellipse                3.12     0.001142                              *
* arc width 1                 174.00     0.003408                              *
* arc width 10                  1.92     0.002903                              *
* filled arc                    2.76     0.001056                              *
* filled chord                  1.19     0.000212                              *
* filled triangle               2.69     0.000013                              *
* text                         26.44     0.000056                              *
* background invisible text    41.39     0.000067                              *
* Picture draw                 15.04     0.004101                              *
* No scaling picture draw       0.73     0.000134                              *
*                                                                              *
* Benchmark results, Linux Intel I7-885U CPU 1.80 GHZ, Nvidia GPM108M          *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                 16.04    0.000009                               *
* line width 10                11.97    0.000009                               *
* rectangle width 1           332.57    0.000051                               *
* rectangle width 10            0.81    0.000004                               *
* rounded rectangle width 1    29.90    0.023449                               *
* rounded rectangle width 10    1.00    0.009552                               *
* filled rectangle              0.00    0.000000                               *
* filled rounded rectangle      1.37    0.003389                               *
* ellipse width 1              70.63    0.003926                               *
* ellipse width 10              8.46    0.003923                               *
* filled ellipse                4.60    0.002363                               *
* arc width 1                  90.90    0.004507                               *
* arc width 10                  7.96    0.004288                               *
* filled arc                    1.22    0.002628                               *
* filled chord                  1.57    0.001941                               *
* filled triangle               1.01    0.000020                               *
* text                         31.97    0.000070                               *
* background invisible text    36.30    0.000094                               *
* Picture draw                  7.30    0.002764                               *
* No scaling picture draw       2.61    0.000224                               *
*                                                                              *
* Benchmark results, Windows AMD Ryzen 9 3950x, Nvidia GTX 3070 Dual           *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                 15.07    0.000046                               *
* line width 10                14.70    0.000049                               *
* rectangle width 1            15.10    0.000065                               *
* rectangle width 10           15.00    0.000072                               *
* rounded rectangle width 1    14.93    0.000077                               *
* rounded rectangle width 10   14.93    0.000090                               *
* filled rectangle             14.95    0.000048                               *
* filled rounded rectangle     14.90    0.000088                               *
* ellipse width 1              15.06    0.000088                               *
* ellipse width 10             14.73    0.000103                               *
* filled ellipse               14.98    0.000108                               *
* arc width 1                  15.06    0.000066                               *
* arc width 10                 15.01    0.000066                               *
* filled arc                   15.09    0.000075                               *
* filled chord                 14.78    0.000071                               *
* filled triangle              14.93    0.000106                               *
* text                         15.29    0.000304                               *
* background invisible text    15.01    0.000305                               *
* Picture draw                 15.07    0.010894                               *
* No scaling picture draw      14.56    0.000971                               *
*                                                                              *
* Benchmark results, Mac OS X/Mac Mini, X86 core, via XQuartz XWindows         *
* emulation                                                                    *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                 13.08    0.000058                               *
* line width 10                11.62    0.000068                               *
* rectangle width 1            16.14    0.000016                               *
* rectangle width 10           14.05    0.000045                               *
* rounded rectangle width 1    14.63    0.080387                               *
* rounded rectangle width 10    0.00    0.000001                               *
* filled rectangle              7.98    0.009935                               *
* filled rounded rectangle     14.24    0.003286                               *
* ellipse width 1              55.30    0.007777                               *
* ellipse width 10              8.00    0.007349                               *
* filled ellipse                9.07    0.008989                               *
* arc width 1                  19.86    0.009687                               *
* arc width 10                  0.00    0.000000                               *
* filled arc                    6.30    0.006423                               *
* filled chord                 15.86    0.000769                               *
* filled triangle              18.54    0.000163                               *
* text                         16.35    0.000114                               *
* background invisible text    13.68    0.000061                               *
* Picture draw                  0.32    0.002281                               *
* No scaling picture draw       5.66    0.002523                               *
*                                                                              *
* Benchmark results, Mac OS X/Mac Mini, Apple M4 chip.                         *
* emulation                                                                    *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                  5.01    0.000004                               *
* line width 10                 5.02    0.000005                               *
* rectangle width 1             5.02    0.000004                               *
* rectangle width 10            5.03    0.000005                               *
* rounded rectangle width 1     5.01    0.000008                               *
* rounded rectangle width 10    5.03    0.000020                               *
* filled rectangle              4.99    0.000004                               *
* filled rounded rectangle      5.02    0.000009                               *
* ellipse width 1               4.98    0.000009                               *
* ellipse width 10              5.03    0.000022                               *
* filled ellipse                4.96    0.000009                               *
* arc width 1                   5.00    0.000005                               *
* arc width 10                  5.00    0.000005                               *
* filled arc                    4.98    0.000006                               *
* filled chord                  5.01    0.000006                               *
* filled triangle               5.00    0.000008                               *
* text                          5.01    0.000035                               *
* background invisible text     5.00    0.000031                               *
* Picture draw                  5.00    0.000275                               *
* No scaling picture draw       4.98    0.000007                               *
*                                                                              *
* Benchmark results, Linux AMD Ryzen 9 5950x 16-core x 32,                     *
* Ubuntu 20.04.6 LTS                                                           *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                   5.04    0.000004                              *
* line width 10                  4.88    0.000004                              *
* rectangle width 1              5.98    0.000047                              *
* rectangle width 10             4.93    0.000001                              *
* rounded rectangle width 1      5.28    0.011113                              *
* rounded rectangle width 10     9.24    0.012761                              *
* filled rectangle               5.07    0.000003                              *
* filled rounded rectangle       4.85    0.000381                              *
* ellipse width 1                5.77    0.002357                              *
* ellipse width 10               5.11    0.002265                              *
* filled ellipse                 5.22    0.000090                              *
* arc width 1                    0.62    0.000508                              *
* arc width 10                   5.20    0.002421                              *
* filled arc                     5.17    0.000125                              *
* filled chord                   4.95    0.000127                              *
* filled triangle                5.03    0.000010                              *
* text                           4.81    0.000038                              *
* background invisible text      5.47    0.000108                              *
* Picture draw                   4.42    0.003874                              *
* No scaling picture draw        4.91    0.000102                              *
*                                                                              *
* Benchmark results, Linux AMD Ryzen 9 5950x 16-core x 32,                     *
* Ubuntu 20.04.6 LTS, remote display: graph_client to                          *
* graph_server over the local socket connection                                *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                   5.16    0.000004                              *
* line width 10                  5.05    0.000004                              *
* rectangle width 1              0.57    0.000006                              *
* rectangle width 10             5.05    0.000001                              *
* rounded rectangle width 1      1.25    0.001468                              *
* rounded rectangle width 10     1.94    0.002115                              *
* filled rectangle               0.00    0.000000                              *
* filled rounded rectangle       0.70    0.003780                              *
* ellipse width 1                2.63    0.024387                              *
* ellipse width 10               2.85    0.007868                              *
* filled ellipse                 6.19    0.008128                              *
* arc width 1                    1.89    0.001886                              *
* arc width 10                   0.00    0.000001                              *
* filled arc                     3.00    0.000345                              *
* filled chord                   4.96    0.000131                              *
* filled triangle                5.02    0.000009                              *
* text                           5.03    0.000036                              *
* background invisible text      5.48    0.000103                              *
* Picture draw                   1.22    0.001278                              *
* No scaling picture draw        0.00    0.000000                              *
*                                                                              *
* Benchmark results, Linux AMD Ryzen 9 5950x 16-core x 32,                     *
* Ubuntu 26.04 LTS, native Wayland backend (linux/wayland, pdisplay -O3),      *
* GNOME Mutter compositor, pdisplay at -O3                                     *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                   4.88    0.000001                              *
* line width 10                  5.03    0.000006                              *
* rectangle width 1              5.01    0.000003                              *
* rectangle width 10             5.02    0.000011                              *
* rounded rectangle width 1      4.45    0.000008                              *
* rounded rectangle width 10     4.74    0.000045                              *
* filled rectangle               4.98    0.000003                              *
* filled rounded rectangle       5.28    0.000035                              *
* ellipse width 1                5.05    0.000004                              *
* ellipse width 10               4.81    0.000005                              *
* filled ellipse                 4.73    0.000005                              *
* arc width 1                    5.41    0.000009                              *
* arc width 10                   5.00    0.000009                              *
* filled arc                     5.03    0.000060                              *
* filled chord                   4.95    0.000062                              *
* filled triangle                5.00    0.000014                              *
* text                           5.27    0.000004                              *
* background invisible text      4.68    0.000003                              *
* Picture draw                   6.23    0.000802                              *
* No scaling picture draw        5.46    0.000010                              *
*                                                                              *
* Benchmark results, Linux AMD Ryzen 9 5950x 16-core x 32,                     *
* Ubuntu 26.04 LTS, Xlib backend under XWayland,                               *
* GNOME Mutter compositor                                                      *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                   4.39    0.000011                              *
* line width 10                  4.35    0.000011                              *
* rectangle width 1              5.24    0.000053                              *
* rectangle width 10             5.04    0.000018                              *
* rounded rectangle width 1      0.00    0.000000                              *
* rounded rectangle width 10     8.83    0.011639                              *
* filled rectangle               5.11    0.000009                              *
* filled rounded rectangle       4.44    0.000510                              *
* ellipse width 1                5.15    0.002032                              *
* ellipse width 10               6.38    0.002170                              *
* filled ellipse                 0.36    0.000150                              *
* arc width 1                    5.00    0.002236                              *
* arc width 10                   5.23    0.002526                              *
* filled arc                     5.04    0.000171                              *
* filled chord                   5.01    0.000171                              *
* filled triangle                5.08    0.000012                              *
* text                           5.19    0.003722                              *
* background invisible text      5.28    0.003697                              *
* Picture draw                   5.11    0.003563                              *
* No scaling picture draw        5.41    0.000141                              *
*                                                                              *
* Benchmark results, Linux AMD Ryzen 9 5950x 16-core x 32,                     *
* Ubuntu 26.04 LTS, GTK4/Cairo edition (tests/graphics_test_gtk.c):            *
* Cairo image surface, antialiasing off, shapes rasterized offscreen           *
* and presented only between benchmarks (no display cost in the                *
* timed loops, unlike the native Wayland rows above)                           *
*                                                                              *
* Type                        Seconds   Per fig                                *
* --------------------------------------------------                           *
* line width 1                   4.58    0.000003                              *
* line width 10                  4.75    0.000005                              *
* rectangle width 1              4.99    0.000001                              *
* rectangle width 10             5.03    0.000002                              *
* rounded rectangle width 1      5.02    0.000007                              *
* rounded rectangle width 10     4.58    0.000010                              *
* filled rectangle               4.72    0.000006                              *
* filled rounded rectangle       5.03    0.000010                              *
* ellipse width 1                5.02    0.000017                              *
* ellipse width 10               5.49    0.000024                              *
* filled ellipse                 5.02    0.000016                              *
* arc width 1                    5.27    0.000010                              *
* arc width 10                   4.74    0.000014                              *
* filled arc                     5.02    0.000012                              *
* filled chord                   5.17    0.000011                              *
* filled triangle                5.01    0.000017                              *
* text                           5.30    0.000002                              *
* background invisible text      4.70    0.000002                              *
* Picture draw                   5.56    0.001404                              *
* No scaling picture draw        5.04    0.000121                              *
*                                                                              *
*******************************************************************************/
/*******************************************************************************

Usage:

    graphics_test [auto [file]] [events] [first [last]]

    auto   Walks every pattern with no input and exits after them: the
           regression mode. A following non-numeric argument names the
           file the screens are captured to. The patterns driven by the
           user take their events from tests/graphics_test.evt (see
           doc/auto_events.md).
    events Takes the events of tests/graphics_test.evt in an interactive
           run as well, where the file has them for the frame at hand.
    first  The first frame to stop on. The patterns before it pass
           without a stop or capture.
    last   The last frame; the run ends after it. Left off, the run
           goes from the first frame to the end of the test.

    graphics_test bench

    Runs the benchmark section alone.

Each pattern stamps "frame N" centered on its top line, so a frame can
be called out by number and revisited alone: graphics_test 146 stops
on frame 146 and every one after, graphics_test 5 8 on frames 5
through 8 alone.

*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <services.h>
#include <graphics.h>

/* externals */
extern void screen_capture(void);
extern void screen_capture_name(const char* fn);
extern void screen_capture_label(const char* s);
extern void auto_event_name(const char* fn);
extern void auto_event_beside(const char* capfile, const char* name);
extern void auto_event_frame(int frame, int step);
extern int  auto_event_ready(void);
extern void auto_event(FILE* f, ami_evtrec* er);

#define EVENTNAME "graphics_test.evt"  /* the events of the automatic run */
#define EVENTFILE "tests/" EVENTNAME    /* where they are from the repository */

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); \
                                fflush(stderr); } while (0)

#define S1     "Moving string"
#define S2     "Variable size string"
#define S3     "Sizing test string"
#define S4     "Justify test string"
#define S5     "Invisible body text"
#define S6     "Example text"
#define COLDIV 6 /* number of color divisions */
#define AUTOSTEP 8 /* pixels the moving string steps per frame in automatic runs;
                      1 is the interactive pace, and about 1100 captured steps */
#define COLSQR 20 /* size of color square */
#define OFF    FALSE
#define ON     TRUE
#define DEGREE (LONG_MAX/360)
#define SECOND 10000 /* one second elapsed time */

typedef enum {

    bnline1,     /* line width 1 */
    bnline10,    /* line width 10 */
    bnrect1,     /* rectangle width 1 */
    bnrect10,    /* rectangle width 10 */
    bnrrect1,    /* rounded rectangle width 1 */
    bnrrect10,   /* rounded rectangle width 10 */
    bnfrect,     /* filled rectangle */
    bnfrrect,    /* filled rounded rectangle */
    bnellipse1,  /* ellipse width 1 */
    bnellipse10, /* ellipse width 10 */
    bnfellipse,  /* filled ellipse */
    bnarc1,      /* arc width 1 */
    bnarc10,     /* arc width 10 */
    bnfarc,      /* filled arc */
    bnfchord,    /* filled chord */
    bnftriangle, /* filled triangle */
    bntext,      /* text */
    bntextbi,    /* background invisible text */
    bnpict,      /* picture draw */
    bnpictns     /* no scale picture draw */

} bench;

static jmp_buf   terminate_buf;
static int       framenum = 0;
static int       tstlo = 1;      /* first frame in the selected range */
static int       tsthi = 0;      /* last frame, 0 for no limit */
static int       stepnum = 0;    /* animation step within the frame */
/* "auto" on the command line: walk every pattern without waiting for a
   keypress, so the run can be compared against a standard by the
   regression harness. Every wait for the user answers at once, the
   patterns that are driven by the user (scrolling, the mouse, the
   viewport) show their first screen and pass on, and the benchmarks,
   whose output is timings and never the same twice, are left out. */
static int       autorun = FALSE;
static char      fns[100];
static int       x, y;
static int       xs, ys;
static int       i;
static int       dx, dy;
static int       ln;
static int       term;
static int       bright, bbot;   /* the moving string has bounced right, bottom */
static int       w;
static int       l;
static ami_long  a;
static ami_long  r, g, b;
static ami_color  c, c1, c2;
static int       lx, ly;
static int       tx1, ty1, tx2, ty2, tx3, ty3;
static int       h;
static int       cnt;
static ami_evtrec er;
static int       fsiz;
static ami_long  aa, ab;
static ami_long  s;
static struct { /* benchmark stats records */

    int  iter; /* number of iterations performed */
    ami_long time; /* time in 100us for test */

} benchtab[bnpictns+1];
static bench bi;

/* Find random number between 0 and N. */

static ami_long randn(ami_long limit)

{

    return (double)limit*rand()/RAND_MAX;

}

/* Find random number in given range */

static ami_long randr(ami_long s, ami_long e)

{

    return (randn(e-s)+s);

}

static void swap(int* a, int* b)

{

    int t;

    t = *a;
    *a = *b;
    *b = t;

}

/* wait time in 100 microseconds, with return to terminate */

static void waitchar(ami_long t, int* st)

{

    ami_evtrec er;

    if (autorun || framenum < tstlo)
        { *st = TRUE; return; } /* end the pattern at once */
    *st = FALSE; /* set no space terminate */
    ami_timer(stdout, 1, t, FALSE);
    do { ami_event(stdin, &er); }
    while (er.etype != ami_ettim && er.etype != ami_etterm &&
           er.etype != ami_etchar && er.etype != ami_etenter);
   if (er.etype == ami_etchar && er.echar == ' ') *st = TRUE;
   if (er.etype == ami_etenter) *st = TRUE;
   if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

}

/* Go on to the next frame. Each pattern is a frame, and the frame number
   is advanced at the start of the pattern, so the title while it draws, the
   steps of an animation within it, and the capture at its end all carry the
   same number. Past the selected range the test is over. */

static void frmnext(void)

{

    char buf[80];

    framenum++;
    stepnum = 0; /* the steps count within the frame */
    if (tsthi && framenum > tsthi) longjmp(terminate_buf, 1);
    auto_event_frame(framenum, 0);
    if (framenum < tstlo) return; /* not in range: count alone */
    sprintf(buf, "graphics_test: frame %d", framenum);
    ami_title(stdout, buf);

}

/* Mark and capture the pattern just drawn: the frame number on the title
   and on the top line at the right, a tenth of the width in from the edge,
   out of the way of what the patterns write there, and the capture. The label draws in xor
   with no background, so it reads on any field and can be taken off again,
   and the states touched (foreground mode and color, background mode) are
   left at the test's steady defaults: overwrite, black, invisible. Before
   the selected range nothing is captured. */

static void frmmark(void)

{

    char buf[80];

    if (framenum < tstlo) return; /* not in range */
    sprintf(buf, "graphics_test: frame %d", framenum);
    ami_title(stdout, buf);
    /* the label centered on the top line, in the current font as the
       rest of the surface's writing, xor so it reads on any field */
    sprintf(buf, "frame %d", framenum);
    ami_binvis(stdout);
    ami_fxor(stdout);
    ami_fcolor(stdout, ami_white);
    ami_cursor(stdout, ami_maxx(stdout)-ami_maxx(stdout)/10-(ami_long)strlen(buf)+1, 1);
    printf("%s", buf);
    ami_cursor(stdout, 1, 1); /* leave the cursor on a safe place */
    ami_fover(stdout);
    ami_fcolor(stdout, ami_black);

    /* capture test screens, the label as the picture's title */
    screen_capture_label(buf);
    screen_capture();

}

/* Mark and capture one step of an animation within the frame being built.

   The step is numbered "frame N.s" after the frame, so the whole frame
   count, and the frame selection on the command line, are unchanged by the
   animation length. The label goes in the title and on the displayed
   surface where the frame mark goes, in xor, and is
   taken off again after the capture so the animation's own drawing is left
   as it was. Used in automatic runs only. */
static void frmstep(void)
{
    char buf[80];
    int  i;

    if (framenum < tstlo) return; /* not in range */
    stepnum++;
    sprintf(buf, "graphics_test: frame %d.%d", framenum, stepnum);
    ami_title(stdout, buf);
    sprintf(buf, "frame %d.%d", framenum, stepnum);
    ami_binvis(stdout);
    ami_fxor(stdout);
    ami_fcolor(stdout, ami_white);
    for (i = 0; i < 2; i++) { /* the label on, capture, the label off */

        ami_cursor(stdout, ami_maxx(stdout)-ami_maxx(stdout)/10-(ami_long)strlen(buf)+1, 1);
        printf("%s", buf);
        if (!i) { screen_capture_label(buf); screen_capture(); }

    }
    ami_cursor(stdout, 1, 1); /* leave the cursor on a safe place */
    ami_fover(stdout);
    ami_fcolor(stdout, ami_black);
    auto_event_frame(framenum, stepnum);

}

static void waitnext(void)

{

    ami_evtrec er; /* event record */

    frmmark(); /* label and capture the pattern */
    /* automatic runs and the frames before the range pass at once */
    if (!autorun && framenum >= tstlo) {

        do { ami_event(stdin, &er); }
        while (er.etype != ami_etenter && er.etype != ami_etterm);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    }
    frmnext(); /* on to the next pattern */

}

/* print centered string */

static void prtcen(int y, const char* s)

{

   ami_cursor(stdout, (ami_maxx(stdout)/2)-(strlen(s)/2), y);
   printf("%s", s);

}

/* print centered string graphical */

static void prtceng(int y, const char* s)

{

   ami_cursorg(stdout, (ami_maxxg(stdout)/2)-(ami_strsiz(stdout, s)/2), y);
   printf("%s", s);

}

/* print all printable characters */

static void prtall(void)

{

    char c;
    char s[2];

    s[1] = 0;
    for (c = ' '; c <= '}'; c++) {

        s[0] = 'c';
        if (ami_curxg(stdout)+ami_strsiz(stdout, s) >
            ami_maxxg(stdout)) ami_cursorg(stdout, 1, ami_curyg(stdout)+ami_chrsizy(stdout));
        putchar(c);

    }
    putchar('\n');

}

/* draw a character grid */

static void chrgrid(void)

{

    int x, y;

    ami_fcolor(stdout, ami_yellow);
    y = 1;
    while (y < ami_maxyg(stdout)) {

        ami_line(stdout, 1, y, ami_maxxg(stdout), y);
        y = y+ami_chrsizy(stdout);

    }
    x = 1;
    while (x < ami_maxxg(stdout)) {

        ami_line(stdout, x, 1, x, ami_maxyg(stdout));
        x = x+ami_chrsizx(stdout);

    }
    ami_fcolor(stdout, ami_black);

}

/* find rectangular coordinates from polar, relative to center of circle,
  with given diameter */

static void rectcord(int  a, /* angle, 0-359 */
              int  r, /* radius of circle */
              int* x,
              int* y  /* returns rectangular coordinate */
             )

{

    float angle; /* angle in radian measure */

    angle = a*0.01745329; /* find radian measure */
    *x = round(sin(angle)*r); /* find distance x */
    *y = round(cos(angle)*r); /* find distance y */

}

/* draw polar coordinate line */

static void pline(int a,  /* angle of line */
                  int o,  /* length of line */
                  int cx, /* center of circle in x and y */
                  int cy,
                  int w   /* width of line */
                 )

{

    int ex, ey; /* line start and end */

    rectcord(a, o, &ex, &ey); /* find endpoint of line */
    ami_linewidth(stdout, w); /* set width */
    ami_line(stdout, cx, cy, cx+ex, cy-ey); /* draw line */

}

/* draw centered justified text */

static void justcenter(const char* s, int l)

{

    int i, x;

    x = ami_maxxg(stdout)/2-l/2;
    ami_cursorg(stdout, x, ami_curyg(stdout));
    ami_writejust(stdout, s, l);
    putchar('\n');
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, x, ami_curyg(stdout), x+l-1, ami_curyg(stdout)+ami_chrsizy(stdout)-1);
    ami_fcolor(stdout, ami_black);
    ami_rect(stdout, x, ami_curyg(stdout), x+l-1, ami_curyg(stdout)+ami_chrsizy(stdout)-1);
    for (i = 0; i < strlen(s); i++)
      ami_line(stdout, x+ami_justpos(stdout, s, i, l), ami_curyg(stdout),
              x+ami_justpos(stdout, s, i, l), ami_curyg(stdout)+ami_chrsizy(stdout)-1);
    putchar('\n');

}

/* draw graphics grid */

static void grid(void)

{

    int   x, y;
    int   xspace;
    int   yspace;
    float yrat;

    yrat = (double)ami_dpmy(stdout)/ami_dpmx(stdout); /* find aspect ratio */
    xspace = ami_maxxg(stdout)/60;
    yspace = xspace*yrat;
    ami_linewidth(stdout, 1);
    ami_fcolor(stdout, ami_cyan);
    x = 10;
    while (x <= ami_maxxg(stdout)) {

        ami_line(stdout, x, 1, x, ami_maxyg(stdout));
        x = x+xspace;

    }
    y = 10;
    while (y <= ami_maxyg(stdout)) {

        ami_line(stdout, 1, y, ami_maxxg(stdout), y);
        y = y+yspace;

    }
    ami_fcolor(stdout, ami_black);

}

/* draw screen edge */

static void edge(void)

{

    /* draw boundary lines showing valid coordinate space */
    ami_fcolor(stdout, ami_red);
    ami_line(stdout, 1, 1, ami_maxxg(stdout), 1);
    ami_line(stdout, 1, ami_maxyg(stdout), ami_maxxg(stdout), ami_maxyg(stdout));
    ami_line(stdout, 1, 1, 1, ami_maxyg(stdout));
    ami_line(stdout, ami_maxxg(stdout), 1, ami_maxxg(stdout), ami_maxyg(stdout));
    ami_fcolor(stdout, ami_black);

}

/* This is the square2 program */
#define MAXSQUARE  (10)
#define REPRATE    (2) /* number of moves per frame, should be low */
#define ANISTEPS   (100) /* steps of the animation in automatic runs */

typedef struct { /* square data record */

    int      x, y;   /* current position */
    int      lx, ly; /* last position */
    int      xd, yd; /* deltas */
    ami_color c;      /* color */

} balrec;

int squaresize; /* size of square */
int halfsquare; /* size of 1/2 square */

static balrec baltbl[MAXSQUARE];

static int chkbrk(void)

{

    ami_evtrec er; /* event record */
    int done;

    if (autorun || framenum < tstlo)
        return (TRUE); /* one frame of it is enough */
    done = FALSE;
    do { ami_event(stdin, &er); }
    while (er.etype != ami_etframe && er.etype != ami_etterm &&
           er.etype != ami_etchar && er.etype != ami_etenter);
    if (er.etype == ami_etterm) { longjmp(terminate_buf, 1); }
    if (er.etype == ami_etchar || er.etype == ami_etenter)
        done = TRUE; /* terminate */

    return (done);

}

static void drawsquare(ami_color c, int x, int y)

{

    ami_fcolor(stdout, c); /* set color */
    ami_frect(stdout, x-halfsquare+1, y-halfsquare+1, x+halfsquare-1, y+halfsquare-1);

}

static void movesquare(int s)

{

    balrec* bt;
    int nx, ny; /* temp coordinates holders */

    bt = &baltbl[s];
    nx = bt->x+bt->xd; /* trial move square */
    ny = bt->y+bt->yd;
    /* check out of bounds and reverse direction */
    if (nx < halfsquare || nx > ami_maxxg(stdout)-halfsquare+1) bt->xd = -bt->xd;
    if (ny < halfsquare || ny > ami_maxyg(stdout)-halfsquare+1) bt->yd = -bt->yd;
    bt->x = bt->x+bt->xd; /* move square */
    bt->y = bt->y+bt->yd;

}

static void squares(void)

{

    int     cd; /* current display flip select */
    int     i; /* index for table */
    int     rc; /* repetition counter */
    int     done; /* done flag */
    int     steps; /* steps run, automatic */
    balrec* bt;

    squaresize = ami_maxyg(stdout)/5;
    steps = 0;
    halfsquare = squaresize/2;
    /* initalize square data */
    for (i = 0; i < MAXSQUARE; i++) {

        bt = &baltbl[i];
        bt->x = randn(ami_maxxg(stdout)-squaresize)+halfsquare;
        bt->y = randn(ami_maxyg(stdout)-squaresize)+halfsquare;
        if (!randn(TRUE)) bt->xd = +1; else bt->xd = -1;
        if (!randn(TRUE)) bt->yd = +1; else bt->yd = -1;
        bt->lx = bt->x; /* set last position to same */
        bt->ly = bt->y;
        bt->c = randr(ami_red, ami_magenta); /* set random color */

    }
    ami_curvis(stdout, FALSE); /* turn off cursor */
    cd = FALSE; /* set 1st display */
    /* place squares on display */
    for (i = 0; i < MAXSQUARE; i++)
        drawsquare(baltbl[i].c, baltbl[i].x, baltbl[i].y);
    done = FALSE; /* set ! done */
    while (!done) {

        /* select display and update surfaces */
        ami_select(stdout, !cd+1, cd+1);
        if (autorun && framenum >= tstlo) {

            /* automatic: capture the step, labeled in the displayed surface */
            ami_select(stdout, cd+1, cd+1);
            frmstep();
            ami_select(stdout, !cd+1, cd+1);

        }
        putchar('\f');
        ami_fover(stdout);
        ami_fcolor(stdout, ami_black);
        prtcen(ami_maxy(stdout), "Animation test");
        ami_fxor(stdout);
        /* save old positions */
        for (i = 0; i < MAXSQUARE; i++) {

            baltbl[i].lx = baltbl[i].x; /* save last position */
            baltbl[i].ly = baltbl[i].y;

        }
        /* move squares */
        for (rc = 1; rc <= REPRATE; rc++) /* repeats per frame */
            for (i = 0; i < MAXSQUARE; i++) movesquare(i); /* process squares */
        /* draw squares */
        for (i = 0; i < MAXSQUARE; i++)
            drawsquare(baltbl[i].c, baltbl[i].x, baltbl[i].y);
        cd = !cd; /* flip display and update surfaces */
        /* automatic: run for a while, at once if the frame is not selected */
        if (autorun) done = framenum < tstlo || ++steps >= ANISTEPS;
        else done = chkbrk(); /* check complete */

    }
    ami_select(stdout, 1, 1); /* restore buffer surfaces */
    ami_fover(stdout); /* restore foreground overwrite */
    frmmark(); /* capture where the squares got to */
    frmnext();

}

/* draw standard graphical test, which is all the figures possible
  arranged on the screen */

static void graphtest(int lw) /* line width */

{

    int fsiz;
    int x, y;
    int xsize;   /* total space x for figures */
    int xspace;  /* x space between figures */
    int yspace;  /* y space between figures */
    int xfigsiz; /* size of figure x */
    int yfigsiz; /* size of figure y */
    float yrat;  /* aspect ratio of y to x */

    ami_auto(stdout, OFF);
    ami_font(stdout, AMI_FONT_SIGN);
    fsiz = ami_chrsizy(stdout); /* save character size to restore */
    ami_fontsiz(stdout, ami_maxyg(stdout)/20);
    ami_bcolor(stdout, ami_yellow);
    ami_cursorg(stdout, ami_maxxg(stdout)/2-ami_strsiz(stdout, S6)/2, ami_curyg(stdout));
    printf("%s\n", S6);
    putchar('\n');
    /* note: after this we can't use text spacing */

    /* we assume x > y */
    yrat = (double)ami_dpmy(stdout)/ami_dpmx(stdout); /* find aspect ratio */
    xsize = ami_maxxg(stdout)/5; /* set spacing of figures */
    xspace = xsize/5; /* set x space between figures */
    xfigsiz = xsize-xspace; /* net y size of figure */
    yfigsiz = xfigsiz*yrat; /* net y size of figure */
    yspace = xspace*yrat; /* set y space between figures */

    /* first row of figures */
    ami_fcolor(stdout, ami_magenta);
    ami_linewidth(stdout, lw);
    y = ami_curyg(stdout); /* set y location to current */
    x = xspace/2; /* set x location to after space left */
    ami_rect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    ami_fcolor(stdout, ami_green);
    x = x+xfigsiz+xspace;
    ami_frect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    ami_fcolor(stdout, ami_yellow);
    x = x+xfigsiz+xspace;
    ami_ftriangle(stdout, x, y+yfigsiz-1, x+xfigsiz/2-1, y,
                         x+xfigsiz-1, y+yfigsiz-1);
    ami_fcolor(stdout, ami_red);
    x = x+xfigsiz+xspace;
    ami_rrect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 20, 20);
    ami_fcolor(stdout, ami_magenta);
    x = x+xfigsiz+xspace;
    ami_arc(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 0, LONG_MAX/4);
    ami_fcolor(stdout, ami_green);
    ami_farc(stdout, x, y, x+xfigsiz-1, y+xfigsiz-1,
                          LONG_MAX/2, LONG_MAX/2+LONG_MAX/4);
    y = y+yfigsiz+yspace;
    x = xspace/2;

    /* second row of figures */
    ami_fcolor(stdout, ami_blue);
    ami_frect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    x = x+xfigsiz+xspace;
    ami_fcolor(stdout, ami_magenta);
    ami_frrect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 20, 20);
    x = x+xfigsiz+xspace;
    ami_fcolor(stdout, ami_green);
    ami_ellipse(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    x = x+xfigsiz+xspace;
    ami_fcolor(stdout, ami_yellow);
    ami_fellipse(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    x = x+xfigsiz+xspace;
    ami_fcolor(stdout, ami_blue);
    ami_fchord(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 0, LONG_MAX/2);
    y = y+xfigsiz+xspace;

    /* third row of figures (lines) */
    ami_fcolor(stdout, ami_red);
    ami_linewidth(stdout, 1);
    ami_line(stdout, 20, y, ami_maxxg(stdout)-20, y);
    y = y+10;
    ami_fcolor(stdout, ami_green);
    ami_linewidth(stdout, 3);
    ami_line(stdout, 20, y, ami_maxxg(stdout)-20, y);
    y = y+10;
    ami_fcolor(stdout, ami_blue);
    ami_linewidth(stdout, 7);
    ami_line(stdout, 20, y, ami_maxxg(stdout)-20, y);
    y = y+20;
    ami_fcolor(stdout, ami_magenta);
    ami_linewidth(stdout, 15);
    ami_line(stdout, 20, y, ami_maxxg(stdout)-20, y);
    ami_linewidth(stdout, 1);
    ami_fontsiz(stdout, fsiz); /* restore font size */
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    ami_font(stdout, AMI_FONT_TERM);

}

/* pointer to benchmark test */

typedef void (*benchtestFP)(int w, int t, ami_long* s);

/* run benchmark test
 *
 * Since different processors take different amounts of time to run the tests,
 * we normalize them to gather as much data as needed to run for 5 seconds
 * per test. To find out how many test we need to run for that time, we measure
 * a small number of tests, then extrapolate that time to 5 seconds. Since the
 * test could iterate in a very short (unmeasurable) time, we run the tests at
 * progressive orders of magnitude until we run at least 10ms of test time and
 * at least 10 iterations.
 */
static void benchtest(
    /* benchtest routine pointer */       benchtestFP fp,
    /* number of test */                  bench       bn,
    /* width of line (if applies) */      int         w
    )

{

    int i;
    ami_long t;
    ami_long et;
    ami_long s;

    /* test how many iterations we need to get a measurable timebase */
    i = 10;
    do {

        t = ami_clock(); /* get base time */
        fp(w, i, &s); /* perform test */
        et = ami_elapsed(t); /* find time to execute */
        i *= 2; /* scale for next pass */

    } while (et < SECOND); /* set minimum time to measure for stability */
    i /= 2; /* remove last scale */
    /* find iterations for 5 second run */
    i = (SECOND*5*0.0001)/(et*0.0001/i);
    fp(w, i, &s); /* run final test */
    benchtab[bn].iter = i; /* place iterations */
    benchtab[bn].time = s; /* place time to run */

}

/* test line speed */

static void linespeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    ami_linewidth(stdout, w);
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_line(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                        randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)));

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test rectangle speed */

static void rectspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    ami_linewidth(stdout, w);
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_rect(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                        randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)));

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test rounded rectangle speed */

static void rrectspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    ami_linewidth(stdout, w);
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_rrect(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                         randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                         randn(100-1)+1, randn(100-1)+1);

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test filled rectangle speed */

static void frectspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_frect(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                         randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)));

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test filled rounded rectangle speed */

static void frrectspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_frrect(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                          randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                          randn(100-1)+1, randn(100-1)+1);

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test ellipse speed */

static void ellipsespeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    ami_linewidth(stdout, w);
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_ellipse(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                           randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)));

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test filled ellipse speed */

static void fellipsespeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_fellipse(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                            randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)));

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test arc speed */

static void arcspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;
    ami_long sa, ea;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    ami_linewidth(stdout, w);
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        do {

            sa = randn(LONG_MAX);
            ea = randn(LONG_MAX);

        } while (ea <= sa);
        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_arc(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                       randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                       sa, ea);

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test filled arc speed */

static void farcspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;
    ami_long sa, ea;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        do {

            sa = randn(LONG_MAX);
            ea = randn(LONG_MAX);

        } while (ea <= sa);
        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_farc(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                        randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                        sa, ea);

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test filled chord speed */

static void fchordspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;
    ami_long sa, ea;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        do {

            sa = randn(LONG_MAX);
            ea = randn(LONG_MAX);

        } while (ea <= sa);
        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_fchord(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                          randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                          sa, ea);

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test filled triangle speed */

static void ftrianglespeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_ftriangle(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                             randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)),
                             randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)));

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}

/* test text speed */

static void ftextspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        ami_fcolor(stdout, randr(ami_red, ami_magenta));
        ami_bcolor(stdout, randr(ami_red, ami_magenta));
        ami_cursorg(stdout, randr(1, ami_maxxg(stdout)), randr(1, ami_maxyg(stdout)));
        printf("Test text");

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);

}

/* test picture draw speed */

static void fpictspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;
    int x1, y1, x2, y2;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    ami_loadpict(stdout, 1, "tests/mypic");
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        x1 = randr(1, ami_maxxg(stdout));
        x2 = randr(1, ami_maxxg(stdout));
        if (x1 > x2) swap(&x1, &x2);
        y1 = randr(1, ami_maxyg(stdout));
        y2 = randr(1, ami_maxyg(stdout));
        if (y1 > y2) swap(&y1, &y2);
        ami_picture(stdout, 1, x1, y1, x2, y2);

   }
   *s = ami_elapsed(c);
   ami_fcolor(stdout, ami_black);

}

/* test picture draw speed, no scaling */

static void fpictnsspeed(int w, int t, ami_long* s)

{

    int i;
    ami_long c;
    int x, y;
    int xs, ys;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    putchar('\f');
    ami_loadpict(stdout, 1, "tests/mypic");
    xs = ami_pictsizx(stdout, 1);
    ys = ami_pictsizy(stdout, 1);
    c = ami_clock();
    for (i = 1; i <= t; i++) {

        x = randr(1, ami_maxxg(stdout));
        y = randr(1, ami_maxyg(stdout));
        ami_picture(stdout, 1, x, y, x+xs-1, y+ys-1);

    }
    *s = ami_elapsed(c);
    ami_fcolor(stdout, ami_black);

}


/* Block copy tests: ami_blockcopyg moves a block of pixels from one buffer
   to another, or elsewhere in the same buffer, stretched to the destination
   box and placed with the write mode of the update buffer. Six patterns:
   simple figures moved and resized within the display buffer; a picture,
   parts of it moved and enlarged; a picture and figures drawn in a hidden
   buffer and moved into the display; then the write modes, which make
   stencils: an and copy shows the block only in the white parts of a black
   and white stencil, an or copy only in the black parts, and an xor copy
   reverses the colors, twice over giving them back. */

static void blockcopy(void)

{

    char fn[100];
    ami_long p;  /* the panel: a square, a third of the height */
    ami_long m;  /* the margin between panels */
    ami_long cx, cy; /* the center of the right panel */
    ami_long q, g;   /* a quarter enlarged, and the gap between quarters */
    ami_long ox, oy; /* the top left of the quarters set out */
    ami_long x, y, x2, y2;
    ami_long fsiz;

    p = ami_maxyg(stdout)/3;
    m = (ami_maxxg(stdout)-3*p)/4;
    y = ami_maxyg(stdout)/2-p/2;
    y2 = y+p-1;
    cx = m+2*(p+m)+p/2;
    cy = ami_maxyg(stdout)/2;

    /* ********************* Block copy within a buffer ******************** */

    putchar('\f');
    grid();
    /* three figures in the left panel */
    x = m;
    x2 = x+p-1;
    ami_fcolor(stdout, ami_red);
    ami_fellipse(stdout, x+p/8, y+p/8, x+p/2, y+p/2);
    ami_fcolor(stdout, ami_blue);
    ami_frect(stdout, x+p/2, y+p/2, x+p-p/8, y+p-p/8);
    ami_fcolor(stdout, ami_green);
    ami_ftriangle(stdout, x+p/2+p/4, y+p/8, x+p/2, y+p/2-p/8, x+p-p/8, y+p/2-p/8);
    ami_fcolor(stdout, ami_black);
    ami_rect(stdout, x, y, x2, y2);
    /* the same size in the middle, half again as large at the right, and
       the top left quarter at half size below that */
    ami_blockcopyg(stdout, 1, 1, x, y, x2, y2, x+p+m, y, x2+p+m, y2);
    ami_blockcopyg(stdout, 1, 1, x, y, x2, y2,
                   cx-3*p/4, y-p/4, cx+3*p/4-1, y2+p/4);
    ami_blockcopyg(stdout, 1, 1, x, y, x+p/2-1, y+p/2-1,
                   cx-p/8, y2+p/4+m/4, cx+p/8-1, y2+p/4+m/4+p/4-1);
    prtcen(2, "The figures at the left are copied to the middle, half again");
    prtcen(3, "as large at the right, and their top left quarter halved below that");
    prtcen(ami_maxy(stdout), "Block copy within a buffer test");
    waitnext();

    /* ********************* Block copy of a picture ******************** */

    putchar('\f');
    grid();
    ami_maknam(fn, 100, "tests", "mypic", "");
    ami_loadpict(stdout, 1, fn);
    x = m;
    x2 = x+p-1;
    ami_picture(stdout, 1, x, y, x2, y2);
    ami_delpict(stdout, 1);
    /* the whole in the middle, then its four quarters enlarged and set out
       around the right panel's center */
    ami_blockcopyg(stdout, 1, 1, x, y, x2, y2, x+p+m, y, x2+p+m, y2);
    q = 3*p/4;
    g = m/2;
    ox = cx-(2*q+g)/2;
    oy = cy-(2*q+g)/2;
    ami_blockcopyg(stdout, 1, 1, x, y, x+p/2-1, y+p/2-1,
                   ox, oy, ox+q-1, oy+q-1);
    ami_blockcopyg(stdout, 1, 1, x+p/2, y, x2, y+p/2-1,
                   ox+q+g, oy, ox+2*q+g-1, oy+q-1);
    ami_blockcopyg(stdout, 1, 1, x, y+p/2, x+p/2-1, y2,
                   ox, oy+q+g, ox+q-1, oy+2*q+g-1);
    ami_blockcopyg(stdout, 1, 1, x+p/2, y+p/2, x2, y2,
                   ox+q+g, oy+q+g, ox+2*q+g-1, oy+2*q+g-1);
    prtcen(2, "The picture at the left is copied to the middle, and its");
    prtcen(3, "quarters are enlarged and set apart at the right");
    prtcen(ami_maxy(stdout), "Block copy of a picture test");
    waitnext();

    /* ******************** Block copy between buffers ******************* */

    /* the picture and the figures are drawn in buffer 2 and shown there
       first, the source; then buffer 1 is shown again, empty but for the
       grid, and they are moved into it, the destination */
    putchar('\f');
    grid();
    ami_select(stdout, 2, 2); /* draw in 2, and show it */
    putchar('\f');
    ami_maknam(fn, 100, "tests", "mypic1", "bmp");
    ami_loadpict(stdout, 1, fn);
    x = m;
    x2 = x+p-1;
    ami_picture(stdout, 1, x, y, x2, y2);
    ami_delpict(stdout, 1);
    ami_fcolor(stdout, ami_magenta);
    ami_fellipse(stdout, x+p+m, y, x2+p+m, y2);
    ami_fcolor(stdout, ami_cyan);
    ami_frect(stdout, x+p+m+p/4, y+p/4, x2+p+m-p/4, y2-p/4);
    ami_fcolor(stdout, ami_black);
    prtcen(2, "A picture and figures drawn in buffer 2, on display: the source");
    prtcen(3, "of the copy on the next screen, which shows buffer 1");
    prtcen(ami_maxy(stdout), "Block copy between buffers source test");
    waitnext();
    ami_select(stdout, 1, 1); /* back to the display buffer, with the grid */
    ami_blockcopyg(stdout, 2, 1, x, y, x2+p+m, y2, x, y, x2+p+m, y2);
    ami_blockcopyg(stdout, 2, 1, x, y, x2, y2,
                   x+2*(p+m), y+p/4, x2+2*(p+m)-p/2, y2-p/4);
    prtcen(2, "Buffer 1, with the picture and figures moved into it from buffer 2,");
    prtcen(3, "whole at the left, and the picture narrowed at the right");
    prtcen(ami_maxy(stdout), "Block copy between buffers destination test");
    waitnext();

    /* ********************** Block copy and stencil ********************** */

    /* the display holds a stencil, white figures on black; the picture in
       buffer 2 is copied over it with and, and shows in the white alone */
    fsiz = ami_chrsizy(stdout); /* save character size to restore */
    putchar('\f');
    ami_select(stdout, 2, 1); /* the picture in 2, across the three panels */
    putchar('\f');
    ami_maknam(fn, 100, "tests", "mypic", "");
    ami_loadpict(stdout, 1, fn);
    x = m;
    x2 = m+3*p+2*m-1;
    ami_picture(stdout, 1, x, y, x2, y2);
    ami_delpict(stdout, 1);
    ami_select(stdout, 1, 1);
    /* the stencil */
    ami_fcolor(stdout, ami_black);
    ami_frect(stdout, x, y, x2, y2);
    ami_fcolor(stdout, ami_white);
    ami_fellipse(stdout, x+p/8, y+p/8, x+p-p/8, y2-p/8);
    ami_frect(stdout, x+p+m+p/8, y+p/8, x+2*p+m-p/8, y2-p/8);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, p/2);
    ami_cursorg(stdout, x+2*(p+m)+p/2-ami_strsiz(stdout, "Ami")/2, y+p/2-p/4);
    ami_binvis(stdout);
    printf("Ami");
    ami_fontsiz(stdout, fsiz);
    ami_font(stdout, AMI_FONT_TERM);
    ami_fcolor(stdout, ami_black);
    /* the copy, with and */
    ami_fand(stdout);
    ami_blockcopyg(stdout, 2, 1, x, y, x2, y2, x, y, x2, y2);
    ami_fover(stdout);
    prtcen(2, "A picture copied with and over a stencil of white figures on");
    prtcen(3, "black: it shows in the white parts alone");
    prtcen(ami_maxy(stdout), "Block copy and stencil test");
    waitnext();

    /* *********************** Block copy or stencil ********************** */

    /* the stencil reversed, black figures on white, and the copy with or:
       the picture shows in the black alone */
    putchar('\f');
    ami_fcolor(stdout, ami_black);
    ami_fellipse(stdout, x+p/8, y+p/8, x+p-p/8, y2-p/8);
    ami_frect(stdout, x+p+m+p/8, y+p/8, x+2*p+m-p/8, y2-p/8);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, p/2);
    ami_cursorg(stdout, x+2*(p+m)+p/2-ami_strsiz(stdout, "Ami")/2, y+p/2-p/4);
    printf("Ami");
    ami_fontsiz(stdout, fsiz);
    ami_font(stdout, AMI_FONT_TERM);
    ami_rect(stdout, x, y, x2, y2);
    ami_for(stdout);
    ami_blockcopyg(stdout, 2, 1, x, y, x2, y2, x, y, x2, y2);
    ami_fover(stdout);
    prtcen(2, "The same picture copied with or over the stencil reversed,");
    prtcen(3, "black figures on white: it shows in the black parts alone");
    prtcen(ami_maxy(stdout), "Block copy or stencil test");
    waitnext();

    /* ************************** Block copy xor ************************** */

    /* the picture at the left; copied with xor onto white in the middle it
       comes out reversed; the middle copied with xor onto white at the right
       comes out reversed again, the picture itself */
    putchar('\f');
    grid();
    ami_select(stdout, 2, 1);
    putchar('\f');
    ami_select(stdout, 1, 1);
    ami_maknam(fn, 100, "tests", "mypic1", "bmp");
    ami_loadpict(stdout, 1, fn);
    x = m;
    x2 = x+p-1;
    ami_picture(stdout, 1, x, y, x2, y2);
    ami_delpict(stdout, 1);
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, x+p+m, y, x2+2*(p+m), y2); /* white to copy onto */
    ami_fcolor(stdout, ami_black);
    ami_fxor(stdout);
    ami_blockcopyg(stdout, 1, 1, x, y, x2, y2, x+p+m, y, x2+p+m, y2);
    ami_blockcopyg(stdout, 1, 1, x+p+m, y, x2+p+m, y2,
                   x+2*(p+m), y, x2+2*(p+m), y2);
    ami_fover(stdout);
    ami_rect(stdout, x, y, x2, y2);
    ami_rect(stdout, x+p+m, y, x2+p+m, y2);
    ami_rect(stdout, x+2*(p+m), y, x2+2*(p+m), y2);
    prtcen(2, "The picture at the left copied with xor onto white is reversed");
    prtcen(3, "in the middle, and the middle copied the same way is the picture again");
    prtcen(ami_maxy(stdout), "Block copy xor test");
    waitnext();
    ami_binvis(stdout);

}

int main(int argc, char* argv[])

{

    int yspace;
    int xspace;
    int xsize;
    int ysize;
    int x1, y1, x2, y2;
    char fn[100];
    int argi;
    ami_long x, y;
    ami_long t, et;
    float f;

    ami_frametimer(stdout, TRUE); /* start frame timer */
    if (setjmp(terminate_buf)) goto terminate;
    ami_curvis(stdout, FALSE);
    ami_binvis(stdout);
    /* "graphics_test bench" runs the benchmark section alone, without
       walking the interactive patterns: for timing comparisons and the
       headless rig */
    if (argc > 1 && !strcmp(argv[1], "bench")) goto benchmarks;
    /* "graphics_test auto" walks every pattern with no input at all and
       exits at the end of them, which is how the regression runs it. A
       second argument names the file the screens are captured to, so runs
       beside each other do not write over one another */
    argi = 1;
    if (argc > argi && !strcmp(argv[argi], "auto")) {

        autorun = TRUE;
        ami_autohold(FALSE); /* end when the patterns end */
        auto_event_name(EVENTFILE);
        argi++;
        /* a non-numeric argument names the capture file */
        if (argc > argi && (argv[argi][0] < '0' || argv[argi][0] > '9') &&
            strcmp(argv[argi], "events")) {

            screen_capture_name(argv[argi]);
            auto_event_beside(argv[argi], EVENTNAME); /* the events beside it */
            argi++;

        }

    }
    /* "graphics_test events" plays the event file in an interactive run */
    if (argc > argi && !strcmp(argv[argi], "events")) {

        auto_event_name(EVENTFILE);
        argi++;

    }
    /* "graphics_test [auto [file]] first [last]" runs the numbered
       frames alone: the patterns before the first pass without a stop,
       and the run ends after the last. With no last it runs from the
       first frame to the end of the test. */
    if (argc > argi) {

        tstlo = atoi(argv[argi]);
        if (tstlo < 1) tstlo = 1;
        argi++;

    }
    if (argc > argi) tsthi = atoi(argv[argi]);
    frmnext(); /* the first frame */
    printf("Graphics screen test vs. 0.1\n");
    printf("\n");
    printf("Screen size in characters: x -> %lld y -> %lld\n", AMI_LONG_CAST(ami_maxx(stdout)),
                                                           AMI_LONG_CAST(ami_maxy(stdout)));
    printf("            in pixels:     x -> %lld y -> %lld\n",
           AMI_LONG_CAST(ami_maxxg(stdout)), AMI_LONG_CAST(ami_maxyg(stdout)));
    printf("Size of character in default font: x -> %lld y -> %lld\n",
           AMI_LONG_CAST(ami_chrsizx(stdout)), AMI_LONG_CAST(ami_chrsizy(stdout)));
    printf("Dots per meter: dpmx: %lld dpmy: %lld\n", AMI_LONG_CAST(ami_dpmx(stdout)), AMI_LONG_CAST(ami_dpmy(stdout)));
    printf("Aspect ratio: %f\n", (double)ami_dpmx(stdout)/ami_dpmy(stdout));
    prtcen(ami_maxy(stdout),
           "Press return to start test (and to pass each pattern)");
    waitnext();

    /* ************************ Graphical figures test ************************* */

    putchar('\f');

    grid();
    printf("\n");
    ami_bover(stdout);
    graphtest(1);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Graphical figures test, linewidth == 1");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(2);
    prtcen(ami_maxy(stdout), "Graphical figures test, linewidth == 2");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(3);
    prtcen(ami_maxy(stdout), "Graphical figures test, linewidth == 3");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(5);
    prtcen(ami_maxy(stdout), "Graphical figures test, linewidth == 5");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(11);
    prtcen(ami_maxy(stdout), "Graphical figures test, linewidth == 11");
    waitnext();

   /* ***************************** Standard Fonts test *********************** */

    putchar('\f');
    chrgrid();
    prtcen(ami_maxy(stdout), "Standard fonts test");
    ami_auto(stdout, FALSE);
    ami_home(stdout);
    ami_binvis(stdout);
    ami_fontnam(stdout, AMI_FONT_TERM, fns, 100);
    if (strlen(fns) > 0) {

        ami_font(stdout, AMI_FONT_TERM);
        printf("This is the terminal font: System name: \"%s\"\n", fns);
        printf("Size x -> %lld y -> %lld\n", AMI_LONG_CAST(ami_chrsizx(stdout)), AMI_LONG_CAST(ami_chrsizy(stdout)));
        prtall();
        printf("\n");

    } else {

        printf("There is no terminal font\n");
        printf("\n");

    }
    ami_fontnam(stdout, AMI_FONT_BOOK, fns, 100);
    if (strlen(fns) > 0) {

        ami_font(stdout, AMI_FONT_BOOK);
        printf("This is the book font: System name: \"%s\"\n", fns);
        printf("Size x -> %lld y -> %lld\n", AMI_LONG_CAST(ami_chrsizx(stdout)), AMI_LONG_CAST(ami_chrsizy(stdout)));
        prtall();
        printf("\n");

    } else {

        printf("There is no book font\n");
        printf("\n");

    }
    ami_fontnam(stdout, AMI_FONT_SIGN, fns, 100);
    if (strlen(fns) > 0) {

        ami_font(stdout, AMI_FONT_SIGN);
        printf("This is the sign font: System name: \"%s\"\n", fns);
        printf("Size x -> %lld y -> %lld\n", AMI_LONG_CAST(ami_chrsizx(stdout)), AMI_LONG_CAST(ami_chrsizy(stdout)));
        prtall();
        printf("\n");

    } else {

        printf("There is no sign font\n");
        printf("\n");

    }
    ami_fontnam(stdout, AMI_FONT_TECH, fns, 100);
    if (strlen(fns) > 0) {

        ami_font(stdout, AMI_FONT_TECH);
        printf("This is the technical font: System name: \"%s\"\n", fns);
        printf("Size x -> %lld y -> %lld\n", AMI_LONG_CAST(ami_chrsizx(stdout)), AMI_LONG_CAST(ami_chrsizy(stdout)));
        prtall();
        printf("\n");

    } else {

        printf("There is no technical font\n");
        printf("\n");

    }
    ami_font(stdout, AMI_FONT_TERM);
    printf("Complete\n");
    waitnext();

   /* ********************** Graphical cursor movement test ******************* */

    putchar('\f');
    ami_auto(stdout, FALSE);
    prtcen(ami_maxy(stdout), "Graphical cursor movement test");
    x = 1;
    y = 1;
    i = 10000;
    dx = +1;
    dy = +1;
    if (autorun) { dx = AUTOSTEP; dy = AUTOSTEP; } /* automatic: a fair pace */
    ln = ami_strsiz(stdout, S1);
    term = FALSE;
    bright = FALSE; /* no bounces yet */
    bbot = FALSE;
    while (!term) {

        ami_cursorg(stdout, x, y);
        printf("%s", S1);
        xs = x;
        ys = y;
        x = x+dx;
        y = y+dy;
        if (x < 1 || x+ln-1 > ami_maxxg(stdout)) {

            x = xs;
            dx = -dx;
            if (dx < 0) bright = TRUE; /* bounced off the right side */

        }
        if (y < 1 || y+ami_chrsizy(stdout)*2 > ami_maxyg(stdout)) {

            y = ys;
            dy = -dy;
            if (dy < 0) bbot = TRUE; /* bounced off the bottom */

        }
        if (autorun && framenum >= tstlo) {

            /* automatic: capture each step, and run until the string has
               bounced off both the bottom and the right side */
            frmstep();
            term = bright && bbot;

        } else waitchar(100, &term);
        if (!term) { /* take the string off for the next step */

            ami_cursorg(stdout, xs, ys);
            ami_fcolor(stdout, ami_white);
            printf("%s", S1);
            ami_fcolor(stdout, ami_black);

        }

    }
    frmmark(); /* capture where the string stopped */
    frmnext();

    /* ************************** Horizontal lines test ************************ */

    putchar('\f');
    grid();
    prtcen(ami_maxy(stdout), "Horizontal lines test");
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/50;
    y = yspace;
    w = 1;
    while (y+w/2 < ami_maxyg(stdout)-ami_chrsizy(stdout)) {

        ami_linewidth(stdout, w);
        ami_line(stdout, xspace, y, ami_maxxg(stdout)-xspace, y);
        y = y+yspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    waitnext();

    /* ************************** Vertical lines test ************************** */

    putchar('\f');
    grid();
    prtcen(ami_maxy(stdout), "Vertical lines test");
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/20;
    x = xspace;
    w = 1;
    while (x+w/2 < ami_maxxg(stdout)-20) {

        ami_linewidth(stdout, w);
        ami_line(stdout, x, yspace, x, ami_maxyg(stdout)-ami_chrsizy(stdout));
        x = x+xspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    waitnext();

    /* ************************** 45 degree lines test ************************* */

    putchar('\f');
    grid();
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/20;
    /* Lines have slope 1 (y = x + y_intercept). Draw them long enough to
       exit the window on both ends — X11 clips to the window area. Stepping
       the y-intercept from below the top-right corner down to above the
       bottom-left fills the whole usable window with parallel diagonals. */
    ysize = ami_maxxg(stdout)+ami_maxyg(stdout);
    y = -(ami_maxxg(stdout)-xspace);
    w = 1;
    while (y+w/2 < ami_maxyg(stdout)-ami_chrsizy(stdout)-yspace) {

        ami_linewidth(stdout, w);
        ami_line(stdout, 0, y, ysize, y+ysize);
        y = y+xspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    /* caption last — the text blanks the background behind it, so it's
       still readable after the diagonals have swept through the bottom */
    prtcen(ami_maxy(stdout), "45 degree lines test");
    waitnext();

    /* ******************** Line style tests (dashed + dotted) ***************** *
     *                                                                          *
     * Repeat the Vertical / Horizontal / 45 degree lines tests for each of the *
     * two non-solid styles. Each page sets ami_linestyle once, draws the same  *
     * family of increasing-width lines as the solid version, then restores     *
     * ami_lssolid so subsequent tests are unaffected.                          *
     *                                                                          *
     **************************************************************************/

    /* ********************* Horizontal lines test — dashed ******************** */

    putchar('\f');
    grid();
    ami_linestyle(stdout, ami_lsdash);
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/50;
    y = yspace;
    w = 1;
    while (y+w/2 < ami_maxyg(stdout)-ami_chrsizy(stdout)) {

        ami_linewidth(stdout, w);
        ami_line(stdout, xspace, y, ami_maxxg(stdout)-xspace, y);
        y = y+yspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    ami_linestyle(stdout, ami_lssolid);
    prtcen(ami_maxy(stdout), "Horizontal lines test (dashed)");
    waitnext();

    /* ********************* Horizontal lines test — dotted ******************** */

    putchar('\f');
    grid();
    ami_linestyle(stdout, ami_lsdot);
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/50;
    y = yspace;
    w = 1;
    while (y+w/2 < ami_maxyg(stdout)-ami_chrsizy(stdout)) {

        ami_linewidth(stdout, w);
        ami_line(stdout, xspace, y, ami_maxxg(stdout)-xspace, y);
        y = y+yspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    ami_linestyle(stdout, ami_lssolid);
    prtcen(ami_maxy(stdout), "Horizontal lines test (dotted)");
    waitnext();

    /* ********************** Vertical lines test — dashed ********************* */

    putchar('\f');
    grid();
    ami_linestyle(stdout, ami_lsdash);
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/20;
    x = xspace;
    w = 1;
    while (x+w/2 < ami_maxxg(stdout)-20) {

        ami_linewidth(stdout, w);
        ami_line(stdout, x, yspace, x, ami_maxyg(stdout)-ami_chrsizy(stdout));
        x = x+xspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    ami_linestyle(stdout, ami_lssolid);
    prtcen(ami_maxy(stdout), "Vertical lines test (dashed)");
    waitnext();

    /* ********************** Vertical lines test — dotted ********************* */

    putchar('\f');
    grid();
    ami_linestyle(stdout, ami_lsdot);
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/20;
    x = xspace;
    w = 1;
    while (x+w/2 < ami_maxxg(stdout)-20) {

        ami_linewidth(stdout, w);
        ami_line(stdout, x, yspace, x, ami_maxyg(stdout)-ami_chrsizy(stdout));
        x = x+xspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    ami_linestyle(stdout, ami_lssolid);
    prtcen(ami_maxy(stdout), "Vertical lines test (dotted)");
    waitnext();

    /* ********************* 45 degree lines test — dashed ********************* */

    putchar('\f');
    grid();
    ami_linestyle(stdout, ami_lsdash);
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/20;
    ysize = ami_maxxg(stdout)+ami_maxyg(stdout);
    y = -(ami_maxxg(stdout)-xspace);
    w = 1;
    while (y+w/2 < ami_maxyg(stdout)-ami_chrsizy(stdout)-yspace) {

        ami_linewidth(stdout, w);
        ami_line(stdout, 0, y, ysize, y+ysize);
        y = y+xspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    ami_linestyle(stdout, ami_lssolid);
    prtcen(ami_maxy(stdout), "45 degree lines test (dashed)");
    waitnext();

    /* ********************* 45 degree lines test — dotted ********************* */

    putchar('\f');
    grid();
    ami_linestyle(stdout, ami_lsdot);
    yspace = ami_maxyg(stdout)/20;
    xspace = ami_maxxg(stdout)/20;
    ysize = ami_maxxg(stdout)+ami_maxyg(stdout);
    y = -(ami_maxxg(stdout)-xspace);
    w = 1;
    while (y+w/2 < ami_maxyg(stdout)-ami_chrsizy(stdout)-yspace) {

        ami_linewidth(stdout, w);
        ami_line(stdout, 0, y, ysize, y+ysize);
        y = y+xspace;
        w = w+1;

    }
    ami_linewidth(stdout, 1);
    ami_linestyle(stdout, ami_lssolid);
    prtcen(ami_maxy(stdout), "45 degree lines test (dotted)");
    waitnext();

    /* **************************** Polar lines test *************************** */

    putchar('\f');
    grid();
    prtcen(ami_maxy(stdout), "Polar lines test");
    x = ami_maxxg(stdout)/2;
    y = ami_maxyg(stdout)/2;
    if (ami_maxxg(stdout) > ami_maxyg(stdout)) l = ami_maxyg(stdout)/2-ami_chrsizy(stdout);
    else l = ami_maxxg(stdout)/2-ami_chrsizy(stdout);
    w = 1;
    ami_fcolor(stdout, ami_blue);
    ami_ellipse(stdout, x-l, y-l, x+l, y+l);
    ami_fcolor(stdout, ami_black);
    ami_bover(stdout);
    while (w < 10) {

        a = 0; /* set angle */
        while (a < 360) {

            pline(a, l, x, y, w);
            a = a+10;

        };
        ami_home(stdout);
        printf("Line width: %d\n", w);
        w = w+1;
        waitnext();

    }
    ami_binvis(stdout);
    ami_linewidth(stdout, 1);

    /* ******************************* Color test 1 ****************************** */

    putchar('\f');
    y = 1; /* set 1st row */
    r = 0; /* set colors */
    g = 0;
    b = 0;
    while (y < ami_maxyg(stdout)) {

        x = 1;
        while (x < ami_maxxg(stdout)) {

            ami_fcolorg(stdout, r, g, b);
            ami_frect(stdout, x, y, x+COLSQR-1, y+COLSQR-1);
            x = x+COLSQR;
            if (r <= LONG_MAX-LONG_MAX/COLDIV) r = r+LONG_MAX/COLDIV;
            else {

                r = 0;
                if (g <= LONG_MAX-LONG_MAX/COLDIV) g = g+LONG_MAX/COLDIV;
                else {

                    g = 0;
                    if (b <= LONG_MAX-LONG_MAX/COLDIV) b = b+LONG_MAX/COLDIV;
                    else b = 0;

                }

            }

        }
        y = y+COLSQR;

    }
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    ami_bover(stdout);
    prtcen(ami_maxy(stdout), "Color test 1");
    ami_binvis(stdout);
    waitnext();

    /* ******************************* Color test 2 ****************************** */

    putchar('\f');
    x = 1; /* set 2st collumn */
    while (x < ami_maxxg(stdout)) {

        ami_fcolorg(stdout, LONG_MAX/ami_maxxg(stdout)*x, 0, 0);
        ami_line(stdout, x, 1, x, ami_maxyg(stdout));
        x = x+1;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    ami_bover(stdout);
    prtcen(ami_maxy(stdout), "Color test 2");
    ami_binvis(stdout);
    waitnext();

    /* ******************************* Color test 3 ****************************** */

    putchar('\f');
    x = 1; /* set 2st collumn */
    while (x < ami_maxxg(stdout)) {

        ami_fcolorg(stdout, 0, LONG_MAX/ami_maxxg(stdout)*x, 0);
        ami_line(stdout, x, 1, x, ami_maxyg(stdout));
        x = x+1;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    ami_bover(stdout);
    prtcen(ami_maxy(stdout), "Color test 3");
    ami_binvis(stdout);
    waitnext();

    /* ******************************* Color test 4 ****************************** */

    putchar('\f');
    x = 1; /* set 2st collumn */
    while (x < ami_maxxg(stdout)) {

        ami_fcolorg(stdout, 0, 0, LONG_MAX/ami_maxxg(stdout)*x);
        ami_line(stdout, x, 1, x, ami_maxyg(stdout));
        x = x+1;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    ami_bover(stdout);
    prtcen(ami_maxy(stdout), "Color test 4");
    ami_binvis(stdout);
    waitnext();

    /* ***************************** Rectangle test **************************** */

    putchar('\f');
    grid();
    l = 10;
    x = ami_maxxg(stdout)/2; /* find center */
    y = ami_maxyg(stdout)/2;
    w = 1;
    c = ami_black;
    while (l < ami_maxxg(stdout)/2 && l < ami_maxyg(stdout)/2-ami_chrsizy(stdout)) {

        ami_fcolor(stdout, c);
        ami_linewidth(stdout, w);
        ami_rect(stdout, x-l, y-l, x+l, y+l);
        l = l+20;
        w = w+1;
        if (c < ami_magenta) c++;
        else c = ami_black;
        if (c == ami_white) c++;

    };
    ami_linewidth(stdout, 1);
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Rectangle test");
    waitnext();

    /* ************************ Filled rectangle test 1 ************************ */

    putchar('\f');
    grid();
    if (ami_maxxg(stdout) > ami_maxyg(stdout)) l = ami_maxyg(stdout)/2-ami_chrsizy(stdout);
    else l = ami_maxxg(stdout)/2-ami_chrsizy(stdout);
    x = ami_maxxg(stdout)/2; /* find center */
    y = ami_maxyg(stdout)/2;
    c = ami_black;
    while (l >= 10) {

        ami_fcolor(stdout, c);
        ami_frect(stdout, x-l, y-l, x+l, y+l);
        l = l-20;
        if (c < ami_magenta) c++;
        else c = ami_black;
        if (c == ami_white) c++;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled rectangle test 1");
    waitnext();

    /* ************************ Filled rectangle test 2 ************************ */

    putchar('\f');
    grid();
    l = 10;
    x = 20;
    y = 20;
    c = ami_black;
    while (y+l*2 < ami_maxyg(stdout)-ami_chrsizy(stdout)) {

        while (x+l*2 < ami_maxxg(stdout)-ami_chrsizy(stdout)) {

            ami_fcolor(stdout, c);
            ami_frect(stdout, x, y, x+l*2, y+l*2);
            x = x+l*2+20;
            l = l+5;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = 10;
        y = y+l*2+10;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled rectangle test 2");
    waitnext();

    /* ************************* Rounded rectangle test ************************ */

    ami_binvis(stdout);
    r = 1;
    while (r < 100) {

        putchar('\f');
        grid();
        l = 10;
        x = ami_maxxg(stdout)/2; /* find center */
        y = ami_maxyg(stdout)/2;
        w = 1;
        c = ami_black;
        printf("r: %lld\n", AMI_LONG_CAST(r));
        while (l+w/2 < ami_maxxg(stdout)/2 && l < ami_maxyg(stdout)/2-ami_chrsizy(stdout)) {

            ami_fcolor(stdout, c);
            ami_linewidth(stdout, w);
            ami_rrect(stdout, x-l, y-l, x+l, y+l, r, r);
            l = l+w;
            w = w+1;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        ami_linewidth(stdout, 1);
        ami_fcolor(stdout, ami_black);
        prtcen(ami_maxy(stdout), "Rounded rectangle test");
        waitnext();
        r = r+10;

    }
    /* ******************* rounded rectangle minimums test **************** */

    putchar('\f');
    xsize = ami_maxxg(stdout)/20;
    ysize = ami_maxyg(stdout)/20;

    /* paint the grid */
    ami_fcolor(stdout, ami_cyan);
    x1 = 1;
    while (x1 < ami_maxxg(stdout)) {

        ami_line(stdout, x1, 1, x1, ami_maxyg(stdout));
        x1 += xsize;

    }
    y1 = 1;
    while (y1 < ami_maxyg(stdout)) {

        ami_line(stdout, 1, y1, ami_maxxg(stdout), y1);
        y1 += ysize;

    }
    ami_fcolor(stdout, ami_black);

    /* draw vertical */
    x1 = 1+xsize;
    y1 = 1+ysize;
    x2 = x1+xsize*2;
    y2 = y1;
    while (y2+ysize < ami_maxyg(stdout)) {

        ami_rrect(stdout, x1, y1, x2, y2, 10, 10);
        y1 = y1+ysize;
        y2 = y2+ysize+1;

    }

    /* draw horizontal */
    x1 = 1+xsize*4;
    y1 = 1+ysize;
    x2 = x1;
    y2 = ysize*4;
    while (x2+xsize < ami_maxxg(stdout)) {

        ami_rrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 = x1+xsize;
        x2 = x2+xsize+1;

    }

    /* draw boxes */
    x1 = 1+xsize*4;
    y1 = 1+ysize*6;
    x2 = x1;
    y2 = y1;
    while (x2 < ami_maxxg(stdout)) {

        ami_rrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 += xsize;
        x2 += xsize+1;
        y2++;

    }
    prtcen(ami_maxy(stdout), "Rounded Rectangle Minimums Test");
    waitnext();

    /* ******************** Filled rounded rectangle test 1 ******************** */

    ami_binvis(stdout);
    r = 1;
    while (r < 100) {

        putchar('\f');
        grid();
        if (ami_maxxg(stdout) > ami_maxyg(stdout)) l = ami_maxyg(stdout)/2-ami_chrsizy(stdout);
        else l = ami_maxxg(stdout)/2-ami_chrsizy(stdout);
        x = ami_maxxg(stdout)/2; /* find center */
        y = ami_maxyg(stdout)/2;
        c = ami_black;
        printf("r: %lld\n", AMI_LONG_CAST(r));
        while (l >= 10) {

            ami_fcolor(stdout, c);
            ami_frrect(stdout, x-l, y-l, x+l, y+l, r, r);
            l = l-20;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        ami_fcolor(stdout, ami_black);
        prtcen(ami_maxy(stdout), "Filled rounded rectangle test 1");
        waitnext();
        r = r+10;

    }

    /* ******************** Filled rounded rectangle test 2 ******************** */

    ami_binvis(stdout);
    r = 1;
    while (r < 100) {

        putchar('\f');
        grid();
        printf("r: %lld\n", AMI_LONG_CAST(r));
        l = 10;
        x = 20;
        y = ami_curyg(stdout);
        c = ami_black;
        while (y+l*2 < ami_maxyg(stdout)-20) {

            while (x+l*2 < ami_maxxg(stdout)-20) {

                ami_fcolor(stdout, c);
                ami_frrect(stdout, x, y, x+l*2, y+l*2, r, r);
                x = x+l*2+20;
                l = l+5;
                if (c < ami_magenta) c++; else c = ami_black;
                if (c == ami_white) c++;

            }
            x = 10;
            y = y+l*2+10;

        }
        ami_fcolor(stdout, ami_black);
        ami_binvis(stdout);
        prtcen(ami_maxy(stdout), "Filled rounded rectangle test 2");
        waitnext();
        r = r+10;

    }

    /* ******************* filled rounded rectangle minimums test **************** */

    putchar('\f');
    xsize = ami_maxxg(stdout)/20;
    ysize = ami_maxyg(stdout)/20;

    /* paint the grid */
    ami_fcolor(stdout, ami_cyan);
    x1 = 1;
    while (x1 < ami_maxxg(stdout)) {

        ami_line(stdout, x1, 1, x1, ami_maxyg(stdout));
        x1 += xsize;

    }
    y1 = 1;
    while (y1 < ami_maxyg(stdout)) {

        ami_line(stdout, 1, y1, ami_maxxg(stdout), y1);
        y1 += ysize;

    }
    ami_fcolor(stdout, ami_black);

    /* draw vertical */
    x1 = 1+xsize;
    y1 = 1+ysize;
    x2 = x1+xsize*2;
    y2 = y1;
    while (y2+ysize < ami_maxyg(stdout)) {

        ami_frrect(stdout, x1, y1, x2, y2, 10, 10);
        y1 = y1+ysize;
        y2 = y2+ysize+1;

    }

    /* draw horizontal */
    x1 = 1+xsize*4;
    y1 = 1+ysize;
    x2 = x1;
    y2 = ysize*4;
    while (x2+xsize < ami_maxxg(stdout)) {

        ami_frrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 = x1+xsize;
        x2 = x2+xsize+1;

    }

    /* draw boxes */
    x1 = 1+xsize*4;
    y1 = 1+ysize*6;
    x2 = x1;
    y2 = y1;
    while (x2 < ami_maxxg(stdout)) {

        ami_frrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 += xsize;
        x2 += xsize+1;
        y2++;

    }
    prtcen(ami_maxy(stdout), "Filled Rectangle Minimums Test");
    waitnext();

    /* ****************************** Ellipse test ***************************** */

    ami_binvis(stdout);
    w = 1;
    while (w < 10) {

        putchar('\f');
        grid();
        lx = ami_maxxg(stdout)/2-10;
        lx = lx-lx%10;
        ly = ami_maxyg(stdout)/2-10-ami_chrsizy(stdout);
        ly = ly-ly%10;
        x = ami_maxxg(stdout)/2; /* find center */
        y = ami_maxyg(stdout)/2;
        x = x-x%10;
        y = y-y%10;
        c = ami_black;
        printf("width: %d\n", w);
        while (lx >= 10 && ly >= 10) {

            ami_fcolor(stdout, c);
            ami_linewidth(stdout, w);
            ami_ellipse(stdout, x-lx, y-ly, x+lx, y+ly);
            lx = lx-20;
            ly = ly-20;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        ami_fcolor(stdout, ami_black);
        prtcen(ami_maxy(stdout), "Ellipse test");
        waitnext();
        w = w+1;

    }
    ami_linewidth(stdout, 1);

    /* ************************** Filled ellipse test 1 ************************** */

    putchar('\f');
    grid();
    lx = ami_maxxg(stdout)/2-10;
    lx = lx-lx%10;
    ly = ami_maxyg(stdout)/2-10-ami_chrsizy(stdout);
    ly = ly-ly%10;
    x = ami_maxxg(stdout)/2; /* find center */
    y = ami_maxyg(stdout)/2;
    x = x-x%10;
    y = y-y%10;
    c = ami_black;
    while (lx >= 10 && ly >= 10) {

        ami_fcolor(stdout, c);
        ami_fellipse(stdout, x-lx, y-ly, x+lx, y+ly);
        lx = lx-20;
        ly = ly-20;
        if (c < ami_magenta) c++; else c = ami_black;
        if (c == ami_white) c++;

    }
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled ellipse test 1");
    waitnext();

    /* ************************ Filled ellipse test 2 ************************ */

    putchar('\f');
    grid();
    l = 10;
    x = 20;
    y = 20;
    c = ami_black;
    while (y+l*2 < ami_maxyg(stdout)-20) {

        while (x+l*2 < ami_maxxg(stdout)-20) {

            ami_fcolor(stdout, c);
            ami_fellipse(stdout, x, y, x+l*2, y+l*2);
            x = x+l*2+20;
            l = l+5;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = 10;
        y = y+l*2+10;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled ellipse test 2");
    waitnext();

    /* ******************************* Arc test 1 ******************************** */

    ami_binvis(stdout);
    w = 1;
    while (w < 10) {

        putchar('\f');
        grid();
        a = 0;
        c = ami_black;
        i = 10;
        printf("Linewidth: %d", w);
        while (i < ami_maxxg(stdout)/2 && i < (ami_maxyg(stdout)-ami_chrsizy(stdout))/2) {

            a = 0;
            while (a <= LONG_MAX-LONG_MAX/10) {

                ami_fcolor(stdout, c);
                ami_linewidth(stdout, w);
                ami_arc(stdout, i, i, ami_maxxg(stdout)-i,
                       ami_maxyg(stdout)-ami_chrsizy(stdout)-i, a, a+LONG_MAX/10);
                a = a+LONG_MAX/5;
                if (c < ami_magenta) c++; else c = ami_black;
                if (c == ami_white) c++;

            }
            i = i+20;

        }
        ami_fcolor(stdout, ami_black);
        prtcen(ami_maxy(stdout), "Arc test 1");
        waitnext();
        w = w+1;

    }

    /* ************************ Arc test 2 ************************ */

    ami_binvis(stdout);
    w = 1;
    xspace = ami_maxxg(stdout)/40;
    yspace = ami_maxyg(stdout)/40;
    while (w < 10) {

        putchar('\f');
        grid();
        printf("Linewidth: %d\n", w);
        l = ami_maxxg(stdout)/40;
        x = xspace;
        y = ami_curyg(stdout);
        c = ami_black;
        aa = 0;
        ab = LONG_MAX / 360*90;
        while (y+l*2 < ami_maxyg(stdout)-yspace) {

            while (x+l*2 < ami_maxxg(stdout)-xspace) {

                ami_fcolor(stdout, ami_red);
                ami_linewidth(stdout, 1);
                ami_rect(stdout, x, y, x+l*2, y+l*2);
                ami_fcolor(stdout, ami_black);
                ami_linewidth(stdout, w);
                ami_arc(stdout, x, y, x+l*2, y+l*2, aa, ab);
                x = x+l*2+xspace;
                l = l+ami_maxxg(stdout)/60;

            }
            x = xspace;
            y = y+l*2+yspace;

        }
        ami_binvis(stdout);
        prtcen(ami_maxy(stdout), "Arc test 2");
        waitnext();
        w = w+1;

    }

    /* ************************ Arc test 3 ************************ */

    ami_binvis(stdout);
    w = 1;
    xspace = ami_maxxg(stdout)/25;
    yspace = xspace;
    while (w < 10) {

        putchar('\f');
        grid();
        printf("Linewidth: %d\n", w);
        l = xspace;
        x = xspace;
        y = ami_curyg(stdout);
        c = ami_black;
        aa = 0;
        ab = 10;
        while (y+l*2 < ami_maxyg(stdout)-yspace && ab <= 360) {

            while (x+l*2 < ami_maxxg(stdout)-xspace && ab <= 360) {

                ami_fcolor(stdout, ami_red);
                ami_linewidth(stdout, 1);
                ami_rect(stdout, x, y, x+l*2, y+l*2);
                ami_fcolor(stdout, ami_black);
                ami_linewidth(stdout, w);
                ami_arc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
                x = x+l*2+xspace;
                ab = ab+10;

            }
            x = xspace;
            y = y+l*2+yspace;

        }
        ami_binvis(stdout);
        prtcen(ami_maxy(stdout), "Arc test 3");
        waitnext();
        w = w+1;

    }

    /* ************************ Arc test 4 ************************ */

    ami_binvis(stdout);
    w = 1;
    xspace = ami_maxxg(stdout)/25;
    yspace = xspace;
    while (w < 10) {

        putchar('\f');
        grid();
        printf("Linewidth: %d\n", w);
        l = xspace;
        x = xspace;
        y = ami_curyg(stdout);
        c = ami_black;
        aa = 0;
        ab = 360;
        while (y+l*2 < ami_maxyg(stdout)-yspace && aa < 360) {

            while (x+l*2 < ami_maxxg(stdout)-xspace && aa < 360) {

                ami_fcolor(stdout, ami_red);
                ami_linewidth(stdout, 1);
                ami_rect(stdout, x, y, x+l*2, y+l*2);
                ami_fcolor(stdout, ami_black);
                ami_linewidth(stdout, w);
                ami_arc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
                x = x+l*2+xspace;
                aa = aa+10;

            }
            x = xspace;
            y = y+l*2+yspace;

        }
        ami_binvis(stdout);
        prtcen(ami_maxy(stdout), "Arc test 4");
        waitnext();
        w = w+1;

    }

    /* **************************** Filled arc test 1 **************************** */

    putchar('\f');
    grid();
    a = 0;
    c = ami_black;
    a = 0;
    x = ami_maxxg(stdout)-10;
    x = x-x % 10;
    y = ami_maxyg(stdout)-ami_chrsizy(stdout)-10;
    y = y-y % 10;
    while (a <= LONG_MAX-LONG_MAX/10) {

        ami_fcolor(stdout, c);
        ami_farc(stdout, 10, 10, x, y, a, a+LONG_MAX/10);
        a = a+LONG_MAX / 5;
        if (c < ami_magenta) c++; else c = ami_black;
        if (c == ami_white) c++;

    };
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled arc test 1");
    waitnext();

    /* ************************ filled arc test 2 ************************ */

    putchar('\f');
    xspace = ami_maxxg(stdout)/40;
    yspace = ami_maxyg(stdout)/40;
    grid();
    l = ami_maxxg(stdout)/50;
    x = xspace;
    y = yspace;
    c = ami_black;
    aa = 0;
    ab = LONG_MAX / 360*90;
    while (y+l*2 < ami_maxyg(stdout)-yspace) {

        while (x+l*2 < ami_maxxg(stdout)-xspace) {

            ami_fcolor(stdout, ami_red);
            ami_linewidth(stdout, 1);
            ami_rect(stdout, x, y, x+l*2, y+l*2);
            ami_fcolor(stdout, c);
            ami_farc(stdout, x, y, x+l*2, y+l*2, aa, ab);
            x = x+l*2+xspace;
            l = l+ami_maxxg(stdout)/40;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled arc test 2");
    waitnext();

    /* ************************ Filled arc test 3 ************************ */

    putchar('\f');
    xspace = ami_maxxg(stdout)/40;
    yspace = ami_maxyg(stdout)/40;
    grid();
    l = ami_maxxg(stdout)/21;
    x = xspace;
    y = yspace;
    c = ami_black;
    aa = 0;
    ab = 10;
    while (y+l*2 < ami_maxyg(stdout)-yspace && ab <= 360) {

        while (x+l*2 < ami_maxxg(stdout)-xspace && ab <= 360) {

            ami_fcolor(stdout, c);
            ami_farc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            ab = ab+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Arc test 3");
    waitnext();

    /* ************************ Filled arc test 4 ************************ */

    putchar('\f');
    xspace = ami_maxxg(stdout)/40;
    yspace = ami_maxyg(stdout)/40;
    grid();
    l = ami_maxxg(stdout)/21;
    x = xspace;
    y = yspace;
    c = ami_black;
    aa = 0;
    ab = 360;
    while (y+l*2 < ami_maxyg(stdout)-yspace && aa < 360) {

        while (x+l*2 < ami_maxxg(stdout)-xspace && aa < 360) {

            ami_fcolor(stdout, c);
            ami_farc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            aa = aa+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Arc test 4");
    waitnext();

    /* *************************** Filled chord test 1 *************************** */

    putchar('\f');
    grid();
    a = 0;
    c = ami_black;
    a = 0;
    i = 8;
    x = ami_maxxg(stdout)-10;
    x = x-x%10;
    y = ami_maxyg(stdout)-ami_chrsizy(stdout)-10;
    y = y-y%10;
    while (a <= LONG_MAX-LONG_MAX/i) {

        ami_fcolor(stdout, c);
        ami_fchord(stdout, 10, 10, x, y, a, a+LONG_MAX/i);
        a = a+LONG_MAX/(i/2);
        if (c < ami_magenta) c++; else c = ami_black;
        if (c == ami_white) c++;

    }
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled chord test 1");
    waitnext();

    /* ************************ filled chord test 2 ************************ */

    putchar('\f');
    xspace = ami_maxxg(stdout)/50;
    yspace = xspace;
    grid();
    l = ami_maxxg(stdout)/100;
    x = xspace;
    y = yspace;
    c = ami_black;
    aa = 0;
    ab = LONG_MAX/360*90;
    while (y+l*2 < ami_maxyg(stdout)-yspace) {

        while (x+l*2 < ami_maxxg(stdout)-xspace) {

            ami_fcolor(stdout, c);
            ami_fchord(stdout, x, y, x+l*2, y+l*2, aa, ab);
            x = x+l*2+xspace;
            l = l+ami_maxxg(stdout)/100;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled chord test 2");
    waitnext();

    /* ************************ Filled chord test 3 ************************ */

    putchar('\f');
    xspace = ami_maxxg(stdout)/50;
    yspace = xspace;
    grid();
    l = ami_maxxg(stdout)/20;
    x = xspace;
    y = yspace;
    c = ami_black;
    aa = 0;
    ab = 10;
    while (y+l*2 < ami_maxyg(stdout)-yspace && ab <= 360) {

        while (x+l*2 < ami_maxxg(stdout)-xspace && ab <= 360) {

            ami_fcolor(stdout, c);
            ami_fchord(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            ab = ab+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled chord test 3");
    waitnext();

    /* ************************ Filled chord test 4 ************************ */

    putchar('\f');
    xspace = ami_maxxg(stdout)/50;
    yspace = xspace;
    grid();
    l = ami_maxxg(stdout)/20;
    x = xspace;
    y = yspace;
    c = ami_black;
    aa = 0;
    ab = 360;
    while (y+l*2 < ami_maxyg(stdout)-yspace && aa < 360) {

        while (x+l*2 < ami_maxxg(stdout)-xspace && aa < 360) {

            ami_fcolor(stdout, c);
            ami_fchord(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            aa = aa+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled chord test 4");
    waitnext();

    /* ************************** Filled triangle test 1 ************************* */

    putchar('\f');
    grid();
    tx1 = 10;
    ty1 = ami_maxyg(stdout)-ami_chrsizy(stdout)-10;
    ty1 = ty1-ty1%10;
    tx2 = ami_maxxg(stdout)/2;
    ty2 = 10;
    tx3 = ami_maxxg(stdout)-10;
    tx3 = tx3-tx3%10;
    ty3 = ami_maxyg(stdout)-ami_chrsizy(stdout)-10;
    ty3 = ty3-ty3%10;
    c = ami_black;
    i = 40;
    while (tx1 <= tx3-10 && ty2 <= ty3-10) {

        ami_fcolor(stdout, c);
        ami_ftriangle(stdout, tx1, ty1, tx2, ty2, tx3, ty3);
        tx1 = tx1+i;
        ty1 = ty1-i/2;
        ty2 = ty2+i;
        tx3 = tx3-i;
        ty3 = ty3-i/2;
        if (c < ami_magenta) c++; else c = ami_black;
        if (c == ami_white) c++;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled triangle test 1");
    waitnext();

    /* ************************** Filled triangle test 2 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < ami_maxyg(stdout)-20-l) {

        while (y < ami_maxyg(stdout)-20-l && x < ami_maxxg(stdout)-20-l) {

            ami_fcolor(stdout, c);
            ami_ftriangle(stdout, x, y+l, x+l / 2, y, x+l, y+l);
            x = x+l+20;
            l = l+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled triangle test 2");
    waitnext();

    /* ************************** Filled triangle test 3 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < ami_maxyg(stdout)-20-l) {

        while (y < ami_maxyg(stdout)-20-l && x < ami_maxxg(stdout)-20-l) {

            ami_fcolor(stdout, c);
            ami_ftriangle(stdout, x, y+l, x, y, x+l, y+l);
            x = x+l+20;
            l = l+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled triangle test 3");
    waitnext();

    /* ************************** Filled triangle test 4 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < ami_maxyg(stdout)-20-l) {

        while (y < ami_maxyg(stdout)-20-l && x < ami_maxxg(stdout)-20-l) {

            ami_fcolor(stdout, c);
            ami_ftriangle(stdout, x, y+l, x, y, x+l, y);
            x = x+l+20;
            l = l+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled triangle test 4");
    waitnext();

    /* ************************** Filled triangle test 5 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < ami_maxyg(stdout)-20-l) {

        while (y < ami_maxyg(stdout)-20-l && x < ami_maxxg(stdout)-20-l) {

            ami_fcolor(stdout, c);
            ami_ftriangle(stdout, x+l/2, y+l, x, y, x+l, y);
            x = x+l+20;
            l = l+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled triangle test 5");
    waitnext();

    /* ************************** Filled triangle test 6 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    c = ami_black;
    while (y < ami_maxyg(stdout)-20-l) {

        while (y < ami_maxyg(stdout)-20-l && x < ami_maxxg(stdout)-20-l) {

            ami_fcolor(stdout, c);
            ami_ftriangle(stdout, x+l, y+l, x, y, x+l, y);
            x = x+l+20;
            l = l+10;
            if (c < ami_magenta) c++; else c = ami_black;
            if (c == ami_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled triangle test 6");
    waitnext();

    /* ************************** Filled triangle test 7 ************************* */

    putchar('\f');
    grid();
    c = ami_black;
    ami_fcolor(stdout, c);
    ami_ftriangle(stdout, 50, 50, 50, 100, 200, 50);
    if (c < ami_magenta) c++; else c = ami_black;
    if (c == ami_white) c++;
    ami_fcolor(stdout, c);
    ami_ftriangle(stdout, 50, 100, 300, 200, 200, 50);
    if (c < ami_magenta) c++; else c = ami_black;
    if (c == ami_white) c++;
    ami_fcolor(stdout, c);
    ami_ftriangle(stdout, 200, 50, 300, 200, 350, 100);
    if (c < ami_magenta) c++; else c = ami_black;
    if (c == ami_white) c++;
    ami_fcolor(stdout, c);
    ami_ftriangle(stdout, 350, 100, 400, 300, 300, 200);
    if (c < ami_magenta) c++; else c = ami_black;
    if (c == ami_white) c++;
    ami_binvis(stdout);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "Filled triangle test 7");
    waitnext();

    /* ************************** Filled triangle test 8 ************************* */

    putchar('\f');
    grid();
    ami_fcolor(stdout, ami_black);
    ami_ftriangle(stdout, 50, 50, 50, 100, 200, 50);
    ami_ftriangle(stdout, 50, 100, 300, 200, 200, 50);
    ami_ftriangle(stdout, 200, 50, 300, 200, 350, 100);
    ami_ftriangle(stdout, 350, 100, 400, 300, 300, 200);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Filled triangle test 8");
    waitnext();

    /* **************************** Font sizing test *************************** */

    putchar('\f');
    grid();
    fsiz = ami_chrsizy(stdout); /* save character size to restore */
    h = 10;
    ami_auto(stdout, OFF);
    ami_font(stdout, AMI_FONT_SIGN);
    c1 = ami_black;
    c2 = ami_blue;
    ami_bover(stdout);
    while (ami_curyg(stdout)+ami_chrsizy(stdout) <= ami_maxyg(stdout)-20) {

        ami_fcolor(stdout, c1);
        ami_bcolor(stdout, c2);
        ami_fontsiz(stdout, h);
        printf("%lld:%.1f: %s\n", AMI_LONG_CAST(ami_chrsizy(stdout)), ami_points(stdout), S2);
        h = h+5;
        if (c1 < ami_magenta) c1++; else c1 = ami_black;
        if (c1 == ami_white) c1++;
        if (c2 < ami_magenta) c2++; else c2 = ami_black;
        if (c2 == ami_white) c2++;

    }
    ami_fontsiz(stdout, fsiz); /* restore font size */
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    ami_font(stdout, AMI_FONT_TERM);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Font sizing test");
    waitnext();

    /* ************************ Font point sizing test ************************* */

    putchar('\f');
    grid();
    fsiz = ami_chrsizy(stdout); /* save character size to restore */
    {
        float ps = 6.0f;
        ami_auto(stdout, OFF);
        ami_font(stdout, AMI_FONT_SIGN);
        c1 = ami_black;
        c2 = ami_blue;
        ami_bover(stdout);
        while (ami_curyg(stdout)+ami_chrsizy(stdout) <= ami_maxyg(stdout)-20) {

            ami_fcolor(stdout, c1);
            ami_bcolor(stdout, c2);
            ami_setpoints(stdout, ps);
            printf("%lld:%.1f: %s\n", AMI_LONG_CAST(ami_chrsizy(stdout)), ami_points(stdout), S2);
            ps = ps+3.0f;
            if (c1 < ami_magenta) c1++; else c1 = ami_black;
            if (c1 == ami_white) c1++;
            if (c2 < ami_magenta) c2++; else c2 = ami_black;
            if (c2 == ami_white) c2++;

        }
    }
    ami_fontsiz(stdout, fsiz); /* restore font size */
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    ami_font(stdout, AMI_FONT_TERM);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Font point sizing test");
    waitnext();

    /* ***************************** Font list test **************************** */

    putchar('\f');
    grid();
    printf("Number of fonts: %lld\n", AMI_LONG_CAST(ami_fonts(stdout)));
    printf("\n");
    cnt = ami_fonts(stdout);
    for (i = 1; i <= cnt; i++) { /* visit each font code */

        ami_fontnam(stdout, i, fns, 100);
        printf("%d: %s\n", i, fns);
        if (ami_cury(stdout) >= ami_maxy (stdout)) { /* screen overflows */

            printf("Press return to continue");
            waitnext();
            putchar('\f');
            grid();

        }

    }
    printf("\n");
    printf("List complete\n");
    waitnext();

    /* *************************** Font examples test ************************** */

    putchar('\f');
    grid();
    ami_auto(stdout, OFF);
    ami_bcolor(stdout, ami_cyan);
    ami_bover(stdout);
    cnt = ami_fonts(stdout);
    for (i = 1; i <= cnt; i++) { /* visit each font code */

        ami_fontnam(stdout, i, fns, 100);
        ami_font(stdout, i);
        printf("%d: %s\n", i, fns);
        if (ami_cury(stdout) >= ami_maxy(stdout)) { /* screen overflows */

            ami_font(stdout, AMI_FONT_TERM);
            printf("Press return to continue");
            waitnext();
            ami_bcolor(stdout, ami_white);
            putchar('\f');
            grid();
            ami_bcolor(stdout, ami_cyan);

        }

    }
    ami_bcolor(stdout, ami_white);
    ami_font(stdout, AMI_FONT_TERM);
    ami_binvis(stdout);
    printf("\n");
    printf("List complete\n");
    waitnext();

    /* ************************** Extended effects test ************************ */

    putchar('\f');
    grid();
    ami_auto(stdout, OFF);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_condensed(stdout, ON);
    printf("Condensed\n");
    ami_condensed(stdout, OFF);
    ami_extended(stdout, ON);
    printf("Extended\n");
    ami_extended(stdout, OFF);
    ami_xlight(stdout, ON);
    printf("Extra light\n");
    ami_xlight(stdout, OFF);
    ami_light(stdout, ON);
    printf("Light\n");
    ami_light(stdout, OFF);
    ami_xbold(stdout, ON);
    printf("Extra bold\n");
    ami_xbold(stdout, OFF);
    ami_hollow(stdout, ON);
    printf("Hollow\n");
    ami_hollow(stdout, OFF);
    ami_raised(stdout, ON);
    printf("Raised\n");
    ami_raised(stdout, OFF);
    ami_font(stdout, AMI_FONT_TERM);
    prtcen(ami_maxy(stdout), "Extended effects test");
    waitnext();

    /* ****************** Character sizes and positions test ******************* */

    putchar('\f');
    grid();
    ami_auto(stdout, OFF);
    fsiz = ami_chrsizy(stdout); /* save character size to restore */
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, ami_maxyg(stdout)/12);
    printf("Size of test string: %lld\n", AMI_LONG_CAST(ami_strsiz(stdout, S3)));
    printf("\n");
    x = (ami_maxxg(stdout)/2)-(ami_strsiz(stdout, S3)/2);
    ami_cursorg(stdout, x, ami_curyg(stdout)); /* go to centered */
    ami_bcolor(stdout, ami_cyan);
    ami_bover(stdout);
    printf("%s\n", S3);
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, x, ami_curyg(stdout), x+ami_strsiz(stdout, S3)-1,
            ami_curyg(stdout)+ami_chrsizy(stdout)-1);
    ami_fcolor(stdout, ami_black);
    ami_rect(stdout, x, ami_curyg(stdout), x+ami_strsiz(stdout, S3)-1,
            ami_curyg(stdout)+ami_chrsizy(stdout)-1);
    for (i = 0; i < strlen(S3); i++)
        ami_line(stdout, x+ami_chrpos(stdout, S3, i), ami_curyg(stdout),
                x+ami_chrpos(stdout, S3, i),
                ami_curyg(stdout)+ami_chrsizy(stdout)-1);
    printf("\n");

    l = ami_strsiz(stdout, S4); /* get minimum sizing for string */
    justcenter(S4, l);
    justcenter(S4, l+40);
    justcenter(S4, l+80);

    ami_fontsiz(stdout, fsiz); /* restore font size */
    ami_font(stdout, AMI_FONT_TERM);
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Character sizes and positions");
    waitnext();
    ami_bcolor(stdout, ami_white);

    /* ************************** Polar text lines test ************************ */

    putchar('\f');
    grid();
    ami_auto(stdout, OFF); /* rotated text is incompatible with the text grid */
    x = ami_maxxg(stdout)/2; /* window center */
    y = ami_maxyg(stdout)/2;
    l = ami_chrsizx(stdout)*5; /* start radius — 5 char widths from center */
    ami_fcolor(stdout, ami_black);
    ami_bover(stdout);
    a = 0;
    while (a < 360) {

        /* Endpoint of radial at angle a, radius l. */
        rectcord(a, l, &tx1, &ty1);
        /* Rotate text so it reads OUTWARD along the radial. ami_path uses
           the same compass convention as rectcord (0 = north, 90 = east),
           ratioed to LONG_MAX = 360°. */
        ami_path(stdout, a*DEGREE);
        /* Shift origin perpendicular to the drawing direction by half the
           character height, so the radial passes through the vertical
           center of the text (not the default top-left origin). The
           baseline side sits in direction (cos(a), sin(a)) from the
           origin in screen coords — we shift origin the opposite way. */
        f = a*0.01745329; /* degrees to radians (matches rectcord) */
        ami_cursorg(stdout,
                    x+tx1 - (int)(ami_chrsizy(stdout)/2.0 * cos(f)),
                    y-ty1 - (int)(ami_chrsizy(stdout)/2.0 * sin(f)));
        printf("this is a test string");
        a = a+10;

    }
    ami_path(stdout, LONG_MAX/4); /* restore default (90° / east-reading) */
    ami_binvis(stdout);
    prtcen(ami_maxy(stdout), "Polar text lines");
    waitnext();
    ami_auto(stdout, ON); /* re-enable the text grid */

    /* ************************* Graphical tabbing test ************************ */

    putchar('\f');
    grid();
    ami_auto(stdout, OFF);
    ami_font(stdout, AMI_FONT_TERM);
    for (i = 1; i <= 5; i++) {

        for (x = 1; x <= i; x++) putchar('\t');
        printf("Terminal tab: %d\n", i);

    }
    ami_clrtab(stdout);
    for (i = 1; i <= 5; i++) ami_settabg(stdout, i*43);
    for (i = 1; i <= 5; i++) {

        for (x = 1; x <= i; x++) putchar('\t');
        printf("Graphical tab number: %d position: %d\n", i, i*43);

    }
    ami_restabg(stdout, 2*43);
    ami_restabg(stdout, 4*43);
    printf("\n");
    printf("After removing tabs %d and %d\n", 2*43, 4*43);
    printf("\n");
    for (i = 1; i <= 5; i++) {

        for (x = 1; x <= i; x++) putchar('\t');
        printf("Graphical tab number: %d\n", i);

    }
    prtcen(ami_maxy(stdout), "Graphical tabbing test");
    waitnext();

    /* ************************** Picture draw test **************************** */

    putchar('\f');
    grid();
    ami_maknam(fn, 100, "tests", "mypic", "");
    ami_loadpict(stdout, 1, fn);
    printf("Picture size for 1: x: %lld y: %lld\n", AMI_LONG_CAST(ami_pictsizx(stdout, 1)),
           AMI_LONG_CAST(ami_pictsizy(stdout, 1)));
    ami_maknam(fn, 100, "tests", "mypic1", "bmp");
    ami_loadpict(stdout, 2, fn);
    printf("Picture size for 2: x: %lld y: %lld\n", AMI_LONG_CAST(ami_pictsizx(stdout, 2)),
           AMI_LONG_CAST(ami_pictsizy(stdout, 2)));
    printf("\n");
    y = ami_curyg(stdout);
    xspace = ami_maxxg(stdout)/20;
    xsize = ami_maxxg(stdout)/6;
    yspace = xspace;
    ysize = xsize;
    ami_picture(stdout, 1, xspace, y, xspace+xsize, y+ysize);
    ami_picture(stdout, 1, xspace+xsize, y+ysize, xspace+xsize*2, y+ysize*2);
    ami_picture(stdout, 1, xspace, y+ysize*2, xspace+xsize, y+ysize*3);
    ami_picture(stdout, 2, xspace+ami_maxxg(stdout)/2, y,
                          xspace+xsize+ami_maxxg(stdout)/2, y+ysize);
    ami_picture(stdout, 2, xspace+xsize+ami_maxxg(stdout)/2, y+ysize,
                          xspace+xsize*2+ami_maxxg(stdout)/2, y+ysize+ysize/2);

    ami_picture(stdout, 2, xspace+ami_maxxg(stdout)/2, y+ysize*2,
                          xspace+xsize/2+ami_maxxg(stdout)/2, y+ysize*3);
    ami_delpict(stdout, 1);
    ami_delpict(stdout, 2);
    prtcen(ami_maxy(stdout), "Picture draw test");
    waitnext();

    /* ********************** Invisible foreground test ************************ */

    putchar('\f');
    grid();
    printf("\n");
    ami_bover(stdout);
    ami_finvis(stdout);
    graphtest(1);
    ami_binvis(stdout);
    ami_fover(stdout);
    prtcen(ami_maxy(stdout), "Invisible foreground test");
    waitnext();
    ami_fover(stdout);

    /* ********************** Invisible background test ************************ */

    putchar('\f');
    grid();
    printf("\n");
    ami_binvis(stdout);
    ami_fover(stdout);
    graphtest(1);
    ami_binvis(stdout);
    ami_fover(stdout);
    prtcen(ami_maxy(stdout), "Invisible background test");
    waitnext();
    ami_bover(stdout);

    /* ************************** Xor foreground test ************************** */

    putchar('\f');
    grid();
    printf("\n");
    ami_bover(stdout);
    ami_fxor(stdout);
    graphtest(1);
    ami_binvis(stdout);
    ami_fover(stdout);
    prtcen(ami_maxy(stdout), "Xor foreground test");
    waitnext();
    ami_fover(stdout);

    /* ************************* Xor background test *************************** */

    putchar('\f');
    grid();
    printf("\n");
    ami_bxor(stdout);
    ami_fover(stdout);
    graphtest(1);
    ami_binvis(stdout);
    ami_fover(stdout);
    prtcen(ami_maxy(stdout), "Xor background test");
    waitnext();
    ami_bover(stdout);

    /* ************************** Graphical scrolling test **************************** */

    putchar('\f');
    grid();
    ami_binvis(stdout);
    prtcen(2, "Use up, down, right && left keys to scroll by pixel");
    prtcen(3, "Hit enter to continue");
    prtcen(4, "Note that edges will clear to green as screen moves");
    prtcen(ami_maxy(stdout), "Graphical scrolling test");
    ami_bcolor(stdout, ami_green);
    /* the user scrolls with the arrows, or the event file does: an automatic
       run goes through the loop only while the file has events for it, and
       captures a step for each */
    if ((!autorun || auto_event_ready()) && framenum >= tstlo) do {

        auto_event(stdin, &er);
        if (er.etype == ami_etup) ami_scrollg(stdout, 0, -1);
        if (er.etype == ami_etdown) ami_scrollg(stdout, 0, 1);
        if (er.etype == ami_etright) ami_scrollg(stdout, 1, 0);
        if (er.etype == ami_etleft) ami_scrollg(stdout, -1, 0);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (autorun && er.etype != ami_etenter) frmstep();

    } while (er.etype != ami_etenter && (!autorun || auto_event_ready()));
    frmmark(); /* capture where the scrolling left it */
    frmnext();
    ami_bover(stdout);
    ami_bcolor(stdout, ami_white);

    /* ************************** Graphical mouse movement test **************************** */

    putchar('\f');
    prtcen(2, "Move the mouse around");
    prtcen(3, "Hit Enter to continue");
    prtcen(ami_maxy(stdout), "Graphical mouse movement test");
    x = -1;
    y = -1;
    /* the user moves the mouse, or the event file does: an automatic run
       goes through the loop only while the file has events for it, and
       captures a step for each */
    if ((!autorun || auto_event_ready()) && framenum >= tstlo) do {

        auto_event(stdin, &er);
        if (er.etype == ami_etmoumovg) {

            if (x > 0 && y > 0) ami_line(stdout, x, y, er.moupxg, er.moupyg);
            x = er.moupxg;
            y = er.moupyg;

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (autorun && er.etype != ami_etenter) frmstep();

    } while (er.etype != ami_etenter && (!autorun || auto_event_ready()));
    frmmark(); /* capture what the mouse drew */
    frmnext();

    /* ************************** Animation test **************************** */

    squares();

    /* ************************** View offset test **************************** */

    putchar('\f');
    ami_auto(stdout, OFF); /* turn off autoscroll */
    ami_curvis(stdout, FALSE); /* turn off cursor */
    edge();
    /* move the origin right and down */
    ami_viewoffg(stdout, ami_maxxg(stdout)/2, ami_maxyg(stdout)/2);
    grid();
    ami_fcolor(stdout, ami_green);
    ami_frect(stdout, 0, 0, 100, 100);
    ami_cursorg(stdout, 1, -(ami_maxyg(stdout)/2));
    /* reset origin */
    ami_viewoffg(stdout, 0, 0);
    ami_fcolor(stdout, ami_black);
    prtcen(ami_maxy(stdout), "View offset test");
    prtcen(2, "The 1,1 origin is now at screen center\n");
    waitnext();
    ami_auto(stdout, ON); /* back to normal character */
    ami_curvis(stdout, TRUE); /* turn on cursor */

   /* ************************** View scale test **************************** */

    putchar('\f');
    ami_auto(stdout, OFF);
    ami_curvis(stdout, FALSE); /* turn off cursor */
    edge();
    ami_viewscale(stdout, 0.5f, 0.5f);
    grid();
    ami_fcolor(stdout, ami_green);
    ami_frect(stdout, 0, 0, 100, 100);
    ami_fcolor(stdout, ami_black);
    /* reset origin and scale */
    ami_viewscale(stdout, 1.0f, 1.0f);
    ami_viewoffg(stdout, 0, 0);
    prtcen(2, "Logical coordinates are now 1/2 size");
    prtcen(ami_maxy(stdout), "View scale test");
    waitnext();
    ami_auto(stdout, ON); /* back to normal character */
    ami_curvis(stdout, TRUE); /* turn on cursor */

    /* ************************ Viewport scaling test ************************** */

    putchar('\f');
    ami_auto(stdout, OFF);
    ami_curvis(stdout, FALSE); /* turn off cursor */
    fsiz = ami_chrsizy(stdout); /* save default font size */
    ami_font(stdout, AMI_FONT_SIGN);
    {
        int cx = ami_maxxg(stdout)/2; /* center x */
        int cy = ami_maxyg(stdout)/2; /* center y */
        int ww = ami_maxxg(stdout);   /* window width */
        int wh = ami_maxyg(stdout);   /* window height */
        int gs = 80; /* gate size */
        float vsx = 1.0f, vsy = 1.0f;
        int vox = 0, voy = 0;
        ami_evtrec er;
        int done = 0;
        int applied = 0; /* a key was applied: the redraw is a step */

        /* initial offset: center the drawing */
        vox = (int)(ww/2 - (cx-1) * vsx);
        voy = (int)(wh/2 - (cy-1) * vsy);
        ami_viewoffg(stdout, vox, voy);

        while (!done) {

            putchar('\f');
            /* draw boundary lines showing valid coordinate space */
            ami_fcolor(stdout, ami_cyan);
            ami_line(stdout, 1, 1, ami_maxxg(stdout), 1);
            ami_line(stdout, 1, ami_maxyg(stdout), ami_maxxg(stdout), ami_maxyg(stdout));
            ami_line(stdout, 1, 1, 1, ami_maxyg(stdout));
            ami_line(stdout, ami_maxxg(stdout), 1, ami_maxxg(stdout), ami_maxyg(stdout));
            /* draw a simple CMOS inverter schematic */
            ami_fcolor(stdout, ami_black);
            /* VDD and VSS rails */
            ami_line(stdout, cx-gs*2, cy-gs*2, cx+gs*2, cy-gs*2); /* VDD */
            ami_line(stdout, cx-gs*2, cy+gs*2, cx+gs*2, cy+gs*2); /* VSS */
            /* PMOS: gate on left, source/drain vertical */
            ami_rect(stdout, cx-gs/2, cy-gs*3/2, cx+gs/2, cy-gs/2); /* body */
            ami_line(stdout, cx-gs, cy-gs, cx-gs/2, cy-gs); /* gate */
            ami_line(stdout, cx, cy-gs*2, cx, cy-gs*3/2); /* source to VDD */
            ami_line(stdout, cx, cy-gs/2, cx, cy); /* drain to mid */
            /* NMOS: gate on left, source/drain vertical */
            ami_rect(stdout, cx-gs/2, cy+gs/2, cx+gs/2, cy+gs*3/2); /* body */
            ami_line(stdout, cx-gs, cy+gs, cx-gs/2, cy+gs); /* gate */
            ami_line(stdout, cx, cy+gs*3/2, cx, cy+gs*2); /* source to VSS */
            ami_line(stdout, cx, cy, cx, cy+gs/2); /* drain from mid */
            /* input line */
            ami_line(stdout, cx-gs*2, cy, cx-gs, cy);
            ami_line(stdout, cx-gs, cy-gs, cx-gs, cy+gs);
            /* output line */
            ami_line(stdout, cx, cy, cx+gs*2, cy);
            /* output dot */
            ami_fellipse(stdout, cx+gs/4-5, cy-5, cx+gs/4+5, cy+5);
            /* labels */
            ami_fontsiz(stdout, 20);
            ami_cursorg(stdout, cx-gs/4, cy-gs*2-5);
            printf("VDD");
            ami_cursorg(stdout, cx-gs/4, cy+gs*2+20);
            printf("VSS");
            ami_cursorg(stdout, cx-gs*2-30, cy+6);
            printf("IN");
            ami_cursorg(stdout, cx+gs*2+5, cy+6);
            printf("OUT");
            /* status at top, caption at bottom — draw at identity scale
               so UI text stays the same physical size regardless of zoom */
            ami_viewscale(stdout, 1.0f, 1.0f);
            ami_viewoffg(stdout, 0, 0);
            ami_fontsiz(stdout, fsiz);
            ami_font(stdout, AMI_FONT_TERM);
            {
                char sb[120];
                sprintf(sb, "Sx:%.2f Sy:%.2f Off:%d,%d PgUp/Dn=zoom Arrows=pan Home/End=Yzoom Enter=next",
                        vsx, vsy, vox, voy);
                prtcen(2, sb);
            }
            prtcen(ami_maxy(stdout), "View drawing scale test");
            if (autorun && applied) frmstep(); /* the view after the key */
            /* restore the current scale for next redraw */
            ami_viewscale(stdout, vsx, vsy);
            ami_viewoffg(stdout, vox, voy);
            /* wait for key: the user's, or the event file's; an automatic
               run goes on only while the file has one for it */
            if ((autorun && !auto_event_ready()) || framenum < tstlo)
                { done = 1; continue; }
            do { auto_event(stdin, &er); } while (er.etype != ami_etenter &&
                er.etype != ami_etterm && er.etype != ami_etpagu &&
                er.etype != ami_etpagd && er.etype != ami_etup &&
                er.etype != ami_etdown && er.etype != ami_etleft &&
                er.etype != ami_etright && er.etype != ami_ethomel &&
                er.etype != ami_etendl);
            if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
            else if (er.etype == ami_etenter) done = 1;
            applied = !done;
            if (er.etype == ami_etpagu) {

                vsx *= 1.25f;
                vsy *= 1.25f;
                ami_viewscale(stdout, vsx, vsy);
                vox = (int)(ww/2 - (cx-1) * vsx);
                voy = (int)(wh/2 - (cy-1) * vsy);
                ami_viewoffg(stdout, vox, voy);

            } else if (er.etype == ami_etpagd) {

                vsx /= 1.25f;
                vsy /= 1.25f;
                if (vsx < 0.01f) vsx = 0.01f;
                if (vsy < 0.01f) vsy = 0.01f;
                ami_viewscale(stdout, vsx, vsy);
                vox = (int)(ww/2 - (cx-1) * vsx);
                voy = (int)(wh/2 - (cy-1) * vsy);
                ami_viewoffg(stdout, vox, voy);

            } else if (er.etype == ami_ethomel) {

                vsy *= 1.25f;
                ami_viewscale(stdout, vsx, vsy);
                voy = (int)(wh/2 - (cy-1) * vsy);
                ami_viewoffg(stdout, vox, voy);

            } else if (er.etype == ami_etendl) {

                vsy /= 1.25f;
                if (vsy < 0.01f) vsy = 0.01f;
                ami_viewscale(stdout, vsx, vsy);
                voy = (int)(wh/2 - (cy-1) * vsy);
                ami_viewoffg(stdout, vox, voy);

            } else if (er.etype == ami_etup) {

                voy -= 20;
                ami_viewoffg(stdout, vox, voy);

            } else if (er.etype == ami_etdown) {

                voy += 20;
                ami_viewoffg(stdout, vox, voy);

            } else if (er.etype == ami_etleft) {

                vox -= 20;
                ami_viewoffg(stdout, vox, voy);

            } else if (er.etype == ami_etright) {

                vox += 20;
                ami_viewoffg(stdout, vox, voy);

            }

        }
        /* reset to identity: the label of the capture draws unscaled */
        ami_viewscale(stdout, 1.0f, 1.0f);
        ami_viewoffg(stdout, 0, 0);
        frmmark(); /* capture the view as left */
        frmnext();
        ami_curvis(stdout, FALSE); /* turn off cursor */

    }

    /* ************************* Block copy tests ************************ */

    blockcopy();

    /* ************************** Benchmarks **************************** */

    /* the benchmarks report times, which are never the same twice, so an
       automatic run ends before them */
    if (autorun) goto terminate;

    benchmarks:

    ami_bover(stdout);

    benchtest(linespeed, bnline1, 1);
    i = benchtab[bnline1].iter;
    s = benchtab[bnline1].time;
    printf("Line speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per line %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(linespeed, bnline10, 10);
    i = benchtab[bnline10].iter;
    s = benchtab[bnline10].time;
    printf("Line speed for width: 10, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per line %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(rectspeed, bnrect1, 1);
    i = benchtab[bnrect1].iter;
    s = benchtab[bnrect1].time;
    printf("Rectangle speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per rectangle %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(rectspeed, bnrect10, 10);
    i = benchtab[bnrect10].iter;
    s = benchtab[bnrect10].time;
    printf("Rectangle speed for width: 10, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per rectangle %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(rrectspeed, bnrrect1, 1);
    i = benchtab[bnrrect1].iter;
    s = benchtab[bnrrect1].time;
    printf("Rounded rectangle speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per rounded rectangle %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(rrectspeed, bnrrect10, 10);
    i = benchtab[bnrrect10].iter;
    s = benchtab[bnrrect10].time;
    printf("Rounded rectangle speed for width: 10, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per rounded rectangle %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(frectspeed, bnfrect, 1);
    i = benchtab[bnfrect].iter;
    s = benchtab[bnfrect].time;
    printf("Filled rectangle speed, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per filled rectangle %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(frrectspeed, bnfrrect, 1);
    i = benchtab[bnfrrect].iter;
    s = benchtab[bnfrrect].time;
    printf("Filled rounded rectangle speed, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per filled rounded rectangle %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(ellipsespeed, bnellipse1, 1);
    i = benchtab[bnellipse1].iter;
    s = benchtab[bnellipse1].time;
    printf("Ellipse speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per ellipse %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(ellipsespeed, bnellipse10, 10);
    i = benchtab[bnellipse10].iter;
    s = benchtab[bnellipse10].time;
    printf("Ellipse speed for width: 10, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per ellipse %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(fellipsespeed, bnfellipse, 1);
    i = benchtab[bnfellipse].iter;
    s = benchtab[bnfellipse].time;
    printf("Filled ellipse speed, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per filled ellipse %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(arcspeed, bnarc1, 1);
    i = benchtab[bnarc1].iter;
    s = benchtab[bnarc1].time;
    printf("Arc speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per arc %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(arcspeed, bnarc10, 1);
    i = benchtab[bnarc10].iter;
    s = benchtab[bnarc10].time;
    printf("Arc speed for width: 10, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per arc %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(farcspeed, bnfarc, 1);
    i = benchtab[bnfarc].iter;
    s = benchtab[bnfarc].time;
    printf("Filled arc speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per filled arc %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(fchordspeed, bnfchord, 1);
    i = benchtab[bnfchord].iter;
    s = benchtab[bnfchord].time;
    printf("Filled chord speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per filled chord %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(ftrianglespeed, bnftriangle, 1);
    i = benchtab[bnftriangle].iter;
    s = benchtab[bnftriangle].time;
    printf("Filled triangle speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per filled triangle %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    ami_bover(stdout);
    ami_fover(stdout);
    benchtest(ftextspeed, bntext, 1);
    i = benchtab[bntext].iter;
    s = benchtab[bntext].time;
    ami_home(stdout);
    printf("Text speed, with overwrite, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per write %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    ami_binvis(stdout);
    ami_fover(stdout);
    benchtest(ftextspeed, bntextbi, 1);
    i = benchtab[bntextbi].iter;
    s = benchtab[bntextbi].time;
    ami_home(stdout);
    ami_bover(stdout);
    printf("Text speed, invisible background, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per write %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(fpictspeed, bnpict, 1);
    i = benchtab[bnpict].iter;
    s = benchtab[bnpict].time;
    printf("Picture draw speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per picture %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    benchtest(fpictnsspeed, bnpictns, 1);
    i = benchtab[bnpictns].iter;
    s = benchtab[bnpictns].time;
    printf("No scale picture draw speed for width: 1, %d iterations %f seconds\n", i, s*0.0001);
    printf("Seconds per picture %f\n", s*0.0001/i);
    chkbrk(); /* check user break*/

    /* output table */

    fprintf(stderr, "\n");
    fprintf(stderr, "Benchmark table\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Type                        Seconds   Per fig\n");
    fprintf(stderr, "--------------------------------------------------\n");
    for (bi = bnline1; bi <= bnpictns; bi++) {

        switch (bi) { /* benchmark type */

            case bnline1:     fprintf(stderr, "line width 1                "); break;
            case bnline10:    fprintf(stderr, "line width 10               "); break;
            case bnrect1:     fprintf(stderr, "rectangle width 1           "); break;
            case bnrect10:    fprintf(stderr, "rectangle width 10          "); break;
            case bnrrect1:    fprintf(stderr, "rounded rectangle width 1   "); break;
            case bnrrect10:   fprintf(stderr, "rounded rectangle width 10  "); break;
            case bnfrect:     fprintf(stderr, "filled rectangle            "); break;
            case bnfrrect:    fprintf(stderr, "filled rounded rectangle    "); break;
            case bnellipse1:  fprintf(stderr, "ellipse width 1             "); break;
            case bnellipse10: fprintf(stderr, "ellipse width 10            "); break;
            case bnfellipse:  fprintf(stderr, "filled ellipse              "); break;
            case bnarc1:      fprintf(stderr, "arc width 1                 "); break;
            case bnarc10:     fprintf(stderr, "arc width 10                "); break;
            case bnfarc:      fprintf(stderr, "filled arc                  "); break;
            case bnfchord:    fprintf(stderr, "filled chord                "); break;
            case bnftriangle: fprintf(stderr, "filled triangle             "); break;
            case bntext:      fprintf(stderr, "text                        "); break;
            case bntextbi:    fprintf(stderr, "background invisible text   "); break;
            case bnpict:      fprintf(stderr, "Picture draw                "); break;
            case bnpictns:    fprintf(stderr, "No scaling picture draw     "); break;

        };
        fprintf(stderr, "%6.2f", benchtab[bi].time*0.0001);
        fprintf(stderr, "    ");
        fprintf(stderr, "%f", benchtab[bi].time*0.0001/benchtab[bi].iter);
        fprintf(stderr, "\n");

    }
    fflush(stderr); /* force print */

    terminate: /* terminate */

    putchar('\f');
    ami_auto(stdout, OFF);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, 50);
    prtceng(ami_maxy(stdout)/2, "Test complete");

    return (0);

}
