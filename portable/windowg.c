/** ****************************************************************************

\file

\brief GRAPHICAL MODE WINDOW MANAGER

Copyright (C) 2026 Scott A. Franco

This is a graphical window submanager for Petit-Ami. It takes a graphical
surface as provided by the graphics level API and subdivides it into
windows.

It is portable, meaning that it relies only on the graphics level API as
defined in graphics.h. It works by overriding the base calls and giving a
window view to the client program, the same pattern windowc applies to a
character surface. The plug-in stack it serves is:

    device <- graphics <- windowg <- widgets

The device layer (the Linux frame buffer today) hands the graphics layer a
screen; the graphics layer draws and delivers input on it as one surface;
this manager subdivides that surface into windows; the widget packages
stand on the windows.

The manager's windows live in the graphics layer's own screen contexts:
every window owns a pair of the layer's select() screens, one holding the
client content -- a full layer screen, so the cursor, colors, tabs, auto
scrolling and drawing state of a window are all simply the layer's own,
per screen -- and one holding the frame. Client drawing calls are
redirected: the manager selects the window's client screen for update,
passes the call down unchanged (client coordinates are the backing
screen's coordinates), and composes the damage to the display screen.
Composition is a set of rectangle copies through blockcopyg(): each
window's visible region is kept as a list of rectangles, the window
rectangle minus the windows above it in Z order, and the manager blits the
frame and client backings through those rectangles bottom to top. Since
every window is buffered in the layer, exposure -- a window moved, closed,
or sent behind -- is repaired by recomposition alone; no client is asked
to redraw.

Since the root window is the original graphics surface, windows exist only
within it, and the manager is transparent by default: the standard input/
output surface is created maximized and frameless, so a non-manager aware
program runs as if it had the screen to itself.

Interactive window moves and resizes are old school, as befits a CPU
drawn system: a press on the title bar or a sizing edge starts a rubber
band, an xor outline drawn through the layer, and the window moves or
resizes once, when the button releases.

                          BSD LICENSE INFORMATION

Copyright (C) 2026 - Scott A. Franco

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the project nor the names of its contributors may be
   used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL THE PROJECT
OR CONTRIBUTORS BE LIABLE FOR ANY DAMAGES ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>

/* local definitions */
#include <localdefs.h>
#include <graphics.h>

#if !defined(__MACH__)
extern char *program_invocation_short_name;
#endif

#define MAXFIL 100 /* maximum open files */
#define MAXCON 10  /* screen contexts per window (the client's select range) */
#define MAXLIN 250 /* maximum length of input buffered line */

/* the name of the null device; window files park on it, the manager
   supplying the content itself */
#define NULLDEV "/dev/null"

/* file handle numbers at the system interface level */
#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */

/* the display screen of the underlying layer */
#define DSPSCN 1

/* frame metrics, in pixels at the frame font's cell height fc */
#define FRMBORDER  2              /* plain border width */
#define CORNERZ    16             /* corner grab zone reach */
#define SIZHALO    8              /* invisible sizing reach past the edge */
/* The border a window wears. The sizing edges are invisible, the
   desktop way: the pointer's sizing shape announces them, and the
   grab reaches SIZHALO past the window edge. */
#define BORD(w)    FRMBORDER
#define BARH(fc)   ((fc)+6)       /* title bar height */
#define BTNW(fc)   ((fc)+6)       /* frame button width */

/* the rubber band line width */
#define BANDW 2

/* minimum interactive window size */
#define MINWIN 60

