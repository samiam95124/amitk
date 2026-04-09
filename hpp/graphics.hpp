/** ****************************************************************************
 *
 * Graphics library interface C++ wrapper header
 *
 * Redeclares graphics library definitions using the graphics namespace.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

#ifndef __GRAPHICS_HPP__
#define __GRAPHICS_HPP__

#include <graphics.h>

namespace graphics {

#define MAXTIM      AMI_MAXTIM      /**< maximum number of timers available */

/* standard fonts */
#define FONT_TERM   AMI_FONT_TERM   /**< terminal (fixed space) font */
#define FONT_BOOK   AMI_FONT_BOOK   /**< serif font */
#define FONT_SIGN   AMI_FONT_SIGN   /**< san-serif font */
#define FONT_TECH   AMI_FONT_TECH   /**< technical (scalable) font */

/* standardized menu entries */
#define SMNEW        AMI_SMNEW        /**< new file */
#define SMOPEN       AMI_SMOPEN       /**< open file */
#define SMCLOSE      AMI_SMCLOSE      /**< close file */
#define SMSAVE       AMI_SMSAVE       /**< save file */
#define SMSAVEAS     AMI_SMSAVEAS     /**< save file as name */
#define SMPAGESET    AMI_SMPAGESET    /**< page setup */
#define SMPRINT      AMI_SMPRINT      /**< print */
#define SMEXIT       AMI_SMEXIT       /**< exit program */
#define SMUNDO       AMI_SMUNDO       /**< undo edit */
#define SMCUT        AMI_SMCUT        /**< cut selection */
#define SMPASTE      AMI_SMPASTE      /**< paste selection */
#define SMDELETE     AMI_SMDELETE     /**< delete selection */
#define SMFIND       AMI_SMFIND       /**< find text */
#define SMFINDNEXT   AMI_SMFINDNEXT   /**< find next */
#define SMREPLACE    AMI_SMREPLACE    /**< replace text */
#define SMGOTO       AMI_SMGOTO       /**< goto line */
#define SMSELECTALL  AMI_SMSELECTALL  /**< select all text */
#define SMNEWWINDOW  AMI_SMNEWWINDOW  /**< new window */
#define SMTILEHORIZ  AMI_SMTILEHORIZ  /**< tile child windows horizontally */
#define SMTILEVERT   AMI_SMTILEVERT   /**< tile child windows vertically */
#define SMCASCADE    AMI_SMCASCADE    /**< cascade windows */
#define SMCLOSEALL   AMI_SMCLOSEALL   /**< close all windows */
#define SMHELPTOPIC  AMI_SMHELPTOPIC  /**< help topics */
#define SMABOUT      AMI_SMABOUT      /**< about this program */
#define SMMAX        AMI_SMMAX        /**< maximum defined standard menu entries */

/* colors displayable in text mode */
typedef enum { black, white, red, green, blue, cyan,
               yellow, magenta, backcolor } color;

/* events */
typedef enum {

    /** ANSI character returned */      etchar,
    /** cursor up one line */           etup,
    /** down one line */                etdown,
    /** left one character */           etleft,
    /** right one character */          etright,
    /** left one word */                etleftw,
    /** right one word */               etrightw,
    /** home of document */             ethome,
    /** home of screen */               ethomes,
    /** home of line */                 ethomel,
    /** end of document */              etend,
    /** end of screen */                etends,
    /** end of line */                  etendl,
    /** scroll left one character */    etscrl,
    /** scroll right one character */   etscrr,
    /** scroll up one line */           etscru,
    /** scroll down one line */         etscrd,
    /** page down */                    etpagd,
    /** page up */                      etpagu,
    /** tab */                          ettab,
    /** enter line */                   etenter,
    /** insert block */                 etinsert,
    /** insert line */                  etinsertl,
    /** insert toggle */                etinsertt,
    /** delete block */                 etdel,
    /** delete line */                  etdell,
    /** delete character forward */     etdelcf,
    /** delete character backward */    etdelcb,
    /** copy block */                   etcopy,
    /** copy line */                    etcopyl,
    /** cancel current operation */     etcan,
    /** stop current operation */       etstop,
    /** continue current operation */   etcont,
    /** print document */               etprint,
    /** print block */                  etprintb,
    /** print screen */                 etprints,
    /** function key */                 etfun,
    /** display menu */                 etmenu,
    /** mouse button assertion */       etmouba,
    /** mouse button deassertion */     etmoubd,
    /** mouse move */                   etmoumov,
    /** timer matures */                ettim,
    /** joystick button assertion */    etjoyba,
    /** joystick button deassertion */  etjoybd,
    /** joystick move */                etjoymov,
    /** window was resized */           etresize,
    /** window has focus */             etfocus,
    /** window lost focus */            etnofocus,
    /** window being hovered */         ethover,
    /** window stopped being hovered */ etnohover,
    /** terminate program */            etterm,
    /** frame sync */                   etframe,
    /** mouse move graphical */         etmoumovg,
    /** window redraw */                etredraw,
    /** window minimized */             etmin,
    /** window maximized */             etmax,
    /** window normalized */            etnorm,
    /** menu item selected */           etmenus,
    /** button assert */                etbutton,
    /** checkbox click */               etchkbox,
    /** radio button click */           etradbut,
    /** scroll up/left line */          etsclull,
    /** scroll down/right line */       etscldrl,
    /** scroll up/left page */          etsclulp,
    /** scroll down/right page */       etscldrp,
    /** scroll bar position */          etsclpos,
    /** edit box signals done */        etedtbox,
    /** number select box signals done */ etnumbox,
    /** list box selection */           etlstbox,
    /** drop box selection */           etdrpbox,
    /** drop edit box signals done */   etdrebox,
    /** slider position */              etsldpos,
    /** tab bar select */               ettabbar,

    /* Reserved extra code areas, these are module defined. */
    etsys    = 0x1000, /**< start of base system reserved codes */
    etman    = 0x2000, /**< start of window management reserved codes */
    etwidget = 0x3000, /**< start of widget reserved codes */
    etuser   = 0x4000  /**< start of user defined codes */

} evtcod;

