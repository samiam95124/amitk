/** ****************************************************************************

\file

\brief CHARACTER MODE WINDOW MANAGER

Copyright (C) 2022 Scott A. Franco

This is a character mode window submanager for Petit-Ami. It takes a character
based surface as provided by terminal or compatible, and subdivides it into
windows.

It is portable, meaning that it only relies on the terminal level API as
defined in terminal.h. It works by overriding the base calls and giving a window
view to the client program.

The use case of windowc is to subdivide a surface like an xterm that
normally cannot present subwindows. Thus it can provide windowing to terminal
packages such as xterm or a DOS window under Windows.

There are a few important differences between this window manager and a built-in
window manager such as XWindows or Windows.

1. Since the root window is the original terminal surface, it cannot create
independent windows on the desktop, only within the parent terminal window. This
has effects on the following points.

2. When windows are maximized, no system bar or frame edges are presented. That
is, a maximized windows is the same as the original window of the terminal
surface.

3. The default I/O surface is created maximized.

4. By default, only standard ASCII characters are used to depict frame
components.

The reason for these rules is that windowc is "transparent" by default. A
non-manager aware program will run full screen, and behave as if it has the
the terminal window to itself. windowc will be entirely in the background.

                          BSD LICENSE INFORMATION

Copyright (C) 2019 - Scott A. Franco

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the project nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>

/* linux definitions */
#include <limits.h>

/* local definitions */
#include <localdefs.h>
#include <config.h>
#include <terminalw.h>

#include <diag.h>

/* external definitions */
#if defined(_WIN32)

/* Windows has no glibc program_invocation_short_name. _pgmptr carries the full
   path of the running program, so take the name off the end of it. */

static char* progshortname(void)

{

    char* p;
    char* s;

    s = _pgmptr; /* get program path */
    if (!s) return ("");
    /* step past any path in front of the name */
    for (p = s; *p; p++) if (*p == '\\' || *p == '/') s = p+1;

    return (s);

}

#define program_invocation_short_name progshortname()

#elif !defined(__MACH__) /* Mac OS X */
extern char *program_invocation_short_name;
#endif

/* select dialog/command line error */
#define USEDLG

#ifndef __MACH__ /* Mac OS X */
#define NOCANCEL /* include nocancel overrides */
#endif

#define MAXFIL 100   /* maximum open files */
#define MAXCON 10    /* number of screen contexts */
#define MAXTAB 250   /* total number of tabs possible per window */
#define MAXLIN 250   /* maximum length of input bufferred line */
#define USEUNICODE   /* use unicode frame characters */
//#define PRTROOTEVT /* print root window events */
//#define PRTEVT     /* print outbound events */
//#define PRTFMASK /* print the forward masks calculated */

/* Name of the null device. Window files are parked on it, as the window
   manager supplies the content itself. Windows spells it differently. */
#ifdef _WIN32
#define NULLDEV "nul"
#else
#define NULLDEV "/dev/null"
#endif

/* file handle numbers at the system interface level */
#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

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
    /* Greyed (disabled) text. Internal to the manager: there is no
       terminal attribute behind it; it renders by blending the foreground
       toward the background through the full color (fcolorc) call. */
    sagrey,

} scnatt;

/* rectangle */
typedef struct { ami_long x1, y1, x2, y2; } rectangle;

/* drag type */

typedef enum {

    dt_none,   /* no drag active */
    dt_sysbar, /* sysbar drag (whole window) */
    dt_ulcnr,  /* upper left corner */
    dt_urcnr,  /* upper right corner */
    dt_blcnr,  /* bottom left corner */
    dt_brcnr,  /* bottom right corner */
    dt_top,    /* top frame bar */
    dt_left,   /* left frame bar */
    dt_right,  /* right frame bar */
    dt_bottom  /* bottom frame bar */

} drgtyp;

/* PA queue structure. Its a bubble list. */
typedef struct paevtque {

    struct paevtque* next; /* next in list */
    struct paevtque* last; /* last in list */
    ami_evtrec       evt;  /* event data */

} paevtque;

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

/* saved vectors for API entry calls */
static _pa_cursor_t cursor_vect;
static _pa_maxx_t maxx_vect;
static _pa_maxy_t maxy_vect;
static _pa_home_t home_vect;
static _pa_del_t del_vect;
static _pa_up_t up_vect;
static _pa_down_t down_vect;
static _pa_left_t left_vect;
static _pa_right_t right_vect;
static _pa_blink_t blink_vect;
static _pa_reverse_t reverse_vect;
static _pa_underline_t underline_vect;
static _pa_superscript_t superscript_vect;
static _pa_subscript_t subscript_vect;
static _pa_italic_t italic_vect;
static _pa_bold_t bold_vect;
static _pa_strikeout_t strikeout_vect;
static _pa_standout_t standout_vect;
static _pa_fcolor_t fcolor_vect;
static _pa_bcolor_t bcolor_vect;
static _pa_auto_t auto_vect;
static _pa_curvis_t curvis_vect;
static _pa_scroll_t scroll_vect;
static _pa_curx_t curx_vect;
static _pa_cury_t cury_vect;
static _pa_curbnd_t curbnd_vect;
static _pa_select_t select_vect;
static _pa_event_t event_vect;
static _pa_timer_t timer_vect;
static _pa_killtimer_t killtimer_vect;
static _pa_mouse_t mouse_vect;
static _pa_mousebutton_t mousebutton_vect;
static _pa_joystick_t joystick_vect;
static _pa_joybutton_t joybutton_vect;
static _pa_joyaxis_t joyaxis_vect;
static _pa_settab_t settab_vect;
static _pa_restab_t restab_vect;
static _pa_clrtab_t clrtab_vect;
static _pa_funkey_t funkey_vect;
static _pa_frametimer_t frametimer_vect;
static _pa_autohold_t autohold_vect;
static _pa_wrtstr_t wrtstr_vect;
static _pa_wrtstrn_t wrtstrn_vect;
static _pa_sizbuf_t sizbuf_vect;
static _pa_title_t title_vect;
static _pa_titlen_t titlen_vect;
static _pa_fcolorc_t fcolorc_vect;
static _pa_bcolorc_t bcolorc_vect;
static _pa_eventover_t eventover_vect;
static _pa_eventsover_t eventsover_vect;
static _pa_sendevent_t sendevent_vect;
static _pa_openwin_t openwin_vect;
static _pa_buffer_t buffer_vect;
static _pa_getsiz_t getsiz_vect;
static _pa_setsiz_t setsiz_vect;
static _pa_setpos_t setpos_vect;
static _pa_scnsiz_t scnsiz_vect;
static _pa_scncen_t scncen_vect;
static _pa_winclient_t winclient_vect;
static _pa_front_t front_vect;
static _pa_back_t back_vect;
static _pa_frame_t frame_vect;
static _pa_sizable_t sizable_vect;
static _pa_sysbar_t sysbar_vect;
static _pa_menu_t menu_vect;
static _pa_menuena_t menuena_vect;
static _pa_menusel_t menusel_vect;
static _pa_stdmenu_t stdmenu_vect;
static _pa_focus_t focus_vect;
static _pa_getwinid_t getwinid_vect;

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
    ami_long   id;                 /* user id of item */
    ami_long   fx1, fy1, fx2, fy2; /* subclient position of window */
    int    prime;              /* is a prime (onscreen) entry */
    int    pressed;            /* in the pressed state */
    FILE*  wf;                 /* output file for the menu window */
    char*  title;              /* title text */
    FILE*  parent;             /* parent window */
    FILE*  evtfil;             /* file to post menu events to */
    ami_long   wid;                /* menu window id */

} metrec;

/** single character on screen container. note that not all the attributes
   that appear here can be changed */
typedef struct {

      /* character at location */        char ch;
      /* foreground color at location */ ami_color forec;
      /* background color at location */ ami_color backc;
      /* active attribute at location */ scnatt attr;

} scnrec, *scnptr;

/* macro to access screen elements by y,x */
#define SCNBUF(sc, x, y) (sc[(y-1)*win->maxx+(x-1)])

/* window description */
typedef struct winrec* winptr;
typedef struct wigrec* wigptr;

/* menu enable state, kept per window aside the caller's menu records */
typedef struct menena* menenaptr;
typedef struct menena {

    struct menena* next;
    ami_long id;   /* menu item id */
    int  ena;  /* enabled */

} menena;
typedef struct winrec {

    winptr   next;              /* next entry (for free list) */
    int      root;              /* window is the root */
    int      parlfn;            /* logical parent */
    winptr   parwin;            /* link to parent (or NULL for parentless) */
    ami_long wid;               /* this window logical id */
    winptr   childwin;          /* list of child windows */
    winptr   childlst;          /* list pointer if this is a child */
    winptr   winlst;            /* master list of all windows */
    winptr   rootlst;           /* master list of all roots */
    winptr   zmin2max;          /* Z order minimum to maximum list */
    winptr   zmax2min;          /* Z order maximum to minimum list */
    scnrec*  screens[MAXCON];   /* screen contexts array */
    ami_color sbcolor[MAXCON];  /* background each screen was cleared to */
    int      redrawpend;        /* a redraw announcement is queued */
    int      maxed;             /* window is maximized */
    int      mined;             /* window is minimized */
    ami_long normx, normy;      /* geometry restored to by normalize */
    ami_long normw, normh;
    int      curdsp;            /* index for current display screen */
    int      curupd;            /* index for current update screen */
    ami_long orgx;              /* window origin in root x */
    ami_long orgy;              /* window origin in root y */
    ami_long coffx;             /* client offset x */
    ami_long coffy;             /* client offset y */
    /* note: maxx/y tracks the buffer size in buffered mode, but tracks the
       client size with buffering off */
    ami_long maxx;              /* maximum x size */
    ami_long maxy;              /* maximum y size */
    ami_long bufx;              /* buffer size x characters */
    ami_long bufy;              /* buffer size y characters */
    ami_long cmaxx;             /* onscreen client size x */
    ami_long cmaxy;             /* onscreen client size x */
    ami_long pmaxx;             /* parent maximum x */
    ami_long pmaxy;             /* parent maximum y */
    ami_long mpx, mpy;          /* mouse current position */
    ami_long curx;              /* current cursor location x */
    ami_long cury;              /* current cursor location y */
    int      attr;              /* set of active attributes */
    ami_color fcolor;            /* foreground color */
    ami_color bcolor;            /* background color */
    int      curv;              /* cursor visible */
    int      autof;             /* current status of scroll and wrap */
    int      bufmod;            /* buffered screen mode */
    int      tab[MAXTAB];       /* tabbing array */
    metptr   metlst;            /* menu tracking list */
    metptr   menu;              /* "faux menu" bar */
    int      frame;             /* frame on/off */
    int      size;              /* size bars on/off */
    int      sysbar;            /* system bar on/off */
    int      fixed;             /* size bars draw but sizing is ignored */
    char     inpbuf[MAXLIN];    /* input line buffer */
    int      inpptr;            /* input line index */
    int      visible;           /* window is visible */
    char*    title;             /* window title */
    int      focus;             /* window has focus */
    int      hover;             /* window being hovered */
    int      zorder;            /* Z ordering of window, 0 = bottom, N = top */
    unsigned char* fmask;       /* forward mask in bits per character */
    ami_long fmasklen;          /* length of the bitmask */
    int      widget;            /* window is a widget face */
    wigptr   wig;               /* widget data if so */
    wigptr   wiglst;            /* list of widgets owned by this window */
    ami_menuptr amenu;          /* API menu attached to this window */
    wigptr   mbar;              /* the menu bar widget if a menu is on */
    menenaptr menena;           /* menu item enable states */
    int      timers[AMI_MAXTIM]; /* timer id array */
    int      frmtim;            /* frame timer */
    ami_color frmcolor;          /* frame color */

} winrec;

/* widget types */
typedef enum {

    wtbutton, wtcheckbox, wtradio, wtgroup, wtbackground,
    wtscrollvert, wtscrollhoriz, wtnumselbox, wteditbox, wtprogbar,
    wtlistbox, wtslidehoriz, wtslidevert, wtdropbox, wtdropeditbox,
    wttabbar, wtmenubar, wtpopup

} wigtyp;

/* widget tracking record. A widget is a small frameless subwindow of its
   owning window, drawn in character cells, whose events are intercepted
   and translated to widget events for the owner. */
typedef struct wigrec {

    wigptr next;    /* next widget in the owner's list */
    ami_long   id;      /* widget logical id, unique per owner */
    wigtyp typ;     /* type of widget */
    winptr parent;  /* owning window */
    FILE*  wf;      /* widget subwindow file */
    winptr win;     /* widget subwindow record */
    char*  face;    /* label, or edit box content */
    char** list;    /* list box strings */
    ami_long   listn;   /* number of list strings */
    ami_long   top;     /* list box scroll offset, 0 based */
    ami_color* lcol; /* per entry face colors, or NULL: entries showing
                        their own color (internal, the dialogs' use) */
    ami_long   fatt;    /* extra face attributes (internal, the dialogs' use) */
    int    enb;     /* enabled */
    int    sel;     /* selected state */
    ami_long   val;     /* value: numsel, progress, scroll and slider position */
    ami_long   low, high; /* numsel range */
    ami_long   sclsiz;  /* scroll bar thumb size, full scale */
    ami_long   marks;   /* slider tick mark count */
    ami_long   curs;    /* edit box cursor index, 0 based */
    ami_tabori tor; /* tab bar orientation */
    wigptr owner;   /* for popups, the widget or bar that opened it */
    ami_menuptr mitems; /* for menu popups, the item list shown */

} wigrec;

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec* filptr;
typedef struct filrec {

    FILE*  sfp; /* file pointer used to establish entry, or NULL */
    winptr win; /* associated window (if exists) */
    int    inl; /* this output file is linked to the input file, logical */
    int    inw; /* entry is input linked to window */

} filrec;

/* Characters for window "dressing" (frame components) */
typedef enum {

    horzlin,   /* horizontal line */
    vertlin,   /* vertical line */
    sysudl,    /* system bar underline */
    toplftcnr, /* top left corner*/
    toprgtcnr, /* top right corner */
    btmlftcnr, /* bottom left corner */
    btmrgtcnr, /* bottom right corner */
    intlft,    /* left intersection */
    intrgt,    /* right intersection */
    minbtn,    /* minimize button */
    maxbtn,    /* maximize button */
    canbtn    /* cancel button */

} framecomp;

#ifdef USEUNICODE
/*
 * Unicode characters MUST be fixed pitch and match the other fixed pitch
 * characters of the current character set in width. Xterm takes Unicode
 * characters that do NOT meet these restructions and thus it malfunctions.
 * Its up to the user to carefully choose these characters.
 */
char* frmchrs[] = {

    "═", /* horizontal line */
    "║", /* vertical line */
    "═", /* system bar underline */
    "╔", /* top left corner*/
    "╗", /* top right corner */
    "╚", /* bottom left corner */
    "╝", /* bottom right corner */
    "╠", /* left intersection */
    "╣", /* right intersection */
    "-", /* minimize button */
    "▯", /* maximize button */
    "X", /* cancel button */

};
#else
char* frmchrs[] = {

    "-", /* horizontal line */
    "|", /* vertical line */
    "=", /* system bar underline */
    "+", /* top left corner*/
    "+", /* top right corner */
    "+", /* bottom left corner */
    "+", /* bottom right corner */
    "|", /* left intersection */
    "|", /* right intersection */
    "_", /* minimize button */
    "^", /* maximize button */
    "X", /* cancel button */

};
#endif

static filptr opnfil[MAXFIL];     /* open files table */
static int    xltwin[MAXFIL*2+1]; /* window equivalence table, includes
                                     negatives and 0 */
static int    filwin[MAXFIL];     /* file to window equivalence table */

/* colors and attributes for root window */
static int      attr;         /* set of active attributes */
static ami_color fcolor;       /* foreground color */
static ami_color bcolor;       /* background color */
static ami_long curx;         /* cursor x */
static ami_long cury;         /* cursor y */
static int      curon;        /* current on/off visible state of cursor */
static winptr   winfre;       /* free windows structure list */
static winptr   winlst;       /* master list of all windows */
static winptr   rootlst;      /* master list of all roots */
static winptr   zmin2max;     /* Z order minimum to maximum list */
static winptr   zmax2min;     /* Z order maximum to minimum list */
static winptr   curfocus;     /* current focus window, or NULL */

/* Diagnostic log, enabled by PETIT_AMI_LOG in the environment: appends
   a trace of the manager operations and the events delivered to the
   program to the common diagnostic log /tmp/petit_ami.log, which other
   Petit-Ami modules may also append to. The log writes with the raw
   system calls, touching none of the interdicted, buffered paths the
   manager itself stands in -- a buffered log write from inside the
   machinery recursed into it. Off, it costs one check. */

static int mclogfd = -2; /* unopened */

static int mclog(void)

{

    if (mclogfd == -2)
        mclogfd = getenv("PETIT_AMI_LOG")?
            open("/tmp/petit_ami.log", O_WRONLY|O_CREAT|O_APPEND, 0644): -1;

    return (mclogfd >= 0);

}

static void mclogf(const char* fmt, ...)

{

    char    buf[128];
    va_list ap;
    int     n;

    if (!mclog()) return;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) { ssize_t r = write(mclogfd, buf, n); (void)r; }

}
static int      opnwig;       /* opening a widget face window */
static int      ztop;         /* current maximum/front Z order */
static ami_long mousex;       /* mouse tracking x */
static ami_long mousey;       /* mouse tracking y */
static winptr   timtbl[AMI_MAXTIM]; /* timer translation table */
static ami_long timids[AMI_MAXTIM]; /* timer logical ids */
static int      fautohold;    /* automatic hold on exit flag */
static int      fend;         /* end of program ordered flag */
static drgtyp   drag;         /* drag type in progress */

/* frame parts, for hover feedback */
typedef enum {

    fp_none,   /* not on a live frame part */
    fp_min,    /* minimize button */
    fp_max,    /* maximize button */
    fp_close,  /* close button */
    fp_top,    /* sizing edges */
    fp_bottom,
    fp_left,
    fp_right,
    fp_ulcnr,  /* sizing corners */
    fp_urcnr,
    fp_blcnr,
    fp_brcnr

} frmpart;

static winptr  hovfwin;  /* window holding the hover highlight, or NULL */
static frmpart hovfpart; /* the highlighted frame part */
static wigptr  hovwig;   /* widget holding the hover highlight, or NULL */
static ami_long    hovwprt;  /* the highlighted widget part, coded per type */
static int     frmhrev;  /* the frame draws reversed (hover highlight) */
static winptr  hovflt;   /* frame cells draw only where this window is top */
static winptr fndtop(ami_long x, ami_long y); /* forward */
static winptr   drgwin;       /* drag window */
static wigptr   drgwig;       /* drag widget (slider or scroll thumb) */
#define MAXPOP 8              /* maximum popup nesting (menu cascade) */
static wigptr   popstk[MAXPOP]; /* open popup stack, bottom first */
static int      popcnt;       /* number of open popups */
static ami_long drgx;         /* drag pin x */
static ami_long drgy;         /* drag pin y */
static ami_pevthan evthan[ami_etmenus+1]; /* array of event handler routines */
static ami_pevthan evtshan;        /* single master event handler routine */
static paevtque*  paqfre;         /* free PA event queue entries list */
static paevtque*  paqevt;         /* PA event input save queue */
static ami_long   dimx, dimy;     /* terminal/root dimensions */

/* forwards */
static void plcchr(FILE* f, char c);

/*******************************************************************************

Find absolute screen position of window origin

The window list is a tree: every window but the root has a parent, and a
window's origin is stored relative to its parent's client area. The
absolute screen position is the sum up the parent chain, so placing a
window in any parent, and moving a parent with its children, follow from
the structure rather than from per-case arithmetic. The root has no
parent; its origin is the screen origin.

*******************************************************************************/

static ami_long absx(winptr win)

{

    if (!win->parwin) return (win->orgx);

    return (absx(win->parwin)+win->parwin->coffx+win->orgx-1);

}

static ami_long absy(winptr win)

{

    if (!win->parwin) return (win->orgy);

    return (absy(win->parwin)+win->parwin->coffy+win->orgy-1);

}

/** ****************************************************************************

Process error

*******************************************************************************/

static void error(
    /** Error string */ char* es
)


{

    /* Leave the alternate screen before reporting. The message would
       otherwise be written onto the alternate screen, which the exit
       sequence then switches away from and discards, leaving the user
       looking at a blank screen or a single stray character of the
       message. stderr shares the terminal with stdout, so the sequence
       takes effect from here. */
    fprintf(stderr, "\033[0m\033[?1049l\r\n");
    fprintf(stderr, "Error: Managerc: %s\n", es);
    fflush(stderr);

    exit(1);

}

/** ***************************************************************************

Print event type

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

static void prtevtt(
    /** Error code */ ami_evtcod e
)

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
        case ami_etframe:   fprintf(stderr, "etframe  "); break;
        case ami_etmin:     fprintf(stderr, "etmin    "); break;
        case ami_etmax:     fprintf(stderr, "etmax    "); break;
        case ami_etnorm:    fprintf(stderr, "etnorm   "); break;
        case ami_etfocus:   fprintf(stderr, "etfocus  "); break;
        case ami_etnofocus: fprintf(stderr, "etnofocus"); break;
        case ami_ethover:   fprintf(stderr, "ethover  "); break;
        case ami_etnohover: fprintf(stderr, "etnohover"); break;
        case ami_etmenus:   fprintf(stderr, "etmenus  "); break;

        default: fprintf(stderr, "???");

    }

}

/** ***************************************************************************

Print Petit-Ami event diagnostic

Prints a decoded version of PA events on one line, including paraemters. Only
prints if the dump PA event flag is true. Does not terminate the line.

Note: does not output a debugging preamble. If that is required, print it
before calling this routine.

******************************************************************************/

static void prtevt(
    /** Event record */ ami_evtptr er
)

{

    fprintf(stderr, "PA Event: Window: %lld ", AMI_LONG_CAST(er->winid));
    prtevtt(er->etype);
    switch (er->etype) {

        case ami_etchar: fprintf(stderr, ": char: %c", er->echar); break;
        case ami_ettim: fprintf(stderr, ": timer: %lld", AMI_LONG_CAST(er->timnum)); break;
        case ami_etmoumov: fprintf(stderr, ": mouse: %lld x: %4lld y: %4lld",
                                  AMI_LONG_CAST(er->mmoun), AMI_LONG_CAST(er->moupx), AMI_LONG_CAST(er->moupy)); break;
        case ami_etmouba: fprintf(stderr, ": mouse: %lld button: %lld",
                                 AMI_LONG_CAST(er->amoun), AMI_LONG_CAST(er->amoubn)); break;
        case ami_etmoubd: fprintf(stderr, ": mouse: %lld button: %lld",
                                 AMI_LONG_CAST(er->dmoun), AMI_LONG_CAST(er->dmoubn)); break;
        case ami_etjoyba: fprintf(stderr, ": joystick: %lld button: %lld",
                                 AMI_LONG_CAST(er->ajoyn), AMI_LONG_CAST(er->ajoybn)); break;
        case ami_etjoybd: fprintf(stderr, ": joystick: %lld button: %lld",
                                 AMI_LONG_CAST(er->djoyn), AMI_LONG_CAST(er->djoybn)); break;
        case ami_etjoymov: fprintf(stderr, ": joystick: %lld x: %4lld y: %4lld z: %4lld "
                                  "a4: %4lld a5: %4lld a6: %4lld", AMI_LONG_CAST(er->mjoyn),
                                  AMI_LONG_CAST(er->joypx), AMI_LONG_CAST(er->joypy), AMI_LONG_CAST(er->joypz),
                                  AMI_LONG_CAST(er->joyp4), AMI_LONG_CAST(er->joyp5), AMI_LONG_CAST(er->joyp6)); break;
        case ami_etresize: fprintf(stderr, ": x: %lld y: %lld", AMI_LONG_CAST(er->rszx), AMI_LONG_CAST(er->rszy));
                          break;
        case ami_etfun: fprintf(stderr, ": key: %lld", AMI_LONG_CAST(er->fkey)); break;
        case ami_etmenus: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->menuid)); break;

        default: ;

    }

}

/** ****************************************************************************

Print screen buffer

Prints the screen buffer to the error output. A diagnostic.

*******************************************************************************/

void prtscnbuf(winptr win, int bufno)

{

    scnptr sc;
    ami_long y;
    ami_long x;

    fprintf(stderr, "Buffer for wid: %lld\n", AMI_LONG_CAST(win->wid));
    sc = win->screens[bufno-1]; /* index screen */
    for (y = 1; y <= win->maxy; y++) {

        fprintf(stderr, "%02lld: \"", AMI_LONG_CAST(y));
        for (x = 1; x <= win->maxx; x++)
            fputc(SCNBUF(sc, x, y).ch, stderr);
        fprintf(stderr, "\"\n"); fflush(stderr);

    }
    fprintf(stderr, "Complete\n");
    fflush(stderr);

}

/** ****************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

static void getfil(filptr* fp)

{

    *fp = malloc(sizeof(filrec)); /* get new file entry */
    (*fp)->sfp = NULL; /* set no file pointer */
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = -1; /* set no input file linked */

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

    } else p = malloc(sizeof(winrec));
    if (!p) error("Out of memory");
    /* The forward mask is examined before opnwin allocates it, when the Z
       order lists are remade with this window first entered into them, so
       it cannot be left as garbage from malloc or from the record's
       previous life. */
    p->fmask = NULL;
    p->fmasklen = 0;
    p->widget = FALSE; /* not a widget, and owns no widgets */
    p->wig = NULL;
    p->wiglst = NULL;
    p->amenu = NULL; /* no API menu */
    p->mbar = NULL;
    p->menena = NULL;

    return (p);

}

/** ****************************************************************************

Get freed/new window structure

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void frmenu(ami_menuptr m); /* forward: defined with the menu code */

static void putwin(winptr p)

{

    if (p->amenu) { frmenu(p->amenu); p->amenu = NULL; } /* release menu copy */
    p->next = winfre; /* push to list */
    winfre = p;

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

    if (fn < 0 || fn >= MAXFIL) error("Invalid handle");
    if (!opnfil[fn]) error("Invalid handle");
    if (!opnfil[fn]->win) error("Not a window file");

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
    if (fn < 0) error("Invalid file");

    return (lfn2win(fn)); /* get logical filenumber for file */

}


/*******************************************************************************

Index window from logical window

Finds the windows context record from the logical window number, with checking.

*******************************************************************************/

static winptr lwn2win(ami_long wid)

{

    int    ofn; /* output file handle */
    winptr win; /* window context pointer */

    if (wid < -MAXFIL || wid >= MAXFIL || !wid)  error("Invalid file handle");
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
    if (fn < 0) error("Invalid file");

    return (fn); /* return result */

}

/*******************************************************************************

Set rectangle from individual values

Sets up a rectangle structure according to the top left and bottom right x,y
coordinates.

*******************************************************************************/

/* set rectangle to values */
static void setrect(rectangle* r, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    r->x1 = x1;
    r->y1 = y1;
    r->x2 = x2;
    r->y2 = y2;

}

/*******************************************************************************

Check intersection of rectangles

Returns true if the given rectangles intersect, that is, overlap in some area.

*******************************************************************************/

static int intersect(rectangle* r1, rectangle* r2)

{

    return (r1->x2 >= r2->x1 && r1->x1 <= r2->x2 &&
            r1->y2 >= r2->y1 && r1->y1 <= r2->y2);

}

/*******************************************************************************

Check inclusion of rectangle

Returns true if the second rectangle is included in the first, that is, the
second rectangle is completely within the first.

*******************************************************************************/

static int included(rectangle* r1, rectangle* r2)

{

    return (r2->x1 >= r1->x1 && r2->x2 <= r1->x2 &&
            r2->y1 >= r1->y1 && r2->y2 <= r1->y2);

}

/*******************************************************************************

Find intersection of rectangles

Finds a rectangle that is the intersection of the two given rectangles. The
operation is meaningless if the rectangles do not intersect.

*******************************************************************************/

static void intersection(rectangle* ri, rectangle* r1, rectangle* r2)

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

/*******************************************************************************

Find rectangles zero

Checks if the rectangle provided is all zeros. Returns true if so.

*******************************************************************************/

int zerorect(rectangle* r)

{

    return (!(r->x1 | r->x2 | r->y1 | r->y2));

}

/*******************************************************************************

Subtract two rectangles

Subtracts the second rectangle from the first, and returns 1 to 4 rectangles
resulting from that subtraction. Any of the rectangles returned could be zero,
and all should be checked.

*******************************************************************************/

void subrect(rectangle* r1, rectangle* r2, rectangle* rst, rectangle* rsl,
             rectangle* rsr, rectangle* rsb)

{

    /* zero all outputs */
    rst->x1 = rst->x2 = rst->y1 = rst->y2 = 0;
    rsl->x1 = rsl->x2 = rsl->y1 = rsl->y2 = 0;
    rsr->x1 = rsr->x2 = rsr->y1 = rsr->y2 = 0;
    rsb->x1 = rsb->x2 = rsb->y1 = rsb->y2 = 0;

    /* top */
    if (r1->y1 < r2->y1 && r2->y1 <= r1->y2)
        { rst->x1 = r1->x1; rst->x2 = r1->x2;
          rst->y1 = r1->y1; rst->y2 = r2->y1-1; }

    /* left */
    if (r1->x1 < r2->x1 && r2->x1 <= r1->x2)
        { rsl->x1 = r1->x1; rsl->x2 = r2->x1-1;
          rsl->y1 = r1->y1; rsl->y2 = r1->y2; }

    /* right */
    if (r1->x1 <= r2->x2 && r2->x2 < r1->x2)
        { rsr->x1 = r2->x2+1; rsr->x2 = r1->x2;
          rsr->y1 = r1->y1; rsr->y2 = r1->y2; }

    /* bottom */
    if (r1->y1 <= r2->y2 && r2->y2 < r1->y2)
        { rsb->x1 = r1->x1; rsb->x2 = r1->x2;
          rsb->y1 = r2->y2+1; rsb->y2 = r1->y2; }

    if (included(r1, r2)) {

        /* adjust left and right for included rectangle */
        if (r2->y1 > r1->y1) {

            if (!zerorect(rsl)) rsl->y1 = r2->y1;
            if (!zerorect(rsr)) rsr->y1 = r2->y1;

        }
        if (r2->y2 < r1->y2) {

            if (!zerorect(rsl)) rsl->y2 = r2->y2;
            if (!zerorect(rsr)) rsr->y2 = r2->y2;

        }

    }

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

    } else p = (paevtque*)malloc(sizeof(paevtque));

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

    if (!paqevt) error("System fault"); /* should not be called empty */
    /* we push TO next (current) and take FROM last (final) */
    p = paqevt->last; /* index final entry */
    if (p->next == p) paqevt = NULL; /* only one entry, clear list */
    else { /* other entries */

        p->last->next = p->next; /* point last at current */
        p->next->last = p->last; /* point current at last */

    }
    memcpy(e, &p->evt, sizeof(ami_evtrec)); /* copy out to caller */
    putpaevt(p); /* release queue entry to free */
    mclogf("deliver type %d wid %lld\n",
                         (int)e->etype, AMI_LONG_CAST(e->winid));
    if (e->etype == ami_etredraw) { /* the announcement is now delivered */

        winptr wp = winlst;

        while (wp && wp->wid != e->winid) wp = wp->winlst;
        if (wp) wp->redrawpend = FALSE;

    }

}

/*******************************************************************************

Set cursor cached

Sets the root cursor if it has changed.

*******************************************************************************/

static void setcursor(ami_long x, ami_long y)

{

    if (x != curx || y != cury) {

        (*cursor_vect)(stdout, x, y); /* set new position */
        curx = x; /* set new location */
        cury = y;

    }

}

/*******************************************************************************

Set cursor on/off cached

Sets the root cursor visible if it has changed.

*******************************************************************************/

static void setcurvis(int e)

{

    if (e != curon) {

        (*curvis_vect)(stdout, e); /* set new visible state */
        curon = e; /* set cache */

    }

}

/* Full color cells: a cell color is either a primary code or an encoded
   24 bit rgb color, marked by the flag bit. Exact primaries stay primary
   codes, so the common paths and the run batching see the same values
   the primary calls set; everything else the full color calls set is
   carried exactly and emitted through the full color vector. */
#define RGBFLG (1L<<30)
#define RGBENC(r, g, b) (RGBFLG|(r) << 16|(g) << 8|(b))
#define ISRGB(c) (((c)&RGBFLG) != 0)
#define RGBR(c) ((c) >> 16&0xff)
#define RGBG(c) ((c) >> 8&0xff)
#define RGBB(c) ((c)&0xff)

/* full scale color component to 8 bits, and back */
static ami_long colc8(ami_long v)

{

    if (v <= 0) return (0);
    if (v >= LONG_MAX-LONG_MAX/255) return (255);

    return (v/(LONG_MAX/255));

}

static ami_long col8full(ami_long v)

{

    if (v <= 0) return (0);
    if (v >= 255) return (LONG_MAX);

    return (v*(LONG_MAX/255));

}

/*******************************************************************************

Set foreground color cached

Sets the root foreground color if it has changed.

*******************************************************************************/

static void setfcolor(ami_color c)

{

    if (c != fcolor) {

        if (ISRGB(c)) /* an encoded color emits through the full color call */
            (*fcolorc_vect)(stdout, col8full(RGBR(c)), col8full(RGBG(c)),
                                    col8full(RGBB(c)));
        else (*fcolor_vect)(stdout, c); /* set new color */
        fcolor = c; /* cache that */

    }

}

/*******************************************************************************

Set background color cached

Sets the root background color if it has changed.

*******************************************************************************/

static void setbcolor(ami_color c)

{

    if (c != bcolor) {

        if (ISRGB(c)) /* an encoded color emits through the full color call */
            (*bcolorc_vect)(stdout, col8full(RGBR(c)), col8full(RGBG(c)),
                                    col8full(RGBB(c)));
        else (*bcolor_vect)(stdout, c); /* set new color */
        bcolor = c; /* cache that */

    }

}

/*******************************************************************************

Set foreground color for content

Sets the foreground for content carrying the given attributes. Greyed
(disabled) content presents as grey lettering: the foreground is blended
halfway to the background and emitted through the full color call, which
is the existing color interface -- there is no terminal attribute behind
greying. The color cache is invalidated, since the terminal is then on a
color the cache cannot represent.

*******************************************************************************/

/* primary or encoded color code to full color components */
static void colnumrgb(ami_color c, ami_long* r, ami_long* g, ami_long* b)

{

    if (ISRGB(c)) {

        *r = col8full(RGBR(c));
        *g = col8full(RGBG(c));
        *b = col8full(RGBB(c));

        return;

    }
    *r = (c == ami_red || c == ami_yellow || c == ami_magenta ||
          c == ami_white)? LONG_MAX: 0;
    *g = (c == ami_green || c == ami_yellow || c == ami_cyan ||
          c == ami_white)? LONG_MAX: 0;
    *b = (c == ami_blue || c == ami_cyan || c == ami_magenta ||
          c == ami_white)? LONG_MAX: 0;

}

static ami_color greyfc = -1; /* the blend pair the terminal is on */
static ami_color greybc = -1;

static void setfcolora(ami_color fc, ami_color bc, int at)

{

    ami_long fr, fg, fb, br, bg, bb;

    if (BIT(sagrey) & at) {

        /* already on this blend: consecutive greyed characters cost one
           emission, like the primary color cache gives normal text */
        if (fcolor == -1 && greyfc == fc && greybc == bc) return;
        colnumrgb(fc, &fr, &fg, &fb);
        colnumrgb(bc, &br, &bg, &bb);
        /* halfway blend; the halves are taken first to avoid overflow */
        (*fcolorc_vect)(stdout, fr/2+br/2, fg/2+bg/2, fb/2+bb/2);
        fcolor = -1; /* no primary matches the terminal now */
        greyfc = fc;
        greybc = bc;

    } else setfcolor(fc);

}

/******************************************************************************

Translate rgb to primary color code

Translates an rgb color to a primary color code. It does this by finding the
nearest primary color to the given RGB color.

******************************************************************************/

static ami_color colrgbnum(ami_long r, ami_long g, ami_long b)

{

    ami_color c;

    switch ((r > LONG_MAX/2) << 2 | (g > LONG_MAX/2) << 1 | (b > LONG_MAX/2)) {

        /* rgb */
        /* 000 */ case 0: c = ami_black;   break;
        /* 001 */ case 1: c = ami_blue;    break;
        /* 010 */ case 2: c = ami_green;   break;
        /* 011 */ case 3: c = ami_cyan;    break;
        /* 100 */ case 4: c = ami_red;     break;
        /* 101 */ case 5: c = ami_magenta; break;
        /* 110 */ case 6: c = ami_yellow;  break;
        /* 111 */ case 7: c = ami_white;   break;

    }

    return (c); /* exit with translated color */

}

/** ****************************************************************************

Set screen attribute cached

Sets the current root screen attribute according to the current set of attributes.
Note that if multiple attributes are set, but the system only allows a single
attribute, then only the standout attribute will be set, since that is the last
attribute to be activated. The turn off attributes are processed first, because
in a single attribute system, off attributes turn all of them off.

The standout attribute is a series of attributes in priority order.

The attribute change is only coped to the root window if it has changed from the
root window to cut down on chatter between the modules.

*******************************************************************************/

static void setattrs(int at)

