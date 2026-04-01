/*******************************************************************************
*                                                                              *
*                           GRAPHICAL MODE LIBRARY                             *
*                                                                              *
*                       Copyright (C) 2006 Scott A. FRANCO                     *
*                                                                              *
*                              4/96 S. A. Moore                                *
*                                                                              *
* Implements the graphical mode functions on Windows. graph is upward          *
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
* fand, band                                                                   *
*                                                                              *
* Used with move to implement arbitrary clips usng move, above.                *
*                                                                              *
* History:                                                                     *
*                                                                              *
* graph started in 1996 as a graphical window demonstrator as a twin to        *
* ansilib, the ANSI control character based terminal mode library.             *
* In 2003, graph was upgraded to the graphical terminal standard.              *
* In 2005, graph was upgraded to include the window mangement calls, and the   *
* widget calls.                                                                *
*                                                                              *
* graph uses three different tasks. The main task is passed on to the          *
* program, and two subthreads are created. The first one is to run the         *
* display, and the second runs widgets. The Display task both isolates the     *
* user interface from any hangs or slowdowns in the main thread, and also      *
* allows the display task to be a completely regular windows message loop      *
* with class handler, that just happens to communicate all of its results      *
* back to the main thread. This solves several small problems with adapting    *
* the X Windows/Mac OS style we use to Windows style. The main and the         *
* display thread are "joined" such that they can both access the same          *
* windows. The widget task is required because of this joining, and serves to  *
* isolate the running of widgets from the main or display threads.             *
*                                                                              *
*                            BSD LICENSE INFORMATION                           *
*                                                                              *
* Copyright (C) 2020 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*     notice, this list of conditions and the following disclaimer.            *
* 2. Redistributions in binary form must reproduce the above copyright         *
*     notice, this list of conditions and the following disclaimer in the      *
*     documentation and/or other materials provided with the distribution.     *
* 3. Neither the name of the project nor the names of its contributors         *
*     may be used to endorse or promote products derived from this software    *
*     without specific prior written permission.                               *
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

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00
#define _WIN32_IE 0xA00

#include <sys/types.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include <commctrl.h>

#include <config.h>
#include <graphics.h>

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

/* enter debugger on fail */
#define ENTDBG 1
//#define WAITCANCEL /* wait for user cancel on uncommanded exit */

/*
 * Configurable parameters
 *
 * These parameters can be configured here at compile time, or are overriden
 * at runtime by values of the same name in the config files.
 */
#define MAXXD     80 /* standard terminal, 80x25 */
#define MAXYD     25
#define DIALOGERR 1     /* send runtime errors to dialog */
#define MOUSEENB  TRUE  /* enable mouse */
#define JOYENB    TRUE  /* enable joysticks */
#define DMPMSG    FALSE /* enable dump Windows API messages */
#define DMPEVT    FALSE /* enable dump Petit-Ami messages */

/*
 * Enable/disable general lock
 *
 * This is done from paranoia that Windows performs multithread locking.
 * The Windows documents on this subject are ambiguous, some parts say it does,
 * others warn it does not.
 */
#define MAINLOCKING 1

#define BIT(b) (1<<b) /* set bit from bit number */
#define BITMSK(b) (~BIT(b)) /* mask out bit number */


/* the "standard character" sizes are used to form a pseudo-size for desktop
   character measurements in a graphical system. */
#define STDCHRX   8
#define STDCHRY   12
#define MAXLIN    250   /* maximum length of input bufferred line */
#define MAXCON    10    /* number of screen contexts */
#define MAXTAB    50    /* total number of tabs possible per screen */
#define MAXPIC    50    /* total number of loadable pictures */
#define FHEIGHT   15    /* default font height, matches Windows "system" default */
#define FQUALITY  NONANTIALIASED_QUALITY /* font writing quality */
#define FRMTIM    0     /* handle number of framing timer */
#define PI        3.1415926535897932 /* PI to 17 digits */
#define MAXMSG    1000  /* size of input message queue */
 /* Messages defined in this module. The system message block runs from
   0x000-0x3ff, so the user mesage area starts at 0x400. */
#define UM_MAKWIN  0x404 /* create standard window */
#define UM_WINSTR  0x405 /* window was created */
#define UM_CLSWIN  0x406 /* close window */
#define UM_WINCLS  0x407 /* window was closed */
#define UM_IM      0x408 /* intratask message */
#define UM_EDITCR  0x409 /* edit widget sends cr */
#define UM_NUMCR   0x410 /* number select widget sends cr */
/* standard file handles */
#define INPFIL    0     /* input */
#define OUTFIL    1     /* output */
#define ERRFIL    3     /* error */

/* foreground pen style */
/* FPENSTL  ps_geometric | ps_endcap_flat | ps_solid */
#define FPENSTL  (PS_GEOMETRIC | PS_ENDCAP_FLAT | PS_SOLID | PS_JOIN_MITER)
/* foreground single pixel pen style */
#define FSPENSTL  PS_SOLID
#define PACKMSG  TRUE   /* pack paint messages in queue */
#define MAXFIL   100    /* maximum open files */
#define MAXFNM   250    /* number of filename characters in buffer */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int     (*popen_t)(const char*, int, int);
typedef int     (*pclose_t)(int);
typedef off_t   (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

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

/* font description entry */
typedef struct fontrec {

    char*           fn;   /* name of font */
    int             fix;  /* fixed pitch font flag */
    int             sys;  /* font is system fixed (default) */
    struct fontrec* next; /* next font in list */

} fontrec, *fontptr;

typedef enum { mdnorm, mdinvis, mdxor } mode; /* color mix modes */

/* Menu tracking. This is a mirror image of the menu we were given by the
   user. However, we can do with less information than is in the original
   tree as passed. The menu items are a linear list, since they contain
   both the menu handle and the relative number 0-n of the item, neither
   the lack of tree structure nor the order of the list matters. */
typedef struct metrec {

    struct metrec* next;   /* next entry */
    HMENU          han;    /* handle of menu entry is attached to */
    int            inx;    /* index position, 0-n, of item */
    int            onoff;  /* the item is on-off highlighted */
    int            select; /* the current on/off state of the highlight */
    struct metrec* oneof;  /* "one of" chain pointer */
    int            id;     /* user id of item */

} metrec, *metptr;

/* widget type */
typedef enum  {
    wtbutton, wtcheckbox, wtradiobutton, wtgroup, wtbackground,
    wtscrollvert, wtscrollhoriz, wtnumselbox, wteditbox,
    wtprogressbar, wtlistbox, wtdropbox, wtdropeditbox,
    wtslidehoriz, wtslidevert, wttabbar
} wigtyp;

/* widget tracking entry */
typedef struct wigrec {

    struct wigrec* next; /* next entry in list */
    HWND           han;  /* handle to widget window */
    HWND           han2; /* handle to "buddy" window */
    int            id;   /* logical id of widget */
    wigtyp         typ;  /* type of widget */
    int            siz;  /* size of slider in scroll widget, in windows terms */
    WNDPROC        wprc; /* subclassed windows procedure, converted to int */
    int            low;  /* low limit of up/down control */
    int            high; /* high limit of up/down control */
    int            enb;  /* widget is enabled */

} wigrec, *wigptr;

typedef struct scncon { /* screen context */

    HDC     bdc;         /* handle for backing bitmap */
    HGDIOBJ bhn;         /* handle for bitmap object */
    HPEN    fpen;        /* foreground pen handle */
    HBRUSH  fbrush;      /* foreground brush handle */
    HPEN    fspen;       /* foreground single pixel pen */
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
    int     fcrgb;       /* current writing foreground color in rgb */
    int     bcrgb;       /* current writing background color in rgb */
    mode    fmod;        /* foreground mix mode */
    mode    bmod;        /* background mix mode */
    HFONT   font;        /* current font handle */
    fontptr cfont;       /* active font entry */
    int     cspc;        /* character spacing */
    int     lspc;        /* line spacing */
    int     attr;        /* set of active attributes */
    int     autof;       /* current status of scroll and wrap */
    int     tab[MAXTAB]; /* tabbing array */
    int     curv;        /* cursor visible */
    int     offx;        /* viewport offset x */
    int     offy;        /* viewport offset y */
    int     wextx;       /* window extent x */
    int     wexty;       /* window extent y */
    int     vextx;       /* viewport extent x */
    int     vexty;       /* viewport extent y */

} scncon, *scnptr;

typedef struct pict { /* picture tracking record */

    HBITMAP han; /* handle to bitmap */
    HDC     hdc; /* handle to DC for bitmap */
    HGDIOBJ ohn; /* handle to previous object */
    int     sx;  /* size in x */
    int     sy;  /* size in y */

} pict;

/* window description */
typedef struct winrec {

    int      parlfn;          /* logical parent */
    HWND     parhan;          /* handle to window parent */
    HWND     winhan;          /* handle to window */
    HDC      devcon;          /* device context */
    scnptr   screens[MAXCON]; /* screen contexts array */
    int      curdsp;          /* index for current display screen */
    int      curupd;          /* index for current update screen */
    /* global sets. these are the global set parameters that apply to any new
      created screen buffer */
    int      gmaxx;           /* maximum x size */
    int      gmaxy;           /* maximum y size */
    int      gmaxxg;          /* size of client area in x */
    int      gmaxyg;          /* size of client area in y */
    int      bufx;            /* buffer size x characters */
    int      bufy;            /* buffer size y characters */
    int      bufxg;           /* buffer size x pixels */
    int      bufyg;           /* buffer size y pixels */
    int      gattr;           /* current attributes */
    int      gauto;           /* state of auto */
    int      gfcrgb;          /* foreground color in rgb */
    int      gbcrgb;          /* background color in rgb */
    int      gcurv;           /* state of cursor visible */
    fontptr  gcfont;          /* current font select */
    int      gfhigh;          /* current font height */
    mode     gfmod;           /* foreground mix mode */
    mode     gbmod;           /* background mix mode */
    int      goffx;           /* viewport offset x */
    int      goffy;           /* viewport offset y */
    int      gwextx;          /* window extent x */
    int      gwexty;          /* window extent y */
    int      gvextx;          /* viewpor extent x */
    int      gvexty;          /* viewport extent y */
    int      termfnt;         /* terminal font number */
    int      bookfnt;         /* book font number */
    int      signfnt;         /* sign font number */
    int      techfnt;         /* technical font number */
    int      mb1;             /* mouse assert status button 1 */
    int      mb2;             /* mouse assert status button 2 */
    int      mb3;             /* mouse assert status button 3 */
    int      mpx, mpy;        /* mouse current position */
    int      mpxg, mpyg;      /* mouse current position graphical */
    int      nmb1;            /* new mouse assert status button 1 */
    int      nmb2;            /* new mouse assert status button 2 */
    int      nmb3;            /* new mouse assert status button 3 */
    int      nmpx, nmpy;      /* new mouse current position */
    int      nmpxg, nmpyg;    /* new mouse current position graphical */
    int      linespace;       /* line spacing in pixels */
    int      charspace;       /* character spacing in pixels */
    int      curspace;        /* size of cursor, in pixels */
    int      baseoff;         /* font baseline offset from top */
    int      shift;           /* state of shift key */
    int      cntrl;           /* state of control key */
    int      fcurdwn;         /* cursor on screen flag */
    int      numjoy;          /* number of joysticks found */
    int      joy1cap;         /* joystick 1 is captured */
    int      joy2cap;         /* joystick 2 is captured */
    int      joy1xs;          /* last joystick position 1x */
    int      joy1ys;          /* last joystick position 1y */
    int      joy1zs;          /* last joystick position 1z */
    int      joy2xs;          /* last joystick position 2x */
    int      joy2ys;          /* last joystick position 2y */
    int      joy2zs;          /* last joystick position 2z */
    int      shsize;          /* display screen size x in millimeters */
    int      svsize;          /* display screen size y in millimeters */
    int      shres;           /* display screen pixels in x */
    int      svres;           /* display screen pixels in y */
    int      sdpmx;           /* display screen find dots per meter x */
    int      sdpmy;           /* display screen find dots per meter y */
    char     inpbuf[MAXLIN];  /* input line buffer */
    int      inpptr;          /* input line index */
    int      frmrun;          /* framing timer is running */
    MMRESULT frmhan;          /* handle for framing timer */
    struct {

       MMRESULT han; /* handle for timer */
       int      rep; /* timer repeat flag */

    } timers[10];
    int      focus;           /* screen in focus */
    pict     pictbl[MAXPIC];  /* loadable pictures table */
    int      bufmod;          /* buffered screen mode */
    HMENU    menhan;          /* handle to (main) menu */
    metptr   metlst;          /* menu tracking list */
    wigptr   wiglst;          /* widget tracking list */
    int      frame;           /* frame on/off */
    int      size;            /* size bars on/off */
    int      sysbar;          /* system bar on/off */
    int      sizests;         /* last resize status save */
    int      visible;         /* window is visible */

} winrec, *winptr;

/* Event queuing.
   Events are kept as a double linked "bubble" to ease their FIFO status. */
typedef struct eqerec {

    ami_evtrec evt;       /* event */
    struct eqerec* last; /* last list pointer */
    struct eqerec* next; /* next list pointer */

} eqerec, *eqeptr;

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec {

      FILE* sfp;  /* file pointer used to establish entry, or NULL */
      winptr win; /* associated window (if exists) */
      int inw;    /* entry is input linked to window */
      int inl;    /* this output file is linked to the input file, logical */
      eqeptr evt; /* event queue */

} filrec, *filptr;

/* Intermessage entries. These are kept to allow communication between
   tasks. */
typedef enum {

    imalert,    /* present alert */
    imqcolor,   /* query color */
    imqopen,    /* query file open */
    imqsave,    /* query file save */
    imqfind,    /* query find */
    imqfindrep, /* query find/replace */
    imqfont,    /* query font */
    imupdown,   /* up/down control */
    imwidget,  /* general purpose widget handler */

} imcode;

typedef struct imrec { /* intermessage record */

    struct imrec* next; /* next message in list */
    imcode im; /* message type */
    wigtyp wt; /* widget type */
    union { /* intermessage type */

        struct { /* imalert */

            char* alttit; /* title string pointer */
            char* altmsg; /* message string pointer */

        };
        struct { /* imqcolor */

            int clrred;   /* colors */
            int clrgreen;
            int clrblue;

        };
        struct { /* imqopen */

            char* opnfil; /* filename to open */

        };
        struct { /* imqsave */

            char* savfil; /* filename to save */

        };
        struct { /* imqfind */

            char* fndstr; /* string to find */
            int   fndopt; /* find options */
            HWND  fndhan; /* dialog window handle */

        };
        struct { /* imqfindrep */

            char* fnrsch; /* string to search for */
            char* fnrrep; /* string to replace */
            int   fnropt; /* options */
            HWND  fnrhan; /* dialog window handle */

        };
        struct { /* imqfont */

            char* fntstr; /* font string */
            int fnteff;   /* font effects */
            int fntfr;    /* foreground red */
            int fntfg;    /* foreground green */
            int fntfb;    /* foreground blue */
            int fntbr;    /* background red */
            int fntbg;    /* background green */
            int fntbb;    /* bakcground blue */
            int fntsiz;   /* size */

        };
        struct { /* imupdown */

            int     udflg;    /* flags */
            int     udx;      /* coordinates x */
            int     udy;      /* coordinates y */
            int     udcx;     /* width */
            int     udcy;     /* height */
            HWND    udpar;    /* parent window */
            int     udid;     /* id */
            HMODULE udinst;   /* instance */
            HWND    udbuddy;  /* buddy window handle */
            int     udup;     /* upper bound */
            int     udlow;    /* lower bound */
            int     udpos;    /* control position */
            HWND    udhan;    /* returns handle to control */

        };
        struct { /* imwidget */

            char*   wigcls; /* class string */
            char*   wigtxt; /* text label string */
            int     wigflg; /* flags */
            int     wigx;   /* origin x */
            int     wigy;   /* origin y */
            int     wigw;   /* width */
            int     wigh;   /* height */
            HWND    wigpar; /* parent window handle */
            int     wigid;  /* widget id */
            HMODULE wigmod; /* module */
            HWND    wigwin; /* handle to widget */
            HWND    wigscl; /* handle to superclass window */

        };

    };

} imrec, *imptr;

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
    enoopn,   /* Cannot open file */
    einvfil,  /* File is invalid */
    eftntl,   /* font name too large */
    estrtl,   /* string too long for destination */
    enofnt,   /* Cannot create font */
    enosel,   /* Cannot select object */
    ecfgval,  /* invalid configuration value */
    efntnf,   /* no matching font found */
    einvpff,  /* invalid picture file format */
    esystem   /* System consistency check */

} errcod;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overridden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static plseek_t  ofplseek;

static filptr opnfil[MAXFIL]; /* open files table */
static int xltwin[MAXFIL]; /* window equivalence table */
static int filwin[MAXFIL]; /* file to window equivalence table */

static int       fi;           /* index for files table */
static int       fend;         /* end of program ordered flag */
static int       fautohold;    /* automatic hold on exit flag */
static char*     pgmnam;       /* program name string */
static char*     trmnam;       /* program termination string */
/* These are duplicates from the windows record. They must be here because
   Windows calls us back, and the results have to be passed via a global. */
static fontptr   fntlst;       /* list of windows fonts */
static int       fntcnt;       /* number of fonts in font list */
static ami_evtrec er;           /* event record */
static int       r;            /* result holder */
static int       b;            /* int result holder */
static eqeptr    eqefre;       /* free event queuing entry list */
static wigptr    wigfre;       /* free widget entry list */
/* message input queue */
static MSG       msgque[MAXMSG];
static int       msginp;       /* input pointer */
static int       msgout;       /* ouput pointer */
static HANDLE    msgrdy;       /* message ready event */
/* Control message queue. We send messages around for internal controls, but
  we don"t want to discard user messages to get them. So we use a separate
  queue to store control messages. */
static MSG       imsgque[MAXMSG];
static int       imsginp;      /* input pointer */
static int       imsgout;      /* ouput pointer */
static HANDLE    imsgrdy;      /* message ready event */
/* this array stores color choices from the user in the color pick dialog */
static COLORREF  gcolorsav[16];
static int       i;            /* index for that */
static int       fndrepmsg;    /* message assignment for find/replace */
static HWND      dispwin;      /* handle to display thread window */
static HWND      dialogwin;    /* handle to dialog thread window */
static HANDLE    threadstart;  /* thread start event handle */
static DWORD     threadid;     /* dummy thread id (unused) */
static HANDLE    threadhdl;    /* windows thread handle */
static HWND      mainwin;      /* handle to main thread dummy window */
static int       mainthreadid; /* main thread id */
/* This block communicates with the subthread to create standard windows. */
static int       stdwinflg;    /* flags */
static int       stdwinx;      /* x position */
static int       stdwiny;      /* y position */
static int       stdwinw;      /* width */
static int       stdwinh;      /* height */
static HWND      stdwinpar;    /* parent */
static HWND      stdwinwin;    /* window window handle */
static int       stdwinj1c;    /* joystick 1 capture */
static int       stdwinj2c;    /* joystick 1 capture */
/* lock for all global structures */
CRITICAL_SECTION mainlock;     /* main task lock */
static imptr     freitm;       /* intratask message free list */
static ami_pevthan evthan[ami_ettabbar+1]; /* array of event handler routines */
static ami_pevthan evtshan;     /* single master event handler routine */

/* The double fault flag is set when exiting, so if we exit again, it
  is checked,  forces an immediate exit. This keeps faults from
  looping. */
static int       dblflt;       /* double fault flag */

/* config settable runtime options */
static int maxxd;     /* default window dimensions */
static int maxyd;
static int dialogerr; /* send runtime errors to dialog */
static int mouseenb;  /* enable mouse */
static int joyenb;    /* enable joysticks */
static int dmpmsg;    /* enable dump Windows API messages */
static int dmpevt;    /* enable dump Petit-Ami messages */

/*
 * Forward declarations.
 *
 */
static void clswin(int fn);
static LRESULT CALLBACK wndproc(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK wndprocdialog(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam);
void ami_alert(char* title, char* message);
static void error(errcod e);

/******************************************************************************

Print string to debug dialog

Outputs the given string into a debug dialog. Used for debug purposes.

******************************************************************************/

static void diastr(char* s)

{

    int r;

    r = MessageBox(0, s, "Debug message", MB_OK);

}

/*******************************************************************************

Print contents of open file table

A diagnostic. Prints out all open records from the files table.

*******************************************************************************/

static void prtfil(void)

{

    int i; /* index for files table */

    for (i = 0; i < MAXFIL; i++) /* traverse table */
        if (opnfil[i]) { /* print file record */

        fprintf(stderr, "File: %d", i);
        fprintf(stderr, " Win: ");
        if (opnfil[i]->win) fprintf(stderr, "yes");
        else fprintf(stderr, "no");
        fprintf(stderr, " Input side of: ");
        if (opnfil[i]->inw) fprintf(stderr, "yes");
        else fprintf(stderr, "no");
        fprintf(stderr, " link to file: %d", opnfil[i]->inl);
        fprintf(stderr, " Queue is: ");
        if (opnfil[i]->evt) fprintf(stderr, "nonempty\n");
        else fprintf(stderr, "empty\n");

    }

}

/*******************************************************************************

Print menu

A diagnostic. Prints out the given menu.

*******************************************************************************/

static void dooff(int offset)

{

    int i;


    for (i = 1; i <= offset; i++) fputc(' ', stderr);

}

static void prtmenuelm(ami_menuptr m, int offset)

{

    while (m) { /* list entries */

        /* print menu entries */
        dooff(offset); fprintf(stderr, "Onoff:  %d\n", m->onoff);
        dooff(offset); fprintf(stderr, "Oneof:  %d\n", m->oneof);
        dooff(offset); fprintf(stderr, "Bar:    %d\n", m->bar);
        dooff(offset); fprintf(stderr, "Id:     %d\n", m->id);
        dooff(offset); fprintf(stderr, "Face:   %s\n", m->face);
        fprintf(stderr, "\n");
        /* if branch exists, print that list as sublist */
        if (m->branch) prtmenuelm(m->branch, offset+3);
        m = m->next; /* next entry */

    }

}

static void prtmenu(ami_menuptr m)

{

    fprintf(stderr, "Menu:\n");
    fprintf(stderr, "\n");
    prtmenuelm(m, 0);
    fprintf(stderr, "\n");

}

/*******************************************************************************

Print widget

A diagnostic. Prints the contents of a widget.

*******************************************************************************/

static void prtwig(wigptr wp)

{

    fprintf(stderr, "Window handle: ");
    fprintf(stderr, "%p", wp->han);
    fprintf(stderr, " \"buddy\" Window handle: ");
    fprintf(stderr, "%p", wp->han2);
    fprintf(stderr, " Logical id: %d", wp->id);
    fprintf(stderr, " Type: ");
    switch (wp->typ) { /* widget */

        case wtbutton:      fprintf(stderr, "Button"); break;
        case wtcheckbox:    fprintf(stderr, "Checkbox"); break;
        case wtradiobutton: fprintf(stderr, "Radio Button"); break;
        case wtgroup:       fprintf(stderr, "Group Box"); break;
        case wtbackground:  fprintf(stderr, "Background Box"); break;
        case wtscrollvert:  fprintf(stderr, "Vertical Scroll"); break;
        case wtscrollhoriz: fprintf(stderr, "Horizontal Scroll"); break;
        case wtnumselbox:   fprintf(stderr, "Number Select Box"); break;
        case wteditbox:     fprintf(stderr, "Edit Box"); break;
        case wtprogressbar: fprintf(stderr, "Progress Bar"); break;
        case wtlistbox:     fprintf(stderr, "List Box"); break;
        case wtdropbox:     fprintf(stderr, "Drop Box"); break;
        case wtdropeditbox: fprintf(stderr, "Drop Edit Box"); break;
        case wtslidehoriz:  fprintf(stderr, "Horizontal Slider"); break;
        case wtslidevert:   fprintf(stderr, "Vertical Slider"); break;
        case wttabbar:      fprintf(stderr, "Tab Bar"); break;

    }
    if (wp->typ == wtscrollvert || wp->typ == wtscrollhoriz)
        fprintf(stderr, " Slider size: %d", wp->siz);

}

/*******************************************************************************

Print widget list

A diagnostic. Prints the contents of a widget list.

*******************************************************************************/

static void prtwiglst(wigptr wp)

{

    fprintf(stderr, "Widget list\n");
    fprintf(stderr, "\n");
    while (wp) {

        prtwig(wp);
        fprintf(stderr, "\n");
        wp = wp->next;

    }
    fprintf(stderr, "\n");

}

/*******************************************************************************

Find string match

Finds if the give strings match, without regard to case.

*******************************************************************************/

static int comps(char* d, char* s)

{

    int r; /* result save */

    r = TRUE;
    while (*d && *s && tolower(*d) == tolower(*s)) { d++; s++; }
    if (*d || *s) r = FALSE;

    return (r);

}

/*******************************************************************************

Get decimal number from string

Parses a decimal number from the given string. Allows signed numbers. Provides
a flag return that indicates overflow or invalid digit.

*******************************************************************************/

static int intv(char* s, /* string containing int */
                int* err /* error occurred */
                )

{

   int r;
   char* ep;

   r = strtol(s, &ep, 10);
   *err = !!*ep;

   return (r);

}

/*******************************************************************************

Lock mutual access

Locks multiple accesses to graph globals.

*******************************************************************************/

static void lockmain(void)

{

#if MAINLOCKING
    /* int r; */

//    dbg_printf(dlinfo, "Thread id: %d\n", GetCurrentThreadId());
    EnterCriticalSection(&mainlock); /* start exclusive access */
   /* r = waitforsingleobject(mainlock, -1) */ /* start exclusive access */
/*;if r == -1  fprintf(stderr, "Lockmain: lock operation fails\cr\lf");*/
#endif

}

/*******************************************************************************

Unlock mutual access

Unlocks multiple accesses to graph globals.

*******************************************************************************/

static void unlockmain(void)

{

#if MAINLOCKING
    /* int b; */

    LeaveCriticalSection(&mainlock); /* end exclusive access */
//    dbg_printf(dlinfo, "Thread id: %d\n", GetCurrentThreadId());
   /* b = releasemutex(mainlock); */ /* end exclusive access */
/*;if (!b)  fprintf(stderr, "Unlockmain: lock operation fails\cr\lf");*/
#endif

}

/*******************************************************************************

Write error string

This is the hook replacement for the standard syslib routine. The syslib
serial method of outputing errors won"t work in the windowed environment,
because the standard output is not connected.

The error message is output in a dialog.

*******************************************************************************/

static void wrterr(char* es)

{

   /* Output in a dialog */
   ami_alert("Runtime Error", es);

}

/*******************************************************************************

Print graph error string

Prints a string in graph specific format.

*******************************************************************************/

static void grawrterr(char* es)

{

    unlockmain(); /* end exclusive access */
    if (dialogerr) ami_alert("Graphics Module", es);
    else {

        fprintf(stderr, "\nError: Graphics: ");
        fprintf(stderr, es);
        fprintf(stderr, "\n");

    }
    lockmain(); /* resume exclusive access */

}

/*******************************************************************************

Abort module

Close open files, unlock main and exit.

*******************************************************************************/

static void abortm(void)

{

    int fi; /* file index */

    /* close any open windows */
    if (!dblflt)  { /* we haven"t already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files and windows */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->win)
                clswin(fi); /* close open window */

    }
    unlockmain(); /* end exclusive access */
    fend = TRUE; /* set exit ordered */

/* for debugging, cause a system exception to route to the debugger */
#ifdef ENTDBG
    { int a, b; a = b = 0; a = a/b; }
#endif
    exit(1); /* exit */

}

/*******************************************************************************

Print error

Prints the given error in ASCII text,  aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/

static void error(errcod e)

{

    switch (e) { /* error */

        case eftbful:  grawrterr("Too many files"); break;
        case ejoyacc:  grawrterr("No joystick access available"); break;
        case etimacc:  grawrterr("No timer access available"); break;
        case einvhan:  grawrterr("Invalid file number"); break;
        case efilopr:  grawrterr("Cannot perform operation on special file"); break;
        case einvscn:  grawrterr("Invalid screen number"); break;
        case einvtab:  grawrterr("Tab position specified off screen"); break;
        case eatopos:  grawrterr("Cannot position text by pixel with auto on"); break;
        case eatocur:  grawrterr("Cannot position outside screen with auto on"); break;
        case eatoofg:  grawrterr("Cannot reenable auto off grid"); break;
        case eatoecb:  grawrterr("Cannot reenable auto outside screen"); break;
        case einvftn:  grawrterr("Invalid font number"); break;
        case etrmfnt:  grawrterr("No valid terminal font was found"); break;
        case eatofts:  grawrterr("Cannot resize font with auto enabled"); break;
        case eatoftc:  grawrterr("Cannot change fonts with auto enabled"); break;
        case einvfnm:  grawrterr("Invalid logical font number"); break;
        case efntemp:  grawrterr("Logical font number has no assigned font"); break;
        case etrmfts:  grawrterr("Cannot size terminal font"); break;
        case etabful:  grawrterr("Too many tabs set"); break;
        case eatotab:  grawrterr("Cannot set off grid tabs with auto on"); break;
        case estrinx:  grawrterr("String index out of range"); break;
        case epicfnf:  grawrterr("Picture file not found"); break;
        case epicftl:  grawrterr("Picture filename too large"); break;
        case etimnum:  grawrterr("Invalid timer number"); break;
        case ejstsys:  grawrterr("Cannot justify system font"); break;
        case efnotwin: grawrterr("File is not attached to a window"); break;
        case ewinuse:  grawrterr("Window id in use"); break;
        case efinuse:  grawrterr("File already in use"); break;
        case einmode:  grawrterr("Input side of window in wrong mode"); break;
        case edcrel:   grawrterr("Cannot release Windows device context"); break;
        case einvsiz:  grawrterr("Invalid buffer size"); break;
        case ebufoff:  grawrterr("Buffered mode not enabled"); break;
        case edupmen:  grawrterr("Menu id was duplicated"); break;
        case emennf:   grawrterr("Menu id was not found"); break;
        case ewignf:   grawrterr("Widget id was not found"); break;
        case ewigdup:  grawrterr("Widget id was duplicated"); break;
        case einvspos: grawrterr("Invalid scroll bar slider position"); break;
        case einvssiz: grawrterr("Invalid scroll bar slider size"); break;
        case ectlfal:  grawrterr("Attempt to create control fails"); break;
        case eprgpos:  grawrterr("Invalid progress bar position"); break;
        case estrspc:  grawrterr("Out of string space"); break;
        case etabbar:  grawrterr("Unable to create tab in tab bar"); break;
        case efildlg:  grawrterr("Unable to create file dialog"); break;
        case efnddlg:  grawrterr("Unable to create find dialog"); break;
        case efntdlg:  grawrterr("Unable to create font dialog"); break;
        case efndstl:  grawrterr("Find/replace string too long"); break;
        case einvwin:  grawrterr("Invalid window number"); break;
        case einvjye:  grawrterr("Invalid joystick event"); break;
        case ejoyqry:  grawrterr("Could not get information on joystick"); break;
        case einvjoy:  grawrterr("Invalid joystick ID"); break;
        case eclsinw:  grawrterr("Cannot directly close input side of window"); break;
        case ewigsel:  grawrterr("Widget is not selectable"); break;
        case ewigptxt: grawrterr("Cannot put text in this widget"); break;
        case ewiggtxt: grawrterr("Cannot get text from this widget"); break;
        case ewigdis:  grawrterr("Cannot disable this widget"); break;
        case estrato:  grawrterr("Cannot direct write string with auto on"); break;
        case etabsel:  grawrterr("Invalid tab select"); break;
        case enomem:   grawrterr("Out of memory"); break;
        case enoopn:   grawrterr("Cannot open file"); break;
        case einvfil:  grawrterr("File is invalid"); break;
        case eftntl:   grawrterr("Font name too large"); break;
        case estrtl:   grawrterr("String too long for destination"); break;
        case enofnt:   grawrterr("Cannot create font"); break;
        case enosel:   grawrterr("Cannot select object"); break;
        case ecfgval:  grawrterr("Invalid configuration value"); break;
        case efntnf:   grawrterr("No matching font found"); break;
        case einvpff:  grawrterr("Invalid picture file format"); break;
        case esystem:  grawrterr("System consistency check, please contact vendor");

    }

    abortm(); /* abort module */

}

/*******************************************************************************

Handle windows error

Only called if the last error variable is set. The text string for the error
is output, and the program halted.

*******************************************************************************/

static void winerr(void)

{

    int e;
    char *p;
    LPVOID lpMsgBuf;

    e = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, e,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0, NULL);
    unlockmain(); /* end exclusive access */
    fprintf(stderr, "\nError: Graph: Windows error: ");
    fprintf(stderr, lpMsgBuf);
    fprintf(stderr, "\n");
    lockmain(); /* resume exclusive access */

    abortm(); /* abort module */

}

/******************************************************************************

Internal version of malloc/free

This wrapper around malloc/free both handles errors and also gives us a chance
to track memory useage. Running out of memory is a terminal event, so we don't
need to return.

Only allocated memory is tallied, no accounting is done for ifree(). The vast
majority of memory used in this package is never returned.

******************************************************************************/

static void *imalloc(size_t size)

{

    void* ptr;

    ptr = malloc(size);
    if (!ptr) error(enomem);

    return ptr;

}

static void ifree(void* ptr)

{

    free(ptr);

}

/*******************************************************************************

Print message string

This routine is for diagnostic use. Comment it out on production builds.

*******************************************************************************/

static void prtmsgstr(int mn)

{

    fprintf(stderr, "%04x: ", mn);
    if (mn >= 0x800 && mn <= 0xbfff) fprintf(stderr, "User message");
    else if (mn >= 0xc000 && mn <= 0xffff) fprintf(stderr, "Registered message");
    else switch (mn) {

        case 0x0000: fprintf(stderr, "WM_NULL"); break;
        case 0x0001: fprintf(stderr, "WM_CREATE"); break;
        case 0x0002: fprintf(stderr, "WM_DESTROY"); break;
        case 0x0003: fprintf(stderr, "WM_MOVE"); break;
        case 0x0005: fprintf(stderr, "WM_SIZE"); break;
        case 0x0006: fprintf(stderr, "WM_ACTIVATE"); break;
        case 0x0007: fprintf(stderr, "WM_SETFOCUS"); break;
        case 0x0008: fprintf(stderr, "WM_KILLFOCUS"); break;
        case 0x000A: fprintf(stderr, "WM_ENABLE"); break;
        case 0x000B: fprintf(stderr, "WM_SETREDRAW"); break;
        case 0x000C: fprintf(stderr, "WM_SETTEXT"); break;
        case 0x000D: fprintf(stderr, "WM_GETTEXT"); break;
        case 0x000E: fprintf(stderr, "WM_GETTEXTLENGTH"); break;
        case 0x000F: fprintf(stderr, "WM_PAINT"); break;
        case 0x0010: fprintf(stderr, "WM_CLOSE"); break;
        case 0x0011: fprintf(stderr, "WM_QUERYENDSESSION"); break;
        case 0x0012: fprintf(stderr, "WM_QUIT"); break;
        case 0x0013: fprintf(stderr, "WM_QUERYOPEN"); break;
        case 0x0014: fprintf(stderr, "WM_ERASEBKGND"); break;
        case 0x0015: fprintf(stderr, "WM_SYSCOLORCHANGE"); break;
        case 0x0016: fprintf(stderr, "WM_ENDSESSION"); break;
        case 0x0018: fprintf(stderr, "WM_ShowWindow"); break;
        case 0x001A: fprintf(stderr, "WM_WININICHANGE"); break;
        case 0x001B: fprintf(stderr, "WM_DEVMODECHANGE"); break;
        case 0x001C: fprintf(stderr, "WM_ACTIVATEAPP"); break;
        case 0x001D: fprintf(stderr, "WM_FONTCHANGE"); break;
        case 0x001E: fprintf(stderr, "WM_TIMECHANGE"); break;
        case 0x001F: fprintf(stderr, "WM_CANCELMODE"); break;
        case 0x0020: fprintf(stderr, "WM_SETCURSOR"); break;
        case 0x0021: fprintf(stderr, "WM_MOUSEACTIVATE"); break;
        case 0x0022: fprintf(stderr, "WM_CHILDACTIVATE"); break;
        case 0x0023: fprintf(stderr, "WM_QUEUESYNC"); break;
        case 0x0024: fprintf(stderr, "WM_GETMINMAXINFO"); break;
        case 0x0026: fprintf(stderr, "WM_PAINTICON"); break;
        case 0x0027: fprintf(stderr, "WM_ICONERASEBKGND"); break;
        case 0x0028: fprintf(stderr, "WM_NEXTDLGCTL"); break;
        case 0x002A: fprintf(stderr, "WM_SPOOLERSTATUS"); break;
        case 0x002B: fprintf(stderr, "WM_DRAWITEM"); break;
        case 0x002C: fprintf(stderr, "WM_MEASUREITEM"); break;
        case 0x002D: fprintf(stderr, "WM_DELETEITEM"); break;
        case 0x002E: fprintf(stderr, "WM_VKEYTOITEM"); break;
        case 0x002F: fprintf(stderr, "WM_CHARTOITEM"); break;
        case 0x0030: fprintf(stderr, "WM_SETFONT"); break;
        case 0x0031: fprintf(stderr, "WM_GETFONT"); break;
        case 0x0032: fprintf(stderr, "WM_SETHOTKEY"); break;
        case 0x0033: fprintf(stderr, "WM_GETHOTKEY"); break;
        case 0x0037: fprintf(stderr, "WM_QUERYDRAGICON"); break;
        case 0x0039: fprintf(stderr, "WM_COMPAREITEM"); break;
        case 0x0041: fprintf(stderr, "WM_COMPACTING"); break;
        case 0x0042: fprintf(stderr, "WM_OTHERWINDOWCREATED"); break;
        case 0x0043: fprintf(stderr, "WM_OTHERWINDOWDESTROYED"); break;
        case 0x0044: fprintf(stderr, "WM_COMMNOTIFY"); break;
        case 0x0045: fprintf(stderr, "WM_HOTKEYEVENT"); break;
        case 0x0046: fprintf(stderr, "WM_WINDOWPOSCHANGING"); break;
        case 0x0047: fprintf(stderr, "WM_WINDOWPOSCHANGED"); break;
        case 0x0048: fprintf(stderr, "WM_POWER"); break;
        case 0x004A: fprintf(stderr, "WM_COPYDATA"); break;
        case 0x004B: fprintf(stderr, "WM_CANCELJOURNAL"); break;
        case 0x004E: fprintf(stderr, "WM_NOTIFY"); break;
        case 0x0050: fprintf(stderr, "WM_INPUTLANGCHANGEREQUEST"); break;
        case 0x0051: fprintf(stderr, "WM_INPUTLANGCHANGE"); break;
        case 0x0052: fprintf(stderr, "WM_TCARD"); break;
        case 0x0053: fprintf(stderr, "WM_HELP"); break;
        case 0x0054: fprintf(stderr, "WM_USERCHANGED"); break;
        case 0x0055: fprintf(stderr, "WM_NOTIFYFORMAT"); break;
        case 0x007B: fprintf(stderr, "WM_CONTEXTMENU"); break;
        case 0x007C: fprintf(stderr, "WM_STYLECHANGING"); break;
        case 0x007D: fprintf(stderr, "WM_STYLECHANGED"); break;
        case 0x007E: fprintf(stderr, "WM_DISPLAYCHANGE"); break;
        case 0x007F: fprintf(stderr, "WM_GETICON"); break;
        case 0x0080: fprintf(stderr, "WM_SETICON"); break;
        case 0x0081: fprintf(stderr, "WM_NCCREATE"); break;
        case 0x0082: fprintf(stderr, "WM_NCDESTROY"); break;
        case 0x0083: fprintf(stderr, "WM_NCCALCSIZE"); break;
        case 0x0084: fprintf(stderr, "WM_NCHITTEST"); break;
        case 0x0085: fprintf(stderr, "WM_NCPAINT"); break;
        case 0x0086: fprintf(stderr, "WM_NCACTIVATE"); break;
        case 0x0087: fprintf(stderr, "WM_GETDLGCODE"); break;
        case 0x0088: fprintf(stderr, "WM_SYNCPAINT"); break;
        case 0x00A0: fprintf(stderr, "WM_NCMOUSEMOVE"); break;
        case 0x00A1: fprintf(stderr, "WM_NCLBUTTONDOWN"); break;
        case 0x00A2: fprintf(stderr, "WM_NCLBUTTONUP"); break;
        case 0x00A3: fprintf(stderr, "WM_NCLBUTTONDBLCLK"); break;
        case 0x00A4: fprintf(stderr, "WM_NCRBUTTONDOWN"); break;
        case 0x00A5: fprintf(stderr, "WM_NCRBUTTONUP"); break;
        case 0x00A6: fprintf(stderr, "WM_NCRBUTTONDBLCLK"); break;
        case 0x00A7: fprintf(stderr, "WM_NCMBUTTONDOWN"); break;
        case 0x00A8: fprintf(stderr, "WM_NCMBUTTONUP"); break;
        case 0x00A9: fprintf(stderr, "WM_NCMBUTTONDBLCLK"); break;
        case 0x00B0: fprintf(stderr, "EM_GETSEL"); break;
        case 0x00B1: fprintf(stderr, "EM_SETSEL"); break;
        case 0x00B2: fprintf(stderr, "EM_GETRECT"); break;
        case 0x00B3: fprintf(stderr, "EM_SETRECT"); break;
        case 0x00B4: fprintf(stderr, "EM_SETRECTNP"); break;
        case 0x00B5: fprintf(stderr, "EM_SCROLL"); break;
        case 0x00B6: fprintf(stderr, "EM_LINESCROLL"); break;
        case 0x00B7: fprintf(stderr, "EM_SCROLLCARET"); break;
        case 0x00B8: fprintf(stderr, "EM_GETMODIFY"); break;
        case 0x00B9: fprintf(stderr, "EM_SETMODIFY"); break;
        case 0x00BA: fprintf(stderr, "EM_GETLINECOUNT"); break;
        case 0x00BB: fprintf(stderr, "EM_LINEINDEX"); break;
        case 0x00BC: fprintf(stderr, "EM_SETHANDLE"); break;
        case 0x00BD: fprintf(stderr, "EM_GETHANDLE"); break;
        case 0x00BE: fprintf(stderr, "EM_GETTHUMB"); break;
        case 0x00C1: fprintf(stderr, "EM_LINELENGTH"); break;
        case 0x00C2: fprintf(stderr, "EM_REPLACESEL"); break;
        case 0x00C4: fprintf(stderr, "EM_GETLINE"); break;
        case 0x00C5: fprintf(stderr, "EM_LIMITTEXT"); break;
        case 0x00C6: fprintf(stderr, "EM_CANUNDO"); break;
        case 0x00C7: fprintf(stderr, "EM_UNDO"); break;
        case 0x00C8: fprintf(stderr, "EM_FMTLINES"); break;
        case 0x00C9: fprintf(stderr, "EM_LINEFROMCHAR"); break;
        case 0x00CB: fprintf(stderr, "EM_SETTABSTOPS"); break;
        case 0x00CC: fprintf(stderr, "EM_SETPASSWORDCHAR"); break;
        case 0x00CD: fprintf(stderr, "EM_EMPTYUNDOBUFFER"); break;
        case 0x00CE: fprintf(stderr, "EM_GETFIRSTVISIBLELINE"); break;
        case 0x00CF: fprintf(stderr, "EM_SETREADONLY"); break;
        case 0x00D0: fprintf(stderr, "EM_SETWORDBREAKPROC"); break;
        case 0x00D1: fprintf(stderr, "EM_GETWORDBREAKPROC"); break;
        case 0x00D2: fprintf(stderr, "EM_GETPASSWORDCHAR"); break;
        case 0x00D3: fprintf(stderr, "EM_SETMARGINS"); break;
        case 0x00D4: fprintf(stderr, "EM_GETMARGINS"); break;
        case 0x00D5: fprintf(stderr, "EM_GETLIMITTEXT"); break;
        case 0x00D6: fprintf(stderr, "EM_POSFROMCHAR"); break;
        case 0x00D7: fprintf(stderr, "EM_CHARFROMPOS"); break;
        case 0x00D8: fprintf(stderr, "EM_SETIMESTATUS"); break;
        case 0x00D9: fprintf(stderr, "EM_GETIMESTATUS"); break;
        case 0x00F0: fprintf(stderr, "BM_GETCHECK"); break;
        case 0x00F1: fprintf(stderr, "BM_SETCHECK"); break;
        case 0x00F2: fprintf(stderr, "BM_GETSTATE"); break;
        case 0x00F3: fprintf(stderr, "BM_SETSTATE"); break;
        case 0x00F4: fprintf(stderr, "BM_SETSTYLE"); break;
        case 0x00F5: fprintf(stderr, "BM_CLICK"); break;
        case 0x00F6: fprintf(stderr, "BM_GETIMAGE"); break;
        case 0x00F7: fprintf(stderr, "BM_SETIMAGE"); break;
    /*  case 0x0100: fprintf(stderr, "WM_KEYFIRST"); break;*/
        case 0x0100: fprintf(stderr, "WM_KEYDOWN"); break;
        case 0x0101: fprintf(stderr, "WM_KEYUP"); break;
        case 0x0102: fprintf(stderr, "WM_CHAR"); break;
        case 0x0103: fprintf(stderr, "WM_DEADCHAR"); break;
        case 0x0104: fprintf(stderr, "WM_SYSKEYDOWN"); break;
        case 0x0105: fprintf(stderr, "WM_SYSKEYUP"); break;
        case 0x0106: fprintf(stderr, "WM_SYSCHAR"); break;
        case 0x0107: fprintf(stderr, "WM_SYSDEADCHAR"); break;
        case 0x0108: fprintf(stderr, "WM_KEYLAST"); break;
        case 0x0109: fprintf(stderr, "WM_UNICHAR"); break;
        case 0x0110: fprintf(stderr, "WM_INITDIALOG"); break;
        case 0x0111: fprintf(stderr, "WM_COMMAND"); break;
        case 0x0112: fprintf(stderr, "WM_SYSCOMMAND"); break;
        case 0x0113: fprintf(stderr, "WM_TIMER"); break;
        case 0x0114: fprintf(stderr, "WM_HSCROLL"); break;
        case 0x0115: fprintf(stderr, "WM_VSCROLL"); break;
        case 0x0116: fprintf(stderr, "WM_INITMENU"); break;
        case 0x0117: fprintf(stderr, "WM_INITMENUPOPUP"); break;
        case 0x011F: fprintf(stderr, "WM_MENUSELECT"); break;
        case 0x0120: fprintf(stderr, "WM_MENUCHAR"); break;
        case 0x0121: fprintf(stderr, "WM_ENTERIDLE"); break;
        case 0x0132: fprintf(stderr, "WM_CTLCOLORMSGBOX"); break;
        case 0x0133: fprintf(stderr, "WM_CTLCOLOREDIT"); break;
        case 0x0134: fprintf(stderr, "WM_CTLCOLORLISTBOX"); break;
        case 0x0135: fprintf(stderr, "WM_CTLCOLORBTN"); break;
        case 0x0136: fprintf(stderr, "WM_CTLCOLORDLG"); break;
        case 0x0137: fprintf(stderr, "WM_CTLCOLORSCROLLBAR"); break;
        case 0x0138: fprintf(stderr, "WM_CTLCOLORSTATIC"); break;

        case 0x140: fprintf(stderr, "CB_GETEDITSEL"); break;
        case 0x141: fprintf(stderr, "CB_LIMITTEXT"); break;
        case 0x142: fprintf(stderr, "CB_SETEDITSEL"); break;
        case 0x143: fprintf(stderr, "CB_ADDSTRING"); break;
        case 0x144: fprintf(stderr, "CB_DELETESTRING"); break;
        case 0x145: fprintf(stderr, "CB_DIR"); break;
        case 0x146: fprintf(stderr, "CB_GETCOUNT"); break;
        case 0x147: fprintf(stderr, "CB_GETCURSEL"); break;
        case 0x148: fprintf(stderr, "CB_GETLBTEXT"); break;
        case 0x149: fprintf(stderr, "CB_GETLBTEXTLEN"); break;
        case 0x14A: fprintf(stderr, "CB_INSERTSTRING"); break;
        case 0x14B: fprintf(stderr, "CB_RESETCONTENT"); break;
        case 0x14C: fprintf(stderr, "CB_FINDSTRING"); break;
        case 0x14D: fprintf(stderr, "CB_SELECTSTRING"); break;
        case 0x14E: fprintf(stderr, "CB_SETCURSEL"); break;
        case 0x14F: fprintf(stderr, "CB_SHOWDROPDOWN"); break;
        case 0x150: fprintf(stderr, "CB_GETITEMDATA"); break;
        case 0x151: fprintf(stderr, "CB_SETITEMDATA"); break;
        case 0x152: fprintf(stderr, "CB_GETDROPPEDCONTROLRECT"); break;
        case 0x153: fprintf(stderr, "CB_SETITEMHEIGHT"); break;
        case 0x154: fprintf(stderr, "CB_GETITEMHEIGHT"); break;
        case 0x155: fprintf(stderr, "CB_SETEXTENDEDUI"); break;
        case 0x156: fprintf(stderr, "CB_GETEXTENDEDUI"); break;
        case 0x157: fprintf(stderr, "CB_GETDROPPEDSTATE"); break;
        case 0x158: fprintf(stderr, "CB_FINDSTRINGEXACT"); break;
        case 0x159: fprintf(stderr, "CB_SETLOCALE"); break;
        case 0x15A: fprintf(stderr, "CB_GETLOCALE"); break;
        case 0x15B: fprintf(stderr, "CB_GETTOPINDEX"); break;
        case 0x15C: fprintf(stderr, "CB_SETTOPINDEX"); break;
        case 0x15D: fprintf(stderr, "CB_GETHORIZONTALEXTENT"); break;
        case 0x15E: fprintf(stderr, "CB_SETHORIZONTALEXTENT"); break;
        case 0x15F: fprintf(stderr, "CB_GETDROPPEDWIDTH"); break;
        case 0x160: fprintf(stderr, "CB_SETDROPPEDWIDTH"); break;
        case 0x161: fprintf(stderr, "CB_INITSTORAGE"); break;
        case 0x163: fprintf(stderr, "CB_MULTIPLEADDSTRING"); break;
        case 0x164: fprintf(stderr, "CB_GETCOMBOBOXINFO"); break;

     /*   case 0x0200: fprintf(stderr, "WM_MOUSEFIRST"); break; */
        case 0x0200: fprintf(stderr, "WM_MOUSEMOVE"); break;
        case 0x0201: fprintf(stderr, "WM_LBUTTONDOWN"); break;
        case 0x0202: fprintf(stderr, "WM_LBUTTONUP"); break;
        case 0x0203: fprintf(stderr, "WM_LBUTTONDBLCLK"); break;
        case 0x0204: fprintf(stderr, "WM_RBUTTONDOWN"); break;
        case 0x0205: fprintf(stderr, "WM_RBUTTONUP"); break;
        case 0x0206: fprintf(stderr, "WM_RBUTTONDBLCLK"); break;
        case 0x0207: fprintf(stderr, "WM_MBUTTONDOWN"); break;
        case 0x0208: fprintf(stderr, "WM_MBUTTONUP"); break;
        case 0x0209: fprintf(stderr, "WM_MBUTTONDBLCLK"); break;
      /*   case 0x0209: fprintf(stderr, "WM_MOUSELAST"); break; */
        case 0x0210: fprintf(stderr, "WM_PARENTNOTIFY"); break;
        case 0x0211: fprintf(stderr, "WM_ENTERMENULOOP"); break;
        case 0x0212: fprintf(stderr, "WM_EXITMENULOOP"); break;
        case 0x0213: fprintf(stderr, "WM_NEXTMENU"); break;
        case 0x0214: fprintf(stderr, "WM_SIZING"); break;
        case 0x0215: fprintf(stderr, "WM_CAPTURECHANGED"); break;
        case 0x0216: fprintf(stderr, "WM_MOVING"); break;
        case 0x0218: fprintf(stderr, "WM_POWERBROADCAST"); break;
        case 0x0220: fprintf(stderr, "WM_MDICREATE"); break;
        case 0x0221: fprintf(stderr, "WM_MDIDESTROY"); break;
        case 0x0222: fprintf(stderr, "WM_MDIACTIVATE"); break;
        case 0x0223: fprintf(stderr, "WM_MDIRESTORE"); break;
        case 0x0224: fprintf(stderr, "WM_MDINEXT"); break;
        case 0x0225: fprintf(stderr, "WM_MDIMAXIMIZE"); break;
        case 0x0226: fprintf(stderr, "WM_MDITILE"); break;
        case 0x0227: fprintf(stderr, "WM_MDICASCADE"); break;
        case 0x0228: fprintf(stderr, "WM_MDIICONARRANGE"); break;
        case 0x0229: fprintf(stderr, "WM_MDIGETACTIVE"); break;
        case 0x0230: fprintf(stderr, "WM_MDISetMenu"); break;
        case 0x0231: fprintf(stderr, "WM_ENTERSIZEMOVE"); break;
        case 0x0232: fprintf(stderr, "WM_EXITSIZEMOVE"); break;
        case 0x0233: fprintf(stderr, "WM_DROPFILES"); break;
        case 0x0234: fprintf(stderr, "WM_MDIREFRESHMENU"); break;
        case 0x0281: fprintf(stderr, "WM_IME_SETCONTEXT"); break;
        case 0x0282: fprintf(stderr, "WM_IME_NOTIFY"); break;
        case 0x0283: fprintf(stderr, "WM_IME_CONTROL"); break;
        case 0x0284: fprintf(stderr, "WM_IME_COMPOSITIONFULL"); break;
        case 0x0285: fprintf(stderr, "WM_IME_SELECT"); break;
        case 0x0286: fprintf(stderr, "WM_IME_CHAR"); break;
        case 0x0290: fprintf(stderr, "WM_IME_KEYDOWN"); break;
        case 0x0291: fprintf(stderr, "WM_IME_KEYUP"); break;
        case 0x02A1: fprintf(stderr, "WM_MOUSEHOVER"); break;
        case 0x02A2: fprintf(stderr, "WM_NCMOUSELEAVE"); break;
        case 0x02A3: fprintf(stderr, "WM_MOUSELEAVE"); break;
        case 0x0300: fprintf(stderr, "WM_CUT"); break;
        case 0x0301: fprintf(stderr, "WM_COPY"); break;
        case 0x0302: fprintf(stderr, "WM_PASTE"); break;
        case 0x0303: fprintf(stderr, "WM_CLEAR"); break;
        case 0x0304: fprintf(stderr, "WM_UNDO"); break;
        case 0x0305: fprintf(stderr, "WM_RENDERFORMAT"); break;
        case 0x0306: fprintf(stderr, "WM_RENDERALLFORMATS"); break;
        case 0x0307: fprintf(stderr, "WM_DESTROYCLIPBOARD"); break;
        case 0x0308: fprintf(stderr, "WM_DRAWCLIPBOARD"); break;
        case 0x0309: fprintf(stderr, "WM_PAINTCLIPBOARD"); break;
        case 0x030A: fprintf(stderr, "WM_VSCROLLCLIPBOARD"); break;
        case 0x030B: fprintf(stderr, "WM_SIZECLIPBOARD"); break;
        case 0x030C: fprintf(stderr, "WM_ASKCBFORMATNAME"); break;
        case 0x030D: fprintf(stderr, "WM_CHANGECBCHAIN"); break;
        case 0x030E: fprintf(stderr, "WM_HSCROLLCLIPBOARD"); break;
        case 0x030F: fprintf(stderr, "WM_QUERYNEWPALETTE"); break;
        case 0x0310: fprintf(stderr, "WM_PALETTEISCHANGING"); break;
        case 0x0311: fprintf(stderr, "WM_PALETTECHANGED"); break;
        case 0x0312: fprintf(stderr, "WM_HOTKEY"); break;
        case 0x0380: fprintf(stderr, "WM_PENWINFIRST"); break;
        case 0x038F: fprintf(stderr, "WM_PENWINLAST"); break;
        case 0x0390: fprintf(stderr, "WM_COALESCE_FIRST"); break;
        case 0x039F: fprintf(stderr, "WM_COALESCE_LAST"); break;
        case 0x03A0: fprintf(stderr, "MM_JOY1MOVE"); break;
        case 0x03A1: fprintf(stderr, "MM_JOY2MOVE"); break;
        case 0x03A2: fprintf(stderr, "MM_JOY1ZMOVE"); break;
        case 0x03A3: fprintf(stderr, "MM_JOY2ZMOVE"); break;
        case 0x03B5: fprintf(stderr, "MM_JOY1BUTTONDOWN"); break;
        case 0x03B6: fprintf(stderr, "MM_JOY2BUTTONDOWN"); break;
        case 0x03B7: fprintf(stderr, "MM_JOY1BUTTONUP"); break;
        case 0x03B8: fprintf(stderr, "MM_JOY2BUTTONUP"); break;
        case 0x03E0: fprintf(stderr, "WM_DDE_FIRST"); break;
        /* case 0x03E0: fprintf(stderr, "WM_DDE_INITIATE"); break; */
        case 0x03E1: fprintf(stderr, "WM_DDE_TERMINATE"); break;
        case 0x03E2: fprintf(stderr, "WM_DDE_ADVISE"); break;
        case 0x03E3: fprintf(stderr, "WM_DDE_UNADVISE"); break;
        case 0x03E4: fprintf(stderr, "WM_DDE_ACK"); break;
        case 0x03E5: fprintf(stderr, "WM_DDE_DATA"); break;
        case 0x03E6: fprintf(stderr, "WM_DDE_REQUEST"); break;
        case 0x03E7: fprintf(stderr, "WM_DDE_POKE"); break;
        case 0x03E8: fprintf(stderr, "WM_DDE_EXECUTE"); break;
        /* case 0x03E8: fprintf(stderr, "WM_DDE_LAST"); break; */

        /* user defined codes (from this module) */
        case UM_MAKWIN: fprintf(stderr, "UM_MAKWIN"); break;
        case UM_WINSTR: fprintf(stderr, "UM_WINSTR"); break;
        case UM_CLSWIN: fprintf(stderr, "UM_CLSWIN"); break;
        case UM_WINCLS: fprintf(stderr, "UM_WINCLS"); break;
        case UM_IM:     fprintf(stderr, "UM_IM"); break;
        case UM_EDITCR: fprintf(stderr, "UM_EDITCR"); break;
        case UM_NUMCR:  fprintf(stderr, "UM_NUMCR"); break;

        default: fprintf(stderr, "???"); break;

    }

}

/*******************************************************************************

Print message diagnostic

This routine is for diagnostic use. Comment it out on production builds.

*******************************************************************************/

static void prtmsg(MSG* m)

{

    fprintf(stderr, "handle: %p", m->hwnd);
    fprintf(stderr, " message: ");
    prtmsgstr(m->message);
    fprintf(stderr, " wparam: %08x", m->wParam);
    fprintf(stderr, " lparam: %08x", m->lParam);
    fprintf(stderr, "\n");

}

/*******************************************************************************

Print unpacked message diagnostic

This routine is for diagnostic use. Comment it out on production builds.

*******************************************************************************/

static void prtmsgu(HWND hwnd, int imsg, int wparam, int lparam)

{

    dbg_printf(dlinfo, "handle: %p message: ", hwnd);
    prtmsgstr(imsg);
    fprintf(stderr, " wparam: %08x", wparam);
    fprintf(stderr, " lparam: %08x", lparam);
    fprintf(stderr, "\n");

}

/******************************************************************************

Print event symbol

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

void prtevtcod(ami_evtcod e)

{

    switch (e) {

        case ami_etchar:    fprintf(stderr, "etchar"); break;
        case ami_etup:      fprintf(stderr, "etup"); break;
        case ami_etdown:    fprintf(stderr, "etdown"); break;
        case ami_etleft:    fprintf(stderr, "etleft"); break;
        case ami_etright:   fprintf(stderr, "etright"); break;
        case ami_etleftw:   fprintf(stderr, "etleftw"); break;
        case ami_etrightw:  fprintf(stderr, "etrightw"); break;
        case ami_ethome:    fprintf(stderr, "ethome"); break;
        case ami_ethomes:   fprintf(stderr, "ethomes"); break;
        case ami_ethomel:   fprintf(stderr, "ethomel"); break;
        case ami_etend:     fprintf(stderr, "etend"); break;
        case ami_etends:    fprintf(stderr, "etends"); break;
        case ami_etendl:    fprintf(stderr, "etendl"); break;
        case ami_etscrl:    fprintf(stderr, "etscrl"); break;
        case ami_etscrr:    fprintf(stderr, "etscrr"); break;
        case ami_etscru:    fprintf(stderr, "etscru"); break;
        case ami_etscrd:    fprintf(stderr, "etscrd"); break;
        case ami_etpagd:    fprintf(stderr, "etpagd"); break;
        case ami_etpagu:    fprintf(stderr, "etpagu"); break;
        case ami_ettab:     fprintf(stderr, "ettab"); break;
        case ami_etenter:   fprintf(stderr, "etenter"); break;
        case ami_etinsert:  fprintf(stderr, "etinsert"); break;
        case ami_etinsertl: fprintf(stderr, "etinsertl"); break;
        case ami_etinsertt: fprintf(stderr, "etinsertt"); break;
        case ami_etdel:     fprintf(stderr, "etdel"); break;
        case ami_etdell:    fprintf(stderr, "etdell"); break;
        case ami_etdelcf:   fprintf(stderr, "etdelcf"); break;
        case ami_etdelcb:   fprintf(stderr, "etdelcb"); break;
        case ami_etcopy:    fprintf(stderr, "etcopy"); break;
        case ami_etcopyl:   fprintf(stderr, "etcopyl"); break;
        case ami_etcan:     fprintf(stderr, "etcan"); break;
        case ami_etstop:    fprintf(stderr, "etstop"); break;
        case ami_etcont:    fprintf(stderr, "etcont"); break;
        case ami_etprint:   fprintf(stderr, "etprint"); break;
        case ami_etprintb:  fprintf(stderr, "etprintb"); break;
        case ami_etprints:  fprintf(stderr, "etprints"); break;
        case ami_etfun:     fprintf(stderr, "etfun"); break;
        case ami_etmenu:    fprintf(stderr, "etmenu"); break;
        case ami_etmouba:   fprintf(stderr, "etmouba"); break;
        case ami_etmoubd:   fprintf(stderr, "etmoubd"); break;
        case ami_etmoumov:  fprintf(stderr, "etmoumov"); break;
        case ami_ettim:     fprintf(stderr, "ettim"); break;
        case ami_etjoyba:   fprintf(stderr, "etjoyba"); break;
        case ami_etjoybd:   fprintf(stderr, "etjoybd"); break;
        case ami_etjoymov:  fprintf(stderr, "etjoymov"); break;
        case ami_etresize:  fprintf(stderr, "etresize"); break;
        case ami_etterm:    fprintf(stderr, "etterm"); break;
        case ami_etmoumovg: fprintf(stderr, "etmoumovg"); break;
        case ami_etframe:   fprintf(stderr, "etframe"); break;
        case ami_etredraw:  fprintf(stderr, "etredraw"); break;
        case ami_etmin:     fprintf(stderr, "etmin"); break;
        case ami_etmax:     fprintf(stderr, "etmax"); break;
        case ami_etnorm:    fprintf(stderr, "etnorm"); break;
        case ami_etmenus:   fprintf(stderr, "etmenus"); break;
        case ami_etbutton:  fprintf(stderr, "etbutton"); break;
        case ami_etchkbox:  fprintf(stderr, "etchkbox"); break;
        case ami_etradbut:  fprintf(stderr, "etradbut"); break;
        case ami_etsclull:  fprintf(stderr, "etsclull"); break;
        case ami_etscldrl:  fprintf(stderr, "etscldrl"); break;
        case ami_etsclulp:  fprintf(stderr, "etsclulp"); break;
        case ami_etscldrp:  fprintf(stderr, "etscldrp"); break;
        case ami_etsclpos:  fprintf(stderr, "etsclpos"); break;
        case ami_etedtbox:  fprintf(stderr, "etedtbox"); break;
        case ami_etnumbox:  fprintf(stderr, "etnumbox"); break;
        case ami_etlstbox:  fprintf(stderr, "etlstbox"); break;
        case ami_etdrpbox:  fprintf(stderr, "etdrpbox"); break;
        case ami_etdrebox:  fprintf(stderr, "etdrebox"); break;
        case ami_etsldpos:  fprintf(stderr, "etsldpos"); break;
        case ami_ettabbar:   fprintf(stderr, "ettabbar"); break;

        default: fprintf(stderr, "???");

    }

}

/** ****************************************************************************

Print event record

A diagnostic, prints the given event with possible parameters.

*******************************************************************************/

static void prtevt(ami_evtrec* ev)

{

    prtevtcod(ev->etype);
    fprintf(stderr, " Window: %d Handled: %d", ev->winid, ev->handled);
    switch (ev->etype) {

        case ami_etchar: fprintf(stderr, " Char: %c", ev->echar); break;
        case ami_ettim: fprintf(stderr, " Timer: %d", ev->timnum); break;
        case ami_etmoumov:
            fprintf(stderr, " Mouse: %d x: %d y: %d", ev->mmoun, ev->moupx,
                    ev->moupy);
            break;
        case ami_etmouba:
            fprintf(stderr, " Mouse: %d Button: %d", ev->amoun, ev->amoubn);
            break;
        case ami_etmoubd:
            fprintf(stderr, " Mouse: %d Button: %d", ev->dmoun, ev->dmoubn);
            break;
        case ami_etjoyba:
            fprintf(stderr, " Joystick: %d Button: %d", ev->ajoyn, ev->ajoybn);
            break;
        case ami_etjoybd:
            fprintf(stderr, " Joystick: %d Button: %d", ev->djoyn, ev->djoybn);
            break;
        case ami_etjoymov:
            fprintf(stderr, " Joystick: %d x: %d y: %d z: %d", ev->mjoyn,
                    ev->joypx, ev->joypy, ev->joypz);
            break;
        case ami_etfun: fprintf(stderr, " Function key: %d", ev->fkey);
        case ami_etmoumovg:
            fprintf(stderr, " Mouse: %d x: %d y: %d", ev->mmoung, ev->moupxg,
                    ev->moupyg);
            break;
        case ami_etredraw:
            fprintf(stderr, " bounds: sx: %d sy: %d ex: %d ey: %d", ev->rsx,
                    ev->rsy, ev->rex, ev->rey);
            break;
        case ami_etmenus: fprintf(stderr, " Menu: %d", ev->menuid); break;
        case ami_etbutton: fprintf(stderr, " Button: %d", ev->butid); break;
        case ami_etchkbox: fprintf(stderr, " Checkbox: %d", ev->ckbxid); break;
        case ami_etradbut: fprintf(stderr, " Button: %d", ev->radbid); break;
        case ami_etsclull: fprintf(stderr, " Scroll bar: %d", ev->sclulid); break;
        case ami_etscldrl: fprintf(stderr, " Scroll bar: %d", ev->scldrid); break;
        case ami_etsclulp: fprintf(stderr, " Scroll bar: %d", ev->sclupid); break;
        case ami_etscldrp: fprintf(stderr, " Scroll bar: %d", ev->scldpid); break;
        case ami_etsclpos:
            fprintf(stderr, " Scroll bar: %d position: %d", ev->sclpid,
                    ev->sclpos);
            break;
        case ami_etedtbox: fprintf(stderr, " Edit box: %d", ev->edtbid); break;
        case ami_etnumbox:
            fprintf(stderr, " Number box: %d value: %d", ev->numbid,
                    ev->numbsl);
            break;
        case ami_etlstbox:
            fprintf(stderr, " List box: %d select: %d", ev->lstbid, ev->lstbsl);
            break;
        case ami_etdrpbox:
            fprintf(stderr, " Drop box: %d select: %d", ev->drpbid, ev->drpbsl);
            break;
        case ami_etdrebox: fprintf(stderr, " Drop edit box: %d", ev->drebid);
            break;
        case ami_etsldpos:
            fprintf(stderr, " Slider: %d position: %d", ev->sldpid, ev->sldpos);
            break;
        case ami_ettabbar:
            fprintf(stderr, " Tab bar: %d select: %d", ev->tabid, ev->tabsel);
            break;

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

Find next queue pointer

Finds the next value of a queue pointer, which wraps around at the end.

*******************************************************************************/

static int next(int mi)

{

    mi++; /* next message */
    if (mi >= MAXMSG) mi = 0; /* if at end, wrap */

    return (mi); /* return result */

}

/*******************************************************************************

Place message entry in queue

Places a message into the input queue. If the queue is full, overwrites the
oldest event.

We implement a "paint packing" option here. We track if paint messages were
put into the queue, and if so, new paint messages are merged with the existing
paint message by finding the least encompassing rectangle. This can save
a lot of redraw thrashing due to paints stacking up in the queue. Windows
implements this on the main queue, but since we manage this queue ourselves,
we have to do it here.

*******************************************************************************/

/* unpack paint message */

static void upackpm(WPARAM wparam, LPARAM lparam, int* x1, int* y1, int* x2,
                    int* y2)

{

    *x1 = wparam/0x10000;
    *y1 = wparam%0x10000;
    *x2 = lparam/0x10000;
    *y2 = lparam%0x10000;

}

/* pack paint message */

static void packpm(WPARAM* wparam, LPARAM* lparam, int x1, int y1, int x2, int y2)

{

    *wparam = x1*0x10000+y1;
    *lparam = x2*0x10000+y2;

}

/* find message matching type and window in queue */

static int fndmsg(HWND hwnd, UINT msg)

{

    int mi; /* message index */
    int fm; /* message found */

    fm = 0; /* set no message found */
    mi = msgout; /* index next output message */
    while (mi != msginp) { /* search forward */

        /* Check message type and window handle matches */
        if (msgque[mi].message == msg && msgque[mi].hwnd == hwnd)  {

            fm = mi; /* set found message */
            mi = msginp; /* terminate search */

        } else mi = next(mi); /* next entry */

    }

    return (fm); /* return found status */

}

/* enter new message to queue */

static void enter(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)

{

    BOOL b;

    /* if the queue is full, dump the oldest entry */
    if (next(msginp) == msgout) msgout = next(msgout);
    msgque[msginp].hwnd = hwnd; /* place windows handle */
    msgque[msginp].message = msg; /* place message code */
    msgque[msginp].wParam = wparam; /* place parameters */
    msgque[msginp].lParam = lparam;
    msginp = next(msginp); /* advance input pointer */
    b = SetEvent(msgrdy); /* flag message ready */
    if (!b) winerr(); /* fails */

}

static void putmsg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)

{

    int b;
    int nx1, ny1, nx2, ny2;
    int ox1, oy1, ox2, oy2;
    int fm;                 /* found message */

//dbg_printf(dlinfo, "Message: "); prtmsgu(hwnd, msg, wparam, lparam);

    lockmain(); /* start exclusive access */
/* Turning on paint compression causes lost updates */
    if (msg == WM_PAINT && PACKMSG)  {

        fm = fndmsg(hwnd, msg); /* find matching message */
        if (fm)  {

            /* There is a matching paint message in the queue, fold this message
               into the existing one by finding smallest encompassing
               rectangle. */
            upackpm(wparam, lparam, &nx1, &ny1, &nx2, &ny2); /* unpack new */
            /* unpack old */
            upackpm(msgque[fm].wParam, msgque[fm].lParam, &ox1, &oy1, &ox2, &oy2);
            /* find greater bounding */
            if (nx1 < ox1) ox1 = nx1;
            if (ny1 < oy1) oy1 = ny1;
            if (nx2 > ox2) ox2 = nx2;
            if (ny2 > oy2) oy2 = ny2;
            packpm(&msgque[fm].wParam, &msgque[fm].lParam, ox1, oy1, ox2, oy2);

        } else enter(hwnd, msg, wparam, lparam); /* enter as new message */

    } else if (msg == WM_SIZE && PACKMSG) {

        fm = fndmsg(hwnd, msg); /* find matching message */
        if (fm) {

            /* We only need the latest size, so overwrite the old with new. */
            msgque[fm].hwnd = hwnd; /* place windows handle */
            msgque[fm].wParam = wparam; /* place parameters */
            msgque[fm].lParam = lparam;

        } else enter(hwnd, msg, wparam, lparam); /* enter as new message */

    } else enter(hwnd, msg, wparam, lparam); /* enter new message */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get next message from queue

Retrives the next message from the queue. Does nothing if the queue is empty.
Queue empty should be checked before calling this routine, which is indicated
by msginp == msgout.

*******************************************************************************/

static void getmsg(MSG* msg)

{

    int   f; /* found message flag */
    DWORD r; /* result */

    f = FALSE; /* set no message found */
    /* It should not happen, but if we get a FALSE signal, loop waiting for
       signal, and don"t leave until we get a TRUE message. */
    do { /* wait for message */

        if (msginp == msgout && imsginp == imsgout)  {

            /* nothing in queue */
            unlockmain(); /* end exclusive access */
            r = WaitForSingleObject(msgrdy, -1); /* wait for next event */
            if (r == -1) winerr(); /* process windows error */
            b = ResetEvent(msgrdy); /* flag message not ready */
            lockmain(); /* start exclusive access */

        }
        /* get messages from the standard queue */
        if (msginp != msgout) { /* queue not empty */

            memcpy(msg, &msgque[msgout], sizeof(MSG)); /* get next message */
            msgout = next(msgout); /* advance output pointer */
            f = TRUE; /* found a message */

        }

    } while (!f); /* until we have a message */

//dbg_printf(dlinfo, "Message: "); prtmsg(msg);

}

/*******************************************************************************

Place message entry in control queue

Places a message into the control input queue. If the queue is full, overwrites
the oldest event.

*******************************************************************************/

static void iputmsg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)

{

    int b;

    lockmain(); /* start exclusive access */
    /* if the queue is full, dump the oldest entry */
    if (next(imsginp) == imsgout) imsgout = next(imsgout);
    imsgque[imsginp].hwnd = hwnd; /* place windows handle */
    imsgque[imsginp].message = msg; /* place message code */
    imsgque[imsginp].wParam = wparam; /* place parameters */
    imsgque[imsginp].lParam = lparam;
    imsginp = next(imsginp); /* advance input pointer */
    b = SetEvent(imsgrdy); /* flag message ready */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get next message from control queue

Retrives the next message from the control queue. Waits if the queue is
empty. Queue empty should be checked before calling this routine, which is
indicated by imsginp == imsgout.

*******************************************************************************/

static void igetmsg(MSG* msg)

{

    int   f; /* found message flag */
    DWORD r; /* result */

    f = FALSE; /* set no message found */
    /* It should not happen, but if we get a FALSE signal, loop waiting for
       signal, and don"t leave until we get a TRUE message. */
    do { /* wait for message */

        if (imsginp == imsgout)  {

            /* nothing in queue */
            unlockmain(); /* end exclusive access */
            r = WaitForSingleObject(imsgrdy, -1); /* wait for next event */
            if (r == -1) winerr(); /* process windows error */
            b = ResetEvent(imsgrdy); /* flag message not ready */
            lockmain(); /* start exclusive access */

        };
        /* retrive messages from the control queue first */
        if (imsginp != imsgout)  { /* queue not empty */

            memcpy(msg, &imsgque[imsgout], sizeof(MSG)); /* get next message */
            imsgout = next(imsgout); /* advance output pointer */
            f = TRUE; /* found a message */

        }

    } while (!f); /* until we have a message */

}

/*******************************************************************************

Get intratask message entry

Gets a new intratask message, or recycles an old one.

*******************************************************************************/

static void getitm(imptr* p)

{

    if (freitm) { /* free entry available, get that */

        *p = freitm; /* index that */
        freitm = freitm->next; /* gap out of free list */

    } else *p = imalloc(sizeof(struct imrec)); /* else get a new one */
    (*p)->next = NULL; /* clear next */

}

/*******************************************************************************

Put intratask message entry

Places the given intratask message into the free list.

*******************************************************************************/

static void putitm(imptr p)

{

    p->next = freitm; /* push onto list */
    freitm = p;

}

/*******************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

static void getfet(filptr* fp)

{

    *fp = imalloc(sizeof(filrec)); /* get new file entry */
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = -1; /* set no input file linked */
    (*fp)->evt = NULL; /* set no queued events */

}

/*******************************************************************************

Make file entry

Indexes a present file entry or creates a new one. Looks for a free entry
in the files table, indicated by 0. If found, that is returned, otherwise
the file table is full.

Note that the "predefined" file slots are never allocated.

*******************************************************************************/

static void makfil(int* fn) /* file handle */

{

    int fi; /* index for files table */
    int ff; /* found file entry */

    /* find idle file slot (file with closed file entry) */
    ff = 0; /* clear found file */
    for (fi = ERRFIL+1; fi < MAXFIL; fi++) { /* search all file entries */

        if (!opnfil[fi]) /* found an unallocated slot */
            ff = fi; /* set found */
        else /* check if slot is allocated, but unoccupied */
            if (!opnfil[fi]->win) ff = fi; /* set found */

    }
    if (!ff) error(einvhan); /* no empty file positions found */
    if (!opnfil[ff]) getfet(&opnfil[ff]); /* get and initalize the file entry */
    *fn = ff; /* set file id number */

}

/********************************************************************************

Trim leading and trailing spaces off string.

Removes leading and trailing spaces from a given string. Since the string will
shrink by definition, no length is needed.

********************************************************************************/

static void trim(char *s)
{

    char* p;
    char* d;

    /* trim front */
    p = s;
    while (*p == ' ') p++;
    d = s;
    if (p != s) while (*d++ = *p++);

    /* trim back */
    p = s;
    while (*p && *p == ' ') p++; /* find end of string */
    if (p > s) {

        p--;
        while (p > s && *p == ' ') p--; /* back up to first non-space */
        p++; /* reindex */
        *p = 0; /* terminate */

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

    if (wid < 0 || wid >= MAXFIL)  error(einvhan); /* error */
    ofn = xltwin[wid-1]; /* get the output file handle */
    win = lfn2win(ofn); /* index window context */

    return (win); /* return result */

}

/*******************************************************************************

Find logical output file from Windows handle

Finds the logical output file that corresponds to the given window handle.
This is done the stupid way now for testing, by searching, but later can be
converted to hash methods.

*******************************************************************************/

static int hwn2lfn(HWND hw)

{

    int fi;  /* index for file handles */
    int fn; /* resulting file number */

    fn = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++) /* search output files */
        /* has an entry, has window assigned, and matches our entry */
        if (opnfil[fi] && opnfil[fi]->win && opnfil[fi]->win->winhan == hw)
            fn = fi; /* found */

    return (fn); /* return result */

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

Get event queuing entry

Either gets a used event queuing entry off the free list, or allocates a new
one.

*******************************************************************************/

static void geteqe(eqeptr* ep)

{

   if (eqefre) { /* get used entry */

      *ep = eqefre; /* index top of list */
      eqefre = eqefre->next; /* gap out of list */

   } else *ep = imalloc(sizeof(eqerec));
   (*ep)->last = NULL; /* clear pointers */
   (*ep)->next = NULL;

}

/*******************************************************************************

Put event queuing entry

Places the given event queuing entry onto the free list.

*******************************************************************************/

static void puteqe(eqeptr ep)

{

   ep->next = eqefre; /* push onto free list */
   eqefre = ep;

}

/*******************************************************************************

Get widget

Get a widget and place into the window tracking list. If a free widget entry
is available, that will be used, otherwise a new entry is allocated.

*******************************************************************************/

static void getwig(winptr win, wigptr* wp)

{

    if (wigfre) { /* used entry exists, get that */

        *wp = wigfre; /* index top entry */
        wigfre = wigfre->next; /* gap out */

   } else *wp = imalloc(sizeof(wigrec)); /* get entry */
   (*wp)->next = win->wiglst; /* push onto list */
   win->wiglst = *wp;
   (*wp)->han = 0; /* clear handles */
   (*wp)->han2 = 0;
   (*wp)->id = 0; /* clear id */
   (*wp)->typ = wtbutton; /* arbitrary set for type */
   (*wp)->siz = 0; /* size */
   (*wp)->enb = TRUE; /* set enabled by default */

}

/*******************************************************************************

Put widget

Removes the widget from the window list, and releases the widget entry to free
list.

*******************************************************************************/

static void putwig(winptr win, wigptr wp)

{

    wigptr lp;

    /* find last entry */
    if (win->wiglst == wp)  win->wiglst = win->wiglst->next; /* gap top of list */
    else { /* mid or last in list */

        lp = win->wiglst; /* index list top */
        /* find last entry */
        while (lp->next != wp && lp->next) lp = lp->next;
        if (!lp->next) error(esystem); /* should have found it */
        lp->next = wp->next; /* gap out entry */

    }
    wp->next = wigfre; /* push onto list */
    wigfre = wp;

}

/*******************************************************************************

Find widget by number

Finds the given widget by number.

*******************************************************************************/

static wigptr fndwig(winptr win, int id)

{

    wigptr wp, fp;  /* widget pointer */

    wp = win->wiglst; /* index top of list */
    fp = NULL; /* clear found */
    while (wp) { /* traverse list */

        if (wp->id == id) fp = wp; /* found */
        wp = wp->next; /* next in list */

    }

    return (fp); /* return result */

}

/*******************************************************************************

Find widget by window handle

Finds the given widget by window handle.

*******************************************************************************/

static wigptr fndwighan(winptr win, HWND han)

{

    wigptr wp, fp;  /* widget pointer */

    wp = win->wiglst; /* index top of list */
    fp = NULL; /* clear found */
    while (wp) { /* traverse list */

        if (wp->han == han || wp->han2 == han) fp = wp; /* found */
        wp = wp->next; /* next in list */

    }

    return (fp); /* return result */

}

/*******************************************************************************

Translate colors code

Translates an independent to a terminal specific primary color code for Windows.

*******************************************************************************/

static int colnum(ami_color c)

{

    int n;

    /* translate color number */
    switch (c) { /* color */

      case ami_black:     n = 0x000000; break;
      case ami_white:     n = 0xffffff; break;
      case ami_red:       n = 0x0000ff; break;
      case ami_green:     n = 0x00ff00; break;
      case ami_blue:      n = 0xff0000; break;
      case ami_cyan:      n = 0xffff00; break;
      case ami_yellow:    n = 0x00ffff; break;
      case ami_magenta:   n = 0xff00ff; break;
      case ami_backcolor: n = 0xd8e9ea; break;

   }

   return (n); /* return number */

}

/*******************************************************************************

Translate color to rgb

Translates colors to rgb, in ratioed INT_MAX form.

*******************************************************************************/

static void colrgb(ami_color c, int* r, int* g, int* b)

{

    /* translate color number */
    switch (c) { /* color */

        case ami_black:     *r = 0; *g = 0; *b = 0; break;
        case ami_white:     *r = INT_MAX; *g = INT_MAX; *b = INT_MAX; break;
        case ami_red:       *r = INT_MAX; *g = 0; *b = 0; break;
        case ami_green:     *r = 0; *g = INT_MAX; *b = 0; break;
        case ami_blue:      *r = 0; *g = 0; *b = INT_MAX; break;
        case ami_cyan:      *r = 0; *g = INT_MAX; *b = INT_MAX; break;
        case ami_yellow:    *r = INT_MAX; *g = INT_MAX; *b = 0; break;
        case ami_magenta:   *r = INT_MAX; *g = 0; *b = INT_MAX; break;
        case ami_backcolor: *r = 0xea*0x800000; *g = 0xe9*0x800000;
                           *b = 0xd8*0x800000; break;

    }

}

/*******************************************************************************

Translate rgb to color

Translates rgb to color, in ratioed INT_MAX form. Works by choosing the nearest
colors.

*******************************************************************************/

static void rgbcol(int r, int g, int b, ami_color* c)

{

   if (r < INT_MAX/2 && g < INT_MAX/2 && b < INT_MAX/2) *c = ami_black;
   else if (r >= INT_MAX/2 && g < INT_MAX/2 && b < INT_MAX/2) *c = ami_red;
   else if (r < INT_MAX/2 && g >= INT_MAX/2 && b < INT_MAX/2) *c = ami_green;
   else if (r < INT_MAX/2 && g < INT_MAX/2 && b >= INT_MAX/2) *c = ami_blue;
   else if (r < INT_MAX/2 && g >= INT_MAX/2 && b >= INT_MAX/2) *c = ami_cyan;
   else if (r >= INT_MAX/2 && g >= INT_MAX/2 && b < INT_MAX/2) *c = ami_yellow;
   else if (r >= INT_MAX/2 && g < INT_MAX/2 && b >= INT_MAX/2) *c = ami_magenta;
   else if (r >= INT_MAX/2 && g >= INT_MAX/2 && b >= INT_MAX/2) *c = ami_white;
   else error(esystem); /* should have been one of those */

}

/*******************************************************************************

Translate rgb to windows color

Translates a ratioed INT_MAX graph color to the windows form, which is a 32
bit word with blue, green and red bytes.

*******************************************************************************/

static int rgb2win(int r, int g, int b)

{

   return ((b/8388608)*65536+(g/8388608)*256+(r/8388608));

}

/*******************************************************************************

Translate windows color to rgb color

Translates a windows int color to our ratioed INT_MAX rgb color.

*******************************************************************************/

static void win2rgb(int wc, int* r, int* g, int* b)

{

   *r = (wc&0xff)*0x800000; /* get red value */
   *g = (wc>>8&0xff)*0x800000; /* get greeen value */
   *b = (wc>>16&0xff)*0x800000; /* get blue value */

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

static void clrbuf(winptr win, scnptr sc)

{

    RECT   r;  /* rectangle */
    HBRUSH hb; /* handle to brush */
    int    b;  /* return value */

    r.left = 0; /* set all client area for rectangle clear */
    r.top = 0;
    r.right = win->gmaxxg;
    r.bottom = win->gmaxyg;
    hb = CreateSolidBrush(sc->bcrgb); /* get a brush for background */
    if (!hb) winerr(); /* process error */
    /* clear buffer surface */
    b = FillRect(sc->bdc, &r, hb);
    if (!b) winerr(); /* process error */
    b = DeleteObject(hb); /* free the brush */
    if (!b) winerr(); /* process error */

}

/*******************************************************************************

Clear window

Clears the entire window to spaces with the current colors and attributes.

*******************************************************************************/

static void clrwin(winptr win)

{

    RECT   r;  /* rectangle */
    HBRUSH hb; /* handle to brush */
    int    b;   /* return value */

    r.left = 0; /* set all client area for rectangle clear */
    r.top = 0;
    r.right = win->gmaxxg;
    r.bottom = win->gmaxyg;
    hb = CreateSolidBrush(win->gbcrgb); /* get a brush for background */
    if (!hb) winerr(); /* process error */
    /* clear buffer surface */
    b = FillRect(win->devcon, &r, hb);
    if (!b) winerr(); /* process error */
    b = DeleteObject(hb); /* free the brush */
    if (!b) winerr(); /* process error */

}

/*******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

static int icurbnd(scnptr sc)

{

   return (sc->curx >= 1 && sc->curx <= sc->maxx &&
           sc->cury >= 1 && sc->cury <= sc->maxy);

}

int ami_curbnd(FILE* f)

{

    winptr win; /* windows record pointer */
    int    cb;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    cb = icurbnd(win->screens[win->curupd-1]);
    unlockmain(); /* end exclusive access */

    return (cb);

}

/*******************************************************************************

Set cursor visable

Makes the cursor visible.

*******************************************************************************/

static void curon(winptr win)

{

    int b;

    if (!win->fcurdwn && win->screens[win->curdsp-1]->curv &&
        icurbnd(win->screens[win->curdsp-1]) && win->focus)  {

        /* cursor not already down, cursor visible, cursor in bounds, screen
           in focus */
        b = ShowCaret(win->winhan); /* show the caret */
        if (!b) winerr(); /* process error */
        win->fcurdwn = TRUE; /* set cursor on screen */

    }

}

/*******************************************************************************

Set cursor invisible

Makes the cursor invisible.

*******************************************************************************/

static void curoff(winptr win)

{

    int b;

    if (win->fcurdwn) { /* cursor visable */

        b = HideCaret(win->winhan); /* hide the caret */
        if (!b) winerr(); /* process error */
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
        icurbnd(win->screens[win->curdsp-1]) && win->focus) {

        /* cursor should be visible */
        if (!win->fcurdwn) { /* not already down */

            /* cursor not already down, cursor visible, cursor in bounds */
            b = ShowCaret(win->winhan); /* show the caret */
            if (!b) winerr(); /* process error */
            win->fcurdwn = TRUE; /* set cursor on screen */

        }

    } else {

         /* cursor should not be visible */
        if (win->fcurdwn) { /* cursor visable */

            b = HideCaret(win->winhan); /* hide the caret */
            if (!b) winerr(); /* process error */
            win->fcurdwn = FALSE; /* set cursor not on screen */

        }

    }

}

/*******************************************************************************

Position cursor

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that.

*******************************************************************************/

static void setcur(winptr win)

{

    int b;

    /* check cursor in bounds and visible, and window has focus */
    if (icurbnd(win->screens[win->curupd-1]) && win->focus) {

        /* set to bottom of character bounding box */
        b = SetCaretPos(win->screens[win->curdsp-1]->curxg-1,
                        win->screens[win->curdsp-1]->curyg-1+win->linespace-3);
        /* setcaret position is always returning an error, even when correct */
        /* if (!b) winerr(); */ /* process error */

    }
    cursts(win); /* process cursor status change */

}

/*******************************************************************************

Change cursor

Changes the cursor size. If the cursor is in display,  it is destroyed
and remade to the new cursor size. Otherwise, it is left alone, and will be
created on the next focus event.

*******************************************************************************/

static void chgcur(winptr win)

{

    int b; /* return value */

    if (win->focus) { /* change it */

        b = DestroyCaret(); /* remove text cursor */
        if (!b) winerr(); /* process error */
        b = CreateCaret(win->winhan, 0, win->curspace, 3); /* activate caret */
        if (!b) winerr(); /* process error */
        win->fcurdwn = FALSE; /* set cursor not down */
        setcur(win); /* replace it */

    }

}

/*******************************************************************************

Create font from current attributes

Creates a font using the attrbutes in the current update screen, and sets the
metrics for the font.

*******************************************************************************/

static void newfont(winptr win)

{

    int        w; /* weight temp */
    int        h; /* height temp */
    TEXTMETRIC tm; /* text metric structure */
    int        r; /* result holder */
    HGDIOBJ    sf; /* system fixed font object */
    int        b;
    int        attrc;
    scnptr     sc;
    HGDIOBJ    rv;


    sc = win->screens[win->curupd-1];
    if (sc->font) { /* there is a font */

       /* get the current font out of the DCs */
       sf = GetStockObject(SYSTEM_FIXED_FONT);
       if (!sf) winerr(); /* process windows error */
       rv = SelectObject(sc->bdc, sf);
       if (rv == HGDI_ERROR) winerr();
       if (indisp(win)) {

            rv = SelectObject(win->devcon, sf);
            if (rv == HGDI_ERROR) winerr();

       }
       /* this indicates an error when there is none */
       /* if ! DeleteObject(font)  winerr(); */ /* delete old font */
       b = DeleteObject(sc->font); /* delete old font */

    }
    if (sc->cfont->sys) { /* select as system font */

        /* select the system font */
        sf = GetStockObject(SYSTEM_FIXED_FONT);
        if (!sf) winerr(); /* process windows error */
        rv = SelectObject(sc->bdc, sf);
        if (rv == HGDI_ERROR) winerr();
        /* select to screen dc */
        if (indisp(win)) {

            rv = SelectObject(win->devcon, sf); /* process error */
            if (rv == HGDI_ERROR) winerr();

        }

    } else {

        attrc = win->screens[win->curupd-1]->attr; /* copy attribute */
        w = FW_REGULAR;
        if (BIT(saxlight) & attrc) w = FW_EXTRALIGHT;
        else if (BIT(salight) & attrc) w = FW_LIGHT;
        else if (BIT(sabold) & attrc) w = FW_BOLD;
        else if (BIT(saxbold) & attrc) w = FW_EXTRABOLD;
        /* set normal height or half height for subscript/superscript */
        if (BIT(sasuper) & attrc | BIT(sasubs) & attrc)
            h = trunc(win->gfhigh*0.75); else h = win->gfhigh;
        sc->font = CreateFont(h, 0, 0, 0, w, BIT(saital) & attrc,
                          BIT(saundl) & sc->attr, BIT(sastkout) & sc->attr, ANSI_CHARSET,
                          OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                          FQUALITY, DEFAULT_PITCH,
                          sc->cfont->fn);
        if (!win->screens[win->curupd-1]->font) winerr(); /* process windows error */
        /* select to buffer DC */
        rv = SelectObject(win->screens[win->curupd-1]->bdc, sc->font);
        if (rv == HGDI_ERROR) winerr();
        /* select to screen DC */
        if (indisp(win)) {

            rv = SelectObject(win->devcon, sc->font); /* process error */
            if (rv == HGDI_ERROR) winerr();

        }

    }
    b = GetTextMetrics(win->screens[win->curupd-1]->bdc, &tm); /* get the standard metrics */
    if (!b) winerr(); /* process windows error */
    /* Calculate line spacing */
    win->linespace = tm.tmHeight;
    win->screens[win->curupd-1]->lspc = win->linespace;
    /* calculate character spacing */
    win->charspace = tm.tmMaxCharWidth;
    /* set cursor width */
    win->curspace = tm.tmAveCharWidth;
    win->screens[win->curupd-1]->cspc = win->charspace;
    /* calculate baseline offset */
    win->baseoff = win->linespace-tm.tmDescent-1;
    /* change cursor to match new font/size */
    if (indisp(win)) chgcur(win);

}

/*******************************************************************************

Restore screen

Updates all the buffer and screen parameters from the display screen to the
terminal.

*******************************************************************************/

static void restore(winptr win,   /* window to restore */
                    int    whole) /* whole or part window */

{

    int b;        /* return value */
    int r;        /* return value */
    RECT cr, cr2; /* client rectangle */
    HGDIOBJ oh;   /* old pen */
    HBRUSH hb;    /* handle to brush */
    SIZE s;       /* size holder */
    int x, y;     /* x and y coordinates */
    scnptr sc;

    sc = win->screens[win->curdsp-1];
    if (win->bufmod && win->visible)  { /* buffered mode is on, and visible */

        curoff(win); /* hide the cursor for drawing */
        /* set colors and attributes */
        if (BIT(sarev) & sc->attr)  { /* reverse */

            r = SetBkColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetTextColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        } else {

            r = SetBkColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetTextColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */

        }
        /* select any viewport offset to display */
        b = SetViewportOrgEx(win->devcon, sc->offx, sc->offy, NULL);
        if (!b)  winerr(); /* process windows error */
        /* select the extents */
        b = SetWindowExtEx(win->devcon, sc->wextx, sc->wexty, &s);
        /* if (!b) winerr(); */ /* process windows error */
        b = SetViewportExtEx(win->devcon, sc->vextx, sc->vexty, &s);
        if (!b)  winerr(); /* process windows error */
        oh = SelectObject(win->devcon, sc->font); /* select font to display */
        if (oh == HGDI_ERROR) winerr();
        oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
        if (oh == HGDI_ERROR) winerr();
        if (whole) { /* get whole client area */

            b = GetClientRect(win->winhan, &cr);
            if (!b) winerr(); /* process windows error */

        } else
            /* get only update area. This only happens during a WM_PAINT
               operation. */
            b = GetUpdateRect(win->winhan, &cr, FALSE);
        /* validate it so windows won"t send multiple notifications */
        b = ValidateRgn(win->winhan, NULL); /* validate region */
        if (cr.left != 0 || cr.top != 0 || cr.right != 0 || cr.bottom != 0) {
            /* area is not NULL */

            /* convert to device coordinates */
            cr.left = cr.left+sc->offx;
            cr.top = cr.top+sc->offy;
            cr.right = cr.right+sc->offx;
            cr.bottom = cr.bottom+sc->offy;
            /* clip update rectangle to buffer */
            if (cr.left <= win->gmaxxg || cr.bottom <= win->gmaxyg)  {

                /* It's within the buffer. Now clip the right and bottom. */
                x = cr.right; /* copy right and bottom sides */
                y = cr.bottom;
                if (x > win->gmaxxg) x = win->gmaxxg;
                if (y > win->gmaxyg) y = win->gmaxyg;
                /* copy backing bitmap to screen */
                b = BitBlt(win->devcon, cr.left, cr.top, x-cr.left+1,
                           y-cr.top+1, sc->bdc, cr.left, cr.top, SRCCOPY);

            }
            /* Now fill the right and bottom sides of the client beyond the
               bitmap. */
           hb = CreateSolidBrush(sc->bcrgb); /* get a brush for background */
           if (hb == 0) winerr(); /* process windows error */
           /* check right side fill */
           memcpy(&cr2, &cr, sizeof(RECT)); /* copy update rectangle */
           /* subtract overlapping space */
           if (cr2.left <= win->gmaxxg) cr2.left = win->gmaxxg;
           if (cr2.left <= cr2.right) /* still has width */
                b = FillRect(win->devcon, &cr2, hb);
           /* check bottom side fill */
           memcpy(&cr2, &cr, sizeof(RECT)); /* copy update rectangle */
           /* subtract overlapping space */
           if (cr2.top <= win->gmaxyg) cr2.top = win->gmaxyg;
           if (cr2.top <= cr2.bottom) /* still has height */
                b = FillRect(win->devcon, &cr2, hb);
            b = DeleteObject(hb); /* free the brush */
            if (!b)  winerr(); /* process windows error */

        }
        setcur(win); /* show the cursor */

    }

}

/*******************************************************************************

Display window

Presents a window, and sends it a first paint message. Used to process the
delayed window display function.

*******************************************************************************/

static void winvis(winptr win)

{

    int    b;   /* int result holder */
    winptr par; /* parent window pointer */

    /* If we are making a child window visible, we have to also force its
       parent visible. This is recursive all the way up. */
    if (win->parlfn >= 0) {

        par = lfn2win(win->parlfn); /* get parent data */
        if (!par->visible) winvis(par); /* make visible if not */

    }
    unlockmain(); /* end exclusive access */
    /* present the window */
    b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
    /* send first paint message */
    b = UpdateWindow(win->winhan);
    lockmain(); /* start exclusive access */
    win->visible = TRUE; /* set now visible */
    restore(win, TRUE); /* restore window */

}

/*******************************************************************************

Initalize screen

Clears all the parameters in the present screen context. Also, the backing
buffer bitmap is created and cleared to the present colors.

*******************************************************************************/

static void iniscn(winptr win, scnptr sc)

{

    int      i, x;
    HBITMAP  hb;
    LOGBRUSH lb;
    int      r;
    HGDIOBJ  rv;

    sc->maxx = win->gmaxx; /* set character dimensions */
    sc->maxy = win->gmaxy;
    sc->maxxg = win->gmaxxg; /* set pixel dimensions */
    sc->maxyg = win->gmaxyg;
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    sc->fcrgb = win->gfcrgb; /* set colors and attributes */
    sc->bcrgb = win->gbcrgb;
    sc->attr = win->gattr;
    sc->autof = win->gauto; /* set auto scroll and wrap */
    sc->curv = win->gcurv; /* set cursor visibility */
    sc->lwidth = 1; /* set single pixel width */
    sc->font = 0; /* set no font active */
    sc->cfont = win->gcfont; /* set current font */
    sc->fmod = win->gfmod; /* set mix modes */
    sc->bmod = win->gbmod;
    sc->offx = win->goffx; /* set viewport offset */
    sc->offy = win->goffy;
    sc->wextx = win->gwextx; /* set extents */
    sc->wexty = win->gwexty;
    sc->vextx = win->gvextx;
    sc->vexty = win->gvexty;
    /* create a matching device context */
    sc->bdc = CreateCompatibleDC(win->devcon);
    if (!sc->bdc) winerr(); /* process windows error */
    /* create a bitmap for that */
    hb = CreateCompatibleBitmap(win->devcon, win->gmaxxg, win->gmaxyg);
    if (!hb) winerr(); /* process windows error */
    sc->bhn = SelectObject(sc->bdc, hb); /* select bitmap into dc */
    if (sc->bhn == HGDI_ERROR) winerr(); /* process windows error */
    win->bufx = win->gmaxx; /* save as buffer size */
    win->bufy = win->gmaxy;
    win->bufxg = win->gmaxxg;
    win->bufyg = win->gmaxyg;
    newfont(win); /* create font for buffer */
    /* set non-braindamaged stretch mode */
    r = SetStretchBltMode(win->screens[win->curupd-1]->bdc, HALFTONE);
    if (!r) winerr(); /* process windows error */
    /* set pen to foreground */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process windows error */
    rv = SelectObject(sc->bdc, sc->fpen);
    if (rv == HGDI_ERROR) error(enosel);
    /* set brush to foreground */
    sc->fbrush = CreateSolidBrush(sc->fcrgb);
    if (!sc->fbrush) winerr(); /* process windows error */
    /* remove fills */
    rv = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
    if (rv == HGDI_ERROR) error(enosel);
    /* set single pixel pen to foreground */
    sc->fspen = CreatePen(FSPENSTL, 1, sc->fcrgb);
    if (!sc->fspen) winerr(); /* process windows error */
    /* set colors and attributes */
    if (BIT(sarev) & sc->attr) { /* reverse */

        r = SetBkColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetTextColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */

    } else {

        r = SetBkColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetTextColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */

    }
    clrbuf(win, sc); /* clear screen buffer with that */
    for (i = 0; i < MAXTAB; i++) sc->tab[i] = 0; /* clear tab array */
    /* set up tabbing to be on each 8th position */
    i = 9; /* set 1st tab position */
    x = 0; /* set 1st tab slot */
    while (i < sc->maxx && x < MAXTAB) {

        sc->tab[x] = (i-1)*win->charspace+1;  /* set tab */
        i = i+8; /* next tab */
        x = x+1;

    }

}

/*******************************************************************************

Disinitalize screen

Removes all data for the given screen buffer.

Note: The major objects are cleaned, but the minor ones need to be as well.

*******************************************************************************/

static void disscn(winptr win, scnptr sc)

{

    /* need to do disposals here */

}

/*******************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void iclear(winptr win)

{

    if (win->bufmod) clrbuf(win, win->screens[win->curupd-1]); /* clear the screen buffer */
    win->screens[win->curupd-1]->curx = 1; /* set cursor at home */
    win->screens[win->curupd-1]->cury = 1;
    win->screens[win->curupd-1]->curxg = 1;
    win->screens[win->curupd-1]->curyg = 1;
    if (indisp(win)) { /* also process to display */

        if (win->bufmod) {

            /* in buffered mode, we just copy the now cleared buffer to the
               main screen */
            setcur(win); /* set cursor on screen */
            restore(win, TRUE); /* copy to real screen */

        } else clrwin(win); /* clear window directly */

    }

}

/*******************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer,  restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

static void iscrollg(winptr win, int x, int y)

{

    int dx, dy, dw, dh; /* destination coordinates and sizes */
    int sx, sy;         /* destination coordinates */
    int b;              /* result */
    HBRUSH hb;          /* handle to brush */
    RECT frx, fry;      /* fill rectangles */

    /* scroll would result in complete clear, do it */
    if (x <= -win->gmaxxg || x >= win->gmaxxg ||
        y <= -win->gmaxyg || y >= win->gmaxyg)
        iclear(win); /* clear the screen buffer */
    else { /* scroll */

       /* set y movement */
       if (y >= 0)  { /* move up */

          sy = y; /* from y lines down */
          dy = 0; /* move to top of screen */
          dh = win->gmaxyg-y; /* height minus lines to move */
          fry.left = 0; /* set fill to y lines at bottom */
          fry.right = win->gmaxxg;
          fry.top = win->gmaxyg-y;
          fry.bottom = win->gmaxyg;

       } else { /* move down */

          sy = 0; /* from top */
          dy = abs(y); /* move to y lines down */
          dh = win->gmaxyg-abs(y); /* height minus lines to move */
          fry.left = 0; /* set fill to y lines at top */
          fry.right = win->gmaxxg;
          fry.top = 0;
          fry.bottom = abs(y);

       }
       /* set x movement */
       if (x >= 0) { /* move text left */

          sx = x; /* from x characters to the right */
          dx = 0; /* move to left side */
          dw = win->gmaxxg-x; /* width - x characters */
          /* set fill x character collums at right */
          frx.left = win->gmaxxg-x;
          frx.right = win->gmaxxg;
          frx.top = 0;
          frx.bottom = win->gmaxyg;

       } else { /* move text right */

          sx = 0; /* from x left */
          dx = abs(x); /* move from left side */
          dw = win->gmaxxg-abs(x); /* width - x characters */
          /* set fill x character collums at left */
          frx.left = 0;
          frx.right = abs(x);
          frx.top = 0;
          frx.bottom = win->gmaxyg;

       }
       if (win->bufmod) { /* apply to buffer */

          b = BitBlt(win->screens[win->curupd-1]->bdc, dx, dy, dw, dh,
                     win->screens[win->curupd-1]->bdc, sx, sy, SRCCOPY);
          if (!b) winerr(); /* process windows error */
          /* get a brush for background */
          hb = CreateSolidBrush(win->screens[win->curupd-1]->bcrgb);
          if (!hb) winerr(); /* process windows error */
          /* fill vacated x */
          if (x) if (!FillRect(win->screens[win->curupd-1]->bdc, &frx, hb)) winerr();
          /* fill vacated y */
          if (y) if (!FillRect(win->screens[win->curupd-1]->bdc, &fry, hb)) winerr();
          b = DeleteObject(hb); /* free the brush */
          if (!b) winerr; /* process windows error */

       } else { /* scroll on screen */

          b = BitBlt(win->devcon, dx, dy, dw, dh, win->devcon, sx, sy, SRCCOPY);
          if (!b) winerr(); /* process windows error */
          /* get a brush for background */
          hb = CreateSolidBrush(win->gbcrgb);
          if (!hb) winerr(); /* process windows error */
          /* fill vacated x */
          if (x) if (!FillRect(win->devcon, &frx, hb)) winerr();
          /* fill vacated y */
          if (y) if (!FillRect(win->devcon, &fry, hb)) winerr();
          b = DeleteObject(hb); /* free the brush */
          if (!b) winerr; /* process windows error */

       }

    }
    if (indisp(win) && win->bufmod)
        restore(win, TRUE); /* move buffer to screen */

}

void ami_scrollg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscrollg(win, x, y); /* process scroll */
    unlockmain(); /* end exclusive access */

}

void ami_scroll(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscrollg(win, x*win->charspace, y*win->linespace); /* process scroll */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(winptr win, int x, int y)

{

    if (x != win->screens[win->curupd-1]->curx ||
        y != win->screens[win->curupd-1]->cury) {

        win->screens[win->curupd-1]->cury = y; /* set new position */
        win->screens[win->curupd-1]->curx = x;
        win->screens[win->curupd-1]->curxg = (x-1)*win->charspace+1;
        win->screens[win->curupd-1]->curyg = (y-1)*win->linespace+1;
        if (!icurbnd(win->screens[win->curupd-1]) &&
            win->screens[win->curupd-1]->autof)
            error(eatocur); /* bad cursor position with auto */
        if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

void ami_cursor(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    icursor(win, x, y); /* position cursor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

*******************************************************************************/

static void icursorg(winptr win, int x, int y)

{

    if (win->screens[win->curupd-1]->autof)
        error(eatopos); /* cannot perform with auto on */
    if (x != win->screens[win->curupd-1]->curxg ||
        y != win->screens[win->curupd-1]->curyg)  {

         win->screens[win->curupd-1]->curyg = y; /* set new position */
         win->screens[win->curupd-1]->curxg = x;
         win->screens[win->curupd-1]->curx = x / win->charspace+1;
         win->screens[win->curupd-1]->cury = y / win->linespace+1;
         if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

void ami_cursorg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    icursorg(win, x, y); /* position cursor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find character baseline

Returns the offset, from the top of the current fonts character bounding box,
to the font baseline. The baseline is the line all characters rest on.

*******************************************************************************/

int ami_baseline(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->baseoff; /* return current line spacing */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int ami_maxx(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxx; /* set maximum x */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int ami_maxy(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxy; /* set maximum y */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum x dimension graphical

Returns the maximum x dimension, which is the width of the client surface in
pixels.

*******************************************************************************/

int ami_maxxg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxxg; /* set maximum x */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum y dimension graphical

Returns the maximum y dimension, which is the height of the client surface in
pixels.

*******************************************************************************/

int ami_maxyg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxyg; /* set maximum y */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

static void ihome(winptr win)

{

    win->screens[win->curdsp-1]->curx = 1; /* set cursor at home */
    win->screens[win->curdsp-1]->cury = 1;
    win->screens[win->curdsp-1]->curxg = 1;
    win->screens[win->curdsp-1]->curyg = 1;
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void ami_home(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ihome(win);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

static void iup(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    /* check not top of screen */
    if (sc->cury > 1) {

        /* update position */
        sc->cury = sc->cury-1;
        sc->curyg = sc->curyg-win->linespace;

    /* check auto mode */
    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, -1*win->linespace); /* scroll up */
    /* check won't overflow */
    else if (sc->cury > -INT_MAX) {

        /* set new position */
        sc->cury = sc->cury-1;
        sc->curyg = sc->curyg-win->linespace;

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void ami_up(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iup(win); /* move up */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

static void idown(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    /* check not bottom of screen */
    if (sc->cury < sc->maxy) {

       sc->cury = sc->cury+1; /* update position */
       sc->curyg = sc->curyg+win->linespace;

    /* check auto mode */
    } else if (sc->autof)
       iscrollg(win, 0*win->charspace, +1*win->linespace); /* scroll down */
    else if (sc->cury < INT_MAX) {

       sc->cury = sc->cury+1; /* set new position */
       sc->curyg = sc->curyg+win->linespace;

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void ami_down(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    idown(win); /* move cursor down */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by INT_MAX.

*******************************************************************************/

static void ileft(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];

    /* check not extreme left */
    if (sc->curx > 1) {

       sc->curx = sc->curx-1; /* update position */
       sc->curxg = sc->curxg-win->charspace;

    } else { /* wrap cursor motion */

       if (sc->autof) { /* autowrap is on */

          iup(win); /* move cursor up one line */
          sc->curx = sc->maxx; /* set cursor to extreme right */
          sc->curxg = sc->maxxg-win->charspace;

       } else
          /* check won"t overflow */
          if (sc->curx > -INT_MAX) {

          sc->curx = sc->curx-1; /* update position */
          sc->curxg = sc->curxg-win->charspace;

       }

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void ami_left(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ileft(win); /* move cursor left */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void iright(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];

    /* check not at extreme right */
    if (sc->curx < sc->maxx) {

       sc->curx = sc->curx+1; /* update position */
       sc->curxg = sc->curxg+win->charspace;

    } else { /* wrap cursor motion */

       if (sc->autof)  { /* autowrap is on */

          idown(win); /* move cursor up one line */
          sc->curx = 1; /* set cursor to extreme left */
          sc->curxg = 1;

       /* check won"t overflow */
       } else if (sc->curx < INT_MAX)  {

          sc->curx = sc->curx+1; /* update position */
          sc->curxg = sc->curxg+win->charspace;

       }

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void ami_right(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iright(win); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

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

    /* first, find if next tab even exists */
    x = sc->curxg+1; /* get just after the current x position */
    if (x < 1)  x = 1; /* don"t bother to search to left of screen */
    /* find tab or end of screen */
    i = 0; /* set 1st tab position */
    while (x > sc->tab[i] && sc->tab[i] && i < MAXTAB && x < sc->maxxg) i++;
    if (sc->tab[i] && x < sc->tab[i]) { /* not off right of tabs */

       sc->curxg = sc->tab[i]; /* set position to that tab */
       sc->curx = sc->curxg/win->charspace+1;
       if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

/*******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

*******************************************************************************/

void ami_blink(FILE* f, int e)

{

   /* no capability */

}

/*******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

static void ireverse(winptr win, int e)

{

    int r; /* return value */
    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* reverse on */

        sc->attr |= BIT(sarev); /* set attribute active */
        win->gattr |= BIT(sarev);
        /* activate in buffer */
        r = SetTextColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetBkColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */
        if (indisp(win)) { /* activate on screen */

            /* reverse the colors */
            r = SetTextColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetBkColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */

        }

    } else { /* turn it off */

        sc->attr &= ~BIT(sarev); /* set attribute inactive */
        win->gattr &= ~BIT(sarev);
        /* activate in buffer */
        r = SetTextColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetBkColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */
        if (indisp(win)) { /* activate on screen */

            /* set normal colors */
            r = SetTextColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetBkColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        }

    }

}

void ami_reverse(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ireverse(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
This is not implemented, but could be done by drawing a line under each
character drawn.

*******************************************************************************/

static void iunderline(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* underline on */

        sc->attr |= BIT(saundl); /* set attribute active */
        win->gattr |= BIT(saundl);

    } else { /* turn it off */

        sc->attr &= ~BIT(saundl); /* set attribute inactive */
        win->gattr &= ~BIT(saundl);

    }
    newfont(win); /* select new font */

}

void ami_underline(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iunderline(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

static void isuperscript(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e && !sc->autof)  { /* superscript on */

        sc->attr |= BIT(sasuper); /* set attribute active */
        win->gattr |= BIT(sasuper);

    } else { /* turn it off */

        sc->attr &= ~BIT(sasuper); /* set attribute inactive */
        win->gattr &= ~BIT(sasuper);

    }
    newfont(win); /* select new font */

}

void ami_superscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isuperscript(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

static void isubscript(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e && !sc->autof) { /* subscript on */

        sc->attr |= BIT(sasubs); /* set attribute active */
        win->gattr |= BIT(sasubs);

    } else { /* turn it off */

        sc->attr &= ~BIT(sasubs); /* set attribute inactive */
        win->gattr &= ~BIT(sasubs);

    }
    newfont(win); /* activate new font with that */

}

void ami_subscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isubscript(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

Italic is causing problems with fixed mode on some fonts, and Windows does not
seem to want to share with me just what the TRUE width of an italic font is
(without taking heroic measures like drawing and testing pixels). So we disable
italic on fixed fonts.

*******************************************************************************/

static void iitalic(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* italic on */

        sc->attr |= BIT(saital); /* set attribute active */
        win->gattr |= BIT(saital);

    } else { /* turn it off */

        sc->attr &= ~BIT(saital); /* set attribute inactive */
        win->gattr &= ~BIT(saital);

    }
    newfont(win); /* activate new font with that */

}

void ami_italic(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iitalic(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.

Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off,  reset everything, including the
colors, which an ATTRIBUTE command seems to mess with not.

*******************************************************************************/

static void ibold(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(sabold); /* set attribute active */
        win->gattr |= BIT(sabold);

    } else { /* turn it off */

        sc->attr &= ~BIT(sabold); /* set attribute inactive */
        win->gattr &= ~BIT(sabold);

    }
    newfont(win); /* activate new font with that */

}

void ami_bold(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ibold(win, e); /* set bold */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

static void istrikeout(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sastkout); /* set attribute active */
       win->gattr |= BIT(sastkout);

    } else { /* turn it off */

       sc->attr &= ~BIT(sastkout); /* set attribute inactive */
       win->gattr &= ~BIT(sastkout);

    }
    newfont(win); /* activate new font with that */

}

void ami_strikeout(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    istrikeout(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_standout(FILE* f, int e)

{

   ami_reverse(f, e); /* implement as reverse */

}

/*******************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

static void ifcolor(winptr win, ami_color c)

{

    int      r;  /* return value */
    HGDIOBJ  oh; /* old pen */
    int      b;  /* return value */
    LOGBRUSH lb;
    scnptr   sc;

    sc = win->screens[win->curupd-1];
    sc->fcrgb = colnum(c); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

       r = SetBkColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    } else {

       r = SetTextColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    }
    /* also activate general graphics color. note that reverse does not apply
      to graphical coloring */
    b = DeleteObject(sc->fpen); /* remove old pen */
    if (!b) winerr(); /* process windows error */
    b = DeleteObject(sc->fbrush); /* remove old brush */
    if (!b) winerr(); /* process windows error */
    b = DeleteObject(sc->fspen); /* remove old single pixel pen */
    if (!b) winerr(); /* process windows error */
    /* create new pen */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process windows error */
    /* create new brush */
    sc->fbrush = CreateSolidBrush(sc->fcrgb);
    if (!sc->fbrush) winerr(); /* process windows error */
    /* create new single pixel pen */
    sc->fspen = CreatePen(FSPENSTL, 1, sc->fcrgb);
    if (!sc->fspen) winerr(); /* process windows error */
    /* select to buffer dc */
    oh = SelectObject(sc->bdc, sc->fpen);
    if (oh == HGDI_ERROR) winerr();
    if (indisp(win)) { /* activate on screen */

       /* set screen color according to reverse */
       if (BIT(sarev) & sc->attr) {

          r = SetBkColor(win->devcon, sc->fcrgb);
          if (r == -1) winerr(); /* process windows error */

       } else {

          r = SetTextColor(win->devcon, sc->fcrgb);
          if (r == -1) winerr(); /* process windows error */

       }
       oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
       if (oh == HGDI_ERROR) winerr();

    }

}

void ami_fcolor(FILE* f, ami_color c)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ifcolor(win, c); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground color graphical

Sets the foreground color from RGB primaries. The RGB values are scaled from
INT_MAX, so 255 == INT_MAX. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

static void ifcolorg(winptr win, int r, int g, int b)

{

    int      rv; /* return value */
    HGDIOBJ  oh; /* old pen */
    int      bv; /* return value */
    LOGBRUSH lb;
    scnptr   sc;

    sc = win->screens[win->curupd-1];
    sc->fcrgb = rgb2win(r, g, b); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

       r = SetBkColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    } else {

       rv = SetTextColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    }
    /* also activate general graphics color. note that reverse does not apply
      to graphical coloring */
    bv = DeleteObject(sc->fpen); /* remove old pen */
    if (!bv) winerr(); /* process error */
    bv = DeleteObject(sc->fbrush); /* remove old brush */
    if (!bv) winerr(); /* process error */
    bv = DeleteObject(sc->fspen); /* remove old single pixel pen */
    if (!bv) winerr(); /* process error */
    /* create new pen */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process error */
    /* create new brush */
    sc->fbrush = CreateSolidBrush(sc->fcrgb);
    if (!sc->fbrush) winerr(); /* process error */
    /* create new single pixel pen */
    sc->fspen = CreatePen(FSPENSTL, 1, sc->fcrgb);
    if (!sc->fspen) winerr(); /* process error */
    /* select to buffer dc */
    oh = SelectObject(sc->bdc, sc->fpen);
    if (oh == HGDI_ERROR) winerr();
    if (indisp(win))  { /* activate on screen */

       /* set screen color according to reverse */
       if (BIT(sarev) & sc->attr) {

          rv = SetBkColor(win->devcon, sc->fcrgb);
          if (rv == -1) winerr(); /* process windows error */

       } else {

          rv = SetTextColor(win->devcon, sc->fcrgb);
          if (rv == -1) winerr(); /* process windows error */

       };
       oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
       if (oh == HGDI_ERROR) winerr();

    }

}

void ami_fcolorg(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ifcolorg(win, r, g, b); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

static void ibcolor(winptr win, ami_color c)

{

    int r;
    scnptr   sc;

    sc = win->screens[win->curupd-1];
    sc->bcrgb = colnum(c); /* set color status */
    win->gbcrgb = sc->bcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

        r = SetTextColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */

    } else {

        r = SetBkColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* activate on screen */

        /* set screen color according to reverse */
        if (BIT(sarev) & sc->attr) {

            r = SetTextColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        } else {

            r = SetBkColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        }

    }

}

void ami_bcolor(FILE* f, ami_color c)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ibcolor(win, c); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
INT_MAX, so 255 == INT_MAX. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

static void ibcolorg(winptr win, int r, int g, int b)

{

    int    rv;
    scnptr sc;

    sc = win->screens[win->curupd-1];
    sc->bcrgb = rgb2win(r, g, b); /* set color status */
    win->gbcrgb = sc->bcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

        rv = SetTextColor(sc->bdc, sc->bcrgb);
        if (rv == -1) winerr(); /* process windows error */

    } else {

        rv = SetBkColor(sc->bdc, sc->bcrgb);
        if (rv == -1) winerr(); /* process windows error */

    }
    if (indisp(win))  { /* activate on screen */

        /* set screen color according to reverse */
        if (BIT(sarev) & sc->attr)  {

            rv = SetTextColor(win->devcon, sc->bcrgb);
            if (rv == -1) winerr(); /* process windows error */

        } else {

            rv = SetBkColor(win->devcon, sc->bcrgb);
            if (rv == -1) winerr(); /* process windows error */

        }

    }

}

void ami_bcolorg(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ibcolorg(win, r, g, b); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

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

        /* check display is on grid and in bounds */
        if ((sc->curxg-1)%win->charspace || (sc->curyg-1)%win->linespace)
            error(eatoofg);
        if (!icurbnd(sc)) error(eatoecb);

    }
    sc->autof = e; /* set auto status */
    win->gauto = e;

}

void ami_auto(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iauto(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void icurvis(winptr win, int e)

{

    win->screens[win->curupd-1]->curv = e; /* set cursor visible status */
    win->gcurv = e;
    cursts(win); /* process any cursor status change */

}

void ami_curvis(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    icurvis(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int ami_curx(FILE* f)

{

    winptr win; /* windows record pointer */
    int    x;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    x = win->screens[win->curupd-1]->curx; /* return current location x */
    unlockmain(); /* end exclusive access */

    return (x);

}

/*******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int ami_cury(FILE* f)

{

    winptr win; /* windows record pointer */
    int    y;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    y = win->screens[win->curupd-1]->cury; /* return current location y */
    unlockmain(); /* end exclusive access */

    return (y);

}

/*******************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

*******************************************************************************/

int ami_curxg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    x;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    x = win->screens[win->curupd-1]->curxg; /* return current location x */
    unlockmain(); /* end exclusive access */

    return (x);

}

/*******************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

*******************************************************************************/

int ami_curyg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    y;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    y = win->screens[win->curupd-1]->curyg; /* return current location y */
    unlockmain(); /* end exclusive access */

    return (y);

}

/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used, a
new screen is allocated and cleared.

The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

static void iselect(winptr win, int u, int d)

{

    int ld; /* last display screen number save */

    if (!win->bufmod) error(ebufoff); /* error */
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    ld = win->curdsp; /* save the current display screen number */
    win->curupd = u; /* set the current update screen */
    if (!win->screens[win->curupd-1]) { /* no screen, create one */

        /* get a new screen context */
        win->screens[win->curupd-1] = imalloc(sizeof(scncon));
        iniscn(win, win->screens[win->curupd-1]); /* initalize that */

    }
    win->curdsp = d; /* set the current display screen */
    if (!win->screens[win->curdsp-1]) { /* no screen, create one */

        /* no current screen, create a new one */
        win->screens[win->curdsp-1] = imalloc(sizeof(scncon));
        iniscn(win, win->screens[win->curdsp-1]); /* initalize that */

    }
    /* if the screen has changed, restore it */
    if (win->curdsp != ld) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        else restore(win, TRUE);

    }

}

void ami_select(FILE* f, int u, int d)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iselect(win, u, d); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attributes.

Note: cannot place text with foreground for background xor modes, since there
is no direct windows feature for that. The only way to add this is to write
characters into a buffer DC, blt them back to the main buffer and display
using ROP combinations.

*******************************************************************************/

static void plcchr(winptr win, char c)

{

    int    b;   /* int return */
    int    off; /* subscript offset */
    SIZE   sz;  /* size holder */
    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* handle special character cases first */
    if (c == '\r') {

        /* carriage return, position to extreme left */
        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;
        if (indisp(win)) setcur(win); /* set cursor on screen */

    } else if (c == '\n') {

        /* because the I/O package is a Unix/Linux emulation environment, we
           linefeed to CRLF */
        idown(win); /* line feed, move down */
        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;
        if (indisp(win)) setcur(win); /* set cursor on screen */

    } else if (c == '\b') ileft(win); /* back space, move left */
    else if (c == '\f') iclear(win); /* clear screen */
    else if (c == '\t') itab(win); /* process tab */
    else if (c >= ' ' && c != 0x7f) { /* character is visible */

        off = 0; /* set no subscript offset */
        if (BIT(sasubs) & sc->attr) off = trunc(win->linespace*0.35);
        /* update buffer */
        if (win->bufmod) { /* buffer is active */

            /* draw character */
            b = TextOut(sc->bdc, sc->curxg-1, sc->curyg-1+off, &c, 1);
            if (!b) winerr(); /* process windows error */

        }
        if (indisp(win)) { /* activate on screen */

            /* draw character on screen */
            curoff(win); /* hide the cursor */
            /* draw character */
            b = TextOut(win->devcon, sc->curxg-1, sc->curyg-1+off, &c, 1);
            if (!b) winerr(); /* process windows error */
            curon(win); /* show the cursor */

        }
        if (sc->cfont->sys) iright(win); /* move cursor right character */
        else { /* perform proportional version */

            b = GetTextExtentPoint32(sc->bdc, &c, 1, &sz); /* get spacing */
            if (!b) winerr(); /* process windows error */
            sc->curxg = sc->curxg+sz.cx; /* advance the character width */
            sc->curx = sc->curxg/win->charspace+1; /* recalculate character position */
            if (indisp(win)) setcur(win); /* set cursor on screen */

        }

    }

}

/*******************************************************************************

Write string to current cursor position

Writes a string to the current cursor position,  updates the cursor
position. This acts as a series of write character calls. However, it eliminates
several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

*******************************************************************************/

static void iwrtstr(winptr win,  char* s)

{

    int    b;   /* int return */
    int    off; /* subscript offset */
    SIZE   sz;  /* size holder */
    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (sc->autof) error(estrato); /* autowrap is on */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    off = 0; /* set no subscript offset */
    if (BIT(sasubs) & sc->attr) off = trunc(win->linespace*0.35);
    /* update buffer */
    if (win->bufmod) { /* buffer is active */

       /* draw character */
       b = TextOut(sc->bdc, sc->curxg-1, sc->curyg-1+off, s, strlen(s));
       if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* activate on screen */

       /* draw character on screen */
       curoff(win); /* hide the cursor */
       /* draw character */
       b = TextOut(win->devcon, sc->curxg-1, sc->curyg-1+off, s, strlen(s));
       if (!b) winerr(); /* process windows error */
       curon(win); /* show the cursor */

    }
    if (sc->cfont->sys) { /* perform fixed system advance */

          /* should check if this exceeds INT_MAX */
          sc->curx = sc->curx+strlen(s); /* update position */
          sc->curxg = sc->curxg+win->charspace*strlen(s);

    } else { /* perform proportional version */

       b = GetTextExtentPoint32(sc->bdc, s, strlen(s), &sz); /* get spacing */
       if (!b) winerr(); /* process windows error */
       sc->curxg = sc->curxg+sz.cx; /* advance the character width */
       sc->curx = sc->curxg/win->charspace+1; /* recalculate character position */
       if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

void ami_wrtstr(FILE* f, char* s)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iwrtstr(win, s); /* perform string write */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

static void idel(winptr win)

{

    ileft(win); /* back up cursor */
    plcchr(win, ' '); /* blank out */
    ileft(win); /* back up again */

}

void ami_del(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    idel(win); /* perform delete */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw line

Draws a single line in the foreground color.

*******************************************************************************/

static void iline(winptr win, int x1, int y1, int x2, int y2)

{

    int b;      /* results */
    int tx, ty; /* temp holder */
    int dx, dy;
    scnptr sc;

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
    /* Try to compensate for windows not drawing line endings. */
    if (y1 == y2)  dy = 0;
    else if (y1 < y2) dy = +1;
    else dy = -1;
    if (x1 == x2) dx = 0;
    else dx = +1;
    if (win->bufmod) { /* buffer is active */

        /* set current position of origin */
        b = MoveToEx(sc->bdc, x1-1, y1-1, NULL);
        if (!b) winerr(); /* process windows error */
        b = LineTo(sc->bdc, x2-1+dx, y2-1+dy);
        if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = MoveToEx(win->devcon, x1-1, y1-1, NULL);
        if (!b) winerr(); /* process windows error */
        b = LineTo(win->devcon, x2-1+dx, y2-1+dy);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void ami_line(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iline(win, x1, y1, x2, y2); /* draw line */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

*******************************************************************************/

static void irect(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL b; /* return value */

    if (win->bufmod) { /* buffer is active */

        /* draw to buffer */
        b = Rectangle(win->screens[win->curupd-1]->bdc, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) {

        /* draw to screen */
        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = Rectangle(win->devcon, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void ami_rect(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irect(win, x1, y1, x2, y2); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

*******************************************************************************/

static void ifrect(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL    b; /* return value */
    HGDIOBJ r; /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (win->bufmod) { /* buffer is active */

        /* for filled ellipse, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw to buffer */
        b = Rectangle(sc->bdc, x1-1, y1-1, x2, y2);
        if (!b)  winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        b = Rectangle(win->devcon, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void ami_frect(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifrect(win, x1, y1, x2, y2); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

*******************************************************************************/

static void irrect(winptr win, int x1, int y1, int x2, int y2, int xs, int ys)

{


    BOOL b; /* return value */

    if (win->bufmod)  { /* buffer is active */

        /* draw to buffer */
        b = RoundRect(win->screens[win->curupd-1]->bdc, x1-1, y1-1, x2, y2, xs, ys);
        if (!b) winerr(); /* process windows error */

    }
    /* draw to screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = RoundRect(win->devcon, x1-1, y1-1, x2, y2, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void ami_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irrect(win, x1, y1, x2, y2, xs, ys); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

*******************************************************************************/

static void ifrrect(winptr win, int x1, int y1, int x2, int y2, int xs, int ys)

{

    BOOL    b; /* return value */
    HGDIOBJ r; /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (win->bufmod) { /* buffer is active */

       /* for filled ellipse, the pen and brush settings are all wrong. we need
         a single pixel pen and a background brush. we set and restore these */
       r = SelectObject(sc->bdc, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       /* draw to buffer */
       b = RoundRect(sc->bdc, x1-1, y1-1, x2, y2, xs, ys);
       if (!b) winerr(); /* process windows error */
       /* restore */
       r = SelectObject(sc->bdc, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

       if (!win->visible)  winvis(win); /* make sure we are displayed */
       r = SelectObject(win->devcon, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       curoff(win);
       b = RoundRect(win->devcon, x1-1, y1-1, x2, y2, xs, ys);
       if (!b) winerr(); /* process windows error */
       curon(win);
       r = SelectObject(win->devcon, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }

}

void ami_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifrrect(win, x1, y1, x2, y2, xs, ys); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

*******************************************************************************/

static void iellipse(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL b; /* return value */

    if (win->bufmod) { /* buffer is active */

        /* draw to buffer */
        b = Ellipse(win->screens[win->curupd-1]->bdc, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */

    }
    /* draw to screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = Ellipse(win->devcon, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void ami_ellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iellipse(win, x1, y1, x2, y2); /* draw ellipse */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

*******************************************************************************/

static void ifellipse(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL b; /* return value */
    HGDIOBJ r; /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (win->bufmod) { /* buffer is active */

        /* for filled ellipse, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw to buffer */
        b = Ellipse(sc->bdc, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        b = Ellipse(win->devcon, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void ami_fellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifellipse(win, x1, y1, x2, y2); /* draw ellipse */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw arc

Draws an arc in the current foreground color and line width. The containing
rectangle of the ellipse is given, and the start and end angles clockwise from
0 degrees delimit the arc.

Windows takes the start and end delimited by a line extending from the center
of the arc. The way we do the convertion is to project the angle upon a circle
whose radius is the precision we wish to use for the calculation.  that
point on the circle is found by triangulation.

The larger the circle of precision, the more angles can be represented, but
the trade off is that the circle must not reach the edge of an int
(-INT_MAX..INT_MAX). That means that the total logical coordinate space must be
shortened by the precision. To find out what division of the circle precis
represents, use cd = precis*2*PI. So, for example, precis == 100 means 628
divisions of the circle.

The end and start points can be negative.
Note that Windows draws arcs counterclockwise, so our start and end points are
swapped.

Negative angles are allowed.

*******************************************************************************/

static void iarc(winptr win, int x1, int y1, int x2, int y2, int sa, int ea)

{

    const int precis = 1000; /* precision of circle calculation */

    float saf, eaf;       /* starting angles in radian float */
    int   xs, ys, xe, ye; /* start and end coordinates */
    int   xc, yc;         /* center point */
    int   t;              /* swapper */
    BOOL  b;              /* return value */

    /* rationalize rectangle for processing */
    if (x1 > x2) { t = x1; x1 = x2; x2 = t; };
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; };
    /* convert start and end to radian measure */
    saf = sa*2.0*PI/INT_MAX;
    eaf = ea*2.0*PI/INT_MAX;
    /* find center of ellipse */
    xc = (x2-x1) / 2+x1;
    yc = (y2-y1) / 2+y1;
    /* resolve start to x, y */
    xs = xc+precis*cos(PI/2-saf);
    ys = yc-precis*sin(PI/2-saf);
    /* resolve } to x, y */
    xe = xc+precis*cos(PI/2-eaf);
    ye = yc-precis*sin(PI/2-eaf);
    if (win->bufmod) { /* buffer is active */

        b = Arc(win->screens[win->curupd-1]->bdc, x1-1, y1-1, x2, y2, xe, ye,
                xs, ys);
        if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = Arc(win->devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void ami_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iarc(win, x1, y1, x2, y2, sa, ea); /* draw arc */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc int above.

*******************************************************************************/

static void ifarc(winptr win, int x1, int y1, int x2, int y2, int sa, int ea)

{

    const int precis = 1000; /* precision of circle calculation */

    float saf, eaf;     /* starting angles in radian float */
    int xs, ys, xe, ye; /* start and end coordinates */
    int xc, yc;         /* center point */
    int t;              /* swapper */
    BOOL b;             /* return value */
    HGDIOBJ r;          /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd-1];
    /* rationalize rectangle for processing */
    if (x1 > x2) { t = x1; x1 = x2; x2 = t; };
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; };
    /* convert start and end to radian measure */
    saf = sa*2*PI/INT_MAX;
    eaf = ea*2*PI/INT_MAX;
    /* find center of ellipse */
    xc = (x2-x1)/2+x1;
    yc = (y2-y1)/2+y1;
    /* resolve start to x, y */
    xs = xc+precis*cos(PI/2-saf);
    ys = yc-precis*sin(PI/2-saf);
    /* resolve end to x, y */
    xe = xc+precis*cos(PI/2-eaf);
    ye = yc-precis*sin(PI/2-eaf);
    if (win->bufmod) { /* buffer is active */

        /* for filled shape, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw shape */
        b = Pie(sc->bdc, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        /* draw shape */
        b = Pie(win->devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void ami_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifarc(win, x1, y1, x2, y2, sa, ea); /* draw arc */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc int above.

*******************************************************************************/

static void ifchord(winptr win, int x1, int y1, int x2, int y2, int sa, int ea)

{

    const int precis = 1000; /* precision of circle calculation */

    float   saf, eaf;       /* starting angles in radian float */
    int     xs, ys, xe, ye; /* start and end coordinates */
    int     xc, yc;         /* center point */
    int     t;              /* swapper */
    BOOL    b;              /* return value */
    HGDIOBJ r;              /* result holder */
    scnptr  sc;             /* screen context */

    sc = win->screens[win->curupd-1];
    /* rationalize rectangle for processing */
    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (y1 > y2)  { t = y1; y1 = y2; y2 = t; }
    /* convert start and end to radian measure */
    saf = sa*2*PI/INT_MAX;
    eaf = ea*2*PI/INT_MAX;
    /* find center of ellipse */
    xc = (x2-x1)/2+x1;
    yc = (y2-y1)/2+y1;
    /* resolve start to x, y */
    xs = xc+precis*cos(PI/2-saf);
    ys = yc-precis*sin(PI/2-saf);
    /* resolve } to x, y */
    xe = xc+precis*cos(PI/2-eaf);
    ye = yc-precis*sin(PI/2-eaf);
    if (win->bufmod) { /* buffer is active */

        /* for filled shape, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw shape */
        b = Chord(sc->bdc, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        /* draw shape */
        b = Chord(win->devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void ami_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifchord(win, x1, y1, x2, y2, sa, ea); /* draw cord */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

*******************************************************************************/

static void iftriangle(winptr win, int x1, int y1, int x2, int y2, int x3, int y3)

{

    POINT   pa[3]; /* points of triangle */
    BOOL    b;     /* return value */
    HGDIOBJ r;     /* result holder */
    scnptr  sc;

    sc = win->screens[win->curupd-1];
    /* place triangle points in array */
    pa[0].x = x1-1;
    pa[0].y = y1-1;
    pa[1].x = x2-1;
    pa[1].y = y2-1;
    pa[2].x = x3-1;
    pa[2].y = y3-1;
    if (win->bufmod) { /* buffer is active */

        /* for filled shape, the pen and brush settings are all wrong. we need
           a single pixel pen and a background brush. we set and restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw to buffer */
        b = Polygon(sc->bdc, pa, 3);
        if (!b) winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        b = Polygon(win->devcon, pa, 3);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void ami_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iftriangle(win, x1, y1, x2, y2, x3, y3); /* draw triangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

*******************************************************************************/

static void isetpixel(winptr win, int x, int y)

{

    COLORREF r; /* return value */

    if (win->bufmod)  { /* buffer is active */

        /* paint buffer */
        r = SetPixel(win->screens[win->curupd-1]->bdc, x-1, y-1,
                     win->screens[win->curupd-1]->fcrgb);
        if (r == -1) winerr(); /* process windows error */

    }
    /* paint screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        r = SetPixel(win->devcon, x-1, y-1, win->screens[win->curupd-1]->fcrgb);
        if (r == -1) winerr(); /* process windows error */
        curon(win);

    }

}

void ami_setpixel(FILE* f, int x, int y)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isetpixel(win, x, y); /* set pixel */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

*******************************************************************************/

static void ifover(winptr win)

{

    int r;

    win->gfmod = mdnorm; /* set foreground mode normal */
    win->screens[win->curupd-1]->fmod = mdnorm;
    r = SetROP2(win->screens[win->curupd-1]->bdc, R2_COPYPEN);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetROP2(win->devcon, R2_COPYPEN);

}

void ami_fover(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifover(win); /* set overwrite */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

*******************************************************************************/

static void ibover(winptr win)

{

    int r;

    win->gbmod = mdnorm; /* set background mode normal */
    win->screens[win->curupd-1]->bmod = mdnorm;
    r = SetBkMode(win->screens[win->curupd-1]->bdc, OPAQUE);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetBkMode(win->devcon, OPAQUE);

}

void ami_bover(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ibover(win); /* set overwrite */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

*******************************************************************************/

static void ifinvis(winptr win)

{

    int r;

    win->gfmod = mdinvis; /* set foreground mode invisible */
    win->screens[win->curupd-1]->fmod = mdinvis;
    r = SetROP2(win->screens[win->curupd-1]->bdc, R2_NOP);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetROP2(win->devcon, R2_NOP);

}

void ami_finvis(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifinvis(win); /* set invisible */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background to invisible

Sets the background write mode to invisible.

*******************************************************************************/

static void ibinvis(winptr win)

{

    int r;

    win->gbmod = mdinvis; /* set background mode invisible */
    win->screens[win->curupd-1]->bmod = mdinvis;
    r = SetBkMode(win->screens[win->curupd-1]->bdc, TRANSPARENT);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetBkMode(win->devcon, TRANSPARENT);

}

void ami_binvis(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ibinvis(win); /* set invisible */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

*******************************************************************************/

static void ifxor(winptr win)

{

    int r;

    win->gfmod = mdxor; /* set foreground mode xor */
    win->screens[win->curupd-1]->fmod = mdxor;
    r = SetROP2(win->screens[win->curupd-1]->bdc, R2_XORPEN);
    if (!r) winerr(); /* process windows error */
    if (indisp(win)) r = SetROP2(win->devcon, R2_XORPEN);

}

void ami_fxor(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifxor(win); /* set xor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background to xor

Sets the background write mode to xor.

*******************************************************************************/

static void ibxor(winptr win)

{

    win->gbmod = mdxor; /* set background mode xor */
    win->screens[win->curupd-1]->bmod = mdxor;

}

void ami_bxor(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ibxor(win); /* set xor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set line width

Sets the width of lines and several other figures.

*******************************************************************************/

static void ilinewidth(winptr win, int w)

{

    HGDIOBJ  oh; /* old pen */
    int      b;  /* return value */
    LOGBRUSH lb;
    scnptr   sc;

    sc = win->screens[win->curupd-1];
    sc->lwidth = w; /* set new width */
    /* create new pen with desired width */
    b = DeleteObject(sc->fpen); /* remove old pen */
    if (!b) winerr(); /* process windows error */
    /* create new pen */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process windows error */
    /* select to buffer dc */
    oh = SelectObject(sc->bdc, sc->fpen);
    if (oh == HGDI_ERROR) error(enosel);
    if (indisp(win)) { /* activate on screen */

        oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
        if (oh == HGDI_ERROR) error(enosel);

    }

}

void ami_linewidth(FILE* f, int w)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ilinewidth(win, w); /* set line width */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find character size x

Returns the character width.

*******************************************************************************/

int ami_chrsizx(FILE* f)

{

    winptr win; /* window pointer */
    int    cs;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    cs = win->charspace;
    unlockmain(); /* end exclusive access */

    return (cs);

}

/*******************************************************************************

Find character size y

Returns the character height.

*******************************************************************************/

int ami_chrsizy(FILE* f)

{

    winptr win; /* window pointer */
    int cs;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    cs = win->linespace;
    unlockmain(); /* end exclusive access */

    return (cs);

}

/*******************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

*******************************************************************************/

int ami_fonts(FILE* f)

{

    return (fntcnt); /* return global font counter */

}

/*******************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

*******************************************************************************/

static void ifont(winptr win, int fc)

{

    fontptr fp;

    if (win->screens[win->curupd-1]->autof)
        error(eatoftc); /* cannot perform with auto on */
    if (fc < 1) error(einvfnm); /* invalid font number */
    /* find indicated font */
    fp = fntlst;
    while (fp != NULL && fc > 1) { /* search */

       fp = fp->next; /* mext font entry */
       fc--; /* count */

    }
    if (fc > 1)  error(einvfnm); /* invalid font number */
    if (!strlen(fp->fn)) error(efntemp); /* font is not assigned */
    win->screens[win->curupd-1]->cfont = fp; /* place new font */
    win->gcfont = fp;
    newfont(win); /* activate font */
    chgcur(win); /* change cursors */

}

void ami_font(FILE* f, int fc)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifont(win, fc); /* set font */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find name of font

Returns the name of a font by number.

*******************************************************************************/

static void ifontnam(winptr win, int fc, char* fns, int fnsl)

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

void ami_fontnam(FILE* f, int fc, char* fns, int fnsl)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifontnam(win, fc, fns, fnsl); /* find font name */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

*******************************************************************************/

static void ifontsiz(winptr win, int s)

{

    /* cannot perform with system font */
    if (win->screens[win->curupd-1]->cfont->sys) error(etrmfts);
    if (win->screens[win->curupd-1]->autof)
        error(eatofts); /* cannot perform with auto on */
    win->gfhigh = s; /* set new font height */
    newfont(win); /* activate font */

}

void ami_fontsiz(FILE* f, int s)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifontsiz(win, s); /* set font size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "ledding".

Not implemented yet.

*******************************************************************************/

void ami_chrspcy(FILE* f, int s)

{

   /* not implemented */

}

/*******************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

Not implemented yet.

*******************************************************************************/

void ami_chrspcx(FILE* f, int s)

{

   /* not implemented */

}

/*******************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

*******************************************************************************/

int ami_dpmx(FILE* f)

{

    winptr win; /* window pointer */
    int    dpm;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    dpm = win->sdpmx;
    unlockmain(); /* end exclusive access */

    return (dpm);

}

/*******************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

*******************************************************************************/

int ami_dpmy(FILE* f)

{

    winptr win; /* window pointer */
    int    dpm;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    dpm = win->sdpmy;
    unlockmain(); /* end exclusive access */

    return (dpm);

}

/*******************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

*******************************************************************************/

static int istrsiz(winptr win, const char* s)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    int ss;

    /* get spacing */
    b = GetTextExtentPoint32(win->screens[win->curupd-1]->bdc, s, strlen(s),
                             &sz);
    if (!b) winerr(); /* process windows error */
    ss = sz.cx; /* return that */

    return (ss);

}

int ami_strsiz(FILE* f, const char* s)

{

    winptr win; /* window pointer */
    int    ss;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ss = istrsiz(win, s); /* find string size */
    unlockmain(); /* end exclusive access */

    return (ss);

}

/*******************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

*******************************************************************************/

static int ichrpos(winptr win, const char* s, int p)

{

    char* sp; /* string holder */
    int   siz; /* size holder */
    SIZE  sz; /* size holder */
    BOOL  b; /* return value */
    int   i; /* index for string */

    if (p < 0 || p >= strlen(s)) error(estrinx); /* out of range */
    if (p == 0) siz = 0; /* its already at the position */
    else { /* find substring length */

        /* get spacing */
        b = GetTextExtentPoint32(win->screens[win->curupd-1]->bdc, s, p, &sz);
        if (!b) winerr(); /* process windows error */
        siz = sz.cx; /* place size */

    }

    return (siz); /* return result */

}

int ami_chrpos(FILE* f, const char* s, int p)

{

    winptr win; /* window pointer */
    int    cp;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    cp = ichrpos(win, s, p); /* find character position */
    unlockmain(); /* end exclusive access */

    return (cp);

}

/*******************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function, nor can it be used on
the system font.

*******************************************************************************/

static void iwritejust(winptr win, const char* s, int n)

{

    SIZE        sz;  /* size holder */
    BOOL        b;   /* return value */
    int         off; /* subscript offset */
    GCP_RESULTS ra;  /* placement info record */
    scnptr      sc;
    DWORD       r;

    if (strlen(s) > 1000) error(estrtl);
    sc = win->screens[win->curupd-1];
    if (sc->cfont->sys) error(ejstsys); /* cannot perform on system font */
    if (sc->autof) error(eatopos); /* cannot perform with auto on */
    off = 0; /* set no subscript offset */
    if (BIT(sasubs) & sc->attr) off = trunc(win->linespace*0.35);
    /* get minimum spacing for string */
    b = GetTextExtentPoint32(sc->bdc, s, strlen(s), &sz);
    if (!b) winerr(); /* process windows error */
    /* if requested less than required, force required */
    if (sz.cx > n) n = sz.cx;
    /* find justified spacing */
    ra.lStructSize = sizeof(GCP_RESULTS); /* set length of record */
    /* new(ra.lpoutstring); */
    ra.lpOutString = NULL;
    ra.lpOrder = NULL;
    ra.lpDx = imalloc(strlen(s)*sizeof(int));
    ra.lpCaretPos = NULL;
    ra.lpClass = NULL;
    ra.lpGlyphs = NULL;
    ra.nGlyphs = strlen(s);
    ra.nMaxFit = 0;
    r = GetCharacterPlacement(win->screens[win->curupd-1]->bdc, s, strlen(s),
                              n, &ra, GCP_JUSTIFY | GCP_MAXEXTENT);
    if (r == 0) winerr(); /* process windows error */
    if (win->bufmod) { /* draw to buffer */

       /* draw the string to current position */
       b = ExtTextOut(sc->bdc, sc->curxg-1, sc->curyg-1+off, 0, NULL, s,
                      strlen(s), ra.lpDx);
       if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) {

       if (!win->visible) winvis(win); /* make sure we are displayed */
       /* draw character on screen */
       curoff(win); /* hide the cursor */
       /* draw the string to current position */
       b = ExtTextOut(win->devcon, sc->curxg-1, sc->curyg-1+off, 0, NULL,
                      s, strlen(s), ra.lpDx);
       if (!b) winerr(); /* process windows error */
       curon(win); /* show the cursor */

    }
    sc->curxg = sc->curxg+n; /* advance the character width */
    sc->curx = sc->curxg / win->charspace+1; /* recalculate character position */
    if (indisp(win)) setcur(win); /* set cursor on screen */
    ifree(ra.lpDx);

}

void ami_writejust(FILE* f, const char* s, int n)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iwritejust(win, s, n); /* write justified text */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

*******************************************************************************/

static int ijustpos(winptr win, const char* s, int p, int n)

{

    int         off; /* offset to character */
    int         w;   /* minimum string size */
    GCP_RESULTS ra;  /* placement info record */
    int         i;   /* index for string */

    if (p < 0 || p >= strlen(s))  error(estrinx); /* out of range */
    if (p == 0) off = 0; /* its already at the position */
    else { /* find substring length */

       w = istrsiz(win, s); /* find minimum character spacing */
       /* if allowed is less than the min, return nominal spacing */
       if (n <= w) off = ichrpos(win, s, p); else {

          /* find justified spacing */
          ra.lStructSize = sizeof(GCP_RESULTS); /* set length of record */
          ra.lpOutString = NULL;
          ra.lpOrder = NULL;
          /* allocate size array */
          ra.lpDx = imalloc(strlen(s)*sizeof(int));
          ra.lpCaretPos = NULL;
          ra.lpClass = NULL;
          ra.lpGlyphs = NULL;
          ra.nGlyphs = strlen(s);
          ra.nMaxFit = 0;
          r = GetCharacterPlacement(win->screens[win->curupd-1]->bdc, s,
                                    strlen(s), n, &ra,
                                    GCP_JUSTIFY | GCP_MAXEXTENT);
          if (r == 0) winerr(); /* process windows error */
          off = 0; /* clear offset */
          /* add in all widths to the left of position to offset */
          for (i = 0; i < p; i++) off = off+ra.lpDx[i];
          ifree(ra.lpDx);

       }

    }

    return (off); /* return result */

}

int ami_justpos(FILE* f, const char* s, int p, int n)

{

    winptr win; /* window pointer */
    int    jp;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    jp = ijustpos(win, s, p, n); /* find justified character position */
    unlockmain(); /* end exclusive access */

    return (jp);

}

/*******************************************************************************

Turn on condensed attribute

Turns on/off the condensed attribute. Condensed is a character set with a
shorter baseline than normal characters in the current font.

Note that the attributes can only be set singly.

*******************************************************************************/

static void icondensed(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(sacondensed); /* set attribute active */
        win->gattr |= BIT(sacondensed);

    } else { /* turn it off */

        sc->attr &= ~BIT(sacondensed); /* set attribute inactive */
        win->gattr &= ~BIT(sacondensed);

    }
    newfont(win); /* activate new font with that */

}

void ami_condensed(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    icondensed(win, e); /* set condensed */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on extended attribute

Turns on/off the extended attribute. Ext}ed is a character set with a
longer baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

static void iextended(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(saextended); /* set attribute active */
        win->gattr |= BIT(saextended);

    } else { /* turn it off */

        sc->attr &= ~BIT(saextended); /* set attribute inactive */
        win->gattr &= ~BIT(saextended);

    }
    newfont(win); /* activate new font with that */

}

void ami_extended(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iextended(win, e); /* set extended */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on extra light attribute

Turns on/off the extra light attribute. Extra light is a character thiner than
light.

Note that the attributes can only be set singly.

*******************************************************************************/

static void ixlight(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(saxlight); /* set attribute active */
        win->gattr |= BIT(saxlight);

    } else { /* turn it off */

        sc->attr &= ~BIT(saxlight); /* set attribute inactive */
        win->gattr &= ~BIT(salight);

    }
    newfont(win); /* activate new font with that */

}

void ami_xlight(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ixlight(win, e); /* set extra light */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on light attribute

Turns on/off the light attribute. Light is a character thiner than normal
characters in the currnet font.

Note that the attributes can only be set singly.

*******************************************************************************/

static void ilight(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(salight); /* set attribute active */
        win->gattr |= BIT(salight);

    } else { /* turn it off */

        sc->attr &= ~BIT(salight); /* set attribute inactive */
        win->gattr &= ~BIT(salight);

    }
    newfont(win); /* activate new font with that */

}

void ami_light(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ilight(win, e); /* set light */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on extra bold attribute

Turns on/off the extra bold attribute. Extra bold is a character thicker than
bold.

Note that the attributes can only be set singly.

*******************************************************************************/

static void ixbold(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(saxbold); /* set attribute active */
        win->gattr |= BIT(saxbold);

    } else { /* turn it off */

        sc->attr &= ~BIT(saxbold); /* set attribute inactive */
        win->gattr &= ~BIT(saxbold);

    }
    newfont(win); /* activate new font with that */

}

void ami_xbold(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ixbold(win, e); /* set extra bold */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on hollow attribute

Turns on/off the hollow attribute. Hollow is an embossed or 3d effect that
makes the characters appear sunken into the page.

Note that the attributes can only be set singly.

*******************************************************************************/

static void ihollow(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(sahollow); /* set attribute active */
        win->gattr |= BIT(sahollow);

    } else { /* turn it off */

        sc->attr &= ~BIT(sahollow); /* set attribute inactive */
        win->gattr &= ~BIT(sahollow);

    }
    newfont(win); /* activate new font with that */

}

void ami_hollow(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ihollow(win, e); /* set hollow */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on raised attribute

Turns on/off the raised attribute. Raised is an embossed or 3d effect that
makes the characters appear coming off the page.

Note that the attributes can only be set singly.

*******************************************************************************/

static void iraised(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    if (e) { /* turn it on */

        sc->attr |= BIT(saraised); /* set attribute active */
        win->gattr |= BIT(saraised);

    } else { /* turn it off */

        sc->attr &= ~BIT(saraised); /* set attribute inactive */
        win->gattr &= ~BIT(saraised);

    }
    newfont(win); /* activate new font with that */

}

void ami_raised(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iraised(win, e); /* set raised */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Delete picture

Deletes a loaded picture.

*******************************************************************************/

static void idelpict(winptr win, int p)

{

    HGDIOBJ r; /* result holder */
    BOOL b; /* result holder */

    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1].han) error(einvhan); /* bad picture handle */
    /* reselect old object */
    r = SelectObject(win->pictbl[p-1].hdc, win->pictbl[p-1].ohn);
    if (r == HGDI_ERROR) error(enosel);
    b = DeleteDC(win->pictbl[p-1].hdc); /* delete device context */
    if (!b) winerr(); /* process windows error */
    b = DeleteObject(win->pictbl[p-1].han); /* delete bitmap */
    if (!b) winerr(); /* process windows error */
    win->pictbl[p-1].han = 0; /* set this entry free */

}

void ami_delpict(FILE* f, int p)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    idelpict(win, p); /* delete picture file */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array.

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

/* find if file exists */
static int exists(char *fn)
{

    DWORD atb;

    atb = GetFileAttributes(fn);

    return (atb != INVALID_FILE_ATTRIBUTES &&
            !(atb & FILE_ATTRIBUTE_DIRECTORY));

}

static void iloadpict(winptr win, int p, char* fn)

{

    int    r;           /* result holder */
    BITMAP bmi;         /* bit map information header */
    char   fnh[MAXFNM]; /* file name holder */
    int    i;           /* index for string */

    if (strlen(fn) > MAXFNM) error(epicftl); /* filename too large */
    *fnh = 0; /* clear filename holding */
    strcpy(fnh, fn); /* copy */
    setext(fnh, ".bmp"); /* try bitmap first */
    if (!exists(fnh))  { /* try dib */

       setext(fnh, ".dib");
       if (!exists(fnh)) error(epicfnf); /* no file found */

    }
    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    /* if the slot is already occupied, delete that picture */
    if (win->pictbl[p-1].han) idelpict(win, p);
    /* load the image into memory */
    win->pictbl[p-1].han =
       LoadImage(NULL, fnh, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!win->pictbl[p-1].han) {

        /* Windows is particular about it's .bmp formats, and unfortuantely
           returns "no error" status for this problem. So we flag the error
           ourselves */
        if (GetLastError() == ERROR_SUCCESS) error(einvpff);
        else winerr(); /* process windows error */

    }
    /* put it into a device context */
    win->pictbl[p-1].hdc = CreateCompatibleDC(win->devcon);
    if (!win->pictbl[p-1].hdc) winerr(); /* process windows error */
    /* select that to device context */
    win->pictbl[p-1].ohn = SelectObject(win->pictbl[p-1].hdc, win->pictbl[p-1].han);
    if (win->pictbl[p-1].ohn == HGDI_ERROR) error(enosel);
    /* get sizes */
    r = GetObject(win->pictbl[p-1].han, sizeof(BITMAP), &bmi);
    if (!r) winerr(); /* process windows error */
    win->pictbl[p-1].sx = bmi.bmWidth; /* set size x */
    win->pictbl[p-1].sy = bmi.bmHeight; /* set size x */

}

void ami_loadpict(FILE* f, int p, char* fn)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iloadpict(win, p, fn); /* load picture file */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

*******************************************************************************/

int ami_pictsizx(FILE* f, int p)

{

    winptr win; /* window pointer */
    int    x;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1].han) error(einvhan); /* bad picture handle */
    x = win->pictbl[p-1].sx; /* return x size */
    unlockmain(); /* end exclusive access */

    return (x);

}

/*******************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

*******************************************************************************/

int ami_pictsizy(FILE* f, int p)

{

    winptr win; /* window pointer */
    int    y;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1].han) error(einvhan); /* bad picture handle */
    y = win->pictbl[p-1].sy; /* return x size */
    unlockmain(); /* end exclusive access */

    return (y);

}

/*******************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

*******************************************************************************/

static void ipicture(winptr win, int p, int x1, int y1, int x2, int y2)

{

    BOOL  b;    /* result holder */
    DWORD rop; /* rop holder */

    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1].han) error(einvhan); /* bad picture handle */
    switch (win->screens[win->curupd-1]->fmod) { /* rop */

        case mdnorm:  rop = SRCCOPY; break; /* straight */
        case mdinvis: break; /* no-op */
        case mdxor:   rop = SRCINVERT; break; /* xor */

    }
    if (win->screens[win->curupd-1]->fmod != mdinvis) { /* not a no-op */

        if (win->bufmod) { /* buffer mode on */

            /* paint to buffer */
            b = StretchBlt(win->screens[win->curupd-1]->bdc,
                           x1-1, y1-1, x2-x1+1, y2-y1+1,
                           win->pictbl[p-1].hdc, 0, 0,
                           win->pictbl[p-1].sx, win->pictbl[p-1].sy,
                           rop);
            if (!b) winerr(); /* process windows error */

        }
        if (indisp(win)) { /* paint to screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win);
            b = StretchBlt(win->devcon, x1-1, y1-1, x2-x1+1, y2-y1+1,
                           win->pictbl[p-1].hdc, 0, 0, win->pictbl[p-1].sx,
                           win->pictbl[p-1].sy, rop);
           if (!b) winerr(); /* process windows error */
           curon(win);

        }

    }

}

void ami_picture(FILE* f, int p, int x1, int y1, int x2, int y2)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ipicture(win, p, x1, y1, x2, y2); /* draw picture */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-INT_MAX to INT_MAX.

*******************************************************************************/

static void iviewoffg(winptr win, int x, int y)

{

    /* check change is needed */
    if (x != win->screens[win->curupd-1]->offx &&
        y != win->screens[win->curupd-1]->offy) {

        win->screens[win->curupd-1]->offx = x; /* set offsets */
        win->screens[win->curupd-1]->offy = y;
        win->goffx = x;
        win->goffy = y;
        iclear(win); /* clear buffer */

    }

}

void ami_viewoffg(FILE* f, int x, int y)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iviewoffg(win, x, y); /* set viewport offset */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set viewport scale

Sets the viewport scale in x and y. The scale is a real fraction between 0 and
1, with 1 being 1:1 scaling. Viewport scales are allways smaller than logical
scales, which means that there are more than one logical pixel to map to a
given physical pixel, but never the reverse. This means that pixels are lost
in going to the display, but the display never needs to interpolate pixels
from logical pixels.

Note:

Right now, symmetrical scaling (both x and y scales set the same) are all that
works completely, since we don"t presently have a method to warp text to fit
a scaling process. However, this can be done by various means, including
painting into a buffer and transfering asymmetrically, or using outlines.

*******************************************************************************/

static void iviewscale(winptr win, float x, float y)

{

    /* in this starting simplistic formula, the ratio is set x*INT_MAX/INT_MAX.
      it works, but can overflow for large coordinates or scales near 1 */
    win->screens[win->curupd-1]->wextx = 100;
    win->screens[win->curupd-1]->wexty = 100;
    win->screens[win->curupd-1]->vextx = trunc(x*100);
    win->screens[win->curupd-1]->vexty = trunc(y*100);
    win->gwextx = 100;
    win->gwexty = 100;
    win->gvextx = trunc(x*100);
    win->gvexty = trunc(y*100);
    iclear(win); /* clear buffer */

}

void ami_viewscale(FILE* f, float x, float y)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iviewscale(win, x, y); /* set viewport scale */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Aquire next input event

Waits for end returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

*******************************************************************************/

/*

Process keyboard event.
The events are mapped from IBM-PC keyboard keys as follows:

etup      up arrow            cursor up one line
etdown    down arrow          down one line
etleft    left arrow          left one character
etright   right arrow         right one character
etleftw   cntrl-left arrow    left one word
etrightw  cntrl-right arrow   right one word
ethome    cntrl-home          home of document
ethomes   shift-home          home of screen
ethomel   home                home of line
etend     cntrl-end           end of document
etends    shift-end           end of screen
etendl    end                 end of line
etscrl    shift-left arrow    scroll left one character
etscrr    shift-right arrow   scroll right one character
etscru    cntrl-up arrow      scroll up one line
etscrd    cntrl-down arrow    scroll down one line
etpagd    page down           page down
etpagu    page up             page up
ettab     tab                 tab
etenter   enter               enter line
etinsert  cntrl-insert        insert block
etinsertl shift-insert        insert line
etinsertt insert              insert toggle
etdel     cntrl-del           delete block
etdell    shift-del           delete line
etdelcf   del                 delete character forward
etdelcb   backspace           delete character backward
etcopy    cntrl-f1            copy block
etcopyl   shift-f1            copy line
etcan     esc                 cancel current operation
etstop    cntrl-s             stop current operation
etcont    cntrl-q             continue current operation
etprint   shift-f2            print document
etprintb  cntrl-f2            print block
etprints  cntrl-f3            print screen
etfun     f1-f12              int keys
etmenu    alt                 display menu
etend     cntrl-c             terminate program

This is equivalent to the CUA or Common User Access keyboard mapping with
various local extentions.

*/

static void keyevent(ami_evtrec* er, MSG* msg, int* keep)

{

   if (msg->wParam == '\r') er->etype = ami_etenter; /* set enter line */
   else if (msg->wParam == '\b')
      er->etype = ami_etdelcb; /* set delete character backwards */
   else if (msg->wParam == '\t') er->etype = ami_ettab; /* set tab */
   else if (msg->wParam == 0x03) /*etx*/  {

      er->etype = ami_etterm; /* set end program */
      fend = TRUE; /* set end was ordered */

   } else if (msg->wParam == 0x13) /* xoff */
      er->etype = ami_etstop; /* set stop program */
   else if (msg->wParam == 0x11) /* xon */
      er->etype = ami_etcont; /* set continue program */
   else if (msg->wParam == 0x1b) /* esc */
      er->etype = ami_etcan; /* set cancel operation */
   else { /* normal character */

      er->etype = ami_etchar; /* set character event */
      er->echar = msg->wParam;

   }
   *keep = TRUE; /* set keep event */

}

static void ctlevent(winptr win, ami_evtrec* er, MSG* msg, int* keep)

{

    /* find key we process */
    *keep = TRUE; /* set keep event */
    switch (msg->wParam) { /* key */

        case VK_HOME: /* home */
            if (win->cntrl) er->etype = ami_ethome; /* home document */
            else if (win->shift) er->etype = ami_ethomes; /* home screen */
            else er->etype = ami_ethomel; /* home line */
            break;

        case VK_END: /* end */
            if (win->cntrl) er->etype = ami_etend; /* } document */
            else if (win->shift) er->etype = ami_etends; /* } screen */
            else er->etype = ami_etendl; /* } line */
            break;

        case VK_UP: /* up */
            if (win->cntrl) er->etype = ami_etscru; /* scroll up */
            else er->etype = ami_etup; /* up line */
            break;

        case VK_DOWN: /* down */
            if (win->cntrl) er->etype = ami_etscrd; /* scroll down */
            else er->etype = ami_etdown; /* up line */
            break;

        case VK_LEFT: /* left */
            if (win->cntrl) er->etype = ami_etleftw; /* left one word */
            else if (win->shift) er->etype = ami_etscrl; /* scroll left one character */
            else er->etype = ami_etleft; /* left one character */
            break;

        case VK_RIGHT: /* right */
            if (win->cntrl) er->etype = ami_etrightw; /* right one word */
            else if (win->shift) er->etype = ami_etscrr; /* scroll right one character */
            else er->etype = ami_etright; /* left one character */
            break;

        case VK_INSERT: /* insert */
            if (win->cntrl) er->etype = ami_etinsert; /* insert block */
            else if (win->shift) er->etype = ami_etinsertl; /* insert line */
            else er->etype = ami_etinsertt; /* insert toggle */
            break;

        case VK_DELETE: /* delete */
            if (win->cntrl) er->etype = ami_etdel; /* delete block */
            else if (win->shift) er->etype = ami_etdell; /* delete line */
            else er->etype = ami_etdelcf; /* insert toggle */
            break;

        case VK_PRIOR: er->etype = ami_etpagu; /* page up */
        case VK_NEXT: er->etype = ami_etpagd; /* page down */
        case VK_F1: /* f1 */
            if (win->cntrl) er->etype = ami_etcopy; /* copy block */
            else if (win->shift) er->etype = ami_etcopyl; /* copy line */
            else { /* f1 */

                er->etype = ami_etfun;
                er->fkey = 1;

            }
            break;

        case VK_F2: /* f2 */
            if (win->cntrl) er->etype = ami_etprintb; /* print block */
            else if (win->shift) er->etype = ami_etprint; /* print document */
            else { /* f2 */

                er->etype = ami_etfun;
                er->fkey = 2;

            }
            break;

        case VK_F3: /* f3 */
            if (win->cntrl) er->etype = ami_etprints; /* print screen */
            else { /* f3 */

                er->etype = ami_etfun;
                er->fkey = 3;

            }
            break;

        case VK_F4: /* f4 */
            er->etype = ami_etfun;
            er->fkey = 4;
            break;

        case VK_F5: /* f5 */
            er->etype = ami_etfun;
            er->fkey = 5;
            break;

        case VK_F6: /* f6 */
            er->etype = ami_etfun;
            er->fkey = 6;
            break;

        case VK_F7: /* f7 */
            er->etype = ami_etfun;
            er->fkey = 7;
            break;

        case VK_F8: /* f8 */
            er->etype = ami_etfun;
            er->fkey = 8;
            break;

        case VK_F9: /* f9 */
            er->etype = ami_etfun;
            er->fkey = 9;
            break;

        case VK_F10: /* f10 */
            er->etype = ami_etfun;
            er->fkey = 10;
            break;

        case VK_F11: /* f11 */
            er->etype = ami_etfun;
            er->fkey = 11;
            break;

        case VK_F12: /* f12 */
            er->etype = ami_etfun;
            er->fkey = 12;
            break;

        case VK_MENU: er->etype = ami_etmenu; break; /* alt */
        case VK_CANCEL: er->etype = ami_etterm; break; /* ctl-brk */

        default: *keep = FALSE; /* others are dropped */
            break;

    }

}

/*

Process mouse event.
Buttons are assigned to Win 95 as follows:

button 1: Windows left button
button 2: Windows right button
button 3: Windows middle from left button

Button 4 is unused. Double clicks will be ignored, displaying my utter
contempt for the whole double click concept.

*/

/* update mouse parameters */

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

    }

}

/* register mouse status */

static void mouseevent(winptr win, MSG* msg)

{

    win->nmpx = msg->lParam%65536/win->charspace+1; /* get mouse x */
    win->nmpy = msg->lParam/65536/win->linespace+1; /* get mouse y */
    win->nmpxg = msg->lParam%65536+1; /* get mouse graphical x */
    win->nmpyg = msg->lParam/65536+1; /* get mouse graphical y */
    /* set new button statuses */
    if (msg->message == WM_LBUTTONDOWN) win->nmb1 = TRUE;
    if (msg->message == WM_LBUTTONUP) win->nmb1 = FALSE;
    if (msg->message == WM_MBUTTONDOWN) win->nmb2 = TRUE;
    if (msg->message == WM_MBUTTONUP) win->nmb2 = FALSE;
    if (msg->message == WM_RBUTTONDOWN) win->nmb3 = TRUE;
    if (msg->message == WM_RBUTTONUP) win->nmb3 = FALSE;

}

/* queue event to window */

static void enqueue(eqeptr* el, ami_evtrec* er)

{

    eqeptr ep; /* pointer to queue entries */

    geteqe(&ep); /* get a new event container */
    memcpy(&ep->evt, er, sizeof(ami_evtrec)); /* copy event to container */
    /* insert into bubble list */
    if (*el == NULL) { /* list empty, place as first entry */

        ep->last = ep; /* tie entry to itself */
        ep->next = ep;

    } else { /* list has entries */

        ep->last = *el; /* link last to current */
        ep->next = (*el)->next; /* link next to next of current */
        (*el)->next = ep; /* link current to new */

    }
    *el = ep; /* set that as new root */
    /* ok, new entries are moving to the last direction, and we remove entries
       from the next direction. */

}

/* issue event for changed button */

static void updn(ami_evtrec* er, MSG* msg, int ofn, int bn, int bm, int* keep)

{

   /* if there is already a message processed, enqueue that */
   if (*keep) enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */
   if (msg->wParam && bm) { /* assert */

      er->etype = ami_etjoyba; /* set assert */
      if (msg->message == MM_JOY1BUTTONDOWN ||
          msg->message == MM_JOY1BUTTONUP) er->ajoyn = 1;
      else er->ajoyn = 2;
      er->ajoybn = bn; /* set number */

   } else { /* deassert */

      er->etype = ami_etjoybd; /* set deassert */
      if (msg->message == MM_JOY1BUTTONDOWN ||
          msg->message == MM_JOY1BUTTONUP) er->ajoyn = 1;
      else er->ajoyn = 2;
      er->djoybn = bn; /* set number */

   }
   *keep = TRUE; /* set keep event */

}

/* process joystick messages */

static void joymes(ami_evtrec* er, MSG* msg, int ofn, int* keep)

{

   /* register changes on each button */
   if (msg->wParam & JOY_BUTTON1CHG) updn(er, msg, ofn, 1, JOY_BUTTON1, keep);
   if (msg->wParam & JOY_BUTTON2CHG) updn(er, msg, ofn, 2, JOY_BUTTON2, keep);
   if (msg->wParam & JOY_BUTTON3CHG) updn(er, msg, ofn, 3, JOY_BUTTON3, keep);
   if (msg->wParam & JOY_BUTTON4CHG) updn(er, msg, ofn, 4, JOY_BUTTON4, keep);

}

/* process windows messages to event */

static void winevt(winptr win, ami_evtrec* er, MSG* msg, int ofn, int* keep)

{

    RECT   cr;         /* client rectangle */
    wigptr wp;         /* widget entry pointer */
    int    r;          /* result holder */
    int    b;          /* int result */
    int    v;          /* value */
    int    x, y, z;    /* joystick readback */
    int    dx, dy, dz; /* joystick readback differences */
    int    nm;         /* notification message */
    float  f;          /* floating point temp */
    NMHDR* nhp;        /* notification header */

//dbg_printf(dlinfo, "message: "); prtmsg(msg); fprintf(stderr, "\n"); fflush(stderr);
    if (msg->message == WM_PAINT)  { /* window paint */

        if (!win->bufmod) { /* our client handles it"s own redraws */

            /* form redraw request */
            b = GetUpdateRect(win->winhan, &cr, FALSE);
            er->etype = ami_etredraw; /* set redraw message */
            er->rsx = msg->wParam/0x10000; /* fill the rectangle with update */
            er->rsy = msg->wParam%0x10000;
            er->rex = msg->lParam/0x10000;
            er->rey = msg->lParam%0x10000;
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == WM_SIZE) { /* resize */

        if (!win->bufmod) { /* main thread handles resizes */

            /* check if maximize, minimize, or exit from either mode */
            if (msg->wParam == SIZE_MAXIMIZED)  {

                er->etype = ami_etmax; /* set maximize event */
                /* save the event ahead of the resize */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */

            } else if (msg->wParam == SIZE_MINIMIZED)  {

                er->etype = ami_etmin; /* set minimize event */
                /* save the event ahead of the resize */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */

            } else if (msg->wParam == SIZE_RESTORED &&
                       (win->sizests == SIZE_MINIMIZED ||
                        win->sizests == SIZE_MAXIMIZED)) {

                /* window is restored, and was minimized or maximized */
                er->etype = ami_etnorm; /* set normalize event */
                /* save the event ahead of the resize */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */

            }
            win->sizests = msg->wParam; /* save size status */
            /* process resize message */
            win->gmaxxg = msg->lParam & 0xffff; /* set x size */
            win->gmaxyg = msg->lParam >> 16 & 0xffff; /* set y size */
            win->gmaxx = win->gmaxxg / win->charspace; /* find character size x */
            win->gmaxy = win->gmaxyg / win->linespace; /* find character size y */
            win->screens[win->curdsp-1]->maxx = win->gmaxx; /* copy to screen control */
            win->screens[win->curdsp-1]->maxy = win->gmaxy;
            win->screens[win->curdsp-1]->maxxg = win->gmaxxg;
            win->screens[win->curdsp-1]->maxyg = win->gmaxyg;
            /* place the resize message */
            er->etype = ami_etresize; /* set resize message */
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == WM_CHAR) keyevent(er, msg, keep); /* process characters */
    else if (msg->message == WM_KEYDOWN) {

        if (msg->wParam == VK_SHIFT) win->shift = TRUE; /* set shift active */
        if (msg->wParam == VK_CONTROL) win->cntrl = TRUE; /* set control active */
        ctlevent(win, er, msg, keep); /* process control character */

    } else if (msg->message == WM_KEYUP) {

        if (msg->wParam == VK_SHIFT) win->shift = FALSE; /* set shift inactive */
        if (msg->wParam == VK_CONTROL) win->cntrl = FALSE; /* set control inactive */

    } else if (msg->message == WM_QUIT || msg->message == WM_CLOSE) {

        er->etype = ami_etterm; /* set terminate */
        fend = TRUE; /* set } of program ordered */
        *keep = TRUE; /* set keep event */

    } else if ((msg->message == WM_MOUSEMOVE || msg->message == WM_LBUTTONDOWN ||
                msg->message == WM_LBUTTONUP || msg->message == WM_MBUTTONDOWN ||
                msg->message == WM_MBUTTONUP || msg->message == WM_RBUTTONDOWN ||
                msg->message == WM_RBUTTONUP) && mouseenb) {

        mouseevent(win, msg); /* mouse event */
        mouseupdate(win, er, keep); /* check any mouse details need processing */

    } else if (msg->message == WM_TIMER) {

        /* check its a standard timer */
        if (msg->wParam > 0 && msg->wParam <= AMI_MAXTIM)  {

            er->etype = ami_ettim; /* set timer event occurred */
            er->timnum = msg->wParam; /* set what timer */
            *keep = TRUE; /* set keep event */

        } else if (msg->wParam == FRMTIM) { /* its the framing timer */

            er->etype = ami_etframe; /* set frame event occurred */
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY2MOVE ||
               msg->message == MM_JOY1ZMOVE || msg->message == MM_JOY2ZMOVE) {

        er->etype = ami_etjoymov; /* set joystick moved */
        /* set what joystick */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY1ZMOVE)
            er->mjoyn = 1;
        else er->mjoyn = 2;
        /* Set all variables to default to same. This way, only the joystick
           axes that are actually set by the message are registered. */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY1ZMOVE) {

            x = win->joy1xs;
            y = win->joy1ys;
            z = win->joy1zs;

        } else {

            x = win->joy2xs;
            y = win->joy2ys;
            z = win->joy2zs;

        }
        /* If it's an x/y move, split the x and y axies parts of the message
           up. */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY2MOVE) {

            x = LOWORD(msg->lParam); /* get x and y for joystick */
            y = HIWORD(msg->lParam);

        /* For z axis, get a single variable. */
        } else z = msg->lParam & 0xffff;
        /* We perform thresholding on the joystick right here, which is
           limited to 255 steps (same as joystick hardware. find joystick
         diffs and update */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY1ZMOVE) {

            dx = abs(win->joy1xs-x); /* find differences */
            dy = abs(win->joy1ys-y);
            dz = abs(win->joy1zs-z);
            win->joy1xs = x; /* place old values */
            win->joy1ys = y;
            win->joy1zs = z;

        } else {

            dx = abs(win->joy2xs-x); /* find differences */
            dy = abs(win->joy2ys-y);
            dz = abs(win->joy2zs-z);
            win->joy2xs = x; /* place old values */
            win->joy2ys = y;
            win->joy2zs = z;

        }
        /* now reject moves below the threshold */
        if (dx > 65535 / 255 || dy > 65535 / 255 || dz > 65535 / 255) {

            /* scale axies between -INT_MAX..INT_MAX and place */
            er->joypx = (x - 32767)*(INT_MAX/32768);
            er->joypy = (y - 32767)*(INT_MAX/32768);
            er->joypz = (z - 32767)*(INT_MAX/32768);
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == MM_JOY1BUTTONDOWN ||
               msg->message == MM_JOY2BUTTONDOWN ||
               msg->message == MM_JOY1BUTTONUP ||
               msg->message == MM_JOY2BUTTONUP) joymes(er, msg, ofn, keep);
    else if (msg->message == WM_COMMAND) {

        if (msg->lParam) { /* it's a widget */

            wp = fndwig(win, msg->wParam & 0xffff); /* find the widget */
            if (wp == NULL) error(esystem); /* should be in the list */
            nm = msg->wParam / 0x10000; /* get notification message */
            switch (wp->typ) { /* widget type */

                case wtbutton: /* button */
                    if (nm == BN_CLICKED) {

                        er->etype = ami_etbutton; /* set button assert event */
                        er->butid = wp->id; /* get widget id */
                        *keep = TRUE; /* set keep event */

                    }
                    break;

                case wtcheckbox: /* checkbox */
                    er->etype = ami_etchkbox; /* set checkbox select event */
                    er->ckbxid = wp->id; /* get widget id */
                    *keep = TRUE; /* set keep event */
                    break;

                case wtradiobutton: /* radio button */
                    er->etype = ami_etradbut; /* set checkbox select event */
                    er->radbid = wp->id; /* get widget id */
                    *keep = TRUE; /* set keep event */
                    break;

                case wtgroup: break; /* group box, gives no messages */
                case wtbackground: break; /* background box, gives no messages */
                case wtscrollvert: break; /* scrollbar, gives no messages */
                case wtscrollhoriz: break; /* scrollbar, gives no messages */
                case wteditbox: break; /* edit box, requires no messages */
                case wtlistbox: /* list box */
                    if (nm == LBN_DBLCLK) {

                        unlockmain(); /* end exclusive access */
                        r = SendMessage(wp->han, LB_GETCURSEL, 0, 0);
                        lockmain(); /* start exclusive access */
                        if (r == -1) error(esystem); /* should be a select */
                        er->etype = ami_etlstbox; /* set list box select event */
                        er->lstbid = wp->id; /* get widget id */
                        er->lstbsl = r+1; /* set selection */
                        *keep = TRUE; /* set keep event */

                    }
                    break;

                case wtdropbox: /* drop box */
                    if (nm == CBN_SELENDOK) {

                        unlockmain(); /* end exclusive access */
                        r = SendMessage(wp->han, CB_GETCURSEL, 0, 0);
                        lockmain(); /* start exclusive access */
                        if (r == -1) error(esystem); /* should be a select */
                        er->etype = ami_etdrpbox; /* set list box select event */
                        er->drpbid = wp->id; /* get widget id */
                        er->drpbsl = r+1; /* set selection */
                        *keep = TRUE; /* set keep event */

                    }
                    break;

                case wtdropeditbox: break; /* drop edit box */
                case wtslidehoriz: break;
                case wtslidevert: break;
                case wtnumselbox: break;

            }

        } else { /* it"s a menu select */

            er->etype = ami_etmenus; /* set menu select event */
            er->menuid = msg->wParam & 0xffff; /* get menu id */
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == WM_VSCROLL)  {

        v = msg->wParam & 0xffff; /* find subcommand */
        if (v == SB_THUMBTRACK ||
            v == SB_LINEUP || v == SB_LINEDOWN ||
            v == SB_PAGEUP || v == SB_PAGEDOWN) {

            /* position request */
            wp = fndwighan(win, (HWND)msg->lParam); /* find widget tracking entry */
            if (wp == NULL) error(esystem); /* should have been found */
            if (wp->typ == wtscrollvert) { /* scroll bar */

                if (v == SB_LINEUP) {

                    er->etype = ami_etsclull; /* line up */
                    er->sclulid = wp->id;

                } else if (v == SB_LINEDOWN) {

                    er->etype = ami_etscldrl; /* line down */
                    er->scldrid = wp->id;

                } else if (v == SB_PAGEUP) {

                    er->etype = ami_etsclulp; /* page up */
                    er->sclupid = wp->id;

                } else if (v == SB_PAGEDOWN) {

                    er->etype = ami_etscldrp; /* page down */
                    er->scldpid = wp->id;

                } else {

                    er->etype = ami_etsclpos; /* set scroll position event */
                    er->sclpid = wp->id; /* set widget id */
                    f = msg->wParam/0x10000; /* get current position to float */
                    /* clamp to INT_MAX */
                    if (f*INT_MAX/(255-wp->siz) >= INT_MAX) er->sclpos = INT_MAX;
                    else er->sclpos = f*INT_MAX/(255-wp->siz);
                    /*er->sclpos = msg->wParam / 65536*0x800000*/ /* get position */

                }
                *keep = TRUE; /* set keep event */

            } else if (wp->typ == wtslidevert) { /* slider */

                er->etype = ami_etsldpos; /* set scroll position event */
                er->sldpid = wp->id; /* set widget id */
                /* get position */
                if (v == SB_THUMBTRACK) /* message includes position */
                    er->sldpos = msg->wParam/65536*(INT_MAX/100);
                else { /* must retrive the position by message */

                    unlockmain(); /* end exclusive access */
                    r = SendMessage(wp->han, TBM_GETPOS, 0, 0);
                    lockmain(); /* start exclusive access */
                    er->sldpos = r*(INT_MAX/100); /* set position */

                }
                *keep = TRUE; /* set keep event */

            } else error(esystem); /* should be one of those */

        }

    } else if (msg->message == WM_HSCROLL) {

        v = msg->wParam & 0xffff; /* find subcommand */
        if (v == SB_THUMBTRACK || v == SB_LINELEFT || v == SB_LINERIGHT ||
            v == SB_PAGELEFT || v == SB_PAGERIGHT) {

            /* position request */
            wp = fndwighan(win, (HWND)msg->lParam); /* find widget tracking entry */
            if (!wp) error(esystem); /* should have been found */
            if (wp->typ == wtscrollhoriz)  { /* scroll bar */

                if (v == SB_LINELEFT) {

                    er->etype = ami_etsclull; /* line up */
                    er->sclulid = wp->id;

                } else if (v == SB_LINERIGHT) {

                    er->etype = ami_etscldrl; /* line down */
                    er->scldrid = wp->id;

                } else if (v == SB_PAGELEFT) {

                    er->etype = ami_etsclulp; /* page up */
                    er->sclupid = wp->id;

                } else if (v == SB_PAGERIGHT) {

                    er->etype = ami_etscldrp; /* page down */
                    er->scldpid = wp->id;

                } else {

                    er->etype = ami_etsclpos; /* set scroll position event */
                    er->sclpid = wp->id; /* set widget id */
                    er->sclpos = msg->wParam / 65536*0x800000; /* get position */

                }
                *keep = TRUE; /* set keep event */

            } else if (wp->typ == wtslidehoriz) { /* slider */

                er->etype = ami_etsldpos; /* set scroll position event */
                er->sldpid = wp->id; /* set widget id */
                /* get position */
                if (v == SB_THUMBTRACK) /* message includes position */
                    er->sldpos = msg->wParam/65536*(INT_MAX/100);
                else { /* must retrive the position by message */

                    unlockmain(); /* end exclusive access */
                    r = SendMessage(wp->han, TBM_GETPOS, 0, 0);
                    lockmain(); /* start exclusive access */
                    er->sldpos = r*(INT_MAX/100); /* set position */

                }
                *keep = TRUE; /* set keep event */

            } else error(esystem); /* should be one of those */

        }

    } else if (msg->message == WM_NOTIFY) {

        wp = fndwig(win, msg->wParam); /* find widget tracking entry */
        if (!wp) error(esystem); /* should have been found */
        nhp = (NMHDR*)msg->lParam; /* convert lparam to record pointer */
        v = nhp->code; /* get code */
        /* no, I don"t know why this works, or what the TCN_SELCHANGE code is.
           Tab controls are giving me multiple indications, and the
           TCN_SELCHANGE code is more reliable as a selection indicator. */
        if (v == TCN_SELCHANGE) {

            unlockmain(); /* end exclusive access */
            r = SendMessage(wp->han, TCM_GETCURSEL, 0, 0);
            lockmain(); /* start exclusive access */
            er->etype = ami_ettabbar; /* set tab bar type */
            er->tabid = wp->id; /* set id */
            er->tabsel = r+1; /* set tab number */
            *keep = TRUE; /* keep event */

        }

    } else if (msg->message == UM_EDITCR) {

        wp = fndwig(win, msg->wParam); /* find widget tracking entry */
        if (!wp) error(esystem); /* should have been found */
        if (wp->typ == wteditbox) {

             er->etype = ami_etedtbox; /* set edit box complete event */
             er->edtbid = wp->id; /* get widget id */

        } else {

            er->etype = ami_etdrebox; /* set drop edit box complete event */
            er->drebid = wp->id; /* get widget id */

        }
        *keep = TRUE; /* set keep event */

    } else if (msg->message == UM_NUMCR) {

        wp = fndwig(win, msg->wParam); /* find widget tracking entry */
        if (!wp) error(esystem); /* should have been found */
        er->etype = ami_etnumbox; /* set number select box complete event */
        er->numbid = wp->id; /* get widget id */
        er->numbsl = msg->lParam; /* set number selected */
        *keep = TRUE; /* set keep event */

    }

}

static void sigevt(ami_evtrec* er, MSG* msg, int* keep)

{

    if (msg->message == WM_QUIT || msg->message == WM_CLOSE)  {

        er->etype = ami_etterm; /* set terminate */
        fend = TRUE; /* set end of program ordered */
        *keep = TRUE; /* set keep event */

    }

}

static void ievent(int ifn, ami_evtrec* er)

{

    MSG        msg;      /* windows message */
    int        keep;     /* keep event flag */
    winptr     win;      /* pointer to windows structure */
    int        ofn;      /* file handle from incoming message */
    eqeptr     ep;       /* event queuing pointer */
    int        b;        /* return value */
    static int ecnt = 0; /* PA event counter */

    /* Windows gdi caches, which can cause written graphics to pause uncompleted
       while we await user input. This next causes a sync-up. */
    b = GdiFlush();
    /* check there are events waiting on the input queue */
    if (opnfil[ifn]->evt) {

        /* We pick one, and only one, event off the input queue. The messages are
           removed in fifo order. */
        ep = opnfil[ifn]->evt->next; /* index the entry to dequeue */
        *er = ep->evt; /* get the queued event */
        /* if this is the last entry in the queue, just empty the list */
        if (ep->next == ep) opnfil[ifn]->evt = NULL;
        else { /* not last entry */

            ep->next->last = ep->last; /* link next to last */
            ep->last->next = ep->next; /* link last to next */
            puteqe(ep); /* release entry */

        }

    } else do {

        keep = FALSE; /* set don"t keep by default */
        /* get next message */
        getmsg(&msg);
//dbg_printf(dlinfo, "Intercom message: "); prtmsg(&msg);
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(msg.hwnd);
        /* A message can have a window associated with it, or be sent anonymously.
           Anonymous messages are typically intertask housekeeping signals. */
        if (ofn > 0)  {

            win = lfn2win(ofn); /* index window from output file */
            er->winid = filwin[ofn]; /* set window id */
            winevt(win, er, &msg, ofn, &keep); /* process messsage */
            if (!keep) sigevt(er, &msg, &keep); /* if not found, try intertask signal */

        } else sigevt(er, &msg, &keep); /* process signal */
        if (keep && (ofn >= 0))  { /* we have an event, and not main */

            /* check and error if no logical input file is linked to this output
               window */
            if (opnfil[ofn]->inl < 0) error(esystem);
            if (opnfil[ofn]->inl != ifn) {

                /* The event is not for the input agent that is calling us. We will
                   queue the message up on the input file it is intended for. Why
                   would this happen ? Only if the program is switching between
                   input channels, since each input is locked to a task. */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er);
                /* Now, keep receiving events until one matches the input file we
                   were looking for. */
                keep = FALSE;

            }

        }

    } while (!keep); /* until an event we want occurs */

    /* do diagnostic dump of PA events */
    if (dmpevt) {

        dbg_printf(dlinfo, "PA Event: %5d ", ecnt++); prtevt(er);
        fprintf(stderr, "\n"); fflush(stderr);

    }

} /* ievent */

/* external event interface */

void ami_event(FILE* f, ami_evtrec* er)

{

    do { /* loop handling via event vectors */

        lockmain(); /* start exclusive access */
        /* get logical input file number for input, and get the event for that. */
        ievent(txt2lfn(f), er); /* process event */
        unlockmain(); /* end exclusive access */
        er->handled = 1; /* set event is handled by default */
        (evtshan)(er); /* call master event handler */
        if (!er->handled) { /* send it to fanout */

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

void ami_eventover(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh)

{

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

void ami_eventsover(ami_pevthan eh,  ami_pevthan* oeh)

{

    *oeh = evtshan; /* save existing event handler */
    evtshan = eh; /* place new event handler */

}

/*******************************************************************************

Wait for intratask message

Waits for the given intratask message. Discards any other messages or
intratask messages. The im is returned back to free, which matches the common
use of im to just return the entry as acknowledgement.

*******************************************************************************/

static void waitim(imcode m, imptr* ip)

{

    int done; /* done flag */
    MSG msg;  /* message */

    done = FALSE; /* set not done */
    do {

        igetmsg(&msg); /* get next message */
        if (msg.message == UM_IM) { /* receive im */

            *ip = (imptr)msg.wParam; /* get im pointer */
            if ((*ip)->im == m) done = TRUE; /* found it */
            putitm(*ip); /* release im entry */

        }

    } while (!done); /* until message found */

}

/*******************************************************************************

Timer handler procedure

Called by the callback thunk set with TimeSetEvent. We are given a user word
of data, in which we multiplex the two peices of data we need, the logical
file number for the window file, from which we get the windows handle, and
the timer number.

With that data, we  post a message back to the queue, containing the
graph number of the timer that went off.

The reason we multiplex the logical file number is because the windows handle,
which we need, has a range determined by Windows, and we have to assume it
occupies a full word. The alternatives to multiplexing were to have the timer
callback thunk be customized, which is a non-standard solution, or use one
of the other parameters Windows passes to this function, which are not
documented.

*******************************************************************************/

static void CALLBACK timeout(UINT id, UINT msg, DWORD_PTR usr, DWORD_PTR dw1,
                      DWORD_PTR dw2)

{

    int fn; /* logical file number */
    HWND wh; /* window handle */

    lockmain(); /* start exclusive access */
    fn = usr/AMI_MAXTIM; /* get lfn multiplexed in user data */
    /* Validate it, but do nothing if wrong. We just don"t want to crash on
       errors here. */
    if (fn >= 0 && fn < MAXFIL)  /* valid lfn */
       if (opnfil[fn]) /* file is defined */
          if (opnfil[fn]->win) { /* file has window context */

        wh = opnfil[fn]->win->winhan; /* get window handle */
        unlockmain(); /* end exclusive access */
        putmsg(wh, WM_TIMER, usr%AMI_MAXTIM /* multiplexed timer number*/, 0);

    } else unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.

Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

*******************************************************************************/

static void itimer(winptr win, /* file to send event to */
                   int    lf,  /* logical file number */
                   int    i,   /* timer handle */
                   long   t,   /* number of tenth-milliseconds to run */
                   int    r)   /* timer is to rerun after completion */

{

    int tf; /* timer flags */
    int mt; /* millisecond time */

    if (i < 1 || i > AMI_MAXTIM)  error(etimnum); /* bad timer number */
    mt = t / 10; /* find millisecond time */
    if (!mt) mt = 1; /* fell below minimum, must be >= 1 */
    /* set flags for timer */
    tf = TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS;
    /* set repeat/one shot status */
    if (r) tf |= TIME_PERIODIC;
    else tf |= TIME_ONESHOT;
    /* We need both the timer number, and the window number in the handler,
      but we only have a single callback parameter available. So we mux
      them together in a word. */
    win->timers[i].han = timeSetEvent(mt, 0, timeout, lf*AMI_MAXTIM+i, tf);
    if (!win->timers[i].han) error(etimacc); /* no timer available */
    win->timers[i].rep = r; /* set timer repeat flag */
    /* should check and return an error */

}

void ami_timer(FILE* f, /* file to send event to */
                     int   i, /* timer handle */
                     long  t, /* number of tenth-milliseconds to run */
                     int   r) /* timer is to rerun after completion */

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* index output file */
    itimer(win, txt2lfn(f), i, t, r); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Kill timer

Kills a given timer, by it"s id number. Only repeating timers should be killed.

*******************************************************************************/

static void ikilltimer(winptr win, /* file to kill timer on */
                       int    i)   /* handle of timer */

{

    MMRESULT r; /* return value */

    if (i < 1 || i > AMI_MAXTIM) error(etimnum); /* bad timer number */
    r = timeKillEvent(win->timers[i].han); /* kill timer */
    if (r) error(etimacc); /* error */

}

void ami_killtimer(FILE* f, /* file to kill timer on */
               int   i) /* handle of timer */

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* index output file */
    ikilltimer(win, i); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

static void iframetimer(winptr win, int lf, int e)

{

    if (e) { /* enable framing timer */

        if (!win->frmrun) { /* it is not running */

            /* set timer to run, 17ms */
            win->frmhan = timeSetEvent(17, 0, timeout, lf*AMI_MAXTIM+FRMTIM,
                                       TIME_CALLBACK_FUNCTION ||
                                       TIME_KILL_SYNCHRONOUS ||
                                       TIME_PERIODIC);
            if (!win->frmhan) error(etimacc); /* error */
            win->frmrun = TRUE; /* set timer running */

        }

    } else { /* disable framing timer */

        if (win->frmrun)  { /* it is currently running */

            r = timeKillEvent(win->frmhan); /* kill timer */
            if (r) error(etimacc); /* error */
            win->frmrun = FALSE; /* set timer not running */

        }

    }

}

void ami_frametimer(FILE* f, int e)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* index output file */
    iframetimer(win, txt2lfn(f), e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from graph.
This exists to allow the results of graph unaware programs to be viewed after
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding graph unaware programs.

*******************************************************************************/

void ami_autohold(int e)

{

    fautohold = e; /* set new state of autohold */

}

/*******************************************************************************

Return number of mice

Returns the number of mice implemented. Windows supports only one mouse.

*******************************************************************************/

int ami_mouse(FILE* f)

{

    int rv; /* return value */

    rv = GetSystemMetrics(SM_MOUSEPRESENT); /* find mouse present */

    return (!!rv); /* set single mouse */

}

/*******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

*******************************************************************************/

int ami_mousebutton(FILE* f, int m)

{

    int bn; /* number of mouse buttons */

    if (m != 1) error(einvhan); /* bad mouse number */
    bn = GetSystemMetrics(SM_CMOUSEBUTTONS); /* find mouse buttons */

    if (bn > 3) bn = 3; /* limit mouse buttons to 3*/

    return (bn);

}

/*******************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int ami_joystick(FILE* f)

{

    winptr win; /* window pointer */
    int    jn;  /* joystick number */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    jn = win->numjoy; /* two */
    unlockmain(); /* end exclusive access */

    return (jn);

}

/*******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

int ami_joybutton(FILE* f, int j)

{

    JOYCAPS  jc;  /* joystick characteristics data */
    winptr   win; /* window pointer */
    int      nb;  /* number of buttons */
    MMRESULT r;   /* return value */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    if (j < 1 || j > win->numjoy) error(einvjoy); /* bad joystick id */
    r = joyGetDevCaps(j-1, &jc, sizeof(JOYCAPS));
    if (r != JOYERR_NOERROR) error(ejoyqry); /* could not access joystick */
    nb = jc.wNumButtons; /* set number of buttons */
    /* We don't support more than 4 buttons. */
    if (nb > 4) nb = 4;
    unlockmain(); /* end exclusive access */

    return (nb);

}

/*******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y,  z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

static int ijoyaxis(winptr win, int j)

{

    JOYCAPS  jc; /* joystick characteristics data */
    int      na; /* number of axes */
    MMRESULT r;

    if (j < 1 || j > win->numjoy) error(einvjoy); /* bad joystick id */
    r = joyGetDevCaps(j-1, &jc, sizeof(JOYCAPS));
    if (r != JOYERR_NOERROR) error(ejoyqry); /* could not access joystick */
    na = jc.wNumAxes; /* set number of axes */
    /* We don"t support more than 3 axes. */
    if (na > 3) na = 3;

    return (na);

}

int ami_joyaxis(FILE* f, int j)

{

    winptr win; /* window pointer */
    int    na;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    na = ijoyaxis(win, j); /* find joystick axes */
    unlockmain(); /* end exclusive access */

    return (na);

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

void ami_settabg(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, t); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void ami_settab(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, (t-1)*win->charspace+1); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

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
       sc->tab[MAXTAB] = 0; /* clear any last tab */

    }

}

void ami_restabg(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, t); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void ami_restab(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, (t-1)*win->charspace+1); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void ami_clrtab(FILE* f)

{

    int    i;
    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    for (i = 0; i < MAXTAB; i++) win->screens[win->curupd-1]->tab[i] = 0;
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

int ami_funkey(FILE* f)

{

    return (12); /* number of function keys */

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editting. The line is placed into the
input line buffer.

The upgrade for this is to implement a full set of editting features.

Now edits multiple buffers in multiple windows. Each event is received from
ievent, dispatched to the buffer whose window it belongs to. When a
buffer is completed by hitting "enter",  we return.

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

        ievent(fd, &er); /* get next event */
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

    } while (!lcmp); /* until line complete */
    win->inpptr = 0; /* set 1st position on active line */

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

/*******************************************************************************

Get program name

Retrieves the program name off the Windows command line. This routine is very
dependent on the structure of a windows command line. The name is enclosed
in quotes, has a path, and is terminated by ".".

A variation on this is a program that executes us directly may not have the
quotes, and we account for this. I have also accounted for there being no
extention, just in case these kinds of things turn up.

Also sets up the termination name, used to place a title on a window that is
held after execution ends for a program with autohold set.

Notes:

1. Win98 drops the quotes.
2. Win XP/2000 drops the path and the extention.
3. In windows 10, I have seen both quoted and non-quoted names.
4. In gdb runs, both \ and / are used in paths (yes at the same time).

*******************************************************************************/

static void getpgm(void)

{

    int   l;    /* length of program name string */
    char* cp;   /* holds command line pointer */
    char* s;
    char* s2;
    char  fini[] = "Finished - ";

    cp = GetCommandLine(); /* get command line */
    s = cp; /* index start of line */
    if (*s == '"') s++; /* skip quote */
    /* find last "\" or "/" in quoted section */
    s2 = NULL; /* flag not found */
    while (*s && *s != '"' && *s != ' ')
        { if (*s == '\\' || *s == '/') s2 = s+1; s++; }
    /* no path found, use whole name */
    if (!s2) s2 = cp;
    /* count program name length */
    l = 0; /* clear length */
    s = s2; /* index program name */
    /* count off length to end or extension */
    while (*s && *s != '.' && *s != ' ') { s++; l++; }
    pgmnam = imalloc(l+1);
    s = s2; /* index program name */
    s2 = pgmnam; /* index 1st character */
    while (*s && *s != '.' && *s != ' ') *s2++ = *s++; /* place characters */
    *s2 = 0; /* place terminator */
    /* form the name for termination */
    trmnam = imalloc(strlen(pgmnam)+strlen(fini)+1); /* get the holding string */
    strcpy(trmnam, fini); /* place first part */
    strcat(trmnam, pgmnam); /* place program name */

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

    fp = fntlst;
    c = 1;
    while (fp) {

        dbg_printf(dlinfo, "Font %2d: %s Fix: %d Sys: %d\n", c, fp->fn, fp->fix,
                   fp->sys);
        fp = fp->next;
        c++;

    }

}

/*******************************************************************************

Font list callback

This routine is called by windows when we register it from getfonts. It is
called once for each installed font. We ignore non-TRUEType fonts,  place
the found fonts on the global fonts list in no particular order.

Note that OpenType fonts are also flagged as TRUEType fonts.

We remove any "bold", "italic" or "oblique" descriptor word from the end of
the font string, as these are attributes, not part of the name.

*******************************************************************************/

static int CALLBACK enumfont(const LOGFONT* lfd, const TEXTMETRIC* pfd, DWORD ft, LPARAM ad)

{

    fontptr        fp;     /* pointer to font entry */
    int            c, i;   /* indexes */
    ENUMLOGFONTEX* lfde;

    lfde = (ENUMLOGFONTEX*)lfd;
    if ((ft & TRUETYPE_FONTTYPE) && (*lfde->elfFullName != '@') &&
        (lfde->elfLogFont.lfCharSet == ANSI_CHARSET ||
         lfde->elfLogFont.lfCharSet == SYMBOL_CHARSET ||
         lfde->elfLogFont.lfCharSet == DEFAULT_CHARSET))  {

      /* ansi character set, record it */
      fp = imalloc(sizeof(fontrec)); /* get a font entry */
      fp->next = fntlst; /* push to list */
      fntlst = fp;
      fntcnt = fntcnt+1; /* count font */
      fp->fn = str(lfde->elfFullName); /* create string and copy */
      fp->fix = (lfde->elfLogFont.lfPitchAndFamily & 3) == FIXED_PITCH;
      fp->sys = FALSE; /* set not system */

   }

   return (TRUE); /* set continue */

}

/*******************************************************************************

Get fonts list

Loads the windows font list. The list is filtered for TRUEType/OpenType
only. We also retrive the fixed pitch status.

Because of the callback arrangement, we have to pass the font list and count
through globals, which are  placed into the proper window.

*******************************************************************************/

static void getfonts(winptr win)

{

    int r;
    LOGFONT lf;

    fntlst = NULL; /* clear fonts list */
    fntcnt = 0; /* clear fonts count */
    lf.lfHeight = 0; /* use default height */
    lf.lfWidth = 0; /* use default width */
    lf.lfEscapement = 0; /* no escapement */
    lf.lfOrientation = 0; /* orient to x axis */
    lf.lfWeight = FW_DONTCARE; /* default weight */
    lf.lfItalic = 0;  /* no italic */
    lf.lfUnderline = 0; /* no underline */
    lf.lfStrikeOut = 0; /* no strikeout */
    lf.lfCharSet = DEFAULT_CHARSET; /* use default characters */
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS; /* use default precision */
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS; /* use default clipping */
    lf.lfQuality = DEFAULT_QUALITY; /* use default quality */
    lf.lfPitchAndFamily = 0; /* must be zero */
    lf.lfFaceName[0] = 0; /* match all typeface names */
    r = EnumFontFamiliesEx(win->devcon, &lf, enumfont, 0, 0);

#if 0
    prtfnt();
#endif

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

Separate standard fonts

Finds the standard fonts, in order, and either moves these to the top of the
table or creates a blank entry.

Note: could also default to style searching for book and sign fonts.

*******************************************************************************/

/* search string list */
fontptr schstr(char** p, int fix)

{

    fontptr fp;

    fp = NULL;
    while (!fp && *p) {

        fp = fndfnt(*p, fix);
        if (!fp) p++;

    }

    return (fp);

}

void stdfont(void)

{

    fontptr nfl; /* new font list */
    fontptr fp; /* font pointer */
    fontptr sp; /* sign font pointer */
    char* p, f;
    char* termfont[] = {

        "System Fixed",
        "Courier New",
        "DejaVu Sans Mono",
        "Liberation Mono",
        "Lucida Sans Typewriter",
        NULL

    };
    char* bookfont[] = {

        "Times New Roman",
        "Garamond",
        "Book Antiqua",
        "Georgia",
        "Palatino Linotype",
        NULL

    };
    char* signfont[] = {

        "Tahoma",
        "Microsoft Sans Serif",
        "Arial",
        "Century Gothic",
        "Franklin Gothic",
        "Trebuchet MS",
        "Verdana",

    };

    /* select first 4 fonts for standard fonts */
    nfl = NULL; /* clear target list */

    /* search 1: terminal font */
#if 1
    /* set up terminal font. terminal font is set to system default */
    fp = imalloc(sizeof(fontrec)); /* get a new entry */
    fp->fix = TRUE; /* set fixed */
    fp->sys = TRUE; /* set system */
    fp->fn = str("System Fixed");
    fp->next = nfl; /* push to fonts list */
    nfl = fp;
    fntcnt = fntcnt+1; /* add to font count */
#else
    fp = schstr(termfont, TRUE); /* search table */
    if (fp) { /* found */

        delfnt(fp); /* remove from source list */
        fp->next = nfl; /* insert to target list */
        nfl = fp;

    } else error(efntnf); /* no matching font found */
#endif

    /* search 2: book (serif) font */
    fp = schstr(bookfont, FALSE); /* search table */
    if (fp) { /* found */

        delfnt(fp); /* remove from source list */
        fp->next = nfl; /* insert to target list */
        nfl = fp;

    } else error(efntnf); /* no matching font found */

    /* search 3: sign (san serif) font */
    fp = schstr(signfont, FALSE); /* search table */
    if (fp) { /* found */

        delfnt(fp); /* remove from source list */
        fp->next = nfl; /* insert to target list */
        nfl = fp;

    } else error(efntnf); /* no matching font found */

    sp = fp; /* save sign font */

    /* search 4: technical font, make copy of sign */
    fp = (fontptr)imalloc(sizeof(fontrec));

    /* copy sign font parameters, but with new name */
    fp->fn = str("Technical");
    fp->fix = sp->fix;
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

#if 0
    prtfnt();
#endif

}

/*******************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void ami_title(FILE* f, char* ts)

{

    BOOL   b;   /* result code */
    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    /* Set title is actually done via a "wm_settext" message to the display
       window, so we have to remove the lock to allow that to be processed.
       Otherwise, setwindowtext will wait for acknoledgement of the message
       and lock us. */
    unlockmain(); /* end exclusive access */
    /* set window title text */
    b = SetWindowText(win->winhan, ts);
    lockmain();/* start exclusive access */
    if (!b) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Register standard window class

Registers the "stdwin" class. All of graph's normal windows use the same
class, which has the name "stdwin". This class only needs to be registered
once, and is thereafter referenced by name.

*******************************************************************************/

static void regstd(void)

{

    WNDCLASS wc; /* windows class structure */
    ATOM     b;

    /* set windows class to a normal window without scroll bars,
       with a windows void pointing to the message mirror.
       The message mirror reflects messages that should be handled
       by the program back into the queue, sending others on to
       the windows default handler */
//    wc.style      = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle(NULL);
    if (!wc.hInstance) winerr(); /* process windows error */
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    if (!wc.hIcon) winerr(); /* process windows error */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    if (!wc.hCursor) winerr(); /* process windows error */
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    if (!wc.hbrBackground) winerr(); /* process windows error */
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = str("stdwin");
    /* register that class */
    b = RegisterClass(&wc);
    if (!b) winerr(); /* process windows error */

}

/*******************************************************************************

Kill window

Sends a destroy window command to the window. We can"t directly kill a window
from the main thread, so we send a message to the window to kill it for us.

*******************************************************************************/

static void kilwin(HWND wh)

{

    MSG  msg; /* intertask message */
    BOOL b;

    stdwinwin = wh; /* place window handle */
    /* order window to close */
    b = PostMessage(dispwin, UM_CLSWIN, 0, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for window close. */
    do { igetmsg(&msg); } while (msg.message != UM_WINCLS);

}

/*******************************************************************************

Open and present window

Given a windows record, opens and presents the window associated with it. All
of the screen buffer data is cleared, and a single buffer assigned to the
window.

*******************************************************************************/

static void opnwin(int fn, int pfn)

{

    RECT       cr;   /* client rectangle holder */
    int        r;    /* result holder */
    int        b;    /* int result holder */
    ami_evtrec  er;   /* event holding record */
    int        ti;   /* index for repeat array */
    int        pin;  /* index for loadable pictures array */
    int        si;   /* index for current display screen */
    TEXTMETRIC tm;   /* TRUE type text metric structure */
    winptr     win;  /* window pointer */
    winptr     pwin; /* parent window pointer */
    int        f;    /* window creation flags */
    MSG        msg;  /* intertask message */
    HGDIOBJ    rv;

    win = lfn2win(fn); /* get a pointer to the window */
    /* find parent */
    win->parlfn = pfn; /* set parent logical number */
    if (pfn >= 0) {

       pwin = lfn2win(pfn); /* index parent window */
       win->parhan = pwin->winhan; /* set parent window handle */

    } else win->parhan = 0; /* set no parent */
    win->mb1 = FALSE; /* set mouse as assumed no buttons down, at origin */
    win->mb2 = FALSE;
    win->mb3 = FALSE;
    win->mpx = 1;
    win->mpy = 1;
    win->mpxg = 1;
    win->mpyg = 1;
    win->nmb1 = FALSE;
    win->nmb2 = FALSE;
    win->nmb3 = FALSE;
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
    win->numjoy = 0; /* set number of joysticks 0 */
    win->inpptr = -1; /* set buffer empty */
    win->frmrun = FALSE; /* set framing timer not running */
    win->bufmod = TRUE; /* set buffering on */
    win->menhan = 0; /* set no menu */
    win->metlst = NULL; /* clear menu tracking list */
    win->wiglst = NULL; /* clear widget list */
    win->frame = TRUE; /* set frame on */
    win->size = TRUE; /* set size bars on */
    win->sysbar = TRUE; /* set system bar on */
    win->sizests = 0; /* clear last size status word */
    /* clear timer repeat array */
    for (ti = 0; ti < 10; ti++) {

       win->timers[ti].han = 0; /* set no active timer */
       win->timers[ti].rep = FALSE; /* set no repeat */

    }
    /* clear loadable pictures table */
    for (pin = 0; pin < MAXPIC; pin++) win->pictbl[pin].han = 0;
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    win->screens[0] = imalloc(sizeof(scncon)); /* get the default screen */
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set not visible */
    /* now perform windows setup */
    /* set flags for window create */
    f = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    /* add flags for child window */
    if (win->parhan) f |= WS_CHILD | WS_CLIPSIBLINGS;
    /* Create the window, using display task. */
    stdwinflg = f;
    stdwinx = 0x80000000;
    stdwiny = 0x80000000;
    stdwinw = 0x80000000;
    stdwinh = 0x80000000;
    stdwinpar = win->parhan;
    /* order window to start */
    b = PostMessage(dispwin, UM_MAKWIN, 0, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for window start. */
    do { igetmsg(&msg); } while (msg.message != UM_WINSTR);
    win->winhan = stdwinwin; /* get the new handle */
    if (!win->winhan) winerr(); /* process windows error */

    /* Joysticks were captured with the window open. Set status of joysticks.

      Do we need to release and recapture the joysticks each time we gain and
      loose focus ? Windows could have easily outgrown that need by copying
      the joystick messages. This needs testing. */

    win->numjoy = 0; /* clear joystick counter */
    win->joy1cap = stdwinj1c; /* set joystick 1 capture status */
    win->numjoy = win->numjoy+win->joy1cap; /* count that */
    win->joy2cap = stdwinj2c; /* set joystick 1 capture status */
    win->numjoy = win->numjoy+win->joy2cap; /* count that */

    /* create a device context for the window */
    win->devcon = GetDC(win->winhan); /* get device context */
    if (!win->devcon) winerr(); /* process windows error */
    /* set rescalable mode */
    r = SetMapMode(win->devcon, MM_ANISOTROPIC);
    if (!r) winerr(); /* process windows error */
    /* set non-braindamaged stretch mode */
    r = SetStretchBltMode(win->devcon, HALFTONE);
    if (!r) winerr(); /* process windows error */
    /* remove fills */
    rv = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
    if (rv == HGDI_ERROR) error(enosel);
    /* because this is an "open ended" (no feedback) emulation, we must bring
      the terminal to a known state */
    win->gfhigh = FHEIGHT; /* set default font height */
    getfonts(win); /* get the global fonts list */
    stdfont(); /* mark/create the standard fonts */
    /* index terminal font */
    win->gcfont = fndfnt("System Fixed", TRUE);
    /* set up system default parameters */
    rv = SelectObject(win->devcon, GetStockObject(SYSTEM_FIXED_FONT));
    if (rv == HGDI_ERROR) error(enosel);
    b = GetTextMetrics(win->devcon, &tm); /* get the standard metrics */
    if (!b) winerr(); /* process windows error */
    /* calculate line spacing */
    win->linespace = tm.tmHeight;
    /* calculate character spacing */
    win->charspace = tm.tmMaxCharWidth;
    /* set cursor width */
    win->curspace = tm.tmAveCharWidth;
    /* find screen device parameters for dpm calculations */
    win->shsize = GetDeviceCaps(win->devcon, HORZSIZE); /* size x in millimeters */
    win->svsize = GetDeviceCaps(win->devcon, VERTSIZE); /* size y in millimeters */
    win->shres = GetDeviceCaps(win->devcon, HORZRES); /* pixels in x */
    win->svres = GetDeviceCaps(win->devcon, VERTRES); /* pixels in y */
    win->sdpmx = win->shres/win->shsize*1000; /* find dots per meter x */
    win->sdpmy = win->svres/win->svsize*1000; /* find dots per meter y */
    win->gmaxxg = maxxd*win->charspace; /* find client size x */
    win->gmaxyg = maxyd*win->linespace; /* find client size y */
    win->gmaxx = maxxd; /* character max x */
    win->gmaxy = maxyd; /* character max y */

#if 0
    dbg_printf(dlinfo, "Display width in pixels:  %d\n", win->shres);
    dbg_printf(dlinfo, "Display height in pixels: %d\n", win->svres);
    dbg_printf(dlinfo, "Display width in mm:      %d\n", win->shsize);
    dbg_printf(dlinfo, "Display height in mm:     %d\n", win->svsize);
    dbg_printf(dlinfo, "Dots per meter x:         %d\n", win->sdpmx);
    dbg_printf(dlinfo, "Dots per meter y:         %d\n", win->sdpmy);
    dbg_printf(dlinfo, "Client width in pixels:   %d\n", win->gmaxxg);
    dbg_printf(dlinfo, "Client height in pixels:  %d\n", win->gmaxyg);
    dbg_printf(dlinfo, "Characters in x:          %d\n", win->gmaxx);
    dbg_printf(dlinfo, "Characters in y:          %d\n", win->gmaxy);
    dbg_printf(dlinfo, "Character size x:         %d\n", win->charspace);
    dbg_printf(dlinfo, "Character size y:         %d\n", win->linespace);
#endif

    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = win->gmaxxg;
    cr.bottom = win->gmaxyg;
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    if (!b) winerr(); /* process windows error */
    /* now, resize the window to just fit our character mode */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, cr.right-cr.left, cr.bottom-cr.top,
                         SWP_NOMOVE | SWP_NOZORDER);
    if (!b) winerr(); /* process windows error */

/* now handled in winvis */
#if 0
    /* present the window */
    b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
    /* send first paint message */
    b = UpdateWindow(win->winhan);
#endif

    lockmain(); /* start exclusive access */
    /* set up global buffer parameters */

    win->gattr = 0; /* no attribute */
    win->gauto = TRUE; /* auto on */
    win->gfcrgb = colnum(ami_black); /* foreground black */
    win->gbcrgb = colnum(ami_white); /* background white */
    win->gcurv = TRUE; /* cursor visible */
    win->gfmod = mdnorm; /* set mix modes */
    win->gbmod = mdnorm;
    win->goffx = 0;  /* set 0 offset */
    win->goffy = 0;
    win->gwextx = 1; /* set 1:1 extents */
    win->gwexty = 1;
    win->gvextx = 1;
    win->gvexty = 1;
    iniscn(win, win->screens[0]); /* initalize screen buffer */
    restore(win, TRUE); /* update to screen */
/* This next is taking needed messages out of the queue, and I don"t believe it
  is needed anywmore with display tasking. */
#if 0
    /* have seen problems with actions being performed before events are pulled
      from the queue, like the focus event. the answer to this is to wait a short
      delay until these messages clear. in fact, all that is really required is
      to reenter the OS so it can do the callback */
    frmhan = TimeSetEvent(10, 0, timeout, fn*MAXTIM+1,
                          TIME_CALLBACK_INT or
                          TIME_KILL_SYNCHRONOUS or
                          TIME_ONESHOT);
    if (!frmhan) error(etimacc); /* no timer available */
    do { ievent(opnfil[fn]->inl, er);
    } while (er.etype != ami_ettim && er.etype != ami_etterm);
    if (er.etype == ami_etterm) abortm();
#endif

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
    b = ReleaseDC(win->winhan, win->devcon); /* release device context */
    if (!b) winerr(); /* process error */
    /* release the joysticks */
    if (win->joy1cap) {

        r = joyReleaseCapture(JOYSTICKID1);
        if (r) error(ejoyacc); /* error */

    }
    if (win->joy2cap) {

        r = joyReleaseCapture(JOYSTICKID2);
        if (r) error(ejoyacc); /* error */

    }
    kilwin(win->winhan); /* kill window */

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

    eqeptr ep; /* pointer for event containers */
    int    si; /* index for screens */
    filptr fp;

    fp = opnfil[fn];
    /* release all of the screen buffers */
    for (si = 0; si < MAXCON; si++)
        if (fp->win->screens[si]) ifree(fp->win->screens[si]);
    ifree(fp->win); /* release the window data */
    fp->win = NULL; /* set end open */
    fp->inw = FALSE;
    fp->inl = -1;
    while (fp->evt) {

        ep = fp->evt; /* index top */
        if (fp->evt->next == fp->evt)
            fp->evt = NULL; /* last entry, clear list */
        else fp->evt = fp->evt->next; /* gap out entry */
        ifree(ep); /* release */

    }

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

    int ifn; /* input file id */
    int wid; /* window id */

    lockmain(); /* begin exclusive access */
    wid = filwin[ofn]; /* get window id */
    ifn = opnfil[ofn]->inl; /* get the input file link */
    clswin(ofn); /* close the window */
    clsfil(ofn); /* flush and close output file */
    /* if no remaining links exist, flush and close input file */
    if (!inplnk(ifn)) clsfil(ifn);
    filwin[ofn] = -1; /* clear file to window translation */
    xltwin[wid-1] = -1; /* clear window to file translation */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   int wid)

{

    /* if output was never opened, create it now */
    if (!opnfil[ofn]) getfet(&opnfil[ofn]);
    /* if input was never opened, create it now */
    if (!opnfil[ifn]) getfet(&opnfil[ifn]);
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
        opnfil[ofn]->win = imalloc(sizeof(winrec));
        opnwin(ofn, pfn); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid-1] >= 0 && xltwin[wid-1] != ofn) error(ewinuse); /* flag error */
    xltwin[wid-1] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

}

/*******************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.
The window id can be from 0 to MAXFIL, but the input, output and error file ids
of 0, 1 and 2 are reserved for the input, output and error files, and cannot be
used directly.

Opening a window file as either input or output from/to a window creates a
special file id that bypasses the usual file I/O (much as sockets do). However,
for several reasons we open the file anyways as "nul", which in Mingw/windows
creates a "do nothing" file. This enables the file to be opened (to nothing),
and also acts as a placeholder for the fid.

The pairing of multiple output files to a single input file is done by detecting
that the same input file is used. Since no previous state is implied by a stdio
FILE structure, we do that by matching the address of the structure.

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

static void iopenwin(FILE** infile, FILE** outfile, int pfn, int wid)

{

    int ifn, ofn; /* file logical handles */

    /* check valid window handle */
    if (wid < 1 || wid > MAXFIL) error(einvwin);
    /* check if the window id is already in use */
    if (xltwin[wid-1] >= 0) error(ewinuse); /* error */
    ifn = fndfil(*infile); /* find previous open input side */
    if (ifn < 0) { /* no other input file, open new */

        /* open input file */
        unlockmain(); /* end exclusive access */
        *infile = fopen("nul", "r"); /* open null as read only */
        lockmain(); /* start exclusive access */
        if (!*infile) error(enoopn); /* can't open */

    }
    /* open output file */
    unlockmain(); /* end exclusive access */
    *outfile = fopen("nul", "w");
    ofn = fileno(*outfile); /* get logical file no. */
    if (ofn == -1) error(esystem);
    if (!*outfile) error(enoopn); /* can't open */

    /* check either input is unused, or is already an input side of a window */
    if (opnfil[ifn]) /* entry exists */
        if (!opnfil[ifn]->inw || opnfil[ifn]->win) error(einmode); /* wrong mode */
   /* check output file is in use for input or output from window */
   if (opnfil[ofn]) /* entry exists */
        if (opnfil[ofn]->inw || opnfil[ofn]->win)
            error(efinuse); /* file in use */
   /* establish all logical files and links, translation tables, and open window */
   openio(*infile, *outfile, ifn, ofn, pfn, wid);

}

void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

    winptr win; /* window context pointer */

    lockmain(); /* start exclusive access */
    if (parent) {

        win = txt2win(parent); /* validate parent is a window file */
        iopenwin(infile, outfile, txt2lfn(parent), wid); /* process open */

    } else iopenwin(infile, outfile, -1, wid); /* process open */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

*******************************************************************************/

static void isizbufg(winptr win, int x, int y)

{

    RECT cr; /* client rectangle holder */
    int si;  /* index for current display screen */

    if (x < 1 || y < 1)  error(einvsiz); /* invalid buffer size */
    /* set buffer size */
    win->gmaxx = x/win->charspace; /* find character size x */
    win->gmaxy = y/win->linespace; /* find character size y */
    win->gmaxxg = x; /* set size in pixels x */
    win->gmaxyg = y; /* set size in pixels y */
    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = win->gmaxxg;
    cr.bottom = win->gmaxyg;
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    if (!b) winerr(); /* process windows error */
    /* now, resize the window to just fit our new buffer size */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, cr.right-cr.left, cr.bottom-cr.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* all the screen buffers are wrong, so tear them out */
    for (si = 0; si < MAXCON; si++) {

        disscn(win, win->screens[si]);
        ifree(win->screens[si]); /* free screen data */
        win->screens[si] = NULL; /* clear screen data */

    }
    win->screens[win->curdsp-1] = imalloc(sizeof(scncon));
    iniscn(win, win->screens[win->curdsp-1]); /* initalize screen buffer */
    restore(win, TRUE); /* update to screen */
    if (win->curdsp != win->curupd) { /* also create the update buffer */

        win->screens[win->curupd-1] = imalloc(sizeof(scncon)); /* get the display screen */
        iniscn(win, win->screens[win->curupd-1]); /* initalize screen buffer */

    }

}

void ami_sizbufg(FILE* f, int x, int y)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isizbufg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void ami_sizbuf(FILE* f, int x, int y)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    /* just translate from characters to pixels and do the resize in pixels. */
    isizbufg(win, x*win->charspace, y*win->linespace);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

static void ibuffer(winptr win, int e)

{

    int  si; /* index for current display screen */
    BOOL b;  /* result */
    RECT r;  /* rectangle */

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
        r.left = 0; /* set up desired client rectangle */
        r.top = 0;
        r.right = win->gmaxxg;
        r.bottom = win->gmaxyg;
        /* find window size from client size */
        b = AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
        if (!b) winerr(); /* process windows error */
        /* resize the window to just fit our buffer size */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0, r.right-r.left, r.bottom-r.top,
                         SWP_NOMOVE | SWP_NOZORDER);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */
        restore(win, TRUE); /* restore buffer to screen */

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
        b = GetClientRect(win->winhan, &r);
        if (!b) winerr(); /* process windows error */
        win->gmaxxg = r.right-r.left; /* return size */
        win->gmaxyg = r.bottom-r.top;
        win->gmaxx = win->gmaxxg / win->charspace; /* find character size x */
        win->gmaxy = win->gmaxyg / win->linespace; /* find character size y */

        /* tell the window to resize */
        b = PostMessage(win->winhan, WM_SIZE, SIZE_RESTORED,
                            win->gmaxyg*65536+win->gmaxxg);
        if (!b) winerr(); /* process windows error */
        /* tell the window to repaint */
        /*b = PostMessage(win->winhan, wm_paint, 0, 0);*/
        putmsg(win->winhan, WM_PAINT, 0, 0);
        if (!b) winerr(); /* process windows error */

    }

}

void ami_buffer(FILE* f, int e)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    ibuffer(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is NULL,  the active menu is
deleted.

*******************************************************************************/

/* create menu tracking entry */
static void mettrk(winptr win, HMENU han, int inx, ami_menuptr m)

{

    metptr mp; /* menu tracking entry pointer */

    mp = imalloc(sizeof(metrec)); /* get a new tracking entry */
    mp->next = win->metlst; /* push onto tracking list */
    win->metlst = mp;
    mp->han = han; /* place menu handle */
    mp->inx = inx; /* place menu index */
    mp->onoff = m->onoff; /* place on/off highlighter */
    mp->select = FALSE; /* place status of select (off) */
    mp->id = m->id; /* place id */
    mp->oneof = NULL; /* set no "one of" */
    /* We are walking backwards in the list, and we need the next list entry
      to know the "one of" chain. So we tie the entry to itself as a flag
      that it chains to the next entry. That chain will get fixed on the
      next entry. */
    if (m->oneof) mp->oneof = mp;
    /* now tie the last entry to this if indicated */
    if (mp->next) /* there is a next entry */
        if (mp->next->oneof == mp->next) mp->next->oneof = mp;

}

/* create menu list */
static void createmenu(winptr win, ami_menuptr m, HMENU* mh)

{

    HMENU sm;  /* submenu handle */
    int   f;   /* menu flags */
    int   inx; /* index number for this menu */

    *mh = CreateMenu(); /* create new menu */
    if (!*mh) winerr(); /* process windows error */
    inx = 0; /* set first in sequence */
    while (m) { /* add menu item */

        f = MF_STRING | MF_ENABLED; /* set string and enable */
        if (m->branch) { /* handle submenu */

            createmenu(win, m->branch, &sm); /* create submenu */
            b = AppendMenu(*mh, f | MF_POPUP, (UINT_PTR)sm, m->face);
            if (!b) winerr(); /* process windows error */
            mettrk(win, *mh, inx, m); /* enter that into tracking */

        } else { /* handle terminal menu */

            b = AppendMenu(*mh, f, (UINT_PTR)m->id, m->face);
            if (!b) winerr(); /* process windows error */
            mettrk(win, *mh, inx, m); /* enter that into tracking */

        }
        if (m->bar) { /* add separator bar */

            /* a separator bar is a blank entry that will never be referenced */
            b = AppendMenu(*mh, MF_SEPARATOR, 0, "");
            if (!b) winerr(); /* process windows error */
            inx = inx+1; /* next in sequence */

        }
        m = m->next; /* next menu entry */
        inx++; /* next in sequence */

    }

}

static void imenu(winptr win, ami_menuptr m)

{

    BOOL   b;  /* int result */
    metptr mp; /* tracking entry pointer */
    RECT cr;  /* client rectangle holder */
    int  fl1; /* flag */

    if (win->menhan) { /* distroy previous menu */

        b = DestroyMenu(win->menhan); /* destroy it */
        if (!b) winerr(); /* process windows error */
        /* dispose of menu tracking entries */
        while (win->metlst) {

            mp = win->metlst; /* remove top entry */
            win->metlst = win->metlst->next; /* gap out */
            ifree(mp); /* free the entry */

        }
        win->menhan = 0; /* set no menu active */

    }
    if (m) /* there is a new menu to activate */
        createmenu(win, m, &win->menhan);
    unlockmain(); /* end exclusive access */
    b = SetMenu(win->winhan, win->menhan); /* set the menu to the window */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */
    b = DrawMenuBar(win->winhan); /* display menu */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

    /* set minimum style */
    fl1 = WS_OVERLAPPED | WS_CLIPCHILDREN;
    /* add frame features */
    if (win->size) fl1 |= WS_THICKFRAME;
    else fl1 |= WS_BORDER;
    if (win->sysbar)
        fl1 |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    /* add flags for child window */
    if (win->parhan) fl1 |= WS_CHILD | WS_CLIPSIBLINGS;
    /* change window size to match new mode */
    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = win->gmaxxg;
    cr.bottom = win->gmaxyg;
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, fl1, TRUE, 0);
    if (!b) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0,
                     cr.right-cr.left, cr.bottom-cr.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_menu(FILE* f, ami_menuptr m)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    imenu(win, m); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find menu entry

Finds a menu entry by id. If the entry is not found, it generates an error.
If the entry exists more than once, it generates an error.

*******************************************************************************/

static metptr fndmenu(winptr win, int id)

{

    metptr mp; /* tracking entry pointer */
    metptr fp; /* found entry pointer */

    fp = NULL; /* set no entry found */
    mp = win->metlst; /* index top of list */
    while (mp) { /* traverse */

        if (mp->id == id) { /* found */

            if (fp) error(edupmen); /* menu entry is duplicated */
            fp = mp; /* set found entry */

        }
        mp = mp->next; /* next entry */

    }
    if (!fp) error(emennf); /* no menu entry found with id */

    return (fp); /* return entry */

}

/*******************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

static void imenuena(winptr win, int id, int onoff)

{

    int    fl; /* flags */
    metptr mp; /* tracking pointer */
    BOOL   b;  /* result */

    mp = fndmenu(win, id); /* find the menu entry */
    fl = MF_BYPOSITION; /* place position select flag */
    if (onoff) fl |= MF_ENABLED; /* enable it */
    else fl |= MF_GRAYED; /* disable it */
    b = EnableMenuItem(mp->han, mp->inx, fl); /* perform that */
    if (b == -1) error(esystem); /* should not happen */
    unlockmain(); /* end exclusive access */
    b = DrawMenuBar(win->winhan); /* display menu */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_menuena(FILE* f, int id, int onoff)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    imenuena(win, id, onoff); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

/* find top of "one of" list */
static metptr fndtop(metptr mp)

{

    if (mp->next) /* there is a next */
        if (mp->next->oneof == mp) { /* next entry links this one */

        mp = mp->next; /* go to next entry */
        mp = fndtop(mp); /* try again */

   }

   return (mp); /* return result */

}

/* clear "one of" select list */
static void clrlst(int* fl, metptr mp)

{

    DWORD r;

    do { /* items in list */

        *fl = MF_BYPOSITION | MF_UNCHECKED; /* place position select flag */
        r = CheckMenuItem(mp->han, mp->inx, *fl); /* perform that */
        if (r == -1) error(esystem); /* should not happen */
        mp = mp->oneof; /* link next */

    } while (mp); /* no more */

}

static void imenusel(winptr win, int id, int select)

{

    int    fl; /* flags */
    metptr mp; /* tracking pointer */
    BOOL   b;  /* result */
    DWORD  r;  /* result */

    mp = fndmenu(win, id); /* find the menu entry */
    clrlst(&fl, fndtop(mp)); /* clear "one of" group */
    mp->select = select; /* set the new select */
    fl = MF_BYPOSITION; /* place position select flag */
    if (mp->select) fl |= MF_CHECKED; /* select it */
    else fl |= MF_UNCHECKED; /* deselect it */
    r = CheckMenuItem(mp->han, mp->inx, fl); /* perform that */
    if (r == -1) error(esystem); /* should not happen */
    unlockmain(); /* end exclusive access */
    b = DrawMenuBar(win->winhan); /* display menu */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_menusel(FILE* f, int id, int select)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    imenusel(win, id, select); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

static void ifront(winptr win)

{

    BOOL b; /* result holder */
    int fl;

    fl = 0;
    fl = ! fl;
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0/*fl*/ /*hwnd_topmost*/, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

#if 0
    fl = 1;
    fl = ! fl;
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, fl /*hwnd_notopmost*/, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
#endif

    unlockmain(); /* end exclusive access */
    b = PostMessage(win->winhan, WM_PAINT, 0, 0);
    if (!b) winerr(); /* process windows error */
    lockmain(); /* start exclusive access */

    if (win->parhan) {

        unlockmain(); /* end exclusive access */
        b = PostMessage(win->parhan, WM_PAINT, 0, 0);
        if (!b) winerr(); /* process windows error */
        lockmain(); /* start exclusive access */

    }

}

void ami_front(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ifront(win); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

static void iback(winptr win)

{

    BOOL b; /* result holder */

    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_back(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iback(win); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get window size graphical

Gets the onscreen window size.

*******************************************************************************/

static void igetsizg(winptr win, int* x, int* y)

{

    BOOL  b; /* result holder */
    RECT  r; /* rectangle */

    b = GetWindowRect(win->winhan, &r);
    if (!b) winerr(); /* process windows error */
    *x = r.right-r.left; /* return size */
    *y = r.bottom-r.top;

}

void ami_getsizg(FILE* f, int* x, int* y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    igetsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get window size character

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void ami_getsiz(FILE* f, int* x, int* y)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    igetsizg(win, x, y); /* execute */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *x = (*x-1) / par->charspace+1;
        *y = (*y-1) / par->linespace+1;

    } else {

        /* find character based sizes */
        *x = (*x-1) / STDCHRX+1;
        *y = (*y-1) / STDCHRY+1;

    }
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

*******************************************************************************/

static void isetsizg(winptr win, int x, int y)

{

    BOOL b; /* result holder */

    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, x, y, SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_setsizg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isetsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window size character

Sets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void ami_setsiz(FILE* f, int x, int y)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        x = x*par->charspace;
        y = y*par->linespace;

    } else {

        /* find character based sizes */
        x = x*STDCHRX;
        y = y*STDCHRY;

    }
    isetsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

*******************************************************************************/

static void isetposg(winptr win, int x, int y)

{

    BOOL b; /* result holder */

    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, x-1, y-1, 0, 0, SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_setposg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isetposg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window position character

Sets the onscreen window position, in character terms. If the window has a
parent, the demensions are converted to the current character size there.
Otherwise, pixel based demensions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void ami_setpos(FILE* f, int x, int y)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        x = (x-1)*par->charspace+1;
        y = (y-1)*par->linespace+1;

    } else {

        /* find character based sizes */
        x = (x-1)*STDCHRX+1;
        y = (y-1)*STDCHRY+1;

    }
    isetposg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get screen size graphical

Gets the total screen size.

*******************************************************************************/

static void iscnsizg(winptr win, int* x, int* y)

{

    BOOL b;      /* result holder */
    RECT r;      /* rectangle */
    HWND scnhan; /* desktop handle */

    scnhan = GetDesktopWindow();
    b = GetWindowRect(scnhan, &r);
    if (!b) winerr(); /* process windows error */
    *x = r.right-r.left; /* return size */
    *y = r.bottom-r.top;

}

void ami_scnsizg(FILE* f, int* x, int* y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscnsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find window size from client

Finds the window size, in parent terms, needed to result in a given client
window size.

Note: this routine should be able to find the minimum size of a window using
the given style, and return the minimums if the input size is lower than this.
This does not seem to be obvious under Windows.

Do we also need a menu style type ?

*******************************************************************************/

static void iwinclientg(winptr win, int cx, int cy, int* wx, int* wy,
                        ami_winmodset ms)

{

    RECT cr; /* client rectangle holder */
    int fl;  /* flag */

    lockmain(); /* start exclusive access */
    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = cx;
    cr.bottom = cy;
    /* set minimum style */
    fl = WS_OVERLAPPED | WS_CLIPCHILDREN;
    /* add flags for child window */
    if (win->parhan) fl |= WS_CHILD | WS_CLIPSIBLINGS;
    if (BIT(ami_wmframe) & ms) { /* add frame features */

        if (BIT(ami_wmsize) & ms) fl |= WS_THICKFRAME;
        if (BIT(ami_wmsysbar) & ms) fl |= WS_CAPTION | WS_SYSMENU |
                                         WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    }
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, fl, FALSE, 0);
    if (!b) winerr(); /* process windows error */
    *wx = cr.right-cr.left; /* return window size */
    *wy = cr.bottom-cr.top;
    unlockmain(); /* end exclusive access */

}

void ami_winclient(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    /* execute */
    iwinclientg(win, cx*win->charspace, cy*win->linespace, wx, wy, ms);
    /* find character based sizes */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *wx = (*wx-1) / par->charspace+1;
        *wy = (*wy-1) / par->linespace+1;

    } else {

        /* find character based sizes */
        *wx = (*wx-1) / STDCHRX+1;
        *wy = (*wy-1) / STDCHRY+1;

    }
    unlockmain(); /* end exclusive access */

}

void ami_winclientg(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iwinclientg(win, cx, cy, wx, wy, ms); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

void ami_scnsiz(FILE* f, int* x, int* y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscnsizg(win, x, y); /* execute */
    *x = *x/STDCHRX; /* convert to "standard character" size */
    *y = *y/STDCHRY;
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

static void iframe(winptr win, int e)

{

    int  fl1; /* flag */
    RECT cr;  /* client rectangle holder */

    win->frame = e; /* set new status of frame */
    fl1 = WS_OVERLAPPED | WS_CLIPCHILDREN; /* set minimum style */
    /* add flags for child window */
    if (win->parhan) fl1 |= WS_CHILD | WS_CLIPSIBLINGS;
    /* if we are enabling frames, add the frame parts back */
    if (e) {

        if (win->size) fl1 |= WS_THICKFRAME;
        if (win->sysbar) fl1 |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                               WS_MAXIMIZEBOX;

    }
    unlockmain(); /* end exclusive access */
    r = SetWindowLong(win->winhan, GWL_STYLE, fl1);
    lockmain(); /* start exclusive access */
    if (!r) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                                                 SWP_FRAMECHANGED);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* present the window */
    unlockmain(); /* end exclusive access */
    b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
    lockmain(); /* start exclusive access */
    /* change window size to match new mode */
    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = win->gmaxxg;
    cr.bottom = win->gmaxyg;
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, fl1, FALSE, 0);
    if (!b) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0,
                        cr.right-cr.left, cr.bottom-cr.top,
                        SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_frame(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iframe(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

*******************************************************************************/

static void isizable(winptr win, int e)

{

    int  fl1; /* flag */
    RECT cr;  /* client rectangle holder */
    BOOL b;
    LONG r;

    win->size = e; /* set new status of size bars */
    /* no point in making the change if framing is off entirely */
    if (win->frame) {

        /* set minimum style */
        fl1 = WS_OVERLAPPED | WS_CLIPCHILDREN;
        /* add frame features */
        if (win->size) fl1 |= WS_THICKFRAME;
        else fl1 |= WS_BORDER;
        if (win->sysbar)
            fl1 |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        /* add flags for child window */
        if (win->parhan) fl1 |= WS_CHILD | WS_CLIPSIBLINGS;
        /* if we are enabling frames, add the frame parts back */
        if (e) fl1 |= WS_THICKFRAME;
        unlockmain(); /* end exclusive access */
        r = SetWindowLong(win->winhan, GWL_STYLE, fl1);
        lockmain(); /* start exclusive access */
        if (!r) winerr(); /* process windows error */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                                                     SWP_FRAMECHANGED);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */
        /* present the window */
        unlockmain(); /* end exclusive access */
        b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
        lockmain(); /* start exclusive access */
        /* change window size to match new mode */
        cr.left = 0; /* set up desired client rectangle */
        cr.top = 0;
        cr.right = win->gmaxxg;
        cr.bottom = win->gmaxyg;
        /* find window size from client size */
        b = AdjustWindowRectEx(&cr, fl1, FALSE, 0);
        if (!b) winerr(); /* process windows error */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0,
                         cr.right-cr.left, cr.bottom-cr.top,
                         SWP_NOMOVE | SWP_NOZORDER);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void ami_sizable(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isizable(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

*******************************************************************************/

static void isysbar(winptr win, int e)

{

    int  fl1; /* flag */
    RECT cr;  /* client rectangle holder */
    LONG r;
    BOOL b;

    win->sysbar = e; /* set new status of system bar */
    /* no point in making the change if framing is off entirely */
    if (win->frame) {

        /* set minimum style */
        fl1 = WS_OVERLAPPED | WS_CLIPCHILDREN;
        /* add frame features */
        if (win->size) fl1 |= WS_THICKFRAME;
        else fl1 |= WS_BORDER;
        if (win->sysbar)
            fl1 |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        /* add flags for child window */
        if (win->parhan) fl1 |= WS_CHILD | WS_CLIPSIBLINGS;
        /* if we are enabling frames, add the frame parts back */
        if (e) fl1 |= WS_THICKFRAME;
        unlockmain(); /* end exclusive access */
        r = SetWindowLong(win->winhan, GWL_STYLE, fl1);
        lockmain(); /* start exclusive access */
        if (!r) winerr(); /* process windows error */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0, 0, 0,
                             SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */
        /* present the window */
        unlockmain(); /* end exclusive access */
        ShowWindow(win->winhan, SW_SHOWDEFAULT);
        lockmain(); /* start exclusive access */
        /* change window size to match new mode */
        cr.left = 0; /* set up desired client rectangle */
        cr.top = 0;
        cr.right = win->gmaxxg;
        cr.bottom = win->gmaxyg;
        /* find window size from client size */
        b = AdjustWindowRectEx(&cr, fl1, FALSE, 0);
        if (!b) winerr(); /* process windows error */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0,
                             cr.right-cr.left, cr.bottom-cr.top,
                             SWP_NOMOVE | SWP_NOZORDER);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void ami_sysbar(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isysbar(win, e); /* execute */
    unlockmain(); /* end exclusive access */

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

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu,  the program selections placed in the menu.

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

void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)

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

/*******************************************************************************

Create widget

Creates a widget within the given window, within the specified bounding box,
and using the face string and type, and the given id. The string may or may not
be used.

Widgets use the subthread to buffer them. There were various problems from
trying to start them on the main window.

*******************************************************************************/

/* create widget according to type */
static HWND createwidget(winptr win, wigtyp typ, int x1, int y1, int x2, int y2,
                         char* s, int id, int exfl)

{

    HWND  wh;     /* handle to widget */
    imptr ip;     /* intertask message pointer */
    char* clsstr;
    int   fl;     /* creation flag */
    int   b;      /* return value */

    /* search previous definition with same id */
    if (fndwig(win, id)) error(ewigdup); /* duplicate widget id */
    switch (typ) { /* widget type */

        case wtbutton:
            clsstr = str("button");
            fl = BS_PUSHBUTTON | exfl;  /* button */
            break;

       case wtcheckbox:
          clsstr = str("button");
          fl = BS_CHECKBOX | exfl;    /* checkbox */
          break;

       case wtradiobutton:
          clsstr = str("button");
          fl = BS_RADIOBUTTON | exfl; /* radio button */
          break;

       case wtgroup:
          clsstr = str("button");
          fl = BS_GROUPBOX | exfl;    /* group box */
          break;

       case wtbackground:
          clsstr = str("static");
          fl = 0 | exfl;   /* background */
          break;

       case wtscrollvert: /* vertical scroll bar */
          clsstr = str("scrollbar");
          fl = SBS_VERT | exfl;
          break;

       case wtscrollhoriz: /* horizontal scrollbar */
          clsstr = str("scrollbar");
          fl = SBS_HORZ | exfl;
          break;

       case wteditbox: /* single line edit */
          clsstr = str("edit");
          fl = WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | exfl;
          break;

       case wtprogressbar: /* progress bar */
          clsstr = str("msctls_progress32");
          fl = 0 | exfl;
          break;

       case wtlistbox: /* list box */
          clsstr = str("listbox");
          fl = (LBS_STANDARD&~LBS_SORT) | exfl;
          break;

       case wtdropbox: /* list box */
          clsstr = str("combobox");
          fl = CBS_DROPDOWNLIST | exfl;
          break;

       case wtdropeditbox: /* list box */
          clsstr = str("combobox");
          fl = CBS_DROPDOWN | exfl;
          break;

       case wtslidehoriz: /* horizontal slider */
          clsstr = str("msctls_trackbar32");
          fl = TBS_HORZ | TBS_AUTOTICKS | exfl;
          break;

       case wtslidevert: /* vertical slider */
          clsstr = str("msctls_trackbar32");
          fl = TBS_VERT | TBS_AUTOTICKS | exfl;
          break;

       case wttabbar: /* tab bar */
          clsstr = str("systabcontrol32");
          fl = WS_VISIBLE | exfl;
          break;

    }
    /* create an intertask message to start the widget */
    getitm(&ip); /* get a im pointer */
    ip->im = imwidget; /* set type is widget */
    ip->wt = typ; /* set type of widget */
    ip->wigcls = clsstr; /* place class string */
    ip->wigtxt = str(s); /* place face text */
    ip->wigflg = WS_CHILD | WS_VISIBLE | fl;
    ip->wigx = x1-1; /* place origin */
    ip->wigy = y1-1;
    ip->wigw = x2-x1+1; /* place size */
    ip->wigh = y2-y1+1;
    ip->wigpar = win->winhan; /* place parent */
    ip->wigid = id; /* place id */
    ip->wigmod = GetModuleHandle(NULL); /* place module */
    /* order widget to start */
    b = PostMessage(dispwin, UM_IM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for widget start, this also keeps our window going. */
    waitim(imwidget, &ip); /* wait for the return */
    wh = ip->wigwin; /* place handle to widget */
    ifree(ip->wigcls); /* release class string */
    ifree(ip->wigtxt); /* release face text string */
    putitm(ip); /* release im */

    return (wh); /* return handle */

}

static void widget(winptr win, int x1, int y1, int x2, int y2, char* s, int id,
                   wigtyp typ, int exfl, wigptr* wp)

{

    getwig(win, wp); /* get new widget */
    /* Group widgets don"t have a background, so we pair it up with a background
       widget. */
    if (typ == wtgroup)  /* create buddy for group */
        (*wp)->han2 = createwidget(win, wtbackground, x1, y1, x2, y2, "", id,
                                  exfl);
    /* create widget */
    (*wp)->han = createwidget(win, typ, x1, y1, x2, y2, s, id, exfl);
    (*wp)->id = id; /* place button id */
    (*wp)->typ = typ; /* place type */

}

/*******************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

static void ikillwidget(winptr win, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    kilwin(wp->han); /* kill window */
    if (wp->han2) kilwin(wp->han2); /* distroy buddy window */
    putwig(win, wp); /* release widget entry */

}

void ami_killwidget(FILE* f, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ikillwidget(win, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

static void iselectwidget(winptr win, int id, int e)

{

    wigptr wp;  /* widget entry */
    int    r; /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    /* check this widget is selectable */
    if (wp->typ != wtcheckbox && wp->typ != wtradiobutton) error(ewigsel);
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, BM_SETCHECK, e, 0);
    lockmain();/* start exclusive access */

}

void ami_selectwidget(FILE* f, int id, int e)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iselectwidget(win, id, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

static void ienablewidget(winptr win, int id, int e)

{

    wigptr wp; /* widget entry */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    /* check this widget can get text */
    if (wp->typ != wtbutton && wp->typ != wtcheckbox &&
        wp->typ != wtradiobutton && wp->typ != wtgroup &&
        wp->typ != wtscrollvert && wp->typ != wtscrollhoriz &&
        wp->typ != wtnumselbox && wp->typ != wteditbox &&
        wp->typ != wtlistbox && wp->typ != wtdropbox &&
        wp->typ != wtdropeditbox && wp->typ != wtslidehoriz &&
        wp->typ != wtslidevert && wp->typ != wttabbar) error(ewigdis);
    unlockmain(); /* end exclusive access */
    EnableWindow(wp->han, e); /* perform */
    lockmain(); /* start exclusive access */
    wp->enb = e; /* save enable/disable status */

}

void ami_enablewidget(FILE* f, int id, int e)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ienablewidget(win, id, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

static void igetwidgettext(winptr win, int id, char* s, int sl)

{

    wigptr wp; /* widget pointer */
    int    ls; /* length of text */
    char*  sp; /* pointer to string */
    int    i;  /* index for string */
    int    r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    /* check this widget can get text */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox) error(ewiggtxt);
    /* There is no real way to process an error, as listed in the
      documentation, for GetWindowText. The docs define
      a zero return as being for a zero length string, but also apparently
      uses that value for errors. */
    unlockmain(); /* end exclusive access */
    r = GetWindowText(wp->han, s, sl); /* get the text */
    lockmain(); /* start exclusive access */

}

void ami_getwidgettext(FILE* f, int id, char* s, int sl)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igetwidgettext(win, id, s, sl); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Put edit box text

Places text into an edit box.

*******************************************************************************/

static void iputwidgettext(winptr win, int id, char* s)

{

    wigptr wp; /* widget pointer */
    BOOL   b;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    /* check this widget can put text */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox) error(ewigptxt);
    unlockmain(); /* end exclusive access */
    b = SetWindowText(wp->han, s); /* get the text */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void ami_putwidgettext(FILE* f, int id, char* s)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iputwidgettext(win, id, s); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Resize widget

Changes the size of a widget.

*******************************************************************************/

static void isizwidgetg(winptr win, int id,  int x, int y)

{

    wigptr wp; /* widget pointer */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, 0, 0, 0, x, y, SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also resize the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, 0, 0, 0, x, y, SWP_NOMOVE | SWP_NOZORDER);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void ami_sizwidgetg(FILE* f, int id, int x, int y)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    isizwidgetg(win, id, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Reposition widget

Changes the parent position of a widget.

*******************************************************************************/

static void iposwidgetg(winptr win, int id, int x, int y)

{

    wigptr wp; /* widget pointer */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, 0, x-1, y-1, 0, 0, SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also reposition the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, 0, x-1, y-1, 0, 0, SWP_NOSIZE);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void ami_poswidgetg(FILE* f, int id, int x, int y)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iposwidgetg(win, id, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Place widget to back of Z order

*******************************************************************************/

static void ibackwidget(winptr win, int id)

{

    wigptr wp; /* widget pointer */
    BOOL   b;  /* result holder */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also reposition the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void ami_backwidget(FILE* f, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
     win = txt2win(f); /* get windows context */
    ibackwidget(win, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Place widget to front of Z order

*******************************************************************************/

static void ifrontwidget(winptr win, int id)

{

    wigptr wp; /* widget pointer */
    BOOL   b;  /* result holder */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also reposition the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void ami_frontwidget(FILE* f, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ifrontwidget(win, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

*******************************************************************************/

static void ibuttonsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* add button borders to size */
    *w = sz.cx+GetSystemMetrics(SM_CXEDGE)*2;
    *h = sz.cy+GetSystemMetrics(SM_CYEDGE)*2;

}

static void ibuttonsiz(winptr win, char* s, int* w, int* h)

{

    ibuttonsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void ami_buttonsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibuttonsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_buttonsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibuttonsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

*******************************************************************************/

static void ibuttong(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtbutton, 0, &wp);

}

static void ibutton(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    ibuttong(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void ami_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibuttong(win, x1, y1, x2, y2, s, id);
    unlockmain(); /* end exclusive access */

}

void ami_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibutton(win, x1, y1, x2, y2, s, id);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

*******************************************************************************/

static void icheckboxsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* We needed to add a fudge factor for the space between the checkbox, the
       left edge of the widget, and the left edge of the text. */
    *w = sz.cx+GetSystemMetrics(SM_CXMENUCHECK)+6; /* return size */
    *h = sz.cy;

}

static void icheckboxsiz(winptr win, char* s, int* w, int* h)

{

    icheckboxsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void ami_checkboxsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckboxsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_checkboxsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckboxsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

static void icheckboxg(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtcheckbox, 0, &wp);

}

static void icheckbox(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    icheckboxg(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void ami_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckboxg(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckbox(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

*******************************************************************************/

static void iradiobuttonsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* We needed to add a fudge factor for the space between the checkbox, the
       left edge of the widget, and the left edge of the text. */
    *w = sz.cx+GetSystemMetrics(SM_CXMENUCHECK)+6; /* return size */
    *h = sz.cy;

}

static void iradiobuttonsiz(winptr win, char* s, int* w, int* h)

{

    iradiobuttonsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void ami_radiobuttonsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobuttonsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_radiobuttonsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobuttonsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

*******************************************************************************/

static void iradiobuttong(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtradiobutton, 0, &wp);

}

static void iradiobutton(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iradiobuttong(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void ami_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobuttong(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobutton(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

static void igroupsizg(winptr win, char* s, int cw, int ch, int* w, int* h,
                       int* ox, int* oy)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* Use the string sizing, and rules of thumb for the edges */
    *w = sz.cx+7*2; /* return size */
    /* if string is greater than width plus edges, use the string. */
    if (cw+7*2 > *w) *w = cw+7*2;
    *h = sz.cy+ch+5*2;
    /* set offset to client area */
    *ox = 5;
    *oy = sz.cy;

}

static void igroupsiz(winptr win, char* s, int cw, int ch, int* w, int* h,
                      int* ox, int* oy)

{

    /* convert client sizes to graphical */
    cw = cw*win->charspace;
    ch = ch*win->linespace;
    igroupsizg(win, s, cw, ch, w, h, ox, oy); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;
    *ox = (*ox-1)/win->charspace+1;
    *oy = (*oy-1)/win->linespace+1;

}

void ami_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h,
                  int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroupsizg(win, s, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h,
              int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroupsiz(win, s, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

*******************************************************************************/

static void igroupg(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtgroup, 0, &wp);

}

static void igroup(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    igroupg(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void ami_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroupg(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroup(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

static void ibackgroundg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtbackground, 0, &wp);

}

static void ibackground(winptr win, int x1, int y1, int x2, int y2, int id)

{

   /* form graphical from character coordinates */
   x1 = (x1-1)*win->charspace+1;
   y1 = (y1-1)*win->linespace+1;
   x2 = (x2)*win->charspace;
   y2 = (y2)*win->linespace;
   ibackgroundg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void ami_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibackgroundg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_background(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibackground(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollvertsizg(winptr win, int* w, int* h)

{

    /* get system values for scroll bar arrow width and height, for which there
       are two. */
    *w = GetSystemMetrics(SM_CXVSCROLL);
    *h = GetSystemMetrics(SM_CYVSCROLL)*2;

}

static void iscrollvertsiz(winptr win, int* w, int* h)

{

    /* Use fixed sizes, as this looks best */
    *w = 2;
    *h = 2;

}

void ami_scrollvertsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvertsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_scrollvertsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvertsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

*******************************************************************************/

static void iscrollvertg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr     wp; /* widget pointer */
    SCROLLINFO si; /* scroll information structure */
    BOOL       b;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtscrollvert, 0, &wp);
    /* The scroll set for windows is arbitrary. We expand that to 0..INT_MAX on
       messages. */
    unlockmain(); /* end exclusive access */
    b = SetScrollRange(wp->han, SB_CTL, 0, 255, FALSE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* retrieve the default size of slider */
    si.cbSize = sizeof(SCROLLINFO); /* set size */
    si.fMask = SIF_PAGE; /* set page size */
    unlockmain(); /* end exclusive access */
    b = GetScrollInfo(wp->han, SB_CTL, &si);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    wp->siz = si.nPage; /* get size */

}

static void iscrollvert(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iscrollvertg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void ami_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvertg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvert(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollhorizsizg(winptr win, int* w, int* h)

{

    /* get system values for scroll bar arrow width and height, for which there
       are two. */
    *w = GetSystemMetrics(SM_CXHSCROLL)*2;
    *h = GetSystemMetrics(SM_CYHSCROLL);

}

static void iscrollhorizsiz(winptr win, int* w, int* h)

{

    /* Use fixed sizes, as this looks best */
    *w = 2;
    *h = 1;

}

void ami_scrollhorizsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhorizsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_scrollhorizsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhorizsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

*******************************************************************************/

static void iscrollhorizg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr     wp; /* widget pointer */
    SCROLLINFO si; /* scroll information structure */
    BOOL       b;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtscrollhoriz, 0, &wp);
    /* The scroll set for windows is arbitrary. We expand that to 0..INT_MAX on
       messages. */
    unlockmain(); /* end exclusive access */
    b = SetScrollRange(wp->han, SB_CTL, 0, 255, FALSE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* retrieve the default size of slider */
    si.cbSize = sizeof(SCROLLINFO); /* set size */
    si.fMask = SIF_PAGE; /* set page size */
    unlockmain(); /* end exclusive access */
    b = GetScrollInfo(wp->han, SB_CTL, &si);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    wp->siz = si.nPage; /* get size */

}

static void iscrollhoriz(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iscrollhorizg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void ami_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhorizg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhoriz(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

static void iscrollpos(winptr win, int id, int r)

{

    wigptr wp; /* widget pointer */
    int    rv; /* return value */
    float  f;  /* floating temp */
    int    p;  /* calculated position to set */

    if (r < 0) error(einvspos); /* invalid position */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    f = r; /* place position in float */
    /* clamp to max */
    if (f*(255-wp->siz)/INT_MAX > 255) p = 255;
    else p = f*(255-wp->siz)/INT_MAX;
    unlockmain(); /* end exclusive access */
    rv = SetScrollPos(wp->han, SB_CTL, p, TRUE);
    lockmain();/* start exclusive access */

}

void ami_scrollpos(FILE* f, int id, int r)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollpos(win, id, r); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

static void iscrollsiz(winptr win, int id, int r)

{

    wigptr     wp; /* widget pointer */
    int        rv; /* return value */
    SCROLLINFO si; /* scroll information structure */

    if (r < 0) error(einvssiz); /* invalid scrollbar size */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    si.cbSize = sizeof(SCROLLINFO); /* set size */
    si.fMask = SIF_PAGE; /* set page size */
    si.nMin = 0; /* no min */
    si.nMax = 0; /* no max */
    si.nPage = r/0x800000; /* set size */
    si.nPos = 0; /* no position */
    si.nTrackPos = 0; /* no track position */
    unlockmain(); /* end exclusive access */
    rv = SetScrollInfo(wp->han, SB_CTL, &si, TRUE);
    lockmain(); /* start exclusive access */
    wp->siz = r/0x800000; /* set size */

}

void ami_scrollsiz(FILE* f, int id, int r)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollsiz(win, id, r); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Control window function for number edit box

This message handler is to allow us to capture the carriage return from an
number edit box, and turn that into a message. It also restricts input to the
box to numeric characters.

*******************************************************************************/

static LRESULT CALLBACK wndprocnum(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam)

{

    int    r;      /* result */
    HWND   wh;     /* parent window handle */
    int    lfn;    /* logical number for parent window */
    winptr win;    /* parent window data */
    wigptr wp;     /* widget pointer */
    char   s[100]; /* buffer for edit string */
    int    v;      /* value from edit control */
    int    err;    /* error parsing value */

/*int i;*/

/*;fprintf(stderr, "wndprocnum: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);*/

    /* We need to find out who we are talking to. */
    lockmain(); /* start exclusive access */
    wh = GetParent(hwnd); /* get the widget parent handle */
    lfn = hwn2lfn(wh); /* get the logical window number */
    win = lfn2win(lfn); /* index window from logical number */
    wp = fndwighan(win, hwnd); /* find the widget from that */
    unlockmain(); /* end exclusive access */
    r = 0; /* set no error */
    /* check its a character */
    if (imsg == WM_CHAR) {

        if (wp->enb) { /* is the widget enabled ? */

            /* check control is receiving a carriage return */
            if (wparam == '\r') {

                r = GetWindowText(wp->han2, s, 100); /* get contents of edit */
                v = intv(s, &err); /* get value */
                /* Send edit sends cr message to parent window, with widget logical
                   number embedded as wparam. */
                if (!err && v >= wp->low && v <= wp->high)
                    putmsg(wh, UM_NUMCR, wp->id, v); /* send select message */
                else
                    /* Send the message on to its owner, this will ring the bell in
                       Windows XP. */
                    r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

            } else {

                /* Check valid numerical character, with backspace control. If not,
                   replace with carriage return. This will cause the control to emit
                   an error, a bell in Windows XP. */
                if (!((wparam >= '0' && wparam <= '9') || wparam == '+' ||
                      wparam == '-' || wparam == '\b'))
                    wparam = '\r';
                r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

            }

        }

    } else
        /* send the message on to its owner */
        r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

    return (r); /* return result */

}

/*******************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated and returned.

*******************************************************************************/

static void inumselboxsizg(winptr win, int l, int u,  int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    /* get size of text */
    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    if (u > 9) b = GetTextExtentPoint32(dc, "00", 2, &sz); /* get sizing */
    else b = GetTextExtentPoint32(dc, "0", 1, &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* width of text, plus up/down arrows, and border and divider lines */
    *w = sz.cx+GetSystemMetrics(SM_CXVSCROLL)+6;
    *h = sz.cy+2; /* height of text plus border lines */

}

static void inumselboxsiz(winptr win, int l, int u, int* w, int* h)

{

    inumselboxsizg(win, l, u, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void ami_numselboxsizg(FILE* f, int l, int u, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselboxsizg(win, l, u, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_numselboxsiz(FILE* f, int l, int u, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselboxsiz(win, l, u, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create number selector

Creates an up/down control for numeric selection.

*******************************************************************************/

static void inumselboxg(winptr win, int x1, int y1, int x2, int y2, int l, int u,
                 int id)

{

    imptr  ip;  /* intratask message pointer */
    wigptr wp;  /* widget pointer */
    BOOL   br;  /* result */
    int    udw; /* up/down control width */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* search previous definition with same id */
    if (fndwig(win, id)) error(ewigdup); /* duplicate widget id */
    /* Number select is a composite control, and it will send messages
      immediately after creation, as the different components talk to each
      other. Because of this, we must create a widget entry first, even if
      it is incomplete. */
    getwig(win, &wp); /* get new widget */
    wp->id = id; /* place button id */
    wp->typ = wtnumselbox; /* place type */
    wp->han = 0; /* clear handles */
    wp->han2 = 0;
    wp->low = l; /* place limits */
    wp->high = u;
    /* get width of up/down control (same as scroll arrow) */
    udw = GetSystemMetrics(SM_CXHSCROLL);
    /* If the width is not enough for the control to appear, force it. */
    if (x2-x1+1 < udw) x2 = x1+udw-1;
    getitm(&ip); /* get a im pointer */
    ip->im = imupdown; /* set is up/down control */
    ip->udflg = WS_CHILD | WS_VISIBLE | WS_BORDER | UDS_SETBUDDYINT;
    ip->udx = x1-1;
    ip->udy = y1-1;
    ip->udcx = x2-x1+1;
    ip->udcy = y2-y1+1;
    ip->udpar = win->winhan;
    ip->udid = id;
    ip->udinst = GetModuleHandle(NULL);
    ip->udup = u;
    ip->udlow = l;
    ip->udpos = l;
    br = PostMessage(dispwin, UM_IM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imupdown, &ip); /* wait for the return */
    wp->han = ip->udhan; /* place control handle */
    wp->han2 = ip->udbuddy; /* place buddy handle */
    putitm(ip); /* release im */
    /* place our subclass handler for the edit control */
    wp->wprc = (WNDPROC)GetWindowLongPtr(wp->han2, GWL_WNDPROC);
    if (!wp->wprc) winerr(); /* process windows error */
    r = SetWindowLong(wp->han2, GWL_WNDPROC, (LONG)wndprocnum);
    if (!r) winerr(); /* process windows error */

}

static void inumselbox(winptr win, int x1, int y1, int x2, int y2, int l, int u,
                int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = x2*win->charspace;
    y2 = y2*win->linespace;
    inumselboxg(win, x1, y1, x2, y2, l, u, id); /* create button graphical */

}

void ami_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselboxg(win, x1, y1, x2, y2, l, u, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselbox(win, x1, y1, x2, y2, l, u, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Control window function for edit box

This message handler is to allow us to capture the carriage return from an
edit box, and turn that into a message.

*******************************************************************************/

static int wndprocedit(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam)

{

    LRESULT r;   /* result */
    HWND    wh;  /* parent window handle */
    int     lfn; /* logical number for parent window */
    winptr  win; /* parent window data */
    wigptr  wp;  /* widget pointer */

//dbg_printf(dlinfo, "Message: "); prtmsgu(hwnd, imsg, wparam, lparam);
    /* We need to find out who we are talking to. */
    wh = GetParent(hwnd); /* get the widget parent handle */
    lfn = hwn2lfn(wh); /* get the logical window number */
    win = lfn2win(lfn); /* index window from logical number */
    wp = fndwighan(win, hwnd); /* find the widget from that */
    /* check control is receiving a carriage return */
    if (imsg == WM_CHAR && wparam == '\r') {

        /* Send edit sends cr message to parent window, with widget logical
           number embedded as wparam. */
        putmsg(wh, UM_EDITCR, wp->id, 0);
        SetFocus(wh); /* send keyboard focus back to parent */

    } else if (imsg == CB_GETDROPPEDSTATE) {

        /* drop edit box signals done */
        putmsg(wh, UM_EDITCR, wp->id, 0);
        SetFocus(wh); /* send keyboard focus back to parent */

    } else
        /* send the message on to its owner */
        r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

    return (r); /* return result */

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

static void ieditboxsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* add borders to size */
    *w = sz.cx+4;
    *h = sz.cy+4;

}

static void ieditboxsiz(winptr win, char* s, int* w, int* h)

{

    ieditboxsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void ami_editboxsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditboxsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_editboxsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditboxsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create edit box

Creates single line edit box

*******************************************************************************/

static void ieditboxg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget pointer */
    LONG   r;

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wteditbox, 0, &wp);
    /* get the windows internal procedure for subclassing */
    wp->wprc = (WNDPROC)GetWindowLongPtr(wp->han, GWL_WNDPROC);
    if (!wp->wprc) winerr(); /* process windows error */
    r = SetWindowLong(wp->han, GWL_WNDPROC, (LONG)wndprocedit);
    if (!r) winerr(); /* process windows error */

}

static void ieditbox(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    ieditboxg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void ami_editboxg(FILE* f,  int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditboxg(win, x1,y1, x2,y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_editbox(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditbox(win, x1,y1, x2,y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

static void iprogbarsizg(winptr win, int* w, int* h)

{

    /* Progress bars are arbitrary for sizing. We choose a size that allows for
       20 bar elements. Note that the size of the blocks in a Windows progress
       bar are ratioed to the height. */
    *w = 20*14+2;
    *h = 20+2;

}

static void iprogbarsiz(winptr win, int* w, int* h)

{

    iprogbarsizg(win, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

};

void ami_progbarsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_progbarsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create progress bar

Creates a progress bar.

*******************************************************************************/

static void iprogbarg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* create the progress bar */
    widget(win, x1, y1, x2, y2, "", id, wtprogressbar, 0, &wp);
    /* use 0..INT_MAX ratio */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, PBM_SETRANGE32, 0, INT_MAX);
    lockmain(); /* start exclusive access */

}

static void iprogbar(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iprogbarg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void ami_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_progbar(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbar(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to INT_MAX.

*******************************************************************************/

static void iprogbarpos(winptr win, int id, int pos)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;

   if (!win->visible) winvis(win); /* make sure we are displayed */
   if (pos < 0) error(eprgpos); /* bad position */
   wp = fndwig(win, id); /* find widget */
   if (!wp) error(ewignf); /* not found */
   /* set the range */
   unlockmain(); /* end exclusive access */
   r = SendMessage(wp->han, PBM_SETPOS, pos, 0);
   lockmain(); /* start exclusive access */

}

void ami_progbarpos(FILE* f, int id, int pos)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarpos(win, id, pos); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard list box size

Finds the minimum size for an list box. Given a string list, the minimum size
of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

*******************************************************************************/

static void ilistboxsizg(winptr win, ami_strptr sp, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */
    int  mw; /* max width */

    *w = 4; /* set minimum overhead */
    *h = 2;
    while (sp) { /* traverse string list */

        dc = GetWindowDC(NULL); /* get screen dc */
        if (!dc) winerr(); /* process windows error */
        b = GetTextExtentPoint32(dc, sp->str, strlen(sp->str), &sz); /* get sizing */
        if (!b) winerr(); /* process windows error */
        /* add borders to size */
        mw = sz.cx+4;
        if (mw > *w) *w = mw; /* set new maximum */
        *h = *h+sz.cy;
        sp = sp->next; /* next string */

    }

}

static void ilistboxsiz(winptr win, ami_strptr sp, int* w, int* h)

{

    ilistboxsizg(win, sp, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void ami_listboxsizg(FILE* f, ami_strptr sp, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistboxsizg(win, sp, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_listboxsiz(FILE* f, ami_strptr sp, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistboxsiz(win, sp, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

*******************************************************************************/

static void ilistboxg(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    wigptr wp; /* widget pointer */
    LRESULT r; /* return value */

    if (!win->visible)  winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtlistbox, 0, &wp);
    while (sp) { /* add strings to list */

        unlockmain(); /* end exclusive access */
        r = SendMessage(wp->han, LB_ADDSTRING, 0, (LPARAM)sp->str); /* add string */
        lockmain(); /* start exclusive access */
        if (r == -1) error(estrspc); /* out of string space */
        sp = sp->next; /* next string */

    }

}

static void ilistbox(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    ilistboxg(win, x1, y1, x2, y2, sp, id); /* create button graphical */

}

void ami_listboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistboxg(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_listbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistbox(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find size for single line of text

Find the pixel size in x for the given line of text.

*******************************************************************************/

static void getsizlin(char* s, LPSIZE sz)

{

    HDC dc;

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), sz); /* get sizing */
    if (!b) winerr(); /* process windows error */

}

/*******************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

static void idropboxsizg(winptr win, ami_strptr sp, int* cw, int* ch,
                         int* ow, int* oh)

{

    /* I can't find a reasonable system metrics version of the drop arrow
       demensions, so they are hardcoded here. */
    const int darrowx = 17;
    const int darrowy = 20;

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    /* calculate first line */
    getsizlin(sp->str, &sz); /* find sizing for line */
    /* Find size of string x, drop arrow width, box edges, and add fudge factor
      to space text out. */
    *cw = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
    *ow = *cw; /* open is the same */
    /* drop arrow height+shadow overhead+drop box bounding */
    *oh = darrowy+GetSystemMetrics(SM_CYEDGE)*2+2;
    /* drop arrow height+shadow overhead */
    *ch = darrowy+GetSystemMetrics(SM_CYEDGE)*2;
    /* add all lines to drop box section */
    while (sp) { /* traverse string list */

        getsizlin(sp->str, &sz); /* find sizing for this line */
        /* find open width on this string only */
        *ow = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
        if (*ow > *cw) *cw = *ow; /* larger than closed width, set new max */
        *oh = *oh+sz.cy; /* add to open height */
        sp = sp->next; /* next string */

    }
    *ow = *cw; /* set maximum open width */

}

static void idropboxsiz(winptr win, ami_strptr sp, int* cw, int* ch,
                        int* ow, int* oh)

{

    idropboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/win->charspace+1;
    *ch = (*ch-1)/win->linespace+1;
    *ow = (*ow-1)/win->charspace+1;
    *oh = (*oh-1)/win->linespace+1;

}

void ami_dropboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_dropboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropboxsiz(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

*******************************************************************************/

static void idropboxg(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp,
                      int id)

{

    wigptr wp;  /* widget pointer */
    ami_strptr sp1;  /* string pointer */
    LRESULT r; /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtdropbox, 0, &wp);
    sp1 = sp; /* index top of string list */
    while (sp1) { /* add strings to list */

        unlockmain(); /* end exclusive access */
        r = SendMessage(wp->han, CB_ADDSTRING, 0, (LPARAM)sp1->str); /* add string */
        lockmain(); /* start exclusive access */
        if (r == -1) error(estrspc); /* out of string space */
        sp1 = sp1->next; /* next string */

    }
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, CB_SETCURSEL, 0, 0);
    lockmain(); /* start exclusive access */
    if (r == -1)  error(esystem); /* should not happen */

}

static void idropbox(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp,
                     int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    idropboxg(win, x1, y1, x2, y2, sp, id); /* create button graphical */

}

void ami_dropboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropboxg(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_dropbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropbox(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard drop edit box size

Finds the minimum size for a drop edit box. Given the face string, the minimum
size of a drop edit box is calculated and returned, for both the "open" and
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

static void idropeditboxsizg(winptr win, ami_strptr sp, int* cw, int* ch,
                             int* ow, int* oh)

{

    /* I can"t find a reasonable system metrics version of the drop arrow
       demensions, so they are hardcoded here. */
    const int darrowx = 17;
    const int darrowy = 20;

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    /* calculate first line */
    getsizlin(sp->str, &sz); /* find sizing for line */
    /* Find size of string x, drop arrow width, box edges, and add fudge factor
       to space text out. */
//    cw = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
    *ow = *cw; /* open is the same */
    /* drop arrow height+shadow overhead+drop box bounding */
    *oh = darrowy+GetSystemMetrics(SM_CYEDGE)*2+2;
    /* drop arrow height+shadow overhead */
//    ch = darrowy+GetSystemMetrics(SM_CYEDGE)*2;
    /* add all lines to drop box section */
    while (sp) { /* traverse string list */

        getsizlin(sp->str, &sz); /* find sizing for this line */
        /* find open width on this string only */
        *ow = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
        if (*ow > *cw) *cw = *ow; /* larger than closed width, set new max */
        *oh = *oh+sz.cy; /* add to open height */
        sp = sp->next; /* next string */

    }
    *ow = *cw; /* set maximum open width */

}

static void idropeditboxsiz(winptr win, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    idropeditboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/win->charspace+1;
    *ch = (*ch-1)/win->linespace+1;
    *ow = (*ow-1)/win->charspace+1;
    *oh = (*oh-1)/win->linespace+1;

}

void ami_dropeditboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_dropeditboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditboxsiz(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

static void idropeditboxg(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp,
                   int id)

{

    wigptr    wp;  /* widget pointer */
    ami_strptr sp1; /* string pointer */
    LRESULT   r;   /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtdropeditbox, 0, &wp);
    /* get the windows internal procedure for subclassing */
    wp->wprc = (WNDPROC)GetWindowLongPtr(wp->han, GWL_WNDPROC);
    if (!wp->wprc) winerr(); /* process windows error */
    r = SetWindowLong(wp->han, GWL_WNDPROC, (LONG)wndprocedit);
    if (!r) winerr(); /* process windows error */
    sp1 = sp; /* index top of string list */
    while (sp1) { /* add strings to list */

        unlockmain(); /* end exclusive access */
        r = SendMessage(wp->han, CB_ADDSTRING, 0, (LPARAM)sp1->str); /* add string */
        lockmain(); /* start exclusive access */
        if (r == -1) error(estrspc); /* out of string space */
        sp1 = sp1->next; /* next string */

    }

}

static void idropeditbox(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    idropeditboxg(win, x1, y1, x2, y2, sp, id); /* create button graphical */

}

void ami_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditboxg(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditbox(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

*******************************************************************************/

static void islidehorizsizg(winptr win, int* w, int* h)

{

    /* The width is that of an average slider. The height is what is needed to
       present the slider, tick marks, and 2 pixels of spacing around it. */
    *w = 200;
    *h = 32;

}

static void islidehorizsiz(winptr win, int* w, int* h)

{

    islidehorizsizg(win, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void ami_slidehorizsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehorizsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_slidehorizsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehorizsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

static void islidehorizg(winptr win, int x1, int y1, int x2, int y2, int mark, int id)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (!mark) /* tick marks enabled */
        widget(win, x1, y1, x2, y2, "", id, wtslidehoriz, TBS_NOTICKS, &wp);
    else /* tick marks enabled */
        widget(win, x1, y1, x2, y2, "", id, wtslidehoriz, 0, &wp);
    /* set tickmark frequency */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, TBM_SETTICFREQ, mark, 0);
    lockmain(); /* start exclusive access */

}

static void islidehoriz(winptr win, int x1, int y1, int x2, int y2, int mark, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    islidehorizg(win, x1, y1, x2, y2, mark, id); /* create button graphical */

}

void ami_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehorizg(win, x1, y1, x2, y2, mark, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehoriz(win, x1, y1, x2, y2, mark, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated and returned.

*******************************************************************************/

static void islidevertsizg(winptr win, int* w, int* h)

{

    /* The height is that of an average slider. The width is what is needed to
       present the slider, tick marks, and 2 pixels of spacing around it. */
    *w = 32;
    *h = 200;

}

static void islidevertsiz(winptr win, int* w, int* h)

{

    islidevertsizg(win, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void ami_slidevertsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevertsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_slidevertsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevertsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

static void islidevertg(winptr win, int x1, int y1, int x2, int y2, int mark,
                        int id)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (!mark) /* tick marks off */
        widget(win, x1, y1, x2, y2, "", id, wtslidevert, TBS_NOTICKS, &wp);
    else /* tick marks enabled */
        widget(win, x1, y1, x2, y2, "", id, wtslidevert, 0, &wp);
    /* set tickmark frequency */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, TBM_SETTICFREQ, mark, 0);
    lockmain(); /* start exclusive access */

}

static void islidevert(winptr win, int x1, int y1, int x2, int y2, int mark, int id)

{

   /* form graphical from character coordinates */
   x1 = (x1-1)*win->charspace+1;
   y1 = (y1-1)*win->linespace+1;
   x2 = (x2)*win->charspace;
   y2 = (y2)*win->linespace;
   islidevertg(win, x1, y1, x2, y2, mark, id); /* create button graphical */

}

void ami_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevertg(win, x1, y1, x2, y2, mark, id);
    unlockmain(); /* end exclusive access */

}

void ami_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevert(win, x1, y1, x2, y2, mark, id);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create and distroy useless widget

This appears to be a windows bug. When tab bars are created, they allow
themselves to be overritten by the parent. This only occurs on tab bars.
The workaround is to create a distroy a widget right after creating the
tab bar, since only the last widget created has this problem.

Bug: this is not getting deleted. I fixed it temporarily by making it
invisible.

*******************************************************************************/

static void uselesswidget(winptr win)

{

    imptr ip; /* intratask message pointer */

    getitm(&ip); /* get a im pointer */
    ip->im = imwidget; /* set type is widget */
    ip->wigcls = str("static");
    ip->wigtxt = str("");
    ip->wigflg = WS_CHILD /*or WS_VISIBLE*/;
    ip->wigx = 50;
    ip->wigy = 50;
    ip->wigw = 50;
    ip->wigh = 50;
    ip->wigpar = win->winhan;
    ip->wigid = 0;
    ip->wigmod = GetModuleHandle(NULL);
    /* order widget to start */
    b = PostMessage(dispwin, UM_IM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for widget start, this also keeps our window going. */
    waitim(imwidget, &ip); /* wait for the return */
    kilwin(ip->wigwin); /* kill widget */
    ifree(ip->wigcls); /* release class string */
    ifree(ip->wigtxt); /* release face text string */
    putitm(ip); /* release im */

}

/*******************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

static void itabbarsizg(winptr win, ami_tabori tor, int cw, int ch, int* w, int* h,
                        int* ox, int* oy)

{

    if (tor == ami_toright || tor == ami_toleft)  { /* vertical bar */

        *w = 32; /* set minimum width */
        *h = 2+20*2; /* set minimum height */
        *w = *w+cw; /* add client space to width */
        if (ch+4 > *h) *h = ch+4; /* set to client if greater */
        if (tor == ami_toleft) {

            *ox = 28; /* set offsets */
            *oy = 4;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    } else { /* horizontal bar */

        *w = 2+20*2; /* set minimum width, edges, arrows */
        *h = 32; /* set minimum height */
        if (cw+4 > *w) *w = cw+4; /* set to client if greater */
        *h = *h+ch; /* add client space to height */
        if (tor == ami_totop)  {

            *ox = 4; /* set offsets */
            *oy = 28;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    }

}

static void itabbarsiz(winptr win, ami_tabori tor, int cw, int ch, int* w, int* h,
                       int* ox, int* oy)

{

    int gw, gh, gox, goy;

    /* convert client sizes to graphical */
    cw = cw*win->charspace;
    ch = ch*win->linespace;
    itabbarsizg(win, tor, cw, ch, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *w = (gw-1) / win->charspace+1;
    *h = (gh-1) / win->linespace+1;
    *ox = (gox-1) / win->charspace+1;
    *oy = (goy-1) / win->linespace+1;
    /* must make sure that client dosen"t intrude on edges */
    if (gw-gox-4%win->charspace) *w = *w+1;
    if (gh-goy-4%win->charspace) *h = *h+1;

}

void ami_tabbarsizg(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h,
                int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarsizg(win, tor, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_tabbarsiz(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarsiz(win, tor, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

*******************************************************************************/

static void itabbarclientg(winptr win, ami_tabori tor, int w, int h, int* cw, int* ch,
                           int* ox, int* oy)

{

    if (tor == ami_toright || tor == ami_toleft)  { /* vertical bar */

        /* Find client height and width from total height and width minus
         tabbar overhead. */
        *cw = w-32;
        *ch = h-8;
        if (tor == ami_toleft) {

            *ox = 28; /* set offsets */
            *oy = 4;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    } else { /* horizontal bar */

        /* Find client height and width from total height and width minus
           tabbar overhead. */
        *cw = w-8;
        *ch = h-32;
        if (tor == ami_totop) {

            *ox = 4; /* set offsets */
            *oy = 28;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    }

}

static void itabbarclient(winptr win, ami_tabori tor, int w, int h, int* cw, int* ch,
                          int* ox, int* oy)

{

    int gw, gh, gox, goy;

    /* convert sizes to graphical */
    w = w*win->charspace;
    h = h*win->linespace;
    itabbarsizg(win, tor, w, h, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *cw = (gw-1)/win->charspace+1;
    *ch = (gh-1)/win->linespace+1;
    *ox = (gox-1)/win->charspace+1;
    *oy = (goy-1)/win->linespace+1;
    /* must make sure that client dosen"t intrude on edges */
    if (gw-gox-4%win->charspace) w = w+1;
    if (gh-goy-4%win->charspace) h = h+1;

}

void ami_tabbarclientg(FILE* f, ami_tabori tor, int w, int h, int*  cw, int* ch,
                   int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarclientg(win, tor, w, h, cw, ch, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

void ami_tabbarclient(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                  int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarclient(win, tor, w, h, cw, ch, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

*******************************************************************************/

static void itabbarg(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp,
                     ami_tabori tor, int id)

{

    wigptr  wp;  /* widget pointer */
    int     inx; /* index for tabs */
    TCITEM  tcr; /* tab attributes record */
    int     i;   /* idnex for string */
    int     m;   /* maximum length of string */
    int     fl;  /* flags */
    LRESULT r;

    if (!win->visible) winvis(win); /* make sure we are displayed */
    fl = 0; /* clear parameter flags */
    if (tor == ami_toright || tor == ami_toleft) fl |= TCS_VERTICAL;
    if (tor == ami_toright) fl |= TCS_RIGHT;
    if (tor == ami_tobottom) fl |= TCS_BOTTOM;
    widget(win, x1, y1, x2, y2, "", id, wttabbar, fl, &wp);
    inx = 0; /* set index */
    while (sp) { /* add strings to list */

        /* create a string buffer with space for terminating zero */
        tcr.mask = TCIF_TEXT; /* set record contains text label */
        tcr.dwState = 0; /* clear state */
        tcr.dwStateMask = 0; /* clear state mask */
        tcr.pszText = sp->str; /* place string */
        tcr.iImage = -1; /* no image */
        tcr.lParam = 0; /* no parameter */
        unlockmain(); /* end exclusive access */
        /* add string */
        r = SendMessage(wp->han, TCM_INSERTITEM, (WPARAM)inx, (LPARAM)&tcr);
        lockmain(); /* start exclusive access */
        if (r == -1)  error(etabbar); /* can"t create tab */
        sp = sp->next; /* next string */
        inx++; /* next index */

    }
    uselesswidget(win); /* stop overwrite bug in Windows */

}

static void itabbar(winptr win, int x1, int y1, int x2, int y2, ami_strptr sp,
                    ami_tabori tor, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    itabbarg(win, x1, y1, x2, y2, sp, tor, id); /* create button graphical */

}

void ami_tabbarg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, ami_tabori tor,
             int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarg(win, x1, y1, x2, y2, sp, tor, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void ami_tabbar(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, ami_tabori tor,
            int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbar(win, x1, y1, x2, y2, sp, tor, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

static void itabsel(winptr win, int id, int tn)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;

    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (tn < 1) error(etabsel); /* bad tab select */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    /* set the range */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, TCM_SETCURSEL, tn-1, 0);
    lockmain(); /* start exclusive access */

}

void ami_tabsel(FILE* f, int id, int tn)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabsel(win, id, tn); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

*******************************************************************************/

void ami_alert(char* title, char* message)

{

    imptr ip; /* intratask message pointer */
    BOOL  b;  /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imalert; /* set is alert */
    ip->alttit = title; /* copy strings */
    ip->altmsg = message;
    b = PostMessage(dialogwin, UM_IM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    waitim(imalert, &ip); /* wait for the return */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose color dialog

Presents the choose color dialog,  returns the resulting color.

Bug: does not take the input color as the default.

*******************************************************************************/

void ami_querycolor(int* r, int* g, int* b)

{

    imptr ip; /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqcolor; /* set is color query */
    ip->clrred = *r; /* set colors */
    ip->clrgreen = *g;
    ip->clrblue = *b;
    br = PostMessage(dialogwin, UM_IM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqcolor, &ip); /* wait for the return */
    *r = ip->clrred; /* set new colors */
    *g = ip->clrgreen;
    *b = ip->clrblue;
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose file dialog for open

Presents the choose open file dialog. The input string is used to fill the filename
to start, which could be empty. Returns the resulting file string in the same
buffer.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, a null string will be returned.

It is an error if the passed string buffer is not big enough to contain the
result.

*******************************************************************************/

void ami_queryopen(char* s, int sl)

{

    imptr ip; /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqopen; /* set is open file query */
    ip->opnfil = str(s); /* copy input string */
    br = PostMessage(dialogwin, UM_IM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqopen, &ip); /* wait for the return */
    if (strlen(ip->opnfil) > sl) error(estrtl); /* string overflow */
    strcpy(s, ip->opnfil); /* set output string */
    ifree(ip->opnfil); /* free the temp string */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose file dialog for save

Presents the choose save file dialog. The input string is used to fill the filename
to start, which could be empty. Returns the resulting file string in the same
buffer.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, a null string will be returned.

It is an error if the passed string buffer is not big enough to contain the
result.

It is an error if the passed string buffer is not big enough to contain the
result.

*******************************************************************************/

void ami_querysave(char* s, int sl)

{

    imptr ip; /* intratask message pointer */
    BOOL br;  /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqsave; /* set is open file query */
    ip->opnfil = str(s); /* set input string */
    br = PostMessage(dialogwin, UM_IM, (WPARAM)ip, 0);
    if (!br)  winerr(); /* process windows error */
    waitim(imqsave, &ip); /* wait for the return */
    if (strlen(ip->savfil) > sl) error(estrtl); /* string overflow */
    strcpy(s, ip->savfil); /* set output string */
    ifree(ip->savfil); /* free the temp string */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose find text dialog

Presents the choose find text dialog,  returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: should return null string on cancel. Unlike other dialogs, windows
provides no indication of if the cancel button was pushed. To do this, we
would need to hook (or subclass) the find dialog.

After note: tried hooking the window. The issue is that the cancel button is
just a simple button that gets pressed. Trying to rely on the button id
sounds very system dep}ent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

*******************************************************************************/

void ami_queryfind(char* s, int sl, int* opt)

{

    imptr ip;   /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqfind; /* set is find query */
    ip->fndstr = str(s); /* set input string */
    ip->fndopt = *opt; /* set options */
    br = PostMessage(dialogwin, UM_IM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqfind, &ip); /* wait for the return */
    if (strlen(ip->fndstr) > sl) error(estrtl); /* string overflow */
    strcpy(s, ip->fndstr); /* set output string */
    ifree(ip->fndstr); /* free the temp string */
    *opt = ip->fndopt; /* set output options */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog,  returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

*******************************************************************************/

void ami_queryfindrep(char* s, int sl, char* r, int rl, int* opt)

{

    imptr ip; /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqfindrep; /* set is find/replace query */
    ip->fnrsch = str(s); /* set input find string */
    ip->fnrrep = str(r); /* set input replace string */
    ip->fnropt = *opt; /* set options */
    br = PostMessage(dialogwin, UM_IM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqfindrep, &ip); /* wait for the return */
    if (strlen(ip->fnrsch) > sl) error(estrtl); /* string overflow */
    strcpy(s, ip->fnrsch); /* set output find string */
    ifree(ip->fnrsch); /* free the temp string */
    if (strlen(ip->fnrrep) > rl) error(estrtl); /* string overflow */
    strcpy(r, ip->fnrrep);
    ifree(ip->fnrrep); /* free the temp string */
    *opt = ip->fnropt; /* set output options */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose font dialog

Presents the choose font dialog,  returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made,  updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

*******************************************************************************/

/* find font number in fonts list */

static int fndfntnum(winptr win, char* fns)

{

    fontptr fp;     /* pointer for fonts list */
    int     fc, ff; /* font counters */

    fp = fntlst; /* index top of fonts */
    fc = 1; /* set 1st font */
    ff = 0; /* set no font found */
    while (fp) { /* traverse */

        if (comps(fns, fp->fn)) ff = fc; /* found */
        fp = fp->next; /* next entry */
        fc++; /* count */

    }
    /* The font string should match one from the list, since that list was itself
       formed from the system font list. */
    if (!ff)  error(esystem); /* should have found matching font */

    return (ff); /* return font */

}

static void iqueryfont(winptr win, int* fc, int* s, int* fr, int* fg, int* fb,
                       int* br, int* bg, int* bb, int* effect)

{

    imptr ip;               /* intratask message pointer */
    BOOL  b;                /* result */
    char  fns[LF_FACESIZE]; /* name of font */

    getitm(&ip); /* get a im pointer */
    ip->im = imqfont; /* set is font query */
    ifontnam(win, *fc, fns, LF_FACESIZE); /* get the name of the font */
    ip->fntstr = fns; /* place in record */
    ip->fnteff = *effect; /* copy effects */
    ip->fntfr = *fr; /* copy colors */
    ip->fntfg = *fg;
    ip->fntfb = *fb;
    ip->fntbr = *br;
    ip->fntbg = *bg;
    ip->fntbb = *bb;
    ip->fntsiz = *s; /* place font size */
    /* send request */
    b = PostMessage(dialogwin, UM_IM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    waitim(imqfont, &ip); /* wait for the return */
    /* pull back the output parameters */
    *fc = fndfntnum(win, ip->fntstr); /* find font from list */
    *effect = ip->fnteff; /* effects */
    *fr = ip->fntfr; /* colors */
    *fg = ip->fntfg;
    *fb = ip->fntfb;
    *br = ip->fntbr;
    *bg = ip->fntbg;
    *bb = ip->fntbb;
    *s = ip->fntsiz; /* font size */
    putitm(ip); /* release im */

}

void ami_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb,
               int* br, int* bg, int* bb, int* effect)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iqueryfont(win, fc, s, fr, fg, fb, br, bg, bb, effect); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Window procedure for display thread

This is the window handler callback for all display windows.

*******************************************************************************/

static LRESULT CALLBACK wndproc(HWND hwnd, UINT imsg, WPARAM wparam,
                                LPARAM lparam)

{

    LRESULT r;   /* result holders */
    BOOL    b;
    int     ofn; /* output file handle */
    winptr  win; /* pointer to windows structure */
    imptr   ip;  /* intratask message pointer */
    int     udw; /* up/down control width */
    RECT    cr;  /* client rectangle */

    /* dump messages (diagnostic) */
    if (dmpmsg) {

        dbg_printf(dlinfo, "Message: "); prtmsgu(hwnd, imsg, wparam, lparam);

    }

    if (imsg == WM_CREATE) {

        r = 0;

    } else if (imsg == WM_PAINT) {

        lockmain(); /* start exclusive access */
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(hwnd);
        if (ofn) { /* there is a window */

            win = lfn2win(ofn); /* index window from output file */
            if (win->bufmod) restore(win, FALSE); /* perform selective update */
            else { /* main task will handle it */

                /* get update region */
                b = GetUpdateRect(hwnd, &cr, FALSE);
                /* validate it so windows won"t send multiple notifications */
                b = ValidateRgn(hwnd, NULL); /* validate region */
                /* Pack the update region in the message. This limits the update
                   region to 16 bit coordinates. We need an im to fix this. */
                wparam = cr.left*0x10000+cr.top;
                lparam = cr.right*0x10000+cr.bottom;
                unlockmain(); /* end exclusive access */
                putmsg(hwnd, imsg, wparam, lparam); /* send message up */
                lockmain(); /* start exclusive access */

            }
            r = 0;

        } else r = DefWindowProc(hwnd, imsg, wparam, lparam);
        unlockmain(); /* end exclusive access */
        r = 0;

    } else if (imsg == WM_SETFOCUS)  {

        lockmain(); /* start exclusive access */
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(hwnd);
        if (ofn) { /* there is a window */

            win = lfn2win(ofn); /* index window from output file */
            /* activate caret */
            b = CreateCaret(win->winhan, 0, win->curspace, 3);
            /* set caret (text cursor) position at bottom of bounding box */
            b = SetCaretPos(win->screens[win->curdsp-1]->curxg-1,
                            win->screens[win->curdsp-1]->curyg-1+win->linespace-3);
            win->focus = TRUE; /* set screen in focus */
            curon(win); /* show the cursor */

        }
        unlockmain(); /* end exclusive access */
        putmsg(hwnd, imsg, wparam, lparam); /* copy to main thread */
        r = 0;

    } else if (imsg == WM_KILLFOCUS) {

        lockmain(); /* start exclusive access */
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(hwnd);
        if (ofn) { /* there is a window */

           win = lfn2win(ofn); /* index window from output file */
           win->focus = FALSE; /* set screen not in focus */
           curoff(win); /* hide the cursor */
           b = DestroyCaret(); /* remove text cursor */

        }
        unlockmain(); /* end exclusive access */
        putmsg(hwnd, imsg, wparam, lparam); /* copy to main thread */
        r = 0;

    } else if (imsg == UM_MAKWIN) { /* create standard window */

        /* create the window */
        stdwinwin = CreateWindow("StdWin", pgmnam, stdwinflg,
                                  stdwinx, stdwiny, stdwinw, stdwinh,
                                  stdwinpar, 0, GetModuleHandle(NULL), NULL);

        stdwinj1c = FALSE; /* set no joysticks */
        stdwinj2c = FALSE;
        if (joyenb) {

            r = joySetCapture(stdwinwin, JOYSTICKID1, 33, FALSE);
            stdwinj1c = r == 0; /* set joystick 1 was captured */
            r = joySetCapture(stdwinwin, JOYSTICKID2, 33, FALSE);
            stdwinj2c = r == 0; /* set joystick 1 was captured */

        }

        /* signal we started window */
        iputmsg(0, UM_WINSTR, 0, 0);
        r = 0;

    } else if (imsg == UM_CLSWIN) { /* close standard window */

        b = DestroyWindow(stdwinwin); /* remove window from screen */

        /* signal we closed window */
        iputmsg(0, UM_WINCLS, 0, 0);
        r = 0;

    } else if (imsg == WM_ERASEBKGND) {

        /* Letting windows erase the background is not good, because it flashes, and
           is redundant in any case, because we handle that. */
        r = 1; /* say we are handling the erase */

    } else if (imsg == WM_CLOSE) {

        /* we handle our own window close, so don't pass this on */
        putmsg(0, imsg, wparam, lparam);
        r = 0;

    } else if (imsg == WM_DESTROY) {

        /* here's a problem. Posting quit causes the thread/process to terminate,
           not just the window. MSDN says to quit only the main window, but what
           is the main window here ? We send our terminate message based on
           wm_quit, and the window, even the main, does get closed. The postquit
           message appears to only be for closing down the program, which our
           program does by exiting. */
        /* postquitmessage(0); */
        r = 0;

    } else if (imsg == WM_LBUTTONDOWN || imsg == WM_MBUTTONDOWN ||
               imsg == WM_RBUTTONDOWN) {

        /* Windows allows child windows to capture the focus, but they don"t
           give it up (its a feature). We get around this by  returning the
           focus back to any window that is clicked by the mouse, but does
           not have the focus. */
        SetFocus(hwnd);
        putmsg(hwnd, imsg, wparam, lparam);
        r = DefWindowProc(hwnd, imsg, wparam, lparam);

    } else if (imsg == UM_IM) { /* intratask message */

        ip = (imptr)wparam; /* get im pointer */
        switch (ip->im) { /* im type */

            case imupdown: /* create up/down control */
                ip->udbuddy =
                    CreateWindowEx(WS_EX_LEFT | WS_EX_CLIENTEDGE,
                                      WC_EDIT,
                                      NULL,
                                      WS_CHILDWINDOW | WS_VISIBLE | WS_BORDER |
                                      ES_NUMBER | ES_LEFT,
                                      ip->udx, ip->udy, ip->udcx, ip->udcy,
                                      ip->udpar, (HMENU)ip->udid, ip->udinst, NULL);
                ip->udhan = CreateWindowEx(WS_EX_LEFT | WS_EX_LTRREADING,
                    UPDOWN_CLASS,
                    NULL,
                    WS_CHILDWINDOW | WS_VISIBLE | WS_BORDER |
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_HOTTRACK,
                    0, 0, 0, 0,
                    /*ip->udx, ip->udy, ip->udcx-udw-1, ip->udcy,*/
                    ip->udpar, (HMENU)ip->udid, ip->udinst, NULL);
                /* signal complete */
                iputmsg(0, UM_IM, wparam, 0);
                break;

            case imwidget: /* create widget */
                /* start widget window */
                ip->wigwin = CreateWindow(ip->wigcls, ip->wigtxt, ip->wigflg,
                                          ip->wigx, ip->wigy, ip->wigw,
                                          ip->wigh, ip->wigpar, (HMENU)ip->wigid,
                                          ip->wigmod, NULL);
                /* signal we started widget */
                iputmsg(0, UM_IM, wparam, 0);
                break;

        }
        r = 0; /* clear result */

    } else { /* default handling */

        /* Copy messages we are interested in to the main thread. By keeping the
           messages passed down to only the interesting ones, we help prevent
           queue "flooding". This is done with a case, and not a set, because sets
           are limited to 256 elements. */
        switch (imsg) {

            case WM_PAINT: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_MBUTTONDOWN:
            case WM_MBUTTONUP: case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_SIZE:
            case WM_CHAR: case WM_KEYDOWN: case WM_KEYUP: case WM_QUIT: case WM_CLOSE:
            case WM_MOUSEMOVE: case WM_TIMER: case WM_COMMAND: case WM_VSCROLL:
            case WM_HSCROLL: case WM_NOTIFY: case MM_JOY1MOVE: case MM_JOY2MOVE:
            case MM_JOY1ZMOVE: case MM_JOY2ZMOVE: case MM_JOY1BUTTONDOWN:
            case MM_JOY2BUTTONDOWN: case MM_JOY1BUTTONUP: case MM_JOY2BUTTONUP:

/*
fprintf(stderr, "wndproc: passed to main: msg: %d ", msgcnt);
prtmsgu(hwnd, imsg, wparam, lparam); msgcnt = msgcnt+1;
*/
                putmsg(hwnd, imsg, wparam, lparam);
                break;

        }
        r = DefWindowProc(hwnd, imsg, wparam, lparam);

    }

    return (r);

}

/*******************************************************************************

Create dummy window

Create window to pass messages only. The window will have no display.

*******************************************************************************/

static void createdummy(WNDPROC wndproc, char* name, HWND* dummywin)

{

    WNDCLASS wc; /* windows class structure */
    ATOM     b;  /* int return */

    /* create dummy class for message handling */
    wc.style         = 0;
    wc.lpfnWndProc   = wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = 0;
    wc.hCursor       = 0;
    wc.hbrBackground = 0;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = str(name);
    /* register that class */
    b = RegisterClass(&wc);
    /* create the window */
    *dummywin =
        CreateWindow(name, "", 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                     GetModuleHandle(NULL), NULL);

}

/*******************************************************************************

Window display thread

Handles the actual display of all windows and input queues associated with
them. We create a dummy window to allow "headless" communication with the
thread, but any number of subwindows will be started in the thread.

*******************************************************************************/

static DWORD WINAPI dispthread(LPVOID lpParameter)

{

    MSG     msg;
    BOOL    b;   /* int return */
    LRESULT r;   /* result holder */

    /* create dummy window for message handling */
    createdummy(wndproc, "dispthread", &dispwin);

    b = SetEvent(threadstart); /* flag subthread has started up */

    /* message handling loop */
    while (GetMessage(&msg, 0, 0, 0)) { /* not a quit message */

        b = TranslateMessage(&msg); /* translate keyboard events */
        r = DispatchMessage(&msg);

    }

}

/*******************************************************************************

Window procedure

This is the event handler for the main thread. It"s a dummy, and we handle
everything by sending it on.

*******************************************************************************/

static LRESULT CALLBACK wndprocmain(HWND hwnd, UINT imsg, WPARAM wparam,
                                    LPARAM lparam)

{

    LRESULT r; /* result holder */

/*;fprintf(stderr, "wndprocmain: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);*/
    if (imsg == WM_CREATE) {

        r = 0;

    } else if (imsg == WM_DESTROY) {

        PostQuitMessage(0);
        r = 0;

    } else r = DefWindowProc(hwnd, imsg, wparam, lparam);

    return (r);

}

/*******************************************************************************

Dialog query window procedure

This message handler is to allow is to fix certain features of dialogs, like
the fact that they come up behind the main window.

*******************************************************************************/

/* Note: The Microsoft online description does not use APIENTRY, this is in
   MINGW */
static UINT_PTR APIENTRY wndprocfix(HWND hwnd, UINT imsg, WPARAM wparam,
                                    LPARAM lparam)

{

    BOOL b; /* return value */

/*;fprintf(stderr, "wndprocfix: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);*/

    /* If dialog is focused, send it to the foreground. This solves the issue
       where dialogs are getting parked behind the main window. */
    if (imsg == WM_SETFOCUS) b = SetForegroundWindow(hwnd);

    return (0); /* tell callback to handle own messages */

}

/*******************************************************************************

Dialog procedure

Runs the various dialogs.

*******************************************************************************/

static LRESULT CALLBACK wndprocdialog(HWND hwnd, UINT imsg, WPARAM wparam,
                                      LPARAM lparam)

{

    DWORD        r;    /* result holder */
    imptr        ip;   /* intratask message pointer */
    CHOOSECOLOR  cr;   /* color select structure */
    BOOL         b;    /* result holder */
    int          i;    /* index for string */
    OPENFILENAME fr;   /* file select structure */
    char*        bs;   /* filename holder */
    FINDREPLACE* frrp; /* dialog control record for find/replace */
    char*        fs;   /* pointer to finder string */
    char*        rs;   /* pointer to replacement string */
    int          fl;   /* flags */
    CHOOSEFONT   fns;  /* font select structure */
    LOGFONT*     lf;   /* logical font structure */
    int          sl;
    int          fsl, rsl;

/*;fprintf(stderr, "wndprocdialog: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);
;fprintf(stderr, "");*/
    if (imsg == WM_CREATE)  {

        r = 0;

    } else if (imsg == WM_DESTROY)  {

        PostQuitMessage(0);
        r = 0;

    } else if (imsg == UM_IM) { /* intratask message */

        ip = (imptr)wparam; /* get im pointer */
        switch (ip->im) { /* im type */

            case imalert: /* it"s an alert */
                r = MessageBox(0, ip->altmsg, ip->alttit,
                               MB_OK | MB_SETFOREGROUND);
                /* signal complete */
                iputmsg(0, UM_IM, wparam, 0);
                break;

            case imqcolor:
                /* set starting color */
                cr.rgbResult = rgb2win(ip->clrred, ip->clrgreen, ip->clrblue);
                cr.lStructSize = 9*4; /* set size */
                cr.hwndOwner = 0; /* set no owner */
                cr.hInstance = 0; /* no instance */
                /*??? cr.rgbResult = 0;*/ /* clear color */
                cr.lpCustColors = gcolorsav; /* set global color save */
                /* set display all colors, start with initalized color */
                cr.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_ENABLEHOOK;
                cr.lCustData = 0; /* no data */
                cr.lpfnHook = wndprocfix; /* hook to force front */
                cr.lpTemplateName = NULL; /* set no template name */
                b = ChooseColor(&cr); /* perform choose color */
                /* set resulting color */
                win2rgb(cr.rgbResult, &ip->clrred, &ip->clrgreen, &ip->clrblue);
                /* signal complete */
                iputmsg(0, UM_IM, wparam, 0);
                break;

            case imqopen:
            case imqsave:
                /* find length of input string */
                if (ip->im == imqopen) sl = strlen(ip->opnfil)+1;
                else sl = strlen(ip->savfil)+1;
                if (sl < 256) sl = 256;
                bs = imalloc(sl); /* get string */
                if (ip->im == imqopen) { /* it's open */

                    strcpy(bs, ip->opnfil); /* copy to new string */
                    ifree(ip->opnfil); /* free the passed string */
                    ip->opnfil = bs; /* return result in this string */

                } else { /* it's save */

                    strcpy(bs, ip->savfil); /* copy to new string */
                    ifree(ip->savfil); /* free the passed string */
                    ip->savfil = bs; /* return result in this string */

                }
                fr.lStructSize = sizeof(OPENFILENAME); /* set size */
                fr.hwndOwner = 0;
                fr.hInstance = 0;
                fr.lpstrFilter = NULL;
                fr.lpstrCustomFilter = NULL;
                fr.nFilterIndex = 0;
                fr.lpstrFile = bs;
                fr.nMaxFile = sl;
                fr.lpstrFileTitle = NULL;
                fr.lpstrInitialDir = NULL;
                fr.lpstrTitle = NULL;
                fr.Flags = OFN_HIDEREADONLY | OFN_ENABLEHOOK;
                fr.nFileOffset = 0;
                fr.nFileExtension = 0;
                fr.lpstrDefExt = NULL;
                fr.lCustData = 0;
                fr.lpfnHook = wndprocfix; /* hook to force front */
                fr.lpTemplateName = NULL;
                fr.pvReserved = 0;
                fr.dwReserved = 0;
                fr.FlagsEx = 0;
                if (ip->im == imqopen)  /* it's open */
                    b = GetOpenFileName(&fr); /* perform choose file */
                else /* it's save */
                    b = GetSaveFileName(&fr); /* perform choose file */
                if (!b) {

                    /* Check was a cancel. If the user canceled, return a null
                       string. */
                    r = CommDlgExtendedError();
                    if (!r) error(efildlg); /* error */
                    /* Since the main code is expecting us to make a new string for
                       the result, we must copy the input to the output so that it"s
                       disposal will be valid. */
                    if (ip->im == imqopen) *(ip->opnfil) = 0; /* terminate string */
                    else *(ip->savfil) = 0;

                }
                /* signal complete */
                iputmsg(0, UM_IM, wparam, 0);
                break;

            case imqfind:
                sl = strlen(ip->fndstr)+1; /* find length of input */
                /* must be >= 80 characters */
                if (sl < 80) sl = 80;
                fs = imalloc(sl); /* get string */
                strcpy(fs, ip->fndstr); /* copy input string to buffer */
                ifree(ip->fndstr); /* free the passed string */
                ip->fndstr = fs; /* index buffer for return */
                frrp = imalloc(sizeof(FINDREPLACE)); /* get find/replace entry */
                frrp->lStructSize = sizeof(FINDREPLACE); /* set size */
                frrp->hwndOwner = dialogwin; /* set owner */
                frrp->hInstance = 0; /* no instance */
                /* set flags */
                fl = FR_HIDEWHOLEWORD; /* or FR_ENABLEHOOK*/;
                /* set status of down */
                if (!(BIT(ami_qfnup) & ip->fndopt)) fl |= FR_DOWN;
                /* set status of case */
                if (BIT(ami_qfncase) & ip->fndopt) fl |= FR_MATCHCASE;
                frrp->Flags = fl;
                frrp->lpstrFindWhat = fs; /* place finder string */
                frrp->lpstrReplaceWith = NULL; /* set no replacement string */
                frrp->wFindWhatLen = sl; /* set length */
                frrp->wReplaceWithLen = 0; /* set null replacement string */
                frrp->lCustData = (LPARAM)ip; /* send ip with this record */
                frrp->lpfnHook = 0; /* no callback */
                frrp->lpTemplateName = NULL; /* set no template */
                /* start find dialog */
                fndrepmsg = RegisterWindowMessage("commdlg_FindReplace");
                ip->fndhan = FindText(frrp); /* perform dialog */
                /* now bring that to the front */
                b = SetWindowPos(ip->fndhan, HWND_TOP, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE);
                b = SetForegroundWindow(ip->fndhan);
                r = 0;
                break;

            case imqfindrep:
                /* find length of search string */
                fsl =strlen(ip->fnrsch);
                if (fsl < 80) fsl = 80; /* ensure >= 80 */
                fs = imalloc(sl); /* get string */
                strcpy(fs, ip->fnrsch); /* copy input string to buffer */
                ifree(ip->fnrsch); /* free the passed string */
                ip->fnrsch = fs; /* index buffer for return */
                /* find length of replacement string */
                rsl = strlen(ip->fnrrep);
                if (rsl < 80) rsl = 80; /* ensure >= 80 */
                rs = imalloc(sl); /* get string */
                strcpy(rs, ip->fnrrep); /* copy input string to buffer */
                ifree(ip->fnrrep); /* free the passed string */
                ip->fnrrep = rs; /* index buffer for return */
                frrp = imalloc(sizeof(FINDREPLACE)); /* get find/replace entry */
                frrp->lStructSize = sizeof(FINDREPLACE); /* set size */
                frrp->hwndOwner = dialogwin; /* set owner */
                frrp->hInstance = 0; /* no instance */
                /* set flags */
                fl = FR_HIDEWHOLEWORD;
                if (!(BIT(ami_qfrup) & ip->fnropt)) fl |= FR_DOWN; /* set status of down */
                if (BIT(ami_qfrcase) & ip->fnropt) fl |= FR_MATCHCASE; /* set status of case */
                frrp->Flags = fl;
                frrp->lpstrFindWhat = fs; /* place finder string */
                frrp->lpstrReplaceWith = rs; /* place replacement string */
                frrp->wFindWhatLen = fsl; /* set length */
                frrp->wReplaceWithLen = rsl; /* set null replacement string */
                frrp->lCustData = (LPARAM)ip; /* send ip with this record */
                frrp->lpfnHook = 0; /* clear these */
                frrp->lpTemplateName = NULL;
                /* start find dialog */
                fndrepmsg = RegisterWindowMessage("commdlg_FindReplace");
                ip->fnrhan = ReplaceText(frrp); /* perform dialog */
                /* now bring that to the front */
                b = SetWindowPos(ip->fnrhan, HWND_TOP, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE);
                b = SetForegroundWindow(ip->fnrhan);
                r = 0;
                break;

            case imqfont:
                lf = imalloc(sizeof(LOGFONT)); /* get a logical font structure */
                /* initalize logical font structure */
                lf->lfHeight = ip->fntsiz; /* use default height */
                lf->lfWidth = 0; /* use default width */
                lf->lfEscapement = 0; /* no escapement */
                lf->lfOrientation = 0; /* orient to x axis */
                if (BIT(ami_qftebold) & ip->fnteff) lf->lfWeight = FW_BOLD; /* set bold */
                else lf->lfWeight = FW_DONTCARE; /* default weight */
                lf->lfItalic = BIT(ami_qfteitalic) & ip->fnteff;  /* italic */
                lf->lfUnderline = BIT(ami_qfteunderline) & ip->fnteff; /* underline */
                lf->lfStrikeOut = BIT(ami_qftestrikeout) & ip->fnteff; /* strikeout */
                lf->lfCharSet = DEFAULT_CHARSET; /* use default characters */
                /* use default precision */
                lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
                /* use default clipping */
                lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
                lf->lfQuality = DEFAULT_QUALITY; /* use default quality */
                lf->lfPitchAndFamily = 0; /* must be zero */
                strncpy(lf->lfFaceName, ip->fntstr, 32); /* place face name */
                /* initalize choosefont structure */
                fns.lStructSize = sizeof(CHOOSEFONT); /* set size */
                fns.hwndOwner = 0; /* set no owner */
                fns.hDC = 0; /* no device context */
                fns.lpLogFont = lf; /* logical font */
                fns.iPointSize = 0; /* no point size */
                /* set flags */
                fns.Flags = CF_SCREENFONTS | CF_EFFECTS |
                            CF_NOSCRIPTSEL | CF_FORCEFONTEXIST |
                            CF_TTONLY | CF_INITTOLOGFONTSTRUCT |
                            CF_ENABLEHOOK;
                /* color */
                fns.rgbColors = rgb2win(ip->fntfr, ip->fntfg, ip->fntfb);
                fns.lCustData = 0; /* no data */
                fns.lpfnHook = wndprocfix; /* hook to force front */
                fns.lpTemplateName = NULL; /* no template name */
                fns.hInstance = 0; /* no instance */
                fns.lpszStyle = NULL; /* no style */
                fns.nFontType = 0; /* no font type */
                fns.nSizeMin = 0; /* no minimum size */
                fns.nSizeMax = 0; /* no maximum size */
                b = ChooseFont(&fns); /* perform choose font */
                if (!b) {

                    /* Check was a cancel. If the user canceled, just return the string
                       empty. */
                    r = CommDlgExtendedError();
                    if (r) error(efnddlg); /* error */
                    /* Since the main code is expecting us to make a new string for
                       the result, we must copy the input to the output so that it"s
                       disposal will be valid. */
                    ip->fntstr = imalloc(1);
                    *(ip->fntstr) = 0;

                } else { /* set what the dialog changed */

                    ip->fnteff = 0; /* clear what was set */
                    if (lf->lfItalic) ip->fnteff |= BIT(ami_qfteitalic); /* italic */
                    else ip->fnteff &= ~BIT(ami_qfteitalic);
                    if (fns.nFontType & BOLD_FONTTYPE)
                        ip->fnteff |= BIT(ami_qftebold); /* bold */
                    else
                        ip->fnteff &= ~BIT(ami_qftebold);
                    if (lf->lfUnderline)
                        ip->fnteff |= BIT(ami_qfteunderline); /* underline */
                    else ip->fnteff &= ~BIT(ami_qfteunderline);
                    if (lf->lfStrikeOut)
                        ip->fnteff |= BIT(ami_qftestrikeout); /* strikeout */
                    else ip->fnteff &= ~BIT(ami_qftestrikeout);
                    /* place foreground colors */
                    win2rgb(fns.rgbColors, &ip->fntfr, &ip->fntfg, &ip->fntfb);
                    strncpy(ip->fntstr, lf->lfFaceName, 32); /* copy font string back */
                    ip->fntsiz = abs(lf->lfHeight); /* set size */

                }
                /* signal complete */
                iputmsg(0, UM_IM, wparam, 0);
                break;

        }
        r = 0; /* clear result */

    } else if (imsg == fndrepmsg) { /* find is done */

        /* Here's a series of dirty tricks. The find/replace record pointer is given
           back to us as an int by windows. We also stored the ip as "customer
           data" in int. */
        frrp = (FINDREPLACEA*) lparam; /* get find/replace data pointer */
        ip = (imptr)frrp->lCustData; /* get im pointer */
        if (ip->im == imqfind) { /* it"s a find */

            b = DestroyWindow(ip->fndhan); /* destroy the dialog */
            /* check and set case match mode */
            if (frrp->Flags & FR_MATCHCASE) ip->fndopt |= BIT(ami_qfncase);
            else ip->fndopt &= ~BIT(ami_qfncase);
            /* check and set up/down mode */
            if (frrp->Flags & FR_DOWN) ip->fndopt &= ~BIT(ami_qfnup);
            else ip->fndopt |= BIT(ami_qfnup);

        } else { /* it's a find/replace */

            b = DestroyWindow(ip->fnrhan); /* destroy the dialog */
            /* check and set case match mode */
            if (frrp->Flags & FR_MATCHCASE) ip->fnropt |= BIT(ami_qfrcase);
            else ip->fnropt &= ~BIT(ami_qfrcase);
            /* check and set find mode */
            if (frrp->Flags & FR_FINDNEXT)
                ip->fnropt = (ip->fnropt & ~BIT(ami_qfrallfil) &
                              ~BIT(ami_qfralllin)) | BIT(ami_qfrfind);
            /* check and set replace mode */
            if (frrp->Flags & FR_REPLACE)
                ip->fnropt = (ip->fnropt & ~BIT(ami_qfrfind) &
                              ~BIT(ami_qfrallfil) & ~BIT(ami_qfralllin));
            /* check and set replace all mode */
            if (frrp->Flags & FR_REPLACEALL)
                ip->fnropt = (ip->fnropt & ~BIT(ami_qfrfind) &
                              ~BIT(ami_qfralllin)) | BIT(ami_qfrallfil);

        }
        ifree(frrp); /* release find/replace entry */
        /* signal complete */
        iputmsg(0, UM_IM, (WPARAM)ip, 0);

    } else r = DefWindowProc(hwnd, imsg, wparam, lparam);

    return (r);

}

/*******************************************************************************

Dialog thread

*******************************************************************************/

static DWORD WINAPI dialogthread(LPVOID lpParameter)

{

    MSG     msg;
    BOOL    b;   /* int return */
    LRESULT r;   /* result holder */

    /* create dummy window for message handling */
    createdummy(wndprocdialog, "dialogthread", &dialogwin);

    b = SetEvent(threadstart); /* flag subthread has started up */

    /* message handling loop */
    while (GetMessage(&msg, 0, 0, 0)) { /* not a quit message */

        b = TranslateMessage(&msg); /* translate keyboard events */
        r = DispatchMessage(&msg);

    }

}

/*******************************************************************************

Console control handler

This procedure gets activated as a callback when Windows flags a termination
event to console. We immediately abort.

At the present time, we don't care what type of termination event it was,
all generate an etterm signal.

*******************************************************************************/

static BOOL WINAPI conhan(DWORD ct)

{

    /* had some issues with zombie processes, so use Windows "ultra-kill"
       call */
    ExitProcess(1);

    //abortm(); /* abort run */

    return (1); /* set event handled */

}

/*******************************************************************************

Open

We don't do anything for this, so pass it on.

*******************************************************************************/

static int iopen(const char* pathname, int flags, int perm)

{

    return (*ofpopen)(pathname, flags, perm);

}

/*******************************************************************************

Close

Does nothing but pass on.

*******************************************************************************/

static int iclose(int fd)

{

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    /* check if the file is an output window, and close if so */
    if (opnfil[fd] && opnfil[fd]->win) closewin(fd);

    return (*ofpclose)(fd);

}

/*******************************************************************************

Lseek

Lseek is passed on.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    return (*ofplseek)(fd, offset, whence);

}

/*******************************************************************************

Read file

If the file is the stdin file, we process that by reading from the event queue
and returning any characters found. Any events besides character events are
discarded, which is why reading from the stdin file is a downward compatible
operation.

The input from user is line buffered and may be edited by the user.

All other files are passed on to the system level.

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
            if (opnfil[fi]->win->inpptr >= 0) ff = fi; /* found one */

    return (ff); /* return result */

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    int            l;   /* length left on destination */
    winptr         win; /* pointer to window data */
    int            ofn; /* output file handle */
    ssize_t        rc;  /* return code */
    unsigned char* ba;

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->inw) { /* process input file */

        lockmain(); /* start exclusive access */
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
                    /* if we have just read the last of that line,  flag buffer
                       empty */
                    if (*ba == '\n') win->inpptr = -1;
                    l--; /* count characters */

                }

            }

      }
      rc = count; /* set all bytes read */
      unlockmain();/* end exclusive access */

    } else
        /* passdown */
        rc = (*ofpread)(fd, buff, count);

    return rc;

}

/*******************************************************************************

Write

If the file is the stdout file, we send that output to the screen, recognizing
any control characters.

All other files are passed on to the system level.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    int     i;   /* index for destination */
    int     l;   /* length left on destination */
    winptr  win; /* pointer to window data */
    ssize_t rc;  /* return code */
    unsigned char* ba;

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->win) { /* process window output file */

        lockmain(); /* start exclusive access */
        win = lfn2win(fd); /* index window */
        l = count; /* set length of source */
        ba = (unsigned char*)buff; /* index buffer */
        while (l > 0) { /* write output bytes */

            plcchr(win, *ba++); /* send character to terminal emulator */
            l--; /* count characters */

        }
        rc = count; /* set number of bytes written */
        unlockmain(); /* end exclusive access */

    } else /* standard file */
        rc = (*ofpwrite)(fd, buff, count);

    return (rc); /* return read count/error */

}

/*******************************************************************************

Graph startup

*******************************************************************************/

static void ami_init_graph (void) __attribute__((constructor (103)));
static void ami_init_graph()

{

    int       i;
    int       fi;
    int       ofn, ifn;
    ami_valptr config_root; /* root for config block */
    ami_valptr term_root; /* root for terminal block */
    ami_valptr graph_root; /* root for graphics block */
    ami_valptr diag_root; /* root for diagnostics block */
    ami_valptr win_root; /* root for windows */
    ami_valptr vp;
    char*     errstr;
    ami_evtcod e;

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);

    maxxd = MAXXD; /* set default window dimensions */
    maxyd = MAXYD;
    dialogerr = DIALOGERR; /* send runtime errors to dialog */
    mouseenb = MOUSEENB; /* enable mouse */
    joyenb = JOYENB; /* enable joystick */
    dmpmsg = DMPMSG; /* dump windows API messages */
    dmpevt = DMPEVT; /* dump Petit-Ami messages */

    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */
    eqefre = NULL; /* set free event queuing list empty */
    dblflt = FALSE; /* set no double fault */
    wigfre = NULL; /* set free widget tracking list empty */
    freitm = NULL; /* clear intratask message free list */


    /* get setup configuration */
    config_root = NULL;
    ami_config(&config_root);

    /* find "terminal" block */
    term_root = ami_schlst("terminal", config_root);
    if (term_root && term_root->sublist) term_root = term_root->sublist;

    /* find x an y max if they exist */
    vp = ami_schlst("maxxd", term_root);
    if (vp) maxxd = strtol(vp->value, &errstr, 10);
    if (*errstr) error(ecfgval);
    vp = ami_schlst("maxyd", term_root);
    if (vp) maxyd = strtol(vp->value, &errstr, 10);
    if (*errstr) error(ecfgval);
    vp = ami_schlst("joystick", term_root);
    if (vp) joyenb = strtol(vp->value, &errstr, 10);
    vp = ami_schlst("mouse", term_root);
    if (vp) mouseenb = strtol(vp->value, &errstr, 10);
    vp = ami_schlst("dump_event", term_root);
    if (vp) dmpevt = strtol(vp->value, &errstr, 10);

    /* find graph block */
    graph_root = ami_schlst("graphics", config_root);
    if (graph_root) {

        vp = ami_schlst("dialogerr", graph_root->sublist);
        if (vp) dialogerr = strtol(vp->value, &errstr, 10);
        if (*errstr) error(ecfgval);

        /* find windows subsection */
        win_root = ami_schlst("windows", graph_root->sublist);
        if (win_root) {

            /* find diagnostic subsection */
            diag_root = ami_schlst("diagnostics", win_root->sublist);
            if (diag_root) {

                vp = ami_schlst("dump_messages", diag_root->sublist);
                if (vp) dmpmsg = strtol(vp->value, &errstr, 10);
                if (*errstr) error(ecfgval);

            }

        }

    }

    /* Set up private message queuing */
    msginp = 0; /* clear message input queue */
    msgout = 0;
    msgrdy = CreateEvent(NULL, TRUE, FALSE, NULL); /* create message event */
    imsginp = 0; /* clear control message message input queue */
    imsgout = 0;
    imsgrdy = CreateEvent(NULL, TRUE, FALSE, NULL); /* create message event */
    InitializeCriticalSection(&mainlock); /* initialize the sequencer lock */
    /* mainlock = createmutex(FALSE); */ /* create mutex with no owner */
    /* if mainlock == 0  winerr(); */ /* process windows error */
    fndrepmsg = 0; /* set no find/replace message active */
    for (i = 0; i < 16; i++) gcolorsav[i] = 0xffffff; /* set all to white */

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_ettabbar; e++) evthan[e] = defaultevent;

    /* clear open files table */
    for (fi = 0; fi < MAXFIL; fi++) {

        opnfil[fi] = NULL; /* set unoccupied */
        /* clear window logical number translator table */
        xltwin[fi] = -1; /* set unoccupied */
        /* clear file to window logical number translator table */
        filwin[fi] = -1; /* set unoccupied */

    }

    /* Create dummy window for message handling. This is only required so that
      the main thread can be attached to the display thread */
    createdummy(wndprocmain, "mainthread", &mainwin);
    mainthreadid = GetCurrentThreadId();

    getpgm(); /* get the program name from the command line */
    /* Now start the display thread, which both manages all displays, and sends us
      all messages from those displays. */
    threadstart = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!threadstart) winerr(); /* process windows error */
    /* create subthread */
    ResetEvent(threadstart); /* clear event */
    threadhdl = CreateThread(NULL, 0, dispthread, NULL, 0, &threadid);
    WaitForSingleObject(threadstart, -1); /* wait for thread to start */
    if (r == -1) winerr(); /* process windows error */
    /* Past this point, we need to lock for access between us and the thread. */

    /* Now attach the main thread to the display thread. This is required for the
      main thread to have access to items like the display window caret. */
    b = AttachThreadInput(mainthreadid, threadid, TRUE);
    if (!b) winerr(); /* process windows error */

    /* Start widget thread */
    ResetEvent(threadstart); /* clear event */
    CreateThread(NULL, 0, dialogthread, NULL, 0, &threadid);
    r = WaitForSingleObject(threadstart, -1); /* wait for thread to start */
    if (r == -1) winerr(); /* process windows error */

    /* register the stdwin class used to create all windows */
    regstd();

    /* open stdin and stdout as I/O window set */
    ifn = fileno(stdin); /* get logical id stdin */
    ofn = fileno(stdout); /* get logical id stdout */
    lockmain(); /* lock access */
    openio(stdin, stdout, ifn, ofn, -1, 1); /* process open */
    unlockmain(); /* unlock access */
    /* capture control handler so that ctl-c to main window cancels properly */
    SetConsoleCtrlHandler(conhan, TRUE);

}

/*******************************************************************************

Graph shutdown

*******************************************************************************/

static void ami_deinit_graph (void) __attribute__((destructor (102)));
static void ami_deinit_graph(void)

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    plseek_t cpplseek;

    winptr wp;
    int    fi;

    lockmain(); /* start exclusive access */
    /* using mingw, the graphics .dlls will be exited by the time we reach here,
       so we cannot perform the wait uncommanded exit */
#ifdef WAITCANCEL
    /* if the program tries to exit when the user has not ordered an exit, it
       is assumed to be a windows "unaware" program. We stop before we exit
       these, so that their content may be viewed */
    if (!fend && fautohold) { /* process automatic exit sequence */

        /* See if output is open at all */
        if (opnfil[OUTFIL]) /* output is allocated */
            if (opnfil[OUTFIL]->win) {

            wp = opnfil[OUTFIL]->win;
            /* make sure we are displayed */
            if (!wp->visible) winvis(wp);
            /* If buffering is off, turn it back on. This will cause the screen
               to come up clear, but this is better than an unrefreshed "hole"
               in the screen. */
            if (!wp->bufmod) ibuffer(wp, TRUE);
            /* Check framed. We don"t want that off, either, since the user cannot
               close the window via the system close button. */
            if (!wp->frame) iframe(wp, TRUE);
            /* Same with system bar */
            if (!wp->sysbar) isysbar(wp, TRUE);
            /* change window label to alert user */
            unlockmain(); /* end exclusive access */
            SetWindowText(wp->winhan, trmnam);
            lockmain(); /* start exclusive access */
            /* wait for a formal end */
            while (!fend) ievent(INPFIL, &er);

        }

    }

    /* close any open windows */
    if (!dblflt)  { /* we haven"t already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files and windows */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->win)
                clswin(fi); /* close open window */

    }
#endif
    /* kill subthread */
    TerminateThread(threadhdl, 0);
    unlockmain(); /* end exclusive access */

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cpplseek != ilseek)
        error(esystem);
    /* release control handler */
    SetConsoleCtrlHandler(NULL, FALSE);

}
