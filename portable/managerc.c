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

The use case of managerc is to subdivide a surface like an xterm that
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

The reason for these rules is that managerc is "transparent" by default. A
non-manager aware program will run full screen, and behave as if it has the
the terminal window to itself. managerc will be entirely in the background.

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
#include <ctype.h>
#include <math.h>

/* linux definitions */
#include <limits.h>

/* local definitions */
#include <localdefs.h>
#include <config.h>
#include <terminal.h>

#include <diag.h>

/* external definitions */
#ifndef __MACH__ /* Mac OS X */
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
//#define PRTFMASK   /* print the forward masks calculated */

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

} scnatt;

/* rectangle */
typedef struct { int x1, y1, x2, y2; } rectangle;

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
typedef struct winrec {

    winptr   next;              /* next entry (for free list) */
    int      root;              /* window is the root */
    int      parlfn;            /* logical parent */
    winptr   parwin;            /* link to parent (or NULL for parentless) */
    int      wid;               /* this window logical id */
    winptr   childwin;          /* list of child windows */
    winptr   childlst;          /* list pointer if this is a child */
    winptr   winlst;            /* master list of all windows */
    winptr   rootlst;           /* master list of all roots */
    winptr   zmin2max;          /* Z order minimum to maximum list */
    winptr   zmax2min;          /* Z order maximum to minimum list */
    scnrec*  screens[MAXCON];   /* screen contexts array */
    int      curdsp;            /* index for current display screen */
    int      curupd;            /* index for current update screen */
    int      orgx;              /* window origin in root x */
    int      orgy;              /* window origin in root y */
    int      coffx;             /* client offset x */
    int      coffy;             /* client offset y */
    /* note: maxx/y tracks the buffer size in buffered mode, but tracks the
       client size with buffering off */
    int      maxx;              /* maximum x size */
    int      maxy;              /* maximum y size */
    int      bufx;              /* buffer size x characters */
    int      bufy;              /* buffer size y characters */
    int      cmaxx;             /* onscreen client size x */
    int      cmaxy;             /* onscreen client size x */
    int      pmaxx;             /* parent maximum x */
    int      pmaxy;             /* parent maximum y */
    int      mpx, mpy;          /* mouse current position */
    int      curx;              /* current cursor location x */
    int      cury;              /* current cursor location y */
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
    char     inpbuf[MAXLIN];    /* input line buffer */
    int      inpptr;            /* input line index */
    int      visible;           /* window is visible */
    char*    title;             /* window title */
    int      focus;             /* window has focus */
    int      hover;             /* window being hovered */
    int      zorder;            /* Z ordering of window, 0 = bottom, N = top */
    unsigned char* fmask;       /* forward mask in bits per character */
    int      fmasklen;          /* length of the bitmask */
    int      timers[PA_MAXTIM]; /* timer id array */
    int      frmtim;            /* frame timer */
    ami_color frmcolor;          /* frame color */

} winrec;

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
static int      curx;         /* cursor x */
static int      cury;         /* cursor y */
static int      curon;        /* current on/off visible state of cursor */
static winptr   winfre;       /* free windows structure list */
static winptr   winlst;       /* master list of all windows */
static winptr   rootlst;      /* master list of all roots */
static winptr   zmin2max;     /* Z order minimum to maximum list */
static winptr   zmax2min;     /* Z order maximum to minimum list */
static winptr   curfocus;     /* current focus window, or NULL */
static int      ztop;         /* current maximum/front Z order */
static int      mousex;       /* mouse tracking x */
static int      mousey;       /* mouse tracking y */
static winptr   timtbl[PA_MAXTIM]; /* timer translation table */
static int      timids[PA_MAXTIM]; /* timer logical ids */
static int      fautohold;    /* automatic hold on exit flag */
static int      fend;         /* end of program ordered flag */
static drgtyp   drag;         /* drag type in progress */
static winptr   drgwin;       /* drag window */
static int      drgx;         /* drag pin x */
static int      drgy;         /* drag pin y */
static ami_pevthan evthan[ami_etmenus+1]; /* array of event handler routines */
static ami_pevthan evtshan;        /* single master event handler routine */
static paevtque*  paqfre;         /* free PA event queue entries list */
static paevtque*  paqevt;         /* PA event input save queue */
static int        dimx, dimy;     /* terminal/root dimensions */

/* forwards */
static void plcchr(FILE* f, char c);

/** ****************************************************************************

Process error

*******************************************************************************/

static void error(
    /** Error string */ char* es
)


