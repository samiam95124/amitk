/*******************************************************************************
*                                                                              *
*                     TRANSPARENT SCREEN CONTROL MODULE                        *
*                          FOR WINDOWS CONSOLE MODE                            *
*                                                                              *
*                             2020 S. A. Franco                                *
*                                                                              *
* This module implements the standard terminal calls for windows console       *
* mode.                                                                        *
*                                                                              *
* Windows console mode is fully buffered, with multiple buffering and buffer   *
* to display switching, with buffer parameters stored in each buffer. Because  *
* of this, we let Windows manage the buffer operations, and mostly just        *
* reformat our calls to console mode.                                          *
*                                                                              *
*                                                                              *
* Remaining to be done:                                                        *
*                                                                              *
* 1. Make sure new buffers get proper coloring.                                *
*                                                                              *
* 2. We are getting "button 0" messages from the joystick on deassert during   *
*     fast repeated pushes.                                                    *
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

#include <sys/types.h>
#include <stdio.h>
#include <limits.h>
#include <windows.h>
#include <terminal.h>

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

/* standard file handles */
#define INPFIL 0   /* _input */
#define OUTFIL 1   /* _output */
#define MAXLIN 250 /* maximum length of input buffered line */
#define MAXCON 10  /* number of screen contexts */
#define MAXTAB 250 /* maximum number of tabs (length of buffer in x) */
#define FRMTIM 11  /* handle number of framing timer */

/* special user events */
#define UIV_BASE           0x8000 /* base of user events */
#define UIV_TIM            0x8000 /* timer matures */
#define UIV_JOY1MOVE       0x8001 /* joystick 1 move */
#define UIV_JOY1ZMOVE      0x8002 /* joystick 1 z move */
#define UIV_JOY2MOVE       0x8003 /* joystick 2 move */
#define UIV_JOY2ZMOVE      0x8004 /* joystick 2 z move */
#define UIV_JOY1BUTTONDOWN 0x8005 /* joystick 1 button down */
#define UIV_JOY2BUTTONDOWN 0x8006 /* joystick 2 button down */
#define UIV_JOY1BUTTONUP   0x8007 /* joystick 1 button up */
#define UIV_JOY2BUTTONUP   0x8008 /* joystick 2 button up */
#define UIV_TERM           0x8009 /* terminate program */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef int (*punlink_t)(const char*);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_unlink(punlink_t nfp, punlink_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/** screen attribute */
typedef enum {

    /* no attribute */               sanone,
    /* blinking text (foreground) */ sablink,
    /* reverse video */              sarev,
    /* underline */                  saundl,
    /* superscript */                sasuper,
    /* subscripting */               sasubs,
    /* italic text */                saital,
    /* bold text */                  sabold,
    /* strikeout text */             sastkout

} scnatt;

typedef struct { /* screen context */

    HANDLE   han;         /* screen buffer handle */
    int      maxx;        /* maximum x */
    int      maxy;        /* maximum y */
    int      offy;        /* offset within buffer to display area */
    int      curx;        /* current cursor location x */
    int      cury;        /* current cursor location y */
    int      curv;        /* cursor visible */
    ami_color forec;       /* current writing foreground color */
    ami_color backc;       /* current writing background color */
    scnatt   attr;        /* current writing attribute */
    int      autof;       /* current status of scroll and wrap */
    int      tab[MAXTAB]; /* tabbing array */
    int      sattr;       /* current character attributes */

} scncon, *scnptr;

/* error code this module */
typedef enum {

    eftbful, /* file table full */
    ejoyacc, /* joystick access */
    etimacc, /* timer access */
    efilopr, /* cannot perform operation on special file */
    efilzer, /* filename is empty */
    einvscn, /* invalid screen number */
    einvhan, /* invalid handle */
    einvtab, /* invalid tab position */
    esbfcrt, /* cannot create screen buffer */
    ejoyqry, /* Could not get information on joystick */
    einvjoy, /* Invalid joystick ID */
    enomem,  /* insufficient memory */
    esystem  /* System consistency check */

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
static punlink_t ofpunlink;
static plseek_t  ofplseek;

HANDLE inphdl;           /* "input" file handle */
int     nummbt;          /* number of mouse buttons */
int     mb1;             /* mouse assert status button 1 */
int     mb2;             /* mouse assert status button 2 */
int     mb3;             /* mouse assert status button 3 */
int     mb4;             /* mouse assert status button 4 */
int     mpx, mpy;        /* mouse current position */
int     nmb1;            /* new mouse assert status button 1 */
int     nmb2;            /* new mouse assert status button 2 */
int     nmb3;            /* new mouse assert status button 3 */
int     nmb4;            /* new mouse assert status button 4 */
int     nmpx, nmpy;      /* new mouse current position */
char    inpbuf[MAXLIN];  /* input line buffer */
int     inpptr;          /* input line index */
scnptr  screens[MAXCON]; /* screen contexts array */
int     curdsp;          /* index for current display screen */
int     curupd;          /* index for current update screen */
struct {

    int han; /* handle for timer */
    int rep; /* timer repeat flag */

} timers[10];


CONSOLE_SCREEN_BUFFER_INFO bi; /* screen buffer info structure */
CONSOLE_CURSOR_INFO        ci; /* console cursor info structure */
int      ti;          /* index for repeat array */
DWORD    mode;        /* console mode */
int      b;           /* int return */
int      i;           /* tab index */
HWND     winhan;      /* main window id */
DWORD    threadid;    /* dummy thread id (unused) */
int      threadstart; /* thread starts */
int      joy1xs;      /* last joystick position 1x */
int      joy1ys;      /* last joystick position 1y */
int      joy1zs;      /* last joystick position 1z */
int      joy2xs;      /* last joystick position 2x */
int      joy2ys;      /* last joystick position 2y */
int      joy2zs;      /* last joystick position 2z */
int      numjoy;      /* number of joysticks found */
/* global sets. these are the global set parameters that apply to any new
   created screen buffer */
int      gmaxx;       /* maximum x size */
int      gmaxy;       /* maximum y size */
scnatt   gattr;       /* current attribute */
int      gautof;      /* state of auto */
ami_color gforec;      /* forground color */
ami_color gbackc;      /* background color */
int      gcurv;       /* state of cursor visible */
int      cix;         /* index for display screens */
int      frmrun;      /* framing timer is running */
int      frmhan;      /* framing timer handle */
DWORD    cmodes;      /* previous console mode settings */
static ami_pevthan evthan[ami_etframe+1]; /* array of event handler routines */
static ami_pevthan evtshan; /* single master event handler routine */

/*******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/

static void error(int e)

{

    /* if screen is active and not screen 1, flip back to screen 1 so the
       error can be seen */
    if (screens[0] && curdsp != 1)
       SetConsoleActiveScreenBuffer(screens[0]->han);
    fprintf(stderr, "*** Error: console: ");
    switch (e) { /* error */

        case eftbful: fprintf(stderr, "Too many files"); break;
        case ejoyacc: fprintf(stderr, "No joystick access available"); break;
        case etimacc: fprintf(stderr, "No timer access available"); break;
        case einvhan: fprintf(stderr, "Invalid handle"); break;
        case efilopr: fprintf(stderr, "Cannot perform operation on special file");
                      break;
        case efilzer: fprintf(stderr, "Filename is empty"); break;
        case einvscn: fprintf(stderr, "Invalid screen number"); break;
        case einvtab: fprintf(stderr, "Tab position specified off screen"); break;
        case esbfcrt: fprintf(stderr, "Cannot create screen buffer"); break;
        case einvjoy: fprintf(stderr, "Invalid joystick ID"); break;
        case ejoyqry: fprintf(stderr, "Could not get information on joystick");
                      break;
        case enomem:  fprintf(stderr, "Insufficient memory"); break;
        case esystem: fprintf(stderr, "System fault"); break;

    }
    fprintf(stderr, "\n");

    exit(1);

}

/********************************************************************************

Handle Windows error

Only called if the last error variable is set. The text string for the error
is output, and then the program halted.

********************************************************************************/

static void winerr(void)
{

    int e;
    char *p;
    LPVOID lpMsgBuf;
    int le;

    e = GetLastError();
    le = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, e,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0, NULL);
    fprintf(stderr, "\n*** Windows error: %s\n", (char*)lpMsgBuf);

    exit(1);

}

/******************************************************************************

Print event symbol

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

void prtevt(ami_evtcod e)

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
        case ami_etframe:   fprintf(stderr, "etframe"); break;

        default: fprintf(stderr, "???");

    }

}

/*******************************************************************************

Set colors

Sets the current background and foreground colors in windows attribute format
from the coded colors and the "reverse" attribute.

Despite the name, also sets the attributes. We obey reverse coloring, and
set the following "substitute" attributes:

italics      Foreground half intensity.

underline    Background half intensity.

*******************************************************************************/

static int colnum(ami_color c, int i)

{

    int r;

    switch (c) { /* color */

        case ami_black:  r = 0x0000 | i*FOREGROUND_INTENSITY; break;
        case ami_white:  r = FOREGROUND_BLUE | FOREGROUND_GREEN |
                            FOREGROUND_RED | !i*FOREGROUND_INTENSITY; break;
        case ami_red:      r = FOREGROUND_RED | !i*FOREGROUND_INTENSITY; break;
        case ami_green:    r = FOREGROUND_GREEN | !i*FOREGROUND_INTENSITY; break;
        case ami_blue:     r = FOREGROUND_BLUE | !i*FOREGROUND_INTENSITY; break;
        case ami_cyan:     r = FOREGROUND_BLUE | FOREGROUND_GREEN |
                         !i*FOREGROUND_INTENSITY; break;
        case ami_yellow:  r = FOREGROUND_RED | FOREGROUND_GREEN |
                        !i*FOREGROUND_INTENSITY; break;
        case ami_magenta: r = FOREGROUND_RED | FOREGROUND_BLUE |
                        !i*FOREGROUND_INTENSITY; break;

    }

    return (r); /* return result */

}