/* types of system vectors for override calls */
typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);
#if !defined(__MACH__)
#define NOCANCEL
extern void ovr_read_nocancel(pread_t nfp, pread_t* ofp);
extern void ovr_write_nocancel(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open_nocancel(popen_t nfp, popen_t* ofp);
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
#endif

/* rectangle, 1 based inclusive */
typedef struct { ami_long x1, y1, x2, y2; } rectangle;

/* rectangle list, the visible region of a window */
#define MAXVIS 64 /* rectangles a visible region may shatter into */
typedef struct {

    int       n;
    rectangle r[MAXVIS];

} rectlist;

/* PA queue structure. Its a bubble list. */
typedef struct paevtque {

    struct paevtque* next; /* next in list */
    struct paevtque* last; /* last in list */
    ami_evtrec       evt;  /* event data */

} paevtque;

/* drag types */
typedef enum {

    dt_none, /* no drag active */
    dt_move, /* the title bar drag */
    dt_size  /* a sizing edge drag */

} dragtyp;

/* window description */
typedef struct winrec* winptr;
typedef struct winrec {

    winptr   next;              /* next entry (for free list) */
    int      root;              /* window is the root surface window */
    int      parlfn;            /* logical parent */
    winptr   parwin;            /* link to parent (or NULL for parentless) */
    ami_long wid;               /* this window logical id */
    winptr   childwin;          /* list of child windows */
    winptr   childlst;          /* list pointer if this is a child */
    winptr   winlst;            /* master list of all windows */
    int      zorder;            /* Z ordering, 0 = bottom, N = top */
    /* layer screen assignments */
    int      frmscn;            /* layer screen holding the frame image */
    int      scns[MAXCON];      /* layer screens backing the client's
                                   select() contexts; 0 = not allocated */
    int      curdsp;            /* client's displayed screen, 1-MAXCON */
    int      curupd;            /* client's updated screen, 1-MAXCON */
    /* geometry, all pixels */
    ami_long orgx, orgy;        /* window origin in root, 1 based */
    ami_long pmaxx, pmaxy;      /* whole window size (frame included) */
    ami_long coffx, coffy;      /* client offset within the window */
    ami_long cmaxx, cmaxy;      /* onscreen client size */
    ami_long bufx, bufy;        /* client buffer size */
    ami_long normx, normy;      /* geometry restored to by normalize */
    ami_long normw, normh;
    int      maxed;             /* window is maximized */
    int      mined;             /* window is minimized */
    /* the window's font context in the layer, reapplied on entry */
    ami_long font;              /* logical font number */
    ami_long fontsiz;           /* cell height, 0 until set */
    float    points;            /* point size set, 0 if not */
    ami_long chrspcx, chrspcy;  /* extra character spacing */
    int      attrs;             /* font-changing attribute states */
    ami_long leadx, leady;      /* not used yet */
    /* window state */
    int      bufmod;            /* client asked for buffered mode (the
                                   backing is kept either way) */
    int      frame;             /* frame on/off */
    int      size;              /* size bars on/off */
    int      sysbar;            /* system bar on/off */
    int      visible;           /* window is visible */
    char*    title;             /* window title */
    int      focus;             /* window has focus */
    int      hover;             /* window being hovered */
    rectlist vis;               /* visible region, root coordinates */
    char     inpbuf[MAXLIN];    /* input line buffer */
    int      inpptr;            /* input line index */
    ami_long mpx, mpy;          /* last mouse position sent, client chars */
    ami_long mpxg, mpyg;        /* last mouse position sent, client pixels */
    int      mb1, mb2, mb3;     /* buttons the window believes are down */
    int      autof[MAXCON];     /* auto state per client screen */
    int      curvf[MAXCON];     /* caret visibility per client screen */
    /* menus */
    ami_menuptr amenu;          /* the window's menu, a private copy */
    struct menena* mena;        /* menu item enable states */
    ami_long mtx1[24];          /* bar title spans, window local */
    ami_long mtx2[24];
    int      mtn;               /* count of bar titles tracked */
    int      popup;             /* window is a menu popup */
    ami_menuptr pitems;         /* popup: the menu level shown */
    struct winrec* mowner;      /* popup: the menu's owner window */
    ami_long psel;              /* popup: row with an open cascade */
    ami_long phov;              /* popup: hovered row */
    int      timers[AMI_MAXTIM]; /* timers active on this window */
    ami_long frmtim;            /* frame timer active */

} winrec;

/* attribute bits recorded in winrec attrs (font-changing set) */
#define WABOLD   (1<<0)
#define WAITAL   (1<<1)
#define WACOND   (1<<2)
#define WAEXT    (1<<3)
#define WAXLIGHT (1<<4)
#define WALIGHT  (1<<5)
#define WAXBOLD  (1<<6)
#define WAHOLLOW (1<<7)
#define WARAISED (1<<8)

/* file tracking */
typedef struct filrec* filptr;
typedef struct filrec {

    FILE*  sfp; /* file pointer used to establish entry, or NULL */
    winptr win; /* associated window (if exists) */
    int    inl; /* this output file is linked to the input file, logical */
    int    inw; /* entry is input linked to window */

} filrec;

/*******************************************************************************

Saved API vectors

Every call the manager overrides saves the layer's vector here and calls
down through it. The layer beneath is the root surface.

*******************************************************************************/

static ami_cursor_t          cursor_down;
static ami_maxx_t            maxx_down;
static ami_maxy_t            maxy_down;
static ami_home_t            home_down;
static ami_del_t             del_down;
static ami_up_t              up_down;
static ami_down_t            down_down;
static ami_left_t            left_down;
static ami_right_t           right_down;
static ami_blink_t           blink_down;
static ami_reverse_t         reverse_down;
static ami_underline_t       underline_down;
static ami_superscript_t     superscript_down;
static ami_subscript_t       subscript_down;
static ami_italic_t          italic_down;
static ami_bold_t            bold_down;
static ami_strikeout_t       strikeout_down;
static ami_standout_t        standout_down;
static ami_fcolor_t          fcolor_down;
static ami_bcolor_t          bcolor_down;
static ami_fcolorc_t         fcolorc_down;
static ami_bcolorc_t         bcolorc_down;
static ami_fcolorg_t         fcolorg_down;
static ami_bcolorg_t         bcolorg_down;
static ami_auto_t            auto_down;
static ami_curvis_t          curvis_down;
static ami_scroll_t          scroll_down;
static ami_scrollg_t         scrollg_down;
static ami_curx_t            curx_down;
static ami_cury_t            cury_down;
static ami_curxg_t           curxg_down;
static ami_curyg_t           curyg_down;
static ami_curbnd_t          curbnd_down;
static ami_select_t          select_down;
static ami_event_t           event_down;
static ami_timer_t           timer_down;
static ami_killtimer_t       killtimer_down;
static ami_mouse_t           mouse_down;
static ami_mousebutton_t     mousebutton_down;
static ami_joystick_t        joystick_down;
static ami_joybutton_t       joybutton_down;
static ami_joyaxis_t         joyaxis_down;
static ami_settab_t          settab_down;
static ami_settabg_t         settabg_down;
static ami_restab_t          restab_down;
static ami_restabg_t         restabg_down;
static ami_clrtab_t          clrtab_down;
static ami_funkey_t          funkey_down;
static ami_frametimer_t      frametimer_down;
static ami_autohold_t        autohold_down;
static ami_wrtstr_t          wrtstr_down;
static ami_wrtstrn_t         wrtstrn_down;
static ami_eventover_t       eventover_down;
static ami_eventsover_t      eventsover_down;
static ami_sendevent_t       sendevent_down;
static ami_maxxg_t           maxxg_down;
static ami_maxyg_t           maxyg_down;
static ami_line_t            line_down;
static ami_linewidth_t       linewidth_down;
static ami_linestyle_t       linestyle_down;
static ami_rect_t            rect_down;
static ami_frect_t           frect_down;
static ami_rrect_t           rrect_down;
static ami_frrect_t          frrect_down;
static ami_ellipse_t         ellipse_down;
static ami_fellipse_t        fellipse_down;
static ami_arc_t             arc_down;
static ami_farc_t            farc_down;
static ami_fchord_t          fchord_down;
static ami_ftriangle_t       ftriangle_down;
static ami_cursorg_t         cursorg_down;
static ami_baseline_t        baseline_down;
static ami_setpixel_t        setpixel_down;
static ami_fover_t           fover_down;
static ami_bover_t           bover_down;
static ami_finvis_t          finvis_down;
static ami_binvis_t          binvis_down;
static ami_fxor_t            fxor_down;
static ami_bxor_t            bxor_down;
static ami_fand_t            fand_down;
static ami_band_t            band_down;
static ami_for_t             for_down;
static ami_bor_t             bor_down;
static ami_chrsizx_t         chrsizx_down;
static ami_chrsizy_t         chrsizy_down;
static ami_fonts_t           fonts_down;
static ami_font_t            font_down;
static ami_fontnam_t         fontnam_down;
static ami_fontsiz_t         fontsiz_down;
static ami_setpoints_t       setpoints_down;
static ami_points_t          points_down;
static ami_chrspcy_t         chrspcy_down;
static ami_chrspcx_t         chrspcx_down;
static ami_dpmx_t            dpmx_down;
static ami_dpmy_t            dpmy_down;
static ami_strsiz_t          strsiz_down;
static ami_chrpos_t          chrpos_down;
static ami_writejust_t       writejust_down;
static ami_justpos_t         justpos_down;
static ami_condensed_t       condensed_down;
static ami_extended_t        extended_down;
static ami_xlight_t          xlight_down;
static ami_light_t           light_down;
static ami_xbold_t           xbold_down;
static ami_hollow_t          hollow_down;
static ami_raised_t          raised_down;
static ami_loadpict_t        loadpict_down;
static ami_pictsizx_t        pictsizx_down;
static ami_pictsizy_t        pictsizy_down;
static ami_picture_t         picture_down;
static ami_delpict_t         delpict_down;
static ami_viewoffg_t        viewoffg_down;
static ami_viewscale_t       viewscale_down;
static ami_scalex_t          scalex_down;
static ami_scaley_t          scaley_down;
static ami_path_t            path_down;
static ami_blockcopyg_t      blockcopyg_down;
static ami_title_t           title_down;
static ami_openwin_t         openwin_down;
static ami_buffer_t          buffer_down;
static ami_sizbuf_t          sizbuf_down;
static ami_sizbufg_t         sizbufg_down;
static ami_getsiz_t          getsiz_down;
static ami_getsizg_t         getsizg_down;
static ami_setsiz_t          setsiz_down;
static ami_setsizg_t         setsizg_down;
static ami_setpos_t          setpos_down;
static ami_setposg_t         setposg_down;
static ami_scnsiz_t          scnsiz_down;
static ami_scnsizg_t         scnsizg_down;
static ami_scncen_t          scncen_down;
static ami_scnceng_t         scnceng_down;
static ami_winclient_t       winclient_down;
static ami_winclientg_t      winclientg_down;
static ami_front_t           front_down;
static ami_back_t            back_down;
static ami_frame_t           frame_down;
static ami_sizable_t         sizable_down;
static ami_sysbar_t          sysbar_down;
static ami_getwinid_t        getwinid_down;
static ami_focus_t           focus_down;

/* saved system I/O vectors */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static plseek_t  ofplseek;
#ifdef NOCANCEL
static pread_t   ofpread_nocancel;
static pwrite_t  ofpwrite_nocancel;
static popen_t   ofpopen_nocancel;
static pclose_t  ofpclose_nocancel;
#endif

/*******************************************************************************

Module state

*******************************************************************************/

static filptr   opnfil[MAXFIL];       /* open files table */
static int      xltwin[MAXFIL*2+1];   /* window equivalence table */
static int      filwin[MAXFIL];       /* file to window equivalence table */
static winptr   winfre;               /* free windows structure list */
static winptr   winlst;               /* master list of all windows */
static int      ztop;                 /* highest Z order in use, -1 none */
static winptr   focwin;               /* window with the keyboard focus */
static winptr   hovwin;               /* window under the pointer */
static int      fend;                 /* end of program ordered */
static ami_long fautohold;            /* automatic hold on exit */
static paevtque* paqfre;              /* free PA event queue entries */
static paevtque* paqevt;              /* PA event input queue */
static ami_pevthan evthan[ami_etdsize+1]; /* event handler routines */
static ami_pevthan evtshan;           /* master event handler */
static winptr   timtbl[AMI_MAXTIM];   /* layer timer to window map */
static winptr   frmtimwin;            /* window holding the frame timer */
static ami_long dimxg, dimyg;         /* the root surface size in pixels */
static ami_long rootcell;             /* the root font cell height */
static int      scnuse[MAXCON*10+1];  /* layer screens in use; index 1 up */
static int      scnmax;               /* the layer's real screen count */
static winptr   ctxwin;               /* window whose context the layer
                                         currently holds (font, screens) */
static dragtyp  drag;                 /* active drag type */
static winptr   drgwin;               /* window being dragged */
static ami_long drgax, drgay;         /* drag anchor, root pixels */
static ami_long drgox, drgoy;         /* window origin at drag start */
static ami_long drgow, drgoh;         /* window size at drag start */
static unsigned drgedges;             /* sizing edges grabbed */
static rectangle bandr;               /* the band rectangle on screen */
static int      banddrawn;            /* band outline is on the display */
static int      carshown;             /* caret block is on the display */
static ami_long carx, cary;           /* caret block position, root space */
static ami_long carw, carh;           /* caret block cell size */
static int      incar;                /* caret update in progress */
static int      carhold;              /* caret held off for a compound update */
static int      wgheld;               /* composition held (test fast forward) */
static int      curshp;               /* the pointer shape now worn */

/* The pointer shape backdoor into a graphics layer that draws its own
   pointer: 0 arrow, 1 horizontal sizing, 2 vertical sizing, 3 nw-se,
   4 ne-sw. Weak: a backend whose desktop owns the pointer lacks it,
   and the shape feedback simply does not apply. */
extern void grx_pointer(int shape) __attribute__((weak));
static int      mgractive;            /* the manager finished initializing */

/* forward declarations */
static void error(const char* s);
static winptr txt2win(FILE* f);
static void entercli(winptr win);
static void enterdsp(void);
static void composewin(winptr win, rectangle* dr);
static void composeall(rectangle* dr);
static void caroff(void);
static void caron(void);
static void calcvis(winptr win);
static void calcvisall(void);
static void drwfrm(winptr win);
static void plcchr(winptr win, char c);

/*******************************************************************************

Error handler

*******************************************************************************/

static void error(const char* s)

{

    fprintf(stderr, "*** Error: windowg: %s\n", s);
    fflush(stderr);

    /* under diagnosis, die where it happened: the crash dump reports the
       stack of this thread, and the core holds the rest */
    if (getenv("AMI_ERRABORT")) abort();
    exit(1);

}

/*******************************************************************************

Rectangle operations

Rectangles are 1 based and inclusive. The visible region of a window is
a list of disjoint rectangles: the window rectangle, shattered by
subtracting the windows above it.

*******************************************************************************/

static void setrect(rectangle* r, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    r->x1 = x1; r->y1 = y1;
    r->x2 = x2; r->y2 = y2;

}

static int intersect(rectangle* r1, rectangle* r2)

{

    return (r1->x2 >= r2->x1 && r1->x1 <= r2->x2 &&
            r1->y2 >= r2->y1 && r1->y1 <= r2->y2);

}

static void intersection(rectangle* ri, rectangle* r1, rectangle* r2)

{

    *ri = *r1;
    if (r1->x1 < r2->x1) ri->x1 = r2->x1;
    if (r1->x2 > r2->x2) ri->x2 = r2->x2;
    if (r1->y1 < r2->y1) ri->y1 = r2->y1;
    if (r1->y2 > r2->y2) ri->y2 = r2->y2;

}

static int emptyrect(rectangle* r)

{

    return (r->x1 > r->x2 || r->y1 > r->y2);

}

static int inrect(ami_long x, ami_long y, rectangle* r)

{

    return (x >= r->x1 && x <= r->x2 && y >= r->y1 && y <= r->y2);

}

/* append a rectangle to a list, dropping empties and overflow beyond
   MAXVIS (a very shattered window loses slivers, never crashes) */
static void addrect(rectlist* l, rectangle* r)

{

    if (emptyrect(r)) return;
    if (l->n < MAXVIS) l->r[l->n++] = *r;

}

/* subtract rectangle s from every rectangle of the list, shattering the
   remainders into up to four pieces each */
static void subrect(rectlist* l, rectangle* s)

{

    rectlist  o;
    rectangle p;
    rectangle w;
    int       i;

    o = *l;
    l->n = 0;
    for (i = 0; i < o.n; i++) {

        p = o.r[i];
        if (!intersect(&p, s)) { addrect(l, &p); continue; }
        /* the band above s */
        if (p.y1 < s->y1) {

            setrect(&w, p.x1, p.y1, p.x2, s->y1-1);
            addrect(l, &w);

        }
        /* the band below s */
        if (p.y2 > s->y2) {

            setrect(&w, p.x1, s->y2+1, p.x2, p.y2);
            addrect(l, &w);

        }
        /* the left band beside s */
        if (p.x1 < s->x1) {

            setrect(&w, p.x1,
                    p.y1 > s->y1? p.y1: s->y1, s->x1-1,
                    p.y2 < s->y2? p.y2: s->y2);
            addrect(l, &w);

        }
        /* the right band beside s */
        if (p.x2 > s->x2) {

            setrect(&w, s->x2+1,
                    p.y1 > s->y1? p.y1: s->y1, p.x2,
                    p.y2 < s->y2? p.y2: s->y2);
            addrect(l, &w);

        }

    }

}

/*******************************************************************************

File table

*******************************************************************************/

static void getfil(filptr* fp)

{

    *fp = malloc(sizeof(filrec));
    if (!*fp) error("Out of memory");
    (*fp)->win = NULL;
    (*fp)->inw = FALSE;
    (*fp)->inl = -1;
    (*fp)->sfp = NULL;

}

static winptr lfn2win(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error("Invalid file handle");
    if (!opnfil[fn]) error("Invalid file handle");
    if (!opnfil[fn]->win) error("File is not attached to a window");

    return (opnfil[fn]->win);

}

static winptr txt2win(FILE* f)

{

    int fn;

    fn = fileno(f);
    if (fn < 0) error("Invalid file");

    return (lfn2win(fn));

}

static winptr lwn2win(ami_long wid)

{

    int ofn;

    if (wid < -MAXFIL || wid >= MAXFIL || !wid) error("Invalid window id");
    ofn = xltwin[wid+MAXFIL];
    if (ofn < 0) error("Invalid window id");

    return (lfn2win(ofn));

}

/*******************************************************************************

Layer screen allocation

The layer's select() screens back the manager's windows. Screen 1 is the
display; the rest are handed out here.

*******************************************************************************/

static int alcscn(void)

{

    int i;

    for (i = 2; i <= scnmax; i++)
        if (!scnuse[i]) {

            scnuse[i] = TRUE;
            return (i);

        }
    error("Out of window backing screens");

    return (0);

}

static void frescn(int i)

{

    if (i > 0 && i <= scnmax) scnuse[i] = FALSE;

}

/*******************************************************************************

Layer context

The layer holds one font and one screen selection for the root surface.
Before acting for a window, the manager makes the layer's state that
window's: the font context is reapplied when the window differs from the
one the layer last served, and the window's current update screen is
selected, with the display screen staying on display.

*******************************************************************************/

/* apply the window's font context to the layer */
static void applyfont(winptr win)

{

    (*bold_down)(stdout, !!(win->attrs & WABOLD));
    (*italic_down)(stdout, !!(win->attrs & WAITAL));
    (*condensed_down)(stdout, !!(win->attrs & WACOND));
    (*extended_down)(stdout, !!(win->attrs & WAEXT));
    (*xlight_down)(stdout, !!(win->attrs & WAXLIGHT));
    (*light_down)(stdout, !!(win->attrs & WALIGHT));
    (*xbold_down)(stdout, !!(win->attrs & WAXBOLD));
    if (win->font) (*font_down)(stdout, win->font);
    if (win->points > 0) (*setpoints_down)(stdout, win->points);
    else if (win->fontsiz) (*fontsiz_down)(stdout, win->fontsiz);
    (*chrspcx_down)(stdout, win->chrspcx);
    (*chrspcy_down)(stdout, win->chrspcy);

}

/* Create a client backing screen for a window. The layer lays a new
   screen's default tabs from the font it happens to hold, which may be
   another window's (the frame drawing leaves the sign font, for one);
   the tabs are rebuilt here under the window's own font: every 8th of
   its character cell, the terminal default. */
static void alcbacking(winptr win, int i)

{

    ami_long cs, mx, t, n;

    char ff = '\f';

    win->scns[i] = alcscn();
    (*select_down)(stdout, win->scns[i], DSPSCN);
    (*sizbufg_down)(stdout, win->bufx, win->bufy);
    ctxwin = NULL;
    /* The index may be recycled from a closed window, and the layer
       keeps a screen's state: reset to the fresh defaults -- colors,
       cursor visible, cleared content with the cursor home (on grid
       for any font) -- under a lifted auto, with the window's font. */
    (*auto_down)(stdout, FALSE);
    applyfont(win);
    (*fcolor_down)(stdout, ami_black);
    (*bcolor_down)(stdout, ami_white);
    (*curvis_down)(stdout, FALSE); /* the manager draws the caret */
    (*ofpwrite)(OUTFIL, &ff, 1); /* clear and home through the layer */
    (*auto_down)(stdout, win->autof[i]);
    ctxwin = win;
    /* The default tabs in the window's own cell. The layer's tab
       table is finite; a wide window at a small cell asks for more
       stops than it holds, so the lay caps as the layer's own default
       lay does. */
    cs = (*chrsizx_down)(stdout);
    if (cs < 1) cs = 1;
    mx = win->bufx/cs;
    (*clrtab_down)(stdout);
    n = 0;
    for (t = 9; t <= mx && n < 200; t += 8, n++)
        (*settabg_down)(stdout, (t-1)*cs+1);

}

/* enter a window's client context */
static void entercli(winptr win)

{

    int scn;

    if (!win->scns[win->curupd-1])
        /* the client screen allocates at need */
        alcbacking(win, win->curupd-1);
    scn = win->scns[win->curupd-1];
    (*select_down)(stdout, scn, DSPSCN);
    if (ctxwin != win) {

        /* the font reapply is illegal under auto; lift it around the
           switch and put the window's own state back */
        (*auto_down)(stdout, FALSE);
        applyfont(win);
        (*auto_down)(stdout, win->autof[win->curupd-1]);
        ctxwin = win;

    }

}

/* enter the display for direct drawing (the rubber band) */
static void enterdsp(void)

{

    (*select_down)(stdout, DSPSCN, DSPSCN);
    ctxwin = NULL;

}

/* Enter the display context for composition. The layer's blockcopy
   mixes by the write mode of its current update screen; at compose
   time that is the client's backing, in whatever mode the client
   left -- xor blits frames over each other, self canceling to black,
   and invisible drops them whole. The display screen's own mode is
   the composer's, overwrite (the rubber band borrows it and puts it
   back), so compose with the display as the update screen. Only
   blits follow, no font or client state, so the client context
   cache stands and the next entercli reselects as it always does. */
static void entercmp(void)

{

    (*select_down)(stdout, DSPSCN, DSPSCN);

}

/* An unbuffered client's surface follows the window: when the client
   size changes, the backings resize to it. The layer's resize hands
   back fresh screen state, so the window's context reapplies on the
   next client entry. */
static void trackbuf(winptr win)

{

    int i;

    if (win->bufmod) return;
    if (win->bufx == win->cmaxx && win->bufy == win->cmaxy) return;
    win->bufx = win->cmaxx;
    win->bufy = win->cmaxy;
    for (i = 0; i < MAXCON; i++)
        if (win->scns[i]) {

            (*select_down)(stdout, win->scns[i], DSPSCN);
            (*sizbufg_down)(stdout, win->bufx, win->bufy);

        }
    ctxwin = NULL;

}

/*******************************************************************************

Geometry

*******************************************************************************/

/* the window rectangle in root coordinates */
static void winrect(winptr win, rectangle* r)

{

    setrect(r, win->orgx, win->orgy,
            win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);

}

/* the client rectangle in root coordinates */
static void clirect(winptr win, rectangle* r)

{

    setrect(r, win->orgx+win->coffx, win->orgy+win->coffy,
            win->orgx+win->coffx+win->cmaxx-1,
            win->orgy+win->coffy+win->cmaxy-1);

}

/* the ancestor clip: a child shows only within its parents' clients */
static int ancclip(winptr win, rectangle* r)

{

    winptr    p = win->parwin;
    rectangle pr, ri;

    setrect(r, 1, 1, dimxg, dimyg);
    while (p) {

        clirect(p, &pr);
        if (!intersect(r, &pr)) return (FALSE);
        intersection(&ri, r, &pr);
        *r = ri;
        p = p->parwin;

    }

    return (TRUE);

}

/* the parent's client origin in root space: child coordinates are
   relative to it */
static void parorigin(winptr win, ami_long* ox, ami_long* oy)

{

    rectangle cr;

    if (win->parwin) {

        clirect(win->parwin, &cr);
        *ox = cr.x1-1;
        *oy = cr.y1-1;

    } else {

        *ox = 0;
        *oy = 0;

    }

}

/* a moved window carries its children, recursively */
static void movekids(winptr win, ami_long dx, ami_long dy)

{

    winptr c;

    for (c = win->childwin; c; c = c->childlst) {

        c->orgx += dx;
        c->orgy += dy;
        movekids(c, dx, dy);

    }

}

/* frame metrics from the frame state */
static void frmmetrics(winptr win)

{

    if (win->frame) {

        win->coffx = BORD(win);
        win->coffy = BORD(win)+(win->sysbar? BARH(rootcell): 0)
                     +(win->amenu? BARH(rootcell): 0);
        win->pmaxx = win->cmaxx+2*BORD(win);
        win->pmaxy = win->cmaxy+win->coffy+BORD(win);

    } else {

        win->coffx = 0;
        /* the menu row is not a frame decoration: it is a band above
           the client, frameless or not */
        win->coffy = (win->amenu? BARH(rootcell): 0);
        win->pmaxx = win->cmaxx;
        win->pmaxy = win->cmaxy+win->coffy;

    }

}

/*******************************************************************************

Z order and hit testing

*******************************************************************************/

/* the topmost visible window containing the root point */
static winptr winat(ami_long x, ami_long y)

{

    winptr    wp, best;
    rectangle r, ac;
    int       bz = -1;

    best = NULL;
    for (wp = winlst; wp; wp = wp->winlst)
        if (wp->visible) {

            winrect(wp, &r);
            if (inrect(x, y, &r) && ancclip(wp, &ac) && inrect(x, y, &ac) &&
                wp->zorder > bz) {

                best = wp;
                bz = wp->zorder;

            }

        }

    return (best);

}

/* pull a window (and its children, keeping their order) to the top */
static void ztotop(winptr win)

{

    winptr wp;
    winptr c;

    if (win->root) return; /* the root is the floor: it never rises */
    if (win->zorder == ztop) return;
    /* close the gap the window leaves */
    for (wp = winlst; wp; wp = wp->winlst)
        if (wp != win && wp->zorder > win->zorder) wp->zorder--;
    win->zorder = ztop;
    /* Children ride above their parent, keeping their relative order:
       raise them bottom first by their current order, and the order
       carries. A raised child sits on top, so the minimum is always
       one not yet raised. */
    {

        int n = 0;

        for (c = win->childwin; c; c = c->childlst) n++;
        while (n--) {

            winptr low = NULL;

            for (c = win->childwin; c; c = c->childlst)
                if (!low || c->zorder < low->zorder) low = c;
            if (low) ztotop(low);

        }

    }

}

/*******************************************************************************

Visibility

A window's visible region is its rectangle, clipped to its ancestors,
minus every window above it in Z order (clipped to theirs). Kept as a
rectangle list, recomputed when the window arrangement changes.

*******************************************************************************/

static void calcvis(winptr win)

{

    winptr    wp;
    rectangle r, ac, wr, wac, wi;

    win->vis.n = 0;
    if (!win->visible) return;
    winrect(win, &r);
    if (!ancclip(win, &ac)) return;
    if (!intersect(&r, &ac)) return;
    intersection(&wi, &r, &ac);
    addrect(&win->vis, &wi);
    for (wp = winlst; wp; wp = wp->winlst)
        if (wp != win && wp->visible &&
            wp->zorder > win->zorder) {

            winrect(wp, &wr);
            if (!ancclip(wp, &wac)) continue;
            if (!intersect(&wr, &wac)) continue;
            intersection(&wi, &wr, &wac);
            subrect(&win->vis, &wi);

        }

}

static void calcvisall(void)

{

    winptr wp;

    for (wp = winlst; wp; wp = wp->winlst) calcvis(wp);

}

/*******************************************************************************

Composition

The display is rebuilt from the window backings with blockcopyg: for
each window, bottom to top, the visible rectangles that touch the damage
are copied from the frame and client backing screens to the display.

*******************************************************************************/

/* copy one root-space rectangle of a window to the display */
static void comprect(winptr win, rectangle* r)

{

    rectangle cr, ri;
    ami_long  sx, sy;

    if ((win->frame || win->amenu) && win->frmscn) {

        /* the frame image carries the whole window; the client blit
           covers its center */
        sx = r->x1-win->orgx+1;
        sy = r->y1-win->orgy+1;
        (*blockcopyg_down)(stdout, win->frmscn, DSPSCN,
                           sx, sy, sx+(r->x2-r->x1), sy+(r->y2-r->y1),
                           r->x1, r->y1, r->x2, r->y2);

    }
    clirect(win, &cr);
    /* the client blit clips to the buffer, so a client larger than the
       buffer shows the frame screen's field in the margin */
    if (cr.x2 > cr.x1+win->bufx-1) cr.x2 = cr.x1+win->bufx-1;
    if (cr.y2 > cr.y1+win->bufy-1) cr.y2 = cr.y1+win->bufy-1;
    if (intersect(r, &cr)) {

        intersection(&ri, r, &cr);
        sx = ri.x1-cr.x1+1;
        sy = ri.y1-cr.y1+1;
        if (win->scns[win->curdsp-1])
            (*blockcopyg_down)(stdout, win->scns[win->curdsp-1], DSPSCN,
                               sx, sy, sx+(ri.x2-ri.x1), sy+(ri.y2-ri.y1),
                               ri.x1, ri.y1, ri.x2, ri.y2);

    }

}

/* compose the parts of one window that fall in the damage rectangle */
static void composewin(winptr win, rectangle* dr)

{

    rectangle ri;
    int       i;

    if (!win->visible || wgheld) return;
    entercmp(); /* the blits mix by the display screen's mode */
    for (i = 0; i < win->vis.n; i++) {

        if (!intersect(&win->vis.r[i], dr)) continue;
        intersection(&ri, &win->vis.r[i], dr);
        comprect(win, &ri);

    }

}

/* compose every window, bottom to top, within the damage rectangle */
static void composeall(rectangle* dr)

{

    winptr wp;
    int    z;

    /* visible regions are disjoint, so order is free; walk by Z for
       clarity */
    caroff();
    for (z = 0; z <= ztop; z++)
        for (wp = winlst; wp; wp = wp->winlst)
            if (wp->zorder == z) composewin(wp, dr);
    caron();

}

/* compose the damage a client drawing call made: the window's client
   rectangle (or a part of it) intersected with its visible region */
/* first content presents the window: frame and all */
static void winvis(winptr win)

{

    rectangle r;

    if (win->visible) return;
    win->visible = TRUE;
    calcvisall();
    winrect(win, &r);
    composeall(&r);

}

static void compdmg(winptr win)

{

    rectangle cr;

    if (!win->visible) winvis(win);
    caroff();
    clirect(win, &cr);
    composewin(win, &cr);
    caron();

}

/*******************************************************************************

Menus, part one: state and the bar

The menu model is windowc's: the bar takes a row of the window above
the client, which cedes it; pulldowns are parentless bordered windows
on a cascade stack, drawn by the manager through the layer; a selection
queues etmenus to the menu's owner. The machinery that needs the later
window operations lives in part two, below them.

*******************************************************************************/

/* menu item enable state, kept aside the window's menu copy */
typedef struct menena* menenaptr;
typedef struct menena {

    menenaptr next;
    ami_long  id;
    int       ena;

} menena;

#define MAXPOP    8    /* maximum popup cascade */
#define MAXMTITLE 24   /* most bar titles tracked for hits */
#define MENSEP    12   /* pixel gap between bar titles */

/* the menu palette */
#define MNUFLD  0xfa, 0xfa, 0xfa /* popup field */
#define MNUHIL  0x35, 0x60, 0xb0 /* highlighted entry field */
#define MNUHITX 0xff, 0xff, 0xff /* highlighted entry text */
#define MNUGREY 0x9a, 0x9a, 0x9a /* disabled entry text */

static winptr popstk[MAXPOP];  /* the open pulldown cascade */
static FILE*  popfil[MAXPOP];  /* their files, for closing */
static int    popcnt;          /* how many are open */
static winptr menuwin;         /* window whose bar is open */
static int    menutitle;       /* the open title, 1-n */

static ami_menu_t    menu_down;
static ami_menuena_t menuena_down;
static ami_menusel_t menusel_down;
static ami_stdmenu_t stdmenu_down;

static void clspops(int downto);
static void frmenu(ami_menuptr m);
static void regeom(winptr win, rectangle* old);
static void annresize(winptr win);
static void menu_ivf(FILE* f, ami_menuptr m);
static void menuena_ivf(FILE* f, ami_long id, ami_long onoff);
static void menusel_ivf(FILE* f, ami_long id, ami_long select);
static void stdmenu_ivf(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm);
static void popclick(winptr pw, ami_long lx, ami_long ly);
static void pophover(winptr pw, ami_long ly);
static void barpress(winptr win, int n);
static void menudismiss(void);

/* is a menu item enabled for a window */
static int menenb(winptr win, ami_long id)

{

    menenaptr me;

    for (me = win->mena; me; me = me->next)
        if (me->id == id) return (me->ena);

    return (TRUE); /* enabled unless said otherwise */

}

/* the nth entry of a menu level */
static ami_menuptr mennth(ami_menuptr m, ami_long n)

{

    while (m && n > 1) { m = m->next; n--; }

    return (m);

}

/* the length of a menu level */
static ami_long menlen(ami_menuptr m)

{

    ami_long n = 0;

    while (m) { n++; m = m->next; }

    return (n);

}

/* the bar title under a window local x, 1-n, or 0 */
static int mbarhitn(winptr win, ami_long x)

{

    int n;

    for (n = 0; n < win->mtn; n++)
        if (x >= win->mtx1[n] && x <= win->mtx2[n]) return (n+1);

    return (0);

}

/*******************************************************************************

Frame drawing

The frame is drawn into the window's frame backing screen through the
layer: a bar with the title and the minimize, maximize and close
buttons, and a plain border. The GNOME-ish light theme.

*******************************************************************************/

/* frame palette */
#define FRMBAR  0xdd, 0xdd, 0xdd /* title bar field */
#define FRMBARF 0xc5, 0xc5, 0xc5 /* title bar field, no focus */
#define FRMEDGE 0x99, 0x99, 0x99 /* border */
#define FRMTEXT 0x10, 0x10, 0x10 /* title and button glyphs */

/* set layer foreground from 8 bit components */
static void fcolor8(int r, int g, int b)

{

    (*fcolorc_down)(stdout, (ami_long)r*(LONG_MAX/255), (ami_long)g*(LONG_MAX/255),
                    (ami_long)b*(LONG_MAX/255));

}

static void bcolor8(int r, int g, int b)

{

    (*bcolorc_down)(stdout, (ami_long)r*(LONG_MAX/255), (ami_long)g*(LONG_MAX/255),
                    (ami_long)b*(LONG_MAX/255));

}

/* Draw the menu bar row into the frame screen. The caller (drwfrm) has
   the frame screen selected, the sign font up and auto off. The title
   spans record for hit testing as they lay out. */
static void drwmbar(winptr win)

{

    ami_menuptr p;
    ami_long bd = win->frame? BORD(win): 0;
    ami_long by1 = win->coffy-BARH(rootcell)+1;
    ami_long by2 = win->coffy;
    ami_long x = bd+1+MENSEP/2;
    ami_long cy = by1+(BARH(rootcell)-rootcell)/2;
    ami_long l;
    int  n = 0;

    /* the bar field */
    if (win->focus) fcolor8(FRMBAR); else fcolor8(FRMBARF);
    (*frect_down)(stdout, bd+1, by1, win->pmaxx-bd, by2);
    (*font_down)(stdout, AMI_FONT_SIGN);
    (*fontsiz_down)(stdout, rootcell);
    for (p = win->amenu; p; p = p->next) {

        l = (*strsiz_down)(stdout, p->face);
        if (n < MAXMTITLE) {

            win->mtx1[n] = x-MENSEP/2;
            win->mtx2[n] = x+l+MENSEP/2-1;

        }
        if (menuwin == win && menutitle == n+1) {

            /* the open title shows reversed */
            fcolor8(MNUHIL);
            (*frect_down)(stdout, x-MENSEP/2, by1, x+l+MENSEP/2-1, by2);
            fcolor8(MNUHITX);

        } else if (!menenb(win, p->id)) fcolor8(MNUGREY);
        else fcolor8(FRMTEXT);
        (*cursorg_down)(stdout, x, cy);
        (*wrtstrn_down)(stdout, p->face, strlen(p->face));
        x += l+MENSEP;
        n++;

    }
    win->mtn = n;
    fcolor8(FRMTEXT);

}


/* the x spans of the frame buttons, window local; right to left:
   close, maximize, minimize */
static void btnspan(winptr win, int n, ami_long* x1, ami_long* x2)

{

    ami_long bw = BTNW(rootcell);

    *x2 = win->pmaxx-BORD(win)-1-n*(bw+2);
    *x1 = *x2-bw+1;

}

/* which frame element is at the window-local point: 0 none, 1 title,
   2 close, 3 maximize, 4 minimize, 5 sizing edge */
static int frmhit(winptr win, ami_long x, ami_long y)

{

    ami_long x1, x2;
    int  n;

    if (!win->frame && !win->amenu) return (0);
    /* the sizing edges, when sizing is enabled; corners fold into them */
    if (win->frame && win->size && !win->maxed && !win->mined &&
        (x <= BORD(win)+2 || x > win->pmaxx-BORD(win)-2 ||
         y <= BORD(win)+2 || y > win->pmaxy-BORD(win)-2))
        return (5);
    /* the menu bar row, below the system bar */
    if (win->amenu && y > win->coffy-BARH(rootcell) && y <= win->coffy)
        return (6);
    if (win->frame && win->sysbar &&
        y <= win->coffy-(win->amenu? BARH(rootcell): 0)) {

        for (n = 0; n < 3; n++) {

            btnspan(win, n, &x1, &x2);
            if (x >= x1 && x <= x2) return (2+n);

        }
        return (1); /* the title bar */

    }

    return (0);

}

/* The invisible sizing halo: the topmost sizable window whose halo
   covers a point just outside its rectangle takes the sizing grab,
   unless the window the point actually lies in stands above it. */
static winptr haloat(ami_long x, ami_long y, winptr over)

{

    winptr    wp, best = NULL;
    rectangle r;

    for (wp = winlst; wp; wp = wp->winlst) {

        if (!wp->visible || wp->mined || wp->maxed) continue;
        if (!wp->frame || !wp->size || wp->root) continue;
        winrect(wp, &r);
        if (x >= r.x1-SIZHALO && x <= r.x2+SIZHALO &&
            y >= r.y1-SIZHALO && y <= r.y2+SIZHALO &&
            (x < r.x1 || x > r.x2 || y < r.y1 || y > r.y2) &&
            (!best || wp->zorder > best->zorder))
            best = wp;

    }
    if (best && over && over->zorder > best->zorder) best = NULL;
    return (best);

}

/* the sizing edges a point grabs: 1 top, 2 bottom, 4 left, 8 right */
static unsigned frmedges(winptr win, ami_long x, ami_long y)

{

    unsigned e = 0;
    ami_long cz = CORNERZ; /* the widened corner zone */

    if (x <= BORD(win)+2 || (x <= cz && (y <= cz || y > win->pmaxy-cz)))
        e |= 4;
    if (x > win->pmaxx-BORD(win)-2 ||
        (x > win->pmaxx-cz && (y <= cz || y > win->pmaxy-cz)))
        e |= 8;
    if (y <= BORD(win)+2 || (y <= cz && (x <= cz || x > win->pmaxx-cz)))
        e |= 1;
    if (y > win->pmaxy-BORD(win)-2 ||
        (y > win->pmaxy-cz && (x <= cz || x > win->pmaxx-cz)))
        e |= 2;

    return (e);

}

/* draw the frame into the frame backing screen */
static void drwfrm(winptr win)

{

    ami_long x1, x2, bw, bh, cy, l, n;
    char* tp;

    if ((!win->frame && !win->amenu) || !win->frmscn) return;
    /* the frame screen carries the whole window */
    (*select_down)(stdout, win->frmscn, DSPSCN);
    ctxwin = NULL; /* the selection and font both change */
    (*sizbufg_down)(stdout, win->pmaxx, win->pmaxy);
    (*auto_down)(stdout, FALSE);
    (*curvis_down)(stdout, FALSE);
    (*binvis_down)(stdout); /* labels draw foreground only */
    /* the border */
    fcolor8(FRMEDGE);
    (*frect_down)(stdout, 1, 1, win->pmaxx, win->pmaxy);
    /* the field inside the border (the client area shows over it) */
    if (win->focus) fcolor8(FRMBAR); else fcolor8(FRMBARF);
    (*frect_down)(stdout, BORD(win)+1, BORD(win)+1,
                  win->pmaxx-BORD(win), win->pmaxy-BORD(win));
    if (win->sysbar) {

        bh = BARH(rootcell);
        bw = BTNW(rootcell);
        cy = BORD(win)+bh/2; /* the bar's center line */
        /* the title, centered in the root font AT the root size: the
           layer's current size is whatever a client last set */
        (*font_down)(stdout, AMI_FONT_SIGN);
        (*fontsiz_down)(stdout, rootcell);
        fcolor8(FRMTEXT);
        if (win->title) {

            l = (*strsiz_down)(stdout, win->title);
            btnspan(win, 2, &x1, &x2); /* the leftmost button */
            if (l > x1-2-BORD(win)-2) l = x1-2-BORD(win)-2;
            if (l > 0) {

                (*cursorg_down)(stdout,
                    BORD(win)+1+(x1-BORD(win))/2-l/2,
                    cy-rootcell/2);
                /* clip by character count to the space */
                tp = win->title;
                while (*tp &&
                       (*strsiz_down)(stdout, win->title) > 0 &&
                       (*chrpos_down)(stdout, win->title, tp-win->title) < l)
                    tp++;
                (*wrtstrn_down)(stdout, win->title, tp-win->title);

            }

        }
        /* the buttons: minimize, maximize, close, right to left the
           other way about */
        for (n = 0; n < 3; n++) {

            btnspan(win, n, &x1, &x2);
            fcolor8(FRMTEXT);
            (*linewidth_down)(stdout, 2);
            if (n == 0) {

                /* close: the cross */
                (*line_down)(stdout, x1+bw/4, cy-bw/4, x2-bw/4, cy+bw/4);
                (*line_down)(stdout, x1+bw/4, cy+bw/4, x2-bw/4, cy-bw/4);

            } else if (n == 1) {

                /* maximize: the chevron up */
                (*line_down)(stdout, x1+bw/4, cy+bw/8, x1+bw/2, cy-bw/8);
                (*line_down)(stdout, x1+bw/2, cy-bw/8, x2-bw/4, cy+bw/8);

            } else {

                /* minimize: the chevron down */
                (*line_down)(stdout, x1+bw/4, cy-bw/8, x1+bw/2, cy+bw/8);
                (*line_down)(stdout, x1+bw/2, cy+bw/8, x2-bw/4, cy-bw/8);

            }
            (*linewidth_down)(stdout, 1);

        }

    }
    if (win->amenu) drwmbar(win); /* the menu bar row below the title */

}

/*******************************************************************************

Event queue

Holds events bound for the client that arise out of order: the second
half of a translation, or state changes the manager synthesizes.

*******************************************************************************/

static paevtque* getpaevt(void)

{

    paevtque* p;

    if (paqfre) { p = paqfre; paqfre = p->next; }
    else {

        p = malloc(sizeof(paevtque));
        if (!p) error("Out of memory");

    }

    return (p);

}

static void putpaevt(paevtque* p)

{

    p->next = paqfre;
    paqfre = p;

}

static void enquepaevt(ami_evtrec* e)

{

    paevtque* p;

    p = getpaevt();
    memcpy(&p->evt, e, sizeof(ami_evtrec));
    if (paqevt) {

        p->next = paqevt;
        p->last = paqevt->last;
        paqevt->last->next = p;
        paqevt->last = p;

    } else {

        p->next = p;
        p->last = p;
        paqevt = p;

    }

}

static int dequepaevt(ami_evtrec* e)

{

    paevtque* p;

    if (!paqevt) return (FALSE);
    p = paqevt;
    if (p->next == p) paqevt = NULL;
    else {

        paqevt->last->next = p->next;
        p->next->last = paqevt->last;
        paqevt = p->next;

    }
    memcpy(e, &p->evt, sizeof(ami_evtrec));
    putpaevt(p);

    return (TRUE);

}

/* queue a simple event to a window */
static void quevent(winptr win, ami_evtcod e)

{

    ami_evtrec er;

    er.etype = e;
    er.winid = win->wid;
    enquepaevt(&er);

}

/*******************************************************************************

Focus

*******************************************************************************/

static void setfocus(winptr win)

{

    winptr old;

    if (win == focwin) return;
    caroff();
    carhold++;
    old = focwin;
    focwin = win;
    if (old) {

        old->focus = FALSE;
        quevent(old, ami_etnofocus);
        drwfrm(old);
        compdmg(old); /* the bar tint changed */
        { rectangle r; winrect(old, &r); composewin(old, &r); }

    }
    if (win) {

        win->focus = TRUE;
        quevent(win, ami_etfocus);
        drwfrm(win);
        { rectangle r; winrect(win, &r); composewin(win, &r); }

    }
    carhold--;
    caron();

}

/*******************************************************************************

The rubber band

An xor outline drawn on the display screen through the layer; drawing it
twice removes it.

*******************************************************************************/

static void bandflip(void)

{

    enterdsp();
    (*fxor_down)(stdout);
    (*linewidth_down)(stdout, BANDW);
    (*fcolor_down)(stdout, ami_white);
    (*rect_down)(stdout, bandr.x1, bandr.y1, bandr.x2, bandr.y2);
    (*linewidth_down)(stdout, 1);
    (*fover_down)(stdout);
    (*fcolor_down)(stdout, ami_black);

}

static void banddraw(void)

{

    if (!banddrawn) { bandflip(); banddrawn = TRUE; }

}

static void banderase(void)

{

    if (banddrawn) { bandflip(); banddrawn = FALSE; }

}

/*******************************************************************************

The caret

The manager owns the caret: every backing screen's cursor stays
invisible at the layer, and the manager draws its own -- an xor block
on the display screen at the focus window's client cursor, in root
space, so it lands where the window is, not where its backing lies.
Drawing it twice removes it; it comes off before any composition and
goes back after, so the block never mixes into content.

*******************************************************************************/

static void carflip(void)

{

    entercmp();
    (*fxor_down)(stdout);
    (*fcolor_down)(stdout, ami_white);
    (*frect_down)(stdout, carx, cary, carx+carw-1, cary+carh-1);
    (*fover_down)(stdout);
    (*fcolor_down)(stdout, ami_black);

}

/* take the caret off the display */
static void caroff(void)

{

    if (carshown) { carflip(); carshown = FALSE; }

}

/* place the caret at the focus window's client cursor, if it shows */
static void caron(void)

{

    winptr    win = focwin;
    rectangle cr, car;
    ami_long  cx, cy;
    int       i;

    if (carshown || incar || carhold || wgheld) return;
    if (!win || !win->visible || win->mined) return;
    if (!win->curvf[win->curupd-1]) return;
    incar = TRUE;
    entercli(win); /* the cursor and cell in the window's own context */
    cx = (*curxg_down)(stdout);
    cy = (*curyg_down)(stdout);
    carw = (*chrsizx_down)(stdout);
    carh = (*chrsizy_down)(stdout);
    incar = FALSE;
    /* to root space, inside the client shown on screen */
    clirect(win, &cr);
    carx = cr.x1+cx-1;
    cary = cr.y1+cy-1;
    if (carx < cr.x1 || cary < cr.y1 ||
        carx+carw-1 > cr.x2 || cary+carh-1 > cr.y2) return;
    /* and only where the window is actually visible */
    setrect(&car, carx, cary, carx+carw-1, cary+carh-1);
    for (i = 0; i < win->vis.n; i++)
        if (car.x1 >= win->vis.r[i].x1 && car.x2 <= win->vis.r[i].x2 &&
            car.y1 >= win->vis.r[i].y1 && car.y2 <= win->vis.r[i].y2) {

            carflip();
            carshown = TRUE;
            break;

        }

}

/* the band rectangle from the drag state and pointer */
static void bandcalc(ami_long gx, ami_long gy)

{

    ami_long dx = gx-drgax;
    ami_long dy = gy-drgay;
    ami_long nx = drgox, ny = drgoy, nw = drgow, nh = drgoh;

    if (drag == dt_move) {

        nx = drgox+dx;
        ny = drgoy+dy;

    } else if (drag == dt_size) {

        if (drgedges & 8) nw = drgow+dx;
        if (drgedges & 2) nh = drgoh+dy;
        if (drgedges & 4) { nw = drgow-dx; nx = drgox+dx; }
        if (drgedges & 1) { nh = drgoh-dy; ny = drgoy+dy; }
        if (nw < MINWIN) { if (drgedges & 4) nx -= MINWIN-nw; nw = MINWIN; }
        if (nh < MINWIN) { if (drgedges & 1) ny -= MINWIN-nh; nh = MINWIN; }

    }
    setrect(&bandr, nx, ny, nx+nw-1, ny+nh-1);

}

/* end the drag, applying what the band outlined */
static void dragend(ami_long gx, ami_long gy)

{

    winptr    win = drgwin;
    rectangle r1, r2;
    ami_evtrec er;

    banderase();
    if (!win) { drag = dt_none; return; }
    winrect(win, &r1);
    /* the band's final rectangle computes under the live drag state --
       bandcalc switches on it -- so the state clears after */
    bandcalc(gx, gy);
    drag = dt_none;
    drgwin = NULL;
    if (bandr.x1 == win->orgx && bandr.y1 == win->orgy &&
        bandr.x2-bandr.x1+1 == win->pmaxx &&
        bandr.y2-bandr.y1+1 == win->pmaxy) return; /* nothing moved */
    movekids(win, bandr.x1-win->orgx, bandr.y1-win->orgy);
    win->orgx = bandr.x1;
    win->orgy = bandr.y1;
    if (bandr.x2-bandr.x1+1 != win->pmaxx ||
        bandr.y2-bandr.y1+1 != win->pmaxy) {

        /* A resize: the client area changes; the buffer does not, per
           the buffered model. The client is told, and may follow with
           sizbuf itself. */
        win->pmaxx = bandr.x2-bandr.x1+1;
        win->pmaxy = bandr.y2-bandr.y1+1;
        win->cmaxx = win->pmaxx-(win->frame? 2*BORD(win): 0);
        win->cmaxy = win->pmaxy-win->coffy-(win->frame? BORD(win): 0);
        if (win->cmaxx < 1) win->cmaxx = 1;
        if (win->cmaxy < 1) win->cmaxy = 1;
        trackbuf(win); /* an unbuffered surface follows the window */
        drwfrm(win);
        er.etype = ami_etresize;
        er.winid = win->wid;
        er.rszxg = win->cmaxx;
        er.rszyg = win->cmaxy;
        entercli(win); /* the metrics queries below want the window's font */
        er.rszx = win->cmaxx/(*chrsizx_down)(stdout);
        er.rszy = win->cmaxy/(*chrsizy_down)(stdout);
        enquepaevt(&er);
        /* and the redraw notice the desktop backends send after it:
           an unbuffered client repaints on this */
        er.etype = ami_etredraw;
        er.winid = win->wid;
        enquepaevt(&er);

    }
    winrect(win, &r2);
    /* recompose what it left and where it landed */
    if (r2.x1 < r1.x1) r1.x1 = r2.x1;
    if (r2.y1 < r1.y1) r1.y1 = r2.y1;
    if (r2.x2 > r1.x2) r1.x2 = r2.x2;
    if (r2.y2 > r1.y2) r1.y2 = r2.y2;
    calcvisall();
    composeall(&r1);

}

/*******************************************************************************

Window state changes

*******************************************************************************/

/* maximize toggle: a maximized window fills the root, frameless, per
   the manager's transparency rule */
/* The manager carries these states itself, the windowc way: maximize
   fills the container -- the parent's client, or the display for a
   parentless window -- minimize reduces the window to its parked bar,
   and normalize restores the geometry the window had before either.
   The state announcements are made by the callers. */

/* the container a window maximizes into and parks its bar in */
static void container(winptr win, rectangle* r)

{

    if (win->parwin) clirect(win->parwin, r);
    else setrect(r, 1, 1, dimxg, dimyg);

}

/* apply a place and size, children riding, and announce the change */
static void plcgeom(winptr win, ami_long x, ami_long y, ami_long pw, ami_long ph)

{

    rectangle old;

    winrect(win, &old);
    movekids(win, x-win->orgx, y-win->orgy);
    win->orgx = x;
    win->orgy = y;
    win->cmaxx = pw-(win->frame? 2*BORD(win): 0);
    win->cmaxy = ph-win->coffy-(win->frame? BORD(win): 0);
    if (win->cmaxx < 1) win->cmaxx = 1;
    if (win->cmaxy < 1) win->cmaxy = 1;
    calcvisall();
    regeom(win, &old);
    annresize(win);

}

/* note what normalize restores to, when the geometry is the normal */
static void savnorm(winptr win)

{

    if (!win->maxed && !win->mined) {

        win->normx = win->orgx;
        win->normy = win->orgy;
        win->normw = win->pmaxx;
        win->normh = win->pmaxy;

    }

}

static void intmaxg(winptr win)

{

    rectangle c;

    savnorm(win);
    win->maxed = TRUE;
    win->mined = FALSE;
    container(win, &c);
    plcgeom(win, c.x1, c.y1, c.x2-c.x1+1, c.y2-c.y1+1);

}

static void intming(winptr win)

{

    rectangle c;
    winptr    sib;
    ami_long  w, h, tx, ty;
    int       moved;

    savnorm(win);
    win->mined = TRUE;
    win->maxed = FALSE;
    /* The window reduces to its bar: borders, the system bar and a
       sliver of client. Wide enough for a title and the buttons.
       Minimized windows park along the container's bottom, left to
       right, wrapping up a row when one fills; the slot walk steps
       past minimized siblings of the same parent. */
    w = 8*rootcell+3*(BTNW(rootcell)+2)+2*BORD(win)+MENSEP;
    h = win->coffy+(win->frame? BORD(win): 0)+1;
    container(win, &c);
    if (w > c.x2-c.x1+1) w = c.x2-c.x1+1;
    tx = c.x1;
    ty = c.y2-h+1;
    do {

        moved = FALSE;
        if (tx+w-1 > c.x2 && tx > c.x1) { /* row full: wrap up */

            tx = c.x1;
            ty -= h;
            moved = TRUE;

        }
        if (ty < c.y1) break; /* overfull: best effort */
        for (sib = winlst; sib; sib = sib->winlst)
            if (sib != win && sib->mined && !sib->root &&
                sib->parwin == win->parwin &&
                sib->orgx < tx+w && sib->orgx+sib->pmaxx > tx &&
                sib->orgy < ty+h && sib->orgy+sib->pmaxy > ty) {

                tx = sib->orgx+sib->pmaxx; /* step past this bar */
                moved = TRUE;

            }

    } while (moved);
    if (ty < c.y1) { tx = c.x1; ty = c.y1; }
    plcgeom(win, tx, ty, w, h);

}

static void intnormg(winptr win)

{

    if (!win->maxed && !win->mined) return;
    win->maxed = FALSE;
    win->mined = FALSE;
    plcgeom(win, win->normx, win->normy, win->normw, win->normh);

}

/*******************************************************************************

Place character

Characters written to a window file funnel here: the window's context is
entered and the character passes to the layer through the saved system
write vector, landing in the window's client backing; the damage then
composes to the display.

*******************************************************************************/

static void plcchr(winptr win, char c)

{

    entercli(win);
    (*ofpwrite)(OUTFIL, &c, 1);
    compdmg(win);

}

static void plcstr(winptr win, const char* p, size_t n)

{

    entercli(win);
    (*ofpwrite)(OUTFIL, p, n);
    compdmg(win);

}

/*******************************************************************************

System call interdiction

The window files are parked on the null device; their reads and writes
divert here. The root surface file (standard output) is a manager window
like any other.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->win &&
        mgractive) {

        plcstr(opnfil[fd]->win, buff, count);
        return (count);

    }

    return ((*ofpwrite)(fd, buff, count));

}

#ifdef NOCANCEL
static ssize_t iwrite_nocancel(int fd, const void* buff, size_t count)

{

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->win &&
        mgractive) {

        plcstr(opnfil[fd]->win, buff, count);
        return (count);

    }

    return ((*ofpwrite_nocancel)(fd, buff, count));

}
#endif

/* the readline machinery: line input with editing, driven by events */
static void readline(int fd)

{

    ami_evtrec er;
    winptr     win;
    int        ins = 1;
    int        lcmp = FALSE;
    int        i;
    int        ofn;

    do {

        ami_event(opnfil[fd]->sfp, &er);
        ofn = xltwin[er.winid+MAXFIL];
        if (ofn >= 0 && opnfil[ofn] && opnfil[ofn]->inl == fd) {

            win = lwn2win(er.winid);
            if (win->inpptr < 0) {

                win->inpptr = 0;
                win->inpbuf[0] = 0;
                ins = 1;

            }
            switch (er.etype) {

                case ami_etterm: exit(1);
                case ami_etenter:
                    while (win->inpbuf[win->inpptr]) win->inpptr++;
                    win->inpbuf[win->inpptr] = '\n';
                    win->inpbuf[win->inpptr+1] = 0;
                    plcchr(win, '\r');
                    plcchr(win, '\n');
                    lcmp = TRUE;
                    break;
                case ami_etchar:
                    if (win->inpptr < MAXLIN-2) {

                        if (ins) {

                            i = win->inpptr;
                            while (win->inpbuf[i]) i++;
                            while (win->inpptr <= i)
                                { win->inpbuf[i+1] = win->inpbuf[i]; i--; }
                            win->inpbuf[win->inpptr] = er.echar;
                            i = win->inpptr;
                            while (win->inpbuf[i]) plcchr(win, win->inpbuf[i++]);
                            i = win->inpptr;
                            while (win->inpbuf[i++]) plcchr(win, '\b');
                            plcchr(win, win->inpbuf[win->inpptr]);
                            win->inpptr++;

                        } else {

                            if (!win->inpbuf[win->inpptr])
                                win->inpbuf[win->inpptr+1] = 0;
                            win->inpbuf[win->inpptr] = er.echar;
                            plcchr(win, win->inpbuf[win->inpptr]);
                            win->inpptr++;

                        }

                    }
                    break;
                case ami_etdelcb:
                    if (win->inpptr > 0) {

                        win->inpptr--;
                        i = win->inpptr;
                        while (win->inpbuf[i])
                            { win->inpbuf[i] = win->inpbuf[i+1]; i++; }
                        plcchr(win, '\b');
                        i = win->inpptr;
                        while (win->inpbuf[i]) plcchr(win, win->inpbuf[i++]);
                        plcchr(win, ' ');
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
                case ami_etinsertt: ins = !ins; break;
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

    } while (!lcmp);
    win->inpptr = 0;

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    int            l, ofn, fi;
    winptr         win;
    unsigned char* ba;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->inw &&
        mgractive) {

        ba = (unsigned char*)buff;
        l = count;
        while (l > 0) {

            /* find any window with buffered data pointing at this input */
            ofn = -1;
            for (fi = 0; fi < MAXFIL; fi++)
                if (opnfil[fi] && opnfil[fi]->inl == fd && opnfil[fi]->win &&
                    strlen(opnfil[fi]->win->inpbuf) > 0) ofn = fi;
            if (ofn < 0) readline(fd);
            else {

                win = lfn2win(ofn);
                while (win->inpbuf[win->inpptr] && l) {

                    *ba = win->inpbuf[win->inpptr];
                    if (win->inpptr < MAXLIN) win->inpptr++;
                    if (*ba == '\n') { win->inpptr = -1; win->inpbuf[0] = 0; }
                    l--;
                    ba++;

                }

            }

        }
        return (count);

    }

    return ((*ofpread)(fd, buff, count));

}

#ifdef NOCANCEL
static ssize_t iread_nocancel(int fd, void* buff, size_t count)

{

    return (iread(fd, buff, count));

}
#endif

static int iopen(const char* pathname, int flags, int perm)

{

    return ((*ofpopen)(pathname, flags, perm));

}

/* tear down the window a closing file carries */
static void clswin(int fd)

{

    winptr    win, wp;
    winptr*   lp;
    rectangle r;
    int       i;

    win = opnfil[fd]->win;
    if (win->root) return; /* the root surface stays */
    winrect(win, &r);
    /* out of the master list */
    lp = &winlst;
    while (*lp && *lp != win) lp = &(*lp)->winlst;
    if (*lp) *lp = win->winlst;
    /* out of the parent's child list */
    if (win->parwin) {

        lp = &win->parwin->childwin;
        while (*lp && *lp != win) lp = &(*lp)->childlst;
        if (*lp) *lp = win->childlst;

    }
    /* the Z order closes over it */
    for (wp = winlst; wp; wp = wp->winlst)
        if (wp->zorder > win->zorder) wp->zorder--;
    if (ztop > -1) ztop--;
    /* the backing screens return */
    for (i = 0; i < MAXCON; i++) if (win->scns[i]) frescn(win->scns[i]);
    if (win->frmscn) frescn(win->frmscn);
    /* the tables clear */
    if (win->wid >= -MAXFIL && win->wid < MAXFIL)
        xltwin[win->wid+MAXFIL] = -1;
    filwin[fd] = -1;
    if (focwin == win) focwin = NULL;
    if (hovwin == win) hovwin = NULL;
    caroff();
    /* the window's menus die with it */
    { int i; for (i = popcnt-1; i >= 0; i--)
        if (popstk[i]->mowner == win) clspops(i); }
    if (menuwin == win) { menuwin = NULL; menutitle = 0; }
    if (win->amenu) { frmenu(win->amenu); win->amenu = NULL; }
    while (win->mena) {

        menenaptr me = win->mena;

        win->mena = me->next;
        free(me);

    }
    if (focwin == win) focwin = NULL;
    if (hovwin == win) hovwin = NULL; /* no hover on a freed window */
    if (ctxwin == win) ctxwin = NULL;
    if (drgwin == win) { banderase(); drag = dt_none; drgwin = NULL; }
    for (i = 0; i < AMI_MAXTIM; i++)
        if (timtbl[i] == win) timtbl[i] = NULL;
    if (frmtimwin == win) frmtimwin = NULL;
    free(win->title);
    /* the input side record goes if it serves only this window */
    if (opnfil[fd]->inl >= 0 && opnfil[fd]->inl != fileno(stdin)) {

        int ifn = opnfil[fd]->inl;

        if (opnfil[ifn] && opnfil[ifn]->sfp != stdin) {

            free(opnfil[ifn]);
            opnfil[ifn] = NULL;

        }

    }
    free(opnfil[fd]);
    opnfil[fd] = NULL;
    win->next = winfre; /* recycle the record */
    winfre = win;
    /* the desktop under it shows */
    calcvisall();
    composeall(&r);

}

static int iclose(int fd)

{

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->win &&
        mgractive)
        clswin(fd);
    else if (fd >= 0 && fd < MAXFIL && opnfil[fd] && !opnfil[fd]->win &&
             mgractive && opnfil[fd]->inw) {

        /* an input side closing alone */
        free(opnfil[fd]);
        opnfil[fd] = NULL;

    }

    return ((*ofpclose)(fd));

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    return ((*ofplseek)(fd, offset, whence));

}

#ifdef NOCANCEL
static int iopen_nocancel(const char* pathname, int flags, int perm)

{

    return ((*ofpopen_nocancel)(pathname, flags, perm));

}

static int iclose_nocancel(int fd)

{

    return ((*ofpclose_nocancel)(fd));

}
#endif

/*******************************************************************************

Event translation

The layer reports events on the root surface; the manager routes them to
windows: pointer events by position and Z order, with the frame
interactions -- title drag, sizing edges, the bar buttons -- consumed
here; keyboard events to the focus window; timers by their recorded
owners.

*******************************************************************************/

static ami_long rootmx = 1, rootmy = 1; /* the pointer, root pixels */

/* deliver pending mouse coordinate change for a window, pixel first then
   character, one event per call */
static int moudeliver(winptr win, ami_long cxg, ami_long cyg, ami_evtrec* er)

{

    ami_long cx, cy;

    if (cxg == win->mpxg && cyg == win->mpyg) return (FALSE);
    win->mpxg = cxg;
    win->mpyg = cyg;
    er->etype = ami_etmoumovg;
    er->winid = win->wid;
    er->mmoung = 1;
    er->moupxg = cxg;
    er->moupyg = cyg;
    /* the character grid position follows in the window's font */
    entercli(win);
    cx = (cxg-1)/(*chrsizx_down)(stdout)+1;
    cy = (cyg-1)/(*chrsizy_down)(stdout)+1;
    if (cx != win->mpx || cy != win->mpy) {

        ami_evtrec e2;

        win->mpx = cx;
        win->mpy = cy;
        e2.etype = ami_etmoumov;
        e2.winid = win->wid;
        e2.mmoun = 1;
        e2.moupx = cx;
        e2.moupy = cy;
        enquepaevt(&e2);

    }

    return (TRUE);

}

/* get and translate one layer event; TRUE when er carries a client event */
static int transevt(ami_evtrec* le, ami_evtrec* er)

{

    winptr    tw;
    ami_long  lx, ly, cxg, cyg;
    int       hit;
    rectangle r;

    switch (le->etype) {

        case ami_etmoumovg:
            rootmx = le->moupxg;
            rootmy = le->moupyg;
            if (drag != dt_none) {

                banderase();
                bandcalc(rootmx, rootmy);
                banddraw();
                break;

            }
            tw = winat(rootmx, rootmy);
            /* over a sizing edge the pointer wears the sizing shape,
               the feedback a desktop gives */
            if (grx_pointer) {

                int      shp = 0;
                unsigned e = 0;
                winptr   hw;

                if (tw && frmhit(tw, rootmx-tw->orgx+1,
                                     rootmy-tw->orgy+1) == 5)
                    e = frmedges(tw, rootmx-tw->orgx+1, rootmy-tw->orgy+1);
                else if ((hw = haloat(rootmx, rootmy, tw)))
                    e = frmedges(hw, rootmx-hw->orgx+1, rootmy-hw->orgy+1);
                if (e) {

                    if ((e & 1 && e & 4) || (e & 2 && e & 8)) shp = 3;
                    else if ((e & 1 && e & 8) || (e & 2 && e & 4)) shp = 4;
                    else if (e & (4|8)) shp = 1;
                    else shp = 2;

                }
                if (shp != curshp) { curshp = shp; grx_pointer(shp); }

            }
            if (tw && tw->popup) {

                /* a popup under the pointer tracks its hover row */
                pophover(tw, rootmy-tw->orgy+1);
                break;

            }
            if (tw != hovwin) {

                if (hovwin) quevent(hovwin, ami_etnohover);
                if (tw) quevent(tw, ami_ethover);
                hovwin = tw;

            }
            if (!tw) break;
            lx = rootmx-tw->orgx+1;
            ly = rootmy-tw->orgy+1;
            cxg = lx-tw->coffx;
            cyg = ly-tw->coffy;
            if (cxg >= 1 && cyg >= 1 && cxg <= tw->cmaxx && cyg <= tw->cmaxy)
                return (moudeliver(tw, cxg, cyg, er));
            break;

        case ami_etmoumov:
            break; /* the manager synthesizes the character grid form */

        case ami_etmouba:
            if (drag != dt_none) break; /* the band owns the pointer */
            tw = winat(rootmx, rootmy);
            /* an open menu takes the click first */
            if (popcnt && le->amoubn == 1) {

                if (tw && tw->popup) {

                    popclick(tw, rootmx-tw->orgx+1, rootmy-tw->orgy+1);
                    break;

                }
                if (!(tw && tw->amenu &&
                      frmhit(tw, rootmx-tw->orgx+1,
                                 rootmy-tw->orgy+1) == 6)) {

                    menudismiss();
                    break; /* the dismissing click is spent */

                }

            }
            /* a sizing grab may reach from just outside the window */
            if (le->amoubn == 1) {

                winptr hw = haloat(rootmx, rootmy, tw);

                if (hw) {

                    if (hw->zorder != ztop) {

                        ztotop(hw);
                        calcvisall();
                        winrect(hw, &r);
                        composeall(&r);

                    }
                    setfocus(hw);
                    drag = dt_size;
                    drgwin = hw;
                    drgedges = frmedges(hw, rootmx-hw->orgx+1,
                                        rootmy-hw->orgy+1);
                    drgax = rootmx; drgay = rootmy;
                    drgox = hw->orgx; drgoy = hw->orgy;
                    drgow = hw->pmaxx; drgoh = hw->pmaxy;
                    bandcalc(rootmx, rootmy);
                    banddraw();
                    break;

                }

            }
            if (!tw) break;
            lx = rootmx-tw->orgx+1;
            ly = rootmy-tw->orgy+1;
            hit = frmhit(tw, lx, ly);
            if (le->amoubn == 1) {

                /* click to front and focus */
                if (tw->zorder != ztop && !tw->root) {

                    ztotop(tw);
                    calcvisall();
                    winrect(tw, &r);
                    composeall(&r);

                }
                setfocus(tw);
                if (hit == 1 && !tw->maxed) {

                    /* the title: begin the move band */
                    drag = dt_move;
                    drgwin = tw;
                    drgax = rootmx; drgay = rootmy;
                    drgox = tw->orgx; drgoy = tw->orgy;
                    drgow = tw->pmaxx; drgoh = tw->pmaxy;
                    bandcalc(rootmx, rootmy);
                    banddraw();
                    break;

                }
                if (hit == 5 && !tw->maxed) {

                    /* a sizing edge: begin the size band */
                    drag = dt_size;
                    drgwin = tw;
                    drgedges = frmedges(tw, lx, ly);
                    drgax = rootmx; drgay = rootmy;
                    drgox = tw->orgx; drgoy = tw->orgy;
                    drgow = tw->pmaxx; drgoh = tw->pmaxy;
                    bandcalc(rootmx, rootmy);
                    banddraw();
                    break;

                }
                if (hit == 6) {

                    /* the menu bar: open or select */
                    barpress(tw, mbarhitn(tw, lx));
                    break;

                }
                if (hit == 2) {

                    /* close: terminate to the window; for a toplevel this
                       is the user ordering exit */
                    er->etype = ami_etterm;
                    er->winid = tw->wid;
                    if (!tw->parwin) fend = TRUE;
                    return (TRUE);

                }
                if (hit == 3) {

                    /* maximize, or restore a maximized window */
                    if (tw->maxed) { intnormg(tw); quevent(tw, ami_etnorm); }
                    else if (!tw->root) {

                        intmaxg(tw);
                        quevent(tw, ami_etmax);

                    }
                    break;

                }
                if (hit == 4) {

                    /* minimize, or restore a minimized window */
                    if (tw->mined) { intnormg(tw); quevent(tw, ami_etnorm); }
                    else if (!tw->root) {

                        intming(tw);
                        quevent(tw, ami_etmin);

                    }
                    break;

                }

            }
            if (!hit) {

                er->etype = ami_etmouba;
                er->winid = tw->wid;
                er->amoun = 1;
                er->amoubn = le->amoubn;
                return (TRUE);

            }
            break;

        case ami_etmoubd:
            if (drag != dt_none && le->dmoubn == 1) {

                dragend(rootmx, rootmy);
                break;

            }
            tw = winat(rootmx, rootmy);
            if (!tw) break;
            if (tw->popup) break; /* releases on a popup stay internal */
            lx = rootmx-tw->orgx+1;
            ly = rootmy-tw->orgy+1;
            if (!frmhit(tw, lx, ly)) {

                er->etype = ami_etmoubd;
                er->winid = tw->wid;
                er->dmoun = 1;
                er->dmoubn = le->dmoubn;
                return (TRUE);

            }
            break;

        case ami_ettim:
            *er = *le;
            er->winid = (le->timnum >= 1 && le->timnum <= AMI_MAXTIM &&
                         timtbl[le->timnum-1])?
                        timtbl[le->timnum-1]->wid: 1;
            return (TRUE);

        case ami_etframe:
            *er = *le;
            er->winid = frmtimwin? frmtimwin->wid: 1;
            return (TRUE);

        case ami_etterm:
            fend = TRUE;
            *er = *le;
            er->winid = focwin? focwin->wid: 1;
            return (TRUE);

        default:
            /* everything else -- keyboard, joystick, function keys --
               belongs to the focus window */
            *er = *le;
            er->winid = focwin? focwin->wid: 1;
            return (TRUE);

    }

    return (FALSE);

}

static void ievent(FILE* f, ami_evtrec* er)

{

    ami_evtrec le;
    int        got = FALSE;

    while (!got) {

        /* Processing a layer event can queue client events -- a drag
           release queues the resize, a focus change its notices. They
           deliver before the layer is pulled again, or a client sitting
           in this loop on a quiet display would never hear them. */
        if (dequepaevt(er)) return;
        (*event_down)(stdin, &le); /* the layer's next event */
        got = transevt(&le, er);

    }

}

/* the default handler passes the event through to the caller */
static void defaultevent(ami_evtrec* ev)

{

    ev->handled = 0;

}

void _pa_event_ovr(ami_event_t nfp, ami_event_t* ofp);
static ami_event_t event_vect;

static void event_ivf(FILE* f, ami_evtrec* er)

{

    do {

        if (!dequepaevt(er)) ievent(f, er);
        er->handled = 1;
        (evtshan)(er);
        if (!er->handled && er->etype <= ami_etdsize) {

            er->handled = 1;
            (*evthan[er->etype])(er);

        }

    } while (er->handled);

}

static ami_eventover_t  eventover_vect;
static ami_eventsover_t eventsover_vect;
static ami_sendevent_t  sendevent_vect;

static void eventover_ivf(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)

{

    if (e > ami_etdsize) error("Cannot vector auxiliary event");
    *oeh = evthan[e];
    evthan[e] = eh;

}

static void eventsover_ivf(ami_pevthan eh, ami_pevthan* oeh)

{

    *oeh = evtshan;
    evtshan = eh;

}

static void sendevent_ivf(FILE* f, ami_evtrec* er)

{

    ami_evtrec ec;
    winptr     win;

    win = txt2win(f);
    memcpy(&ec, er, sizeof(ami_evtrec));
    ec.winid = win->wid;
    enquepaevt(&ec);

}

/*******************************************************************************

Timers

The layer's timers are a shared set; the manager records which window
asked for each so the maturing event routes home.

*******************************************************************************/

static void timer_ivf(FILE* f, ami_long i, ami_long t, ami_long r)

{

    winptr win;

    if (i < 1 || i > AMI_MAXTIM) error("Invalid timer handle");
    win = txt2win(f);
    timtbl[i-1] = win;
    (*timer_down)(stdout, i, t, r);

}

static void killtimer_ivf(FILE* f, ami_long i)

{

    if (i < 1 || i > AMI_MAXTIM) error("Invalid timer handle");
    (*killtimer_down)(stdout, i);
    timtbl[i-1] = NULL;

}

static void frametimer_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    frmtimwin = e? win: NULL;
    (*frametimer_down)(stdout, e);

}

static void autohold_ivf(ami_long e)

{

    fautohold = e;
    (*autohold_down)(e);

}

/*******************************************************************************

Redirected calls

The client's drawing, text and state calls redirect into the window's
client backing: the manager enters the window's context and passes the
call down unchanged, since client coordinates are the backing screen's
own; a call that marks pixels then composes the window's damage to the
display.

*******************************************************************************/

static void line_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*line_down)(stdout, x1, y1, x2, y2);
    compdmg(win);

}

