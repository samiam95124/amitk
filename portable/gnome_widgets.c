/** ****************************************************************************

\file

\brief WIDGETS PACKAGE FOR GNOME APPEARANCE

Copyright (C) 2021 Scott A. Franco

2021/08/08 S. A. Franco

This is the Gnome look and feel widget package for Petit-Ami. This is used for
systems that don't have a standard widget package, like XWindows. It uses
Petit-Ami graphics statements to construct and operate widgets, and thus is
portable to any system with Petit-Ami up to the graphical management level.

Although it is portable, the look and feel should match the system it is on.
Thus this package implements the Gnome look and feel, or at least a reasonable
subset of it.

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

/* select dialog/command line error */
#define USEDLG

#ifndef __MACH__ /* Mac OS X */
#define NOCANCEL /* include nocancel overrides */
#endif

#define MAXFIL 100 /* maximum open files */
#define MAXWIG 100 /* maximum widgets per window */
/* amount of space in pixels to add around scrollbar sliders */
#define ENDSPACE 6
#define ENDLEDSPC 10 /* space at start and end of text edit box */
/* user defined messages */
#define WMC_LGTFOC ami_etwidget+0 /* widget message code: light up focus */
#define WMC_DRKFOC ami_etwidget+1 /* widget message code: turn off focus */
#define TABHGT 2 /* tab bar tab height * char size y */

/* macro to make a color from RGB values */
#define RGB(r, g, b) (r<<16|g<<8|b)

/* macro to make a black and white value */
#define BW(v) (v<<16|v<<8|v)

/* unpack RGB packed values */
#define REDP(v)   (v >> 16 & 0xff)
#define GREENP(v) (v >> 8 & 0xff)
#define BLUEP(v)  (v & 0xff)

/* macros to unpack color table entries to INT_MAX ratioed numbers */
#define RED(v)   (INT_MAX/256*REDP(v))   /* red */
#define GREEN(v) (INT_MAX/256*GREENP(v)) /* green */
#define BLUE(v)  (INT_MAX/256*BLUEP(v)) /* blue */

/* default values for color table. Note these can be overridden.
 * To increase or decrease luminescence, add or subtract a BW() value from
 * another color or B&W value, but be careful not to overflow.
 */
#define TD_BACKPRESSED        BW(211)             /* button background pressed */
#define TD_BACK               BW(252)             /* button background not pressed */
#define TD_OUTLINE1           BW(196)             /* button outline */
#define TD_TEXT               BW(61)              /* widget face text */
#define TD_TEXTDIS            BW(191)             /* widget face text disabled */
#define TD_FOCUS              RGB(236, 174, 152)  /* widget focused outline */
#define TD_CHKRAD             RGB(146, 77,  139)  /* checkbox/radio button selected */
#define TD_CHKRADOUT          BW(186)             /* checkbox/radio button outline */
#define TD_SCROLLBACK         BW(210)             /* scrollbar background */
#define TD_SCROLLBAR          BW(135)             /* scrollbar not pressed */
#define TD_SCROLLBARPRESSED   RGB(195, 65,  19)   /* scrollbar pressed */
#define TD_NUMSELDIV          BW(239)             /* numselbox divider */
#define TD_NUMSELUD           BW(164)             /* numselbox up/down figures */
#define TD_TEXTERR            RGB(255, 61,  61)   /* widget face text in error */
#define TD_PROGINACEN         BW(222)             /* progress bar inactive center */
#define TD_PROGINAEDG         BW(196)             /* progress bar inactive edge */
#define TD_PROGACTCEN         RGB(146, 77,  139)  /* progress bar active center */
#define TD_PROGACTEDG         RGB(129, 68,  123)  /* progress bar active edge */
#define TD_LSTHOV             BW(230)             /* list background for hover */
#define TD_OUTLINE2           BW(206)             /* numselbox, dropbox outline */
#define TD_DROPARROW          BW(61)              /* dropbox arrow */
#define TD_DROPTEXT           BW(0)               /* dropbox text */
#define TD_SLDINT             BW(233)             /* slider track internal */
#define TD_TABDIS             BW(154)             /* tab unselected text */
#define TD_TABBACK            BW(247)             /* tab background */
#define TD_TABSEL             RGB(233, 84, 32)    /* tab selected underbar */
#define TD_TABFOCUS           (TD_TABSEL+BW(20))  /* tab focus box */
#define TD_CANCELBACKFOCUS    BW(72)              /* Cancel background in focus */
#define TD_CANCELTEXTFOCUS    BW(247)             /* Cancel text in focus */
#define TD_CANCELOUTLINE      BW(42)              /* Cancel outline unfocused */
#define TD_SELECTBACKFOCUS    RGB(17,164,40)      /* Select background in focus */
#define TD_SELECTBACK         RGB(16,155,38)      /* Select background in normal */
#define TD_SELECTTEXTFOCUS    BW(255)             /* Select text in focus */
#define TD_SELECTTEXT         BW(220)             /* Select text in focus */
#define TD_SELECTOUTLINE      RGB(16,155,38)      /* Select outline unfocused */
#define TD_SELECTOUTLINEFOCUS RGB(16,155,38)      /* Select outline focused */
#define TD_PLUSBACKFOCUS      RGB(252,252,252)    /* Select background in focus */
#define TD_PLUSBACK           RGB(252,252,252)    /* Select background in normal */
#define TD_PLUSTEXTFOCUS      BW(61)              /* Select text in focus */
#define TD_PLUSTEXT           BW(61)              /* Select text in focus */
#define TD_PLUSOUTLINE        BW(196)             /* Select outline unfocused */
#define TD_PLUSOUTLINEFOCUS   BW(196)             /* Select outline focused */
#define TD_TITLE              BW(48)              /* GTK dialog titlebar color */

/* colors in the querycolor select grid */
#define TD_QUERYCOLOR1        RGB(239,41,41)
#define TD_QUERYCOLOR2        RGB(252,175,62)
#define TD_QUERYCOLOR3        RGB(252,233,79)
#define TD_QUERYCOLOR4        RGB(138,226,52)
#define TD_QUERYCOLOR5        RGB(114,159,207)
#define TD_QUERYCOLOR6        RGB(173,127,168)
#define TD_QUERYCOLOR7        RGB(233,185,110)
#define TD_QUERYCOLOR8        RGB(136,138,133)
#define TD_QUERYCOLOR9        RGB(238,238,236)
#define TD_QUERYCOLOR10       RGB(204,0,0)
#define TD_QUERYCOLOR11       RGB(245,121,0)
#define TD_QUERYCOLOR12       RGB(237,212,0)
#define TD_QUERYCOLOR13       RGB(115,210,22)
#define TD_QUERYCOLOR14       RGB(52,101,164)
#define TD_QUERYCOLOR15       RGB(117,80,123)
#define TD_QUERYCOLOR16       RGB(193,125,17)
#define TD_QUERYCOLOR17       RGB(85,87,83)
#define TD_QUERYCOLOR18       RGB(211,215,207)
#define TD_QUERYCOLOR19       RGB(164,0,0)
#define TD_QUERYCOLOR20       RGB(206,92,0)
#define TD_QUERYCOLOR21       RGB(196,160,0)
#define TD_QUERYCOLOR22       RGB(78,154,6)
#define TD_QUERYCOLOR23       RGB(32,74,135)
#define TD_QUERYCOLOR24       RGB(92,53,102)
#define TD_QUERYCOLOR25       RGB(143,89,2)
#define TD_QUERYCOLOR26       RGB(46,52,54)
#define TD_QUERYCOLOR27       RGB(186,189,182)
#define TD_QUERYCOLOR28       RGB(0,0,0)
#define TD_QUERYCOLOR29       RGB(46,52,54)
#define TD_QUERYCOLOR30       RGB(85,87,83)
#define TD_QUERYCOLOR31       RGB(136,138,133)
#define TD_QUERYCOLOR32       RGB(186,189,182)
#define TD_QUERYCOLOR33       RGB(211,215,207)
#define TD_QUERYCOLOR34       RGB(238,238,236)
#define TD_QUERYCOLOR35       RGB(243,243,243)
#define TD_QUERYCOLOR36       RGB(255,255,255)

/* find given percentage of N */
#define PERCENT(n, p) (n*p/100)

/* find RGB value as percentage */
#define PERRGB(rgb, p) (PERCENT(REDP(rgb), p)<<16 | PERCENT(GREENP(rgb), p)<<8 | \
        PERCENT(BLUEP(rgb), p))

/* types of system vectors for override calls */

typedef int (*pclose_t)(int);

/* system override calls */

extern void ovr_close(pclose_t nfp, pclose_t* ofp);

#ifdef NOCANCEL
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
#endif

/* values table ids */

typedef enum {

    th_backpressed,        /* button background when pressed */
    th_back,               /* button background when not pressed */
    th_outline1,           /* button outline */
    th_text,               /* button face text enabled */
    th_textdis,            /* button face text disabled */
    th_focus,              /* button focused outline */
    th_chkrad,             /* checkbox/radio button selected */
    th_chkradout,          /* checkbox/radio button outline */
    th_scrollback,         /* scrollbar background */
    th_scrollbar,          /* scrollbar not pressed */
    th_scrollbarpressed,   /* scrollbar pressed */
    th_numseldiv,          /* numselbox divider */
    th_numselud,           /* numselbox up/down figures */
    th_texterr,            /* widget face text in error */
    th_proginacen,         /* progress bar inactive center */
    th_proginaedg,         /* progress bar inactive edge */
    th_progactcen,         /* progress bar active center */
    th_progactedg,         /* progress bar active edge */
    th_lsthov,             /* list background for hover */
    th_outline2,           /* numselbox, dropbox outline */
    th_droparrow,          /* dropbox arrow */
    th_droptext,           /* dropbox text */
    th_sldint,             /* slider track internal */
    th_tabdis,             /* tab unselected text */
    th_tabback,            /* tab background */
    th_tabsel,             /* tab selected underbar */
    th_tabfocus,           /* tab focus box */
    th_cancelbackfocus,    /* Cancel background in focus */
    th_canceltextfocus,    /* Cancel text in focus */
    th_canceloutline,      /* Cancel outline unfocused */
    th_selectbackfocus,    /* Select background in focus */
    th_selectback,         /* Select background in normal */
    th_selecttextfocus,    /* Select text in focus */
    th_selecttext,         /* Select text in focus */
    th_selectoutline,      /* Select outline unfocused */
    th_selectoutlinefocus, /* Select outline focused */
    th_plusbackfocus,      /* Select background in focus */
    th_plusback,           /* Select background in normal */
    th_plustextfocus,      /* Select text in focus */
    th_plustext,           /* Select text in focus */
    th_plusoutline,        /* Select outline unfocused */
    th_plusoutlinefocus,   /* Select outline focused */
    th_title,              /* GTK dialog titlebar color */
    /* colors in color chooser grid */
    th_querycolor1,
    th_querycolor2,
    th_querycolor3,
    th_querycolor4,
    th_querycolor5,
    th_querycolor6,
    th_querycolor7,
    th_querycolor8,
    th_querycolor9,
    th_querycolor10,
    th_querycolor11,
    th_querycolor12,
    th_querycolor13,
    th_querycolor14,
    th_querycolor15,
    th_querycolor16,
    th_querycolor17,
    th_querycolor18,
    th_querycolor19,
    th_querycolor20,
    th_querycolor21,
    th_querycolor22,
    th_querycolor23,
    th_querycolor24,
    th_querycolor25,
    th_querycolor26,
    th_querycolor27,
    th_querycolor28,
    th_querycolor29,
    th_querycolor30,
    th_querycolor31,
    th_querycolor32,
    th_querycolor33,
    th_querycolor34,
    th_querycolor35,
    th_querycolor36,
    th_endmarker           /* end of theme entries */

} themeindex;

/* widget type */
typedef enum  {

    wtcbutton, wtbutton, wtcheckbox, wtradiobutton, wtgroup, wtbackground,
    wtscrollvert, wtscrollhoriz, wtnumselbox, wteditbox,
    wtprogbar, wtlistbox, wtdropbox, wtdropeditbox,
    wtslidehoriz, wtslidevert, wttabbar

} wigtyp;

/* custom button color structure */
typedef struct ccolor* ccolorp;
typedef struct ccolor {

    /** Button background normal */           unsigned long bbn;
    /** Button background pressed */          unsigned long bbp;
    /** Button outline normal */              unsigned long bon;
    /** Button outline focus */               unsigned long bof;
    /** Button text normal */                 unsigned long btn;
    /** Button text disabled */               unsigned long btd;

} ccolor;

/* Widget control structure */
typedef struct wigrec* wigptr;
typedef struct wigrec {

    /** next entry in list */                 wigptr    next;
    /** type of widget */                     wigtyp    typ;
    /** in the pressed state */               int       pressed;
    /** last pressed state */                 int       lpressed;
    /** the current on/off state */           int       select;
    /** output file for the widget window */  FILE*     wf;
    /** face text */                          char*     face;
    /** parent window */                      FILE*     parent;
    /** id number */                          int       id;
    /** widget window id */                   int       wid;
    /** widget is enabled */                  int       enb;
    /** scrollbar size in MAXINT ratio */     int       sclsiz;
    /** scrollbar position in MAXINT ratio */ int       sclpos;
    /** mouse tracking in widget */           int       mpx, mpy;
    /** last mouse position */                int       lmpx, lmpy;
    /** text cursor */                        int       curs;
    /** text left side index */               int       tleft;
    /** focused */                            int       focus;
    /** hovered */                            int       hover;
    /** insert/overwrite mode */              int       ins;
    /** allow only numeric entry */           int       num;
    /** low bound of number */                int       lbnd;
    /** upper bound of number */              int       ubnd;
    /** child/subclassed widget */            wigptr    cw;
    /** child/subclassed widget 2 */          wigptr    cw2;
    /** parent widget */                      wigptr    pw;
    /** parent file (used to send subclass
       messages) */                           FILE*     pf;
    /** up button pressed */                  int       uppress;
    /** down buton pressed */                 int       downpress;
    /** progress bar position */              int       ppos;
    /** string list */                        ami_strptr strlst;
    /** string selected, 0 if none */         int       ss;
    /** string hovered, 0 if none */          int       sh;
    /** position of widget in parent */       int       px, py;
    /** child window id */                    int       cid;
    /** mouse grabs scrollbar/slider */       int       grab;
    /** tick marks on slider */               int       ticks;
    /** Tab orientation */                    ami_tabori tor;
    /** Character based */                    int       charb;

    /** Configurable button fields */         ccolorp   cbc;
    /** use check/text */                     int       check;

} wigrec;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pclose_t  ofpclose;
static pclose_t  ofpclose_nocancel;

/*
 * Saved vectors for entry calls for widgets.
 */
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

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec* filptr;
typedef struct filrec {

    /* table of widgets in window, includes negatives and 0 */
    wigptr widgets[MAXWIG*2+1];

} filrec;

static ami_pevthan    widget_event_old;   /* previous event vector save */
static wigptr        wigfre;             /* free widget entry list */
static filptr        opnfil[MAXFIL];     /* open files table */
static wigptr        xltwig[MAXFIL*2+1]; /* widget entry equivalence table */
static FILE*         win0;               /* "window zero" dummy window */
/* table of colors or other theme values */
static unsigned long themetable[th_endmarker];

/** ****************************************************************************

Process error

*******************************************************************************/

static void error(
    /** Error string */ char* es
)


{

#ifdef USEDLG
    ami_alert("Error: widgets", es);
#else
    fprintf(stderr, "Error: widgets: %s\n", es);
    fflush(stderr);
#endif

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

/** ****************************************************************************

Place string in storage

Places the given string into dynamic storage, and returns that.

\returns Pointer to string copy in storage.

*******************************************************************************/

static char* str(
    /** String to place in storage */ char* s
)

{

    char* p;

    p = malloc(strlen(s)+1);
    strcpy(p, s);

    return (p);

}

/** ****************************************************************************

Copy string list

Makes a copy of a string list

*******************************************************************************/

static void cpystrlst(
    /** Destination string list */ ami_strptr* dp,
    /** Source string list */      ami_strptr  sp
)

{

    ami_strptr sp1;
    ami_strptr lh, lhs, p;

    /* make a copy of the list */
    lh = NULL;
    while (sp) { /* traverse the list */

        sp1 = malloc(sizeof(ami_strrec)); /* get string entry */
        sp1->str = str(sp->str); /* copy the string */
        sp1->next = lh; /* push to list */
        lh = sp1;
        sp = sp->next; /* next entry */

    }
    /* reverse the list */
    lhs = lh;
    lh = NULL;
    while (lhs) {

        p = lhs; /* pick top entry */
        lhs = lhs->next; /* gap out */
        p->next = lh; /* push to new list */
        lh = p;

    }

    *dp = lh; /* return the copied list */

}

/** ****************************************************************************

Dispose string list

Recycles a string list

*******************************************************************************/

static void frestrlst(
    /** String list */ ami_strptr sp
)

{

    ami_strptr sp1;

    while (sp) { /* list not empty */

        sp1 = sp; /* index top */
        sp = sp->next; /* gap */
        free(sp1->str); /* free string */
        free(sp1); /* free entry */

    }

}

/** ****************************************************************************

Get file entry

Allocates and initializes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

\returns Pointer to new file entry.

*******************************************************************************/

static filptr getfil(void)

{

    filptr fp;
    int    i;

    fp = malloc(sizeof(filrec)); /* get new file entry */
    /* clear widget table */
    for (i = 0; i < MAXWIG*2+1; i++) fp->widgets[i] = NULL;

    return (fp); /* exit with file entry */

}

/** ****************************************************************************

Make file entry

If the indicated file does not contain a file control structure, one is created.
Otherwise it is a no-op.

*******************************************************************************/

static void makfil(
    /** File entry pointer */ FILE* f
)

{

    int fn;

    if (!f) error("Invalid window file");
    fn = fileno(f); /* get the file logical number */
    if (fn > MAXFIL) error("Invalid file number");
    /* check table empty */
    if (!opnfil[fn]) opnfil[fn] = getfil(); /* allocate file entry */

}

/** ****************************************************************************

Get widget

Get a widget and place into the window tracking list. If a free widget entry
is available, that will be used, otherwise a new entry is allocated.

\returns Pointer to new widget.

*******************************************************************************/

static wigptr getwig(void)

{

    wigptr wp; /* widget pointer */

    if (wigfre) { /* used entry exists, get that */

        wp = wigfre; /* index top entry */
        wigfre = wigfre->next; /* gap out */

    } else wp = malloc(sizeof(wigrec)); /* get entry */
    wp->pressed = FALSE; /* set not pressed */
    wp->lpressed = FALSE;
    wp->select = FALSE; /* set not selected */
    wp->enb = FALSE; /* set not enabled */
    wp->sclpos = 0; /* set scrollbar position top/left */
    wp->curs = 0; /* set text cursor */
    wp->tleft = 0; /* set text left side in edit box */
    wp->focus = 0; /* set not focused */
    wp->hover = 0; /* set no hover */
    wp->ins = 0; /* set insert mode */
    wp->mpx = 0; /* clear mouse position */
    wp->mpy = 0;
    wp->lmpx = 0;
    wp->lmpy = 0;
    wp->num = FALSE; /* set any character entry */
    wp->lbnd = -INT_MAX; /* set low bound */
    wp->ubnd = INT_MAX; /* set high bound */
    wp->cw = NULL; /* clear children */
    wp->cw2 = NULL;
    wp->pw = NULL; /* clear parent */
    wp->uppress = FALSE; /* set up not pressed */
    wp->downpress = FALSE; /* set down not pressed */
    wp->ppos = 0; /* progress bar extreme left */
    wp->strlst = NULL; /* clear string list */
    wp->ss = 0; /* no string selected */
    wp->sh = 0; /* no string hovered */
    wp->px = 0; /* clear origin in parent */
    wp->py = 0;
    wp->cid = 0; /* clear child id */
    wp->grab = FALSE; /* set no scrollbar/slider grab */
    wp->ticks = 0; /* set no tick marks on slider */
    wp->tor = ami_totop; /* set tab orientation top */
    wp->charb = FALSE; /* widget based on character grid */
    wp->check = FALSE; /* do not use check instead of text */

    return wp; /* return entry */

}

/** ****************************************************************************

Put widget

Removes the widget from the window list, and releases the widget entry to free
list.

*******************************************************************************/

static void putwig(
    /** Pointer to wiget to free */ wigptr wp)

{

    /* if not a subclass widget, free string list */
    if (!wp->pw) frestrlst(wp->strlst);
    if (wp->face) free(wp->face); /* free face string if exists */
    wp->next = wigfre; /* push to free list */
    wigfre = wp;

}

/** ****************************************************************************

Find widget

Given a file specification and a widget id, returns a pointer to the given
widget. Validates the file and the widget number.

\returns Pointer to found widget.

*******************************************************************************/

static wigptr fndwig(
    /** Window file pointer */ FILE* f,
    /** Logical wiget id */    int id
)

{

    int       fn;  /* logical file name */
    wigptr    wp;  /* widget entry pointer */

    if (id <= -MAXWIG || id > MAXWIG || !id) error("Invalid widget id");
    fn = fileno(f); /* get the file index */
    if (fn < 0 || fn > MAXFIL) error("Invalid file number");
    if (!opnfil[fn]->widgets[id+MAXWIG]) error("No widget by given id");
    wp = opnfil[fn]->widgets[id+MAXWIG]; /* index that */

    return (wp); /* return the widget pointer */

}

/** ****************************************************************************

Send redraw to widget

Sends a redraw request to the given widget. The common workflow with widgets
is to reconfigure it by changing the parameters of it, then sending it a redraw
to update itself with the new parameters.

*******************************************************************************/

static void widget_redraw(
    /** Widget data block pointer */ wigptr wp
)

{

    ami_evtrec ev;  /* outbound menu event */

    ev.etype = ami_etredraw; /* set redraw event */
    ev.rsx = 1; /* set extent */
    ev.rsy = 1;
    ev.rex = ami_maxxg(wp->wf);
    ev.rey = ami_maxyg(wp->wf);
    ami_sendevent(wp->wf, &ev); /* send to widget window */

}

/** ****************************************************************************

Draw foreground color from packed 32 bit color

Takes a file and a 32 bit packed RGB color, and sets the foreground color.

*******************************************************************************/

static void fcolorp(
    /** Window file pointer */ FILE*         f,
    /** 32 bit packed color */ unsigned long c
)

{

    ami_fcolorg(f, RED(c), GREEN(c), BLUE(c));

}

/** ****************************************************************************

Draw background color from packed 32 bit color

Takes a file and a 32 bit packed RGB color, and sets the background color.
table.

*******************************************************************************/

static void bcolorp(
    /** Window file pointer */ FILE*         f,
    /** 32 bit packed color */ unsigned long c
)

{

    ami_bcolorg(f, RED(c), GREEN(c), BLUE(c));

}

/** ****************************************************************************

Draw foreground color from theme table

Takes a file and a theme index, and sets the foreground color from the theme
table.

*******************************************************************************/

static void fcolort(
    /** Window file pointer */ FILE*      f,
    /** Theme color index */   themeindex t
)

{

    fcolorp(f, themetable[t]);

}

/** ****************************************************************************

Draw background color from theme table

Takes a file and a theme index, and sets the background color from the theme
table.

*******************************************************************************/

static void bcolort(
    /** Window file pointer */ FILE*      f,
    /** Theme color index */   themeindex t
)

{

    bcolorp(f, themetable[t]);

}

/** ****************************************************************************

Find number of digits in value

Finds the number of digits required to represent a decimal value. Does not
consider the sign.

*******************************************************************************/

static int digits(
    /** Value to measure */ int v
)

{

    int p; /* power */
    int c; /* count */

    p = 1; /* set first power */
    c = 1; /* set initial count (at least one digit) */
    while (p < INT_MAX/10 && p < v) { /* will not overflow */

        p *= 10; /* advance power */
        c++; /* count digits */

    }

    return (c); /* return digits */

}

/** ****************************************************************************

Kill widget

Kills the given widget by id and in the window file by file id.

*******************************************************************************/

static void intkillwidget(
    /** file id */           int fn,
    /** Logical widget id */ int wid
)

{

    wigptr wp; /* widget entry pointer */

    if (fn < 0 || fn > MAXFIL) error("Invalid file number");
    if (!opnfil[fn]) error("File by id not open");
    if (wid <= -MAXWIG || wid > MAXWIG || !wid) error("Invalid widget id");
    if (!opnfil[fn]->widgets[wid+MAXWIG]) error("No widget by given id");
    wp = opnfil[fn]->widgets[wid+MAXWIG]; /* index that */
    /* if there is a subwidget, kill that as well */
    if (wp->cw) ami_killwidget(wp->cw->pw->wf, wp->cw->id);
    if (wp->cw2) ami_killwidget(wp->cw2->pw->wf, wp->cw2->id);
    fclose(wp->wf); /* close the window file */
    opnfil[fn]->widgets[wid+MAXWIG] = NULL; /* clear widget slot  */
    putwig(wp); /* release widget data */

}

/*******************************************************************************

Create widget

Creates a widget within the given window, within the specified bounding box,
and using the face string and type, and the given id. The string may or may not
be used.

A predefined widget entry can be passed in. This allows subclassing widgets. The
subclasser uses the pass-in to set parameters to control the subclassing. If the
pass-in is NULL, then a new entry will be created. This, or the predefined entry
will be passed back to the user.

*******************************************************************************/

static void widget(
    /** Parent window file */              FILE* f,
    /** Containing rectangle for widget */ int x1, int y1, int x2, int y2,
    /** Face string (if exists) */         char* s,
    /** logical id for widget */           int id,
    /** type code for widget */            wigtyp typ,
    /** Widget I/O pointer */              wigptr* wpr
)

{

    int fn; /* logical file name */
    wigptr wp;

    if (id <= -MAXWIG || id > MAXWIG || !id) error("Invalid widget id");
    makfil(f); /* ensure there is a file entry and validate */
    fn = fileno(f); /* get the file index */
    wp = *wpr; /* get any predefined widget entry */
    if (!wp) wp = getwig(); /* get widget entry if none passed in */
    if (opnfil[fn]->widgets[id+MAXWIG]) error("Widget by id already in use");
    opnfil[fn]->widgets[id+MAXWIG] = wp; /* set widget entry */

    wp->face = str(s); /* place face */
    wp->wid = ami_getwinid(); /* allocate a buried wid */
    ami_openwin(&stdin, &wp->wf, f, wp->wid); /* open widget window */
    wp->parent = f; /* save parent file */
    xltwig[wp->wid+MAXFIL] = wp; /* set the tracking entry for the window */
    wp->id = id; /* set button widget id */
    ami_buffer(wp->wf, FALSE); /* turn off buffering */
    ami_auto(wp->wf, FALSE); /* turn off auto */
    ami_curvis(wp->wf, FALSE); /* turn off cursor */
    ami_font(wp->wf, AMI_FONT_SIGN); /* set sign font */
    ami_frame(wp->wf, FALSE); /* turn off frame */
    ami_setposg(wp->wf, x1, y1); /* place at position */
    ami_setsizg(wp->wf, x2-x1, y2-y1); /* set size */
    ami_binvis(wp->wf); /* no background write */
    wp->typ = typ; /* place type */
    wp->enb = TRUE; /* set is enabled */
    wp->sclsiz = INT_MAX/10; /* set default size scrollbar */
    wp->px = x1; /* set widget position in parent */
    wp->py = y1;

    *wpr = wp; /* copy back to caller */

}

/** ****************************************************************************

Customizable button draw handler

Handles drawing customizable buttons. Customizable buttons are designed to be
subclassed only, and have their parameters set by the widget record they
receive.

*******************************************************************************/

static void cbutton_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int sq; /* size of checkbox square */
    int sqm; /* center x of checkbox square */
    int md; /* checkbox center line */
    int cb; /* bounding box of check figure */

    /* color the background */
    if (wg->pressed) fcolorp(wg->wf, wg->cbc->bbp);
    else fcolorp(wg->wf, wg->cbc->bbn);
    ami_frrect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf), 20, 20);
    /* outline */
    ami_linewidth(wg->wf, 3);
    if (wg->focus) fcolorp(wg->wf, wg->cbc->bof);
    else fcolorp(wg->wf, wg->cbc->bon);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 20, 20);
    if (wg->enb) fcolorp(wg->wf, wg->cbc->btn);
    else fcolorp(wg->wf, wg->cbc->btd);
    if (wg->select && wg->check) { /* use check instead of text */

        /* set size of square as ratio of font height */
        sq = 0.80*ami_chrsizy(wg->wf);
        md = ami_maxyg(wg->wf)/2; /* set middle line of checkbox */
        sqm = ami_maxxg(wg->wf)/2; /* set square middle x */
        cb = sq*.70; /* set bounding box of check figure */
        /* place selected checkmark */
        ami_linewidth(wg->wf, 4);
        ami_line(wg->wf, sqm-cb/2, md-cb*.10,
                        sqm, md+cb*.25);
        ami_line(wg->wf, sqm-1, md+cb*.25-1,
                        sqm+cb/2, md-cb*.4);
        ami_linewidth(wg->wf, 1);

    } else { /* use text */

        ami_cursorg(wg->wf,
                   ami_maxxg(wg->wf)/2-ami_strsiz(wg->wf, wg->face)/2,
                   ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2);
        fprintf(wg->wf, "%s", wg->face); /* place button face */

    }

}

