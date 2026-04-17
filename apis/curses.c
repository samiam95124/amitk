/*******************************************************************************
*                                                                              *
*                     CURSES COMPATIBILITY LAYER FOR PETIT-AMI                 *
*                                                                              *
* Maps curses API calls to the Petit-Ami terminal interface.                   *
*                                                                              *
* Key mappings:                                                                *
*   curses          Petit-Ami                                                  *
*   ------          ---------                                                  *
*   initscr()       ami_auto(off), ami_curvis(off)                             *
*   endwin()        ami_auto(on), ami_curvis(on)                               *
*   move(y,x)       ami_cursor(stdout, x+1, y+1)   (0-based to 1-based)       *
*   addch(c)        putchar(c)                                                 *
*   clear()         putchar('\f')                                              *
*   getch()         ami_event loop                                             *
*   attron/off      ami_bold, ami_reverse, ami_underline, etc.                 *
*   COLOR_PAIR      ami_fcolor, ami_bcolor                                     *
*                                                                              *
* Coordinate convention: curses uses (y, x) 0-based. Ami uses (x, y) 1-based. *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <terminal.h>
#include "curses.h"

/* internal state */
static int   cur_initialized = 0;
static int   cur_echo = 0;      /* echo mode (default off after initscr) */
static int   cur_nodelay = 0;   /* non-blocking mode */
static int   cur_timeout_ms = -1; /* getch timeout (-1 = blocking) */
static int   cur_keypad = 1;    /* keypad mode (always on) */
static attr_t cur_attrs = A_NORMAL;
static int   cur_ended = 0;
static int   cur_ungetch = -1;  /* ungetch buffer */

/* color pair table: fg/bg for each pair */
static struct { short fg; short bg; } color_pairs[COLOR_PAIRS];

/* screen content buffer for mvinch/winch readback */
static int  scrbuf_inited = 0;
static char scrbuf[256][256];

static void scrbuf_init(void) {

    memset(scrbuf, ' ', sizeof(scrbuf));
    scrbuf_inited = 1;

}

/* the single window */
static WINDOW stdscr_data;
WINDOW* stdscr = &stdscr_data;
int LINES = 25;
int COLS = 80;

/* map curses color to Ami color */
static ami_color color_to_ami(short c) {

    switch (c) {
        case COLOR_BLACK:   return ami_black;
        case COLOR_RED:     return ami_red;
        case COLOR_GREEN:   return ami_green;
        case COLOR_YELLOW:  return ami_yellow;
        case COLOR_BLUE:    return ami_blue;
        case COLOR_MAGENTA: return ami_magenta;
        case COLOR_CYAN:    return ami_cyan;
        case COLOR_WHITE:   return ami_white;
        default:            return ami_white;
    }

}

/* apply the current attribute set to Ami */
static void apply_attrs(void) {

    ami_bold(stdout, (cur_attrs & A_BOLD) ? 1 : 0);
    ami_reverse(stdout, (cur_attrs & A_REVERSE) ? 1 : 0);
    ami_underline(stdout, (cur_attrs & A_UNDERLINE) ? 1 : 0);
    ami_blink(stdout, (cur_attrs & A_BLINK) ? 1 : 0);
    ami_italic(stdout, (cur_attrs & A_ITALIC) ? 1 : 0);
    ami_standout(stdout, (cur_attrs & A_STANDOUT) ? 1 : 0);

    /* apply color pair */
    {
        int pair = PAIR_NUMBER(cur_attrs);
        if (pair > 0 && pair < COLOR_PAIRS) {

            ami_fcolor(stdout, color_to_ami(color_pairs[pair].fg));
            ami_bcolor(stdout, color_to_ami(color_pairs[pair].bg));

        }
    }

}

/* encode a Unicode code point as UTF-8 and write to stdout */
static void put_utf8(int cp) {

    if (cp < 0x80) {

        putchar(cp);

    } else if (cp < 0x800) {

        putchar(0xC0 | (cp >> 6));
        putchar(0x80 | (cp & 0x3F));

    } else {

        putchar(0xE0 | (cp >> 12));
        putchar(0x80 | ((cp >> 6) & 0x3F));
        putchar(0x80 | (cp & 0x3F));

    }

}

/*******************************************************************************

Initialization

*******************************************************************************/

