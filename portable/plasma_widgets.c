/** ****************************************************************************

\file

\brief WIDGETS PACKAGE FOR KDE PLASMA APPEARANCE

Copyright (C) 2026 Scott A. Franco

2026/08/17 S. A. Franco

This is the KDE Plasma look and feel widget package for Petit-Ami, formed
from the Gnome package and restyled to the Breeze theme: the Breeze blue
accent, flat light surfaces, small corner radii, and dialogs carrying their
buttons at the bottom right in the Qt order. It uses Petit-Ami graphics
statements to construct and operate widgets, and thus is portable to any
system with Petit-Ami up to the graphical management level.

The package registers itself only when the desktop is KDE (XDG_CURRENT_DESKTOP),
its Gnome sibling only when it is not, so one binary serves both desktops.

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
#include <strings.h> /* strcasecmp */
#include <ctype.h>
#include <math.h>
#include <time.h>

/* linux definitions */
#include <limits.h>

/* local definitions */
#include <localdefs.h>
#include <config.h>
#include <graphics.h>
#include <services.h>
#include <widget_base.h>

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

/* amount of space in pixels to add around scrollbar sliders */
#define ENDSPACE 6
#define ENDLEDSPC 10 /* space at start and end of text edit box */
/* user defined messages */
#define WMC_LGTFOC ami_etwidget+0 /* widget message code: light up focus */
#define WMC_DRKFOC ami_etwidget+1 /* widget message code: turn off focus */
#define TABHGT 1.67 /* tab strip depth * char size y, as Breeze has it */
/* A list row is the font and a third, which is the proportion Breeze keeps
   between its row height and its text. The sizing call, the drawing and the
   hit test all measure rows with this, and so cannot fall out of step. */
/* A list row is the font and a third, and a list keeps half a line of
   margin above its first row. Both are whole pixels: a box is sized by
   the one and drawn by the other, and when the row was a fraction the
   size, truncated to a pixel, came out a fraction short of the rows
   drawn against it, and a box made for three entries showed two. */
#define LSTROW(f) ((ami_long)(ami_chrsizy(f)*1.35))
#define LSTMARGIN(f) (ami_chrsizy(f)/2)

/* macro to make a color from RGB values */
#define RGB(r, g, b) (r<<16|g<<8|b)

/* macro to make a black and white value */
#define BW(v) (v<<16|v<<8|v)

/* unpack RGB packed values */
#define REDP(v)   (v >> 16 & 0xff)
#define GREENP(v) (v >> 8 & 0xff)
#define BLUEP(v)  (v & 0xff)

/* macros to unpack color table entries to LONG_MAX ratioed numbers */
/* Unpack a byte color component to the API full scale. Dividing by 255,
   not 256, so a component of 0xff gives exactly LONG_MAX: the divide is
   first, so the multiply cannot overflow */
#define RED(v)   (LONG_MAX/255*REDP(v))   /* red */
#define GREEN(v) (LONG_MAX/255*GREENP(v)) /* green */
#define BLUE(v)  (LONG_MAX/255*BLUEP(v)) /* blue */

/* default values for color table. Note these can be overridden.
 * To increase or decrease luminescence, add or subtract a BW() value from
 * another color or B&W value, but be careful not to overflow.
 */
#define TD_BACKPRESSED        RGB(194,224,245)    /* button background pressed */
#define TD_BACK               RGB(252,252,252)    /* button background not pressed */
#define TD_BACKHOVER          RGB(236,246,252)    /* button background for hover */
#define TD_OUTLINE1           RGB(182,185,187)    /* button outline */
#define TD_TEXT               RGB(35,38,41)       /* widget face text */
#define TD_TEXTDIS            RGB(168,169,170)    /* widget face text disabled */
#define TD_FOCUS              RGB(61,174,233)     /* widget focused outline */
#define TD_CHKRAD             RGB(61,174,233)     /* checkbox/radio button selected */
#define TD_CHKRADOUT          RGB(127,140,141)    /* checkbox/radio button outline */
#define TD_SCROLLBACK         RGB(239,240,241)    /* scrollbar background */
#define TD_SCROLLBAR          RGB(159,161,163)    /* scrollbar not pressed */
#define TD_SCROLLBARPRESSED   RGB(61,174,233)     /* scrollbar pressed */
#define TD_NUMSELDIV          RGB(220,222,224)    /* numselbox divider */
#define TD_NUMSELUD           RGB(119,123,126)    /* numselbox up/down figures */
#define TD_TEXTERR            RGB(218,68,83)      /* widget face text in error */
#define TD_PROGINACEN         RGB(214,216,218)    /* progress bar inactive center */
#define TD_PROGINAEDG         RGB(200,202,204)    /* progress bar inactive edge */
#define TD_PROGACTCEN         RGB(61,174,233)     /* progress bar active center */
#define TD_PROGACTEDG         RGB(52,148,199)     /* progress bar active edge */
#define TD_LSTHOV             RGB(214,236,248)    /* list background for hover */
#define TD_LSTSEL             RGB(168,214,240)    /* list background for selection */
#define TD_OUTLINE2           RGB(182,185,187)    /* numselbox, dropbox outline */
#define TD_DROPARROW          RGB(35,38,41)       /* dropbox arrow */
#define TD_DROPTEXT           BW(0)               /* dropbox text */
#define TD_SLDINT             RGB(217,219,221)    /* slider track internal */
#define TD_TABDIS             RGB(112,116,120)    /* tab unselected text */
#define TD_TABBACK            RGB(239,240,241)    /* tab background */
#define TD_TABSEL             RGB(61,174,233)     /* tab selected underbar */
#define TD_TABFOCUS           (TD_TABSEL+BW(20))  /* tab focus box */
#define TD_CANCELBACKFOCUS    RGB(252,252,252)    /* Cancel background in focus */
#define TD_CANCELTEXTFOCUS    RGB(35,38,41)       /* Cancel text in focus */
#define TD_CANCELOUTLINE      RGB(182,185,187)    /* Cancel outline unfocused */
#define TD_SELECTBACKFOCUS    RGB(61,174,233)     /* Select background in focus */
#define TD_SELECTBACK         RGB(61,174,233)     /* Select background in normal */
#define TD_SELECTTEXTFOCUS    BW(255)             /* Select text in focus */
#define TD_SELECTTEXT         RGB(252,252,252)    /* Select text in focus */
#define TD_SELECTOUTLINE      RGB(41,128,185)     /* Select outline unfocused */
#define TD_SELECTOUTLINEFOCUS RGB(41,128,185)     /* Select outline focused */
#define TD_PLUSBACKFOCUS      RGB(252,252,252)    /* Select background in focus */
#define TD_PLUSBACK           RGB(252,252,252)    /* Select background in normal */
#define TD_PLUSTEXTFOCUS      BW(61)              /* Select text in focus */
#define TD_PLUSTEXT           BW(61)              /* Select text in focus */
#define TD_PLUSOUTLINE        RGB(182,185,187)    /* Select outline unfocused */
#define TD_PLUSOUTLINEFOCUS   RGB(182,185,187)    /* Select outline focused */
#define TD_TITLE              RGB(239,240,241)    /* dialog titlebar: Breeze window */

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

/* values table ids */

typedef enum {

    th_backpressed,        /* button background when pressed */
    th_back,               /* button background when not pressed */
    th_backhover,          /* button background when hovered */
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
    th_lstsel,             /* list background for selection */
    th_outline2,           /* numselbox, dropbox outline */
    th_droparrow,          /* dropbox arrow */
    th_droptext,           /* dropbox text */
    th_sldint,             /* slider track internal */
    th_tabdis,             /* tab unselected text */
    th_tabback,            /* tab background */
    th_tabsel,             /* tab selected panel */
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

    /** Button background normal */           ami_ulong bbn;
    /** Button background pressed */          ami_ulong bbp;
    /** Button outline normal */              ami_ulong bon;
    /** Button outline focus */               ami_ulong bof;
    /** Button text normal */                 ami_ulong btn;
    /** Button text disabled */               ami_ulong btd;
    /** Surface behind the rounded corners */ ami_ulong bsr;

} ccolor;

/* Widget control structure. The base head lays down the fields every
   package's record shares (widget_base.h); this package's own follow. */
typedef struct wigrec* wigptr;
typedef struct wigrec {

    WB_WIGHEAD(wigptr) /* the widget base head, first */
    /** type of widget */                     wigtyp    typ;
    /** creation order stamp */               ami_long  seq;
    /** in the pressed state */               ami_long  pressed;
    /** last pressed state */                 ami_long  lpressed;
    /** the current on/off state */           ami_long  select;
    /** face text */                          char*     face;
    /** scrollbar size in LONG_MAX ratio */   ami_long  sclsiz;
    /** scrollbar position in LONG_MAX ratio */ ami_long    sclpos;
    /** mouse tracking in widget */           ami_long  mpx, mpy;
    /** last mouse position */                ami_long  lmpx, lmpy;
    /** text cursor */                        ami_long  curs;
    /** text left side index */               ami_long  tleft;
    /** insert/overwrite mode */              ami_long  ins;
    /** allow only numeric entry */           ami_long  num;
    /** low bound of number */                ami_long  lbnd;
    /** upper bound of number */              ami_long  ubnd;
    /** child/subclassed widget */            wigptr    cw;
    /** child/subclassed widget 2 */          wigptr    cw2;
    /** parent widget */                      wigptr    pw;
    /** parent file (used to send subclass
       messages) */                           FILE*     pf;
    /** up button pressed */                  ami_long  uppress;
    /** down buton pressed */                 ami_long  downpress;
    /** progress bar position */              ami_long  ppos;
    /** string list */                        ami_strptr strlst;
    /** string selected, 0 if none */         ami_long  ss;
    /** string hovered, 0 if none */          ami_long  sh;
    /** list selection held, 0 if none */     ami_long  lsel;
    /** list box: first entry shown, 1 based */ ami_long    top;
    /** List box column stops, in pixels from the left of the box. An
       entry is split at tabs and each field placed at its stop; a
       negative stop right aligns the field to it, as a column of sizes
       reads. None means the entry is one string, as it always was. */
                                              ami_long  tabs[4];
    /** how many stops */                     ami_long  ntabs;
    /** child window id */                    ami_long  cid;
    /** mouse grabs scrollbar/slider */       ami_long  grab;
    /** tick marks on slider */               ami_long  ticks;
    /** Tab orientation */                    ami_tabori tor;
    /** Character based */                    ami_long  charb;

    /** Configurable button fields */         ccolorp   cbc;
    /** use check/text */                     ami_long  check;

} wigrec;

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
static wbpkg         pkg;                /* this package's widget base
                                            instance */
static FILE*         win0;               /* "window zero" dummy window */
/* table of colors or other theme values */
static ami_ulong themetable[th_endmarker];


/** ****************************************************************************

The desktop's own font

Plasma keeps its interface font in kdeglobals, as

    [General]
    font=Noto Sans,10,-1,5,50,0,0,0,0,0

the family first and the point size second. The set draws its faces in that
family, so a page of widgets is lettered like the rest of the desktop.

Where the file says nothing -- and it often says nothing, Plasma writing the
line only once the user has changed it -- the family falls back to the Plasma
default.

The size is not taken. kdeglobals states it in points and fontsiz() takes
pixels, and the conversion wants a dots per inch this code has no honest way
to ask for: on a scaled display the answer is not the panel's. Taking the
number as it stands would set ten pixel text on any desktop that states a
size, which is worse than leaving the window's own size alone. The faces keep
the size the window has and change only their family.

The font is looked up by name in the display's list, whose entries read
"vendor: family: encoding". The family is the middle field, compared without
regard to case. Nothing found leaves the generic sans font, which is what the
set used before it asked.

*******************************************************************************/

#define PLASMA_DEFAULT_FONT "Noto Sans"

static ami_long wigfont;     /* the font the faces are drawn in */

/* the family the desktop asks for, TRUE if the file said */
static int deskfontname(char* fam, int faml)

{

    FILE*       fp = NULL;
    char        path[512], line[512];
    const char* home;
    int         ingrp = FALSE;
    int         got = FALSE;

    home = getenv("XDG_CONFIG_HOME");
    if (home && home[0]) snprintf(path, sizeof(path), "%s/kdeglobals", home);
    else {

        home = getenv("HOME");
        if (home) snprintf(path, sizeof(path), "%s/.config/kdeglobals", home);
        else path[0] = 0;

    }
    if (path[0]) fp = fopen(path, "r");
    if (!fp) return (FALSE);
    while (fgets(line, sizeof(line), fp)) {

        if (line[0] == '[') { ingrp = !strncmp(line, "[General]", 9); continue; }
        if (!ingrp || strncmp(line, "font=", 5)) continue;
        { /* family,size,... */

            char* p = line+5;
            char* c = strchr(p, ',');
            int   n;

            if (c) *c = 0;
            n = strlen(p);
            while (n && (p[n-1] == '\n' || p[n-1] == ' ')) p[--n] = 0;
            if (n) { snprintf(fam, faml, "%s", p); got = TRUE; }

        }
        break;

    }
    fclose(fp);

    return (got);

}

/* the middle field of a font list entry, "vendor: family: encoding" */
static void famof(const char* nm, char* fam, int faml)

{

    const char* a = strchr(nm, ':');
    const char* b;
    int         n;

    if (!a) { snprintf(fam, faml, "%s", nm); return; }
    a++;
    while (*a == ' ') a++;
    b = strchr(a, ':');
    n = b? (int)(b-a): (int)strlen(a);
    while (n && a[n-1] == ' ') n--;
    if (n > faml-1) n = faml-1;
    memcpy(fam, a, n);
    fam[n] = 0;

}

/* find the desktop's font in the display's list, or leave the generic sans */
static void findfont(void)

{

    char fam[128], want[128], nm[256];
    ami_long i, n;

    wigfont = AMI_FONT_SIGN;
    snprintf(want, sizeof(want), "%s", PLASMA_DEFAULT_FONT);
    deskfontname(want, sizeof(want));
    n = ami_fonts(win0);
    for (i = 1; i <= n; i++) {

        ami_fontnam(win0, i, nm, sizeof(nm));
        famof(nm, fam, sizeof(fam));
        if (!strcasecmp(fam, want)) { wigfont = i; return; }

    }

}

/* Set the face font on a widget's window. The lookup happens the first time
   a face is drawn, not in the package's constructor: the display's font list
   is not ready that early, and asking for it there takes the program down
   before main() is reached. */

static void setwigfont(FILE* f)

{

    static int found = FALSE;

    if (!found) { findfont(); found = TRUE; }
    ami_font(f, wigfont);

}

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

/** ****************************************************************************

Copy string to critical buffer

Copies a string to a "critical" output buffer of the given length. If the
string is longer than the buffer, an error results. If the string exactly
fills the buffer, the terminating zero is left off. Otherwise, the result is
zero terminated.

*******************************************************************************/

static void cpycrit(
    /** Destination buffer */           char*       d,
    /** Length of destination buffer */ ami_long    dl,
    /** Source string */                const char* s
)

{

    ami_long l; /* length of source string */

    l = strlen(s); /* find length of source */
    if (l > dl) error("String too large for result buffer");
    memcpy(d, s, l); /* copy string into place */
    if (l < dl) d[l] = 0; /* zero terminate if buffer not entirely filled */

}

/** ****************************************************************************

Get widget text with guaranteed zero termination

Internal helper. ami_getwidgettext() fills a critical buffer, meaning the
terminating zero is left off when the text exactly fills the buffer. Internal
callers that parse the result need a proper C string, so this reserves one
byte of the buffer for the terminator and guarantees termination.

*******************************************************************************/

static void getwidgettextz(
    /** Window file */             FILE* f,
    /** Logical widget id */       ami_long  id,
    /** Output buffer for text */  char* s,
    /** Size of output buffer */   ami_long  sl
)

{

    ami_getwidgettext(f, id, s, sl-1); /* get text, reserve terminator byte */
    s[sl-1] = 0; /* guarantee termination if text exactly filled buffer */

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
        case ami_etresize: fprintf(stderr, ": x: %lld y: %lld xg: %lld yg: %lld",
                                  AMI_LONG_CAST(er->rszx), AMI_LONG_CAST(er->rszy),
                                  AMI_LONG_CAST(er->rszxg), AMI_LONG_CAST(er->rszyg)); break;
        case ami_etfun: fprintf(stderr, ": key: %lld", AMI_LONG_CAST(er->fkey)); break;
        case ami_etmoumovg: fprintf(stderr, ": mouse: %lld x: %4lld y: %4lld",
                                   AMI_LONG_CAST(er->mmoung), AMI_LONG_CAST(er->moupxg), AMI_LONG_CAST(er->moupyg)); break;
        case ami_etredraw: fprintf(stderr, ": sx: %4lld sy: %4lld ex: %4lld ey: %4lld",
                                  AMI_LONG_CAST(er->rsx), AMI_LONG_CAST(er->rsy), AMI_LONG_CAST(er->rex), AMI_LONG_CAST(er->rey)); break;
        case ami_etmenus: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->menuid)); break;
        case ami_etbutton: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->butid)); break;
        case ami_etchkbox: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->ckbxid)); break;
        case ami_etradbut: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->radbid)); break;
        case ami_etsclull: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->sclulid)); break;
        case ami_etscldrl: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->scldrid)); break;
        case ami_etsclulp: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->sclupid)); break;
        case ami_etscldrp: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->scldpid)); break;
        case ami_etsclpos: fprintf(stderr, ": id: %lld position: %lld",
                                  AMI_LONG_CAST(er->sclpid), AMI_LONG_CAST(er->sclpos)); break;
        case ami_etedtbox: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->edtbid)); break;
        case ami_etnumbox: fprintf(stderr, ": id: %lld number: %lld",
                                  AMI_LONG_CAST(er->numbid), AMI_LONG_CAST(er->numbsl)); break;
        case ami_etlstbox: fprintf(stderr, ": id: %lld select: %lld",
                                  AMI_LONG_CAST(er->lstbid), AMI_LONG_CAST(er->lstbsl)); break;
        case ami_etdrpbox: fprintf(stderr, ": id: %lld select: %lld",
                                  AMI_LONG_CAST(er->drpbid), AMI_LONG_CAST(er->drpbsl)); break;
        case ami_etdrebox: fprintf(stderr, ": id: %lld", AMI_LONG_CAST(er->drebid)); break;
        case ami_etsldpos: fprintf(stderr, ": id: %lld postion: %lld",
                                  AMI_LONG_CAST(er->sldpid), AMI_LONG_CAST(er->sldpos)); break;
        case ami_ettabbar: fprintf(stderr, ": id: %lld select: %lld",
                                  AMI_LONG_CAST(er->tabid), AMI_LONG_CAST(er->tabsel)); break;
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

Get widget

Get a widget and place into the window tracking list. If a free widget entry
is available, that will be used, otherwise a new entry is allocated.

\returns Pointer to new widget.

*******************************************************************************/

static wigptr getwig(void)

{

    return ((wigptr)wb_getwig(&pkg)); /* base allocates, wiginit fills */

}

/* base callback: initialize this package's fields of a fresh record;
   the base has initialized the head */
static void wiginit(void* vwg)

