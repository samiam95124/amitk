/** ****************************************************************************
 *
 * Graphics library interface C++ wrapper
 *
 * Wraps the calls in graphics with C++ conventions. This brings several
 * advantages over C code:
 *
 * 1. The functions and other defintions do not need a "ami_" prefix, but rather
 * we let the namespace feature handle namespace isolation.
 *
 * 2. Parameters like what file handle controls the output can be defaulted.
 *
 * 3. A graph object can be used instead of individual calls.
 *
 * 4. Instead of registering callbacks in C, the graph object features virtual
 * functions for each event than can be individually overriden.
 *
 * Graphics has two distinct types of interfaces, the procedural and the object/
 * class interfaces. The procedural interface expects the specification of
 * what output surface we are talking to to be the first parameter of all
 * procedures and functions (even if defaulted to stdin or stdout). The object/
 * class interface keeps that as part of the object.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>
#include <stdlib.h>

#include <graphics.h>

}

#include "graphics.hpp"

/* A C++ program in this model wants its standard streams in the model as
   well, so the stream binding (cpp/stdio.cpp) travels with this class: the
   reference pulls its member from the library. */
extern "C" int ami_iostream(void);
static int (*const keep_iostream)(void) __attribute__((used)) = ami_iostream;

namespace graphics {

/* hook for sending events back to methods */
graph* graphoCB;
pevthan graphoeh;

/* procedures and functions */

/* text */
void cursor(FILE* f, ami_long x, ami_long y) { ami_cursor(f, x, y); }
void cursor(ami_long x, ami_long y) { ami_cursor(stdout, x, y); }
ami_long  maxx(FILE* f) { return ami_maxx(f); }
ami_long  maxx(void) { return ami_maxx(stdout); }
ami_long  maxy(FILE* f) { return ami_maxy(f); }
ami_long  maxy(void) { return ami_maxy(stdout); }
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
void blink(FILE* f, ami_long e) { ami_blink(f, e); }
void blink(ami_long e) { ami_blink(stdout, e); }
void reverse(FILE* f, ami_long e) { ami_reverse(f, e); }
void reverse(ami_long e) { ami_reverse(stdout, e); }
void underline(FILE* f, ami_long e) { ami_underline(f, e); }
void underline(ami_long e) { ami_underline(stdout, e); }
void superscript(FILE* f, ami_long e) { ami_superscript(f, e); }
void superscript(ami_long e) { ami_superscript(stdout, e); }
void subscript(FILE* f, ami_long e) { ami_subscript(f, e); }
void subscript(ami_long e) { ami_subscript(stdout, e); }
void italic(FILE* f, ami_long e) { ami_italic(f, e); }
void italic(ami_long e) { ami_italic(stdout, e); }
void bold(FILE* f, ami_long e) { ami_bold(f, e); }
void bold(ami_long e) { ami_bold(stdout, e); }
void strikeout(FILE* f, ami_long e) { ami_strikeout(f, e); }
void strikeout(ami_long e) { ami_strikeout(stdout, e); }
void standout(FILE* f, ami_long e) { ami_standout(f, e); }
void standout(ami_long e) { ami_standout(stdout, e); }
void fcolor(FILE* f, color c) { ami_fcolor(f, (ami_color)c); }
void fcolor(color c) { ami_fcolor(stdout, (ami_color)c); }
void bcolor(FILE* f, color c) { ami_bcolor(f, (ami_color)c); }
void bcolor(color c) { ami_bcolor(stdout, (ami_color)c); }
void autom(FILE* f, ami_long e) { ami_auto(f, e); }
void autom(ami_long e) { ami_auto(stdout, e); }
void curvis(FILE* f, ami_long e) { ami_curvis(f, e); }
void curvis(ami_long e) { ami_curvis(stdout, e); }
void scroll(FILE* f, ami_long x, ami_long y) { ami_scroll(f, x, y); }
void scroll(ami_long x, ami_long y) { ami_scroll(stdout, x, y); }
ami_long  curx(FILE* f) { return ami_curx(f); }
ami_long  curx(void) { return ami_curx(stdout); }
ami_long  cury(FILE* f) { return ami_cury(f); }
ami_long  cury(void) { return ami_cury(stdout); }
ami_long  curbnd(FILE* f) { return ami_curbnd(f); }
ami_long  curbnd(void) { return ami_curbnd(stdout); }
void select(FILE* f, ami_long u, ami_long d) { ami_select(f, u, d); }
void select(ami_long u, ami_long d) { ami_select(stdout, u, d); }
void event(FILE* f, evtrec* er) { ami_event(f, (ami_evtptr)er); }
void event(evtrec* er) { ami_event(stdin, (ami_evtptr)er); }
void timer(FILE* f, ami_long i, ami_long t, ami_long r) { ami_timer(f, i, t, r); }
void timer(ami_long i, ami_long t, ami_long r) { ami_timer(stdout, i, t, r); }
void killtimer(FILE* f, ami_long i) { ami_killtimer(f, i); }
void killtimer(ami_long i) { ami_killtimer(stdout, i); }
ami_long  mouse(FILE* f) { return ami_mouse(f); }
ami_long  mouse(void) { return ami_mouse(stdout); }
ami_long  mousebutton(FILE* f, ami_long m) { return ami_mousebutton(f, m); }
ami_long  mousebutton(ami_long m) { return ami_mousebutton(stdout, m); }
ami_long  joystick(FILE* f) { return ami_joystick(f); }
ami_long  joystick(void) { return ami_joystick(stdout); }
ami_long  joybutton(FILE* f, ami_long j) { return ami_joybutton(f, j); }
ami_long  joybutton(ami_long j) { return ami_joybutton(stdout, j); }
ami_long  joyaxis(FILE* f, ami_long j) { return ami_joyaxis(f, j); }
ami_long  joyaxis(ami_long j) { return ami_joyaxis(stdout, j); }
void settab(FILE* f, ami_long t) { ami_settab(f, t); }
void settab(ami_long t) { ami_settab(stdout, t); }
void restab(FILE* f, ami_long t) { ami_restab(f, t); }
void restab(ami_long t) { ami_restab(stdout, t); }
void clrtab(FILE* f) { ami_clrtab(f); }
void clrtab(void) { ami_clrtab(stdout); }
ami_long  funkey(FILE* f) { return ami_funkey(f); }
ami_long  funkey(void) { return ami_funkey(stdout); }
void frametimer(FILE* f, ami_long e) { ami_frametimer(f, e); }
void frametimer(ami_long e) { ami_frametimer(stdout, e); }
void autohold(ami_long e) { ami_autohold(e); }
void wrtstr(FILE* f, char* s) { ami_wrtstr(f, s); }
void wrtstr(char* s) { ami_wrtstr(stdout, s); }
void wrtstrn(FILE* f, char* s, ami_long n) { ami_wrtstrn(f, s, n); }
void wrtstrn(char* s, ami_long n) { ami_wrtstrn(stdout, s, n); }
void sizbuf(FILE* f, ami_long x, ami_long y) { ami_sizbuf(f, x, y); }
void sizbuf(ami_long x, ami_long y) { ami_sizbuf(stdout, x, y); }
void title(FILE* f, char* ts) { ami_title(f, ts); }
void title(char* ts) { ami_title(stdout, ts); }
void eventover(evtcod e, pevthan eh, pevthan* oeh) { ami_eventover((ami_evtcod)e, (ami_pevthan)eh, (ami_pevthan*)oeh); }
void eventsover(pevthan eh, pevthan* oeh) { ami_eventsover((ami_pevthan)eh, (ami_pevthan*)oeh); }
void sendevent(FILE* f, evtrec* er) { ami_sendevent(f, (ami_evtptr)er); }
void sendevent(evtrec* er) { ami_sendevent(stdout, (ami_evtptr)er); }

/* graphical */
ami_long  maxxg(FILE* f) { return ami_maxxg(f); }
ami_long  maxxg(void) { return ami_maxxg(stdout); }
ami_long  maxyg(FILE* f) { return ami_maxyg(f); }
ami_long  maxyg(void) { return ami_maxyg(stdout); }
ami_long  curxg(FILE* f) { return ami_curxg(f); }
ami_long  curxg(void) { return ami_curxg(stdout); }
ami_long  curyg(FILE* f) { return ami_curyg(f); }
ami_long  curyg(void) { return ami_curyg(stdout); }
void line(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_line(f, x1, y1, x2, y2); }
void line(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_line(stdout, x1, y1, x2, y2); }
void linewidth(FILE* f, ami_long w) { ami_linewidth(f, w); }
void linewidth(ami_long w) { ami_linewidth(stdout, w); }
void rect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_rect(f, x1, y1, x2, y2); }
void rect(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_rect(stdout, x1, y1, x2, y2); }
void frect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_frect(f, x1, y1, x2, y2); }
void frect(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_frect(stdout, x1, y1, x2, y2); }
void rrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_rrect(f, x1, y1, x2, y2, xs, ys); }
void rrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_rrect(stdout, x1, y1, x2, y2, xs, ys); }
void frrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_frrect(f, x1, y1, x2, y2, xs, ys); }
void frrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_frrect(stdout, x1, y1, x2, y2, xs, ys); }
void ellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_ellipse(f, x1, y1, x2, y2); }
void ellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_ellipse(stdout, x1, y1, x2, y2); }
void fellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_fellipse(f, x1, y1, x2, y2); }
void fellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_fellipse(stdout, x1, y1, x2, y2); }
void arc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_arc(f, x1, y1, x2, y2, sa, ea); }
void arc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_arc(stdout, x1, y1, x2, y2, sa, ea); }
void farc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_farc(f, x1, y1, x2, y2, sa, ea); }
void farc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_farc(stdout, x1, y1, x2, y2, sa, ea); }
void fchord(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_fchord(f, x1, y1, x2, y2, sa, ea); }
void fchord(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_fchord(stdout, x1, y1, x2, y2, sa, ea); }
void ftriangle(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3) { ami_ftriangle(f, x1, y1, x2, y2, x3, y3); }
void ftriangle(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3) { ami_ftriangle(stdout, x1, y1, x2, y2, x3, y3); }
void cursorg(FILE* f, ami_long x, ami_long y) { ami_cursorg(f, x, y); }
void cursorg(ami_long x, ami_long y) { ami_cursorg(stdout, x, y); }
ami_long  baseline(FILE* f) { return ami_baseline(f); }
ami_long  baseline(void) { return ami_baseline(stdout); }
void setpixel(FILE* f, ami_long x, ami_long y) { ami_setpixel(f, x, y); }
void setpixel(ami_long x, ami_long y) { ami_setpixel(stdout, x, y); }
void fover(FILE* f) { ami_fover(f); }
void fover(void) { ami_fover(stdout); }
void bover(FILE* f) { ami_bover(f); }
void bover(void) { ami_bover(stdout); }
void finvis(FILE* f) { ami_finvis(f); }
void finvis(void) { ami_finvis(stdout); }
void binvis(FILE* f) { ami_binvis(f); }
void binvis(void) { ami_binvis(stdout); }
void fxor(FILE* f) { ami_fxor(f); }
void fxor(void) { ami_fxor(stdout); }
void bxor(FILE* f) { ami_bxor(f); }
void bxor(void) { ami_bxor(stdout); }
void fand(FILE* f) { ami_fand(f); }
void fand(void) { ami_fand(stdout); }
void band(FILE* f) { ami_band(f); }
void band(void) { ami_band(stdout); }
void for_(FILE* f) { ami_for(f); }
void for_(void) { ami_for(stdout); }
void bor(FILE* f) { ami_bor(f); }
void bor(void) { ami_bor(stdout); }
ami_long  chrsizx(FILE* f) { return ami_chrsizx(f); }
ami_long  chrsizx(void) { return ami_chrsizx(stdout); }
ami_long  chrsizy(FILE* f) { return ami_chrsizy(f); }
ami_long  chrsizy(void) { return ami_chrsizy(stdout); }
ami_long  fonts(FILE* f) { return ami_fonts(f); }
ami_long  fonts(void) { return ami_fonts(stdout); }
void font(FILE* f, ami_long fc) { ami_font(f, fc); }
void font(ami_long fc) { ami_font(stdout, fc); }
void fontnam(FILE* f, ami_long fc, char* fns, ami_long fnsl) { ami_fontnam(f, fc, fns, fnsl); }
void fontnam(ami_long fc, char* fns, ami_long fnsl) { ami_fontnam(stdout, fc, fns, fnsl); }
void fontsiz(FILE* f, ami_long s) { ami_fontsiz(f, s); }
void fontsiz(ami_long s) { ami_fontsiz(stdout, s); }
void chrspcy(FILE* f, ami_long s) { ami_chrspcy(f, s); }
void chrspcy(ami_long s) { ami_chrspcy(stdout, s); }
void chrspcx(FILE* f, ami_long s) { ami_chrspcx(f, s); }
void chrspcx(ami_long s) { ami_chrspcx(stdout, s); }
ami_long  dpmx(FILE* f) { return ami_dpmx(f); }
ami_long  dpmx(void) { return ami_dpmx(stdout); }
ami_long  dpmy(FILE* f) { return ami_dpmy(f); }
ami_long  dpmy(void) { return ami_dpmy(stdout); }
ami_long  strsiz(FILE* f, const char* s) { return ami_strsiz(f, s); }
ami_long  strsiz(const char* s) { return ami_strsiz(stdout, s); }
ami_long  chrpos(FILE* f, const char* s, ami_long p) { return ami_chrpos(f, s, p); }
ami_long  chrpos(const char* s, ami_long p) { return ami_chrpos(stdout, s, p); }
void writejust(FILE* f, const char* s, ami_long n) { ami_writejust(f, s, n); }
void writejust(const char* s, ami_long n) { ami_writejust(stdout, s, n); }
ami_long  justpos(FILE* f, const char* s, ami_long p, ami_long n) { return ami_justpos(f, s, p, n); }
ami_long  justpos(const char* s, ami_long p, ami_long n) { return ami_justpos(stdout, s, p, n); }
void condensed(FILE* f, ami_long e) { ami_condensed(f, e); }
void condensed(ami_long e) { ami_condensed(stdout, e); }
void extended(FILE* f, ami_long e) { ami_extended(f, e); }
void extended(ami_long e) { ami_extended(stdout, e); }
void xlight(FILE* f, ami_long e) { ami_xlight(f, e); }
void xlight(ami_long e) { ami_xlight(stdout, e); }
void light(FILE* f, ami_long e) { ami_light(f, e); }
void light(ami_long e) { ami_light(stdout, e); }
void xbold(FILE* f, ami_long e) { ami_xbold(f, e); }
void xbold(ami_long e) { ami_xbold(stdout, e); }
void hollow(FILE* f, ami_long e) { ami_hollow(f, e); }
void hollow(ami_long e) { ami_hollow(stdout, e); }
void raised(FILE* f, ami_long e) { ami_raised(f, e); }
void raised(ami_long e) { ami_raised(stdout, e); }
void settabg(FILE* f, ami_long t) { ami_settabg(f, t); }
void settabg(ami_long t) { ami_settabg(stdout, t); }
void restabg(FILE* f, ami_long t) { ami_restabg(f, t); }
void restabg(ami_long t) { ami_restabg(stdout, t); }
void fcolorg(FILE* f, ami_long r, ami_long g, ami_long b) { ami_fcolorg(f, r, g, b); }
void fcolorg(ami_long r, ami_long g, ami_long b) { ami_fcolorg(stdout, r, g, b); }
void fcolorc(FILE* f, ami_long r, ami_long g, ami_long b) { ami_fcolorc(f, r, g, b); }
void fcolorc(ami_long r, ami_long g, ami_long b) { ami_fcolorc(stdout, r, g, b); }
void bcolorg(FILE* f, ami_long r, ami_long g, ami_long b) { ami_bcolorg(f, r, g, b); }
void bcolorg(ami_long r, ami_long g, ami_long b) { ami_bcolorg(stdout, r, g, b); }
void bcolorc(FILE* f, ami_long r, ami_long g, ami_long b) { ami_bcolorc(f, r, g, b); }
void bcolorc(ami_long r, ami_long g, ami_long b) { ami_bcolorc(stdout, r, g, b); }
void loadpict(FILE* f, ami_long p, char* fn) { ami_loadpict(f, p, fn); }
void loadpict(ami_long p, char* fn) { ami_loadpict(stdout, p, fn); }
ami_long  pictsizx(FILE* f, ami_long p) { return ami_pictsizx(f, p); }
ami_long  pictsizx(ami_long p) { return ami_pictsizx(stdout, p); }
ami_long  pictsizy(FILE* f, ami_long p) { return ami_pictsizy(f, p); }
ami_long  pictsizy(ami_long p) { return ami_pictsizy(stdout, p); }
void picture(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_picture(f, p, x1, y1, x2, y2); }
void picture(ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_picture(stdout, p, x1, y1, x2, y2); }
void delpict(FILE* f, ami_long p) { ami_delpict(f, p); }
void delpict(ami_long p) { ami_delpict(stdout, p); }
void scrollg(FILE* f, ami_long x, ami_long y) { ami_scrollg(f, x, y); }
void scrollg(ami_long x, ami_long y) { ami_scrollg(stdout, x, y); }
void blockcopyg(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2,
                ami_long sy2, ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2)
    { ami_blockcopyg(f, s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2); }
