/** ****************************************************************************
*                                                                              *
*                    GRAPHICAL MODE LIBRARY FOR THE LINUX FRAME BUFFER         *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                              BSD LICENSE INFORMATION                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.                            *
*                                                                              *
* The graphics subset of Petit-Ami, drawn straight onto the Linux frame        *
* buffer, /dev/fb0, through the framebuffer.h interface. The program's whole   *
* display is the console screen: one full screen surface, no windows, no      *
* widgets. The drawing and text calls are the same ones the windowed          *
* backends carry, rasterized in software here exactly as the Wayland          *
* backend's display layer does -- the same span, line, ellipse, wedge and     *
* glyph walks -- landing in an offscreen buffer per Petit-Ami screen, and     *
* presented by copying damaged rectangles to the frame buffer.                *
*                                                                              *
* Buffered mode is the standard model: the program writes its buffer, the     *
* module presents it. select() switches among MAXCON buffers as on the        *
* other backends. Text comes from fontconfig-discovered scalable fonts        *
* rendered by FreeType, with the standard four logical fonts at the head      *
* of the list.                                                                *
*                                                                              *
* Events are the console's: keyboard characters from stdin in raw mode        *
* (arrow keys, function keys and editing keys translated from their escape    *
* sequences), timers and the frame timer through system_event, joysticks      *
* from /dev/input/js*, and etterm from SIGINT/SIGTERM. There is no mouse      *
* and there are no windows: the window management and widget calls error     *
* as unimplemented, the way a backend without a window manager linked does.   *
*                                                                              *
* Run it on a text console (ctrl-alt-F3): under a graphical desktop the       *
* compositor owns the screen and nothing is seen.                             *
*                                                                              *
*******************************************************************************/

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

/* linux definitions */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#ifndef NOJOYSTICK
#include <sys/ioctl.h>
#include <linux/joystick.h>
#endif

/* local definitions */
#include <localdefs.h>
#include <graphics.h>
#include "system_event.h"
#include "framebuffer.h"

/* FreeType/fontconfig for font rendering */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>

extern char *program_invocation_short_name;

/* the "standard character" sizes, used to form a pseudo-size for desktop
   character measurements */
#define STDCHRX 8
#define STDCHRY 12

#define MAXCON 100  /* number of screen contexts */
#define MAXTAB 250  /* total number of tabs possible per screen */
#define MAXPIC 50   /* total number of loadable pictures */
#define MAXLIN 250  /* maximum length of input buffered line */
#define MAXFIL 1000 /* maximum open files */
#define MAXFNM 250  /* number of filename characters in buffer */
#define MAXJOY 10   /* number of joysticks possible */
#define MAXSID 100  /* number of possible logical system events */
#define MINJST 1    /* minimum pixels for space in justification */
#define IOWIN  1    /* logical window number of input/output pair */

#define POINT  (0.353) /* point size in mm */
#define STRIKE (1.5)   /* strikeout percentage (from top of cell to baseline) */

/* size and offset of missing font character in cell y fractions */
#define MISCHRX 0.5  /* size x */
#define MISCHRY 0.75 /* size y */
#define MISOFFX 0.2  /* offset x */
#define MISOFFY 0.2  /* offset y */

#ifndef CONPNT
#define CONPNT 11 /* height of console font in points */
#endif

/* With no display information channel, the physical size of the console
   panel is unknown; the desktop's default of 96 dots per inch stands in.
   In dots per meter. */
#define FBDPM 3780

/* types of system vectors for override calls */
typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */
extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);
extern void ovr_read_nocancel(pread_t nfp, pread_t* ofp);
extern void ovr_write_nocancel(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open_nocancel(popen_t nfp, popen_t* ofp);
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);

/*******************************************************************************

Rasterizer

The software rasterizer the Wayland backend draws through, carried here
whole: an ARGB8888 canvas, a draw parameter record whose mix function is
first class, and the span, line, polygon, ellipse, wedge and glyph walks.
The one difference is presentation: a canvas may be the screen canvas,
and damage to that canvas is copied to the frame buffer at once.

*******************************************************************************/

typedef struct canvas {

    int       w, h;
    uint32_t* px;

} canvas;

/* pixel mix functions */
typedef enum {

    pd_mixcopy,  /* d = s */
    pd_mixxor,   /* d = d^s, color bits */
    pd_mixand,   /* d = d&s, color bits */
    pd_mixor,    /* d = d|s, color bits */
    pd_mixclear, /* d = 0 */
    pd_mixnone   /* no store (draw suppressed) */

} pd_mix;

/* line patterns */
typedef enum {

    pd_linesolid,
    pd_linedash,
    pd_linedot

} pd_linestyle;

typedef struct {

    uint32_t     fg;      /* foreground, ARGB8888 */
    uint32_t     bg;      /* background, ARGB8888 */
    pd_mix       mix;     /* pixel mix function */
    int          lw;      /* line width, pixels, >= 1 */
    pd_linestyle lstyle;  /* line pattern */
    canvas*      stipple; /* fill stipple, nonzero pixels paint; NULL none */
    int          stipx;   /* stipple origin */
    int          stipy;

} pd_draw;

/* forward: damage on the screen canvas presents to the frame buffer */
static void candmg(canvas* c, int x, int y, int w, int h);

static canvas* newcanvas(int w, int h)

{

    canvas* c;

    c = calloc(1, sizeof(canvas));
    if (!c) return (NULL);
    c->w = w > 0? w: 1;
    c->h = h > 0? h: 1;
    c->px = calloc((size_t)c->w*c->h, sizeof(uint32_t));
    if (!c->px) { free(c); return (NULL); }

    return (c);

}

static void freecanvas(canvas* c)

{

    if (c) { free(c->px); free(c); }

}

/* the pixel store: every drawing operation lands here, applying the mix
   function */
static inline void plot(canvas* c, int x, int y, uint32_t col, pd_mix mix)

{

    uint32_t* p;

    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    p = &c->px[(size_t)y*c->w+x];
    switch (mix) {

        case pd_mixcopy: *p = col; break;
        case pd_mixxor:  *p ^= col&0xffffff; break;
        case pd_mixand:  *p &= col|0xff000000; break;
        case pd_mixor:   *p |= col&0xffffff; break;
        case pd_mixnone: break;
        case pd_mixclear: *p = 0; break;
        default: *p = col; break;

    }

}

/* stipple test for stippled fills */
static int stipbit(pd_draw* dr, int x, int y)

{

    canvas* c;
    int sx, sy;

    c = dr->stipple;
    if (!c) return (1);
    sx = (x-dr->stipx)%c->w; if (sx < 0) sx += c->w;
    sy = (y-dr->stipy)%c->h; if (sy < 0) sy += c->h;

    return (c->px[(size_t)sy*c->w+sx] != 0);

}

/* horizontal span, the rasterizer's hot path */
static void hspan(canvas* c, pd_draw* dr, int x, int y, int w)

{

    uint32_t* p;
    uint32_t  col;
    int       i;

    if (y < 0 || y >= c->h) return;
    if (x < 0) { w += x; x = 0; }
    if (x+w > c->w) w = c->w-x;
    if (w <= 0) return;
    if (dr->stipple) {

        /* the stipple test is per pixel by nature; stay on the slow path */
        for (i = 0; i < w; i++)
            if (stipbit(dr, x+i, y)) plot(c, x+i, y, dr->fg, dr->mix);
        return;

    }
    p = &c->px[(size_t)y*c->w+x];
    col = dr->fg;
    switch (dr->mix) {

        case pd_mixnone:  break;
        case pd_mixclear: col = 0; /* fallthrough: clear is copy of zero */
        case pd_mixcopy:
        default:          for (i = 0; i < w; i++) *p++ = col;  break;
        case pd_mixxor:   col &= 0xffffff;
                          for (i = 0; i < w; i++) *p++ ^= col; break;
        case pd_mixand:   col |= 0xff000000;
                          for (i = 0; i < w; i++) *p++ &= col; break;
        case pd_mixor:    col &= 0xffffff;
                          for (i = 0; i < w; i++) *p++ |= col; break;

    }

}

/* full ellipse, filled: the scanline walk */
static void fillellipse(canvas* c, pd_draw* dr, int x, int y, unsigned w,
                        unsigned h)

{

    double rx = w/2.0, ry = h/2.0;
    double cx = x+rx, cy = y+ry;
    double dy, t;
    int    j, xs, xe;

    if (!w || !h) return;
    for (j = y; j < y+(int)h; j++) {

        dy = (j+0.5-cy)/ry;
        t = 1.0-dy*dy;
        if (t <= 0) continue;
        t = rx*sqrt(t);
        xs = (int)ceil(cx-t-0.5);
        xe = (int)floor(cx+t-0.5);
        if (xe >= xs) hspan(c, dr, xs, j, xe-xs+1);

    }

}

/* full ellipse, stroked: the annulus between the ellipse dilated and eroded
   by half the line width */
static void ringellipse(canvas* c, pd_draw* dr, int x, int y, unsigned w,
                        unsigned h)

{

    double lw = dr->lw > 1? dr->lw: 1;
    double rxo = w/2.0+lw/2, ryo = h/2.0+lw/2;
    double rxi = w/2.0-lw/2, ryi = h/2.0-lw/2;
    double cx = x+w/2.0, cy = y+h/2.0;
    double dy, t;
    int    j, xs, xe, xis, xie;

    for (j = (int)floor(cy-ryo); j <= (int)ceil(cy+ryo); j++) {

        dy = (j+0.5-cy)/ryo;
        t = 1.0-dy*dy;
        if (t <= 0) continue;
        t = rxo*sqrt(t);
        xs = (int)ceil(cx-t-0.5);
        xe = (int)floor(cx+t-0.5);
        if (xe < xs) continue;
        t = 0;
        if (rxi > 0 && ryi > 0) {

            dy = (j+0.5-cy)/ryi;
            t = 1.0-dy*dy;

        }
        if (t > 0) {

            /* the row crosses the hole: rim spans on either side */
            t = rxi*sqrt(t);
            xis = (int)ceil(cx-t-0.5);
            xie = (int)floor(cx+t-0.5);
            if (xie >= xis) {

                hspan(c, dr, xs, j, xis-xs);
                hspan(c, dr, xie+1, j, xe-xie);
                continue;

            }

        }
        hspan(c, dr, xs, j, xe-xs+1);

    }

}

/* the angular cut: a wedge for pies and strokes, a half plane for chords */
typedef struct {

    int    chord;      /* cut at the chord line */
    int    big;        /* sweep passes 180 degrees */
    double sx, sy;     /* sweep start endpoint ray */
    double ex, ey;     /* sweep end endpoint ray */
    double lx, ly, lc; /* chord line: lx*px+ly*py >= lc holds the arc */

} arccut;

static double crossz(double ax, double ay, double bx, double by)
{ return (ax*by-ay*bx); }

/* build the cut for a sweep from a1 by a2, in 64ths of a degree */
static void mkcut(arccut* ct, int a1, int a2, double rx, double ry, int chord)

{

    double sa, ea, ma;

    /* normalize to a positive sweep */
    if (a2 < 0) { a1 = a1+a2; a2 = -a2; }
    sa = a1/64.0*M_PI/180.0;
    ea = (a1+a2)/64.0*M_PI/180.0;
    ct->chord = chord;
    ct->big = a2 > 180*64;
    ct->sx = rx*cos(sa); ct->sy = ry*sin(sa);
    ct->ex = rx*cos(ea); ct->ey = ry*sin(ea);
    if (chord) {

        /* the line through the sweep endpoints, oriented so the side
           holding the arc's midpoint is kept */
        ma = (sa+ea)/2;
        ct->lx = ct->ey-ct->sy;
        ct->ly = ct->sx-ct->ex;
        ct->lc = ct->lx*ct->sx+ct->ly*ct->sy;
        if (ct->lx*rx*cos(ma)+ct->ly*ry*sin(ma) < ct->lc) {

            ct->lx = -ct->lx; ct->ly = -ct->ly; ct->lc = -ct->lc;

        }

    }

}

/* is the point (y-up, center relative) on the kept side of the cut */
static int incut(arccut* ct, double px, double py)

{

    if (ct->chord) return (ct->lx*px+ct->ly*py >= ct->lc);
    if (ct->big)
        return (!(crossz(ct->ex, ct->ey, px, py) > 0 &&
                  crossz(px, py, ct->sx, ct->sy) > 0));

    return (crossz(ct->sx, ct->sy, px, py) >= 0 &&
            crossz(px, py, ct->ex, ct->ey) >= 0);

}

/* the cut boundaries' crossings of the row, appended to the divisions */
static int cutrow(arccut* ct, double py, double* xs, int n)

{

    double t;

    if (ct->chord) {

        if (ct->lx != 0) xs[n++] = (ct->lc-ct->ly*py)/ct->lx;

    } else if (py == 0)
        /* the apex row: divide at the vertex, where every direction
           test ties, so the side midpoints decide */
        xs[n++] = 0;
    else {

        /* each boundary ray crosses the row at most once, outbound */
        if (ct->sy != 0) { t = py/ct->sy; if (t > 0) xs[n++] = t*ct->sx; }
        if (ct->ey != 0) { t = py/ct->ey; if (t > 0) xs[n++] = t*ct->ex; }

    }

    return (n);

}

/* emit the kept parts of the center-relative interval [l, r] on row j */
static void cutspans(canvas* c, pd_draw* dr, arccut* ct, int j,
                     double cx, double py, double l, double r)

{

    double xs[6];
    double v, mid;
    int    n, i, k;
    int    x1, x2;

    xs[0] = l; xs[1] = r;
    n = cutrow(ct, py, xs, 2);
    for (i = 1; i < n; i++) {

        v = xs[i];
        for (k = i-1; k >= 0 && xs[k] > v; k--) xs[k+1] = xs[k];
        xs[k+1] = v;

    }
    for (i = 0; i+1 < n; i++) {

        if (xs[i+1] <= l || xs[i] >= r) continue;
        v = xs[i] > l? xs[i]: l;
        mid = (v+(xs[i+1] < r? xs[i+1]: r))/2;
        if (!incut(ct, mid, py)) continue;
        x1 = (int)ceil(cx+v-0.5);
        x2 = (int)floor(cx+(xs[i+1] < r? xs[i+1]: r)-0.5);
        if (x2 >= x1) hspan(c, dr, x1, j, x2-x1+1);

    }

}

/* partial sweep, filled: pie closes through the center, chord rim to rim */
static void fillarcpart(canvas* c, pd_draw* dr, int x, int y, unsigned w,
                        unsigned h, int a1, int a2, int pie)

{

    double rx = w/2.0, ry = h/2.0;
    double cx = x+rx, cy = y+ry;
    double py, dy, t;
    arccut ct;
    int    j;

    if (!w || !h) return;
    mkcut(&ct, a1, a2, rx, ry, !pie);
    for (j = y; j < y+(int)h; j++) {

        py = -(j+0.5-cy);
        dy = py/ry;
        t = 1.0-dy*dy;
        if (t <= 0) continue;
        t = rx*sqrt(t);
        cutspans(c, dr, &ct, j, cx, py, -t, t);

    }

}

/* partial sweep, stroked: the annulus rows of the ring, cut to the wedge */
static void ringarcpart(canvas* c, pd_draw* dr, int x, int y, unsigned w,
                        unsigned h, int a1, int a2)

{

    double lw = dr->lw > 1? dr->lw: 1;
    double rxo = w/2.0+lw/2, ryo = h/2.0+lw/2;
    double rxi = w/2.0-lw/2, ryi = h/2.0-lw/2;
    double cx = x+w/2.0, cy = y+h/2.0;
    double py, dy, to, ti;
    arccut ct;
    int    j;

    mkcut(&ct, a1, a2, w/2.0, h/2.0, 0);
    for (j = (int)floor(cy-ryo); j <= (int)ceil(cy+ryo); j++) {

        py = -(j+0.5-cy);
        dy = py/ryo;
        to = 1.0-dy*dy;
        if (to <= 0) continue;
        to = rxo*sqrt(to);
        ti = 0;
        if (rxi > 0 && ryi > 0) {

            dy = py/ryi;
            ti = 1.0-dy*dy;
            ti = ti > 0? rxi*sqrt(ti): 0;

        }
        if (ti > 0) {

            /* the row crosses the hole: rim intervals on either side */
            cutspans(c, dr, &ct, j, cx, py, -to, -ti);
            cutspans(c, dr, &ct, j, cx, py, ti, to);

        } else cutspans(c, dr, &ct, j, cx, py, -to, to);

    }

}

/* filled rectangle, honoring the stipple */
static void fillrect(canvas* c, pd_draw* dr, int x, int y, int w, int h)

{

    int j;

    for (j = y; j < y+h; j++) hspan(c, dr, x, j, w);

}

/* dash pattern lengths by style and width */
static void dashpattern(pd_draw* dr, int* on, int* off)

{

    int w;

    w = dr->lw > 0? dr->lw: 1;
    if (dr->lstyle == pd_linedash) {

        *on  = w*4; if (*on  < 16) *on  = 16; if (*on  > 127) *on  = 127;
        *off = w*2; if (*off <  8) *off =  8; if (*off > 127) *off = 127;

    } else {

        *on  = w;   if (*on  <  2) *on  =  2; if (*on  > 127) *on  = 127;
        *off = w*3; if (*off <  6) *off =  6; if (*off > 127) *off = 127;

    }

}

/* solid or dashed unit line via Bresenham; wide lines are quads below */
static void thinline(canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)

{

    int dx, dy, sx, sy, err, e2;
    int don, doff, dpos, pen;

    dx = abs(x2-x1); dy = -abs(y2-y1);
    sx = x1 < x2? 1: -1; sy = y1 < y2? 1: -1;
    err = dx+dy;
    /* dash bookkeeping */
    don = doff = 0;
    if (dr->lstyle != pd_linesolid) dashpattern(dr, &don, &doff);
    dpos = 0; pen = 1;
    for (;;) {

        if (don) { /* dashed */

            pen = dpos < don;
            if (++dpos >= don+doff) dpos = 0;

        }
        if (pen) plot(c, x1, y1, dr->fg, dr->mix);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }

    }

}

/* polygon corner point, kept in the short range the raster grid uses */
typedef struct { short x, y; } ppoint;

/* scanline even-odd polygon fill */
static void fillpoly(canvas* c, pd_draw* dr, ppoint* pt, int n)

{

    int   miny, maxy, y, i, j, k;
    float xs[64];
    int   nx;
    int   x1, x2;

    if (n < 3) return;
    miny = maxy = pt[0].y;
    for (i = 1; i < n; i++) {

        if (pt[i].y < miny) miny = pt[i].y;
        if (pt[i].y > maxy) maxy = pt[i].y;

    }
    for (y = miny; y <= maxy; y++) {

        nx = 0;
        j = n-1;
        for (i = 0; i < n; i++) {

            int yi = pt[i].y, yj = pt[j].y;
            if ((yi <= y && yj > y) || (yj <= y && yi > y)) {

                if (nx < 64)
                    xs[nx++] = pt[i].x+
                        (float)(y-yi)/(yj-yi)*(pt[j].x-pt[i].x);

            }
            j = i;

        }
        /* insertion sort */
        for (i = 1; i < nx; i++) {

            float v = xs[i];
            for (k = i-1; k >= 0 && xs[k] > v; k--) xs[k+1] = xs[k];
            xs[k+1] = v;

        }
        for (i = 0; i+1 < nx; i += 2) {

            x1 = (int)ceilf(xs[i]);
            x2 = (int)floorf(xs[i+1]);
            hspan(c, dr, x1, y, x2-x1+1);

        }

    }

}

/* wide line as a filled quad, butt capped */
static void wideline(canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)

{

    float  dx, dy, len, nx, ny, hw;
    ppoint q[4];

    dx = x2-x1; dy = y2-y1;
    len = sqrtf(dx*dx+dy*dy);
    if (len < 0.001f) { fillrect(c, dr, x1-dr->lw/2, y1-dr->lw/2, dr->lw, dr->lw); return; }
    hw = dr->lw/2.0f;
    /* an axis-aligned butt-capped segment is a rectangle */
    if (y1 == y2) {

        int xa = x1 < x2? x1: x2, xb = x1 > x2? x1: x2;
        int yq1 = lroundf(y1-hw), yq2 = lroundf(y1+hw);

        fillrect(c, dr, xa, yq1, xb-xa+1, yq2-yq1);
        return;

    }
    if (x1 == x2) {

        int ya = y1 < y2? y1: y2, yb = y1 > y2? y1: y2;
        int xq1 = lroundf(x1-hw), xq2 = lroundf(x1+hw);

        fillrect(c, dr, xq1, ya, xq2-xq1, yb-ya+1);
        return;

    }
    nx = -dy/len*hw; ny = dx/len*hw;
    q[0].x = (short)lroundf(x1+nx); q[0].y = (short)lroundf(y1+ny);
    q[1].x = (short)lroundf(x2+nx); q[1].y = (short)lroundf(y2+ny);
    q[2].x = (short)lroundf(x2-nx); q[2].y = (short)lroundf(y2-ny);
    q[3].x = (short)lroundf(x1-nx); q[3].y = (short)lroundf(y1-ny);
    fillpoly(c, dr, q, 4);

}

static void anyline(canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)

{

    float dx, dy, len, t0, t1, p;
    int   don, doff;
    int   sx1, sy1, sx2, sy2;

    if (dr->lw <= 1) { thinline(c, dr, x1, y1, x2, y2); return; }
    if (dr->lstyle == pd_linesolid) {

        wideline(c, dr, x1, y1, x2, y2);
        return;

    }
    /* wide dashed: quads per dash segment */
    dx = x2-x1; dy = y2-y1;
    len = sqrtf(dx*dx+dy*dy);
    dashpattern(dr, &don, &doff);
    p = 0;
    while (p < len) {

        t0 = p/len;
        t1 = (p+don) > len? 1.0f: (p+don)/len;
        sx1 = (int)lroundf(x1+dx*t0); sy1 = (int)lroundf(y1+dy*t0);
        sx2 = (int)lroundf(x1+dx*t1); sy2 = (int)lroundf(y1+dy*t1);
        wideline(c, dr, sx1, sy1, sx2, sy2);
        p += don+doff;

    }

}

/* arc path points: bounding box, angles in 64ths of a degree, zero at
   three o'clock, positive counterclockwise. Returns point count */
static int arcpath(int x, int y, int w, int h, int a1, int a2,
                   ppoint* pt, int maxpt)

{

    float cx, cy, rx, ry, st, en, stp;
    int   n, i;

    cx = x+w/2.0f; cy = y+h/2.0f;
    rx = w/2.0f; ry = h/2.0f;
    st = a1/64.0f*(float)M_PI/180.0f;
    en = (a1+a2)/64.0f*(float)M_PI/180.0f;
    n = (int)(fabsf(en-st)*(rx > ry? rx: ry))+2;
    if (n > maxpt-2) n = maxpt-2;
    if (n < 2) n = 2;
    stp = (en-st)/(n-1);
    for (i = 0; i < n; i++) {

        pt[i].x = (short)lroundf(cx+rx*cosf(st+stp*i));
        /* the y axis grows downward; the angle convention is
           counterclockwise in right side up terms, so y subtracts */
        pt[i].y = (short)lroundf(cy-ry*sinf(st+stp*i));

    }

    return (n);

}

#define MAXARC 1024

static int drnoop(pd_draw* dr)
{ return (dr->mix == pd_mixnone); }

static void pd_point(canvas* c, pd_draw* dr, int x, int y)

{

    if (c && !drnoop(dr)) {

        plot(c, x, y, dr->fg, dr->mix);
        candmg(c, x, y, 1, 1);

    }

}

static void pd_line(canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)

{

    int lx, ly, hx, hy, b;

    if (c && !drnoop(dr)) {

        anyline(c, dr, x1, y1, x2, y2);
        lx = x1 < x2? x1: x2; hx = x1 > x2? x1: x2;
        ly = y1 < y2? y1: y2; hy = y1 > y2? y1: y2;
        b = dr->lw/2+1;
        candmg(c, lx-b, ly-b, hx-lx+2*b+1, hy-ly+2*b+1);

    }

}

static void pd_rect(canvas* c, pd_draw* dr, int x, int y, int w, int h)

{

    if (c && !drnoop(dr)) {

        if (dr->lw > 1 && dr->lstyle == pd_linesolid && !dr->stipple) {

            /* the frame as four fills with square-joined corners; the
               bands partition the frame, so xor and friends touch each
               pixel once */
            float hw = dr->lw/2.0f;
            int   xo = lroundf(x-hw),   yo = lroundf(y-hw);
            int   xi = lroundf(x+hw),   yi = lroundf(y+hw);
            int   x2 = lroundf(x+w-hw), y2 = lroundf(y+h-hw);
            int   xe = lroundf(x+w+hw), ye = lroundf(y+h+hw);

            if (xi >= x2 || yi >= y2)
                /* the walls meet: the frame is solid */
                fillrect(c, dr, xo, yo, xe-xo, ye-yo);
            else {

                fillrect(c, dr, xo, yo, xe-xo, yi-yo);   /* top */
                fillrect(c, dr, xo, y2, xe-xo, ye-y2);   /* bottom */
                fillrect(c, dr, xo, yi, xi-xo, y2-yi);   /* left */
                fillrect(c, dr, x2, yi, xe-x2, y2-yi);   /* right */

            }

        } else {

            anyline(c, dr, x, y, x+w, y);
            anyline(c, dr, x+w, y, x+w, y+h);
            anyline(c, dr, x+w, y+h, x, y+h);
            anyline(c, dr, x, y+h, x, y);

        }
        candmg(c, x-dr->lw, y-dr->lw, w+2*dr->lw+1, h+2*dr->lw+1);

    }

}

static void pd_frect(canvas* c, pd_draw* dr, int x, int y, int w, int h)

{

    if (c && !drnoop(dr)) {

        fillrect(c, dr, x, y, w, h);
        candmg(c, x, y, w, h);

    }

}

static void pd_fpoly(canvas* c, pd_draw* dr, const int* xy, int n)

{

    ppoint  stk[64];
    ppoint* pt;
    int     i, lx, ly, hx, hy;

    if (c && !drnoop(dr) && n > 0) {

        pt = n <= 64? stk: malloc((size_t)n*sizeof(ppoint));
        for (i = 0; i < n; i++) {

            pt[i].x = (short)xy[i*2];
            pt[i].y = (short)xy[i*2+1];

        }
        fillpoly(c, dr, pt, n);
        lx = hx = pt[0].x; ly = hy = pt[0].y;
        for (i = 1; i < n; i++) {

            if (pt[i].x < lx) lx = pt[i].x;
            if (pt[i].x > hx) hx = pt[i].x;
            if (pt[i].y < ly) ly = pt[i].y;
            if (pt[i].y > hy) hy = pt[i].y;

        }
        candmg(c, lx, ly, hx-lx+1, hy-ly+1);
        if (pt != stk) free(pt);

    }

}

static void pd_arc(canvas* c, pd_draw* dr, int x, int y, int w, int h,
                   int a1, int a2)

{

    ppoint pt[MAXARC];
    int    n, i;

    if (c && !drnoop(dr)) {

        if (abs(a2) >= 360*64 && dr->lstyle == pd_linesolid && !dr->stipple)
            /* the complete ellipse walks scanlines; the path polygon
               is for thin partial sweeps and patterned lines */
            ringellipse(c, dr, x, y, w, h);
        else if (dr->lw > 1 && dr->lstyle == pd_linesolid && !dr->stipple)
            /* a wide partial sweep walks the ring, cut to the wedge */
            ringarcpart(c, dr, x, y, w, h, a1, a2);
        else {

            n = arcpath(x, y, w, h, a1, a2, pt, MAXARC);
            for (i = 0; i+1 < n; i++)
                anyline(c, dr, pt[i].x, pt[i].y, pt[i+1].x, pt[i+1].y);

        }
        candmg(c, x-dr->lw, y-dr->lw, w+2*dr->lw+1, h+2*dr->lw+1);

    }

}

/* filled arc core: pie closes through the center, chord rim to rim */
static void farc(canvas* c, pd_draw* dr, int x, int y, int w, int h,
                 int a1, int a2, int pie)

{

    if (abs(a2) >= 360*64)
        /* the complete ellipse walks scanlines directly */
        fillellipse(c, dr, x, y, w, h);
    else fillarcpart(c, dr, x, y, w, h, a1, a2, pie);
    candmg(c, x, y, w+1, h+1);

}

static void pd_farcpie(canvas* c, pd_draw* dr, int x, int y, int w, int h,
                       int a1, int a2)

{

    if (c && !drnoop(dr)) farc(c, dr, x, y, w, h, a1, a2, 1);

}

static void pd_farcchord(canvas* c, pd_draw* dr, int x, int y, int w, int h,
                         int a1, int a2)

{

    if (c && !drnoop(dr)) farc(c, dr, x, y, w, h, a1, a2, 0);

}

static void pd_glyph(canvas* c, pd_draw* dr, int x, int y,
                     const uint8_t* mask, int mw, int mh, int stride,
                     int depth)

{

    int i, j, on;

    if (c && !drnoop(dr) && mask) {

        for (j = 0; j < mh; j++)
            for (i = 0; i < mw; i++) {

                /* the alpha threshold renders text 1-bit, the library's
                   character everywhere */
                if (depth == 1)
                    on = (mask[(size_t)j*stride+i/8] >> (7-i%8))&1;
                else
                    on = mask[(size_t)j*stride+i] > 127;
                if (on) plot(c, x+i, y+j, dr->fg, dr->mix);

            }
        candmg(c, x, y, mw, mh);

    }

}

static void pd_blit(canvas* dst, int dx, int dy,
                    canvas* src, int sx, int sy, int w, int h)

{

    int iw, ih, y;

    if (src && dst) {

        iw = w; ih = h;
        /* clip both rectangles into their canvases */
        if (sx < 0) { iw += sx; dx -= sx; sx = 0; }
        if (sy < 0) { ih += sy; dy -= sy; sy = 0; }
        if (dx < 0) { iw += dx; sx -= dx; dx = 0; }
        if (dy < 0) { ih += dy; sy -= dy; dy = 0; }
        if (sx+iw > src->w) iw = src->w-sx;
        if (sy+ih > src->h) ih = src->h-sy;
        if (dx+iw > dst->w) iw = dst->w-dx;
        if (dy+ih > dst->h) ih = dst->h-dy;
        if (iw > 0 && ih > 0) {

            for (y = 0; y < ih; y++)
                memcpy(&dst->px[(size_t)(dy+y)*dst->w+dx],
                       &src->px[(size_t)(sy+y)*src->w+sx], (size_t)iw*4);
            candmg(dst, dx, dy, iw, ih);

        }

    }

}

static void pd_scroll(canvas* c, int dx, int dy, int sx, int sy, int w, int h)

{

    int iw, ih, y, ydir;

    if (c) {

        iw = w; ih = h;
        if (sx < 0) { iw += sx; dx -= sx; sx = 0; }
        if (sy < 0) { ih += sy; dy -= sy; sy = 0; }
        if (dx < 0) { iw += dx; sx -= dx; dx = 0; }
        if (dy < 0) { ih += dy; sy -= dy; dy = 0; }
        if (sx+iw > c->w) iw = c->w-sx;
        if (sy+ih > c->h) ih = c->h-sy;
        if (dx+iw > c->w) iw = c->w-dx;
        if (dy+ih > c->h) ih = c->h-dy;
        if (iw > 0 && ih > 0) {

            /* row order against the copy direction: overlapping vertical
               moves must not read rows already written */
            ydir = dy > sy? -1: 1;
            for (y = ydir > 0? 0: ih-1; ydir > 0? y < ih: y >= 0; y += ydir)
                memmove(&c->px[(size_t)(dy+y)*c->w+dx],
                        &c->px[(size_t)(sy+y)*c->w+sx], (size_t)iw*4);
            candmg(c, dx, dy, iw, ih);

        }

    }

}

/* copy between canvases honoring a mix function */
static void blitmix(canvas* dst, int dx, int dy, canvas* src,
                    int sx, int sy, int w, int h, int mix)

{

    uint32_t* dp;
    uint32_t* sp;
    int x, y;
    uint32_t v;

    if (!dst || !src) return;
    if (mix == pd_mixcopy) { pd_blit(dst, dx, dy, src, sx, sy, w, h); return; }
    dp = dst->px;
    sp = src->px;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {

            if (sx+x < 0 || sx+x >= src->w || sy+y < 0 || sy+y >= src->h)
                continue;
            if (dx+x < 0 || dx+x >= dst->w || dy+y < 0 || dy+y >= dst->h)
                continue;
            v = sp[(size_t)(sy+y)*src->w+sx+x];
            switch (mix) {

                case pd_mixxor: dp[(size_t)(dy+y)*dst->w+dx+x] ^= v&0xffffff; break;
                case pd_mixand: dp[(size_t)(dy+y)*dst->w+dx+x] &= v|0xff000000; break;
                case pd_mixor:  dp[(size_t)(dy+y)*dst->w+dx+x] |= v&0xffffff; break;
                default:        dp[(size_t)(dy+y)*dst->w+dx+x] = v; break;

            }

        }
    candmg(dst, dx, dy, w, h);

}

/* Rescale an image from source to destination canvas, bilinear. Each
   destination pixel is taken at the source point its center falls on, the
   two laid over each other edge to edge, so the last source row and column
   reach the last of the destination (a ratio of sw-1 over dw left them at
   a fraction of their weight, and an edge line faded from a stretched
   copy). The neighbors are clamped to the source. */
static void rescale(canvas* dp, canvas* sp)

{

    unsigned int px1, px2, px3, px4;
    int sx, sy, dx, dy;
    int sx1, sy1; /* the neighbors, clamped */
    float xr, yr;
    float fx, fy; /* the source point sampled */
    float xd, yd; /* its fraction toward the next pixel */
    int b, r, g;
    uint32_t* src;
    uint32_t* dest;
    int sw, sh, dw, dh;
    size_t si, di;

    sw = sp->w; sh = sp->h;
    dw = dp->w; dh = dp->h;
    src = sp->px;
    dest = dp->px;
    xr = ((float)sw)/dw; /* find scaling ratio x */
    yr = ((float)sh)/dh; /* find scaling ratio y */
    /* copy and scale source to destination */
    for (dy = 0; dy < dh; dy++) {

        di = (size_t)dy*dw; /* set destination index */
        fy = (dy+0.5f)*yr-0.5f; /* the source row under this row's center */
        if (fy < 0) fy = 0;
        sy = (int)fy;
        yd = fy-sy;
        if (sy >= sh-1) { sy = sh-1; yd = 0; }
        sy1 = sy+1 < sh? sy+1: sy;
        for (dx = 0; dx < dw; dx++) {

            fx = (dx+0.5f)*xr-0.5f; /* the source column under this one */
            if (fx < 0) fx = 0;
            sx = (int)fx;
            xd = fx-sx;
            if (sx >= sw-1) { sx = sw-1; xd = 0; }
            sx1 = sx+1 < sw? sx+1: sx;
            si = (size_t)sy*sw+sx; /* find net source index */
            px1 = src[si]; /* get this pixel */
            px2 = src[(size_t)sy*sw+sx1]; /* get right pixel */
            px3 = src[(size_t)sy1*sw+sx]; /* get down pixel */
            px4 = src[(size_t)sy1*sw+sx1]; /* get down/right pixel */
            b = (px1&0xff)*(1-xd)*(1-yd)+(px2&0xff)*xd*(1-yd)+
                   (px3&0xff)*yd*(1-xd)+(px4&0xff)*xd*yd;
            g = ((px1>>8)&0xff)*(1-xd)*(1-yd)+((px2>>8)&0xff)*xd*(1-yd)+
                    ((px3>>8)&0xff)*yd*(1-xd)+((px4>>8)&0xff)*xd*yd;
            r = ((px1>>16)&0xff)*(1-xd)*(1-yd)+((px2>>16)&0xff)*xd*(1-yd)+
                  ((px3>>16)&0xff)*(yd)*(1-xd)+((px4>>16)&0xff)*xd*yd;
            dest[di++] = 0xff000000|(r<<16&0xff0000)|(g<<8&0xff00)|b;

        }

    }
    candmg(dp, 0, 0, dw, dh);

}

/*******************************************************************************

Module types and state

*******************************************************************************/

/* color mix modes */
typedef enum { mdnorm, mdinvis, mdxor, mdand, mdor } mode;

/* screen text attribute */
typedef enum {

    sablink,     /* blinking text (foreground) */
    sarev,       /* reverse video */
    saundl,      /* underline */
    sasuper,     /* superscript */
    sasubs,      /* subscripting */
    saital,      /* italic text */
    sabold,      /* bold text */
    sastkout,    /* strikeout text */
    sacondensed, /* condensed */
    saextended,  /* extended */
    saxlight,    /* extra light */
    salight,     /* light */
    saxbold,     /* bold */
    sahollow,    /* hollow */
    saraised     /* raised */

} scnatt;

/* font attributes, used to automatically recreate font specifications.
   Mapped into PA standard text attributes by each set routine. */
typedef enum {

    /* weights */
    xcnormal,        /* normal */
    xcmedium,        /* medium */
    xcbold,          /* bold */
    xcdemibold,      /* demibold */
    xcdark,          /* dark */
    xclight,         /* light */
    xcblack,         /* black */

    /* slants */
    xcroman,         /* no slant */
    xcital,          /* italic */
    xcoblique,       /* oblique */
    xcrital,         /* reverse italic */
    xcroblique,      /* reverse oblique */

    /* widths */
    xcnormalw,       /* normal */
    xcnarrow,        /* narrow */
    xccondensed,     /* condensed */
    xcsemicondensed, /* semicondensed */
    xcexpanded,      /* expanded */

    /* spacing */
    xcproportional,  /* proportional */
    xcmonospace,     /* monospaced */
    xcchar           /* character spaced */

} xwcaps;

typedef struct xcaplst {

    struct xcaplst* next;  /* next entry */
    int             caps;  /* font capabilities set */
    char*           path;  /* font file path for this variant */
    int             index; /* face index in font collection file */

} xcaplst;

/* font description entry */
typedef struct fontrec {

    char*           fn;     /* name of font */
    int             fix;    /* fixed pitch font flag */
    int             caps;   /* set of font capabilities */
    xcaplst*        caplst; /* list of all font capabilities */
    struct fontrec* next;   /* next font in list */

} fontrec, *fontptr;

typedef struct scncon* scnptr;
typedef struct scncon { /* screen context */

    ami_long    lwidth;      /* width of lines */
    ami_lstyle lstyle;   /* style of lines */
    /* the pixel and character dimensions and positions are kept in
       parallel for both characters and pixels */
    ami_long    maxx;        /* maximum characters in x */
    ami_long    maxy;        /* maximum characters in y */
    ami_long    maxxg;       /* maximum pixels in x */
    ami_long    maxyg;       /* maximum pixels in y */
    ami_long    curx;        /* current cursor location x */
    ami_long    cury;        /* current cursor location y */
    ami_long    curxg;       /* current cursor location in pixels x */
    ami_long    curyg;       /* current cursor location in pixels y */
    ami_long    angle;       /* character drawing angle */
    ami_long    fcrgb;       /* current writing foreground color in rgb */
    ami_long    bcrgb;       /* current writing background color in rgb */
    mode    fmod;        /* foreground mix mode */
    mode    bmod;        /* background mix mode */
    fontptr cfont;       /* active font entry */
    int     attr;        /* set of active attributes */
    ami_long    autof;       /* current status of scroll and wrap */
    ami_long    tab[MAXTAB]; /* tabbing array */
    ami_long    curv;        /* cursor visible */
    ami_long    offx;        /* viewport offset x */
    ami_long    offy;        /* viewport offset y */
    ami_long    wextx;       /* window extent x */
    ami_long    wexty;       /* window extent y */
    ami_long    vextx;       /* viewport extent x */
    ami_long    vexty;       /* viewport extent y */
    pd_draw* xcxt;       /* graphics context */
    canvas*  xbuf;       /* canvas for screen backing buffer */

} scncon;

