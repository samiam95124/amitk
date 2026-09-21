/*******************************************************************************
*                                                                              *
*                   WIDGET TEST PROGRAM, CHARACTER MODE                        *
*                                                                              *
*                    Copyright (C) 2005 Scott A. Franco                        *
*                                                                              *
* Tests the character mode widgets and dialogs. The root window serves as the  *
* desktop: all testing is applied to a child window, a standard 80x25 terminal *
* surface, the way the graphical form of this test applies to a program window *
* on the desktop. Maximize the root window to give the test room to work.      *
*                                                                              *
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

#define SECOND 10000 /* one second timer */

static jmp_buf       terminate_buf;
static ami_evtrec     er;
static int           chk, chk2, chk3;
static char          s[100];
static char          ss[100], rs[100];
static int           prog;
static ami_strptr     sp, lp;
static ami_long      x, y, lm, xs, ys, bx, by, ix,iy;
static ami_long      r, g, b;
static ami_qfnopts    optf;
static ami_qfropts    optfr;
static ami_long      fc;
static ami_long      fs;
static ami_long      fr, fg, fb;
static ami_long      br, bg, bb;
static ami_qfteffects fe;
static ami_long      cx, cy;
static ami_long      ox, oy;
static ami_long      cox, coy;
static ami_long      csx, csy;

static ami_long      i;
static int           cnt;
static char          fns[100];

/* allocate memory with error checking */
static void *imalloc(size_t size)

{

    void* p;

    p = malloc(size);
    if (!p) {

        fprintf(stderr, "*** Out of memory ***\n");
        exit(1);

    }

    return (p);

}

/* get string in dynamic storage */
static char* str(char* s)

{

    char* p;

    p = imalloc(strlen(s)+1);
    strcpy(p, s);

    return (p);

}

static int framenum = 0; /* current frame number */
/* The frames selected: widget_testc <start> [<end>] runs the frames from
   start to end, or to the last. Before the range every wait answers itself
   and nothing is captured; past it the run ends. 0 for no limit. */
static int tstlo = 0; /* first frame in the selected range */
static int tsthi = 0; /* last frame, 0 for no limit */
static int autorun = FALSE; /* walk every screen with no input */

extern void screen_capture(void);
extern void screen_capture_name(const char* fn);

/* "widget_testc auto" walks every screen with no input at all, capturing
   each, and exits at the end: this is how the regression runs it, as it
   runs window_testc. Widgets are windows of their own and paint from events,
   so an automatic run pumps events for a moment to let the screen settle
   before capturing it, then answers the wait with a return. */
#define AUTOSETL 3000 /* settle time before a capture, 100us units */
#define AUTOSKIP 100  /* the events a skipped frame's wait lets through */
#define AUTOTIM  9    /* timer the settle runs on */

static void settle(int t)

{

    ami_evtrec er;

    ami_timer(stdout, AUTOTIM, t, FALSE);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_ettim || er.timnum != AUTOTIM);

}

/* Mark the page with its frame number, as terminal_test marks its pages: on
   the top line at the right, five columns in from the edge, the cursor put
   back where it was and the colors left alone, so the label prints in the
   frame's own. The root window has no title bar to carry the number, as the
   child window had. */
#define LABELW 12 /* the label's field */
static void frmmark(void)

{

    char lbl[40], buf[40];
    ami_long cx, cy;

    sprintf(lbl, "frame %d", framenum);
    sprintf(buf, "%*s", LABELW, lbl); /* right in its field */
    cx = ami_curx(stdout);
    cy = ami_cury(stdout);
    ami_cursor(stdout, ami_maxx(stdout)-5-LABELW+1, 1);
    printf("%s", buf);
    ami_cursor(stdout, cx, cy);

}

/* Every wait for the user comes through here. Before the selected range the
   wait answers itself, after the port has had its events for a moment. An
   automatic run lets the screen settle, captures it, and answers with the
   return the screen was waiting for, from the window the test is on. */