/* event record */
typedef struct {

    /* identifier of window for event */ int winid;
    /* event type */                     evtcod etype;
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
        /* etjoyba */
        struct {

            /** joystick number */ int ajoyn;
            /** button number */   int ajoybn;

        };
        /* etjoybd */
        struct {

            /** joystick number */ int djoyn;
            /** button number */   int djoybn;

        };
        /* etjoymov */
        struct {

            /** joystick number */      int mjoyn;
            /** joystick coordinates */ int joypx, joypy, joypz;
                                        int joyp4, joyp5, joyp6;

        };
        /* etfun */
        /** function key */ int fkey;
        /* etresize */
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
        /* etmenus */
        int menuid; /**< menu item selected */
        /* etbutton */
        int butid; /**< button id */
        /* etchkbox */
        int ckbxid; /**< checkbox id */
        /* etradbut */
        int radbid; /**< radio button id */
        /* etsclull */
        int sclulid; /**< scroll up/left line id */
        /* etscldrl */
        int scldrid; /**< scroll down/right line id */
        /* etsclulp */
        int sclupid; /**< scroll up/left page id */
        /* etscldrp */
        int scldpid; /**< scroll down/right page id */
        /* etsclpos */
        struct {

            int sclpid; /**< scroll bar id */
            int sclpos; /**< scroll bar position */

        };
        /* etedtbox */
        int edtbid; /**< edit box complete id */
        /* etnumbox */
        struct {

            int numbid; /**< num sel box id */
            int numbsl; /**< num select value */

        };
        /* etlstbox */
        struct {

            int lstbid; /**< list box id */
            int lstbsl; /**< list box select number */

        };
        /* etdrpbox */
        struct {

            int drpbid; /**< drop box id */
            int drpbsl; /**< drop box select */

        };
        /* etdrebox */
        int drebid; /**< drop edit box id */
        /* etsldpos */
        struct {

            int sldpid; /**< slider id */
            int sldpos; /**< slider position */

        };
        /* ettabbar */
        struct {

            int tabid;  /**< tab bar id */
            int tabsel; /**< tab select */

        };

     };

} evtrec, *evtptr;

/** event function pointer */
typedef void (*pevthan)(evtrec*);

/* menu record */
typedef struct menurec* menuptr;
typedef struct menurec {

        menuptr next;   /**< next menu item in list */
        menuptr branch; /**< menu branch */
        int     onoff;  /**< on/off highlight */
        int     oneof;  /**< "one of" highlight */
        int     bar;    /**< place bar under */
        int     id;     /**< id of menu item */
        char*   face;   /**< text to place on button */

} menurec;

/* standard menu selector */
typedef int stdmenusel;

/* windows mode sets */
typedef enum {

    wmframe, /**< frame on/off */
    wmsize,  /**< size bars on/off */
    wmsysbar /**< system bar on/off */

} winmod;
typedef int winmodset;

/* string set for list box */
typedef struct strrec* strptr;
typedef struct strrec {

    strptr next; /**< next entry in list */
    char*  str;  /**< string */

} strrec;

/* orientation for tab bars */
typedef enum { totop, toright, tobottom, toleft } tabori;

/* settable items in find query */
typedef enum { qfncase, qfnup, qfnre } qfnopt;
typedef int qfnopts;

/* settable items in replace query */
typedef enum { qfrcase, qfrup, qfrre, qfrfind, qfrallfil, qfralllin } qfropt;
typedef int qfropts;