static void setcolor(scnptr sc)

{

    if (sc->attr == sarev) /* set reverse colors */
        sc->sattr = colnum(sc->forec, (sc->attr == saital || sc->attr == sabold))*16+
                    colnum(sc->backc, (sc->attr == saundl || sc->attr == sabold));
    else /* set normal colors */
        sc->sattr = colnum(sc->backc, (sc->attr == saundl || sc->attr == sabold))*16+
                    colnum(sc->forec, (sc->attr == saital || sc->attr == sabold));
    gattr = sc->attr; /* set global to match */

}

/*******************************************************************************

Find colors

Find colors from attribute word.

*******************************************************************************/


ami_color numcol(int a)

{

    ami_color c;

    switch (a%8) { /* color */

         case 0: c = ami_black; break;
         case 1: c = ami_blue; break;
         case 2: c = ami_green; break;
         case 3: c = ami_cyan; break;
         case 4: c = ami_red; break;
         case 5: c = ami_magenta; break;
         case 6: c = ami_yellow; break;
         case 7: c = ami_white; break;

    }

    return (c);

}

static void fndcolor(int a)

{

    screens[curupd-1]->forec = numcol(a); /* set foreground color */
    screens[curupd-1]->backc = numcol(a/16); /* set background color */

}

/*******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns true if so.

*******************************************************************************/

static int icurbnd(scnptr sc)

{

    return (sc->curx >= 1 && sc->curx <= sc->maxx && sc->cury >= 1 &&
            sc->cury <= sc->maxy);

}

/*******************************************************************************

Set cursor status

Sets the cursor visible or invisible. If the cursor is out of bounds, it is
invisible regardless. Otherwise, it is visible according to the state of the
current buffer's visible status.

*******************************************************************************/

static void cursts(scnptr sc)

{

    int b;
    CONSOLE_CURSOR_INFO ci;
    int cv;

    cv = sc->curv; /* set current buffer status */
    if (!icurbnd(sc)) cv = 0; /* not in bounds, force off */
    /* get current console information */
    b = GetConsoleCursorInfo(sc->han, &ci);
    ci.bVisible = cv; /* set cursor status */
    b = SetConsoleCursorInfo(sc->han, &ci);

}

/*******************************************************************************

Position cursor

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that.

Windows has a nasty bug that setting the cursor position of a buffer that
isn't in display causes a cursor mark to be made at that position on the
active display. So we don't position if not in display.

*******************************************************************************/

static void setcur(scnptr sc)

{

    int b;
    COORD xy;

    /* check cursor in bounds, and buffer in display */
    if (icurbnd(sc) && sc == screens[curdsp-1]) {

        /* set cursor position */
        xy.X = sc->curx-1;
        xy.Y = sc->cury+sc->offy-1;
        b = SetConsoleCursorPosition(sc->han, xy);
        /* error occurs because our version of maxx/maxy is behind real window
           size */
        /* if (!b) winerr(); */

    }
    cursts(sc); /* set new cursor status */

}

/*******************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void iclear(scnptr sc)

{

    int   x, y;
    char  cb; /* character output buffer */
    WORD  ab; /* attribute output buffer */
    int   b;
    COORD xy;
    DWORD len;

    cb = ' '; /* set space */
    ab = sc->sattr; /* set attributes */
    for (y = 0; y < sc->maxy; y++) {

        for (x = 0; x < sc->maxx; x++) {

            xy.X = x;
            xy.Y = y+sc->offy;
            b = WriteConsoleOutputCharacter(sc->han, &cb, 1, xy, &len);
            b = WriteConsoleOutputAttribute(sc->han, &ab, 1, xy, &len);

        }

    }
    sc->cury = 1; /* set cursor at home */
    sc->curx = 1;
    setcur(sc);

}

/*******************************************************************************

Initalize screen

Clears all the parameters in the present screen context.

*******************************************************************************/

static void iniscn(scnptr sc)

{

    int i;
    COORD xy;

    sc->maxx = gmaxx; /* set size */
    sc->maxy = gmaxy;
    xy.X = sc->maxx;
    xy.Y = sc->maxy;
    b = SetConsoleScreenBufferSize(sc->han, xy);
    sc->forec = gforec; /* set colors and attributes */
    sc->backc = gbackc;
    sc->attr = gattr;
    sc->autof = gautof; /* set auto scroll and wrap */
    sc->curv = gcurv; /* set cursor visibility */
    setcolor(sc); /* set current color */
    iclear(sc); /* clear screen buffer with that */
    /* set up tabbing to be on each 8th position */
    for (i = 0; i < sc->maxx; i++) sc->tab[i] = i%8 == 0;

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

Scroll screen

Scrolls the terminal screen by deltas in any given direction. Windows performs
scrolls as source->destination rectangle moves. This can be easily converted
from our differentials, but there is additional complexity in that both the
source and destination must be on screen, and not in negative space. So we
take each case of up, down, left right and break it out into moves that will
remain on the screen.
Because Windows (oddly) fills based on the source rectangle, we perform each
scroll as a separate sequence in each x and y direction. This makes the code
simpler, and lets Windows perform the fills for us.

*******************************************************************************/

static void iscroll(int x, int y)

{

    SMALL_RECT sr; /* scroll rectangle */
    CHAR_INFO  f;  /* fill character info */
    int        b;  /* return value */
    scnptr     sc; /* screen context */
    COORD      xy;

    sc = screens[curupd-1]; /* index screen context */
    f.Char.AsciiChar = ' '; /* set fill values */
    f.Attributes = sc->sattr;
    if (x <= -sc->maxx || x >= sc->maxx || y <= -sc->maxy || y >= sc->maxy)
        /* scroll would result in complete clear, do it */
        iclear(screens[curupd-1]); /* clear the screen buffer */
    else { /* scroll */

        /* perform y moves */
        if (y >= 0) { /* move text up */

            sr.Left = 0;
            sr.Right = sc->maxx-1;
            sr.Top = y;
            sr.Bottom = sc->maxy+sc->offy-1;
            xy.X = 0;
            xy.Y = 0;
            b = ScrollConsoleScreenBuffer(sc->han, &sr, NULL, xy, &f);

        } else { /* move text down */

            sr.Left = 0;
            sr.Right = sc->maxx-1;
            sr.Top = 0;
            sr.Bottom = sc->maxy+sc->offy-1;
            xy.X = 0;
            xy.Y = abs(y);
            b = ScrollConsoleScreenBuffer(sc->han, &sr, NULL, xy, &f);

        }
        /* perform x moves */
        if (x >= 0) { /* move text left */

            sr.Left = x;
            sr.Right = sc->maxx-1;
            sr.Top = 0;
            sr.Bottom = sc->maxy+sc->offy-1;
            xy.X = 0;
            xy.Y = 0;
            b = ScrollConsoleScreenBuffer(sc->han, &sr, NULL, xy, &f);

        } else { /* move text right */

            sr.Left = 0;
            sr.Right = sc->maxx-1;
            sr.Top = 0;
            sr.Bottom = sc->maxy+sc->offy-1;
            xy.X = abs(x);
            xy.Y = 0;
            b = ScrollConsoleScreenBuffer(sc->han, &sr, NULL, xy, &f);

        }

    }

}

void ami_scroll(FILE* f, int x, int y)

{

    iscroll(x, y); /* process scroll */

}

/*******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(int x, int y)

{

    screens[curupd-1]->cury = y; /* set new position */
    screens[curupd-1]->curx = x;
    setcur(screens[curupd-1]); /* set cursor on screen */

}

void ami_cursor(FILE* f, int x, int y)

{

    icursor(x, y); /* position cursor */

}

/*******************************************************************************

Find if cursor is in screen bounds

This is the external interface to curbnd.

*******************************************************************************/

int ami_curbnd(FILE* f)

{

    return (icurbnd(screens[curupd-1]));

}

/*******************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int ami_maxx(FILE* f)

{

    return (screens[curupd-1]->maxx); /* set maximum x */

}

/*******************************************************************************

Return maximum y dimension

Returns the maximum y demension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int ami_maxy(FILE* f)

{

    return (screens[curupd-1]->maxy); /* set maximum y */

}

/*******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void ami_home(FILE* f)

{

    screens[curupd-1]->cury = 1; /* set cursor at home */
    screens[curupd-1]->curx = 1;
    setcur(screens[curupd-1]); /* set cursor on screen */

}

/*******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

static void iup(void)

{

    scnptr sc;

    sc = screens[curupd-1];
    if (sc->autof) { /* auto mode is on */

        /* check not top of screen */
        if (sc->cury > 1) sc->cury--; /* update position */
        else iscroll(0, -1); /* scroll up */

       } else /* auto mode is off */
        /* check won't overflow */
        if (sc->cury > -INT_MAX) sc->cury--; /* set new position */
    setcur(sc); /* set cursor on screen */

}

void ami_up(FILE* f)

{

    iup(); /* move up */

}