typedef struct pict* picptr;
typedef struct pict { /* picture tracking record */

    struct pict* next; /* list of rescaled images */
    int          sx;   /* size in x */
    int          sy;   /* size in y */
    canvas*      xi;   /* pixel content */

} pict;

/* window description: the frame buffer carries exactly one, the screen */
typedef struct winrec* winptr;
typedef struct winrec {

    int          parlfn;            /* logical parent */
    ami_long     wid;               /* this window logical id */
    scnptr       screens[MAXCON];   /* screen contexts array */
    int          curdsp;            /* index for current display screen */
    int          curupd;            /* index for current update screen */
    /* global sets, applying to any new created screen buffer */
    ami_long     gmaxx;             /* maximum x size */
    ami_long     gmaxy;             /* maximum y size */
    ami_long     gmaxxg;            /* size of display in x */
    ami_long     gmaxyg;            /* size of display in y */
    ami_long     bufx;              /* buffer size x characters */
    ami_long     bufy;              /* buffer size y characters */
    ami_long     bufxg;             /* buffer size x pixels */
    ami_long     bufyg;             /* buffer size y pixels */
    int          gattr;             /* current attributes */
    ami_long     gauto;             /* state of auto */
    ami_long     gfcrgb;            /* foreground color in rgb */
    ami_long     gbcrgb;            /* background color in rgb */
    ami_long     gcurv;             /* state of cursor visible */
    fontptr      gcfont;            /* current font select */
    int          gfhigh;            /* physical em-square pixel size y */
    int          gfhighx;           /* physical em-square pixel size x */
    int          gfhigh_log;        /* logical em-square pixel size y */
    int          gfcellh;           /* target character cell height (pixels) */
    float        gfpoint;           /* current font point size */
    int          mischrx;           /* missing font character x */
    int          mischry;           /* missing font character y */
    int          misoffx;           /* missing font offset x */
    int          misoffy;           /* missing font offset y */
    mode         gfmod;             /* foreground mix mode */
    mode         gbmod;             /* background mix mode */
    ami_long     goffx;             /* viewport offset x (physical pixels) */
    ami_long     goffy;             /* viewport offset y (physical pixels) */
    float        vsx;               /* viewport scale x (default 1.0) */
    float        vsy;               /* viewport scale y (default 1.0) */
    ami_long     gwextx;            /* window extent x */
    ami_long     gwexty;            /* window extent y */
    ami_long     gvextx;            /* viewport extent x */
    ami_long     gvexty;            /* viewport extent y */
    int          linespace;         /* line spacing in pixels */
    int          charspace;         /* character spacing in pixels */
    ami_long     chrspcx;           /* extra space between characters */
    ami_long     chrspcy;           /* extra space between lines */
    int          baseoff;           /* font baseline offset from top */
    int          fcurdwn;           /* cursor on screen flag */
    int          mb1;               /* mouse button 1 state */
    int          mb2;               /* mouse button 2 state */
    int          mb3;               /* mouse button 3 state */
    ami_long     mpx, mpy;          /* mouse current position */
    ami_long     mpxg, mpyg;        /* mouse current position graphical */
    ami_long     joy1xs;            /* last joystick position 1x */
    ami_long     joy1ys;            /* last joystick position 1y */
    ami_long     joy1zs;            /* last joystick position 1z */
    ami_long     joy2xs;            /* last joystick position 2x */
    ami_long     joy2ys;            /* last joystick position 2y */
    ami_long     joy2zs;            /* last joystick position 2z */
    int          shres;             /* display screen pixels in x */
    int          svres;             /* display screen pixels in y */
    int          sdpmx;             /* display screen dots per meter x */
    int          sdpmy;             /* display screen dots per meter y */
    char         inpbuf[MAXLIN];    /* input line buffer */
    int          inpptr;            /* input line index */
    int          frmrun;            /* framing timer is running */
    int          timers[AMI_MAXTIM]; /* timer id array */
    int          frmsev;            /* frame timer system event */
    int          focus;             /* screen in focus */
    picptr       pictbl[MAXPIC];    /* loadable pictures table */
    int          bufmod;            /* buffered screen mode */
    int          visible;           /* window is visible */
    FT_Face      ftface;            /* current FreeType font face */

} winrec;

/* file tracking */
typedef struct filrec* filptr;
typedef struct filrec {

    FILE*  sfp;  /* file pointer used to establish entry, or NULL */
    winptr win;  /* associated window (if exists) */
    int    inw;  /* entry is input linked to window */
    int    inl;  /* this output file is linked to the input file, logical */

} filrec;

/* joystick tracking structure */
typedef struct joyrec* joyptr;
typedef struct joyrec {

    int fid;    /* joystick file id */
    int sid;    /* system event id */
    int axis;   /* number of joystick axes */
    int button; /* number of joystick buttons */
    ami_long ax;    /* joystick axis saves */
    ami_long ay;
    ami_long az;
    ami_long a4;
    ami_long a5;
    ami_long a6;
    int no;     /* logical number of joystick, 1-n */

} joyrec;

/* logical system event record */
typedef struct systrk* sevtptr;
typedef struct systrk {

    winptr  win; /* window associated with this event */
    int     tim; /* timer number associated with this event */
    int     frm; /* is a framing timer */
    int     joy; /* joystick number associated with this event */

} systrk;

/* error codes */
typedef enum {

    etimacc,  /* Timer access */
    efilopr,  /* Cannot perform operation on special file */
    einvscn,  /* Invalid screen number */
    einvhan,  /* Invalid handle */
    einvtab,  /* Invalid tab position */
    eatopos,  /* Cannot position text by pixel with auto on */
    eatoofg,  /* Cannot reenable auto off grid */
    eatoecb,  /* Cannot reenable auto outside screen */
    einvftn,  /* Invalid font number */
    eatoftc,  /* Cannot change fonts with auto enabled */
    einvfnm,  /* Invalid logical font number */
    efntemp,  /* Empty logical font */
    etabful,  /* Too many tabs set */
    eatotab,  /* Cannot use graphical tabs with auto on */
    estrinx,  /* String index out of range */
    epicfnf,  /* Picture file not found */
    epicftl,  /* Picture filename too large */
    ejstsys,  /* Cannot justify system font */
    efnotwin, /* File is not attached to a window */
    ewinuse,  /* Window id in use */
    ebufoff,  /* Buffered mode not enabled */
    estrato,  /* Cannot direct write string with auto on */
    enomem,   /* Out of memory */
    einvfil,  /* File is invalid */
    estdfnt,  /* Cannot find standard font */
    eftntl,   /* Font name too large */
    epicopn,  /* Cannot open picture file */
    ebadfmt,  /* Bad format of picture file */
    enoinps,  /* No input side for this window */
    evecaxe,  /* Cannot vector auxiliary event */
    eangato,  /* Cannot set character drawing angle in auto mode */
    eatoang,  /* Cannot reenable auto with non-90 degree text */
    esystem   /* System consistency check */

} errcod;

/* mode to function table */
static int mod2fnc[mdor+1] = {

    pd_mixcopy, /* mdnorm */
    pd_mixnone, /* mdinvis */
    pd_mixxor,  /* mdxor */
    pd_mixand,  /* mdand */
    pd_mixor    /* mdor */

};

/* PA queue structure. Its a bubble list. */
typedef struct paevtque {

    struct paevtque* next; /* next in list */
    struct paevtque* last; /* last in list */
    ami_evtrec       evt;  /* event data */

} paevtque;

/* logical to physical viewport transform; Petit-Ami primitives shift
   their 1-based logical coordinates by -1 before calling the rasterizer,
   so the transform operates entirely in 0-based space */
#define L2PX(w, v)  ((int)((v) * (w)->vsx) + (w)->goffx)
#define L2PY(w, v)  ((int)((v) * (w)->vsy) + (w)->goffy)
#define L2PW(w, n)  ((int)((n) * (w)->vsx))
#define L2PH(w, n)  ((int)((n) * (w)->vsy))
#define L2PDX(w, v) ((int)((v) * (w)->vsx))
#define L2PDY(w, v) ((int)((v) * (w)->vsy))

/* saved vectors to system calls */
static pread_t   ofpread;
static pread_t   ofpread_nocancel;
static pwrite_t  ofpwrite;
static pwrite_t  ofpwrite_nocancel;
static popen_t   ofpopen;
static popen_t   ofpopen_nocancel;
static pclose_t  ofpclose;
static pclose_t  ofpclose_nocancel;
static plseek_t  ofplseek;

/* frame buffer facts, from framebuffer.h at init */
static ami_long       fbrows;   /* device rows */
static ami_long       fbcols;   /* device columns */
static ami_long       fbpixsiz; /* bytes per pixel */
static ami_long       fbroff;   /* component byte offsets */
static ami_long       fbgoff;
static ami_long       fbboff;
static unsigned char* fbbase;   /* mapped device memory */
static int            fbfast;   /* device layout == canvas layout: rows copy */

static canvas*    scrcan;         /* the screen canvas, presented on damage */
static int        scrsup;         /* suppress presentation (batch draw) */
static winptr     thewin;         /* the one window, the screen */
static int        fend;           /* end of program ordered flag */
static ami_long   fautohold;      /* automatic hold on exit flag */
static int        errflg;         /* an error has been flagged */
static filptr     opnfil[MAXFIL]; /* open files table */
static int        xltwin[MAXFIL*2+1]; /* window equivalence table */
static ami_long   filwin[MAXFIL]; /* file to window equivalence table */
static fontptr    fntlst;         /* list of fonts */
static int        fntcnt;         /* number of fonts */
static FT_Library ftlibrary;      /* FreeType library instance */
static picptr     frepic;         /* free picture entries */
static int        numjoy;         /* number of joysticks found */
static joyptr     joytab[MAXJOY]; /* joystick control table */
static int        cfgcap;         /* "configuration" caps */
static ami_pevthan evthan[ami_etdsize+1]; /* array of event handler routines */
static ami_pevthan evtshan;       /* single master event handler routine */
static paevtque*  paqfre;         /* free PA event queue entries list */
static paevtque*  paqevt;         /* PA event input save queue */
static sevtptr    sidtab[MAXSID]; /* system event table */
static int        stdchrx;        /* standard/reference character size x */
static int        stdchry;        /* standard/reference character size y */
static int        kbdsev;         /* keyboard (stdin) system event id */
static int        intsev;         /* console interrupt system event */
static int        termsev;        /* terminate signal system event */
static int        inraw;          /* the console is in raw mode */
static int        ginit;          /* the module reached full initialization */
static struct termios saveterm;   /* console mode to restore */
static int        joyenb = TRUE;  /* enable joysticks */
static int        micefd = -1;    /* mouse device file */
static int        micesev;        /* mouse system event id */
static unsigned char micebuf[3];  /* mouse packet accumulator */
static int        micecnt;        /* bytes held */
static int        mptrx;          /* pointer position, 0 based device pixels */
static int        mptry;
static int        mptrvis;        /* pointer is shown (first movement seen) */

/* the font lock: a FreeType face is not thread safe and the glyph cache
   hangs off it; drawing calls nest, so the lock is recursive */
static pthread_mutex_t ftlock;

/* forward declarations */
static void plcchr(winptr win, char c);
static void setfnt(winptr win);
static void restore(winptr win);
static void curon(winptr win);
static void curoff(winptr win);
static void icursor(winptr win, ami_long x, ami_long y);
static void iclear(winptr win);
static void error(errcod e);

/*******************************************************************************

Present to the frame buffer

The screen canvas mirrors the display. Damage to it is copied to the
device at once, converting the canvas's ARGB8888 pixels to the device
layout. On the common device -- 32 bits, blue, green, red, alpha in
ascending bytes -- the layouts coincide and rows copy whole.

*******************************************************************************/

static void fbputpix(int x, int y, uint32_t v)

{

    unsigned char* dp;

    if (x < 0 || y < 0 || x >= fbcols || y >= fbrows) return;
    dp = fbbase+((size_t)y*fbcols+x)*fbpixsiz;
    dp[fbroff] = v>>16&0xff;
    dp[fbgoff] = v>>8&0xff;
    dp[fbboff] = v&0xff;

}

static void fbcopy(int x, int y, int w, int h)

{

    int            i, j;
    uint32_t       v;
    uint32_t*      sp;
    unsigned char* dp;

    /* clip to the screen */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x+w > fbcols) w = fbcols-x;
    if (y+h > fbrows) h = fbrows-y;
    if (w <= 0 || h <= 0) return;
    if (fbfast) {

        /* the device rows are the canvas rows */
        for (j = 0; j < h; j++)
            memcpy(fbbase+((size_t)(y+j)*fbcols+x)*4,
                   &scrcan->px[(size_t)(y+j)*scrcan->w+x], (size_t)w*4);

    } else {

        /* convert pixel by pixel to the device layout */
        for (j = 0; j < h; j++) {

            sp = &scrcan->px[(size_t)(y+j)*scrcan->w+x];
            dp = fbbase+((size_t)(y+j)*fbcols+x)*fbpixsiz;
            for (i = 0; i < w; i++) {

                v = *sp++;
                dp[fbroff] = v>>16&0xff;
                dp[fbgoff] = v>>8&0xff;
                dp[fbboff] = v&0xff;
                dp += fbpixsiz;

            }

        }

    }
    /* the mapping alone does not promise the glass follows */
    frame_flush(x, y, w, h);

}

/*******************************************************************************

Mouse pointer

The pointer is drawn on the device only, never into the screen canvas:
the canvas stays exactly what the program drew, captures carry no
pointer, and any present of the area it stands on simply repaints it.

*******************************************************************************/

/* the arrow, drawn from the hot point at its tip: X black, O white */
static const char* mptrimg[] = {

    "O",
    "OO",
    "OXO",
    "OXXO",
    "OXXXO",
    "OXXXXO",
    "OXXXXXO",
    "OXXXXXXO",
    "OXXXXXXXO",
    "OXXXXXXXXO",
    "OXXXXXOOOOO",
    "OXXOXXO",
    "OXOOXXO",
    "OO  OXXO",
    "O   OXXO",
    "     OXXO",
    "     OO"

};
#define MPTRH ((int)(sizeof(mptrimg)/sizeof(mptrimg[0])))
#define MPTRW 11

/* The sizing shapes, hot at their centers: the double arrows a manager
   above shows over a window's sizing edges, via grx_pointer(). */
static const char* mptrsizeh[] = {

    "   O           O",
    "  OXO         OXO",
    " OXXOOOOOOOOOOOXXO",
    "OXXXXXXXXXXXXXXXXXO",
    " OXXOOOOOOOOOOOXXO",
    "  OXO         OXO",
    "   O           O",

};
static const char* mptrsizev[] = {

    "   O",
    "  OXO",
    " OXXXO",
    "OXXXXXO",
    "OOOXOOO",
    "  OXO",
    "  OXO",
    "  OXO",
    "  OXO",
    "  OXO",
    "  OXO",
    "  OXO",
    "  OXO",
    "  OXO",
    "OOOXOOO",
    "OXXXXXO",
    " OXXXO",
    "  OXO",
    "   O",

};
static const char* mptrnwse[] = {

    "OOOOOO",
    "OXXXXO",
    "OXXXO",
    "OXXXXO",
    "OXOXXXO",
    "OO OXXXO",
    "    OXXXO",
    "     OXXXO OO",
    "      OXXXOXO",
    "       OXXXXO",
    "        OXXXO",
    "       OXXXXO",
    "       OOOOOO",

};
static const char* mptrnesw[] = {

    "       OOOOOO",
    "       OXXXXO",
    "        OXXXO",
    "       OXXXXO",
    "      OXXXOXO",
    "     OXXXO OO",
    "    OXXXO",
    "OO OXXXO",
    "OXOXXXO",
    "OXXXXO",
    "OXXXO",
    "OXXXXO",
    "OOOOOO",

};

/* the shape table: image, box, hot point */
static const struct mptrshp {

    const char** img;
    int          rows;
    int          w, h;
    int          hotx, hoty;

} mptrshptbl[] = {

    { mptrimg,   MPTRH, MPTRW, MPTRH, 0, 0 }, /* GRXP_ARROW, hot the tip */
    { mptrsizeh, 7, 19, 7, 9, 3 },    /* GRXP_SIZEH */
    { mptrsizev, 19, 7, 19, 3, 9 },   /* GRXP_SIZEV */
    { mptrnwse, 13, 13, 13, 6, 6 },   /* GRXP_NWSE */
    { mptrnesw, 13, 13, 13, 6, 6 },   /* GRXP_NESW */

};
#define MPTRSHAPES ((int)(sizeof(mptrshptbl)/sizeof(mptrshptbl[0])))
static int mptrshp = 0; /* the shape the pointer wears */

/* paint the pointer at its position, in its shape */
static void mptrdraw(void)

{

    const struct mptrshp* sp = &mptrshptbl[mptrshp];
    int         i, j;
    const char* r;

    for (j = 0; j < sp->h; j++) {

        r = sp->img[j];
        for (i = 0; r[i]; i++)
            if (r[i] == 'X')
                fbputpix(mptrx-sp->hotx+i, mptry-sp->hoty+j, 0x000000);
            else if (r[i] == 'O')
                fbputpix(mptrx-sp->hotx+i, mptry-sp->hoty+j, 0xffffff);

    }
    frame_flush(mptrx-sp->hotx, mptry-sp->hoty, sp->w, sp->h);

}

/* the box the pointer's shape covers */
static void mptrbox(int* x, int* y, int* w, int* h)

{

    const struct mptrshp* sp = &mptrshptbl[mptrshp];

    *x = mptrx-sp->hotx;
    *y = mptry-sp->hoty;
    *w = sp->w;
    *h = sp->h;

}

/* present a rectangle: the raw copy, then the pointer back on top if
   the rectangle touched it */
static void fbpresent(int x, int y, int w, int h)

{

    int px, py, pw, ph;

    fbcopy(x, y, w, h);
    mptrbox(&px, &py, &pw, &ph);
    if (mptrvis && x < px+pw && x+w > px && y < py+ph && y+h > py)
        mptrdraw();

}

/* move the pointer: restore the canvas under the old place, draw anew */
static void mptrmove(int nx, int ny)

{

    int px, py, pw, ph;

    mptrbox(&px, &py, &pw, &ph);
    mptrx = nx; mptry = ny;
    if (mptrvis) fbcopy(px, py, pw, ph);
    mptrvis = TRUE;
    mptrdraw();

}

/*******************************************************************************

Set pointer shape

A backdoor for the manager above: the pointer wears one of the shapes
0 arrow, 1 horizontal sizing, 2 vertical sizing, 3 nw-se sizing,
4 ne-sw sizing. The display draws its own pointer here, so the shape
feedback a desktop would give over a sizing edge is given by this.

*******************************************************************************/

void grx_pointer(int shape)

{

    int px, py, pw, ph;

    if (shape < 0 || shape >= MPTRSHAPES || shape == mptrshp) return;
    mptrbox(&px, &py, &pw, &ph);
    mptrshp = shape;
    if (mptrvis) { fbcopy(px, py, pw, ph); mptrdraw(); }

}

/* canvas damage: the screen canvas presents, others are memory only */
static void candmg(canvas* c, int x, int y, int w, int h)

{

    if (c == scrcan && !scrsup) fbpresent(x, y, w, h);

}

/*******************************************************************************

Error handler

Prints the reason on stderr and ends the program.

*******************************************************************************/

static const char* errstr(errcod e)

{

    const char* s;

    switch (e) {

        case etimacc:  s = "No timer available"; break;
        case efilopr:  s = "Cannot perform operation on special file"; break;
        case einvscn:  s = "Invalid screen number"; break;
        case einvhan:  s = "Invalid file handle"; break;
        case einvtab:  s = "Tab position specified off screen"; break;
        case eatopos:  s = "Cannot position text by pixel with auto on"; break;
        case eatoofg:  s = "Cannot reenable auto off grid"; break;
        case eatoecb:  s = "Cannot reenable auto outside screen"; break;
        case einvftn:  s = "Invalid font number"; break;
        case eatoftc:  s = "Cannot change fonts with auto enabled"; break;
        case einvfnm:  s = "Invalid logical font number"; break;
        case efntemp:  s = "Logical font number has no assigned font"; break;
        case etabful:  s = "Too many tabs set"; break;
        case eatotab:  s = "Cannot set off grid tabs with auto on"; break;
        case estrinx:  s = "String index out of range"; break;
        case epicfnf:  s = "Picture file not found"; break;
        case epicftl:  s = "Picture filename too large"; break;
        case ejstsys:  s = "Cannot justify system font"; break;
        case efnotwin: s = "File is not attached to a window"; break;
        case ewinuse:  s = "Window id in use"; break;
        case ebufoff:  s = "Buffered mode not enabled"; break;
        case estrato:  s = "Cannot direct print with auto mode"; break;
        case enomem:   s = "Out of memory"; break;
        case einvfil:  s = "File is invalid"; break;
        case estdfnt:  s = "Cannot find standard font"; break;
        case eftntl:   s = "Font name too large"; break;
        case epicopn:  s = "Cannot open picture file"; break;
        case ebadfmt:  s = "Bad format of picture file"; break;
        case enoinps:  s = "No input side for this window"; break;
        case evecaxe:  s = "Cannot vector auxiliary event"; break;
        case eangato:  s = "Cannot set character drawing angle in auto mode"; break;
        case eatoang:  s = "Cannot reenable auto with non-90 degree text"; break;
        case esystem:  s = "System consistency check, please contact vendor"; break;
        default:       s = "Unknown error"; break;

    }

    return (s);

}

static void error(errcod e)

{

    fprintf(stderr, "*** Error: graphics: %s\n", errstr(e));
    fflush(stderr);
    errflg = TRUE; /* flag error occurred */

    exit(1);

}

/* a window management or widget call with no manager linked */
static void unimp(const char* name)

{

    fprintf(stderr, "*** Error: graphics: %s: unimplemented\n", name);
    fflush(stderr);
    errflg = TRUE;

    exit(1);

}

/*******************************************************************************

Memory allocation

Wrapper around malloc that treats running out as terminal.

*******************************************************************************/

static void *imalloc(size_t size)

{

    void* ptr;

    ptr = malloc(size);
    if (!ptr) error(enomem);

    return (ptr);

}

static void ifree(void* ptr)

{

    free(ptr);

}

/*******************************************************************************

Copy critical string

Copies a string to a critical output buffer of the given length. If the
string fills the buffer, the terminating zero is left off. Otherwise, the
result is zero terminated.

*******************************************************************************/

static void cpycrit(char* d, ami_long dl, const char* s)

{

    ami_long l; /* length of source string */

    l = strlen(s); /* find length of source */
    if (l > dl) error(eftntl); /* string too large for buffer */
    memcpy(d, s, l); /* copy string into place */
    if (l < dl) d[l] = 0; /* zero terminate if buffer not entirely filled */

}

/*******************************************************************************

Translate colors code

Translates an independent to a terminal specific primary RGB color code.

*******************************************************************************/

static int colnum(ami_color c)

{

    int n;

    /* translate color number */
    switch (c) { /* color */

        case ami_black:     n = 0x000000; break;
        case ami_white:     n = 0xffffff; break;
        case ami_red:       n = 0xff0000; break;
        case ami_green:     n = 0x00ff00; break;
        case ami_blue:      n = 0x0000ff; break;
        case ami_cyan:      n = 0x00ffff; break;
        case ami_yellow:    n = 0xffff00; break;
        case ami_magenta:   n = 0xff00ff; break;
        case ami_backcolor: n = 0xf7f7f7; break;
        default:            n = 0x000000; break;

    }

    return (n); /* return number */

}

/*******************************************************************************

Translate rgb to color code

Translates a ratioed LONG_MAX graph color to the packed 32 bit form, with
red, green and blue bytes.

*******************************************************************************/

static int rgb2xwin(ami_long r, ami_long g, ami_long b)

{

   return ((int)((r/(LONG_MAX/256+1))*65536+(g/(LONG_MAX/256+1))*256+
                 (b/(LONG_MAX/256+1))));

}

/*******************************************************************************

Create graphics context

Creates a new drawing context with the defaults.

*******************************************************************************/

static pd_draw* gcnew(void)

{

    pd_draw* g;

    g = calloc(1, sizeof(pd_draw));
    if (!g) error(enomem);
    g->mix = pd_mixcopy;
    g->lw = 1;
    g->fg = 0;
    g->bg = 0xffffff;

    return (g);

}

/*******************************************************************************

Font list management

The font list is loaded from fontconfig, one entry per family, each
carrying the list of its variants' capabilities and file paths. The four
standard logical fonts are selected by name and moved to the head of the
list, terminal first.

*******************************************************************************/

/* search for font by name and fixed pitch status */
static fontptr fndfnt(char* fn, int fix)

{

    fontptr p;
    fontptr fp;

    fp = NULL;
    p = fntlst; /* index top of font list */
    while (!fp && p) { /* traverse font list */

        if (!strcmp(p->fn, fn) && p->fix == fix) fp = p;
        else p = p->next; /* next entry */

    }

    return (fp); /* return found font */

}

/* find font by family substring, case-insensitive */
static fontptr fndfntsub(const char* sub, int fix)

{

    fontptr p;
    const char *s, *n;
    int sl, nl;

    sl = strlen(sub);
    p = fntlst;
    while (p) {

        if (p->fix == fix) {

            /* case-insensitive substring search in font name */
            nl = strlen(p->fn);
            for (n = p->fn; nl >= sl; n++, nl--) {

                for (s = sub; *s; s++)
                    if (tolower((unsigned char)*s) !=
                        tolower((unsigned char)n[s-sub])) break;
                if (!*s) return p; /* found match */

            }

        }
        p = p->next;

    }

    return NULL;

}

/* search for font by name */
static fontptr schfnt(char* fn)

{

    fontptr p;
    fontptr fp;

    fp = NULL;
    p = fntlst; /* index top of font list */
    while (!fp && p) { /* traverse font list */

        if (!strcmp(p->fn, fn)) fp = p;
        else p = p->next; /* next entry */

    }

    return (fp); /* return found font */

}

/* delete font entry from the global font list; does not recycle it */
static void delfnt(fontptr fp)

{

    fontptr flp;
    fontptr fl;

    if (fp == fntlst) fntlst = fntlst->next; /* gap from top of list */
    else { /* search whole list */

        /* find last pointer */
        flp = fntlst;
        fl = NULL; /* set no last */
        while (flp && flp != fp) { /* find last */

            fl = flp; /* set last */
            flp = flp->next; /* go next */

        }
        if (!fl) error(esystem); /* should have found it */
        fl->next = fp->next; /* gap over entry */

    }

}

/* preselect standard fonts: the list of 4 standard fonts, reordered so
   they are at the top */
static void stdfont(void)

{

    fontptr nfl; /* new font list */
    fontptr fp;  /* font pointer */
    fontptr sp;  /* sign font pointer */

    /* select first 4 fonts for standard fonts */
    nfl = NULL; /* clear target list */
    /* search 1: terminal (fixed pitch) font */
    fp = fndfntsub("courier 10 pitch", TRUE);
    if (!fp) fp = fndfntsub("courier new", TRUE);
    if (!fp) fp = fndfntsub("dejavu sans mono", TRUE);
    if (!fp) fp = fndfntsub("liberation mono", TRUE);
    if (!fp) fp = fndfntsub("noto sans mono", TRUE);
    if (!fp) {

        /* last resort: find any fixed pitch font */
        fontptr p = fntlst;
        while (p && !fp) { if (p->fix) fp = p; else p = p->next; }

    }
    if (!fp) error(estdfnt);
    delfnt(fp);
    fp->next = nfl;
    nfl = fp;
    /* search 2: book (serif) font */
    fp = fndfntsub("bitstream charter", FALSE);
    if (!fp) fp = fndfntsub("dejavu serif", FALSE);
    if (!fp) fp = fndfntsub("liberation serif", FALSE);
    if (!fp) fp = fndfntsub("noto serif", FALSE);
    if (!fp) fp = fndfntsub("serif", FALSE);
    if (!fp) error(estdfnt);
    delfnt(fp);
    fp->next = nfl;
    nfl = fp;
    /* search 3: sign (sans serif) font */
    fp = fndfntsub("dejavu sans:", FALSE);
    if (!fp) fp = fndfntsub("liberation sans", FALSE);
    if (!fp) fp = fndfntsub("noto sans:", FALSE);
    if (!fp) fp = fndfntsub("ubuntu:", FALSE);
    if (!fp) fp = fndfntsub("sans", FALSE);
    if (!fp) error(estdfnt);
    delfnt(fp);
    fp->next = nfl;
    nfl = fp;
    sp = fp; /* save sign font */
    /* search 4: technical font, make copy of sign */
    fp = (fontptr)imalloc(sizeof(fontrec));
    /* copy sign font parameters */
    fp->fn = sp->fn;
    fp->fix = sp->fix;
    fp->caps = sp->caps;
    fp->caplst = sp->caplst;
    fp->next = nfl; /* insert to target list */
    nfl = fp;
    fntcnt++; /* add to font count */
    /* transfer all remaining entries to the font list */
    while (fntlst) {

        fp = fntlst;
        fntlst = fntlst->next; /* gap from list */
        fp->next = nfl; /* insert to new list */
        nfl = fp;

    }
    /* now insert back to master list, and reverse entries to order */
    while (nfl) {

        fp = nfl;
        nfl = nfl->next; /* gap from list */
        fp->next = fntlst; /* insert to new list */
        fntlst = fp;

    }

}

/* append trimmed lowercase string; returns the advanced build pointer */
static char* apptrim(char* dp, const char* s)

{

    const char* e; /* end of trimmed source */

    while (*s && isspace((unsigned char)*s)) s++; /* skip leading whitespace */
    e = s+strlen(s); /* find end of source */
    while (e > s && isspace((unsigned char)e[-1])) e--; /* trim trailing */
    while (s < e) *dp++ = tolower((unsigned char)*s++); /* copy lowercased */

    return (dp); /* return advanced pointer */

}

/* load the font list using fontconfig; only scalable fonts, named
   "foundry: family: iso10646-1" for compatibility with the internal
   naming system */
static void getfonts(void)

{

    FcPattern*   pat;      /* fontconfig search pattern */
    FcObjectSet* os;       /* requested properties */
    FcFontSet*   fs;       /* result font set */
    int          ifc;      /* internal font count */
    char         buf[250]; /* buffer for string name */
    int          i;
    char*        dp;
    fontptr      flp;
    xcaplst*     xcl;
    FcChar8*     fcfamily;
    FcChar8*     fcfoundry;
    FcChar8*     fcfile;
    int          fcweight, fcslant, fcwidth, fcspacing, fcindex;
    FcCharSet*   fccs;

    /* query fontconfig for all scalable fonts */
    pat = FcPatternCreate();
    FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
    os = FcObjectSetBuild(FC_FAMILY, FC_FOUNDRY, FC_STYLE, FC_WEIGHT,
                          FC_SLANT, FC_WIDTH, FC_SPACING, FC_FILE, FC_INDEX,
                          FC_CHARSET, NULL);
    fs = FcFontList(NULL, pat, os);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);

    fntlst = NULL; /* clear destination list */
    ifc = 0; /* clear internal font counter */

    for (i = 0; i < fs->nfont; i++) {

        FcPattern* font = fs->fonts[i];

        /* get font properties */
        if (FcPatternGetString(font, FC_FAMILY, 0, &fcfamily) != FcResultMatch)
            continue;
        if (FcPatternGetString(font, FC_FILE, 0, &fcfile) != FcResultMatch)
            continue;

        /* skip fonts that don't cover basic Latin (A-Z, a-z) */
        if (FcPatternGetCharSet(font, FC_CHARSET, 0, &fccs) == FcResultMatch) {

            if (!FcCharSetHasChar(fccs, 'A') || !FcCharSetHasChar(fccs, 'z'))
                continue;

        }

        /* get foundry, default to empty */
        if (FcPatternGetString(font, FC_FOUNDRY, 0, &fcfoundry) != FcResultMatch)
            fcfoundry = (FcChar8*)"unknown";

        /* get face index for collection files */
        if (FcPatternGetInteger(font, FC_INDEX, 0, &fcindex) != FcResultMatch)
            fcindex = 0;

        /* construct display name: "foundry: family: iso10646-1", trimming
           leading and trailing whitespace from each component */
        dp = buf;
        dp = apptrim(dp, (const char*)fcfoundry);
        *dp++ = ':'; *dp++ = ' ';
        dp = apptrim(dp, (const char*)fcfamily);
        *dp++ = ':'; *dp++ = ' ';
        {
            const char* cs = "iso10646-1";
            while (*cs) *dp++ = *cs++;
        }
        *dp = 0;

        /* search for duplicates */
        flp = schfnt(buf);

        if (!flp) { /* entry is unique */

            flp = (fontptr)imalloc(sizeof(fontrec));
            flp->fn = (char*)imalloc(strlen(buf)+1);
            strcpy(flp->fn, buf);
            flp->caps = 0;
            flp->caplst = NULL;
            flp->next = fntlst;
            fntlst = flp;
            ifc++;

        }

        /* create capability entry for this variant */
        xcl = (xcaplst*)imalloc(sizeof(xcaplst));
        xcl->caps = 0;
        xcl->next = flp->caplst;
        flp->caplst = xcl;

        /* store font file path and index */
        xcl->path = (char*)imalloc(strlen((char*)fcfile)+1);
        strcpy(xcl->path, (char*)fcfile);
        xcl->index = fcindex;

        /* map fontconfig weight to capabilities */
        if (FcPatternGetInteger(font, FC_WEIGHT, 0, &fcweight) == FcResultMatch) {

            if (fcweight <= FC_WEIGHT_LIGHT) xcl->caps |= BIT(xclight);
            else if (fcweight <= FC_WEIGHT_REGULAR) xcl->caps |= BIT(xcnormal);
            else if (fcweight <= FC_WEIGHT_MEDIUM) xcl->caps |= BIT(xcmedium);
            else if (fcweight <= FC_WEIGHT_DEMIBOLD) xcl->caps |= BIT(xcdemibold);
            else if (fcweight <= FC_WEIGHT_BOLD) xcl->caps |= BIT(xcbold);
            else if (fcweight <= FC_WEIGHT_BLACK) xcl->caps |= BIT(xcblack);
            else xcl->caps |= BIT(xcbold);

        } else xcl->caps |= BIT(xcnormal);

        /* map fontconfig slant to capabilities */
        if (FcPatternGetInteger(font, FC_SLANT, 0, &fcslant) == FcResultMatch) {

            if (fcslant == FC_SLANT_ROMAN) xcl->caps |= BIT(xcroman);
            else if (fcslant == FC_SLANT_ITALIC) xcl->caps |= BIT(xcital);
            else if (fcslant == FC_SLANT_OBLIQUE) xcl->caps |= BIT(xcoblique);

        } else xcl->caps |= BIT(xcroman);

        /* map fontconfig width to capabilities */
        if (FcPatternGetInteger(font, FC_WIDTH, 0, &fcwidth) == FcResultMatch) {

            if (fcwidth <= FC_WIDTH_CONDENSED) xcl->caps |= BIT(xccondensed);
            else if (fcwidth <= FC_WIDTH_SEMICONDENSED)
                xcl->caps |= BIT(xcsemicondensed);
            else if (fcwidth <= FC_WIDTH_NORMAL) xcl->caps |= BIT(xcnormalw);
            else xcl->caps |= BIT(xcexpanded);

        } else xcl->caps |= BIT(xcnormalw);

        /* map fontconfig spacing to capabilities */
        if (FcPatternGetInteger(font, FC_SPACING, 0, &fcspacing) == FcResultMatch) {

            if (fcspacing == FC_PROPORTIONAL) xcl->caps |= BIT(xcproportional);
            else if (fcspacing == FC_MONO || fcspacing == FC_DUAL)
                xcl->caps |= BIT(xcmonospace);
            else if (fcspacing == FC_CHARCELL) xcl->caps |= BIT(xcchar);

        } else xcl->caps |= BIT(xcproportional);

        /* form set of all capabilities */
        flp->caps |= xcl->caps;

        /* set font flags based on spacing */
        flp->fix = (flp->caps & BIT(xcmonospace)) || (flp->caps & BIT(xcchar));

    }

    FcFontSetDestroy(fs);

    fntcnt = ifc; /* set internal font count */

    /* select the standard fonts */
    stdfont();

}

/*******************************************************************************

Attribute to capability matching

Finds the set of font capabilities that are requested in the set of PA
attributes, then selects among the font's variants by priority.

*******************************************************************************/

static int fndxcap(int caps, int at)

{

    int ncaps; /* font capabilities */

    ncaps = 0; /* clear result */
    /* weight */
    if (at & BIT(sabold) && caps & BIT(xcbold)) ncaps |= BIT(xcbold);
    else if (at & BIT(salight) && caps & BIT(xclight)) ncaps |= BIT(xclight);
    /* slant */
    if (at & BIT(saital) && caps & BIT(xcital)) ncaps |= BIT(xcital);
    /* widths */
    if (at & BIT(sacondensed) && caps & BIT(xccondensed))
        ncaps |= BIT(xccondensed);
    else if (at & BIT(saextended) && caps & BIT(xcexpanded))
        ncaps |= BIT(xcexpanded);

    return (ncaps); /* return new capabilities */

}

/* find number of bits in set */
static int bitcnt(int i)

{

    int c, b;

    c = 0;
    b = 1;
    while (b >= 0) { /* this will overflow negative */

        if (i & b) c++;
        b <<= 1;

    }

    return (c); /* return bit count */

}

/* find if capabilities set matches one from list */
static int matchcap(int caps, xcaplst* cl, int* mc)

{

    int fnd;     /* found/not found flag */
    int bn, bn2; /* bit number */

    fnd = FALSE; /* set not found */
    bn = INT_MAX; /* set bit number maximum */
    while (cl) {

        /* all of the requested capability bits must also exist in the target
           capabilities */
        if ((cl->caps&caps) == caps) {

            /* find number of configuring bits in new set */
            bn2 = bitcnt(cl->caps & cfgcap);
            if (bn2 < bn) {

                /* fewer side effects */
                fnd = TRUE; /* set found */
                *mc = cl->caps; /* return what was matched */
                bn = bn2; /* set new minimum */

            }

        }
        cl = cl->next;

    }

    return (fnd); /* return match/no match */

}

static int fndxcapp(fontptr fp, int at)