/* effects in font query */
typedef enum { qfteblink, qftereverse, qfteunderline, qftesuperscript,
               qftesubscript, qfteitalic, qftebold, qftestrikeout,
               qftestandout, qftecondensed, qfteextended, qftexlight,
               qftelight, qftexbold, qftehollow, qfteraised } qfteffect;
typedef int qfteffects;

/* procedural interface */

/* text */
void cursor(FILE* f, int x, int y);
void cursor(int x, int y);
int  maxx(FILE* f);
int  maxx(void);
int  maxy(FILE* f);
int  maxy(void);
void home(FILE* f);
void home(void);
void del(FILE* f);
void del(void);
void up(FILE* f);
void up(void);
void down(FILE* f);
void down(void);
void left(FILE* f);
void left(void);
void right(FILE* f);
void right(void);
void blink(FILE* f, int e);
void blink(int e);
void reverse(FILE* f, int e);
void reverse(int e);
void underline(FILE* f, int e);
void underline(int e);
void superscript(FILE* f, int e);
void superscript(int e);
void subscript(FILE* f, int e);
void subscript(int e);
void italic(FILE* f, int e);
void italic(int e);
void bold(FILE* f, int e);
void bold(int e);
void strikeout(FILE* f, int e);
void strikeout(int e);
void standout(FILE* f, int e);
void standout(int e);
void fcolor(FILE* f, color c);
void fcolor(color c);
void bcolor(FILE* f, color c);
void bcolor(color c);
void autom(FILE* f, int e);
void autom(int e);
void curvis(FILE* f, int e);
void curvis(int e);
void scroll(FILE* f, int x, int y);
void scroll(int x, int y);
int  curx(FILE* f);
int  curx(void);
int  cury(FILE* f);
int  cury(void);
int  curbnd(FILE* f);
int  curbnd(void);
void select(FILE* f, int u, int d);
void select(int u, int d);
void event(FILE* f, evtrec* er);
void event(evtrec* er);
void timer(FILE* f, int i, long t, int r);
void timer(int i, long t, int r);
void killtimer(FILE* f, int i);
void killtimer(int i);
int  mouse(FILE* f);
int  mouse(void);
int  mousebutton(FILE* f, int m);
int  mousebutton(int m);
int  joystick(FILE* f);
int  joystick(void);
int  joybutton(FILE* f, int j);
int  joybutton(int j);
int  joyaxis(FILE* f, int j);
int  joyaxis(int j);
void settab(FILE* f, int t);
void settab(int t);
void restab(FILE* f, int t);
void restab(int t);
void clrtab(FILE* f);
void clrtab(void);
int  funkey(FILE* f);
int  funkey(void);
void frametimer(FILE* f, int e);
void frametimer(int e);
void autohold(int e);
void wrtstr(FILE* f, char* s);
void wrtstr(char* s);
void wrtstrn(FILE* f, char* s, int n);
void wrtstrn(char* s, int n);
void sizbuf(FILE* f, int x, int y);
void sizbuf(int x, int y);
void title(FILE* f, char* ts);
void title(char* ts);
void eventover(evtcod e, pevthan eh, pevthan* oeh);
void eventsover(pevthan eh, pevthan* oeh);
void sendevent(FILE* f, evtrec* er);
void sendevent(evtrec* er);