static void rect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*rect_down)(stdout, x1, y1, x2, y2);
    compdmg(win);

}

static void frect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*frect_down)(stdout, x1, y1, x2, y2);
    compdmg(win);

}

static void rrect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*rrect_down)(stdout, x1, y1, x2, y2, xs, ys);
    compdmg(win);

}

static void frrect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*frrect_down)(stdout, x1, y1, x2, y2, xs, ys);
    compdmg(win);

}

static void ellipse_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*ellipse_down)(stdout, x1, y1, x2, y2);
    compdmg(win);

}

static void fellipse_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fellipse_down)(stdout, x1, y1, x2, y2);
    compdmg(win);

}

static void arc_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*arc_down)(stdout, x1, y1, x2, y2, sa, ea);
    compdmg(win);

}

static void farc_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*farc_down)(stdout, x1, y1, x2, y2, sa, ea);
    compdmg(win);

}

static void fchord_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fchord_down)(stdout, x1, y1, x2, y2, sa, ea);
    compdmg(win);

}

static void ftriangle_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*ftriangle_down)(stdout, x1, y1, x2, y2, x3, y3);
    compdmg(win);

}

static void setpixel_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*setpixel_down)(stdout, x, y);
    compdmg(win);

}

static void scroll_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*scroll_down)(stdout, x, y);
    compdmg(win);

}