{

    /* capabilities list in lowest to highest priority */
    const int cappri[] = {

        sablink,     /* blinking text (foreground) */
        saxlight,    /* extra light */
        saxbold,     /* bold */
        salight,     /* light */
        sarev,       /* reverse video */
        saundl,      /* underline */
        sasuper,     /* superscript */
        sasubs,      /* subscripting */
        sastkout,    /* strikeout text */
        sahollow,    /* hollow */
        saraised,    /* raised */
        sacondensed, /* condensed */
        saextended,  /* extended */
        saital,      /* italic text */
        sabold,      /* bold text */
        INT_MAX      /* end */

    };

    int caps;
    int match;
    int ia, lia;
    int mc;

    ia = 0;
    lia = 0;
    match = FALSE;
    mc = 0;
    do { /* search capabilities */

        /* find capabilities from this set of attributes */
        caps = fndxcap(fp->caps, at);
        if (matchcap(caps, fp->caplst, &mc)) match = TRUE; /* found a match */
        else { /* try again */

            at &= ~BIT(cappri[ia]); /* remove attribute by priority */
            lia = ia; /* save select */
            ia++; /* next attribute */

        }

    } while (!match &&
             (cappri[lia] != INT_MAX)); /* until found or no more attributes */
    /* if we still have not found anything, it has to be a system error, since
       we removed all attributes */
    if (!match) error(esystem);

    return (mc); /* return matching caps */

}

/*******************************************************************************

Glyph cache for FreeType rendered glyphs

Caches alpha masks of rendered glyphs to avoid recreating them on every
draw call.

*******************************************************************************/

#define GLYPH_CACHE_SIZE 512

typedef struct {

    FT_Face  face;         /* font face this glyph belongs to */
    int      pixel_size_y; /* em-square pixel size y */
    int      pixel_size_x; /* em-square pixel size x */
    int      glyph_index;  /* glyph index in font */
    uint8_t* mask;         /* glyph alpha mask, w*h bytes */
    int      width;        /* bitmap width */
    int      height;       /* bitmap height */
    int      bitmap_left;  /* left bearing */
    int      bitmap_top;   /* top bearing */
    int      advance;      /* horizontal advance */
    int      valid;        /* entry is valid */

} glyphcache;

static glyphcache gcache[GLYPH_CACHE_SIZE];

/* invalidate all cache entries that reference a given face */
static void ft_invalidate_face_un(FT_Face face)

{

    int i;

    for (i = 0; i < GLYPH_CACHE_SIZE; i++) {

        if (gcache[i].valid && gcache[i].face == face) {

            free(gcache[i].mask);
            gcache[i].valid = 0;
            gcache[i].mask = 0;

        }

    }

}

/* clear the whole glyph cache */
static void ft_cache_clear_un(void)

{

    int i;

    for (i = 0; i < GLYPH_CACHE_SIZE; i++) {

        if (gcache[i].valid) {

            free(gcache[i].mask);
            gcache[i].valid = 0;
            gcache[i].mask = 0;

        }

    }

}

/* find or create cached glyph */
static glyphcache* ft_cache_glyph(FT_Face face, int pixel_size_x,
                                  int pixel_size_y, char c)

{

    unsigned int gi;
    unsigned int hash;
    glyphcache* ge;
    FT_GlyphSlot slot;
    int w, h, pitch;
    unsigned char* buf;
    int j;

    gi = FT_Get_Char_Index(face, (unsigned char)c);
    hash = (gi * 31 + pixel_size_y * 17 + pixel_size_x * 13) % GLYPH_CACHE_SIZE;

    ge = &gcache[hash];

    /* check for cache hit */
    if (ge->valid && ge->face == face &&
        ge->pixel_size_y == pixel_size_y &&
        ge->pixel_size_x == pixel_size_x &&
        ge->glyph_index == (int)gi) return ge;

    /* cache miss - render the glyph as 8-bit grayscale */
    FT_Set_Pixel_Sizes(face, pixel_size_x, pixel_size_y);
    if (FT_Load_Char(face, (unsigned char)c, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
        return NULL;

    slot = face->glyph;
    w = slot->bitmap.width;
    h = slot->bitmap.rows;

    /* evict old entry if present */
    if (ge->valid) free(ge->mask);

    ge->face = face;
    ge->pixel_size_y = pixel_size_y;
    ge->pixel_size_x = pixel_size_x;
    ge->glyph_index = gi;
    ge->bitmap_left = slot->bitmap_left;
    ge->bitmap_top = slot->bitmap_top;
    ge->advance = (int)(slot->advance.x >> 6);
    ge->width = w;
    ge->height = h;
    ge->valid = 1;

    if (w == 0 || h == 0) {

        ge->mask = 0;
        return ge;

    }

    /* keep the alpha mask rows for the glyph blit */
    buf = slot->bitmap.buffer;
    pitch = slot->bitmap.pitch;
    ge->mask = malloc((size_t)w*h);
    for (j = 0; j < h; j++)
        memcpy(ge->mask+(size_t)j*w, buf+(size_t)j*pitch, w);

    return ge;

}

/* draw single character from the cached alpha mask, honoring the draw
   record's mix mode */
static void ft_draw_char_un(canvas* d, pd_draw* gc, FT_Face face,
                            int pixel_size_x, int pixel_size_y,
                            int x, int y, char c)

{

    glyphcache* ge;

    ge = ft_cache_glyph(face, pixel_size_x, pixel_size_y, c);
    if (!ge || !ge->mask) return;

    pd_glyph(d, gc, x + ge->bitmap_left, y - ge->bitmap_top,
             ge->mask, ge->width, ge->height, ge->width, 8);

}

/* draw string using cached glyphs */
static void ft_draw_string_un(canvas* d, pd_draw* gc, FT_Face face,
                              int pixel_size_x, int pixel_size_y,
                              int x, int y, char* s, int len)

{

    int i;

    for (i = 0; i < len; i++) {

        ft_draw_char_un(d, gc, face, pixel_size_x, pixel_size_y, x, y, s[i]);
        if (FT_Load_Char(face, (unsigned char)s[i], FT_LOAD_DEFAULT) == 0)
            x += (int)(face->glyph->advance.x >> 6);

    }

}

/* calculate text width */
static int ft_text_width_un(FT_Face face, const char* s, int len)

{

    int w;
    int i;

    w = 0;
    for (i = 0; i < len; i++) {

        if (FT_Load_Char(face, (unsigned char)s[i], FT_LOAD_DEFAULT) == 0)
            w += (int)(face->glyph->advance.x >> 6);

    }

    return w;

}

/* draw rotated character: the glyph is rasterized pre-rotated by FreeType
   itself via FT_Set_Transform */
static void ft_draw_char_rotated_un(canvas* d, pd_draw* gc, FT_Face face,
                                    int pixel_size_x, int pixel_size_y,
                                    float angle_rad,
                                    int x, int y, char c)

{

    FT_GlyphSlot slot;
    FT_Matrix    mat;
    int          w, h, pitch;
    unsigned char* buf;
    uint8_t*     mask;
    int          j;

    FT_Set_Pixel_Sizes(face, pixel_size_x, pixel_size_y);

    /* 16.16 fixed-point rotation matrix for FreeType. The input angle_rad
       is Petit-Ami's COMPASS convention (0 = north/up, pi/2 = east,
       positive turns clockwise in screen view). FreeType's matrix is
       math-CCW with its own y-up coordinate system, so the FT rotation
       angle is (pi/2 - angle_rad). */
    mat.xx = (FT_Fixed)( sin(angle_rad) * 0x10000);
    mat.xy = (FT_Fixed)(-cos(angle_rad) * 0x10000);
    mat.yx = (FT_Fixed)( cos(angle_rad) * 0x10000);
    mat.yy = (FT_Fixed)( sin(angle_rad) * 0x10000);
    FT_Set_Transform(face, &mat, NULL);
    if (FT_Load_Char(face, (unsigned char)c,
                     FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) {

        FT_Set_Transform(face, NULL, NULL);
        return;

    }
    slot = face->glyph;
    w = slot->bitmap.width;
    h = slot->bitmap.rows;
    if (w > 0 && h > 0) {

        buf = slot->bitmap.buffer;
        pitch = slot->bitmap.pitch;
        mask = malloc((size_t)w*h);
        if (mask) {

            for (j = 0; j < h; j++)
                memcpy(mask+(size_t)j*w, buf+(size_t)j*pitch, w);
            pd_glyph(d, gc, x + slot->bitmap_left, y - slot->bitmap_top,
                     mask, w, h, w, 8);
            free(mask);

        }

    }
    FT_Set_Transform(face, NULL, NULL); /* restore identity */

}

/* the locked forms: hold the font lock across the work */
static void ft_draw_char(canvas* d, pd_draw* gc, FT_Face face,
                         int pixel_size_x, int pixel_size_y,
                         int x, int y, char c)

{

    pthread_mutex_lock(&ftlock);
    ft_draw_char_un(d, gc, face, pixel_size_x, pixel_size_y, x, y, c);
    pthread_mutex_unlock(&ftlock);

}

static void ft_draw_char_rotated(canvas* d, pd_draw* gc, FT_Face face,
                                 int pixel_size_x, int pixel_size_y,
                                 float angle_rad, int x, int y, char c)

{

    pthread_mutex_lock(&ftlock);
    ft_draw_char_rotated_un(d, gc, face, pixel_size_x, pixel_size_y,
                            angle_rad, x, y, c);
    pthread_mutex_unlock(&ftlock);

}

static void ft_draw_string(canvas* d, pd_draw* gc, FT_Face face,
                           int pixel_size_x, int pixel_size_y,
                           int x, int y, char* s, int len)

{

    pthread_mutex_lock(&ftlock);
    ft_draw_string_un(d, gc, face, pixel_size_x, pixel_size_y, x, y, s, len);
    pthread_mutex_unlock(&ftlock);

}

static int ft_text_width(FT_Face face, const char* s, int len)

{

    int w;

    pthread_mutex_lock(&ftlock);
    w = ft_text_width_un(face, s, len);
    pthread_mutex_unlock(&ftlock);

    return (w);

}

static void ft_invalidate_face(FT_Face face)

{

    pthread_mutex_lock(&ftlock);
    ft_invalidate_face_un(face);
    pthread_mutex_unlock(&ftlock);

}

static void ft_cache_clear(void)

{

    pthread_mutex_lock(&ftlock);
    ft_cache_clear_un();
    pthread_mutex_unlock(&ftlock);

}

/*******************************************************************************

Select font

Sets the currently selected font active, processing the current
attributes as far as the font's variants carry them. The em-square pixel
size is searched so the resulting character cell (ascender + |descender|
+ 2) fits within the target cell height, keeping the cell constant
across font changes.

*******************************************************************************/

static void setfnt(winptr win)

{

    int      caps; /* matched capabilities set */
    xcaplst* cl;   /* capability list pointer */
    xcaplst* best; /* best matching entry */

    /* release any existing FreeType face */
    if (win->ftface) {

        ft_invalidate_face(win->ftface);
        FT_Done_Face(win->ftface);
        win->ftface = NULL;

    }

    /* find matching capabilities set */
    caps = fndxcapp(win->gcfont, win->gattr);

    /* find the xcaplst entry that matches the capabilities */
    best = NULL;
    cl = win->gcfont->caplst;
    while (cl) {

        if ((cl->caps & caps) == caps) { best = cl; break; }
        cl = cl->next;

    }
    /* if no exact match, take first entry with a valid path */
    if (!best) {

        cl = win->gcfont->caplst;
        while (cl) {

            if (cl->path) { best = cl; break; }
            cl = cl->next;

        }

    }
    if (!best || !best->path) error(esystem);

    /* load the font face */
    if (FT_New_Face(ftlibrary, best->path, best->index, &win->ftface))
        error(esystem);

    /* set the em-square pixel size so the resulting character cell fits
       within gfcellh; on first call (gfcellh == 0), bootstrap gfcellh
       from the initial gfhigh */
    {
        FT_Face face = win->ftface;
        int     target, pixsiz, asc, dsc;

        if (win->gfcellh == 0) {

            /* bootstrap: use gfhigh directly and record the resulting
               cell height as the persistent target */
            FT_Set_Pixel_Sizes(face, 0, win->gfhigh);
            asc = (int)( face->size->metrics.ascender  >> 6);
            dsc = (int)(-face->size->metrics.descender >> 6);
            win->gfcellh = asc + dsc + 2;
            pixsiz = win->gfhigh;

        } else {

            /* find the largest em-square pixel size such that the
               cell fits within gfcellh */
            target = win->gfcellh - 2; /* ascender + |descender| target */
            if (target < 1) target = 1;
            pixsiz = target;
            if (pixsiz < 1) pixsiz = 1;
            FT_Set_Pixel_Sizes(face, 0, pixsiz);
            asc = (int)( face->size->metrics.ascender  >> 6);
            dsc = (int)(-face->size->metrics.descender >> 6);
            while (asc + dsc > target && pixsiz > 1) {

                pixsiz--;
                FT_Set_Pixel_Sizes(face, 0, pixsiz);
                asc = (int)( face->size->metrics.ascender  >> 6);
                dsc = (int)(-face->size->metrics.descender >> 6);

            }
            while (1) {

                int np = pixsiz + 1;
                int na, nd;

                FT_Set_Pixel_Sizes(face, 0, np);
                na = (int)( face->size->metrics.ascender  >> 6);
                nd = (int)(-face->size->metrics.descender >> 6);
                if (na + nd > target) {

                    FT_Set_Pixel_Sizes(face, 0, pixsiz);
                    break;

                }
                pixsiz = np;
                asc = na;
                dsc = nd;

            }

        }
        /* gfhigh/gfhighx are the PHYSICAL em-square pixel sizes used by
           FreeType for glyph rendering, including the viewport scale.
           gfhigh_log is the LOGICAL (unscaled) em-square for metric
           queries. charspace and linespace stay LOGICAL for cursor
           advancement. */
        win->gfhigh_log = pixsiz;
        win->gfhigh    = (int)(pixsiz * win->vsy);
        win->gfhighx   = (int)(pixsiz * win->vsx);
        if (win->gfhigh  < 1) win->gfhigh  = 1;
        if (win->gfhighx < 1) win->gfhighx = 1;
        win->gfpoint   = pixsiz * 2835.0f / (float)win->sdpmy;
        win->linespace = win->gfcellh;
        win->baseoff   = asc + 1;
    }
    /* read charspace at LOGICAL pixel size for cursor advancement */
    FT_Set_Pixel_Sizes(win->ftface, 0, win->gfhigh_log);
    win->charspace = (int)(win->ftface->size->metrics.max_advance >> 6);
    win->chrspcx = 0;
    win->chrspcy = 0;

}

/*******************************************************************************

Find width of character

Finds and returns the width of a character. Normally used for
proportional fonts.

*******************************************************************************/

static int xwidth(winptr win, char c)

{

    int w;

    pthread_mutex_lock(&ftlock);
    /* ensure face is at logical pixel size for correct metric queries */
    FT_Set_Pixel_Sizes(win->ftface, 0, win->gfhigh_log);
    if (FT_Load_Char(win->ftface, (unsigned char)c, FT_LOAD_DEFAULT)) w = 0;
    else w = (int)(win->ftface->glyph->advance.x >> 6);
    pthread_mutex_unlock(&ftlock);

    return (w);

}

/*******************************************************************************

File table

Files can be passthrough to the OS, or can be associated with the screen
window pair.

*******************************************************************************/

static void getfil(filptr* fp)

{

    *fp = imalloc(sizeof(filrec)); /* get new file entry */
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = -1; /* set no input file linked */
    (*fp)->sfp = NULL; /* set no file pointer */

}

/* index window from logical file number, with checking */
static winptr lfn2win(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (!opnfil[fn]) error(einvhan); /* invalid handle */
    if (!opnfil[fn]->win) error(efnotwin); /* not a window file */

    return (opnfil[fn]->win); /* return windows pointer */

}

/* index window from file */
static winptr txt2win(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file number */
    if (fn < 0) error(einvfil); /* file invalid */

    return (lfn2win(fn)); /* get logical filenumber for file */

}

/* index window from logical window number */
static winptr lwn2win(ami_long wid)

{

    int    ofn; /* output file handle */

    if (wid < -MAXFIL || wid >= MAXFIL || !wid) error(einvhan); /* error */
    ofn = xltwin[wid+MAXFIL]; /* get the output file handle */

    return (lfn2win(ofn)); /* index window context */

}

/* get logical file number from file, verified */
static int txt2lfn(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file id */
    if (fn < 0) error(einvfil); /* invalid */

    return (fn); /* return result */

}

/* allocate system event tracking entry */
static void getsee(int sid)

{

    if (sid < 1 || sid > MAXSID) error(esystem);
    if (!sidtab[sid-1]) {

        sidtab[sid-1] = imalloc(sizeof(systrk));
        sidtab[sid-1]->win = NULL; /* set no window */
        sidtab[sid-1]->tim = 0; /* set no timer */
        sidtab[sid-1]->frm = FALSE; /* set not a frame timer */
        sidtab[sid-1]->joy = 0; /* set no joystick */

    }

}

/*******************************************************************************

Picture entries

*******************************************************************************/

static picptr getpic(void)

{

    picptr pp; /* pointer to picture entry */

    if (frepic) { /* there is a free entry */

        pp = frepic; /* index top free */
        frepic = pp->next; /* gap out */

    } else pp = imalloc(sizeof(pict)); /* allocate new one */
    pp->xi = NULL; /* set no image */
    pp->next = NULL; /* set no next */

    return (pp); /* return entry */

}

static void putpic(picptr pp)

{

    pp->next = frepic; /* push to free list */
    frepic = pp;

}

/* delete all the scaled copies of the picture by number */
static void delpic(winptr win, ami_long p)

{

    picptr pp; /* pointer to picture entries */

    while (win->pictbl[p-1]) {

        /* remove top entry */
        pp = win->pictbl[p-1];
        win->pictbl[p-1] = pp->next;
        freecanvas(pp->xi); /* release image */
        putpic(pp); /* release image entry */

    }

}

/*******************************************************************************

Cursor

The cursor is a reversing rectangle drawn in xor mode, so drawing it
twice removes it.

*******************************************************************************/

/* find if cursor is in screen bounds */
static int icurbnd(scnptr sc)

{

    return (sc->curx >= 1 && sc->curx <= sc->maxx &&
            sc->cury >= 1 && sc->cury <= sc->maxy);

}

/* the window is on display when the update screen is the display screen */
static int indisp(winptr win)

{

    return (win->curupd == win->curdsp);

}

/* draw reversing cursor; used both to place and remove the cursor */
static void curdrw(winptr win)

{

    scnptr sc; /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current update screen */
    sc->xcxt->fg = colnum(ami_white);
    sc->xcxt->mix = pd_mixxor; /* set reverse */
    pd_frect(scrcan, sc->xcxt,
             L2PX(win, sc->curxg-1), L2PY(win, sc->curyg-1),
             L2PW(win, win->charspace), L2PH(win, win->linespace));
    sc->xcxt->mix = pd_mixcopy; /* set overwrite */
    if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
    else sc->xcxt->fg = sc->fcrgb;

}

/* set cursor visible */
static void curon(winptr win)

{

    scnptr sc; /* pointer to current screen */

    /* The caret gates on the update screen: the caret tracks the
       screen being written, and a manager above holds the display on
       another screen while writing this one */
    sc = win->screens[win->curupd-1]; /* index current screen */
    if (!win->fcurdwn && sc->curv && icurbnd(sc)) {

        /* cursor not already down, cursor visible, cursor in bounds */
        curdrw(win);
        win->fcurdwn = TRUE; /* set cursor on screen */

    }

}

/* set cursor invisible */
static void curoff(winptr win)

{

    scnptr sc; /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current screen */
    if (win->fcurdwn && sc->curv && icurbnd(sc)) {

        curdrw(win); /* remove cursor */
        win->fcurdwn = FALSE; /* set cursor not on screen */

    }

}

/* set cursor status from position and visibility */
static void cursts(winptr win)

{

    if (win->screens[win->curupd-1]->curv &&
        icurbnd(win->screens[win->curupd-1])) {

        /* cursor should be visible */
        if (!win->fcurdwn) { /* not already down */

            curdrw(win); /* show cursor */
            win->fcurdwn = TRUE; /* set cursor on screen */

        }

    } else {

        /* cursor should not be visible */
        if (win->fcurdwn) { /* cursor visible */

            curdrw(win); /* remove cursor */
            win->fcurdwn = FALSE; /* set cursor not on screen */

        }

    }

}

/*******************************************************************************

Restore screen

Copies the current display buffer to the screen. The buffer tracks the
screen size here, so there are no margins to fill.

*******************************************************************************/

static void restore(winptr win) /* window to restore */

{

    scnptr sc;

    sc = win->screens[win->curdsp-1]; /* index screen */
    if (win->bufmod && win->visible) { /* buffered mode is on, and visible */

        curoff(win); /* hide the cursor for drawing */
        /* set colors and attributes */
        if (BIT(sarev) & sc->attr) { /* reverse */

            sc->xcxt->fg = sc->bcrgb;
            sc->xcxt->bg = sc->fcrgb;

        } else {

            sc->xcxt->bg = sc->bcrgb;
            sc->xcxt->fg = sc->fcrgb;

        }
        /* copy buffer to screen */
        pd_blit(scrcan, 0, 0, sc->xbuf, 0, 0, sc->maxxg, sc->maxyg);
        curon(win); /* show the cursor */

    }

}

/* make the screen visible; on the frame buffer it always is, so the
   first call paints the buffer to the screen */
static void winvis(winptr win)

{

    if (!win->visible) {

        win->visible = TRUE;
        restore(win);

    }

}

/*******************************************************************************

Screen buffer operations

*******************************************************************************/

/* clear screen buffer to background */
static void clrbuf(scnptr sc)

{

    sc->xcxt->fg = sc->bcrgb;
    pd_frect(sc->xbuf, sc->xcxt, 0, 0, sc->maxxg, sc->maxyg);
    sc->xcxt->fg = sc->fcrgb;

}

/* clear screen and home cursor */
static void iclear(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index current update screen */
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    if (win->bufmod) clrbuf(sc); /* clear screen buffer */
    if (indisp(win)) { /* also process to display */

        curoff(win); /* hide the cursor */
        sc->xcxt->fg = sc->bcrgb;
        pd_frect(scrcan, sc->xcxt, 0, 0, sc->maxxg, sc->maxyg);
        sc->xcxt->fg = sc->fcrgb;
        curon(win); /* show the cursor */

    }

}

/* scroll screen by deltas in any given direction */
static void iscrollg(winptr win, ami_long x, ami_long y)

{

    int dx, dy; /* destination coordinates */
    int sx, sy, sw, sh; /* source coordinates */
    struct { /* fill rectangle */

        int x, y; /* origin (left, top) */
        int w, h; /* width, height */

    } frx, fry; /* x fill, y fill */
    scnptr sc; /* pointer to current screen */

    /* transform scroll deltas to physical pixels for viewport scaling */
    x = L2PDX(win, x);
    y = L2PDY(win, y);

    sc = win->screens[win->curupd-1]; /* index current screen */
    /* scroll would result in complete clear, do it */
    if (x <= -sc->maxxg || x >= sc->maxxg ||
        y <= -sc->maxyg || y >= sc->maxyg)
        iclear(win); /* clear the screen buffer */
    else { /* scroll */

        /* set y movement */
        if (y >= 0) { /* move up */

            sy = y; /* from y lines down */
            sh = sc->maxyg-y; /* height minus lines to move */
            dy = 0; /* move to top of screen */
            fry.x = 0; /* set fill to y lines at bottom */
            fry.w = sc->maxxg-1;
            fry.y = sc->maxyg-y;
            fry.h = y;

        } else { /* move down */

            sy = 0; /* from top */
            sh = sc->maxyg-abs(y); /* height minus lines to move */
            dy = abs(y); /* move to y lines down */
            fry.x = 0; /* set fill to y lines at top */
            fry.w = sc->maxxg-1;
            fry.y = 0;
            fry.h = abs(y);

        }
        /* set x movement */
        if (x >= 0) { /* move left */

            sx = x; /* from x columns to the right */
            sw = sc->maxxg-x; /* width - x columns */
            dx = 0; /* move to left side */
            /* set fill x columns at right */
            frx.x = sc->maxxg-x;
            frx.w = x;
            frx.y = 0;
            frx.h = sc->maxyg-1;

        } else { /* move right */

            sx = 0; /* from x left */
            sw = sc->maxxg-abs(x); /* width - x columns */
            dx = abs(x); /* move from left side */
            /* set fill x columns at left */
            frx.x = 0;
            frx.w = abs(x);
            frx.y = 0;
            frx.h = sc->maxyg-1;

        }
        if (win->bufmod) { /* apply to buffer */

            pd_scroll(sc->xbuf, dx, dy, sx, sy, sw, sh);
            sc->xcxt->fg = sc->bcrgb;
            /* fill vacated x */
            if (x) pd_frect(sc->xbuf, sc->xcxt, frx.x, frx.y, frx.w, frx.h);
            /* fill vacated y */
            if (y) pd_frect(sc->xbuf, sc->xcxt, fry.x, fry.y, fry.w, fry.h);
            sc->xcxt->fg = sc->fcrgb;

        } else { /* scroll on screen */

            curoff(win); /* hide the cursor for drawing */
            pd_scroll(scrcan, dx, dy, sx, sy, sw, sh);
            sc->xcxt->fg = sc->bcrgb;
            /* fill vacated x */
            if (x) pd_frect(scrcan, sc->xcxt, frx.x, frx.y, frx.w, frx.h);
            /* fill vacated y */
            if (y) pd_frect(scrcan, sc->xcxt, fry.x, fry.y, fry.w, fry.h);
            sc->xcxt->fg = sc->fcrgb;
            curon(win); /* show the cursor */

        }

    }
    if (indisp(win) && win->bufmod)
        restore(win); /* move buffer to screen */

}

/* position cursor */
static void icursor(winptr win, ami_long x, ami_long y)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index update screen */
    curoff(win); /* hide the cursor */
    sc->cury = y; /* set new position */
    sc->curx = x;
    sc->curxg = (x-1)*win->charspace+1;
    sc->curyg = (y-1)*win->linespace+1;
    curon(win); /* show the cursor */

}

/* position cursor graphical */
static void icursorg(winptr win, ami_long x, ami_long y)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    sc->curyg = y; /* set new position */
    sc->curxg = x;
    sc->curx = x/win->charspace+1;
    sc->cury = y/win->linespace+1;
    curon(win); /* show the cursor */

}

/* home cursor */
static void ihome(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    /* reset cursors */
    sc->curx = 1;
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    curon(win); /* show the cursor */

}

/* move cursor up internal */
static void iup(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    /* check not top of screen */
    if (sc->cury > 1) {

        curoff(win); /* hide the cursor */
        sc->cury--; /* update position */
        sc->curyg -= win->linespace; /* go last character line */
        curon(win); /* show the cursor */

    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, -1*win->linespace); /* scroll up */
    /* check won't overflow */
    else if (sc->cury > -LONG_MAX) {

        curoff(win); /* hide the cursor */
        sc->cury--; /* set new position */
        sc->curyg -= win->linespace;
        curon(win); /* show the cursor */

    }

}

/* move cursor down internal */
static void idown(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    /* check not bottom of screen */
    if (sc->cury < sc->maxy) {

        curoff(win); /* hide the cursor */
        sc->cury++; /* update position */
        sc->curyg += win->linespace+win->chrspcy; /* move to next line */
        curon(win); /* show the cursor */

    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, +1*win->linespace); /* scroll down */
    else if (sc->cury < LONG_MAX) {

        curoff(win); /* hide the cursor */
        sc->cury++; /* set new position */
        sc->curyg += win->linespace+win->chrspcy; /* move to next line */
        curon(win); /* show the cursor */

    }

}

/* move cursor left internal */
static void ileft(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    /* check not at extreme left */
    if (sc->curx > 1) {

        curoff(win); /* hide the cursor */
        sc->curx--; /* update position */
        sc->curxg -= win->charspace; /* back one character */
        curon(win); /* show the cursor */

    } else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            iup(win); /* move cursor up one line */
            curoff(win); /* hide the cursor */
            sc->curx = sc->maxx; /* set cursor to extreme right */
            /* the last column's cell: one past the width less a cell is
               off the grid when the width is a whole number of cells */
            sc->curxg = (sc->maxx-1)*win->charspace+1;
            curon(win); /* show the cursor */

        } else {

            /* check won't overflow */
            if (sc->curx > -LONG_MAX) {

                curoff(win); /* hide the cursor */
                sc->curx--; /* update position */
                sc->curxg -= win->charspace;
                curon(win); /* show the cursor */

            }

        }

    }

}

/* move cursor right internal */
static void iright(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    /* check not at extreme right */
    if (sc->curx < sc->maxx) {

        curoff(win); /* hide the cursor */
        sc->curx++; /* update position */
        sc->curxg += win->charspace;
        curon(win); /* show the cursor */

    } else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            idown(win); /* move cursor down one line */
            curoff(win); /* hide the cursor */
            sc->curx = 1; /* set cursor to extreme left */
            sc->curxg = 1;
            curon(win); /* show the cursor */

        } else {

            /* check won't overflow */
            if (sc->curx < LONG_MAX) {

                curoff(win); /* hide the cursor */
                sc->curx++; /* update position */
                sc->curxg += win->charspace;
                curon(win); /* show the cursor */

            }

        }

    }

}

/* process tab */
static void itab(winptr win)

{

    int i;
    ami_long x;
    scnptr sc;

    sc = win->screens[win->curupd-1];
    curoff(win); /* hide the cursor */
    /* first, find if next tab even exists */
    x = sc->curxg+1; /* get just after the current x position */
    if (x < 1) x = 1; /* don't bother to search to left of screen */
    /* find tab or end of screen */
    i = 0; /* set 1st tab position */
    while (x > sc->tab[i] && sc->tab[i] && i < MAXTAB && x < sc->maxxg) i++;
    if (sc->tab[i] && x < sc->tab[i]) { /* not off right of tabs */

       sc->curxg = sc->tab[i]; /* set position to that tab */
       sc->curx = sc->curxg/win->charspace+1;

    }
    curon(win); /* show the cursor */

}

/* set tab graphical */
static void isettabg(winptr win, ami_long t)

{

    int i, x; /* tab index */
    scncon* sc; /* screen context */

    sc = win->screens[win->curupd-1];
    if (sc->autof && (t-1)%win->charspace)
        error(eatotab); /* cannot perform with auto on */
    if (t < 1 || t > sc->maxxg) error(einvtab); /* bad tab position */
    /* find free location or tab beyond position */
    i = 0;
    while (i < MAXTAB && sc->tab[i] && t > sc->tab[i]) i = i+1;
    if (i == MAXTAB && t < sc->tab[i]) error(etabful); /* tab table full */
    if (t != sc->tab[i]) { /* not the same tab yet again */

        if (sc->tab[MAXTAB-1]) error(etabful); /* tab table full */
        /* move tabs above us up */
        for (x = MAXTAB-1; x > i; x--) sc->tab[x] = sc->tab[x-1];
        sc->tab[i] = t; /* place tab in order */

    }

}

/* reset tab graphical */
static void irestabg(winptr win, ami_long t)

{

    int     i;  /* tab index */
    int     ft; /* found tab */
    scncon* sc; /* screen context */

    sc = win->screens[win->curupd-1];
    if (t < 1 || t > sc->maxxg) error(einvtab); /* bad tab position */
    /* search for that tab */
    ft = 0; /* set not found */
    for (i = 0; i < MAXTAB; i++) if (sc->tab[i] == t) ft = i; /* found */
    if (ft != 0) { /* found the tab, remove it */

       /* move all tabs down to gap out */
       for (i = ft; i < MAXTAB-1; i++) sc->tab[i] = sc->tab[i+1];
       sc->tab[MAXTAB-1] = 0; /* clear any last tab */

    }

}

/* enable/disable automatic scroll and wrap */
static void iauto(winptr win, ami_long e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    /* check we are transitioning to auto mode */
    if (e) {

        /* check display is on grid and in bounds */
        if ((sc->curxg-1) % win->charspace) error(eatoofg);
        if ((sc->curyg-1) % win->linespace) error(eatoofg);
        if (sc->angle != LONG_MAX/4) error(eatoang);
        if (!icurbnd(sc)) error(eatoecb);

    }
    sc->autof = e; /* set auto status */
    win->gauto = e;

}

/*******************************************************************************

Character drawing

The 90 degree case is the normal text path; arbitrary angles rotate the
glyph and its cell.

*******************************************************************************/

/* place character with foreground and background, 90 degrees */
static void drwchr90(winptr win, scnptr sc, int cs, int ce, canvas* d, char c)