/* graphical */
int  maxxg(FILE* f);
int  maxxg(void);
int  maxyg(FILE* f);
int  maxyg(void);
int  curxg(FILE* f);
int  curxg(void);
int  curyg(FILE* f);
int  curyg(void);
void line(FILE* f, int x1, int y1, int x2, int y2);
void line(int x1, int y1, int x2, int y2);
void linewidth(FILE* f, int w);
void linewidth(int w);
void rect(FILE* f, int x1, int y1, int x2, int y2);
void rect(int x1, int y1, int x2, int y2);
void frect(FILE* f, int x1, int y1, int x2, int y2);
void frect(int x1, int y1, int x2, int y2);
void rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
void rrect(int x1, int y1, int x2, int y2, int xs, int ys);
void frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys);
void frrect(int x1, int y1, int x2, int y2, int xs, int ys);
void ellipse(FILE* f, int x1, int y1, int x2, int y2);
void ellipse(int x1, int y1, int x2, int y2);
void fellipse(FILE* f, int x1, int y1, int x2, int y2);
void fellipse(int x1, int y1, int x2, int y2);
void arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void arc(int x1, int y1, int x2, int y2, int sa, int ea);
void farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void farc(int x1, int y1, int x2, int y2, int sa, int ea);
void fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea);
void fchord(int x1, int y1, int x2, int y2, int sa, int ea);
void ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3);
void ftriangle(int x1, int y1, int x2, int y2, int x3, int y3);
void cursorg(FILE* f, int x, int y);
void cursorg(int x, int y);
int  baseline(FILE* f);
int  baseline(void);
void setpixel(FILE* f, int x, int y);
void setpixel(int x, int y);
void fover(FILE* f);
void fover(void);
void bover(FILE* f);
void bover(void);
void finvis(FILE* f);
void finvis(void);
void binvis(FILE* f);
void binvis(void);
void fxor(FILE* f);
void fxor(void);
void bxor(FILE* f);
void bxor(void);
void fand(FILE* f);
void fand(void);
void band(FILE* f);
void band(void);
void for_(FILE* f);
void for_(void);
void bor(FILE* f);
void bor(void);
int  chrsizx(FILE* f);
int  chrsizx(void);
int  chrsizy(FILE* f);
int  chrsizy(void);
int  fonts(FILE* f);
int  fonts(void);
void font(FILE* f, int fc);
void font(int fc);
void fontnam(FILE* f, int fc, char* fns, int fnsl);
void fontnam(int fc, char* fns, int fnsl);
void fontsiz(FILE* f, int s);
void fontsiz(int s);
void chrspcy(FILE* f, int s);
void chrspcy(int s);
void chrspcx(FILE* f, int s);
void chrspcx(int s);
int  dpmx(FILE* f);
int  dpmx(void);
int  dpmy(FILE* f);
int  dpmy(void);
int  strsiz(FILE* f, const char* s);
int  strsiz(const char* s);
int  chrpos(FILE* f, const char* s, int p);
int  chrpos(const char* s, int p);
void writejust(FILE* f, const char* s, int n);
void writejust(const char* s, int n);
int  justpos(FILE* f, const char* s, int p, int n);
int  justpos(const char* s, int p, int n);
void condensed(FILE* f, int e);
void condensed(int e);
void extended(FILE* f, int e);
void extended(int e);
void xlight(FILE* f, int e);
void xlight(int e);
void light(FILE* f, int e);
void light(int e);
void xbold(FILE* f, int e);
void xbold(int e);
void hollow(FILE* f, int e);
void hollow(int e);
void raised(FILE* f, int e);
void raised(int e);
void settabg(FILE* f, int t);
void settabg(int t);
void restabg(FILE* f, int t);
void restabg(int t);
void fcolorg(FILE* f, int r, int g, int b);
void fcolorg(int r, int g, int b);
void fcolorc(FILE* f, int r, int g, int b);
void fcolorc(int r, int g, int b);
void bcolorg(FILE* f, int r, int g, int b);
void bcolorg(int r, int g, int b);
void bcolorc(FILE* f, int r, int g, int b);
void bcolorc(int r, int g, int b);
void loadpict(FILE* f, int p, char* fn);
void loadpict(int p, char* fn);
int  pictsizx(FILE* f, int p);
int  pictsizx(int p);
int  pictsizy(FILE* f, int p);
int  pictsizy(int p);
void picture(FILE* f, int p, int x1, int y1, int x2, int y2);
void picture(int p, int x1, int y1, int x2, int y2);
void delpict(FILE* f, int p);
void delpict(int p);
void scrollg(FILE* f, int x, int y);
void scrollg(int x, int y);
void path(FILE* f, int a);
void path(int a);

/* window management */
void openwin(FILE** infile, FILE** outfile, FILE* parent, int wid);
void buffer(FILE* f, int e);
void buffer(int e);
void sizbufg(FILE* f, int x, int y);
void sizbufg(int x, int y);
void getsiz(FILE* f, int* x, int* y);
void getsiz(int* x, int* y);
void getsizg(FILE* f, int* x, int* y);
void getsizg(int* x, int* y);
void setsiz(FILE* f, int x, int y);
void setsiz(int x, int y);
void setsizg(FILE* f, int x, int y);
void setsizg(int x, int y);
void setpos(FILE* f, int x, int y);
void setpos(int x, int y);
void setposg(FILE* f, int x, int y);
void setposg(int x, int y);
void scnsiz(FILE* f, int* x, int* y);
void scnsiz(int* x, int* y);
void scnsizg(FILE* f, int* x, int* y);
void scnsizg(int* x, int* y);
void scncen(FILE* f, int* x, int* y);
void scncen(int* x, int* y);
void scnceng(FILE* f, int* x, int* y);
void scnceng(int* x, int* y);
void winclient(FILE* f, int cx, int cy, int* wx, int* wy, winmodset ms);
void winclient(int cx, int cy, int* wx, int* wy, winmodset ms);
void winclientg(FILE* f, int cx, int cy, int* wx, int* wy, winmodset ms);
void winclientg(int cx, int cy, int* wx, int* wy, winmodset ms);
void front(FILE* f);
void front(void);
void back(FILE* f);
void back(void);
void frame(FILE* f, int e);
void frame(int e);
void sizable(FILE* f, int e);
void sizable(int e);
void sysbar(FILE* f, int e);
void sysbar(int e);
void menu(FILE* f, menuptr m);
void menu(menuptr m);
void menuena(FILE* f, int id, int onoff);
void menuena(int id, int onoff);
void menusel(FILE* f, int id, int select);
void menusel(int id, int select);
void stdmenu(stdmenusel sms, menuptr* sm, menuptr pm);
int  getwinid(void);
void focus(FILE* f);
void focus(void);