WINDOW* initscr(void) {

    if (cur_initialized) return stdscr;
    cur_initialized = 1;
    cur_ended = 0;
    cur_echo = 0;

    /* Ami terminal auto mode handles scrolling and cursor movement.
       For curses compatibility we turn it off so we have full control. */
    ami_auto(stdout, 0);
    ami_curvis(stdout, 0);

    /* query screen size */
    LINES = ami_maxy(stdout);
    COLS  = ami_maxx(stdout);

    /* init color pairs to default (pair 0 = terminal's current colors) */
    memset(color_pairs, 0, sizeof(color_pairs));
    color_pairs[0].fg = COLOR_WHITE;
    color_pairs[0].bg = COLOR_BLACK;

    return stdscr;

}

int endwin(void) {

    if (!cur_initialized) return ERR;
    cur_ended = 1;
    ami_auto(stdout, 1);
    ami_curvis(stdout, 1);
    /* reset attributes */
    ami_bold(stdout, 0);
    ami_reverse(stdout, 0);
    ami_underline(stdout, 0);
    ami_blink(stdout, 0);
    ami_italic(stdout, 0);
    ami_fcolor(stdout, ami_white);
    ami_bcolor(stdout, ami_black);
    return OK;

}

int isendwin(void) { return cur_ended; }

/*******************************************************************************

Output

*******************************************************************************/

int refresh(void) {

    fflush(stdout);
    return OK;

}

int clear(void) {

    if (!scrbuf_inited) scrbuf_init();
    else memset(scrbuf, ' ', sizeof(scrbuf));
    putchar('\f');
    return OK;

}

int erase(void) { return clear(); }

int clrtoeol(void) {

    int cx = ami_curx(stdout);
    int cy = ami_cury(stdout);
    int mx = ami_maxx(stdout);
    int i;

    for (i = cx; i <= mx; i++) putchar(' ');
    ami_cursor(stdout, cx, cy);
    return OK;

}

int clrtobot(void) {

    int cx = ami_curx(stdout);
    int cy = ami_cury(stdout);
    int mx = ami_maxx(stdout);
    int my = ami_maxy(stdout);
    int x, y;

    /* clear rest of current line */
    for (x = cx; x <= mx; x++) putchar(' ');
    /* clear remaining lines */
    for (y = cy + 1; y <= my; y++) {

        ami_cursor(stdout, 1, y);
        for (x = 1; x <= mx; x++) putchar(' ');

    }
    ami_cursor(stdout, cx, cy);
    return OK;

}

int move(int y, int x) {

    /* curses: 0-based (y, x). Ami: 1-based (x, y) */
    ami_cursor(stdout, x + 1, y + 1);
    return OK;

}

int addch(int ch) {

    if (!scrbuf_inited) scrbuf_init();
    /* track screen contents for mvinch/winch readback */
    {
        int cx = ami_curx(stdout) - 1;
        int cy = ami_cury(stdout) - 1;
        if (cy >= 0 && cy < 256 && cx >= 0 && cx < 256)
            scrbuf[cy][cx] = (char)(ch & 0x7F);
    }
    if (ch > 0x7F) put_utf8(ch); /* ACS/Unicode character */
    else putchar(ch);
    return OK;

}

int addstr(const char* str) {

    while (*str) addch((unsigned char)*str++);
    return OK;

}

int addnstr(const char* str, int n) {

    int i;
    for (i = 0; i < n && str[i]; i++) addch((unsigned char)str[i]);
    return OK;

}

int mvaddch(int y, int x, int ch) {

    move(y, x);
    return addch(ch);

}

int mvaddstr(int y, int x, const char* str) {

    move(y, x);
    return addstr(str);

}