/*******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

static void idown(void)

{

    scnptr                     sc; /* screen data pointer */
    CONSOLE_SCREEN_BUFFER_INFO bi; /* screen buffer info structure */
    SMALL_RECT                 sr; /* scroll rectangle */
    COORD                      xy; /* point */
    CHAR_INFO                  f;  /* fill character info */

    sc = screens[curupd-1];
    if (sc->autof) { /* auto mode is on */

        /* check not bottom of screen */
        if (sc->cury < sc->maxy) sc->cury++; /* update position */
        else {

            /* see if we have room between us and end of buffer */
            GetConsoleScreenBufferInfo(sc->han, &bi);
            if (bi.srWindow.Bottom < bi.dwSize.Y-1) {

                bi.srWindow.Top++; /* move display down */
                bi.srWindow.Bottom++;
                SetConsoleWindowInfo(sc->han, TRUE, &bi.srWindow);
                sc->offy++; /* and increase our offset */

            } else { /* scroll down */

                xy.X = 0;
                xy.Y = 0;
                sr.Top = 1; /* specify the last of the buffer minus one line */
                sr.Bottom = bi.dwSize.Y-1;
                sr.Left = 0;
                sr.Right = bi.dwSize.X;
                f.Char.AsciiChar = ' '; /* set fill values */
                f.Attributes = sc->sattr;
                ScrollConsoleScreenBuffer(sc->han, &sr, NULL, xy, &f);

            }

        }

    } else /* auto mode is off */
        /* prevent overflow, but otherwise increment unlimited */
        if (sc->cury < INT_MAX) sc->cury++;
    setcur(screens[curupd-1]); /* set cursor on screen */

}

void ami_down(FILE* f)

{

    idown(); /* move cursor down */

}

/*******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

static void ileft(void)

{

    scnptr sc;

    sc = screens[curupd-1];
    if (sc->autof) { /* auto mode is on */

        /* check not extreme left */
        if (sc->curx > 1) sc->curx--; /* update position */
        else { /* wrap cursor motion */

            iup(); /* move cursor up one line */
            sc->curx = sc->maxx; /* set cursor to extreme right */

        }

      } else /* auto mode is off */
        /* check won't overflow, but otherwise its unlimited */
        if (sc->curx > -INT_MAX) sc->curx--; /* update position */
    setcur(screens[curupd-1]); /* set cursor on screen */

}

void ami_left(FILE* f)

{

    ileft(); /* move cursor left */

}

/*******************************************************************************

Move cursor right internal

Moves the cursor one character right.

*******************************************************************************/

static void iright(void)

{

    scnptr sc;

    sc = screens[curupd-1];
    if (sc->autof) { /* auto mode is on */
        /* check not at extreme right */
        if (sc->curx < sc->maxx) sc->curx++; /* update position */
        else { /* wrap cursor motion */

            idown(); /* move cursor up one line */
            sc->curx = 1; /* set cursor to extreme left */

        }

    } else /* auto mode is off */
        /* check won't overflow, but otherwise its unlimited */
        if (sc->curx < INT_MAX) sc->curx++; /* update position */
    setcur(sc); /* set cursor on screen */

}

void ami_right(FILE* f)

{

    iright(); /* move cursor right */

}

/*******************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

static void itab(void)

{

    int i;

    /* first, find if next tab even exists */
    i = screens[curupd-1]->curx+1; /* get the current x position +1 */
    if (i < 1) i = 1; /* don't bother to search to left of screen */
    /* find tab or end of screen */
    while (i < screens[curupd-1]->maxx && !screens[curupd-1]->tab[i-1]) i++;
    if (screens[curupd-1]->tab[i-1]) /* we found a tab */
        while (screens[curupd-1]->curx < i) iright(); /* go to the tab */

}

/*******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

*******************************************************************************/

void ami_blink(FILE* f, int e)

{

    /* no capability */
    screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

}

/*******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_reverse(FILE* f, int e)

{

    if (e) screens[curupd-1]->attr = sarev; /* set reverse status */
    else screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

}

/*******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_underline(FILE* f, int e)

{

    /* substituted by background half intensity */
    if (e) screens[curupd-1]->attr = saundl; /* set underline status */
    else screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

}

/*******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_superscript(FILE* f, int e)

{

    /* no capability */
    screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

}

/*******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_subscript(FILE* f, int e)

{

    /* no capability */
    screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

}

/*******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_italic(FILE* f, int e)

{

    /* substituted by foreground half intensity */
    if (e) screens[curupd-1]->attr = saital; /* set italic status */
    else screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

}

/*******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void ami_bold(FILE* f, int e)

{

    /* substituted by foreground and background half intensity */
    if (e) screens[curupd-1]->attr = sabold; /* set bold status */
    else screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

}

/*******************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void ami_strikeout(FILE* f, int e)

{

    /* no capability */
    screens[curupd-1]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd-1]); /* set those colors active */

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

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

void ami_fcolor(FILE* f, ami_color c)

{

    screens[curupd-1]->forec = c; /* set color status */
    gforec = c; /* set global as well */
    setcolor(screens[curupd-1]); /* activate */

}

/*******************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void ami_bcolor(FILE* f, ami_color c)

{

    screens[curupd-1]->backc = c; /* set color status */
    gbackc = c; /* set global as well */
    setcolor(screens[curupd-1]); /* activate */

}

/*******************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and end of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up        Scroll down
down      Scroll up
right     Line down, start at left
left      Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

*******************************************************************************/

void ami_auto(FILE* f, int e)

{

    screens[curupd-1]->autof = e; /* set auto status */
    gautof = e; /* set global as well */

}

/*******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void ami_curvis(FILE* f, int e)

{

    screens[curupd-1]->curv = e; /* set cursor visible status */
    gcurv = e; /* set global as well */
    cursts(screens[curupd-1]); /* update cursor status */

}

/*******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int ami_curx(FILE* f)

{

    return (screens[curupd-1]->curx); /* return current location x */

}

/*******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int ami_cury(FILE* f)

{

    return (screens[curupd-1]->cury); /* return current location y */

}

/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

static void iselect(int u, int d)

{

    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    curupd = u; /* set current update screen */
    if (!screens[curupd-1]) { /* no screen, create one */

        screens[curupd-1] = malloc(sizeof(scncon)); /* get a new screen context */
        if (!screens[curupd-1]) error(enomem); /* failed to allocate */
        /* create the screen */
        screens[curupd-1]->han =
            CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        /* check handle is valid */
        if (screens[curupd-1]->han == INVALID_HANDLE_VALUE) error(esbfcrt);
        iniscn(screens[curupd-1]); /* initalize that */

    }
    curdsp = d; /* set the current display screen */
    if (!screens[curdsp-1]) { /* no screen, create one */

        screens[curdsp-1] = malloc(sizeof(scncon)); /* get a new screen context */
        if (!screens[curdsp-1]) error(enomem); /* failed to allocate */
        /* create the screen */
        screens[curdsp-1]->han =
            CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        /* check handle is valid */
        if (screens[curdsp-1]->han == INVALID_HANDLE_VALUE) error(esbfcrt);
        iniscn(screens[curdsp-1]); /* initalize that */

    }
    /* set display buffer as active display console */
    SetConsoleActiveScreenBuffer(screens[curdsp-1]->han);
    setcur(screens[curdsp-1]); /* make sure the cursor is at correct point */

}

void ami_select(FILE* f, int u, int d)

{

    iselect(u, d); /* perform select */

}

/*******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attributes. Obeys a series of elementary control characters.

We don't try to perform a terminal emulation here. The terminal interface does
that, and in any case an emulator would be layered above that.

*******************************************************************************/

static void plcchr(char c)

{

    int    b;   /* int return */
    char   cb;  /* character output buffer */
    WORD   ab;  /* attribute output buffer */
    DWORD  len; /* length dummy */
    scnptr sc;  /* screen context pointer */
    COORD  xy;

    sc = screens[curupd-1];
    /* handle special character cases first */
    if (c == '\r') /* carriage return, position to extreme left */
        icursor(1, sc->cury);
    else if (c == '\n') {

        idown(); /* line feed, move down */
#ifdef __MINGW32__
        /* if in a Unix/Linux emulation environment, we want to expand linefeed
           to CRLF */
        icursor(1, sc->cury); /* position to extreme left */
#endif

    } else if (c == '\b') ileft(); /* back space, move left */
    else if (c == '\f') iclear(sc); /* clear screen */
    else if (c == '\t') itab(); /* process tab */
    else if (c >= ' ' && c != 0x7f) { /* character is visible */

        if (icurbnd(sc)) { /* cursor in bounds */

            cb = c; /* place character in buffer */
            ab = sc->sattr; /* place attribute in buffer */
            /* write character */
            xy.X = sc->curx-1;
            xy.Y = sc->cury+sc->offy-1;
            b = WriteConsoleOutputCharacter(sc->han, &cb, 1, xy, &len);
            b = WriteConsoleOutputAttribute(sc->han, &ab, 1, xy, &len);

        }
        iright(); /* move cursor right */

    }

}

/*******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void ami_del(FILE* f)

{

    ami_left(f); /* back up cursor */
    plcchr(' '); /* blank out */
    ami_left(f); /* back up again */

}

/*******************************************************************************

Acquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

*******************************************************************************/

