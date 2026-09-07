/*******************************************************************************
*                                                                              *
* PLATFORM DISPLAY INTERFACE                                                   *
*                                                                              *
* The seam between the graphical backend and the platform: windows, raster     *
* canvases, input, and presentation, in Ami's own vocabulary. The interface    *
* is an implementation's whole contract, and implementations interchange       *
* behind it; the Wayland module carries it on that platform.                   *
*                                                                              *
* Design rules:                                                                *
*                                                                              *
* 1. Native vocabulary. Every entry point exists because the backend           *
*    needs it, with the semantics the backend needs.                           *
*                                                                              *
* 2. Produce, damage, present. All drawing lands in canvases (CPU pixel        *
*    buffers, ARGB8888). A window owns a canvas; presenting declares the       *
*    damaged rectangle and the layer batches, paces to the compositor's        *
*    frame callbacks, and publishes whole frames. Immediate-mode callers       *
*    get a live beat: the layer publishes on its own when a caller draws       *
*    without returning to the event machinery.                                 *
*                                                                              *
* 3. Data-scoped locking. Every entry point is thread-safe. Locks live         *
*    inside the layer, scoped to the data they protect and held no wider:      *
*    a canvas lock for that canvas's pixels, a queue lock for the event        *
*    queue, a wire lock for compositor traffic. Callers never take a lock      *
*    to use this interface, and no entry point holds one lock while            *
*    sleeping on another's data.                                               *
*                                                                              *
* The test rig rides inside the implementation and is invisible here:          *
* PD_DUMP names a prefix for per-present frame dumps, PD_INPUT a fifo of       *
* synthesized input lines. The Wayland module honors the AMI_WL_* names        *
* as well.                                                                     *
*                                                                              *
*******************************************************************************/

#ifndef PDISPLAY_H
#define PDISPLAY_H

#include <stdint.h>
#include <stddef.h>

/* opaque handles: the connection, a window in the tree, a raster canvas */
typedef struct pd_display pd_display;
typedef struct pd_win     pd_win;
typedef struct pd_canvas  pd_canvas;

/*******************************************************************************

Drawing parameters

A plain struct the caller owns and fills. The mix function is
first-class: the bitwise modes are the reason this layer rasterizes for
itself.

*******************************************************************************/

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
    pd_canvas*   stipple; /* fill stipple, nonzero pixels paint; NULL none */
    int          stipx;   /* stipple origin */
    int          stipy;

} pd_draw;

/*******************************************************************************

Events

One tagged record; pd_evtnext delivers them in arrival order. Keyboard
codes are xkb keysyms (numerically the traditional keysym space), with
the modifier state carried on every input event.

*******************************************************************************/

typedef enum {

    pd_etnone,    /* queue empty (nonblocking read) */
    pd_etkeydown, /* key: keysym, code; mods */
    pd_etkeyup,
    pd_etmouse,   /* pointer motion: win x, y; mods */
    pd_etbtndown, /* button: btn 1-3, wheel 4-5; win x, y; mods */
    pd_etbtnup,
    pd_etenter,   /* pointer entered win */
    pd_etleave,   /* pointer left win */
    pd_etfocus,   /* keyboard focus to win */
    pd_etnofocus, /* keyboard focus left win */
    pd_etresize,  /* compositor sized win: w, h are the client area */
    pd_etredraw,  /* region must be repainted: x, y, w, h */
    pd_etclose,   /* user ordered the window closed */
    pd_etmap,     /* win reached the screen */
    pd_etframe,   /* compositor frame pacing tick for win */
    pd_etmin,     /* window minimized state changes */
    pd_etmax,
    pd_etrestore

} pd_etype;

/* modifier and button state mask */
#define PD_MSHIFT (1<<0)
#define PD_MCTRL  (1<<1)
#define PD_MALT   (1<<2)
#define PD_MBTN1  (1<<3)
#define PD_MBTN2  (1<<4)
#define PD_MBTN3  (1<<5)