{

   /* process "off" attributes */
   if ((BIT(sasuper) & at) != (BIT(sasuper) & attr)) /* has changed */
        if (!(BIT(sasuper) & at)) (*superscript_vect)(stdout, FALSE);
    if ((BIT(sasubs) & at) != (BIT(sasubs) & attr)) /* has changed */
        if (!(BIT(sasubs) & at)) (*subscript_vect)(stdout, FALSE);
    if ((BIT(sablink) & at) != (BIT(sablink) & attr)) /* has changed */
        if (!(BIT(sablink) & at)) (*blink_vect)(stdout, FALSE);
    if ((BIT(sastkout) & at) != (BIT(sastkout) & attr)) /* has changed */
        if(!(BIT(sastkout) & at)) (*strikeout_vect)(stdout, FALSE);
    if ((BIT(saital) & at) != (BIT(saital) & attr)) /* has changed */
        if (!(BIT(saital) & at)) (*italic_vect)(stdout, FALSE);
    if ((BIT(sabold) & at) != (BIT(sabold) & attr)) /* has changed */
        if (!(BIT(sabold) & at)) (*bold_vect)(stdout, FALSE);
    if ((BIT(saundl) & at) != (BIT(saundl) & attr)) /* has changed */
        if (!(BIT(saundl) & at)) (*underline_vect)(stdout, FALSE);
    if ((BIT(sarev) & at) != (BIT(sarev) & attr)) /* has changed */
        if (!(BIT(sarev) & at)) (*reverse_vect)(stdout, FALSE);

    /* process "on" attributes */
   if ((BIT(sasuper) & at) != (BIT(sasuper) & attr)) /* has changed */
        if (BIT(sasuper) & at) (*superscript_vect)(stdout, TRUE);
    if ((BIT(sasubs) & at) != (BIT(sasubs) & attr)) /* has changed */
        if (BIT(sasubs) & at) (*subscript_vect)(stdout, TRUE);
    if ((BIT(sablink) & at) != (BIT(sablink) & attr)) /* has changed */
        if (BIT(sablink) & at) (*blink_vect)(stdout, TRUE);
    if ((BIT(sastkout) & at) != (BIT(sastkout) & attr)) /* has changed */
        if (BIT(sastkout) & at) (*strikeout_vect)(stdout, TRUE);
    if ((BIT(saital) & at) != (BIT(saital) & attr)) /* has changed */
        if (BIT(saital) & at) (*italic_vect)(stdout, TRUE);
    if ((BIT(sabold) & at) != (BIT(sabold) & attr)) /* has changed */
        if (BIT(sabold) & at) (*bold_vect)(stdout, TRUE);
    if ((BIT(saundl) & at) != (BIT(saundl) & attr)) /* has changed */
        if (BIT(saundl) & at) (*underline_vect)(stdout, TRUE);
    if ((BIT(sarev) & at) != (BIT(sarev) & attr)) /* has changed */
        if (BIT(sarev) & at) (*reverse_vect)(stdout, TRUE);

    attr = at; /* update the root mask */

}

/*******************************************************************************

Output character to root window

Outputs a single character to the root window and advances the x cursor. Note
we assume auto is off and x can climb to infinity (LONG_MAX).

*******************************************************************************/

static void wrtchr(char c)

{

    (*ofpwrite)(OUTFIL, &c, 1);
    curx++;

}

/*******************************************************************************

Output character string to root window

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (LONG_MAX).

*******************************************************************************/

static void wrtstr(char* s)

{

    while (*s) { wrtchr(*s); s++; }

}

/*******************************************************************************

Output extended character string to root window

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (LONG_MAX). This routine writes extended
UTF-8 characters, meaning that it only advances the cursor once for the whole
string.

*******************************************************************************/

static void wrtext(char* s)

{

    while (*s) { (*ofpwrite)(OUTFIL, s, 1); s++; }
    curx++; /* advance cursor */

}

/*******************************************************************************

Find point in rectangle

Finds if the given x y point is included in the given rectangle. Returns true if
so.

*******************************************************************************/

static int inrect(ami_long x, ami_long y, rectangle* r)

{

    return (r->x1 <= x && x <= r->x2 && r->y1 <= y && y <= r->y2);

}

/*******************************************************************************

Output character to root window with clipping

Outputs a single character to the root window and advances the x cursor. Note
we assume auto is off and x can climb to infinity (LONG_MAX).

Clips to the given rectangle. Note the cursor position is still advanced.

*******************************************************************************/

static void wrtchrclp(char c, rectangle* cr)

{

    if (inrect(curx, cury, cr) &&
        (!hovflt || fndtop(curx, cury) == hovflt)) { /* not clipped */

        (*ofpwrite)(OUTFIL, &c, 1);
        curx++;

    } else setcursor(curx+1, cury); /* just move the cursor */

}

/*******************************************************************************

Output character string to root window with clipping

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (LONG_MAX).

Clips to the given rectangle. Note the cursor position is still advanced.

*******************************************************************************/

static void wrtstrclp(char* s, ami_long l, rectangle* cr)

{

    while (*s && l) { wrtchrclp(*s, cr); s++; l--; }

}

/*******************************************************************************

Output extended character string to root window

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (LONG_MAX). This routine writes extended
UTF-8 characters, meaning that it only advances the cursor once for the whole
string.

Clips to the given rectangle. Note the cursor position is still advanced.

*******************************************************************************/

static void wrtextclp(char* s, rectangle* cr)

{

    if (inrect(curx, cury, cr) &&
        (!hovflt || fndtop(curx, cury) == hovflt)) { /* not clipped */

        while (*s) { (*ofpwrite)(OUTFIL, s, 1); s++; }
        curx++; /* advance cursor */

    } else setcursor(curx+1, cury); /* just move the cursor */


}

/** ****************************************************************************

Initalize screen

Clears all the parameters in the present screen context. Also, the backing
buffer bitmap is created and cleared to the present colors.

*******************************************************************************/

static void iniscn(winptr win, scnrec* sc)

{

    int x, y;
    int si;
    scnrec* scp;   /* pointer to screen location */

    /* The client area beyond the buffer shows the background the surface
       was cleared to, and a later redraw must reproduce it even if the
       program has since changed its background for drawing, so the color
       is noted with the screen being cleared. */
    for (si = 0; si < MAXCON; si++)
        if (win->screens[si] == sc) win->sbcolor[si] = win->bcolor;

    /* clear buffer */
    for (y = 1; y <= win->maxy; y++)
        for (x = 1; x <= win->maxx; x++) {

        /* index screen character location */
        scp = &SCNBUF(sc, x, y);
        /* Place character to buffer. The clear takes the colors and
           attributes in force for the window, as the terminal's own clear
           does: a program that sets a background and then clears expects
           the cleared surface in that background. These were black on
           white whatever the window was set to, so a clear threw the
           chosen colors away and only the characters written afterwards
           carried them. */
        scp->ch = ' ';
        scp->forec = win->fcolor;
        scp->backc = win->bcolor;
        scp->attr = win->attr;

    }

}

/** ****************************************************************************

Clear screen buffers

Releases all screen buffers for the current window and clears their indexes.

*******************************************************************************/

static void clrbufs(winptr win)

{

    int si; /* index for screen buffers */

    for (si = 0; si < MAXCON; si++) {

        if (win->screens[si]) free(win->screens[si]); /* free screen data */
        win->screens[si] = NULL; /* clear screen data */
        if (win->fmask) free(win->fmask); /* free mask data */
        win->fmask = NULL; /* clear it */

    }

}

/** ****************************************************************************

Allocate new mask

Allocates a new forward mask for the given window.

*******************************************************************************/

static void alcfmask(winptr win)

{

    ami_long i, t;

    t = win->bufy*win->bufx; /* find total characters in buffer */
    i = t/8; /* find bytes for forward mask */
    if (t%8) i++; /* round up */
    win->fmask = malloc(i); /* allocate forward mask */
    if (!win->fmask) error("Out of memory");
    memset(win->fmask, 0xff, i); /* set the bitmap */
    win->fmasklen = i; /* save the length */

}

/** ****************************************************************************

Construct new forward bitmask

Clears the forward bitmask, then draws 0's in it for each window in the Z-order
that is in front of it. This forms a mask of where drawing on the window is
valid.

*******************************************************************************/

/*******************************************************************************

Find the ancestor clip of a window

A child window shows only within its parent, and that in turn within its
own parent: the visible region of a window is the intersection of the
client areas of every ancestor up the tree. Fills r with that region in
absolute terms and returns TRUE, or FALSE when the intersection is empty
and the window shows nowhere (as when its parent is minimized). A widget
face is exempted one level: it lives in its owner's decorations -- the
menu bar sits in the frame's menu row -- so it clips to the owner's whole
window rather than its client.

*******************************************************************************/

static int ancclip(winptr win, rectangle* r)

{

    winptr    c = win; /* the window whose bound we take next */
    winptr    p = win->parwin;
    rectangle pr, ri;

    setrect(r, 1, 1, dimx, dimy); /* the screen bounds everything */
    while (p) {

        if (c->widget)
            setrect(&pr, absx(p), absy(p),
                         absx(p)+p->pmaxx-1, absy(p)+p->pmaxy-1);
        else
            setrect(&pr, absx(p)+p->coffx, absy(p)+p->coffy,
                         absx(p)+p->coffx+p->cmaxx-1,
                         absy(p)+p->coffy+p->cmaxy-1);
        if (!intersect(r, &pr)) return (FALSE); /* shows nowhere */
        intersection(&ri, r, &pr);
        *r = ri;
        c = p;
        p = p->parwin;

    }

    return (TRUE);

}

static void calcfmask(winptr win)

{

    winptr    wp;         /* window structure pointer */
    rectangle r1, r2, r3; /* window rectangles */
    rectangle ra, rw;     /* ancestor clips */
    ami_long  x, y;
    ami_long  cx, cy;
    ami_long  l;

    if (!win->fmask) return; /* window not fully constructed yet */
    memset(win->fmask, 0xff, win->fmasklen); /* set the bitmap */
    /* find the onscreen client rectangle in root terms */
    setrect(&r1, absx(win)+win->coffx, absy(win)+win->coffy,
                 absx(win)+win->coffx+win->cmaxx-1, 
                 absy(win)+win->coffy+win->cmaxy-1);
    /* The window shows only within its ancestors: cells outside the
       ancestor clip are off, so nothing of the window draws where it
       hangs out of its parent. */
    if (!ancclip(win, &ra)) {

        memset(win->fmask, 0, win->fmasklen); /* shows nowhere */
        return;

    }
    for (y = r1.y1; y <= r1.y2; y++)
        for (x = r1.x1; x <= r1.x2; x++)
            if (!inrect(x, y, &ra)) {

            cx = x-(absx(win)+win->coffx);
            cy = y-(absy(win)+win->coffy);
            if (cx < win->bufx && cy < win->bufy) {

                l = cy*win->bufx+cx;
                win->fmask[l/8] &= ~(1<<(l%8));

            }

        }
    wp = win->zmin2max; /* index windows in front by Z order */
    while (wp) { /* tour the (possibly) lapping windows */

        /* A window above occludes only where it shows: clipped by its own
           ancestors. One hanging out of its parent does not blot windows
           below in the region where it is not drawn. */
        if (!ancclip(wp, &rw)) { wp = wp->zmin2max; continue; }
        /* set window rectangle in root terms */
        setrect(&r2, absx(wp), absy(wp),
                    absx(wp)+wp->pmaxx-1, absy(wp)+wp->pmaxy-1);
        if (!intersect(&r2, &rw)) { wp = wp->zmin2max; continue; }
        intersection(&r3, &r2, &rw);
        r2 = r3;
        if (intersect(&r1, &r2)) { /* if this window overlaps our client area */

            intersection(&r3, &r1, &r2); /* find the intersected rectangle */
            /* draw into mask */
            for (y = r3.y1; y <= r3.y2; y++)
                for (x = r3.x1; x <= r3.x2; x++) {

                /* find net client offset */
                cx = x-(absx(win)+win->coffx);
                cy = y-(absy(win)+win->coffy);
                /* The mask is shaped by the buffer, and the client area
                   can be larger than the buffer, which is what a program
                   asks for when it sizes the buffer smaller than the
                   window. Cells outside the buffer have no bit to clear;
                   writing one lands outside the allocation. */
                if (cx < win->bufx && cy < win->bufy) {

                    l = cy*win->bufx+cx; /* find character location */
                    win->fmask[l/8] &= ~(1<<(l%8)); /* mask off that bit */

                }

            }

        }   
        wp = wp->zmin2max; /* next window entry */

    }

    /* diagnostic: print forward mask */
#ifdef PRTFMASK
    fprintf(stderr, "Forward mask: wid: %lld size x: %lld y: %lld\n",
                    AMI_LONG_CAST(win->wid), AMI_LONG_CAST(win->maxx), AMI_LONG_CAST(win->maxy));
    fflush(stderr);
    fprintf(stderr, "     ");
    for (x = 1; x <= win->maxx; x++) fprintf(stderr, "%c", (char)(x%10+'0'));
    fprintf(stderr, "\n");
    fflush(stderr);
    for (y = 1; y <= win->maxy; y++) {

        fprintf(stderr, "%03lld: ", AMI_LONG_CAST(y));
        for (x = 1; x <= win->maxx; x++) {

            l = (y-1)*win->bufx+(x-1); /* find character location */
            if (win->fmask[l/8] & 1<<(l%8)) fprintf(stderr, "1");
            else fprintf(stderr, "0");

        }
        fprintf(stderr, "\n"); 
        fflush(stderr);

    }
    fprintf(stderr, "\n"); 
    fflush(stderr);
#endif

}

/** ****************************************************************************

Recalculate all window forward masks

Reconstructs the forward mask for all windows. Goes through the windows list and
recalculates the forward mask. This is used anytime new windows or a change in
window position, size or Z-order is done.

This could be optimized by checking if the target window lies in the bounding
box of the change.

*******************************************************************************/

void recalcfmask(void)

{

    winptr    win; /* pointer to windows list */

    win = winlst; /* get the master list */
    while (win) { /* traverse the windows list */

        calcfmask(win); /* recalculate forward mask */
        win = win->winlst; /* next window */

    }

}

/*******************************************************************************

Resize window buffer

Resizes the given window's screen buffers to the given dimensions, keeping
their contents where old and new overlap; new cells are blanked in the
window's current colors. The forward mask is reallocated to match, since it
is sized and indexed by the buffer.

The screens hold window content in both buffered and follow modes: they are
what repaints the surface when the window arrangement changes. So every
follow mode path that changes the client size must come through here, or
the client and the buffer disagree and buffer indexed drawing runs outside
the allocation.

*******************************************************************************/

static void resizewinbuf(winptr win, ami_long nx, ami_long ny)

{

    scnrec* ns; /* new screen */
    scnrec* os; /* old screen */
    scnrec* scp;
    ami_long    x, y;
    int     si;

    if (nx < 1) nx = 1; /* observe minimum size */
    if (ny < 1) ny = 1;
    if (nx == win->maxx && ny == win->maxy) return; /* nothing to do */
    for (si = 0; si < MAXCON; si++) if (win->screens[si]) {

        os = win->screens[si]; /* index old screen */
        ns = malloc(sizeof(scnrec)*ny*nx);
        if (!ns) error("Out of memory");
        for (y = 1; y <= ny; y++)
            for (x = 1; x <= nx; x++) {

            scp = &ns[(y-1)*nx+(x-1)]; /* index new cell */
            if (x <= win->maxx && y <= win->maxy)
                /* keep old contents */
                *scp = os[(y-1)*win->maxx+(x-1)];
            else { /* new cell, blank in window colors */

                scp->ch = ' ';
                scp->forec = win->fcolor;
                scp->backc = win->bcolor;
                scp->attr = win->attr;

            }

        }
        free(os); /* release the old screen */
        win->screens[si] = ns;

    }
    win->maxx = nx; /* set new buffer size */
    win->maxy = ny;
    win->bufx = nx;
    win->bufy = ny;
    if (win->fmask) free(win->fmask); /* mask must match the buffer */
    alcfmask(win);
    if (win->curx > nx) win->curx = nx; /* keep the cursor on the surface */
    if (win->cury > ny) win->cury = ny;

}

/*******************************************************************************

Grow window buffer

Grows the window buffer to at least the given size, keeping contents. The
buffer never shrinks: this serves the follow mode root window, which keeps
its largest extent over terminal size changes.

*******************************************************************************/

static void growwinbuf(winptr win, ami_long nx, ami_long ny)