{

    /* transform position and sizes to physical pixels */
    int px  = L2PX(win, sc->curxg-1);
    int py  = L2PY(win, sc->curyg-1);
    int pcs = L2PW(win, cs);
    int pls = L2PH(win, win->linespace);
    int pbo = L2PH(win, win->baseoff);
    int pmx = L2PW(win, win->mischrx);
    int pmy = L2PH(win, win->mischry);
    int pmox = L2PW(win, win->misoffx);
    int pmoy = L2PH(win, win->misoffy);

    if (sc->bmod != mdinvis) { /* background is visible */

        /* set background function */
        sc->xcxt->mix = mod2fnc[sc->bmod];
        /* set background to foreground to draw character background */
        if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->fcrgb;
        else sc->xcxt->fg = sc->bcrgb;
        pd_frect(d, sc->xcxt, px, py, pcs, pls);
        /* xor is non-destructive, and we can restore it */
        if (sc->bmod == mdxor) {

            if (ce) /* character exists */
                ft_draw_char(d, sc->xcxt, win->ftface, win->gfhighx,
                             win->gfhigh, px, py+pbo, c);
            else /* does not exist, draw missing character box */
                pd_rect(d, sc->xcxt, px+pmox, py+pmoy, pmx, pmy);

        }
        /* restore colors */
        if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
        else sc->xcxt->fg = sc->fcrgb;
        /* reset background function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }
    if (sc->fmod != mdinvis) {

        /* set foreground function */
        sc->xcxt->mix = mod2fnc[sc->fmod];
        if (ce) /* character exists */
            ft_draw_char(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                         px, py+pbo, c);
        else /* does not exist, draw missing character box */
            pd_rect(d, sc->xcxt, px+pmox, py+pmoy, pmx, pmy);
        /* check draw underline */
        if (sc->attr & BIT(saundl)) {

            /* double line, may need adjusting for low DPI displays */
            pd_line(d, sc->xcxt, px, py+pbo+1, px+pcs, py+pbo+1);
            pd_line(d, sc->xcxt, px, py+pbo+2, px+pcs, py+pbo+2);

        }
        /* check draw strikeout */
        if (sc->attr & BIT(sastkout)) {

            pd_line(d, sc->xcxt,
                    px, py+(int)(pbo/STRIKE), px+pcs, py+(int)(pbo/STRIKE));
            pd_line(d, sc->xcxt,
                    px, py+(int)(pbo/STRIKE)+1, px+pcs, py+(int)(pbo/STRIKE)+1);

        }
        /* reset foreground function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }

}

/* add vector to position: angle and length to an x-y position. The angle
   is PA 12 o'clock LONG_MAX ratioed angles clockwise. */
static void addvect(ami_long* x, ami_long* y, float a, ami_long l)

{

    *x += round(sin(a)*l);
    *y -= round(cos(a)*l);

}

/* convert PA LONG_MAX ratio angle to RADIAN measure */
#define RADIAN(a) ((double)2*M_PI/LONG_MAX*a)

/* draw rotated rectangle */
static void drwrecta(canvas* d, scnptr sc, ami_long a, int x, int y, int w, int h)

{

    ami_long x1, x2, x3, x4;
    ami_long y1, y2, y3, y4;
    ami_long c;
    double ac;

    x1 = x2 = x3 = x4 = x;
    y1 = y2 = y3 = y4 = y;
    addvect(&x2, &y2, RADIAN(a), w);
    addvect(&x4, &y4, RADIAN(a)+2*M_PI/4, h);
    c = sqrt((double)w*w+(double)h*h);
    ac = atan((double)h/w);
    addvect(&x3, &y3, ac+RADIAN(a), c);
    pd_line(d, sc->xcxt, x1, y1, x2, y2);
    pd_line(d, sc->xcxt, x2, y2, x3, y3);
    pd_line(d, sc->xcxt, x3, y3, x4, y4);
    pd_line(d, sc->xcxt, x4, y4, x1, y1);

}

/* draw filled rotated rectangle */
static void drwfrecta(canvas* d, scnptr sc, ami_long a, int x, int y, int w, int h)

{

    ami_long x1, x2, x3, x4;
    ami_long y1, y2, y3, y4;
    ami_long c;
    double ac;
    int xp[8];

    x1 = x2 = x3 = x4 = x;
    y1 = y2 = y3 = y4 = y;
    addvect(&x2, &y2, RADIAN(a), w);
    addvect(&x4, &y4, RADIAN(a)+2*M_PI/4, h);
    c = sqrt((double)w*w+(double)h*h);
    ac = atan((double)h/w);
    addvect(&x3, &y3, ac+RADIAN(a), c);
    xp[0] = x1;
    xp[1] = y1;
    xp[2] = x2;
    xp[3] = y2;
    xp[4] = x3;
    xp[5] = y3;
    xp[6] = x4;
    xp[7] = y4;
    pd_fpoly(d, sc->xcxt, xp, 4);

}

/* place character with foreground and background at any angle */
static void drwchr(winptr win, scnptr sc, int cs, int ce, canvas* d, char c)

{

    ami_long xb, yb;     /* rotated baseline */
    ami_long xull, yull, xulr, yulr; /* underline */
    ami_long xsol, ysol, xsor, ysor; /* strikeout */

    /* transform starting position and sizes for viewport scaling */
    int px  = L2PX(win, sc->curxg-1);
    int py  = L2PY(win, sc->curyg-1);
    int pcs = L2PW(win, cs);
    int pls = L2PH(win, win->linespace);
    int pbo = L2PH(win, win->baseoff);
    int pmx = L2PW(win, win->mischrx);
    int pmy = L2PH(win, win->mischry);
    int pmox = L2PW(win, win->misoffx);
    int pmoy = L2PH(win, win->misoffy);

    /* find rotated character baseline */
    xb = px;
    yb = py;
    addvect(&xb, &yb, RADIAN(sc->angle)+2*M_PI/4, pbo);

    /* find rotated underline left side */
    xull = px;
    yull = py;
    addvect(&xull, &yull, RADIAN(sc->angle)+2*M_PI/4, pbo+1);
    /* find right side */
    xulr = xull;
    yulr = yull;
    addvect(&xulr, &yulr, RADIAN(sc->angle), pcs);

    /* find rotated strikeout left side */
    xsol = px;
    ysol = py;
    addvect(&xsol, &ysol, RADIAN(sc->angle)+2*M_PI/4, (int)(pbo/STRIKE));
    /* find right side */
    xsor = xsol;
    ysor = ysol;
    addvect(&xsor, &ysor, RADIAN(sc->angle), pcs);

    if (sc->bmod != mdinvis) { /* background is visible */

        /* set background function */
        sc->xcxt->mix = mod2fnc[sc->bmod];
        /* set background to foreground to draw character background */
        if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->fcrgb;
        else sc->xcxt->fg = sc->bcrgb;
        drwfrecta(d, sc, sc->angle, px, py, pcs, pls);
        /* xor is non-destructive, and we can restore it */
        if (sc->bmod == mdxor) {

            if (ce) /* character exists */
                ft_draw_char_rotated(d, sc->xcxt, win->ftface, win->gfhighx,
                                     win->gfhigh, RADIAN(sc->angle), xb, yb, c);
            else /* does not exist, draw missing character box */
                drwrecta(d, sc, sc->angle, px+pmox, py+pmoy, pmx, pmy);

        }
        /* restore colors */
        if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
        else sc->xcxt->fg = sc->fcrgb;
        /* reset background function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }
    if (sc->fmod != mdinvis) {

        /* set foreground function */
        sc->xcxt->mix = mod2fnc[sc->fmod];
        if (ce) /* character exists */
            ft_draw_char_rotated(d, sc->xcxt, win->ftface, win->gfhighx,
                                 win->gfhigh, RADIAN(sc->angle), xb, yb, c);
        else /* does not exist, draw missing character box */
            drwrecta(d, sc, sc->angle, px+pmox, py+pmoy, pmx, pmy);
        /* check draw underline */
        if (sc->attr & BIT(saundl)) {

            sc->xcxt->lw = 2; sc->xcxt->lstyle = pd_linesolid;
            pd_line(d, sc->xcxt, xull, yull, xulr, yulr);
            sc->xcxt->lw = sc->lwidth; sc->xcxt->lstyle = pd_linesolid;

        }
        /* check draw strikeout */
        if (sc->attr & BIT(sastkout)) {

            sc->xcxt->lw = 2; sc->xcxt->lstyle = pd_linesolid;
            pd_line(d, sc->xcxt, xsol, ysol, xsor, ysor);
            sc->xcxt->lw = sc->lwidth; sc->xcxt->lstyle = pd_linesolid;

        }
        /* reset foreground function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }

}

/*******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the
current colors and attribute. Elementary control codes are handled here:
newline, backspace, form feed and tab.

*******************************************************************************/

static void plcchr(winptr win, char c)

{

    scnptr sc; /* pointer to current screen */
    int    cs; /* character spacing */
    int    ce; /* character exists */

    sc = win->screens[win->curupd-1]; /* index current screen */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* handle special character cases first */
    if (c == '\r') {

        /* carriage return, position to extreme left */
        curoff(win); /* hide the cursor */
        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;
        curon(win); /* show the cursor */

    } else if (c == '\n') {

        curoff(win); /* hide the cursor */
        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;
        curon(win); /* show the cursor */
        idown(win); /* line feed, move down */

    } else if (c == '\b') ileft(win); /* back space, move left */
    else if (c == '\f') iclear(win); /* clear screen */
    else if (c == '\t') itab(win); /* process tab */
    /* only output visible characters */
    else if (c >= ' ' && c != 0x7f) {

        /* find character spacing */
        if (sc->cfont->fix) cs = win->charspace;
        else cs = xwidth(win, c)+win->chrspcx;
        ce = TRUE; /* set character exists */
        if (!cs) { /* character does not exist */

            /* set spacing of cell */
            cs = win->misoffx+win->mischrx+win->misoffx+1;
            ce = FALSE; /* set character does not exist */

        }
        if (win->bufmod) { /* buffer is active */

            /* draw character to buffer */
            if (sc->angle == LONG_MAX/4) drwchr90(win, sc, cs, ce, sc->xbuf, c);
            else drwchr(win, sc, cs, ce, sc->xbuf, c);

        }
        if (indisp(win)) { /* do it again for the current screen */

            curoff(win); /* hide the cursor */
            /* draw character to active screen */
            if (sc->angle == LONG_MAX/4) drwchr90(win, sc, cs, ce, scrcan, c);
            else drwchr(win, sc, cs, ce, scrcan, c);
            curon(win); /* show the cursor */

        }
        /* advance to next character */
        if (sc->angle == LONG_MAX/4) {

            if (sc->cfont->fix) iright(win); /* move cursor right character */
            else { /* perform proportional version */

                if (indisp(win)) curoff(win); /* remove cursor */
                sc->curxg += cs; /* advance the character width */
                /* the cursor x position really has no meaning with
                   proportional but we recalculate it using space anyway */
                sc->curx = sc->curxg/win->charspace+1;
                if (indisp(win)) curon(win); /* set cursor on screen */

            }

        } else { /* arbitrary angle */

            /* if fixed font use fixed width of character */
            if (sc->cfont->fix) cs = win->charspace;
            if (indisp(win)) curoff(win); /* remove cursor */
            /* find next position */
            addvect(&sc->curxg, &sc->curyg, RADIAN(sc->angle), cs);
            sc->curx = sc->curxg/win->charspace+1;
            sc->cury = sc->curyg/win->linespace+1;
            if (indisp(win)) curon(win); /* set cursor on screen */

        }

    }

}

/* place string with foreground and background, 90 degrees */
static void drwstr90(winptr win, scnptr sc, int tw, canvas* d, char* s, int l)

{

    /* transform position and sizes to physical pixels */
    int px  = L2PX(win, sc->curxg-1);
    int py  = L2PY(win, sc->curyg-1);
    int ptw = L2PW(win, tw);
    int pls = L2PH(win, win->linespace);
    int pbo = L2PH(win, win->baseoff);

    if (sc->bmod != mdinvis) { /* background is visible */

        /* set background function */
        sc->xcxt->mix = mod2fnc[sc->bmod];
        /* set background to foreground to draw character background */
        if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->fcrgb;
        else sc->xcxt->fg = sc->bcrgb;
        pd_frect(d, sc->xcxt, px, py, ptw, pls);
        /* xor is non-destructive, and we can restore it */
        if (sc->bmod == mdxor)
            ft_draw_string(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                           px, py+pbo, s, l);
        /* restore colors */
        if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
        else sc->xcxt->fg = sc->fcrgb;
        /* reset background function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }
    if (sc->fmod != mdinvis) {

        /* set foreground function */
        sc->xcxt->mix = mod2fnc[sc->fmod];
        /* draw string */
        ft_draw_string(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                       px, py+pbo, s, l);
        /* check draw underline */
        if (sc->attr & BIT(saundl)) {

            pd_line(d, sc->xcxt, px, py+pbo+1, px+ptw, py+pbo+1);
            pd_line(d, sc->xcxt, px, py+pbo+2, px+ptw, py+pbo+2);

        }
        /* check draw strikeout */
        if (sc->attr & BIT(sastkout)) {

            pd_line(d, sc->xcxt,
                    px, py+(int)(pbo/STRIKE), px+ptw, py+(int)(pbo/STRIKE));
            pd_line(d, sc->xcxt,
                    px, py+(int)(pbo/STRIKE)+1, px+ptw, py+(int)(pbo/STRIKE)+1);

        }
        /* reset foreground function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }

}

/*******************************************************************************

Initialize screen

Clears all the parameters in the present screen context.

*******************************************************************************/

static void iniscn(winptr win, scnptr sc)

{

    int i, x;

    sc->maxx = win->gmaxx; /* set character dimensions */
    sc->maxy = win->gmaxy;
    sc->maxxg = win->gmaxxg; /* set pixel dimensions */
    sc->maxyg = win->gmaxyg;
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    sc->angle = LONG_MAX/4; /* set character draw at 90 degrees */
    sc->fcrgb = win->gfcrgb; /* set colors and attributes */
    sc->bcrgb = win->gbcrgb;
    sc->attr = win->gattr;
    sc->autof = win->gauto; /* set auto scroll and wrap */
    sc->curv = win->gcurv; /* set cursor visibility */
    sc->lwidth = 1; /* set single pixel width */
    sc->lstyle = ami_lssolid; /* set default line style */
    sc->cfont = win->gcfont; /* set current font */
    sc->fmod = win->gfmod; /* set mix modes */
    sc->bmod = win->gbmod;
    sc->offx = win->goffx; /* set viewport offset */
    sc->offy = win->goffy;
    sc->wextx = win->gwextx; /* set extents */
    sc->wexty = win->gwexty;
    sc->vextx = win->gvextx;
    sc->vexty = win->gvexty;
    for (i = 0; i < MAXTAB; i++) sc->tab[i] = 0; /* clear tab array */
    /* set up tabbing to be on each 8th position */
    i = 9; /* set 1st tab position */
    x = 0; /* set 1st tab slot */
    while (i < sc->maxx && x < MAXTAB) {

        sc->tab[x] = (i-1)*win->charspace+1; /* set tab */
        i = i+8; /* next tab */
        x = x+1;

    }

    /* create graphics context for screen */
    sc->xcxt = gcnew();

    /* set colors and attributes */
    if (BIT(sarev) & sc->attr) { /* reverse */

        sc->xcxt->bg = sc->fcrgb;
        sc->xcxt->fg = sc->bcrgb;

    } else {

        sc->xcxt->bg = sc->bcrgb;
        sc->xcxt->fg = sc->fcrgb;

    }

    /* set line attributes */
    sc->xcxt->lw = 1; sc->xcxt->lstyle = pd_linesolid;

    /* set up backing buffer */
    sc->xbuf = newcanvas(sc->maxxg, sc->maxyg);
    if (!sc->xbuf) error(enomem);

    /* save buffer size */
    win->bufx = win->gmaxx;
    win->bufy = win->gmaxy;
    win->bufxg = win->gmaxxg;
    win->bufyg = win->gmaxyg;

    /* clear it */
    clrbuf(sc);

}

/*******************************************************************************

Open and present window

The frame buffer carries one window: the whole screen. All of the screen
buffer data is cleared, and a single buffer assigned to it.

*******************************************************************************/

static void opnwin(int fn, int pfn, ami_long wid)

{

    int    ti;  /* index for timer array */
    int    pin; /* index for loadable pictures array */
    int    si;  /* index for current display screen */
    winptr win; /* window pointer */

    win = lfn2win(fn); /* get a pointer to the window */
    win->parlfn = pfn; /* set parent logical number */
    win->wid = wid; /* set window id */
    win->fcurdwn = FALSE; /* set cursor is not down */
    win->focus = TRUE; /* the screen is always in focus */
    win->mb1 = FALSE; /* set mouse as assumed no buttons down, at origin */
    win->mb2 = FALSE;
    win->mb3 = FALSE;
    win->mpx = 1;
    win->mpy = 1;
    win->mpxg = 1;
    win->mpyg = 1;
    win->joy1xs = 0; /* clear joystick saves */
    win->joy1ys = 0;
    win->joy1zs = 0;
    win->joy2xs = 0;
    win->joy2ys = 0;
    win->joy2zs = 0;
    win->inpptr = -1; /* set buffer empty */
    win->inpbuf[0] = 0;
    win->frmrun = FALSE; /* set framing timer not running */
    win->bufmod = TRUE; /* set buffering on */
    /* clear timer array */
    for (ti = 0; ti < AMI_MAXTIM; ti++) win->timers[ti] = 0;
    win->frmsev = 0; /* clear frame timer */
    /* clear loadable pictures table */
    for (pin = 0; pin < MAXPIC; pin++) win->pictbl[pin] = NULL;
    /* clear the screen array */
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    win->screens[0] = imalloc(sizeof(scncon)); /* get the default screen */
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set not yet painted */

    /* set up global buffer parameters: the screen is the display */
    win->gattr = 0; /* no attribute */
    win->gauto = TRUE; /* auto on */
    win->gfcrgb = colnum(ami_black); /* foreground black */
    win->gbcrgb = colnum(ami_white); /* background white */
    win->gcurv = TRUE; /* cursor visible */
    win->gfmod = mdnorm; /* set mix modes */
    win->gbmod = mdnorm;
    win->goffx = 0; /* set 0 offset */
    win->goffy = 0;
    win->vsx = 1.0f; /* viewport scale starts at 1:1 */
    win->vsy = 1.0f;
    win->gwextx = 1; /* set 1:1 extents */
    win->gwexty = 1;
    win->gvextx = 1;
    win->gvexty = 1;

    /* screen parameters: the panel's physical size is not carried by the
       frame buffer device, so the desktop default density stands in */
    win->shres = fbcols;
    win->svres = fbrows;
    win->sdpmx = FBDPM; /* dots per meter */
    win->sdpmy = FBDPM;

    win->gcfont = fntlst; /* index terminal font entry */
    win->gfhigh = (int)(CONPNT*POINT*win->sdpmy/1000); /* set font height */
    win->gfcellh = 0; /* 0 means: bootstrap from gfhigh on first setfnt */
    win->gfpoint = (float)CONPNT*POINT/1000.0f; /* set initial point size */
    /* set parameters of missing font character */
    win->mischrx = win->gfhigh*MISCHRX; /* set size x */
    win->mischry = win->gfhigh*MISCHRY; /* set size y */
    win->misoffx = win->gfhigh*MISOFFX; /* set offset x */
    win->misoffy = win->gfhigh*MISOFFY; /* set offset y */
    win->ftface = NULL; /* clear current font face */
    setfnt(win); /* select font */

    /* set standard/reference font sizes */
    stdchrx = win->charspace;
    stdchry = win->linespace;

    /* the display is the whole frame buffer; the character grid follows
       from the font cell */
    win->gmaxxg = fbcols;
    win->gmaxyg = fbrows;
    win->gmaxx = win->gmaxxg/win->charspace;
    win->gmaxy = win->gmaxyg/win->linespace;

    iniscn(win, win->screens[0]); /* initialize screen buffer */
    restore(win); /* place on display */

}

/*******************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   ami_long wid)

{

    /* if output was never opened, create it now */
    if (!opnfil[ofn]) getfil(&opnfil[ofn]);
    /* if input was never opened, create it now */
    if (!opnfil[ifn]) getfil(&opnfil[ifn]);
    opnfil[ofn]->inl = ifn; /* link output to input */
    opnfil[ifn]->inw = TRUE; /* set input is window handler */
    /* set file descriptor locations */
    opnfil[ifn]->sfp = infile;
    opnfil[ofn]->sfp = outfile;
    /* now see if it has a window attached */
    if (!opnfil[ofn]->win) {

        /* haven't already started the main input/output window, so
           allocate and start that */
        opnfil[ofn]->win = thewin;
        opnwin(ofn, pfn, wid); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid+MAXFIL] >= 0 && xltwin[wid+MAXFIL] != ofn)
        error(ewinuse); /* flag error */
    xltwin[wid+MAXFIL] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

}

/*******************************************************************************

Event queuing

Holds PA events posted by sendevent until event() drains them.

*******************************************************************************/

static pthread_mutex_t paevtlock = PTHREAD_MUTEX_INITIALIZER;

static paevtque* getpaevt(void)

{

    paevtque* p;

    if (paqfre) { /* there is a freed entry */

        p = paqfre; /* index top entry */
        paqfre = p->next; /* gap from list */

    } else p = imalloc(sizeof(paevtque));

    return (p);

}

static void putpaevt(paevtque* p)

{

    p->next = paqfre; /* push to list */
    paqfre = p;

}

/* place PA event into input queue */
static void enquepaevt(ami_evtrec* e)

{

    paevtque* p;

    pthread_mutex_lock(&paevtlock);
    p = getpaevt(); /* get a queue entry */
    memcpy(&p->evt, e, sizeof(ami_evtrec)); /* copy event to queue entry */
    if (paqevt) { /* there are entries in queue */

        /* we push to the last entry, which is the entry point */
        p->next = paqevt; /* link next to queue top */
        p->last = paqevt->last; /* link last to queue bottom */
        paqevt->last->next = p;
        paqevt->last = p;

    } else { /* queue is empty */

        p->next = p; /* link to self */
        p->last = p;
        paqevt = p; /* place as top */

    }
    pthread_mutex_unlock(&paevtlock);

}

/* remove PA event from input queue; returns TRUE with an event */
static int dequepaevt(ami_evtrec* e)

{

    paevtque* p;
    int       r;

    r = FALSE;
    pthread_mutex_lock(&paevtlock);
    if (paqevt) { /* there are entries */

        p = paqevt; /* index top of queue */
        if (p->next == p) paqevt = NULL; /* only entry, clear queue */
        else { /* other entries */

            paqevt->last->next = p->next; /* gap out */
            p->next->last = paqevt->last;
            paqevt = p->next; /* set new top */

        }
        memcpy(e, &p->evt, sizeof(ami_evtrec)); /* copy out */
        putpaevt(p); /* release entry */
        r = TRUE;

    }
    pthread_mutex_unlock(&paevtlock);

    return (r);

}

/*******************************************************************************

Keyboard translation

The console keyboard arrives on stdin as bytes in raw mode. Plain
characters are etchar; return is etenter; the ANSI escape sequences the
console keys generate are translated to the cursor, editing and function
key events.

*******************************************************************************/

/* keyboard decode state */
static char kbdseq[16]; /* pending escape sequence */
static int  kbdseql;    /* characters held */

/* translate a complete escape sequence to an event; returns TRUE if the
   sequence made an event */
static int kbdxlat(ami_evtrec* er)

{

    int r;

    r = TRUE;
    er->winid = 1;
    /* kbdseq holds the characters after the ESC */
    if (!strcmp(kbdseq, "[A")) er->etype = ami_etup;
    else if (!strcmp(kbdseq, "[B")) er->etype = ami_etdown;
    else if (!strcmp(kbdseq, "[C")) er->etype = ami_etright;
    else if (!strcmp(kbdseq, "[D")) er->etype = ami_etleft;
    else if (!strcmp(kbdseq, "[H")) er->etype = ami_ethomel;
    else if (!strcmp(kbdseq, "[F")) er->etype = ami_etendl;
    else if (!strcmp(kbdseq, "[1~")) er->etype = ami_ethomel;
    else if (!strcmp(kbdseq, "[4~")) er->etype = ami_etendl;
    else if (!strcmp(kbdseq, "[2~")) er->etype = ami_etinsertt;
    else if (!strcmp(kbdseq, "[3~")) er->etype = ami_etdelcf;
    else if (!strcmp(kbdseq, "[5~")) er->etype = ami_etpagu;
    else if (!strcmp(kbdseq, "[6~")) er->etype = ami_etpagd;
    else if (!strcmp(kbdseq, "[1;5D")) er->etype = ami_etleftw;
    else if (!strcmp(kbdseq, "[1;5C")) er->etype = ami_etrightw;
    else if (!strcmp(kbdseq, "OP")||!strcmp(kbdseq, "[11~"))
        { er->etype = ami_etfun; er->fkey = 1; }
    else if (!strcmp(kbdseq, "OQ")||!strcmp(kbdseq, "[12~"))
        { er->etype = ami_etfun; er->fkey = 2; }
    else if (!strcmp(kbdseq, "OR")||!strcmp(kbdseq, "[13~"))
        { er->etype = ami_etfun; er->fkey = 3; }
    else if (!strcmp(kbdseq, "OS")||!strcmp(kbdseq, "[14~"))
        { er->etype = ami_etfun; er->fkey = 4; }
    else if (!strcmp(kbdseq, "[15~")) { er->etype = ami_etfun; er->fkey = 5; }
    else if (!strcmp(kbdseq, "[17~")) { er->etype = ami_etfun; er->fkey = 6; }
    else if (!strcmp(kbdseq, "[18~")) { er->etype = ami_etfun; er->fkey = 7; }
    else if (!strcmp(kbdseq, "[19~")) { er->etype = ami_etfun; er->fkey = 8; }
    else if (!strcmp(kbdseq, "[20~")) { er->etype = ami_etfun; er->fkey = 9; }
    else if (!strcmp(kbdseq, "[21~")) { er->etype = ami_etfun; er->fkey = 10; }
    else if (!strcmp(kbdseq, "[23~")) { er->etype = ami_etfun; er->fkey = 11; }
    else if (!strcmp(kbdseq, "[24~")) { er->etype = ami_etfun; er->fkey = 12; }
    else r = FALSE; /* nothing we know */
    kbdseql = 0; /* clear sequence */
    kbdseq[0] = 0;

    return (r);

}

/* is the pending sequence complete? A CSI sequence ends on a character
   in @..~; an SS3 (O-prefixed) sequence is one character after the O */
static int kbdcmpl(void)

{

    if (!kbdseql) return (FALSE);
    if (kbdseq[0] == '[')
        return (kbdseql > 1 && kbdseq[kbdseql-1] >= '@' &&
                kbdseq[kbdseql-1] <= '~');
    if (kbdseq[0] == 'O') return (kbdseql > 1);

    return (TRUE); /* single character after ESC: alt-key, complete */

}

/* process one keyboard byte; returns TRUE with an event */
static int kbdbyte(unsigned char b, ami_evtrec* er)

{

    int r;

    r = FALSE;
    er->winid = 1;
    if (kbdseql < 0) { /* escape received, sequence pending */

        if (kbdseql == -1 && b != '[' && b != 'O') {

            /* ESC followed by an ordinary character: deliver escape as
               cancel, then take the character on the next byte */
            kbdseql = 0;
            er->etype = ami_etcan;
            r = TRUE;
            /* the character itself is lost as a control accompaniment;
               plain characters follow the normal path next time */

        } else {

            kbdseql = 0;
            kbdseq[kbdseql++] = b;
            kbdseq[kbdseql] = 0;

        }

    } else if (kbdseql > 0) { /* inside a sequence */

        if (kbdseql < (int)sizeof(kbdseq)-1) {

            kbdseq[kbdseql++] = b;
            kbdseq[kbdseql] = 0;

        } else kbdseql = 0; /* overflow, drop */
        if (kbdcmpl()) r = kbdxlat(er);

    } else if (b == 0x1b) kbdseql = -1; /* escape: start sequence */
    else if (b == '\r' || b == '\n') { er->etype = ami_etenter; r = TRUE; }
    else if (b == '\t') { er->etype = ami_ettab; r = TRUE; }
    else if (b == 0x7f || b == '\b')
        { er->etype = ami_etdelcb; r = TRUE; } /* backspace */
    else if (b == 0x03) { /* ctrl-c: stop */

        er->etype = ami_etterm;
        fend = TRUE;
        r = TRUE;

    } else if (b == 0x13) { er->etype = ami_etstop; r = TRUE; } /* ctrl-s */
    else if (b == 0x11) { er->etype = ami_etcont; r = TRUE; } /* ctrl-q */
    else if (b >= ' ') { /* ordinary character */

        er->etype = ami_etchar;
        er->echar = b;
        r = TRUE;

    }

    return (r);

}

/*******************************************************************************

Joystick events

The Linux joystick device delivers button and axis records; they are
translated to Petit-Ami joystick events, with axes scaled to the
LONG_MAX ratio range.

*******************************************************************************/

#ifndef NOJOYSTICK
static int joyevt(ami_evtrec* er, joyptr jp)

{

    struct js_event ev;
    ssize_t         rl;
    int             keep;
    ami_long        v;

    keep = FALSE;
    rl = read(jp->fid, &ev, sizeof(ev));
    if (rl == sizeof(ev)) {

        ev.type &= ~JS_EVENT_INIT; /* the synthetic initial state reports */
        if (ev.type == JS_EVENT_BUTTON) {

            er->winid = 1;
            if (ev.value) { /* press */

                er->etype = ami_etjoyba;
                er->ajoyn = jp->no;
                er->ajoybn = ev.number+1;

            } else { /* release */

                er->etype = ami_etjoybd;
                er->djoyn = jp->no;
                er->djoybn = ev.number+1;

            }
            keep = TRUE;

        } else if (ev.type == JS_EVENT_AXIS) {

            /* scale to the LONG_MAX ratio range */
            v = (ami_long)ev.value*(LONG_MAX/32768);
            switch (ev.number) {

                case 0: jp->ax = v; break;
                case 1: jp->ay = v; break;
                case 2: jp->az = v; break;
                case 3: jp->a4 = v; break;
                case 4: jp->a5 = v; break;
                case 5: jp->a6 = v; break;

            }
            er->winid = 1;
            er->etype = ami_etjoymov;
            er->mjoyn = jp->no;
            er->joypx = jp->ax;
            er->joypy = jp->ay;
            er->joypz = jp->az;
            er->joyp4 = jp->a4;
            er->joyp5 = jp->a5;
            er->joyp6 = jp->a6;
            keep = TRUE;

        }

    }

    return (keep);

}
#endif

/*******************************************************************************

Mouse events

The mouse arrives on /dev/input/mice as PS/2 packets: a button byte
with a sync bit, then x and y deltas. Position is tracked here and the
pointer drawn; movement and button edges queue as Petit-Ami events,
movement before buttons so a click arrives at a current position.

*******************************************************************************/

/* process one complete packet, queueing the events it makes */
static void micepacket(winptr win)

{

    int        dx, dy;
    int        nb1, nb2, nb3;
    ami_long   nxg, nyg, nx, ny;
    ami_evtrec er;

    /* deltas, sign bits in the header byte; device y grows upward */
    dx = micebuf[1] - ((micebuf[0] & 0x10) ? 256 : 0);
    dy = micebuf[2] - ((micebuf[0] & 0x20) ? 256 : 0);
    dy = -dy;
    /* buttons: left, right, middle to Ami 1, 3, 2 */
    nb1 = !!(micebuf[0] & 0x01);
    nb3 = !!(micebuf[0] & 0x02);
    nb2 = !!(micebuf[0] & 0x04);

    if (dx || dy || !mptrvis) {

        nxg = win->mpxg+dx;
        nyg = win->mpyg+dy;
        if (nxg < 1) nxg = 1;
        if (nyg < 1) nyg = 1;
        if (nxg > win->gmaxxg) nxg = win->gmaxxg;
        if (nyg > win->gmaxyg) nyg = win->gmaxyg;
        mptrmove((int)nxg-1, (int)nyg-1); /* show the pointer there */
        if (nxg != win->mpxg || nyg != win->mpyg) {

            win->mpxg = nxg;
            win->mpyg = nyg;
            er.winid = win->wid;
            er.etype = ami_etmoumovg;
            er.mmoung = 1;
            er.moupxg = nxg;
            er.moupyg = nyg;
            enquepaevt(&er);
            /* the character position, when it crosses a cell */
            nx = (nxg-1)/win->charspace+1;
            ny = (nyg-1)/win->linespace+1;
            if (nx != win->mpx || ny != win->mpy) {

                win->mpx = nx;
                win->mpy = ny;
                er.winid = win->wid;
                er.etype = ami_etmoumov;
                er.mmoun = 1;
                er.moupx = nx;
                er.moupy = ny;
                enquepaevt(&er);

            }

        }

    }
    /* button edges */
    if (nb1 != win->mb1) {

        win->mb1 = nb1;
        er.winid = win->wid;
        er.etype = nb1? ami_etmouba: ami_etmoubd;
        if (nb1) { er.amoun = 1; er.amoubn = 1; }
        else { er.dmoun = 1; er.dmoubn = 1; }
        enquepaevt(&er);

    }
    if (nb2 != win->mb2) {

        win->mb2 = nb2;
        er.winid = win->wid;
        er.etype = nb2? ami_etmouba: ami_etmoubd;
        if (nb2) { er.amoun = 1; er.amoubn = 2; }
        else { er.dmoun = 1; er.dmoubn = 2; }
        enquepaevt(&er);

    }
    if (nb3 != win->mb3) {

        win->mb3 = nb3;
        er.winid = win->wid;
        er.etype = nb3? ami_etmouba: ami_etmoubd;
        if (nb3) { er.amoun = 1; er.amoubn = 3; }
        else { er.dmoun = 1; er.dmoubn = 3; }
        enquepaevt(&er);

    }

}

/* drain the mouse device, accumulating packets; resync on a byte
   without the sync bit where a header should be */
static void miceread(winptr win)

{

    unsigned char b;

    while (read(micefd, &b, 1) == 1) {

        if (micecnt == 0 && !(b & 0x08)) continue; /* not a header: resync */
        micebuf[micecnt++] = b;
        if (micecnt == 3) {

            micecnt = 0;
            micepacket(win);

        }

    }

}

/*******************************************************************************

Acquire next input event

Waits for and returns the next event: keyboard characters and their
translations, mouse movement and buttons, timers, the frame timer,
joysticks, and terminate on SIGINT/SIGTERM.

*******************************************************************************/

static void ievent(FILE* f, ami_evtrec* er)

{

    int           keep; /* keep event flag */
    sysevt        sev;  /* system event */
    unsigned char b;    /* keyboard byte */
    ssize_t       rl;

    keep = FALSE; /* set do not keep event */
    do {

        system_event_getsevt(&sev); /* get the next system event */
        if (sev.typ == se_inp) {

            if (sev.lse == kbdsev) {

                /* keyboard bytes: run each through the decoder; queue
                   any events past the first */
                rl = read(0, &b, 1);
                if (rl == 1) keep = kbdbyte(b, er);

            } else if (micefd >= 0 && sev.lse == micesev) {

                /* mouse packets queue their events; take the first */
                miceread(thewin);
                keep = dequepaevt(er);

            }
#ifndef NOJOYSTICK
            else if (sev.lse >= 1 && sev.lse <= MAXSID &&
                     sidtab[sev.lse-1] && sidtab[sev.lse-1]->joy && joyenb)
                /* process joystick event */
                keep = joyevt(er, joytab[sidtab[sev.lse-1]->joy-1]);
#endif

        } else if (sev.typ == se_sig) {

            if (sev.lse == intsev || sev.lse == termsev) {

                /* the shell asked this program to stop */
                er->etype = ami_etterm;
                er->winid = 1;
                fend = TRUE;
                keep = TRUE;

            }

        } else if (sev.typ == se_tim) {

            if (sev.lse >= 1 && sev.lse <= MAXSID && sidtab[sev.lse-1]) {

                if (sidtab[sev.lse-1]->frm) {

                    /* frame event */
                    er->etype = ami_etframe;
                    er->winid = 1;
                    keep = TRUE;

                } else {

                    /* timer event */
                    er->etype = ami_ettim;
                    er->timnum = sidtab[sev.lse-1]->tim;
                    er->winid = 1;
                    keep = TRUE;

                }

            }

        }

    } while (!keep); /* until we have a client event */

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into
the window's input line buffer.

*******************************************************************************/

static void readline(int fd)

{

    ami_evtrec er;  /* event record */
    scnptr    sc;   /* pointer to current screen */
    winptr    win;  /* window pointer */
    int       ins;  /* insert/overwrite mode */
    int       lcmp; /* line complete */
    int       i;
    int       ofn;  /* logical output file */

    lcmp = FALSE; /* set line not complete */
    ins = 1;
    do { /* get line characters */

        ami_event(opnfil[fd]->sfp, &er); /* get next event */
        ofn = xltwin[er.winid+MAXFIL]; /* get logical output file */
        if (ofn >= 0 && opnfil[ofn]->inl == fd) {

            /* output file indexes our input file */
            win = lwn2win(er.winid); /* get the window from the id */
            sc = win->screens[win->curupd-1]; /* index current screen */
            if (win->inpptr < 0) { /* buffer is flagged empty */

                win->inpptr = 0; /* reset input */
                win->inpbuf[win->inpptr] = 0; /* and terminate buffer */
                ins = 1;

            }
            switch (er.etype) { /* event */

                case ami_etterm: exit(1); /* halt program */
                case ami_etenter: /* line terminate */
                    while (win->inpbuf[win->inpptr])
                        win->inpptr++; /* advance to end */
                    win->inpbuf[win->inpptr] = '\n'; /* return newline */
                    /* terminate the line */
                    win->inpbuf[win->inpptr+1] = 0;
                    plcchr(win, '\r'); /* output newline sequence */
                    plcchr(win, '\n');
                    lcmp = TRUE; /* set line complete */
                    break;
                case ami_etchar: /* character */
                    if (win->inpptr < MAXLIN-2) {

                        if (ins) { /* insert */

                            i = win->inpptr; /* find end */
                            while (win->inpbuf[i]) i++;
                            /* move line up */
                            while (win->inpptr <= i)
                                { win->inpbuf[i+1] = win->inpbuf[i]; i--; }
                            /* place new character */
                            win->inpbuf[win->inpptr] = er.echar;
                            /* reprint line */
                            i = win->inpptr;
                            while (win->inpbuf[i])
                                plcchr(win, win->inpbuf[i++]);
                            /* back up */
                            i = win->inpptr;
                            while (win->inpbuf[i++]) plcchr(win, '\b');
                            /* forward and next char */
                            plcchr(win, win->inpbuf[win->inpptr]);
                            win->inpptr++;

                        } else { /* overwrite */

                            /* if end, move end marker */
                            if (!win->inpbuf[win->inpptr])
                                win->inpbuf[win->inpptr+1] = 0;
                            /* place new character */
                            win->inpbuf[win->inpptr] = er.echar;
                            /* forward and next char */
                            plcchr(win, win->inpbuf[win->inpptr]);
                            win->inpptr++;

                        }

                    }
                    break;
                case ami_etdelcb: /* delete character backwards */
                    if (win->inpptr > 0) { /* not at extreme left */

                        win->inpptr--; /* back up pointer */
                        /* move characters back */
                        i = win->inpptr;
                        while (win->inpbuf[i])
                            { win->inpbuf[i] = win->inpbuf[i+1]; i++; }
                        plcchr(win, '\b'); /* move cursor back */
                        /* repaint line */
                        i = win->inpptr;
                        while (win->inpbuf[i]) plcchr(win, win->inpbuf[i++]);
                        plcchr(win, ' '); /* blank last */
                        /* back up */
                        plcchr(win, '\b');
                        i = win->inpptr;
                        while (win->inpbuf[i++]) plcchr(win, '\b');

                    }
                    break;
                case ami_etdelcf: /* delete character forward */
                    if (win->inpbuf[win->inpptr]) { /* not at extreme right */

                        /* move characters down */
                        i = win->inpptr;
                        while (win->inpbuf[i])
                            { win->inpbuf[i] = win->inpbuf[i+1]; i++; }
                        /* repaint right */
                        i = win->inpptr;
                        while (win->inpbuf[i]) plcchr(win, win->inpbuf[i++]);
                        plcchr(win, ' '); /* blank last */
                        /* back up */
                        plcchr(win, '\b');
                        i = win->inpptr;
                        while (win->inpbuf[i++]) plcchr(win, '\b');

                    }
                    break;
                case ami_etright: /* right character */
                    /* not at extreme right, go right */
                    if (win->inpbuf[win->inpptr]) {

                        plcchr(win, win->inpbuf[win->inpptr]);
                        win->inpptr++; /* advance input */

                    }
                    break;
                case ami_etleft: /* left character */
                    /* not at extreme left, go left */
                    if (win->inpptr > 0) {

                        plcchr(win, '\b');
                        win->inpptr--; /* back up pointer */

                    }
                    break;
                case ami_ethomel: /* beginning of line */
                    /* back up to start of line */
                    while (win->inpptr) {

                        plcchr(win, '\b');
                        win->inpptr--;

                    }
                    break;
                case ami_etendl: /* end of line */
                    /* go to end of line */
                    while (win->inpbuf[win->inpptr]) {

                        plcchr(win, win->inpbuf[win->inpptr]);
                        win->inpptr++;

                    }
                    break;
                case ami_etinsertt: /* toggle insert mode */
                    ins = !ins; /* toggle insert mode */
                    break;
                case ami_etdell: /* delete whole line */
                    /* back up to start of line */
                    while (win->inpptr) {

                        plcchr(win, '\b');
                        win->inpptr--;

                    }
                    /* erase line on screen */
                    while (win->inpbuf[win->inpptr]) {

                        plcchr(win, ' ');
                        win->inpptr++;

                    }
                    /* back up again */
                    while (win->inpptr) {

                        plcchr(win, '\b');
                        win->inpptr--;

                    }
                    win->inpbuf[win->inpptr] = 0; /* clear line */
                    break;
                case ami_etleftw: /* left word */
                    /* back over any spaces */
                    while (win->inpptr && win->inpbuf[win->inpptr-1] == ' ') {

                        plcchr(win, '\b');
                        win->inpptr--;

                    }
                    /* now back over any non-space */
                    while (win->inpptr && win->inpbuf[win->inpptr-1] != ' ') {

                        plcchr(win, '\b');
                        win->inpptr--;

                    }
                    break;
                case ami_etrightw: /* right word */
                    /* advance over any non-space */
                    while (win->inpbuf[win->inpptr] &&
                           win->inpbuf[win->inpptr] != ' ') {

                        plcchr(win, win->inpbuf[win->inpptr]);
                        win->inpptr++;

                    }
                    /* advance over any spaces */
                    while (win->inpbuf[win->inpptr] &&
                           win->inpbuf[win->inpptr] == ' ') {

                        plcchr(win, win->inpbuf[win->inpptr]);
                        win->inpptr++;

                    }
                    break;
                default: ;

            }

        }

    } while (!lcmp); /* until line complete */
    win->inpptr = 0; /* set 1st position on active line */

}

/*******************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement
stdio. The 0 (input) and 1 (output) files are interdicted and routed to
the screen and keyboard.

*******************************************************************************/

/* find window with non-zero input buffer */
static int fndful(int fd) /* output window file */

{

    int fi; /* file index */
    int ff; /* found file */

    ff = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++) if (opnfil[fi])
        if (opnfil[fi]->inl == fd && opnfil[fi]->win != NULL)
            /* links the input file, and has a window */
            if (strlen(opnfil[fi]->win->inpbuf) > 0) ff = fi; /* found one */

    return (ff); /* return result */

}

static ssize_t ivread(pread_t readdc, int fd, void* buff, size_t count)

{

    int            l;   /* length left on destination */
    winptr         win; /* pointer to window data */
    int            ofn; /* output file handle */
    ssize_t        rc;  /* return code */
    unsigned char* ba;

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->inw) { /* process input file */

        ba = (unsigned char*)buff; /* index start of buffer */
        l = count; /* set length of destination */
        while (l > 0) { /* while there is space left in the buffer */

            /* find any window with a buffer with data that points to this
               input file */
            ofn = fndful(fd);
            if (ofn == -1) readline(fd); /* none, read a buffer */
            else { /* read characters */

                win = lfn2win(ofn); /* get the window */
                while (win->inpbuf[win->inpptr] && l) {

                    /* there is data in the buffer, and we need that data */
                    *ba = win->inpbuf[win->inpptr]; /* get next character */
                    if (win->inpptr < MAXLIN) win->inpptr++; /* next */
                    /* if we have just read the last of that line, flag
                       buffer empty */
                    if (*ba == '\n') {

                        win->inpptr = -1;
                        win->inpbuf[0] = 0;

                    }
                    l--; /* count characters */
                    ba++;

                }

            }

        }
        rc = count; /* set all bytes read */

    } else rc = (*readdc)(fd, buff, count);

    return rc;

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    return ivread(ofpread, fd, buff, count);

}

static ssize_t iread_nocancel(int fd, void* buff, size_t count)

{

    return ivread(ofpread_nocancel, fd, buff, count);

}

static ssize_t ivwrite(pwrite_t writedc, int fd, const void* buff, size_t count)

{

    ssize_t rc; /* return code */
    char*   p = (char *)buff;
    size_t  cnt = count;
    winptr  win; /* pointer to window data */

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->win) { /* process window output file */

        win = opnfil[fd]->win; /* index window */
        /* send data to terminal */
        while (cnt--) plcchr(win, *p++);
        rc = count; /* set return same as count */

    } else rc = (*writedc)(fd, buff, count);

    return rc;

}

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    return ivwrite(ofpwrite, fd, buff, count);

}

static ssize_t iwrite_nocancel(int fd, const void* buff, size_t count)

{

    return ivwrite(ofpwrite_nocancel, fd, buff, count);

}

/* the screen files are assumed opened when the system starts and closed
   when it shuts down, so open, close and lseek pass through */
static int iopen(const char* pathname, int flags, int perm)

{

    return (*ofpopen)(pathname, flags, perm);

}

static int iopen_nocancel(const char* pathname, int flags, int perm)

{

    return (*ofpopen_nocancel)(pathname, flags, perm);

}

static int iclose(int fd)

{

    return (*ofpclose)(fd);

}

static int iclose_nocancel(int fd)

{

    return (*ofpclose_nocancel)(fd);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    return (*ofplseek)(fd, offset, whence);

}

/*******************************************************************************

Override vectors

Each API call runs through a vector so that it can be overridden.

*******************************************************************************/

static ami_cursor_t          cursor_vect;
static ami_maxx_t            maxx_vect;
static ami_maxy_t            maxy_vect;
static ami_home_t            home_vect;
static ami_del_t             del_vect;
static ami_up_t              up_vect;
static ami_down_t            down_vect;
static ami_left_t            left_vect;
static ami_right_t           right_vect;
static ami_blink_t           blink_vect;
static ami_reverse_t         reverse_vect;
static ami_underline_t       underline_vect;
static ami_superscript_t     superscript_vect;
static ami_subscript_t       subscript_vect;
static ami_italic_t          italic_vect;
static ami_bold_t            bold_vect;
static ami_strikeout_t       strikeout_vect;
static ami_standout_t        standout_vect;
static ami_fcolor_t          fcolor_vect;
static ami_bcolor_t          bcolor_vect;
static ami_auto_t            auto_vect;
static ami_curvis_t          curvis_vect;
static ami_scroll_t          scroll_vect;
static ami_curx_t            curx_vect;
static ami_cury_t            cury_vect;
static ami_curbnd_t          curbnd_vect;
static ami_select_t          select_vect;
static ami_event_t           event_vect;
static ami_timer_t           timer_vect;
static ami_killtimer_t       killtimer_vect;
static ami_mouse_t           mouse_vect;
static ami_mousebutton_t     mousebutton_vect;
static ami_joystick_t        joystick_vect;
static ami_joybutton_t       joybutton_vect;
static ami_joyaxis_t         joyaxis_vect;
static ami_settab_t          settab_vect;
static ami_restab_t          restab_vect;
static ami_clrtab_t          clrtab_vect;
static ami_funkey_t          funkey_vect;
static ami_frametimer_t      frametimer_vect;
static ami_autohold_t        autohold_vect;
static ami_wrtstr_t          wrtstr_vect;
static ami_wrtstrn_t         wrtstrn_vect;
static ami_eventover_t       eventover_vect;
static ami_eventsover_t      eventsover_vect;
static ami_sendevent_t       sendevent_vect;
static ami_maxxg_t           maxxg_vect;
static ami_maxyg_t           maxyg_vect;
static ami_curxg_t           curxg_vect;
static ami_curyg_t           curyg_vect;
static ami_line_t            line_vect;
static ami_linewidth_t       linewidth_vect;
static ami_linestyle_t       linestyle_vect;
static ami_rect_t            rect_vect;
static ami_frect_t           frect_vect;
static ami_rrect_t           rrect_vect;
static ami_frrect_t          frrect_vect;
static ami_ellipse_t         ellipse_vect;
static ami_fellipse_t        fellipse_vect;
static ami_arc_t             arc_vect;
static ami_farc_t            farc_vect;
static ami_fchord_t          fchord_vect;
static ami_ftriangle_t       ftriangle_vect;
static ami_cursorg_t         cursorg_vect;
static ami_baseline_t        baseline_vect;
static ami_setpixel_t        setpixel_vect;
static ami_fover_t           fover_vect;
static ami_bover_t           bover_vect;
static ami_finvis_t          finvis_vect;
static ami_binvis_t          binvis_vect;
static ami_fxor_t            fxor_vect;
static ami_bxor_t            bxor_vect;
static ami_fand_t            fand_vect;
static ami_band_t            band_vect;
static ami_for_t             for_vect;
static ami_bor_t             bor_vect;
static ami_chrsizx_t         chrsizx_vect;
static ami_chrsizy_t         chrsizy_vect;
static ami_fonts_t           fonts_vect;
static ami_font_t            font_vect;
static ami_fontnam_t         fontnam_vect;
static ami_fontsiz_t         fontsiz_vect;
static ami_setpoints_t       setpoints_vect;
static ami_points_t          points_vect;
static ami_chrspcy_t         chrspcy_vect;
static ami_chrspcx_t         chrspcx_vect;
static ami_dpmx_t            dpmx_vect;
static ami_dpmy_t            dpmy_vect;
static ami_strsiz_t          strsiz_vect;
static ami_chrpos_t          chrpos_vect;
static ami_writejust_t       writejust_vect;
static ami_justpos_t         justpos_vect;
static ami_condensed_t       condensed_vect;
static ami_extended_t        extended_vect;
static ami_xlight_t          xlight_vect;
static ami_light_t           light_vect;
static ami_xbold_t           xbold_vect;
static ami_hollow_t          hollow_vect;
static ami_raised_t          raised_vect;
static ami_settabg_t         settabg_vect;
static ami_restabg_t         restabg_vect;
static ami_fcolorg_t         fcolorg_vect;
static ami_fcolorc_t         fcolorc_vect;
static ami_bcolorg_t         bcolorg_vect;
static ami_bcolorc_t         bcolorc_vect;
static ami_loadpict_t        loadpict_vect;
static ami_pictsizx_t        pictsizx_vect;
static ami_pictsizy_t        pictsizy_vect;
static ami_picture_t         picture_vect;
static ami_delpict_t         delpict_vect;
static ami_viewoffg_t        viewoffg_vect;
static ami_viewscale_t       viewscale_vect;
static ami_scalex_t          scalex_vect;
static ami_scaley_t          scaley_vect;
static ami_scrollg_t         scrollg_vect;
static ami_path_t            path_vect;
static ami_blockcopyg_t      blockcopyg_vect;
static ami_title_t           title_vect;
static ami_openwin_t         openwin_vect;
static ami_buffer_t          buffer_vect;
static ami_sizbuf_t          sizbuf_vect;
static ami_sizbufg_t         sizbufg_vect;
static ami_getsiz_t          getsiz_vect;
static ami_getsizg_t         getsizg_vect;
static ami_setsiz_t          setsiz_vect;
static ami_setsizg_t         setsizg_vect;
static ami_setpos_t          setpos_vect;
static ami_setposg_t         setposg_vect;
static ami_scnsiz_t          scnsiz_vect;
static ami_scnsizg_t         scnsizg_vect;
static ami_scncen_t          scncen_vect;
static ami_scnceng_t         scnceng_vect;
static ami_winclient_t       winclient_vect;
static ami_winclientg_t      winclientg_vect;
static ami_front_t           front_vect;
static ami_back_t            back_vect;
static ami_frame_t           frame_vect;
static ami_sizable_t         sizable_vect;
static ami_sysbar_t          sysbar_vect;
static ami_menu_t            menu_vect;
static ami_menuena_t         menuena_vect;
static ami_menusel_t         menusel_vect;
static ami_stdmenu_t         stdmenu_vect;
static ami_getwinid_t        getwinid_vect;
static ami_focus_t           focus_vect;
static ami_getwigid_t        getwigid_vect;
static ami_killwidget_t      killwidget_vect;
static ami_selectwidget_t    selectwidget_vect;
static ami_enablewidget_t    enablewidget_vect;
static ami_getwidgettext_t   getwidgettext_vect;
static ami_putwidgettext_t   putwidgettext_vect;
static ami_sizwidget_t       sizwidget_vect;
static ami_sizwidgetg_t      sizwidgetg_vect;
static ami_poswidget_t       poswidget_vect;
static ami_poswidgetg_t      poswidgetg_vect;
static ami_backwidget_t      backwidget_vect;
static ami_frontwidget_t     frontwidget_vect;
static ami_focuswidget_t     focuswidget_vect;
static ami_buttonsiz_t       buttonsiz_vect;
static ami_buttonsizg_t      buttonsizg_vect;
static ami_button_t          button_vect;
static ami_buttong_t         buttong_vect;
static ami_checkboxsiz_t     checkboxsiz_vect;
static ami_checkboxsizg_t    checkboxsizg_vect;
static ami_checkbox_t        checkbox_vect;
static ami_checkboxg_t       checkboxg_vect;
static ami_radiobuttonsiz_t  radiobuttonsiz_vect;
static ami_radiobuttonsizg_t radiobuttonsizg_vect;
static ami_radiobutton_t     radiobutton_vect;
static ami_radiobuttong_t    radiobuttong_vect;
static ami_groupsizg_t       groupsizg_vect;
static ami_groupsiz_t        groupsiz_vect;
static ami_group_t           group_vect;
static ami_groupg_t          groupg_vect;
static ami_background_t      background_vect;
static ami_backgroundg_t     backgroundg_vect;
static ami_scrollvertsizg_t  scrollvertsizg_vect;
static ami_scrollvertsiz_t   scrollvertsiz_vect;
static ami_scrollvert_t      scrollvert_vect;
static ami_scrollvertg_t     scrollvertg_vect;
static ami_scrollhorizsizg_t scrollhorizsizg_vect;
static ami_scrollhorizsiz_t  scrollhorizsiz_vect;
static ami_scrollhoriz_t     scrollhoriz_vect;
static ami_scrollhorizg_t    scrollhorizg_vect;
static ami_scrollpos_t       scrollpos_vect;
static ami_scrollsiz_t       scrollsiz_vect;
static ami_numselboxsizg_t   numselboxsizg_vect;
static ami_numselboxsiz_t    numselboxsiz_vect;
static ami_numselbox_t       numselbox_vect;
static ami_numselboxg_t      numselboxg_vect;
static ami_editboxsizg_t     editboxsizg_vect;
static ami_editboxsiz_t      editboxsiz_vect;
static ami_editbox_t         editbox_vect;
static ami_editboxg_t        editboxg_vect;
static ami_progbarsizg_t     progbarsizg_vect;
static ami_progbarsiz_t      progbarsiz_vect;
static ami_progbar_t         progbar_vect;
static ami_progbarg_t        progbarg_vect;
static ami_progbarpos_t      progbarpos_vect;
static ami_listboxsizg_t     listboxsizg_vect;
static ami_listboxsiz_t      listboxsiz_vect;
static ami_listbox_t         listbox_vect;
static ami_listboxg_t        listboxg_vect;
static ami_dropboxsizg_t     dropboxsizg_vect;
static ami_dropboxsiz_t      dropboxsiz_vect;
static ami_dropbox_t         dropbox_vect;
static ami_dropboxg_t        dropboxg_vect;
static ami_dropeditboxsizg_t dropeditboxsizg_vect;
static ami_dropeditboxsiz_t  dropeditboxsiz_vect;
static ami_dropeditbox_t     dropeditbox_vect;
static ami_dropeditboxg_t    dropeditboxg_vect;
static ami_slidehorizsizg_t  slidehorizsizg_vect;
static ami_slidehorizsiz_t   slidehorizsiz_vect;
static ami_slidehoriz_t      slidehoriz_vect;
static ami_slidehorizg_t     slidehorizg_vect;
static ami_slidevertsizg_t   slidevertsizg_vect;
static ami_slidevertsiz_t    slidevertsiz_vect;
static ami_slidevert_t       slidevert_vect;
static ami_slidevertg_t      slidevertg_vect;
static ami_tabbarsizg_t      tabbarsizg_vect;
static ami_tabbarsiz_t       tabbarsiz_vect;
static ami_tabbarclientg_t   tabbarclientg_vect;
static ami_tabbarclient_t    tabbarclient_vect;
static ami_tabbar_t          tabbar_vect;
static ami_tabbarg_t         tabbarg_vect;
static ami_tabsel_t          tabsel_vect;
static ami_alert_t           alert_vect;
static ami_querycolor_t      querycolor_vect;
static ami_queryopen_t       queryopen_vect;
static ami_querysave_t       querysave_vect;
static ami_queryfind_t       queryfind_vect;
static ami_queryfindrep_t    queryfindrep_vect;
static ami_queryfont_t       queryfont_vect;

/*******************************************************************************

Terminal level API

*******************************************************************************/

void _pa_scrollg_ovr(ami_scrollg_t nfp, ami_scrollg_t* ofp)
    { *ofp = scrollg_vect; scrollg_vect = nfp; }
void ami_scrollg(FILE* f, ami_long x, ami_long y) { (*scrollg_vect)(f, x, y); }

static void scrollg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iscrollg(win, x, y); /* process */

}

