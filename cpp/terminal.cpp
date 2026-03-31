/** ****************************************************************************
 *
 * Terminal library interface C++ wrapper
 *
 * Wraps the calls in terminal with C++ conventions. This brings several
 * advantages over C code:
 *
 * 1. The functions and other defintions do not need a "ami_" prefix, but rather
 * we let the namespace feature handle namespace isolation.
 *
 * 2. Parameters like what file handle controls the terminal can be defaulted.
 *
 * 3. A terminal object can be used instead of individual calls.
 *
 * 4. Instead of registering callbacks in C, the term object features virtual
 * functions for each event than can be individually overriden.
 *
 * Terminal has two distinct types of interfaces, the procedural and the object/
 * class interfaces. The procedural interface expects the specification of
 * what terminal surface we are talking to to be the first parameter of all
 * procedures and functions (even if defaulted to stdin or stdout). The object/
 * class interface keeps that as part of the object.
 *
 * Since the terminal, just as the graphics interface, only specifies the
 * default interface (usually specified by stdin/stdout), the object/class
 * interface does not get interesting until multiple screens/windows are used.
 * This is a consequence of the upward compatible model.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>

#include <terminal.h>

}

#include "terminal.hpp"

namespace terminal {

/* hook for sending events back to methods */
term* termoCB;
pevthan termoeh;

