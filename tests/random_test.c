/*******************************************************************************
*                                                                              *
*                             RANDOM TORTURE TEST                              *
*                                                                              *
* Runs random procedures of the Petit-Ami API, on several threads at once,     *
* until it is cancelled. The point is not what the screen looks like: nothing  *
* the test draws is checked. The point is that the library survives any        *
* sequence of calls from any number of threads, so the test's only judgement   *
* is that it is still running. A fault is the library's error message, a       *
* crash or a hang, and the seed and the narration (-v) replay the run that    *
* found it.                                                                    *
*                                                                              *
* Each test is one case of a large switch, drawn from the API of each module   *
* and from the individual tests, terminal_test, graphics_test, window_test     *
* and the rest: text, attributes and colors, fonts, cursor motion and          *
* scrolling, the figures, buffers, child windows -- opened at random places    *
* and sizes, with random tests run inside them, recursively -- the network,    *
* against a message server on a thread of this program, and, asked for, the    *
* synthesizer. Every parameter is random within the range the window or        *
* buffer allows.                                                               *
*                                                                              *
* A window has modes, and the API refuses some calls in some modes: the fonts  *
* and justified text cannot change with auto on, the buffer cannot be sized    *
* or switched with buffering off. Those refusals are correct, so the test      *
* never makes them. A thread keeps the modes of the windows it owns and picks  *
* only tests its current modes allow. By default each worker thread owns a     *
* toplevel window of its own, and every test is open to it there. With -w the  *
* workers share the main window, and then their tests are one group of the     *
* mode groups below, whose members never change a mode and never need one      *
* another does not: the shared window's modes are set once for the group.      *
*                                                                              *
* Each thread has a random stream of its own, seeded from the run's seed and   *
* the thread's number, so a run is reproduced from its seed alone, thread by   *
* thread; the interleaving between threads is the system's and is the thing    *
* under test. The main thread runs the event loop: it keeps the count in the   *
* title, and the terminate event -- the close button, control-c in the        *
* window -- stops the workers and ends the run.                                *
*                                                                              *
*******************************************************************************/

/*******************************************************************************

Usage:

    random_test [-t threads] [-w] [-m group] [-s] [-v] [-p] [seed]

    -t     The number of worker threads, 4 by default; 1 runs single
           threaded.
    -w     The workers share the main window, instead of each owning a
           toplevel window of its own. The tests are then one mode group.
    -m     The mode group, for -w (the default group is draw):
             draw    text, attributes, colors, cursor motion, scrolling,
                     tabs, the figures, child windows: what the default
                     modes, auto on and buffer on, allow, and nothing that
                     changes a mode or the window's geometry
             font    the fonts, font sizes and justified text, with the
                     drawing, auto off for the run
             buffer  the buffer sizing, switching and block copies, with
                     the drawing
             window  child windows, with the drawing; the children are
                     each thread's own, and take every test
             all     everything, modes included: only for windows that
                     are one thread's own, which is the default
    -s     Include the synthesizer: random notes and instruments on the
           first synthesizer output. Off unless asked for, since it plays.
    -v     Narrate: each thread names each test on the error channel as it
           starts it, so the last line names the test a fault was in.
    -p     Report the count on the error channel every ten seconds, for a
           run watched from a script.
    -n     Stop after about this many tests, for a bounded run (a leak
           check under a sanitizer); 0, the default, runs until cancelled.
    -P     The message echo server's port, 4919 by default. Two runs on one
           machine need different ports.
    seed   The seed. Left off, or 0, one is taken from the clock. The seed
           is always reported on the error channel, so any run can be
           replayed.

The run ends on the terminate event: close a window, or control-c in one.
A library error goes where the configuration sends it, the error dialog by
default; dialogerr 0 in a petit_ami.cfg of the current directory sends it
to the error channel instead, which a scripted run wants.

*******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include <localdefs.h>
#include <services.h>
#include <sound.h>
#include <network.h>
#include <graphics.h>

#define MAXTHREADS 32   /* worker threads at most */
#define MAXDEPTH   3    /* child windows within child windows */
#define MAXCHILD   30   /* tests run in a child window at most */
#define NETPORT    4919 /* the message echo server's port, by default */
#define MAXMSG     1000 /* longest message exchanged */
#define MAXSTR     80   /* longest string written */
#define MAXBUF     2000 /* largest buffer dimension asked for, in pixels */
#define STATTIM    10   /* the timer the count runs on */
#define STATTIME   10000 /* its period, a second */
#define STALLSECS 120 /* seconds without a completed test that is a stall */
#define MAXWID     100  /* the highest window id the library takes */