{

    wigptr wp = (wigptr)vwg;

    wp->pressed = FALSE; /* set not pressed */
    wp->lpressed = FALSE;
    wp->select = FALSE; /* set not selected */
    wp->face = NULL; /* no face yet */
    wp->sclpos = 0; /* set scrollbar position top/left */
    wp->curs = 0; /* set text cursor */
    wp->tleft = 0; /* set text left side in edit box */
    wp->ins = 0; /* set insert mode */
    wp->mpx = 0; /* clear mouse position */
    wp->mpy = 0;
    wp->lmpx = 0;
    wp->lmpy = 0;
    wp->num = FALSE; /* set any character entry */
    wp->lbnd = -LONG_MAX; /* set low bound */
    wp->ubnd = LONG_MAX; /* set high bound */
    wp->cw = NULL; /* clear children */
    wp->cw2 = NULL;
    wp->pw = NULL; /* clear parent */
    wp->uppress = FALSE; /* set up not pressed */
    wp->downpress = FALSE; /* set down not pressed */
    wp->ppos = 0; /* progress bar extreme left */
    wp->strlst = NULL; /* clear string list */
    wp->ss = 0; /* no string selected */
    wp->sh = 0; /* no string hovered */
    wp->lsel = 0; /* no list selection held */
    wp->top = 1; /* the list starts at its first entry */
    wp->ntabs = 0; /* no columns: the entry is one string */
    wp->cid = 0; /* clear child id */
    wp->grab = FALSE; /* set no scrollbar/slider grab */
    wp->ticks = 0; /* set no tick marks on slider */
    wp->tor = ami_totop; /* set tab orientation top */
    wp->charb = FALSE; /* widget based on character grid */
    wp->check = FALSE; /* do not use check instead of text */

}

/** ****************************************************************************

Put widget

Removes the widget from the window list, and releases the widget entry to free
list.

*******************************************************************************/

static void wigfree(void* vwg)

{

    wigptr wp = (wigptr)vwg;

    /* if not a subclass widget, free string list */
    if (!wp->pw) frestrlst(wp->strlst);
    if (wp->face) free(wp->face); /* free face string if exists */

}

/** ****************************************************************************

Find widget

Given a file specification and a widget id, returns a pointer to the given
widget. Validates the file and the widget number.

\returns Pointer to found widget.

*******************************************************************************/

static wigptr fndwig(
    /** Window file pointer */ FILE* f,
    /** Logical wiget id */    ami_long id
)

{

    return ((wigptr)wb_fndwig(&pkg, f, id));

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

    wp->live = TRUE; /* whole from here: every creator ends with its
                        first-paint request, and reconfigures keep it */
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
    /** 32 bit packed color */ ami_ulong c
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
    /** 32 bit packed color */ ami_ulong c
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

static ami_long digits(
    /** Value to measure */ ami_long v
)

{

    ami_long p; /* power */
    ami_long c; /* count */

    p = 1; /* set first power */
    c = 1; /* set initial count (at least one digit) */
    while (p < LONG_MAX/10 && p < v) { /* will not overflow */

        p *= 10; /* advance power */
        c++; /* count digits */

    }

    return (c); /* return digits */

}

/** ****************************************************************************

Kill widget

Kills the given widget by id and in the window file by file id.

*******************************************************************************/

static void wigkill(void* vwg)

{

    wigptr wp = (wigptr)vwg;

    /* if there is a subwidget, kill that as well */
    if (wp->cw) ami_killwidget(wp->cw->pw->wf, wp->cw->id);
    if (wp->cw2) ami_killwidget(wp->cw2->pw->wf, wp->cw2->id);

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

static ami_long wigseq = 0; /* widget creation sequence */

/* a component is a widget made to be layered under others: it presents a
   surface, not a control */
static ami_long component(wigtyp typ)

{

    return (typ == wtgroup || typ == wtbackground || typ == wtprogbar);

}

static void widget(
    /** Parent window file */              FILE* f,
    /** Containing rectangle for widget */ ami_long x1, ami_long y1, ami_long x2, ami_long y2,
    /** Face string (if exists) */         char* s,
    /** logical id for widget */           ami_long id,
    /** type code for widget */            wigtyp typ,
    /** Widget I/O pointer */              wigptr* wpr
)

{

    wigptr wp;
    wigptr sp; /* sibling walk */
    wigptr np; /* next control to raise */
    ami_long   last; /* last sequence raised */
    int    i;

    wp = *wpr; /* get any predefined widget entry */
    /* the base opens, places and registers the widget subwindow */
    wb_widget(&pkg, f, x1, y1, x2, y2, id, &wp);
    wp->seq = ++wigseq; /* stamp creation order */
    wp->face = str(s); /* place face */
    setwigfont(wp->wf); /* set sign font */
    wp->typ = typ; /* place type */
    wp->sclsiz = LONG_MAX/10; /* set default size scrollbar */
    /* Implicit Z ordering: a component goes behind the controls of its
       window. It is not sent to the back, which would bury a group made
       inside a group; instead the sibling controls are raised over it in
       their creation order, which keeps their own stacking intact. */
    if (component(typ)) {

        last = 0;
        do {

            np = NULL;
            for (i = 0; i < MAXWIG*2+1; i++) {

                sp = (wigptr)pkg.opnfil[fileno(f)]->widgets[i];
                if (sp && sp != wp && !component(sp->typ) &&
                    sp->seq > last && (!np || sp->seq < np->seq)) np = sp;

            }
            if (np) { ami_front(np->wf); last = np->seq; }

        } while (np);

    }
    /* The first paint is requested here, covering every creation, the
       dialogs' direct subclasses included; it also takes the widget
       live, ending the construction gap the event gate holds shut.
       Creators that set parameters after this request their own
       completing redraw, which repaints over the default face. */
    widget_redraw(wp);

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

    ami_long sq; /* size of checkbox square */
    ami_long sqm; /* center x of checkbox square */
    ami_long md; /* checkbox center line */
    ami_long cb; /* bounding box of check figure */

    /* the surround first: the square behind the rounded corners shows the
       surface the button sits on, not the window's blank canvas */
    fcolorp(wg->wf, wg->cbc->bsr);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* color the background */
    if (wg->pressed) fcolorp(wg->wf, wg->cbc->bbp);
    else fcolorp(wg->wf, wg->cbc->bbn);
    ami_frrect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf), 6, 6);
    /* outline */
    ami_linewidth(wg->wf, 3);
    /* a disabled widget does not show the focus ring; see button_draw */
    if (wg->focus && wg->enb) fcolorp(wg->wf, wg->cbc->bof);
    else fcolorp(wg->wf, wg->cbc->bon);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 6, 6);
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

    /* color the background; a selected button keeps the pressed in look,
       giving a persistent active state for modal buttons. Hover shows on
       the outline, the Breeze way, not the face */
    if (wg->pressed || wg->select) fcolort(wg->wf, th_backpressed);
    else fcolort(wg->wf, th_back);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf),
              ami_maxyg(wg->wf));
    /* Outline: the Breeze blue on focus or hover. A disabled widget shows
       neither: it cannot be operated, so advertising focus on it misleads
       (windows denies focus to disabled widgets outright) */
    ami_linewidth(wg->wf, 4);
    if ((wg->focus || wg->hover) && wg->enb) fcolort(wg->wf, th_focus);
    else fcolort(wg->wf, th_outline1);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1,
             ami_maxyg(wg->wf)-1, 6, 6);
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

    } else if (ev->etype == ami_ethover) {

        wg->hover = 1; /* hovered, light up */
        button_draw(wg); /* redraw the window */

    } else if (ev->etype == ami_etnohover) {

        wg->hover = 0; /* not hovered */
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

    ami_long sq; /* size of checkbox square */
    ami_long sqm; /* center x of checkbox square */
    ami_long sqo; /* checkbox offset left */
    ami_long md; /* checkbox center line */
    ami_long cb; /* bounding box of check figure */

    /* color the background */
    ami_fcolor(wg->wf, ami_backcolor);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* outline */
    ami_linewidth(wg->wf, 4);
    if (wg->focus) {

        fcolort(wg->wf, th_focus);
        ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1,
                 ami_maxyg(wg->wf)-1, 6, 6);

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
        ami_frrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 4, 4);
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
        ami_frrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 4, 4);
        ami_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_chkradout);
        ami_rrect(wg->wf, sqo, md-sq/2, sqo+sq, md+sq/2, 4, 4);

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

    ami_long cr; /* size of radiobutton circle */
    ami_long crm; /* center x of radiobutton circle */
    ami_long cro; /* radiobutton offset left */
    ami_long md; /* radiobutton center line */

    /* color the background */
    ami_fcolor(wg->wf, ami_backcolor);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* outline */
    ami_linewidth(wg->wf, 4);
    if (wg->focus) {

        fcolort(wg->wf, th_focus);
        ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1,
                 ami_maxyg(wg->wf)-1, 6, 6);

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

        /* the Breeze figure: a blue ring holding a blue dot */
        ami_fcolor(wg->wf, ami_white);
        ami_fellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);
        ami_linewidth(wg->wf, 3);
        fcolort(wg->wf, th_chkrad);
        ami_ellipse(wg->wf, cro, md-cr/2, cro+cr, md+cr/2);
        ami_fellipse(wg->wf, crm-cr/5, md-cr/5, crm+cr/5, md+cr/5);

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

    ami_long  sclsizp; /* size of slider in pixels */
    ami_long  sclposp; /* offset of slider in pixels */
    ami_long  remsizp; /* remaining space after slider in pixels */
    ami_long  totsizp; /* total size of slider space after padding */
    ami_long  botposp; /* bottom position of slider */
    ami_long  inbar;   /* mouse is in scroll bar */
    ami_long  sclpos;  /* new scrollbar position */
    ami_evtrec er;      /* outbound button event */
    ami_long  y;

    /* find net total slider space */
    totsizp = ami_maxyg(wg->wf)-ENDSPACE-ENDSPACE;
    /* find size of slider in pixels */
    sclsizp = round((double)totsizp*wg->sclsiz/LONG_MAX);
    /* find remaining size after slider */
    remsizp = totsizp-sclsizp;
    /* find position of top of slider in pixels offset */
    sclposp = round((double)remsizp*wg->sclpos/LONG_MAX);
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
            sclpos = y >= remsizp? LONG_MAX: LONG_MAX/remsizp*y;
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
    /* the Breeze thumb: a slim pill riding the track center, blue under
       the pointer or drag */
    if ((wg->pressed && (inbar || wg->grab)) || (wg->hover && inbar))
        fcolort(wg->wf, th_scrollbarpressed);
    else
        fcolort(wg->wf, th_scrollbar);
    ami_frrect(wg->wf, ami_maxxg(wg->wf)*0.30, ENDSPACE+sclposp,
              ami_maxxg(wg->wf)*0.70, ENDSPACE+sclposp+sclsizp,
              ami_maxxg(wg->wf)*0.20, ami_maxxg(wg->wf)*0.20);

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

    ami_long  sclpos;  /* new scrollbar position */
    ami_long  sclsizp; /* size of slider in pixels */
    ami_long  remsizp; /* remaining space after slider in pixels */
    ami_long  totsizp; /* total size of slider space after padding */
    ami_evtrec er;      /* outbound button event */
    ami_long  y;

    if (ev->etype == ami_etredraw) scrollvert_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        /* find net total slider space */
        totsizp = ami_maxyg(wg->wf)-ENDSPACE-ENDSPACE;
        /* find size of slider in pixels */
        sclsizp = round((double)totsizp*wg->sclsiz/LONG_MAX);
        /* find remaining size after slider */
        remsizp = totsizp-sclsizp;
        /* find new top for click */
        y = wg->mpy-sclsizp/2;
        if (y < ENDSPACE) y = ENDSPACE; /* limit top travel */
        else if (y+sclsizp > ami_maxyg(wg->wf)-ENDSPACE)
            y = ami_maxyg(wg->wf)-sclsizp-ENDSPACE;
        /* find new ratioed position */
        sclpos = y-ENDSPACE >= remsizp? LONG_MAX: LONG_MAX/remsizp*(y-ENDSPACE);
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
        /* only repaint while actually dragging the thumb; a plain hover does
           not change the bar's appearance, and repainting the (unbuffered)
           widget on every motion event causes flashing and sluggishness */
        if (wg->pressed) scrollvert_draw(wg);
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

    ami_long  sclsizp; /* size of slider in pixels */
    ami_long  sclposp; /* offset of slider in pixels */
    ami_long  remsizp; /* remaining space after slider in pixels */
    ami_long  totsizp; /* total size of slider space after padding */
    ami_long  botposp; /* bottom position of slider */
    ami_long  inbar;   /* mouse is in scroll bar */
    ami_long  sclpos;  /* new scrollbar position */
    ami_evtrec er;      /* outbound event */
    ami_long  x;

    /* find net total slider space */
    totsizp = ami_maxxg(wg->wf)-ENDSPACE-ENDSPACE;
    /* find size of slider in pixels */
    sclsizp = round((double)totsizp*wg->sclsiz/LONG_MAX);
    /* find remaining size after slider */
    remsizp = totsizp-sclsizp;
    /* find position of top of slider in pixels offset */
    sclposp = round((double)remsizp*wg->sclpos/LONG_MAX);
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
        sclpos = x >= remsizp? LONG_MAX: LONG_MAX/remsizp*x;
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
    /* the Breeze thumb: a slim pill riding the track center, blue under
       the pointer or drag */
    if ((wg->pressed && (inbar || wg->grab)) || (wg->hover && inbar))
        fcolort(wg->wf, th_scrollbarpressed);
    else
        fcolort(wg->wf, th_scrollbar);
    ami_frrect(wg->wf, ENDSPACE+sclposp, ami_maxyg(wg->wf)*0.30,
              ENDSPACE+sclposp+sclsizp, ami_maxyg(wg->wf)*0.70,
              ami_maxyg(wg->wf)*0.20, ami_maxyg(wg->wf)*0.20);

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

    ami_long  sclpos;  /* new scrollbar position */
    ami_long  sclsizp; /* size of slider in pixels */
    ami_long  remsizp; /* remaining space after slider in pixels */
    ami_long  totsizp; /* total size of slider space after padding */
    ami_evtrec er;      /* outbound button event */
    ami_long  x;

    if (ev->etype == ami_etredraw) scrollhoriz_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        wg->lpressed = wg->pressed; /* save last pressed state */
        wg->pressed = TRUE; /* set is pressed */
        totsizp = ami_maxxg(wg->wf)-ENDSPACE-ENDSPACE; /* find net total slider space */
        /* find size of slider in pixels */
        sclsizp = round((double)totsizp*wg->sclsiz/LONG_MAX);
        /* find remaining size after slider */
        remsizp = totsizp-sclsizp;
        /* find new top for click */
        x = wg->mpx-sclsizp/2;
        if (x < ENDSPACE) x = ENDSPACE; /* limit left travel */
        else if (x+sclsizp > ami_maxxg(wg->wf)-ENDSPACE)
            x = ami_maxxg(wg->wf)-sclsizp-ENDSPACE;
        /* find new ratioed position */
        sclpos = x-ENDSPACE >= remsizp? LONG_MAX: LONG_MAX/remsizp*(x-ENDSPACE);
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
        /* only repaint while actually dragging the thumb (see scrollvert) */
        if (wg->pressed) scrollhoriz_draw(wg);
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
    /* Breeze's group is a rounded panel with its name centred inside the
       top of it, not a square rule with the name sitting on the corner. */
    fcolort(wg->wf, th_outline1);
    ami_linewidth(wg->wf, 2);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 6, 6);
    ami_fcolor(wg->wf, ami_black);
    ami_bover(wg->wf);
    ami_bcolor(wg->wf, ami_backcolor);
    ami_cursorg(wg->wf,
               (ami_maxxg(wg->wf)-ami_strsiz(wg->wf, wg->face))/2,
               ami_chrsizy(wg->wf)*0.35);
    fprintf(wg->wf, "%s", wg->face); /* place the group's name */


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

    ami_long  cl;
    ami_long  x;
    char* s;
    ami_long  err;
    ami_long  v;

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
                 ami_maxyg(wg->wf)-1, 6, 6);

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
    while (*s && ami_curxg(wg->wf)+ami_chrpos(wg->wf, s, 1) <=
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
    ami_long  l;    /* length */
    ami_long  span; /* span between characters */
    ami_long  off;  /* offset from last character */
    ami_evtrec er;   /* outbound button event */
    ami_long  i;

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

    if (!wg->cw) return; /* not yet wired: the completing redraw paints */

    ami_long udspc; /* up/down control space */
    ami_long figsiz; /* size of up/down figures */

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */
    /* color the background */
    ami_fcolor(wg->wf, ami_white);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* The two controls stand one above the other at the right end, as
       chevrons with no cell drawn around them and no line dividing them
       from the number: Breeze gives them no furniture, and the press
       shows as a wash over the half that was hit. */
    if (wg->downpress || wg->uppress) {

        fcolort(wg->wf, th_backpressed);
        if (wg->uppress)
            ami_frect(wg->wf, ami_maxxg(wg->wf)-udspc, 1,
                             ami_maxxg(wg->wf), ami_maxyg(wg->wf)/2);
        else
            ami_frect(wg->wf, ami_maxxg(wg->wf)-udspc, ami_maxyg(wg->wf)/2,
                             ami_maxxg(wg->wf), ami_maxyg(wg->wf));

    }
    {

        ami_long aw = ami_chrsizy(win0)*0.45; /* the chevrons' span */
        ami_long ah = ami_chrsizy(win0)*0.20; /* and their depth */
        ami_long cx = ami_maxxg(wg->wf)-udspc*0.5;
        ami_long uy = ami_maxyg(wg->wf)*0.30;  /* the up one */
        ami_long dy = ami_maxyg(wg->wf)*0.70;  /* and the down one */

        figsiz = 0; /* the figures are the chevrons now */
        fcolort(wg->wf, th_numselud);
        ami_linewidth(wg->wf, ami_chrsizy(win0)*0.09);
        ami_line(wg->wf, cx-aw*0.5, uy+ah*0.5, cx, uy-ah*0.5);
        ami_line(wg->wf, cx, uy-ah*0.5, cx+aw*0.5, uy+ah*0.5);
        ami_line(wg->wf, cx-aw*0.5, dy-ah*0.5, cx, dy+ah*0.5);
        ami_line(wg->wf, cx, dy+ah*0.5, cx+aw*0.5, dy-ah*0.5);

    }
    /* outline */
    if (wg->focus | wg->cw->focus) {

        ami_linewidth(wg->wf, 4);
        fcolort(wg->wf, th_focus);

    } else {

        ami_linewidth(wg->wf, 2);
        fcolort(wg->wf, th_outline1);

    }
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 6, 6);

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

    if (!wg->cw) return; /* not yet wired: the completing redraw paints */

    ami_long udspc;    /* up/down control space */
    char buff[40]; /* buffer for number entered (holds full long) */
    ami_evtrec er;  /* outbound button event */
    ami_long v;

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
            er.numbsl = atol(wg->cw->face); /* set value */
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

                    if (wg->mpx >= ami_maxxg(wg->wf)-udspc &&
                        wg->mpy >= ami_maxyg(wg->wf)/2) {

                        /* down control: the lower chevron */
                        getwidgettextz(wg->wf, wg->cw->id, buff, sizeof(buff));
                        v = atol(buff);
                        if (wg->cw->lbnd < v && v <= wg->cw->ubnd) v--;
                        sprintf(buff, "%lld", AMI_LONG_CAST(v));
                        ami_putwidgettext(wg->wf, wg->cw->id, buff);
                        if (wg->cw->curs > strlen(wg->cw->face))
                            wg->cw->curs = strlen(wg->cw->face);
                        editbox_draw(wg->cw);
                        wg->downpress = TRUE; /* set down pressed */
                        numselbox_draw(wg); /* redraw */

                    } else if (wg->mpx >= ami_maxxg(wg->wf)-udspc) {

                        /* up control: the upper chevron */
                        getwidgettextz(wg->wf, wg->cw->id, buff, sizeof(buff));
                        v = atol(buff);
                        if (wg->cw->lbnd <= v && v < wg->cw->ubnd) v++;
                        sprintf(buff, "%lld", AMI_LONG_CAST(v));
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

    ami_long pbpp; /* prog bar pixel position right side */

    /* draw inactive background */
    fcolort(wg->wf, th_proginacen);
    ami_linewidth(wg->wf, 2);
    ami_frrect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf), 4, 4);
    /* draw inactive edget */
    fcolort(wg->wf, th_proginaedg);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 4, 4);
    /* find right side of prog bar */
    pbpp = round((double)wg->ppos*ami_maxxg(wg->wf)/LONG_MAX);
    /* now draw active */
    fcolort(wg->wf, th_progactcen);
    ami_linewidth(wg->wf, 2);
    ami_frrect(wg->wf, 1, 1, pbpp, ami_maxyg(wg->wf), 4, 4);
    /* draw inactive edget */
    fcolort(wg->wf, th_progactedg);
    ami_rrect(wg->wf, 2, 2,pbpp-1, ami_maxyg(wg->wf)-1, 4, 4);

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