/* procedures and functions */
void cursor(FILE* f, int x, int y) { ami_cursor(f, x, y); }
void cursor(int x, int y) { ami_cursor(stdout, x, y); }
int  maxx(FILE* f) { return ami_maxx(f); }
int  maxx(void) { return ami_maxx(stdout); }
int  maxy(FILE* f) { return ami_maxy(f); }
int  maxy(void) { return ami_maxy(stdout); }
void home(FILE* f) { ami_home(f); }
void home(void) { ami_home(stdout); }
void del(FILE* f) { ami_del(f); }
void del(void) { ami_del(stdout); }
void up(FILE* f) { ami_up(f); }
void up(void) { ami_up(stdout); }
void down(FILE* f) { ami_down(f); }
void down(void) { ami_down(stdout); }
void left(FILE* f) { ami_left(f); }
void left(void) { ami_left(stdout); }
void right(FILE* f) { ami_right(f); }
void right(void) { ami_right(stdout); }
void blink(FILE* f, int e) { ami_blink(f, e); }
void blink(int e) { ami_blink(stdout, e); }
void reverse(FILE* f, int e) { ami_reverse(f, e); }
void reverse(int e) { ami_reverse(stdout, e); }
void underline(FILE* f, int e) { ami_underline(f, e); }
void underline(int e) { ami_underline(stdout, e); }
void superscript(FILE* f, int e) { ami_superscript(f, e); }
void superscript(int e) { ami_superscript(stdout, e); }
void subscript(FILE* f, int e) { ami_subscript(f, e); }
void subscript(int e) { ami_subscript(stdout, e); }
void italic(FILE* f, int e) { ami_italic(f, e); }
void italic(int e) { ami_italic(stdout, e); }
void bold(FILE* f, int e) { ami_bold(f, e); }
void bold(int e) { ami_bold(stdout, e); }
void strikeout(FILE* f, int e) { ami_strikeout(f, e); }
void strikeout(int e) { ami_strikeout(stdout, e); }
void standout(FILE* f, int e) { ami_standout(f, e); }
void standout(int e) { ami_standout(stdout, e); }
void fcolor(FILE* f, color c) { ami_fcolor(f, (ami_color)c); }
void fcolor(color c) { ami_fcolor(stdout, (ami_color)c); }
void bcolor(FILE* f, color c) { ami_bcolor(f, (ami_color)c); }
void bcolor(color c) { ami_bcolor(stdout, (ami_color)c); }
void autom(FILE* f, int e) { ami_auto(f, e); }
void autom(int e) { ami_auto(stdout, e); }
void curvis(FILE* f, int e) { ami_curvis(f, e); }
void curvis(int e) { ami_curvis(stdout, e); }
void scroll(FILE* f, int x, int y) { ami_scroll(f, x, y); }
void scroll(int x, int y) { ami_scroll(stdout, x, y); }
int  curx(FILE* f) { return ami_curx(f); }
int  curx(void) { return ami_curx(stdout); }
int  cury(FILE* f) { return ami_cury(f); }
int  cury(void) { return ami_cury(stdout); }
int  curbnd(FILE* f) { return ami_curbnd(f); }
int  curbnd(void) { return ami_curbnd(stdout); }
void select(FILE *f, int u, int d) { ami_select(f, u, d); }
void select(int u, int d) { ami_select(stdout, u, d); }
void event(FILE* f, evtrec* er) { ami_event(f, (ami_evtptr)er); }
void event(evtrec* er) { ami_event(stdin, (ami_evtptr)er); }
void timer(FILE* f, int i, int t, int r) { ami_timer(f, i, t, r); }
void timer(int i, int t, int r) { ami_timer(stdout, i, t, r); }
void killtimer(FILE* f, int i) { ami_killtimer(f, i); }
void killtimer(int i) { ami_killtimer(stdout, i); }
int  mouse(FILE *f) { return ami_mouse(f); }
int  mouse(void) { return ami_mouse(stdout); }
int  mousebutton(FILE* f, int m) { return ami_mousebutton(f, m); }
int  mousebutton(int m) { return ami_mousebutton(stdout, m); }
int  joystick(FILE* f) { return ami_joystick(f); }
int  joystick(void) { return ami_joystick(stdout); }
int  joybutton(FILE* f, int j) { return ami_joybutton(f, j); }
int  joybutton(int j) { return ami_joybutton(stdout, j); }
int  joyaxis(FILE* f, int j) { return ami_joyaxis(f, j); }
int  joyaxis(int j) { return ami_joyaxis(stdout, j); }
void settab(FILE* f, int t) { ami_settab(f, t); }
void settab(int t) { ami_settab(stdout, t); }
void restab(FILE* f, int t) { ami_restab(f, t); }
void restab(int t) { ami_restab(stdout, t); }
void clrtab(FILE* f) { ami_clrtab(f); }
void clrtab(void) { ami_clrtab(stdout); }
int  funkey(FILE* f) { return ami_funkey(f); }
int  funkey(void) { return ami_funkey(stdout); }
void frametimer(FILE* f, int e) { ami_frametimer(f, e); }
void frametimer(int e) { ami_frametimer(stdout, e); }
void autohold(int e) { ami_autohold(e); }
void wrtstr(FILE* f, char *s) { ami_wrtstr(f, s); }
void wrtstr(char *s) { ami_wrtstr(stdout, s); }
void wrtstr(FILE* f, char *s, int n) { ami_wrtstrn(f, s, n); }
void wrtstr(char *s, int n) { ami_wrtstrn(stdout, s, n); }
void wrtstrn(FILE* f, char *s, int n) { ami_wrtstrn(f, s, n); }
void wrtstrn(char *s, int n) { ami_wrtstrn(stdout, s, n); }
void sizbuf(FILE* f, int x, int y) { ami_sizbuf(f, x, y); }
void sizbuf(int x, int y) { ami_sizbuf(stdout, x, y); }
void eventover(evtcod e, pevthan eh, pevthan* oeh) { ami_eventover((ami_evtcod)e, (ami_pevthan)eh, (ami_pevthan*)oeh); }
void eventsover(pevthan eh, pevthan* oeh) { ami_eventsover((ami_pevthan)eh, (ami_pevthan*)oeh); }

/* methods */
term::term(void)

{

    infile = stdin;
    outfile = stdout;
    termoCB = this;
    eventsover(termCB, &termoeh);

}