/** ****************************************************************************

Button event handler

Handles the events posted to buttons.

*******************************************************************************/

static void cbutton_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    ami_evtrec er; /* outbound button event */

    if (ev->etype == ami_etredraw) cbutton_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        if (wg->enb) { /* enabled */

            /* send event back to parent window */
            er.etype = ami_etbutton; /* set button event */
            er.butid = wg->id; /* set id */
            ami_sendevent(wg->parent, &er); /* send the event to the parent */

        }

        /* process button press */
        wg->pressed = TRUE;
        cbutton_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etmoubd  && ev->dmoubn == 1) {

        wg->pressed = FALSE;
        cbutton_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etfocus) {

        wg->focus = 1; /* in focus */
        cbutton_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnofocus) {

        wg->focus = 0; /* out of focus */
        cbutton_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_ethover) {

        wg->hover = 1; /* hovered */
        cbutton_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnohover) {

        wg->hover = 0; /* not hovered */
        cbutton_draw(wg); /* redraw the window */

    }

}

/** ****************************************************************************

Button draw handler

Handles drawing buttons.

*******************************************************************************/

static void button_draw(
    /** Widget data pointer */ wigptr wg
)

{

    /* color the background */
    if (wg->pressed) fcolort(wg->wf, th_backpressed);
    else fcolort(wg->wf, th_back);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf),
              ami_maxyg(wg->wf));
    /* outline */
    ami_linewidth(wg->wf, 4);
    if (wg->focus) fcolort(wg->wf, th_focus);
    else fcolort(wg->wf, th_outline1);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1,
             ami_maxyg(wg->wf)-1, 20, 20);
    if (wg->enb) fcolort(wg->wf, th_text);
    else fcolort(wg->wf, th_textdis);
    ami_cursorg(wg->wf,
               ami_maxxg(wg->wf)/2-ami_strsiz(wg->wf, wg->face)/2,
               ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2);
    fprintf(wg->wf, "%s", wg->face); /* place button face */

}

/** ****************************************************************************

Button event handler

Handles the events posted to buttons.

*******************************************************************************/

static void button_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    ami_evtrec er; /* outbound button event */

    if (ev->etype == ami_etredraw) button_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        if (wg->enb) { /* enabled */

            /* send event back to parent window */
            er.etype = ami_etbutton; /* set button event */
            er.butid = wg->id; /* set id */
            ami_sendevent(wg->parent, &er); /* send the event to the parent */

        }

        /* process button press */
        wg->pressed = TRUE;
        button_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etmoubd  && ev->dmoubn == 1) {

        wg->pressed = FALSE;
        button_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etfocus) {

        wg->focus = 1; /* in focus */
        button_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnofocus) {

        wg->focus = 0; /* out of focus */
        button_draw(wg); /* redraw the window */

    }

}

/** ****************************************************************************

Checkbox draw handler

Handles drawing checkboxes.

*******************************************************************************/

static void checkbox_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int sq; /* size of checkbox square */
    int sqm; /* center x of checkbox square */
    int sqo; /* checkbox offset left */
    int md; /* checkbox center line */
    int cb; /* bounding box of check figure */

    /* color the background */
    ami_fcolor(wg->wf, ami_backcolor);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* outline */
    ami_linewidth(wg->wf, 4);
    if (wg->focus) {

        fcolort(wg->wf, th_focus);
        ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1,
                 ami_maxyg(wg->wf)-1, 20, 20);

    }
    /* draw text */
    if (wg->enb) fcolort(wg->wf, th_text);
    else fcolort(wg->wf, th_textdis);
    ami_cursorg(wg->wf, ami_chrsizy(wg->wf)+ami_chrsizy(wg->wf)/2,
                       ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2);
    fprintf(wg->wf, "%s", wg->face); /* place button face */
    /* set size of square as ratio of font height */
    sq = 0.80*ami_chrsizy(wg->wf);
    md = ami_maxyg(wg->wf)/2; /* set middle line of checkbox */
    sqo = ami_maxyg(wg->wf)/4; /* set offset of square from left */
    sqm = sqo+sq/2; /* set square middle x */
    cb = sq*.70; /* set bounding box of check figure */

    if (wg->select) {

        /* place selected checkmark */
        fcolort(wg->wf, th_chkrad);
        ami_frrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 10, 10);
        ami_fcolor(wg->wf, ami_white);
        ami_linewidth(wg->wf, 4);
        ami_line(wg->wf, sqm-cb/2, md-cb*.10,
                        sqm, md+cb*.25);
        ami_line(wg->wf, sqm-1, md+cb*.25-1,
                        sqm+cb/2, md-cb*.4);
        ami_linewidth(wg->wf, 1);

    } else {

        /* place non-selected checkmark background */
        ami_fcolor(wg->wf, ami_white);
        ami_frrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 10, 10);
        ami_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_chkradout);
        ami_rrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 10, 10);

    }

}

/** ****************************************************************************

Checkbox event handler

Handles the events posted to checkboxes.

*******************************************************************************/

static void checkbox_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    ami_evtrec er; /* outbound checkbox event */

    if (ev->etype == ami_etredraw) checkbox_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        if (wg->enb) { /* enabled */

            /* send event back to parent window */
            er.etype = ami_etchkbox; /* set checkbox event */
            er.butid = wg->id; /* set id */
            ami_sendevent(wg->parent, &er); /* send the event to the parent */

        }
        checkbox_draw(wg);

    } else if (ev->etype == ami_etmoubd && ev->amoubn == 1) checkbox_draw(wg);
    else if (ev->etype == ami_etfocus) {

        wg->focus = 1; /* in focus */
        checkbox_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnofocus) {

        wg->focus = 0; /* out of focus */
        checkbox_draw(wg); /* redraw the window */

    }

}

/** ****************************************************************************

Radio button event handler

Handles the events posted to radiobuttons.

*******************************************************************************/

static void radiobutton_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int cr; /* size of radiobutton circle */
    int crm; /* center x of radiobutton circle */
    int cro; /* radiobutton offset left */
    int md; /* radiobutton center line */

    /* color the background */
    ami_fcolor(wg->wf, ami_backcolor);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* outline */
    ami_linewidth(wg->wf, 4);
    if (wg->focus) {

        fcolort(wg->wf, th_focus);
        ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1,
                 ami_maxyg(wg->wf)-1, 20, 20);

    }
    /* draw text */
    if (wg->enb) fcolort(wg->wf, th_text);
    else fcolort(wg->wf, th_textdis);
    ami_cursorg(wg->wf, ami_chrsizy(wg->wf)+ami_chrsizy(wg->wf)/2,
                       ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2);
    fprintf(wg->wf, "%s", wg->face); /* place button face */
    /* set size of circle as ratio of font height */
    cr = 0.80*ami_chrsizy(wg->wf);
    md = ami_maxyg(wg->wf)/2; /* set middle line of radiobutton */
    cro = ami_maxyg(wg->wf)/4; /* set offset of circle from left */
    crm = cro+cr/2; /* set circle middle x */

    if (wg->select) {

        /* place select figure */
        fcolort(wg->wf, th_chkrad);
        ami_fellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);
        ami_fcolor(wg->wf, ami_white);
        ami_fellipse(wg->wf, crm-cr/6, md-cr/6, crm+cr/6, md+cr/6);

    } else {

        /* place non-selected background */
        ami_fcolor(wg->wf, ami_white);
        ami_fellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);
        ami_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_chkradout);
        ami_ellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);

    }

}

static void radiobutton_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    ami_evtrec er; /* outbound radiobutton event */

    if (ev->etype == ami_etredraw) radiobutton_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        if (wg->enb) { /* enabled */

            /* send event back to parent window */
            er.etype = ami_etradbut; /* set button event */
            er.butid = wg->id; /* set id */
            ami_sendevent(wg->parent, &er); /* send the event to the parent */

        }
        radiobutton_draw(wg);

    } else if (ev->etype == ami_etmoubd) radiobutton_draw(wg);
    else if (ev->etype == ami_etfocus) {

        wg->focus = 1; /* in focus */
        radiobutton_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnofocus) {

        wg->focus = 0; /* out of focus */
        radiobutton_draw(wg); /* redraw the window */

    }
}

/** ****************************************************************************

Vertical scrollbar draw handler

Handles draws vertical scrollbars.

*******************************************************************************/

static void scrollvert_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int       sclsizp; /* size of slider in pixels */
    int       sclposp; /* offset of slider in pixels */
    int       remsizp; /* remaining space after slider in pixels */
    int       totsizp; /* total size of slider space after padding */
    int       botposp; /* bottom position of slider */
    int       inbar;   /* mouse is in scroll bar */
    int       sclpos;  /* new scrollbar position */
    ami_evtrec er;      /* outbound button event */
    int       y;

    /* find net total slider space */
    totsizp = ami_maxyg(wg->wf)-ENDSPACE-ENDSPACE;
    /* find size of slider in pixels */
    sclsizp = round((double)totsizp*wg->sclsiz/INT_MAX);
    /* find remaining size after slider */
    remsizp = totsizp-sclsizp;
    /* find position of top of slider in pixels offset */
    sclposp = round((double)remsizp*wg->sclpos/INT_MAX);
    /* find bottom of slider in pixels offset */
    botposp = sclposp+sclsizp-1;
    /* set status of mouse inside the bar */
    inbar = wg->mpy >= sclposp+ENDSPACE && wg->mpy <= botposp+ENDSPACE;

    /* check drag */
    if ((inbar || wg->grab) && wg->pressed && wg->lpressed && wg->mpy != wg->lmpy) {

        /* mouse bar drag, process */
        y = sclposp+(wg->mpy-wg->lmpy); /* find difference in pixel location */
        if (y < 0) y = 0; /* limit to zero */
        if (y > remsizp) y = remsizp; /* limit to max */
        if (y) { /* not a null move */

            /* find new ratioed position */
            sclpos = round((double)INT_MAX*y/remsizp);
            /* send event back to parent window */
            er.etype = ami_etsclpos; /* set scroll position event */
            er.sclpid = wg->id; /* set id */
            er.sclpos = sclpos; /* set scrollbar position */
            ami_sendevent(wg->parent, &er); /* send the event to the parent */
            wg->grab = TRUE; /* set we grabbed the scrollbar */

        }

    } else if (!wg->pressed) wg->grab = FALSE;

    /* color the background */
    fcolort(wg->wf, th_scrollback);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    if (wg->pressed && (inbar || wg->grab))
        /* color as pressed */
        fcolort(wg->wf, th_scrollbarpressed);
    else
        /* color as not pressed */
        fcolort(wg->wf, th_scrollbar);
    ami_frrect(wg->wf, ENDSPACE, ENDSPACE+sclposp, ami_maxxg(wg->wf)-ENDSPACE, ENDSPACE+sclposp+sclsizp,
              10, 10);

}

/** ****************************************************************************

Vertical scrollbar event handler

Handles the events posted to vertical scrollbars.

*******************************************************************************/

static void scrollvert_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    int       sclpos;  /* new scrollbar position */
    int       sclsizp; /* size of slider in pixels */
    int       remsizp; /* remaining space after slider in pixels */
    int       totsizp; /* total size of slider space after padding */
    ami_evtrec er;      /* outbound button event */
    int       y;

    if (ev->etype == ami_etredraw) scrollvert_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        /* find net total slider space */
        totsizp = ami_maxyg(wg->wf)-ENDSPACE-ENDSPACE;
        /* find size of slider in pixels */
        sclsizp = round((double)totsizp*wg->sclsiz/INT_MAX);
        /* find remaining size after slider */
        remsizp = totsizp-sclsizp;
        /* find new top for click */
        y = wg->mpy-sclsizp/2;
        if (y < ENDSPACE) y = ENDSPACE; /* limit top travel */
        else if (y+sclsizp > ami_maxyg(wg->wf)-ENDSPACE)
            y = ami_maxyg(wg->wf)-sclsizp-ENDSPACE;
        /* find new ratioed position */
        sclpos = round((double)INT_MAX*(y-ENDSPACE)/remsizp);
        /* send event back to parent window */
        er.etype = ami_etsclpos; /* set scroll position event */
        er.sclpid = wg->id; /* set id */
        er.sclpos = sclpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */
        scrollvert_draw(wg);

    } else if (ev->etype == ami_etmoubd) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = FALSE; /* set not pressed */
        scrollvert_draw(wg);

    } else if (ev->etype == ami_etmoumovg) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        /* mouse moved, track position */
        wg->lmpx = wg->mpx; /* move present to last */
        wg->lmpy = wg->mpy;
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;
        scrollvert_draw(wg);
        wg->lmpx = wg->mpx; /* now set equal to cancel move */
        wg->lmpy = wg->mpy;

    }

}

/** ****************************************************************************

Horizontal scrollbar draw handler

Handles drawing horizontal scrollbars.

*******************************************************************************/

static void scrollhoriz_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int       sclsizp; /* size of slider in pixels */
    int       sclposp; /* offset of slider in pixels */
    int       remsizp; /* remaining space after slider in pixels */
    int       totsizp; /* total size of slider space after padding */
    int       botposp; /* bottom position of slider */
    int       inbar;   /* mouse is in scroll bar */
    int       sclpos;  /* new scrollbar position */
    ami_evtrec er;      /* outbound event */
    int       x;

    /* find net total slider space */
    totsizp = ami_maxxg(wg->wf)-ENDSPACE-ENDSPACE;
    /* find size of slider in pixels */
    sclsizp = round((double)totsizp*wg->sclsiz/INT_MAX);
    /* find remaining size after slider */
    remsizp = totsizp-sclsizp;
    /* find position of top of slider in pixels offset */
    sclposp = round((double)remsizp*wg->sclpos/INT_MAX);
    /* find bottom of slider in pixels offset */
    botposp = sclposp+sclsizp-1;
    /* set status of mouse inside the bar */
    inbar = wg->mpx >= sclposp+ENDSPACE && wg->mpx <= botposp+ENDSPACE;

    /* check drag */
    if ((inbar || wg->grab) && wg->pressed && wg->lpressed &&
               wg->mpx != wg->lmpx) {

        /* mouse bar drag, process */
        x = sclposp+(wg->mpx-wg->lmpx); /* find difference in pixel location */
        if (x < 0) x = 0; /* limit to zero */
        if (x > remsizp) x = remsizp; /* limit to max */
        /* find new ratioed position */
        sclpos = round((double)INT_MAX*x/remsizp);
        /* send event back to parent window */
        er.etype = ami_etsclpos; /* set scroll position event */
        er.sclpid = wg->id; /* set id */
        er.sclpos = sclpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */
        wg->grab = TRUE; /* set we grabbed the scrollbar */

    } else if (!wg->pressed) wg->grab = FALSE;

    /* color the background */
    fcolort(wg->wf, th_scrollback);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    if (wg->pressed && (inbar || wg->grab))
        /* color as pressed */
        fcolort(wg->wf, th_scrollbarpressed);
    else
        /* color as not pressed */
        fcolort(wg->wf, th_scrollbar);
    ami_frrect(wg->wf, ENDSPACE+sclposp, ENDSPACE, ENDSPACE+sclposp+sclsizp, ami_maxyg(wg->wf)-ENDSPACE,
              10, 10);

}

/** ****************************************************************************

Horizontal scrollbar event handler

Handles the events posted to horizontal scrollbars.

*******************************************************************************/

static void scrollhoriz_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    int       sclpos;  /* new scrollbar position */
    int       sclsizp; /* size of slider in pixels */
    int       remsizp; /* remaining space after slider in pixels */
    int       totsizp; /* total size of slider space after padding */
    ami_evtrec er;      /* outbound button event */
    int       x;

    if (ev->etype == ami_etredraw) scrollhoriz_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        totsizp = ami_maxxg(wg->wf)-ENDSPACE-ENDSPACE; /* find net total slider space */
        /* find size of slider in pixels */
        sclsizp = round((double)totsizp*wg->sclsiz/INT_MAX);
        /* find remaining size after slider */
        remsizp = totsizp-sclsizp;
        /* find new top for click */
        x = wg->mpx-sclsizp/2;
        if (x < ENDSPACE) x = ENDSPACE; /* limit left travel */
        else if (x+sclsizp > ami_maxxg(wg->wf)-ENDSPACE)
            x = ami_maxxg(wg->wf)-sclsizp-ENDSPACE;
        /* find new ratioed position */
        sclpos = round((double)INT_MAX*(x-ENDSPACE)/remsizp);
        /* send event back to parent window */
        er.etype = ami_etsclpos; /* set scroll position event */
        er.sclpid = wg->id; /* set id */
        er.sclpos = sclpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */

        scrollhoriz_draw(wg);

    } else if (ev->etype == ami_etmoubd) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = FALSE; /* set not pressed */
        scrollhoriz_draw(wg);

    } else if (ev->etype == ami_etmoumovg) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        /* mouse moved, track position */
        wg->lmpx = wg->mpx; /* move present to last */
        wg->lmpy = wg->mpy;
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;
        scrollhoriz_draw(wg);
        wg->lmpx = wg->mpx; /* now set equal to cancel move */
        wg->lmpy = wg->mpy;

    }

}

/** ****************************************************************************

Group box draw handler

Handles drawing group boxes.

*******************************************************************************/