/* the tests: one case each */
typedef enum {

    tchar, tstring, tcursor, tcursorg, thome, tmove, tdel, tnewline, tclear,
    tattr, tcolor, tcolorg, tfont, tfontsiz, ttab, tscroll, tscrollg, tauto,
    tcurvis, twrtstr, tjust,
    tline, tlinewidth, tlinestyle, trect, tfrect, trrect, tfrrect, tellipse,
    tfellipse, tarc, tfarc, tfchord, ttriangle, tpixel, tmode,
    tselect, tsizbuf, tsizbufg, tblockcopy, tbuffer, tchild, tchildpos,
    tchildsiz, tchildorder, tchildframe, ttitle, tview,
    tnet, tsound,
    tmax /* the count */

} testcod;

static const char* testnam[] = {

    "char", "string", "cursor", "cursorg", "home", "move", "del", "newline",
    "clear", "attr", "color", "colorg", "font", "fontsiz", "tab", "scroll",
    "scrollg", "auto", "curvis", "wrtstr", "just",
    "line", "linewidth", "linestyle", "rect", "frect", "rrect", "frrect",
    "ellipse", "fellipse", "arc", "farc", "fchord", "triangle", "pixel", "mode",
    "select", "sizbuf", "sizbufg", "blockcopy", "buffer", "child", "childpos",
    "childsiz", "childorder", "childframe", "title", "view",
    "net", "sound"

};

/* The mode groups a test belongs to, as bits, and what a test needs of the
   window's modes. A test that changes a mode belongs to no shared group. */
#define GDRAW   1  /* draw: nothing modal about it */
#define GFONT   2  /* font: needs auto off */
#define GBUF    4  /* buffer: needs buffering on */
#define GWIN    8  /* window: child windows and their controls */
#define GALL    (GDRAW|GFONT|GBUF|GWIN)
#define NAUTOFF 16 /* the window's auto must be off */
#define NBUFON  32 /* the window's buffer must be on */
#define NCHILD  64 /* the window must be a child window */
#define NMODAL  128 /* changes a mode: only on a window that is one's own */

static const int testgrp[tmax] = {

    /* char string cursor cursorg home move del newline clear */
    GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW,
    /* attr color colorg */
    GDRAW, GDRAW, GDRAW,
    /* font fontsiz: auto off */
    GFONT|NAUTOFF, GFONT|NAUTOFF,
    /* tab: buffer on, since unbuffered the buffer follows the window, on
       the event thread's time, and a tab is judged against it */
    GDRAW|NBUFON,
    /* scroll scrollg */
    GDRAW, GDRAW,
    /* auto: a mode; and buffered only, since unbuffered the screen follows
       the window on the event thread's time, and auto coming back checks
       the cursor against it */
    NMODAL|NBUFON,
    /* curvis */
    GDRAW,
    /* wrtstr: auto off */
    GFONT|NAUTOFF,
    /* just: auto off */
    GFONT|NAUTOFF,
    /* line linewidth linestyle rect frect rrect frrect ellipse fellipse */
    GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW,
    /* arc farc fchord triangle pixel mode */
    GDRAW, GDRAW, GDRAW, GDRAW, GDRAW, GDRAW,
    /* select sizbuf sizbufg blockcopy: buffer on, and the buffer group's
       own: they change the window's geometry under the other threads,
       which is a mode of its own */
    GBUF|NBUFON, GBUF|NBUFON, GBUF|NBUFON, GBUF|NBUFON,
    /* buffer: a mode */
    NMODAL,
    /* child */
    GDRAW|GWIN,
    /* childpos childsiz childorder childframe: on a child */
    GWIN|NCHILD, GWIN|NCHILD, GWIN|NCHILD, GWIN|NCHILD,
    /* title view */
    GDRAW, GDRAW,
    /* net sound */
    GDRAW, GDRAW

};