int printw(const char* fmt, ...) {

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

int mvprintw(int y, int x, const char* fmt, ...) {

    va_list ap;
    move(y, x);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

/*******************************************************************************

Input

*******************************************************************************/

int getch(void) {

    ami_evtrec er;

    /* check ungetch buffer first */
    if (cur_ungetch >= 0) {

        int ch = cur_ungetch;
        cur_ungetch = -1;
        return ch;

    }

    /* if timeout is set, use timer-based approach */
    if (cur_nodelay || cur_timeout_ms == 0) {

        /* non-blocking: check for event without waiting */
        /* Ami doesn't have a non-blocking event check, so we use a
           short timer. For simplicity, return ERR immediately. */
        return ERR;

    }

    /* blocking wait for event */
    while (1) {

        ami_event(stdin, &er);
        switch (er.etype) {

            case ami_etchar:   return er.echar;
            case ami_etup:     return KEY_UP;
            case ami_etdown:   return KEY_DOWN;
            case ami_etleft:   return KEY_LEFT;
            case ami_etright:  return KEY_RIGHT;
            case ami_ethome:   return KEY_HOME;
            case ami_ethomel:  return KEY_HOME;
            case ami_etend:    return KEY_END;
            case ami_etendl:   return KEY_END;
            case ami_etpagu:   return KEY_PPAGE;
            case ami_etpagd:   return KEY_NPAGE;
            case ami_etenter:  return '\n';
            case ami_etdel:    return KEY_BACKSPACE;
            case ami_etdelcf:  return KEY_DC;
            case ami_etdelcb:  return KEY_BACKSPACE;
            case ami_etinsertt: return KEY_IC;
            case ami_etterm:   return ERR;
            case ami_etfun:    return KEY_F(er.fkey);
            case ami_etresize:
                LINES = ami_maxy(stdout);
                COLS  = ami_maxx(stdout);
                return KEY_RESIZE;
            default:           break; /* ignore other events, loop */

        }

    }

}

int nodelay(WINDOW* win, int bf) {

    (void)win;
    cur_nodelay = bf;
    return OK;

}

int timeout(int delay) {

    cur_timeout_ms = delay;
    if (delay == 0) cur_nodelay = 1;
    else cur_nodelay = 0;
    return OK;

}

int keypad(WINDOW* win, int bf) {

    (void)win;
    cur_keypad = bf;
    return OK;

}

int cbreak(void)   { return OK; } /* Ami is always in cbreak mode */
int nocbreak(void)  { return OK; }
int raw(void)       { return OK; }
int noraw(void)     { return OK; }
int echo(void)      { cur_echo = 1; return OK; }
int noecho(void)    { cur_echo = 0; return OK; }

int halfdelay(int tenths) {

    cur_timeout_ms = tenths * 100;
    cur_nodelay = 0;
    return OK;

}

int ungetch(int ch) {

    cur_ungetch = ch;
    return OK;

}

/*******************************************************************************

Attributes

*******************************************************************************/

int attron(int attrs) {

    cur_attrs |= (attr_t)attrs;
    apply_attrs();
    return OK;

}

int attroff(int attrs) {

    cur_attrs &= ~(attr_t)attrs;
    apply_attrs();
    return OK;

}

int attrset(int attrs) {

    cur_attrs = (attr_t)attrs;
    apply_attrs();
    return OK;

}

/*******************************************************************************

Color

*******************************************************************************/

int has_colors(void) { return TRUE; }

int start_color(void) {

    /* Ami always has color support */
    return OK;

}

int init_pair(short pair, short fg, short bg) {

    if (pair < 0 || pair >= COLOR_PAIRS) return ERR;
    color_pairs[pair].fg = fg;
    color_pairs[pair].bg = bg;
    return OK;

}

/*******************************************************************************

Cursor

*******************************************************************************/

int curs_set(int visibility) {

    ami_curvis(stdout, visibility > 0 ? 1 : 0);
    return OK;

}

/*******************************************************************************

Screen size

*******************************************************************************/

int getmaxx(WINDOW* win) { (void)win; return COLS; }
int getmaxy(WINDOW* win) { (void)win; return LINES; }

/* mvinch: move to position and return the character there.
   Ami doesn't have a "read character at position" function, so we
   return ' ' as a fallback. The snake game uses this to check if a
   position is empty — we track the screen content in a simple buffer. */

int mvinch(int y, int x) {

    if (!scrbuf_inited) scrbuf_init();
    if (y < 0 || y >= 256 || x < 0 || x >= 256) return ' ';
    return (unsigned char)scrbuf[y][x];

}

/*******************************************************************************

Miscellaneous

*******************************************************************************/

int napms(int ms) {

    usleep(ms * 1000);
    return OK;

}

int beep(void) { return OK; }
int baudrate(void) { return 38400; }
int scrollok(WINDOW* win, int bf) { (void)win; (void)bf; return OK; }
int touchwin(WINDOW* win) { (void)win; return OK; }
int leaveok(WINDOW* win, int bf) { (void)win; (void)bf; return OK; }
int standout(void) { return attron(A_STANDOUT); }
int standend(void) { return attrset(A_NORMAL); }
int nl(void) { return OK; }
int nonl(void) { return OK; }
char erasechar(void) { return '\b'; }
char killchar(void) { return 0x15; /* Ctrl-U */ }

int winch(WINDOW* win) {

    int cx, cy;
    (void)win;
    if (!scrbuf_inited) scrbuf_init();
    cx = ami_curx(stdout) - 1;
    cy = ami_cury(stdout) - 1;
    if (cy < 0 || cy >= 256 || cx < 0 || cx >= 256) return ' ';
    return (unsigned char)scrbuf[cy][cx];

}

/*******************************************************************************

Box drawing

*******************************************************************************/

int box(WINDOW* win, int verch, int horch) {

    int x, y;

    (void)win;
    if (!verch) verch = ACS_VLINE;
    if (!horch) horch = ACS_HLINE;

    /* top row */
    move(0, 0);
    addch(ACS_ULCORNER);
    for (x = 1; x < COLS - 1; x++) addch(horch);
    addch(ACS_URCORNER);

    /* side lines */
    for (y = 1; y < LINES - 1; y++) {

        mvaddch(y, 0, verch);
        mvaddch(y, COLS - 1, verch);

    }

    /* bottom row */
    move(LINES - 1, 0);
    addch(ACS_LLCORNER);
    for (x = 1; x < COLS - 1; x++) addch(horch);
    addch(ACS_LRCORNER);

    return OK;

}

int hline(int ch, int n) {

    int i;
    if (!ch) ch = ACS_HLINE;
    for (i = 0; i < n; i++) addch(ch);
    return OK;

}

int vline(int ch, int n) {

    int cx = ami_curx(stdout) - 1; /* back to 0-based */
    int cy = ami_cury(stdout) - 1;
    int i;

    if (!ch) ch = ACS_VLINE;
    for (i = 0; i < n; i++) {

        mvaddch(cy + i, cx, ch);

    }
    return OK;

}

int mvhline(int y, int x, int ch, int n) { move(y, x); return hline(ch, n); }
int mvvline(int y, int x, int ch, int n) { move(y, x); return vline(ch, n); }

/*******************************************************************************

Window functions (minimal — all map to stdscr with an origin offset)

Curses windows are mapped to stdscr with a stored origin. Drawing calls
offset by the window's begin_y/begin_x. This is sufficient for programs
that use a small number of non-overlapping windows (like worm's status
bar + game area).

*******************************************************************************/

#define MAX_WINS 16
static struct { int used; int by; int bx; int ny; int nx; } wins[MAX_WINS];

WINDOW* newwin(int nlines, int ncols, int begin_y, int begin_x) {

    int i;
    for (i = 1; i < MAX_WINS; i++) {

        if (!wins[i].used) {

            wins[i].used = 1;
            wins[i].by = begin_y;
            wins[i].bx = begin_x;
            wins[i].ny = nlines;
            wins[i].nx = ncols;
            return (WINDOW*)((long)i);

        }

    }
    return stdscr;

}

static void win_origin(WINDOW* win, int* oy, int* ox) {

    long idx = (long)win;
    if (idx > 0 && idx < MAX_WINS && wins[idx].used) {

        *oy = wins[idx].by;
        *ox = wins[idx].bx;

    } else {

        *oy = 0;
        *ox = 0;

    }

}

int wrefresh(WINDOW* win) { (void)win; return refresh(); }

int wmove(WINDOW* win, int y, int x) {

    int oy, ox;
    win_origin(win, &oy, &ox);
    return move(oy + y, ox + x);

}

int waddch(WINDOW* win, int ch) { (void)win; return addch(ch); }
int waddstr(WINDOW* win, const char* str) { (void)win; return addstr(str); }

int mvwaddch(WINDOW* win, int y, int x, int ch) {

    wmove(win, y, x);
    return addch(ch);

}

int mvwaddstr(WINDOW* win, int y, int x, const char* str) {

    wmove(win, y, x);
    return addstr(str);

}

int wprintw(WINDOW* win, const char* fmt, ...) {

    va_list ap;
    (void)win;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

int mvwprintw(WINDOW* win, int y, int x, const char* fmt, ...) {

    va_list ap;
    wmove(win, y, x);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return OK;

}

int wclear(WINDOW* win) { (void)win; return clear(); }
int wclrtoeol(WINDOW* win) { (void)win; return clrtoeol(); }
int wgetch(WINDOW* win) { (void)win; return getch(); }