void _pa_scroll_ovr(ami_scroll_t nfp, ami_scroll_t* ofp)
    { *ofp = scroll_vect; scroll_vect = nfp; }
void ami_scroll(FILE* f, ami_long x, ami_long y) { (*scroll_vect)(f, x, y); }

static void scroll_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iscrollg(win, x*win->charspace, y*win->linespace); /* process scroll */

}

void _pa_cursor_ovr(ami_cursor_t nfp, ami_cursor_t* ofp)
    { *ofp = cursor_vect; cursor_vect = nfp; }
void ami_cursor(FILE* f, ami_long x, ami_long y) { (*cursor_vect)(f, x, y); }

static void cursor_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    icursor(win, x, y); /* process */

}

void _pa_cursorg_ovr(ami_cursorg_t nfp, ami_cursorg_t* ofp)
    { *ofp = cursorg_vect; cursorg_vect = nfp; }
void ami_cursorg(FILE* f, ami_long x, ami_long y) { (*cursorg_vect)(f, x, y); }

static void cursorg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    icursorg(win, x, y); /* process */

}

void _pa_baseline_ovr(ami_baseline_t nfp, ami_baseline_t* ofp)
    { *ofp = baseline_vect; baseline_vect = nfp; }
ami_long ami_baseline(FILE* f) { return ((*baseline_vect)(f)); }

static ami_long baseline_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->baseoff); /* return baseline offset */

}

void _pa_maxx_ovr(ami_maxx_t nfp, ami_maxx_t* ofp)
    { *ofp = maxx_vect; maxx_vect = nfp; }
ami_long ami_maxx(FILE* f) { return ((*maxx_vect)(f)); }

static ami_long maxx_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxx);

}

void _pa_maxy_ovr(ami_maxy_t nfp, ami_maxy_t* ofp)
    { *ofp = maxy_vect; maxy_vect = nfp; }
ami_long ami_maxy(FILE* f) { return ((*maxy_vect)(f)); }

static ami_long maxy_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxy);

}

void _pa_maxxg_ovr(ami_maxxg_t nfp, ami_maxxg_t* ofp)
    { *ofp = maxxg_vect; maxxg_vect = nfp; }
ami_long ami_maxxg(FILE* f) { return ((*maxxg_vect)(f)); }

static ami_long maxxg_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxxg);

}

void _pa_maxyg_ovr(ami_maxyg_t nfp, ami_maxyg_t* ofp)
    { *ofp = maxyg_vect; maxyg_vect = nfp; }
ami_long ami_maxyg(FILE* f) { return ((*maxyg_vect)(f)); }

static ami_long maxyg_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxyg);

}

void _pa_home_ovr(ami_home_t nfp, ami_home_t* ofp)
    { *ofp = home_vect; home_vect = nfp; }
void ami_home(FILE* f) { (*home_vect)(f); }

static void home_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    ihome(win); /* process */

}

void _pa_up_ovr(ami_up_t nfp, ami_up_t* ofp)
    { *ofp = up_vect; up_vect = nfp; }
void ami_up(FILE* f) { (*up_vect)(f); }

static void up_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iup(win); /* process */

}

void _pa_down_ovr(ami_down_t nfp, ami_down_t* ofp)
    { *ofp = down_vect; down_vect = nfp; }
void ami_down(FILE* f) { (*down_vect)(f); }

static void down_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    idown(win); /* process */

}

void _pa_left_ovr(ami_left_t nfp, ami_left_t* ofp)
    { *ofp = left_vect; left_vect = nfp; }
void ami_left(FILE* f) { (*left_vect)(f); }

static void left_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    ileft(win); /* process */

}

void _pa_right_ovr(ami_right_t nfp, ami_right_t* ofp)
    { *ofp = right_vect; right_vect = nfp; }
void ami_right(FILE* f) { (*right_vect)(f); }

static void right_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iright(win); /* process */

}

void _pa_del_ovr(ami_del_t nfp, ami_del_t* ofp)
    { *ofp = del_vect; del_vect = nfp; }
void ami_del(FILE* f) { (*del_vect)(f); }

static void del_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    ileft(win); /* back up cursor */
    plcchr(win, ' '); /* blank out */
    ileft(win); /* back up again */

}

/*******************************************************************************

Attributes

The simple attributes set or clear their bit; the font-changing ones
reselect the font as well.

*******************************************************************************/

/* set/clear an attribute bit */
static void setattr(FILE* f, ami_long e, scnatt a)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* attribute on */

        sc->attr |= BIT(a); /* set attribute active */
        win->gattr |= BIT(a);

    } else { /* turn it off */

        sc->attr &= ~BIT(a); /* set attribute inactive */
        win->gattr &= ~BIT(a);

    }

}

/* set/clear an attribute bit and reselect the font */
static void setattrf(FILE* f, ami_long e, scnatt a)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    setattr(f, e, a);
    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

void _pa_blink_ovr(ami_blink_t nfp, ami_blink_t* ofp)
    { *ofp = blink_vect; blink_vect = nfp; }
void ami_blink(FILE* f, ami_long e) { (*blink_vect)(f, e); }

static void blink_ivf(FILE* f, ami_long e)

{

    /* no blink capability */
    setattr(f, e, sablink);

}

void _pa_reverse_ovr(ami_reverse_t nfp, ami_reverse_t* ofp)
    { *ofp = reverse_vect; reverse_vect = nfp; }
void ami_reverse(FILE* f, ami_long e) { (*reverse_vect)(f, e); }

static void reverse_ivf(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    setattr(f, e, sarev);
    /* set the colors in the graphics context to match */
    if (BIT(sarev) & sc->attr) {

        sc->xcxt->fg = sc->bcrgb;
        sc->xcxt->bg = sc->fcrgb;

    } else {

        sc->xcxt->fg = sc->fcrgb;
        sc->xcxt->bg = sc->bcrgb;

    }

}

void _pa_underline_ovr(ami_underline_t nfp, ami_underline_t* ofp)
    { *ofp = underline_vect; underline_vect = nfp; }
void ami_underline(FILE* f, ami_long e) { (*underline_vect)(f, e); }

static void underline_ivf(FILE* f, ami_long e)

{

    setattr(f, e, saundl);

}

void _pa_superscript_ovr(ami_superscript_t nfp, ami_superscript_t* ofp)
    { *ofp = superscript_vect; superscript_vect = nfp; }
void ami_superscript(FILE* f, ami_long e) { (*superscript_vect)(f, e); }

static void superscript_ivf(FILE* f, ami_long e)

{

    setattr(f, e, sasuper);

}

void _pa_subscript_ovr(ami_subscript_t nfp, ami_subscript_t* ofp)
    { *ofp = subscript_vect; subscript_vect = nfp; }
void ami_subscript(FILE* f, ami_long e) { (*subscript_vect)(f, e); }

static void subscript_ivf(FILE* f, ami_long e)

{

    setattr(f, e, sasubs);

}

void _pa_italic_ovr(ami_italic_t nfp, ami_italic_t* ofp)
    { *ofp = italic_vect; italic_vect = nfp; }
void ami_italic(FILE* f, ami_long e) { (*italic_vect)(f, e); }

static void italic_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, saital);

}

void _pa_bold_ovr(ami_bold_t nfp, ami_bold_t* ofp)
    { *ofp = bold_vect; bold_vect = nfp; }
void ami_bold(FILE* f, ami_long e) { (*bold_vect)(f, e); }

static void bold_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, sabold);

}

void _pa_strikeout_ovr(ami_strikeout_t nfp, ami_strikeout_t* ofp)
    { *ofp = strikeout_vect; strikeout_vect = nfp; }
void ami_strikeout(FILE* f, ami_long e) { (*strikeout_vect)(f, e); }

static void strikeout_ivf(FILE* f, ami_long e)

{

    setattr(f, e, sastkout);

}

void _pa_standout_ovr(ami_standout_t nfp, ami_standout_t* ofp)
    { *ofp = standout_vect; standout_vect = nfp; }
void ami_standout(FILE* f, ami_long e) { (*standout_vect)(f, e); }

static void standout_ivf(FILE* f, ami_long e)

{

    /* standout is implemented as reverse */
    ami_reverse(f, e);

}

void _pa_condensed_ovr(ami_condensed_t nfp, ami_condensed_t* ofp)
    { *ofp = condensed_vect; condensed_vect = nfp; }
void ami_condensed(FILE* f, ami_long e) { (*condensed_vect)(f, e); }

static void condensed_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, sacondensed);

}

void _pa_extended_ovr(ami_extended_t nfp, ami_extended_t* ofp)
    { *ofp = extended_vect; extended_vect = nfp; }
void ami_extended(FILE* f, ami_long e) { (*extended_vect)(f, e); }

static void extended_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, saextended);

}

void _pa_xlight_ovr(ami_xlight_t nfp, ami_xlight_t* ofp)
    { *ofp = xlight_vect; xlight_vect = nfp; }
void ami_xlight(FILE* f, ami_long e) { (*xlight_vect)(f, e); }

static void xlight_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, saxlight);

}

void _pa_light_ovr(ami_light_t nfp, ami_light_t* ofp)
    { *ofp = light_vect; light_vect = nfp; }
void ami_light(FILE* f, ami_long e) { (*light_vect)(f, e); }

static void light_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, salight);

}

void _pa_xbold_ovr(ami_xbold_t nfp, ami_xbold_t* ofp)
    { *ofp = xbold_vect; xbold_vect = nfp; }
void ami_xbold(FILE* f, ami_long e) { (*xbold_vect)(f, e); }

static void xbold_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, saxbold);

}

void _pa_hollow_ovr(ami_hollow_t nfp, ami_hollow_t* ofp)
    { *ofp = hollow_vect; hollow_vect = nfp; }
void ami_hollow(FILE* f, ami_long e) { (*hollow_vect)(f, e); }

static void hollow_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, sahollow);

}

void _pa_raised_ovr(ami_raised_t nfp, ami_raised_t* ofp)
    { *ofp = raised_vect; raised_vect = nfp; }
void ami_raised(FILE* f, ami_long e) { (*raised_vect)(f, e); }

static void raised_ivf(FILE* f, ami_long e)

{

    setattrf(f, e, saraised);

}

/*******************************************************************************

Colors

*******************************************************************************/

void _pa_fcolor_ovr(ami_fcolor_t nfp, ami_fcolor_t* ofp)
    { *ofp = fcolor_vect; fcolor_vect = nfp; }
void ami_fcolor(FILE* f, ami_color c) { (*fcolor_vect)(f, c); }

static void fcolor_ivf(FILE* f, ami_color c)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->fcrgb = colnum(c); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) sc->xcxt->bg = sc->fcrgb;
    else sc->xcxt->fg = sc->fcrgb;

}

void _pa_fcolorc_ovr(ami_fcolorc_t nfp, ami_fcolorc_t* ofp)
    { *ofp = fcolorc_vect; fcolorc_vect = nfp; }
void ami_fcolorc(FILE* f, ami_long r, ami_long g, ami_long b) { (*fcolorc_vect)(f, r, g, b); }

static void fcolorc_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->fcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) sc->xcxt->bg = sc->fcrgb;
    else sc->xcxt->fg = sc->fcrgb;

}

void _pa_fcolorg_ovr(ami_fcolorg_t nfp, ami_fcolorg_t* ofp)
    { *ofp = fcolorg_vect; fcolorg_vect = nfp; }
void ami_fcolorg(FILE* f, ami_long r, ami_long g, ami_long b) { (*fcolorg_vect)(f, r, g, b); }

static void fcolorg_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    fcolorc_ivf(f, r, g, b);

}

void _pa_bcolor_ovr(ami_bcolor_t nfp, ami_bcolor_t* ofp)
    { *ofp = bcolor_vect; bcolor_vect = nfp; }
void ami_bcolor(FILE* f, ami_color c) { (*bcolor_vect)(f, c); }

static void bcolor_ivf(FILE* f, ami_color c)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->bcrgb = colnum(c); /* set color status */
    win->gbcrgb = sc->bcrgb;
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
    else sc->xcxt->bg = sc->bcrgb;

}

void _pa_bcolorc_ovr(ami_bcolorc_t nfp, ami_bcolorc_t* ofp)
    { *ofp = bcolorc_vect; bcolorc_vect = nfp; }
void ami_bcolorc(FILE* f, ami_long r, ami_long g, ami_long b) { (*bcolorc_vect)(f, r, g, b); }

static void bcolorc_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->bcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gbcrgb = sc->bcrgb;
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
    else sc->xcxt->bg = sc->bcrgb;

}

void _pa_bcolorg_ovr(ami_bcolorg_t nfp, ami_bcolorg_t* ofp)
    { *ofp = bcolorg_vect; bcolorg_vect = nfp; }
void ami_bcolorg(FILE* f, ami_long r, ami_long g, ami_long b) { (*bcolorg_vect)(f, r, g, b); }

static void bcolorg_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    bcolorc_ivf(f, r, g, b);

}

/*******************************************************************************

Auto, cursor visibility and position report

*******************************************************************************/

void _pa_auto_ovr(ami_auto_t nfp, ami_auto_t* ofp)
    { *ofp = auto_vect; auto_vect = nfp; }
void ami_auto(FILE* f, ami_long e) { (*auto_vect)(f, e); }

static void auto_ivf(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iauto(win, e); /* execute */

}

void _pa_curvis_ovr(ami_curvis_t nfp, ami_curvis_t* ofp)
    { *ofp = curvis_vect; curvis_vect = nfp; }
void ami_curvis(FILE* f, ami_long e) { (*curvis_vect)(f, e); }

static void curvis_ivf(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->screens[win->curupd-1]->curv = e; /* set cursor visible status */
    win->gcurv = e;
    cursts(win); /* process any cursor status change */

}

void _pa_curx_ovr(ami_curx_t nfp, ami_curx_t* ofp)
    { *ofp = curx_vect; curx_vect = nfp; }
ami_long ami_curx(FILE* f) { return ((*curx_vect)(f)); }

static ami_long curx_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curx); /* process */

}

void _pa_cury_ovr(ami_cury_t nfp, ami_cury_t* ofp)
    { *ofp = cury_vect; cury_vect = nfp; }
ami_long ami_cury(FILE* f) { return ((*cury_vect)(f)); }

static ami_long cury_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->cury); /* process */

}

void _pa_curxg_ovr(ami_curxg_t nfp, ami_curxg_t* ofp)
    { *ofp = curxg_vect; curxg_vect = nfp; }
ami_long ami_curxg(FILE* f) { return ((*curxg_vect)(f)); }

static ami_long curxg_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curxg); /* process */

}

void _pa_curyg_ovr(ami_curyg_t nfp, ami_curyg_t* ofp)
    { *ofp = curyg_vect; curyg_vect = nfp; }
ami_long ami_curyg(FILE* f) { return ((*curyg_vect)(f)); }

static ami_long curyg_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curyg); /* return yg */

}

void _pa_curbnd_ovr(ami_curbnd_t nfp, ami_curbnd_t* ofp)
    { *ofp = curbnd_vect; curbnd_vect = nfp; }
ami_long ami_curbnd(FILE* f) { return ((*curbnd_vect)(f)); }

static ami_long curbnd_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (icurbnd(win->screens[win->curupd-1]));

}

/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been
used, then a new screen is allocated and cleared.

*******************************************************************************/

void _pa_select_ovr(ami_select_t nfp, ami_select_t* ofp)
    { *ofp = select_vect; select_vect = nfp; }
void ami_select(FILE* f, ami_long u, ami_long d) { (*select_vect)(f, u, d); }

static void select_ivf(FILE* f, ami_long u, ami_long d)

{

    int    ld;  /* last display screen number save */
    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    if (!win->bufmod) error(ebufoff); /* error */
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    ld = win->curdsp; /* save the current display screen number */
    win->curupd = u; /* set the current update screen */
    if (!win->screens[win->curupd-1]) { /* no screen, create one */

        /* get a new screen context */
        win->screens[win->curupd-1] = imalloc(sizeof(scncon));
        iniscn(win, win->screens[win->curupd-1]); /* initialize that */

    }
    win->curdsp = d; /* set the current display screen */
    if (!win->screens[win->curdsp-1]) { /* no screen, create one */

        /* no current screen, create a new one */
        win->screens[win->curdsp-1] = imalloc(sizeof(scncon));
        iniscn(win, win->screens[win->curdsp-1]); /* initialize that */

    }
    /* if the screen has changed, restore it */
    if (win->curdsp != ld) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        else restore(win);

    }

}

/*******************************************************************************

Write strings

Writes a string with (and without) length to the current cursor position,
faster than character at a time, with kerning.

*******************************************************************************/

void _pa_wrtstrn_ovr(ami_wrtstrn_t nfp, ami_wrtstrn_t* ofp)
    { *ofp = wrtstrn_vect; wrtstrn_vect = nfp; }
void ami_wrtstrn(FILE* f, char* s, ami_long l) { (*wrtstrn_vect)(f, s, l); }

static void wrtstrn_ivf(FILE* f, char* s, ami_long l)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int    tw;  /* text length in pixels */
    char*  p;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (sc->autof) error(estrato); /* autowrap is on */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (sc->angle == LONG_MAX/4) { /* text is normal (90 degrees) */

        tw = ft_text_width(win->ftface, s, l); /* find text width in pixels */
        if (win->bufmod) { /* buffer is active */

            /* draw string */
            drwstr90(win, sc, tw, sc->xbuf, s, l);

        }
        if (indisp(win)) { /* do it again for the current screen */

            curoff(win); /* hide the cursor */
            /* draw string */
            drwstr90(win, sc, tw, scrcan, s, l);
            curon(win); /* show the cursor */

        }
        /* advance the cursor */
        curoff(win);
        sc->curxg += tw;
        sc->curx = sc->curxg/win->charspace+1;
        curon(win);

    } else /* off angle */
        /* just pass each character on */
        for (p = s; *p && l; p++, l--) plcchr(win, *p);

}

void _pa_wrtstr_ovr(ami_wrtstr_t nfp, ami_wrtstr_t* ofp)
    { *ofp = wrtstr_vect; wrtstr_vect = nfp; }
void ami_wrtstr(FILE* f, char* s) { (*wrtstr_vect)(f, s); }

static void wrtstr_ivf(FILE* f, char* s)

{

    ami_wrtstrn(f, s, strlen(s));

}

/*******************************************************************************

Graphical figures

Each draws to the update buffer, and again to the screen when the update
screen is displayed.

*******************************************************************************/

void _pa_line_ovr(ami_line_t nfp, ami_line_t* ofp)
    { *ofp = line_vect; line_vect = nfp; }
void ami_line(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { (*line_vect)(f, x1, y1, x2, y2); }

static void line_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        pd_line(sc->xbuf, sc->xcxt,
                L2PX(win, x1-1), L2PY(win, y1-1),
                L2PX(win, x2-1), L2PY(win, y2-1));
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        pd_line(scrcan, sc->xcxt,
                L2PX(win, x1-1), L2PY(win, y1-1),
                L2PX(win, x2-1), L2PY(win, y2-1));
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

void _pa_rect_ovr(ami_rect_t nfp, ami_rect_t* ofp)
    { *ofp = rect_vect; rect_vect = nfp; }
void ami_rect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { (*rect_vect)(f, x1, y1, x2, y2); }

static void rect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        pd_rect(sc->xbuf, sc->xcxt,
                L2PX(win, x1-1), L2PY(win, y1-1),
                L2PW(win, x2-x1), L2PH(win, y2-y1));
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        pd_rect(scrcan, sc->xcxt,
                L2PX(win, x1-1), L2PY(win, y1-1),
                L2PW(win, x2-x1), L2PH(win, y2-y1));
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

void _pa_frect_ovr(ami_frect_t nfp, ami_frect_t* ofp)
    { *ofp = frect_vect; frect_vect = nfp; }
void ami_frect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { (*frect_vect)(f, x1, y1, x2, y2); }

static void frect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        pd_frect(sc->xbuf, sc->xcxt,
                 L2PX(win, x1-1), L2PY(win, y1-1),
                 L2PW(win, x2-x1+1), L2PH(win, y2-y1+1));
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        pd_frect(scrcan, sc->xcxt,
                 L2PX(win, x1-1), L2PY(win, y1-1),
                 L2PW(win, x2-x1+1), L2PH(win, y2-y1+1));
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

void _pa_rrect_ovr(ami_rrect_t nfp, ami_rrect_t* ofp)
    { *ofp = rrect_vect; rrect_vect = nfp; }
void ami_rrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)
    { (*rrect_vect)(f, x1, y1, x2, y2, xs, ys); }

static void rrect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs,
                      ami_long ys)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */
    canvas* d;
    int     pass;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* adjust to 0 base */
    x1--; y1--; x2--; y2--;
    x1 = L2PX(win, x1); y1 = L2PY(win, y1);
    x2 = L2PX(win, x2); y2 = L2PY(win, y2);
    xs = L2PW(win, xs); ys = L2PH(win, ys);
    /* limit the size of the corner ellipse to the rectangle */
    if (xs > x2-x1+1) xs = x2-x1+1;
    if (ys > y2-y1+1) ys = y2-y1+1;
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    for (pass = 0; pass < 2; pass++) {

        if (pass == 0) {

            if (!win->bufmod) continue;
            d = sc->xbuf;

        } else {

            if (!indisp(win)) continue;
            if (!win->visible) winvis(win);
            curoff(win);
            d = scrcan;

        }
        /* stroke the sides */
        pd_line(d, sc->xcxt, x1, y1+ys/2, x1, y2-ys/2);
        pd_line(d, sc->xcxt, x2, y1+ys/2, x2, y2-ys/2);
        pd_line(d, sc->xcxt, x1+xs/2, y1, x2-xs/2, y1);
        pd_line(d, sc->xcxt, x1+xs/2, y2, x2-xs/2, y2);
        /* draw corner arcs */
        pd_arc(d, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
        pd_arc(d, sc->xcxt, x2-xs, y1, xs, ys, 0, 90*64);
        pd_arc(d, sc->xcxt, x1, y2-ys, xs, ys, 180*64, 90*64);
        pd_arc(d, sc->xcxt, x2-xs, y2-ys, xs, ys, 270*64, 90*64);
        if (pass == 1) curon(win);

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

void _pa_frrect_ovr(ami_frrect_t nfp, ami_frrect_t* ofp)
    { *ofp = frrect_vect; frrect_vect = nfp; }
void ami_frrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)
    { (*frrect_vect)(f, x1, y1, x2, y2, xs, ys); }

static void frrect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs,
                       ami_long ys)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */
    int wm;     /* width of middle rectangle */
    int hm;     /* height of middle rectangle */
    int wtb;    /* width of top/bottom rectangle */
    int htb;    /* height of top/bottom rectangle */
    int wlr;    /* width of left/right rectangle */
    int hlr;    /* height of left/right rectangle */
    canvas* d;
    int     pass;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* adjust to 0 base */
    x1--; y1--; x2--; y2--;
    x1 = L2PX(win, x1); y1 = L2PY(win, y1);
    x2 = L2PX(win, x2); y2 = L2PY(win, y2);
    xs = L2PW(win, xs); ys = L2PH(win, ys);
    /* limit the rounding ellipse to the rectangle first */
    if (xs > x2-x1+1) xs = x2-x1+1;
    if (ys > y2-y1+1) ys = y2-y1+1;
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    for (pass = 0; pass < 2; pass++) {

        if (pass == 0) {

            if (!win->bufmod) continue;
            d = sc->xbuf;

        } else {

            if (!indisp(win)) continue;
            if (!win->visible) winvis(win);
            curoff(win);
            d = scrcan;

        }
        if (x2-x1 >= y2-y1) { /* x >= y */

            /* find the widths and heights of components, and minimums */
            wm = x2-x1+1; /* set width of middle */
            if (wm < 1) wm = 1;
            hm = y2-y1+1-ys; /* set height of middle */
            if (ys%2) hm++; /* distribute fraction to middle */
            if (hm < 1) hm = 1;
            wtb = x2-x1+1-xs; /* set width of top and bottom */
            if (xs%2) wtb++; /* distribute fraction to top/bottom width */
            if (wtb < 0) wtb = 0;
            htb = ys/2; /* set height of top and bottom */
            if (y2-y1+1-hm < htb) htb = y2-y1+1-hm;
            if (htb < 0) htb = 0;
            /* middle rectangle */
            pd_frect(d, sc->xcxt, x1, y1+ys/2, wm, hm);
            /* top */
            pd_frect(d, sc->xcxt, x1+xs/2, y1, wtb, htb);
            /* bottom */
            pd_frect(d, sc->xcxt, x1+xs/2, y2-ys/2+1, wtb, htb);

        } else { /* y > x */

            wm = x2-x1+1-xs; /* set width of middle */
            if (xs%2) wm++; /* distribute fraction to middle */
            if (wm < 1) wm = 1;
            hm = y2-y1+1; /* set height of middle */
            if (hm < 1) hm = 1;
            wlr = xs/2; /* set width of left and right */
            if (x2-x1+1-wm < wlr) wlr = x2-x1+1-wm;
            if (wlr < 0) wlr = 0;
            hlr = y2-y1+1-ys; /* set height of left and right */
            if (ys%2) hlr++; /* distribute fraction */
            if (hlr < 0) hlr = 0;
            /* middle rectangle */
            pd_frect(d, sc->xcxt, x1+xs/2, y1, wm, hm);
            /* left */
            pd_frect(d, sc->xcxt, x1, y1+ys/2, wlr, hlr);
            /* right */
            pd_frect(d, sc->xcxt, x2-xs/2+1, y1+ys/2, wlr, hlr);

        }
        /* draw corner arcs */
        pd_farcpie(d, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
        pd_farcpie(d, sc->xcxt, x2-xs+1, y1, xs, ys, 0, 90*64);
        pd_farcpie(d, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64, 90*64);
        pd_farcpie(d, sc->xcxt, x2-xs+1, y2-ys+1, xs, ys, 270*64, 90*64);
        if (pass == 1) curon(win);

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

void _pa_ellipse_ovr(ami_ellipse_t nfp, ami_ellipse_t* ofp)
    { *ofp = ellipse_vect; ellipse_vect = nfp; }
void ami_ellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { (*ellipse_vect)(f, x1, y1, x2, y2); }

static void ellipse_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        pd_arc(sc->xbuf, sc->xcxt,
               L2PX(win, x1-1), L2PY(win, y1-1),
               L2PW(win, x2-x1), L2PH(win, y2-y1), 0, 360*64);
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        pd_arc(scrcan, sc->xcxt,
               L2PX(win, x1-1), L2PY(win, y1-1),
               L2PW(win, x2-x1), L2PH(win, y2-y1), 0, 360*64);
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

void _pa_fellipse_ovr(ami_fellipse_t nfp, ami_fellipse_t* ofp)
    { *ofp = fellipse_vect; fellipse_vect = nfp; }
void ami_fellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { (*fellipse_vect)(f, x1, y1, x2, y2); }

static void fellipse_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        pd_farcpie(sc->xbuf, sc->xcxt,
                   L2PX(win, x1-1), L2PY(win, y1-1),
                   L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), 0, 360*64);
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        pd_farcpie(scrcan, sc->xcxt,
                   L2PX(win, x1-1), L2PY(win, y1-1),
                   L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), 0, 360*64);
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

/* convert PA LONG_MAX ratio angle to 64ths of a degree, zero at three
   o'clock, counterclockwise */
static int rat2a64(ami_long a)

{

    /* normalize for 90 degrees origin */
    a -= LONG_MAX/4; /* normalize for 90 degrees */
    if (a < 0) a += LONG_MAX;
    a /= LONG_MAX/(360*64); /* convert to 64ths degrees */
    if (a) a = 360*64-a;

    return (a); /* return counterclockwise */

}

void _pa_arc_ovr(ami_arc_t nfp, ami_arc_t* ofp)
    { *ofp = arc_vect; arc_vect = nfp; }
void ami_arc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
    { (*arc_vect)(f, x1, y1, x2, y2, sa, ea); }