/* widgets/controls */
int  getwigid(FILE* f);
int  getwigid(void);
void killwidget(FILE* f, int id);
void killwidget(int id);
void selectwidget(FILE* f, int id, int e);
void selectwidget(int id, int e);
void enablewidget(FILE* f, int id, int e);
void enablewidget(int id, int e);
void getwidgettext(FILE* f, int id, char* s, int sl);
void getwidgettext(int id, char* s, int sl);
void putwidgettext(FILE* f, int id, char* s);
void putwidgettext(int id, char* s);
void sizwidget(FILE* f, int id, int x, int y);
void sizwidget(int id, int x, int y);
void sizwidgetg(FILE* f, int id, int x, int y);
void sizwidgetg(int id, int x, int y);
void poswidget(FILE* f, int id, int x, int y);
void poswidget(int id, int x, int y);
void poswidgetg(FILE* f, int id, int x, int y);
void poswidgetg(int id, int x, int y);
void backwidget(FILE* f, int id);
void backwidget(int id);
void frontwidget(FILE* f, int id);
void frontwidget(int id);
void focuswidget(FILE* f, int id);
void focuswidget(int id);
void buttonsiz(FILE* f, char* s, int* w, int* h);
void buttonsiz(char* s, int* w, int* h);
void buttonsizg(FILE* f, char* s, int* w, int* h);
void buttonsizg(char* s, int* w, int* h);
void button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void button(int x1, int y1, int x2, int y2, char* s, int id);
void buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void buttong(int x1, int y1, int x2, int y2, char* s, int id);
void checkboxsiz(FILE* f, char* s, int* w, int* h);
void checkboxsiz(char* s, int* w, int* h);
void checkboxsizg(FILE* f, char* s, int* w, int* h);
void checkboxsizg(char* s, int* w, int* h);
void checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void checkbox(int x1, int y1, int x2, int y2, char* s, int id);
void checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void checkboxg(int x1, int y1, int x2, int y2, char* s, int id);
void radiobuttonsiz(FILE* f, char* s, int* w, int* h);
void radiobuttonsiz(char* s, int* w, int* h);
void radiobuttonsizg(FILE* f, char* s, int* w, int* h);
void radiobuttonsizg(char* s, int* w, int* h);
void radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void radiobutton(int x1, int y1, int x2, int y2, char* s, int id);
void radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void radiobuttong(int x1, int y1, int x2, int y2, char* s, int id);
void groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy);
void groupsiz(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy);
void groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy);
void groupsizg(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy);
void group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void group(int x1, int y1, int x2, int y2, char* s, int id);
void groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id);
void groupg(int x1, int y1, int x2, int y2, char* s, int id);
void background(FILE* f, int x1, int y1, int x2, int y2, int id);
void background(int x1, int y1, int x2, int y2, int id);
void backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id);
void backgroundg(int x1, int y1, int x2, int y2, int id);
void scrollvertsiz(FILE* f, int* w, int* h);
void scrollvertsiz(int* w, int* h);
void scrollvertsizg(FILE* f, int* w, int* h);
void scrollvertsizg(int* w, int* h);
void scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id);
void scrollvert(int x1, int y1, int x2, int y2, int id);
void scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id);
void scrollvertg(int x1, int y1, int x2, int y2, int id);
void scrollhorizsiz(FILE* f, int* w, int* h);
void scrollhorizsiz(int* w, int* h);
void scrollhorizsizg(FILE* f, int* w, int* h);
void scrollhorizsizg(int* w, int* h);
void scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id);
void scrollhoriz(int x1, int y1, int x2, int y2, int id);
void scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id);
void scrollhorizg(int x1, int y1, int x2, int y2, int id);
void scrollpos(FILE* f, int id, int r);
void scrollpos(int id, int r);
void scrollsiz(FILE* f, int id, int r);
void scrollsiz(int id, int r);
void numselboxsiz(FILE* f, int l, int u, int* w, int* h);
void numselboxsiz(int l, int u, int* w, int* h);
void numselboxsizg(FILE* f, int l, int u, int* w, int* h);
void numselboxsizg(int l, int u, int* w, int* h);
void numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id);
void numselbox(int x1, int y1, int x2, int y2, int l, int u, int id);
void numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id);
void numselboxg(int x1, int y1, int x2, int y2, int l, int u, int id);
void editboxsiz(FILE* f, char* s, int* w, int* h);
void editboxsiz(char* s, int* w, int* h);
void editboxsizg(FILE* f, char* s, int* w, int* h);
void editboxsizg(char* s, int* w, int* h);
void editbox(FILE* f, int x1, int y1, int x2, int y2, int id);
void editbox(int x1, int y1, int x2, int y2, int id);
void editboxg(FILE* f, int x1, int y1, int x2, int y2, int id);
void editboxg(int x1, int y1, int x2, int y2, int id);
void progbarsiz(FILE* f, int* w, int* h);
void progbarsiz(int* w, int* h);
void progbarsizg(FILE* f, int* w, int* h);
void progbarsizg(int* w, int* h);
void progbar(FILE* f, int x1, int y1, int x2, int y2, int id);
void progbar(int x1, int y1, int x2, int y2, int id);
void progbarg(FILE* f, int x1, int y1, int x2, int y2, int id);
void progbarg(int x1, int y1, int x2, int y2, int id);
void progbarpos(FILE* f, int id, int pos);
void progbarpos(int id, int pos);
void listboxsiz(FILE* f, strptr sp, int* w, int* h);
void listboxsiz(strptr sp, int* w, int* h);
void listboxsizg(FILE* f, strptr sp, int* w, int* h);
void listboxsizg(strptr sp, int* w, int* h);
void listbox(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id);
void listbox(int x1, int y1, int x2, int y2, strptr sp, int id);
void listboxg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id);
void listboxg(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropboxsiz(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropboxsizg(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropbox(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id);
void dropbox(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropboxg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id);
void dropboxg(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropeditboxsiz(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropeditboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropeditboxsizg(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropeditboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropeditbox(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id);
void dropeditbox(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id);
void dropeditboxg(int x1, int y1, int x2, int y2, strptr sp, int id);
void slidehorizsiz(FILE* f, int* w, int* h);
void slidehorizsiz(int* w, int* h);
void slidehorizsizg(FILE* f, int* w, int* h);
void slidehorizsizg(int* w, int* h);
void slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void slidehoriz(int x1, int y1, int x2, int y2, int mark, int id);
void slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void slidehorizg(int x1, int y1, int x2, int y2, int mark, int id);
void slidevertsiz(FILE* f, int* w, int* h);
void slidevertsiz(int* w, int* h);
void slidevertsizg(FILE* f, int* w, int* h);
void slidevertsizg(int* w, int* h);
void slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void slidevert(int x1, int y1, int x2, int y2, int mark, int id);
void slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id);
void slidevertg(int x1, int y1, int x2, int y2, int mark, int id);
void tabbarsiz(FILE* f, tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy);
void tabbarsiz(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy);
void tabbarsizg(FILE* f, tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy);
void tabbarsizg(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy);
void tabbarclient(FILE* f, tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy);
void tabbarclient(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy);
void tabbarclientg(FILE* f, tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy);
void tabbarclientg(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy);
void tabbar(FILE* f, int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id);
void tabbar(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id);
void tabbarg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id);
void tabbarg(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id);
void tabsel(FILE* f, int id, int tn);
void tabsel(int id, int tn);

/* dialogs */
void alert(char* title, char* message);
void querycolor(int* r, int* g, int* b);
void queryopen(char* s, int sl);
void querysave(char* s, int sl);
void queryfind(char* s, int sl, qfnopts* opt);
void queryfindrep(char* s, int sl, char* r, int rl, qfropts* opt);
void queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
               int* bg, int* bb, qfteffects* effect);
void queryfont(int* fc, int* s, int* fr, int* fg, int* fb, int* br,
               int* bg, int* bb, qfteffects* effect);

/* object based interface */
class graph {

FILE* infile;
FILE* outfile;

public:

/* constructor */
graph();

/* methods */

/* text */
void cursor(int x, int y);
int  maxx(void);
int  maxy(void);
void home(void);
void del(void);
void up(void);
void down(void);
void left(void);
void right(void);
void blink(int e);
void reverse(int e);
void underline(int e);
void superscript(int e);
void subscript(int e);
void italic(int e);
void bold(int e);
void strikeout(int e);
void standout(int e);
void fcolor(color c);
void bcolor(color c);
void autom(int e);
void curvis(int e);
void scroll(int x, int y);
int  curx(void);
int  cury(void);
int  curbnd(void);
void select(int u, int d);
void event(evtrec* er);
void timer(int i, long t, int r);
void killtimer(int i);
int  mouse(void);
int  mousebutton(int m);
int  joystick(void);
int  joybutton(int j);
int  joyaxis(int j);
void settab(int t);
void restab(int t);
void clrtab(void);
int  funkey(void);
void frametimer(int e);
void autohold(int e);
void wrtstr(char* s);
void wrtstrn(char* s, int n);
void sizbuf(int x, int y);
void title(char* ts);
void sendevent(evtrec* er);

/* graphical */
int  maxxg(void);
int  maxyg(void);
int  curxg(void);
int  curyg(void);
void line(int x1, int y1, int x2, int y2);
void linewidth(int w);
void rect(int x1, int y1, int x2, int y2);
void frect(int x1, int y1, int x2, int y2);
void rrect(int x1, int y1, int x2, int y2, int xs, int ys);
void frrect(int x1, int y1, int x2, int y2, int xs, int ys);
void ellipse(int x1, int y1, int x2, int y2);
void fellipse(int x1, int y1, int x2, int y2);
void arc(int x1, int y1, int x2, int y2, int sa, int ea);
void farc(int x1, int y1, int x2, int y2, int sa, int ea);
void fchord(int x1, int y1, int x2, int y2, int sa, int ea);
void ftriangle(int x1, int y1, int x2, int y2, int x3, int y3);
void cursorg(int x, int y);
int  baseline(void);
void setpixel(int x, int y);
void fover(void);
void bover(void);
void finvis(void);
void binvis(void);
void fxor(void);
void bxor(void);
void fand(void);
void band(void);
void for_(void);
void bor(void);
int  chrsizx(void);
int  chrsizy(void);
int  fonts(void);
void font(int fc);
void fontnam(int fc, char* fns, int fnsl);
void fontsiz(int s);
void chrspcy(int s);
void chrspcx(int s);
int  dpmx(void);
int  dpmy(void);
int  strsiz(const char* s);
int  chrpos(const char* s, int p);
void writejust(const char* s, int n);
int  justpos(const char* s, int p, int n);
void condensed(int e);
void extended(int e);
void xlight(int e);
void light(int e);
void xbold(int e);
void hollow(int e);
void raised(int e);
void settabg(int t);
void restabg(int t);
void fcolorg(int r, int g, int b);
void fcolorc(int r, int g, int b);
void bcolorg(int r, int g, int b);
void bcolorc(int r, int g, int b);
void loadpict(int p, char* fn);
int  pictsizx(int p);
int  pictsizy(int p);
void picture(int p, int x1, int y1, int x2, int y2);
void delpict(int p);
void scrollg(int x, int y);
void path(int a);

/* window management */
void buffer(int e);
void sizbufg(int x, int y);
void getsiz(int* x, int* y);
void getsizg(int* x, int* y);
void setsiz(int x, int y);
void setsizg(int x, int y);
void setpos(int x, int y);
void setposg(int x, int y);
void scnsiz(int* x, int* y);
void scnsizg(int* x, int* y);
void scncen(int* x, int* y);
void scnceng(int* x, int* y);
void winclient(int cx, int cy, int* wx, int* wy, winmodset ms);
void winclientg(int cx, int cy, int* wx, int* wy, winmodset ms);
void front(void);
void back(void);
void frame(int e);
void sizable(int e);
void sysbar(int e);
void menu(menuptr m);
void menuena(int id, int onoff);
void menusel(int id, int select);
void focus(void);

/* widgets */
int  getwigid(void);
void killwidget(int id);
void selectwidget(int id, int e);
void enablewidget(int id, int e);
void getwidgettext(int id, char* s, int sl);
void putwidgettext(int id, char* s);
void sizwidget(int id, int x, int y);
void sizwidgetg(int id, int x, int y);
void poswidget(int id, int x, int y);
void poswidgetg(int id, int x, int y);
void backwidget(int id);
void frontwidget(int id);
void focuswidget(int id);
void buttonsiz(char* s, int* w, int* h);
void buttonsizg(char* s, int* w, int* h);
void button(int x1, int y1, int x2, int y2, char* s, int id);
void buttong(int x1, int y1, int x2, int y2, char* s, int id);
void checkboxsiz(char* s, int* w, int* h);
void checkboxsizg(char* s, int* w, int* h);
void checkbox(int x1, int y1, int x2, int y2, char* s, int id);
void checkboxg(int x1, int y1, int x2, int y2, char* s, int id);
void radiobuttonsiz(char* s, int* w, int* h);
void radiobuttonsizg(char* s, int* w, int* h);
void radiobutton(int x1, int y1, int x2, int y2, char* s, int id);
void radiobuttong(int x1, int y1, int x2, int y2, char* s, int id);
void groupsiz(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy);
void groupsizg(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy);
void group(int x1, int y1, int x2, int y2, char* s, int id);
void groupg(int x1, int y1, int x2, int y2, char* s, int id);
void background(int x1, int y1, int x2, int y2, int id);
void backgroundg(int x1, int y1, int x2, int y2, int id);
void scrollvertsiz(int* w, int* h);
void scrollvertsizg(int* w, int* h);
void scrollvert(int x1, int y1, int x2, int y2, int id);
void scrollvertg(int x1, int y1, int x2, int y2, int id);
void scrollhorizsiz(int* w, int* h);
void scrollhorizsizg(int* w, int* h);
void scrollhoriz(int x1, int y1, int x2, int y2, int id);
void scrollhorizg(int x1, int y1, int x2, int y2, int id);
void scrollpos(int id, int r);
void scrollsiz(int id, int r);
void numselboxsiz(int l, int u, int* w, int* h);
void numselboxsizg(int l, int u, int* w, int* h);
void numselbox(int x1, int y1, int x2, int y2, int l, int u, int id);
void numselboxg(int x1, int y1, int x2, int y2, int l, int u, int id);
void editboxsiz(char* s, int* w, int* h);
void editboxsizg(char* s, int* w, int* h);
void editbox(int x1, int y1, int x2, int y2, int id);
void editboxg(int x1, int y1, int x2, int y2, int id);
void progbarsiz(int* w, int* h);
void progbarsizg(int* w, int* h);
void progbar(int x1, int y1, int x2, int y2, int id);
void progbarg(int x1, int y1, int x2, int y2, int id);
void progbarpos(int id, int pos);
void listboxsiz(strptr sp, int* w, int* h);
void listboxsizg(strptr sp, int* w, int* h);
void listbox(int x1, int y1, int x2, int y2, strptr sp, int id);
void listboxg(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropbox(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropboxg(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropeditboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropeditboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh);
void dropeditbox(int x1, int y1, int x2, int y2, strptr sp, int id);
void dropeditboxg(int x1, int y1, int x2, int y2, strptr sp, int id);
void slidehorizsiz(int* w, int* h);
void slidehorizsizg(int* w, int* h);
void slidehoriz(int x1, int y1, int x2, int y2, int mark, int id);
void slidehorizg(int x1, int y1, int x2, int y2, int mark, int id);
void slidevertsiz(int* w, int* h);
void slidevertsizg(int* w, int* h);
void slidevert(int x1, int y1, int x2, int y2, int mark, int id);
void slidevertg(int x1, int y1, int x2, int y2, int mark, int id);
void tabbarsiz(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy);
void tabbarsizg(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy);
void tabbarclient(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy);
void tabbarclientg(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy);
void tabbar(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id);
void tabbarg(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id);
void tabsel(int id, int tn);

/* dialogs */
void queryfont(int* fc, int* s, int* fr, int* fg, int* fb, int* br,
               int* bg, int* bb, qfteffects* effect);

static void graphCB(evtrec* er);

/* virtual callbacks */
virtual int evchar(char c);
virtual int evup(void);
virtual int evdown(void);
virtual int evleft(void);
virtual int evright(void);
virtual int evleftw(void);
virtual int evrightw(void);
virtual int evhome(void);
virtual int evhomes(void);
virtual int evhomel(void);
virtual int evend(void);
virtual int evends(void);
virtual int evendl(void);
virtual int evscrl(void);
virtual int evscrr(void);
virtual int evscru(void);
virtual int evscrd(void);
virtual int evpagd(void);
virtual int evpagu(void);
virtual int evtab(void);
virtual int eventer(void);
virtual int evinsert(void);
virtual int evinsertl(void);
virtual int evinsertt(void);
virtual int evdel(void);
virtual int evdell(void);
virtual int evdelcf(void);
virtual int evdelcb(void);
virtual int evcopy(void);
virtual int evcopyl(void);
virtual int evcan(void);
virtual int evstop(void);
virtual int evcont(void);
virtual int evprint(void);
virtual int evprintb(void);
virtual int evprints(void);
virtual int evfun(int k);
virtual int evmenu(void);
virtual int evmouba(int m, int b);
virtual int evmoubd(int m, int b);
virtual int evmoumov(int m, int x, int y);
virtual int evtim(int t);
virtual int evjoyba(int j, int b);
virtual int evjoybd(int j, int b);
virtual int evjoymov(int j, int x, int y, int z);
virtual int evresize(void);
virtual int evfocus(void);
virtual int evnofocus(void);
virtual int evhover(void);
virtual int evnohover(void);
virtual int evterm(void);
virtual int evframe(void);
virtual int evmoumovg(int m, int x, int y);
virtual int evredraw(int x1, int y1, int x2, int y2);
virtual int evmin(void);
virtual int evmax(void);
virtual int evnorm(void);
virtual int evmenus(int id);
virtual int evbutton(int id);
virtual int evchkbox(int id);
virtual int evradbut(int id);
virtual int evsclull(int id);
virtual int evscldrl(int id);
virtual int evsclulp(int id);
virtual int evscldrp(int id);
virtual int evsclpos(int id, int pos);
virtual int evedtbox(int id);
virtual int evnumbox(int id, int val);
virtual int evlstbox(int id, int sel);
virtual int evdrpbox(int id, int sel);
virtual int evdrebox(int id);
virtual int evsldpos(int id, int pos);
virtual int evtabbar(int id, int sel);

}; /* class graph */

} /* namespace graphics */

#endif /* __GRAPHICS_HPP__ */