void blockcopyg(ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2, ami_long sy2,
                ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2)
    { ami_blockcopyg(stdout, s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2); }
void path(FILE* f, ami_long a) { ami_path(f, a); }
void path(ami_long a) { ami_path(stdout, a); }

/* window management */
void openwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid) { ami_openwin(infile, outfile, parent, wid); }
void buffer(FILE* f, ami_long e) { ami_buffer(f, e); }
void buffer(ami_long e) { ami_buffer(stdout, e); }
void sizbufg(FILE* f, ami_long x, ami_long y) { ami_sizbufg(f, x, y); }
void sizbufg(ami_long x, ami_long y) { ami_sizbufg(stdout, x, y); }
void getsiz(FILE* f, ami_long* x, ami_long* y) { ami_getsiz(f, x, y); }
void getsiz(ami_long* x, ami_long* y) { ami_getsiz(stdout, x, y); }
void getsizg(FILE* f, ami_long* x, ami_long* y) { ami_getsizg(f, x, y); }
void getsizg(ami_long* x, ami_long* y) { ami_getsizg(stdout, x, y); }
void setsiz(FILE* f, ami_long x, ami_long y) { ami_setsiz(f, x, y); }
void setsiz(ami_long x, ami_long y) { ami_setsiz(stdout, x, y); }
void setsizg(FILE* f, ami_long x, ami_long y) { ami_setsizg(f, x, y); }
void setsizg(ami_long x, ami_long y) { ami_setsizg(stdout, x, y); }
void setpos(FILE* f, ami_long x, ami_long y) { ami_setpos(f, x, y); }
void setpos(ami_long x, ami_long y) { ami_setpos(stdout, x, y); }
void setposg(FILE* f, ami_long x, ami_long y) { ami_setposg(f, x, y); }
void setposg(ami_long x, ami_long y) { ami_setposg(stdout, x, y); }
void scnsiz(FILE* f, ami_long* x, ami_long* y) { ami_scnsiz(f, x, y); }
void scnsiz(ami_long* x, ami_long* y) { ami_scnsiz(stdout, x, y); }
void scnsizg(FILE* f, ami_long* x, ami_long* y) { ami_scnsizg(f, x, y); }
void scnsizg(ami_long* x, ami_long* y) { ami_scnsizg(stdout, x, y); }
void scncen(FILE* f, ami_long* x, ami_long* y) { ami_scncen(f, x, y); }
void scncen(ami_long* x, ami_long* y) { ami_scncen(stdout, x, y); }
void scnceng(FILE* f, ami_long* x, ami_long* y) { ami_scnceng(f, x, y); }
void scnceng(ami_long* x, ami_long* y) { ami_scnceng(stdout, x, y); }
void winclient(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclient(f, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclient(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclient(stdout, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclientg(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclientg(f, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclientg(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclientg(stdout, cx, cy, wx, wy, (ami_winmodset)ms); }
void front(FILE* f) { ami_front(f); }
void front(void) { ami_front(stdout); }
void back(FILE* f) { ami_back(f); }
void back(void) { ami_back(stdout); }
void frame(FILE* f, ami_long e) { ami_frame(f, e); }
void frame(ami_long e) { ami_frame(stdout, e); }
void sizable(FILE* f, ami_long e) { ami_sizable(f, e); }
void sizable(ami_long e) { ami_sizable(stdout, e); }
void sysbar(FILE* f, ami_long e) { ami_sysbar(f, e); }
void sysbar(ami_long e) { ami_sysbar(stdout, e); }
void autobuf(FILE* f, ami_long e) { ami_autobuf(f, e); }
void autobuf(ami_long e) { ami_autobuf(stdout, e); }
void menu(FILE* f, menuptr m) { ami_menu(f, (ami_menuptr)m); }
void menu(menuptr m) { ami_menu(stdout, (ami_menuptr)m); }
void menuena(FILE* f, ami_long id, ami_long onoff) { ami_menuena(f, id, onoff); }
void menuena(ami_long id, ami_long onoff) { ami_menuena(stdout, id, onoff); }
void menusel(FILE* f, ami_long id, ami_long select) { ami_menusel(f, id, select); }
void menusel(ami_long id, ami_long select) { ami_menusel(stdout, id, select); }
void stdmenu(stdmenusel sms, menuptr* sm, menuptr pm) { ami_stdmenu(sms, (ami_menuptr*)sm, (ami_menuptr)pm); }
ami_long  getwinid(void) { return ami_getwinid(); }
void focus(FILE* f) { ami_focus(f); }
void focus(void) { ami_focus(stdout); }

/* widgets/controls */
ami_long  getwigid(FILE* f) { return ami_getwigid(f); }
ami_long  getwigid(void) { return ami_getwigid(stdout); }
void killwidget(FILE* f, ami_long id) { ami_killwidget(f, id); }
void killwidget(ami_long id) { ami_killwidget(stdout, id); }
void selectwidget(FILE* f, ami_long id, ami_long e) { ami_selectwidget(f, id, e); }
void selectwidget(ami_long id, ami_long e) { ami_selectwidget(stdout, id, e); }
void enablewidget(FILE* f, ami_long id, ami_long e) { ami_enablewidget(f, id, e); }
void enablewidget(ami_long id, ami_long e) { ami_enablewidget(stdout, id, e); }
void getwidgettext(FILE* f, ami_long id, char* s, ami_long sl) { ami_getwidgettext(f, id, s, sl); }
void getwidgettext(ami_long id, char* s, ami_long sl) { ami_getwidgettext(stdout, id, s, sl); }
void putwidgettext(FILE* f, ami_long id, char* s) { ami_putwidgettext(f, id, s); }
void putwidgettext(ami_long id, char* s) { ami_putwidgettext(stdout, id, s); }
void sizwidget(FILE* f, ami_long id, ami_long x, ami_long y) { ami_sizwidget(f, id, x, y); }
void sizwidget(ami_long id, ami_long x, ami_long y) { ami_sizwidget(stdout, id, x, y); }
void sizwidgetg(FILE* f, ami_long id, ami_long x, ami_long y) { ami_sizwidgetg(f, id, x, y); }
void sizwidgetg(ami_long id, ami_long x, ami_long y) { ami_sizwidgetg(stdout, id, x, y); }
void poswidget(FILE* f, ami_long id, ami_long x, ami_long y) { ami_poswidget(f, id, x, y); }
void poswidget(ami_long id, ami_long x, ami_long y) { ami_poswidget(stdout, id, x, y); }
void poswidgetg(FILE* f, ami_long id, ami_long x, ami_long y) { ami_poswidgetg(f, id, x, y); }
void poswidgetg(ami_long id, ami_long x, ami_long y) { ami_poswidgetg(stdout, id, x, y); }
void backwidget(FILE* f, ami_long id) { ami_backwidget(f, id); }
void backwidget(ami_long id) { ami_backwidget(stdout, id); }
void frontwidget(FILE* f, ami_long id) { ami_frontwidget(f, id); }
void frontwidget(ami_long id) { ami_frontwidget(stdout, id); }
void focuswidget(FILE* f, ami_long id) { ami_focuswidget(f, id); }
void focuswidget(ami_long id) { ami_focuswidget(stdout, id); }
void buttonsiz(FILE* f, char* s, ami_long* w, ami_long* h) { ami_buttonsiz(f, s, w, h); }
void buttonsiz(char* s, ami_long* w, ami_long* h) { ami_buttonsiz(stdout, s, w, h); }
void buttonsizg(FILE* f, char* s, ami_long* w, ami_long* h) { ami_buttonsizg(f, s, w, h); }
void buttonsizg(char* s, ami_long* w, ami_long* h) { ami_buttonsizg(stdout, s, w, h); }
void checkboxsiz(FILE* f, char* s, ami_long* w, ami_long* h) { ami_checkboxsiz(f, s, w, h); }
void checkboxsiz(char* s, ami_long* w, ami_long* h) { ami_checkboxsiz(stdout, s, w, h); }
void checkboxsizg(FILE* f, char* s, ami_long* w, ami_long* h) { ami_checkboxsizg(f, s, w, h); }
void checkboxsizg(char* s, ami_long* w, ami_long* h) { ami_checkboxsizg(stdout, s, w, h); }
void radiobuttonsiz(FILE* f, char* s, ami_long* w, ami_long* h) { ami_radiobuttonsiz(f, s, w, h); }
void radiobuttonsiz(char* s, ami_long* w, ami_long* h) { ami_radiobuttonsiz(stdout, s, w, h); }
void radiobuttonsizg(FILE* f, char* s, ami_long* w, ami_long* h) { ami_radiobuttonsizg(f, s, w, h); }
void radiobuttonsizg(char* s, ami_long* w, ami_long* h) { ami_radiobuttonsizg(stdout, s, w, h); }
void groupsiz(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsiz(f, s, cw, ch, w, h, ox, oy); }
void groupsiz(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsiz(stdout, s, cw, ch, w, h, ox, oy); }
void groupsizg(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsizg(f, s, cw, ch, w, h, ox, oy); }
void groupsizg(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsizg(stdout, s, cw, ch, w, h, ox, oy); }
void scrollvertsiz(FILE* f, ami_long* w, ami_long* h) { ami_scrollvertsiz(f, w, h); }
void scrollvertsiz(ami_long* w, ami_long* h) { ami_scrollvertsiz(stdout, w, h); }
void scrollvertsizg(FILE* f, ami_long* w, ami_long* h) { ami_scrollvertsizg(f, w, h); }
void scrollvertsizg(ami_long* w, ami_long* h) { ami_scrollvertsizg(stdout, w, h); }
void scrollhorizsiz(FILE* f, ami_long* w, ami_long* h) { ami_scrollhorizsiz(f, w, h); }
void scrollhorizsiz(ami_long* w, ami_long* h) { ami_scrollhorizsiz(stdout, w, h); }
void scrollhorizsizg(FILE* f, ami_long* w, ami_long* h) { ami_scrollhorizsizg(f, w, h); }
void scrollhorizsizg(ami_long* w, ami_long* h) { ami_scrollhorizsizg(stdout, w, h); }
void scrollpos(FILE* f, ami_long id, ami_long r) { ami_scrollpos(f, id, r); }
void scrollpos(ami_long id, ami_long r) { ami_scrollpos(stdout, id, r); }
void scrollsiz(FILE* f, ami_long id, ami_long r) { ami_scrollsiz(f, id, r); }
void scrollsiz(ami_long id, ami_long r) { ami_scrollsiz(stdout, id, r); }
void numselboxsiz(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsiz(f, l, u, w, h); }
void numselboxsiz(ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsiz(stdout, l, u, w, h); }
void numselboxsizg(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsizg(f, l, u, w, h); }
void numselboxsizg(ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsizg(stdout, l, u, w, h); }
void editboxsiz(FILE* f, char* s, ami_long* w, ami_long* h) { ami_editboxsiz(f, s, w, h); }
void editboxsiz(char* s, ami_long* w, ami_long* h) { ami_editboxsiz(stdout, s, w, h); }
void editboxsizg(FILE* f, char* s, ami_long* w, ami_long* h) { ami_editboxsizg(f, s, w, h); }
void editboxsizg(char* s, ami_long* w, ami_long* h) { ami_editboxsizg(stdout, s, w, h); }
void progbarsiz(FILE* f, ami_long* w, ami_long* h) { ami_progbarsiz(f, w, h); }
void progbarsiz(ami_long* w, ami_long* h) { ami_progbarsiz(stdout, w, h); }
void progbarsizg(FILE* f, ami_long* w, ami_long* h) { ami_progbarsizg(f, w, h); }
void progbarsizg(ami_long* w, ami_long* h) { ami_progbarsizg(stdout, w, h); }
void progbarpos(FILE* f, ami_long id, ami_long pos) { ami_progbarpos(f, id, pos); }
void progbarpos(ami_long id, ami_long pos) { ami_progbarpos(stdout, id, pos); }
void listboxsiz(FILE* f, strptr sp, ami_long* w, ami_long* h) { ami_listboxsiz(f, (ami_strptr)sp, w, h); }
void listboxsiz(strptr sp, ami_long* w, ami_long* h) { ami_listboxsiz(stdout, (ami_strptr)sp, w, h); }
void listboxsizg(FILE* f, strptr sp, ami_long* w, ami_long* h) { ami_listboxsizg(f, (ami_strptr)sp, w, h); }
void listboxsizg(strptr sp, ami_long* w, ami_long* h) { ami_listboxsizg(stdout, (ami_strptr)sp, w, h); }
void dropboxsiz(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsiz(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsiz(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsizg(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsizg(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsizg(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsiz(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsiz(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsiz(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsizg(FILE* f, strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsizg(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsizg(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void slidehorizsiz(FILE* f, ami_long* w, ami_long* h) { ami_slidehorizsiz(f, w, h); }
void slidehorizsiz(ami_long* w, ami_long* h) { ami_slidehorizsiz(stdout, w, h); }
void slidehorizsizg(FILE* f, ami_long* w, ami_long* h) { ami_slidehorizsizg(f, w, h); }
void slidehorizsizg(ami_long* w, ami_long* h) { ami_slidehorizsizg(stdout, w, h); }
void slidevertsiz(FILE* f, ami_long* w, ami_long* h) { ami_slidevertsiz(f, w, h); }
void slidevertsiz(ami_long* w, ami_long* h) { ami_slidevertsiz(stdout, w, h); }
void slidevertsizg(FILE* f, ami_long* w, ami_long* h) { ami_slidevertsizg(f, w, h); }
void slidevertsizg(ami_long* w, ami_long* h) { ami_slidevertsizg(stdout, w, h); }
void tabbarsiz(FILE* f, strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsiz(f, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsiz(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsiz(stdout, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsizg(FILE* f, strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsizg(f, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsizg(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsizg(stdout, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarclient(FILE* f, tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclient(f, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclient(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclient(stdout, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclientg(FILE* f, tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclientg(f, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclientg(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclientg(stdout, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabsel(FILE* f, ami_long id, ami_long tn) { ami_tabsel(f, id, tn); }
void tabsel(ami_long id, ami_long tn) { ami_tabsel(stdout, id, tn); }

/* dialogs */
void alert(char* title, char* message) { ami_alert(title, message); }
void querycolor(ami_long* r, ami_long* g, ami_long* b) { ami_querycolor(r, g, b); }
void queryopen(char* s, ami_long sl) { ami_queryopen(s, sl); }
void querysave(char* s, ami_long sl) { ami_querysave(s, sl); }
void queryfind(char* s, ami_long sl, qfnopts* opt) { ami_queryfind(s, sl, (ami_qfnopts*)opt); }
void queryfindrep(char* s, ami_long sl, char* r, ami_long rl, qfropts* opt) { ami_queryfindrep(s, sl, r, rl, (ami_qfropts*)opt); }
void queryfont(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
               ami_long* bg, ami_long* bb, qfteffects* effect) { ami_queryfont(f, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }
void queryfont(ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
               ami_long* bg, ami_long* bb, qfteffects* effect) { ami_queryfont(stdout, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }

/* methods */
graph::graph(void)

{

    /* One graph object at a time. The events come back through one
       global hook: a second object would not share the events with the
       first, it would take them all -- and the second hooking of the
       chain saves the hook itself as the handler to pass unhandled
       events to, which is a loop with no bottom. Refusing is the only
       honest answer the wrapper has. */
    if (graphoCB) {

        fprintf(stderr, "graphics: only one graph object may exist\n");
        exit(1);

    }
    infile = stdin;
    outfile = stdout;
    graphoCB = this;
    eventsover(graphCB, &graphoeh);

}

graph::~graph(void)

{

    pevthan tmp;

    /* put the chain back the way it was found, so no event is ever
       delivered to an object that no longer exists */
    eventsover(graphoeh, &tmp);
    graphoCB = 0;

}

/* text */
void graph::cursor(ami_long x, ami_long y) { ami_cursor(outfile, x, y); }
ami_long  graph::maxx(void) { return ami_maxx(outfile); }
ami_long  graph::maxy(void) { return ami_maxy(outfile); }
void graph::home(void) { ami_home(outfile); }
void graph::del(void) { ami_del(outfile); }
void graph::up(void) { ami_up(outfile); }
void graph::down(void) { ami_down(outfile); }
void graph::left(void) { ami_left(outfile); }
void graph::right(void) { ami_right(outfile); }
void graph::blink(ami_long e) { ami_blink(outfile, e); }
void graph::reverse(ami_long e) { ami_reverse(outfile, e); }
void graph::underline(ami_long e) { ami_underline(outfile, e); }
void graph::superscript(ami_long e) { ami_superscript(outfile, e); }
void graph::subscript(ami_long e) { ami_subscript(outfile, e); }
void graph::italic(ami_long e) { ami_italic(outfile, e); }
void graph::bold(ami_long e) { ami_bold(outfile, e); }
void graph::strikeout(ami_long e) { ami_strikeout(outfile, e); }
void graph::standout(ami_long e) { ami_standout(outfile, e); }
void graph::fcolor(color c) { ami_fcolor(outfile, (ami_color)c); }
void graph::bcolor(color c) { ami_bcolor(outfile, (ami_color)c); }
void graph::autom(ami_long e) { ami_auto(outfile, e); }
void graph::curvis(ami_long e) { ami_curvis(outfile, e); }
void graph::scroll(ami_long x, ami_long y) { ami_scroll(outfile, x, y); }
ami_long  graph::curx(void) { return ami_curx(outfile); }
ami_long  graph::cury(void) { return ami_cury(outfile); }
ami_long  graph::curbnd(void) { return ami_curbnd(outfile); }
void graph::select(ami_long u, ami_long d) { ami_select(outfile, u, d); }
void graph::event(evtrec* er) { ami_event(infile, (ami_evtptr)er); }
void graph::timer(ami_long i, ami_long t, ami_long r) { ami_timer(outfile, i, t, r); }
void graph::killtimer(ami_long i) { ami_killtimer(outfile, i); }
ami_long  graph::mouse(void) { return ami_mouse(outfile); }
ami_long  graph::mousebutton(ami_long m) { return ami_mousebutton(outfile, m); }
ami_long  graph::joystick(void) { return ami_joystick(outfile); }
ami_long  graph::joybutton(ami_long j) { return ami_joybutton(outfile, j); }
ami_long  graph::joyaxis(ami_long j) { return ami_joyaxis(outfile, j); }
void graph::settab(ami_long t) { ami_settab(outfile, t); }
void graph::restab(ami_long t) { ami_restab(outfile, t); }
void graph::clrtab(void) { ami_clrtab(outfile); }
ami_long  graph::funkey(void) { return ami_funkey(outfile); }
void graph::frametimer(ami_long e) { ami_frametimer(outfile, e); }
void graph::autohold(ami_long e) { ami_autohold(e); }
void graph::wrtstr(char* s) { ami_wrtstr(outfile, s); }
void graph::wrtstrn(char* s, ami_long n) { ami_wrtstrn(outfile, s, n); }
void graph::sizbuf(ami_long x, ami_long y) { ami_sizbuf(outfile, x, y); }
void graph::title(char* ts) { ami_title(outfile, ts); }
void graph::sendevent(evtrec* er) { ami_sendevent(outfile, (ami_evtptr)er); }

/* graphical */
ami_long  graph::maxxg(void) { return ami_maxxg(outfile); }
ami_long  graph::maxyg(void) { return ami_maxyg(outfile); }
ami_long  graph::curxg(void) { return ami_curxg(outfile); }
ami_long  graph::curyg(void) { return ami_curyg(outfile); }
void graph::line(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_line(outfile, x1, y1, x2, y2); }
void graph::linewidth(ami_long w) { ami_linewidth(outfile, w); }
void graph::rect(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_rect(outfile, x1, y1, x2, y2); }
void graph::frect(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_frect(outfile, x1, y1, x2, y2); }
void graph::rrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_rrect(outfile, x1, y1, x2, y2, xs, ys); }
void graph::frrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_frrect(outfile, x1, y1, x2, y2, xs, ys); }
void graph::ellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_ellipse(outfile, x1, y1, x2, y2); }
void graph::fellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_fellipse(outfile, x1, y1, x2, y2); }
void graph::arc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_arc(outfile, x1, y1, x2, y2, sa, ea); }
void graph::farc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_farc(outfile, x1, y1, x2, y2, sa, ea); }
void graph::fchord(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_fchord(outfile, x1, y1, x2, y2, sa, ea); }
void graph::ftriangle(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3) { ami_ftriangle(outfile, x1, y1, x2, y2, x3, y3); }
void graph::cursorg(ami_long x, ami_long y) { ami_cursorg(outfile, x, y); }
ami_long  graph::baseline(void) { return ami_baseline(outfile); }
void graph::setpixel(ami_long x, ami_long y) { ami_setpixel(outfile, x, y); }
void graph::fover(void) { ami_fover(outfile); }
void graph::bover(void) { ami_bover(outfile); }
void graph::finvis(void) { ami_finvis(outfile); }
void graph::binvis(void) { ami_binvis(outfile); }
void graph::fxor(void) { ami_fxor(outfile); }
void graph::bxor(void) { ami_bxor(outfile); }
void graph::fand(void) { ami_fand(outfile); }
void graph::band(void) { ami_band(outfile); }
void graph::for_(void) { ami_for(outfile); }
void graph::bor(void) { ami_bor(outfile); }
ami_long  graph::chrsizx(void) { return ami_chrsizx(outfile); }
ami_long  graph::chrsizy(void) { return ami_chrsizy(outfile); }
ami_long  graph::fonts(void) { return ami_fonts(outfile); }
void graph::font(ami_long fc) { ami_font(outfile, fc); }
void graph::fontnam(ami_long fc, char* fns, ami_long fnsl) { ami_fontnam(outfile, fc, fns, fnsl); }
void graph::fontsiz(ami_long s) { ami_fontsiz(outfile, s); }
void graph::chrspcy(ami_long s) { ami_chrspcy(outfile, s); }
void graph::chrspcx(ami_long s) { ami_chrspcx(outfile, s); }
ami_long  graph::dpmx(void) { return ami_dpmx(outfile); }
ami_long  graph::dpmy(void) { return ami_dpmy(outfile); }
ami_long  graph::strsiz(const char* s) { return ami_strsiz(outfile, s); }
ami_long  graph::chrpos(const char* s, ami_long p) { return ami_chrpos(outfile, s, p); }
void graph::writejust(const char* s, ami_long n) { ami_writejust(outfile, s, n); }
ami_long  graph::justpos(const char* s, ami_long p, ami_long n) { return ami_justpos(outfile, s, p, n); }
void graph::condensed(ami_long e) { ami_condensed(outfile, e); }
void graph::extended(ami_long e) { ami_extended(outfile, e); }
void graph::xlight(ami_long e) { ami_xlight(outfile, e); }
void graph::light(ami_long e) { ami_light(outfile, e); }
void graph::xbold(ami_long e) { ami_xbold(outfile, e); }
void graph::hollow(ami_long e) { ami_hollow(outfile, e); }
void graph::raised(ami_long e) { ami_raised(outfile, e); }
void graph::settabg(ami_long t) { ami_settabg(outfile, t); }
void graph::restabg(ami_long t) { ami_restabg(outfile, t); }
void graph::fcolorg(ami_long r, ami_long g, ami_long b) { ami_fcolorg(outfile, r, g, b); }
void graph::fcolorc(ami_long r, ami_long g, ami_long b) { ami_fcolorc(outfile, r, g, b); }
void graph::bcolorg(ami_long r, ami_long g, ami_long b) { ami_bcolorg(outfile, r, g, b); }
void graph::bcolorc(ami_long r, ami_long g, ami_long b) { ami_bcolorc(outfile, r, g, b); }
void graph::loadpict(ami_long p, char* fn) { ami_loadpict(outfile, p, fn); }
ami_long  graph::pictsizx(ami_long p) { return ami_pictsizx(outfile, p); }
ami_long  graph::pictsizy(ami_long p) { return ami_pictsizy(outfile, p); }
void graph::picture(ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_picture(outfile, p, x1, y1, x2, y2); }
void graph::delpict(ami_long p) { ami_delpict(outfile, p); }
void graph::scrollg(ami_long x, ami_long y) { ami_scrollg(outfile, x, y); }
void graph::blockcopyg(ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2, ami_long sy2,
                       ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2)
    { ami_blockcopyg(outfile, s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2); }
void graph::path(ami_long a) { ami_path(outfile, a); }

/* window management */
void graph::buffer(ami_long e) { ami_buffer(outfile, e); }
void graph::sizbufg(ami_long x, ami_long y) { ami_sizbufg(outfile, x, y); }
void graph::getsiz(ami_long* x, ami_long* y) { ami_getsiz(outfile, x, y); }
void graph::getsizg(ami_long* x, ami_long* y) { ami_getsizg(outfile, x, y); }
void graph::setsiz(ami_long x, ami_long y) { ami_setsiz(outfile, x, y); }
void graph::setsizg(ami_long x, ami_long y) { ami_setsizg(outfile, x, y); }
void graph::setpos(ami_long x, ami_long y) { ami_setpos(outfile, x, y); }
void graph::setposg(ami_long x, ami_long y) { ami_setposg(outfile, x, y); }
void graph::scnsiz(ami_long* x, ami_long* y) { ami_scnsiz(outfile, x, y); }
void graph::scnsizg(ami_long* x, ami_long* y) { ami_scnsizg(outfile, x, y); }
void graph::scncen(ami_long* x, ami_long* y) { ami_scncen(outfile, x, y); }
void graph::scnceng(ami_long* x, ami_long* y) { ami_scnceng(outfile, x, y); }
void graph::winclient(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclient(outfile, cx, cy, wx, wy, (ami_winmodset)ms); }
void graph::winclientg(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclientg(outfile, cx, cy, wx, wy, (ami_winmodset)ms); }
void graph::front(void) { ami_front(outfile); }
void graph::back(void) { ami_back(outfile); }
void graph::frame(ami_long e) { ami_frame(outfile, e); }
void graph::sizable(ami_long e) { ami_sizable(outfile, e); }
void graph::sysbar(ami_long e) { ami_sysbar(outfile, e); }
void graph::autobuf(ami_long e) { ami_autobuf(outfile, e); }
void graph::menu(menuptr m) { ami_menu(outfile, (ami_menuptr)m); }
void graph::menuena(ami_long id, ami_long onoff) { ami_menuena(outfile, id, onoff); }
void graph::menusel(ami_long id, ami_long select) { ami_menusel(outfile, id, select); }
void graph::focus(void) { ami_focus(outfile); }

/* widgets */
ami_long  graph::getwigid(void) { return ami_getwigid(outfile); }
void graph::killwidget(ami_long id) { ami_killwidget(outfile, id); }
void graph::selectwidget(ami_long id, ami_long e) { ami_selectwidget(outfile, id, e); }
void graph::enablewidget(ami_long id, ami_long e) { ami_enablewidget(outfile, id, e); }
void graph::getwidgettext(ami_long id, char* s, ami_long sl) { ami_getwidgettext(outfile, id, s, sl); }
void graph::putwidgettext(ami_long id, char* s) { ami_putwidgettext(outfile, id, s); }
void graph::sizwidget(ami_long id, ami_long x, ami_long y) { ami_sizwidget(outfile, id, x, y); }
void graph::sizwidgetg(ami_long id, ami_long x, ami_long y) { ami_sizwidgetg(outfile, id, x, y); }
void graph::poswidget(ami_long id, ami_long x, ami_long y) { ami_poswidget(outfile, id, x, y); }
void graph::poswidgetg(ami_long id, ami_long x, ami_long y) { ami_poswidgetg(outfile, id, x, y); }
void graph::backwidget(ami_long id) { ami_backwidget(outfile, id); }
void graph::frontwidget(ami_long id) { ami_frontwidget(outfile, id); }
void graph::focuswidget(ami_long id) { ami_focuswidget(outfile, id); }
void graph::buttonsiz(char* s, ami_long* w, ami_long* h) { ami_buttonsiz(outfile, s, w, h); }
void graph::buttonsizg(char* s, ami_long* w, ami_long* h) { ami_buttonsizg(outfile, s, w, h); }
void graph::button(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_button(outfile, x1, y1, x2, y2, s, id); }
void graph::buttong(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_buttong(outfile, x1, y1, x2, y2, s, id); }
void graph::checkboxsiz(char* s, ami_long* w, ami_long* h) { ami_checkboxsiz(outfile, s, w, h); }
void graph::checkboxsizg(char* s, ami_long* w, ami_long* h) { ami_checkboxsizg(outfile, s, w, h); }
void graph::checkbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_checkbox(outfile, x1, y1, x2, y2, s, id); }
void graph::checkboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_checkboxg(outfile, x1, y1, x2, y2, s, id); }
void graph::radiobuttonsiz(char* s, ami_long* w, ami_long* h) { ami_radiobuttonsiz(outfile, s, w, h); }
void graph::radiobuttonsizg(char* s, ami_long* w, ami_long* h) { ami_radiobuttonsizg(outfile, s, w, h); }
void graph::radiobutton(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_radiobutton(outfile, x1, y1, x2, y2, s, id); }
void graph::radiobuttong(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_radiobuttong(outfile, x1, y1, x2, y2, s, id); }
void graph::groupsiz(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsiz(outfile, s, cw, ch, w, h, ox, oy); }
void graph::groupsizg(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsizg(outfile, s, cw, ch, w, h, ox, oy); }
void graph::group(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_group(outfile, x1, y1, x2, y2, s, id); }
void graph::groupg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { ami_groupg(outfile, x1, y1, x2, y2, s, id); }
void graph::background(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_background(outfile, x1, y1, x2, y2, id); }
void graph::backgroundg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_backgroundg(outfile, x1, y1, x2, y2, id); }
void graph::scrollvertsiz(ami_long* w, ami_long* h) { ami_scrollvertsiz(outfile, w, h); }
void graph::scrollvertsizg(ami_long* w, ami_long* h) { ami_scrollvertsizg(outfile, w, h); }
void graph::scrollvert(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_scrollvert(outfile, x1, y1, x2, y2, id); }
void graph::scrollvertg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_scrollvertg(outfile, x1, y1, x2, y2, id); }
void graph::scrollhorizsiz(ami_long* w, ami_long* h) { ami_scrollhorizsiz(outfile, w, h); }
void graph::scrollhorizsizg(ami_long* w, ami_long* h) { ami_scrollhorizsizg(outfile, w, h); }
void graph::scrollhoriz(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_scrollhoriz(outfile, x1, y1, x2, y2, id); }
void graph::scrollhorizg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_scrollhorizg(outfile, x1, y1, x2, y2, id); }
void graph::scrollpos(ami_long id, ami_long r) { ami_scrollpos(outfile, id, r); }
void graph::scrollsiz(ami_long id, ami_long r) { ami_scrollsiz(outfile, id, r); }
void graph::numselboxsiz(ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsiz(outfile, l, u, w, h); }
void graph::numselboxsizg(ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsizg(outfile, l, u, w, h); }
void graph::numselbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id) { ami_numselbox(outfile, x1, y1, x2, y2, l, u, id); }
void graph::numselboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id) { ami_numselboxg(outfile, x1, y1, x2, y2, l, u, id); }
void graph::editboxsiz(char* s, ami_long* w, ami_long* h) { ami_editboxsiz(outfile, s, w, h); }
void graph::editboxsizg(char* s, ami_long* w, ami_long* h) { ami_editboxsizg(outfile, s, w, h); }
void graph::editbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_editbox(outfile, x1, y1, x2, y2, id); }
void graph::editboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_editboxg(outfile, x1, y1, x2, y2, id); }
void graph::progbarsiz(ami_long* w, ami_long* h) { ami_progbarsiz(outfile, w, h); }
void graph::progbarsizg(ami_long* w, ami_long* h) { ami_progbarsizg(outfile, w, h); }
void graph::progbar(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_progbar(outfile, x1, y1, x2, y2, id); }
void graph::progbarg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { ami_progbarg(outfile, x1, y1, x2, y2, id); }
void graph::progbarpos(ami_long id, ami_long pos) { ami_progbarpos(outfile, id, pos); }
void graph::listboxsiz(strptr sp, ami_long* w, ami_long* h) { ami_listboxsiz(outfile, (ami_strptr)sp, w, h); }
void graph::listboxsizg(strptr sp, ami_long* w, ami_long* h) { ami_listboxsizg(outfile, (ami_strptr)sp, w, h); }
void graph::listbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id) { ami_listbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::listboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id) { ami_listboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsiz(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsizg(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id) { ami_dropbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id) { ami_dropboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropeditboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsiz(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropeditboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsizg(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropeditbox(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id) { ami_dropeditbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropeditboxg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id) { ami_dropeditboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::slidehorizsiz(ami_long* w, ami_long* h) { ami_slidehorizsiz(outfile, w, h); }
void graph::slidehorizsizg(ami_long* w, ami_long* h) { ami_slidehorizsizg(outfile, w, h); }
void graph::slidehoriz(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { ami_slidehoriz(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidehorizg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { ami_slidehorizg(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidevertsiz(ami_long* w, ami_long* h) { ami_slidevertsiz(outfile, w, h); }
void graph::slidevertsizg(ami_long* w, ami_long* h) { ami_slidevertsizg(outfile, w, h); }
void graph::slidevert(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { ami_slidevert(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidevertg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { ami_slidevertg(outfile, x1, y1, x2, y2, mark, id); }
void graph::tabbarsiz(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsiz(outfile, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void graph::tabbarsizg(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsizg(outfile, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void graph::tabbarclient(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclient(outfile, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void graph::tabbarclientg(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclientg(outfile, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void graph::tabbar(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, tabori tor, ami_long id) { ami_tabbar(outfile, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void graph::tabbarg(ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, tabori tor, ami_long id) { ami_tabbarg(outfile, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void graph::tabsel(ami_long id, ami_long tn) { ami_tabsel(outfile, id, tn); }

/* dialogs */
void graph::queryfont(ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
                      ami_long* bg, ami_long* bb, qfteffects* effect) { ami_queryfont(outfile, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }

/* virtual callbacks */
ami_long graph::evchar(char c) { return 0; }
ami_long graph::evup(void) { return 0; }
ami_long graph::evdown(void) { return 0; }
ami_long graph::evleft(void) { return 0; }
ami_long graph::evright(void) { return 0; }
ami_long graph::evleftw(void) { return 0; }
ami_long graph::evrightw(void) { return 0; }
ami_long graph::evhome(void) { return 0; }
ami_long graph::evhomes(void) { return 0; }
ami_long graph::evhomel(void) { return 0; }
ami_long graph::evend(void) { return 0; }
ami_long graph::evends(void) { return 0; }
ami_long graph::evendl(void) { return 0; }
ami_long graph::evscrl(void) { return 0; }
ami_long graph::evscrr(void) { return 0; }
ami_long graph::evscru(void) { return 0; }
ami_long graph::evscrd(void) { return 0; }
ami_long graph::evpagd(void) { return 0; }
ami_long graph::evpagu(void) { return 0; }
ami_long graph::evtab(void) { return 0; }
ami_long graph::eventer(void) { return 0; }
ami_long graph::evinsert(void) { return 0; }
ami_long graph::evinsertl(void) { return 0; }
ami_long graph::evinsertt(void) { return 0; }
ami_long graph::evdel(void) { return 0; }
ami_long graph::evdell(void) { return 0; }
ami_long graph::evdelcf(void) { return 0; }
ami_long graph::evdelcb(void) { return 0; }
ami_long graph::evcopy(void) { return 0; }
ami_long graph::evcopyl(void) { return 0; }
ami_long graph::evcan(void) { return 0; }
ami_long graph::evstop(void) { return 0; }
ami_long graph::evcont(void) { return 0; }
ami_long graph::evprint(void) { return 0; }
ami_long graph::evprintb(void) { return 0; }
ami_long graph::evprints(void) { return 0; }
ami_long graph::evfun(ami_long k) { return 0; }
ami_long graph::evmenu(void) { return 0; }
ami_long graph::evmouba(ami_long m, ami_long b) { return 0; }
ami_long graph::evmoubd(ami_long m, ami_long b) { return 0; }
ami_long graph::evmoumov(ami_long m, ami_long x, ami_long y) { return 0; }
ami_long graph::evtim(ami_long t) { return 0; }
ami_long graph::evjoyba(ami_long j, ami_long b) { return 0; }
ami_long graph::evjoybd(ami_long j, ami_long b) { return 0; }
ami_long graph::evjoymov(ami_long j, ami_long x, ami_long y, ami_long z) { return 0; }
ami_long graph::evresize(void) { return 0; }
ami_long graph::evfocus(void) { return 0; }
ami_long graph::evnofocus(void) { return 0; }
ami_long graph::evhover(void) { return 0; }
ami_long graph::evnohover(void) { return 0; }
ami_long graph::evterm(void) { return 0; }
ami_long graph::evframe(void) { return 0; }
ami_long graph::evmoumovg(ami_long m, ami_long x, ami_long y) { return 0; }
ami_long graph::evredraw(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { return 0; }
ami_long graph::evmin(void) { return 0; }
ami_long graph::evmax(void) { return 0; }
ami_long graph::evnorm(void) { return 0; }
ami_long graph::evmenus(ami_long id) { return 0; }
ami_long graph::evbutton(ami_long id) { return 0; }
ami_long graph::evchkbox(ami_long id) { return 0; }
ami_long graph::evradbut(ami_long id) { return 0; }
ami_long graph::evsclull(ami_long id) { return 0; }
ami_long graph::evscldrl(ami_long id) { return 0; }
ami_long graph::evsclulp(ami_long id) { return 0; }
ami_long graph::evscldrp(ami_long id) { return 0; }
ami_long graph::evsclpos(ami_long id, ami_long pos) { return 0; }
ami_long graph::evedtbox(ami_long id) { return 0; }
ami_long graph::evnumbox(ami_long id, ami_long val) { return 0; }
ami_long graph::evlstbox(ami_long id, ami_long sel) { return 0; }
ami_long graph::evdrpbox(ami_long id, ami_long sel) { return 0; }
ami_long graph::evdrebox(ami_long id) { return 0; }
ami_long graph::evsldpos(ami_long id, ami_long pos) { return 0; }
ami_long graph::evtabbar(ami_long id, ami_long sel) { return 0; }
ami_long graph::evusize(void) { return 0; }
ami_long graph::evdsize(void) { return 0; }

void graph::graphCB(evtrec* er)

{

    ami_long handled;

    switch (er->etype) {

        case etchar:    handled = graphoCB->evchar(er->echar); break;
        case etup:      handled = graphoCB->evup(); break;
        case etdown:    handled = graphoCB->evdown(); break;
        case etleft:    handled = graphoCB->evleft(); break;
        case etright:   handled = graphoCB->evright(); break;
        case etleftw:   handled = graphoCB->evleftw(); break;
        case etrightw:  handled = graphoCB->evrightw(); break;
        case ethome:    handled = graphoCB->evhome(); break;
        case ethomes:   handled = graphoCB->evhomes(); break;
        case ethomel:   handled = graphoCB->evhomel(); break;
        case etend:     handled = graphoCB->evend(); break;
        case etends:    handled = graphoCB->evends(); break;
        case etendl:    handled = graphoCB->evendl(); break;
        case etscrl:    handled = graphoCB->evscrl(); break;
        case etscrr:    handled = graphoCB->evscrr(); break;
        case etscru:    handled = graphoCB->evscru(); break;
        case etscrd:    handled = graphoCB->evscrd(); break;
        case etpagd:    handled = graphoCB->evpagd(); break;
        case etpagu:    handled = graphoCB->evpagu(); break;
        case ettab:     handled = graphoCB->evtab(); break;
        case etenter:   handled = graphoCB->eventer(); break;
        case etinsert:  handled = graphoCB->evinsert(); break;
        case etinsertl: handled = graphoCB->evinsertl(); break;
        case etinsertt: handled = graphoCB->evinsertt(); break;
        case etdel:     handled = graphoCB->evdel(); break;
        case etdell:    handled = graphoCB->evdell(); break;
        case etdelcf:   handled = graphoCB->evdelcf(); break;
        case etdelcb:   handled = graphoCB->evdelcb(); break;
        case etcopy:    handled = graphoCB->evcopy(); break;
        case etcopyl:   handled = graphoCB->evcopyl(); break;
        case etcan:     handled = graphoCB->evcan(); break;
        case etstop:    handled = graphoCB->evstop(); break;
        case etcont:    handled = graphoCB->evcont(); break;
        case etprint:   handled = graphoCB->evprint(); break;
        case etprintb:  handled = graphoCB->evprintb(); break;
        case etprints:  handled = graphoCB->evprints(); break;
        case etfun:     handled = graphoCB->evfun(er->fkey); break;
        case etmenu:    handled = graphoCB->evmenu(); break;
        case etmouba:   handled = graphoCB->evmouba(er->amoun, er->amoubn);
            break;
        case etmoubd:   handled = graphoCB->evmoubd(er->dmoun, er->dmoubn);
            break;
        case etmoumov:
            handled = graphoCB->evmoumov(er->mmoun, er->moupx, er->moupy);
            break;
        case ettim:     handled = graphoCB->evtim(er->timnum); break;
        case etjoyba:   handled = graphoCB->evjoyba(er->ajoyn, er->ajoybn);
            break;
        case etjoybd:   handled = graphoCB->evjoybd(er->djoyn, er->djoybn);
            break;
        case etjoymov:
            handled = graphoCB->evjoymov(er->mjoyn, er->joypx, er->joypy,
                                         er->joypz);
            break;
        case etresize:  handled = graphoCB->evresize(); break;
        case etfocus:   handled = graphoCB->evfocus(); break;
        case etnofocus: handled = graphoCB->evnofocus(); break;
        case ethover:   handled = graphoCB->evhover(); break;
        case etnohover: handled = graphoCB->evnohover(); break;
        case etterm:    handled = graphoCB->evterm(); break;
        case etframe:   handled = graphoCB->evframe(); break;
        case etmoumovg:
            handled = graphoCB->evmoumovg(er->mmoung, er->moupxg, er->moupyg);
            break;
        case etredraw:  handled = graphoCB->evredraw(er->rsx, er->rsy, er->rex, er->rey); break;
        case etmin:     handled = graphoCB->evmin(); break;
        case etmax:     handled = graphoCB->evmax(); break;
        case etnorm:    handled = graphoCB->evnorm(); break;
        case etmenus:   handled = graphoCB->evmenus(er->menuid); break;
        case etbutton:  handled = graphoCB->evbutton(er->butid); break;
        case etchkbox:  handled = graphoCB->evchkbox(er->ckbxid); break;
        case etradbut:  handled = graphoCB->evradbut(er->radbid); break;
        case etsclull:  handled = graphoCB->evsclull(er->sclulid); break;
        case etscldrl:  handled = graphoCB->evscldrl(er->scldrid); break;
        case etsclulp:  handled = graphoCB->evsclulp(er->sclupid); break;
        case etscldrp:  handled = graphoCB->evscldrp(er->scldpid); break;
        case etsclpos:  handled = graphoCB->evsclpos(er->sclpid, er->sclpos); break;
        case etedtbox:  handled = graphoCB->evedtbox(er->edtbid); break;
        case etnumbox:  handled = graphoCB->evnumbox(er->numbid, er->numbsl); break;
        case etlstbox:  handled = graphoCB->evlstbox(er->lstbid, er->lstbsl); break;
        case etdrpbox:  handled = graphoCB->evdrpbox(er->drpbid, er->drpbsl); break;
        case etdrebox:  handled = graphoCB->evdrebox(er->drebid); break;
        case etsldpos:  handled = graphoCB->evsldpos(er->sldpid, er->sldpos); break;
        case ettabbar:  handled = graphoCB->evtabbar(er->tabid, er->tabsel); break;
        case etusize:   handled = graphoCB->evusize(); break;
        case etdsize:   handled = graphoCB->evdsize(); break;
        default: handled = 0; break;

    }

    er->handled = handled;
    if (!handled) (*graphoeh)(er);

}


/*******************************************************************************

The window and widget objects

The events of every window come down one chain, each record naming its
window. The wrapper hooks the chain once, keeps a registry of the
window objects that exist, and hands each event to the object that
holds its window -- the widget it names first, then the window's own
virtual, then the chain, so procedural code and objects mix freely,
window by window.

*******************************************************************************/

#define MAXWINOBJ 250 /* window objects at one time */

static struct { ami_long wid; window* wp; } winreg[MAXWINOBJ];
static ami_long    winregs;   /* entries in use */
static ami_long    winhooked; /* the chain hook is in */
static pevthan winpreveh; /* and what it displaced */

static window* fndwin(ami_long wid)

{

    ami_long i;

    for (i = 0; i < winregs; i++)
        if (winreg[i].wid == wid) return (winreg[i].wp);

    return (0);

}



static void regwin(ami_long wid, window* wp)

{

    if (fndwin(wid)) {

        fprintf(stderr, "graphics: a window object already holds window "
                        "%lld\n", AMI_LONG_CAST(wid));
        exit(1);

    }
    if (winregs >= MAXWINOBJ) {

        fprintf(stderr, "graphics: too many window objects\n");
        exit(1);

    }
    if (!winhooked) { /* the first object hooks the chain, once */

        eventsover(windowCB, &winpreveh);
        winhooked = 1;

    }
    winreg[winregs].wid = wid;
    winreg[winregs].wp = wp;
    winregs++;

}

static void unregwin(ami_long wid)

{

    ami_long i;

    for (i = 0; i < winregs; i++) if (winreg[i].wid == wid) {

        winreg[i] = winreg[winregs-1];
        winregs--;
        if (!winregs && winhooked) {

            /* The last window object out takes the hook back out. The
               chain is put back in the order it was built: whoever
               hooked after us has already left, and whoever hooked
               before us -- the widget system does -- verifies on its
               own way out that what it removes is its own. */
            pevthan tmp;

            eventsover(winpreveh, &tmp);
            winhooked = 0;

        }

        return;

    }

}

/* the chain hook: route by the window the record names */
void windowCB(evtrec* er)

{

    window* wp = fndwin(er->winid);
    ami_long    handled = 0;

    if (wp) {

        ami_long    wgid = 0;
        widget* wg;

        /* the widget the event names, if it names one */
        switch (er->etype) {

        case etbutton: wgid = er->butid; break;
        case etchkbox: wgid = er->ckbxid; break;
        case etradbut: wgid = er->radbid; break;
        case etsclull: wgid = er->sclulid; break;
        case etscldrl: wgid = er->scldrid; break;
        case etsclulp: wgid = er->sclupid; break;
        case etscldrp: wgid = er->scldpid; break;
        case etsclpos: wgid = er->sclpid; break;
        case etedtbox: wgid = er->edtbid; break;
        case etnumbox: wgid = er->numbid; break;
        case etlstbox: wgid = er->lstbid; break;
        case etdrpbox: wgid = er->drpbid; break;
        case etdrebox: wgid = er->drebid; break;
        case etsldpos: wgid = er->sldpid; break;
        case ettabbar: wgid = er->tabid; break;
        default: break;

        }
        if (wgid) {

            for (wg = wp->wlist; wg; wg = wg->next) if (wg->id() == wgid) break;
            if (wg) switch (er->etype) {

            case etbutton:  handled = wg->pressed(); break;
            case etchkbox:  handled = wg->clicked(); break;
            case etradbut:  handled = wg->clicked(); break;
            case etsclull:  handled = wg->upline(); break;
            case etscldrl:  handled = wg->downline(); break;
            case etsclulp:  handled = wg->uppage(); break;
            case etscldrp:  handled = wg->downpage(); break;
            case etsclpos:  handled = wg->moved(er->sclpos); break;
            case etedtbox:  handled = wg->done(); break;
            case etnumbox:  handled = wg->selected(er->numbsl); break;
            case etlstbox:  handled = wg->selected(er->lstbsl); break;
            case etdrpbox:  handled = wg->selected(er->drpbsl); break;
            case etdrebox:  handled = wg->done(); break;
            case etsldpos:  handled = wg->moved(er->sldpos); break;
            case ettabbar:  handled = wg->selected(er->tabsel); break;
            default: break;

            }

        }
        if (!handled) switch (er->etype) {

        case etchar: handled = wp->evchar(er->echar); break;
        case etup: handled = wp->evup(); break;
        case etdown: handled = wp->evdown(); break;
        case etleft: handled = wp->evleft(); break;
        case etright: handled = wp->evright(); break;
        case etleftw: handled = wp->evleftw(); break;
        case etrightw: handled = wp->evrightw(); break;
        case ethome: handled = wp->evhome(); break;
        case ethomes: handled = wp->evhomes(); break;
        case ethomel: handled = wp->evhomel(); break;
        case etend: handled = wp->evend(); break;
        case etends: handled = wp->evends(); break;
        case etendl: handled = wp->evendl(); break;
        case etscrl: handled = wp->evscrl(); break;
        case etscrr: handled = wp->evscrr(); break;
        case etscru: handled = wp->evscru(); break;
        case etscrd: handled = wp->evscrd(); break;
        case etpagd: handled = wp->evpagd(); break;
        case etpagu: handled = wp->evpagu(); break;
        case ettab: handled = wp->evtab(); break;
        case etenter: handled = wp->eventer(); break;
        case etinsert: handled = wp->evinsert(); break;
        case etinsertl: handled = wp->evinsertl(); break;
        case etinsertt: handled = wp->evinsertt(); break;
        case etdel: handled = wp->evdel(); break;
        case etdell: handled = wp->evdell(); break;
        case etdelcf: handled = wp->evdelcf(); break;
        case etdelcb: handled = wp->evdelcb(); break;
        case etcopy: handled = wp->evcopy(); break;
        case etcopyl: handled = wp->evcopyl(); break;
        case etcan: handled = wp->evcan(); break;
        case etstop: handled = wp->evstop(); break;
        case etcont: handled = wp->evcont(); break;
        case etprint: handled = wp->evprint(); break;
        case etprintb: handled = wp->evprintb(); break;
        case etprints: handled = wp->evprints(); break;
        case etfun: handled = wp->evfun(er->fkey); break;
        case etmenu: handled = wp->evmenu(); break;
        case ettim: handled = wp->evtim(er->timnum); break;
        case etresize: handled = wp->evresize(); break;
        case etfocus: handled = wp->evfocus(); break;
        case etnofocus: handled = wp->evnofocus(); break;
        case ethover: handled = wp->evhover(); break;
        case etnohover: handled = wp->evnohover(); break;
        case etterm: handled = wp->evterm(); break;
        case etframe: handled = wp->evframe(); break;
        case etredraw: handled = wp->evredraw(er->rsx, er->rsy, er->rex, er->rey); break;
        case etmin: handled = wp->evmin(); break;
        case etmax: handled = wp->evmax(); break;
        case etnorm: handled = wp->evnorm(); break;
        case etmenus: handled = wp->evmenus(er->menuid); break;
        case etbutton: handled = wp->evbutton(er->butid); break;
        case etchkbox: handled = wp->evchkbox(er->ckbxid); break;
        case etradbut: handled = wp->evradbut(er->radbid); break;
        case etsclull: handled = wp->evsclull(er->sclulid); break;
        case etscldrl: handled = wp->evscldrl(er->scldrid); break;
        case etsclulp: handled = wp->evsclulp(er->sclupid); break;
        case etscldrp: handled = wp->evscldrp(er->scldpid); break;
        case etsclpos: handled = wp->evsclpos(er->sclpid, er->sclpos); break;
        case etedtbox: handled = wp->evedtbox(er->edtbid); break;
        case etnumbox: handled = wp->evnumbox(er->numbid, er->numbsl); break;
        case etlstbox: handled = wp->evlstbox(er->lstbid, er->lstbsl); break;
        case etdrpbox: handled = wp->evdrpbox(er->drpbid, er->drpbsl); break;
        case etdrebox: handled = wp->evdrebox(er->drebid); break;
        case etsldpos: handled = wp->evsldpos(er->sldpid, er->sldpos); break;
        case ettabbar: handled = wp->evtabbar(er->tabid, er->tabsel); break;
        case etusize: handled = wp->evusize(); break;
        case etdsize: handled = wp->evdsize(); break;
        default: break;

        }

    }
    if (!handled) winpreveh(er);

}

/* methods */
window::window(void)

{

    /* attach to the main window, which is already open */
    wf = stdout;
    wid = 1;
    owned = 0;
    nextid = 0x2000; /* clear of the ids programs give by hand */
    wlist = 0;
    regwin(wid, this);

}

window::window(window* parent)

{

    FILE* win = stdin; /* share the event queue, as all windows do */

    wid = ami_getwinid();
    ami_openwin(&win, &wf, parent? parent->wf: NULL, wid);
    owned = 1;
    nextid = 0x2000;
    wlist = 0;
    regwin(wid, this);

}

window::~window(void)

{

    widget* wg;

    /* Disarm the widgets still standing: the window is going, and their
       destructors must not reach into it after it. In the subclass
       pattern the members die first and this list is already empty. */
    for (wg = wlist; wg; wg = wg->next) wg->dead = 1;
    unregwin(wid);
    if (owned) fclose(wf);

}

window::operator FILE*(void) { return wf; }
ami_long window::id(void) { return wid; }
ami_long window::newid(void) { return ++nextid; }

void window::cursor(ami_long x, ami_long y) { ami_cursor(wf, x, y); }
ami_long  window::maxx(void) { return ami_maxx(wf); }
ami_long  window::maxy(void) { return ami_maxy(wf); }
void window::home(void) { ami_home(wf); }
void window::del(void) { ami_del(wf); }
void window::up(void) { ami_up(wf); }
void window::down(void) { ami_down(wf); }
void window::left(void) { ami_left(wf); }
void window::right(void) { ami_right(wf); }
void window::blink(ami_long e) { ami_blink(wf, e); }
void window::reverse(ami_long e) { ami_reverse(wf, e); }
void window::underline(ami_long e) { ami_underline(wf, e); }
void window::superscript(ami_long e) { ami_superscript(wf, e); }
void window::subscript(ami_long e) { ami_subscript(wf, e); }
void window::italic(ami_long e) { ami_italic(wf, e); }
void window::bold(ami_long e) { ami_bold(wf, e); }
void window::strikeout(ami_long e) { ami_strikeout(wf, e); }
void window::standout(ami_long e) { ami_standout(wf, e); }
void window::fcolor(color c) { ami_fcolor(wf, (ami_color)c); }
void window::bcolor(color c) { ami_bcolor(wf, (ami_color)c); }
void window::autom(ami_long e) { ami_auto(wf, e); }
void window::curvis(ami_long e) { ami_curvis(wf, e); }
void window::scroll(ami_long x, ami_long y) { ami_scroll(wf, x, y); }
ami_long  window::curx(void) { return ami_curx(wf); }
ami_long  window::cury(void) { return ami_cury(wf); }
ami_long  window::curbnd(void) { return ami_curbnd(wf); }
void window::select(ami_long u, ami_long d) { ami_select(wf, u, d); }
void window::timer(ami_long i, ami_long t, ami_long r) { ami_timer(wf, i, t, r); }
void window::killtimer(ami_long i) { ami_killtimer(wf, i); }
ami_long  window::mouse(void) { return ami_mouse(wf); }
ami_long  window::mousebutton(ami_long m) { return ami_mousebutton(wf, m); }
ami_long  window::joystick(void) { return ami_joystick(wf); }
ami_long  window::joybutton(ami_long j) { return ami_joybutton(wf, j); }
ami_long  window::joyaxis(ami_long j) { return ami_joyaxis(wf, j); }
void window::settab(ami_long t) { ami_settab(wf, t); }
void window::restab(ami_long t) { ami_restab(wf, t); }
void window::clrtab(void) { ami_clrtab(wf); }
ami_long  window::funkey(void) { return ami_funkey(wf); }
void window::frametimer(ami_long e) { ami_frametimer(wf, e); }
void window::autohold(ami_long e) { ami_autohold(e); }
void window::wrtstr(char* s) { ami_wrtstr(wf, s); }
void window::wrtstrn(char* s, ami_long n) { ami_wrtstrn(wf, s, n); }
void window::sizbuf(ami_long x, ami_long y) { ami_sizbuf(wf, x, y); }
void window::title(char* ts) { ami_title(wf, ts); }
void window::sendevent(evtrec* er) { ami_sendevent(wf, (ami_evtptr)er); }
ami_long  window::maxxg(void) { return ami_maxxg(wf); }
ami_long  window::maxyg(void) { return ami_maxyg(wf); }
ami_long  window::curxg(void) { return ami_curxg(wf); }
ami_long  window::curyg(void) { return ami_curyg(wf); }
void window::line(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_line(wf, x1, y1, x2, y2); }
void window::linewidth(ami_long w) { ami_linewidth(wf, w); }
void window::rect(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_rect(wf, x1, y1, x2, y2); }
void window::frect(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_frect(wf, x1, y1, x2, y2); }
void window::rrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_rrect(wf, x1, y1, x2, y2, xs, ys); }
void window::frrect(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { ami_frrect(wf, x1, y1, x2, y2, xs, ys); }
void window::ellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_ellipse(wf, x1, y1, x2, y2); }
void window::fellipse(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_fellipse(wf, x1, y1, x2, y2); }
void window::arc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_arc(wf, x1, y1, x2, y2, sa, ea); }
void window::farc(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_farc(wf, x1, y1, x2, y2, sa, ea); }
void window::fchord(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { ami_fchord(wf, x1, y1, x2, y2, sa, ea); }
void window::ftriangle(ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3) { ami_ftriangle(wf, x1, y1, x2, y2, x3, y3); }
void window::cursorg(ami_long x, ami_long y) { ami_cursorg(wf, x, y); }
ami_long  window::baseline(void) { return ami_baseline(wf); }
void window::setpixel(ami_long x, ami_long y) { ami_setpixel(wf, x, y); }
void window::fover(void) { ami_fover(wf); }
void window::bover(void) { ami_bover(wf); }
void window::finvis(void) { ami_finvis(wf); }
void window::binvis(void) { ami_binvis(wf); }
void window::fxor(void) { ami_fxor(wf); }
void window::bxor(void) { ami_bxor(wf); }
void window::fand(void) { ami_fand(wf); }
void window::band(void) { ami_band(wf); }
void window::for_(void) { ami_for(wf); }
void window::bor(void) { ami_bor(wf); }
ami_long  window::chrsizx(void) { return ami_chrsizx(wf); }
ami_long  window::chrsizy(void) { return ami_chrsizy(wf); }
ami_long  window::fonts(void) { return ami_fonts(wf); }
void window::font(ami_long fc) { ami_font(wf, fc); }
void window::fontnam(ami_long fc, char* fns, ami_long fnsl) { ami_fontnam(wf, fc, fns, fnsl); }
void window::fontsiz(ami_long s) { ami_fontsiz(wf, s); }
void window::chrspcy(ami_long s) { ami_chrspcy(wf, s); }
void window::chrspcx(ami_long s) { ami_chrspcx(wf, s); }
ami_long  window::dpmx(void) { return ami_dpmx(wf); }
ami_long  window::dpmy(void) { return ami_dpmy(wf); }
ami_long  window::strsiz(const char* s) { return ami_strsiz(wf, s); }
ami_long  window::chrpos(const char* s, ami_long p) { return ami_chrpos(wf, s, p); }
void window::writejust(const char* s, ami_long n) { ami_writejust(wf, s, n); }
ami_long  window::justpos(const char* s, ami_long p, ami_long n) { return ami_justpos(wf, s, p, n); }
void window::condensed(ami_long e) { ami_condensed(wf, e); }
void window::extended(ami_long e) { ami_extended(wf, e); }
void window::xlight(ami_long e) { ami_xlight(wf, e); }
void window::light(ami_long e) { ami_light(wf, e); }
void window::xbold(ami_long e) { ami_xbold(wf, e); }
void window::hollow(ami_long e) { ami_hollow(wf, e); }
void window::raised(ami_long e) { ami_raised(wf, e); }
void window::settabg(ami_long t) { ami_settabg(wf, t); }
void window::restabg(ami_long t) { ami_restabg(wf, t); }
void window::fcolorg(ami_long r, ami_long g, ami_long b) { ami_fcolorg(wf, r, g, b); }
void window::fcolorc(ami_long r, ami_long g, ami_long b) { ami_fcolorc(wf, r, g, b); }
void window::bcolorg(ami_long r, ami_long g, ami_long b) { ami_bcolorg(wf, r, g, b); }
void window::bcolorc(ami_long r, ami_long g, ami_long b) { ami_bcolorc(wf, r, g, b); }
void window::loadpict(ami_long p, char* fn) { ami_loadpict(wf, p, fn); }
ami_long  window::pictsizx(ami_long p) { return ami_pictsizx(wf, p); }
ami_long  window::pictsizy(ami_long p) { return ami_pictsizy(wf, p); }
void window::picture(ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { ami_picture(wf, p, x1, y1, x2, y2); }
void window::delpict(ami_long p) { ami_delpict(wf, p); }
void window::scrollg(ami_long x, ami_long y) { ami_scrollg(wf, x, y); }
void window::blockcopyg(ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2, ami_long sy2,
                        ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2)
    { ami_blockcopyg(wf, s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2); }
void window::path(ami_long a) { ami_path(wf, a); }
void window::buffer(ami_long e) { ami_buffer(wf, e); }
void window::sizbufg(ami_long x, ami_long y) { ami_sizbufg(wf, x, y); }
void window::getsiz(ami_long* x, ami_long* y) { ami_getsiz(wf, x, y); }
void window::getsizg(ami_long* x, ami_long* y) { ami_getsizg(wf, x, y); }
void window::setsiz(ami_long x, ami_long y) { ami_setsiz(wf, x, y); }
void window::setsizg(ami_long x, ami_long y) { ami_setsizg(wf, x, y); }
void window::setpos(ami_long x, ami_long y) { ami_setpos(wf, x, y); }
void window::setposg(ami_long x, ami_long y) { ami_setposg(wf, x, y); }
void window::scnsiz(ami_long* x, ami_long* y) { ami_scnsiz(wf, x, y); }
void window::scnsizg(ami_long* x, ami_long* y) { ami_scnsizg(wf, x, y); }
void window::scncen(ami_long* x, ami_long* y) { ami_scncen(wf, x, y); }
void window::scnceng(ami_long* x, ami_long* y) { ami_scnceng(wf, x, y); }
void window::winclient(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclient(wf, cx, cy, wx, wy, (ami_winmodset)ms); }
void window::winclientg(ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, winmodset ms) { ami_winclientg(wf, cx, cy, wx, wy, (ami_winmodset)ms); }
void window::front(void) { ami_front(wf); }
void window::back(void) { ami_back(wf); }
void window::frame(ami_long e) { ami_frame(wf, e); }
void window::sizable(ami_long e) { ami_sizable(wf, e); }
void window::sysbar(ami_long e) { ami_sysbar(wf, e); }
void window::autobuf(ami_long e) { ami_autobuf(wf, e); }
void window::menu(menuptr m) { ami_menu(wf, (ami_menuptr)m); }
void window::menuena(ami_long id, ami_long onoff) { ami_menuena(wf, id, onoff); }
void window::menusel(ami_long id, ami_long select) { ami_menusel(wf, id, select); }
void window::focus(void) { ami_focus(wf); }
ami_long  window::getwigid(void) { return ami_getwigid(wf); }
void window::killwidget(ami_long id) { ami_killwidget(wf, id); }
void window::selectwidget(ami_long id, ami_long e) { ami_selectwidget(wf, id, e); }
void window::enablewidget(ami_long id, ami_long e) { ami_enablewidget(wf, id, e); }
void window::getwidgettext(ami_long id, char* s, ami_long sl) { ami_getwidgettext(wf, id, s, sl); }
void window::putwidgettext(ami_long id, char* s) { ami_putwidgettext(wf, id, s); }
void window::sizwidget(ami_long id, ami_long x, ami_long y) { ami_sizwidget(wf, id, x, y); }
void window::sizwidgetg(ami_long id, ami_long x, ami_long y) { ami_sizwidgetg(wf, id, x, y); }
void window::poswidget(ami_long id, ami_long x, ami_long y) { ami_poswidget(wf, id, x, y); }
void window::poswidgetg(ami_long id, ami_long x, ami_long y) { ami_poswidgetg(wf, id, x, y); }
void window::backwidget(ami_long id) { ami_backwidget(wf, id); }
void window::frontwidget(ami_long id) { ami_frontwidget(wf, id); }
void window::focuswidget(ami_long id) { ami_focuswidget(wf, id); }
void window::buttonsiz(char* s, ami_long* w, ami_long* h) { ami_buttonsiz(wf, s, w, h); }
void window::buttonsizg(char* s, ami_long* w, ami_long* h) { ami_buttonsizg(wf, s, w, h); }
void window::checkboxsiz(char* s, ami_long* w, ami_long* h) { ami_checkboxsiz(wf, s, w, h); }
void window::checkboxsizg(char* s, ami_long* w, ami_long* h) { ami_checkboxsizg(wf, s, w, h); }
void window::radiobuttonsiz(char* s, ami_long* w, ami_long* h) { ami_radiobuttonsiz(wf, s, w, h); }
void window::radiobuttonsizg(char* s, ami_long* w, ami_long* h) { ami_radiobuttonsizg(wf, s, w, h); }
void window::groupsiz(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsiz(wf, s, cw, ch, w, h, ox, oy); }
void window::groupsizg(char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_groupsizg(wf, s, cw, ch, w, h, ox, oy); }
void window::scrollvertsiz(ami_long* w, ami_long* h) { ami_scrollvertsiz(wf, w, h); }
void window::scrollvertsizg(ami_long* w, ami_long* h) { ami_scrollvertsizg(wf, w, h); }
void window::scrollhorizsiz(ami_long* w, ami_long* h) { ami_scrollhorizsiz(wf, w, h); }
void window::scrollhorizsizg(ami_long* w, ami_long* h) { ami_scrollhorizsizg(wf, w, h); }
void window::scrollpos(ami_long id, ami_long r) { ami_scrollpos(wf, id, r); }
void window::scrollsiz(ami_long id, ami_long r) { ami_scrollsiz(wf, id, r); }
void window::numselboxsiz(ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsiz(wf, l, u, w, h); }
void window::numselboxsizg(ami_long l, ami_long u, ami_long* w, ami_long* h) { ami_numselboxsizg(wf, l, u, w, h); }
void window::editboxsiz(char* s, ami_long* w, ami_long* h) { ami_editboxsiz(wf, s, w, h); }
void window::editboxsizg(char* s, ami_long* w, ami_long* h) { ami_editboxsizg(wf, s, w, h); }
void window::progbarsiz(ami_long* w, ami_long* h) { ami_progbarsiz(wf, w, h); }
void window::progbarsizg(ami_long* w, ami_long* h) { ami_progbarsizg(wf, w, h); }
void window::progbarpos(ami_long id, ami_long pos) { ami_progbarpos(wf, id, pos); }
void window::listboxsiz(strptr sp, ami_long* w, ami_long* h) { ami_listboxsiz(wf, (ami_strptr)sp, w, h); }
void window::listboxsizg(strptr sp, ami_long* w, ami_long* h) { ami_listboxsizg(wf, (ami_strptr)sp, w, h); }
void window::dropboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsiz(wf, (ami_strptr)sp, cw, ch, ow, oh); }
void window::dropboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropboxsizg(wf, (ami_strptr)sp, cw, ch, ow, oh); }
void window::dropeditboxsiz(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsiz(wf, (ami_strptr)sp, cw, ch, ow, oh); }
void window::dropeditboxsizg(strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { ami_dropeditboxsizg(wf, (ami_strptr)sp, cw, ch, ow, oh); }
void window::slidehorizsiz(ami_long* w, ami_long* h) { ami_slidehorizsiz(wf, w, h); }
void window::slidehorizsizg(ami_long* w, ami_long* h) { ami_slidehorizsizg(wf, w, h); }
void window::slidevertsiz(ami_long* w, ami_long* h) { ami_slidevertsiz(wf, w, h); }
void window::slidevertsizg(ami_long* w, ami_long* h) { ami_slidevertsizg(wf, w, h); }
void window::tabbarsiz(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsiz(wf, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void window::tabbarsizg(strptr sp, tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { ami_tabbarsizg(wf, (ami_strptr)sp, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void window::tabbarclient(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclient(wf, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void window::tabbarclientg(tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { ami_tabbarclientg(wf, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void window::tabsel(ami_long id, ami_long tn) { ami_tabsel(wf, id, tn); }
void window::queryfont(ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br,
                      ami_long* bg, ami_long* bb, qfteffects* effect) { ami_queryfont(wf, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }

/* the virtuals, which do nothing until overridden */
ami_long window::evchar(char c) { return 0; }
ami_long window::evup(void) { return 0; }
ami_long window::evdown(void) { return 0; }
ami_long window::evleft(void) { return 0; }
ami_long window::evright(void) { return 0; }
ami_long window::evleftw(void) { return 0; }
ami_long window::evrightw(void) { return 0; }
ami_long window::evhome(void) { return 0; }
ami_long window::evhomes(void) { return 0; }
ami_long window::evhomel(void) { return 0; }
ami_long window::evend(void) { return 0; }
ami_long window::evends(void) { return 0; }
ami_long window::evendl(void) { return 0; }
ami_long window::evscrl(void) { return 0; }
ami_long window::evscrr(void) { return 0; }
ami_long window::evscru(void) { return 0; }
ami_long window::evscrd(void) { return 0; }
ami_long window::evpagd(void) { return 0; }
ami_long window::evpagu(void) { return 0; }
ami_long window::evtab(void) { return 0; }
ami_long window::eventer(void) { return 0; }
ami_long window::evinsert(void) { return 0; }
ami_long window::evinsertl(void) { return 0; }
ami_long window::evinsertt(void) { return 0; }
ami_long window::evdel(void) { return 0; }
ami_long window::evdell(void) { return 0; }
ami_long window::evdelcf(void) { return 0; }
ami_long window::evdelcb(void) { return 0; }
ami_long window::evcopy(void) { return 0; }
ami_long window::evcopyl(void) { return 0; }
ami_long window::evcan(void) { return 0; }
ami_long window::evstop(void) { return 0; }
ami_long window::evcont(void) { return 0; }
ami_long window::evprint(void) { return 0; }
ami_long window::evprintb(void) { return 0; }
ami_long window::evprints(void) { return 0; }
ami_long window::evfun(ami_long k) { return 0; }
ami_long window::evmenu(void) { return 0; }
ami_long window::evmouba(ami_long m, ami_long b) { return 0; }
ami_long window::evmoubd(ami_long m, ami_long b) { return 0; }
ami_long window::evmoumov(ami_long m, ami_long x, ami_long y) { return 0; }
ami_long window::evtim(ami_long t) { return 0; }
ami_long window::evjoyba(ami_long j, ami_long b) { return 0; }
ami_long window::evjoybd(ami_long j, ami_long b) { return 0; }
ami_long window::evjoymov(ami_long j, ami_long x, ami_long y, ami_long z) { return 0; }
ami_long window::evresize(void) { return 0; }
ami_long window::evfocus(void) { return 0; }
ami_long window::evnofocus(void) { return 0; }
ami_long window::evhover(void) { return 0; }
ami_long window::evnohover(void) { return 0; }
ami_long window::evterm(void) { return 0; }
ami_long window::evframe(void) { return 0; }
ami_long window::evmoumovg(ami_long m, ami_long x, ami_long y) { return 0; }
ami_long window::evredraw(ami_long x1, ami_long y1, ami_long x2, ami_long y2) { return 0; }
ami_long window::evmin(void) { return 0; }
ami_long window::evmax(void) { return 0; }
ami_long window::evnorm(void) { return 0; }
ami_long window::evmenus(ami_long id) { return 0; }
ami_long window::evbutton(ami_long id) { return 0; }
ami_long window::evchkbox(ami_long id) { return 0; }
ami_long window::evradbut(ami_long id) { return 0; }
ami_long window::evsclull(ami_long id) { return 0; }
ami_long window::evscldrl(ami_long id) { return 0; }
ami_long window::evsclulp(ami_long id) { return 0; }
ami_long window::evscldrp(ami_long id) { return 0; }
ami_long window::evsclpos(ami_long id, ami_long pos) { return 0; }
ami_long window::evedtbox(ami_long id) { return 0; }
ami_long window::evnumbox(ami_long id, ami_long val) { return 0; }
ami_long window::evlstbox(ami_long id, ami_long sel) { return 0; }
ami_long window::evdrpbox(ami_long id, ami_long sel) { return 0; }
ami_long window::evdrebox(ami_long id) { return 0; }
ami_long window::evsldpos(ami_long id, ami_long pos) { return 0; }
ami_long window::evtabbar(ami_long id, ami_long sel) { return 0; }
ami_long window::evusize(void) { return 0; }
ami_long window::evdsize(void) { return 0; }

widget::widget(window& wo, ami_long id): w(wo)

{

    dead = 0;
    wid = id? id: wo.newid();
    next = wo.wlist; /* onto the window's list */
    wo.wlist = this;

}

widget::~widget(void)

{

    widget** p;

    if (!dead) {

        ami_killwidget(w.wf, wid);
        /* off the window's list */
        for (p = &w.wlist; *p; p = &(*p)->next) if (*p == this) {

            *p = next;
            break;

        }

    }

}

ami_long widget::id(void) { return wid; }
void widget::kill(void)
    { if (!dead) { ami_killwidget(w.wf, wid); dead = 1; } }
void widget::select(ami_long e) { ami_selectwidget(w.wf, wid, e); }
void widget::enable(ami_long e) { ami_enablewidget(w.wf, wid, e); }
void widget::gettext(char* s, ami_long sl) { ami_getwidgettext(w.wf, wid, s, sl); }
void widget::puttext(const char* s) { ami_putwidgettext(w.wf, wid, (char*)s); }
void widget::pos(ami_long x, ami_long y) { ami_poswidgetg(w.wf, wid, x, y); }
void widget::siz(ami_long x, ami_long y) { ami_sizwidgetg(w.wf, wid, x, y); }
void widget::back(void) { ami_backwidget(w.wf, wid); }
void widget::front(void) { ami_frontwidget(w.wf, wid); }
void widget::focus(void) { ami_focuswidget(w.wf, wid); }

/* the widget virtuals, which do nothing until overridden */
ami_long widget::pressed(void) { return 0; }
ami_long widget::clicked(void) { return 0; }
ami_long widget::done(void) { return 0; }
ami_long widget::selected(ami_long v) { return 0; }
ami_long widget::moved(ami_long v) { return 0; }
ami_long widget::upline(void) { return 0; }
ami_long widget::downline(void) { return 0; }
ami_long widget::uppage(void) { return 0; }
ami_long widget::downpage(void) { return 0; }

/* the typed widgets: constructing one makes it */
button::button(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s, ami_long id): widget(wo, id)

{

    ami_buttong(wo, x1, y1, x2, y2, (char*)s, wid);

}

checkbox::checkbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s, ami_long id): widget(wo, id)

{

    ami_checkboxg(wo, x1, y1, x2, y2, (char*)s, wid);

}

radiobutton::radiobutton(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s, ami_long id): widget(wo, id)

{

    ami_radiobuttong(wo, x1, y1, x2, y2, (char*)s, wid);

}

group::group(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, const char* s, ami_long id): widget(wo, id)

{

    ami_groupg(wo, x1, y1, x2, y2, (char*)s, wid);

}

background::background(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id): widget(wo, id)

{

    ami_backgroundg(wo, x1, y1, x2, y2, wid);

}

scrollvert::scrollvert(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id): widget(wo, id)

{

    ami_scrollvertg(wo, x1, y1, x2, y2, wid);

}

scrollhoriz::scrollhoriz(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id): widget(wo, id)

{

    ami_scrollhorizg(wo, x1, y1, x2, y2, wid);

}

numselbox::numselbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id): widget(wo, id)

{

    ami_numselboxg(wo, x1, y1, x2, y2, l, u, wid);

}

editbox::editbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id): widget(wo, id)

{

    ami_editboxg(wo, x1, y1, x2, y2, wid);

}

progbar::progbar(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id): widget(wo, id)

{

    ami_progbarg(wo, x1, y1, x2, y2, wid);

}

listbox::listbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id): widget(wo, id)

{

    ami_listboxg(wo, x1, y1, x2, y2, (ami_strptr)sp, wid);

}

dropbox::dropbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id): widget(wo, id)

{

    ami_dropboxg(wo, x1, y1, x2, y2, (ami_strptr)sp, wid);

}

dropeditbox::dropeditbox(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, ami_long id): widget(wo, id)

{

    ami_dropeditboxg(wo, x1, y1, x2, y2, (ami_strptr)sp, wid);

}

slidehoriz::slidehoriz(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id): widget(wo, id)

{

    ami_slidehorizg(wo, x1, y1, x2, y2, mark, wid);

}

slidevert::slidevert(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id): widget(wo, id)

{

    ami_slidevertg(wo, x1, y1, x2, y2, mark, wid);

}

tabbar::tabbar(window& wo, ami_long x1, ami_long y1, ami_long x2, ami_long y2, strptr sp, tabori tor, ami_long id): widget(wo, id)

{

    ami_tabbarg(wo, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, wid);

}

} /* namespace graphics */