/*

Process keyboard event.
The events are mapped from IBM-PC keyboard keys as follows:

etup        up arrow            cursor up one line
etdown     down arrow           down one line
etleft     left arrow           left one character
etright    right arrow          right one character
etleftw    shift-left arrow     left one word
etrightw   shift-right arrow    right one word
ethome     cntrl-home           home of document
ethomes    shift-home           home of screen
ethomel    home                 home of line
etend      cntrl-end            end of document
etends     shift-end            end of screen
etendl     end                  end of line
etscrl     cntrl-left arrow     scroll left one character
etscrr     cntrl-right arrow    scroll right one character
etscru     cntrl-up arrow       scroll up one line
etscrd     cntrl-down arrow     scroll down one line
etpagd     page down            page down
etpagu     page up              page up
ettab      tab                  tab
etenter    enter                enter line
etinsert   cntrl-insert         insert block
etinsertl  shift-insert         insert line
etinsertt  insert               insert toggle
etdel      cntrl-del            delete block
etdell     shift-del            delete line
etdelcf    del                  delete character forward
etdelcb    backspace            delete character backward
etcopy     cntrl-f1             copy block
etcopyl    shift-f1             copy line
etcan      esc                  cancel current operation
etstop     cntrl-s              stop current operation
etcont     cntrl-q              continue current operation
etprint    shift-f2             print document
etprintb   cntrl-f2             print block
etprints   cntrl-f3             print screen
etf1       f1                   int key 1
etf2       f2                   int key 2
etf3       f3                   int key 3
etf4       f4                   int key 4
etf5       f5                   int key 5
etf6       f6                   int key 6
etf7       f7                   int key 7
etf8       f8                   int key 8
etf9       f9                   int key 9
etf10      10                   int key 10
etmenu     alt                  display menu
etend      cntrl-c              terminate program

*/

/* check control key pressed */

static int cntrl(INPUT_RECORD* inpevt)

{

    return (!!((*inpevt).Event.KeyEvent.dwControlKeyState &
               (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)));

}

/* check ALT key pressed */

static int alt(INPUT_RECORD* inpevt)

{

    return (!!((*inpevt).Event.KeyEvent.dwControlKeyState &
               (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)));

}

/* check shift key pressed */

static int shift(INPUT_RECORD* inpevt)

{

    return (!!((*inpevt).Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED));

}

static void keyevent(ami_evtptr er, INPUT_RECORD* inpevt, int* keep)

{

    /* uncomment for key scan diagnostic */

    /*
    printf("keyevent: up/dwn: %d rpt: %d vkc: %x vsc: %x asc: %x "
           "altl: %d altr: %d ctll: %d ctlr: %d shft: %d\n",
           inpevt->Event.KeyEvent.bKeyDown, inpevt->Event.KeyEvent.wRepeatCount,
           inpevt->Event.KeyEvent.wVirtualKeyCode,
           inpevt->Event.KeyEvent.wVirtualScanCode,
           inpevt->Event.KeyEvent.uChar.AsciiChar,
           !!(inpevt->Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED),
           !!(inpevt->Event.KeyEvent.dwControlKeyState & RIGHT_ALT_PRESSED),
           !!(inpevt->Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED),
           !!(inpevt->Event.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED),
           !!(inpevt->Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)
           );
    */

    /* we only take key down (pressed) events */
    if (inpevt->Event.KeyEvent.bKeyDown) {

        /* Check character is non-zero. The zero character occurs
          whenever a control feature is hit. */
        if (inpevt->Event.KeyEvent.uChar.AsciiChar) {

            if (inpevt->Event.KeyEvent.uChar.AsciiChar == '\r')
                er->etype = ami_etenter; /* set enter line */
            else if (inpevt->Event.KeyEvent.uChar.AsciiChar == '\b')
                er->etype = ami_etdelcb; /* set delete character backwards */
            else if (inpevt->Event.KeyEvent.uChar.AsciiChar == '\t')
                er->etype = ami_ettab; /* set tab */
            else if (inpevt->Event.KeyEvent.uChar.AsciiChar == 'C'-64)
                er->etype = ami_etterm; /* set end program */
            else if (inpevt->Event.KeyEvent.uChar.AsciiChar == 'S'-64)
                er->etype = ami_etstop; /* set stop program */
            else if (inpevt->Event.KeyEvent.uChar.AsciiChar == 'Q'-64)
                er->etype = ami_etcont; /* set continue program */
            else { /* normal character */

                er->etype = ami_etchar; /* set character event */
                er->echar = inpevt->Event.KeyEvent.uChar.AsciiChar;

            }
            *keep = 1; /* set keep event */

        } else switch (inpevt->Event.KeyEvent.wVirtualKeyCode) { /* key */

            case VK_HOME: /* home */
                if (alt(inpevt) && shift(inpevt)) er->etype = ami_ethome; /* home document */
                else if (alt(inpevt)) er->etype = ami_ethomes; /* home screen */
                else er->etype = ami_ethomel; /* home line */
                *keep = 1; /* set keep event */
                break;

            case VK_END: /* end */
                if (alt(inpevt) && shift(inpevt)) er->etype = ami_etend; /* end document */
                else if (alt(inpevt)) er->etype = ami_etends; /* end screen */
                else er->etype = ami_etendl; /* end line */
                *keep = 1; /* set keep event */
                break;

            case VK_UP: /* up */
                if (alt(inpevt)) er->etype = ami_etscru; /* scroll up */
                else er->etype = ami_etup; /* up line */
                *keep = 1; /* set keep event */
                break;

            case VK_DOWN: /* down */
                if (alt(inpevt)) er->etype = ami_etscrd; /* scroll down */
                else er->etype = ami_etdown; /* up line */
                *keep = 1; /* set keep event */
                break;

            case VK_LEFT: /* left */
                if (alt(inpevt) && shift(inpevt)) er->etype = ami_etscrl; /* scroll left one character */
                else if (alt(inpevt)) er->etype = ami_etleftw; /* left one word */
                else er->etype = ami_etleft; /* left one character */
                *keep = 1; /* set keep event */
                break;

            case VK_RIGHT: /* right */
                if (alt(inpevt) && shift(inpevt)) er->etype = ami_etscrr; /* scroll right one character */
                else if (alt(inpevt)) er->etype = ami_etrightw; /* right one word */
                else er->etype = ami_etright; /* left one character */
                *keep = 1; /* set keep event */
                break;

            case VK_INSERT: /* insert */
                if (cntrl(inpevt) && shift(inpevt)) er->etype = ami_etinsert; /* insert block */
                else if (cntrl(inpevt)) er->etype = ami_etinsertl; /* insert line */
                else er->etype = ami_etinsertt; /* insert toggle */
                *keep = 1; /* set keep event */
                break;

            case VK_DELETE: /* delete */
                if (cntrl(inpevt)) er->etype = ami_etdel; /* delete block */
                else if (shift(inpevt)) er->etype = ami_etdell; /* delete line */
                else er->etype = ami_etdelcf; /* delete character forward */
                *keep = 1; /* set keep event */
                break;

            case VK_PRIOR:
                er->etype = ami_etpagu; /* page up */
                *keep = 1; /* set keep event */
                break;

            case VK_NEXT:
                er->etype = ami_etpagd; /* page down */
                *keep = 1; /* set keep event */
                break;

            case VK_F1: /* f1 */
                if (cntrl(inpevt)) er->etype = ami_etcopy; /* copy block */
                else if (shift(inpevt)) er->etype = ami_etcopyl; /* copy line */
                else { /* f1 */

                    er->etype = ami_etfun;
                    er->fkey = 1;

                }
                *keep = 1; /* set keep event */
                break;

            case VK_F2: /* f2 */
                if (cntrl(inpevt)) er->etype = ami_etprintb; /* print block */
                else if (shift(inpevt)) er->etype = ami_etprint; /* print document */
                else { /* f2 */

                    er->etype = ami_etfun;
                    er->fkey = 2;

                }
                *keep = 1; /* set keep event */
                break;

            case VK_F3: /* f3 */
                if (cntrl(inpevt)) er->etype = ami_etprints; /* print screen */
                else { /* f3 */

                    er->etype = ami_etfun;
                    er->fkey = 3;

                }
                *keep = 1; /* set keep event */
                break;

            case VK_F4: /* f4 */
                er->etype = ami_etfun;
                er->fkey = 4;
                *keep = 1; /* set keep event */
                break;

            case VK_F5: /* f5 */
                er->etype = ami_etfun;
                er->fkey = 5;
                *keep = 1; /* set keep event */
                break;

            case VK_F6: /* f6 */
                er->etype = ami_etfun;
                er->fkey = 6;
                *keep = 1; /* set keep event */
                break;

            case VK_F7: /* f7 */
                er->etype = ami_etfun;
                er->fkey = 7;
                *keep = 1; /* set keep event */
                break;

            case VK_F8: /* f8 */
                er->etype = ami_etfun;
                er->fkey = 8;
                *keep = 1; /* set keep event */
                break;

            case VK_F9: /* f9 */
                er->etype = ami_etfun;
                er->fkey = 9;
                *keep = 1; /* set keep event */
                break;

            case VK_F10: /* f10 */
                er->etype = ami_etfun;
                er->fkey = 10;
                *keep = 1; /* set keep event */
                break;

            case VK_F11: /* f11 */
                er->etype = ami_etfun;
                er->fkey = 11;
                *keep = 1; /* set keep event */
                break;

            case VK_F12: /* f12 */
                er->etype = ami_etfun;
                er->fkey = 12;
                *keep = 1; /* set keep event */
                break;

            case VK_MENU:
                er->etype = ami_etmenu; /* alt */
                *keep = 1; /* set keep event */
                break;

            case VK_CANCEL:
                er->etype = ami_etterm; /* ctl-brk */
                *keep = 1; /* set keep event */
                break;

        }

    }

}

