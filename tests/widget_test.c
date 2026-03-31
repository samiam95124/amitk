/*******************************************************************************
*                                                                              *
*                          WIDGET TEST PROGRAM                                 *
*                                                                              *
*                    Copyright (C) 2005 Scott A. Franco                        *
*                                                                              *
* Tests the widgets and dialogs available.                                     *
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
#include <graphics.h>

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
static int           x, y, lm, xs, ys, bx, by, ix,iy;
static int           r, g, b;
static ami_qfnopts    optf;
static ami_qfropts    optfr;
static int           fc;
static int           fs;
static int           fr, fg, fb;
static int           br, bg, bb;
static ami_qfteffects fe;
static int           cx, cy;
static int           ox, oy;
static int           cox, coy;
static int           csx, csy;

static int           i, cnt;
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

/* wait return to be pressed, or handle terminate */
static void waitnext(void)

{

    ami_evtrec er; /* event record */

    do { ami_event(stdin, &er); }
    while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

}

/* draw a character grid */
static void chrgrid(void)

{

    int x, y;

    ami_fcolor(stdout, ami_yellow);
    y = 1;
    while (y < ami_maxyg(stdout)) {

        ami_line(stdout, 1, y, ami_maxxg(stdout), y);
        y = y+ami_chrsizy(stdout);

    }
    x = 1;
    while (x < ami_maxxg(stdout)) {

        ami_line(stdout, x, 1, x, ami_maxyg(stdout));
        x = x+ami_chrsizx(stdout);

    }
    ami_fcolor(stdout, ami_black);

}

int main(void)