static void arc_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa,
                    ami_long ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */
    int a1, a2; /* converted angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        sc->xcxt->mix = mod2fnc[sc->fmod];
        if (win->bufmod) /* buffer is active */
            pd_arc(sc->xbuf, sc->xcxt,
                   L2PX(win, x1-1), L2PY(win, y1-1),
                   L2PW(win, x2-x1), L2PH(win, y2-y1), a1, a2);
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            pd_arc(scrcan, sc->xcxt,
                   L2PX(win, x1-1), L2PY(win, y1-1),
                   L2PW(win, x2-x1), L2PH(win, y2-y1), a1, a2);
            curon(win); /* show the cursor */

        }
        /* reset foreground function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }

}

void _pa_farc_ovr(ami_farc_t nfp, ami_farc_t* ofp)
    { *ofp = farc_vect; farc_vect = nfp; }
void ami_farc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
    { (*farc_vect)(f, x1, y1, x2, y2, sa, ea); }

static void farc_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa,
                     ami_long ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */
    int a1, a2; /* converted angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        sc->xcxt->mix = mod2fnc[sc->fmod];
        if (win->bufmod) /* buffer is active */
            pd_farcpie(sc->xbuf, sc->xcxt,
                       L2PX(win, x1-1), L2PY(win, y1-1),
                       L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), a1, a2);
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            pd_farcpie(scrcan, sc->xcxt,
                       L2PX(win, x1-1), L2PY(win, y1-1),
                       L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), a1, a2);
            curon(win); /* show the cursor */

        }
        /* reset foreground function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }

}

void _pa_fchord_ovr(ami_fchord_t nfp, ami_fchord_t* ofp)
    { *ofp = fchord_vect; fchord_vect = nfp; }
void ami_fchord(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
    { (*fchord_vect)(f, x1, y1, x2, y2, sa, ea); }

static void fchord_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa,
                       ami_long ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    ami_long tx, ty; /* temps */
    int a1, a2; /* converted angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        sc->xcxt->mix = mod2fnc[sc->fmod];
        if (win->bufmod) /* buffer is active */
            pd_farcchord(sc->xbuf, sc->xcxt,
                         L2PX(win, x1-1), L2PY(win, y1-1),
                         L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), a1, a2);
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            pd_farcchord(scrcan, sc->xcxt,
                         L2PX(win, x1-1), L2PY(win, y1-1),
                         L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), a1, a2);
            curon(win); /* show the cursor */

        }
        /* reset foreground function */
        sc->xcxt->mix = mod2fnc[mdnorm];

    }

}

void _pa_ftriangle_ovr(ami_ftriangle_t nfp, ami_ftriangle_t* ofp)
    { *ofp = ftriangle_vect; ftriangle_vect = nfp; }
void ami_ftriangle(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3)
    { (*ftriangle_vect)(f, x1, y1, x2, y2, x3, y3); }

static void ftriangle_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3,
                          ami_long y3)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int pa[6]; /* triangle point pairs */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* place the triangle points in the array */
    pa[0] = L2PX(win, x1-1);  pa[1] = L2PY(win, y1-1);
    pa[2] = L2PX(win, x2-1);  pa[3] = L2PY(win, y2-1);
    pa[4] = L2PX(win, x3-1);  pa[5] = L2PY(win, y3-1);

    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        pd_fpoly(sc->xbuf, sc->xcxt, pa, 3);
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        pd_fpoly(scrcan, sc->xcxt, pa, 3);
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

void _pa_setpixel_ovr(ami_setpixel_t nfp, ami_setpixel_t* ofp)
    { *ofp = setpixel_vect; setpixel_vect = nfp; }
void ami_setpixel(FILE* f, ami_long x, ami_long y) { (*setpixel_vect)(f, x, y); }

static void setpixel_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        pd_point(sc->xbuf, sc->xcxt, L2PX(win, x-1), L2PY(win, y-1));
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        pd_point(scrcan, sc->xcxt, L2PX(win, x-1), L2PY(win, y-1));
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

/*******************************************************************************

Write modes

*******************************************************************************/

void _pa_fover_ovr(ami_fover_t nfp, ami_fover_t* ofp)
    { *ofp = fover_vect; fover_vect = nfp; }
void ami_fover(FILE* f) { (*fover_vect)(f); }

static void fover_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gfmod = mdnorm; /* set foreground mode overwrite */
    win->screens[win->curupd-1]->fmod = mdnorm;

}

void _pa_bover_ovr(ami_bover_t nfp, ami_bover_t* ofp)
    { *ofp = bover_vect; bover_vect = nfp; }
void ami_bover(FILE* f) { (*bover_vect)(f); }

static void bover_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gbmod = mdnorm; /* set background mode overwrite */
    win->screens[win->curupd-1]->bmod = mdnorm;

}

void _pa_finvis_ovr(ami_finvis_t nfp, ami_finvis_t* ofp)
    { *ofp = finvis_vect; finvis_vect = nfp; }
void ami_finvis(FILE* f) { (*finvis_vect)(f); }

static void finvis_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gfmod = mdinvis; /* set foreground mode invisible */
    win->screens[win->curupd-1]->fmod = mdinvis;

}

void _pa_binvis_ovr(ami_binvis_t nfp, ami_binvis_t* ofp)
    { *ofp = binvis_vect; binvis_vect = nfp; }
void ami_binvis(FILE* f) { (*binvis_vect)(f); }

static void binvis_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gbmod = mdinvis; /* set background mode invisible */
    win->screens[win->curupd-1]->bmod = mdinvis;

}

void _pa_fxor_ovr(ami_fxor_t nfp, ami_fxor_t* ofp)
    { *ofp = fxor_vect; fxor_vect = nfp; }
void ami_fxor(FILE* f) { (*fxor_vect)(f); }

static void fxor_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gfmod = mdxor; /* set foreground mode xor */
    win->screens[win->curupd-1]->fmod = mdxor;

}

void _pa_bxor_ovr(ami_bxor_t nfp, ami_bxor_t* ofp)
    { *ofp = bxor_vect; bxor_vect = nfp; }
void ami_bxor(FILE* f) { (*bxor_vect)(f); }

static void bxor_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gbmod = mdxor; /* set background mode xor */
    win->screens[win->curupd-1]->bmod = mdxor;

}

void _pa_fand_ovr(ami_fand_t nfp, ami_fand_t* ofp)
    { *ofp = fand_vect; fand_vect = nfp; }
void ami_fand(FILE* f) { (*fand_vect)(f); }

static void fand_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gfmod = mdand; /* set foreground mode and */
    win->screens[win->curupd-1]->fmod = mdand;

}

void _pa_band_ovr(ami_band_t nfp, ami_band_t* ofp)
    { *ofp = band_vect; band_vect = nfp; }
void ami_band(FILE* f) { (*band_vect)(f); }

static void band_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gbmod = mdand; /* set background mode and */
    win->screens[win->curupd-1]->bmod = mdand;

}

void _pa_for_ovr(ami_for_t nfp, ami_for_t* ofp)
    { *ofp = for_vect; for_vect = nfp; }
void ami_for(FILE* f) { (*for_vect)(f); }

static void for_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gfmod = mdor; /* set foreground mode or */
    win->screens[win->curupd-1]->fmod = mdor;

}

void _pa_bor_ovr(ami_bor_t nfp, ami_bor_t* ofp)
    { *ofp = bor_vect; bor_vect = nfp; }
void ami_bor(FILE* f) { (*bor_vect)(f); }

static void bor_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    win->gbmod = mdor; /* set background mode or */
    win->screens[win->curupd-1]->bmod = mdor;

}

/*******************************************************************************

Line attributes

*******************************************************************************/

void _pa_linewidth_ovr(ami_linewidth_t nfp, ami_linewidth_t* ofp)
    { *ofp = linewidth_vect; linewidth_vect = nfp; }
void ami_linewidth(FILE* f, ami_long w) { (*linewidth_vect)(f, w); }

static void linewidth_ivf(FILE* f, ami_long w)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->lwidth = w; /* set the line width */
    sc->xcxt->lw = w; /* push to context */

}

void _pa_linestyle_ovr(ami_linestyle_t nfp, ami_linestyle_t* ofp)
    { *ofp = linestyle_vect; linestyle_vect = nfp; }
void ami_linestyle(FILE* f, ami_lstyle style) { (*linestyle_vect)(f, style); }

static void linestyle_ivf(FILE* f, ami_lstyle style)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f);
    sc = win->screens[win->curupd-1];
    sc->lstyle = style;
    /* push to context */
    switch (style) {

        case ami_lsdash: sc->xcxt->lstyle = pd_linedash; break;
        case ami_lsdot:  sc->xcxt->lstyle = pd_linedot; break;
        default:         sc->xcxt->lstyle = pd_linesolid; break;

    }

}

/*******************************************************************************

Character metrics and fonts

*******************************************************************************/

void _pa_chrsizx_ovr(ami_chrsizx_t nfp, ami_chrsizx_t* ofp)
    { *ofp = chrsizx_vect; chrsizx_vect = nfp; }
ami_long ami_chrsizx(FILE* f) { return ((*chrsizx_vect)(f)); }

static ami_long chrsizx_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->charspace); /* return character spacing */

}

void _pa_chrsizy_ovr(ami_chrsizy_t nfp, ami_chrsizy_t* ofp)
    { *ofp = chrsizy_vect; chrsizy_vect = nfp; }
ami_long ami_chrsizy(FILE* f) { return ((*chrsizy_vect)(f)); }

static ami_long chrsizy_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->linespace); /* return line spacing */

}

void _pa_fonts_ovr(ami_fonts_t nfp, ami_fonts_t* ofp)
    { *ofp = fonts_vect; fonts_vect = nfp; }
ami_long ami_fonts(FILE* f) { return ((*fonts_vect)(f)); }

static ami_long fonts_ivf(FILE* f)

{

    return (fntcnt); /* just return the global font count */

}

void _pa_font_ovr(ami_font_t nfp, ami_font_t* ofp)
    { *ofp = font_vect; font_vect = nfp; }
void ami_font(FILE* f, ami_long fc) { (*font_vect)(f, fc); }

static void font_ivf(FILE* f, ami_long fc)

{

    fontptr fp;  /* font pointer */
    winptr  win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->screens[win->curupd-1]->autof)
        error(eatoftc); /* cannot perform with auto on */
    if (fc < 1) error(einvfnm); /* invalid font number */
    /* find indicated font */
    fp = fntlst;
    while (fp != NULL && fc > 1) { /* search */

       fp = fp->next; /* next font entry */
       fc--; /* count */

    }
    if (fc > 1) error(einvfnm); /* invalid font number */
    if (!strlen(fp->fn)) error(efntemp); /* font is not assigned */
    curoff(win); /* remove cursor with old font characteristics */
    win->screens[win->curupd-1]->cfont = fp; /* place new font */
    win->gcfont = fp;
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

void _pa_fontnam_ovr(ami_fontnam_t nfp, ami_fontnam_t* ofp)
    { *ofp = fontnam_vect; fontnam_vect = nfp; }
void ami_fontnam(FILE* f, ami_long fc, char* fns, ami_long fnsl)
    { (*fontnam_vect)(f, fc, fns, fnsl); }

static void fontnam_ivf(FILE* f, ami_long fc, char* fns, ami_long fnsl)

{

    fontptr fp; /* pointer to font entries */

    if (fc <= 0) error(einvftn); /* invalid number */
    fp = fntlst; /* index top of list */
    while (fc > 1) { /* walk fonts */

       fp = fp->next; /* next font */
       fc = fc-1; /* count */
       if (!fp) error(einvftn); /* check null */

    }
    cpycrit(fns, fnsl, fp->fn); /* copy name to critical result buffer */

}

void _pa_fontsiz_ovr(ami_fontsiz_t nfp, ami_fontsiz_t* ofp)
    { *ofp = fontsiz_vect; fontsiz_vect = nfp; }
void ami_fontsiz(FILE* f, ami_long s) { (*fontsiz_vect)(f, s); }

static void fontsiz_ivf(FILE* f, ami_long s)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->screens[win->curupd-1]->autof)
        error(eatoftc); /* cannot perform with auto on */
    curoff(win); /* remove cursor with old font characteristics */
    win->gfcellh = s; /* set new target cell height */
    setfnt(win); /* find em-square pixel size for this cell height */
    win->mischrx = win->gfhigh*MISCHRX;
    win->mischry = win->gfhigh*MISCHRY;
    win->misoffx = win->gfhigh*MISOFFX;
    win->misoffy = win->gfhigh*MISOFFY;
    curon(win);

}

void _pa_setpoints_ovr(ami_setpoints_t nfp, ami_setpoints_t* ofp)
    { *ofp = setpoints_vect; setpoints_vect = nfp; }
void ami_setpoints(FILE* f, float ps) { (*setpoints_vect)(f, ps); }

static void setpoints_ivf(FILE* f, float ps)

{

    winptr win;
    int    pixsiz, asc, dsc;

    win = txt2win(f);
    if (win->screens[win->curupd-1]->autof) error(eatoftc);
    curoff(win);
    pixsiz = (int)(ps * (float)win->sdpmy / 2835.0f + 0.5f);
    if (pixsiz < 1) pixsiz = 1;
    /* apply the point size directly to the current face to measure the
       resulting cell height, then promote that to gfcellh so subsequent
       font changes preserve it */
    if (!win->ftface) setfnt(win);
    win->gfhigh_log = pixsiz;
    {
        int phys_y = (int)(pixsiz * win->vsy);
        int phys_x = (int)(pixsiz * win->vsx);

        if (phys_y < 1) phys_y = 1;
        if (phys_x < 1) phys_x = 1;
        win->gfhigh  = phys_y;
        win->gfhighx = phys_x;
    }
    pthread_mutex_lock(&ftlock);
    FT_Set_Pixel_Sizes(win->ftface, 0, pixsiz); /* logical for metrics */
    asc = (int)( win->ftface->size->metrics.ascender  >> 6);
    dsc = (int)(-win->ftface->size->metrics.descender >> 6);
    win->gfcellh = asc + dsc + 2;
    win->gfpoint = ps;
    win->linespace = win->gfcellh;
    win->baseoff = asc + 1;
    win->charspace = (int)(win->ftface->size->metrics.max_advance >> 6);
    pthread_mutex_unlock(&ftlock);
    win->mischrx = win->gfhigh*MISCHRX;
    win->mischry = win->gfhigh*MISCHRY;
    win->misoffx = win->gfhigh*MISOFFX;
    win->misoffy = win->gfhigh*MISOFFY;
    curon(win);

}

void _pa_points_ovr(ami_points_t nfp, ami_points_t* ofp)
    { *ofp = points_vect; points_vect = nfp; }
float ami_points(FILE* f) { return ((*points_vect)(f)); }

static float points_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);

    return (win->gfpoint);

}

void _pa_chrspcy_ovr(ami_chrspcy_t nfp, ami_chrspcy_t* ofp)
    { *ofp = chrspcy_vect; chrspcy_vect = nfp; }
void ami_chrspcy(FILE* f, ami_long s) { (*chrspcy_vect)(f, s); }

static void chrspcy_ivf(FILE* f, ami_long s)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->chrspcy = s; /* set leading */

}

void _pa_chrspcx_ovr(ami_chrspcx_t nfp, ami_chrspcx_t* ofp)
    { *ofp = chrspcx_vect; chrspcx_vect = nfp; }
void ami_chrspcx(FILE* f, ami_long s) { (*chrspcx_vect)(f, s); }

static void chrspcx_ivf(FILE* f, ami_long s)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->chrspcx = s; /* set spacing */

}

void _pa_dpmx_ovr(ami_dpmx_t nfp, ami_dpmx_t* ofp)
    { *ofp = dpmx_vect; dpmx_vect = nfp; }
ami_long ami_dpmx(FILE* f) { return ((*dpmx_vect)(f)); }

static ami_long dpmx_ivf(FILE* f)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */

    return (win->sdpmx); /* return value */

}

void _pa_dpmy_ovr(ami_dpmy_t nfp, ami_dpmy_t* ofp)
    { *ofp = dpmy_vect; dpmy_vect = nfp; }
ami_long ami_dpmy(FILE* f) { return ((*dpmy_vect)(f)); }

static ami_long dpmy_ivf(FILE* f)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */

    return (win->sdpmy); /* return value */

}

void _pa_strsiz_ovr(ami_strsiz_t nfp, ami_strsiz_t* ofp)
    { *ofp = strsiz_vect; strsiz_vect = nfp; }
ami_long ami_strsiz(FILE* f, const char* s) { return ((*strsiz_vect)(f, s)); }

static ami_long strsiz_ivf(FILE* f, const char* s)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */

    return (ft_text_width(win->ftface, s, strlen(s))); /* return value */

}

void _pa_chrpos_ovr(ami_chrpos_t nfp, ami_chrpos_t* ofp)
    { *ofp = chrpos_vect; chrpos_vect = nfp; }
ami_long ami_chrpos(FILE* f, const char* s, ami_long p)
    { return ((*chrpos_vect)(f, s, p)); }

static ami_long chrpos_ivf(FILE* f, const char* s, ami_long p)

{

    winptr win; /* window pointer */

    if (p < 0 || p > strlen(s)) error(estrinx); /* out of range */
    win = txt2win(f); /* get window pointer from text file */

    return (ft_text_width(win->ftface, s, p)); /* return value */

}

/*******************************************************************************

Justified text

Justification distributes the extra space among the spaces in the string.

*******************************************************************************/

void _pa_writejust_ovr(ami_writejust_t nfp, ami_writejust_t* ofp)
    { *ofp = writejust_vect; writejust_vect = nfp; }
void ami_writejust(FILE* f, const char* s, ami_long n) { (*writejust_vect)(f, s, n); }

static void writejust_ivf(FILE* f, const char* s, ami_long n)

{

    winptr win; /* window pointer */
    scnptr sc;  /* pointer to update screen */
    int    spc; /* space of spaces */
    int    ns;  /* number of spaces */
    int    ss;  /* spaces total size */
    int    sz;  /* critical size, chars+min space */
    int    cs;  /* size of characters only */
    int    cbs; /* character background spacing */
    int    i;
    int    l;

    win = txt2win(f); /* get window pointer from text file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (sc->autof) error(eatopos); /* cannot perform with auto on */
    l = strlen(s); /* find string length */
    /* find critical spacing, that is, spaces are minimum */
    sz = 0; /* clear size */
    ns = 0; /* clear number of spaces */
    cs = 0; /* clear space in characters */
    for (i = 0; i < l; i++) {

        if (s[i] == ' ') { sz += MINJST; ns++; }
        else {

            sz += xwidth(win, s[i]); /* calculate chars+min space */
            cs += xwidth(win, s[i]); /* calculate chars only */

        }

    }
    spc = MINJST; /* set minimum */
    /* if space provided is greater than the minimum, distribute the extra
       space among the existing spaces */
    ss = ns*MINJST; /* set minimum distribution of space */
    if (n > sz && ns) { spc = (n-cs)/ns; ss = n-cs; }
    /* output the string with our chosen spacing */
    for (i = 0; i < l; i++) {

        if (s[i] == ' ') {

            if (spc > ss) cbs = ss; /* set space to next character */
            else cbs = spc;
            /* draw in background */
            if (win->bufmod) { /* buffer is active */

                if (sc->bmod != mdinvis) { /* background is visible */

                    /* set background function */
                    sc->xcxt->mix = mod2fnc[sc->bmod];
                    /* set background to foreground to draw background */
                    if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->fcrgb;
                    else sc->xcxt->fg = sc->bcrgb;
                    pd_frect(sc->xbuf, sc->xcxt, sc->curxg-1, sc->curyg-1,
                             cbs, win->linespace);
                    /* restore colors */
                    if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
                    else sc->xcxt->fg = sc->fcrgb;
                    /* reset background function */
                    sc->xcxt->mix = mod2fnc[mdnorm];

                }

            }
            if (indisp(win)) { /* do it again for the current screen */

                if (!win->visible) winvis(win); /* make sure displayed */
                curoff(win); /* hide the cursor */
                if (sc->bmod != mdinvis) { /* background is visible */

                    sc->xcxt->mix = mod2fnc[sc->bmod];
                    if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->fcrgb;
                    else sc->xcxt->fg = sc->bcrgb;
                    pd_frect(scrcan, sc->xcxt, sc->curxg-1, sc->curyg-1,
                             cbs, win->linespace);
                    if (BIT(sarev) & sc->attr) sc->xcxt->fg = sc->bcrgb;
                    else sc->xcxt->fg = sc->fcrgb;
                    sc->xcxt->mix = mod2fnc[mdnorm];

                }
                curon(win); /* show the cursor */

            }
            /* now move forward */
            if (spc > ss) sc->curxg += ss; /* space off */
            else { sc->curxg += spc; ss -= spc; }

        } else plcchr(win, s[i]); /* print the character with natural spacing */

    }

}

void _pa_justpos_ovr(ami_justpos_t nfp, ami_justpos_t* ofp)
    { *ofp = justpos_vect; justpos_vect = nfp; }
ami_long ami_justpos(FILE* f, const char* s, ami_long p, ami_long n)
    { return ((*justpos_vect)(f, s, p, n)); }

static ami_long justpos_ivf(FILE* f, const char* s, ami_long p, ami_long n)

{

    winptr win; /* window pointer */
    scnptr sc;  /* pointer to update screen */
    int    spc; /* space of spaces */
    int    ns;  /* number of spaces */
    int    ss;  /* spaces total size */
    int    sz;  /* critical size, chars+min space */
    int    cs;  /* size of characters only */
    int    cp;  /* character pixel position */
    int    crp; /* character result position */
    int    i;
    int    l;

    win = txt2win(f); /* get window pointer from text file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (sc->autof) error(eatopos); /* cannot perform with auto on */
    l = strlen(s); /* find string length */
    if (p < 0 || p >= l) error(estrinx); /* out of range */
    /* find critical spacing, that is, spaces are minimum */
    sz = 0; /* clear size */
    ns = 0; /* clear number of spaces */
    cs = 0; /* clear space in characters */
    for (i = 0; i < l; i++) {

        if (s[i] == ' ') { sz += MINJST; ns++; }
        else {

            sz += xwidth(win, s[i]); /* calculate chars+min space */
            cs += xwidth(win, s[i]); /* calculate chars only */

        }

    }
    spc = MINJST; /* set minimum */
    ss = ns*MINJST; /* set minimum distribution of space */
    if (n > sz && ns) { spc = (n-cs)/ns; ss = n-cs; }
    cp = 0; /* set 0 offset to character */
    crp = 0; /* clear result position */
    /* walk the string with our chosen spacing */
    for (i = 0; i < l; i++) {

        if (i == p) crp = cp;
        if (s[i] == ' ') {

            /* now move forward */
            if (spc > ss) cp += ss; /* space off */
            else { cp += spc; ss -= spc; }

        } else cp += xwidth(win, s[i]); /* move forward character space */

    }

    return (crp); /* return result */

}

/*******************************************************************************

Tabs

*******************************************************************************/

void _pa_settabg_ovr(ami_settabg_t nfp, ami_settabg_t* ofp)
    { *ofp = settabg_vect; settabg_vect = nfp; }
void ami_settabg(FILE* f, ami_long t) { (*settabg_vect)(f, t); }

static void settabg_ivf(FILE* f, ami_long t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, t); /* process */

}

void _pa_settab_ovr(ami_settab_t nfp, ami_settab_t* ofp)
    { *ofp = settab_vect; settab_vect = nfp; }
void ami_settab(FILE* f, ami_long t) { (*settab_vect)(f, t); }

static void settab_ivf(FILE* f, ami_long t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, (t-1)*win->charspace+1); /* translate to graphical call */

}

void _pa_restabg_ovr(ami_restabg_t nfp, ami_restabg_t* ofp)
    { *ofp = restabg_vect; restabg_vect = nfp; }
void ami_restabg(FILE* f, ami_long t) { (*restabg_vect)(f, t); }

static void restabg_ivf(FILE* f, ami_long t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, t); /* process */

}

void _pa_restab_ovr(ami_restab_t nfp, ami_restab_t* ofp)
    { *ofp = restab_vect; restab_vect = nfp; }
void ami_restab(FILE* f, ami_long t) { (*restab_vect)(f, t); }

static void restab_ivf(FILE* f, ami_long t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, (t-1)*win->charspace+1); /* translate to graphical call */

}

void _pa_clrtab_ovr(ami_clrtab_t nfp, ami_clrtab_t* ofp)
    { *ofp = clrtab_vect; clrtab_vect = nfp; }
void ami_clrtab(FILE* f) { (*clrtab_vect)(f); }

static void clrtab_ivf(FILE* f)

{

    int    i;
    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    for (i = 0; i < MAXTAB; i++) win->screens[win->curupd-1]->tab[i] = 0;

}

void _pa_funkey_ovr(ami_funkey_t nfp, ami_funkey_t* ofp)
    { *ofp = funkey_vect; funkey_vect = nfp; }
ami_long ami_funkey(FILE* f) { return ((*funkey_vect)(f)); }

static ami_long funkey_ivf(FILE* f)

{

    return (12); /* number of function keys */

}

void _pa_title_ovr(ami_title_t nfp, ami_title_t* ofp)
    { *ofp = title_vect; title_vect = nfp; }
void ami_title(FILE* f, char* ts) { (*title_vect)(f, ts); }

static void title_ivf(FILE* f, char* ts)

{

    /* the console has no title bar; accepted and ignored */

}

/*******************************************************************************

Pictures

Only .bmp files are loaded, and those must be in 24 bit Truecolor.

*******************************************************************************/

/* place extension on filename */
static void setext(char* fnh, char* ext)

{

    char* ec; /* extension character location */
    char* cp;

    /* find extension or end */
    cp = fnh;
    ec = NULL;
    while (*cp) {

        if (*cp == '.') ec = cp;
        cp++;

    }
    if (!ec) ec = cp;
    if (ec-fnh+strlen(ext) >= MAXFNM)
        error(epicftl); /* filename too large */
    strcpy(ec, ext); /* place extension */

}

static byte getbyt(FILE* f)

{

    byte b;
    size_t nb;

    nb = fread(&b, sizeof(byte), 1, f);
    if (nb != 1) error(ebadfmt);

    return (b);

}

static unsigned int read32(FILE* f)

{

    union {

        unsigned int i;
        byte         b[4];

    } i2b;
    int i;

    i2b.i = 0;
    for (i = 0; i < 4; i++) i2b.b[i] = getbyt(f);

    return (i2b.i);

}

static unsigned int read16(FILE* f)

{

    union {

        unsigned int i;
        byte         b[4];

    } i2b;
    int i;

    i2b.i = 0;
    for (i = 0; i < 2; i++) i2b.b[i] = getbyt(f);

    return (i2b.i);

}

void _pa_loadpict_ovr(ami_loadpict_t nfp, ami_loadpict_t* ofp)
    { *ofp = loadpict_vect; loadpict_vect = nfp; }
void ami_loadpict(FILE* f, ami_long p, char* fn) { (*loadpict_vect)(f, p, fn); }

static void loadpict_ivf(FILE* f, ami_long p, char* fn)

{

    winptr win; /* window pointer */
    FILE* pf; /* picture file */
    const byte signature[2] = { 0x42, 0x4d }; /* "BM" */
    unsigned int pw; /* picture width */
    unsigned int ph; /* picture height */
    byte r, g, b; /* colors */
    int pad;
    uint32_t* pp;
    unsigned int x, y;
    unsigned int t;
    int i;
    unsigned int hs;
    picptr ip; /* image pointer */
    char fnh[MAXFNM]; /* file name holder */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    /* if the slot is already occupied, delete that picture */
    delpic(win, p);
    /* copy filename and add extension if required */
    if (strlen(fn) >= MAXFNM) error(epicftl);
    strcpy(fnh, fn); /* copy */
    setext(fnh, ".bmp"); /* set or overwrite extension */
    pf = fopen(fnh, "r"); /* open picture for read only */
    if (!pf) error(epicopn); /* cannot open picture file */
    for (i = 0; i < 2; i++) { /* read and compare signature */

        b = getbyt(pf); /* get next byte */
        if (b != signature[i]) error(ebadfmt);

    }
    read32(pf); /* size of bmp file */
    read16(pf); /* reserved */
    read16(pf); /* reserved */
    read32(pf); /* offset */
    hs = read32(pf); /* size of header */
    pw = read32(pf); /* image width */
    ph = read32(pf); /* image height */
    t = read16(pf); /* get number of planes */
    if (t != 1) error(ebadfmt); /* should be single plane */
    t = read16(pf); /* get number of bits in pixel */
    if (t != 24) error(ebadfmt); /* should be 24 bits */
    t = read32(pf); /* compression type */
    if (t != 0) error(ebadfmt); /* should be no compression */
    read32(pf); /* image size */
    read32(pf); /* pixels per meter x */
    read32(pf); /* pixels per meter y */
    t = read32(pf); /* number of colors */
    if (t != 0) error(ebadfmt); /* should be no palette */
    read32(pf); /* important colors */
    /* read and dispose of the rest of the header */
    for (i = 0; i < (int)hs-40; i++) getbyt(pf);

    ip = getpic(); /* get new image entry */
    ip->next = win->pictbl[p-1]; /* push to list */
    win->pictbl[p-1] = ip;
    /* set picture size */
    ip->sx = pw;
    ip->sy = ph;
    /* create the picture canvas */
    ip->xi = newcanvas(pw, ph);
    if (!ip->xi) error(enomem);

    /* find end of row padding */
    pad = 0;
    if (pw*3%4) pad = 4-(pw*3%4);
    /* fill picture with data, bottom to top */
    for (y = 0; y < ph; y++) {

        pp = &ip->xi->px[(size_t)(ph-1-y)*pw]; /* index row from bottom */
        for (x = 0; x < pw; x++) { /* fill left to right */

            b = getbyt(pf); /* get blue */
            g = getbyt(pf); /* get green */
            r = getbyt(pf); /* get red */
            *pp++ = 0xff000000|(uint32_t)r<<16|(uint32_t)g<<8|b;

        }
        /* remove padding */
        for (i = 0; i < pad; i++) getbyt(pf);

    }
    fclose(pf); /* close the input file */

}

void _pa_delpict_ovr(ami_delpict_t nfp, ami_delpict_t* ofp)
    { *ofp = delpict_vect; delpict_vect = nfp; }
void ami_delpict(FILE* f, ami_long p) { (*delpict_vect)(f, p); }

static void delpict_ivf(FILE* f, ami_long p)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1] || !win->pictbl[p-1]->xi)
        error(einvhan); /* bad picture handle */
    delpic(win, p); /* delete all of the scaled copies */

}

void _pa_pictsizx_ovr(ami_pictsizx_t nfp, ami_pictsizx_t* ofp)
    { *ofp = pictsizx_vect; pictsizx_vect = nfp; }
ami_long ami_pictsizx(FILE* f, ami_long p) { return ((*pictsizx_vect)(f, p)); }

static ami_long pictsizx_ivf(FILE* f, ami_long p)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1] || !win->pictbl[p-1]->xi)
        error(einvhan); /* bad picture handle */

    return (win->pictbl[p-1]->sx); /* return x size */

}

void _pa_pictsizy_ovr(ami_pictsizy_t nfp, ami_pictsizy_t* ofp)
    { *ofp = pictsizy_vect; pictsizy_vect = nfp; }
ami_long ami_pictsizy(FILE* f, ami_long p) { return ((*pictsizy_vect)(f, p)); }

static ami_long pictsizy_ivf(FILE* f, ami_long p)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1] || !win->pictbl[p-1]->xi)
        error(einvhan); /* bad picture handle */

    return (win->pictbl[p-1]->sy); /* return y size */

}

void _pa_picture_ovr(ami_picture_t nfp, ami_picture_t* ofp)
    { *ofp = picture_vect; picture_vect = nfp; }
void ami_picture(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { (*picture_vect)(f, p, x1, y1, x2, y2); }

static void picture_ivf(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr  win; /* window record pointer */
    scnptr  sc;  /* screen buffer */
    ami_long    tx, ty; /* temps */
    int     pw, ph; /* picture width and height */
    picptr  pp, fp; /* picture entry pointers */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1] || !win->pictbl[p-1]->xi)
        error(einvhan); /* bad picture handle */
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1; ty = y1;
       x1 = x2; y1 = y2;
       x2 = tx; y2 = ty;

    }
    /* set picture width and height */
    pw = x2-x1+1;
    ph = y2-y1+1;
    pp = win->pictbl[p-1]; /* index top picture */
    fp = NULL; /* set none found */
    while (pp) { /* search for scale that matches */

        if (pp->sx == pw && pp->sy == ph) fp = pp; /* found matching entry */
        pp = pp->next; /* go next */

    }
    if (!fp) {

        /* new scale does not match any previous; create a new scaled
           image from the original (bottom of list) */
        pp = win->pictbl[p-1]; /* index top picture */
        while (pp->next) pp = pp->next; /* go to bottom of list */
        fp = getpic(); /* get new image entry */
        fp->next = win->pictbl[p-1]; /* push to list */
        win->pictbl[p-1] = fp;
        /* set picture size */
        fp->sx = pw;
        fp->sy = ph;
        /* create the scaled canvas */
        fp->xi = newcanvas(pw, ph);
        if (!fp->xi) error(enomem);
        rescale(fp->xi, pp->xi); /* rescale to new canvas */

    }
    /* set foreground function */
    sc->xcxt->mix = mod2fnc[sc->fmod];
    if (win->bufmod) /* buffer is active */
        blitmix(sc->xbuf, L2PX(win, x1-1), L2PY(win, y1-1), fp->xi, 0, 0,
                L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), sc->xcxt->mix);
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        blitmix(scrcan, L2PX(win, x1-1), L2PY(win, y1-1), fp->xi, 0, 0,
                L2PW(win, x2-x1+1), L2PH(win, y2-y1+1), sc->xcxt->mix);
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    sc->xcxt->mix = mod2fnc[mdnorm];

}

/*******************************************************************************

Viewport

*******************************************************************************/

void _pa_viewoffg_ovr(ami_viewoffg_t nfp, ami_viewoffg_t* ofp)
    { *ofp = viewoffg_vect; viewoffg_vect = nfp; }
void ami_viewoffg(FILE* f, ami_long x, ami_long y) { (*viewoffg_vect)(f, x, y); }

static void viewoffg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    win->goffx = x;
    win->goffy = y;

}

void _pa_viewscale_ovr(ami_viewscale_t nfp, ami_viewscale_t* ofp)
    { *ofp = viewscale_vect; viewscale_vect = nfp; }
void ami_viewscale(FILE* f, float x, float y) { (*viewscale_vect)(f, x, y); }

static void viewscale_ivf(FILE* f, float x, float y)

{

    winptr win;

    if (x <= 0.0f || y <= 0.0f) error(esystem);
    win = txt2win(f);
    win->vsx = x;
    win->vsy = y;
    setfnt(win); /* re-select font at the new pixel size */

}

void _pa_scalex_ovr(ami_scalex_t nfp, ami_scalex_t* ofp)
    { *ofp = scalex_vect; scalex_vect = nfp; }
ami_long ami_scalex(FILE* f, ami_long x) { return ((*scalex_vect)(f, x)); }

static ami_long scalex_ivf(FILE* f, ami_long x)

{

    winptr win;

    win = txt2win(f);
    if (win->vsx == 0.0f) return x; /* defensive */

    return (int)(((float)(x - win->goffx)) / win->vsx);

}

void _pa_scaley_ovr(ami_scaley_t nfp, ami_scaley_t* ofp)
    { *ofp = scaley_vect; scaley_vect = nfp; }
ami_long ami_scaley(FILE* f, ami_long y) { return ((*scaley_vect)(f, y)); }

static ami_long scaley_ivf(FILE* f, ami_long y)

{

    winptr win;

    win = txt2win(f);
    if (win->vsy == 0.0f) return y;

    return (int)(((float)(y - win->goffy)) / win->vsy);

}

/*******************************************************************************

Character path

*******************************************************************************/

void _pa_path_ovr(ami_path_t nfp, ami_path_t* ofp)
    { *ofp = path_vect; path_vect = nfp; }
void ami_path(FILE* f, ami_long a) { (*path_vect)(f, a); }

static void path_ivf(FILE* f, ami_long a)

{

    winptr win; /* pointer to windows context */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window context */
    sc = win->screens[win->curupd-1];
    if (sc->autof) error(eangato); /* autowrap is on */
    sc->angle = a; /* set drawing angle */

}

/*******************************************************************************

Block copy

Copies a block of pixels from a source screen buffer bounding box to a
destination buffer bounding box, stretched or compressed as needed. The
current foreground write mode applies.

*******************************************************************************/

void _pa_blockcopyg_ovr(ami_blockcopyg_t nfp, ami_blockcopyg_t* ofp)
    { *ofp = blockcopyg_vect; blockcopyg_vect = nfp; }
void ami_blockcopyg(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2,
                    ami_long sy2, ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2)
    { (*blockcopyg_vect)(f, s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2); }

static void blockcopyg_ivf(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1,
                           ami_long sx2, ami_long sy2, ami_long dx1, ami_long dy1,
                           ami_long dx2, ami_long dy2)