/*

Process mouse event.
Buttons are assigned to Win 95 as follows:

button 1: Windows left button
button 2: Windows right button
button 3: Windows 2nd from left button
button 4: Windows 3rd from left button

The Windows 4th from left button is unused. The double click events will
} up, as, well, two clicks of a single button, displaying my utter
contempt for the whole double click concept.

*/

/* update mouse parameters */

static void mouseupdate(ami_evtptr er, int* keep)

{

    /* we prioritize events by: movements 1st, button clicks 2nd */
    if (nmpx != mpx || nmpy != mpy) { /* create movement event */

        er->etype = ami_etmoumov; /* set movement event */
        er->mmoun = 1; /* mouse 1 */
        er->moupx = nmpx; /* set new mouse position */
        er->moupy = nmpy;
        mpx = nmpx; /* save new position */
        mpy = nmpy;
        *keep = 1; /* set to keep */

    } else if (nmb1 > mb1) {

        er->etype = ami_etmouba; /* button 1 assert */
        er->amoun = 1; /* mouse 1 */
        er->amoubn = 1; /* button 1 */
        mb1 = nmb1; /* update status */
        *keep = 1; /* set to keep */

    } else if (nmb2 > mb2) {

        er->etype = ami_etmouba; /* button 2 assert */
        er->amoun = 1; /* mouse 1 */
        er->amoubn = 2; /* button 2 */
        mb2 = nmb2; /* update status */
        *keep = 1; /* set to keep */

    } else if (nmb3 > mb3) {

        er->etype = ami_etmouba; /* button 3 assert */
        er->amoun = 1; /* mouse 1 */
        er->amoubn = 3; /* button 3 */
        mb3 = nmb3; /* update status */
        *keep = 1; /* set to keep */

    } else if (nmb4 > mb4) {

        er->etype = ami_etmouba; /* button 4 assert */
        er->amoun = 1; /* mouse 1 */
        er->amoubn = 4; /* button 4 */
        mb4 = nmb4; /* update status */
        *keep = 1; /* set to keep */

    } else if (nmb1 < mb1) {

        er->etype = ami_etmoubd; /* button 1 deassert */
        er->dmoun = 1; /* mouse 1 */
        er->dmoubn = 1; /* button 1 */
        mb1 = nmb1; /* update status */
        *keep = 1; /* set to keep */

    } else if (nmb2 < mb2) {

        er->etype = ami_etmoubd; /* button 2 deassert */
        er->dmoun = 1; /* mouse 1 */
        er->dmoubn = 2; /* button 2 */
        mb2 = nmb2; /* update status */
        *keep = 1; /* set to keep */

    } else if (nmb3 < mb3) {

        er->etype = ami_etmoubd; /* button 3 deassert */
        er->dmoun = 1; /* mouse 1 */
        er->dmoubn = 3; /* button 3 */
        mb3 = nmb3; /* update status */
        *keep = 1; /* set to keep */

    } else if (nmb4 < mb4) {

        er->etype = ami_etmoubd; /* button 4 deassert */
        er->dmoun = 1; /* mouse 1 */
        er->dmoubn = 4; /* button 4 */
        mb4 = nmb4; /* update status */
        *keep = 1; /* set to keep */

    }

}

/* register mouse status */

static void mouseevent(INPUT_RECORD* inpevt)

{

    /* gather a new mouse status */
    nmpx = inpevt->Event.MouseEvent.dwMousePosition.X+1; /* get mouse position */
    nmpy = inpevt->Event.MouseEvent.dwMousePosition.Y+1;
    nmb1 = !!(inpevt->Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED);
    nmb2 = !!(inpevt->Event.MouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED);
    nmb3 = !!(inpevt->Event.MouseEvent.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED);
    nmb4 = !!(inpevt->Event.MouseEvent.dwButtonState & FROM_LEFT_3RD_BUTTON_PRESSED);

}

/* issue event for changed button */

static void updn(ami_evtptr er, INPUT_RECORD* inpevt, int* keep, int bn, int bm)

{

    /* Note that if there are multiple up/down bits, we only register the last
      one. Windows is ambiguous as to if it will combine such events */
    if (!(inpevt->Event.KeyEvent.wVirtualKeyCode & bm)) { /* assert */

        er->etype = ami_etjoyba; /* set assert */
        if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1BUTTONDOWN ||
            inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1BUTTONUP) er->ajoyn = 1;
        else er->ajoyn = 2;
        er->ajoybn = bn; /* set number */

    } else { /* deassert */

        er->etype = ami_etjoybd; /* set deassert */
        if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1BUTTONDOWN ||
            inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1BUTTONUP) er->ajoyn = 1;
        else er->ajoyn = 2;
        er->djoybn = bn; /* set number */

    }
    *keep = 1; /* set keep event */

}

/* process joystick messages */

static void joymes(ami_evtptr er, INPUT_RECORD* inpevt, int* keep)

{

    /* register changes on each button */
    if (inpevt->Event.KeyEvent.wVirtualKeyCode & JOY_BUTTON1CHG)
        updn(er, inpevt, keep, 1, JOY_BUTTON1);
    if (inpevt->Event.KeyEvent.wVirtualKeyCode & JOY_BUTTON2CHG)
        updn(er, inpevt, keep, 2, JOY_BUTTON2);
    if (inpevt->Event.KeyEvent.wVirtualKeyCode & JOY_BUTTON3CHG)
        updn(er, inpevt, keep, 3, JOY_BUTTON3);
    if (inpevt->Event.KeyEvent.wVirtualKeyCode & JOY_BUTTON4CHG)
        updn(er, inpevt, keep, 4, JOY_BUTTON4);

}

/* process custom events */

static void custevent(ami_evtptr er, INPUT_RECORD* inpevt, int* keep)

{

    int x, y, z;    /* joystick readback */
    int dx, dy, dz; /* joystick readback differences */

    if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_TIM) { /* timer event */

        if (inpevt->Event.KeyEvent.wVirtualKeyCode == FRMTIM) er->etype = ami_etframe;
        else {

            er->etype = ami_ettim; /* set timer event occurred */
            /* set what timer */
            er->timnum = inpevt->Event.KeyEvent.wVirtualKeyCode;

        }
        *keep = 1; /* set to keep */

    } else if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1MOVE ||
               inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1ZMOVE ||
               inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY2MOVE ||
               inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY2ZMOVE) {

        /* joystick move */
        er->etype = ami_etjoymov; /* set joystick moved */
        /* set what joystick */
        if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1MOVE ||
            inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1ZMOVE) er->mjoyn = 1;
        else er->mjoyn = 2;
        /* Set all variables to default to same. This way, only the joystick
          axes that are actually set by the message are registered. */
        if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1MOVE ||
            inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1ZMOVE) {

            x = joy1xs;
            y = joy1ys;
            z = joy1zs;

        } else {

            x = joy2xs;
            y = joy2ys;
            z = joy2zs;

        }
        /* If it's an x/y move, split the x and y axies parts of the message
          up. */
        if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1MOVE ||
            inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY2MOVE) {

            /* get x and y positions */
            x = inpevt->Event.KeyEvent.wVirtualKeyCode;
            y = inpevt->Event.KeyEvent.wVirtualScanCode;

        } else /* get z position */
            z = inpevt->Event.KeyEvent.wVirtualKeyCode;
        /* We perform thresholding on the joystick right here, which is
          limited to 255 steps (same as joystick hardware. find joystick
          diffs and update */
        if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1MOVE ||
            inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1ZMOVE) {

            dx = abs(joy1xs-x); /* find differences */
            dy = abs(joy1ys-y);
            dz = abs(joy1zs-z);
            joy1xs = x; /* place old values */
            joy1ys = y;
            joy1zs = z;

        } else {

            dx = abs(joy2xs-x); /* find differences */
            dy = abs(joy2ys-y);
            dz = abs(joy2zs-z);
            joy2xs = x; /* place old values */
            joy2ys = y;
            joy2zs = z;

        }
        /* now reject moves below the threshold */
        if (dx > 65535 / 255 || dy > 65535 / 255 ||
            dz > 65535 / 255) {

            /* scale axies between -INT_MAX..INT_MAX and place */
            er->joypx = (x - 32767)*(INT_MAX / 32768);
            er->joypy = (y - 32767)*(INT_MAX / 32768);
            er->joypz = (z - 32767)*(INT_MAX / 32768);
            *keep = 1; /* set keep event */

        }

    } else if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1BUTTONDOWN ||
               inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY2BUTTONDOWN ||
               inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY1BUTTONUP ||
               inpevt->Event.KeyEvent.dwControlKeyState == UIV_JOY2BUTTONUP)
        joymes(er, inpevt, keep);
    else if (inpevt->Event.KeyEvent.dwControlKeyState == UIV_TERM) {

        er->etype = ami_etterm; /* set end program */
        *keep = 1; /* set keep event */

    }

}

static void ievent(ami_evtptr er)