{

    if (nx <= win->maxx && ny <= win->maxy) return; /* nothing to grow */
    if (nx < win->maxx) nx = win->maxx; /* never shrink */
    if (ny < win->maxy) ny = win->maxy;
    resizewinbuf(win, nx, ny);

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

/** ****************************************************************************

Draw frame on window

Draws a frame around the indicated window. This is one of the following:

(ASCII)

    +------------------+
    |            _ ^ X |
    |==================|
    |                  |
    |                  |
    +------------------+

(UTF-8)
    ╔══════════════════╗
    ║           - ▢ Ⓧ ║
    ╠══════════════════╣
    ║                  ║
    ║                  ║
    ╚══════════════════╝

(it is seamless in xterm, here it depends on the editor).

Accepts a clipping rectangle and clips to that.

*******************************************************************************/

static void drwfrm(winptr win, rectangle* cr)

{

    ami_long x, y, l;

    if (win->frame) { /* draw window frame */

        /* The decorations draw in the frame color on the decoration
           background, with no attributes. These go through the caches, so
           the drawing that follows knows the state left behind. Without
           the background set, the decorations landed on whatever
           background the last drawing left, and the system bar showed
           whatever color the window content had been painted with. */
        setfcolor(win->frmcolor);
        setbcolor(ami_white);
        setattrs(frmhrev? BIT(sarev): 0);
        if (win->size) { /* draw size bars */

            /* draw top and bottom */
            setcursor(absx(win), absy(win));
            wrtextclp(frmchrs[toplftcnr], cr);
            for (x = 2; x <= win->pmaxx-1; x++) wrtextclp(frmchrs[horzlin], cr);
            wrtextclp(frmchrs[toprgtcnr], cr);

            setcursor(absx(win), absy(win)+win->pmaxy-1);
            wrtextclp(frmchrs[btmlftcnr], cr);
            for (x = 2; x <= win->pmaxx-1; x++) wrtextclp(frmchrs[horzlin], cr);
            wrtextclp(frmchrs[btmrgtcnr], cr);

            /* draw sides */
            for (y = absy(win)+1; y < absy(win)+win->pmaxy-1; y++) {

                setcursor(absx(win), y);
                wrtextclp(frmchrs[vertlin], cr);
                setcursor(absx(win)+win->pmaxx-1, y);
                wrtextclp(frmchrs[vertlin], cr);

            }

        }
        if (win->sysbar && win->pmaxy >= 3) { /* draw system bar */

            y = win->size; /* offset to system bar */
            /* draw blanks in title section */
            if (win->pmaxx-6 > 2) {

                setcursor(absx(win)+1, absy(win)+y);
                for (x = 1; x < win->pmaxx-6; x++) wrtchrclp(' ', cr);

            }
            x = win->pmaxx-6; /* start of system bar buttons */
            /* lay each button down if it's location is valid */
            if (x > 2) {

                /* set draw location */
                setcursor(absx(win)+x-1, absy(win)+y);
                wrtextclp(frmchrs[minbtn], cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(absx(win)+x-1, absy(win)+y);
                wrtchrclp(' ', cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(absx(win)+x-1, absy(win)+y);
                wrtextclp(frmchrs[maxbtn], cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(absx(win)+x-1, absy(win)+y);
                wrtchrclp(' ', cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(absx(win)+x-1, absy(win)+y);
                wrtextclp(frmchrs[canbtn], cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(absx(win)+x-1, absy(win)+y);
                wrtchrclp(' ', cr);

            }

            /* draw title, if exists */
            if (win->title) {

                l = strlen(win->title); /* get length */
                /* limit string length to available space */
                if (win->pmaxx-6-4 < l) l = win->pmaxx-6-4;
                if (l > 0) { /* there is room for some of the title */

                    setcursor(absx(win)+2+(win->pmaxx-6-4)/2-(l/2), absy(win)+y);
                    wrtstrclp(win->title, l, cr);

                }

            }

            /* draw underbar */
            y++;
            setcursor(absx(win), absy(win)+y);
            if (win->pmaxy <= 3) wrtextclp(frmchrs[btmlftcnr], cr);
            else wrtextclp(frmchrs[intlft], cr);
            for (x = 2; x <= win->pmaxx-1; x++) wrtextclp(frmchrs[sysudl], cr);
            if (win->pmaxy <= 3) wrtextclp(frmchrs[btmrgtcnr], cr);
            else wrtextclp(frmchrs[intrgt], cr);

        }

    }

}

/** ****************************************************************************

Find if cursor is in screen bounds internal

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

static int intcurbnd(winptr win)

{

    return (win->curx >= 1 && win->curx <= win->cmaxx &&
            win->cury >= 1 && win->cury <= win->cmaxy);

}

/*******************************************************************************

Position cursor in window

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that. We consider the current position and
visible/invisible status, and try to output only the minimum terminal controls
to bring the old state of the display to the same state as the new display.

*******************************************************************************/

static void setcur(winptr win)

{

    /* The physical cursor belongs to the focus window: cursor activity in
       any other window changes only that window's recorded position, and
       the visible cursor stays where the user is typing. Without this the
       cursor was left wherever the last drawing operation finished, and
       came back to the focus window only when something happened to
       restore that window last. When no window has focus yet, the given
       window stands. */
    if (curfocus && win != curfocus) return;
    if (indisp(win)) { /* in display */

        /* check cursor in bounds */
        if (intcurbnd(win)) {

            setcurvis(win->curv); /* set cursor on or off */
            /* position actual cursor */
            setcursor(win->curx+absx(win)-1+win->coffx,
                      win->cury+absy(win)-1+win->coffy);

        } else setcurvis(FALSE); /* set cursor off out of bounds */

    }

}

/** ****************************************************************************

Clip to root surface

Clips the given rectangle to the terminal root surface. The rectangle must be
regular.

*******************************************************************************/

static void cliproot(rectangle* r)

{

    if (r->x1 < 1) r->x1 = 1;
    if (r->x2 > dimx) r->x2 = dimx;

    if (r->y1 < 1) r->y1 = 1;
    if (r->y2 > dimy) r->y2 = dimy;

}

/** ****************************************************************************

Restore screen with clipping

Updates all the buffer and screen parameters from the display screen to the
terminal into the selected rectangle. Note we assume there exists an
intersection of the window with the clipping rectangle.

*******************************************************************************/

static void restoreclp(winptr win,   /* window to restore */
                      rectangle* cr) /* area to restore */

{

    scnrec* scp;   /* pointer to screen location */
    scnrec* sc;
    ami_long x, y;
    ami_long bx, by;   /* buffer location */
    ami_long l;        /* mask index */
    rectangle r1, r2;
    rectangle rca, rcc; /* ancestor clip */
    char runbuf[MAXLIN]; /* run of characters to emit as a string */
    ami_long runx;           /* screen x the run starts at */
    ami_color runfc, runbc; /* the run's colors */
    int  runat;          /* the run's attributes */

    if (!win->visible) return; /* nothing onscreen to restore */
    /* the window shows only within its ancestors: frame, client and
       margins all clip to them */
    if (!ancclip(win, &rca)) return; /* shows nowhere */
    if (!intersect(cr, &rca)) return; /* no part in the clip area */
    intersection(&rcc, cr, &rca);
    cr = &rcc;
    setcurvis(FALSE); /* turn off cursor for drawing */
    if (win->frame) drwfrm(win, cr); /* draw window frame */
    if (!win->bufmod) {

        /* Follow mode: there is no content store, the program owns the
           content, as it does in the graphical implementations. The
           manager paints the frame above and the client background here,
           and the program repaints its content on the redraw and resize
           events the manager sends for the operations that disturb it. */
        ami_long mx, my;

        setfcolor(win->fcolor);
        setbcolor(win->sbcolor[win->curdsp-1]);
        setattrs(0);
        for (my = 1; my <= win->cmaxy; my++)
            for (mx = 1; mx <= win->cmaxx; mx++) {

            ami_long sx = absx(win)+win->coffx+mx-1;
            ami_long sy = absy(win)+win->coffy+my-1;
            ami_long ml = (my-1)*win->bufx+(mx-1);

            if (!inrect(sx, sy, cr)) continue; /* outside the clip */
            /* an occluded cell belongs to the window above */
            if (mx <= win->bufx && my <= win->bufy &&
                !(win->fmask[ml/8] & 1<<(ml%8))) continue;
            setcursor(sx, sy);
            wrtchr(' ');

        }
        setcur(curfocus? curfocus: win); /* reenable cursor */

    } else { /* buffered mode is on */

        /* Find intersection with client area. The area is bounded by both
           the client dimensions and the buffer dimensions: the client can
           be larger than the buffer, and the buffer must not be read
           outside itself. */
        setrect(&r1, absx(win)+win->coffx, absy(win)+win->coffy,
                   absx(win)+win->coffx+
                       (win->cmaxx < win->maxx? win->cmaxx: win->maxx)-1,
                   absy(win)+win->coffy+
                       (win->cmaxy < win->maxy? win->cmaxy: win->maxy)-1);
        if (intersect(cr, &r1)) { /* there is an intersection with client area */

            intersection(&r2, cr, &r1); /* find client-clip intersection */
            sc = win->screens[win->curdsp-1]; /* index screen */
            /* Restore window from buffer. Cells the forward mask holds off
               belong to windows above this one and are skipped: without
               that check a restore of a lower window paints over whatever
               overlaps it. The cursor is positioned per cell, which costs
               nothing for consecutive visible cells, since the position
               cache knows the cursor advances with each character. */
            /* Draw in runs. A run is a stretch of cells on one line that
               are visible, share colors and attributes, and hold no
               control character. Those go out with one string call, which
               exists to bypass the per character protocol below; the
               leftovers go a character at a time. Drawing every cell
               singly was the bulk of the manager's output cost. */
            for (y = r2.y1; y <= r2.y2; y++) {

                ami_long rl = 0; /* length of the run being gathered */

                for (x = r2.x1; x <= r2.x2+1; x++) {

                    int vis = FALSE;

                    scp = NULL;
                    if (x <= r2.x2) { /* a real cell, not the end sentinel */

                        bx = x-(absx(win)+win->coffx)+1; /* buffer location */
                        by = y-(absy(win)+win->coffy)+1;
                        l = (by-1)*win->bufx+(bx-1); /* mask index */
                        vis = !!(win->fmask[l/8] & 1<<(l%8));
                        if (vis) scp = &SCNBUF(sc, bx, by);

                    }
                    /* a cell continues the run if it is visible, matches
                       the run's colors and attributes, and is printable */
                    if (scp && scp->ch >= ' ' && scp->ch != 0x7f &&
                        (!rl || (scp->forec == runfc && scp->backc == runbc &&
                                 scp->attr == runat)) && rl < MAXLIN-1) {

                        if (!rl) { /* starting a run, note its state */

                            runfc = scp->forec;
                            runbc = scp->backc;
                            runat = scp->attr;
                            runx = x;

                        }
                        runbuf[rl++] = scp->ch;

                    } else { /* the run ends here */

                        if (rl) { /* flush what was gathered */

                            setcursor(runx, y);
                            setfcolora(runfc, runbc, runat);
                            setbcolor(runbc);
                            setattrs(runat);
                            (*wrtstrn_vect)(stdout, runbuf, rl);
                            curx += rl; /* the string moved the cursor */
                            rl = 0;

                        }
                        if (scp) { /* an odd cell, place it singly */

                            setcursor(x, y);
                            setfcolora(scp->forec, scp->backc, scp->attr);
                            setbcolor(scp->backc);
                            setattrs(scp->attr);
                            wrtchr(scp->ch);

                        }

                    }

                }

            }

        }
        /* If the buffer is smaller than the client area, the part of the
           client beyond it is filled with the window's background. That
           area belongs to the window and shows its background, which is
           what the graphical implementation does with the margins it has
           left over, and what the terminal does under its own buffer. The
           background used is the one the surface was cleared to, not the
           current drawing background: a program that paints and then
           changes its background for later drawing must see the same
           margins on a redraw that the paint gave it. */
        if (win->maxx < win->cmaxx || win->maxy < win->cmaxy) {

            ami_long mx, my;

            setfcolor(win->fcolor);
            setbcolor(win->sbcolor[win->curdsp-1]);
            setattrs(0);
            for (my = 1; my <= win->cmaxy; my++) {

                /* the strip right of the buffer, and whole lines below it */
                mx = my <= win->maxy? win->maxx+1: 1;
                for (; mx <= win->cmaxx; mx++) {

                    ami_long sx = absx(win)+win->coffx+mx-1;
                    ami_long sy = absy(win)+win->coffy+my-1;
                    ami_long ml = (my-1)*win->bufx+(mx-1);

                    if (!inrect(sx, sy, cr)) continue; /* outside the clip */
                    /* an occluded cell belongs to the window above */
                    if (mx <= win->bufx && my <= win->bufy &&
                        !(win->fmask[ml/8] & 1<<(ml%8))) continue;
                    setcursor(sx, sy);
                    wrtchr(' ');

                }

            }

        }
        setcur(curfocus? curfocus: win); /* reenable cursor */

    }

}

/** ****************************************************************************

Restore screen with clipping

Updates all the buffer and screen parameters from the display screen to the
terminal.

*******************************************************************************/

static void restore(winptr win) /* window to restore */

{

    rectangle cr;

    /* set clipping rectangle to whole window */
    setrect(&cr, absx(win), absy(win),
               absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);
    restoreclp(win, &cr);

}

/** ****************************************************************************

Redraw screen

Given an onscreen rectangle, redraws the "hole" in the screen by touring the
current windows and redrawing parts of the screen from that.

There are two basic algorithms for redrawing a screen hole, the bottom up and
top down methods. A bottom up draws each window found from the bottom of the Z
order to the top. A top down method draws from the top of the Z order to the
bottom. The difference is that the bottom up method will redundantly redraw
parts of the window as uppermost Z order elements draw over lower ones. In the
top down method, we must keep track of already drawn parts of the rectangle,
which means keeping a list of increasingly fractured rectangles. It does not
redundantly draw, and thus is more efficient and produces less onscreen
"sparkle" due to overdraws. The current implementation uses top down.

The "fractioning" keeps the list on the stack, and tracks the subtrectangles
by recursion. First the intersection of the draw area and the list window is
found and redrawn, then that is subtracted from the draw area, yeilding 1 to 4
subrectangles, then each of these is recursively applied to the next window in
the max 2 min list given.

*******************************************************************************/

static void annredraw(winptr win); /* forward */

static void redraw(winptr win, ami_long x1, ami_long y1, ami_long x2, ami_long y2)

{

    rectangle r1, r2, r3, rt, rl, rr, rb;
    rectangle rw; /* ancestor clip */

    if (win) { /* if window exists */


        setrect(&r1, x1, y1, x2, y2); /* set update rectangle */
        cliproot(&r1); /* clip to terminal root window */
        /* The window covers only the part of its rectangle within its
           ancestors; the rest of the damage falls through to the windows
           below. */
        setrect(&r2, absx(win), absy(win),
                    absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);
        if (ancclip(win, &rw) && intersect(&r2, &rw)) {

            intersection(&r3, &r2, &rw);
            r2 = r3;

        } else setrect(&r2, 0, 0, -1, -1); /* shows nowhere: empty */
        if (intersect(&r1, &r2)) { /* there is an intersection */

            intersection(&r3, &r1, &r2); /* find the intersected rectangle */
            /* restore that part of the window */
            restoreclp(win, &r3);
            annredraw(win); /* a follow mode window needs its program */
            if (win->zmax2min) { /* not last window in max 2 min list*/

                /* find rectangle fractions */
                subrect(&r1, &r3, &rt, &rl, &rr, &rb);
                /* process down list for each fraction */
                if (!zerorect(&rt))
                    redraw(win->zmax2min, rt.x1, rt.y1, rt.x2, rt.y2);
                if (!zerorect(&rl))
                    redraw(win->zmax2min, rl.x1, rl.y1, rl.x2, rl.y2);
                if (!zerorect(&rr))
                    redraw(win->zmax2min, rr.x1, rr.y1, rr.x2, rr.y2);
                if (!zerorect(&rb))
                    redraw(win->zmax2min, rb.x1, rb.y1, rb.x2, rb.y2);

            }

        } else
            /* no intersect this window, go next */
            redraw(win->zmax2min, x1, y1, x2, y2);

    }

}

/** ****************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void clrscn(FILE* f)

{

    winptr win; /* windows record pointer */
    scnptr sc;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index current update screen */
    win->curx = 1; /* set cursor at home */
    win->cury = 1;
    iniscn(win, sc); /* clear screen buffer */
    if (indisp(win)) restore(win); /* also process to display */

}

/** ****************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

static void itab(FILE* f)

{

    winptr win; /* windows record pointer */
    ami_long i;
    scnptr sc;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* first, find if next tab even exists */
    i = win->curx+1; /* get just after the current x position */
    if (i < 1) i = 1; /* don't bother to search to left of screen */
    /* find tab or end of screen; the stop for column i is tab[i-1], as
       settab and the default stops place it */
    while (i < MAXTAB && !win->tab[i-1] && i < win->maxx) i++;
    if (win->tab[i-1]) /* not off right of tabs */
       win->curx = i; /* set position to that tab */
    setcur(win); /* update screen */

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

static void wigevt(wigptr wg, ami_evtrec* er); /* forward */
static ami_long wighit(wigptr wg, ami_long lx, ami_long ly); /* forward */
static void wigdrag(void); /* forward */
static void clspops(int downto); /* forward */
static void wigdrw(wigptr wg); /* forward */
static int  menselkey(ami_long etype);   /* forward: menu select mode */
static int  menselstart(void);
static void menselmouse(winptr win);
static void mbarsiz(winptr win); /* forward */
static void fronttree(winptr win); /* forward */
static void intsetpos(winptr win, ami_long x, ami_long y); /* forward */

static void intsendevent(winptr win, ami_evtrec* er)

{

    ami_evtrec ec; /* copy of event record */

    /* events for a widget window go to the widget logic, never to the
       client program */
    if (win && win->widget) { wigevt(win->wig, er); return; }
    memcpy(&ec, er, sizeof(ami_evtrec));
    ec.winid = 0; /* set anonymous window id */
    if (win) ec.winid = win->wid; /* overwrite window id */
    enquepaevt(&ec); /* send to queue */

}

/*******************************************************************************

Announce redraw to the program

Asks the program that owns an unbuffered window to repaint it. Buffered
windows are repainted from their buffers by the manager; a follow mode
window has no content store, so the manager repaints the frame and the
client background, and the program supplies the content, the way the
graphical implementations do with expose events. Called by the manager
initiated operations that disturb the window: moves, sizes, Z order
changes, and the repaint of revealed areas. It is never called from the
program's own drawing, which would ask the program to redraw because it
drew.

*******************************************************************************/

static void annredraw(winptr win)

{

    ami_evtrec er;

    if (!win->bufmod && win->visible && !win->widget && !win->redrawpend) {

        win->redrawpend = TRUE; /* one announcement serves until delivered */
        mclogf("queue redraw wid %lld\n", AMI_LONG_CAST(win->wid));
        er.etype = ami_etredraw;
        intsendevent(win, &er);

    }

}

/*******************************************************************************

Announce resize to the program

Tells the program its window client area changed size, with the new
dimensions, as the graphical implementations do. Sent for user sizing and
for decoration changes, which resize the client within the same window.

*******************************************************************************/

static void annresize(winptr win)

{

    ami_evtrec er;

    if (win->visible && !win->widget) {

        mclogf("queue resize wid %lld %lldx%lld\n",
                             AMI_LONG_CAST(win->wid), AMI_LONG_CAST(win->cmaxx), AMI_LONG_CAST(win->cmaxy));
        er.etype = ami_etresize;
        er.rszx = win->cmaxx;
        er.rszy = win->cmaxy;
        intsendevent(win, &er);
        /* The program draws on redraw announcements, to the size it last
           heard. A redraw already queued sits ahead of this resize, so
           the draw it produces is to the old size: every resize is
           followed by its own redraw, and the last size heard always has
           a draw behind it. */
        win->redrawpend = FALSE;
        annredraw(win);

    }

}

/*******************************************************************************

Remove all focus windows

Removes all windows that show focus. Usually used before marking a window as
having focus. Note that normally there should only be a single window with
focus.

*******************************************************************************/

void remfocus(void)

{

    winptr    win; /* pointer to windows list */
    ami_evtrec ev;  /* local event record */

    win = winlst; /* get the master list */
    while (win) { /* traverse the windows list */

        if (win->focus) { /* if this window has focus */

            /* Clear the flag before announcing the loss. A widget redraws
               its face when it hears this, and reads the flag to decide
               whether to show itself focused: announcing first left every
               widget drawn as though it still held the focus. */
            win->focus = FALSE;
            /* send defocus message */
            ev.etype = ami_etnofocus; /* set no focus event */
            intsendevent(win, &ev); /* send to queue */

        }
        win->focus = FALSE;
        curfocus = NULL;
        win = win->winlst; /* next window */

    }

}

/*******************************************************************************

Find character overhead of the window decorations

Gives the number of character cells the decorations occupy beyond the client
area, in x and y. All decorations live inside the frame: the border needs
frame and size, and the system bar with its menu row needs frame and sysbar.
drwfrm() draws by these same rules, so client geometry computed here stays in
step with what is actually drawn.

*******************************************************************************/

static ami_long decorx(winptr win)

{

    return ((win->frame && win->size)*2);

}

static ami_long decory(winptr win)

{

    /* An installed menu adds its own row: the system bar and its
       underbar stay as they are and the client starts below the menu,
       as the graphical form presents it.

       The menu row is counted whether or not there is a frame. It is
       not a frame decoration -- it is a band above the client, and the
       client is what is left inside the frame, the system bar and the
       menu. Gated on the frame, as it was, a frameless window kept a
       client the full height of the window and the bar was laid over
       the client's first row: a program drawing from row one drew over
       its own menu and never saw it. */
    return ((win->frame && win->size)*2+(win->frame && win->sysbar)*2+
            (win->mbar != 0));

}

/*******************************************************************************

Check x,y location is in the client area

Finds if the given x,y location lies in the client area. Note that this does not
include frame or system bar.

*******************************************************************************/

static int inclient(winptr win, ami_long x, ami_long y)

{

    ami_long ox, oy;

    ox = decorx(win);
    oy = decory(win);
    /* check in client area */
    return (absx(win)+win->coffx <= x &&
            x <= absx(win)+win->coffx+win->pmaxx-ox-1 &&
            absy(win)+win->coffy <= y &&
            y <= absy(win)+win->coffy+win->pmaxy-oy-1);

}

/*******************************************************************************

Remove hover if left window

Expects the current x,y position of the mouse. If any window has hover active,
but does not contain the mouse in it's client area, the hover mode is cancelled.

*******************************************************************************/

static void remhover(ami_long x, ami_long y)

{

    winptr    win; /* pointer to windows list */
    ami_evtrec ev;  /* local event record */

    win = winlst; /* get the master list */
    while (win) { /* traverse the windows list */

        if (win->hover && !inclient(win, x, y)) {

            /* Window has hover active, but no longer in client area */
            ev.etype = ami_etnohover; /* set no focus event */
            intsendevent(win, &ev); /* send to queue */
            win->hover = FALSE; /* cancel hover */

        }
        win = win->winlst; /* next window */

    }

}

/*******************************************************************************

Find Z order top window from point

Given an X-Y point in the root surface, finds the topmost window containing that
point. If there is no containing window, NULL is returned.

*******************************************************************************/

static winptr fndtop(ami_long x, ami_long y)

{

    winptr win; /* pointer to windows list */
    winptr fp;  /* found window pointer */
    int    z;   /* z order of last found */
    rectangle ra; /* ancestor clip */

    win = winlst; /* get the master list */
    fp = NULL; /* set no window found */
    z = -1; /* set invalid z order */
    while (win) { /* traverse the windows list */

        if (absx(win) <= x && x <= absx(win)+win->pmaxx-1 &&
            absy(win) <= y && y <= absy(win)+win->pmaxy-1 &&
            win->zorder > z &&
            ancclip(win, &ra) && inrect(x, y, &ra)) {

            /* found inclusion where the window shows, Z above previous */
            fp = win; /* set candidate */
            z = win->zorder;

        }
        win = win->winlst; /* next window */

    }

    return (fp); /* exit wth container */

}

/*******************************************************************************

Find the frame part at a screen location

Returns which live frame part of the window the location lies on: the
system bar buttons, the sizing edges, or the sizing corners. The same
geometry the mouse click handler acts on, so the hover feedback marks
exactly what a click would do.

*******************************************************************************/

static frmpart frmhit(winptr win, ami_long x, ami_long y)

{

    ami_long lx = x-absx(win);
    ami_long ly = y-absy(win);

    if (!win->frame) return (fp_none);
    if (win->sysbar && ly == win->size) { /* the system bar row */

        /* the sizing side borders pass through the bar row, and a click
           there sizes, so the highlight follows */
        if (win->size && lx == 0) return (fp_left);
        if (win->size && lx == win->pmaxx-1) return (fp_right);
        if (lx == win->pmaxx-3) return (fp_close);
        if (lx == win->pmaxx-5) return (fp_max);
        if (lx == win->pmaxx-7) return (fp_min);
        return (fp_none);

    }
    if (win->size) { /* the sizing border */

        int t = ly == 0, b = ly == win->pmaxy-1;
        int l = lx == 0, r = lx == win->pmaxx-1;

        if (t && l) return (fp_ulcnr);
        if (t && r) return (fp_urcnr);
        if (b && l) return (fp_blcnr);
        if (b && r) return (fp_brcnr);
        if (t) return (fp_top);
        if (b) return (fp_bottom);
        if (l) return (fp_left);
        if (r) return (fp_right);

    }

    return (fp_none);

}

/*******************************************************************************

Draw a frame part highlighted or normal

Repaints just the given frame part, reversed for the hover highlight or
back to normal. The paint is the ordinary frame draw clipped to the
part's rectangle, and the cells emit only where this window is topmost,
so the highlight never lands on a window lying over the frame.

*******************************************************************************/

static void drwfrmpart(winptr win, frmpart p, int rev)

{

    rectangle r;
    ami_long  x1 = absx(win);
    ami_long  y1 = absy(win);

    if (p == fp_none) return;
    switch (p) {

        case fp_close:  setrect(&r, x1+win->pmaxx-3, y1+win->size,
                                    x1+win->pmaxx-3, y1+win->size); break;
        case fp_max:    setrect(&r, x1+win->pmaxx-5, y1+win->size,
                                    x1+win->pmaxx-5, y1+win->size); break;
        case fp_min:    setrect(&r, x1+win->pmaxx-7, y1+win->size,
                                    x1+win->pmaxx-7, y1+win->size); break;
        case fp_top:    setrect(&r, x1+1, y1, x1+win->pmaxx-2, y1); break;
        case fp_bottom: setrect(&r, x1+1, y1+win->pmaxy-1,
                                    x1+win->pmaxx-2, y1+win->pmaxy-1); break;
        case fp_left:   setrect(&r, x1, y1+1, x1, y1+win->pmaxy-2); break;
        case fp_right:  setrect(&r, x1+win->pmaxx-1, y1+1,
                                    x1+win->pmaxx-1, y1+win->pmaxy-2); break;
        case fp_ulcnr:  setrect(&r, x1, y1, x1, y1); break;
        case fp_urcnr:  setrect(&r, x1+win->pmaxx-1, y1,
                                    x1+win->pmaxx-1, y1); break;
        case fp_blcnr:  setrect(&r, x1, y1+win->pmaxy-1,
                                    x1, y1+win->pmaxy-1); break;
        case fp_brcnr:  setrect(&r, x1+win->pmaxx-1, y1+win->pmaxy-1,
                                    x1+win->pmaxx-1, y1+win->pmaxy-1); break;
        default: return;

    }
    setcurvis(FALSE); /* no cursor while drawing */
    frmhrev = rev; /* reversed or normal */
    hovflt = win; /* only where this window shows */
    drwfrm(win, &r);
    hovflt = NULL;
    frmhrev = FALSE;
    setcur(curfocus? curfocus: win); /* hand the cursor back */

}

/*******************************************************************************

Find Z order window

Finds the given Z order window. Returns that or NULL if not found.

*******************************************************************************/

static winptr fndzorder(int z)

{

    winptr win; /* pointer to windows list */
    winptr fp;  /* found window pointer */

    win = winlst; /* get the master list */
    fp = NULL; /* set no window found */
    while (win) { /* traverse the windows list */

        if (win->zorder == z) { fp = win; win = NULL; }
        else win = win->winlst; /* next window */

    }

    return (fp); /* exit wth container or NULL */

}

/*******************************************************************************

Construct minimum to maximum Z order list

Sorts the window list for Z order, and leaves the list in zmin2max. This picks
from the general window list and does not affect it, so can be rerun at any
time.

*******************************************************************************/

static void makzmin2max(void)

{

    winptr win; /* pointer to windows list */
    int    z;

    zmin2max = NULL; /* clear the target list */
    if (ztop >= 0) { /* Z order is valid */

        for (z = ztop; z >= 0; z--) { /* find each window by Z order */

            /* since we find max to min, the list gets pushed backwards and ends
               up min to max */
            win = fndzorder(z); /* find this Z window */
            win->zmin2max = zmin2max; /* push to list */
            zmin2max = win;

        }

    }

}

/*******************************************************************************

Construct maximum to minimum Z order list

Sorts the window list for Z order, and leaves the list in zmax2min. This picks
from the general window list and does not affect it, so can be rerun at any
time.

*******************************************************************************/

static void makzmax2min(void)

{

    winptr win; /* pointer to windows list */
    int    z;

    zmax2min = NULL; /* clear the target list */
    if (ztop >= 0) { /* Z order is valid */

        for (z = 0; z <= ztop; z++) { /* find each window by Z order */

            /* since we find min to max, the list gets pushed backwards and ends
               up max to min */
            win = fndzorder(z); /* find this Z window */
            /* a gap in the order leaves nothing to place here */
            if (win) {

                win->zmax2min = zmax2min; /* push to list */
                zmax2min = win;

            }

        }

    }
    /* The occlusion masks depend on the Z order, and the restore paths
       honor them, so every change of the order must recalculate them.
       Previously only open, close, move and resize did: a window brought to
       the front by a focus click kept the mask from its old depth, and was
       restored with holes where windows used to be above it. */
    recalcfmask();

}

/*******************************************************************************

Print min 2 max windows list

Prints the contents of the min 2 max list. A diagnostic.

*******************************************************************************/

static void prtmin2maxlst(void)

{

    winptr wp;

    fprintf(stderr, "Min to max windows list\n");
    wp = zmin2max; /* index top of list */
    while (wp) {

        fprintf(stderr, "Window; %lld zorder: %d\n", AMI_LONG_CAST(wp->wid), wp->zorder);
        wp = wp->zmin2max; /* next */

    }
    fprintf(stderr, "\n");
    fflush(stderr);

}

/*******************************************************************************

Print max 2 min windows list

Prints the contents of the max 2 min list. A diagnostic.

*******************************************************************************/

static void prtmax2minlst(void)

{

    winptr wp;

    fprintf(stderr, "Max to min windows list\n");
    wp = zmax2min; /* index top of list */
    while (wp) {

        fprintf(stderr, "Window; %lld zorder: %d\n", AMI_LONG_CAST(wp->wid), wp->zorder);
        wp = wp->zmax2min; /* next */

    }
    fprintf(stderr, "\n");
    fflush(stderr);

}

/*******************************************************************************

Remove window from min 2 max list

Removes the given window from the min to max list.

*******************************************************************************/

static void remmin2max(winptr win)

{

    winptr wp, lp;

    /* remove from min to maxlist */
    if (zmin2max == win) /* is first entry */
        zmin2max = zmin2max->zmin2max; /* gap top list */
    else { /* find in list */

        wp = zmin2max; /* index top of list */
        while (wp != win) { /* traverse */

            lp = wp; /* set last */
            wp = wp->zmin2max; /* go next */
            if (!wp) error("System fault");

        }
        lp->zmin2max = win->zmin2max; /* gap out of list */

    }

}

/*******************************************************************************

Remove window from max 2 min list

Removes the given window from the max 2 min list.

*******************************************************************************/

static void remmax2min(winptr win)

{

    winptr wp, lp;

    /* remove from min to maxlist */
    if (zmax2min == win) /* is first entry */
        zmax2min = zmax2min->zmax2min; /* gap top list */
    else { /* find in list */

        wp = zmax2min; /* index top of list */
        while (wp != win) { /* traverse */

            lp = wp; /* set last */
            wp = wp->zmax2min; /* go next */
            if (!wp) error("System fault");

        }
        lp->zmax2min = win->zmax2min; /* gap out of list */

    }

}

/*******************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

static void intfront(winptr win)

{

    winptr wp, lp;
    int    z;

    remmin2max(win); /* remove from min to maxlist */
    /* reorder list */
    wp = zmin2max; /* index top of list */
    z = 0; /* set z order count */
    lp = NULL; /* set no last */
    while (wp) { /* traverse the list */

        lp = wp; /* set last entry */
        wp->zorder = z++; /* set new order */
        wp = wp->zmin2max; /* next entry */

    }
    win->zmin2max = NULL; /* terminate last entry */
    if (lp) /* if there is a last entry */
        lp->zmin2max = win; /* set as new last */
    else zmin2max = win; /* list is empty, set first */
    win->zorder = ztop; /* set our entry as top Z order */
    makzmax2min(); /* remake the max 2 min list */
    /* Repaint the window in its new place. This was left to the mouse
       click handler, so a front change made through the API reordered the
       lists but left the screen stale. */
    if (win->visible)
        redraw(win, absx(win), absy(win),
                    absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);

}

/*******************************************************************************

Put window to back of the Z order

Puts the indicated window to the back of the Z order. Note that this routine
will never put the window in back of Z order window 0, since that is the root.
That makes the back of the list 1.

*******************************************************************************/

static void intback(winptr win)

{

    winptr wp, lp;
    int    z;

    remmin2max(win); /* remove from min to maxlist */
    /* Put it at the back of the list, behind everything else, and number
       the order from there. The root, if it is on the list, stays behind
       even that: it is the surface the rest sit on.

       The numbering has to run from 0 without a gap, because the max to
       min list is built by looking each order up in turn, and a missing
       one is a null window. This renumbered from 2 and gave the moved
       window the top number, so nothing held 0 or 1 and building that
       list faulted. */
    if (zmin2max && zmin2max->root && zmin2max != win) {

        /* second place, just ahead of the root */
        win->zmin2max = zmin2max->zmin2max;
        zmin2max->zmin2max = win;

    } else { /* first place */

        win->zmin2max = zmin2max;
        zmin2max = win;

    }
    /* number the list from the back, without gaps */
    wp = zmin2max;
    z = 0;
    while (wp) { wp->zorder = z++; wp = wp->zmin2max; }
    ztop = z-1; /* the top of the order is the last one numbered */
    makzmax2min(); /* remake the max 2 min list */
    /* repaint the region from the new order: whatever was under this
       window is now on top of it */
    if (win->visible)
        redraw(zmax2min, absx(win), absy(win),
                         absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);

}

/*******************************************************************************

Display window

Presents a window, and sends it a first paint message. Used to process the
delayed window display function.

*******************************************************************************/

static void winvis(winptr win)

{

   if (!win->visible) { /* not already visible */

        /* first make all parents visible */
        if (win->parwin) winvis(win->parwin);

        /* now display this one */
        win->visible = TRUE; /* set now visible */
        restore(win); /* restore window */
        annredraw(win); /* a follow mode window needs its program */

    }

}

/** ****************************************************************************

Open and present window

Given a windows file id and an (optional) parent window file id opens and
presents the window associated with it. All of the screen buffer data is
cleared, and a single buffer assigned to the window.

*******************************************************************************/

static void opnwin(int fn, int pfn, ami_long wid, int subclient, int root)

{

    int     si;   /* index for current display screen */
    winptr  win;  /* window pointer */
    winptr  pwin; /* parent window pointer */
    int     t;
    int     ti;
    int     i;

    win = lfn2win(fn); /* get a pointer to the window */
    win->root = root; /* set root window status */
    /* find parent */
    win->parlfn = pfn; /* set parent logical number */
    win->wid = wid; /* set window id */
    pwin = NULL; /* set no parent */
    if (pfn >= 0) pwin = lfn2win(pfn); /* index parent window */
    /* The window list is a tree: every window but the root has a parent.
       A window opened with no parent given hangs off the root, and its
       origin is relative to the root as any child's is to its parent. */
    if (!pwin && !root) {

        pwin = winlst;
        while (pwin && !pwin->root) pwin = pwin->winlst;

    }
    win->parwin = pwin; /* copy link to windows structure */
    win->childwin = NULL; /* clear the child window list */
    win->childlst = NULL; /* clear child member list pointer */
    /* push window to master list */
    win->winlst = winlst;
    winlst = win;
    /* push window to root list */
    win->rootlst = rootlst;
    rootlst = win;
    if (pwin) { /* we have a parent, enter this child to the parent list */

        win->childlst = pwin->childwin; /* push to parent's child list */
        pwin->childwin = win;

    }
    ztop++; /* increase Z ordering */
    if (!opnwig) { /* widget faces never take the keyboard */

        remfocus(); /* remove any existing focus */
        win->focus = TRUE; /* last window in gets focus */
        curfocus = win;

    } else win->focus = FALSE;
    win->hover = FALSE; /* set no hover */
    win->redrawpend = FALSE; /* no redraw announcement pending */
    win->maxed = FALSE; /* window presents at its set size */
    win->mined = FALSE;
    win->zorder = ztop; /* set Z order for this window */
    makzmin2max(); /* (re)create the Z min to max list */
    makzmax2min(); /* (re)create the Z max to min list */
    win->inpptr = -1; /* set buffer empty */
    win->inpbuf[0] = 0;
    win->bufmod = TRUE; /* set buffering on */
    win->metlst = NULL; /* clear menu tracking list */
    win->menu = NULL; /* set menu bar not active */
    if (root) { /* set up root without frame */

        win->frame = FALSE; /* set frame off */
        win->size = FALSE; /* set size bars off */
        win->sysbar = FALSE; /* set system bar off */

    } else {

        win->frame = TRUE; /* set frame on */
        win->size = TRUE; /* set size bars on */
        win->sysbar = TRUE; /* set system bar on */

    }

    /* set up global buffer parameters */
    win->pmaxx = dimx; /* character max dimensions */
    win->pmaxy = dimy;
    win->maxx = win->pmaxx; /* copy to client dimensions */
    win->maxy = win->pmaxy;
    /* subtract frame from client if enabled */
    win->maxx -= decorx(win);
    win->maxy -= decory(win);
    win->cmaxx = win->maxx; /* set client size to track buffer */
    win->cmaxy = win->maxy;
    win->mpx = 0; /* set mouse relative position invalid */
    win->mpy = 0;
    /* No attribute. This copied the root emission cache, which is
       whatever attribute the terminal was last left on: a window created
       right after a blinking face drew stored its whole face blinking,
       and a dialog created after a reversed face drew came up reversed
       whole -- black, with a black swatch no color could change. */
    win->attr = 0;
    win->autof = TRUE; /* auto on */
    win->fcolor = ami_black; /*foreground black */
    win->bcolor = ami_white; /* background white */
    win->frmcolor = ami_blue; /* frame color blue */
    win->fixed = FALSE; /* sizing works where the bars show */
    win->curv = TRUE; /* cursor visible */
    win->orgx = 1;  /* set origin to root */
    win->orgy = 1;
    /* set client offset considering framing characteristics */
    win->coffx = 0+(win->frame && win->size);
    win->coffy = 0+(win->frame && win->size)+(win->frame && win->sysbar)*2+
                 (win->mbar != 0);
    win->curx = 1; /* set cursor at home */
    win->cury = 1;
    /* clear tabs and set to 8ths */
    for (t = 1; t <= MAXTAB; t++) win->tab[t-1] = ((t-1)%8 == 0) && (t != 1);
    /* clear timer array */
    for (ti = 0; ti < AMI_MAXTIM; ti++) win->timers[ti] = 0;
    win->frmtim = 0; /* clear frame timer */

    /* clear the screen array */
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    /* get the default screen */
    win->screens[0] = malloc(sizeof(scnrec)*win->maxy*win->maxx);
    if (!win->screens[0]) error("Out of memory");
    win->bufx = win->maxx; /* save size of buffer */
    win->bufy = win->maxy;
    alcfmask(win); /* allocate forward mask */
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set not visible */
    win->title = NULL; /* set no title */
#ifndef __MACH__ /* Mac OS X */
    /* Every window titles as the invoking program until the program sets
       its own, as the graphical window managers present it; only the root
       carried the default before, and a fresh child window showed an
       empty title bar. */
    win->title = malloc(strlen(program_invocation_short_name)+1);
    if (!win->title) error("Out of memory");
    strcpy(win->title, program_invocation_short_name);
#endif

    iniscn(win, win->screens[0]); /* initalize screen buffer */
    restore(win); /* update to screen */
    recalcfmask(); /* recalculate the forward masks */

}

/** ****************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   ami_long wid, int subclient, int root)

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

        /* Haven't already started the main input/output window, so allocate
           and start that. We tolerate multiple opens to the output file. */
        opnfil[ofn]->win = getwin();
        opnwin(ofn, pfn, wid, subclient, root); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid+MAXFIL] >= 0 && xltwin[wid+MAXFIL] != ofn)
        error("Window in use"); /* flag error */
    xltwin[wid+MAXFIL] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

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

static void intopenwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)

{

    int ifn, ofn, pfn; /* file logical handles */

    /* check valid window handle */
    if (!wid || wid < -MAXFIL || wid > MAXFIL) error("Invalid window ID");
    /* check if the window id is already in use */
    if (xltwin[wid+MAXFIL] >= 0) error("Window ID already in use");
    if (parent) {

        txt2win(parent); /* validate parent is a window file */
        pfn = txt2lfn(parent); /* get logical parent */

    } else pfn = -1; /* set no parent */
    ifn = fndfil(*infile); /* find previous open input side */
    if (ifn < 0) { /* no other input file, open new */

        /* open input file */
        *infile = fopen(NULLDEV, "r"); /* open null as read only */
        if (!*infile) error("Can't open file"); /* can't open */
        setvbuf(*infile, NULL, _IONBF, 0); /* turn off buffering */
        ifn = fileno(*infile); /* get logical file no */

    }
    /* open output file */
    *outfile = fopen(NULLDEV, "w");
    ofn = fileno(*outfile); /* get logical file no. */
    if (ofn == -1) error("System consistency error");
    if (!*outfile) error("Can't open file");
    setvbuf(*outfile, NULL, _IONBF, 0); /* turn off buffering */

    /* check either input is unused, or is already an input side of a window */
    if (opnfil[ifn]) /* entry exists */
        if (!opnfil[ifn]->inw || opnfil[ifn]->win)
            error("File in incorrect mode");
    /* check output file is in use for input or output from window */
    if (opnfil[ofn]) /* entry exists */
        if (opnfil[ofn]->inw || opnfil[ofn]->win)
            error("File in use"); /* file in use */
    /* establish all logical files and links, translation tables, and open
       window */
    openio(*infile, *outfile, ifn, ofn, pfn, wid, TRUE, FALSE);

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
            if (fp->win->screens[si]) free(fp->win->screens[si]);
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

static void closewin(int ofn)

{

    int       ifn;  /* input file id */
    int       wid;  /* window id */
    winptr    win;  /* window data structure */
    winptr    pwin; /* parent window */
    winptr    rw;   /* root window */
    winptr    zp;   /* z order renumber pointer */
    ami_long  z;    /* z order count */
    winptr*   lp;   /* list pointer for unlinking */
    wigptr    wg;   /* widget pointer */
    rectangle cr;   /* the area the window occupied */
    int       vis;  /* it was visible */
    ami_evtrec er;   /* PA event record */

    wid = filwin[ofn]; /* get window id */
    ifn = opnfil[ofn]->inl; /* get the input file link */
    win = lfn2win(ofn); /* get a pointer to the window */
    /* Take the window down. Closing the file used to close the file and
       nothing else: the window stayed on the Z order lists and in its
       parent's children, so it kept being drawn, and a program that closed
       a window watched it remain on the screen. */
    setrect(&cr, absx(win), absy(win),
                 absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);
    vis = win->visible;
    /* the widgets it owns go with it */
    while (win->wiglst) {

        wg = win->wiglst;
        win->wiglst = wg->next;
        wg->parent = NULL; /* it is being taken down with the owner */
        if (hovwig == wg) hovwig = NULL; /* the highlight dies with it */
        if (xltwin[wg->win->wid+MAXFIL] >= 0) fclose(wg->wf);
        if (wg->face) free(wg->face);
        if (wg->lcol) free(wg->lcol);
        if (wg->list) {

            ami_long i;
            for (i = 0; i < wg->listn; i++) free(wg->list[i]);
            free(wg->list);

        }
        free(wg);

    }
    if (win->focus) { /* it holds the focus, hand it to the root */

        win->focus = FALSE;
        curfocus = NULL;
        rw = winlst;
        while (rw && !rw->root) rw = rw->winlst;
        if (rw && rw != win) { rw->focus = TRUE; curfocus = rw; }

    }
    if (hovfwin == win) hovfwin = NULL; /* the highlight dies with it */
    remmin2max(win); /* out of the Z order lists */
    remmax2min(win);
    /* Renumber the remaining order from the back, without gaps. The max to
       min list is rebuilt by looking each order from 0 to ztop up, so a
       hole leaves the windows above it off the list, and their own close
       then cannot find them. */
    zp = zmin2max;
    z = 0;
    while (zp) { zp->zorder = z++; zp = zp->zmin2max; }
    ztop = z-1;
    /* out of the parent's children */
    if (win->parwin) {

        lp = &win->parwin->childwin;
        while (*lp && *lp != win) lp = &(*lp)->childlst;
        if (*lp) *lp = win->childlst;

    }
    /* out of the master list */
    lp = &winlst;
    while (*lp && *lp != win) lp = &(*lp)->winlst;
    if (*lp) *lp = win->winlst;
    win->visible = FALSE;
    /* these are the window's own, and clsfil does not know of them */
    if (win->fmask) free(win->fmask);
    if (win->title) free(win->title);
    clsfil(ofn); /* flush and close output file */
    /* if no remaining links exist, flush and close input file */
    if (!inplnk(ifn)) clsfil(ifn);
    filwin[ofn] = -1; /* clear file to window translation */
    xltwin[wid+MAXFIL] = -1; /* clear window to file translation */
    /* clsfil released the screen buffers and returned the record to the
       free list, so only what it does not know about is released here */
    if (win->fmask) { win->fmask = NULL; win->fmasklen = 0; }
    win->title = NULL;
    makzmax2min(); /* remake the order, which redoes the masks */
    /* paint over where it was, from whatever is behind it */
    if (vis) redraw(zmax2min, cr.x1, cr.y1, cr.x2, cr.y2);

}

/** ****************************************************************************

Dump buffer for window

Dumps the indicated window buffer to stderr. A diagnostic.

*******************************************************************************/

static void dumpbuffer(winptr win)

{

    scnptr sc;   /* pointer to current screen */
    scnptr sp;   /* pointer to screen record */
    int    x, y;

    sc = win->screens[win->curupd-1]; /* index current screen */
    fprintf(stderr, "Window: %lld buffer\n", AMI_LONG_CAST(win->wid)); fflush(stderr);
    for (y = 1; y <= win->maxy; y++) { /* lines */

        fprintf(stderr, "\"");
        for (x = 1; x <= win->maxx; x++) { /* characters */

            /* for each new character, we compare the attributes and colors
               with what is set. if a new color or attribute is called for,
               we set that, and update the saves. this technique cuts down on
               the amount of output characters */
            sp = &SCNBUF(sc, x, y);
            fputc(sp->ch, stderr);

        }
        fprintf(stderr, "\"\n"); fflush(stderr);

    }

}

/** ****************************************************************************

Scroll screen

Scrolls the screen by deltas in any given direction. If the scroll would move
all content off the screen, the screen is simply blanked. Otherwise, we find the
section of the screen that would remain after the scroll, determine its source
and destination rectangles, and use a move.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

static void intscroll(winptr win, ami_long x, ami_long y)

{

    ami_long  xi, yi; /* screen counters */
    scnptr    sc;     /* pointer to current screen */
    scnptr    sp;     /* pointer to screen record */
    rectangle cr;     /* client rectangle */

    /* when the scroll is arbitrary, we do it by completely refreshing the
       contents of the screen from the buffer */
    if (x <= -win->maxx || x >= win->maxx || y <= -win->maxy || y >= win->maxy) {

        /* scroll would result in complete clear, do it. Note this clears
           and repaints only this window: the previous code cleared the
           whole root surface, taking every other window with it. */
        iniscn(win, win->screens[win->curupd-1]);   /* clear the screen buffer */
        if (indisp(win)) { /* in display, repaint the client area */

            setrect(&cr, absx(win)+win->coffx, absy(win)+win->coffy,
                       absx(win)+win->coffx+win->cmaxx-1,
                       absy(win)+win->coffy+win->cmaxy-1);
            restoreclp(win, &cr);

        }

    } else { /* scroll */

        /* true scroll is done in two steps. first, the contents of the buffer
           are adjusted to read as after the scroll. then, the contents of the
           buffer are output to the terminal. before the buffer is changed,
           we perform a full save of it, which then represents the "current"
           state of the real terminal. then, the new buffer contents are
           compared to that while being output. this saves work when most of
           the screen is spaces anyways */
        sc = win->screens[win->curupd-1]; /* index current screen */
        if (y > 0) {  /* move text up */

            for (yi = 1; yi < win->maxy; yi++) /* move any lines up */
                if (yi + y <= win->maxy) /* still within buffer */
                    /* move lines up */
                    memcpy(&sc[(yi-1)*win->maxx], &sc[(yi+y-1)*win->maxx],
                           win->maxx*sizeof(scnrec));
            for (yi = win->maxy-y+1; yi <= win->maxy; yi++)
                /* clear blank lines at end */
                for (xi = 1; xi <= win->maxx; xi++) {

                sp = &SCNBUF(sc, xi, yi);
                sp->ch = ' ';   /* clear to blanks at colors and attributes */
                sp->forec = sc->forec;
                sp->backc = sc->backc;
                sp->attr = sc->attr;

            }

        } else if (y < 0) { /* move text down */

            for (yi = win->maxy; yi >= 2; yi--)   /* move any lines up */
                if (yi + y >= 1) /* still within buffer */
                    /* move lines up */
                    memcpy(&sc[(yi-1)*win->maxx], &sc[(yi+y-1)*win->maxx],
                           win->maxx*sizeof(scnrec));
            for (yi = 1; yi <= labs(y); yi++) /* clear blank lines at start */
                for (xi = 1; xi <= win->maxx; xi++) {

                sp = &SCNBUF(sc, xi, yi);
                /* clear to blanks at colors and attributes */
                sp->ch = ' ';
                sp->forec = sc->forec;
                sp->backc = sc->backc;
                sp->attr = sc->attr;

            }

        }
        if (x > 0) { /* move text left */
            for (yi = 1; yi <= win->maxy; yi++) { /* move text left */

                for (xi = 1; xi <= win->maxx-1; xi++) /* move left */
                    if (xi+x <= win->maxx) /* still within buffer */
                        /* move characters left */
                        memcpy(&SCNBUF(sc, xi, yi), &SCNBUF(sc, xi+x, yi),
                               sizeof(scnrec));
                /* clear blank spaces at right */
                for (xi = win->maxx-x+1; xi <= win->maxx; xi++) {

                    sp = &SCNBUF(sc, xi, yi);
                    /* clear to blanks at colors and attributes */
                    sp->ch = ' ';
                    sp->forec = sc->forec;
                    sp->backc = sc->backc;
                    sp->attr = sc->attr;

                }

            }

        } else if (x < 0) { /* move text right */

            for (yi = 1; yi <= win->maxy; yi++) { /* move text right */

                for (xi = win->maxx; xi >= 2; xi--) /* move right */
                    if (xi+x >= 1) /* still within buffer */
                        /* move characters left */
                        memcpy(&SCNBUF(sc, xi, yi), &SCNBUF(sc, xi+x, yi),
                               sizeof(scnrec));
                /* clear blank spaces at left */
                for (xi = 1; xi <= labs(x); xi++) {

                    sp = &SCNBUF(sc, xi, yi);
                    sp->ch = ' ';   /* clear to blanks at colors and attributes */
                    sp->forec = sc->forec;
                    sp->backc = sc->backc;
                    sp->attr = sc->attr;

                }

            }

        }
        if (indisp(win)) { /* in display */

            /* The buffer is adjusted; repaint the client area from it by
               the same clipped path every other repaint uses. The previous
               code here kept a private unclipped paint loop over the whole
               buffer, which sprayed outside the window whenever the buffer
               was larger than the client area, ignored occlusion, and
               diffed against a saved copy of the buffer on the assumption
               that the screen still matched it, which the clipping of other
               windows had long since made false: the visible result was
               window contents that flashed in and then blanked out. */
            setrect(&cr, absx(win)+win->coffx, absy(win)+win->coffy,
                       absx(win)+win->coffx+win->cmaxx-1,
                       absy(win)+win->coffy+win->cmaxy-1);
            restoreclp(win, &cr);

        }

    }

}

/** ****************************************************************************

Set window size character internal

Sets the onscreen window size, in character terms.

*******************************************************************************/

static void intsetsiz(winptr win, ami_long x, ami_long y)

{

    ami_long ox, oy; /* previous size of window */
    rectangle r1, r2, r3, rt, rl, rr, rb;

    if (win->frame && win->size) {

        /* if size bars are on */
        if (x < 2) x = 2; /* set minimum size to preseve size bars */
        if (y < 2) y = 2;

    }
    ox = win->pmaxx; /* save previous size of window */
    oy = win->pmaxy;
    if (x == ox && y == oy) return; /* size is unchanged */
    mclogf("setsiz wid %lld %lldx%lld -> %lldx%lld\n",
                         AMI_LONG_CAST(win->wid), AMI_LONG_CAST(ox), AMI_LONG_CAST(oy), AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    win->pmaxx = x; /* set size */
    win->pmaxy = y;
    win->cmaxx = win->pmaxx; /* copy to client dimensions */
    win->cmaxy = win->pmaxy;
    /* subtract frame from client if enabled */
    win->cmaxx -= decorx(win);
    win->cmaxy -= decory(win);
    if (win->cmaxx < 0) win->cmaxx = 0; /* a bar only window has no client */
    if (win->cmaxy < 0) win->cmaxy = 0;
    /* in follow mode the buffer tracks the client */
    if (!win->bufmod) resizewinbuf(win, win->cmaxx, win->cmaxy);
    /* As in intsetpos, the repaints below consult the masks, so they must
       reflect the new size first. The buffer is left as it is: a buffer
       smaller than the window is what a program gets when it sizes one,
       and the client area beyond it simply shows nothing. */
    recalcfmask();
    /* The resize announcement goes out before the repaints: the repaints
       announce redraws, and the program draws on those -- it must hear
       the new size first, or it draws to the old one and the parts beyond
       the new client are lost. */
    annresize(win);
    if (win->visible) { /* window is onscreen */

        /* check old and new overlap */
        setrect(&r1, absx(win), absy(win), absx(win)+ox-1, absy(win)+oy-1);
        setrect(&r2, absx(win), absy(win),
                             absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);
        if (intersect(&r1, &r2)) { /* intersects */

            /* find rectangle fractions */
            subrect(&r1, &r2, &rt, &rl, &rr, &rb);
            /* process down list for each fraction */
            if (!zerorect(&rt))
                redraw(win->zmax2min, rt.x1, rt.y1, rt.x2, rt.y2);
            if (!zerorect(&rl))
                redraw(win->zmax2min, rl.x1, rl.y1, rl.x2, rl.y2);
            if (!zerorect(&rr))
                redraw(win->zmax2min, rr.x1, rr.y1, rr.x2, rr.y2);
            if (!zerorect(&rb))
                redraw(win->zmax2min, rb.x1, rb.y1, rb.x2, rb.y2);
            /* note we kept the Z order of the repositioned window */
            makzmax2min(); /* remake the max 2 min list */
            /* draw the new window position in */
            redraw(zmax2min, absx(win), absy(win),
                             absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);

        } else {

            remmax2min(win); /* take out of max 2 min list */
            /* draw the current window position out */
            redraw(zmax2min, absx(win), absy(win), absx(win)+ox-1, absy(win)+oy-1);
            /* note we kept the Z order of the repositioned window */
            makzmax2min(); /* remake the max 2 min list */
            /* draw the new window size in */
            redraw(zmax2min, absx(win), absy(win),
                             absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);

        }

    }
    recalcfmask(); /* recalculate the forward masks */
    mbarsiz(win); /* the menu bar follows the client width */

}

/*******************************************************************************

Minimize, maximize and normalize a window

The manager carries these states itself, as the graphical window managers
do: the buttons announced the events but changed nothing, so minimize and
maximize did not function. Maximize fills the parent's client area;
minimize reduces the window to its bar, the decorations alone; normalize
restores the geometry the window had before either. The announcements
(etmax, etmin, etnorm) are made by the callers.

*******************************************************************************/

static void savnorm(winptr win)

{

    if (!win->maxed && !win->mined) { /* the current geometry is the normal */

        win->normx = win->orgx;
        win->normy = win->orgy;
        win->normw = win->pmaxx;
        win->normh = win->pmaxy;

    }

}

static void intmax(winptr win)

{

    winptr par = win->parwin;

    if (!par) return; /* the root is the screen; it has no other size */
    savnorm(win); /* note what to restore to */
    win->maxed = TRUE;
    win->mined = FALSE;
    intsetpos(win, 1, 1); /* fill the parent's client */
    intsetsiz(win, par->cmaxx, par->cmaxy);

}

static void intmin(winptr win)

{

    winptr par = win->parwin;
    winptr c;
    ami_long   h, w, tx, ty;
    int    moved;

    if (!par) return; /* the root does not minimize */
    savnorm(win); /* note what to restore to */
    win->mined = TRUE;
    win->maxed = FALSE;
    /* The window reduces to its title bar alone -- top border, the bar,
       bottom border, no client at all -- and wide enough for the whole
       title, or eight characters, plus the borders and the bar buttons.
       The frame draws its underbar row as the bottom border on a window
       three high. */
    h = (win->frame && win->size)*2+(win->frame && win->sysbar);
    if (h < 2) h = 2; /* observe the minimum */
    w = win->title? (ami_long)strlen(win->title): 0;
    if (w < 8) w = 8; /* the title, or eight characters */
    w += 10; /* borders, buttons, and the title's margin */
    if (w > par->cmaxx) w = par->cmaxx; /* no wider than the parent */
    /* Minimized windows park along the bottom of the parent's client,
       left to right in the order minimized, wrapping back to the left a
       row up when the row is full. The slot search walks the minimized
       siblings and steps past any that overlap the candidate. */
    tx = 1;
    ty = par->cmaxy-h+1;
    do {

        moved = FALSE;
        if (tx+w-1 > par->cmaxx && tx > 1) { /* row is full, wrap */

            tx = 1;
            ty -= h;
            moved = TRUE;

        }
        if (ty < 1) break; /* parent is full of bars; stack at the top */
        for (c = par->childwin; c; c = c->childlst)
            if (c != win && c->mined &&
                c->orgx < tx+w && c->orgx+c->pmaxx > tx &&
                c->orgy < ty+h && c->orgy+c->pmaxy > ty) {

                tx = c->orgx+c->pmaxx; /* step past this bar */
                moved = TRUE;

            }

    } while (moved);
    if (ty < 1) { ty = 1; tx = 1; } /* overfull parent: best effort */
    intsetsiz(win, w, h);
    intsetpos(win, tx, ty);

}

static void intnorm(winptr win)

{

    if (!win->maxed && !win->mined) return; /* already normal */
    win->maxed = FALSE;
    win->mined = FALSE;
    intsetpos(win, win->normx, win->normy);
    intsetsiz(win, win->normw, win->normh);

}

/** ****************************************************************************

Set window position character internal

Sets the onscreen window position, in character terms. Keeps the existing Z
order.

*******************************************************************************/

/* Find the bounding rectangle of a window and its whole subtree, in
   absolute screen terms. Children are placed in their parents but are not
   bound to stay inside them, so the repaint of a moved window must cover
   everything that moved with it. */

static void treerect(winptr win, rectangle* r)

{

    winptr    c;
    rectangle cr;

    setrect(r, absx(win), absy(win),
               absx(win)+win->pmaxx-1, absy(win)+win->pmaxy-1);
    for (c = win->childwin; c; c = c->childlst) {

        treerect(c, &cr);
        if (cr.x1 < r->x1) r->x1 = cr.x1;
        if (cr.y1 < r->y1) r->y1 = cr.y1;
        if (cr.x2 > r->x2) r->x2 = cr.x2;
        if (cr.y2 > r->y2) r->y2 = cr.y2;

    }

}

static void intsetpos(winptr win, ami_long x, ami_long y)

{

    rectangle r1, r2, r3, rt, rl, rr, rb;

    mclogf("setpos wid %lld %lld,%lld -> %lld,%lld\n",
                         AMI_LONG_CAST(win->wid), AMI_LONG_CAST(win->orgx), AMI_LONG_CAST(win->orgy), AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    treerect(win, &r1); /* previous place of the window and its subtree */
    win->orgx = x; /* set position in parent */
    win->orgy = y;
    /* The children, widget faces among them, are placed relative to this
       window, so they move with it by construction; there is no list walk
       to carry them along. */
    treerect(win, &r2); /* the new place of the window and its subtree */
    /* The masks must reflect the new position before anything repaints:
       the repaint of the vacated area consults them, and with the old
       masks it skips exactly the cells this window used to cover, leaving
       its old frame behind on the screen. */
    recalcfmask();
    if (win->visible) { /* window is onscreen */

        /* check old and new overlap */
        if (intersect(&r1, &r2)) { /* intersects */

            /* find rectangle fractions */
            subrect(&r1, &r2, &rt, &rl, &rr, &rb);
            /* process down list for each fraction */
            if (!zerorect(&rt))
                redraw(win->zmax2min, rt.x1, rt.y1, rt.x2, rt.y2);
            if (!zerorect(&rl))
                redraw(win->zmax2min, rl.x1, rl.y1, rl.x2, rl.y2);
            if (!zerorect(&rr))
                redraw(win->zmax2min, rr.x1, rr.y1, rr.x2, rr.y2);
            if (!zerorect(&rb))
                redraw(win->zmax2min, rb.x1, rb.y1, rb.x2, rb.y2);
            /* note we kept the Z order of the repositioned window */
            makzmax2min(); /* remake the max 2 min list */
            /* draw the new place in */
            redraw(zmax2min, r2.x1, r2.y1, r2.x2, r2.y2);

        } else {

            remmax2min(win); /* take out of max 2 min list */
            /* draw the old place out */
            redraw(zmax2min, r1.x1, r1.y1, r1.x2, r1.y2);
            /* note we kept the Z order of the repositioned window */
            makzmax2min(); /* remake the max 2 min list */
            /* draw the new place in */
            redraw(zmax2min, r2.x1, r2.y1, r2.x2, r2.y2);

        }

    }
    recalcfmask(); /* recalculate the forward masks */

}

/** ****************************************************************************

API calls implemented at this level

*******************************************************************************/

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->cury = y; /* set new position */
    win->curx = x;
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

static ami_long imaxx(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->maxx);

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

static ami_long imaxy(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->maxy);

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

static void ihome(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* reset cursors */
    win->curx = 1;
    win->cury = 1;
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor up

Moves the cursor position up one line. If the cursor is at screen top, and auto
is on, the screen is scrolled up, meaning that the screen contents are moved
down a line of text. If auto is off, the cursor can simply continue into
negative space as long as it stays within the bounds -LONG_MAX to LONG_MAX.

*******************************************************************************/

static void iup(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* check not top of screen */
    if (win->cury > 1) win->cury--; /* update position */
    else if (win->autof) intscroll(win, 0, -1); /* scroll up */
    /* check won't overflow */
    else if (win->cury > -LONG_MAX) win->cury--; /* set new position */
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor down

Moves the cursor position down one line. If the cursor is at screen bottom, and
auto is on, the screen is scrolled down, meaning that the screen contents are
moved up a line of text. If auto is off, the cursor can simply continue into
undrawn space as long as it stays within the bounds of -LONG_MAX to LONG_MAX.

*******************************************************************************/

static void idown(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    /* check not bottom of screen */
    if (win->cury < win->maxy) win->cury++; /* update position */
    else if (win->autof) intscroll(win, 0, +1); /* scroll down */
    /* check won't overflow */
    else if (win->cury < LONG_MAX) win->cury++; /* set new position */
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor left

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

static void ileft(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* check not at extreme left */
    if (win->curx > 1) win->curx--; /* update position */
    else { /* wrap cursor motion */

        if (win->autof) { /* autowrap is on */

            iup(f); /* move cursor up one line */
            win->curx = win->maxx; /* set cursor to extreme right */

        } else
            /* check won't overflow */
            if (win->curx > -LONG_MAX) win->curx--; /* update position */

    }
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

static void iright(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* check not at extreme right */
    if (win->curx < win->maxx) win->curx++; /* update position */
    else { /* wrap cursor motion */

        if (win->autof) { /* autowrap is on */

            idown(f); /* move cursor up one line */
            win->curx = 1; /* set cursor to extreme left */

        } else
            /* check won't overflow */
            if (win->curx < LONG_MAX) win->curx++; /* update position */

    }
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

static void idel(FILE* f)

{

    ileft(f); /* back up cursor */
    plcchr(f, ' '); /* blank out */
    ileft(f); /* back up again */

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

Graphical mode does not implement blink mode.

*******************************************************************************/

static void iblink(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sablink); /* turn on */
    else win->attr &= ~BIT(sablink); /* turn off */

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

static void ireverse(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sarev); /* turn on */
    else win->attr &= ~BIT(sarev); /* turn off */

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

static void iunderline(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(saundl); /* turn on */
    else win->attr &= ~BIT(saundl); /* turn off */

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and elevated font.

*******************************************************************************/

static void isuperscript(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sasuper); /* turn on */
    else win->attr &= ~BIT(sasuper); /* turn off */

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and lowered font.

*******************************************************************************/

static void isubscript(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sasubs); /* turn on */
    else win->attr &= ~BIT(sasubs); /* turn off */

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

static void iitalic(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(saital); /* turn on */
    else win->attr &= ~BIT(saital); /* turn off */

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

static void ibold(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sabold); /* turn on */
    else win->attr &= ~BIT(sabold); /* turn off */

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

static void istrikeout(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sastkout); /* turn on */
    else win->attr &= ~BIT(sastkout); /* turn off */

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

static void istandout(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sarev); /* turn on */
    else win->attr &= ~BIT(sarev); /* turn off */

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

static void ifcolor(FILE* f, ami_color c)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->fcolor = c; /* set color */

}

/** ****************************************************************************

Set foreground color RGB

Sets the foreground color from individual r, g, and b values.

*******************************************************************************/

static void ifcolorc(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win = txt2win(f); /* get window from file */
    ami_long r8 = colc8(r), g8 = colc8(g), b8 = colc8(b);

    /* an exact primary stays a primary code; the rest carry exactly */
    if ((r8 == 0 || r8 == 255) && (g8 == 0 || g8 == 255) &&
        (b8 == 0 || b8 == 255))
        win->fcolor = colrgbnum(r, g, b);
    else win->fcolor = RGBENC(r8, g8, b8);

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

static void ibcolor(FILE* f, ami_color c)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->bcolor = c; /* set color */

}

/** ****************************************************************************

Set background color RGB

Sets the background color from individual r, g, and b values.

*******************************************************************************/

static void ibcolorc(FILE* f, ami_long r, ami_long g, ami_long b)

{

    winptr win = txt2win(f); /* get window from file */
    ami_long r8 = colc8(r), g8 = colc8(g), b8 = colc8(b);

    /* an exact primary stays a primary code; the rest carry exactly */
    if ((r8 == 0 || r8 == 255) && (g8 == 0 || g8 == 255) &&
        (b8 == 0 || b8 == 255))
        win->bcolor = colrgbnum(r, g, b);
    else win->bcolor = RGBENC(r8, g8, b8);

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

static void iauto(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->autof = e; /* set auto state */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

static void icurvis(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->curv = e; /* set/reset cursor visibility */
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Scroll screen

Scrolls the window by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

static void iscroll(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    intscroll(win, x, y); /* process scroll */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

static ami_long icurx(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return win->curx; /* return cursor x */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

static ami_long icury(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return win->cury; /* return cursor y */

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

static ami_long icurbnd(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return intcurbnd(win); /* return cursor bound status */

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

static void iselect(FILE* f, ami_long u, ami_long d)

{

    int    ld;   /* last display screen number save */
    winptr win;  /* window record pointer */
    int    i, t;

    win = txt2win(f); /* get window from file */
    if (!win->bufmod) error("Buffered mode not enabled"); /* error */
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error("Invalid screen number"); /* invalid screen number */
    ld = win->curdsp; /* save the current display screen number */
    win->curupd = u; /* set the current update screen */
    if (!win->screens[win->curupd-1]) { /* no screen, create one */

        /* get a new screen context */
        win->screens[win->curupd-1] =
            malloc(sizeof(scnrec)*win->maxy*win->maxx);
        iniscn(win, win->screens[win->curupd-1]); /* initalize that */

    }
    win->curdsp = d; /* set the current display screen */
    if (!win->screens[win->curdsp-1]) { /* no screen, create one */

        /* no current screen, create a new one */
        win->screens[win->curdsp-1] =
            malloc(sizeof(scnrec)*win->maxy*win->maxx);
        iniscn(win, win->screens[win->curdsp-1]); /* initalize that */

    }
    /* if the screen has changed, restore it */
    if (win->curdsp != ld) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        else restore(win);

    }

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

Process a click on the window frame

Handles a button 1 press on a window's frame: the system bar buttons --
close, maximize or restore, minimize or restore -- the title bar drag,
and the sizing edge and corner drags. Called for the click on a focused
window, and for the click that focuses one: a button acts on the same
click that focuses its window, as a widget does; needing a second click
was not usable.

*******************************************************************************/

static void frmclick(winptr win)

{

    ami_evtrec er;


    if (win->sysbar && mousey-absy(win) == win->size &&
        mousex-absx(win) >= 1 &&
        mousex-absx(win) < win->pmaxx-1) {

        /* check for system bar events */
        if (mousex-absx(win) == win->pmaxx-3) {

            /* terminate */
            er.etype = ami_etterm; /* set type */
            intsendevent(win, &er); /* issue event */
            fend = TRUE; /* set end program requested */

        } else if (mousex-absx(win) == win->pmaxx-5) {

            /* maximize, or restore a maximized window; a fixed window
               ignores it, as it ignores all sizing */
            if (!win->fixed) {

                if (win->maxed) {

                    intnorm(win);
                    er.etype = ami_etnorm;

                } else {

                    intmax(win);
                    er.etype = ami_etmax;

                }
                intsendevent(win, &er); /* issue event */

            }

        } else if (mousex-absx(win) == win->pmaxx-7) {

            /* minimize, or restore a minimized window */
            if (win->mined) {

                intnorm(win);
                er.etype = ami_etnorm;

            } else {

                intmin(win);
                er.etype = ami_etmin;

            }
            intsendevent(win, &er); /* issue event */

        } else if (mousex-absx(win) >= 1 &&
                   mousex-absx(win) < win->pmaxx-1) {

            /* system bar click */
            drag = dt_sysbar; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        }

    } else if (win->frame && win->size && !win->fixed) {

        /* frame and sizebars are enabled */
        if (mousey-absy(win) == 0 &&
              mousex-absx(win) == 0) {

            /* top left click */
            drag = dt_ulcnr; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        } else if (mousey-absy(win) == 0 &&
              mousex-absx(win) == win->pmaxx-1) {

            /* top right click */
            drag = dt_urcnr; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        } else if (mousey-absy(win) == win->pmaxy-1 &&
              mousex-absx(win) == 0) {

            /* bottom left click */
            drag = dt_blcnr; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        } else if (mousey-absy(win) == win->pmaxy-1 &&
              mousex-absx(win) == win->pmaxx-1) {

            /* bottom right click */
            drag = dt_brcnr; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        } else if (mousey-absy(win) == 0) {

            /* top bar click */
            drag = dt_top; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        } else if (mousey-absy(win) == win->pmaxy-1) {

            /* bottom bar click */
            drag = dt_bottom; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        } else if (mousex-absx(win) == 0) {

            /* left bar click */
            drag = dt_left; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        } else if (mousex-absx(win) == win->pmaxx-1) {

            /* right bar click */
            drag = dt_right; /* set drag type */
            drgwin = win; /* set up drag pin */
            drgx = mousex;
            drgy = mousey;

        }

    }


}

/** ****************************************************************************

Acquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle always used.

The event loop for X and the event loop for Petit-Ami are similar. Its not a
coincidence. I designed it after a description I read of the X system in 1997.
Our event loop here is like an event to event translation.

*******************************************************************************/

static void intevent(FILE* f)

{

    ami_evtrec ev, er;    /* local event record */
    winptr    win;   /* windows record pointer */
    ami_long  x, y;

    win = NULL; /* set no window active */
    (*event_vect)(stdin, &ev); /* get root event */
#ifdef PRTROOTEVT
        fprintf(stderr, "Inbound: "); prtevt(&ev); fprintf(stderr, "\n"); fflush(stderr);
#endif
    switch (ev.etype) { /* process root events */

        case ami_etchar: /* input character ready */

            win = curfocus; /* get focus window (if any) */
            if (win) { /* found focus window */

                er.etype = ami_etchar; /* place character code */
                er.echar = ev.echar; /* place character */
                er.winid = win->wid; /* send keys to focus window */
                intsendevent(win, &er); /* issue event */

            }
            break;
        /* The terminal surface gained or lost the host focus, or the mouse
           entered or left it. These describe the surface as a whole, so
           they go to the window that owns the manager's focus, and to the
           root when nothing else holds it. They were dropped entirely,
           which left a program running under the manager seeing neither
           focus nor hover. */
        case ami_etfocus:
        case ami_etnofocus:

            win = curfocus;
            if (!win) { /* nothing focused, find the root */

                win = winlst;
                while (win && !win->root) win = win->winlst;

            }
            if (win) {

                er.etype = ev.etype;
                er.winid = win->wid;
                intsendevent(win, &er);

            }
            break;

        case ami_ethover:
        case ami_etnohover:

            /* Hover follows the mouse rather than the focus: it belongs to
               the window under the pointer. The manager tracks hover for
               its own windows on mouse movement, so the surface level
               event is only passed on when the pointer is over the root
               itself, which is the case a lone program sees. */
            win = fndtop(mousex, mousey);
            if (!win) { /* off any window, use the root */

                win = winlst;
                while (win && !win->root) win = win->winlst;

            }
            /* Only pass it on when the state actually changes. The
               manager raises hover itself when the pointer moves into a
               window, so forwarding unconditionally reported it twice. */
            if (win && !!win->hover != (ev.etype == ami_ethover)) {

                er.etype = ev.etype;
                er.winid = win->wid;
                intsendevent(win, &er);
                win->hover = ev.etype == ami_ethover;

            }
            break;

        case ami_etresize: /* the terminal surface changed size */

            /* Follow the new terminal dimensions. Without this the manager
               kept the size found at startup: after a terminal resize the
               clip to the old root bounds was wrong, and the underlying
               terminal package had already repainted its raw screen over
               the composition, so nothing came back until some other
               action forced a redraw. */
            dimx = ev.rszx; /* set new root dimensions */
            dimy = ev.rszy;
            win = winlst; /* find the root window record */
            while (win && !win->root) win = win->winlst;
            if (win) { /* track the surface in the root window */

                win->pmaxx = dimx; /* the root window is the surface */
                win->pmaxy = dimy;
                /* The client is what the decorations leave. The root
                   window is frameless, but a menu bar still takes its
                   first row, and a client told it has that row draws its
                   last line where nothing is shown: a program with a
                   strip along its foot lost the strip on every terminal
                   resize, and did not get it back by resizing again. */
                win->cmaxx = dimx-decorx(win);
                win->cmaxy = dimy-decory(win);
                /* In follow mode the buffer is the window, so it tracks
                   the new surface. In buffered mode the program chose the
                   buffer size and it keeps it: the window simply shows
                   more or less of it. Growing it here regardless threw
                   away the size a program had asked for. */
                if (!win->bufmod) growwinbuf(win, win->cmaxx, win->cmaxy);
                /* The layer below keeps its own screen buffers, sized when
                   it started, and clips writes to them. Grow those through
                   the standard call rather than by reaching into that
                   module: the manager runs over any terminal implementation
                   and must not require changes in it. The contents are
                   discarded by that call, which costs nothing here since
                   the whole surface is repainted below. */
                (*sizbuf_vect)(stdout, dimx, dimy);

            }
            recalcfmask(); /* occlusion clips to the new bounds */
            if (win) mbarsiz(win); /* the root menu bar follows the width */
            if (win) { /* tell the client its surface changed */

                /* Before the repaint: the repaint announces redraws, and
                   the client draws on those to the size it last heard. */
                er.etype = ami_etresize;
                er.rszx = win->cmaxx; /* the client's size, as everywhere */
                er.rszy = win->cmaxy;
                er.winid = win->wid;
                intsendevent(win, &er);

            }
            /* repaint the whole composition over the terminal's own
               recovery paint */
            redraw(zmax2min, 1, 1, dimx, dimy);
            break;
        case ami_etmouba:  /* mouse button assertion */
            win = fndtop(mousex, mousey); /* find the enclosing window */
            menselmouse(win); /* a click ends the keyboard's turn at a menu */
            /* a click outside the open popups dismisses them */
            if (popcnt) {

                int inpop = FALSE;
                int pi;

                for (pi = 0; pi < popcnt; pi++)
                    if (win == popstk[pi]->win) inpop = TRUE;
                if (win && win->widget && win->wig->typ == wtmenubar)
                    inpop = TRUE; /* the bar manages its own popups */
                if (!inpop) clspops(0); /* close them all */

            }
            /* first click with no focus gives focus, next click gives message */
            if (win) {

                /* A widget acts on the same click that focuses it: the
                   click-consuming focus rule below is window etiquette,
                   and making a scroll bar or button need two clicks is
                   not. The focus change still happens in the unfocused
                   branch; the action is delivered here either way. */
                if (win->widget && inclient(win, mousex, mousey)) {

                    winptr wp;

                    er.etype = ami_etmouba;
                    er.amoun = ev.amoun;
                    er.amoubn = ev.amoubn;
                    intsendevent(win, &er); /* to the widget logic */
                    /* The action may have taken the window down: a popup
                       selection closes the popup, which frees its window
                       record. Touch it no further unless it is still on
                       the window list; the focus etiquette below was
                       marking the freed record focused and asking the
                       closed window to redraw. */
                    wp = winlst;
                    while (wp && wp != win) wp = wp->winlst;
                    if (!wp) break;

                }
                if (win->focus) { /* window has focus */

                    if (inclient(win, mousex, mousey)) {

                        if (!win->widget) { /* widgets were served above */

                            /* in client area */
                            er.etype = ami_etmouba; /* set mouse button asserts */
                            er.amoun = ev.amoun; /* set mouse number */
                            er.amoubn = ev.amoubn; /* set button number */
                            er.winid = win->wid; /* set window logical id */
                            intsendevent(win, &er); /* issue event */

                        }

                    } else if (ev.mmoun == 1) frmclick(win);


                } else if (ev.mmoun == 1 &&
                           !(win->widget &&
                             (win->wig->typ == wtmenubar ||
                              win->wig->typ == wtpopup))) { /* button 1 click */

                    /* Menu controls are excluded above: a click on the
                       menu bar or an open menu acts on the menu, and the
                       keyboard focus stays with the window it was in. It
                       was following the menu, and the cursor with it,
                       parked at the end of the menu bar. */
                    remfocus(); /* remove previous focus */
                    /* Take the focus before announcing it. A widget
                       redraws its face when it hears this and reads the
                       flag to decide how to look, so announcing first
                       drew it as though it did not have the focus. */
                    win->focus = TRUE; /* set current focus */
                    curfocus = win;
                    /* send focus message */
                    er.etype = ami_etfocus; /* set focus event */
                    intsendevent(win, &er); /* send to queue */
                    /* The cursor belongs to the focused window, so it
                       moves to this one. Without this the cursor stayed
                       in whatever window last held the focus, and the
                       test that asks whether the cursor follows the
                       selection failed. */
                    setcur(win);
                    if (inclient(win, mousex, mousey)) {

                        /* window went into focus, can have hover now */
                        if (!win->hover) { /* enter hover mode */

                            er.etype = ami_ethover; /* set enter hover */
                            intsendevent(win, &er); /* issue event */
                            win->hover = TRUE; /* set hover active */

                        }
                        /* The click that focuses a client also acts on
                           it. Widgets have acted on their focusing click
                           all along, and a frame click acts on the same
                           click that focuses -- the client area was the
                           one place the click was swallowed, and a
                           program that opens a message on a click needed
                           two where the graphical library needs one.
                           The position goes first, since a client that
                           tracks the mouse from move events has been
                           told nothing while it was unfocused: without
                           it the click lands on coordinates from
                           whenever the window last had the focus. */
                        if (!win->widget) {

                            er.etype = ami_etmoumov;
                            er.mmoun = ev.mmoun;
                            er.moupx = mousex-(absx(win)+win->coffx)+1;
                            er.moupy = mousey-(absy(win)+win->coffy)+1;
                            intsendevent(win, &er);
                            er.etype = ami_etmouba;
                            er.amoun = ev.amoun;
                            er.amoubn = ev.amoubn; /* the button that was
                                pressed, which may be the wheel: calling
                                everything button 1 turned a wheel notch
                                over an unfocused window into a click */
                            intsendevent(win, &er);

                        }

                    }
                    if (!win->root) { /* the root stays put */

                        winptr fw = win->widget? win->wig->parent: win;
                        wigptr wg;

                        /* widgets on the root need no fronting: the root
                           is always at the bottom and its widgets above */
                        if (fw->zorder != ztop && !fw->root) {

                            int pi;

                            /* Bring the family to the front: the window,
                               then its subtree over it -- widget faces
                               and child windows are all children in the
                               window tree. Fronting the window alone
                               raised it over its own children. */
                            fronttree(fw);
                            /* Popups float over everything, and the click
                               being served may just have opened one: a
                               drop box opened its list, then the fronting
                               here buried it under the family, and the
                               box needed a second click to show it. */
                            for (pi = 0; pi < popcnt; pi++)
                                fronttree(popstk[pi]->win);

                        }

                    }
                    /* A frame click acts on the same click that focuses
                       the window: the bar buttons and the drags work on
                       the first click, as a widget does. Client clicks
                       stay consumed by the focus, as window etiquette. */
                    if (!win->widget && !inclient(win, mousex, mousey))
                        frmclick(win);

                }

            }
            break;
        case ami_etmoubd:  /* mouse button deassertion */
            win = fndtop(mousex, mousey); /* find the enclosing window */
            if (win && win->focus && inclient(win, mousex, mousey)) {

                er.etype = ami_etmoubd; /* set mouse button deasserts */
                er.dmoun = ev.dmoun; /* set mouse number */
                er.dmoubn = ev.dmoubn; /* set button number */
                er.winid = win->wid; /* set window logical id */
                intsendevent(win, &er); /* send to queue */

            }
            /* cancel any drag */
            drag = dt_none;
            drgwig = NULL;
            break;
        case ami_etmoumov: /* mouse move */
            mousex = ev.moupx; /* set current mouse position */
            mousey = ev.moupy;
            remhover(mousex, mousey); /* remove hover if left a window */
            win = fndtop(mousex, mousey); /* see if in a window */
            if (win && win->focus) { /* in window and in focus */

                /* check in client area */
                if (inclient(win, mousex, mousey)) {

                    er.etype = ami_etmoumov; /* set mouse move event */
                    er.mmoun = ev.mmoun; /* set mouse number */
                    /* calculate relative location in client area */
                    er.moupx = mousex-(absx(win)+win->coffx)+1;
                    er.moupy = mousey-(absy(win)+win->coffy)+1;
                    win->mpx = er.moupx; /* copy to window data */
                    win->mpy = er.moupy;
                    er.winid = win->wid; /* set window logical id */
                    intsendevent(win, &er); /* issue event */
                    if (!win->hover) { /* enter hover mode */

                        er.etype = ami_ethover; /* set enter hover */
                        intsendevent(win, &er); /* issue event */
                        win->hover = TRUE; /* set hover active */

                    }

                }

            }
            /* Frame hover feedback: the frame part under the mouse
               shows reversed, so the sizing edges and the bar buttons
               show they are live. Held off while a drag runs; the drag
               repaints make it stale and the next move renews it. */
            if (drag == dt_none && !drgwig) {

                winptr  hw = fndtop(mousex, mousey);
                frmpart hp = fp_none;
                wigptr  ww = NULL;
                ami_long    wp = 0;

                if (hw && hw->widget) { /* the live widget part reverses */

                    ww = hw->wig;
                    if (ww->enb)
                        wp = wighit(ww, mousex-absx(hw)+1,
                                        mousey-absy(hw)+1);
                    if (!wp) ww = NULL;

                } else if (hw && !hw->root && hw->frame) {

                    hp = frmhit(hw, mousex, mousey);
                    /* a fixed window ignores sizing: its size bars and
                       maximize button are not live, so no highlight */
                    if (hw->fixed && (hp >= fp_top || hp == fp_max))
                        hp = fp_none;

                }
                if (hp == fp_none) hw = NULL;
                if (hw != hovfwin || hp != hovfpart) {

                    if (hovfwin) drwfrmpart(hovfwin, hovfpart, FALSE);
                    hovfwin = hw; /* note the new holder */
                    hovfpart = hp;
                    if (hovfwin) drwfrmpart(hovfwin, hovfpart, TRUE);

                }
                if (ww != hovwig || wp != hovwprt) {

                    wigptr ow = hovwig;

                    hovwig = ww; /* the face draw reads these */
                    hovwprt = wp;
                    if (ow) wigdrw(ow);
                    if (ww) wigdrw(ww);

                }

            }
            /* a value widget being dragged follows the mouse */
            if (drgwig) wigdrag();
            /* check drag is active and mouse has moved */
            if (drag != dt_none && (drgx != mousex || drgy != mousey)) {

                /* find drag distance */
                x = mousex-drgx;
                y = mousey-drgy;
                /* move the window */
                switch (drag) {

                    case dt_none:   /* no drag active */
                        break;
                    case dt_sysbar: /* sysbar drag (whole window) */
                        /* The origin is parent relative and intsetpos
                           takes it so; the mouse delta is the same in
                           screen and parent space. Applying the delta to
                           the absolute position handed intsetpos the
                           parent offset over again, and a child of a
                           child ran away by that much per step. */
                        intsetpos(drgwin, drgwin->orgx+x, drgwin->orgy+y);
                        break;
                    case dt_ulcnr:  /* upper left corner */
                        intsetpos(drgwin, drgwin->orgx+x, drgwin->orgy+y);
                        intsetsiz(drgwin, drgwin->pmaxx-x, drgwin->pmaxy-y);
                        break;
                    case dt_urcnr:  /* upper right corner */
                        intsetpos(drgwin, drgwin->orgx, drgwin->orgy+y);
                        intsetsiz(drgwin, drgwin->pmaxx+x, drgwin->pmaxy-y);
                        break;
                    case dt_blcnr:  /* bottom left corner */
                        intsetpos(drgwin, drgwin->orgx+x, drgwin->orgy);
                        intsetsiz(drgwin, drgwin->pmaxx-x, drgwin->pmaxy+y);
                        break;
                    case dt_brcnr:  /* bottom right corner */
                        intsetsiz(drgwin, drgwin->pmaxx+x, drgwin->pmaxy+y);
                        break;
                    case dt_top:    /* top frame bar */
                        intsetpos(drgwin, drgwin->orgx, drgwin->orgy+y);
                        intsetsiz(drgwin, drgwin->pmaxx, drgwin->pmaxy-y);
                        break;
                    case dt_left:   /* left frame bar */
                        intsetpos(drgwin, drgwin->orgx+x, drgwin->orgy);
                        intsetsiz(drgwin, drgwin->pmaxx-x, drgwin->pmaxy);
                        break;
                    case dt_right:  /* right frame bar */
                        intsetsiz(drgwin, drgwin->pmaxx+x, drgwin->pmaxy);
                        break;
                    case dt_bottom:  /* bottom frame bar */
                        intsetsiz(drgwin, drgwin->pmaxx, drgwin->pmaxy+y);
                        break;

                }
                drgx = mousex; /* reset drag position */
                drgy = mousey;

            }
            break;
        case ami_etup:      /* cursor up one line */
        case ami_etdown:    /* down one line */
        case ami_etleft:    /* left one character */
        case ami_etright:   /* right one character */
        case ami_etleftw:   /* left one word */
        case ami_etrightw:  /* right one word */
        case ami_ethome:    /* home of document */
        case ami_ethomes:   /* home of screen */
        case ami_ethomel:   /* home of line */
        case ami_etend:     /* end of document */
        case ami_etends:    /* end of screen */
        case ami_etendl:    /* end of line */
        case ami_etscrl:    /* scroll left one character */
        case ami_etscrr:    /* scroll right one character */
        case ami_etscru:    /* scroll up one line */
        case ami_etscrd:    /* scroll down one line */
        case ami_etpagd:    /* page down */
        case ami_etpagu:    /* page up */
        case ami_ettab:     /* tab */
        case ami_etenter:   /* enter line */
        case ami_etinsert:  /* insert block */
        case ami_etinsertl: /* insert line */
        case ami_etinsertt: /* insert toggle */
        case ami_etdel:     /* delete block */
        case ami_etdell:    /* delete line */
        case ami_etdelcf:   /* delete character forward */
        case ami_etdelcb:   /* delete character backward */
        case ami_etcopy:    /* copy block */
        case ami_etcopyl:   /* copy line */
        case ami_etcan:     /* cancel current operation */
        case ami_etstop:    /* stop current operation */
        case ami_etcont:    /* continue current operation */
        case ami_etprint:   /* print document */
        case ami_etprintb:  /* print block */
        case ami_etprints:  /* print screen */
        case ami_etfun:     /* function key */
        case ami_etmenu:    /* display menu */
            /* the keyboard at a menu takes its keys, and the menu key
               puts it there: see menu select mode below */
            if (menselkey(ev.etype)) break;
            if (ev.etype == ami_etmenu && menselstart()) break;
            win = curfocus; /* get the focus window (if any) */
            if (win) {

                er.etype = ev.etype; /* set key type */
                er.winid = win->wid; /* set window logical id */
                intsendevent(win, &er); /* issue event */

            }
            break;
        case ami_ettim:     /* timer matures */
            if (timtbl[ev.timnum-1]) { /* there is a window assigned */

                win = timtbl[ev.timnum-1]; /* get the assigned window */
                 /* check framing/normal timer */
                if (win->frmtim == ev.timnum) er.etype = ami_etframe;
                else {

                    er.etype = ami_ettim; /* set type */
                    er.timnum = timids[ev.timnum-1]; /* set id of timer */

                }
                er.winid = win->wid; /* set window logical id */
                intsendevent(win, &er); /* issue event */

            }
            break;
        case ami_etjoyba:  /* joystick button assertion */
                er.etype = ami_etjoyba; /* set joystick button asserts */
                er.ajoyn = ev.ajoyn; /* set joystick number */
                er.ajoybn = ev.ajoybn; /* set button number */
                er.winid = 0; /* set window logical id (anonymous) */
                intsendevent(win, &er); /* issue event */
            break;
        case ami_etjoybd:  /* joystick button deassertion */
                er.etype = ami_etjoybd; /* set joystick button deasserts */
                er.djoyn = ev.djoyn; /* set joystick number */
                er.djoybn = ev.djoybn; /* set button number */
                er.winid = 0; /* set window logical id (anonymous) */
                intsendevent(win, &er); /* issue event */
            break;
        case ami_etjoymov: /* joystick move */
                er.etype = ami_etjoymov; /* set joystick move */
                er.mjoyn = ev.mjoyn; /* set joystick number */
                er.joypx = ev.joypx; /* set motion axies */
                er.joypy = ev.joypy;
                er.joypz = ev.joypz;
                er.joyp4 = ev.joyp4;
                er.joyp5 = ev.joyp5;
                er.joyp6 = ev.joyp6;
                er.winid = 0; /* set window logical id (anonymous) */
                intsendevent(win, &er); /* issue event */
            break;
        case ami_etterm: /* terminate */
            er.etype = ami_etterm; /* set type */
            intsendevent(win, &er); /* issue event */
            fend = TRUE; /* set end program requested */
            break;
        default: ; /* ignore the rest */

    }

}

static void ievent(FILE* f, ami_evtrec* er)

{

    do { /* loop handling via event vectors and queuing */

        /* check input PA queue */
        while (!paqevt) intevent(f); /* get next event */
        dequepaevt(er); /* get next queued event */
#ifdef PRTEVT
        fprintf(stderr, "Outbound: "); prtevt(er); fprintf(stderr, "\n"); fflush(stderr);
#endif
        er->handled = 1; /* set event is handled by default */
        (evtshan)(er); /* call master event handler */
        if (!er->handled && er->etype <= ami_etmenus) { /* send it to fanout */

            er->handled = 1; /* set event is handled by default */
            (*evthan[er->etype])(er); /* call event handler first */

        }

    } while (er->handled);
    /* event not handled, return it to the caller */

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

static void ieventover(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh)

{

    if (e > ami_etmenus) error("Cannot vector auxillary event");
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

static void ieventsover(ami_pevthan eh,  ami_pevthan* oeh)

{

    *oeh = evtshan; /* save existing event handler */
    evtshan = eh; /* place new event handler */

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

static void isendevent(FILE* f, ami_evtrec* er)

{

    winptr win;   /* pointer to windows context */
    int fn;       /* logical file number */

    fn = fileno(f); /* find find number */
    if (fn < 0) error("Invalid file");
    if (opnfil[fn]->inl < 0) error("No input side for window");
    win = lfn2win(fn); /* index window for file */
    intsendevent(win, er); /* send it */

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into the
input line buffer.

*******************************************************************************/

static void readline(int fd)

{

    ami_evtrec er;   /* event record */
    winptr    win;  /* window pointer */
    int       ins;  /* insert/overwrite mode */
    ami_long  xoff; /* x starting line offset */
    ami_long  l;    /* buffer length */
    int       ofn;  /* logical output file */
    int       lcmp; /* line complete */
    int       i;
    FILE*     f;

    lcmp = FALSE; /* set line not complete */
    do { /* get line characters */

        /* The file plays no part in fetching an event: the root is read
           through stdin whatever is passed. It was passed f here, which
           on the first turn of this loop had never been set. */
        ievent(stdin, &er); /* get next event */
        ofn = xltwin[er.winid+MAXFIL]; /* get logical output file */
        if (ofn >= 0 && opnfil[ofn]->inl == fd) {

            /* output file indexes our input file */
            f = opnfil[ofn]->sfp; /* get window file, echoes land there */
            win = lwn2win(er.winid); /* get the window from the id */
            if (win->inpptr < 0) { /* buffer is flagged empty */

                win->inpptr = 0; /* reset input */
                win->inpbuf[win->inpptr] = 0; /* and terminate buffer */
                ins = 1;
                xoff = win->curx; /* save starting line offset */

            }
            switch (er.etype) { /* event */

                case ami_etterm: exit(1); /* halt program */
                case ami_etenter: /* line terminate */
                    while (win->inpbuf[win->inpptr])
                        win->inpptr++; /* advance to end */
                    win->inpbuf[win->inpptr] = '\n'; /* return newline */
                    /* terminate the line */
                    win->inpbuf[win->inpptr+1] = 0;
                    plcchr(f, '\r'); /* output newline sequence */
                    plcchr(f, '\n');
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
                                plcchr(f, win->inpbuf[i++]);
                            /* back up */
                            i = win->inpptr;
                            while (win->inpbuf[i++]) plcchr(f, '\b');
                            /* forward and next char */
                            plcchr(f, win->inpbuf[win->inpptr]);
                            win->inpptr++;

                        } else { /* overwrite */

                            /* if end, move end marker */
                            if (!win->inpbuf[win->inpptr]) win->inpbuf[win->inpptr+1] = 0;
                            win->inpbuf[win->inpptr] = er.echar; /* place new character */
                            /* forward and next char */
                            plcchr(f, win->inpbuf[win->inpptr]);
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
                        plcchr(f, '\b'); /* move cursor back */
                        /* repaint line */
                        i = win->inpptr;
                        while (win->inpbuf[i]) plcchr(f, win->inpbuf[i++]);
                        plcchr(f, ' '); /* blank last */
                        /* back up */
                        plcchr(f, '\b');
                        i = win->inpptr;
                        while (win->inpbuf[i++]) plcchr(f, '\b');

                    }
                    break;
                case ami_etdelcf: /* delete character forward */
                    if (win->inpbuf[win->inpptr]) { /* not at extreme right */

                        /* move characters down */
                        i = win->inpptr;
                        while (win->inpbuf[i]) { win->inpbuf[i] = win->inpbuf[i+1]; i++; }
                        /* repaint right */
                        i = win->inpptr;
                        while (win->inpbuf[i]) plcchr(f, win->inpbuf[i++]);
                        plcchr(f, ' '); /* blank last */
                        /* back up */
                        plcchr(f, '\b');
                        i = win->inpptr;
                        while (win->inpbuf[i++]) plcchr(f, '\b');

                    }
                    break;
                case ami_etright: /* right character */
                    /* not at extreme right, go right */
                    if (win->inpbuf[win->inpptr]) {

                        plcchr(f, win->inpbuf[win->inpptr]);
                        win->inpptr++; /* advance input */

                    }
                    break;

                case ami_etleft: /* left character */
                    /* not at extreme left, go left */
                    if (win->inpptr > 0) {

                        plcchr(f, '\b');
                        win->inpptr--; /* back up pointer */

                    }
                    break;

                case ami_etmoumov: /* mouse moved */
                    /* we can track this internally */
                    break;

                case ami_etmouba: /* mouse click */
                    if (er.amoubn == 1) {

                        l = strlen(win->inpbuf);
                        if (win->cury == win->mpy && xoff <= win->mpx && xoff+l >= win->mpx) {

                            /* mouse position is within buffer space, set
                               position */
                            icursor(f, win->mpx, win->cury);
                            win->inpptr = win->mpx-xoff;

                        }

                    }
                    break;

                case ami_ethomel: /* beginning of line */
                    /* back up to start of line */
                    while (win->inpptr) {

                        plcchr(f, '\b');
                        win->inpptr--;

                    }
                    break;

                case ami_etendl: /* end of line */
                    /* go to end of line */
                    while (win->inpbuf[win->inpptr]) {

                        plcchr(f, win->inpbuf[win->inpptr]);
                        win->inpptr++;

                    }
                    break;

                case ami_etinsertt: /* toggle insert mode */
                    ins = !ins; /* toggle insert mode */
                    break;

                case ami_etdell: /* delete whole line */
                    /* back up to start of line */
                    while (win->inpptr) {

                        plcchr(f, '\b');
                        win->inpptr--;

                    }
                    /* erase line on screen */
                    while (win->inpbuf[win->inpptr]) {

                        plcchr(f, ' ');
                        win->inpptr++;

                    }
                    /* back up again */
                    while (win->inpptr) {

                        plcchr(f, '\b');
                        win->inpptr--;

                    }
                    win->inpbuf[win->inpptr] = 0; /* clear line */
                    break;

                case ami_etleftw: /* left word */
                    /* back over any spaces */
                    while (win->inpptr && win->inpbuf[win->inpptr-1] == ' ') {

                        plcchr(f, '\b');
                        win->inpptr--;

                    }
                    /* now back over any non-space */
                    while (win->inpptr && win->inpbuf[win->inpptr-1] != ' ') {

                        plcchr(f, '\b');
                        win->inpptr--;

                    }
                    break;

                case ami_etrightw: /* right word */
                    /* advance over any non-space */
                    while (win->inpbuf[win->inpptr] && win->inpbuf[win->inpptr] != ' ') {

                        plcchr(f, win->inpbuf[win->inpptr]);
                        win->inpptr++;

                    }
                    /* advance over any spaces */
                    while (win->inpbuf[win->inpptr] && win->inpbuf[win->inpptr] == ' ') {

                        plcchr(f, win->inpbuf[win->inpptr]);
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

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.

Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

*******************************************************************************/

static void itimer(FILE* f, ami_long i, ami_long t, ami_long r)

{

    winptr win; /* windows record pointer */
    int    ti;

    if (i < 1 || i > AMI_MAXTIM) error("Invalid timer handle");
    win = txt2win(f); /* get window from file */
    if (!win->timers[i-1]) { /* no current timer assigned */

        ti = 0;
        while (ti < AMI_MAXTIM && timtbl[ti]) ti++;
        if (ti >= AMI_MAXTIM) error("Root timers are full");
        timtbl[ti] = win; /* place owner link */
        timids[ti] = i; /* place timer logical id */
        win->timers[i-1] = ti+1; /* place root id */

    }
    /* pass it down */
    (*timer_vect)(stdout, win->timers[i-1], t, r);

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

static void ikilltimer(FILE* f, ami_long i)

{

    winptr win; /* windows record pointer */

    if (i < 1 || i > AMI_MAXTIM) error("Invalid timer handle");
    win = txt2win(f); /* get window from file */
    if (!win->timers[i-1]) error("No such timer");
    /* pass it down */
    (*killtimer_vect)(stdout, win->timers[i-1]);
    /* release the root timer so that we can reuse it */
    timtbl[win->timers[i-1]-1] = NULL;
    win->timers[i-1] = 0;


}

/** ****************************************************************************

Return number of mice

Returns the number of mice implemented. This is a pure passthrough function.

*******************************************************************************/

static ami_long imouse(FILE* f)

{

    return (*mouse_vect)(stdout); /* find number of mice */

}

/** ****************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the given mouse. This is a pure passthrough
function.

*******************************************************************************/

static ami_long imousebutton(FILE* f, ami_long m)

{

    return (*mousebutton_vect)(stdout, m); /* find number of buttons */

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached. This is a pure passthrough function.

*******************************************************************************/

static ami_long ijoystick(FILE* f)

{

    return (*joystick_vect)(stdout); /* find number of joysticks */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick. This is a pure passthrough
function.

*******************************************************************************/

static ami_long ijoybutton(FILE* f, ami_long j)

{

    return (*joybutton_vect)(stdout, j); /* find number of buttons */

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning. This is a pure
passthrough function.

*******************************************************************************/

static ami_long ijoyaxis(FILE* f, ami_long j)

{

    return (*joyaxis_vect)(stdout, j); /* find number of axies */

}

/** ****************************************************************************

Set tab

Sets a tab at the indicated column number.

*******************************************************************************/

static void isettab(FILE* f, ami_long t)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (t < 1 || t > MAXTAB) error("Invalid tab position");
    win->tab[t-1] = TRUE; /* set tab position */

}

/** ****************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

static void irestab(FILE* f, ami_long t)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (t < 1 || t > MAXTAB) error("Invalid tab position");
    win->tab[t-1] = FALSE; /* set tab position */

}

/** ****************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

static void iclrtab(FILE* f)

{

    winptr win; /* windows record pointer */
    int t;

    win = txt2win(f); /* get window from file */
    for (t = 0; t < MAXTAB; t++) win->tab[t] = 0; /* clear all tab stops */

}

/** ****************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. This is a pure
passthrough function.

*******************************************************************************/

static ami_long ifunkey(FILE* f)

{

    return (*funkey_vect)(stdout); /* find number of function keys */

}

/** ****************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

static void iframetimer(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */
    int    ti;

    win = txt2win(f); /* get window from file */
    if (e) { /* enable framing timer */

        if (!win->frmtim) { /* no current timer assigned */

            ti = 0;
            while (ti < AMI_MAXTIM && timtbl[ti]) ti++;
            if (ti >= AMI_MAXTIM) error("Root timers are full");
            timtbl[ti] = win; /* place owner link */
            win->frmtim = ti+1; /* place root id */

        }
        /* pass it down */
        (*timer_vect)(stdout, win->frmtim, 166, TRUE);

    } else { /* disable framing timer */

        /* pass it down */
        (*killtimer_vect)(stdout, win->frmtim);
        /* release the root timer so that we can reuse it */
        timtbl[win->frmtim-1] = NULL;
        win->frmtim = 0;

    }

}

/** ****************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from manager.
This exists to allow the results of manager unaware programs to be viewed after
termination, instead of exiting an destroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fulfills the requirement of
holding manager unaware programs.

*******************************************************************************/

static void iautohold(ami_long e)

{

    (*autohold_vect)(e); /* copy state to root */
    fautohold = e; /* set new state of autohold */

}

/** ****************************************************************************

Write string to current cursor position with length

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

*******************************************************************************/

static void iwrtstrn(FILE* f, char* s, ami_long n)

{

    winptr win; /* window record pointer */
    scnptr scp; /* screen buffer */
    ami_long   l;   /* character location */

    win = txt2win(f); /* get window from file */
    if (win->autof) error("Cannot direct write string with auto on");
    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* Write each character with the same bounds as plcchr: the store is
       bounded by the buffer, the display by the client area and the forward
       mask. The previous code kept separate store and display loops, both
       bounded by the client area, which discarded buffer content below the
       viewport, drew over occluding windows, and advanced the cursor once
       in each loop, twice per character in all. */
    while (n) {

        if (win->curx >= 1 && win->curx <= win->bufx &&
            win->cury >= 1 && win->cury <= win->bufy) { /* in buffer */

            if (win->bufmod) { /* buffer is active */

                /* the store goes to the UPDATE screen: writing to the
                   display screen sent everything to whatever was being
                   shown, so buffers prepared while another was displayed
                   stayed empty */
                scp = &SCNBUF(win->screens[win->curupd-1],
                              win->curx, win->cury);
                scp->ch = *s; /* place character to buffer */
                scp->forec = win->fcolor;
                scp->backc = win->bcolor;
                scp->attr = win->attr;

            }
            l = (win->cury-1)*win->bufx+(win->curx-1);
            if (indisp(win) && intcurbnd(win) &&
                win->fmask[l/8] & 1<<(l%8)) { /* visible on screen */

                setattrs(win->attr); /* set attributes */
                setfcolora(win->fcolor, win->bcolor, win->attr);
                setbcolor(win->bcolor);
                setcursor(win->curx+absx(win)-1+win->coffx,
                          win->cury+absy(win)-1+win->coffy);
                wrtchr(*s); /* output */

            }

        }
        if (win->curx < LONG_MAX) win->curx++; /* next location */
        s++; /* next character */
        n--; /* count */

    }
    setcur(curfocus? curfocus: win); /* place the cursor */

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

*******************************************************************************/

static void iwrtstr(FILE* f, char* s)

{

    iwrtstrn(f, s, strlen(s)); /* write string */

}

/** ****************************************************************************

Set window title with length

Sets the title of the current window.

*******************************************************************************/

static void ititlen(FILE* f, char* ts, ami_long n)

{

    winptr    win; /* windows record pointer */
    rectangle r;

    win = txt2win(f); /* get window from file */
    if (win->title) free(win->title); /* free previous string */
    win->title = malloc(n+1);
    if (!win->title) error("Out of memory");
    /* set title to invoking program */
    strncpy(win->title, ts, n);
    win->title[n] = 0; /* terminate */
    /* if its the root window, copy down to underlying window */
    if (win->root) (*titlen_vect)(stdout, ts, n);
    else if (win->frame && win->sysbar && win->pmaxy >= 3) {

        /* we own the window, the frame and system bar is on, and it is 
           visible */

        /* set title bounding box */
        setrect(&r, absx(win)+2, absy(win)+win->size, 
                   win->pmaxx-6-4, absy(win)+win->size); 
        drwfrm(win, &r); /* draw or redraw title */

    }

}

/** ****************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

static void ititle(FILE* f, char* ts)

{

    ititlen(f, ts, strlen(ts)); /* place title */

}

/** ****************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.

The window id can be from 1 to MAXFIL, but 1 is reserved for the main I/O
window.

*******************************************************************************/

static void iopenwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)

{

    /* open as child of client window */
    intopenwin(infile, outfile, parent, wid);

}

/** ****************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

static void ibuffer(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (e != win->bufmod) { /* buffered status has changed */

        win->bufmod = e; /* set new buffer status */
        /* The screens hold the window content in both modes: they are what
           repaints the surface when the window arrangement changes. The
           difference is only what governs their size: entering follow mode
           sizes the buffer to the client, and from there it tracks the
           client; entering buffered mode keeps the buffer as it stands, and
           from there the program governs it with sizbuf. */
        if (!e) {

            resizewinbuf(win, win->cmaxx, win->cmaxy);
            /* In follow mode the program owns the content and draws on
               the redraw announcements; entering it, the surface is the
               program's to paint, so it is asked to, as the graphical
               expose after a buffer change asks. Without this the window
               sat blank until some other action disturbed it. */
            annresize(win);
            annredraw(win);

        }

    }

}

/** ****************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

static void isizbuf(FILE* f, ami_long x, ami_long y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (!win->bufmod) error("Buffer mode is not enabled");
    if (x < 1 || y < 1) error("Invalid buffer size");
    if (win->bufx != x || win->bufy != y) {

        /* buffer size has changed */
        clrbufs(win); /* all the screen buffers are wrong, so tear them out */
        /* Take the new size before allocating: the buffer dimensions are
           what the screens are sized and indexed by, and they were left at
           the old values here, so the new screens came out the previous
           size. */
        win->bufx = x;
        win->bufy = y;
        win->maxx = x;
        win->maxy = y;
        /* Allocate the screens that are selected, not just the first one.
           A program that has selected any other screen, which the select
           call makes routine, was left with a null screen pointer and
           faulted on its next write. */
        win->screens[win->curupd-1] = malloc(sizeof(scnrec)*y*x);
        if (!win->screens[win->curupd-1]) error("Out of memory");
        iniscn(win, win->screens[win->curupd-1]);
        if (win->curdsp != win->curupd) { /* display screen is a different one */

            win->screens[win->curdsp-1] = malloc(sizeof(scnrec)*y*x);
            if (!win->screens[win->curdsp-1]) error("Out of memory");
            iniscn(win, win->screens[win->curdsp-1]);

        }
        free(win->fmask); /* release previous mask */
        alcfmask(win); /* allocate and clear forward mask */
        if (indisp(win)) restore(win); /* repaint from the new buffer */

    }
    recalcfmask(); /* recalculate the forward masks */

}

/** ****************************************************************************

Get window size character

Gets the onscreen parent window size, in character terms.

*******************************************************************************/

static void igetsiz(FILE* f, ami_long* x, ami_long* y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    *x = win->pmaxx; /* set size */
    *y = win->pmaxy;

}

/** ****************************************************************************

Set window size character

Sets the onscreen window size, in character terms.

*******************************************************************************/

static void isetsiz(FILE* f, ami_long x, ami_long y)

{

    intsetsiz(txt2win(f), x, y);

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

static void isetpos(FILE* f, ami_long x, ami_long y)

{

    intsetpos(txt2win(f), x, y);

}

/** ****************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

static void iscnsiz(FILE* f, ami_long* x, ami_long* y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    *x = dimx; /* return parent size */
    *y = dimy;

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

static void iscncen(FILE* f, ami_long* x, ami_long* y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    *x = dimx/2; /* return parent size/2 */
    *y = dimy/2;

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

static void mbarsiz(winptr win)

{

    wigptr wg = win->mbar;

    /* The menu bar spans the client width, so a change to the window's
       geometry re-spans it: without this the bar kept the width it was
       created at, and a window grown wider showed it stopping short. */
    if (!wg) return;
    isizbuf(wg->wf, win->cmaxx, 1); /* buffer to the new width */
    intsetsiz(wg->win, win->cmaxx, 1); /* face to the new width */
    /* Row zero of the owner's client space, which is the row just above
       the client -- where the client now begins whether the window is
       framed or not. */
    intsetpos(wg->win, 1, 0);
    wigdrw(wg); /* the size buffer change cleared the face */

}

static void iwinclient(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms)

{

    ami_long frame, size, sysbar;

    /* the inverse of the client geometry in recompcli()/decorx()/decory(),
       from the given mode set rather than a window's current state */
    frame = !!(BIT(ami_wmframe) & ms);
    size = !!(BIT(ami_wmsize) & ms);
    sysbar = !!(BIT(ami_wmsysbar) & ms);
    *wx = cx+(frame && size)*2;
    *wy = cy+(frame && size)*2+(frame && sysbar)*2
            +!!(BIT(ami_wmmenu) & ms); /* the menu row, frameless or not */

}

/** ****************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

/* Find whether a window lies in the subtree of another. */

static int intree(winptr t, winptr w)

{

    while (w) {

        if (w == t) return (TRUE);
        w = w->parwin;

    }

    return (FALSE);

}

/* Move a window and its whole subtree to sit just above the given anchor
   window in the Z order, keeping the subtree's internal order; a NULL
   anchor places it at the very back. The subtree is pulled out of the
   bottom-first list in one pass, which preserves its order, and spliced
   back in above the anchor; the order is then renumbered without gaps and
   the masks follow. */

static void placetree(winptr win, winptr anchor)

{

    winptr  chain = NULL; /* the extracted subtree, bottom first */
    winptr  chend = NULL;
    winptr  wp;
    winptr* lp;
    ami_long    z;

    lp = &zmin2max;
    while (*lp) {

        if (intree(win, *lp)) { /* pull out, keeping order */

            wp = *lp;
            *lp = wp->zmin2max;
            wp->zmin2max = NULL;
            if (chend) chend->zmin2max = wp; else chain = wp;
            chend = wp;

        } else lp = &(*lp)->zmin2max;

    }
    if (anchor) { /* splice back in just above the anchor */

        chend->zmin2max = anchor->zmin2max;
        anchor->zmin2max = chain;

    } else { /* the very back */

        chend->zmin2max = zmin2max;
        zmin2max = chain;

    }
    /* renumber from the back, without gaps */
    wp = zmin2max;
    z = 0;
    while (wp) { wp->zorder = z++; wp = wp->zmin2max; }
    ztop = z-1;
    makzmax2min(); /* remake the top-first list */
    recalcfmask(); /* occlusion follows the new order */

}

/* Bring a window and its subtree to the front, above everything outside
   the subtree, keeping its internal order: the children stay above their
   window, and what the user had on top stays on top. */

static void fronttree(winptr win)

{

    winptr    wp;
    winptr    anchor = NULL;
    rectangle r;

    /* the topmost window outside the subtree is the one to land on */
    for (wp = zmin2max; wp; wp = wp->zmin2max)
        if (!intree(win, wp)) anchor = wp;
    placetree(win, anchor);
    treerect(win, &r);
    redraw(zmax2min, r.x1, r.y1, r.x2, r.y2); /* repaint its region */

}

/* Put a window and its subtree to the back of its sibling group: just
   above its parent, keeping its internal order. The back of the order is
   relative to the parent in the window tree -- a child sent to the back
   goes behind its siblings, not behind the window that contains it. */

static void backtree(winptr win)

{

    rectangle r;

    placetree(win, win->parwin); /* NULL parent: the root, the very back */
    treerect(win, &r);
    redraw(zmax2min, r.x1, r.y1, r.x2, r.y2); /* repaint its region */

}

static void ifront(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    fronttree(win); /* make this the front window, children above */

}

/** ****************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

static void iback(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    backtree(win); /* make this the back window, children above it */

}

/** ****************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

/* Recompute the client geometry after a decoration change. The client
   offsets and dimensions are derived from the frame, size and system bar
   flags; they were set at open and must follow any change of those flags,
   with the buffer grown to keep covering the client, and the masks redone
   for the new client rectangle. */
static void recompcli(winptr win)

{

    win->coffx = 0+(win->frame && win->size);
    win->coffy = 0+(win->frame && win->size)+(win->frame && win->sysbar)*2+
                 (win->mbar != 0);
    win->cmaxx = win->pmaxx; /* client dimensions from decorations */
    win->cmaxy = win->pmaxy;
    win->cmaxx -= decorx(win);
    win->cmaxy -= decory(win);
    if (win->cmaxx < 0) win->cmaxx = 0; /* a bar only window has no client */
    if (win->cmaxy < 0) win->cmaxy = 0;
    /* in follow mode the buffer tracks the client; in buffered mode the
       buffer keeps the size it was given */
    if (!win->bufmod) resizewinbuf(win, win->cmaxx, win->cmaxy);
    recalcfmask();
    mbarsiz(win); /* the menu bar follows the client */

}

static void iframe(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (win->frame != e) {

        win->frame = e; /* set frame state */
        recompcli(win); /* the client geometry follows the decorations */
        restore(win); /* redraw */
        annresize(win); /* the client changed size within the window */
        annredraw(win); /* a follow mode window needs its program */

    }

}

/** ****************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

On GNOME/Ubuntu 20.04 with GDM3 window manager, we are not capable of turning
off the size bars alone, so this is a no-op. It may work on other window
managers.

*******************************************************************************/

static void isizable(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (win->size != e) {

        win->size = e; /* set frame state */
        recompcli(win); /* the client geometry follows the decorations */
        restore(win); /* redraw */
        annresize(win); /* the client changed size within the window */
        annredraw(win); /* a follow mode window needs its program */

    }

}

/** ****************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

I don't think XWindow can do this separately. Instead, the frame() function is
used to create component windows.

*******************************************************************************/

static void isysbar(FILE* f, ami_long e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (win->sysbar != e) {

        win->sysbar = e; /* set frame state */
        recompcli(win); /* the client geometry follows the decorations */
        restore(win); /* redraw */
        annresize(win); /* the client changed size within the window */
        annredraw(win); /* a follow mode window needs its program */

    }

}

/** ****************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/


/** ****************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/


/** ****************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/


/** ****************************************************************************

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu, then the program selections placed in the menu.

*******************************************************************************/


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

static ami_long igetwinid(void)

{

    ami_long wid; /* window id */

    wid = -1; /* start at -1 */
    /* find any open entry */
    while (wid > -MAXFIL && xltwin[wid+MAXFIL] >= 0) wid--;
    if (wid == -MAXFIL) error("No more anonymous wids");

    return (wid); /* return the wid */

}

/** ****************************************************************************

Set window focus

Sends the focus, or which window gets input characters, to a given window.

*******************************************************************************/

static void ifocus(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (!win->focus) { /* not already in focus */

        remfocus(); /* remove previous focus */
        win->focus = TRUE; /* set current focus */
        curfocus = win;
        setcur(win); /* set cursor active */

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

static void plcchr(FILE* f, char c)

{

    winptr  win;   /* windows record pointer */
    scnrec* scp;   /* pointer to screen location */
    ami_long    l;

    win = txt2win(f); /* get window from file */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* handle special character cases first */
    if (c == '\r')
        /* carriage return, position to extreme left */
        icursor(f, 1, win->cury);
    else if (c == '\n') {

        /* line end */
        idown(f); /* line feed, move down */
        /* position to extreme left */
        icursor(f, 1, win->cury);

    } else if (c == '\b') ileft(f); /* back space, move left */
    else if (c == '\f') {

        mclogf("clear wid %lld %lldx%lld\n",
                             AMI_LONG_CAST(win->wid), AMI_LONG_CAST(win->maxx), AMI_LONG_CAST(win->maxy));
        clrscn(f); /* clear screen */

    }
    else if (c == '\t') itab(f); /* process tab */
    /* only output visible characters */
    else if (c >= ' ' && c != 0x7f) {

        /* find character location */
        l = (win->cury-1)*win->bufx+(win->curx-1); 
        /* The store and the display have different bounds. The store is
           bounded by the buffer, which can be larger than the window: the
           client area is a viewport onto it. The display is bounded by the
           client area, and also by the forward mask, since an occluded
           character must not be drawn but must still reach the buffer, or
           it would be missing when the occluding window moves away. The
           previous code gated both on the client area and the mask
           together, so anything written below the viewport, or under
           another window, was silently discarded from the buffer as well:
           a window with a buffer taller than itself lost everything past
           the visible rows. */
        if (win->curx >= 1 && win->curx <= win->bufx &&
            win->cury >= 1 && win->cury <= win->bufy) { /* in buffer */

            if (win->bufmod) { /* buffer is active */

                /* Index screen character location. This is the UPDATE
                   screen, which is the one being written; it is the
                   display screen only when the two are selected the same.
                   Indexing the display screen here sent writes to whatever
                   happened to be shown, so a program preparing buffers
                   while displaying another left them all empty. */
                scp = &SCNBUF(win->screens[win->curupd-1],
                              win->curx, win->cury);
                /* place character to buffer */
                scp->ch = c;
                scp->forec = win->fcolor;
                scp->backc = win->bcolor;
                scp->attr = win->attr;

            }
            if (indisp(win) && intcurbnd(win) &&
                win->fmask[l/8] & 1<<(l%8)) { /* visible on screen */

                setattrs(win->attr); /* set attributes */
                setfcolora(win->fcolor, win->bcolor, win->attr);
                setbcolor(win->bcolor);
                /* draw character to active screen */
                setcursor(win->curx+absx(win)-1+win->coffx,
                          win->cury+absy(win)-1+win->coffy);
                wrtchr(c); /* output */

            }

        }
        /* advance to next character */
        iright(f);

    }

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

    if (fd < 0 || fd >= MAXFIL) error("Invalid handle");
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

    if (fd < 0 || fd >= MAXFIL) error("Invalid handle");
    if (opnfil[fd] && opnfil[fd]->win) { /* process window output file */

        win = opnfil[fd]->win; /* index window */
        /* The cursor is off while drawing, then restored to the focus
           window: client output must not leave the visible cursor sitting
           wherever the last character landed. */
        setcurvis(FALSE);
        /* send data to terminal */
        while (cnt--) plcchr(opnfil[fd]->sfp, *p++);
        setcur(curfocus? curfocus: win); /* restore the cursor */
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

    if (fd < 0 || fd >= MAXFIL) error("Invalid handle");
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
    if (fd == INPFIL || fd == OUTFIL)
        error("Cannot perform operation on special file");

    return (*lseekdc)(fd, offset, whence);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    return ivlseek(ofplseek, fd, offset, whence);

}

/** ****************************************************************************

                          CHARACTER MODE WIDGETS

Implements the standard Petit-Ami widget set in character cells. Each widget
is a small frameless subwindow of its owning window, kept on the owner's
widget list. The subwindow machinery provides drawing, buffering, occlusion
and repaint for free; this section provides the faces, drawn in characters,
and the behavior: events arriving for a widget window are intercepted in
intsendevent() and translated here into widget events for the owner.

Only the character coordinate calls exist; the widgets have no graphical
counterparts in this module. Drop boxes and tab bars are not (yet)
implemented.

*******************************************************************************/

static ami_long wigmul(ami_long range, ami_long val); /* forward */
static ami_long wigscl(ami_long num, ami_long den); /* forward */

/* find widget by id in a window */
static wigptr fndwig(winptr win, ami_long id)

{

    wigptr wg;

    wg = win->wiglst; /* index the widget list */
    while (wg && wg->id != id) wg = wg->next;

    return (wg);

}

/* write a string into a widget face, with reverse video select */
static void wigtxt(wigptr wg, ami_long x, ami_long y, const char* s, int rev)

{

    winptr win = wg->win;
    ami_long   n = win->cmaxx-x+1; /* room to the edge: never run past it */
    ami_long   sat = win->attr; /* restored whole: a face can carry its own
                               attributes, and clearing just the reverse
                               bit was dropping them */

    icursor(wg->wf, x, y); /* place cursor */
    if (rev) win->attr |= BIT(sarev);
    while (*s && n-- > 0) plcchr(wg->wf, *s++);
    win->attr = sat;

}

/* clear a widget face to spaces. The face attributes stay off the
   blanks: an underlined label underlines its text, not the whole row */
static void wigclr(wigptr wg)

{

    ami_long x, y;
    ami_long sat = wg->win->attr;

    wg->win->attr &= ~wg->fatt;
    for (y = 1; y <= wg->win->cmaxy; y++) {

        icursor(wg->wf, 1, y);
        for (x = 1; x <= wg->win->cmaxx; x++) plcchr(wg->wf, ' ');

    }
    wg->win->attr = sat;

}

/* draw a centered label into a row, clipped to width */
static void wiglab(wigptr wg, ami_long y, const char* s, int rev)

{

    ami_long w = wg->win->cmaxx;
    ami_long l = strlen(s);
    ami_long x;

    if (l > w) l = w; /* clip */
    x = (w-l)/2+1; /* center */
    icursor(wg->wf, x, y);
    if (rev) wg->win->attr |= BIT(sarev);
    while (l--) plcchr(wg->wf, *s++);
    wg->win->attr &= ~BIT(sarev);

}

/** ****************************************************************************

                        POPUPS, TAB BARS AND MENUS

A popup is a widget whose face is a list of lines, floated above everything
and dismissed by a selection or by a click elsewhere. Drop boxes use one to
show their list; menus use a cascade of them. Popups are kept on a stack so
a click outside closes the whole cascade.

*******************************************************************************/

/* close popups from the top down to the given depth */
static void clspops(int downto)

{

    wigptr wg;

    while (popcnt > downto) {

        wg = popstk[--popcnt];
        popstk[popcnt] = NULL;
        if (hovwig == wg) hovwig = NULL; /* the highlight dies with it */
        /* unlink from its owner's widget list and drop it */
        if (wg->parent) {

            wigptr* lp = &wg->parent->wiglst;
            while (*lp && *lp != wg) lp = &(*lp)->next;
            if (*lp) *lp = wg->next;

        }
        fclose(wg->wf); /* close the popup window */
        if (wg->face) free(wg->face);
        if (wg->lcol) free(wg->lcol);
        if (wg->list) {

            ami_long i;
            for (i = 0; i < wg->listn; i++) free(wg->list[i]);
            free(wg->list);

        }
        free(wg);

    }

}

/* Open a popup list at a root position. The strings are copied. Returns the
   popup widget, which is pushed on the popup stack. */
static wigptr opnpop(winptr par, ami_long rx, ami_long ry, char** strs, ami_long n,
                     wigptr owner, ami_menuptr mitems, ami_color* lcol)

{

    wigptr wg;
    FILE*  wf;
    ami_long   i, w = 1;

    if (popcnt >= MAXPOP) clspops(MAXPOP-1); /* bound the cascade */
    for (i = 0; i < n; i++) /* widest entry sets the width */
        if ((ami_long)strlen(strs[i]) > w) w = strlen(strs[i]);
    w += 2; /* frame columns */
    wg = malloc(sizeof(wigrec));
    if (!wg) error("Out of memory");
    /* parentless: floats over all. As with widget faces, popups take no
       keyboard focus */
    opnwig = TRUE;
    iopenwin(&stdin, &wf, NULL, igetwinid());
    opnwig = FALSE;
    wg->id = 0; /* popups have no client id */
    wg->typ = wtpopup;
    wg->parent = par;
    wg->wf = wf;
    wg->win = txt2win(wf);
    wg->face = NULL;
    wg->list = malloc(sizeof(char*)*(n? n: 1));
    if (!wg->list) error("Out of memory");
    for (i = 0; i < n; i++) {

        wg->list[i] = malloc(strlen(strs[i])+1);
        if (!wg->list[i]) error("Out of memory");
        strcpy(wg->list[i], strs[i]);

    }
    wg->listn = n;
    wg->enb = TRUE;
    wg->sel = 0;
    wg->val = 0;
    wg->low = 0;
    wg->high = 0;
    wg->sclsiz = 0;
    wg->marks = 0;
    wg->top = 0;
    wg->lcol = NULL;
    if (lcol && n) { /* the entries keep their own colors */

        wg->lcol = malloc(sizeof(ami_color)*n);
        if (!wg->lcol) error("Out of memory");
        memcpy(wg->lcol, lcol, sizeof(ami_color)*n);

    }
    wg->fatt = 0;
    wg->curs = 0;
    wg->tor = ami_totop;
    wg->owner = owner;
    wg->mitems = mitems;
    wg->win->widget = TRUE;
    wg->win->wig = wg;
    wg->win->curv = FALSE; /* a widget face never shows the cursor */
    wg->next = par->wiglst; /* on the owner's list for cleanup */
    par->wiglst = wg;
    /* frame it, no system bar: a plain bordered list */
    wg->win->frame = TRUE;
    wg->win->size = FALSE;
    wg->win->sysbar = FALSE;
    recompcli(wg->win);
    intsetsiz(wg->win, w, n+2);
    /* keep it on the surface */
    if (rx+w-1 > dimx) rx = dimx-w+1;
    if (ry+n+1 > dimy) ry = dimy-n-1;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    intsetpos(wg->win, rx, ry);
    intfront(wg->win); /* above everything */
    popstk[popcnt++] = wg; /* push */
    wigdrw(wg);

    return (wg);

}

/* count and collect a menu level into a string array. Returns the count.
   Submenus are marked with a trailing arrow, checked items with a mark. */
static ami_long mencol(winptr win, ami_menuptr m, char*** strs)

{

    ami_menuptr p;
    ami_long n = 0, i = 0;
    char buf[MAXLIN];
    menenaptr me;

    for (p = m; p; p = p->next) n++;
    *strs = malloc(sizeof(char*)*(n? n: 1));
    if (!*strs) error("Out of memory");
    for (p = m; p; p = p->next) {

        int ena = TRUE;
        for (me = win->menena; me; me = me->next)
            if (me->id == p->id) ena = me->ena;
        snprintf(buf, sizeof(buf), "%c%c%s%s",
                 ena? ' ': '(', /* disabled shown in parens */
                 p->onoff? '*': ' ', /* check mark: the set state only; the
                                        oneof flag is group structure, not a
                                        selection */
                 p->face,
                 p->branch? " >": (ena? "": ")"));
        (*strs)[i] = malloc(strlen(buf)+1);
        if (!(*strs)[i]) error("Out of memory");
        strcpy((*strs)[i], buf);
        i++;

    }

    return (n);

}

/* find the nth entry of a menu level */
static ami_menuptr mennth(ami_menuptr m, ami_long n)

{

    while (m && n > 1) { m = m->next; n--; }

    return (m);

}

/* is a menu item enabled */
static int menenb(winptr win, ami_long id)

{

    menenaptr me;

    for (me = win->menena; me; me = me->next) if (me->id == id) return (me->ena);

    return (TRUE); /* enabled unless said otherwise */

}

/* draw the menu bar face: top level titles in a row */
static void drwmbar(wigptr wg)

{

    ami_menuptr p;
    ami_long x = 1;
    ami_long hp = wg == hovwig? hovwprt: 0; /* hovered title start column */

    wigclr(wg);
    for (p = wg->mitems; p; p = p->next) {

        int enb = menenb(wg->parent, p->id);

        wigtxt(wg, x, 1, " ", FALSE);
        /* a disabled item shows grey; selected or hovered shows reversed */
        if (!enb) wg->win->attr |= BIT(sagrey);
        wigtxt(wg, x+1, 1, p->face, wg->sel == x || hp == x);
        wg->win->attr &= ~BIT(sagrey);
        x += strlen(p->face)+2;

    }

}

/* find which top level menu title a bar column lies in; returns the item or
   NULL, and sets the column the title starts at */
static ami_menuptr mbarhit(wigptr wg, ami_long lx, ami_long* startx)

{

    ami_menuptr p;
    ami_long x = 1;

    for (p = wg->mitems; p; p = p->next) {

        ami_long w = strlen(p->face)+2;
        if (lx >= x && lx < x+w) { *startx = x; return (p); }
        x += w;

    }

    return (NULL);

}

/* draw the face of a widget from its state */
static void wigdrw(wigptr wg)

{

    winptr win = wg->win;
    ami_long   w = win->cmaxx;
    ami_long   h = win->cmaxy;
    ami_long   x, y, i, n, ts, tp;
    char   buf[MAXLIN];

    /* The cursor is off while the face draws, so it does not flash along
       the drawing; setcur() below restores it, with its visibility, at
       the focus window when the face is done. */
    setcurvis(FALSE);
    int    rev;
    /* the live part under the mouse shows reversed, as frame parts do */
    ami_long   hp = wg == hovwig? hovwprt: 0;

    /* a disabled widget shows its whole face grey, as a disabled menu
       item does; a face carrying its own attributes shows them */
    if (!wg->enb) win->attr |= BIT(sagrey);
    win->attr |= wg->fatt;
    switch (wg->typ) {

        case wtbutton:
            wigclr(wg);
            rev = wg->sel || hp || (win->focus && wg->enb);
            if (h >= 3) { /* boxed face */

                icursor(wg->wf, 1, 1);
                plcchr(wg->wf, '+');
                for (x = 2; x < w; x++) plcchr(wg->wf, '-');
                plcchr(wg->wf, '+');
                for (y = 2; y < h; y++) {

                    icursor(wg->wf, 1, y); plcchr(wg->wf, '|');
                    icursor(wg->wf, w, y); plcchr(wg->wf, '|');

                }
                icursor(wg->wf, 1, h);
                plcchr(wg->wf, '+');
                for (x = 2; x < w; x++) plcchr(wg->wf, '-');
                plcchr(wg->wf, '+');
                wiglab(wg, (h+1)/2, wg->face, rev);

            } else { /* single row face */

                icursor(wg->wf, 1, 1); plcchr(wg->wf, '[');
                icursor(wg->wf, w, 1); plcchr(wg->wf, ']');
                wiglab(wg, 1, wg->face, rev);

            }
            break;

        case wtcheckbox:
            wigclr(wg);
            snprintf(buf, sizeof(buf), "[%c] %s", wg->sel? 'X': ' ', wg->face);
            wigtxt(wg, 1, 1, buf, hp || (win->focus && wg->enb));
            break;

        case wtradio:
            wigclr(wg);
            snprintf(buf, sizeof(buf), "(%c) %s", wg->sel? '*': ' ', wg->face);
            wigtxt(wg, 1, 1, buf, hp || (win->focus && wg->enb));
            break;

        case wtgroup:
            wigclr(wg);
            icursor(wg->wf, 1, 1);
            plcchr(wg->wf, '+');
            for (x = 2; x < w; x++) plcchr(wg->wf, '-');
            plcchr(wg->wf, '+');
            for (y = 2; y < h; y++) {

                icursor(wg->wf, 1, y); plcchr(wg->wf, '|');
                icursor(wg->wf, w, y); plcchr(wg->wf, '|');

            }
            icursor(wg->wf, 1, h);
            plcchr(wg->wf, '+');
            for (x = 2; x < w; x++) plcchr(wg->wf, '-');
            plcchr(wg->wf, '+');
            /* title in the top run */
            if (wg->face && *wg->face) {

                n = strlen(wg->face); if (n > w-4) n = w-4;
                icursor(wg->wf, 3, 1);
                plcchr(wg->wf, ' ');
                for (i = 0; i < n; i++) plcchr(wg->wf, wg->face[i]);
                plcchr(wg->wf, ' ');

            }
            break;

        case wtbackground:
            wigclr(wg);
            break;

        case wtscrollvert:
            /* arrows at the ends, track between, thumb from size/position;
               the arrows, the thumb, and the paging runs of the track are
               each their own hover part */
            n = h-2; /* track length */
            ts = tp = 0;
            if (n > 0) {

                ts = wigmul(n, wg->sclsiz); /* thumb size */
                if (ts < 1) ts = 1;
                if (ts > n) ts = n;
                tp = wigmul(n-ts, wg->val); /* offset */

            }
            wigtxt(wg, 1, 1, "^", hp == 1);
            for (y = 2; y < h; y++)
                wigtxt(wg, 1, y, ".", y-2 < tp? hp == 4: hp == 5);
            wigtxt(wg, 1, h, "v", hp == 2);
            for (y = 0; y < ts; y++) wigtxt(wg, 1, 2+tp+y, "#", hp == 3);
            break;

        case wtscrollhoriz:
            n = w-2;
            ts = tp = 0;
            if (n > 0) {

                ts = wigmul(n, wg->sclsiz);
                if (ts < 1) ts = 1;
                if (ts > n) ts = n;
                tp = wigmul(n-ts, wg->val);

            }
            wigtxt(wg, 1, 1, "<", hp == 1);
            for (x = 2; x < w; x++)
                wigtxt(wg, x, 1, ".", x-2 < tp? hp == 4: hp == 5);
            wigtxt(wg, w, 1, ">", hp == 2);
            for (x = 0; x < ts; x++) wigtxt(wg, 2+tp+x, 1, "#", hp == 3);
            break;

        case wtnumselbox:
            wigclr(wg);
            wigtxt(wg, 1, 1, "-", hp == 1);
            wigtxt(wg, w, 1, "+", hp == 2);
            snprintf(buf, sizeof(buf), "%*lld", (int)(w-2), AMI_LONG_CAST(wg->val));
            wigtxt(wg, 2, 1, buf, win->focus);
            break;

        case wteditbox:
            wigclr(wg);
            n = strlen(wg->face);
            /* keep the cursor visible: scroll the text if needed */
            i = 0; /* display start */
            if (wg->curs >= w) i = wg->curs-w+1;
            for (x = 1; x <= w; x++) {

                char c = i+x-1 < n? wg->face[i+x-1]: ' ';
                /* hover reverses the field; the edit cursor cell flips
                   back, so it stays visible within the highlight */
                rev = (win->focus && (i+x-1 == wg->curs)) ^ (hp != 0);
                buf[0] = c; buf[1] = 0;
                wigtxt(wg, x, 1, buf, rev);

            }
            break;

        case wtprogbar:
            for (x = 1; x <= w; x++) {

                n = wigmul(w, wg->val);
                wigtxt(wg, x, 1, x <= n? "=": ".", FALSE);

            }
            break;

        case wtlistbox:
            wigclr(wg);
            for (y = 1; y <= h && wg->top+y <= wg->listn; y++)
                wigtxt(wg, 1, y, wg->list[wg->top+y-1],
                       wg->top+y == wg->sel || y == hp);
            break;

        case wtslidehoriz:
            for (x = 1; x <= w; x++) wigtxt(wg, x, 1, "-", FALSE);
            /* tick marks spaced evenly along the rail, ends included */
            if (wg->marks > 1)
                for (i = 0; i < wg->marks; i++)
                    wigtxt(wg, 1+((w-1)*i+(wg->marks-1)/2)/(wg->marks-1), 1,
                           "+", FALSE);
            n = 1+wigmul(w-1, wg->val);
            wigtxt(wg, n, 1, "#", hp != 0);
            break;

        case wtslidevert:
            for (y = 1; y <= h; y++) wigtxt(wg, 1, y, "|", FALSE);
            if (wg->marks > 1)
                for (i = 0; i < wg->marks; i++)
                    wigtxt(wg, 1, 1+((h-1)*i+(wg->marks-1)/2)/(wg->marks-1),
                           "+", FALSE);
            n = 1+wigmul(h-1, wg->val);
            wigtxt(wg, 1, n, "#", hp != 0);
            break;

        case wtdropbox:
            /* closed face: the selection and a drop arrow; an entry
               carrying its own color shows in it, light ones grounded
               on black so they read on the light face */
            wigclr(wg);
            if (wg->sel >= 1 && wg->sel <= wg->listn) {

                ami_color sf = win->fcolor, sb = win->bcolor;

                if (wg->lcol) {

                    win->fcolor = wg->lcol[wg->sel-1];
                    if (win->fcolor == ami_white) win->bcolor = ami_black;

                }
                wigtxt(wg, 1, 1, wg->list[wg->sel-1], win->focus || hp);
                win->fcolor = sf;
                win->bcolor = sb;

            }
            wigtxt(wg, w, 1, "v", hp != 0);
            break;

        case wtdropeditbox:
            /* an edit box with a drop arrow in the last cell; the arrow
               and the edit area are separate hover parts */
            wigclr(wg);
            n = strlen(wg->face);
            i = 0;
            if (wg->curs >= w-1) i = wg->curs-w+2;
            for (x = 1; x <= w-1; x++) {

                char c = i+x-1 < n? wg->face[i+x-1]: ' ';
                rev = (win->focus && (i+x-1 == wg->curs)) ^ (hp == 1);
                buf[0] = c; buf[1] = 0;
                wigtxt(wg, x, 1, buf, rev);

            }
            wigtxt(wg, w, 1, "v", hp == 2);
            break;

        case wttabbar:
            wigclr(wg);
            if (wg->tor == ami_totop || wg->tor == ami_tobottom) {

                /* names along a row, separated by bars */
                x = 1;
                for (i = 0; i < wg->listn && x <= w; i++) {

                    wigtxt(wg, x, 1, wg->list[i],
                           wg->sel == i+1 || hp == i+1);
                    x += strlen(wg->list[i]);
                    if (i+1 < wg->listn) { wigtxt(wg, x, 1, "|", FALSE); x++; }

                }

            } else {

                /* names down the column, one character per row, with a
                   separator between tabs */
                y = 1;
                for (i = 0; i < wg->listn && y <= h; i++) {

                    const char* p = wg->list[i];
                    while (*p && y <= h) {

                        buf[0] = *p++; buf[1] = 0;
                        wigtxt(wg, 1, y++, buf, wg->sel == i+1 || hp == i+1);

                    }
                    if (i+1 < wg->listn && y <= h)
                        wigtxt(wg, 1, y++, "-", FALSE);

                }

            }
            break;

        case wtmenubar:
            drwmbar(wg);
            break;

        case wtpopup:
            /* the list, one entry per line, current selection reversed; a
               disabled menu entry shows grey */
            wigclr(wg);
            for (y = 1; y <= h && y <= wg->listn; y++) {

                int enb = TRUE;

                if (wg->mitems) { /* a menu level knows enabled state */

                    ami_menuptr item = mennth(wg->mitems, y);

                    if (item) enb = menenb(wg->parent, item->id);

                }
                if (!enb) wg->win->attr |= BIT(sagrey);
                if (wg->lcol) { /* the entry in its own color */

                    ami_color sf = win->fcolor, sb = win->bcolor;

                    win->fcolor = wg->lcol[y-1];
                    if (win->fcolor == ami_white) win->bcolor = ami_black;
                    wigtxt(wg, 1, y, wg->list[y-1], y == wg->sel || y == hp);
                    win->fcolor = sf;
                    win->bcolor = sb;

                } else
                    wigtxt(wg, 1, y, wg->list[y-1], y == wg->sel || y == hp);
                wg->win->attr &= ~BIT(sagrey);

            }
            break;

    }
    win->attr &= ~(BIT(sagrey)|wg->fatt);
    /* The drawing walked the physical cursor over the widget face; it
       belongs to the focus window. Without this the cursor was left
       sitting at the end of whatever widget drew last -- the menu bar
       showed it parked after its last title. */
    setcur(curfocus? curfocus: wg->parent);

}

/* Map a position in a widget face to the live part under it, for hover
   feedback. Zero is no live part, and the codes are per type, matching
   what a click there does: parts that are dead to clicks (a group box, a
   progress bar, the value cell of a number select) return zero, so the
   highlight marks exactly what is live. */
static ami_long wighit(wigptr wg, ami_long lx, ami_long ly)

{

    winptr win = wg->win;
    ami_long   n, ts, tp;

    switch (wg->typ) {

        case wtbutton:
        case wtcheckbox:
        case wtradio:
        case wteditbox:
        case wtslidehoriz:
        case wtslidevert:
        case wtdropbox:
            return (1); /* the whole face is live */

        case wtscrollvert:
            if (ly == 1) return (1); /* up arrow */
            if (ly == win->cmaxy) return (2); /* down arrow */
            n = win->cmaxy-2; /* track length */
            if (n <= 0) return (0);
            ts = wigmul(n, wg->sclsiz); /* thumb size */
            if (ts < 1) ts = 1;
            if (ts > n) ts = n;
            tp = wigmul(n-ts, wg->val); /* offset */
            if (ly-2 >= tp && ly-2 < tp+ts) return (3); /* thumb */
            return (ly-2 < tp? 4: 5); /* page up/down runs of the track */

        case wtscrollhoriz:
            if (lx == 1) return (1);
            if (lx == win->cmaxx) return (2);
            n = win->cmaxx-2;
            if (n <= 0) return (0);
            ts = wigmul(n, wg->sclsiz);
            if (ts < 1) ts = 1;
            if (ts > n) ts = n;
            tp = wigmul(n-ts, wg->val);
            if (lx-2 >= tp && lx-2 < tp+ts) return (3);
            return (lx-2 < tp? 4: 5);

        case wtnumselbox:
            if (lx == 1) return (1); /* decrement */
            if (lx == win->cmaxx) return (2); /* increment */
            return (0); /* the value cell is dead */

        case wtlistbox: /* the entry row */
            return (ly >= 1 && wg->top+ly <= wg->listn? ly: 0);

        case wtdropeditbox: /* drop arrow, or the edit area */
            return (lx == win->cmaxx? 2: 1);

        case wttabbar: { /* the tab under the pointer */

            ami_long i, p = 1;

            if (wg->tor == ami_totop || wg->tor == ami_tobottom) {

                for (i = 0; i < wg->listn; i++) {

                    ami_long tw = strlen(wg->list[i]);
                    if (lx >= p && lx < p+tw) return (i+1);
                    p += tw+1; /* name and separator */

                }

            } else {

                for (i = 0; i < wg->listn; i++) {

                    ami_long th = strlen(wg->list[i]);
                    if (ly >= p && ly < p+th) return (i+1);
                    p += th+1;

                }

            }
            return (0);

        }

        case wtmenubar: { /* the title under the pointer, by start column,
                             which is how the bar codes its selection */

            ami_long sx;
            ami_menuptr mp = mbarhit(wg, lx, &sx);

            return (mp && menenb(wg->parent, mp->id)? sx: 0);

        }

        case wtpopup: /* the entry row, if the item is enabled */
            if (ly < 1 || ly > wg->listn) return (0);
            if (wg->mitems) { /* a menu level knows enabled state */

                ami_menuptr item = mennth(wg->mitems, ly);

                if (!item || !menenb(wg->parent, item->id)) return (0);

            }
            return (ly);

        default: /* group, background, progress bar: nothing is live */
            return (0);

    }

}

/* send a widget event to the owner */
static void wigsig(wigptr wg, ami_evtcod e, ami_long v)

{

    ami_evtrec er;

    er.etype = e;
    switch (e) { /* fill the union by type */

        case ami_etbutton: er.butid = wg->id; break;
        case ami_etchkbox: er.ckbxid = wg->id; break;
        case ami_etradbut: er.radbid = wg->id; break;
        case ami_etsclull: er.sclulid = wg->id; break;
        case ami_etscldrl: er.scldrid = wg->id; break;
        case ami_etsclulp: er.sclupid = wg->id; break;
        case ami_etscldrp: er.scldpid = wg->id; break;
        case ami_etsclpos: er.sclpid = wg->id; er.sclpos = v; break;
        case ami_etedtbox: er.edtbid = wg->id; break;
        case ami_etnumbox: er.numbid = wg->id; er.numbsl = v; break;
        case ami_etlstbox: er.lstbid = wg->id; er.lstbsl = v; break;
        case ami_etsldpos: er.sldpid = wg->id; er.sldpos = v; break;
        case ami_etdrpbox: er.drpbid = wg->id; er.drpbsl = v; break;
        case ami_etdrebox: er.drebid = wg->id; break;
        case ami_ettabbar: er.tabid = wg->id; er.tabsel = v; break;
        default: break;

    }
    intsendevent(wg->parent, &er); /* to the owner's queue */

}

/* cells from a full scale value: round(range*val/LONG_MAX), end exact.
   The previous fixed point form quantized to 1024 steps and floored, so a
   full scale value always came out one cell short of the end. */
static ami_long wigmul(ami_long range, ami_long val)

{

    if (val <= 0 || range <= 0) return (0);
    if (val >= LONG_MAX) return (range);

    return ((ami_long)((double)range*val/(double)LONG_MAX+0.5));

}

/* full scale value from a fraction, guarding the ends */
static ami_long wigscl(ami_long num, ami_long den)

{

    if (den <= 0) return (0);
    if (num <= 0) return (0);
    if (num >= den) return (LONG_MAX);

    return (num*(LONG_MAX/den));

}

/* Track a mouse drag on a value widget: recompute the value from the
   mouse position, and redraw and notify only when it changes. Sliders
   take the position directly; scroll bars position the thumb center at
   the pointer within the track. */
static void wigdrag(void)

{

    wigptr wg = drgwig;
    winptr win;
    ami_long   n, ts, p, nv;

    if (!wg) return;
    win = wg->win;
    nv = wg->val;
    switch (wg->typ) {

        case wtslidehoriz:
            p = mousex-absx(win); /* 0 based position */
            if (p < 0) p = 0;
            if (p > win->cmaxx-1) p = win->cmaxx-1;
            nv = wigscl(p, win->cmaxx-1);
            break;

        case wtslidevert:
            p = mousey-absy(win);
            if (p < 0) p = 0;
            if (p > win->cmaxy-1) p = win->cmaxy-1;
            nv = wigscl(p, win->cmaxy-1);
            break;

        case wtscrollvert:
            n = win->cmaxy-2; /* track length */
            ts = wigmul(n, wg->sclsiz); /* thumb size */
            if (ts < 1) ts = 1;
            if (n-ts <= 0) return; /* thumb fills the track */
            p = mousey-absy(win)-1-ts/2; /* thumb top from pointer */
            if (p < 0) p = 0;
            if (p > n-ts) p = n-ts;
            nv = wigscl(p, n-ts);
            break;

        case wtscrollhoriz:
            n = win->cmaxx-2;
            ts = wigmul(n, wg->sclsiz);
            if (ts < 1) ts = 1;
            if (n-ts <= 0) return;
            p = mousex-absx(win)-1-ts/2;
            if (p < 0) p = 0;
            if (p > n-ts) p = n-ts;
            nv = wigscl(p, n-ts);
            break;

        default: return; /* not a draggable type */

    }
    if (nv != wg->val) { /* the value moved */

        wg->val = nv;
        wigdrw(wg);
        if (wg->typ == wtscrollvert || wg->typ == wtscrollhoriz)
            wigsig(wg, ami_etsclpos, nv);
        else wigsig(wg, ami_etsldpos, nv);

    }

}

/* widget window event processor. Called from intsendevent for any event
   whose target window is a widget. */
static void wigevt(wigptr wg, ami_evtrec* er)

{

    winptr win = wg->win;
    ami_long   lx, ly, n;

    switch (er->etype) {

        case ami_etfocus:
        case ami_etnofocus:
        case ami_etredraw:
            wigdrw(wg); /* focus look changed, or repaint */
            break;

        case ami_etmouba: /* click in the widget */
            if (!wg->enb) break; /* disabled, dead */
            lx = mousex-absx(win)+1; /* local position */
            ly = mousey-absy(win)+1;
            switch (wg->typ) {

                case wtbutton: wigsig(wg, ami_etbutton, 0); break;
                case wtcheckbox: wigsig(wg, ami_etchkbox, 0); break;
                case wtradio: wigsig(wg, ami_etradbut, 0); break;
                case wtscrollvert:
                    if (ly == 1) wigsig(wg, ami_etsclull, 0);
                    else if (ly == win->cmaxy) wigsig(wg, ami_etscldrl, 0);
                    else { /* in the track */

                        n = win->cmaxy-2;
                        if (n > 0) {

                            ami_long ts = wigmul(n, wg->sclsiz);
                            ami_long tp;
                            if (ts < 1) ts = 1;
                            tp = wigmul(n-ts, wg->val);
                            if (ly-2 >= tp && ly-2 < tp+ts)
                                drgwig = wg; /* on the thumb: drag it */
                            else if (ly-2 < tp) wigsig(wg, ami_etsclulp, 0);
                            else wigsig(wg, ami_etscldrp, 0);

                        }

                    }
                    break;
                case wtscrollhoriz:
                    if (lx == 1) wigsig(wg, ami_etsclull, 0);
                    else if (lx == win->cmaxx) wigsig(wg, ami_etscldrl, 0);
                    else { /* in the track */

                        n = win->cmaxx-2;
                        if (n > 0) {

                            ami_long ts = wigmul(n, wg->sclsiz);
                            ami_long tp;
                            if (ts < 1) ts = 1;
                            tp = wigmul(n-ts, wg->val);
                            if (lx-2 >= tp && lx-2 < tp+ts)
                                drgwig = wg; /* on the thumb: drag it */
                            else if (lx-2 < tp) wigsig(wg, ami_etsclulp, 0);
                            else wigsig(wg, ami_etscldrp, 0);

                        }

                    }
                    break;
                case wtnumselbox:
                    if (lx == 1 && wg->val > wg->low) wg->val--;
                    else if (lx == win->cmaxx && wg->val < wg->high) wg->val++;
                    else break;
                    wigdrw(wg);
                    wigsig(wg, ami_etnumbox, wg->val);
                    break;
                case wteditbox:
                    n = strlen(wg->face);
                    wg->curs = lx-1 <= n? lx-1: n; /* place cursor by click */
                    wigdrw(wg);
                    break;
                case wtlistbox:
                    if (ly >= 1 && wg->top+ly <= wg->listn) {

                        wg->sel = wg->top+ly;
                        wigdrw(wg);
                        wigsig(wg, ami_etlstbox, wg->sel);

                    }
                    break;
                case wtslidehoriz:
                    wg->val = wigscl(lx-1, win->cmaxx-1);
                    wigdrw(wg);
                    wigsig(wg, ami_etsldpos, wg->val);
                    drgwig = wg; /* and follow the mouse until release */
                    break;
                case wtslidevert:
                    wg->val = wigscl(ly-1, win->cmaxy-1);
                    wigdrw(wg);
                    wigsig(wg, ami_etsldpos, wg->val);
                    drgwig = wg; /* and follow the mouse until release */
                    break;
                case wtdropbox:
                    /* click toggles the drop list */
                    if (popcnt && popstk[popcnt-1]->owner == wg) clspops(0);
                    else {

                        clspops(0);
                        opnpop(wg->parent, absx(win), absy(win)+1,
                               wg->list, wg->listn, wg, NULL, wg->lcol);

                    }
                    break;
                case wtdropeditbox:
                    if (lx == win->cmaxx) { /* the drop arrow */

                        if (popcnt && popstk[popcnt-1]->owner == wg)
                            clspops(0);
                        else {

                            clspops(0);
                            opnpop(wg->parent, absx(win), absy(win)+1,
                                   wg->list, wg->listn, wg, NULL, wg->lcol);

                        }

                    } else { /* place the edit cursor */

                        n = strlen(wg->face);
                        wg->curs = lx-1 <= n? lx-1: n;
                        wigdrw(wg);

                    }
                    break;
                case wttabbar: { /* find the tab under the click */

                    ami_long i, p = 1, hit = 0;

                    if (wg->tor == ami_totop || wg->tor == ami_tobottom) {

                        for (i = 0; i < wg->listn && !hit; i++) {

                            ami_long tw = strlen(wg->list[i]);
                            if (lx >= p && lx < p+tw) hit = i+1;
                            p += tw+1; /* name and separator */

                        }

                    } else {

                        for (i = 0; i < wg->listn && !hit; i++) {

                            ami_long th = strlen(wg->list[i]);
                            if (ly >= p && ly < p+th) hit = i+1;
                            p += th+1;

                        }

                    }
                    if (hit && hit != wg->sel) {

                        wg->sel = hit;
                        wigdrw(wg);
                        wigsig(wg, ami_ettabbar, hit);

                    }
                    break;

                }
                case wtmenubar: { /* open a pulldown */

                    ami_long sx;
                    ami_menuptr mp = mbarhit(wg, lx, &sx);

                    clspops(0);
                    wg->sel = 0;
                    if (mp && menenb(wg->parent, mp->id)) {

                        if (mp->branch) { /* open the pulldown under it */

                            char** strs;
                            ami_long n = mencol(wg->parent, mp->branch, &strs);
                            ami_long i;

                            wg->sel = sx; /* show the open title */
                            opnpop(wg->parent, absx(win)+sx-1, absy(win)+1,
                                   strs, n, wg, mp->branch, NULL);
                            for (i = 0; i < n; i++) free(strs[i]);
                            free(strs);

                        } else { /* a bare top level item selects */

                            ami_evtrec er2;

                            er2.etype = ami_etmenus;
                            er2.menuid = mp->id;
                            intsendevent(wg->parent, &er2);

                        }

                    }
                    wigdrw(wg);
                    break;

                }
                case wtpopup: { /* selection in a popup list */

                    ami_long row = ly;
                    wigptr ow = wg->owner;

                    if (row < 1 || row > wg->listn) break;
                    if (wg->mitems) { /* a menu level */

                        ami_menuptr item = mennth(wg->mitems, row);
                        int depth;

                        if (!item || !menenb(wg->parent, item->id)) break;
                        /* find this popup's depth for cascade control */
                        for (depth = 0; depth < popcnt; depth++)
                            if (popstk[depth] == wg) break;
                        if (item->branch) { /* cascade a submenu */

                            char** strs;
                            ami_long n = mencol(wg->parent, item->branch, &strs);
                            ami_long i;

                            clspops(depth+1); /* drop deeper levels */
                            wg->sel = row;
                            wigdrw(wg);
                            opnpop(wg->parent,
                                   absx(win)+win->pmaxx-1, absy(win)+row-1,
                                   strs, n, wg->owner, item->branch, NULL);
                            for (i = 0; i < n; i++) free(strs[i]);
                            free(strs);

                        } else { /* an item: select and close the menu */

                            ami_evtrec er2;
                            winptr mown = wg->parent;

                            er2.etype = ami_etmenus;
                            er2.menuid = item->id;
                            clspops(0);
                            if (mown->mbar) {

                                mown->mbar->sel = 0;
                                wigdrw(mown->mbar);

                            }
                            intsendevent(mown, &er2);

                        }

                    } else if (ow) { /* a drop box list */

                        if (ow->typ == wtdropbox) {

                            ow->sel = row;
                            clspops(0);
                            wigdrw(ow);
                            wigsig(ow, ami_etdrpbox, row);

                        } else if (ow->typ == wtdropeditbox) {

                            snprintf(ow->face, MAXLIN, "%s", wg->list[row-1]);
                            ow->curs = strlen(ow->face);
                            clspops(0);
                            wigdrw(ow);
                            wigsig(ow, ami_etdrebox, 0);

                        }

                    }
                    break;

                }
                default: break;

            }
            break;

        case ami_etchar: /* keys to the focused widget */
            if (!wg->enb) break;
            if (wg->typ == wteditbox || wg->typ == wtdropeditbox) {

                n = strlen(wg->face);
                if (er->echar >= ' ' && er->echar != 0x7f && n < MAXLIN-1) {

                    /* insert at the cursor */
                    memmove(wg->face+wg->curs+1, wg->face+wg->curs,
                            n-wg->curs+1);
                    wg->face[wg->curs++] = er->echar;
                    wigdrw(wg);

                }

            } else if (wg->typ == wtnumselbox &&
                       er->echar >= '0' && er->echar <= '9') {

                /* digits shift into the value; a digit that would run
                   past the range starts a new number, so a full value
                   types over without clearing first */
                ami_long d = er->echar-'0';
                ami_long nv = wg->val*10+(wg->val < 0? -d: d);

                if (nv > wg->high || (wg->val < 0 && nv < wg->low))
                    nv = wg->val < 0? -d: d;
                if (nv <= wg->high) {

                    wg->val = nv;
                    wigdrw(wg);
                    /* a partial entry can sit under the range bottom;
                       it reports only once it is a valid value */
                    if (nv >= wg->low) wigsig(wg, ami_etnumbox, nv);

                }

            } else if (wg->typ == wtnumselbox && er->echar == '-' &&
                       wg->low < 0) {

                wg->val = -wg->val;
                wigdrw(wg);
                if (wg->val >= wg->low && wg->val <= wg->high)
                    wigsig(wg, ami_etnumbox, wg->val);

            } else if (er->echar == ' ') { /* space activates */

                if (wg->typ == wtbutton) wigsig(wg, ami_etbutton, 0);
                else if (wg->typ == wtcheckbox) wigsig(wg, ami_etchkbox, 0);
                else if (wg->typ == wtradio) wigsig(wg, ami_etradbut, 0);

            }
            break;

        case ami_etenter:
            if (!wg->enb) break;
            if (wg->typ == wteditbox) wigsig(wg, ami_etedtbox, 0);
            else if (wg->typ == wtdropeditbox) wigsig(wg, ami_etdrebox, 0);
            else if (wg->typ == wtnumselbox) {

                /* a partial entry left under the range clamps to it */
                if (wg->val < wg->low) { wg->val = wg->low; wigdrw(wg); }
                wigsig(wg, ami_etnumbox, wg->val);

            } else if (wg->typ == wtbutton) wigsig(wg, ami_etbutton, 0);
            break;

        case ami_etdelcb: /* backspace */
            if ((wg->typ == wteditbox || wg->typ == wtdropeditbox) &&
                wg->curs > 0) {

                n = strlen(wg->face);
                memmove(wg->face+wg->curs-1, wg->face+wg->curs, n-wg->curs+1);
                wg->curs--;
                wigdrw(wg);

            } else if (wg->typ == wtnumselbox) {

                wg->val /= 10; /* drop the last digit */
                wigdrw(wg);
                if (wg->val >= wg->low) wigsig(wg, ami_etnumbox, wg->val);

            }
            break;

        case ami_etdelcf: /* delete forward */
            if (wg->typ == wteditbox || wg->typ == wtdropeditbox) {

                n = strlen(wg->face);
                if (wg->curs < n) {

                    memmove(wg->face+wg->curs, wg->face+wg->curs+1,
                            n-wg->curs);
                    wigdrw(wg);

                }

            }
            break;

        case ami_etleft:
            if ((wg->typ == wteditbox || wg->typ == wtdropeditbox) &&
                wg->curs > 0)
                { wg->curs--; wigdrw(wg); }
            break;

        case ami_etright:
            if ((wg->typ == wteditbox || wg->typ == wtdropeditbox) &&
                wg->curs < (ami_long)strlen(wg->face))
                { wg->curs++; wigdrw(wg); }
            break;

        case ami_ethomel:
            if (wg->typ == wteditbox || wg->typ == wtdropeditbox)
                { wg->curs = 0; wigdrw(wg); }
            break;

        case ami_etendl:
            if (wg->typ == wteditbox || wg->typ == wtdropeditbox)
                { wg->curs = strlen(wg->face); wigdrw(wg); }
            break;

        case ami_etup:
            if (!wg->enb) break;
            if (wg->typ == wtnumselbox && wg->val < wg->high)
                { wg->val++; wigdrw(wg); wigsig(wg, ami_etnumbox, wg->val); }
            else if (wg->typ == wtlistbox && wg->sel > 1) {

                wg->sel--;
                /* scroll to keep the selection shown */
                if (wg->sel-1 < wg->top) wg->top = wg->sel-1;
                wigdrw(wg);
                wigsig(wg, ami_etlstbox, wg->sel);

            }
            break;

        case ami_etdown:
            if (!wg->enb) break;
            if (wg->typ == wtnumselbox && wg->val > wg->low)
                { wg->val--; wigdrw(wg); wigsig(wg, ami_etnumbox, wg->val); }
            else if (wg->typ == wtlistbox && wg->sel < wg->listn) {

                wg->sel++;
                /* scroll to keep the selection shown */
                if (wg->sel > wg->top+wg->win->cmaxy)
                    wg->top = wg->sel-wg->win->cmaxy;
                wigdrw(wg);
                wigsig(wg, ami_etlstbox, wg->sel);

            }
            break;

        default: break; /* other events are of no interest */

    }

}

/* create a widget: subwindow plus tracking record */
static wigptr wigcre(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id,
                     wigtyp typ)

{

    winptr par;  /* owning window */
    wigptr wg;   /* new widget */
    FILE*  wf;

    par = txt2win(f); /* index owner */
    if (!id) error("Invalid widget id");
    if (fndwig(par, id)) error("Widget by id already in use");
    if (x2 < x1 || y2 < y1) error("Invalid widget rectangle");
    wg = malloc(sizeof(wigrec));
    if (!wg) error("Out of memory");
    /* open the face window as an anonymous subwindow of the owner. It must
       not take the keyboard: focus moves to a widget by click, not by the
       widget being created */
    opnwig = TRUE;
    iopenwin(&stdin, &wf, f, igetwinid());
    opnwig = FALSE;
    wg->id = id;
    wg->typ = typ;
    wg->parent = par;
    wg->wf = wf;
    wg->win = txt2win(wf);
    wg->face = NULL;
    wg->list = NULL;
    wg->listn = 0;
    wg->enb = TRUE;
    wg->sel = FALSE;
    wg->val = 0;
    wg->low = 0;
    wg->high = 0;
    wg->sclsiz = LONG_MAX/8; /* nominal thumb */
    wg->marks = 0;
    wg->top = 0;
    wg->lcol = NULL;
    wg->fatt = 0;
    wg->curs = 0;
    wg->win->widget = TRUE; /* mark as widget window */
    wg->win->wig = wg;
    /* A widget face never shows the physical cursor. It takes the focus
       when clicked, and the cursor followed it and sat on the face -- on
       a scroll bar it showed riding the thumb as it dragged. */
    wg->win->curv = FALSE;
    wg->next = par->wiglst; /* push onto the owner's list */
    par->wiglst = wg;
    /* Shape the face: no decorations at all, sized and placed in owner
       client terms. The flags are set directly and the geometry recomputed
       once, rather than through the API calls, which would repaint after
       each flag. */
    wg->win->frame = FALSE;
    wg->win->size = FALSE;
    wg->win->sysbar = FALSE;
    recompcli(wg->win);
    intsetsiz(wg->win, x2-x1+1, y2-y1+1);
    intsetpos(wg->win, x1, y1); /* placed in the owner's client space */
    /* widget colors follow the owner */
    wg->win->fcolor = par->fcolor;
    wg->win->bcolor = par->bcolor;
    /* Implicit Z ordering: a component, a widget made to be layered
       under others, goes behind the controls of its owner. It is not
       sent to the back, which would bury a group made inside a group;
       instead the sibling controls are raised over it in their creation
       order, which keeps their own stacking intact. The list is newest
       first, so it is walked from the tail. */
    if (typ == wtgroup || typ == wtbackground || typ == wtprogbar) {

        wigptr p;    /* list walk */
        ami_long   cnt;  /* widgets on the owner */
        ami_long   i, j;

        cnt = 0;
        for (p = par->wiglst; p; p = p->next) cnt++;
        for (i = cnt-1; i >= 0; i--) {

            p = par->wiglst;
            for (j = 0; j < i; j++) p = p->next;
            if (p != wg && p->typ != wtgroup && p->typ != wtbackground &&
                p->typ != wtprogbar) fronttree(p->win);

        }

    }

    return (wg);

}

/* set the face text of a widget */
static void wigfac(wigptr wg, const char* s)

{

    if (wg->face) free(wg->face);
    wg->face = malloc(strlen(s)+1);
    if (!wg->face) error("Out of memory");
    strcpy(wg->face, s);

}

/** ****************************************************************************

Widget API

*******************************************************************************/

ami_long ami_getwigid(FILE* f)

{

    winptr win = txt2win(f);
    ami_long   id = -1;

    while (fndwig(win, id)) id--; /* find a free negative id */

    return (id);

}

void ami_killwidget(FILE* f, ami_long id)

{

    winptr win = txt2win(f);
    wigptr wg = fndwig(win, id);
    wigptr* lp;

    if (!wg) error("No widget by given id");
    if (hovwig == wg) hovwig = NULL; /* the highlight dies with it */
    /* unlink from the owner */
    lp = &win->wiglst;
    while (*lp != wg) lp = &(*lp)->next;
    *lp = wg->next;
    fclose(wg->wf); /* close the face window, which erases it */
    if (wg->face) free(wg->face);
    if (wg->lcol) free(wg->lcol);
    if (wg->list) {

        ami_long i;
        for (i = 0; i < wg->listn; i++) free(wg->list[i]);
        free(wg->list);

    }
    free(wg);

}

void ami_selectwidget(FILE* f, ami_long id, ami_long e)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    wg->sel = !!e;
    wigdrw(wg);

}

void ami_enablewidget(FILE* f, ami_long id, ami_long e)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    wg->enb = !!e;
    /* a disabled widget takes no hover highlight */
    if (!wg->enb && hovwig == wg) hovwig = NULL;
    wigdrw(wg);

}

void ami_getwidgettext(FILE* f, ami_long id, char* s, ami_long sl)

{

    wigptr wg = fndwig(txt2win(f), id);
    ami_long   l;

    if (!wg) error("No widget by given id");
    l = wg->face? strlen(wg->face): 0;
    /* critical output buffer: error if it does not fit, no terminator on
       an exact fit */
    if (l > sl) error("String too large for destination");
    if (l) memcpy(s, wg->face, l);
    if (l < sl) s[l] = 0;

}

void ami_putwidgettext(FILE* f, ami_long id, char* s)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    wigfac(wg, s);
    wg->curs = strlen(s);
    wigdrw(wg);

}

void ami_sizwidget(FILE* f, ami_long id, ami_long x, ami_long y)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    intsetsiz(wg->win, x, y);
    wigdrw(wg);

}

void ami_poswidget(FILE* f, ami_long id, ami_long x, ami_long y)

{

    winptr win = txt2win(f);
    wigptr wg = fndwig(win, id);

    if (!wg) error("No widget by given id");
    intsetpos(wg->win, x, y); /* placed in the owner's client space */

}

void ami_backwidget(FILE* f, ami_long id)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    backtree(wg->win); /* behind its siblings, above its owner */

}

void ami_frontwidget(FILE* f, ami_long id)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    fronttree(wg->win);

}

void ami_focuswidget(FILE* f, ami_long id)

{

    wigptr wg = fndwig(txt2win(f), id);
    ami_evtrec er;

    if (!wg) error("No widget by given id");
    remfocus(); /* drop focus elsewhere */
    wg->win->focus = TRUE;
    curfocus = wg->win;
    er.etype = ami_etfocus; /* let the widget show it */
    intsendevent(wg->win, &er);
    setcur(wg->win); /* and place the cursor */

}

void ami_buttonsiz(FILE* f, char* s, ami_long* w, ami_long* h)

{

    *w = strlen(s)+4; /* label, brackets and breathing room */
    *h = 1;

}

void ami_button(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtbutton);

    wigfac(wg, s);
    wigdrw(wg);

}

void ami_checkboxsiz(FILE* f, char* s, ami_long* w, ami_long* h)

{

    *w = strlen(s)+4; /* box, space and label */
    *h = 1;

}

void ami_checkbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtcheckbox);

    wigfac(wg, s);
    wigdrw(wg);

}

void ami_radiobuttonsiz(FILE* f, char* s, ami_long* w, ami_long* h)

{

    *w = strlen(s)+4;
    *h = 1;

}

void ami_radiobutton(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s,
                     ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtradio);

    wigfac(wg, s);
    wigdrw(wg);

}

void ami_groupsiz(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h,
                  ami_long* ox, ami_long* oy)

{

    ami_long tw = strlen(s)+4; /* title needs the top run */

    *w = cw+2; /* client plus frame */
    if (*w < tw) *w = tw;
    *h = ch+2;
    *ox = 1; /* client offset within the group */
    *oy = 1;

}

void ami_group(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtgroup);

    wigfac(wg, s);
    wigdrw(wg);

}

void ami_background(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtbackground);

    wigfac(wg, "");
    wigdrw(wg);

}

void ami_scrollvertsiz(FILE* f, ami_long* w, ami_long* h)

{

    *w = 1; /* a column */
    *h = 4; /* arrows and some track; usually overridden by the caller */

}

void ami_scrollvert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtscrollvert);

    wigfac(wg, "");
    wigdrw(wg);

}

void ami_scrollhorizsiz(FILE* f, ami_long* w, ami_long* h)

{

    *w = 4;
    *h = 1;

}

void ami_scrollhoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtscrollhoriz);

    wigfac(wg, "");
    wigdrw(wg);

}

void ami_scrollpos(FILE* f, ami_long id, ami_long r)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    if (r < 0) r = 0;
    wg->val = r;
    wigdrw(wg);

}

void ami_scrollsiz(FILE* f, ami_long id, ami_long r)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    if (r < 0) r = 0;
    wg->sclsiz = r;
    wigdrw(wg);

}

void ami_numselboxsiz(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)

{

    char buf[40];
    ami_long wl, wu;

    snprintf(buf, sizeof(buf), "%lld", AMI_LONG_CAST(l)); wl = strlen(buf);
    snprintf(buf, sizeof(buf), "%lld", AMI_LONG_CAST(u)); wu = strlen(buf);
    if (wl > wu) wu = wl;
    *w = wu+2; /* digits plus the spin cells */
    *h = 1;

}

void ami_numselbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u,
                   ami_long id)

{

    wigptr wg;

    if (l > u) error("Invalid number range");
    wg = wigcre(f, x1, y1, x2, y2, id, wtnumselbox);
    wg->low = l;
    wg->high = u;
    wg->val = l;
    wigfac(wg, "");
    wigdrw(wg);

}

void ami_editboxsiz(FILE* f, char* s, ami_long* w, ami_long* h)

{

    *w = strlen(s)+2;
    *h = 1;

}

void ami_editbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wteditbox);

    wg->face = malloc(MAXLIN); /* edit content buffer */
    if (!wg->face) error("Out of memory");
    wg->face[0] = 0;
    wigdrw(wg);

}

void ami_progbarsiz(FILE* f, ami_long* w, ami_long* h)

{

    *w = 10;
    *h = 1;

}

void ami_progbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtprogbar);

    wigfac(wg, "");
    wigdrw(wg);

}

void ami_progbarpos(FILE* f, ami_long id, ami_long pos)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    if (pos < 0) pos = 0;
    wg->val = pos;
    wigdrw(wg);

}

