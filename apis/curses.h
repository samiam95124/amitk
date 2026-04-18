/*******************************************************************************
*                                                                              *
*                     CURSES COMPATIBILITY LAYER FOR PETIT-AMI                 *
*                                                                              *
* Implements the most commonly used curses/ncurses functions on top of the     *
* Petit-Ami terminal API. Programs written for curses can be compiled against  *
* this header and linked with curses.c + the Petit-Ami terminal library        *
* instead of ncurses.                                                          *
*                                                                              *
* Differences from ncurses:                                                    *
* - Only stdscr is supported (no subwindows via newwin)                        *
* - Color pairs are simplified (8 fg x 8 bg)                                  *
* - ACS characters use Unicode box-drawing                                     *
* - Input is event-based (Ami events mapped to curses key codes)               *
*                                                                              *
*******************************************************************************/

#ifndef _AMI_CURSES_H
#define _AMI_CURSES_H

#include <stdio.h>
#include <stdarg.h>

/* boolean */
#include <stdbool.h>
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef ERR
#define ERR (-1)
#endif
#ifndef OK
#define OK  0
#endif

/* window type (we only support stdscr — this is a dummy struct) */
typedef struct _win_st { int dummy; } WINDOW;
extern WINDOW* stdscr;
#define curscr stdscr
extern int LINES;
extern int COLS;

/* initialization and termination */
WINDOW* initscr(void);
int     endwin(void);
int     isendwin(void);

/* output */
int refresh(void);
int clear(void);
int erase(void);
int clrtoeol(void);
int clrtobot(void);
int move(int y, int x);
int addch(int ch);
int addstr(const char* str);
int addnstr(const char* str, int n);
int mvaddch(int y, int x, int ch);
int mvaddstr(int y, int x, const char* str);
int printw(const char* fmt, ...);
int mvprintw(int y, int x, const char* fmt, ...);

/* input */
int getch(void);
int nodelay(WINDOW* win, int bf);
int timeout(int delay);
int keypad(WINDOW* win, int bf);
int cbreak(void);
int nocbreak(void);
int raw(void);
int noraw(void);
int echo(void);
int noecho(void);
int halfdelay(int tenths);
int ungetch(int ch);

/* attributes */
typedef unsigned long chtype;
typedef unsigned long attr_t;

#define A_NORMAL     0x00000000UL
#define A_STANDOUT   0x00010000UL
#define A_UNDERLINE  0x00020000UL
#define A_REVERSE    0x00040000UL
#define A_BLINK      0x00080000UL
#define A_BOLD       0x00100000UL
#define A_DIM        0x00200000UL
#define A_ITALIC     0x00400000UL

int attron(int attrs);
int attroff(int attrs);
int attrset(int attrs);

/* color */
#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

#define COLOR_PAIRS   64
#define COLOR_PAIR(n) ((n) << 8)
#define PAIR_NUMBER(a) (((a) >> 8) & 0x3F)

int  has_colors(void);
int  start_color(void);
int  init_pair(short pair, short fg, short bg);

/* cursor */
int curs_set(int visibility);

/* screen size */
int getmaxx(WINDOW* win);
int getmaxy(WINDOW* win);
/* getmaxyx is a macro in real curses — assigns to y and x directly */
#define getmaxyx(w, y, x) do { (y) = getmaxy(w); (x) = getmaxx(w); } while(0)

/* screen reading */
int mvinch(int y, int x);

/* windows (minimal — all map to stdscr) */
WINDOW* newwin(int nlines, int ncols, int begin_y, int begin_x);
int     wrefresh(WINDOW* win);
int     wmove(WINDOW* win, int y, int x);
int     waddch(WINDOW* win, int ch);
int     waddstr(WINDOW* win, const char* str);
int     mvwaddch(WINDOW* win, int y, int x, int ch);
int     mvwaddstr(WINDOW* win, int y, int x, const char* str);
int     wprintw(WINDOW* win, const char* fmt, ...);
int     mvwprintw(WINDOW* win, int y, int x, const char* fmt, ...);
int     mvwaddnstr(WINDOW* win, int y, int x, const char* str, int n);
int     wclear(WINDOW* win);
int     wclrtoeol(WINDOW* win);
int     wgetch(WINDOW* win);
int     wattron(WINDOW* win, int attrs);
int     wattroff(WINDOW* win, int attrs);
int     wattr_on(WINDOW* win, attr_t attrs, void* opts);
int     wattr_off(WINDOW* win, attr_t attrs, void* opts);
int     wattrset(WINDOW* win, attr_t attrs);
int     whline(WINDOW* win, chtype ch, int n);
int     wvline(WINDOW* win, chtype ch, int n);

/* terminfo / low-level terminal — minimal surface for programs that use
   <term.h> (e.g. htop's CRT.c). Our adapter doesn't drive terminfo, so
   cur_term is a dummy and these are mostly no-op stubs. */
