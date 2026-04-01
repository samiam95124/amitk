/** ****************************************************************************
*                                                                              *
*                                 Petit AMI                                    *
*                                                                              *
* xterm/ANSI console interface                                                 *
*                                                                              *
* This is a standard PA/TK terminal module using ANSI control codes, some      *
* of which are specific to various VT10x terminals and xterm which emulates    *
* them. Its mainly for xterm and compatibles, which means Linux and Mac OS X.  *
*                                                                              *
* Uses ANSI C and a good bit of POSIX. The stdio interface is done by a        *
* specially modified library that includes the ability to hook or override     *
* the bottom level of I/O.                                                     *
*                                                                              *
* The module works by keeping a in memory image of the output terminal and     *
* its attributes, along the lines of what curses does. Because it always knows *
* what the state of the actual terminal should be, it does not need to read    *
* from the terminal to determine the state of individual character cells.      *
*                                                                              *
* In this version, the file argument is not used.                              *
*                                                                              *
* The ANSI interface is mainly useful in Linux/BSD because the ANSI controls   *
* are standardized there, and serial connections are more widely used          *
* (like SSH). Curses is also used, but it, too, is typically just a wrapper    *
* for ANSI controls, since the wide variety of different serial terminals from *
* the 1970s and before have died off (which perhaps shows that one way to      *
* standardize the world is to get a smaller world).                            *
*                                                                              *
* The ANSI driver really has two modes: one when used as a local program, and  *
* another when used remotely via serial connection, telnet, ssh or similar     *
* program. In the latter case, the joystick position is irrelevant, because    *
* there is, at this writing, no remote method to read the joystick.            *
*                                                                              *
* Terminal can also be used on Linux that boots to the console. This is either *
* a Linux that boots up without XWindows, or by switching to an alternate      *
* console. This may or may not support a mouse.                                *
*                                                                              *
* Petit-Ami is a standard that goes back to a start in 1984 with significant   *
* improvements in the 1997 and on years. It was the library standard for       *
* Pascaline, but I translated it to C.                                         *
*                                                                              *
* The first version of this package implemented the call set on a Wyse 80      *
* terminal.                                                                    *
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

/* linux definitions */
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X or BSD */
#include <linux/joystick.h>
#endif
#include <fcntl.h>

/* Petit-Ami definitions */
#include <localdefs.h>
#include <config.h>
#include <terminal.h>
#include "system_event.h"

#include <diag.h>

/* external definitions */
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X or BSD */
extern char *program_invocation_short_name;
#endif

/*
 * Configurable parameters
 *
 * These parameters can be configured here at compile time, or are overriden
 * at runtime by values of the same name in the config files.
 * The values are overriddable.
 */
#ifndef JOYENB
#define JOYENB   TRUE /* enable joysticks */
#endif

#ifndef MOUSEENB
#define MOUSEENB TRUE /* enable mouse */
#endif

/* Default terminal size sets the geometry of the terminal if we cannot find
   out the geometry from the terminal itself. */
#ifndef DEFXD
#define DEFXD 80 /* default terminal size, 80x24, this is Linux standard */
#endif

#ifndef DEFYD
#define DEFYD 24
#endif

/* Set unresponsive timer and present message and state if the program has not
   serviced the event queue */
#ifndef UNRESPONSE
#define UNRESPONSE TRUE /* check non-responsive program */
#endif

/* Allow the user to force terminate an unesposive program */
#ifndef UNRESPONSEKILL
#define UNRESPONSEKILL TRUE /* terminate non-responsive program */
#endif

/*
 * Use xterm title function
 *
 * Use the xterm/ANSI title function, or use flashing title bar for autohold
 */
#ifndef XTERMTITLE
#define XTERMTITLE TRUE /* enable xterm title */
#endif

#define MAXKEY    20          /* maximum length of key sequence */
#define MAXCON    10          /**< number of screen contexts */
#define MAXLIN    250         /* maximum length of input buffered line */
#define MAXFKEY   10          /**< maximum number of function keys */
#define MAXJOY    10          /* number of joysticks possible */
#define DMPEVT    FALSE       /* enable dump Petit-Ami messages */
#define SECOND    10000       /* 1 second time (using 100us timer */
#define ALLOWUTF8             /* enable UTF-8 encoding */
#define HOVERTIME (1*SECOND)  /* hover timeout, 1 second */ 
#define RESPTIME  (15*SECOND) /* default response time limit */

/*
 * Standard mouse decoding has a limit of about 223 in x or y. SGR mode
 * can go from 1 to 2015.
 */
#define MOUSESGR      /* use SGR mouse mode (extended positions) */

/*
 * Use 24 bit color encoding. This was used to get pure white in an Xterm, which
 * normally does not appear to be possible. Note this only applies to convertion
 * of standard primary colors to screen colors.
 */
#define COLOR24

/*
 * Use 24 bit color encoding throughout. This means that colors are stored in 24
 * bit rgb form, and presented on the screen in that form. It also sets the
 * COLOR24 flag.
 */
#define NATIVE24

/* file handle numbers at the system interface level */

#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* Foreground and background color bases.
   The "normal" ANSI base gives dull colors on Linux, Windows and probably the
   Mac as well (untested). On linux the AIX colors give bright, and on Windows
   blink gives bright (apparently since blink is not implemented there). This
   was considered a non-issue since we use the Windows console mode driver
   instead of this driver.
   Note that dull colors are mainly an issue for "paper white" background
   programs because dull white looks different from every other window on the
   system. */
#define AIXTERM 1 /* set aix terminal colors active */

#ifdef AIXTERM

#define ANSIFORECOLORBASE 30 /* ansi override base */
#define ANSIBACKCOLORBASE 40

#define FORECOLORBASE 90 /* aixterm */
#define BACKCOLORBASE 100

#else

#define ANSIFORECOLORBASE 30 /* ansi override base */
#define ANSIBACKCOLORBASE 40

#define FORECOLORBASE 30 /* ansi */
#define BACKCOLORBASE 40

#endif

#define APIOVER(name) void _pa_##name##_ovr(_pa_##name##_t nfp, _pa_##name##_t* ofp) \
                      { *ofp = name##_vect; name##_vect = nfp; }

#define EVTOVER(name) \
APIOVER(name##over) \
void ami_##name##over(ami_ev##name##_t eh, ami_ev##name##_t* oeh) { (*name##over_vect)(eh, oeh); } \
static void name##over_ivf(ami_ev##name##_t eh, ami_ev##name##_t* oeh) { \
    dbg_printf(dlapi, "API\n"); \
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */ \
    *oeh = ev##name##_vect; /* save existing event handler */ \
    ev##name##_vect = eh; /* place new event handler */ \
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */ \
}

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
    /* bold text */                  sabold

} scnatt;

/** single character on screen container. note that not all the attributes
   that appear here can be changed */
typedef struct {

#ifdef ALLOWUTF8
    /* character at location */        unsigned char ch[4]; /* encoded utf-8 */
#else
    /* character at location */        unsigned char ch;
#endif
#ifdef NATIVE24
    /* foreground color at location */ int forergb;
    /* background color at location */ int backrgb;
#else
    /* foreground color at location */ ami_color forec;
    /* background color at location */ ami_color backc;
#endif
    /* active attribute at location */ scnatt attr;

} scnrec, *scnptr;

/* macro to access screen elements by y,x */
#define SCNBUF(sc, x, y) (sc[(y-1)*bufx+(x-1)])

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

/* PA queue structure. Its a bubble list. */
typedef struct paevtque {

    struct paevtque* next; /* next in list */
    struct paevtque* last; /* last in list */
    ami_evtrec       evt;  /* event data */

} paevtque;

/*
 * keyboard key equivalents table
 *
 * Contains equivalent strings as are returned from xterm keys
 * attached to an IBM-PC keyboard, or for special codes xterm sends.
 *
 * Note these definitions are mosly CUA (common user interface). One exception
 * was the terminate key, which has a long tradition as CTRL-C, and I left it.
 *
 * In xterm the home and end keys return the same regardless of their shift,
 * control or alt status. Some of the CUA keys may not be available simply
 * because the GUI intercepts them. For example, print screen, insert and
 * similar keys. Thus we need a xterm equivalent, and we use alternative keys.
 *
 */

char *keytab[ami_etterm+1+MAXFKEY] = {

    /* Common controls are:
    Codes                   Meaning                   IBM-PC keyboard equivalents */
    "",                     /* ANSI character returned */
    "\33\133\101",          /* cursor up one line         (up arrow) */
    "\33\133\102",          /* down one line              (down arrow) */
    "\33\133\104",          /* left one character         (left arrow) */
    "\33\133\103",          /* right one character        (right arrow) */
    "\33\133\61\73\65\104", /* left one word              (ctrl-left arrow)*/
    "\33\133\61\73\65\103", /* right one word             (ctrl-right arrow) */
    "\33\133\61\73\65\110", /* home of document           (ctrl-home) */
    "\10",                  /* home of screen             (ctrl-h) */
    "\33\133\110",          /* home of line               (home) */
    "\33\133\61\73\65\106", /* end of document            (ctrl-end) */
    "\5",                   /* end of screen              (ctrl-e) */
    "\33\133\106",          /* end of line                (end) */
    "\33\133\65\73\65\176", /* scroll left one character  (ctrl-page up) */
    "\33\133\66\73\65\176", /* scroll right one character (ctrl-page down)  */
    "\33\133\61\73\65\102", /* scroll up one line         (ctrl-up arrow) */
    "\33\133\61\73\65\101", /* scroll down one line       (ctrl-down arrow) */
    "\33\133\66\176",       /* page down                  (page down) */
    "\33\133\65\176",       /* page up                    (page up) */
    "\11",                  /* tab                        (tab) */
    "\15",                  /* enter line                 (enter) */
    "\26",                  /* insert block               (ctrl-v) */
    "",                     /* insert line */
    "\33\133\62\176",       /* insert toggle              (insert) */
    "\33\133\63\73\62\176", /* delete block               (shift-del) */
    "\33\133\63\73\65\176", /* delete line                (ctrl-del) */
    "\33\133\63\176",       /* delete character forward   (del) */
    "\177",                 /* delete character backward  (backspace) */
    "\33\143",              /* copy block                 (alt-c) */
    "",                     /* copy line */
    "\33\33",               /* cancel current operation   (esc esc) */
    "\23",                  /* stop current operation     (ctrl-s) */
    "\21",                  /* continue current operation (ctrl-q) */
    "\20",                  /* print document             (ctrl-p) */
    "",                     /* print block */
    "",                     /* print screen */
    "",                     /* function key */
    "",                     /* display menu */
    "",                     /* mouse button assertion */
    "",                     /* mouse button deassertion */
    /* mouse move is just the leader for the mouse move/assert message. The
       characters are read in the input handler. */
#ifdef MOUSESGR
    "\33[<",                /* mouse move */
#else
    "\33[M",                /* mouse move */
#endif
    "",                     /* timer matures */
    "",                     /* joystick button assertion */
    "",                     /* joystick button deassertion */
    "",                     /* joystick move */
    "",                     /* window resize */
    "\33[I",                /* focus in */
    "\33[O",                /* focus out */
    "",                     /* hover */
    "",                     /* no hover */
    "\3",                   /* terminate program           (ctrl-c) */
    /* we added the Fx key codes to the end here */
    "\33\117\120",          /* F1 */
    "\33\117\121",          /* F2 */
    "\33\117\122",          /* F3 */
    "\33\117\123",          /* F4 */
    "\33\133\61\65\176",    /* F5 */
    "\33\133\61\67\176",    /* F6 */
    "\33\133\61\70\176",    /* F7 */
    "\33\133\61\71\176",    /* F8 */
    "\33\133\62\60\176",    /* F9 */
    /* F12 is a "pseudo 10th" key in that I wanted to preserve the PA tradition
       of giving 10 function keys, so I reassigned the last one, since F10 is
       taken by xterm (I'm sure its a CUA thing) */
    "\33\133\62\64\176"     /* F12 */

};

#ifdef ALLOWUTF8
/*
 * bit count table for UTF-8
 */
unsigned char utf8bits[] = {

    0, /* 0000 */
    1, /* 0001 */
    1, /* 0010 */
    2, /* 0011 */
    1, /* 0100 */
    2, /* 0101 */
    2, /* 0110 */
    3, /* 0111 */
    1, /* 1000 */
    2, /* 1001 */
    2, /* 1010 */
    3, /* 1011 */
    2, /* 1100 */
    3, /* 1101 */
    3, /* 1110 */
    4  /* 1111 */

};
#endif

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static punlink_t ofpunlink;
static plseek_t  ofplseek;

/*
 * Override vectors for calls in this package
 *
 */
static _pa_cursor_t      cursor_vect;
static _pa_maxx_t        maxx_vect;
static _pa_maxy_t        maxy_vect;
static _pa_home_t        home_vect;
static _pa_del_t         del_vect;
static _pa_up_t          up_vect;
static _pa_down_t        down_vect;
static _pa_left_t        left_vect;
static _pa_right_t       right_vect;
static _pa_blink_t       blink_vect;
static _pa_reverse_t     reverse_vect;
static _pa_underline_t   underline_vect;
static _pa_superscript_t superscript_vect;
static _pa_subscript_t   subscript_vect;
static _pa_italic_t      italic_vect;
static _pa_bold_t        bold_vect;
static _pa_strikeout_t   strikeout_vect;
static _pa_standout_t    standout_vect;
static _pa_fcolor_t      fcolor_vect;
static _pa_bcolor_t      bcolor_vect;
static _pa_fcolorc_t     fcolorc_vect;
static _pa_bcolorc_t     bcolorc_vect;
static _pa_auto_t        auto_vect;
static _pa_curvis_t      curvis_vect;
static _pa_scroll_t      scroll_vect;
static _pa_curx_t        curx_vect;
static _pa_cury_t        cury_vect;
static _pa_curbnd_t      curbnd_vect;
static _pa_select_t      select_vect;
static _pa_event_t       event_vect;
static _pa_timer_t       timer_vect;
static _pa_killtimer_t   killtimer_vect;
static _pa_mouse_t       mouse_vect;
static _pa_mousebutton_t mousebutton_vect;
static _pa_joystick_t    joystick_vect;
static _pa_joybutton_t   joybutton_vect;
static _pa_joyaxis_t     joyaxis_vect;
static _pa_settab_t      settab_vect;
static _pa_restab_t      restab_vect;
static _pa_clrtab_t      clrtab_vect;
static _pa_funkey_t      funkey_vect;
static _pa_frametimer_t  frametimer_vect;
static _pa_autohold_t    autohold_vect;
static _pa_wrtstr_t      wrtstr_vect;
static _pa_wrtstrn_t     wrtstrn_vect;
static _pa_eventover_t   eventover_vect;
static _pa_eventsover_t  eventsover_vect;
static _pa_sendevent_t   sendevent_vect;
static _pa_title_t       title_vect;
static _pa_titlen_t      titlen_vect;
static _pa_openwin_t     openwin_vect;
static _pa_buffer_t      buffer_vect;
static _pa_sizbuf_t      sizbuf_vect;
static _pa_getsiz_t      getsiz_vect;
static _pa_setsiz_t      setsiz_vect;
static _pa_setpos_t      setpos_vect;
static _pa_scnsiz_t      scnsiz_vect;
static _pa_scncen_t      scncen_vect;
static _pa_winclient_t   winclient_vect;
static _pa_front_t       front_vect;
static _pa_back_t        back_vect;
static _pa_frame_t       frame_vect;
static _pa_sizable_t     sizable_vect;
static _pa_sysbar_t      sysbar_vect;
static _pa_menu_t        menu_vect;
static _pa_menuena_t     menuena_vect;
static _pa_menusel_t     menusel_vect;
static _pa_stdmenu_t     stdmenu_vect;
static _pa_getwinid_t    getwinid_vect;
static _pa_focus_t       focus_vect;

/*
 * Event override vectors
 */
static ami_evchar_t    evchar_vect;
static ami_evup_t      evup_vect;
static ami_evdown_t    evdown_vect;
static ami_evleft_t    evleft_vect;
static ami_evright_t   evright_vect;
static ami_evleftw_t   evleftw_vect;
static ami_evrightw_t  evrightw_vect;
static ami_evhome_t    evhome_vect;
static ami_evhomes_t   evhomes_vect;
static ami_evhomel_t   evhomel_vect;
static ami_evend_t     evend_vect;
static ami_evends_t    evends_vect;
static ami_evendl_t    evendl_vect;
static ami_evscrl_t    evscrl_vect;
static ami_evscrr_t    evscrr_vect;
static ami_evscru_t    evscru_vect;
static ami_evscrd_t    evscrd_vect;
static ami_evpagd_t    evpagd_vect;
static ami_evpagu_t    evpagu_vect;
static ami_evtab_t     evtab_vect;
static ami_eventer_t   eventer_vect;
static ami_evinsert_t  evinsert_vect;
static ami_evinsertl_t evinsertl_vect;
static ami_evinsertt_t evinsertt_vect;
static ami_evdel_t     evdel_vect;
static ami_evdell_t    evdell_vect;
static ami_evdelcf_t   evdelcf_vect;
static ami_evdelcb_t   evdelcb_vect;
static ami_evcopy_t    evcopy_vect;
static ami_evcopyl_t   evcopyl_vect;
static ami_evcan_t     evcan_vect;
static ami_evstop_t    evstop_vect;
static ami_evcont_t    evcont_vect;
static ami_evprint_t   evprint_vect;
static ami_evprintb_t  evprintb_vect;
static ami_evprints_t  evprints_vect;
static ami_evfun_t     evfun_vect;
static ami_evmenu_t    evmenu_vect;
static ami_evmouba_t   evmouba_vect;
static ami_evmoubd_t   evmoubd_vect;
static ami_evmoumov_t  evmoumov_vect;
static ami_evtim_t     evtim_vect;
static ami_evjoyba_t   evjoyba_vect;
static ami_evjoybd_t   evjoybd_vect;
static ami_evjoymov_t  evjoymov_vect;
static ami_evresize_t  evresize_vect;
static ami_evfocus_t   evfocus_vect;
static ami_evnofocus_t evnofocus_vect;
static ami_evhover_t   evhover_vect;
static ami_evnohover_t evnohover_vect;
static ami_evterm_t    evterm_vect;
static ami_evframe_t   evframe_vect;

