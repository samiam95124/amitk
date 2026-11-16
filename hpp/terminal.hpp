/** ****************************************************************************
 *
 * Terminal library interface C++ wrapper header
 *
 * Redeclares terminal library definitions using the terminal namespace.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

#ifndef __TERMINAL_HPP__
#define __TERMINAL_HPP__

#include <terminal.h>

namespace terminal {

#define MAXTIM AMI_MAXTIM /**< maximum number of timers available */

/* colors displayable in text mode */
typedef enum { black, white, red, green, blue, cyan,
               yellow, magenta } color;

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
    /** window redraw */                etredraw,
    /** window minimized */             etmin,
    /** window maximized */             etmax,
    /** window normalized */            etnorm,
    /** menu item selected */           etmenus,

    /* Reserved extra code areas, these are module defined. */
    etsys    = 0x1000, /* start of base system reserved codes */
    etman    = 0x2000, /* start of window management reserved codes */
    etwidget = 0x3000, /* start of widget reserved codes */
    etuser   = 0x4000  /* start of user defined codes */

} evtcod;

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
        /** etmouba */
        struct {

            /** mouse handle */  ami_long amoun;
            /** button number */ ami_long amoubn;

        };
        /** etmoubd */
        struct {

            /** mouse handle */  ami_long dmoun;
            /** button number */ ami_long dmoubn;

        };
        /** etjoyba */
        struct {

            /** joystick number */ ami_long ajoyn;
            /** button number */   ami_long ajoybn;

        };
        /** etjoybd */
        struct {

            /** joystick number */ ami_long djoyn;
            /** button number */   ami_long djoybn;

        };
        /** etjoymov */
        struct {

            /** joystick number */      ami_long mjoyn;
            /** joystick coordinates */ ami_long joypx, joypy, joypz;
                                        ami_long joyp4, joyp5, joyp6;

        };
        /** function key */ ami_long fkey;
        /** etresize */
        struct {

            ami_long rszx, rszy;

        };
        /** etredraw */
        struct {

            /** bounding rectangle, in characters */
            ami_long rsx, rsy, rex, rey;

        };
        /* etmenus */
        ami_long menuid; /* menu item selected */

     };

} evtrec, *evtptr;

/** event function pointer */
typedef void (*pevthan)(evtrec*);

/* procedural interface */
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
void select(FILE *f, ami_long u, ami_long d);
void select(ami_long u, ami_long d);
void event(FILE* f, evtrec* er);
void event(evtrec* er);
void timer(FILE* f, ami_long i, ami_long t, ami_long r);
void timer(ami_long i, ami_long t, ami_long r);
void killtimer(FILE* f, ami_long i);
void killtimer(ami_long i);
ami_long  mouse(FILE *f);
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
void wrtstr(FILE* f, char *s);
void wrtstr(char *s);
void wrtstr(FILE* f, char *s, ami_long n);
void wrtstr(char *s, ami_long n);
void wrtstrn(FILE* f, char* s, ami_long n);
void wrtstrn(char* s, ami_long n);
void sizbuf(FILE* f, ami_long x, ami_long y);
void sizbuf(ami_long x, ami_long y);
void eventover(evtcod e, pevthan eh, pevthan* oeh);
void eventsover(pevthan eh, pevthan* oeh);
void title(FILE* f, char* ts);
void title(char* ts);
void titlen(FILE* f, char* ts, ami_long n);
void titlen(char* ts, ami_long n);
void fcolorc(FILE* f, ami_long r, ami_long g, ami_long b);
void fcolorc(ami_long r, ami_long g, ami_long b);
void bcolorc(FILE* f, ami_long r, ami_long g, ami_long b);
void bcolorc(ami_long r, ami_long g, ami_long b);
void openprint(FILE** f, char* n);
void errorover(ami_errhan nfp, ami_errhan* ofp);

/* object based interface */
class term {

FILE*      infile;
FILE*      outfile;

public:

/* constructor */
term();

/* destructor */
~term();

/* copying is refused: two objects would free one event hook */
term(const term&) = delete;
term& operator=(const term&) = delete;

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
void wrtstr(char *s);
void wrtstr(char *s, ami_long n);
void wrtstrn(char *s, ami_long n);
void sizbuf(ami_long x, ami_long y);
void title(char* ts);
void titlen(char* ts, ami_long n);
void fcolorc(ami_long r, ami_long g, ami_long b);
void bcolorc(ami_long r, ami_long g, ami_long b);
static void termCB(evtrec* er);

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
virtual ami_long evresize(ami_long rszx, ami_long rszy);
virtual ami_long evfocus(void);
virtual ami_long evnofocus(void);
virtual ami_long evhover(void);
virtual ami_long evnohover(void);
virtual ami_long evterm(void);
virtual ami_long evframe(void);

}; /* class term */

} /* namespace terminal */

#endif /* __TERMINAL_HPP__ */