static void nextevt(ami_evtrec* er)

{

    if (framenum < tstlo) { /* before the range */

        settle(AUTOSKIP);
        er->etype = ami_etenter;
        er->winid = 1;
        return;

    }
    if (autorun) {

        settle(AUTOSETL);
        frmmark();
        screen_capture();
        er->etype = ami_etenter;
        er->winid = 1;
        return;

    }
    ami_event(stdin, er);

}

/* set the window title with the chapter frame number, as graphics_test does;
   called at the start of each chapter so every screen is numbered, including
   the interactive ones that wait on their own event loop instead of waitnext */
static void setframe(void)

{

    char titlebuf[80];

    framenum++;
    if (tsthi && framenum > tsthi) longjmp(terminate_buf, 1); /* past the range */
    sprintf(titlebuf, "widget_test: frame %d", framenum);
    ami_title(stdout, titlebuf);

}

/* wait return to be pressed, or handle terminate */
static void waitnext(void)

{

    ami_evtrec er; /* event record */

    do { nextevt(&er); }
    while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

}

int main(int argc, char* argv[])

{

    /* widget_testc [auto [<capture>]] [<start> [<end>]]

       "auto" runs every screen with no input, for the regression; it ends
       when the screens do. A name after it is the file the screens are
       captured to. Numbers select the frames to run, from the first to the
       second or to the last. */
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

    }

    if (setjmp(terminate_buf)) goto terminate;

    /* The widgets are laid on the root window, as widget_test lays them on
       its window: the terminal is the surface under test. The test runs on
       the window's second screen, as terminal_test does, so the first, the
       one it was started from, is as it was when it ends. */
    ami_select(stdout, 2, 2);
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    printf("Widget test vs. 0.1\n");
    printf("\n");
    printf("Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Character Button test ************************* */

    setframe();

    printf("\f");
    printf("Character buttons test\n");
    printf("\n");
    ami_buttonsiz(stdout, "Hello, there", &x, &y);
    ami_button(stdout, 10, 7, 10+x-1, 7+y-1, "Hello, there", 1); 
    ami_buttonsiz(stdout, "Bark!", &x, &y);
    ami_button(stdout, 10, 10, 10+x-1, 10+y-1, "Bark!", 2);
    ami_buttonsiz(stdout, "Sniff", &x, &y);
    ami_button(stdout, 10, 13, 10+x-1, 13+y-1, "Sniff", 3);
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        };
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle button is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        };
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* ************************** Character Checkbox test ************************** */

    setframe();

    printf("\f");
    printf("Character checkbox test\n");
    printf("\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    ami_checkboxsiz(stdout, "Pick me", &x, &y);
    ami_checkbox(stdout, 10, 7, 10+x-1, 7+y-1, "Pick me", 1);
    ami_checkboxsiz(stdout, "Or me", &x, &y);
    ami_checkbox(stdout, 10, 10, 10+x-1, 10+y-1, "Or me", 2);
    ami_checkboxsiz(stdout, "No, me", &x, &y);
    ami_checkbox(stdout, 10, 13, 10+x-1, 13+y-1, "No, me", 3);
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etchkbox) {

            if (er.ckbxid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(stdout, 1, chk);

            } else if (er.ckbxid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(stdout, 2, chk2);

            } else if (er.ckbxid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle checkbox is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etchkbox) {

            if (er.ckbxid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(stdout, 1, chk);

            } else if (er.ckbxid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(stdout, 2, chk2);

            } else if (er.ckbxid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* *********************** Character radio button test ********************* */

    setframe();

    printf("\f");
    printf("Character radio button test\n");
    printf("\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    ami_radiobuttonsiz(stdout, "Station 1", &x, &y);
    ami_radiobutton(stdout, 10, 7, 10+x-1, 7+y-1, "Station 1", 1);
    ami_radiobuttonsiz(stdout, "Station 2", &x, &y);
    ami_radiobutton(stdout, 10, 10, 10+x-1, 10+y-1, "Station 2", 2);
    ami_radiobuttonsiz(stdout, "Station 3", &x, &y);
    ami_radiobutton(stdout, 10, 13, 10+x-1, 13+y-1, "Station 3", 3);
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etradbut) {

            if (er.radbid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(stdout, 1, chk);

            } else if (er.radbid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(stdout, 2, chk2);

            } else if (er.radbid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %lld !!!", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle radio button is disabled, and should not be able\n");
    printf("to be pressed.\n");
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etradbut) {

            if (er.radbid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(stdout, 1, chk);

            } else if (er.radbid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(stdout, 2, chk2);

            } else if (er.radbid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* *********************** Character Group box test ************************ */

    setframe();

    printf("\f");
    printf("Character group box test\n");
    printf("\n");
    ami_groupsiz(stdout, "Hello there", 0, 0, &x, &y, &ox, &oy);
    ami_group(stdout, 10, 10, 10+x, 10+y, "Hello there", 1);
    printf("This is a group box with a null client area\n");
    printf("Hit return to continue\n");
    waitnext();
    ami_killwidget(stdout, 1);
    ami_groupsiz(stdout, "Hello there", 20, 10, &x, &y, &ox, &oy);
    ami_group(stdout, 10, 10, 10+x, 10+y, "Hello there", 1);
    printf("This is a group box with a 20,10 client area\n");
    printf("Hit return to continue\n");
    waitnext();
    ami_killwidget(stdout, 1);
    ami_groupsiz(stdout, "Hello there", 20, 10, &x, &y, &ox, &oy);
    ami_group(stdout, 10, 10, 10+x, 10+y, "Hello there", 1);
    ami_button(stdout, 10+ox, 10+oy, 10+ox+20-1, 10+oy+10-1, "Bark, bark!", 2);
    printf("This is a group box with a 20,10 layered button\n");
    printf("Hit return to continue\n");
    waitnext();
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* *********************** Character background test ************************ */

    setframe();

    printf("\f");
    printf("Character background test\n");
    printf("\n");
    ami_background(stdout, 10, 10, 40, 20, 1);
    printf("Hit return to continue\n");
    waitnext();
    ami_button(stdout, 11, 11, 39, 19, "Bark, bark!", 2);
    printf("This is a background with a layered button\n");
    printf("Hit return to continue\n");
    waitnext();
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* *********************** Character scroll bar test *********************** */

    setframe();

    printf("\f");
    printf("Character scroll bar test\n");
    printf("\n");
    ami_scrollvertsiz(stdout, &x, &y);
    ami_scrollvert(stdout, 10, 10, 10+x-1, 20, 1);
    ami_scrollhorizsiz(stdout, &x, &y);
    ami_scrollhoriz(stdout, 15, 10, 35, 10+y-1, 2);
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* ******************* Character scroll bar sizing test ******************** */

    setframe();

    printf("\f");
    printf("Character scroll bar sizing test\n");
    printf("\n");
    ami_scrollvert(stdout, 10, 10, 12, 20, 1);
    ami_scrollsiz(stdout, 1, (LONG_MAX / 4)*3);
    ami_scrollvert(stdout, 10+5, 10, 12+5, 20, 2);
    ami_scrollsiz(stdout, 2, LONG_MAX / 2);
    ami_scrollvert(stdout, 10+10, 10, 12+10, 20, 3);
    ami_scrollsiz(stdout, 3, LONG_MAX / 4);
    ami_scrollvert(stdout, 10+15, 10, 12+15, 20, 4);
    ami_scrollsiz(stdout, 4, LONG_MAX / 8);
    printf("Now should be four scrollbars, decending in size to the right.\n");
    printf("All of the scrollbars can be manipulated.\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ****************** Character scroll bar minimums test ******************* */

    setframe();

    printf("\f");
    printf("Character scroll bar minimums test\n");
    printf("\n");
    ami_scrollvertsiz(stdout, &x, &y);
    ami_scrollvert(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    ami_scrollhorizsiz(stdout, &x, &y);
    ami_scrollhoriz(stdout, 15, 10, 15+x-1, 10+y-1, 2);
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* ************ Character scroll bar fat and skinny bars test ************** */

    setframe();

    printf("\f");
    printf("Character scroll bar fat and skinny bars test\n");
    printf("\n");
    ami_scrollvertsiz(stdout, &x, &y);
    ami_scrollvert(stdout, 10, 10, 10, 10+10, 1);
    ami_scrollvert(stdout, 12, 10, 20, 10+10, 3);
    ami_scrollhorizsiz(stdout, &x, &y);
    ami_scrollhoriz(stdout, 30, 10, 30+20, 10, 2);
    ami_scrollhoriz(stdout, 30, 12, 30+20, 20, 4);
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ******************** Character number select box test ******************* */

    setframe();

    printf("\f");
    printf("Character number select box test\n");
    printf("\n");
    ami_numselboxsiz(stdout, 1, 10, &x, &y);
    ami_numselbox(stdout, 10, 10, 10+x-1, 10+y-1, 1, 10, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etnumbox) printf("You selected: %lld\n", AMI_LONG_CAST(er.numbsl));
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Character edit box test ************************ */

    setframe();

    printf("\f");
    printf("Character edit box test\n");
    printf("\n");
    ami_editboxsiz(stdout, "Hi there, george", &x, &y);
    ami_editbox(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    ami_putwidgettext(stdout, 1, "Hi there, george");
    do {

        nextevt(&er);
        if (er.etype == ami_etedtbox) {

            ami_getwidgettext(stdout, 1, s, 100);
            printf("You entered: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* *********************** Character progress bar test ********************* */

    setframe();

    printf("\f");
    printf("Character progress bar test\n");
    printf("\n");
    ami_progbarsiz(stdout, &x, &y);
    ami_progbar(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    ami_timer(stdout, 1, SECOND, TRUE);
    prog = 1;
    do {

        nextevt(&er);
        if (er.etype == ami_ettim) {

            if (prog < 20) {

                ami_progbarpos(stdout, 1, LONG_MAX-((20-prog)*(LONG_MAX / 20)));
                prog = prog+1; /* next progress value */

            } else if (prog == 20) {

                ami_progbarpos(stdout, 1, LONG_MAX);
                printf("Done !\n");
                prog = 11;
                ami_killtimer(stdout, 1);

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Character list box test ************************ */

    setframe();

    printf("\f");
    printf("Character list box test\n");
    printf("\n");
    printf("Note that it is normal for this box to not fill to exact\n");
    printf("character cells.\n");
    printf("\n");
    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Blue");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Red");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Green");
    sp->next = lp;
    lp = sp;
    ami_listboxsiz(stdout, lp, &x, &y);
    ami_listbox(stdout, 10, 10, 10+x-1, 10+y-1, lp, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etlstbox) {

            switch (er.lstbsl) {

                case 1: printf("You selected pa_green\n"); break;
                case 2: printf("You selected pa_red\n"); break;
                case 3: printf("You selected pa_blue\n"); break;
                default: printf("!!! Bad select number !!!\n");

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Character dropdown box test ************************ */

    setframe();

    printf("\f");
    printf("Character dropdown box test\n");
    printf("\n");
    printf("Note that it is normal for this box to not fill to exact\n");
    printf("character cells.\n");
    printf("\n");
    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("dog");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("cat");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("bird");
    sp->next = lp;
    lp = sp;
    ami_dropboxsiz(stdout, lp, &cx, &cy, &ox, &oy);
    ami_dropbox(stdout, 10, 10, 10+ox-1, 10+oy-1, lp, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etdrpbox) {

            switch (er.drpbsl) {

                case 1: printf("You selected Bird\n"); break;
                case 2: printf("You selected Cat\n"); break;
                case 3: printf("You selected Dog\n"); break;
                default: printf("!!! Bad select number !!!\n");

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ******************* Character dropdown edit box test ******************** */

    setframe();

    printf("\f");
    printf("Character dropdown edit box test\n");
    printf("\n");
    printf("Note that it is normal for this box to not fill to exact\n");
    printf("character cells.\n");
    printf("\n");
    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("corn");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("flower");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Tortillas");
    sp->next = lp;
    lp = sp;
    ami_dropeditboxsiz(stdout, lp, &cx, &cy, &ox, &oy);
    ami_dropeditbox(stdout, 10, 10, 10+ox-1, 10+oy-1, lp, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etdrebox) {

            ami_getwidgettext(stdout, 1, s, 100);
            printf("You selected: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Character slider test ************************ */

    setframe();

    printf("\f");
    printf("Character slider test\n");
    ami_slidehorizsiz(stdout, &x, &y);
    x = 20;
    ami_slidehoriz(stdout, 10, 10, 10+x-1, 10+y-1, 10, 1);
    ami_slidehoriz(stdout, 10, 20, 10+x-1, 20+y-1, 0, 2);
    ami_slidevertsiz(stdout, &x, &y);
    y = 10;
    ami_slidevert(stdout, 40, 10, 40+x-1, 10+y-1, 10, 3);
    ami_slidevert(stdout, 50, 10, 50+x-1, 10+y-1, 0, 4);
    printf("Bottom and right sliders should not have tick marks\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etsldpos)
            printf("Slider id: %lld position: %lld\n", AMI_LONG_CAST(er.sldpid), AMI_LONG_CAST(er.sldpos));
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ************************* Character tab bar test ************************ */

    setframe();

    printf("\f");
    printf("Character tab bar test\n");
    printf("\n");

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Right");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Left");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_totop, 20, 2, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 15, 3, 15+x-1, 3+y-1, lp, ami_totop, 1);

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Bottom");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Top");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_toright, 4, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 37, 7, 37+x-1, 7+y-1, lp, ami_toright, 2);

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Right");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Left");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_tobottom, 20, 2, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 15, 19, 15+x-1, 19+y-1, lp, ami_tobottom, 3);

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Bottom");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Top");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_toleft, 4, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 5, 7, 5+x-1, 7+y-1, lp, ami_toleft, 4);

    do {

        nextevt(&er);
        if (er.etype == ami_ettabbar) {

            if (er.tabid == 1) switch (er.tabsel) {

                case 1: printf("Top bar: You selected Left\n"); break;
                case 2: printf("Top bar: You selected Center\n"); break;
                case 3: printf("Top bar: You selected Right\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 2) switch (er.tabsel) {

                case 1: printf("Right bar: You selected Top\n"); break;
                case 2: printf("Right bar: You selected Center\n"); break;
                case 3: printf("Right bar: You selected Bottom\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 3) switch (er.tabsel) {

                case 1: printf("Bottom bar: You selected Left\n"); break;
                case 2: printf("Bottom bar: You selected Center\n"); break;
                case 3: printf("Bottom bar: You selected right\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 4) switch (er.tabsel) {

                case 1: printf("Left bar: You selected Top\n"); break;
                case 2: printf("Left bar: You selected Center\n"); break;
                case 3: printf("Left bar: You selected Bottom\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else printf("!!! Bad tab id !!!\n");

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ************************* Character overlaid tab bar test ************************ */

    setframe();

    printf("\f");
    printf("Character overlaid tab bar test\n");
    printf("\n");

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Right");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Left");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_totop, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_totop, 1);

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Bottom");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Top");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_toright, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_toright, 2);

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Right");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Left");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_tobottom, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_tobottom, 3);

    lp = (ami_strptr)imalloc(sizeof(ami_strrec));
    lp->str = str("Bottom");
    lp->next = NULL;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Center");
    sp->next = lp;
    lp = sp;
    sp = (ami_strptr)imalloc(sizeof(ami_strrec));
    sp->str = str("Top");
    sp->next = lp;
    lp = sp;
    ami_tabbarsiz(stdout, lp, ami_toleft, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_toleft, 4);

    do {

        nextevt(&er);
        if (er.etype == ami_ettabbar) {

            if (er.tabid == 1) switch (er.tabsel) {

                case 1: printf("Top bar: You selected Left\n"); break;
                case 2: printf("Top bar: You selected Center\n"); break;
                case 3: printf("Top bar: You selected Right\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 2) switch (er.tabsel) {

                case 1: printf("Right bar: You selected Top\n"); break;
                case 2: printf("Right bar: You selected Center\n"); break;
                case 3: printf("Right bar: You selected Bottom\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 3) switch (er.tabsel) {

                case 1: printf("Bottom bar: You selected Left\n"); break;
                case 2: printf("Bottom bar: You selected Center\n"); break;
                case 3: printf("Bottom bar: You selected right\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 4) switch (er.tabsel) {

                case 1: printf("Left bar: You selected Top\n"); break;
                case 2: printf("Left bar: You selected Center\n"); break;
                case 3: printf("Left bar: You selected Bottom\n"); break;
                default: printf("!!! Bad select number !!!\n"); break;

            } else printf("!!! Bad tab id !!!\n");

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ************************* Alert test ************************ */

    setframe();

    printf("\f");
    printf("Alert test\n");
    printf("\n");
    printf("There should be an pa_alert dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_alert("This is an important message", "There has been an event !\n");
    printf("\n");
    printf("Alert dialog should have completed now\n");
    waitnext();

    /* ************************* Color query test ************************ */

    setframe();

    printf("\f");
    printf("Color query test\n");
    printf("\n");
    printf("There should be an color query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The color pa_white should be the default selection\n");
    r = LONG_MAX;
    g = LONG_MAX;
    b = LONG_MAX;
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_querycolor(&r, &g, &b);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Colors are: red: %lld green: %lld blue: %lld\n", AMI_LONG_CAST(r), AMI_LONG_CAST(g), AMI_LONG_CAST(b));
    waitnext();

    /* ************************* Open file query test ************************ */

    setframe();

    printf("\f");
    printf("Open file query test\n");
    printf("\n");
    printf("There should be an open file query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have \"myfile.txt\" as the default filename\n");
    strcpy(s, "myfile.txt");
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_queryopen(s, 100);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Filename is: %s\n", s);
    waitnext();

    /* ************************* Save file query test ************************ */

    setframe();

    printf("\f");
    printf("Save file query test\n");
    printf("\n");
    printf("There should be an save file query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have \"myfile.txt\" as the default filename\n");
    strcpy(s, "myfile.txt");
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_querysave(s, 100);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Filename is: %s\n", s);
    waitnext();

    /* ************************* Find query test ************************ */

    setframe();

    printf("\f");
    printf("Find query test\n");
    printf("\n");
    printf("There should be a find query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have \"mystuff\" as the default search string\n");
    strcpy(s, "mystuff");
    optf = 0;
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_queryfind(s, 100, &optf);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Search string is: \"%s\"\n", s);
    if (BIT(ami_qfncase)&optf) printf("Case sensitive is on\n");
    else printf("Case sensitive is off\n");
    if (BIT(ami_qfnup)&optf) printf("Search up\n");
    else printf("Search down\n");
    if (BIT(ami_qfnre)&optf) printf("Use regular expression\n");
    else printf("Use literal expression\n");
    waitnext();

    /* ************************* Find/replace query test ************************ */

    setframe();

    printf("\f");
    printf("Find/replace query test\n");
    printf("\n");
    printf("There should be a find/replace query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have \"bark\" as the default search string\n");
    printf("and should have \"sniff\" as the default replacement string\n");
    strcpy(ss, "bark");
    strcpy(rs, "sniff");
    optfr = 0;
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_queryfindrep (ss, 100, rs, 100, &optfr);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Search string is: \"%s\"\n", ss);
    printf("Replace string is: \"%s\"\n", rs);
    if (BIT(ami_qfrcase)&optfr) printf("Case sensitive is on\n");
    else printf("Case sensitive is off\n");
    if (BIT(ami_qfrup)&optfr) printf("Search/replace up\n");
    else printf("Search/replace down\n");
    if (BIT(ami_qfrre)&optfr) printf("Regular expressions are on\n");
    else printf("Regular expressions are off\n");
    if (BIT(ami_qfrfind)&optfr) printf("Mode is find\n");
    else printf("Mode is find/replace\n");
    if (BIT(ami_qfrallfil)&optfr) printf("Mode is find/replace all in file\n");
    else printf("Mode is find/replace first in file\n");
    if (BIT(ami_qfralllin)&optfr) printf("Mode is find/replace all on line(s)\n");
    else printf("Mode is find/replace first on line(s)\n");
    waitnext();

    /* ************************* Font query test ************************ */

    setframe();

    printf("\f");
    printf("Font query test\n");
    printf("\n");
    printf("There should be a font query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    /* A character terminal has one font at one size, so the dialog offers
       the colors and the effects it can present; the font and size come
       back as they went in. */
    fc = 1;
    fs = 1;
    fr = 0; /* set foreground to black */
    fg = 0;
    fb = 0;
    br = LONG_MAX; /* set background to white */
    bg = LONG_MAX;
    bb = LONG_MAX;
    fe = 0;
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_queryfont(stdout, &fc, &fs, &fr, &fg, &fb, &br, &bg, &bb, &fe);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Font code: %lld\n", AMI_LONG_CAST(fc));
    printf("Font size: %lld\n", AMI_LONG_CAST(fs));
    printf("Foreground color: Red: %lld Green: %lld Blue: %lld\n", AMI_LONG_CAST(fr), AMI_LONG_CAST(fg), AMI_LONG_CAST(fb));
    printf("Background color: Red: %lld Green: %lld Blue: %lld\n", AMI_LONG_CAST(br), AMI_LONG_CAST(bg), AMI_LONG_CAST(bb));
    if (BIT(ami_qfteblink)&fe) printf("Blink\n");
    if (BIT(ami_qftereverse)&fe) printf("Reverse\n");
    if (BIT(ami_qfteunderline)&fe) printf("Underline\n");
    if (BIT(ami_qftesuperscript)&fe) printf("Superscript\n");
    if (BIT(ami_qftesubscript)&fe) printf("Subscript\n");
    if (BIT(ami_qfteitalic)&fe) printf("Italic\n");
    if (BIT(ami_qftebold)&fe) printf("Bold\n");
    if (BIT(ami_qftestrikeout)&fe) printf("Strikeout\n");
    if (BIT(ami_qftestandout)&fe) printf("Standout\n");
    if (BIT(ami_qftecondensed)&fe) printf("Condensed\n");
    if (BIT(ami_qfteextended)&fe) printf("Extended\n");
    if (BIT(ami_qftexlight)&fe) printf("Xlight\n");
    if (BIT(ami_qftelight)&fe) printf("Light\n");
    if (BIT(ami_qftexbold)&fe) printf("Xbold\n");
    if (BIT(ami_qftehollow)&fe) printf("Hollow\n");
    if (BIT(ami_qfteraised)&fe) printf("Raised\n");
    waitnext();

    terminate:;

    /* back to the first screen, as it was, with the cursor and the wrap
       the test turned off */
    ami_select(stdout, 1, 1);
    ami_curvis(stdout, TRUE);
    ami_auto(stdout, TRUE);
    printf("\n");
    printf("Test complete\n");

}