void term::cursor(int x, int y) { ami_cursor(outfile, x, y); }
int  term::maxx(void) { return ami_maxx(outfile); }
int  term::maxy(void) { return ami_maxy(outfile); }
void term::home(void) { ami_home(outfile); }
void term::del(void) { ami_del(outfile); }
void term::up(void) { ami_up(outfile); }
void term::down(void) { ami_down(outfile); }
void term::left(void) { ami_left(outfile); }
void term::right(void) { ami_right(outfile); }
void term::blink(int e) { ami_blink(outfile, e); }
void term::reverse(int e) { ami_reverse(outfile, e); }
void term::underline(int e) { ami_underline(outfile, e); }
void term::superscript(int e) { ami_superscript(outfile, e); }
void term::subscript(int e) { ami_subscript(outfile, e); }
void term::italic(int e) { ami_italic(outfile, e); }
void term::bold(int e) { ami_bold(outfile, e); }
void term::strikeout(int e) { ami_strikeout(outfile, e); }
void term::standout(int e) { ami_standout(outfile, e); }
void term::fcolor(color c) { ami_fcolor(outfile, (ami_color)c); }
void term::bcolor(color c) { ami_bcolor(outfile, (ami_color)c); }
void term::autom(int e) { ami_auto(outfile, e); }
void term::curvis(int e) { ami_curvis(outfile, e); }
void term::scroll(int x, int y) { ami_scroll(outfile, x, y); }
int  term::curx(void) { return ami_curx(outfile); }
int  term::cury(void) { return ami_cury(outfile); }
int  term::curbnd(void) { return ami_curbnd(outfile); }
void term::select(int u, int d) { ami_select(outfile, u, d); }
void term::event(evtrec* er) { ami_event(infile, (ami_evtptr)er); }
void term::timer(int i, int t, int r) { ami_timer(outfile, i, t, r); }
void term::killtimer(int i) { ami_killtimer(outfile, i); }
int  term::mouse(void) { return ami_mouse(outfile); }
int  term::mousebutton(int m) { return ami_mousebutton(outfile, m); }
int  term::joystick(void) { return ami_joystick(outfile); }
int  term::joybutton(int j) { return ami_joybutton(outfile, j); }
int  term::joyaxis(int j) { return ami_joyaxis(outfile, j); }
void term::settab(int t) { ami_settab(outfile, t); }
void term::restab(int t) { ami_restab(outfile, t); }
void term::clrtab(void) { ami_clrtab(outfile); }
int  term::funkey(void) { return ami_funkey(outfile); }
void term::frametimer(int e) { ami_frametimer(outfile, e); }
void term::autohold(int e) { ami_autohold(e); }
void term::wrtstr(char *s) { ami_wrtstr(outfile, s); }
void term::wrtstr(char *s, int n) { ami_wrtstrn(outfile, s, n); }
void term::wrtstrn(char *s, int n) { ami_wrtstrn(outfile, s, n); }
void term::sizbuf(int x, int y) { ami_sizbuf(outfile, x, y); }

/* virtual callbacks */
int term::evchar(char c) { return 0; }
int term::evup(void) { return 0; }
int term::evdown(void) { return 0; }
int term::evleft(void) { return 0; }
int term::evright(void) { return 0; }
int term::evleftw(void) { return 0; }
int term::evrightw(void) { return 0; }
int term::evhome(void) { return 0; }
int term::evhomes(void) { return 0; }
int term::evhomel(void) { return 0; }
int term::evend(void) { return 0; }
int term::evends(void) { return 0; }
int term::evendl(void) { return 0; }
int term::evscrl(void) { return 0; }
int term::evscrr(void) { return 0; }
int term::evscru(void) { return 0; }
int term::evscrd(void) { return 0; }
int term::evpagd(void) { return 0; }
int term::evpagu(void) { return 0; }
int term::evtab(void) { return 0; }
int term::eventer(void) { return 0; }
int term::evinsert(void) { return 0; }
int term::evinsertl(void) { return 0; }
int term::evinsertt(void) { return 0; }
int term::evdel(void) { return 0; }
int term::evdell(void) { return 0; }
int term::evdelcf(void) { return 0; }
int term::evdelcb(void) { return 0; }
int term::evcopy(void) { return 0; }
int term::evcopyl(void) { return 0; }
int term::evcan(void) { return 0; }
int term::evstop(void) { return 0; }
int term::evcont(void) { return 0; }
int term::evprint(void) { return 0; }
int term::evprintb(void) { return 0; }
int term::evprints(void) { return 0; }
int term::evfun(int k) { return 0; }
int term::evmenu(void) { return 0; }
int term::evmouba(int m, int b) { return 0; }
int term::evmoubd(int m, int b) { return 0; }
int term::evmoumov(int m, int x, int y) { return 0; }
int term::evtim(int t) { return 0; }
int term::evjoyba(int j, int b) { return 0; }
int term::evjoybd(int j, int b) { return 0; }
int term::evjoymov(int j, int x, int y, int z) { return 0; }
int term::evresize(void) { return 0; }
int term::evfocus(void) { return 0; }
int term::evnofocus(void) { return 0; }
int term::evhover(void) { return 0; }
int term::evnohover(void) { return 0; }
int term::evterm(void) { return 0; }