static void group_draw(
    /** Widget data pointer */ wigptr wg
)

{

    /* color the background */
    ami_fcolor(wg->wf, ami_backcolor);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    fcolort(wg->wf, th_outline1);
    ami_linewidth(wg->wf, 2);
    ami_rect(wg->wf, 2, ami_chrsizy(wg->wf)/2, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    ami_fcolor(wg->wf, ami_black);
    ami_cursorg(wg->wf, 1, 1);
    ami_bover(wg->wf);
    ami_bcolor(wg->wf, ami_backcolor);
    fprintf(wg->wf, "%s", wg->face); /* place button face */


}

/** ****************************************************************************

Group box event handler

Handles the events posted to group boxes.

*******************************************************************************/

static void group_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    if (ev->etype == ami_etredraw) group_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Background draw handler

Handles drawing backgrounds.

*******************************************************************************/

static void background_draw(
    /** Widget data pointer */ wigptr wg
)

{

    /* color the background */
    ami_fcolor(wg->wf, ami_backcolor);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    ami_fcolor(wg->wf, ami_black);

}

/** ****************************************************************************

Background event handler

Handles the events posted to backgrounds.

*******************************************************************************/

static void background_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    if (ev->etype == ami_etredraw) background_draw(wg); /* redraw the window */

}

/** ****************************************************************************

Edit box draw handler

Handles drawing edit boxes.

*******************************************************************************/

static void editbox_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int   cl;
    int   x;
    char* s;
    int   err;
    int   v;

    /* see if the numeric contents are in range */
    err = FALSE; /* set no error */
    if (wg->num) {

        v = atoi(wg->face); /* get the value */
        if (v < wg->lbnd || v > wg->ubnd) err = TRUE;

    }
    /* color the background */
    ami_fcolor(wg->wf, ami_white);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    if (!wg->pw) { /* if not subclassed, draw background and outline */

        /* outline */
        if (wg->focus) {

            ami_linewidth(wg->wf, 4);
            fcolort(wg->wf, th_focus);

        } else {

            ami_linewidth(wg->wf, 2);
            fcolort(wg->wf, th_outline1);

        }
        ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1,
                 ami_maxyg(wg->wf)-1, 20, 20);

    }
    /* text */
    if (wg->enb) {

        if (err) fcolort(wg->wf, th_texterr);
        else fcolort(wg->wf, th_text);

    } else fcolort(wg->wf, th_textdis);
    ami_cursorg(wg->wf, ENDLEDSPC, ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2);
    /* check cursor in box */
    if (wg->tleft > strlen(wg->face)) wg->tleft = 0;
    cl = ENDLEDSPC+ami_chrpos(wg->wf, wg->face, wg->curs)-
         ami_chrpos(wg->wf, wg->face, wg->tleft);
    while (cl < ENDLEDSPC && wg->tleft > 0) {

        /* cursor out of field left */
        wg->tleft--; /* back up left margin */
        /* recalculate */
        cl = ENDLEDSPC+ami_chrpos(wg->wf, wg->face, wg->curs)-
             ami_chrpos(wg->wf, wg->face, wg->tleft);

    }
    while (cl > ami_maxxg(wg->wf)-ENDLEDSPC && wg->tleft < strlen(wg->face)) {

        /* cursor out of field right */
        wg->tleft++; /* advance left margin */
        /* recalculate */
        cl = ENDLEDSPC+ami_chrpos(wg->wf, wg->face, wg->curs)-
             ami_chrpos(wg->wf, wg->face, wg->tleft);

    }
    /* display only characters that completely fit the field */
    s = &wg->face[wg->tleft]; /* index displayable string */
    while (*s && ami_curxg(wg->wf)+ami_chrpos(wg->wf, s, 1) <
                 ami_maxxg(wg->wf)-ENDLEDSPC)
        fputc(*s++, wg->wf);
    if (wg->focus && wg->enb) { /* if in focus and enabled, draw the cursor */

        fcolort(wg->wf, th_text); /* set color */
        /* find x location of cursor */
        x = ENDLEDSPC+ami_chrpos(wg->wf, wg->face, wg->curs)-
            ami_chrpos(wg->wf, wg->face, wg->tleft);
        if (wg->ins) { /* in overwrite mode */

            ami_reverse(wg->wf, TRUE); /* set reverse mode */
            ami_bover(wg->wf); /* paint background */
            /* index cursor character */
            ami_cursorg(wg->wf, x, ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2);
            /* if off the end of string, use space to reverse */
            if (wg->curs >= strlen(wg->face)) fputc(' ', wg->wf);
            else fputc(wg->face[wg->curs], wg->wf);
            ami_reverse(wg->wf, FALSE); /* reset reverse mode */
            ami_binvis(wg->wf); /* remove background */

        } else { /* in insert mode */

            ami_linewidth(wg->wf, 2); /* set line size */
            ami_line(wg->wf, x, ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2,
                            x, ami_maxyg(wg->wf)/2-ami_chrsizy(wg->wf)/2+
                            ami_chrsizy(wg->wf));

        }

    }

}

/** ****************************************************************************

Edit box event handler

Handles the events posted to edit boxes.

*******************************************************************************/

static void editbox_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    char*     s;    /* temp string */
    int       l;    /* length */
    int       span; /* span between characters */
    int       off;  /* offset from last character */
    ami_evtrec er;   /* outbound button event */
    int       i;

    switch (ev->etype) {

        case ami_etredraw: /* redraw window */
            editbox_draw(wg); /* redraw the window */
            break;
        case ami_etchar: /* enter character */
            if (!wg->num || isdigit(ev->echar) || ev->echar == '-' ||
                ev->echar == '=') {

                l = strlen(wg->face); /* get length of existing face string */
                if (!wg->ins || wg->curs >= l) { /* insert mode or end */

                    s = malloc(l+1+1); /* get new face string */
                    strcpy(s, wg->face); /* copy old string into place */
                    free(wg->face); /* release previous string */
                    /* move characters after cursor up */
                    for (i = l; i >= wg->curs; i--) s[i+1] = s[i];
                    wg->face = s; /* place new string */

                }
                wg->face[wg->curs] = ev->echar; /* place new character */
                wg->curs++; /* position after character inserted */
                editbox_draw(wg); /* redraw the window */

            }
            break;

        case ami_etfocus: /* gain focus */
            wg->focus = 1; /* in focus */
            editbox_draw(wg); /* redraw */
            /* send light focus event to parent */
            if (wg->pw) { /* if subclassed */

                /* send the event to the parent */
                er.etype = WMC_LGTFOC; /* set light up */
                ami_sendevent(wg->pw->wf, &er);

            }
            break;

        case ami_etnofocus: /* lose focus */
            wg->focus = 0; /* out of focus */
            editbox_draw(wg); /* redraw */
            /* send light focus event to parent */
            if (wg->pw) { /* if subclassed */

                /* send the event to the parent */
                er.etype = WMC_DRKFOC; /* set light up */
                ami_sendevent(wg->pw->wf, &er);

            }
            break;

        case ami_etright: /* right character */
            /* not extreme right, go right */
            if (wg->curs < strlen(wg->face)) {

                wg->curs++;
                editbox_draw(wg); /* redraw */

            }
            break;

        case ami_etleft: /* left character */
            /* not extreme left, go left */
            if (wg->curs > 0) {

                wg->curs--;
                editbox_draw(wg); /* redraw */

            }
            break;

        case ami_etdelcb: /* delete character backward */
            /* not extreme left, delete left */
            if (wg->curs > 0) {

                l = strlen(wg->face); /* get length of existing face string */
                /* back up right characters past cursor */
                for (i = wg->curs-1; i < l; i++) wg->face[i] = wg->face[i+1];
                wg->curs--;
                editbox_draw(wg); /* redraw */

            }
            break;

        case ami_etdelcf: /* delete character forward */
            /* not extreme right, go right */
            if (wg->curs < strlen(wg->face)) {

                l = strlen(wg->face); /* get length of existing face string */
                /* back up right characters past cursor */
                for (i = wg->curs; i < l; i++) wg->face[i] = wg->face[i+1];
                editbox_draw(wg); /* redraw */

            }
            break;

        case ami_etmoumovg: /* mouse moved */
            /* track position */
            wg->mpx = ev->moupxg; /* set present position */
            wg->mpy = ev->moupyg;
            break;

        case ami_etmouba: /* mouse click */
            if (ev->amoubn == 1) {

                /* mouse click, select character it indexes */
                l = strlen(wg->face); /* get length of existing face string */
                i = 0;
                /* find first character beyond click */
                while (ENDLEDSPC+ami_chrpos(wg->wf, wg->face, i) < wg->mpx && i < l) i++;
                if (i) {

                    /* find span between last and next characters */
                    span = ami_chrpos(wg->wf, wg->face, i)-
                           ami_chrpos(wg->wf, wg->face, i-1);
                    /* find offset last to mouse click */
                    off = wg->mpx-(ENDLEDSPC+ami_chrpos(wg->wf, wg->face, i-1));
                    /* if mouse click is closer to last, index last */
                    if (off < span/2) i--;

                }
                wg->curs = i; /* set final position */
                editbox_draw(wg); /* redraw */

            }
            break;

        case ami_etenter: /* signal entry done */
            /* send event back to parent window */
            er.etype = ami_etedtbox; /* set button event */
            er.edtbid = wg->id; /* set id */
            ami_sendevent(wg->parent, &er); /* send the event to the parent */
            break;

        case ami_ethomel: /* beginning of line */
            wg->curs = 0;
            editbox_draw(wg); /* redraw */
            break;

        case ami_etendl: /* end of line */
            wg->curs = strlen(wg->face);
            editbox_draw(wg); /* redraw */
            break;

        case ami_etinsertt: /* toggle insert mode */
            wg->ins = !wg->ins;
            editbox_draw(wg); /* redraw */
            break;

        case ami_etdell: /* delete whole line */
            wg->curs = 0;
            wg->face[0] = 0;
            editbox_draw(wg); /* redraw */
            break;

        case ami_etleftw: /* left word */
            /* back over any spaces */
            while (wg->curs > 0 && wg->face[wg->curs-1] == ' ') wg->curs--;
            /* now back over any non-space */
            while (wg->curs > 0 && wg->face[wg->curs-1] != ' ') wg->curs--;
            editbox_draw(wg); /* redraw */
            break;

        case ami_etrightw: /* right word */
            l = strlen(wg->face); /* get string length */
            /* advance over any non-space */
            while (wg->curs < l && wg->face[wg->curs] != ' ') wg->curs++;
            /* advance over any spaces */
            while (wg->curs < l && wg->face[wg->curs] == ' ') wg->curs++;
            editbox_draw(wg); /* redraw */
            break;

        default: ;

    }

}

/** ****************************************************************************

Number select box draw handler

Handles drawing number select boxes.

*******************************************************************************/

static void numselbox_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int udspc; /* up/down control space */
    int figsiz; /* size of up/down figures */

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */
    /* color the background */
    ami_fcolor(wg->wf, ami_white);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    if (wg->downpress) {

        fcolort(wg->wf, th_backpressed);
        ami_frect(wg->wf, ami_maxxg(wg->wf)-udspc*2, 1,
                        ami_maxxg(wg->wf)-udspc*1-1, ami_maxyg(wg->wf));

    } else if (wg->uppress) {

        fcolort(wg->wf, th_backpressed);
        ami_frect(wg->wf, ami_maxxg(wg->wf)-udspc*1, 1,
                        ami_maxxg(wg->wf), ami_maxyg(wg->wf));

    }
    /* draw divider lines */
    fcolort(wg->wf, th_numseldiv);
    ami_line(wg->wf, ami_maxxg(wg->wf)-udspc, 1,
                    ami_maxxg(wg->wf)-udspc, ami_maxyg(wg->wf));
    ami_line(wg->wf, ami_maxxg(wg->wf)-udspc*2, 1,
                    ami_maxxg(wg->wf)-udspc*2, ami_maxyg(wg->wf));
    /* draw up/down figures */
    figsiz = ami_maxyg(wg->wf)/2; /* set figure size */
    fcolort(wg->wf, th_numselud);
    ami_line(wg->wf, ami_maxxg(wg->wf)-udspc*1.5-figsiz/2.75, ami_maxyg(wg->wf)/2,
                    ami_maxxg(wg->wf)-udspc*1.5+figsiz/2.75, ami_maxyg(wg->wf)/2);
    ami_line(wg->wf, ami_maxxg(wg->wf)-udspc*0.5-figsiz/2.75, ami_maxyg(wg->wf)/2,
                    ami_maxxg(wg->wf)-udspc*0.5+figsiz/2.75, ami_maxyg(wg->wf)/2);
    ami_line(wg->wf, ami_maxxg(wg->wf)-udspc*0.5, ami_maxyg(wg->wf)/2-figsiz/2.75,
                    ami_maxxg(wg->wf)-udspc*0.5, ami_maxyg(wg->wf)/2+figsiz/2.75);
    /* outline */
    if (wg->focus | wg->cw->focus) {

        ami_linewidth(wg->wf, 4);
        fcolort(wg->wf, th_focus);

    } else {

        ami_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_outline1);

    }
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 20, 20);

}

/** ****************************************************************************

Number select box event handler

Handles the events posted to number select boxes.

*******************************************************************************/

static void numselbox_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    int  udspc;    /* up/down control space */
    char buff[20]; /* buffer for number entered */
    ami_evtrec er;  /* outbound button event */
    int  v;

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */
    switch (ev->etype) {

        case ami_etredraw: /* redraw window */
            numselbox_draw(wg); /* redraw the window */
            break;

        case ami_etfocus: /* gain focus */
            wg->focus = 1; /* in focus */
            /* if we get focus, send it on to subclassed edit window */
            ami_focus(wg->cw->wf);
            numselbox_draw(wg); /* redraw */
            break;

        case ami_etnofocus: /* lose focus */
            wg->focus = 0; /* out of focus */
            numselbox_draw(wg); /* redraw */
            break;

        case ami_etedtbox: /* signal entry done */
            /* send event back to parent window */
            er.etype = ami_etnumbox; /* set button event */
            er.numbid = wg->id; /* set id */
            er.numbsl = atoi(wg->cw->face); /* set value */
            ami_sendevent(wg->parent, &er); /* send the event to the parent */
            break;

        case ami_etmoumovg: /* mouse moved */
            /* track position */
            wg->mpx = ev->moupxg; /* set present position */
            wg->mpy = ev->moupyg;
            break;

        case ami_etmouba: /* mouse click */
            if (ev->amoubn == 1) {

                if (wg->cw->face[0]) {

                    if (wg->mpx >= ami_maxxg(wg->wf)-udspc*2 &&
                        wg->mpx < ami_maxxg(wg->wf)-udspc) {

                        /* down control */
                        ami_getwidgettext(wg->wf, wg->cw->id, buff, 20);
                        v = atoi(buff);
                        if (wg->cw->lbnd < v && v <= wg->cw->ubnd) v--;
                        sprintf(buff, "%d", v);
                        ami_putwidgettext(wg->wf, wg->cw->id, buff);
                        if (wg->cw->curs > strlen(wg->cw->face))
                            wg->cw->curs = strlen(wg->cw->face);
                        editbox_draw(wg->cw);
                        wg->downpress = TRUE; /* set down pressed */
                        numselbox_draw(wg); /* redraw */

                    } else if (wg->mpx >= ami_maxxg(wg->wf)-udspc) {

                        /* up control */
                        ami_getwidgettext(wg->wf, wg->cw->id, buff, 20);
                        v = atoi(buff);
                        if (wg->cw->lbnd <= v && v < wg->cw->ubnd) v++;
                        sprintf(buff, "%d", v);
                        ami_putwidgettext(wg->wf, wg->cw->id, buff);
                        if (wg->cw->curs > strlen(wg->cw->face))
                            wg->cw->curs = strlen(wg->cw->face);
                        editbox_draw(wg->cw);
                        wg->uppress = TRUE; /* set up pressed */
                        numselbox_draw(wg); /* redraw */

                    }

                }

            }
            break;

        case ami_etmoubd: /* mouse click */
            if (ev->dmoubn == 1) {

                wg->downpress = FALSE; /* set none pressed */
                wg->uppress = FALSE;
                numselbox_draw(wg); /* redraw */

            }

        default: ;

    }

}

/** ****************************************************************************

Progress bar display handler

Handles the display of progress bars.

*******************************************************************************/

static void progbar_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int pbpp; /* prog bar pixel position right side */

    /* draw inactive background */
    fcolort(wg->wf, th_proginacen);
    ami_linewidth(wg->wf, 2);
    ami_frrect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf), 10, 10);
    /* draw inactive edget */
    fcolort(wg->wf, th_proginaedg);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 10, 10);
    /* find right side of prog bar */
    pbpp = (long long)wg->ppos*ami_maxxg(wg->wf)/INT_MAX;
    /* now draw active */
    fcolort(wg->wf, th_progactcen);
    ami_linewidth(wg->wf, 2);
    ami_frrect(wg->wf, 1, 1, pbpp, ami_maxyg(wg->wf), 10, 10);
    /* draw inactive edget */
    fcolort(wg->wf, th_progactedg);
    ami_rrect(wg->wf, 2, 2,pbpp-1, ami_maxyg(wg->wf)-1, 10, 10);

}

/** ****************************************************************************

Progress bar event handler

Handles the events posted to progress bars.

*******************************************************************************/

static void progbar_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr     wg
)

{

    if (ev->etype == ami_etredraw) progbar_draw(wg); /* redraw the window */

}

/** ****************************************************************************

List box draw handler

Handles drawing list boxes.

*******************************************************************************/

static void listbox_draw(
    /** Widget data pointer */ wigptr wg
)

{

    ami_strptr sp;
    int       y;
    int       sc;

    /* draw background */
    ami_fcolor(wg->wf, ami_white);
    ami_frrect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf), 10, 10);
    /* draw outline */
    fcolort(wg->wf, th_outline1);
    ami_linewidth(wg->wf, 2);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 10, 10);
    sp = wg->strlst; /* index top of stringlist */
    y = ami_chrsizy(wg->wf)*0.5; /* space to first string */
    ami_fcolor(wg->wf, ami_black);
    sc = 1; /* set first string */
    while (sp) { /* traverse and paint */

        if (wg->hover && sc == wg->ss) {

            /* draw in hover background */
            fcolort(wg->wf, th_lsthov); /* set hover background */
            ami_frect(wg->wf, 1, y, ami_maxxg(wg->wf), y+ami_chrsizy(wg->wf)-1);

        }
        ami_fcolor(wg->wf, ami_black);
        ami_cursorg(wg->wf, ami_chrsizy(wg->wf)*0.5, y);
        fprintf(wg->wf, "%s", sp->str); /* place string */
        y += ami_chrsizy(wg->wf); /* next line */
        sp = sp->next; /* next string */
        sc++; /* next select */

    }

}

/** ****************************************************************************

List box event handler

Handles the events posted to list boxes.

*******************************************************************************/

static void listbox_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr wg
)

{

    ami_evtrec er; /* outbound button event */
    int       y;
    int       sc;
    ami_strptr sp;

    if (ev->etype == ami_etredraw) listbox_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        /* note that if there is a click in the window, there must have also
           a mouse move */
        if (wg->ss) { /* there is a string select */

            /* send event back to parent window */
            er.etype = ami_etlstbox; /* set button event */
            er.lstbid = wg->id; /* set id */
            er.lstbsl = wg->ss; /* set string select */
            if (wg->pw)
                /* send the event to the superclass widget */
                ami_sendevent(wg->pf, &er);
            else
                /* send the event to the parent */
                ami_sendevent(wg->parent, &er);

        }

    } else if (ev->etype == ami_etmoumovg) {

        /* track position */
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;

        /* find which string the mouse is over */
        y = ami_chrsizy(wg->wf)*0.5; /* space to first string */
        sp = wg->strlst; /* index top of string list */
        sc = 1; /* set first string */
        wg->ss = 0; /* set no string selected */
        while (sp) { /* traverse string list */

            /* if within the string bounding box, select it */
            if (wg->mpy >= y && wg->mpy <= y+ami_chrsizy(wg->wf)) wg->ss = sc;
            y += ami_chrsizy(wg->wf); /* next line */
            sc++; /* next select */
            sp = sp->next; /* next string */

        }
        listbox_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_ethover) {

        wg->hover = 1; /* hovered */
        listbox_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnohover) {

        wg->hover = 0; /* not hovered */
        listbox_draw(wg); /* redraw the window */

    }

}

/** ****************************************************************************

Drop box draw handler

Handles drawing drop boxes.

*******************************************************************************/

static void dropbox_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int       ddspc;  /* up/down control space */
    int       figsiz; /* size of up/down figures */
    ami_strptr sp;
    int       sc;
    int       aw;
    int       ah;
    int       cx;
    int       cy;

    ddspc = ami_chrsizy(win0)*1.9; /* square space for dropdown control */
    aw = ddspc*0.3; /* set dropdown arrow width */
    ah = ddspc*0.15; /* set dropdown arrow height */
    cx = ami_maxxg(wg->wf)-ddspc*0.5; /* center dropdown arrow */
    cy = ami_maxyg(wg->wf)*0.5-ami_maxyg(wg->wf)*0.05;
    /* color the background */
    fcolort(wg->wf, th_back);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));

    /* outline */
    if (wg->pw && wg->focus) { /* superclassed by dropeditbox and in focus */

        ami_linewidth(wg->wf, 4);
        fcolort(wg->wf, th_focus);

    } else {

        ami_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_outline2);

    }
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 20, 20);

    /* draw divider lines */
    ami_linewidth(wg->wf, 2);
    fcolort(wg->wf, th_outline2);
    ami_line(wg->wf, ami_maxxg(wg->wf)-ddspc, 1,
                    ami_maxxg(wg->wf)-ddspc, ami_maxyg(wg->wf));
    /* draw dropbox arrow */
    fcolort(wg->wf, th_droparrow);
    ami_ftriangle(wg->wf, cx-aw*0.5, cy, cx+aw*0.5, cy, cx, cy+ah);

    /* draw current select */
    sp = wg->strlst;
    sc = wg->ss;
    /* find selected string */
    while (sc > 1 && sp) { sp = sp->next; sc--; }
    fcolort(wg->wf, th_droptext);
    ami_cursorg(wg->wf, ami_chrsizy(wg->wf)*0.5, ami_chrsizy(wg->wf)*0.5);
    fprintf(wg->wf, "%s", sp->str); /* place string */

}

/** ****************************************************************************

Drop box event handler

Handles the events posted to drop boxes.

*******************************************************************************/

static void dropbox_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr wg
)