typedef struct {

    pd_etype  etype; /* what happened */
    pd_win*   win;   /* where */
    unsigned  mods;  /* modifier/button state at the event */
    uint32_t  keysym; /* key events: translated symbol */
    uint32_t  code;   /* key events: raw platform code */
    int       x, y;   /* pointer events: window coordinates */
    int       btn;    /* button events */
    int       w, h;   /* resize */
    unsigned long token; /* resize: the pd_winsize request answered,
                            zero when the compositor originated it */
    int       rx, ry, rw, rh; /* redraw region */
    uint32_t  time;  /* platform timestamp, ms */

} pd_evt;

/*******************************************************************************

Connection and screen

*******************************************************************************/

/* open the platform connection; NULL on failure (no compositor) */
pd_display* pd_open(void);

/* orderly close; windows and canvases die with it */
void pd_close(pd_display* d);

/* primary output metrics: pixels and millimeters. Values follow the
   output the compositor reports; a caller wanting density divides */
void pd_screen(pd_display* d, int* wpx, int* hpx, int* wmm, int* hmm);
/* the output's integer scale: buffer pixels per logical pixel, 1 on an
   unscaled desktop. A toplevel is a whole number of logical pixels on a
   side, so a size that is not a multiple of this comes back from the
   compositor rounded up */
int  pd_scale(pd_display* d);

/*******************************************************************************

Windows

A tree: toplevels parent to NULL, children clip and stack within their
parent, coordinates are parent-relative. The layer routes input by
position and z-order and composes the tree into each toplevel's frame.
Decoration is the backend's paint; the frame regions below only declare
where the compositor may run interactive move and resize.

*******************************************************************************/

pd_win* pd_winnew(pd_display* d, pd_win* parent, int x, int y, int w, int h);
void    pd_windel(pd_win* w);

void    pd_winmap(pd_win* w, int visible);
void    pd_winmove(pd_win* w, int x, int y);

/* sizing returns a request token; the pd_etresize answering it carries
   the same token, and a compositor-initiated resize carries zero. The
   caller tells its own requests' echoes from fresh sizings by token */
unsigned long pd_winsize(pd_win* w, int width, int height);

void    pd_winraise(pd_win* w);
void    pd_winlower(pd_win* w);

/* window geometry and map state as the layer holds them */
void    pd_wingeom(pd_win* w, int* x, int* y, int* width, int* height,
                   int* mapped);

/* size bounds the compositor honors during interactive resize;
   zeros lift a bound */
void    pd_winlimits(pd_win* w, int minw, int minh, int maxw, int maxh);

/* route all pointer input to w until released, as a drag holds it */
void    pd_grab(pd_win* w, int on);

/* current pointer position in w's coordinates; returns nonzero with
   the pointer inside w */
int     pd_pointer(pd_win* w, int* x, int* y);

/* the compositor-facing identity of a toplevel */
void    pd_wintitle(pd_win* w, const char* title);

/* declare the interactive regions of an owner-drawn frame: the title
   rectangle drags the window, a border of the given width resizes it.
   The border is an input width: the grab ring rides over the client
   edge, thinning to the title inset beside the title row, so the caller
   may draw a slimmer frame than it declares. Zero widths clear the
   declarations */
void    pd_winframe(pd_win* w, int titx, int tity, int titw, int tith,
                    int borderw);

/* shell state requests, honored as toplevels */
void    pd_minimize(pd_win* w);
void    pd_maximize(pd_win* w, int on);

/* hand the toplevel to the compositor for an interactive move, riding
   the most recent button press */
void    pd_windrag(pd_win* w);

/* standard cursor shapes over a window */
typedef enum {

    pd_curarrow, pd_curtext, pd_curcross, pd_curhand,
    pd_cursizeh, pd_cursizev, pd_cursizenwse, pd_cursizenesw

} pd_curshape;

void    pd_cursor(pd_win* w, pd_curshape shape);

/*******************************************************************************

Canvases and drawing

A canvas is a CPU pixel rectangle. Windows own one (pd_wincanvas); free
canvases serve as backing stores, stipples, and picture sources. All
primitives honor the full pd_draw record, mix modes included, and every
entry locks only the canvases it touches.

*******************************************************************************/