/* Paint a single list line (1-based index idx, string sp, top at y): hover-grey
   background when it is the hovered line, list-white otherwise, then the text.
   The fill is inset horizontally so the rounded outline is never overpainted --
   that lets the motion handler repaint just the line that gains or loses the
   hover highlight instead of redrawing (and re-rendering) the whole list. */
/* the entries a list box has room for */
static ami_long listbox_vis(wigptr wg)

{

    ami_long n = (ami_maxyg(wg->wf)-LSTMARGIN(wg->wf))/LSTROW(wg->wf);

    return (n < 1? 1: n);

}

/* the entries it holds */
static ami_long listbox_cnt(wigptr wg)

{

    ami_strptr sp = wg->strlst;
    ami_long n = 0;

    while (sp) { n++; sp = sp->next; }

    return (n);

}

/* Hold the view inside the list: it starts at the first entry, and it
   cannot start so late that the box would run past the end. */
static void listbox_clamp(wigptr wg)

{

    ami_long last = listbox_cnt(wg)-listbox_vis(wg)+1;

    if (wg->top > last) wg->top = last;
    if (wg->top < 1) wg->top = 1;

}

static void listbox_line(wigptr wg, ami_strptr sp, ami_long idx, ami_long y)

{

    if (idx == wg->lsel)
        fcolort(wg->wf, th_lstsel); /* the held selection */
    else if (wg->hover && idx == wg->ss)
        fcolort(wg->wf, th_lsthov); /* hover background */
    else ami_fcolor(wg->wf, ami_white); /* normal background */
    ami_frect(wg->wf, 4, y, ami_maxxg(wg->wf)-3, y+LSTROW(wg->wf)-1);
    ami_fcolor(wg->wf, ami_black);
    /* the text rides in the middle of the row, not at its top */
    y += (LSTROW(wg->wf)-ami_chrsizy(wg->wf))*0.5;
    if (wg->ntabs) { /* in columns, each field at its stop */

        const char* p = sp->str;
        ami_long i = 0;

        while (p && i <= wg->ntabs) {

            const char* e = strchr(p, '\t');
            char  fld[256];
            ami_long  l = e? (ami_long)(e-p): (ami_long)strlen(p);
            ami_long  x;

            if (l > (ami_long)sizeof(fld)-1) l = sizeof(fld)-1;
            memcpy(fld, p, l);
            fld[l] = 0;
            if (!i) x = ami_chrsizy(wg->wf)*0.5; /* the name leads */
            else if (wg->tabs[i-1] < 0) /* right aligned to its stop */
                x = -wg->tabs[i-1]-ami_strsiz(wg->wf, fld);
            else x = wg->tabs[i-1];
            ami_cursorg(wg->wf, x, y);
            fprintf(wg->wf, "%s", fld);
            if (!e) break;
            p = e+1;
            i++;

        }

    } else {

        ami_cursorg(wg->wf, ami_chrsizy(wg->wf)*0.5, y);
        fprintf(wg->wf, "%s", sp->str); /* place string */

    }

}

/* Repaint just the line at 1-based index idx (used to move the hover highlight
   without a full-list redraw). Out-of-range indices (including 0 = none) are a
   no-op. */
static void listbox_line_idx(wigptr wg, ami_long idx)

{

    ami_strptr sp;
    ami_long  y;
    ami_long  sc;

    if (idx < wg->top) return; /* above the view, nothing to paint */
    sp = wg->strlst; /* index top of stringlist */
    sc = 1; /* set first string */
    while (sp && sc < wg->top) { sp = sp->next; sc++; } /* to the view */
    y = LSTMARGIN(wg->wf); /* space to first string */
    while (sp && sc < idx) { /* walk to the target line */

        y += LSTROW(wg->wf); /* next line */
        sp = sp->next; /* next string */
        sc++; /* next select */

    }
    /* in the view: paint it. Below it, there is nothing to paint */
    if (sp && y+LSTROW(wg->wf) <= ami_maxyg(wg->wf))
        listbox_line(wg, sp, idx, y);

}

static void listbox_draw(
    /** Widget data pointer */ wigptr wg
)