{

    int udspc;    /* up/down control space */
    int lbw, lbh; /* listbox sizing */
    int w, h;     /* net width and height */
    ami_evtrec er; /* outbound event */
    FILE* par;    /* ultimate parent */
    int   px,py;  /* position of widget in ultimate parent */
    wigptr wp;

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */
    if (ev->etype == ami_etredraw) dropbox_draw(wg); /* redraw the window */
    else if (ev->etype == WMC_LGTFOC) { /* light focus */

        /* light focus, but we don't really have it */
        wg->focus = 1; /* in focus */
        dropbox_draw(wg); /* redraw */

    } else if (ev->etype == WMC_DRKFOC) { /* lose focus */

        /* dark focus */
        wg->focus = 0; /* out of focus */
        dropbox_draw(wg); /* redraw */

    } else if (ev->etype == ami_etmoumovg) { /* mouse moved */

            /* track position */
            wg->mpx = ev->moupxg; /* set present position */
            wg->mpy = ev->moupyg;

    } else if (ev->etype == ami_etmouba && ev->amoubn == 1) { /* mouse click */

        if (wg->mpx >= ami_maxxg(wg->wf)-udspc) { /* dropdown control */

            /* find parent parameters, since subwidget displays in that
               parent */
            par = wg->parent; /* set near parent */
            px = wg->px; /* set near origin */
            py = wg->py;
            if (wg->pw) { /* if we are subclass, parent is up one */

                par = wg->pw->parent; /* set near parent */
                px = wg->pw->px; /* set near origin */
                py = wg->pw->py;

            }
            if (!wg->cw) { /* not already in dropdown mode */

                /* find dimensions */
                ami_listboxsizg(wg->wf, wg->strlst, &lbw, &lbh);
                w = ami_maxxg(wg->wf); /* set width as same */
                h = lbh;

                /* create the list subwidget */
                wp = getwig(); /* predef so we can plant list before display */
                wp->strlst = wg->strlst; /* plant the list */
                /* set to send messages to us (and not logical parent) */
                wp->pf = wg->wf;
                wg->cw = wp; /* set child widget */
                wp->pw = wg; /* set parent widget */
                /* open listbox */
                wg->cid = ami_getwigid(par); /* get anonymous widget id */
                widget(par, px, py+ami_maxyg(wg->wf)-1,
                            px+w, py+ami_maxyg(wg->wf)-1+h,
                       "", wg->cid, wtlistbox, &wp);

            } else { /* already in dropdown mode */

                ami_killwidget(par, wg->cid); /* close the widget */
                wg->cw = NULL; /* set no child window */

            }

        }

    } if (ev->etype == ami_etlstbox) {

        /* find parent parameters, since subwidget displays in that
           parent */
        par = wg->parent; /* set near parent */
        if (wg->pw) par = wg->pw->parent; /* set near parent up one */
        /* send event back to parent window */
        er.etype = ami_etdrpbox; /* set button event */
        er.drpbid = wg->id; /* set id */
        er.drpbsl = ev->lstbsl; /* set string select */
        /* send the event to the near parent */
        ami_sendevent(wg->parent, &er);
        ami_killwidget(par, wg->cid); /* close the widget in far parent */
        wg->cw = NULL; /* set no child window */
        wg->ss = ev->lstbsl; /* set our new select */
        dropbox_draw(wg); /* redraw our widget */

    }

}

/** ****************************************************************************

Drop edit box draw handler

Handles drawing drop edit boxes.

*******************************************************************************/

static void dropeditbox_draw(
    /** Widget data pointer */ wigptr wg
)

{

    /* everything in this widget is drawn by subclassed widgets */

}

/** ****************************************************************************

Drop edit box event handler

Handles the events posted to drop edit boxes.

*******************************************************************************/

static void dropeditbox_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr wg
)

{

    ami_evtrec er; /* outbound event */
    ami_strptr sp;
    int       sc;
    int       l;

    if (ev->etype == ami_etredraw) dropeditbox_draw(wg); /* redraw the window */
    else if (ev->etype == WMC_LGTFOC) { /* light focus */

        /* edit box got focus, wants us to light it up, cross to dropbox */
        er.etype = WMC_LGTFOC; /* send light focus event */
        ami_sendevent(wg->cw->wf, &er); /* send to dropbox */

    } else if (ev->etype == WMC_DRKFOC) { /* dark focus */

        /* edit box got focus, wants us to turn it off, cross to dropbox */
        er.etype = WMC_DRKFOC; /* set dark focus event */
        ami_sendevent(wg->cw->wf, &er); /* send to child */

    } else if (ev->etype == ami_etdrpbox) {

        /* find current select */
        sp = wg->cw->strlst;
        sc = ev->drpbsl;
        /* find selected string */
        while (sc > 1 && sp) { sp = sp->next; sc--; }
        free(wg->cw2->face); /* free existing face string in edit */
        wg->cw2->face = str(sp->str); /* copy selected to edit */
        l = strlen(sp->str); /* find string length */
        /* if cursor past string, clip it */
        if (wg->cw2->curs > l) wg->cw2->curs = l;
        editbox_draw(wg->cw2); /* redraw edit widget */

    } else if (ev->etype == ami_etedtbox) {

        free(wg->face); /* release previous face string */
        wg->face = str(wg->cw2->face); /* copy the resulting string */
        /* send event back to parent window */
        er.etype = ami_etdrebox; /* set drop edit completion event */
        er.drebid = wg->id; /* set id */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */

    }

}

/** ****************************************************************************

Horizontal slider draw handler

Handles drawing the horizontal slider.

*******************************************************************************/

static void slidehoriz_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int sldsizp;    /* size of slider in pixels */
    int sldposp;    /* position of slider in pixels */
    int mid;        /* y midpoint */
    int thk;        /* slider y thickness */
    int margin;     /* margin at slider edges */
    int trksizp;    /* track size in pixels */
    int insld;      /* mouse is in slider */
    int sldpos;     /* slider position */
    ami_evtrec er;   /* outbound event */
    double tiksizp; /* space between ticks in pixels */
    int tickno;     /* ticks counter */
    int x;

    mid = ami_maxyg(wg->wf)*0.5; /* find y midpoint */
    thk = ami_chrsizy(wg->wf)*0.14; /* find slider track thickness */
    sldsizp = ami_chrsizy(wg->wf)*1.0; /* find slider size in pixels */
    margin = sldsizp*0.5+ENDSPACE; /* set edge margins */
    trksizp = ami_maxxg(wg->wf)-margin*2; /* set track width */
    sldposp = margin+round((double)trksizp*wg->sclpos/INT_MAX);

    /* set status of mouse inside the slider */
    insld = wg->mpx >= sldposp-margin && wg->mpx <= sldposp+margin;

    /* process off slider click */
    if (!insld && wg->pressed && !wg->lpressed) {

        /* find new top if click is middle */
        x = wg->mpx;
        if (x < margin) x = margin; /* limit travel */
        else if (x+sldsizp > ami_maxxg(wg->wf)-margin)
            x = ami_maxxg(wg->wf)-margin;
        /* find new ratioed position */
        sldpos = round((double)INT_MAX*(x-margin)/trksizp);
        wg->sclpos = sldpos; /* place to widget data */
        /* send event back to parent window */
        er.etype = ami_etsldpos; /* set scroll position event */
        er.sldpid = wg->id; /* set id */
        er.sldpos = sldpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */

    } else if ((insld || wg->grab) && wg->pressed && wg->lpressed &&
               wg->mpx != wg->lmpx) {

        /* mouse bar drag, process */
        x = sldposp+(wg->mpx-wg->lmpx)-margin; /* find difference in pixel location */
        if (x < 0) x = 0; /* limit to zero */
        if (x > trksizp) x = trksizp; /* limit to max */
        /* find new ratioed position */
        sldpos = round((double)INT_MAX*x/trksizp);
        wg->sclpos = sldpos; /* place to widget data */
        /* send event back to parent window */
        er.etype = ami_etsldpos; /* set scroll position event */
        er.sldpid = wg->id; /* set id */
        er.sldpos = sldpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */
        wg->grab = TRUE; /* set we grabbed the scrollbar */

    } else if (!wg->pressed) wg->grab = FALSE;

    /* recalculate for any slide movements */
    sldposp = margin+round((double)trksizp*wg->sclpos/INT_MAX);

    /* color the background */
    ami_fcolor(wg->wf, ami_white);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* color scale track */
    fcolort(wg->wf, th_sldint);
    ami_frrect(wg->wf, margin, mid-thk*0.5, ami_maxxg(wg->wf)-margin,
              mid+thk*0.5, 10, 10);
    ami_linewidth(wg->wf, 2);
    fcolort(wg->wf, th_outline2);
    ami_rrect(wg->wf, margin, mid-thk*0.5, ami_maxxg(wg->wf)-margin,
             mid+thk*0.5, 10, 10);
    /* color active side */
    fcolort(wg->wf, th_progactcen);
    ami_frrect(wg->wf, margin, mid-thk*0.5, sldposp, mid+thk*0.5, 10, 10);
    /* draw slider */
    ami_fcolor(wg->wf, ami_white);
    ami_fellipse(wg->wf, sldposp-sldsizp*0.5, mid-sldsizp*0.5,
                       sldposp+sldsizp*0.5, mid+sldsizp*0.5);
    if (wg->pressed && (insld || wg->grab))
        /* color as pressed */
        fcolort(wg->wf, th_droptext);
    else
        /* color as not pressed */
        fcolort(wg->wf, th_outline2);
    ami_ellipse(wg->wf, sldposp-sldsizp*0.5, mid-sldsizp*0.5,
                      sldposp+sldsizp*0.5, mid+sldsizp*0.5);

    /* place tickmarks */
    if (wg->ticks) {

        tiksizp = trksizp/(wg->ticks-1); /* find number of pixels between ticks */
        tickno = 0; /* start at left */
        x = margin+tiksizp*tickno; /* set location */
        ami_fcolor(wg->wf, ami_black); /* set color */
        while (x <= margin+trksizp) { /* place tick marks */

            ami_line(wg->wf, x, 1, x, mid-sldsizp*0.5); /* draw tick */
            tickno++; /* count ticks */
            x = margin+tiksizp*tickno; /* next location */

        }

    }

}

/** ****************************************************************************

Horizontal slider event handler

Handles the events posted to a horizontal slider.

*******************************************************************************/

static void slidehoriz_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr wg
)

{

    if (ev->etype == ami_etredraw) slidehoriz_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        slidehoriz_draw(wg);

    } else if (ev->etype == ami_etmoubd) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = FALSE; /* set not pressed */
        slidehoriz_draw(wg);

    } else if (ev->etype == ami_etmoumovg) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        /* mouse moved, track position */
        wg->lmpx = wg->mpx; /* move present to last */
        wg->lmpy = wg->mpy;
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;
        slidehoriz_draw(wg);
        wg->lmpx = wg->mpx; /* now set equal to cancel move */
        wg->lmpy = wg->mpy;

    }

}

/** ****************************************************************************

Vertical slider draw handler

Handles drawing a vertical slider.

*******************************************************************************/

static void slidevert_draw(
    /** Widget data pointer */ wigptr wg
)

{

    int sldsizp;  /* size of slider in pixels */
    int sldposp;  /* position of slider in pixels */
    int mid;      /* y midpoint */
    int thk;      /* slider y thickness */
    int margin;   /* margin at slider edges */
    int trksizp;  /* track size in pixels */
    int insld;    /* mouse is in slider */
    int sldpos;   /* slider position */
    ami_evtrec er; /* outbound event */
    double tiksizp; /* space between ticks in pixels */
    int tickno;     /* ticks counter */
    int y;

    mid = ami_maxxg(wg->wf)*0.5; /* find x midpoint */
    thk = ami_chrsizy(wg->wf)*0.14; /* find slider track thickness */
    sldsizp = ami_chrsizy(wg->wf)*1.0; /* find slider size in pixels */
    margin = sldsizp*0.5+ENDSPACE; /* set edge margins */
    trksizp = ami_maxyg(wg->wf)-margin*2; /* set track width */
    sldposp = margin+round((double)trksizp*wg->sclpos/INT_MAX);

    /* set status of mouse inside the slider */
    insld = wg->mpy >= sldposp-margin && wg->mpy <= sldposp+margin;

    /* process off slider click */
    if (!insld && wg->pressed && !wg->lpressed) {

        /* find new top if click is middle */
        y = wg->mpy;
        if (y < margin) y = margin; /* limit travel */
        else if (y+sldsizp > ami_maxyg(wg->wf)-margin)
            y = ami_maxyg(wg->wf)-margin;
        /* find new ratioed position */
        sldpos = round((double)INT_MAX*(y-margin)/trksizp);
        wg->sclpos = sldpos; /* place to widget data */
        /* send event back to parent window */
        er.etype = ami_etsldpos; /* set scroll position event */
        er.sldpid = wg->id; /* set id */
        er.sldpos = sldpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */

    } else if ((insld || wg->grab) && wg->pressed && wg->lpressed &&
               wg->mpy != wg->lmpy) {

        /* mouse bar drag, process */
        y = sldposp+(wg->mpy-wg->lmpy)-margin; /* find difference in pixel location */
        if (y < 0) y = 0; /* limit to zero */
        if (y > trksizp) y = trksizp; /* limit to max */
        /* find new ratioed position */
        sldpos = round((double)INT_MAX*y/trksizp);
        wg->sclpos = sldpos; /* place to widget data */
        /* send event back to parent window */
        er.etype = ami_etsldpos; /* set scroll position event */
        er.sldpid = wg->id; /* set id */
        er.sldpos = sldpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */
        wg->grab = TRUE; /* set we grabbed the scrollbar */

    } else if (!wg->pressed) wg->grab = FALSE;

    /* recalculate for any slide movements */
    sldposp = margin+round((double)trksizp*wg->sclpos/INT_MAX);

    /* color the background */
    ami_fcolor(wg->wf, ami_white);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* color scale track */
    fcolort(wg->wf, th_sldint);
    ami_frrect(wg->wf, mid-thk*0.5, margin, mid+thk*0.5,
              ami_maxyg(wg->wf)-margin, 10, 10);
    ami_linewidth(wg->wf, 2);
    fcolort(wg->wf, th_outline2);
    ami_rrect(wg->wf, mid-thk*0.5, margin, mid+thk*0.5,
             ami_maxyg(wg->wf)-margin, 10, 10);
    /* color active side */
    fcolort(wg->wf, th_progactcen);
    ami_frrect(wg->wf, mid-thk*0.5, margin, mid+thk*0.5, sldposp, 10, 10);
    /* draw slider */
    ami_fcolor(wg->wf, ami_white);
    ami_fellipse(wg->wf, mid-sldsizp*0.5, sldposp-sldsizp*0.5,
                       mid+sldsizp*0.5, sldposp+sldsizp*0.5);
    if (wg->pressed && (insld || wg->grab))
        /* color as pressed */
        fcolort(wg->wf, th_droptext);
    else
        /* color as not pressed */
        fcolort(wg->wf, th_outline2);
    ami_ellipse(wg->wf, mid-sldsizp*0.5, sldposp-sldsizp*0.5,
                      mid+sldsizp*0.5, sldposp+sldsizp*0.5);

    /* place tickmarks */
    if (wg->ticks) {

        tiksizp = trksizp/(wg->ticks-1); /* find number of pixels between ticks */
        tickno = 0; /* start at top */
        y = margin+tiksizp*tickno; /* set location */
        ami_fcolor(wg->wf, ami_black); /* set color */
        while (y <= margin+trksizp) { /* place tick marks */

            ami_line(wg->wf, mid+sldsizp*0.5, y, ami_maxxg(wg->wf), y); /* draw tick */
            tickno++; /* count ticks */
            y = margin+tiksizp*tickno; /* next location */

        }

    }

}

/** ****************************************************************************

Vertical slider event handler

Handles the events posted to a vertical slider.

*******************************************************************************/

static void slidevert_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr wg
)

{

    if (ev->etype == ami_etredraw) slidevert_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        slidevert_draw(wg);

    } else if (ev->etype == ami_etmoubd) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = FALSE; /* set not pressed */
        slidevert_draw(wg);

    } else if (ev->etype == ami_etmoumovg) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        /* mouse moved, track position */
        wg->lmpx = wg->mpx; /* move present to last */
        wg->lmpy = wg->mpy;
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;
        slidevert_draw(wg);
        wg->lmpx = wg->mpx; /* now set equal to cancel move */
        wg->lmpy = wg->mpy;

    }

}

/** ****************************************************************************

Tab bar draw handler

Handles drawing a tab bar.

*******************************************************************************/

static void tabbar_draw(
    /** Widget data pointer */ wigptr wg
)

{

    ami_strptr sp; /* string list pointer */
    int       sc;
    int       xm, y, x1, x2;
    int       th; /* tabbar height/width (by orientation) */

    /* find tabbar height/width */
    if (wg->charb) th = ami_chrsizy(wg->parent)*TABHGT; /* character */
    th = ami_chrsizy(wg->wf)*TABHGT; /* graphical */
    if (wg->tor == ami_totop || wg->tor == ami_tobottom) { /* top or bottom */

        /* color the background */
        ami_fcolor(wg->wf, ami_white);
        ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
        fcolort(wg->wf, th_tabback);
        if (wg->tor == ami_totop)
            ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), th);
        else /* bottom */
            ami_frect(wg->wf, 1, ami_maxyg(wg->wf)-th,
                             ami_maxxg(wg->wf), ami_maxyg(wg->wf));
        /* outline */
        ami_linewidth(wg->wf, 1);
        fcolort(wg->wf, th_outline1);
        ami_rect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
        ami_rect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1);
        if (wg->tor == ami_totop) {

            ami_line(wg->wf, 1, th, ami_maxxg(wg->wf), th);
            ami_line(wg->wf, 1, th-1, ami_maxxg(wg->wf), th-1);

        } else {

            ami_line(wg->wf, 1, ami_maxyg(wg->wf)-th+1, 
                            ami_maxxg(wg->wf), ami_maxyg(wg->wf)-th+1);
            ami_line(wg->wf, 1, ami_maxyg(wg->wf)-th+2, 
                            ami_maxxg(wg->wf), ami_maxyg(wg->wf)-th+2);

        }
        /* draw tab text */
        if (wg->tor == ami_totop)
            ami_cursorg(wg->wf, ami_chrsizy(wg->wf), ami_chrsizy(wg->wf)*0.5);
        else /* bottom */
            ami_cursorg(wg->wf, ami_chrsizy(wg->wf), 
                               ami_maxyg(wg->wf)-th+ami_chrsizy(wg->wf)*0.5);
        sp = wg->strlst; /* index tab string list */
        sc = 1; /* set first string */
        while (sp && ami_curxg(wg->wf) <= ami_maxxg(wg->wf)) {

            if (sc == wg->ss || sc == wg->sh) { /* draw select/hover */

                ami_linewidth(wg->wf, 6);
                if (sc == wg->ss) fcolort(wg->wf, th_tabsel);
                else fcolort(wg->wf, th_outline1);
                if (wg->tor == ami_totop)
                    ami_line(wg->wf, ami_curxg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                    th-2,
                                    ami_curxg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                                             ami_chrsizy(wg->wf)*0.5,
                                    th-2);
                else
                    ami_line(wg->wf, ami_curxg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                    ami_maxyg(wg->wf)-th+4,
                                    ami_curxg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                                             ami_chrsizy(wg->wf)*0.5,
                                    ami_maxyg(wg->wf)-th+4);

            }
            if (sc == wg->ss && wg->focus) { /* draw focus box */

                ami_linewidth(wg->wf, 2);
                fcolort(wg->wf, th_tabfocus);
                if (wg->tor == ami_totop)
                    ami_rrect(wg->wf, ami_curxg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                     5,
                                     ami_curxg(wg->wf)+
                                         ami_strsiz(wg->wf, sp->str)+
                                         ami_chrsizy(wg->wf)*0.5,
                                     th-3,
                                     10, 10);
                else
                    ami_rrect(wg->wf, ami_curxg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                     ami_maxyg(wg->wf)-th+5,
                                     ami_curxg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                                         ami_chrsizy(wg->wf)*0.5,
                                     ami_maxyg(wg->wf)-th+th-3,
                                     10, 10);

            }
            fcolort(wg->wf, th_tabdis); /* set color disabled */
            fprintf(wg->wf, "%s", sp->str); /* place button face */
            /* space off between tabs */
            if (sp->next) ami_cursorg(wg->wf,
                                     ami_curxg(wg->wf)+ami_chrsizy(wg->wf),
                                     ami_curyg(wg->wf));
            sp = sp->next; /* next tab */
            sc++; /* count */

        }

    } else { /* left or right */

        /* if in character mode, round tabbar size to character cell */
        if (wg->charb && th % ami_chrsizx(wg->parent)) 
            th = th-(th % ami_chrsizx(wg->parent))+ami_chrsizx(wg->parent);
        /* color the background */
        ami_fcolor(wg->wf, ami_white);
        ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
        fcolort(wg->wf, th_tabback);
        if (wg->tor == ami_toleft) {

            x1 = 1;
            x2 = th;
            ami_frect(wg->wf, x1, 1, x2, ami_maxyg(wg->wf));

        } else {

            x1 = ami_maxxg(wg->wf)-th;
            x2 = ami_maxxg(wg->wf);
            ami_frect(wg->wf, x1+1, 1, x2, ami_maxyg(wg->wf));

        }
        /* outline */
        ami_linewidth(wg->wf, 1);
        fcolort(wg->wf, th_outline1);
        ami_rect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
        ami_rect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1);

        if (wg->tor == ami_toleft) {

            ami_line(wg->wf, th, 1, th, ami_maxyg(wg->wf));
            ami_line(wg->wf, th, 1, th-1, ami_maxyg(wg->wf)-1);

        } else { /* right */

           ami_line(wg->wf, ami_maxxg(wg->wf)-th+1, 1,
                           ami_maxxg(wg->wf)-th+1, ami_maxyg(wg->wf));
           ami_line(wg->wf, ami_maxxg(wg->wf)-th+2, 1,
                           ami_maxxg(wg->wf)-th+2, ami_maxyg(wg->wf));

        }

        /* draw tab text */
        if (wg->tor == ami_toleft)
            ami_cursorg(wg->wf, ami_chrsizy(wg->wf)*0.5, ami_chrsizy(wg->wf));
        else
            ami_cursorg(wg->wf, ami_maxxg(wg->wf)-th+
                                   ami_chrsizy(wg->wf)+ami_chrsizy(wg->wf)*0.5,
                               ami_chrsizy(wg->wf));
        xm = ami_curxg(wg->wf); /* save the left margin */
        sp = wg->strlst; /* index tab string list */
        sc = 1; /* set first string */
        if (wg->tor == ami_toleft)
            ami_path(wg->wf, 0); /* set vertical upwards text */
        else /* right */
            ami_path(wg->wf, INT_MAX/2); /* set vertical downwards text */
        while (sp && ami_curyg(wg->wf) >= 1) {

            if (sc == wg->ss || sc == wg->sh) { /* draw select/hover */

                ami_linewidth(wg->wf, 6);
                if (sc == wg->ss) fcolort(wg->wf, th_tabsel);
                else fcolort(wg->wf, th_outline1);
                if (wg->tor == ami_toleft)
                    ami_line(wg->wf, th-3,
                                    ami_curyg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                    th-3,
                                    ami_curyg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                                             ami_chrsizy(wg->wf)*0.5);
                else
                    ami_line(wg->wf, ami_maxxg(wg->wf)-th+3,
                                    ami_curyg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                    ami_maxxg(wg->wf)-th+3,
                                    ami_curyg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                                             ami_chrsizy(wg->wf)*0.5);

            }
            if (sc == wg->ss && wg->focus) { /* draw focus box */

                ami_linewidth(wg->wf, 2);
                fcolort(wg->wf, th_tabfocus);
                if (wg->tor == ami_toleft)
                    ami_rrect(wg->wf, 5, ami_curyg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                     th-3, ami_curyg(wg->wf)+
                                               ami_strsiz(wg->wf, sp->str)+
                                               ami_chrsizy(wg->wf)*0.5,
                                     10, 10);
                else
                    ami_rrect(wg->wf, ami_maxxg(wg->wf)-th+5,
                                     ami_curyg(wg->wf)-ami_chrsizy(wg->wf)*0.5,
                                     ami_maxxg(wg->wf)-th+th-3,
                                     ami_curyg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                                         ami_chrsizy(wg->wf)*0.5,
                                     10, 10);

            }
            fcolort(wg->wf, th_tabdis); /* set color disabled */
            if (wg->tor == ami_toleft)
                /* back up to start of string */
                ami_cursorg(wg->wf, ami_curxg(wg->wf),
                                   ami_curyg(wg->wf)+ami_strsiz(wg->wf, sp->str));
            y = ami_curyg(wg->wf); /* save y position */
            fprintf(wg->wf, "%s", sp->str); /* place button face */
            /* space off between tabs */
            if (sp->next) {

                if (wg->tor == ami_toleft)
                    ami_cursorg(wg->wf, xm, y+ami_chrsizy(wg->wf));
                else
                    ami_cursorg(wg->wf, xm, ami_curyg(wg->wf)+ami_chrsizy(wg->wf));


            }
            sp = sp->next; /* next tab */
            sc++; /* count */

        }
        ami_path(wg->wf, INT_MAX/4); /* set normal text */

    }

}