/*
 * Event override override vectors
 */
static _pa_charover_t    charover_vect;
static _pa_upover_t      upover_vect;
static _pa_downover_t    downover_vect;
static _pa_leftover_t    leftover_vect;
static _pa_rightover_t   rightover_vect;
static _pa_leftwover_t   leftwover_vect;
static _pa_rightwover_t  rightwover_vect;
static _pa_homeover_t    homeover_vect;
static _pa_homesover_t   homesover_vect;
static _pa_homelover_t   homelover_vect;
static _pa_endover_t     endover_vect;
static _pa_endsover_t    endsover_vect;
static _pa_endlover_t    endlover_vect;
static _pa_scrlover_t    scrlover_vect;
static _pa_scrrover_t    scrrover_vect;
static _pa_scruover_t    scruover_vect;
static _pa_scrdover_t    scrdover_vect;
static _pa_pagdover_t    pagdover_vect;
static _pa_paguover_t    paguover_vect;
static _pa_tabover_t     tabover_vect;
static _pa_enterover_t   enterover_vect;
static _pa_insertover_t  insertover_vect;
static _pa_insertlover_t insertlover_vect;
static _pa_inserttover_t inserttover_vect;
static _pa_delover_t     delover_vect;
static _pa_dellover_t    dellover_vect;
static _pa_delcfover_t   delcfover_vect;
static _pa_delcbover_t   delcbover_vect;
static _pa_copyover_t    copyover_vect;
static _pa_copylover_t   copylover_vect;
static _pa_canover_t     canover_vect;
static _pa_stopover_t    stopover_vect;
static _pa_contover_t    contover_vect;
static _pa_printover_t   printover_vect;
static _pa_printbover_t  printbover_vect;
static _pa_printsover_t  printsover_vect;
static _pa_funover_t     funover_vect;
static _pa_menuover_t    menuover_vect;
static _pa_moubaover_t   moubaover_vect;
static _pa_moubdover_t   moubdover_vect;
static _pa_moumovover_t  moumovover_vect;
static _pa_timover_t     timover_vect;
static _pa_joybaover_t   joybaover_vect;
static _pa_joybdover_t   joybdover_vect;
static _pa_joymovover_t  joymovover_vect;
static _pa_resizeover_t  resizeover_vect;
static _pa_focusover_t   focusover_vect;
static _pa_nofocusover_t nofocusover_vect;
static _pa_hoverover_t   hoverover_vect;
static _pa_nohoverover_t nohoverover_vect;
static _pa_termover_t    termover_vect;
static _pa_frameover_t   frameover_vect;
/**
 * Save for terminal status
 */
static struct termios trmsav;

/**
 * Active timers table
 */

static pthread_mutex_t timlock; /* lock for timer table */
static int timtbl[AMI_MAXTIM];
static int frmsev; /* frame timer system event number */
/* end of timlock region */

/*
 * Input event parsing
 */

/*
 * Key matching input buffer.
 *
 * Note mouse also comes in as input keys.
 */
static unsigned char keybuf[MAXKEY]; /* buffer */
static int    keylen;      /* number of characters in buffer */
/* current tracking states of mouse */
static int    button1;     /* button 1 state: 0=assert, 1=deassert */
static int    button2;     /* button 2 state: 0=assert, 1=deassert */
static int    button3;     /* button 3 state: 0=assert, 1=deassert */
static int    mpx;         /* mouse x/y current position */
static int    mpy;
/* new, incoming states of mouse */
static int    nbutton1;    /* button 1 state: 0=assert, 1=deassert */
static int    nbutton2;    /* button 2 state: 0=assert, 1=deassert */
static int    nbutton3;    /* button 3 state: 0=assert, 1=deassert */
static int    nmpx;        /* mouse x/y current position */
static int    nmpy;

static pthread_mutex_t termlock;        /* broadlock for terminal calls */
static scnrec* screens[MAXCON];         /* screen contexts array */ 
static int curdsp;                      /* index for current display screen */ 
static int curupd;                      /* index for current update screen */ 
static ami_pevthan evthan[ami_etframe+1]; /* array of event handler routines */ 
static ami_pevthan evtshan;              /* single master event handler routine */
static ami_errhan error_vect;            /* PA error handler override */
static _pa_linuxerrhan linuxerror_vect;  /* Linux system error handler override */

static int*     tabs;        /* tabs set */
static int      dimx;        /* actual width of screen */
static int      dimy;        /* actual height of screen */
static int      bufx, bufy;  /* buffer size */
static int      curon;       /* current on/off state of cursor */
static int      curx;        /* cursor position on screen */
static int      cury;
static int      ncurx;       /* new cursor position on screen */
static int      ncury;
static int      curval;      /* physical cursor position valid */
static int      curvis;      /* current status of cursor visible */
static ami_color forec;       /* current writing foreground color primaries */
static int      forergb;     /* foreground color in RGB */
static ami_color backc;       /* current writing background color primaries */
static int      backrgb;     /* background color in RGB */
static scnatt   attr;        /* current writing attribute */
/* global scroll enable. This does not reflect the physical state, we never
   turn on automatic scroll. */
static int    scroll;
static int    hover;       /* current state of hover */
static int    hovsev;      /* hover timer event */
static int    blksev;      /* finish blink event */
static int    respsev;     /* response check event */
static int    respto;      /* response has timed out */
static int    fend;        /* end of program ordered flag */
static int    fautohold;   /* automatic hold on exit flag */
static int    errflg;      /* error occurred */

/* maximum power of 10 in integer */
static int    maxpow10;
static int    numjoy;         /* number of joysticks found */

static int    frmfid;         /* framing timer fid */
volatile char inpbuf[MAXLIN]; /* input line buffer */
volatile int  inpptr;         /* input line index */
static joyptr joytab[MAXJOY]; /* joystick control table */
static int    dmpevt;         /* enable dump Petit-Ami messages */
static int    inpsev;         /* keyboard input system event number */
static int    winchsev;       /* windows change system event number */
#ifdef ALLOWUTF8
static int    utf8cnt;        /* UTF-8 extended character count */
#endif
static char*  titsav;         /* save for title string */

/*
 * Configurable parameters
 */
static int    joyenb;         /* enable joysticks */
static int    mouseenb;       /* enable mouse */
static int    unresponse;     /* check non-responsive program */
static int    unresponsekill; /* enable kill of non-responsive programs */
static int    xtermtitle;     /* use xterm title bar set mode */

static paevtque*       paqfre;      /* free PA event queue entries list */
static paevtque*       paqevt;      /* PA event input save queue */
static pthread_t       eventthread; /* thread handle for event thread */
static pthread_mutex_t evtlck;      /* event queue lock */
static pthread_cond_t  evtquene;    /* semaphore: event queue not empty */
static int             evtquecnt;   /* number of entries in event queue */
static int             evtquemax;   /* high water mark for event queue */
static int             matrem;      /* matching entries removed */

/** ****************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.

*******************************************************************************/

void ami_errorover(ami_errhan nfp, ami_errhan* ofp)
    { *ofp = error_vect; error_vect = nfp; }
static void error(ami_errcod e) { (*error_vect)(e); }
static void error_ivf(ami_errcod e)

{

    fprintf(stderr, "*** Error: xterm: ");
    switch (e) { /* error */

        case ami_dispeftbful: fprintf(stderr, "Too many files"); break;
        case ami_dispejoyacc: fprintf(stderr, "No joystick access available"); break;
        case ami_dispetimacc: fprintf(stderr, "No timer access available"); break;
        case ami_dispefilopr: fprintf(stderr, "Cannot perform operation on special file"); break;
        case ami_dispeinvpos: fprintf(stderr, "Invalid screen position"); break;
        case ami_dispefilzer: fprintf(stderr, "Filename is empty"); break;
        case ami_dispeinvscn: fprintf(stderr, "Invalid screen number"); break;
        case ami_dispeinvhan: fprintf(stderr, "Invalid file handle"); break;
        case ami_dispeinvthn: fprintf(stderr, "Invalid timer handle"); break;
        case ami_dispemouacc: fprintf(stderr, "No mouse access available"); break;
        case ami_dispeoutdev: fprintf(stderr, "Error in output device"); break;
        case ami_dispeinpdev: fprintf(stderr, "Error in input device"); break;
        case ami_dispeinvtab: fprintf(stderr, "Invalid tab stop position"); break;
        case ami_dispeinvjoy: fprintf(stderr, "Invalid joystick ID"); break;
        case ami_dispecfgval: fprintf(stderr, "Invalid configuration value"); break;
        case ami_dispenomem: fprintf(stderr, "Out of memory"); break;
        case ami_dispesendevent_unimp: fprintf(stderr, "sendevent unimplemented"); break;
        case ami_dispeopenwin_unimp: fprintf(stderr, "openwin unimplemented"); break;
        case ami_dispebuffer_unimp: fprintf(stderr, "buffer unimplemented"); break;
        case ami_dispesizbuf_unimp: fprintf(stderr, "sizbuf unimplemented"); break;
        case ami_dispegetsiz_unimp: fprintf(stderr, "getsiz unimplemented"); break;
        case ami_dispesetsiz_unimp: fprintf(stderr, "setsiz unimplemented"); break;
        case ami_dispesetpos_unimp: fprintf(stderr, "setpos unimplemented"); break;
        case ami_dispescnsiz_unimp: fprintf(stderr, "scnsiz unimplemented"); break;
        case ami_dispescncen_unimp: fprintf(stderr, "scncen unimplemented"); break;
        case ami_dispewinclient_unimp: fprintf(stderr, "winclient unimplemented"); break;
        case ami_dispefront_unimp: fprintf(stderr, "front unimplemented"); break;
        case ami_dispeback_unimp: fprintf(stderr, "back unimplemented"); break;
        case ami_dispeframe_unimp: fprintf(stderr, "frame unimplemented"); break;
        case ami_dispesizable_unimp: fprintf(stderr, "sizable unimplemented"); break;
        case ami_dispesysbar_unimp: fprintf(stderr, "sysbar unimplemented"); break;
        case ami_dispemenu_unimp: fprintf(stderr, "menu unimplemented"); break;
        case ami_dispemenuena_unimp: fprintf(stderr, "menuena unimplemented"); break;
        case ami_dispemenusel_unimp: fprintf(stderr, "menusel unimplemented"); break;
        case ami_dispestdmenu_unimp: fprintf(stderr, "stdmenu unimplemented"); break;
        case ami_dispegetwinid_unimp: fprintf(stderr, "getwinid unimplemented"); break;
        case ami_dispefocus_unimp: fprintf(stderr, "focus unimplemented"); break;
        case ami_dispesystem: fprintf(stderr, "System fault"); break;

    }
    fprintf(stderr, "\n");
    errflg = TRUE; /* flag error occurred */

    exit(1);

}

/** ****************************************************************************

Print Linux error

Accepts a linux error code. Prints the error string and exits.

*******************************************************************************/

void _pa_linuxerrorover(_pa_linuxerrhan nfp, _pa_linuxerrhan* ofp)
    { *ofp = linuxerror_vect; linuxerror_vect = nfp; }
static void linuxerror(int ec) { (*linuxerror_vect)(ec); }
static void linuxerror_ivf(int ec)

{

    fprintf(stderr, "Linux error: %s\n", strerror(ec)); fflush(stderr);
    errflg = TRUE; /* flag error occurred */

    exit(1);

}

/******************************************************************************

Print event symbol

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

static void prtevtt(ami_evtcod e)

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
        case ami_etfocus:   fprintf(stderr, "etfocus"); break;
        case ami_etnofocus: fprintf(stderr, "etnofocus"); break;
        case ami_ethover:   fprintf(stderr, "ethover"); break;
        case ami_etnohover: fprintf(stderr, "etnohover"); break;
        case ami_etterm:    fprintf(stderr, "etterm"); break;
        case ami_etframe:   fprintf(stderr, "etframe"); break;

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

    fprintf(stderr, "PA Event: ");
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
        case ami_etfun: fprintf(stderr, ": key: %d", er->fkey); break;
        default: ;

    }

}

/*******************************************************************************

Get size

Finds the x-y window size from the input device. Note that if this is not
sucessful, the size remains unchanged.

*******************************************************************************/

static void findsize(int* x, int* y)

{

    /** window size record */          struct winsize ws;
    /** result code */                 int r;

    /* attempt to determine actual screen size */
    r = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    if (!r) {

        *x = ws.ws_col;
        *y = ws.ws_row;

    }

}

/** ****************************************************************************

Read character from input file

Reads a single character to from the input file. Used to read from the input file
directly.

On the input file, we can't use the override, because the select() call bypasses
it on input, and so we must as well.

*******************************************************************************/

static char getchr(void)

{

    char    c;  /* character read */
    ssize_t rc; /* return code */

    /* receive character to the next hander in the override chain */
    rc = (*ofpread)(INPFIL, &c, 1);
    // rc = read(INPFIL, &c, 1);
    if (rc != 1) error(ami_dispeinpdev); /* input device error */

    return c; /* return character */

}

/** ****************************************************************************

Write character to output file

Writes a single character to the output file. Used to write to the output file
directly.

Uses the write() override.

*******************************************************************************/

static void putchr(unsigned char c)

{

    ssize_t rc; /* return code */

    /* send character to the next hander in the override chain */
    rc = (*ofpwrite)(OUTFIL, &c, 1);

    if (rc != 1) error(ami_dispeoutdev); /* output device error */

}

/** ****************************************************************************

Write string to output file

Writes a string directly to the output file.

*******************************************************************************/

static void putstr(unsigned char *s)


{

    /** index for string */ int i;

    while (*s) putchr(*s++); /* output characters */

}

/** ****************************************************************************

Write string to output file signed

Writes a signed string directly to the output file. This is to stop clang
warnings.

*******************************************************************************/

static void putstrc(char *s)

{

    /** index for string */ int i;

    while (*s) putchr(*s++); /* output characters */

}

/** ****************************************************************************

Write n length string to output file

Writes a string directly to the output file of n length.

*******************************************************************************/

static void putnstr(unsigned char *s, int n)

{

    /** index for string */ int i;

    while (*s && n--) putchr(*s++); /* output characters */

}

/** ****************************************************************************

Write n length string to output file signed

Writes a string directly to the output file of n length. This is to stop clang
warnings.

*******************************************************************************/

static void putnstrc(char *s, int n)


{

    /** index for string */ int i;

    while (*s && n--) putchr(*s++); /* output characters */

}

/** ****************************************************************************

Write integer to output file

Writes a simple unsigned integer to the output file.

*******************************************************************************/

static void wrtint(int i)

