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

#include <graphics.h>

}

#include "graphics.hpp"

namespace graphics {

/* hook for sending events back to methods */
graph* graphoCB;
pevthan graphoeh;

/* procedures and functions */

/* text */
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
void select(FILE* f, int u, int d) { ami_select(f, u, d); }
void select(int u, int d) { ami_select(stdout, u, d); }
void event(FILE* f, evtrec* er) { ami_event(f, (ami_evtptr)er); }
void event(evtrec* er) { ami_event(stdin, (ami_evtptr)er); }
void timer(FILE* f, int i, long t, int r) { ami_timer(f, i, t, r); }
void timer(int i, long t, int r) { ami_timer(stdout, i, t, r); }
void killtimer(FILE* f, int i) { ami_killtimer(f, i); }
void killtimer(int i) { ami_killtimer(stdout, i); }
int  mouse(FILE* f) { return ami_mouse(f); }
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
void wrtstr(FILE* f, char* s) { ami_wrtstr(f, s); }
void wrtstr(char* s) { ami_wrtstr(stdout, s); }
void wrtstrn(FILE* f, char* s, int n) { ami_wrtstrn(f, s, n); }
void wrtstrn(char* s, int n) { ami_wrtstrn(stdout, s, n); }
void sizbuf(FILE* f, int x, int y) { ami_sizbuf(f, x, y); }
void sizbuf(int x, int y) { ami_sizbuf(stdout, x, y); }
void title(FILE* f, char* ts) { ami_title(f, ts); }
void title(char* ts) { ami_title(stdout, ts); }
void eventover(evtcod e, pevthan eh, pevthan* oeh) { ami_eventover((ami_evtcod)e, (ami_pevthan)eh, (ami_pevthan*)oeh); }
void eventsover(pevthan eh, pevthan* oeh) { ami_eventsover((ami_pevthan)eh, (ami_pevthan*)oeh); }
void sendevent(FILE* f, evtrec* er) { ami_sendevent(f, (ami_evtptr)er); }
void sendevent(evtrec* er) { ami_sendevent(stdout, (ami_evtptr)er); }

/* graphical */
int  maxxg(FILE* f) { return ami_maxxg(f); }
int  maxxg(void) { return ami_maxxg(stdout); }
int  maxyg(FILE* f) { return ami_maxyg(f); }
int  maxyg(void) { return ami_maxyg(stdout); }
int  curxg(FILE* f) { return ami_curxg(f); }
int  curxg(void) { return ami_curxg(stdout); }
int  curyg(FILE* f) { return ami_curyg(f); }
int  curyg(void) { return ami_curyg(stdout); }
void line(FILE* f, int x1, int y1, int x2, int y2) { ami_line(f, x1, y1, x2, y2); }
void line(int x1, int y1, int x2, int y2) { ami_line(stdout, x1, y1, x2, y2); }
void linewidth(FILE* f, int w) { ami_linewidth(f, w); }
void linewidth(int w) { ami_linewidth(stdout, w); }
void rect(FILE* f, int x1, int y1, int x2, int y2) { ami_rect(f, x1, y1, x2, y2); }
void rect(int x1, int y1, int x2, int y2) { ami_rect(stdout, x1, y1, x2, y2); }
void frect(FILE* f, int x1, int y1, int x2, int y2) { ami_frect(f, x1, y1, x2, y2); }
void frect(int x1, int y1, int x2, int y2) { ami_frect(stdout, x1, y1, x2, y2); }
void rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys) { ami_rrect(f, x1, y1, x2, y2, xs, ys); }
void rrect(int x1, int y1, int x2, int y2, int xs, int ys) { ami_rrect(stdout, x1, y1, x2, y2, xs, ys); }
void frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys) { ami_frrect(f, x1, y1, x2, y2, xs, ys); }
void frrect(int x1, int y1, int x2, int y2, int xs, int ys) { ami_frrect(stdout, x1, y1, x2, y2, xs, ys); }
void ellipse(FILE* f, int x1, int y1, int x2, int y2) { ami_ellipse(f, x1, y1, x2, y2); }
void ellipse(int x1, int y1, int x2, int y2) { ami_ellipse(stdout, x1, y1, x2, y2); }
void fellipse(FILE* f, int x1, int y1, int x2, int y2) { ami_fellipse(f, x1, y1, x2, y2); }
void fellipse(int x1, int y1, int x2, int y2) { ami_fellipse(stdout, x1, y1, x2, y2); }
void arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea) { ami_arc(f, x1, y1, x2, y2, sa, ea); }
void arc(int x1, int y1, int x2, int y2, int sa, int ea) { ami_arc(stdout, x1, y1, x2, y2, sa, ea); }
void farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea) { ami_farc(f, x1, y1, x2, y2, sa, ea); }
void farc(int x1, int y1, int x2, int y2, int sa, int ea) { ami_farc(stdout, x1, y1, x2, y2, sa, ea); }
void fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea) { ami_fchord(f, x1, y1, x2, y2, sa, ea); }
void fchord(int x1, int y1, int x2, int y2, int sa, int ea) { ami_fchord(stdout, x1, y1, x2, y2, sa, ea); }
void ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3) { ami_ftriangle(f, x1, y1, x2, y2, x3, y3); }
void ftriangle(int x1, int y1, int x2, int y2, int x3, int y3) { ami_ftriangle(stdout, x1, y1, x2, y2, x3, y3); }
void cursorg(FILE* f, int x, int y) { ami_cursorg(f, x, y); }
void cursorg(int x, int y) { ami_cursorg(stdout, x, y); }
int  baseline(FILE* f) { return ami_baseline(f); }
int  baseline(void) { return ami_baseline(stdout); }
void setpixel(FILE* f, int x, int y) { ami_setpixel(f, x, y); }
void setpixel(int x, int y) { ami_setpixel(stdout, x, y); }
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
int  chrsizx(FILE* f) { return ami_chrsizx(f); }
int  chrsizx(void) { return ami_chrsizx(stdout); }
int  chrsizy(FILE* f) { return ami_chrsizy(f); }
int  chrsizy(void) { return ami_chrsizy(stdout); }
int  fonts(FILE* f) { return ami_fonts(f); }
int  fonts(void) { return ami_fonts(stdout); }
void font(FILE* f, int fc) { ami_font(f, fc); }
void font(int fc) { ami_font(stdout, fc); }
void fontnam(FILE* f, int fc, char* fns, int fnsl) { ami_fontnam(f, fc, fns, fnsl); }
void fontnam(int fc, char* fns, int fnsl) { ami_fontnam(stdout, fc, fns, fnsl); }
void fontsiz(FILE* f, int s) { ami_fontsiz(f, s); }
void fontsiz(int s) { ami_fontsiz(stdout, s); }
void chrspcy(FILE* f, int s) { ami_chrspcy(f, s); }
void chrspcy(int s) { ami_chrspcy(stdout, s); }
void chrspcx(FILE* f, int s) { ami_chrspcx(f, s); }
void chrspcx(int s) { ami_chrspcx(stdout, s); }
int  dpmx(FILE* f) { return ami_dpmx(f); }
int  dpmx(void) { return ami_dpmx(stdout); }
int  dpmy(FILE* f) { return ami_dpmy(f); }
int  dpmy(void) { return ami_dpmy(stdout); }
int  strsiz(FILE* f, const char* s) { return ami_strsiz(f, s); }
int  strsiz(const char* s) { return ami_strsiz(stdout, s); }
int  chrpos(FILE* f, const char* s, int p) { return ami_chrpos(f, s, p); }
int  chrpos(const char* s, int p) { return ami_chrpos(stdout, s, p); }
void writejust(FILE* f, const char* s, int n) { ami_writejust(f, s, n); }
void writejust(const char* s, int n) { ami_writejust(stdout, s, n); }
int  justpos(FILE* f, const char* s, int p, int n) { return ami_justpos(f, s, p, n); }
int  justpos(const char* s, int p, int n) { return ami_justpos(stdout, s, p, n); }
void condensed(FILE* f, int e) { ami_condensed(f, e); }
void condensed(int e) { ami_condensed(stdout, e); }
void extended(FILE* f, int e) { ami_extended(f, e); }
void extended(int e) { ami_extended(stdout, e); }
void xlight(FILE* f, int e) { ami_xlight(f, e); }
void xlight(int e) { ami_xlight(stdout, e); }
void light(FILE* f, int e) { ami_light(f, e); }
void light(int e) { ami_light(stdout, e); }
void xbold(FILE* f, int e) { ami_xbold(f, e); }
void xbold(int e) { ami_xbold(stdout, e); }
void hollow(FILE* f, int e) { ami_hollow(f, e); }
void hollow(int e) { ami_hollow(stdout, e); }
void raised(FILE* f, int e) { ami_raised(f, e); }
void raised(int e) { ami_raised(stdout, e); }
void settabg(FILE* f, int t) { ami_settabg(f, t); }
void settabg(int t) { ami_settabg(stdout, t); }
void restabg(FILE* f, int t) { ami_restabg(f, t); }
void restabg(int t) { ami_restabg(stdout, t); }
void fcolorg(FILE* f, int r, int g, int b) { ami_fcolorg(f, r, g, b); }
void fcolorg(int r, int g, int b) { ami_fcolorg(stdout, r, g, b); }
void fcolorc(FILE* f, int r, int g, int b) { ami_fcolorc(f, r, g, b); }
void fcolorc(int r, int g, int b) { ami_fcolorc(stdout, r, g, b); }
void bcolorg(FILE* f, int r, int g, int b) { ami_bcolorg(f, r, g, b); }
void bcolorg(int r, int g, int b) { ami_bcolorg(stdout, r, g, b); }
void bcolorc(FILE* f, int r, int g, int b) { ami_bcolorc(f, r, g, b); }
void bcolorc(int r, int g, int b) { ami_bcolorc(stdout, r, g, b); }
void loadpict(FILE* f, int p, char* fn) { ami_loadpict(f, p, fn); }
void loadpict(int p, char* fn) { ami_loadpict(stdout, p, fn); }
int  pictsizx(FILE* f, int p) { return ami_pictsizx(f, p); }
int  pictsizx(int p) { return ami_pictsizx(stdout, p); }
int  pictsizy(FILE* f, int p) { return ami_pictsizy(f, p); }
int  pictsizy(int p) { return ami_pictsizy(stdout, p); }
void picture(FILE* f, int p, int x1, int y1, int x2, int y2) { ami_picture(f, p, x1, y1, x2, y2); }
void picture(int p, int x1, int y1, int x2, int y2) { ami_picture(stdout, p, x1, y1, x2, y2); }
void delpict(FILE* f, int p) { ami_delpict(f, p); }
void delpict(int p) { ami_delpict(stdout, p); }
void scrollg(FILE* f, int x, int y) { ami_scrollg(f, x, y); }
void scrollg(int x, int y) { ami_scrollg(stdout, x, y); }
void path(FILE* f, int a) { ami_path(f, a); }
void path(int a) { ami_path(stdout, a); }

