/*******************************************************************************
*                                                                              *
*                        GRAPHICAL MODE LIBRARY HEADER                         *
*                                                                              *
*                       Copyright (C) 2019 Scott A. Franco                     *
*                                                                              *
*                            2019/05/03 S. A. Franco                           *
*                                                                              *
* Describes the full Petit-ami graphical subsystem, including terminal level,  *
* graphics level, windowing, and widgets.                                      *
*                                                                              *
*******************************************************************************/

#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define AMI_MAXTIM 10 /**< maximum number of timers available */

/* standard fonts */

#define AMI_FONT_TERM 1 /* terminal (fixed space) font */
#define AMI_FONT_BOOK 2 /* serif font */
#define AMI_FONT_SIGN 3 /* san-serif font */
#define AMI_FONT_TECH 4 /* technical (scalable) font */

/* standardized menu entries */

#define AMI_SMNEW        1 /* new file */
#define AMI_SMOPEN       2 /* open file */
#define AMI_SMCLOSE      3 /* close file */
#define AMI_SMSAVE       4 /* save file */
#define AMI_SMSAVEAS     5 /* save file as name */
#define AMI_SMPAGESET    6 /* page setup */
#define AMI_SMPRINT      7 /* print */
#define AMI_SMEXIT       8 /* exit program */
#define AMI_SMUNDO       9 /* undo edit */
#define AMI_SMCUT       10 /* cut selection */
#define AMI_SMPASTE     11 /* paste selection */
#define AMI_SMDELETE    12 /* delete selection */
#define AMI_SMFIND      13 /* find text */
#define AMI_SMFINDNEXT  14 /* find next */
#define AMI_SMREPLACE   15 /* replace text */
#define AMI_SMGOTO      16 /* goto line */
#define AMI_SMSELECTALL 17 /* select all text */
#define AMI_SMNEWWINDOW 18 /* new window */
#define AMI_SMTILEHORIZ 19 /* tile child windows horizontally */
#define AMI_SMTILEVERT  20 /* tile child windows vertically */
#define AMI_SMCASCADE   21 /* cascade windows */
#define AMI_SMCLOSEALL  22 /* close all windows */
#define AMI_SMHELPTOPIC 23 /* help topics */
#define AMI_SMABOUT     24 /* about this program */
#define AMI_SMMAX       24 /* maximum defined standard menu entries */

/* Colors displayable in text mode. Background is the color that will match
   widgets placed onto it. */
typedef enum { ami_black, ami_white, ami_red, ami_green, ami_blue, ami_cyan,
               ami_yellow, ami_magenta, ami_backcolor } ami_color;
/* events */
typedef enum {
    ami_etchar,     /* ANSI character returned */
    ami_etup,       /* cursor up one line */
    ami_etdown,     /* down one line */
    ami_etleft,     /* left one character */
    ami_etright,    /* right one character */
    ami_etleftw,    /* left one word */
    ami_etrightw,   /* right one word */
    ami_ethome,     /* home of document */
    ami_ethomes,    /* home of screen */
    ami_ethomel,    /* home of line */
    ami_etend,      /* end of document */
    ami_etends,     /* end of screen */
    ami_etendl,     /* end of line */
    ami_etscrl,     /* scroll left one character */
    ami_etscrr,     /* scroll right one character */
    ami_etscru,     /* scroll up one line */
    ami_etscrd,     /* scroll down one line */
    ami_etpagd,     /* page down */
    ami_etpagu,     /* page up */
    ami_ettab,      /* tab */
    ami_etenter,    /* enter line */
    ami_etinsert,   /* insert block */
    ami_etinsertl,  /* insert line */
    ami_etinsertt,  /* insert toggle */
    ami_etdel,      /* delete block */
    ami_etdell,     /* delete line */
    ami_etdelcf,    /* delete character forward */
    ami_etdelcb,    /* delete character backward */
    ami_etcopy,     /* copy block */
    ami_etcopyl,    /* copy line */
    ami_etcan,      /* cancel current operation */
    ami_etstop,     /* stop current operation */
    ami_etcont,     /* continue current operation */
    ami_etprint,    /* print document */
    ami_etprintb,   /* print block */
    ami_etprints,   /* print screen */
    ami_etfun,      /* function key */
    ami_etmenu,     /* display menu */
    ami_etmouba,    /* mouse button assertion */
    ami_etmoubd,    /* mouse button deassertion */
    ami_etmoumov,   /* mouse move */
    ami_ettim,      /* timer matures */
    ami_etjoyba,    /* joystick button assertion */
    ami_etjoybd,    /* joystick button deassertion */
    ami_etjoymov,   /* joystick move */
    ami_etresize,   /* window was resized */
    ami_etfocus,    /* window has focus */
    ami_etnofocus,  /* window lost focus */
    ami_ethover,    /* window being hovered */
    ami_etnohover,  /* window stopped being hovered */
    ami_etterm,     /* terminate program */
    ami_etframe,    /* frame sync */
    ami_etmoumovg,  /* mouse move graphical */
    ami_etredraw,   /* window redraw */
    ami_etmin,      /* window minimized */
    ami_etmax,      /* window maximized */
    ami_etnorm,     /* window normalized */
    ami_etmenus,    /* menu item selected */
    ami_etbutton,   /* button assert */
    ami_etchkbox,   /* checkbox click */
    ami_etradbut,   /* radio button click */
    ami_etsclull,   /* scroll up/left line */
    ami_etscldrl,   /* scroll down/right line */
    ami_etsclulp,   /* scroll up/left page */
    ami_etscldrp,   /* scroll down/right page */
    ami_etsclpos,   /* scroll bar position */
    ami_etedtbox,   /* edit box signals done */
    ami_etnumbox,   /* number select box signals done */
    ami_etlstbox,   /* list box selection */
    ami_etdrpbox,   /* drop box selection */
    ami_etdrebox,   /* drop edit box signals done */
    ami_etsldpos,   /* slider position */
    ami_ettabbar,   /* tab bar select */

    /* Reserved extra code areas, these are module defined. */
    ami_etsys    = 0x1000, /* start of base system reserved codes */
    ami_etman    = 0x2000, /* start of window management reserved codes */
    ami_etwidget = 0x3000, /* start of widget reserved codes */
    ami_etuser   = 0x4000  /* start of user defined codes */

} ami_evtcod;
/* event record */
typedef struct {

    /* identifier of window for event */ int winid;
    /* event type */                     ami_evtcod etype;
    /* event was handled */              int handled;
    union {

        /* these events require parameter data */

        /** etchar: ANSI character returned */  char echar;
        /** ettim: timer handle that matured */ int timnum;
        /** etmoumov: */
        struct {

            /** mouse number */   int mmoun;
            /** mouse movement */ int moupx, moupy;

        };
        /* etmouba */
        struct {

            /** mouse handle */  int amoun;
            /** button number */ int amoubn;

        };
        /* etmoubd */
        struct {

            /** mouse handle */  int dmoun;
            /** button number */ int dmoubn;

        };
        /* ami_etjoyba */
        struct {

            /** joystick number */ int ajoyn;
            /** button number */   int ajoybn;

        };
        /* ami_etjoybd */
        struct {

            /** joystick number */ int djoyn;
            /** button number */   int djoybn;

        };
        /* ami_etjoymov */
        struct {

            /** joystick number */      int mjoyn;
            /** joystick coordinates */ int joypx, joypy, joypz;
                                        int joyp4, joyp5, joyp6;

        };
        /* ami_etfun */
        /** function key */ int fkey;
        /* ami_etresize */
        struct {

            int rszx, rszy, rszxg, rszyg;

        };

        /** etmoumovg: */
        struct {

            /** mouse number */   int mmoung;
            /** mouse movement */ int moupxg, moupyg;

        };
        /** etredraw */
        struct {

            /** bounding rectangle */
            int rsx, rsy, rex, rey;

        };
        /* ami_etmenus */
        int menuid; /* menu item selected */
        /* ami_etbutton */
        int butid; /* button id */
        /* ami_etchkbox */
        int ckbxid; /* checkbox id */
        /* ami_etradbut */
        int radbid; /* radio button id */
        /* ami_etsclull */
        int sclulid; /* scroll up/left line id*/
        /* ami_etscldrl */
        int scldrid; /* scroll down/right line id */
        /* ami_etsclulp */
        int sclupid; /* scroll up/left page id */
        /* ami_etscldrp */
        int scldpid; /* scroll down/right page id */
        /* ami_etsclpos */
        struct {

            int sclpid; /* scroll bar id */
            int sclpos; /* scroll bar position */

        };
        /* ami_etedtbox */
        int edtbid; /* edit box complete id */
        /* ami_etnumbox */
        struct { /* number select box signals done */

            int numbid; /* num sel box id */
            int numbsl; /* num select value */

        };
        /* ami_etlstbox */
        struct {

            int lstbid; /* list box id */
            int lstbsl; /* list box select number */

        };
        /* ami_etdrpbox */
        struct {

            int drpbid; /* drop box id */
            int drpbsl; /* drop box select */

        };
        /* ami_etdrebox */
        int drebid; /* drop edit box id */
        /* ami_etsldpos */
        struct {

            int sldpid; /* slider id */
            int sldpos; /* slider position */

        };
        /* ami_ettabbar */
        struct {

            int tabid;  /* tab bar id */
            int tabsel; /* tab select */

        };

     };

} ami_evtrec, *ami_evtptr;