{

    /* power holder */       int  p;
    /* digit holder */       char digit;
    /* leading digit flag */ int  leading;

    p = maxpow10; /* set maximum power of 10 */
    leading = 0; /* set no leading digit encountered */
    while (p != 0) { /* output digits */

        digit = i/p%10+'0'; /* get next digit */
        p = p/10; /* next digit */
        if (digit != '0' || p == 0) /* leading digit or beginning of number */
            leading = 1; /* set leading digit found */
        if (leading) putchr(digit); /* output digit */

    }

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

Note: This routine should be called within the event queue lock.

*******************************************************************************/

static paevtque* getpaevt(void)

{

    paevtque* p;

    if (paqfre) { /* there is a freed entry */

        p = paqfre; /* index top entry */
        paqfre = p->next; /* gap from list */

    } else p = (paevtque*)malloc(sizeof(paevtque));
    /* set invalid fields */
    p->next = NULL;
    p->last = NULL;
    p->evt.etype = ami_etsys;

    return (p);

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

Note: This routine should be called within the event queue lock.

*******************************************************************************/

static void putpaevt(paevtque* p)

{

    /* set invalid fields for use after free */
    p->last = NULL;
    p->evt.etype = ami_etsys;
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
    int       r;
    int       loopcnt = evtquecnt+10;

    r = pthread_mutex_lock(&evtlck); /* lock event queue */
    if (r) linuxerror(r);
    loopcnt = evtquecnt; /* set total queue entries */
    p = paqevt; /* index root entry */
    while (p) {

        prtevt(&p->evt); /* print this entry */
        fprintf(stderr, "\n"); fflush(stderr);
        p = p->next; /* link next */
        if (p == paqevt) p = NULL; /* end of queue, terminate */
        loopcnt--; /* count entries */
        if (loopcnt < 0) { /* too many iterations, halt */

            r = pthread_mutex_unlock(&evtlck); /* release event queue */
            if (r) linuxerror(r);
            error(ami_dispesystem); /* go error */

        }

    }
    r = pthread_mutex_unlock(&evtlck); /* release event queue */
    if (r) linuxerror(r);

}

/** ****************************************************************************

Remove queue duplicates

Removes any entries in the current queue that would be made redundant by the new
queue entry. Right now this consists only of mouse movements.

Should be called only within lock context.

*******************************************************************************/

static void remdupque(ami_evtrec* e)

{

    paevtque* p;
    paevtque* fp;

    if (paqevt) { /* the queue has content */

        p = paqevt; /* index first queue entry */
        do { /* run around the bubble */

            if ((e->etype == ami_etmoumov && p->evt.etype == ami_etmoumov &&
                 e->mmoun == p->evt.mmoun) ||
                (e->etype == ami_etresize && p->evt.etype == ami_etresize) ||
                (e->etype == ami_etjoymov && p->evt.etype == ami_etjoymov &&
                 e->mjoyn == p->evt.mjoyn)) {

                /* matching entry, remove */
                fp = p; /* set entry found */
                matrem++; /* count */
                if (p->next == p) {

                    paqevt = NULL; /* only one entry, clear list */
                    p = NULL; /* set no next entry */

                } else { /* other entries */

                    p->last->next = p->next; /* point last at current */
                    p->next->last = p->last; /* point current at last */
                    /* if removing the queue root, step to next */
                    if (paqevt == p) paqevt = p->next;
                    p = p->next; /* go next entry */

                }
                evtquecnt--; /* count entries in queue */
                putpaevt(fp); /* release queue entry to free */

            } else p = p->next; /* go next queue entry */

        } while (p && p != paqevt); /* not back at beginning */

    }

}

/** ****************************************************************************

Place PA event into input queue

*******************************************************************************/

static void enquepaevt(ami_evtrec* e)

{

    paevtque* p;
    int       r;

    r = pthread_mutex_lock(&evtlck); /* lock event queue */
    if (r) linuxerror(r);
    remdupque(e); /* remove any duplicates */
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
        r = pthread_cond_signal(&evtquene);
        if (r) linuxerror(r);

    }
    evtquecnt++; /* count entries in queue */
    /* set new high water mark */
    if (evtquecnt > evtquemax) evtquemax = evtquecnt;
    r = pthread_mutex_unlock(&evtlck); /* release event queue */
    if (r) linuxerror(r);

}

/** ****************************************************************************

Remove PA event from input queue

*******************************************************************************/

static void dequepaevt(ami_evtrec* e)

{

    paevtque* p;
    int       r;

    r = pthread_mutex_lock(&evtlck); /* lock event queue */
    if (r) linuxerror(r);
    /* if queue is empty, wait for not empty event */
    while (!paqevt) {

        r = pthread_cond_wait(&evtquene, &evtlck);
        if (r) {

            pthread_mutex_unlock(&evtlck); /* release event queue */
            linuxerror(r);

        }

    }
    /* we push TO next (current) and take FROM last (final) */
    p = paqevt->last; /* index final entry */
    if (p->next == p) paqevt = NULL; /* only one entry, clear list */
    else { /* other entries */

        p->last->next = p->next; /* point last at current */
        p->next->last = p->last; /* point current at last */

    }
    memcpy(e, &p->evt, sizeof(ami_evtrec)); /* copy out to caller */
    putpaevt(p); /* release queue entry to free */
    evtquecnt--; /* count entries in queue */
    r = pthread_mutex_unlock(&evtlck); /* release event queue */
    if (r) linuxerror(r);

}

/** *****************************************************************************

Translate colors code

Translates an independent to a terminal specific primary color code for an
ANSI compliant terminal..

*******************************************************************************/

static int colnum(ami_color c)

{

    /** numeric equivalent of color */ int n;

    /* translate color number */
    switch (c) { /* color */

        case ami_black:   n = 0; break;
        case ami_white:   n = 7; break;
        case ami_red:     n = 1; break;
        case ami_green:   n = 2; break;
        case ami_blue:    n = 4; break;
        case ami_cyan:    n = 6; break;
        case ami_yellow:  n = 3; break;
        case ami_magenta: n = 5; break;

    }

    return n; /* return number */

}

/******************************************************************************

Translate color code to rgb

Translates a primary color code to RGB colors.

******************************************************************************/

static void colnumrgb(ami_color c, int* r, int* g, int* b)

{

    int n;

    /* translate color number */
    switch (c) { /* color */

        case ami_black:     *r = 0x00; *g = 0x00; *b = 0x00; break;
        case ami_white:     *r = 0xff; *g = 0xff; *b = 0xff; break;
        case ami_red:       *r = 0xff; *g = 0x00; *b = 0x00; break;
        case ami_green:     *r = 0x00; *g = 0xff; *b = 0x00; break;
        case ami_blue:      *r = 0x00; *g = 0x00; *b = 0xff; break;
        case ami_cyan:      *r = 0x00; *g = 0xff; *b = 0xff; break;
        case ami_yellow:    *r = 0xff; *g = 0xff; *b = 0x00; break;
        case ami_magenta:   *r = 0xff; *g = 0x00; *b = 0xff; break;

    }

}

/******************************************************************************

Translate colors code to packed rgb

Translates an independent to a packed RGB color word.

******************************************************************************/

static int colnumrgbp(ami_color c)

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

    }

    return (n); /* return number */

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

/*******************************************************************************

Translate rgb to packed 24 bit color

Translates a ratioed INT_MAX graph color to packed 24 bit form, which is a 32
bit word with blue, green and red bytes.

*******************************************************************************/

static int rgb2rgbp(int r, int g, int b)

{

   return ((r/8388608)*65536+(g/8388608)*256+(b/8388608));

}

/** ****************************************************************************

Basic terminal controls

These routines control the basic terminal functions. They exist just to
encapsulate this information. All of these functions are specific to ANSI
compliant terminals.

ANSI is able to set more than one attribute at a time, but under windows 95
there are no two attributes that you can detect together ! This is because
win95 modifies the attributes quite a bit (there is no blink). This capability
can be replaced later if needed.

Other notes:

1. Underline only works on monochrome terminals. On color, it makes
the text turn blue.

2. On linux, gnome-terminal and xterm both do not also home the cursor on
a clear (as the ANSI spec says). We fake this by adding a specific cursor home.

*******************************************************************************/

/** clear screen and home cursor */
static void trm_clear(void) { putstrc("\33[2J\33[H"); }
/** home cursor */ static void trm_home(void) { putstrc("\33[H"); }
/** move cursor up */ static void trm_up(void) { putstrc("\33[A"); }
/** move cursor down */ static void trm_down(void) { putstrc("\33[B"); }
/** move cursor left */ static void trm_left(void) { putstrc("\33[D"); }
/** move cursor right */ static void trm_right(void) { putstrc("\33[C"); }
/** turn on blink attribute */ static void trm_blink(void) { putstrc("\33[5m"); }
/** turn on reverse video */ static void trm_rev(void) { putstrc("\33[7m"); }
/** turn on underline */ static void trm_undl(void) { putstrc("\33[4m"); }
/** turn on bold attribute */ static void trm_bold(void) { putstrc("\33[1m"); }
/** turn on italic attribute */ static void trm_ital(void) { putstrc("\33[3m"); }
/** turn off all attributes */
static void trm_attroff(void) { putstrc("\33[0m"); }
/** turn on cursor wrap */ static void trm_wrapon(void) { putstrc("\33[7h"); }
/** turn off cursor wrap */ static void trm_wrapoff(void) { putstrc("\33[7l"); }
/** turn off cursor */ static void trm_curoff(void) { putstrc("\33[?25l"); }
/** turn on cursor */ static void trm_curon(void) { putstrc("\33[?25h"); }

/** set foreground color in rgb */
static void trm_fcolorrgb(int rgb)

{

    putstrc("\33[38;2;");
    wrtint(rgb >> 16 & 0xff);
    putstrc(";");
    wrtint(rgb >> 8 & 0xff);
    putstrc(";");
    wrtint(rgb & 0xff);
    putstrc("m");

}

/** set background color in rgb */
static void trm_bcolorrgb(int rgb)

{

    putstrc("\33[48;2;");
    wrtint(rgb >> 16 & 0xff);
    putstrc(";");
    wrtint(rgb >> 8 & 0xff);
    putstrc(";");
    wrtint(rgb & 0xff);
    putstrc("m");

}

/** set foreground color */
static void trm_fcolor(ami_color c)

{

#if defined(COLOR24) || defined(NATIVE24)
    int r, g, b;

    colnumrgb(c, &r, &g, &b); /* get rgb equivalent color */
    putstrc("\33[38;2;");
    wrtint(r);
    putstrc(";");
    wrtint(g);
    putstrc(";");
    wrtint(b);
    putstrc("m");
#else
    putstrc("\33[");
    /* override "bright" black, which is more like grey */
    if (c == ami_black) wrtint(ANSIFORECOLORBASE+colnum(c));
    else wrtint(FORECOLORBASE+colnum(c));
    putstrc("m");
#endif

}

/** set background color */
static void trm_bcolor(ami_color c)

{

#if defined(COLOR24) || defined(NATIVE24)
    int r, g, b;

    colnumrgb(c, &r, &g, &b); /* get rgb equivalent color */
    putstrc("\33[48;2;");
    wrtint(r);
    putstrc(";");
    wrtint(g);
    putstrc(";");
    wrtint(b);
    putstrc("m");
#else
    putstrc("\33[");
    /* override "bright" black, which is more like grey */
    if (c == ami_black) wrtint(ANSIBACKCOLORBASE+colnum(c));
    else wrtint(BACKCOLORBASE+colnum(c));
    putstrc("m");
#endif

}

/** position cursor */
static void trm_cursor(int x, int y)

{

    putstrc("\33[");
    wrtint(y);
    putstrc(";");
    wrtint(x);
    putstrc("H");

}

/** set title */
static void trm_title(char* title)

{

    putstrc("\33]0;");
    putstrc(title);
    putstrc("\7");

}

/** set title with length */
static void trm_titlen(char* title, int l)

{

    putstrc("\33]0;");
    putnstrc(title, l);
    putstrc("\7");

}

/** ****************************************************************************

Check in display

Check that the given screen context is currently being displayed.

*******************************************************************************/

static int indisp(scnptr sc)

{

    return sc == screens[curdsp-1];

}

/** ****************************************************************************

Set attribute from attribute code

Accepts a "universal" attribute code, and executes the attribute set required
to make that happen on screen. A few of these don't work on ANSI terminals,
including superscript and subscript.

*******************************************************************************/

static void setattr(scnptr sc, scnatt a)

{

    if (indisp(sc)) { /* in display */

        switch (a) { /* attribute */

            case sanone:  trm_attroff(); break; /* no attribute */
            case sablink: trm_blink();   break; /* blinking text (foreground) */
            case sarev:   trm_rev();     break; /* reverse video */
            case saundl:  trm_undl();    break; /* underline */
            case sasuper: break;                /* superscript */
            case sasubs:  break;                /* subscripting */
            case saital:  trm_ital();    break; /* italic text */
            case sabold:  trm_bold();    break; /* bold text */

        }
        /* attribute off may change the colors back to "normal" (normal for that
           particular implementation), apparently to remove reverse video. So we
           need to restore colors in this case, since PA/TK preserves colors. */
        if (a == sanone) {

#ifdef NATIVE24
            trm_fcolorrgb(forergb); /* set current colors */
            trm_bcolorrgb(backrgb);
#else
            trm_fcolor(forec); /* set current colors */
            trm_bcolor(backc);
#endif

        }

    }

}


/*******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns true if so.

*******************************************************************************/

int icurbnd(scnptr sc)

{

   return (ncurx >= 1) && (ncurx <= dimx) && (ncury >= 1) && (ncury <= dimy);

}

/*******************************************************************************

Set cursor status

Sets the cursor visible or invisible. If the cursor is out of bounds, it is
invisible regardless. Otherwise, it is visible according to the state of the
current buffer's visible status.

Should suppress redundant visibility sets here.

*******************************************************************************/

static void cursts(scnptr sc)

{

    int cv;

    if (indisp(sc)) { /* in display */

        cv = curvis; /* set current buffer status */
        if (!icurbnd(sc)) cv = 0; /* not in bounds, force off */
        if (cv != curon) { /* not already at the desired state */

            if (cv) {

                trm_curon();
                curon = TRUE;

            } else {

                trm_curoff();
                curon = FALSE;

            }

        }

    }

}

/*******************************************************************************

Position cursor

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that. We consider the current position and
visible/invisible status, and try to output only the minimum terminal controls
to bring the old state of the display to the same state as the new display.

*******************************************************************************/

static void setcur(scnptr sc)

{

    if (indisp(sc)) { /* in display */

        /* check cursor in bounds */
        if (icurbnd(sc)) {

            /* set cursor position */
            if ((ncurx != curx || ncury != cury) && curval) {

                /* Cursor position and actual don't match. Try some optimized
                   cursor positions to reduce bandwidth. Note we don't count on
                   real terminal behavior at the borders. */
                if (ncurx == 1 && ncury == 1) trm_home();
                else if (ncurx == curx && ncury == cury-1) trm_up();
                else if (ncurx == curx && ncury == cury+1) trm_down();
                else if (ncurx == curx-1 && ncury == cury) trm_left();
                else if (ncurx == curx+1 && ncury == cury) trm_right();
                else if (ncurx == 1 && ncury == cury) putchr('\r');
                else trm_cursor(ncurx, ncury);
                curx = ncurx;
                cury = ncury;
                curval = 1;

            } else {

                /* don't count on physical cursor location, just reset */
                trm_cursor(ncurx, ncury);
                curx = ncurx;
                cury = ncury;
                curval = 1;

            }

        }
        cursts(sc); /* set new cursor status */

    }

}

/** ****************************************************************************

Restore screen

Updates all the buffer and screen parameters to the terminal. We specifically
write each location. A clear would be faster, but would flash.

*******************************************************************************/

static void restore(scnptr sc)