static void scrollg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*scrollg_down)(stdout, x, y);
    compdmg(win);

}

static void picture_ivf(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*picture_down)(stdout, p, x1, y1, x2, y2);
    compdmg(win);

}

static void writejust_ivf(FILE* f, const char* s, ami_long n)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*writejust_down)(stdout, s, n);
    compdmg(win);

}

static void del_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*del_down)(stdout);
    compdmg(win);

}

static void home_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*home_down)(stdout);
    compdmg(win);

}

static void up_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*up_down)(stdout);
    compdmg(win);

}

static void down_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*down_down)(stdout);
    compdmg(win);

}

static void left_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*left_down)(stdout);
    compdmg(win);

}

static void right_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*right_down)(stdout);
    compdmg(win);

}

static void wrtstr_ivf(FILE* f, char* s)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*wrtstr_down)(stdout, s);
    compdmg(win);

}

static void wrtstrn_ivf(FILE* f, char* s, ami_long n)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*wrtstrn_down)(stdout, s, n);
    compdmg(win);

}

static void cursor_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*cursor_down)(stdout, x, y);
    caroff();
    caron();

}

static void cursorg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*cursorg_down)(stdout, x, y);
    caroff();
    caron();

}

static void auto_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    win->autof[win->curupd-1] = !!e;
    entercli(win);
    (*auto_down)(stdout, e);

}