{

    if (setjmp(terminate_buf)) goto terminate;

    ami_curvis(stdout, FALSE);
    printf("Widget test vs. 0.1\n");
    printf("\n");
    printf("Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Background test ************************* */

    ami_bcolor(stdout, ami_backcolor);
    printf("\f");
    printf("Background pa_color test\n");
    printf("\n");
    printf("The background color should match widgets now.\n");
    waitnext();
    ami_bcolor(stdout, ami_white);

    /* ************************** Terminal Button test ************************* */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal buttons test\n");
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

        ami_event(stdin, &er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle button is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* ************************* Graphical Buttons test ************************* */

    printf("\f");
    printf("Graphical buttons test\n");
    printf("\n");
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    lm = ami_maxxg(stdout)/20; /* set left margin */
    i = ami_curyg(stdout); /* set y position buttons */
    ami_buttonsizg(stdout, "Hello, there", &x, &y);
    ami_buttong(stdout, lm, i, lm+x, i+y, "Hello, there", 1);
    i += y+y/2; /* set increment between buttons */
    ami_buttonsizg(stdout, "Bark!", &x, &y);
    ami_buttong(stdout, lm, i, lm+x, i+y, "Bark!", 2);
    i += y+y/2; /* set increment between buttons */
    ami_buttonsizg(stdout, "Sniff", &x, &y);
    ami_buttong(stdout, lm, i, lm+x, i+y, "Sniff", 3);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %d\n !!!", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle button is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %d !!!\n", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* ************************** Terminal Checkbox test ************************** */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal checkbox test\n");
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

        ami_event(stdin, &er);
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

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle checkbox is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        ami_event(stdin, &er);
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

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* ************************** Graphical Checkbox test ************************** */

    printf("\f");
    printf("Graphical checkbox test\n");
    printf("\n");
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    lm = ami_maxxg(stdout)/20; /* set left margin */
    i = ami_curyg(stdout); /* set y position buttons */
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    ami_checkboxsizg(stdout, "Pick me", &x, &y);
    ami_checkboxg(stdout, lm, i, lm+x, i+y, "Pick me", 1);
        i += y+y/2; /* set increment between checkboxes */
    ami_checkboxsizg(stdout, "Or me", &x, &y);
    ami_checkboxg(stdout, lm, i, lm+x, i+y, "Or me", 2);
        i += y+y/2; /* set increment between checkboxes */
    ami_checkboxsizg(stdout, "No, me", &x, &y);
    ami_checkboxg(stdout, lm, i, lm+x, i+y, "No, me", 3);

    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etchkbox) {

            if (er.ckbxid == 1) {

                printf("You selected the top checkbox\n");
                chk = ! chk;
                ami_selectwidget(stdout, 1, chk);

            } else if (er.ckbxid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = ! chk2;
                ami_selectwidget(stdout, 2, chk2);

            } else if (er.ckbxid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = ! chk3;
                ami_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle checkbox is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        ami_event(stdin, &er);
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

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* *********************** Terminal radio button test ********************* */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal radio button test\n");
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

        ami_event(stdin, &er);
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

            } else printf("!!! No button with id: %d !!!", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle radio button is disabled, and should not be able\n");
    printf("to be pressed.\n");
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        ami_event(stdin, &er);
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

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);

    /* *********************** Graphical radio button test ********************* */

    printf("\f");
    printf("Graphical radio button test\n");
    printf("\n");
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    lm = ami_maxxg(stdout)/20; /* set left margin */
    i = ami_curyg(stdout); /* set y position buttons */
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    ami_radiobuttonsizg(stdout, "Station 1", &x, &y);
    ami_radiobuttong(stdout, lm, i, lm+x, i+y, "Station 1", 1);
    i += y+y/2; /* set increment between buttons */
    ami_radiobuttonsizg(stdout, "Station 2", &x, &y);
    ami_radiobuttong(stdout, lm, i, lm+x, i+y, "Station 2", 2);
    i += y+y/2; /* set increment between buttons */
    ami_radiobuttonsizg(stdout, "Station 3", &x, &y);
    ami_radiobuttong(stdout, lm, i, lm+x, i+y, "Station 3", 3);

    do {

        ami_event(stdin, &er);
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

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_enablewidget(stdout, 2, FALSE);
    printf("Now the middle radio button is disabled, and should not be able\n");
    printf("to be pressed.\n");
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        ami_event(stdin, &er);
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

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);


    /* *********************** Terminal Group box test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal group box test\n");
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

    /* *********************** Graphical Group box test ************************ */

    printf("\f");
    printf("Graphical group box test\n");
    printf("\n");
    printf("This is a group box with a null client area\n");
    printf("Hit return to continue\n");
    printf("\n");
    xs = ami_maxxg(stdout)/10; /* set size of group client */
    ys = xs;
    lm = ami_maxxg(stdout)/20; /* set left margin */
    i = ami_curyg(stdout)+7*ami_chrsizy(stdout); /* set y position buttons */
    ami_groupsizg(stdout, "Hello there", 0, 0, &x, &y, &ox, &oy);
    ami_groupg(stdout, lm, i, lm+x, i+y, "Hello there", 1);
    waitnext();
    ami_killwidget(stdout, 1);
    printf("This is a group box with a %d,%d client area\n", xs, ys);
    printf("Hit return to continue\n");
    printf("\n");
    ami_groupsizg(stdout, "Hello there", xs, ys, &x, &y, &ox, &oy);
    ami_groupg(stdout, lm, i, lm+x, i+y, "Hello there", 1);
    waitnext();
    ami_killwidget(stdout, 1);
    printf("This is a group box with a %d,%d layered button\n", xs, ys);
    printf("Hit return to continue");
    printf("\n");
    ami_groupsizg(stdout, "Hello there", xs, ys, &x, &y, &ox, &oy);
    ami_groupg(stdout, lm, i, lm+x, i+y, "Hello there", 1);
    ami_buttong(stdout, lm+ox, i+oy, lm+ox+xs, i+oy+ys, "Bark, bark!", 2);
    waitnext();
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* *********************** Terminal background test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal background test\n");
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

    /* *********************** Graphical background test ************************ */

    printf("\f");
    printf("Graphical background test\n");
    printf("\n");
    printf("Hit return to continue\n");
    printf("\n");
    xs = ami_maxxg(stdout)/5; /* set size of group client */
    ys = xs;
    bx = xs/10;
    by = bx;
    lm = ami_maxxg(stdout)/20; /* set left margin */
    i = ami_curyg(stdout)+ami_chrsizy(stdout)*3; /* set y position buttons */
    ami_backgroundg(stdout, lm, i, lm+xs, i+ys, 1);
    waitnext();
    ami_buttong(stdout, lm+bx, i+by, lm+xs-bx, i+ys-by, "Bark, bark!", 2);
    printf("This is a background with a layered button\n");
    printf("Hit return to continue\n");
    waitnext();
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* *********************** Terminal scroll bar test *********************** */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal scroll bar test\n");
    printf("\n");
    xs = ami_maxxg(stdout)/5; /* set size of group client */
    ys = xs;
    bx = xs/10;
    by = bx;
    lm = ami_maxxg(stdout)/20; /* set left margin */
    ami_scrollvertsiz(stdout, &x, &y);
    ami_scrollvert(stdout, 10, 10, 10+x-1, 20, 1);
    ami_scrollhorizsiz(stdout, &x, &y);
    ami_scrollhoriz(stdout, 15, 10, 35, 10+y-1, 2);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* ******************* Terminal scroll bar sizing test ******************** */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal scroll bar sizing test\n");
    printf("\n");
    ami_scrollvert(stdout, 10, 10, 12, 20, 1);
    ami_scrollsiz(stdout, 1, (INT_MAX / 4)*3);
    ami_scrollvert(stdout, 10+5, 10, 12+5, 20, 2);
    ami_scrollsiz(stdout, 2, INT_MAX / 2);
    ami_scrollvert(stdout, 10+10, 10, 12+10, 20, 3);
    ami_scrollsiz(stdout, 3, INT_MAX / 4);
    ami_scrollvert(stdout, 10+15, 10, 12+15, 20, 4);
    ami_scrollsiz(stdout, 4, INT_MAX / 8);
    printf("Now should be four scrollbars, decending in size to the right.\n");
    printf("All of the scrollbars can be manipulated.\n");
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ****************** Terminal scroll bar minimums test ******************* */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal scroll bar minimums test\n");
    printf("\n");
    ami_scrollvertsiz(stdout, &x, &y);
    ami_scrollvert(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    ami_scrollhorizsiz(stdout, &x, &y);
    ami_scrollhoriz(stdout, 15, 10, 15+x-1, 10+y-1, 2);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* ************ Terminal scroll bar fat and skinny bars test ************** */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal scroll bar fat and skinny bars test\n");
    printf("\n");
    ami_scrollvertsiz(stdout, &x, &y);
    ami_scrollvert(stdout, 10, 10, 10, 10+10, 1);
    ami_scrollvert(stdout, 12, 10, 20, 10+10, 3);
    ami_scrollhorizsiz(stdout, &x, &y);
    ami_scrollhoriz(stdout, 30, 10, 30+20, 10, 2);
    ami_scrollhoriz(stdout, 30, 12, 30+20, 20, 4);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* *********************** Graphical scroll bar test *********************** */

    printf("\f");
    printf("Graphical scroll bar test\n");
    printf("\n");
    lm = ami_maxxg(stdout)/20; /* set left margin */
    iy = ami_curyg(stdout); /* set y increment */
    ys = ami_maxyg(stdout)/4;
    xs = ys;
    ami_scrollvertsizg(stdout, &x, &y);
    ami_scrollvertg(stdout, lm, iy, lm+x, iy+ys, 1);
    ami_scrollhorizsizg(stdout, &x, &y);
    ami_scrollhorizg(stdout, lm+x+ami_chrsizx(stdout), iy,
                            lm+x+ami_chrsizx(stdout)+xs, iy+y, 2);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/pa_left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* ******************* Graphical scroll bar sizing test ******************** */

    printf("\f");
    printf("Graphical scroll bar sizing test\n");
    printf("\n");
    printf("Now should be four scrollbars, decending in size to the right.\n");
    printf("All of the scrollbars can be manipulated.\n");
    printf("\n");
    lm = ami_maxxg(stdout)/20; /* set left margin */
    iy = ami_curyg(stdout); /* set y increment */
    ys = ami_maxyg(stdout)/4;
    xs = ami_maxxg(stdout)/30;
    ami_scrollvertsizg(stdout, &x, &y);
    ami_scrollvertg(stdout, lm, iy, lm+x, iy+ys, 1);
    ami_scrollsiz(stdout, 1, (INT_MAX / 4)*3);
    ami_scrollvertg(stdout, lm+xs, iy, lm+xs+x, iy+ys, 2);
    ami_scrollsiz(stdout, 2, INT_MAX / 2);
    ami_scrollvertg(stdout, lm+xs*2, iy, lm+xs*2+x, iy+ys, 3);
    ami_scrollsiz(stdout, 3, INT_MAX / 4);
    ami_scrollvertg(stdout, lm+xs*3, iy, lm+xs*3+x, iy+ys, 4);
    ami_scrollsiz(stdout, 4, INT_MAX / 8);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/pa_left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ****************** Graphical scroll bar minimums test ******************* */

    printf("\f");
    printf("Graphical scroll bar minimums test\n");
    printf("\n");
    lm = ami_maxxg(stdout)/20; /* set left margin */
    iy = ami_curyg(stdout); /* set y increment */
    xs = ami_maxxg(stdout)/30;
    ami_scrollvertsizg(stdout, &x, &y);
    ami_scrollvertg(stdout, lm, iy, lm+x, iy+y, 1);
    ami_scrollsiz(stdout, 1, (INT_MAX/2));
    ami_scrollhorizsizg(stdout, &x, &y);
    ami_scrollhorizg(stdout, lm+xs, iy, lm+xs+x, iy+y, 2);
    ami_scrollsiz(stdout, 2, (INT_MAX/2));
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);

    /* ************ Graphical scroll bar fat and skinny bars test ************** */

    printf("\f");
    printf("Graphical scroll bar fat and skinny bars test\n");
    printf("\n");
    lm = ami_maxxg(stdout)/20; /* set left margin */
    iy = ami_curyg(stdout); /* set y increment */
    ix = ami_maxxg(stdout)/30; /* set x increment */
    xs = ami_maxxg(stdout)/4;
    ys = xs;
    ami_scrollvertsizg(stdout, &x, &y);
    ami_scrollvertg(stdout, lm, iy, lm+x, iy+ys, 1);
    ami_scrollvertg(stdout, lm+ix, iy, lm+ix+ami_maxxg(stdout)/10, iy+ys, 3);
    lm = lm+ix+ami_maxxg(stdout)/10+ami_maxxg(stdout)/20;
    ami_scrollhorizsizg(stdout, &x, &y);
    ami_scrollhorizg(stdout, lm, iy, lm+xs, iy+y, 2);
    ami_scrollhorizg(stdout, lm, iy+ix, lm+xs, iy+y+ix+ami_maxxg(stdout)/10, 4);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == ami_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldrid);
        if (er.etype == ami_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == ami_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == ami_etsclpos) {

            ami_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ******************** Terminal number select box test ******************* */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal number select box test\n");
    printf("\n");
    ami_numselboxsiz(stdout, 1, 10, &x, &y);
    ami_numselbox(stdout, 10, 10, 10+x-1, 10+y-1, 1, 10, 1);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etnumbox) printf("You selected: %d\n", er.numbsl);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ******************** Graphical number select box test ******************* */

    printf("\f");
    printf("Graphical number select box test\n");
    printf("\n");
    ami_numselboxsizg(stdout, 1, 10, &x, &y);
    ami_numselboxg(stdout, 100, 100, 100+x, 100+y, 1, 10, 1);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etnumbox) printf("You selected: %d\n", er.numbsl);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Terminal edit box test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal edit box test\n");
    printf("\n");
    ami_editboxsiz(stdout, "Hi there, george", &x, &y);
    ami_editbox(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    ami_putwidgettext(stdout, 1, "Hi there, george");
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etedtbox) {

            ami_getwidgettext(stdout, 1, s, 100);
            printf("You entered: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Graphical edit box test ************************ */

    printf("\f");
    printf("Graphical edit box test\n");
    printf("\n");
    ami_editboxsizg(stdout, "Hi there, george", &x, &y);
    ami_editboxg(stdout, 100, 100, 100+x-1, 100+y-1, 1);
    ami_putwidgettext(stdout, 1, "Hi there, george");
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etedtbox) {

            ami_getwidgettext(stdout, 1, s, 100);
            printf("You entered: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* *********************** Terminal progress bar test ********************* */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal progress bar test\n");
    printf("\n");
    ami_progbarsiz(stdout, &x, &y);
    ami_progbar(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    ami_timer(stdout, 1, SECOND, TRUE);
    prog = 1;
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_ettim) {

            if (prog < 20) {

                ami_progbarpos(stdout, 1, INT_MAX-((20-prog)*(INT_MAX / 20)));
                prog = prog+1; /* next progress value */

            } else if (prog == 20) {

                ami_progbarpos(stdout, 1, INT_MAX);
                printf("Done !\n");
                prog = 11;
                ami_killtimer(stdout, 1);

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* *********************** Graphical progress bar test ********************* */

    printf("\f");
    printf("Graphical progress bar test\n");
    printf("\n");
    ami_progbarsizg(stdout, &x, &y);
    ami_progbarg(stdout, 100, 100, 100+x-1, 100+y-1, 1);
    ami_timer(stdout, 1, SECOND, TRUE);
    prog = 1;
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_ettim) {

            if (prog < 20) {

                ami_progbarpos(stdout, 1, INT_MAX-((20-prog)*(INT_MAX / 20)));
                prog = prog+1; /* next progress value */

            } else if (prog == 20) {

                ami_progbarpos(stdout, 1, INT_MAX);
                printf("Done !\n");
                prog = 11;
                ami_killtimer(stdout, 1);

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Terminal list box test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal list box test\n");
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

        ami_event(stdin, &er);
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

    /* ************************* Graphical list box test ************************ */

    printf("\f");
    printf("Graphical list box test\n");
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
    ami_listboxsizg(stdout, lp, &x, &y);
    ami_listboxg(stdout, 100, 100, 100+x-1, 100+y-1, lp, 1);
    do {

        ami_event(stdin, &er);
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

    /* ************************* Terminal dropdown box test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal dropdown box test\n");
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

        ami_event(stdin, &er);
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

    /* ************************* Graphical dropdown box test ************************ */

    printf("\f");
    printf("Graphical dropdown box test\n");
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
    ami_dropboxsizg(stdout, lp, &cx, &cy, &ox, &oy);
    ami_dropboxg(stdout, 100, 100, 100+ox-1, 100+oy-1, lp, 1);
    do {

        ami_event(stdin, &er);
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

    /* ******************* Terminal dropdown edit box test ******************** */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal dropdown edit box test\n");
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

        ami_event(stdin, &er);
        if (er.etype == ami_etdrebox) {

            ami_getwidgettext(stdout, 1, s, 100);
            printf("You selected: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ******************* Graphical dropdown edit box test ******************** */

    printf("\f");
    printf("Graphical dropdown edit box test\n");
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
    ami_dropeditboxsizg(stdout, lp, &cx, &cy, &ox, &oy);
    ami_dropeditboxg(stdout, 100, 100, 100+ox-1, 100+oy-1, lp, 1);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etdrebox) {

            ami_getwidgettext(stdout, 1, s, 100);
            printf("You selected: %s\n", s);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);

    /* ************************* Terminal slider test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal slider test\n");
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

        ami_event(stdin, &er);
        if (er.etype == ami_etsldpos)
            printf("Slider id: %d position: %d\n", er.sldpid, er.sldpos);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ************************* Graphical slider test ************************ */

    printf("\f");
    printf("Graphical slider test\n");
    printf("\n");
    printf("Bottom and right sliders should not have tick marks\n");
    printf("\n");
    ox = ami_maxyg(stdout)*0.125;
    oy = ami_curyg(stdout);
    ami_slidehorizsizg(stdout, &xs, &ys);
    xs = ami_maxxg(stdout)*0.25;
    ami_slidehorizg(stdout, ox, oy, ox+xs-1, oy+ys-1, 10, 1);
    oy += ami_maxyg(stdout)*0.25;
    ami_slidehorizg(stdout, ox, oy, ox+xs-1, oy+ys-1, 0, 2);
    x = xs; /* save slider size x */
    ami_slidevertsizg(stdout, &xs, &ys);

    ox += x+ox; /* offset past horizontals */
    oy = ami_curyg(stdout); /* reset to top */
    ys = ami_maxxg(stdout)*0.25;

    ami_slidevertg(stdout, ox, oy, ox+xs-1, oy+ys-1, 10, 3);
    ox += ami_maxxg(stdout)*0.125;
    ami_slidevertg(stdout, ox, oy, ox+xs-1, oy+ys-1, 0, 4);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etsldpos)
            printf("Slider id: %d position: %d\n", er.sldpid, er.sldpos);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_killwidget(stdout, 1);
    ami_killwidget(stdout, 2);
    ami_killwidget(stdout, 3);
    ami_killwidget(stdout, 4);

    /* ************************* Terminal tab bar test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal tab bar test\n");
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
    ami_tabbarsiz(stdout, ami_totop, 20, 2, &x, &y, &ox, &oy);
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
    ami_tabbarsiz(stdout, ami_toright, 4, 12, &x, &y, &ox, &oy);
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
    ami_tabbarsiz(stdout, ami_tobottom, 20, 2, &x, &y, &ox, &oy);
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
    ami_tabbarsiz(stdout, ami_toleft, 4, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 5, 7, 5+x-1, 7+y-1, lp, ami_toleft, 4);

    do {

        ami_event(stdin, &er);
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

   /* ************************* Graphical tab bar test ************************ */

    printf("\f");
    printf("Graphical tab bar test\n");
    printf("\n");

    ox = ami_maxyg(stdout)*0.3;
    oy = ami_curyg(stdout);
    csx = ami_maxyg(stdout)*0.5;
    csy = ami_maxyg(stdout)*0.1;

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
    ami_tabbarsizg(stdout, ami_totop, csx, csy, &xs, &ys, &cox, &coy);
    ami_tabbarg(stdout, ox, oy, ox+xs-1, oy+ys-1, lp, ami_totop, 1);
    y = oy+ys-1;

    ox = ami_maxxg(stdout)*0.5;
    oy = y;
    csx = ami_maxyg(stdout)*0.1;
    csy = ami_maxyg(stdout)*0.5;

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
    ami_tabbarsizg(stdout, ami_toright, csx, csy, &xs, &ys, &cox, &coy);
    ami_tabbarg(stdout, ox, oy, ox+xs-1, oy+ys-1, lp, ami_toright, 2);

    ox = ami_maxyg(stdout)*0.3;
    oy = oy+ys-1;
    csx = ami_maxyg(stdout)*0.5;
    csy = ami_maxyg(stdout)*0.1;

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
    ami_tabbarsizg(stdout, ami_tobottom, csx, csy, &xs, &ys, &cox, &coy);
    ami_tabbarg(stdout, ox, oy, ox+xs-1, oy+ys-1, lp, ami_tobottom, 3);

    ox = ami_maxxg(stdout)*0.05;
    oy = y;
    csx = ami_maxyg(stdout)*0.1;
    csy = ami_maxyg(stdout)*0.5;

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
    ami_tabbarsizg(stdout, ami_toleft, csx, csy, &xs, &ys, &cox, &coy);
    ami_tabbarg(stdout, ox, oy, ox+xs-1, oy+ys-1, lp, ami_toleft, 4);

    do {

        ami_event(stdin, &er);
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

    /* ************************* Terminal overlaid tab bar test ************************ */

    printf("\f");
    chrgrid();
    ami_binvis(stdout);
    printf("Terminal overlaid tab bar test\n");
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
    ami_tabbarsiz(stdout, ami_totop, 30, 12, &x, &y, &ox, &oy);
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
    ami_tabbarsiz(stdout, ami_toright, 30, 12, &x, &y, &ox, &oy);
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
    ami_tabbarsiz(stdout, ami_tobottom, 30, 12, &x, &y, &ox, &oy);
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
    ami_tabbarsiz(stdout, ami_toleft, 30, 12, &x, &y, &ox, &oy);
    ami_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, ami_toleft, 4);

    do {

        ami_event(stdin, &er);
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

    /* ************************* Graphical overlaid tab bar test ************************ */

    printf("\f");
    printf("Graphical overlaid tab bar test\n");
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
    ami_tabbarsizg(stdout, ami_totop, 200, 200, &x, &y, &ox, &oy);
    ami_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, ami_totop, 1);

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
    ami_tabbarsizg(stdout, ami_toright, 200, 200, &x, &y, &ox, &oy);
    ami_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, ami_toright, 2);

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
    ami_tabbarsizg(stdout, ami_tobottom, 200, 200, &x, &y, &ox, &oy);
    ami_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, ami_tobottom, 3);

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
    ami_tabbarsizg(stdout, ami_toleft, 200, 200, &x, &y, &ox, &oy);
    ami_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, ami_toleft, 4);

    do {

        ami_event(stdin, &er);
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

    printf("\f");
    printf("Alert test\n");
    printf("\n");
    printf("There should be an pa_alert dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    ami_alert("This is an important message", "There has been an event !\n");
    printf("\n");
    printf("Alert dialog should have completed now\n");
    waitnext();

    /* ************************* Color query test ************************ */

    printf("\f");
    printf("Color query test\n");
    printf("\n");
    printf("There should be an pa_color query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The pa_color pa_white should be the default selection\n");
    r = INT_MAX;
    g = INT_MAX;
    b = INT_MAX;
    ami_querycolor(&r, &g, &b);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Colors are: red: %d green: %d blue: %d\n", r, g, b);
    waitnext();

    /* ************************* Open file query test ************************ */

    printf("\f");
    printf("Open file query test\n");
    printf("\n");
    printf("There should be an open file query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have \"myfile.txt\" as the default filename\n");
    strcpy(s, "myfile.txt");
    ami_queryopen(s, 100);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Filename is: %s\n", s);
    waitnext();

    /* ************************* Save file query test ************************ */

    printf("\f");
    printf("Save file query test\n");
    printf("\n");
    printf("There should be an save file query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have \"myfile.txt\" as the default filename\n");
    strcpy(s, "myfile.txt");
    ami_querysave(s, 100);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Filename is: %s\n", s);
    waitnext();

    /* ************************* Find query test ************************ */

    printf("\f");
    printf("Find query test\n");
    printf("\n");
    printf("There should be a find query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have \"mystuff\" as the default search string\n");
    strcpy(s, "mystuff");
    optf = 0;
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

    printf("\f");
    printf("Font query test\n");
    printf("\n");
    printf("There should be a font query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    fc = PA_FONT_BOOK;
    fs = ami_chrsizy(stdout);
    fr = 0; /* set foreground to ami_black */
    fg = 0;
    fb = 0;
    br = INT_MAX; /* set ami_back(stdout)ground to ami_white */
    bg = INT_MAX;
    bb = INT_MAX;
    fe = 0;
    ami_queryfont(stdout, &fc, &fs, &fr, &fg, &fb, &br, &bg, &bb, &fe);
    strcpy(s, "");
    ami_fontnam(stdout, fc, s, 100);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Font code: %d(%s)\n", fc, s);
    printf("Font size: %d\n", fs);
    printf("Foreground pa_color: Red: %d Green: %d Blue: %d\n", fr, fg, fb);
    printf("Background pa_color: Red: %d Green: %d Blue: %d\n", br, bg, bb);
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

    printf("\f");
    printf("Test complete\n");

}