/* a thread's state: its random stream, and the window its tests are on */
typedef struct {

    int                thread; /* thread number, 1-n */
    unsigned long long rs;     /* random stream state */
    FILE*              f;      /* the window under test */
    int                depth;  /* child window depth, 0 on a toplevel */
    int                child;  /* the window is a child, this thread's own */
    int                own;    /* the window is this thread's own: its
                                  modes are the thread's to change */
    int                groups; /* the mode groups the tests come from */
    int                autoon; /* the update buffer's auto mode is on */
    int                bufon;  /* the window's buffer is on */
    ami_long           fsiz0;  /* the window's font size as opened */
    unsigned long long count;  /* tests run */

} ctx;

/* the run's settings */
static int                threads = 4;    /* worker threads */
static int                shared = FALSE; /* the workers share the main window */
static int                groups = GALL;  /* the mode groups, for a shared window */
static int                sound = FALSE;  /* the synthesizer is included */
static int                verbose = FALSE; /* narrate each test */
static int                progress = FALSE; /* the count on the error channel */
static unsigned long long limit = 0; /* stop after this many tests, 0 = never */
static int                netport = NETPORT; /* the echo server's port: -P lets two runs coexist */
static unsigned long long seed;           /* the run's seed */

/* the run's state */
static volatile int  stop;          /* the workers are to stop */
static ami_long      lockid;        /* the run's lock */
static ami_long      sigid;         /* a worker stopped */
static int           running;       /* workers still running */
static int           started;       /* workers started, under the lock */
static ctx           ctxs[MAXTHREADS]; /* the threads' states */
static char          widused[MAXWID+1]; /* window ids in use, under the lock */
static ami_ulong     netaddr;       /* the loopback address */
static int           netok;         /* the echo server is up */

/*******************************************************************************

Random numbers

A stream per thread, xorshift, so that a run replays from its seed whatever
the platform's rand() does, and whatever the other threads do.

*******************************************************************************/

static unsigned long long rnd64(ctx* c)

{

    unsigned long long x = c->rs;

    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    c->rs = x;

    return (x*0x2545F4914F6CDD1DULL);

}

/* 0 to n-1 */
static ami_long rnd(ctx* c, ami_long n)

{

    if (n <= 1) return (0);

    return ((ami_long)(rnd64(c)%(unsigned long long)n));

}

/* lo to hi inclusive */
static ami_long rndr(ctx* c, ami_long lo, ami_long hi)

{

    if (hi <= lo) return (lo);

    return (lo+rnd(c, hi-lo+1));

}

/* a coordinate in the window, 1 to its extent */
static ami_long rndx(ctx* c) { return (rndr(c, 1, ami_maxxg(c->f))); }
static ami_long rndy(ctx* c) { return (rndr(c, 1, ami_maxyg(c->f))); }

/* a ratioed value, 0 to LONG_MAX, as colors and angles take */
static ami_long rndratio(ctx* c) { return ((ami_long)(rnd64(c) & LONG_MAX)); }

/* a random printable string */
static void rndstr(ctx* c, char* s, int max)

{

    int i, n = (int)rndr(c, 1, max);

    for (i = 0; i < n; i++) s[i] = (char)rndr(c, ' ', '~');
    s[n] = 0;

}

/*******************************************************************************

The message echo server

Runs on a thread of its own for the life of the program: takes the port
once, and sends back whatever it is sent, to whoever sent it. The network
test's exchange is against it. The socket is one for the run: a message sent
while the port was between one binding and the next would be lost, and the
sender's read of the reply would time out, which the library reports as a
fault. A read waits for the message to be there first, since the read
itself gives up after a while.

*******************************************************************************/

static void echoserver(void)

{

    ami_long fn, len;
    char     buf[MAXMSG];

    fn = ami_waitmsg(netport, FALSE);
    for (;;) {

        while (!ami_rdymsg(fn, 100000)) ; /* until a message is there */
        len = ami_rdmsg(fn, buf, MAXMSG);
        if (len > 0) ami_wrmsg(fn, buf, len);

    }

}

/*******************************************************************************

The tests

Each takes the thread's state and does one random thing to its window.

*******************************************************************************/

static void runtests(ctx* c, int n); /* forward */

/* a window id of the run's own, unique across the threads: the library
   allows ids up to MAXWID, so they are reused as windows close; 1 is the
   main window's */