{

    int          keep;   /* event keep flag */
    BOOL         b;      /* int return value */
    DWORD        ne;     /* number of events */
    INPUT_RECORD inpevt; /* event read buffer */
    int          x, y, oy;
    int          ssy;

    do {

        keep = FALSE; /* set don't keep by default */
        mouseupdate(er, &keep); /* check any mouse details need processing */
        if (!keep) { /* no, go ahead with event read */

            b = ReadConsoleInput(inphdl, &inpevt, 1, &ne); /* get the next event */
            if (!b) winerr(); /* stop on fail */
            if (ne) { /* process valid event */

                /* decode by event */
                if (inpevt.EventType == KEY_EVENT) {

                    if (inpevt.Event.KeyEvent.dwControlKeyState >= UIV_BASE)
                        custevent(er, &inpevt, &keep); /* process our custom event */
                    else
                        keyevent(er, &inpevt, &keep); /* key event */

                } else if (inpevt.EventType == MOUSE_EVENT)
                    mouseevent(&inpevt); /* mouse event */
                else if (inpevt.EventType == WINDOW_BUFFER_SIZE_EVENT) {

                    er->etype = ami_etresize; /* set resize */
                    keep = TRUE; /* set keep event */
                    b = GetConsoleScreenBufferInfo(screens[curupd-1]->han, &bi);
                    /* Compensate for windows scrollback buffer by placing us in
                       the display area */
                    ssy = bi.srWindow.Bottom-bi.srWindow.Top+1; /* find displayed y size */
                    x = bi.dwSize.X; /* place maximum sizes */
                    y = ssy; /* set y is displayed only */
                    oy = bi.dwSize.Y-ssy; /* then set offset to area */
                    if (screens[curupd-1]->maxx != x || screens[curupd-1]->maxy != y) {

                        /* filter out any messages with no net change. This was
                           seen commonly. */
                        screens[curupd-1]->maxx = x; /* place maximum sizes */
                        screens[curupd-1]->maxy = y; /* set y is displayed only */
                        screens[curupd-1]->offy = oy; /* then set offset to area */

                    } else keep = FALSE; /* otherwise no event */

                }


            }

        }

    } while (!keep); /* until an event we want occurs */

} /* event */

void ami_event(FILE* f, ami_evtptr er)

{

    do { /* loop handling via event vectors */

        /* get next input event */
        ievent(er);
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

Overrides or "hooks" the indicated event handler. The existing even handler is
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

Timer handler procedure

Called when the windows multimedia event timer expires. We prepare a message
to send up to the console even handler. Since the console event system does not
have user defined messages, this is done by using a key event with an invalid
control key code.

*******************************************************************************/

static void CALLBACK timeout(UINT id, UINT msg, DWORD_PTR usr, DWORD_PTR dw1, DWORD_PTR dw2)

{

    INPUT_RECORD inpevt; /* windows event record array */
    DWORD ne;  /* number of events written */

    /* we mux this into special key fields */
    inpevt.EventType = KEY_EVENT; /* set key event type */
    inpevt.Event.KeyEvent.dwControlKeyState = UIV_TIM; /* set timer code */
    inpevt.Event.KeyEvent.wVirtualKeyCode = usr; /* set timer handle */
    WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

}

/*******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.
Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it s}s a timer mature event to
the associated input file.

*******************************************************************************/

static void itimer(int i, /* timer handle */
            long t, /* number of tenth-milliseconds to run */
            int r) /* timer is to rerun after completion */

{

    UINT tf; /* timer flags */
    UINT mt; /* millisecond time */

    mt = t / 10; /* find millisecond time */
    if (mt == 0) mt = 1; /* fell below minimum, must be >= 1 */
    /* set flags for timer */
    tf = TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS;
    /* set repeat/one shot status */
    if (r) tf = tf | TIME_PERIODIC;
    else tf = tf | TIME_ONESHOT;
    timers[i].han = timeSetEvent(mt, 0, timeout, i, tf);
    timers[i].rep = r; /* set timer repeat flag */
    /* should check and return an error */

}

void ami_timer(FILE* f, int i, long t, int r)

{

    itimer(i, t, r); /* set timer */

}

/*******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void ami_killtimer(FILE* f, /* file to kill timer on */
               int   i) /* handle of timer */

{

    MMRESULT r; /* return value */

    r = timeKillEvent(timers[i].han); /* kill timer */
    /* should check for return error */

}

/*******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

static void iframetimer(int e)

{

    int r;

    if (e) { /* enable timer */

        if (!frmrun) { /* it is not running */

            /* set timer to run, 17ms */
            frmhan = timeSetEvent(17, 0, timeout, FRMTIM,
                                  TIME_CALLBACK_FUNCTION |
                                  TIME_KILL_SYNCHRONOUS |
                                  TIME_PERIODIC);
            if (!frmhan) error(etimacc); /* error */
            frmrun = 1; /* set timer running */

        }

    } else { /* disable framing timer */

        if (frmrun) { /* it is currently running */

            r = timeKillEvent(frmhan); /* kill timer */
            if (r) error(etimacc); /* error */
            frmrun = 0; /* set timer not running */

        }

    }

}

void ami_frametimer(FILE* f, int e)

{

    iframetimer(e); /* execute */

}

/*******************************************************************************

Return number of mice

Returns the number of mice implemented. Windows is fixed to one mouse, but
I suspect they will eventually have to remove this limit.

*******************************************************************************/

int ami_mouse(FILE* f)

{

    return (1); /* set single mouse */

}

/*******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

*******************************************************************************/

int ami_mousebutton(FILE* f, int m)

{

    if (m != 1) error(einvhan); /* bad mouse number */

    return (nummbt); /* set 3 buttons */

}

/*******************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int ami_joystick(FILE* f)

{

    return (numjoy); /* two */

}

/*******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

static int ijoybutton(int j)

{

    JOYCAPS jc; /* joystick characteristics data */
    int nb;     /* number of buttons */
    int r;      /* return value */

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */
    r = joyGetDevCaps(j-1, &jc, sizeof(JOYCAPS));
    if (r) error(ejoyqry); /* could not access joystick */
    nb = jc.wNumButtons; /* set number of buttons */
    /* We don't support more than 4 buttons. */
    if (nb > 4) nb = 4;

    return (nb); /* set number of buttons */

}

int ami_joybutton(FILE* f, int j)

{

    return (ijoybutton(j));

}

/*******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

int ami_joyaxis(FILE* f, int j)

{

    JOYCAPS jc; /* joystick characteristics data */
    int axis;   /* number of axes */
    MMRESULT r;      /* return value */

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */
    r = joyGetDevCaps(j-1, &jc, sizeof(JOYCAPS));
    if (r) error(ejoyqry); /* could not access joystick */
    axis = jc.wNumAxes; /* set number of axes */
    /* We don't support more than 4 buttons. */
    if (axis > 3) axis = 3;

    return (axis); /* 2d */

}

/*******************************************************************************

Set tab

Sets a tab. The tab number t is 1 to n, and indicates the column for the tab.
Setting a tab stop means that when a tab is received, it will move to the next
tab stop that is set. If there is no next tab stop, nothing will happen.

*******************************************************************************/

void ami_settab(FILE* f, int t)

{

    if (t < 1 || t > screens[curupd-1]->maxx)
        error(einvtab); /* bad tab position */
    screens[curupd-1]->tab[t-1] = 1; /* place tab at that position */

}

/*******************************************************************************

Reset tab

Resets a tab. The tab number t is 1 to n, and indicates the column for the tab.

*******************************************************************************/

void ami_restab(FILE* f, int t)

{

    if (t < 1 || t > screens[curupd-1]->maxx)
        error(einvtab); /* bad tab position */
    screens[curupd-1]->tab[t-1] = 0; /* clear tab at that position */

}

/*******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void ami_clrtab(FILE* f)

{

    int i;

    for (i = 0; i < screens[curupd-1]->maxx; i++) screens[curupd-1]->tab[i] = 0;

}

/*******************************************************************************

Find number of function keys

Finds the total number of int, or general assignment keys. Currently, we
just implement the 12 unshifted PC int keys. We may add control and shift
int keys as well.

*******************************************************************************/

int ami_funkey(FILE* f)

{

    return (12); /* number of function keys */

}

/** ****************************************************************************

Autohold

Turns on or off automatic hold mode.

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from terminal.
This exists to allow the results of terminal unaware programs to be viewed after
termination, instead of exiting and clearing the screen. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by CTL-C. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding terminal unaware programs.

Windows console does not have or need autohold because it has "transparent" 
mode. This means that terminal unaware programs operate just as other terminal
programs, and write on the main screen (screen 1). Note this is a critical
difference between Windows and Liux/MacOS, because transparent mode is
specifically disallowed there.

*******************************************************************************/

void ami_autohold(int e)

{

    /* no function */

}

/** ****************************************************************************

Write string direct with length

Writes a string with length direct to the terminal, bypassing character
handling.

*******************************************************************************/

void ami_wrtstrn(FILE* f, char *s, int n)

{

    WORD   ab;  /* attribute output buffer */
    DWORD  len; /* length dummy */
    scnptr sc;  /* screen context pointer */
    COORD  xy;

    sc = screens[curupd-1];
    while (n > 0) {

        if (icurbnd(sc)) { /* cursor in bounds */

            ab = sc->sattr; /* place attribute in buffer */
            /* write character */
            xy.X = sc->curx-1;
            xy.Y = sc->cury+sc->offy-1;
            WriteConsoleOutputCharacter(sc->han, s, 1, xy, &len);
            WriteConsoleOutputAttribute(sc->han, &ab, 1, xy, &len);

        }
        iright(); /* move cursor right */
        s++; /* next character */
        n--;

    }

}

/** ****************************************************************************

Write string direct

Writes a string direct to the terminal, bypassing character handling.

*******************************************************************************/

void ami_wrtstr(FILE* f, char *s)

{

    ami_wrtstrn(f, s, strlen(s));

}

/** ****************************************************************************

Size buffer

Sets or resets the size of the buffer surface.

*******************************************************************************/