{

    /** screen indexes */         int xi, yi;
#ifdef NATIVE24
    /** color saves */            int fs, bs;
#else
    /** color saves */            ami_color fs, bs;
#endif
    /** attribute saves */        scnatt as;
    /** screen element pointer */ scnrec *p;
    /** clipped buffer sizes */   int cbufx, cbufy;

    trm_curoff(); /* turn cursor off for display */
    curon = FALSE;
    trm_home(); /* restore cursor to upper left to start */
    /* set colors and attributes */
#ifdef NATIVE24
    trm_fcolorrgb(forergb); /* restore colors */
    trm_bcolorrgb(backrgb);
#else
    trm_fcolor(forec); /* restore colors */
    trm_bcolor(backc);
#endif
    setattr(sc, attr); /* restore attributes */
#ifdef NATIVE24
    fs = forergb; /* save current colors and attributes */
    bs = backrgb;
#else
    fs = forec; /* save current colors and attributes */
    bs = backc;
#endif
    as = attr;
    /* find buffer sizes clipped by onscreen image */
    cbufx = bufx;
    cbufy = bufy;
    if (cbufx > dimx) cbufx = dimx;
    if (cbufy > dimy) cbufy = dimy;
    /* copy buffer to screen */
    for (yi = 1; yi <= cbufy; yi++) { /* lines */

        for (xi = 1; xi <= cbufx; xi++) { /* characters */

            /* for each new character, we compare the attributes and colors
               with what is set. if a new color or attribute is called for,
               we set that, and update the saves. this technique cuts down on
               the amount of output characters */
            p = &SCNBUF(sc, xi, yi); /* index this screen element */
#ifdef NATIVE24
            if (p->forergb != fs) { /* new foreground color */
#else
            if (p->forec != fs) { /* new foreground color */
#endif

#ifdef NATIVE24
                trm_fcolorrgb(p->forergb); /* set the new color */
                fs = p->forergb; /* set save */
#else
                trm_fcolor(p->forec); /* set the new color */
                fs = p->forec; /* set save */
#endif

            };
#ifdef NATIVE24
            if (p->backrgb != bs) { /* new foreground color */
#else
            if (p->backc != bs) { /* new foreground color */
#endif

#ifdef NATIVE24
                trm_bcolorrgb(p->backrgb); /* set the new color */
                bs = p->backrgb; /* set save */
#else
                trm_bcolor(p->backc); /* set the new color */
                bs = p->backc; /* set save */
#endif

            };
            if (p->attr != as) { /* new attribute */

                setattr(sc, p->attr); /* set the new attribute */
                as = p->attr; /* set save */

            };
#ifdef ALLOWUTF8
            putnstr(p->ch, 4); /* now output the actual character */
#else
            putchr(p->ch); /* now output the actual character */
#endif

        };
        if (yi < cbufy)
            /* output next line sequence on all lines but the last. this is
               because the last one would cause us to scroll */
            putstrc("\r\n");

    };
    /* color backgrounds outside of buffer */
    if (dimx > bufx) {

        /* space to right */
#ifdef NATIVE24
        trm_bcolorrgb(backrgb); /* set background color */
#else
        trm_bcolor(backc); /* set background color */
#endif
        for (yi = 1; yi <= bufy; yi++) {

            trm_cursor(bufx+1, yi); /* locate to line start */
            for (xi = bufx+1; xi <= dimx; xi++) putchr(' '); /* fill */

        }

    }
    if (dimy > bufy) {

        /* space to bottom, we color right bottom here because it is easier */
#ifdef NATIVE24
        trm_bcolorrgb(backrgb); /* set background color */
#else
        trm_bcolor(backc); /* set background color */
#endif
        for (yi = bufy+1; yi <= dimy; yi++) {

            trm_cursor(1, yi); /* locate to line start */
            for (xi = 1; xi <= dimx; xi++) putchr(' '); /* fill */

        }

    }
    /* restore cursor position */
    trm_cursor(ncurx, ncury);
    curx = ncurx; /* set physical cursor */
    cury = ncury;
    curval = 1; /* set it is valid */
#ifdef NATIVE24
    trm_fcolorrgb(forergb); /* restore colors */
    trm_bcolorrgb(backrgb);
#else
    trm_fcolor(forec); /* restore colors */
    trm_bcolor(backc);
#endif
    setattr(sc, attr); /* restore attributes */
    setcur(sc); /* set cursor status */

}

/** *****************************************************************************

Get keyboard code control match or other event

Performs a successive match to keyboard input. A keyboard character is read,
and matched against the keyboard equivalence table. If we find a match, we keep
reading in characters until we get a single unambiguous matching entry.

If the match never results in a full match, the buffered characters are simply
discarded, and matching goes on with the next input character. Such "stillborn"
matches are either the result of ill considered input key equivalences, or of
a user typing in keys manually that happen to evaluate to special keys.

ievent() is called within the input spooler task only.

*******************************************************************************/

/* get and process a joystick event */
static void joyevt(ami_evtrec* er, joyptr jp)

{

#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X or BSD */
    struct js_event ev;

    read(jp->fid, &ev, sizeof(ev)); /* get next joystick event */
    if (!(ev.type & JS_EVENT_INIT)) {

        if (ev.type & JS_EVENT_BUTTON) {

            /* we use Linux button numbering, because, what the heck */
            if (ev.value) { /* assert */

                er->etype = ami_etjoyba; /* set assert */
                er->ajoyn = jp->no; /* set joystick 1 */
                er->ajoybn = ev.number+1; /* set button number */
                enquepaevt(er); /* send to queue */

            } else { /* deassert */

                er->etype = ami_etjoybd; /* set assert */
                er->djoyn = jp->no; /* set joystick 1 */
                er->djoybn = ev.number+1; /* set button number */
                enquepaevt(er); /* send to queue */

            }

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
                er->mjoyn = jp->no; /* set joystick number */
                er->joypx = jp->ax; /* place joystick axies */
                er->joypy = jp->ay;
                er->joypz = jp->az;
                er->joyp4 = jp->a4;
                er->joyp5 = jp->a5;
                er->joyp6 = jp->a6;
                enquepaevt(er); /* send to queue */

            }

        }

    }
#endif

}

/* Type specific strncmp to make clang happy */
int strncmpus(const unsigned char* cs, const char* ct, size_t n)

{

    /* skip to end of either, or first non-equal character */
    while (*cs && *ct && (*cs == *ct) && n) cs++, ct++, n--;

    if (!n || (!*cs && !*ct)) return (0); /* end of strings, return equal */
    if (!*cs) return (-1); /* end of 1st, return less than */
    if (!*ct) return (1); /* end of 2nd, return greater than */
    if (*cs < *ct) return -1; /* return less than status */
    else if (*cs > *ct) return 1; /* return greater than status */
    return (0); /* return match status */

}

static void ievent(void)

{

    int       pmatch;  /* partial match found */
    ami_evtcod i;       /* index for events */
    int       rv;      /* return value */
    int       evtfnd;  /* found an event */
    int       ti;      /* index for timers */
    int       ji;      /* index for joysticks */
    enum { mnone, mbutton, mx, my } mousts; /* mouse state variable */
    int       dimxs;   /* save for screen size */
    int       dimys;
    sysevt    sev;     /* system event */
    joyptr    jp;
    int       bn;      /* mouse button number */
    int       ba;      /* mouse button assert */
    ami_evtrec er;      /* event record */

    mousts = mnone; /* set no mouse event being processed */
    do { /* match input events */

        evtfnd = 0; /* set no event found */
        system_event_getsevt(&sev); /* get the next system event */
        /* check the read file has signaled */
        if (sev.typ == se_inp && sev.lse == inpsev) {

            /* keyboard (standard input) */
            keybuf[keylen++] = getchr(); /* get next character to match buffer */
            if (mousts == mnone) { /* do table matching */

                pmatch = 0; /* set no partial matches */
                for (i = ami_etchar; i <= ami_etterm+MAXFKEY && !evtfnd; i++)
                    if (!strncmpus(keybuf, keytab[i], keylen)) {

                    pmatch = 1; /* set partial match */
                    /* set if the match is whole key */
                    if (strlen(keytab[i]) == keylen) {

                        if (i == ami_etmoumov)
                            /* mouse move leader, start state machine */
                            mousts = mbutton; /* set next is button state */
                        else {

                            /* complete match found, set as event */
                            if (i > ami_etterm) { /* it's a function key */

                                er.etype = ami_etfun;
                                /* compensate for F12 subsitution */
                                if (i == ami_etterm+MAXFKEY) er.fkey = 10;
                                else er.fkey = i-ami_etterm;

                            } else er.etype = i; /* set event */
                            evtfnd = 1; /* set event found */
                            enquepaevt(&er); /* send to queue */
                            keylen = 0; /* clear buffer */
                            pmatch = 0; /* clear partial match */

                            /* if its an unresponsive program timeout, we can
                               handle the termination right here */
                            if (unresponsekill && respto && 
                                er.etype == ami_etterm) exit(1);

                        }

                    }

                }
                if (!pmatch) {

                    /* if no partial match and there are characters in buffer, something
                       went wrong, or there never was at match at all. For such
                       "stillborn" matches we start over */
                    if (keylen > 1) keylen = 0;
                    else if (keylen == 1) { /* have valid character */

                        er.etype = ami_etchar; /* set event */
                        er.echar = keybuf[0]; /* place character */
                        evtfnd = 1; /* set event found */
                        enquepaevt(&er); /* send to queue */
                        keylen = 0; /* clear buffer */

                    }

                }

            } else { /* parse mouse components */

#ifdef MOUSESGR
                /* SGR is variable length */
                if (keybuf[keylen-1] == 'm' || keybuf[keylen-1] == 'M') {

                    if (mouseenb) { /* mouse is enabled */

                        /* mouse message is complete, parse */
                        ba = keybuf[keylen-1] == 'm'; /* set assert */
                        keylen = 3; /* set start of sequence in buffer */
                        bn = 0; /* clear button number */
                        while (keybuf[keylen] >= '0' && keybuf[keylen] <= '9')
                            bn = bn*10+keybuf[keylen++]-'0';
                        if (keybuf[keylen] == ';') keylen++;
                        nmpx = 0; /* clear x */
                        while (keybuf[keylen] >= '0' && keybuf[keylen] <= '9')
                            nmpx = nmpx*10+keybuf[keylen++]-'0';
                        if (keybuf[keylen] == ';') keylen++;
                        nmpy = 0; /* clear y */
                        while (keybuf[keylen] >= '0' && keybuf[keylen] <= '9')
                            nmpy = nmpy*10+keybuf[keylen++]-'0';
                        if (keybuf[keylen] == 'm' || keybuf[keylen] == 'M') {

                            /* mouse sequence is correct, process */
                            switch (bn) { /* decode button */

                                case 0: nbutton1 = ba; break; /* assert button 1 */
                                case 1: nbutton2 = ba; break; /* assert button 2 */
                                case 2: nbutton3 = ba; break; /* assert button 3 */
                                default: break; /* deassert all, do nothing */

                            }

                        }

                    }
                    keylen = 0; /* clear key buffer */
                    mousts = mnone; /* reset mouse aquire */

                }
#else
                /* standard mouse encode */
                if (mousts < my) mousts++;
                else { /* mouse has matured */

                    if (mouseenb) { /* mouse is enabled */

                        /* the mouse event state is laid out in the buffer, we will
                           decompose it into a new mouse status */
                        nbutton1 = 1;
                        nbutton2 = 1;
                        nbutton3 = 1;
                        switch (keybuf[3] & 0x3) {

                            case 0: nbutton1 = 0; break; /* assert button 1 */
                            case 1: nbutton2 = 0; break; /* assert button 2 */
                            case 2: nbutton3 = 0; break; /* assert button 3 */
                            case 3: break; /* deassert all, do nothing */

                        }
                        /* set new mouse position */
                        nmpx = keybuf[4]-33+1;
                        nmpy = keybuf[5]-33+1;

                    }
                    keylen = 0; /* clear key buffer */
                    mousts = mnone; /* reset mouse aquire */

                }
#endif

            }

        } else if (sev.typ == se_tim) {

            /* look in timer set */
            pthread_mutex_lock(&timlock); /* take the timer lock */
            for (ti = 0; ti < AMI_MAXTIM && !evtfnd; ti++) {

                if (timtbl[ti] == sev.lse) {

                    /* timer found, set as event */
                    er.etype = ami_ettim; /* set timer type */
                    er.timnum = ti+1; /* set timer number */
                    evtfnd = 1; /* set event found */
                    enquepaevt(&er); /* send to queue */

                }

            }

            /* check the frame timer */
            if (!evtfnd && sev.lse == frmsev) {

                er.etype = ami_etframe; /* set frame event occurred */
                evtfnd = TRUE; /* set event found */
                enquepaevt(&er); /* send to queue */

            }

            /* check the hover timer */
            if (!evtfnd && sev.lse == hovsev && hover) {

                er.etype = ami_etnohover; /* set no hover event occurred */
                evtfnd = TRUE; /* set event found */
                enquepaevt(&er); /* send to queue */
                hover = FALSE; /* remove hover status */

            }

            /* check the finish blink timer */
            if (!evtfnd && sev.lse == blksev) {

                er.etype = ami_etsys; /* set system timer event occurred */
                evtfnd = TRUE; /* set event found */
                enquepaevt(&er); /* send to queue */

            }

            /* check the response timer */
            if (!evtfnd && sev.lse == respsev && unresponse) {

                /* present unresponsive message and flag state */
                trm_title("Program unresponsive");
                respto = TRUE;

            }

            pthread_mutex_unlock(&timlock); /* release the timer lock */

        } else if (sev.typ == se_inp && !evtfnd && joyenb) {

            /* look in joystick set */
            for (ji = 0; ji < numjoy && !evtfnd; ji++) {

                if (joytab[ji] && joytab[ji]->sid == sev.lse) {

                    joyevt(&er, joytab[ji]); /* process joystick */

                }

            }

        } else if (sev.typ == se_sig && !evtfnd && sev.lse == winchsev) {

            findsize(&dimxs, &dimys); /* get new size */
            er.etype = ami_etresize;
            er.rszx = dimxs; /* send new size in message */
            er.rszy = dimys;
            evtfnd = 1;
            enquepaevt(&er); /* send to queue */

        }

        if (!evtfnd) {

            /* check any mouse states have changed, flag and remove */
            if (nbutton1 < button1) {

                er.etype = ami_etmouba;
                er.amoun = 1;
                er.amoubn = 1;
                evtfnd = 1;
                enquepaevt(&er); /* send to queue */
                button1 = nbutton1;

            } else if (nbutton1 > button1) {

                er.etype = ami_etmoubd;
                er.dmoun = 1;
                er.dmoubn = 1;
                evtfnd = 1;
                enquepaevt(&er); /* send to queue */
                button1 = nbutton1;

            } else if (nbutton2 < button2) {

                er.etype = ami_etmouba;
                er.amoun = 1;
                er.amoubn = 2;
                evtfnd = 1;
                enquepaevt(&er); /* send to queue */
                button2 = nbutton2;

            } else if (nbutton2 > button2) {

                er.etype = ami_etmoubd;
                er.dmoun = 1;
                er.dmoubn = 2;
                evtfnd = 1;
                enquepaevt(&er); /* send to queue */
                button2 = nbutton2;

            } else if (nbutton3 < button3) {

                er.etype = ami_etmouba;
                er.amoun = 1;
                er.amoubn = 3;
                evtfnd = 1;
                enquepaevt(&er); /* send to queue */
                button3 = nbutton3;

            } else if (nbutton3 > button3) {

                er.etype = ami_etmoubd;
                er.dmoun = 1;
                er.dmoubn = 3;
                evtfnd = 1;
                enquepaevt(&er); /* send to queue */
                button3 = nbutton3;

            } if (nmpx != mpx || nmpy != mpy) {

                er.etype = ami_etmoumov;
                er.mmoun = 1;
                er.moupx = nmpx;
                er.moupy = nmpy;
                evtfnd = 1;
                enquepaevt(&er); /* send to queue */
                mpx = nmpx;
                mpy = nmpy;
                /* mouse moved, that means we are within the window. Check if
                   hover is activated */
                if (!hover) {

                    er.etype = ami_ethover;
                    evtfnd = 1;
                    enquepaevt(&er); /* send to queue */
                    hover = TRUE; /* activate hover */

                }
                /* set the hover timer, one shot, 5 seconds */
                hovsev = system_event_addsetim(hovsev, HOVERTIME, FALSE);

            }

        }

    } while (1); /* forever */

}

/** ****************************************************************************

Event input thread

This thread runs continuously and gets events from the lower level, then spools
them into the input queue. This allows the input queue to run ahead of the
client program.

*******************************************************************************/

static void* eventtask(void* param)

{

    ami_evtrec er; /* event record */

    ievent(); /* process events (forever) */

    return (NULL);

}

/** ****************************************************************************

Place character in screen buffer

Places the next character or extension in the given screen buffer location.
Handles either ISO 8859 characters or UTF-8 characters.

For UTF-8, there are a few errors possible. Here is how they are handled:

1. Too many extension (10xxxxxx) characters. Overflowing 4 places will cause
the sequence to be reset to 0 and thus cleared.

2. Too many extension (10xxxxxx) characters for format. This happens if the
first or count character indicates fewer than the number of extension characters
received. The sequence is cleared and reset.

3. An extension (10xxxxxx) character received as the first character. The
sequence is cleared.

*******************************************************************************/

static void plcchrext(scnrec* p, unsigned char c)

{

#ifdef ALLOWUTF8
    int ci;

    if (c < 0x80 || c >= 0xc0) { /* normal ASCII or start UTF-8 character */

        /* start of character sequence, clear whole sequence */
        for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;
        p->ch[0] = c; /* place start character */

    } else if ( (c & 0xc0) == 0x80) { /* extension character */

        if (p->ch[0] == 0) { /* extension received as first character */

            for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;

        } else {

            /* follow on character */
            ci = 0;
            while (ci < 4 && p->ch[ci]) ci++;
            /* overflow, clear out */
            if (ci >= 4) {

                for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;

            } else {

                /* more extension characters than count char */
                if (ci > utf8bits[p->ch[0]])
                    for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;
                else /* place next in sequence */
                    p->ch[ci] = c;

            }

        }

    }
#else
    p->ch = c; /* place character */
#endif

}

/** ****************************************************************************

Clear screen buffer

Clears the entire screen buffer to spaces with the current colors and
attributes.

*******************************************************************************/

static void clrbuf(scnptr sc)

{

    /** screen indexes */           int x, y;
    /** pointer to screen record */ scnrec* sp;

    /* clear the screen buffer */
    for (y = 1;  y <= bufy; y++)
        for (x = 1; x <= bufx; x++) {

        /* index screen character location */
        sp = &SCNBUF(sc, x, y);
        plcchrext(sp, ' '); /* clear to spaces */
        /* colors and attributes to the global set */
#ifdef NATIVE24
        sp->forergb = forergb;
        sp->backrgb = backrgb;
#else
        sp->forec = forec;
        sp->backc = backc;
#endif
        sp->attr = attr;

    }

}

/** ****************************************************************************

Initialize screen

Clears all the parameters in the present screen context, and updates the
display to match.

*******************************************************************************/

static void iniscn(scnptr sc)

{

    ncury = 1; /* set cursor at home */
    ncurx = 1;
    /* these attributes and colors are pretty much windows 95 specific. The
       Bizarre setting of "blink" actually allows access to bright white */
    forec = ami_black; /* set colors and attributes */
    forergb = colnumrgbp(ami_black);
    backc = ami_white;
    backrgb = colnumrgbp(ami_white);
    attr = sanone;
    curvis = curon; /* set cursor visible from curent state */
    clrbuf(sc); /* clear screen buffer with that */

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

Scrolls the ANSI terminal screen by deltas in any given direction. For an ANSI
terminal, we special case any scroll that is downward only, without any
movement in x. These are then done by an arbitrary number of line feeds
executed at the bottom of the screen.
For all other scrolls, we do this by completely refreshing the contents of the
screen, including blank lines or columns for the "scrolled in" areas. The
blank areas are all given the current attributes and colors.
The cursor always remains in place for these scrolls, even though the text
is moving under it.

*******************************************************************************/

void prtbuf(scnptr sc)

{

    int x, y;

    fprintf(stderr, "Screen:\n\n");
    for (y = 1; y <= dimy; y++) {

        fprintf(stderr, "%2d\"", y);
#ifdef ALLOWUTF8
        for (x = 1; x <= dimx; x++) fprintf(stderr, "%c", SCNBUF(sc, x, y).ch[0]);
#else
        for (x = 1; x <= dimx; x++) fprintf(stderr, "%c", SCNBUF(sc, x, y).ch);
#endif
        fprintf(stderr, "\"\n");

    }

}

static void iscroll(scnptr sc, int x, int y)

{

    int      xi, yi; /* screen counters */
    ami_color fs, bs; /* color saves */
    scnatt   as;     /* attribute saves */
    scnrec*  scnsav; /* full screen buffer save */
    int      lx;     /* last unmatching character index */
    int      m;      /* match flag */
    scnrec*  sp;     /* pointer to screen record */
    scnrec*  sp2;

    if (y > 0 && x == 0) {

        if (indisp(sc)) { /* in display */

            trm_curoff(); /* turn cursor off for display */
            curon = FALSE;
            /* downward straight scroll, we can do this with native scrolling */
            trm_cursor(1, dimy); /* position to bottom of screen */
            /* use linefeed to scroll. linefeeds work no matter the state of
               wrap, and use whatever the current background color is */
            yi = y;   /* set line count */
            while (yi > 0) {  /* scroll down requested lines */

                putchr('\n');   /* scroll down */
                yi--;   /* count lines */

            }
            /* restore cursor position */
            trm_cursor(ncurx, ncury);
            cursts(sc); /* re-enable cursor */

        }
        /* now, adjust the buffer to be the same */
        for (yi = 1; yi <= bufy-1; yi++) /* move any lines up */
            if (yi+y <= bufy) /* still within buffer */
                /* move lines up */
                    memcpy(&sc[(yi-1)*bufx], &sc[(yi+y-1)*bufx],
                           bufx*sizeof(scnrec));
        for (yi = bufy-y+1; yi <= bufy; yi++) /* clear blank lines at end */
            for (xi = 1; xi <= bufx; xi++) {

            sp = &SCNBUF(sc, xi, yi);
            plcchrext(sp, ' '); /* clear to blanks at colors and attributes */
#ifdef NATIVE24
            sp->forergb = forergb;
            sp->backrgb = backrgb;
#else
            sp->forec = forec;
            sp->backc = backc;
#endif
            sp->attr = attr;

        }

    } else { /* odd direction scroll */

        /* when the scroll is arbitrary, we do it by completely refreshing the
           contents of the screen from the buffer */
        if (x <= -bufx || x >= bufx || y <= -bufy || y >= bufy) {

            /* scroll would result in complete clear, do it */
            trm_clear();   /* scroll would result in complete clear, do it */
            clrbuf(sc);   /* clear the screen buffer */
            /* restore cursor position */
            trm_cursor(ncurx, ncury);

        } else { /* scroll */

            /* true scroll is done in two steps. first, the contents of the buffer
               are adjusted to read as after the scroll. then, the contents of the
               buffer are output to the terminal. before the buffer is changed,
               we perform a full save of it, which then represents the "current"
               state of the real terminal. then, the new buffer contents are
               compared to that while being output. this saves work when most of
               the screen is spaces anyways */

            /* save the entire buffer */
            scnsav = malloc(sizeof(scnrec)*bufy*bufx);
            memcpy(scnsav, sc, sizeof(scnrec)*bufy*bufx);
            if (y > 0) {  /* move text up */

                for (yi = 1; yi < bufy; yi++) /* move any lines up */
                    if (yi + y <= bufy) /* still within buffer */
                        /* move lines up */
                        memcpy(&sc[(yi-1)*bufx], &sc[(yi+y-1)*bufx],
                           bufx*sizeof(scnrec));
                for (yi = bufy-y+1; yi <= bufy; yi++)
                    /* clear blank lines at end */
                    for (xi = 1; xi <= bufx; xi++) {

                    sp = &SCNBUF(sc, xi, yi);
                    /* clear to blanks at colors and attributes */
                    plcchrext(sp, ' ');
#ifdef NATIVE24
                    sp->forergb = forergb;
                    sp->backrgb = backrgb;
#else
                    sp->forec = forec;
                    sp->backc = backc;
#endif
                    sp->attr = attr;

                }

            } else if (y < 0) { /* move text down */

                for (yi = bufy; yi >= 2; yi--)   /* move any lines up */
                    if (yi + y >= 1) /* still within buffer */
                        /* move lines up */
                        memcpy(&sc[(yi-1)*bufx], &sc[(yi+y-1)*bufx],
                           bufx*sizeof(scnrec));
                for (yi = 1; yi <= abs(y); yi++) /* clear blank lines at start */
                    for (xi = 1; xi <= bufx; xi++) {

                    sp = &SCNBUF(sc, xi, yi);
                    /* clear to blanks at colors and attributes */
                    plcchrext(sp, ' ');
#ifdef NATIVE24
                    sp->forergb = forergb;
                    sp->backrgb = backrgb;
#else
                    sp->forec = forec;
                    sp->backc = backc;
#endif
                    sp->attr = attr;

                }

            }
            if (x > 0) { /* move text left */
                for (yi = 1; yi <= bufy; yi++) { /* move text left */

                    for (xi = 1; xi <= bufx-1; xi++) /* move left */
                        if (xi+x <= bufx) /* still within buffer */
                            /* move characters left */
                            memcpy(&SCNBUF(sc, xi, yi), &SCNBUF(sc, xi+x, yi),
                               sizeof(scnrec));
                    /* clear blank spaces at right */
                    for (xi = bufx-x+1; xi <= bufx; xi++) {

                        sp = &SCNBUF(sc, xi, yi);
                        /* clear to blanks at colors and attributes */
                        plcchrext(sp, ' ');
#ifdef NATIVE24
                        sp->forergb = forergb;
                        sp->backrgb = backrgb;
#else
                        sp->forec = forec;
                        sp->backc = backc;
#endif
                        sp->attr = attr;

                    }

                }

            } else if (x < 0) { /* move text right */

                for (yi = 1; yi <= bufy; yi++) { /* move text right */

                    for (xi = bufx; xi >= 2; xi--) /* move right */
                        if (xi+x >= 1) /* still within buffer */
                            /* move characters left */
                            memcpy(&SCNBUF(sc, xi, yi), &SCNBUF(sc, xi+x, yi),
                               sizeof(scnrec));
                    /* clear blank spaces at left */
                    for (xi = 1; xi <= abs(x); xi++) {

                        sp = &SCNBUF(sc, xi, yi);
                        /* clear to blanks at colors and attributes */
                        plcchrext(sp, ' ');
#ifdef NATIVE24
                        sp->forergb = forergb;
                        sp->backrgb = backrgb;
#else
                        sp->forec = forec;
                        sp->backc = backc;
#endif
                        sp->attr = attr;

                    }

                }

            }
            if (indisp(sc)) restore(sc); /* in display, restore to screen */

        }

    }

}

/** ****************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void iclear(scnptr sc)

{

    clrbuf(sc); /* clear the screen buffer */
    ncury = 1; /* set cursor at home */
    ncurx = 1;
    if (indisp(sc)) { /* in display */

        trm_clear(); /* erase screen */
        curx = 1; /* set actual cursor location */
        cury = 1;
        curval = 1;
        setcur(sc);

    }


}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(scnptr sc, int x, int y)

{

    ncury = y; /* set new position */
    ncurx = x;
    setcur(sc);

}

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

static void iup(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncury > 1) /* not at top of screen */
            ncury = ncury-1; /* update position */
        else if (scroll) /* scroll enabled */
            iscroll(sc, 0, -1); /* at top already, scroll up */
        else /* wrap cursor around to screen bottom */
            ncury = bufy; /* set new position */

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncury > -INT_MAX) ncury--;
    setcur(sc);

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