/* event function pointer */
typedef void (*ami_pevthan)(ami_evtrec*);

/* menu */
typedef struct ami_menurec* ami_menuptr;
typedef struct ami_menurec {

        ami_menuptr next;   /* next menu item in list */
        ami_menuptr branch; /* menu branch */
        int        onoff;  /* on/off highlight */
        int        oneof;  /* "one of" highlight */
        int        bar;    /* place bar under */
        int        id;     /* id of menu item */
        char*      face;   /* text to place on button */

} ami_menurec;
/* standard menu selector */
typedef int ami_stdmenusel;
/* windows mode sets */
typedef enum {

    ami_wmframe, /* frame on/off */
    ami_wmsize,  /* size bars on/off */
    ami_wmsysbar /* system bar on/off */

} ami_winmod;
typedef int ami_winmodset;
/* string set for list box */
typedef struct ami_strrec* ami_strptr;
typedef struct ami_strrec {

    ami_strptr next; /* next entry in list */
    char*    str;  /* string */

} ami_strrec;
/* orientation for tab bars */
typedef enum { ami_totop, ami_toright, ami_tobottom, ami_toleft } ami_tabori;
/* settable items in find query */
typedef enum { ami_qfncase, ami_qfnup, ami_qfnre } ami_qfnopt;
typedef int ami_qfnopts;
/* settable items in replace query */
typedef enum { ami_qfrcase, ami_qfrup, ami_qfrre, ami_qfrfind, ami_qfrallfil, ami_qfralllin } ami_qfropt;
typedef int ami_qfropts;
/* effects in font query */
typedef enum { ami_qfteblink, ami_qftereverse, ami_qfteunderline, ami_qftesuperscript,
                  ami_qftesubscript, ami_qfteitalic, ami_qftebold, ami_qftestrikeout,
                  ami_qftestandout, ami_qftecondensed, ami_qfteextended, ami_qftexlight,
                  ami_qftelight, ami_qftexbold, ami_qftehollow, ami_qfteraised} ami_qfteffect;
typedef int ami_qfteffects;

/* functions at this level */

/* text */

void ami_cursor(FILE* f, int x, int y);
int ami_maxx(FILE* f);
int ami_maxy(FILE* f);
void ami_home(FILE* f);
void ami_del(FILE* f);
void ami_up(FILE* f);
void ami_down(FILE* f);
void ami_left(FILE* f);
void ami_right(FILE* f);
void ami_blink(FILE* f, int e);
void ami_reverse(FILE* f, int e);
void ami_underline(FILE* f, int e);
void ami_superscript(FILE* f, int e);
void ami_subscript(FILE* f, int e);
void ami_italic(FILE* f, int e);
void ami_bold(FILE* f, int e);
void ami_strikeout(FILE* f, int e);
void ami_standout(FILE* f, int e);
void ami_fcolor(FILE* f, ami_color c);
void ami_bcolor(FILE* f, ami_color c);
void ami_auto(FILE* f, int e);
void ami_curvis(FILE* f, int e);
void ami_scroll(FILE* f, int x, int y);
int ami_curx(FILE* f);
int ami_cury(FILE* f);
int ami_curbnd(FILE* f);
void ami_select(FILE* f, int u, int d);
void ami_event(FILE* f, ami_evtrec* er);
void ami_timer(FILE* f, int i, long t, int r);
void ami_killtimer(FILE* f, int i);
int ami_mouse(FILE* f);
int ami_mousebutton(FILE* f, int m);
int ami_joystick(FILE* f);
int ami_joybutton(FILE* f, int j);
int ami_joyaxis(FILE* f, int j);
void ami_settab(FILE* f, int t);
void ami_restab(FILE* f, int t);
void ami_clrtab(FILE* f);
int ami_funkey(FILE* f);
void ami_frametimer(FILE* f, int e);
void ami_autohold(int e);
void ami_wrtstr(FILE* f, char* s);
void ami_wrtstrn(FILE* f, char* s, int n);
void ami_sizbuf(FILE* f, int x, int y);
void ami_title(FILE* f, char* ts);
void ami_eventover(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh);
void ami_eventsover(ami_pevthan eh,  ami_pevthan* oeh);
void ami_sendevent(FILE* f, ami_evtrec* er);

/* graphical */