void ami_listboxsiz(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)

{

    ami_long mw = 0, n = 0, l;

    while (sp) {

        l = strlen(sp->str);
        if (l > mw) mw = l;
        n++;
        sp = sp->next;

    }
    *w = mw? mw: 1;
    *h = n? n: 1;

}

void ami_listbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                 ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtlistbox);
    ami_strptr p;
    ami_long n = 0, i;

    for (p = sp; p; p = p->next) n++; /* count strings */
    wg->list = malloc(sizeof(char*)*(n? n: 1));
    if (!wg->list) error("Out of memory");
    i = 0;
    for (p = sp; p; p = p->next) {

        wg->list[i] = malloc(strlen(p->str)+1);
        if (!wg->list[i]) error("Out of memory");
        strcpy(wg->list[i], p->str);
        i++;

    }
    wg->listn = n;
    wg->sel = 0; /* no selection */
    wigfac(wg, "");
    wigdrw(wg);

}

void ami_slidehorizsiz(FILE* f, ami_long* w, ami_long* h)

{

    *w = 10;
    *h = 1;

}

void ami_slidehoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
                    ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtslidehoriz);

    wg->marks = mark;
    wigfac(wg, "");
    wigdrw(wg);

}

void ami_slidevertsiz(FILE* f, ami_long* w, ami_long* h)