static void idown(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncury < bufy) /* not at bottom of screen */
            ncury = ncury+1; /* update position */
        else if (scroll) /* wrap enabled */
            iscroll(sc, 0, +1); /* already at bottom, scroll down */
        else /* wrap cursor around to screen top */
            ncury = 1; /* set new position */

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncury < INT_MAX) ncury++;
    setcur(sc);

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left.

*******************************************************************************/

static void ileft(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncurx > 1)  /* not at extreme left */
            ncurx = ncurx-1; /* update position */
        else { /* wrap cursor motion */

            iup(sc); /* move cursor up one line */
            ncurx = bufx; /* set cursor to extreme right */

        }

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncurx > -INT_MAX) ncurx--;
    setcur(sc);

}

/** ****************************************************************************

Move cursor right internal

Moves the cursor one character right.

*******************************************************************************/

static void iright(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncurx < bufx) /* not at extreme right */
            ncurx = ncurx+1; /* update position */
        else { /* wrap cursor motion */

            idown(sc); /* move cursor up one line */
            ncurx = 1; /* set cursor to extreme left */

        }

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncurx < INT_MAX) ncurx++;
    setcur(sc);

}

/** ****************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attribute.

We handle some elementary control codes here, like newline, backspace and form
feed. However, the idea is not to provide a parallel set of screen controls.
That's what the API is for.

*******************************************************************************/

static void plcchr(scnptr sc, unsigned char c)

{

    scnrec* p;
    int i;

    /* handle special character cases first */
    if (c == '\r') /* carriage return, position to extreme left */
        icursor(sc, 1, ncury);
    else if (c == '\n') {

        /* line end */
        idown(sc); /* line feed, move down */
        /* position to extreme left */
        icursor(sc, 1, ncury);

    } else if (c == '\b') ileft(sc); /* back space, move left */
    else if (c == '\f') iclear(sc); /* clear screen */
    else if (c == '\t') {

        /* find next tab position */
        i = ncurx+1; /* find current x +1 */
        while (i < dimx && !tabs[i-1]) i++;
        if (tabs[i-1]) /* we found a tab */
           while (ncurx < i) iright(sc);

    } else if (c >= ' ' && c != 0x7f) {

#ifdef ALLOWUTF8
        /* if UTF-8 leader, set character count */
        if (c >= 0xc0) utf8cnt = utf8bits[c >> 4];
        if (utf8cnt) utf8cnt--; /* count off characters */
#endif
        /* normal character case, not control character */
        if (ncurx >= 1 && ncurx <= bufx &&
            ncury >= 1 && ncury <= bufy) {

            /* within the buffer space, otherwise just dump */
            p = &SCNBUF(sc, ncurx, ncury);
            plcchrext(p, c); /* place character in buffer */
#ifdef NATIVE24
            p->forergb = forergb; /* place colors */
            p->backrgb = backrgb;
#else
            p->forec = forec; /* place colors */
            p->backc = backc;
#endif
            p->attr = attr; /* place attribute */

        }
        /* cursor in bounds, in display, and not mid-UTF-8 */
        if (icurbnd(sc) && indisp(sc)) {

            /* This handling is from iright. We do this here because
               placement implicitly moves the cursor */
            if (ncurx >= 1 && ncurx <= bufx &&
                ncury >= 1 && ncury <= bufy)
                putchr(c); /* output character to terminal */
#ifdef ALLOWUTF8
            if (!utf8cnt)
#endif
            { /* not working on partial character */

                /* at right side, don't count on the screen wrap action */
                if (curx == dimx) curval = 0;
                else curx++; /* update physical cursor */
                if (scroll) { /* autowrap is on */

                    if (ncurx < bufx) /* not at extreme right */
                        ncurx = ncurx+1; /* update position */
                    else { /* wrap cursor motion */

                        idown(sc); /* move cursor down one line */
                        ncurx = 1; /* set cursor to extreme left */

                    }

                } else {/* autowrap is off */

                    /* prevent overflow, but otherwise its unlimited */
                    if (ncurx < INT_MAX) ncurx++;
                    /* don't count on physical cursor behavior if scrolling is
                       off and we are at extreme right */
                    curval = 0;

                }
                setcur(sc); /* update physical cursor */

            }

        } else iright(sc); /* move right */

    }

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into the
input line buffer.

*******************************************************************************/

/* threadable version of strlen */

static size_t strlent(volatile const char* s)

{

    int sl;

    sl = 0;
    while (*s) { s++; sl++; }

    return (sl);

}

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
    xoff = ncurx; /* save starting line offset */
    do { /* get line characters */

        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        ami_event(stdin, &er); /* get next event */
        pthread_mutex_lock(&termlock); /* lock terminal broadlock */
        switch (er.etype) { /* event */

            case ami_etterm: exit(1); /* halt program */
            case ami_etenter: /* line terminate */
                while (inpbuf[inpptr]) inpptr++; /* advance to end */
                inpbuf[inpptr] = '\n'; /* return newline */
                /* terminate the line for debug prints */
                inpbuf[inpptr+1] = 0;
                plcchr(sc, '\r'); /* output newline sequence */
                plcchr(sc, '\n');
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
                        while (inpbuf[i]) plcchr(sc, inpbuf[i++]);
                        /* back up */
                        i = inpptr;
                        while (inpbuf[i++]) plcchr(sc, '\b');
                        /* forward and next char */
                        plcchr(sc, inpbuf[inpptr]);
                        inpptr++;

                    } else { /* overwrite */

                        /* if end, move end marker */
                        if (!inpbuf[inpptr]) inpbuf[inpptr+1] = 0;
                        inpbuf[inpptr] = er.echar; /* place new character */
                        /* forward and next char */
                        plcchr(sc, inpbuf[inpptr]);
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
                    plcchr(sc, '\b'); /* move cursor back */
                    /* repaint line */
                    i = inpptr;
                    while (inpbuf[i]) plcchr(sc, inpbuf[i++]);
                    plcchr(sc, ' '); /* blank last */
                    /* back up */
                    plcchr(sc, '\b');
                    i = inpptr;
                    while (inpbuf[i++]) plcchr(sc, '\b');

                }
                break;
            case ami_etdelcf: /* delete character forward */
                if (inpbuf[inpptr]) { /* not at extreme right */

                    /* move characters down */
                    i = inpptr;
                    while (inpbuf[i]) { inpbuf[i] = inpbuf[i+1]; i++; }
                    /* repaint right */
                    i = inpptr;
                    while (inpbuf[i]) plcchr(sc, inpbuf[i++]);
                    plcchr(sc, ' '); /* blank last */
                    /* back up */
                    plcchr(sc, '\b');
                    i = inpptr;
                    while (inpbuf[i++]) plcchr(sc, '\b');

                }
                break;
            case ami_etright: /* right character */
                /* not at extreme right, go right */
                if (inpbuf[inpptr]) {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++; /* advance input */

                }
                break;

            case ami_etleft: /* left character */
                /* not at extreme left, go left */
                if (inpptr > 0) {

                    plcchr(sc, '\b');
                    inpptr--; /* back up pointer */

                }
                break;

            case ami_etmoumov: /* mouse moved */
                /* we can track this internally */
                break;

            case ami_etmouba: /* mouse click */
                if (er.amoubn == 1) {

                    l = strlent(inpbuf);
                    if (ncury == nmpy && xoff <= nmpx && xoff+l >= nmpx) {

                        /* mouse position is within buffer space, set
                           position */
                        icursor(sc, nmpx, ncury);
                        inpptr = nmpx-xoff;

                    }

                }
                break;

            case ami_ethomel: /* beginning of line */
                /* back up to start of line */
                while (inpptr) {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                break;

            case ami_etendl: /* end of line */
                /* go to end of line */
                while (inpbuf[inpptr]) {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++;

                }
                break;

            case ami_etinsertt: /* toggle insert mode */
                ins = !ins; /* toggle insert mode */
                break;

            case ami_etdell: /* delete whole line */
                /* back up to start of line */
                while (inpptr) {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                /* erase line on screen */
                while (inpbuf[inpptr]) {

                    plcchr(sc, ' ');
                    inpptr++;

                }
                /* back up again */
                while (inpptr) {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                inpbuf[inpptr] = 0; /* clear line */
                break;

            case ami_etleftw: /* left word */
                /* back over any spaces */
                while (inpptr && inpbuf[inpptr-1] == ' ') {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                /* now back over any non-space */
                while (inpptr && inpbuf[inpptr-1] != ' ') {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                break;

            case ami_etrightw: /* right word */
                /* advance over any non-space */
                while (inpbuf[inpptr] && inpbuf[inpptr] != ' ') {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++;

                }
                /* advance over any spaces */
                while (inpbuf[inpptr] && inpbuf[inpptr] == ' ') {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++;

                }
                break;

            default: ;

        }

    } while (er.etype != ami_etenter); /* until line terminate */
    inpptr = 0; /* set 1st position on active line */

}

/*******************************************************************************

Present finish message

Presents a finish message to a bar at top of screen. The message is flashed so
that the underlying screen content is seen. Exits on termination.

*******************************************************************************/

static void finish(char* title)

{

    int       bobble; /* bobble display bit */
    scnptr    sc;     /* screen buffer */
    scnrec*   p;      /* screen element pointer */
    int       ml;     /* message length */
    ami_evtrec er;     /* event record */
    int       xi;
    int       i;
    int       xs;

    bobble = FALSE; /* start bobble bit */
    trm_curoff(); /* turn cursor off for display */
    curon = FALSE;
    ml = strlen(title); /* set size of title */
    /* set the blink timer, repeating, 1 second */
    blksev = system_event_addsetim(blksev, 1*10000, TRUE);
    sc = screens[curdsp-1]; /* index display screen */
    /* wait for a formal end */
    while (!fend) {

        if (bobble) { /* clear top line by redrawing it */

            trm_home(); /* restore cursor to upper left to start */
            for (xi = 1; xi <= bufx; xi++) {

                p = &SCNBUF(sc, xi, 1); /* index this screen element */
#ifdef NATIVE24
                trm_fcolorrgb(p->forergb); /* set the new color */
                trm_bcolorrgb(p->backrgb); /* set the new color */
#else
                trm_fcolor(p->forec); /* set the new color */
                trm_bcolor(p->backc); /* set the new color */
#endif
                setattr(sc, p->attr); /* set the new attribute */
#ifdef ALLOWUTF8
                putnstr(p->ch, 4); /* now output the actual character */
#else
                putchr(p->ch); /* now output the actual character */
#endif

            }
            /* color leftover line after buffer */
            trm_bcolor(backc); /* set background color */
            setattr(sc, sanone); /* set the new attribute */
            for (xi = bufx+1; xi <= dimx; xi++) {

                p = &SCNBUF(sc, xi, 1); /* index this screen element */
                putchr(' '); /* blank out */

            }                

        } else {

            /* blank out */
            trm_home(); /* restore cursor to upper left to start */
            setattr(sc, sanone); /* set no attribute */
            trm_bcolor(ami_black); /* set background color */
            trm_fcolor(ami_black); /* set foreground color */
            for (xi = 1; xi <= dimx; xi++) putchr(' '); /* blank out */
            /* draw the "finished" message */
            trm_home(); /* restore cursor to upper left to start */
            trm_bcolor(ami_black); /* set background color */
            trm_fcolor(ami_white); /* set foreground color */
            i = 0; /* set string start */
            xs = dimx/2-ml/2; /* set centered line start */
            if (xs < 1) xs = 1; /* if string too long, clip right */
            trm_cursor(xs, 1); /* move cursor to start */
            for (xi = xs; xi <= dimx && i < ml; xi++) {

                p = &SCNBUF(sc, xi, 1); /* index this screen element */
                putchr(title[i++]); /* place message character */

            }

        }
        ami_event(stdin, &er);
        /* if the blink timer fires, flip the display */
        if (er.etype == ami_etsys) bobble = !bobble;
        /* let the enter exit via enter */
        if (er.etype == ami_etenter) fend = TRUE;

    }

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

            pthread_mutex_lock(&termlock); /* lock terminal broadlock */
            /* if there is no line in the input buffer, get one */
            if (inpptr == -1) readline();
            *p = inpbuf[inpptr]; /* get and place next character */
            if (inpptr < MAXLIN) inpptr++; /* next */
            /* if we have just read the last of that line, then flag buffer
               empty */
            if (*p == '\n') inpptr = -1;
            p++; /* next character */
            cnt--; /* count characters */
            pthread_mutex_unlock(&termlock); /* release terminal broadlock */

        }
        rc = count; /* set return same as count */

    } else rc = (*ofpread)(fd, buff, count);

    return rc;

}

/*******************************************************************************

Write

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    ssize_t rc; /* return code */
    unsigned char *p = (unsigned char *)buff;
    size_t cnt = count;

    if (fd == OUTFIL) {

        /* send data to terminal */
        while (cnt--) {

            pthread_mutex_lock(&termlock); /* lock terminal broadlock */
            plcchr(screens[curupd-1], *p++);
            pthread_mutex_unlock(&termlock); /* release terminal broadlock */

        }
        rc = count; /* set return same as count */

    } else rc = (*ofpwrite)(fd, buff, count);

    return rc;

}

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
    if (fd == INPFIL || fd == OUTFIL) error(ami_dispefilopr);

    return (*ofplseek)(fd, offset, whence);

}