static void curvis_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    /* the manager owns the caret; the layer's stays dark */
    win->curvf[win->curupd-1] = !!e;
    caroff();
    caron();

}

static void fcolor_ivf(FILE* f, ami_color c)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fcolor_down)(stdout, c);

}

static void bcolor_ivf(FILE* f, ami_color c)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*bcolor_down)(stdout, c);

}

static void fcolorc_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fcolorc_down)(stdout, r, g, b);

}

static void bcolorc_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*bcolorc_down)(stdout, r, g, b);

}

static void fcolorg_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fcolorg_down)(stdout, r, g, b);

}

static void bcolorg_ivf(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*bcolorg_down)(stdout, r, g, b);

}

static void fover_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fover_down)(stdout);

}

static void bover_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*bover_down)(stdout);

}

static void finvis_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*finvis_down)(stdout);

}

static void binvis_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*binvis_down)(stdout);

}

static void fxor_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fxor_down)(stdout);

}

static void bxor_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*bxor_down)(stdout);

}

static void fand_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fand_down)(stdout);

}

static void band_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*band_down)(stdout);

}

static void for_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*for_down)(stdout);

}

static void bor_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*bor_down)(stdout);

}

static void linewidth_ivf(FILE* f, ami_long w)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*linewidth_down)(stdout, w);

}

static void linestyle_ivf(FILE* f, ami_lstyle style)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*linestyle_down)(stdout, style);

}

static void settab_ivf(FILE* f, ami_long t)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*settab_down)(stdout, t);

}

static void settabg_ivf(FILE* f, ami_long t)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*settabg_down)(stdout, t);

}

static void restab_ivf(FILE* f, ami_long t)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*restab_down)(stdout, t);

}

static void restabg_ivf(FILE* f, ami_long t)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*restabg_down)(stdout, t);

}

static void clrtab_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*clrtab_down)(stdout);

}

static void viewoffg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*viewoffg_down)(stdout, x, y);

}

static void viewscale_ivf(FILE* f, float x, float y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*viewscale_down)(stdout, x, y);

}

static void path_ivf(FILE* f, ami_long a)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*path_down)(stdout, a);

}

static void blink_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*blink_down)(stdout, e);

}

static void reverse_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*reverse_down)(stdout, e);

}

static void underline_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*underline_down)(stdout, e);

}

static void superscript_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*superscript_down)(stdout, e);

}

static void subscript_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*subscript_down)(stdout, e);

}

static void strikeout_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*strikeout_down)(stdout, e);

}

static void standout_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*standout_down)(stdout, e);

}

static void loadpict_ivf(FILE* f, ami_long p, char* fn)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*loadpict_down)(stdout, p, fn);

}

static void delpict_ivf(FILE* f, ami_long p)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*delpict_down)(stdout, p);

}

static ami_long curx_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*curx_down)(stdout));

}

static ami_long cury_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*cury_down)(stdout));

}

static ami_long curxg_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*curxg_down)(stdout));

}

static ami_long curyg_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*curyg_down)(stdout));

}

static ami_long curbnd_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*curbnd_down)(stdout));

}

static ami_long chrsizx_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*chrsizx_down)(stdout));

}

static ami_long chrsizy_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*chrsizy_down)(stdout));

}

static ami_long baseline_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*baseline_down)(stdout));

}

static ami_long strsiz_ivf(FILE* f, const char* s)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*strsiz_down)(stdout, s));

}

static ami_long chrpos_ivf(FILE* f, const char* s, ami_long p)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*chrpos_down)(stdout, s, p));

}

static ami_long justpos_ivf(FILE* f, const char* s, ami_long p, ami_long n)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*justpos_down)(stdout, s, p, n));

}

static ami_long dpmx_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*dpmx_down)(stdout));

}

static ami_long dpmy_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*dpmy_down)(stdout));

}

static ami_long fonts_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*fonts_down)(stdout));

}

static float points_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*points_down)(stdout));

}

static ami_long pictsizx_ivf(FILE* f, ami_long p)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*pictsizx_down)(stdout, p));

}

static ami_long pictsizy_ivf(FILE* f, ami_long p)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*pictsizy_down)(stdout, p));

}

static ami_long scalex_ivf(FILE* f, ami_long x)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*scalex_down)(stdout, x));

}

static ami_long scaley_ivf(FILE* f, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return ((*scaley_down)(stdout, y));

}

static void fontnam_ivf(FILE* f, ami_long fc, char* fns, ami_long fnsl)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    (*fontnam_down)(stdout, fc, fns, fnsl);

}

static ami_long mouse_ivf(FILE* f)

{

    return ((*mouse_down)(stdout));

}

static ami_long mousebutton_ivf(FILE* f, ami_long m)

{

    return ((*mousebutton_down)(stdout, m));

}

static ami_long joystick_ivf(FILE* f)

{

    return ((*joystick_down)(stdout));

}

static ami_long joybutton_ivf(FILE* f, ami_long j)

{

    return ((*joybutton_down)(stdout, j));

}

static ami_long joyaxis_ivf(FILE* f, ami_long j)

{

    return ((*joyaxis_down)(stdout, j));

}

static ami_long funkey_ivf(FILE* f)

{

    return ((*funkey_down)(stdout));

}

/*******************************************************************************

Font and size calls

The font context is per window; each setting records in the window and
passes down, and the record reapplies whenever the layer's context
switches windows.

*******************************************************************************/

static void font_ivf(FILE* f, ami_long fc)

{

    winptr win;

    win = txt2win(f);
    win->font = fc;
    entercli(win);
    (*font_down)(stdout, fc);

}

static void fontsiz_ivf(FILE* f, ami_long s)

{

    winptr win;

    win = txt2win(f);
    win->fontsiz = s;
    win->points = 0;
    entercli(win);
    (*fontsiz_down)(stdout, s);

}

static void setpoints_ivf(FILE* f, float ps)

{

    winptr win;

    win = txt2win(f);
    win->points = ps;
    win->fontsiz = 0;
    entercli(win);
    (*setpoints_down)(stdout, ps);

}

static void chrspcx_ivf(FILE* f, ami_long s)

{

    winptr win;

    win = txt2win(f);
    win->chrspcx = s;
    entercli(win);
    (*chrspcx_down)(stdout, s);

}

static void chrspcy_ivf(FILE* f, ami_long s)

{

    winptr win;

    win = txt2win(f);
    win->chrspcy = s;
    entercli(win);
    (*chrspcy_down)(stdout, s);

}

/* an attribute that reselects the font: record the bit, pass down */
static void fontattr(FILE* f, ami_long e, int bit, void (*down)(FILE*, ami_long))

{

    winptr win;

    win = txt2win(f);
    if (e) win->attrs |= bit; else win->attrs &= ~bit;
    entercli(win);
    (*down)(stdout, e);

}

static void bold_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WABOLD, bold_down); }
static void italic_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WAITAL, italic_down); }
static void condensed_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WACOND, condensed_down); }
static void extended_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WAEXT, extended_down); }
static void xlight_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WAXLIGHT, xlight_down); }
static void light_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WALIGHT, light_down); }
static void xbold_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WAXBOLD, xbold_down); }
static void hollow_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WAHOLLOW, hollow_down); }
static void raised_ivf(FILE* f, ami_long e)
    { fontattr(f, e, WARAISED, raised_down); }

/*******************************************************************************

Sizes, screens and buffers

*******************************************************************************/

static ami_long maxx_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return (win->bufx/(*chrsizx_down)(stdout));

}

static ami_long maxy_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    entercli(win);

    return (win->bufy/(*chrsizy_down)(stdout));

}

static ami_long maxxg_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);

    return (win->bufx);

}

static ami_long maxyg_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);

    return (win->bufy);

}

static void select_ivf(FILE* f, ami_long u, ami_long d)

{

    winptr win;

    win = txt2win(f);
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error("Invalid screen number");
    win->curupd = u;
    if (win->curdsp != d) {

        win->curdsp = d;
        /* the display switch shows at once */
        if (!win->scns[d-1]) alcbacking(win, d-1);
        compdmg(win);

    }
    entercli(win);
    caroff();
    caron();

}

static void buffer_ivf(FILE* f, ami_long e)

{

    winptr win;

    win = txt2win(f);
    /* the backing is kept either way; the flag notes the client's model */
    win->bufmod = !!e;
    trackbuf(win); /* an unbuffered surface follows the window */

}

static void sizbufg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    if (x < 1) x = 1;
    if (y < 1) y = 1;
    win->bufx = x;
    win->bufy = y;
    entercli(win);
    (*sizbufg_down)(stdout, x, y);
    compdmg(win);

}

static void sizbuf_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    ami_sizbufg(f, x*(*chrsizx_down)(stdout), y*(*chrsizy_down)(stdout));

}

/*******************************************************************************

Windows

*******************************************************************************/

static winptr getwin(void)

{

    winptr p;

    if (winfre) { p = winfre; winfre = p->next; }
    else {

        p = malloc(sizeof(winrec));
        if (!p) error("Out of memory");

    }
    memset(p, 0, sizeof(winrec));

    return (p);

}

/* set up a new window record and present it */
static void opnwin(int fn, int pfn, ami_long wid, int root)

{

    winptr    win;
    winptr    pw = NULL;
    rectangle r;
    static int cascade;

    win = lfn2win(fn);
    win->root = root;
    win->parlfn = pfn;
    win->wid = wid;
    if (pfn >= 0) pw = lfn2win(pfn);
    win->parwin = pw;
    if (pw) {

        win->childlst = pw->childwin;
        pw->childwin = win;

    }
    win->winlst = winlst;
    winlst = win;
    win->amenu = NULL;
    win->mena = NULL;
    win->mtn = 0;
    win->popup = FALSE;
    win->pitems = NULL;
    win->mowner = NULL;
    win->psel = 0;
    win->phov = 0;
    win->curdsp = 1;
    win->curupd = 1;
    win->bufmod = TRUE;
    { int i; for (i = 0; i < MAXCON; i++)
        { win->autof[i] = TRUE; win->curvf[i] = TRUE; } }
    /* a window shows when it first has content, as on the other
       backends: the widget package's metrics window, never drawn to,
       never appears */
    /* the font context starts at the explicit defaults, so entering the
       window always restores them over whatever the frame drawing or
       another window left in the layer */
    win->font = AMI_FONT_TERM;
    win->fontsiz = rootcell;
    win->inpptr = -1;
    win->visible = root; /* the root is the surface; the rest show on
                            first content */
    win->title = strdup(program_invocation_short_name);
    win->zorder = ++ztop;
    if (root) {

        /* the root surface: maximized, frameless, transparent */
        win->frame = FALSE;
        win->size = FALSE;
        win->sysbar = FALSE;
        win->orgx = 1;
        win->orgy = 1;
        win->cmaxx = dimxg;
        win->cmaxy = dimyg;
        win->maxed = TRUE;

    } else {

        win->frame = TRUE;
        win->size = TRUE;
        win->sysbar = TRUE;
        /* the standard cascade */
        win->cmaxx = dimxg/2;
        win->cmaxy = dimyg/2;
        win->orgx = dimxg/8+cascade*BARH(rootcell);
        win->orgy = dimyg/8+cascade*BARH(rootcell);
        cascade = (cascade+1)%8;

    }
    win->bufx = win->cmaxx;
    win->bufy = win->cmaxy;
    frmmetrics(win);
    /* the client backing */
    alcbacking(win, 0);
    /* the frame backing and image */
    if (win->frame) {

        win->frmscn = alcscn();
        drwfrm(win);

    }
    calcvisall();
    if (win->visible) {

        winrect(win, &r);
        composeall(&r);

    }

}