{

    *w = 1;
    *h = 10;

}

void ami_slidevert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
                   ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtslidevert);

    wg->marks = mark;
    wigfac(wg, "");
    wigdrw(wg);

}

/** ****************************************************************************

Drop box, drop edit box and tab bar API

*******************************************************************************/

/* copy a string list into a widget */
static void wiglst(wigptr wg, ami_strptr sp)

{

    ami_strptr p;
    ami_long n = 0, i = 0;

    for (p = sp; p; p = p->next) n++;
    wg->list = malloc(sizeof(char*)*(n? n: 1));
    if (!wg->list) error("Out of memory");
    for (p = sp; p; p = p->next) {

        wg->list[i] = malloc(strlen(p->str)+1);
        if (!wg->list[i]) error("Out of memory");
        strcpy(wg->list[i], p->str);
        i++;

    }
    wg->listn = n;

}

/* widest string in a list */
static ami_long lstwid(ami_strptr sp)

{

    ami_long w = 0;

    while (sp) {

        if ((ami_long)strlen(sp->str) > w) w = strlen(sp->str);
        sp = sp->next;

    }

    return (w);

}

/* count of strings in a list */
static ami_long lstcnt(ami_strptr sp)

{

    ami_long n = 0;

    while (sp) { n++; sp = sp->next; }

    return (n);

}

