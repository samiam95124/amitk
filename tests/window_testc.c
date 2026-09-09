/*******************************************************************************
*                                                                             *
*              WINDOW MANAGEMENT TEST PROGRAM, CHARACTER MODE                  *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
*                                                                             *
* Tests text windows management calls. The root window serves as the         *
* desktop: all testing is applied to a child window, a standard 80x25         *
* terminal surface, the way the graphical form of this test applies to a      *
* program window on the desktop. Tests that create child windows make them    *
* children of the test window, a child of a child. Maximize the root window   *
* to give the test room to work.                                              *
*                                                                             *
*******************************************************************************/

/*******************************************************************************

Usage:

    window_testc [auto [file]] [first [last]]

    auto   Walks every screen with no input and exits after them: the
           regression mode. A following non-numeric argument names the
           file the screens are captured to.
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
#include <terminalw.h>

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
static int        autorun = FALSE; /* walk every screen with no input */
static ami_long   autowin = 2;  /* the window an automatic return comes from */
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
static ami_long   t, et;
static ami_color   c1, c2, c3;

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
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

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); \
                                fflush(stderr); } while (0)

extern void screen_capture(void);
extern void screen_capture_name(const char* fn);

/* "window_testc auto" walks every screen with no input at all,
   capturing each, and exits at the end: this is how the regression runs
   it. Child windows and menus are windows of their own and paint from
   events, so an automatic run pumps events for a moment to let the screen
   settle before capturing it, then answers the wait with a return. */
#define AUTOSETL 3000 /* settle time before a capture, 100us units */
#define AUTOTIM  9    /* timer the settle runs on */

static void autosettle(void)

{

    ami_evtrec er;

    ami_timer(stdout, AUTOTIM, AUTOSETL, FALSE);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_ettim || er.timnum != AUTOTIM);

}

/* Every wait for the user comes through here. An automatic run lets the
   screen settle and answers with the return the screen was waiting for,
   from the window the test is on. Before the selected range every wait
   answers at once. */
static void nextevt(ami_evtrec* er)

{

    if (framenum < tstlo) {

        er->etype = ami_etenter;
        er->winid = autowin;
        return;

    }
    if (autorun) {

        autosettle();
        er->etype = ami_etenter;
        er->winid = autowin;
        return;

    }
    ami_event(stdin, er);

}

/* A step of an animation, in an automatic run: the number of the frame
   being drawn and the step within it as a fraction, frame 18.1, 18.2 and
   so on, stamped into the title as the frame mark is, so the frame count
   and the frame range a run selects are untouched; then the capture. The
   frame's own mark ends the run of steps. Outside the selected range
   nothing is captured. */
static int stepnum; /* the step within the frame being drawn */

static void frmstep(void)
{

    char titlebuf[80];

    if (framenum+1 < tstlo || (tsthi && framenum+1 > tsthi)) return;
    stepnum++;
    sprintf(titlebuf, "window_test: frame %d.%d", framenum+1, stepnum);
    ami_title(tw, titlebuf);
    autosettle(); /* let the screen settle before the capture */
    screen_capture();

}

/* wait return to be pressed, or handle terminate */

static void waitnextt(int keeptitle)

