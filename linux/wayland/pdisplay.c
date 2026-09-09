/** ****************************************************************************
*                                                                              *
*                    PLATFORM DISPLAY - WAYLAND MODULE                         *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
* Carries the platform display interface (pdisplay.h) on Wayland for the       *
* Petit-Ami graphical backend.                                                 *
*                                                                              *
* The model implemented:                                                       *
*                                                                              *
* - A window is a client-side record: a rectangle in its parent, a stacking    *
*   position, and a canvas holding its content. Root-parented windows are      *
*   Wayland xdg toplevels owning a wl_surface and a pair of shared memory      *
*   buffers; everything below them lives only in this process. The composite   *
*   of a toplevel's tree is assembled into its shm buffer and committed,       *
*   which is the plan's client-side child window tree: what a display server   *
*   does for child windows, done here.                                         *
*                                                                              *
* - Drawing is scanline rasterization into canvases, through a pixel store     *
*   honoring the mix function (copy/xor/and/or), the property that makes       *
*   the layer rasterize for itself. Stippled fills carry the glyph path.       *
*                                                                              *
* - Events arrive as pd_evt records synthesized from Wayland input and shell   *
*   events, keysyms translated through xkbcommon at the point of arrival.      *
*                                                                              *
* - pd_evtfd() returns an epoll descriptor combining the Wayland socket with   *
*   the layer's internal wake and repeat timers, so a select-based caller      *
*   drives everything from one descriptor.                                     *
*                                                                              *
* The test rig rides inside: PD_DUMP (or AMI_WL_DUMP) names a prefix for       *
* per-commit frame dumps; PD_INPUT (or AMI_WL_INPUT) names a fifo whose        *
* lines synthesize input; PD_NOBEAT (or AMI_WL_NOBEAT) stills the live         *
* beat for timing isolation.                                                   *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* (same terms as graphics.c)                                                   *
*                                                                              *
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <time.h>

#include <wayland-client.h>
#include <wayland-cursor.h>
#include "wlproto/xdg-shell-client-protocol.h"
#include <xkbcommon/xkbcommon.h>

#include "pdisplay.h"

/* Toplevel corner radius in logical pixels, as the desktop rounds its
   own windows; maximized windows square off, as the desktop squares
   them */
#define CORNERRAD 12

/* canvas: pixel content of a window or free canvas. Depth 1 canvases
   (glyph stipples) store one word per pixel holding 0 or 1; uniform
   addressing costs a little memory on tiny bitmaps and saves every op
   having two layouts */
struct pd_canvas {

    int       w, h;
    int       depth;
    uint32_t* px;
    pd_win*   owner; /* window whose content this is; NULL free */
    pthread_mutex_t lk; /* the pixels, held for a drawing operation only */

};

/* toplevel (root-parented window) data */
typedef struct wltop {

    pthread_mutex_t lk;     /* the toplevel: buffers, callback and configure
                               state, held through a compose */
    pthread_mutex_t dlk;    /* the damage rectangle alone: a drawing thread
                               unions into it without waiting for a compose */
    int     refs;           /* holders: the window, and a flush walk while it
                               composes; freed when the last lets go */
    pd_win* win;            /* the window it belongs to, NULL once dropped */
    struct wl_surface*  surf;
    struct xdg_surface* xsurf;
    struct xdg_toplevel* xtop;
    int    confw, confh;    /* compositor proposed size */
    int    configured;      /* first configure arrived */
    int    activated;       /* keyboard active state */
    int    maximized;
    int    mapnoted;        /* pd_etmap synthesized */
    struct wl_buffer* buf[2];
    uint32_t* bufpx[2];
    size_t bufsz[2];
    int    bufw, bufh;
    int    bufbusy[2];
    int    curbuf;
    int    dmg;             /* damage accumulated */
    int    dx1, dy1, dx2, dy2;
    struct wl_callback* fcb; /* outstanding frame callback */
#define PACEMS 15               /* a refresh, less a little: the compose pace while it is outstanding */
    int64_t fcbtime;        /* when it was armed */
    int    commitpend;      /* damage waiting on frame callback */
    int    applying;        /* a compositor resize is being applied: hold
                               commits so every configure answers with one
                               fully reconfigured frame, keeping the
                               compositor's resize anchoring in step */
    int64_t applyms;        /* when the application began, for the bailout */
    int    titx, tity, titw, tith; /* interactive move rectangle */
    int    borderw;         /* resize border width, 0 if none */

} wltop;

/* window record */
struct pd_win {

    pd_win*    parent;
    pd_win*    childs;   /* stacking order, head is bottom */
    pd_win*    sibnext;
    int        x, y;     /* parent relative */
    int        w, h;
    int        mapped;
    pd_canvas* can;
    wltop*     top;      /* toplevel data if root child */
    char*      title;
    int        cursor;   /* pd_curshape, -1 inherits the parent's */
    int        frmevt;   /* deliver frame callbacks as pd_etframe. On the
                            window, not the toplevel data: the request may
                            arrive before the window maps */

};

/* event queue entry */
typedef struct evq {

    pd_evt      e;
    struct evq* next;

} evq;

/* the display */
struct pd_display {

    struct wl_display*    dpy;
    /* buffer releases ride their own queue: the live beat drains it for
       free buffers without dispatching configures into a draw in flight */
    struct wl_event_queue* bufq;
    struct wl_registry*   reg;
    struct wl_compositor* comp;
    struct wl_shm*        shm;
    struct xdg_wm_base*   wm;
    struct wl_seat*       seat;
    struct wl_pointer*    ptr;
    struct wl_keyboard*   kbd;
    struct wl_output*     out;
    int    outw, outh;      /* output pixels */
    int    outmmw, outmmh;  /* output millimeters */
    int    scale;           /* output integer scale: buffer pixels per
                               logical pixel, 1 on an unscaled desktop */
    int    epfd;            /* epoll handed out as the event descriptor */
    int    evfd;            /* eventfd waking the queue */
    int    rtfd;            /* key repeat timerfd */
    pthread_mutex_t lk;     /* the connection and the seat: the socket's
                               read and dispatch, the pointer, keyboard,
                               focus and grab state, the cursor. Drawing,
                               composition and the tree are not under it */
    unsigned long sizetok;  /* resize request token counter */
    evq   *eqh, *eqt, *eqf; /* event queue head/tail/free */
    struct pd_win root;
    /* pointer state */
    pd_win* ptrtop;         /* toplevel the pointer is on */
    pd_win* ptrwin;         /* leaf window under pointer */
    double ptrx, ptry;      /* surface coordinates */
    unsigned mods;          /* modifier and button mask, PD_M encoding */
    pd_win* grab;           /* pointer grab window */
    pd_win* igrab;          /* implicit grab: the window holding a button
                               press owns the pointer until release, so the
                               release always reaches the presser */
    uint32_t inserial;      /* last input serial, for interactive move */
    /* keyboard state */
    pd_win* focus;          /* explicit input focus window */
    pd_win* kbdtop;         /* toplevel with wayland keyboard focus */
    struct xkb_context* xkb;
    struct xkb_keymap*  keymap;
    struct xkb_state*   xst;
    /* cursor theme */
    struct wl_cursor_theme* ctheme;
    struct wl_surface*      csurf;
    uint32_t enterserial;   /* serial of the pointer enter, for set_cursor */
    int    curshown;        /* shape currently shown, to skip re-sets */
    int    rptdelay, rptrate; /* repeat delay/rate, ms */
    uint32_t rptkey;        /* repeating key (X-style keycode), 0 if none */
    int    dumpseq;
    int    injfd;           /* rig input injection stream, -1 if none */
    int64_t livems;         /* last draw-path publish, for the live beat */
    int64_t burstms;        /* when the drawing since the last flush began,
                               0 when the program is back in its event loop */

};

static struct pd_display thedpy; /* the single connection */
static int    dpyopen;

/* forward */
static void pump(pd_display* d);
static void compose(pd_display* d, wltop* t);
static void flushtops(pd_display* d);
static void setcursor(pd_display* d, pd_win* w);
static void mktoplevel(pd_display* d, pd_win* w);
static void sizebufs(pd_display* d, pd_win* win);
static int  roundon(wltop* t);

/*******************************************************************************

Lock and time helpers

*******************************************************************************/

#define LK(d)   pthread_mutex_lock(&(d)->lk)
#define ULK(d)  pthread_mutex_unlock(&(d)->lk)

/* The other blocks and their locks. The tree lock covers the window
   tree's structure and each window's geometry, mapped flag, canvas and
   toplevel pointers: composition and hit testing read it, creation,
   deletion, moves, sizes and restacking write it. The queue lock covers
   the event queue alone. A canvas and a toplevel carry their own locks
   in their records. The order, where locks nest: connection, then a
   toplevel, then the tree, then a canvas; the queue last, under any.
   Damage marking takes the toplevel's lock by itself, never under the
   tree's; the tree lock is taken for reading again inside a compose
   started from the flush walk, which the reader-preferring kind allows */
static pthread_rwlock_t treelk;
static pthread_mutex_t  qlk = PTHREAD_MUTEX_INITIALIZER;
#define TREERD() pthread_rwlock_rdlock(&treelk)
#define TREEWR() pthread_rwlock_wrlock(&treelk)
#define TREEUN() pthread_rwlock_unlock(&treelk)
#define QLK()    pthread_mutex_lock(&qlk)
#define QULK()   pthread_mutex_unlock(&qlk)

static void reclock(pthread_mutex_t* m)
{
    pthread_mutexattr_t ma;

    pthread_mutexattr_init(&ma);
    pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &ma);
    pthread_mutexattr_destroy(&ma);
}
static void canlk(pd_canvas* c)  { pthread_mutex_lock(&c->lk); }
static void canulk(pd_canvas* c) { pthread_mutex_unlock(&c->lk); }
static void toplk(wltop* t)      { pthread_mutex_lock(&t->lk); }
static void topulk(wltop* t)     { pthread_mutex_unlock(&t->lk); }
static void dmglk(wltop* t)      { pthread_mutex_lock(&t->dlk); }
static void dmgulk(wltop* t)     { pthread_mutex_unlock(&t->dlk); }
static void topref(wltop* t)     { __atomic_add_fetch(&t->refs, 1, __ATOMIC_RELAXED); }
static void topunref(wltop* t)
{
    if (__atomic_sub_fetch(&t->refs, 1, __ATOMIC_ACQ_REL) == 0) {

        pthread_mutex_destroy(&t->lk);
        pthread_mutex_destroy(&t->dlk);
        free(t);

    }
}
/* two canvases, in address order, once when they are the same */
static void canlk2(pd_canvas* a, pd_canvas* b)
{
    if (a == b) { canlk(a); return; }
    if (a < b) { canlk(a); canlk(b); } else { canlk(b); canlk(a); }
}
static void canulk2(pd_canvas* a, pd_canvas* b)
{
    canulk(a);
    if (a != b) canulk(b);
}

static int64_t nowms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((int64_t)ts.tv_sec*1000+ts.tv_nsec/1000000);
}

/* a rig environment control, by its native name or its historical one */
static const char* rigenv(const char* pdname, const char* aminame)
{
    const char* s;

    s = getenv(pdname);
    if (!s) s = getenv(aminame);
    return (s);
}

/*******************************************************************************

Event queue

Events synthesized anywhere are appended here; the read calls take them
off. The eventfd keeps the caller's select loop honest when an event is
enqueued without Wayland socket traffic (cross thread posts, repeats).

*******************************************************************************/

static void enq(pd_display* d, pd_evt* e)
{
    evq* p;
    uint64_t one = 1;

    QLK();
    if (d->eqf) { p = d->eqf; d->eqf = p->next; }
    else p = malloc(sizeof(evq));
    p->e = *e;
    p->next = NULL;
    if (d->eqt) d->eqt->next = p; else d->eqh = p;
    d->eqt = p;
    QULK();
    if (write(d->evfd, &one, 8) < 0) { /* wake is advisory */ }
}

/* the queued events of a window leaving: delivered after its record is
   freed, they carry a dangling handle to the reader */
static void purgewin(pd_display* d, pd_win* w)
{
    evq** pp;
    evq*  p;

    QLK();
    pp = &d->eqh;
    while (*pp) {

        if ((*pp)->e.win == w) {

            p = *pp;
            *pp = p->next;
            p->next = d->eqf;
            d->eqf = p;

        } else pp = &(*pp)->next;

    }
    d->eqt = NULL;
    for (p = d->eqh; p; p = p->next) d->eqt = p;
    QULK();
}