void ami_dropboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow,
                    ami_long* oh)

{

    *cw = lstwid(sp)+1; /* the face: widest entry and the arrow */
    *ch = 1;
    *ow = lstwid(sp)+3; /* the open list: entries and the frame */
    *oh = lstcnt(sp)+2;

}

void ami_dropbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                 ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtdropbox);

    wiglst(wg, sp);
    wg->sel = wg->listn? 1: 0; /* first entry shows by default */
    wigfac(wg, "");
    wigdrw(wg);

}

void ami_dropeditboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow,
                        ami_long* oh)

{

    *cw = lstwid(sp)+1;
    *ch = 1;
    *ow = lstwid(sp)+3;
    *oh = lstcnt(sp)+2;

}

void ami_dropeditbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2,
                     ami_strptr sp, ami_long id)

{

    wigptr wg = wigcre(f, x1, y1, x2, y2, id, wtdropeditbox);

    wiglst(wg, sp);
    wg->face = malloc(MAXLIN); /* edit content */
    if (!wg->face) error("Out of memory");
    wg->face[0] = 0;
    if (wg->listn) { /* start on the first entry */

        snprintf(wg->face, MAXLIN, "%s", wg->list[0]);
        wg->curs = strlen(wg->face);

    }
    wigdrw(wg);

}