pd_canvas* pd_cannew(pd_display* d, int w, int h);
void       pd_candel(pd_canvas* c);
pd_canvas* pd_wincanvas(pd_win* w);
/* Compose a window and everything below it into a buffer of its own: the
   picture as presented, carrying the child windows a canvas alone does
   not. The caller frees it. Used by the screen capture. */
uint32_t*  pd_winsnap(pd_display* d, pd_win* win, int* width, int* height);
void       pd_cansize(pd_canvas* c, int* w, int* h);

/* raw pixel access for loaders; the caller brackets its own access
   with pd_canlock/unlock and touches nothing outside the canvas */
uint32_t*  pd_canlock(pd_canvas* c, int* stride);
void       pd_canunlock(pd_canvas* c);

void pd_point(pd_canvas* c, pd_draw* dr, int x, int y);
void pd_line(pd_canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2);
void pd_rect(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h);
void pd_frect(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h);
void pd_fpoly(pd_canvas* c, pd_draw* dr, const int* xy, int n);

/* ellipse arcs: the box, then start and extent in 64ths of a degree,
   positive counterclockwise from three o'clock. A full sweep takes the
   scanline path; pie closes through the center, chord rim to rim */
void pd_arc(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h,
            int a1, int a2);
void pd_farcpie(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h,
                int a1, int a2);
void pd_farcchord(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h,
                  int a1, int a2);

/* glyph blit: blend a FreeType mask (1-bit or 8-bit alpha, given
   stride) in the draw color and mix at x, y. The text path's one need */
void pd_glyph(pd_canvas* c, pd_draw* dr, int x, int y,
              const uint8_t* mask, int mw, int mh, int stride, int depth);

/* canvas to canvas copy, same size */
void pd_blit(pd_canvas* dst, int dx, int dy,
             pd_canvas* src, int sx, int sy, int w, int h);

/* copy within one canvas, overlap-correct: the scroll */
void pd_scroll(pd_canvas* c, int dx, int dy, int sx, int sy, int w, int h);

/* scaled copy, source rectangle to destination rectangle */
void pd_stretch(pd_canvas* dst, int dx, int dy, int dw, int dh,
                pd_canvas* src, int sx, int sy, int sw, int sh);

/*******************************************************************************

Presentation

Presenting publishes a window's canvas to the compositor. The layer
coalesces the declared damage, paces commits to the compositor's frame
callbacks, and carries the live beat for callers that draw straight
through. pd_flush pushes pending damage now; pd_sync returns when the
compositor holds it.

*******************************************************************************/

void pd_present(pd_win* w, int x, int y, int width, int height);
void pd_flush(pd_display* d);
/* one window's toplevel flushed alone: what a frame draw wants */
void pd_winflush(pd_win* w);
void pd_sync(pd_display* d);

/*******************************************************************************

Event delivery

pd_evtfd yields a descriptor for the caller's poll loop; when readable,
pd_evtnext drains. With block set, pd_evtnext waits for traffic and
pumps the wire while waiting. Frame ticks (pd_etframe) are opt-in per
toplevel and arrive at the display's refresh cadence while the
compositor presents the window; the caller floors them elsewhere when
withheld.

*******************************************************************************/

int  pd_evtfd(pd_display* d);
int  pd_evtnext(pd_display* d, pd_evt* e, int block);
void pd_frameevents(pd_win* w, int on);

/* view the head of the queue without taking it; zero when empty */
int  pd_evtpeek(pd_display* d, pd_evt* e);

/* take the next queued event of the given type for the given window
   (any window with NULL), leaving the rest in order; zero when none.
   The coalescing drains ride on this */
int  pd_evtcheck(pd_display* d, pd_win* w, pd_etype t, pd_evt* e);

/* place an event on the queue as if the platform delivered it; the
   backend's cross-thread sends arrive through here */
void pd_evtpost(pd_display* d, const pd_evt* e);

#endif /* PDISPLAY_H */