static int deq(pd_display* d, pd_evt* e)
{
    evq* p;

    QLK();
    p = d->eqh;
    if (!p) { QULK(); return (0); }
    d->eqh = p->next;
    if (!d->eqh) d->eqt = NULL;
    *e = p->e;
    p->next = d->eqf;
    d->eqf = p;
    QULK();
    return (1);
}

/* base event fill */
static void mkevt(pd_evt* e, pd_etype t, pd_win* w)
{
    memset(e, 0, sizeof(pd_evt));
    e->etype = t;
    e->win = w;
    e->mods = thedpy.mods;
    e->time = (uint32_t)nowms();
}

/* a resize that grows a window reveals fresh canvas; queue redraw events
   for the right and bottom strips so the client repaints what the growth
   exposed */
static void expodmg(pd_display* d, pd_win* win, int ow, int oh)
{
    pd_evt e;

    if (win->w > ow) {

        mkevt(&e, pd_etredraw, win);
        e.rx = ow; e.ry = 0; e.rw = win->w-ow; e.rh = win->h;
        enq(d, &e);

    }
    if (win->h > oh) {

        mkevt(&e, pd_etredraw, win);
        e.rx = 0; e.ry = oh;
        e.rw = (ow < win->w)? ow: win->w; e.rh = win->h-oh;
        enq(d, &e);

    }
}

/*******************************************************************************

Canvas management and rasterization

*******************************************************************************/

static pd_canvas* newcanvas(int w, int h, int depth)
{
    pd_canvas* c;

    if (w < 1) w = 1;
    if (h < 1) h = 1;
    c = malloc(sizeof(pd_canvas));
    c->w = w; c->h = h; c->depth = depth;
    c->px = calloc((size_t)w*h, 4);
    c->owner = NULL;
    reclock(&c->lk);
    return (c);
}

static void freecanvas(pd_canvas* c)
{
    if (!c) return;
    pthread_mutex_destroy(&c->lk);
    free(c->px);
    free(c);
}

/* window canvas on demand; windows begin life white, the paper color of the
   library */
static pd_canvas* wincanvas(pd_win* w)
{
    int i;

    if (!w->can) {

        w->can = newcanvas(w->w, w->h, 32);
        w->can->owner = w;
        for (i = 0; i < w->can->w*w->can->h; i++) w->can->px[i] = 0xffffff;

    }
    return (w->can);
}

/* resize a window canvas preserving overlapping content; new area white.
   Under the tree's write lock: the canvas pointer changes. The old canvas
   is taken before it is read and freed, so a drawing in flight in it
   finishes first */
static void sizecanvas(pd_win* w, int nw, int nh)
{
    pd_canvas* c;
    pd_canvas* n;
    int x, y;

    c = w->can;
    if (!c) return; /* created at need at the new size */
    if (c->w == nw && c->h == nh) return;
    n = newcanvas(nw, nh, c->depth);
    n->owner = w;
    canlk(c);
    for (y = 0; y < nh; y++)
        for (x = 0; x < nw; x++)
            n->px[y*nw+x] = (x < c->w && y < c->h)? c->px[y*c->w+x]: 0xffffff;
    canulk(c);
    w->can = n;
    freecanvas(c);
}

/* The live beat's publisher: drain buffer releases from their own queue
   and push composed damage out. The default queue stays undispatched, so
   a configure cannot run inside the draw call that raised the beat */
static void beatpump(pd_display* d)
{
    struct pollfd pf;

    if (!d->dpy) return;
    LK(d); /* the socket read and the release queue's dispatch */
    while (wl_display_prepare_read_queue(d->dpy, d->bufq) != 0)
        wl_display_dispatch_queue_pending(d->dpy, d->bufq);
    wl_display_flush(d->dpy);
    pf.fd = wl_display_get_fd(d->dpy);
    pf.events = POLLIN;
    if (poll(&pf, 1, 0) > 0) wl_display_read_events(d->dpy);
    else wl_display_cancel_read(d->dpy);
    wl_display_dispatch_queue_pending(d->dpy, d->bufq);
    ULK(d);
    flushtops(d); /* the composes, under their toplevels' locks */
}

/* accumulate damage on the toplevel containing a window; rect in window
   coordinates */
static void windmg(pd_win* p, int x, int y, int w, int h)
{
    wltop* t;
    int x2, y2;

    /* The walk to the toplevel reads the ancestors without the tree lock:
       a window's ancestors outlive it, the caller owning the window it
       draws in, and an offset read while a parent moves only widens the
       union. The lock this takes is the toplevel's, for the union, and
       never under the tree's: composition holds the toplevel's lock and
       then reads the tree, and the reverse would deadlock */
    while (p->parent && p->parent->parent) { x += p->x; y += p->y; p = p->parent; }
    if (!p->parent) return; /* the root draws nowhere */
    t = p->top;
    if (!t) return;
    x2 = x+w; y2 = y+h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    dmglk(t);
    if (!t->dmg) { t->dx1 = x; t->dy1 = y; t->dx2 = x2; t->dy2 = y2; t->dmg = 1; }
    else {

        if (x < t->dx1) t->dx1 = x;
        if (y < t->dy1) t->dy1 = y;
        if (x2 > t->dx2) t->dx2 = x2;
        if (y2 > t->dy2) t->dy2 = y2;

    }
    dmgulk(t);
    /* The other half of immediate-mode reconciliation: a program drawing
       in a tight loop never returns to the event machinery, and nothing
       else would publish its damage -- the screen freezes while memory
       animates. Publish from the draw path itself at a beat fast enough
       to read as continuous motion. The beat publishes only: dispatching
       inbound events here would run a compositor resize in the middle of
       the very draw call that raised the beat, freeing the canvas under
       it. Inbound traffic waits for the event loop. Callers hold the
       lock */
    {
        pd_display* d = &thedpy;
        int64_t now = nowms();

        /* Only once the program has been drawing for a beat without
           returning to its event loop, which is the tight loop this is
           for. Fired on the first draw after a quiet spell, it published
           a frame from inside the first call of an ordinary redraw -- a
           status line's fresh ground went out before its text, and the
           line blanked for a frame on every update through a fetch.
           A redraw that returns to the loop within a beat is published
           whole by the flush there. The burst mark is written by the
           flush without a lock; a stale read costs one early beat */
        if (!d->burstms) d->burstms = now;
        if (now-d->burstms >= 33 && now-d->livems >= 33 &&
            !rigenv("PD_NOBEAT", "AMI_WL_NOBEAT"))
            { d->livems = now; beatpump(d); }
    }
}

/* damage from a canvas draw: an owned canvas publishes through its window */
static void candmg(pd_canvas* c, int x, int y, int w, int h)
{
    if (c->owner) windmg(c->owner, x, y, w, h);
}

/* the pixel store: every drawing operation lands here, applying the mix
   function. This one function is why the backend's bitwise color mix
   modes survive the platform change */
static inline void plot(pd_canvas* c, int x, int y, uint32_t col, pd_mix mix)
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
    pd_canvas* c;
    int sx, sy;

    c = dr->stipple;
    if (!c) return (1);
    sx = (x-dr->stipx)%c->w; if (sx < 0) sx += c->w;
    sy = (y-dr->stipy)%c->h; if (sy < 0) sy += c->h;
    return (c->px[(size_t)sy*c->w+sx] != 0);
}

/* Horizontal span, the rasterizer's hot path: clip once, hoist the mix
   function out of the loop, and run the row as bare stores the compiler
   vectorizes. Every area primitive reduces to these rows; this one loop
   is most of the distance between per-pixel dispatch and the fill rates
   a display server bought with the GPU */
static void hspan(pd_canvas* c, pd_draw* dr, int x, int y, int w)
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

/* Full ellipse, filled: the scanline walk. Each row's extent comes from
   the ellipse equation directly -- one sqrt a row against the thousand-
   point polygon and per-row edge sort the general arc path costs */