/*******************************************************************************
*
* External API routines
*
*******************************************************************************/

/** ****************************************************************************

Position cursor

This is the external interface to cursor.

*******************************************************************************/

APIOVER(cursor)
void ami_cursor(FILE* f, int x, int y) { (*cursor_vect)(f, x, y); }
static void cursor_ivf(FILE *f, int x, int y)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    icursor(screens[curupd-1], x, y); /* position cursor */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/*******************************************************************************

Find if cursor is in screen bounds

This is the external interface to curbnd.

*******************************************************************************/

APIOVER(curbnd)
int ami_curbnd(FILE* f) { return ((*curbnd_vect)(f)); }
static int curbnd_ivf(FILE *f)

{

    int fl;

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    fl = icurbnd(screens[curupd-1]);
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

    return (fl);

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

APIOVER(maxx)
int ami_maxx(FILE* f) { return ((*maxx_vect)(f)); }
static int maxx_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    return (bufx); /* set maximum x */

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

APIOVER(maxy)
int ami_maxy(FILE* f) { return ((*maxy_vect)(f)); }
static int maxy_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    return (bufy); /* set maximum y */

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

APIOVER(home)
void ami_home(FILE* f) { (*home_vect)(f); }
static void home_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    ncury = 1; /* set cursor at home */
    ncurx = 1;
    setcur(screens[curupd-1]);
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

APIOVER(del)
void ami_del(FILE* f) { (*del_vect)(f); }
static void del_ivf(FILE* f)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    ileft(screens[curupd-1]); /* back up cursor */
    plcchr(screens[curupd-1], ' '); /* blank out */
    ileft(screens[curupd-1]); /* back up again */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Move cursor up

This is the external interface to up.

*******************************************************************************/

APIOVER(up)
void ami_up(FILE* f) { (*up_vect)(f); }
static void up_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    iup(screens[curupd-1]); /* move up */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}


/** ****************************************************************************

Move cursor down

This is the external interface to down.

*******************************************************************************/

APIOVER(down)
void ami_down(FILE* f) { (*down_vect)(f); }
static void down_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    idown(screens[curupd-1]); /* move cursor down */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Move cursor left

This is the external interface to left.

*******************************************************************************/

APIOVER(left)
void ami_left(FILE* f) { (*left_vect)(f); }
static void left_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    ileft(screens[curupd-1]); /* move cursor left */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Move cursor right

This is the external interface to right.

*******************************************************************************/

APIOVER(right)
void ami_right(FILE* f) { (*right_vect)(f); }
static void right_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    iright(screens[curupd-1]); /* move cursor right */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Turn on/off attribute

Turns on or off a single attribute. The attributes can only be set singly.
If the screen is in display, refreshes the colors. This is because in Windows,
changing attributes causes the colors to change. This should be verified on
xterm.

*******************************************************************************/

static void attronoff(FILE *f, int e, scnatt nattr)

{

    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    setattr(screens[curupd-1], sanone); /* turn off attributes */
    if (e) { /* attribute on */

        attr = nattr; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], attr);
        if (curupd == curdsp) { /* in display */

#ifdef NATIVE24
            trm_fcolorrgb(forergb); /* set current colors */
            trm_bcolorrgb(backrgb);
#else
            trm_fcolor(forec); /* set current colors */
            trm_bcolor(backc);
#endif

        }

    } else { /* turn it off */

        attr = sanone; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], attr);
        if (curupd == curdsp) { /* in display */

#ifdef NATIVE24
            trm_fcolorrgb(forergb); /* set current colors */
            trm_bcolorrgb(backrgb);
#else
            trm_fcolor(forec); /* set current colors */
            trm_bcolor(backc);
#endif

        }

    }
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute. Note that under windows 95 in a shell window,
blink does not mean blink, but instead "bright". We leave this alone because
we are supposed to also work over a com interface.

*******************************************************************************/

APIOVER(blink)
void ami_blink(FILE* f, int e) { (*blink_vect)(f, e); }
static void blink_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    attronoff(f, e, sablink); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute.

*******************************************************************************/

APIOVER(reverse)
void ami_reverse(FILE* f, int e) { (*reverse_vect)(f, e); }
static void reverse_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    attronoff(f, e, sarev); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.

*******************************************************************************/

APIOVER(underline)
void ami_underline(FILE* f, int e) { (*underline_vect)(f, e); }
static void underline_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    attronoff(f, e, saundl); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.

*******************************************************************************/

APIOVER(superscript)
void ami_superscript(FILE* f, int e) { (*superscript_vect)(f, e); }
static void superscript_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    /* no capability */

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.

*******************************************************************************/

APIOVER(subscript)
void ami_subscript(FILE* f, int e) { (*subscript_vect)(f, e); }
static void subscript_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    /* no capability */

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.

*******************************************************************************/

APIOVER(italic)
void ami_italic(FILE* f, int e) { (*italic_vect)(f, e); }
static void italic_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    attronoff(f, e, saital); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.

*******************************************************************************/

APIOVER(bold)
void ami_bold(FILE* f, int e) { (*bold_vect)(f, e); }
static void bold_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    attronoff(f, e, sabold); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.

Not implemented.

*******************************************************************************/

APIOVER(strikeout)
void ami_strikeout(FILE* f, int e) { (*strikeout_vect)(f, e); }
static void strikeout_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    /* no capability */

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

APIOVER(standout)
void ami_standout(FILE* f, int e) { (*standout_vect)(f, e); }
static void standout_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    ami_reverse(f, e); /* implement as reverse */

}

/** ****************************************************************************

Set foreground color

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

APIOVER(fcolor)
void ami_fcolor(FILE* f, ami_color c) { (*fcolor_vect)(f, c); }
static void fcolor_ivf(FILE *f, ami_color c)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (curupd == curdsp) trm_fcolor(c); /* set color */
    forec = c;
    forergb = colnumrgbp(c);
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

APIOVER(bcolor)
void ami_bcolor(FILE* f, ami_color c) { (*bcolor_vect)(f, c); }
static void bcolor_ivf(FILE *f, ami_color c)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (curupd == curdsp) trm_bcolor(c); /* set color */
    backc = c;
    backrgb = colnumrgbp(c);
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Enable/disable automatic scroll

Enables or disables automatic screen scroll. With automatic scroll on, moving
off the screen at the top or bottom will scroll up or down, respectively.

*******************************************************************************/

APIOVER(auto)
void ami_auto(FILE* f, int e) { (*auto_vect)(f, e); }
static void auto_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    scroll = e; /* set line wrap status */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

APIOVER(curvis)
void ami_curvis(FILE* f, int e) { (*curvis_vect)(f, e); }
static void curvis_ivf(FILE *f, int e)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    curvis = !!e; /* set cursor visible status */
    if (e) trm_curon(); else trm_curoff();
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Scroll screen

Process full delta scroll on screen. This is the external interface to this
int.

*******************************************************************************/

APIOVER(scroll)
void ami_scroll(FILE* f, int x, int y) { (*scroll_vect)(f, x, y); }
static void scroll_ivf(FILE *f, int x, int y)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    iscroll(screens[curupd-1], x, y); /* process scroll */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

APIOVER(curx)
int ami_curx(FILE* f) { return ((*curx_vect)(f)); }
static int curx_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    return (ncurx); /* return current location x */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

APIOVER(cury)
int ami_cury(FILE* f) { return ((*cury_vect)(f)); }
static int cury_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    return (ncury); /* return current location y */

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

Note that split update and display screens are not implemented at present.

*******************************************************************************/

APIOVER(select)
void ami_select(FILE* f, int u, int d) { (*select_vect)(f, u, d); }
static void select_ivf(FILE *f, int u, int d)