/* open an input and output pair for a window */
static void iopenwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)

{

    int ifn, ofn, pfn, fn;

    /* Window files park on the null device; the manager is the content.
       The input side reuses a known open file -- stdin, or another
       window's input -- and anything else opens fresh. The output side
       always opens fresh: callers reuse their file pointer variables
       across close and open, the windowc convention, so a stale pointer
       must never be trusted. */
    ifn = -1;
    for (fn = 0; fn < MAXFIL; fn++)
        if (opnfil[fn] && opnfil[fn]->sfp == *infile) ifn = fn;
    if (ifn < 0) {

        *infile = fopen(NULLDEV, "r");
        if (!*infile) error("Cannot open window file");
        setvbuf(*infile, NULL, _IONBF, 0);
        ifn = fileno(*infile);

    }
    *outfile = fopen(NULLDEV, "w");
    if (!*outfile) error("Cannot open window file");
    setvbuf(*outfile, NULL, _IONBF, 0);
    ofn = fileno(*outfile);
    if (ifn < 0 || ofn < 0 || ifn >= MAXFIL || ofn >= MAXFIL)
        error("Invalid file handle");
    pfn = -1;
    if (parent) {

        pfn = fileno(parent);
        (void)lfn2win(pfn); /* validate */

    }
    if (!opnfil[ofn]) getfil(&opnfil[ofn]);
    if (!opnfil[ifn]) getfil(&opnfil[ifn]);
    opnfil[ofn]->inl = ifn;
    opnfil[ifn]->inw = TRUE;
    opnfil[ifn]->sfp = *infile;
    opnfil[ofn]->sfp = *outfile;
    if (wid < -MAXFIL || wid >= MAXFIL || !wid) error("Invalid window id");
    if (xltwin[wid+MAXFIL] >= 0) error("Window id in use");
    xltwin[wid+MAXFIL] = ofn;
    filwin[ofn] = wid;
    opnfil[ofn]->win = getwin();
    opnwin(ofn, pfn, wid, FALSE);

}

static void openwin_ivf(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)

{

    iopenwin(infile, outfile, parent, wid);

}

static ami_long getwinid_ivf(void)

{

    ami_long wid;

    /* allocated ids are negative -- buried -- so they never collide
       with the positive ids programs pick for themselves */
    wid = -1;
    while (wid > -MAXFIL && xltwin[wid+MAXFIL] >= 0) wid--;
    if (wid == -MAXFIL) error("No more window ids");

    return (wid);

}

static void title_ivf(FILE* f, char* ts)

{

    winptr    win;
    rectangle r;

    win = txt2win(f);
    free(win->title);
    win->title = strdup(ts? ts: "");
    if (win->frame) {

        drwfrm(win);
        winrect(win, &r);
        composewin(win, &r);

    }

}

/* apply a geometry change: metrics, frame, visibility, composition */
static void regeom(winptr win, rectangle* old)

{

    rectangle r;

    frmmetrics(win);
    trackbuf(win); /* an unbuffered surface follows the window */
    if ((win->frame || win->amenu) && !win->frmscn) win->frmscn = alcscn();
    if (win->frame || win->amenu) drwfrm(win);
    calcvisall();
    winrect(win, &r);
    if (old) {

        if (old->x1 < r.x1) r.x1 = old->x1;
        if (old->y1 < r.y1) r.y1 = old->y1;
        if (old->x2 > r.x2) r.x2 = old->x2;
        if (old->y2 > r.y2) r.y2 = old->y2;

    }
    composeall(&r);

}

static void setsizg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr     win;
    rectangle  old;
    ami_long   ocx, ocy;
    ami_evtrec er;

    win = txt2win(f);
    /* the root is the whole surface: it cannot size */
    if (win->root) error("Cannot size the root window");
    winrect(win, &old);
    ocx = win->cmaxx;
    ocy = win->cmaxy;
    /* the size given is the whole window; the client area follows from
       the frame */
    win->pmaxx = x;
    win->pmaxy = y;
    /* the client is what the decorations leave: coffy carries the top
       chrome including any menu band, as frmmetrics keeps it */
    win->cmaxx = x-(win->frame? 2*BORD(win): 0);
    win->cmaxy = y-win->coffy-(win->frame? BORD(win): 0);
    if (win->cmaxx < 1) win->cmaxx = 1;
    if (win->cmaxy < 1) win->cmaxy = 1;
    regeom(win, &old);
    if (win->cmaxx != ocx || win->cmaxy != ocy) {

        /* the client hears its new size, as the desktop backends
           deliver after their window manager round trip */
        er.etype = ami_etresize;
        er.winid = win->wid;
        er.rszxg = win->cmaxx;
        er.rszyg = win->cmaxy;
        entercli(win); /* the metrics in the window's font */
        er.rszx = win->cmaxx/(*chrsizx_down)(stdout);
        er.rszy = win->cmaxy/(*chrsizy_down)(stdout);
        enquepaevt(&er);
        /* and the redraw notice the desktop backends send after it */
        er.etype = ami_etredraw;
        er.winid = win->wid;
        enquepaevt(&er);

    }

}

static void setsiz_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    setsizg_ivf(f, x*(*chrsizx_down)(stdout)+(win->frame? 2*BORD(win): 0),
                y*(*chrsizy_down)(stdout)+
                    (win->frame? BORD(win)+(win->sysbar? BARH(rootcell): 0)
                                +BORD(win): 0));

}

static void getsizg_ivf(FILE* f, ami_long* x, ami_long* y)

{

    winptr win;

    win = txt2win(f);
    if (x) *x = win->pmaxx;
    if (y) *y = win->pmaxy;

}

static void getsiz_ivf(FILE* f, ami_long* x, ami_long* y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    if (x) *x = win->pmaxx/(*chrsizx_down)(stdout);
    if (y) *y = win->pmaxy/(*chrsizy_down)(stdout);

}

static void setposg_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr    win;
    rectangle old;

    win = txt2win(f);
    /* nothing lies behind the root: it cannot move */
    if (win->root) error("Cannot move the root window");
    winrect(win, &old);
    /* a child's coordinates are relative to its parent's client */
    {

        ami_long ox, oy, dx, dy;

        parorigin(win, &ox, &oy);
        dx = ox+x-win->orgx;
        dy = oy+y-win->orgy;
        win->orgx += dx;
        win->orgy += dy;
        movekids(win, dx, dy);

    }
    calcvisall();
    regeom(win, &old);

}

static void setpos_ivf(FILE* f, ami_long x, ami_long y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    setposg_ivf(f, (x-1)*(*chrsizx_down)(stdout)+1,
                (y-1)*(*chrsizy_down)(stdout)+1);

}

static void scnsizg_ivf(FILE* f, ami_long* x, ami_long* y)

{

    if (x) *x = dimxg;
    if (y) *y = dimyg;

}

static void scnsiz_ivf(FILE* f, ami_long* x, ami_long* y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    if (x) *x = dimxg/(*chrsizx_down)(stdout);
    if (y) *y = dimyg/(*chrsizy_down)(stdout);

}

static void scnceng_ivf(FILE* f, ami_long* x, ami_long* y)

{

    if (x) *x = dimxg/2;
    if (y) *y = dimyg/2;

}

static void scncen_ivf(FILE* f, ami_long* x, ami_long* y)

{

    winptr win;

    win = txt2win(f);
    entercli(win);
    if (x) *x = dimxg/(*chrsizx_down)(stdout)/2;
    if (y) *y = dimyg/(*chrsizy_down)(stdout)/2;

}

static void winclientg_ivf(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy,
                           ami_winmodset ms)

{

    ami_long fx = 0, fy = 0;

    if (BIT(ami_wmframe) & ms) {

        fx = 2*FRMBORDER;
        fy = 2*FRMBORDER;
        if (BIT(ami_wmsysbar) & ms) fy += BARH(rootcell);

    }
    /* the menu row is not a frame decoration: it is a band above the
       client, frameless or not, as frmmetrics counts it */
    if (BIT(ami_wmmenu) & ms) fy += BARH(rootcell);
    if (wx) *wx = cx+fx;
    if (wy) *wy = cy+fy;

}

static void winclient_ivf(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy,
                          ami_winmodset ms)

{

    winptr win;
    ami_long   csx, csy;

    win = txt2win(f);
    entercli(win);
    csx = (*chrsizx_down)(stdout);
    csy = (*chrsizy_down)(stdout);
    winclientg_ivf(f, cx*csx, cy*csy, wx, wy, ms);
    if (wx) *wx = (*wx+csx-1)/csx;
    if (wy) *wy = (*wy+csy-1)/csy;

}

static void front_ivf(FILE* f)

{

    winptr    win;
    rectangle r;

    win = txt2win(f);
    /* the root floors the Z order: it cannot reorder */
    if (win->root) error("Cannot order the root window");
    if (win->zorder != ztop) {

        ztotop(win);
        calcvisall();
        winrect(win, &r);
        composeall(&r);

    }

}

static void back_ivf(FILE* f)

{

    winptr    win, wp;
    rectangle r;

    win = txt2win(f);
    /* the root floors the Z order: it cannot reorder */
    if (win->root) error("Cannot order the root window");
    {

        /* To back means the back of the window's own pile: behind its
           siblings, above its parent -- a child under its parent would
           be occluded whole -- or above the root floor for a window
           with no parent. */
        ami_long fl = win->parwin? win->parwin->zorder+1: 1;

        if (win->zorder > fl) {

            for (wp = winlst; wp; wp = wp->winlst)
                if (wp != win && wp->zorder >= fl &&
                    wp->zorder < win->zorder)
                    wp->zorder++;
            win->zorder = fl;
            calcvisall();
            winrect(win, &r);
            composeall(&r);

        }

    }

}

static void frame_ivf(FILE* f, ami_long e)

{

    winptr    win;
    rectangle old;

    win = txt2win(f);
    if (!!e == win->frame) return;
    winrect(win, &old);
    win->frame = !!e;
    regeom(win, &old);

}

static void sizable_ivf(FILE* f, ami_long e)

{

    winptr    win;
    rectangle old;

    win = txt2win(f);
    if (!!e == win->size) return;
    winrect(win, &old);
    /* the sizing border comes and goes with the ability */
    win->size = !!e;
    regeom(win, &old);

}

static void sysbar_ivf(FILE* f, ami_long e)

{

    winptr    win;
    rectangle old;

    win = txt2win(f);
    if (!!e == win->sysbar) return;
    winrect(win, &old);
    win->sysbar = !!e;
    regeom(win, &old);

}

static void focus_ivf(FILE* f)

{

    setfocus(txt2win(f));

}


/* blockcopyg: the client's screen numbers map to the window's backing
   screens in the layer */
static void blockcopyg_ivf(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1,
                           ami_long sx2, ami_long sy2, ami_long dx1, ami_long dy1,
                           ami_long dx2, ami_long dy2)

{

    winptr win;
    int    i;

    win = txt2win(f);
    if (s < 1 || s > MAXCON || d < 1 || d > MAXCON)
        error("Invalid screen number");
    for (i = 0; i < 2; i++) {

        ami_long n = i? d: s;

        if (!win->scns[n-1]) alcbacking(win, n-1);

    }
    entercli(win);
    (*blockcopyg_down)(stdout, win->scns[s-1], win->scns[d-1],
                       sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2);
    if (d == win->curdsp) compdmg(win);

}

/*******************************************************************************

Initialize the manager

Runs after the graphics layer is up (its constructor is below this
priority) and before the widget packages (theirs is above), hooking
every call it manages and standing up the root surface window.

*******************************************************************************/

static void init_windowg(void) __attribute__((constructor (104)));
static void init_windowg(void)

{

    int        fn;
    ami_evtcod e;
    int        ifn, ofn;

    winfre = NULL;
    winlst = NULL;
    ztop = -1;
    focwin = NULL;
    hovwin = NULL;
    fend = FALSE;
    fautohold = TRUE;
    drag = dt_none;
    drgwin = NULL;
    banddrawn = FALSE;
    carshown = FALSE;
    incar = FALSE;
    curshp = 0;
    wgheld = FALSE;
    popcnt = 0;
    menuwin = NULL;
    menutitle = 0;
    carhold = 0;
    ctxwin = NULL;
    paqfre = NULL;
    paqevt = NULL;
    frmtimwin = NULL;
    for (fn = 0; fn < MAXFIL; fn++) {

        opnfil[fn] = NULL;
        filwin[fn] = -1;

    }
    for (fn = 0; fn < MAXFIL*2+1; fn++) xltwin[fn] = -1;
    for (fn = 0; fn <= MAXCON*10; fn++) scnuse[fn] = FALSE;
    scnuse[DSPSCN] = TRUE; /* the display is spoken for */
    scnmax = MAXCON*10; /* the layer's screen range this manager uses */
    for (fn = 0; fn < AMI_MAXTIM; fn++) timtbl[fn] = NULL;
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_etdsize; e++) evthan[e] = defaultevent;

    /* hook the system I/O vectors */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);
#ifdef NOCANCEL
    ovr_read_nocancel(iread_nocancel, &ofpread_nocancel);
    ovr_write_nocancel(iwrite_nocancel, &ofpwrite_nocancel);
    ovr_open_nocancel(iopen_nocancel, &ofpopen_nocancel);
    ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);
