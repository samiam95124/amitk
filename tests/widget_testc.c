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
static FILE*         tw; /* the window under test */
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

/* Every wait for the user comes through here. Before the selected range the
   wait answers itself, after the port has had its events for a moment. An
   automatic run lets the screen settle, captures it, and answers with the
   return the screen was waiting for, from the window the test is on. */
static void nextevt(ami_evtrec* er)

{

    if (framenum < tstlo) { /* before the range */

        settle(AUTOSKIP);
        er->etype = ami_etenter;
        er->winid = 2;
        return;

    }
    if (autorun) {

        settle(AUTOSETL);
        screen_capture();
        er->etype = ami_etenter;
        er->winid = 2;
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
    ami_title(tw, titlebuf);

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

    ami_long wx, wy;

    if (setjmp(terminate_buf)) goto terminate;

    /* The root window is the desktop for this test; everything below is
       applied to a child window on it. */
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    printf("\f");
    printf("Character mode widget test -- this window is the desktop\n");

    /* open the window under test, a standard 80x25 terminal surface */
    ami_openwin(&stdin, &tw, stdout, 2);
    ami_winclient(tw, 80, 25, &wx, &wy,
                  BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsiz(tw, wx, wy);
    ami_setpos(tw, 2, 2);
    ami_sizbuf(tw, 80, 25);
    ami_curvis(tw, FALSE);

    fprintf(tw, "Widget test vs. 0.1\n");
    fprintf(tw, "\n");
    fprintf(tw, "If this window does not fit the desktop, expand or maximize\n");
    fprintf(tw, "the desktop window, and/or move this window, before continuing\n");
    fprintf(tw, "\n");
    fprintf(tw, "Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Character Button test ************************* */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character buttons test\n");
    fprintf(tw, "\n");
    ami_buttonsiz(tw, "Hello, there", &x, &y);
    ami_button(tw, 10, 7, 10+x-1, 7+y-1, "Hello, there", 1); 
    ami_buttonsiz(tw, "Bark!", &x, &y);
    ami_button(tw, 10, 10, 10+x-1, 10+y-1, "Bark!", 2);
    ami_buttonsiz(tw, "Sniff", &x, &y);
    ami_button(tw, 10, 13, 10+x-1, 13+y-1, "Sniff", 3);
    fprintf(tw, "Hit the buttons, or return to continue\n");
    fprintf(tw, "\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) fprintf(tw, "Hello to you, too\n");
            else if (er.butid == 2) fprintf(tw, "Bark bark\n");
            else if (er.butid == 3) fprintf(tw, "Sniff sniff\n");
            else fprintf(tw, "!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        };
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(tw, 2, FALSE);
    fprintf(tw, "Now the middle button is disabled, and should not be able to\n");
    fprintf(tw, "be pressed.\n");
    fprintf(tw, "Hit the buttons, or return to continue\n");
    fprintf(tw, "\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) fprintf(tw, "Hello to you, too\n");
            else if (er.butid == 2) fprintf(tw, "Bark bark\n");
            else if (er.butid == 3) fprintf(tw, "Sniff sniff\n");
            else fprintf(tw, "!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        };
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);

    /* ************************** Character Checkbox test ************************** */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character checkbox test\n");
    fprintf(tw, "\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    ami_checkboxsiz(tw, "Pick me", &x, &y);
    ami_checkbox(tw, 10, 7, 10+x-1, 7+y-1, "Pick me", 1);
    ami_checkboxsiz(tw, "Or me", &x, &y);
    ami_checkbox(tw, 10, 10, 10+x-1, 10+y-1, "Or me", 2);
    ami_checkboxsiz(tw, "No, me", &x, &y);
    ami_checkbox(tw, 10, 13, 10+x-1, 13+y-1, "No, me", 3);
    fprintf(tw, "Hit the checkbox, or return to continue\n");
    fprintf(tw, "\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etchkbox) {

            if (er.ckbxid == 1) {

                fprintf(tw, "You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(tw, 1, chk);

            } else if (er.ckbxid == 2) {

                fprintf(tw, "You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(tw, 2, chk2);

            } else if (er.ckbxid == 3) {

                fprintf(tw, "You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(tw, 3, chk3);

            } else fprintf(tw, "!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(tw, 2, FALSE);
    fprintf(tw, "Now the middle checkbox is disabled, and should not be able to\n");
    fprintf(tw, "be pressed.\n");
    fprintf(tw, "Hit the checkbox, or return to continue\n");
    fprintf(tw, "\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etchkbox) {

            if (er.ckbxid == 1) {

                fprintf(tw, "You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(tw, 1, chk);

            } else if (er.ckbxid == 2) {

                fprintf(tw, "You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(tw, 2, chk2);

            } else if (er.ckbxid == 3) {

                fprintf(tw, "You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(tw, 3, chk3);

            } else fprintf(tw, "!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);

    /* *********************** Character radio button test ********************* */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character radio button test\n");
    fprintf(tw, "\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    ami_radiobuttonsiz(tw, "Station 1", &x, &y);
    ami_radiobutton(tw, 10, 7, 10+x-1, 7+y-1, "Station 1", 1);
    ami_radiobuttonsiz(tw, "Station 2", &x, &y);
    ami_radiobutton(tw, 10, 10, 10+x-1, 10+y-1, "Station 2", 2);
    ami_radiobuttonsiz(tw, "Station 3", &x, &y);
    ami_radiobutton(tw, 10, 13, 10+x-1, 13+y-1, "Station 3", 3);
    fprintf(tw, "Hit the radio button, or return to continue\n");
    fprintf(tw, "\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etradbut) {

            if (er.radbid == 1) {

                fprintf(tw, "You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(tw, 1, chk);

            } else if (er.radbid == 2) {

                fprintf(tw, "You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(tw, 2, chk2);

            } else if (er.radbid == 3) {

                fprintf(tw, "You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(tw, 3, chk3);

            } else fprintf(tw, "!!! No button with id: %lld !!!", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(tw, 2, FALSE);
    fprintf(tw, "Now the middle radio button is disabled, and should not be able\n");
    fprintf(tw, "to be pressed.\n");
    fprintf(tw, "Hit the radio button, or return to continue\n");
    fprintf(tw, "\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etradbut) {

            if (er.radbid == 1) {

                fprintf(tw, "You selected the top checkbox\n");
                chk = !chk;
                ami_selectwidget(tw, 1, chk);

            } else if (er.radbid == 2) {

                fprintf(tw, "You selected the middle checkbox\n");
                chk2 = !chk2;
                ami_selectwidget(tw, 2, chk2);

            } else if (er.radbid == 3) {

                fprintf(tw, "You selected the bottom checkbox\n");
                chk3 = !chk3;
                ami_selectwidget(tw, 3, chk3);

            } else fprintf(tw, "!!! No button with id: %lld !!!\n", AMI_LONG_CAST(er.butid));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);

    /* *********************** Character Group box test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character group box test\n");
    fprintf(tw, "\n");
    ami_groupsiz(tw, "Hello there", 0, 0, &x, &y, &ox, &oy);
    ami_group(tw, 10, 10, 10+x, 10+y, "Hello there", 1);
    fprintf(tw, "This is a group box with a null client area\n");
    fprintf(tw, "Hit return to continue\n");
    waitnext();
    ami_killwidget(tw, 1);
    ami_groupsiz(tw, "Hello there", 20, 10, &x, &y, &ox, &oy);
    ami_group(tw, 10, 10, 10+x, 10+y, "Hello there", 1);
    fprintf(tw, "This is a group box with a 20,10 client area\n");
    fprintf(tw, "Hit return to continue\n");
    waitnext();
    ami_killwidget(tw, 1);
    ami_groupsiz(tw, "Hello there", 20, 10, &x, &y, &ox, &oy);
    ami_group(tw, 10, 10, 10+x, 10+y, "Hello there", 1);
    ami_button(tw, 10+ox, 10+oy, 10+ox+20-1, 10+oy+10-1, "Bark, bark!", 2);
    fprintf(tw, "This is a group box with a 20,10 layered button\n");
    fprintf(tw, "Hit return to continue\n");
    waitnext();
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);

    /* *********************** Character background test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character background test\n");
    fprintf(tw, "\n");
    ami_background(tw, 10, 10, 40, 20, 1);
    fprintf(tw, "Hit return to continue\n");
    waitnext();
    ami_button(tw, 11, 11, 39, 19, "Bark, bark!", 2);
    fprintf(tw, "This is a background with a layered button\n");
    fprintf(tw, "Hit return to continue\n");
    waitnext();
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);

    /* *********************** Character scroll bar test *********************** */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character scroll bar test\n");
    fprintf(tw, "\n");
    ami_scrollvertsiz(tw, &x, &y);
    ami_scrollvert(tw, 10, 10, 10+x-1, 20, 1);
    ami_scrollhorizsiz(tw, &x, &y);
    ami_scrollhoriz(tw, 15, 10, 35, 10+y-1, 2);
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            fprintf(tw, "Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            fprintf(tw, "Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            fprintf(tw, "Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            fprintf(tw, "Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(tw, er.sclpid, er.sclpos); /* set new position for scrollbar */
            fprintf(tw, "Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);

    /* ******************* Character scroll bar sizing test ******************** */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character scroll bar sizing test\n");
    fprintf(tw, "\n");
    ami_scrollvert(tw, 10, 10, 12, 20, 1);
    ami_scrollsiz(tw, 1, (LONG_MAX / 4)*3);
    ami_scrollvert(tw, 10+5, 10, 12+5, 20, 2);
    ami_scrollsiz(tw, 2, LONG_MAX / 2);
    ami_scrollvert(tw, 10+10, 10, 12+10, 20, 3);
    ami_scrollsiz(tw, 3, LONG_MAX / 4);
    ami_scrollvert(tw, 10+15, 10, 12+15, 20, 4);
    ami_scrollsiz(tw, 4, LONG_MAX / 8);
    fprintf(tw, "Now should be four scrollbars, decending in size to the right.\n");
    fprintf(tw, "All of the scrollbars can be manipulated.\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            fprintf(tw, "Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            fprintf(tw, "Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            fprintf(tw, "Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            fprintf(tw, "Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(tw, er.sclpid, er.sclpos); /* set new position for scrollbar */
            fprintf(tw, "Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);
    ami_killwidget(tw, 4);

    /* ****************** Character scroll bar minimums test ******************* */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character scroll bar minimums test\n");
    fprintf(tw, "\n");
    ami_scrollvertsiz(tw, &x, &y);
    ami_scrollvert(tw, 10, 10, 10+x-1, 10+y-1, 1);
    ami_scrollhorizsiz(tw, &x, &y);
    ami_scrollhoriz(tw, 15, 10, 15+x-1, 10+y-1, 2);
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            fprintf(tw, "Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            fprintf(tw, "Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            fprintf(tw, "Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            fprintf(tw, "Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(tw, er.sclpid, er.sclpos); /* set new position for scrollbar */
            fprintf(tw, "Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);

    /* ************ Character scroll bar fat and skinny bars test ************** */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character scroll bar fat and skinny bars test\n");
    fprintf(tw, "\n");
    ami_scrollvertsiz(tw, &x, &y);
    ami_scrollvert(tw, 10, 10, 10, 10+10, 1);
    ami_scrollvert(tw, 12, 10, 20, 10+10, 3);
    ami_scrollhorizsiz(tw, &x, &y);
    ami_scrollhoriz(tw, 30, 10, 30+20, 10, 2);
    ami_scrollhoriz(tw, 30, 12, 30+20, 20, 4);
    do {

        nextevt(&er);
        if (er.etype == ami_etsclull)
            fprintf(tw, "Scrollbar: %lld up/left line\n", AMI_LONG_CAST(er.sclulid));
        if (er.etype == ami_etscldrl)
            fprintf(tw, "Scrollbar: %lld down/right line\n", AMI_LONG_CAST(er.scldrid));
        if (er.etype == ami_etsclulp)
            fprintf(tw, "Scrollbar: %lld up/left page\n", AMI_LONG_CAST(er.sclupid));
        if (er.etype == ami_etscldrp)
            fprintf(tw, "Scrollbar: %lld down/right page\n", AMI_LONG_CAST(er.scldpid));
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(tw, er.sclpid, er.sclpos); /* set new position for scrollbar */
            fprintf(tw, "Scrollbar: %lld position set: %lld\n", AMI_LONG_CAST(er.sclpid), AMI_LONG_CAST(er.sclpos));

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);
    ami_killwidget(tw, 4);

    /* ******************** Character number select box test ******************* */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character number select box test\n");
    fprintf(tw, "\n");
    ami_numselboxsiz(tw, 1, 10, &x, &y);
    ami_numselbox(tw, 10, 10, 10+x-1, 10+y-1, 1, 10, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etnumbox) fprintf(tw, "You selected: %lld\n", AMI_LONG_CAST(er.numbsl));
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);

    /* ************************* Character edit box test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character edit box test\n");
    fprintf(tw, "\n");
    ami_editboxsiz(tw, "Hi there, george", &x, &y);
    ami_editbox(tw, 10, 10, 10+x-1, 10+y-1, 1);
    ami_putwidgettext(tw, 1, "Hi there, george");
    do {

        nextevt(&er);
        if (er.etype == ami_etedtbox) {

            ami_getwidgettext(tw, 1, s, 100);
            fprintf(tw, "You entered: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);

    /* *********************** Character progress bar test ********************* */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character progress bar test\n");
    fprintf(tw, "\n");
    ami_progbarsiz(tw, &x, &y);
    ami_progbar(tw, 10, 10, 10+x-1, 10+y-1, 1);
    ami_timer(tw, 1, SECOND, TRUE);
    prog = 1;
    do {

        nextevt(&er);
        if (er.etype == ami_ettim) {

            if (prog < 20) {

                ami_progbarpos(tw, 1, LONG_MAX-((20-prog)*(LONG_MAX / 20)));
                prog = prog+1; /* next progress value */

            } else if (prog == 20) {

                ami_progbarpos(tw, 1, LONG_MAX);
                fprintf(tw, "Done !\n");
                prog = 11;
                ami_killtimer(tw, 1);

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);

    /* ************************* Character list box test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character list box test\n");
    fprintf(tw, "\n");
    fprintf(tw, "Note that it is normal for this box to not fill to exact\n");
    fprintf(tw, "character cells.\n");
    fprintf(tw, "\n");
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
    ami_listboxsiz(tw, lp, &x, &y);
    ami_listbox(tw, 10, 10, 10+x-1, 10+y-1, lp, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etlstbox) {

            switch (er.lstbsl) {

                case 1: fprintf(tw, "You selected pa_green\n"); break;
                case 2: fprintf(tw, "You selected pa_red\n"); break;
                case 3: fprintf(tw, "You selected pa_blue\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n");

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);

    /* ************************* Character dropdown box test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character dropdown box test\n");
    fprintf(tw, "\n");
    fprintf(tw, "Note that it is normal for this box to not fill to exact\n");
    fprintf(tw, "character cells.\n");
    fprintf(tw, "\n");
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
    ami_dropboxsiz(tw, lp, &cx, &cy, &ox, &oy);
    ami_dropbox(tw, 10, 10, 10+ox-1, 10+oy-1, lp, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etdrpbox) {

            switch (er.drpbsl) {

                case 1: fprintf(tw, "You selected Bird\n"); break;
                case 2: fprintf(tw, "You selected Cat\n"); break;
                case 3: fprintf(tw, "You selected Dog\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n");

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);

    /* ******************* Character dropdown edit box test ******************** */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character dropdown edit box test\n");
    fprintf(tw, "\n");
    fprintf(tw, "Note that it is normal for this box to not fill to exact\n");
    fprintf(tw, "character cells.\n");
    fprintf(tw, "\n");
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
    ami_dropeditboxsiz(tw, lp, &cx, &cy, &ox, &oy);
    ami_dropeditbox(tw, 10, 10, 10+ox-1, 10+oy-1, lp, 1);
    do {

        nextevt(&er);
        if (er.etype == ami_etdrebox) {

            ami_getwidgettext(tw, 1, s, 100);
            fprintf(tw, "You selected: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);

    /* ************************* Character slider test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character slider test\n");
    ami_slidehorizsiz(tw, &x, &y);
    x = 20;
    ami_slidehoriz(tw, 10, 10, 10+x-1, 10+y-1, 10, 1);
    ami_slidehoriz(tw, 10, 20, 10+x-1, 20+y-1, 0, 2);
    ami_slidevertsiz(tw, &x, &y);
    y = 10;
    ami_slidevert(tw, 40, 10, 40+x-1, 10+y-1, 10, 3);
    ami_slidevert(tw, 50, 10, 50+x-1, 10+y-1, 0, 4);
    fprintf(tw, "Bottom and right sliders should not have tick marks\n");
    do {

        nextevt(&er);
        if (er.etype == ami_etsldpos)
            fprintf(tw, "Slider id: %lld position: %lld\n", AMI_LONG_CAST(er.sldpid), AMI_LONG_CAST(er.sldpos));
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);
    ami_killwidget(tw, 4);

    /* ************************* Character tab bar test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character tab bar test\n");
    fprintf(tw, "\n");

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
    ami_tabbarsiz(tw, lp, ami_totop, 20, 2, &x, &y, &ox, &oy);
    ami_tabbar(tw, 15, 3, 15+x-1, 3+y-1, lp, ami_totop, 1);

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
    ami_tabbarsiz(tw, lp, ami_toright, 4, 12, &x, &y, &ox, &oy);
    ami_tabbar(tw, 37, 7, 37+x-1, 7+y-1, lp, ami_toright, 2);

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
    ami_tabbarsiz(tw, lp, ami_tobottom, 20, 2, &x, &y, &ox, &oy);
    ami_tabbar(tw, 15, 19, 15+x-1, 19+y-1, lp, ami_tobottom, 3);

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
    ami_tabbarsiz(tw, lp, ami_toleft, 4, 12, &x, &y, &ox, &oy);
    ami_tabbar(tw, 5, 7, 5+x-1, 7+y-1, lp, ami_toleft, 4);

    do {

        nextevt(&er);
        if (er.etype == ami_ettabbar) {

            if (er.tabid == 1) switch (er.tabsel) {

                case 1: fprintf(tw, "Top bar: You selected Left\n"); break;
                case 2: fprintf(tw, "Top bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Top bar: You selected Right\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 2) switch (er.tabsel) {

                case 1: fprintf(tw, "Right bar: You selected Top\n"); break;
                case 2: fprintf(tw, "Right bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Right bar: You selected Bottom\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 3) switch (er.tabsel) {

                case 1: fprintf(tw, "Bottom bar: You selected Left\n"); break;
                case 2: fprintf(tw, "Bottom bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Bottom bar: You selected right\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 4) switch (er.tabsel) {

                case 1: fprintf(tw, "Left bar: You selected Top\n"); break;
                case 2: fprintf(tw, "Left bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Left bar: You selected Bottom\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else fprintf(tw, "!!! Bad tab id !!!\n");

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);
    ami_killwidget(tw, 4);

    /* ************************* Character overlaid tab bar test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Character overlaid tab bar test\n");
    fprintf(tw, "\n");

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
    ami_tabbarsiz(tw, lp, ami_totop, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(tw, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_totop, 1);

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
    ami_tabbarsiz(tw, lp, ami_toright, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(tw, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_toright, 2);

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
    ami_tabbarsiz(tw, lp, ami_tobottom, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(tw, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_tobottom, 3);

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
    ami_tabbarsiz(tw, lp, ami_toleft, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(tw, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_toleft, 4);

    do {

        nextevt(&er);
        if (er.etype == ami_ettabbar) {

            if (er.tabid == 1) switch (er.tabsel) {

                case 1: fprintf(tw, "Top bar: You selected Left\n"); break;
                case 2: fprintf(tw, "Top bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Top bar: You selected Right\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 2) switch (er.tabsel) {

                case 1: fprintf(tw, "Right bar: You selected Top\n"); break;
                case 2: fprintf(tw, "Right bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Right bar: You selected Bottom\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 3) switch (er.tabsel) {

                case 1: fprintf(tw, "Bottom bar: You selected Left\n"); break;
                case 2: fprintf(tw, "Bottom bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Bottom bar: You selected right\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else if (er.tabid == 4) switch (er.tabsel) {

                case 1: fprintf(tw, "Left bar: You selected Top\n"); break;
                case 2: fprintf(tw, "Left bar: You selected Center\n"); break;
                case 3: fprintf(tw, "Left bar: You selected Bottom\n"); break;
                default: fprintf(tw, "!!! Bad select number !!!\n"); break;

            } else fprintf(tw, "!!! Bad tab id !!!\n");

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(tw, 1);
    ami_killwidget(tw, 2);
    ami_killwidget(tw, 3);
    ami_killwidget(tw, 4);

    /* ************************* Alert test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Alert test\n");
    fprintf(tw, "\n");
    fprintf(tw, "There should be an pa_alert dialog\n");
    fprintf(tw, "Both the dialog and this window should be fully reactive\n");
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_alert("This is an important message", "There has been an event !\n");
    fprintf(tw, "\n");
    fprintf(tw, "Alert dialog should have completed now\n");
    waitnext();

    /* ************************* Color query test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Color query test\n");
    fprintf(tw, "\n");
    fprintf(tw, "There should be an color query dialog\n");
    fprintf(tw, "Both the dialog and this window should be fully reactive\n");
    fprintf(tw, "The color pa_white should be the default selection\n");
    r = LONG_MAX;
    g = LONG_MAX;
    b = LONG_MAX;
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_querycolor(&r, &g, &b);
    fprintf(tw, "\n");
    fprintf(tw, "Dialog should have completed now\n");
    fprintf(tw, "Colors are: red: %lld green: %lld blue: %lld\n", AMI_LONG_CAST(r), AMI_LONG_CAST(g), AMI_LONG_CAST(b));
    waitnext();

    /* ************************* Open file query test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Open file query test\n");
    fprintf(tw, "\n");
    fprintf(tw, "There should be an open file query dialog\n");
    fprintf(tw, "Both the dialog and this window should be fully reactive\n");
    fprintf(tw, "The dialog should have \"myfile.txt\" as the default filename\n");
    strcpy(s, "myfile.txt");
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_queryopen(s, 100);
    fprintf(tw, "\n");
    fprintf(tw, "Dialog should have completed now\n");
    fprintf(tw, "Filename is: %s\n", s);
    waitnext();

    /* ************************* Save file query test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Save file query test\n");
    fprintf(tw, "\n");
    fprintf(tw, "There should be an save file query dialog\n");
    fprintf(tw, "Both the dialog and this window should be fully reactive\n");
    fprintf(tw, "The dialog should have \"myfile.txt\" as the default filename\n");
    strcpy(s, "myfile.txt");
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_querysave(s, 100);
    fprintf(tw, "\n");
    fprintf(tw, "Dialog should have completed now\n");
    fprintf(tw, "Filename is: %s\n", s);
    waitnext();

    /* ************************* Find query test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Find query test\n");
    fprintf(tw, "\n");
    fprintf(tw, "There should be a find query dialog\n");
    fprintf(tw, "Both the dialog and this window should be fully reactive\n");
    fprintf(tw, "The dialog should have \"mystuff\" as the default search string\n");
    strcpy(s, "mystuff");
    optf = 0;
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_queryfind(s, 100, &optf);
    fprintf(tw, "\n");
    fprintf(tw, "Dialog should have completed now\n");
    fprintf(tw, "Search string is: \"%s\"\n", s);
    if (BIT(ami_qfncase)&optf) fprintf(tw, "Case sensitive is on\n");
    else fprintf(tw, "Case sensitive is off\n");
    if (BIT(ami_qfnup)&optf) fprintf(tw, "Search up\n");
    else fprintf(tw, "Search down\n");
    if (BIT(ami_qfnre)&optf) fprintf(tw, "Use regular expression\n");
    else fprintf(tw, "Use literal expression\n");
    waitnext();

    /* ************************* Find/replace query test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Find/replace query test\n");
    fprintf(tw, "\n");
    fprintf(tw, "There should be a find/replace query dialog\n");
    fprintf(tw, "Both the dialog and this window should be fully reactive\n");
    fprintf(tw, "The dialog should have \"bark\" as the default search string\n");
    fprintf(tw, "and should have \"sniff\" as the default replacement string\n");
    strcpy(ss, "bark");
    strcpy(rs, "sniff");
    optfr = 0;
    if (!autorun && framenum >= tstlo) /* modal: it waits for a person to answer */
    ami_queryfindrep (ss, 100, rs, 100, &optfr);
    fprintf(tw, "\n");
    fprintf(tw, "Dialog should have completed now\n");
    fprintf(tw, "Search string is: \"%s\"\n", ss);
    fprintf(tw, "Replace string is: \"%s\"\n", rs);
    if (BIT(ami_qfrcase)&optfr) fprintf(tw, "Case sensitive is on\n");
    else fprintf(tw, "Case sensitive is off\n");
    if (BIT(ami_qfrup)&optfr) fprintf(tw, "Search/replace up\n");
    else fprintf(tw, "Search/replace down\n");
    if (BIT(ami_qfrre)&optfr) fprintf(tw, "Regular expressions are on\n");
    else fprintf(tw, "Regular expressions are off\n");
    if (BIT(ami_qfrfind)&optfr) fprintf(tw, "Mode is find\n");
    else fprintf(tw, "Mode is find/replace\n");
    if (BIT(ami_qfrallfil)&optfr) fprintf(tw, "Mode is find/replace all in file\n");
    else fprintf(tw, "Mode is find/replace first in file\n");
    if (BIT(ami_qfralllin)&optfr) fprintf(tw, "Mode is find/replace all on line(s)\n");
    else fprintf(tw, "Mode is find/replace first on line(s)\n");
    waitnext();

    /* ************************* Font query test ************************ */

    setframe();

    fprintf(tw, "\f");
    fprintf(tw, "Font query test\n");
    fprintf(tw, "\n");
    fprintf(tw, "There should be a font query dialog\n");
    fprintf(tw, "Both the dialog and this window should be fully reactive\n");
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
    ami_queryfont(tw, &fc, &fs, &fr, &fg, &fb, &br, &bg, &bb, &fe);
    fprintf(tw, "\n");
    fprintf(tw, "Dialog should have completed now\n");
    fprintf(tw, "Font code: %lld\n", AMI_LONG_CAST(fc));
    fprintf(tw, "Font size: %lld\n", AMI_LONG_CAST(fs));
    fprintf(tw, "Foreground color: Red: %lld Green: %lld Blue: %lld\n", AMI_LONG_CAST(fr), AMI_LONG_CAST(fg), AMI_LONG_CAST(fb));
    fprintf(tw, "Background color: Red: %lld Green: %lld Blue: %lld\n", AMI_LONG_CAST(br), AMI_LONG_CAST(bg), AMI_LONG_CAST(bb));
    if (BIT(ami_qfteblink)&fe) fprintf(tw, "Blink\n");
    if (BIT(ami_qftereverse)&fe) fprintf(tw, "Reverse\n");
    if (BIT(ami_qfteunderline)&fe) fprintf(tw, "Underline\n");
    if (BIT(ami_qftesuperscript)&fe) fprintf(tw, "Superscript\n");
    if (BIT(ami_qftesubscript)&fe) fprintf(tw, "Subscript\n");
    if (BIT(ami_qfteitalic)&fe) fprintf(tw, "Italic\n");
    if (BIT(ami_qftebold)&fe) fprintf(tw, "Bold\n");
    if (BIT(ami_qftestrikeout)&fe) fprintf(tw, "Strikeout\n");
    if (BIT(ami_qftestandout)&fe) fprintf(tw, "Standout\n");
    if (BIT(ami_qftecondensed)&fe) fprintf(tw, "Condensed\n");
    if (BIT(ami_qfteextended)&fe) fprintf(tw, "Extended\n");
    if (BIT(ami_qftexlight)&fe) fprintf(tw, "Xlight\n");
    if (BIT(ami_qftelight)&fe) fprintf(tw, "Light\n");
    if (BIT(ami_qftexbold)&fe) fprintf(tw, "Xbold\n");
    if (BIT(ami_qftehollow)&fe) fprintf(tw, "Hollow\n");
    if (BIT(ami_qfteraised)&fe) fprintf(tw, "Raised\n");
    waitnext();

    terminate:;

    fputc('\f', tw);
    fprintf(tw, "Test complete\n");

}