/** ****************************************************************************

Tab bar event handler

Handles the events posted to a tab bar.

*******************************************************************************/

static void tabbar_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  wigptr wg
)

{

    ami_evtrec er; /* outbound button event */
    int       th; /* tabbar height/width (by orientation) */
    int       x, y;
    int       sc;
    int       sh;
    ami_strptr sp;

    th = ami_chrsizy(wg->wf)*TABHGT; /* find tabbar height/width graphical */
    if (ev->etype == ami_etredraw) tabbar_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        /* note that if there is a click in the window, there must have also
           a mouse move */
        if (wg->sh) { /* there is a string hover */

            wg->ss = wg->sh; /* set hover as select */
            /* send event back to parent window */
            er.etype = ami_ettabbar; /* set tabbar event */
            er.tabid = wg->id; /* set id */
            er.tabsel = wg->ss; /* set string select */
            /* send the event to the parent */
            ami_sendevent(wg->parent, &er);
            tabbar_draw(wg); /* redraw the window */

        }

    } else if (ev->etype == ami_etmoumovg) {

        /* track position */
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;

        /* find which string the mouse is over */
        if (wg->tor == ami_totop || wg->tor == ami_tobottom)
            x = ami_chrsizy(wg->wf); /* space to first string */
        else
            y = ami_chrsizy(wg->wf); /* space to first string */
        sp = wg->strlst; /* index top of string list */
        sc = 1; /* set first string */
        sh = wg->sh; /* save previous hover */
        wg->sh = 0; /* set no string selected */
        while (sp) { /* traverse string list */

            /* if within the string bounding box, select it */
            if (wg->tor == ami_totop) {

                if (wg->mpx >= x-ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpx <= x+ami_strsiz(wg->wf, sp->str)+ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpy <= th)
                    wg->sh = sc;

            } else if (wg->tor == ami_tobottom) {

                if (wg->mpx >= x-ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpx <= x+ami_strsiz(wg->wf, sp->str)+ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpy >= ami_maxyg(wg->wf)-th)
                    wg->sh = sc;

            } if (wg->tor == ami_toleft) {

                if (wg->mpy >= y-ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpy <= y+ami_strsiz(wg->wf, sp->str)+ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpx <= th)
                    wg->sh = sc;

            } else {

                if (wg->mpy >= y-ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpy <= y+ami_strsiz(wg->wf, sp->str)+ami_chrsizy(wg->wf)*0.5 &&
                    wg->mpx >= ami_maxxg(wg->wf)-th)
                    wg->sh = sc;

            }

            /* next tab */
            if (wg->tor == ami_totop || wg->tor == ami_tobottom)
                x += ami_strsiz(wg->wf, sp->str)+ami_chrsizy(wg->wf);
            else
                y += ami_strsiz(wg->wf, sp->str)+ami_chrsizy(wg->wf);
            sc++; /* next select */
            sp = sp->next; /* next string */

        }
        /* only draw if the hover has changed */
        if (sh != wg->sh) tabbar_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_ethover) {

        wg->hover = 1; /* hovered */
        tabbar_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnohover) {

        wg->hover = 0; /* not hovered */
        tabbar_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etfocus) {

        wg->focus = 1; /* in focus */
        tabbar_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnofocus) {

        wg->focus = 0; /* out of focus */
        tabbar_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etleft &&
                   (wg->tor == ami_totop || wg->tor == ami_tobottom) ||
               ev->etype == ami_etup &&
                   (wg->tor == ami_toleft || wg->tor == ami_toright)) {

        if (wg->focus && wg->strlst) {

            /* in focus, there is a list */
            wg->ss--; /* back up */
            if (wg->ss < 1) { /* off end, go to last entry */

                sp = wg->strlst; /* index top of string list */
                sc = 1; /* set first string */
                /* find last */
                while (sp->next) { sp = sp->next; sc++; }
                wg->ss = sc;

            }
            /* send event back to parent window */
            er.etype = ami_ettabbar; /* set tabbar event */
            er.tabid = wg->id; /* set id */
            er.tabsel = wg->ss; /* set string select */
            /* send the event to the parent */
            ami_sendevent(wg->parent, &er);
            tabbar_draw(wg); /* redraw the window */

        }

    } else if (ev->etype == ami_etright &&
                   (wg->tor == ami_totop || wg->tor == ami_tobottom) ||
               ev->etype == ami_etdown &&
                   (wg->tor == ami_toleft || wg->tor == ami_toright)) {

        if (wg->focus && sp) { /* in focus, there is a list */

            /* find last entry */
            sp = wg->strlst; /* index top of string list */
            sc = 1; /* set first string */
            /* find last */
            while (sp->next) { sp = sp->next; sc++; }
            wg->ss++; /* next tab */
            if (wg->ss > sc) wg->ss = 1; /* off end, wrap */
            /* send event back to parent window */
            er.etype = ami_ettabbar; /* set tabbar event */
            er.tabid = wg->id; /* set id */
            er.tabsel = wg->ss; /* set string select */
            /* send the event to the parent */
            ami_sendevent(wg->parent, &er);
            tabbar_draw(wg); /* redraw the window */

        }

    }

}

/** ****************************************************************************

Widget event handler

Handles the events posted to widgets.

*******************************************************************************/

static void widget_event(
    /** Event record pointer */ ami_evtrec* ev
)

{

    wigptr wg; /* pointer to widget */

    /* if not our window, send it on */
    wg = xltwig[ev->winid+MAXFIL]; /* get possible widget entry */
    if (!wg) widget_event_old(ev);
    else switch (wg->typ) { /* handle according to type */

        case wtcbutton:      cbutton_event(ev, wg); break;
        case wtbutton:       button_event(ev, wg); break;
        case wtcheckbox:     checkbox_event(ev, wg); break;
        case wtradiobutton:  radiobutton_event(ev, wg); break;
        case wtgroup:        group_event(ev, wg); break;
        case wtbackground:   background_event(ev, wg); break;
        case wtscrollvert:   scrollvert_event(ev, wg); break;
        case wtscrollhoriz:  scrollhoriz_event(ev, wg); break;
        case wtnumselbox:    numselbox_event(ev, wg); break;
        case wteditbox:      editbox_event(ev, wg); break;
        case wtprogbar:      progbar_event(ev, wg); break;
        case wtlistbox:      listbox_event(ev, wg); break;
        case wtdropbox:      dropbox_event(ev, wg); break;
        case wtdropeditbox:  dropeditbox_event(ev, wg); break;
        case wtslidehoriz:   slidehoriz_event(ev, wg); break;
        case wtslidevert:    slidevert_event(ev, wg); break;
        case wttabbar:       tabbar_event(ev, wg); break;

    }

}



/** ****************************************************************************

Allocate anonymous widget id

Allocates and returns an "anonymous" widget id for the given window. Normal
widget ids are assigned by the client program. However, there a an alternative
set of ids that are allocated as needed. Graphics keeps track of which anonymous
ids have been allocated and which have been freed.

The implementation here is to assign anonymous ids negative numbers,
starting with -1 and proceeding downwards. 0 is never assigned. The use of
negative ids insure that the normal widget ids will never overlap any anonymous
widget ids.

Note that the widget id entry will actually be opened by a widget create call,
and will be closed by killwidget(), so there is no need to deallocate this
widget id. Once an anonymous id is allocated, it is reserved until it is used
and removed by killwidget().

\returns File pointer for widget.

*******************************************************************************/

static int igetwigid(
    /** Window file */ FILE* f
)

{

    int fn;  /* logical file name */
    int wid; /* widget id */

    fn = fileno(f); /* get the logical file number */
    wid = -1; /* start at -1 */
    /* find any open entry */
    while (wid > -MAXWIG && opnfil[fn]->widgets[wid+MAXWIG]) wid--;
    if (wid == -MAXWIG)
        error("No more anonymous widget IDs"); /* ran out of anonymous wids */

    return (wid); /* return the wid */

}

/** ****************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

static void ikillwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ int id
)

{

    int    fn; /* logical file name */

    fn = fileno(f); /* get the logical file number */
    intkillwidget(fn, id); /* kill widget */

}

/** ****************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

static void iselectwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ int id,
    /** On/off for select */ int e
)

{

    wigptr    wp;  /* widget entry pointer */
    int       chg; /* widget state changes */

    wp = fndwig(f, id); /* index the widget */
    /* check this widget is selectable */
    if (wp->typ != wtcheckbox && wp->typ != wtradiobutton && 
        wp->typ != wtcbutton)
        error("Widget is not selectable");
    chg = wp->select != !!e; /* check select state changes */
    wp->select = !!e; /* set select state */
    /* if the select changes, refresh the checkbox */
    if (chg) widget_redraw(wp); /* send redraw to widget */

}

/** ****************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

static void ienablewidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ int   id,
    /** On/off for enable */ int   e
)

{

    wigptr    wp;  /* widget entry pointer */
    int       chg; /* widget state changes */

    e = !!e; /* clean the enable value */
    wp = fndwig(f, id); /* index the widget */
    /* check this widget can be enabled/disabled */
    if (wp->typ != wtbutton && wp->typ != wtcheckbox &&
        wp->typ != wtradiobutton)
        error("Widget is not disablable");
    chg = wp->enb != e; /* check enable state changes */
    wp->enb = e; /* set enable state */
    /* if the select changes, refresh the checkbox */
    if (chg) widget_redraw(wp); /* send redraw to widget */

}

/** ****************************************************************************

Get widget text

Retrieves the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

static void igetwidgettext(
    /** Window file */                   FILE* f,
    /** Logical widget id */             int   id,
    /** Output pointer to widget text */ char* s,
    /** Length of string buffer */       int   sl
)

{

    wigptr wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* check this widget can have face text read */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox)
        error("Widget content cannot be read");
    /* check face text too large for buffer */
    if (strlen(wp->face) >= sl) error("Face text too large for result");
    strcpy(s, wp->face); /* copy face text to result */

}

/** ****************************************************************************

put edit box text

Places text into an edit box.

*******************************************************************************/

static void iputwidgettext(
    /** Window file */       FILE* f,
    /** Logical widget id */ int   id,
    /** Text to place */     char* s
)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* check this widget can have face text read */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox)
        error("Widget contents cannot be written");
    free(wp->face); /* dispose of previous face string */
    wp->face = str(s); /* place new face */
    widget_redraw(wp); /* send redraw to widget */

}

/** ****************************************************************************

Resize widget graphical

Changes the size of a graphical widget.

*******************************************************************************/

static void isizwidgetg(
    /** Window file */         FILE* f,
    /** Logical widget id */   int   id,
    /** New size for widget */ int   x,
                               int   y
)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    ami_setsizg(wp->wf, x, y); /* set size */

}

/** ****************************************************************************

Resize widget text

Changes the size of a text widget.

*******************************************************************************/

static void isizwidget(
    /** Window file */         FILE* f,
    /** Logical widget id */   int   id,
    /** New size for widget */ int   x,
                               int   y
)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* form graphical from character size */
    x = (x-1)*ami_chrsizx(f)+1;
    y = (y-1)*ami_chrsizy(f)+1;
    ami_setsizg(wp->wf, x, y); /* set size */

}

/** ****************************************************************************

Reposition widget graphical

Changes the parent position of a graphical widget.

*******************************************************************************/

static void iposwidgetg(
    /** Window file */             FILE* f,
    /** Logical widget id */       int   id,
    /** New position for widget */ int   x,
                                   int   y
)

{

    wigptr wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    ami_setposg(wp->wf, x, y); /* set size */

}

/** ****************************************************************************

Reposition widget text

Changes the parent position of a text widget.

*******************************************************************************/

static void iposwidget(
    /** Window file */             FILE* f,
    /** Logical widget id */       int   id,
    /** New position for widget */ int   x,
                                   int   y
)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* form graphical from character coordinates */
    x = (x-1)*ami_chrsizx(f)+1;
    y = (y-1)*ami_chrsizy(f)+1;
    ami_setposg(wp->wf, x, y); /* set size */

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

static void ibackwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ int   id
)

{

    wigptr wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    ami_back(wp->wf); /* place to back */

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

static void ifrontwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ int   id
)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    ami_front(wp->wf); /* place to front */

}

/** ****************************************************************************

Place input focus on a given widget

*******************************************************************************/

static void ifocuswidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ int   id
)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    ami_focus(wp->wf); /* send focus to that window */

}

/** ****************************************************************************

Find minimum/standard button size graphical

Finds the minimum size for a graphical button. Given the face string, the
minimum/ideal size of a button is calculated and returned.

Note the spacing is copied from gnome defaults.

*******************************************************************************/

static void ibuttonsizg(
    /** Window file */           FILE* f,
    /** Face string */           char* s,
    /** Minimum width return */  int*  w,
    /** Minimum height return */ int*  h
)

{

    *h = ami_chrsizy(win0)*2; /* set height */
    *w = ami_strsiz(win0, s)+ami_chrsizy(win0)*2;

}

/** ****************************************************************************

Find minimum/standard button size text

Finds the minimum size for a text button. Given the face string, the
minimum/ideal size of a button is calculated and returned.

Note the spacing is copied from gnome defaults.

*******************************************************************************/

static void ibuttonsiz(
    /** Window file */           FILE* f,
    /** Face string */           char* s,
    /** Minimum width return */  int*  w,
    /** Minimum height return */ int*  h
)

{

    ami_buttonsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / ami_chrsizx(f)+1;
    *h = (*h-1) / ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create button graphical

Creates a standard graphical button within the specified rectangle, on the given
window.

*******************************************************************************/

static void ibuttong(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtbutton, &wp);
    ami_linewidth(wp->wf, 3); /* thicker lines */

}

/** ****************************************************************************

Create button text

Creates a standard text button within the specified rectangle, on the given
window.

*******************************************************************************/

static void ibutton(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_buttong(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard checkbox size graphical

Finds the minimum size for a graphical checkbox. Given the face string, the
minimum size of a checkbox is calculated and returned.

*******************************************************************************/

static void icheckboxsizg(
    /** Window file */   FILE* f,
    /** Face string */   char* s,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    *h = ami_chrsizy(win0)*2; /* set height */
    *w = ami_chrsizy(win0)+ami_chrsizy(win0)/2+ami_strsiz(win0, s)+
         ami_chrsizy(win0)/2;

}

/** ****************************************************************************

Find minimum/standard checkbox size text

Finds the minimum size for a text checkbox. Given the face string, the minimum
size of a checkbox is calculated and returned.

*******************************************************************************/

static void icheckboxsiz(
    /** Window file */   FILE* f,
    /** Face string */   char* s,
    /** Return width */  int*  w,
    /** return height */ int*  h
)

{

    ami_checkboxsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / ami_chrsizx(f)+1;
    *h = (*h-1) / ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create checkbox graphical

Creates a standard graphical checkbox within the specified rectangle, on the
given window.

*******************************************************************************/

static void icheckboxg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** Logical widget id */   int   id)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtcheckbox, &wp);

}

/** ****************************************************************************

Create checkbox text

Creates a standard text checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

static void icheckbox(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_checkboxg(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard radio button size graphical

Finds the minimum size for a graphical radio button. Given the face string, the
minimum size of a radio button is calculated and returned.

*******************************************************************************/

static void iradiobuttonsizg(
    /** Window file */   FILE* f,
    /** Face string */   char* s,
    /** Return width */  int*  w,
    /** Return height */ int*  h)

{

    *h = ami_chrsizy(win0)*2; /* set height */
    *w = ami_chrsizy(win0)+ami_chrsizy(win0)/2+ami_strsiz(win0, s)+
         ami_chrsizy(win0)/2;

}

/** ****************************************************************************

Find minimum/standard radio button size text

Finds the minimum size for a text radio button. Given the face string, the
minimum size of a radio button is calculated and returned.

*******************************************************************************/

static void iradiobuttonsiz(
    /** Window file */   FILE* f,
    /** Face string */   char* s,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    ami_radiobuttonsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / ami_chrsizx(f)+1;
    *h = (*h-1) / ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create radio button graphical

Creates a standard graphical radio button within the specified rectangle, on the
given window.

*******************************************************************************/

static void iradiobuttong(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtradiobutton, &wp);

}

/** ****************************************************************************

Create radio button text

Creates a standard text radio button within the specified rectangle, on the
given window.

*******************************************************************************/

static void iradiobutton(
    /** Window file */ FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_radiobuttong(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard group size graphical

Finds the minimum size for a graphical group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

static void igroupsizg(
    /** Window file */           FILE* f,
    /** Face string */           char* s,
    /** Client width */          int   cw,
    /** Client height */         int   ch,
    /** Returns width */         int*  w,
    /** Returns height */        int*  h,
    /** Returns client origin */ int*  ox,
                                 int*  oy
)

{

    *h = ami_chrsizy(win0)+ch+5; /* set height */
    *w = ami_strsiz(win0, s);
    /* if string is less than client, width is client */
    if (*w < cw+7) *w = cw+7;
    *ox = 4; /* set offset to client */
    *oy = ami_chrsizy(win0);

}

/** ****************************************************************************

Find minimum/standard group size text

Finds the minimum size for a text group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

static void igroupsiz(
    /** Window file */           FILE* f,
    /** Face string */           char* s,
    /** Client width */          int cw,
    /** Client height */         int ch,
    /** Returns width */         int* w,
    /** Returns height */        int* h,
    /** Returns client origin */ int* ox,
                                 int* oy
)

{

    /* convert client sizes to graphical */
    cw = cw*ami_chrsizx(f);
    ch = ch*ami_chrsizy(f);
    ami_groupsizg(f, s, cw, ch, w, h, ox, oy); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/ami_chrsizx(f)+1;
    *h = (*h-1)/ami_chrsizy(f)+1;
    *ox = (*ox-1)/ami_chrsizx(f)+1;
    *oy = (*oy-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create group box graphical

Creates a graphical group box, which is really just a decorative feature that
gererates no messages. It is used as a background for other widgets.

*******************************************************************************/

static void igroupg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtgroup, &wp);

}

/** ****************************************************************************

Create group box text

Creates a text group box, which is really just a decorative feature that
gererates no messages. It is used as a background for other widgets.

*******************************************************************************/

static void igroup(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Face string */         char* s,
    /** logical widget id */   int   id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_groupg(f, x1, y1, x2, y2, s, id); /* create button graphical */

}

/** ****************************************************************************

Create background box graphical

Creates a graphical background box, which is really just a decorative feature
that generates no messages. It is used as a background for other widgets.

*******************************************************************************/

static void ibackgroundg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtbackground, &wp);

}

/** ****************************************************************************

Create background box text

Creates a text background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

static void ibackground(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_backgroundg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard vertical scrollbar size graphical

Finds the minimum size for a graphical vertical scrollbar. The minimum size of a
vertical scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollvertsizg(
    /** Window file */    FILE* f,
    /** Returns width */  int*  w,
    /** Returns height */ int*  h
)

{

    *w = 20;
    *h = 40;

}

/** ****************************************************************************

Find minimum/standard vertical scrollbar size text

Finds the minimum size for a text vertical scrollbar. The minimum size of a
vertical scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollvertsiz(
    /** Window file */    FILE* f,
    /** Returns width */  int*  w,
    /** Returns height */ int*  h
)

{

    ami_scrollvertsizg(f, w, h); /* get in graphics terms */
    /* change graphical size to character */
    *w = (*w-1)/ami_chrsizx(f)+1;
    *h = (*h-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create vertical scrollbar graphical

Creates a graphical vertical scrollbar.

*******************************************************************************/

static void iscrollvertg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtscrollvert, &wp);

}

/** ****************************************************************************

Create vertical scrollbar text

Creates a text vertical scrollbar.

*******************************************************************************/

static void iscrollvert(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_scrollvertg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard horizontal scrollbar size graphical

Finds the minimum size for a graphical horizontal scrollbar. The minimum size of
a horizontal scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollhorizsizg(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    *w = 40;
    *h = 20;

}

/** ****************************************************************************

Find minimum/standard horizontal scrollbar size text

Finds the minimum size for a text horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollhorizsiz(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    ami_scrollhorizsizg(f, w, h); /* get in graphics terms */
    /* change graphical size to character */
    *w = (*w-1)/ami_chrsizx(f)+1;
    *h = (*h-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create horizontal scrollbar graphical

Creates a graphical horizontal scrollbar.

*******************************************************************************/

static void iscrollhorizg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtscrollhoriz, &wp);

}

/** ****************************************************************************

Create horizontal scrollbar text

Creates a text horizontal scrollbar.

*******************************************************************************/

static void iscrollhoriz(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_scrollhorizg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

static void iscrollpos(
    /** Window file */             FILE* f,
    /** Logical widget id */       int id,
    /** Ratioed slider position */ int r
)

{

    wigptr wp;  /* widget entry pointer */

    if (r < 0) error("Invalid scroll bar postition");
    wp = fndwig(f, id); /* index the widget */
    /* check this widget is a scrollbar */
    if (wp->typ != wtscrollvert && wp->typ != wtscrollhoriz)
        error("Widget not a scroll bar");
    wp->sclpos = r; /* set scroll bar postition */
    widget_redraw(wp); /* send redraw to widget */

}

/** ****************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

static void iscrollsiz(
    /** Window file */       FILE* f,
    /** Logical widget id */ int   id,
    /** Ratioed size */      int   r
)

{

    wigptr wp;  /* widget entry pointer */

    if (r < 0) error("Invalid scroll bar size");
    wp = fndwig(f, id); /* index the widget */
    /* check this widget is a scrollbar */
    if (wp->typ != wtscrollvert && wp->typ != wtscrollhoriz)
        error("Widget not a scroll bar");
    wp->sclsiz = r; /* set scroll bar size */
    widget_redraw(wp); /* send redraw to widget */

}

/** ****************************************************************************

Find minimum/standard number select box size graphical

Finds the minimum size for a graphical number select box. The minimum size of a
number select box is calculated and returned.

*******************************************************************************/

static void inumselboxsizg(
    /** Window file */    FILE* f,
    /** Lower bound */    int   l,
    /** Upper bound */    int   u,
    /** Returns width */  int*  w,
    /** Returns height */ int*  h
)