static void fillellipse(pd_canvas* c, pd_draw* dr, int x, int y, unsigned w,
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

/* Full ellipse, stroked: the annulus between the ellipse dilated and
   eroded by half the line width, walked the same way; two spans a row
   where the hole crosses, one at the caps */
static void ringellipse(pd_canvas* c, pd_draw* dr, int x, int y, unsigned w,
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

/*******************************************************************************

Partial arc scanlines

A pie is the ellipse cut to an angular wedge, a chord the ellipse cut at
the chord line, and a stroked partial arc the annulus cut to the wedge.
Each row the curve contributes intervals and the cut contributes boundary
crossings; testing the midpoint of every division against one predicate
keeps all the sweep geometry in a single place. Angle work runs y-up
about the center, matching the arc convention, with wedge boundaries
along the true endpoint rays (rx cos, ry sin) so noncircular ellipses
cut correctly.

*******************************************************************************/

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

/* emit the kept parts of the center-relative interval [l, r] on row j.
   py is the row's y-up center-relative coordinate, cx the center's
   surface x */
static void cutspans(pd_canvas* c, pd_draw* dr, arccut* ct, int j,
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
static void fillarcpart(pd_canvas* c, pd_draw* dr, int x, int y, unsigned w,
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
static void ringarcpart(pd_canvas* c, pd_draw* dr, int x, int y, unsigned w,
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
static void fillrect(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h)
{
    int j;

    for (j = y; j < y+h; j++) hspan(c, dr, x, j, w);
}

/* Dash pattern lengths by style and width, the values the backend always
   drew with: dashes stretch with the stroke, dots stay near-round */
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
static void thinline(pd_canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)
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
static void fillpoly(pd_canvas* c, pd_draw* dr, ppoint* pt, int n)
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
static void wideline(pd_canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)
{
    float  dx, dy, len, nx, ny, hw;
    ppoint q[4];

    dx = x2-x1; dy = y2-y1;
    len = sqrtf(dx*dx+dy*dy);
    if (len < 0.001f) { fillrect(c, dr, x1-dr->lw/2, y1-dr->lw/2, dr->lw, dr->lw); return; }
    hw = dr->lw/2.0f;
    /* an axis-aligned butt-capped segment is a rectangle: the rows and
       columns the quad's scanlines would produce, as one fill */
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

static void anyline(pd_canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)
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

/*******************************************************************************

Drawing entries

*******************************************************************************/

static int drnoop(pd_draw* dr)
{ return (dr->mix == pd_mixnone); }

void pd_point(pd_canvas* c, pd_draw* dr, int x, int y)
{
    pd_display* d = &thedpy;

    (void)d;
    if (c && !drnoop(dr)) {

        canlk(c);
        plot(c, x, y, dr->fg, dr->mix);
        canulk(c);
        candmg(c, x, y, 1, 1);

    }
}

void pd_line(pd_canvas* c, pd_draw* dr, int x1, int y1, int x2, int y2)
{
    pd_display* d = &thedpy;
    int lx, ly, hx, hy, b;

    (void)d;
    if (c && !drnoop(dr)) {

        canlk(c);
        anyline(c, dr, x1, y1, x2, y2);
        canulk(c);
        lx = x1 < x2? x1: x2; hx = x1 > x2? x1: x2;
        ly = y1 < y2? y1: y2; hy = y1 > y2? y1: y2;
        b = dr->lw/2+1;
        candmg(c, lx-b, ly-b, hx-lx+2*b+1, hy-ly+2*b+1);

    }
}

void pd_rect(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h)
{
    pd_display* d = &thedpy;

    (void)d;
    if (c && !drnoop(dr)) {

        canlk(c);
        if (dr->lw > 1 && dr->lstyle == pd_linesolid && !dr->stipple) {

            /* The frame as four fills with square-joined corners, the
               join the server gave a wide rectangle. Butt-capped side
               lines leave the outer corner squares void. The bands
               partition the frame, so xor and friends touch each pixel
               once */
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
        canulk(c);
        candmg(c, x-dr->lw, y-dr->lw, w+2*dr->lw+1, h+2*dr->lw+1);

    }
}

void pd_frect(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h)
{
    pd_display* d = &thedpy;

    (void)d;
    if (c && !drnoop(dr)) {

        canlk(c);
        fillrect(c, dr, x, y, w, h);
        canulk(c);
        candmg(c, x, y, w, h);

    }
}

void pd_fpoly(pd_canvas* c, pd_draw* dr, const int* xy, int n)
{
    pd_display* d = &thedpy;
    ppoint  stk[64];
    ppoint* pt;
    int     i, lx, ly, hx, hy;

    (void)d;
    if (c && !drnoop(dr) && n > 0) {

        pt = n <= 64? stk: malloc((size_t)n*sizeof(ppoint));
        for (i = 0; i < n; i++) {

            pt[i].x = (short)xy[i*2];
            pt[i].y = (short)xy[i*2+1];

        }
        canlk(c);
        fillpoly(c, dr, pt, n);
        canulk(c);
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

void pd_arc(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h,
            int a1, int a2)
{
    pd_display* d = &thedpy;
    ppoint pt[MAXARC];
    int    n, i;

    (void)d;
    if (c && !drnoop(dr)) {

        canlk(c);
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
        canulk(c);
        candmg(c, x-dr->lw, y-dr->lw, w+2*dr->lw+1, h+2*dr->lw+1);

    }
}

/* filled arc core: pie closes through the center, chord rim to rim */
static void farc(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h,
                 int a1, int a2, int pie)
{
    canlk(c);
    if (abs(a2) >= 360*64)
        /* the complete ellipse walks scanlines directly */
        fillellipse(c, dr, x, y, w, h);
    else fillarcpart(c, dr, x, y, w, h, a1, a2, pie);
    canulk(c);
    candmg(c, x, y, w+1, h+1);
}

void pd_farcpie(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h,
                int a1, int a2)
{
    pd_display* d = &thedpy;

    (void)d;
    if (c && !drnoop(dr)) farc(c, dr, x, y, w, h, a1, a2, 1);
}

void pd_farcchord(pd_canvas* c, pd_draw* dr, int x, int y, int w, int h,
                  int a1, int a2)
{
    pd_display* d = &thedpy;

    (void)d;
    if (c && !drnoop(dr)) farc(c, dr, x, y, w, h, a1, a2, 0);
}

void pd_glyph(pd_canvas* c, pd_draw* dr, int x, int y,
              const uint8_t* mask, int mw, int mh, int stride, int depth)
{
    pd_display* d = &thedpy;
    int i, j, on;

    (void)d;
    if (c && !drnoop(dr) && mask) {

        canlk(c);
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
        canulk(c);
        candmg(c, x, y, mw, mh);

    }
}

void pd_blit(pd_canvas* dst, int dx, int dy,
             pd_canvas* src, int sx, int sy, int w, int h)
{
    pd_display* d = &thedpy;
    int iw, ih, y;

    (void)d;
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

            canlk2(dst, src);
            for (y = 0; y < ih; y++)
                memcpy(&dst->px[(size_t)(dy+y)*dst->w+dx],
                       &src->px[(size_t)(sy+y)*src->w+sx], (size_t)iw*4);
            canulk2(dst, src);
            candmg(dst, dx, dy, iw, ih);

        }

    }
}

void pd_scroll(pd_canvas* c, int dx, int dy, int sx, int sy, int w, int h)
{
    pd_display* d = &thedpy;
    int iw, ih, y, ydir;

    (void)d;
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
            canlk(c);
            for (y = ydir > 0? 0: ih-1; ydir > 0? y < ih: y >= 0; y += ydir)
                memmove(&c->px[(size_t)(dy+y)*c->w+dx],
                        &c->px[(size_t)(sy+y)*c->w+sx], (size_t)iw*4);
            canulk(c);
            candmg(c, dx, dy, iw, ih);

        }

    }
}

void pd_stretch(pd_canvas* dst, int dx, int dy, int dw, int dh,
                pd_canvas* src, int sx, int sy, int sw, int sh)
{
    pd_display* d = &thedpy;
    int   x, y, fx, fy;
    float xr, yr;

    (void)d;
    if (src && dst && dw > 0 && dh > 0 && sw > 0 && sh > 0) {

        xr = (float)sw/dw;
        yr = (float)sh/dh;
        canlk2(dst, src);
        for (y = 0; y < dh; y++) {

            if (dy+y < 0 || dy+y >= dst->h) continue;
            fy = sy+(int)(yr*y);
            if (fy < 0) fy = 0;
            if (fy >= src->h) fy = src->h-1;
            for (x = 0; x < dw; x++) {

                if (dx+x < 0 || dx+x >= dst->w) continue;
                fx = sx+(int)(xr*x);
                if (fx < 0) fx = 0;
                if (fx >= src->w) fx = src->w-1;
                dst->px[(size_t)(dy+y)*dst->w+dx+x] =
                    src->px[(size_t)fy*src->w+fx];

            }

        }
        canulk2(dst, src);
        candmg(dst, dx, dy, dw, dh);

    }
}

/*******************************************************************************

Canvas entries

*******************************************************************************/

pd_canvas* pd_cannew(pd_display* d, int w, int h)
{
    pd_canvas* c;

    (void)d;
    c = newcanvas(w, h, 32);
    return (c);
}

void pd_candel(pd_canvas* c)
{
    pd_display* d = &thedpy;

    (void)d;
    freecanvas(c);
}

pd_canvas* pd_wincanvas(pd_win* w)
{
    pd_display* d = &thedpy;
    pd_canvas*  c;

    (void)d;
    /* the pointer under the tree's read lock; a first use makes the
       canvas under the write lock */
    TREERD();
    c = w->can;
    TREEUN();
    if (!c) {

        TREEWR();
        c = wincanvas(w);
        TREEUN();

    }
    return (c);
}

void pd_cansize(pd_canvas* c, int* w, int* h)
{
    *w = c->w;
    *h = c->h;
}

/* the stride returned counts 32-bit pixels per row */
uint32_t* pd_canlock(pd_canvas* c, int* stride)
{
    canlk(c); /* the canvas's own lock, held until the unlock */
    *stride = c->w;
    return (c->px);
}

void pd_canunlock(pd_canvas* c)
{
    /* writes through the raw pointer publish with the unlock */
    canulk(c);
    candmg(c, 0, 0, c->w, c->h);
}

/*******************************************************************************

Window tree

*******************************************************************************/

/* stack child on top (initial placement) */
static void linkchild(pd_win* parent, pd_win* w)
{
    pd_win** p;

    w->parent = parent;
    w->sibnext = NULL;
    p = &parent->childs;
    while (*p) p = &(*p)->sibnext;
    *p = w;
}

static void unlinkchild(pd_win* w)
{
    pd_win** p;

    if (!w->parent) return;
    p = &w->parent->childs;
    while (*p && *p != w) p = &(*p)->sibnext;
    if (*p) *p = w->sibnext;
    w->sibnext = NULL;
}

/* the toplevel above a window (the root child containing it) */
static pd_win* winTop(pd_win* w)
{
    if (!w || !w->parent) return (NULL);
    while (w->parent && w->parent->parent) w = w->parent;
    return (w->top? w: NULL);
}

/* surface coordinates of a window's origin */
static void winorg(pd_win* w, int* x, int* y)
{
    *x = 0; *y = 0;
    while (w && w->parent) { *x += w->x; *y += w->y; w = w->parent; }
}

/* deepest mapped window containing surface point; xo/yo receive the
   window-local coordinates */
static pd_win* hittest(pd_win* top, int sx, int sy, int* xo, int* yo)
{
    pd_win* w;
    pd_win* c;
    pd_win* best;
    int     bx, by;

    w = top; bx = sx; by = sy;
    for (;;) {

        best = NULL;
        /* later siblings are higher in the stack; take the last hit */
        for (c = w->childs; c; c = c->sibnext)
            if (c->mapped && bx >= c->x && by >= c->y &&
                bx < c->x+c->w && by < c->y+c->h) best = c;
        if (!best) break;
        bx -= best->x; by -= best->y;
        w = best;

    }
    *xo = bx; *yo = by;
    return (w);
}

/* full recomposition of the toplevel above a window. The toplevel is
   found under the tree's read lock and released before its own lock is
   taken: the toplevel's lock is never taken under the tree's */
static void topfulldmg(pd_win* w)
{
    pd_win* t;
    wltop*  tp;
    int     tw, th;

    TREERD();
    t = winTop(w);
    if (!t && w->top) t = w;
    tp = t? t->top: NULL;
    tw = t? t->w: 0; th = t? t->h: 0;
    TREEUN();
    if (tp) {

        dmglk(tp);
        tp->dmg = 1; tp->dx1 = 0; tp->dy1 = 0;
        tp->dx2 = tw; tp->dy2 = th;
        dmgulk(tp);

    }
}

/* a window's toplevel record, under the tree's read lock */
static wltop* wintopof(pd_win* w)
{
    wltop* tp;

    TREERD();
    tp = w->top;
    TREEUN();
    return (tp);
}

pd_win* pd_winnew(pd_display* d, pd_win* parent, int x, int y, int w, int h)
{
    pd_win* win;

    win = calloc(1, sizeof(pd_win));
    win->x = x; win->y = y;
    win->w = w? w: 1; win->h = h? h: 1;
    win->cursor = -1;
    TREEWR();
    linkchild(parent? parent: &d->root, win);
    TREEUN();
    return (win);
}

/* Take a toplevel down. Detached from the window under the tree's write
   lock first, so no flush walk finds it after; then its protocol objects
   are destroyed under the connection lock, which the dispatch of their
   remaining events also holds, and its record freed */
static void droptop(pd_display* d, pd_win* w)
{
    wltop* t;
    int    i;

    TREEWR();
    t = w->top;
    w->top = NULL;
    TREEUN();
    if (!t) return;
    LK(d);
    toplk(t);
    if (t->fcb) wl_callback_destroy(t->fcb);
    for (i = 0; i < 2; i++)
        if (t->buf[i]) {

            wl_buffer_destroy(t->buf[i]);
            munmap(t->bufpx[i], t->bufsz[i]);

        }
    if (t->xtop) xdg_toplevel_destroy(t->xtop);
    if (t->xsurf) xdg_surface_destroy(t->xsurf);
    if (t->surf) wl_surface_destroy(t->surf);
    wl_display_flush(d->dpy);
    t->win = NULL; /* a compose finding this composes nothing */
    topulk(t);
    ULK(d);
    topunref(t); /* the window's reference; a flush walk's may remain */
}

void pd_windel(pd_win* win)
{
    pd_display* d = &thedpy;
    pd_win* c;

    for (;;) {

        TREERD();
        c = win->childs;
        TREEUN();
        if (!c) break;
        pd_windel(c);

    }
    LK(d); /* the seat's references to it */
    if (d->ptrwin == win) d->ptrwin = NULL;
    if (d->ptrtop == win) d->ptrtop = NULL;
    if (d->focus == win) d->focus = NULL;
    if (d->kbdtop == win) d->kbdtop = NULL;
    if (d->grab == win) d->grab = NULL;
    if (d->igrab == win) d->igrab = NULL;
    ULK(d);
    droptop(d, win);
    TREEWR(); /* out of the tree: no walk reaches it after this */
    unlinkchild(win);
    TREEUN();
    if (win->can) { canlk(win->can); canulk(win->can); } /* a use in flight ends */
    purgewin(d, win); /* its queued events: a handle about to be freed */
    freecanvas(win->can);
    free(win->title);
    free(win);
}

void pd_winmap(pd_win* win, int visible)
{
    pd_display* d = &thedpy;
    pd_evt e;

    int istop, w, h, was, hastop;

    TREEWR();
    was = win->mapped;
    istop = win->parent == &d->root;
    hastop = win->top != NULL;
    w = win->w; h = win->h;
    if (visible && !was) win->mapped = 1;
    else if (!visible && was) win->mapped = 0;
    TREEUN();
    if (visible && !was) {

        if (istop) {

            /* toplevel: begin the map handshake; pd_etmap follows the
               first configure */
            if (!hastop) { LK(d); mktoplevel(d, win); ULK(d); }

        } else {

            /* child: immediate */
            mkevt(&e, pd_etmap, win);
            enq(d, &e);
            mkevt(&e, pd_etredraw, win);
            e.rw = w; e.rh = h;
            enq(d, &e);
            windmg(win, 0, 0, w, h);

        }

    } else if (!visible && was) {

        if (istop) droptop(d, win);
        else topfulldmg(win);

    }
}

void pd_winmove(pd_win* win, int x, int y)
{
    pd_display* d = &thedpy;

    int child;

    TREEWR();
    win->x = x; win->y = y;
    child = win->parent != &d->root;
    TREEUN();
    /* a toplevel records the wish; the compositor places toplevels. A
       child moves within its parent, and the ancestor surface recomposes */
    if (child) topfulldmg(win);
}

unsigned long pd_winsize(pd_win* win, int width, int height)
{
    pd_display* d = &thedpy;
    unsigned long tok;
    pd_evt e;
    int ow, oh;

    wltop* tp;

    tok = __atomic_add_fetch(&d->sizetok, 1, __ATOMIC_RELAXED);
    TREEWR(); /* the geometry and the canvas */
    ow = win->w; oh = win->h;
    win->w = width; win->h = height;
    sizecanvas(win, width, height);
    tp = win->parent == &d->root? win->top: NULL;
    TREEUN();
    if (tp) { toplk(tp); sizebufs(d, win); topulk(tp); } /* its buffers */
    topfulldmg(win);
    /* the answering resize, carrying the caller's token */
    mkevt(&e, pd_etresize, win);
    e.w = width; e.h = height;
    e.token = tok;
    enq(d, &e);
    expodmg(d, win, ow, oh);
    return (tok);
}

void pd_winraise(pd_win* win)
{
    pd_display* d = &thedpy;

    int child;

    TREEWR();
    child = win->parent && win->parent != &d->root;
    if (child) {

        unlinkchild(win);
        linkchild(win->parent, win);

    }
    TREEUN();
    if (child) topfulldmg(win);
}

void pd_winlower(pd_win* win)
{
    pd_display* d = &thedpy;

    int child;

    TREEWR();
    child = win->parent && win->parent != &d->root;
    if (child) {

        unlinkchild(win);
        win->sibnext = win->parent->childs;
        win->parent->childs = win;

    }
    TREEUN();
    if (child) topfulldmg(win);
}

void pd_wingeom(pd_win* win, int* x, int* y, int* width, int* height,
                int* mapped)
{
    pd_display* d = &thedpy;
    pd_win* p;
    int     m;

    TREERD();
    if (win == &d->root) {

        if (x) *x = 0;
        if (y) *y = 0;
        if (width) *width = d->outw;
        if (height) *height = d->outh;
        if (mapped) *mapped = 1;

    } else {

        if (x) *x = win->x;
        if (y) *y = win->y;
        if (width) *width = win->w;
        if (height) *height = win->h;
        m = 1;
        for (p = win; p && p != &d->root; p = p->parent)
            if (!p->mapped) m = 0;
        if (mapped) *mapped = m;

    }
    TREEUN();
}

void pd_winlimits(pd_win* win, int minw, int minh, int maxw, int maxh)
{
    pd_display* d = &thedpy;

    wltop* tp = wintopof(win);

    if (tp && tp->xtop) {

        xdg_toplevel_set_min_size(tp->xtop,
            (minw+d->scale-1)/d->scale, (minh+d->scale-1)/d->scale);
        xdg_toplevel_set_max_size(tp->xtop,
            (maxw+d->scale-1)/d->scale, (maxh+d->scale-1)/d->scale);
        wl_display_flush(d->dpy);

    }
}

void pd_grab(pd_win* win, int on)
{
    pd_display* d = &thedpy;

    LK(d);
    d->grab = on? win: NULL;
    ULK(d);
}

int pd_pointer(pd_win* win, int* x, int* y)
{
    pd_display* d = &thedpy;
    int ox, oy, in;

    LK(d); /* the pointer's place */
    TREERD(); /* the window's */
    winorg(win, &ox, &oy);
    *x = (int)d->ptrx-ox;
    *y = (int)d->ptry-oy;
    in = d->ptrtop && winTop(win) == d->ptrtop &&
         *x >= 0 && *y >= 0 && *x < win->w && *y < win->h;
    TREEUN();
    ULK(d);
    return (in);
}

void pd_wintitle(pd_win* win, const char* title)
{
    pd_display* d = &thedpy;

    wltop* tp;
    char*  old;

    TREEWR(); /* the window's title */
    old = win->title;
    win->title = strdup(title? title: "");
    tp = win->top;
    if (tp && tp->xtop) xdg_toplevel_set_title(tp->xtop, win->title);
    TREEUN();
    free(old);
}

void pd_winframe(pd_win* win, int titx, int tity, int titw, int tith,
                 int borderw)
{
    pd_display* d = &thedpy;

    wltop* tp = wintopof(win);

    if (tp) {

        toplk(tp);
        tp->titx = titx; tp->tity = tity;
        tp->titw = titw; tp->tith = tith;
        tp->borderw = borderw;
        topulk(tp);

    }
}

void pd_minimize(pd_win* win)
{
    pd_display* d = &thedpy;
    pd_evt e;

    wltop* tp = wintopof(win);

    if (tp && tp->xtop) {

        xdg_toplevel_set_minimized(tp->xtop);
        wl_display_flush(d->dpy);
        /* the shell reports nothing back for minimize; the caller learns
           of its own request here */
        mkevt(&e, pd_etmin, win);
        enq(d, &e);

    }
}

void pd_maximize(pd_win* win, int on)
{
    pd_display* d = &thedpy;

    wltop* tp = wintopof(win);

    if (tp && tp->xtop) {

        if (on) xdg_toplevel_set_maximized(tp->xtop);
        else xdg_toplevel_unset_maximized(tp->xtop);
        wl_display_flush(d->dpy);

    }
}

void pd_windrag(pd_win* win)
{
    pd_display* d = &thedpy;

    wltop* tp = wintopof(win);

    if (tp && tp->xtop) {

        /* the compositor owns the move from here; the press that started
           it releases through the implicit grab when the pointer leaves */
        LK(d); /* the seat and the serial */
        xdg_toplevel_move(tp->xtop, d->seat, d->inserial);
        ULK(d);
        wl_display_flush(d->dpy);

    }
}

void pd_cursor(pd_win* win, pd_curshape shape)
{
    pd_display* d = &thedpy;

    int inside;

    LK(d); /* the shape is seat state: setcursor reads it under this lock */
    win->cursor = shape;
    /* the visible cursor changes immediately when the pointer is inside;
       the frame hover cursors depend on it */
    TREERD();
    inside = d->ptrwin == win || (d->ptrwin && winTop(d->ptrwin) == win);
    TREEUN();
    if (inside) setcursor(d, d->ptrwin);
    ULK(d);
}

/*******************************************************************************

Toplevel surface management

*******************************************************************************/

static void framedone(void* data, struct wl_callback* cb, uint32_t t);

static const struct wl_callback_listener frame_lis = { framedone };

static void bufrelease(void* data, struct wl_buffer* b)
{
    wltop* t = data;
    int    i;

    toplk(t);
    for (i = 0; i < 2; i++) if (t->buf[i] == b) t->bufbusy[i] = 0;
    topulk(t);
}

static const struct wl_buffer_listener buf_lis = { bufrelease };

static int mkshmfd(size_t size)
{
    int fd;

    fd = memfd_create("ami-pd", 0);
    if (fd < 0) return (-1);
    if (ftruncate(fd, size) < 0) { close(fd); return (-1); }
    return (fd);
}

/* (re)create the buffer pair at the window's size */
static void sizebufs(pd_display* d, pd_win* win)
{
    wltop* t;
    int    i, fd;
    size_t sz;
    struct wl_shm_pool* pool;

    t = win->top;
    if (!t) return;
    if (t->bufw >= win->w && t->bufh >= win->h &&
        t->bufw-win->w < d->scale && t->bufh-win->h < d->scale &&
        t->buf[0]) return;
    for (i = 0; i < 2; i++)
        if (t->buf[i]) {

            wl_buffer_destroy(t->buf[i]);
            munmap(t->bufpx[i], t->bufsz[i]);
            t->buf[i] = NULL;

        }
    /* the buffer must be a whole number of logical pixels on a side */
    t->bufw = (win->w+d->scale-1)/d->scale*d->scale;
    t->bufh = (win->h+d->scale-1)/d->scale*d->scale;
    sz = (size_t)t->bufw*t->bufh*4;
    for (i = 0; i < 2; i++) {

        fd = mkshmfd(sz);
        if (fd < 0) return;
        t->bufpx[i] = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        t->bufsz[i] = sz;
        /* mmap's zero pages leave the padding rows and columns
           transparent, invisible beside the rounded corners */
        pool = wl_shm_create_pool(d->shm, fd, sz);
        t->buf[i] = wl_shm_pool_create_buffer(pool, 0, t->bufw, t->bufh,
                                              t->bufw*4, WL_SHM_FORMAT_ARGB8888);
        wl_proxy_set_queue((struct wl_proxy*)t->buf[i], d->bufq);
        wl_buffer_add_listener(t->buf[i], &buf_lis, t);
        wl_shm_pool_destroy(pool);
        close(fd);
        t->bufbusy[i] = 0;

    }
    /* full redraw next commit */
    dmglk(t);
    t->dmg = 1; t->dx1 = 0; t->dy1 = 0; t->dx2 = win->w; t->dy2 = win->h;
    dmgulk(t);
}

/* recursive blit of the window tree into the composition buffer */
static void blittree(pd_win* w, uint32_t* dst, int dw, int dh, int ox, int oy,
                     int cx1, int cy1, int cx2, int cy2)
{
    pd_win*    c;
    pd_canvas* cv;
    int        x1, y1, x2, y2, y;

    if (!w->mapped && w->parent) return;
    cv = w->can;
    if (cv) {

        x1 = ox; y1 = oy; x2 = ox+w->w; y2 = oy+w->h;
        if (x1 < cx1) x1 = cx1;
        if (y1 < cy1) y1 = cy1;
        if (x2 > cx2) x2 = cx2;
        if (y2 > cy2) y2 = cy2;
        if (x2 > x1 && y2 > y1) {

            canlk(cv); /* its rows, whole */
            for (y = y1; y < y2; y++)
                memcpy(&dst[(size_t)y*dw+x1],
                       &cv->px[(size_t)(y-oy)*cv->w+(x1-ox)],
                       (size_t)(x2-x1)*4);
            canulk(cv);

        }

    }
    for (c = w->childs; c; c = c->sibnext)
        blittree(c, dst, dw, dh, ox+c->x, oy+c->y, cx1, cy1, cx2, cy2);
}

/* Content pixels carry no alpha; the buffer format does. Stamp the
   damaged rows opaque */
static void alphaover(uint32_t* px, int stride, int x1, int y1, int x2,
                      int y2)
{
    uint32_t* p;
    int       x, y;

    for (y = y1; y < y2; y++) {

        p = &px[(size_t)y*stride+x1];
        for (x = x1; x < x2; x++) *p++ |= 0xff000000;

    }
}

/* Round the frame's corners: pixels outside the corner radius go
   transparent, the edge covered proportionally. The format is
   premultiplied, so coverage scales the color channels with the
   alpha */
static void roundcorners(pd_display* d, pd_win* win, uint32_t* px,
                         int stride, int x1, int y1, int x2, int y2)
{
    double rad = (double)CORNERRAD*d->scale;
    double cx, cy, dx, dy, dist, cov;
    int    corner, bx, by, x, y, a;
    uint32_t c;

    for (corner = 0; corner < 4; corner++) {

        bx = (corner&1)? win->w-(int)rad: 0;
        by = (corner&2)? win->h-(int)rad: 0;
        cx = (corner&1)? win->w-rad: rad;
        cy = (corner&2)? win->h-rad: rad;
        for (y = by; y < by+(int)rad; y++) {

            if (y < y1 || y >= y2) continue;
            for (x = bx; x < bx+(int)rad; x++) {

                if (x < x1 || x >= x2) continue;
                dx = x+0.5-cx; dy = y+0.5-cy;
                dist = sqrt(dx*dx+dy*dy);
                cov = rad-dist+0.5;
                if (cov >= 1.0) continue;
                if (cov <= 0) { px[(size_t)y*stride+x] = 0; continue; }
                a = (int)(cov*255.0+0.5);
                c = px[(size_t)y*stride+x];
                px[(size_t)y*stride+x] =
                    ((uint32_t)a<<24)|
                    ((((c>>16&0xff)*a/255))<<16)|
                    ((((c>>8&0xff)*a/255))<<8)|
                    ((c&0xff)*a/255);

            }

        }

    }
}

/* the corner cut decides per commit: an Ami-framed toplevel rounds,
   a maximized one squares */
static int roundon(wltop* t)
{
    return ((t->titw > 0 || t->borderw > 0) && !t->maximized);
}

/*******************************************************************************

Snapshot a window

Composes a window and everything below it into a buffer of its own and
hands it back, the picture as it would be presented. This is what a
screen capture wants: the client draws into canvases, one per window, and
only the composition carries the widgets, menus and child frames that
stand on top of a window. The caller frees the buffer. The rounded
corners a toplevel wears on screen are left off: they are a property of
the desktop it sits on, not of what the program drew.

*******************************************************************************/

uint32_t* pd_winsnap(pd_display* d, pd_win* win, int* width, int* height)
{
    uint32_t* px;
    int       w, h;

    if (!d || !win) return (NULL);
    TREERD(); /* the tree below the window, and each canvas as it is read */
    w = win->w;
    h = win->h;
    if (w <= 0 || h <= 0) { TREEUN(); return (NULL); }
    px = calloc((size_t)w*h, sizeof(uint32_t));
    if (px) {

        blittree(win, px, w, h, 0, 0, 0, 0, w, h);
        alphaover(px, w, 0, 0, w, h);

    }
    TREEUN();
    if (width) *width = w;
    if (height) *height = h;

    return (px);
}

/* compose and commit a toplevel's damage */
/* the flattened tree of a toplevel: what a compose blits, taken under the
   tree's read lock and used after it is released */
typedef struct {
    pd_canvas* cv;
    int        ox, oy, w, h;
} blitent;
#define MAXBLIT 512 /* windows a compose can carry; more are left out */

static int flatten(pd_win* w, int ox, int oy, blitent* out, int n)
{
    pd_win* c;

    if (!w->mapped && w->parent) return (n);
    if (w->can && n < MAXBLIT) {

        out[n].cv = w->can; out[n].ox = ox; out[n].oy = oy;
        out[n].w = w->w; out[n].h = w->h;
        n++;

    }
    for (c = w->childs; c; c = c->sibnext)
        n = flatten(c, ox+c->x, oy+c->y, out, n);
    return (n);
}

static int canvcmp(const void* a, const void* b)
{
    const pd_canvas* const* x = a; const pd_canvas* const* y = b;
    return (*x < *y? -1: *x > *y? 1: 0);
}

/* blit the flattened tree into the composition buffer, clipped; the
   canvases are held by the caller */
static void blitlist(blitent* l, int n, uint32_t* dst, int dw,
                     int cx1, int cy1, int cx2, int cy2)
{
    int i, x1, y1, x2, y2, y;
    pd_canvas* cv;

    for (i = 0; i < n; i++) {

        cv = l[i].cv;
        x1 = l[i].ox; y1 = l[i].oy; x2 = l[i].ox+l[i].w; y2 = l[i].oy+l[i].h;
        if (x1 < cx1) x1 = cx1;
        if (y1 < cy1) y1 = cy1;
        if (x2 > cx2) x2 = cx2;
        if (y2 > cy2) y2 = cy2;
        if (x2 > x1 && y2 > y1)
            for (y = y1; y < y2; y++)
                memcpy(&dst[(size_t)y*dw+x1],
                       &cv->px[(size_t)(y-l[i].oy)*cv->w+(x1-l[i].ox)],
                       (size_t)(x2-x1)*4);

    }
}

static void compose(pd_display* d, wltop* t)
{
    pd_win* win;
    int     b;
    int     x1, y1, x2, y2;
    int     ww, wh, mapped, dmg;
    blitent list[MAXBLIT];
    pd_canvas* lk[MAXBLIT]; /* the canvases in address order, for the locks */
    int     n, i, k;

    /* Under the toplevel's lock throughout: its buffers and callback are
       its own. The tree is read only to flatten it, and the canvases of
       the flattened list are locked in address order, the order every
       taker of more than one uses, so the blit runs holding no lock but
       theirs. The damage rectangle is under its own small lock, so a
       drawing thread's union never waits for this. */
    if (!t) return;
    toplk(t);
    win = t->win;
    if (!win) { topulk(t); return; } /* dropped */
    TREERD();
    ww = win->w; wh = win->h; mapped = win->mapped;
    TREEUN();
    dmglk(t);
    dmg = t->dmg;
    x1 = t->dx1; y1 = t->dy1; x2 = t->dx2; y2 = t->dy2;
    dmgulk(t);
    if (!t->configured || !mapped || !dmg) { topulk(t); return; }
    /* pace to the frame callback, for one refresh: while a callback is
       outstanding and younger than a refresh, the frame in flight is as
       fresh as the screen can show; the damage waits, and the callback
       composes it (framedone). Past a refresh with no callback the
       mailbox behavior below stands, a late frame replaced by a fresher
       one. Without this, a window the compositor is not showing, whose
       buffers come straight back and whose callbacks never come, was
       recomposed in full on every poll of the display */
    if (t->fcb && nowms()-t->fcbtime < PACEMS) { t->commitpend = 1; topulk(t); return; }
    /* a resize mid-application holds its commit; the bailout keeps a
       caller that never finishes from freezing the window */
    if (t->applying) {

        if (nowms()-t->applyms < 50) { t->commitpend = 1; topulk(t); return; }
        t->applying = 0;

    }
    sizebufs(d, win);
    b = t->curbuf;
    if (t->bufbusy[b]) b = 1-b;
    if (t->bufbusy[b]) { t->commitpend = 1; topulk(t); return; }
    if (!t->buf[b]) { topulk(t); return; }
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > ww) x2 = ww;
    if (y2 > wh) y2 = wh;
    /* both buffers must carry the history; compose damage into both is
       avoided by composing the union of this and the other buffer's staleness:
       simplest correct form, compose full frame when switching buffers */
    if (b != t->curbuf) { x1 = 0; y1 = 0; x2 = ww; y2 = wh; }
    /* the damage is taken: what lands from here is the next frame's */
    dmglk(t);
    t->dmg = 0;
    dmgulk(t);
    if (x2 <= x1 || y2 <= y1) { topulk(t); return; }
    TREERD(); /* the tree below, flattened, and its canvases taken */
    n = flatten(win, 0, 0, list, 0);
    /* the locks in address order, the order every taker of several uses;
       the list itself stays in tree order, parents before children, which
       is the order the blit needs: sorted, a parent's canvas landed over
       its child's and the content vanished under the background */
    for (i = 0; i < n; i++) lk[i] = list[i].cv;
    qsort(lk, n, sizeof(pd_canvas*), canvcmp);
    for (i = 0; i < n; i++) if (!i || lk[i] != lk[i-1]) canlk(lk[i]);
    TREEUN();
    blitlist(list, n, t->bufpx[b], t->bufw, x1, y1, x2, y2);
    alphaover(t->bufpx[b], t->bufw, x1, y1, x2, y2);
    if (roundon(t))
        roundcorners(d, win, t->bufpx[b], t->bufw, x1, y1, x2, y2);
    /* The corner boxes recompose every commit: a corner's pixels mix
       content and rounding state settled at different moments, and a
       stale corner survives any damage that misses it. Four small
       blits buy corners that are always current */
    {
        int rad = CORNERRAD*d->scale;
        int bx, by, bx2, by2;

        for (k = 0; k < 4; k++) {

            bx = (k&1)? ww-rad: 0;
            by = (k&2)? wh-rad: 0;
            if (bx < 0) bx = 0;
            if (by < 0) by = 0;
            bx2 = bx+rad > ww? ww: bx+rad;
            by2 = by+rad > wh? wh: by+rad;
            if (bx2 <= bx || by2 <= by) continue;
            blitlist(list, n, t->bufpx[b], t->bufw, bx, by, bx2, by2);
            alphaover(t->bufpx[b], t->bufw, bx, by, bx2, by2);
            if (roundon(t))
                roundcorners(d, win, t->bufpx[b], t->bufw, bx, by,
                             bx2, by2);
            wl_surface_damage_buffer(t->surf, bx, by, bx2-bx, by2-by);

        }
    }
    for (i = 0; i < n; i++) if (!i || lk[i] != lk[i-1]) canulk(lk[i]);
    wl_surface_attach(t->surf, t->buf[b], 0, 0);
    wl_surface_damage_buffer(t->surf, x1, y1, x2-x1, y2-y1);
    /* Mailbox presentation: a free buffer commits at once, and the
       compositor shows the freshest frame each refresh. One pacing
       callback stays armed at a time; strict wait-for-callback pacing
       can phase-lock the whole draw loop to half rate when commits
       land just past the compositor's deadline */
    if (!t->fcb) {

        t->fcb = wl_surface_frame(t->surf);
        t->fcbtime = nowms();
        wl_callback_add_listener(t->fcb, &frame_lis, win);

    }
    wl_surface_commit(t->surf);
    t->bufbusy[b] = 1;
    t->curbuf = b;
    t->commitpend = 0;
    /* the rig's capture path: record exactly what was committed. Written
       here, a teardown flush cannot overwrite a good frame with the
       dismantled tree */
    {
        const char* dump = rigenv("PD_DUMP", "AMI_WL_DUMP");
        char fn[256];
        FILE* f;
        int x, y;
        uint32_t p;
        pd_win* rc;

        if (dump) {

            i = 0;
            TREERD();
            for (rc = d->root.childs; rc && rc != win; rc = rc->sibnext) i++;
            TREEUN();
            /* one file per commit: teardown commits a black frame as the
               windows dismantle, and sequencing keeps every earlier frame
               inspectable */
            snprintf(fn, sizeof(fn), "%s-%d-%04d.ppm", dump, i,
                     d->dumpseq++);
            f = fopen(fn, "w");
            if (f) {

                fprintf(f, "P6 %d %d 255\n", t->bufw, t->bufh);
                for (y = 0; y < t->bufh; y++)
                    for (x = 0; x < t->bufw; x++) {

                        p = t->bufpx[b][(size_t)y*t->bufw+x];
                        fputc(p>>16&0xff, f); fputc(p>>8&0xff, f);
                        fputc(p&0xff, f);

                    }
                fclose(f);

            }

        }
    }
    topulk(t);
}

static void framedone(void* data, struct wl_callback* cb, uint32_t tm)
{
    pd_win* win = data;
    pd_display* d = &thedpy;
    pd_evt e;

    (void)tm;
    wltop* t = wintopof(win);

    /* The callback is destroyed here only while the toplevel still holds
       it. A flush's fallback on another thread, or the drop, destroys the
       one it holds and clears it under the toplevel's lock; a done event
       already in flight for that callback then arrives here, and a second
       destroy would take libwayland's reference count below zero */
    if (t) {
        toplk(t);
        if (t->fcb == cb) { t->fcb = NULL; wl_callback_destroy(cb); }
        if (t->commitpend || t->dmg) {
            dmglk(t); t->dmg = 1; dmgulk(t);
            compose(d, t);
        }
        topulk(t);
    }
    /* the compositor's frame pacing, surfaced to the client when asked:
       this is the true beginning-of-refresh signal for this surface */
    if (win->frmevt) {

        mkevt(&e, pd_etframe, win);
        enq(d, &e);

    }
}

static void flushtops(pd_display* d)
{
    pd_win* c;
    wltop*  held[MAXBLIT];
    int     n, i;

    /* the toplevels with damage, taken under the tree's read lock and
       held by reference, so the composes run with the tree free for the
       window operations of other threads */
    n = 0;
    TREERD();
    for (c = d->root.childs; c && n < MAXBLIT; c = c->sibnext)
        if (c->top && c->top->dmg) { topref(c->top); held[n++] = c->top; }
    TREEUN();
    for (i = 0; i < n; i++) {

        /* A compositor throttles a surface it is not showing by
           withholding frame callbacks; damage must still land
           eventually or a hidden window never updates and a
           callback-less compositor wedges everything. Past a beat,
           commit without the pacing */
        toplk(held[i]);
        if (held[i]->fcb && nowms()-held[i]->fcbtime > 100) {

            wl_callback_destroy(held[i]->fcb);
            held[i]->fcb = NULL;

        }
        topulk(held[i]);
        compose(d, held[i]);
        topunref(held[i]);

    }
    wl_display_flush(d->dpy);
}

/* flush one window's toplevel: its damage composed and committed now */
void pd_winflush(pd_win* w)
{
    pd_display* d = &thedpy;
    pd_win*     top;
    wltop*      t;

    TREERD();
    top = winTop(w);
    if (!top && w && w->top) top = w;
    t = top? top->top: NULL;
    if (t) topref(t);
    TREEUN();
    if (t) {

        compose(d, t);
        topunref(t);

    }
    wl_display_flush(d->dpy);
}

/* xdg surface configure */
static void xsconf(void* data, struct xdg_surface* s, uint32_t serial)
{
    pd_win*     win = data;
    pd_display* d = &thedpy;
    wltop*      t;
    pd_evt      e;
    int         ow, oh;

    t = wintopof(win);
    if (!t) return;
    xdg_surface_ack_configure(s, serial);
    toplk(t);
    t->configured = 1;
    TREERD();
    ow = win->w; oh = win->h;
    TREEUN();
    if (t->confw > 0 && t->confh > 0 &&
        (t->confw != ow || t->confh != oh)) {

        /* The compositor resized us. Apply the size before reporting, so
           the caller reacts to a fait accompli, and the resize carries
           token zero: the compositor originated it. Commits hold until
           the caller finishes reconfiguring (frame, children, chrome):
           publishing the half-applied state would hand the compositor
           interim sizes and make its resize anchoring wander */
        TREEWR(); /* the geometry and the canvas */
        win->w = t->confw; win->h = t->confh;
        sizecanvas(win, win->w, win->h);
        TREEUN();
        t->applying = 1;
        t->applyms = nowms();
        sizebufs(d, win);
        mkevt(&e, pd_etresize, win);
        e.w = t->confw; e.h = t->confh;
        enq(d, &e);
        expodmg(d, win, ow, oh);

    }
    if (!t->mapnoted) {

        t->mapnoted = 1;
        mkevt(&e, pd_etmap, win);
        enq(d, &e);
        mkevt(&e, pd_etredraw, win);
        e.rw = win->w; e.rh = win->h;
        enq(d, &e);
        /* first content: whatever has been drawn shows now */
        TREERD();
        ow = win->w; oh = win->h;
        TREEUN();
        dmglk(t);
        t->dmg = 1; t->dx1 = 0; t->dy1 = 0; t->dx2 = ow; t->dy2 = oh;
        dmgulk(t);
        compose(d, t);

    }
    topulk(t);
}

static const struct xdg_surface_listener xsurf_lis = { xsconf };

static void xtconf(void* data, struct xdg_toplevel* xt, int32_t w, int32_t h,
                   struct wl_array* states)
{
    pd_win*     win = data;
    pd_display* d = &thedpy;
    wltop*      t;
    uint32_t*   st;
    int         act, max;
    pd_evt      e;

    (void)xt;
    t = wintopof(win);
    if (!t) return;
    toplk(t);
    t->confw = w*d->scale; t->confh = h*d->scale;
    act = 0; max = 0;
    wl_array_for_each(st, states) {

        if (*st == XDG_TOPLEVEL_STATE_ACTIVATED) act = 1;
        if (*st == XDG_TOPLEVEL_STATE_MAXIMIZED) max = 1;

    }
    if (max != t->maximized) {

        t->maximized = max;
        mkevt(&e, max? pd_etmax: pd_etrestore, win);
        enq(d, &e);

    }
    if (act != t->activated) {

        t->activated = act;
        mkevt(&e, act? pd_etfocus: pd_etnofocus, win);
        enq(d, &e);

    }
    topulk(t);
}

static void xtclose(void* data, struct xdg_toplevel* xt)
{
    pd_win*     win = data;
    pd_display* d = &thedpy;
    pd_evt      e;

    (void)xt;
    mkevt(&e, pd_etclose, win);
    enq(d, &e);
}

static const struct xdg_toplevel_listener xtop_lis = { xtconf, xtclose };

static void mktoplevel(pd_display* d, pd_win* win)
{
    wltop* t;

    t = calloc(1, sizeof(wltop));
    reclock(&t->lk);
    pthread_mutex_init(&t->dlk, NULL);
    t->refs = 1; /* the window's */
    t->win = win;
    TREEWR();
    win->top = t;
    TREEUN();
    t->surf = wl_compositor_create_surface(d->comp);
    wl_surface_set_user_data(t->surf, win);
    if (d->scale > 1) wl_surface_set_buffer_scale(t->surf, d->scale);
    t->xsurf = xdg_wm_base_get_xdg_surface(d->wm, t->surf);
    xdg_surface_add_listener(t->xsurf, &xsurf_lis, win);
    t->xtop = xdg_surface_get_toplevel(t->xsurf);
    xdg_toplevel_add_listener(t->xtop, &xtop_lis, win);
    xdg_toplevel_set_title(t->xtop, win->title? win->title: "ami");
    wl_surface_commit(t->surf); /* the no-buffer commit of the handshake */
    wl_display_flush(d->dpy);
}

/*******************************************************************************

Input listeners

*******************************************************************************/

/* cursor theme names by shape; cursor themes carry these names */
static const char* curname(int shape)
{
    switch (shape) {

        case pd_curtext:     return ("xterm");
        case pd_curcross:    return ("crosshair");
        case pd_curhand:     return ("hand2");
        case pd_cursizeh:    return ("sb_h_double_arrow");
        case pd_cursizev:    return ("sb_v_double_arrow");
        case pd_cursizenwse: return ("bottom_right_corner");
        case pd_cursizenesw: return ("bottom_left_corner");

    }
    return ("left_ptr");
}

/* show the cursor belonging to a window: its own, or the nearest
   ancestor's, or the arrow */
static void setcursor(pd_display* d, pd_win* w)
{
    struct wl_cursor*       cur;
    struct wl_cursor_image* img;
    struct wl_buffer*       buf;
    int                     shape;

    if (!d->ptr) return; /* no pointer, no one to show it to */
    shape = -1;
    TREERD();
    while (w && shape < 0) { shape = w->cursor; w = w->parent; }
    TREEUN();
    if (shape < 0) shape = pd_curarrow;
    if (shape == d->curshown && d->csurf) return;
    if (!d->ctheme) {

        d->ctheme = wl_cursor_theme_load(NULL, 24, d->shm);
        if (!d->ctheme) return;
        d->csurf = wl_compositor_create_surface(d->comp);

    }
    cur = wl_cursor_theme_get_cursor(d->ctheme, curname(shape));
    if (!cur) cur = wl_cursor_theme_get_cursor(d->ctheme, "left_ptr");
    if (!cur || !cur->image_count) return;
    img = cur->images[0];
    buf = wl_cursor_image_get_buffer(img);
    wl_surface_attach(d->csurf, buf, 0, 0);
    wl_surface_damage(d->csurf, 0, 0, img->width, img->height);
    wl_surface_commit(d->csurf);
    wl_pointer_set_cursor(d->ptr, d->enterserial, d->csurf,
                          img->hotspot_x, img->hotspot_y);
    d->curshown = shape;
}

static unsigned btnmask(int b)
{
    switch (b) {

        case 1: return (PD_MBTN1);
        case 2: return (PD_MBTN2);
        case 3: return (PD_MBTN3);

    }
    return (0);
}

/* route a pointer event: honor the grab, else hit test */
/* Resize edges grabbed at (px,py) on a toplevel's declared frame ring, 0
   when off the ring. The ring is borderw wide and rides over the client
   edge, thinning to the title inset beside the title row so the title and
   its buttons keep their presses; near a corner the edge band widens so
   the diagonal grab is not a tiny square */
static unsigned frmedges(pd_win* win, int px, int py)
{
    wltop*   t = win->top;
    int      w, h, bw, topw, sidew, cz, titrow;
    unsigned edges = 0;

    if (!t || t->borderw <= 0 || t->maximized) return (0);
    w = win->w; h = win->h;
    if (px < 0 || py < 0 || px >= w || py >= h) return (0);
    bw = t->borderw;
    titrow = t->tith > 0 && py < t->tity+t->tith;
    topw = t->tith > 0 && t->tity < bw? t->tity: bw;
    sidew = titrow && t->titx < bw? t->titx: bw;
    if (!(px < sidew || py < topw || px >= w-sidew || py >= h-bw))
        return (0);
    cz = bw*4 > 16? bw*4: 16;
    if (py < topw || (py < cz && (px < cz || px >= w-cz)))
        edges |= XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    if (py >= h-bw || (py >= h-cz && (px < cz || px >= w-cz)))
        edges |= XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    if (px < sidew || (px < cz && (py < cz || py >= h-cz)))
        edges |= XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    if (px >= w-sidew || (px >= w-cz && (py < cz || py >= h-cz)))
        edges |= XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    return (edges);
}

static pd_win* ptrroute(pd_display* d, int* x, int* y)
{
    pd_win* w;
    int     ox, oy, px, py;

    /* the tree is read under its lock; the seat state under the
       connection lock the dispatch holds */
    if (d->grab) {

        TREERD();
        winorg(d->grab, &ox, &oy);
        TREEUN();
        *x = (int)d->ptrx-ox; *y = (int)d->ptry-oy;
        return (d->grab);

    }
    if (d->igrab) {

        /* the press window holds the pointer until release, so drags
           track and the release arrives even outside its bounds */
        TREERD();
        winorg(d->igrab, &ox, &oy);
        TREEUN();
        *x = (int)d->ptrx-ox; *y = (int)d->ptry-oy;
        return (d->igrab);

    }
    if (!d->ptrtop) return (NULL);
    /* the frame ring owns the pointer where it rides over the client
       edge, the way invisible resize handles behave */
    px = (int)d->ptrx; py = (int)d->ptry;
    TREERD();
    if (frmedges(d->ptrtop, px, py)) {

        TREEUN();
        *x = px; *y = py;
        return (d->ptrtop);

    }
    w = hittest(d->ptrtop, px, py, x, y);
    TREEUN();
    return (w);
}

static void crossevt(pd_display* d, pd_win* w, pd_etype t)
{
    pd_evt e;
    int    ox, oy;

    if (!w) return;
    mkevt(&e, t, w);
    TREERD();
    winorg(w, &ox, &oy);
    TREEUN();
    e.x = (int)d->ptrx-ox;
    e.y = (int)d->ptry-oy;
    enq(d, &e);
}

static void ptrenter(void* data, struct wl_pointer* p, uint32_t serial,
                     struct wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy)
{
    pd_display* d = data;
    pd_win*     win;
    int         x, y;

    (void)p;
    d->inserial = serial;
    d->enterserial = serial;
    win = surf? wl_surface_get_user_data(surf): NULL;
    d->ptrtop = win;
    d->ptrx = wl_fixed_to_double(sx)*d->scale;
    d->ptry = wl_fixed_to_double(sy)*d->scale;
    d->ptrwin = ptrroute(d, &x, &y);
    d->curshown = -2; /* the enter must set a cursor or none shows */
    setcursor(d, d->ptrwin);
    crossevt(d, d->ptrwin, pd_etenter);
}

static void ptrleave(void* data, struct wl_pointer* p, uint32_t serial,
                     struct wl_surface* surf)
{
    pd_display* d = data;

    (void)p; (void)serial; (void)surf;
    /* A press in flight cannot follow the pointer off the surface: the
       compositor stops the stream at the edge. Release the press to its
       owner, or a widget under a runaway drag stays pressed forever */
    if (d->igrab) {

        pd_evt e;
        int    ox, oy, b;

        TREERD();
        winorg(d->igrab, &ox, &oy);
        TREEUN();
        for (b = 1; b <= 3; b++)
            if (d->mods & btnmask(b)) {

                d->mods &= ~btnmask(b);
                mkevt(&e, pd_etbtnup, d->igrab);
                e.btn = b;
                e.x = (int)d->ptrx-ox;
                e.y = (int)d->ptry-oy;
                enq(d, &e);

            }
        d->igrab = NULL;

    }
    crossevt(d, d->ptrwin, pd_etleave);
    d->ptrtop = NULL;
    d->ptrwin = NULL;
}

static void ptrmotion(void* data, struct wl_pointer* p, uint32_t time,
                      wl_fixed_t sx, wl_fixed_t sy)
{
    pd_display* d = data;
    pd_win*     nw;
    int         x, y;
    pd_evt      e;

    (void)p;
    d->ptrx = wl_fixed_to_double(sx)*d->scale;
    d->ptry = wl_fixed_to_double(sy)*d->scale;
    nw = ptrroute(d, &x, &y);
    if (nw != d->ptrwin) {

        crossevt(d, d->ptrwin, pd_etleave);
        crossevt(d, nw, pd_etenter);
        d->ptrwin = nw;
        setcursor(d, nw);

    }
    if (nw) {

        mkevt(&e, pd_etmouse, nw);
        e.x = x; e.y = y;
        e.time = time;
        enq(d, &e);

    }
}

static void ptrbutton(void* data, struct wl_pointer* p, uint32_t serial,
                      uint32_t time, uint32_t button, uint32_t state)
{
    pd_display* d = data;
    pd_win*     w;
    pd_win*     t;
    int         x, y, b;
    pd_evt      e;

    (void)p;
    d->inserial = serial;
    /* linux button codes: BTN_LEFT 0x110, RIGHT 0x111, MIDDLE 0x112 */
    b = button == 0x110? 1: button == 0x111? 3:
        button == 0x112? 2: 0;
    if (!b) return;
    w = ptrroute(d, &x, &y);
    if (state) d->mods |= btnmask(b);
    else d->mods &= ~btnmask(b);
    /* A press on a toplevel's declared frame goes to the compositor: the
       resize border becomes an interactive resize with the grabbed edges,
       the title bar an interactive move. The declared title rectangle
       excludes the frame buttons, whose presses flow to the application */
    t = d->ptrtop;
    if (state && b == 1 && !d->grab && t && t->top && w == t) {

        int px = (int)d->ptrx, py = (int)d->ptry;
        unsigned edges;

        TREERD();
        edges = frmedges(t, px, py);
        TREEUN();

        if (edges) {

            xdg_toplevel_resize(t->top->xtop, d->seat, serial, edges);
            return;

        }
        if (t->top->titw > 0 &&
            px >= t->top->titx && px < t->top->titx+t->top->titw &&
            py >= t->top->tity && py < t->top->tity+t->top->tith) {

            xdg_toplevel_move(t->top->xtop, d->seat, serial);
            return;

        }

    }
    /* a press takes the implicit grab: the presser owns the pointer until
       the last button releases. Frame presses returned above; the
       compositor owns those interactions */
    if (state && w && !d->grab && !d->igrab) d->igrab = w;
    if (w) {

        mkevt(&e, state? pd_etbtndown: pd_etbtnup, w);
        e.btn = b;
        e.x = x; e.y = y;
        e.time = time;
        enq(d, &e);

    }
    if (!state && d->igrab &&
        !(d->mods & (PD_MBTN1|PD_MBTN2|PD_MBTN3))) {

        /* last button up: the grab releases, and the pointer re-routes to
           whatever is under it now, with the crossings that implies */
        pd_win* nw;
        int     nx, ny;

        d->igrab = NULL;
        nw = ptrroute(d, &nx, &ny);
        if (nw != d->ptrwin) {

            crossevt(d, d->ptrwin, pd_etleave);
            crossevt(d, nw, pd_etenter);
            d->ptrwin = nw;
            setcursor(d, nw);

        }

    }
}

static void ptraxis(void* data, struct wl_pointer* p, uint32_t time,
                    uint32_t axis, wl_fixed_t value)
{
    pd_display* d = data;
    pd_win*     w;
    int         x, y, b;
    pd_evt      e;

    (void)p;
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;
    b = wl_fixed_to_double(value) < 0? 4: 5;
    w = ptrroute(d, &x, &y);
    if (w) {

        mkevt(&e, pd_etbtndown, w);
        e.btn = b;
        e.x = x; e.y = y;
        e.time = time;
        enq(d, &e);
        e.etype = pd_etbtnup;
        enq(d, &e);

    }
}

/* the seat v5 grouping and axis-detail events: the model takes nothing
   from them, but every slot a bound version can deliver must be filled --
   libwayland aborts on a NULL listener entry */
static void ptrframe(void* data, struct wl_pointer* p)
{ (void)data; (void)p; }

static void ptraxissrc(void* data, struct wl_pointer* p, uint32_t src)
{ (void)data; (void)p; (void)src; }

static void ptraxisstop(void* data, struct wl_pointer* p, uint32_t time,
                        uint32_t axis)
{ (void)data; (void)p; (void)time; (void)axis; }

static void ptraxisdisc(void* data, struct wl_pointer* p, uint32_t axis,
                        int32_t steps)
{ (void)data; (void)p; (void)axis; (void)steps; }

static const struct wl_pointer_listener ptr_lis = {
    ptrenter, ptrleave, ptrmotion, ptrbutton, ptraxis,
    ptrframe, ptraxissrc, ptraxisstop, ptraxisdisc
};

/* keyboard */

static void kbmap(void* data, struct wl_keyboard* k, uint32_t fmt, int fd,
                  uint32_t size)
{
    pd_display* d = data;
    char*       map;

    (void)k;
    if (fmt != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
    map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map != MAP_FAILED) {

        if (d->keymap) xkb_keymap_unref(d->keymap);
        if (d->xst) xkb_state_unref(d->xst);
        d->keymap = xkb_keymap_new_from_string(d->xkb, map,
                        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        d->xst = d->keymap? xkb_state_new(d->keymap): NULL;
        munmap(map, size);

    }
    close(fd);
}

/* keyboard focus goes to the entered toplevel unless the caller set an
   explicit focus inside it */
static void kbenter(void* data, struct wl_keyboard* k, uint32_t serial,
                    struct wl_surface* surf, struct wl_array* keys)
{
    pd_display* d = data;
    pd_win*     win;
    pd_evt      e;

    (void)k; (void)keys;
    d->inserial = serial;
    win = surf? wl_surface_get_user_data(surf): NULL;
    d->kbdtop = win;
    if (win) { mkevt(&e, pd_etfocus, win); enq(d, &e); }
}

static void kbleave(void* data, struct wl_keyboard* k, uint32_t serial,
                    struct wl_surface* surf)
{
    pd_display* d = data;
    pd_evt      e;

    (void)k; (void)serial; (void)surf;
    if (d->kbdtop) { mkevt(&e, pd_etnofocus, d->kbdtop); enq(d, &e); }
    d->kbdtop = NULL;
    d->rptkey = 0;
    { struct itimerspec z = {0}; timerfd_settime(d->rtfd, 0, &z, NULL); }
}

/* the window key events deliver to */
static pd_win* keywin(pd_display* d)
{
    if (d->focus) return (d->focus);
    return (d->kbdtop);
}

/* keysym for an X-style keycode, through the seat keymap. A seat with no
   keyboard (headless) never delivered one; rig injection still needs
   translation, so the default rules (pc105/us) fill in */
static uint32_t keysymof(pd_display* d, uint32_t code)
{
    const xkb_keysym_t* syms;
    int n;

    if (!d->keymap && d->xkb) {

        struct xkb_rule_names rn;

        memset(&rn, 0, sizeof(rn));
        d->keymap = xkb_keymap_new_from_names(d->xkb, &rn,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (d->keymap) d->xst = xkb_state_new(d->keymap);

    }
    if (!d->keymap) return (0);
    n = xkb_keymap_key_get_syms_by_level(d->keymap, code, 0, 0, &syms);
    if (n < 1) return (0);
    return (syms[0]);
}

static void keyevt(pd_display* d, pd_etype t, uint32_t xkc, uint32_t time)
{
    pd_win* w;
    pd_evt  e;

    w = keywin(d);
    if (!w) return;
    mkevt(&e, t, w);
    e.code = xkc;
    e.keysym = keysymof(d, xkc);
    e.x = (int)d->ptrx; e.y = (int)d->ptry;
    e.time = time;
    enq(d, &e);
}

static void kbkey(void* data, struct wl_keyboard* k, uint32_t serial,
                  uint32_t time, uint32_t key, uint32_t state)
{
    pd_display* d = data;
    uint32_t xkc;
    struct itimerspec its;

    (void)k;
    d->inserial = serial;
    xkc = key+8; /* evdev to X keycode convention */
    keyevt(d, state? pd_etkeydown: pd_etkeyup, xkc, time);
    /* client-side repeat: the compositor sends one press */
    if (state && d->keymap && d->rptrate > 0 &&
        xkb_keymap_key_repeats(d->keymap, xkc)) {

        d->rptkey = xkc;
        memset(&its, 0, sizeof(its));
        its.it_value.tv_sec = d->rptdelay/1000;
        its.it_value.tv_nsec = (long)(d->rptdelay%1000)*1000000;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = d->rptrate > 0? 1000000000L/d->rptrate: 0;
        timerfd_settime(d->rtfd, 0, &its, NULL);

    } else if (!state && xkc == d->rptkey) {

        struct itimerspec z = {0};
        d->rptkey = 0;
        timerfd_settime(d->rtfd, 0, &z, NULL);

    }
}

static void kbmod(void* data, struct wl_keyboard* k, uint32_t serial,
                  uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp)
{
    pd_display* d = data;

    (void)k; (void)serial;
    if (d->xst) {

        xkb_state_update_mask(d->xst, dep, lat, lock, 0, 0, grp);
        d->mods &= ~(PD_MSHIFT|PD_MCTRL|PD_MALT);
        if (xkb_state_mod_name_is_active(d->xst, XKB_MOD_NAME_SHIFT,
            XKB_STATE_MODS_EFFECTIVE) > 0) d->mods |= PD_MSHIFT;
        if (xkb_state_mod_name_is_active(d->xst, XKB_MOD_NAME_CTRL,
            XKB_STATE_MODS_EFFECTIVE) > 0) d->mods |= PD_MCTRL;
        if (xkb_state_mod_name_is_active(d->xst, XKB_MOD_NAME_ALT,
            XKB_STATE_MODS_EFFECTIVE) > 0) d->mods |= PD_MALT;

    }
}

static void kbrpt(void* data, struct wl_keyboard* k, int32_t rate,
                  int32_t delay)
{
    pd_display* d = data;

    (void)k;
    d->rptrate = rate;
    d->rptdelay = delay;
}

static const struct wl_keyboard_listener kbd_lis = {
    kbmap, kbenter, kbleave, kbkey, kbmod, kbrpt
};

static void seatcaps(void* data, struct wl_seat* s, uint32_t caps)
{
    pd_display* d = data;

    if ((caps&WL_SEAT_CAPABILITY_POINTER) && !d->ptr) {

        d->ptr = wl_seat_get_pointer(s);
        wl_pointer_add_listener(d->ptr, &ptr_lis, d);

    }
    if ((caps&WL_SEAT_CAPABILITY_KEYBOARD) && !d->kbd) {

        d->kbd = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(d->kbd, &kbd_lis, d);

    }
}

static void seatname(void* data, struct wl_seat* s, const char* n)
{ (void)data; (void)s; (void)n; }

static const struct wl_seat_listener seat_lis = { seatcaps, seatname };

/* output */

static void outgeom(void* data, struct wl_output* o, int32_t x, int32_t y,
                    int32_t pw, int32_t ph, int32_t subpix, const char* make,
                    const char* model, int32_t transform)
{
    pd_display* d = data;

    (void)o; (void)x; (void)y; (void)subpix; (void)make; (void)model;
    (void)transform;
    if (pw > 0) d->outmmw = pw;
    if (ph > 0) d->outmmh = ph;
}

static void outmode(void* data, struct wl_output* o, uint32_t flags,
                    int32_t w, int32_t h, int32_t refresh)
{
    pd_display* d = data;

    (void)o; (void)refresh;
    if (flags&WL_OUTPUT_MODE_CURRENT) { d->outw = w; d->outh = h; }
}

static void outdone(void* data, struct wl_output* o) { (void)data; (void)o; }
static void outscale(void* data, struct wl_output* o, int32_t f)
{
    pd_display* d = data;

    (void)o;
    if (f > d->scale) d->scale = f;
}

static const struct wl_output_listener out_lis = {
    outgeom, outmode, outdone, outscale
};

/* shell ping */
static void wmping(void* data, struct xdg_wm_base* wm, uint32_t serial)
{ (void)data; xdg_wm_base_pong(wm, serial); }

static const struct xdg_wm_base_listener wm_lis = { wmping };

/* registry */
static void regglobal(void* data, struct wl_registry* reg, uint32_t name,
                      const char* iface, uint32_t ver)
{
    pd_display* d = data;

    (void)ver;
    if (!strcmp(iface, wl_compositor_interface.name))
        d->comp = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    else if (!strcmp(iface, wl_shm_interface.name))
        d->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    else if (!strcmp(iface, xdg_wm_base_interface.name)) {

        d->wm = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(d->wm, &wm_lis, d);

    } else if (!strcmp(iface, wl_seat_interface.name)) {

        d->seat = wl_registry_bind(reg, name, &wl_seat_interface, 5);
        wl_seat_add_listener(d->seat, &seat_lis, d);

    } else if (!strcmp(iface, wl_output_interface.name) && !d->out) {

        d->out = wl_registry_bind(reg, name, &wl_output_interface, 2);
        wl_output_add_listener(d->out, &out_lis, d);

    }
}

static void regremove(void* data, struct wl_registry* reg, uint32_t name)
{ (void)data; (void)reg; (void)name; }

static const struct wl_registry_listener reg_lis = { regglobal, regremove };

/*******************************************************************************

Connection management

*******************************************************************************/

pd_display* pd_open(void)
{
    pd_display* d;
    struct epoll_event ev;
    pthread_mutexattr_t ma;

    if (dpyopen) return (&thedpy);
    d = &thedpy;
    memset(d, 0, sizeof(thedpy));
    d->dpy = wl_display_connect(NULL);
    if (!d->dpy) return (NULL);
    {
        /* the tree lock prefers readers: a compose started from the flush
           walk takes the read lock again inside, and must not wait behind
           a writer queued between the two */
        pthread_rwlockattr_t ra;

        pthread_rwlockattr_init(&ra);
        pthread_rwlockattr_setkind_np(&ra, PTHREAD_RWLOCK_PREFER_READER_NP);
        pthread_rwlock_init(&treelk, &ra);
        pthread_rwlockattr_destroy(&ra);
    }
    d->bufq = wl_display_create_queue(d->dpy);
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&d->lk, &ma);
    d->root.mapped = 1;
    d->curshown = -1;
    d->outw = 1920; d->outh = 1080;   /* until the output reports */
    d->outmmw = 508; d->outmmh = 285; /* 96 dpi, the desktop fiction */
    d->scale = 1;
    d->reg = wl_display_get_registry(d->dpy);
    wl_registry_add_listener(d->reg, &reg_lis, d);
    wl_display_roundtrip(d->dpy); /* globals */
    wl_display_roundtrip(d->dpy); /* seat caps, keymap, output modes */
    {
        const char* sv = rigenv("PD_SCALE", "AMI_WL_SCALE");

        if (sv && atoi(sv) >= 1) d->scale = atoi(sv);
    }
    if (d->scale > 1) {

        /* A scaled desktop: buffers carry scale x scale pixels per
           logical pixel, and density follows the desktop convention of
           96 dpi per scale step -- the same figure the X backend reads
           from Xft.dpi -- so a point is the same size on either backend */
        d->outmmw = d->outw*254/(960*d->scale);
        d->outmmh = d->outh*254/(960*d->scale);

    } else
    /* An output reporting nonsense physical size (headless compositors
       report millimeters equal to pixels; broken EDID reports zeros) would
       shrink or explode every font. Outside plausible density, keep the
       96 dpi desktop fiction */
    if (d->outmmw < 1 || d->outmmh < 1 ||
        d->outw*25/d->outmmw < 50 || d->outw*25/d->outmmw > 500 ||
        d->outh*25/d->outmmh < 50 || d->outh*25/d->outmmh > 500) {

        d->outmmw = d->outw*254/960; /* mm = px * 25.4 / 96 */
        d->outmmh = d->outh*254/960;

    }
    if (!d->comp || !d->shm || !d->wm) {

        fprintf(stderr, "pdisplay: compositor lacks required globals\n");
        return (NULL);

    }
    d->xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    d->rptdelay = 400; d->rptrate = 25;
    d->injfd = -1;
    /* the combined wait fd */
    d->epfd = epoll_create1(EPOLL_CLOEXEC);
    d->evfd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    d->rtfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC);
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = wl_display_get_fd(d->dpy);
    epoll_ctl(d->epfd, EPOLL_CTL_ADD, wl_display_get_fd(d->dpy), &ev);
    ev.data.fd = d->evfd;
    epoll_ctl(d->epfd, EPOLL_CTL_ADD, d->evfd, &ev);
    ev.data.fd = d->rtfd;
    epoll_ctl(d->epfd, EPOLL_CTL_ADD, d->rtfd, &ev);
    dpyopen = 1;
    return (d);
}

void pd_close(pd_display* d)
{
    if (!dpyopen) return;
    wl_display_disconnect(d->dpy);
    close(d->epfd);
    close(d->evfd);
    close(d->rtfd);
    dpyopen = 0;
}

void pd_screen(pd_display* d, int* wpx, int* hpx, int* wmm, int* hmm)
{
    if (wpx) *wpx = d->outw;
    if (hpx) *hpx = d->outh;
    if (wmm) *wmm = d->outmmw;
    if (hmm) *hmm = d->outmmh;
}

int pd_scale(pd_display* d)
{
    return d->scale;
}

int pd_evtfd(pd_display* d)
{
    return (d->epfd);
}

/*******************************************************************************

Rig input injection

PD_INPUT (or AMI_WL_INPUT) names a fifo; lines arriving there synthesize
input as if the seat delivered it, which is what lets a compositor
without virtual input protocols (headless weston) drive interactive
tests. Commands:
    key <xkeycode>        press and release
    keydown <xkeycode>    press only
    keyup <xkeycode>      release only
    move <x> <y>          pointer motion, surface coordinates
    btn <1|2|3> <x> <y>   move, press, release

*******************************************************************************/

static void injline(pd_display* d, char* ln)
{
    int a, x, y;
    pd_win* c;

    /* a pointer target: the first mapped toplevel */
    if (!d->ptrtop) {

        TREERD();
        for (c = d->root.childs; c; c = c->sibnext)
            if (c->mapped && c->top) { d->ptrtop = c; break; }
        TREEUN();

    }
    if (!d->kbdtop) d->kbdtop = d->ptrtop;
    if (sscanf(ln, "key %d", &a) == 1) {

        keyevt(d, pd_etkeydown, a, (uint32_t)nowms());
        keyevt(d, pd_etkeyup, a, (uint32_t)nowms());

    } else if (sscanf(ln, "keydown %d", &a) == 1)
        keyevt(d, pd_etkeydown, a, (uint32_t)nowms());
    else if (sscanf(ln, "keyup %d", &a) == 1)
        keyevt(d, pd_etkeyup, a, (uint32_t)nowms());
    else if (sscanf(ln, "move %d %d", &x, &y) == 2)
        ptrmotion(d, NULL, (uint32_t)nowms(),
                  wl_fixed_from_double((double)x/d->scale),
                  wl_fixed_from_double((double)y/d->scale));
    else if (sscanf(ln, "btn %d %d %d", &a, &x, &y) == 3) {

        int bc = a == 1? 0x110: a == 2? 0x112: 0x111;

        ptrmotion(d, NULL, (uint32_t)nowms(),
                  wl_fixed_from_double((double)x/d->scale),
                  wl_fixed_from_double((double)y/d->scale));
        ptrbutton(d, NULL, 0, (uint32_t)nowms(), bc, 1);
        ptrbutton(d, NULL, 0, (uint32_t)nowms(), bc, 0);

    } else if (sscanf(ln, "btndown %d %d %d", &a, &x, &y) == 3) {

        int bc = a == 1? 0x110: a == 2? 0x112: 0x111;

        ptrmotion(d, NULL, (uint32_t)nowms(),
                  wl_fixed_from_double((double)x/d->scale),
                  wl_fixed_from_double((double)y/d->scale));
        ptrbutton(d, NULL, 0, (uint32_t)nowms(), bc, 1);

    } else if (sscanf(ln, "btnup %d %d %d", &a, &x, &y) == 3) {

        int bc = a == 1? 0x110: a == 2? 0x112: 0x111;

        ptrmotion(d, NULL, (uint32_t)nowms(),
                  wl_fixed_from_double((double)x/d->scale),
                  wl_fixed_from_double((double)y/d->scale));
        ptrbutton(d, NULL, 0, (uint32_t)nowms(), bc, 0);

    } else if (sscanf(ln, "target %d", &a) == 1) {

        /* aim the synthetic pointer at the a'th mapped toplevel */
        int i = 0;

        d->ptrtop = NULL;
        TREERD();
        for (c = d->root.childs; c; c = c->sibnext)
            if (c->mapped && c->top && i++ == a) { d->ptrtop = c; break; }
        TREEUN();
        d->kbdtop = d->ptrtop;
        d->ptrwin = NULL;

    } else if (sscanf(ln, "conf %d %d", &x, &y) == 2) {

        /* a synthesized compositor resize, in buffer pixels: what an
           interactive resize delivers, minus the pointer */
        if (d->ptrtop && d->ptrtop->top) {

            pd_win* w = d->ptrtop;
            pd_evt  e;
            int     ow, oh;

            wltop* tp = w->top;

            toplk(tp);
            tp->confw = x; tp->confh = y;
            if (x > 0 && y > 0 && (x != w->w || y != w->h)) {

                TREEWR();
                ow = w->w; oh = w->h;
                w->w = x; w->h = y;
                sizecanvas(w, x, y);
                TREEUN();
                sizebufs(d, w);
                mkevt(&e, pd_etresize, w);
                e.w = x; e.h = y;
                enq(d, &e);
                expodmg(d, w, ow, oh);

            }
            topulk(tp);

        }

    }
}

static void injpoll(pd_display* d)
{
    static char buf[256];
    static int  len;
    const char* fn;
    char        c;
    struct epoll_event ev;

    if (d->injfd < 0) {

        fn = rigenv("PD_INPUT", "AMI_WL_INPUT");
        if (!fn) return;
        d->injfd = open(fn, O_RDONLY|O_NONBLOCK);
        if (d->injfd < 0) return;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = d->injfd;
        epoll_ctl(d->epfd, EPOLL_CTL_ADD, d->injfd, &ev);

    }
    while (read(d->injfd, &c, 1) == 1) {

        if (c == '\n') { buf[len] = 0; len = 0; injline(d, buf); }
        else if (len < (int)sizeof(buf)-1) buf[len++] = c;

    }
}

/*******************************************************************************

Pump, presentation, and event delivery

*******************************************************************************/

/* drain wayland traffic and timers without blocking; the wake fds clear as
   a side effect. Callers hold the lock */
static void pump(pd_display* d)
{
    struct pollfd pf;
    uint64_t exp;
    int      i, n;

    if (!d->dpy) return;
    LK(d); /* the socket, its dispatch and the seat state it updates */
    injpoll(d); /* rig-injected input, when enabled */
    /* the prepare-read protocol: we are the only reader thread by
       construction (the caller's event loop), but the protocol keeps us
       honest against future queue users */
    while (wl_display_prepare_read(d->dpy) != 0)
        wl_display_dispatch_pending(d->dpy);
    wl_display_flush(d->dpy);
    pf.fd = wl_display_get_fd(d->dpy);
    pf.events = POLLIN;
    if (poll(&pf, 1, 0) > 0) wl_display_read_events(d->dpy);
    else wl_display_cancel_read(d->dpy);
    wl_display_dispatch_pending(d->dpy);
    /* buffer releases ride their own queue; drain it here too */
    wl_display_dispatch_queue_pending(d->dpy, d->bufq);
    /* clear the queue wake */
    { uint64_t c; while (read(d->evfd, &c, 8) > 0); }
    /* key repeat expirations synthesize presses */
    n = 0;
    if (read(d->rtfd, &exp, 8) == 8 && d->rptkey) n = (int)exp;
    for (i = 0; i < n && i < 10; i++)
        keyevt(d, pd_etkeydown, d->rptkey, (uint32_t)nowms());
    ULK(d);
    /* opportunistic commit of deferred damage, under the toplevels' locks */
    flushtops(d);
}

void pd_present(pd_win* win, int x, int y, int width, int height)
{
    pd_display* d = &thedpy;

    (void)d;
    windmg(win, x, y, width, height);
}

void pd_flush(pd_display* d)
{
    pd_win* c;

    /* the caller declares its state complete: held resizes commit */
    TREERD();
    for (c = d->root.childs; c; c = c->sibnext)
        if (c->top) { toplk(c->top); c->top->applying = 0; topulk(c->top); }
    TREEUN();
    flushtops(d);
    d->burstms = 0; /* back in the event loop: no burst in progress */
}

void pd_sync(pd_display* d)
{
    pd_win* c;
    int     pend, i;

    /* a sync completes the round trip: pump until every toplevel's damage
       has been committed (frame callbacks release deferred commits), with
       a bound so a stalled compositor cannot wedge the caller */
    for (i = 0; i < 200; i++) {

        flushtops(d);
        pump(d);
        pend = 0;
        TREERD();
        for (c = d->root.childs; c; c = c->sibnext)
            if (c->top && (c->top->dmg || c->top->commitpend)) pend = 1;
        TREEUN();
        if (!pend) break;
        usleep(1000);

    }
}

int pd_evtnext(pd_display* d, pd_evt* e, int block)
{
    int got;

    for (;;) {

        pump(d);
        got = deq(d, e);
        if (got) return (1);
        if (!block) { e->etype = pd_etnone; return (0); }
        usleep(1000);

    }
}

int pd_evtpeek(pd_display* d, pd_evt* e)
{
    int got;

    pump(d);
    got = 0;
    QLK();
    if (d->eqh) { *e = d->eqh->e; got = 1; }
    QULK();
    return (got);
}

int pd_evtcheck(pd_display* d, pd_win* w, pd_etype t, pd_evt* e)
{
    evq** pp;
    evq*  p;

    pump(d);
    QLK();
    pp = &d->eqh;
    while (*pp) {

        if ((*pp)->e.etype == t && (!w || (*pp)->e.win == w)) {

            p = *pp;
            *e = p->e;
            *pp = p->next;
            if (d->eqt == p) { d->eqt = NULL; for (p = d->eqh; p; p = p->next) d->eqt = p; }
            else { p->next = d->eqf; d->eqf = p; }
            QULK();
            return (1);

        }
        pp = &(*pp)->next;

    }
    QULK();
    return (0);
}

void pd_evtpost(pd_display* d, const pd_evt* e)
{
    pd_evt c;

    c = *e;
    enq(d, &c);
}

/* Deliver the compositor's frame pacing as pd_etframe events for the
   toplevel above w: the display's own refresh beat, in place of a
   free-running timer. Callbacks flow only while the compositor is
   presenting the surface -- the caller carries the floor for the
   withheld case */
void pd_frameevents(pd_win* w, int on)
{
    pd_display* d = &thedpy;
    pd_win* p;

    (void)d;
    TREEWR();
    p = w;
    if (p) {

        while (p->parent && p->parent->parent) p = p->parent;
        p->frmevt = on;

    }
    TREEUN();
}
