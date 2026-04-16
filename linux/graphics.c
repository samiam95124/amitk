/** ****************************************************************************
 *                                                                              *
 *                        GRAPHICAL MODE LIBRARY FOR X                          *
 *                                                                              *
 *                       Copyright (C) 2019 Scott A. Franco                     *
 *                                                                              *
 *                            2019/05/17 S. A. Franco                           *
 *                                                                              *
 * Implements the graphical mode functions on X. Gralib is upward               *
 * compatible with trmlib functions.                                            *
 *                                                                              *
 * Proposed improvements:                                                       *
 *                                                                              *
 * Move(f, d, dx, dy, s, sx1, sy1, sx2, sy2)                                    *
 *                                                                              *
 * Moves a block of pixels from one buffer to another, or to a different place  *
 * in the same buffer. Used to implement various features like intrabuffer      *
 * moves, off screen image chaching, special clipping, etc.                     *
 *                                                                              *
 * History:                                                                     *
 *                                                                              *
 * Gralib started in 1996 as a graphical window demonstrator as a twin to       *
 * ansilib, the ANSI control character based terminal mode library.             *
 * In 2003, gralib was upgraded to the graphical terminal standard.             *
 * In 2005, gralib was upgraded to include the window mangement calls, and the  *
 * widget calls.                                                                *
 *                                                                              *
 * The XWindow version started at various times around 2018, the first try was  *
 * an attempt at use of GTK.This encountered technical problems that seemed to  *
 * be a dead end, but later a solution was found. Irregardless, the rule from   *
 * the effort was "the shallower the depth of stacked APIs, the better".        *
 *                                                                              *
 * The XWindow version was created about the same time as the Windows version   *
 * was translated to C. An attempt was and is made to make the structure of the *
 * code to be as similar as possible between them. Never the less, there is no  *
 * attempt made to produce a universal code base between them. If for no other  *
 * reason, that is Petit-Ami's job description.                                 *
 *                                                                              *
 *                          BSD LICENSE INFORMATION                             *
 *                                                                              *
 * Copyright (C) 2019 - Scott A. Franco                                         *
 *                                                                              *
 * All rights reserved.                                                         *
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
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
 * SUCH DAMAGE.                                                                 *
 *                                                                              *
 *******************************************************************************/

/* X11 definitions */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* linux definitions */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X */
//#include <sys/timerfd.h>
#endif
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X */
#include <linux/joystick.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

/* local definitions */
#include <localdefs.h>
#include <config.h>
#include <graphics.h>
#include "system_event.h" /* system event (mouse, joystick) handler */
/* FreeType/fontconfig for client-side font rendering */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>

/* external definitions */
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X */
extern char *program_invocation_short_name;
#endif

/* forward declarations for FreeType helper functions */
static void ft_draw_char(Drawable d, GC gc, FT_Face face,
                         int pixel_size_x, int pixel_size_y,
                         int x, int y, char c);
static void ft_draw_string(Drawable d, GC gc, FT_Face face,
                           int pixel_size_x, int pixel_size_y,
                           int x, int y, char* s, int len);
static int  ft_text_width(FT_Face face, const char* s, int len);
static void ft_draw_char_rotated(Drawable d, GC gc, FT_Face face,
                                 int pixel_size_x, int pixel_size_y,
                                 float angle_rad,
                                 int x, int y, char c);
static void ft_cache_clear(void);
static void ft_invalidate_face(FT_Face face);

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

//#define PRTFNT /* print internal fonts list */
//#define PRTMEM /* print memory allocations at exit */
//#define PRTPWM /* print window parameters on open */
//#define PRTFRM /* print Xwindow frame parameters */
//#define NOWDELAY /* don't delay window presentation until drawn */
//#define NOFAKEFOCUS /* don't fake focus for child windows */
#define WAITWMR     /* wait on window manager replies for window configures */

#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X */
#define NOCANCEL /* include nocancel overrides */
#endif

/* the "standard character" sizes are used to form a pseudo-size for desktop
   character measurements in a graphical system. */
#define STDCHRX   8
#define STDCHRY   12
#define MAXBUF 10  /* maximum number of buffers available */
#define IOWIN  1   /* logical window number of input/output pair */
#define MAXCON 10  /* number of screen contexts */
#define MAXTAB 50  /* total number of tabs possible per screen */
#define MAXPIC 50  /* total number of loadable pictures */
#define MAXLIN 250 /* maximum length of input bufferred line */
#define MAXFIL 1000 /* maximum open files */
#define MINJST 1   /* minimum pixels for space in justification */
#define MAXFNM 250 /* number of filename characters in buffer */
#define MAXJOY 10  /* number of joysticks possible */

/* child frame dimensions (Ami-drawn frames for child windows).
   Title bar height is based on the window's font line height so it scales
   with text size; border and button sizes are proportional to title bar. */
#define CFRM_TITBAR_H(win)  ((int)((win)->linespace * 2.2))
#define CFRM_BORDER_W       4  /* resize border width in pixels */
#define CFRM_BUTTON_SZ(win) ((int)((win)->linespace * 1.1))
#define CFRM_BUTTON_GAP     6  /* gap between buttons */
#define CFRM_BUTTON_MG      8  /* button margin from edge */
#define CFRM_TITLE_SZ(win)  ((int)((win)->gfhigh * 1.15))
#define CFRM_MIN_W          200 /* width of a minimized child window */

/* Logical → physical viewport transform. Petit-Ami primitives shift their
   1-based logical coordinates by -1 before calling into X11; the transform
   is applied after that shift so operation is entirely in 0-based space.
   At scale 1.0 with zero offset the transform is a no-op. */
#define L2PX(w, v)  ((int)((v) * (w)->vsx) + (w)->goffx)
#define L2PY(w, v)  ((int)((v) * (w)->vsy) + (w)->goffy)
#define L2PW(w, n)  ((int)((n) * (w)->vsx))
#define L2PH(w, n)  ((int)((n) * (w)->vsy))
/* deltas (no offset) used by scroll helpers */
#define L2PDX(w, v) ((int)((v) * (w)->vsx))
#define L2PDY(w, v) ((int)((v) * (w)->vsy))
/* inverse: physical → logical, for mouse coordinate helpers */
#define P2LX(w, v)  ((int)(((v) - (w)->goffx) / (w)->vsx))
#define P2LY(w, v)  ((int)(((v) - (w)->goffy) / (w)->vsy))

#define MAXSID 100 /* number of possible logical system events */
/* extra space to add in x/y for initial window */
#ifdef __MACH__ /* Mac OS X */
/* in XQuartz XWindows emulation, there is a move square in the lower right hand
   corner that we have to avoid. We fill in scroll bars around it */
#define EXTSPC 15
#else
#define EXTSPC 0
#endif

/* To properly compensate for high DPI displays, we use actual height onscreen
   to determine the character height. Note the point size was choosen to most
   closely match xterm. */
#define POINT  (0.353) /* point size in mm */
#define STRIKE (1.5)   /* strikeout percentage (from top of cell to baseline */
#define EXTRAMENUY 10  /* extra space for menu bar y */
#define EXTRAMENUX 10  /* extra space for menu bar x */

/* Size and offset of missing font character in cell y fractions. We define this
   here because the character set may or may not have such a character. It is
   a Unicode white square */
#define MISCHRX 0.5  /* size x */
#define MISCHRY 0.75 /* size y */
#define MISOFFX 0.2  /* offset x */
#define MISOFFY 0.2  /* offset y */

/*
 * Configurable parameters
 *
 * These parameters can be configured here at compile time, or are overriden
 * at runtime by values of the same name in the config files.
 * The values are overriddable.
 */
#ifndef MAXXD
#define MAXXD     80 /* standard terminal, 80x25 */
#endif

#ifndef MAXYD
#define MAXYD     25
#endif

#ifndef DIALOGERR
#define DIALOGERR 1     /* send runtime errors to dialog */
#endif

#ifndef MOUSEENB
#define MOUSEENB  TRUE  /* enable mouse */
#endif

#ifndef JOYENB
#define JOYENB    TRUE  /* enable joysticks */
#endif

#ifndef DMPMSG
#define DMPMSG    FALSE /* enable dump messages (diagnostic) */
#endif

#ifndef DMPEVT
#define DMPEVT    FALSE /* enable dump Petit-Ami messages */
#endif

#ifndef PRTFTM
#define PRTFTM    FALSE /* print font metrics (diagnostic) */
#endif

#ifndef CONPNT
#define CONPNT    11    /* height of console font in points */
#endif

/* file handle numbers at the system interface level */
#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* XWindows call lock/unlock */
#define XWLOCK() pthread_mutex_lock(&xwlock)
#define XWUNLOCK() pthread_mutex_unlock(&xwlock)

/* motif window manager decoration bits, used to enable or disable windows
   decorations. These are no longer defined in XLIB, but are operative. */
#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

/* types of override calls not already declared in graphics.h.
 * This is because these are unpublished as yet.
 */
typedef void (*ami_viewoffg_t)(FILE* f, int x, int y);
typedef void (*ami_viewscale_t)(FILE* f, float x, float y);
typedef int  (*ami_scalex_t)(FILE* f, int x);
typedef int  (*ami_scaley_t)(FILE* f, int y);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

#ifdef NOCANCEL
extern void ovr_read_nocancel(pread_t nfp, pread_t* ofp);
extern void ovr_write_nocancel(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open_nocancel(popen_t nfp, popen_t* ofp);
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
#endif

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

/* XWindow font attributes. These are similar, but not identical to the screen
   text attributes. Its XWindow specific because we have to use these attibutes
   to automatically recreate the font specifications. They are mapped into PA
   standard text attributes by each set routine. */
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
    int             caps;   /* set of XWindow font capabilities */
    xcaplst*        caplst; /* list of all XWindow font capabilities */
    struct fontrec* next;   /* next font in list */

} fontrec, *fontptr;

typedef enum { mdnorm, mdinvis, mdxor, mdand, mdor } mode; /* color mix modes */

/* Menu tracking. This is a mirror image of the menu we were given by the
   user. However, we can do with less information than is in the original
   tree as passed. The menu items are a linear list, since they contain
   both the menu handle and the relative number 0-n of the item, neither
   the lack of tree structure nor the order of the list matters. */
typedef struct metrec* metptr;
typedef struct metrec {

    metptr next;               /* next entry */
    metptr branch;             /* menu branch */
    metptr frame;              /* frame for pulldown menu */
    metptr head;               /* head of menu pointer */
    int    menubar;            /* is the menu bar */
    int    frm;                /* is a frame */
    int    onoff;              /* the item is on-off highlighted */
    int    select;             /* the current on/off state of the highlight */
    metptr oneof;              /* "one of" chain pointer */
    metptr chnhd;              /* head of "one of" chain */
    int    ena;                /* enabled/disabled */
    int    bar;                /* has bar under */
    int    id;                 /* user id of item */
    int    fx1, fy1, fx2, fy2; /* subclient position of window */
    int    prime;              /* is a prime (onscreen) entry */
    int    pressed;            /* in the pressed state */
    FILE*  wf;                 /* output file for the menu window */
    char*  title;              /* title text */
    FILE*  parent;             /* parent window */
    FILE*  evtfil;             /* file to post menu events to */
    int    wid;                /* menu window id */

} metrec;

typedef struct scncon* scnptr;
typedef struct scncon { /* screen context */

    /* fields used by graph module */
    int     lwidth;      /* width of lines */
    /* note that the pixel and character dimensions and positions are kept
      in parallel for both characters and pixels */
    int     maxx;        /* maximum characters in x */
    int     maxy;        /* maximum characters in y */
    int     maxxg;       /* maximum pixels in x */
    int     maxyg;       /* maximum pixels in y */
    int     curx;        /* current cursor location x */
    int     cury;        /* current cursor location y */
    int     curxg;       /* current cursor location in pixels x */
    int     curyg;       /* current cursor location in pixels y */
    int     angle;       /* character drawing angle */
    int     fcrgb;       /* current writing foreground color in rgb */
    int     bcrgb;       /* current writing background color in rgb */
    mode    fmod;        /* foreground mix mode */
    mode    bmod;        /* background mix mode */
    fontptr cfont;       /* active font entry */
    int     cspc;        /* character spacing */
    int     lspc;        /* line spacing */
    int     attr;        /* set of active attributes */
    int     autof;       /* current status of scroll and wrap */
    int     tab[MAXTAB]; /* tabbing array */
    int     curv;        /* cursor visible */
    /* note that view offsets and scaling are experimental features */
    int     offx;        /* viewport offset x */
    int     offy;        /* viewport offset y */
    int     wextx;       /* window extent x */
    int     wexty;       /* window extent y */
    int     vextx;       /* viewpor extent x */
    int     vexty;       /* viewport extent y */

    /* fields used by graphics subsystem */
    GC      xcxt;        /* graphics context */
    Pixmap  xbuf;        /* pixmap for screen backing buffer */

} scncon;

typedef struct pict* picptr;
typedef struct pict { /* picture tracking record */

    struct pict* next; /* list of rescaled images */
    int          sx; /* size in x */
    int          sy; /* size in y */
    XImage*      xi; /* Xwindows image */

} pict;

/* XWindow style rectangle */
typedef struct {

    int         x, y; /* origin */
    int         w, h; /* width/height */

} xrect;

/* window description */
typedef struct winrec* winptr;
typedef struct winrec {

    winptr       next;              /* next entry (for free list) */
    /* fields used by graph module */
    int          parlfn;            /* logical parent */
    winptr       parwin;            /* link to parent (or NULL for parentless) */
    int          wid;               /* this window logical id */
    winptr       childwin;          /* list of child windows */
    winptr       childlst;          /* list pointer if this is a child */
    scnptr       screens[MAXCON];   /* screen contexts array */
    int          curdsp;            /* index for current display screen */
    int          curupd;            /* index for current update screen */
    /* global sets. these are the global set parameters that apply to any new
      created screen buffer */
    int          gmaxx;             /* maximum x size */
    int          gmaxy;             /* maximum y size */
    int          gmaxxg;            /* size of client area in x */
    int          gmaxyg;            /* size of client area in y */
    int          bufx;              /* buffer size x characters */
    int          bufy;              /* buffer size y characters */
    int          bufxg;             /* buffer size x pixels */
    int          bufyg;             /* buffer size y pixels */
    int          gattr;             /* current attributes */
    int          gauto;             /* state of auto */
    int          gfcrgb;            /* foreground color in rgb */
    int          gbcrgb;            /* background color in rgb */
    int          gcurv;             /* state of cursor visible */
    fontptr      gcfont;            /* current font select */
    int          gfhigh;            /* current em-square pixel size y (FreeType) */
    int          gfhighx;           /* current em-square pixel size x (asymmetric) */
    int          gfcellh;           /* target character cell height (pixels) */
    float        gfpoint;           /* current font point size */
    int          mischrx;           /* missing font character x */
    int          mischry;           /* missing font character y */
    int          misoffx;           /* missing font offset x */
    int          misoffy;           /* missing font offset y */
    mode         gfmod;             /* foreground mix mode */
    mode         gbmod;             /* background mix mode */
    int          goffx;             /* viewport offset x (physical pixels) */
    int          goffy;             /* viewport offset y (physical pixels) */
    float        vsx;                /* viewport scale x (default 1.0) */
    float        vsy;                /* viewport scale y (default 1.0) */
    int          gwextx;            /* window extent x */
    int          gwexty;            /* window extent y */
    int          gvextx;            /* viewpor extent x */
    int          gvexty;            /* viewport extent y */
    int          termfnt;           /* terminal font number */
    int          bookfnt;           /* book font number */
    int          signfnt;           /* sign font number */
    int          techfnt;           /* technical font number */
    int          mb1;               /* mouse assert status button 1 */
    int          mb2;               /* mouse assert status button 2 */
    int          mb3;               /* mouse assert status button 3 */
    int          mb4;               /* mouse assert status button 4 */
    int          mb5;               /* mouse assert status button 5 */
    int          mpx, mpy;          /* mouse current position */
    int          mpxg, mpyg;        /* mouse current position graphical */
    int          nmb1;              /* new mouse assert status button 1 */
    int          nmb2;              /* new mouse assert status button 2 */
    int          nmb3;              /* new mouse assert status button 3 */
    int          nmb4;              /* new mouse assert status button 4 */
    int          nmb5;              /* new mouse assert status button 5 */
    int          nmpx, nmpy;        /* new mouse current position */
    int          nmpxg, nmpyg;      /* new mouse current position graphical */
    int          linespace;         /* line spacing in pixels */
    int          charspace;         /* character spacing in pixels */
    int          chrspcx;           /* extra space between characters */
    int          chrspcy;           /* extra space between lines */
    int          curspace;          /* size of cursor, in pixels */
    int          baseoff;           /* font baseline offset from top */
    int          menuspcy;          /* amount of space for menu in y (if exists) */
    int          shift;             /* state of shift key */
    int          cntrl;             /* state of control key */
    int          fcurdwn;           /* cursor on screen flag */
    int          joy1cap;           /* joystick 1 is captured */
    int          joy2cap;           /* joystick 2 is captured */
    int          joy1xs;            /* last joystick position 1x */
    int          joy1ys;            /* last joystick position 1y */
    int          joy1zs;            /* last joystick position 1z */
    int          joy2xs;            /* last joystick position 2x */
    int          joy2ys;            /* last joystick position 2y */
    int          joy2zs;            /* last joystick position 2z */
    int          shsize;            /* display screen size x in millimeters */
    int          svsize;            /* display screen size y in millimeters */
    int          shres;             /* display screen pixels in x */
    int          svres;             /* display screen pixels in y */
    int          sdpmx;             /* display screen find dots per meter x */
    int          sdpmy;             /* display screen find dots per meter y */
    char         inpbuf[MAXLIN];    /* input line buffer */
    int          inpptr;            /* input line index */
    int          frmrun;            /* framing timer is running */
    int          timers[AMI_MAXTIM]; /* timer id array */
    int          frmsev;            /* frame timer system event */
    int          focus;             /* screen in focus */
    picptr       pictbl[MAXPIC];    /* loadable pictures table */
    int          bufmod;            /* buffered screen mode */
    metptr       metlst;            /* menu tracking list */
    metptr       menu;              /* "faux menu" bar */
    int          frame;             /* frame on/off */
    int          size;              /* size bars on/off */
    int          sysbar;            /* system bar on/off */
    int          sizests;           /* last resize status save */
    int          visible;           /* window is visible */
    /* window state, 0 = normal, 1 = maximized, 2 = minimized */
    int          winstate;
    int          lwinstate;         /* last window state */

    /* fields used by graphics subsystem */
    Window       xmwhan;            /* master window */
    Window       xwhan;             /* subclient window */
    xrect        xmwr;              /* master window rectangle */
    xrect        xwr;               /* subclient window rectangle */
    FT_Face      ftface;            /* current FreeType font face */
    Atom         delmsg;            /* windows manager delete window message */
    int          pfw;               /* parent/frame width (extra) */
    int          pfh;               /* parent frame height (extra) */
    int          cwox;              /* client window offset from parent
                                       origin x */
    int          cwoy;              /* client window offset from parent
                                       origin y */
    int          childfrm;          /* TRUE if using Ami-drawn child frame */
    char*        wintitle;          /* window title string (for child frames) */
    GC           frmgc;             /* GC for drawing on xmwhan (child frame) */
    int          minimized;         /* TRUE if child frame is minimized */
    int          minslot;           /* slot index when minimized (for x pos) */
    xrect        savxmwr;           /* xmwr before minimize, for restore */
    int          savgmaxxg;         /* gmaxxg before minimize */
    int          savgmaxyg;         /* gmaxyg before minimize */

} winrec;

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec* filptr;
typedef struct filrec {

      FILE*  sfp;  /* file pointer used to establish entry, or NULL */
      winptr win;  /* associated window (if exists) */
      int    inw;  /* entry is input linked to window */
      int    inl;  /* this output file is linked to the input file, logical */
      int    tim;  /* fid has a timer associated with it */
      winptr twin; /* window associated with timer */

} filrec;

/* internal client messages */
typedef enum {

    cm_timer /* timer fires */

} clientmessagecode;

/* error codes */
typedef enum {

    eftbful,  /* File table full */
    ejoyacc,  /* Joystick access */
    etimacc,  /* Timer access */
    efilopr,  /* Cannot perform operation on special file */
    einvscn,  /* Invalid screen number */
    einvhan,  /* Invalid handle */
    einvtab,  /* Invalid tab position */
    eatopos,  /* Cannot position text by pixel with auto on */
    eatocur,  /* Cannot position outside screen with auto on */
    eatoofg,  /* Cannot reenable auto off grid */
    eatoecb,  /* Cannot reenable auto outside screen */
    einvftn,  /* Invalid font number */
    etrmfnt,  /* Valid terminal font not found */
    eatofts,  /* Cannot resize font with auto enabled */
    eatoftc,  /* Cannot change fonts with auto enabled */
    einvfnm,  /* Invalid logical font number */
    efntemp,  /* Empty logical font */
    etrmfts,  /* Cannot size terminal font */
    etabful,  /* Too many tabs set */
    eatotab,  /* Cannot use graphical tabs with auto on */
    estrinx,  /* String index out of range */
    epicfnf,  /* Picture file not found */
    epicftl,  /* Picture filename too large */
    etimnum,  /* Invalid timer number */
    ejstsys,  /* Cannot justify system font */
    efnotwin, /* File is not attached to a window */
    ewinuse,  /* Window id in use */
    efinuse,  /* File already in use */
    einmode,  /* Input side of window in wrong mode */
    edcrel,   /* Cannot release Windows device context */
    einvsiz,  /* Invalid buffer size */
    ebufoff,  /* buffered mode not enabled */
    edupmen,  /* Menu id was duplicated */
    emennf,   /* Meny id was not found */
    ewignf,   /* Widget id was not found */
    ewigdup,  /* Widget id was duplicated */
    einvspos, /* Invalid scroll bar slider position */
    einvssiz, /* Invalid scroll bar size */
    ectlfal,  /* Attempt to create control fails */
    eprgpos,  /* Invalid progress bar position */
    estrspc,  /* Out of string space */
    etabbar,  /* Unable to create tab in tab bar */
    efildlg,  /* Unable to create file dialog */
    efnddlg,  /* Unable to create find dialog */
    efntdlg,  /* Unable to create font dialog */
    efndstl,  /* Find/replace string too long */
    einvwin,  /* Invalid window number */
    einvjye,  /* Invalid joystick event */
    ejoyqry,  /* Could not get information on joystick */
    einvjoy,  /* Invalid joystick ID */
    eclsinw,  /* Cannot directly close input side of window */
    ewigsel,  /* Widget is not selectable */
    ewigptxt, /* Cannot put text in this widget */
    ewiggtxt, /* Cannot get text from this widget */
    ewigdis,  /* Cannot disable this widget */
    estrato,  /* Cannot direct write string with auto on */
    etabsel,  /* Invalid tab select */
    enomem,   /* Out of memory */
    einvfil,  /* File is invalid */
    enotinp,  /* not input side of any window */
    estdfnt,  /* Cannot find standard font */
    eftntl,   /* Font name too large */
    epicopn,  /* Cannot open picture file */
    ebadfmt,  /* Bad format of picture file */
    ecfgval,  /* invalid configuration value */
    enoopn,   /* Cannot open file */
    enoinps,  /* no input side for this window */
    enowid,   /* No more window ids available */
    evecaxe,  /* cannot vector auxillary event */
    eangato,  /* cannot set character drawing angle in auto mode */
    eatoang,  /* Cannot reenable auto with non-90 degree text */

    /* unimplemented override errors */
    egetwigid_unimp,        /* getwigid unimplemented */
    ekillwidget_unimp,      /* killwidget unimplemented */
    eselectwidget_unimp,    /* selectwidget unimplemented */
    eenablewidget_unimp,    /* enablewidget unimplemented */
    egetwidgettext_unimp,   /* getwidgettext unimplemented */
    eputwidgettext_unimp,   /* putwidgettext unimplemented */
    esizwidget_unimp,       /* sizwidget unimplemented */
    esizwidgetg_unimp,      /* sizwidgetg unimplemented */
    eposwidget_unimp,       /* poswidget unimplemented */
    eposwidgetg_unimp,      /* poswidgetg unimplemented */
    ebackwidget_unimp,      /* backwidget unimplemented */
    efrontwidget_unimp,     /* frontwidget unimplemented */
    efocuswidget_unimp,     /* focuswidget unimplemented */
    ebuttonsiz_unimp,       /* buttonsiz unimplemented */
    ebuttonsizg_unimp,      /* buttonsizg unimplemented */
    ebutton_unimp,          /* button unimplemented */
    ebuttong_unimp,         /* buttong unimplemented */
    echeckboxsiz_unimp,     /* checkboxsiz unimplemented */
    echeckboxsizg_unimp,    /* checkboxsizg unimplemented */
    echeckbox_unimp,        /* checkbox unimplemented */
    echeckboxg_unimp,       /* checkboxg unimplemented */
    eradiobuttonsiz_unimp,  /* radiobuttonsiz unimplemented */
    eradiobuttonsizg_unimp, /* radiobuttonsizg unimplemented */
    eradiobutton_unimp,     /* radiobutton unimplemented */
    eradiobuttong_unimp,    /* radiobuttong unimplemented */
    egroupsizg_unimp,       /* groupsizg unimplemented */
    egroupsiz_unimp,        /* groupsiz unimplemented */
    egroup_unimp,           /* group unimplemented */
    egroupg_unimp,          /* groupg unimplemented */
    ebackground_unimp,      /* background unimplemented */
    ebackgroundg_unimp,     /* backgroundg unimplemented */
    escrollvertsizg_unimp,  /* scrollvertsizg unimplemented */
    escrollvertsiz_unimp,   /* scrollvertsiz unimplemented */
    escrollvert_unimp,      /* scrollvert unimplemented */
    escrollvertg_unimp,     /* scrollvertg unimplemented */
    escrollhorizsizg_unimp, /* scrollhorizsizg unimplemented */
    escrollhorizsiz_unimp,  /* scrollhorizsiz unimplemented */
    escrollhoriz_unimp,     /* scrollhoriz unimplemented */
    escrollhorizg_unimp,    /* scrollhorizg unimplemented */
    escrollpos_unimp,       /* scrollpos unimplemented */
    escrollsiz_unimp,       /* scrollsiz unimplemented */
    enumselboxsizg_unimp,   /* numselboxsizg unimplemented */
    enumselboxsiz_unimp,    /* numselboxsiz unimplemented */
    enumselbox_unimp,       /* numselbox unimplemented */
    enumselboxg_unimp,      /* numselboxg unimplemented */
    eeditboxsizg_unimp,     /* editboxsizg unimplemented */
    eeditboxsiz_unimp,      /* editboxsiz unimplemented */
    eeditbox_unimp,         /* editbox unimplemented */
    eeditboxg_unimp,        /* editboxg unimplemented */
    eprogbarsizg_unimp,     /* progbarsizg unimplemented */
    eprogbarsiz_unimp,      /* progbarsiz unimplemented */
    eprogbar_unimp,         /* progbar unimplemented */
    eprogbarg_unimp,        /* progbarg unimplemented */
    eprogbarpos_unimp,      /* progbarpos unimplemented */
    elistboxsizg_unimp,     /* listboxsizg unimplemented */
    elistboxsiz_unimp,      /* listboxsiz unimplemented */
    elistbox_unimp,         /* listbox unimplemented */
    elistboxg_unimp,        /* listboxg unimplemented */
    edropboxsizg_unimp,     /* dropboxsizg unimplemented */
    edropboxsiz_unimp,      /* dropboxsiz unimplemented */
    edropbox_unimp,         /* dropbox unimplemented */
    edropboxg_unimp,        /* dropboxg unimplemented */
    edropeditboxsizg_unimp, /* dropeditboxsizg unimplemented */
    edropeditboxsiz_unimp,  /* dropeditboxsiz unimplemented */
    edropeditbox_unimp,     /* dropeditbox unimplemented */
    edropeditboxg_unimp,    /* dropeditboxg unimplemented */
    eslidehorizsizg_unimp,  /* slidehorizsizg unimplemented */
    eslidehorizsiz_unimp,   /* slidehorizsiz unimplemented */
    eslidehoriz_unimp,      /* slidehoriz unimplemented */
    eslidehorizg_unimp,     /* slidehorizg unimplemented */
    eslidevertsizg_unimp,   /* slidevertsizg unimplemented */
    eslidevertsiz_unimp,    /* slidevertsiz unimplemented */
    eslidevert_unimp,       /* slidevert unimplemented */
    eslidevertg_unimp,      /* slidevertg unimplemented */
    etabbarsizg_unimp,      /* tabbarsizg unimplemented */
    etabbarsiz_unimp,       /* tabbarsiz unimplemented */
    etabbarclientg_unimp,   /* tabbarclientg unimplemented */
    etabbarclient_unimp,    /* tabbarclient unimplemented */
    etabbar_unimp,          /* tabbar unimplemented */
    etabbarg_unimp,         /* tabbarg unimplemented */
    etabsel_unimp,          /* tabsel unimplemented */
    ealert_unimp,           /* alert unimplemented */
    equerycolor_unimp,      /* querycolor unimplemented */
    equeryopen_unimp,       /* queryopen unimplemented */
    equerysave_unimp,       /* querysave unimplemented */
    equeryfind_unimp,       /* queryfind unimplemented */
    equeryfindrep_unimp,    /* queryfindrep unimplemented */
    equeryfont_unimp,       /* queryfont unimplemented */

    esystem   /* System consistency check */

} errcod;

/* mode to function table */
int mod2fnc[mdor+1] = {

    GXcopy, /* mdnorm */
    GXnoop, /* mdinvis */
    GXxor,  /* mdxor */
    GXand,  /* mdand */
    GXor    /* mdor */

};

/* XEvent queue structure. Its a bubble list. */
typedef struct xevtque {

    struct xevtque* next; /* next in list */
    struct xevtque* last; /* last in list */
    XEvent          evt;  /* event data */

} xevtque;

/* PA queue structure. Its a bubble list. */
typedef struct paevtque {

    struct paevtque* next; /* next in list */
    struct paevtque* last; /* last in list */
    ami_evtrec       evt;  /* event data */

} paevtque;

/* Joystick tracking structure */
typedef struct joyrec* joyptr; /* pointer to joystick record */
typedef struct joyrec {

    int fid;    /* joystick file id */
    int sid;    /* system event id */
    int axis;   /* number of joystick axes */
    int button; /* number of joystick buttons */
    int ax;     /* joystick x axis save */
    int ay;     /* joystick y axis save */
    int az;     /* joystick z axis save */
    int a4;     /* joystick axis 4 save */
    int a5;     /* joystick axis 5 save */
    int a6;     /* joystick axis 6 save */
    int no;     /* logical number of joystick, 1-n */

} joyrec;

/* logical system event record */
typedef struct systrk* sevtptr; /* pointer to system event */
typedef struct systrk {

    winptr  win; /* window associated with this event */
    int     tim; /* timer number assocated with this event */
    int     frm; /* is a framing timer */
    int     joy; /* joystick number associated with this event */

} systrk;

/* motif property decoration hints data structure. This is no longer defined
   in XLIB, but is still operative.  */
typedef struct
{
    unsigned long       flags;
    unsigned long       functions;
    unsigned long       decorations;
    long                inputmode;
    unsigned long       status;

} mwmhints;

/* top level frame parameters */
typedef enum {

    frmcfgall, /* all frame components on */
    frmcfgfrm, /* frame all off */
    frmcfgsiz, /* sizebars off */
    frmcfgsys, /* system bar off */

} frmcfg;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pread_t   ofpread_nocancel;
static pwrite_t  ofpwrite;
static pwrite_t  ofpwrite_nocancel;
static popen_t   ofpopen;
static popen_t   ofpopen_nocancel;
static pclose_t  ofpclose;
static pclose_t  ofpclose_nocancel;
static plseek_t  ofplseek;

/*
 * Override vectors for calls in this package
 *
 */
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
static ami_maxxg_t           maxxg_vect;;
static ami_maxyg_t           maxyg_vect;
static ami_curxg_t           curxg_vect;
static ami_curyg_t           curyg_vect;
static ami_line_t            line_vect;
static ami_linewidth_t       linewidth_vect;
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
static ami_setpoints_t        setpoints_vect;
static ami_points_t        points_vect;
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
static ami_querysave_t       querysave_vect;
static ami_querysave_t       querysave_vect;
static ami_querysave_t       querysave_vect;
static ami_queryfind_t       queryfind_vect;
static ami_queryfindrep_t    queryfindrep_vect;
static ami_queryfont_t       queryfont_vect;

/* X Windows globals */

static int fend;      /* end of program ordered flag */
static int fautohold; /* automatic hold on exit flag */
static pthread_mutex_t xwlock; /* XWindow call lock */

/* X windows display characteristics.
 *
 * Note that some of these are going to need to move to a per-window structure.
 */
static Display*   padisplay;      /* current display */
static int        pascreen;       /* current screen */

/* cursors used by child-frame edge resize feedback (created lazily) */
static Cursor cfrm_cursor_left     = 0;
static Cursor cfrm_cursor_right    = 0;
static Cursor cfrm_cursor_top      = 0;
static Cursor cfrm_cursor_bottom   = 0;
static Cursor cfrm_cursor_btmleft  = 0;
static Cursor cfrm_cursor_btmright = 0;
static Cursor cfrm_cursor_topleft  = 0;
static Cursor cfrm_cursor_topright = 0;
static Cursor cfrm_cursor_arrow    = 0;
static int        ctrll, ctrlr;   /* control key active */
static int        shiftl, shiftr; /* shift key active */
static int        altl, altr;     /* alt key active */
static int        capslock;       /* caps lock key active */
static filptr     opnfil[MAXFIL]; /* open files table */
static int        xltwin[MAXFIL*2+1]; /* window equivalence table, includes
                                         negatives and 0 */
static metptr     xltmnu[MAXFIL*2+1]; /* menu entry equivalence table */
static int        filwin[MAXFIL]; /* file to window equivalence table */
static int        esck;           /* previous key was escape */
static fontptr    fntlst;         /* list of fonts */
static int        fntcnt;         /* number of fonts */
static FT_Library ftlibrary;     /* FreeType library instance */
static picptr     frepic;         /* free picture entries */
static int        numjoy;         /* number of joysticks found */
static joyptr     joytab[MAXJOY]; /* joystick control table */
static int        frmfid;         /* framing timer fid */
static int        cfgcap;         /* "configuration" caps */
static ami_pevthan evthan[ami_ettabbar+1]; /* array of event handler routines */
static ami_pevthan evtshan;        /* single master event handler routine */
static xevtque*   freque;         /* free XEvent queue entries list */
static xevtque*   evtque;         /* XEvent input save queue */
static paevtque*  paqfre;         /* free PA event queue entries list */
static paevtque*  paqevt;         /* PA event input save queue */
static ami_pevthan menu_event_oeh; /* event callback save for menus */
static metptr     fremet;         /* free menu entrys list */
static winptr     winfre;         /* free windows structure list */
static int        stdchrx;        /* standard/reference character size x */
static int        stdchry;        /* standard/reference character size y */
static int        errflg;         /* an error has been flagged */
static int        dspsev;         /* XWindows display system event */
static sevtptr    sidtab[MAXSID]; /* system event table */
static int        xerrbyp;        /* bypass the xerror() handler */
static int        evtcnt;         /* count of PA event diagnostics output */

/* code storage for XWindow state atoms */
Atom cmaxhorz; /* horizontally maximized */
Atom cmaxvert; /* vertically maximized */
Atom cfocused; /* focused */
Atom chidden;  /* hidden (iconized) */

/* XWindow frame characteristics */
int               frmextwdt[frmcfgsys+1];   /* frame extra width */
int               frmexthgt[frmcfgsys+1];   /* frame extra height */
int               frmoffx[frmcfgsys+1];     /* frame offset to client x */
int               frmoffy[frmcfgsys+1];     /* frame offset to client y */

/* memory statistics/diagnostics */
static unsigned long memusd;    /* total memory in use for malloc */
static unsigned long memrty;    /* retries executed on malloc */
static unsigned long maxrty;    /* maximum retry count */
static unsigned long fontcnt;   /* font entry counter */
static unsigned long fonttot;   /* font entry total */
static unsigned long filcnt;    /* file entry counter */
static unsigned long filtot;    /* file entry total */
static unsigned long piccnt;    /* picture entry counter */
static unsigned long pictot;    /* picture entry total */
static unsigned long scncnt;    /* screen struct counter */
static unsigned long scntot;    /* screen struct total */
static unsigned long wincnt;    /* windows structure counter */
static unsigned long wintot;    /* windows structure total */
static unsigned long imgcnt;    /* image frame counter */
static unsigned long imgtot;    /* image frame total */
static unsigned long metcnt;    /* menu entries counter */
static unsigned long mettot;    /* menu entries total */

/* config settable runtime options */
static int maxxd;     /* default window dimensions */
static int maxyd;
static int dialogerr; /* send runtime errors to dialog */
static int mouseenb;  /* enable mouse */
static int joyenb;    /* enable joysticks */
static int dmpmsg;    /* enable dump messages (diagnostic, windows only) */
static int dmpevt;    /* enable dump Petit-Ami messages */
static int prtftm;    /* print font metrics (diagnostic) */
static int conpnt;    /* size of console font in points */

static void iopenwin(FILE** infile, FILE** outfile, FILE* parent, int wid,
                     int subclient);

/** ****************************************************************************

Present error dialog

Presents the given text in a dialog. Used for graphical errors. Tries to be
standalone, using little or no other resources in the system.

If an error dialog cannot be presented to font not found or other error, returns
1, otherwise returns 0 on success.

Notes:

Obviously this could use better ratios and less magic numbers.

*******************************************************************************/

#define NEGCIRCLE    80 /* negative sign circle size */
#define NEGCIRCLESPC 20 /* space around negative circle */

static int errdlg(
    /** dialog title */    char* t,
    /** dialog contents */ char* s
)

{

    Window            w;
    GC                cxt;
    XEvent            e;
    FT_Face           dlg_face;
    int               dlg_size;
    int               mw, wd;
    char              cb[] = "Close";
    int               ww, wh;
    int               cw;
    int               bx1, by1, bx2, by2;
    XWindowAttributes xwa;
    FcPattern*        pat;
    FcPattern*        match;
    FcResult          result;
    FcChar8*          fcfile;

    dlg_size = 14; /* dialog font pixel size */

    /* find a sans-serif font via fontconfig */
    pat = FcNameParse((FcChar8*)"sans:style=Bold Oblique");
    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    match = FcFontMatch(NULL, pat, &result);
    FcPatternDestroy(pat);

    if (!match) return (1); /* indicate display error */

    if (FcPatternGetString(match, FC_FILE, 0, &fcfile) != FcResultMatch) {

        FcPatternDestroy(match);
        return (1);

    }

    if (FT_New_Face(ftlibrary, (char*)fcfile, 0, &dlg_face)) {

        FcPatternDestroy(match);
        return (1);

    }
    FcPatternDestroy(match);
    FT_Set_Pixel_Sizes(dlg_face, 0, dlg_size);

    /* minimum width for dialog system bar */
    mw = ft_text_width(dlg_face, t, strlen(t))+200;
    /* minimum width for dialog contents */
    wd = NEGCIRCLESPC+NEGCIRCLE+NEGCIRCLESPC+ft_text_width(dlg_face, s, strlen(s));
    if (wd > mw) mw = wd; /* set minimum overall */

    XWLOCK();
    /* find screen placement */
    XGetWindowAttributes(padisplay, RootWindow(padisplay, pascreen), &xwa);

    ww = mw+40; /* set dialog width and height */
    wh = 200;
    /* create dialog window centered on screen */
    w = XCreateSimpleWindow(padisplay, RootWindow(padisplay, pascreen), 0, 0,
                            ww, wh, 5,
                            BlackPixel(padisplay, pascreen),
                            WhitePixel(padisplay, pascreen));
    XSelectInput(padisplay, w, ExposureMask|KeyPressMask|ButtonPressMask);
    XMapWindow(padisplay, w);
    /* centering works well unless you have dual screens */
    XMoveWindow(padisplay, w, xwa.width/2-ww/2, 0/*xwa.height/2-wh/2*/);
    cxt = XCreateGC(padisplay, w, 0, NULL);

    XStoreName(padisplay, w, t);
    XSetIconName(padisplay, w, t);
    XWUNLOCK();

    cw = ft_text_width(dlg_face, cb, strlen(cb))+50;
    /* set button rectangle */
    bx1 = ww-cw-20;
    by1 = 125;
    bx2 = bx1+cw-1;
    by2 = by1+40-1;

    do {

        XWLOCK();
        XNextEvent(padisplay, &e);
        XWUNLOCK();
        if (e.type == Expose && e.xany.window == w) {

            XWLOCK();
            /* set background color to grey */
            XSetForeground(padisplay, cxt, 0xe0e0e0);
            XFillRectangle(padisplay, w, cxt, e.xexpose.x, e.xexpose.y,
                           e.xexpose.width, e.xexpose.height);
            /* draw error circle */
            XSetForeground(padisplay, cxt, 0xce3c30);
            XFillArc(padisplay, w, cxt, NEGCIRCLESPC, NEGCIRCLESPC,
                     NEGCIRCLE, NEGCIRCLE, 0, 360*64);
            /* draw dash */
            XSetForeground(padisplay, cxt, 0xffffff);
            XFillRectangle(padisplay, w, cxt, 35, 60-5, 50, 10);
            /* set text color */
            XSetForeground(padisplay, cxt, 0x000000);
            /* center text on circle to the right */
            ft_draw_string(w, cxt, dlg_face, dlg_size, dlg_size,
                           NEGCIRCLESPC+NEGCIRCLE+NEGCIRCLESPC,
                           NEGCIRCLESPC+NEGCIRCLE/2, s, strlen(s));
            /* place close button */
            XSetForeground(padisplay, cxt, 0xffffff);
            XFillRectangle(padisplay, w, cxt, ww-cw-20, 125, cw, 40);
            XSetForeground(padisplay, cxt, 0x000000);
            XDrawRectangle(padisplay, w, cxt, ww-cw-20, 125, cw, 40);
            ft_draw_string(w, cxt, dlg_face, dlg_size, dlg_size,
                           ww-cw-20+15, 155, cb, strlen(cb));
            XWUNLOCK();

        } else if (e.type == ButtonPress) {

            /* close button press, exit */
            if (e.xbutton.x >= bx1 && e.xbutton.x <= bx2 &&
                e.xbutton.y >= by1 && e.xbutton.y <= by2) break;

        }

    } while (1);

    XWLOCK();
    XDestroyWindow(padisplay, w);
    XWUNLOCK();
    FT_Done_Face(dlg_face);

    return (0); /* exit no error */

}

/** ****************************************************************************

Get error string

Returns the error string corresponding to the error code given.

*******************************************************************************/

static char* errstr(errcod e)

{

    char* s;

    switch (e) { /* error */

        case eftbful:  s = "Too many files"; break;
        case ejoyacc:  s = "No joystick access available"; break;
        case etimacc:  s = "No timer access available"; break;
        case einvhan:  s = "Invalid file number"; break;
        case efilopr:  s = "Cannot perform operation on special file"; break;
        case einvscn:  s = "Invalid screen number"; break;
        case einvtab:  s = "Tab position specified off screen"; break;
        case eatopos:  s = "Cannot position text by pixel with auto on"; break;
        case eatocur:  s = "Cannot position outside screen with auto on"; break;
        case eatoofg:  s = "Cannot reenable auto off grid"; break;
        case eatoecb:  s = "Cannot reenable auto outside screen"; break;
        case einvftn:  s = "Invalid font number"; break;
        case etrmfnt:  s = "No valid terminal font was found"; break;
        case eatofts:  s = "Cannot resize font with auto enabled"; break;
        case eatoftc:  s = "Cannot change fonts with auto enabled"; break;
        case einvfnm:  s = "Invalid logical font number"; break;
        case efntemp:  s = "Logical font number has no assigned font"; break;
        case etrmfts:  s = "Cannot size terminal font"; break;
        case etabful:  s = "Too many tabs set"; break;
        case eatotab:  s = "Cannot set off grid tabs with auto on"; break;
        case estrinx:  s = "String index out of range"; break;
        case epicfnf:  s = "Picture file not found"; break;
        case epicftl:  s = "Picture filename too large"; break;
        case etimnum:  s = "Invalid timer number"; break;
        case ejstsys:  s = "Cannot justify system font"; break;
        case efnotwin: s = "File is not attached to a window"; break;
        case ewinuse:  s = "Window id in use"; break;
        case efinuse:  s = "File already in use"; break;
        case einmode:  s = "Input side of window in wrong mode"; break;
        case edcrel:   s = "Cannot release Windows device context"; break;
        case einvsiz:  s = "Invalid buffer size"; break;
        case ebufoff:  s = "Buffered mode not enabled"; break;
        case edupmen:  s = "Menu id was duplicated"; break;
        case emennf:   s = "Menu id was not found"; break;
        case ewignf:   s = "Widget id was not found"; break;
        case ewigdup:  s = "Widget id was duplicated"; break;
        case einvspos: s = "Invalid scroll bar slider position"; break;
        case einvssiz: s = "Invalid scroll bar slider size"; break;
        case ectlfal:  s = "Attempt to create control fails"; break;
        case eprgpos:  s = "Invalid progress bar position"; break;
        case estrspc:  s = "Out of string space"; break;
        case etabbar:  s = "Unable to create tab in tab bar"; break;
        case efildlg:  s = "Unable to create file dialog"; break;
        case efnddlg:  s = "Unable to create find dialog"; break;
        case efntdlg:  s = "Unable to create font dialog"; break;
        case efndstl:  s = "Find/replace string too long"; break;
        case einvwin:  s = "Invalid window number"; break;
        case einvjye:  s = "Invalid joystick event"; break;
        case ejoyqry:  s = "Could not get information on joystick"; break;
        case einvjoy:  s = "Invalid joystick ID"; break;
        case eclsinw:  s = "Cannot directly close input side of window"; break;
        case ewigsel:  s = "Widget is not selectable"; break;
        case ewigptxt: s = "Cannot put text in this widget"; break;
        case ewiggtxt: s = "Cannot get text from this widget"; break;
        case ewigdis:  s = "Cannot disable this widget"; break;
        case estrato:  s = "Cannot direct write string with auto on"; break;
        case etabsel:  s = "Invalid tab select"; break;
        case enomem:   s = "Out of memory"; break;
        case einvfil:  s = "File is invalid"; break;
        case enotinp:  s = "Not input side of any window"; break;
        case estdfnt:  s = "Cannot find standard font"; break;
        case eftntl:   s = "Font name too large"; break;
        case epicopn:  s = "Cannot open picture file"; break;
        case ebadfmt:  s = "Bad format of picture file"; break;
        case ecfgval:  s = "Invalid configuration value"; break;
        case enoopn:   s = "Cannot open file"; break;
        case enoinps:  s = "No input side for this window"; break;
        case enowid:   s = "No more window ids available"; break;
        case evecaxe:  s = "Cannot vector auxillary event"; break;
        case eangato:  s = "Cannot set character drawing angle in auto mode"; break;
        case eatoang:  s = "Cannot reenable auto with non-90 degree text"; break;
        case egetwigid_unimp:        s = "getwigid unimplemented"; break;
        case ekillwidget_unimp:      s = "killwidget unimplemented"; break;
        case eselectwidget_unimp:    s = "selectwidget unimplemented"; break;
        case eenablewidget_unimp:    s = "enablewidget unimplemented"; break;
        case egetwidgettext_unimp:   s = "getwidgettext unimplemented"; break;
        case eputwidgettext_unimp:   s = "putwidgettext unimplemented"; break;
        case esizwidget_unimp:       s = "sizwidget unimplemented"; break;
        case esizwidgetg_unimp:      s = "sizwidgetg unimplemented"; break;
        case eposwidget_unimp:       s = "poswidget unimplemented"; break;
        case eposwidgetg_unimp:      s = "poswidgetg unimplemented"; break;
        case ebackwidget_unimp:      s = "backwidget unimplemented"; break;
        case efrontwidget_unimp:     s = "frontwidget unimplemented"; break;
        case efocuswidget_unimp:     s = "focuswidget unimplemented"; break;
        case ebuttonsiz_unimp:       s = "buttonsiz unimplemented"; break;
        case ebuttonsizg_unimp:      s = "buttonsizg unimplemented"; break;
        case ebutton_unimp:          s = "button unimplemented"; break;
        case ebuttong_unimp:         s = "buttong unimplemented"; break;
        case echeckboxsiz_unimp:     s = "checkboxsiz unimplemented"; break;
        case echeckboxsizg_unimp:    s = "checkboxsizg unimplemented"; break;
        case echeckbox_unimp:        s = "checkbox unimplemented"; break;
        case echeckboxg_unimp:       s = "checkboxg unimplemented"; break;
        case eradiobuttonsiz_unimp:  s = "radiobuttonsiz unimplemented"; break;
        case eradiobuttonsizg_unimp: s = "radiobuttonsizg unimplemented"; break;
        case eradiobutton_unimp:     s = "radiobutton unimplemented"; break;
        case eradiobuttong_unimp:    s = "radiobuttong unimplemented"; break;
        case egroupsizg_unimp:       s = "groupsizg unimplemented"; break;
        case egroupsiz_unimp:        s = "groupsiz unimplemented"; break;
        case egroup_unimp:           s = "group unimplemented"; break;
        case egroupg_unimp:          s = "groupg unimplemented"; break;
        case ebackground_unimp:      s = "background unimplemented"; break;
        case ebackgroundg_unimp:     s = "backgroundg unimplemented"; break;
        case escrollvertsizg_unimp:  s = "scrollvertsizg unimplemented"; break;
        case escrollvertsiz_unimp:   s = "scrollvertsiz unimplemented"; break;
        case escrollvert_unimp:      s = "scrollvert unimplemented"; break;
        case escrollvertg_unimp:     s = "scrollvertg unimplemented"; break;
        case escrollhorizsizg_unimp: s = "scrollhorizsizg unimplemented"; break;
        case escrollhorizsiz_unimp:  s = "scrollhorizsiz unimplemented"; break;
        case escrollhoriz_unimp:     s = "scrollhoriz unimplemented"; break;
        case escrollhorizg_unimp:    s = "scrollhorizg unimplemented"; break;
        case escrollpos_unimp:       s = "scrollpos unimplemented"; break;
        case escrollsiz_unimp:       s = "scrollsiz unimplemented"; break;
        case enumselboxsizg_unimp:   s = "numselboxsizg unimplemented"; break;
        case enumselboxsiz_unimp:    s = "numselboxsiz unimplemented"; break;
        case enumselbox_unimp:       s = "numselbox unimplemented"; break;
        case enumselboxg_unimp:      s = "numselboxg unimplemented"; break;
        case eeditboxsizg_unimp:     s = "editboxsizg unimplemented"; break;
        case eeditboxsiz_unimp:      s = "editboxsiz unimplemented"; break;
        case eeditbox_unimp:         s = "editbox unimplemented"; break;
        case eeditboxg_unimp:        s = "editboxg unimplemented"; break;
        case eprogbarsizg_unimp:     s = "progbarsizg unimplemented"; break;
        case eprogbarsiz_unimp:      s = "progbarsiz unimplemented"; break;
        case eprogbar_unimp:         s = "progbar unimplemented"; break;
        case eprogbarg_unimp:        s = "progbarg unimplemented"; break;
        case eprogbarpos_unimp:      s = "progbarpos unimplemented"; break;
        case elistboxsizg_unimp:     s = "listboxsizg unimplemented"; break;
        case elistboxsiz_unimp:      s = "listboxsiz unimplemented"; break;
        case elistbox_unimp:         s = "listbox unimplemented"; break;
        case elistboxg_unimp:        s = "listboxg unimplemented"; break;
        case edropboxsizg_unimp:     s = "dropboxsizg unimplemented"; break;
        case edropboxsiz_unimp:      s = "dropboxsiz unimplemented"; break;
        case edropbox_unimp:         s = "dropbox unimplemented"; break;
        case edropboxg_unimp:        s = "dropboxg unimplemented"; break;
        case edropeditboxsizg_unimp: s = "dropeditboxsizg unimplemented"; break;
        case edropeditboxsiz_unimp:  s = "dropeditboxsiz unimplemented"; break;
        case edropeditbox_unimp:     s = "dropeditbox unimplemented"; break;
        case edropeditboxg_unimp:    s = "dropeditboxg unimplemented"; break;
        case eslidehorizsizg_unimp:  s = "slidehorizsizg unimplemented"; break;
        case eslidehorizsiz_unimp:   s = "slidehorizsiz unimplemented"; break;
        case eslidehoriz_unimp:      s = "slidehoriz unimplemented"; break;
        case eslidehorizg_unimp:     s = "slidehorizg unimplemented"; break;
        case eslidevertsizg_unimp:   s = "slidevertsizg unimplemented"; break;
        case eslidevertsiz_unimp:    s = "slidevertsiz unimplemented"; break;
        case eslidevert_unimp:       s = "slidevert unimplemented"; break;
        case eslidevertg_unimp:      s = "slidevertg unimplemented"; break;
        case etabbarsizg_unimp:      s = "tabbarsizg unimplemented"; break;
        case etabbarsiz_unimp:       s = "tabbarsiz unimplemented"; break;
        case etabbarclientg_unimp:   s = "tabbarclientg unimplemented"; break;
        case etabbarclient_unimp:    s = "tabbarclient unimplemented"; break;
        case etabbar_unimp:          s = "tabbar unimplemented"; break;
        case etabbarg_unimp:         s = "tabbarg unimplemented"; break;
        case etabsel_unimp:          s = "tabsel unimplemented"; break;
        case ealert_unimp:           s = "alert unimplemented"; break;
        case equerycolor_unimp:      s = "querycolor unimplemented"; break;
        case equeryopen_unimp:       s = "queryopen unimplemented"; break;
        case equerysave_unimp:       s = "querysave unimplemented"; break;
        case equeryfind_unimp:       s = "queryfind unimplemented"; break;
        case equeryfindrep_unimp:    s = "queryfindrep unimplemented"; break;
        case equeryfont_unimp:       s = "queryfont unimplemented"; break;
        case esystem:  s = "System consistency check"; break;
        default:       s = "Unknown error"; break;

    }

    return (s); /* return error string */

}

/** ****************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.

*******************************************************************************/

static void error(errcod e)

{

    int r; /* error return */

    r = 1; /* set error not displayed */
    if (dialogerr) {

        /* send error to dialog */
        r = errdlg("Graphics Module", errstr(e));

    }
    if (r) { /* send error to console */

        fprintf(stderr, "*** Error: graphics: %s\n", errstr(e));
        fflush(stderr); /* make sure error message is output */

    }
    errflg = TRUE; /* flag error occurred */

    exit(1);

}

/******************************************************************************

XWindow error handler

Receives errors from XLIB and handles them here.

******************************************************************************/

static int xerror(Display* d, XErrorEvent* e)

{

    char ebuf[250]; /* buffer for error string */

    /* if the bypass flag is on, just return ignoring the error */
    if (xerrbyp) return (0);

    /* by definition the XWindows lock is active, since xerror() will only be
       called from within XWindows */
    XWUNLOCK();
    /* get text of error */
    XGetErrorText(padisplay, e->error_code, ebuf, 250);
    fprintf(stderr, "*** Error: Graphics: XWindow: %s\n", ebuf);
    fflush(stderr); /* make sure error message is output */
    errflg = TRUE; /* flag error occurred */

    exit(1);

}

/******************************************************************************

Internal version of malloc/free

This wrapper around malloc/free both handles errors and also gives us a chance
to track memory useage. Running out of memory is a terminal event, so we don't
need to return.

Only allocated memory is tallied, no accounting is done for free(). The vast
majority of memory used in this package is never returned.

******************************************************************************/

static void *imalloc(size_t size)

{

    int rt;

    void* ptr;

    rt = 0;
    do {

        ptr = malloc(size);
        rt++;
        memrty++;
        if (memrty > maxrty) maxrty = memrty;

    } while (rt < 100);
    if (!ptr) {

#ifdef PRTMEM
        fprintf(stderr, "Malloc fail, memory used: %lu retries: %lu\n", memusd,
                        memrty);
        fprintf(stderr, "Maximum retry: %lu\n", maxrty);
        fprintf(stderr, "Font entry counter:    %lu\n", fontcnt);
        fprintf(stderr, "Font entry total:      %lu\n", fonttot);
        fprintf(stderr, "File entry counter:    %lu\n", filcnt);
        fprintf(stderr, "File entry total:      %lu\n", filtot);
        fprintf(stderr, "Picture entry counter: %lu\n", piccnt);
        fprintf(stderr, "Picture entry total:   %lu\n", pictot);
        fprintf(stderr, "Screen entry counter:  %lu\n", scncnt);
        fprintf(stderr, "Screen entry total:    %lu\n", scntot);
        fprintf(stderr, "Window entry counter:  %lu\n", wincnt);
        fprintf(stderr, "Window entry total:    %lu\n", wintot);
        fprintf(stderr, "Image frame counter:   %lu\n", imgcnt);
        fprintf(stderr, "Image frame total:     %lu\n", imgtot);
        fprintf(stderr, "Menu entries counter:  %lu\n", metcnt);
        fprintf(stderr, "Menu entries total:    %lu\n", mettot);
#endif
        error(enomem);

    }
    memusd += size;

    return ptr;

}

static void ifree(void* ptr)

{

    free(ptr);

}

/******************************************************************************

Print event type

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

void prtevtt(ami_evtcod e)

{

    switch (e) {

        case ami_etchar:    fprintf(stderr, "etchar   "); break;
        case ami_etup:      fprintf(stderr, "etup     "); break;
        case ami_etdown:    fprintf(stderr, "etdown   "); break;
        case ami_etleft:    fprintf(stderr, "etleft   "); break;
        case ami_etright:   fprintf(stderr, "etright  "); break;
        case ami_etleftw:   fprintf(stderr, "etleftw  "); break;
        case ami_etrightw:  fprintf(stderr, "etrightw "); break;
        case ami_ethome:    fprintf(stderr, "ethome   "); break;
        case ami_ethomes:   fprintf(stderr, "ethomes  "); break;
        case ami_ethomel:   fprintf(stderr, "ethomel  "); break;
        case ami_etend:     fprintf(stderr, "etend    "); break;
        case ami_etends:    fprintf(stderr, "etends   "); break;
        case ami_etendl:    fprintf(stderr, "etendl   "); break;
        case ami_etscrl:    fprintf(stderr, "etscrl   "); break;
        case ami_etscrr:    fprintf(stderr, "etscrr   "); break;
        case ami_etscru:    fprintf(stderr, "etscru   "); break;
        case ami_etscrd:    fprintf(stderr, "etscrd   "); break;
        case ami_etpagd:    fprintf(stderr, "etpagd   "); break;
        case ami_etpagu:    fprintf(stderr, "etpagu   "); break;
        case ami_ettab:     fprintf(stderr, "ettab    "); break;
        case ami_etenter:   fprintf(stderr, "etenter  "); break;
        case ami_etinsert:  fprintf(stderr, "etinsert "); break;
        case ami_etinsertl: fprintf(stderr, "etinsertl"); break;
        case ami_etinsertt: fprintf(stderr, "etinsertt"); break;
        case ami_etdel:     fprintf(stderr, "etdel    "); break;
        case ami_etdell:    fprintf(stderr, "etdell   "); break;
        case ami_etdelcf:   fprintf(stderr, "etdelcf  "); break;
        case ami_etdelcb:   fprintf(stderr, "etdelcb  "); break;
        case ami_etcopy:    fprintf(stderr, "etcopy   "); break;
        case ami_etcopyl:   fprintf(stderr, "etcopyl  "); break;
        case ami_etcan:     fprintf(stderr, "etcan    "); break;
        case ami_etstop:    fprintf(stderr, "etstop   "); break;
        case ami_etcont:    fprintf(stderr, "etcont   "); break;
        case ami_etprint:   fprintf(stderr, "etprint  "); break;
        case ami_etprintb:  fprintf(stderr, "etprintb "); break;
        case ami_etprints:  fprintf(stderr, "etprints "); break;
        case ami_etfun:     fprintf(stderr, "etfun    "); break;
        case ami_etmenu:    fprintf(stderr, "etmenu   "); break;
        case ami_etmouba:   fprintf(stderr, "etmouba  "); break;
        case ami_etmoubd:   fprintf(stderr, "etmoubd  "); break;
        case ami_etmoumov:  fprintf(stderr, "etmoumov "); break;
        case ami_ettim:     fprintf(stderr, "ettim    "); break;
        case ami_etjoyba:   fprintf(stderr, "etjoyba  "); break;
        case ami_etjoybd:   fprintf(stderr, "etjoybd  "); break;
        case ami_etjoymov:  fprintf(stderr, "etjoymov "); break;
        case ami_etresize:  fprintf(stderr, "etresize "); break;
        case ami_etterm:    fprintf(stderr, "etterm   "); break;
        case ami_etmoumovg: fprintf(stderr, "etmoumovg"); break;
        case ami_etframe:   fprintf(stderr, "etframe  "); break;
        case ami_etredraw:  fprintf(stderr, "etredraw "); break;
        case ami_etmin:     fprintf(stderr, "etmin    "); break;
        case ami_etmax:     fprintf(stderr, "etmax    "); break;
        case ami_etnorm:    fprintf(stderr, "etnorm   "); break;
        case ami_etfocus:   fprintf(stderr, "etfocus  "); break;
        case ami_etnofocus: fprintf(stderr, "etnofocus"); break;
        case ami_ethover:   fprintf(stderr, "ethover  "); break;
        case ami_etnohover: fprintf(stderr, "etnohover"); break;
        case ami_etmenus:   fprintf(stderr, "etmenus  "); break;
        case ami_etbutton:  fprintf(stderr, "etbutton "); break;
        case ami_etchkbox:  fprintf(stderr, "etchkbox "); break;
        case ami_etradbut:  fprintf(stderr, "etradbut "); break;
        case ami_etsclull:  fprintf(stderr, "etsclull "); break;
        case ami_etscldrl:  fprintf(stderr, "etscldrl "); break;
        case ami_etsclulp:  fprintf(stderr, "etsclulp "); break;
        case ami_etscldrp:  fprintf(stderr, "etscldrp "); break;
        case ami_etsclpos:  fprintf(stderr, "etsclpos "); break;
        case ami_etedtbox:  fprintf(stderr, "etedtbox "); break;
        case ami_etnumbox:  fprintf(stderr, "etnumbox "); break;
        case ami_etlstbox:  fprintf(stderr, "etlstbox "); break;
        case ami_etdrpbox:  fprintf(stderr, "etdrpbox "); break;
        case ami_etdrebox:  fprintf(stderr, "etdrebox "); break;
        case ami_etsldpos:  fprintf(stderr, "etsldpos "); break;
        case ami_ettabbar:  fprintf(stderr, "ettabbar "); break;

        default: fprintf(stderr, "???");

    }

}

/******************************************************************************

Print Petit-Ami event diagnostic

Prints a decoded version of PA events on one line, including paraemters. Only
prints if the dump PA event flag is true. Does not terminate the line.

Note: does not output a debugging preamble. If that is required, print it
before calling this routine.

******************************************************************************/

void prtevt(ami_evtptr er)

{

    if (dmpevt) {

        fprintf(stderr, "PA Event: %5d Window: %d ", evtcnt++, er->winid);
        prtevtt(er->etype);
        switch (er->etype) {

            case ami_etchar: fprintf(stderr, ": char: %c", er->echar); break;
            case ami_ettim: fprintf(stderr, ": timer: %d", er->timnum); break;
            case ami_etmoumov: fprintf(stderr, ": mouse: %d x: %4d y: %4d",
                                      er->mmoun, er->moupx, er->moupy); break;
            case ami_etmouba: fprintf(stderr, ": mouse: %d button: %d",
                                     er->amoun, er->amoubn); break;
            case ami_etmoubd: fprintf(stderr, ": mouse: %d button: %d",
                                     er->dmoun, er->dmoubn); break;
            case ami_etjoyba: fprintf(stderr, ": joystick: %d button: %d",
                                     er->ajoyn, er->ajoybn); break;
            case ami_etjoybd: fprintf(stderr, ": joystick: %d button: %d",
                                     er->djoyn, er->djoybn); break;
            case ami_etjoymov: fprintf(stderr, ": joystick: %d x: %4d y: %4d z: %4d "
                                      "a4: %4d a5: %4d a6: %4d", er->mjoyn,
                                      er->joypx, er->joypy, er->joypz,
                                      er->joyp4, er->joyp5, er->joyp6); break;
            case ami_etresize: fprintf(stderr, ": x: %d y: %d xg: %d yg: %d",
                                      er->rszx, er->rszy,
                                      er->rszxg, er->rszyg); break;
            case ami_etfun: fprintf(stderr, ": key: %d", er->fkey); break;
            case ami_etmoumovg: fprintf(stderr, ": mouse: %d x: %4d y: %4d",
                                       er->mmoung, er->moupxg, er->moupyg); break;
            case ami_etredraw: fprintf(stderr, ": sx: %4d sy: %4d ex: %4d ey: %4d",
                                      er->rsx, er->rsy, er->rex, er->rey); break;
            case ami_etmenus: fprintf(stderr, ": id: %d", er->menuid); break;
            case ami_etbutton: fprintf(stderr, ": id: %d", er->butid); break;
            case ami_etchkbox: fprintf(stderr, ": id: %d", er->ckbxid); break;
            case ami_etradbut: fprintf(stderr, ": id: %d", er->radbid); break;
            case ami_etsclull: fprintf(stderr, ": id: %d", er->sclulid); break;
            case ami_etscldrl: fprintf(stderr, ": id: %d", er->scldrid); break;
            case ami_etsclulp: fprintf(stderr, ": id: %d", er->sclupid); break;
            case ami_etscldrp: fprintf(stderr, ": id: %d", er->scldpid); break;
            case ami_etsclpos: fprintf(stderr, ": id: %d position: %d",
                                      er->sclpid, er->sclpos); break;
            case ami_etedtbox: fprintf(stderr, ": id: %d", er->edtbid); break;
            case ami_etnumbox: fprintf(stderr, ": id: %d number: %d",
                                      er->numbid, er->numbsl); break;
            case ami_etlstbox: fprintf(stderr, ": id: %d select: %d",
                                      er->lstbid, er->lstbsl); break;
            case ami_etdrpbox: fprintf(stderr, ": id: %d select: %d",
                                      er->drpbid, er->drpbsl); break;
            case ami_etdrebox: fprintf(stderr, ": id: %d", er->drebid); break;
            case ami_etsldpos: fprintf(stderr, ": id: %d postion: %d",
                                      er->sldpid, er->sldpos); break;
            case ami_ettabbar: fprintf(stderr, ": id: %d select: %d",
                                      er->tabid, er->tabsel); break;
            default: ;

        }

    }

}

/******************************************************************************

Print XWindow event type

A diagnostic. Prints the XWindow event type codes.

******************************************************************************/

void prtxevtt(int type)

{

    switch (type) {

        case 2:  fprintf(stderr, "KeyPress"); break;
        case 3:  fprintf(stderr, "KeyRelease"); break;
        case 4:  fprintf(stderr, "ButtonPress"); break;
        case 5:  fprintf(stderr, "ButtonRelease"); break;
        case 6:  fprintf(stderr, "MotionNotify"); break;
        case 7:  fprintf(stderr, "EnterNotify"); break;
        case 8:  fprintf(stderr, "LeaveNotify"); break;
        case 9:  fprintf(stderr, "FocusIn"); break;
        case 10: fprintf(stderr, "FocusOut"); break;
        case 11: fprintf(stderr, "KeymapNotify"); break;
        case 12: fprintf(stderr, "Expose"); break;
        case 13: fprintf(stderr, "GraphicsExpose"); break;
        case 14: fprintf(stderr, "NoExpose"); break;
        case 15: fprintf(stderr, "VisibilityNotify"); break;
        case 16: fprintf(stderr, "CreateNotify"); break;
        case 17: fprintf(stderr, "DestroyNotify"); break;
        case 18: fprintf(stderr, "UnmapNotify"); break;
        case 19: fprintf(stderr, "MapNotify"); break;
        case 20: fprintf(stderr, "MapRequest"); break;
        case 21: fprintf(stderr, "ReparentNotify"); break;
        case 22: fprintf(stderr, "ConfigureNotify"); break;
        case 23: fprintf(stderr, "ConfigureRequest"); break;
        case 24: fprintf(stderr, "GravityNotify"); break;
        case 25: fprintf(stderr, "ResizeRequest"); break;
        case 26: fprintf(stderr, "CirculateNotify"); break;
        case 27: fprintf(stderr, "CirculateRequest"); break;
        case 28: fprintf(stderr, "PropertyNotify"); break;
        case 29: fprintf(stderr, "SelectionClear"); break;
        case 30: fprintf(stderr, "SelectionRequest"); break;
        case 31: fprintf(stderr, "SelectionNotify"); break;
        case 32: fprintf(stderr, "ColormapNotify"); break;
        case 33: fprintf(stderr, "ClientMessage"); break;
        case 34: fprintf(stderr, "MappingNotify"); break;
        case 35: fprintf(stderr, "GenericEvent"); break;
        default: fprintf(stderr, "???"); break;

    }

}

/** ****************************************************************************

Print XEvent message

A diagnostic, prints fields in an XEvent message.

*******************************************************************************/

void prtxevt(XEvent* e)

{

    fprintf(stderr, "X Event: %5ld Window: %lx ", e->xany.serial,
            e->xany.window);
    prtxevtt(e->type);
    switch (e->type) {

        case Expose: fprintf(stderr, ": x: %d y: %d w: %d h: %d",
                             e->xexpose.x, e->xexpose.y,
                             e->xexpose.width, e->xexpose.height); break;
        case ConfigureNotify: fprintf(stderr, ": x: %d y: %d w: %d h: %d",
                             e->xconfigure.x, e->xconfigure.y,
                             e->xconfigure.width, e->xconfigure.height); break;
        case MotionNotify: fprintf(stderr, ": x: %d y: %d",
                                   e->xmotion.x, e->xmotion.y); break;
        case PropertyNotify: fprintf(stderr, ": atom: %s",
                                     XGetAtomName(padisplay, e->xproperty.atom));

    }

}

/******************************************************************************

Print attributes set

Prints the contents of a PA attributes set.

******************************************************************************/

void prtatset(int at)

{

    if (at & BIT(sablink)) fprintf(stderr, "blink ");
    if (at & BIT(sarev)) fprintf(stderr, "rev ");
    if (at & BIT(saundl)) fprintf(stderr, "underl ");
    if (at & BIT(sasuper)) fprintf(stderr, "super ");
    if (at & BIT(sasubs)) fprintf(stderr, "subs ");
    if (at & BIT(saital)) fprintf(stderr, "italic ");
    if (at & BIT(sabold)) fprintf(stderr, "bold ");
    if (at & BIT(sastkout)) fprintf(stderr, "strkout ");
    if (at & BIT(sacondensed)) fprintf(stderr, "cond ");
    if (at & BIT(saextended)) fprintf(stderr, "ext ");
    if (at & BIT(saxlight)) fprintf(stderr, "xlight ");
    if (at & BIT(salight)) fprintf(stderr, "light ");
    if (at & BIT(saxbold)) fprintf(stderr, "xbold ");
    if (at & BIT(sahollow)) fprintf(stderr, "hollow ");
    if (at & BIT(saraised)) fprintf(stderr, "raised ");

}

/******************************************************************************

Print capabilities set

Prints the contents of a XWindows capabilities set.

******************************************************************************/

void prtxcset(int caps)

{

    if (caps & BIT(xcnormal)) fprintf(stderr, "norm ");
    if (caps & BIT(xcmedium)) fprintf(stderr, "med ");
    if (caps & BIT(xcbold)) fprintf(stderr, "bold ");
    if (caps & BIT(xcdemibold)) fprintf(stderr, "dbold ");
    if (caps & BIT(xcdark)) fprintf(stderr, "dark ");
    if (caps & BIT(xcblack)) fprintf(stderr, "black ");
    if (caps & BIT(xclight)) fprintf(stderr, "light ");
    if (caps & BIT(xcroman)) fprintf(stderr, "rom ");
    if (caps & BIT(xcital)) fprintf(stderr, "ital ");
    if (caps & BIT(xcoblique)) fprintf(stderr, "obliq ");
    if (caps & BIT(xcrital)) fprintf(stderr, "rital ");
    if (caps & BIT(xcroblique)) fprintf(stderr, "robliq ");
    if (caps & BIT(xcnormalw)) fprintf(stderr, "normw ");
    if (caps & BIT(xcnarrow)) fprintf(stderr, "narrw ");
    if (caps & BIT(xccondensed)) fprintf(stderr, "cond ");
    if (caps & BIT(xcsemicondensed)) fprintf(stderr, "scond ");
    if (caps & BIT(xcexpanded)) fprintf(stderr, "exp ");
    if (caps & BIT(xcproportional)) fprintf(stderr, "prop ");
    if (caps & BIT(xcmonospace)) fprintf(stderr, "mono ");
    if (caps & BIT(xcchar)) fprintf(stderr, "char ");

}

/******************************************************************************

Print windows tree

Prints the tree structure of child windows. Automatically finds the top of the
tree.

******************************************************************************/

void prtwinety(winptr wp, int indent)

{

    winptr cp; /* child window pointer */

    if (wp) {

        fprintf(stderr, "%*cWindow: %p Master: %lx Subclient: %lx\n",
                        indent, ' ', wp, wp->xmwhan, wp->xwhan);
        indent += 4; /* index next level */
        cp = wp->childwin; /* index child window list */
        while (cp) {

            prtwinety(cp, indent); /* print this entry */
            cp = cp->childlst;  /* next child entry */

        }

    }

}

void prtwintre(winptr wp)

{

    fprintf(stderr, "Windows tree:\n");
    while (wp->parwin) wp = wp->parwin; /* find top of tree */
    prtwinety(wp, 0); /* print tree */

}

/** ****************************************************************************

Enable or disable window frame

Turns the window frame on and off. Expects the Xwindow handle, and an on/off
flag. Note that decoration properties can only be set/reset one at a time.

*******************************************************************************/

void enbxfrm(Window xwh, int e)

{

    Atom mwmHintsProperty;
    mwmhints hints;

    XWLOCK();
    mwmHintsProperty = XInternAtom(padisplay, "_MOTIF_WM_HINTS", 0);
    hints.flags = MWM_HINTS_DECORATIONS;
    if (e) hints.decorations = MWM_DECOR_ALL;
    else hints.decorations = 0; /* everything off */
    XChangeProperty(padisplay, xwh, mwmHintsProperty, mwmHintsProperty,
                    32, PropModeReplace, (unsigned char *)&hints, 5);
    XWUNLOCK();

}

/** ****************************************************************************

Enable or disable window size bars

Turns the window size bars on and off. Expects the Xwindow handle, and an on/off
flag. Note that decoration properties can only be set/reset one at a time.

On GNOME/Ubuntu 20.04 with GDM3 window manager, we are not capable of turning
off the size bars alone, so this is a no-op. It may work on other window
managers.

*******************************************************************************/

void enbxsiz(Window xwh, int e)

{

    Atom mwmHintsProperty;
    mwmhints hints;
    XSizeHints sh;

    XWLOCK();

    /* set motif hints to control decoration and function */
    mwmHintsProperty = XInternAtom(padisplay, "_MOTIF_WM_HINTS", 0);
    hints.flags = MWM_HINTS_DECORATIONS | MWM_HINTS_FUNCTIONS;
    if (e) {

        hints.decorations = MWM_DECOR_ALL;
        hints.functions = MWM_FUNC_ALL;

    } else {

        hints.decorations = MWM_DECOR_TITLE|MWM_DECOR_MENU|MWM_DECOR_MINIMIZE|
                            MWM_DECOR_MAXIMIZE;
        hints.functions = MWM_FUNC_MOVE|MWM_FUNC_MINIMIZE|MWM_FUNC_MAXIMIZE|
                          MWM_FUNC_CLOSE;

    }
    XChangeProperty(padisplay, xwh, mwmHintsProperty, mwmHintsProperty,
                    32, PropModeReplace, (unsigned char *)&hints, 5);

    /* constrain size via WM_NORMAL_HINTS: setting min == max prevents resize
       on WMs that ignore _MOTIF_WM_HINTS functions */
    if (!e) {

        XWindowAttributes a;
        XGetWindowAttributes(padisplay, xwh, &a);
        sh.flags = PMinSize | PMaxSize;
        sh.min_width = a.width;
        sh.min_height = a.height;
        sh.max_width = a.width;
        sh.max_height = a.height;
        XSetWMNormalHints(padisplay, xwh, &sh);

    } else {

        sh.flags = PMinSize | PMaxSize;
        sh.min_width = 1;
        sh.min_height = 1;
        sh.max_width = 32767;
        sh.max_height = 32767;
        XSetWMNormalHints(padisplay, xwh, &sh);

    }

    XWUNLOCK();

}

/** ****************************************************************************

Enable or disable window system bar

Turns the window system bar on and off. Expects the Xwindow handle, and an on/off
flag. Note that decoration properties can only be set/reset one at a time.

*******************************************************************************/

void enbxsys(Window xwh, int e)

{

    Atom mwmHintsProperty;
    mwmhints hints;

    XWLOCK();
    mwmHintsProperty = XInternAtom(padisplay, "_MOTIF_WM_HINTS", 0);
    hints.flags = MWM_HINTS_DECORATIONS;
    if (e) hints.decorations = MWM_DECOR_ALL;
    else hints.decorations = MWM_DECOR_BORDER;
    XChangeProperty(padisplay, xwh, mwmHintsProperty, mwmHintsProperty,
                    32, PropModeReplace, (unsigned char *)&hints, 5);
    XWUNLOCK();

}

/** ****************************************************************************

Get window frame measurements

Uses the difference between the given window and it's parent window to find the
extra width, extra height, and offset to client of the child window.

*******************************************************************************/

void fndfrmdif(Window xw, int* ew, int* eh, int* ox, int* oy)

{

    XWindowAttributes xwga, xpwga; /* XWindow get attributes */
    Window            pw, rw;
    Window*           cwl;
    unsigned          ncw;

    /* get frame measurements */
    XWLOCK();
    XQueryTree(padisplay, xw, &rw, &pw, &cwl, &ncw);
    XGetWindowAttributes(padisplay, pw, &xpwga);
    XGetWindowAttributes(padisplay, xw, &xwga);
    XWUNLOCK();

    /* find net extra width of frame from client area */
    *ew = xpwga.width-xwga.width;
    *eh = xpwga.height-xwga.height;
    /* find offset from parent origin to client origin */
    *ox = xwga.x;
    *oy = xwga.y;

#ifdef PRTFRM
    dbg_printf(dlinfo, "Frame extra width all: %d\n", frmextwdt[frmcfgall]);
    dbg_printf(dlinfo, "Frame extra height all: %d\n", frmexthgt[frmcfgall]);
    dbg_printf(dlinfo, "Parent to client offset all: x: %d y: %d\n",
                       frmoffx[frmcfgall], frmoffy[frmcfgall]);
#endif

}

/** ****************************************************************************

Find Xwindow frame specifications

Finds the extra width, height, and offset to client of a XWindows frame. These
characteristics are the same in all windows. We find them by creating a test
window and measuring the difference between the outer (frame) window and the
client window.

*******************************************************************************/

void fndfrm(void)

{

    Window            wh;
    XEvent            xe;
    Window            pw, rw;
    Window*           cwl;
    unsigned          ncw;
    XWindowAttributes xwga, xpwga; /* XWindow get attributes */
    GC                xcxt;

    /* measure window frame characteristics */
    XWLOCK();
    wh = XCreateWindow(padisplay, RootWindow(padisplay, pascreen), 0, 0, 1, 1,
                       0, CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
    /* select what events we want */
    XSelectInput(padisplay, wh, ExposureMask|KeyPressMask|
                 KeyReleaseMask|PointerMotionMask|ButtonPressMask|
                 ButtonReleaseMask|StructureNotifyMask|FocusChangeMask|
                 EnterWindowMask|LeaveWindowMask|PropertyChangeMask);
    XMapWindow(padisplay, wh);
    XFlush(padisplay);
    XWUNLOCK();

    /* wait window present */
    do {

        XWLOCK();
        XNextEvent(padisplay, &xe); /* get next event */
        XWUNLOCK();

    } while (xe.type != MapNotify || xe.xany.window != wh);

    /* get frame measurements */
    fndfrmdif(wh, &frmextwdt[frmcfgall], &frmexthgt[frmcfgall],
                  &frmoffx[frmcfgall], &frmoffy[frmcfgall]);

    /* adjust and unmap window */
    XWLOCK();
    XMoveWindow(padisplay, wh, 0, 0);
    XUnmapWindow(padisplay, wh);
    /* destroy the window */
    XDestroyWindow(padisplay, wh);
    XWUNLOCK();

    /* set frame off parameters to all zeros */
    frmextwdt[frmcfgfrm] = 0;
    frmexthgt[frmcfgfrm] = 0;
    frmoffx[frmcfgfrm] = 0;
    frmoffy[frmcfgfrm] = 0;

    /* set sizebars off parameters to same as normal (no-op) */
    frmextwdt[frmcfgsiz] = frmextwdt[frmcfgfrm] = 0;
    frmexthgt[frmcfgsiz] = frmexthgt[frmcfgfrm] = 0;
    frmoffx[frmcfgsiz] = frmoffx[frmcfgfrm] = 0;
    frmoffy[frmcfgsiz] = frmoffy[frmcfgfrm] = 0;

    /* set system bar off parameters to all zeros */
    frmextwdt[frmcfgsys] = 0;
    frmexthgt[frmcfgsys] = 0;
    frmoffx[frmcfgsys] = 0;
    frmoffy[frmcfgsys] = 0;

}

/** ****************************************************************************

Default event handler

If we reach this event handler, it means none of the overriders has handled the
event, but rather passed it down. We flag the event was not handled and return,
which will cause the event to return to the event() caller.

*******************************************************************************/

static void defaultevent(ami_evtrec* ev)

{

    /* set not handled and exit */
    ev->handled = 0;

}

/*******************************************************************************

Place string in storage

Places the given string into dynamic storage, and returns that.

*******************************************************************************/

static char* str(char* s)

{

    char* p;

    p = imalloc(strlen(s)+1);
    strcpy(p, s);

    return (p);

}

/******************************************************************************

Translate colors code

Translates an independent to a terminal specific primary RGB color code for
XWindow.

******************************************************************************/

int colnum(ami_color c)

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

    }

    return (n); /* return number */

}

/*******************************************************************************

Translate rgb to XWindow color

Translates a ratioed INT_MAX graph color to the XWindow form, which is a 32
bit word with blue, green and red bytes.

*******************************************************************************/

static int rgb2xwin(int r, int g, int b)

{

   return ((r/8388608)*65536+(g/8388608)*256+(b/8388608));

}

/*******************************************************************************

Search for font by name

Finds a font in the list of fonts. Also matches fixed/no fixed pitch status.

*******************************************************************************/

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

/*******************************************************************************

Find font by family substring

Searches the font list for a font whose name contains the given substring
(case-insensitive). Used for flexible standard font selection with fontconfig
names.

*******************************************************************************/

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
                        tolower((unsigned char)n[s - sub])) break;
                if (!*s) return p; /* found match */

            }

        }
        p = p->next;

    }

    return NULL;

}

/*******************************************************************************

Search for font by name

Finds a font in the list of fonts by name.

*******************************************************************************/

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

/*******************************************************************************

Delete font

Deletes the given font entry from the global font list. Does not recycle it.

*******************************************************************************/

void delfnt(fontptr fp)

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

/*******************************************************************************

Print font list

A diagnostic, prints the internal font list.

*******************************************************************************/

void prtfnt(void)

{

    fontptr  fp;
    int      c;
    xcaplst* cp;

    fp = fntlst;
    c = 1;
    while (fp) {

        dbg_printf(dlinfo, "Font %2d: %s Capabilities: ", c, fp->fn);
        prtxcset(fp->caps);
        fprintf(stderr, "\n");
        cp = fp->caplst;
        while (cp) {

            fprintf(stderr, "    ");
            prtxcset(cp->caps);
            fprintf(stderr, "\n");
            cp = cp->next;

        }
        fp = fp->next;
        c++;

    }

}

/*******************************************************************************

Preselect standard fonts

Selects the list of 1-4 standard fonts, and reorders the list so they are at the
top.

The standard fonts are selected by name in this version. This relies on XWindow
having a standard series of fonts that it carries with it. They could also be
selected by capabilities, but the sticking point there would be to determine
serif/sans serif, which does not have a formal capability.

We prefer 10646-1 (Unicode) fonts here, but will take 8859-1 (ISO international
8 bit).

It is an error in this version if we can't find all the standard fonts. This
means that programs don't need to account for missing fonts as in old style PA.
The programs that try to account for missing fonts still work, because the fonts
will never be missing.

In the current versions, the technical font is just a copy of the sign or sans
serif font. It used to be a vector stroke font, but that is no longer necessary.
We actually make a copy of the font entry to keep accounting correct. Its up to
the user to remove that in lists as required, and perhaps alphabetize the fonts.

Todo

1. Need to check fonts have the correct capabilities, ie., roman/no slant, etc.

*******************************************************************************/

void stdfont(void)

{

    fontptr nfl; /* new font list */
    fontptr fp; /* font pointer */
    fontptr sp; /* sign font pointer */

    /* select first 4 fonts for standard fonts */
    nfl = NULL; /* clear target list */

    /* search 1: terminal (fixed pitch) font */
    fp = fndfntsub("courier 10 pitch", TRUE);
    if (!fp) fp = fndfntsub("courier new", TRUE);
    if (!fp) fp = fndfntsub("dejavu sans mono", TRUE);
    if (!fp) fp = fndfntsub("liberation mono", TRUE);
    if (!fp) fp = fndfntsub("noto sans mono", TRUE);
    if (!fp) fp = fndfntsub("menlo", TRUE);       /* macOS */
    if (!fp) fp = fndfntsub("monaco", TRUE);      /* macOS */
    if (!fp) fp = fndfntsub("andale mono", TRUE); /* macOS */
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
    if (!fp) fp = fndfntsub("georgia", FALSE);  /* macOS */
    if (!fp) fp = fndfntsub("palatino", FALSE); /* macOS */
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
    if (!fp) fp = fndfntsub("helvetica neue", FALSE); /* macOS */
    if (!fp) fp = fndfntsub("helvetica:", FALSE);     /* macOS */
    if (!fp) fp = fndfntsub("arial:", FALSE);         /* macOS */
    if (!fp) fp = fndfntsub("verdana", FALSE);        /* macOS */
    if (!fp) fp = fndfntsub("sans", FALSE);
    if (!fp) error(estdfnt);
    delfnt(fp);
    fp->next = nfl;
    nfl = fp;
    sp = fp; /* save sign font */

    /* search 4: technical font, make copy of sign */
    fp = (fontptr)imalloc(sizeof(fontrec));
    fontcnt++;
    fonttot += sizeof(fontrec);

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

/*******************************************************************************

Load fonts list

Loads the font list using fontconfig. We only load scalable fonts, since PA has
no ability to adapt to fonts that don't scale. The font names are constructed as
"foundry: family: iso10646-1" for compatibility with the internal naming system.

*******************************************************************************/

void getfonts(void)

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
    FcBool       fcscalable;
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

        /* construct display name: "foundry: family: iso10646-1" */
        dp = buf;
        {
            const char* s;
            s = (const char*)fcfoundry;
            while (*s) { *dp = tolower(*s); dp++; s++; }
        }
        *dp++ = ':'; *dp++ = ' ';
        {
            const char* s;
            s = (const char*)fcfamily;
            while (*s) { *dp = tolower(*s); dp++; s++; }
        }
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
            fontcnt++;
            fonttot += sizeof(fontrec);
            flp->fn = (string)imalloc(strlen(buf)+1);
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
        xcl->path = (string)imalloc(strlen((char*)fcfile)+1);
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

#ifdef PRTFNT
    /* print resulting font list */
    dbg_printf(dlinfo, "Internal font list:\n");
    prtfnt();
    fflush(stderr);
#endif

}

/*******************************************************************************

Find requested font capabilities

Finds the set of XWindows capabilities that are requested in the set of PA
attributes.

*******************************************************************************/

int fndxcap(int caps, int at)

{

    int ncaps;   /* XWindow font capabilities */

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

/*******************************************************************************

Select attributes by priority

Given a set of Petit-Ami attributes, finds a set of XWindows capabilities from
the list of possible capabilities by selecting all of the possible attributes,
then removing attributes by lowest priority until a match is found. This is
required because the XWindow capability set may have conflicting attributes. For
example, bold may have to have extended due to the new size of the font.

Returns the resulting set of XWindow capabilities as a set.

*******************************************************************************/

/* find number of bits in set */
int bitcnt(int i)

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
int matchcap(int caps, xcaplst* cl, int* mc)

{

    int fnd; /* found/not found flag */
    int bn, bn2;  /* bit number */

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

int fndxcapp(fontptr fp, int at)

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
    match = FALSE;
//dbg_printf(dlinfo, "full caps: "); prtxcset(fp->caps); fprintf(stderr, "\n"); fflush(stderr);
    do { /* search capabilities */

//dbg_printf(dlinfo, "at: "); prtatset(at); fprintf(stderr, "\n"); fflush(stderr);
        /* find capabilities from this set of attributes */
        caps = fndxcap(fp->caps, at);
//dbg_printf(dlinfo, "caps: "); prtxcset(caps); fprintf(stderr, "\n"); fflush(stderr);
        if (matchcap(caps, fp->caplst, &mc)) match = TRUE; /* found a match */
        else { /* try again */

//dbg_printf(dlinfo, "Will remove: "); prtatset(BIT(cappri[ia])); fprintf(stderr, "\n"); fflush(stderr);
            at &= ~BIT(cappri[ia]); /* remove attribute by priority */
            lia = ia; /* save select */
            ia++; /* next attribute */

        }

    } while (!match &&
             (cappri[lia] != INT_MAX)); /* until found or no more attributes */
    /* if we still have not found anything, it has to be a system error, since
       we all attributes */
    if (!match) error(esystem);
//dbg_printf(dlinfo, "matching caps: "); prtxcset(mc); fprintf(stderr, "\n"); fflush(stderr);

    return (mc); /* return matching caps */

}

/*******************************************************************************

Select font

Sets the currently selected font active. Processes all current attributes as
applicable, if the XWindow font set has that capability. Capability must be an
exact match, that is, capability indicates an actual XWindows font with that
capability exists. If any capability is not found, we default to blank or no
characters in the field.

The translation of attributes to capabilities is done on a priority basis. For
example, bold and light are mutually exclusive, and bold is prioritized.

*******************************************************************************/

void setfnt(winptr win)

{

    fontptr  fp;   /* pointer to current font */
    int      caps; /* matched capabilities set */
    xcaplst* cl;   /* capability list pointer */
    xcaplst* best; /* best matching entry */

    /* release any existing FreeType face */
    if (win->ftface) {

        fflush(stderr);
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

    /* Set the em-square pixel size so the resulting character cell
       (ascender + |descender| + 2) fits within win->gfcellh. This keeps
       the cell height constant across font changes. On first call
       (gfcellh == 0), bootstrap gfcellh from the initial gfhigh. */
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
        win->gfhigh    = pixsiz;
        win->gfhighx   = (win->vsy != 0.0f) ?
                          (int)(pixsiz * win->vsx / win->vsy) : pixsiz;
        win->gfpoint   = pixsiz * 2835.0f / (float)win->sdpmy;
        win->linespace = win->gfcellh;
        win->baseoff   = asc + 1;
    }
    win->charspace = (int)(win->ftface->size->metrics.max_advance >> 6);
    win->chrspcx = 0;
    win->chrspcy = 0;

    if (prtftm) {

        dbg_printf(dlinfo, "FreeType font: %s\n", best->path);
        dbg_printf(dlinfo, "Font ascender:  %d\n", win->baseoff);
        dbg_printf(dlinfo, "Font descender: %d\n",
                   (int)(win->ftface->size->metrics.descender >> 6));
        dbg_printf(dlinfo, "Width of character cell:  %d\n", win->charspace);
        dbg_printf(dlinfo, "Height of character cell: %d\n", win->linespace);
        dbg_printf(dlinfo, "Base offset:              %d\n", win->baseoff);

    }

}

/*******************************************************************************

Find width of character in XWindow

Finds and returns the width of a character in XWindow. Normally used for
proportional fonts.

*******************************************************************************/

int xwidth(winptr win, char c)

{

    if (FT_Load_Char(win->ftface, (unsigned char)c, FT_LOAD_DEFAULT))
        return 0;
    return (int)(win->ftface->glyph->advance.x >> 6);

}

/*******************************************************************************

Glyph cache for FreeType rendered glyphs

Caches 1-bit depth Pixmaps of rendered glyphs to avoid recreating them on
every draw call.

*******************************************************************************/

#define GLYPH_CACHE_SIZE 512

typedef struct {

    FT_Face  face;          /* font face this glyph belongs to */
    int      pixel_size_y;  /* em-square pixel size y */
    int      pixel_size_x;  /* em-square pixel size x (asymmetric scaling) */
    int      glyph_index;   /* glyph index in font */
    Pixmap   stipple;     /* 1-bit depth pixmap */
    int      width;       /* bitmap width */
    int      height;      /* bitmap height */
    int      bitmap_left; /* left bearing */
    int      bitmap_top;  /* top bearing */
    int      advance;     /* horizontal advance */
    int      valid;       /* entry is valid */

} glyphcache;

static glyphcache gcache[GLYPH_CACHE_SIZE];

/* Invalidate all cache entries that reference a given face */
static void ft_invalidate_face(FT_Face face)

{

    int i;

    for (i = 0; i < GLYPH_CACHE_SIZE; i++) {

        if (gcache[i].valid && gcache[i].face == face) {

            if (gcache[i].stipple)
                XFreePixmap(padisplay, gcache[i].stipple);
            gcache[i].valid = 0;
            gcache[i].stipple = 0;

        }

    }

}

/*******************************************************************************

Find or create cached glyph

Looks up the glyph in the cache. If not found, renders it with FreeType and
creates a 1-bit Pixmap for stipple-based rendering.

*******************************************************************************/

static glyphcache* ft_cache_glyph(FT_Face face, int pixel_size_x,
                                   int pixel_size_y, char c)

{

    unsigned int gi;
    unsigned int hash;
    glyphcache* ge;
    FT_GlyphSlot slot;
    int w, h, pitch;
    unsigned char* buf;
    char* data;
    int bw;
    int i, j;
    XImage* ximg;
    Pixmap pix;
    GC gc1;

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
    if (ge->valid && ge->stipple)
        XFreePixmap(padisplay, ge->stipple);

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

        ge->stipple = 0;
        return ge;

    }

    buf = slot->bitmap.buffer;
    pitch = slot->bitmap.pitch;

    /* create 1-bit data from FreeType 8-bit alpha bitmap.
       Let XCreateImage set native byte/bit order, then use XPutPixel
       to avoid bit order issues. */
    bw = (w + 7) / 8; /* bytes per row */
    data = (char*)calloc(bw * h, 1);

    ximg = XCreateImage(padisplay, DefaultVisual(padisplay, pascreen),
                        1, XYBitmap, 0, data, w, h, 8, bw);
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            XPutPixel(ximg, i, j, (buf[j * pitch + i] > 127) ? 1 : 0);


    pix = XCreatePixmap(padisplay, DefaultRootWindow(padisplay), w, h, 1);
    {
        XGCValues gcv;
        gcv.foreground = 1;
        gcv.background = 0;
        gc1 = XCreateGC(padisplay, pix, GCForeground | GCBackground, &gcv);
    }
    XPutImage(padisplay, pix, gc1, ximg, 0, 0, 0, 0, w, h);
    XFreeGC(padisplay, gc1);

    XDestroyImage(ximg); /* frees data */
    ge->stipple = pix;

    return ge;

}

/*******************************************************************************

Draw single character using FreeType

Renders a character glyph onto an X11 Drawable using a cached 1-bit stipple
Pixmap. This respects the GC function mode (normal, XOR, AND, OR).

*******************************************************************************/

static void ft_draw_char(Drawable d, GC gc, FT_Face face,
                         int pixel_size_x, int pixel_size_y,
                         int x, int y, char c)

{

    glyphcache* ge;

    ge = ft_cache_glyph(face, pixel_size_x, pixel_size_y, c);
    if (!ge || !ge->stipple) return;

    XSetStipple(padisplay, gc, ge->stipple);
    XSetFillStyle(padisplay, gc, FillStippled);
    XSetTSOrigin(padisplay, gc, x + ge->bitmap_left, y - ge->bitmap_top);
    XFillRectangle(padisplay, d, gc, x + ge->bitmap_left, y - ge->bitmap_top,
                   ge->width, ge->height);
    XSetFillStyle(padisplay, gc, FillSolid);

}

/*******************************************************************************

Draw string using FreeType

Renders a string onto an X11 Drawable using cached glyph stipples.

*******************************************************************************/

static void ft_draw_string(Drawable d, GC gc, FT_Face face,
                           int pixel_size_x, int pixel_size_y,
                           int x, int y, char* s, int len)

{

    int i;

    for (i = 0; i < len; i++) {

        ft_draw_char(d, gc, face, pixel_size_x, pixel_size_y, x, y, s[i]);
        if (FT_Load_Char(face, (unsigned char)s[i], FT_LOAD_DEFAULT) == 0)
            x += (int)(face->glyph->advance.x >> 6);

    }

}

/*******************************************************************************

Calculate text width using FreeType

Returns the total pixel width of a string rendered with the given face.

*******************************************************************************/

static int ft_text_width(FT_Face face, const char* s, int len)

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

/*******************************************************************************

Draw rotated character using FreeType

Renders a character at an arbitrary angle onto an X11 Drawable. The glyph
bitmap is rendered at 0 degrees then rotated client-side using the same
pixel rotation algorithm from rotated.c.

*******************************************************************************/

static void ft_draw_char_rotated(Drawable d, GC gc, FT_Face face,
                                 int pixel_size_x, int pixel_size_y,
                                 float angle_rad,
                                 int x, int y, char c)

{

    FT_GlyphSlot slot;
    int w, h, pitch;
    unsigned char* buf;
    int ow, oh;        /* output dimensions */
    float sin_a, cos_a;
    int i, j, si, sj;
    int bw;
    char* data;
    XImage* ximg;
    Pixmap pix;
    GC gc1;
    float cx, cy;      /* center of input */
    float ocx, ocy;    /* center of output */

    FT_Set_Pixel_Sizes(face, pixel_size_x, pixel_size_y);
    if (FT_Load_Char(face, (unsigned char)c, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
        return;

    slot = face->glyph;
    w = slot->bitmap.width;
    h = slot->bitmap.rows;
    if (w == 0 || h == 0) return;

    buf = slot->bitmap.buffer;
    pitch = slot->bitmap.pitch;

    sin_a = sin(angle_rad);
    cos_a = cos(angle_rad);

    /* compute output bounding box */
    ow = (int)(fabs(w * cos_a) + fabs(h * sin_a) + 2);
    oh = (int)(fabs(w * sin_a) + fabs(h * cos_a) + 2);
    if (ow == 0 || oh == 0) return;

    cx = w / 2.0f;
    cy = h / 2.0f;
    ocx = ow / 2.0f;
    ocy = oh / 2.0f;

    /* create 1-bit rotated bitmap using XPutPixel for portable bit order */
    bw = (ow + 7) / 8;
    data = (char*)calloc(bw * oh, 1);

    ximg = XCreateImage(padisplay, DefaultVisual(padisplay, pascreen),
                        1, XYBitmap, 0, data, ow, oh, 8, bw);
    for (j = 0; j < oh; j++) {
        for (i = 0; i < ow; i++) {

            /* reverse-map from output to input */
            float dx = i - ocx;
            float dy = j - ocy;
            si = (int)(dx * cos_a + dy * sin_a + cx);
            sj = (int)(-dx * sin_a + dy * cos_a + cy);
            if (si >= 0 && si < w && sj >= 0 && sj < h) {

                if (buf[sj * pitch + si] > 127)
                    XPutPixel(ximg, i, j, 1);

            }

        }
    }

    /* paint the rotated glyph pixel by pixel */
    {
        int gx, gy;
        /* adjust position for rotation center offset */
        gx = x + slot->bitmap_left - (int)(ocx - cx * cos_a - cy * sin_a);
        gy = y - slot->bitmap_top - (int)(ocy + cx * sin_a - cy * cos_a);

        for (j = 0; j < oh; j++)
            for (i = 0; i < ow; i++)
                if (XGetPixel(ximg, i, j))
                    XDrawPoint(padisplay, d, gc, gx + i, gy + j);
    }

    XDestroyImage(ximg); /* frees data */

}

/*******************************************************************************

Invalidate glyph cache

Clears all cached glyph pixmaps. Called when font system is shutting down.

*******************************************************************************/

static void ft_cache_clear(void)

{

    int i;

    XWLOCK();
    for (i = 0; i < GLYPH_CACHE_SIZE; i++) {

        if (gcache[i].valid && gcache[i].stipple)
            XFreePixmap(padisplay, gcache[i].stipple);
        gcache[i].valid = 0;

    }
    XWUNLOCK();

}

/*******************************************************************************

Append menu entry

Appends a new menu entry to the given list.

*******************************************************************************/

static void appendmenu(ami_menuptr* list, ami_menuptr m)

{

    ami_menuptr lp;

    /* clear these links for insurance */
    m->next = NULL; /* clear next */
    m->branch = NULL; /* clear branch */
    if (!*list) *list = m; /* list empty, set as first entry */
    else { /* list non-empty */

        /* find last entry in list */
        lp = *list; /* index 1st on list */
        while (lp->next) lp = lp->next;
        lp->next = m; /* append at end */

    }

}

/*******************************************************************************

Check in display mode

Checks if the current update screen is also the current display screen. Returns
TRUE if so. If the screen is in display, it means that all of the actions to
the update screen should also be reflected on the real screen.

*******************************************************************************/

static int indisp(winptr win)

{

    return (win->curupd == win->curdsp);

}

/*******************************************************************************

Clear screen buffer

Clears the entire screen buffer to spaces with the current colors and
attributes.

*******************************************************************************/

static void clrbuf(scnptr sc)

{

    XWLOCK();
    XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
    XFillRectangle(padisplay, sc->xbuf, sc->xcxt, 0, 0, sc->maxxg, sc->maxyg);
    XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

static void getfil(filptr* fp)

{

    *fp = imalloc(sizeof(filrec)); /* get new file entry */
    filcnt++;
    filtot += sizeof(filrec);
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = -1; /* set no input file linked */
    (*fp)->tim = 0; /* set no timer associated with it */
    (*fp)->twin = NULL; /* set no window for timer */

}

/** ****************************************************************************

Get system event entry

Gets a system entry in the system table for the given system event number.

*******************************************************************************/

void getsee(int sid)

{

    if (!sidtab[sid-1]) {

        sidtab[sid-1] = imalloc(sizeof(systrk));
        sidtab[sid-1]->win = NULL; /* set no window */
        sidtab[sid-1]->tim = 0; /* set no timer */
        sidtab[sid-1]->frm = FALSE; /* set not a frame timer */
        sidtab[sid-1]->joy = 0; /* set no joystick */

    }

}

/** ****************************************************************************

Get picture entry

Allocates and returns a new picture entry.

*******************************************************************************/

static picptr getpic(void)

{

    picptr pp; /* pointer to picture entry */

    if (frepic) { /* there is a free entry */

        pp = frepic; /* index top free */
        frepic = pp->next; /* gap out */

    } else { /* allocate new one */

        pp = imalloc(sizeof(pict)); /* get new file entry */
        piccnt++;
        pictot += sizeof(pict);

    }
    pp->xi = NULL; /* set no image */
    pp->next = NULL; /* set no next */

    return (pp); /* return entry */

}

/** ****************************************************************************

Put picture entry

Frees a picture entry.

*******************************************************************************/

static void putpic(picptr pp)

{

    pp->next = frepic; /* push to free list */
    frepic = pp;

}

/** ****************************************************************************

Delete picture entries

Deletes all the scaled copies of the picture by number. No error checking is
done.

*******************************************************************************/

static void delpic(winptr win, int p)

{

    picptr pp; /* pointer to picture entries */

    while (win->pictbl[p-1]) {

        /* remove top entry */
        pp = win->pictbl[p-1];
        win->pictbl[p-1] = pp->next;
        XWLOCK();
        XDestroyImage(pp->xi); /* release image */
        XWUNLOCK();
        putpic(pp); /* release image entry */

    }

}

/*******************************************************************************

Index window from logical file number

Finds the window associated with a logical file id. The file is checked
Finds the window associated with a text file. Gets the logical top level
filenumber for the file, converts this via its top to bottom alias,
validates that an alias has been established. This effectively means the file
was opened. , the window structure assigned to the file is fetched, and
validated. That means that the file was opened as a window input or output
file.

*******************************************************************************/

static winptr lfn2win(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (!opnfil[fn]) error(einvhan); /* invalid handle */
    if (!opnfil[fn]->win)
        error(efnotwin); /* not a window file */

    return (opnfil[fn]->win); /* return windows pointer */

}

/*******************************************************************************

Index window from file

Finds the window associated with a text file. Gets the logical top level
filenumber for the file, converts this via its top to bottom alias,
validates that an alias has been established. This effectively means the file
was opened. , the window structure assigned to the file is fetched, and
validated. That means that the file was opened as a window input or output
file.

*******************************************************************************/

static winptr txt2win(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file number */
    if (fn < 0) error(einvfil); /* file invalid */

    return (lfn2win(fn)); /* get logical filenumber for file */

}

/*******************************************************************************

Index window from logical window

Finds the windows context record from the logical window number, with checking.

*******************************************************************************/

static winptr lwn2win(int wid)

{

    int    ofn; /* output file handle */
    winptr win; /* window context pointer */

    if (wid < -MAXFIL || wid >= MAXFIL || !wid)  error(einvhan); /* error */
    ofn = xltwin[wid+MAXFIL]; /* get the output file handle */
    win = lfn2win(ofn); /* index window context */

    return (win); /* return result */

}

/*******************************************************************************

Get logical file number from file

Gets the logical translated file number from a text file, and verifies it
is valid.

*******************************************************************************/

static int txt2lfn(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file id */
    if (fn < 0) error(einvfil); /* invalid */

    return (fn); /* return result */

}

/*******************************************************************************

Get input side file from output file

Returns the input side file attached to the given output file.

*******************************************************************************/

static FILE* out2inp(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file id */
    if (fn < 0) error(einvfil); /* file invalid */
    if (!opnfil[fn]) error(einvhan); /* invalid handle */
    if (!opnfil[fn]->win) error(efnotwin); /* not a window file */
    if (opnfil[fn]->inl < 0) error(enoinps); /* no input side for this window */
    fn = opnfil[fn]->inl; /* link input side */
    if (!opnfil[fn]->sfp) error(enoinps); /* no input side for this window */

    return (opnfil[fn]->sfp); /* return that */

}

/*******************************************************************************

Rescale XWindows image

Rescales an image from the source to the destination. Both scaling up and down
are possible, as well as changing the aspect ratio. Uses bilinear interpolation.

*******************************************************************************/

void rescale(XImage* dp, XImage* sp)

{

    unsigned int px1, px2, px3, px4;
    int sx, sy, dx, dy;
    float xr, yr;
    int xd, yd;
    int b, r, g;
    unsigned int* src;
    unsigned int* dest;
    int si, di;

    xr = ((float)(sp->width-1))/dp->width; /* find scaling ratio x */
    yr = ((float)(sp->height-1))/dp->height; /* find scaling ratio y */
    src = (unsigned int*)(sp->data); /* index source pixmap */
    dest = (unsigned int*)(dp->data); /* index destination pixmap */
    di = 0; /* set destination index */

    /* copy and scale source to destination */
    for (dy = 0; dy < dp->height; dy++) {

        for (dx = 0; dx < dp->width; dx++) {

            sx = xr*dx; /* find source x location */
            sy = yr*dy; /* find source y location */
            xd = (xr*dx)-sx;
            yd = (yr*dy)-sy;
            si = (sy*sp->width+sx); /* find net source index */
            px1 = src[si]; /* get this pixel */
            px2 = src[si+1]; /* get right pixel */
            px3 = src[si+sp->width]; /* get down pixel */
            px4 = src[si+sp->width+1]; /* get down/right pixel */

            b = (px1&0xff)*(1-xd)*(1-yd)+(px2&0xff)*xd*(1-yd)+
                   (px3&0xff)*yd*(1-xd)+(px4&0xff)*xd*yd;

            g = ((px1>>8)&0xff)*(1-xd)*(1-yd)+((px2>>8)&0xff)*xd*(1-yd)+
                    ((px3>>8)&0xff)*yd*(1-xd)+((px4>>8)&0xff)*xd*yd;

            r = ((px1>>16)&0xff)*(1-xd)*(1-yd)+((px2>>16)&0xff)*xd*(1-yd)+
                  ((px3>>16)&0xff)*(yd)*(1-xd)+((px4>>16)&0xff)*xd*yd;

            dest[di++] = 0xff000000|r<<16&0xff0000|g<<8&0xff00|b;

        }

    }

}

/** ****************************************************************************

Convert Petit-Ami ratioed angles to XWindow 64ths angles

PA uses 0 to INT_MAX clockwise angular measurements. Xwindows uses 64 degree
measurements, with 0 degress at the 3'oclock position, and counterclockwise
drawing direction. Converts PA angles to XWindow angles.

*******************************************************************************/

#define DEGREE (INT_MAX/360)

int rat2a64(int a)

{

    /* normalize for 90 degrees origin */
    a -= INT_MAX/4; /* normalize for 90 degrees */
    if (a < 0) a += INT_MAX;
    a /= INT_MAX/(360*64); /* convert to 64ths degrees */
    if (a) a = 360*64-a;

    return (a); /* return counterclockwise */

}

/** ****************************************************************************

Get freed/new window structure

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static winptr getwin(void)

{

    winptr p;

    if (winfre) { /* there is a freed entry */

        p = winfre; /* index top entry */
        winfre = p->next; /* gap from list */

    } else {

        p = imalloc(sizeof(winrec));
        wincnt++; /* count entries */
        wintot += sizeof(winrec); /* add to total memory used */

    }

    return (p);

}

/** ****************************************************************************

Get freed/new window structure

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putwin(winptr p)

{

    p->next = winfre; /* push to list */
    winfre = p;

}

/** ****************************************************************************

Get freed/new queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static xevtque* getxevt(void)

{

    xevtque* p;

    if (freque) { /* there is a freed entry */

        p = freque; /* index top entry */
        freque = p->next; /* gap from list */

    } else p = (xevtque*)imalloc(sizeof(xevtque));

    return (p);

}

/** ****************************************************************************

Get freed/new queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putxevt(xevtque* p)

{

    p->next = freque; /* push to list */
    freque = p;

}

/** ****************************************************************************

Place XEvent into input queue

*******************************************************************************/

static void enquexevt(XEvent* e)

{

    xevtque* p;

    p = getxevt(); /* get a queue entry */
    memcpy(&p->evt, e, sizeof(XEvent)); /* copy event to queue entry */
    if (evtque) { /* there are entries in queue */

        /* we push TO next (current) and take FROM last (final) */
        p->next = evtque; /* link next to current entry */
        p->last = evtque->last; /* link last to final entry */
        evtque->last = p; /* link current to this */
        p->last->next = p; /* link final to this */
        evtque = p; /* point to new entry */

    } else { /* queue is empty */

        p->next = p; /* link to self */
        p->last = p;
        evtque = p; /* place in list */

    }

}

/** ****************************************************************************

Remove XEvent from input queue

*******************************************************************************/

static void dequexevt(XEvent* e)

{

    xevtque* p;

    if (!evtque) error(esystem); /* should not be called empty */
    /* we push TO next (current) and take FROM last (final) */
    p = evtque->last; /* index final entry */
    if (p->next == p) evtque = NULL; /* only one entry, clear list */
    else { /* other entries */

        p->last->next = p->next; /* point last at current */
        p->next->last = p->last; /* point current at last */

    }
    memcpy(e, &p->evt, sizeof(XEvent)); /* copy out to caller */
    putxevt(p); /* release queue entry to free */

}

/** ****************************************************************************

Find window output file number from XWindow handle

Search file table for XWindow handle and returns the table index corresponding,
or -1 if not found.

*******************************************************************************/

static int fndevt(Window w)

{

    int fi; /* index for file table */
    int ff; /* found file */

    fi = 0; /* start index */
    ff = -1; /* set no file found */
    while (fi < MAXFIL) {

        if (opnfil[fi] && opnfil[fi]->win &&
            (opnfil[fi]->win->xmwhan == w || opnfil[fi]->win->xwhan == w)) {

            ff = fi; /* set found */
            fi = MAXFIL; /* terminate */

        } else fi++; /* next entry */

    }

    return (ff);

}

/** ****************************************************************************

Wait response message

Looks into the XWindows events until a given respose is found. Events are placed
back into the input via a queue so that we don't discard important events like
expose.

Should have timeouts.

*******************************************************************************/

static void peekxevt(XEvent* e)

{

    XWLOCK();
    XNextEvent(padisplay, e); /* get next event */
    XWUNLOCK();
    enquexevt(e); /* place in input queue */
    /* there is another diagnostic in ami_event(), but you might want to see
       these events immediately */
    //dbg_printf(dlinfo, ""); prtxevt(e); fprintf(stderr, "\n"); fflush(stderr);

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static paevtque* getpaevt(void)

{

    paevtque* p;

    if (paqfre) { /* there is a freed entry */

        p = paqfre; /* index top entry */
        paqfre = p->next; /* gap from list */

    } else p = (paevtque*)imalloc(sizeof(paevtque));

    return (p);

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putpaevt(paevtque* p)

{

    p->next = paqfre; /* push to list */
    paqfre = p;

}

/** ****************************************************************************

Print contents of PA queue

A diagnostic, prints the contents of the PA queue.

*******************************************************************************/

static void prtquepaevt(void)

{

    paevtque* p;

    p = paqevt; /* index root entry */
    while (p) {

        prtevt(&p->evt); /* print this entry */
        fprintf(stderr, "\n"); fflush(stderr);
        p = p->next; /* link next */
        if (p == paqevt) p = NULL; /* end of queue, terminate */

    }

}

/** ****************************************************************************

Place PA event into input queue

*******************************************************************************/

static void enquepaevt(ami_evtrec* e)

{

    paevtque* p;

    p = getpaevt(); /* get a queue entry */
    memcpy(&p->evt, e, sizeof(ami_evtrec)); /* copy event to queue entry */
    if (paqevt) { /* there are entries in queue */

        /* we push TO next (current) and take FROM last (final) */
        p->next = paqevt; /* link next to current entry */
        p->last = paqevt->last; /* link last to final entry */
        paqevt->last = p; /* link current to this */
        p->last->next = p; /* link final to this */
        paqevt = p; /* point to new entry */

    } else { /* queue is empty */

        p->next = p; /* link to self */
        p->last = p;
        paqevt = p; /* place in list */

    }

}

/** ****************************************************************************

Remove PA event from input queue

*******************************************************************************/

static void dequepaevt(ami_evtrec* e)

{

    paevtque* p;

    if (!paqevt) error(esystem); /* should not be called empty */
    /* we push TO next (current) and take FROM last (final) */
    p = paqevt->last; /* index final entry */
    if (p->next == p) paqevt = NULL; /* only one entry, clear list */
    else { /* other entries */

        p->last->next = p->next; /* point last at current */
        p->next->last = p->last; /* point current at last */

    }
    memcpy(e, &p->evt, sizeof(ami_evtrec)); /* copy out to caller */
    putpaevt(p); /* release queue entry to free */

}

/** ****************************************************************************

Remove all matching window PA events from input queue

*******************************************************************************/

static void remquepawin(int winid)

{

    paevtque* p;
    paevtque* p2;

    p = paqevt; /* index root entry */
    while (p) {

        if (p->evt.winid == winid) {

            /* delete this entry */
            if (p == p->next) {

                paqevt = NULL; /* only one entry, clear queue */
                p2 = NULL; /* set no next */

            } else { /* list delete */

                p->last->next = p->next; /* point last at current */
                p->next->last = p->last; /* point current at last */
                p2 = p->next; /* go next */
                /* if deleting the root entry, skip to next */
                if (paqevt == p) paqevt = p->next;

            }
            putpaevt(p); /* release queue entry to free */
            p = p2; /* step to next */

        } else {

            p = p->next; /* index next */
            if (p == paqevt) p = NULL; /* wrapped around queue, stop */

        }

    }

}

/** ****************************************************************************

Send event to window

Send an event to the given window. The event is placed into the queue for the
given window. Note that the input side of the window is found, and the event
spooled for that side. Note that any window number given the event is
overwritten with the proper window id after a copy is made.

The difference between this and inserting to the event chain is that this
routine enters to the top of the chain, and specifies the input side of the
window. Thus it is a more complete send of the event.

*******************************************************************************/

static void isendevent(winptr win, ami_evtrec* er)

{

    ami_evtrec ec; /* copy of event record */

    memcpy(&ec, er, sizeof(ami_evtrec));
    ec.winid = win->wid; /* overwrite window id */
    enquepaevt(&ec); /* send to queue */

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

static int icurbnd(scnptr sc)

{

    return (sc->curx >= 1 && sc->curx <= sc->maxx &&
            sc->cury >= 1 && sc->cury <= sc->maxy);

}

/*******************************************************************************

Draw reversing cursor

Draws a cursor rectangle in xor mode. This is used both to place and remove the
cursor.

*******************************************************************************/

static void curdrw(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current update screen */
    XWLOCK();
    XSetForeground(padisplay, sc->xcxt, colnum(ami_white));
    XSetFunction(padisplay, sc->xcxt, GXxor); /* set reverse */
    if (win->focus)
        XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                       L2PX(win, sc->curxg-1), L2PY(win, sc->curyg-1),
                       L2PW(win, win->charspace), L2PH(win, win->linespace));
    else {

        XDrawRectangle(padisplay, win->xwhan, sc->xcxt,
                       L2PX(win, sc->curxg-1), L2PY(win, sc->curyg-1),
                       L2PW(win, win->charspace), L2PH(win, win->linespace));
        XDrawRectangle(padisplay, win->xwhan, sc->xcxt,
                       L2PX(win, sc->curxg-1+1), L2PY(win, sc->curyg-1+1),
                       L2PW(win, win->charspace-2), L2PH(win, win->linespace-2));

    }
    XSetFunction(padisplay, sc->xcxt, GXcopy); /* set overwrite */
    if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/*******************************************************************************

Set cursor visable

Makes the cursor visible.

*******************************************************************************/

static void curon(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curdsp-1]; /* index current screen */
    if (!win->fcurdwn && sc->curv && icurbnd(sc))  {

        /* cursor not already down, cursor visible, cursor in bounds, screen
           in focus */
        curdrw(win);
        win->fcurdwn = TRUE; /* set cursor on screen */

    }

}

/*******************************************************************************

Set cursor invisible

Makes the cursor invisible.

*******************************************************************************/

static void curoff(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curdsp-1]; /* index current screen */
    if (win->fcurdwn && sc->curv && icurbnd(sc))  {

        curdrw(win); /* remove cursor */
        win->fcurdwn = FALSE; /* set cursor not on screen */

    }

}

/*******************************************************************************

Set cursor status

Changes the current cursor status. If the cursor is out of bounds, or not
set as visible, it is set off. Otherwise, it is set on. Used to change status
of cursor after position and visible status events. Acts as a combination of
curon and curoff routines.

*******************************************************************************/

static void cursts(winptr win)

{

    int b;

    if (win->screens[win->curdsp-1]->curv &&
        icurbnd(win->screens[win->curdsp-1])) {

        /* cursor should be visible */
        if (!win->fcurdwn) { /* not already down */

            /* cursor not already down, cursor visible, cursor in bounds */
            curdrw(win); /* show cursor */
            win->fcurdwn = TRUE; /* set cursor on screen */

        }

    } else {

         /* cursor should not be visible */
        if (win->fcurdwn) { /* cursor visable */

            curdrw(win); /* remove cursor */
            win->fcurdwn = FALSE; /* set cursor not on screen */

        }

    }

}

/** ****************************************************************************

Update cursor for child frame mouse position

Sets the appropriate cursor shape on xmwhan based on which edge (if any) the
mouse is hovering over. Standard arrow when not on a resize edge, edge-specific
arrow on borders, diagonal arrow on corners.

*******************************************************************************/

static void childfrm_set_cursor(winptr win, int mx, int my)
{
    int mw = win->xmwr.w;
    int mh = win->xmwr.h;
    /* when minimized, all edges show the arrow cursor — no resizing */
    int on_left   = !win->minimized && (mx < CFRM_BORDER_W);
    int on_right  = !win->minimized && (mx >= mw - CFRM_BORDER_W);
    int on_top    = !win->minimized && (my < CFRM_BORDER_W);
    int on_bottom = !win->minimized && (my >= mh - CFRM_BORDER_W);
    Cursor c;

    /* lazy-init cursors on first use */
    if (!cfrm_cursor_arrow) {
        XWLOCK();
        cfrm_cursor_arrow    = XCreateFontCursor(padisplay, XC_left_ptr);
        cfrm_cursor_left     = XCreateFontCursor(padisplay, XC_left_side);
        cfrm_cursor_right    = XCreateFontCursor(padisplay, XC_right_side);
        cfrm_cursor_top      = XCreateFontCursor(padisplay, XC_top_side);
        cfrm_cursor_bottom   = XCreateFontCursor(padisplay, XC_bottom_side);
        cfrm_cursor_btmleft  = XCreateFontCursor(padisplay, XC_bottom_left_corner);
        cfrm_cursor_btmright = XCreateFontCursor(padisplay, XC_bottom_right_corner);
        cfrm_cursor_topleft  = XCreateFontCursor(padisplay, XC_top_left_corner);
        cfrm_cursor_topright = XCreateFontCursor(padisplay, XC_top_right_corner);
        XWUNLOCK();
    }

    if (on_left && on_top)          c = cfrm_cursor_topleft;
    else if (on_right && on_top)    c = cfrm_cursor_topright;
    else if (on_left && on_bottom)  c = cfrm_cursor_btmleft;
    else if (on_right && on_bottom) c = cfrm_cursor_btmright;
    else if (on_left)               c = cfrm_cursor_left;
    else if (on_right)              c = cfrm_cursor_right;
    else if (on_top)                c = cfrm_cursor_top;
    else if (on_bottom)             c = cfrm_cursor_bottom;
    else                            c = cfrm_cursor_arrow;

    XWLOCK();
    XDefineCursor(padisplay, win->xmwhan, c);
    XWUNLOCK();
}

/* forward declarations */
static void childfrm_draw(winptr win);
static void restore(winptr win);

/* Find the lowest unused minimize slot among siblings of win. The slot is the
   x position index for arranging minimized child windows along the bottom of
   the parent client area. */
static int childfrm_find_minslot(winptr win)
{

    winptr parent = win->parwin;
    winptr sib;
    int slot = 0;
    int found;

    if (!parent) return 0;
    do {
        found = 0;
        for (sib = parent->childwin; sib; sib = sib->childlst) {

            if (sib != win && sib->minimized && sib->minslot == slot) {

                slot++;
                found = 1;
                break;

            }

        }
    } while (found);
    return slot;

}

/* Compute the screen position of a minimized child window slot. The slot is
   placed along the bottom of the parent's client area. If the parent is too
   narrow to hold all minimized windows side by side, slots wrap back to the
   left and overlap previously placed minimized windows (Windows MDI style). */
static void childfrm_minslot_pos(winptr win, int slot, int* x, int* y)
{

    int min_w = CFRM_MIN_W;
    int min_h = CFRM_TITBAR_H(win);
    int parent_w, parent_h, slots_per_row;

    parent_w = win->parwin ? win->parwin->xwr.w : min_w;
    parent_h = win->parwin ? win->parwin->xwr.h : min_h;
    slots_per_row = parent_w / min_w;
    if (slots_per_row < 1) slots_per_row = 1;
    *x = (slot % slots_per_row) * min_w;
    *y = parent_h - min_h;

}

/* Minimize a child window: collapse it to title-bar-only at a slot along the
   bottom of the parent client area. Saves current geometry for later restore. */
static void childfrm_minimize(winptr win)
{

    int x, y, min_w, min_h;

    if (win->minimized || !win->childfrm) return;
    /* save current geometry */
    win->savxmwr = win->xmwr;
    win->savgmaxxg = win->gmaxxg;
    win->savgmaxyg = win->gmaxyg;
    /* assign a slot and compute its position */
    win->minslot = childfrm_find_minslot(win);
    childfrm_minslot_pos(win, win->minslot, &x, &y);
    min_w = CFRM_MIN_W;
    min_h = CFRM_TITBAR_H(win);
    win->xmwr.x = x;
    win->xmwr.y = y;
    win->xmwr.w = min_w;
    win->xmwr.h = min_h;
    win->minimized = TRUE;
    XWLOCK();
    XMoveResizeWindow(padisplay, win->xmwhan, x, y, min_w, min_h);
    /* hide the subclient — we only want to show the title bar */
    XUnmapWindow(padisplay, win->xwhan);
    XWUNLOCK();
    childfrm_draw(win);

}

/* Restore a minimized child window to its previous position and size. */
static void childfrm_restore(winptr win)
{

    if (!win->minimized || !win->childfrm) return;
    win->xmwr = win->savxmwr;
    win->gmaxxg = win->savgmaxxg;
    win->gmaxyg = win->savgmaxyg;
    win->minimized = FALSE;
    XWLOCK();
    XMoveResizeWindow(padisplay, win->xmwhan,
                      win->xmwr.x, win->xmwr.y,
                      win->xmwr.w, win->xmwr.h);
    XMapWindow(padisplay, win->xwhan);
    XWUNLOCK();
    restore(win);
    childfrm_draw(win);

}

/** ****************************************************************************

Draw child window frame

Renders the Ami-drawn frame chrome on xmwhan for child windows. This includes
the title bar, close button, and resize border. Called after restore() and on
Expose events for xmwhan.

*******************************************************************************/

static void childfrm_draw(winptr win)

{

    int mw, mh;       /* master window dimensions */
    int tbh;          /* title bar height */
    int bsz;          /* button diameter */
    int by;           /* button y position */
    int bx_close;     /* close button x */
    int bx_max;       /* maximize button x */
    int bx_min;       /* minimize button x */
    int tlen;         /* title text pixel length */
    int title_size;   /* title font pixel size */
    int corner_r;     /* outer corner radius */

    if (!win->childfrm || !win->frmgc || !win->linespace) return;
    if (!win->frame) return; /* frame is turned off — nothing to draw */

    /* Query actual geometry from the X server. XSync ensures any pending
       resize requests have been processed before we query. We cannot rely
       on xmwr — some code paths set it to the parent window's dimensions. */
    {
        Window root_ret;
        int rx, ry;
        unsigned int rw, rh, bw, dp;
        XWLOCK();
        XSync(padisplay, False);
        XGetGeometry(padisplay, win->xmwhan, &root_ret,
                     &rx, &ry, &rw, &rh, &bw, &dp);
        XWUNLOCK();
        mw = (int)rw;
        mh = (int)rh;
    }
    tbh = CFRM_TITBAR_H(win);
    bsz = CFRM_BUTTON_SZ(win);
    title_size = CFRM_TITLE_SZ(win);
    corner_r = tbh / 3;

    XWLOCK();

    /* Clear entire xmwhan to parent background (ParentRelative), then
       draw the frame shape. This ensures rounded corners show the parent
       through and no stale pixels remain after resize. */
    XClearWindow(padisplay, win->xmwhan);

    /* fill master frame background. Only the top-left and top-right corners
       are rounded; bottom corners remain square because they sit against the
       rectangular client area. We draw the body below the corner row, the
       top strip between corners, then full circles at each corner (the parts
       that extend below corner_r are hidden by the body fill). */
    XSetForeground(padisplay, win->frmgc, 0x303030); /* dark frame bg */

    /* body below the top corner row */
    XFillRectangle(padisplay, win->xmwhan, win->frmgc,
                   0, corner_r, mw, mh - corner_r);

    /* top strip between the two top corner caps */
    XFillRectangle(padisplay, win->xmwhan, win->frmgc,
                   corner_r, 0, mw - corner_r*2, corner_r);

    /* 2 top corner circles */
    XFillArc(padisplay, win->xmwhan, win->frmgc,
             0, 0, corner_r*2, corner_r*2, 0, 360*64);
    XFillArc(padisplay, win->xmwhan, win->frmgc,
             mw - corner_r*2, 0, corner_r*2, corner_r*2, 0, 360*64);

    /* draw title text - centered, using FreeType for native font rendering.
       If the title is too wide for the available space (between left edge and
       the leftmost button), truncate and append "..." */
    /* button row layout: minimize | maximize | close, right-aligned */
    by = (tbh - bsz) / 2;
    bx_close = mw - bsz - CFRM_BUTTON_MG;
    bx_max   = bx_close - bsz - CFRM_BUTTON_GAP;
    bx_min   = bx_max   - bsz - CFRM_BUTTON_GAP;

    if (win->wintitle && win->wintitle[0] && win->ftface) {

        int len = strlen(win->wintitle);
        int tleft = CFRM_BUTTON_MG; /* title left margin */
        int avail = bx_min - CFRM_BUTTON_GAP - tleft; /* space for title */
        tlen = ft_text_width(win->ftface, win->wintitle, len);
        int ty = (tbh + title_size) / 2 - 2;

        XSetForeground(padisplay, win->frmgc, 0xffffff);
        if (tlen <= avail) {

            /* title fits: center it in the available space */
            int tx = tleft + (avail - tlen) / 2;
            ft_draw_string(win->xmwhan, win->frmgc, win->ftface,
                           title_size, title_size,
                           tx, ty, win->wintitle, len);

        } else {

            /* truncate with "..." */
            int dotw = ft_text_width(win->ftface, "...", 3);
            if (avail > dotw) {

                int tw = 0;
                int tl;
                for (tl = 0; tl < len; tl++) {

                    int cw = 0;
                    if (FT_Load_Char(win->ftface,
                                     (unsigned char)win->wintitle[tl],
                                     FT_LOAD_DEFAULT) == 0)
                        cw = (int)(win->ftface->glyph->advance.x >> 6);
                    if (tw + cw + dotw > avail) break;
                    tw += cw;

                }
                ft_draw_string(win->xmwhan, win->frmgc, win->ftface,
                               title_size, title_size,
                               tleft, ty, win->wintitle, tl);
                ft_draw_string(win->xmwhan, win->frmgc, win->ftface,
                               title_size, title_size,
                               tleft + tw, ty, "...", 3);

            }

        }

    }

    {
        /* button colors depend on focus state */
        unsigned long btn_bg = 0x303030; /* min/max match title bar bg always */
        unsigned long btn_fg = win->focus ? 0xffffff : 0x808080;
        unsigned long cls_bg = win->focus ? 0xe04040 : 0x303030;

        /* draw minimize button (circle with horizontal line) */
        XSetForeground(padisplay, win->frmgc, btn_bg);
        XFillArc(padisplay, win->xmwhan, win->frmgc,
                 bx_min, by, bsz, bsz, 0, 360*64);
        XSetForeground(padisplay, win->frmgc, btn_fg);
        XSetLineAttributes(padisplay, win->frmgc, 2, LineSolid, CapButt, JoinMiter);
        XDrawLine(padisplay, win->xmwhan, win->frmgc,
                  bx_min + bsz/4, by + bsz - bsz/3,
                  bx_min + bsz - bsz/4, by + bsz - bsz/3);

        /* draw maximize button (circle with square outline) */
        XSetForeground(padisplay, win->frmgc, btn_bg);
        XFillArc(padisplay, win->xmwhan, win->frmgc,
                 bx_max, by, bsz, bsz, 0, 360*64);
        XSetForeground(padisplay, win->frmgc, btn_fg);
        XDrawRectangle(padisplay, win->xmwhan, win->frmgc,
                       bx_max + bsz/4, by + bsz/4,
                       bsz - bsz/2, bsz - bsz/2);

        /* draw close button (circle with X) */
        XSetForeground(padisplay, win->frmgc, cls_bg);
        XFillArc(padisplay, win->xmwhan, win->frmgc,
                 bx_close, by, bsz, bsz, 0, 360*64);
        XSetForeground(padisplay, win->frmgc, btn_fg);
        {
            int margin = bsz / 4;
            XDrawLine(padisplay, win->xmwhan, win->frmgc,
                      bx_close + margin, by + margin,
                      bx_close + bsz - margin - 1, by + bsz - margin - 1);
            XDrawLine(padisplay, win->xmwhan, win->frmgc,
                      bx_close + bsz - margin - 1, by + margin,
                      bx_close + margin, by + bsz - margin - 1);
        }
        XSetLineAttributes(padisplay, win->frmgc, 1, LineSolid, CapButt, JoinMiter);
    }

    /* Apply a shape mask that physically cuts the top corners out of the
       xmwhan window. This makes the rounded corners truly transparent —
       the parent window shows through those pixels. */
    {
        Pixmap mask = XCreatePixmap(padisplay, win->xmwhan, mw, mh, 1);
        GC mgc = XCreateGC(padisplay, mask, 0, NULL);
        /* fill mask with 0 (transparent everywhere) */
        XSetForeground(padisplay, mgc, 0);
        XFillRectangle(padisplay, mask, mgc, 0, 0, mw, mh);
        /* fill the opaque (visible) area with 1 */
        XSetForeground(padisplay, mgc, 1);
        /* body below top corner row */
        XFillRectangle(padisplay, mask, mgc, 0, corner_r, mw, mh - corner_r);
        /* top strip between corners */
        XFillRectangle(padisplay, mask, mgc,
                       corner_r, 0, mw - corner_r*2, corner_r);
        /* two top corner discs */
        XFillArc(padisplay, mask, mgc,
                 0, 0, corner_r*2, corner_r*2, 0, 360*64);
        XFillArc(padisplay, mask, mgc,
                 mw - corner_r*2, 0, corner_r*2, corner_r*2, 0, 360*64);
        XShapeCombineMask(padisplay, win->xmwhan, ShapeBounding, 0, 0,
                          mask, ShapeSet);
        XFreeGC(padisplay, mgc);
        XFreePixmap(padisplay, mask);
    }

    XFlush(padisplay);
    XWUNLOCK();

}

/** ****************************************************************************

Restore screen

Updates all the buffer and screen parameters from the display screen to the
terminal.

*******************************************************************************/

static void restore(winptr win) /* window to restore */

{

    int rgb;
    scnptr sc;
    int winw, winh; /* client area size */

    sc = win->screens[win->curdsp-1]; /* index screen */
    if (win->bufmod && win->visible)  { /* buffered mode is on, and visible */

        curoff(win); /* hide the cursor for drawing */
        /* set colors and attributes */
        if (BIT(sarev) & sc->attr)  { /* reverse */

            XWLOCK();
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();

        } else {

            XWLOCK();
            XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
            XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();

        }
        /* copy buffer to screen - ensure no clip mask interferes */
        XWLOCK();
        XSetClipMask(padisplay, sc->xcxt, None);
        XCopyArea(padisplay, sc->xbuf, win->xwhan, sc->xcxt, 0, 0,
                  sc->maxxg, sc->maxyg, 0, 0);

        /* if the buffer is smaller than the client area, clear the margins
           to the background color */
        winw = win->xwr.w;
        winh = win->xwr.h;
        if (sc->maxxg < winw || sc->maxyg < winh) {

            /* set background color as foreground for fill */
            if (BIT(sarev) & sc->attr)
                XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            else
                XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            /* right margin: from buffer right edge to window right edge,
               full height */
            if (sc->maxxg < winw)
                XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                               sc->maxxg, 0, winw - sc->maxxg, winh);
            /* bottom margin: from buffer bottom to window bottom,
               buffer width only (right strip already covered above) */
            if (sc->maxyg < winh)
                XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                               0, sc->maxyg, sc->maxxg, winh - sc->maxyg);
            /* restore foreground color */
            if (BIT(sarev) & sc->attr)
                XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            else
                XSetForeground(padisplay, sc->xcxt, sc->fcrgb);

        }
        XWUNLOCK();
        curon(win); /* show the cursor */

        /* redraw Ami-drawn child frame if applicable */
        if (win->childfrm) childfrm_draw(win);

    }

}

/*******************************************************************************

Create window without background draw

Creates a window with the given parent, width and height. Returns the Xwindow
handle. If "no window delay" flag is set, presents the window immediately.

*******************************************************************************/

/*******************************************************************************

Flush pending Xlib draw requests on the internal display connection.

External tools (e.g. the screen_capture module) that connect to the same X
server on their own connection cannot see draws that have been issued by
Petit-Ami but are still buffered inside its Xlib output queue. Calling
this function forces those requests to the server synchronously so a
subsequent XGetImage on any connection observes the up-to-date window.

*******************************************************************************/

void pa_xflush(void)
{
    if (padisplay) {
        XWLOCK();
        XSync(padisplay, False);
        XWUNLOCK();
    }
}

static Window createwindow(Window parent, int x, int y, int w, int h)

{

    Window wh; /* XWindow handle */
    XEvent e; /* XWindow event */

    /* create our window with no background */
    XWLOCK();
    wh = XCreateWindow(padisplay, parent, 0, 0, w, h, 0, CopyFromParent,
                      InputOutput, CopyFromParent, 0, NULL);

    /* advertise our PID so external tools (e.g. screen_capture) can find
       our windows reliably via EWMH _NET_WM_PID */
    {
        Atom pid_atom = XInternAtom(padisplay, "_NET_WM_PID", False);
        unsigned long pid_val = (unsigned long)getpid();
        XChangeProperty(padisplay, wh, pid_atom, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)&pid_val, 1);
    }

    /* select what events we want */
    XSelectInput(padisplay, wh, ExposureMask|KeyPressMask|
                 KeyReleaseMask|PointerMotionMask|ButtonPressMask|
                 ButtonReleaseMask|StructureNotifyMask|FocusChangeMask|
                 EnterWindowMask|LeaveWindowMask|PropertyChangeMask);
    XWUNLOCK();

/* now handled in winvis */
#ifdef NOWDELAY
    /* present the window onscreen */
    XWLOCK();
    XMapWindow(padisplay, wh);
    XFlush(padisplay);
    XWUNLOCK();

    /* wait for the window to be displayed */
    do { peekxevt(&e); } while (e.type !=  MapNotify || e.xany.window != wh);
#endif

    return (wh);

}

/*******************************************************************************

Display window

Presents a window, and sends it a first paint message. Used to process the
delayed window display function.

*******************************************************************************/

static void winvis(winptr win)

{

    XEvent e; /* XWindow event */

#ifndef NOWDELAY
   if (!win->visible) { /* not already visible */

        /* first make all parents visible */
        if (win->parwin) winvis(win->parwin);

        /* present the master window onscreen */
        XWLOCK();
        XMapWindow(padisplay, win->xmwhan);
        /* place */
        XMoveWindow(padisplay, win->xmwhan, win->xmwr.x, win->xmwr.y);
        XFlush(padisplay);
        XWUNLOCK();

        /* wait for the window to be displayed */
        do { peekxevt(&e);
        } while (e.type !=  MapNotify || e.xany.window != win->xmwhan);

        /* present the subclient window onscreen */
        XWLOCK();
        XMapWindow(padisplay, win->xwhan);
        XFlush(padisplay);
        XWUNLOCK();

        /* wait for the window to be displayed */
        do { peekxevt(&e); } while (e.type !=  MapNotify || e.xany.window != win->xwhan);

        win->visible = TRUE; /* set now visible */
        restore(win); /* restore window */

    }
#endif

}

/** ****************************************************************************

Initalize screen

Clears all the parameters in the present screen context. Also, the backing
buffer bitmap is created and cleared to the present colors.

*******************************************************************************/

static void iniscn(winptr win, scnptr sc)

{

    int      i, x;
    int      r;
    int      depth;

    sc->maxx = win->gmaxx; /* set character dimensions */
    sc->maxy = win->gmaxy;
    sc->maxxg = win->gmaxxg; /* set pixel dimensions */
    sc->maxyg = win->gmaxyg;
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    sc->angle = INT_MAX/4; /* set character draw at 90 degrees */
    sc->fcrgb = win->gfcrgb; /* set colors and attributes */
    sc->bcrgb = win->gbcrgb;
    sc->attr = win->gattr;
    sc->autof = win->gauto; /* set auto scroll and wrap */
    sc->curv = win->gcurv; /* set cursor visibility */
    sc->lwidth = 1; /* set single pixel width */
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

        sc->tab[x] = (i-1)*win->charspace+1;  /* set tab */
        i = i+8; /* next tab */
        x = x+1;

    }

    /* create graphics context for screen */
    XWLOCK();
    sc->xcxt = XCreateGC(padisplay, win->xwhan, 0, NULL);
    XWUNLOCK();

    /* set colors && attributes */
    if (BIT(sarev) & sc->attr) { /* reverse */

        XWLOCK();
        XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XWUNLOCK();

    } else {

        XWLOCK();
        XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        XWUNLOCK();

    }

    XWLOCK();
    /* set line attributes */
    XSetLineAttributes(padisplay, sc->xcxt, 1, LineSolid, CapButt, JoinMiter);

    /* set up pixmap backing buffer */
    depth = DefaultDepth(padisplay, pascreen);
    sc->xbuf = XCreatePixmap(padisplay, win->xwhan, sc->maxxg, sc->maxyg, depth);
    XWUNLOCK();

    /* save buffer size */
    win->bufx = win->gmaxx;
    win->bufy = win->gmaxy;
    win->bufxg = win->gmaxxg;
    win->bufyg = win->gmaxyg;

    /* clear it */
    clrbuf(sc);

#if 0
    /* draw grid for character cell diagnosis */
    XWLOCK();
    XSetForeground(padisplay, sc->xcxt, colnum(ami_cyan));
    for (y = 0; y < sc->maxyg; y += win->linespace)
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, 0, y, sc->maxxg, y);
    for (x = 0; x < sc->maxxg; x += win->charspace)
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x, 0, x, sc->maxyg);
    XSetForeground(padisplay, sc->xcxt, colnum(ami_black));
    XWUNLOCK();
#endif

#if 0
    /* reveal the background (diagnostic) */
    XWLOCK();
    XSetBackground(padisplay, sc->xcxt, colnum(ami_yellow));
    XWUNLOCK();
#endif

}

/*******************************************************************************

Disinitalize screen

Removes all data for the given screen buffer.

*******************************************************************************/

static void disscn(winptr win, scnptr sc)

{

    /* need to do disposals here */

}

/** ****************************************************************************

Open and present window

Given a windows file id and an (optional) parent window file id opens and
presents the window associated with it. All of the screen buffer data is
cleared, and a single buffer assigned to the window.

*******************************************************************************/

static void opnwin(int fn, int pfn, int wid, int subclient)

{

    int                  r;     /* result holder */
    int                  b;     /* int result holder */
    ami_evtrec            er;    /* event holding record */
    int                  ti;    /* index for repeat array */
    int                  pin;   /* index for loadable pictures array */
    int                  si;    /* index for current display screen */
    winptr               win;   /* window pointer */
    winptr               pwin;  /* parent window pointer */
    XSetWindowAttributes xwsa;  /* XWindow set attributes */
    XWindowAttributes    xwga, xpwga; /* XWindow get attributes */
    XEvent               e;     /* XWindow event */
    char                 buf[250];
    Window               pw, rw;
    Window*              cwl;
    unsigned             ncw;

    win = lfn2win(fn); /* get a pointer to the window */
    /* find parent */
    win->parlfn = pfn; /* set parent logical number */
    win->wid = wid; /* set window id */
    pwin = NULL; /* set no parent */
    if (pfn >= 0) pwin = lfn2win(pfn); /* index parent window */
    win->parwin = pwin; /* copy link to windows structure */
    win->childwin = NULL; /* clear the child window list */
    win->childlst = NULL; /* clear child member list pointer */
    if (pwin) { /* we have a parent, enter this child to the parent list */

        win->childlst = pwin->childwin; /* push to parent's child list */
        pwin->childwin = win;

    }
    win->mb1 = FALSE; /* set mouse as assumed no buttons down, at origin */
    win->mb2 = FALSE;
    win->mb3 = FALSE;
    win->mb4 = FALSE;
    win->mb5 = FALSE;
    win->mpx = 1;
    win->mpy = 1;
    win->mpxg = 1;
    win->mpyg = 1;
    win->nmb1 = FALSE;
    win->nmb2 = FALSE;
    win->nmb3 = FALSE;
    win->nmb4 = FALSE;
    win->nmb5 = FALSE;
    win->nmpx = 1;
    win->nmpy = 1;
    win->nmpxg = 1;
    win->nmpyg = 1;
    win->shift = FALSE; /* set no shift active */
    win->cntrl = FALSE; /* set no control active */
    win->fcurdwn = FALSE; /* set cursor is not down */
    win->focus = FALSE; /* set not in focus */
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
    win->metlst = NULL; /* clear menu tracking list */
    win->menu = NULL; /* set menu bar not active */
    win->frame = TRUE; /* set frame on */
    win->size = TRUE; /* set size bars on */
    win->sysbar = TRUE; /* set system bar on */
    win->sizests = 0; /* clear last size status word */
    /* clear timer array */
    for (ti = 0; ti < 10; ti++) win->timers[ti] = 0;
    win->frmsev = 0; /* clear frame timer */
    /* clear loadable pictures table */
    for (pin = 0; pin < MAXPIC; pin++) win->pictbl[pin] = NULL;
    /* clear the screen array */
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    win->screens[0] = imalloc(sizeof(scncon)); /* get the default screen */
    scncnt++;
    scntot += sizeof(scncon);
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set not visible */
    win->winstate = 0; /* set normal window */
    win->lwinstate = 0;
    win->childfrm = FALSE; /* set no Ami-drawn child frame */
    win->wintitle = NULL; /* no title */
    win->frmgc = 0; /* no frame GC */

    /* set up global buffer parameters */
    win->gmaxx = maxxd; /* character max dimensions */
    win->gmaxy = maxyd;
    win->gattr = 0; /* no attribute */
    win->gauto = TRUE; /* auto on */
    win->gfcrgb = colnum(ami_black); /*foreground black */
    win->gbcrgb = colnum(ami_white); /* background white */
    win->gcurv = TRUE; /* cursor visible */
    win->gfmod = mdnorm; /* set mix modes */
    win->gbmod = mdnorm;
    win->goffx = 0;  /* set 0 offset */
    win->goffy = 0;
    win->vsx = 1.0f; /* viewport scale starts at 1:1 */
    win->vsy = 1.0f;
    win->gwextx = 1; /* set 1:1 extents */
    win->gwexty = 1;
    win->gvextx = 1;
    win->gvexty = 1;

    win->xmwhan = 0; /* clear the XWindow handles */
    win->xwhan = 0;

    /* get screen parameters */
    XWLOCK();
    win->shsize = DisplayWidthMM(padisplay, pascreen); /* size x in millimeters */
    win->svsize = DisplayHeightMM(padisplay, pascreen); /* size y in millimeters */
    win->shres = DisplayWidth(padisplay, pascreen);
    win->svres = DisplayHeight(padisplay, pascreen);
    XWUNLOCK();
    win->sdpmx = win->shres*1000/win->shsize; /* find dots per meter x */
    win->sdpmy = win->svres*1000/win->svsize; /* find dots per meter y */

#ifdef PRTPWM
    dbg_printf(dlinfo, "Display width in pixels:  %d\n", win->shres);
    dbg_printf(dlinfo, "Display height in pixels: %d\n", win->svres);
    dbg_printf(dlinfo, "Display width in mm:      %d\n", win->shsize);
    dbg_printf(dlinfo, "Display height in mm:     %d\n", win->svsize);
    dbg_printf(dlinfo, "Dots per meter x:         %d\n", win->sdpmx);
    dbg_printf(dlinfo, "Dots per meter y:         %d\n", win->sdpmy);
#endif

    win->gcfont = fntlst; /* index terminal font entry */
    win->gfhigh = (int)(conpnt*POINT*win->sdpmy/1000); /* set font height */
    win->gfcellh = 0; /* 0 means: bootstrap from gfhigh on first setfnt */
    win->gfpoint = (float)conpnt*POINT/1000.0f; /* set initial point size */
    /* set parameters of missing font character */
    win->mischrx = win->gfhigh*MISCHRX; /* set size x */
    win->mischry = win->gfhigh*MISCHRY; /* set size y */
    win->misoffx = win->gfhigh*MISOFFX; /* set offset x */
    win->misoffy = win->gfhigh*MISOFFY; /* set offset x */
    win->ftface = NULL; /* clear current font face */
    setfnt(win); /* select font */

    /* set standard/reference font sizes */
    stdchrx = win->charspace;
    stdchry = win->linespace;

    /* set buffer size required for character spacing at default character grid
       size */
    win->gmaxxg = maxxd*win->charspace;
    win->gmaxyg = maxyd*win->linespace;

    /* set XWindow display origins sizes and sizes */
    win->xmwr.x = 0;
    win->xmwr.y = 0;
    win->xmwr.w = win->gmaxxg;
    win->xmwr.h = win->gmaxyg;

    win->xwr.x = 0;
    win->xwr.y = 0;
    win->xwr.w = win->gmaxxg;
    win->xwr.h = win->gmaxyg;

    /* set menu line spacing now, from our choosen font sized from the window.
       This then won't be reset by the client. */
    win->menuspcy = win->linespace+EXTRAMENUY;

    /* set parent window, either the given or the root window */
    if (pwin) { /* there is a parent window */

        if (subclient) pw = pwin->xwhan; /* make child of subclient */
        else pw = pwin->xmwhan; /* make child of master (menu components) */

    } else pw = RootWindow(padisplay, pascreen); /* root */

    /* create master window */
    win->xmwr.x = 0; /* set current rectangle */
    win->xmwr.y = 0;
    win->xmwr.w = win->gmaxxg+EXTSPC;
    win->xmwr.h = win->gmaxyg+EXTSPC;
    win->xmwhan = createwindow(pw, win->xmwr.x, win->xmwr.y,
                                 win->xmwr.w, win->xmwr.h);

    /* hook close event from windows manager */
    XWLOCK();
    win->delmsg = XInternAtom(padisplay, "WM_DELETE_WINDOW", FALSE);
    XSetWMProtocols(padisplay, win->xmwhan, &win->delmsg, 1);
    XWUNLOCK();

    /* set up child frame or WM frame parameters */
    if (pwin && subclient) {

        /* child window: Ami draws its own frame on xmwhan.
           Set background to ParentRelative so the rounded corner areas
           (not covered by the frame fill) show the parent through. */
        XWLOCK();
        XSetWindowBackgroundPixmap(padisplay, win->xmwhan, ParentRelative);
        XWUNLOCK();

        win->childfrm = TRUE;
        win->minimized = FALSE;
        win->pfw = CFRM_BORDER_W * 2;
        win->pfh = CFRM_TITBAR_H(win) + CFRM_BORDER_W;
        win->cwox = CFRM_BORDER_W;
        win->cwoy = CFRM_TITBAR_H(win);

        /* size master to include frame around client area */
        XWLOCK();
        XResizeWindow(padisplay, win->xmwhan,
                      win->gmaxxg + win->pfw,
                      win->gmaxyg + win->pfh);
        XWUNLOCK();
        win->xmwr.w = win->gmaxxg + win->pfw;
        win->xmwr.h = win->gmaxyg + win->pfh;

    } else {

        /* top-level window: WM provides the frame */
        win->childfrm = FALSE;
        win->pfw = frmextwdt[frmcfgall];
        win->pfh = frmexthgt[frmcfgall];
        win->cwox = frmoffx[frmcfgall];
        win->cwoy = frmoffy[frmcfgall];

    }

    /* create subclient window, offset by frame dimensions */
    win->xwr.x = win->childfrm ? win->cwox : 0;
    win->xwr.y = win->childfrm ? win->cwoy : 0;
    win->xwr.w = win->gmaxxg;
    win->xwr.h = win->gmaxyg;
    win->xwhan = createwindow(win->xmwhan, win->xwr.x, win->xwr.y,
                                           win->xwr.w, win->xwr.h);

    /* create GC for drawing on xmwhan (used for child frame rendering) */
    if (win->childfrm) {

        XWLOCK();
        win->frmgc = XCreateGC(padisplay, win->xmwhan, 0, NULL);
        XWUNLOCK();

    }

    /* set window title */
    XWLOCK();
    if (win->childfrm) {

        /* store title for Ami-drawn frame */
#if defined(__MACH__) || defined(__FreeBSD__)
        win->wintitle = strdup(getprogname());
#else
        win->wintitle = strdup(program_invocation_short_name);
#endif

    } else {

        /* set WM title for top-level windows */
#if defined(__MACH__) || defined(__FreeBSD__)
        XStoreName(padisplay, win->xmwhan, getprogname());
#else
        XStoreName(padisplay, win->xmwhan, program_invocation_short_name);
#endif

    }
    XWUNLOCK();

    iniscn(win, win->screens[0]); /* initalize screen buffer */
    restore(win); /* update to screen */

}

/*******************************************************************************

Close window

Shuts down, removes and releases a window.

*******************************************************************************/

static void clswin(int fn)

{

    int    r;   /* result holder */
    int    b;   /* int result holder */
    winptr win; /* window pointer */

    win = lfn2win(fn); /* get a pointer to the window */
    XWLOCK();
    /* destroy the window */
    XDestroyWindow(padisplay, win->xwhan);
    XDestroyWindow(padisplay, win->xmwhan);
    XWUNLOCK();

}

/*******************************************************************************

Close window

Closes an open window pair. Accepts an output window. The window is closed, and
the window and file handles are freed. The input file is freed only if no other
window also links it.

*******************************************************************************/

/* flush and close file */

static void clsfil(int fn)

{

    int    si; /* index for screens */
    filptr fp;

    fp = opnfil[fn];
    if (fp->win) { /* there is a window component */

        /* release all of the screen buffers */
        for (si = 0; si < MAXCON; si++)
            if (fp->win->screens[si]) ifree(fp->win->screens[si]);
        putwin(fp->win); /* release the window data */

    }
    fp->win = NULL; /* set end open */
    fp->inw = FALSE;
    fp->inl = -1;

}

static int inplnk(int fn)

{

    int fi; /* index for files */
    int fc; /* counter for files */

    fc = 0; /* clear count */
    for (fi = 0; fi < MAXFIL; fi++) /* traverse files */
        if (opnfil[fi]) /* entry is occupied */
            if (opnfil[fi]->inl == fn) fc++; /* count the file link */

    return (fc); /* return result */

}

/* remove window from child window list */
static void remchlwin(winptr par, winptr win)

{

    winptr p; /* pointer to window */
    winptr l; /* pointer to last */
    winptr f; /* found entry */

    p = par->childwin; /* index top of list */
    /* if top of list, link parent to next to gap it */
    if (p == win) par->childwin = p->childlst;
    else {

        f = NULL; /* set no entry found */
        while (p && !f) { /* traverse the list */

            if (p == win) {

                f = p; /* set entry found */
                p = NULL; /* terminate */

            } else {

                l = p; /* set last window */
                p = p->childlst; /* link next */

            }

        }
        if (!f) error(esystem); /* should have found the window */
        l->childlst = f->childlst; /* gap over entry */

    }

}

static void closewin(int ofn)

{

    int       ifn;  /* input file id */
    int       wid;  /* window id */
    winptr    win;  /* window data structure */
    winptr    pwin; /* parent window */
    ami_evtrec er;   /* PA event record */

    wid = filwin[ofn]; /* get window id */
    ifn = opnfil[ofn]->inl; /* get the input file link */
    win = lfn2win(ofn); /* get a pointer to the window */
    clswin(ofn); /* close the window */
    clsfil(ofn); /* flush and close output file */
    /* if no remaining links exist, flush and close input file */
    if (!inplnk(ifn)) clsfil(ifn);
    filwin[ofn] = -1; /* clear file to window translation */
    xltwin[wid+MAXFIL] = -1; /* clear window to file translation */
    remquepawin(wid); /* remove any pending PA queue entries */
    if (win->parwin) { /* is child window */

        if (win->focus) { /* child has focus */

            /* move focus to parent */
            pwin = win->parwin; /* index parent */
            er.etype = ami_etfocus; /* set focus event */
            isendevent(pwin, &er); /* send it */
            curoff(pwin); /* remove cursor */
            pwin->focus = TRUE; /* put focus */
            curon(pwin); /* replace cursor */

        }
        /* remove from parent tree */
        remchlwin(win->parwin, win);

    }

}

/** ****************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   int wid, int subclient)

{

    /* if output was never opened, create it now */
    if (!opnfil[ofn]) getfil(&opnfil[ofn]);
    /* if input was never opened, create it now */
    if (!opnfil[ifn]) getfil(&opnfil[ifn]);
    opnfil[ofn]->inl = ifn; /* link output to input */
    opnfil[ifn]->inw = TRUE; /* set input is window handler */
    /* set file descriptor locations (note this is only really used for input
       files */
    opnfil[ifn]->sfp = infile;
    opnfil[ofn]->sfp = outfile;
    /* now see if it has a window attached */
    if (!opnfil[ofn]->win) {

        /* Haven't already started the main input/output window, so allocate
           and start that. We tolerate multiple opens to the output file. */
        opnfil[ofn]->win = getwin();
        opnwin(ofn, pfn, wid, subclient); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid+MAXFIL] >= 0 && xltwin[wid+MAXFIL] != ofn)
        error(ewinuse); /* flag error */
    xltwin[wid+MAXFIL] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

}

/** ****************************************************************************

Get freed/new menu tracking entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static metptr getmet(void)

{

    metptr p;

    if (fremet) { /* there is a freed entry */

        p = fremet; /* index top entry */
        fremet = p->next; /* gap from list */

    } else {

        p = (metptr)imalloc(sizeof(metrec));
        metcnt++; /* count entries */
        mettot += sizeof(metrec); /* add to total memory used */

    }

    return (p);

}

/** ****************************************************************************

Get freed/new menu entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putmet(metptr p)

{

    p->next = fremet; /* push to list */
    fremet = p;

}

/*******************************************************************************

Find menu entry

Finds a menu entry by id. If the entry is not found, it generates an error.
If the entry exists more than once, it generates an error.

*******************************************************************************/

/* search subtree for meny entry */
static metptr fndmenu_tree(metptr mp, int id)

{

    metptr fp, fp2; /* found entry pointer */

    fp = NULL; /* set no entry found */
    while (mp) { /* traverse */

        if (mp->id == id) { /* found */

            if (fp) error(edupmen); /* menu entry is duplicated */
            fp = mp; /* set found entry */

        }
        fp2 = fndmenu_tree(mp->branch, id);
        if (fp2) { /* found in subtree */

            if (fp) error(edupmen); /* menu entry is duplicated */
            fp = fp2; /* set found entry */

        }
        mp = mp->next; /* next entry */

    }

    return (fp); /* return entry */

}

static metptr fndmenu(winptr win, int id)

{

    metptr fp; /* found entry pointer */

    fp = fndmenu_tree(win->metlst, id); /* find entry in list */
    if (!fp) error(emennf); /* no menu entry found with id */

    return (fp); /* return entry */

}

/** ****************************************************************************

Open menu item as window

The onscreen menu consists of a series of widgets representing menu items, each
in its own window. This routine opens a window at the given location on screen,
then prepares it to act as a widget. The actual drawing of the contents and
running event actions is left to the menu event handler.

The parent file can be NULL, in which case the menu entry is "free floating",
that is, an independent window on the desktop.

*******************************************************************************/

static void openmenu(
    /* input side */         FILE* f,
    /** parent */            FILE* p,
    /* rectangle */          int x1, int y1, int x2, int y2,
    /* menu tracker entry */ metptr mp
)

{

    mp->wid = ami_getwinid(); /* allocate a buried wid */
    iopenwin(&f, &mp->wf, p, mp->wid, FALSE); /* open widget window */
    mp->parent = p; /* set parent file */
    xltmnu[mp->wid+MAXFIL] = mp; /* set the tracking entry for window */
    ami_buffer(mp->wf, FALSE); /* turn off buffering */
    ami_frame(mp->wf, FALSE); /* turn off frame */
    ami_auto(mp->wf, FALSE); /* turn off auto */
    ami_curvis(mp->wf, FALSE); /* turn off cursor */
    ami_font(mp->wf, AMI_FONT_SIGN); /* set button font */
    ami_bold(mp->wf, TRUE); /* set bold */
    ami_setposg(mp->wf, x1, y1); /* place at position */
    ami_setsizg(mp->wf, x2-x1+1, y2-y1+1); /* set size */
    ami_binvis(mp->wf); /* no background write */

}

/** ****************************************************************************

Menu event handler

This routine is called as a plug-in to the event handler chain. The events for
a menu item widget are performed.

*******************************************************************************/

/* present pulldown menu */
static void pulmen(
    /** window containing menu */    FILE* f,
    /** menu containing branch */    metptr mp,
    /** location in client window */ int x, int y
)

{

    int    mw; /* cumulative width of submenu */
    metptr p;  /* pointer to menu entries */
    int    w;
    winptr win;
    int    fx, fy;
    int    wc;

    win = txt2win(f); /* index window structure */
    /* find cumulative width of branch entries */
    p = mp->branch; /* index first in list */
    mw = 0; /* set no width */
    wc = 0; /* clear window count */
    while (p) { /* traverse */

        w = ami_strsiz(f, p->title); /* get width this entry */
        if (w > mw) mw = w; /* find maximum */
        wc++; /* count windows */
        p = p->next; /* next in list */

    }
    /* pad to sides, 2 squares, left for select, right for branch arrows),
       plus extra padding */
    mw += win->menuspcy*2+win->menuspcy/2;
    /* present frame */
    openmenu(out2inp(f), mp->evtfil, x, y, x+mw+4-1, y+wc*win->menuspcy+4-1, mp->frame);
    /* present the branch list as children of the frame */
    p = mp->branch;
    fx = 3; /* set frame coordinates, upper left+2 */
    fy = 3;
    while (p) { /* traverse */

        /* open menu item */
        openmenu(out2inp(f), mp->frame->wf, fx, fy, fx+mw-1, fy+win->menuspcy-1, p);
        p->fx1 = x; /* set base location in client  */
        p->fy1 = y;
        p->fx2 = x+mw+4-1;
        p->fy2 = y+win->menuspcy-1;
        p = p->next; /* next entry */
        fy += win->menuspcy; /* next location */
        y += win->menuspcy;

    }

}

/* remove pulldown menus */
static void remmen(metptr mp)

{

    /* close any open entry in this chain */
    while (mp) {

        /* if window file is open, close it */
        if (mp->wf && !mp->prime) {

            fclose(mp->wf); /* close */
            mp->wf = NULL; /* clear file link */
            mp->pressed = FALSE; /* remove any press status */

        }
        /* and close any submenus */
        if (mp->branch) remmen(mp->branch);
        /* close frame, if exists */
        if (mp->branch) remmen(mp->frame);
        mp = mp->next; /* next menu entry */

    }

}

static void menu_release_all(metptr mp, metptr skip);

/* draw menu button title */
static void menu_draw(metptr mp)

{

    if (mp->title) { /* there is a title */

        if (mp->ena) ami_fcolor(mp->wf, ami_black);
        else ami_fcolorg(mp->wf, INT_MAX/256*150, INT_MAX/256*150,
                               INT_MAX/256*150);
        if (mp->prime)
            ami_cursorg(mp->wf, 1,
                       ami_maxyg(mp->wf)/2-ami_chrsizy(mp->wf)/2);
        else ami_cursorg(mp->wf, ami_maxyg(mp->wf),
                                ami_maxyg(mp->wf)/2-ami_chrsizy(mp->wf)/2);
        fprintf(mp->wf, "%s", mp->title); /* place button title */
        /* if selected and on/off highlighted, place checkmark */
        if (mp->select && !mp->prime) {

            ami_fcolor(mp->wf, ami_black);
            ami_linewidth(mp->wf, 4);
            ami_line(mp->wf, ami_maxyg(mp->wf)/4, ami_maxyg(mp->wf)/2,
                            ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3);
            ami_line(mp->wf, ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3,
                            ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/6, ami_maxyg(mp->wf)/3);
            ami_linewidth(mp->wf, 1);

        }

    }

}

/* handle menu button press */
static void menu_press(metptr mp)

{

    winptr par; /* window parent */
    int x, y;
    metptr fp;

    par = NULL; /* set no parent (floating menu) */
    if (mp->parent) par = txt2win(mp->parent); /* index parent window */
    /* process button press */
    mp->pressed = TRUE;
    ami_fcolorg(mp->wf, INT_MAX-INT_MAX/4, INT_MAX-INT_MAX/4, INT_MAX-INT_MAX/4);
    ami_frect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
    menu_draw(mp); /* draw menu title */
    /* draw underbar */
    ami_fcolorg(mp->wf, INT_MAX/256*233, INT_MAX/256*84, INT_MAX/256*32);
    ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-4, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
    /* if it is a branch, present floating menu */
    if (mp->branch) {

        if (mp->prime) {

            /* we are starting a pulldown tree, remove any others */
            remmen(mp->head); /* remove any other pulldowns */
            menu_release_all(mp->head, mp); /* release other menus */
            x = mp->fx1; /* find location of button bottom in client terms */
            y = mp->fy1+par->menuspcy;

        } else {

            x = mp->fx2+1; /* find location of button in client terms */
            y = mp->fy1;

        }
        pulmen(mp->wf, mp, x, y); /* present this pulldown */

    }

}

/* handle menu button release */
static void menu_release(metptr mp)

{

    mp->pressed = FALSE;
    ami_fcolor(mp->wf, ami_white);
    ami_frect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
    menu_draw(mp); /* draw menu title */
    ami_fcolorg(mp->wf,
               INT_MAX/256*223, INT_MAX/256*223, INT_MAX/256*223);
    ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-1,
                        ami_maxxg(mp->wf), ami_maxyg(mp->wf));

}

/* remove any top level menu press states */
static void menu_release_all(metptr mp, metptr skip)

{

    /* close any open entry in this chain */
    while (mp) {

        if (mp->pressed && mp != skip) menu_release(mp); /* release this menu */
        mp = mp->next; /* next menu entry */

    }

}

static void menu_event(ami_evtrec* ev)

{

    ami_evtrec er; /* outbound menu event */
    metptr    mp; /* tracking entry for meny entries */
    winptr    par; /* window parent */
    int x, y;

    /* if not our window, send it on */
    mp = xltmnu[ev->winid+MAXFIL]; /* get possible menu entry */
    if (!mp) menu_event_oeh(ev); /* pass on if not a menu entry */
    else { /* handle it here */

        par = NULL; /* set no parent (floating menu) */
        if (mp->parent) par = txt2win(mp->parent); /* index parent window */
        if (ev->etype == ami_etredraw) { /* redraw the window */

            /* color the background */
            ami_fcolor(mp->wf, ami_white);
            ami_frect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
            menu_draw(mp); /* draw menu title */
            if (mp->pressed) {

                /* draw underbar */
                ami_fcolorg(mp->wf, INT_MAX/256*233, INT_MAX/256*84, INT_MAX/256*32);
                ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-4, ami_maxxg(mp->wf), ami_maxyg(mp->wf));

            } else if (mp->prime) { /* draw divider line */

                ami_fcolorg(mp->wf,
                           INT_MAX/256*223, INT_MAX/256*223, INT_MAX/256*223);
                ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-1,
                                    ami_maxxg(mp->wf), ami_maxyg(mp->wf));

            }
            if (mp->frm) { /* box the frame */

                ami_fcolorg(mp->wf,
                           INT_MAX/256*150, INT_MAX/256*150, INT_MAX/256*150);
                ami_rect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
                ami_rect(mp->wf, 2, 2, ami_maxxg(mp->wf)-1, ami_maxyg(mp->wf)-1);

            }
            if (mp->bar && !mp->prime) { /* draw bar under */

                ami_fcolorg(mp->wf,
                           INT_MAX/256*150, INT_MAX/256*150, INT_MAX/256*150);
                ami_line(mp->wf, 1, ami_maxyg(mp->wf), ami_maxxg(mp->wf),
                        ami_maxyg(mp->wf));

            }
            if (!mp->prime && mp->branch && !mp->menubar) {

                ami_fcolor(mp->wf, ami_black);
                ami_ftriangle(mp->wf, ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)/3,
                                     ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/4, ami_maxyg(mp->wf)/2,
                                     ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3);

            }

        } else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

            /* mouse button 1 activation in window */
            if (!mp->menubar) { /* if not the menu bar */

                /* if button not pressed */
                if (!mp->pressed) menu_press(mp); /* process menu press */
                else if (mp->branch) {

                    /* second press on floating menu */
                    menu_release(mp); /* release the button */
                    remmen(mp->branch); /* remove floating menus */
                    remmen(mp->frame); /* remove frame */

                }

            }

            if (!mp->menubar && !mp->branch && mp->ena) { /* not a branch entry */

                /* send event back to parent window */
                er.etype = ami_etmenus; /* set button event */
                er.butid = mp->id; /* set id */
                ami_sendevent(mp->evtfil/*parent*/, &er); /* send the event to the parent */
                remmen(mp->head); /* remove all floating menus but bar */
                menu_release_all(mp->head, mp); /* remove all top level presses */

            }

        } else if (ev->etype == ami_etmoubd && ev->dmoubn == 1) {

            /* mouse button 1 deactivation in window */
            if (!mp->menubar && !mp->branch) /* not the menu bar and not branch */
                menu_release(mp); /* release menu button */

        }

    }

}

/** ****************************************************************************

Activate onscreen menu for window

Takes a window file. The onscreen menu for the window, if it exists, is
presented in the window. Only the top level menu items are presented. The lower
level menus are activated as needed by the menu event handler.

*******************************************************************************/

static void actmenu(FILE* f)

{

    winptr win; /* pointer to windows context */
    metptr mp;  /* menu pointer */
    int x;      /* running position of menu entries */
    int w;      /* width of menu face text */
    FILE* bf;   /* menu bar file */
    FILE* inf;  /* input file for output window */

    win = txt2win(f); /* get window context */
    inf = out2inp(f); /* get input file for window */
    /* open the menu bar */
    openmenu(inf, f, 1, 1, ami_maxxg(f), win->menuspcy, win->menu);
    bf = win->menu->wf; /* get the menu bar file */
    x = 1; /* set initial menu bar position */
    mp = win->metlst; /* index top of menu list */
    while (mp) { /* traverse top level list */

        /* find width of face text in menu bar terms */
        w = ami_strsiz(bf, mp->title);
        /* open menu item here */
        openmenu(inf, f, x, 1, x+w+EXTRAMENUX, win->menuspcy, mp);
        mp->fx1 = x; /* save rectangle */
        mp->fy1 = 1;
        mp->prime = TRUE; /* set is a prime (onscreen) menu */
        x = x+w+EXTRAMENUX; /* go next menu position */
        mp = mp->next; /* next top menu item */

    }

}

/** ****************************************************************************

Internal versions of external interface routines

These routines both isolate lower level details from higher level details and
also use internal data structures as parameters. This allows upper level
to call down to common functions without having to find the internal data
structures each time from a file handle.

Each internal routine has the same name as an external routine (without
coining), but with a leading "i".

*******************************************************************************/

/** ****************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void iclear(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index current update screen */
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    clrbuf(sc); /* clear screen buffer */
    if (indisp(win)) { /* also process to display */

        curoff(win); /* hide the cursor */
        XWLOCK();
        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XFillRectangle(padisplay, win->xwhan, sc->xcxt, 0, 0,
                                  sc->maxxg, sc->maxyg);
        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }

}

/** ****************************************************************************

Scroll screen

Scrolls the screen by deltas in any given direction. If the scroll would move
all content off the screen, the screen is simply blanked. Otherwise, we find the
section of the screen that would remain after the scroll, determine its source
and destination rectangles, and use a bitblt to move it.

One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

static void iscrollg(winptr win, int x, int y)

{

    int dx, dy, dw, dh; /* destination coordinates and sizes */
    int sx, sy, sw, sh; /* destination coordinates */
    struct { /* fill rectangle */

        int x, y; /* origin (left, top) */
        int w, h; /* width, height */

    } frx, fry; /* x fill, y fill */
    scnptr sc;  /* pointer to current screen */

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
        if (y >= 0)  { /* move up */

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

            sx = x; /* from x characters to the right */
            sw = sc->maxxg-x; /* width - x characters */
            dx = 0; /* move to left side */
            /* set fill x character collums at right */
            frx.x = sc->maxxg-x;
            frx.w = x;
            frx.y = 0;
            frx.h = sc->maxyg-1;

        } else { /* move right */

            sx = 0; /* from x left */
            sw = sc->maxxg-abs(x); /* width - x characters */
            dx = abs(x); /* move from left side */
            /* set fill x character collums at left */
            frx.x = 0;
            frx.w = abs(x);
            frx.y = 0;
            frx.h = sc->maxyg-1;

        }
        if (win->bufmod) { /* apply to buffer */

            XWLOCK();
            XCopyArea(padisplay, sc->xbuf, sc->xbuf, sc->xcxt,
                      sx, sy, sw, sh, dx, dy);
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            /* fill vacated x */
            if (x) XFillRectangle(padisplay, sc->xbuf, sc->xcxt, frx.x, frx.y,
                                  frx.w, frx.h);
            /* fill vacated y */
            if (y) XFillRectangle(padisplay, sc->xbuf, sc->xcxt, fry.x, fry.y,
                                  fry.w, fry.h);
            XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();

        } else { /* scroll on screen */

            curoff(win); /* hide the cursor for drawing */
            XWLOCK();
            XCopyArea(padisplay, win->xwhan, win->xwhan, sc->xcxt,
                      sx, sy, sw, sh, dx, dy);
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            /* fill vacated x */
            if (x) XFillRectangle(padisplay, win->xwhan, sc->xcxt, frx.x, frx.y,
                                  frx.w, frx.h);
            /* fill vacated y */
            if (y) XFillRectangle(padisplay, win->xwhan, sc->xcxt, fry.x, fry.y,
                                  fry.w, fry.h);
            XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }

    }
    if (indisp(win) && win->bufmod)
        restore(win); /* move buffer to screen */

}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(winptr win, int x, int y)

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

/** ****************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

*******************************************************************************/

static void icursorg(winptr win, int x, int y)

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

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

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

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line. If the cursor is at screen top, and auto
is on, the screen is scrolled up, meaning that the screen contents are moved
down a line of text. If auto is off, the cursor can simply continue into
negative space as long as it stays within the bounds -INT_MAX to INT_MAX.

*******************************************************************************/

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
    else if (sc->cury > -INT_MAX) {

        curoff(win); /* hide the cursor */
        sc->cury--; /* set new position */
        sc->curyg -= win->linespace;
        curon(win); /* show the cursor */

    }

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line. If the cursor is at screen bottom, and
auto is on, the screen is scrolled down, meaning that the screen contents are
moved up a line of text. If auto is off, the cursor can simply continue into
undrawn space as long as it stays within the bounds of -INT_MAX to INT_MAX.

*******************************************************************************/

static void idown(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    /* check not bottom of screen */
    if (sc->cury < sc->maxy) {

        curoff(win); /* hide the cursor */
        sc->cury++; /* update position */
        sc->curyg += win->linespace+win->chrspcy; /* move to next character line */
        curon(win); /* show the cursor */

    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, +1*win->linespace); /* scroll down */
    else if (sc->cury < INT_MAX) {

        curoff(win); /* hide the cursor */
        sc->cury++; /* set new position */
        sc->curyg += win->linespace+win->chrspcy; /* move to next text line */
        curon(win); /* show the cursor */

    }

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

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
            sc->curxg = sc->maxxg-win->charspace;
            curon(win); /* show the cursor */

        } else {

            /* check won't overflow */
            if (sc->curx > -INT_MAX) {

                curoff(win); /* hide the cursor */
                sc->curx--; /* update position */
                sc->curxg -= win->charspace;
                curon(win); /* show the cursor */

            }

        }

    }

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

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

            idown(win); /* move cursor up one line */
            curoff(win); /* hide the cursor */
            sc->curx = 1; /* set cursor to extreme left */
            sc->curxg = 1;
            curon(win); /* show the cursor */

        /* check won't overflow */
        } else {

            if (sc->curx < INT_MAX) {

                curoff(win); /* hide the cursor */
                sc->curx++; /* update position */
                sc->curxg += win->charspace;
                curon(win); /* show the cursor */

            }

        }

    }

}

/** ****************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

static void itab(winptr win)

{

    int i;
    int x;
    scnptr sc;

    sc = win->screens[win->curupd-1];
    curoff(win); /* hide the cursor */
    /* first, find if next tab even exists */
    x = sc->curxg+1; /* get just after the current x position */
    if (x < 1) x = 1; /* don"t bother to search to left of screen */
    /* find tab || } of screen */
    i = 0; /* set 1st tab position */
    while (x > sc->tab[i] && sc->tab[i] && i < MAXTAB && x < sc->maxxg) i++;
    if (sc->tab[i] && x < sc->tab[i]) { /* not off right of tabs */

       sc->curxg = sc->tab[i]; /* set position to that tab */
       sc->curx = sc->curxg/win->charspace+1;

    }
    curon(win); /* show the cursor */

}

/*******************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

static void isettabg(winptr win, int t)

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
    if (t != sc->tab[i])  { /* not the same tab yet again */

        if (sc->tab[MAXTAB-1]) error(etabful); /* tab table full */
        /* move tabs above us up */
        for (x = MAXTAB-1; x > i; x--) sc->tab[x] = sc->tab[x-1];
        sc->tab[i] = t; /* place tab in order */

    }

}

/*******************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

static void irestabg(winptr win, int t)

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

/** ****************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and end of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up       Scroll down
down     Scroll up
right    Line down, start at left
left     Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

*******************************************************************************/

static void iauto(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    /* check we are transitioning to auto mode */
    if (e) {

        /* check display is on grid and in bounds. Note: parentheses are
           required because % binds tighter than -. The cursor graphical
           position must land on a character cell boundary for x and y. */
        if ((sc->curxg-1) % win->charspace) error(eatoofg);
        if ((sc->curyg-1) % win->linespace) error(eatoofg);
        if (sc->angle != INT_MAX/4) error(eatoang);
        if (!icurbnd(sc)) error(eatoecb);

    }
    sc->autof = e; /* set auto status */
    win->gauto = e;

}

/** ****************************************************************************

Place character with foreground and background 90 degrees

Places the given character with selected background and foreground. Accepts the
current window, current screen, character spacing, a flag indicating the
character exists in the current font, and the window to draw on.

This routine handles the special case of a 90 degree draw, which means normal
XWindows text.

*******************************************************************************/

static void drwchr90(winptr win, scnptr sc, int cs, int ce, Drawable d, char c)

{

    /* transform position and sizes to physical pixels for viewport scaling */
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

        XWLOCK();
        /* set background function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
        XSetFillStyle(padisplay, sc->xcxt, FillSolid); /* ensure solid fill */
        /* set background to foreground to draw character background */
        if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XFillRectangle(padisplay, d, sc->xcxt, px, py, pcs, pls);
        /* xor is non-destructive, and we can restore it. And and or are
           destructive, and would require a combining buffer to perform */
        if (sc->bmod == mdxor) {

            if (ce) /* character exists */
                /* draw character */
                ft_draw_char(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                             px, py+pbo, c);
            else /* does not exist, draw missing character box */
                XDrawRectangle(padisplay, d, sc->xcxt,
                               px+pmox, py+pmoy, pmx, pmy);

        }
        /* restore colors */
        if (BIT(sarev) & sc->attr)
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        /* reset background function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }
    if (sc->fmod != mdinvis) {

        XWLOCK();
        /* set foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        if (ce) /* character exists */
            /* draw character */
            ft_draw_char(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                         px, py+pbo, c);
        else /* does not exist, draw missing character box */
            XDrawRectangle(padisplay, d, sc->xcxt,
                           px+pmox, py+pmoy, pmx, pmy);
        /* check draw underline */
        if (sc->attr & BIT(saundl)){

            /* double line, may need ajusting for low DP displays */
            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+pbo+1, px+pcs, py+pbo+1);
            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+pbo+2, px+pcs, py+pbo+2);

        }

        /* check draw strikeout */
        if (sc->attr & BIT(sastkout)) {

            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+(int)(pbo/STRIKE),
                      px+pcs, py+(int)(pbo/STRIKE));
            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+(int)(pbo/STRIKE)+1,
                      px+pcs, py+(int)(pbo/STRIKE)+1);

        }
        /* reset foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Add vector to position

Adds a vector specified by angle and length to an x-y position and returns that.
The angle for the vector is using PA 12'oclock INT_MAX ratioed angles clockwise.

*******************************************************************************/

static void addvect(int* x, int* y, float a, int l)

{

    *x += round(sin(a)*l);
    *y -= round(cos(a)*l);

}

/** ****************************************************************************

Draw rotated rectangle

Draws a rectangle rotated by an angle.

*******************************************************************************/

/* convert PA INT_MAX ratio angle to RADIAN measure */
#define RADIAN(a) ((double)2*M_PI/INT_MAX*a)

static void drwrecta(Drawable d, scnptr sc, int a, int x, int y, int w, int h)

{

    int x1, x2, x3, x4;
    int y1, y2, y3, y4;
    int c;
    double ac;

    x1 = x2 = x3 = x4= x;
    y1 = y2 = y3 = y4= y;
    addvect(&x2, &y2, RADIAN(a), w);
    addvect(&x4, &y4, RADIAN(a)+2*M_PI/4, h);
    c = sqrt((double)w*w+(double)h*h);
    ac = atan((double)h/w);
    addvect(&x3, &y3, ac+RADIAN(a), c);
    XDrawLine(padisplay, d, sc->xcxt, x1, y1, x2, y2);
    XDrawLine(padisplay, d, sc->xcxt, x2, y2, x3, y3);
    XDrawLine(padisplay, d, sc->xcxt, x3, y3, x4, y4);
    XDrawLine(padisplay, d, sc->xcxt, x4, y4, x1, y1);

}

/** ****************************************************************************

Draw filled rotated rectangle

Draws a filled rectangle rotated by an angle.

*******************************************************************************/

static void drwfrecta(Drawable d, scnptr sc, int a, int x, int y, int w, int h)

{

    int x1, x2, x3, x4;
    int y1, y2, y3, y4;
    int c;
    double ac;
    XPoint xp[4];

    x1 = x2 = x3 = x4= x;
    y1 = y2 = y3 = y4= y;
    addvect(&x2, &y2, RADIAN(a), w);
    addvect(&x4, &y4, RADIAN(a)+2*M_PI/4, h);
    c = sqrt((double)w*w+(double)h*h);
    ac = atan((double)h/w);
    addvect(&x3, &y3, ac+RADIAN(a), c);
    xp[0].x = x1;
    xp[0].y = y1;
    xp[1].x = x2;
    xp[1].y = y2;
    xp[2].x = x3;
    xp[2].y = y3;
    xp[3].x = x4;
    xp[3].y = y4;
    XFillPolygon(padisplay, d, sc->xcxt, xp, 4, Nonconvex, CoordModeOrigin);

}

/** ****************************************************************************

Place character with foreground and background

Places the given character with selected background and foreground. Accepts the
current window, current screen, character spacing, a flag indicating the
character exists in the current font, and the window to draw on.

This routine handles drawing at any angle.

*******************************************************************************/

/* Convert PA angle notation to XRot notation, 360 degrees 0 at right */
#define ROTANGLE(a) (360-(double)a*360/INT_MAX+90)

static void drwchr(winptr win, scnptr sc, int cs, int ce, Drawable d, char c)

{

    char cb[2];      /* character buffer */
    int  xb, yb;     /* rotated baseline */
    int  xull, yull, xulr, yulr; /* underline 1 */
    int  xsol, ysol, xsor, ysor; /* underline 1 */

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

    cb[0] = c; /* place character in string form */
    cb[1] = 0;
    if (sc->bmod != mdinvis) { /* background is visible */

        XWLOCK();
        /* set background function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
        /* set background to foreground to draw character background */
        if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        drwfrecta(d, sc, sc->angle, px, py, pcs, pls);
        /* xor is non-destructive, and we can restore it. And and or are
           destructive, and would require a combining buffer to perform */
        if (sc->bmod == mdxor) {

            if (ce) /* character exists */
                /* draw character */
                ft_draw_char_rotated(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                                     RADIAN(sc->angle), xb, yb, c);
            else /* does not exist, draw missing character box */
                drwrecta(d, sc, sc->angle,
                         px+pmox, py+pmoy, pmx, pmy);

        }
        /* restore colors */
        if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        /* reset background function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }
    if (sc->fmod != mdinvis) {

        XWLOCK();
        /* set foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        if (ce) /* character exists */
            /* draw character */
            ft_draw_char_rotated(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                                 RADIAN(sc->angle), xb, yb, c);
        else /* does not exist, draw missing character box */
            drwrecta(d, sc, sc->angle,
                     px+pmox, py+pmoy, pmx, pmy);
        /* check draw underline */
        if (sc->attr & BIT(saundl)){

            /* double line, may need ajusting for low DP displays */
            XSetLineAttributes(padisplay, sc->xcxt, 2, LineSolid, CapButt, JoinMiter);
            XDrawLine(padisplay, d, sc->xcxt, xull, yull, xulr, yulr);
            XSetLineAttributes(padisplay, sc->xcxt, sc->lwidth, LineSolid, CapButt,
                               JoinMiter);

        }

        /* check draw strikeout */
        if (sc->attr & BIT(sastkout)) {

            XSetLineAttributes(padisplay, sc->xcxt, 2, LineSolid, CapButt, JoinMiter);
            XDrawLine(padisplay, d, sc->xcxt, xsol, ysol, xsor, ysor);
            XSetLineAttributes(padisplay, sc->xcxt, sc->lwidth, LineSolid, CapButt,
                               JoinMiter);

        }
        /* reset foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attribute.

We handle some elementary control codes here, like newline, backspace and form
feed. However, the idea is not to provide a parallel set of screen controls.
That's what the API is for.

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
            if (sc->angle == INT_MAX/4) drwchr90(win, sc, cs, ce, sc->xbuf, c);
            else drwchr(win, sc, cs, ce, sc->xbuf, c);

        }
        if (indisp(win)) { /* do it again for the current screen */

            curoff(win); /* hide the cursor */
            /* draw character to active screen */
            if (sc->angle == INT_MAX/4) drwchr90(win, sc, cs, ce, win->xwhan, c);
            else drwchr(win, sc, cs, ce, win->xwhan, c);
            curon(win); /* show the cursor */

        }
        /* advance to next character */
        if (sc->angle == INT_MAX/4) {

            if (sc->cfont->fix) iright(win); /* move cursor right character */
            else { /* perform proportional version */

                if (indisp(win)) curoff(win); /* remove cursor */
                sc->curxg += cs; /* advance the character width */
                /* the cursor x position really has no meaning with proportional
                   but we recalculate it using space anyways. */
                sc->curx = sc->curxg/win->charspace+1;
                if (indisp(win)) curon(win); /* set cursor on screen */

            }

        } else  { /* arbitrary angle */

            /* if fixed font use fixed width of character */
            if (sc->cfont->fix) cs = win->charspace;
            if (indisp(win)) curoff(win); /* remove cursor */
            /* find next position */
            addvect(&sc->curxg, &sc->curyg, RADIAN(sc->angle), cs);
            /* the cursor position really has no meaning with proportional
               and off-angle but we recalculate it using space anyways. */
            sc->curx = sc->curxg/win->charspace+1;
            sc->cury = sc->curyg/win->linespace+1;
            if (indisp(win)) curon(win); /* set cursor on screen */

        }

    }

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into the
input line buffer.

*******************************************************************************/

static void readline(int fd)

{

    ami_evtrec er;   /* event record */
    scnptr    sc;   /* pointer to current screen */
    winptr    win;  /* window pointer */
    int       ins;  /* insert/overwrite mode */
    int       xoff; /* x starting line offset */
    int       l;    /* buffer length */
    int       ofn;  /* logical output file */
    int       lcmp; /* line complete */
    int       i;

    lcmp = FALSE; /* set line not complete */
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
                xoff = sc->curx; /* save starting line offset */

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
                            if (!win->inpbuf[win->inpptr]) win->inpbuf[win->inpptr+1] = 0;
                            win->inpbuf[win->inpptr] = er.echar; /* place new character */
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
                        while (win->inpbuf[i]) { win->inpbuf[i] = win->inpbuf[i+1]; i++; }
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
                        while (win->inpbuf[i]) { win->inpbuf[i] = win->inpbuf[i+1]; i++; }
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

                case ami_etmoumov: /* mouse moved */
                    /* we can track this internally */
                    break;

                case ami_etmouba: /* mouse click */
                    if (er.amoubn == 1) {

                        l = strlen(win->inpbuf);
                        if (sc->cury == win->mpy && xoff <= win->mpx && xoff+l >= win->mpx) {

                            /* mouse position is within buffer space, set
                               position */
                            icursor(win, win->mpx, sc->cury);
                            win->inpptr = win->mpx-xoff;

                        }

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
                    while (win->inpbuf[win->inpptr] && win->inpbuf[win->inpptr] != ' ') {

                        plcchr(win, win->inpbuf[win->inpptr]);
                        win->inpptr++;

                    }
                    /* advance over any spaces */
                    while (win->inpbuf[win->inpptr] && win->inpbuf[win->inpptr] == ' ') {

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

/** ****************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
lseek

We use interdiction to filter standard I/O calls towards the terminal. The
0 (input) and 1 (output) files are interdicted. In ANSI terminal, we act as a
filter, so this does not change the user ability to redirect the file handles
elsewhere.

*******************************************************************************/

/** ****************************************************************************

Read

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
            /* read input bytes */

            /* find any window with a buffer with data that points to this input
               file */
            ofn = fndful(fd);
            if (ofn == -1) readline(fd); /* none, read a buffer */
            else { /* read characters */

                win = lfn2win(ofn); /* get the window */
                while (win->inpbuf[win->inpptr] && l) {

                    /* there is data in the buffer, and we need that data */
                    *ba = win->inpbuf[win->inpptr]; /* get and place next character */
                    if (win->inpptr < MAXLIN) win->inpptr++; /* next */
                    /* if we have just read the last of that line, flag buffer
                       empty */
                    if (*ba == '\n') {

                        win->inpptr = -1;
                        win->inpbuf[0] = 0;

                    }
                    l--; /* count characters */

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

/** ****************************************************************************

Write

*******************************************************************************/

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

/** ****************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

*******************************************************************************/

static int ivopen(popen_t opendc, const char* pathname, int flags, int perm)

{

    return (*opendc)(pathname, flags, perm);

}

static int iopen(const char* pathname, int flags, int perm)

{

    return ivopen(ofpopen, pathname, flags, perm);

}

static int iopen_nocancel(const char* pathname, int flags, int perm)

{

    return ivopen(ofpopen_nocancel, pathname, flags, perm);

}

/** ****************************************************************************

Close

If the file is attached to an output window, closes the window file. Otherwise,
the close is just passed on.

*******************************************************************************/

static int ivclose(pclose_t closedc, int fd)

{

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    /* check if the file is an output window, and close if so */
    if (opnfil[fd] && opnfil[fd]->win) closewin(fd);

    return (*closedc)(fd);

}

static int iclose(int fd)

{

    return ivclose(ofpclose, fd);

}

static int iclose_nocancel(int fd)

{

    return ivclose(ofpclose_nocancel, fd);

}

/** ****************************************************************************

Lseek

Lseek is never possible on a terminal, so this is always an error on the stdin
or stdout handle.

*******************************************************************************/

static off_t ivlseek(plseek_t lseekdc, int fd, off_t offset, int whence)

{

    /* check seeking on terminal attached file (input or output) and error
       if so */
    if (fd == INPFIL || fd == OUTFIL) error(efilopr);

    return (*lseekdc)(fd, offset, whence);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    return ivlseek(ofplseek, fd, offset, whence);

}

/** ****************************************************************************

External interface routines

These routines all have override capability.

*******************************************************************************/

/** ****************************************************************************

Scroll screen

Scrolls the terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

void _pa_scrollg_ovr(ami_scrollg_t nfp, ami_scrollg_t* ofp)
    { *ofp = scrollg_vect; scrollg_vect = nfp; }
void ami_scrollg(FILE* f, int x, int y) { (*scrollg_vect)(f, x, y); }

static void scrollg_ivf(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iscrollg(win, x, y); /* process */

}

void _pa_scroll_ovr(ami_scroll_t nfp, ami_scroll_t* ofp)
    { *ofp = scroll_vect; scroll_vect = nfp; }
void ami_scroll(FILE* f, int x, int y) { (*scroll_vect)(f, x, y); }

static void scroll_ivf(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    iscrollg(win, x*win->charspace, y*win->linespace); /* process scroll */

}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void _pa_cursor_ovr(ami_cursor_t nfp, ami_cursor_t* ofp)
    { *ofp = cursor_vect; cursor_vect = nfp; }
void ami_cursor(FILE* f, int x, int y) { (*cursor_vect)(f, x, y); }

static void cursor_ivf(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    icursor(win, x, y); /* process */

}

/** ****************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

*******************************************************************************/

void _pa_cursorg_ovr(ami_cursorg_t nfp, ami_cursorg_t* ofp)
    { *ofp = cursorg_vect; cursorg_vect = nfp; }
void ami_cursorg(FILE* f, int x, int y) { (*cursorg_vect)(f, x, y); }

static void cursorg_ivf(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    icursorg(win, x, y); /* process */

}

/** ****************************************************************************

Find character baseline

Returns the offset, from the top of the current fonts character bounding box,
to the font baseline. The baseline is the line all characters rest on.

*******************************************************************************/

void _pa_baseline_ovr(ami_baseline_t nfp, ami_baseline_t* ofp)
    { *ofp = baseline_vect; baseline_vect = nfp; }
int ami_baseline(FILE* f) { return ((*baseline_vect)(f)); }

static int baseline_ivf(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    win = txt2win(f); /* get window from file */
    r = win->baseoff; /* return current line spacing */

    return (r);

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

void _pa_maxx_ovr(ami_maxx_t nfp, ami_maxx_t* ofp)
    { *ofp = maxx_vect; maxx_vect = nfp; }
int ami_maxx(FILE* f) { return ((*maxx_vect)(f)); }

static int maxx_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxx);

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

void _pa_maxy_ovr(ami_maxy_t nfp, ami_maxy_t* ofp)
    { *ofp = maxy_vect; maxy_vect = nfp; }
int ami_maxy(FILE* f) { return ((*maxy_vect)(f)); }

static int maxy_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxy);

}

/** ****************************************************************************

Return maximum x dimension graphical

Returns the maximum x dimension, which is the width of the client surface in
pixels.

*******************************************************************************/

void _pa_maxxg_ovr(ami_maxxg_t nfp, ami_maxxg_t* ofp)
    { *ofp = maxxg_vect; maxxg_vect = nfp; }
int ami_maxxg(FILE* f) { return ((*maxxg_vect)(f)); }

static int maxxg_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxxg);

}

/** ****************************************************************************

Return maximum y dimension graphical

Returns the maximum y dimension, which is the height of the client surface in
pixels.

*******************************************************************************/

void _pa_maxyg_ovr(ami_maxyg_t nfp, ami_maxyg_t* ofp)
    { *ofp = maxyg_vect; maxyg_vect = nfp; }
int ami_maxyg(FILE* f) { return ((*maxyg_vect)(f)); }

static int maxyg_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxyg);

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void _pa_home_ovr(ami_home_t nfp, ami_home_t* ofp)
    { *ofp = home_vect; home_vect = nfp; }
void ami_home(FILE* f) { (*home_vect)(f); }

static void home_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    ihome(win); /* process */

}

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

void _pa_up_ovr(ami_up_t nfp, ami_up_t* ofp)
    { *ofp = up_vect; up_vect = nfp; }
void ami_up(FILE* f) { (*up_vect)(f); }

static void up_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iup(win); /* process */

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

void _pa_down_ovr(ami_down_t nfp, ami_down_t* ofp)
    { *ofp = down_vect; down_vect = nfp; }
void ami_down(FILE* f) { (*down_vect)(f); }

static void down_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    idown(win); /* process */

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

void _pa_left_ovr(ami_left_t nfp, ami_left_t* ofp)
    { *ofp = left_vect; left_vect = nfp; }
void ami_left(FILE* f) { (*left_vect)(f); }

static void left_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    ileft(win); /* process */

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void _pa_right_ovr(ami_right_t nfp, ami_right_t* ofp)
    { *ofp = right_vect; right_vect = nfp; }
void ami_right(FILE* f) { (*right_vect)(f); }

static void right_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iright(win); /* process */

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

Graphical mode does not implement blink mode.

*******************************************************************************/

void _pa_blink_ovr(ami_blink_t nfp, ami_blink_t* ofp)
    { *ofp = blink_vect; blink_vect = nfp; }
void ami_blink(FILE* f, int e) { (*blink_vect)(f, e); }

static void blink_ivf(FILE* f, int e)

{

   /* no capability */

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void _pa_reverse_ovr(ami_reverse_t nfp, ami_reverse_t* ofp)
    { *ofp = reverse_vect; reverse_vect = nfp; }
void ami_reverse(FILE* f, int e) { (*reverse_vect)(f, e); }

static void reverse_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc; /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* reverse on */

        sc->attr |= BIT(sarev); /* set attribute active */
        win->gattr |= BIT(sarev);
        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XSetBackground(padisplay, sc->xcxt, sc->fcrgb);

    } else { /* turn it off */

        sc->attr &= ~BIT(sarev); /* set attribute inactive */
        win->gattr &= ~BIT(sarev);
        XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);

    }

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void _pa_underline_ovr(ami_underline_t nfp, ami_underline_t* ofp)
    { *ofp = underline_vect; underline_vect = nfp; }
void ami_underline(FILE* f, int e) { (*underline_vect)(f, e); }

static void underline_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* underline on */

        sc->attr |= BIT(saundl); /* set attribute active */
        win->gattr |= BIT(saundl);

    } else { /* turn it off */

        sc->attr &= ~BIT(saundl); /* set attribute inactive */
        win->gattr &= ~BIT(saundl);

    }

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and elevated font.

*******************************************************************************/

void _pa_superscript_ovr(ami_superscript_t nfp, ami_superscript_t* ofp)
    { *ofp = superscript_vect; superscript_vect = nfp; }
void ami_superscript(FILE* f, int e) { (*superscript_vect)(f, e); }

static void superscript_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sasuper); /* set attribute active */
       win->gattr |= BIT(sasuper);

    } else { /* turn it off */

       sc->attr &= ~BIT(sasuper); /* set attribute inactive */
       win->gattr &= ~BIT(sasuper);

    }

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and lowered font.

*******************************************************************************/

void _pa_subscript_ovr(ami_subscript_t nfp, ami_subscript_t* ofp)
    { *ofp = subscript_vect; subscript_vect = nfp; }
void ami_subscript(FILE* f, int e) { (*subscript_vect)(f, e); }

static void subscript_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sasubs); /* set attribute active */
       win->gattr |= BIT(sasubs);

    } else { /* turn it off */

       sc->attr &= ~BIT(sasubs); /* set attribute inactive */
       win->gattr &= ~BIT(sasubs);

    }

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void _pa_italic_ovr(ami_italic_t nfp, ami_italic_t* ofp)
    { *ofp = italic_vect; italic_vect = nfp; }
void ami_italic(FILE* f, int e) { (*italic_vect)(f, e); }

static void italic_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saital); /* set attribute active */
       win->gattr |= BIT(saital);

    } else { /* turn it off */

       sc->attr &= ~BIT(saital); /* set attribute inactive */
       win->gattr &= ~BIT(saital);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void _pa_bold_ovr(ami_bold_t nfp, ami_bold_t* ofp)
    { *ofp = bold_vect; bold_vect = nfp; }
void ami_bold(FILE* f, int e) { (*bold_vect)(f, e); }

static void bold_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sabold); /* set attribute active */
       win->gattr |= BIT(sabold);

    } else { /* turn it off */

       sc->attr &= ~BIT(sabold); /* set attribute inactive */
       win->gattr &= ~BIT(sabold);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void _pa_strikeout_ovr(ami_strikeout_t nfp, ami_strikeout_t* ofp)
    { *ofp = strikeout_vect; strikeout_vect = nfp; }
void ami_strikeout(FILE* f, int e) { (*strikeout_vect)(f, e); }

static void strikeout_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sastkout); /* set attribute active */
       win->gattr |= BIT(sastkout);

    } else { /* turn it off */

       sc->attr &= ~BIT(sastkout); /* set attribute inactive */
       win->gattr &= ~BIT(sastkout);

    }

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void _pa_standout_ovr(ami_standout_t nfp, ami_standout_t* ofp)
    { *ofp = standout_vect; standout_vect = nfp; }
void ami_standout(FILE* f, int e) { (*standout_vect)(f, e); }

static void standout_ivf(FILE* f, int e)

{

   ami_reverse(f, e); /* implement as reverse */

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

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
    XWLOCK();
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from individual r, g, b values.

*******************************************************************************/

void _pa_fcolorc_ovr(ami_fcolorc_t nfp, ami_fcolorc_t* ofp)
    { *ofp = fcolorc_vect; fcolorc_vect = nfp; }
void ami_fcolorc(FILE* f, int r, int g, int b) { (*fcolorc_vect)(f, r, g, b); }

static void fcolorc_ivf(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->fcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* set screen color according to reverse */
    XWLOCK();
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set foreground color graphical

Sets the foreground color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

Fcolor exists as an overload to the text version, but we also provide an
fcolorg for backward compatiblity to the days before overloads.

*******************************************************************************/

void _pa_fcolorg_ovr(ami_fcolorg_t nfp, ami_fcolorg_t* ofp)
    { *ofp = fcolorg_vect; fcolorg_vect = nfp; }
void ami_fcolorg(FILE* f, int r, int g, int b) { (*fcolorg_vect)(f, r, g, b); }

static void fcolorg_ivf(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->fcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* set screen color according to reverse */
    XWLOCK();
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

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
    XWLOCK();
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set background color

Sets the background color from individual r, g, b values.

*******************************************************************************/

void _pa_bcolorc_ovr(ami_bcolorc_t nfp, ami_bcolorc_t* ofp)
    { *ofp = bcolorc_vect; bcolorc_vect = nfp; }
void ami_bcolorc(FILE* f, int r, int g, int b) { (*bcolorc_vect)(f, r, g, b); }

static void bcolorc_ivf(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->bcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gbcrgb = sc->bcrgb;
    XWLOCK();
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

void _pa_bcolorg_ovr(ami_bcolorg_t nfp, ami_bcolorg_t* ofp)
    { *ofp = bcolorg_vect; bcolorg_vect = nfp; }
void ami_bcolorg(FILE* f, int r, int g, int b) { (*bcolorg_vect)(f, r, g, b); }

static void bcolorg_ivf(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->bcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gbcrgb = sc->bcrgb; /* copy to master */
    XWLOCK();
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

void _pa_curbnd_ovr(ami_curbnd_t nfp, ami_curbnd_t* ofp)
    { *ofp = curbnd_vect; curbnd_vect = nfp; }
int ami_curbnd(FILE* f) { return ((*curbnd_vect)(f)); }

static int curbnd_ivf(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (icurbnd(win->screens[win->curupd-1]));

}

/** ****************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and end of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up       Scroll down
down     Scroll up
right    Line down, start at left
left     Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

*******************************************************************************/

void _pa_auto_ovr(ami_auto_t nfp, ami_auto_t* ofp)
    { *ofp = auto_vect; auto_vect = nfp; }
void ami_auto(FILE* f, int e) { (*auto_vect)(f, e); }

static void auto_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iauto(win, e); /* execute */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void _pa_curvis_ovr(ami_curvis_t nfp, ami_curvis_t* ofp)
    { *ofp = curvis_vect; curvis_vect = nfp; }
void ami_curvis(FILE* f, int e) { (*curvis_vect)(f, e); }

static void curvis_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->screens[win->curupd-1]->curv = e; /* set cursor visible status */
    win->gcurv = e;
    cursts(win); /* process any cursor status change */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

void _pa_curx_ovr(ami_curx_t nfp, ami_curx_t* ofp)
    { *ofp = curx_vect; curx_vect = nfp; }
int ami_curx(FILE* f) { return ((*curx_vect)(f)); }

static int curx_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curx); /* process */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

void _pa_cury_ovr(ami_cury_t nfp, ami_cury_t* ofp)
    { *ofp = cury_vect; cury_vect = nfp; }
int ami_cury(FILE* f) { return ((*cury_vect)(f)); }

static int cury_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->cury); /* process */

}

/** ****************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

*******************************************************************************/

void _pa_curxg_ovr(ami_curxg_t nfp, ami_curxg_t* ofp)
    { *ofp = curxg_vect; curxg_vect = nfp; }
int ami_curxg(FILE* f) { return ((*curxg_vect)(f)); }

static int curxg_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curxg); /* process */

}

/** ****************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

*******************************************************************************/

void _pa_curyg_ovr(ami_curyg_t nfp, ami_curyg_t* ofp)
    { *ofp = curyg_vect; curyg_vect = nfp; }
int ami_curyg(FILE* f) { return ((*curyg_vect)(f)); }

static int curyg_ivf(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curyg); /* return yg */

}

/** ****************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

void _pa_select_ovr(ami_select_t nfp, ami_select_t* ofp)
    { *ofp = select_vect; select_vect = nfp; }
void ami_select(FILE* f, int u, int d) { (*select_vect)(f, u, d); }

static void select_ivf(FILE* f, int u, int d)

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
        scncnt++;
        scntot += sizeof(scncon);
        iniscn(win, win->screens[win->curupd-1]); /* initalize that */

    }
    win->curdsp = d; /* set the current display screen */
    if (!win->screens[win->curdsp-1]) { /* no screen, create one */

        /* no current screen, create a new one */
        win->screens[win->curdsp-1] = imalloc(sizeof(scncon));
        scncnt++;
        scntot += sizeof(scncon);
        iniscn(win, win->screens[win->curdsp-1]); /* initalize that */

    }
    /* if the screen has changed, restore it */
    if (win->curdsp != ld) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        else restore(win);

    }

}

/** ****************************************************************************

Place string with foreground and background 90 degrees

Places the given string with selected background and foreground. Accepts the
current window, current screen, character spacing, a flag indicating the
character exists in the current font, and the window to draw on.

This routine handles the special case of a 90 degree draw, which means normal
XWindows text.

Note: Does not deal with missing characters in font.

*******************************************************************************/

static void drwstr90(winptr win, scnptr sc, int tw, Drawable d, char* s, int l)

{

    /* transform position and sizes to physical pixels for viewport scaling */
    int px  = L2PX(win, sc->curxg-1);
    int py  = L2PY(win, sc->curyg-1);
    int ptw = L2PW(win, tw);
    int pls = L2PH(win, win->linespace);
    int pbo = L2PH(win, win->baseoff);

    if (sc->bmod != mdinvis) { /* background is visible */

        XWLOCK();
        /* set background function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
        XSetFillStyle(padisplay, sc->xcxt, FillSolid); /* ensure solid fill */
        /* set background to foreground to draw character background */
        if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XFillRectangle(padisplay, d, sc->xcxt, px, py, ptw, pls);
        /* xor is non-destructive, and we can restore it. And and or are
           destructive, and would require a combining buffer to perform */
        if (sc->bmod == mdxor)
            /* restore surface under text */
            ft_draw_string(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                           px, py+pbo, s, l);
        /* restore colors */
        if (BIT(sarev) & sc->attr)
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        /* reset background function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }
    if (sc->fmod != mdinvis) {

        XWLOCK();
        /* set foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        /* draw character */
        ft_draw_string(d, sc->xcxt, win->ftface, win->gfhighx, win->gfhigh,
                       px, py+pbo, s, l);
        /* check draw underline */
        if (sc->attr & BIT(saundl)){

            /* double line, may need ajusting for low DP displays */
            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+pbo+1, px+ptw, py+pbo+1);
            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+pbo+2, px+ptw, py+pbo+2);

        }

        /* check draw strikeout */
        if (sc->attr & BIT(sastkout)) {

            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+(int)(pbo/STRIKE),
                      px+ptw, py+(int)(pbo/STRIKE));
            XDrawLine(padisplay, d, sc->xcxt,
                      px, py+(int)(pbo/STRIKE)+1,
                      px+ptw, py+(int)(pbo/STRIKE)+1);

        }
        /* reset foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Write string to current cursor position with length

Writes a string with length to the current cursor position, then updates the 
cursor position. This acts as a series of write character calls. However, it 
eliminates several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

Attributes are performed, such as foreground/background coloring, modes, and
character attributes.

Character kerning is only available via this routine, and strsiz() is only
accurate for this routine, and not direct character placement, if kerning is
enabled.

Note: If an off angle (non-90 degree) text path is selected, this routine just
passes the characters on to plcchr(). This basically negates any speed
advantage.

*******************************************************************************/

void _pa_wrtstrn_ovr(ami_wrtstrn_t nfp, ami_wrtstrn_t* ofp)
    { *ofp = wrtstrn_vect; wrtstrn_vect = nfp; }
void ami_wrtstrn(FILE* f, char* s, int l) { (*wrtstrn_vect)(f, s, l); }

static void wrtstrn_ivf(FILE* f, char* s, int l)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int    tw;  /* text length in pixels */
    char*  p;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (sc->autof) error(estrato); /* autowrap is on */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (sc->angle == INT_MAX/4) { /* text is normal (90 degrees) */

        tw = ft_text_width(win->ftface, s, l); /* find text width in pixels */
        if (win->bufmod) { /* buffer is active */

            /* draw string */
            drwstr90(win, sc, tw, sc->xbuf, s, l);

        }
        if (indisp(win)) { /* do it again for the current screen */

            curoff(win); /* hide the cursor */
            /* draw string */
            drwstr90(win, sc, tw, win->xwhan, s, l);
            curon(win); /* show the cursor */

        }

    } else /* off angle */
        /* just pass each character on */
        for (p = s; *p && l; p++, l--) plcchr(win, *p);

}

/** ****************************************************************************

Write string to current cursor position

Writes a string to the current cursor position, then updates the cursor
position. This acts as a series of write character calls. However, it eliminates
several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

Attributes are performed, such as foreground/background coloring, modes, and
character attributes.

Character kerning is only available via this routine, and strsiz() is only
accurate for this routine, and not direct character placement, if kerning is
enabled.

Note: If an off angle (non-90 degree) text path is selected, this routine just
passes the characters on to plcchr(). This basically negates any speed
advantage.

*******************************************************************************/

void _pa_wrtstr_ovr(ami_wrtstr_t nfp, ami_wrtstr_t* ofp)
    { *ofp = wrtstr_vect; wrtstr_vect = nfp; }
void ami_wrtstr(FILE* f, char* s) { (*wrtstr_vect)(f, s); }

static void wrtstr_ivf(FILE* f, char* s)

{

    int    l;   /* length of string */

    ami_wrtstrn(f, s, strlen(s));

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

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

/** ****************************************************************************

Draw line

Draws a single line in the foreground color.

*******************************************************************************/

void _pa_line_ovr(ami_line_t nfp, ami_line_t* ofp)
    { *ofp = line_vect; line_vect = nfp; }
void ami_line(FILE* f, int x1, int y1, int x2, int y2)
    { (*line_vect)(f, x1, y1, x2, y2); }

static void line_ivf(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the line */
        XWLOCK();
        XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                  L2PX(win, x1-1), L2PY(win, y1-1),
                  L2PX(win, x2-1), L2PY(win, y2-1));
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        XWLOCK();
        /* draw the line */
        XDrawLine(padisplay, win->xwhan, sc->xcxt,
                  L2PX(win, x1-1), L2PY(win, y1-1),
                  L2PX(win, x2-1), L2PY(win, y2-1));
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

*******************************************************************************/

void _pa_rect_ovr(ami_rect_t nfp, ami_rect_t* ofp)
    { *ofp = rect_vect; rect_vect = nfp; }
void ami_rect(FILE* f, int x1, int y1, int x2, int y2)
    { (*rect_vect)(f, x1, y1, x2, y2); }

static void rect_ivf(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the rectangle */
        XWLOCK();
        XDrawRectangle(padisplay, sc->xbuf, sc->xcxt,
                       L2PX(win, x1-1), L2PY(win, y1-1),
                       L2PW(win, x2-x1), L2PH(win, y2-y1));
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the rectangle */
        XWLOCK();
        XDrawRectangle(padisplay, win->xwhan, sc->xcxt,
                       L2PX(win, x1-1), L2PY(win, y1-1),
                       L2PW(win, x2-x1), L2PH(win, y2-y1));
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

*******************************************************************************/

void _pa_frect_ovr(ami_frect_t nfp, ami_frect_t* ofp)
    { *ofp = frect_vect; frect_vect = nfp; }
void ami_frect(FILE* f, int x1, int y1, int x2, int y2)
    { (*frect_vect)(f, x1, y1, x2, y2); }

static void frect_ivf(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the rectangle */
        XWLOCK();
        XFillRectangle(padisplay, sc->xbuf, sc->xcxt,
                       L2PX(win, x1-1), L2PY(win, y1-1),
                       L2PW(win, x2-x1+1), L2PH(win, y2-y1+1));
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the rectangle */
        XWLOCK();
        XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                       L2PX(win, x1-1), L2PY(win, y1-1),
                       L2PW(win, x2-x1+1), L2PH(win, y2-y1+1));
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

In XWindow, this has to be constructed, since there is no equivalent function.

*******************************************************************************/

void _pa_rrect_ovr(ami_rrect_t nfp, ami_rrect_t* ofp)
    { *ofp = rrect_vect; rrect_vect = nfp; }
void ami_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)
    { (*rrect_vect)(f, x1, y1, x2, y2, xs, ys); }

static void rrect_ivf(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* adjust for X */
    x1--;
    y1--;
    x2--;
    y2--;
    x1 = L2PX(win, x1); y1 = L2PY(win, y1);
    x2 = L2PX(win, x2); y2 = L2PY(win, y2);
    xs = L2PW(win, xs); ys = L2PH(win, ys);
    /* limit the size of the corner circles if greater than the height or
        width */
    if (xs > x2-x1+1) xs = x2-x1+1; /* limit rounding elipse */
    if (ys > y2-y1+1) ys = y2-y1+1;
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        XWLOCK();
        /* stroke the sides */
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x1, y1+ys/2, x1, y2-ys/2);
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x2, y1+ys/2, x2, y2-ys/2);
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y1, x2-xs/2, y1);
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y2, x2-xs/2, y2);
        /* draw corner arcs */
        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x2-xs, y1, xs, ys, 0, 90*64);

        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x1, y2-ys, xs, ys, 180*64,
                 90*64);

        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x2-xs, y2-ys, xs, ys, 270*64,
                 90*64);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        XWLOCK();
        /* stroke the sides */
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x1, y1+ys/2, x1, y2-ys/2);
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x2, y1+ys/2, x2, y2-ys/2);
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y1, x2-xs/2, y1);
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y2, x2-xs/2, y2);
        /* draw corner arcs */
        XDrawArc(padisplay, win->xwhan, sc->xcxt, x1, y1, xs, ys,
                 90*64, 90*64);
        XDrawArc(padisplay, win->xwhan, sc->xcxt, x2-xs, y1, xs, ys,
                 0, 90*64);
        XDrawArc(padisplay, win->xwhan, sc->xcxt, x1, y2-ys, xs, ys, 180*64,
                 90*64);

        XDrawArc(padisplay, win->xwhan, sc->xcxt, x2-xs, y2-ys, xs, ys, 270*64,
                 90*64);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);

}

/** ****************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

In XWindow, this has to be constructed, since there is no equivalent function.

The code compensates quite a bit for degenerative cases such as single line
x or y sizes.

*******************************************************************************/

void _pa_frrect_ovr(ami_frrect_t nfp, ami_frrect_t* ofp)
    { *ofp = frrect_vect; frrect_vect = nfp; }
void ami_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)
    { (*frrect_vect)(f, x1, y1, x2, y2, xs, ys); }

static void frrect_ivf(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int wm;     /* width of middle rectangle */
    int hm;     /* height of middle rectangle */
    int wtb;    /* width of top/bottom rectangle */
    int htb;    /* height of top/bottom rectangle */
    int wlr;    /* width of left/right rectangle */
    int hlr;    /* height of left/right rectangle */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* adjust for X */
    x1--;
    y1--;
    x2--;
    y2--;
    x1 = L2PX(win, x1); y1 = L2PY(win, y1);
    x2 = L2PX(win, x2); y2 = L2PY(win, y2);
    xs = L2PW(win, xs); ys = L2PH(win, ys);
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (x2-x1 >= y2-y1) { /* x >= y */

        /* find the widths and heights of components, and find minimums */
        wm = x2-x1+1; /* set width of middle */
        if (wm < 1) wm = 1;
        hm = y2-y1+1-ys; /* set height of middle */
        if (ys%2) hm++; /* distribute fraction to middle */
        if (hm < 1) hm = 1;
        wtb = x2-x1+1-xs; /* set width of top and bottom */
        if (xs%2) wtb++; /* distribute fraction to top and bottom width */
        if (wtb < 0) wtb = 0;
        htb = ys/2; /* set height of top and bottom */
        if (y2-y1+1-hm < htb) htb = y2-y1+1-hm;
        if (htb < 0) htb = 0;
        if (xs > x2-x1+1) xs = x2-x1+1; /* limit rounding elipse */
        if (ys > y2-y1+1) ys = y2-y1+1;
        if (win->bufmod) { /* buffer is active */

            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1, y1+ys/2, wm, hm);

            /* top */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y1, wtb, htb);

            /* bottom */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y2-ys/2+1, wtb, htb);

            /* draw corner arcs */
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs+1, y1, xs, ys, 0, 90*64);

            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);

            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs+1, y2-ys+1, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible)  winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1, y1+ys/2, wm, hm);

            /* top */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y1, wtb, htb);

            /* bottom */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y2-ys/2+1, wtb, htb);

            /* draw corner arcs */
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y1, xs, ys,
                     90*64, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y1, xs, ys,
                     0, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y2-ys+1, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }

    } else { /* y > x */

        /* find the widths and heights of components, and find minimums */
        wm = x2-x1+1-xs; /* set width of middle */
        if (xs%2) wm++; /* distribute fraction to middle */
        if (wm < 1) wm = 1;
        hm = y2-y1+1; /* set height of middle */
        if (hm < 1) hm = 1;
        wlr = xs/2; /* set width of left and right */
        if (x2-x1+1-wm < wlr) wlr = x2-x1+1-wm;
        if (wlr < 0) wlr = 0;
        hlr = y2-y1+1-ys; /* set height of top and bottom */
        if (ys%2) hlr++; /* distribute fraction to left and right height */
        if (hlr < 0) hlr = 0;
        if (xs > x2-x1+1) xs = x2-x1+1; /* limit rounding elipse */
        if (ys > y2-y1+1) ys = y2-y1+1;
        if (win->bufmod) { /* buffer is active */

            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y1, wm, hm);

            /* left */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1, y1+ys/2, wlr, hlr);

            /* right */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x2-xs/2+1, y1+ys/2, wlr, hlr);

            /* draw corner arcs */
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs+1, y1, xs, ys, 0, 90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs-1, y2-ys, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible)  winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y1, wm, hm);

            /* left */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1, y1+ys/2, wlr, hlr);

            /* right */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x2-xs/2+1, y1+ys/2, wlr, hlr);

            /* draw corner arcs */
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y1, xs, ys, 0, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y2-ys+1, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

*******************************************************************************/

void _pa_ellipse_ovr(ami_ellipse_t nfp, ami_ellipse_t* ofp)
    { *ofp = ellipse_vect; ellipse_vect = nfp; }
void ami_ellipse(FILE* f, int x1, int y1, int x2, int y2)
    { (*ellipse_vect)(f, x1, y1, x2, y2); }

static void ellipse_ivf(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the ellipse */
        XWLOCK();
        XDrawArc(padisplay, sc->xbuf, sc->xcxt,
                 L2PX(win, x1-1), L2PY(win, y1-1),
                 L2PW(win, x2-x1), L2PH(win, y2-y1),
                 0, 360*64);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the ellipse */
        XWLOCK();
        XDrawArc(padisplay, win->xwhan, sc->xcxt,
                 L2PX(win, x1-1), L2PY(win, y1-1),
                 L2PW(win, x2-x1), L2PH(win, y2-y1),
                 0, 360*64);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

*******************************************************************************/

void _pa_fellipse_ovr(ami_fellipse_t nfp, ami_fellipse_t* ofp)
    { *ofp = fellipse_vect; fellipse_vect = nfp; }
void ami_fellipse(FILE* f, int x1, int y1, int x2, int y2)
    { (*fellipse_vect)(f, x1, y1, x2, y2); }

static void fellipse_ivf(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the ellipse */
        XWLOCK();
        XFillArc(padisplay, sc->xbuf, sc->xcxt,
                 L2PX(win, x1-1), L2PY(win, y1-1),
                 L2PW(win, x2-x1+1), L2PH(win, y2-y1+1),
                 0, 360*64);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the ellipse */
        XWLOCK();
        XFillArc(padisplay, win->xwhan, sc->xcxt,
                 L2PX(win, x1-1), L2PY(win, y1-1),
                 L2PW(win, x2-x1+1), L2PH(win, y2-y1+1),
                 0, 360*64);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw arc

Draws an arc in the current foreground color and line width. The containing
rectangle of the ellipse is given, and the start and end angles clockwise from
0 degrees delimit the arc.

Windows takes the start and end delimited by a line extending from the center
of the arc. The way we do the convertion is to project the angle upon a circle
whose radius is the precision we wish to use for the calculation. Then that
point on the circle is found by triangulation.

The larger the circle of precision, the more angles can be represented, but
the trade off is that the circle must not reach the edge of an integer
(-maxint..maxint). That means that the total logical coordinate space must be
shortened by the precision. To find out what division of the circle precis
represents, use cd := precis*2*pi. So, for example, precis = 100 means 628
divisions of the circle.

The end and start points can be negative.
Note that XWindow draws arcs counterclockwise, so our start and end points are
swapped.

Negative angles are allowed.

*******************************************************************************/

void _pa_arc_ovr(ami_arc_t nfp, ami_arc_t* ofp)
    { *ofp = arc_vect; arc_vect = nfp; }
void ami_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)
    { (*arc_vect)(f, x1, y1, x2, y2, sa, ea); }

static void arc_ivf(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int a1, a2; /* XWindow angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        XWUNLOCK();
        if (win->bufmod) { /* buffer is active */

            /* draw the arc */
            XWLOCK();
            XDrawArc(padisplay, sc->xbuf, sc->xcxt,
                     L2PX(win, x1-1), L2PY(win, y1-1),
                     L2PW(win, x2-x1), L2PH(win, y2-y1),
            		 a1, a2);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            /* draw the arc */
            XWLOCK();
            XDrawArc(padisplay, win->xwhan, sc->xcxt,
                     L2PX(win, x1-1), L2PY(win, y1-1),
                     L2PW(win, x2-x1), L2PH(win, y2-y1),
            		 a1, a2);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }
        /* reset foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc function above.

Note: XWindows was found to be off the origin by 1, so we put corrections in for
this.

*******************************************************************************/

void _pa_farc_ovr(ami_farc_t nfp, ami_farc_t* ofp)
    { *ofp = farc_vect; farc_vect = nfp; }
void ami_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)
    { (*farc_vect)(f, x1, y1, x2, y2, sa, ea); }

static void farc_ivf(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int a1, a2; /* XWindow angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        XWUNLOCK();
        if (win->bufmod) { /* buffer is active */

            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, sc->xbuf, sc->xcxt,
                     L2PX(win, x1-1), L2PY(win, y1-1),
                     L2PW(win, x2-x1+1), L2PH(win, y2-y1+1),
                     a1, a2);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, win->xwhan, sc->xcxt,
                     L2PX(win, x1-1), L2PY(win, y1-1),
                     L2PW(win, x2-x1+1), L2PH(win, y2-y1+1),
                     a1, a2);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }
        /* reset foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void _pa_fchord_ovr(ami_fchord_t nfp, ami_fchord_t* ofp)
    { *ofp = fchord_vect; fchord_vect = nfp; }
void ami_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)
    { (*fchord_vect)(f, x1, y1, x2, y2, sa, ea); }

static void fchord_ivf(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int a1, a2; /* XWindow angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        XSetArcMode(padisplay, sc->xcxt, ArcChord); /* set chord mode */
        XWUNLOCK();
        if (win->bufmod) { /* buffer is active */

            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, sc->xbuf, sc->xcxt, L2PX(win, x1-1), L2PY(win, y1-1), L2PW(win, x2-x1+1), L2PH(win, y2-y1+1),
                     a1, a2);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, win->xwhan, sc->xcxt, L2PX(win, x1-1), L2PY(win, y1-1), L2PW(win, x2-x1+1), L2PH(win, y2-y1+1),
                     a1, a2);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }
        XWLOCK();
        XSetArcMode(padisplay, sc->xcxt, ArcPieSlice); /* set pie mode */
        /* reset foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

*******************************************************************************/

void _pa_ftriangle_ovr(ami_ftriangle_t nfp, ami_ftriangle_t* ofp)
    { *ofp = ftriangle_vect; ftriangle_vect = nfp; }
void ami_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)
    { (*ftriangle_vect)(f, x1, y1, x2, y2, x3, y3); }

static void ftriangle_ivf(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    XPoint pa[3]; /* XWindow points array */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* place the triangle points in the X array */
    pa[0].x = L2PX(win, x1-1);  pa[0].y = L2PY(win, y1-1);
    pa[1].x = L2PX(win, x2-1);  pa[1].y = L2PY(win, y2-1);
    pa[2].x = L2PX(win, x3-1);  pa[2].y = L2PY(win, y3-1);

    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the triangle */
        XWLOCK();
        XFillPolygon(padisplay, sc->xbuf, sc->xcxt, pa, 3, Convex,
                     CoordModeOrigin);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the ellipse */
        XWLOCK();
        XFillPolygon(padisplay, win->xwhan, sc->xcxt, pa, 3, Convex,
                     CoordModeOrigin);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

*******************************************************************************/

void _pa_setpixel_ovr(ami_setpixel_t nfp, ami_setpixel_t* ofp)
    { *ofp = setpixel_vect; setpixel_vect = nfp; }
void ami_setpixel(FILE* f, int x, int y) { (*setpixel_vect)(f, x, y); }

static void setpixel_ivf(FILE* f, int x, int y)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        curoff(win); /* hide the cursor */
        /* draw the pixel */
        XWLOCK();
        XDrawPoint(padisplay, sc->xbuf, sc->xcxt,
                   L2PX(win, x-1), L2PY(win, y-1));
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the pixel */
        XWLOCK();
        XDrawPoint(padisplay, win->xwhan, sc->xcxt,
                   L2PX(win, x-1), L2PY(win, y-1));
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

*******************************************************************************/

void _pa_fover_ovr(ami_fover_t nfp, ami_fover_t* ofp)
    { *ofp = fover_vect; fover_vect = nfp; }
void ami_fover(FILE* f) { (*fover_vect)(f); }

static void fover_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdnorm; /* set foreground mode overwrite */
    sc->fmod = mdnorm;

}

/** ****************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

*******************************************************************************/

void _pa_bover_ovr(ami_bover_t nfp, ami_bover_t* ofp)
    { *ofp = bover_vect; bover_vect = nfp; }
void ami_bover(FILE* f) { (*bover_vect)(f); }

static void bover_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdnorm; /* set background mode overwrite */
    sc->bmod = mdnorm;

}

/** ****************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

*******************************************************************************/

void _pa_finvis_ovr(ami_finvis_t nfp, ami_finvis_t* ofp)
    { *ofp = finvis_vect; finvis_vect = nfp; }
void ami_finvis(FILE* f) { (*finvis_vect)(f); }

static void finvis_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdinvis; /* set foreground mode invisible */
    sc->fmod = mdinvis;

}

/** ****************************************************************************

Set background to invisible

Sets the background write mode to invisible.

*******************************************************************************/

void _pa_binvis_ovr(ami_binvis_t nfp, ami_binvis_t* ofp)
    { *ofp = binvis_vect; binvis_vect = nfp; }
void ami_binvis(FILE* f) { (*binvis_vect)(f); }

static void binvis_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdinvis; /* set background mode invisible */
    sc->bmod = mdinvis;

}

/** ****************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

*******************************************************************************/

void _pa_fxor_ovr(ami_fxor_t nfp, ami_fxor_t* ofp)
    { *ofp = fxor_vect; fxor_vect = nfp; }
void ami_fxor(FILE* f) { (*fxor_vect)(f); }

static void fxor_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdxor; /* set foreground mode xor */
    sc->fmod = mdxor;

}

/** ****************************************************************************

Set background to xor

Sets the background write mode to xor.

*******************************************************************************/

void _pa_bxor_ovr(ami_bxor_t nfp, ami_bxor_t* ofp)
    { *ofp = bxor_vect; bxor_vect = nfp; }
void ami_bxor(FILE* f) { (*bxor_vect)(f); }

static void bxor_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdxor; /* set background mode xor */
    sc->bmod = mdxor;

}

/** ****************************************************************************

Set foreground to and

Sets the foreground write mode to and.

*******************************************************************************/

void _pa_fand_ovr(ami_fand_t nfp, ami_fand_t* ofp)
    { *ofp = fand_vect; fand_vect = nfp; }
void ami_fand(FILE* f) { (*fand_vect)(f); }

static void fand_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdand; /* set foreground mode and */
    sc->fmod = mdand;

}

/** ****************************************************************************

Set background to and

Sets the background write mode to and.

*******************************************************************************/

void _pa_band_ovr(ami_band_t nfp, ami_band_t* ofp)
    { *ofp = band_vect; band_vect = nfp; }
void ami_band(FILE* f) { (*band_vect)(f); }

static void band_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdand; /* set background mode and */
    sc->bmod = mdand;

}

/** ****************************************************************************

Set foreground to or

Sets the foreground write mode to or.

*******************************************************************************/

void _pa_for_ovr(ami_for_t nfp, ami_for_t* ofp)
    { *ofp = for_vect; for_vect = nfp; }
void ami_for(FILE* f) { (*for_vect)(f); }

static void for_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdor; /* set foreground mode or */
    sc->fmod = mdor;

}

/** ****************************************************************************

Set background to or

Sets the background write mode to or.

*******************************************************************************/

void _pa_bor_ovr(ami_bor_t nfp, ami_bor_t* ofp)
    { *ofp = bor_vect; bor_vect = nfp; }
void ami_bor(FILE* f) { (*bor_vect)(f); }

static void bor_ivf(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdor; /* set background mode or */
    sc->bmod = mdor;

}

/** ****************************************************************************

Set line width

Sets the width of lines and several other figures.

*******************************************************************************/

void _pa_linewidth_ovr(ami_linewidth_t nfp, ami_linewidth_t* ofp)
    { *ofp = linewidth_vect; linewidth_vect = nfp; }
void ami_linewidth(FILE* f, int w) { (*linewidth_vect)(f, w); }

static void linewidth_ivf(FILE* f, int w)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->lwidth = w; /* set the line width */
    XWLOCK();
    /* copy to X */
    XSetLineAttributes(padisplay, sc->xcxt, w, LineSolid, CapButt, JoinMiter);
    XWUNLOCK();

}

/** ****************************************************************************

Find character size x

Returns the character width. This only works if the font is fixed. If it is
proportional, this just returns the width of a space, which is the widest
character in the character set.

*******************************************************************************/

void _pa_chrsizx_ovr(ami_chrsizx_t nfp, ami_chrsizx_t* ofp)
    { *ofp = chrsizx_vect; chrsizx_vect = nfp; }
int ami_chrsizx(FILE* f) { return ((*chrsizx_vect)(f)); }

static int chrsizx_ivf(FILE* f)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */

    return (win->charspace); /* return character spacing */

}

/** ****************************************************************************

Find character size y

Returns the character height.

*******************************************************************************/

void _pa_chrsizy_ovr(ami_chrsizy_t nfp, ami_chrsizy_t* ofp)
    { *ofp = chrsizy_vect; chrsizy_vect = nfp; }
int ami_chrsizy(FILE* f) { return ((*chrsizy_vect)(f)); }

static int chrsizy_ivf(FILE* f)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */

    return (win->linespace); /* return line spacing */

}

/** ****************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

*******************************************************************************/

void _pa_fonts_ovr(ami_fonts_t nfp, ami_fonts_t* ofp)
    { *ofp = fonts_vect; fonts_vect = nfp; }
int ami_fonts(FILE* f) { return ((*fonts_vect)(f)); }

static int fonts_ivf(FILE* f)

{

    return (fntcnt); /* just return the global font count */

}

/** ****************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

*******************************************************************************/

void _pa_font_ovr(ami_font_t nfp, ami_font_t* ofp)
    { *ofp = font_vect; font_vect = nfp; }
void ami_font(FILE* f, int fc) { (*font_vect)(f, fc); }

static void font_ivf(FILE* f, int fc)

{

    fontptr fp;  /* font pointer */
    winptr  win; /* windows record pointer */
    scnptr  sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (win->screens[win->curupd-1]->autof)
        error(eatoftc); /* cannot perform with auto on */
    if (fc < 1) error(einvfnm); /* invalid font number */
    /* find indicated font */
    fp = fntlst;
    while (fp != NULL && fc > 1) { /* search */

       fp = fp->next; /* next font entry */
       fc--; /* count */

    }
    if (fc > 1)  error(einvfnm); /* invalid font number */
    if (!strlen(fp->fn)) error(efntemp); /* font is not assigned */
    curoff(win); /* remove cursor with old font characteristics */
    win->screens[win->curupd-1]->cfont = fp; /* place new font */
    win->gcfont = fp;
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Find name of font

Returns the name of a font by number.

*******************************************************************************/

void _pa_fontnam_ovr(ami_fontnam_t nfp, ami_fontnam_t* ofp)
    { *ofp = fontnam_vect; fontnam_vect = nfp; }
void ami_fontnam(FILE* f, int fc, char* fns, int fnsl)
    { (*fontnam_vect)(f, fc, fns, fnsl); }

static void fontnam_ivf(FILE* f, int fc, char* fns, int fnsl)

{

    fontptr fp; /* pointer to font entries */
    int i; /* string index */

    if (fc <= 0) error(einvftn); /* invalid number */
    fp = fntlst; /* index top of list */
    while (fc > 1) { /* walk fonts */

       fp = fp->next; /* next font */
       fc = fc-1; /* count */
       if (!fp) error(einvftn); /* check null */

    }
    if (strlen(fp->fn) > fnsl+1) error(eftntl);
    strcpy(fns, fp->fn);

}

/** ****************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

*******************************************************************************/

void _pa_fontsiz_ovr(ami_fontsiz_t nfp, ami_fontsiz_t* ofp)
    { *ofp = fontsiz_vect; fontsiz_vect = nfp; }
void ami_fontsiz(FILE* f, int s) { (*fontsiz_vect)(f, s); }

static void fontsiz_ivf(FILE* f, int s)

{

    winptr  win; /* windows record pointer */

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

/* Set font size by point size (typographic points). The em-square pixel
   size is computed from the DPI via gfhigh = ps * dpmy / 2835. The
   resulting character cell height can be queried via ami_chrsizy(). */
void _pa_setpoints_ovr(ami_setpoints_t nfp, ami_setpoints_t* ofp)
    { *ofp = setpoints_vect; setpoints_vect = nfp; }
void ami_setpoints(FILE* f, float ps) { (*setpoints_vect)(f, ps); }

static void setpoints_ivf(FILE* f, float ps)

{

    winptr win;
    int    pixsiz, asc, dsc;

    win = txt2win(f);
    if (win->screens[win->curupd-1]->autof)
        error(eatoftc);
    curoff(win);
    pixsiz = (int)(ps * (float)win->sdpmy / 2835.0f + 0.5f);
    if (pixsiz < 1) pixsiz = 1;
    /* apply the point size directly to the current face to measure the
       resulting cell height, then promote that to gfcellh so subsequent
       font changes preserve it */
    if (!win->ftface) setfnt(win);
    {
        int pixsizx = (win->vsy != 0.0f) ?
                       (int)(pixsiz * win->vsx / win->vsy) : pixsiz;
        FT_Set_Pixel_Sizes(win->ftface, pixsizx, pixsiz);
        win->gfhighx = pixsizx;
    }
    asc = (int)( win->ftface->size->metrics.ascender  >> 6);
    dsc = (int)(-win->ftface->size->metrics.descender >> 6);
    win->gfcellh = asc + dsc + 2;
    win->gfhigh  = pixsiz;
    win->gfpoint = ps;
    win->linespace = win->gfcellh;
    win->baseoff = asc + 1;
    win->charspace = (int)(win->ftface->size->metrics.max_advance >> 6);
    win->mischrx = win->gfhigh*MISCHRX;
    win->mischry = win->gfhigh*MISCHRY;
    win->misoffx = win->gfhigh*MISOFFX;
    win->misoffy = win->gfhigh*MISOFFY;
    curon(win);

}

/* Return the current font point size. */
void _pa_points_ovr(ami_points_t nfp, ami_points_t* ofp)
    { *ofp = points_vect; points_vect = nfp; }
float ami_points(FILE* f) { return ((*points_vect)(f)); }

static float points_ivf(FILE* f)

{

    winptr win;

    win = txt2win(f);
    return win->gfpoint;

}

/** ****************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "leading".

*******************************************************************************/

void _pa_chrspcy_ovr(ami_chrspcy_t nfp, ami_chrspcy_t* ofp)
    { *ofp = chrspcy_vect; chrspcy_vect = nfp; }
void ami_chrspcy(FILE* f, int s) { (*chrspcy_vect)(f, s); }

static void chrspcy_ivf(FILE* f, int s)

{

    winptr  win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->chrspcy = s; /* set leading */

}

/** ****************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

*******************************************************************************/

void _pa_chrspcx_ovr(ami_chrspcx_t nfp, ami_chrspcx_t* ofp)
    { *ofp = chrspcx_vect; chrspcx_vect = nfp; }
void ami_chrspcx(FILE* f, int s) { (*chrspcx_vect)(f, s); }

static void chrspcx_ivf(FILE* f, int s)

{

    winptr  win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->chrspcx = s; /* set ledding */

}

/** ****************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

*******************************************************************************/

void _pa_dpmx_ovr(ami_dpmx_t nfp, ami_dpmx_t* ofp)
    { *ofp = dpmx_vect; dpmx_vect = nfp; }
int ami_dpmx(FILE* f) { return ((*dpmx_vect)(f)); }

static int dpmx_ivf(FILE* f)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */

    return (win->sdpmx); /* return value */

}

/** ****************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

*******************************************************************************/

void _pa_dpmy_ovr(ami_dpmy_t nfp, ami_dpmy_t* ofp)
    { *ofp = dpmy_vect; dpmy_vect = nfp; }
int ami_dpmy(FILE* f) { return ((*dpmy_vect)(f)); }

static int dpmy_ivf(FILE* f)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */

    return (win->sdpmy); /* return value */

}

/** ****************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

*******************************************************************************/

void _pa_strsiz_ovr(ami_strsiz_t nfp, ami_strsiz_t* ofp)
    { *ofp = strsiz_vect; strsiz_vect = nfp; }
int ami_strsiz(FILE* f, const char* s) { return ((*strsiz_vect)(f, s)); }

static int strsiz_ivf(FILE* f, const char* s)

{

    winptr win; /* window pointer */
    int    rv;

    win = txt2win(f); /* get window pointer from text file */
    rv = ft_text_width(win->ftface, s, strlen(s)); /* return value */

    return (rv);

}

/** ****************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

*******************************************************************************/

void _pa_chrpos_ovr(ami_chrpos_t nfp, ami_chrpos_t* ofp)
    { *ofp = chrpos_vect; chrpos_vect = nfp; }
int ami_chrpos(FILE* f, const char* s, int p) 
    { return ((*chrpos_vect)(f, s, p)); }

static int chrpos_ivf(FILE* f, const char* s, int p)

{

    winptr win; /* window pointer */
    int    rv;

    if (p < 0 || p > strlen(s)) error(estrinx); /* out of range */
    win = txt2win(f); /* get window pointer from text file */
    rv = ft_text_width(win->ftface, s, p); /* return value */

    return (rv);

}

/** ****************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function.

XWindow does not provide justification. We implement a simple algorithim that
distributes the space amoung the spaces present in the string.

*******************************************************************************/

void _pa_writejust_ovr(ami_writejust_t nfp, ami_writejust_t* ofp)
    { *ofp = writejust_vect; writejust_vect = nfp; }
void ami_writejust(FILE* f, const char* s, int n) { (*writejust_vect)(f, s, n); }

static void writejust_ivf(FILE* f, const char* s, int n)

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
    /* if space provided is greater than the minimum, distribute the extra space
       amoung the existing spaces */
    ss = ns*MINJST; /* set minimum distribution of space */
    if (n > sz) { spc = (n-cs)/ns; ss = n-cs; }
    /* Output the string with our choosen spacing */
    for (i = 0; i < l; i++) {

        if (s[i] == ' ') {

            if (spc > ss) cbs = ss; /* set space to next character */
            else cbs = spc;
            /* draw in background */
            if (win->bufmod) { /* buffer is active */

                if (sc->bmod != mdinvis) { /* background is visible */

                    XWLOCK();
                    /* set background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
                    /* set background to foreground to draw character background */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    XFillRectangle(padisplay, sc->xbuf, sc->xcxt,
                                   sc->curxg-1, sc->curyg-1,
                                   cbs, win->linespace);
                    /* restore colors */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    /* reset background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                    XWUNLOCK();

                }

            }

            if (indisp(win)) { /* do it again for the current screen */

                if (!win->visible) winvis(win); /* make sure we are displayed */
                curoff(win); /* hide the cursor */
                if (sc->bmod != mdinvis) { /* background is visible */

                    XWLOCK();
                    /* set background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
                    /* set background to foreground to draw character background */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                                   sc->curxg-1, sc->curyg-1,
                                   cbs, win->linespace);
                    /* restore colors */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    /* reset background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                    XWUNLOCK();

                }
                curon(win); /* show the cursor */

            }

            /* now move forward */
            if (spc > ss) sc->curxg += ss; /* space off */
            else { sc->curxg += spc; ss -= spc; }

        } else plcchr(win, s[i]); /* print the character with natural spacing */

    }

}

/** ****************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

*******************************************************************************/

void _pa_justpos_ovr(ami_justpos_t nfp, ami_justpos_t* ofp)
    { *ofp = justpos_vect; justpos_vect = nfp; }
int ami_justpos(FILE* f, const char* s, int p, int n)
    { return ((*justpos_vect)(f, s, p, n)); }

static int justpos_ivf(FILE* f, const char* s, int p, int n)

{

    winptr win; /* window pointer */
    scnptr sc;  /* pointer to update screen */
    int    spc; /* space of spaces */
    int    ns;  /* number of spaces */
    int    ss;  /* spaces total size */
    int    sz;  /* critical size, chars+min space */
    int    cs;  /* size of characters only */
    int    cbs; /* character background spacing */
    int    cp;  /* character pixel position */
    int    crp; /* character result position */
    int    i;
    int    l;

    win = txt2win(f); /* get window pointer from text file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (sc->autof) error(eatopos); /* cannot perform with auto on */
    l = strlen(s); /* find string length */
    if (p < 0 || p >= strlen(s))  error(estrinx); /* out of range */
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
    /* if space provided is greater than the minimum, distribute the extra space
       amoung the existing spaces */
    ss = ns*MINJST; /* set minimum distribution of space */
    if (n > sz) { spc = (n-cs)/ns; ss = n-cs; }
    cp = 0; /* set 0 offset to character */
    crp = 0; /* clear result position */
    /* Output the string with our choosen spacing */
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

/** ****************************************************************************

Turn on condensed attribute

Turns on/off the condensed attribute. Condensed is a character set with a
shorter baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void _pa_condensed_ovr(ami_condensed_t nfp, ami_condensed_t* ofp)
    { *ofp = condensed_vect; condensed_vect = nfp; }
void ami_condensed(FILE* f, int e) { (*condensed_vect)(f, e); }

static void condensed_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sacondensed); /* set attribute active */
       win->gattr |= BIT(sacondensed);

    } else { /* turn it off */

       sc->attr &= ~BIT(sacondensed); /* set attribute inactive */
       win->gattr &= ~BIT(sacondensed);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on extended attribute

Turns on/off the extended attribute. Extended is a character set with a
longer baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void _pa_extended_ovr(ami_extended_t nfp, ami_extended_t* ofp)
    { *ofp = extended_vect; extended_vect = nfp; }
void ami_extended(FILE* f, int e) { (*extended_vect)(f, e); }

static void extended_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saextended); /* set attribute active */
       win->gattr |= BIT(saextended);

    } else { /* turn it off */

       sc->attr &= ~BIT(saextended); /* set attribute inactive */
       win->gattr &= ~BIT(saextended);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on extra light attribute

Turns on/off the extra light attribute. Extra light is a character thiner than
light.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void _pa_xlight_ovr(ami_xlight_t nfp, ami_xlight_t* ofp)
    { *ofp = xlight_vect; xlight_vect = nfp; }
void ami_xlight(FILE* f, int e) { (*xlight_vect)(f, e); }

static void xlight_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saxlight); /* set attribute active */
       win->gattr |= BIT(saxlight);

    } else { /* turn it off */

       sc->attr &= ~BIT(saxlight); /* set attribute inactive */
       win->gattr &= ~BIT(saxlight);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on light attribute

Turns on/off the light attribute. Light is a character thiner than normal
characters in the currnet font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void _pa_light_ovr(ami_light_t nfp, ami_light_t* ofp)
    { *ofp = light_vect; light_vect = nfp; }
void ami_light(FILE* f, int e) { (*light_vect)(f, e); }

static void light_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(salight); /* set attribute active */
       win->gattr |= BIT(salight);

    } else { /* turn it off */

       sc->attr &= ~BIT(salight); /* set attribute inactive */
       win->gattr &= ~BIT(salight);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on extra bold attribute

Turns on/off the extra bold attribute. Extra bold is a character thicker than
bold.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void _pa_xbold_ovr(ami_xbold_t nfp, ami_xbold_t* ofp)
    { *ofp = xbold_vect; xbold_vect = nfp; }
void ami_xbold(FILE* f, int e) { (*xbold_vect)(f, e); }

static void xbold_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saxbold); /* set attribute active */
       win->gattr |= BIT(saxbold);

    } else { /* turn it off */

       sc->attr &= ~BIT(saxbold); /* set attribute inactive */
       win->gattr &= ~BIT(saxbold);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on hollow attribute

Turns on/off the hollow attribute. Hollow is an embossed or 3d effect that
makes the characters appear sunken into the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void _pa_hollow_ovr(ami_hollow_t nfp, ami_hollow_t* ofp)
    { *ofp = hollow_vect; hollow_vect = nfp; }
void ami_hollow(FILE* f, int e) { (*hollow_vect)(f, e); }

static void hollow_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sahollow); /* set attribute active */
       win->gattr |= BIT(sahollow);

    } else { /* turn it off */

       sc->attr &= ~BIT(sahollow); /* set attribute inactive */
       win->gattr &= ~BIT(sahollow);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on raised attribute

Turns on/off the raised attribute. Raised is an embossed or 3d effect that
makes the characters appear coming off the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void _pa_raised_ovr(ami_raised_t nfp, ami_raised_t* ofp)
    { *ofp = raised_vect; raised_vect = nfp; }
void ami_raised(FILE* f, int e) { (*raised_vect)(f, e); }

static void raised_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saraised); /* set attribute active */
       win->gattr |= BIT(saraised);

    } else { /* turn it off */

       sc->attr &= ~BIT(saraised); /* set attribute inactive */
       win->gattr &= ~BIT(saraised);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Delete picture

Deletes a loaded picture.

*******************************************************************************/

void _pa_delpict_ovr(ami_delpict_t nfp, ami_delpict_t* ofp)
    { *ofp = delpict_vect; delpict_vect = nfp; }
void ami_delpict(FILE* f, int p) { (*delpict_vect)(f, p); }

static void delpict_ivf(FILE* f, int p)

{

    winptr win; /* window pointer */
    picptr pp; /* image pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */
    delpic(win, p); /* delete all of the scaled copies */

}

/** ****************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array. In this version,
only .bmp files are loaded, and those must be in 24 bit Truecolor.

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

        if (*cp == '.' || !*cp) ec = cp;
        cp++;

    }
    if (!*cp && !ec) ec = cp;
    if (cp-fnh+strlen(ext) > MAXFNM)
        error(epicftl); /* filename too large */
    strcpy(ec, ext); /* place extention */

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


    for (i = 0; i < 2; i++) i2b.b[i] = getbyt(f);

    return (i2b.i);

}

void _pa_loadpict_ovr(ami_loadpict_t nfp, ami_loadpict_t* ofp)
    { *ofp = loadpict_vect; loadpict_vect = nfp; }
void ami_loadpict(FILE* f, int p, char* fn) { (*loadpict_vect)(f, p, fn); }

static void loadpict_ivf(FILE* f, int p, char* fn)

{

    winptr win; /* window pointer */
    FILE* pf; /* picture file */
    /* signature of PNG file */
    const byte signature[8] = { 0x42, 0x4d };
    unsigned int pw; /* picture width */
    unsigned int ph; /* picture height */
    byte r, g, b; /* colors */
    int pad;
    Visual* vi;
    byte* frmdat;
    byte* pp;
    int x, y;
    unsigned int t;
    int i;
    unsigned int hs;
    picptr ip; /* image pointer */
    char fnh[MAXFNM]; /* file name holder */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    /* if the slot is already occupied, delete that picture */
    delpic(win, p);
    /* copy filename and add extension if required */
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
    t = read32(pf); /* Number of colors */
    if (t != 0) error(ebadfmt); /* should be no palette */
    read32(pf); /* important colors */
    /* read and dispose of the rest of the header */
    for (i = 0; i < hs-40; i++) getbyt(pf);

    ip = getpic(); /* get new image entry */
    ip->next = win->pictbl[p-1]; /* push to list */
    win->pictbl[p-1] = ip;
    /* set picture size */
    ip->sx = pw;
    ip->sy = ph;
    /* create image structure */
    vi = DefaultVisual(padisplay, 0); /* define direct map color */
    frmdat = (byte*)imalloc(pw*ph*4); /* allocate image frame */
    imgcnt++;
    imgtot += pw*ph*4;
    /* create truecolor image */
    XWLOCK();
    ip->xi = XCreateImage(padisplay, vi, 24, ZPixmap, 0, (char*)frmdat,
                          pw, ph, 32, 0);
    XWUNLOCK();

    /* find end of row padding */
    pad = 0;
    if (pw*3%4) pad = 4-(pw*3%4);
    pp = frmdat+pw*ph*4-pw*4; /* index last line */
    /* fill picture with data */
    for (y = ph-1; y >= 0; y--) { /* fill bottom to top */

        for (x = 0; x < pw; x++) { /* fill left to right */

            *pp++ = getbyt(pf); /* get blue */
            *pp++ = getbyt(pf); /* get green */
            *pp++ = getbyt(pf); /* get red */
            pp++; /* skip alpha */

        }
        /* remove padding */
        for (i = 0; i < pad; i++) getbyt(pf);
        pp -= pw*4*2; /* go back one line */

    }
    fclose(pf); /* close the input file */

}

/** ****************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

*******************************************************************************/

void _pa_pictsizx_ovr(ami_pictsizx_t nfp, ami_pictsizx_t* ofp)
    { *ofp = pictsizx_vect; pictsizx_vect = nfp; }
int ami_pictsizx(FILE* f, int p) { return ((*pictsizx_vect)(f, p)); }

static int pictsizx_ivf(FILE* f, int p)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */

    return (win->pictbl[p-1]->sx); /* return x size */

}

/** ****************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

*******************************************************************************/

void _pa_pictsizy_ovr(ami_pictsizy_t nfp, ami_pictsizy_t* ofp)
    { *ofp = pictsizy_vect; pictsizy_vect = nfp; }
int ami_pictsizy(FILE* f, int p) { return ((*pictsizy_vect)(f, p)); }

static int pictsizy_ivf(FILE* f, int p)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */

    return (win->pictbl[p-1]->sy); /* return x size */

}

/** ****************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

*******************************************************************************/

void _pa_picture_ovr(ami_picture_t nfp, ami_picture_t* ofp)
    { *ofp = picture_vect; picture_vect = nfp; }
void ami_picture(FILE* f, int p, int x1, int y1, int x2, int y2)
    { (*picture_vect)(f, p, x1, y1, x2, y2); }

static void picture_ivf(FILE* f, int p, int x1, int y1, int x2, int y2)

{

    winptr  win; /* window record pointer */
    scnptr  sc;  /* screen buffer */
    int     tx, ty; /* temps */
    int     pw, ph; /* picture width and height */
    picptr  pp, fp; /* picture entry pointers */
    byte*   frmdat;
    Visual* vi;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

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

        /* New scale does not match any previous. We create a new scaled
           image. */
        pp = win->pictbl[p-1]; /* index top picture */
        while (pp->next) pp = pp->next; /* go to bottom of list */
        fp = getpic(); /* get new image entry */
        fp->next = win->pictbl[p-1]; /* push to list */
        win->pictbl[p-1] = fp;
        /* set picture size */
        fp->sx = pw;
        fp->sy = ph;
        /* create image structure */
        vi = DefaultVisual(padisplay, 0); /* define direct map color */
        frmdat = (byte*)imalloc(pw*ph*4); /* allocate image frame */
        imgcnt++;
        imgtot += pw*ph*4;
        /* create truecolor image */
        XWLOCK();
        fp->xi = XCreateImage(padisplay, vi, 24, ZPixmap, 0, (char*)frmdat,
                              pw, ph, 32, 0);
        XWUNLOCK();
        rescale(fp->xi, pp->xi); /* rescale to new image */

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the picture */
        XWLOCK();
        XPutImage(padisplay, sc->xbuf, sc->xcxt, fp->xi, 0, 0,
                  L2PX(win, x1-1), L2PY(win, y1-1),
                  L2PW(win, x2-x1+1), L2PH(win, y2-y1+1));
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the rectangle */
        XWLOCK();
        XPutImage(padisplay, win->xwhan, sc->xcxt, fp->xi, 0, 0,
                  L2PX(win, x1-1), L2PY(win, y1-1),
                  L2PW(win, x2-x1+1), L2PH(win, y2-y1+1));
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-maxint to maxint.

*******************************************************************************/

void _pa_viewoffg_ovr(ami_viewoffg_t nfp, ami_viewoffg_t* ofp)
    { *ofp = viewoffg_vect; viewoffg_vect = nfp; }
void ami_viewoffg(FILE* f, int x, int y) { (*viewoffg_vect)(f, x, y); }

static void viewoffg_ivf(FILE* f, int x, int y)

{

    winptr win;

    win = txt2win(f);
    win->goffx = x;
    win->goffy = y;

}

/** ****************************************************************************

Set viewport scale

Sets the viewport scale factors in x and y. The scale is a positive floating
point multiplier applied to logical drawing coordinates to produce physical
pixel positions. A scale of 1.0 is identity (no scaling). Scales greater than
1.0 magnify; scales less than 1.0 shrink.

The scale applies to user drawing primitives (lines, figures, pictures, text,
cursor, child window placement) but does not affect UI chrome (title bars,
frame decorations, menus, widget internals) or mouse coordinate reporting.
Applications that need to convert mouse coordinates from physical to logical
can use ami_scalex() and ami_scaley().

Asymmetric scaling (different x and y factors) is supported via FreeType's
asymmetric pixel size facility, which stretches glyph outlines independently
in each axis before rasterization. charspace and linespace track the
asymmetric scale automatically.

*******************************************************************************/

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

/** ****************************************************************************

Convert physical pixel coordinate to logical drawing coordinate

Applies the inverse of the current viewscale/viewoff transform. Used to map
mouse coordinates (which stay in physical pixels for UI purposes) back to the
logical drawing space so applications can hit-test or position against a
scaled drawing.

*******************************************************************************/

void _pa_scalex_ovr(ami_scalex_t nfp, ami_scalex_t* ofp)
    { *ofp = scalex_vect; scalex_vect = nfp; }
int ami_scalex(FILE* f, int x) { return ((*scalex_vect)(f, x)); }

static int scalex_ivf(FILE* f, int x)

{

    winptr win;

    win = txt2win(f);
    if (win->vsx == 0.0f) return x; /* defensive */
    return (int)(((float)(x - win->goffx)) / win->vsx);

}

void _pa_scaley_ovr(ami_scaley_t nfp, ami_scaley_t* ofp)
    { *ofp = scaley_vect; scaley_vect = nfp; }
int ami_scaley(FILE* f, int y) { return ((*scaley_vect)(f, y)); }

static int scaley_ivf(FILE* f, int y)

{

    winptr win;

    win = txt2win(f);
    if (win->vsy == 0.0f) return y;
    return (int)(((float)(y - win->goffy)) / win->vsy);

}

/** ****************************************************************************

Acquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle always used.

The event loop for X and the event loop for Petit-Ami are similar. Its not a
coincidence. I designed it after a description I read of the X system in 1997.
Our event loop here is like an event to event translation.

*******************************************************************************/

/* get and process a joystick event */
static void joyevt(ami_evtrec* er, int* keep, joyptr jp)

{

#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X */
    struct js_event ev;

    read(jp->fid, &ev, sizeof(ev)); /* get next joystick event */
    if (!(ev.type & JS_EVENT_INIT)) {

        if (ev.type & JS_EVENT_BUTTON) {

            /* we use Linux button numbering, because, what the heck */
            if (ev.value) { /* assert */

                er->etype = ami_etjoyba; /* set assert */
                er->winid = 0; /* set impossible window number */
                er->ajoyn = jp->no; /* set logical joystick number */
                er->ajoybn = ev.number+1; /* set button number */

            } else { /* deassert */

                er->etype = ami_etjoybd; /* set assert */
                er->winid = 0; /* set impossible window number */
                er->djoyn = jp->no; /* set logical joystick number */
                er->djoybn = ev.number+1; /* set button number */

            }
            *keep = TRUE; /* set keep event */

        }
        if (ev.type & JS_EVENT_AXIS) {

            /* update the axies */
            if (ev.number == 0) jp->ax = ev.value*(INT_MAX/32768);
            else if (ev.number == 1) jp->ay = ev.value*(INT_MAX/32768);
            else if (ev.number == 2) jp->az = ev.value*(INT_MAX/32768);
            else if (ev.number == 3) jp->a4 = ev.value*(INT_MAX/32768);
            else if (ev.number == 4) jp->a5 = ev.value*(INT_MAX/32768);
            else if (ev.number == 5) jp->a6 = ev.value*(INT_MAX/32768);

            /* we support up to 6 axes on a joystick. After 6, they get thrown
               out, leaving just the buttons to respond */
            if (ev.number < 6) {

                er->etype = ami_etjoymov; /* set joystick move */
                er->winid = 0; /* set impossible window number */
                er->mjoyn = jp->no; /* set joystick number */
                er->joypx = jp->ax; /* place joystick axies */
                er->joypy = jp->ay;
                er->joypz = jp->az;
                er->joyp4 = jp->a4;
                er->joyp5 = jp->a5;
                er->joyp6 = jp->a6;
                *keep = TRUE; /* set keep event */

            }

        }

    }
#endif

}

/* Update mouse parameters.
   Use state flags to create events. */

static void mouseupdate(winptr win, ami_evtrec* er, int* keep)

{

    /* we prioritize events by: movements 1st, button clicks 2nd */
    if (win->nmpx != win->mpx || win->nmpy != win->mpy) {

        /* create movement event */
        er->etype = ami_etmoumov; /* set movement event */
        er->mmoun = 1; /* mouse 1 */
        er->moupx = win->nmpx; /* set new mouse position */
        er->moupy = win->nmpy;
        win->mpx = win->nmpx; /* save new position */
        win->mpy = win->nmpy;
        *keep = TRUE; /* set to keep */

    } else if (win->nmpxg != win->mpxg || win->nmpyg != win->mpyg) {

        /* create graphical movement event */
        er->etype = ami_etmoumovg; /* set movement event */
        er->mmoung = 1; /* mouse 1 */
        er->moupxg = win->nmpxg; /* set new mouse position */
        er->moupyg = win->nmpyg;
        win->mpxg = win->nmpxg; /* save new position */
        win->mpyg = win->nmpyg;
       *keep = TRUE; /* set to keep */

    } else if (win->nmb1 > win->mb1) {

       er->etype = ami_etmouba; /* button 1 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 1; /* button 1 */
       win->mb1 = win->nmb1; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb2 > win->mb2) {

       er->etype = ami_etmouba; /* button 2 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 2; /* button 2 */
       win->mb2 = win->nmb2; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb3 > win->mb3) {

       er->etype = ami_etmouba; /* button 3 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 3; /* button 3 */
       win->mb3 = win->nmb3; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb4 > win->mb4) {

       er->etype = ami_etmouba; /* button 4 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 4; /* button 3 */
       win->mb4 = win->nmb4; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb5 > win->mb5) {

       er->etype = ami_etmouba; /* button 5 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 5; /* button 3 */
       win->mb5 = win->nmb5; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb1 < win->mb1) {

       er->etype = ami_etmoubd; /* button 1 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 1; /* button 1 */
       win->mb1 = win->nmb1; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb2 < win->mb2) {

       er->etype = ami_etmoubd; /* button 2 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 2; /* button 2 */
       win->mb2 = win->nmb2; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb3 < win->mb3) {

       er->etype = ami_etmoubd; /* button 3 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 3; /* button 3 */
       win->mb3 = win->nmb3; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb4 < win->mb4) {

       er->etype = ami_etmoubd; /* button 4 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 4; /* button 3 */
       win->mb4 = win->nmb4; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb5 < win->mb5) {

       er->etype = ami_etmoubd; /* button 5 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 5; /* button 3 */
       win->mb5 = win->nmb5; /* update status */
       *keep = TRUE; /* set to keep */

    }

}

/* Register mouse status.
   Get mouse status from XWindow event to window data flags. */

static void mouseevent(winptr win, XEvent* e)

{

    if (e->type == MotionNotify) {

        win->nmpx = e->xmotion.x/win->charspace+1; /* get mouse x */
        win->nmpy = e->xmotion.y/win->linespace+1; /* get mouse y */
        win->nmpxg = e->xmotion.x+1; /* get mouse graphical x */
        win->nmpyg = e->xmotion.y+1; /* get mouse graphical y */

    } else if (e->type == ButtonPress) {

        if (e->xbutton.button == Button1) win->nmb1 = TRUE;
        else if (e->xbutton.button == Button2) win->nmb2 = TRUE;
        else if (e->xbutton.button == Button3) win->nmb3 = TRUE;
        else if (e->xbutton.button == Button4) win->nmb4 = TRUE;
        else if (e->xbutton.button == Button5) win->nmb5 = TRUE;

    } else if (e->type == ButtonRelease) {

        if (e->xbutton.button == Button1) win->nmb1 = FALSE;
        else if (e->xbutton.button == Button2) win->nmb2 = FALSE;
        else if (e->xbutton.button == Button3) win->nmb3 = FALSE;
        else if (e->xbutton.button == Button4) win->nmb4 = FALSE;
        else if (e->xbutton.button == Button5) win->nmb5 = FALSE;

    }

}

/* rectangle */
typedef struct { int x1, y1, x2, y2; } rectangle;

/* set rectangle to values */
void setrect(rectangle* r, int x1, int y1, int x2, int y2)

{

    r->x1 = x1;
    r->y1 = y1;
    r->x2 = x2;
    r->y2 = y2;

}

/* find if rectangles intersect */
int intersect(rectangle* r1, rectangle* r2)

{

    return ((*r1).x2 >= (*r2).x1 && (*r1).x1 <= (*r2).x2 &&
            (*r1).y2 >= (*r2).y1 && (*r1).y1 <= (*r2).y2);

}

/* find intersection of rectangles as a rectangle (meaningless if they don't
   intersect) */
void intersection(rectangle* ri, rectangle* r1, rectangle* r2)

{

    /* copy to destination */
    ri->x1 = r1->x1;
    ri->x2 = r1->x2;
    ri->y1 = r1->y1;
    ri->y2 = r1->y2;

    /* find intersection */
    if (r1->x1 < r2->x1) ri->x1 = r2->x1;
    if (r1->x2 > r2->x2) ri->x2 = r2->x2;
    if (r1->y1 < r2->y1) ri->y1 = r2->y1;
    if (r1->y2 > r2->y2) ri->y2 = r2->y2;

}

/* find rectangle is null */

int zerorect(rectangle* r)

{

    return (!(r->x1 | r->x2 | r->y1 | r->y2));

}

/* Subtract rectangles. Relies on the rectangles to be rational and
   intersecting. */
void subrect(rectangle* r1, rectangle* r2, rectangle* rr, rectangle* rb)

{

    /* right full */
    rr->x1 = r1->x2+1;
    rr->x2 = r2->x2;
    rr->y1 = r2->y1;
    rr->y2 = r2->y2;

    /* bottom partial */
    rb->x1 = r2->x1;
    rb->x2 = r2->x2;
    rb->y1 = r1->y2+1;
    rb->y2 = r2->y2;

    /* if null, zero out */
    if (rr->x1 > rr->x2) rr->x1 = rr->x2 = rr->y1 = rr->y2 = 0;
    if (rb->y1 > rb->y2) rb->x1 = rb->x2 = rb->y1 = rb->y2 = 0;

}

/* print tree focus
   A diagnostic, prints the focus for all windows in the given
   tree. */
static void prtfocus(int level, winptr win)

{

    while (win) {

        dbg_printf(dlinfo, "%*cWindow: %d focus: %d\n", level, ' ', win->wid, win->focus);
        /* find focus in children of this window */
        prtfocus(level+4, win->childwin);
        win = win->childlst; /* link next child */

    }

}

/* find root window
   Find the root, or ultimate parent, of a given window. */

static winptr root(winptr win)

{

    while (win->parwin) win = win->parwin;

    return (win);

}

/* find focus in subtree. Searches the given tree for the window marked as
   having focus, and returns that, or NULL. */
static winptr fndfocus(winptr win)

{

    winptr fwin; /* focus window found */

    fwin = NULL; /* set no focus window found */
    while (win && !fwin) {

        if (win->focus) fwin = win; /* found focus */
        /* find focus in children of this window */
        else fwin = fndfocus(win->childwin);
        win = win->childlst; /* link next child */

    }

    return (fwin); /* exit with focus found */

}

/* Remove focus from window. Searches from the given window down the tree until
   the window containing focus is found. The focus is them removed by sending a
   defocus event, removing focus status, and repainting the cursor.
   If a focus window is provided, will not take the focus from that. This is
   to prevent "bouncing" the focus. */
static int remfocus(winptr win, winptr curwin)

{

    ami_evtrec er;   /* event record */

    win = fndfocus(win); /* find focused window */
    if (win && win != curwin) { /* found */

        /* found the focus window */
        er.etype = ami_etnofocus; /* send defocus to parent */
        isendevent(win, &er); /* send it */
        curoff(win); /* remove cursor */
        win->focus = FALSE; /* remove focus */
        curon(win); /* replace cursor */

    }

    return (!!win); /* exit with focus found */

}

/* process Xwindow status messages */
static void winstat(winptr win, ami_evtrec* er, XEvent* e, int* keep)

{

    int               maxhorz;
    int               maxvert;
    int               focused;
    int               hidden;
    int               status;
    Atom              prop;
    Atom              type;
    int               format;
    unsigned long     length;
    unsigned long     after;
    unsigned char*    dp;
    ami_evtrec         er2;
    XWindowAttributes xwa; /* XWindow attributes */
    XEvent            xe;  /* XWindow event */
    int               m;

    XWLOCK();
    m = !strcmp(XGetAtomName(padisplay, e->xproperty.atom), "_NET_WM_STATE");
    XWUNLOCK();
    if (m) {

        after = 1L;
        focused = 0;
        maxhorz = 0;
        maxvert = 0;
        hidden = 0;
        do {

            XWLOCK();
            status = XGetWindowProperty(padisplay, win->xmwhan, e->xproperty.atom,
                                        0L, after, 0,
                                        4/*XA_ATOM*/, &type, &format,
                                        &length, &after, &dp);
            XWUNLOCK();
            if (status == Success && type == 4/*XA_ATOM*/ && dp && format == 32 && length) {

                for (int i = 0; i < length; i++) {

                    prop = ((Atom*)dp)[i];

                    if (prop == cfocused) focused = 1;
                    if (prop == cmaxhorz) maxhorz = 1;
                    if (prop == cmaxvert) maxvert = 1;
                    if (prop == chidden) hidden = 1;

                }

            }

        } while (after);
        if (hidden) {

            win->lwinstate = win->winstate;
            win->winstate = 2;
            if (win->lwinstate != win->winstate) {

                er->etype = ami_etmin; /* set minimize event */
                *keep = TRUE;

            }

        } else if (maxhorz || maxvert) {

            win->lwinstate = win->winstate;
            win->winstate = 1;
            if (win->lwinstate != win->winstate) {

                er->etype = ami_etmax; /* set maximize event */
                *keep = TRUE;

            }

        } else if (focused) {

            win->lwinstate = win->winstate;
            win->winstate = 0;
            if (win->lwinstate != win->winstate) {

                er->etype = ami_etnorm; /* set normalize event */
                *keep = TRUE;

            }

        }

    }

}

/* XWindow event process */

static void xwinevt(winptr win, ami_evtrec* er, XEvent* e, int* keep)

{

    KeySym         ks;
    scnptr         sc;  /* screen pointer */
    rectangle      r1, r2, ri, rr, rb;
    XWindowChanges xwc; /* XWindow values */
    XEvent         xe;
    winptr         mwin;
    ami_evtrec      er2;
    winptr         wp;
    int            ff;  /* found focus flag */
    winptr         fwin; /* focus window */

    sc = win->screens[win->curdsp-1]; /* index screen */

    /* handle Expose on xmwhan for child-framed windows: repaint the frame */
    if (e->type == Expose && win->childfrm &&
        win->xmwhan == e->xany.window) {

        childfrm_draw(win);

    } else if (e->type == Expose && win->xmwhan != e->xany.window) {

        if (win->bufmod) { /* use buffer to satisfy event */

            curoff(win); /* remove cursor */
            /* make expose mask into rectangle */
            setrect(&r1, e->xexpose.x, e->xexpose.y,
                    e->xexpose.x+e->xexpose.width-1,
                    e->xexpose.y+e->xexpose.height-1);
            /* make buffer into 0,0 rectangle */
            setrect(&r2, 0, r2.y1 = 0, win->gmaxxg-1, win->gmaxyg-1);
            if (intersect(&r1, &r2)) {

                intersection(&ri, &r1, &r2); /* find intersection of those */
                XWLOCK();
                XCopyArea(padisplay, sc->xbuf, win->xwhan, sc->xcxt, ri.x1, ri.y1,
                      ri.x2-ri.x1+1, ri.y2-ri.y1+1, ri.x1, ri.y1);
                subrect(&r2, &r1, &rr, &rb); /* find subtraction r1-r2 */
                /* check any result */
                if (!zerorect(&rr) || !zerorect(&rb)) {

                    /* set background color to foreground */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    /* paint right */
                    if (!zerorect(&rr))
                        XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                                       rr.x1, rr.y1,
                                       rr.x2-rr.x1+1, rr.y2-rr.y1+1);
                    /* paint bottom */
                    if (!zerorect(&rb))
                        XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                                       rb.x1, rb.y1,
                                       rb.x2-rb.x1+1, rb.y2-rb.y1+1);
                    /* restore foreground color */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    /* we shouldn't need to do this, but I have seen unpainted
                       of the window if not while resizing */
                    XFlush(padisplay);

                }
                XWUNLOCK();

            } else {

                /* paint right or bottom off buffer space */
                XWLOCK();
                /* set background color to foreground */
                if (BIT(sarev) & sc->attr)
                    XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                               e->xexpose.x, e->xexpose.y,
                               e->xexpose.width, e->xexpose.height);
                /* restore foreground color */
                if (BIT(sarev) & sc->attr)
                    XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                XFlush(padisplay);
                XWUNLOCK();

            }
            curon(win); /* replace cursor */

        } else { /* let the client handle it */

            er->etype = ami_etredraw; /* set redraw event */
            er->rsx = e->xexpose.x+1; /* set redraw rectangle */
            er->rsy = e->xexpose.y+1;
            er->rex = e->xexpose.x+e->xexpose.width;
            er->rey = e->xexpose.y+e->xexpose.height;
            *keep = TRUE; /* set found */

        }

    } else if (e->type == ConfigureNotify) {

        if (win->xmwhan == e->xany.window) { /* it's the master window */

            /* update master window tracking dimensions */
            win->xmwr.w = e->xconfigure.width;
            win->xmwr.h = e->xconfigure.height;
            /* find size of subclient */
            xwc.width = e->xconfigure.width; /* set frameless offset to client */
            xwc.height = e->xconfigure.height;
            /* for child-framed windows, subtract frame thickness */
            if (win->childfrm) {

                xwc.width -= win->pfw;
                xwc.height -= win->pfh;

            }
            if (win->menu) /* if menu is active, remove that space from subclient */
                xwc.height -= win->menuspcy;
            if (xwc.width <= 0) xwc.width = 1; /* set minimum width */
            if (xwc.height <= 0) xwc.height = 1; /* set minimum height */
            /* check subclient window has changed size */
            if (xwc.width != win->xwr.w || xwc.height != win->xwr.h) {

                XWLOCK();
                if (win->childfrm) {

                    /* keep the subclient offset by the frame thickness */
                    XMoveResizeWindow(padisplay, win->xwhan,
                                      win->cwox, win->cwoy,
                                      xwc.width, xwc.height);

                } else {

                    XConfigureWindow(padisplay, win->xwhan, CWWidth|CWHeight, &xwc);

                }
                XWUNLOCK();
#ifdef WAITWMR
                /* wait for the configure response with correct sizes */
                do { peekxevt(&xe); /* peek next event */
                } while (xe.type != ConfigureNotify || xe.xconfigure.width != xwc.width ||
                         xe.xconfigure.height != xwc.height || xe.xany.window != win->xwhan);
#endif
                /* change saved size to match */
                win->xwr.w = xwc.width;
                win->xwr.h = xwc.height;

            }

            /* if menu bar is active, also send a configure to it */
            if (win->menu) {

                mwin = txt2win(win->menu->wf); /* index window */
                /* find resulting size of menu bar */
                xwc.width = e->xconfigure.width; /* width is client */
                xwc.height = win->menuspcy; /* height is menu text */
                /* check menu bar has changed size */
                if (xwc.width != mwin->xmwr.w || xwc.height != mwin->xmwr.h) {

                    XWLOCK();
                    XConfigureWindow(padisplay, mwin->xmwhan, CWWidth|CWHeight, &xwc);
                    XWUNLOCK();
#ifdef WAITWMR
                    /* wait for the configure response with correct sizes */
                    do { peekxevt(&xe); /* peek next event */
                    } while (xe.type != ConfigureNotify || xe.xconfigure.width != xwc.width ||
                             xe.xconfigure.height != xwc.height || xe.xany.window != mwin->xmwhan);
#endif
                    /* change saved size to match */
                    mwin->xmwr.w = xwc.width;
                    mwin->xmwr.h = xwc.height;

                }

            }

            /* redraw child frame after master resize */
            if (win->childfrm) childfrm_draw(win);

        } else { /* its the subclient window */

            /* size of window has changed, send event */
            er->etype = ami_etresize; /* set resize event */
            er->rszxg = e->xconfigure.width; /* set graphics size */
            er->rszyg = e->xconfigure.height;
            er->rszx = e->xconfigure.width/win->charspace; /* set character size */
            er->rszy = e->xconfigure.height/win->linespace;
            *keep = TRUE; /* set found */
            if (!win->bufmod) {

                /* reset tracking sizes */
                win->gmaxxg = er->rszxg; /* graphics x */
                win->gmaxyg = er->rszyg; /* graphics y */
                /* find character size x */
                win->gmaxx = win->gmaxxg/win->charspace;
                /* find character size y */
                win->gmaxy = win->gmaxyg/win->linespace;
                /* set screen sizes */
                win->screens[win->curdsp-1]->maxxg = win->gmaxxg; /* pixel size */
                win->screens[win->curdsp-1]->maxyg = win->gmaxyg;
                win->screens[win->curdsp-1]->maxx = win->gmaxx; /* character size */
                win->screens[win->curdsp-1]->maxy = win->gmaxy;

            }

        }

    } else if (e->type == KeyPress) {

        XWLOCK();
        ks = XLookupKeysym(&e->xkey, ((e->xkey.state & ShiftMask) == 0 ? 0 : 1));
        XWUNLOCK();
        er->etype = ami_etchar; /* place default code */
        fwin = win; /* set parent to self */
        if (!fwin) fwin = win->parwin; /* set parent of child window */
        fwin = fndfocus(fwin); /* find focus window */
        if (fwin) er->winid = fwin->wid; /* send keys to focus window */
        if (ks >= ' ' && ks <= 0x7e && !ctrll && !ctrlr && !altl && !altr) {

            /* issue standard key event */
            er->etype = ami_etchar; /* set event */
            /* set code, normal or shifted */
            if (shiftl || shiftr) er->echar = !capslock?toupper(ks):ks;
            else er->echar = capslock?toupper(ks):ks; /* set code */
            *keep = TRUE; /* set found */

        } else {

            switch (ks) {

                /* process control characters */
                case XK_BackSpace: er->etype = ami_etdelcb; break;
                case XK_Tab:       er->etype = ami_ettab; break;
                case XK_Return:    er->etype = ami_etenter; break;
                case XK_Escape:    if (esck)
                                       { er->etype = ami_etcan; esck = FALSE; }
                                   else esck = TRUE;
                                   break;
                case XK_Delete:    if (shiftl || shiftr) er->etype = ami_etdel;
                                   else if (ctrll || ctrlr) er->etype = ami_etdell;
                                   else er->etype = ami_etdelcf;
                                   break;

                case XK_Home:      if (ctrll || ctrlr) er->etype = ami_ethome;
                                   else er->etype = ami_ethomel;
                                   break;
                case XK_Left:      if (ctrll || ctrlr) er->etype = ami_etleftw;
                                   else er->etype = ami_etleft;
                                   break;
                case XK_Up:        if (ctrll || ctrlr) er->etype = ami_etscru;
                                   else er->etype = ami_etup;
                                   break;
                case XK_Right:     if (ctrll || ctrlr) er->etype = ami_etrightw;
                                   else er->etype = ami_etright; break;
                case XK_Down:      if (ctrll || ctrlr) er->etype = ami_etscrd;
                                   else er->etype = ami_etdown;
                                   break;
                case XK_Page_Up:   if (ctrll || ctrlr) er->etype = ami_etscrl;
                                   else er->etype = ami_etpagu;
                                   break;
                case XK_Page_Down: if (ctrll || ctrlr) er->etype = ami_etscrr;
                                   else er->etype = ami_etpagd;
                                   break;
                case XK_End:       if (ctrll || ctrlr) er->etype = ami_etend;
                                   else er->etype = ami_etendl;
                                   break;

                case XK_Insert:    er->etype = ami_etinsertt; break;

                case XK_F1:
                case XK_F2:
                case XK_F3:
                case XK_F4:
                case XK_F5:
                case XK_F6:
                case XK_F7:
                case XK_F8:
                case XK_F9:
                case XK_F10:
                case XK_F11:
                case XK_F12:
                    /* X11 gives us all 12 function keys for our use, plus
                       are sequential */
                    er->etype = ami_etfun; /* function key */
                    er->fkey = ks-XK_F1+1;
                    break;

                case XK_C:
                case XK_c:         if (ctrll || ctrlr) {

                                       er->etype = ami_etterm;
                                       fend = TRUE;

                                   }
                                   else if (altl || altr) er->etype = ami_etcopy;
                                   break;
                case XK_S:
                case XK_s:         if (ctrll || ctrlr)
                                       er->etype = ami_etstop;
                                   break;
                case XK_Q:
                case XK_q:         if (ctrll || ctrlr)
                                       er->etype = ami_etcont;
                                   break;
                case XK_P:
                case XK_p:         if (ctrll || ctrlr)
                                       er->etype = ami_etprint;
                                   break;
                case XK_H:
                case XK_h:         if (ctrll || ctrlr)
                                       er->etype = ami_ethomes;
                                   break;
                case XK_E:
                case XK_e:         if (ctrll || ctrlr)
                                       er->etype = ami_etends;
                                   break;
                case XK_V:
                case XK_v:         if (ctrll || ctrlr)
                                       er->etype = ami_etinsert;
                                   break;

                case XK_Shift_L:   shiftl = TRUE; break; /* Left shift */
                case XK_Shift_R:   shiftr = TRUE; break; /* Right shift */
                case XK_Control_L: ctrll = TRUE; break;  /* Left control */
                case XK_Control_R: ctrlr = TRUE; break;  /* Right control */
                case XK_Alt_L:     altl = TRUE; break;  /* Left alt */
                case XK_Alt_R:     altr = TRUE; break;  /* Right alt */
                case XK_Caps_Lock: capslock = !capslock; /* Caps lock */

            }
            if (er->etype != ami_etchar)
                *keep = TRUE; /* a control was found */

        }

    } else if (e->type == KeyRelease) {

        /* Petit-ami does not track key releases, but we need to account for
          control and shift keys up/down */
        XWLOCK();
        ks = XLookupKeysym(&e->xkey, 0); /* find code */
        XWUNLOCK();
        switch (ks) {

            case XK_Shift_L:   shiftl = FALSE; break; /* Left shift */
            case XK_Shift_R:   shiftr = FALSE; break; /* Right shift */
            case XK_Control_L: ctrll = FALSE; break;  /* Left control */
            case XK_Control_R: ctrlr = FALSE; break;  /* Right control */
            case XK_Alt_L:     altl = FALSE; break;  /* Left alt */
            case XK_Alt_R:     altr = FALSE; break;  /* Right alt */

        }

    } else if (win->childfrm && win->xmwhan == e->xany.window &&
               (e->type == ButtonPress || e->type == ButtonRelease ||
                e->type == MotionNotify)) {

        /* update cursor shape based on hover position */
        if (e->type == MotionNotify) {
            childfrm_set_cursor(win, e->xmotion.x, e->xmotion.y);
        }

        /* child frame mouse interaction: title bar drag, close button, resize */
        if (e->type == ButtonPress && e->xbutton.button == Button1) {

            int mx = e->xbutton.x;
            int my = e->xbutton.y;
            int mw = win->xmwr.w;
            int tbh = CFRM_TITBAR_H(win);
            int bsz = CFRM_BUTTON_SZ(win);

            /* selecting a child: raise it to the top of the stacking
               order so it appears in front of its siblings, and
               give it focus */
            XWLOCK();
            XRaiseWindow(padisplay, win->xmwhan);
            XWUNLOCK();
#ifndef NOFAKEFOCUS
            if (!win->focus) {

                ami_evtrec er2;
                int ff;
                ff = remfocus(root(win), win);
                if (ff) {
                    er2.etype = ami_etfocus;
                    isendevent(win, &er2);
                }
                curoff(win);
                win->focus = TRUE;
                curon(win);
                childfrm_draw(win);
                XWLOCK();
                XSetInputFocus(padisplay, win->xmwhan, RevertToNone,
                               CurrentTime);
                XWUNLOCK();

            }
#endif

            /* hit test: close, max, min buttons (right-aligned) */
            int cbx = mw - bsz - CFRM_BUTTON_MG;
            int cby = (tbh - bsz) / 2;
            int mabx = cbx - bsz - CFRM_BUTTON_GAP;
            int mibx = mabx - bsz - CFRM_BUTTON_GAP;
            if (mx >= cbx && mx < cbx + bsz &&
                my >= cby && my < cby + bsz) {

                /* close button clicked: send terminate */
                er->etype = ami_etterm;
                *keep = TRUE;

            } else if (mx >= mabx && mx < mabx + bsz &&
                       my >= cby && my < cby + bsz) {

                /* max button: when minimized, restore to previous size */
                if (win->minimized) childfrm_restore(win);
                *keep = FALSE;
                return;

            } else if (mx >= mibx && mx < mibx + bsz &&
                       my >= cby && my < cby + bsz) {

                /* min button: collapse to title-bar slot at bottom of parent */
                if (!win->minimized) childfrm_minimize(win);
                *keep = FALSE;
                return;

            } else if (my >= CFRM_BORDER_W && my < tbh &&
                       mx >= CFRM_BORDER_W && mx < mw - CFRM_BORDER_W) {

                /* title bar (excluding the top resize border and side
                   resize borders that overlap the title bar): initiate drag */
                int startx = e->xbutton.x_root;
                int starty = e->xbutton.y_root;
                int origx = win->xmwr.x;
                int origy = win->xmwr.y;
                XEvent de;
                int dragging = TRUE;

                /* grab pointer for drag tracking */
                XWLOCK();
                XGrabPointer(padisplay, win->xmwhan, False,
                             ButtonReleaseMask | PointerMotionMask,
                             GrabModeAsync, GrabModeAsync,
                             None, None, CurrentTime);
                XWUNLOCK();

                while (dragging) {

                    XWLOCK();
                    XNextEvent(padisplay, &de);
                    XWUNLOCK();
                    if (de.type == MotionNotify) {

                        int dx = de.xmotion.x_root - startx;
                        int dy = de.xmotion.y_root - starty;
                        win->xmwr.x = origx + dx;
                        win->xmwr.y = origy + dy;
                        XWLOCK();
                        XMoveWindow(padisplay, win->xmwhan,
                                    win->xmwr.x, win->xmwr.y);
                        XWUNLOCK();

                    } else if (de.type == ButtonRelease) {

                        dragging = FALSE;

                    } else if (de.type == Expose) {

                        /* X has notified us that something needs repainting
                           because of our move. Dispatch to the right window. */
                        int ofn = fndevt(de.xany.window);
                        if (ofn >= 0) {

                            winptr ewin = lfn2win(ofn);
                            /* if it's a child-framed window's xmwhan,
                               redraw the frame */
                            if (ewin->childfrm &&
                                ewin->xmwhan == de.xany.window) {

                                childfrm_draw(ewin);

                            } else {

                                /* it's a buffered window content area;
                                   restore from the buffer */
                                restore(ewin);

                            }

                        }

                    }

                }
                XWLOCK();
                XUngrabPointer(padisplay, CurrentTime);
                XWUNLOCK();
                /* final repaint after release */
                if (win->parwin) {
                    winptr sib;
                    restore(win->parwin);
                    for (sib = win->parwin->childwin; sib;
                         sib = sib->childlst) {
                        if (sib == win) continue;
                        restore(sib);
                        if (sib->childfrm) childfrm_draw(sib);
                    }
                }

            } else if (!win->minimized &&
                       (my >= win->xmwr.h - CFRM_BORDER_W ||
                        mx >= win->xmwr.w - CFRM_BORDER_W ||
                        mx < CFRM_BORDER_W ||
                        my < CFRM_BORDER_W)) {

                /* resize edge: determine which edges are grabbed */
                int rsz_left   = (mx < CFRM_BORDER_W);
                int rsz_right  = (mx >= win->xmwr.w - CFRM_BORDER_W);
                int rsz_top    = (my < CFRM_BORDER_W);
                int rsz_bottom = (my >= win->xmwr.h - CFRM_BORDER_W);
                int startx = e->xbutton.x_root;
                int starty = e->xbutton.y_root;
                int origx = win->xmwr.x;
                int origy = win->xmwr.y;
                int origw = win->xmwr.w;
                int origh = win->xmwr.h;
                XEvent de;
                int resizing = TRUE;

                XWLOCK();
                XGrabPointer(padisplay, win->xmwhan, False,
                             ButtonReleaseMask | PointerMotionMask,
                             GrabModeAsync, GrabModeAsync,
                             None, None, CurrentTime);
                XWUNLOCK();

                while (resizing) {

                    XWLOCK();
                    XNextEvent(padisplay, &de);
                    XWUNLOCK();
                    if (de.type == MotionNotify) {

                        int dx = de.xmotion.x_root - startx;
                        int dy = de.xmotion.y_root - starty;
                        int nx = origx;
                        int ny = origy;
                        int nw = origw;
                        int nh = origh;
                        if (rsz_right)  nw = origw + dx;
                        if (rsz_bottom) nh = origh + dy;
                        if (rsz_left) {
                            nw = origw - dx;
                            nx = origx + dx;
                        }
                        if (rsz_top) {
                            nh = origh - dy;
                            ny = origy + dy;
                        }
                        /* enforce minimums; if hitting min on left/top edge,
                           keep the opposite edge fixed */
                        if (nw < win->pfw + 40) {
                            if (rsz_left) nx -= (win->pfw + 40 - nw);
                            nw = win->pfw + 40;
                        }
                        if (nh < win->pfh + 40) {
                            if (rsz_top) ny -= (win->pfh + 40 - nh);
                            nh = win->pfh + 40;
                        }
                        int shrunk = (nw < win->xmwr.w || nh < win->xmwr.h);
                        win->xmwr.x = nx;
                        win->xmwr.y = ny;
                        win->xmwr.w = nw;
                        win->xmwr.h = nh;
                        win->xwr.w = nw - win->pfw;
                        win->xwr.h = nh - win->pfh;
                        XWLOCK();
                        if (nx != origx || ny != origy) {
                            XMoveResizeWindow(padisplay, win->xmwhan,
                                              nx, ny, nw, nh);
                        } else {
                            XResizeWindow(padisplay, win->xmwhan, nw, nh);
                        }
                        XResizeWindow(padisplay, win->xwhan,
                                      win->xwr.w, win->xwr.h);
                        XWUNLOCK();
                        /* the client area may now be larger than the buffer;
                           restore() copies the buffer to the screen and fills
                           any uncovered margin with the background color */
                        restore(win);
                        childfrm_draw(win);
                        /* if the child shrank, repaint the parent and
                           sibling children where this child used to be */
                        if (shrunk && win->parwin) {
                            winptr sib;
                            restore(win->parwin);
                            for (sib = win->parwin->childwin; sib;
                                 sib = sib->childlst) {
                                if (sib == win) continue;
                                restore(sib);
                                if (sib->childfrm) childfrm_draw(sib);
                            }
                        }

                    } else if (de.type == ButtonRelease) {

                        resizing = FALSE;

                    }

                }
                XWLOCK();
                XUngrabPointer(padisplay, CurrentTime);
                XWUNLOCK();
                if (win->parwin) {
                    winptr sib;
                    restore(win->parwin);
                    for (sib = win->parwin->childwin; sib;
                         sib = sib->childlst) {
                        if (sib == win) continue;
                        restore(sib);
                        if (sib->childfrm) childfrm_draw(sib);
                    }
                }

                /* send resize event to the client */
                er->etype = ami_etresize;
                er->rszx = win->xwr.w / win->charspace;
                er->rszy = win->xwr.h / win->linespace;
                er->rszxg = win->xwr.w;
                er->rszyg = win->xwr.h;
                *keep = TRUE;

            }

        }

    } else if ((e->type == MotionNotify || e->type == ButtonPress ||
               e->type == ButtonRelease) && mouseenb) {

        mouseevent(win, e); /* process mouse event */
        /* check any mouse details need processing */
        mouseupdate(win, er, keep);
#ifndef NOFAKEFOCUS
        /* Check we fake focus here. This is a mouse button 1 click in a child
           window. */
        if (*keep && er->etype == ami_etmouba && er->amoubn == 1 && !win->focus) {

            /* We will fake focus. We still need to process the mouse click, so
               we will send the focus event on ahead. */
            ff = remfocus(root(win), win); /* remove focus from whatever window has it */
            if (!ff) error(esystem); /* couldn't find focus window */
            er2.etype = ami_etfocus; /* set focus event */
            isendevent(win, &er2); /* send it */
            curoff(win); /* remove cursor */
            win->focus = TRUE; /* put focus */
            curon(win); /* replace cursor */
            if (win->childfrm) childfrm_draw(win);
            XWLOCK();
            XSetInputFocus(padisplay, win->xmwhan, RevertToNone, CurrentTime);
            XWUNLOCK();

        }
#endif

    } else if (e->type == FocusOut) {

        remfocus(root(win), NULL); /* remove focus from child window if it has it */
        curoff(win); /* remove cursor */
        win->focus = FALSE; /* remove focus */
        curon(win); /* replace cursor */
        if (win->childfrm) childfrm_draw(win);
        er->etype = ami_etnofocus; /* set no focus event */
        *keep = TRUE; /* set found */

    } else if (e->type == FocusIn) {

        remfocus(root(win), win); /* remove focus from child window if it has it */
        curoff(win); /* remove cursor */
        win->focus = TRUE; /* put focus */
        curon(win); /* replace cursor */
        if (win->childfrm) childfrm_draw(win);
        er->etype = ami_etfocus; /* set focus event */
        *keep = TRUE; /* set found */

    } else if (e->type == EnterNotify) {

        er->etype = ami_ethover; /* set hover event */
        *keep = TRUE; /* set found */

    } else if (e->type == LeaveNotify) {

        er->etype = ami_etnohover; /* set no hover event */
        *keep = TRUE; /* set found */

    } else if (e->type == ClientMessage){

        /* windows manager has message for us */
        if ((Atom)e->xclient.data.l[0] == win->delmsg) {

            /* terminate client window */
            er->etype = ami_etterm;
            fend = TRUE;
            *keep = TRUE;

        }

    } else if (e->type == PropertyNotify)
        /* process windows status messages */
        winstat(win, er, e, keep);

}

/* prepare and process XWindow event */
static void xwinprc(XEvent* e, ami_evtrec* er, int* keep)

{

    winptr     win; /* window record pointer */
    int        ofn; /* output lfn associated with window */
    int        rv;

    if (dmpmsg) {

        /* note we don't print XEvent diagnostics until they get extracted from
           the input queue */
        prtxevt(e);
        fprintf(stderr, "\n");
        fflush(stderr);

    }
    ofn = fndevt(e->xany.window); /* get output window lfn */
    if (ofn >= 0) { /* its one of our windows */

        win = lfn2win(ofn); /* get window for that */
        er->winid = filwin[ofn]; /* get window number */
        xwinevt(win, er, e, keep); /* process XWindow event */

    }

}

/* get and process XWindow event */
static void xwinget(ami_evtrec* er, int* keep)

{

    XEvent e;  /* XWindow event record */
    int    rv;

    do {

        rv = 0;
        /* check input queue has events */
        while (evtque && !*keep) {

            dequexevt(&e); /* remove event from queue */
            xwinprc(&e, er, keep); /* process */

        }
        if (!*keep) {

            XWLOCK();
            rv = XPending(padisplay);
            XWUNLOCK();
            if (rv) {

                XWLOCK();
                XNextEvent(padisplay, &e); /* get next event */
                XWUNLOCK();
                xwinprc(&e, er, keep); /* pass to processing */

            }

        }

    } while (rv && !*keep);

}

static void ievent(FILE* f, ami_evtrec* er)

{

    int        keep;     /* keep event flag */
    int        dfid;     /* XWindow display FID */
    int        rv;       /* return value */
    uint64_t   exp;      /* timer expiration time */
    winptr     win;      /* window record pointer */
    XEvent     e;        /* XWindow event */
    sysevt     sev;      /* system event */
    int        i;

    /* make sure all drawing is complete before we take inputs */
    XWLOCK();
    XFlush(padisplay);
    XWUNLOCK();
    keep = FALSE; /* set do not keep event */
    dfid = ConnectionNumber(padisplay); /* find XWindow display fid */
    do {

        /* check XWindows event queue before we wait on system events */
        xwinget(er, &keep);
        if (!keep) {

            system_event_getsevt(&sev); /* get the next system event */
            /* check display event occurred */
            if (sev.typ == se_inp) {

                if (sev.lse == dspsev) xwinget(er, &keep);
                else if (sidtab[sev.lse-1] && sidtab[sev.lse-1]->joy && joyenb)
                    /* process joystick event */
                    joyevt(er,  &keep, joytab[sidtab[sev.lse-1]->joy-1]);

            } else if (sev.typ == se_tim) {

                if (sidtab[sev.lse-1]->frm) {

                    /* frame event */
                    er->etype = ami_etframe; /* set frame event occurred */
                    /* set window number */
                    er->winid = sidtab[sev.lse-1]->win->wid;
                    keep = TRUE; /* set event found */

                } else {

                    /* timer event */
                    er->etype = ami_ettim; /* set timer type */
                    /* set timer number */
                    er->timnum = sidtab[sev.lse-1]->tim;
                    /* set window number */
                    er->winid = sidtab[sev.lse-1]->win->wid;
                    keep = TRUE; /* set event found */

                }

            }

        }

    } while (!keep); /* until we have a client event */

}

/* external event interface */

void _pa_event_ovr(ami_event_t nfp, ami_event_t* ofp)
    { *ofp = event_vect; event_vect = nfp; }
void ami_event(FILE* f, ami_evtrec* er) { (*event_vect)(f, er); }

static void event_ivf(FILE* f, ami_evtrec* er)

{

    do { /* loop handling via event vectors and queuing */

        /* check input PA queue */
        if (paqevt) dequepaevt(er);
        /* get logical input file number for input, and get the event for that. */
        else ievent(f, er); /* process event */
        if (dmpevt) {

            prtevt(er); /* do diagnostic dump of PA events */
            fprintf(stderr, "\n"); fflush(stderr);

        }
        er->handled = 1; /* set event is handled by default */
        (evtshan)(er); /* call master event handler */
        if (!er->handled && er->etype <= ami_ettabbar) { /* send it to fanout */

            er->handled = 1; /* set event is handled by default */
            (*evthan[er->etype])(er); /* call event handler first */

        }

    } while (er->handled);
    /* event not handled, return it to the caller */

}

/** ****************************************************************************

Send event to window

Send an event to the given window. The event is placed into the queue for the
given window. Note that the input side of the window is found, and the event
spooled for that side. Note that any window number given the event is
overwritten with the proper window id after a copy is made.

The difference between this and inserting to the event chain is that this
routine enters to the top of the chain, and specifies the input side of the
window. Thus it is a more complete send of the event.

*******************************************************************************/

void _pa_sendevent_ovr(ami_sendevent_t nfp, ami_sendevent_t* ofp)
    { *ofp = sendevent_vect; sendevent_vect = nfp; }
void ami_sendevent(FILE* f, ami_evtrec* er) { (*sendevent_vect)(f, er); }

static void sendevent_ivf(FILE* f, ami_evtrec* er)

{

    winptr win;   /* pointer to windows context */
    int fn;       /* logical file number */

    fn = fileno(f); /* find find number */
    if (fn < 0) error(einvfil); /* file invalid */
    if (opnfil[fn]->inl < 0) error(enoinps); /* no input side for window */
    win = lfn2win(fn); /* index window for file */
    isendevent(win, er); /* send it */

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void _pa_eventover_ovr(ami_eventover_t nfp, ami_eventover_t* ofp)
    { *ofp = eventover_vect; eventover_vect = nfp; }
void ami_eventover(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)
    { (*eventover_vect)(e, eh, oeh); }

static void eventover_ivf(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh)

{

    if (e > ami_ettabbar) error(evecaxe); /* cannot vector auxillary event */
    *oeh = evthan[e]; /* save existing event handler */
    evthan[e] = eh; /* place new event handler */

}

/** ****************************************************************************

Override master event handler

Overrides or "hooks" the master event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void _pa_eventsover_ovr(ami_eventsover_t nfp, ami_eventsover_t* ofp)
    { *ofp = eventsover_vect; eventsover_vect = nfp; }
void ami_eventsover(ami_pevthan eh,  ami_pevthan* oeh)
    { (*eventsover_vect)(eh, oeh); }

static void eventsover_ivf(ami_pevthan eh,  ami_pevthan* oeh)

{

    *oeh = evtshan; /* save existing event handler */
    evtshan = eh; /* place new event handler */

}

/** ****************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.

Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

*******************************************************************************/

void _pa_timer_ovr(ami_timer_t nfp, ami_timer_t* ofp)
    { *ofp = timer_vect; timer_vect = nfp; }
void ami_timer(FILE* f, int i, long t, int r) { (*timer_vect)(f, i, t, r); }

static void timer_ivf(FILE* f, /* file to send event to */
                      int   i, /* timer handle */
                      long  t, /* number of tenth-milliseconds to run */
                      int   r) /* timer is to rerun after completion */

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
    sidtab[sid-1]->win = win; /* set window assocated */
    sidtab[sid-1]->tim = i; /* set timer assocated */

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void _pa_killtimer_ovr(ami_killtimer_t nfp, ami_killtimer_t* ofp)
    { *ofp = killtimer_vect; killtimer_vect = nfp; }
void ami_killtimer(FILE* f, int   i ) { (*killtimer_vect)(f, i ); }

static void killtimer_ivf(FILE* f, /* file to kill timer on */
                          int   i  /* handle of timer */
                         )

{

    winptr win; /* windows record pointer */

    if (i < 1 || i > AMI_MAXTIM) error(einvhan); /* invalid timer handle */
    win = txt2win(f); /* get window from file */
    if (!win->timers[i-1]) error(etimacc); /* no such timer */
    system_event_deasetim(win->timers[i-1]); /* deactivate timer */

}

/** ****************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

void _pa_frametimer_ovr(ami_frametimer_t nfp, ami_frametimer_t* ofp)
    { *ofp = frametimer_vect; frametimer_vect = nfp; }
void ami_frametimer(FILE* f, int e) { (*frametimer_vect)(f, e); }

static void frametimer_ivf(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    int    sid; /* system event */

    win = txt2win(f); /* get window from file */
    if (e) { /* set framing timer to run */

        sid = system_event_addsetim(win->frmsev, 166, TRUE);
        win->frmsev = sid;
        /* get system event entry */
        getsee(sid); /* allocate system event entry */
        sidtab[sid-1]->win = win; /* set window assocated */
        sidtab[sid-1]->frm = TRUE; /* set is the framing timer */

    } else {

        system_event_deasetim(win->frmsev);

    }

}

/** ****************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from gralib.
This exists to allow the results of gralib unaware programs to be viewed after
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding gralib unaware programs.

*******************************************************************************/

void _pa_autohold_ovr(ami_autohold_t nfp, ami_autohold_t* ofp)
    { *ofp = autohold_vect; autohold_vect = nfp; }
void ami_autohold(int e) { (*autohold_vect)(e); }

static void autohold_ivf(int e)

{

    fautohold = e; /* set new state of autohold */

}

/** ****************************************************************************

Return number of mice

Returns the number of mice implemented. XWindow supports only one mouse.

*******************************************************************************/

void _pa_mouse_ovr(ami_mouse_t nfp, ami_mouse_t* ofp)
    { *ofp = mouse_vect; mouse_vect = nfp; }
int ami_mouse(FILE* f) { return ((*mouse_vect)(f)); }

static int mouse_ivf(FILE* f)

{

    return 1;

}

/** ****************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version. XWindow supports from 1 to 5 buttons.

*******************************************************************************/

void _pa_mousebutton_ovr(ami_mousebutton_t nfp, ami_mousebutton_t* ofp)
    { *ofp = mousebutton_vect; mousebutton_vect = nfp; }
int ami_mousebutton(FILE* f, int m) { return ((*mousebutton_vect)(f, m)); }

static int mousebutton_ivf(FILE* f, int m)

{

    return 5;

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

void _pa_joystick_ovr(ami_joystick_t nfp, ami_joystick_t* ofp)
    { *ofp = joystick_vect; joystick_vect = nfp; }
int ami_joystick(FILE* f) { return ((*joystick_vect)(f)); }

static int joystick_ivf(FILE* f)

{

    return (numjoy); /* set number of joysticks */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

void _pa_joybutton_ovr(ami_joybutton_t nfp, ami_joybutton_t* ofp)
    { *ofp = joybutton_vect; joybutton_vect = nfp; }
int ami_joybutton(FILE* f, int j) { return ((*joybutton_vect)(f, j)); }

static int joybutton_ivf(FILE* f, int j)

{

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */
    if (!joytab[j-1]) error(esystem); /* should be a table entry */

    return (joytab[j-1]->button); /* return button number */

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

void _pa_joyaxis_ovr(ami_joyaxis_t nfp, ami_joyaxis_t* ofp)
    { *ofp = joyaxis_vect; joyaxis_vect = nfp; }
int ami_joyaxis(FILE* f, int j) { return ((*joyaxis_vect)(f, j)); }

static int joyaxis_ivf(FILE* f, int j)

{

    int ja;

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */
    if (!joytab[j-1]) error(esystem); /* should be a table entry */

    ja = joytab[j-1]->axis; /* get axis number */
    if (ja > 6) ja = 6; /* limit to 6 maximum */

    return (ja); /* set axis number */

}

/** ****************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

void _pa_settabg_ovr(ami_settabg_t nfp, ami_settabg_t* ofp)
    { *ofp = settabg_vect; settabg_vect = nfp; }
void ami_settabg(FILE* f, int t) { (*settabg_vect)(f, t); }

static void settabg_ivf(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, t); /* translate to graphical call */

}

/** ****************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void _pa_settab_ovr(ami_settab_t nfp, ami_settab_t* ofp)
    { *ofp = settab_vect; settab_vect = nfp; }
void ami_settab(FILE* f, int t) { (*settab_vect)(f, t); }

static void settab_ivf(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, (t-1)*win->charspace+1); /* translate to graphical call */

}

/** ****************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

void _pa_restabg_ovr(ami_restabg_t nfp, ami_restabg_t* ofp)
    { *ofp = restabg_vect; restabg_vect = nfp; }
void ami_restabg(FILE* f, int t) { (*restabg_vect)(f, t); }

static void restabg_ivf(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, t); /* translate to graphical call */

}

/** ****************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void _pa_restab_ovr(ami_restab_t nfp, ami_restab_t* ofp)
    { *ofp = restab_vect; restab_vect = nfp; }
void ami_restab(FILE* f, int t) { (*restab_vect)(f, t); }

static void restab_ivf(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, (t-1)*win->charspace+1); /* translate to graphical call */

}

/** ****************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

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

/** ****************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

void _pa_funkey_ovr(ami_funkey_t nfp, ami_funkey_t* ofp)
    { *ofp = funkey_vect; funkey_vect = nfp; }
int ami_funkey(FILE* f) { return ((*funkey_vect)(f)); }

static int funkey_ivf(FILE* f)

{

    return (12); /* number of function keys */

}

/** ****************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void _pa_title_ovr(ami_title_t nfp, ami_title_t* ofp)
    { *ofp = title_vect; title_vect = nfp; }
void ami_title(FILE* f, char* ts) { (*title_vect)(f, ts); }

static void title_ivf(FILE* f, char* ts)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->childfrm) {

        /* Ami-drawn child frame: store title and repaint frame */
        if (win->wintitle) free(win->wintitle);
        win->wintitle = strdup(ts);
        childfrm_draw(win);

    } else {

        /* top-level: set WM title */
        XWLOCK();
        XStoreName(padisplay, win->xmwhan, ts);
        XSetIconName(padisplay, win->xwhan, ts);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Allocate anonymous window id

Allocates and returns an "anonymous" window id. The window id numbers are assigned
by the client program. However, there a an alternative set of ids that are
allocated as needed. Graphics keeps track of which anonymous ids have been
allocated and which have been freed.

The implementation here is to assign anonymous window ids negative numbers,
starting with -1 and proceeding downwards. 0 is never assigned. The use of
negative ids insure that the normal window ids will never overlap any anonyous
window ids.

Note that the wid entry will actually be opened by openwin(), and will be closed
by closewin(), so there is no need to deallocate this wid. Once an anonymous id
is allocated, it is reserved until it is used and removed by killwidget().

*******************************************************************************/

void _pa_getwinid_ovr(ami_getwinid_t nfp, ami_getwinid_t* ofp)
    { *ofp = getwinid_vect; getwinid_vect = nfp; }
int ami_getwinid(void) { return ((*getwinid_vect)()); }

static int getwinid_ivf(void)

{

    int wid; /* window id */

    wid = -1; /* start at -1 */
    /* find any open entry */
    while (wid > -MAXFIL && xltwin[wid+MAXFIL] >= 0) wid--;
    if (wid == -MAXFIL) error(enowid); /* ran out of buried wids */

    return (wid); /* return the wid */

}

/** ****************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.

The window id can be from 1 to MAXFIL, but 1 is reserved for the main I/O
window.

*******************************************************************************/

/* check file is already in use */
static int fndfil(FILE* fp)

{

    int fi; /* file index */
    int ff; /* found file */

    ff = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++)
        if (opnfil[fi] && opnfil[fi]->sfp == fp) ff = fi; /* set found */

    return (ff);

}

static void iopenwin(FILE** infile, FILE** outfile, FILE* parent, int wid,
                     int subclient)

{

    int ifn, ofn, pfn; /* file logical handles */

    /* check valid window handle */
    if (!wid || wid < -MAXFIL || wid > MAXFIL) error(einvwin);
    /* check if the window id is already in use */
    if (xltwin[wid+MAXFIL] >= 0) error(ewinuse); /* error */
    if (parent) {

        txt2win(parent); /* validate parent is a window file */
        pfn = txt2lfn(parent); /* get logical parent */

    } else pfn = -1; /* set no parent */
    ifn = fndfil(*infile); /* find previous open input side */
    if (ifn < 0) { /* no other input file, open new */

        /* open input file */
        *infile = fopen("/dev/null", "r"); /* open null as read only */
        if (!*infile) error(enoopn); /* can't open */
        setvbuf(*infile, NULL, _IONBF, 0); /* turn off buffering */
        ifn = fileno(*infile); /* get logical file no */

    }
    /* open output file */
    *outfile = fopen("/dev/null", "w");
    ofn = fileno(*outfile); /* get logical file no. */
    if (ofn == -1) error(esystem);
    if (!*outfile) error(enoopn); /* can't open */
    setvbuf(*outfile, NULL, _IONBF, 0); /* turn off buffering */

    /* check either input is unused, or is already an input side of a window.
       A filrec for a closed window remains in opnfil[] with inw=FALSE and
       win=NULL; we must treat that "cleared" state as unused, same as NULL. */
    if (opnfil[ifn] && (opnfil[ifn]->inw || opnfil[ifn]->win))
        if (!opnfil[ifn]->inw || opnfil[ifn]->win) error(einmode); /* wrong mode */
    /* check output file is in use for input or output from window */
    if (opnfil[ofn]) /* entry exists */
        if (opnfil[ofn]->inw || opnfil[ofn]->win)
            error(efinuse); /* file in use */
    /* establish all logical files and links, translation tables, and open window */
    openio(*infile, *outfile, ifn, ofn, pfn, wid, subclient);

}

void _pa_openwin_ovr(ami_openwin_t nfp, ami_openwin_t* ofp)
    { *ofp = openwin_vect; openwin_vect = nfp; }
void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)
    { (*openwin_vect)(infile, outfile, parent, wid); }

static void openwin_ivf(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

    /* open as child of client window */
    iopenwin(infile, outfile, parent, wid, TRUE);

}

/** ****************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

*******************************************************************************/

void _pa_sizbufg_ovr(ami_sizbufg_t nfp, ami_sizbufg_t* ofp)
    { *ofp = sizbufg_vect; sizbufg_vect = nfp; }
void ami_sizbufg(FILE* f, int x, int y) { (*sizbufg_vect)(f, x, y); }

static void sizbufg_ivf(FILE* f, int x, int y)

{

    int            si;     /* index for current display screen */
    winptr         win;    /* pointer to windows context */
    Pixmap         oldbuf; /* saved old buffer pixmap */
    int            oldw, oldh; /* old buffer dimensions */
    int            copyw, copyh; /* intersection to copy */
    scnptr         sc;     /* screen pointer */
    ami_evtrec     er;     /* event record for redraws */
    int            depth;

    if (x < 1 || y < 1)  error(einvsiz); /* invalid buffer size */
    win = txt2win(f); /* get window context */
    if (!win->bufmod) error(ebufoff); /* error */

    /* save old buffer info from the current display screen */
    sc = win->screens[win->curdsp-1];
    oldbuf = sc->xbuf;
    oldw = sc->maxxg;
    oldh = sc->maxyg;

    /* set new buffer size */
    win->gmaxx = x/win->charspace; /* find character size x */
    win->gmaxy = y/win->linespace; /* find character size y */
    win->gmaxxg = x; /* set size in pixels x */
    win->gmaxyg = y; /* set size in pixels y */

    /* tear out all screen buffers except we detach the current display
       buffer's pixmap first (we saved it above) */
    sc->xbuf = 0; /* detach so disscn doesn't free it */
    for (si = 0; si < MAXCON; si++) {

        if (win->screens[si]) {

            disscn(win, win->screens[si]);
            ifree(win->screens[si]); /* free screen data */
            win->screens[si] = NULL; /* clear screen data */

        }

    }

    /* allocate new screen buffer at the new size */
    win->screens[win->curdsp-1] = imalloc(sizeof(scncon));
    iniscn(win, win->screens[win->curdsp-1]); /* creates new pixmap + clears it */
    if (win->curdsp != win->curupd) { /* also create the update buffer */

        win->screens[win->curupd-1] = imalloc(sizeof(scncon));
        iniscn(win, win->screens[win->curupd-1]);

    }

    /* copy the intersection of old and new buffers */
    sc = win->screens[win->curdsp-1]; /* re-index (iniscn created new sc) */
    copyw = oldw < x ? oldw : x;
    copyh = oldh < y ? oldh : y;
    if (copyw > 0 && copyh > 0) {

        XWLOCK();
        XCopyArea(padisplay, oldbuf, sc->xbuf, sc->xcxt,
                  0, 0, copyw, copyh, 0, 0);
        XWUNLOCK();

    }

    /* release the old buffer pixmap */
    XWLOCK();
    XFreePixmap(padisplay, oldbuf);
    XWUNLOCK();

    /* restore buffer to screen (handles margins if buffer < window) */
    restore(win);

    /* generate redraw events for newly exposed areas of the buffer
       (areas in the new buffer not covered by the old buffer) */
    if (x > oldw) {

        /* right strip: from old width to new width, full new height */
        er.etype = ami_etredraw;
        er.rsx = oldw + 1;
        er.rsy = 1;
        er.rex = x;
        er.rey = y;
        isendevent(win, &er);

    }
    if (y > oldh) {

        /* bottom strip: from old height to new height, old width only
           (right strip already covered above) */
        er.etype = ami_etredraw;
        er.rsx = 1;
        er.rsy = oldh + 1;
        er.rex = oldw < x ? oldw : x;
        er.rey = y;
        isendevent(win, &er);

    }

}

/** ****************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void _pa_sizbuf_ovr(ami_sizbuf_t nfp, ami_sizbuf_t* ofp)
    { *ofp = sizbuf_vect; sizbuf_vect = nfp; }
void ami_sizbuf(FILE* f, int x, int y) { (*sizbuf_vect)(f, x, y); }

static void sizbuf_ivf(FILE* f, int x, int y)

{

    winptr win; /* pointer to windows context */

    win = txt2win(f); /* get window context */
    /* just translate from characters to pixels and do the resize in pixels. */
    ami_sizbufg(f, x*win->charspace, y*win->linespace);

}

/** ****************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

void _pa_buffer_ovr(ami_buffer_t nfp, ami_buffer_t* ofp)
    { *ofp = buffer_vect; buffer_vect = nfp; }
void ami_buffer(FILE* f, int e) { (*buffer_vect)(f, e); }

static void buffer_ivf(FILE* f, int e)

{

    winptr            win; /* pointer to windows context */
    XWindowChanges    xwc; /* XWindow values */
    XWindowAttributes xwa; /* XWindow attributes */
    XEvent            xe;  /* XWindow event */
    int               si;  /* index for screens */

    win = txt2win(f); /* get window context */
    if (e) { /* perform buffer on actions */

        win->bufmod = TRUE; /* turn buffer mode on */
        /* restore last buffer size */
        win->gmaxxg = win->bufxg; /* pixel size */
        win->gmaxyg = win->bufyg;
        win->gmaxx = win->bufx; /* character size */
        win->gmaxy = win->bufy;
        win->screens[win->curdsp-1]->maxxg = win->gmaxxg; /* pixel size */
        win->screens[win->curdsp-1]->maxyg = win->gmaxyg;
        win->screens[win->curdsp-1]->maxx = win->gmaxx; /* character size */
        win->screens[win->curdsp-1]->maxy = win->gmaxy;
        xwc.width = win->gmaxxg; /* set XWindow width and height */
        xwc.height = win->gmaxyg;
        XWLOCK();
        XConfigureWindow(padisplay, win->xmwhan, CWWidth|CWHeight, &xwc);
        XWUNLOCK();
        /* change saved size to match */
        win->xmwr.w = xwc.width;
        win->xmwr.h = xwc.height;
#ifdef WAITWMR
        /* wait for the configure response with correct sizes */
        do { peekxevt(&xe); /* peek next event */
        } while (xe.type != ConfigureNotify || xe.xconfigure.width != xwc.width ||
                 xe.xconfigure.height != xwc.height || xe.xany.window != win->xmwhan);
#endif
        restore(win); /* restore buffer to screen */

    } else if (win->bufmod) { /* perform buffer off actions */

        /* The screen buffer contains a lot of drawing information, so we have
           to keep one of them. We keep the current display, and force the
           update to point to it as well. This single buffer  serves as
           a "template" for the real pixels on screen. */
        win->bufmod = FALSE; /* turn buffer mode off */
        /* dispose of screen data structures */
        for (si = 0; si < MAXCON; si++) if (si != win->curdsp-1)
            if (win->screens[si]) {

            disscn(win, win->screens[si]); /* free buffer data */
            ifree(win->screens[si]); /* free screen data */
            win->screens[si] = NULL; /* clear screen data */

        }
        win->curupd = win->curdsp; /* unify the screens */
        /* get actual size of onscreen window, and set that as client space */
        XWLOCK();
        XGetWindowAttributes(padisplay, win->xwhan, &xwa);
        win->gmaxxg = xwa.width; /* return size */
        win->gmaxyg = xwa.height;
        win->gmaxx = win->gmaxxg/win->charspace; /* find character size x */
        win->gmaxy = win->gmaxyg/win->linespace; /* find character size y */
        /* tell the window to resize */
        xe.type = ConfigureNotify;
        xe.xany.serial = 0;
        xe.xconfigure.width = win->gmaxxg;
        xe.xconfigure.height = win->gmaxyg;
        xe.xconfigure.window = win->xwhan;
        XSendEvent(padisplay, win->xwhan, FALSE, 0, &xe);
        /* tell the window to repaint */
        xe.type = Expose;
        xe.xany.serial = 0;
        xe.xexpose.x = 0;
        xe.xexpose.y = 0;
        xe.xexpose.width = win->gmaxxg;
        xe.xexpose.height = win->gmaxyg;
        xe.xexpose.window = win->xwhan;
        /* I don't know why, but the XSendEvent() call does nothing here */
        enquexevt(&xe);
        //XSendEvent(padisplay, win->xmwhan, FALSE, 0, &xe);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/

/* insert at list end */
static void insend(metptr* root, metptr mp, metptr* lp)

{

    mp->next = NULL; /* clear next */
    *lp = NULL; /* clear last */
    if (*root) {

        /* find last entry */
        *lp = *root;
        while ((*lp)->next) *lp = (*lp)->next;
        (*lp)->next = mp; /* set new last */

    } else *root = mp; /* insert only entry */

}

/* create menu tracking entry */
static void mettrk(
    /** window file for menu */          FILE*      f,
    /** window structure */              winptr     win,
    /** root of menu list */             metptr*    root,
    /** client side menu tree pointer */ ami_menuptr m,
    /** new entry created */             metptr*    nm)

{

    metptr mp; /* menu tracking entry pointer */
    metptr lp; /* last entry */

    mp = getmet(); /* get a new tracking entry */
    insend(root, mp, &lp); /* insert to end */
    mp->branch = NULL; /* clear branch */
    mp->frame = NULL; /* clear frame */
    mp->head = win->metlst; /* point back to root (used for floating menus) */
    mp->menubar = FALSE; /* set not menu bar */
    mp->frm = FALSE; /* set not frame */
    mp->onoff = FALSE; /* set no on/off highlighter */
    if (m) mp->onoff = m->onoff; /* place on/off highlighter */
    mp->select = FALSE; /* place status of select (off) */
    mp->id = 0; /* set invalid id */
    if (m) mp->id = m->id; /* place id */
    mp->oneof = NULL; /* set no "one of" */
    mp->chnhd = mp; /* point "one of" head to self */
    mp->ena = TRUE; /* set enabled */
    mp->bar = FALSE; /* set no bar under */
    if (m) mp->bar = m->bar; /* place bar state */
    /* set up the button properties */
    mp->pressed = FALSE; /* not pressed */
    mp->wf = NULL; /* set no window file attached */
    mp->title = NULL; /* set no title */
    if (m) mp->title = str(m->face); /* copy face string */
    mp->evtfil = f; /* set menu event post file */
    mp->prime = FALSE; /* set not prime menu */
    /* We are walking backwards in the list, and we need the next list entry
      to know the "one of" chain. So we tie the entry to itself as a flag
      that it chains to the next entry. That chain will get fixed on the
      next entry. */
    if (m) if (m->oneof) mp->oneof = mp;
    /* now tie the last entry to this if indicated */
    if (lp) /* there is a next entry */
        if (lp->oneof == lp) { /* it indexes itself */

        lp->oneof = mp; /* link last to this entry */
        mp->chnhd = lp->chnhd; /* copy the last entry head to this */

    }
    *nm = mp; /* pass back created entry */

}

/* create menu list */
static void createmenu(FILE* f, winptr win, metptr* root, ami_menuptr m)

{

    metptr mp, mp2;  /* pointer to menu tracking entry */

    while (m) { /* add menu item */

        if (m->branch) { /* handle submenu */

            mettrk(f, win, root, m, &mp); /* enter that into tracking */
            createmenu(f, win, &mp->branch, m->branch); /* create submenu */
            mettrk(f, win, &mp->frame, NULL, &mp2); /* create frame for submenu */
            mp2->frm = TRUE; /* set is frame */

        } else { /* handle terminal menu */

            mettrk(f, win, root, m, &mp); /* enter that into tracking */

        }
        m = m->next; /* next menu entry */

    }

}

/* close and free menu list */
static void menu_close(metptr mp)

{

    metptr np; /* next pointer */

    while (mp) {

        menu_close(mp->branch); /* clear submenus */
        if (mp->wf) fclose(mp->wf); /* close window file if open */
        np = mp->next; /* get next entry before dispose */
        putmet(mp); /* dispose entry */
        mp = np; /* set next */

    }

}

/* configure window for menu on */
static void menu_resize(FILE* f, winptr win, int menuon)

{

    XEvent e;   /* XWindow event */
    int wx, wy; /* window sizes */
    int yes;    /* y extra size */

    yes = 0; /* set no menu extra size */
    if (menuon) yes = win->menuspcy; /* set menu extra y size */
    /* resize the master window and activate the menu bar */
    ami_winclientg(f, win->gmaxxg, win->gmaxyg+yes, &wx, &wy,
                  BIT(ami_wmframe)*win->frame|BIT(ami_wmsize)*win->size|
                  BIT(ami_wmsysbar)*win->sysbar);
    ami_setsizg(f, wx, wy);
    /* move subclient window down past menu bar */
    XWLOCK();
    XMoveWindow(padisplay, win->xwhan, 0, yes);
    XWUNLOCK();

#ifdef WAITWMR
    /* wait for the configure response */
    do { peekxevt(&e); /* peek next event */
    } while (e.type != ConfigureNotify || e.xconfigure.x != 0 ||
             e.xconfigure.y != yes ||
             e.xany.window != win->xwhan);
#endif
    restore(win);

}

void _pa_menu_ovr(ami_menu_t nfp, ami_menu_t* ofp)
    { *ofp = menu_vect; menu_vect = nfp; }
void ami_menu(FILE* f, ami_menuptr m) { (*menu_vect)(f, m); }

static void menu_ivf(FILE* f, ami_menuptr m)

{

    winptr win;  /* pointer to windows context */
    metptr mp;   /* pointer to menu tracking entry */
    XEvent e;    /* XWindow event */
    int wx, wy;  /* window sizes */
    int menuact; /* is a menu active */

    win = txt2win(f); /* get window context */
    menuact = FALSE; /* set no menu active */
    if (win->metlst) { /* destroy previous menu */

        menuact = TRUE; /* set menu active */
        /* dispose of menu tracking entries */
        menu_close(win->menu); /* release menu bar */
        win->menu = NULL; /* clear */
        menu_close(win->metlst); /* close menu list */
        win->metlst = NULL; /* clear */
        /* if there is no new menu, restore window without menu bar */
        if (!m) menu_resize(f, win, FALSE);

    }
    if (m) { /* there is a new menu to activate */

        /* no previous menu active, resize the master window and activate the
           menu bar */
        if (!menuact) menu_resize(f, win, TRUE);
        /* get an entry for the menu bar */
        win->menu = getmet();
        win->menu->next = NULL; /* clear next */
        win->menu->branch = NULL; /* branch */
        win->menu->menubar = TRUE; /* flag is menu bar */
        win->menu->frm = FALSE; /* not frame */
        win->menu->bar = FALSE; /* no bar (underline) */
        win->menu->prime = FALSE; /* not prime */
        win->menu->title = NULL; /* set no face string */
        win->menu->pressed = FALSE; /* set not pressed */
        /* make internal copy of menu */
        createmenu(f, win, &win->metlst, m);
        /* activate top level menu */
        actmenu(f);

    }

}

/** ****************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

void _pa_menuena_ovr(ami_menuena_t nfp, ami_menuena_t* ofp)
    { *ofp = menuena_vect; menuena_vect = nfp; }
void ami_menuena(FILE* f, int id, int onoff) { (*menuena_vect)(f, id, onoff); }

static void menuena_ivf(FILE* f, int id, int onoff)

{

    winptr win; /* pointer to windows context */
    XEvent xe;  /* XWindow event */
    metptr mp;  /* menu entry pointer */

    win = txt2win(f); /* get window context */
    mp = fndmenu(win, id); /* find the menu entry */
    mp->ena = !!onoff; /* set state of enable */
    /* tell the window to repaint */
    xe.type = Expose;
    xe.xany.serial = 0;
    xe.xexpose.x = 0;
    xe.xexpose.y = 0;
    xe.xexpose.width = win->gmaxxg;
    xe.xexpose.height = win->gmaxyg;
    xe.xexpose.window = win->xwhan;
    XWLOCK();
    XSendEvent(padisplay, win->xwhan, FALSE, 0, &xe);
    XWUNLOCK();

}

/** ****************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

/* repaint menu button */
static void menu_repaint(metptr mp)

{

    winptr win; /* pointer to windows context */
    XEvent xe;  /* XWindow event */

    if (mp->wf) { /* check menu file is active */

        win = txt2win(mp->wf); /* get window context */
        /* tell the window to repaint */
        xe.type = Expose;
        xe.xany.serial = 0;
        xe.xexpose.x = 0;
        xe.xexpose.y = 0;
        xe.xexpose.width = win->gmaxxg;
        xe.xexpose.height = win->gmaxyg;
        xe.xexpose.window = win->xwhan;
        XWLOCK();
        XSendEvent(padisplay, win->xwhan, FALSE, 0, &xe);
        XWUNLOCK();

    }

}

/* clear "one of" select list */
static void clrlst(metptr mp)

{

    do { /* items in list */

        mp->select = FALSE; /* remove select */
        menu_repaint(mp); /* display that */
        mp = mp->oneof; /* link next */

    } while (mp); /* no more */

}

void _pa_menusel_ovr(ami_menusel_t nfp, ami_menusel_t* ofp)
    { *ofp = menusel_vect; menusel_vect = nfp; }
void ami_menusel(FILE* f, int id, int select) { (*menusel_vect)(f, id, select); }

static void menusel_ivf(FILE* f, int id, int select)

{

    winptr win; /* pointer to windows context */
    metptr mp;  /* menu entry pointer */

    win = txt2win(f); /* get window context */
    mp = fndmenu(win, id); /* find the menu entry */
    if (mp->chnhd->oneof != mp->chnhd) /* is a "one of" chain */
        clrlst(mp->chnhd); /* clear "one of" group */
    mp->select = !!select; /* set state of select */
    menu_repaint(mp); /* tell the window to repaint */

}

/** ****************************************************************************

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu, then the program selections placed in the menu.

*******************************************************************************/

/* get menu entry */
static void getmenu(ami_menuptr* m, int id, char* face)

{

    *m = imalloc(sizeof(ami_menurec));
    (*m)->next   = NULL; /* no next */
    (*m)->branch = NULL; /* no branch */
    (*m)->onoff  = FALSE; /* not an on/off value */
    (*m)->oneof  = FALSE; /* not a "one of" value */
    (*m)->bar    = FALSE; /* no bar under */
    (*m)->id     = id;    /* no id */
    (*m)->face = str(face); /* place face string */

}

/* add standard list item */
static void additem(ami_stdmenusel sms, int i, ami_menuptr* m, ami_menuptr* l,
                    char* s, int b)

{

    if (BIT(i) & sms) { /* this item is active */

        getmenu(m, i, s); /* get menu item */
        appendmenu(l, *m); /* add to list */
        (*m)->bar = b; /* set bar status */

    }

}

void _pa_stdmenu_ovr(ami_stdmenu_t nfp, ami_stdmenu_t* ofp)
    { *ofp = stdmenu_vect; stdmenu_vect = nfp; }
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
    { (*stdmenu_vect)(sms, sm, pm); }

static void stdmenu_ivf(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)

{

    ami_menuptr m, hm; /* pointer for menu entries */

    *sm = NULL; /* clear menu */

    /* check and perform "file" menu */

    if (sms & (BIT(AMI_SMNEW) | BIT(AMI_SMOPEN) | BIT(AMI_SMCLOSE) |
               BIT(AMI_SMSAVE) | BIT(AMI_SMSAVEAS) | BIT(AMI_SMPAGESET) |
               BIT(AMI_SMPRINT) | BIT(AMI_SMEXIT))) { /* file menu */

        getmenu(&hm, 0, "File"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, AMI_SMNEW, &m, &hm->branch, "New", FALSE);
        additem(sms, AMI_SMOPEN, &m, &hm->branch, "Open", FALSE);
        additem(sms, AMI_SMCLOSE, &m, &hm->branch, "Close", FALSE);
        additem(sms, AMI_SMSAVE, &m, &hm->branch, "Save", FALSE);
        additem(sms, AMI_SMSAVEAS, &m, &hm->branch, "Save As", TRUE);
        additem(sms, AMI_SMPAGESET, &m, &hm->branch, "Page Setup", FALSE);
        additem(sms, AMI_SMPRINT, &m, &hm->branch, "Print", TRUE);
        additem(sms, AMI_SMEXIT, &m, &hm->branch, "Exit", FALSE);

   }

   /* check and perform "edit" menu */

   if (sms&(BIT(AMI_SMUNDO) | BIT(AMI_SMCUT) | BIT(AMI_SMPASTE) |
            BIT(AMI_SMDELETE) | BIT(AMI_SMFIND) | BIT(AMI_SMFINDNEXT) |
            BIT(AMI_SMREPLACE) | BIT(AMI_SMGOTO) | BIT(AMI_SMSELECTALL))) {

        /* file menu */
        getmenu(&hm, 0, "Edit"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, AMI_SMUNDO, &m, &hm->branch, "Undo", TRUE);
        additem(sms, AMI_SMCUT, &m, &hm->branch, "Cut", FALSE);
        additem(sms, AMI_SMPASTE, &m, &hm->branch, "Paste", FALSE);
        additem(sms, AMI_SMDELETE, &m, &hm->branch, "Delete", TRUE);
        additem(sms, AMI_SMFIND, &m, &hm->branch, "Find", FALSE);
        additem(sms, AMI_SMFINDNEXT, &m, &hm->branch, "Find Next", FALSE);
        additem(sms, AMI_SMREPLACE, &m, &hm->branch, "Replace", FALSE);
        additem(sms, AMI_SMGOTO, &m, &hm->branch, "Goto", TRUE);
        additem(sms, AMI_SMSELECTALL, &m, &hm->branch, "Select All", FALSE);

   }

   /* insert custom menu */

   while (pm) { /* insert entries */

        m = pm; /* index top button */
        pm = pm->next; /* next button */
        appendmenu(sm, m);

   }

   /* check and perform "window" menu */

   if (sms & (BIT(AMI_SMNEWWINDOW) | BIT(AMI_SMTILEHORIZ) | BIT(AMI_SMTILEVERT) |
              BIT(AMI_SMCASCADE) | BIT(AMI_SMCLOSEALL)))  { /* file menu */

        getmenu(&hm, 0, "Window"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, AMI_SMNEWWINDOW, &m, &hm->branch, "New Window", TRUE);
        additem(sms, AMI_SMTILEHORIZ, &m, &hm->branch, "Tile Horizontally", FALSE);
        additem(sms, AMI_SMTILEVERT, &m, &hm->branch, "Tile Vertically", FALSE);
        additem(sms, AMI_SMCASCADE, &m, &hm->branch, "Cascade", TRUE);
        additem(sms, AMI_SMCLOSEALL, &m, &hm->branch, "Close All", FALSE);

   }

   /* check and perform "help" menu */

   if (sms & (BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT))) { /* file menu */

        getmenu(&hm, 0, "Help"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, AMI_SMHELPTOPIC, &m, &hm->branch, "Help Topics", TRUE);
        additem(sms, AMI_SMABOUT, &m, &hm->branch, "About", FALSE);

    }

}

/** ****************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

void _pa_front_ovr(ami_front_t nfp, ami_front_t* ofp)
    { *ofp = front_vect; front_vect = nfp; }
void ami_front(FILE* f) { (*front_vect)(f); }

static void front_ivf(FILE* f)

{

    winptr win; /* pointer to windows context */

    win = txt2win(f); /* get window context */
    XWLOCK();
    XRaiseWindow(padisplay, win->xmwhan);
    XWUNLOCK();

}

/** ****************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

void _pa_back_ovr(ami_back_t nfp, ami_back_t* ofp)
    { *ofp = back_vect; back_vect = nfp; }
void ami_back(FILE* f) { (*back_vect)(f); }

static void back_ivf(FILE* f)

{

    winptr win; /* pointer to windows context */

    win = txt2win(f); /* get window context */
    XWLOCK();
    XLowerWindow(padisplay, win->xmwhan);
    XWUNLOCK();

}

/** ****************************************************************************

Get window size graphical

Gets the onscreen window size.

*******************************************************************************/

void _pa_getsizg_ovr(ami_getsizg_t nfp, ami_getsizg_t* ofp)
    { *ofp = getsizg_vect; getsizg_vect = nfp; }
void ami_getsizg(FILE* f, int* x, int* y) { (*getsizg_vect)(f, x, y); }

static void getsizg_ivf(FILE* f, int* x, int* y)

{

    XWindowAttributes xwa; /* XWindow attributes */
    winptr            win; /* pointer to windows context */
    Window            cw, pw, rw;
    Window*           cwl;
    unsigned          ncw;

    win = txt2win(f); /* get window context */
#ifdef WAITWMR
    /* if wait for window manager is set, ask the WM what the size is */
    XWLOCK();
    /* find parent */
    XQueryTree(padisplay, win->xwhan, &rw, &pw, &cwl, &ncw);
    /* get parent parameters */
    XGetWindowAttributes(padisplay, pw, &xwa);
    XWUNLOCK();
    *x = xwa.width+win->pfw;
    *y = xwa.height+win->pfh;
#else
    /* if wait for window manager is not set, just use stored */
    *x = win->xmwr.w+win->pfw;
    *y = win->xmwr.h+win->pfh;
#endif

}

/** ****************************************************************************

Get window size character

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void _pa_getsiz_ovr(ami_getsiz_t nfp, ami_getsiz_t* ofp)
    { *ofp = getsiz_vect; getsiz_vect = nfp; }
void ami_getsiz(FILE* f, int* x, int* y) { (*getsiz_vect)(f, x, y); }

static void getsiz_ivf(FILE* f, int* x, int* y)

{

    winptr win; /* pointer to windows context */
    winptr par; /* pointer to parent windows context */
    int    gx, gy;

    win = txt2win(f); /* get window context */
    ami_getsizg(f, &gx, &gy); /* get graphics size */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *x = gx/par->charspace;
        *y = gy/par->linespace;

    } else {

        /* find character based sizes */
        *x = gx/stdchrx;
        *y = gy/stdchry;

    }

}

/** ****************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

Note: XWindow, or at least the window manager, apparently accepts a width of 1
and height of 1 as the minimum size window.

*******************************************************************************/

void _pa_setsizg_ovr(ami_setsizg_t nfp, ami_setsizg_t* ofp)
    { *ofp = setsizg_vect; setsizg_vect = nfp; }
void ami_setsizg(FILE* f, int x, int y) { (*setsizg_vect)(f, x, y); }

static void setsizg_ivf(FILE* f, int x, int y)

{

    winptr win; /* pointer to windows context */
    XWindowChanges xwc; /* XWindow values */
    XEvent e; /* Xwindow event */

    win = txt2win(f); /* get window context */
    /* if child, apply parent's viewport scale to the requested size */
    if (win->parwin) { x = L2PW(win->parwin, x); y = L2PH(win->parwin, y); }
    /* change to client terms with zero clip */
    xwc.width = x-win->pfw; if (xwc.width < 1) xwc.width = 1;
    xwc.height = y-win->pfh; if (xwc.height < 1) xwc.height = 1;
    /* Check repeated sizing. This prevents hangups due to the window manager
       ignoring such sets. */
    if (xwc.width != win->xmwr.w || xwc.height != win->xmwr.h) {

        /* reconfigure window */
        XWLOCK();
        if (win->childfrm) {

            /* For child-framed windows: x/y are total master dimensions.
               Master = x,y. Subclient = (x-pfw, y-pfh) at offset (cwox,cwoy). */
            XResizeWindow(padisplay, win->xmwhan, x, y);
            XMoveResizeWindow(padisplay, win->xwhan,
                              win->cwox, win->cwoy,
                              xwc.width, xwc.height);
            win->xmwr.w = x;
            win->xmwr.h = y;
            win->xwr.x = win->cwox;
            win->xwr.y = win->cwoy;
            win->xwr.w = xwc.width;
            win->xwr.h = xwc.height;

        } else {

            XConfigureWindow(padisplay, win->xmwhan, CWWidth|CWHeight, &xwc);
            /* set new size */
            win->xmwr.w = xwc.width;
            win->xmwr.h = xwc.height;

        }
        XWUNLOCK();

        if (win->childfrm) childfrm_draw(win);

#ifdef WAITWMR
        /* wait for the configure response with correct sizes (top-level only;
           child-framed windows resize synchronously above and don't get
           ConfigureNotify from a window manager) */
        if (!win->childfrm) {
            do { peekxevt(&e); /* peek next event */
            } while (e.type != ConfigureNotify || e.xconfigure.width != xwc.width ||
                     e.xconfigure.height != xwc.height || e.xany.window != win->xmwhan);
        }
#endif

        /* because this event may not reach ami_event() for some time, we have to
           set the dimensions now */
        if (!win->bufmod) {

            /* reset tracking sizes */
            win->gmaxxg = e.xconfigure.width; /* graphics x */
            win->gmaxyg = e.xconfigure.height; /* graphics y */
            /* find character size x */
            win->gmaxx = win->gmaxxg/win->charspace;
            /* find character size y */
            win->gmaxy = win->gmaxyg/win->linespace;

        }

    }

}

/** ****************************************************************************

Set window size character

Sets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void _pa_setsiz_ovr(ami_setsiz_t nfp, ami_setsiz_t* ofp)
    { *ofp = setsiz_vect; setsiz_vect = nfp; }
void ami_setsiz(FILE* f, int x, int y) { (*setsiz_vect)(f, x, y); }

static void setsiz_ivf(FILE* f, int x, int y)

{


    winptr win, par; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        x = x*par->charspace;
        y = y*par->linespace;

    } else {

        /* find character based sizes */
        x = x*stdchrx;
        y = y*stdchry;

    }
    ami_setsizg(f, x, y); /* execute */

}

/** ****************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

*******************************************************************************/

void _pa_setposg_ovr(ami_setposg_t nfp, ami_setposg_t* ofp)
    { *ofp = setposg_vect; setposg_vect = nfp; }
void ami_setposg(FILE* f, int x, int y) { (*setposg_vect)(f, x, y); }

static void setposg_ivf(FILE* f, int x, int y)

{

    winptr win;         /* pointer to windows context */
    XWindowChanges xwc; /* XWindow values */
    XEvent         e;   /* XWindow event */

    win = txt2win(f); /* get window context */

    /* don't repeat positions, it will cause a no-op in windows manager */
    if (x-1 != win->xmwr.x || y-1 != win->xmwr.y) {

        /* reconfigure window; if child, apply parent's viewport scale */
        XWLOCK();
        if (win->parwin)
            XMoveWindow(padisplay, win->xmwhan,
                        L2PX(win->parwin, x-1), L2PY(win->parwin, y-1));
        else
            XMoveWindow(padisplay, win->xmwhan, x-1, y-1);
        XWUNLOCK();

#ifdef WAITWMR
        /* wait for the configure response */
        do { peekxevt(&e); /* peek next event */
        } while (e.type != ConfigureNotify || e.xany.window != win->xmwhan);
#endif

        /* set origin for next time */
        win->xmwr.x = x-1;
        win->xmwr.y = y-1;

    }

}

/** ****************************************************************************

Set window position character

Sets the onscreen window position, in character terms. If the window has a
parent, the demensions are converted to the current character size there.
Otherwise, pixel based demensions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void _pa_setpos_ovr(ami_setpos_t nfp, ami_setpos_t* ofp)
    { *ofp = setpos_vect; setpos_vect = nfp; }
void ami_setpos(FILE* f, int x, int y) { (*setpos_vect)(f, x, y); }

static void setpos_ivf(FILE* f, int x, int y)

{

    winptr win, par; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based position */
        x = (x-1)*par->charspace+1;
        y = (y-1)*par->linespace+1;

    } else {

        /* find character based position */
        x = (x-1)*stdchrx+1;
        y = (y-1)*stdchry+1;

    }
    ami_setposg(f, x, y); /* execute */

}

/** ****************************************************************************

Get screen size graphical

Gets the total screen size.

*******************************************************************************/

void _pa_scnsizg_ovr(ami_scnsizg_t nfp, ami_scnsizg_t* ofp)
    { *ofp = scnsizg_vect; scnsizg_vect = nfp; }
void ami_scnsizg(FILE* f, int* x, int* y) { (*scnsizg_vect)(f, x, y); }

static void scnsizg_ivf(FILE* f, int* x, int* y)

{

    XWindowAttributes xwa; /* XWindow attributes */
    winptr            win; /* pointer to windows context */
    Window            cw, pw, rw;
    Window*           cwl;
    unsigned          ncw;

    win = txt2win(f); /* get window context */
    XWLOCK();
    /* find parent */
    XQueryTree(padisplay, win->xwhan, &rw, &pw, &cwl, &ncw);
    /* get root parameters */
    XGetWindowAttributes(padisplay, rw, &xwa);
    XWUNLOCK();
    *x = xwa.width;
    *y = xwa.height;

}

/** ****************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

void _pa_scnsiz_ovr(ami_scnsiz_t nfp, ami_scnsiz_t* ofp)
    { *ofp = scnsiz_vect; scnsiz_vect = nfp; }
void ami_scnsiz(FILE* f, int* x, int* y) { (*scnsiz_vect)(f, x, y); }

static void scnsiz_ivf(FILE* f, int* x, int* y)

{

    ami_scnsizg(f, x, y); /* execute */
    *x = *x/stdchrx; /* convert to "standard character" size */
    *y = *y/stdchry;

}

/** ****************************************************************************

Get screen center graphical

Finds the center of the screen which contains the given window. This is usually
used to locate where dialogs will go. It could be another location depending on
the system.

The old system was to simply locate the dialogs in the middle of the screen,
which fails (or at least is not the optimal placement) in the case of multiple
screens that are joined at one or more sides.

*******************************************************************************/

void _pa_scnceng_ovr(ami_scnceng_t nfp, ami_scnceng_t* ofp)
    { *ofp = scnceng_vect; scnceng_vect = nfp; }
void ami_scnceng(FILE* f, int* x, int* y) { (*scnceng_vect)(f, x, y); }

static void scnceng_ivf(FILE* f, int* x, int* y)

{

    /* give fixed position for testing */
    *x = 5120/2;
    *y = 5760/4;

}

/** ****************************************************************************

Get screen center character

Finds the center of the screen which contains the given window. This is usually
used to locate where dialogs will go. It could be another location depending on
the system.

The old system was to simply locate the dialogs in the middle of the screen,
which fails (or at least is not the optimal placement) in the case of multiple
screens that are joined at one or more sides.

*******************************************************************************/

void _pa_scncen_ovr(ami_scncen_t nfp, ami_scncen_t* ofp)
    { *ofp = scncen_vect; scncen_vect = nfp; }
void ami_scncen(FILE* f, int* x, int* y) { (*scncen_vect)(f, x, y); }

static void scncen_ivf(FILE* f, int* x, int* y)

{

    ami_scnceng(f, x, y); /* execute */
    *x = *x/stdchrx; /* convert to "standard character" size */
    *y = *y/stdchry;

}

/** ****************************************************************************

Find window size from client

Finds the window size, in parent terms, needed to result in a given client
window size.

Note: this routine should be able to find the minimum size of a window using
the given style, and return the minimums if the input size is lower than this.
This does not seem to be obvious under Windows.

Do we also need a menu style type ?

*******************************************************************************/

void _pa_winclientg_ovr(ami_winclientg_t nfp, ami_winclientg_t* ofp)
    { *ofp = winclientg_vect; winclientg_vect = nfp; }
void ami_winclientg(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)
    { (*winclientg_vect)(f, cx, cy, wx, wy, ms); }

static void winclientg_ivf(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    *wx = cx+frmextwdt[frmcfgall]; /* find framed size */
    *wy = cy+frmexthgt[frmcfgall];
    /* we only allow one frame mode at a time, so we process here in priority
       order */
    if (!(BIT(ami_wmframe) & ms)) {

        *wx = cx+frmextwdt[frmcfgfrm]; /* find no frame size */
        *wy = cy+frmexthgt[frmcfgfrm];

    } else if (!(BIT(ami_wmsize) & ms)) {

        *wx = cx+frmextwdt[frmcfgsiz]; /* find no size bar size */
        *wy = cy+frmexthgt[frmcfgsiz];

    } else if (!(BIT(ami_wmsysbar) & ms)) {

        *wx = cx+frmextwdt[frmcfgsys]; /* find no system bar size */
        *wy = cy+frmexthgt[frmcfgsys];

    }

}

void _pa_winclient_ovr(ami_winclient_t nfp, ami_winclient_t* ofp)
    { *ofp = winclient_vect; winclient_vect = nfp; }
void ami_winclient(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)
    { (*winclient_vect)(f, cx, cy, wx, wy, ms); }

static void winclient_ivf(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)

{

    winptr win, par; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* execute */
    ami_winclientg(f, cx*win->charspace, cy*win->linespace, wx, wy, ms);
    /* find character based sizes */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *wx = (*wx-1) / par->charspace+1;
        *wy = (*wy-1) / par->linespace+1;

    } else {

        /* find character based sizes */
        *wx = (*wx-1) / stdchrx+1;
        *wy = (*wy-1) / stdchry+1;

    }

}

/** ****************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

void _pa_frame_ovr(ami_frame_t nfp, ami_frame_t* ofp)
    { *ofp = frame_vect; frame_vect = nfp; }
void ami_frame(FILE* f, int e) { (*frame_vect)(f, e); }

static void frame_ivf(FILE* f, int e)

{

    winptr win; /* pointer to windows context */
    Atom mwmHintsProperty;
    mwmhints hints;
    XWindowChanges xwc; /* XWindow values */
    int chg;            /* geometry change */

    win = txt2win(f); /* get window context */
    chg = win->frame != !!e; /* set frame has changed */
    win->frame = FALSE; /* turn off all frame configures */
    win->size = FALSE;
    win->sysbar = FALSE;
    win->frame = !!e; /* set new status of frame */

    if (win->childfrm) {

        /* Ami-drawn child frame: adjust offsets and geometry */
        if (e) {

            win->pfw = CFRM_BORDER_W * 2;
            win->pfh = CFRM_TITBAR_H(win) + CFRM_BORDER_W;
            win->cwox = CFRM_BORDER_W;
            win->cwoy = CFRM_TITBAR_H(win);

        } else {

            win->pfw = 0;
            win->pfh = 0;
            win->cwox = 0;
            win->cwoy = 0;

        }

        if (chg) {

            /* resize master window */
            win->xmwr.w = win->gmaxxg + win->pfw;
            win->xmwr.h = win->gmaxyg + win->pfh;
            XWLOCK();
            XResizeWindow(padisplay, win->xmwhan,
                          win->xmwr.w, win->xmwr.h);
            /* reposition and resize subclient within master */
            XMoveResizeWindow(padisplay, win->xwhan,
                              win->cwox, win->cwoy,
                              win->gmaxxg, win->gmaxyg);
            /* when frame is turned off, clear the rounded-corner shape
               mask so the client window presents as a plain rectangle */
            if (!e) {

                XRectangle r;
                r.x = 0;
                r.y = 0;
                r.width  = win->xmwr.w;
                r.height = win->xmwr.h;
                XShapeCombineRectangles(padisplay, win->xmwhan,
                                        ShapeBounding, 0, 0, &r, 1,
                                        ShapeSet, Unsorted);

            }
            XWUNLOCK();

            restore(win);
            /* redraw the frame (and re-apply the shape mask) when frame
               is turned back on */
            if (e) childfrm_draw(win);

        }

    } else {

        /* top-level: use WM frame */
        enbxfrm(win->xmwhan, e); /* enable/disable frame */

        if (e) {

            win->pfw = frmextwdt[frmcfgall];
            win->pfh = frmexthgt[frmcfgall];
            win->cwox = frmoffx[frmcfgall];
            win->cwoy = frmoffy[frmcfgall];

        } else {

            win->pfw = frmextwdt[frmcfgfrm];
            win->pfh = frmexthgt[frmcfgfrm];
            win->cwox = frmoffx[frmcfgfrm];
            win->cwoy = frmoffy[frmcfgfrm];

        }

        if (chg) {

            xwc.width = win->gmaxxg;
            xwc.height = win->gmaxyg;
            XWLOCK();
            XConfigureWindow(padisplay, win->xmwhan, CWWidth|CWHeight, &xwc);
            XWUNLOCK();

            win->xmwr.w = win->gmaxxg;
            win->xmwr.h = win->gmaxyg;

            restore(win);

        }

    }

}

/** ****************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

On GNOME/Ubuntu 20.04 with GDM3 window manager, we are not capable of turning
off the size bars alone, so this is a no-op. It may work on other window
managers.

*******************************************************************************/

void _pa_sizable_ovr(ami_sizable_t nfp, ami_sizable_t* ofp)
    { *ofp = sizable_vect; sizable_vect = nfp; }
void ami_sizable(FILE* f, int e) { (*sizable_vect)(f, e); }

static void sizable_ivf(FILE* f, int e)

{

    winptr win; /* pointer to windows context */
    Atom mwmHintsProperty;
    mwmhints hints;
    XWindowChanges xwc; /* XWindow values */
    int chg;            /* geometry change */

    win = txt2win(f); /* get window context */
    chg = win->size != !!e; /* set size has changed */
    win->frame = FALSE; /* turn off all frame configures */
    win->size = FALSE;
    win->sysbar = FALSE;
    win->size = !!e; /* set new status of size bars */
    enbxsiz(win->xmwhan, e); /* enable/disable size bars */

    if (e) {

        /* find net extra width of frame from client area */
        win->pfw = frmextwdt[frmcfgall];
        win->pfh = frmexthgt[frmcfgall];
        /* find offset from parent origin to client origin */
        win->cwox = frmoffx[frmcfgall];
        win->cwoy = frmoffy[frmcfgall];

    } else {

        /* find net extra width of frame from client area */
        win->pfw = frmextwdt[frmcfgsiz];
        win->pfh = frmexthgt[frmcfgsiz];
        /* find offset from parent origin to client origin */
        win->cwox = frmoffx[frmcfgsiz];
        win->cwoy = frmoffy[frmcfgsiz];

    }

    if (chg) { /* geometry has changed */

        xwc.width = win->gmaxxg; /* set XWindow width and height */
        xwc.height = win->gmaxyg;
        XWLOCK();
        XConfigureWindow(padisplay, win->xmwhan, CWWidth|CWHeight, &xwc);
        XWUNLOCK();

        /* set new size */
        win->xmwr.w = win->gmaxxg;
        win->xmwr.h = win->gmaxyg;

        restore(win); /* restore buffer to screen */

    }

}

/** ****************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

I don't think XWindow can do this separately. Instead, the frame() function is
used to create component windows.

*******************************************************************************/

void _pa_sysbar_ovr(ami_sysbar_t nfp, ami_sysbar_t* ofp)
    { *ofp = sysbar_vect; sysbar_vect = nfp; }
void ami_sysbar(FILE* f, int e) { (*sysbar_vect)(f, e); }

static void sysbar_ivf(FILE* f, int e)

{

    winptr win; /* pointer to windows context */
    Atom mwmHintsProperty;
    mwmhints hints;
    XWindowChanges xwc; /* XWindow values */
    int chg;            /* geometry change */

    win = txt2win(f); /* get window context */
    chg = win->sysbar != !!e; /* set sysbar has changed */
    win->frame = FALSE; /* turn off all frame configures */
    win->size = FALSE;
    win->sysbar = FALSE;
    win->sysbar = !!e; /* set new status of system bar */
    enbxsys(win->xmwhan, e); /* enable/disable system bar */

    if (e) {

        /* find net extra width of frame from client area */
        win->pfw = frmextwdt[frmcfgall];
        win->pfh = frmexthgt[frmcfgall];
        /* find offset from parent origin to client origin */
        win->cwox = frmoffx[frmcfgall];
        win->cwoy = frmoffy[frmcfgall];

    } else {

        /* find net extra width of frame from client area */
        win->pfw = frmextwdt[frmcfgsys];
        win->pfh = frmexthgt[frmcfgsys];
        /* find offset from parent origin to client origin */
        win->cwox = frmoffx[frmcfgsys];
        win->cwoy = frmoffy[frmcfgsys];

    }

    if (chg) { /* geometry has changed */

        xwc.width = win->gmaxxg; /* set XWindow width and height */
        xwc.height = win->gmaxyg;
        XWLOCK();
        XConfigureWindow(padisplay, win->xmwhan, CWWidth|CWHeight, &xwc);
        XWUNLOCK();

        /* set new size */
        win->xmwr.w = win->gmaxxg;
        win->xmwr.h = win->gmaxyg;

        restore(win); /* restore buffer to screen */

    }

}

/** ****************************************************************************

Set window focus

Sends the focus, or which window gets input characters, to a given window.

*******************************************************************************/

void _pa_focus_ovr(ami_focus_t nfp, ami_focus_t* ofp)
    { *ofp = focus_vect; focus_vect = nfp; }
void ami_focus(FILE* f) { (*focus_vect)(f); }

static void focus_ivf(FILE* f)

{

    winptr win; /* pointer to windows context */

    win = txt2win(f); /* get window context */
    XWLOCK();
    XSetInputFocus(padisplay, win->xmwhan, RevertToNone, CurrentTime);
    XWUNLOCK();

}

/** ****************************************************************************

Set character drawing angle

Sets the angle or path at which characters are drawn. The angle is 0 up,
clockwise to 360, in INT_MAX ratio. The normal setting is INT_MAX/4 or 90
degrees.

This cannot be set with auto on (even to 90 degrees), nor can auto be enabled if
the angle is not 90 degrees and on grid. The background is rotated along with
the characters, and the cursor position will be updated along with the character
path.

In XWindows, the rotation is done in software, so the user can expect the
performance of off-angle drawing to be compromsed. At this time only the 90
degree case is optimized, but it is possible to optimize at 90 degree
increments.

*******************************************************************************/

void _pa_path_ovr(ami_path_t nfp, ami_path_t* ofp)
    { *ofp = path_vect; path_vect = nfp; }
void ami_path(FILE* f, int a) { (*path_vect)(f, a); }

static void path_ivf(FILE* f, int a)

{

    winptr win; /* pointer to windows context */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window context */
    sc = win->screens[win->curupd-1];
    if (sc->autof) error(eangato); /* autowrap is on */
    sc->angle = a; /* set drawing angle */

}

/** ****************************************************************************

Widget extention package

There are no native widgets in XWindows. This section is a series of
overrideable vectors. Each vector will normally give an "unimplemented" error.
The widget package for XWindows will be supplied by an add-on package.

*******************************************************************************/

void _pa_getwigid_ovr(ami_getwigid_t nfp, ami_getwigid_t* ofp)
    { *ofp = getwigid_vect; getwigid_vect = nfp; }
int ami_getwigid(FILE* f) { return ((*getwigid_vect)(f)); }

static int getwigid_ivf(FILE* f) { error(egetwigid_unimp); return (0); }

void _pa_killwidget_ovr(ami_killwidget_t nfp, ami_killwidget_t* ofp)
    { *ofp = killwidget_vect; killwidget_vect = nfp; }
void ami_killwidget(FILE* f, int id) { (*killwidget_vect)(f, id); }

static void killwidget_ivf(FILE* f, int id) { error(ekillwidget_unimp); }

void _pa_selectwidget_ovr(ami_selectwidget_t nfp, ami_selectwidget_t* ofp)
    { *ofp = selectwidget_vect; selectwidget_vect = nfp; }
void ami_selectwidget(FILE* f, int id, int e)
    { (*selectwidget_vect)(f, id, e); }

static void selectwidget_ivf(FILE* f, int id, int e)
    { error(eselectwidget_unimp); }

void _pa_enablewidget_ovr(ami_enablewidget_t nfp, ami_enablewidget_t* ofp)
    { *ofp = enablewidget_vect; enablewidget_vect = nfp; }
void ami_enablewidget(FILE* f, int id, int e)
    { (*enablewidget_vect)(f, id, e); }

static void enablewidget_ivf(FILE* f, int id, int e)
    { error(eenablewidget_unimp); }

void _pa_getwidgettext_ovr(ami_getwidgettext_t nfp, ami_getwidgettext_t* ofp)
    { *ofp = getwidgettext_vect; getwidgettext_vect = nfp; }
void ami_getwidgettext(FILE* f, int id, char* s, int sl)
    { (*getwidgettext_vect)(f, id, s, sl); }

static void getwidgettext_ivf(FILE* f, int id, char* s, int sl)
    { error(egetwidgettext_unimp); }

void _pa_putwidgettext_ovr(ami_putwidgettext_t nfp, ami_putwidgettext_t* ofp)
    { *ofp = putwidgettext_vect; putwidgettext_vect = nfp; }
void ami_putwidgettext(FILE* f, int id, char* s)
    { (*putwidgettext_vect)(f, id, s); }

static void putwidgettext_ivf(FILE* f, int id, char* s)
    { error(eputwidgettext_unimp); }

void _pa_sizwidget_ovr(ami_sizwidget_t nfp, ami_sizwidget_t* ofp)
    { *ofp = sizwidget_vect; sizwidget_vect = nfp; }
void ami_sizwidget(FILE* f, int id, int x, int y)
    { (*sizwidget_vect)(f, id, x, y); }

static void sizwidget_ivf(FILE* f, int id, int x, int y)
    { error(esizwidget_unimp); }

void _pa_sizwidgetg_ovr(ami_sizwidgetg_t nfp, ami_sizwidgetg_t* ofp)
    { *ofp = sizwidgetg_vect; sizwidgetg_vect = nfp; }
void ami_sizwidgetg(FILE* f, int id, int x, int y)
    { (*sizwidgetg_vect)(f, id, x, y); }

static void sizwidgetg_ivf(FILE* f, int id, int x, int y)
    { error(esizwidgetg_unimp); }

void _pa_poswidget_ovr(ami_poswidget_t nfp, ami_poswidget_t* ofp)
    { *ofp = poswidget_vect; poswidget_vect = nfp; }
void ami_poswidget(FILE* f, int id, int x, int y)
    { (*poswidget_vect)(f, id, x, y); }

static void poswidget_ivf(FILE* f, int id, int x, int y)
    { error(eposwidget_unimp); }

void _pa_poswidgetg_ovr(ami_poswidgetg_t nfp, ami_poswidgetg_t* ofp)
    { *ofp = poswidgetg_vect; poswidgetg_vect = nfp; }
void ami_poswidgetg(FILE* f, int id, int x, int y)
    { (*poswidgetg_vect)(f, id, x, y); }

static void poswidgetg_ivf(FILE* f, int id, int x, int y)
    { error(eposwidgetg_unimp); }

void _pa_backwidget_ovr(ami_backwidget_t nfp, ami_backwidget_t* ofp)
    { *ofp = backwidget_vect; backwidget_vect = nfp; }
void ami_backwidget(FILE* f, int id) { (*backwidget_vect)(f, id); }

static void backwidget_ivf(FILE* f, int id) { error(ebackwidget_unimp); }

void _pa_frontwidget_ovr(ami_frontwidget_t nfp, ami_frontwidget_t* ofp)
    { *ofp = frontwidget_vect; frontwidget_vect = nfp; }
void ami_frontwidget(FILE* f, int id) { (*frontwidget_vect)(f, id); }

static void frontwidget_ivf(FILE* f, int id) { error(efrontwidget_unimp); }

void _pa_focuswidget_ovr(ami_focuswidget_t nfp, ami_focuswidget_t* ofp)
    { *ofp = focuswidget_vect; focuswidget_vect = nfp; }
void ami_focuswidget(FILE* f, int id) { (*focuswidget_vect)(f, id); }

static void focuswidget_ivf(FILE* f, int id) { error(efocuswidget_unimp); }

void _pa_buttonsiz_ovr(ami_buttonsiz_t nfp, ami_buttonsiz_t* ofp)
    { *ofp = buttonsiz_vect; buttonsiz_vect = nfp; }
void ami_buttonsiz(FILE* f, char* s, int* w, int* h)
    { (*buttonsiz_vect)(f, s, w, h); }

static void buttonsiz_ivf(FILE* f, char* s, int* w, int* h)
    { error(ebuttonsiz_unimp); }

void _pa_buttonsizg_ovr(ami_buttonsizg_t nfp, ami_buttonsizg_t* ofp)
    { *ofp = buttonsizg_vect; buttonsizg_vect = nfp; }
void ami_buttonsizg(FILE* f, char* s, int* w, int* h)
    { (*buttonsizg_vect)(f, s, w, h); }

static void buttonsizg_ivf(FILE* f, char* s, int* w, int* h)
    { error(ebuttonsizg_unimp); }

void _pa_button_ovr(ami_button_t nfp, ami_button_t* ofp)
    { *ofp = button_vect; button_vect = nfp; }
void ami_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*button_vect)(f, x1, y1, x2, y2, s, id); }

static void button_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(ebutton_unimp); }

void _pa_buttong_ovr(ami_buttong_t nfp, ami_buttong_t* ofp)
    { *ofp = buttong_vect; buttong_vect = nfp; }
void ami_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*buttong_vect)(f, x1, y1, x2, y2, s, id); }

static void buttong_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(ebuttong_unimp); }

void _pa_checkboxsiz_ovr(ami_checkboxsiz_t nfp, ami_checkboxsiz_t* ofp)
    { *ofp = checkboxsiz_vect; checkboxsiz_vect = nfp; }
void ami_checkboxsiz(FILE* f, char* s, int* w, int* h)
    { (*checkboxsiz_vect)(f, s, w, h); }

static void checkboxsiz_ivf(FILE* f, char* s, int* w, int* h)
    { error(echeckboxsiz_unimp); }

void _pa_checkboxsizg_ovr(ami_checkboxsizg_t nfp, ami_checkboxsizg_t* ofp)
    { *ofp = checkboxsizg_vect; checkboxsizg_vect = nfp; }
void ami_checkboxsizg(FILE* f, char* s, int* w, int* h)
    { (*checkboxsizg_vect)(f, s, w, h); }

static void checkboxsizg_ivf(FILE* f, char* s, int* w, int* h)
    { error(echeckboxsizg_unimp); }

void _pa_checkbox_ovr(ami_checkbox_t nfp, ami_checkbox_t* ofp)
    { *ofp = checkbox_vect; checkbox_vect = nfp; }
void ami_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*checkbox_vect)(f, x1, y1, x2, y2, s, id); }

static void checkbox_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(echeckbox_unimp); }

void _pa_checkboxg_ovr(ami_checkboxg_t nfp, ami_checkboxg_t* ofp)
    { *ofp = checkboxg_vect; checkboxg_vect = nfp; }
void ami_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*checkboxg_vect)(f, x1, y1, x2, y2, s, id); }

static void checkboxg_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(echeckboxg_unimp); }

void _pa_radiobuttonsiz_ovr(ami_radiobuttonsiz_t nfp, ami_radiobuttonsiz_t* ofp)
    { *ofp = radiobuttonsiz_vect; radiobuttonsiz_vect = nfp; }
void ami_radiobuttonsiz(FILE* f, char* s, int* w, int* h)
    { (*radiobuttonsiz_vect)(f, s, w, h); }

static void radiobuttonsiz_ivf(FILE* f, char* s, int* w, int* h)
    { error(eradiobuttonsiz_unimp); }

void _pa_radiobuttonsizg_ovr(ami_radiobuttonsizg_t nfp, ami_radiobuttonsizg_t* ofp)
    { *ofp = radiobuttonsizg_vect; radiobuttonsizg_vect = nfp; }
void ami_radiobuttonsizg(FILE* f, char* s, int* w, int* h)
    { (*radiobuttonsizg_vect)(f, s, w, h); }

static void radiobuttonsizg_ivf(FILE* f, char* s, int* w, int* h)
    { error(eradiobuttonsizg_unimp); }

void _pa_radiobutton_ovr(ami_radiobutton_t nfp, ami_radiobutton_t* ofp)
    { *ofp = radiobutton_vect; radiobutton_vect = nfp; }
void ami_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*radiobutton_vect)(f, x1, y1, x2, y2, s, id); }

static void radiobutton_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(eradiobutton_unimp); }

void _pa_radiobuttong_ovr(ami_radiobuttong_t nfp, ami_radiobuttong_t* ofp)
    { *ofp = radiobuttong_vect; radiobuttong_vect = nfp; }
void ami_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*radiobuttong_vect)(f, x1, y1, x2, y2, s, id); }

static void radiobuttong_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(eradiobuttong_unimp); }

void _pa_groupsizg_ovr(ami_groupsizg_t nfp, ami_groupsizg_t* ofp)
    { *ofp = groupsizg_vect; groupsizg_vect = nfp; }
void ami_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy)
    { (*groupsizg_vect)(f, s, cw, ch, w, h, ox, oy); }

static void groupsizg_ivf(FILE* f, char* s, int cw, int ch, int* w, int* h,
                          int* ox, int* oy)
    { error(egroupsizg_unimp); }

void _pa_groupsiz_ovr(ami_groupsiz_t nfp, ami_groupsiz_t* ofp)
    { *ofp = groupsiz_vect; groupsiz_vect = nfp; }
void ami_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy)
    { (*groupsiz_vect)(f, s, cw, ch, w, h, ox, oy); }

static void groupsiz_ivf(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                 int* oy) { error(egroupsiz_unimp); }

void _pa_group_ovr(ami_group_t nfp, ami_group_t* ofp)
    { *ofp = group_vect; group_vect = nfp; }
void ami_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*group_vect)(f, x1, y1, x2, y2, s, id); }

static void group_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(egroup_unimp); }

void _pa_groupg_ovr(ami_groupg_t nfp, ami_groupg_t* ofp)
    { *ofp = groupg_vect; groupg_vect = nfp; }
void ami_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { (*groupg_vect)(f, x1, y1, x2, y2, s, id); }

static void groupg_ivf(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
    { error(egroupg_unimp); }

void _pa_background_ovr(ami_background_t nfp, ami_background_t* ofp)
    { *ofp = background_vect; background_vect = nfp; }
void ami_background(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*background_vect)(f, x1, y1, x2, y2, id); }

static void background_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(ebackground_unimp); }

void _pa_backgroundg_ovr(ami_backgroundg_t nfp, ami_backgroundg_t* ofp)
    { *ofp = backgroundg_vect; backgroundg_vect = nfp; }
void ami_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*backgroundg_vect)(f, x1, y1, x2, y2, id); }

static void backgroundg_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(ebackgroundg_unimp); }

void _pa_scrollvertsizg_ovr(ami_scrollvertsizg_t nfp, ami_scrollvertsizg_t* ofp)
    { *ofp = scrollvertsizg_vect; scrollvertsizg_vect = nfp; }
void ami_scrollvertsizg(FILE* f, int* w, int* h)
    { (*scrollvertsizg_vect)(f, w, h); }

static void scrollvertsizg_ivf(FILE* f, int* w, int* h)
    { error(escrollvertsizg_unimp); }

void _pa_scrollvertsiz_ovr(ami_scrollvertsiz_t nfp, ami_scrollvertsiz_t* ofp)
    { *ofp = scrollvertsiz_vect; scrollvertsiz_vect = nfp; }
void ami_scrollvertsiz(FILE* f, int* w, int* h)
    { (*scrollvertsiz_vect)(f, w, h); }

static void scrollvertsiz_ivf(FILE* f, int* w, int* h)
    { error(escrollvertsiz_unimp); }

void _pa_scrollvert_ovr(ami_scrollvert_t nfp, ami_scrollvert_t* ofp)
    { *ofp = scrollvert_vect; scrollvert_vect = nfp; }
void ami_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*scrollvert_vect)(f, x1, y1, x2, y2, id); }

static void scrollvert_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(escrollvert_unimp); }

void _pa_scrollvertg_ovr(ami_scrollvertg_t nfp, ami_scrollvertg_t* ofp)
    { *ofp = scrollvertg_vect; scrollvertg_vect = nfp; }
void ami_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*scrollvertg_vect)(f, x1, y1, x2, y2, id); }

static void scrollvertg_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(escrollvertg_unimp); }

void _pa_scrollhorizsizg_ovr(ami_scrollhorizsizg_t nfp, ami_scrollhorizsizg_t* ofp)
    { *ofp = scrollhorizsizg_vect; scrollhorizsizg_vect = nfp; }
void ami_scrollhorizsizg(FILE* f, int* w, int* h)
    { (*scrollhorizsizg_vect)(f, w, h); }

static void scrollhorizsizg_ivf(FILE* f, int* w, int* h)
    { error(escrollhorizsizg_unimp); }

void _pa_scrollhorizsiz_ovr(ami_scrollhorizsiz_t nfp, ami_scrollhorizsiz_t* ofp)
    { *ofp = scrollhorizsiz_vect; scrollhorizsiz_vect = nfp; }
void ami_scrollhorizsiz(FILE* f, int* w, int* h)
    { (*scrollhorizsiz_vect)(f, w, h); }

static void scrollhorizsiz_ivf(FILE* f, int* w, int* h)
    { error(escrollhorizsiz_unimp); }

void _pa_scrollhoriz_ovr(ami_scrollhoriz_t nfp, ami_scrollhoriz_t* ofp)
    { *ofp = scrollhoriz_vect; scrollhoriz_vect = nfp; }
void ami_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*scrollhoriz_vect)(f, x1, y1, x2, y2, id); }

static void scrollhoriz_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(escrollhoriz_unimp); }

void _pa_scrollhorizg_ovr(ami_scrollhorizg_t nfp, ami_scrollhorizg_t* ofp)
    { *ofp = scrollhorizg_vect; scrollhorizg_vect = nfp; }
void ami_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*scrollhorizg_vect)(f, x1, y1, x2, y2, id); }

static void scrollhorizg_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(escrollhorizg_unimp); }

void _pa_scrollpos_ovr(ami_scrollpos_t nfp, ami_scrollpos_t* ofp)
    { *ofp = scrollpos_vect; scrollpos_vect = nfp; }
void ami_scrollpos(FILE* f, int id, int r)
    { (*scrollpos_vect)(f, id, r); }

static void scrollpos_ivf(FILE* f, int id, int r) { error(escrollpos_unimp); }

void _pa_scrollsiz_ovr(ami_scrollsiz_t nfp, ami_scrollsiz_t* ofp)
    { *ofp = scrollsiz_vect; scrollsiz_vect = nfp; }
void ami_scrollsiz(FILE* f, int id, int r)
    { (*scrollsiz_vect)(f, id, r); }

static void scrollsiz_ivf(FILE* f, int id, int r) { error(escrollsiz_unimp); }

void _pa_numselboxsizg_ovr(ami_numselboxsizg_t nfp, ami_numselboxsizg_t* ofp)
    { *ofp = numselboxsizg_vect; numselboxsizg_vect = nfp; }
void ami_numselboxsizg(FILE* f, int l, int u, int* w, int* h)
    { (*numselboxsizg_vect)(f, l, u, w, h); }

static void numselboxsizg_ivf(FILE* f, int l, int u, int* w, int* h)
    { error(enumselboxsizg_unimp); }

void _pa_numselboxsiz_ovr(ami_numselboxsiz_t nfp, ami_numselboxsiz_t* ofp)
    { *ofp = numselboxsiz_vect; numselboxsiz_vect = nfp; }
void ami_numselboxsiz(FILE* f, int l, int u, int* w, int* h)
    { (*numselboxsiz_vect)(f, l, u, w, h); }

static void numselboxsiz_ivf(FILE* f, int l, int u, int* w, int* h)
    { error(enumselboxsiz_unimp); }

void _pa_numselbox_ovr(ami_numselbox_t nfp, ami_numselbox_t* ofp)
    { *ofp = numselbox_vect; numselbox_vect = nfp; }
void ami_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)
    { (*numselbox_vect)(f, x1, y1, x2, y2, l, u, id); }

static void numselbox_ivf(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                  int id) { error(enumselbox_unimp); }

void _pa_numselboxg_ovr(ami_numselboxg_t nfp, ami_numselboxg_t* ofp)
    { *ofp = numselboxg_vect; numselboxg_vect = nfp; }
void ami_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)
    { (*numselboxg_vect)(f, x1, y1, x2, y2, l, u, id); }

static void numselboxg_ivf(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                   int id) { error(enumselboxg_unimp); }

void _pa_editboxsizg_ovr(ami_editboxsizg_t nfp, ami_editboxsizg_t* ofp)
    { *ofp = editboxsizg_vect; editboxsizg_vect = nfp; }
void ami_editboxsizg(FILE* f, char* s, int* w, int* h)
    { (*editboxsizg_vect)(f, s, w, h); }

static void editboxsizg_ivf(FILE* f, char* s, int* w, int* h)
    { error(eeditboxsizg_unimp); }

void _pa_editboxsiz_ovr(ami_editboxsiz_t nfp, ami_editboxsiz_t* ofp)
    { *ofp = editboxsiz_vect; editboxsiz_vect = nfp; }
void ami_editboxsiz(FILE* f, char* s, int* w, int* h)
    { (*editboxsiz_vect)(f, s, w, h); }

static void editboxsiz_ivf(FILE* f, char* s, int* w, int* h)
    { error(eeditboxsiz_unimp); }

void _pa_editbox_ovr(ami_editbox_t nfp, ami_editbox_t* ofp)
    { *ofp = editbox_vect; editbox_vect = nfp; }
void ami_editbox(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*editbox_vect)(f, x1, y1, x2, y2, id); }

static void editbox_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(eeditbox_unimp); }

void _pa_editboxg_ovr(ami_editboxg_t nfp, ami_editboxg_t* ofp)
    { *ofp = editboxg_vect; editboxg_vect = nfp; }
void ami_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*editboxg_vect)(f, x1, y1, x2, y2, id); }

static void editboxg_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(eeditboxg_unimp); }

void _pa_progbarsizg_ovr(ami_progbarsizg_t nfp, ami_progbarsizg_t* ofp)
    { *ofp = progbarsizg_vect; progbarsizg_vect = nfp; }
void ami_progbarsizg(FILE* f, int* w, int* h) { (*progbarsizg_vect)(f, w, h); }

static void progbarsizg_ivf(FILE* f, int* w, int* h)
    { error(eprogbarsizg_unimp); }

void _pa_progbarsiz_ovr(ami_progbarsiz_t nfp, ami_progbarsiz_t* ofp)
    { *ofp = progbarsiz_vect; progbarsiz_vect = nfp; }
void ami_progbarsiz(FILE* f, int* w, int* h) { (*progbarsiz_vect)(f, w, h); }

static void progbarsiz_ivf(FILE* f, int* w, int* h)
    { error(eprogbarsiz_unimp); }

void _pa_progbar_ovr(ami_progbar_t nfp, ami_progbar_t* ofp)
    { *ofp = progbar_vect; progbar_vect = nfp; }
void ami_progbar(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*progbar_vect)(f, x1, y1, x2, y2, id); }

static void progbar_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(eprogbar_unimp); }

void _pa_progbarg_ovr(ami_progbarg_t nfp, ami_progbarg_t* ofp)
    { *ofp = progbarg_vect; progbarg_vect = nfp; }
void ami_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)
    { (*progbarg_vect)(f, x1, y1, x2, y2, id); }

static void progbarg_ivf(FILE* f, int x1, int y1, int x2, int y2, int id)
    { error(eprogbarg_unimp); }

void _pa_progbarpos_ovr(ami_progbarpos_t nfp, ami_progbarpos_t* ofp)
    { *ofp = progbarpos_vect; progbarpos_vect = nfp; }
void ami_progbarpos(FILE* f, int id, int pos) { (*progbarpos_vect)(f, id, pos); }

static void progbarpos_ivf(FILE* f, int id, int pos)
    { error(eprogbarpos_unimp); }

void _pa_listboxsizg_ovr(ami_listboxsizg_t nfp, ami_listboxsizg_t* ofp)
    { *ofp = listboxsizg_vect; listboxsizg_vect = nfp; }
void ami_listboxsizg(FILE* f, ami_strptr sp, int* w, int* h)
    { (*listboxsizg_vect)(f, sp, w, h); }

static void listboxsizg_ivf(FILE* f, ami_strptr sp, int* w, int* h)
    { error(elistboxsizg_unimp); }

void _pa_listboxsiz_ovr(ami_listboxsiz_t nfp, ami_listboxsiz_t* ofp)
    { *ofp = listboxsiz_vect; listboxsiz_vect = nfp; }
void ami_listboxsiz(FILE* f, ami_strptr sp, int* w, int* h)
    { (*listboxsiz_vect)(f, sp, w, h); }

static void listboxsiz_ivf(FILE* f, ami_strptr sp, int* w, int* h)
    { error(elistboxsiz_unimp); }

void _pa_listbox_ovr(ami_listbox_t nfp, ami_listbox_t* ofp)
    { *ofp = listbox_vect; listbox_vect = nfp; }
void ami_listbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { (*listbox_vect)(f, x1, y1, x2, y2, sp, id); }

static void listbox_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { error(elistbox_unimp); }

void _pa_listboxg_ovr(ami_listboxg_t nfp, ami_listboxg_t* ofp)
    { *ofp = listboxg_vect; listboxg_vect = nfp; }
void ami_listboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { (*listboxg_vect)(f, x1, y1, x2, y2, sp, id); }

static void listboxg_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { error(elistboxg_unimp); }

void _pa_dropboxsizg_ovr(ami_dropboxsizg_t nfp, ami_dropboxsizg_t* ofp)
    { *ofp = dropboxsizg_vect; dropboxsizg_vect = nfp; }
void ami_dropboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { (*dropboxsizg_vect)(f, sp, cw, ch, ow, oh); }

static void dropboxsizg_ivf(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { error(edropboxsizg_unimp); }

void _pa_dropboxsiz_ovr(ami_dropboxsiz_t nfp, ami_dropboxsiz_t* ofp)
    { *ofp = dropboxsiz_vect; dropboxsiz_vect = nfp; }
void ami_dropboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { (*dropboxsiz_vect)(f, sp, cw, ch, ow, oh); }

static void dropboxsiz_ivf(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { error(edropboxsiz_unimp); }

void _pa_dropbox_ovr(ami_dropbox_t nfp, ami_dropbox_t* ofp)
    { *ofp = dropbox_vect; dropbox_vect = nfp; }
void ami_dropbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { (*dropbox_vect)(f, x1, y1, x2, y2, sp, id); }

static void dropbox_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { error(edropbox_unimp); }

void _pa_dropboxg_ovr(ami_dropboxg_t nfp, ami_dropboxg_t* ofp)
    { *ofp = dropboxg_vect; dropboxg_vect = nfp; }
void ami_dropboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { (*dropboxg_vect)(f, x1, y1, x2, y2, sp, id); }

static void dropboxg_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { error(edropboxg_unimp); }

void _pa_dropeditboxsizg_ovr(ami_dropeditboxsizg_t nfp, ami_dropeditboxsizg_t* ofp)
    { *ofp = dropeditboxsizg_vect; dropeditboxsizg_vect = nfp; }
void ami_dropeditboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { (*dropeditboxsizg_vect)(f, sp, cw, ch, ow, oh); }

static void dropeditboxsizg_ivf(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { error(edropeditboxsizg_unimp); }

void _pa_dropeditboxsiz_ovr(ami_dropeditboxsiz_t nfp, ami_dropeditboxsiz_t* ofp)
    { *ofp = dropeditboxsiz_vect; dropeditboxsiz_vect = nfp; }
void ami_dropeditboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { (*dropeditboxsiz_vect)(f, sp, cw, ch, ow, oh); }

static void dropeditboxsiz_ivf(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
    { error(edropeditboxsiz_unimp); }

void _pa_dropeditbox_ovr(ami_dropeditbox_t nfp, ami_dropeditbox_t* ofp)
    { *ofp = dropeditbox_vect; dropeditbox_vect = nfp; }
void ami_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { (*dropeditbox_vect)(f, x1, y1, x2, y2, sp, id); }

static void dropeditbox_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { error(edropeditbox_unimp); }

void _pa_dropeditboxg_ovr(ami_dropeditboxg_t nfp, ami_dropeditboxg_t* ofp)
    { *ofp = dropeditboxg_vect; dropeditboxg_vect = nfp; }
void ami_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)
    { (*dropeditboxg_vect)(f, x1, y1, x2, y2, sp, id); }

static void dropeditboxg_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
                             int id)
    { error(edropeditboxg_unimp); }

void _pa_slidehorizsizg_ovr(ami_slidehorizsizg_t nfp, ami_slidehorizsizg_t* ofp)
    { *ofp = slidehorizsizg_vect; slidehorizsizg_vect = nfp; }
void ami_slidehorizsizg(FILE* f, int* w, int* h) { (*slidehorizsizg_vect)(f, w, h); }

static void slidehorizsizg_ivf(FILE* f, int* w, int* h)
    { error(eslidehorizsizg_unimp); }

void _pa_slidehorizsiz_ovr(ami_slidehorizsiz_t nfp, ami_slidehorizsiz_t* ofp)
    { *ofp = slidehorizsiz_vect; slidehorizsiz_vect = nfp; }
void ami_slidehorizsiz(FILE* f, int* w, int* h)
    { (*slidehorizsiz_vect)(f, w, h); }

static void slidehorizsiz_ivf(FILE* f, int* w, int* h)
    { error(eslidehorizsiz_unimp); }

void _pa_slidehoriz_ovr(ami_slidehoriz_t nfp, ami_slidehoriz_t* ofp)
    { *ofp = slidehoriz_vect; slidehoriz_vect = nfp; }
void ami_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { (*slidehoriz_vect)(f, x1, y1, x2, y2, mark, id); }

static void slidehoriz_ivf(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { error(eslidehoriz_unimp); }

void _pa_slidehorizg_ovr(ami_slidehorizg_t nfp, ami_slidehorizg_t* ofp)
    { *ofp = slidehorizg_vect; slidehorizg_vect = nfp; }
void ami_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { (*slidehorizg_vect)(f, x1, y1, x2, y2, mark, id); }

static void slidehorizg_ivf(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { error(eslidehorizg_unimp); }

void _pa_slidevertsizg_ovr(ami_slidevertsizg_t nfp, ami_slidevertsizg_t* ofp)
    { *ofp = slidevertsizg_vect; slidevertsizg_vect = nfp; }
void ami_slidevertsizg(FILE* f, int* w, int* h)
    { (*slidevertsizg_vect)(f, w, h); }

static void slidevertsizg_ivf(FILE* f, int* w, int* h)
    { error(eslidevertsizg_unimp); }

void _pa_slidevertsiz_ovr(ami_slidevertsiz_t nfp, ami_slidevertsiz_t* ofp)
    { *ofp = slidevertsiz_vect; slidevertsiz_vect = nfp; }
void ami_slidevertsiz(FILE* f, int* w, int* h) { (*slidevertsiz_vect)(f, w, h); }

static void slidevertsiz_ivf(FILE* f, int* w, int* h)
    { error(eslidevertsiz_unimp); }

void _pa_slidevert_ovr(ami_slidevert_t nfp, ami_slidevert_t* ofp)
    { *ofp = slidevert_vect; slidevert_vect = nfp; }
void ami_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { (*slidevert_vect)(f, x1, y1, x2, y2, mark, id); }

static void slidevert_ivf(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { error(eslidevert_unimp); }

void _pa_slidevertg_ovr(ami_slidevertg_t nfp, ami_slidevertg_t* ofp)
    { *ofp = slidevertg_vect; slidevertg_vect = nfp; }
void ami_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { (*slidevertg_vect)(f, x1, y1, x2, y2, mark, id); }

static void slidevertg_ivf(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
    { error(eslidevertg_unimp); }

void _pa_tabbarsizg_ovr(ami_tabbarsizg_t nfp, ami_tabbarsizg_t* ofp)
    { *ofp = tabbarsizg_vect; tabbarsizg_vect = nfp; }
void ami_tabbarsizg(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy)
    { (*tabbarsizg_vect)(f, tor, cw, ch, w, h, ox, oy); }

static void tabbarsizg_ivf(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h,
                           int* ox, int* oy)
    { error(etabbarsizg_unimp); }

void _pa_tabbarsiz_ovr(ami_tabbarsiz_t nfp, ami_tabbarsiz_t* ofp)
    { *ofp = tabbarsiz_vect; tabbarsiz_vect = nfp; }
void ami_tabbarsiz(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy)
    { (*tabbarsiz_vect)(f, tor, cw, ch, w, h, ox, oy); }

static void tabbarsiz_ivf(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox,
                  int* oy)
    { error(etabbarsiz_unimp); }

void _pa_tabbarclientg_ovr(ami_tabbarclientg_t nfp, ami_tabbarclientg_t* ofp)
    { *ofp = tabbarclientg_vect; tabbarclientg_vect = nfp; }
void ami_tabbarclientg(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy)
    { (*tabbarclientg_vect)(f, tor, cw, ch, w, h, ox, oy); }

static void tabbarclientg_ivf(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                              int* ox, int* oy)
    { error(etabbarclientg_unimp); }

void _pa_tabbarclient_ovr(ami_tabbarclient_t nfp, ami_tabbarclient_t* ofp)
    { *ofp = tabbarclient_vect; tabbarclient_vect = nfp; }
void ami_tabbarclient(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy)
    { (*tabbarclient_vect)(f, tor, cw, ch, w, h, ox, oy); }

static void tabbarclient_ivf(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                     int* ox, int* oy)
    { error(etabbarclient_unimp); }

void _pa_tabbar_ovr(ami_tabbar_t nfp, ami_tabbar_t* ofp)
    { *ofp = tabbar_vect; tabbar_vect = nfp; }
void ami_tabbar(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, ami_tabori tor, int id)
    { (*tabbar_vect)(f, x1, y1, x2, y2, sp, tor, id); }

static void tabbar_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
               ami_tabori tor, int id)
    { error(etabbar_unimp); }

void _pa_tabbarg_ovr(ami_tabbarg_t nfp, ami_tabbarg_t* ofp)
    { *ofp = tabbarg_vect; tabbarg_vect = nfp; }
void ami_tabbarg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, ami_tabori tor, int id)
    { (*tabbarg_vect)(f, x1, y1, x2, y2, sp, tor, id); }

static void tabbarg_ivf(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
                ami_tabori tor, int id)
    { error(etabbarg_unimp); }

void _pa_tabsel_ovr(ami_tabsel_t nfp, ami_tabsel_t* ofp)
    { *ofp = tabsel_vect; tabsel_vect = nfp; }
void ami_tabsel(FILE* f, int id, int tn) { (*tabsel_vect)(f, id, tn); }

static void tabsel_ivf(FILE* f, int id, int tn)
    { error(etabsel_unimp); }

void _pa_alert_ovr(ami_alert_t nfp, ami_alert_t* ofp)
    { *ofp = alert_vect; alert_vect = nfp; }
void ami_alert(char* title, char* message) { (*alert_vect)(title, message); }

static void alert_ivf(char* title, char* message) { error(ealert_unimp); }

void _pa_querycolor_ovr(ami_querycolor_t nfp, ami_querycolor_t* ofp)
    { *ofp = querycolor_vect; querycolor_vect = nfp; }
void ami_querycolor(int* r, int* g, int* b) { (*querycolor_vect)(r, g, b); }

static void querycolor_ivf(int* r, int* g, int* b) { error(equerycolor_unimp); }

void _pa_queryopen_ovr(ami_queryopen_t nfp, ami_queryopen_t* ofp)
    { *ofp = queryopen_vect; queryopen_vect = nfp; }
void ami_queryopen(char* s, int sl) { (*queryopen_vect)(s, sl); }

static void queryopen_ivf(char* s, int sl) { error(equeryopen_unimp); }

void _pa_querysave_ovr(ami_querysave_t nfp, ami_querysave_t* ofp)
    { *ofp = querysave_vect; querysave_vect = nfp; }
void ami_querysave(char* s, int sl) { (*querysave_vect)(s, sl); }

static void querysave_ivf(char* s, int sl) { error(equerysave_unimp); }

void _pa_queryfind_ovr(ami_queryfind_t nfp, ami_queryfind_t* ofp)
    { *ofp = queryfind_vect; queryfind_vect = nfp; }
void ami_queryfind(char* s, int sl, ami_qfnopts* opt)
    { (*queryfind_vect)(s, sl, opt); }

static void queryfind_ivf(char* s, int sl, ami_qfnopts* opt)
    { error(equeryfind_unimp); }

void _pa_queryfindrep_ovr(ami_queryfindrep_t nfp, ami_queryfindrep_t* ofp)
    { *ofp = queryfindrep_vect; queryfindrep_vect = nfp; }
void ami_queryfindrep(char* s, int sl, char* r, int rl, ami_qfropts* opt)
    { (*queryfindrep_vect)(s, sl, r, rl, opt); }

static void queryfindrep_ivf(char* s, int sl, char* r, int rl, ami_qfropts* opt)
    { error(equeryfindrep_unimp); }

void _pa_queryfont_ovr(ami_queryfont_t nfp, ami_queryfont_t* ofp)
    { *ofp = queryfont_vect; queryfont_vect = nfp; }
void ami_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb,
                  int* br, int* bg, int* bb, ami_qfteffects* effect)
    { (*queryfont_vect)(f, fc, s, fr, fg, fb, br, bg, bb, effect); }

static void queryfont_ivf(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb,
                          int* br, int* bg, int* bb, ami_qfteffects* effect)
    { error(equeryfont_unimp); }

/** ****************************************************************************

Gralib startup

*******************************************************************************/

static void ami_init_graphics (int argc, char *argv[]) __attribute__((constructor (102)));
static void ami_init_graphics(int argc, char *argv[])

{

    int       ofn;  /* standard output file number */
    int       ifn;  /* standard input file number */
    winptr    win;  /* windows record pointer */
    int       dfid; /* XWindow display FID */
    int       f;    /* window creation flags */
    ami_valptr config_root; /* root for config block */
    ami_valptr term_root; /* root for terminal block */

    ami_valptr graph_root; /* root for graphics block */
    ami_valptr diag_root; /* root for diagnostics block */
    ami_valptr xwin_root; /* root for xwindow */
    ami_valptr vp;
    char*     errstr;
    int       fi;
    ami_evtcod e;
    int       ji;
    char      joyfil[] = "/dev/input/js0";
    int       joyfid;
    char      jc;

    /* set override vectors to defaults */
    cursor_vect =          cursor_ivf;
    maxx_vect =            maxx_ivf;
    maxy_vect =            maxy_ivf;
    home_vect =            home_ivf;
    del_vect =             del_ivf;
    up_vect =              up_ivf;
    down_vect =            down_ivf;
    left_vect =            left_ivf;
    right_vect =           right_ivf;
    blink_vect =           blink_ivf;
    reverse_vect =         reverse_ivf;
    underline_vect =       underline_ivf;
    superscript_vect =     superscript_ivf;
    subscript_vect =       subscript_ivf;
    italic_vect =          italic_ivf;
    bold_vect =            bold_ivf;
    strikeout_vect =       strikeout_ivf;
    standout_vect =        standout_ivf;
    fcolor_vect =          fcolor_ivf;
    bcolor_vect =          bcolor_ivf;
    auto_vect =            auto_ivf;
    curvis_vect =          curvis_ivf;
    scroll_vect =          scroll_ivf;
    curx_vect =            curx_ivf;
    cury_vect =            cury_ivf;
    curbnd_vect =          curbnd_ivf;
    select_vect =          select_ivf;
    event_vect =           event_ivf;
    timer_vect =           timer_ivf;
    killtimer_vect =       killtimer_ivf;
    mouse_vect =           mouse_ivf;
    mousebutton_vect =     mousebutton_ivf;
    joystick_vect =        joystick_ivf;
    joybutton_vect =       joybutton_ivf;
    joyaxis_vect =         joyaxis_ivf;
    settab_vect =          settab_ivf;
    restab_vect =          restab_ivf;
    clrtab_vect =          clrtab_ivf;
    funkey_vect =          funkey_ivf;
    frametimer_vect =      frametimer_ivf;
    autohold_vect =        autohold_ivf;
    wrtstr_vect =          wrtstr_ivf;
    wrtstrn_vect =         wrtstrn_ivf;
    eventover_vect =       eventover_ivf;
    eventsover_vect =      eventsover_ivf;
    sendevent_vect =       sendevent_ivf;
    maxxg_vect =           maxxg_ivf;
    maxyg_vect =           maxyg_ivf;
    curxg_vect =           curxg_ivf;
    curyg_vect =           curyg_ivf;
    line_vect =            line_ivf;
    linewidth_vect =       linewidth_ivf;
    rect_vect =            rect_ivf;
    frect_vect =           frect_ivf;
    rrect_vect =           rrect_ivf;
    frrect_vect =          frrect_ivf;
    ellipse_vect =         ellipse_ivf;
    fellipse_vect =        fellipse_ivf;
    arc_vect =             arc_ivf;
    farc_vect =            farc_ivf;
    fchord_vect =          fchord_ivf;
    ftriangle_vect =       ftriangle_ivf;
    cursorg_vect =         cursorg_ivf;
    baseline_vect =        baseline_ivf;
    setpixel_vect =        setpixel_ivf;
    fover_vect =           fover_ivf;
    bover_vect =           bover_ivf;
    finvis_vect =          finvis_ivf;
    binvis_vect =          binvis_ivf;
    fxor_vect =            fxor_ivf;
    bxor_vect =            bxor_ivf;
    fand_vect =            fand_ivf;
    band_vect =            band_ivf;
    for_vect =             for_ivf;
    bor_vect =             bor_ivf;
    chrsizx_vect =         chrsizx_ivf;
    chrsizy_vect =         chrsizy_ivf;
    fonts_vect =           fonts_ivf;
    font_vect =            font_ivf;
    fontnam_vect =         fontnam_ivf;
    fontsiz_vect =         fontsiz_ivf;
    setpoints_vect =        setpoints_ivf;
    points_vect =        points_ivf;
    chrspcy_vect =         chrspcy_ivf;
    chrspcx_vect =         chrspcx_ivf;
    dpmx_vect =            dpmx_ivf;
    dpmy_vect =            dpmy_ivf;
    strsiz_vect =          strsiz_ivf;
    chrpos_vect =          chrpos_ivf;
    writejust_vect =       writejust_ivf;
    justpos_vect =         justpos_ivf;
    condensed_vect =       condensed_ivf;
    extended_vect =        extended_ivf;
    xlight_vect =          xlight_ivf;
    light_vect =           light_ivf;
    xbold_vect =           xbold_ivf;
    hollow_vect =          hollow_ivf;
    raised_vect =          raised_ivf;
    settabg_vect =         settabg_ivf;
    restabg_vect =         restabg_ivf;
    fcolorg_vect =         fcolorg_ivf;
    fcolorc_vect =         fcolorc_ivf;
    bcolorg_vect =         bcolorg_ivf;
    bcolorc_vect =         bcolorc_ivf;
    loadpict_vect =        loadpict_ivf;
    pictsizx_vect =        pictsizx_ivf;
    pictsizy_vect =        pictsizy_ivf;
    picture_vect =         picture_ivf;
    delpict_vect =         delpict_ivf;
    viewoffg_vect =        viewoffg_ivf;
    viewscale_vect =       viewscale_ivf;
    scalex_vect =          scalex_ivf;
    scaley_vect =          scaley_ivf;
    scrollg_vect =         scrollg_ivf;
    path_vect =            path_ivf;
    title_vect =           title_ivf;
    openwin_vect =         openwin_ivf;
    buffer_vect =          buffer_ivf;
    sizbuf_vect =          sizbuf_ivf;
    sizbufg_vect =         sizbufg_ivf;
    getsiz_vect =          getsiz_ivf;
    getsizg_vect =         getsizg_ivf;
    setsiz_vect =          setsiz_ivf;
    setsizg_vect =         setsizg_ivf;
    setpos_vect =          setpos_ivf;
    setposg_vect =         setposg_ivf;
    scnsiz_vect =          scnsiz_ivf;
    scnsizg_vect =         scnsizg_ivf;
    scncen_vect =          scncen_ivf;
    scnceng_vect =         scnceng_ivf;
    winclient_vect =       winclient_ivf;
    winclientg_vect =      winclientg_ivf;
    front_vect =           front_ivf;
    back_vect =            back_ivf;
    frame_vect =           frame_ivf;
    sizable_vect =         sizable_ivf;
    sysbar_vect =          sysbar_ivf;
    menu_vect =            menu_ivf;
    menuena_vect =         menuena_ivf;
    menusel_vect =         menusel_ivf;
    stdmenu_vect =         stdmenu_ivf;
    getwinid_vect =        getwinid_ivf;
    focus_vect =           focus_ivf;
    getwigid_vect =        getwigid_ivf;
    killwidget_vect =      killwidget_ivf;
    selectwidget_vect =    selectwidget_ivf;
    enablewidget_vect =    enablewidget_ivf;
    getwidgettext_vect =   getwidgettext_ivf;
    putwidgettext_vect =   putwidgettext_ivf;
    sizwidget_vect =       sizwidget_ivf;
    sizwidgetg_vect =      sizwidgetg_ivf;
    poswidget_vect =       poswidget_ivf;
    poswidgetg_vect =      poswidgetg_ivf;
    backwidget_vect =      backwidget_ivf;
    frontwidget_vect =     frontwidget_ivf;
    focuswidget_vect =     focuswidget_ivf;
    buttonsiz_vect =       buttonsiz_ivf;
    buttonsizg_vect =      buttonsizg_ivf;
    button_vect =          button_ivf;
    buttong_vect =         buttong_ivf;
    checkboxsiz_vect =     checkboxsiz_ivf;
    checkboxsizg_vect =    checkboxsizg_ivf;
    checkbox_vect =        checkbox_ivf;
    checkboxg_vect =       checkboxg_ivf;
    radiobuttonsiz_vect =  radiobuttonsiz_ivf;
    radiobuttonsizg_vect = radiobuttonsizg_ivf;
    radiobutton_vect =     radiobutton_ivf;
    radiobuttong_vect =    radiobuttong_ivf;
    groupsizg_vect =       groupsizg_ivf;
    groupsiz_vect =        groupsiz_ivf;
    group_vect =           group_ivf;
    groupg_vect =          groupg_ivf;
    background_vect =      background_ivf;
    backgroundg_vect =     backgroundg_ivf;
    scrollvertsizg_vect =  scrollvertsizg_ivf;
    scrollvertsiz_vect =   scrollvertsiz_ivf;
    scrollvert_vect =      scrollvert_ivf;
    scrollvertg_vect =     scrollvertg_ivf;
    scrollhorizsizg_vect = scrollhorizsizg_ivf;
    scrollhorizsiz_vect =  scrollhorizsiz_ivf;
    scrollhoriz_vect =     scrollhoriz_ivf;
    scrollhorizg_vect =    scrollhorizg_ivf;
    scrollpos_vect =       scrollpos_ivf;
    scrollsiz_vect =       scrollsiz_ivf;
    numselboxsizg_vect =   numselboxsizg_ivf;
    numselboxsiz_vect =    numselboxsiz_ivf;
    numselbox_vect =       numselbox_ivf;
    numselboxg_vect =      numselboxg_ivf;
    editboxsizg_vect =     editboxsizg_ivf;
    editboxsiz_vect =      editboxsiz_ivf;
    editbox_vect =         editbox_ivf;
    editboxg_vect =        editboxg_ivf;
    progbarsizg_vect =     progbarsizg_ivf;
    progbarsiz_vect =      progbarsiz_ivf;
    progbar_vect =         progbar_ivf;
    progbarg_vect =        progbarg_ivf;
    progbarpos_vect =      progbarpos_ivf;
    listboxsizg_vect =     listboxsizg_ivf;
    listboxsiz_vect =      listboxsiz_ivf;
    listbox_vect =         listbox_ivf;
    listboxg_vect =        listboxg_ivf;
    dropboxsizg_vect =     dropboxsizg_ivf;
    dropboxsiz_vect =      dropboxsiz_ivf;
    dropbox_vect =         dropbox_ivf;
    dropboxg_vect =        dropboxg_ivf;
    dropeditboxsizg_vect = dropeditboxsizg_ivf;
    dropeditboxsiz_vect =  dropeditboxsiz_ivf;
    dropeditbox_vect =     dropeditbox_ivf;
    dropeditboxg_vect =    dropeditboxg_ivf;
    slidehorizsizg_vect =  slidehorizsizg_ivf;
    slidehorizsiz_vect =   slidehorizsiz_ivf;
    slidehoriz_vect =      slidehoriz_ivf;
    slidehorizg_vect =     slidehorizg_ivf;
    slidevertsizg_vect =   slidevertsizg_ivf;
    slidevertsiz_vect =    slidevertsiz_ivf;
    slidevert_vect =       slidevert_ivf;
    slidevertg_vect =      slidevertg_ivf;
    tabbarsizg_vect =      tabbarsizg_ivf;
    tabbarsiz_vect =       tabbarsiz_ivf;
    tabbarclientg_vect =   tabbarclientg_ivf;
    tabbarclient_vect =    tabbarclient_ivf;
    tabbar_vect =          tabbar_ivf;
    tabbarg_vect =         tabbarg_ivf;
    tabsel_vect =          tabsel_ivf;
    alert_vect =           alert_ivf;
    querycolor_vect =      querycolor_ivf;
    queryopen_vect =       queryopen_ivf;
    querysave_vect =       querysave_ivf;
    querysave_vect =       querysave_ivf;
    querysave_vect =       querysave_ivf;
    querysave_vect =       querysave_ivf;
    queryfind_vect =       queryfind_ivf;
    queryfindrep_vect =    queryfindrep_ivf;
    queryfont_vect =       queryfont_ivf;

    /* clear malloc in use total */
    memusd = 0; /* total memory in use */
    memrty = 0; /* number of retries */
    maxrty = 0; /* retry maximum */
    fontcnt = 0; /* font entry counter */
    fonttot = 0; /* font entry total */
    filcnt = 0; /* file entry counter */
    filtot = 0; /* file entry total */
    piccnt = 0; /* picture entry counter */
    pictot = 0; /* picture entry total */
    scncnt = 0; /* screen struct counter */
    scntot = 0; /* screen struct total */
    wincnt = 0; /* windows structure counter */
    wintot = 0; /* windows structure total */
    imgcnt = 0; /* image frame counter */
    imgtot = 0; /* image frame total */
    metcnt = 0; /* menu entries counter */
    mettot = 0; /* menu entries total space */
    evtcnt = 0; /* clear PA event count */

    /* initialize the XWindow lock */
    pthread_mutex_init(&xwlock, NULL);

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
#ifdef NOCANCEL
    ovr_read_nocancel(iread_nocancel, &ofpread_nocancel);
    ovr_write_nocancel(iwrite_nocancel, &ofpwrite_nocancel);
    ovr_open_nocancel(iopen_nocancel, &ofpopen_nocancel);
    ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);
#endif

    /* set internal configurable settings */
    maxxd     = MAXXD;     /* set default window dimensions */
    maxyd     = MAXYD;
    dialogerr = DIALOGERR; /* send runtime errors to dialog */
    mouseenb  = MOUSEENB;  /* enable mouse */
    joyenb    = JOYENB;    /* enable joystick */
    dmpmsg    = DMPMSG;    /* dump XWindow messages */
    dmpevt    = DMPEVT;    /* dump Petit-Ami messages */
    prtftm    = PRTFTM;    /* print font metrics on load */
    conpnt    = CONPNT;    /* point size of console font */

    /* set state of shift, control and alt keys */
    ctrll = FALSE;
    ctrlr = FALSE;
    shiftl = FALSE;
    shiftr = FALSE;
    altl = FALSE;
    altr = FALSE;
    capslock = FALSE;

    esck = FALSE; /* set no previous escape */

    /* set "configuration" XWindow font capabilities */
    cfgcap = BIT(xcmedium) | BIT(xcbold) | BIT(xcdemibold) | BIT(xcdark) |
             BIT(xclight) | BIT(xcblack) | BIT(xcital) | BIT(xcoblique) |
             BIT(xcrital) | BIT(xcroblique) | BIT(xcnarrow) | BIT(xccondensed) |
             BIT(xcsemicondensed) | BIT(xcexpanded);

    /* set internal states */
    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */

    fntlst = NULL; /* clear font list */
    fntcnt = 0;
    frepic = NULL; /* clear free pictures list */
    freque = NULL; /* clear free x input queues */
    evtque = NULL; /* clear x event input queue */
    paqfre = NULL; /* clear pa event free queue */
    paqevt = NULL; /* clear pa event input queue */
    fremet = NULL; /* clear free menu tracking entries */
    winfre = NULL; /* clear free windows structure list */

    stdchrx = stdchrx; /* set default for standard/reference character size x */
    stdchry = stdchry; /* set default for standard/reference character size y */

    errflg = FALSE; /* set no error occurred */

    /* clear open files tables */
    for (fi = 0; fi < MAXFIL; fi++) {

        opnfil[fi] = NULL; /* set unoccupied */
        /* clear file to window logical number translator table */
        filwin[fi] = -1; /* set unoccupied */

    }

    /* clear window equivalence table */
    for (fi = 0; fi < MAXFIL*2+1; fi++) {

        /* clear window logical number translator table */
        xltwin[fi] = -1; /* set unoccupied */
        xltmnu[fi] = NULL; /* set no menu entry */

    }

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_ettabbar; e++) evthan[e] = defaultevent;

    /* get setup configuration */
    config_root = NULL;
    ami_config(&config_root);

    /* find "terminal" block */
    term_root = ami_schlst("terminal", config_root);
    if (term_root && term_root->sublist) term_root = term_root->sublist;

    /* find x an y max if they exist */
    vp = ami_schlst("maxxd", term_root);
    if (vp) {

        maxxd = strtol(vp->value, &errstr, 10);
        if (*errstr) error(ecfgval);

    }
    vp = ami_schlst("maxyd", term_root);
    if (vp) {

        maxyd = strtol(vp->value, &errstr, 10);
        if (*errstr) error(ecfgval);

    }
    vp = ami_schlst("joystick", term_root);
    if (vp) {

        joyenb = strtol(vp->value, &errstr, 10);
        if (*errstr) error(ecfgval);

    }
    vp = ami_schlst("mouse", term_root);
    if (vp) {

        mouseenb = strtol(vp->value, &errstr, 10);
        if (*errstr) error(ecfgval);

    }
    vp = ami_schlst("dump_event", term_root);
    if (vp) {

        dmpevt = strtol(vp->value, &errstr, 10);
        if (*errstr) error(ecfgval);

    }

    /* find graph block */
    graph_root = ami_schlst("graphics", config_root);
    if (graph_root) {

        vp = ami_schlst("console_points", graph_root->sublist);
        if (vp) {

            conpnt = strtol(vp->value, &errstr, 10);
            if (*errstr) error(ecfgval);

        }

        vp = ami_schlst("dialogerr", graph_root->sublist);
        if (vp) {

            dialogerr = strtol(vp->value, &errstr, 10);
            if (*errstr) error(ecfgval);

        }

        /* find windows subsection */
        xwin_root = ami_schlst("xwindow", graph_root->sublist);
        if (xwin_root) {

            /* find diagnostic subsection */
            diag_root = ami_schlst("diagnostics", xwin_root->sublist);
            if (diag_root) {

                vp = ami_schlst("dump_messages", diag_root->sublist);
                if (vp) {

                    dmpmsg = strtol(vp->value, &errstr, 10);
                    if (*errstr) error(ecfgval);

                }

                vp = ami_schlst("print_font_metrics", diag_root->sublist);
                if (vp) {

                    prtftm = strtol(vp->value, &errstr, 10);
                    if (*errstr) error(ecfgval);

                }

            }

        }

    }

    /* capture the XWindow errors */
    xerrbyp = FALSE; /* set no xerror() bypass */
    XSetErrorHandler(xerror); /* establish handler */

    /* find existing display */
    XWLOCK();
    padisplay = XOpenDisplay(NULL);
    XWUNLOCK();
    if (padisplay == NULL) {

        fprintf(stderr, "Cannot open display\n");
        exit(1);

    }
    XWLOCK();
    pascreen = DefaultScreen(padisplay);
    XWUNLOCK();

    /* initialize FreeType and fontconfig */
    if (FT_Init_FreeType(&ftlibrary)) {

        fprintf(stderr, "Cannot initialize FreeType\n");
        exit(1);

    }
    FcInit();

    /* load the font set */
    getfonts();

    /* find frame characteristics */
    fndfrm();

    /* open stdin and stdout as I/O window set */
    ifn = fileno(stdin); /* get logical id stdin */
    ofn = fileno(stdout); /* get logical id stdout */
    openio(stdin, stdout, ifn, ofn, -1, 1, FALSE); /* process open */

    /* select XWindow display file */
    XWLOCK();
    dfid = ConnectionNumber(padisplay);
    XWUNLOCK();
    dspsev = system_event_addseinp(dfid);

    /* clear joystick table */
    for (ji = 0; ji < MAXJOY; ji++) joytab[ji] = NULL;

    strcpy(joyfil, "/dev/input/js0"); /* set name of joystick file */

    /* open joystick if available */
    numjoy = 0; /* set no joysticks */
    if (joyenb) { /* if joystick is to be enabled */

        do { /* find joysticks */

            joyfil[13] = numjoy+'0'; /* set number of joystick to find */
            joyfid = open(joyfil, O_RDONLY);
            if (joyfid >= 0) { /* found */

                /* get a joystick table entry */
                joytab[numjoy] = imalloc(sizeof(joyrec));
                joytab[numjoy]->fid = joyfid; /* set fid */
                /* enable system event */
                joytab[numjoy]->sid = system_event_addseinp(joyfid);
                    /* get system event entry */
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
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X */
                /* get number of axes */
                ioctl(joyfid, JSIOCGAXES, &jc);
                joytab[numjoy]->axis = jc;
                /* get number of buttons */
                ioctl(joyfid, JSIOCGBUTTONS, &jc);
                joytab[numjoy]->button = jc;
#endif
                numjoy++; /* count joysticks */

            }

        } while (numjoy < MAXJOY && joyfid >= 0); /* no more joysticks */

    }

    /* override the event handler for menus */
    ami_eventsover(menu_event, &menu_event_oeh);

    /* get the codes for windows state atoms */
    cfocused = XInternAtom(padisplay, "_NET_WM_STATE_FOCUSED", 1);
    cmaxhorz = XInternAtom(padisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", 1);
    cmaxvert = XInternAtom(padisplay, "_NET_WM_STATE_MAXIMIZED_VERT", 1);
    chidden = XInternAtom(padisplay, "_NET_WM_STATE_HIDDEN", 1);

}

/** ****************************************************************************

Gralib shutdown

*******************************************************************************/

static void ami_deinit_graphics (void) __attribute__((destructor (102)));
static void ami_deinit_graphics()

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

    winptr win;    /* windows record pointer */
    string trmnam; /* termination name */
    char   fini[] = "Finished - ";
    int    ji;
    int    fn;

    ami_evtrec er;
    ami_evtcod e; /* event index */

    /* Reset all event vectors back to the default handler. The client program
       may have installed overrides (e.g. longjmp-based terminate handlers)
       that are no longer safe to call now that main() has returned — the
       stack frame they longjmp into is gone. */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_ettabbar; e++) evthan[e] = defaultevent;

    /* try to get window from stdout */
    win = NULL; /* set no window */
    fn = fileno(stdout); /* get fid */
    if (fn >= 0 && fn < MAXFIL && opnfil[fn]) win = opnfil[fn]->win;
    if (win && !errflg) { /* if stdout is attached to a window and no error
                             exit */

        /* if the program tries to exit when the user has not ordered an exit,
           it is assumed to be a windows "unaware" program. We stop before we
           exit these, so that their content may be viewed */
        if (!fend && fautohold) {

            /* process automatic exit sequence */
            if (!win->visible) winvis(win); /* make sure we are displayed */
            /* construct final name for window */
#if defined(__MACH__) || defined(__FreeBSD__)
            trmnam = imalloc(strlen(fini)+strlen(getprogname())+1);
            strcpy(trmnam, fini);
            strcat(trmnam, getprogname());
#else
            trmnam = imalloc(strlen(fini)+
                             strlen(program_invocation_short_name)+1);
            strcpy(trmnam, fini); /* place first part */
            strcat(trmnam, program_invocation_short_name);
#endif
            /* set window title */
            XStoreName(padisplay, win->xmwhan, trmnam);
            /* wait for a formal end */
            while (!fend) ami_event(stdin, &er);
            ifree(trmnam); /* free up termination name */

        }
        XWLOCK();
        /* destroy the main window */
        XDestroyWindow(padisplay, win->xwhan);
        /* close X Window */
        XCloseDisplay(padisplay);
        XWUNLOCK();

    }

    /* shutdown FreeType and fontconfig.
       Note: we clear the glyph cache (freeing X pixmaps) but do NOT call
       FT_Done_FreeType(). Individual FT_Done_Face() calls have already
       been made during window close, and FT_Done_FreeType() would try to
       free them again (double-free → segfault). The process is exiting,
       so the OS reclaims all heap memory regardless. */
    ft_cache_clear();
    /* FT_Done_FreeType(ftlibrary); — skipped, see above */
    FcFini();

    /* close joysticks */
    for (ji = 0; ji < MAXJOY; ji++) if (joytab[ji]) close(joytab[ji]->fid);

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_lseek(ofplseek, &cpplseek);
#ifdef NOCANCEL
    ovr_read_nocancel(ofpread_nocancel, &cppread_nocancel);
    ovr_write_nocancel(ofpwrite_nocancel, &cppwrite_nocancel);
    ovr_open_nocancel(ofpopen_nocancel, &cppopen_nocancel);
    ovr_close_nocancel(ofpclose_nocancel, &cppclose_nocancel);
#endif

    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cpplseek != ilseek)
        error(esystem);

    /* release the XWindow call lock */
    pthread_mutex_destroy(&xwlock);

#ifdef PRTMEM
    fprintf(stderr, "Total memory used: %lu Total retries on malloc(): %lu\n",
            memusd, memrty);
    fprintf(stderr, "Maximum retry: %lu\n", maxrty);
    fprintf(stderr, "Font entry counter:    %lu\n", fontcnt);
    fprintf(stderr, "Font entry total:      %lu\n", fonttot);
    fprintf(stderr, "File entry counter:    %lu\n", filcnt);
    fprintf(stderr, "File entry total:      %lu\n", filtot);
    fprintf(stderr, "Picture entry counter: %lu\n", piccnt);
    fprintf(stderr, "Picture entry total:   %lu\n", pictot);
    fprintf(stderr, "Screen entry counter:  %lu\n", scncnt);
    fprintf(stderr, "Screen entry total:    %lu\n", scntot);
    fprintf(stderr, "Window entry counter:  %lu\n", wincnt);
    fprintf(stderr, "Window entry total:    %lu\n", wintot);
    fprintf(stderr, "Image frame counter:   %lu\n", imgcnt);
    fprintf(stderr, "Image frame total:     %lu\n", imgtot);

#endif

}