{

    int mv; /* maximum value */
    int dc; /* digit count */
    int udspc; /* up/down control space */

    /* first determine the number of digit places, including the sign */
    mv = u; /* set upper value */
    if (abs(l) > abs(u)) mv = l; /* find maximum digits */
    dc = digits(abs(mv)); /* find the digit count */
    if (mv < 0) dc++; /* add the sign */

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */

    *h = ami_chrsizy(win0)*1.5; /* set height */
    /* width is number of digits, two chry size boxes, and .5 of chry for each
       side for spacing */
    *w = ami_strsiz(win0, "0")*dc+udspc*2+ami_chrsizy(win0); /* set total width */

}

/** ****************************************************************************

Find minimum/standard number select box size text

Finds the minimum size for a text number select box. The minimum size of a
number select box is calculated and returned.

*******************************************************************************/

static void inumselboxsiz(
    /** Window file */    FILE* f,
    /** Lower bound */    int   l,
    /** Upper bound */    int   u,
    /** Returns width */  int*  w,
    /** Returns height */ int*  h
)

{

    ami_numselboxsizg(f, l, u, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / ami_chrsizx(f)+1;
    *h = (*h-1) / ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create number selector graphical

Creates an up/down control for a graphical numeric selection.

*******************************************************************************/

static void inumselboxg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Lower bound */         int   l,
    /** Upper bound */         int   u,
    /** Logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */
    wigptr wps; /* widget subclass entry pointer */
    int udspc; /* up/down control space */

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtnumselbox, &wp);

    /* set up a subclass entry */
    wps = getwig(); /* get widget entry */
    wps->num = TRUE; /* set numeric only */
    wps->lbnd = l; /* set lower bound */
    wps->ubnd = u; /* set upper bound */
    /* subclass an edit control,leaving space for up/down controls */
    widget(wp->wf, 1+4, 1+4, ami_maxxg(wp->wf)-udspc*2-4, ami_maxyg(wp->wf)-4, "", 1,
           wteditbox, &wps);
    ami_curvis(wps->wf, FALSE); /* turn on cursor */
    wp->cw = wps; /* give the master its child window */
    wps->pw = wp; /* give the child its master */

}

/** ****************************************************************************

Create number selector text

Creates an up/down control for a text numeric selection.

*******************************************************************************/