#endif

    /* hook the API */
    _pa_event_ovr(event_ivf, &event_down);
    _pa_eventover_ovr(eventover_ivf, &eventover_down);
    _pa_eventsover_ovr(eventsover_ivf, &eventsover_down);
    _pa_sendevent_ovr(sendevent_ivf, &sendevent_down);
    _pa_timer_ovr(timer_ivf, &timer_down);
    _pa_killtimer_ovr(killtimer_ivf, &killtimer_down);
    _pa_frametimer_ovr(frametimer_ivf, &frametimer_down);
    _pa_autohold_ovr(autohold_ivf, &autohold_down);
    _pa_line_ovr(line_ivf, &line_down);
    _pa_rect_ovr(rect_ivf, &rect_down);
    _pa_frect_ovr(frect_ivf, &frect_down);
    _pa_rrect_ovr(rrect_ivf, &rrect_down);
    _pa_frrect_ovr(frrect_ivf, &frrect_down);
    _pa_ellipse_ovr(ellipse_ivf, &ellipse_down);
    _pa_fellipse_ovr(fellipse_ivf, &fellipse_down);
    _pa_arc_ovr(arc_ivf, &arc_down);
    _pa_farc_ovr(farc_ivf, &farc_down);
    _pa_fchord_ovr(fchord_ivf, &fchord_down);
    _pa_ftriangle_ovr(ftriangle_ivf, &ftriangle_down);
    _pa_setpixel_ovr(setpixel_ivf, &setpixel_down);
    _pa_scroll_ovr(scroll_ivf, &scroll_down);
    _pa_scrollg_ovr(scrollg_ivf, &scrollg_down);
    _pa_picture_ovr(picture_ivf, &picture_down);
    _pa_writejust_ovr(writejust_ivf, &writejust_down);
    _pa_del_ovr(del_ivf, &del_down);
    _pa_home_ovr(home_ivf, &home_down);
    _pa_up_ovr(up_ivf, &up_down);
    _pa_down_ovr(down_ivf, &down_down);
    _pa_left_ovr(left_ivf, &left_down);
    _pa_right_ovr(right_ivf, &right_down);
    _pa_wrtstr_ovr(wrtstr_ivf, &wrtstr_down);
    _pa_wrtstrn_ovr(wrtstrn_ivf, &wrtstrn_down);
    _pa_cursor_ovr(cursor_ivf, &cursor_down);
    _pa_cursorg_ovr(cursorg_ivf, &cursorg_down);
    _pa_auto_ovr(auto_ivf, &auto_down);
    _pa_curvis_ovr(curvis_ivf, &curvis_down);
    _pa_fcolor_ovr(fcolor_ivf, &fcolor_down);
    _pa_bcolor_ovr(bcolor_ivf, &bcolor_down);
    _pa_fcolorc_ovr(fcolorc_ivf, &fcolorc_down);
    _pa_bcolorc_ovr(bcolorc_ivf, &bcolorc_down);
    _pa_fcolorg_ovr(fcolorg_ivf, &fcolorg_down);
    _pa_bcolorg_ovr(bcolorg_ivf, &bcolorg_down);
    _pa_fover_ovr(fover_ivf, &fover_down);
    _pa_bover_ovr(bover_ivf, &bover_down);
    _pa_finvis_ovr(finvis_ivf, &finvis_down);
    _pa_binvis_ovr(binvis_ivf, &binvis_down);
    _pa_fxor_ovr(fxor_ivf, &fxor_down);
    _pa_bxor_ovr(bxor_ivf, &bxor_down);
    _pa_fand_ovr(fand_ivf, &fand_down);
    _pa_band_ovr(band_ivf, &band_down);
    _pa_for_ovr(for_ivf, &for_down);
    _pa_bor_ovr(bor_ivf, &bor_down);
    _pa_linewidth_ovr(linewidth_ivf, &linewidth_down);
    _pa_linestyle_ovr(linestyle_ivf, &linestyle_down);
    _pa_settab_ovr(settab_ivf, &settab_down);
    _pa_settabg_ovr(settabg_ivf, &settabg_down);
    _pa_restab_ovr(restab_ivf, &restab_down);
    _pa_restabg_ovr(restabg_ivf, &restabg_down);
    _pa_clrtab_ovr(clrtab_ivf, &clrtab_down);
    _pa_viewoffg_ovr(viewoffg_ivf, &viewoffg_down);
    _pa_viewscale_ovr(viewscale_ivf, &viewscale_down);
    _pa_path_ovr(path_ivf, &path_down);
    _pa_blink_ovr(blink_ivf, &blink_down);
    _pa_reverse_ovr(reverse_ivf, &reverse_down);
    _pa_underline_ovr(underline_ivf, &underline_down);
    _pa_superscript_ovr(superscript_ivf, &superscript_down);
    _pa_subscript_ovr(subscript_ivf, &subscript_down);
    _pa_strikeout_ovr(strikeout_ivf, &strikeout_down);
    _pa_standout_ovr(standout_ivf, &standout_down);
    _pa_loadpict_ovr(loadpict_ivf, &loadpict_down);
    _pa_delpict_ovr(delpict_ivf, &delpict_down);
    _pa_curx_ovr(curx_ivf, &curx_down);
    _pa_cury_ovr(cury_ivf, &cury_down);
    _pa_curxg_ovr(curxg_ivf, &curxg_down);
    _pa_curyg_ovr(curyg_ivf, &curyg_down);
    _pa_curbnd_ovr(curbnd_ivf, &curbnd_down);
    _pa_chrsizx_ovr(chrsizx_ivf, &chrsizx_down);
    _pa_chrsizy_ovr(chrsizy_ivf, &chrsizy_down);
    _pa_baseline_ovr(baseline_ivf, &baseline_down);
    _pa_strsiz_ovr(strsiz_ivf, &strsiz_down);
    _pa_chrpos_ovr(chrpos_ivf, &chrpos_down);
    _pa_justpos_ovr(justpos_ivf, &justpos_down);
    _pa_dpmx_ovr(dpmx_ivf, &dpmx_down);
    _pa_dpmy_ovr(dpmy_ivf, &dpmy_down);
    _pa_fonts_ovr(fonts_ivf, &fonts_down);
    _pa_points_ovr(points_ivf, &points_down);
    _pa_pictsizx_ovr(pictsizx_ivf, &pictsizx_down);
    _pa_pictsizy_ovr(pictsizy_ivf, &pictsizy_down);
    _pa_scalex_ovr(scalex_ivf, &scalex_down);
    _pa_scaley_ovr(scaley_ivf, &scaley_down);
    _pa_fontnam_ovr(fontnam_ivf, &fontnam_down);
    _pa_mouse_ovr(mouse_ivf, &mouse_down);
    _pa_mousebutton_ovr(mousebutton_ivf, &mousebutton_down);
    _pa_joystick_ovr(joystick_ivf, &joystick_down);
    _pa_joybutton_ovr(joybutton_ivf, &joybutton_down);
    _pa_joyaxis_ovr(joyaxis_ivf, &joyaxis_down);
    _pa_funkey_ovr(funkey_ivf, &funkey_down);
    _pa_font_ovr(font_ivf, &font_down);
    _pa_fontsiz_ovr(fontsiz_ivf, &fontsiz_down);
    _pa_setpoints_ovr(setpoints_ivf, &setpoints_down);
    _pa_chrspcx_ovr(chrspcx_ivf, &chrspcx_down);
    _pa_chrspcy_ovr(chrspcy_ivf, &chrspcy_down);
    _pa_bold_ovr(bold_ivf, &bold_down);
    _pa_italic_ovr(italic_ivf, &italic_down);
    _pa_condensed_ovr(condensed_ivf, &condensed_down);
    _pa_extended_ovr(extended_ivf, &extended_down);
    _pa_xlight_ovr(xlight_ivf, &xlight_down);
    _pa_light_ovr(light_ivf, &light_down);
    _pa_xbold_ovr(xbold_ivf, &xbold_down);
    _pa_hollow_ovr(hollow_ivf, &hollow_down);
    _pa_raised_ovr(raised_ivf, &raised_down);
    _pa_maxx_ovr(maxx_ivf, &maxx_down);
    _pa_maxy_ovr(maxy_ivf, &maxy_down);
    _pa_maxxg_ovr(maxxg_ivf, &maxxg_down);
    _pa_maxyg_ovr(maxyg_ivf, &maxyg_down);
    _pa_select_ovr(select_ivf, &select_down);
    _pa_buffer_ovr(buffer_ivf, &buffer_down);
    _pa_sizbufg_ovr(sizbufg_ivf, &sizbufg_down);
    _pa_sizbuf_ovr(sizbuf_ivf, &sizbuf_down);
    _pa_openwin_ovr(openwin_ivf, &openwin_down);
    _pa_blockcopyg_ovr(blockcopyg_ivf, &blockcopyg_down);
    _pa_title_ovr(title_ivf, &title_down);
    _pa_setsizg_ovr(setsizg_ivf, &setsizg_down);
    _pa_setsiz_ovr(setsiz_ivf, &setsiz_down);
    _pa_getsizg_ovr(getsizg_ivf, &getsizg_down);
    _pa_getsiz_ovr(getsiz_ivf, &getsiz_down);
    _pa_setposg_ovr(setposg_ivf, &setposg_down);
    _pa_setpos_ovr(setpos_ivf, &setpos_down);
    _pa_scnsizg_ovr(scnsizg_ivf, &scnsizg_down);
    _pa_scnsiz_ovr(scnsiz_ivf, &scnsiz_down);
    _pa_scnceng_ovr(scnceng_ivf, &scnceng_down);
    _pa_scncen_ovr(scncen_ivf, &scncen_down);
    _pa_winclientg_ovr(winclientg_ivf, &winclientg_down);
    _pa_winclient_ovr(winclient_ivf, &winclient_down);
    _pa_front_ovr(front_ivf, &front_down);
    _pa_back_ovr(back_ivf, &back_down);
    _pa_frame_ovr(frame_ivf, &frame_down);
    _pa_sizable_ovr(sizable_ivf, &sizable_down);
    _pa_sysbar_ovr(sysbar_ivf, &sysbar_down);
    _pa_focus_ovr(focus_ivf, &focus_down);
    _pa_getwinid_ovr(getwinid_ivf, &getwinid_down);
    _pa_menu_ovr(menu_ivf, &menu_down);
    _pa_menuena_ovr(menuena_ivf, &menuena_down);
    _pa_menusel_ovr(menusel_ivf, &menusel_down);
    _pa_stdmenu_ovr(stdmenu_ivf, &stdmenu_down);

    /* The composed display screen is this manager's, not any client's:
       its cursor is never a caret (each window's caret rides its own
       backing), and with composition selecting it as the update screen
       a visible cursor here paints a reversed block at its home. Off,
       once; the state is the screen's and keeps. */
    (*select_down)(stdout, DSPSCN, DSPSCN);
    (*curvis_down)(stdout, FALSE);

    /* the root surface */
    dimxg = (*maxxg_down)(stdout);
    dimyg = (*maxyg_down)(stdout);
    rootcell = (*chrsizy_down)(stdout);

    /* bind standard input and output as the root window */
    ifn = fileno(stdin);
    ofn = fileno(stdout);
    getfil(&opnfil[ofn]);
    getfil(&opnfil[ifn]);
    opnfil[ofn]->inl = ifn;
    opnfil[ifn]->inw = TRUE;
    opnfil[ifn]->sfp = stdin;
    opnfil[ofn]->sfp = stdout;
    xltwin[1+MAXFIL] = ofn;
    filwin[ofn] = 1;
    opnfil[ofn]->win = getwin();
    opnwin(ofn, -1, 1, TRUE);
    focwin = opnfil[ofn]->win;
    focwin->focus = TRUE;

    mgractive = TRUE; /* the write path redirects from here */

}

/*******************************************************************************

Menus, part two: pulldowns and the API

*******************************************************************************/

/* announce a client size change: the resize, then the redraw notice */
static void annresize(winptr win)

{

    ami_evtrec er;

    er.etype = ami_etresize;
    er.winid = win->wid;
    er.rszxg = win->cmaxx;
    er.rszyg = win->cmaxy;
    entercli(win); /* the metrics in the window's font */
    er.rszx = win->cmaxx/(*chrsizx_down)(stdout);
    er.rszy = win->cmaxy/(*chrsizy_down)(stdout);
    enquepaevt(&er);
    er.etype = ami_etredraw;
    er.winid = win->wid;
    enquepaevt(&er);

}

/* a private copy of a menu definition, per the interface */
static ami_menuptr cpymenu(ami_menuptr m)

{

    ami_menuptr r = NULL, l = NULL, e;

    while (m) {

        e = malloc(sizeof(ami_menurec));
        if (!e) error("Out of memory");
        e->next = NULL;
        e->branch = cpymenu(m->branch);
        e->onoff = m->onoff;
        e->oneof = m->oneof;
        e->bar = m->bar;
        e->id = m->id;
        e->face = malloc(strlen(m->face)+1);
        if (!e->face) error("Out of memory");
        strcpy(e->face, m->face);
        if (l) l->next = e; else r = e;
        l = e;
        m = m->next;

    }

    return (r);

}

/* free a menu copy */
static void frmenu(ami_menuptr m)

{

    while (m) {

        ami_menuptr n = m->next;

        frmenu(m->branch);
        free(m->face);
        free(m);
        m = n;

    }

}

/* the sibling list holding the item with the given id */
static ami_menuptr fndmenlist(ami_menuptr root, ami_long id)

{

    ami_menuptr p, r;

    for (p = root; p; p = p->next) if (p->id == id) return (root);
    for (p = root; p; p = p->next)
        if (p->branch) {

            r = fndmenlist(p->branch, id);
            if (r) return (r);

        }

    return (NULL);

}

/* an entry's face with its marks: the check, and the cascade arrow */
static void popface(ami_menuptr p, char* buf, int len)

{

    snprintf(buf, len, "%s%s%s", p->onoff? "* ": "  ", p->face,
             p->branch? "  >": "");

}

/* draw a popup's entries into its client */
static void drwpop(winptr pw)

{

    ami_menuptr p;
    ami_long eh = BARH(rootcell);
    ami_long y = 1;
    ami_long i = 1;
    char buf[256];
    winptr own = pw->mowner;

    pw->autof[pw->curupd-1] = FALSE; /* graphical positioning */
    ctxwin = NULL;
    entercli(pw);
    (*binvis_down)(stdout); /* the entries draw foreground only */
    for (p = pw->pitems; p; p = p->next, i++) {

        int ena = menenb(own, p->id);
        int hil = (i == pw->psel || i == pw->phov) && ena;

        if (hil) fcolor8(MNUHIL); else fcolor8(MNUFLD);
        (*frect_down)(stdout, 1, y, pw->cmaxx, y+eh-1);
        if (!ena) fcolor8(MNUGREY);
        else if (hil) fcolor8(MNUHITX);
        else fcolor8(FRMTEXT);
        popface(p, buf, sizeof(buf));
        (*cursorg_down)(stdout, MENSEP/2+1, y+(eh-rootcell)/2);
        (*wrtstrn_down)(stdout, buf, strlen(buf));
        if (p->bar) { /* the group separator under the entry */

            fcolor8(FRMEDGE);
            (*line_down)(stdout, 1, y+eh-1, pw->cmaxx, y+eh-1);

        }
        y += eh;

    }
    fcolor8(FRMTEXT);
    compdmg(pw);

}

/* Open a pulldown at a root position: a parentless bordered window on
   the cascade stack, sized to its entries. */
static winptr mkpop(winptr own, ami_menuptr items, ami_long rx, ami_long ry)

{

    FILE*       pf = NULL;
    winptr      pw;
    ami_menuptr p;
    rectangle   old;
    char        buf[256];
    ami_long    n = 0, w = 40, l;
    ami_long    eh = BARH(rootcell);
    ami_long    pwid, ph;

    if (popcnt >= MAXPOP) clspops(MAXPOP-1); /* bound the cascade */
    iopenwin(&stdin, &pf, NULL, getwinid_ivf());
    pw = txt2win(pf);
    pw->popup = TRUE;
    pw->pitems = items;
    pw->mowner = own;
    pw->psel = 0;
    pw->phov = 0;
    /* a plain bordered list: no bar, no sizing; entries in the bar's
       font */
    pw->sysbar = FALSE;
    pw->size = FALSE;
    pw->font = AMI_FONT_SIGN;
    pw->fontsiz = rootcell;
    ctxwin = NULL;
    entercli(pw);
    for (p = items; p; p = p->next) {

        popface(p, buf, sizeof(buf));
        l = (*strsiz_down)(stdout, buf);
        if (l > w) w = l;
        n++;

    }
    pwid = w+MENSEP+2*FRMBORDER;
    ph = n*eh+2*FRMBORDER;
    if (rx+pwid-1 > dimxg) rx = dimxg-pwid+1;
    if (ry+ph-1 > dimyg) ry = dimyg-ph+1;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    winrect(pw, &old);
    pw->orgx = rx;
    pw->orgy = ry;
    pw->pmaxx = pwid;
    pw->pmaxy = ph;
    pw->cmaxx = pwid-2*FRMBORDER;
    pw->cmaxy = ph-2*FRMBORDER;
    regeom(pw, &old);
    popfil[popcnt] = pf;
    popstk[popcnt++] = pw;
    drwpop(pw); /* first content presents it */

    return (pw);

}

/* close pulldowns down to the given depth */
static void clspops(int downto)

{

    winptr pw;

    while (popcnt > downto) {

        pw = popstk[--popcnt];
        popstk[popcnt] = NULL;
        pw->psel = 0;
        fclose(popfil[popcnt]);
        popfil[popcnt] = NULL;

    }

}

/* close the whole menu: pulldowns and the open bar title */
static void menudismiss(void)

{

    winptr mw;

    clspops(0);
    if (menuwin) {

        mw = menuwin;
        menuwin = NULL;
        menutitle = 0;
        drwfrm(mw);
        {

            rectangle r;

            winrect(mw, &r);
            caroff();
            composewin(mw, &r);
            caron();

        }

    }

}

/* redraw a window's bar and any open pulldowns, after a state change */
static void menredraw(winptr win)

{

    rectangle r;
    int       i;

    if (!win->amenu) return;
    drwfrm(win);
    winrect(win, &r);
    caroff();
    composewin(win, &r);
    caron();
    for (i = 0; i < popcnt; i++)
        if (popstk[i]->mowner == win) drwpop(popstk[i]);

}

/* a click in a pulldown: cascade a branch, or select an item */
static void popclick(winptr pw, ami_long lx, ami_long ly)

{

    ami_long    cy = ly-pw->coffy;
    ami_long    row;
    ami_menuptr item;
    winptr      own = pw->mowner;
    ami_evtrec  er;
    int         depth;

    if (cy < 1) return;
    row = (cy-1)/BARH(rootcell)+1;
    item = mennth(pw->pitems, row);
    if (!item || !menenb(own, item->id)) return;
    for (depth = 0; depth < popcnt; depth++)
        if (popstk[depth] == pw) break;
    if (item->branch) { /* cascade the submenu beside the row */

        clspops(depth+1);
        pw->psel = row;
        pw->phov = 0;
        drwpop(pw);
        mkpop(own, item->branch, pw->orgx+pw->pmaxx-4,
              pw->orgy+pw->coffy+(row-1)*BARH(rootcell));

    } else { /* an item: select and close the menu */

        er.etype = ami_etmenus;
        er.winid = own->wid;
        er.menuid = item->id;
        menudismiss();
        enquepaevt(&er);

    }

}

/* pointer motion over a popup: the hover row follows */
static void pophover(winptr pw, ami_long ly)

{

    ami_long cy = ly-pw->coffy;
    ami_long row = 0;

    if (cy >= 1) row = (cy-1)/BARH(rootcell)+1;
    if (row < 1 || row > menlen(pw->pitems)) row = 0;
    if (row != pw->phov) {

        pw->phov = row;
        drwpop(pw);

    }

}

/* a press on the bar: open a pulldown, select a bare item, or toggle
   the open one closed */
static void barpress(winptr win, int n)

{

    ami_menuptr item;
    ami_evtrec  er;
    rectangle   r;
    int         wasopen;

    wasopen = (menuwin == win && menutitle == n);
    menudismiss();
    if (n < 1 || wasopen) return;
    item = mennth(win->amenu, n);
    if (!item || !menenb(win, item->id)) return;
    if (item->branch) { /* open the pulldown under the title */

        menuwin = win;
        menutitle = n;
        drwfrm(win);
        winrect(win, &r);
        caroff();
        composewin(win, &r);
        caron();
        mkpop(win, item->branch,
              win->orgx+(n <= win->mtn? win->mtx1[n-1]: 1)-1,
              win->orgy+win->coffy);

    } else { /* a bare top level item selects */

        er.etype = ami_etmenus;
        er.winid = win->wid;
        er.menuid = item->id;
        enquepaevt(&er);

    }

}

/* set (or remove) a window's menu; the definition is copied */
static void menu_ivf(FILE* f, ami_menuptr m)

{

    winptr    win = txt2win(f);
    rectangle old;
    ami_long  delta;

    /* any open menu of this window closes */
    { int i; for (i = popcnt-1; i >= 0; i--)
        if (popstk[i]->mowner == win) clspops(i); }
    if (menuwin == win) { menuwin = NULL; menutitle = 0; }
    delta = (m? BARH(rootcell): 0)-(win->amenu? BARH(rootcell): 0);
    if (win->amenu) { frmenu(win->amenu); win->amenu = NULL; }
    win->mtn = 0;
    if (m) win->amenu = cpymenu(m);
    /* the client cedes the bar's row, or takes it back: the window
       keeps its size */
    winrect(win, &old);
    win->cmaxy -= delta;
    if (win->cmaxy < 1) win->cmaxy = 1;
    regeom(win, &old);
    annresize(win);

}

/* enable or disable a menu item */
static void menuena_ivf(FILE* f, ami_long id, ami_long onoff)

{

    winptr    win = txt2win(f);
    menenaptr me;

    for (me = win->mena; me; me = me->next) if (me->id == id) break;
    if (!me) {

        me = malloc(sizeof(menena));
        if (!me) error("Out of memory");
        me->id = id;
        me->next = win->mena;
        win->mena = me;

    }
    me->ena = !!onoff;
    menredraw(win);

}

/* set an item's select state; a "one of" group clears its others */
static void menusel_ivf(FILE* f, ami_long id, ami_long select)

{

    winptr      win = txt2win(f);
    ami_menuptr lst = fndmenlist(win->amenu, id);
    ami_menuptr p, gs, ge, q;

    if (!lst) error("No menu item by given id");
    /* The item's group is the contiguous run of items carrying the
       oneof flag, closed by the item that ends the run -- the last
       member is unflagged, by the interface convention. */
    gs = lst;
    p = lst;
    while (p->id != id) {

        if (!p->oneof) gs = p->next; /* a run closed; the next begins after */
        p = p->next;

    }
    if (p->oneof || gs != p) { /* in a group: clear the other members */

        ge = p;
        while (ge->oneof && ge->next) ge = ge->next;
        for (q = gs; ; q = q->next) {

            if (q != p) q->onoff = FALSE;
            if (q == ge) break;

        }

    }
    p->onoff = !!select;
    menredraw(win);

}

/* build the standard menu list around the program's own */
static void stdmenu_ivf(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)