{

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

    fprintf(stderr, "PA Event: Window: %d ", er->winid);
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
        case ami_etresize: fprintf(stderr, ": x: %d y: %d", er->rszx, er->rszy);
                          break;
        case ami_etfun: fprintf(stderr, ": key: %d", er->fkey); break;
        case ami_etmenus: fprintf(stderr, ": id: %d", er->menuid); break;

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
    int y;
    int x;

    fprintf(stderr, "Buffer for wid: %d\n", win->wid);
    sc = win->screens[bufno-1]; /* index screen */
    for (y = 1; y <= win->maxy; y++) {

        fprintf(stderr, "%02d: \"", y);
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

static winptr lwn2win(int wid)

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
static void setrect(rectangle* r, int x1, int y1, int x2, int y2)

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

}

/*******************************************************************************

Set cursor cached

Sets the root cursor if it has changed.

*******************************************************************************/

static int setcursor(int x, int y)

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

/*******************************************************************************

Set foreground color cached

Sets the root foreground color if it has changed.

*******************************************************************************/

static int setfcolor(ami_color c)

{

    if (c != fcolor) {

        (*fcolor_vect)(stdout, c); /* set new color */
        fcolor = c; /* cache that */

    }

}

/*******************************************************************************

Set background color cached

Sets the root background color if it has changed.

*******************************************************************************/

static int setbcolor(ami_color c)

{

    if (c != bcolor) {

        (*bcolor_vect)(stdout, c); /* set new color */
        bcolor = c; /* cache that */

    }

}

/******************************************************************************

Translate rgb to primary color code

Translates an rgb color to a primary color code. It does this by finding the
nearest primary color to the given RGB color.

******************************************************************************/

static ami_color colrgbnum(int r, int g, int b)

{

    ami_color c;

    switch ((r > INT_MAX/2) << 2 | (g > INT_MAX/2) << 1 | (b > INT_MAX/2)) {

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
we assume auto is off and x can climb to infinity (INT_MAX).

*******************************************************************************/

static void wrtchr(char c)

{

    (*ofpwrite)(OUTFIL, &c, 1);
    curx++;

}

/*******************************************************************************

Output character string to root window

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (INT_MAX).

*******************************************************************************/

static void wrtstr(char* s)

{

    while (*s) { wrtchr(*s); s++; }

}

/*******************************************************************************

Output extended character string to root window

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (INT_MAX). This routine writes extended
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

static int inrect(int x, int y, rectangle* r)

{

    return (r->x1 <= x && x <= r->x2 && r->y1 <= y && y <= r->y2);

}

/*******************************************************************************

Output character to root window with clipping

Outputs a single character to the root window and advances the x cursor. Note
we assume auto is off and x can climb to infinity (INT_MAX).

Clips to the given rectangle. Note the cursor position is still advanced.

*******************************************************************************/

static void wrtchrclp(char c, rectangle* cr)

{

    if (inrect(curx, cury, cr)) { /* not clipped */

        (*ofpwrite)(OUTFIL, &c, 1);
        curx++;

    } else setcursor(curx+1, cury); /* just move the cursor */

}

/*******************************************************************************

Output character string to root window with clipping

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (INT_MAX).

Clips to the given rectangle. Note the cursor position is still advanced.

*******************************************************************************/

static void wrtstrclp(char* s, int l, rectangle* cr)

{

    while (*s && l) { wrtchrclp(*s, cr); s++; l--; }

}

/*******************************************************************************

Output extended character string to root window

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (INT_MAX). This routine writes extended
UTF-8 characters, meaning that it only advances the cursor once for the whole
string.

Clips to the given rectangle. Note the cursor position is still advanced.

*******************************************************************************/

static void wrtextclp(char* s, rectangle* cr)

{

    if (inrect(curx, cury, cr)) { /* not clipped */

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
    scnrec* scp;   /* pointer to screen location */

    /* clear buffer */
    for (y = 1; y <= win->maxy; y++)
        for (x = 1; x <= win->maxx; x++) {

        /* index screen character location */
        scp = &SCNBUF(sc, x, y);
        /* place character to buffer */
        scp->ch = ' ';
        scp->forec = ami_black;
        scp->backc = ami_white;
        scp->attr = 0;

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

    int i, t;

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

static void calcfmask(winptr win)

{

    winptr    wp;         /* window structure pointer */
    rectangle r1, r2, r3; /* window rectangles */
    int       x, y;
    int       cx, cy;
    int       l;

    memset(win->fmask, 0xff, win->fmasklen); /* set the bitmap */
    /* find the onscreen client rectangle in root terms */
    setrect(&r1, win->orgx+win->coffx, win->orgy+win->coffy,
                 win->orgx+win->coffx+win->cmaxx-1, 
                 win->orgy+win->coffy+win->cmaxy-1);
    wp = win->zmin2max; /* index windows in front by Z order */
    while (wp) { /* tour the (possibly) lapping windows */

        /* set window rectangle in root terms */
        setrect(&r2, wp->orgx, wp->orgy,
                    wp->orgx+wp->pmaxx-1, wp->orgy+wp->pmaxy-1);
        if (intersect(&r1, &r2)) { /* if this window overlaps our client area */

            intersection(&r3, &r1, &r2); /* find the intersected rectangle */
            /* draw into mask */
            for (y = r3.y1; y <= r3.y2; y++)
                for (x = r3.x1; x <= r3.x2; x++) {

                /* find net client offset */
                cx = x-(win->orgx+win->coffx);
                cy = y-(win->orgy+win->coffy);
                /* clear the cx/cy bit */
                l = cy*win->bufx+cx; /* find character location */
                win->fmask[l/8] &= ~(1<<(l%8)); /* mask off that bit */

            }

        }   
        wp = wp->zmin2max; /* next window entry */

    }

    /* diagnostic: print forward mask */
#ifdef PRTFMASK
    fprintf(stderr, "Forward mask: wid: %d size x: %d y: %d\n", 
                    win->wid, win->maxx, win->maxy); 
    fflush(stderr);
    fprintf(stderr, "     ");
    for (x = 1; x <= win->maxx; x++) fprintf(stderr, "%c", x%10+'0');
    fprintf(stderr, "\n");
    fflush(stderr);
    for (y = 1; y <= win->maxy; y++) {

        fprintf(stderr, "%03d: ", y);
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

    int x, y, l;

    if (win->frame) { /* draw window frame */

        (*fcolor_vect)(stdout, win->frmcolor);
        if (win->size) { /* draw size bars */

            /* draw top and bottom */
            setcursor(win->orgx, win->orgy);
            wrtextclp(frmchrs[toplftcnr], cr);
            for (x = 2; x <= win->pmaxx-1; x++) wrtextclp(frmchrs[horzlin], cr);
            wrtextclp(frmchrs[toprgtcnr], cr);

            setcursor(win->orgx, win->orgy+win->pmaxy-1);
            wrtextclp(frmchrs[btmlftcnr], cr);
            for (x = 2; x <= win->pmaxx-1; x++) wrtextclp(frmchrs[horzlin], cr);
            wrtextclp(frmchrs[btmrgtcnr], cr);

            /* draw sides */
            for (y = win->orgy+1; y < win->orgy+win->pmaxy-1; y++) {

                setcursor(win->orgx, y);
                wrtextclp(frmchrs[vertlin], cr);
                setcursor(win->orgx+win->pmaxx-1, y);
                wrtextclp(frmchrs[vertlin], cr);

            }

        }
        if (win->sysbar && win->pmaxy >= 3) { /* draw system bar */

            y = win->size; /* offset to system bar */
            /* draw blanks in title section */
            if (win->pmaxx-6 > 2) {

                setcursor(win->orgx+1, win->orgy+y);
                for (x = 1; x < win->pmaxx-6; x++) wrtchrclp(' ', cr);

            }
            x = win->pmaxx-6; /* start of system bar buttons */
            /* lay each button down if it's location is valid */
            if (x > 2) {

                /* set draw location */
                setcursor(win->orgx+x-1, win->orgy+y);
                wrtextclp(frmchrs[minbtn], cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(win->orgx+x-1, win->orgy+y);
                wrtchrclp(' ', cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(win->orgx+x-1, win->orgy+y);
                wrtextclp(frmchrs[maxbtn], cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(win->orgx+x-1, win->orgy+y);
                wrtchrclp(' ', cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(win->orgx+x-1, win->orgy+y);
                wrtextclp(frmchrs[canbtn], cr);

            }
            x++;
            if (x > 2) {

                /* set draw location */
                setcursor(win->orgx+x-1, win->orgy+y);
                wrtchrclp(' ', cr);

            }

            /* draw title, if exists */
            if (win->title) {

                l = strlen(win->title); /* get length */
                /* limit string length to available space */
                if (win->pmaxx-6-4 < l) l = win->pmaxx-6-4;
                if (l > 0) { /* there is room for some of the title */

                    setcursor(win->orgx+2+(win->pmaxx-6-4)/2-(l/2), win->orgy+y);
                    wrtstrclp(win->title, l, cr);

                }

            }

            /* draw underbar */
            y++;
            setcursor(win->orgx, win->orgy+y);
            if (win->pmaxy <= 3) wrtextclp(frmchrs[btmlftcnr], cr);
            else wrtextclp(frmchrs[intlft], cr);
            for (x = 2; x <= win->pmaxx-1; x++) wrtextclp(frmchrs[sysudl], cr);
            if (win->pmaxy <= 3) wrtextclp(frmchrs[btmrgtcnr], cr);
            else wrtextclp(frmchrs[intrgt], cr);

        }
        (*fcolor_vect)(stdout, fcolor);

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

    if (indisp(win)) { /* in display */

        /* check cursor in bounds */
        if (intcurbnd(win)) {

            setcurvis(win->curv); /* set cursor on or off */
            /* position actual cursor */
            setcursor(win->curx+win->orgx-1+win->coffx,
                      win->cury+win->orgy-1+win->coffy);

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
    int x, y;
    rectangle r1, r2;

    if (win->bufmod && win->visible)  { /* buffered mode is on, and visible */

        /* find intersection with client area */
        setcurvis(FALSE); /* turn off cursor for drawing */
        if (win->frame) drwfrm(win, cr); /* draw window frame */

        /* find intersection with client area */
        setrect(&r1, win->orgx+win->coffx, win->orgy+win->coffy,
                   win->orgx+win->coffx+win->cmaxx-1,
                   win->orgy+win->coffy+win->cmaxy-1);
        if (intersect(cr, &r1)) { /* there is an intersection with client area */

            intersection(&r2, cr, &r1); /* find client-clip intersection */
            sc = win->screens[win->curdsp-1]; /* index screen */
            /* restore window from buffer */
            for (y = r2.y1; y <= r2.y2; y++) {

                /* Reset cursor at the start of each line. Note frame offsets. */
                setcursor(r2.x1, y);
                /* draw each line */
                for (x = r2.x1; x <= r2.x2; x++) {

                    /* index screen character location */
                    scp = &SCNBUF(sc, x-(win->orgx+win->coffx)+1,
                                      y-(win->orgy+win->coffy)+1);
                    setfcolor(scp->forec); /* set colors */
                    setbcolor(scp->backc);
                    setattrs(scp->attr); /* set attributes */
                    wrtchr(scp->ch); /* output character */

                }

            }

        }
        setcur(win); /* reenable cursor */

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
    setrect(&cr, win->orgx, win->orgy,
               win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);
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

static void redraw(winptr win, int x1, int y1, int x2, int y2)

{

    rectangle r1, r2, r3, rt, rl, rr, rb;

    if (win) { /* if window exists */


        setrect(&r1, x1, y1, x2, y2); /* set update rectangle */
        cliproot(&r1); /* clip to terminal root window */
        /* set window rectangle */
        setrect(&r2, win->orgx, win->orgy,
                    win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);
        if (intersect(&r1, &r2)) { /* there is an intersection */

            intersection(&r3, &r1, &r2); /* find the intersected rectangle */
            /* restore that part of the window */
            restoreclp(win, &r3);
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
    int i;
    scnptr sc;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* first, find if next tab even exists */
    i = win->curx+1; /* get just after the current x position */
    if (i < 1) i = 1; /* don't bother to search to left of screen */
    /* find tab or end of screen */
    while (i < MAXTAB && !win->tab[i] && i < win->maxx) i++;
    if (win->tab[i]) /* not off right of tabs */
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

static void intsendevent(winptr win, ami_evtrec* er)

{

    ami_evtrec ec; /* copy of event record */

    memcpy(&ec, er, sizeof(ami_evtrec));
    ec.winid = 0; /* set anonymous window id */
    if (win) ec.winid = win->wid; /* overwrite window id */
    enquepaevt(&ec); /* send to queue */

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

Check x,y location is in the client area

Finds if the given x,y location lies in the client area. Note that this does not
include frame or system bar.

*******************************************************************************/

static int inclient(winptr win, int x, int y)

{

    int ox, oy;

    ox = (win->frame && win->size)*2;
    oy = (win->frame && win->size)*2+win->sysbar*2;
    /* check in client area */
    return (win->orgx+win->coffx <= x &&
            x <= win->orgx+win->coffx+win->pmaxx-ox-1 &&
            win->orgy+win->coffy <= y &&
            y <= win->orgy+win->coffy+win->pmaxy-oy-1);

}

/*******************************************************************************

Remove hover if left window

Expects the current x,y position of the mouse. If any window has hover active,
but does not contain the mouse in it's client area, the hover mode is cancelled.

*******************************************************************************/

static void remhover(int x, int y)

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

static winptr fndtop(int x, int y)

{

    winptr win; /* pointer to windows list */
    winptr fp;  /* found window pointer */
    int    z;   /* z order of last found */

    win = winlst; /* get the master list */
    fp = NULL; /* set no window found */
    z = -1; /* set invalid z order */
    while (win) { /* traverse the windows list */

        if (win->orgx <= x && x <= win->orgx+win->pmaxx-1 &&
            win->orgy <= y && y <= win->orgy+win->pmaxy-1 &&
            win->zorder > z) {

            /* found inclusion, Z order above previous */
            fp = win; /* set candidate */
            z = win->zorder;

        }
        win = win->winlst; /* next window */

    }

    return (fp); /* exit wth container */

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
            win->zmax2min = zmax2min; /* push to list */
            zmax2min = win;

        }

    }

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

        fprintf(stderr, "Window; %d zorder: %d\n", wp->wid, wp->zorder);
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

        fprintf(stderr, "Window; %d zorder: %d\n", wp->wid, wp->zorder);
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
    /* reorder list */
    wp = zmin2max; /* index top of list */
    z = 2; /* set z order count */
    while (wp) { /* traverse the list */

        wp->zorder = z++; /* set new order */
        wp = wp->zmin2max; /* next entry */

    }
    win->zmin2max = zmin2max->zmin2max; /* push ahead of root */
    zmin2max->zmin2max = win;
    win->zorder = ztop; /* set our entry as bottom Z order */
    makzmax2min(); /* remake the max 2 min list */

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

    }

}

/** ****************************************************************************

Open and present window

Given a windows file id and an (optional) parent window file id opens and
presents the window associated with it. All of the screen buffer data is
cleared, and a single buffer assigned to the window.

*******************************************************************************/

static void opnwin(int fn, int pfn, int wid, int subclient, int root)

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
    remfocus(); /* remove any existing focus */
    win->focus = TRUE; /* last window in gets focus */
    curfocus = win;
    win->hover = FALSE; /* set no hover */
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
    win->maxx -= (win->frame && win->size)*2;
    win->maxy -= win->frame*2+win->size*2;
    win->cmaxx = win->maxx; /* set client size to track buffer */
    win->cmaxy = win->maxy;
    win->mpx = 0; /* set mouse relative position invalid */
    win->mpy = 0;
    win->attr = attr; /* no attribute */
    win->autof = TRUE; /* auto on */
    win->fcolor = ami_black; /*foreground black */
    win->bcolor = ami_white; /* background white */
    win->frmcolor = ami_blue; /* frame color blue */
    win->curv = TRUE; /* cursor visible */
    win->orgx = 1;  /* set origin to root */
    win->orgy = 1;
    /* set client offset considering framing characteristics */
    win->coffx = 0+(win->frame && win->size);
    win->coffy = 0+(win->frame && win->size)+win->sysbar*2;
    win->curx = 1; /* set cursor at home */
    win->cury = 1;
    /* clear tabs and set to 8ths */
    for (t = 1; t <= MAXTAB; t++) win->tab[t-1] = ((t-1)%8 == 0) && (t != 1);
    /* clear timer array */
    for (ti = 0; ti < PA_MAXTIM; ti++) win->timers[ti] = 0;
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
    if (root) {

#ifndef __MACH__ /* Mac OS X */
    win->title = malloc(strlen(program_invocation_short_name)+1);
    if (!win->title) error("Out of memory");
    /* set title to invoking program */
    strcpy(win->title, program_invocation_short_name);
#endif

    }

    iniscn(win, win->screens[0]); /* initalize screen buffer */
    restore(win); /* update to screen */
    recalcfmask(); /* recalculate the forward masks */

}

/** ****************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   int wid, int subclient, int root)

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

static void intopenwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

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
        *infile = fopen("/dev/null", "r"); /* open null as read only */
        if (!*infile) error("Can't open file"); /* can't open */
        setvbuf(*infile, NULL, _IONBF, 0); /* turn off buffering */
        ifn = fileno(*infile); /* get logical file no */

    }
    /* open output file */
    *outfile = fopen("/dev/null", "w");
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
    ami_evtrec er;   /* PA event record */

    wid = filwin[ofn]; /* get window id */
    ifn = opnfil[ofn]->inl; /* get the input file link */
    win = lfn2win(ofn); /* get a pointer to the window */
    clsfil(ofn); /* flush and close output file */
    /* if no remaining links exist, flush and close input file */
    if (!inplnk(ifn)) clsfil(ifn);
    filwin[ofn] = -1; /* clear file to window translation */
    xltwin[wid+MAXFIL] = -1; /* clear window to file translation */

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
    fprintf(stderr, "Window: %d buffer\n", win->wid); fflush(stderr);
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

static void intscroll(winptr win, int x, int y)

{

    int      xi, yi;       /* screen counters */
    scnptr   scnsav;       /* full screen buffer save */
    int      lx;           /* last unmatching character index */
    int      m;            /* match flag */
    scnptr   sc;           /* pointer to current screen */
    scnptr   sp;           /* pointer to screen record */
    int      curxs, curys; /* cursor position save */

    /* when the scroll is arbitrary, we do it by completely refreshing the
       contents of the screen from the buffer */
    if (x <= -win->maxx || x >= win->maxx || y <= -win->maxy || y >= win->maxy) {

        wrtchr('\f'); /* scroll would result in complete clear, do it */
        iniscn(win, win->screens[win->curupd-1]);   /* clear the screen buffer */
        /* restore cursor position */
        setcursor(win->orgx+win->coffx, win->orgy+win->coffy);

    } else { /* scroll */

        /* true scroll is done in two steps. first, the contents of the buffer
           are adjusted to read as after the scroll. then, the contents of the
           buffer are output to the terminal. before the buffer is changed,
           we perform a full save of it, which then represents the "current"
           state of the real terminal. then, the new buffer contents are
           compared to that while being output. this saves work when most of
           the screen is spaces anyways */
        sc = win->screens[win->curupd-1]; /* index current screen */
        if (indisp(win)) { /* in display */

            /* save the entire buffer */
            scnsav = malloc(sizeof(scnrec)*win->maxy*win->maxx);
            memcpy(scnsav, sc, sizeof(scnrec)*win->maxy*win->maxx);

        }
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
            for (yi = 1; yi <= abs(y); yi++) /* clear blank lines at start */
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
                for (xi = 1; xi <= abs(x); xi++) {

                    sp = &SCNBUF(sc, xi, yi);
                    sp->ch = ' ';   /* clear to blanks at colors and attributes */
                    sp->forec = sc->forec;
                    sp->backc = sc->backc;
                    sp->attr = sc->attr;

                }

            }

        }
        if (indisp(win)) { /* in display */

            curxs = curx; /* save cursor position */
            curys = cury;
            /* the buffer is adjusted. now just copy the complete buffer to the
               screen */
            for (yi = 1; yi <= win->maxy; yi++) { /* lines */

                win->curx = 1; /* set cursor */
                win->cury = yi;
                setcur(win);
                /* find the last unmatching character between real and new buffers.
                   Then, we only need output the leftmost non-matching characters
                   on the line. note that it does not really help us that characters
                   WITHIN the line match, because a character output is as or more
                   efficient as a cursor movement. if, however, you want to get
                   SERIOUSLY complex, we could check runs of matching characters,
                   then check if performing a direct cursor position is less output
                   characters than just outputting data :) */
                lx = win->maxx; /* set to end */
                do { /* check matches */

                    m = 1; /* set match */
                    /* check all elements match */
                    if (SCNBUF(sc, lx, yi).ch != SCNBUF(scnsav, lx, yi).ch)
                        m = 0;
                    if (SCNBUF(sc, lx, yi).forec !=
                        SCNBUF(scnsav, lx, yi).forec) m = 0;
                    if (SCNBUF(sc, lx, yi).backc !=
                        SCNBUF(scnsav, lx, yi).backc) m = 0;
                    if (SCNBUF(sc, lx, yi).attr != SCNBUF(scnsav, lx, yi).attr)
                        m = 0;
                    if (m) lx--; /* next character */

                } while (m && lx); /* until match or no more */
                for (xi = 1; xi <= lx; xi++) { /* characters */

                    /* for each new character, we compare the attributes and colors
                       with what is set. if a new color or attribute is called for,
                       we set that, and update the saves. this technique cuts down on
                       the amount of output characters */
                    sp = &SCNBUF(sc, xi, yi);
                    setfcolor(sp->forec); /* set the foreground color */
                    setbcolor(sp->backc); /* set the background color */
                    setattrs(sp->attr); /* set the attribute */
                    wrtchr(sp->ch);

                }

            }
            (*cursor_vect)(stdout, curxs, curys); /* restore cursor position */

        }

    }

}

/** ****************************************************************************

Set window size character internal

Sets the onscreen window size, in character terms.

*******************************************************************************/

static void intsetsiz(winptr win, int x, int y)

{

    int ox, oy; /* previous size of window */
    rectangle r1, r2, r3, rt, rl, rr, rb;

    if (win->frame && win->size) {

        /* if size bars are on */
        if (x < 2) x = 2; /* set minimum size to preseve size bars */
        if (y < 2) y = 2;

    }
    ox = win->pmaxx; /* save previous size of window */
    oy = win->pmaxy;
    win->pmaxx = x; /* set size */
    win->pmaxy = y;
    win->cmaxx = win->pmaxx; /* copy to client dimensions */
    win->cmaxy = win->pmaxy;
    /* subtract frame from client if enabled */
    win->cmaxx -= (win->frame && win->size)*2;
    win->cmaxy -= win->frame*2+win->size*2;
    if (!win->bufmod) { /* in follow mode */

        win->maxx = win->pmaxx; /* copy to client area dimensions */
        win->maxy = win->pmaxy;
        /* subtract frame from client area if enabled */
        win->maxx -= (win->frame && win->size)*2;
        win->maxy -= win->frame*2+win->size*2;

    }
    if (win->visible) { /* window is onscreen */

        /* check old and new overlap */
        setrect(&r1, win->orgx, win->orgy, win->orgx+ox-1, win->orgy+oy-1);
        setrect(&r2, win->orgx, win->orgy,
                             win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);
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
            redraw(zmax2min, win->orgx, win->orgy,
                             win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);

        } else {

            remmax2min(win); /* take out of max 2 min list */
            /* draw the current window position out */
            redraw(zmax2min, win->orgx, win->orgy, win->orgx+ox-1, win->orgy+oy-1);
            /* note we kept the Z order of the repositioned window */
            makzmax2min(); /* remake the max 2 min list */
            /* draw the new window size in */
            redraw(zmax2min, win->orgx, win->orgy,
                             win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);

        }

    }
    recalcfmask(); /* recalculate the forward masks */

}

/** ****************************************************************************

Set window position character internal

Sets the onscreen window position, in character terms. Keeps the existing Z
order.

*******************************************************************************/

static void intsetpos(winptr win, int x, int y)

{

    int ox, oy; /* previous position of window */
    rectangle r1, r2, r3, rt, rl, rr, rb;

    ox = win->orgx; /* save previous position of window */
    oy = win->orgy;
    win->orgx = x; /* set position in parent */
    win->orgy = y;
    if (win->visible) { /* window is onscreen */

        /* check old and new overlap */
        setrect(&r1, ox, oy, ox+win->pmaxx-1, oy+win->pmaxy-1);
        setrect(&r2, win->orgx, win->orgy,
                    win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);
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
            redraw(zmax2min, win->orgx, win->orgy,
                             win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);

        } else {

            remmax2min(win); /* take out of max 2 min list */
            /* draw the current window position out */
            redraw(zmax2min, ox, oy, ox+win->pmaxx-1, oy+win->pmaxy-1);
            /* note we kept the Z order of the repositioned window */
            makzmax2min(); /* remake the max 2 min list */
            /* draw the new window position in */
            redraw(zmax2min, win->orgx, win->orgy,
                             win->orgx+win->pmaxx-1, win->orgy+win->pmaxy-1);

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

static void icursor(FILE* f, int x, int y)

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

static int imaxx(FILE* f)

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

static int imaxy(FILE* f)

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
negative space as long as it stays within the bounds -INT_MAX to INT_MAX.

*******************************************************************************/

static void iup(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* check not top of screen */
    if (win->cury > 1) win->cury--; /* update position */
    else if (win->autof) intscroll(win, 0, -1); /* scroll up */
    /* check won't overflow */
    else if (win->cury > -INT_MAX) win->cury--; /* set new position */
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor down

Moves the cursor position down one line. If the cursor is at screen bottom, and
auto is on, the screen is scrolled down, meaning that the screen contents are
moved up a line of text. If auto is off, the cursor can simply continue into
undrawn space as long as it stays within the bounds of -INT_MAX to INT_MAX.

*******************************************************************************/

static void idown(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    /* check not bottom of screen */
    if (win->cury < win->maxy) win->cury++; /* update position */
    else if (win->autof) intscroll(win, 0, +1); /* scroll down */
    /* check won't overflow */
    else if (win->cury < INT_MAX) win->cury++; /* set new position */
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
            if (win->curx > -INT_MAX) win->curx--; /* update position */

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
            if (win->curx < INT_MAX) win->curx++; /* update position */

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

static void iblink(FILE* f, int e)

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

static void ireverse(FILE* f, int e)

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

static void iunderline(FILE* f, int e)

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

static void isuperscript(FILE* f, int e)

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

static void isubscript(FILE* f, int e)

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

static void iitalic(FILE* f, int e)

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

static void ibold(FILE* f, int e)

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

static void istrikeout(FILE* f, int e)

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

static void istandout(FILE* f, int e)

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

static void ifcolorc(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->fcolor = colrgbnum(r, g, b); /* set color */

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

static void ibcolorc(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->bcolor = colrgbnum(r, g, b); /* set color */

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

static void iauto(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->autof = e; /* set auto state */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

static void icurvis(FILE* f, int e)

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

static void iscroll(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    intscroll(win, x, y); /* process scroll */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

static int icurx(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return win->curx; /* return cursor x */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

static int icury(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return win->cury; /* return cursor y */

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

static int icurbnd(FILE* f)

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

static void iselect(FILE* f, int u, int d)

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
    int       x, y;

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
        case ami_etmouba:  /* mouse button assertion */
            win = fndtop(mousex, mousey); /* find the enclosing window */
            /* first click with no focus gives focus, next click gives message */
            if (win) {

                if (win->focus) { /* window has focus */

                    if (inclient(win, mousex, mousey)) {

                        /* in client area */
                        er.etype = ami_etmouba; /* set mouse button asserts */
                        er.amoun = ev.amoun; /* set mouse number */
                        er.amoubn = ev.amoubn; /* set button number */
                        er.winid = win->wid; /* set window logical id */
                        intsendevent(win, &er); /* issue event */

                    } else if (ev.mmoun == 1) {

                        if (win->sysbar && mousey-win->orgy == win->size &&
                            mousex-win->orgx >= 1 &&
                            mousex-win->orgx < win->pmaxx-1) {

                            /* check for system bar events */
                            if (mousex-win->orgx == win->pmaxx-3) {

                                /* terminate */
                                er.etype = ami_etterm; /* set type */
                                intsendevent(win, &er); /* issue event */
                                fend = TRUE; /* set end program requested */

                            } if (mousex-win->orgx == win->pmaxx-5) {

                                /* max */
                                er.etype = ami_etmax; /* set type */
                                intsendevent(win, &er); /* issue event */

                            } if (mousex-win->orgx == win->pmaxx-7) {

                                /* min */
                                er.etype = ami_etmin; /* set type */
                                intsendevent(win, &er); /* issue event */

                            } else if (mousex-win->orgx >= 1 &&
                                       mousex-win->orgx < win->pmaxx-1) {

                                /* system bar click */
                                drag = dt_sysbar; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            }

                        } else if (win->frame && win->size) {

                            /* frame and sizebars are enabled */
                            if (mousey-win->orgy == 0 &&
                                  mousex-win->orgx == 0) {

                                /* top left click */
                                drag = dt_ulcnr; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            } else if (mousey-win->orgy == 0 &&
                                  mousex-win->orgx == win->pmaxx-1) {

                                /* top right click */
                                drag = dt_urcnr; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            } else if (mousey-win->orgy == win->pmaxy-1 &&
                                  mousex-win->orgx == 0) {

                                /* bottom left click */
                                drag = dt_blcnr; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            } else if (mousey-win->orgy == win->pmaxy-1 &&
                                  mousex-win->orgx == win->pmaxx-1) {

                                /* bottom right click */
                                drag = dt_brcnr; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            } else if (mousey-win->orgy == 0) {

                                /* top bar click */
                                drag = dt_top; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            } else if (mousey-win->orgy == win->pmaxy-1) {

                                /* bottom bar click */
                                drag = dt_bottom; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            } else if (mousex-win->orgx == 0) {

                                /* left bar click */
                                drag = dt_left; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            } else if (mousex-win->orgx == win->pmaxx-1) {

                                /* right bar click */
                                drag = dt_right; /* set drag type */
                                drgwin = win; /* set up drag pin */
                                drgx = mousex;
                                drgy = mousey;

                            }

                        }

                    }

                } else if (ev.mmoun == 1) { /* button 1 click */

                    remfocus(); /* remove previous focus */
                    /* send focus message */
                    er.etype = ami_etfocus; /* set focus event */
                    intsendevent(win, &er); /* send to queue */
                    win->focus = TRUE; /* set current focus */
                    curfocus = win;
                    if (inclient(win, mousex, mousey)) {

                        /* window went into focus, can have hover now */
                        if (!win->hover) { /* enter hover mode */

                            er.etype = ami_ethover; /* set enter hover */
                            intsendevent(win, &er); /* issue event */
                            win->hover = TRUE; /* set hover active */

                        }

                    }
                    if (win->zorder != ztop && !win->root) { /* if not already top window */

                        intfront(win); /* bring to front */
                        /* redraw for order */
                        redraw(win, win->orgx, win->orgy,
                                    win->orgx+win->pmaxx-1,
                                    win->orgy+win->pmaxy-1);

                    }

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
                    er.moupx = mousex-(win->orgx+win->coffx)+1;
                    er.moupy = mousey-(win->orgy+win->coffy)+1;
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
    int       xoff; /* x starting line offset */
    int       l;    /* buffer length */
    int       ofn;  /* logical output file */
    int       lcmp; /* line complete */
    int       i;
    FILE*     f;

    lcmp = FALSE; /* set line not complete */
    do { /* get line characters */

        ievent(f, &er); /* get next event */
        ofn = xltwin[er.winid+MAXFIL]; /* get logical output file */
        f = opnfil[ofn]->sfp; /* get window file */
        if (ofn >= 0 && opnfil[ofn]->inl == fd) {

            /* output file indexes our input file */
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

static void itimer(FILE* f, int i, long t, int r)

{

    winptr win; /* windows record pointer */
    int    ti;

    if (i < 1 || i > PA_MAXTIM) error("Invalid timer handle");
    win = txt2win(f); /* get window from file */
    if (!win->timers[i-1]) { /* no current timer assigned */

        ti = 0;
        while (ti < PA_MAXTIM && timtbl[ti]) ti++;
        if (ti >= PA_MAXTIM) error("Root timers are full");
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

static void ikilltimer(FILE* f, int i)

{

    winptr win; /* windows record pointer */

    if (i < 1 || i > PA_MAXTIM) error("Invalid timer handle");
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

static int imouse(FILE* f)

{

    return (*mouse_vect)(stdout); /* find number of mice */

}

/** ****************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the given mouse. This is a pure passthrough
function.

*******************************************************************************/

static int imousebutton(FILE* f, int m)

{

    return (*mousebutton_vect)(stdout, m); /* find number of buttons */

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached. This is a pure passthrough function.

*******************************************************************************/

static int ijoystick(FILE* f)

{

    return (*joystick_vect)(stdout); /* find number of joysticks */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick. This is a pure passthrough
function.

*******************************************************************************/

static int ijoybutton(FILE* f, int j)

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

static int ijoyaxis(FILE* f, int j)

{

    return (*joyaxis_vect)(stdout, j); /* find number of axies */

}

/** ****************************************************************************

Set tab

Sets a tab at the indicated column number.

*******************************************************************************/

static void isettab(FILE* f, int t)

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

static void irestab(FILE* f, int t)

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

static int ifunkey(FILE* f)

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

static void iframetimer(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    int    ti;

    win = txt2win(f); /* get window from file */
    if (e) { /* enable framing timer */

        if (!win->frmtim) { /* no current timer assigned */

            ti = 0;
            while (ti < PA_MAXTIM && timtbl[ti]) ti++;
            if (ti >= PA_MAXTIM) error("Root timers are full");
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

static void iautohold(int e)

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

static void iwrtstrn(FILE* f, char* s, int n)

{

    winptr win; /* window record pointer */
    scnptr scp; /* screen buffer */
    char*  ss;
    int    ns;

    win = txt2win(f); /* get window from file */
    if (win->autof) error("Cannot direct write string with auto on");
    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (win->bufmod) { /* buffer is active */

        ss = s; /* save string */
        ns = n;
        while (ns && icurbnd(f)) { /* print string */

            /* index screen character location */
            scp = &SCNBUF(win->screens[win->curdsp-1],
                            win->curx, win->cury);
            /* place character to buffer */
            scp->ch = *ss++;
            scp->forec = win->fcolor;
            scp->backc = win->bcolor;
            scp->attr = win->attr;
            win->curx++; /* next location */
            ns--; /* count */

        }

    }
    if (indisp(win)) { /* do it again for the current screen */

        /* set root cursor to correct position */
        setcursor(win->curx+win->orgx-1+win->coffx,
                  win->cury+win->orgy-1+win->coffy);
        setattrs(win->attr); /* set attributes */
        setfcolor(win->fcolor); /* set colors */
        setbcolor(win->bcolor);
        while (*s && icurbnd(f)) { /* print string */

            /* draw character to active screen */
            wrtchr(*s++); /* output */
            win->curx++; /* next location */
            n--; /* count */

        }

    }

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

static void ititlen(FILE* f, char* ts, int n)

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
        setrect(&r, win->orgx+2, win->orgy+win->size, 
                   win->pmaxx-6-4, win->orgy+win->size); 
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

static void iopenwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

    /* open as child of client window */
    intopenwin(infile, outfile, parent, wid);

}

/** ****************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

static void ibuffer(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (e != win->bufmod) { /* buffered status has changed */

        win->bufmod = e; /* set new buffer status */
        if (e) { /* enable buffered mode */

            win->maxx = win->bufx; /* restore previous buffer size */
            win->maxy = win->bufy;
            /* allocate prime buffer */
            win->screens[0] = malloc(sizeof(scnrec)*win->maxy*win->maxx);
            /* clear */
            iniscn(win, win->screens[0]);

        } else {

            /* find dimensions from parent for follow modes */
            win->maxx = win->pmaxx; /* copy to client dimensions */
            win->maxy = win->pmaxy;
            /* subtract frame from client if enabled */
            win->maxx -= (win->frame && win->size)*2;
            win->maxy -= win->frame*2+win->size*2;
            clrbufs(win); /* clear out all allocatedbuffers */

        }

    }

}

/** ****************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

static void isizbuf(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (!win->bufmod) error("Buffer mode is not enabled");
    if (win->bufx != x || win->bufy != y) {

        /* buffer size has changed */
        clrbufs(win); /* all the screen buffers are wrong, so tear them out */
        /* allocate prime buffer */
        win->screens[0] = malloc(sizeof(scnrec)*win->maxy*win->maxx);
        /* clear */
        iniscn(win, win->screens[0]);
        free(win->fmask); /* release previous mask */
        alcfmask(win); /* allocate and clear forward mask */

    }
    recalcfmask(); /* recalculate the forward masks */

}

/** ****************************************************************************

Get window size character

Gets the onscreen parent window size, in character terms.

*******************************************************************************/

static void igetsiz(FILE* f, int* x, int* y)

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

static void isetsiz(FILE* f, int x, int y)

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

static void isetpos(FILE* f, int x, int y)

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

static void iscnsiz(FILE* f, int* x, int* y)

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

static void iscncen(FILE* f, int* x, int* y)

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

static void iwinclient(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)

{

    if ((BIT(ami_wmframe) & ms) && (BIT(ami_wmsize) & ms)) {

        cx++; /* add size bars to client */
        cy++;

    }
    if (BIT(ami_wmsysbar) & ms) cy += 2; /* add sysbar to client */

}

/** ****************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

static void ifront(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    intfront(win); /* make this the front window */

}

/** ****************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

static void iback(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    intback(win); /* make this the back window */

}

/** ****************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

static void iframe(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (win->frame != e) {

        win->frame = e; /* set frame state */
        restore(win); /* redraw */

    }

}

/** ****************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

On GNOME/Ubuntu 20.04 with GDM3 window manager, we are not capable of turning
off the size bars alone, so this is a no-op. It may work on other window
managers.

*******************************************************************************/

static void isizable(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (win->size != e) {

        win->size = e; /* set frame state */
        restore(win); /* redraw */

    }

}

/** ****************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

I don't think XWindow can do this separately. Instead, the frame() function is
used to create component windows.

*******************************************************************************/

static void isysbar(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    e = !!e; /* clean boolean */
    if (win->sysbar != e) {

        win->sysbar = e; /* set frame state */
        restore(win); /* redraw */

    }

}

/** ****************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/

static void imenu(FILE* f, ami_menuptr m)

{

}

/** ****************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

static void imenuena(FILE* f, int id, int onoff)

{

}

/** ****************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

static void imenusel(FILE* f, int id, int select)

{

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

static void istdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)

{

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

static int igetwinid(void)

{

    int wid; /* window id */

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
    int     l;

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
    else if (c == '\f') clrscn(f); /* clear screen */
    else if (c == '\t') itab(f); /* process tab */
    /* only output visible characters */
    else if (c >= ' ' && c != 0x7f) {

        /* find character location */
        l = (win->cury-1)*win->bufx+(win->curx-1); 
        if (icurbnd(f) && win->fmask[l/8] & 1<<(l%8)) { /* cursor is in bounds */

            if (win->bufmod) { /* buffer is active */

                /* index screen character location */
                scp = &SCNBUF(win->screens[win->curdsp-1],
                              win->curx, win->cury);
                /* place character to buffer */
                scp->ch = c;
                scp->forec = win->fcolor;
                scp->backc = win->bcolor;
                scp->attr = win->attr;

            }
            if (indisp(win)) { /* do it again for the current screen */

                setattrs(win->attr); /* set attributes */
                setfcolor(win->fcolor); /* set colors */
                setbcolor(win->bcolor);
                /* draw character to active screen */
                setcursor(win->curx+win->orgx-1+win->coffx,
                          win->cury+win->orgy-1+win->coffy);
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
        /* send data to terminal */
        while (cnt--) plcchr(opnfil[fd]->sfp, *p++);
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

Managerc startup

*******************************************************************************/

static void init_managerc(void) __attribute__((constructor (104)));
static void init_managerc()

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
    for (ti = 0; ti < PA_MAXTIM; ti++) timtbl[ti] = NULL;

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

static void deinit_managerc(void) __attribute__((destructor (104)));
static void deinit_managerc()

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