static ami_long newwid(void)

{

    ami_long wid;

    ami_lock(lockid);
    for (wid = 2; wid <= MAXWID && widused[wid]; wid++) ;
    if (wid > MAXWID) {

        ami_unlock(lockid);
        fprintf(stderr, "random_test: out of window ids\n");
        exit(1);

    }
    widused[wid] = 1;
    ami_unlock(lockid);

    return (wid);

}

static void freewid(ami_long wid)

{

    ami_lock(lockid);
    widused[wid] = 0;
    ami_unlock(lockid);

}

/* a child window of the current window: opened at a random place and size,
   a random number of tests run in it, and closed. The child is the thread's
   own whatever the parent is, so its modes are the thread's to change and
   every test is open in it. */
static void childtest(ctx* c)

{

    ctx      cc;
    FILE*    win;
    ami_long px, py, w, h, wid;

    if (c->depth >= MAXDEPTH) return;
    wid = newwid();
    ami_openwin(&stdin, &win, c->f, wid);
    /* somewhere on the parent, some size that fits it */
    px = rndr(c, 1, ami_maxxg(c->f)/2);
    py = rndr(c, 1, ami_maxyg(c->f)/2);
    w = rndr(c, ami_chrsizx(win)*4, ami_maxxg(c->f)-px+1);
    h = rndr(c, ami_chrsizy(win)*2, ami_maxyg(c->f)-py+1);
    ami_setposg(win, px, py);
    ami_setsizg(win, w, h);
    cc = *c;
    cc.f = win;
    cc.depth = c->depth+1;
    cc.child = TRUE;
    cc.own = TRUE;
    cc.groups = GALL;
    cc.autoon = TRUE;
    cc.bufon = TRUE;
    cc.fsiz0 = ami_chrsizy(win);
    runtests(&cc, (int)rndr(c, 1, MAXCHILD));
    c->rs = cc.rs; /* the stream went on in the child */
    c->count = cc.count;
    fclose(win);
    freewid(wid);

}

/* the network exchange: a random message to the echo server and back. The
   server takes one exchange at a time, so the exchanges are one at a time. */
static void nettest(ctx* c)

{

    ami_long fn, len, i;
    char     buf[MAXMSG];

    if (!netok) return;
    len = rndr(c, 1, MAXMSG);
    for (i = 0; i < len; i++) buf[i] = (char)rnd(c, 256);
    ami_lock(lockid);
    fn = ami_openmsg(netaddr, netport, FALSE);
    ami_wrmsg(fn, buf, len);
    ami_rdmsg(fn, buf, MAXMSG);
    ami_clsmsg(fn);
    ami_unlock(lockid);

}

/* the synthesizer: a note on, a note off, or an instrument change, on a
   random channel */
static void soundtest(ctx* c)

{

    ami_long ch = rndr(c, 1, 16);

    switch (rnd(c, 3)) {

        case 0: ami_noteon(1, 0, ch, rndr(c, 1, 128), rndratio(c)); break;
        case 1: ami_noteoff(1, 0, ch, rndr(c, 1, 128), rndratio(c)); break;
        case 2: ami_instchange(1, 0, ch, rndr(c, 1, 128)); break;

    }

}

/* The screens were remade, or another selected. Auto is a mode of each
   screen, a remade screen taking the window's last setting and the others
   keeping theirs, and a screen made before a resize keeps its size: rather
   than model that, the test puts the update screen into a known state, auto
   off, which every mode allows, and lets the auto test bring it back. */
static void knownscreen(ctx* c)

{

    ami_auto(c->f, FALSE);
    c->autoon = FALSE;

}

/* is the test open on this window, in its modes, in this run */
static int allowed(ctx* c, testcod t)

{

    int g = testgrp[t];

    if (t == tsound && !sound) return (FALSE);
    if ((g & NMODAL) && !c->own) return (FALSE); /* a mode change: on one's own window */
    if (!(g & c->groups)) return (FALSE); /* not this run's group */
    if ((g & NAUTOFF) && c->autoon) return (FALSE);
    if ((g & NBUFON) && !c->bufon) return (FALSE);
    if ((g & NCHILD) && !c->child) return (FALSE);

    return (TRUE);

}