void term::termCB(evtrec* er)

{

    int handled;

    switch (er->etype) {

        case etchar:    handled = termoCB->evchar(er->echar); break;
        case etup:      handled = termoCB->evup(); break;
        case etdown:    handled = termoCB->evdown(); break;
        case etleft:    handled = termoCB->evleft(); break;
        case etright:   handled = termoCB->evright(); break;
        case etleftw:   handled = termoCB->evleftw(); break;
        case etrightw:  handled = termoCB->evrightw(); break;
        case ethome:    handled = termoCB->evhome(); break;
        case ethomes:   handled = termoCB->evhomes(); break;
        case ethomel:   handled = termoCB->evhomel(); break;
        case etend:     handled = termoCB->evend(); break;
        case etends:    handled = termoCB->evends(); break;
        case etendl:    handled = termoCB->evendl(); break;
        case etscrl:    handled = termoCB->evscrl(); break;
        case etscrr:    handled = termoCB->evscrr(); break;
        case etscru:    handled = termoCB->evscru(); break;
        case etscrd:    handled = termoCB->evscrd(); break;
        case etpagd:    handled = termoCB->evpagd(); break;
        case etpagu:    handled = termoCB->evpagu(); break;
        case ettab:     handled = termoCB->evtab(); break;
        case etenter:   handled = termoCB->eventer(); break;
        case etinsert:  handled = termoCB->evinsert(); break;
        case etinsertl: handled = termoCB->evinsertl(); break;
        case etinsertt: handled = termoCB->evinsertt(); break;
        case etdel:     handled = termoCB->evdel(); break;
        case etdell:    handled = termoCB->evdell(); break;
        case etdelcf:   handled = termoCB->evdelcf(); break;
        case etdelcb:   handled = termoCB->evdelcb(); break;
        case etcopy:    handled = termoCB->evcopy(); break;
        case etcopyl:   handled = termoCB->evcopyl(); break;
        case etcan:     handled = termoCB->evcan(); break;
        case etstop:    handled = termoCB->evstop(); break;
        case etcont:    handled = termoCB->evcont(); break;
        case etprint:   handled = termoCB->evprint(); break;
        case etprintb:  handled = termoCB->evprintb(); break;
        case etprints:  handled = termoCB->evprints(); break;
        case etfun:     handled = termoCB->evfun(er->fkey); break;
        case etmenu:    handled = termoCB->evmenu(); break;
        case etmouba:   handled = termoCB->evmouba(er->amoun, er->amoubn);
            break;
        case etmoubd:   handled = termoCB->evmoubd(er->dmoun, er->dmoubn);
            break;
        case etmoumov:
            handled = termoCB->evmoumov(er->mmoun, er->moupx, er->moupy);
            break;
        case ettim:     handled = termoCB->evtim(er->timnum); break;
        case etjoyba:   handled = termoCB->evjoyba(er->ajoyn, er->ajoybn);
            break;
        case etjoybd:   handled = termoCB->evjoybd(er->djoyn, er->djoybn);
            break;
        case etjoymov:
            handled = termoCB->evjoymov(er->mjoyn, er->joypx, er->joypy,
                                        er->joypz);
            break;
        case etresize:  handled = termoCB->evresize(); break;
        case etfocus:   handled = termoCB->evfocus(); break;
        case etnofocus: handled = termoCB->evnofocus(); break;
        case ethover:   handled = termoCB->evhover(); break;
        case etnohover: handled = termoCB->evnohover(); break;
        case etterm:    handled = termoCB->evterm(); break;

    }

    if (!handled) termoeh(er);

}

} /* namespace terminal */