{

    dbg_printf(dlapi, "API\n");
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(ami_dispeinvscn); /* invalid screen number */
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (curupd != u) { /* update screen changes */

        curupd = u; /* change to new screen */
        if (!screens[curupd-1]) { /* no screen allocated there */

            /* allocate screen array */
            screens[curupd-1] = malloc(sizeof(scnrec)*bufy*bufx);
            iniscn(screens[curupd-1]); /* initalize that */

        }

    }
    if (curdsp != d) { /* display screen changes */

        curdsp = d; /* change to new screen */
        if (screens[curdsp-1]) /* screen allocated there */
            restore(screens[curdsp-1]); /* restore current screen */
        else { /* no current screen, create a new one */

            /* allocate screen array */
            screens[curdsp-1] = malloc(sizeof(scnrec)*bufy*bufx);
            iniscn(screens[curdsp-1]); /* initalize that */
            restore(screens[curdsp-1]); /* place on display */

        }

    }
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Default event handlers

Gives the default handling for event function vectors. The default is just to
return handled is false. Not all function vectors have individual handlers.
They are ganged by parameter signature.

*******************************************************************************/

static int genfnc(void) { return FALSE; }
static int charfnc(char c) { return FALSE; }
static int funfnc(int k) { return FALSE; }
static int moubafnc(int m, int b) { return FALSE; }
static int moubdfnc(int m, int b) { return FALSE; }
static int moumovfnc(int m, int x, int y) { return FALSE; }
static int timfnc(int t) { return FALSE; }
static int joybafnc(int j, int b) { return FALSE; }
static int joybdfnc(int j, int b) { return FALSE; }
static int joymovfnc(int j, int x, int y, int z) { return FALSE; }
static int resizefnc(int rszx, int rszy) { return FALSE; }

/** ****************************************************************************

Initialize event function vectors

Sets the default handlers for each of the event function vectors.

*******************************************************************************/

void inifncvec(void)

{

    evchar_vect    = charfnc;
    evup_vect      = genfnc;
    evdown_vect    = genfnc;
    evleft_vect    = genfnc;
    evright_vect   = genfnc;
    evleftw_vect   = genfnc;
    evrightw_vect  = genfnc;
    evhome_vect    = genfnc;
    evhomes_vect   = genfnc;
    evhomel_vect   = genfnc;
    evend_vect     = genfnc;
    evends_vect    = genfnc;
    evendl_vect    = genfnc;
    evscrl_vect    = genfnc;
    evscrr_vect    = genfnc;
    evscru_vect    = genfnc;
    evscrd_vect    = genfnc;
    evpagd_vect    = genfnc;
    evpagu_vect    = genfnc;
    evtab_vect     = genfnc;
    eventer_vect   = genfnc;
    evinsert_vect  = genfnc;
    evinsertl_vect = genfnc;
    evinsertt_vect = genfnc;
    evdel_vect     = genfnc;
    evdell_vect    = genfnc;
    evdelcf_vect   = genfnc;
    evdelcb_vect   = genfnc;
    evcopy_vect    = genfnc;
    evcopyl_vect   = genfnc;
    evcan_vect     = genfnc;
    evstop_vect    = genfnc;
    evcont_vect    = genfnc;
    evprint_vect   = genfnc;
    evprintb_vect  = genfnc;
    evprints_vect  = genfnc;
    evfun_vect     = funfnc;
    evmenu_vect    = genfnc;
    evmouba_vect   = moubafnc;
    evmoubd_vect   = moubdfnc;
    evmoumov_vect  = moumovfnc;
    evtim_vect     = timfnc;
    evjoyba_vect   = joybafnc;
    evjoybd_vect   = joybdfnc;
    evjoymov_vect  = joymovfnc;
    evresize_vect  = resizefnc;
    evfocus_vect   = genfnc;
    evnofocus_vect = genfnc;
    evhover_vect   = genfnc;
    evnohover_vect = genfnc;
    evterm_vect    = genfnc;
    evframe_vect   = genfnc;

}

/** ****************************************************************************

Execute event function handler

Executes a function handler for a given event. Each function returns a boolean
that is true if it has handled the given event.

*******************************************************************************/

void evtfnc(ami_evtrec* er)

{

    switch (er->etype) {

        case ami_etchar:    er->handled = (*evchar_vect)(er->echar); break;
        case ami_etup:      er->handled = (*evup_vect)(); break;    
        case ami_etdown:    er->handled = (*evdown_vect)(); break;
        case ami_etleft:    er->handled = (*evleft_vect)(); break;
        case ami_etright:   er->handled = (*evright_vect)(); break;
        case ami_etleftw:   er->handled = (*evleftw_vect)(); break;
        case ami_etrightw:  er->handled = (*evrightw_vect)(); break;
        case ami_ethome:    er->handled = (*evhome_vect)(); break;
        case ami_ethomes:   er->handled = (*evhomes_vect)(); break;
        case ami_ethomel:   er->handled = (*evhomel_vect)(); break;
        case ami_etend:     er->handled = (*evend_vect)(); break;
        case ami_etends:    er->handled = (*evends_vect)(); break;
        case ami_etendl:    er->handled = (*evendl_vect)(); break;
        case ami_etscrl:    er->handled = (*evscrl_vect)(); break;
        case ami_etscrr:    er->handled = (*evscrr_vect)(); break;
        case ami_etscru:    er->handled = (*evscru_vect)(); break;
        case ami_etscrd:    er->handled = (*evscrd_vect)(); break;
        case ami_etpagd:    er->handled = (*evpagd_vect)(); break;
        case ami_etpagu:    er->handled = (*evpagu_vect)(); break;
        case ami_ettab:     er->handled = (*evtab_vect)(); break;
        case ami_etenter:   er->handled = (*eventer_vect)(); break;
        case ami_etinsert:  er->handled = (*evinsert_vect)(); break;
        case ami_etinsertl: er->handled = (*evinsertl_vect)(); break;
        case ami_etinsertt: er->handled = (*evinsertt_vect)(); break;
        case ami_etdel:     er->handled = (*evdel_vect)(); break;
        case ami_etdell:    er->handled = (*evdell_vect)(); break;
        case ami_etdelcf:   er->handled = (*evdelcf_vect)(); break;
        case ami_etdelcb:   er->handled = (*evdelcb_vect)(); break;
        case ami_etcopy:    er->handled = (*evcopy_vect)(); break;
        case ami_etcopyl:   er->handled = (*evcopyl_vect)(); break;
        case ami_etcan:     er->handled = (*evcan_vect)(); break;
        case ami_etstop:    er->handled = (*evstop_vect)(); break;
        case ami_etcont:    er->handled = (*evcont_vect)(); break;
        case ami_etprint:   er->handled = (*evprint_vect)(); break;
        case ami_etprintb:  er->handled = (*evprintb_vect)(); break;
        case ami_etprints:  er->handled = (*evprints_vect)(); break;
        case ami_etfun:     er->handled = (*evfun_vect)(er->fkey); break;
        case ami_etmenu:    er->handled = (*evmenu_vect)(); break;
        case ami_etmouba:   er->handled = (*evmouba_vect)(er->amoun, er->amoubn);
                           break;
        case ami_etmoubd:   er->handled = (*evmoubd_vect)(er->dmoun, er->dmoubn);
                           break;
        case ami_etmoumov:  
            er->handled = (*evmoumov_vect)(er->mmoun, er->moupx, er->moupy); 
            break;
        case ami_ettim:     er->handled = (*evtim_vect)(er->timnum); break;
        case ami_etjoyba:   er->handled = (*evjoyba_vect)(er->ajoyn, er->ajoybn);
                           break;
        case ami_etjoybd:   er->handled = (*evjoybd_vect)(er->djoyn, er->djoybn);
                           break;
        case ami_etjoymov:  
            er->handled = (*evjoymov_vect)(er->mjoyn, er->joypx, er->joypy, 
                                           er->joypz); 
            break;
        case ami_etresize:  er->handled = (*evresize_vect)(er->rszx, er->rszy); 
                           break;
        case ami_etfocus:   er->handled = (*evfocus_vect)(); break;
        case ami_etnofocus: er->handled = (*evnofocus_vect)(); break;
        case ami_ethover:   er->handled = (*evhover_vect)(); break;
        case ami_etnohover: er->handled = (*evnohover_vect)(); break;
        case ami_etterm:    er->handled = (*evterm_vect)(); break;
        case ami_etframe:   er->handled = (*evframe_vect)(); break;

        default: ;

    }

}

/** ****************************************************************************

Function event overrides

Each routine overrides an individual function event routine. The EVTOVER() macro
gives both event function overrides, and the override of the override routine
itself.

*******************************************************************************/

EVTOVER(char)
EVTOVER(up)
EVTOVER(down)
EVTOVER(left)
EVTOVER(right)
EVTOVER(leftw)
EVTOVER(rightw)
EVTOVER(home)
EVTOVER(homes)
EVTOVER(homel)
EVTOVER(end)
EVTOVER(ends)
EVTOVER(endl)
EVTOVER(scrl)
EVTOVER(scrr)
EVTOVER(scru)
EVTOVER(scrd)
EVTOVER(pagd)
EVTOVER(pagu)
EVTOVER(tab)
EVTOVER(enter)
EVTOVER(insert)
EVTOVER(insertl)
EVTOVER(insertt)
EVTOVER(del)
EVTOVER(dell)
EVTOVER(delcf)
EVTOVER(delcb)
EVTOVER(copy)
EVTOVER(copyl)
EVTOVER(can)
EVTOVER(stop)
EVTOVER(cont)
EVTOVER(print)
EVTOVER(printb)
EVTOVER(prints)
EVTOVER(fun)
EVTOVER(menu)
EVTOVER(mouba)
EVTOVER(moubd)
EVTOVER(moumov)
EVTOVER(tim)
EVTOVER(joyba)
EVTOVER(joybd)
EVTOVER(joymov)
EVTOVER(resize)
EVTOVER(focus)
EVTOVER(nofocus)
EVTOVER(hover)
EVTOVER(nohover)
EVTOVER(term)
EVTOVER(frame)

/** ****************************************************************************

Acquire next input event

Decodes the input for various events. These are sent to the override handlers
first, then if no chained handler dealt with it, we return the event to the
caller.

*******************************************************************************/

APIOVER(event)
void ami_event(FILE* f, ami_evtrec* er) { (*event_vect)(f, er); }
static void event_ivf(FILE* f, ami_evtrec *er)

{

    static int ecnt = 0; /* PA event counter */

    dbg_printf(dlapi, "API\n");
    do { /* loop handling via event vectors */

        /* reset the response timer */
        if (unresponse) {

            system_event_deasetim(respsev); /* kill the response timer */
            /* reset any response state */
            if (respto) {

                if (titsav) trm_title(titsav); /* set previous title */
                else trm_title(""); /* reset message (should reset previous) */
                respto = FALSE; /* reset state */

            }

        }

        /* get next input event */
        dequepaevt(er); /* get next queued event */
        pthread_mutex_lock(&termlock); /* lock terminal broadlock */
        /* handle actions we must take here */
        if (er->etype == ami_etresize) {

            /* set new size */
            dimx = er->rszx;
            dimy = er->rszy;
            /* linux/xterm has an oddity here, if the winch contracts in y, it
               occasionally relocates the buffer contents up. This means we
               always need to refresh, and means it can flash. */
            restore(screens[curdsp-1]);

        } else if (er->etype == ami_etterm) 
            /* set user ordered termination */
            fend = TRUE;
        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        er->handled = 1; /* set event is handled by default */
        (evtshan)(er); /* call master event handler */
        if (!er->handled) { /* send it to fanout */

            if (er->etype <= ami_etframe) {

                er->handled = 1; /* set event is handled by default */
                (*evthan[er->etype])(er); /* call event handler first */
                if (!er->handled) 
                    evtfnc(er); /* send it to function handler */

            }

        }

    } while (er->handled);
    /* reset the response timer */
    if (unresponse) respsev = system_event_addsetim(respsev, RESPTIME, FALSE);

    /* event not handled, return it to the caller */

    /* do diagnostic dump of PA events */
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (dmpevt) {

        prtevt(er); fprintf(stderr, "\n"); fflush(stderr);

    }
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Set timer

*******************************************************************************/

APIOVER(timer)
void ami_timer(FILE* f, int i, long t, int r) { (*timer_vect)(f, i, t, r); }
static void timer_ivf(/* file to send event to */              FILE* f,
                      /* timer handle */                       int   i,
                      /* number of 100us counts */             long  t,
                      /* timer is to rerun after completion */ int   r)

{

    dbg_printf(dlapi, "API\n");
    if (i < 1 || i > AMI_MAXTIM) error(ami_dispeinvthn); /* invalid timer handle */
    pthread_mutex_lock(&timlock); /* take the timer lock */
    timtbl[i-1] = system_event_addsetim(timtbl[i-1], t, r);
    pthread_mutex_unlock(&timlock); /* release the timer lock */

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.
Killed timers are not removed. Once a timer is set active, it is always set
in reserve.

*******************************************************************************/

APIOVER(killtimer)
void ami_killtimer(FILE* f, int   i ) { (*killtimer_vect)(f, i ); }
static void killtimer_ivf(/* file to kill timer on */ FILE *f,
                  /* handle of timer */       int i)

{

    dbg_printf(dlapi, "API\n");
    if (i < 1 || i > AMI_MAXTIM) error(ami_dispeinvthn); /* invalid timer handle */
    pthread_mutex_lock(&timlock); /* take the timer lock */
    if (timtbl[i-1] <= 0) {

        pthread_mutex_unlock(&timlock); /* release the timer lock */
        error(ami_dispetimacc); /* no such timer */

    }
    system_event_deasetim(timtbl[i-1]); /* deactivate timer */
    pthread_mutex_unlock(&timlock); /* release the timer lock */

}

/** ****************************************************************************

Returns number of mice

Returns the number of mice attached. In xterm, we can't actually determine if
we have a mouse or not, so we just assume we have one. It will be a dead mouse
if none is available, never changing it's state.

*******************************************************************************/

APIOVER(mouse)
int ami_mouse(FILE* f) { return ((*mouse_vect)(f)); }
static int mouse_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    return !!mouseenb; /* set 0 or 1 mouse */

}

/** ****************************************************************************

Returns number of buttons on a mouse

Returns the number of buttons implemented on a given mouse. With xterm we have
to assume 3 buttons.

*******************************************************************************/

APIOVER(mousebutton)
int ami_mousebutton(FILE* f, int m) { return ((*mousebutton_vect)(f, m)); }
static int mousebutton_ivf(FILE *f, int m)

{

    dbg_printf(dlapi, "API\n");
    return 3; /* set three buttons */

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

APIOVER(joystick)
int ami_joystick(FILE* f) { return ((*joystick_vect)(f)); }
static int joystick_ivf(FILE *f)

{

    dbg_printf(dlapi, "API\n");
    return (numjoy); /* set number of joysticks */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

APIOVER(joybutton)
int ami_joybutton(FILE* f, int j) { return ((*joybutton_vect)(f, j)); }
static int joybutton_ivf(FILE *f, int j)

{

    int b;

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (j < 1 || j > numjoy) {

        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        error(ami_dispeinvjoy); /* bad joystick id */

    }
    if (!joytab[j-1]) {

        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        error(ami_dispesystem); /* should be a table entry */

    }
    b = joytab[j-1]->button; /* get button count */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

    return (b); /* return button count */

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

APIOVER(joyaxis)
int ami_joyaxis(FILE* f, int j) { return ((*joyaxis_vect)(f, j)); }
static int joyaxis_ivf(FILE *f, int j)

{

    int ja;

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (j < 1 || j > numjoy) {

        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        error(ami_dispeinvjoy); /* bad joystick id */

    }
    if (!joytab[j-1]) {

        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        error(ami_dispesystem); /* should be a table entry */

    }
    ja = joytab[j-1]->axis; /* get axis number */
    if (ja > 6) ja = 6; /* limit to 6 maximum */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

    return (ja); /* set axis number */

}

/** ****************************************************************************

settab

Sets a tab. The tab number t is 1 to n, and indicates the column for the tab.
Setting a tab stop means that when a tab is received, it will move to the next
tab stop that is set. If there is no next tab stop, nothing will happen.

*******************************************************************************/

APIOVER(settab)
void ami_settab(FILE* f, int t) { (*settab_vect)(f, t); }
static void settab_ivf(FILE* f, int t)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (t < 1 || t > dimx) {

        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        error(ami_dispeinvtab); /* invalid tab position */

    }
    tabs[t-1] = 1; /* set tab position */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

restab

Resets a tab. The tab number t is 1 to n, and indicates the column for the tab.

*******************************************************************************/

APIOVER(restab)
void ami_restab(FILE* f, int t) { (*restab_vect)(f, t); }
static void restab_ivf(FILE* f, int t)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (t < 1 || t > dimx) {

        pthread_mutex_unlock(&termlock); /* release terminal broadlock */
        error(ami_dispeinvtab); /* invalid tab position */

    }
    tabs[t-1] = 0; /* reset tab position */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

clrtab

Clears all tabs.

*******************************************************************************/

APIOVER(clrtab)
void ami_clrtab(FILE* f) { (*clrtab_vect)(f); }
static void clrtab_ivf(FILE* f)

{

    int i;

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    for (i = 0; i < dimx; i++) tabs[i] = 0; /* clear all tab stops */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

funkey

Return number of function keys. xterm gives us F1 to F9, takes F10 and F11,
and leaves us F12. It only reserves F10 of the shifted keys, takes most of
the ALT-Fx keys, and leaves all of the CONTROL-Fx keys.
The tradition in PA is to take the F1-F10 keys (it's a nice round number),
but more can be allocated if needed.

*******************************************************************************/

APIOVER(funkey)
int ami_funkey(FILE* f) { return ((*funkey_vect)(f)); }
static int funkey_ivf(FILE* f)

{

    dbg_printf(dlapi, "API\n");
    return MAXFKEY;

}

/** ****************************************************************************

Frametimer

Enables or disables the framing timer.

*******************************************************************************/

APIOVER(frametimer)
void ami_frametimer(FILE* f, int e) { (*frametimer_vect)(f, e); }
static void frametimer_ivf(FILE* f, int e)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&timlock); /* take the timer lock */
    if (e) { /* set framing timer to run */

        frmsev = system_event_addsetim(frmsev, 166, TRUE);

    } else {

        system_event_deasetim(frmsev);

    }
    pthread_mutex_unlock(&timlock); /* release the timer lock */

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

*******************************************************************************/

APIOVER(autohold)
void ami_autohold(int e) { (*autohold_vect)(e); }
static void autohold_ivf(int e)

{

    dbg_printf(dlapi, "API\n");
    fautohold = e; /* set new state of autohold */

}

/** ****************************************************************************

Write string direct

Writes a string direct to the terminal, bypassing character handling.

*******************************************************************************/

APIOVER(wrtstr)
void ami_wrtstr(FILE* f, char* s) { (*wrtstr_vect)(f, s); }
static void wrtstr_ivf(FILE* f, char *s)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    putstrc(s);
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Write string direct with length

Writes a string with length direct to the terminal, bypassing character
handling.

*******************************************************************************/

APIOVER(wrtstrn)
void ami_wrtstrn(FILE* f, char* s, int n) { (*wrtstrn_vect)(f, s, n); }
static void wrtstrn_ivf(FILE* f, char *s, int n)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    while (n--) putchr(*s++);
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Size buffer

Sets or resets the size of the buffer surface.

*******************************************************************************/

APIOVER(sizbuf)
void ami_sizbuf(FILE* f, int x, int y) { (*sizbuf_vect)(f, x, y); }
static void sizbuf_ivf(FILE* f, int x, int y)

{

    int si;

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    if (bufx != x || bufy != y) {

        /* set new buffer size */
        bufx = x;
        bufy = y;
        /* the buffer asked is not the same as present */
        for (si = 0; si < MAXCON; si++) {

            /* free up any/all present buffers */
            if (screens[si]) {

                free(screens[si]);
                screens[si] = NULL; /* clear it */

            }

        }
        /* allocate new update screen */
        screens[curupd-1] = malloc(sizeof(scnrec)*y*x);
        /* clear it */
        clrbuf(screens[curupd-1]);
        if (curupd != curdsp) { /* display screen not the same */

            /* allocate display screen */
            screens[curdsp-1] = malloc(sizeof(scnrec)*y*x);
            /* clear it */
            clrbuf(screens[curdsp-1]);

        }
        /* redraw screen */
        restore(screens[curdsp-1]);

    }
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Set window title with length

Sets the title of the current window.

*******************************************************************************/

APIOVER(titlen)
void ami_titlen(FILE* f, char* ts, int l) { (*titlen_vect)(f, ts, l); }
static void titlen_ivf(FILE* f, char* ts, int l)
    
{ 

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    trm_titlen(ts, l); /* set title */
    if (titsav) free(titsav); /* free any existing title string */
    titsav = malloc(l+1);
    if (!titsav) error(ami_dispenomem); /* no memory */
    strncpy(titsav, ts, l); /* place string */
    titsav[l] = 0; /* terminate */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

APIOVER(title)
void ami_title(FILE* f, char* ts) { (*title_vect)(f, ts); }
static void title_ivf(FILE* f, char* ts)
    
{ 

    dbg_printf(dlapi, "API\n");
    titlen_ivf(f, ts, strlen(ts)); /* set title */

}

/** ****************************************************************************

Set foreground color rgb

Sets the foreground color from individual r, g, b values.

*******************************************************************************/

APIOVER(fcolorc)
void ami_fcolorc(FILE* f, int r, int g, int b) { (*fcolorc_vect)(f, r, g, b); }
static void fcolorc_ivf(FILE* f, int r, int g, int b)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    forec = colrgbnum(r, g, b);
    forergb = rgb2rgbp(r, g, b);
    if (curupd == curdsp) 
#ifdef NATIVE24
        trm_fcolorrgb(forergb); /* set color */
#else
        trm_fcolor(forec); /* set color */
#endif
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Set background color

Sets the background color from individual r, g, b values.

*******************************************************************************/

APIOVER(bcolorc)
void ami_bcolorc(FILE* f, int r, int g, int b) { (*bcolorc_vect)(f, r, g, b); }
static void bcolorc_ivf(FILE* f, int r, int g, int b)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    backc = colrgbnum(r, g, b);
    backrgb = rgb2rgbp(r, g, b);
    if (curupd == curdsp) 
#ifdef NATIVE24
        trm_bcolorrgb(backrgb); /* set color */
#else
        trm_bcolor(backc); /* set color */
#endif
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing even handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

APIOVER(eventover)
void ami_eventover(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)
    { (*eventover_vect)(e, eh, oeh); }
static void eventover_ivf(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    *oeh = evthan[e]; /* save existing event handler */
    evthan[e] = eh; /* place new event handler */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Override master event handler

Overrides or "hooks" the master event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

APIOVER(eventsover)
void ami_eventsover(ami_pevthan eh,  ami_pevthan* oeh)
    { (*eventsover_vect)(eh, oeh); }
static void eventsover_ivf(ami_pevthan eh,  ami_pevthan* oeh)

{

    dbg_printf(dlapi, "API\n");
    pthread_mutex_lock(&termlock); /* lock terminal broadlock */
    *oeh = evtshan; /* save existing event handler */
    evtshan = eh; /* place new event handler */
    pthread_mutex_unlock(&termlock); /* release terminal broadlock */

}

/** ****************************************************************************

Management extension package

This section is a series of override vectors for unimplemented window management
calls.

*******************************************************************************/

APIOVER(sendevent)
void ami_sendevent(FILE* f, ami_evtrec* er) { (*sendevent_vect)(f, er); }
static void sendevent_ivf(FILE* f, ami_evtrec* er) { error(ami_dispesendevent_unimp); }

APIOVER(openwin)
void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)
    { (*openwin_vect)(infile, outfile, parent, wid); }
static void openwin_ivf(FILE** infile, FILE** outfile, FILE* parent, int wid)
    { error(ami_dispeopenwin_unimp); }

APIOVER(buffer)
void ami_buffer(FILE* f, int e) { (*buffer_vect)(f, e); }
static void buffer_ivf(FILE* f, int e) { error(ami_dispebuffer_unimp); }

APIOVER(getsiz)
void ami_getsiz(FILE* f, int* x, int* y) { (*getsiz_vect)(f, x, y); }
static void getsiz_ivf(FILE* f, int* x, int* y) { error(ami_dispegetsiz_unimp); }

APIOVER(setsiz)
void ami_setsiz(FILE* f, int x, int y) { (*setsiz_vect)(f, x, y); }
static void setsiz_ivf(FILE* f, int x, int y) { error(ami_dispesetsiz_unimp); }

APIOVER(setpos)
void ami_setpos(FILE* f, int x, int y) { (*setpos_vect)(f, x, y); }
static void setpos_ivf(FILE* f, int x, int y) { error(ami_dispesetpos_unimp); }

APIOVER(scnsiz)
void ami_scnsiz(FILE* f, int* x, int* y) { (*scnsiz_vect)(f, x, y); }
static void scnsiz_ivf(FILE* f, int* x, int* y) { error(ami_dispescnsiz_unimp); }

APIOVER(scncen)
void ami_scncen(FILE* f, int* x, int* y) { (*scncen_vect)(f, x, y); }
static void scncen_ivf(FILE* f, int* x, int* y) { error(ami_dispescncen_unimp); }

APIOVER(winclient)
void ami_winclient(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)
    { (*winclient_vect)(f, cx, cy, wx, wy, ms); }
static void winclient_ivf(FILE* f, int cx, int cy, int* wx, int* wy, 
                          ami_winmodset ms)
    { error(ami_dispewinclient_unimp); }

APIOVER(front)
void ami_front(FILE* f) { (*front_vect)(f); }
static void front_ivf(FILE* f) { error(ami_dispefront_unimp); }

APIOVER(back)
void ami_back(FILE* f) { (*back_vect)(f); }
static void back_ivf(FILE* f) { error(ami_dispeback_unimp); }

APIOVER(frame)
void ami_frame(FILE* f, int e) { (*frame_vect)(f, e); }
static void frame_ivf(FILE* f, int e) { error(ami_dispeframe_unimp); }

APIOVER(sizable)
void ami_sizable(FILE* f, int e) { (*sizable_vect)(f, e); }
static void sizable_ivf(FILE* f, int e) { error(ami_dispesizable_unimp); }

APIOVER(sysbar)
void ami_sysbar(FILE* f, int e) { (*sysbar_vect)(f, e); }
static void sysbar_ivf(FILE* f, int e) { error(ami_dispesysbar_unimp); }

APIOVER(menu)
void ami_menu(FILE* f, ami_menuptr m) { (*menu_vect)(f, m); }
static void menu_ivf(FILE* f, ami_menuptr m) { error(ami_dispemenu_unimp); }

APIOVER(menuena)
void ami_menuena(FILE* f, int id, int onoff) { (*menuena_vect)(f, id, onoff); }
static void menuena_ivf(FILE* f, int id, int onoff) { error(ami_dispemenuena_unimp); }

APIOVER(menusel)
void ami_menusel(FILE* f, int id, int select) { (*menusel_vect)(f, id, select); }
static void menusel_ivf(FILE* f, int id, int select) { error(ami_dispemenusel_unimp); }

APIOVER(stdmenu)
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
    { (*stdmenu_vect)(sms, sm, pm); }
static void stdmenu_ivf(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
    { error(ami_dispestdmenu_unimp); }

APIOVER(getwinid)
int ami_getwinid(void) { return ((*getwinid_vect)()); }
static int getwinid_ivf(void) { error(ami_dispegetwinid_unimp); return 0; }

APIOVER(focus)
void ami_focus(FILE* f) { (*focus_vect)(f); }
static void focus_ivf(FILE* f) { error(ami_dispefocus_unimp); }

/*******************************************************************************

Module startup/shutdown

*******************************************************************************/

/** ****************************************************************************

Initialize output terminal

We initialize all variables and tables, then clear the screen to bring it to
a known state.

This is the startup routine for terminal, and is executed automatically
before the client program runs.

*******************************************************************************/

static void ami_init_terminal (int argc, char* argv[]) __attribute__((constructor (106)));
static void ami_init_terminal(int argc, char* argv[])

{

    /** index for events */            ami_evtcod      e;
    /** build new terminal settings */ struct termios raw;
    /** index */                       int            i;
    /** digit count integer */         int            dci;
    /** joystick index */              int            ji;
    /** joystick file id */            int            joyfid;
    /** joystick device name */        char           joyfil[] = "/dev/input/js0";
    /** joystick parameter read */     char           jc;
    /** Linux return value */          int            r;
    /** root for config block */       ami_valptr      config_root;
    /** root for terminal block */     ami_valptr      term_root; 
                                       ami_valptr      vp;
                                       char*          errstr;

    /* set override vectors to defaults */
    cursor_vect      = cursor_ivf;
    cursor_vect      = cursor_ivf;
    maxx_vect        = maxx_ivf;
    maxy_vect        = maxy_ivf;
    home_vect        = home_ivf;
    del_vect         = del_ivf;
    up_vect          = up_ivf;
    down_vect        = down_ivf;
    left_vect        = left_ivf;
    right_vect       = right_ivf;
    blink_vect       = blink_ivf;
    reverse_vect     = reverse_ivf;
    underline_vect   = underline_ivf;
    superscript_vect = superscript_ivf;
    subscript_vect   = subscript_ivf;
    italic_vect      = italic_ivf;
    bold_vect        = bold_ivf;
    strikeout_vect   = strikeout_ivf;
    standout_vect    = standout_ivf;
    fcolor_vect      = fcolor_ivf;
    bcolor_vect      = bcolor_ivf;
    fcolorc_vect     = fcolorc_ivf;
    bcolorc_vect     = bcolorc_ivf;
    auto_vect        = auto_ivf;
    curvis_vect      = curvis_ivf;
    scroll_vect      = scroll_ivf;
    curx_vect        = curx_ivf;
    cury_vect        = cury_ivf;
    curbnd_vect      = curbnd_ivf;
    select_vect      = select_ivf;
    event_vect       = event_ivf;
    timer_vect       = timer_ivf;
    killtimer_vect   = killtimer_ivf;
    mouse_vect       = mouse_ivf;
    mousebutton_vect = mousebutton_ivf;
    joystick_vect    = joystick_ivf;
    joybutton_vect   = joybutton_ivf;
    joyaxis_vect     = joyaxis_ivf;
    settab_vect      = settab_ivf;
    restab_vect      = restab_ivf;
    clrtab_vect      = clrtab_ivf;
    funkey_vect      = funkey_ivf;
    frametimer_vect  = frametimer_ivf;
    autohold_vect    = autohold_ivf;
    wrtstr_vect      = wrtstr_ivf;
    wrtstrn_vect     = wrtstrn_ivf;
    eventover_vect   = eventover_ivf;
    eventsover_vect  = eventsover_ivf;
    sendevent_vect   = sendevent_ivf;
    title_vect       = title_ivf;
    titlen_vect      = titlen_ivf;
    openwin_vect     = openwin_ivf;
    buffer_vect      = buffer_ivf;
    sizbuf_vect      = sizbuf_ivf;
    getsiz_vect      = getsiz_ivf;
    setsiz_vect      = setsiz_ivf;
    setpos_vect      = setpos_ivf;
    scnsiz_vect      = scnsiz_ivf;
    scncen_vect      = scncen_ivf;
    winclient_vect   = winclient_ivf;
    front_vect       = front_ivf;
    back_vect        = back_ivf;
    frame_vect       = frame_ivf;
    sizable_vect     = sizable_ivf;
    sysbar_vect      = sysbar_ivf;
    menu_vect        = menu_ivf;
    menuena_vect     = menuena_ivf;
    menusel_vect     = menusel_ivf;
    stdmenu_vect     = stdmenu_ivf;
    getwinid_vect    = getwinid_ivf;
    focus_vect       = focus_ivf;

    /* set override override vectors to defaults */
    charover_vect    = charover_ivf;
    upover_vect      = upover_ivf;
    downover_vect    = downover_ivf;
    leftover_vect    = leftover_ivf;
    rightover_vect   = rightover_ivf;
    leftwover_vect   = leftwover_ivf;
    rightwover_vect  = rightwover_ivf;
    homeover_vect    = homeover_ivf;
    homesover_vect   = homesover_ivf;
    homelover_vect   = homelover_ivf;
    endover_vect     = endover_ivf;
    endsover_vect    = endsover_ivf;
    endlover_vect    = endlover_ivf;
    scrlover_vect    = scrlover_ivf;
    scrrover_vect    = scrrover_ivf;
    scruover_vect    = scruover_ivf;
    scrdover_vect    = scrdover_ivf;
    pagdover_vect    = pagdover_ivf;
    paguover_vect    = paguover_ivf;
    tabover_vect     = tabover_ivf;
    enterover_vect   = enterover_ivf;
    insertover_vect  = insertover_ivf;
    insertlover_vect = insertlover_ivf;
    inserttover_vect = inserttover_ivf;
    delover_vect     = delover_ivf;
    dellover_vect    = dellover_ivf;
    delcfover_vect   = delcfover_ivf;
    delcbover_vect   = delcbover_ivf;
    copyover_vect    = copyover_ivf;
    copylover_vect   = copylover_ivf;
    canover_vect     = canover_ivf;
    stopover_vect    = stopover_ivf;
    contover_vect    = contover_ivf;
    printover_vect   = printover_ivf;
    printbover_vect  = printbover_ivf;
    printsover_vect  = printsover_ivf;
    funover_vect     = funover_ivf;
    menuover_vect    = menuover_ivf;
    moubaover_vect   = moubaover_ivf;
    moubdover_vect   = moubdover_ivf;
    moumovover_vect  = moumovover_ivf;
    timover_vect     = timover_ivf;
    joybaover_vect   = joybaover_ivf;
    joybdover_vect   = joybdover_ivf;
    joymovover_vect  = joymovover_ivf;
    resizeover_vect  = resizeover_ivf;
    focusover_vect   = focusover_ivf;
    nofocusover_vect = nofocusover_ivf;
    hoverover_vect   = hoverover_ivf;
    nohoverover_vect = nohoverover_ivf;
    termover_vect    = termover_ivf;
    frameover_vect   = frameover_ivf;

    /*
     * Set error override vector
     */
    error_vect = error_ivf;

    /* 
     * Set linux error override vector
     */
    linuxerror_vect = linuxerror_ivf;

    /*
     * Event override vectors
     */
    inifncvec();

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* change to alternate screen/turn off wrap */
    printf("\033[?1049h\033[H"); fflush(stdout);

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
//    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    /* set internal configurable settings */
    dmpevt         = DMPEVT;         /* dump Petit-Ami messages */
    joyenb         = JOYENB;         /* enable/disable joystick */
    mouseenb       = MOUSEENB;       /* enable/disable mouse */
    unresponse     = UNRESPONSE;     /* check unresponsive program */
    unresponsekill = UNRESPONSEKILL; /* kill unresponsive program */
    xtermtitle     = XTERMTITLE;     /* use xterm title bar mode */

    /* set default screen geometry */
    dimx = DEFXD;
    dimy = DEFYD;

    /* try getting size from system */
    findsize(&dimx, &dimy);

    /* set buffer size */
    bufx = dimx;
    bufy = dimy;

    /* clear title string */
    titsav = NULL;

    /* get setup configuration */
    config_root = NULL;
    ami_config(&config_root);

    /* find "terminal" block */
    term_root = ami_schlst("terminal", config_root);
    if (term_root && term_root->sublist) term_root = term_root->sublist;

    if (term_root) {

        /* find x an y max if they exist */
        vp = ami_schlst("maxxd", term_root);
        if (vp) {

            bufx = strtol(vp->value, &errstr, 10);
            if (*errstr) error(ami_dispecfgval);

        }
        vp = ami_schlst("maxyd", term_root);
        if (vp) {

            bufy = strtol(vp->value, &errstr, 10);
            if (*errstr) error(ami_dispecfgval);

        }
        /* enable/disable joystick */
        vp = ami_schlst("joystick", term_root);
        if (vp) {

            joyenb = strtol(vp->value, &errstr, 10);
            if (*errstr) error(ami_dispecfgval);

        }
        /* enable/disable mouse */
        vp = ami_schlst("mouse", term_root);
        if (vp) {

            mouseenb = strtol(vp->value, &errstr, 10);
            if (*errstr) error(ami_dispecfgval);

        }
        /* dump PA events */
        vp = ami_schlst("dump_event", term_root);
        if (vp) {

            dmpevt = strtol(vp->value, &errstr, 10);
            if (*errstr) error(ami_dispecfgval);

        }

    }

    /* clear screens array */
    for (curupd = 1; curupd <= MAXCON; curupd++) screens[curupd-1] = NULL;
    /* allocate screen array */
    screens[0] = malloc(sizeof(scnrec)*bufy*bufx);

    /* allocate tab array */
    tabs = malloc(sizeof(int)*dimx);

    curdsp = 1; /* set display current screen */
    curupd = 1; /* set current update screen */
    trm_wrapoff(); /* physical wrap is always off */
    scroll = 1; /* turn on virtual wrap */
    curon = 1; /* set default cursor on */
    hover = FALSE; /* set hover off */
    hovsev = 0; /* set no hover timer */
    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */
    errflg = FALSE; /* set no error occurred */
    trm_curon(); /* and make sure that is so */
    iniscn(screens[curdsp-1]); /* initalize screen */
    restore(screens[curdsp-1]); /* place on display */

    inpptr = -1; /* set no input line active */
#ifdef ALLOWUTF8
    utf8cnt = 0; /* clear utf-8 character count */
#endif
    paqfre    = NULL;  /* clear pa event free queue */
    paqevt    = NULL;  /* clear pa event input queue */
    evtquecnt = 0;     /* clear event queue counter */
    evtquemax = 0;     /* clear event queue max */
    matrem    = 0;     /* set no duplicate removal */

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_etframe; e++) evthan[e] = defaultevent;

    /* clear keyboard match buffer */
    keylen = 0;

    /* set maximum power of 10 for conversions */
    maxpow10 = INT_MAX;
    dci = 0;
    while (maxpow10) { maxpow10 /= 10; dci++; }
    /* find top power of 10 */
    maxpow10 = 1;
    while (--dci) maxpow10 = maxpow10*10;

    /*
     * Set terminal in raw mode
     */
    tcgetattr(0,&trmsav); /* save original state of terminal */
    raw = trmsav; /* copy into new state */

    /* input modes - clear indicated ones giving: no break, no CR to NL,
       no parity check, no strip char, no start/stop output (sic) control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* output modes - clear giving: no post processing such as NL to CR+NL */
    raw.c_oflag &= ~(OPOST);

    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);

    /* local modes - clear giving: echoing off, canonical off (no erase with
       backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* add input file event */
    inpsev = system_event_addseinp(0);

    /* clear the timers table */
    for (i = 0; i < AMI_MAXTIM; i++) timtbl[i] = 0;

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
                joytab[numjoy] = malloc(sizeof(joyrec));
                joytab[numjoy]->fid = joyfid; /* set fid */
                /* enable system event */
                joytab[numjoy]->sid = system_event_addseinp(joyfid);
                joytab[numjoy]->ax = 0; /* clear joystick axis saves */
                joytab[numjoy]->ay = 0;
                joytab[numjoy]->az = 0;
                joytab[numjoy]->a4 = 0;
                joytab[numjoy]->a5 = 0;
                joytab[numjoy]->a6 = 0;
                joytab[numjoy]->no = numjoy+1; /* set logical number */
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X or BSD */
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

    /* clear tabs and set to 8ths */
    for (i = 1; i <= dimx; i++) tabs[i-1] = ((i-1)%8 == 0) && (i != 1);

    /* clear mouse state. Note mouse will be all buttons deasserted, and
       location 0,0 and thus offscreen. This is important, since in xterm we
       can't know if there is or is not a mouse, thus a missing mouse is dead
       and offscreen */
    button1 = nbutton1 = 1;
    button2 = nbutton2 = 1;
    button3 = nbutton3 = 1;
    mpx = nmpx = -INT_MAX;
    mpy = nmpy = -INT_MAX;

    /* now signal xterm we want all mouse events including all movements */
    putstrc("\33[?1003h");
#ifdef MOUSESGR
    putstrc("\33[?1006h"); /* enable SGR mouse mode (extended) */
#endif

    /* signal we want xterm focus in/out events */
    putstrc("\33[?1004h");

    /* enable windows change signal */
    winchsev = system_event_addsesig(SIGWINCH);

    /* restore terminal state after flushing */
    tcsetattr(0,TCSAFLUSH,&raw);

    /* initialize the event tracking lock */
    r = pthread_mutex_init(&evtlck, NULL);
    if (r) linuxerror(r);

    /* initialize the time table lock */
    r = pthread_mutex_init(&timlock, NULL);
    if (r) linuxerror(r);

    /* initialize the broadlock for terminal APIs */
    r = pthread_mutex_init(&termlock, NULL);
    if (r) linuxerror(r);

    /* initialize queue not empty semaphore */
    r = pthread_cond_init(&evtquene, NULL);
    if (r) linuxerror(r);

    /* start event thread */
    r = pthread_create(&eventthread, NULL, eventtask, NULL);
    if (r) linuxerror(r); /* error, print and exit */

    /*
     * Set response timer
     */
    if (unresponse)
        respsev = system_event_addsetim(respsev, RESPTIME, FALSE);

}

/** ****************************************************************************

Deinitialize output terminal

Removes overrides. We check if the contents of the override vector have our
vectors in them. If that is not so, then a stacking order violation occurred,
and that should be corrected.

*******************************************************************************/

static void ami_deinit_terminal (void) __attribute__((destructor (106)));
static void ami_deinit_terminal()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    ami_evtrec er;     /* event record */
    string    trmnam; /* termination name */
    char      fini[] = "Finished - ";
    ami_evtcod e;

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = ami_etchar; e <= ami_etframe; e++) evthan[e] = defaultevent;

    /* clear function events */
    inifncvec();

    /* if the program tries to exit when the user has not ordered an exit,
       it is assumed to be a windows "unaware" program. We stop before we
       exit these, so that their content may be viewed */
    if (!fend && fautohold && !errflg) {

        /* process automatic exit sequence */
#if !defined(__MACH__) && !defined(__FreeBSD__) /* Mac OS X or BSD */        
        /* construct final name for window */
        trmnam = malloc(strlen(fini)+strlen(program_invocation_short_name)+1);
        strcpy(trmnam, fini); /* place first part */
        /* place program name */
        strcat(trmnam, program_invocation_short_name);
#endif
        if (xtermtitle) {

            trm_title(trmnam); /* put up termination title */
            /* wait for user termination */
            do { ami_event(stdin, &er);
            } while (!fend && er.etype != ami_etenter);
            trm_title(""); /* blank out title */

        } else finish(trmnam);
        free(trmnam); /* free up termination name */

    }

    /* release the terminal broadlock */
    pthread_mutex_destroy(&termlock);

    /* release the time table lock */
    pthread_mutex_destroy(&timlock);

    /* release the event lock */
    pthread_mutex_destroy(&evtlck);

    /* restore cursor visible */
    trm_curon();

    /* restore terminal */
    tcsetattr(0,TCSAFLUSH,&trmsav);

    /* turn off xterm focus in/out events */
    putstrc("\33[?1004l");

    /* turn off mouse tracking */
    putstrc("\33[?1003l");

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
//    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose /* || cppunlink != iunlink */ || cpplseek != ilseek)
        error(ami_dispesystem);

    /* back to normal buffer on xterm */
    putstrc("\033[?1049l"); fflush(stdout);

}