int ami_maxxg(FILE* f);
int ami_maxyg(FILE* f);
int ami_curxg(FILE* f);
int ami_curyg(FILE* f);
void ami_line(FILE* f, int x1, int y1, int x2, int y2);
void ami_linewidth(FILE* f, int w);
void ami_rect(FILE* f, int x1, int y1, int x2, int y2);
void ami_frect(FILE* f, int x1, int y1, int x2, int y2);
void ami_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
void ami_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
void ami_ellipse(FILE* f, int x1, int y1, int x2, int y2);
void ami_fellipse(FILE* f, int x1, int y1, int x2, int y2);
void ami_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void ami_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void ami_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void ami_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3);
void ami_cursorg(FILE* f, int x, int y);
int ami_baseline(FILE* f);
void ami_setpixel(FILE* f, int x, int y);
void ami_fover(FILE* f);
void ami_bover(FILE* f);
void ami_finvis(FILE* f);
void ami_binvis(FILE* f);
void ami_fxor(FILE* f);
void ami_bxor(FILE* f);
void ami_fand(FILE* f);
void ami_band(FILE* f);
void ami_for(FILE* f);
void ami_bor(FILE* f);
int ami_chrsizx(FILE* f);
int ami_chrsizy(FILE* f);
int ami_fonts(FILE* f);
void ami_font(FILE* f, int fc);
void ami_fontnam(FILE* f, int fc, char* fns, int fnsl);
void ami_fontsiz(FILE* f, int s);
void ami_setpoints(FILE* f, float ps);
float ami_points(FILE* f);
void ami_chrspcy(FILE* f, int s);
void ami_chrspcx(FILE* f, int s);
int ami_dpmx(FILE* f);
int ami_dpmy(FILE* f);
int ami_strsiz(FILE* f, const char* s);
int ami_chrpos(FILE* f, const char* s, int p);
void ami_writejust(FILE* f, const char* s, int n);
int ami_justpos(FILE* f, const char* s, int p, int n);
void ami_condensed(FILE* f, int e);
void ami_extended(FILE* f, int e);
void ami_xlight(FILE* f, int e);
void ami_light(FILE* f, int e);
void ami_xbold(FILE* f, int e);
void ami_hollow(FILE* f, int e);
void ami_raised(FILE* f, int e);
void ami_settabg(FILE* f, int t);
void ami_restabg(FILE* f, int t);
void ami_fcolorg(FILE* f, int r, int g, int b);
void ami_fcolorc(FILE* f, int r, int g, int b);
void ami_bcolorg(FILE* f, int r, int g, int b);
void ami_bcolorc(FILE* f, int r, int g, int b);
void ami_loadpict(FILE* f, int p, char* fn);
int ami_pictsizx(FILE* f, int p);
int ami_pictsizy(FILE* f, int p);
void ami_picture(FILE* f, int p, int x1, int y1, int x2, int y2);
void ami_delpict(FILE* f, int p);
void ami_scrollg(FILE* f, int x, int y);
void ami_path(FILE* f, int a);

/* Window management functions */

void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid);
void ami_buffer(FILE* f, int e);
void ami_sizbufg(FILE* f, int x, int y);
void ami_getsiz(FILE* f, int* x, int* y);
void ami_getsizg(FILE* f, int* x, int* y);
void ami_setsiz(FILE* f, int x, int y);
void ami_setsizg(FILE* f, int x, int y);
void ami_setpos(FILE* f, int x, int y);
void ami_setposg(FILE* f, int x, int y);
void ami_scnsiz(FILE* f, int* x, int* y);
void ami_scnsizg(FILE* f, int* x, int*y);
void ami_scncen(FILE* f, int* x, int* y);
void ami_scnceng(FILE* f, int* x, int* y);
void ami_winclient(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms);
void ami_winclientg(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms);
void ami_front(FILE* f);
void ami_back(FILE* f);
void ami_frame(FILE* f, int e);
void ami_sizable(FILE* f, int e);
void ami_sysbar(FILE* f, int e);
void ami_menu(FILE* f, ami_menuptr m);
void ami_menuena(FILE* f, int id, int onoff);
void ami_menusel(FILE* f, int id, int select);
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm);
int ami_getwinid(void);
void ami_focus(FILE* f);

/* widgets/controls */

int ami_getwigid(FILE* f);
void ami_killwidget(FILE* f, int id);
void ami_selectwidget(FILE* f, int id, int e);
void ami_enablewidget(FILE* f, int id, int e);
void ami_getwidgettext(FILE* f, int id, char* s, int sl);
void ami_putwidgettext(FILE* f, int id, char* s);
void ami_sizwidget(FILE* f, int id, int x, int y);
void ami_sizwidgetg(FILE* f, int id, int x, int y);
void ami_poswidget(FILE* f, int id, int x, int y);
void ami_poswidgetg(FILE* f, int id, int x, int y);
void ami_backwidget(FILE* f, int id);
void ami_frontwidget(FILE* f, int id);
void ami_focuswidget(FILE* f, int id);
void ami_buttonsiz(FILE* f, char* s, int* w, int* h);
void ami_buttonsizg(FILE* f, char* s, int* w, int* h);
void ami_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_checkboxsiz(FILE* f, char* s, int* w, int* h);
void ami_checkboxsizg(FILE* f, char* s, int* w, int* h);
void ami_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_radiobuttonsiz(FILE* f, char* s, int* w, int* h);
void ami_radiobuttonsizg(FILE* f, char* s, int* w, int* h);
void ami_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
void ami_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                 int* oy);
void ami_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void ami_background(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_scrollvertsizg(FILE* f, int* w, int* h);
void ami_scrollvertsiz(FILE* f, int* w, int* h);
void ami_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_scrollhorizsizg(FILE* f, int* w, int* h);
void ami_scrollhorizsiz(FILE* f, int* w, int* h);
void ami_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_scrollpos(FILE* f, int id, int r);
void ami_scrollsiz(FILE* f, int id, int r);
void ami_numselboxsizg(FILE* f, int l, int u, int* w, int* h);
void ami_numselboxsiz(FILE* f, int l, int u, int* w, int* h);
void ami_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                  int id);
void ami_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                   int id);
void ami_editboxsizg(FILE* f, char* s, int* w, int* h);
void ami_editboxsiz(FILE* f, char* s, int* w, int* h);
void ami_editbox(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_progbarsizg(FILE* f, int* w, int* h);
void ami_progbarsiz(FILE* f, int* w, int* h);
void ami_progbar(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id);
void ami_progbarpos(FILE* f, int id, int pos);
void ami_listboxsizg(FILE* f, ami_strptr sp, int* w, int* h);
void ami_listboxsiz(FILE* f, ami_strptr sp, int* w, int* h);
void ami_listbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
void ami_listboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
void ami_dropboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
void ami_dropboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
void ami_dropbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
void ami_dropboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
void ami_dropeditboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
void ami_dropeditboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
void ami_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
void ami_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
                     int id);
void ami_slidehorizsizg(FILE* f, int* w, int* h);
void ami_slidehorizsiz(FILE* f, int* w, int* h);
void ami_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void ami_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void ami_slidevertsizg(FILE* f, int* w, int* h);
void ami_slidevertsiz(FILE* f, int* w, int* h);
void ami_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void ami_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void ami_tabbarsizg(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h,
                   int* ox, int* oy);
void ami_tabbarsiz(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
void ami_tabbarclientg(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                      int* ox, int* oy);
void ami_tabbarclient(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                     int* ox, int* oy);
void ami_tabbar(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
               ami_tabori tor, int id);
void ami_tabbarg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
                ami_tabori tor, int id);
void ami_tabsel(FILE* f, int id, int tn);
void ami_alert(char* title, char* message);
void ami_querycolor(int* r, int* g, int* b);
void ami_queryopen(char* s, int sl);
void ami_querysave(char* s, int sl);
void ami_queryfind(char* s, int sl, ami_qfnopts* opt);
void ami_queryfindrep(char* s, int sl, char* r, int rl, ami_qfropts* opt);
void ami_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
                  int* bg, int* bb, ami_qfteffects* effect);

/*
 * Override vector types
 *
 */