void ami_tabbarsiz(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h,
                   ami_long* ox, ami_long* oy)

{

    if (tor == ami_totop || tor == ami_tobottom) {

        *w = cw+2; /* client plus the frame sides */
        *h = ch+3; /* client, the tab row and the frame */
        *ox = 1;
        *oy = tor == ami_totop? 2: 0;

    } else {

        *w = cw+3; /* client, the tab column and the frame */
        *h = ch+2;
        *ox = tor == ami_toleft? 2: 0;
        *oy = 1;

    }

}

void ami_tabbarclient(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw,
                      ami_long* ch, ami_long* ox, ami_long* oy)

{

    if (tor == ami_totop || tor == ami_tobottom) {

        *cw = w-2;
        *ch = h-3;
        *ox = 1;
        *oy = tor == ami_totop? 2: 0;

    } else {

        *cw = w-3;
        *ch = h-2;
        *ox = tor == ami_toleft? 2: 0;
        *oy = 1;

    }

}

void ami_tabbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                ami_tabori tor, ami_long id)

{

    wigptr wg;

    /* The bar occupies only the tab strip: a row for top or bottom
       orientation, a column for left or right. The client area beside it
       belongs to the caller, which is what tabbarclient describes. */
    if (tor == ami_totop) y2 = y1;
    else if (tor == ami_tobottom) y1 = y2;
    else if (tor == ami_toleft) x2 = x1;
    else x1 = x2;
    wg = wigcre(f, x1, y1, x2, y2, id, wttabbar);
    wg->tor = tor;
    wiglst(wg, sp);
    wg->sel = wg->listn? 1: 0; /* first tab selected */
    wigfac(wg, "");
    wigdrw(wg);

}

void ami_tabsel(FILE* f, ami_long id, ami_long tn)

{

    wigptr wg = fndwig(txt2win(f), id);

    if (!wg) error("No widget by given id");
    if (tn < 1 || tn > wg->listn) error("Invalid tab number");
    wg->sel = tn;
    wigdrw(wg);

}

/** ****************************************************************************

Menus

The menu bar is a widget across the top of the window's client area. Its
pulldowns are popups, cascading for submenus. Selections are returned to the
client as etmenus events, as the graphical implementations do.

*******************************************************************************/

/* deep copy a menu definition: the interface promises the caller's
   structure is free for reuse after menu() returns */
static ami_menuptr cpymenu(ami_menuptr m)

{

    ami_menuptr  root = NULL;
    ami_menuptr* lp = &root;

    while (m) {

        ami_menuptr e = malloc(sizeof(ami_menurec));
        if (!e) error("Out of memory");
        e->next = NULL;
        e->branch = cpymenu(m->branch);
        /* The definition's onoff is the capability flag: the item HAS
           on/off highlighting. In the working copy the field is the check
           state itself, which starts clear and moves only by menusel(). */
        e->onoff = FALSE;
        e->oneof = m->oneof;
        e->bar = m->bar;
        e->id = m->id;
        e->face = malloc(strlen(m->face)+1);
        if (!e->face) error("Out of memory");
        strcpy(e->face, m->face);
        *lp = e; lp = &e->next;
        m = m->next;

    }

    return (root);

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

/*******************************************************************************

Menu select mode

The keyboard at a window's menu, as doc/menu_select.md has it. The menu
key -- F11 on a terminal, since xterm keeps F10 for itself -- puts the
keyboard at the first title of the bar, which stands reversed as an open
title does. Left and Right walk the bar and stop at its ends. Down, or
Return, opens the pulldown under the title and picks its first row; Up
and Down walk a list; Right opens the submenu of a row that has one, and
Left closes the list the keyboard is in and comes back out to what
opened it. Return on a row is the item chosen: the window gets the same
menu event a click would have sent, and the mode ends. Cancel ends it
with nothing chosen, and so does a click, which then does what a click
does.

*******************************************************************************/

static int      mensel;      /* the keyboard is at a menu */
static winptr   menselwin;   /* the window whose bar it is at */
static ami_long menselidx;   /* the title picked on the bar, 1 based */

/* a bar title's start column, and how many titles there are */
static ami_long mbarcol(wigptr wg, ami_long idx)

{

    ami_menuptr p;
    ami_long    x = 1;

    for (p = wg->mitems; p && idx > 1; p = p->next, idx--) x += strlen(p->face)+2;

    return (x);

}

static ami_long mbarcount(wigptr wg)

{

    ami_menuptr p;
    ami_long    n = 0;

    for (p = wg->mitems; p; p = p->next) n++;

    return (n);

}

/* the mode starts at the first title of the bar of the focus window, or
   of the nearest window up from it that has one; FALSE when none does */
static int menselstart(void)

{

    winptr w = curfocus;

    while (w && !w->mbar) w = w->parwin;
    if (!w) return (FALSE);
    clspops(0);
    mensel = TRUE;
    menselwin = w;
    menselidx = 1;
    w->mbar->sel = mbarcol(w->mbar, 1);
    wigdrw(w->mbar);

    return (TRUE);

}

/* the mode ends: the lists go and the bar is at rest */
static void menselend(void)

{

    if (!mensel) return;
    clspops(0);
    if (menselwin->mbar) { menselwin->mbar->sel = 0; wigdrw(menselwin->mbar); }
    mensel = FALSE;
    menselwin = NULL;

}

/* a click ends the mode. On the bar or a list the click acts on the menu
   and tidies the bar itself; anywhere else the bar is put at rest here. */
static void menselmouse(winptr win)

{

    wigptr bar;

    if (!mensel) return;
    bar = menselwin->mbar;
    mensel = FALSE;
    menselwin = NULL;
    if (bar && !(win && win->widget &&
                 (win->wig->typ == wtmenubar || win->wig->typ == wtpopup))) {

        clspops(0);
        bar->sel = 0;
        wigdrw(bar);

    }

}

/* The pulldown under the title picked opens, or the submenu of the row
   picked in the list the keyboard is in, and its first row is picked. A
   row without a submenu, or a disabled one, opens nothing. */
static void menselopen(void)

{

    wigptr      bar = menselwin->mbar;
    wigptr      pop;
    ami_menuptr item;
    char**      strs;
    ami_long    n, i;

    if (!popcnt) {

        item = mennth(bar->mitems, menselidx);
        if (!item || !item->branch || !menenb(menselwin, item->id)) return;
        n = mencol(menselwin, item->branch, &strs);
        pop = opnpop(menselwin, absx(bar->win)+bar->sel-1, absy(bar->win)+1,
                     strs, n, bar, item->branch, NULL);

    } else {

        pop = popstk[popcnt-1];
        item = mennth(pop->mitems, pop->sel);
        if (!item || !item->branch || !menenb(menselwin, item->id)) return;
        n = mencol(menselwin, item->branch, &strs);
        pop = opnpop(menselwin, absx(pop->win)+pop->win->pmaxx-1,
                     absy(pop->win)+pop->sel-1, strs, n, bar, item->branch, NULL);

    }
    for (i = 0; i < n; i++) free(strs[i]);
    free(strs);
    pop->sel = 1;
    wigdrw(pop);

}

/* a key while the mode is on: TRUE if the key was one of the mode's */
static int menselkey(ami_long etype)

{

    wigptr      bar, pop;
    ami_menuptr item;

    if (!mensel) return (FALSE);
    bar = menselwin->mbar;
    pop = popcnt? popstk[popcnt-1]: NULL;
    switch (etype) {

        case ami_etcan: menselend(); return (TRUE);
        case ami_etleft:
            if (pop) clspops(popcnt-1); /* out of this list, to what opened it */
            else if (menselidx > 1) {

                menselidx--;
                bar->sel = mbarcol(bar, menselidx);
                wigdrw(bar);

            }
            return (TRUE);
        case ami_etright:
            if (pop) menselopen();
            else if (menselidx < mbarcount(bar)) {

                menselidx++;
                bar->sel = mbarcol(bar, menselidx);
                wigdrw(bar);

            }
            return (TRUE);
        case ami_etup:
            if (pop && pop->sel > 1) { pop->sel--; wigdrw(pop); }
            return (TRUE);
        case ami_etdown:
            if (!pop) menselopen();
            else if (pop->sel < pop->listn) { pop->sel++; wigdrw(pop); }
            return (TRUE);
        case ami_etenter:
            item = pop? mennth(pop->mitems, pop->sel): mennth(bar->mitems, menselidx);
            if (!item) return (TRUE);
            if (item->branch) menselopen();
            else if (menenb(menselwin, item->id)) {

                /* the item chosen: what a click on it sends */
                ami_evtrec er;
                winptr     w = menselwin;

                memset(&er, 0, sizeof(er));
                er.etype = ami_etmenus;
                er.menuid = item->id;
                menselend();
                intsendevent(w, &er);

            }
            return (TRUE);
        default: return (FALSE);

    }

}

static void imenu(FILE* f, ami_menuptr m)

{

    winptr win = txt2win(f);
    wigptr wg;
    ami_long   id;

    if (mensel && menselwin == win) { /* the menu goes: the keyboard's turn at it is over */

        clspops(0);
        mensel = FALSE;
        menselwin = NULL;

    }
    if (win->mbar) { /* remove the previous bar */

        wigptr* lp = &win->wiglst;
        while (*lp && *lp != win->mbar) lp = &(*lp)->next;
        if (*lp) *lp = win->mbar->next;
        fclose(win->mbar->wf);
        if (win->mbar->face) free(win->mbar->face);
        free(win->mbar);
        win->mbar = NULL;

    }
    /* the definition is copied, per the interface: the caller's structure
       is not used after this call */
    if (win->amenu) frmenu(win->amenu);
    win->amenu = cpymenu(m);
    if (!m) { /* menu removed: the client gets its row back */

        recompcli(win);
        restore(win);
        annresize(win);
        annredraw(win);
        return;

    }
    /* The bar gets its own row of the frame, below the system bar and its
       underbar, which stay as they are -- the graphical form keeps the
       system bar untouched and the client simply starts below the menu.
       The client area shrinks by the row, which decory() accounts once
       the bar is on, framed or not. */
    id = ami_getwigid(f);
    wg = wigcre(f, 1, 1, win->cmaxx, 1, id, wtmenubar);
    wg->mitems = win->amenu;
    wg->sel = 0;
    wigfac(wg, "");
    /* the bar carries its own background, setting it off from the client */
    wg->win->bcolor = ami_cyan;
    win->mbar = wg;
    recompcli(win); /* the client gives up a row to the menu */
    intsetpos(wg->win, 1, 0); /* the row just above the client */
    restore(win); /* redraw the window under the new layout */
    annresize(win);
    annredraw(win);
    wigdrw(wg);

}

static void imenuena(FILE* f, ami_long id, ami_long onoff)

{

    winptr    win = txt2win(f);
    menenaptr me;

    for (me = win->menena; me; me = me->next) if (me->id == id) break;
    if (!me) { /* no entry yet, make one */

        me = malloc(sizeof(menena));
        if (!me) error("Out of memory");
        me->id = id;
        me->next = win->menena;
        win->menena = me;

    }
    me->ena = !!onoff;
    if (win->mbar) wigdrw(win->mbar);

}

/* find a menu item by id anywhere in a menu tree */
static ami_menuptr fndmen(ami_menuptr m, ami_long id)

{

    ami_menuptr p, r;

    for (p = m; p; p = p->next) {

        if (p->id == id) return (p);
        if (p->branch) { r = fndmen(p->branch, id); if (r) return (r); }

    }

    return (NULL);

}

/* find the sibling list containing the item with the given id, searching
   the branches */
static ami_menuptr fndmenlist(ami_menuptr root, ami_long id)

{

    ami_menuptr p, r;

    for (p = root; p; p = p->next) if (p->id == id) return (root);
    for (p = root; p; p = p->next)
        if (p->branch) { r = fndmenlist(p->branch, id); if (r) return (r); }

    return (NULL);

}

static void imenusel(FILE* f, ami_long id, ami_long select)

{

    winptr      win = txt2win(f);
    ami_menuptr lst = fndmenlist(win->amenu, id);
    ami_menuptr p, gs, ge, q;

    if (!lst) error("No menu item by given id");
    /* The item's "one of" group is the contiguous run of items carrying
       the oneof flag, closed by the item that ends the run -- the last
       member is unflagged, by the interface convention. Walk the sibling
       list tracking where the run holding the item begins. */
    gs = lst; p = lst;
    while (p->id != id) {

        if (!p->oneof) gs = p->next; /* a run closed; the next begins after */
        p = p->next;

    }
    if (p->oneof || gs != p) { /* in a group: clear the other members */

        ge = p;
        while (ge->oneof && ge->next) ge = ge->next; /* find the run's close */
        for (q = gs; ; q = q->next) {

            if (q != p) q->onoff = FALSE;
            if (q == ge) break;

        }

    }
    p->onoff = !!select;
    if (win->mbar) wigdrw(win->mbar);

}

static void istdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)