void ami_sizbuf(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void ami_title(FILE* f, char* ts)
    
{ 

    int r;

    r = SetConsoleTitle(ts);
    if (!r) winerr();
    
}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into the
input line buffer.

*******************************************************************************/

static void readline(void)

{

    ami_evtrec er;   /* event record */
    scnptr    sc;   /* pointer to current screen */
    int       ins;  /* insert/overwrite mode */
    int       xoff; /* x starting line offset */
    int       l;    /* buffer length */
    int       i;

    sc = screens[curupd-1]; /* index current screen */
    inpptr = 0; /* set 1st character position */
    inpbuf[0] = 0; /* terminate line */
    ins = 1;
    xoff = sc->curx; /* save starting line offset */
    do { /* get line characters */

        ievent(&er); /* get next event */
        switch (er.etype) { /* event */

            case ami_etterm: exit(1); /* halt program */
            case ami_etenter: /* line terminate */
                while (inpbuf[inpptr]) inpptr++; /* advance to end */
                inpbuf[inpptr] = '\n'; /* return newline */
                /* terminate the line for debug prints */
                inpbuf[inpptr+1] = 0;
                plcchr('\r'); /* output newline sequence */
                plcchr('\n');
                break;
            case ami_etchar: /* character */
                if (inpptr < MAXLIN-2) {

                    if (ins) { /* insert */

                        i = inpptr; /* find end */
                        while (inpbuf[i]) i++;
                        /* move line up */
                        while (inpptr <= i) { inpbuf[i+1] = inpbuf[i]; i--; }
                        inpbuf[inpptr] = er.echar; /* place new character */
                        /* reprint line */
                        i = inpptr;
                        while (inpbuf[i]) plcchr(inpbuf[i++]);
                        /* back up */
                        i = inpptr;
                        while (inpbuf[i++]) plcchr('\b');
                        /* forward and next char */
                        plcchr(inpbuf[inpptr]);
                        inpptr++;

                    } else { /* overwrite */

                        /* if end, move end marker */
                        if (!inpbuf[inpptr]) inpbuf[inpptr+1] = 0;
                        inpbuf[inpptr] = er.echar; /* place new character */
                        /* forward and next char */
                        plcchr(inpbuf[inpptr]);
                        inpptr++;

                    }

                }
                break;
            case ami_etdelcb: /* delete character backwards */
                if (inpptr > 0) { /* not at extreme left */

                    inpptr--; /* back up pointer */
                    /* move characters back */
                    i = inpptr;
                    while (inpbuf[i]) { inpbuf[i] = inpbuf[i+1]; i++; }
                    plcchr('\b'); /* move cursor back */
                    /* repaint line */
                    i = inpptr;
                    while (inpbuf[i]) plcchr(inpbuf[i++]);
                    plcchr(' '); /* blank last */
                    /* back up */
                    plcchr('\b');
                    i = inpptr;
                    while (inpbuf[i++]) plcchr('\b');

                }
                break;
            case ami_etdelcf: /* delete character forward */
                if (inpbuf[inpptr]) { /* not at extreme right */

                    /* move characters down */
                    i = inpptr;
                    while (inpbuf[i]) { inpbuf[i] = inpbuf[i+1]; i++; }
                    /* repaint right */
                    i = inpptr;
                    while (inpbuf[i]) plcchr(inpbuf[i++]);
                    plcchr(' '); /* blank last */
                    /* back up */
                    plcchr('\b');
                    i = inpptr;
                    while (inpbuf[i++]) plcchr('\b');

                }
                break;
            case ami_etright: /* right character */
                /* not at extreme right, go right */
                if (inpbuf[inpptr]) {

                    plcchr(inpbuf[inpptr]);
                    inpptr++; /* advance input */

                }
                break;

            case ami_etleft: /* left character */
                /* not at extreme left, go left */
                if (inpptr > 0) {

                    plcchr('\b');
                    inpptr--; /* back up pointer */

                }
                break;

            case ami_etmoumov: /* mouse moved */
                /* we can track this internally */
                break;

            case ami_etmouba: /* mouse click */
                if (er.amoubn == 1) {

                    l = strlen(inpbuf);
                    if (sc->cury == nmpy && xoff <= nmpx && xoff+l >= nmpx) {

                        /* mouse position is within buffer space, set
                           position */
                        icursor(nmpx, sc->cury);
                        inpptr = nmpx-xoff;

                    }

                }
                break;

            case ami_ethomel: /* beginning of line */
                /* back up to start of line */
                while (inpptr) {

                    plcchr('\b');
                    inpptr--;

                }
                break;

            case ami_etendl: /* end of line */
                /* go to end of line */
                while (inpbuf[inpptr]) {

                    plcchr(inpbuf[inpptr]);
                    inpptr++;

                }
                break;

            case ami_etinsertt: /* toggle insert mode */
                ins = !ins; /* toggle insert mode */
                break;

            case ami_etdell: /* delete whole line */
                /* back up to start of line */
                while (inpptr) {

                    plcchr('\b');
                    inpptr--;

                }
                /* erase line on screen */
                while (inpbuf[inpptr]) {

                    plcchr(' ');
                    inpptr++;

                }
                /* back up again */
                while (inpptr) {

                    plcchr('\b');
                    inpptr--;

                }
                inpbuf[inpptr] = 0; /* clear line */
                break;

            case ami_etleftw: /* left word */
                /* back over any spaces */
                while (inpptr && inpbuf[inpptr-1] == ' ') {

                    plcchr('\b');
                    inpptr--;

                }
                /* now back over any non-space */
                while (inpptr && inpbuf[inpptr-1] != ' ') {

                    plcchr('\b');
                    inpptr--;

                }
                break;

            case ami_etrightw: /* right word */
                /* advance over any non-space */
                while (inpbuf[inpptr] && inpbuf[inpptr] != ' ') {

                    plcchr(inpbuf[inpptr]);
                    inpptr++;

                }
                /* advance over any spaces */
                while (inpbuf[inpptr] && inpbuf[inpptr] == ' ') {

                    plcchr(inpbuf[inpptr]);
                    inpptr++;

                }
                break;

            default: ;

        }

    } while (er.etype != ami_etenter); /* until line terminate */
    inpptr = 0; /* set 1st position on active line */

}

/*******************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
unlink
lseek

We use interdiction to filter standard I/O calls towards the terminal. The
0 (input) and 1 (output) files are interdicted. In ANSI terminal, we act as a
filter, so this does not change the user ability to redirect the file handles
elsewhere.

*******************************************************************************/

/*******************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

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

    return (*ofpclose)(fd);

}

/*******************************************************************************

Unlink

Unlink has nothing to do with us, so we just pass it on.

*******************************************************************************/

static int iunlink(const char* pathname)

{

    return (*ofpunlink)(pathname);

}

/*******************************************************************************

Lseek

Lseek is never possible on a terminal, so this is always an error on the stdin
or stdout handle.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    /* check seeking on terminal attached file (input or output) and error
       if so */
    if (fd == INPFIL || fd == OUTFIL) error(efilopr);

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

static ssize_t iread(int fd, void* buff, size_t count)

{

    ssize_t rc; /* return code */
    char *p = (char *)buff;
    size_t cnt = count;

    if (fd == INPFIL) {

        /* get data from terminal */
        while (cnt) {

            /* if there is no line in the input buffer, get one */
            if (inpptr == -1) readline();
            *p = inpbuf[inpptr]; /* get and place next character */
            if (inpptr < MAXLIN) inpptr++; /* next */
            /* if we have just read the last of that line, then flag buffer
               empty */
            if (*p == '\n') inpptr = -1;
            p++; /* next character */
            cnt--; /* count characters */

        }
        rc = count; /* set return same as count */

    } else rc = (*ofpread)(fd, buff, count);

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

    ssize_t rc; /* return code */
    char *p = (char *)buff;
    size_t cnt = count;

    if (fd == OUTFIL) {

        /* send data to terminal */
        while (cnt--) plcchr(*p++);
        rc = count; /* set return same as count */

    } else rc = (*ofpwrite)(fd, buff, count);

    return rc;

}

/*******************************************************************************

Windows class handler

Handles messages for the dummy window. The purpose of this routine is to relay
timer and joystick messages back to the main console queue.

*******************************************************************************/

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)

{

    int          b;       /* result holder */
    int          r;       /* result holder */
    DWORD        ne;      /* number of events written */
    int          x, y, z;
    INPUT_RECORD inpevt;  /* event buffer */

    if (msg == WM_CREATE) {

        r = 0;

    } else if (msg == MM_JOY1MOVE) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY1MOVE;
        x = LOWORD(lparam); /* get x and y for joystick */
        y = HIWORD(lparam);
        inpevt.Event.KeyEvent.wVirtualKeyCode = x; /* place */
        inpevt.Event.KeyEvent.wVirtualScanCode = y; /* place */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == MM_JOY1ZMOVE) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY1ZMOVE;
        z = LOWORD(lparam); /* get z for joystick */
        inpevt.Event.KeyEvent.wVirtualKeyCode = z; /* place */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == MM_JOY2MOVE) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY2MOVE;
        x = LOWORD(lparam); /* get x and y for joystick */
        y = HIWORD(lparam);
        inpevt.Event.KeyEvent.wVirtualKeyCode = x; /* place */
        inpevt.Event.KeyEvent.wVirtualScanCode = y; /* place */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == MM_JOY2ZMOVE) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY2ZMOVE;
        z = LOWORD(lparam); /* get z for joystick */
        inpevt.Event.KeyEvent.wVirtualKeyCode = z; /* place */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == MM_JOY1BUTTONDOWN) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY1BUTTONDOWN;
        inpevt.Event.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == MM_JOY2BUTTONDOWN) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY2BUTTONDOWN;
        inpevt.Event.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == MM_JOY1BUTTONUP) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY1BUTTONUP;
        inpevt.Event.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == MM_JOY2BUTTONUP) {

        inpevt.EventType = KEY_EVENT; /* set key event type */
        /* set joystick move code */
        inpevt.Event.KeyEvent.dwControlKeyState = UIV_JOY2BUTTONUP;
        inpevt.Event.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    } else if (msg == WM_DESTROY) {

        PostQuitMessage(0);
        r = 0;

    } else r = DefWindowProc(hwnd, msg, wparam, lparam);

    return (r);

}