{

    static struct { ami_long sel; char* face; } std[] = {

        { AMI_SMNEW, "New" }, { AMI_SMOPEN, "Open" },
        { AMI_SMCLOSE, "Close" }, { AMI_SMSAVE, "Save" },
        { AMI_SMSAVEAS, "Save As" }, { AMI_SMPAGESET, "Page Setup" },
        { AMI_SMPRINT, "Print" }, { AMI_SMEXIT, "Exit" },
        { AMI_SMUNDO, "Undo" }, { AMI_SMCUT, "Cut" },
        { AMI_SMPASTE, "Paste" }, { AMI_SMDELETE, "Delete" },
        { AMI_SMFIND, "Find" }, { AMI_SMFINDNEXT, "Find Next" },
        { AMI_SMREPLACE, "Replace" }, { AMI_SMGOTO, "Goto" },
        { AMI_SMSELECTALL, "Select All" }, { AMI_SMNEWWINDOW, "New Window" },
        { AMI_SMTILEHORIZ, "Tile Horizontally" },
        { AMI_SMTILEVERT, "Tile Vertically" }, { AMI_SMCASCADE, "Cascade" },
        { AMI_SMCLOSEALL, "Close All" }, { AMI_SMHELPTOPIC, "Help Topics" },
        { AMI_SMABOUT, "About" },

    };
    static const ami_long filist[] = { AMI_SMNEW, AMI_SMOPEN, AMI_SMCLOSE,
        AMI_SMSAVE, AMI_SMSAVEAS, AMI_SMPAGESET, AMI_SMPRINT, AMI_SMEXIT, 0 };
    static const ami_long edlist[] = { AMI_SMUNDO, AMI_SMCUT, AMI_SMPASTE,
        AMI_SMDELETE, AMI_SMFIND, AMI_SMFINDNEXT, AMI_SMREPLACE, AMI_SMGOTO,
        AMI_SMSELECTALL, 0 };
    static const ami_long wilist[] = { AMI_SMNEWWINDOW, AMI_SMTILEHORIZ,
        AMI_SMTILEVERT, AMI_SMCASCADE, AMI_SMCLOSEALL, 0 };
    static const ami_long helist[] = { AMI_SMHELPTOPIC, AMI_SMABOUT, 0 };
    static struct { const ami_long* lst; char* face; } tops[] = {

        { filist, "File" }, { edlist, "Edit" }, { NULL, NULL },
        { wilist, "Window" }, { helist, "Help" },

    };
    ami_menuptr root = NULL, rtl = NULL;
    ami_long ti, i;

    /* the documented order: file edit <program> window help */
    for (ti = 0; ti < 5; ti++) {

        ami_menuptr sub = NULL, subl = NULL, top;

        if (!tops[ti].lst) { /* the program's own menu goes here */

            if (pm) {

                if (rtl) rtl->next = pm; else root = pm;
                rtl = pm;
                while (rtl->next) rtl = rtl->next;

            }
            continue;

        }
        for (i = 0; tops[ti].lst[i]; i++) {

            ami_long sel = tops[ti].lst[i];
            ami_long si;

            if (!(sms & (1L<<sel))) continue;
            for (si = 0; si < AMI_SMMAX; si++)
                if (std[si].sel == sel) break;
            if (si >= AMI_SMMAX) continue;
            {

                ami_menuptr e = malloc(sizeof(ami_menurec));

                if (!e) error("Out of memory");
                e->next = NULL;
                e->branch = NULL;
                e->onoff = FALSE;
                e->oneof = FALSE;
                e->bar = FALSE;
                e->id = std[si].sel;
                e->face = std[si].face;
                if (subl) subl->next = e; else sub = e;
                subl = e;

            }

        }
        if (sub) { /* the list has entries: its top level title */

            top = malloc(sizeof(ami_menurec));
            if (!top) error("Out of memory");
            top->next = NULL;
            top->branch = sub;
            top->onoff = FALSE;
            top->oneof = FALSE;
            top->bar = FALSE;
            top->id = 0;
            top->face = tops[ti].face;
            if (rtl) rtl->next = top; else root = top;
            rtl = top;

        }

    }
    *sm = root;

}

/*******************************************************************************

Hold composition

A backdoor for the tests, in the layer backdoor style: a run fast
forwarding to a selected frame holds composition through the frames it
passes, and the release recomposes the display whole. Weak at the
callers, so the desktop backends simply have no hold to offer.

*******************************************************************************/

void wg_hold(ami_long on)

{

    rectangle scr;

    if (!!on == wgheld) return;
    wgheld = !!on;
    if (!wgheld) {

        calcvisall();
        setrect(&scr, 1, 1, dimxg, dimyg);
        composeall(&scr);

    }

}

/*******************************************************************************

Drag window

Begins an interactive move of the window, riding the button press the
caller just fielded: the manager's rubber band takes the pointer from
here, and the window lands when the button releases. This is what a
dialog's title bar does with its press.

*******************************************************************************/

void ami_dragwin(FILE* f)

{

    winptr win;

    win = txt2win(f);
    if (drag != dt_none || win->maxed) return;
    drag = dt_move;
    drgwin = win;
    drgax = rootmx; drgay = rootmy;
    drgox = win->orgx; drgoy = win->orgy;
    drgow = win->pmaxx; drgoh = win->pmaxy;
    bandcalc(rootmx, rootmy);
    banddraw();

}

/*******************************************************************************

Deinitialize the manager

Runs before the layers below deinitialize (destructors descend), taking
the manager's hooks back off the top of every chain so each layer's own
teardown finds its vectors where it left them.

*******************************************************************************/

static void deinit_windowg(void) __attribute__((destructor (104)));
static void deinit_windowg(void)

{

    pread_t  cpread;
    pwrite_t cpwrite;
    popen_t  cpopen;
    pclose_t cpclose;
    plseek_t cplseek;

    /* A failed layer below exits before this manager's constructor
       ran: there is nothing hooked to take back off. */
    if (!mgractive) return;
    mgractive = FALSE;
    /* leave the layer showing and updating its display screen */
    (*select_down)(stdout, DSPSCN, DSPSCN);
    /* the API vectors come back off */
    { ami_event_t t; _pa_event_ovr(event_down, &t); }
    { ami_eventover_t t; _pa_eventover_ovr(eventover_down, &t); }
    { ami_eventsover_t t; _pa_eventsover_ovr(eventsover_down, &t); }
    { ami_sendevent_t t; _pa_sendevent_ovr(sendevent_down, &t); }
    { ami_timer_t t; _pa_timer_ovr(timer_down, &t); }
    { ami_killtimer_t t; _pa_killtimer_ovr(killtimer_down, &t); }
    { ami_frametimer_t t; _pa_frametimer_ovr(frametimer_down, &t); }
    { ami_autohold_t t; _pa_autohold_ovr(autohold_down, &t); }
    { ami_line_t t; _pa_line_ovr(line_down, &t); }
    { ami_rect_t t; _pa_rect_ovr(rect_down, &t); }
    { ami_frect_t t; _pa_frect_ovr(frect_down, &t); }
    { ami_rrect_t t; _pa_rrect_ovr(rrect_down, &t); }
    { ami_frrect_t t; _pa_frrect_ovr(frrect_down, &t); }
    { ami_ellipse_t t; _pa_ellipse_ovr(ellipse_down, &t); }
    { ami_fellipse_t t; _pa_fellipse_ovr(fellipse_down, &t); }
    { ami_arc_t t; _pa_arc_ovr(arc_down, &t); }
    { ami_farc_t t; _pa_farc_ovr(farc_down, &t); }
    { ami_fchord_t t; _pa_fchord_ovr(fchord_down, &t); }
    { ami_ftriangle_t t; _pa_ftriangle_ovr(ftriangle_down, &t); }
    { ami_setpixel_t t; _pa_setpixel_ovr(setpixel_down, &t); }
    { ami_scroll_t t; _pa_scroll_ovr(scroll_down, &t); }
    { ami_scrollg_t t; _pa_scrollg_ovr(scrollg_down, &t); }
    { ami_picture_t t; _pa_picture_ovr(picture_down, &t); }
    { ami_writejust_t t; _pa_writejust_ovr(writejust_down, &t); }
    { ami_del_t t; _pa_del_ovr(del_down, &t); }
    { ami_home_t t; _pa_home_ovr(home_down, &t); }
    { ami_up_t t; _pa_up_ovr(up_down, &t); }
    { ami_down_t t; _pa_down_ovr(down_down, &t); }
    { ami_left_t t; _pa_left_ovr(left_down, &t); }
    { ami_right_t t; _pa_right_ovr(right_down, &t); }
    { ami_wrtstr_t t; _pa_wrtstr_ovr(wrtstr_down, &t); }
    { ami_wrtstrn_t t; _pa_wrtstrn_ovr(wrtstrn_down, &t); }
    { ami_cursor_t t; _pa_cursor_ovr(cursor_down, &t); }
    { ami_cursorg_t t; _pa_cursorg_ovr(cursorg_down, &t); }
    { ami_auto_t t; _pa_auto_ovr(auto_down, &t); }
    { ami_curvis_t t; _pa_curvis_ovr(curvis_down, &t); }
    { ami_fcolor_t t; _pa_fcolor_ovr(fcolor_down, &t); }
    { ami_bcolor_t t; _pa_bcolor_ovr(bcolor_down, &t); }
    { ami_fcolorc_t t; _pa_fcolorc_ovr(fcolorc_down, &t); }
    { ami_bcolorc_t t; _pa_bcolorc_ovr(bcolorc_down, &t); }
    { ami_fcolorg_t t; _pa_fcolorg_ovr(fcolorg_down, &t); }
    { ami_bcolorg_t t; _pa_bcolorg_ovr(bcolorg_down, &t); }
    { ami_fover_t t; _pa_fover_ovr(fover_down, &t); }
    { ami_bover_t t; _pa_bover_ovr(bover_down, &t); }
    { ami_finvis_t t; _pa_finvis_ovr(finvis_down, &t); }
    { ami_binvis_t t; _pa_binvis_ovr(binvis_down, &t); }
    { ami_fxor_t t; _pa_fxor_ovr(fxor_down, &t); }
    { ami_bxor_t t; _pa_bxor_ovr(bxor_down, &t); }
    { ami_fand_t t; _pa_fand_ovr(fand_down, &t); }
    { ami_band_t t; _pa_band_ovr(band_down, &t); }
    { ami_for_t t; _pa_for_ovr(for_down, &t); }
    { ami_bor_t t; _pa_bor_ovr(bor_down, &t); }
    { ami_linewidth_t t; _pa_linewidth_ovr(linewidth_down, &t); }
    { ami_linestyle_t t; _pa_linestyle_ovr(linestyle_down, &t); }
    { ami_settab_t t; _pa_settab_ovr(settab_down, &t); }
    { ami_settabg_t t; _pa_settabg_ovr(settabg_down, &t); }
    { ami_restab_t t; _pa_restab_ovr(restab_down, &t); }
    { ami_restabg_t t; _pa_restabg_ovr(restabg_down, &t); }
    { ami_clrtab_t t; _pa_clrtab_ovr(clrtab_down, &t); }
    { ami_viewoffg_t t; _pa_viewoffg_ovr(viewoffg_down, &t); }
    { ami_viewscale_t t; _pa_viewscale_ovr(viewscale_down, &t); }
    { ami_path_t t; _pa_path_ovr(path_down, &t); }
    { ami_blink_t t; _pa_blink_ovr(blink_down, &t); }
    { ami_reverse_t t; _pa_reverse_ovr(reverse_down, &t); }
    { ami_underline_t t; _pa_underline_ovr(underline_down, &t); }
    { ami_superscript_t t; _pa_superscript_ovr(superscript_down, &t); }
    { ami_subscript_t t; _pa_subscript_ovr(subscript_down, &t); }
    { ami_strikeout_t t; _pa_strikeout_ovr(strikeout_down, &t); }
    { ami_standout_t t; _pa_standout_ovr(standout_down, &t); }
    { ami_loadpict_t t; _pa_loadpict_ovr(loadpict_down, &t); }
    { ami_delpict_t t; _pa_delpict_ovr(delpict_down, &t); }
    { ami_curx_t t; _pa_curx_ovr(curx_down, &t); }
    { ami_cury_t t; _pa_cury_ovr(cury_down, &t); }
    { ami_curxg_t t; _pa_curxg_ovr(curxg_down, &t); }
    { ami_curyg_t t; _pa_curyg_ovr(curyg_down, &t); }
    { ami_curbnd_t t; _pa_curbnd_ovr(curbnd_down, &t); }
    { ami_chrsizx_t t; _pa_chrsizx_ovr(chrsizx_down, &t); }
    { ami_chrsizy_t t; _pa_chrsizy_ovr(chrsizy_down, &t); }
    { ami_baseline_t t; _pa_baseline_ovr(baseline_down, &t); }
    { ami_strsiz_t t; _pa_strsiz_ovr(strsiz_down, &t); }
    { ami_chrpos_t t; _pa_chrpos_ovr(chrpos_down, &t); }
    { ami_justpos_t t; _pa_justpos_ovr(justpos_down, &t); }
    { ami_dpmx_t t; _pa_dpmx_ovr(dpmx_down, &t); }
    { ami_dpmy_t t; _pa_dpmy_ovr(dpmy_down, &t); }
    { ami_fonts_t t; _pa_fonts_ovr(fonts_down, &t); }
    { ami_points_t t; _pa_points_ovr(points_down, &t); }
    { ami_pictsizx_t t; _pa_pictsizx_ovr(pictsizx_down, &t); }
    { ami_pictsizy_t t; _pa_pictsizy_ovr(pictsizy_down, &t); }
    { ami_scalex_t t; _pa_scalex_ovr(scalex_down, &t); }
    { ami_scaley_t t; _pa_scaley_ovr(scaley_down, &t); }
    { ami_fontnam_t t; _pa_fontnam_ovr(fontnam_down, &t); }
    { ami_mouse_t t; _pa_mouse_ovr(mouse_down, &t); }
    { ami_mousebutton_t t; _pa_mousebutton_ovr(mousebutton_down, &t); }
    { ami_joystick_t t; _pa_joystick_ovr(joystick_down, &t); }
    { ami_joybutton_t t; _pa_joybutton_ovr(joybutton_down, &t); }
    { ami_joyaxis_t t; _pa_joyaxis_ovr(joyaxis_down, &t); }
    { ami_funkey_t t; _pa_funkey_ovr(funkey_down, &t); }
    { ami_font_t t; _pa_font_ovr(font_down, &t); }
    { ami_fontsiz_t t; _pa_fontsiz_ovr(fontsiz_down, &t); }
    { ami_setpoints_t t; _pa_setpoints_ovr(setpoints_down, &t); }
    { ami_chrspcx_t t; _pa_chrspcx_ovr(chrspcx_down, &t); }
    { ami_chrspcy_t t; _pa_chrspcy_ovr(chrspcy_down, &t); }
    { ami_bold_t t; _pa_bold_ovr(bold_down, &t); }
    { ami_italic_t t; _pa_italic_ovr(italic_down, &t); }
    { ami_condensed_t t; _pa_condensed_ovr(condensed_down, &t); }
    { ami_extended_t t; _pa_extended_ovr(extended_down, &t); }
    { ami_xlight_t t; _pa_xlight_ovr(xlight_down, &t); }
    { ami_light_t t; _pa_light_ovr(light_down, &t); }
    { ami_xbold_t t; _pa_xbold_ovr(xbold_down, &t); }
    { ami_hollow_t t; _pa_hollow_ovr(hollow_down, &t); }
    { ami_raised_t t; _pa_raised_ovr(raised_down, &t); }
    { ami_maxx_t t; _pa_maxx_ovr(maxx_down, &t); }
    { ami_maxy_t t; _pa_maxy_ovr(maxy_down, &t); }
    { ami_maxxg_t t; _pa_maxxg_ovr(maxxg_down, &t); }
    { ami_maxyg_t t; _pa_maxyg_ovr(maxyg_down, &t); }
    { ami_select_t t; _pa_select_ovr(select_down, &t); }
    { ami_buffer_t t; _pa_buffer_ovr(buffer_down, &t); }
    { ami_sizbufg_t t; _pa_sizbufg_ovr(sizbufg_down, &t); }
    { ami_sizbuf_t t; _pa_sizbuf_ovr(sizbuf_down, &t); }
    { ami_openwin_t t; _pa_openwin_ovr(openwin_down, &t); }
    { ami_blockcopyg_t t; _pa_blockcopyg_ovr(blockcopyg_down, &t); }
    { ami_getwinid_t t; _pa_getwinid_ovr(getwinid_down, &t); }
    { ami_menu_t t; _pa_menu_ovr(menu_down, &t); }
    { ami_menuena_t t; _pa_menuena_ovr(menuena_down, &t); }
    { ami_menusel_t t; _pa_menusel_ovr(menusel_down, &t); }
    { ami_stdmenu_t t; _pa_stdmenu_ovr(stdmenu_down, &t); }
    { ami_title_t t; _pa_title_ovr(title_down, &t); }
    { ami_setsizg_t t; _pa_setsizg_ovr(setsizg_down, &t); }
    { ami_setsiz_t t; _pa_setsiz_ovr(setsiz_down, &t); }
    { ami_getsizg_t t; _pa_getsizg_ovr(getsizg_down, &t); }
    { ami_getsiz_t t; _pa_getsiz_ovr(getsiz_down, &t); }
    { ami_setposg_t t; _pa_setposg_ovr(setposg_down, &t); }
    { ami_setpos_t t; _pa_setpos_ovr(setpos_down, &t); }
    { ami_scnsizg_t t; _pa_scnsizg_ovr(scnsizg_down, &t); }
    { ami_scnsiz_t t; _pa_scnsiz_ovr(scnsiz_down, &t); }
    { ami_scnceng_t t; _pa_scnceng_ovr(scnceng_down, &t); }
    { ami_scncen_t t; _pa_scncen_ovr(scncen_down, &t); }
    { ami_winclientg_t t; _pa_winclientg_ovr(winclientg_down, &t); }
    { ami_winclient_t t; _pa_winclient_ovr(winclient_down, &t); }
    { ami_front_t t; _pa_front_ovr(front_down, &t); }
    { ami_back_t t; _pa_back_ovr(back_down, &t); }
    { ami_frame_t t; _pa_frame_ovr(frame_down, &t); }
    { ami_sizable_t t; _pa_sizable_ovr(sizable_down, &t); }
    { ami_sysbar_t t; _pa_sysbar_ovr(sysbar_down, &t); }
    { ami_focus_t t; _pa_focus_ovr(focus_down, &t); }
    /* and the system vectors; if we do not come off the top of the
       chain the stacking is corrupt */
    ovr_read(ofpread, &cpread);
    ovr_write(ofpwrite, &cpwrite);
    ovr_open(ofpopen, &cpopen);
    ovr_close(ofpclose, &cpclose);
    ovr_lseek(ofplseek, &cplseek);
#ifdef NOCANCEL
    {
        pread_t  cr; pwrite_t cw; popen_t co; pclose_t cc;

        ovr_read_nocancel(ofpread_nocancel, &cr);
        ovr_write_nocancel(ofpwrite_nocancel, &cw);
        ovr_open_nocancel(ofpopen_nocancel, &co);
        ovr_close_nocancel(ofpclose_nocancel, &cc);
    }
#endif
    if (cpwrite != iwrite || cpread != iread)
        error("System consistency check");

}
