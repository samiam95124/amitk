/*******************************************************************************
*                                                                             *
*                     WINDOW MANAGEMENT TEST PROGRAM                          *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
*                                                                             *
* Tests text and graphical windows management calls. Every test applies to   *
* the window under test, tw: normally the program window itself, riding a    *
* desktop that can move and size it.                                          *
*                                                                             *
* With the -r (rooted) option the program window is instead treated as a     *
* desktop: the frame buffer backend's program window is the root surface,     *
* which cannot move, size or change Z order -- there is nothing behind it.    *
* The root then only carries a banner, tw is a child window on it, and        *
* tests that create child windows make them children of the test window, a   *
* child of a child. (window_testc will need the same treatment when a     *
* character based desktop arrives.)                                           *
*                                                                             *
*******************************************************************************/
/*******************************************************************************

Usage:

    window_test [auto [file]] [-r] [first [last]]

    auto   Walks every screen with no input and exits after them: the
           regression mode. A following non-numeric argument names the
           file the screens are captured to.
    -r     The rooted form: the program window serves as the desktop
           (the frame buffer backend's root, which cannot move or
           size), and every test applies to a child window on it.
    first  The first frame to stop on. The frames before it pass
           without a stop or capture.
    last   The last frame; the run ends after it. Left off, the run
           goes from the first frame to the end of the test.

Each frame stamps its number into the test window's title bar, so a
frame can be called out by number and revisited alone.

*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <services.h>
#include <graphics.h>

#define OFF 0
#define ON 1

/* Window-size round-trip tolerance. Wayland/XWayland compositors do not honor
   exact-pixel window sizes -- Mutter adjusts a client's size by a pixel or two
   for frame/geometry reasons -- so a setsiz/getsiz round trip can differ
   slightly from what was requested. Allow a small delta instead of demanding
   exact equality (native X11 WMs granted exact sizes and still pass). */
#define SIZTOLC 1  /* character-cell tolerance */
#define SIZTOLG 2  /* pixel tolerance          */
#define SIZOFF(a, b, tol) (labs((a) - (b)) > (tol))

static jmp_buf terminate_buf;
static FILE*      tw;           /* the window under test */
static int        rooted = FALSE; /* the program window is a desktop */
static ami_long   mainwid;      /* the test window's id */
static ami_long   wid2, wid3, wid4; /* the ids of the windows it opens */
static FILE*      win2;
static FILE*      win3;
static FILE*      win4;
static ami_long   x, x2, y, y2;
static ami_long   ox, oy;       /* original size of window */
static int        fb;           /* front/back flipper */
static ami_evtrec  er;
static ami_menuptr mp;           /* menu pointer */
static ami_menuptr ml;           /* menu list */
static int        framenum = 0;
static int        tstlo = 0;    /* first frame in the selected range */
static int        tsthi = 0;    /* last frame, 0 for no limit */
static ami_menuptr sm;           /* submenu list */
static int        sred;         /* state variables */
static int        sgreen;
static int        sblue;
static int        mincnt;       /* minimize counter */
static int        maxcnt;       /* maximize counter */
static int        nrmcnt;       /* normalize counter */
static int        i;
static ami_long   xs, ys;
static ami_long   mxs, mys;      /* maximum window size the WM will grant */
static ami_long   cs;
static ami_long   t, et;
static ami_color   c1, c2, c3;

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_fprintf(tw, dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_fprintf(tw, lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); \
                                fflush(stderr); } while (0)

extern void screen_capture(void);
extern void screen_capture_name(const char* fn);
extern void screen_capture_label(const char* s);
extern void auto_event_name(const char* fn);
extern void auto_event_beside(const char* capfile, const char* name);
extern void auto_event_frame(int frame, int step);
extern int  auto_event_ready(void);
extern void auto_event(FILE* f, ami_evtrec* er);

#define EVENTNAME "window_test.evt"    /* the events of the automatic run */
#define EVENTFILE "tests/" EVENTNAME   /* where they are from the repository */
/* the manager's composition hold, where the display has one: the fast
   forward to a selected frame passes its frames unseen */
extern void wg_hold(ami_long on) __attribute__((weak));
/* the display diagnostic, where the display offers one: at any wait,
   'd' compares the composed canvas against the glass */
extern void grx_glassdiff(void) __attribute__((weak));

/* "window_test auto" walks every screen with no input at all,
   capturing each, and exits at the end: this is how the regression runs
   it. Child windows and menus are windows of their own and paint from
   events, so an automatic run pumps events for a moment to let the screen
   settle before capturing it, then answers the wait with a return. */
static int autorun = FALSE;

#define AUTOSETL 3000 /* settle time before a capture, 100us units */
#define AUTOTIM  9    /* timer the settle runs on */

static void autosettle(void)

{

    ami_evtrec er;

    ami_timer(tw, AUTOTIM, AUTOSETL, FALSE);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_ettim || er.timnum != AUTOTIM);

}

/* Every wait for the user comes through here. An automatic run lets the
   screen settle and answers with the return the screen was waiting for;
   the capture is taken by waitnextt, which stamps the frame. */
static void nextevt(ami_evtrec* er)

{

    if (framenum < tstlo) {

        /* Before the selected range every wait answers at once. The
           gate is the frame count itself: the interactive loops that
           run between one mark and the next belong to the frame being
           approached, and they too pass -- only the arrival frame's
           own wait is real. */
        er->etype = ami_etenter;
        er->winid = mainwid;

        return;

    }
    if (autorun) {

        /* the event file's events, while it has them for this frame */
        if (auto_event_ready()) { auto_event(stdin, er); return; }
        autosettle();
        /* the return the screen waits for, from the main window: a wait
           that takes only its own window's return must see one */
        er->etype = ami_etenter;
        er->winid = mainwid;

        return;

    }
    auto_event(stdin, er); /* the user's, or the file's where it has them */

}

/* Go on to the next frame. Each pattern is a frame, and the frame number
   is advanced at the start of the pattern, so the title while it draws, the
   steps of an animation within it and the capture at its end all carry the
   same number. Past the selected range the test is over. */

static int stepnum; /* the step within the frame being drawn */

static void frmnext(void)

{

    framenum++;
    stepnum = 0; /* the frame's steps count from one */
    if (tsthi && framenum > tsthi) longjmp(terminate_buf, 1);
    auto_event_frame(framenum, 0);

}

/* Mark and capture the pattern just drawn: the frame number stamped into
   the title bar, unless the caller is testing ami_title itself (keeptitle
   preserves the title under test), the label as the picture's title, and
   the capture. Before the selected range nothing is captured. */

static void frmmark(int keeptitle)

{

    char titlebuf[80];

    if (framenum < tstlo) return; /* not in range */
    if (wg_hold) wg_hold(0); /* arriving: the display composes again */
    if (!keeptitle) {

        sprintf(titlebuf, "window_test: frame %d", framenum);
        ami_title(tw, titlebuf);

    }
    if (autorun) autosettle(); /* let the screen finish before it is taken */
    sprintf(titlebuf, "frame %d", framenum);
    screen_capture_label(titlebuf);
    screen_capture();
    if (grx_glassdiff) grx_glassdiff(); /* the canvas-against-glass check */

}

/* Mark and capture one step of a pattern driven by events, in an automatic
   run: stamped as the frame mark is, with the step as a fraction, frame 17.1,
   17.2 and so on, so the frame count itself and the frame range a run
   selects are untouched. Outside the selected range nothing is captured. */

static void frmstep(void)

{

    char titlebuf[80];

    if (framenum < tstlo) return; /* not in range */
    stepnum++;
    sprintf(titlebuf, "window_test: frame %d.%d", framenum, stepnum);
    ami_title(tw, titlebuf);
    autosettle();
    sprintf(titlebuf, "frame %d.%d", framenum, stepnum);
    screen_capture_label(titlebuf);
    screen_capture();
    auto_event_frame(framenum, stepnum);

}