{

    ami_strptr sp;
    ami_long  y;
    ami_long  sc;

    /* draw background */
    ami_fcolor(wg->wf, ami_white);
    ami_frrect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf), 4, 4);
    /* draw outline */
    fcolort(wg->wf, th_outline1);
    ami_linewidth(wg->wf, 2);
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 4, 4);
    /* from the entry the view starts at, for as many as the box holds */
    listbox_clamp(wg);
    sp = wg->strlst; /* index top of stringlist */
    sc = 1; /* set first string */
    while (sp && sc < wg->top) { sp = sp->next; sc++; } /* to the view */
    y = LSTMARGIN(wg->wf); /* space to first string */
    while (sp && y+LSTROW(wg->wf) <= ami_maxyg(wg->wf)) {

        listbox_line(wg, sp, sc, y); /* paint this line */
        y += LSTROW(wg->wf); /* next line */
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
    ami_long  y;
    ami_long  sc;
    ami_strptr sp;

    if (ev->etype == ami_etredraw) listbox_draw(wg); /* redraw the window */
    else if (ev->etype == ami_etmouba &&
             (ev->amoubn == 4 || ev->amoubn == 5)) {

        /* The wheel moves the view. A list longer than its box could not
           be reached at all before: it drew from its first entry and
           there it stayed. */
        ami_long was = wg->top;

        wg->top += ev->amoubn == 4? -3: 3; /* the usual three lines */
        listbox_clamp(wg);
        if (wg->top != was) listbox_draw(wg);

    } else if (ev->etype == ami_etscru || ev->etype == ami_etscrd) {

        ami_long was = wg->top; /* the scroll keys, a line at a time */

        wg->top += ev->etype == ami_etscru? -1: 1;
        listbox_clamp(wg);
        if (wg->top != was) listbox_draw(wg);

    } else if (ev->etype == ami_etpagu || ev->etype == ami_etpagd) {

        ami_long was = wg->top; /* and by the boxful */

        wg->top += (ev->etype == ami_etpagu? -1: 1)*listbox_vis(wg);
        listbox_clamp(wg);
        if (wg->top != was) listbox_draw(wg);

    } else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        /* note that if there is a click in the window, there must have also
           a mouse move */
        if (wg->ss) { /* there is a string select */

            /* the clicked entry holds the selection shading, as a GTK
               list row does */
            ami_long oldsel = wg->lsel;

            wg->lsel = wg->ss;
            if (oldsel != wg->lsel) {

                listbox_line_idx(wg, oldsel);
                listbox_line_idx(wg, wg->lsel);

            }
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

        ami_long oldss = wg->ss; /* remember previously hovered string */

        /* track position */
        wg->mpx = ev->moupxg; /* set present position */
        wg->mpy = ev->moupyg;

        /* which string the mouse is over, counting from the entry the
           view starts at */
        sp = wg->strlst; /* index top of string list */
        sc = 1; /* set first string */
        while (sp && sc < wg->top) { sp = sp->next; sc++; } /* to the view */
        y = LSTMARGIN(wg->wf); /* space to first string */
        wg->ss = 0; /* set no string selected */
        while (sp && y+LSTROW(wg->wf) <= ami_maxyg(wg->wf)) {

            /* if within the string bounding box, select it */
            if (wg->mpy >= y && wg->mpy <= y+LSTROW(wg->wf)) wg->ss = sc;
            y += LSTROW(wg->wf); /* next line */
            sc++; /* next select */
            sp = sp->next; /* next string */

        }
        /* only repaint when the highlighted item actually changes, and then
           repaint just the two affected lines (the one losing the highlight and
           the one gaining it) rather than re-rendering the whole list -- the
           list is unbuffered, so a full redraw per motion flashes and lags */
        if (wg->ss != oldss) {

            listbox_line_idx(wg, oldss);  /* un-highlight the previous line */
            listbox_line_idx(wg, wg->ss); /* highlight the current line */

        }

    } else if (ev->etype == ami_ethover) {

        /* Crossing into a nested window produces several EnterNotify events
           (NotifyNonlinear/Virtual/Ancestor), and hover only affects the one
           highlighted line -- so act only on the real 0->1 transition and
           repaint just that line, never the whole list (which flashed). */
        if (!wg->hover) {

            wg->hover = 1; /* hovered */
            listbox_line_idx(wg, wg->ss); /* highlight the hovered line, if any */

        }

    } else if (ev->etype == ami_etnohover) {

        if (wg->hover) {

            wg->hover = 0; /* not hovered */
            listbox_line_idx(wg, wg->ss); /* un-highlight (now paints normal) */
            wg->ss = 0; /* forget the line so re-entry starts clean */

        }

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

    ami_long  ddspc;  /* up/down control space */
    ami_long  figsiz; /* size of up/down figures */
    ami_strptr sp;
    ami_long  sc;
    ami_long  aw;
    ami_long  ah;
    ami_long  cx;
    ami_long  cy;

    ddspc = ami_chrsizy(win0)*1.9; /* square space for dropdown control */
    aw = ami_chrsizy(win0)*0.5;  /* the chevron's span */
    ah = ami_chrsizy(win0)*0.25;  /* and its depth */
    cx = ami_maxxg(wg->wf)-ami_chrsizy(win0)*0.8; /* in from the right edge */
    cy = ami_maxyg(wg->wf)*0.5-ah*0.5;
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
    ami_rrect(wg->wf, 2, 2, ami_maxxg(wg->wf)-1, ami_maxyg(wg->wf)-1, 6, 6);

    /* The mark is a chevron standing on its own, with no cell drawn around
       it and no line dividing it from the text: Breeze gives the drop mark
       no furniture of its own, the whole face being one control. */
    fcolort(wg->wf, th_droparrow);
    ami_linewidth(wg->wf, ami_chrsizy(win0)*0.09);
    ami_line(wg->wf, cx-aw*0.5, cy-ah*0.5, cx, cy+ah*0.5);
    ami_line(wg->wf, cx, cy+ah*0.5, cx+aw*0.5, cy-ah*0.5);

    /* draw current select */
    sp = wg->strlst;
    sc = wg->ss;
    /* find selected string */
    while (sc > 1 && sp) { sp = sp->next; sc--; }
    fcolort(wg->wf, th_droptext);
    ami_cursorg(wg->wf, ami_chrsizy(wg->wf)*0.5, ami_chrsizy(wg->wf)*0.5);
    if (sp) fprintf(wg->wf, "%s", sp->str); /* place string */

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

    ami_long udspc;    /* up/down control space */
    ami_long lbw, lbh; /* listbox sizing */
    ami_long w, h;     /* net width and height */
    ami_evtrec er; /* outbound event */
    FILE* par;    /* ultimate parent */
    ami_long  px,py;  /* position of widget in ultimate parent */
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

        /* The whole face opens the list, as it does on the desktop. The
           drop box within a drop edit box keeps to its control: there the
           face belongs to the edit box standing over it. */
        if (!wg->pw || wg->mpx >= ami_maxxg(wg->wf)-udspc) {

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

    if (!wg->cw || !wg->cw2) return; /* not yet wired: the completing redraw paints */

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

    if (!wg->cw || !wg->cw2) return; /* not yet wired: the completing redraw paints */

    ami_evtrec er; /* outbound event */
    ami_strptr sp;
    ami_long  sc;
    ami_long  l;

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



    ami_long sldsizp;    /* size of slider in pixels */
    ami_long sldposp;    /* position of slider in pixels */
    ami_long mid;        /* y midpoint */
    ami_long thk;        /* slider y thickness */
    ami_long margin;     /* margin at slider edges */
    ami_long trksizp;    /* track size in pixels */
    ami_long insld;      /* mouse is in slider */
    ami_long sldpos;     /* slider position */
    ami_evtrec er;   /* outbound event */
    double tiksizp; /* space between ticks in pixels */
    ami_long tickno;     /* ticks counter */
    ami_long x;

    mid = ami_maxyg(wg->wf)*0.5; /* find y midpoint */
    thk = ami_chrsizy(wg->wf)*0.14; /* find slider track thickness */
    sldsizp = ami_chrsizy(wg->wf)*1.0; /* find slider size in pixels */
    margin = sldsizp*0.5+ENDSPACE; /* set edge margins */
    trksizp = ami_maxxg(wg->wf)-margin*2; /* set track width */
    sldposp = margin+round((double)trksizp*wg->sclpos/LONG_MAX);

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
        sldpos = x-margin >= trksizp? LONG_MAX: LONG_MAX/trksizp*(x-margin);
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
        sldpos = x >= trksizp? LONG_MAX: LONG_MAX/trksizp*x;
        wg->sclpos = sldpos; /* place to widget data */
        /* send event back to parent window */
        er.etype = ami_etsldpos; /* set scroll position event */
        er.sldpid = wg->id; /* set id */
        er.sldpos = sldpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */
        wg->grab = TRUE; /* set we grabbed the scrollbar */

    } else if (!wg->pressed) wg->grab = FALSE;

    /* recalculate for any slide movements */
    sldposp = margin+round((double)trksizp*wg->sclpos/LONG_MAX);

    /* color the background */
    ami_fcolor(wg->wf, ami_white);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* color scale track */
    fcolort(wg->wf, th_sldint);
    ami_frrect(wg->wf, margin, mid-thk*0.5, ami_maxxg(wg->wf)-margin,
              mid+thk*0.5, 4, 4);
    ami_linewidth(wg->wf, 2);
    fcolort(wg->wf, th_outline2);
    ami_rrect(wg->wf, margin, mid-thk*0.5, ami_maxxg(wg->wf)-margin,
             mid+thk*0.5, 4, 4);
    /* color active side */
    fcolort(wg->wf, th_progactcen);
    ami_frrect(wg->wf, margin, mid-thk*0.5, sldposp, mid+thk*0.5, 4, 4);
    /* draw slider */
    ami_fcolor(wg->wf, ami_white);
    ami_fellipse(wg->wf, sldposp-sldsizp*0.5, mid-sldsizp*0.5,
                       sldposp+sldsizp*0.5, mid+sldsizp*0.5);
    if ((wg->pressed && (insld || wg->grab)) || wg->hover)
        /* engaged: the Breeze blue */
        fcolort(wg->wf, th_focus);
    else
        /* at rest */
        fcolort(wg->wf, th_outline2);
    ami_linewidth(wg->wf, 3);
    ami_ellipse(wg->wf, sldposp-sldsizp*0.5, mid-sldsizp*0.5,
                      sldposp+sldsizp*0.5, mid+sldsizp*0.5);

    /* place tickmarks */
    if (wg->ticks) {

        /* Counted, not accumulated: a degenerate track size makes the
           step zero, and a while over an unmoving position never ends. */
        tiksizp = wg->ticks > 1? trksizp/(wg->ticks-1): 0;
        ami_fcolor(wg->wf, ami_black); /* set color */
        for (tickno = 0; tickno < wg->ticks; tickno++) {

            x = margin+tiksizp*tickno; /* set location */
            /* below the track: Breeze puts its ticks under a horizontal
               slider, and to the right of a vertical one */
            ami_line(wg->wf, x, mid+sldsizp*0.5, x, ami_maxyg(wg->wf));

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
        /* only repaint while actually dragging the slider (see scrollvert) */
        if (wg->pressed) slidehoriz_draw(wg);
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



    ami_long sldsizp;  /* size of slider in pixels */
    ami_long sldposp;  /* position of slider in pixels */
    ami_long mid;      /* y midpoint */
    ami_long thk;      /* slider y thickness */
    ami_long margin;   /* margin at slider edges */
    ami_long trksizp;  /* track size in pixels */
    ami_long insld;    /* mouse is in slider */
    ami_long sldpos;   /* slider position */
    ami_evtrec er; /* outbound event */
    double tiksizp; /* space between ticks in pixels */
    ami_long tickno;     /* ticks counter */
    ami_long y;

    mid = ami_maxxg(wg->wf)*0.5; /* find x midpoint */
    thk = ami_chrsizy(wg->wf)*0.14; /* find slider track thickness */
    sldsizp = ami_chrsizy(wg->wf)*1.0; /* find slider size in pixels */
    margin = sldsizp*0.5+ENDSPACE; /* set edge margins */
    trksizp = ami_maxyg(wg->wf)-margin*2; /* set track width */
    /* The position counts down the track: 0 is the top, as graphics.h
       defines it. Qt counts a vertical slider the other way, up from the
       foot, but the position a program is handed cannot depend on which
       desktop it happens to be running under. */
    sldposp = margin+round((double)trksizp*wg->sclpos/LONG_MAX);

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
        sldpos = y-margin >= trksizp? LONG_MAX: LONG_MAX/trksizp*(y-margin);
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
        sldpos = y >= trksizp? LONG_MAX: LONG_MAX/trksizp*y;
        wg->sclpos = sldpos; /* place to widget data */
        /* send event back to parent window */
        er.etype = ami_etsldpos; /* set scroll position event */
        er.sldpid = wg->id; /* set id */
        er.sldpos = sldpos; /* set scrollbar position */
        ami_sendevent(wg->parent, &er); /* send the event to the parent */
        wg->grab = TRUE; /* set we grabbed the scrollbar */

    } else if (!wg->pressed) wg->grab = FALSE;

    /* recalculate for any slide movements */
    sldposp = margin+round((double)trksizp*wg->sclpos/LONG_MAX);

    /* color the background */
    ami_fcolor(wg->wf, ami_white);
    ami_frect(wg->wf, 1, 1, ami_maxxg(wg->wf), ami_maxyg(wg->wf));
    /* color scale track */
    fcolort(wg->wf, th_sldint);
    ami_frrect(wg->wf, mid-thk*0.5, margin, mid+thk*0.5,
              ami_maxyg(wg->wf)-margin, 4, 4);
    ami_linewidth(wg->wf, 2);
    fcolort(wg->wf, th_outline2);
    ami_rrect(wg->wf, mid-thk*0.5, margin, mid+thk*0.5,
             ami_maxyg(wg->wf)-margin, 4, 4);
    /* color active side */
    fcolort(wg->wf, th_progactcen);
    ami_frrect(wg->wf, mid-thk*0.5, margin, mid+thk*0.5, sldposp, 4, 4);
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

        /* Counted, not accumulated: a degenerate track size makes the
           step zero, and a while over an unmoving position never ends. */
        tiksizp = wg->ticks > 1? trksizp/(wg->ticks-1): 0;
        ami_fcolor(wg->wf, ami_black); /* set color */
        for (tickno = 0; tickno < wg->ticks; tickno++) {

            y = margin+tiksizp*tickno; /* set location */
            ami_line(wg->wf, mid+sldsizp*0.5, y, ami_maxxg(wg->wf), y); /* draw tick */

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
        /* only repaint while actually dragging the slider (see scrollvert) */
        if (wg->pressed) slidevert_draw(wg);
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
    ami_long  sc;
    ami_long  xm, y, x1, x2;
    ami_long  th; /* tabbar height/width (by orientation) */

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

                /* The chosen tab is a panel of the client's own colour,
                   standing out of the strip and open to the client below
                   it: the line that divides strip from client is painted
                   over for the width of the tab, so the two read as one
                   surface. Breeze marks the choice that way and puts no
                   bar under it. A hovered tab is only washed. */
                ami_long tx1 = ami_curxg(wg->wf)-ami_chrsizy(wg->wf)*0.5;
                ami_long tx2 = ami_curxg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                           ami_chrsizy(wg->wf)*0.5;

                if (sc == 1) tx1 = 2; /* the first tab stands at the edge */

                if (sc == wg->ss) ami_fcolor(wg->wf, ami_white);
                else fcolort(wg->wf, th_backhover);
                if (wg->tor == ami_totop)
                    ami_frect(wg->wf, tx1, 2, tx2, sc == wg->ss? th: th-2);
                else
                    ami_frect(wg->wf, tx1,
                             ami_maxyg(wg->wf)-th+(sc == wg->ss? 0: 2),
                             tx2, ami_maxyg(wg->wf)-2);
                /* the panel keeps the outline the strip has, but not on
                   the side it shares with the client */
                if (sc == wg->ss) {

                    ami_linewidth(wg->wf, 1);
                    fcolort(wg->wf, th_outline1);
                    if (wg->tor == ami_totop) {

                        ami_line(wg->wf, tx1, 2, tx1, th);
                        ami_line(wg->wf, tx2, 2, tx2, th);

                    } else {

                        ami_line(wg->wf, tx1, ami_maxyg(wg->wf)-th,
                                        tx1, ami_maxyg(wg->wf)-2);
                        ami_line(wg->wf, tx2, ami_maxyg(wg->wf)-th,
                                        tx2, ami_maxyg(wg->wf)-2);

                    }

                }

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
            ami_path(wg->wf, LONG_MAX/2); /* set vertical downwards text */
        while (sp && ami_curyg(wg->wf) >= 1) {

            if (sc == wg->ss || sc == wg->sh) { /* draw select/hover */

                ami_linewidth(wg->wf, 6);
                /* as at the top and the bottom: the chosen tab is a panel
                   of the client's colour, open to the client beside it */
                ami_long ty1 = ami_curyg(wg->wf)-ami_chrsizy(wg->wf)*0.5;
                ami_long ty2 = ami_curyg(wg->wf)+ami_strsiz(wg->wf, sp->str)+
                           ami_chrsizy(wg->wf)*0.5;

                if (sc == 1) ty1 = 2; /* the first tab stands at the edge */
                if (sc == wg->ss) ami_fcolor(wg->wf, ami_white);
                else fcolort(wg->wf, th_backhover);
                if (wg->tor == ami_toleft)
                    ami_frect(wg->wf, 2, ty1, sc == wg->ss? th: th-2, ty2);
                else
                    ami_frect(wg->wf,
                             ami_maxxg(wg->wf)-th+(sc == wg->ss? 0: 2), ty1,
                             ami_maxxg(wg->wf)-2, ty2);
                if (sc == wg->ss) {

                    ami_linewidth(wg->wf, 1);
                    fcolort(wg->wf, th_outline1);
                    if (wg->tor == ami_toleft) {

                        ami_line(wg->wf, 2, ty1, th, ty1);
                        ami_line(wg->wf, 2, ty2, th, ty2);

                    } else {

                        ami_line(wg->wf, ami_maxxg(wg->wf)-th, ty1,
                                        ami_maxxg(wg->wf)-2, ty1);
                        ami_line(wg->wf, ami_maxxg(wg->wf)-th, ty2,
                                        ami_maxxg(wg->wf)-2, ty2);

                    }

                }

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
        ami_path(wg->wf, LONG_MAX/4); /* set normal text */

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
    ami_long  th; /* tabbar height/width (by orientation) */
    ami_long  x, y;
    ami_long  sc;
    ami_long  sh;
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
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  void*       vwg
)

{

    wigptr wg = (wigptr)vwg; /* the base found one of ours */

    switch (wg->typ) { /* handle according to type */

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

static ami_long igetwigid(
    /** Window file */ FILE* f
)

{

    return (wb_getwigid(&pkg, f)); /* the base allocates */

}

/** ****************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

/* Shut an open drop down list.

The list a drop box drops is a widget of the ultimate parent window, put
there so it can stand outside the drop box's own window. Nothing else knows
to close it, so a drop box killed with its list standing takes its window
out from under a widget that still points at it, and the kill never
returns. */

static void dropdownkill(
    /** Widget being killed */ wigptr wp
)

{

    wigptr dp;  /* the drop box holding the list */
    FILE*  par; /* the window the list stands in */

    if (!wp) return;
    /* a drop edit box drops its list through the drop box within it */
    dp = wp->typ == wtdropeditbox? wp->cw: wp;
    if (!dp || dp->typ != wtdropbox || !dp->cw) return;
    par = dp->parent; /* set near parent */
    if (dp->pw) par = dp->pw->parent; /* a subclass parents one up */
    ami_killwidget(par, dp->cid); /* close the list */
    dp->cw = NULL; /* set no child widget */

}

/** ****************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

static void ikillwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ ami_long id
)

{

    dropdownkill(fndwig(f, id)); /* a list still down goes with it */
    wb_killwidget(&pkg, f, id); /* wigkill cascades any subwidgets */

}

/** ****************************************************************************

Select/deselect widget

Selects or deselects a widget. Any widget accepts and records the select
state. Buttons render it as a persistent pressed in look, checkboxes and
radio buttons as their check figure; widget kinds with no select rendering
simply record the state.

*******************************************************************************/

static void iselectwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ ami_long id,
    /** On/off for select */ ami_long e
)

{

    wigptr    wp;  /* widget entry pointer */
    ami_long  chg; /* widget state changes */

    wp = fndwig(f, id); /* index the widget */
    chg = wp->select != !!e; /* check select state changes */
    wp->select = !!e; /* set select state */
    /* if the select changes, refresh the widget */
    if (chg) widget_redraw(wp); /* send redraw to widget */

}

/** ****************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

static void ienablewidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ ami_long  id,
    /** On/off for enable */ ami_long  e
)

{

    wigptr    wp;  /* widget entry pointer */
    ami_long  chg; /* widget state changes */

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

The text is returned in a critical buffer: if the text exactly fills the
buffer, the terminating zero is left off, and it is an error if the text
cannot fit in the buffer.

*******************************************************************************/

static void igetwidgettext(
    /** Window file */                   FILE* f,
    /** Logical widget id */             ami_long  id,
    /** Output pointer to widget text */ char* s,
    /** Length of string buffer */       ami_long  sl
)

{

    wigptr wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* a number select box and a drop edit box show their text in the edit
       box they are made of, and that is the one holding the live string */
    if (wp->typ == wtnumselbox) wp = wp->cw;
    else if (wp->typ == wtdropeditbox && wp->cw2) wp = wp->cw2;
    /* check this widget can have face text read */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox)
        error("Widget content cannot be read");
    cpycrit(s, sl, wp->face); /* copy face text to critical result buffer */

}

/** ****************************************************************************

put edit box text

Places text into an edit box.

*******************************************************************************/

static void iputwidgettext(
    /** Window file */       FILE* f,
    /** Logical widget id */ ami_long  id,
    /** Text to place */     char* s
)

{

    wigptr    wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* a number select box and a drop edit box show their text in the edit
       box they are made of, so that is where it is placed. The master of a
       drop edit box keeps the same string, which is what it reports when
       the edit completes. */
    if (wp->typ == wtnumselbox) wp = wp->cw;
    else if (wp->typ == wtdropeditbox && wp->cw2) {

        free(wp->face); /* dispose of previous face string */
        wp->face = str(s); /* place new face */
        wp = wp->cw2;

    }
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
    /** Logical widget id */   ami_long  id,
    /** New size for widget */ ami_long  x,
                               ami_long  y
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
    /** Logical widget id */   ami_long  id,
    /** New size for widget */ ami_long  x,
                               ami_long  y
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
    /** Logical widget id */       ami_long  id,
    /** New position for widget */ ami_long  x,
                                   ami_long  y
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
    /** Logical widget id */       ami_long  id,
    /** New position for widget */ ami_long  x,
                                   ami_long  y
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

/* Is this widget a component?

The Z order runs in three layers, bottom to top: the windows behind, the
parent among them; the components; and the widgets and controls. Components
are the widgets that only display, and they are the layer controls stand on.

*******************************************************************************/

static int iscomponent(
    /** Widget entry */ wigptr wp
)

{

    return (wp->typ == wtbackground || wp->typ == wtgroup ||
            wp->typ == wtprogbar);

}

/** ****************************************************************************

Sink the components below a control

Puts every component of the window behind the control just sent back. The
top layer is self referencing: a control sent to the back goes to the bottom
of its own layer and no further, so the components follow it down. Two
components never lie over one another, so the order they end in among
themselves does not signify.

*******************************************************************************/

static void sinkcomponents(
    /** Window file */           FILE*  f,
    /** The control sent back */ wigptr wp
)

{

    wbfilptr fp; /* the window's widget table */
    wigptr   q;  /* widget walked */
    ami_long i;

    fp = pkg.opnfil[fileno(f)];
    if (!fp) return;
    for (i = 0; i < MAXWIG*2+1; i++) {

        q = (wigptr)fp->widgets[i];
        if (q && q != wp && iscomponent(q)) ami_back(q->wf);

    }

}

static void ibackwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ ami_long  id
)

{

    wigptr wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* components do not lie over one another, so there is no place in the
       order to move one to */
    if (iscomponent(wp)) error("Z order call on a component");
    ami_back(wp->wf); /* to the back of everything... */
    sinkcomponents(f, wp); /* ...and the components behind it again */

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

static void ifrontwidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ ami_long  id
)

{

    wigptr wp;  /* widget entry pointer */

    wp = fndwig(f, id); /* index the widget */
    /* components do not lie over one another, so there is no place in the
       order to move one to */
    if (iscomponent(wp)) error("Z order call on a component");
    /* the top of everything is the top of the control layer */
    ami_front(wp->wf);

}

/** ****************************************************************************

Place input focus on a given widget

*******************************************************************************/

static void ifocuswidget(
    /** Window file */       FILE* f,
    /** Logical widget id */ ami_long  id
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
    /** Minimum width return */  ami_long*  w,
    /** Minimum height return */ ami_long*  h
)

{

    *h = ami_chrsizy(win0)*1.9; /* set height */
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
    /** Minimum width return */  ami_long*  w,
    /** Minimum height return */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtbutton, &wp);
    ami_linewidth(wp->wf, 3); /* thicker lines */

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create button text

Creates a standard text button within the specified rectangle, on the given
window.

*******************************************************************************/

static void ibutton(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** logical widget id */   ami_long  id
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
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
)

{

    *h = ami_chrsizy(win0)*1.25; /* set height */
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
    /** Return width */  ami_long*  w,
    /** return height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** Logical widget id */   ami_long  id)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtcheckbox, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create checkbox text

Creates a standard text checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

static void icheckbox(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** logical widget id */   ami_long  id
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
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h)

{

    *h = ami_chrsizy(win0)*1.25; /* set height */
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
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtradiobutton, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create radio button text

Creates a standard text radio button within the specified rectangle, on the
given window.

*******************************************************************************/

static void iradiobutton(
    /** Window file */ FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** logical widget id */   ami_long  id
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
    /** Client width */          ami_long  cw,
    /** Client height */         ami_long  ch,
    /** Returns width */         ami_long*  w,
    /** Returns height */        ami_long*  h,
    /** Returns client origin */ ami_long*  ox,
                                 ami_long*  oy
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
    /** Client width */          ami_long cw,
    /** Client height */         ami_long ch,
    /** Returns width */         ami_long* w,
    /** Returns height */        ami_long* h,
    /** Returns client origin */ ami_long* ox,
                                 ami_long* oy
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, s, id, wtgroup, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create group box text

Creates a text group box, which is really just a decorative feature that
gererates no messages. It is used as a background for other widgets.

*******************************************************************************/

static void igroup(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Face string */         char* s,
    /** logical widget id */   ami_long  id)

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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtbackground, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create background box text

Creates a text background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

static void ibackground(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
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
    /** Returns width */  ami_long*  w,
    /** Returns height */ ami_long*  h
)

{

    *w = ami_chrsizy(win0)*1.17;
    *h = 40;

}

/** ****************************************************************************

Find minimum/standard vertical scrollbar size text

Finds the minimum size for a text vertical scrollbar. The minimum size of a
vertical scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollvertsiz(
    /** Window file */    FILE* f,
    /** Returns width */  ami_long*  w,
    /** Returns height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtscrollvert, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create vertical scrollbar text

Creates a text vertical scrollbar.

*******************************************************************************/

static void iscrollvert(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
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
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
)

{

    *w = 40;
    *h = ami_chrsizy(win0)*1.17;

}

/** ****************************************************************************

Find minimum/standard horizontal scrollbar size text

Finds the minimum size for a text horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

static void iscrollhorizsiz(
    /** Window file */   FILE* f,
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtscrollhoriz, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create horizontal scrollbar text

Creates a text horizontal scrollbar.

*******************************************************************************/

static void iscrollhoriz(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
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
    /** Logical widget id */       ami_long id,
    /** Ratioed slider position */ ami_long r
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
    /** Logical widget id */ ami_long  id,
    /** Ratioed size */      ami_long  r
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
    /** Lower bound */    ami_long  l,
    /** Upper bound */    ami_long  u,
    /** Returns width */  ami_long*  w,
    /** Returns height */ ami_long*  h
)

{

    ami_long mv; /* maximum value */
    ami_long dc; /* digit count */
    ami_long udspc; /* up/down control space */

    /* first determine the number of digit places, including the sign */
    mv = u; /* set upper value */
    if (labs(l) > labs(u)) mv = l; /* find maximum digits */
    dc = digits(labs(mv)); /* find the digit count */
    if (mv < 0) dc++; /* add the sign */

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */

    *h = ami_chrsizy(win0)*1.8; /* set height */
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
    /** Lower bound */    ami_long  l,
    /** Upper bound */    ami_long  u,
    /** Returns width */  ami_long*  w,
    /** Returns height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Lower bound */         ami_long  l,
    /** Upper bound */         ami_long  u,
    /** Logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */
    wigptr wps; /* widget subclass entry pointer */
    ami_long udspc; /* up/down control space */
    char numbuf[25]; /* the number the box starts at */

    udspc = ami_chrsizy(win0)*1.9; /* square space for up/down control */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtnumselbox, &wp);

    /* set up a subclass entry */
    wps = getwig(); /* get widget entry */
    wps->num = TRUE; /* set numeric only */
    wps->lbnd = l; /* set lower bound */
    wps->ubnd = u; /* set upper bound */
    sprintf(numbuf, "%lld", AMI_LONG_CAST(l)); /* the box opens showing its lower bound */
    /* subclass an edit control,leaving space for up/down controls */
    widget(wp->wf, 1+4, 1+4, ami_maxxg(wp->wf)-udspc*2-4, ami_maxyg(wp->wf)-4,
           numbuf, 1, wteditbox, &wps);
    ami_curvis(wps->wf, FALSE); /* turn on cursor */
    wp->cw = wps; /* give the master its child window */
    wps->pw = wp; /* give the child its master */
    widget_redraw(wps); /* the child's first paint */
    widget_redraw(wp); /* now whole: the finished face */

}

/** ****************************************************************************

Create number selector text

Creates an up/down control for a text numeric selection.

*******************************************************************************/

static void inumselbox(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Lower bound */         ami_long  l,
    /** Upper bound */         ami_long  u,
    /** Logical widget id */   ami_long  id)

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
    /** Returns width */      ami_long*  w,
    /** Returns height */     ami_long*  h
)

{

    *h = ami_chrsizy(win0)*1.8; /* set height */
    *w = ami_strsiz(win0, s)+ENDLEDSPC*2; /* the face sits inside end spaces */

}

/** ****************************************************************************

Find minimum/standard edit box size text

Finds the minimum size for a text edit box. Given a sample face string, the
minimum size of an edit box is calculated and returned.

*******************************************************************************/

static void ieditboxsiz(
    /** Window file */        FILE* f,
    /** Sample face string */ char* s,
    /** Returns width */      ami_long*  w,
    /** Returns height */     ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wteditbox, &wp);
    ami_curvis(wp->wf, FALSE); /* turn on cursor */

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create edit box text

Creates single line text edit box

*******************************************************************************/

static void ieditbox(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
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
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
)

{

    *w = 400;
    *h = ami_chrsizy(win0)*1.45;

}

/** ****************************************************************************

Find minimum/standard progress bar size text

Progress bars are fairly arbitrary, and the dimensions given are more of a
suggestion. The height is based on character size, which is a pretty good base
measure, but the width is really up to the caller.

*******************************************************************************/

static void iprogbarsiz(
    /** Window file */   FILE* f,
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = NULL; /* set no predefinition */
    widget(f, x1, y1, x2, y2, "", id, wtprogbar, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create progress bar text

Creates a progress bar.

*******************************************************************************/

static void iprogbar(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** logical widget id */   ami_long  id
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
    /** logical widget id */ ami_long id,
    /** Ratioed position */  ami_long pos)

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
    /** Return width */        ami_long*      w,
    /** Return height */       ami_long*      h
)

{

    ami_long  lc;   /* line counter */
    ami_long  maxp; /* maximum pixel length */
    ami_long  pl;   /* pixel length */
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
    *h = lc*LSTROW(win0)+LSTMARGIN(win0); /* the rows, and the margin above them */

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
    /** Return width */        ami_long*      w,
    /** Return height */       ami_long*      h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   ami_long  id
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

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create list box text

Creates a text list box. Fills it with the string list provided.

*******************************************************************************/

static void ilistbox(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** String list pointer */ ami_strptr sp,
    /** logical widget id */   ami_long  id
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
    /** Closed width */        ami_long*      cw,
    /** Closed height */       ami_long*      ch,
    /** Open width */          ami_long*      ow,
    /** Open height */         ami_long*      oh
)

{

    ami_long lbw, lbh;

    /* find listbox sizing first */
    ami_listboxsizg(f, sp, &lbw, &lbh);

    /* closed size is width of listbox plus down arrow, height is character */
    *cw = lbw+ami_chrsizy(win0)*1.9; /* find closed width */
    *ch = ami_chrsizy(win0)*1.8; /* find closed height */

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
    /** Closed width */        ami_long*      cw,
    /** Closed height */       ami_long*      ch,
    /** Open width */          ami_long*      ow,
    /** Open height */         ami_long*      oh
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   ami_long  id
)

{

    wigptr    wp; /* widget entry pointer */
    ami_strptr nl; /* new string list */
    ami_long  ch; /* closed height */

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

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create dropdown box text

Creates a text dropdown box. Fills it with the string list provided.

*******************************************************************************/

static void idropbox(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   ami_long  id
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
    /** Return closed width */  ami_long*      cw,
    /** Return closed height */ ami_long*      ch,
    /** Return open width */    ami_long*      ow,
    /** Return open height */   ami_long*      oh
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
    /** Return closed width */  ami_long*      cw,
    /** Return closed height */ ami_long*      ch,
    /** Return open width */    ami_long*      ow,
    /** Return open height */   ami_long*      oh
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   ami_long  id
)

{

    wigptr    wp;     /* widget entry pointer */
    wigptr    wps;    /* widget subclass entry pointer */
    ami_strptr nl;     /* new string list */
    ami_long  cw, ch; /* closed dimensions */
    ami_long  ow, oh; /* open dimensions */

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
    widget_redraw(wps); /* the child's first paint */

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
    widget_redraw(wps); /* the child's first paint */
    widget_redraw(wp); /* now whole: the finished face */

}

/** ****************************************************************************

Create dropdown edit box text

Creates a text dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

static void idropeditbox(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** String list pointer */ ami_strptr sp,
    /** Logical widget id */   ami_long  id
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
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
)

{

    *w = 40;
    *h = ami_chrsizy(win0)*1.1;

}

/** ****************************************************************************

Find minimum/standard horizontal slider size text

Finds the minimum size for a text horizontal slider. The minimum size of a
horizontal slider is calculated and returned.

*******************************************************************************/

static void islidehorizsiz(
    /** Window file */   FILE* f,
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Tick mark interval */  ami_long  mark,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = getwig(); /* predef so we can plant ticks before display */
    wp->ticks = mark; /* set tick marks */
    widget(f, x1, y1, x2, y2, "", id, wtslidehoriz, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create horizontal slider text

Creates a text horizontal slider.

*******************************************************************************/

static void islidehoriz(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Tick mark interval */  ami_long  mark,
    /** logical widget id */   ami_long  id
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
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
)

{

    *w = ami_chrsizy(win0)*1.1;
    *h = 40;

}

/** ****************************************************************************

Find minimum/standard vertical slider size text

Finds the minimum size for a text vertical slider. The minimum size of a
vertical slider is calculated and returned.

*******************************************************************************/

static void islidevertsiz(
    /** Window file */   FILE* f,
    /** Return width */  ami_long*  w,
    /** Return height */ ami_long*  h
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Tick mark interval */  ami_long  mark,
    /** logical widget id */   ami_long  id
)

{

    wigptr wp; /* widget entry pointer */

    wp = getwig(); /* predef so we can plant ticks before display */
    wp->ticks = mark; /* set tick marks */
    widget(f, x1, y1, x2, y2, "", id, wtslidevert, &wp);

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create vertical slider text

Creates a text vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

static void islidevert(
    /** Window file */         FILE* f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Tick mark interval */  ami_long  mark,
    /** logical widget id */   ami_long  id
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
    /** Tab strings */            ami_strptr sp,
    /** Tab orientation */        ami_tabori tor,
    /** Client width */           ami_long  cw,
    /** Client height */          ami_long  ch,
    /** Return width */           ami_long*      w,
    /** Return height */          ami_long*      h,
    /** Return client offset x */ ami_long*      ox,
    /** Return client offset x */ ami_long*      oy
)

{

    ami_long need;   /* the tab strip's own minimum along its length */
    ami_long chy;

    /* The strip must hold the tabs. They are laid out from a margin of one
       character height, each tab as wide as its string, with a character
       height between them and the same again at the end. A client wider
       than that decides the size; a client narrower than that does not,
       because tabs that do not fit cannot be read or hit. */
    chy = ami_chrsizy(f);
    need = chy;
    while (sp) {

        need += ami_strsiz(f, sp->str)+chy;
        sp = sp->next;

    }

    /* find width */
    if (tor == ami_toleft || tor == ami_toright) *w = cw+ami_chrsizy(win0)*TABHGT;
    else *w = cw < need? need: cw;
    /* find height */
    if (tor == ami_toleft || tor == ami_toright) *h = ch < need? need: ch;
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
    /** Tab strings */            ami_strptr sp,
    /** Tab orientation */        ami_tabori tor,
    /** Client width */           ami_long  cw,
    /** Client height */          ami_long  ch,
    /** Return width */           ami_long*      w,
    /** Return height */          ami_long*      h,
    /** Return client offset x */ ami_long*      ox,
    /** Return client offset x */ ami_long*      oy
)

{

    ami_long gw, gh, gox, goy;

    /* convert client sizes to graphical */
    cw = cw*ami_chrsizx(f);
    ch = ch*ami_chrsizy(f);
    ami_tabbarsizg(f, sp, tor, cw, ch, &gw, &gh, &gox, &goy); /* get size */
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
    /** Return client width */    ami_long  w,
    /** Return client height */   ami_long  h,
    /** Width */                  ami_long*      cw,
    /** Height */                 ami_long*      ch,
    /** Return client offset x */ ami_long*      ox,
    /** Return client offset x */ ami_long*      oy
)

{

    /* find width */
    if (tor == ami_toleft || tor == ami_toright) *cw = w-ami_chrsizy(win0)*TABHGT;
    else *cw = w;
    /* find height */
    if (tor == ami_toleft || tor == ami_toright) *ch = h;
    else *ch = h-ami_chrsizy(win0)*TABHGT;
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
    /** Return client width */    ami_long  w,
    /** Return client height */   ami_long  h,
    /** Width */                  ami_long*      cw,
    /** Height */                 ami_long*      ch,
    /** Return client offset x */ ami_long*      ox,
    /** Return client offset x */ ami_long*      oy
)

{

    ami_long gw, gh, gox, goy;

    /* convert sizes to graphical */
    w = w*ami_chrsizx(f);
    h = h*ami_chrsizy(f);
    /* the client area turns on the strip's thickness, not on the tabs, so
       the list is not wanted here */
    ami_tabbarsizg(f, NULL, tor, w, h, &gw, &gh, &gox, &goy); /* get size */
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
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Tab string list */     ami_strptr sp,
    /** Tab orientation */     ami_tabori tor,
    /** logical widget id */   ami_long  id
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

    widget_redraw(wp); /* first paint of the finished face */

}

/** ****************************************************************************

Create tab bar text

Creates a text tab bar with the given orientation.

*******************************************************************************/

static void itabbar(
    /** Window file */         FILE*     f,
    /** Placement rectangle */ ami_long  x1,
                               ami_long  y1,
                               ami_long  x2,
                               ami_long  y2,
    /** Tab string list */     ami_strptr sp,
    /** Tab orientation */     ami_tabori tor,
    /** logical widget id */   ami_long  id
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

    /* The first paint is requested here, not left to fate: a widget
       window becomes visible when it is first drawn, and its first
       draw was riding on a later parent-window redraw that a quiet
       display never sends. The queued redraw arrives after the
       creator finishes, paints the finished face, and maps the
       window. */
    widget_redraw(wp);

}

/** ****************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

static void itabsel(
    /** Window file */         FILE* f,
    /** logical widget id */   ami_long  id,
    /** Logical tab number */  ami_long  tn
)

{

    wigptr    wp;  /* widget entry pointer */
    ami_long  chg; /* widget state changes */
    ami_strptr sp;
    ami_long  ss;

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

/** ****************************************************************************

Finish a modal dialog

The modal dialogs run their own event loop, and manufacture ami_etterm to
mean "the dialog was dismissed". A terminate that came from outside, such as
the window manager closing the program's window, arrives as the same event
type, so the two must be told apart: a dismissal ends the dialog only, while
a real terminate must end the program.

The dialog loops flag a terminate that arrived from ami_event, as opposed to
one they made themselves, and pass that flag here. A real terminate is sent
on to the main window, so the application's own event loop receives it and
can shut down. Manufactured ones are dropped, having done their job of
ending the dialog.

*******************************************************************************/

static void endmodal(
    /** the event that ended the dialog */  ami_evtrec* er,
    /** the terminate came from outside */  int realterm
)

{

    if (realterm) ami_sendevent(stdout, er); /* pass the terminate on */

}

static void ialert(
    /** Title string */   char* title,
    /** Message string */ char* message
)

{

    FILE*      in;       /* window to create */
    FILE*      out;
    ami_long   wid;      /* window number */
    ami_long   mxs;      /* maximum text size */
    ami_evtrec  er;       /* event record */
    ami_long   ts;       /* title pixel size */
    ami_long   ms;       /* message pixel size */
    ami_long   icsize;   /* size of circle i in pixels */
    ami_long   isize;    /* size of i character in pixels */
    ami_long   tstart;   /* start of text to right of i circle */
    ami_long   fs;       /* font size save */
    ami_long   mpx, mpy; /* mouse position */
    themeindex tc;       /* text color */
    ami_long   focus;    /* in focus */
    int        realterm; /* a terminate arrived from outside */

    realterm = FALSE; /* set no outside terminate */
    focus = FALSE; /* set not in focus */
    tc = th_text; /* set focused text */
    wid = ami_getwinid(); /* get anonymous window id */
    in = NULL; /* a fresh input side, not a shared one */
    ami_openwin(&in, &out, NULL, wid); /* create window */
    ami_buffer(out, FALSE); /* turn off buffering */
    ami_auto(out, FALSE); /* turn off auto */
    ami_curvis(out, FALSE); /* turn off cursor */
    setwigfont(out); /* set sign font */
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

    /* The first paint is requested, not left to fate: the dialog
       paints on redraw events, its window maps on its first draw, and
       an expose cannot arrive for a window not yet mapped. */
    er.etype = ami_etredraw;
    er.rsx = 1;
    er.rsy = 1;
    er.rex = ami_maxxg(out);
    er.rey = ami_maxyg(out);
    ami_sendevent(out, &er);

    /* start with events */
    do {

        ami_event(in, &er);
        /* a terminate here came from outside, since the dialog makes its own
           only further down; remember it so it can be passed on */
        if (er.etype == ami_etterm) realterm = TRUE;
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

                /* The OK button, a Breeze button at the bottom right;
                   the focus ring rides its outline */
                fcolort(out, th_back);
                ami_frrect(out, ami_maxxg(out)-ami_chrsizy(out)*5,
                                ami_maxyg(out)-ami_chrsizy(out)*1.8,
                                ami_maxxg(out)-ami_chrsizy(out)*0.7,
                                ami_maxyg(out)-ami_chrsizy(out)*0.4, 6, 6);
                ami_linewidth(out, 3);
                if (focus) fcolort(out, th_focus);
                else fcolort(out, th_outline1);
                ami_rrect(out, ami_maxxg(out)-ami_chrsizy(out)*5,
                               ami_maxyg(out)-ami_chrsizy(out)*1.8,
                               ami_maxxg(out)-ami_chrsizy(out)*0.7,
                               ami_maxyg(out)-ami_chrsizy(out)*0.4, 6, 6);
                fcolort(out, tc);
                ami_cursorg(out, ami_maxxg(out)-ami_chrsizy(out)*2.85-
                                 ami_strsiz(out, "OK")*0.5,
                                ami_maxyg(out)-ami_chrsizy(out)*1.6);
                fputs("OK", out);

                break;

            case ami_etmoumovg:
                mpx = er.moupxg; /* save mouse position */
                mpy = er.moupyg;
                break;

            case ami_etmouba:
                if (er.amoubn == 1 && mpy >= ami_maxyg(out)-ami_chrsizy(out)*2)
                    er.etype = ami_etterm; /* the OK button was pressed */
                break;

            case ami_etenter:
                er.etype = ami_etterm; /* enter is the same as OK */
                break;

            default: ;

        }

    } while (er.etype != ami_etterm); /* until dismissed or terminated */

    /* kill the dialog window */
    fclose(out);

    /* a terminate from outside must end the program, not just this dialog */
    endmodal(&er, realterm);

}

/** ****************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

*******************************************************************************/

static void iquerycolor(
    /** Input/Output for red ratioed color */   ami_long* r,
    /** Input/Output for green ratioed color */ ami_long* g,
    /** Input/Output for blue ratioed color */  ami_long* b
)

{
    int        realterm; /* a terminate arrived from outside */

    realterm = FALSE; /* set no outside terminate */

    FILE*         in = NULL;  /* window to create */
    FILE*         out;
    ami_long      wid;      /* window number */
    ami_evtrec     er;       /* event record */
    char*         title = "Select a color"; /* title string */
    char*         cancel = "Cancel"; /* cancel string */
    char*         selects = "Select"; /* select string */
    ami_long      titbot;   /* bottom of title bar */
    const double  mg = 0.15; /* button to side margin fraction */
    ami_long      mgt;      /* margin for system bar */
    wigptr        wp;       /* widget entry pointer */
    const double  gtop = 0.65; /* color grid top */
    ami_long      gtopp;
    const double  gside = 0.5; /* color grid side */
    ami_long      gsidep;
    const double  ggapv = 0.1; /* color gap between buttons vertical */
    ami_long      ggapvp;
    const double  ggaph = 0.1; /* color gap between buttons horizontal */
    ami_long      ggaphp;
    const double  ggap = 0.5; /* color to b&w grid gap */
    ami_long      ggapp;
    ami_long      cbx, cby; /* color button size */
    ami_long      rw, cl;   /* row and collumn */
    themeindex    th; /* theme index */
    ami_long      wn; /* widget number */
    ami_long      cusy; /* location of "custom" message */
    ami_long      rs, gs, bs; /* colors selected */
    ami_ulong rgb; /* packed color selected */
    ami_long      cursel; /* currently selected color widget */
    ami_long      mpy; /* mouse position */
    ami_long      sx, sy; /* screen center */
    ami_long      wpx, wpy; /* window position in parent */
    ami_long      x, y;

    /* colors for cancel button */
    ccolor cancel_cbc = {

        themetable[th_cancelbackfocus], /* background normal */
        themetable[th_cancelbackfocus], /* background pressed */
        themetable[th_canceloutline],   /* outline normal */
        themetable[th_canceloutline],   /* outline focused */
        themetable[th_canceltextfocus], /* text normal */
        themetable[th_canceltextfocus], /* text disabled */
        themetable[th_title]            /* surround: the system bar */

    };

    /* colors for select button */
    ccolor select_cbc = {

        themetable[th_selectbackfocus],    /* background normal */
        themetable[th_selectbackfocus],    /* background pressed */
        themetable[th_selectoutline],      /* outline normal */
        themetable[th_selectoutlinefocus], /* outline focused */
        themetable[th_selecttextfocus],    /* text normal */
        themetable[th_selecttextfocus],    /* text disabled */
        themetable[th_title]               /* surround: the system bar */

    };

    /* colors for "+" button */
    ccolor plus_cbc = {

        themetable[th_plusbackfocus],    /* background normal */
        themetable[th_plusbackfocus],    /* background pressed */
        themetable[th_plusoutline],      /* outline normal */
        themetable[th_plusoutlinefocus], /* outline focused */
        themetable[th_plustextfocus],    /* text normal */
        themetable[th_plustextfocus],    /* text disabled */
        RGB(255, 255, 255)               /* surround: the dialog body */

    };

    /* black/white map for color button checkboxes, on is white */
    ami_long bwmap[36] = {

        TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,
        TRUE,FALSE,FALSE,FALSE,TRUE,TRUE,FALSE,TRUE,FALSE,
        TRUE,TRUE,FALSE,TRUE,TRUE,TRUE,TRUE,TRUE,FALSE,
        TRUE,TRUE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE

    };


    wid = ami_getwinid(); /* get anonymous window id */
    in = NULL; /* a fresh input side, not a shared one */
    ami_openwin(&in, &out, NULL, wid); /* create window */
    ami_buffer(out, FALSE); /* turn off buffering */
    ami_auto(out, FALSE); /* turn off auto */
    ami_curvis(out, FALSE); /* turn off cursor */
    setwigfont(out); /* set sign font */
    ami_binvis(out); /* no background write */
    ami_frame(out, FALSE); /* turn off sizing bars */
    /* size the dialog, with room for the bottom button row */
    ami_setsizg(out, ami_chrsizy(out)*25.8,
                    ami_chrsizy(out)*17.4);

    /* the first paint is requested, not left to fate (see ialert) */
    er.etype = ami_etredraw;
    er.rsx = 1;
    er.rsy = 1;
    er.rex = ami_maxxg(out);
    er.rey = ami_maxyg(out);
    ami_sendevent(out, &er);


    /* center the dialog */
    ami_scnceng(out, &sx, &sy); /* find screen center */
    ami_getsizg(out, &x, &y); /* find window size */
    wpx = sx-x/2; /* set window position */
    wpy = sy-y/2;
    ami_setposg(out, wpx, wpy); /* set center position */

    titbot = ami_chrsizy(out)*1.6; /* slim Breeze title bar */
    mgt = ami_chrsizy(out)*0.5; /* set margins */

    /* The buttons sit at the bottom right in the Qt order, the
       affirmative left of Cancel */
    /* place cancel button, rightmost */
    wp = getwig(); /* get widget entry */
    wp->cbc = &cancel_cbc; /* set colors */
    widget(out, ami_maxxg(out)-mgt-ami_strsiz(out, cancel)-ami_chrsizy(out)*1.5,
                ami_maxyg(out)-ami_chrsizy(out)*1.9,
                ami_maxxg(out)-mgt,
                ami_maxyg(out)-ami_chrsizy(out)*0.4,
                cancel, 1, wtcbutton, &wp);

    /* place select button to its left */
    wp = getwig(); /* get widget entry */
    wp->cbc = &select_cbc; /* set colors */
    widget(out, ami_maxxg(out)-mgt-ami_strsiz(out, cancel)-ami_chrsizy(out)*2.0-
                ami_strsiz(out, selects)-ami_chrsizy(out)*1.5,
                ami_maxyg(out)-ami_chrsizy(out)*1.9,
                ami_maxxg(out)-mgt-ami_strsiz(out, cancel)-ami_chrsizy(out)*2.0,
                ami_maxyg(out)-ami_chrsizy(out)*0.4,
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
        (ami_maxyg(out)-(titbot+gtopp*2+ggaphp*2+ggapp+ami_chrsizy(out)*4))/5;

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
            /* the surround: the swatches sit on the dialog body */
            wp->cbc->bsr = RGB(255, 255, 255);
            if (bwmap[th-th_querycolor1]) { /* set white checkmark */

                wp->cbc->btn = BW(255);
                wp->cbc->btd = BW(255);

            } else {

                wp->cbc->btn = themetable[th_selecttextfocus];
                wp->cbc->btd = themetable[th_selecttextfocus];

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

    mpy = 0; /* no tracked pointer position yet */

    /* start with events */
    do {

        ami_event(in, &er);
        /* a terminate here came from outside, since the dialog makes its own
           only further down; remember it so it can be passed on */
        if (er.etype == ami_etterm) realterm = TRUE;
        switch (er.etype) {

            case ami_etredraw:
                /* draw background */
                ami_fcolor(out, ami_backcolor);
                ami_frect(out, 1, 1, ami_maxxg(out), ami_maxyg(out));
                /* draw system bar */
                fcolort(out, th_title);
                ami_frect(out, 1, 1, ami_maxxg(out), titbot);
                /* draw title */
                fcolort(out, th_text);
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
                mpy = er.moupyg; /* track pointer y for title-bar hit test */
                break;

            case ami_etmouba:
                /* Press on the title bar: hand the drag to the graphics layer,
                   which tracks in desktop coordinates with the pointer grabbed
                   (see the other query dialogs). */
                if (er.amoubn == 1 && mpy <= titbot) ami_dragwin(out);
                break;

            default: ;

        }

    } while (er.etype != ami_etterm); /* until terminate */
    
    /* a terminate from outside must end the program, not just
       this dialog */
    endmodal(&er, realterm);

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

The result is returned in a critical buffer: if it exactly fills the buffer,
the terminating zero is left off, and it is an error if the result cannot fit
in the buffer.

*******************************************************************************/

/*
 * Widget IDs for file open/save dialogs.
 */
#define QFL_ID_CANCEL   1
#define QFL_ID_OK       2
#define QFL_ID_PATH     3
#define QFL_ID_LIST     4
#define QFL_ID_NAME     5
#define QFL_ID_PLACES   6 /* the places at the left */

/* Rebuild the file list for the directory now current. The widget is
   made anew rather than refilled, which is what the widget set gives;
   killing it frees the old list, so the old pointer must not be touched
   after. Kept in one place because the geometry must match the layout
   below wherever the list is rebuilt. */
#define QFL_RELIST() do { \
    ami_killwidget(out, QFL_ID_LIST); \
    listsp = build_qfl_list(curdir); \
    wp = getwig(); \
    wp->strlst = listsp; \
    wp->tabs[0] = -chrw*47; \
    wp->tabs[1] = chrw*49; \
    wp->ntabs = 2; \
    widget(out, chrw*16, titbot+chrsz*3.4, chrw*78, titbot+chrsz*19.0, \
           "", QFL_ID_LIST, wtlistbox, &wp); \
} while (0)

/*
 * Build a listbox string list for directory `dir`. Directories get a trailing
 * "/" to distinguish them visually, and ".." is always first.
 * Returns a list suitable to pass to widget(...wtlistbox...).
 * Caller must pass the returned list to free_qfl_list() when done.
 */
/* The places: the directories a file is most often wanted from. The
   home directory and the standard folders under it, each shown by its
   own name and carrying its path. */
static const char* qfl_places[] = {

    "Home", "", "Desktop", "Desktop", "Documents", "Documents",
    "Downloads", "Downloads", "Music", "Music", "Pictures", "Pictures",
    "Videos", "Videos", NULL, NULL

};

static ami_strptr build_qfl_places(void)

{

    ami_strptr head = NULL, tail = NULL, e;
    ami_long i;

    for (i = 0; qfl_places[i]; i += 2) {

        e = malloc(sizeof(ami_strrec));
        if (!e) break;
        e->next = NULL;
        e->str = strdup(qfl_places[i]);
        if (!head) head = e; else tail->next = e;
        tail = e;

    }

    return (head);

}

/* the path a place stands for */
static void qfl_placepath(ami_long n, char* d, ami_long dl)

{

    const char* home = getenv("HOME");

    if (!home) home = "/";
    if (n < 1) { strncpy(d, home, dl-1); d[dl-1] = 0; return; }
    if (!qfl_places[(n-1)*2]) { strncpy(d, home, dl-1); d[dl-1] = 0; return; }
    snprintf(d, dl, "%s%s%s", home, qfl_places[(n-1)*2+1][0]? "/": "",
             qfl_places[(n-1)*2+1]);

}

/* An entry as the list shows it: the name, then its size and the date it
   was last changed, in columns. The list is set in the fixed pitch font,
   so the columns line up under their headings. */
#define QFL_NAMEW 32 /* the name column, in characters */

static void qfl_entry(char* d, ami_long dl, const char* name, int isdir,
                      long long size, ami_long modify)

{

    char   szs[32];
    char   dts[32];
    char   tms[32];
    ami_long   i;

    /* the name, and a directory marked as one */
    for (i = 0; i < QFL_NAMEW && name[i]; i++) d[i] = name[i];
    if (name[i]) { d[QFL_NAMEW-3] = '.'; d[QFL_NAMEW-2] = '.';
                   d[QFL_NAMEW-1] = '.'; i = QFL_NAMEW; }
    if (isdir) d[i++] = '/';
    d[i++] = '\t'; /* the columns are placed by the list box */
    /* the size, in the units it reads best in; a directory has none */
    if (isdir) strcpy(szs, "");
    else if (size < 1000LL) sprintf(szs, "%lld B", size);
    else if (size < 1000000LL) sprintf(szs, "%.1f kB", size/1000.0);
    else if (size < 1000000000LL) sprintf(szs, "%.1f MB", size/1000000.0);
    else sprintf(szs, "%.1f GB", size/1000000000.0);
    strcpy(d+i, szs);
    i += strlen(szs);
    d[i++] = '\t';
    /* When it was last changed, said the way a file list says it: the
       time alone for today, Yesterday for the day before, the day and
       month within the year, and the year as well for anything older.
       The times here are the services module's own, not the C
       library's, and read as the C library's every file landed in
       another century. */
    if (modify) {

        ami_long lt = ami_local(modify);
        ami_long now = ami_local(ami_time());
        ami_long day = 24L*60*60; /* a day: these times count in seconds */
        char yr[32]; /* the formatter writes a whole date, not four
                        characters: a short buffer is an error to it */

        ami_dates(dts, sizeof(dts), lt);
        ami_times(tms, sizeof(tms), lt);
        if (strlen(tms) > 5) strcpy(tms+5, tms+8); /* drop the seconds */
        ami_dates(yr, sizeof(yr), now);
        yr[4] = 0; /* this year, to compare against */
        if (now >= lt && now-lt < day) snprintf(d+i, dl-i, "%s", tms);
        else if (now >= lt && now-lt < 2*day)
            snprintf(d+i, dl-i, "Yesterday");
        else if (!strncmp(dts, yr, 4)) /* this year: no need to say which */
            snprintf(d+i, dl-i, "%s", dts+5);
        else snprintf(d+i, dl-i, "%s", dts);

    } else d[i] = 0; /* nothing to say, as for .. */

}

/* The name out of a list entry, which carries its columns: the name
   fills the first field, padded, and a directory ends with a slash.
   Returns whether the entry is a directory. */
static int qfl_name(const char* e, char* d, ami_long dl)

{

    ami_long i = 0, isdir;

    while (e[i] && e[i] != '\t' && i < dl-1) { d[i] = e[i]; i++; }
    d[i] = 0;
    isdir = i > 0 && d[i-1] == '/';
    if (isdir) d[i-1] = 0; /* the slash marks it, it is not in the name */

    return (isdir);

}

/* directories before files, and each in name order, which is how a file
   list is expected to arrive */
static int qfl_before(const char* an, int ad, const char* bn, int bd)

{

    if (ad != bd) return (ad); /* a directory leads a file */

    return (strcasecmp(an, bn) < 0);

}

static ami_strptr build_qfl_list(const char* dir) {

    ami_filrec *files = NULL;
    ami_strptr  head = NULL;
    ami_strptr  tail = NULL;
    char        pat[4096];
    char        line[256];
    ami_long    dl;

    /* always include ".." for navigation */
    head = malloc(sizeof(ami_strrec));
    head->next = NULL;
    qfl_entry(line, sizeof(line), "..", TRUE, 0, 0);
    head->str  = strdup(line);
    tail = head;

    /* ami_list() treats its argument as dir + wildcard-filename. To list
       everything in `dir` we must append "/ *" (no space). */
    dl = (ami_long)strlen(dir);
    if (dl >= (ami_long)sizeof(pat) - 3) dl = sizeof(pat) - 3;
    memcpy(pat, dir, dl); pat[dl] = 0;
    if (dl == 0 || pat[dl-1] != '/') strcat(pat, "/");
    strcat(pat, "*");
    ami_list(pat, &files);
    for (ami_filrec *fp = files; fp; fp = fp->next) {
        ami_long is_dir;
        ami_strptr e, lp;
        if (!fp->name) continue;
        /* the hidden entries stay hidden, as a file dialog shows them
           only when asked; .. is already in the list */
        if (fp->name[0] == '.') continue;
        is_dir = !!(fp->attr & (1L << ami_atdir));
        qfl_entry(line, sizeof(line), fp->name, is_dir, fp->size, fp->modify);
        e = malloc(sizeof(ami_strrec));
        e->next = NULL;
        e->str  = strdup(line);
        /* in place, after the entries that come before it. The first
           entry is always .., which nothing displaces */
        lp = head;
        while (lp->next) {

            char  onam[512];
            int   odir = qfl_name(lp->next->str, onam, sizeof(onam));
            char  nnam[512];

            qfl_name(e->str, nnam, sizeof(nnam));
            if (qfl_before(nnam, is_dir, onam, odir)) break;
            lp = lp->next;

        }
        e->next = lp->next;
        lp->next = e;
        if (tail == lp) tail = e;
    }

    /* NOTE: ami_list's returned records are owned by services; we do not
       attempt to free them here to avoid assuming an allocator contract. */

    return head;

}

static void free_qfl_list(ami_strptr sp) {
    while (sp) {
        ami_strptr n = sp->next;
        if (sp->str) free(sp->str);
        free(sp);
        sp = n;
    }
}

/*
 * Join a directory and a filename into dst of size dstsz. If dir already ends
 * with '/', no extra separator is added.
 */
static void join_path(char* dst, ami_long dstsz, const char* dir, const char* fn) {
    ami_long dl = (ami_long)strlen(dir);
    ami_long need_sep = (dl > 0 && dir[dl-1] != '/');
    if (dstsz <= 0) return;
    if (!need_sep) {
        snprintf_or_copy: ;
        if (dl >= dstsz) dl = dstsz - 1;
        memcpy(dst, dir, dl);
        dst[dl] = 0;
        strncat(dst, fn, (size_t)(dstsz - 1 - dl));
    } else {
        ami_long fl;
        if (dl >= dstsz) dl = dstsz - 1;
        memcpy(dst, dir, dl);
        dst[dl] = 0;
        if (dl + 1 < dstsz) { dst[dl] = '/'; dst[dl+1] = 0; dl++; }
        fl = dstsz - 1 - dl;
        if (fl > 0) strncat(dst, fn, (size_t)fl);
    }
}

/*
 * Normalize a directory path, resolving a trailing "..".
 * Input: dir ends (ideally) with '/'. Output: dst gets the normalized path.
 * Very simple: if dir ends in "../" (or ".."), strip the last component.
 */
static void normalize_dir(char* dst, ami_long dstsz, const char* dir) {
    ami_long dl = (ami_long)strlen(dir);
    ami_long i;
    if (dstsz <= 0) return;
    /* copy in */
    if (dl >= dstsz) dl = dstsz - 1;
    memcpy(dst, dir, dl);
    dst[dl] = 0;
    /* remove trailing slash if any */
    while (dl > 1 && dst[dl-1] == '/') { dst[--dl] = 0; }
    /* check for trailing "/.." */
    if (dl >= 3 && dst[dl-1] == '.' && dst[dl-2] == '.' &&
        (dl == 2 || dst[dl-3] == '/')) {
        /* strip the ".." */
        dl -= 2; dst[dl] = 0;
        /* strip trailing slash */
        while (dl > 1 && dst[dl-1] == '/') { dst[--dl] = 0; }
        /* strip parent component */
        i = dl - 1;
        while (i >= 0 && dst[i] != '/') i--;
        if (i < 0) {
            /* nothing left, use "." */
            dst[0] = '.'; dst[1] = 0;
        } else if (i == 0) {
            dst[1] = 0; /* keep root "/" */
        } else {
            dst[i] = 0;
        }
    }
    /* ensure non-empty */
    if (dst[0] == 0) { dst[0] = '.'; dst[1] = 0; }
}

/*
 * Common implementation for iqueryopen / iquerysave. `title` chooses the
 * label shown in the title bar ("Open" vs "Save As").
 */
static void qfl_dialog(char* s, ami_long sl, const char* title) {

    FILE*      in = NULL;
    FILE*      out;
    ami_long   wid;
    ami_evtrec er;
    int        realterm; /* a terminate arrived from outside */
    wigptr     wp;
    ami_long   chrsz;
    ami_long   titbot;
    ami_long   mpy;
    ami_long   sx, sy, x, y, wpx, wpy;
    ami_long   cancelled;
    char       curdir[4096];
    char       curfile[512];
    ami_strptr listsp;
    ami_strptr plcsp;
    ami_long   chrw; /* character width, for the horizontal */
    char       tmpbuf[4096];
    ami_long   i;

    ccolor cancel_cbc = {
        themetable[th_cancelbackfocus], themetable[th_cancelbackfocus],
        themetable[th_canceloutline],   themetable[th_canceloutline],
        themetable[th_canceltextfocus], themetable[th_canceltextfocus],
        themetable[th_title]
    };
    ccolor select_cbc = {
        themetable[th_selectbackfocus],    themetable[th_selectbackfocus],
        themetable[th_selectoutline],      themetable[th_selectoutlinefocus],
        themetable[th_selecttextfocus],    themetable[th_selecttextfocus],
        themetable[th_title]
    };

    /* split input s into directory + filename */
    curdir[0] = 0; curfile[0] = 0;
    if (s && s[0]) {
        /* find last slash */
        ami_long slash = -1;
        ami_long l = (ami_long)strlen(s);
        for (i = l - 1; i >= 0; i--) if (s[i] == '/') { slash = i; break; }
        if (slash < 0) {
            strncpy(curfile, s, sizeof(curfile)-1); curfile[sizeof(curfile)-1]=0;
        } else {
            ami_long dl = slash;
            if (dl == 0) { curdir[0] = '/'; curdir[1] = 0; }
            else { if (dl >= (ami_long)sizeof(curdir)) dl = sizeof(curdir)-1;
                   memcpy(curdir, s, dl); curdir[dl] = 0; }
            strncpy(curfile, s+slash+1, sizeof(curfile)-1);
            curfile[sizeof(curfile)-1] = 0;
        }
    }
    if (curdir[0] == 0) { curdir[0] = '.'; curdir[1] = 0; }

    wid = ami_getwinid();
    in = NULL; /* a fresh input side, not a shared one */
    ami_openwin(&in, &out, NULL, wid);
    ami_buffer(out, FALSE);
    ami_auto(out, FALSE);
    ami_curvis(out, FALSE);
    setwigfont(out);
    ami_binvis(out);
    ami_frame(out, FALSE);

    chrsz = ami_chrsizy(out);
    /* A slim Breeze title bar; the buttons sit at the bottom right */
    titbot = chrsz*1.6;
    /* The horizontal is laid out in character widths and the vertical in
       character heights. Both were the height before, so every width came
       out twice what it read as: the dialog was half again wider than the
       screen it had to fit. */
    chrw = ami_strsiz(out, "0");

    ami_setsizg(out, chrw*79, titbot+chrsz*26.2);

    ami_scnceng(out, &sx, &sy);
    ami_getsizg(out, &x, &y);
    wpx = sx-x/2; wpy = sy-y/2;
    ami_setposg(out, wpx, wpy);

    /* path edit box: current directory */
    wp = getwig();
    widget(out, chrw*6, titbot+chrsz*0.6,
                chrw*78, titbot+chrsz*2.0, "", QFL_ID_PATH, wteditbox, &wp);
    ami_putwidgettext(out, QFL_ID_PATH, curdir);

    /* The places at the left: the directories a file is most often
       wanted from, which saves typing a path to reach them. */
    plcsp = build_qfl_places();
    wp = getwig();
    wp->strlst = plcsp;
    widget(out, chrw, titbot+chrsz*3.4,
                chrw*15, titbot+chrsz*19.0, "", QFL_ID_PLACES, wtlistbox, &wp);

    /* file list box. Set wp->strlst BEFORE widget() so that any draws
       during construction see a populated list. The widget will free the
       list automatically when it is killed (via putwig/frestrlst). */
    listsp = build_qfl_list(curdir);
    wp = getwig();
    wp->strlst = listsp;
    wp->tabs[0] = -chrw*47; /* the size, right aligned as sizes read */
    wp->tabs[1] = chrw*49;  /* the date */
    wp->ntabs = 2;
    widget(out, chrw*16, titbot+chrsz*3.4,
                chrw*78, titbot+chrsz*19.0, "", QFL_ID_LIST, wtlistbox, &wp);

    /* filename edit box */
    wp = getwig();
    widget(out, chrw*6, titbot+chrsz*19.8,
                chrw*78, titbot+chrsz*21.2, "", QFL_ID_NAME, wteditbox, &wp);
    if (curfile[0]) ami_putwidgettext(out, QFL_ID_NAME, curfile);

    /* the buttons at the bottom right, the affirmative left of Cancel */
    /* OK button */
    wp = getwig();
    wp->cbc = &select_cbc;
    widget(out, chrw*58, titbot+chrsz*24.4, chrw*67, titbot+chrsz*25.9,
                title[0] == 'S'? "Save": "Open",
                QFL_ID_OK, wtcbutton, &wp);

    /* Cancel button */
    wp = getwig();
    wp->cbc = &cancel_cbc;
    widget(out, chrw*69, titbot+chrsz*24.4, chrw*78, titbot+chrsz*25.9,
                "Cancel", QFL_ID_CANCEL, wtcbutton, &wp);

    mpy = 0;
    cancelled = FALSE;
    realterm = FALSE; /* set no outside terminate */

    do {

        ami_event(in, &er);
        /* a terminate here came from outside, since the dialog makes its own
           only further down; remember it so it can be passed on */
        if (er.etype == ami_etterm) realterm = TRUE;
        switch (er.etype) {

            case ami_etredraw:
                ami_fcolor(out, ami_backcolor);
                ami_frect(out, 1, 1, ami_maxxg(out), ami_maxyg(out));
                fcolort(out, th_title);
                ami_frect(out, 1, 1, ami_maxxg(out), titbot);
                fcolort(out, th_text);
                ami_bold(out, TRUE);
                /* centered between the buttons at the ends */
                ami_cursorg(out, ami_maxxg(out)/2-
                            ami_strsiz(out, title)/2,
                            titbot*0.5 - chrsz*0.5);
                fputs(title, out);
                ami_bold(out, FALSE);
                ami_fcolor(out, ami_black);
                ami_cursorg(out, chrw, titbot+chrsz*0.9);
                fputs("Path:", out);
                ami_cursorg(out, chrw, titbot+chrsz*20.1);
                fputs("Name:", out);
                /* the headings over the two lists. The file headings are
                   set in the fixed pitch font the list itself uses, so
                   they stand over their columns. */
                ami_bold(out, TRUE);
                ami_cursorg(out, chrw, titbot+chrsz*2.4);
                fputs("Places", out);
                ami_cursorg(out, chrw*16, titbot+chrsz*2.4);
                fputs("Name", out);
                ami_cursorg(out, chrw*16+chrw*47-ami_strsiz(out, "Size"),
                            titbot+chrsz*2.4);
                fputs("Size", out);
                ami_cursorg(out, chrw*16+chrw*49, titbot+chrsz*2.4);
                fputs("Modified", out);
                ami_bold(out, FALSE);
                break;

            case ami_etlstbox:
                if (er.lstbid == QFL_ID_PLACES) { /* to a place */

                    qfl_placepath(er.lstbsl, tmpbuf, sizeof(tmpbuf));
                    normalize_dir(curdir, sizeof(curdir), tmpbuf);
                    QFL_RELIST();
                    ami_putwidgettext(out, QFL_ID_PATH, curdir);

                } else if (er.lstbid == QFL_ID_LIST) {
                    /* find the selected string */
                    ami_strptr sp = listsp;
                    ami_long n = er.lstbsl;
                    for (i = 1; i < n && sp; i++) sp = sp->next;
                    if (sp && sp->str) {
                        char name[512];
                        ami_long is_dir = qfl_name(sp->str, name, sizeof(name));
                        if (is_dir) {
                            /* navigate into this directory */
                            join_path(tmpbuf, sizeof(tmpbuf), curdir, name);
                            normalize_dir(curdir, sizeof(curdir), tmpbuf);
                            /* rebuild listbox. ami_killwidget frees the
                               previous strlst via putwig, so we must not
                               touch the old listsp pointer. */
                            QFL_RELIST();
                            /* update path edit */
                            ami_putwidgettext(out, QFL_ID_PATH, curdir);
                        } else {
                            /* file selected: copy to name edit box */
                            ami_putwidgettext(out, QFL_ID_NAME, name);
                        }
                    }
                }
                break;

            case ami_etedtbox:
                if (er.edtbid == QFL_ID_PATH) {
                    /* user pressed enter in path: reload */
                    getwidgettextz(out, QFL_ID_PATH, curdir, sizeof(curdir));
                    if (curdir[0] == 0) { curdir[0]='.'; curdir[1]=0; }
                    QFL_RELIST();
                } else if (er.edtbid == QFL_ID_NAME) {
                    er.etype = ami_etterm; /* enter in name = OK */
                }
                break;

            case ami_etbutton:
                if (er.butid == QFL_ID_CANCEL) {
                    cancelled = TRUE; er.etype = ami_etterm;
                } else if (er.butid == QFL_ID_OK) {
                    er.etype = ami_etterm;
                }
                break;

            case ami_etenter:
                er.etype = ami_etterm;
                break;

            case ami_etmoumovg:
                mpy = er.moupyg; /* track pointer y for title-bar hit test */
                break;

            case ami_etmouba:
                /* Press on the title bar: hand the drag to the graphics layer.
                   It tracks in desktop coordinates with the pointer grabbed, so
                   the window follows smoothly even when the pointer outruns it
                   and leaves the window's bounds -- a window-relative self-drag
                   cannot, because motion stops being delivered the moment the
                   pointer is no longer over the dialog. */
                if (er.amoubn == 1 && mpy <= titbot) ami_dragwin(out);
                break;

            default: ;

        }

    } while (er.etype != ami_etterm);
    
    /* a terminate from outside must end the program, not just
       this dialog */
    endmodal(&er, realterm);

    if (cancelled) {
        if (s && sl > 0) s[0] = 0;
    } else if (s && sl > 0) {
        /* read both edit boxes fresh */
        getwidgettextz(out, QFL_ID_PATH, curdir,  sizeof(curdir));
        getwidgettextz(out, QFL_ID_NAME, curfile, sizeof(curfile));
        if (curfile[0] == 0) {
            s[0] = 0;                              /* no filename given */
        } else {
            /* build the full result locally, then copy to the critical
               result buffer */
            if (curfile[0] == '/')
                strcpy(tmpbuf, curfile);           /* absolute path */
            else if (curdir[0] == 0 ||
                     (curdir[0] == '.' && curdir[1] == 0))
                strcpy(tmpbuf, curfile);           /* CWD relative */
            else
                join_path(tmpbuf, sizeof(tmpbuf), curdir, curfile);
            cpycrit(s, sl, tmpbuf);
        }
    }

    /* fclose tears down all child widgets; their string lists are freed
       inside putwig via frestrlst. Do NOT touch listsp here. */
    fclose(out);

}

static void iqueryopen(
    /** Input/output for filename string */ char* s,
    /** Length of filename string buffer */ ami_long sl
)

{

    qfl_dialog(s, sl, "Open");

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

The result is returned in a critical buffer: if it exactly fills the buffer,
the terminating zero is left off, and it is an error if the result cannot fit
in the buffer.

*******************************************************************************/

static void iquerysave(
    /** Input/output for filename string */ char* s,
    /** Length of filename string buffer */ ami_long sl
)

{

    qfl_dialog(s, sl, "Save As");

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

The search string is returned in a critical buffer: if it exactly fills the
buffer, the terminating zero is left off, and it is an error if it cannot fit
in the buffer.

Bug: should return null string on cancel. Unlike other dialogs, windows
provides no indication of if the cancel button was pushed. To do this, we
would need to hook (or subclass) the find dialog.

After note: tried hooking the window. The issue is that the cancel button is
just a simple button that gets pressed. Trying to rely on the button id
sounds very system dependent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

*******************************************************************************/

/*
 * Widget IDs used within the Find dialog.
 */
#define QFN_ID_CANCEL   1
#define QFN_ID_FIND     2
#define QFN_ID_EDIT     3
#define QFN_ID_CASE     4
#define QFN_ID_UP       5
#define QFN_ID_RE       6

static void iqueryfind(
    /** Input/output for search string */   char* s,
    /** Length of search string buffer */ ami_long sl,
    /** Set of find/replace options */      ami_qfnopts* opt
)

{
    int        realterm; /* a terminate arrived from outside */

    realterm = FALSE; /* set no outside terminate */

    FILE*      in = NULL; /* window to create */
    FILE*      out;
    ami_long   wid;       /* window number */
    ami_evtrec er;        /* event record */
    char*      title = "Find";
    wigptr     wp;
    ami_long   chrsz;     /* character height in pixels */
    ami_long   titbot;    /* bottom of title bar */
    ami_long   mpy;  /* mouse position */
    ami_long   sx, sy, x, y, wpx, wpy;
    ami_long   case_on, up_on, re_on; /* checkbox states */
    ami_long   cancelled;

    /* colors for Cancel button */
    ccolor cancel_cbc = {
        themetable[th_cancelbackfocus], themetable[th_cancelbackfocus],
        themetable[th_canceloutline],   themetable[th_canceloutline],
        themetable[th_canceltextfocus], themetable[th_canceltextfocus],
        themetable[th_title]
    };
    /* colors for Find Next button */
    ccolor select_cbc = {
        themetable[th_selectbackfocus],    themetable[th_selectbackfocus],
        themetable[th_selectoutline],      themetable[th_selectoutlinefocus],
        themetable[th_selecttextfocus],    themetable[th_selecttextfocus],
        themetable[th_title]
    };

    /* create the dialog window */
    wid = ami_getwinid();
    in = NULL; /* a fresh input side, not a shared one */
    ami_openwin(&in, &out, NULL, wid);
    ami_buffer(out, FALSE);
    ami_auto(out, FALSE);
    ami_curvis(out, FALSE);
    setwigfont(out);
    ami_binvis(out);
    ami_frame(out, FALSE);

    chrsz = ami_chrsizy(out);
    titbot = chrsz*1.6;

    /* set size */
    ami_setsizg(out, chrsz*34, titbot+chrsz*9);

    /* the first paint is requested, not left to fate (see ialert) */
    er.etype = ami_etredraw;
    er.rsx = 1;
    er.rsy = 1;
    er.rex = ami_maxxg(out);
    er.rey = ami_maxyg(out);
    ami_sendevent(out, &er);


    /* center on screen */
    ami_scnceng(out, &sx, &sy);
    ami_getsizg(out, &x, &y);
    wpx = sx-x/2; wpy = sy-y/2;
    ami_setposg(out, wpx, wpy);

    /* edit box for search text */
    wp = getwig();
    widget(out, chrsz*7, titbot+chrsz*0.6,
                chrsz*23, titbot+chrsz*2.0, "", QFN_ID_EDIT, wteditbox, &wp);
    if (s && s[0]) ami_putwidgettext(out, QFN_ID_EDIT, s);

    /* the buttons at the bottom right, the affirmative left of Cancel */
    /* Find Next button */
    wp = getwig();
    wp->cbc = &select_cbc;
    widget(out, chrsz*13, titbot+chrsz*7.2,
                chrsz*23, titbot+chrsz*8.6, "Find Next",
                QFN_ID_FIND, wtcbutton, &wp);

    /* Cancel button */
    wp = getwig();
    wp->cbc = &cancel_cbc;
    widget(out, chrsz*24, titbot+chrsz*7.2,
                chrsz*33, titbot+chrsz*8.6, "Cancel",
                QFN_ID_CANCEL, wtcbutton, &wp);

    /* Match case checkbox */
    wp = getwig();
    widget(out, chrsz, titbot+chrsz*3.0,
                chrsz*12, titbot+chrsz*4.0, "Match case",
                QFN_ID_CASE, wtcheckbox, &wp);
    case_on = !!(*opt & (1 << ami_qfncase));
    if (case_on) ami_selectwidget(out, QFN_ID_CASE, TRUE);

    /* Search up checkbox */
    wp = getwig();
    widget(out, chrsz, titbot+chrsz*4.2,
                chrsz*12, titbot+chrsz*5.2, "Search up",
                QFN_ID_UP, wtcheckbox, &wp);
    up_on = !!(*opt & (1 << ami_qfnup));
    if (up_on) ami_selectwidget(out, QFN_ID_UP, TRUE);

    /* Regular expression checkbox */
    wp = getwig();
    widget(out, chrsz, titbot+chrsz*5.4,
                chrsz*17, titbot+chrsz*6.4, "Regular expression",
                QFN_ID_RE, wtcheckbox, &wp);
    re_on = !!(*opt & (1 << ami_qfnre));
    if (re_on) ami_selectwidget(out, QFN_ID_RE, TRUE);

    mpy = 0;
    cancelled = FALSE;

    /* event loop */
    do {

        ami_event(in, &er);
        /* a terminate here came from outside, since the dialog makes its own
           only further down; remember it so it can be passed on */
        if (er.etype == ami_etterm) realterm = TRUE;
        switch (er.etype) {

            case ami_etredraw:
                /* background */
                ami_fcolor(out, ami_backcolor);
                ami_frect(out, 1, 1, ami_maxxg(out), ami_maxyg(out));
                /* title bar */
                fcolort(out, th_title);
                ami_frect(out, 1, 1, ami_maxxg(out), titbot);
                fcolort(out, th_text);
                ami_bold(out, TRUE);
                ami_cursorg(out, chrsz, titbot*0.5 - chrsz*0.5);
                fputs(title, out);
                ami_bold(out, FALSE);
                /* "Find what:" label */
                ami_fcolor(out, ami_black);
                ami_cursorg(out, chrsz, titbot+chrsz*0.9);
                fputs("Find what:", out);
                break;

            case ami_etchkbox:
                /* toggle local state on checkbox click */
                if (er.ckbxid == QFN_ID_CASE) {
                    case_on = !case_on;
                    ami_selectwidget(out, QFN_ID_CASE, case_on);
                } else if (er.ckbxid == QFN_ID_UP) {
                    up_on = !up_on;
                    ami_selectwidget(out, QFN_ID_UP, up_on);
                } else if (er.ckbxid == QFN_ID_RE) {
                    re_on = !re_on;
                    ami_selectwidget(out, QFN_ID_RE, re_on);
                }
                break;

            case ami_etbutton:
                if (er.butid == QFN_ID_CANCEL) {
                    cancelled = TRUE;
                    er.etype = ami_etterm;
                } else if (er.butid == QFN_ID_FIND) {
                    er.etype = ami_etterm;
                }
                break;

            case ami_etedtbox:
                if (er.edtbid == QFN_ID_EDIT) er.etype = ami_etterm;
                break;

            case ami_etenter:
                er.etype = ami_etterm;
                break;

            case ami_etmoumovg:
                mpy = er.moupyg; /* track pointer y for title-bar hit test */
                break;

            case ami_etmouba:
                /* Press on the title bar: hand the drag to the graphics layer.
                   It tracks in desktop coordinates with the pointer grabbed, so
                   the window follows smoothly even when the pointer outruns it
                   and leaves the window's bounds -- a window-relative self-drag
                   cannot, because motion stops being delivered the moment the
                   pointer is no longer over the dialog. */
                if (er.amoubn == 1 && mpy <= titbot) ami_dragwin(out);
                break;

            default: ;

        }

    } while (er.etype != ami_etterm);
    
    /* a terminate from outside must end the program, not just
       this dialog */
    endmodal(&er, realterm);

    /* flow-through result */
    if (cancelled) {
        if (s && sl > 0) s[0] = 0;
    } else {
        if (s && sl > 0) ami_getwidgettext(out, QFN_ID_EDIT, s, sl);
        /* update option flags */
        *opt = 0;
        if (case_on) *opt |= (1 << ami_qfncase);
        if (up_on)   *opt |= (1 << ami_qfnup);
        if (re_on)   *opt |= (1 << ami_qfnre);
    }

    fclose(out);

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

The search and replace strings are returned in critical buffers: if a result
exactly fills its buffer, the terminating zero is left off, and it is an
error if a result cannot fit in its buffer.

Bug: See comment, queryfind.

*******************************************************************************/

/*
 * Widget IDs used within the Find/Replace dialog.
 */
#define QFR_ID_CANCEL   1
#define QFR_ID_FIND     2
#define QFR_ID_REPLACE  3
#define QFR_ID_REPALL   4
#define QFR_ID_EDITS    5
#define QFR_ID_EDITR    6
#define QFR_ID_CASE     7
#define QFR_ID_UP       8
#define QFR_ID_RE       9

static void iqueryfindrep(
    /** Input/output for search string */  char* s,
    /** Length of search string buffer */  ami_long sl,
    /** Input/output for replace string */ char* r,
    /** Length of replace string buffer */ ami_long rl,
    /** Set of find/replace options */     ami_qfropts* opt
)

{
    int        realterm; /* a terminate arrived from outside */

    realterm = FALSE; /* set no outside terminate */

    FILE*      in = NULL;
    FILE*      out;
    ami_long   wid;
    ami_evtrec er;
    char*      title = "Replace";
    wigptr     wp;
    ami_long   chrsz;
    ami_long   titbot;
    ami_long   mpy;
    ami_long   sx, sy, x, y, wpx, wpy;
    ami_long   case_on, up_on, re_on;
    ami_long   cancelled, did_find, did_replall;

    ccolor cancel_cbc = {
        themetable[th_cancelbackfocus], themetable[th_cancelbackfocus],
        themetable[th_canceloutline],   themetable[th_canceloutline],
        themetable[th_canceltextfocus], themetable[th_canceltextfocus],
        themetable[th_title]
    };
    ccolor select_cbc = {
        themetable[th_selectbackfocus],    themetable[th_selectbackfocus],
        themetable[th_selectoutline],      themetable[th_selectoutlinefocus],
        themetable[th_selecttextfocus],    themetable[th_selecttextfocus],
        themetable[th_title]
    };

    wid = ami_getwinid();
    in = NULL; /* a fresh input side, not a shared one */
    ami_openwin(&in, &out, NULL, wid);
    ami_buffer(out, FALSE);
    ami_auto(out, FALSE);
    ami_curvis(out, FALSE);
    setwigfont(out);
    ami_binvis(out);
    ami_frame(out, FALSE);

    chrsz = ami_chrsizy(out);
    titbot = chrsz*1.6;

    ami_setsizg(out, chrsz*45, titbot+chrsz*11);

    /* the first paint is requested, not left to fate (see ialert) */
    er.etype = ami_etredraw;
    er.rsx = 1;
    er.rsy = 1;
    er.rex = ami_maxxg(out);
    er.rey = ami_maxyg(out);
    ami_sendevent(out, &er);


    ami_scnceng(out, &sx, &sy);
    ami_getsizg(out, &x, &y);
    wpx = sx-x/2; wpy = sy-y/2;
    ami_setposg(out, wpx, wpy);

    /* search edit box */
    wp = getwig();
    widget(out, chrsz*9, titbot+chrsz*0.6,
                chrsz*24, titbot+chrsz*2.0, "", QFR_ID_EDITS, wteditbox, &wp);
    if (s && s[0]) ami_putwidgettext(out, QFR_ID_EDITS, s);

    /* replace edit box */
    wp = getwig();
    widget(out, chrsz*9, titbot+chrsz*2.3,
                chrsz*24, titbot+chrsz*3.7, "", QFR_ID_EDITR, wteditbox, &wp);
    if (r && r[0]) ami_putwidgettext(out, QFR_ID_EDITR, r);

    /* the buttons in one row at the bottom right, the affirmatives
       left of Cancel in the Qt order */
    /* Find Next button */
    wp = getwig();
    wp->cbc = &select_cbc;
    widget(out, chrsz*1.0, titbot+chrsz*9.2,
                chrsz*11.0, titbot+chrsz*10.6, "Find Next",
                QFR_ID_FIND, wtcbutton, &wp);

    /* Replace button */
    wp = getwig();
    wp->cbc = &select_cbc;
    widget(out, chrsz*11.5, titbot+chrsz*9.2,
                chrsz*21.5, titbot+chrsz*10.6, "Replace",
                QFR_ID_REPLACE, wtcbutton, &wp);

    /* Replace All button */
    wp = getwig();
    wp->cbc = &select_cbc;
    widget(out, chrsz*22.0, titbot+chrsz*9.2,
                chrsz*33.0, titbot+chrsz*10.6, "Replace All",
                QFR_ID_REPALL, wtcbutton, &wp);

    /* Cancel button */
    wp = getwig();
    wp->cbc = &cancel_cbc;
    widget(out, chrsz*33.5, titbot+chrsz*9.2,
                chrsz*44.0, titbot+chrsz*10.6, "Cancel",
                QFR_ID_CANCEL, wtcbutton, &wp);

    /* Match case checkbox */
    wp = getwig();
    widget(out, chrsz, titbot+chrsz*5.0,
                chrsz*12, titbot+chrsz*6.0, "Match case",
                QFR_ID_CASE, wtcheckbox, &wp);
    case_on = !!(*opt & (1 << ami_qfrcase));
    if (case_on) ami_selectwidget(out, QFR_ID_CASE, TRUE);

    /* Search up checkbox */
    wp = getwig();
    widget(out, chrsz, titbot+chrsz*6.2,
                chrsz*12, titbot+chrsz*7.2, "Search up",
                QFR_ID_UP, wtcheckbox, &wp);
    up_on = !!(*opt & (1 << ami_qfrup));
    if (up_on) ami_selectwidget(out, QFR_ID_UP, TRUE);

    /* Regular expression checkbox */
    wp = getwig();
    widget(out, chrsz, titbot+chrsz*7.4,
                chrsz*17, titbot+chrsz*8.4, "Regular expression",
                QFR_ID_RE, wtcheckbox, &wp);
    re_on = !!(*opt & (1 << ami_qfrre));
    if (re_on) ami_selectwidget(out, QFR_ID_RE, TRUE);

    mpy = 0;
    cancelled = FALSE; did_find = FALSE; did_replall = FALSE;

    do {

        ami_event(in, &er);
        /* a terminate here came from outside, since the dialog makes its own
           only further down; remember it so it can be passed on */
        if (er.etype == ami_etterm) realterm = TRUE;
        switch (er.etype) {

            case ami_etredraw:
                ami_fcolor(out, ami_backcolor);
                ami_frect(out, 1, 1, ami_maxxg(out), ami_maxyg(out));
                fcolort(out, th_title);
                ami_frect(out, 1, 1, ami_maxxg(out), titbot);
                fcolort(out, th_text);
                ami_bold(out, TRUE);
                ami_cursorg(out, chrsz, titbot*0.5 - chrsz*0.5);
                fputs(title, out);
                ami_bold(out, FALSE);
                ami_fcolor(out, ami_black);
                ami_cursorg(out, chrsz, titbot+chrsz*0.9);
                fputs("Find what:", out);
                ami_cursorg(out, chrsz, titbot+chrsz*2.6);
                fputs("Replace with:", out);
                break;

            case ami_etchkbox:
                if (er.ckbxid == QFR_ID_CASE) {
                    case_on = !case_on;
                    ami_selectwidget(out, QFR_ID_CASE, case_on);
                } else if (er.ckbxid == QFR_ID_UP) {
                    up_on = !up_on;
                    ami_selectwidget(out, QFR_ID_UP, up_on);
                } else if (er.ckbxid == QFR_ID_RE) {
                    re_on = !re_on;
                    ami_selectwidget(out, QFR_ID_RE, re_on);
                }
                break;

            case ami_etbutton:
                if (er.butid == QFR_ID_CANCEL) {
                    cancelled = TRUE; er.etype = ami_etterm;
                } else if (er.butid == QFR_ID_FIND) {
                    did_find = TRUE; er.etype = ami_etterm;
                } else if (er.butid == QFR_ID_REPLACE) {
                    er.etype = ami_etterm;
                } else if (er.butid == QFR_ID_REPALL) {
                    did_replall = TRUE; er.etype = ami_etterm;
                }
                break;

            case ami_etenter:
                er.etype = ami_etterm;
                break;

            case ami_etmoumovg:
                mpy = er.moupyg; /* track pointer y for title-bar hit test */
                break;

            case ami_etmouba:
                /* Press on the title bar: hand the drag to the graphics layer.
                   It tracks in desktop coordinates with the pointer grabbed, so
                   the window follows smoothly even when the pointer outruns it
                   and leaves the window's bounds -- a window-relative self-drag
                   cannot, because motion stops being delivered the moment the
                   pointer is no longer over the dialog. */
                if (er.amoubn == 1 && mpy <= titbot) ami_dragwin(out);
                break;

            default: ;

        }

    } while (er.etype != ami_etterm);
    
    /* a terminate from outside must end the program, not just
       this dialog */
    endmodal(&er, realterm);

    if (cancelled) {
        if (s && sl > 0) s[0] = 0;
        if (r && rl > 0) r[0] = 0;
    } else {
        if (s && sl > 0) ami_getwidgettext(out, QFR_ID_EDITS, s, sl);
        if (r && rl > 0) ami_getwidgettext(out, QFR_ID_EDITR, r, rl);
        *opt = 0;
        if (case_on)     *opt |= (1 << ami_qfrcase);
        if (up_on)       *opt |= (1 << ami_qfrup);
        if (re_on)       *opt |= (1 << ami_qfrre);
        if (did_find)    *opt |= (1 << ami_qfrfind);
        if (did_replall) *opt |= (1 << ami_qfrallfil);
    }

    fclose(out);

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
    /** Input/output font code */        ami_long*           fc,
    /** Input/output point size */       ami_long*           s,
    /** Input/output foreground red */   ami_long*           fr,
    /** Input/output foreground green */ ami_long*           fg,
    /** Input/output foreground blue */  ami_long*           fb,
    /** Input/output background red */   ami_long*           br,
    /** Input/output background green */ ami_long*           bg,
    /** Input/output background blue */  ami_long*           bb,
    /** Input/output font effects */     ami_qfteffects* effect
)

{
    int        realterm; /* a terminate arrived from outside */

    realterm = FALSE; /* set no outside terminate */

    FILE*       in = NULL;
    FILE*       out;
    ami_long    wid;
    ami_evtrec  er;
    char*       title = "Font";
    wigptr      wp;
    ami_long    chrsz;
    ami_long    titbot;
    ami_long    mpy;
    ami_long    sx, sy, x, y, wpx, wpy;
    ami_long    cancelled;
    ami_long    strike_on, under_on, bold_on, italic_on;
    ami_long    nfonts, i;
    ami_strptr  fontlist = NULL;
    ami_strptr  fontlist_tail = NULL;
    char        namebuf[256];
    char        fambuf[256];
    ami_long*       fontmap = NULL; /* the font each row stands for */
    ami_long    nrows = 0;      /* rows in the list */
    ami_long    cur_size;
    ami_long    cur_font;
    ami_qfteffects eff_in;

    ccolor cancel_cbc = {
        themetable[th_cancelbackfocus], themetable[th_cancelbackfocus],
        themetable[th_canceloutline],   themetable[th_canceloutline],
        themetable[th_canceltextfocus], themetable[th_canceltextfocus],
        themetable[th_title]
    };
    ccolor select_cbc = {
        themetable[th_selectbackfocus],    themetable[th_selectbackfocus],
        themetable[th_selectoutline],      themetable[th_selectoutlinefocus],
        themetable[th_selecttextfocus],    themetable[th_selecttextfocus],
        themetable[th_title]
    };

    eff_in = effect ? *effect : 0;
    cur_font = fc ? *fc : 1; if (cur_font < 1) cur_font = 1;
    cur_size = s ? *s : 12;  if (cur_size < 1) cur_size = 12;
    strike_on = !!(eff_in & (1 << ami_qftestrikeout));
    under_on  = !!(eff_in & (1 << ami_qfteunderline));
    bold_on   = !!(eff_in & (1 << ami_qftebold));
    italic_on = !!(eff_in & (1 << ami_qfteitalic));

    wid = ami_getwinid();
    in = NULL; /* a fresh input side, not a shared one */
    ami_openwin(&in, &out, NULL, wid);
    ami_buffer(out, FALSE);
    ami_auto(out, FALSE);
    ami_curvis(out, FALSE);
    setwigfont(out);
    ami_binvis(out);
    ami_frame(out, FALSE);

    chrsz = ami_chrsizy(out);
    titbot = chrsz*1.6;

    ami_setsizg(out, chrsz*42, titbot+chrsz*19);

    /* the first paint is requested, not left to fate (see ialert) */
    er.etype = ami_etredraw;
    er.rsx = 1;
    er.rsy = 1;
    er.rex = ami_maxxg(out);
    er.rey = ami_maxyg(out);
    ami_sendevent(out, &er);


    ami_scnceng(out, &sx, &sy);
    ami_getsizg(out, &x, &y);
    wpx = sx-x/2; wpy = sy-y/2;
    ami_setposg(out, wpx, wpy);

    /* build font list */
    nfonts = ami_fonts(out);
    if (nfonts < 1) nfonts = 0;
    /* The names come back as "vendor: family: encoding", of which only the
       family means anything to the user. Two foundries' cut of one family
       is one entry, the first, and fontmap carries the font number each row
       stands for, a row no longer being a font number. */
    fontmap = malloc(sizeof(ami_long)*(nfonts+1));
    for (i = 1; i <= nfonts; i++) {
        ami_strptr e;
        ami_strptr q;
        int seen;

        /* leave room for termination, ami_fontnam() fills a critical buffer */
        ami_fontnam(out, i, namebuf, sizeof(namebuf)-1);
        namebuf[sizeof(namebuf)-1] = 0;
        famof(namebuf, fambuf, sizeof(fambuf));
        if (!*fambuf) continue;
        seen = FALSE;
        for (q = fontlist; q && !seen; q = q->next)
            if (!strcasecmp(q->str, fambuf)) seen = TRUE;
        if (seen) continue;
        e = malloc(sizeof(ami_strrec));
        e->next = NULL;
        e->str  = strdup(fambuf);
        if (!fontlist) fontlist = e; else fontlist_tail->next = e;
        fontlist_tail = e;
        if (fontmap) fontmap[nrows] = i;
        nrows++;
    }
    if (!fontlist) {
        /* degenerate: add a placeholder */
        fontlist = malloc(sizeof(ami_strrec));
        fontlist->next = NULL;
        fontlist->str  = strdup("(no fonts)");
    }

    /* font list box */
    wp = getwig();
    widget(out, chrsz, titbot+chrsz*1.2,
                chrsz*20, titbot+chrsz*8.0, "", 3, wtlistbox, &wp);
    wp->strlst = fontlist;

    /* size edit box (numeric entry) */
    wp = getwig();
    widget(out, chrsz*22, titbot+chrsz*1.2,
                chrsz*30, titbot+chrsz*2.6, "", 4, wteditbox, &wp);
    {
        char sbuf[16];
        ami_long n = cur_size, k = 0, j;
        char tmp[16];
        if (n < 0) { sbuf[k++] = '-'; n = -n; }
        do { tmp[k++] = '0' + (n % 10); n /= 10; } while (n > 0);
        for (j = 0; j < k; j++) sbuf[j] = tmp[k-1-j];
        sbuf[k] = 0;
        ami_putwidgettext(out, 4, sbuf);
    }

    /* Strikeout checkbox */
    wp = getwig();
    widget(out, chrsz*22, titbot+chrsz*3.8,
                chrsz*35, titbot+chrsz*4.8, "Strikeout", 5, wtcheckbox, &wp);
    if (strike_on) ami_selectwidget(out, 5, TRUE);

    /* Underline checkbox */
    wp = getwig();
    widget(out, chrsz*22, titbot+chrsz*5.0,
                chrsz*35, titbot+chrsz*6.0, "Underline", 6, wtcheckbox, &wp);
    if (under_on) ami_selectwidget(out, 6, TRUE);

    /* Bold checkbox */
    wp = getwig();
    widget(out, chrsz*22, titbot+chrsz*6.2,
                chrsz*35, titbot+chrsz*7.2, "Bold", 7, wtcheckbox, &wp);
    if (bold_on) ami_selectwidget(out, 7, TRUE);

    /* Italic checkbox */
    wp = getwig();
    widget(out, chrsz*22, titbot+chrsz*7.4,
                chrsz*35, titbot+chrsz*8.4, "Italic", 8, wtcheckbox, &wp);
    if (italic_on) ami_selectwidget(out, 8, TRUE);

    /* the buttons at the bottom right, OK left of Cancel */
    /* OK button */
    wp = getwig();
    wp->cbc = &select_cbc;
    widget(out, chrsz*22, titbot+chrsz*17.2,
                chrsz*31, titbot+chrsz*18.6, "OK", 2, wtcbutton, &wp);

    /* Cancel button */
    wp = getwig();
    wp->cbc = &cancel_cbc;
    widget(out, chrsz*32, titbot+chrsz*17.2,
                chrsz*41, titbot+chrsz*18.6, "Cancel", 1, wtcbutton, &wp);

    mpy = 0;
    cancelled = FALSE;

    do {

        ami_event(in, &er);
        /* a terminate here came from outside, since the dialog makes its own
           only further down; remember it so it can be passed on */
        if (er.etype == ami_etterm) realterm = TRUE;
        switch (er.etype) {

            case ami_etredraw:
                ami_fcolor(out, ami_backcolor);
                ami_frect(out, 1, 1, ami_maxxg(out), ami_maxyg(out));
                fcolort(out, th_title);
                ami_frect(out, 1, 1, ami_maxxg(out), titbot);
                fcolort(out, th_text);
                ami_bold(out, TRUE);
                ami_cursorg(out, chrsz, titbot*0.5 - chrsz*0.5);
                fputs(title, out);
                ami_bold(out, FALSE);
                ami_fcolor(out, ami_black);
                ami_cursorg(out, chrsz, titbot+chrsz*0.2);
                fputs("Font:", out);
                ami_cursorg(out, chrsz*22, titbot+chrsz*0.2);
                fputs("Size:", out);
                ami_cursorg(out, chrsz, titbot+chrsz*9.0);
                fputs("Sample:", out);
                /* sample box */
                ami_fcolor(out, ami_white);
                ami_frect(out, chrsz, titbot+chrsz*10.0,
                              chrsz*41, titbot+chrsz*14.0);
                ami_fcolor(out, ami_black);
                ami_rect (out, chrsz, titbot+chrsz*10.0,
                              chrsz*41, titbot+chrsz*14.0);
                /* render sample string with current font/style */
                {
                    ami_long save_font = cur_font;
                    ami_font(out, cur_font);
                    ami_fontsiz(out, cur_size);
                    if (bold_on)   ami_bold(out, TRUE);
                    if (italic_on) ami_italic(out, TRUE);
                    if (under_on)  ami_underline(out, TRUE);
                    if (strike_on) ami_strikeout(out, TRUE);
                    ami_cursorg(out, chrsz*1.5, titbot+chrsz*10.8);
                    fputs("AaBbYyZz 0123", out);
                    /* restore defaults */
                    ami_bold(out, FALSE);
                    ami_italic(out, FALSE);
                    ami_underline(out, FALSE);
                    ami_strikeout(out, FALSE);
                    setwigfont(out);
                    ami_fontsiz(out, chrsz);
                    (void)save_font;
                }
                break;

            case ami_etlstbox:
                if (er.lstbid == 3) {
                    /* the row stands for a font, and is not one */
                    if (fontmap && er.lstbsl >= 1 && er.lstbsl <= nrows)
                        cur_font = fontmap[er.lstbsl-1];
                    if (cur_font < 1) cur_font = 1;
                    /* force repaint of sample area */
                    /* queue a redraw event on our dialog window */
                    er.etype = ami_etredraw;
                    ami_sendevent(out, &er);
                    break;
                }
                break;

            case ami_etedtbox:
                if (er.edtbid == 4) {
                    /* user pressed enter in size box: parse number */
                    char sbuf[32];
                    ami_long n, neg, j;
                    getwidgettextz(out, 4, sbuf, sizeof(sbuf));
                    n = 0; neg = 0; j = 0;
                    if (sbuf[0] == '-') { neg = 1; j = 1; }
                    while (sbuf[j] >= '0' && sbuf[j] <= '9') {
                        n = n*10 + (sbuf[j] - '0'); j++;
                    }
                    if (neg) n = -n;
                    if (n < 1)   n = 1;
                    if (n > 288) n = 288;
                    cur_size = n;
                    er.etype = ami_etredraw;
                    ami_sendevent(out, &er);
                    break;
                }
                break;

            case ami_etchkbox:
                if (er.ckbxid == 5) {
                    strike_on = !strike_on;
                    ami_selectwidget(out, 5, strike_on);
                } else if (er.ckbxid == 6) {
                    under_on = !under_on;
                    ami_selectwidget(out, 6, under_on);
                } else if (er.ckbxid == 7) {
                    bold_on = !bold_on;
                    ami_selectwidget(out, 7, bold_on);
                } else if (er.ckbxid == 8) {
                    italic_on = !italic_on;
                    ami_selectwidget(out, 8, italic_on);
                }
                er.etype = ami_etredraw;
                continue;

            case ami_etbutton:
                if (er.butid == 1) {
                    cancelled = TRUE; er.etype = ami_etterm;
                } else if (er.butid == 2) {
                    er.etype = ami_etterm;
                }
                break;

            case ami_etenter:
                er.etype = ami_etterm;
                break;

            case ami_etmoumovg:
                mpy = er.moupyg; /* track pointer y for title-bar hit test */
                break;

            case ami_etmouba:
                /* Press on the title bar: hand the drag to the graphics layer.
                   It tracks in desktop coordinates with the pointer grabbed, so
                   the window follows smoothly even when the pointer outruns it
                   and leaves the window's bounds -- a window-relative self-drag
                   cannot, because motion stops being delivered the moment the
                   pointer is no longer over the dialog. */
                if (er.amoubn == 1 && mpy <= titbot) ami_dragwin(out);
                break;

            default: ;

        }

    } while (er.etype != ami_etterm);
    
    /* a terminate from outside must end the program, not just
       this dialog */
    endmodal(&er, realterm);

    if (!cancelled) {
        /* re-read the size from the edit box (user may not have pressed enter) */
        {
            char sbuf[32];
            ami_long n, neg, j;
            getwidgettextz(out, 4, sbuf, sizeof(sbuf));
            n = 0; neg = 0; j = 0;
            if (sbuf[0] == '-') { neg = 1; j = 1; }
            while (sbuf[j] >= '0' && sbuf[j] <= '9') {
                n = n*10 + (sbuf[j] - '0'); j++;
            }
            if (neg) n = -n;
            if (n >= 1 && n <= 288) cur_size = n;
        }
        if (fc) *fc = cur_font;
        if (s)  *s  = cur_size;
        if (effect) {
            /* preserve other bits, toggle the ones we own */
            ami_qfteffects out_eff = eff_in;
            out_eff &= ~((1 << ami_qftestrikeout) | (1 << ami_qfteunderline)
                       | (1 << ami_qftebold)      | (1 << ami_qfteitalic));
            if (strike_on) out_eff |= (1 << ami_qftestrikeout);
            if (under_on)  out_eff |= (1 << ami_qfteunderline);
            if (bold_on)   out_eff |= (1 << ami_qftebold);
            if (italic_on) out_eff |= (1 << ami_qfteitalic);
            *effect = out_eff;
        }
        /* foreground/background colors are flow-through: left unchanged */
        (void)fr; (void)fg; (void)fb; (void)br; (void)bg; (void)bb;
    }
    (void)f;

    /* fontlist is owned by the font listbox widget (wp->strlst); fclose
       will tear it down via putwig/frestrlst. Do NOT free it here. */

    fclose(out);

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

Theme point names

The config file names for each theme table entry, in themeindex order. The
"th_" prefix is dropped: theme point th_backpressed is written "backpressed"
in the config file.

*******************************************************************************/

static const char* themnam[th_endmarker] = {

    "backpressed", "back", "backhover", "outline1",
    "text", "textdis", "focus", "chkrad",
    "chkradout", "scrollback", "scrollbar", "scrollbarpressed",
    "numseldiv", "numselud", "texterr", "proginacen",
    "proginaedg", "progactcen", "progactedg", "lsthov",
    "lstsel", "outline2", "droparrow", "droptext", "sldint",
    "tabdis", "tabback", "tabsel", "tabfocus",
    "cancelbackfocus", "canceltextfocus", "canceloutline", "selectbackfocus",
    "selectback", "selecttextfocus", "selecttext", "selectoutline",
    "selectoutlinefocus", "plusbackfocus", "plusback", "plustextfocus",
    "plustext", "plusoutline", "plusoutlinefocus", "title",
    "querycolor1", "querycolor2", "querycolor3", "querycolor4",
    "querycolor5", "querycolor6", "querycolor7", "querycolor8",
    "querycolor9", "querycolor10", "querycolor11", "querycolor12",
    "querycolor13", "querycolor14", "querycolor15", "querycolor16",
    "querycolor17", "querycolor18", "querycolor19", "querycolor20",
    "querycolor21", "querycolor22", "querycolor23", "querycolor24",
    "querycolor25", "querycolor26", "querycolor27", "querycolor28",
    "querycolor29", "querycolor30", "querycolor31", "querycolor32",
    "querycolor33", "querycolor34", "querycolor35", "querycolor36"

};

/** ****************************************************************************

Parse a theme value

Accepts a color as three decimal components "red green blue" (0 to 255), or
as a hex triplet "rrggbb" with an optional leading '#'. Returns TRUE and the
packed value if the string parses, FALSE if it does not. Note the theme table
is a general value table; a plain number is accepted as well, for theme
points that are not colors.

*******************************************************************************/

static int themval(
    /** string to parse */ const char* s,
    /** value returned */  ami_ulong* v
)

{

    ami_long r, g, b;
    char* ep;

    while (*s == ' ') s++; /* skip leading spaces */
    /* a hex triplet is six hex digits and nothing else, so a decimal triple
       that happens to be six characters long is not taken for one */
    if (*s == '#' ||
        (strlen(s) == 6 && strspn(s, "0123456789abcdefABCDEF") == 6)) {

        if (*s == '#') s++;
        *v = strtoul(s, &ep, 16);
        while (*ep == ' ') ep++; /* trailing spaces are not an error */

        return (!*ep);

    }
    /* three decimal components */
    r = strtol(s, &ep, 10);
    if (ep == s) return (FALSE);
    s = ep;
    g = strtol(s, &ep, 10);
    if (ep == s) { /* single number: a non color theme value */

        *v = r;

        return (TRUE);

    }
    s = ep;
    b = strtol(s, &ep, 10);
    if (ep == s) return (FALSE);
    while (*ep == ' ') ep++;
    if (*ep) return (FALSE); /* trailing junk */
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) return (FALSE);
    *v = r<<16 | g<<8 | b;

    return (TRUE);

}

/** ****************************************************************************

Load theme from config

Overrides the compiled in theme defaults with any values found in the
config file, in the block:

begin widgets
    begin theme
        back 252 252 252
        ...
    end
end

An entry whose name is not a known theme point is ignored, so a config
written for a later version still loads here. An entry that does not parse
is an error, since that is a mistake in the file rather than a version
difference.

*******************************************************************************/

static void loadtheme(void)

{

    ami_valptr root;  /* config root */
    ami_valptr wp;    /* widgets block */
    ami_valptr tp;    /* theme block */
    ami_valptr vp;    /* value entry */
    themeindex ti;    /* theme index */
    ami_ulong v;  /* parsed value */

    root = NULL;
    ami_config(&root); /* get the config tree */
    wp = ami_schlst("widgets", root); /* find widgets block */
    if (!wp || !wp->sublist) return; /* no widgets block, keep defaults */
    tp = ami_schlst("theme", wp->sublist); /* find theme subblock */
    if (!tp || !tp->sublist) return; /* no theme block, keep defaults */
    for (vp = tp->sublist; vp; vp = vp->next) { /* traverse theme points */

        if (vp->name && vp->value)
            for (ti = 0; ti < th_endmarker; ti++)
                if (!strcmp(vp->name, themnam[ti])) {

            if (!themval(vp->value, &v)) error("Invalid theme value");
            themetable[ti] = v;

        }

    }

}

/** ****************************************************************************

Widgets startup

*******************************************************************************/

/* This package serves the KDE desktops; the Gnome package serves the
   rest. The two share every symbol privately, so both ride in the same
   binary and the session decides */
static int desksel(void)

{

    const char* d = getenv("XDG_CURRENT_DESKTOP");

    return (d && strstr(d, "KDE"));

}

static void init_widgets(void) __attribute__((constructor (105)));
static void init_widgets()

{

    if (!desksel()) return; /* not our desktop */

    /* Register with the widget base: the tracking tables, the event
       intercept, the close intercept and the widget lifecycle are all
       under it. The error reporter is ours, so widget errors present
       as they always have. */
    wb_init(&pkg, sizeof(wigrec), widget_event, wiginit, wigkill, wigfree,
            error);

    /* open "window 0" dummy window */
    ami_openwin(&stdin, &win0, NULL, ami_getwinid()); /* open window */
    ami_buffer(win0, FALSE); /* turn off buffering */
    ami_auto(win0, FALSE); /* turn off auto (for font change) */
    setwigfont(win0); /* set sign font */
    ami_frame(win0, FALSE); /* turn off frame */

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
    themetable[th_backhover]          = TD_BACKHOVER;
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
    themetable[th_lstsel]             = TD_LSTSEL;
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

    /* override the defaults with any theme values from the config file */
    loadtheme();

}

/** ****************************************************************************

Widgets shutdown

*******************************************************************************/

static void deinit_widgets(void) __attribute__((destructor (105)));
static void deinit_widgets()

{

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

    if (!desksel()) return; /* not our desktop */

    /* Unregister from the widget base, which takes down any widgets
       still standing; the last package out restores the event and close
       intercepts. */
    wb_deinit(&pkg);

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

}
