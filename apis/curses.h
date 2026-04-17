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
int     wclear(WINDOW* win);
int     wclrtoeol(WINDOW* win);
int     wgetch(WINDOW* win);

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
#define KEY_F(n)      (0x109 + (n))
#define KEY_ENTER     0x157
#define KEY_RESIZE    0x19A

#endif /* _AMI_CURSES_H */