/* wait return to be pressed, or handle terminate */

static void waitnextt(int keeptitle)

{

    ami_evtrec er; /* event record */

    frmmark(keeptitle); /* label and capture the pattern */
    do {

        nextevt(&er);
        if (er.etype == ami_etchar && er.echar == 'd' && grx_glassdiff)
            grx_glassdiff(); /* the canvas-against-glass diagnostic */

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
    frmnext(); /* on to the next pattern */

}

/* wait return to be pressed, or handle terminate (stamps the frame number) */

static void waitnext(void) { waitnextt(FALSE); }

/* wait return to be pressed, or handle terminate, while printing characters */

static void waitnextprint(void)

{

    ami_evtrec er; /* event record */

    do {

        nextevt(&er);
        if (er.etype == ami_etchar)
            fprintf(tw, "Window: %lld char: %c\n", AMI_LONG_CAST(er.winid), er.echar);

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

}

/* print centered string */

static void prtcen(ami_long y, const char* s)

{

   ami_cursor(tw, (ami_maxx(tw)/2)-(strlen(s)/2), y);
   fprintf(tw, "%s", s);

}

/* print centered string graphical */

static void prtceng(ami_long y, const char* s)

{

   ami_cursorg(tw, (ami_maxxg(tw)/2)-(ami_strsiz(tw, s)/2), y);
   fprintf(tw, "%s", s);

}

/* wait time in 100 microseconds */

static void waittime(int t)

{

    ami_evtrec er;

    if (framenum < tstlo) return; /* pauses skip outside the range */
    ami_timer(tw, 1, t, FALSE);
    do { ami_event(stdin, &er);
    } while (er.etype != ami_ettim && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

}

/* append a new menu entry to the given list */

static void appendmenu(ami_menuptr* list, ami_menuptr m)

{

    ami_menuptr lp;

    /* clear these links for insurance */
    m->next = NULL; /* clear next */
    m->branch = NULL; /* clear branch */
    if (!*list) *list = m; /* list empty, set as first entry */
    else { /* list non-empty */

        /* find last entry in list */
        lp = *list; /* index 1st on list */
        while (lp->next) lp = lp->next;
        lp->next = m; /* append at end */

    }

}

/* create menu entry */

static void newmenu(ami_menuptr* mp, int onoff, int oneof, int bar,
             int id, const string face)

{

    *mp = malloc(sizeof(ami_menurec));
    if (!*mp) ami_alert("mantst", "Out of memory");
    (*mp)->onoff = onoff;
    (*mp)->oneof = oneof;
    (*mp)->bar = bar;
    (*mp)->id = id;
    (*mp)->face = malloc(strlen(face));
    if (!*mp) ami_alert("mantst", "Out of memory");
    strcpy((*mp)->face, face);

}

/* draw a character grid */

static void chrgrid(void)

{

    int x, y;

    ami_fcolor(tw, ami_yellow);
    y = 1;
    while (y < ami_maxyg(tw)) {

        ami_line(tw, 1, y, ami_maxxg(tw), y);
        y = y+ami_chrsizy(tw);

    }
    x = 1;
    while (x < ami_maxxg(tw)) {

        ami_line(tw, x, 1, x, ami_maxyg(tw));
        x = x+ami_chrsizx(tw);

    }
    ami_fcolor(tw, ami_black);

}

/* display frame test */

static void frameinside(const string s, ami_long x, ami_long y)

{

    fputc('\f', tw);
    ami_fcolor(tw, ami_cyan);
    ami_rect(tw, 1, 1, x, y);
    ami_line(tw, 1, 1, x, y);
    ami_line(tw, 1, y, x, 1);
    ami_fcolor(tw, ami_black);
    ami_binvis(tw);
    fprintf(tw, "%s\n", s);
    ami_bover(tw);

}

static void frmmark(int keeptitle);
static void frmnext(void);

static void frametest(const string s)

{

    ami_evtrec er;
    ami_long  x, y;

    x = ami_maxxg(tw); /* set size */
    y = ami_maxyg(tw);
    frameinside(s, x, y);
    frmmark(FALSE); /* a frame of its own, as the buffered patterns have */
    do {

        nextevt(&er); /* get next event */
        if (er.etype == ami_etredraw) frameinside(s, x, y);
        if (er.etype == ami_etresize) {

            /* Save the new dimensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = ami_maxxg(tw);
            y = ami_maxyg(tw);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    frmnext(); /* on to the next pattern */

}

/* Finds the largest square that fits into the screen, then applies a ratio to
   that. Used to determine a relative size that fits the screen. */
static void sqrrat(ami_long* xs, ami_long* ys, float rat)

{

    /* ratio by screen smallest x-y, then square it up */
    ami_getsizg(tw, xs, ys);
    if (*xs > *ys) { *ys /= rat; *xs = *ys; } /* square */
    else { *xs /= rat; *ys = *xs; }

}

static ami_color nextcolor(ami_color c)

{

    c++;
    if (c > ami_magenta) c = ami_red;

    return (c);

}

int main(int argc, char* argv[])

{

    ami_long xr;

    if (setjmp(terminate_buf)) goto terminate;

    /* "window_test auto" runs every screen with no input, for the
       regression; it ends when the screens do. A following name is the
       file the screens are captured to, so runs beside each other do
       not write over one another. "-r" runs the rooted form: the
       program window serves as the desktop and every test applies to a
       child window on it. */
    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "-r")) rooted = TRUE;
        else if (!strcmp(argv[i], "auto")) {

            autorun = TRUE;
            ami_autohold(FALSE);
            auto_event_name(EVENTFILE);

        } else if (!strcmp(argv[i], "events")) {

            auto_event_name(EVENTFILE); /* the file in an interactive run */

        } else if (argv[i][0] >= '0' && argv[i][0] <= '9') {

            /* "window_test [first [last]]" runs the numbered frames
               alone: the frames before the first pass without a stop,
               and the run ends after the last. With no last it runs
               from the first frame to the end of the test. */
            if (!tstlo) tstlo = atoi(argv[i]);
            else tsthi = atoi(argv[i]);

        } else if (autorun) {

            /* the capture file, and the event file beside it */
            screen_capture_name(argv[i]);
            auto_event_beside(argv[i], EVENTNAME);

        }

    }
    if (tstlo < 1) tstlo = 1;
    frmnext(); /* the first frame */

    /* a selected start frame arrives directly: the frames before it
       pass with composition held, where the manager offers the hold */
    if (tstlo > 1 && wg_hold) wg_hold(1);

    /* the test window and its windows: the program window and 2..4, or a
       child window and 3..5 when the program window is the desktop */
    mainwid = 1; wid2 = 2; wid3 = 3; wid4 = 4;
    if (rooted) { mainwid = 2; wid2 = 3; wid3 = 4; wid4 = 5; }

    if (rooted) {

        /* The program window is the desktop for this test -- on the frame
           buffer backend it is the root surface, which cannot move or
           size; everything below is applied to a child window on it */
        ami_curvis(stdout, OFF);
        ami_auto(stdout, OFF);
        fprintf(stdout, "\f");
        fprintf(stdout, "Graphical window management test -- this window is the desktop\n");

        /* open the window under test, a standard 80x25 terminal surface */
        ami_openwin(&stdin, &tw, stdout, mainwid);
        ami_winclient(tw, 80, 25, &x, &y,
                      BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
        ami_setsiz(tw, x, y);
        ami_setpos(tw, 2, 2);
        ami_sizbuf(tw, 80, 25);

    } else tw = stdout; /* the program window is under test */

    ami_auto(tw, OFF);
    ami_curvis(tw, OFF);
    fprintf(tw, "Managed screen test vs. 0.1\n");
    fprintf(tw, "\n");
    ami_scnsiz(tw, &x, &y);
    fprintf(tw, "Screen size character: x: %lld y: %lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_scnsizg(tw, &x, &y);
    fprintf(tw, "Screen size pixel: x: %lld y: %lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    fprintf(tw, "\n");
    ami_getsiz(tw, &x, &y);
    fprintf(tw, "Window size character: x: %lld y: %lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_getsizg(tw, &ox, &oy);
    fprintf(tw, "Window size graphical: x: %lld y: %lld\n", AMI_LONG_CAST(ox), AMI_LONG_CAST(oy));
    fprintf(tw, "\n");
    fprintf(tw, "Client size character: x: %lld y: %lld\n", AMI_LONG_CAST(ami_maxx(tw)), AMI_LONG_CAST(ami_maxy(tw)));
    fprintf(tw, "Client size graphical: x: %lld y: %lld\n", AMI_LONG_CAST(ami_maxxg(tw)), AMI_LONG_CAST(ami_maxyg(tw)));
    fprintf(tw, "\n");
    fprintf(tw, "Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Window titling test ************************** */

    ami_title(tw, "This is a mangement test window");
    fprintf(tw, "The title bar of this window should read: This is a mangement test window\n");
    prtceng(ami_maxyg(tw)-ami_chrsizy(tw), "Window title test");
    waitnextt(TRUE); /* keep the title we just set -- this frame IS the title test */

    /* ************************** Multiple windows ************************** */

    fputc('\f', tw);
    ami_curvis(tw, ON);
    prtceng(ami_maxyg(tw)-ami_chrsizy(tw), "Multiple window test");
    ami_home(tw);
    ami_auto(tw, ON);
    fprintf(tw, "This is the main window");
    fprintf(tw, "\n");
    fprintf(tw, "Select back and forth between each window, and make sure the\n");
    fprintf(tw, "cursor follows\n");
    fprintf(tw, "\n");
    fprintf(tw, "Here is the cursor->");
    ami_openwin(&stdin, &win2, NULL, wid2);
    if (rooted) {

        /* a standard 80x25 terminal like the test window, offset on the
           desktop so both stay reachable */
        ami_winclient(win2, 80, 25, &x, &y,
                      BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
        ami_setsiz(win2, x, y);
        ami_setpos(win2, 6, 6);
        ami_sizbuf(win2, 80, 25);

    }
    fprintf(win2, "This is the second window\n");
    fprintf(win2, "\n");
    fprintf(win2, "Here is the cursor->");
    waitnext();
    fprintf(tw, "\n");
    fprintf(tw, "Now enter characters to each window, then end with return\n");
    waitnextprint();
    fclose(win2);
    fputc('\f', tw);
    fprintf(tw, "Second window now closed\n");
    waitnext();
    ami_curvis(tw, OFF);
    ami_auto(tw, OFF);

    /* ********************* Resize buffer window character ******************** */

    ox = ami_maxx(tw);
    oy = ami_maxy(tw);
    ami_bcolor(tw, ami_white);
    ami_sizbuf(tw, 50, 50);
    ami_bcolor(tw, ami_cyan);
    fputc('\f', tw);
    for (x = 1; x <= ami_maxx(tw); x++) fprintf(tw, "*");
    ami_cursor(tw, 1, ami_maxy(tw));
    for (x = 1; x <= ami_maxx(tw); x++) fprintf(tw, "*");
    for (y = 1; y <= ami_maxy(tw); y++) { ami_cursor(tw, 1, y); fprintf(tw, "*"); }
    for (y = 1; y <= ami_maxy(tw); y++) { ami_cursor(tw, ami_maxx(tw), y); fprintf(tw, "*"); }
    ami_home(tw);
    fprintf(tw, "Buffer should now be 50 by 50 characters, and\n");
    fprintf(tw, "painted blue\n");
    fprintf(tw, "maxx: %lld maxy: %lld\n", AMI_LONG_CAST(ami_maxx(tw)), AMI_LONG_CAST(ami_maxy(tw)));
    fprintf(tw, "Open up window to verify this\n");
    prtcen(ami_maxy(tw), "Buffer resize character test\n");
    ami_bcolor(tw, ami_white);
    waitnext();
    ami_sizbuf(tw, ox, oy);

    /* *********************** Resize buffer window pixel ********************** */

    ox = ami_maxxg(tw);
    oy = ami_maxyg(tw);
    sqrrat(&xs, &ys, 1.3); /* find square ratio */
    ami_bcolor(tw, ami_white);
    ami_sizbufg(tw, xs, ys);
    ami_bcolor(tw, ami_cyan);
    fputc('\f', tw);
    ami_linewidth(tw, 20);
    ami_line(tw, 1, 1, ami_maxxg(tw), 1);
    ami_line(tw, 1, 1, 1, ami_maxyg(tw));
    ami_line(tw, 1, ami_maxyg(tw), ami_maxxg(tw), ami_maxyg(tw));
    ami_line(tw, ami_maxxg(tw), 1, ami_maxxg(tw), ami_maxyg(tw));
    fprintf(tw, "Buffer should now be %lld by %lld pixels, and\n", AMI_LONG_CAST(xs), AMI_LONG_CAST(ys));
    fprintf(tw, "painted blue\n");
    fprintf(tw, "maxxg: %lld maxyg: %lld\n", AMI_LONG_CAST(ami_maxxg(tw)), AMI_LONG_CAST(ami_maxyg(tw)));
    fprintf(tw, "Open up window to verify this\n");
    prtcen(ami_maxy(tw), "Buffer resize graphical test");
    ami_bcolor(tw, ami_white);
    waitnext();
    ami_sizbufg(tw, ox, oy);

    /* ****************** Resize screen with buffer on character *************** */

    ox = ami_maxxg(tw);
    oy = ami_maxyg(tw);
    for (x = 20; x <= 80; x++) {

        ami_setsiz(tw, x, 25);
        ami_getsiz(tw, &x2, &y2);
        if (SIZOFF(x2, x, SIZTOLC) || SIZOFF(y2, 25, SIZTOLC)) {

            ami_setsiz(tw, 80, 25);
            fputc('\f', tw);
            fprintf(tw, "*** Getsiz does not match setsiz, x: %lld y: %lld vs. x: %lld y: %d\n",
                   AMI_LONG_CAST(x2), AMI_LONG_CAST(y2), AMI_LONG_CAST(x), 25);
            waitnext();
            longjmp(terminate_buf, 1);

        };
        fputc('\f', tw);
        fprintf(tw, "Resize screen buffered character\n");
        fprintf(tw, "*** DON'T MOVE THE WINDOW ***\n");
        fprintf(tw, "\n");
        fprintf(tw, "Moving in x\n");
        waittime(1000);

    }
    fprintf(tw, "\n");
    fprintf(tw, "Complete");
    waitnext();
    for (y = 10; y <= 50; y++) {

        ami_setsiz(tw, 80, y);
        ami_getsiz(tw, &x2, &y2);
        if (SIZOFF(x2, 80, SIZTOLC) || SIZOFF(y2, y, SIZTOLC)) {

            ami_setsiz(tw, 80, 25);
            fputc('\f', tw);
            fprintf(tw, "*** Getsiz does not match setsiz, x: %lld y: %lld vs. x: %d y: %lld\n",
                   AMI_LONG_CAST(x2), AMI_LONG_CAST(y2), 80, AMI_LONG_CAST(y));
            fprintf(tw, "*** Getsiz does not match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        fputc('\f', tw);
        fprintf(tw, "Resize screen buffered character\n");
        fprintf(tw, "*** DON'T MOVE THE WINDOW ***\n");
        fprintf(tw, "\n");
        fprintf(tw, "Moving in y\n");
        waittime(1000);

    }
    fprintf(tw, "\n");
    fprintf(tw, "Complete\n");
    waitnext();
    ami_winclientg(tw, ox, oy, &ox, &oy, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(tw, ox, oy);

    /* ******************** Resize screen with buffer on pixel ***************** */

    ox = ami_maxxg(tw);
    oy = ami_maxyg(tw);
    sqrrat(&xs, &ys, 1.5); /* find square ratio */
    /* Find the maximum size the window manager will grant, which can be less
       than the screen size (windows are typically limited to a single monitor,
       less any panels). Requests past this limit are silently clamped, so cap
       the resize loops to it. */
    ami_scnsizg(tw, &mxs, &mys);
    ami_setsizg(tw, mxs, mys);
    ami_getsizg(tw, &mxs, &mys);
    for (x = xs; x <= xs*4 && x <= mxs; x += xs/64) {

        ami_setsizg(tw, x, ys);
        ami_getsizg(tw, &x2, &y2);
        if (SIZOFF(x2, x, SIZTOLG) || SIZOFF(y2, ys, SIZTOLG)) {

            ami_setsiz(tw, 80, 25);
            fputc('\f', tw);
            fprintf(tw, "*** Getsiz does not match setsiz, x: %lld y: %lld vs. x: %lld y: %lld\n",
                   AMI_LONG_CAST(x2), AMI_LONG_CAST(y2), AMI_LONG_CAST(x), AMI_LONG_CAST(ys));
            fprintf(tw, "*** Getsiz does ! match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        fputc('\f', tw);
        fprintf(tw, "Resize screen buffered graphical\n");
        fprintf(tw, "*** DON'T MOVE THE WINDOW ***\n");
        fprintf(tw, "\n");
        fprintf(tw, "Moving in x\n");
        waittime(100);

    }
    fprintf(tw, "\n");
    fprintf(tw, "Complete\n");
    waitnext();
    for (y = ys; y <= ys*4 && y <= mys; y += ys/64) {

        ami_setsizg(tw, xs, y);
        ami_getsizg(tw, &x2, &y2);
        if (SIZOFF(x2, xs, SIZTOLG) || SIZOFF(y2, y, SIZTOLG)) {

            ami_setsiz(tw, 80, 25);
            fputc('\f', tw);
            fprintf(tw, "*** Getsiz does not match setsiz, x: %lld y: %lld vs. x: %lld y: %lld\n",
                   AMI_LONG_CAST(x2), AMI_LONG_CAST(y2), AMI_LONG_CAST(xs), AMI_LONG_CAST(y));
            fprintf(tw, "*** Getsiz does ! match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        fputc('\f', tw);
        fprintf(tw, "Resize screen buffered graphical\n");
        fprintf(tw, "*** DON'T MOVE THE WINDOW ***\n");
        fprintf(tw, "\n");
        fprintf(tw, "Moving in y\n");
        waittime(100);

    }
    fprintf(tw, "\n");
    fprintf(tw, "Complete\n");
    waitnext();
    ami_winclientg(tw, ox, oy, &ox, &oy, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(tw, ox, oy);

    /* ********************************* Front/back test *********************** */

    if (rooted) {

        /* A reference window to flip against: the rooted desktop is
           bare, so the test provides the neighbor. */
        ami_openwin(&stdin, &win2, NULL, wid2);
        ami_winclient(win2, 30, 10, &x, &y,
                      BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
        ami_setsiz(win2, x, y);
        ami_sizbuf(win2, 30, 10);
        ami_setpos(win2, 20, 8);
        ami_bcolor(win2, ami_yellow);
        putc('\f', win2);
        fprintf(win2, "reference window\n");

    }
    sqrrat(&xs, &ys, 8); /* find square ratio */
    cs = ami_chrsizy(tw); /* save the character size */
    fputc('\f', tw);
    ami_auto(tw, OFF);
    fprintf(tw, rooted? "Position this window over the reference window\n":
                "Position window for front/back test\n");
    fprintf(tw, "Then hit space to flip font/back status, or return to stop\n");
    fb = FALSE; /* clear front/back status */
    ami_font(tw, AMI_FONT_SIGN);
    ami_fontsiz(tw, ys);

    do {

        nextevt(&er);
        if (er.etype == ami_etchar) if (er.echar == ' ') { /* flip front/back */

            fb = !fb;
            if (fb) {

                ami_front(tw);
                ami_fcolor(tw, ami_white);
                prtceng(ami_maxyg(tw)/2-ami_chrsizy(tw)/2, "Back");
                ami_fcolor(tw, ami_black);
                prtceng(ami_maxyg(tw)/2-ami_chrsizy(tw)/2, "Front");

            } else {

                ami_back(tw);
                ami_fcolor(tw, ami_white);
                prtceng(ami_maxyg(tw)/2-ami_chrsizy(tw)/2, "Front");
                ami_fcolor(tw, ami_black);
                prtceng(ami_maxyg(tw)/2-ami_chrsizy(tw)/2, "Back");

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    if (rooted) fclose(win2);
    ami_home(tw);
    ami_fontsiz(tw, cs);
    ami_font(tw, AMI_FONT_TERM);
    ami_auto(tw, ON);

    /* ************************* Frame controls test buffered ****************** */

    fputc('\f', tw);
    ami_fcolor(tw, ami_cyan);
    ami_rect(tw, 1, 1, ami_maxxg(tw), ami_maxyg(tw));
    ami_line(tw, 1, 1, ami_maxxg(tw), ami_maxyg(tw));
    ami_line(tw, 1, ami_maxyg(tw), ami_maxxg(tw), 1);
    ami_fcolor(tw, ami_black);
    ami_binvis(tw);
    fprintf(tw, "Ready for frame controls buffered\n");
    fprintf(tw, "(Note system may not implement all -- or any frame controls)\n");
    waitnext();
    ami_frame(tw, OFF);
    fprintf(tw, "Entire frame off\n");
    waitnext();
    ami_frame(tw, ON);
    fprintf(tw, "Entire frame on\n");
    waitnext();
    ami_sysbar(tw, OFF);
    fprintf(tw, "System bar off\n");
    waitnext();
    ami_sysbar(tw, ON);
    fprintf(tw, "System bar on\n");
    waitnext();
    ami_sizable(tw, OFF);
    fprintf(tw, "Size bars off\n");
    waitnext();
    ami_sizable(tw, ON);
    fprintf(tw, "Size bars on\n");
    waitnext();
    ami_bover(tw);

    /* ************************* Frame controls test unbuffered ****************** */

    ami_buffer(tw, OFF);
    frametest("Ready for frame controls unbuffered - Resize me!");
    fprintf(tw, "(Note system may not implement all -- or any frame controls)\n");
    ami_frame(tw, OFF);
    frametest("Entire frame off");
    ami_frame(tw, ON);
    frametest("Entire frame on");
    ami_sysbar(tw, OFF);
    frametest("System bar off");
    ami_sysbar(tw, ON);
    frametest("System bar on");
    ami_sizable(tw, OFF);
    frametest("Size bars off");
    ami_sizable(tw, ON);
    frametest("Size bars on");
    ami_buffer(tw, ON);

    /* ********************************* Menu test ***************************** */

    ami_auto(tw, ON);
    fputc('\f', tw);
    ami_fcolor(tw, ami_cyan);
    ami_rect(tw, 1, 1, ami_maxxg(tw), ami_maxyg(tw));
    ami_line(tw, 1, 1, ami_maxxg(tw), ami_maxyg(tw));
    ami_line(tw, 1, ami_maxyg(tw), ami_maxxg(tw), 1);
    ami_fcolor(tw, ami_black);
    ml = NULL; /* clear menu list */
    newmenu(&mp, FALSE, FALSE, OFF, 1, "Say hello");
    appendmenu(&ml, mp);
    newmenu(&mp, TRUE, FALSE,  ON, 2, "Bark");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, 3, "Walk");
    appendmenu(&ml, mp);
    newmenu(&sm, FALSE, FALSE, OFF, 4, "Sublist");
    appendmenu(&ml, sm);
    /* these are one/of buttons */
    newmenu(&mp, FALSE, TRUE,  OFF, 5, "slow");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, FALSE, TRUE,  OFF, 6, "medium");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, 7, "fast");
    appendmenu(&sm->branch, mp);
    /* these are on/off buttons */
    newmenu(&mp, TRUE, FALSE,  OFF, 8, "red");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, TRUE, FALSE,  OFF, 9, "green");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, TRUE, FALSE,  OFF, 10, "blue");
    appendmenu(&sm->branch, mp);
    ami_menu(tw, ml);
    ami_menuena(tw, 3, OFF); /* disable "Walk" */
    ami_menusel(tw, 5, ON); /* turn on "slow" */
    ami_menusel(tw, 8, ON); /* turn on "red" */

    ami_home(tw);
    fprintf(tw, "Use sample menu above\n");
    fprintf(tw, "'Walk' is disabled\n");
    fprintf(tw, "'Sublist' is a dropdown\n");
    fprintf(tw, "'slow', 'medium' and 'fast' are a one/of list\n");
    fprintf(tw, "'red', 'green' and 'blue' are on/off\n");
    fprintf(tw, "There should be a bar between slow-medium-fast groups and\n");
    fprintf(tw, "red-green-blue groups.\n");
    sred = ON; /* set states */
    sgreen = OFF;
    sblue = OFF;
    do {

        nextevt(&er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (er.etype == ami_etmenus) {

            fprintf(tw, "Menu select: ");
            switch (er.menuid) {

                case 1:  fprintf(tw, "Say hello\n"); break;
                case 2:  fprintf(tw, "Bark\n"); break;
                case 3:  fprintf(tw, "Walk\n"); break;
                case 4:  fprintf(tw, "Sublist\n"); break;
                case 5:  fprintf(tw, "slow\n"); ami_menusel(tw, 5, ON); break;
                case 6:  fprintf(tw, "medium\n"); ami_menusel(tw, 6, ON); break;
                case 7:  fprintf(tw, "fast\n"); ami_menusel(tw, 7, ON); break;
                case 8:  fprintf(tw, "red\n"); sred = !sred;
                         ami_menusel(tw, 8, sred); break;
                case 9:  fprintf(tw, "green\n"); sgreen = !sgreen;
                         ami_menusel(tw, 9, sgreen); break;
                case 10: fprintf(tw, "blue\n"); sblue = !sblue;
                         ami_menusel(tw, 10, sblue); break;

            }
            if (autorun) frmstep(); /* the selection as reported */

        }

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    frmmark(FALSE); /* capture the menu as left */
    frmnext();
    ami_menu(tw, NULL);

    /* ****************************** Standard menu test ******************** */

    fputc('\f', tw);
    ami_auto(tw, ON);
    ml = NULL; /* clear menu list */
    newmenu(&mp, FALSE, FALSE, OFF, AMI_SMMAX+1, "one");
    appendmenu(&ml, mp);
    newmenu(&mp, TRUE, FALSE,  ON, AMI_SMMAX+2, "two");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, AMI_SMMAX+3, "three");
    appendmenu(&ml, mp);
    ami_stdmenu(BIT(AMI_SMNEW) | BIT(AMI_SMOPEN) | BIT(AMI_SMCLOSE) |
               BIT(AMI_SMSAVE) | BIT(AMI_SMSAVEAS) | BIT(AMI_SMPAGESET) |
               BIT(AMI_SMPRINT) | BIT(AMI_SMEXIT) | BIT(AMI_SMUNDO) |
               BIT(AMI_SMCUT) | BIT(AMI_SMPASTE) | BIT(AMI_SMDELETE) |
               BIT(AMI_SMFIND) | BIT(AMI_SMFINDNEXT) | BIT(AMI_SMREPLACE) |
               BIT(AMI_SMGOTO) | BIT(AMI_SMSELECTALL) | BIT(AMI_SMNEWWINDOW) |
               BIT(AMI_SMTILEHORIZ) | BIT(AMI_SMTILEVERT) | BIT(AMI_SMCASCADE) |
               BIT(AMI_SMCLOSEALL) | BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT),
               &mp, ml);
    ami_menu(tw, mp);
    fprintf(tw, "Standard menu appears above\n");
    fprintf(tw, "Check our 'one', 'two', 'three' buttons are in the program\n");
    fprintf(tw, "defined position\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (er.etype == ami_etmenus) {

            fprintf(tw, "Menu select: ");
            switch (er.menuid) {

                case AMI_SMNEW:       fprintf(tw, "new\n"); break;
                case AMI_SMOPEN:      fprintf(tw, "open\n"); break;
                case AMI_SMCLOSE:     fprintf(tw, "close\n"); break;
                case AMI_SMSAVE:      fprintf(tw, "save\n"); break;
                case AMI_SMSAVEAS:    fprintf(tw, "saveas\n"); break;
                case AMI_SMPAGESET:   fprintf(tw, "pageset\n"); break;
                case AMI_SMPRINT:     fprintf(tw, "print\n"); break;
                case AMI_SMEXIT:      fprintf(tw, "exit\n"); break;
                case AMI_SMUNDO:      fprintf(tw, "undo\n"); break;
                case AMI_SMCUT:       fprintf(tw, "cut\n"); break;
                case AMI_SMPASTE:     fprintf(tw, "paste\n"); break;
                case AMI_SMDELETE:    fprintf(tw, "delete\n"); break;
                case AMI_SMFIND:      fprintf(tw, "find\n"); break;
                case AMI_SMFINDNEXT:  fprintf(tw, "findnext\n"); break;
                case AMI_SMREPLACE:   fprintf(tw, "replace\n"); break;
                case AMI_SMGOTO:      fprintf(tw, "goto\n"); break;
                case AMI_SMSELECTALL: fprintf(tw, "selectall\n"); break;
                case AMI_SMNEWWINDOW: fprintf(tw, "newwindow\n"); break;
                case AMI_SMTILEHORIZ: fprintf(tw, "tilehoriz\n"); break;
                case AMI_SMTILEVERT:  fprintf(tw, "tilevert\n"); break;
                case AMI_SMCASCADE:   fprintf(tw, "cascade\n"); break;
                case AMI_SMCLOSEALL:  fprintf(tw, "closeall\n"); break;
                case AMI_SMHELPTOPIC: fprintf(tw, "helptopic\n"); break;
                case AMI_SMABOUT:     fprintf(tw, "about\n"); break;
                case AMI_SMMAX+1:     fprintf(tw, "one\n"); break;
                case AMI_SMMAX+2:     fprintf(tw, "two\n"); break;
                case AMI_SMMAX+3:     fprintf(tw, "three\n"); break;

            }
            if (autorun) frmstep(); /* the selection as reported */

        }

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    frmmark(FALSE); /* capture the menu as left */
    frmnext();
    ami_menu(tw, NULL);

    /* ************************* Child windows test character ****************** */

    fputc('\f', tw);
    chrgrid();
    prtcen(ami_maxy(tw), "Child windows test character");
    ami_openwin(&stdin, &win2, tw, wid2);
    ami_curvis(win2, OFF);
    ami_setpos(win2, 1, 10);
    ami_sizbuf(win2, 20, 10);
    ami_setsiz(win2, 20, 10);
    ami_openwin(&stdin, &win3, tw, wid3);
    ami_curvis(win3, OFF);
    ami_setpos(win3, 21, 10);
    ami_sizbuf(win3, 20, 10);
    ami_setsiz(win3, 20, 10);
    ami_openwin(&stdin, &win4, tw, wid4);
    ami_curvis(win4, OFF);
    ami_setpos(win4, 41, 10);
    ami_sizbuf(win4, 20, 10);
    ami_setsiz(win4, 20, 10);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_bcolor(win4, ami_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    ami_home(tw);
    fprintf(tw, "There should be 3 labeled child windows below, with frames   \n");
    fprintf(tw, "(the system may not implement frames on child windows)      \n");
    waitnext();
    ami_frame(win2, OFF);
    ami_frame(win3, OFF);
    ami_frame(win4, OFF);
    ami_home(tw);
    fprintf(tw, "There should be 3 labeled child windows below, without frames\n");
    fprintf(tw, "                                                            \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_home(tw);
    fprintf(tw, "Child windows should all be closed                           \n");
    waitnext();

    /* *************************** Child windows test pixel ******************** */

    fputc('\f', tw);
    sqrrat(&xs, &ys, 2.5); /* find square ratio */
    prtcen(ami_maxy(tw), "Child windows test pixel");
    ami_openwin(&stdin, &win2, tw, wid2);
    ami_curvis(win2, OFF);
    ami_setposg(win2, xs*0+1, ys/2.5);
    ami_sizbufg(win2, xs, ys);
    ami_setsizg(win2, xs, ys);
    ami_openwin(&stdin, &win3, tw, wid3);
    ami_curvis(win3, OFF);
    ami_setposg(win3, xs*1+1, ys/2.5);
    ami_sizbufg(win3, xs, ys);
    ami_setsizg(win3, xs, ys);
    ami_openwin(&stdin, &win4, tw, wid4);
    ami_curvis(win4, OFF);
    ami_setposg(win4, xs*2+1, ys/2.5);
    ami_sizbufg(win4, xs, ys);
    ami_setsizg(win4, xs, ys);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_bcolor(win4, ami_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    ami_home(tw);
    fprintf(tw, "There should be 3 labled child windows below, with frames   \n");
    fprintf(tw, "(the system may not implement frames on child windows)      \n");
    waitnext();
    ami_frame(win2, OFF);
    ami_frame(win3, OFF);
    ami_frame(win4, OFF);
    ami_home(tw);
    fprintf(tw, "There should be 3 labled child windows below, without frames\n");
    fprintf(tw, "                                                            \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_home(tw);
    fprintf(tw, "Child windows should all be closed                          \n");
    fprintf(tw, "                                                            \n");
    waitnext();

    /* *************** Child windows independent test character ************ */

    ami_curvis(tw, ON);
    fputc('\f', tw);
    chrgrid();
    prtcen(ami_maxy(tw), "Child windows independent test character");
    ami_openwin(&stdin, &win2, tw, wid2);
    ami_setpos(win2, 11, 10);
    ami_sizbuf(win2, 30, 10);
    ami_setsiz(win2, 30, 10);
    ami_openwin(&stdin, &win3, tw, wid3);
    ami_setpos(win3, 41, 10);
    ami_sizbuf(win3, 30, 10);
    ami_setsiz(win3, 30, 10);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_home(tw);
    fprintf(tw, "There should be 2 labeled child windows below, with frames   \n");
    fprintf(tw, "(the system may not implement frames on child windows)       \n");
    fprintf(tw, "Test focus can be moved between windows, including the main  \n");
    fprintf(tw, "window. Test windows can be minimized and maximized          \n");
    fprintf(tw, "(if framed), test entering characters to windows.            \n");
    do {

        nextevt(&er); /* get next event */
        if (er.etype == ami_etchar) {

            if (er.winid == wid2) fputc(er.echar, win2);
            else if (er.winid == wid3) fputc(er.echar, win3);

        } else if (er.etype == ami_etenter) {

            /* translate the crs so we can test scrolling */
            if (er.winid == wid2) fputc('\n', win2);
            else if (er.winid == wid3) fputc('\n', win3);

        } else if (er.etype == ami_etterm && er.winid == mainwid)
            /* only take terminations from main window */
            longjmp(terminate_buf, 1);

    /* terminate on cr to the main window only */
    } while (er.etype != ami_etenter || er.winid != mainwid);
    fclose(win2);
    fclose(win3);
    ami_home(tw);
    fprintf(tw, "Child windows should all be closed                           \n");
    fprintf(tw, "                                                             \n");
    fprintf(tw, "                                                             \n");
    fprintf(tw, "                                                             \n");
    fprintf(tw, "                                                             \n");
    ami_curvis(tw, OFF);
    waitnext();

    /* ******************** Child windows independent test pixel ************** */

    fputc('\f', tw);
    sqrrat(&xs, &ys, 2); /* find square ratio */
    prtcen(ami_maxy(tw), "Child windows test pixel");
    ami_openwin(&stdin, &win2, tw, wid2);
    ami_setposg(win2, xs*0+xs/5, ys/2);
    ami_sizbufg(win2, xs, ys);
    ami_setsizg(win2, xs, ys);
    ami_openwin(&stdin, &win3, tw, wid3);
    ami_setposg(win3, xs*1+xs/5, ys/2);
    ami_sizbufg(win3, xs, ys);
    ami_setsizg(win3, xs, ys);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_home(tw);
    fprintf(tw, "There should be 2 labeled child windows below, with frames   \n");
    fprintf(tw, "(the system may not implement frames on child windows)      \n");
    fprintf(tw, "Test focus can be moved between windows, test windows can be \n");
    fprintf(tw, "minimized and maximized (if framed), test entering           \n");
    fprintf(tw, "characters to windows.                                       \n");
    do {

        nextevt(&er); /* get next event */
        if (er.etype == ami_etchar) {

            if (er.winid == wid2) fputc(er.echar, win2);
            else if (er.winid == wid3) fputc(er.echar, win3);

        } else if (er.etype == ami_etenter) {

            /* translate the crs so we can test scrolling */
            if (er.winid == wid2) fputc('\n', win2);
            else if (er.winid == wid3) fputc('\n', win3);

        } else if (er.etype == ami_etterm && er.winid == mainwid)
            /* only take terminations from main window */
            longjmp(terminate_buf, 1);

    /* terminate on cr to the main window only */
    } while (er.etype != ami_etenter || er.winid != mainwid);
    fclose(win2);
    fclose(win3);
    ami_home(tw);
    fprintf(tw, "Child windows should all be closed                          \n");
    fprintf(tw, "                                                            \n");
    fprintf(tw, "                                                            \n");
    fprintf(tw, "                                                            \n");
    fprintf(tw, "                                                            \n");
    waitnext();

    /* ******************* Child windows stacking test pixel ******************* */

    fputc('\f', tw);
    sqrrat(&xs, &ys, 2.5); /* find square ratio */
    prtcen(ami_maxy(tw), "Child windows stacking test pixel");
    ami_openwin(&stdin, &win2, tw, wid2);
    ami_curvis(win2, OFF);
    ami_setposg(win2, xs/2*0+xs/5, ys/2.5+ys*0/4);
    ami_sizbufg(win2, xs, ys);
    ami_setsizg(win2, xs, ys);
    ami_openwin(&stdin, &win3, tw, wid3);
    ami_curvis(win3, OFF);
    ami_setposg(win3, xs/2*1+xs/5, ys/2.5+ys*1/4);
    ami_sizbufg(win3, xs, ys);
    ami_setsizg(win3, xs, ys);
    ami_openwin(&stdin, &win4, tw, wid4);
    ami_curvis(win4, OFF);
    ami_setposg(win4, xs/2*2+xs/5, ys/2.5+ys*2/4);
    ami_sizbufg(win4, xs, ys);
    ami_setsizg(win4, xs, ys);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_bcolor(win4, ami_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    ami_home(tw);
    fprintf(tw, "There should be 3 labled child windows below, overlapped,   \n");
    fprintf(tw, "with child 1 on the bottom, child 2 middle, and child 3 top.\n");
    waitnext();
    ami_back(win2);
    ami_back(win3);
    ami_back(win4);
    ami_home(tw);
    fprintf(tw, "Now the windows are reordered, with child 1 on top, child 2 \n");
    fprintf(tw, "below that, and child 3 on the bottom.                      \n");
    waitnext();
    ami_front(win2);
    ami_front(win3);
    ami_front(win4);
    ami_home(tw);
    fprintf(tw, "Now the windows are reordered, with child 3 on top, child 2 \n");
    fprintf(tw, "below that, and child 1 on the bottom.                      \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    fputc('\f', tw);
    fprintf(tw, "Child windows should all be closed                          \n");
    waitnext();

    /* ************** Child windows stacking resize test pixel 1 *************** */

    sqrrat(&xs, &ys, 5); /* find square ratio */
    ami_buffer(tw, OFF);
    ami_auto(tw, OFF);
    ami_openwin(&stdin, &win2, tw, wid2);
    ami_setposg(win2, xs/2*1, ys/2*1);
    ami_sizbufg(win2, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
    ami_setsizg(win2, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
    ami_openwin(&stdin, &win3, tw, wid3);
    ami_setposg(win3, xs/2*2, ys/2*2);
    ami_sizbufg(win3, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
    ami_setsizg(win3, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
    ami_openwin(&stdin, &win4, tw, wid4);
    ami_setposg(win4, xs/2*3, ys/2*3);
    ami_sizbufg(win4, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
    ami_setsizg(win4, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
    ami_curvis(win2, OFF);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_curvis(win3, OFF);
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_curvis(win4, OFF);
    ami_bcolor(win4, ami_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    do {

        nextevt(&er);
        /* repaint the parent on a main-window (winid 1) redraw or resize */
        if ((er.etype == ami_etredraw || er.etype == ami_etresize) &&
            er.winid == mainwid) {

            fputc('\f', tw);
            prtceng(ami_maxyg(tw)-ami_chrsizy(tw),
                    "Child windows stacking resize test pixel 1");
            prtceng(1, "move and resize");
            /* re-fit the children only on an actual PARENT RESIZE -- not on a
               mere redraw. A child's own resize/move makes the unbuffered parent
               repaint (etredraw, winid 1); refitting there would setsizg the
               children back to the parent-derived size and revert a manual child
               resize. Two guards are needed: winid 1 excludes the child's own
               etresize, and etresize excludes the parent's redraw. */
            if (er.etype == ami_etresize) {
                ami_setsizg(win3, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
                ami_setsizg(win4, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
                ami_setsizg(win2, ami_maxxg(tw)-xs*2, ami_maxyg(tw)-ys*2);
            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_buffer(tw, ON);
    fputc('\f', tw);
    fprintf(tw, "Child windows should all be closed                          \n");
    waitnext();

    /* ************** Child windows stacking resize test pixel 2 *************** */

    sqrrat(&xs, &ys, 20); /* find square ratio */
    ami_buffer(tw, OFF);
    ami_openwin(&stdin, &win2, tw, wid2);
    ami_auto(win2, OFF);
    ami_curvis(win2, OFF);
    ami_setposg(win2, xs*1, ys*1);
    ami_sizbufg(win2, ami_strsiz(win2, "I am child window 1"), ami_chrsizy(win2));
    ami_setsizg(win2, ami_maxxg(tw)-xs*1*2, ami_maxyg(tw)-ys*1*2);
    ami_openwin(&stdin, &win3, tw, wid3);
    ami_auto(win3, OFF);
    ami_curvis(win3, OFF);
    ami_setposg(win3, xs*2, ys*2);
    ami_sizbufg(win2, ami_strsiz(win3, "I am child window 2"), ami_chrsizy(win3));
    ami_setsizg(win3, ami_maxxg(tw)-xs*2*2, ami_maxyg(tw)-ys*2*2);
    ami_openwin(&stdin, &win4, tw, wid4);
    ami_auto(win4, OFF);
    ami_curvis(win4, OFF);
    ami_setposg(win4, xs*3, ys*3);
    ami_sizbufg(win2, ami_strsiz(win4, "I am child window 3"), ami_chrsizy(win4));
    ami_setsizg(win4, ami_maxxg(tw)-xs*3*2, ami_maxyg(tw)-ys*3*2);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2");
    ami_bcolor(win4, ami_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3");
    do {

        nextevt(&er);
        /* repaint the parent on a main-window (winid 1) redraw or resize; re-fit
           children only on an actual parent resize -- see the note in stacking
           resize test pixel 1 above */
        if ((er.etype == ami_etredraw  || er.etype == ami_etresize) &&
            er.winid == mainwid) {

            fputc('\f', tw);
            prtceng(ami_maxyg(tw)-ami_chrsizy(tw),
                    "Child windows stacking resize test pixel 2");
            prtceng(1, "move and resize");
            if (er.etype == ami_etresize) {
                ami_setsizg(win2, ami_maxxg(tw)-xs*1*2, ami_maxyg(tw)-ys*1*2);
                ami_setsizg(win3, ami_maxxg(tw)-xs*2*2, ami_maxyg(tw)-ys*2*2);
                ami_setsizg(win4, ami_maxxg(tw)-xs*3*2, ami_maxyg(tw)-ys*3*2);
            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_buffer(tw, ON);
    fputc('\f', tw);
    fprintf(tw, "Child windows should all be closed                          \n");
    waitnext();

    /* ******************************* Buffer off test *********************** */

    fputc('\f', tw);
    cs = ami_chrsizy(tw); /* save the character size */
    ami_auto(tw, OFF);
    ami_buffer(tw, OFF);
    /* initialize prime size information */
    x = ami_maxxg(tw);
    y = ami_maxyg(tw);
    ami_linewidth(tw, 5); /* set large lines */
    ami_font(tw, AMI_FONT_SIGN);
    ami_binvis(tw);
    do {

        nextevt(&er); /* get next event */
        if (er.etype == ami_etredraw || er.etype == ami_etresize) {

            /* clear screen without overwriting frame */
            ami_fcolor(tw, ami_white);
            ami_frect(tw, 1+5, 1+5, x-5, y-5);
            ami_fcolor(tw, ami_black);
            ami_fontsiz(tw, y / 10);
            prtceng(ami_maxyg(tw)/2-ami_chrsizy(tw)/2,
                    "SIZE AND COVER ME !");
            ami_rect(tw, 1+2, 1+2, x-2, y-2); /* frame the window */

        }
        if (er.etype == ami_etresize) {

            /* Save the new demensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = ami_maxxg(tw);
            y = ami_maxyg(tw);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_buffer(tw, ON);
    ami_fontsiz(tw, cs);
    ami_font(tw, AMI_FONT_TERM);
    ami_home(tw);
    ami_auto(tw, ON);

    /* ****************************** min/max/norm test ********************* */

    fputc('\f', tw);
    ami_auto(tw, OFF);
    ami_buffer(tw, OFF);
    ami_font(tw, AMI_FONT_TERM);
    mincnt = 0; /* clear minimize counter */
    maxcnt = 0; /* clear maximize counter */
    nrmcnt = 0; /* clear normalize counter */
    do {

        nextevt(&er); /* get next event */
        /* count minimize, maximize, normalize */
        if (er.etype == ami_etmax) maxcnt = maxcnt+1;
        if (er.etype == ami_etmin) mincnt = mincnt+1;
        if (er.etype == ami_etnorm) nrmcnt = nrmcnt+1;
        if (er.etype == ami_etredraw || er.etype == ami_etmax ||
            er.etype == ami_etmin || er.etype == ami_etnorm) {

            fputc('\f', tw);
            fprintf(tw, "Minimize, maximize and restore this window\n");
            fprintf(tw, "\n");
            fprintf(tw, "Minimize count:  %d\n", mincnt);
            fprintf(tw, "Maximize count:  %d\n", maxcnt);
            fprintf(tw, "Normalize count: %d\n", nrmcnt);

        }

        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_buffer(tw, ON);

    /* ******************** Window size calculate character ***************** */

    fputc('\f', tw);
    prtceng(ami_maxyg(tw)-ami_chrsizy(tw), "Window size calculate character");
    ami_home(tw);
    ami_openwin(&stdin, &win2, NULL, wid2);
    ami_linewidth(tw, 1);

    ami_winclient(tw, 20, 10, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    fprintf(tw, "For (20, 10) client, full frame, window size is: %lld,%lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsiz(win2, x, y);
    putc('\f', win2);
    ami_fcolor(win2, ami_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    ami_fcolor(win2, ami_cyan);
    ami_rect(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 10*ami_chrsizy(win2), 20*ami_chrsizx(win2), 1);
    ami_curvis(win2, OFF);
    fprintf(tw, "Check client window has (20, 10) surface\n");
    waitnext();

    fprintf(tw, "System bar off\n");
    ami_sysbar(win2, OFF);
    ami_winclient(tw, 20, 10, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize));
    fprintf(tw, "For (20, 10) client, no system bar, window size is: %lld,%lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsiz(win2, x, y);
    putc('\f', win2);
    ami_fcolor(win2, ami_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    ami_fcolor(win2, ami_cyan);
    ami_rect(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 10*ami_chrsizy(win2), 20*ami_chrsizx(win2), 1);
    ami_curvis(win2, OFF);
    fprintf(tw, "Check client window has (20, 10) surface\n");
    waitnext();

    fprintf(tw, "Sizing bars off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, OFF);
    ami_winclient(tw, 20, 10, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsysbar));
    fprintf(tw, "For (20, 10) client, no size bars, window size is: %lld,%lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsiz(win2, x, y);
    putc('\f', win2);
    ami_fcolor(win2, ami_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    ami_fcolor(win2, ami_cyan);
    ami_rect(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 10*ami_chrsizy(win2), 20*ami_chrsizx(win2), 1);
    ami_curvis(win2, OFF);
    fprintf(tw, "Check client window has (20, 10) surface\n");
    waitnext();

    fprintf(tw, "frame off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, ON);
    ami_frame(win2, OFF);
    ami_winclient(tw, 20, 10, &x, &y, BIT(ami_wmsize) | BIT(ami_wmsysbar));
    fprintf(tw, "For (20, 10) client, no frame, window size is: %lld,%lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsiz(win2, x, y);
    putc('\f', win2);
    ami_fcolor(win2, ami_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    ami_fcolor(win2, ami_cyan);
    ami_rect(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 1, 20*ami_chrsizx(win2), 10*ami_chrsizy(win2));
    ami_line(win2, 1, 10*ami_chrsizy(win2), 20*ami_chrsizx(win2), 1);
    ami_curvis(win2, OFF);
    fprintf(tw, "Check client window has (20, 10) surface\n");
    waitnext();

    fclose(win2);

    /* ************************ Window size calculate pixel ******************** */

    fputc('\f', tw);
    xr = ami_maxxg(tw)/3; /* ratio window but parent */
    prtceng(ami_maxyg(tw)-ami_chrsizy(tw), "Window size calculate pixel");
    ami_home(tw);
    ami_openwin(&stdin, &win2, NULL, wid2);
    ami_linewidth(tw, 1);
    ami_fcolor(win2, ami_cyan);
    ami_winclientg(tw, xr, xr, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    fprintf(tw, "For (%lld, %lld) client, full frame, window size is: %lld,%lld\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr), AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsizg(win2, x, y);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    ami_curvis(win2, OFF);
    fprintf(tw, "Check client window has (%lld, %lld) surface\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr));
    waitnext();

    fprintf(tw, "System bar off\n");
    ami_sysbar(win2, OFF);
    ami_winclientg(tw, xr, xr, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize));
    fprintf(tw, "For (%lld, %lld) client, no system bar, window size is: %lld,%lld\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr), AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsizg(win2, x, y);
    putc('\f', win2);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    fprintf(tw, "Check client window has (%lld, %lld) surface\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr));
    waitnext();

    fprintf(tw, "Sizing bars off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, OFF);
    ami_winclientg(tw, xr, xr, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsysbar));
    fprintf(tw, "For (%lld, %lld) client, no sizing, window size is: %lld,%lld\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr), AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsizg(win2, x, y);
    putc('\f', win2);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    fprintf(tw, "Check client window has (%lld, %lld) surface\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr));
    waitnext();

    fprintf(tw, "frame off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, ON);
    ami_frame(win2, OFF);
    ami_winclientg(tw, xr, xr, &x, &y, BIT(ami_wmsize) | BIT(ami_wmsysbar));
    fprintf(tw, "For (%lld, %lld) client, no frame, window size is: %lld,%lld\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr), AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsizg(win2, x, y);
    putc('\f', win2);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    fprintf(tw, "Check client window has (%lld, %lld) surface\n", AMI_LONG_CAST(xr), AMI_LONG_CAST(xr));
    waitnext();

    fclose(win2);

    /* ******************* Window size calculate minimums pixel *************** */

    /* this test does not work, winclient needs to return the minimums */

#if 0
    fputc('\f', tw);
    prtceng(ami_maxyg(tw)-ami_chrsizy(tw), "Window size calculate minimum pixel");
    ami_home(tw);
    ami_openwin(&stdin, &win2, NULL, wid2);
    ami_linewidth(tw, 1);
    ami_fcolor(win2, ami_cyan);
    ami_winclientg(tw, 1, 1, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    fprintf(tw, "For (200, 200) client, full frame, window size minimum is: %lld,%lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    ami_setsizg(win2, 1, 1);
    ami_getsizg(win2, &x2, &y2);
    waitnext();

    fclose(win2);
#endif

    /* ********************** Child windows torture test pixel ***************** */

    ami_getsizg(tw, &xs, &ys); /* get window size */
    if (xs > ys) { xs /= 3.5; ys = xs; }
    else { ys /= 3.5; xs = ys; }
    c1 = ami_red;
    c2 = ami_green;
    c3 = ami_blue;
    fputc('\f', tw);
    fprintf(tw, "Child windows torture test pixel\n");
    t = ami_clock(); /* get base time */
    for (i = 1; i <= 100; i++) {

        ami_openwin(&stdin, &win2, tw, wid2);
        ami_setposg(win2, xs/10, ys/5);
        ami_sizbufg(win2, xs, ys);
        ami_setsizg(win2, xs, ys);
        ami_openwin(&stdin, &win3, tw, wid3);
        ami_setposg(win3, xs/10+xs, ys/5);
        ami_sizbufg(win3, xs, ys);
        ami_setsizg(win3, xs, ys);
        ami_openwin(&stdin, &win4, tw, wid4);
        ami_setposg(win4, xs/10+xs*2, ys/5);
        ami_sizbufg(win4, xs, ys);
        ami_setsizg(win4, xs, ys);
        ami_bcolor(win2, c1);
        c1 = nextcolor(c1);
        putc('\f', win2);
        fprintf(win2, "I am child window 1\n");
        ami_bcolor(win3, c2);
        c2 = nextcolor(c2);
        putc('\f', win3);
        fprintf(win3, "I am child window 2\n");
        ami_bcolor(win4, c3);
        c3 = nextcolor(c3);
        putc('\f', win4);
        fprintf(win4, "I am child window 3\n");
        fclose(win2);
        fclose(win3);
        fclose(win4);

    }
    et = ami_elapsed(t);
    ami_home(tw);
    ami_bover(tw);
    fprintf(tw, "Child windows should all be closed\n");
    fprintf(tw, "\n");
    /* the times are never the same twice, so an automatic run, which is
       judged on the screens themselves, leaves them off the screen */
    if (!autorun) {

        fprintf(tw, "Child windows place and remove %d iterations %f seconds\n",
               100, et*0.0001);
        fprintf(tw, "%f per iteration\n", et*0.0001/100);

    }
    waitnext();

    terminate: /* terminate */

    fputc('\f', tw);
    ami_auto(tw, OFF);
    ami_font(tw, AMI_FONT_SIGN);
    ami_fontsiz(tw, 50);
    prtceng(ami_maxyg(tw)/2-ami_chrsizy(tw)/2, "Test complete");

}