{

    static const struct { ami_long sel; ami_long id; char* face; } std[] = {

        { AMI_SMNEW,        AMI_SMNEW,        "New" },
        { AMI_SMOPEN,       AMI_SMOPEN,       "Open" },
        { AMI_SMCLOSE,      AMI_SMCLOSE,      "Close" },
        { AMI_SMSAVE,       AMI_SMSAVE,       "Save" },
        { AMI_SMSAVEAS,     AMI_SMSAVEAS,     "Save As" },
        { AMI_SMPAGESET,    AMI_SMPAGESET,    "Page Setup" },
        { AMI_SMPRINT,      AMI_SMPRINT,      "Print" },
        { AMI_SMEXIT,       AMI_SMEXIT,       "Exit" },
        { AMI_SMUNDO,       AMI_SMUNDO,       "Undo" },
        { AMI_SMCUT,        AMI_SMCUT,        "Cut" },
        { AMI_SMPASTE,      AMI_SMPASTE,      "Paste" },
        { AMI_SMDELETE,     AMI_SMDELETE,     "Delete" },
        { AMI_SMFIND,       AMI_SMFIND,       "Find" },
        { AMI_SMFINDNEXT,   AMI_SMFINDNEXT,   "Find Next" },
        { AMI_SMREPLACE,    AMI_SMREPLACE,    "Replace" },
        { AMI_SMGOTO,       AMI_SMGOTO,       "Goto" },
        { AMI_SMSELECTALL,  AMI_SMSELECTALL,  "Select All" },
        { AMI_SMNEWWINDOW,  AMI_SMNEWWINDOW,  "New Window" },
        { AMI_SMTILEHORIZ,  AMI_SMTILEHORIZ,  "Tile Horizontally" },
        { AMI_SMTILEVERT,   AMI_SMTILEVERT,   "Tile Vertically" },
        { AMI_SMCASCADE,    AMI_SMCASCADE,    "Cascade" },
        { AMI_SMCLOSEALL,   AMI_SMCLOSEALL,   "Close All" },
        { AMI_SMHELPTOPIC,  AMI_SMHELPTOPIC,  "Help Topics" },
        { AMI_SMABOUT,      AMI_SMABOUT,      "About" },

    };
    /* which standard items belong to which top level list */
    static const ami_long filist[] = { AMI_SMNEW, AMI_SMOPEN, AMI_SMCLOSE,
        AMI_SMSAVE, AMI_SMSAVEAS, AMI_SMPAGESET, AMI_SMPRINT, AMI_SMEXIT, 0 };
    static const ami_long edlist[] = { AMI_SMUNDO, AMI_SMCUT, AMI_SMPASTE,
        AMI_SMDELETE, AMI_SMFIND, AMI_SMFINDNEXT, AMI_SMREPLACE, AMI_SMGOTO,
        AMI_SMSELECTALL, 0 };
    static const ami_long wilist[] = { AMI_SMNEWWINDOW, AMI_SMTILEHORIZ,
        AMI_SMTILEVERT, AMI_SMCASCADE, AMI_SMCLOSEALL, 0 };
    static const ami_long helist[] = { AMI_SMHELPTOPIC, AMI_SMABOUT, 0 };
    static const struct { const ami_long* lst; char* face; } tops[] = {

        { filist, "File" }, { edlist, "Edit" }, { NULL, NULL },
        { wilist, "Window" }, { helist, "Help" },

    };
    ami_menuptr root = NULL, rtl = NULL;
    ami_long ti, i;

    /* Build the standard lists, in the documented order:
       file edit <program> window help */
    for (ti = 0; ti < 5; ti++) {

        ami_menuptr sub = NULL, subl = NULL, top;

        if (!tops[ti].lst) { /* the program's own menu goes here */

            if (pm) {

                if (rtl) rtl->next = pm; else root = pm;
                /* walk to the end of the program list */
                rtl = pm;
                while (rtl->next) rtl = rtl->next;

            }
            continue;

        }
        for (i = 0; tops[ti].lst[i]; i++) {

            ami_long sel = tops[ti].lst[i];
            ami_long si;

            if (!(sms & (1L<<sel))) continue; /* not selected */
            for (si = 0; si < AMI_SMMAX; si++) if (std[si].sel == sel) break;
            if (si >= AMI_SMMAX) continue;
            {
                ami_menuptr e = malloc(sizeof(ami_menurec));
                if (!e) error("Out of memory");
                e->next = NULL;
                e->branch = NULL;
                e->onoff = FALSE;
                e->oneof = FALSE;
                e->bar = FALSE;
                e->id = std[si].id;
                e->face = std[si].face;
                if (subl) subl->next = e; else sub = e;
                subl = e;
            }

        }
        if (sub) { /* the list has entries, make its top level entry */

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

/** ****************************************************************************

                            STANDARD DIALOGS

Unlike the graphical implementations, which call up host system dialogs,
these are presented inside the terminal surface as managed windows built
from the character widgets. Each runs its own event loop until dismissed,
which is what makes them modal.

A dialog that is terminated from outside, by the window system rather than
by its own controls, passes the terminate on to the program after closing,
so a quit request is not swallowed by the dialog.

*******************************************************************************/

/* Run a dialog window until its done flag is set. Returns the id of the
   widget that ended it, or 0 for an outside terminate. */
static ami_long dlgloop(FILE* wf, winptr dwin, ami_long okid, ami_long cancelid,
                    void (*lay)(FILE* wf, winptr dwin, void* ctx),
                    void (*evt)(FILE* wf, winptr dwin, ami_evtrec* er,
                                void* ctx),
                    void* ctx)

{

    ami_evtrec er;
    ami_long   res = 0;
    int        done = FALSE;
    int        realterm = FALSE;

    ifocus(wf); /* the dialog takes the focus */
    do {

        ievent(stdin, &er);
        /* Everything here minds its window id: dialogs nest (the font
           query opens the color query), and both loops read the same
           queue, so an event for the one must not act on the other. */
        if (er.etype == ami_etterm) { realterm = TRUE; done = TRUE; }
        else if (er.etype == ami_etresize && er.winid == dwin->wid && lay)
            /* the dialog was resized: flow the layout to the new client */
            lay(wf, dwin, ctx);
        else if (er.etype == ami_etbutton && er.winid == dwin->wid) {

            if (er.butid == okid || er.butid == cancelid) {

                res = er.butid;
                done = TRUE;

            } else if (evt) evt(wf, dwin, &er, ctx); /* its own extras */

        } else if ((er.etype == ami_etedtbox || er.etype == ami_etdrebox) &&
                   er.winid == dwin->wid) {

            /* enter in a text field completes as the ok button does */
            res = okid;
            done = TRUE;

        } else if (er.etype == ami_etchkbox && er.winid == dwin->wid) {

            /* A checkbox reports its click and leaves the state to its
               owner, which a program answers with selectwidget. The
               dialog owns these, so it checks and unchecks them here. */
            wigptr wg = fndwig(dwin, er.ckbxid);

            if (wg) {

                wg->sel = !wg->sel;
                wigdrw(wg);

            }

        } else if (evt) evt(wf, dwin, &er, ctx); /* to the dialog's logic */

    } while (!done);
    if (realterm) { /* pass the terminate on to the program */

        er.etype = ami_etterm;
        enquepaevt(&er);

    }

    return (res);

}

/* Create a dialog window centered on the surface. Returns its file, and
   the window record through the pointer. The given size is in the terms
   the dialogs were laid out in, sysbar only: the sizing border grows the
   window around it, keeping the client those layouts expect. */
static FILE* dlgcre(char* title, ami_long w, ami_long h, winptr* dwin)

{

    FILE*  wf;
    winptr win;

    iopenwin(&stdin, &wf, NULL, igetwinid()); /* parentless: floats */
    win = txt2win(wf);
    win->frame = TRUE;
    win->size = TRUE; /* a complete frame... */
    win->fixed = TRUE; /* ...but a dialog does not resize */
    win->sysbar = TRUE;
    win->curv = FALSE; /* a dialog never shows the cursor */
    recompcli(win);
    w += 2; /* the sizing border */
    h += 2;
    if (w > dimx) w = dimx;
    if (h > dimy) h = dimy;
    intsetsiz(win, w, h);
    intsetpos(win, (dimx-w)/2+1, (dimy-h)/2+1);
    ititle(wf, title);
    intfront(win);
    *dwin = win;

    return (wf);

}

/* flow the alert to its client: the message at the top, the button
   centered on the bottom row */
static void alertlay(FILE* wf, winptr dwin, void* ctx)

{

    char* message = (char*)ctx;
    ami_long  bw, bh, bx;

    fprintf(wf, "\f");
    ami_cursor(wf, 2, 2);
    fprintf(wf, "%.*s", (int)(dwin->cmaxx-2), message);
    ami_buttonsiz(wf, "Ok", &bw, &bh);
    bx = (dwin->cmaxx-bw)/2+1;
    if (bx < 1) bx = 1;
    ami_poswidget(wf, 1, bx, dwin->cmaxy-1);

}

void ami_alert(char* title, char* message)

{

    FILE*  wf;
    winptr dwin;
    ami_long   w, h, bw, bh;
    ami_long   ml = strlen(message);
    ami_long   tl = strlen(title);

    w = (ml > tl? ml: tl)+6;
    if (w < 20) w = 20;
    if (w > dimx-2) w = dimx-2;
    h = 7;
    wf = dlgcre(title, w, h, &dwin);
    ami_buttonsiz(wf, "Ok", &bw, &bh);
    ami_button(wf, (dwin->cmaxx-bw)/2+1, dwin->cmaxy-1,
                   (dwin->cmaxx-bw)/2+bw, dwin->cmaxy-1, "Ok", 1);
    alertlay(wf, dwin, message);
    dlgloop(wf, dwin, 1, 0, alertlay, NULL, message);
    fclose(wf);

}

/* The color query holds a full 24 bit color, 8 bits per component. The
   preset list picks the primaries, the R, G and B sliders enter any
   color directly, the luminosity slider runs the color from black to
   white holding its hue, and the swatch shows the result exactly. */
typedef struct { ami_long r, g, b; } qcolst;

/* the presets, matching the primary color codes */
static char* const qcolnam[] = { "black", "white", "red", "green", "blue",
                                 "cyan", "yellow", "magenta" };
static const ami_long qcolpre[8][3] = {

    { 0, 0, 0 }, { 255, 255, 255 }, { 255, 0, 0 }, { 0, 255, 0 },
    { 0, 0, 255 }, { 0, 255, 255 }, { 255, 255, 0 }, { 255, 0, 255 },

};

/* rgb (0..255) to hsl (h 0..6, s and l 0..1) and back, for the
   luminosity slider: lightness moves, the hue holds */
static void rgbhsl(ami_long r, ami_long g, ami_long b, double* h, double* s, double* l)

{

    double rr = r/255.0, gg = g/255.0, bb = b/255.0;
    double mx = rr, mn = rr, d;

    if (gg > mx) mx = gg;
    if (bb > mx) mx = bb;
    if (gg < mn) mn = gg;
    if (bb < mn) mn = bb;
    d = mx-mn;
    *l = (mx+mn)/2;
    *s = 0;
    *h = 0;
    if (d > 0) {

        *s = *l > 0.5? d/(2-mx-mn): d/(mx+mn);
        if (mx == rr) *h = (gg-bb)/d+(gg < bb? 6: 0);
        else if (mx == gg) *h = (bb-rr)/d+2;
        else *h = (rr-gg)/d+4;

    }

}

static double qcolhue(double p, double q, double t)

{

    if (t < 0) t += 6;
    if (t >= 6) t -= 6;
    if (t < 1) return (p+(q-p)*t);
    if (t < 3) return (q);
    if (t < 4) return (p+(q-p)*(4-t));

    return (p);

}

static void hslrgb(double h, double s, double l, ami_long* r, ami_long* g, ami_long* b)

{

    if (s <= 0) *r = *g = *b = (ami_long)(l*255+0.5);
    else {

        double q = l < 0.5? l*(1+s): l+s-l*s;
        double p = 2*l-q;

        *r = (ami_long)(qcolhue(p, q, h+2)*255+0.5);
        *g = (ami_long)(qcolhue(p, q, h)*255+0.5);
        *b = (ami_long)(qcolhue(p, q, h-2)*255+0.5);

    }

}

/* the luminosity of the held color, 0..255 */
static ami_long qcollum(qcolst* st)

{

    ami_long mx = st->r, mn = st->r;

    if (st->g > mx) mx = st->g;
    if (st->b > mx) mx = st->b;
    if (st->g < mn) mn = st->g;
    if (st->b < mn) mn = st->b;

    return ((mx+mn)/2);

}

/* place a slider on an 8 bit value */
static void qcolsld(winptr dwin, ami_long id, ami_long v8)

{

    wigptr wg = fndwig(dwin, id);

    if (wg) {

        wg->val = wigscl(v8, 255);
        wigdrw(wg);

    }

}

/* keep the preset list marking the held color: selected while the color
   is one of its entries, unmarked while it is not */
static void qcolsel(winptr dwin, qcolst* st)

{

    wigptr wg = fndwig(dwin, 1);
    ami_long   i, sel = 0;

    for (i = 0; i < 8; i++)
        if (st->r == qcolpre[i][0] && st->g == qcolpre[i][1] &&
            st->b == qcolpre[i][2]) sel = i+1;
    if (wg && wg->sel != sel) {

        wg->sel = sel;
        wigdrw(wg);

    }

}

/* show the held color: the swatch paints it exactly, and the component
   readouts follow the sliders */
static void qcolshow(FILE* wf, winptr dwin, qcolst* st)

{

    ami_long x, y;

    ami_bcolorc(wf, col8full(st->r), col8full(st->g), col8full(st->b));
    for (y = 3; y <= 10; y++) {

        ami_cursor(wf, 36, y);
        for (x = 36; x <= 54; x++) fputc(' ', wf);

    }
    ami_bcolor(wf, ami_white);
    ami_cursor(wf, 47, 12); fprintf(wf, "%3lld", AMI_LONG_CAST(st->r));
    ami_cursor(wf, 47, 14); fprintf(wf, "%3lld", AMI_LONG_CAST(st->g));
    ami_cursor(wf, 47, 16); fprintf(wf, "%3lld", AMI_LONG_CAST(st->b));

}

/* flow the color query face */
static void qrycollay(FILE* wf, winptr dwin, void* ctx)

{

    qcolst* st = (qcolst*)ctx;
    ami_long    i, n = dwin->cmaxx;

    fprintf(wf, "\f");
    /* the section header, marked off and centered */
    ami_cursor(wf, 1, 1);
    for (i = 1; i <= n; i++) fputc('-', wf);
    ami_cursor(wf, (n-7)/2+1, 1);
    fprintf(wf, " Color ");
    /* the swatch border: the swatch starts white, the dialog's own
       background, and without this there is no telling it is there */
    ami_cursor(wf, 35, 2);
    fputc('+', wf);
    for (i = 36; i <= 54; i++) fputc('-', wf);
    fputc('+', wf);
    for (i = 3; i <= 10; i++) {

        ami_cursor(wf, 35, i); fputc('|', wf);
        ami_cursor(wf, 55, i); fputc('|', wf);

    }
    ami_cursor(wf, 35, 11);
    fputc('+', wf);
    for (i = 36; i <= 54; i++) fputc('-', wf);
    fputc('+', wf);
    ami_cursor(wf, 3, 12); fprintf(wf, "Red");
    ami_cursor(wf, 3, 14); fprintf(wf, "Green");
    ami_cursor(wf, 3, 16); fprintf(wf, "Blue");
    ami_cursor(wf, 3, 18); fprintf(wf, "Lum");
    ami_poswidget(wf, 2, 3, dwin->cmaxy);
    ami_poswidget(wf, 3, 12, dwin->cmaxy);
    qcolshow(wf, dwin, st);

}

/* the dialog's live logic: presets, component sliders and the luminosity
   slider all hold one color, and every control follows it */
static void qrycolevt(FILE* wf, winptr dwin, ami_evtrec* er, void* ctx)

{

    qcolst* st = (qcolst*)ctx;
    double  h, s, l;

    if (er->winid != dwin->wid) return; /* dialogs nest: not ours */
    if (er->etype == ami_etlstbox && er->lstbid == 1) {

        if (er->lstbsl >= 1 && er->lstbsl <= 8) {

            st->r = qcolpre[er->lstbsl-1][0];
            st->g = qcolpre[er->lstbsl-1][1];
            st->b = qcolpre[er->lstbsl-1][2];
            qcolsld(dwin, 4, st->r);
            qcolsld(dwin, 5, st->g);
            qcolsld(dwin, 6, st->b);
            qcolsld(dwin, 9, qcollum(st));
            qcolshow(wf, dwin, st);

        }

    } else if (er->etype == ami_etsldpos) {

        ami_long v8 = wigmul(255, er->sldpos);

        if (er->sldpid >= 4 && er->sldpid <= 6) {

            if (er->sldpid == 4) st->r = v8;
            else if (er->sldpid == 5) st->g = v8;
            else st->b = v8;
            qcolsld(dwin, 9, qcollum(st));
            qcolsel(dwin, st);
            qcolshow(wf, dwin, st);

        } else if (er->sldpid == 9) {

            rgbhsl(st->r, st->g, st->b, &h, &s, &l);
            hslrgb(h, s, v8/255.0, &st->r, &st->g, &st->b);
            qcolsld(dwin, 4, st->r);
            qcolsld(dwin, 5, st->g);
            qcolsld(dwin, 6, st->b);
            qcolsel(dwin, st);
            qcolshow(wf, dwin, st);

        }

    }

}

void ami_querycolor(ami_long* r, ami_long* g, ami_long* b)

{

    FILE*  wf;
    winptr dwin;
    ami_long   res;
    ami_strrec sr[8];
    ami_long   i, bw, bh;
    qcolst st;

    for (i = 0; i < 8; i++) {

        sr[i].str = qcolnam[i];
        sr[i].next = i < 7? &sr[i+1]: NULL;

    }
    /* the color passed in is where the dialog starts */
    st.r = colc8(*r);
    st.g = colc8(*g);
    st.b = colc8(*b);
    wf = dlgcre("Choose color", 56, 23, &dwin);
    ami_listbox(wf, 3, 3, 13, 10, &sr[0], 1);
    ami_slidehoriz(wf, 10, 12, 44, 12, 0, 4);
    ami_slidehoriz(wf, 10, 14, 44, 14, 0, 5);
    ami_slidehoriz(wf, 10, 16, 44, 16, 0, 6);
    ami_slidehoriz(wf, 10, 18, 44, 18, 0, 9);
    ami_buttonsiz(wf, "Ok", &bw, &bh);
    ami_button(wf, 3, dwin->cmaxy, 3+bw-1, dwin->cmaxy, "Ok", 2);
    ami_button(wf, 12, dwin->cmaxy, 12+bw+4, dwin->cmaxy, "Cancel", 3);
    qcolsld(dwin, 4, st.r);
    qcolsld(dwin, 5, st.g);
    qcolsld(dwin, 6, st.b);
    qcolsld(dwin, 9, qcollum(&st));
    qcolsel(dwin, &st);
    qrycollay(wf, dwin, &st);
    res = dlgloop(wf, dwin, 2, 3, qrycollay, qrycolevt, &st);
    if (res == 2) { /* accepted */

        *r = col8full(st.r);
        *g = col8full(st.g);
        *b = col8full(st.b);

    }
    fclose(wf);

}

/* The file queries are a browser: the current directory across the top,
   the directories within it in a pane at the left, the files within it
   in a pane at the right, each with a scroll bar, and the name field at
   the bottom. Picking a directory enters it, picking a file takes its
   name, and a name can always just be typed. */
#define QFPTH 1024 /* directory path length */
typedef struct {

    char dir[QFPTH];  /* the directory the browser is in */
    char home[QFPTH]; /* the directory the dialog opened in */

} qfilst;

/* replace a list box's contents */
static void lstput(winptr dwin, ami_long id, char** strs, ami_long n)

{

    wigptr wg = fndwig(dwin, id);
    ami_long   i;

    if (!wg) return;
    if (wg->list) {

        for (i = 0; i < wg->listn; i++) free(wg->list[i]);
        free(wg->list);

    }
    if (wg->lcol) { free(wg->lcol); wg->lcol = NULL; }
    wg->list = malloc(sizeof(char*)*(n? n: 1));
    if (!wg->list) error("Out of memory");
    for (i = 0; i < n; i++) {

        wg->list[i] = malloc(strlen(strs[i])+1);
        if (!wg->list[i]) error("Out of memory");
        strcpy(wg->list[i], strs[i]);

    }
    wg->listn = n;
    wg->sel = 0;
    wg->top = 0;
    wigdrw(wg);

}

/* set a pane's scroll bar from its list: the thumb is the visible share,
   at the scrolled position */
static void qfilbar(winptr dwin, ami_long lid, ami_long bid)

{

    wigptr lw = fndwig(dwin, lid);
    wigptr bw = fndwig(dwin, bid);
    ami_long   vis, max;

    if (!lw || !bw) return;
    vis = lw->win->cmaxy;
    max = lw->listn-vis;
    if (max < 0) max = 0;
    bw->sclsiz = lw->listn > vis? wigscl(vis, lw->listn): LONG_MAX;
    bw->val = max? wigscl(lw->top, max): 0;
    wigdrw(bw);

}

/* scroll a pane by its scroll bar's report */
static void qfilscl(winptr dwin, ami_long lid, ami_long bid, ami_evtcod e, ami_long pos)

{

    wigptr lw = fndwig(dwin, lid);
    ami_long   vis, max;

    if (!lw) return;
    vis = lw->win->cmaxy;
    max = lw->listn-vis;
    if (max < 0) max = 0;
    switch (e) {

        case ami_etsclull: lw->top--; break;
        case ami_etscldrl: lw->top++; break;
        case ami_etsclulp: lw->top -= vis; break;
        case ami_etscldrp: lw->top += vis; break;
        case ami_etsclpos: lw->top = wigmul(max, pos); break;
        default: break;

    }
    if (lw->top < 0) lw->top = 0;
    if (lw->top > max) lw->top = max;
    wigdrw(lw);
    qfilbar(dwin, lid, bid);

}

/* show the directory the browser is in, tail first when it is long */
static void qfilpath(FILE* wf, winptr dwin, qfilst* st)

{

    ami_long max = dwin->cmaxx-8;
    ami_long l = strlen(st->dir);
    ami_long i;

    ami_cursor(wf, 2, 1);
    fprintf(wf, "Path: ");
    if (l <= max) {

        fprintf(wf, "%s", st->dir);
        for (i = l; i < max; i++) fputc(' ', wf); /* blank the rest */

    } else fprintf(wf, "...%s", st->dir+l-(max-3));

}

/* Read the browser's directory into the panes: the directories within it
   at the left, dot-dot included, the files at the right, both sorted.
   Hidden entries stay hidden. */
static int qfilcmp(const void* a, const void* b)

{

    return (strcmp(*(char* const*)a, *(char* const*)b));

}

static void qfilread(FILE* wf, winptr dwin, qfilst* st)

{

    DIR*           dp;
    struct dirent* de;
    struct stat    sb;
    char**         nm[2] = { NULL, NULL }; /* directories, then files */
    ami_long       nc[2] = { 0, 0 };
    ami_long       na[2] = { 0, 0 };
    char           pth[QFPTH+300];
    ami_long       i, w;

    dp = opendir(st->dir);
    if (dp) {

        while ((de = readdir(dp))) {

            if (de->d_name[0] == '.' && strcmp(de->d_name, ".."))
                continue; /* hidden, and the directory itself */
            snprintf(pth, sizeof(pth), "%s/%s", st->dir, de->d_name);
            if (stat(pth, &sb)) continue; /* unreadable: not offered */
            w = !S_ISDIR(sb.st_mode);
            if (nc[w] == na[w]) {

                na[w] = na[w]? na[w]*2: 32;
                nm[w] = realloc(nm[w], sizeof(char*)*na[w]);
                if (!nm[w]) error("Out of memory");

            }
            nm[w][nc[w]] = malloc(strlen(de->d_name)+1);
            if (!nm[w][nc[w]]) error("Out of memory");
            strcpy(nm[w][nc[w]++], de->d_name);

        }
        closedir(dp);

    }
    for (w = 0; w < 2; w++) {

        qsort(nm[w], nc[w], sizeof(char*), qfilcmp);
        lstput(dwin, 4+w, nm[w], nc[w]);
        qfilbar(dwin, 4+w, 6+w);
        for (i = 0; i < nc[w]; i++) free(nm[w][i]);
        free(nm[w]);

    }
    qfilpath(wf, dwin, st);

}

/* enter a directory from the pane */
static void qfilnav(FILE* wf, winptr dwin, qfilst* st, const char* name)

{

    if (!strcmp(name, "..")) { /* up: drop the last component */

        char* p = strrchr(st->dir, '/');

        if (p && p != st->dir) *p = 0;
        else strcpy(st->dir, "/");

    } else {

        ami_long l = strlen(st->dir);

        if (l+1+(ami_long)strlen(name)+1 > QFPTH) return; /* cannot hold it */
        snprintf(st->dir+l, QFPTH-l, "%s%s",
                 l && st->dir[l-1] == '/'? "": "/", name);

    }
    qfilread(wf, dwin, st);

}

/* flow the file query face */
static void qryfilelay(FILE* wf, winptr dwin, void* ctx)

{

    qfilst* st = (qfilst*)ctx;

    fprintf(wf, "\f");
    ami_cursor(wf, 2, 3);
    fprintf(wf, "Directories:");
    ami_cursor(wf, 26, 3);
    fprintf(wf, "Files:");
    ami_cursor(wf, 2, 16);
    fprintf(wf, "File name:");
    ami_poswidget(wf, 2, 3, dwin->cmaxy);
    ami_poswidget(wf, 3, 12, dwin->cmaxy);
    qfilpath(wf, dwin, st);

}

/* the browser's live logic */
static void qryfileevt(FILE* wf, winptr dwin, ami_evtrec* er, void* ctx)

{

    qfilst* st = (qfilst*)ctx;
    wigptr  wg;
    ami_long    bid = 0, pos = 0;

    if (er->winid != dwin->wid) return; /* dialogs nest: not ours */
    switch (er->etype) {

        case ami_etlstbox:
            if (er->lstbid == 4) { /* a directory: enter it */

                wg = fndwig(dwin, 4);
                if (wg && er->lstbsl >= 1 && er->lstbsl <= wg->listn)
                    qfilnav(wf, dwin, st, wg->list[er->lstbsl-1]);

            } else if (er->lstbid == 5) { /* a file: take its name */

                wg = fndwig(dwin, 5);
                if (wg && er->lstbsl >= 1 && er->lstbsl <= wg->listn)
                    ami_putwidgettext(wf, 1, wg->list[er->lstbsl-1]);

            }
            break;

        case ami_etsclull: bid = er->sclulid; break;
        case ami_etscldrl: bid = er->scldrid; break;
        case ami_etsclulp: bid = er->sclupid; break;
        case ami_etscldrp: bid = er->scldpid; break;
        case ami_etsclpos: bid = er->sclpid; pos = er->sclpos; break;
        default: break;

    }
    if (bid == 6) qfilscl(dwin, 4, 6, er->etype, pos);
    else if (bid == 7) qfilscl(dwin, 5, 7, er->etype, pos);

}

/* shared file name query for open and save */
static void qryfile(char* title, char* s, ami_long sl)

{

    FILE*  wf;
    winptr dwin;
    ami_long   res, bw, bh;
    wigptr wg;
    qfilst st;

    if (!getcwd(st.dir, sizeof(st.dir))) strcpy(st.dir, "/");
    strcpy(st.home, st.dir);
    wf = dlgcre(title, 58, 22, &dwin);
    ami_listbox(wf, 2, 4, 20, 14, NULL, 4);
    ami_scrollvert(wf, 22, 4, 22, 14, 6);
    ami_listbox(wf, 26, 4, 52, 14, NULL, 5);
    ami_scrollvert(wf, 54, 4, 54, 14, 7);
    ami_editbox(wf, 2, 17, 57, 17, 1);
    /* seed with what the caller passed, if anything */
    if (sl > 0 && *s) ami_putwidgettext(wf, 1, s);
    ami_buttonsiz(wf, "Ok", &bw, &bh);
    ami_button(wf, 3, dwin->cmaxy, 3+bw-1, dwin->cmaxy, "Ok", 2);
    ami_button(wf, 12, dwin->cmaxy, 12+bw+4, dwin->cmaxy, "Cancel", 3);
    qryfilelay(wf, dwin, &st);
    qfilread(wf, dwin, &st);
    ami_focuswidget(wf, 1); /* start in the name field */
    res = dlgloop(wf, dwin, 2, 3, qryfilelay, qryfileevt, &st);
    if (res == 2) { /* accepted: hand back the name */

        wg = fndwig(dwin, 1);
        if (wg && wg->face && *wg->face) {

            char  pth[QFPTH+MAXLIN];
            char* rp = wg->face;
            ami_long  l;

            /* A name picked where the dialog opened, or typed absolute,
               goes back as it stands; browsing elsewhere hands back the
               full path there. */
            if (rp[0] != '/' && strcmp(st.dir, st.home)) {

                snprintf(pth, sizeof(pth), "%s%s%s", st.dir,
                         !strcmp(st.dir, "/")? "": "/", rp);
                rp = pth;

            }
            l = strlen(rp);
            if (l > sl) error("String too large for destination");
            memcpy(s, rp, l);
            if (l < sl) s[l] = 0;

        }

    }
    fclose(wf);

}

void ami_queryopen(char* s, ami_long sl) { qryfile("Open file", s, sl); }

void ami_querysave(char* s, ami_long sl) { qryfile("Save file", s, sl); }

/* flow the find query: the search field takes the width, the option
   boxes hold their rows, the buttons keep the bottom row */
static void qryfndlay(FILE* wf, winptr dwin, void* ctx)

{

    ami_long ew;

    fprintf(wf, "\f");
    ami_cursor(wf, 2, 2);
    fprintf(wf, "Find:");
    ew = dwin->cmaxx-2;
    if (ew < 1) ew = 1;
    ami_sizwidget(wf, 1, ew, 1);
    ami_poswidget(wf, 1, 2, 3);
    ami_poswidget(wf, 2, 3, dwin->cmaxy);
    ami_poswidget(wf, 3, 12, dwin->cmaxy);

}

void ami_queryfind(char* s, ami_long sl, ami_qfnopts* opt)

{

    FILE*  wf;
    winptr dwin;
    ami_long   res, bw, bh;
    wigptr wg;
    ami_long   w = dimx-10;

    if (w > 50) w = 50;
    if (w < 30) w = 30;
    wf = dlgcre("Find", w, 11, &dwin);
    ami_editbox(wf, 2, 3, dwin->cmaxx-1, 3, 1);
    if (sl > 0 && *s) ami_putwidgettext(wf, 1, s);
    ami_checkbox(wf, 2, 5, 20, 5, "Match case", 4);
    ami_checkbox(wf, 2, 6, 20, 6, "Search up", 5);
    ami_checkbox(wf, 2, 7, 24, 7, "Regular expression", 6);
    ami_selectwidget(wf, 4, !!(*opt & (1L<<ami_qfncase)));
    ami_selectwidget(wf, 5, !!(*opt & (1L<<ami_qfnup)));
    ami_selectwidget(wf, 6, !!(*opt & (1L<<ami_qfnre)));
    ami_buttonsiz(wf, "Ok", &bw, &bh);
    ami_button(wf, 3, 9, 3+bw-1, 9, "Ok", 2);
    ami_button(wf, 12, 9, 12+bw+4, 9, "Cancel", 3);
    qryfndlay(wf, dwin, NULL);
    ami_focuswidget(wf, 1);
    res = dlgloop(wf, dwin, 2, 3, qryfndlay, NULL, NULL);
    if (res == 2) {

        wg = fndwig(dwin, 1);
        if (wg && wg->face) {

            ami_long l = strlen(wg->face);
            if (l > sl) error("String too large for destination");
            memcpy(s, wg->face, l);
            if (l < sl) s[l] = 0;

        }
        *opt = 0; /* rebuild the option set from the boxes */
        wg = fndwig(dwin, 4); if (wg && wg->sel) *opt |= 1L<<ami_qfncase;
        wg = fndwig(dwin, 5); if (wg && wg->sel) *opt |= 1L<<ami_qfnup;
        wg = fndwig(dwin, 6); if (wg && wg->sel) *opt |= 1L<<ami_qfnre;

    }
    fclose(wf);

}

/* flow the find/replace query: both fields take the width, the option
   boxes hold their rows, the buttons keep the bottom row */
static void qryfrplay(FILE* wf, winptr dwin, void* ctx)

{

    ami_long ew;

    fprintf(wf, "\f");
    ami_cursor(wf, 2, 2);
    fprintf(wf, "Find:");
    ami_cursor(wf, 2, 5);
    fprintf(wf, "Replace with:");
    ew = dwin->cmaxx-2;
    if (ew < 1) ew = 1;
    ami_sizwidget(wf, 1, ew, 1);
    ami_poswidget(wf, 1, 2, 3);
    ami_sizwidget(wf, 7, ew, 1);
    ami_poswidget(wf, 7, 2, 6);
    ami_poswidget(wf, 2, 3, dwin->cmaxy);
    ami_poswidget(wf, 3, 12, dwin->cmaxy);

}

void ami_queryfindrep(char* s, ami_long sl, char* r, ami_long rl, ami_qfropts* opt)

{

    FILE*  wf;
    winptr dwin;
    ami_long   res, bw, bh;
    wigptr wg;
    ami_long   w = dimx-10;

    if (w > 50) w = 50;
    if (w < 30) w = 30;
    wf = dlgcre("Find and replace", w, 14, &dwin);
    ami_editbox(wf, 2, 3, dwin->cmaxx-1, 3, 1);
    if (sl > 0 && *s) ami_putwidgettext(wf, 1, s);
    ami_editbox(wf, 2, 6, dwin->cmaxx-1, 6, 7);
    if (rl > 0 && *r) ami_putwidgettext(wf, 7, r);
    ami_checkbox(wf, 2, 8, 20, 8, "Match case", 4);
    ami_checkbox(wf, 2, 9, 20, 9, "Search up", 5);
    ami_checkbox(wf, 2, 10, 24, 10, "All in line", 6);
    ami_selectwidget(wf, 4, !!(*opt & (1L<<ami_qfrcase)));
    ami_selectwidget(wf, 5, !!(*opt & (1L<<ami_qfrup)));
    ami_selectwidget(wf, 6, !!(*opt & (1L<<ami_qfralllin)));
    ami_buttonsiz(wf, "Ok", &bw, &bh);
    ami_button(wf, 3, 12, 3+bw-1, 12, "Ok", 2);
    ami_button(wf, 12, 12, 12+bw+4, 12, "Cancel", 3);
    qryfrplay(wf, dwin, NULL);
    ami_focuswidget(wf, 1);
    res = dlgloop(wf, dwin, 2, 3, qryfrplay, NULL, NULL);
    if (res == 2) {

        wg = fndwig(dwin, 1);
        if (wg && wg->face) {

            ami_long l = strlen(wg->face);
            if (l > sl) error("String too large for destination");
            memcpy(s, wg->face, l);
            if (l < sl) s[l] = 0;

        }
        wg = fndwig(dwin, 7);
        if (wg && wg->face) {

            ami_long l = strlen(wg->face);
            if (l > rl) error("String too large for destination");
            memcpy(r, wg->face, l);
            if (l < rl) r[l] = 0;

        }
        *opt = 0;
        wg = fndwig(dwin, 4); if (wg && wg->sel) *opt |= 1L<<ami_qfrcase;
        wg = fndwig(dwin, 5); if (wg && wg->sel) *opt |= 1L<<ami_qfrup;
        wg = fndwig(dwin, 6); if (wg && wg->sel) *opt |= 1L<<ami_qfralllin;

    }
    fclose(wf);

}

/* The font query holds the terminal's one font at its one size; what it
   chooses are the colors and the effects. The color names show in their
   colors, the effect names show in their effects, and the rgb buttons
   open the full color query for a 24 bit pick, shown in the swatch by
   each color's row. */
typedef struct { ami_long fr, fg, fb, br, bg, bb; } qfntst;

/* the drop entries take the preset colors, so each name shows in it */
static void qfntcol(winptr dwin, ami_long id)

{

    static const ami_color pc[8] = {

        ami_black, ami_white, ami_red, ami_green, ami_blue, ami_cyan,
        ami_yellow, ami_magenta

    };
    wigptr wg = fndwig(dwin, id);
    ami_long   i;

    if (!wg) return;
    if (wg->lcol) free(wg->lcol);
    wg->lcol = malloc(sizeof(ami_color)*8);
    if (!wg->lcol) error("Out of memory");
    for (i = 0; i < 8; i++) wg->lcol[i] = pc[i];
    wigdrw(wg);

}

/* the effect faces show their own effects */
static void qfnteff(winptr dwin, ami_long id, ami_long att)

{

    wigptr wg = fndwig(dwin, id);

    if (!wg) return;
    wg->fatt = att;
    wigdrw(wg);

}

/* keep a color drop box marking the held color: selected while it is a
   preset, unmarked while it is a 24 bit pick */
static void qfntsel(winptr dwin, ami_long id, ami_long r, ami_long g, ami_long b)

{

    wigptr wg = fndwig(dwin, id);
    ami_long   i, sel = 0;

    for (i = 0; i < 8; i++)
        if (r == qcolpre[i][0] && g == qcolpre[i][1] && b == qcolpre[i][2])
            sel = i+1;
    if (wg && wg->sel != sel) {

        wg->sel = sel;
        wigdrw(wg);

    }

}

/* the swatches by each color row show the held colors exactly */
static void qfntshow(FILE* wf, winptr dwin, qfntst* st)

{

    ami_bcolorc(wf, col8full(st->fr), col8full(st->fg), col8full(st->fb));
    ami_cursor(wf, 38, 2);
    fprintf(wf, "   ");
    ami_bcolorc(wf, col8full(st->br), col8full(st->bg), col8full(st->bb));
    ami_cursor(wf, 38, 4);
    fprintf(wf, "   ");
    ami_bcolor(wf, ami_white);

}

/* flow the font query face */
static void qryfntlay(FILE* wf, winptr dwin, void* ctx)

{

    qfntst* st = (qfntst*)ctx;

    fprintf(wf, "\f");
    ami_cursor(wf, 2, 2);
    fprintf(wf, "Foreground:");
    ami_cursor(wf, 2, 4);
    fprintf(wf, "Background:");
    ami_cursor(wf, 2, 6);
    fprintf(wf, "Effects:");
    ami_poswidget(wf, 2, 3, dwin->cmaxy);
    ami_poswidget(wf, 3, 12, dwin->cmaxy);
    qfntshow(wf, dwin, st);

}

/* the dialog's live logic: the drop boxes pick presets, the rgb buttons
   open the full color query over this one */
static void qryfntevt(FILE* wf, winptr dwin, ami_evtrec* er, void* ctx)

{

    qfntst* st = (qfntst*)ctx;
    ami_long    r, g, b;

    if (er->winid != dwin->wid) return; /* dialogs nest: not ours */
    if (er->etype == ami_etdrpbox) {

        if (er->drpbsl >= 1 && er->drpbsl <= 8) {

            if (er->drpbid == 1) {

                st->fr = qcolpre[er->drpbsl-1][0];
                st->fg = qcolpre[er->drpbsl-1][1];
                st->fb = qcolpre[er->drpbsl-1][2];

            } else if (er->drpbid == 8) {

                st->br = qcolpre[er->drpbsl-1][0];
                st->bg = qcolpre[er->drpbsl-1][1];
                st->bb = qcolpre[er->drpbsl-1][2];

            }
            qfntshow(wf, dwin, st);

        }

    } else if (er->etype == ami_etbutton) {

        if (er->butid == 11) { /* a 24 bit foreground */

            r = col8full(st->fr);
            g = col8full(st->fg);
            b = col8full(st->fb);
            ami_querycolor(&r, &g, &b);
            st->fr = colc8(r);
            st->fg = colc8(g);
            st->fb = colc8(b);
            qfntsel(dwin, 1, st->fr, st->fg, st->fb);
            qfntshow(wf, dwin, st);

        } else if (er->butid == 12) { /* a 24 bit background */

            r = col8full(st->br);
            g = col8full(st->bg);
            b = col8full(st->bb);
            ami_querycolor(&r, &g, &b);
            st->br = colc8(r);
            st->bg = colc8(g);
            st->bb = colc8(b);
            qfntsel(dwin, 8, st->br, st->bg, st->bb);
            qfntshow(wf, dwin, st);

        }

    }

}

void ami_queryfont(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb,
                   ami_long* br, ami_long* bg, ami_long* bb, ami_qfteffects* effect)

{

    FILE*  wf;
    winptr dwin;
    ami_long   res, bw, bh;
    wigptr wg;
    /* A character terminal has one font at one size, so the font and size
       are reported back unchanged; what can be chosen are the colors and
       the effects the terminal can actually present. */
    static char* const cnam[] = { "black", "white", "red", "green", "blue",
                                  "cyan", "yellow", "magenta" };
    ami_strrec fr_[8], br_[8];
    ami_long i;
    qfntst st;

    for (i = 0; i < 8; i++) {

        fr_[i].str = cnam[i]; fr_[i].next = i < 7? &fr_[i+1]: NULL;
        br_[i].str = cnam[i]; br_[i].next = i < 7? &br_[i+1]: NULL;

    }
    /* the colors passed in are where the dialog starts */
    st.fr = colc8(*fr); st.fg = colc8(*fg); st.fb = colc8(*fb);
    st.br = colc8(*br); st.bg = colc8(*bg); st.bb = colc8(*bb);
    wf = dlgcre("Font", 46, 16, &dwin);
    ami_dropbox(wf, 14, 2, 26, 2, &fr_[0], 1);
    ami_dropbox(wf, 14, 4, 26, 4, &br_[0], 8);
    qfntcol(dwin, 1);
    qfntcol(dwin, 8);
    qfntsel(dwin, 1, st.fr, st.fg, st.fb);
    qfntsel(dwin, 8, st.br, st.bg, st.bb);
    ami_buttonsiz(wf, "rgb", &bw, &bh);
    ami_button(wf, 29, 2, 29+bw-1, 2, "rgb", 11);
    ami_button(wf, 29, 4, 29+bw-1, 4, "rgb", 12);
    ami_checkbox(wf, 2, 7, 20, 7, "Bold", 4);
    ami_checkbox(wf, 2, 8, 20, 8, "Underline", 5);
    ami_checkbox(wf, 2, 9, 20, 9, "Italic", 6);
    ami_checkbox(wf, 2, 10, 20, 10, "Reverse", 9);
    ami_checkbox(wf, 2, 11, 20, 11, "Blink", 10);
    qfnteff(dwin, 4, BIT(sabold));
    qfnteff(dwin, 5, BIT(saundl));
    qfnteff(dwin, 6, BIT(saital));
    qfnteff(dwin, 9, BIT(sarev));
    qfnteff(dwin, 10, BIT(sablink));
    ami_selectwidget(wf, 4, !!(*effect & (1L<<ami_qftebold)));
    ami_selectwidget(wf, 5, !!(*effect & (1L<<ami_qfteunderline)));
    ami_selectwidget(wf, 6, !!(*effect & (1L<<ami_qfteitalic)));
    ami_selectwidget(wf, 9, !!(*effect & (1L<<ami_qftereverse)));
    ami_selectwidget(wf, 10, !!(*effect & (1L<<ami_qfteblink)));
    ami_buttonsiz(wf, "Ok", &bw, &bh);
    ami_button(wf, 3, 14, 3+bw-1, 14, "Ok", 2);
    ami_button(wf, 12, 14, 12+bw+4, 14, "Cancel", 3);
    qryfntlay(wf, dwin, &st);
    res = dlgloop(wf, dwin, 2, 3, qryfntlay, qryfntevt, &st);
    if (res == 2) {

        *fr = col8full(st.fr); *fg = col8full(st.fg); *fb = col8full(st.fb);
        *br = col8full(st.br); *bg = col8full(st.bg); *bb = col8full(st.bb);
        *effect = 0;
        wg = fndwig(dwin, 4); if (wg && wg->sel) *effect |= 1L<<ami_qftebold;
        wg = fndwig(dwin, 5);
        if (wg && wg->sel) *effect |= 1L<<ami_qfteunderline;
        wg = fndwig(dwin, 6); if (wg && wg->sel) *effect |= 1L<<ami_qfteitalic;
        wg = fndwig(dwin, 9);
        if (wg && wg->sel) *effect |= 1L<<ami_qftereverse;
        wg = fndwig(dwin, 10); if (wg && wg->sel) *effect |= 1L<<ami_qfteblink;

    }
    fclose(wf);

}

/** ****************************************************************************

Managerc startup

*******************************************************************************/

/* Managerc overrides the terminal API, so it must initialize after terminal
   (constructor 106) and deinitialize before it: 107 on both, since
   constructors run ascending and destructors descending. It was 104, which
   worked when terminal was 103; when terminal moved to 106 the order
   inverted and windowc captured null vectors at startup. */
static void init_windowc(void) __attribute__((constructor (107)));
static void init_windowc()

{

    int       fn;  /* file number */
    int       wid; /* window id */
    int       ofn; /* standard output file number */
    int       ifn; /* standard input file number */
    int       ti;  /* timer index */
    ami_evtcod e;

    winfre = NULL; /* clear free windows structure list */
    winlst = NULL; /* clear master window list */
    rootlst = NULL; /* clear root window list */
    ztop = -1; /* clear Z order top (none) */
    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */
    drgwin = NULL; /* set no drag active */
    drag = dt_none;
    paqfre = NULL; /* clear pa event free queue */
    paqevt = NULL; /* clear pa event input queue */

    /* clear open files tables */
    for (fn = 0; fn < MAXFIL; fn++) {

        opnfil[fn] = NULL; /* set unoccupied */
        /* clear file to window logical number translator table */
        filwin[fn] = -1; /* set unoccupied */

    }

    /* clear window equivalence table */
    for (fn = 0; fn < MAXFIL*2+1; fn++) {

        /* clear window logical number translator table */
        xltwin[fn] = -1; /* set unoccupied */

    }

    /* clear timer equivalence table */
    for (ti = 0; ti < AMI_MAXTIM; ti++) timtbl[ti] = NULL;

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_etmenus; e++) evthan[e] = defaultevent;

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

    /* override entry calls for API */
    _pa_cursor_ovr(icursor, &cursor_vect);
    _pa_maxx_ovr(imaxx, &maxx_vect);
    _pa_maxy_ovr(imaxy, &maxy_vect);
    _pa_home_ovr(ihome, &home_vect);
    _pa_del_ovr(idel, &del_vect);
    _pa_up_ovr(iup, &up_vect);
    _pa_down_ovr(idown, &down_vect);
    _pa_left_ovr(ileft, &left_vect);
    _pa_right_ovr(iright, &right_vect);
    _pa_blink_ovr(iblink, &blink_vect);
    _pa_reverse_ovr(ireverse, &reverse_vect);
    _pa_underline_ovr(iunderline, &underline_vect);
    _pa_superscript_ovr(isuperscript, &superscript_vect);
    _pa_subscript_ovr(isubscript, &subscript_vect);
    _pa_italic_ovr(iitalic, &italic_vect);
    _pa_bold_ovr(ibold, &bold_vect);
    _pa_strikeout_ovr(istrikeout, &strikeout_vect);
    _pa_standout_ovr(istandout, &standout_vect);
    _pa_fcolor_ovr(ifcolor, &fcolor_vect);
    _pa_bcolor_ovr(ibcolor, &bcolor_vect);
    _pa_auto_ovr(iauto, &auto_vect);
    _pa_curvis_ovr(icurvis, &curvis_vect);
    _pa_scroll_ovr(iscroll, &scroll_vect);
    _pa_curx_ovr(icurx, &curx_vect);
    _pa_cury_ovr(icury, &cury_vect);
    _pa_curbnd_ovr(icurbnd, &curbnd_vect);
    _pa_select_ovr(iselect, &select_vect);
    _pa_event_ovr(ievent, &event_vect);
    _pa_timer_ovr(itimer, &timer_vect);
    _pa_killtimer_ovr(ikilltimer, &killtimer_vect);
    _pa_mouse_ovr(imouse, &mouse_vect);
    _pa_mousebutton_ovr(imousebutton, &mousebutton_vect);
    _pa_joystick_ovr(ijoystick, &joystick_vect);
    _pa_joybutton_ovr(ijoybutton, &joybutton_vect);
    _pa_joyaxis_ovr(ijoyaxis, &joyaxis_vect);
    _pa_settab_ovr(isettab, &settab_vect);
    _pa_restab_ovr(irestab, &restab_vect);
    _pa_clrtab_ovr(iclrtab, &clrtab_vect);
    _pa_funkey_ovr(ifunkey, &funkey_vect);
    _pa_frametimer_ovr(iframetimer, &frametimer_vect);
    _pa_autohold_ovr(iautohold, &autohold_vect);
    _pa_wrtstr_ovr(iwrtstr, &wrtstr_vect);
    _pa_wrtstrn_ovr(iwrtstrn, &wrtstrn_vect);
    _pa_sizbuf_ovr(isizbuf, &sizbuf_vect);
    _pa_title_ovr(ititle, &title_vect);
    _pa_titlen_ovr(ititlen, &titlen_vect);
    _pa_fcolorc_ovr(ifcolorc, &fcolorc_vect);
    _pa_bcolorc_ovr(ibcolorc, &bcolorc_vect);
    _pa_eventover_ovr(ieventover, &eventover_vect);
    _pa_eventsover_ovr(ieventsover, &eventsover_vect);
    _pa_sendevent_ovr(isendevent, &sendevent_vect);
    _pa_openwin_ovr(iopenwin, &openwin_vect);
    _pa_buffer_ovr(ibuffer, &buffer_vect);
    _pa_getsiz_ovr(igetsiz, &getsiz_vect);
    _pa_setsiz_ovr(isetsiz, &setsiz_vect);
    _pa_setpos_ovr(isetpos, &setpos_vect);
    _pa_scnsiz_ovr(iscnsiz, &scnsiz_vect);
    _pa_scncen_ovr(iscncen, &scncen_vect);
    _pa_winclient_ovr(iwinclient, &winclient_vect);
    _pa_front_ovr(ifront, &front_vect);
    _pa_back_ovr(iback, &back_vect);
    _pa_frame_ovr(iframe, &frame_vect);
    _pa_sizable_ovr(isizable, &sizable_vect);
    _pa_sysbar_ovr(isysbar, &sysbar_vect);
    _pa_menu_ovr(imenu, &menu_vect);
    _pa_menuena_ovr(imenuena, &menuena_vect);
    _pa_menusel_ovr(imenusel, &menusel_vect);
    _pa_stdmenu_ovr(istdmenu, &stdmenu_vect);
    _pa_getwinid_ovr(igetwinid, &getwinid_vect);
    _pa_focus_ovr(ifocus, &focus_vect);

    /* find dimensions of base screen */
    dimx = (*maxx_vect)(stdout);
    dimy = (*maxy_vect)(stdout);

    /* reset all attributes */
    (*superscript_vect)(stdout, FALSE);
    (*subscript_vect)(stdout, FALSE);
    (*blink_vect)(stdout, FALSE);
    (*strikeout_vect)(stdout, FALSE);
    (*italic_vect)(stdout, FALSE);
    (*bold_vect)(stdout, FALSE);
    (*underline_vect)(stdout, FALSE);
    (*reverse_vect)(stdout, FALSE);
    attr = 0;

    /* set default colors */
    fcolor = ami_black;
    bcolor = ami_white;
    (*fcolor_vect)(stdout, fcolor);
    (*bcolor_vect)(stdout, bcolor);

    /* home cursor */
    (*home_vect)(stdout);
    curx = 1;
    cury = 1;

    /* set cursor on */
    (*curvis_vect)(stdout, TRUE);
    curon = TRUE;

    /* set no focus window */
    curfocus = NULL;

    /* set auto off */
    (*auto_vect)(stdout, FALSE);

    /* set mouse tracking invalid */
    mousex = -1;
    mousey = -1;

    /* open stdin and stdout as I/O window set */
    ifn = fileno(stdin); /* get logical id stdin */
    ofn = fileno(stdout); /* get logical id stdout */
    openio(stdin, stdout, ifn, ofn, -1, 1, FALSE, TRUE); /* process open */

}

/** ****************************************************************************

Managerc shutdown

*******************************************************************************/

static void deinit_windowc(void) __attribute__((destructor (107)));
static void deinit_windowc()

{

    int fn; /* file number */
    int i;

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

    /* holding copies of API override vectors */
    _pa_cursor_t cppcursor;
    _pa_maxx_t cppmaxx;
    _pa_maxy_t cppmaxy;
    _pa_home_t cpphome;
    _pa_del_t cppdel;
    _pa_up_t cppup;
    _pa_down_t cppdown;
    _pa_left_t cppleft;
    _pa_right_t cppright;
    _pa_blink_t cppblink;
    _pa_reverse_t cppreverse;
    _pa_underline_t cppunderline;
    _pa_superscript_t cppsuperscript;
    _pa_subscript_t cppsubscript;
    _pa_italic_t cppitalic;
    _pa_bold_t cppbold;
    _pa_strikeout_t cppstrikeout;
    _pa_standout_t cppstandout;
    _pa_fcolor_t cppfcolor;
    _pa_bcolor_t cppbcolor;
    _pa_auto_t cppauto;
    _pa_curvis_t cppcurvis;
    _pa_scroll_t cppscroll;
    _pa_curx_t cppcurx;
    _pa_cury_t cppcury;
    _pa_curbnd_t cppcurbnd;
    _pa_select_t cppselect;
    _pa_event_t cppevent;
    _pa_timer_t cpptimer;
    _pa_killtimer_t cppkilltimer;
    _pa_mouse_t cppmouse;
    _pa_mousebutton_t cppmousebutton;
    _pa_joystick_t cppjoystick;
    _pa_joybutton_t cppjoybutton;
    _pa_joyaxis_t cppjoyaxis;
    _pa_settab_t cppsettab;
    _pa_restab_t cpprestab;
    _pa_clrtab_t cppclrtab;
    _pa_funkey_t cppfunkey;
    _pa_frametimer_t cppframetimer;
    _pa_autohold_t cppautohold;
    _pa_wrtstr_t cppwrtstr;
    _pa_eventover_t cppeventover;
    _pa_eventsover_t cppeventsover;
    _pa_sendevent_t cppsendevent;
    _pa_title_t cpptitle;
    _pa_openwin_t cppopenwin;
    _pa_buffer_t cppbuffer;
    _pa_sizbuf_t cppsizbuf;
    _pa_getsiz_t cppgetsiz;
    _pa_setsiz_t cppsetsiz;
    _pa_setpos_t cppsetpos;
    _pa_scnsiz_t cppscnsiz;
    _pa_scncen_t cppscncen;
    _pa_winclient_t cppwinclient;
    _pa_front_t cppfront;
    _pa_back_t cppback;
    _pa_frame_t cppframe;
    _pa_sizable_t cppsizable;
    _pa_sysbar_t cppsysbar;
    _pa_menu_t cppmenu;
    _pa_menuena_t cppmenuena;
    _pa_menusel_t cppmenusel;
    _pa_stdmenu_t cppstdmenu;
    _pa_getwinid_t cppgetwinid;
    _pa_focus_t cppfocus;

    /* If autohold is active and and a local end was ordered, disable autohold
       in the root. Note the root also could have ordered an exit. */
    if (fautohold && fend) (*autohold_vect)(FALSE);

    /* swap old vectors for existing vectors API */
    _pa_cursor_ovr(cursor_vect, &cppcursor);
    _pa_maxx_ovr(maxx_vect, &cppmaxx);
    _pa_maxy_ovr(maxy_vect, &cppmaxy);
    _pa_home_ovr(home_vect, &cpphome);
    _pa_del_ovr(del_vect, &cppdel);
    _pa_up_ovr(up_vect, &cppup);
    _pa_down_ovr(down_vect, &cppdown);
    _pa_left_ovr(left_vect, &cppleft);
    _pa_right_ovr(right_vect, &cppright);
    _pa_blink_ovr(blink_vect, &cppblink);
    _pa_reverse_ovr(reverse_vect, &cppreverse);
    _pa_underline_ovr(underline_vect, &cppunderline);
    _pa_superscript_ovr(superscript_vect, &cppsuperscript);
    _pa_subscript_ovr(subscript_vect, &cppsubscript);
    _pa_italic_ovr(italic_vect, &cppitalic);
    _pa_bold_ovr(bold_vect, &cppbold);
    _pa_strikeout_ovr(strikeout_vect, &cppstrikeout);
    _pa_standout_ovr(standout_vect, &cppstandout);
    _pa_fcolor_ovr(fcolor_vect, &cppfcolor);
    _pa_bcolor_ovr(bcolor_vect, &cppbcolor);
    _pa_auto_ovr(auto_vect, &cppauto);
    _pa_curvis_ovr(curvis_vect, &cppcurvis);
    _pa_scroll_ovr(scroll_vect, &cppscroll);
    _pa_curx_ovr(curx_vect, &cppcurx);
    _pa_cury_ovr(cury_vect, &cppcury);
    _pa_curbnd_ovr(curbnd_vect, &cppcurbnd);
    _pa_select_ovr(select_vect, &cppselect);
    _pa_event_ovr(event_vect, &cppevent);
    _pa_timer_ovr(timer_vect, &cpptimer);
    _pa_killtimer_ovr(killtimer_vect, &cppkilltimer);
    _pa_mouse_ovr(mouse_vect, &cppmouse);
    _pa_mousebutton_ovr(mousebutton_vect, &cppmousebutton);
    _pa_joystick_ovr(joystick_vect, &cppjoystick);
    _pa_joybutton_ovr(joybutton_vect, &cppjoybutton);
    _pa_joyaxis_ovr(joyaxis_vect, &cppjoyaxis);
    _pa_settab_ovr(settab_vect, &cppsettab);
    _pa_restab_ovr(restab_vect, &cpprestab);
    _pa_clrtab_ovr(clrtab_vect, &cppclrtab);
    _pa_funkey_ovr(funkey_vect, &cppfunkey);
    _pa_frametimer_ovr(frametimer_vect, &cppframetimer);
    _pa_autohold_ovr(autohold_vect, &cppautohold);
    _pa_wrtstr_ovr(wrtstr_vect, &cppwrtstr);
    _pa_eventover_ovr(eventover_vect, &cppeventover);
    _pa_eventsover_ovr(eventsover_vect, &cppeventsover);
    _pa_sendevent_ovr(sendevent_vect, &cppsendevent);
    _pa_title_ovr(title_vect, &cpptitle);
    _pa_openwin_ovr(openwin_vect, &cppopenwin);
    _pa_buffer_ovr(buffer_vect, &cppbuffer);
    _pa_sizbuf_ovr(sizbuf_vect, &cppsizbuf);
    _pa_getsiz_ovr(getsiz_vect, &cppgetsiz);
    _pa_setsiz_ovr(setsiz_vect, &cppsetsiz);
    _pa_setpos_ovr(setpos_vect, &cppsetpos);
    _pa_scnsiz_ovr(scnsiz_vect, &cppscnsiz);
    _pa_scncen_ovr(scncen_vect, &cppscncen);
    _pa_winclient_ovr(winclient_vect, &cppwinclient);
    _pa_front_ovr(front_vect, &cppfront);
    _pa_back_ovr(back_vect, &cppback);
    _pa_frame_ovr(frame_vect, &cppframe);
    _pa_sizable_ovr(sizable_vect, &cppsizable);
    _pa_sysbar_ovr(sysbar_vect, &cppsysbar);
    _pa_menu_ovr(menu_vect, &cppmenu);
    _pa_menuena_ovr(menuena_vect, &cppmenuena);
    _pa_menusel_ovr(menusel_vect, &cppmenusel);
    _pa_stdmenu_ovr(stdmenu_vect, &cppstdmenu);
    _pa_getwinid_ovr(getwinid_vect, &cppgetwinid);
    _pa_focus_ovr(focus_vect, &cppfocus);

    /* swap old vectors for existing vectors I/O */
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
        error("System consistency check");

}