/*******************************************************************************

Window handler task

Timers, joysticks and other toys won't work unless they have a window with
full class handling to send messages to. So we create an "invisible" window
by creating a window that never gets presented. The timer and other messages
are sent to that, then the windows void for that forwards them via special
queue messages back to the console input queue.

This void gets its own thread, and is fairly self contained. It starts
and runs the dummy window. The following variables are used to communicate
with the main thread:

winhan          Gives the window to send timer messages to.

threadstart     Flags that the thread has started. We can't set up events for
                the dummy task until it starts. This is a hard wait, I am
                searching for a better method.

timers          Contains the "repeat" flag for each timer. There can be race
                conditions for this, it needs to be studied. Plus, we can use
                some of Windows fancy lock mechanisims.

numjoy          Sets the number of joysticks. This is valid after wait for
                task start.

*******************************************************************************/

DWORD WINAPI dummyloop(LPVOID par)

{

    MSG msg;
    WNDCLASSA wc; /* windows class structure */
    int b;        /* int return */
    int r;        /* result holder */
    JOYCAPS  jc;  /* joytick capabilities */
    MMRESULT mmr; /* result code */

    /* there are a few resources that can only be used by windowed programs, such
      as timers and joysticks. to enable these, we start a false windows
      void with a window that is never presented */
    /* set windows class to a normal window without scroll bars,
      with a windows void pointing to the message mirror.
      The message mirror reflects messages that should be handled
      by the program back into the queue, s}ing others on to
      the windows default handler */
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "stdwin";
    /* register that class */
    b = RegisterClass(&wc);
    /* create the window */
    winhan = CreateWindowEx(
                     0, "StdWin", "Dummy", WS_OVERLAPPEDWINDOW,
                     CW_USEDEFAULT, CW_USEDEFAULT,
                     CW_USEDEFAULT, CW_USEDEFAULT,
                     0, 0, GetModuleHandleA(NULL), NULL
            );
    /* capture joysticks */
    r = joySetCapture(winhan, JOYSTICKID1, 33, 0);
    if (!r) numjoy++; /* count */
    r = joySetCapture(winhan, JOYSTICKID2, 33, 0);
    if (!r) numjoy = numjoy+1; /* count */
    /* flag subthread has started up */
    threadstart = 1; /* set we started */
    /* message handling loop */
    while (GetMessage(&msg, 0, 0, 0) > 0) { /* not a quit message */

        b = TranslateMessage(&msg); /* translate keyboard events */
        r = DispatchMessage(&msg);

    }
    /* release the joysticks */
    r = joyReleaseCapture(JOYSTICKID1);
    r = joyReleaseCapture(JOYSTICKID2);

    return (0); /* return ok (unused) */

}

/*******************************************************************************

Console control handler

This procedure gets activated as a callback when Windows flags a termination
event. We send a termination message back to the event queue, and flag the
event handled.
At the present time, we don't care what type of termination event it was,
all generate an etterm signal.

*******************************************************************************/

static BOOL WINAPI conhan(DWORD ct)

{

    DWORD        ne;     /* number of events written */
    INPUT_RECORD inpevt; /* event buffer */

    /* we mux this into special key fields */
    inpevt.EventType = KEY_EVENT; /* set key event type */
    inpevt.Event.KeyEvent.dwControlKeyState = UIV_TERM; /* set timer code */
    WriteConsoleInput(inphdl, &inpevt, 1, &ne); /* send */

    return (1); /* set event handled */

}

/** ****************************************************************************

Initialize output terminal

We initialize all variables and tables, then clear the screen to bring it to
a known state.

This is the startup routine for terminal, and is executed automatically
before the client program runs.

*******************************************************************************/

static void ami_init_terminal (void) __attribute__((constructor (102)));
static void ami_init_terminal(void)

{

    HANDLE h;
    int    ssy;
    BOOL   b;
    ami_evtcod e;

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    /* get handle of input file */
    inphdl = GetStdHandle(STD_INPUT_HANDLE);
    mb1 = 0; /* set mouse as assumed no buttons down, at origin */
    mb2 = 0;
    mb3 = 0;
    mb4 = 0;
    mpx = 1;
    mpy = 1;
    nmb1 = 0;
    nmb2 = 0;
    nmb3 = 0;
    nmb4 = 0;
    nmpx = 1;
    nmpy = 1;
    joy1xs = 0; /* clear joystick saves */
    joy1ys = 0;
    joy1zs = 0;
    joy2xs = 0;
    joy2ys = 0;
    joy2zs = 0;
    numjoy = 0; /* set number of joysticks 0 */
    inpptr = -1; /* set no input line active */
    frmrun = 0; /* set framing timer not running */
    /* clear timer repeat array */
    for (ti = 1; ti <= 10; ti++) {

        timers[ti].han = 0; /* set no active timer */
        timers[ti].rep = 0; /* set no repeat */

    }

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_etframe; e++) evthan[e] = defaultevent;

    /* clear screen context table */
    for (cix = 0; cix < MAXCON; cix++) screens[cix] = NULL;
    /* get the default screen */
    screens[0] = malloc(sizeof(scncon));
    if (!screens[0]) error(enomem); /* no memory */
    curdsp = 1; /* set current display screen */
    curupd = 1; /* set current update screen */
    /* point handle to present output screen buffer */
    screens[curupd-1]->han = GetStdHandle(STD_OUTPUT_HANDLE);
    b = GetConsoleScreenBufferInfo(screens[curupd-1]->han, &bi);
    /* Compensate for windows scrollback buffer by placing us in the display
       area */
#if 0
dbg_printf(dlinfo, "Display area: left: %d top: %d bottom: %d right: %d cursor: x: %d y: %d\n",
        bi.srWindow.Left, bi.srWindow.Top, bi.srWindow.Bottom, bi.srWindow.Right,
        bi.dwCursorPosition.X, bi.dwCursorPosition.Y);
#endif
    ssy = bi.srWindow.Bottom-bi.srWindow.Top+1; /* find displayed y size */
    screens[curupd-1]->maxx = bi.dwSize.X; /* place maximum sizes */
    screens[curupd-1]->maxy = ssy; /* set y is displayed only */
    screens[curupd-1]->offy = bi.srWindow.Top; /* then set offset to area */
    screens[curupd-1]->curx = bi.dwCursorPosition.X+1; /* place cursor position */
    screens[curupd-1]->cury = bi.dwCursorPosition.Y-bi.srWindow.Top+1;
    screens[curupd-1]->sattr = bi.wAttributes; /* place default attributes */
    /* place max setting for all screens */
    gmaxx = screens[curupd-1]->maxx;
    gmaxy = screens[curupd-1]->maxy;
    screens[curupd-1]->autof = 1; /* turn on auto scroll and wrap */
    gautof = 1;
    fndcolor(screens[curupd-1]->sattr); /* set colors from that */
    gforec = screens[curupd-1]->forec;
    gbackc = screens[curupd-1]->backc;
    b = GetConsoleCursorInfo(screens[curupd-1]->han, &ci); /* get cursor mode */
    screens[curupd-1]->curv = ci.bVisible != 0; /* set cursor visible from that */
    gcurv = ci.bVisible != 0;
    screens[curupd-1]->attr = sanone; /* set no attribute */
    gattr = sanone;
    /* set up tabbing to be on each 8th position */
    for (i = 0; i < screens[curupd-1]->maxx; i++)
        screens[curupd-1]->tab[i] = (i) % 8 == 0;
    /* turn on mouse events */
    GetConsoleMode(inphdl, &mode);
    mode &= ~ENABLE_QUICK_EDIT_MODE; /* enable the mouse */
    SetConsoleMode(inphdl, mode | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS );
    /* save previous output mode and stop wrap modes */
    GetConsoleMode(screens[curupd-1]->han, &cmodes);
    mode = cmodes;
    mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    //mode &= ~DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(screens[curupd-1]->han, mode);
    /* capture control handler */
    SetConsoleCtrlHandler(conhan, TRUE);
    /* find number of mouse buttons */
    nummbt = GetSystemMetrics(SM_CMOUSEBUTTONS);
    if (nummbt > 4) nummbt = 4; /* limit the number of buttons to 4 */
    /* interlock to make sure that thread starts before we continue */
    threadstart = 0;
    h = CreateThread(NULL, 0, dummyloop, NULL, 0, &threadid);
    while (!threadstart); /* wait for thread to start */

}

/** ****************************************************************************

Deinitialize output terminal

Removes overrides. We check if the contents of the override vector have our
vectors in them. If that is not so, then a stacking order violation occurred,
and that should be corrected.

*******************************************************************************/

static void ami_deinit_terminal (void) __attribute__((destructor (102)));
static void ami_deinit_terminal(void)

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose /* || cppunlink != iunlink */ || cpplseek != ilseek)
        error(esystem);
    /* restore previous wrapping behavior */
    SetConsoleMode(screens[curupd-1]->han, cmodes);
    /* release control handler */
    SetConsoleCtrlHandler(NULL, FALSE);

}