{

    winptr win; /* window record pointer */
    scnptr ss;  /* source screen */
    scnptr ds;  /* destination screen */
    scnptr cs;  /* current update screen, holder of the write mode */
    ami_long   t;   /* swap temp */
    int    psx, psy, psw, psh; /* source box, physical */
    int    pdx, pdy, pdw, pdh; /* destination box, physical */
    int    fnc; /* mix function for the write mode */

    win = txt2win(f); /* get window from file */
    if (!win->bufmod) error(ebufoff); /* buffers only exist in buffered mode */
    if (s < 1 || s > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    /* create either buffer if it does not exist yet, as select() does */
    if (!win->screens[s-1]) {

        win->screens[s-1] = imalloc(sizeof(scncon));
        iniscn(win, win->screens[s-1]);

    }
    if (!win->screens[d-1]) {

        win->screens[d-1] = imalloc(sizeof(scncon));
        iniscn(win, win->screens[d-1]);

    }
    ss = win->screens[s-1];
    ds = win->screens[d-1];
    cs = win->screens[win->curupd-1];
    if (cs->fmod == mdinvis) return; /* invisible drops the copy whole */
    /* rationalize both rectangles to top left/bottom right */
    if (sx1 > sx2) { t = sx1; sx1 = sx2; sx2 = t; }
    if (sy1 > sy2) { t = sy1; sy1 = sy2; sy2 = t; }
    if (dx1 > dx2) { t = dx1; dx1 = dx2; dx2 = t; }
    if (dy1 > dy2) { t = dy1; dy1 = dy2; dy2 = t; }
    /* transform to physical pixels */
    psx = L2PX(win, sx1-1);
    psy = L2PY(win, sy1-1);
    psw = L2PW(win, sx2-sx1+1);
    psh = L2PH(win, sy2-sy1+1);
    pdx = L2PX(win, dx1-1);
    pdy = L2PY(win, dy1-1);
    pdw = L2PW(win, dx2-dx1+1);
    pdh = L2PH(win, dy2-dy1+1);
    if (psw < 1 || psh < 1 || pdw < 1 || pdh < 1) return; /* nothing to copy */
    fnc = mod2fnc[cs->fmod]; /* the current foreground write mode */
    if (psw == pdw && psh == pdh) { /* sizes match: copy direct */

        blitmix(ds->xbuf, pdx, pdy, ss->xbuf, psx, psy, psw, psh, fnc);

    } else { /* stretch or compress through scaling canvases */

        canvas* si; /* source canvas */
        canvas* di; /* destination canvas */

        si = newcanvas(psw, psh);
        di = newcanvas(pdw, pdh);
        if (!si || !di) error(enomem);
        pd_blit(si, 0, 0, ss->xbuf, psx, psy, psw, psh);
        rescale(di, si); /* scale source to destination */
        blitmix(ds->xbuf, pdx, pdy, di, 0, 0, pdw, pdh, fnc);
        freecanvas(si); /* release both canvases */
        freecanvas(di);

    }
    if (d == win->curdsp) { /* the destination is on display */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* the buffer holds the composed result: present the destination
           box as it now stands */
        pd_blit(scrcan, pdx, pdy, ds->xbuf, pdx, pdy, pdw, pdh);
        curon(win); /* show the cursor */

    }

}

/*******************************************************************************

Events, timers and input devices

*******************************************************************************/

/* default event handler: pass the event through to the caller */
static void defaultevent(ami_evtrec* ev)

{

    ev->handled = 0; /* set unhandled */

}

void _pa_event_ovr(ami_event_t nfp, ami_event_t* ofp)
    { *ofp = event_vect; event_vect = nfp; }
void ami_event(FILE* f, ami_evtrec* er) { (*event_vect)(f, er); }

static void event_ivf(FILE* f, ami_evtrec* er)

{

    do { /* loop handling via event vectors and queuing */

        /* check input PA queue; if empty, get an event */
        if (!dequepaevt(er)) ievent(f, er); /* process event */
        er->handled = 1; /* set event is handled by default */
        (evtshan)(er); /* call master event handler */
        if (!er->handled && er->etype <= ami_etdsize) { /* send it to fanout */

            er->handled = 1; /* set event is handled by default */
            (*evthan[er->etype])(er); /* call event handler */

        }

    } while (er->handled);
    /* event not handled, return it to the caller */

}

void _pa_sendevent_ovr(ami_sendevent_t nfp, ami_sendevent_t* ofp)
    { *ofp = sendevent_vect; sendevent_vect = nfp; }
void ami_sendevent(FILE* f, ami_evtrec* er) { (*sendevent_vect)(f, er); }

static void sendevent_ivf(FILE* f, ami_evtrec* er)

{

    ami_evtrec ec; /* copy */

    memcpy(&ec, er, sizeof(ami_evtrec));
    ec.winid = 1; /* the screen */
    enquepaevt(&ec); /* send it */

}

void _pa_eventover_ovr(ami_eventover_t nfp, ami_eventover_t* ofp)
    { *ofp = eventover_vect; eventover_vect = nfp; }
void ami_eventover(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)
    { (*eventover_vect)(e, eh, oeh); }

static void eventover_ivf(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)

{

    if (e > ami_etdsize) error(evecaxe); /* cannot vector auxiliary event */
    *oeh = evthan[e]; /* save existing event handler */
    evthan[e] = eh; /* place new event handler */

}

void _pa_eventsover_ovr(ami_eventsover_t nfp, ami_eventsover_t* ofp)
    { *ofp = eventsover_vect; eventsover_vect = nfp; }
void ami_eventsover(ami_pevthan eh, ami_pevthan* oeh)
    { (*eventsover_vect)(eh, oeh); }

static void eventsover_ivf(ami_pevthan eh, ami_pevthan* oeh)

{

    *oeh = evtshan; /* save existing event handler */
    evtshan = eh; /* place new event handler */

}

void _pa_timer_ovr(ami_timer_t nfp, ami_timer_t* ofp)
    { *ofp = timer_vect; timer_vect = nfp; }
void ami_timer(FILE* f, ami_long i, ami_long t, ami_long r) { (*timer_vect)(f, i, t, r); }

static void timer_ivf(FILE* f, /* file to send event to */
                      ami_long  i, /* timer handle */
                      ami_long  t, /* number of tenth-milliseconds to run */
                      ami_long  r) /* timer is to rerun after completion */

{

    winptr win; /* windows record pointer */
    int    sid; /* system event */

    if (i < 1 || i > AMI_MAXTIM) error(einvhan); /* invalid timer handle */
    win = txt2win(f); /* get window from file */
    /* set system event */
    sid = system_event_addsetim(win->timers[i-1], t, r);
    win->timers[i-1] = sid;
    /* get system event entry */
    getsee(sid); /* allocate system event entry */
    sidtab[sid-1]->win = win; /* set window associated */
    sidtab[sid-1]->tim = i; /* set timer associated */

}

void _pa_killtimer_ovr(ami_killtimer_t nfp, ami_killtimer_t* ofp)
    { *ofp = killtimer_vect; killtimer_vect = nfp; }
void ami_killtimer(FILE* f, ami_long i) { (*killtimer_vect)(f, i); }

static void killtimer_ivf(FILE* f, /* file to kill timer on */
                          ami_long  i) /* handle of timer */

{

    winptr win; /* windows record pointer */

    if (i < 1 || i > AMI_MAXTIM) error(einvhan); /* invalid timer handle */
    win = txt2win(f); /* get window from file */
    if (!win->timers[i-1]) error(etimacc); /* no such timer */
    system_event_deasetim(win->timers[i-1]); /* deactivate timer */

}

void _pa_frametimer_ovr(ami_frametimer_t nfp, ami_frametimer_t* ofp)
    { *ofp = frametimer_vect; frametimer_vect = nfp; }
void ami_frametimer(FILE* f, ami_long e) { (*frametimer_vect)(f, e); }

static void frametimer_ivf(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */
    int    sid; /* system event */

    win = txt2win(f); /* get window from file */
    if (e) { /* set framing timer to run */

        /* Writes to the frame buffer show at once; there is no display
           beat to follow, so the frame timer is a plain 60 a second
           heartbeat */
        sid = system_event_addsetim(win->frmsev, 167, TRUE);
        win->frmsev = sid;
        /* get system event entry */
        getsee(sid); /* allocate system event entry */
        sidtab[sid-1]->win = win; /* set window associated */
        sidtab[sid-1]->frm = TRUE; /* set is the framing timer */
        win->frmrun = TRUE; /* set framing timer running */

    } else {

        system_event_deasetim(win->frmsev);
        win->frmrun = FALSE; /* set framing timer not running */

    }

}

void _pa_autohold_ovr(ami_autohold_t nfp, ami_autohold_t* ofp)
    { *ofp = autohold_vect; autohold_vect = nfp; }
void ami_autohold(ami_long e) { (*autohold_vect)(e); }

static void autohold_ivf(ami_long e)

{

    fautohold = e; /* set new state of autohold */

}

void _pa_mouse_ovr(ami_mouse_t nfp, ami_mouse_t* ofp)
    { *ofp = mouse_vect; mouse_vect = nfp; }
ami_long ami_mouse(FILE* f) { return ((*mouse_vect)(f)); }

static ami_long mouse_ivf(FILE* f)

{

    return (micefd >= 0); /* one mouse when the device opened */

}

void _pa_mousebutton_ovr(ami_mousebutton_t nfp, ami_mousebutton_t* ofp)
    { *ofp = mousebutton_vect; mousebutton_vect = nfp; }
ami_long ami_mousebutton(FILE* f, ami_long m) { return ((*mousebutton_vect)(f, m)); }

static ami_long mousebutton_ivf(FILE* f, ami_long m)

{

    if (micefd < 0) return (0); /* no mouse, no buttons */
    if (m != 1) error(einvhan); /* bad mouse number */

    return (3); /* the PS/2 stream carries three */

}

void _pa_joystick_ovr(ami_joystick_t nfp, ami_joystick_t* ofp)
    { *ofp = joystick_vect; joystick_vect = nfp; }
ami_long ami_joystick(FILE* f) { return ((*joystick_vect)(f)); }

static ami_long joystick_ivf(FILE* f)

{

    return (numjoy); /* return number of joysticks found */

}

void _pa_joybutton_ovr(ami_joybutton_t nfp, ami_joybutton_t* ofp)
    { *ofp = joybutton_vect; joybutton_vect = nfp; }
ami_long ami_joybutton(FILE* f, ami_long j) { return ((*joybutton_vect)(f, j)); }

static ami_long joybutton_ivf(FILE* f, ami_long j)

{

    if (j < 1 || j > numjoy) error(einvhan); /* bad joystick id */

    return (joytab[j-1]->button);

}

void _pa_joyaxis_ovr(ami_joyaxis_t nfp, ami_joyaxis_t* ofp)
    { *ofp = joyaxis_vect; joyaxis_vect = nfp; }
ami_long ami_joyaxis(FILE* f, ami_long j) { return ((*joyaxis_vect)(f, j)); }

static ami_long joyaxis_ivf(FILE* f, ami_long j)

{

    if (j < 1 || j > numjoy) error(einvhan); /* bad joystick id */

    return (joytab[j-1]->axis);

}

/*******************************************************************************

Window management and widgets

There is no window manager and no widget package on the frame buffer.
Each call errors as unimplemented; a management or widget package that
can carry them would override these vectors.

*******************************************************************************/

void _pa_openwin_ovr(ami_openwin_t nfp, ami_openwin_t* ofp)
    { *ofp = openwin_vect; openwin_vect = nfp; }
void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)
    { (*openwin_vect)(infile, outfile, parent, wid); }
static void openwin_ivf(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)
    { unimp("openwin"); }

void _pa_buffer_ovr(ami_buffer_t nfp, ami_buffer_t* ofp)
    { *ofp = buffer_vect; buffer_vect = nfp; }
void ami_buffer(FILE* f, ami_long e)
    { (*buffer_vect)(f, e); }
static void buffer_ivf(FILE* f, ami_long e)
    { unimp("buffer"); }

void _pa_sizbuf_ovr(ami_sizbuf_t nfp, ami_sizbuf_t* ofp)
    { *ofp = sizbuf_vect; sizbuf_vect = nfp; }
void ami_sizbuf(FILE* f, ami_long x, ami_long y)
    { (*sizbuf_vect)(f, x, y); }
static void sizbuf_ivf(FILE* f, ami_long x, ami_long y)
    { ami_sizbufg(f, x*thewin->charspace, y*thewin->linespace); }

void _pa_sizbufg_ovr(ami_sizbufg_t nfp, ami_sizbufg_t* ofp)
    { *ofp = sizbufg_vect; sizbufg_vect = nfp; }
void ami_sizbufg(FILE* f, ami_long x, ami_long y)
    { (*sizbufg_vect)(f, x, y); }

/* Resize the current update screen's buffer. Content in the intersection
   of old and new is kept; fresh area fills with the background. The
   screen sizes are per screen, which is what lets a window manager above
   use the screens as window backing stores. */
static void sizbufg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr  win; /* windows record pointer */
    scnptr  sc;  /* screen pointer */
    canvas* n;   /* the new buffer */
    int     i, j;
    uint32_t bg;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (x < 1) x = 1;
    if (y < 1) y = 1;
    if (x == sc->maxxg && y == sc->maxyg) return; /* nothing to do */
    n = newcanvas(x, y);
    if (!n) error(enomem);
    bg = BIT(sarev) & sc->attr? sc->fcrgb: sc->bcrgb;
    for (j = 0; j < y; j++)
        for (i = 0; i < x; i++)
            n->px[(size_t)j*x+i] =
                (i < sc->xbuf->w && j < sc->xbuf->h)?
                    sc->xbuf->px[(size_t)j*sc->xbuf->w+i]: bg;
    freecanvas(sc->xbuf);
    sc->xbuf = n;
    sc->maxxg = x; /* set new pixel dimensions */
    sc->maxyg = y;
    sc->maxx = x/win->charspace; /* and the character grid */
    sc->maxy = y/win->linespace;
    /* The resize hands back a refreshed drawing context, as the other
       backends do. They reset to their window's current global state;
       here a manager above may divide the one window into many, and
       the globals mix every window's settings -- the screen's own
       colors, attributes, font and modes ARE its window's current
       state, so those stand. What the desktops reset unconditionally
       resets here: the cursor home, the line width and style, and the
       default tabs at the new width. */
    sc->curx = 1; /* cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    sc->angle = LONG_MAX/4;
    sc->lwidth = 1; /* single pixel width */
    sc->lstyle = ami_lssolid;
    if (BIT(sarev) & sc->attr) { /* the context follows */

        sc->xcxt->bg = sc->fcrgb;
        sc->xcxt->fg = sc->bcrgb;

    } else {

        sc->xcxt->bg = sc->bcrgb;
        sc->xcxt->fg = sc->fcrgb;

    }
    sc->xcxt->lw = 1; sc->xcxt->lstyle = pd_linesolid;
    /* the default tabs at the new width */
    for (i = 0; i < MAXTAB; i++) sc->tab[i] = 0;
    {
        int t = 9, ti = 0;

        while (t < sc->maxx && ti < MAXTAB) {

            sc->tab[ti] = (t-1)*win->charspace+1;
            t = t+8;
            ti = ti+1;

        }
    }
    if (win->curupd == win->curdsp) {

        /* the displayed screen changed size: track the globals and
           repaint */
        win->gmaxxg = x > win->gmaxxg? win->gmaxxg: x;
        restore(win);

    }

}

void _pa_getsiz_ovr(ami_getsiz_t nfp, ami_getsiz_t* ofp)
    { *ofp = getsiz_vect; getsiz_vect = nfp; }
void ami_getsiz(FILE* f, ami_long* x, ami_long* y)
    { (*getsiz_vect)(f, x, y); }
static void getsiz_ivf(FILE* f, ami_long* x, ami_long* y)
    { unimp("getsiz"); }

void _pa_getsizg_ovr(ami_getsizg_t nfp, ami_getsizg_t* ofp)
    { *ofp = getsizg_vect; getsizg_vect = nfp; }
void ami_getsizg(FILE* f, ami_long* x, ami_long* y)
    { (*getsizg_vect)(f, x, y); }
static void getsizg_ivf(FILE* f, ami_long* x, ami_long* y)
    { unimp("getsizg"); }

void _pa_setsiz_ovr(ami_setsiz_t nfp, ami_setsiz_t* ofp)
    { *ofp = setsiz_vect; setsiz_vect = nfp; }
void ami_setsiz(FILE* f, ami_long x, ami_long y)
    { (*setsiz_vect)(f, x, y); }
static void setsiz_ivf(FILE* f, ami_long x, ami_long y)
    { unimp("setsiz"); }

void _pa_setsizg_ovr(ami_setsizg_t nfp, ami_setsizg_t* ofp)
    { *ofp = setsizg_vect; setsizg_vect = nfp; }
void ami_setsizg(FILE* f, ami_long x, ami_long y)
    { (*setsizg_vect)(f, x, y); }
static void setsizg_ivf(FILE* f, ami_long x, ami_long y)
    { unimp("setsizg"); }

void _pa_setpos_ovr(ami_setpos_t nfp, ami_setpos_t* ofp)
    { *ofp = setpos_vect; setpos_vect = nfp; }
void ami_setpos(FILE* f, ami_long x, ami_long y)
    { (*setpos_vect)(f, x, y); }
static void setpos_ivf(FILE* f, ami_long x, ami_long y)
    { unimp("setpos"); }

void _pa_setposg_ovr(ami_setposg_t nfp, ami_setposg_t* ofp)
    { *ofp = setposg_vect; setposg_vect = nfp; }
void ami_setposg(FILE* f, ami_long x, ami_long y)
    { (*setposg_vect)(f, x, y); }
static void setposg_ivf(FILE* f, ami_long x, ami_long y)
    { unimp("setposg"); }

void _pa_scnsiz_ovr(ami_scnsiz_t nfp, ami_scnsiz_t* ofp)
    { *ofp = scnsiz_vect; scnsiz_vect = nfp; }
void ami_scnsiz(FILE* f, ami_long* x, ami_long* y)
    { (*scnsiz_vect)(f, x, y); }
static void scnsiz_ivf(FILE* f, ami_long* x, ami_long* y)
    { unimp("scnsiz"); }

void _pa_scnsizg_ovr(ami_scnsizg_t nfp, ami_scnsizg_t* ofp)
    { *ofp = scnsizg_vect; scnsizg_vect = nfp; }
void ami_scnsizg(FILE* f, ami_long* x, ami_long*y)
    { (*scnsizg_vect)(f, x, y); }
static void scnsizg_ivf(FILE* f, ami_long* x, ami_long*y)
    { unimp("scnsizg"); }

void _pa_scncen_ovr(ami_scncen_t nfp, ami_scncen_t* ofp)
    { *ofp = scncen_vect; scncen_vect = nfp; }
void ami_scncen(FILE* f, ami_long* x, ami_long* y)
    { (*scncen_vect)(f, x, y); }
static void scncen_ivf(FILE* f, ami_long* x, ami_long* y)
    { unimp("scncen"); }

void _pa_scnceng_ovr(ami_scnceng_t nfp, ami_scnceng_t* ofp)
    { *ofp = scnceng_vect; scnceng_vect = nfp; }
void ami_scnceng(FILE* f, ami_long* x, ami_long* y)
    { (*scnceng_vect)(f, x, y); }
static void scnceng_ivf(FILE* f, ami_long* x, ami_long* y)
    { unimp("scnceng"); }

void _pa_winclient_ovr(ami_winclient_t nfp, ami_winclient_t* ofp)
    { *ofp = winclient_vect; winclient_vect = nfp; }
void ami_winclient(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms)
    { (*winclient_vect)(f, cx, cy, wx, wy, ms); }
static void winclient_ivf(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms)
    { unimp("winclient"); }

void _pa_winclientg_ovr(ami_winclientg_t nfp, ami_winclientg_t* ofp)
    { *ofp = winclientg_vect; winclientg_vect = nfp; }
void ami_winclientg(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms)
    { (*winclientg_vect)(f, cx, cy, wx, wy, ms); }
static void winclientg_ivf(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms)
    { unimp("winclientg"); }

void _pa_front_ovr(ami_front_t nfp, ami_front_t* ofp)
    { *ofp = front_vect; front_vect = nfp; }
void ami_front(FILE* f)
    { (*front_vect)(f); }
static void front_ivf(FILE* f)
    { unimp("front"); }

void _pa_back_ovr(ami_back_t nfp, ami_back_t* ofp)
    { *ofp = back_vect; back_vect = nfp; }
void ami_back(FILE* f)
    { (*back_vect)(f); }
static void back_ivf(FILE* f)
    { unimp("back"); }

void _pa_frame_ovr(ami_frame_t nfp, ami_frame_t* ofp)
    { *ofp = frame_vect; frame_vect = nfp; }
void ami_frame(FILE* f, ami_long e)
    { (*frame_vect)(f, e); }
static void frame_ivf(FILE* f, ami_long e)
    { unimp("frame"); }

void _pa_sizable_ovr(ami_sizable_t nfp, ami_sizable_t* ofp)
    { *ofp = sizable_vect; sizable_vect = nfp; }
void ami_sizable(FILE* f, ami_long e)
    { (*sizable_vect)(f, e); }
static void sizable_ivf(FILE* f, ami_long e)
    { unimp("sizable"); }

void _pa_sysbar_ovr(ami_sysbar_t nfp, ami_sysbar_t* ofp)
    { *ofp = sysbar_vect; sysbar_vect = nfp; }
void ami_sysbar(FILE* f, ami_long e)
    { (*sysbar_vect)(f, e); }
static void sysbar_ivf(FILE* f, ami_long e)
    { unimp("sysbar"); }

void _pa_menu_ovr(ami_menu_t nfp, ami_menu_t* ofp)
    { *ofp = menu_vect; menu_vect = nfp; }
void ami_menu(FILE* f, ami_menuptr m)
    { (*menu_vect)(f, m); }
static void menu_ivf(FILE* f, ami_menuptr m)
    { unimp("menu"); }

void _pa_menuena_ovr(ami_menuena_t nfp, ami_menuena_t* ofp)
    { *ofp = menuena_vect; menuena_vect = nfp; }
void ami_menuena(FILE* f, ami_long id, ami_long onoff)
    { (*menuena_vect)(f, id, onoff); }
static void menuena_ivf(FILE* f, ami_long id, ami_long onoff)
    { unimp("menuena"); }

void _pa_menusel_ovr(ami_menusel_t nfp, ami_menusel_t* ofp)
    { *ofp = menusel_vect; menusel_vect = nfp; }
void ami_menusel(FILE* f, ami_long id, ami_long select)
    { (*menusel_vect)(f, id, select); }
static void menusel_ivf(FILE* f, ami_long id, ami_long select)
    { unimp("menusel"); }

void _pa_stdmenu_ovr(ami_stdmenu_t nfp, ami_stdmenu_t* ofp)
    { *ofp = stdmenu_vect; stdmenu_vect = nfp; }
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
    { (*stdmenu_vect)(sms, sm, pm); }
static void stdmenu_ivf(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
    { unimp("stdmenu"); }

void _pa_getwinid_ovr(ami_getwinid_t nfp, ami_getwinid_t* ofp)
    { *ofp = getwinid_vect; getwinid_vect = nfp; }
ami_long ami_getwinid(void)
    { return (*getwinid_vect)(); }
static ami_long getwinid_ivf(void)
    { unimp("getwinid"); return 0; }

void _pa_focus_ovr(ami_focus_t nfp, ami_focus_t* ofp)
    { *ofp = focus_vect; focus_vect = nfp; }
void ami_focus(FILE* f)
    { (*focus_vect)(f); }
static void focus_ivf(FILE* f)
    { unimp("focus"); }

void _pa_getwigid_ovr(ami_getwigid_t nfp, ami_getwigid_t* ofp)
    { *ofp = getwigid_vect; getwigid_vect = nfp; }
ami_long ami_getwigid(FILE* f)
    { return (*getwigid_vect)(f); }
static ami_long getwigid_ivf(FILE* f)
    { unimp("getwigid"); return 0; }

void _pa_killwidget_ovr(ami_killwidget_t nfp, ami_killwidget_t* ofp)
    { *ofp = killwidget_vect; killwidget_vect = nfp; }
void ami_killwidget(FILE* f, ami_long id)
    { (*killwidget_vect)(f, id); }
static void killwidget_ivf(FILE* f, ami_long id)
    { unimp("killwidget"); }

void _pa_selectwidget_ovr(ami_selectwidget_t nfp, ami_selectwidget_t* ofp)
    { *ofp = selectwidget_vect; selectwidget_vect = nfp; }
void ami_selectwidget(FILE* f, ami_long id, ami_long e)
    { (*selectwidget_vect)(f, id, e); }
static void selectwidget_ivf(FILE* f, ami_long id, ami_long e)
    { unimp("selectwidget"); }

void _pa_enablewidget_ovr(ami_enablewidget_t nfp, ami_enablewidget_t* ofp)
    { *ofp = enablewidget_vect; enablewidget_vect = nfp; }
void ami_enablewidget(FILE* f, ami_long id, ami_long e)
    { (*enablewidget_vect)(f, id, e); }
static void enablewidget_ivf(FILE* f, ami_long id, ami_long e)
    { unimp("enablewidget"); }

void _pa_getwidgettext_ovr(ami_getwidgettext_t nfp, ami_getwidgettext_t* ofp)
    { *ofp = getwidgettext_vect; getwidgettext_vect = nfp; }
void ami_getwidgettext(FILE* f, ami_long id, char* s, ami_long sl)
    { (*getwidgettext_vect)(f, id, s, sl); }
static void getwidgettext_ivf(FILE* f, ami_long id, char* s, ami_long sl)
    { unimp("getwidgettext"); }

void _pa_putwidgettext_ovr(ami_putwidgettext_t nfp, ami_putwidgettext_t* ofp)
    { *ofp = putwidgettext_vect; putwidgettext_vect = nfp; }
void ami_putwidgettext(FILE* f, ami_long id, char* s)
    { (*putwidgettext_vect)(f, id, s); }
static void putwidgettext_ivf(FILE* f, ami_long id, char* s)
    { unimp("putwidgettext"); }

void _pa_sizwidget_ovr(ami_sizwidget_t nfp, ami_sizwidget_t* ofp)
    { *ofp = sizwidget_vect; sizwidget_vect = nfp; }
void ami_sizwidget(FILE* f, ami_long id, ami_long x, ami_long y)
    { (*sizwidget_vect)(f, id, x, y); }
static void sizwidget_ivf(FILE* f, ami_long id, ami_long x, ami_long y)
    { unimp("sizwidget"); }

void _pa_sizwidgetg_ovr(ami_sizwidgetg_t nfp, ami_sizwidgetg_t* ofp)
    { *ofp = sizwidgetg_vect; sizwidgetg_vect = nfp; }
void ami_sizwidgetg(FILE* f, ami_long id, ami_long x, ami_long y)
    { (*sizwidgetg_vect)(f, id, x, y); }
static void sizwidgetg_ivf(FILE* f, ami_long id, ami_long x, ami_long y)
    { unimp("sizwidgetg"); }

void _pa_poswidget_ovr(ami_poswidget_t nfp, ami_poswidget_t* ofp)
    { *ofp = poswidget_vect; poswidget_vect = nfp; }
void ami_poswidget(FILE* f, ami_long id, ami_long x, ami_long y)
    { (*poswidget_vect)(f, id, x, y); }
static void poswidget_ivf(FILE* f, ami_long id, ami_long x, ami_long y)
    { unimp("poswidget"); }

void _pa_poswidgetg_ovr(ami_poswidgetg_t nfp, ami_poswidgetg_t* ofp)
    { *ofp = poswidgetg_vect; poswidgetg_vect = nfp; }
void ami_poswidgetg(FILE* f, ami_long id, ami_long x, ami_long y)
    { (*poswidgetg_vect)(f, id, x, y); }
static void poswidgetg_ivf(FILE* f, ami_long id, ami_long x, ami_long y)
    { unimp("poswidgetg"); }

void _pa_backwidget_ovr(ami_backwidget_t nfp, ami_backwidget_t* ofp)
    { *ofp = backwidget_vect; backwidget_vect = nfp; }
void ami_backwidget(FILE* f, ami_long id)
    { (*backwidget_vect)(f, id); }
static void backwidget_ivf(FILE* f, ami_long id)
    { unimp("backwidget"); }

void _pa_frontwidget_ovr(ami_frontwidget_t nfp, ami_frontwidget_t* ofp)
    { *ofp = frontwidget_vect; frontwidget_vect = nfp; }
void ami_frontwidget(FILE* f, ami_long id)
    { (*frontwidget_vect)(f, id); }
static void frontwidget_ivf(FILE* f, ami_long id)
    { unimp("frontwidget"); }

void _pa_focuswidget_ovr(ami_focuswidget_t nfp, ami_focuswidget_t* ofp)
    { *ofp = focuswidget_vect; focuswidget_vect = nfp; }
void ami_focuswidget(FILE* f, ami_long id)
    { (*focuswidget_vect)(f, id); }
static void focuswidget_ivf(FILE* f, ami_long id)
    { unimp("focuswidget"); }

void _pa_buttonsiz_ovr(ami_buttonsiz_t nfp, ami_buttonsiz_t* ofp)
    { *ofp = buttonsiz_vect; buttonsiz_vect = nfp; }
void ami_buttonsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*buttonsiz_vect)(f, s, w, h); }
static void buttonsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("buttonsiz"); }

void _pa_buttonsizg_ovr(ami_buttonsizg_t nfp, ami_buttonsizg_t* ofp)
    { *ofp = buttonsizg_vect; buttonsizg_vect = nfp; }
void ami_buttonsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*buttonsizg_vect)(f, s, w, h); }
static void buttonsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("buttonsizg"); }

void _pa_button_ovr(ami_button_t nfp, ami_button_t* ofp)
    { *ofp = button_vect; button_vect = nfp; }
void ami_button(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*button_vect)(f, x1, y1, x2, y2, s, id); }
static void button_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("button"); }

void _pa_buttong_ovr(ami_buttong_t nfp, ami_buttong_t* ofp)
    { *ofp = buttong_vect; buttong_vect = nfp; }
void ami_buttong(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*buttong_vect)(f, x1, y1, x2, y2, s, id); }
static void buttong_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("buttong"); }

void _pa_checkboxsiz_ovr(ami_checkboxsiz_t nfp, ami_checkboxsiz_t* ofp)
    { *ofp = checkboxsiz_vect; checkboxsiz_vect = nfp; }
void ami_checkboxsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*checkboxsiz_vect)(f, s, w, h); }
static void checkboxsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("checkboxsiz"); }

void _pa_checkboxsizg_ovr(ami_checkboxsizg_t nfp, ami_checkboxsizg_t* ofp)
    { *ofp = checkboxsizg_vect; checkboxsizg_vect = nfp; }
void ami_checkboxsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*checkboxsizg_vect)(f, s, w, h); }
static void checkboxsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("checkboxsizg"); }

void _pa_checkbox_ovr(ami_checkbox_t nfp, ami_checkbox_t* ofp)
    { *ofp = checkbox_vect; checkbox_vect = nfp; }
void ami_checkbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*checkbox_vect)(f, x1, y1, x2, y2, s, id); }
static void checkbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("checkbox"); }

void _pa_checkboxg_ovr(ami_checkboxg_t nfp, ami_checkboxg_t* ofp)
    { *ofp = checkboxg_vect; checkboxg_vect = nfp; }
void ami_checkboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*checkboxg_vect)(f, x1, y1, x2, y2, s, id); }
static void checkboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("checkboxg"); }

void _pa_radiobuttonsiz_ovr(ami_radiobuttonsiz_t nfp, ami_radiobuttonsiz_t* ofp)
    { *ofp = radiobuttonsiz_vect; radiobuttonsiz_vect = nfp; }
void ami_radiobuttonsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*radiobuttonsiz_vect)(f, s, w, h); }
static void radiobuttonsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("radiobuttonsiz"); }

void _pa_radiobuttonsizg_ovr(ami_radiobuttonsizg_t nfp, ami_radiobuttonsizg_t* ofp)
    { *ofp = radiobuttonsizg_vect; radiobuttonsizg_vect = nfp; }
void ami_radiobuttonsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*radiobuttonsizg_vect)(f, s, w, h); }
static void radiobuttonsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("radiobuttonsizg"); }

void _pa_radiobutton_ovr(ami_radiobutton_t nfp, ami_radiobutton_t* ofp)
    { *ofp = radiobutton_vect; radiobutton_vect = nfp; }
void ami_radiobutton(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*radiobutton_vect)(f, x1, y1, x2, y2, s, id); }
static void radiobutton_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("radiobutton"); }

void _pa_radiobuttong_ovr(ami_radiobuttong_t nfp, ami_radiobuttong_t* ofp)
    { *ofp = radiobuttong_vect; radiobuttong_vect = nfp; }
void ami_radiobuttong(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*radiobuttong_vect)(f, x1, y1, x2, y2, s, id); }
static void radiobuttong_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("radiobuttong"); }

void _pa_groupsizg_ovr(ami_groupsizg_t nfp, ami_groupsizg_t* ofp)
    { *ofp = groupsizg_vect; groupsizg_vect = nfp; }
void ami_groupsizg(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { (*groupsizg_vect)(f, s, cw, ch, w, h, ox, oy); }
static void groupsizg_ivf(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { unimp("groupsizg"); }

void _pa_groupsiz_ovr(ami_groupsiz_t nfp, ami_groupsiz_t* ofp)
    { *ofp = groupsiz_vect; groupsiz_vect = nfp; }
void ami_groupsiz(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { (*groupsiz_vect)(f, s, cw, ch, w, h, ox, oy); }
static void groupsiz_ivf(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { unimp("groupsiz"); }

void _pa_group_ovr(ami_group_t nfp, ami_group_t* ofp)
    { *ofp = group_vect; group_vect = nfp; }
void ami_group(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*group_vect)(f, x1, y1, x2, y2, s, id); }
static void group_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("group"); }

void _pa_groupg_ovr(ami_groupg_t nfp, ami_groupg_t* ofp)
    { *ofp = groupg_vect; groupg_vect = nfp; }
void ami_groupg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { (*groupg_vect)(f, x1, y1, x2, y2, s, id); }
static void groupg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { unimp("groupg"); }

void _pa_background_ovr(ami_background_t nfp, ami_background_t* ofp)
    { *ofp = background_vect; background_vect = nfp; }
void ami_background(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*background_vect)(f, x1, y1, x2, y2, id); }
static void background_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("background"); }

void _pa_backgroundg_ovr(ami_backgroundg_t nfp, ami_backgroundg_t* ofp)
    { *ofp = backgroundg_vect; backgroundg_vect = nfp; }
void ami_backgroundg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*backgroundg_vect)(f, x1, y1, x2, y2, id); }
static void backgroundg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("backgroundg"); }

void _pa_scrollvertsizg_ovr(ami_scrollvertsizg_t nfp, ami_scrollvertsizg_t* ofp)
    { *ofp = scrollvertsizg_vect; scrollvertsizg_vect = nfp; }
void ami_scrollvertsizg(FILE* f, ami_long* w, ami_long* h)
    { (*scrollvertsizg_vect)(f, w, h); }
static void scrollvertsizg_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("scrollvertsizg"); }

void _pa_scrollvertsiz_ovr(ami_scrollvertsiz_t nfp, ami_scrollvertsiz_t* ofp)
    { *ofp = scrollvertsiz_vect; scrollvertsiz_vect = nfp; }
void ami_scrollvertsiz(FILE* f, ami_long* w, ami_long* h)
    { (*scrollvertsiz_vect)(f, w, h); }
static void scrollvertsiz_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("scrollvertsiz"); }

void _pa_scrollvert_ovr(ami_scrollvert_t nfp, ami_scrollvert_t* ofp)
    { *ofp = scrollvert_vect; scrollvert_vect = nfp; }
void ami_scrollvert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*scrollvert_vect)(f, x1, y1, x2, y2, id); }
static void scrollvert_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("scrollvert"); }

void _pa_scrollvertg_ovr(ami_scrollvertg_t nfp, ami_scrollvertg_t* ofp)
    { *ofp = scrollvertg_vect; scrollvertg_vect = nfp; }
void ami_scrollvertg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*scrollvertg_vect)(f, x1, y1, x2, y2, id); }
static void scrollvertg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("scrollvertg"); }

void _pa_scrollhorizsizg_ovr(ami_scrollhorizsizg_t nfp, ami_scrollhorizsizg_t* ofp)
    { *ofp = scrollhorizsizg_vect; scrollhorizsizg_vect = nfp; }
void ami_scrollhorizsizg(FILE* f, ami_long* w, ami_long* h)
    { (*scrollhorizsizg_vect)(f, w, h); }
static void scrollhorizsizg_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("scrollhorizsizg"); }

void _pa_scrollhorizsiz_ovr(ami_scrollhorizsiz_t nfp, ami_scrollhorizsiz_t* ofp)
    { *ofp = scrollhorizsiz_vect; scrollhorizsiz_vect = nfp; }
void ami_scrollhorizsiz(FILE* f, ami_long* w, ami_long* h)
    { (*scrollhorizsiz_vect)(f, w, h); }
static void scrollhorizsiz_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("scrollhorizsiz"); }

void _pa_scrollhoriz_ovr(ami_scrollhoriz_t nfp, ami_scrollhoriz_t* ofp)
    { *ofp = scrollhoriz_vect; scrollhoriz_vect = nfp; }
void ami_scrollhoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*scrollhoriz_vect)(f, x1, y1, x2, y2, id); }
static void scrollhoriz_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("scrollhoriz"); }

void _pa_scrollhorizg_ovr(ami_scrollhorizg_t nfp, ami_scrollhorizg_t* ofp)
    { *ofp = scrollhorizg_vect; scrollhorizg_vect = nfp; }
void ami_scrollhorizg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*scrollhorizg_vect)(f, x1, y1, x2, y2, id); }
static void scrollhorizg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("scrollhorizg"); }

void _pa_scrollpos_ovr(ami_scrollpos_t nfp, ami_scrollpos_t* ofp)
    { *ofp = scrollpos_vect; scrollpos_vect = nfp; }
void ami_scrollpos(FILE* f, ami_long id, ami_long r)
    { (*scrollpos_vect)(f, id, r); }
static void scrollpos_ivf(FILE* f, ami_long id, ami_long r)
    { unimp("scrollpos"); }

void _pa_scrollsiz_ovr(ami_scrollsiz_t nfp, ami_scrollsiz_t* ofp)
    { *ofp = scrollsiz_vect; scrollsiz_vect = nfp; }
void ami_scrollsiz(FILE* f, ami_long id, ami_long r)
    { (*scrollsiz_vect)(f, id, r); }
static void scrollsiz_ivf(FILE* f, ami_long id, ami_long r)
    { unimp("scrollsiz"); }

void _pa_numselboxsizg_ovr(ami_numselboxsizg_t nfp, ami_numselboxsizg_t* ofp)
    { *ofp = numselboxsizg_vect; numselboxsizg_vect = nfp; }
void ami_numselboxsizg(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)
    { (*numselboxsizg_vect)(f, l, u, w, h); }
static void numselboxsizg_ivf(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)
    { unimp("numselboxsizg"); }

void _pa_numselboxsiz_ovr(ami_numselboxsiz_t nfp, ami_numselboxsiz_t* ofp)
    { *ofp = numselboxsiz_vect; numselboxsiz_vect = nfp; }
void ami_numselboxsiz(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)
    { (*numselboxsiz_vect)(f, l, u, w, h); }
static void numselboxsiz_ivf(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)
    { unimp("numselboxsiz"); }

void _pa_numselbox_ovr(ami_numselbox_t nfp, ami_numselbox_t* ofp)
    { *ofp = numselbox_vect; numselbox_vect = nfp; }
void ami_numselbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id)
    { (*numselbox_vect)(f, x1, y1, x2, y2, l, u, id); }
static void numselbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id)
    { unimp("numselbox"); }

void _pa_numselboxg_ovr(ami_numselboxg_t nfp, ami_numselboxg_t* ofp)
    { *ofp = numselboxg_vect; numselboxg_vect = nfp; }
void ami_numselboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id)
    { (*numselboxg_vect)(f, x1, y1, x2, y2, l, u, id); }
static void numselboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id)
    { unimp("numselboxg"); }

void _pa_editboxsizg_ovr(ami_editboxsizg_t nfp, ami_editboxsizg_t* ofp)
    { *ofp = editboxsizg_vect; editboxsizg_vect = nfp; }
void ami_editboxsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*editboxsizg_vect)(f, s, w, h); }
static void editboxsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("editboxsizg"); }

void _pa_editboxsiz_ovr(ami_editboxsiz_t nfp, ami_editboxsiz_t* ofp)
    { *ofp = editboxsiz_vect; editboxsiz_vect = nfp; }
void ami_editboxsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { (*editboxsiz_vect)(f, s, w, h); }
static void editboxsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
    { unimp("editboxsiz"); }

void _pa_editbox_ovr(ami_editbox_t nfp, ami_editbox_t* ofp)
    { *ofp = editbox_vect; editbox_vect = nfp; }
void ami_editbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*editbox_vect)(f, x1, y1, x2, y2, id); }
static void editbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("editbox"); }

void _pa_editboxg_ovr(ami_editboxg_t nfp, ami_editboxg_t* ofp)
    { *ofp = editboxg_vect; editboxg_vect = nfp; }
void ami_editboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*editboxg_vect)(f, x1, y1, x2, y2, id); }
static void editboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("editboxg"); }

void _pa_progbarsizg_ovr(ami_progbarsizg_t nfp, ami_progbarsizg_t* ofp)
    { *ofp = progbarsizg_vect; progbarsizg_vect = nfp; }