extern int   COLORS;
extern void* cur_term;
/* terminfo capability strings — NULL in our adapter; tputs is a no-op on NULL */
extern char* enter_ca_mode;
extern char* exit_ca_mode;
extern char* clear_screen;
int set_escdelay(int ms);
int define_key(const char* definition, int keycode);
int mouseinterval(int interval);
int intrflush(WINDOW* win, int bf);
int mvcur(int oldrow, int oldcol, int newrow, int newcol);
int tputs(const char* str, int affcnt, int (*outc_fn)(int));

/* misc */
int napms(int ms);
int beep(void);
int baudrate(void);
int scrollok(WINDOW* win, int bf);
int touchwin(WINDOW* win);
int winch(WINDOW* win);
int leaveok(WINDOW* win, int bf);
int standout(void);
int standend(void);
int nl(void);
int nonl(void);
char erasechar(void);
char killchar(void);
int delwin(WINDOW* win);
int doupdate(void);
int wnoutrefresh(WINDOW* win);
int clearok(WINDOW* win, int bf);
int use_default_colors(void);
int waddnstr(WINDOW* win, const char* str, int n);
int wredrawln(WINDOW* win, int beg, int num);
int savetty(void);
int resetty(void);
int typeahead(int fd);
int meta(WINDOW* win, int bf);
int idlok(WINDOW* win, int bf);

/* mouse (minimal stubs) */
typedef unsigned long mmask_t;
typedef struct { short id; int x, y, z; mmask_t bstate; } MEVENT;
#define BUTTON1_CLICKED    0x004
#define BUTTON1_PRESSED    0x002
#define BUTTON1_RELEASED   0x001
#define BUTTON2_CLICKED    0x040
#define BUTTON2_PRESSED    0x020
#define BUTTON2_RELEASED   0x010
#define BUTTON3_CLICKED    0x400
#define BUTTON3_PRESSED    0x200
#define BUTTON3_RELEASED   0x100
#define KEY_MOUSE          0x199
#define REPORT_MOUSE_POSITION 0x100000
mmask_t mousemask(mmask_t newmask, mmask_t* oldmask);
int getmouse(MEVENT* event);
int ungetmouse(MEVENT* event);

/* box drawing */
int box(WINDOW* win, int verch, int horch);
int hline(int ch, int n);
int vline(int ch, int n);
int mvhline(int y, int x, int ch, int n);
int mvvline(int y, int x, int ch, int n);

/* ACS (alternate character set) box-drawing characters */
#define ACS_ULCORNER  0x250C  /* top left corner */
#define ACS_URCORNER  0x2510  /* top right corner */
#define ACS_LLCORNER  0x2514  /* bottom left corner */
#define ACS_LRCORNER  0x2518  /* bottom right corner */
#define ACS_HLINE     0x2500  /* horizontal line */
#define ACS_VLINE     0x2502  /* vertical line */
#define ACS_PLUS      0x253C  /* cross/plus */
#define ACS_LTEE      0x251C  /* left tee */
#define ACS_RTEE      0x2524  /* right tee */
#define ACS_TTEE      0x252C  /* top tee */
#define ACS_BTEE      0x2534  /* bottom tee */
#define ACS_DIAMOND   0x25C6  /* diamond */
#define ACS_BLOCK     0x2588  /* solid block */
#define ACS_BULLET    0x2022  /* bullet */

/* key codes */
#define KEY_UP        0x101
#define KEY_DOWN      0x102
#define KEY_LEFT      0x103
#define KEY_RIGHT     0x104
#define KEY_HOME      0x106
#define KEY_END       0x168
#define KEY_PPAGE     0x153  /* page up */
#define KEY_NPAGE     0x152  /* page down */
#define KEY_IC        0x14B  /* insert */
#define KEY_DC        0x14A  /* delete */
#define KEY_BACKSPACE 0x107
#define KEY_F0        0x109
#define KEY_F(n)      (KEY_F0 + (n))
#define KEY_ENTER     0x157
#define KEY_SLEFT     0x189   /* shift-left arrow */
#define KEY_SRIGHT    0x192   /* shift-right arrow */
#define KEY_RESIZE    0x19A
#define KEY_MAX       0x1FF   /* maximum legal key value */

/* Wide-character curses (X/Open Option). Minimal shim: we store a single
   wide char per cell — combining marks in chars[1..] are dropped. That's
   fine for the programs we target (htop only fills chars[0]), and it
   matches terminal.c's own UTF-8-per-cell storage model. */
#include <stddef.h>                 /* for wchar_t without pulling in stdio */
#define CCHARW_MAX 5
typedef struct {
    attr_t   attr;
    wchar_t  chars[CCHARW_MAX];
    int      ext_color;
} cchar_t;

int setcchar(cchar_t* cch, const wchar_t* wch, const attr_t attrs,
             short color_pair, const void* opts);
int getcchar(const cchar_t* cch, wchar_t* wch, attr_t* attrs,
             short* color_pair, void* opts);
int wadd_wch(WINDOW* win, const cchar_t* cch);
int wadd_wchnstr(WINDOW* win, const cchar_t* cchs, int n);
int mvadd_wch(int y, int x, const cchar_t* cch);
int mvadd_wchnstr(int y, int x, const cchar_t* cchs, int n);
int mvaddnstr(int y, int x, const char* str, int n);

#endif /* _AMI_CURSES_H */