/* window management */
void openwin(FILE** infile, FILE** outfile, FILE* parent, int wid) { ami_openwin(infile, outfile, parent, wid); }
void buffer(FILE* f, int e) { ami_buffer(f, e); }
void buffer(int e) { ami_buffer(stdout, e); }
void sizbufg(FILE* f, int x, int y) { ami_sizbufg(f, x, y); }
void sizbufg(int x, int y) { ami_sizbufg(stdout, x, y); }
void getsiz(FILE* f, int* x, int* y) { ami_getsiz(f, x, y); }
void getsiz(int* x, int* y) { ami_getsiz(stdout, x, y); }
void getsizg(FILE* f, int* x, int* y) { ami_getsizg(f, x, y); }
void getsizg(int* x, int* y) { ami_getsizg(stdout, x, y); }
void setsiz(FILE* f, int x, int y) { ami_setsiz(f, x, y); }
void setsiz(int x, int y) { ami_setsiz(stdout, x, y); }
void setsizg(FILE* f, int x, int y) { ami_setsizg(f, x, y); }
void setsizg(int x, int y) { ami_setsizg(stdout, x, y); }
void setpos(FILE* f, int x, int y) { ami_setpos(f, x, y); }
void setpos(int x, int y) { ami_setpos(stdout, x, y); }
void setposg(FILE* f, int x, int y) { ami_setposg(f, x, y); }
void setposg(int x, int y) { ami_setposg(stdout, x, y); }
void scnsiz(FILE* f, int* x, int* y) { ami_scnsiz(f, x, y); }
void scnsiz(int* x, int* y) { ami_scnsiz(stdout, x, y); }
void scnsizg(FILE* f, int* x, int* y) { ami_scnsizg(f, x, y); }
void scnsizg(int* x, int* y) { ami_scnsizg(stdout, x, y); }
void scncen(FILE* f, int* x, int* y) { ami_scncen(f, x, y); }
void scncen(int* x, int* y) { ami_scncen(stdout, x, y); }
void scnceng(FILE* f, int* x, int* y) { ami_scnceng(f, x, y); }
void scnceng(int* x, int* y) { ami_scnceng(stdout, x, y); }
void winclient(FILE* f, int cx, int cy, int* wx, int* wy, winmodset ms) { ami_winclient(f, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclient(int cx, int cy, int* wx, int* wy, winmodset ms) { ami_winclient(stdout, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclientg(FILE* f, int cx, int cy, int* wx, int* wy, winmodset ms) { ami_winclientg(f, cx, cy, wx, wy, (ami_winmodset)ms); }
void winclientg(int cx, int cy, int* wx, int* wy, winmodset ms) { ami_winclientg(stdout, cx, cy, wx, wy, (ami_winmodset)ms); }
void front(FILE* f) { ami_front(f); }
void front(void) { ami_front(stdout); }
void back(FILE* f) { ami_back(f); }
void back(void) { ami_back(stdout); }
void frame(FILE* f, int e) { ami_frame(f, e); }
void frame(int e) { ami_frame(stdout, e); }
void sizable(FILE* f, int e) { ami_sizable(f, e); }
void sizable(int e) { ami_sizable(stdout, e); }
void sysbar(FILE* f, int e) { ami_sysbar(f, e); }
void sysbar(int e) { ami_sysbar(stdout, e); }
void menu(FILE* f, menuptr m) { ami_menu(f, (ami_menuptr)m); }
void menu(menuptr m) { ami_menu(stdout, (ami_menuptr)m); }
void menuena(FILE* f, int id, int onoff) { ami_menuena(f, id, onoff); }
void menuena(int id, int onoff) { ami_menuena(stdout, id, onoff); }
void menusel(FILE* f, int id, int select) { ami_menusel(f, id, select); }
void menusel(int id, int select) { ami_menusel(stdout, id, select); }
void stdmenu(stdmenusel sms, menuptr* sm, menuptr pm) { ami_stdmenu(sms, (ami_menuptr*)sm, (ami_menuptr)pm); }
int  getwinid(void) { return ami_getwinid(); }
void focus(FILE* f) { ami_focus(f); }
void focus(void) { ami_focus(stdout); }

/* widgets/controls */
int  getwigid(FILE* f) { return ami_getwigid(f); }
int  getwigid(void) { return ami_getwigid(stdout); }
void killwidget(FILE* f, int id) { ami_killwidget(f, id); }
void killwidget(int id) { ami_killwidget(stdout, id); }
void selectwidget(FILE* f, int id, int e) { ami_selectwidget(f, id, e); }
void selectwidget(int id, int e) { ami_selectwidget(stdout, id, e); }
void enablewidget(FILE* f, int id, int e) { ami_enablewidget(f, id, e); }
void enablewidget(int id, int e) { ami_enablewidget(stdout, id, e); }
void getwidgettext(FILE* f, int id, char* s, int sl) { ami_getwidgettext(f, id, s, sl); }
void getwidgettext(int id, char* s, int sl) { ami_getwidgettext(stdout, id, s, sl); }
void putwidgettext(FILE* f, int id, char* s) { ami_putwidgettext(f, id, s); }
void putwidgettext(int id, char* s) { ami_putwidgettext(stdout, id, s); }
void sizwidget(FILE* f, int id, int x, int y) { ami_sizwidget(f, id, x, y); }
void sizwidget(int id, int x, int y) { ami_sizwidget(stdout, id, x, y); }
void sizwidgetg(FILE* f, int id, int x, int y) { ami_sizwidgetg(f, id, x, y); }
void sizwidgetg(int id, int x, int y) { ami_sizwidgetg(stdout, id, x, y); }
void poswidget(FILE* f, int id, int x, int y) { ami_poswidget(f, id, x, y); }
void poswidget(int id, int x, int y) { ami_poswidget(stdout, id, x, y); }
void poswidgetg(FILE* f, int id, int x, int y) { ami_poswidgetg(f, id, x, y); }
void poswidgetg(int id, int x, int y) { ami_poswidgetg(stdout, id, x, y); }
void backwidget(FILE* f, int id) { ami_backwidget(f, id); }
void backwidget(int id) { ami_backwidget(stdout, id); }
void frontwidget(FILE* f, int id) { ami_frontwidget(f, id); }
void frontwidget(int id) { ami_frontwidget(stdout, id); }
void focuswidget(FILE* f, int id) { ami_focuswidget(f, id); }
void focuswidget(int id) { ami_focuswidget(stdout, id); }
void buttonsiz(FILE* f, char* s, int* w, int* h) { ami_buttonsiz(f, s, w, h); }
void buttonsiz(char* s, int* w, int* h) { ami_buttonsiz(stdout, s, w, h); }
void buttonsizg(FILE* f, char* s, int* w, int* h) { ami_buttonsizg(f, s, w, h); }
void buttonsizg(char* s, int* w, int* h) { ami_buttonsizg(stdout, s, w, h); }
void button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_button(f, x1, y1, x2, y2, s, id); }
void button(int x1, int y1, int x2, int y2, char* s, int id) { ami_button(stdout, x1, y1, x2, y2, s, id); }
void buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_buttong(f, x1, y1, x2, y2, s, id); }
void buttong(int x1, int y1, int x2, int y2, char* s, int id) { ami_buttong(stdout, x1, y1, x2, y2, s, id); }
void checkboxsiz(FILE* f, char* s, int* w, int* h) { ami_checkboxsiz(f, s, w, h); }
void checkboxsiz(char* s, int* w, int* h) { ami_checkboxsiz(stdout, s, w, h); }
void checkboxsizg(FILE* f, char* s, int* w, int* h) { ami_checkboxsizg(f, s, w, h); }
void checkboxsizg(char* s, int* w, int* h) { ami_checkboxsizg(stdout, s, w, h); }
void checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_checkbox(f, x1, y1, x2, y2, s, id); }
void checkbox(int x1, int y1, int x2, int y2, char* s, int id) { ami_checkbox(stdout, x1, y1, x2, y2, s, id); }
void checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_checkboxg(f, x1, y1, x2, y2, s, id); }
void checkboxg(int x1, int y1, int x2, int y2, char* s, int id) { ami_checkboxg(stdout, x1, y1, x2, y2, s, id); }
void radiobuttonsiz(FILE* f, char* s, int* w, int* h) { ami_radiobuttonsiz(f, s, w, h); }
void radiobuttonsiz(char* s, int* w, int* h) { ami_radiobuttonsiz(stdout, s, w, h); }
void radiobuttonsizg(FILE* f, char* s, int* w, int* h) { ami_radiobuttonsizg(f, s, w, h); }
void radiobuttonsizg(char* s, int* w, int* h) { ami_radiobuttonsizg(stdout, s, w, h); }
void radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_radiobutton(f, x1, y1, x2, y2, s, id); }
void radiobutton(int x1, int y1, int x2, int y2, char* s, int id) { ami_radiobutton(stdout, x1, y1, x2, y2, s, id); }
void radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_radiobuttong(f, x1, y1, x2, y2, s, id); }
void radiobuttong(int x1, int y1, int x2, int y2, char* s, int id) { ami_radiobuttong(stdout, x1, y1, x2, y2, s, id); }
void groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_groupsiz(f, s, cw, ch, w, h, ox, oy); }
void groupsiz(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_groupsiz(stdout, s, cw, ch, w, h, ox, oy); }
void groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_groupsizg(f, s, cw, ch, w, h, ox, oy); }
void groupsizg(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_groupsizg(stdout, s, cw, ch, w, h, ox, oy); }
void group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_group(f, x1, y1, x2, y2, s, id); }
void group(int x1, int y1, int x2, int y2, char* s, int id) { ami_group(stdout, x1, y1, x2, y2, s, id); }
void groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id) { ami_groupg(f, x1, y1, x2, y2, s, id); }
void groupg(int x1, int y1, int x2, int y2, char* s, int id) { ami_groupg(stdout, x1, y1, x2, y2, s, id); }
void background(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_background(f, x1, y1, x2, y2, id); }
void background(int x1, int y1, int x2, int y2, int id) { ami_background(stdout, x1, y1, x2, y2, id); }
void backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_backgroundg(f, x1, y1, x2, y2, id); }
void backgroundg(int x1, int y1, int x2, int y2, int id) { ami_backgroundg(stdout, x1, y1, x2, y2, id); }
void scrollvertsiz(FILE* f, int* w, int* h) { ami_scrollvertsiz(f, w, h); }
void scrollvertsiz(int* w, int* h) { ami_scrollvertsiz(stdout, w, h); }
void scrollvertsizg(FILE* f, int* w, int* h) { ami_scrollvertsizg(f, w, h); }
void scrollvertsizg(int* w, int* h) { ami_scrollvertsizg(stdout, w, h); }
void scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_scrollvert(f, x1, y1, x2, y2, id); }
void scrollvert(int x1, int y1, int x2, int y2, int id) { ami_scrollvert(stdout, x1, y1, x2, y2, id); }
void scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_scrollvertg(f, x1, y1, x2, y2, id); }
void scrollvertg(int x1, int y1, int x2, int y2, int id) { ami_scrollvertg(stdout, x1, y1, x2, y2, id); }
void scrollhorizsiz(FILE* f, int* w, int* h) { ami_scrollhorizsiz(f, w, h); }
void scrollhorizsiz(int* w, int* h) { ami_scrollhorizsiz(stdout, w, h); }
void scrollhorizsizg(FILE* f, int* w, int* h) { ami_scrollhorizsizg(f, w, h); }
void scrollhorizsizg(int* w, int* h) { ami_scrollhorizsizg(stdout, w, h); }
void scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_scrollhoriz(f, x1, y1, x2, y2, id); }
void scrollhoriz(int x1, int y1, int x2, int y2, int id) { ami_scrollhoriz(stdout, x1, y1, x2, y2, id); }
void scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_scrollhorizg(f, x1, y1, x2, y2, id); }
void scrollhorizg(int x1, int y1, int x2, int y2, int id) { ami_scrollhorizg(stdout, x1, y1, x2, y2, id); }
void scrollpos(FILE* f, int id, int r) { ami_scrollpos(f, id, r); }
void scrollpos(int id, int r) { ami_scrollpos(stdout, id, r); }
void scrollsiz(FILE* f, int id, int r) { ami_scrollsiz(f, id, r); }
void scrollsiz(int id, int r) { ami_scrollsiz(stdout, id, r); }
void numselboxsiz(FILE* f, int l, int u, int* w, int* h) { ami_numselboxsiz(f, l, u, w, h); }
void numselboxsiz(int l, int u, int* w, int* h) { ami_numselboxsiz(stdout, l, u, w, h); }
void numselboxsizg(FILE* f, int l, int u, int* w, int* h) { ami_numselboxsizg(f, l, u, w, h); }
void numselboxsizg(int l, int u, int* w, int* h) { ami_numselboxsizg(stdout, l, u, w, h); }
void numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id) { ami_numselbox(f, x1, y1, x2, y2, l, u, id); }
void numselbox(int x1, int y1, int x2, int y2, int l, int u, int id) { ami_numselbox(stdout, x1, y1, x2, y2, l, u, id); }
void numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id) { ami_numselboxg(f, x1, y1, x2, y2, l, u, id); }
void numselboxg(int x1, int y1, int x2, int y2, int l, int u, int id) { ami_numselboxg(stdout, x1, y1, x2, y2, l, u, id); }
void editboxsiz(FILE* f, char* s, int* w, int* h) { ami_editboxsiz(f, s, w, h); }
void editboxsiz(char* s, int* w, int* h) { ami_editboxsiz(stdout, s, w, h); }
void editboxsizg(FILE* f, char* s, int* w, int* h) { ami_editboxsizg(f, s, w, h); }
void editboxsizg(char* s, int* w, int* h) { ami_editboxsizg(stdout, s, w, h); }
void editbox(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_editbox(f, x1, y1, x2, y2, id); }
void editbox(int x1, int y1, int x2, int y2, int id) { ami_editbox(stdout, x1, y1, x2, y2, id); }
void editboxg(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_editboxg(f, x1, y1, x2, y2, id); }
void editboxg(int x1, int y1, int x2, int y2, int id) { ami_editboxg(stdout, x1, y1, x2, y2, id); }
void progbarsiz(FILE* f, int* w, int* h) { ami_progbarsiz(f, w, h); }
void progbarsiz(int* w, int* h) { ami_progbarsiz(stdout, w, h); }
void progbarsizg(FILE* f, int* w, int* h) { ami_progbarsizg(f, w, h); }
void progbarsizg(int* w, int* h) { ami_progbarsizg(stdout, w, h); }
void progbar(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_progbar(f, x1, y1, x2, y2, id); }
void progbar(int x1, int y1, int x2, int y2, int id) { ami_progbar(stdout, x1, y1, x2, y2, id); }
void progbarg(FILE* f, int x1, int y1, int x2, int y2, int id) { ami_progbarg(f, x1, y1, x2, y2, id); }
void progbarg(int x1, int y1, int x2, int y2, int id) { ami_progbarg(stdout, x1, y1, x2, y2, id); }
void progbarpos(FILE* f, int id, int pos) { ami_progbarpos(f, id, pos); }
void progbarpos(int id, int pos) { ami_progbarpos(stdout, id, pos); }
void listboxsiz(FILE* f, strptr sp, int* w, int* h) { ami_listboxsiz(f, (ami_strptr)sp, w, h); }
void listboxsiz(strptr sp, int* w, int* h) { ami_listboxsiz(stdout, (ami_strptr)sp, w, h); }
void listboxsizg(FILE* f, strptr sp, int* w, int* h) { ami_listboxsizg(f, (ami_strptr)sp, w, h); }
void listboxsizg(strptr sp, int* w, int* h) { ami_listboxsizg(stdout, (ami_strptr)sp, w, h); }
void listbox(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id) { ami_listbox(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void listbox(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_listbox(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void listboxg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id) { ami_listboxg(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void listboxg(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_listboxg(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropboxsiz(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropboxsiz(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropboxsiz(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsizg(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropboxsizg(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropboxsizg(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropbox(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropbox(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropbox(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropbox(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropboxg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropboxg(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropboxg(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropboxg(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditboxsiz(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropeditboxsiz(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropeditboxsiz(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsizg(FILE* f, strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropeditboxsizg(f, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropeditboxsizg(stdout, (ami_strptr)sp, cw, ch, ow, oh); }
void dropeditbox(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropeditbox(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditbox(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropeditbox(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropeditboxg(f, x1, y1, x2, y2, (ami_strptr)sp, id); }
void dropeditboxg(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropeditboxg(stdout, x1, y1, x2, y2, (ami_strptr)sp, id); }
void slidehorizsiz(FILE* f, int* w, int* h) { ami_slidehorizsiz(f, w, h); }
void slidehorizsiz(int* w, int* h) { ami_slidehorizsiz(stdout, w, h); }
void slidehorizsizg(FILE* f, int* w, int* h) { ami_slidehorizsizg(f, w, h); }
void slidehorizsizg(int* w, int* h) { ami_slidehorizsizg(stdout, w, h); }
void slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id) { ami_slidehoriz(f, x1, y1, x2, y2, mark, id); }
void slidehoriz(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidehoriz(stdout, x1, y1, x2, y2, mark, id); }
void slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id) { ami_slidehorizg(f, x1, y1, x2, y2, mark, id); }
void slidehorizg(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidehorizg(stdout, x1, y1, x2, y2, mark, id); }
void slidevertsiz(FILE* f, int* w, int* h) { ami_slidevertsiz(f, w, h); }
void slidevertsiz(int* w, int* h) { ami_slidevertsiz(stdout, w, h); }
void slidevertsizg(FILE* f, int* w, int* h) { ami_slidevertsizg(f, w, h); }
void slidevertsizg(int* w, int* h) { ami_slidevertsizg(stdout, w, h); }
void slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id) { ami_slidevert(f, x1, y1, x2, y2, mark, id); }
void slidevert(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidevert(stdout, x1, y1, x2, y2, mark, id); }
void slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id) { ami_slidevertg(f, x1, y1, x2, y2, mark, id); }
void slidevertg(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidevertg(stdout, x1, y1, x2, y2, mark, id); }
void tabbarsiz(FILE* f, tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_tabbarsiz(f, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsiz(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_tabbarsiz(stdout, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsizg(FILE* f, tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_tabbarsizg(f, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarsizg(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_tabbarsizg(stdout, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void tabbarclient(FILE* f, tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy) { ami_tabbarclient(f, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclient(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy) { ami_tabbarclient(stdout, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclientg(FILE* f, tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy) { ami_tabbarclientg(f, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbarclientg(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy) { ami_tabbarclientg(stdout, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void tabbar(FILE* f, int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id) { ami_tabbar(f, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabbar(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id) { ami_tabbar(stdout, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabbarg(FILE* f, int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id) { ami_tabbarg(f, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabbarg(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id) { ami_tabbarg(stdout, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void tabsel(FILE* f, int id, int tn) { ami_tabsel(f, id, tn); }
void tabsel(int id, int tn) { ami_tabsel(stdout, id, tn); }

/* dialogs */
void alert(char* title, char* message) { ami_alert(title, message); }
void querycolor(int* r, int* g, int* b) { ami_querycolor(r, g, b); }
void queryopen(char* s, int sl) { ami_queryopen(s, sl); }
void querysave(char* s, int sl) { ami_querysave(s, sl); }
void queryfind(char* s, int sl, qfnopts* opt) { ami_queryfind(s, sl, (ami_qfnopts*)opt); }
void queryfindrep(char* s, int sl, char* r, int rl, qfropts* opt) { ami_queryfindrep(s, sl, r, rl, (ami_qfropts*)opt); }
void queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
               int* bg, int* bb, qfteffects* effect) { ami_queryfont(f, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }
void queryfont(int* fc, int* s, int* fr, int* fg, int* fb, int* br,
               int* bg, int* bb, qfteffects* effect) { ami_queryfont(stdout, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }

/* methods */
graph::graph(void)

{

    infile = stdin;
    outfile = stdout;
    graphoCB = this;
    eventsover(graphCB, &graphoeh);

}

/* text */
void graph::cursor(int x, int y) { ami_cursor(outfile, x, y); }
int  graph::maxx(void) { return ami_maxx(outfile); }
int  graph::maxy(void) { return ami_maxy(outfile); }
void graph::home(void) { ami_home(outfile); }
void graph::del(void) { ami_del(outfile); }
void graph::up(void) { ami_up(outfile); }
void graph::down(void) { ami_down(outfile); }
void graph::left(void) { ami_left(outfile); }
void graph::right(void) { ami_right(outfile); }
void graph::blink(int e) { ami_blink(outfile, e); }
void graph::reverse(int e) { ami_reverse(outfile, e); }
void graph::underline(int e) { ami_underline(outfile, e); }
void graph::superscript(int e) { ami_superscript(outfile, e); }
void graph::subscript(int e) { ami_subscript(outfile, e); }
void graph::italic(int e) { ami_italic(outfile, e); }
void graph::bold(int e) { ami_bold(outfile, e); }
void graph::strikeout(int e) { ami_strikeout(outfile, e); }
void graph::standout(int e) { ami_standout(outfile, e); }
void graph::fcolor(color c) { ami_fcolor(outfile, (ami_color)c); }
void graph::bcolor(color c) { ami_bcolor(outfile, (ami_color)c); }
void graph::autom(int e) { ami_auto(outfile, e); }
void graph::curvis(int e) { ami_curvis(outfile, e); }
void graph::scroll(int x, int y) { ami_scroll(outfile, x, y); }
int  graph::curx(void) { return ami_curx(outfile); }
int  graph::cury(void) { return ami_cury(outfile); }
int  graph::curbnd(void) { return ami_curbnd(outfile); }
void graph::select(int u, int d) { ami_select(outfile, u, d); }
void graph::event(evtrec* er) { ami_event(infile, (ami_evtptr)er); }
void graph::timer(int i, long t, int r) { ami_timer(outfile, i, t, r); }
void graph::killtimer(int i) { ami_killtimer(outfile, i); }
int  graph::mouse(void) { return ami_mouse(outfile); }
int  graph::mousebutton(int m) { return ami_mousebutton(outfile, m); }
int  graph::joystick(void) { return ami_joystick(outfile); }
int  graph::joybutton(int j) { return ami_joybutton(outfile, j); }
int  graph::joyaxis(int j) { return ami_joyaxis(outfile, j); }
void graph::settab(int t) { ami_settab(outfile, t); }
void graph::restab(int t) { ami_restab(outfile, t); }
void graph::clrtab(void) { ami_clrtab(outfile); }
int  graph::funkey(void) { return ami_funkey(outfile); }
void graph::frametimer(int e) { ami_frametimer(outfile, e); }
void graph::autohold(int e) { ami_autohold(e); }
void graph::wrtstr(char* s) { ami_wrtstr(outfile, s); }
void graph::wrtstrn(char* s, int n) { ami_wrtstrn(outfile, s, n); }
void graph::sizbuf(int x, int y) { ami_sizbuf(outfile, x, y); }
void graph::title(char* ts) { ami_title(outfile, ts); }
void graph::sendevent(evtrec* er) { ami_sendevent(outfile, (ami_evtptr)er); }

/* graphical */
int  graph::maxxg(void) { return ami_maxxg(outfile); }
int  graph::maxyg(void) { return ami_maxyg(outfile); }
int  graph::curxg(void) { return ami_curxg(outfile); }
int  graph::curyg(void) { return ami_curyg(outfile); }
void graph::line(int x1, int y1, int x2, int y2) { ami_line(outfile, x1, y1, x2, y2); }
void graph::linewidth(int w) { ami_linewidth(outfile, w); }
void graph::rect(int x1, int y1, int x2, int y2) { ami_rect(outfile, x1, y1, x2, y2); }
void graph::frect(int x1, int y1, int x2, int y2) { ami_frect(outfile, x1, y1, x2, y2); }
void graph::rrect(int x1, int y1, int x2, int y2, int xs, int ys) { ami_rrect(outfile, x1, y1, x2, y2, xs, ys); }
void graph::frrect(int x1, int y1, int x2, int y2, int xs, int ys) { ami_frrect(outfile, x1, y1, x2, y2, xs, ys); }
void graph::ellipse(int x1, int y1, int x2, int y2) { ami_ellipse(outfile, x1, y1, x2, y2); }
void graph::fellipse(int x1, int y1, int x2, int y2) { ami_fellipse(outfile, x1, y1, x2, y2); }
void graph::arc(int x1, int y1, int x2, int y2, int sa, int ea) { ami_arc(outfile, x1, y1, x2, y2, sa, ea); }
void graph::farc(int x1, int y1, int x2, int y2, int sa, int ea) { ami_farc(outfile, x1, y1, x2, y2, sa, ea); }
void graph::fchord(int x1, int y1, int x2, int y2, int sa, int ea) { ami_fchord(outfile, x1, y1, x2, y2, sa, ea); }
void graph::ftriangle(int x1, int y1, int x2, int y2, int x3, int y3) { ami_ftriangle(outfile, x1, y1, x2, y2, x3, y3); }
void graph::cursorg(int x, int y) { ami_cursorg(outfile, x, y); }
int  graph::baseline(void) { return ami_baseline(outfile); }
void graph::setpixel(int x, int y) { ami_setpixel(outfile, x, y); }
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
int  graph::chrsizx(void) { return ami_chrsizx(outfile); }
int  graph::chrsizy(void) { return ami_chrsizy(outfile); }
int  graph::fonts(void) { return ami_fonts(outfile); }
void graph::font(int fc) { ami_font(outfile, fc); }
void graph::fontnam(int fc, char* fns, int fnsl) { ami_fontnam(outfile, fc, fns, fnsl); }
void graph::fontsiz(int s) { ami_fontsiz(outfile, s); }
void graph::chrspcy(int s) { ami_chrspcy(outfile, s); }
void graph::chrspcx(int s) { ami_chrspcx(outfile, s); }
int  graph::dpmx(void) { return ami_dpmx(outfile); }
int  graph::dpmy(void) { return ami_dpmy(outfile); }
int  graph::strsiz(const char* s) { return ami_strsiz(outfile, s); }
int  graph::chrpos(const char* s, int p) { return ami_chrpos(outfile, s, p); }
void graph::writejust(const char* s, int n) { ami_writejust(outfile, s, n); }
int  graph::justpos(const char* s, int p, int n) { return ami_justpos(outfile, s, p, n); }
void graph::condensed(int e) { ami_condensed(outfile, e); }
void graph::extended(int e) { ami_extended(outfile, e); }
void graph::xlight(int e) { ami_xlight(outfile, e); }
void graph::light(int e) { ami_light(outfile, e); }
void graph::xbold(int e) { ami_xbold(outfile, e); }
void graph::hollow(int e) { ami_hollow(outfile, e); }
void graph::raised(int e) { ami_raised(outfile, e); }
void graph::settabg(int t) { ami_settabg(outfile, t); }
void graph::restabg(int t) { ami_restabg(outfile, t); }
void graph::fcolorg(int r, int g, int b) { ami_fcolorg(outfile, r, g, b); }
void graph::fcolorc(int r, int g, int b) { ami_fcolorc(outfile, r, g, b); }
void graph::bcolorg(int r, int g, int b) { ami_bcolorg(outfile, r, g, b); }
void graph::bcolorc(int r, int g, int b) { ami_bcolorc(outfile, r, g, b); }
void graph::loadpict(int p, char* fn) { ami_loadpict(outfile, p, fn); }
int  graph::pictsizx(int p) { return ami_pictsizx(outfile, p); }
int  graph::pictsizy(int p) { return ami_pictsizy(outfile, p); }
void graph::picture(int p, int x1, int y1, int x2, int y2) { ami_picture(outfile, p, x1, y1, x2, y2); }
void graph::delpict(int p) { ami_delpict(outfile, p); }
void graph::scrollg(int x, int y) { ami_scrollg(outfile, x, y); }
void graph::path(int a) { ami_path(outfile, a); }

/* window management */
void graph::buffer(int e) { ami_buffer(outfile, e); }
void graph::sizbufg(int x, int y) { ami_sizbufg(outfile, x, y); }
void graph::getsiz(int* x, int* y) { ami_getsiz(outfile, x, y); }
void graph::getsizg(int* x, int* y) { ami_getsizg(outfile, x, y); }
void graph::setsiz(int x, int y) { ami_setsiz(outfile, x, y); }
void graph::setsizg(int x, int y) { ami_setsizg(outfile, x, y); }
void graph::setpos(int x, int y) { ami_setpos(outfile, x, y); }
void graph::setposg(int x, int y) { ami_setposg(outfile, x, y); }
void graph::scnsiz(int* x, int* y) { ami_scnsiz(outfile, x, y); }
void graph::scnsizg(int* x, int* y) { ami_scnsizg(outfile, x, y); }
void graph::scncen(int* x, int* y) { ami_scncen(outfile, x, y); }
void graph::scnceng(int* x, int* y) { ami_scnceng(outfile, x, y); }
void graph::winclient(int cx, int cy, int* wx, int* wy, winmodset ms) { ami_winclient(outfile, cx, cy, wx, wy, (ami_winmodset)ms); }
void graph::winclientg(int cx, int cy, int* wx, int* wy, winmodset ms) { ami_winclientg(outfile, cx, cy, wx, wy, (ami_winmodset)ms); }
void graph::front(void) { ami_front(outfile); }
void graph::back(void) { ami_back(outfile); }
void graph::frame(int e) { ami_frame(outfile, e); }
void graph::sizable(int e) { ami_sizable(outfile, e); }
void graph::sysbar(int e) { ami_sysbar(outfile, e); }
void graph::menu(menuptr m) { ami_menu(outfile, (ami_menuptr)m); }
void graph::menuena(int id, int onoff) { ami_menuena(outfile, id, onoff); }
void graph::menusel(int id, int select) { ami_menusel(outfile, id, select); }
void graph::focus(void) { ami_focus(outfile); }

/* widgets */
int  graph::getwigid(void) { return ami_getwigid(outfile); }
void graph::killwidget(int id) { ami_killwidget(outfile, id); }
void graph::selectwidget(int id, int e) { ami_selectwidget(outfile, id, e); }
void graph::enablewidget(int id, int e) { ami_enablewidget(outfile, id, e); }
void graph::getwidgettext(int id, char* s, int sl) { ami_getwidgettext(outfile, id, s, sl); }
void graph::putwidgettext(int id, char* s) { ami_putwidgettext(outfile, id, s); }
void graph::sizwidget(int id, int x, int y) { ami_sizwidget(outfile, id, x, y); }
void graph::sizwidgetg(int id, int x, int y) { ami_sizwidgetg(outfile, id, x, y); }
void graph::poswidget(int id, int x, int y) { ami_poswidget(outfile, id, x, y); }
void graph::poswidgetg(int id, int x, int y) { ami_poswidgetg(outfile, id, x, y); }
void graph::backwidget(int id) { ami_backwidget(outfile, id); }
void graph::frontwidget(int id) { ami_frontwidget(outfile, id); }
void graph::focuswidget(int id) { ami_focuswidget(outfile, id); }
void graph::buttonsiz(char* s, int* w, int* h) { ami_buttonsiz(outfile, s, w, h); }
void graph::buttonsizg(char* s, int* w, int* h) { ami_buttonsizg(outfile, s, w, h); }
void graph::button(int x1, int y1, int x2, int y2, char* s, int id) { ami_button(outfile, x1, y1, x2, y2, s, id); }
void graph::buttong(int x1, int y1, int x2, int y2, char* s, int id) { ami_buttong(outfile, x1, y1, x2, y2, s, id); }
void graph::checkboxsiz(char* s, int* w, int* h) { ami_checkboxsiz(outfile, s, w, h); }
void graph::checkboxsizg(char* s, int* w, int* h) { ami_checkboxsizg(outfile, s, w, h); }
void graph::checkbox(int x1, int y1, int x2, int y2, char* s, int id) { ami_checkbox(outfile, x1, y1, x2, y2, s, id); }
void graph::checkboxg(int x1, int y1, int x2, int y2, char* s, int id) { ami_checkboxg(outfile, x1, y1, x2, y2, s, id); }
void graph::radiobuttonsiz(char* s, int* w, int* h) { ami_radiobuttonsiz(outfile, s, w, h); }
void graph::radiobuttonsizg(char* s, int* w, int* h) { ami_radiobuttonsizg(outfile, s, w, h); }
void graph::radiobutton(int x1, int y1, int x2, int y2, char* s, int id) { ami_radiobutton(outfile, x1, y1, x2, y2, s, id); }
void graph::radiobuttong(int x1, int y1, int x2, int y2, char* s, int id) { ami_radiobuttong(outfile, x1, y1, x2, y2, s, id); }
void graph::groupsiz(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_groupsiz(outfile, s, cw, ch, w, h, ox, oy); }
void graph::groupsizg(char* s, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_groupsizg(outfile, s, cw, ch, w, h, ox, oy); }
void graph::group(int x1, int y1, int x2, int y2, char* s, int id) { ami_group(outfile, x1, y1, x2, y2, s, id); }
void graph::groupg(int x1, int y1, int x2, int y2, char* s, int id) { ami_groupg(outfile, x1, y1, x2, y2, s, id); }
void graph::background(int x1, int y1, int x2, int y2, int id) { ami_background(outfile, x1, y1, x2, y2, id); }
void graph::backgroundg(int x1, int y1, int x2, int y2, int id) { ami_backgroundg(outfile, x1, y1, x2, y2, id); }
void graph::scrollvertsiz(int* w, int* h) { ami_scrollvertsiz(outfile, w, h); }
void graph::scrollvertsizg(int* w, int* h) { ami_scrollvertsizg(outfile, w, h); }
void graph::scrollvert(int x1, int y1, int x2, int y2, int id) { ami_scrollvert(outfile, x1, y1, x2, y2, id); }
void graph::scrollvertg(int x1, int y1, int x2, int y2, int id) { ami_scrollvertg(outfile, x1, y1, x2, y2, id); }
void graph::scrollhorizsiz(int* w, int* h) { ami_scrollhorizsiz(outfile, w, h); }
void graph::scrollhorizsizg(int* w, int* h) { ami_scrollhorizsizg(outfile, w, h); }
void graph::scrollhoriz(int x1, int y1, int x2, int y2, int id) { ami_scrollhoriz(outfile, x1, y1, x2, y2, id); }
void graph::scrollhorizg(int x1, int y1, int x2, int y2, int id) { ami_scrollhorizg(outfile, x1, y1, x2, y2, id); }
void graph::scrollpos(int id, int r) { ami_scrollpos(outfile, id, r); }
void graph::scrollsiz(int id, int r) { ami_scrollsiz(outfile, id, r); }
void graph::numselboxsiz(int l, int u, int* w, int* h) { ami_numselboxsiz(outfile, l, u, w, h); }
void graph::numselboxsizg(int l, int u, int* w, int* h) { ami_numselboxsizg(outfile, l, u, w, h); }
void graph::numselbox(int x1, int y1, int x2, int y2, int l, int u, int id) { ami_numselbox(outfile, x1, y1, x2, y2, l, u, id); }
void graph::numselboxg(int x1, int y1, int x2, int y2, int l, int u, int id) { ami_numselboxg(outfile, x1, y1, x2, y2, l, u, id); }
void graph::editboxsiz(char* s, int* w, int* h) { ami_editboxsiz(outfile, s, w, h); }
void graph::editboxsizg(char* s, int* w, int* h) { ami_editboxsizg(outfile, s, w, h); }
void graph::editbox(int x1, int y1, int x2, int y2, int id) { ami_editbox(outfile, x1, y1, x2, y2, id); }
void graph::editboxg(int x1, int y1, int x2, int y2, int id) { ami_editboxg(outfile, x1, y1, x2, y2, id); }
void graph::progbarsiz(int* w, int* h) { ami_progbarsiz(outfile, w, h); }
void graph::progbarsizg(int* w, int* h) { ami_progbarsizg(outfile, w, h); }
void graph::progbar(int x1, int y1, int x2, int y2, int id) { ami_progbar(outfile, x1, y1, x2, y2, id); }
void graph::progbarg(int x1, int y1, int x2, int y2, int id) { ami_progbarg(outfile, x1, y1, x2, y2, id); }
void graph::progbarpos(int id, int pos) { ami_progbarpos(outfile, id, pos); }
void graph::listboxsiz(strptr sp, int* w, int* h) { ami_listboxsiz(outfile, (ami_strptr)sp, w, h); }
void graph::listboxsizg(strptr sp, int* w, int* h) { ami_listboxsizg(outfile, (ami_strptr)sp, w, h); }
void graph::listbox(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_listbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::listboxg(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_listboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropboxsiz(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropboxsizg(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropbox(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropboxg(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropeditboxsiz(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropeditboxsiz(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropeditboxsizg(strptr sp, int* cw, int* ch, int* ow, int* oh) { ami_dropeditboxsizg(outfile, (ami_strptr)sp, cw, ch, ow, oh); }
void graph::dropeditbox(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropeditbox(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::dropeditboxg(int x1, int y1, int x2, int y2, strptr sp, int id) { ami_dropeditboxg(outfile, x1, y1, x2, y2, (ami_strptr)sp, id); }
void graph::slidehorizsiz(int* w, int* h) { ami_slidehorizsiz(outfile, w, h); }
void graph::slidehorizsizg(int* w, int* h) { ami_slidehorizsizg(outfile, w, h); }
void graph::slidehoriz(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidehoriz(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidehorizg(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidehorizg(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidevertsiz(int* w, int* h) { ami_slidevertsiz(outfile, w, h); }
void graph::slidevertsizg(int* w, int* h) { ami_slidevertsizg(outfile, w, h); }
void graph::slidevert(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidevert(outfile, x1, y1, x2, y2, mark, id); }
void graph::slidevertg(int x1, int y1, int x2, int y2, int mark, int id) { ami_slidevertg(outfile, x1, y1, x2, y2, mark, id); }
void graph::tabbarsiz(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_tabbarsiz(outfile, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void graph::tabbarsizg(tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy) { ami_tabbarsizg(outfile, (ami_tabori)tor, cw, ch, w, h, ox, oy); }
void graph::tabbarclient(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy) { ami_tabbarclient(outfile, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void graph::tabbarclientg(tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy) { ami_tabbarclientg(outfile, (ami_tabori)tor, w, h, cw, ch, ox, oy); }
void graph::tabbar(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id) { ami_tabbar(outfile, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void graph::tabbarg(int x1, int y1, int x2, int y2, strptr sp, tabori tor, int id) { ami_tabbarg(outfile, x1, y1, x2, y2, (ami_strptr)sp, (ami_tabori)tor, id); }
void graph::tabsel(int id, int tn) { ami_tabsel(outfile, id, tn); }

/* dialogs */
void graph::queryfont(int* fc, int* s, int* fr, int* fg, int* fb, int* br,
                      int* bg, int* bb, qfteffects* effect) { ami_queryfont(outfile, fc, s, fr, fg, fb, br, bg, bb, (ami_qfteffects*)effect); }

/* virtual callbacks */
int graph::evchar(char c) { return 0; }
int graph::evup(void) { return 0; }
int graph::evdown(void) { return 0; }
int graph::evleft(void) { return 0; }
int graph::evright(void) { return 0; }
int graph::evleftw(void) { return 0; }
int graph::evrightw(void) { return 0; }
int graph::evhome(void) { return 0; }
int graph::evhomes(void) { return 0; }
int graph::evhomel(void) { return 0; }
int graph::evend(void) { return 0; }
int graph::evends(void) { return 0; }
int graph::evendl(void) { return 0; }
int graph::evscrl(void) { return 0; }
int graph::evscrr(void) { return 0; }
int graph::evscru(void) { return 0; }
int graph::evscrd(void) { return 0; }
int graph::evpagd(void) { return 0; }
int graph::evpagu(void) { return 0; }
int graph::evtab(void) { return 0; }
int graph::eventer(void) { return 0; }
int graph::evinsert(void) { return 0; }
int graph::evinsertl(void) { return 0; }
int graph::evinsertt(void) { return 0; }
int graph::evdel(void) { return 0; }
int graph::evdell(void) { return 0; }
int graph::evdelcf(void) { return 0; }
int graph::evdelcb(void) { return 0; }
int graph::evcopy(void) { return 0; }
int graph::evcopyl(void) { return 0; }
int graph::evcan(void) { return 0; }
int graph::evstop(void) { return 0; }
int graph::evcont(void) { return 0; }
int graph::evprint(void) { return 0; }
int graph::evprintb(void) { return 0; }
int graph::evprints(void) { return 0; }
int graph::evfun(int k) { return 0; }
int graph::evmenu(void) { return 0; }
int graph::evmouba(int m, int b) { return 0; }
int graph::evmoubd(int m, int b) { return 0; }
int graph::evmoumov(int m, int x, int y) { return 0; }
int graph::evtim(int t) { return 0; }
int graph::evjoyba(int j, int b) { return 0; }
int graph::evjoybd(int j, int b) { return 0; }
int graph::evjoymov(int j, int x, int y, int z) { return 0; }
int graph::evresize(void) { return 0; }
int graph::evfocus(void) { return 0; }
int graph::evnofocus(void) { return 0; }
int graph::evhover(void) { return 0; }
int graph::evnohover(void) { return 0; }
int graph::evterm(void) { return 0; }
int graph::evframe(void) { return 0; }
int graph::evmoumovg(int m, int x, int y) { return 0; }
int graph::evredraw(int x1, int y1, int x2, int y2) { return 0; }
int graph::evmin(void) { return 0; }
int graph::evmax(void) { return 0; }
int graph::evnorm(void) { return 0; }
int graph::evmenus(int id) { return 0; }
int graph::evbutton(int id) { return 0; }
int graph::evchkbox(int id) { return 0; }
int graph::evradbut(int id) { return 0; }
int graph::evsclull(int id) { return 0; }
int graph::evscldrl(int id) { return 0; }
int graph::evsclulp(int id) { return 0; }
int graph::evscldrp(int id) { return 0; }
int graph::evsclpos(int id, int pos) { return 0; }
int graph::evedtbox(int id) { return 0; }
int graph::evnumbox(int id, int val) { return 0; }
int graph::evlstbox(int id, int sel) { return 0; }
int graph::evdrpbox(int id, int sel) { return 0; }
int graph::evdrebox(int id) { return 0; }
int graph::evsldpos(int id, int pos) { return 0; }
int graph::evtabbar(int id, int sel) { return 0; }

void graph::graphCB(evtrec* er)

{

    int handled;

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
        default: handled = 0; break;

    }

    er->handled = handled;
    if (!handled) (*graphoeh)(er);

}

} /* namespace graphics */
