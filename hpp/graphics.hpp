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
    /** enlarge what is displayed */    etusize,
    /** reduce what is displayed */     etdsize,

    /* Reserved extra code areas, these are module defined. */
    etsys    = 0x1000, /**< start of base system reserved codes */
    etman    = 0x2000, /**< start of window management reserved codes */
    etwidget = 0x3000, /**< start of widget reserved codes */
    etuser   = 0x4000  /**< start of user defined codes */

} evtcod;

/* event record */
typedef struct {

    /* identifier of window for event */ ami_long winid;
    /* event type */                     evtcod etype;
    /* event was handled */              ami_long handled;
    union {

        /* these events require parameter data */

        /** etchar: ANSI character returned */  char echar;
        /** ettim: timer handle that matured */ ami_long timnum;
        /** etmoumov: */
        struct {

            /** mouse number */   ami_long mmoun;
            /** mouse movement */ ami_long moupx, moupy;

        };
        /* etmouba */
        struct {

            /** mouse handle */  ami_long amoun;
            /** button number */ ami_long amoubn;

        };
        /* etmoubd */
        struct {

            /** mouse handle */  ami_long dmoun;
            /** button number */ ami_long dmoubn;

        };
        /* etjoyba */
        struct {

            /** joystick number */ ami_long ajoyn;
            /** button number */   ami_long ajoybn;

        };
        /* etjoybd */
        struct {

            /** joystick number */ ami_long djoyn;
            /** button number */   ami_long djoybn;

        };
        /* etjoymov */
        struct {

            /** joystick number */      ami_long mjoyn;
            /** joystick coordinates */ ami_long joypx, joypy, joypz;
                                        ami_long joyp4, joyp5, joyp6;

        };
        /* etfun */
        /** function key */ ami_long fkey;
        /* etresize */
        struct {

            ami_long rszx, rszy, rszxg, rszyg;

        };

        /** etmoumovg: */
        struct {

            /** mouse number */   ami_long mmoung;
            /** mouse movement */ ami_long moupxg, moupyg;

        };
        /** etredraw */
        struct {

            /** bounding rectangle */
            ami_long rsx, rsy, rex, rey;

        };
        /* etmenus */
        ami_long menuid; /**< menu item selected */
        /* etbutton */
        ami_long butid; /**< button id */
        /* etchkbox */
        ami_long ckbxid; /**< checkbox id */
        /* etradbut */
        ami_long radbid; /**< radio button id */
        /* etsclull */
        ami_long sclulid; /**< scroll up/left line id */
        /* etscldrl */
        ami_long scldrid; /**< scroll down/right line id */
        /* etsclulp */
        ami_long sclupid; /**< scroll up/left page id */
        /* etscldrp */
        ami_long scldpid; /**< scroll down/right page id */
        /* etsclpos */
        struct {

            ami_long sclpid; /**< scroll bar id */
            ami_long sclpos; /**< scroll bar position */

        };
        /* etedtbox */
        ami_long edtbid; /**< edit box complete id */
        /* etnumbox */
        struct {

            ami_long numbid; /**< num sel box id */
            ami_long numbsl; /**< num select value */

        };
        /* etlstbox */
        struct {

            ami_long lstbid; /**< list box id */
            ami_long lstbsl; /**< list box select number */

        };
        /* etdrpbox */
        struct {

            ami_long drpbid; /**< drop box id */
            ami_long drpbsl; /**< drop box select */

        };
        /* etdrebox */
        ami_long drebid; /**< drop edit box id */
        /* etsldpos */
        struct {

            ami_long sldpid; /**< slider id */
            ami_long sldpos; /**< slider position */

        };
        /* ettabbar */
        struct {

            ami_long tabid;  /**< tab bar id */
            ami_long tabsel; /**< tab select */

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
        ami_long onoff;  /**< on/off highlight */
        ami_long oneof;  /**< "one of" highlight */
        ami_long bar;    /**< place bar under */
        ami_long id;     /**< id of menu item */
        char*   face;   /**< text to place on button */

} menurec;

/* standard menu selector */
typedef ami_long stdmenusel;

/* windows mode sets */
typedef enum {

    wmframe, /**< frame on/off */
    wmsize,  /**< size bars on/off */
    wmsysbar /**< system bar on/off */

} winmod;
typedef ami_long winmodset;

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
typedef ami_long qfnopts;

/* settable items in replace query */
typedef enum { qfrcase, qfrup, qfrre, qfrfind, qfrallfil, qfralllin } qfropt;
typedef ami_long qfropts;

/* effects in font query */
typedef enum { qfteblink, qftereverse, qfteunderline, qftesuperscript,
               qftesubscript, qfteitalic, qftebold, qftestrikeout,
               qftestandout, qftecondensed, qfteextended, qftexlight,
               qftelight, qftexbold, qftehollow, qfteraised } qfteffect;
typedef ami_long qfteffects;

/* procedural interface */

/* text */
void cursor(FILE* f, ami_long x, ami_long y);
void cursor(ami_long x, ami_long y);
ami_long  maxx(FILE* f);
ami_long  maxx(void);
ami_long  maxy(FILE* f);
ami_long  maxy(void);
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
void blink(FILE* f, ami_long e);
void blink(ami_long e);
void reverse(FILE* f, ami_long e);
void reverse(ami_long e);
void underline(FILE* f, ami_long e);
void underline(ami_long e);
void superscript(FILE* f, ami_long e);
void superscript(ami_long e);
void subscript(FILE* f, ami_long e);
void subscript(ami_long e);
void italic(FILE* f, ami_long e);
void italic(ami_long e);
void bold(FILE* f, ami_long e);
void bold(ami_long e);
void strikeout(FILE* f, ami_long e);
void strikeout(ami_long e);
void standout(FILE* f, ami_long e);
void standout(ami_long e);
void fcolor(FILE* f, color c);
void fcolor(color c);
void bcolor(FILE* f, color c);
void bcolor(color c);
void autom(FILE* f, ami_long e);
void autom(ami_long e);
void curvis(FILE* f, ami_long e);
void curvis(ami_long e);
void scroll(FILE* f, ami_long x, ami_long y);
void scroll(ami_long x, ami_long y);
ami_long  curx(FILE* f);
ami_long  curx(void);
ami_long  cury(FILE* f);
ami_long  cury(void);
ami_long  curbnd(FILE* f);
ami_long  curbnd(void);
void select(FILE* f, ami_long u, ami_long d);
void select(ami_long u, ami_long d);
void event(FILE* f, evtrec* er);
void event(evtrec* er);
void timer(FILE* f, ami_long i, ami_long t, ami_long r);
void timer(ami_long i, ami_long t, ami_long r);
void killtimer(FILE* f, ami_long i);
void killtimer(ami_long i);
ami_long  mouse(FILE* f);
ami_long  mouse(void);
ami_long  mousebutton(FILE* f, ami_long m);
ami_long  mousebutton(ami_long m);
ami_long  joystick(FILE* f);
ami_long  joystick(void);
ami_long  joybutton(FILE* f, ami_long j);
ami_long  joybutton(ami_long j);
ami_long  joyaxis(FILE* f, ami_long j);
ami_long  joyaxis(ami_long j);
void settab(FILE* f, ami_long t);
void settab(ami_long t);
void restab(FILE* f, ami_long t);
void restab(ami_long t);
void clrtab(FILE* f);
void clrtab(void);
ami_long  funkey(FILE* f);
ami_long  funkey(void);
void frametimer(FILE* f, ami_long e);
void frametimer(ami_long e);
void autohold(ami_long e);
void wrtstr(FILE* f, char* s);
void wrtstr(char* s);
void wrtstrn(FILE* f, char* s, ami_long n);
void wrtstrn(char* s, ami_long n);
void sizbuf(FILE* f, ami_long x, ami_long y);
void sizbuf(ami_long x, ami_long y);
void title(FILE* f, char* ts);
void title(char* ts);
void eventover(evtcod e, pevthan eh, pevthan* oeh);
void eventsover(pevthan eh, pevthan* oeh);
void sendevent(FILE* f, evtrec* er);
void sendevent(evtrec* er);

/* graphical */
ami_long  maxxg(FILE* f);
ami_long  maxxg(void);
ami_long  maxyg(FILE* f);
ami_long  maxyg(void);
ami_long  curxg(FILE* f);
ami_long  curxg(void);
ami_long  curyg(FILE* f);
ami_long  curyg(void);
void line(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void line(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void linewidth(FILE* f, ami_long w);
void linewidth(ami_long w);
void rect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void rect(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void frect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void frect(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void rrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void rrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void frrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void frrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void ellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void ellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void fellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void fellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void arc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void arc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void farc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void farc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void fchord(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void fchord(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void ftriangle(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3);
void ftriangle(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3);
void cursorg(FILE* f, ami_long x, ami_long y);
void cursorg(ami_long x, ami_long y);
ami_long  baseline(FILE* f);
ami_long  baseline(void);
void setpixel(FILE* f, ami_long x, ami_long y);
void setpixel(ami_long x, ami_long y);
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
ami_long  chrsizx(FILE* f);
ami_long  chrsizx(void);
ami_long  chrsizy(FILE* f);
ami_long  chrsizy(void);
ami_long  fonts(FILE* f);
ami_long  fonts(void);
void font(FILE* f, ami_long fc);
void font(ami_long fc);
void fontnam(FILE* f, ami_long fc, char* fns, ami_long fnsl);
void fontnam(ami_long fc, char* fns, ami_long fnsl);
void fontsiz(FILE* f, ami_long s);
void fontsiz(ami_long s);
void chrspcy(FILE* f, ami_long s);
void chrspcy(ami_long s);
void chrspcx(FILE* f, ami_long s);
void chrspcx(ami_long s);
ami_long  dpmx(FILE* f);
ami_long  dpmx(void);
ami_long  dpmy(FILE* f);
ami_long  dpmy(void);
ami_long  strsiz(FILE* f, const char* s);
ami_long  strsiz(const char* s);
ami_long  chrpos(FILE* f, const char* s, ami_long p);
ami_long  chrpos(const char* s, ami_long p);
void writejust(FILE* f, const char* s, ami_long n);
void writejust(const char* s, ami_long n);
ami_long  justpos(FILE* f, const char* s, ami_long p, ami_long n);
ami_long  justpos(const char* s, ami_long p, ami_long n);
void condensed(FILE* f, ami_long e);
void condensed(ami_long e);
void extended(FILE* f, ami_long e);
void extended(ami_long e);
void xlight(FILE* f, ami_long e);
void xlight(ami_long e);
void light(FILE* f, ami_long e);
void light(ami_long e);
void xbold(FILE* f, ami_long e);
void xbold(ami_long e);
void hollow(FILE* f, ami_long e);
void hollow(ami_long e);
void raised(FILE* f, ami_long e);
void raised(ami_long e);
void settabg(FILE* f, ami_long t);
void settabg(ami_long t);
void restabg(FILE* f, ami_long t);
void restabg(ami_long t);
void fcolorg(FILE* f, ami_long r, ami_long g, ami_long b);
void fcolorg(ami_long r, ami_long g, ami_long b);
void fcolorc(FILE* f, ami_long r, ami_long g, ami_long b);
void fcolorc(ami_long r, ami_long g, ami_long b);
void bcolorg(FILE* f, ami_long r, ami_long g, ami_long b);
void bcolorg(ami_long r, ami_long g, ami_long b);
void bcolorc(FILE* f, ami_long r, ami_long g, ami_long b);
void bcolorc(ami_long r, ami_long g, ami_long b);
void loadpict(FILE* f, ami_long p, char* fn);
void loadpict(ami_long p, char* fn);
ami_long  pictsizx(FILE* f, ami_long p);
ami_long  pictsizx(ami_long p);
ami_long  pictsizy(FILE* f, ami_long p);
ami_long  pictsizy(ami_long p);
void picture(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void picture(ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void delpict(FILE* f, ami_long p);
void delpict(ami_long p);
void scrollg(FILE* f, ami_long x, ami_long y);
void scrollg(ami_long x, ami_long y);
void blockcopyg(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2,
                ami_long sy2, ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2);
void blockcopyg(ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2, ami_long sy2,
                ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2);
void path(FILE* f, ami_long a);
void path(ami_long a);

/* window management */
void openwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid);
void buffer(FILE* f, ami_long e);
void buffer(ami_long e);
void sizbufg(FILE* f, ami_long x, ami_long y);
void sizbufg(ami_long x, ami_long y);
void getsiz(FILE* f, ami_long* x, ami_long* y);
void getsiz(ami_long* x, ami_long* y);
void getsizg(FILE* f, ami_long* x, ami_long* y);
void getsizg(ami_long* x, ami_long* y);
void setsiz(FILE* f, ami_long x, ami_long y);
void setsiz(ami_long x, ami_long y);
void setsizg(FILE* f, ami_long x, ami_long y);
void setsizg(ami_long x, ami_long y);
void setpos(FILE* f, ami_long x, ami_long y);
void setpos(ami_long x, ami_long y);
void setposg(FILE* f, ami_long x, ami_long y);
void setposg(ami_long x, ami_long y);
void scnsiz(FILE* f, ami_long* x, ami_long* y);
void scnsiz(ami_long* x, ami_long* y);
void scnsizg(FILE* f, ami_long* x, ami_long* y);
void scnsizg(ami_long* x, ami_long* y);
void scncen(FILE* f, ami_long* x, ami_long* y);
void scncen(ami_long* x, ami_long* y);
void scnceng(FILE* f, ami_long* x, ami_long* y);
void scnceng(ami_long* x, ami_long* y);
void winclient(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void winclient(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void winclientg(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void winclientg(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void front(FILE* f);
void front(void);
void back(FILE* f);
void back(void);
void frame(FILE* f, ami_long e);
void frame(ami_long e);
void sizable(FILE* f, ami_long e);
void sizable(ami_long e);
void sysbar(FILE* f, ami_long e);
void sysbar(ami_long e);
void autobuf(FILE* f, ami_long e);
void autobuf(ami_long e);
void menu(FILE* f, menuptr m);
void menu(menuptr m);
void menuena(FILE* f, ami_long id, ami_long onoff);
void menuena(ami_long id, ami_long onoff);
void menusel(FILE* f, ami_long id, ami_long select);
void menusel(ami_long id, ami_long select);
void stdmenu(stdmenusel sms, menuptr* sm, menuptr pm);
ami_long  getwinid(void);
void focus(FILE* f);
void focus(void);

/* widgets/controls */
ami_long  getwigid(FILE* f);
ami_long  getwigid(void);
void killwidget(FILE* f, ami_long id);
void killwidget(ami_long id);
void selectwidget(FILE* f, ami_long id, ami_long e);
void selectwidget(ami_long id, ami_long e);
void enablewidget(FILE* f, ami_long id, ami_long e);
void enablewidget(ami_long id, ami_long e);
void getwidgettext(FILE* f, ami_long id, char* s, ami_long sl);
void getwidgettext(ami_long id, char* s, ami_long sl);
void putwidgettext(FILE* f, ami_long id, char* s);
void putwidgettext(ami_long id, char* s);
void sizwidget(FILE* f, ami_long id, ami_long x, ami_long y);
void sizwidget(ami_long id, ami_long x, ami_long y);
void sizwidgetg(FILE* f, ami_long id, ami_long x, ami_long y);
void sizwidgetg(ami_long id, ami_long x, ami_long y);
void poswidget(FILE* f, ami_long id, ami_long x, ami_long y);
void poswidget(ami_long id, ami_long x, ami_long y);
void poswidgetg(FILE* f, ami_long id, ami_long x, ami_long y);
void poswidgetg(ami_long id, ami_long x, ami_long y);
void backwidget(FILE* f, ami_long id);
void backwidget(ami_long id);
void frontwidget(FILE* f, ami_long id);
void frontwidget(ami_long id);
void focuswidget(FILE* f, ami_long id);
void focuswidget(ami_long id);
void buttonsiz(FILE* f, char* s, ami_long* w, ami_long* h);
void buttonsiz(char* s, ami_long* w, ami_long* h);
void buttonsizg(FILE* f, char* s, ami_long* w, ami_long* h);
void buttonsizg(char* s, ami_long* w, ami_long* h);
void checkboxsiz(FILE* f, char* s, ami_long* w, ami_long* h);
void checkboxsiz(char* s, ami_long* w, ami_long* h);
void checkboxsizg(FILE* f, char* s, ami_long* w, ami_long* h);
void checkboxsizg(char* s, ami_long* w, ami_long* h);
void radiobuttonsiz(FILE* f, char* s, ami_long* w, ami_long* h);
void radiobuttonsiz(char* s, ami_long* w, ami_long* h);
void radiobuttonsizg(FILE* f, char* s, ami_long* w, ami_long* h);
void radiobuttonsizg(char* s, ami_long* w, ami_long* h);
void groupsiz(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void groupsiz(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void groupsizg(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void groupsizg(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void scrollvertsiz(FILE* f, ami_long* w, ami_long* h);
void scrollvertsiz(ami_long* w, ami_long* h);
void scrollvertsizg(FILE* f, ami_long* w, ami_long* h);
void scrollvertsizg(ami_long* w, ami_long* h);
void scrollhorizsiz(FILE* f, ami_long* w, ami_long* h);
void scrollhorizsiz(ami_long* w, ami_long* h);
void scrollhorizsizg(FILE* f, ami_long* w, ami_long* h);
void scrollhorizsizg(ami_long* w, ami_long* h);
void scrollpos(FILE* f, ami_long id, ami_long r);
void scrollpos(ami_long id, ami_long r);
void scrollsiz(FILE* f, ami_long id, ami_long r);
void scrollsiz(ami_long id, ami_long r);
void numselboxsiz(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h);
void numselboxsiz(ami_long l, ami_long u, ami_long* w, ami_long* h);
void numselboxsizg(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h);
void numselboxsizg(ami_long l, ami_long u, ami_long* w, ami_long* h);
void editboxsiz(FILE* f, char* s, ami_long* w, ami_long* h);
void editboxsiz(char* s, ami_long* w, ami_long* h);
void editboxsizg(FILE* f, char* s, ami_long* w, ami_long* h);
void editboxsizg(char* s, ami_long* w, ami_long* h);
void progbarsiz(FILE* f, ami_long* w, ami_long* h);
void progbarsiz(ami_long* w, ami_long* h);
void progbarsizg(FILE* f, ami_long* w, ami_long* h);
void progbarsizg(ami_long* w, ami_long* h);
void progbarpos(FILE* f, ami_long id, ami_long pos);
void progbarpos(ami_long id, ami_long pos);
void listboxsiz(FILE* f, strptr sp, ami_long* w, ami_long* h);
void listboxsiz(strptr sp, ami_long* w, ami_long* h);
void listboxsizg(FILE* f, strptr sp, ami_long* w, ami_long* h);
void listboxsizg(strptr sp, ami_long* w, ami_long* h);
void dropboxsiz(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropboxsizg(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsiz(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsizg(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void slidehorizsiz(FILE* f, ami_long* w, ami_long* h);
void slidehorizsiz(ami_long* w, ami_long* h);
void slidehorizsizg(FILE* f, ami_long* w, ami_long* h);
void slidehorizsizg(ami_long* w, ami_long* h);
void slidevertsiz(FILE* f, ami_long* w, ami_long* h);
void slidevertsiz(ami_long* w, ami_long* h);
void slidevertsizg(FILE* f, ami_long* w, ami_long* h);
void slidevertsizg(ami_long* w, ami_long* h);
void tabbarsiz(FILE* f, strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarsiz(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarsizg(FILE* f, strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarsizg(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarclient(FILE* f, tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabbarclient(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabbarclientg(FILE* f, tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabbarclientg(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabsel(FILE* f, ami_long id, ami_long tn);
void tabsel(ami_long id, ami_long tn);

/* dialogs */
void alert(char* title, char* message);
void querycolor(ami_long* r, ami_long* g, ami_long* b);
void queryopen(char* s, ami_long sl);
void querysave(char* s, ami_long sl);
void queryfind(char* s, ami_long sl, qfnopts* opt);
void queryfindrep(char* s, ami_long sl, char* r, ami_long rl, qfropts* opt);
void queryfont(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
               ami_long* bg, ami_long* bb, qfteffects* effect);
void queryfont(ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
               ami_long* bg, ami_long* bb, qfteffects* effect);

/* object based interface */
class graph {

FILE* infile;
FILE* outfile;

public:

/* constructor */
graph();

/* destructor */
~graph();

/* copying is refused: two objects would free one event hook */
graph(const graph&) = delete;
graph& operator=(const graph&) = delete;

/* methods */

/* text */
void cursor(ami_long x, ami_long y);
ami_long  maxx(void);
ami_long  maxy(void);
void home(void);
void del(void);
void up(void);
void down(void);
void left(void);
void right(void);
void blink(ami_long e);
void reverse(ami_long e);
void underline(ami_long e);
void superscript(ami_long e);
void subscript(ami_long e);
void italic(ami_long e);
void bold(ami_long e);
void strikeout(ami_long e);
void standout(ami_long e);
void fcolor(color c);
void bcolor(color c);
void autom(ami_long e);
void curvis(ami_long e);
void scroll(ami_long x, ami_long y);
ami_long  curx(void);
ami_long  cury(void);
ami_long  curbnd(void);
void select(ami_long u, ami_long d);
void event(evtrec* er);
void timer(ami_long i, ami_long t, ami_long r);
void killtimer(ami_long i);
ami_long  mouse(void);
ami_long  mousebutton(ami_long m);
ami_long  joystick(void);
ami_long  joybutton(ami_long j);
ami_long  joyaxis(ami_long j);
void settab(ami_long t);
void restab(ami_long t);
void clrtab(void);
ami_long  funkey(void);
void frametimer(ami_long e);
void autohold(ami_long e);
void wrtstr(char* s);
void wrtstrn(char* s, ami_long n);
void sizbuf(ami_long x, ami_long y);
void title(char* ts);
void sendevent(evtrec* er);

/* graphical */
ami_long  maxxg(void);
ami_long  maxyg(void);
ami_long  curxg(void);
ami_long  curyg(void);
void line(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void linewidth(ami_long w);
void rect(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void frect(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void rrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void frrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void ellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void fellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void arc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void farc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void fchord(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void ftriangle(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3);
void cursorg(ami_long x, ami_long y);
ami_long  baseline(void);
void setpixel(ami_long x, ami_long y);
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
ami_long  chrsizx(void);
ami_long  chrsizy(void);
ami_long  fonts(void);
void font(ami_long fc);
void fontnam(ami_long fc, char* fns, ami_long fnsl);
void fontsiz(ami_long s);
void chrspcy(ami_long s);
void chrspcx(ami_long s);
ami_long  dpmx(void);
ami_long  dpmy(void);
ami_long  strsiz(const char* s);
ami_long  chrpos(const char* s, ami_long p);
void writejust(const char* s, ami_long n);
ami_long  justpos(const char* s, ami_long p, ami_long n);
void condensed(ami_long e);
void extended(ami_long e);
void xlight(ami_long e);
void light(ami_long e);
void xbold(ami_long e);
void hollow(ami_long e);
void raised(ami_long e);
void settabg(ami_long t);
void restabg(ami_long t);
void fcolorg(ami_long r, ami_long g, ami_long b);
void fcolorc(ami_long r, ami_long g, ami_long b);
void bcolorg(ami_long r, ami_long g, ami_long b);
void bcolorc(ami_long r, ami_long g, ami_long b);
void loadpict(ami_long p, char* fn);
ami_long  pictsizx(ami_long p);
ami_long  pictsizy(ami_long p);
void picture(ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void delpict(ami_long p);
void scrollg(ami_long x, ami_long y);
void blockcopyg(ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2, ami_long sy2, ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2);
void path(ami_long a);

/* window management */
void buffer(ami_long e);
void sizbufg(ami_long x, ami_long y);
void getsiz(ami_long* x, ami_long* y);
void getsizg(ami_long* x, ami_long* y);
void setsiz(ami_long x, ami_long y);
void setsizg(ami_long x, ami_long y);
void setpos(ami_long x, ami_long y);
void setposg(ami_long x, ami_long y);
void scnsiz(ami_long* x, ami_long* y);
void scnsizg(ami_long* x, ami_long* y);
void scncen(ami_long* x, ami_long* y);
void scnceng(ami_long* x, ami_long* y);
void winclient(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void winclientg(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void front(void);
void back(void);
void frame(ami_long e);
void sizable(ami_long e);
void sysbar(ami_long e);
void autobuf(ami_long e);
void menu(menuptr m);
void menuena(ami_long id, ami_long onoff);
void menusel(ami_long id, ami_long select);
void focus(void);

/* widgets */
ami_long  getwigid(void);
void killwidget(ami_long id);
void selectwidget(ami_long id, ami_long e);
void enablewidget(ami_long id, ami_long e);
void getwidgettext(ami_long id, char* s, ami_long sl);
void putwidgettext(ami_long id, char* s);
void sizwidget(ami_long id, ami_long x, ami_long y);
void sizwidgetg(ami_long id, ami_long x, ami_long y);
void poswidget(ami_long id, ami_long x, ami_long y);
void poswidgetg(ami_long id, ami_long x, ami_long y);
void backwidget(ami_long id);
void frontwidget(ami_long id);
void focuswidget(ami_long id);
void buttonsiz(char* s, ami_long* w, ami_long* h);
void buttonsizg(char* s, ami_long* w, ami_long* h);
void checkboxsiz(char* s, ami_long* w, ami_long* h);
void checkboxsizg(char* s, ami_long* w, ami_long* h);
void radiobuttonsiz(char* s, ami_long* w, ami_long* h);
void radiobuttonsizg(char* s, ami_long* w, ami_long* h);
void groupsiz(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void groupsizg(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void scrollvertsiz(ami_long* w, ami_long* h);
void scrollvertsizg(ami_long* w, ami_long* h);
void scrollhorizsiz(ami_long* w, ami_long* h);
void scrollhorizsizg(ami_long* w, ami_long* h);
void scrollpos(ami_long id, ami_long r);
void scrollsiz(ami_long id, ami_long r);
void numselboxsiz(ami_long l, ami_long u, ami_long* w, ami_long* h);
void numselboxsizg(ami_long l, ami_long u, ami_long* w, ami_long* h);
void editboxsiz(char* s, ami_long* w, ami_long* h);
void editboxsizg(char* s, ami_long* w, ami_long* h);
void progbarsiz(ami_long* w, ami_long* h);
void progbarsizg(ami_long* w, ami_long* h);
void progbarpos(ami_long id, ami_long pos);
void listboxsiz(strptr sp, ami_long* w, ami_long* h);
void listboxsizg(strptr sp, ami_long* w, ami_long* h);
void dropboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void slidehorizsiz(ami_long* w, ami_long* h);
void slidehorizsizg(ami_long* w, ami_long* h);
void slidevertsiz(ami_long* w, ami_long* h);
void slidevertsizg(ami_long* w, ami_long* h);
void tabbarsiz(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarsizg(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarclient(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabbarclientg(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabsel(ami_long id, ami_long tn);

/* dialogs */
void queryfont(ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
               ami_long* bg, ami_long* bb, qfteffects* effect);

void button(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void buttong(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void checkbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void checkboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void radiobutton(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void radiobuttong(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void group(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void groupg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id);
void background(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void backgroundg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void scrollvert(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void scrollvertg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void scrollhoriz(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void scrollhorizg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void numselbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id);
void numselboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id);
void editbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void editboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void progbar(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void progbarg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id);
void listbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id);
void listboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id);
void dropbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id);
void dropboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id);
void dropeditbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id);
void dropeditboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id);
void slidehoriz(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id);
void slidehorizg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id);
void slidevert(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id);
void slidevertg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id);
void tabbar(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, tabori tor, ami_long id);
void tabbarg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, tabori tor, ami_long id);

static void graphCB(evtrec* er);

/* virtual callbacks */
virtual ami_long evchar(char c);
virtual ami_long evup(void);
virtual ami_long evdown(void);
virtual ami_long evleft(void);
virtual ami_long evright(void);
virtual ami_long evleftw(void);
virtual ami_long evrightw(void);
virtual ami_long evhome(void);
virtual ami_long evhomes(void);
virtual ami_long evhomel(void);
virtual ami_long evend(void);
virtual ami_long evends(void);
virtual ami_long evendl(void);
virtual ami_long evscrl(void);
virtual ami_long evscrr(void);
virtual ami_long evscru(void);
virtual ami_long evscrd(void);
virtual ami_long evpagd(void);
virtual ami_long evpagu(void);
virtual ami_long evtab(void);
virtual ami_long eventer(void);
virtual ami_long evinsert(void);
virtual ami_long evinsertl(void);
virtual ami_long evinsertt(void);
virtual ami_long evdel(void);
virtual ami_long evdell(void);
virtual ami_long evdelcf(void);
virtual ami_long evdelcb(void);
virtual ami_long evcopy(void);
virtual ami_long evcopyl(void);
virtual ami_long evcan(void);
virtual ami_long evstop(void);
virtual ami_long evcont(void);
virtual ami_long evprint(void);
virtual ami_long evprintb(void);
virtual ami_long evprints(void);
virtual ami_long evfun(ami_long k);
virtual ami_long evmenu(void);
virtual ami_long evmouba(ami_long m, ami_long b);
virtual ami_long evmoubd(ami_long m, ami_long b);
virtual ami_long evmoumov(ami_long m, ami_long x, ami_long y);
virtual ami_long evtim(ami_long t);
virtual ami_long evjoyba(ami_long j, ami_long b);
virtual ami_long evjoybd(ami_long j, ami_long b);
virtual ami_long evjoymov(ami_long j, ami_long x, ami_long y, ami_long z);
virtual ami_long evresize(void);
virtual ami_long evfocus(void);
virtual ami_long evnofocus(void);
virtual ami_long evhover(void);
virtual ami_long evnohover(void);
virtual ami_long evterm(void);
virtual ami_long evframe(void);
virtual ami_long evmoumovg(ami_long m, ami_long x, ami_long y);
virtual ami_long evredraw(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
virtual ami_long evmin(void);
virtual ami_long evmax(void);
virtual ami_long evnorm(void);
virtual ami_long evmenus(ami_long id);
virtual ami_long evbutton(ami_long id);
virtual ami_long evchkbox(ami_long id);
virtual ami_long evradbut(ami_long id);
virtual ami_long evsclull(ami_long id);
virtual ami_long evscldrl(ami_long id);
virtual ami_long evsclulp(ami_long id);
virtual ami_long evscldrp(ami_long id);
virtual ami_long evsclpos(ami_long id, ami_long pos);
virtual ami_long evedtbox(ami_long id);
virtual ami_long evnumbox(ami_long id, ami_long val);
virtual ami_long evlstbox(ami_long id, ami_long sel);
virtual ami_long evdrpbox(ami_long id, ami_long sel);
virtual ami_long evdrebox(ami_long id);
virtual ami_long evsldpos(ami_long id, ami_long pos);
virtual ami_long evtabbar(ami_long id, ami_long sel);
virtual ami_long evusize(void);
virtual ami_long evdsize(void);

}; /* class graph */


/*******************************************************************************

The window and widget objects

A window object is one window: the main one, a child of another window,
or an independent top level. Every drawing and configuration call is a
method, the object converts to FILE* so the stdio calls and the C
interface speak to it directly, and the event virtuals below fire for
this window's events alone: the wrapper routes each event by the window
id in its record to the object that holds that window. Any number of
window objects may exist, and events for windows with no object fall
through to the procedural loop untouched.

A widget is made on a window by constructing one of the typed widget
classes; the id is allocated for you unless given. Widget events are
routed to the widget object first -- a button's pressed(), a slider's
moved() -- then, unhandled, to the window's virtual with the id, then
to the chain. The destructor kills the widget.

The widget creators do not appear as free functions of the namespace,
since C++ will not let a class and a function share a name: the classes
are the creators. The C interface keeps them all.

One rule of order: make a widget's window before the widget, and let
the widget die before its window -- which member order in a subclassed
window does by itself, members being destroyed before their base. A
window destroyed first disarms the widgets it still carries, so their
destructors do not reach into a closed window.

*******************************************************************************/

class widget;
void windowCB(evtrec* er); /* the wrapper's chain hook */

class window {

friend class widget;
friend void windowCB(evtrec* er);

protected:

FILE* wf;     /* the window, which is what every C call takes */
ami_long  wid;    /* its logical id, which events carry */
ami_long  owned;  /* it was opened here, and closes here */
ami_long  nextid; /* widget ids not yet given out */
widget* wlist; /* the widgets made on this window */

public:

/* constructor: attach to the main window */
window();

/* constructor: open a window, a child of the given parent, or an
   independent top level when the parent is NULL */
window(window* parent);

/* destructor */
virtual ~window();

/* copying is refused: two objects would free one window */
window(const window&) = delete;
window& operator=(const window&) = delete;

/* the window, for the stdio calls and the C interface */
operator FILE*(void);

/* the logical window id, as events carry it */
ami_long id(void);

/* the next widget id not yet given out */
ami_long newid(void);

/* methods */
void cursor(ami_long x, ami_long y);
ami_long  maxx(void);
ami_long  maxy(void);
void home(void);
void del(void);
void up(void);
void down(void);
void left(void);
void right(void);
void blink(ami_long e);
void reverse(ami_long e);
void underline(ami_long e);
void superscript(ami_long e);
void subscript(ami_long e);
void italic(ami_long e);
void bold(ami_long e);
void strikeout(ami_long e);
void standout(ami_long e);
void fcolor(color c);
void bcolor(color c);
void autom(ami_long e);
void curvis(ami_long e);
void scroll(ami_long x, ami_long y);
ami_long  curx(void);
ami_long  cury(void);
ami_long  curbnd(void);
void select(ami_long u, ami_long d);
void timer(ami_long i, ami_long t, ami_long r);
void killtimer(ami_long i);
ami_long  mouse(void);
ami_long  mousebutton(ami_long m);
ami_long  joystick(void);
ami_long  joybutton(ami_long j);
ami_long  joyaxis(ami_long j);
void settab(ami_long t);
void restab(ami_long t);
void clrtab(void);
ami_long  funkey(void);
void frametimer(ami_long e);
void autohold(ami_long e);
void wrtstr(char* s);
void wrtstrn(char* s, ami_long n);
void sizbuf(ami_long x, ami_long y);
void title(char* ts);
void sendevent(evtrec* er);
ami_long  maxxg(void);
ami_long  maxyg(void);
ami_long  curxg(void);
ami_long  curyg(void);
void line(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void linewidth(ami_long w);
void rect(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void frect(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void rrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void frrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys);
void ellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void fellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void arc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void farc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void fchord(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea);
void ftriangle(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3);
void cursorg(ami_long x, ami_long y);
ami_long  baseline(void);
void setpixel(ami_long x, ami_long y);
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
ami_long  chrsizx(void);
ami_long  chrsizy(void);
ami_long  fonts(void);
void font(ami_long fc);
void fontnam(ami_long fc, char* fns, ami_long fnsl);
void fontsiz(ami_long s);
void chrspcy(ami_long s);
void chrspcx(ami_long s);
ami_long  dpmx(void);
ami_long  dpmy(void);
ami_long  strsiz(const char* s);
ami_long  chrpos(const char* s, ami_long p);
void writejust(const char* s, ami_long n);
ami_long  justpos(const char* s, ami_long p, ami_long n);
void condensed(ami_long e);
void extended(ami_long e);
void xlight(ami_long e);
void light(ami_long e);
void xbold(ami_long e);
void hollow(ami_long e);
void raised(ami_long e);
void settabg(ami_long t);
void restabg(ami_long t);
void fcolorg(ami_long r, ami_long g, ami_long b);
void fcolorc(ami_long r, ami_long g, ami_long b);
void bcolorg(ami_long r, ami_long g, ami_long b);
void bcolorc(ami_long r, ami_long g, ami_long b);
void loadpict(ami_long p, char* fn);
ami_long  pictsizx(ami_long p);
ami_long  pictsizy(ami_long p);
void picture(ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2);
void delpict(ami_long p);
void scrollg(ami_long x, ami_long y);
void blockcopyg(ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2, ami_long sy2, ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2);
void path(ami_long a);
void buffer(ami_long e);
void sizbufg(ami_long x, ami_long y);
void getsiz(ami_long* x, ami_long* y);
void getsizg(ami_long* x, ami_long* y);
void setsiz(ami_long x, ami_long y);
void setsizg(ami_long x, ami_long y);
void setpos(ami_long x, ami_long y);
void setposg(ami_long x, ami_long y);
void scnsiz(ami_long* x, ami_long* y);
void scnsizg(ami_long* x, ami_long* y);
void scncen(ami_long* x, ami_long* y);
void scnceng(ami_long* x, ami_long* y);
void winclient(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void winclientg(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms);
void front(void);
void back(void);
void frame(ami_long e);
void sizable(ami_long e);
void sysbar(ami_long e);
void autobuf(ami_long e);
void menu(menuptr m);
void menuena(ami_long id, ami_long onoff);
void menusel(ami_long id, ami_long select);
void focus(void);
ami_long  getwigid(void);
void killwidget(ami_long id);
void selectwidget(ami_long id, ami_long e);
void enablewidget(ami_long id, ami_long e);
void getwidgettext(ami_long id, char* s, ami_long sl);
void putwidgettext(ami_long id, char* s);
void sizwidget(ami_long id, ami_long x, ami_long y);
void sizwidgetg(ami_long id, ami_long x, ami_long y);
void poswidget(ami_long id, ami_long x, ami_long y);
void poswidgetg(ami_long id, ami_long x, ami_long y);
void backwidget(ami_long id);
void frontwidget(ami_long id);
void focuswidget(ami_long id);
void buttonsiz(char* s, ami_long* w, ami_long* h);
void buttonsizg(char* s, ami_long* w, ami_long* h);
void checkboxsiz(char* s, ami_long* w, ami_long* h);
void checkboxsizg(char* s, ami_long* w, ami_long* h);
void radiobuttonsiz(char* s, ami_long* w, ami_long* h);
void radiobuttonsizg(char* s, ami_long* w, ami_long* h);
void groupsiz(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void groupsizg(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void scrollvertsiz(ami_long* w, ami_long* h);
void scrollvertsizg(ami_long* w, ami_long* h);
void scrollhorizsiz(ami_long* w, ami_long* h);
void scrollhorizsizg(ami_long* w, ami_long* h);
void scrollpos(ami_long id, ami_long r);
void scrollsiz(ami_long id, ami_long r);
void numselboxsiz(ami_long l, ami_long u, ami_long* w, ami_long* h);
void numselboxsizg(ami_long l, ami_long u, ami_long* w, ami_long* h);
void editboxsiz(char* s, ami_long* w, ami_long* h);
void editboxsizg(char* s, ami_long* w, ami_long* h);
void progbarsiz(ami_long* w, ami_long* h);
void progbarsizg(ami_long* w, ami_long* h);
void progbarpos(ami_long id, ami_long pos);
void listboxsiz(strptr sp, ami_long* w, ami_long* h);
void listboxsizg(strptr sp, ami_long* w, ami_long* h);
void dropboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void dropeditboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh);
void slidehorizsiz(ami_long* w, ami_long* h);
void slidehorizsizg(ami_long* w, ami_long* h);
void slidevertsiz(ami_long* w, ami_long* h);
void slidevertsizg(ami_long* w, ami_long* h);
void tabbarsiz(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarsizg(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy);
void tabbarclient(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabbarclientg(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy);
void tabsel(ami_long id, ami_long tn);
void queryfont(ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
                      ami_long* bg, ami_long* bb, qfteffects* effect);

/* the event virtuals, fired for this window's events alone */
virtual ami_long evchar(char c);
virtual ami_long evup(void);
virtual ami_long evdown(void);
virtual ami_long evleft(void);
virtual ami_long evright(void);
virtual ami_long evleftw(void);
virtual ami_long evrightw(void);
virtual ami_long evhome(void);
virtual ami_long evhomes(void);
virtual ami_long evhomel(void);
virtual ami_long evend(void);
virtual ami_long evends(void);
virtual ami_long evendl(void);
virtual ami_long evscrl(void);
virtual ami_long evscrr(void);
virtual ami_long evscru(void);
virtual ami_long evscrd(void);
virtual ami_long evpagd(void);
virtual ami_long evpagu(void);
virtual ami_long evtab(void);
virtual ami_long eventer(void);
virtual ami_long evinsert(void);
virtual ami_long evinsertl(void);
virtual ami_long evinsertt(void);
virtual ami_long evdel(void);
virtual ami_long evdell(void);
virtual ami_long evdelcf(void);
virtual ami_long evdelcb(void);
virtual ami_long evcopy(void);
virtual ami_long evcopyl(void);
virtual ami_long evcan(void);
virtual ami_long evstop(void);
virtual ami_long evcont(void);
virtual ami_long evprint(void);
virtual ami_long evprintb(void);
virtual ami_long evprints(void);
virtual ami_long evfun(ami_long k);
virtual ami_long evmenu(void);
virtual ami_long evmouba(ami_long m, ami_long b);
virtual ami_long evmoubd(ami_long m, ami_long b);
virtual ami_long evmoumov(ami_long m, ami_long x, ami_long y);
virtual ami_long evtim(ami_long t);
virtual ami_long evjoyba(ami_long j, ami_long b);
virtual ami_long evjoybd(ami_long j, ami_long b);
virtual ami_long evjoymov(ami_long j, ami_long x, ami_long y, ami_long z);
virtual ami_long evresize(void);
virtual ami_long evfocus(void);
virtual ami_long evnofocus(void);
virtual ami_long evhover(void);
virtual ami_long evnohover(void);
virtual ami_long evterm(void);
virtual ami_long evframe(void);
virtual ami_long evmoumovg(ami_long m, ami_long x, ami_long y);
virtual ami_long evredraw(ami_long x1, ami_long y1, ami_long x2, ami_long y2);
virtual ami_long evmin(void);
virtual ami_long evmax(void);
virtual ami_long evnorm(void);
virtual ami_long evmenus(ami_long id);
virtual ami_long evbutton(ami_long id);
virtual ami_long evchkbox(ami_long id);
virtual ami_long evradbut(ami_long id);
virtual ami_long evsclull(ami_long id);
virtual ami_long evscldrl(ami_long id);
virtual ami_long evsclulp(ami_long id);
virtual ami_long evscldrp(ami_long id);
virtual ami_long evsclpos(ami_long id, ami_long pos);
virtual ami_long evedtbox(ami_long id);
virtual ami_long evnumbox(ami_long id, ami_long val);
virtual ami_long evlstbox(ami_long id, ami_long sel);
virtual ami_long evdrpbox(ami_long id, ami_long sel);
virtual ami_long evdrebox(ami_long id);
virtual ami_long evsldpos(ami_long id, ami_long pos);
virtual ami_long evtabbar(ami_long id, ami_long sel);
virtual ami_long evusize(void);
virtual ami_long evdsize(void);

}; /* class window */

class widget {

friend class window;
friend void windowCB(evtrec* er);

protected:

window& w;    /* the window the widget is on */
ami_long    wid;  /* the widget id within it */
ami_long    dead; /* the window went first: do not reach after it */
widget* next; /* next widget on the window */

/* the base registers; the typed class creates */
widget(window& wo, ami_long id);

public:

/* destructor: kills the widget */
virtual ~widget();

/* copying is refused: two objects would free one widget */
widget(const widget&) = delete;
widget& operator=(const widget&) = delete;

/* the widget id */
ami_long id(void);

/* operations, all from the C widget set */
void kill(void);
void select(ami_long e);
void enable(ami_long e);
void gettext(char* s, ami_long sl);
void puttext(const char* s);
void pos(ami_long x, ami_long y);
void siz(ami_long x, ami_long y);
void back(void);
void front(void);
void focus(void);

/* the event virtuals; a typed widget overrides what it answers */
virtual ami_long pressed(void);        /* button */
virtual ami_long clicked(void);        /* checkbox, radio button */
virtual ami_long done(void);           /* edit box, drop edit box */
virtual ami_long selected(ami_long v);     /* list, drop, number select, tab bar */
virtual ami_long moved(ami_long v);        /* slider, scroll bar position */
virtual ami_long upline(void);         /* scroll bar steps */
virtual ami_long downline(void);
virtual ami_long uppage(void);
virtual ami_long downpage(void);

}; /* class widget */

/* the typed widgets: constructing one makes it */
class button: public widget {

public:

button(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s,
       ami_long id = 0);

}; /* class button */

class checkbox: public widget {

public:

checkbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s,
         ami_long id = 0);

}; /* class checkbox */

class radiobutton: public widget {

public:

radiobutton(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s,
            ami_long id = 0);

}; /* class radiobutton */

class group: public widget {

public:

group(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s,
      ami_long id = 0);

}; /* class group */

class background: public widget {

public:

background(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id = 0);

}; /* class background */

class scrollvert: public widget {

public:

scrollvert(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id = 0);

}; /* class scrollvert */

class scrollhoriz: public widget {

public:

scrollhoriz(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id = 0);

}; /* class scrollhoriz */

class numselbox: public widget {

public:

numselbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u,
          ami_long id = 0);

}; /* class numselbox */

class editbox: public widget {

public:

editbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id = 0);

}; /* class editbox */

class progbar: public widget {

public:

progbar(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id = 0);

}; /* class progbar */

class listbox: public widget {

public:

listbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp,
        ami_long id = 0);

}; /* class listbox */

class dropbox: public widget {

public:

dropbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp,
        ami_long id = 0);

}; /* class dropbox */

class dropeditbox: public widget {

public:

dropeditbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp,
            ami_long id = 0);

}; /* class dropeditbox */

class slidehoriz: public widget {

public:

slidehoriz(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
           ami_long id = 0);

}; /* class slidehoriz */

class slidevert: public widget {

public:

slidevert(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
          ami_long id = 0);

}; /* class slidevert */

class tabbar: public widget {

public:

tabbar(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp,
       tabori tor, ami_long id = 0);

}; /* class tabbar */

} /* namespace graphics */

#endif /* __GRAPHICS_HPP__ */