static void inumselbox(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Lower bound */         int   l,
    /** Upper bound */         int   u,
    /** Logical widget id */   int   id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_numselboxg(f, x1, y1, x2, y2, l, u, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard edit box size graphical

Finds the minimum size for a graphical edit box. Given a sample face string, the
minimum size of an edit box is calculated and returned.

*******************************************************************************/

static void ieditboxsizg(
    /** Window file */        FILE* f,
    /** Sample face string */ char* s,
    /** Returns width */      int*  w,
    /** Returns height */     int*  h
)

{

    *h = ami_chrsizy(win0)*1.5; /* set height */
    *w = ami_strsiz(win0, s);

}

/** ****************************************************************************

Find minimum/standard edit box size text

Finds the minimum size for a text edit box. Given a sample face string, the
minimum size of an edit box is calculated and returned.

*******************************************************************************/

static void ieditboxsiz(
    /** Window file */        FILE* f,
    /** Sample face string */ char* s,
    /** Returns width */      int*  w,
    /** Returns height */     int*  h
)

{

    ami_editboxsizg(f, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / ami_chrsizx(f)+1;
    *h = (*h-1) / ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create edit box graphical

Creates single line graphical edit box

*******************************************************************************/

static void ieditboxg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wteditbox, &wp);
    ami_curvis(wp->wf, FALSE); /* turn on cursor */

}

/** ****************************************************************************

Create edit box text

Creates single line text edit box

*******************************************************************************/

static void ieditbox(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_editboxg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard progress bar size graphical

Progress bars are fairly arbitrary, and the dimensions given are more of a
suggestion. The height is based on character size, which is a pretty good base
measure, but the width is really up to the caller.

*******************************************************************************/

static void iprogbarsizg(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    *w = 400;
    *h = ami_chrsizy(win0);

}

/** ****************************************************************************

Find minimum/standard progress bar size text

Progress bars are fairly arbitrary, and the dimensions given are more of a
suggestion. The height is based on character size, which is a pretty good base
measure, but the width is really up to the caller.

*******************************************************************************/

static void iprogbarsiz(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    ami_progbarsizg(f, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/ami_chrsizx(f)+1;
    *h = (*h-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create progress bar graphical

Creates a progress bar.

*******************************************************************************/

static void iprogbarg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtprogbar, &wp);

}

/** ****************************************************************************

Create progress bar text

Creates a progress bar.

*******************************************************************************/

static void iprogbar(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_progbarg(f, x1, y1, x2, y2, id); /* create button graphical */

}

/** ****************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to maxint.

*******************************************************************************/

static void iprogbarpos(
    /** Window file */       FILE* f,
    /** logical widget id */ int id,
    /** Ratioed position */  int pos)

{

    wigptr wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    if (wp->typ != wtprogbar) error("Type of widget is not progress bar");
    if (pos < 0) error("Invalid progress bar position");
    wp->ppos = pos; /* set progress bar position */
    progbar_draw(wp); /* redraw the widget with that */

}

/** ****************************************************************************

Find minimum/standard list box size graphical

Finds the minimum size for a graphical list box. Given a string list, the
minimum size of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

*******************************************************************************/

static void ilistboxsizg(
    /** Window file */         FILE*     f,
    /** string list pointer */ ami_strptr sp,
    /** Return width */        int*      w,
    /** Return height */       int*      h
)

{

    int       lc;   /* line counter */
    int       maxp; /* maximum pixel length */
    int       pl;   /* pixel length */
    ami_strptr sp1;

    lc = 0; /* set no lines */
    maxp = 0; /* set no maximum */
    if (!sp) error("Lines in listbox must be greater than zero");
    sp1 = sp; /* index top of list */
    /* traverse the list */
    while (sp1) {

        lc++; /* count entries */
        pl = ami_strsiz(win0, sp1->str); /* find pixel length this string */
        if (pl > maxp) maxp = pl; /* find maximum */
        sp1 = sp1->next; /* link next */

    }
    *w = maxp+ami_chrsizy(win0); /* set width */
    *h = (lc+1)*ami_chrsizy(win0); /* set height */

}

/** ****************************************************************************

Find minimum/standard list box size text

Finds the minimum size for a textlist box. Given a string list, the minimum size
of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

*******************************************************************************/

static void ilistboxsiz(
    /** Window file */         FILE*     f,
    /** string list pointer */ ami_strptr sp,
    /** Return width */        int*      w,
    /** Return height */       int*      h
)

{

    ami_listboxsizg(f, sp, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/ami_chrsizx(f)+1;
    *h = (*h-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create list box graphical

Creates a graphical list box. Fills it with the string list provided.

*******************************************************************************/

static void ilistboxg(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   int       id
)

{

    wigptr    wp; /* widget entry pointer */
    ami_strptr nl; /* new string list */

    /* make a copy of the list */
    cpystrlst(&nl, sp);

    /* create the widget */
    wp = getwig(); /* predef so we can plant list before display */
    wp->strlst = nl; /* plant the list */
    widget(f, x1, y1, x2, y2, "", id, wtlistbox, &wp);

}

/** ****************************************************************************

Create list box text

Creates a text list box. Fills it with the string list provided.

*******************************************************************************/

static void ilistbox(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** String list pointer */ ami_strptr sp,
    /** logical widget id */   int       id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_listboxg(f, x1, y1, x2, y2, sp, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard dropbox size graphical

Finds the minimum size for a graphical dropbox. Given the face string, the
minimum size of a dropbox is calculated and returned, for both the "open" and
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

static void idropboxsizg(
    /** Window file */         FILE*     f,
    /** String list pointer */ ami_strptr sp,
    /** Closed width */        int*      cw,
    /** Closed height */       int*      ch,
    /** Open width */          int*      ow,
    /** Open height */         int*      oh
)

{

    int lbw, lbh;

    /* find listbox sizing first */
    ami_listboxsizg(f, sp, &lbw, &lbh);

    /* closed size is width of listbox plus down arrow, height is character */
    *cw = lbw+ami_chrsizy(win0)*1.9; /* find closed width */
    *ch = ami_chrsizy(win0)*2; /* find closed height */

    /* open size is same width, height of list plus edit box */
    *ow = *cw;
    *oh = lbh+*ch;

}

/** ****************************************************************************

Find minimum/standard dropbox size text

Finds the minimum size for a text dropbox. Given the face string, the minimum
size of a dropbox is calculated and returned, for both the "open" and "closed"
case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

static void idropboxsiz(
    /** Window file */         FILE*     f,
    /** String list pointer */ ami_strptr sp,
    /** Closed width */        int*      cw,
    /** Closed height */       int*      ch,
    /** Open width */          int*      ow,
    /** Open height */         int*      oh
)

{

    ami_dropboxsizg(f, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/ami_chrsizx(f)+1;
    *ch = (*ch-1)/ami_chrsizy(f)+1;
    *ow = (*ow-1)/ami_chrsizx(f)+1;
    *oh = (*oh-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create dropdown box graphical

Creates a graphical dropdown box. Fills it with the string list provided.

*******************************************************************************/

static void idropboxg(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   int       id
)

{

    wigptr    wp; /* widget entry pointer */
    ami_strptr nl; /* new string list */
    int       ch; /* closed height */

    /* make a copy of the list */
    cpystrlst(&nl, sp);

    /* although the dropbox is specified with its open size, we place the
       window as closed size */
    ch = ami_chrsizy(win0)*2; /* find closed height */

    /* create the widget */
    wp = getwig(); /* predef so we can plant list before display */
    wp->strlst = nl; /* plant the list */
    wp->ss = 1; /* select first entry */
    widget(f, x1, y1, x2, y1+ch-1, "", id, wtdropbox, &wp);

}

/** ****************************************************************************

Create dropdown box text

Creates a text dropdown box. Fills it with the string list provided.

*******************************************************************************/

static void idropbox(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   int       id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_dropboxg(f, x1, y1, x2, y2, sp, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard drop edit box size graphical

Finds the minimum size for a graphical drop edit box. Given the face string, the
minimum size of a drop edit box is calculated and returned, for both the "open"
and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

static void idropeditboxsizg(
    /** Window file */          FILE*     f,
    /** string list pointer */  ami_strptr sp,
    /** Return closed width */  int*      cw,
    /** Return closed height */ int*      ch,
    /** Return open width */    int*      ow,
    /** Return open height */   int*      oh
)

{

    /* the dimensions are identical to a dropbox */
    ami_dropboxsizg(f, sp, cw, ch, ow, oh);

}

/** ****************************************************************************

Find minimum/standard drop edit box size text

Finds the minimum size for a text drop edit box. Given the face string, the
minimum size of a drop edit box is calculated and returned, for both the "open"
and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

static void idropeditboxsiz(
    /** Window file */          FILE*     f,
    /** string list pointer */  ami_strptr sp,
    /** Return closed width */  int*      cw,
    /** Return closed height */ int*      ch,
    /** Return open width */    int*      ow,
    /** Return open height */   int*      oh
)

{

    ami_dropeditboxsizg(f, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/ami_chrsizx(f)+1;
    *ch = (*ch-1)/ami_chrsizy(f)+1;
    *ow = (*ow-1)/ami_chrsizx(f)+1;
    *oh = (*oh-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create dropdown edit box graphical

Creates a graphical dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

static void idropeditboxg(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   int       id
)

{

    wigptr    wp;     /* widget entry pointer */
    wigptr    wps;    /* widget subclass entry pointer */
    ami_strptr nl;     /* new string list */
    int       cw, ch; /* closed dimensions */
    int       ow, oh; /* open dimensions */

    /* find (refind) the dimensions of the subclass box */
    ami_dropboxsizg(f, sp, &cw, &ch, &ow, &oh);

    /* make a copy of the list */
    cpystrlst(&nl, sp);

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y1+ch-1, "", id, wtdropeditbox, &wp);

    /* set up a subclass entry for dropbox */
    wps = getwig(); /* get widget entry */
    wps->strlst = nl; /* set up string list */
    wps->ss = 1; /* select first entry */
    /* subclass drop/edit control */
    widget(wp->wf, 1, 1, cw, ch, "", 1, wtdropbox, &wps);

    wp->cw = wps; /* give the master its child window */
    wps->pw = wp; /* give the child its master */

    /* set up a subclass entry for edit */
    wps = getwig(); /* get widget entry */
    /* subclass an edit control,leaving space for dropbox control */
    widget(wp->wf, 1+4, 1+4,
                   ami_maxxg(wp->wf)-ami_chrsizy(win0)*1.9-4,
                   ami_maxyg(wp->wf)-4,
                   "", 2, wteditbox, &wps);
    ami_curvis(wps->wf, FALSE); /* turn on cursor */
    wp->cw2 = wps; /* give the master its child window */
    wps->pw = wp; /* give the child its master */

}

/** ****************************************************************************

Create dropdown edit box text

Creates a text dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

static void idropeditbox(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   int       id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_dropeditboxg(f, x1, y1, x2, y2, sp, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard horizontal slider size graphical

Finds the minimum size for a graphical horizontal slider. The minimum size of a
horizontal slider is calculated and returned.

*******************************************************************************/

static void islidehorizsizg(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    *w = 40;
    *h = ami_chrsizy(win0)*1.5;

}

/** ****************************************************************************

Find minimum/standard horizontal slider size text

Finds the minimum size for a text horizontal slider. The minimum size of a
horizontal slider is calculated and returned.

*******************************************************************************/

static void islidehorizsiz(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    ami_slidehorizsizg(f, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/ami_chrsizx(f)+1;
    *h = (*h-1)/ami_chrsizy(f)+1;


}

/** ****************************************************************************

Create horizontal slider graphical

Creates a graphical horizontal slider.

*******************************************************************************/

static void islidehorizg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Tick mark interval */  int   mark,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = getwig(); /* predef so we can plant ticks before display */
    wp->ticks = mark; /* set tick marks */
    widget(f, x1, y1, x2, y2, "", id, wtslidehoriz, &wp);

}

/** ****************************************************************************

Create horizontal slider text

Creates a text horizontal slider.

*******************************************************************************/

static void islidehoriz(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Tick mark interval */  int   mark,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_slidehorizg(f, x1, y1, x2, y2, mark, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard vertical slider size graphical

Finds the minimum size for a graphical vertical slider. The minimum size of a
vertical slider is calculated and returned.

*******************************************************************************/

static void islidevertsizg(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    *w = ami_chrsizy(win0)*1.5;
    *h = 40;

}

/** ****************************************************************************

Find minimum/standard vertical slider size text

Finds the minimum size for a text vertical slider. The minimum size of a
vertical slider is calculated and returned.

*******************************************************************************/

static void islidevertsiz(
    /** Window file */   FILE* f,
    /** Return width */  int*  w,
    /** Return height */ int*  h
)

{

    ami_slidevertsizg(f, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/ami_chrsizx(f)+1;
    *h = (*h-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create vertical slider graphical

Creates a graphical vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

static void islidevertg(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Tick mark interval */  int   mark,
    /** logical widget id */   int   id
)

{

    wigptr wp; /* widget entry pointer */

    wp = getwig(); /* predef so we can plant ticks before display */
    wp->ticks = mark; /* set tick marks */
    widget(f, x1, y1, x2, y2, "", id, wtslidevert, &wp);

}

/** ****************************************************************************

Create vertical slider text

Creates a text vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

static void islidevert(
    /** Window file */         FILE* f,
    /** Placement rectangle */ int   x1,
                               int   y1,
                               int   x2,
                               int   y2,
    /** Tick mark interval */  int   mark,
    /** logical widget id */   int   id
)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;
    ami_slidevertg(f, x1, y1, x2, y2, mark, id); /* create button graphical */

}

/** ****************************************************************************

Find minimum/standard tab bar size graphical

Finds the minimum size for a graphical tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

static void itabbarsizg(
    /** Window file */            FILE*     f,
    /** Tab orientation */        ami_tabori tor,
    /** Client width */           int       cw,
    /** Client height */          int       ch,
    /** Return width */           int*      w,
    /** Return height */          int*      h,
    /** Return client offset x */ int*      ox,
    /** Return client offset x */ int*      oy
)

{

    /* find width */
    if (tor == ami_toleft || tor == ami_toright) *w = cw+ami_chrsizy(win0)*TABHGT;
    else *w = cw;
    /* find height */
    if (tor == ami_toleft || tor == ami_toright) *h = ch;
    else *h = ch+ami_chrsizy(win0)*TABHGT;
    /* find client offset */
    if (tor == ami_toleft) *ox = ami_chrsizy(win0)*TABHGT;
    else *ox = 0;
    if (tor == ami_totop) *oy = ami_chrsizy(win0)*TABHGT;
    else *oy = 0;

}

/** ****************************************************************************

Find minimum/standard tab bar size text

Finds the minimum size for a text tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

static void itabbarsiz(
    /** Window file */            FILE*     f,
    /** Tab orientation */        ami_tabori tor,
    /** Client width */           int       cw,
    /** Client height */          int       ch,
    /** Return width */           int*      w,
    /** Return height */          int*      h,
    /** Return client offset x */ int*      ox,
    /** Return client offset x */ int*      oy
)

{

    int gw, gh, gox, goy;

    /* convert client sizes to graphical */
    cw = cw*ami_chrsizx(f);
    ch = ch*ami_chrsizy(f);
    ami_tabbarsizg(f, tor, cw, ch, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *w = (gw-1) / ami_chrsizx(f)+1;
    *h = (gh-1) / ami_chrsizy(f)+1;
    *ox = gox / ami_chrsizx(f);
    if (gox % ami_chrsizx(f)) (*ox)++;
    *oy = goy / ami_chrsizy(f);
    if (goy % ami_chrsizy(f)) (*oy)++;

}

/** ****************************************************************************

Find client from tabbar size graphical

Given a graphical tabbar size and orientation, this routine gives the client
size and offset. This is used where the tabbar size is fixed, but the client
area is flexible.

*******************************************************************************/

static void itabbarclientg(
    /** Window file */            FILE*     f,
    /** Tab orientation */        ami_tabori tor,
    /** Return client width */    int       w,
    /** Return client height */   int       h,
    /** Width */                  int*      cw,
    /** Height */                 int*      ch,
    /** Return client offset x */ int*      ox,
    /** Return client offset x */ int*      oy
)

{

    /* find width */
    if (tor == ami_toleft || tor == ami_toright) *cw = w-ami_chrsizy(win0)*TABHGT;
    else *cw = w;
    /* find height */
    if (tor == ami_toleft || tor == ami_toright) *cw = h;
    else *cw = w-ami_chrsizy(win0)*TABHGT;
    /* find client offset */
    if (tor == ami_toleft) *ox = ami_chrsizy(win0)*TABHGT;
    else *ox = 0;
    if (tor == ami_totop) *oy = ami_chrsizy(win0)*TABHGT;
    else *oy = 0;

}


/** ****************************************************************************

Find client from tabbar size text

Given a text tabbar size and orientation, this routine gives the client size and
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

*******************************************************************************/

static void itabbarclient(
    /** Window file */            FILE*     f,
    /** Tab orientation */        ami_tabori tor,
    /** Return client width */    int       w,
    /** Return client height */   int       h,
    /** Width */                  int*      cw,
    /** Height */                 int*      ch,
    /** Return client offset x */ int*      ox,
    /** Return client offset x */ int*      oy
)

{

    int gw, gh, gox, goy;

    /* convert sizes to graphical */
    w = w*ami_chrsizx(f);
    h = h*ami_chrsizy(f);
    ami_tabbarsizg(f, tor, w, h, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *cw = (gw-1)/ami_chrsizx(f)+1;
    *ch = (gh-1)/ami_chrsizy(f)+1;
    *ox = (gox-1)/ami_chrsizx(f)+1;
    *oy = (goy-1)/ami_chrsizy(f)+1;

}

/** ****************************************************************************

Create tab bar graphical

Creates a graphical tab bar with the given orientation.

*******************************************************************************/

static void itabbarg(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** Tab string list */     ami_strptr sp,
    /** Tab orientation */     ami_tabori tor,
    /** logical widget id */   int       id
)

{

    wigptr wp; /* widget entry pointer */
    ami_strptr nl; /* new string list */

    /* make a copy of the list */
    cpystrlst(&nl, sp);

    /* create the widget */
    wp = getwig(); /* predef so we can plant list before display */
    wp->strlst = nl; /* plant the list */
    wp->ss = 1; /* select first entry */
    wp->tor = tor; /* set tab orientation */
    widget(f, x1, y1, x2, y2, "", id, wttabbar, &wp);

}

/** ****************************************************************************

Create tab bar text

Creates a text tab bar with the given orientation.

*******************************************************************************/

static void itabbar(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ int       x1,
                               int       y1,
                               int       x2,
                               int       y2,
    /** Tab string list */     ami_strptr sp,
    /** Tab orientation */     ami_tabori tor,
    /** logical widget id */   int       id
)

{

    wigptr wp; /* widget entry pointer */
    ami_strptr nl; /* new string list */

    /* form graphical from character coordinates */
    x1 = (x1-1)*ami_chrsizx(f)+1;
    y1 = (y1-1)*ami_chrsizy(f)+1;
    x2 = x2*ami_chrsizx(f)+1;
    y2 = y2*ami_chrsizy(f)+1;

    /* make a copy of the list */
    cpystrlst(&nl, sp);

    /* create the widget */
    wp = getwig(); /* predef so we can plant list before display */
    wp->strlst = nl; /* plant the list */
    wp->ss = 1; /* select first entry */
    wp->tor = tor; /* set tab orientation */
    wp->charb = TRUE; /* set character grid */
    widget(f, x1, y1, x2, y2, "", id, wttabbar, &wp);

}

/** ****************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

static void itabsel(
    /** Window file */         FILE* f,
    /** logical widget id */   int   id,
    /** Logical tab number */  int   tn
)

{

    wigptr    wp;  /* widget entry pointer */
    int       chg; /* widget state changes */
    ami_strptr sp;
    int       ss;

    wp = fndwig(f, id); /* index the widget */
    /* check this widget is tab bar */
    if (wp->typ != wttabbar) error("Widget is not a tab bar");
    sp = wp->strlst; /* index top of string list */
    ss = 1;
    /* find indicated tab */
    while (ss != tn && sp) { sp = sp->next; ss++; }
    if (!sp) error("No tab exists by logical number");
    if (wp->ss != ss) { /* if select has changed */

        wp->ss = ss; /* set select */
        /* refresh */
        widget_redraw(wp); /* send redraw to widget */

    }

}

/** ****************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

*******************************************************************************/

#define ICIRCSIZ 2.3 /* size of i circle */
#define ICHRSIZ  0.3 /* size of i character */

static void ialert(
    /** Title string */   char* title,
    /** Message string */ char* message
)

{

    FILE*      in;       /* window to create */
    FILE*      out;
    int        wid;      /* window number */
    int        mxs;      /* maximum text size */
    ami_evtrec  er;       /* event record */
    int        ts;       /* title pixel size */
    int        ms;       /* message pixel size */
    int        icsize;   /* size of circle i in pixels */
    int        isize;    /* size of i character in pixels */
    int        tstart;   /* start of text to right of i circle */
    int        fs;       /* font size save */
    int        mpx, mpy; /* mouse position */
    themeindex tc;       /* text color */
    int        focus;    /* in focus */

    focus = FALSE; /* set not in focus */
    tc = th_text; /* set focused text */
    wid = ami_getwinid(); /* get anonymous window id */
    ami_openwin(&in, &out, NULL, wid); /* create window */
    ami_buffer(out, FALSE); /* turn off buffering */
    ami_auto(out, FALSE); /* turn off auto */
    ami_curvis(out, FALSE); /* turn off cursor */
    ami_font(out, AMI_FONT_SIGN); /* set sign font */
    ami_binvis(out); /* no background write */
    ami_frame(out, FALSE); /* turn off sizing bars */
    /* find maximum text size */
    ami_bold(out, TRUE); /* set bold */
    fs = ami_chrsizy(out); /* save font size */
    ami_fontsiz(out, fs*1.1); /* increase font size */
    ts = ami_strsiz(out, title);
    ami_fontsiz(out, fs); /* restore font size */
    ami_bold(out, FALSE); /* set normal */
    ms = ami_strsiz(out, message);
    mxs = ts;
    if (ms > mxs) mxs = ms;
    /* set size of i circle */
    icsize = ami_chrsizy(out)*ICIRCSIZ;
    /* size of i character */
    isize = ami_chrsizy(out)*ICHRSIZ;
    /* start of text */
    tstart = icsize*3;
    /* set size */
    ami_setsizg(out, icsize*3+mxs+ami_chrsizy(out)*3,
                    ami_chrsizy(out)*7);

    /* start with events */
    do {

        ami_event(in, &er);
        switch (er.etype) {

            case ami_etfocus:
            case ami_etnofocus:
                if (er.etype == ami_etfocus) {

                    tc = th_text;
                    focus = TRUE;

                } else {

                    tc = th_tabdis;
                    focus = FALSE;

                }
                /* fall through to redraw */

            case ami_etredraw:

                /* draw background */
                ami_fcolor(out, ami_backcolor);
                ami_frect(out, 1, 1, ami_maxxg(out), ami_maxyg(out));
                ami_fcolor(out, ami_white);
                ami_frect(out, 1, ami_maxyg(out)-ami_chrsizy(out)*2,
                                ami_maxxg(out), ami_maxyg(out));;
                fcolort(out, th_outline1);
                ami_linewidth(out, 2);
                ami_line(out, 1, ami_maxyg(out)-ami_chrsizy(out)*2,
                                ami_maxxg(out), ami_maxyg(out)-ami_chrsizy(out)*2);
                if (focus) { /* in focus */

                    ami_linewidth(out, 4);
                    fcolort(out, th_focus);
                    ami_rect(out, 2, ami_maxyg(out)-ami_chrsizy(out)*2+2,
                                    ami_maxxg(out)-1, ami_maxyg(out)-1);

                }

                /* draw circle i */
                fcolort(out, tc);
                ami_linewidth(out, 6);
                ami_ellipse(out, icsize, ami_chrsizy(out),
                                icsize+icsize, ami_chrsizy(out)+icsize);
                ami_fellipse(out, icsize+icsize*0.5-isize*0.5, ami_chrsizy(out)+icsize*0.2,
                                 icsize+icsize*0.5+isize*0.5, ami_chrsizy(out)+icsize*0.2+isize);
                ami_frect(out, icsize+icsize*0.5-isize*0.5, ami_chrsizy(out)+icsize*0.4,
                              icsize+icsize*0.5+isize*0.5, ami_chrsizy(out)+icsize*0.75);

                /* draw title and message */
                fcolort(out, tc);
                ami_bold(out, TRUE); /* set bold */
                fs = ami_chrsizy(out); /* save font size */
                ami_fontsiz(out, fs*1.1); /* increase font size */
                ami_cursorg(out, (ami_maxxg(out)-tstart-ami_chrsizy(out)*2)*0.5-ts*0.5+tstart,
                                ami_chrsizy(out));
                fputs(title, out); /* place title */
                ami_fontsiz(out, fs); /* restore font size */
                ami_bold(out, FALSE); /* set normal */
                ami_cursorg(out, (ami_maxxg(out)-tstart-ami_chrsizy(out)*2)*0.5-ms*0.5+tstart,
                                ami_chrsizy(out)*2.5);
                fputs(message, out); /* place message */

                /* draw ok button */
                fcolort(out, tc);
                ami_cursorg(out, ami_maxxg(out)*0.5-ami_strsiz(out, "OK")*0.5,
                                ami_maxyg(out)-ami_chrsizy(out)*1.5);
                fputs("OK", out);

                break;

            case ami_etmoumovg:
                mpx = er.moupxg; /* save mouse position */
                mpy = er.moupyg;
                break;

            case ami_etmouba:
                if (er.amoubn == 1 && mpy >= ami_maxyg(out)-ami_chrsizy(out)*2)
                    er.etype = ami_etterm;
                break;

            case ami_etenter:
                er.etype = ami_etterm;
                break;

            default: ;

        }

    } while (er.etype != ami_etterm); /* until terminate */

    /* kill the dialog window */
    fclose(out);

}

/** ****************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

*******************************************************************************/

static void iquerycolor(
    /** Input/Output for red ratioed color */   int* r,
    /** Input/Output for green ratioed color */ int* g,
    /** Input/Output for blue ratioed color */  int* b
)

{

    FILE*         in = NULL;  /* window to create */
    FILE*         out;
    int           wid;      /* window number */
    ami_evtrec     er;       /* event record */
    char*         title = "Select a color"; /* title string */
    char*         cancel = "Cancel"; /* cancel string */
    char*         selects = "Select"; /* select string */
    int           titbot;   /* bottom of title bar */
    const double  mg = 0.15; /* button to side margin fraction */
    int           mgt;      /* margin for system bar */
    wigptr        wp;       /* widget entry pointer */
    const double  gtop = 0.65; /* color grid top */
    int           gtopp;
    const double  gside = 0.5; /* color grid side */
    int           gsidep;
    const double  ggapv = 0.1; /* color gap between buttons vertical */
    int           ggapvp;
    const double  ggaph = 0.1; /* color gap between buttons horizontal */
    int           ggaphp;
    const double  ggap = 0.5; /* color to b&w grid gap */
    int           ggapp;
    int           cbx, cby; /* color button size */
    int           rw, cl;   /* row and collumn */
    themeindex    th; /* theme index */
    int           wn; /* widget number */
    int           cusy; /* location of "custom" message */
    int           rs, gs, bs; /* colors selected */
    unsigned long rgb; /* packed color selected */
    int           cursel; /* currently selected color widget */
    int           mpx, mpy; /* mouse position */
    int           lmpx, lmpy; /* last mouse position */
    int           pressed; /* mouse button 1 pressed */
    int           sx, sy; /* screen center */
    int           wpx, wpy; /* window position in parent */
    int           x, y;
    int           dx, dy;

    /* colors for cancel button */
    ccolor cancel_cbc = {

        themetable[th_cancelbackfocus], /* background normal */
        themetable[th_cancelbackfocus], /* background pressed */
        themetable[th_canceloutline],   /* outline normal */
        themetable[th_canceloutline],   /* outline focused */
        themetable[th_canceltextfocus], /* text normal */
        themetable[th_canceltextfocus]  /* text disabled */

    };

    /* colors for select button */
    ccolor select_cbc = {

        themetable[th_selectbackfocus],    /* background normal */
        themetable[th_selectbackfocus],    /* background pressed */
        themetable[th_selectoutline],      /* outline normal */
        themetable[th_selectoutlinefocus], /* outline focused */
        themetable[th_selecttextfocus],    /* text normal */
        themetable[th_selecttextfocus]     /* text disabled */

    };

    /* colors for "+" button */
    ccolor plus_cbc = {

        themetable[th_plusbackfocus],    /* background normal */
        themetable[th_plusbackfocus],    /* background pressed */
        themetable[th_plusoutline],      /* outline normal */
        themetable[th_plusoutlinefocus], /* outline focused */
        themetable[th_plustextfocus],    /* text normal */
        themetable[th_plustextfocus]     /* text disabled */

    };

    /* black/white map for color button checkboxes, on is white */
    int bwmap[36] = {

        TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,
        TRUE,FALSE,FALSE,FALSE,TRUE,TRUE,FALSE,TRUE,FALSE,
        TRUE,TRUE,FALSE,TRUE,TRUE,TRUE,TRUE,TRUE,FALSE,
        TRUE,TRUE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE

    };


    wid = ami_getwinid(); /* get anonymous window id */
    ami_openwin(&in, &out, NULL, wid); /* create window */
    ami_buffer(out, FALSE); /* turn off buffering */
    ami_auto(out, FALSE); /* turn off auto */
    ami_curvis(out, FALSE); /* turn off cursor */
    ami_font(out, AMI_FONT_SIGN); /* set sign font */
    ami_binvis(out); /* no background write */
    ami_frame(out, FALSE); /* turn off sizing bars */
    /* size the dialog */
    ami_setsizg(out, ami_chrsizy(out)*25.8,
                    ami_chrsizy(out)*15.2);

    /* center the dialog */
    ami_scnceng(out, &sx, &sy); /* find screen center */
    ami_getsizg(out, &x, &y); /* find window size */
    wpx = sx-x/2; /* set window position */
    wpy = sy-y/2;
    ami_setposg(out, wpx, wpy); /* set center position */

    titbot = ami_maxyg(out)*0.165; /* set bottom of system bar */
    mgt = titbot*mg; /* set margins */

    /* place cancel button */
    wp = getwig(); /* get widget entry */
    wp->cbc = &cancel_cbc; /* set colors */
    widget(out, mgt, mgt,
                mgt+ami_strsiz(out, cancel)+ami_chrsizy(out)*1.5, titbot-mgt,
                cancel, 1, wtcbutton, &wp);

    /* place select button */
    wp = getwig(); /* get widget entry */
    wp->cbc = &select_cbc; /* set colors */
    widget(out, ami_maxxg(out)-(mgt+ami_strsiz(out, selects)+ami_chrsizy(out)*1.9),
                mgt,
                ami_maxxg(out)-mgt, titbot-mgt,
                selects, 2, wtcbutton, &wp);

    /* calculate spacing for the color grid */
    gtopp = ami_chrsizy(out)*gtop;
    gsidep = ami_chrsizy(out)*gside;
    ggapvp = ami_chrsizy(out)*ggapv;
    ggaphp = ami_chrsizy(out)*ggaph;
    ggapp = ami_chrsizy(out)*ggap;

    /* based on that, find size of individual buttons */
    cbx = (ami_maxxg(out)-gsidep*2-ggapvp*8)/9;
    cby =
        (ami_maxyg(out)-(titbot+gtopp*2+ggaphp*2+ggapp+ami_chrsizy(out)*2))/5;

    /* place color buttons */
    th = th_querycolor1; /* set 1st color button */
    x = gsidep; /* place starting position */
    y = titbot+gtopp;
    wn = 3; /* set first widget id */
    for (rw = 0; rw < 4; rw++) {

        for (cl = 0; cl < 9; cl++) {

            wp = getwig(); /* get widget entry */
            wp->cbc = malloc(sizeof(ccolor)); /* set colors */
            /* set color */
            wp->cbc->bbn = themetable[th];
            wp->cbc->bbp = themetable[th];
            /* set outline as 70 percent lumenosity of that */
            wp->cbc->bon = PERRGB(themetable[th], 70);
            wp->cbc->bof = PERRGB(themetable[th], 70);
            if (bwmap[th-th_querycolor1]) { /* set white checkmark */

                wp->cbc->btn = BW(255);
                wp->cbc->btd = BW(255);

            } else {

                wp->cbc->btn = th_selecttextfocus;
                wp->cbc->btd = th_selecttextfocus;

            }
            wp->check = TRUE; /* set use check instead of text */
            widget(out, x, y, x+cbx-1, y+cby-1, "", wn++, wtcbutton, &wp);
            th++; /* next color */
            x += cbx+ggapvp; /* move to next button */

        }
        x = gsidep; /* reset left */
        /* move to next row */
        if (rw == 2) y += cby+ggapp; /* gap between b&w and color */
        else y += cby+ggaphp; /* gap between color rows */

    }
    cusy = y+ami_chrsizy(out)*0.5; /* find top of "custom" message */

    /* place "+" button */
    y += ami_chrsizy(out)*2; /* position past "+" */
    wp = getwig(); /* get widget entry */
    wp->cbc = &plus_cbc; /* set colors */
    wp->check = TRUE; /* set use check instead of text */
    widget(out, x, y, x+cbx-1, y+cby-1, "+", wn, wtcbutton, &wp);

    /* set initial select */
    rgb = themetable[th_querycolor36]; /* get raw color */
    rs = RED(rgb); /* get the individual colors */
    gs = GREEN(rgb);
    bs = BLUE(rgb);
    ami_selectwidget(out, 2+36, TRUE); /* select the widget */
    cursel = 2+36; /* save selection */

    pressed = FALSE; /* set no mouse button pressed */
    mpx = 0; /* set no mouse position */
    mpy = 0;

    /* start with events */
    do {

        ami_event(in, &er);
        switch (er.etype) {

            case ami_etredraw:
                /* draw background */
                ami_fcolor(out, ami_backcolor);
                ami_frect(out, 1, 1, ami_maxxg(out), ami_maxyg(out));
                /* draw system bar */
                fcolort(out, th_title);
                ami_frect(out, 1, 1, ami_maxxg(out), titbot);
                /* draw title */
                ami_fcolor(out, ami_white);
                ami_bold(out, TRUE);
                ami_cursorg(out, ami_maxxg(out)*0.5-ami_strsiz(out, title)*0.5,
                                titbot*0.5-ami_chrsizy(out)*0.5);
                fputs(title, out);
                ami_bold(out, FALSE);
                /* draw "Custom" */
                ami_cursorg(out, gsidep, cusy);
                ami_fcolor(out, ami_black);
                fputs("Custom", out);
                break;

            case ami_etbutton:
                if (er.butid == 1) /* cancel */
                    er.etype = ami_etterm;
                else if (er.butid == 2) { /* select */

                    *r = rs; /* set final colors */
                    *g = gs;
                    *b = bs;
                    er.etype = ami_etterm;

                } else if (er.butid == 2+36+1) { /* plus */

                    er.etype = ami_etterm;

                } else if (er.butid >= 2+1 && er.butid <= 2+36) { /* color select */

                    rgb = themetable[er.butid-(2+1)+th_querycolor1]; /* get raw color */
                    rs = RED(rgb); /* get the individual colors */
                    gs = GREEN(rgb);
                    bs = BLUE(rgb);
                    /* deselect previous button */
                    ami_selectwidget(out, cursel, FALSE); 
                    ami_selectwidget(out, er.butid, TRUE); /* select the widget */
                    cursel = er.butid; /* save selection */

                }
                break;

            case ami_etmoumovg:
                lmpx = mpx; /* save last mouse position */
                lmpy = mpy;
                mpx = er.moupxg; /* save mouse position */
                mpy = er.moupyg;
                if (pressed && mpx && mpy && lmpx && lmpy && 
                    (lmpx != mpx || lmpy != mpy)) {

                    /* mouse button pressed and has moved */
                    dx = mpx-lmpx; /* find difference */
                    dy = mpy-lmpy;
                    x = wpx+dx; /* find potential new position */
                    y = wpy+dy;
                    if (x > 0 && y > 0) { /* valid new position */

                        wpx = x; /* set new position */
                        wpy = y;
                        ami_setposg(out, wpx, wpy); /* set new position */
                        /* Need to adjust the mouse relative positions. The 
                           mouse moves opposite from the window. */
                        lmpx -= dx;
                        lmpy -= dy;
                        mpx -= dx;
                        mpy -= dy;

                    }

                }
                break;

            case ami_etmouba:
                if (er.amoubn == 1 && mpy <= titbot)
                    pressed = TRUE; /* set mouse button assert */
                break;

            case ami_etmoubd:
                if (er.dmoubn == 1) pressed = FALSE;
                break;

            default: ;

        }

    } while (er.etype != ami_etterm); /* until terminate */

    /* kill the dialog window */
    fclose(out);

}

/** ****************************************************************************

Display choose file dialog for open

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

static void iqueryopen(
    /** Input/output for filename string */ char* s,
    /** Length of filename string buffer */ int sl
)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtqueryopen, &wp);

}

/** ****************************************************************************

Display choose file dialog for save

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

static void iquerysave(
    /** Input/output for filename string */ char* s,
    /** Length of filename string buffer */ int sl
)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtquerysave, &wp);

}

/** ****************************************************************************

Display choose find text dialog

Presents the choose find text dialog, then returns the resulting string.
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
sounds very system dependent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

*******************************************************************************/

static void iqueryfind(
    /** Input/output for search string */   char* s,
    /** Length of search string buffer */ int sl,
    /** Set of find/replace options */      ami_qfnopts* opt
)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtqueryfind, &wp);

}

/** ****************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

*******************************************************************************/

static void iqueryfindrep(
    /** Input/output for search string */  char* s,
    /** Length of search string buffer */  int sl,
    /** Input/output for replace string */ char* r,
    /** Length of replace string buffer */ int rl,
    /** Set of find/replace options */     ami_qfnopts* opt
)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, s, id, wtqueryfindrep, &wp);

}

/** ****************************************************************************

Display choose font dialog

Presents the choose font dialog, then returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made, then updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

*******************************************************************************/

static void iqueryfont(
    /** Window file */                   FILE*          f,
    /** Input/output font code */        int*           fc,
    /** Input/output point size */       int*           s,
    /** Input/output foreground red */   int*           fr,
    /** Input/output foreground green */ int*           fg,
    /** Input/output foreground blue */  int*           fb,
    /** Input/output background red */   int*           br,
    /** Input/output background green */ int*           bg,
    /** Input/output background blue */  int*           bb,
    /** Input/output font effects */     ami_qfteffects* effect
)

{

    wigptr wp; /* widget entry pointer */

    //widget(f, x1, y1, x2, y2, "", id, wtqueryfont, &wp);

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

Close

If the file is attached to an output window, closes the window file. Otherwise,
the close is just passed on.

*******************************************************************************/

static int ivclose(
    /** Base call vector */ pclose_t closedc,
    /** File logical id */  int fd)

{

    int i;

    if (fd < 0 || fd >= MAXFIL) error("Invalid file handle");
    /* check if the file is an output window */
    if (opnfil[fd]) {

        /* close any widgets in file */
        for (i = 0; i < MAXWIG*2+1; i++)
            if (opnfil[fd]->widgets[i]) intkillwidget(fd, i-MAXWIG);
        free(opnfil[fd]); /* free the file record */
        opnfil[fd] = NULL; /* clear it */

    }

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

Widgets startup

*******************************************************************************/

static void init_widgets(void) __attribute__((constructor (103)));
static void init_widgets()

{

    int fn; /* file number */
    int wid; /* window id */

    /* override the event handler */
    ami_eventsover(widget_event, &widget_event_old);

    wigfre = NULL; /* clear widget free list */

    /* clear open files table */
    for (fn = 0; fn < MAXFIL; fn++) opnfil[fn] = NULL;

    /* clear window equivalence table */
    for (fn = 0; fn < MAXFIL*2+1; fn++)
        /* clear window logical number translator table */
        xltwig[fn] = NULL; /* set no widget entry */

    /* open "window 0" dummy window */
    ami_openwin(&stdin, &win0, NULL, ami_getwinid()); /* open window */
    ami_buffer(win0, FALSE); /* turn off buffering */
    ami_auto(win0, FALSE); /* turn off auto (for font change) */
    ami_font(win0, AMI_FONT_SIGN); /* set sign font */
    ami_frame(win0, FALSE); /* turn off frame */

    /* override system calls for basic I/O */
    ovr_close(iclose, &ofpclose);
#ifdef NOCANCEL
    ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);
#endif

    /* override entry calls to widgets */
    _pa_getwigid_ovr(igetwigid, &getwigid_vect);
    _pa_killwidget_ovr(ikillwidget, &killwidget_vect);
    _pa_selectwidget_ovr(iselectwidget, &selectwidget_vect);
    _pa_enablewidget_ovr(ienablewidget, &enablewidget_vect);
    _pa_getwidgettext_ovr(igetwidgettext, &getwidgettext_vect);
    _pa_putwidgettext_ovr(iputwidgettext, &putwidgettext_vect);
    _pa_sizwidget_ovr(isizwidget, &sizwidget_vect);
    _pa_sizwidgetg_ovr(isizwidgetg, &sizwidgetg_vect);
    _pa_poswidget_ovr(iposwidget, &poswidget_vect);
    _pa_poswidgetg_ovr(iposwidgetg, &poswidgetg_vect);
    _pa_backwidget_ovr(ibackwidget, &backwidget_vect);
    _pa_frontwidget_ovr(ifrontwidget, &frontwidget_vect);
    _pa_focuswidget_ovr(ifocuswidget, &focuswidget_vect);
    _pa_buttonsiz_ovr(ibuttonsiz, &buttonsiz_vect);
    _pa_buttonsizg_ovr(ibuttonsizg, &buttonsizg_vect);
    _pa_button_ovr(ibutton, &button_vect);
    _pa_buttong_ovr(ibuttong, &buttong_vect);
    _pa_checkboxsiz_ovr(icheckboxsiz, &checkboxsiz_vect);
    _pa_checkboxsizg_ovr(icheckboxsizg, &checkboxsizg_vect);
    _pa_checkbox_ovr(icheckbox, &checkbox_vect);
    _pa_checkboxg_ovr(icheckboxg, &checkboxg_vect);
    _pa_radiobuttonsiz_ovr(iradiobuttonsiz, &radiobuttonsiz_vect);
    _pa_radiobuttonsizg_ovr(iradiobuttonsizg, &radiobuttonsizg_vect);
    _pa_radiobutton_ovr(iradiobutton, &radiobutton_vect);
    _pa_radiobuttong_ovr(iradiobuttong, &radiobuttong_vect);
    _pa_groupsizg_ovr(igroupsizg, &groupsizg_vect);
    _pa_groupsiz_ovr(igroupsiz, &groupsiz_vect);
    _pa_group_ovr(igroup, &group_vect);
    _pa_groupg_ovr(igroupg, &groupg_vect);
    _pa_background_ovr(ibackground, &background_vect);
    _pa_backgroundg_ovr(ibackgroundg, &backgroundg_vect);
    _pa_scrollvertsizg_ovr(iscrollvertsizg, &scrollvertsizg_vect);
    _pa_scrollvertsiz_ovr(iscrollvertsiz, &scrollvertsiz_vect);
    _pa_scrollvert_ovr(iscrollvert, &scrollvert_vect);
    _pa_scrollvertg_ovr(iscrollvertg, &scrollvertg_vect);
    _pa_scrollhorizsizg_ovr(iscrollhorizsizg, &scrollhorizsizg_vect);
    _pa_scrollhorizsiz_ovr(iscrollhorizsiz, &scrollhorizsiz_vect);
    _pa_scrollhoriz_ovr(iscrollhoriz, &scrollhoriz_vect);
    _pa_scrollhorizg_ovr(iscrollhorizg, &scrollhorizg_vect);
    _pa_scrollpos_ovr(iscrollpos, &scrollpos_vect);
    _pa_scrollsiz_ovr(iscrollsiz, &scrollsiz_vect);
    _pa_numselboxsizg_ovr(inumselboxsizg, &numselboxsizg_vect);
    _pa_numselboxsiz_ovr(inumselboxsiz, &numselboxsiz_vect);
    _pa_numselbox_ovr(inumselbox, &numselbox_vect);
    _pa_numselboxg_ovr(inumselboxg, &numselboxg_vect);
    _pa_editboxsizg_ovr(ieditboxsizg, &editboxsizg_vect);
    _pa_editboxsiz_ovr(ieditboxsiz, &editboxsiz_vect);
    _pa_editbox_ovr(ieditbox, &editbox_vect);
    _pa_editboxg_ovr(ieditboxg, &editboxg_vect);
    _pa_progbarsizg_ovr(iprogbarsizg, &progbarsizg_vect);
    _pa_progbarsiz_ovr(iprogbarsiz, &progbarsiz_vect);
    _pa_progbar_ovr(iprogbar, &progbar_vect);
    _pa_progbarg_ovr(iprogbarg, &progbarg_vect);
    _pa_progbarpos_ovr(iprogbarpos, &progbarpos_vect);
    _pa_listboxsizg_ovr(ilistboxsizg, &listboxsizg_vect);
    _pa_listboxsiz_ovr(ilistboxsiz, &listboxsiz_vect);
    _pa_listbox_ovr(ilistbox, &listbox_vect);
    _pa_listboxg_ovr(ilistboxg, &listboxg_vect);
    _pa_dropboxsizg_ovr(idropboxsizg, &dropboxsizg_vect);
    _pa_dropboxsiz_ovr(idropboxsiz, &dropboxsiz_vect);
    _pa_dropbox_ovr(idropbox, &dropbox_vect);
    _pa_dropboxg_ovr(idropboxg, &dropboxg_vect);
    _pa_dropeditboxsizg_ovr(idropeditboxsizg, &dropeditboxsizg_vect);
    _pa_dropeditboxsiz_ovr(idropeditboxsiz, &dropeditboxsiz_vect);
    _pa_dropeditbox_ovr(idropeditbox, &dropeditbox_vect);
    _pa_dropeditboxg_ovr(idropeditboxg, &dropeditboxg_vect);
    _pa_slidehorizsizg_ovr(islidehorizsizg, &slidehorizsizg_vect);
    _pa_slidehorizsiz_ovr(islidehorizsiz, &slidehorizsiz_vect);
    _pa_slidehoriz_ovr(islidehoriz, &slidehoriz_vect);
    _pa_slidehorizg_ovr(islidehorizg, &slidehorizg_vect);
    _pa_slidevertsizg_ovr(islidevertsizg, &slidevertsizg_vect);
    _pa_slidevertsiz_ovr(islidevertsiz, &slidevertsiz_vect);
    _pa_slidevert_ovr(islidevert, &slidevert_vect);
    _pa_slidevertg_ovr(islidevertg, &slidevertg_vect);
    _pa_tabbarsizg_ovr(itabbarsizg, &tabbarsizg_vect);
    _pa_tabbarsiz_ovr(itabbarsiz, &tabbarsiz_vect);
    _pa_tabbarclientg_ovr(itabbarclientg, &tabbarclientg_vect);
    _pa_tabbarclient_ovr(itabbarclient, &tabbarclient_vect);
    _pa_tabbar_ovr(itabbar, &tabbar_vect);
    _pa_tabbarg_ovr(itabbarg, &tabbarg_vect);
    _pa_tabsel_ovr(itabsel, &tabsel_vect);
    _pa_alert_ovr(ialert, &alert_vect);
    _pa_querycolor_ovr(iquerycolor, &querycolor_vect);
    _pa_queryopen_ovr(iqueryopen, &queryopen_vect);
    _pa_querysave_ovr(iquerysave, &querysave_vect);
    _pa_queryfind_ovr(iqueryfind, &queryfind_vect);
    _pa_queryfindrep_ovr(iqueryfindrep, &queryfindrep_vect);
    _pa_queryfont_ovr(iqueryfont, &queryfont_vect);

    /* fill out the theme table defaults */
    themetable[th_backpressed]        = TD_BACKPRESSED;
    themetable[th_back]               = TD_BACK;
    themetable[th_outline1]           = TD_OUTLINE1;
    themetable[th_text]               = TD_TEXT;
    themetable[th_textdis]            = TD_TEXTDIS;
    themetable[th_focus]              = TD_FOCUS;
    themetable[th_chkrad]             = TD_CHKRAD;
    themetable[th_chkradout]          = TD_CHKRADOUT;
    themetable[th_scrollback]         = TD_SCROLLBACK;
    themetable[th_scrollbar]          = TD_SCROLLBAR;
    themetable[th_scrollbarpressed]   = TD_SCROLLBARPRESSED;
    themetable[th_numseldiv]          = TD_NUMSELDIV;
    themetable[th_numselud]           = TD_NUMSELUD;
    themetable[th_texterr]            = TD_TEXTERR;
    themetable[th_proginacen]         = TD_PROGINACEN;
    themetable[th_proginaedg]         = TD_PROGINAEDG;
    themetable[th_progactcen]         = TD_PROGACTCEN;
    themetable[th_progactedg]         = TD_PROGACTEDG;
    themetable[th_lsthov]             = TD_LSTHOV;
    themetable[th_outline2]           = TD_OUTLINE2;
    themetable[th_droparrow]          = TD_DROPARROW;
    themetable[th_droptext]           = TD_DROPTEXT;
    themetable[th_sldint]             = TD_SLDINT;
    themetable[th_tabdis]             = TD_TABDIS;
    themetable[th_tabback]            = TD_TABBACK;
    themetable[th_tabsel]             = TD_TABSEL;
    themetable[th_tabfocus]           = TD_TABFOCUS;
    themetable[th_cancelbackfocus]    = TD_CANCELBACKFOCUS;
    themetable[th_canceltextfocus]    = TD_CANCELTEXTFOCUS;
    themetable[th_canceloutline]      = TD_CANCELOUTLINE;
    themetable[th_selectbackfocus]    = TD_SELECTBACKFOCUS;
    themetable[th_selectback]         = TD_SELECTBACK;
    themetable[th_selecttextfocus]    = TD_SELECTTEXTFOCUS;
    themetable[th_selecttext]         = TD_SELECTTEXT;
    themetable[th_selectoutline]      = TD_SELECTOUTLINE;
    themetable[th_selectoutlinefocus] = TD_SELECTOUTLINEFOCUS;
    themetable[th_plusbackfocus]      = TD_PLUSBACKFOCUS;
    themetable[th_plusback]           = TD_PLUSBACK;
    themetable[th_plustextfocus]      = TD_PLUSTEXTFOCUS;
    themetable[th_plustext]           = TD_PLUSTEXT;
    themetable[th_plusoutline]        = TD_PLUSOUTLINE;
    themetable[th_plusoutlinefocus]   = TD_PLUSOUTLINEFOCUS;
    themetable[th_title]              = TD_TITLE;
    themetable[th_querycolor1]        = TD_QUERYCOLOR1;
    themetable[th_querycolor2]        = TD_QUERYCOLOR2;
    themetable[th_querycolor3]        = TD_QUERYCOLOR3;
    themetable[th_querycolor4]        = TD_QUERYCOLOR4;
    themetable[th_querycolor5]        = TD_QUERYCOLOR5;
    themetable[th_querycolor6]        = TD_QUERYCOLOR6;
    themetable[th_querycolor7]        = TD_QUERYCOLOR7;
    themetable[th_querycolor8]        = TD_QUERYCOLOR8;
    themetable[th_querycolor9]        = TD_QUERYCOLOR9;
    themetable[th_querycolor10]       = TD_QUERYCOLOR10;
    themetable[th_querycolor11]       = TD_QUERYCOLOR11;
    themetable[th_querycolor12]       = TD_QUERYCOLOR12;
    themetable[th_querycolor13]       = TD_QUERYCOLOR13;
    themetable[th_querycolor14]       = TD_QUERYCOLOR14;
    themetable[th_querycolor15]       = TD_QUERYCOLOR15;
    themetable[th_querycolor16]       = TD_QUERYCOLOR16;
    themetable[th_querycolor17]       = TD_QUERYCOLOR17;
    themetable[th_querycolor18]       = TD_QUERYCOLOR18;
    themetable[th_querycolor19]       = TD_QUERYCOLOR19;
    themetable[th_querycolor20]       = TD_QUERYCOLOR20;
    themetable[th_querycolor21]       = TD_QUERYCOLOR21;
    themetable[th_querycolor22]       = TD_QUERYCOLOR22;
    themetable[th_querycolor23]       = TD_QUERYCOLOR23;
    themetable[th_querycolor24]       = TD_QUERYCOLOR24;
    themetable[th_querycolor25]       = TD_QUERYCOLOR25;
    themetable[th_querycolor26]       = TD_QUERYCOLOR26;
    themetable[th_querycolor27]       = TD_QUERYCOLOR27;
    themetable[th_querycolor28]       = TD_QUERYCOLOR28;
    themetable[th_querycolor29]       = TD_QUERYCOLOR29;
    themetable[th_querycolor30]       = TD_QUERYCOLOR30;
    themetable[th_querycolor31]       = TD_QUERYCOLOR31;
    themetable[th_querycolor32]       = TD_QUERYCOLOR32;
    themetable[th_querycolor33]       = TD_QUERYCOLOR33;
    themetable[th_querycolor34]       = TD_QUERYCOLOR34;
    themetable[th_querycolor35]       = TD_QUERYCOLOR35;
    themetable[th_querycolor36]       = TD_QUERYCOLOR36;

}

/** ****************************************************************************

Widgets shutdown

*******************************************************************************/

static void deinit_widgets(void) __attribute__((destructor (103)));
static void deinit_widgets()

{

    int fn; /* file number */
    int i;

    /* holding copies of system vectors */
    pclose_t cppclose;
    pclose_t cppclose_nocancel;

    /* holding copies of widgets override vectors */
    ami_getwigid_t        cppgetwigid;
    ami_killwidget_t      cppkillwidget;
    ami_selectwidget_t    cppselectwidget;
    ami_enablewidget_t    cppenablewidget;
    ami_getwidgettext_t   cppgetwidgettext;
    ami_putwidgettext_t   cppputwidgettext;
    ami_sizwidget_t       cppsizwidget;
    ami_sizwidgetg_t      cppsizwidgetg;
    ami_poswidget_t       cppposwidget;
    ami_poswidgetg_t      cppposwidgetg;
    ami_backwidget_t      cppbackwidget;
    ami_frontwidget_t     cppfrontwidget;
    ami_focuswidget_t     cppfocuswidget;
    ami_buttonsiz_t       cppbuttonsiz;
    ami_buttonsizg_t      cppbuttonsizg;
    ami_button_t          cppbutton;
    ami_buttong_t         cppbuttong;
    ami_checkboxsiz_t     cppcheckboxsiz;
    ami_checkboxsizg_t    cppcheckboxsizg;
    ami_checkbox_t        cppcheckbox;
    ami_checkboxg_t       cppcheckboxg;
    ami_radiobuttonsiz_t  cppradiobuttonsiz;
    ami_radiobuttonsizg_t cppradiobuttonsizg;
    ami_radiobutton_t     cppradiobutton;
    ami_radiobuttong_t    cppradiobuttong;
    ami_groupsizg_t       cppgroupsizg;
    ami_groupsiz_t        cppgroupsiz;
    ami_group_t           cppgroup;
    ami_groupg_t          cppgroupg;
    ami_background_t      cppbackground;
    ami_backgroundg_t     cppbackgroundg;
    ami_scrollvertsizg_t  cppscrollvertsizg;
    ami_scrollvertsiz_t   cppscrollvertsiz;
    ami_scrollvert_t      cppscrollvert;
    ami_scrollvertg_t     cppscrollvertg;
    ami_scrollhorizsizg_t cppscrollhorizsizg;
    ami_scrollhorizsiz_t  cppscrollhorizsiz;
    ami_scrollhoriz_t     cppscrollhoriz;
    ami_scrollhorizg_t    cppscrollhorizg;
    ami_scrollpos_t       cppscrollpos;
    ami_scrollsiz_t       cppscrollsiz;
    ami_numselboxsizg_t   cppnumselboxsizg;
    ami_numselboxsiz_t    cppnumselboxsiz;
    ami_numselbox_t       cppnumselbox;
    ami_numselboxg_t      cppnumselboxg;
    ami_editboxsizg_t     cppeditboxsizg;
    ami_editboxsiz_t      cppeditboxsiz;
    ami_editbox_t         cppeditbox;
    ami_editboxg_t        cppeditboxg;
    ami_progbarsizg_t     cppprogbarsizg;
    ami_progbarsiz_t      cppprogbarsiz;
    ami_progbar_t         cppprogbar;
    ami_progbarg_t        cppprogbarg;
    ami_progbarpos_t      cppprogbarpos;
    ami_listboxsizg_t     cpplistboxsizg;
    ami_listboxsiz_t      cpplistboxsiz;
    ami_listbox_t         cpplistbox;
    ami_listboxg_t        cpplistboxg;
    ami_dropboxsizg_t     cppdropboxsizg;
    ami_dropboxsiz_t      cppdropboxsiz;
    ami_dropbox_t         cppdropbox;
    ami_dropboxg_t        cppdropboxg;
    ami_dropeditboxsizg_t cppdropeditboxsizg;
    ami_dropeditboxsiz_t  cppdropeditboxsiz;
    ami_dropeditbox_t     cppdropeditbox;
    ami_dropeditboxg_t    cppdropeditboxg;
    ami_slidehorizsizg_t  cppslidehorizsizg;
    ami_slidehorizsiz_t   cppslidehorizsiz;
    ami_slidehoriz_t      cppslidehoriz;
    ami_slidehorizg_t     cppslidehorizg;
    ami_slidevertsizg_t   cppslidevertsizg;
    ami_slidevertsiz_t    cppslidevertsiz;
    ami_slidevert_t       cppslidevert;
    ami_slidevertg_t      cppslidevertg;
    ami_tabbarsizg_t      cpptabbarsizg;
    ami_tabbarsiz_t       cpptabbarsiz;
    ami_tabbarclientg_t   cpptabbarclientg;
    ami_tabbarclient_t    cpptabbarclient;
    ami_tabbar_t          cpptabbar;
    ami_tabbarg_t         cpptabbarg;
    ami_tabsel_t          cpptabsel;
    ami_alert_t           cppalert;
    ami_querycolor_t      cppquerycolor;
    ami_queryopen_t       cppqueryopen;
    ami_querysave_t       cppquerysave;
    ami_queryfind_t       cppqueryfind;
    ami_queryfindrep_t    cppqueryfindrep;
    ami_queryfont_t       cppqueryfont;

    /* shut down file and widgets */
    for (fn = 0; fn < MAXFIL; fn++) if (opnfil[fn]) {

        /* close any widgets in file */
        for (i = 0; i < MAXWIG*2+1; i++)
            if (opnfil[fn]->widgets[i]) intkillwidget(fn, i-MAXWIG);
        free(opnfil[fn]); /* free the file record */
        opnfil[fn] = NULL; /* clear it */

    }

    /* swap old override vectors for existing vectors */
    _pa_getwigid_ovr(getwigid_vect, &cppgetwigid);

    _pa_killwidget_ovr(killwidget_vect, &cppkillwidget);
    _pa_selectwidget_ovr(selectwidget_vect, &cppselectwidget);
    _pa_enablewidget_ovr(enablewidget_vect, &cppenablewidget);
    _pa_getwidgettext_ovr(getwidgettext_vect, &cppgetwidgettext);
    _pa_putwidgettext_ovr(putwidgettext_vect, &cppputwidgettext);
    _pa_sizwidget_ovr(sizwidget_vect, &cppsizwidget);
    _pa_sizwidgetg_ovr(sizwidgetg_vect, &cppsizwidgetg);
    _pa_poswidget_ovr(poswidget_vect, &cppposwidget);
    _pa_poswidgetg_ovr(poswidgetg_vect, &cppposwidgetg);
    _pa_backwidget_ovr(backwidget_vect, &cppbackwidget);
    _pa_frontwidget_ovr(frontwidget_vect, &cppfrontwidget);
    _pa_focuswidget_ovr(focuswidget_vect, &cppfocuswidget);
    _pa_buttonsiz_ovr(buttonsiz_vect, &cppbuttonsiz);
    _pa_buttonsizg_ovr(buttonsizg_vect, &cppbuttonsizg);
    _pa_button_ovr(button_vect, &cppbutton);
    _pa_buttong_ovr(buttong_vect, &cppbuttong);
    _pa_checkboxsiz_ovr(checkboxsiz_vect, &cppcheckboxsiz);
    _pa_checkboxsizg_ovr(checkboxsizg_vect, &cppcheckboxsizg);
    _pa_checkbox_ovr(checkbox_vect, &cppcheckbox);
    _pa_checkboxg_ovr(checkboxg_vect, &cppcheckboxg);
    _pa_radiobuttonsiz_ovr(radiobuttonsiz_vect, &cppradiobuttonsiz);
    _pa_radiobuttonsizg_ovr(radiobuttonsizg_vect, &cppradiobuttonsizg);
    _pa_radiobutton_ovr(radiobutton_vect, &cppradiobutton);
    _pa_radiobuttong_ovr(radiobuttong_vect, &cppradiobuttong);
    _pa_groupsizg_ovr(groupsizg_vect, &cppgroupsizg);
    _pa_groupsiz_ovr(groupsiz_vect, &cppgroupsiz);
    _pa_group_ovr(group_vect, &cppgroup);
    _pa_groupg_ovr(groupg_vect, &cppgroupg);
    _pa_background_ovr(background_vect, &cppbackground);
    _pa_backgroundg_ovr(backgroundg_vect, &cppbackgroundg);
    _pa_scrollvertsizg_ovr(scrollvertsizg_vect, &cppscrollvertsizg);
    _pa_scrollvertsiz_ovr(scrollvertsiz_vect, &cppscrollvertsiz);
    _pa_scrollvert_ovr(scrollvert_vect, &cppscrollvert);
    _pa_scrollvertg_ovr(scrollvertg_vect, &cppscrollvertg);
    _pa_scrollhorizsizg_ovr(scrollhorizsizg_vect, &cppscrollhorizsizg);
    _pa_scrollhorizsiz_ovr(scrollhorizsiz_vect, &cppscrollhorizsiz);
    _pa_scrollhoriz_ovr(scrollhoriz_vect, &cppscrollhoriz);
    _pa_scrollhorizg_ovr(scrollhorizg_vect, &cppscrollhorizg);
    _pa_scrollpos_ovr(scrollpos_vect, &cppscrollpos);
    _pa_scrollsiz_ovr(scrollsiz_vect, &cppscrollsiz);
    _pa_numselboxsizg_ovr(numselboxsizg_vect, &cppnumselboxsizg);
    _pa_numselboxsiz_ovr(numselboxsiz_vect, &cppnumselboxsiz);
    _pa_numselbox_ovr(numselbox_vect, &cppnumselbox);
    _pa_numselboxg_ovr(numselboxg_vect, &cppnumselboxg);
    _pa_editboxsizg_ovr(editboxsizg_vect, &cppeditboxsizg);
    _pa_editboxsiz_ovr(editboxsiz_vect, &cppeditboxsiz);
    _pa_editbox_ovr(editbox_vect, &cppeditbox);
    _pa_editboxg_ovr(editboxg_vect, &cppeditboxg);
    _pa_progbarsizg_ovr(progbarsizg_vect, &cppprogbarsizg);
    _pa_progbarsiz_ovr(progbarsiz_vect, &cppprogbarsiz);
    _pa_progbar_ovr(progbar_vect, &cppprogbar);
    _pa_progbarg_ovr(progbarg_vect, &cppprogbarg);
    _pa_progbarpos_ovr(progbarpos_vect, &cppprogbarpos);
    _pa_listboxsizg_ovr(listboxsizg_vect, &cpplistboxsizg);
    _pa_listboxsiz_ovr(listboxsiz_vect, &cpplistboxsiz);
    _pa_listbox_ovr(listbox_vect, &cpplistbox);
    _pa_listboxg_ovr(listboxg_vect, &cpplistboxg);
    _pa_dropboxsizg_ovr(dropboxsizg_vect, &cppdropboxsizg);
    _pa_dropboxsiz_ovr(dropboxsiz_vect, &cppdropboxsiz);
    _pa_dropbox_ovr(dropbox_vect, &cppdropbox);
    _pa_dropboxg_ovr(dropboxg_vect, &cppdropboxg);
    _pa_dropeditboxsizg_ovr(dropeditboxsizg_vect, &cppdropeditboxsizg);
    _pa_dropeditboxsiz_ovr(dropeditboxsiz_vect, &cppdropeditboxsiz);
    _pa_dropeditbox_ovr(dropeditbox_vect, &cppdropeditbox);
    _pa_dropeditboxg_ovr(dropeditboxg_vect, &cppdropeditboxg);
    _pa_slidehorizsizg_ovr(slidehorizsizg_vect, &cppslidehorizsizg);
    _pa_slidehorizsiz_ovr(slidehorizsiz_vect, &cppslidehorizsiz);
    _pa_slidehoriz_ovr(slidehoriz_vect, &cppslidehoriz);
    _pa_slidehorizg_ovr(slidehorizg_vect, &cppslidehorizg);
    _pa_slidevertsizg_ovr(slidevertsizg_vect, &cppslidevertsizg);
    _pa_slidevertsiz_ovr(slidevertsiz_vect, &cppslidevertsiz);
    _pa_slidevert_ovr(slidevert_vect, &cppslidevert);
    _pa_slidevertg_ovr(slidevertg_vect, &cppslidevertg);
    _pa_tabbarsizg_ovr(tabbarsizg_vect, &cpptabbarsizg);
    _pa_tabbarsiz_ovr(tabbarsiz_vect, &cpptabbarsiz);
    _pa_tabbarclientg_ovr(tabbarclientg_vect, &cpptabbarclientg);
    _pa_tabbarclient_ovr(tabbarclient_vect, &cpptabbarclient);
    _pa_tabbar_ovr(tabbar_vect, &cpptabbar);
    _pa_tabbarg_ovr(tabbarg_vect, &cpptabbarg);
    _pa_tabsel_ovr(tabsel_vect, &cpptabsel);
    _pa_alert_ovr(alert_vect, &cppalert);
    _pa_querycolor_ovr(querycolor_vect, &cppquerycolor);
    _pa_queryopen_ovr(queryopen_vect, &cppqueryopen);
    _pa_querysave_ovr(querysave_vect, &cppquerysave);
    _pa_queryfind_ovr(queryfind_vect, &cppqueryfind);
    _pa_queryfindrep_ovr(queryfindrep_vect, &cppqueryfindrep);
    _pa_queryfont_ovr(queryfont_vect, &cppqueryfont);

    /* swap old vectors for existing vectors */
    ovr_close(ofpclose, &cppclose);
#ifdef NOCANCEL
    ovr_close_nocancel(ofpclose_nocancel, &cppclose_nocancel);
#endif

    /* if we don't see our own vector flag an error */
    if (cppclose != iclose) error("System override vector mismatch");

}