void ami_progbarsizg(FILE* f, ami_long* w, ami_long* h)
    { (*progbarsizg_vect)(f, w, h); }
static void progbarsizg_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("progbarsizg"); }

void _pa_progbarsiz_ovr(ami_progbarsiz_t nfp, ami_progbarsiz_t* ofp)
    { *ofp = progbarsiz_vect; progbarsiz_vect = nfp; }
void ami_progbarsiz(FILE* f, ami_long* w, ami_long* h)
    { (*progbarsiz_vect)(f, w, h); }
static void progbarsiz_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("progbarsiz"); }

void _pa_progbar_ovr(ami_progbar_t nfp, ami_progbar_t* ofp)
    { *ofp = progbar_vect; progbar_vect = nfp; }
void ami_progbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*progbar_vect)(f, x1, y1, x2, y2, id); }
static void progbar_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("progbar"); }

void _pa_progbarg_ovr(ami_progbarg_t nfp, ami_progbarg_t* ofp)
    { *ofp = progbarg_vect; progbarg_vect = nfp; }
void ami_progbarg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { (*progbarg_vect)(f, x1, y1, x2, y2, id); }
static void progbarg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { unimp("progbarg"); }

void _pa_progbarpos_ovr(ami_progbarpos_t nfp, ami_progbarpos_t* ofp)
    { *ofp = progbarpos_vect; progbarpos_vect = nfp; }
void ami_progbarpos(FILE* f, ami_long id, ami_long pos)
    { (*progbarpos_vect)(f, id, pos); }
static void progbarpos_ivf(FILE* f, ami_long id, ami_long pos)
    { unimp("progbarpos"); }

void _pa_listboxsizg_ovr(ami_listboxsizg_t nfp, ami_listboxsizg_t* ofp)
    { *ofp = listboxsizg_vect; listboxsizg_vect = nfp; }
void ami_listboxsizg(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
    { (*listboxsizg_vect)(f, sp, w, h); }
static void listboxsizg_ivf(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
    { unimp("listboxsizg"); }

void _pa_listboxsiz_ovr(ami_listboxsiz_t nfp, ami_listboxsiz_t* ofp)
    { *ofp = listboxsiz_vect; listboxsiz_vect = nfp; }
void ami_listboxsiz(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
    { (*listboxsiz_vect)(f, sp, w, h); }
static void listboxsiz_ivf(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
    { unimp("listboxsiz"); }

void _pa_listbox_ovr(ami_listbox_t nfp, ami_listbox_t* ofp)
    { *ofp = listbox_vect; listbox_vect = nfp; }
void ami_listbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { (*listbox_vect)(f, x1, y1, x2, y2, sp, id); }
static void listbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { unimp("listbox"); }

void _pa_listboxg_ovr(ami_listboxg_t nfp, ami_listboxg_t* ofp)
    { *ofp = listboxg_vect; listboxg_vect = nfp; }
void ami_listboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { (*listboxg_vect)(f, x1, y1, x2, y2, sp, id); }
static void listboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { unimp("listboxg"); }

void _pa_dropboxsizg_ovr(ami_dropboxsizg_t nfp, ami_dropboxsizg_t* ofp)
    { *ofp = dropboxsizg_vect; dropboxsizg_vect = nfp; }
void ami_dropboxsizg(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { (*dropboxsizg_vect)(f, sp, cw, ch, ow, oh); }
static void dropboxsizg_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { unimp("dropboxsizg"); }

void _pa_dropboxsiz_ovr(ami_dropboxsiz_t nfp, ami_dropboxsiz_t* ofp)
    { *ofp = dropboxsiz_vect; dropboxsiz_vect = nfp; }
void ami_dropboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { (*dropboxsiz_vect)(f, sp, cw, ch, ow, oh); }
static void dropboxsiz_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { unimp("dropboxsiz"); }

void _pa_dropbox_ovr(ami_dropbox_t nfp, ami_dropbox_t* ofp)
    { *ofp = dropbox_vect; dropbox_vect = nfp; }
void ami_dropbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { (*dropbox_vect)(f, x1, y1, x2, y2, sp, id); }
static void dropbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { unimp("dropbox"); }

void _pa_dropboxg_ovr(ami_dropboxg_t nfp, ami_dropboxg_t* ofp)
    { *ofp = dropboxg_vect; dropboxg_vect = nfp; }
void ami_dropboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { (*dropboxg_vect)(f, x1, y1, x2, y2, sp, id); }
static void dropboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { unimp("dropboxg"); }

void _pa_dropeditboxsizg_ovr(ami_dropeditboxsizg_t nfp, ami_dropeditboxsizg_t* ofp)
    { *ofp = dropeditboxsizg_vect; dropeditboxsizg_vect = nfp; }
void ami_dropeditboxsizg(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { (*dropeditboxsizg_vect)(f, sp, cw, ch, ow, oh); }
static void dropeditboxsizg_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { unimp("dropeditboxsizg"); }

void _pa_dropeditboxsiz_ovr(ami_dropeditboxsiz_t nfp, ami_dropeditboxsiz_t* ofp)
    { *ofp = dropeditboxsiz_vect; dropeditboxsiz_vect = nfp; }
void ami_dropeditboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { (*dropeditboxsiz_vect)(f, sp, cw, ch, ow, oh); }
static void dropeditboxsiz_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
    { unimp("dropeditboxsiz"); }

void _pa_dropeditbox_ovr(ami_dropeditbox_t nfp, ami_dropeditbox_t* ofp)
    { *ofp = dropeditbox_vect; dropeditbox_vect = nfp; }
void ami_dropeditbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { (*dropeditbox_vect)(f, x1, y1, x2, y2, sp, id); }
static void dropeditbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { unimp("dropeditbox"); }

void _pa_dropeditboxg_ovr(ami_dropeditboxg_t nfp, ami_dropeditboxg_t* ofp)
    { *ofp = dropeditboxg_vect; dropeditboxg_vect = nfp; }
void ami_dropeditboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { (*dropeditboxg_vect)(f, x1, y1, x2, y2, sp, id); }
static void dropeditboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
    { unimp("dropeditboxg"); }

void _pa_slidehorizsizg_ovr(ami_slidehorizsizg_t nfp, ami_slidehorizsizg_t* ofp)
    { *ofp = slidehorizsizg_vect; slidehorizsizg_vect = nfp; }
void ami_slidehorizsizg(FILE* f, ami_long* w, ami_long* h)
    { (*slidehorizsizg_vect)(f, w, h); }
static void slidehorizsizg_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("slidehorizsizg"); }

void _pa_slidehorizsiz_ovr(ami_slidehorizsiz_t nfp, ami_slidehorizsiz_t* ofp)
    { *ofp = slidehorizsiz_vect; slidehorizsiz_vect = nfp; }
void ami_slidehorizsiz(FILE* f, ami_long* w, ami_long* h)
    { (*slidehorizsiz_vect)(f, w, h); }
static void slidehorizsiz_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("slidehorizsiz"); }

void _pa_slidehoriz_ovr(ami_slidehoriz_t nfp, ami_slidehoriz_t* ofp)
    { *ofp = slidehoriz_vect; slidehoriz_vect = nfp; }
void ami_slidehoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { (*slidehoriz_vect)(f, x1, y1, x2, y2, mark, id); }
static void slidehoriz_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { unimp("slidehoriz"); }

void _pa_slidehorizg_ovr(ami_slidehorizg_t nfp, ami_slidehorizg_t* ofp)
    { *ofp = slidehorizg_vect; slidehorizg_vect = nfp; }
void ami_slidehorizg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { (*slidehorizg_vect)(f, x1, y1, x2, y2, mark, id); }
static void slidehorizg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { unimp("slidehorizg"); }

void _pa_slidevertsizg_ovr(ami_slidevertsizg_t nfp, ami_slidevertsizg_t* ofp)
    { *ofp = slidevertsizg_vect; slidevertsizg_vect = nfp; }
void ami_slidevertsizg(FILE* f, ami_long* w, ami_long* h)
    { (*slidevertsizg_vect)(f, w, h); }
static void slidevertsizg_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("slidevertsizg"); }

void _pa_slidevertsiz_ovr(ami_slidevertsiz_t nfp, ami_slidevertsiz_t* ofp)
    { *ofp = slidevertsiz_vect; slidevertsiz_vect = nfp; }
void ami_slidevertsiz(FILE* f, ami_long* w, ami_long* h)
    { (*slidevertsiz_vect)(f, w, h); }
static void slidevertsiz_ivf(FILE* f, ami_long* w, ami_long* h)
    { unimp("slidevertsiz"); }

void _pa_slidevert_ovr(ami_slidevert_t nfp, ami_slidevert_t* ofp)
    { *ofp = slidevert_vect; slidevert_vect = nfp; }
void ami_slidevert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { (*slidevert_vect)(f, x1, y1, x2, y2, mark, id); }
static void slidevert_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { unimp("slidevert"); }

void _pa_slidevertg_ovr(ami_slidevertg_t nfp, ami_slidevertg_t* ofp)
    { *ofp = slidevertg_vect; slidevertg_vect = nfp; }
void ami_slidevertg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { (*slidevertg_vect)(f, x1, y1, x2, y2, mark, id); }
static void slidevertg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
    { unimp("slidevertg"); }

void _pa_tabbarsizg_ovr(ami_tabbarsizg_t nfp, ami_tabbarsizg_t* ofp)
    { *ofp = tabbarsizg_vect; tabbarsizg_vect = nfp; }
void ami_tabbarsizg(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { (*tabbarsizg_vect)(f, sp, tor, cw, ch, w, h, ox, oy); }
static void tabbarsizg_ivf(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { unimp("tabbarsizg"); }

void _pa_tabbarsiz_ovr(ami_tabbarsiz_t nfp, ami_tabbarsiz_t* ofp)
    { *ofp = tabbarsiz_vect; tabbarsiz_vect = nfp; }
void ami_tabbarsiz(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { (*tabbarsiz_vect)(f, sp, tor, cw, ch, w, h, ox, oy); }
static void tabbarsiz_ivf(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
    { unimp("tabbarsiz"); }

void _pa_tabbarclientg_ovr(ami_tabbarclientg_t nfp, ami_tabbarclientg_t* ofp)
    { *ofp = tabbarclientg_vect; tabbarclientg_vect = nfp; }
void ami_tabbarclientg(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy)
    { (*tabbarclientg_vect)(f, tor, w, h, cw, ch, ox, oy); }
static void tabbarclientg_ivf(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy)
    { unimp("tabbarclientg"); }

void _pa_tabbarclient_ovr(ami_tabbarclient_t nfp, ami_tabbarclient_t* ofp)
    { *ofp = tabbarclient_vect; tabbarclient_vect = nfp; }
void ami_tabbarclient(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy)
    { (*tabbarclient_vect)(f, tor, w, h, cw, ch, ox, oy); }
static void tabbarclient_ivf(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy)
    { unimp("tabbarclient"); }

void _pa_tabbar_ovr(ami_tabbar_t nfp, ami_tabbar_t* ofp)
    { *ofp = tabbar_vect; tabbar_vect = nfp; }
void ami_tabbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id)
    { (*tabbar_vect)(f, x1, y1, x2, y2, sp, tor, id); }
static void tabbar_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id)
    { unimp("tabbar"); }

void _pa_tabbarg_ovr(ami_tabbarg_t nfp, ami_tabbarg_t* ofp)
    { *ofp = tabbarg_vect; tabbarg_vect = nfp; }
void ami_tabbarg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id)
    { (*tabbarg_vect)(f, x1, y1, x2, y2, sp, tor, id); }
static void tabbarg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id)
    { unimp("tabbarg"); }

void _pa_tabsel_ovr(ami_tabsel_t nfp, ami_tabsel_t* ofp)
    { *ofp = tabsel_vect; tabsel_vect = nfp; }
void ami_tabsel(FILE* f, ami_long id, ami_long tn)
    { (*tabsel_vect)(f, id, tn); }
static void tabsel_ivf(FILE* f, ami_long id, ami_long tn)
    { unimp("tabsel"); }

void _pa_alert_ovr(ami_alert_t nfp, ami_alert_t* ofp)
    { *ofp = alert_vect; alert_vect = nfp; }
void ami_alert(char* title, char* message)
    { (*alert_vect)(title, message); }
static void alert_ivf(char* title, char* message)
    { unimp("alert"); }

void _pa_querycolor_ovr(ami_querycolor_t nfp, ami_querycolor_t* ofp)
    { *ofp = querycolor_vect; querycolor_vect = nfp; }
void ami_querycolor(ami_long* r, ami_long* g, ami_long* b)
    { (*querycolor_vect)(r, g, b); }
static void querycolor_ivf(ami_long* r, ami_long* g, ami_long* b)
    { unimp("querycolor"); }

void _pa_queryopen_ovr(ami_queryopen_t nfp, ami_queryopen_t* ofp)
    { *ofp = queryopen_vect; queryopen_vect = nfp; }
void ami_queryopen(char* s, ami_long sl)
    { (*queryopen_vect)(s, sl); }
static void queryopen_ivf(char* s, ami_long sl)
    { unimp("queryopen"); }

void _pa_querysave_ovr(ami_querysave_t nfp, ami_querysave_t* ofp)
    { *ofp = querysave_vect; querysave_vect = nfp; }
void ami_querysave(char* s, ami_long sl)
    { (*querysave_vect)(s, sl); }
static void querysave_ivf(char* s, ami_long sl)
    { unimp("querysave"); }

void _pa_queryfind_ovr(ami_queryfind_t nfp, ami_queryfind_t* ofp)
    { *ofp = queryfind_vect; queryfind_vect = nfp; }
void ami_queryfind(char* s, ami_long sl, ami_qfnopts* opt)
    { (*queryfind_vect)(s, sl, opt); }
static void queryfind_ivf(char* s, ami_long sl, ami_qfnopts* opt)
    { unimp("queryfind"); }

void _pa_queryfindrep_ovr(ami_queryfindrep_t nfp, ami_queryfindrep_t* ofp)
    { *ofp = queryfindrep_vect; queryfindrep_vect = nfp; }
void ami_queryfindrep(char* s, ami_long sl, char* r, ami_long rl, ami_qfropts* opt)
    { (*queryfindrep_vect)(s, sl, r, rl, opt); }
static void queryfindrep_ivf(char* s, ami_long sl, char* r, ami_long rl, ami_qfropts* opt)
    { unimp("queryfindrep"); }

void _pa_queryfont_ovr(ami_queryfont_t nfp, ami_queryfont_t* ofp)
    { *ofp = queryfont_vect; queryfont_vect = nfp; }
void ami_queryfont(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br, ami_long* bg, ami_long* bb, ami_qfteffects* effect)
    { (*queryfont_vect)(f, fc, s, fr, fg, fb, br, bg, bb, effect); }
static void queryfont_ivf(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br, ami_long* bg, ami_long* bb, ami_qfteffects* effect)
    { unimp("queryfont"); }


/*******************************************************************************

Screen capture service

Returns a copy of the composed display, for the screen_capture test
module. The caller frees the buffer.

*******************************************************************************/

uint32_t* grx_capture(int* width, int* height)

{

    uint32_t* buf;

    if (!scrcan) return (NULL);
    buf = malloc((size_t)scrcan->w*scrcan->h*4);
    if (!buf) return (NULL);
    memcpy(buf, scrcan->px, (size_t)scrcan->w*scrcan->h*4);
    *width = scrcan->w;
    *height = scrcan->h;

    return (buf);

}

/*******************************************************************************

Compare the canvas against the glass

A diagnostic backdoor: compares the composed screen canvas against the
device pixels, row by row, reporting how many pixels differ and where,
and writes both sides as ppm files in the working directory. When the
display shows something the canvas does not (or the reverse), this
says so with numbers.

*******************************************************************************/

void grx_devdump(const char* fn)

{

    ami_long x, y;
    FILE* fp;

    if (!fbbase) return;
    fp = fopen(fn, "wb");
    if (!fp) return;
    fprintf(fp, "P6\n%lld %lld\n255\n", AMI_LONG_CAST(fbcols), AMI_LONG_CAST(fbrows));
    for (y = 0; y < fbrows; y++)
        for (x = 0; x < fbcols; x++) {

            unsigned char* dp = fbbase+((size_t)y*fbcols+x)*fbpixsiz;

            fputc(dp[fbroff], fp);
            fputc(dp[fbgoff], fp);
            fputc(dp[fbboff], fp);

        }
    fclose(fp);

}

void grx_glassdiff(void)

{

    ami_long x, y, n = 0;
    ami_long x1 = -1, y1 = -1, x2 = -1, y2 = -1;
    FILE* fp;

    if (!scrcan || !fbbase) return;
    for (y = 0; y < scrcan->h && y < fbrows; y++)
        for (x = 0; x < scrcan->w && x < fbcols; x++) {

            uint32_t cv = scrcan->px[(size_t)y*scrcan->w+x];
            unsigned char* dp = fbbase+((size_t)y*fbcols+x)*fbpixsiz;

            if ((dp[fbroff] != (cv>>16&0xff)) ||
                (dp[fbgoff] != (cv>>8&0xff)) ||
                (dp[fbboff] != (cv&0xff))) {

                n++;
                if (x1 < 0 || x < x1) x1 = x;
                if (y1 < 0 || y < y1) y1 = y;
                if (x > x2) x2 = x;
                if (y > y2) y2 = y;

            }

        }
    fprintf(stderr, "*** glassdiff: %lld pixels differ", AMI_LONG_CAST(n));
    if (n) fprintf(stderr, ", box %lld,%lld to %lld,%lld", AMI_LONG_CAST(x1), AMI_LONG_CAST(y1), AMI_LONG_CAST(x2), AMI_LONG_CAST(y2));
    fprintf(stderr, "\n");
    fp = fopen("glass_report.txt", "a");
    if (fp) {

        fprintf(fp, "%lld pixels differ", AMI_LONG_CAST(n));
        if (n) fprintf(fp, ", box %lld,%lld to %lld,%lld", AMI_LONG_CAST(x1), AMI_LONG_CAST(y1), AMI_LONG_CAST(x2), AMI_LONG_CAST(y2));
        fprintf(fp, "\n");
        fclose(fp);

    }
    /* the images only when the divergence is more than the pointer */
    if (n <= 2000) return;
    fp = fopen("glass_canvas.ppm", "wb");
    if (fp) {

        fprintf(fp, "P6\n%d %d\n255\n", scrcan->w, scrcan->h);
        for (y = 0; y < scrcan->h; y++)
            for (x = 0; x < scrcan->w; x++) {

                uint32_t cv = scrcan->px[(size_t)y*scrcan->w+x];
                fputc(cv>>16&0xff, fp);
                fputc(cv>>8&0xff, fp);
                fputc(cv&0xff, fp);

            }
        fclose(fp);

    }
    fp = fopen("glass_device.ppm", "wb");
    if (fp) {

        fprintf(fp, "P6\n%lld %lld\n255\n", AMI_LONG_CAST(fbcols), AMI_LONG_CAST(fbrows));
        for (y = 0; y < fbrows; y++)
            for (x = 0; x < fbcols; x++) {

                unsigned char* dp = fbbase+((size_t)y*fbcols+x)*fbpixsiz;

                fputc(dp[fbroff], fp);
                fputc(dp[fbgoff], fp);
                fputc(dp[fbboff], fp);

            }
        fclose(fp);

    }

}

/*******************************************************************************

Initialize graphics

Runs before main(), after the frame buffer module has mapped the device.
Sets the override vectors, hooks stdio, loads the fonts, opens the
screen as the stdin/stdout window pair, puts the console keyboard in raw
mode and registers the input sources with system_event.

*******************************************************************************/

static void ami_init_graphics(void) __attribute__((constructor(103)));
static void ami_init_graphics(void)

{

    int        fi;
    ami_evtcod e;
    ami_long   ps;
    void*      base;
    int        ji;
    char       joyfil[] = "/dev/input/js0";
    int        joyfid;

    /* set override vectors to defaults */
    scrollg_vect = scrollg_ivf;
    scroll_vect = scroll_ivf;
    cursor_vect = cursor_ivf;
    cursorg_vect = cursorg_ivf;
    baseline_vect = baseline_ivf;
    maxx_vect = maxx_ivf;
    maxy_vect = maxy_ivf;
    maxxg_vect = maxxg_ivf;
    maxyg_vect = maxyg_ivf;
    home_vect = home_ivf;
    up_vect = up_ivf;
    down_vect = down_ivf;
    left_vect = left_ivf;
    right_vect = right_ivf;
    del_vect = del_ivf;
    blink_vect = blink_ivf;
    reverse_vect = reverse_ivf;
    underline_vect = underline_ivf;
    superscript_vect = superscript_ivf;
    subscript_vect = subscript_ivf;
    italic_vect = italic_ivf;
    bold_vect = bold_ivf;
    strikeout_vect = strikeout_ivf;
    standout_vect = standout_ivf;
    condensed_vect = condensed_ivf;
    extended_vect = extended_ivf;
    xlight_vect = xlight_ivf;
    light_vect = light_ivf;
    xbold_vect = xbold_ivf;
    hollow_vect = hollow_ivf;
    raised_vect = raised_ivf;
    fcolor_vect = fcolor_ivf;
    fcolorc_vect = fcolorc_ivf;
    fcolorg_vect = fcolorg_ivf;
    bcolor_vect = bcolor_ivf;
    bcolorc_vect = bcolorc_ivf;
    bcolorg_vect = bcolorg_ivf;
    auto_vect = auto_ivf;
    curvis_vect = curvis_ivf;
    curx_vect = curx_ivf;
    cury_vect = cury_ivf;
    curxg_vect = curxg_ivf;
    curyg_vect = curyg_ivf;
    curbnd_vect = curbnd_ivf;
    select_vect = select_ivf;
    wrtstrn_vect = wrtstrn_ivf;
    wrtstr_vect = wrtstr_ivf;
    line_vect = line_ivf;
    rect_vect = rect_ivf;
    frect_vect = frect_ivf;
    rrect_vect = rrect_ivf;
    frrect_vect = frrect_ivf;
    ellipse_vect = ellipse_ivf;
    fellipse_vect = fellipse_ivf;
    arc_vect = arc_ivf;
    farc_vect = farc_ivf;
    fchord_vect = fchord_ivf;
    ftriangle_vect = ftriangle_ivf;
    setpixel_vect = setpixel_ivf;
    fover_vect = fover_ivf;
    bover_vect = bover_ivf;
    finvis_vect = finvis_ivf;
    binvis_vect = binvis_ivf;
    fxor_vect = fxor_ivf;
    bxor_vect = bxor_ivf;
    fand_vect = fand_ivf;
    band_vect = band_ivf;
    for_vect = for_ivf;
    bor_vect = bor_ivf;
    linewidth_vect = linewidth_ivf;
    linestyle_vect = linestyle_ivf;
    chrsizx_vect = chrsizx_ivf;
    chrsizy_vect = chrsizy_ivf;
    fonts_vect = fonts_ivf;
    font_vect = font_ivf;
    fontnam_vect = fontnam_ivf;
    fontsiz_vect = fontsiz_ivf;
    setpoints_vect = setpoints_ivf;
    points_vect = points_ivf;
    chrspcy_vect = chrspcy_ivf;
    chrspcx_vect = chrspcx_ivf;
    dpmx_vect = dpmx_ivf;
    dpmy_vect = dpmy_ivf;
    strsiz_vect = strsiz_ivf;
    chrpos_vect = chrpos_ivf;
    writejust_vect = writejust_ivf;
    justpos_vect = justpos_ivf;
    settabg_vect = settabg_ivf;
    settab_vect = settab_ivf;
    restabg_vect = restabg_ivf;
    restab_vect = restab_ivf;
    clrtab_vect = clrtab_ivf;
    funkey_vect = funkey_ivf;
    title_vect = title_ivf;
    loadpict_vect = loadpict_ivf;
    delpict_vect = delpict_ivf;
    pictsizx_vect = pictsizx_ivf;
    pictsizy_vect = pictsizy_ivf;
    picture_vect = picture_ivf;
    viewoffg_vect = viewoffg_ivf;
    viewscale_vect = viewscale_ivf;
    scalex_vect = scalex_ivf;
    scaley_vect = scaley_ivf;
    path_vect = path_ivf;
    blockcopyg_vect = blockcopyg_ivf;
    event_vect = event_ivf;
    sendevent_vect = sendevent_ivf;
    eventover_vect = eventover_ivf;
    eventsover_vect = eventsover_ivf;
    timer_vect = timer_ivf;
    killtimer_vect = killtimer_ivf;
    frametimer_vect = frametimer_ivf;
    autohold_vect = autohold_ivf;
    mouse_vect = mouse_ivf;
    mousebutton_vect = mousebutton_ivf;
    joystick_vect = joystick_ivf;
    joybutton_vect = joybutton_ivf;
    joyaxis_vect = joyaxis_ivf;
    openwin_vect = openwin_ivf;
    buffer_vect = buffer_ivf;
    sizbuf_vect = sizbuf_ivf;
    sizbufg_vect = sizbufg_ivf;
    getsiz_vect = getsiz_ivf;
    getsizg_vect = getsizg_ivf;
    setsiz_vect = setsiz_ivf;
    setsizg_vect = setsizg_ivf;
    setpos_vect = setpos_ivf;
    setposg_vect = setposg_ivf;
    scnsiz_vect = scnsiz_ivf;
    scnsizg_vect = scnsizg_ivf;
    scncen_vect = scncen_ivf;
    scnceng_vect = scnceng_ivf;
    winclient_vect = winclient_ivf;
    winclientg_vect = winclientg_ivf;
    front_vect = front_ivf;
    back_vect = back_ivf;
    frame_vect = frame_ivf;
    sizable_vect = sizable_ivf;
    sysbar_vect = sysbar_ivf;
    menu_vect = menu_ivf;
    menuena_vect = menuena_ivf;
    menusel_vect = menusel_ivf;
    stdmenu_vect = stdmenu_ivf;
    getwinid_vect = getwinid_ivf;
    focus_vect = focus_ivf;
    getwigid_vect = getwigid_ivf;
    killwidget_vect = killwidget_ivf;
    selectwidget_vect = selectwidget_ivf;
    enablewidget_vect = enablewidget_ivf;
    getwidgettext_vect = getwidgettext_ivf;
    putwidgettext_vect = putwidgettext_ivf;
    sizwidget_vect = sizwidget_ivf;
    sizwidgetg_vect = sizwidgetg_ivf;
    poswidget_vect = poswidget_ivf;
    poswidgetg_vect = poswidgetg_ivf;
    backwidget_vect = backwidget_ivf;
    frontwidget_vect = frontwidget_ivf;
    focuswidget_vect = focuswidget_ivf;
    buttonsiz_vect = buttonsiz_ivf;
    buttonsizg_vect = buttonsizg_ivf;
    button_vect = button_ivf;
    buttong_vect = buttong_ivf;
    checkboxsiz_vect = checkboxsiz_ivf;
    checkboxsizg_vect = checkboxsizg_ivf;
    checkbox_vect = checkbox_ivf;
    checkboxg_vect = checkboxg_ivf;
    radiobuttonsiz_vect = radiobuttonsiz_ivf;
    radiobuttonsizg_vect = radiobuttonsizg_ivf;
    radiobutton_vect = radiobutton_ivf;
    radiobuttong_vect = radiobuttong_ivf;
    groupsizg_vect = groupsizg_ivf;
    groupsiz_vect = groupsiz_ivf;
    group_vect = group_ivf;
    groupg_vect = groupg_ivf;
    background_vect = background_ivf;
    backgroundg_vect = backgroundg_ivf;
    scrollvertsizg_vect = scrollvertsizg_ivf;
    scrollvertsiz_vect = scrollvertsiz_ivf;
    scrollvert_vect = scrollvert_ivf;
    scrollvertg_vect = scrollvertg_ivf;
    scrollhorizsizg_vect = scrollhorizsizg_ivf;
    scrollhorizsiz_vect = scrollhorizsiz_ivf;
    scrollhoriz_vect = scrollhoriz_ivf;
    scrollhorizg_vect = scrollhorizg_ivf;
    scrollpos_vect = scrollpos_ivf;
    scrollsiz_vect = scrollsiz_ivf;
    numselboxsizg_vect = numselboxsizg_ivf;
    numselboxsiz_vect = numselboxsiz_ivf;
    numselbox_vect = numselbox_ivf;
    numselboxg_vect = numselboxg_ivf;
    editboxsizg_vect = editboxsizg_ivf;
    editboxsiz_vect = editboxsiz_ivf;
    editbox_vect = editbox_ivf;
    editboxg_vect = editboxg_ivf;
    progbarsizg_vect = progbarsizg_ivf;
    progbarsiz_vect = progbarsiz_ivf;
    progbar_vect = progbar_ivf;
    progbarg_vect = progbarg_ivf;
    progbarpos_vect = progbarpos_ivf;
    listboxsizg_vect = listboxsizg_ivf;
    listboxsiz_vect = listboxsiz_ivf;
    listbox_vect = listbox_ivf;
    listboxg_vect = listboxg_ivf;
    dropboxsizg_vect = dropboxsizg_ivf;
    dropboxsiz_vect = dropboxsiz_ivf;
    dropbox_vect = dropbox_ivf;
    dropboxg_vect = dropboxg_ivf;
    dropeditboxsizg_vect = dropeditboxsizg_ivf;
    dropeditboxsiz_vect = dropeditboxsiz_ivf;
    dropeditbox_vect = dropeditbox_ivf;
    dropeditboxg_vect = dropeditboxg_ivf;
    slidehorizsizg_vect = slidehorizsizg_ivf;
    slidehorizsiz_vect = slidehorizsiz_ivf;
    slidehoriz_vect = slidehoriz_ivf;
    slidehorizg_vect = slidehorizg_ivf;
    slidevertsizg_vect = slidevertsizg_ivf;
    slidevertsiz_vect = slidevertsiz_ivf;
    slidevert_vect = slidevert_ivf;
    slidevertg_vect = slidevertg_ivf;
    tabbarsizg_vect = tabbarsizg_ivf;
    tabbarsiz_vect = tabbarsiz_ivf;
    tabbarclientg_vect = tabbarclientg_ivf;
    tabbarclient_vect = tabbarclient_ivf;
    tabbar_vect = tabbar_ivf;
    tabbarg_vect = tabbarg_ivf;
    tabsel_vect = tabsel_ivf;
    alert_vect = alert_ivf;
    querycolor_vect = querycolor_ivf;
    queryopen_vect = queryopen_ivf;
    querysave_vect = querysave_ivf;
    queryfind_vect = queryfind_ivf;
    queryfindrep_vect = queryfindrep_ivf;
    queryfont_vect = queryfont_ivf;

    /* set internal states */
    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */
    errflg = FALSE; /* set no error occurred */
    fntlst = NULL; /* clear font list */
    fntcnt = 0;
    frepic = NULL; /* clear free pictures list */
    paqfre = NULL; /* clear free event queue entries */
    paqevt = NULL; /* clear event queue */
    kbdseql = 0; /* clear keyboard decode state */
    kbdseq[0] = 0;

    /* set "configuration" font capabilities */
    cfgcap = BIT(xcmedium) | BIT(xcbold) | BIT(xcdemibold) | BIT(xcdark) |
             BIT(xclight) | BIT(xcblack) | BIT(xcital) | BIT(xcoblique) |
             BIT(xcrital) | BIT(xcroblique) | BIT(xcnarrow) | BIT(xccondensed) |
             BIT(xcsemicondensed) | BIT(xcexpanded);

    /* the font lock, before any drawing exists */
    {

        pthread_mutexattr_t ma;

        pthread_mutexattr_init(&ma);
        pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&ftlock, &ma);
        pthread_mutexattr_destroy(&ma);

    }

    /* clear open files tables */
    for (fi = 0; fi < MAXFIL; fi++) {

        opnfil[fi] = NULL; /* set unoccupied */
        filwin[fi] = -1; /* set unoccupied */

    }
    /* clear window equivalence table */
    for (fi = 0; fi < MAXFIL*2+1; fi++) xltwin[fi] = -1;
    /* clear system event table */
    for (fi = 0; fi < MAXSID; fi++) sidtab[fi] = NULL;

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_etdsize; e++) evthan[e] = defaultevent;

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);
    ovr_read_nocancel(iread_nocancel, &ofpread_nocancel);
    ovr_write_nocancel(iwrite_nocancel, &ofpwrite_nocancel);
    ovr_open_nocancel(iopen_nocancel, &ofpopen_nocancel);
    ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);

    /* take the screen from the frame buffer module */
    frame_geometry(&fbrows, &fbcols);
    frame_pixsiz(&ps);
    fbpixsiz = ps;
    frame_rgboff(&fbroff, &fbgoff, &fbboff);
    frame_buffer(&base);
    fbbase = base;
    /* the fast path: the device pixel is the canvas pixel */
    fbfast = (fbpixsiz == 4 && fbroff == 2 && fbgoff == 1 && fbboff == 0);

    /* the screen canvas mirrors the display */
    scrcan = newcanvas(fbcols, fbrows);
    if (!scrcan) {

        fprintf(stderr, "*** Error: graphics: cannot allocate screen\n");
        exit(1);

    }

    /* initialize FreeType and fontconfig */
    if (FT_Init_FreeType(&ftlibrary)) {

        fprintf(stderr, "*** Error: graphics: cannot initialize FreeType\n");
        exit(1);

    }
    FcInit();

    /* load the font set */
    getfonts();

    /* the one window */
    thewin = imalloc(sizeof(winrec));
    memset(thewin, 0, sizeof(winrec));

    /* open stdin and stdout as the I/O window set */
    openio(stdin, stdout, fileno(stdin), fileno(stdout), -1, 1);

    /* the console keyboard: raw mode, so keys arrive as they are struck */
    inraw = FALSE;
    if (isatty(0)) {

        if (!tcgetattr(0, &saveterm)) {

            struct termios raw;

            raw = saveterm;
            raw.c_lflag &= ~(ICANON|ECHO);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            if (!tcsetattr(0, TCSANOW, &raw)) inraw = TRUE;

        }

    }

    /* The console mouse: the kernel's aggregated PS/2 stream. FBMOUSE
       in the environment overrides the path, which is how the tests
       feed packets through a fifo. No device is no mouse. */
    {

        const char* mp;

        mp = getenv("FBMOUSE");
        if (!mp) mp = "/dev/input/mice";
        micefd = open(mp, O_RDONLY|O_NONBLOCK);
        micecnt = 0;
        mptrvis = FALSE;
        mptrx = fbcols/2; /* the pointer starts centered, hidden until
                             it moves */
        mptry = fbrows/2;
        if (micefd >= 0) micesev = system_event_addseinp(micefd);

    }

    /* register the input sources */
    kbdsev = system_event_addseinp(0); /* the keyboard */
    intsev = system_event_addsesig(SIGINT); /* console interrupt */
    termsev = system_event_addsesig(SIGTERM); /* terminate */

    /* clear joystick table */
    for (ji = 0; ji < MAXJOY; ji++) joytab[ji] = NULL;
    numjoy = 0; /* set no joysticks */
#ifndef NOJOYSTICK
    if (joyenb) { /* if joystick is to be enabled */

        do { /* find joysticks */

            joyfil[13] = numjoy+'0'; /* set number of joystick to find */
            joyfid = open(joyfil, O_RDONLY|O_NONBLOCK);
            if (joyfid >= 0) { /* found */

                char jc;

                /* get a joystick table entry */
                joytab[numjoy] = imalloc(sizeof(joyrec));
                joytab[numjoy]->fid = joyfid; /* set fid */
                /* enable system event */
                joytab[numjoy]->sid = system_event_addseinp(joyfid);
                getsee(joytab[numjoy]->sid); /* allocate system event entry */
                /* set joystick number */
                sidtab[joytab[numjoy]->sid-1]->joy = numjoy+1;
                joytab[numjoy]->ax = 0; /* clear joystick axis saves */
                joytab[numjoy]->ay = 0;
                joytab[numjoy]->az = 0;
                joytab[numjoy]->a4 = 0;
                joytab[numjoy]->a5 = 0;
                joytab[numjoy]->a6 = 0;
                joytab[numjoy]->no = numjoy+1; /* set logical number */
                /* get number of axes */
                jc = 2;
                ioctl(joyfid, JSIOCGAXES, &jc);
                joytab[numjoy]->axis = jc;
                /* get number of buttons */
                jc = 2;
                ioctl(joyfid, JSIOCGBUTTONS, &jc);
                joytab[numjoy]->button = jc;
                numjoy++; /* count joysticks */

            }

        } while (numjoy < MAXJOY && joyfid >= 0); /* no more joysticks */

    }
#endif

    ginit = TRUE; /* the module is up */

}

/*******************************************************************************

Deinitialize graphics

Runs after main() returns or exit(). If the program never received a
terminate and autohold is on, the display is held until the user orders
the exit, so a graphics unaware program's output can be viewed. Then the
console is put back and the stdio hooks removed.

*******************************************************************************/

static void ami_deinit_graphics(void) __attribute__((destructor(103)));
static void ami_deinit_graphics(void)

{

    /* holding copies of system vectors */
    pread_t cppread;
    pread_t cppread_nocancel;
    pwrite_t cppwrite;
    pwrite_t cppwrite_nocancel;
    popen_t cppopen;
    popen_t cppopen_nocancel;
    pclose_t cppclose;
    pclose_t cppclose_nocancel;
    plseek_t cpplseek;
    winptr win;
    int    fn;
    int    ji;
    ami_evtrec er;
    ami_evtcod e;

    /* If initialization never completed -- the frame buffer module
       failed and exited before this module came up -- there is nothing
       here to take down, and the vectors below were never hooked. */
    if (!ginit) return;

    /* Reset all event vectors back to the default handler. The client
       program may have installed overrides that are no longer safe to
       call now that main() has returned. */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_etdsize; e++) evthan[e] = defaultevent;

    /* try to get window from stdout */
    win = NULL; /* set no window */
    fn = fileno(stdout); /* get fid */
    if (fn >= 0 && fn < MAXFIL && opnfil[fn]) win = opnfil[fn]->win;
    if (win && !errflg && !fend && fautohold && inraw) {

        /* If the program tries to exit when the user has not ordered an
           exit, it is assumed to be a graphics unaware program. We stop
           before we exit these, so that their content may be viewed. */
        printf("\nFinished - %s (ctrl-c to exit)",
               program_invocation_short_name);
        /* wait for a formal end */
        while (!fend) ami_event(stdin, &er);

    }

    /* put the console back */
    if (inraw) tcsetattr(0, TCSANOW, &saveterm);

    /* close the mouse */
    if (micefd >= 0) close(micefd);

    /* close joysticks */
    for (ji = 0; ji < MAXJOY; ji++) if (joytab[ji]) close(joytab[ji]->fid);

    /* clear the glyph cache */
    ft_cache_clear();
    /* FT_Done_FreeType/FcFini are skipped: faces were released
       individually, and the process is exiting */

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_lseek(ofplseek, &cpplseek);
    ovr_read_nocancel(ofpread_nocancel, &cppread_nocancel);
    ovr_write_nocancel(ofpwrite_nocancel, &cppwrite_nocancel);
    ovr_open_nocancel(ofpopen_nocancel, &cppopen_nocancel);
    ovr_close_nocancel(ofpclose_nocancel, &cppclose_nocancel);

    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cpplseek != ilseek)
        error(esystem);

    pthread_mutex_destroy(&ftlock);

}