typedef void (*ami_cursor_t)(FILE* f, int x, int y);
typedef int (*ami_maxx_t)(FILE* f);
typedef int (*ami_maxy_t)(FILE* f);
typedef void (*ami_home_t)(FILE* f);
typedef void (*ami_del_t)(FILE* f);
typedef void (*ami_up_t)(FILE* f);
typedef void (*ami_down_t)(FILE* f);
typedef void (*ami_left_t)(FILE* f);
typedef void (*ami_right_t)(FILE* f);
typedef void (*ami_blink_t)(FILE* f, int e);
typedef void (*ami_reverse_t)(FILE* f, int e);
typedef void (*ami_underline_t)(FILE* f, int e);
typedef void (*ami_superscript_t)(FILE* f, int e);
typedef void (*ami_subscript_t)(FILE* f, int e);
typedef void (*ami_italic_t)(FILE* f, int e);
typedef void (*ami_bold_t)(FILE* f, int e);
typedef void (*ami_strikeout_t)(FILE* f, int e);
typedef void (*ami_standout_t)(FILE* f, int e);
typedef void (*ami_fcolor_t)(FILE* f, ami_color c);
typedef void (*ami_bcolor_t)(FILE* f, ami_color c);
typedef void (*ami_auto_t)(FILE* f, int e);
typedef void (*ami_curvis_t)(FILE* f, int e);
typedef void (*ami_scroll_t)(FILE* f, int x, int y);
typedef int (*ami_curx_t)(FILE* f);
typedef int (*ami_cury_t)(FILE* f);
typedef int (*ami_curbnd_t)(FILE* f);
typedef void (*ami_select_t)(FILE* f, int u, int d);
typedef void (*ami_event_t)(FILE* f, ami_evtrec* er);
typedef void (*ami_timer_t)(FILE* f, int i, long t, int r);
typedef void (*ami_killtimer_t)(FILE* f, int i);
typedef int (*ami_mouse_t)(FILE* f);
typedef int (*ami_mousebutton_t)(FILE* f, int m);
typedef int (*ami_joystick_t)(FILE* f);
typedef int (*ami_joybutton_t)(FILE* f, int j);
typedef int (*ami_joyaxis_t)(FILE* f, int j);
typedef void (*ami_settab_t)(FILE* f, int t);
typedef void (*ami_restab_t)(FILE* f, int t);
typedef void (*ami_clrtab_t)(FILE* f);
typedef int (*ami_funkey_t)(FILE* f);
typedef void (*ami_frametimer_t)(FILE* f, int e);
typedef void (*ami_autohold_t)(int e);
typedef void (*ami_wrtstr_t)(FILE* f, char* s);
typedef void (*ami_wrtstrn_t)(FILE* f, char* s, int n);
typedef void (*ami_sizbuf_t)(FILE* f, int x, int y);
typedef void (*ami_title_t)(FILE* f, char* ts);
typedef void (*ami_fcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*ami_bcolorc_t)(FILE* f, int r, int g, int b);
typedef void (*ami_eventover_t)(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh);
typedef void (*ami_eventsover_t)(ami_pevthan eh,  ami_pevthan* oeh);
typedef void (*ami_sendevent_t)(FILE* f, ami_evtrec* er);
typedef int (*ami_maxxg_t)(FILE* f);
typedef int (*ami_maxyg_t)(FILE* f);
typedef int (*ami_curxg_t)(FILE* f);
typedef int (*ami_curyg_t)(FILE* f);
typedef void (*ami_line_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*ami_linewidth_t)(FILE* f, int w);
typedef void (*ami_rect_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*ami_frect_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*ami_rrect_t)(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
typedef void (*ami_frrect_t)(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
typedef void (*ami_ellipse_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*ami_fellipse_t)(FILE* f, int x1, int y1, int x2, int y2);
typedef void (*ami_arc_t)(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
typedef void (*ami_farc_t)(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
typedef void (*ami_fchord_t)(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
typedef void (*ami_ftriangle_t)(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3);
typedef void (*ami_cursorg_t)(FILE* f, int x, int y);
typedef int (*ami_baseline_t)(FILE* f);
typedef void (*ami_setpixel_t)(FILE* f, int x, int y);
typedef void (*ami_fover_t)(FILE* f);
typedef void (*ami_bover_t)(FILE* f);
typedef void (*ami_finvis_t)(FILE* f);
typedef void (*ami_binvis_t)(FILE* f);
typedef void (*ami_fxor_t)(FILE* f);
typedef void (*ami_bxor_t)(FILE* f);
typedef void (*ami_fand_t)(FILE* f);
typedef void (*ami_band_t)(FILE* f);
typedef void (*ami_for_t)(FILE* f);
typedef void (*ami_bor_t)(FILE* f);
typedef int (*ami_chrsizx_t)(FILE* f);
typedef int (*ami_chrsizy_t)(FILE* f);
typedef int (*ami_fonts_t)(FILE* f);
typedef void (*ami_font_t)(FILE* f, int fc);
typedef void (*ami_fontnam_t)(FILE* f, int fc, char* fns, int fnsl);
typedef void (*ami_fontsiz_t)(FILE* f, int s);
typedef void (*ami_setpoints_t)(FILE* f, float ps);
typedef float (*ami_points_t)(FILE* f);
typedef void (*ami_chrspcy_t)(FILE* f, int s);
typedef void (*ami_chrspcx_t)(FILE* f, int s);
typedef int (*ami_dpmx_t)(FILE* f);
typedef int (*ami_dpmy_t)(FILE* f);
typedef int (*ami_strsiz_t)(FILE* f, const char* s);
typedef int (*ami_chrpos_t)(FILE* f, const char* s, int p);
typedef void (*ami_writejust_t)(FILE* f, const char* s, int n);
typedef int (*ami_justpos_t)(FILE* f, const char* s, int p, int n);
typedef void (*ami_condensed_t)(FILE* f, int e);
typedef void (*ami_extended_t)(FILE* f, int e);
typedef void (*ami_xlight_t)(FILE* f, int e);
typedef void (*ami_light_t)(FILE* f, int e);
typedef void (*ami_xbold_t)(FILE* f, int e);
typedef void (*ami_hollow_t)(FILE* f, int e);
typedef void (*ami_raised_t)(FILE* f, int e);
typedef void (*ami_settabg_t)(FILE* f, int t);
typedef void (*ami_restabg_t)(FILE* f, int t);
typedef void (*ami_fcolorg_t)(FILE* f, int r, int g, int b);
typedef void (*ami_bcolorg_t)(FILE* f, int r, int g, int b);
typedef void (*ami_loadpict_t)(FILE* f, int p, char* fn);
typedef int (*ami_pictsizx_t)(FILE* f, int p);
typedef int (*ami_pictsizy_t)(FILE* f, int p);
typedef void (*ami_picture_t)(FILE* f, int p, int x1, int y1, int x2, int y2);
typedef void (*ami_delpict_t)(FILE* f, int p);
typedef void (*ami_scrollg_t)(FILE* f, int x, int y);
typedef void (*ami_path_t)(FILE* f, int a);
typedef void (*ami_openwin_t)(FILE** infile, FILE** outfile, FILE* parent, int wid);
typedef void (*ami_buffer_t)(FILE* f, int e);
typedef void (*ami_sizbufg_t)(FILE* f, int x, int y);
typedef void (*ami_getsiz_t)(FILE* f, int* x, int* y);
typedef void (*ami_getsizg_t)(FILE* f, int* x, int* y);
typedef void (*ami_setsiz_t)(FILE* f, int x, int y);
typedef void (*ami_setsizg_t)(FILE* f, int x, int y);
typedef void (*ami_setpos_t)(FILE* f, int x, int y);
typedef void (*ami_setposg_t)(FILE* f, int x, int y);
typedef void (*ami_scnsiz_t)(FILE* f, int* x, int* y);
typedef void (*ami_scnsizg_t)(FILE* f, int* x, int*y);
typedef void (*ami_scncen_t)(FILE* f, int* x, int* y);
typedef void (*ami_scnceng_t)(FILE* f, int* x, int* y);
typedef void (*ami_winclient_t)(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms);
typedef void (*ami_winclientg_t)(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms);
typedef void (*ami_front_t)(FILE* f);
typedef void (*ami_back_t)(FILE* f);
typedef void (*ami_frame_t)(FILE* f, int e);
typedef void (*ami_sizable_t)(FILE* f, int e);
typedef void (*ami_sysbar_t)(FILE* f, int e);
typedef void (*ami_menu_t)(FILE* f, ami_menuptr m);
typedef void (*ami_menuena_t)(FILE* f, int id, int onoff);
typedef void (*ami_menusel_t)(FILE* f, int id, int select);
typedef void (*ami_stdmenu_t)(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm);
typedef int (*ami_getwinid_t)(void);
typedef void (*ami_focus_t)(FILE* f);
typedef int (*ami_getwigid_t)(FILE* f);
typedef void (*ami_killwidget_t)(FILE* f, int id);
typedef void (*ami_selectwidget_t)(FILE* f, int id, int e);
typedef void (*ami_enablewidget_t)(FILE* f, int id, int e);
typedef void (*ami_getwidgettext_t)(FILE* f, int id, char* s, int sl);
typedef void (*ami_putwidgettext_t)(FILE* f, int id, char* s);
typedef void (*ami_sizwidget_t)(FILE* f, int id, int x, int y);
typedef void (*ami_sizwidgetg_t)(FILE* f, int id, int x, int y);
typedef void (*ami_poswidget_t)(FILE* f, int id, int x, int y);
typedef void (*ami_poswidgetg_t)(FILE* f, int id, int x, int y);
typedef void (*ami_backwidget_t)(FILE* f, int id);
typedef void (*ami_frontwidget_t)(FILE* f, int id);
typedef void (*ami_focuswidget_t)(FILE* f, int id);
typedef void (*ami_buttonsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_buttonsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_button_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_buttong_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_checkboxsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_checkboxsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_checkbox_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_checkboxg_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_radiobuttonsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_radiobuttonsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_radiobutton_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_radiobuttong_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_groupsizg_t)(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
typedef void (*ami_groupsiz_t)(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox,
                 int* oy);
typedef void (*ami_group_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_groupg_t)(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
typedef void (*ami_background_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_backgroundg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_scrollvertsizg_t)(FILE* f, int* w, int* h);
typedef void (*ami_scrollvertsiz_t)(FILE* f, int* w, int* h);
typedef void (*ami_scrollvert_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_scrollvertg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_scrollhorizsizg_t)(FILE* f, int* w, int* h);
typedef void (*ami_scrollhorizsiz_t)(FILE* f, int* w, int* h);
typedef void (*ami_scrollhoriz_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_scrollhorizg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_scrollpos_t)(FILE* f, int id, int r);
typedef void (*ami_scrollsiz_t)(FILE* f, int id, int r);
typedef void (*ami_numselboxsizg_t)(FILE* f, int l, int u, int* w, int* h);
typedef void (*ami_numselboxsiz_t)(FILE* f, int l, int u, int* w, int* h);
typedef void (*ami_numselbox_t)(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                  int id);
typedef void (*ami_numselboxg_t)(FILE* f, int x1, int y1, int x2, int y2, int l, int u,
                   int id);
typedef void (*ami_editboxsizg_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_editboxsiz_t)(FILE* f, char* s, int* w, int* h);
typedef void (*ami_editbox_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_editboxg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_progbarsizg_t)(FILE* f, int* w, int* h);
typedef void (*ami_progbarsiz_t)(FILE* f, int* w, int* h);
typedef void (*ami_progbar_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_progbarg_t)(FILE* f, int x1, int y1, int x2, int y2, int id);
typedef void (*ami_progbarpos_t)(FILE* f, int id, int pos);
typedef void (*ami_listboxsizg_t)(FILE* f, ami_strptr sp, int* w, int* h);
typedef void (*ami_listboxsiz_t)(FILE* f, ami_strptr sp, int* w, int* h);
typedef void (*ami_listbox_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
typedef void (*ami_listboxg_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
typedef void (*ami_dropboxsizg_t)(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*ami_dropboxsiz_t)(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*ami_dropbox_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
typedef void (*ami_dropboxg_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
typedef void (*ami_dropeditboxsizg_t)(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*ami_dropeditboxsiz_t)(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh);
typedef void (*ami_dropeditbox_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id);
typedef void (*ami_dropeditboxg_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
                     int id);
typedef void (*ami_slidehorizsizg_t)(FILE* f, int* w, int* h);
typedef void (*ami_slidehorizsiz_t)(FILE* f, int* w, int* h);
typedef void (*ami_slidehoriz_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*ami_slidehorizg_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*ami_slidevertsizg_t)(FILE* f, int* w, int* h);
typedef void (*ami_slidevertsiz_t)(FILE* f, int* w, int* h);
typedef void (*ami_slidevert_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*ami_slidevertg_t)(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
typedef void (*ami_tabbarsizg_t)(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h,
                   int* ox, int* oy);
typedef void (*ami_tabbarsiz_t)(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox,
                  int* oy);
typedef void (*ami_tabbarclientg_t)(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                      int* ox, int* oy);
typedef void (*ami_tabbarclient_t)(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                     int* ox, int* oy);
typedef void (*ami_tabbar_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
               ami_tabori tor, int id);
typedef void (*ami_tabbarg_t)(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
                ami_tabori tor, int id);
typedef void (*ami_tabsel_t)(FILE* f, int id, int tn);
typedef void (*ami_alert_t)(char* title, char* message);
typedef void (*ami_querycolor_t)(int* r, int* g, int* b);
typedef void (*ami_queryopen_t)(char* s, int sl);
typedef void (*ami_querysave_t)(char* s, int sl);
typedef void (*ami_queryfind_t)(char* s, int sl, ami_qfnopts* opt);
typedef void (*ami_queryfindrep_t)(char* s, int sl, char* r, int rl, ami_qfropts* opt);
typedef void (*ami_queryfont_t)(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
                  int* bg, int* bb, ami_qfteffects* effect);

/*
 * Overrider routines
 */
void _pa_scrollg_ovr(ami_scrollg_t nfp, ami_scrollg_t* ofp);
void _pa_scroll_ovr(ami_scroll_t nfp, ami_scroll_t* ofp);
void _pa_cursor_ovr(ami_cursor_t nfp, ami_cursor_t* ofp);
void _pa_cursorg_ovr(ami_cursorg_t nfp, ami_cursorg_t* ofp);
void _pa_baseline_ovr(ami_baseline_t nfp, ami_baseline_t* ofp);
void _pa_maxx_ovr(ami_maxx_t nfp, ami_maxx_t* ofp);
void _pa_maxy_ovr(ami_maxy_t nfp, ami_maxy_t* ofp);
void _pa_maxxg_ovr(ami_maxxg_t nfp, ami_maxxg_t* ofp);
void _pa_maxyg_ovr(ami_maxyg_t nfp, ami_maxyg_t* ofp);
void _pa_home_ovr(ami_home_t nfp, ami_home_t* ofp);
void _pa_up_ovr(ami_up_t nfp, ami_up_t* ofp);
void _pa_down_ovr(ami_down_t nfp, ami_down_t* ofp);
void _pa_left_ovr(ami_left_t nfp, ami_left_t* ofp);
void _pa_right_ovr(ami_right_t nfp, ami_right_t* ofp);
void _pa_blink_ovr(ami_blink_t nfp, ami_blink_t* ofp);
void _pa_reverse_ovr(ami_reverse_t nfp, ami_reverse_t* ofp);
void _pa_underline_ovr(ami_underline_t nfp, ami_underline_t* ofp);
void _pa_superscript_ovr(ami_superscript_t nfp, ami_superscript_t* ofp);
void _pa_subscript_ovr(ami_subscript_t nfp, ami_subscript_t* ofp);
void _pa_italic_ovr(ami_italic_t nfp, ami_italic_t* ofp);
void _pa_bold_ovr(ami_bold_t nfp, ami_bold_t* ofp);
void _pa_strikeout_ovr(ami_strikeout_t nfp, ami_strikeout_t* ofp);
void _pa_standout_ovr(ami_standout_t nfp, ami_standout_t* ofp);
void _pa_fcolor_ovr(ami_fcolor_t nfp, ami_fcolor_t* ofp);
void _pa_fcolorc_ovr(ami_fcolorc_t nfp, ami_fcolorc_t* ofp);
void _pa_fcolorg_ovr(ami_fcolorg_t nfp, ami_fcolorg_t* ofp);
void _pa_bcolor_ovr(ami_bcolor_t nfp, ami_bcolor_t* ofp);
void _pa_bcolorc_ovr(ami_bcolorc_t nfp, ami_bcolorc_t* ofp);
void _pa_bcolorg_ovr(ami_bcolorg_t nfp, ami_bcolorg_t* ofp);
void _pa_curbnd_ovr(ami_curbnd_t nfp, ami_curbnd_t* ofp);
void _pa_auto_ovr(ami_auto_t nfp, ami_auto_t* ofp);
void _pa_curvis_ovr(ami_curvis_t nfp, ami_curvis_t* ofp);
void _pa_curx_ovr(ami_curx_t nfp, ami_curx_t* ofp);
void _pa_cury_ovr(ami_cury_t nfp, ami_cury_t* ofp);
void _pa_curxg_ovr(ami_curxg_t nfp, ami_curxg_t* ofp);
void _pa_curyg_ovr(ami_curyg_t nfp, ami_curyg_t* ofp);
void _pa_select_ovr(ami_select_t nfp, ami_select_t* ofp);
void _pa_wrtstr_ovr(ami_wrtstr_t nfp, ami_wrtstr_t* ofp);
void _pa_sizbuf_ovr(ami_sizbuf_t nfp, ami_sizbuf_t* ofp);
void _pa_del_ovr(ami_del_t nfp, ami_del_t* ofp);
void _pa_line_ovr(ami_line_t nfp, ami_line_t* ofp);
void _pa_rect_ovr(ami_rect_t nfp, ami_rect_t* ofp);
void _pa_frect_ovr(ami_frect_t nfp, ami_frect_t* ofp);
void _pa_rrect_ovr(ami_rrect_t nfp, ami_rrect_t* ofp);
void _pa_frrect_ovr(ami_frrect_t nfp, ami_frrect_t* ofp);
void _pa_ellipse_ovr(ami_ellipse_t nfp, ami_ellipse_t* ofp);
void _pa_fellipse_ovr(ami_fellipse_t nfp, ami_fellipse_t* ofp);
void _pa_arc_ovr(ami_arc_t nfp, ami_arc_t* ofp);
void _pa_farc_ovr(ami_farc_t nfp, ami_farc_t* ofp);
void _pa_fchord_ovr(ami_fchord_t nfp, ami_fchord_t* ofp);
void _pa_ftriangle_ovr(ami_ftriangle_t nfp, ami_ftriangle_t* ofp);
void _pa_setpixel_ovr(ami_setpixel_t nfp, ami_setpixel_t* ofp);
void _pa_fover_ovr(ami_fover_t nfp, ami_fover_t* ofp);
void _pa_bover_ovr(ami_bover_t nfp, ami_bover_t* ofp);
void _pa_finvis_ovr(ami_finvis_t nfp, ami_finvis_t* ofp);
void _pa_binvis_ovr(ami_binvis_t nfp, ami_binvis_t* ofp);
void _pa_fxor_ovr(ami_fxor_t nfp, ami_fxor_t* ofp);
void _pa_bxor_ovr(ami_bxor_t nfp, ami_bxor_t* ofp);
void _pa_fand_ovr(ami_fand_t nfp, ami_fand_t* ofp);
void _pa_band_ovr(ami_band_t nfp, ami_band_t* ofp);
void _pa_for_ovr(ami_for_t nfp, ami_for_t* ofp);
void _pa_bor_ovr(ami_bor_t nfp, ami_bor_t* ofp);
void _pa_linewidth_ovr(ami_linewidth_t nfp, ami_linewidth_t* ofp);
void _pa_chrsizx_ovr(ami_chrsizx_t nfp, ami_chrsizx_t* ofp);
void _pa_chrsizy_ovr(ami_chrsizy_t nfp, ami_chrsizy_t* ofp);
void _pa_fonts_ovr(ami_fonts_t nfp, ami_fonts_t* ofp);
void _pa_font_ovr(ami_font_t nfp, ami_font_t* ofp);
void _pa_fontnam_ovr(ami_fontnam_t nfp, ami_fontnam_t* ofp);
void _pa_fontsiz_ovr(ami_fontsiz_t nfp, ami_fontsiz_t* ofp);
void _pa_setpoints_ovr(ami_setpoints_t nfp, ami_setpoints_t* ofp);
void _pa_points_ovr(ami_points_t nfp, ami_points_t* ofp);
void _pa_chrspcy_ovr(ami_chrspcy_t nfp, ami_chrspcy_t* ofp);
void _pa_chrspcx_ovr(ami_chrspcx_t nfp, ami_chrspcx_t* ofp);
void _pa_dpmx_ovr(ami_dpmx_t nfp, ami_dpmx_t* ofp);
void _pa_dpmy_ovr(ami_dpmy_t nfp, ami_dpmy_t* ofp);
void _pa_strsiz_ovr(ami_strsiz_t nfp, ami_strsiz_t* ofp);
void _pa_chrpos_ovr(ami_chrpos_t nfp, ami_chrpos_t* ofp);
void _pa_writejust_ovr(ami_writejust_t nfp, ami_writejust_t* ofp);
void _pa_justpos_ovr(ami_justpos_t nfp, ami_justpos_t* ofp);
void _pa_condensed_ovr(ami_condensed_t nfp, ami_condensed_t* ofp);
void _pa_extended_ovr(ami_extended_t nfp, ami_extended_t* ofp);
void _pa_xlight_ovr(ami_xlight_t nfp, ami_xlight_t* ofp);
void _pa_light_ovr(ami_light_t nfp, ami_light_t* ofp);
void _pa_xbold_ovr(ami_xbold_t nfp, ami_xbold_t* ofp);
void _pa_hollow_ovr(ami_hollow_t nfp, ami_hollow_t* ofp);
void _pa_raised_ovr(ami_raised_t nfp, ami_raised_t* ofp);
void _pa_delpict_ovr(ami_delpict_t nfp, ami_delpict_t* ofp);
void _pa_loadpict_ovr(ami_loadpict_t nfp, ami_loadpict_t* ofp);
void _pa_pictsizx_ovr(ami_pictsizx_t nfp, ami_pictsizx_t* ofp);
void _pa_pictsizy_ovr(ami_pictsizy_t nfp, ami_pictsizy_t* ofp);
void _pa_picture_ovr(ami_picture_t nfp, ami_picture_t* ofp);
void _pa_event_ovr(ami_event_t nfp, ami_event_t* ofp);
void _pa_sendevent_ovr(ami_sendevent_t nfp, ami_sendevent_t* ofp);
void _pa_eventover_ovr(ami_eventover_t nfp, ami_eventover_t* ofp);
void _pa_eventsover_ovr(ami_eventsover_t nfp, ami_eventsover_t* ofp);
void _pa_timer_ovr(ami_timer_t nfp, ami_timer_t* ofp);
void _pa_killtimer_ovr(ami_killtimer_t nfp, ami_killtimer_t* ofp);
void _pa_frametimer_ovr(ami_frametimer_t nfp, ami_frametimer_t* ofp);
void _pa_autohold_ovr(ami_autohold_t nfp, ami_autohold_t* ofp);
void _pa_mouse_ovr(ami_mouse_t nfp, ami_mouse_t* ofp);
void _pa_mousebutton_ovr(ami_mousebutton_t nfp, ami_mousebutton_t* ofp);
void _pa_joystick_ovr(ami_joystick_t nfp, ami_joystick_t* ofp);
void _pa_joybutton_ovr(ami_joybutton_t nfp, ami_joybutton_t* ofp);
void _pa_joyaxis_ovr(ami_joyaxis_t nfp, ami_joyaxis_t* ofp);
void _pa_settabg_ovr(ami_settabg_t nfp, ami_settabg_t* ofp);
void _pa_settab_ovr(ami_settab_t nfp, ami_settab_t* ofp);
void _pa_restabg_ovr(ami_restabg_t nfp, ami_restabg_t* ofp);
void _pa_restab_ovr(ami_restab_t nfp, ami_restab_t* ofp);
void _pa_clrtab_ovr(ami_clrtab_t nfp, ami_clrtab_t* ofp);
void _pa_funkey_ovr(ami_funkey_t nfp, ami_funkey_t* ofp);
void _pa_title_ovr(ami_title_t nfp, ami_title_t* ofp);
void _pa_getwinid_ovr(ami_getwinid_t nfp, ami_getwinid_t* ofp);
void _pa_openwin_ovr(ami_openwin_t nfp, ami_openwin_t* ofp);
void _pa_sizbufg_ovr(ami_sizbufg_t nfp, ami_sizbufg_t* ofp);
void _pa_buffer_ovr(ami_buffer_t nfp, ami_buffer_t* ofp);
void _pa_menu_ovr(ami_menu_t nfp, ami_menu_t* ofp);
void _pa_menuena_ovr(ami_menuena_t nfp, ami_menuena_t* ofp);
void _pa_menusel_ovr(ami_menusel_t nfp, ami_menusel_t* ofp);
void _pa_stdmenu_ovr(ami_stdmenu_t nfp, ami_stdmenu_t* ofp);
void _pa_front_ovr(ami_front_t nfp, ami_front_t* ofp);
void _pa_back_ovr(ami_back_t nfp, ami_back_t* ofp);
void _pa_getsizg_ovr(ami_getsizg_t nfp, ami_getsizg_t* ofp);
void _pa_getsiz_ovr(ami_getsiz_t nfp, ami_getsiz_t* ofp);
void _pa_setsizg_ovr(ami_setsizg_t nfp, ami_setsizg_t* ofp);
void _pa_setsiz_ovr(ami_setsiz_t nfp, ami_setsiz_t* ofp);
void _pa_setposg_ovr(ami_setposg_t nfp, ami_setposg_t* ofp);
void _pa_setpos_ovr(ami_setpos_t nfp, ami_setpos_t* ofp);
void _pa_scnsizg_ovr(ami_scnsizg_t nfp, ami_scnsizg_t* ofp);
void _pa_scnsiz_ovr(ami_scnsiz_t nfp, ami_scnsiz_t* ofp);
void _pa_scnceng_ovr(ami_scnceng_t nfp, ami_scnceng_t* ofp);
void _pa_scncen_ovr(ami_scncen_t nfp, ami_scncen_t* ofp);
void _pa_winclientg_ovr(ami_winclientg_t nfp, ami_winclientg_t* ofp);
void _pa_winclient_ovr(ami_winclient_t nfp, ami_winclient_t* ofp);
void _pa_frame_ovr(ami_frame_t nfp, ami_frame_t* ofp);
void _pa_sizable_ovr(ami_sizable_t nfp, ami_sizable_t* ofp);
void _pa_sysbar_ovr(ami_sysbar_t nfp, ami_sysbar_t* ofp);
void _pa_focus_ovr(ami_focus_t nfp, ami_focus_t* ofp);
void _pa_path_ovr(ami_path_t nfp, ami_path_t* ofp);
void _pa_getwigid_ovr(ami_getwigid_t nfp, ami_getwigid_t* ofp);
void _pa_killwidget_ovr(ami_killwidget_t nfp, ami_killwidget_t* ofp);
void _pa_selectwidget_ovr(ami_selectwidget_t nfp, ami_selectwidget_t* ofp);
void _pa_enablewidget_ovr(ami_enablewidget_t nfp, ami_enablewidget_t* ofp);
void _pa_getwidgettext_ovr(ami_getwidgettext_t nfp, ami_getwidgettext_t* ofp);
void _pa_putwidgettext_ovr(ami_putwidgettext_t nfp, ami_putwidgettext_t* ofp);
void _pa_sizwidget_ovr(ami_sizwidget_t nfp, ami_sizwidget_t* ofp);
void _pa_sizwidgetg_ovr(ami_sizwidgetg_t nfp, ami_sizwidgetg_t* ofp);
void _pa_poswidget_ovr(ami_poswidget_t nfp, ami_poswidget_t* ofp);
void _pa_poswidgetg_ovr(ami_poswidgetg_t nfp, ami_poswidgetg_t* ofp);
void _pa_backwidget_ovr(ami_backwidget_t nfp, ami_backwidget_t* ofp);
void _pa_frontwidget_ovr(ami_frontwidget_t nfp, ami_frontwidget_t* ofp);
void _pa_focuswidget_ovr(ami_focuswidget_t nfp, ami_focuswidget_t* ofp);
void _pa_buttonsiz_ovr(ami_buttonsiz_t nfp, ami_buttonsiz_t* ofp);
void _pa_buttonsizg_ovr(ami_buttonsizg_t nfp, ami_buttonsizg_t* ofp);
void _pa_button_ovr(ami_button_t nfp, ami_button_t* ofp);
void _pa_buttong_ovr(ami_buttong_t nfp, ami_buttong_t* ofp);
void _pa_checkboxsiz_ovr(ami_checkboxsiz_t nfp, ami_checkboxsiz_t* ofp);
void _pa_checkboxsizg_ovr(ami_checkboxsizg_t nfp, ami_checkboxsizg_t* ofp);
void _pa_checkbox_ovr(ami_checkbox_t nfp, ami_checkbox_t* ofp);
void _pa_checkboxg_ovr(ami_checkboxg_t nfp, ami_checkboxg_t* ofp);
void _pa_radiobuttonsiz_ovr(ami_radiobuttonsiz_t nfp, ami_radiobuttonsiz_t* ofp);
void _pa_radiobuttonsizg_ovr(ami_radiobuttonsizg_t nfp, ami_radiobuttonsizg_t* ofp);
void _pa_radiobutton_ovr(ami_radiobutton_t nfp, ami_radiobutton_t* ofp);
void _pa_radiobuttong_ovr(ami_radiobuttong_t nfp, ami_radiobuttong_t* ofp);
void _pa_groupsizg_ovr(ami_groupsizg_t nfp, ami_groupsizg_t* ofp);
void _pa_groupsiz_ovr(ami_groupsiz_t nfp, ami_groupsiz_t* ofp);
void _pa_group_ovr(ami_group_t nfp, ami_group_t* ofp);
void _pa_groupg_ovr(ami_groupg_t nfp, ami_groupg_t* ofp);
void _pa_background_ovr(ami_background_t nfp, ami_background_t* ofp);
void _pa_backgroundg_ovr(ami_backgroundg_t nfp, ami_backgroundg_t* ofp);
void _pa_scrollvertsizg_ovr(ami_scrollvertsizg_t nfp, ami_scrollvertsizg_t* ofp);
void _pa_scrollvertsiz_ovr(ami_scrollvertsiz_t nfp, ami_scrollvertsiz_t* ofp);
void _pa_scrollvert_ovr(ami_scrollvert_t nfp, ami_scrollvert_t* ofp);
void _pa_scrollvertg_ovr(ami_scrollvertg_t nfp, ami_scrollvertg_t* ofp);
void _pa_scrollhorizsizg_ovr(ami_scrollhorizsizg_t nfp, ami_scrollhorizsizg_t* ofp);
void _pa_scrollhorizsiz_ovr(ami_scrollhorizsiz_t nfp, ami_scrollhorizsiz_t* ofp);
void _pa_scrollhoriz_ovr(ami_scrollhoriz_t nfp, ami_scrollhoriz_t* ofp);
void _pa_scrollhorizg_ovr(ami_scrollhorizg_t nfp, ami_scrollhorizg_t* ofp);
void _pa_scrollpos_ovr(ami_scrollpos_t nfp, ami_scrollpos_t* ofp);
void _pa_scrollsiz_ovr(ami_scrollsiz_t nfp, ami_scrollsiz_t* ofp);
void _pa_numselboxsizg_ovr(ami_numselboxsizg_t nfp, ami_numselboxsizg_t* ofp);
void _pa_numselboxsiz_ovr(ami_numselboxsiz_t nfp, ami_numselboxsiz_t* ofp);
void _pa_numselbox_ovr(ami_numselbox_t nfp, ami_numselbox_t* ofp);
void _pa_numselboxg_ovr(ami_numselboxg_t nfp, ami_numselboxg_t* ofp);
void _pa_editboxsizg_ovr(ami_editboxsizg_t nfp, ami_editboxsizg_t* ofp);
void _pa_editboxsiz_ovr(ami_editboxsiz_t nfp, ami_editboxsiz_t* ofp);
void _pa_editbox_ovr(ami_editbox_t nfp, ami_editbox_t* ofp);
void _pa_editboxg_ovr(ami_editboxg_t nfp, ami_editboxg_t* ofp);
void _pa_progbarsizg_ovr(ami_progbarsizg_t nfp, ami_progbarsizg_t* ofp);
void _pa_progbarsiz_ovr(ami_progbarsiz_t nfp, ami_progbarsiz_t* ofp);
void _pa_progbar_ovr(ami_progbar_t nfp, ami_progbar_t* ofp);
void _pa_progbarg_ovr(ami_progbarg_t nfp, ami_progbarg_t* ofp);
void _pa_progbarpos_ovr(ami_progbarpos_t nfp, ami_progbarpos_t* ofp);
void _pa_listboxsizg_ovr(ami_listboxsizg_t nfp, ami_listboxsizg_t* ofp);
void _pa_listboxsiz_ovr(ami_listboxsiz_t nfp, ami_listboxsiz_t* ofp);
void _pa_listbox_ovr(ami_listbox_t nfp, ami_listbox_t* ofp);
void _pa_listboxg_ovr(ami_listboxg_t nfp, ami_listboxg_t* ofp);
void _pa_dropboxsizg_ovr(ami_dropboxsizg_t nfp, ami_dropboxsizg_t* ofp);
void _pa_dropboxsiz_ovr(ami_dropboxsiz_t nfp, ami_dropboxsiz_t* ofp);
void _pa_dropbox_ovr(ami_dropbox_t nfp, ami_dropbox_t* ofp);
void _pa_dropboxg_ovr(ami_dropboxg_t nfp, ami_dropboxg_t* ofp);
void _pa_dropeditboxsizg_ovr(ami_dropeditboxsizg_t nfp, ami_dropeditboxsizg_t* ofp);
void _pa_dropeditboxsiz_ovr(ami_dropeditboxsiz_t nfp, ami_dropeditboxsiz_t* ofp);
void _pa_dropeditbox_ovr(ami_dropeditbox_t nfp, ami_dropeditbox_t* ofp);
void _pa_dropeditboxg_ovr(ami_dropeditboxg_t nfp, ami_dropeditboxg_t* ofp);
void _pa_slidehorizsizg_ovr(ami_slidehorizsizg_t nfp, ami_slidehorizsizg_t* ofp);
void _pa_slidehorizsiz_ovr(ami_slidehorizsiz_t nfp, ami_slidehorizsiz_t* ofp);
void _pa_slidehoriz_ovr(ami_slidehoriz_t nfp, ami_slidehoriz_t* ofp);
void _pa_slidehorizg_ovr(ami_slidehorizg_t nfp, ami_slidehorizg_t* ofp);
void _pa_slidevertsizg_ovr(ami_slidevertsizg_t nfp, ami_slidevertsizg_t* ofp);
void _pa_slidevertsiz_ovr(ami_slidevertsiz_t nfp, ami_slidevertsiz_t* ofp);
void _pa_slidevert_ovr(ami_slidevert_t nfp, ami_slidevert_t* ofp);
void _pa_slidevertg_ovr(ami_slidevertg_t nfp, ami_slidevertg_t* ofp);
void _pa_tabbarsizg_ovr(ami_tabbarsizg_t nfp, ami_tabbarsizg_t* ofp);
void _pa_tabbarsiz_ovr(ami_tabbarsiz_t nfp, ami_tabbarsiz_t* ofp);
void _pa_tabbarclientg_ovr(ami_tabbarclientg_t nfp, ami_tabbarclientg_t* ofp);
void _pa_tabbarclient_ovr(ami_tabbarclient_t nfp, ami_tabbarclient_t* ofp);
void _pa_tabbar_ovr(ami_tabbar_t nfp, ami_tabbar_t* ofp);
void _pa_tabbarg_ovr(ami_tabbarg_t nfp, ami_tabbarg_t* ofp);
void _pa_tabsel_ovr(ami_tabsel_t nfp, ami_tabsel_t* ofp);
void _pa_alert_ovr(ami_alert_t nfp, ami_alert_t* ofp);
void _pa_querycolor_ovr(ami_querycolor_t nfp, ami_querycolor_t* ofp);
void _pa_queryopen_ovr(ami_queryopen_t nfp, ami_queryopen_t* ofp);
void _pa_querysave_ovr(ami_querysave_t nfp, ami_querysave_t* ofp);
void _pa_queryfind_ovr(ami_queryfind_t nfp, ami_queryfind_t* ofp);
void _pa_queryfindrep_ovr(ami_queryfindrep_t nfp, ami_queryfindrep_t* ofp);
void _pa_queryfont_ovr(ami_queryfont_t nfp, ami_queryfont_t* ofp);

#ifdef __cplusplus
}
#endif

#endif /* __GRAPH_H__ */