/* one random test */
static void test(ctx* c)

{

    FILE*    f = c->f;
    testcod  t;
    char     s[MAXSTR+1];
    ami_long x1, y1, x2, y2, x3, y3, xs, ys;

    do t = (testcod)rnd(c, tmax); while (!allowed(c, t));
    c->count++;
    if (verbose) {

        fprintf(stderr, "random_test: thread %d test %llu: %s\n",
                c->thread, c->count, testnam[t]);
        fflush(stderr);

    }
    switch (t) {

        /* text */
        case tchar:     fputc((int)rndr(c, ' ', '~'), f); break;
        case tstring:   rndstr(c, s, MAXSTR); fputs(s, f); break;
        case tcursor:   ami_cursor(f, rndr(c, 1, ami_maxx(f)), rndr(c, 1, ami_maxy(f)));
                        break;
        case tcursorg:  ami_cursorg(f, rndx(c), rndy(c)); break;
        case thome:     ami_home(f); break;
        case tmove:     switch (rnd(c, 4)) {

                            case 0: ami_up(f); break;
                            case 1: ami_down(f); break;
                            case 2: ami_left(f); break;
                            case 3: ami_right(f); break;

                        }
                        break;
        case tdel:      ami_del(f); break;
        case tnewline:  fputc('\n', f); break;
        case tclear:    fputc('\f', f); break;
        case tattr:     switch (rnd(c, 16)) {

                            case 0:  ami_bold(f, rnd(c, 2)); break;
                            case 1:  ami_italic(f, rnd(c, 2)); break;
                            case 2:  ami_underline(f, rnd(c, 2)); break;
                            case 3:  ami_strikeout(f, rnd(c, 2)); break;
                            case 4:  ami_standout(f, rnd(c, 2)); break;
                            case 5:  ami_reverse(f, rnd(c, 2)); break;
                            case 6:  ami_blink(f, rnd(c, 2)); break;
                            case 7:  ami_superscript(f, rnd(c, 2)); break;
                            case 8:  ami_subscript(f, rnd(c, 2)); break;
                            case 9:  ami_condensed(f, rnd(c, 2)); break;
                            case 10: ami_extended(f, rnd(c, 2)); break;
                            case 11: ami_xlight(f, rnd(c, 2)); break;
                            case 12: ami_light(f, rnd(c, 2)); break;
                            case 13: ami_xbold(f, rnd(c, 2)); break;
                            case 14: ami_hollow(f, rnd(c, 2)); break;
                            case 15: ami_raised(f, rnd(c, 2)); break;

                        }
                        break;
        case tcolor:    if (rnd(c, 2)) ami_fcolor(f, (ami_color)rnd(c, 8));
                        else ami_bcolor(f, (ami_color)rnd(c, 8));
                        break;
        case tcolorg:   if (rnd(c, 2))
                            ami_fcolorg(f, rndratio(c), rndratio(c), rndratio(c));
                        else
                            ami_bcolorg(f, rndratio(c), rndratio(c), rndratio(c));
                        break;
        case tfont:     ami_font(f, rndr(c, AMI_FONT_TERM, AMI_FONT_TECH)); break;
        case tfontsiz:  ami_fontsiz(f, rndr(c, 6, 48)); break;
        case ttab:      switch (rnd(c, 4)) {

                            /* the columns the font now makes of the
                               buffer: the character width reported
                               stays as the buffer was made */
                            case 0: ami_settab(f, rndr(c, 1, (ami_maxxg(f)-1)/ami_chrsizx(f)+1)); break;
                            case 1: ami_restab(f, rndr(c, 1, (ami_maxxg(f)-1)/ami_chrsizx(f)+1)); break;
                            case 2: ami_clrtab(f); break;
                            case 3: fputc('\t', f); break;

                        }
                        break;
        case tscroll:   ami_scroll(f, rndr(c, -3, 3), rndr(c, -3, 3)); break;
        case tscrollg:  ami_scrollg(f, rndr(c, -20, 20), rndr(c, -20, 20)); break;
        case tauto:     x1 = rnd(c, 2);
                        if (x1 == c->autoon) break; /* no change: nothing to do */
                        if (x1 && !c->autoon) {

                            /* auto comes back only on the standard grid:
                               the terminal font at the size the window
                               opened with, the cursor on the screen */
                            ami_viewscale(f, 1.0f, 1.0f);
                            ami_viewoffg(f, 0, 0);
                            ami_font(f, AMI_FONT_TERM);
                            ami_fontsiz(f, c->fsiz0);
                            ami_home(f);

                        }
                        ami_auto(f, x1);
                        c->autoon = (int)x1;
                        break;
        case tcurvis:   ami_curvis(f, rnd(c, 2)); break;
        case twrtstr:   rndstr(c, s, MAXSTR);
                        if (rnd(c, 2)) ami_wrtstr(f, s);
                        else ami_wrtstrn(f, s, rndr(c, 0, (ami_long)strlen(s)));
                        break;
        case tjust:     rndstr(c, s, MAXSTR);
                        x1 = ami_strsiz(f, s);
                        ami_writejust(f, s, rndr(c, x1, x1*2+1));
                        /* the positions count from zero, before the last */
                        ami_justpos(f, s, rndr(c, 0, (ami_long)strlen(s)-1), x1*2);
                        ami_chrpos(f, s, rndr(c, 0, (ami_long)strlen(s)-1));
                        break;

        /* figures */
        case tline:     ami_line(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tlinewidth: ami_linewidth(f, rndr(c, 1, 10)); break;
        case tlinestyle: ami_linestyle(f, (ami_lstyle)rnd(c, 3)); break;
        case trect:     ami_rect(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tfrect:    ami_frect(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case trrect:
        case tfrrect:   x1 = rndx(c); y1 = rndy(c); x2 = rndx(c); y2 = rndy(c);
                        xs = rndr(c, 0, labs(x2-x1)/2+1);
                        ys = rndr(c, 0, labs(y2-y1)/2+1);
                        if (t == trrect) ami_rrect(f, x1, y1, x2, y2, xs, ys);
                        else ami_frrect(f, x1, y1, x2, y2, xs, ys);
                        break;
        case tellipse:  ami_ellipse(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tfellipse: ami_fellipse(f, rndx(c), rndy(c), rndx(c), rndy(c)); break;
        case tarc:      ami_arc(f, rndx(c), rndy(c), rndx(c), rndy(c),
                                rndratio(c), rndratio(c));
                        break;
        case tfarc:     ami_farc(f, rndx(c), rndy(c), rndx(c), rndy(c),
                                 rndratio(c), rndratio(c));
                        break;
        case tfchord:   ami_fchord(f, rndx(c), rndy(c), rndx(c), rndy(c),
                                   rndratio(c), rndratio(c));
                        break;
        case ttriangle: x1 = rndx(c); y1 = rndy(c); x2 = rndx(c); y2 = rndy(c);
                        x3 = rndx(c); y3 = rndy(c);
                        ami_ftriangle(f, x1, y1, x2, y2, x3, y3);
                        break;
        case tpixel:    ami_setpixel(f, rndx(c), rndy(c)); break;
        case tmode:     switch (rnd(c, 10)) {

                            case 0: ami_fover(f); break;
                            case 1: ami_bover(f); break;
                            case 2: ami_finvis(f); break;
                            case 3: ami_binvis(f); break;
                            case 4: ami_fxor(f); break;
                            case 5: ami_bxor(f); break;
                            case 6: ami_fand(f); break;
                            case 7: ami_band(f); break;
                            case 8: ami_for(f); break;
                            case 9: ami_bor(f); break;

                        }
                        break;

        /* buffers and windows */
        case tselect:   ami_select(f, rndr(c, 1, 4), rndr(c, 1, 4));
                        /* the screen selected may date from before a
                           resize: bring it to the window's size */
                        ami_sizbufg(f, ami_maxxg(f), ami_maxyg(f));
                        knownscreen(c);
                        break;
        case tsizbuf:   ami_sizbuf(f, rndr(c, 1, MAXBUF/ami_chrsizx(f)),
                                   rndr(c, 1, MAXBUF/ami_chrsizy(f)));
                        knownscreen(c);
                        break;
        case tsizbufg:  ami_sizbufg(f, rndr(c, ami_chrsizx(f), MAXBUF),
                                    rndr(c, ami_chrsizy(f), MAXBUF));
                        knownscreen(c);
                        break;
        case tblockcopy: x1 = rndx(c); y1 = rndy(c); x2 = rndx(c); y2 = rndy(c);
                        ami_blockcopyg(f, rndr(c, 1, 4), rndr(c, 1, 4),
                                       x1, y1, x2, y2, rndx(c), rndy(c),
                                       rndx(c), rndy(c));
                        break;
        case tbuffer:   c->bufon = (int)rnd(c, 2); ami_buffer(f, c->bufon);
                        knownscreen(c);
                        break;
        case tchild:    childtest(c); break;
        case tchildpos: ami_setposg(f, rndr(c, 1, MAXBUF), rndr(c, 1, MAXBUF)); break;
        case tchildsiz: ami_setsizg(f, rndr(c, ami_chrsizx(f)*2, MAXBUF),
                                    rndr(c, ami_chrsizy(f)*2, MAXBUF));
                        break;
        case tchildorder: if (rnd(c, 2)) ami_front(f); else ami_back(f); break;
        case tchildframe: switch (rnd(c, 3)) {

                            case 0: ami_frame(f, rnd(c, 2)); break;
                            case 1: ami_sysbar(f, rnd(c, 2)); break;
                            case 2: ami_sizable(f, rnd(c, 2)); break;

                        }
                        break;
        case ttitle:    rndstr(c, s, MAXSTR); ami_title(f, s); break;
        case tview:     if (rnd(c, 2)) ami_viewoffg(f, rndr(c, -50, 50), rndr(c, -50, 50));
                        else {

                            float sx = (float)rndr(c, 5, 20)/10.0f;
                            float sy = (float)rndr(c, 5, 20)/10.0f;

                            ami_viewscale(f, sx, sy);

                        }
                        break;

        /* the other modules */
        case tnet:      nettest(c); break;
        case tsound:    soundtest(c); break;
        case tmax:      break;

    }

}

/* n tests, or until the stop */
static void runtests(ctx* c, int n)

{

    int i;

    for (i = 0; i < n && !stop; i++) test(c);

}

/*******************************************************************************

The worker thread

Takes the next thread's state, opens its window unless the run shares the
main one, runs tests until the stop, and reports itself stopped.

*******************************************************************************/

static void worker(void)

{

    ctx*  c;
    FILE* win = NULL;
    char  title[40];

    ami_lock(lockid);
    c = &ctxs[started++];
    ami_unlock(lockid);
    if (!shared) {

        /* a toplevel window of this thread's own */
        ami_openwin(&stdin, &win, NULL, newwid());
        /* its id is held for the run: the window lives as long */
        sprintf(title, "random_test thread %d", c->thread);
        ami_title(win, title);
        c->f = win;

    }
    c->fsiz0 = ami_chrsizy(c->f);
    while (!stop) test(c);
    if (win) fclose(win);
    ami_lock(lockid);
    running--;
    ami_sendsig(sigid);
    ami_unlock(lockid);

}

/*******************************************************************************

Main

*******************************************************************************/

int main(int argc, char* argv[])

{

    int                i, secs = 0;
    unsigned long long lasttot = 0; /* the count at the last second */
    int                stall = 0;   /* seconds without a test completing */
    ami_evtrec         er;
    unsigned long long total;
    char               title[80];
    const char*        groupnam = "draw";

    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "-t") && i+1 < argc) {

            threads = atoi(argv[++i]);
            if (threads < 1) threads = 1;
            if (threads > MAXTHREADS) threads = MAXTHREADS;

        } else if (!strcmp(argv[i], "-w")) shared = TRUE;
        else if (!strcmp(argv[i], "-m") && i+1 < argc) groupnam = argv[++i];
        else if (!strcmp(argv[i], "-s")) sound = TRUE;
        else if (!strcmp(argv[i], "-v")) verbose = TRUE;
        else if (!strcmp(argv[i], "-p")) progress = TRUE;
        else if (!strcmp(argv[i], "-n") && i+1 < argc) limit = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "-P") && i+1 < argc) netport = atoi(argv[++i]);
        else seed = strtoull(argv[i], NULL, 10);

    }
    if (shared) {

        if (!strcmp(groupnam, "draw")) groups = GDRAW;
        else if (!strcmp(groupnam, "font")) groups = GFONT;
        else if (!strcmp(groupnam, "buffer")) groups = GBUF;
        else if (!strcmp(groupnam, "window")) groups = GWIN;
        else if (!strcmp(groupnam, "all")) groups = GALL;
        else {

            fprintf(stderr, "random_test: no such mode group: %s\n", groupnam);
            return (1);

        }

    }
    if (!seed) seed = (unsigned long long)time(NULL)*2654435761ULL+
                      (unsigned long long)ami_clock();
    fprintf(stderr, "random_test: seed %llu, %d thread%s, %s%s%s\n", seed,
            threads, threads == 1? "": "s",
            shared? "the main window shared, group ": "a window each",
            shared? groupnam: "", sound? ", with sound": "");
    fflush(stderr);

    ami_autohold(FALSE); /* the run ends on the terminate event, not on its own */
    ami_curvis(stdout, FALSE);
    ami_title(stdout, "random_test");
    lockid = ami_initlock();
    sigid = ami_initsig();

    /* the message echo server, on its thread */
    ami_addrnet("127.0.0.1", &netaddr);
    ami_newthread(echoserver);
    netok = TRUE;

    /* the synthesizer, when asked for and there is one */
    if (sound) {

        if (ami_synthout() < 1) {

            fprintf(stderr, "random_test: no synthesizer output, sound left out\n");
            sound = FALSE;

        } else ami_opensynthout(1);

    }

    /* the shared window's modes are the group's, set once here: the font
       group needs auto off, and stays off */
    if (shared && groups == GFONT) ami_auto(stdout, FALSE);

    /* the workers: each with a stream of its own from the seed */
    for (i = 0; i < threads; i++) {

        ctxs[i].thread = i+1;
        ctxs[i].rs = seed ^ ((unsigned long long)(i+1)*0x9E3779B97F4A7C15ULL);
        if (!ctxs[i].rs) ctxs[i].rs = 1; /* xorshift must not start at zero */
        ctxs[i].f = stdout; /* the worker opens its own unless shared */
        ctxs[i].depth = 0;
        ctxs[i].child = FALSE;
        ctxs[i].own = !shared;
        ctxs[i].groups = shared? groups: GALL;
        ctxs[i].autoon = !(shared && groups == GFONT);
        ctxs[i].bufon = TRUE;
        ctxs[i].count = 0;

    }
    running = threads;
    for (i = 0; i < threads; i++) ami_newthread(worker);

    /* the event loop: the count in the title once a second, until the
       terminate event */
    ami_timer(stdout, STATTIM, STATTIME, TRUE);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_ettim && er.timnum == STATTIM) {

            total = 0;
            for (i = 0; i < threads; i++) total += ctxs[i].count;
            sprintf(title, "random_test: %llu tests", total);
            ami_title(stdout, title);
            /* a stall: no thread has completed a test in a while. A hang in
               the library looks like a quiet run otherwise; say so, in the
               title and on the error channel, and leave the process running
               for a debugger to attach to */
            if (total == lasttot) {

                if (++stall == STALLSECS) {

                    fprintf(stderr, "random_test: STALLED: no test completed in %d "
                                    "seconds at %llu tests, seed %llu, pid %d; "
                                    "left running for a debugger\n",
                            STALLSECS, total, (unsigned long long)seed, (int)getpid());
                    sprintf(title, "random_test: STALLED at %llu tests", total);
                    ami_title(stdout, title);

                }

            } else { stall = 0; lasttot = total; }
            if (limit && total >= limit) break; /* the bounded run for leak checks */
            if (progress && ++secs%10 == 0) {

                fprintf(stderr, "random_test: %llu tests\n", total);
                fflush(stderr);

            }

        }

    } while (er.etype != ami_etterm && !(limit && ({ unsigned long long tt = 0; for (i = 0; i < threads; i++) tt += ctxs[i].count; tt; }) >= limit));

    /* stop the workers, and wait for them to finish what they were in */
    stop = TRUE;
    ami_lock(lockid);
    while (running > 0) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);
    if (sound) ami_closesynthout(1);
    total = 0;
    for (i = 0; i < threads; i++) total += ctxs[i].count;
    fprintf(stderr, "random_test: seed %llu, %llu tests, stopped\n", seed, total);

    return (0);

}