{

    ami_evtrec er; /* event record */
    char titlebuf[80];

    framenum++;
    stepnum = 0; /* the next frame's steps count from one */
    if (tsthi && framenum > tsthi) longjmp(terminate_buf, 1);
    if (framenum < tstlo) return; /* before the range: the count alone */
    /* Stamp the frame number into the title bar, unless the caller is testing
       ami_title itself: keeptitle=TRUE preserves the title under test instead
       of clobbering it. */
    if (!keeptitle) {

        sprintf(titlebuf, "window_test: frame %d", framenum);
        ami_title(tw, titlebuf);

    }
    if (autorun) autosettle(); /* let the screen settle before the capture */
    screen_capture();
    if (autorun) return; /* the capture was the point: pass */
    do { ami_event(stdin, &er); }
    while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

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


/* wait time in 100 microseconds */

static void frmstep(void);
static void autosettle(void);

static void waittime(int t)

{

    ami_evtrec er;

    ami_timer(tw, 1, t, FALSE);
    do { ami_event(stdin, &er);
    } while (er.etype != ami_ettim && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
    /* an automatic run captures the step: the resizes are judged on every
       step and not on their last screen alone */
    if (autorun) frmstep();

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


/* display frame test */

/* Box a window in characters. The graphical form of this test rules a
   rectangle and its diagonals; on a character surface the box alone
   carries the same meaning, which is that the window is the size and in
   the place the test asked for. */

static void charbox(FILE* f, ami_long x, ami_long y)

{

    ami_long i;

    /* Auto off: the box includes the bottom right corner cell, and with
       auto on writing it scrolls the window, shifting the box just drawn. */
    ami_auto(f, OFF);
    ami_cursor(f, 1, 1);
    for (i = 1; i <= x; i++) fputc('-', f);
    ami_cursor(f, 1, y);
    for (i = 1; i <= x; i++) fputc('-', f);
    for (i = 1; i <= y; i++) { ami_cursor(f, 1, i); fputc('|', f); }
    for (i = 1; i <= y; i++) { ami_cursor(f, x, i); fputc('|', f); }

}

static void frameinside(const string s, ami_long x, ami_long y)

{

    ami_long i;

    /* draw the frame as a box of characters, with the corners joined, which
       is the character surface equivalent of the ruled rectangle and
       diagonals the graphical form draws */
    fprintf(tw, "\f");
    ami_auto(tw, OFF); /* the box includes the bottom right corner cell */
    ami_cursor(tw, 1, 1);
    for (i = 1; i <= x; i++) fputc('-', tw);
    ami_cursor(tw, 1, y);
    for (i = 1; i <= x; i++) fputc('-', tw);
    for (i = 1; i <= y; i++) { ami_cursor(tw, 1, i); fputc('|', tw); }
    for (i = 1; i <= y; i++) { ami_cursor(tw, x, i); fputc('|', tw); }
    prtcen(y/2, s);

}

static void frametest(const string s)

{

    ami_evtrec er;
    ami_long  x, y;

    x = ami_maxx(tw); /* set size */
    y = ami_maxy(tw);
    frameinside(s, x, y);
    do {

        nextevt(&er); /* get next event */
        if (er.etype == ami_etredraw) frameinside(s, x, y);
        if (er.etype == ami_etresize) {

            /* Save the new dimensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = ami_maxx(tw);
            y = ami_maxy(tw);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);

}


/* Run the sample menu on the given window: install it, exercise it until
   return is hit, then remove it. Used on the root window and then on the
   test window; the menu list ml must be built by the caller. */

static void samplemenu(FILE* w)

{

    ami_evtrec er;

    ami_auto(w, ON);
    ami_menu(w, ml);
    ami_menuena(w, 3, OFF); /* disable "Walk" */
    ami_menusel(w, 5, ON); /* turn on "slow" */
    ami_menusel(w, 8, ON); /* turn on "red" */

    ami_home(w);
    fprintf(w, "Use sample menu above\n");
    fprintf(w, "'Walk' is disabled\n");
    fprintf(w, "'Sublist' is a dropdown\n");
    fprintf(w, "'slow', 'medium' and 'fast' are a one/of list\n");
    fprintf(w, "'red', 'green' and 'blue' are on/off\n");
    fprintf(w, "There should be a bar between slow-medium-fast groups and\n");
    fprintf(w, "red-green-blue groups.\n");
    sred = ON; /* set states */
    sgreen = OFF;
    sblue = OFF;
    do {

        nextevt(&er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (er.etype == ami_etmenus) {

            fprintf(w, "Menu select: ");
            switch (er.menuid) {

                case 1:  fprintf(w, "Say hello\n"); break;
                case 2:  fprintf(w, "Bark\n"); break;
                case 3:  fprintf(w, "Walk\n"); break;
                case 4:  fprintf(w, "Sublist\n"); break;
                case 5:  fprintf(w, "slow\n"); ami_menusel(w, 5, ON); break;
                case 6:  fprintf(w, "medium\n"); ami_menusel(w, 6, ON); break;
                case 7:  fprintf(w, "fast\n"); ami_menusel(w, 7, ON); break;
                case 8:  fprintf(w, "red\n"); sred = !sred;
                         ami_menusel(w, 8, sred); break;
                case 9:  fprintf(w, "green\n"); sgreen = !sgreen;
                         ami_menusel(w, 9, sgreen); break;
                case 10: fprintf(w, "blue\n"); sblue = !sblue;
                         ami_menusel(w, 10, sblue); break;

            }

        }

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    ami_menu(w, NULL);

}

/* Run the standard menu on the given window: build it with the program's
   own entries appended, exercise it until return is hit, then remove it.
   Uses its own menu list, leaving the sample menu list intact. */

static void stdmenutest(FILE* w)

{

    ami_evtrec  er;
    ami_menuptr lml; /* this test's own menu list */
    ami_menuptr lmp;

    fputc('\f', w);
    ami_auto(w, ON);
    lml = NULL; /* clear menu list */
    newmenu(&lmp, FALSE, FALSE, OFF, AMI_SMMAX+1, "one");
    appendmenu(&lml, lmp);
    newmenu(&lmp, TRUE, FALSE,  ON, AMI_SMMAX+2, "two");
    appendmenu(&lml, lmp);
    newmenu(&lmp, FALSE, FALSE, OFF, AMI_SMMAX+3, "three");
    appendmenu(&lml, lmp);
    ami_stdmenu(BIT(AMI_SMNEW) | BIT(AMI_SMOPEN) | BIT(AMI_SMCLOSE) |
               BIT(AMI_SMSAVE) | BIT(AMI_SMSAVEAS) | BIT(AMI_SMPAGESET) |
               BIT(AMI_SMPRINT) | BIT(AMI_SMEXIT) | BIT(AMI_SMUNDO) |
               BIT(AMI_SMCUT) | BIT(AMI_SMPASTE) | BIT(AMI_SMDELETE) |
               BIT(AMI_SMFIND) | BIT(AMI_SMFINDNEXT) | BIT(AMI_SMREPLACE) |
               BIT(AMI_SMGOTO) | BIT(AMI_SMSELECTALL) | BIT(AMI_SMNEWWINDOW) |
               BIT(AMI_SMTILEHORIZ) | BIT(AMI_SMTILEVERT) | BIT(AMI_SMCASCADE) |
               BIT(AMI_SMCLOSEALL) | BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT),
               &lmp, lml);
    ami_menu(w, lmp);
    fprintf(w, "Standard menu appears above\n");
    fprintf(w, "Check our 'one', 'two', 'three' buttons are in the program\n");
    fprintf(w, "defined position\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (er.etype == ami_etmenus) {

            fprintf(w, "Menu select: ");
            switch (er.menuid) {

                case AMI_SMNEW:       fprintf(w, "new\n"); break;
                case AMI_SMOPEN:      fprintf(w, "open\n"); break;
                case AMI_SMCLOSE:     fprintf(w, "close\n"); break;
                case AMI_SMSAVE:      fprintf(w, "save\n"); break;
                case AMI_SMSAVEAS:    fprintf(w, "saveas\n"); break;
                case AMI_SMPAGESET:   fprintf(w, "pageset\n"); break;
                case AMI_SMPRINT:     fprintf(w, "print\n"); break;
                case AMI_SMEXIT:      fprintf(w, "exit\n"); break;
                case AMI_SMUNDO:      fprintf(w, "undo\n"); break;
                case AMI_SMCUT:       fprintf(w, "cut\n"); break;
                case AMI_SMPASTE:     fprintf(w, "paste\n"); break;
                case AMI_SMDELETE:    fprintf(w, "delete\n"); break;
                case AMI_SMFIND:      fprintf(w, "find\n"); break;
                case AMI_SMFINDNEXT:  fprintf(w, "findnext\n"); break;
                case AMI_SMREPLACE:   fprintf(w, "replace\n"); break;
                case AMI_SMGOTO:      fprintf(w, "goto\n"); break;
                case AMI_SMSELECTALL: fprintf(w, "selectall\n"); break;
                case AMI_SMNEWWINDOW: fprintf(w, "newwindow\n"); break;
                case AMI_SMTILEHORIZ: fprintf(w, "tilehoriz\n"); break;
                case AMI_SMTILEVERT:  fprintf(w, "tilevert\n"); break;
                case AMI_SMCASCADE:   fprintf(w, "cascade\n"); break;
                case AMI_SMCLOSEALL:  fprintf(w, "closeall\n"); break;
                case AMI_SMHELPTOPIC: fprintf(w, "helptopic\n"); break;
                case AMI_SMABOUT:     fprintf(w, "about\n"); break;
                case AMI_SMMAX+1:     fprintf(w, "one\n"); break;
                case AMI_SMMAX+2:     fprintf(w, "two\n"); break;
                case AMI_SMMAX+3:     fprintf(w, "three\n"); break;

            }

        }

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    ami_menu(w, NULL);

}

/* Take the test window down for a phase run on the root window; status
   output and the waits target the root while it is down. */

static void suspendtestwin(void)

{

    fclose(tw);
    tw = stdout; /* status and waits go to the root */
    ami_auto(stdout, ON);
    fprintf(stdout, "\f");

}

/* Restore the desktop banner and bring the test window back. */

static void resumetestwin(void)

{

    ami_long x, y;

    ami_auto(stdout, OFF);
    fprintf(stdout, "\f");
    fprintf(stdout, "Character mode window management test -- this window is the desktop\n");
    ami_openwin(&stdin, &tw, stdout, 2);
    ami_winclient(tw, 80, 25, &x, &y,
                  BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsiz(tw, x, y);
    ami_setpos(tw, 2, 2);
    ami_sizbuf(tw, 80, 25);
    ami_auto(tw, OFF);
    ami_curvis(tw, OFF);

}

/* Run the child windows test with children of the given parent. */

static void childtest(FILE* par)

{

    fputc('\f', par);
    prtcen(ami_maxy(par), "Child windows test character");
    ami_openwin(&stdin, &win2, par, 3);
    ami_curvis(win2, OFF);
    ami_setpos(win2, 1, 10);
    ami_sizbuf(win2, 20, 10);
    ami_setsiz(win2, 20, 10);
    ami_openwin(&stdin, &win3, par, 4);
    ami_curvis(win3, OFF);
    ami_setpos(win3, 21, 10);
    ami_sizbuf(win3, 20, 10);
    ami_setsiz(win3, 20, 10);
    ami_openwin(&stdin, &win4, par, 5);
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
    ami_home(par);
    fprintf(par, "There should be 3 labeled child windows below, with frames   \n");
    fprintf(par, "(the system may not implement frames on child windows)      \n");
    waitnext();
    ami_frame(win2, OFF);
    ami_frame(win3, OFF);
    ami_frame(win4, OFF);
    ami_home(par);
    fprintf(par, "There should be 3 labeled child windows below, without frames\n");
    fprintf(par, "                                                            \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_home(par);
    autowin = 2;
    fprintf(par, "Child windows should all be closed                           \n");
    waitnext();

}

/* Run the independent child windows test with children of the given
   parent; parid is the parent's window id, whose return ends the test. */

static void childindtest(FILE* par, ami_long parid)

{

    ami_curvis(par, ON);
    fputc('\f', par);
    prtcen(ami_maxy(par), "Child windows independent test character");
    ami_openwin(&stdin, &win2, par, 3);
    ami_setpos(win2, 11, 10);
    ami_sizbuf(win2, 30, 10);
    ami_setsiz(win2, 30, 10);
    ami_openwin(&stdin, &win3, par, 4);
    ami_setpos(win3, 41, 10);
    ami_sizbuf(win3, 30, 10);
    ami_setsiz(win3, 30, 10);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_home(par);
    fprintf(par, "There should be 2 labeled child windows below, with frames   \n");
    fprintf(par, "(the system may not implement frames on child windows)       \n");
    fprintf(par, "Test focus can be moved between windows, including the main  \n");
    fprintf(par, "window. Test windows can be minimized and maximized          \n");
    fprintf(par, "(if framed), test entering characters to windows.            \n");
    autowin = parid; /* an automatic return comes from the parent */
    do {

        nextevt(&er); /* get next event */
        if (er.etype == ami_etchar) {

            if (er.winid == 3) fputc(er.echar, win2);
            else if (er.winid == 4) fputc(er.echar, win3);

        } else if (er.etype == ami_etenter) {

            /* translate the crs so we can test scrolling */
            if (er.winid == 3) fputc('\n', win2);
            else if (er.winid == 4) fputc('\n', win3);

        } else if (er.etype == ami_etterm &&
                   (er.winid == 1 || er.winid == parid))
            /* only take terminations from the desktop or the parent */
            longjmp(terminate_buf, 1);

    /* terminate on cr to the parent window only */
    } while (er.etype != ami_etenter || er.winid != parid);
    fclose(win2);
    fclose(win3);
    ami_home(par);
    autowin = 2;
    fprintf(par, "Child windows should all be closed                           \n");
    fprintf(par, "                                                             \n");
    fprintf(par, "                                                             \n");
    fprintf(par, "                                                             \n");
    fprintf(par, "                                                             \n");
    ami_curvis(par, OFF);
    waitnext();

}

/* Child windows stacking test: three overlapped children of the given
   parent, reordered with back and front. */

static void childstacktest(FILE* par)

{

    ami_long i;

    fputc('\f', par);
    prtcen(ami_maxy(par), "Child windows stacking test character");
    ami_openwin(&stdin, &win2, par, 3);
    ami_openwin(&stdin, &win3, par, 4);
    ami_openwin(&stdin, &win4, par, 5);
    for (i = 0; i < 3; i++) {

        FILE* w = i == 0? win2: i == 1? win3: win4;

        ami_curvis(w, OFF);
        ami_setpos(w, 5+10*i, 6+3*i);
        ami_sizbuf(w, 20, 10);
        ami_setsiz(w, 20, 10);

    }
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_bcolor(win4, ami_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    ami_home(par);
    fprintf(par, "There should be 3 labeled child windows below, overlapped,  \n");
    fprintf(par, "with child 1 on the bottom, child 2 middle, and child 3 top.\n");
    waitnext();
    ami_back(win2);
    ami_back(win3);
    ami_back(win4);
    ami_home(par);
    fprintf(par, "Now the windows are reordered, with child 1 on top, child 2 \n");
    fprintf(par, "below that, and child 3 on the bottom.                      \n");
    waitnext();
    ami_front(win2);
    ami_front(win3);
    ami_front(win4);
    ami_home(par);
    fprintf(par, "Now the windows are reordered, with child 3 on top, child 2 \n");
    fprintf(par, "below that, and child 1 on the bottom.                      \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    fputc('\f', par);
    fprintf(par, "Child windows should all be closed                          \n");
    waitnext();

}

/* Child windows stacking resize test: three stacked children over the
   unbuffered parent, moved and resized by hand to exercise the repaint
   speed. Variant 1 gives the children full buffers; variant 2 gives them
   a one line buffer, the rest of each face being background fill. */

static void childstackrsz(FILE* par, ami_long parid, int variant)

{

    ami_long mx, my, xs, ys, i;
    ami_evtrec er;

    ami_buffer(par, OFF);
    ami_auto(par, OFF);
    mx = ami_maxx(par);
    my = ami_maxy(par);
    xs = mx/10; /* stack step */
    ys = my/10;
    if (xs < 2) xs = 2;
    if (ys < 1) ys = 1;
    ami_openwin(&stdin, &win2, par, 3);
    ami_openwin(&stdin, &win3, par, 4);
    ami_openwin(&stdin, &win4, par, 5);
    for (i = 1; i <= 3; i++) {

        FILE* w = i == 1? win2: i == 2? win3: win4;

        ami_curvis(w, OFF);
        ami_setpos(w, xs*i, ys*i);
        if (variant == 1) ami_sizbuf(w, mx-xs*4, my-ys*4);
        else ami_sizbuf(w, 19, 1); /* the label line; the rest is fill */
        ami_setsiz(w, mx-xs*4, my-ys*4);

    }
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
        /* repaint the parent on its own redraw or resize */
        if ((er.etype == ami_etredraw || er.etype == ami_etresize) &&
            er.winid == parid) {

            fputc('\f', par);
            prtcen(ami_maxy(par), variant == 1?
                   "Child windows stacking resize test character 1":
                   "Child windows stacking resize test character 2");
            ami_cursor(par, 1, 1);
            fprintf(par, "move and resize");
            if (er.etype == ami_etresize) {

                /* re-fit the children to the new parent size */
                mx = ami_maxx(par);
                my = ami_maxy(par);
                ami_setsiz(win2, mx-xs*4, my-ys*4);
                ami_setsiz(win3, mx-xs*4, my-ys*4);
                ami_setsiz(win4, mx-xs*4, my-ys*4);

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_buffer(par, ON);
    fputc('\f', par);
    fprintf(par, "Child windows should all be closed                          \n");
    waitnext();

}

static ami_color nextcolor(ami_color c); /* forward */

/* Child windows torture test: place and remove three child windows of
   the given parent against the clock, one hundred times, cycling their
   colors, and report the rate. */

static void childtorture(FILE* par)

{

    ami_long i;

    xs = ami_maxx(par)/4; /* size children to the parent */
    ys = ami_maxy(par)/3;
    if (xs < 8) xs = 8;
    if (ys < 4) ys = 4;
    c1 = ami_red;
    c2 = ami_green;
    c3 = ami_blue;
    fputc('\f', par);
    fprintf(par, "Child windows torture test character\n");
    t = ami_clock(); /* get base time */
    for (i = 1; i <= 100; i++) {

        ami_openwin(&stdin, &win2, par, 3);
        ami_setpos(win2, 2, 3);
        ami_sizbuf(win2, xs, ys);
        ami_setsiz(win2, xs, ys);
        ami_openwin(&stdin, &win3, par, 4);
        ami_setpos(win3, 2+xs, 3);
        ami_sizbuf(win3, xs, ys);
        ami_setsiz(win3, xs, ys);
        ami_openwin(&stdin, &win4, par, 5);
        ami_setpos(win4, 2+xs*2, 3);
        ami_sizbuf(win4, xs, ys);
        ami_setsiz(win4, xs, ys);
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
    ami_home(par);
    fprintf(par, "Child windows should all be closed\n");
    fprintf(par, "\n");
    fprintf(par, "Child windows place and remove %d iterations %f seconds\n",
            100, et*0.0001);
    fprintf(par, "%f per iteration\n", et*0.0001/100);
    waitnext();

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

    /* "window_testc auto" runs every screen with no input, for the
       regression; it ends when the screens do. A following name is the
       file the screens are captured to. "window_testc [first [last]]"
       runs the numbered frames alone. */
    {

        int i;

        for (i = 1; i < argc; i++) {

            if (!strcmp(argv[i], "auto")) {

                autorun = TRUE;
                ami_autohold(FALSE);

            } else if (argv[i][0] >= '0' && argv[i][0] <= '9') {

                if (!tstlo) tstlo = atoi(argv[i]);
                else tsthi = atoi(argv[i]);

            } else if (autorun) screen_capture_name(argv[i]);

        }
        if (tstlo < 1) tstlo = 1;

    }

    /* The root window is the desktop for this test; everything below is
       applied to a child window on it */
    ami_curvis(stdout, OFF);
    ami_auto(stdout, OFF);
    fprintf(stdout, "\f");
    fprintf(stdout, "Character mode window management test -- this window is the desktop\n");

    /* open the window under test, a standard 80x25 terminal surface */
    ami_openwin(&stdin, &tw, stdout, 2);
    ami_winclient(tw, 80, 25, &x, &y,
                  BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsiz(tw, x, y);
    ami_setpos(tw, 2, 2);
    ami_sizbuf(tw, 80, 25);

    ami_auto(tw, OFF);
    ami_curvis(tw, OFF);
    fprintf(tw, "Managed screen test vs. 0.1\n");
    fprintf(tw, "\n");
    /* The graphical form of this test reports each measure twice, once in
       characters and once in pixels. A character surface has only the one
       measure, so each is reported once. */
    ami_scnsiz(tw, &x, &y);
    fprintf(tw, "Screen size character: x: %lld y: %lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    fprintf(tw, "\n");
    ami_getsiz(tw, &x, &y);
    fprintf(tw, "Window size character: x: %lld y: %lld\n", AMI_LONG_CAST(x), AMI_LONG_CAST(y));
    fprintf(tw, "\n");
    fprintf(tw, "Client size character: x: %lld y: %lld\n", AMI_LONG_CAST(ami_maxx(tw)), AMI_LONG_CAST(ami_maxy(tw)));
    fprintf(tw, "\n");
    fprintf(tw, "If this window does not fit the desktop, expand or maximize\n");
    fprintf(tw, "the desktop window, and/or move this window, before continuing\n");
    fprintf(tw, "\n");
    fprintf(tw, "Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Window titling test ************************** */

    ami_title(tw, "This is a mangement test window");
    fprintf(tw, "The title bar of this window should read: This is a mangement test window\n");
    prtcen(ami_maxy(tw), "Window title test");
    waitnextt(TRUE); /* keep the title we just set -- this frame IS the title test */

    /* ************************** Multiple windows ************************** */

    fputc('\f', tw);
    ami_curvis(tw, ON);
    prtcen(ami_maxy(tw), "Multiple window test");
    ami_home(tw);
    ami_auto(tw, ON);
    fprintf(tw, "This is the main window");
    fprintf(tw, "\n");
    fprintf(tw, "Select back and forth between each window, and make sure the\n");
    fprintf(tw, "cursor follows\n");
    fprintf(tw, "\n");
    fprintf(tw, "Here is the cursor->");
    /* the second window is a standard 80x25 terminal like the test window,
       offset on the desktop so both stay reachable */
    ami_openwin(&stdin, &win2, NULL, 3);
    ami_winclient(win2, 80, 25, &x, &y,
                  BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsiz(win2, x, y);
    ami_setpos(win2, 6, 6);
    ami_sizbuf(win2, 80, 25);
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

    /* ****************** Resize screen with buffer on character *************** */

    ox = ami_maxx(tw);
    oy = ami_maxy(tw);
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
    ami_winclient(tw, ox, oy, &ox, &oy, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsiz(tw, ox, oy);

    /* ********************************* Front/back test *********************** */

    /* A reference window on the desktop to flip against. The graphical form
       flips the window against whatever else is on the desktop; here the
       desktop is the root window, so the test provides the neighbor. */
    xs = 30;
    ys = 10;
    ami_openwin(&stdin, &win2, NULL, 3);
    ami_winclient(win2, xs, ys, &x, &y,
                  BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsiz(win2, x, y);
    ami_sizbuf(win2, xs, ys);
    ami_setpos(win2, 20, 8);
    ami_bcolor(win2, ami_yellow);
    fputc('\f', win2);
    fprintf(win2, "reference window\n");
    fputc('\f', tw);
    ami_auto(tw, OFF);
    fprintf(tw, "Position this window over the reference window\n");
    fprintf(tw, "Then hit space to flip front/back status, or return to stop\n");
    fb = FALSE; /* clear front/back status */

    do {

        nextevt(&er);
        if (er.etype == ami_etchar) if (er.echar == ' ') { /* flip front/back */

            fb = !fb;
            if (fb) {

                ami_front(tw);
                ami_fcolor(tw, ami_white);
                prtcen(ami_maxy(tw)/2, "Back");
                ami_fcolor(tw, ami_black);
                prtcen(ami_maxy(tw)/2, "Front");

            } else {

                ami_back(tw);
                ami_fcolor(tw, ami_white);
                prtcen(ami_maxy(tw)/2, "Front");
                ami_fcolor(tw, ami_black);
                prtcen(ami_maxy(tw)/2, "Back");

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    fclose(win2);
    ami_home(tw);
    ami_auto(tw, ON);

    /* ************************* Frame controls test buffered ****************** */

    fputc('\f', tw);
    ami_fcolor(tw, ami_cyan);
    charbox(tw, ami_maxx(tw), ami_maxy(tw));
    ami_home(tw);
    ami_fcolor(tw, ami_black);
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

    /* build the sample menu, used on the root window and the test window */
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

    /* The menu presents on the root window as well as child windows. The
       test window is taken down so the desktop carries the menu alone;
       the root is frameless, so the bar takes its first line. */
    suspendtestwin();
    samplemenu(stdout);
    stdmenutest(stdout);
    resumetestwin();

    /* and the same menu presents on it */
    ami_auto(tw, ON);
    fputc('\f', tw);
    ami_fcolor(tw, ami_cyan);
    charbox(tw, ami_maxx(tw), ami_maxy(tw));
    ami_fcolor(tw, ami_black);
    samplemenu(tw);

    /* ****************************** Standard menu test ******************** */

    stdmenutest(tw);

    /* ************************* Child windows test character ****************** */

    /* on the root window first, then on the test window */
    suspendtestwin();
    childtest(stdout);
    resumetestwin();
    childtest(tw);

    /* *************** Child windows independent test character ************ */

    /* on the root window first, then on the test window */
    suspendtestwin();
    childindtest(stdout, 1);
    resumetestwin();
    childindtest(tw, 2);

    /* ************** Child windows stacking test character ***************** */

    /* on the root window first, then on the test window */
    suspendtestwin();
    childstacktest(stdout);
    resumetestwin();
    childstacktest(tw);

    /* ********* Child windows stacking resize test character 1 ************* */

    suspendtestwin();
    childstackrsz(stdout, 1, 1);
    resumetestwin();
    childstackrsz(tw, 2, 1);

    /* ********* Child windows stacking resize test character 2 ************* */

    suspendtestwin();
    childstackrsz(stdout, 1, 2);
    resumetestwin();
    childstackrsz(tw, 2, 2);

    /* ******************************* Buffer off test *********************** */

    fputc('\f', tw);
    ami_auto(tw, OFF);
    ami_buffer(tw, OFF);
    /* initialize prime size information */
    x = ami_maxx(tw);
    y = ami_maxy(tw);
    do {

        nextevt(&er); /* get next event */
        if (er.etype == ami_etresize) {

            /* The new dimensions, taken before any drawing: drawing first
               boxed the window at the old size, and the redraw that
               followed boxed it again at the new -- a doubled border. */
            x = ami_maxx(tw);
            y = ami_maxy(tw);

        }
        if (er.etype == ami_etredraw || er.etype == ami_etresize) {

            /* The graphical form fills a rectangle here, which is also
               its clear; the character clear is the form feed. */
            fputc('\f', tw);
            prtcen(ami_maxy(tw)/2,
                    "SIZE AND COVER ME !");
            charbox(tw, x, y); /* frame the window */

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_buffer(tw, ON);
    ami_home(tw);
    ami_auto(tw, ON);

    /* ****************************** min/max/norm test ********************* */

    fputc('\f', tw);
    ami_auto(tw, OFF);
    ami_buffer(tw, OFF);
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
    prtcen(ami_maxy(tw), "Window size calculate character");
    ami_home(tw);
    ami_openwin(&stdin, &win2, tw, 3); /* a child of the test window */
    ami_setpos(win2, 30, 8);

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
    ami_curvis(win2, OFF);
    fprintf(tw, "Check client window has (20, 10) surface\n");
    waitnext();

    fclose(win2);

    /* ******************* Child windows torture test character ************** */

    /* on the root window first, then on the test window */
    suspendtestwin();
    childtorture(stdout);
    resumetestwin();
    childtorture(tw);

    terminate: /* terminate */

    fputc('\f', tw);
    ami_auto(tw, OFF);
    prtcen(ami_maxy(tw)/2, "Test complete");

}
