/*******************************************************************************
*                                                                             *
*                     WINDOW MANAGEMENT TEST PROGRAM                          *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
*                                                                             *
* Tests text and graphical windows management calls.                          *
*                                                                             *
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

static jmp_buf terminate_buf;
static FILE*      win2;
static FILE*      win3;
static FILE*      win4;
static int        x, x2, y, y2;
static int        ox, oy;       /* original size of window */
static int        fb;           /* front/back flipper */
static pa_evtrec  er;
static pa_menuptr mp;           /* menu pointer */
static pa_menuptr ml;           /* menu list */
static pa_menuptr sm;           /* submenu list */
static int        sred;         /* state variables */
static int        sgreen;
static int        sblue;
static int        mincnt;       /* minimize counter */
static int        maxcnt;       /* maximize counter */
static int        nrmcnt;       /* normalize counter */
static int        i;
static int        xs, ys;
static int        cs;
static long       t, et;
static pa_color   c1, c2, c3;

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

/* wait return to be pressed, or handle terminate */

static void waitnext(void)

{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, &er); }
    while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* wait return to be pressed, or handle terminate, while printing characters */

static void waitnextprint(void)

{

    pa_evtrec er; /* event record */

    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etchar)
            printf("Window: %d char: %c\n", er.winid, er.echar);

    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* print centered string */

static void prtcen(int y, const char* s)

{

   pa_cursor(stdout, (pa_maxx(stdout)/2)-(strlen(s)/2), y);
   printf("%s", s);

}

/* print centered string graphical */

static void prtceng(int y, const char* s)

{

   pa_cursorg(stdout, (pa_maxxg(stdout)/2)-(pa_strsiz(stdout, s)/2), y);
   printf("%s", s);

}

/* wait time in 100 microseconds */

static void waittime(int t)

{

    pa_evtrec er;

    pa_timer(stdout, 1, t, FALSE);
    do { pa_event(stdin, &er);
    } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* append a new menu entry to the given list */

static void appendmenu(pa_menuptr* list, pa_menuptr m)

{

    pa_menuptr lp;

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

static void newmenu(pa_menuptr* mp, int onoff, int oneof, int bar,
             int id, const string face)

{

    *mp = malloc(sizeof(pa_menurec));
    if (!*mp) pa_alert("mantst", "Out of memory");
    (*mp)->onoff = onoff;
    (*mp)->oneof = oneof;
    (*mp)->bar = bar;
    (*mp)->id = id;
    (*mp)->face = malloc(strlen(face));
    if (!*mp) pa_alert("mantst", "Out of memory");
    strcpy((*mp)->face, face);

}

/* draw a character grid */

static void chrgrid(void)

{

    int x, y;

    pa_fcolor(stdout, pa_yellow);
    y = 1;
    while (y < pa_maxyg(stdout)) {

        pa_line(stdout, 1, y, pa_maxxg(stdout), y);
        y = y+pa_chrsizy(stdout);

    }
    x = 1;
    while (x < pa_maxxg(stdout)) {

        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+pa_chrsizx(stdout);

    }
    pa_fcolor(stdout, pa_black);

}

/* display frame test */

static void frameinside(const string s, int x, int y)

{

    putchar('\f');
    pa_fcolor(stdout, pa_cyan);
    pa_rect(stdout, 1, 1, x, y);
    pa_line(stdout, 1, 1, x, y);
    pa_line(stdout, 1, y, x, 1);
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    puts(s);
    pa_bover(stdout);

}

static void frametest(const string s)

{

    pa_evtrec er;
    int       x, y;

    x = pa_maxxg(stdout); /* set size */
    y = pa_maxyg(stdout);
    frameinside(s, x, y);
    do {

        pa_event(stdin, &er); /* get next event */
        if (er.etype == pa_etredraw) frameinside(s, x, y);
        if (er.etype == pa_etresize) {

            /* Save the new dimensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = pa_maxxg(stdout);
            y = pa_maxyg(stdout);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);

}

/* Finds the largest square that fits into the screen, then applies a ratio to
   that. Used to determine a relative size that fits the screen. */
static void sqrrat(int* xs, int* ys, float rat)

{

    /* ratio by screen smallest x-y, then square it up */
    pa_getsizg(stdout, xs, ys);
    if (*xs > *ys) { *ys /= rat; *xs = *ys; } /* square */
    else { *xs /= rat; *ys = *xs; }

}

static pa_color nextcolor(pa_color c)

{

    c++;
    if (c > pa_magenta) c = pa_red;

    return (c);

}

int main(void)

{

    int xr;

    if (setjmp(terminate_buf)) goto terminate;

    pa_auto(stdout, OFF);
    pa_curvis(stdout, OFF);
    printf("Managed screen test vs. 0.1\n");
    printf("\n");
    pa_scnsiz(stdout, &x, &y);
    printf("Screen size character: x: %d y: %d\n", x, y);
    pa_scnsizg(stdout, &x, &y);
    printf("Screen size pixel: x: %d y: %d\n", x, y);
    printf("\n");
    pa_getsiz(stdout, &x, &y);
    printf("Window size character: x: %d y: %d\n", x, y);
    pa_getsizg(stdout, &ox, &oy);
    printf("Window size graphical: x: %d y: %d\n", ox, oy);
    printf("\n");
    printf("Client size character: x: %d y: %d\n", pa_maxx(stdout), pa_maxy(stdout));
    printf("Client size graphical: x: %d y: %d\n", pa_maxxg(stdout), pa_maxyg(stdout));
    printf("\n");
    printf("Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Window titling test ************************** */

    pa_title(stdout, "This is a mangement test window");
    printf("The title bar of this window should read: This is a mangement test window\n");
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window title test");
    waitnext();

    /* ************************** Multiple windows ************************** */

    putchar('\f');
    pa_curvis(stdout, ON);
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Multiple window test");
    pa_home(stdout);
    pa_auto(stdout, ON);
    printf("This is the main window");
    printf("\n");
    printf("Select back and forth between each window, and make sure the\n");
    printf("cursor follows\n");
    printf("\n");
    printf("Here is the cursor->");
    pa_openwin(&stdin, &win2, NULL, 2);
    fprintf(win2, "This is the second window\n");
    fprintf(win2, "\n");
    fprintf(win2, "Here is the cursor->");
    waitnext();
    printf("\n");
    printf("Now enter characters to each window, then end with return\n");
    waitnextprint();
    fclose(win2);
    putchar('\f');
    printf("Second window now closed\n");
    waitnext();
    pa_curvis(stdout, OFF);
    pa_auto(stdout, OFF);

    /* ********************* Resize buffer window character ******************** */

    ox = pa_maxx(stdout);
    oy = pa_maxy(stdout);
    pa_bcolor(stdout, pa_cyan);
    pa_sizbuf(stdout, 50, 50);
    putchar('\f');
    for (x = 1; x <= pa_maxx(stdout); x++) printf("*");
    pa_cursor(stdout, 1, pa_maxy(stdout));
    for (x = 1; x <= pa_maxx(stdout); x++) printf("*");
    for (y = 1; y <= pa_maxy(stdout); y++) { pa_cursor(stdout, 1, y); printf("*"); }
    for (y = 1; y <= pa_maxy(stdout); y++) { pa_cursor(stdout, pa_maxx(stdout), y); printf("*"); }
    pa_home(stdout);
    printf("Buffer should now be 50 by 50 characters, and\n");
    printf("painted blue\n");
    printf("maxx: %d maxy: %d\n", pa_maxx(stdout), pa_maxy(stdout));
    printf("Open up window to verify this\n");
    prtcen(pa_maxy(stdout), "Buffer resize character test\n");
    pa_bcolor(stdout, pa_white);
    waitnext();
    pa_sizbuf(stdout, ox, oy);

    /* *********************** Resize buffer window pixel ********************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    sqrrat(&xs, &ys, 1.3); /* find square ratio */
    pa_bcolor(stdout, pa_cyan);
    pa_sizbufg(stdout, xs, ys);
    putchar('\f');
    pa_linewidth(stdout, 20);
    pa_line(stdout, 1, 1, pa_maxxg(stdout), 1);
    pa_line(stdout, 1, 1, 1, pa_maxyg(stdout));
    pa_line(stdout, 1, pa_maxyg(stdout), pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, pa_maxxg(stdout), 1, pa_maxxg(stdout), pa_maxyg(stdout));
    printf("Buffer should now be %d by %d pixels, and\n", xs, ys);
    printf("painted blue\n");
    printf("maxxg: %d maxyg: %d\n", pa_maxxg(stdout), pa_maxyg(stdout));
    printf("Open up window to verify this\n");
    prtcen(pa_maxy(stdout), "Buffer resize graphical test");
    pa_bcolor(stdout, pa_white);
    waitnext();
    pa_sizbufg(stdout, ox, oy);

    /* ****************** Resize screen with buffer on character *************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    for (x = 20; x <= 80; x++) {

        pa_setsiz(stdout, x, 25);
        pa_getsiz(stdout, &x2, &y2);
        if (x2 != x || y2 != 25) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, x, 25);
            waitnext();
            longjmp(terminate_buf, 1);

        };
        putchar('\f');
        printf("Resize screen buffered character\n");
        printf("\n");
        printf("Moving in x\n");
        waittime(1000);

    }
    printf("\n");
    printf("Complete");
    waitnext();
    for (y = 10; y <= 50; y++) {

        pa_setsiz(stdout, 80, y);
        pa_getsiz(stdout, &x2, &y2);
        if (x2 != 80 || y2 != y) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, 80, y);
            printf("*** Getsiz does not match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        putchar('\f');
        printf("Resize screen buffered character\n");
        printf("\n");
        printf("Moving in y\n");
        waittime(1000);

    }
    printf("\n");
    printf("Complete\n");
    waitnext();
    pa_winclientg(stdout, ox, oy, &ox, &oy, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    pa_setsizg(stdout, ox, oy);

    /* ******************** Resize screen with buffer on pixel ***************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    sqrrat(&xs, &ys, 1.5); /* find square ratio */
    for (x = xs; x <= xs*4; x += xs/64) {

        pa_setsizg(stdout, x, ys);
        pa_getsizg(stdout, &x2, &y2);
        if (x2 != x || y2 != ys) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, x, ys);
            printf("*** Getsiz does ! match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        putchar('\f');
        printf("Resize screen buffered graphical\n");
        printf("\n");
        printf("Moving in x\n");
        waittime(100);

    }
    printf("\n");
    printf("Complete\n");
    waitnext();
    for (y = ys; y <= ys*4; y += ys/64) {

        pa_setsizg(stdout, xs, y);
        pa_getsizg(stdout, &x2, &y2);
        if (x2 != xs || y2 != y) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, 300, y);
            printf("*** Getsiz does ! match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        putchar('\f');
        printf("Resize screen buffered graphical\n");
        printf("\n");
        printf("Moving in y\n");
        waittime(100);

    }
    printf("\n");
    printf("Complete\n");
    waitnext();
    pa_winclientg(stdout, ox, oy, &ox, &oy, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    pa_setsizg(stdout, ox, oy);

    /* ********************************* Front/back test *********************** */

    sqrrat(&xs, &ys, 8); /* find square ratio */
    cs = pa_chrsizy(stdout); /* save the character size */
    putchar('\f');
    pa_auto(stdout, OFF);
    printf("Position window for font/back test\n");
    printf("Then hit space to flip font/back status, or return to stop\n");
    fb = FALSE; /* clear front/back status */
    pa_font(stdout, PA_FONT_SIGN);
    pa_fontsiz(stdout, ys);

    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etchar) if (er.echar == ' ') { /* flip front/back */

            fb = !fb;
            if (fb) {

                pa_front(stdout);
                pa_fcolor(stdout, pa_white);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Back");
                pa_fcolor(stdout, pa_black);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Front");

            } else {

                pa_back(stdout);
                pa_fcolor(stdout, pa_white);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Front");
                pa_fcolor(stdout, pa_black);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Back");

            }

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_home(stdout);
    pa_fontsiz(stdout, cs);
    pa_font(stdout, PA_FONT_TERM);
    pa_auto(stdout, ON);

    /* ************************* Frame controls test buffered ****************** */

    putchar('\f');
    pa_fcolor(stdout, pa_cyan);
    pa_rect(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, pa_maxyg(stdout), pa_maxxg(stdout), 1);
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    printf("Ready for frame controls buffered\n");
    printf("(Note system may not implement all -- or any frame controls)\n");
    waitnext();
    pa_frame(stdout, OFF);
    printf("Entire frame off\n");
    waitnext();
    pa_frame(stdout, ON);
    printf("Entire frame on\n");
    waitnext();
    pa_sysbar(stdout, OFF);
    printf("System bar off\n");
    waitnext();
    pa_sysbar(stdout, ON);
    printf("System bar on\n");
    waitnext();
    pa_sizable(stdout, OFF);
    printf("Size bars off\n");
    waitnext();
    pa_sizable(stdout, ON);
    printf("Size bars on\n");
    waitnext();
    pa_bover(stdout);

    /* ************************* Frame controls test unbuffered ****************** */

    pa_buffer(stdout, OFF);
    frametest("Ready for frame controls unbuffered - Resize me!");
    printf("(Note system may not implement all -- or any frame controls)\n");
    pa_frame(stdout, OFF);
    frametest("Entire frame off");
    pa_frame(stdout, ON);
    frametest("Entire frame on");
    pa_sysbar(stdout, OFF);
    frametest("System bar off");
    pa_sysbar(stdout, ON);
    frametest("System bar on");
    pa_sizable(stdout, OFF);
    frametest("Size bars off");
    pa_sizable(stdout, ON);
    frametest("Size bars on");
    pa_buffer(stdout, ON);

    /* ********************************* Menu test ***************************** */

    pa_auto(stdout, ON);
    putchar('\f');
    pa_fcolor(stdout, pa_cyan);
    pa_rect(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, pa_maxyg(stdout), pa_maxxg(stdout), 1);
    pa_fcolor(stdout, pa_black);
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
    pa_menu(stdout, ml);
    pa_menuena(stdout, 3, OFF); /* disable "Walk" */
    pa_menusel(stdout, 5, ON); /* turn on "slow" */
    pa_menusel(stdout, 8, ON); /* turn on "red" */

    pa_home(stdout);
    printf("Use sample menu above\n");
    printf("'Walk' is disabled\n");
    printf("'Sublist' is a dropdown\n");
    printf("'slow', 'medium' and 'fast' are a one/of list\n");
    printf("'red', 'green' and 'blue' are on/off\n");
    printf("There should be a bar between slow-medium-fast groups and\n");
    printf("red-green-blue groups.\n");
    sred = ON; /* set states */
    sgreen = OFF;
    sblue = OFF;
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);
        if (er.etype == pa_etmenus) {

            printf("Menu select: ");
            switch (er.menuid) {

                case 1:  printf("Say hello\n"); break;
                case 2:  printf("Bark\n"); break;
                case 3:  printf("Walk\n"); break;
                case 4:  printf("Sublist\n"); break;
                case 5:  printf("slow\n"); pa_menusel(stdout, 5, ON); break;
                case 6:  printf("medium\n"); pa_menusel(stdout, 6, ON); break;
                case 7:  printf("fast\n"); pa_menusel(stdout, 7, ON); break;
                case 8:  printf("red\n"); sred = !sred;
                         pa_menusel(stdout, 8, sred); break;
                case 9:  printf("green\n"); sgreen = !sgreen;
                         pa_menusel(stdout, 9, sgreen); break;
                case 10: printf("blue\n"); sblue = !sblue;
                         pa_menusel(stdout, 10, sblue); break;

            }

        }

    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    pa_menu(stdout, NULL);

    /* ****************************** Standard menu test ******************** */

    putchar('\f');
    pa_auto(stdout, ON);
    ml = NULL; /* clear menu list */
    newmenu(&mp, FALSE, FALSE, OFF, PA_SMMAX+1, "one");
    appendmenu(&ml, mp);
    newmenu(&mp, TRUE, FALSE,  ON, PA_SMMAX+2, "two");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, PA_SMMAX+3, "three");
    appendmenu(&ml, mp);
    pa_stdmenu(BIT(PA_SMNEW) | BIT(PA_SMOPEN) | BIT(PA_SMCLOSE) |
               BIT(PA_SMSAVE) | BIT(PA_SMSAVEAS) | BIT(PA_SMPAGESET) |
               BIT(PA_SMPRINT) | BIT(PA_SMEXIT) | BIT(PA_SMUNDO) |
               BIT(PA_SMCUT) | BIT(PA_SMPASTE) | BIT(PA_SMDELETE) |
               BIT(PA_SMFIND) | BIT(PA_SMFINDNEXT) | BIT(PA_SMREPLACE) |
               BIT(PA_SMGOTO) | BIT(PA_SMSELECTALL) | BIT(PA_SMNEWWINDOW) |
               BIT(PA_SMTILEHORIZ) | BIT(PA_SMTILEVERT) | BIT(PA_SMCASCADE) |
               BIT(PA_SMCLOSEALL) | BIT(PA_SMHELPTOPIC) | BIT(PA_SMABOUT),
               &mp, ml);
    pa_menu(stdout, mp);
    printf("Standard menu appears above\n");
    printf("Check our 'one', 'two', 'three' buttons are in the program\n");
    printf("defined position\n");
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);
        if (er.etype == pa_etmenus) {

            printf("Menu select: ");
            switch (er.menuid) {

                case PA_SMNEW:       printf("new\n"); break;
                case PA_SMOPEN:      printf("open\n"); break;
                case PA_SMCLOSE:     printf("close\n"); break;
                case PA_SMSAVE:      printf("save\n"); break;
                case PA_SMSAVEAS:    printf("saveas\n"); break;
                case PA_SMPAGESET:   printf("pageset\n"); break;
                case PA_SMPRINT:     printf("print\n"); break;
                case PA_SMEXIT:      printf("exit\n"); break;
                case PA_SMUNDO:      printf("undo\n"); break;
                case PA_SMCUT:       printf("cut\n"); break;
                case PA_SMPASTE:     printf("paste\n"); break;
                case PA_SMDELETE:    printf("delete\n"); break;
                case PA_SMFIND:      printf("find\n"); break;
                case PA_SMFINDNEXT:  printf("findnext\n"); break;
                case PA_SMREPLACE:   printf("replace\n"); break;
                case PA_SMGOTO:      printf("goto\n"); break;
                case PA_SMSELECTALL: printf("selectall\n"); break;
                case PA_SMNEWWINDOW: printf("newwindow\n"); break;
                case PA_SMTILEHORIZ: printf("tilehoriz\n"); break;
                case PA_SMTILEVERT:  printf("tilevert\n"); break;
                case PA_SMCASCADE:   printf("cascade\n"); break;
                case PA_SMCLOSEALL:  printf("closeall\n"); break;
                case PA_SMHELPTOPIC: printf("helptopic\n"); break;
                case PA_SMABOUT:     printf("about\n"); break;
                case PA_SMMAX+1:     printf("one\n"); break;
                case PA_SMMAX+2:     printf("two\n"); break;
                case PA_SMMAX+3:     printf("three\n"); break;

            }

        }

    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    pa_menu(stdout, NULL);

    /* ************************* Child windows test character ****************** */

    putchar('\f');
    chrgrid();
    prtcen(pa_maxy(stdout), "Child windows test character");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_curvis(win2, OFF);
    pa_setpos(win2, 1, 10);
    pa_sizbuf(win2, 20, 10);
    pa_setsiz(win2, 20, 10);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_curvis(win3, OFF);
    pa_setpos(win3, 21, 10);
    pa_sizbuf(win3, 20, 10);
    pa_setsiz(win3, 20, 10);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_curvis(win4, OFF);
    pa_setpos(win4, 41, 10);
    pa_sizbuf(win4, 20, 10);
    pa_setsiz(win4, 20, 10);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    pa_home(stdout);
    printf("There should be 3 labeled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)      \n");
    waitnext();
    pa_frame(win2, OFF);
    pa_frame(win3, OFF);
    pa_frame(win4, OFF);
    pa_home(stdout);
    printf("There should be 3 labeled child windows below, without frames\n");
    printf("                                                            \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_home(stdout);
    printf("Child windows should all be closed                           \n");
    waitnext();

    /* *************************** Child windows test pixel ******************** */

    putchar('\f');
    sqrrat(&xs, &ys, 2.5); /* find square ratio */
    prtcen(pa_maxy(stdout), "Child windows test pixel");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_curvis(win2, OFF);
    pa_setposg(win2, xs*0+1, ys/2.5);
    pa_sizbufg(win2, xs, ys);
    pa_setsizg(win2, xs, ys);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_curvis(win3, OFF);
    pa_setposg(win3, xs*1+1, ys/2.5);
    pa_sizbufg(win3, xs, ys);
    pa_setsizg(win3, xs, ys);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_curvis(win4, OFF);
    pa_setposg(win4, xs*2+1, ys/2.5);
    pa_sizbufg(win4, xs, ys);
    pa_setsizg(win4, xs, ys);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    pa_home(stdout);
    printf("There should be 3 labled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)      \n");
    waitnext();
    pa_frame(win2, OFF);
    pa_frame(win3, OFF);
    pa_frame(win4, OFF);
    pa_home(stdout);
    printf("There should be 3 labled child windows below, without frames\n");
    printf("                                                            \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_home(stdout);
    printf("Child windows should all be closed                          \n");
    printf("                                                            \n");
    waitnext();

    /* *************** Child windows independent test character ************ */

    pa_curvis(stdout, ON);
    putchar('\f');
    chrgrid();
    prtcen(pa_maxy(stdout), "Child windows independent test character");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setpos(win2, 11, 10);
    pa_sizbuf(win2, 30, 10);
    pa_setsiz(win2, 30, 10);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setpos(win3, 41, 10);
    pa_sizbuf(win3, 30, 10);
    pa_setsiz(win3, 30, 10);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_home(stdout);
    printf("There should be 2 labeled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)       \n");
    printf("Test focus can be moved between windows, including the main  \n");
    printf("window. Test windows can be minimized and maximized          \n");
    printf("(if framed), test entering characters to windows.            \n");
    do {

        pa_event(stdin, &er); /* get next event */
        if (er.etype == pa_etchar) {

            if (er.winid == 2) fputc(er.echar, win2);
            else if (er.winid == 3) fputc(er.echar, win3);

        } else if (er.etype == pa_etenter) {

            /* translate the crs so we can test scrolling */
            if (er.winid == 2) fputc('\n', win2);
            else if (er.winid == 3) fputc('\n', win3);

        } else if (er.etype == pa_etterm && er.winid == 1)
            /* only take terminations from main window */
            longjmp(terminate_buf, 1);

    /* terminate on cr to the main window only */
    } while (er.etype != pa_etenter || er.winid != 1);
    fclose(win2);
    fclose(win3);
    pa_home(stdout);
    printf("Child windows should all be closed                           \n");
    printf("                                                             \n");
    printf("                                                             \n");
    printf("                                                             \n");
    printf("                                                             \n");
    pa_curvis(stdout, OFF);
    waitnext();

    /* ******************** Child windows independent test pixel ************** */

    putchar('\f');
    sqrrat(&xs, &ys, 2); /* find square ratio */
    prtcen(pa_maxy(stdout), "Child windows test pixel");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setposg(win2, xs*0+xs/5, ys/2);
    pa_sizbufg(win2, xs, ys);
    pa_setsizg(win2, xs, ys);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setposg(win3, xs*1+xs/5, ys/2);
    pa_sizbufg(win3, xs, ys);
    pa_setsizg(win3, xs, ys);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_home(stdout);
    printf("There should be 2 labeled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)      \n");
    printf("Test focus can be moved between windows, test windows can be \n");
    printf("minimized and maximized (if framed), test entering           \n");
    printf("characters to windows.                                       \n");
    do {

        pa_event(stdin, &er); /* get next event */
        if (er.etype == pa_etchar) {

            if (er.winid == 2) fputc(er.echar, win2);
            else if (er.winid == 3) fputc(er.echar, win3);

        } else if (er.etype == pa_etenter) {

            /* translate the crs so we can test scrolling */
            if (er.winid == 2) fputc('\n', win2);
            else if (er.winid == 3) fputc('\n', win3);

        } else if (er.etype == pa_etterm && er.winid == 1)
            /* only take terminations from main window */
            longjmp(terminate_buf, 1);

    /* terminate on cr to the main window only */
    } while (er.etype != pa_etenter || er.winid != 1);
    fclose(win2);
    fclose(win3);
    pa_home(stdout);
    printf("Child windows should all be closed                          \n");
    printf("                                                            \n");
    printf("                                                            \n");
    printf("                                                            \n");
    printf("                                                            \n");
    waitnext();

    /* ******************* Child windows stacking test pixel ******************* */

    putchar('\f');
    sqrrat(&xs, &ys, 2.5); /* find square ratio */
    prtcen(pa_maxy(stdout), "Child windows stacking test pixel");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_curvis(win2, OFF);
    pa_setposg(win2, xs/2*0+xs/5, ys/2.5+ys*0/4);
    pa_sizbufg(win2, xs, ys);
    pa_setsizg(win2, xs, ys);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_curvis(win3, OFF);
    pa_setposg(win3, xs/2*1+xs/5, ys/2.5+ys*1/4);
    pa_sizbufg(win3, xs, ys);
    pa_setsizg(win3, xs, ys);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_curvis(win4, OFF);
    pa_setposg(win4, xs/2*2+xs/5, ys/2.5+ys*2/4);
    pa_sizbufg(win4, xs, ys);
    pa_setsizg(win4, xs, ys);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    pa_home(stdout);
    printf("There should be 3 labled child windows below, overlapped,   \n");
    printf("with child 1 on the bottom, child 2 middle, and child 3 top.\n");
    waitnext();
    pa_back(win2);
    pa_back(win3);
    pa_back(win4);
    pa_home(stdout);
    printf("Now the windows are reordered, with child 1 on top, child 2 \n");
    printf("below that, and child 3 on the bottom.                      \n");
    waitnext();
    pa_front(win2);
    pa_front(win3);
    pa_front(win4);
    pa_home(stdout);
    printf("Now the windows are reordered, with child 3 on top, child 2 \n");
    printf("below that, and child 1 on the bottom.                      \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ************** Child windows stacking resize test pixel 1 *************** */

    sqrrat(&xs, &ys, 5); /* find square ratio */
    pa_buffer(stdout, OFF);
    pa_auto(stdout, OFF);
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setposg(win2, xs/2*1, ys/2*1);
    pa_sizbufg(win2, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
    pa_setsizg(win2, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setposg(win3, xs/2*2, ys/2*2);
    pa_sizbufg(win3, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
    pa_setsizg(win3, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_setposg(win4, xs/2*3, ys/2*3);
    pa_sizbufg(win4, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
    pa_setsizg(win4, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
    pa_curvis(win2, OFF);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_curvis(win3, OFF);
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_curvis(win4, OFF);
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etredraw || er.etype == pa_etresize) {

            putchar('\f');
            prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout),
                    "Child windows stacking resize test pixel 1");
            prtceng(1, "move and resize");
            pa_setsizg(win3, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
            pa_setsizg(win4, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);
            pa_setsizg(win2, pa_maxxg(stdout)-xs*2, pa_maxyg(stdout)-ys*2);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_buffer(stdout, ON);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ************** Child windows stacking resize test pixel 2 *************** */

    sqrrat(&xs, &ys, 20); /* find square ratio */
    pa_buffer(stdout, OFF);
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_auto(win2, OFF);
    pa_curvis(win2, OFF);
    pa_setposg(win2, xs*1, ys*1);
    pa_sizbufg(win2, pa_strsiz(win2, "I am child window 1"), pa_chrsizy(win2));
    pa_setsizg(win2, pa_maxxg(stdout)-xs*1*2, pa_maxyg(stdout)-ys*1*2);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_auto(win3, OFF);
    pa_curvis(win3, OFF);
    pa_setposg(win3, xs*2, ys*2);
    pa_sizbufg(win2, pa_strsiz(win3, "I am child window 2"), pa_chrsizy(win3));
    pa_setsizg(win3, pa_maxxg(stdout)-xs*2*2, pa_maxyg(stdout)-ys*2*2);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_auto(win4, OFF);
    pa_curvis(win4, OFF);
    pa_setposg(win4, xs*3, ys*3);
    pa_sizbufg(win2, pa_strsiz(win4, "I am child window 3"), pa_chrsizy(win4));
    pa_setsizg(win4, pa_maxxg(stdout)-xs*3*2, pa_maxyg(stdout)-ys*3*2);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3");
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etredraw  || er.etype == pa_etresize) {

            putchar('\f');
            prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout),
                    "Child windows stacking resize test pixel 2");
            prtceng(1, "move and resize");
            pa_setsizg(win2, pa_maxxg(stdout)-xs*1*2, pa_maxyg(stdout)-ys*1*2);
            pa_setsizg(win3, pa_maxxg(stdout)-xs*2*2, pa_maxyg(stdout)-ys*2*2);
            pa_setsizg(win4, pa_maxxg(stdout)-xs*3*2, pa_maxyg(stdout)-ys*3*2);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_buffer(stdout, ON);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ******************************* Buffer off test *********************** */

    putchar('\f');
    cs = pa_chrsizy(stdout); /* save the character size */
    pa_auto(stdout, OFF);
    pa_buffer(stdout, OFF);
    /* initialize prime size information */
    x = pa_maxxg(stdout);
    y = pa_maxyg(stdout);
    pa_linewidth(stdout, 5); /* set large lines */
    pa_font(stdout, PA_FONT_SIGN);
    pa_binvis(stdout);
    do {

        pa_event(stdin, &er); /* get next event */
        if (er.etype == pa_etredraw || er.etype == pa_etresize) {

            /* clear screen without overwriting frame */
            pa_fcolor(stdout, pa_white);
            pa_frect(stdout, 1+5, 1+5, x-5, y-5);
            pa_fcolor(stdout, pa_black);
            pa_fontsiz(stdout, y / 10);
            prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2,
                    "SIZE AND COVER ME !");
            pa_rect(stdout, 1+2, 1+2, x-2, y-2); /* frame the window */

        }
        if (er.etype == pa_etresize) {

            /* Save the new demensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = pa_maxxg(stdout);
            y = pa_maxyg(stdout);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_buffer(stdout, ON);
    pa_fontsiz(stdout, cs);
    pa_font(stdout, PA_FONT_TERM);
    pa_home(stdout);
    pa_auto(stdout, ON);

    /* ****************************** min/max/norm test ********************* */

    putchar('\f');
    pa_auto(stdout, OFF);
    pa_buffer(stdout, OFF);
    pa_font(stdout, PA_FONT_TERM);
    mincnt = 0; /* clear minimize counter */
    maxcnt = 0; /* clear maximize counter */
    nrmcnt = 0; /* clear normalize counter */
    do {

        pa_event(stdin, &er); /* get next event */
        /* count minimize, maximize, normalize */
        if (er.etype == pa_etmax) maxcnt = maxcnt+1;
        if (er.etype == pa_etmin) mincnt = mincnt+1;
        if (er.etype == pa_etnorm) nrmcnt = nrmcnt+1;
        if (er.etype == pa_etredraw || er.etype == pa_etmax ||
            er.etype == pa_etmin || er.etype == pa_etnorm) {

            putchar('\f');
            printf("Minimize, maximize and restore this window\n");
            printf("\n");
            printf("Minimize count:  %d\n", mincnt);
            printf("Maximize count:  %d\n", maxcnt);
            printf("Normalize count: %d\n", nrmcnt);

        }

        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_buffer(stdout, ON);

    /* ******************** Window size calculate character ***************** */

    putchar('\f');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window size calculate character");
    pa_home(stdout);
    pa_openwin(&stdin, &win2, NULL, 2);
    pa_linewidth(stdout, 1);

    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (20, 10) client, full frame, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
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
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("System bar off\n");
    pa_sysbar(win2, OFF);
    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize));
    printf("For (20, 10) client, no system bar, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
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
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("Sizing bars off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, OFF);
    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsysbar));
    printf("For (20, 10) client, no size bars, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
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
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("frame off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, ON);
    pa_frame(win2, OFF);
    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (20, 10) client, no frame, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
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
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    fclose(win2);

    /* ************************ Window size calculate pixel ******************** */

    putchar('\f');
    xr = pa_maxxg(stdout)/3; /* ratio window but parent */
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window size calculate pixel");
    pa_home(stdout);
    pa_openwin(&stdin, &win2, NULL, 2);
    pa_linewidth(stdout, 1);
    pa_fcolor(win2, pa_cyan);
    pa_winclientg(stdout, xr, xr, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (%d, %d) client, full frame, window size is: %d,%d\n", xr, xr, x, y);
    pa_setsizg(win2, x, y);
    pa_rect(win2, 1, 1, xr, xr);
    pa_line(win2, 1, 1, xr, xr);
    pa_line(win2, 1, xr, xr, 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    printf("System bar off\n");
    pa_sysbar(win2, OFF);
    pa_winclientg(stdout, xr, xr, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize));
    printf("For (%d, %d) client, no system bar, window size is: %d,%d\n", xr, xr, x, y);
    pa_setsizg(win2, x, y);
    putc('\f', win2);
    pa_rect(win2, 1, 1, xr, xr);
    pa_line(win2, 1, 1, xr, xr);
    pa_line(win2, 1, xr, xr, 1);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    printf("Sizing bars off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, OFF);
    pa_winclientg(stdout, xr, xr, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsysbar));
    printf("For (%d, %d) client, no sizing, window size is: %d,%d\n", xr, xr, x, y);
    pa_setsizg(win2, x, y);
    putc('\f', win2);
    pa_rect(win2, 1, 1, xr, xr);
    pa_line(win2, 1, 1, xr, xr);
    pa_line(win2, 1, xr, xr, 1);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    printf("frame off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, ON);
    pa_frame(win2, OFF);
    pa_winclientg(stdout, xr, xr, &x, &y, BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (%d, %d) client, no frame, window size is: %d,%d\n", xr, xr, x, y);
    pa_setsizg(win2, x, y);
    putc('\f', win2);
    pa_rect(win2, 1, 1, xr, xr);
    pa_line(win2, 1, 1, xr, xr);
    pa_line(win2, 1, xr, xr, 1);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    fclose(win2);

    /* ******************* Window size calculate minimums pixel *************** */

    /* this test does not work, winclient needs to return the minimums */

#if 0
    putchar('\f');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window size calculate minimum pixel");
    pa_home(stdout);
    pa_openwin(&stdin, &win2, NULL, 2);
    pa_linewidth(stdout, 1);
    pa_fcolor(win2, pa_cyan);
    pa_winclientg(stdout, 1, 1, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (200, 200) client, full frame, window size minimum is: %d,%d\n", x, y);
    pa_setsizg(win2, 1, 1);
    pa_getsizg(win2, &x2, &y2);
    waitnext();

    fclose(win2);
#endif

    /* ********************** Child windows torture test pixel ***************** */

    pa_getsizg(stdout, &xs, &ys); /* get window size */
    if (xs > ys) { xs /= 3.5; ys = xs; }
    else { ys /= 3.5; xs = ys; }
    c1 = pa_red;
    c2 = pa_green;
    c3 = pa_blue;
    putchar('\f');
    printf("Child windows torture test pixel\n");
    t = pa_clock(); /* get base time */
    for (i = 1; i <= 100; i++) {

        pa_openwin(&stdin, &win2, stdout, 2);
        pa_setposg(win2, xs/10, ys/5);
        pa_sizbufg(win2, xs, ys);
        pa_setsizg(win2, xs, ys);
        pa_openwin(&stdin, &win3, stdout, 3);
        pa_setposg(win3, xs/10+xs, ys/5);
        pa_sizbufg(win3, xs, ys);
        pa_setsizg(win3, xs, ys);
        pa_openwin(&stdin, &win4, stdout, 4);
        pa_setposg(win4, xs/10+xs*2, ys/5);
        pa_sizbufg(win4, xs, ys);
        pa_setsizg(win4, xs, ys);
        pa_bcolor(win2, c1);
        c1 = nextcolor(c1);
        putc('\f', win2);
        fprintf(win2, "I am child window 1\n");
        pa_bcolor(win3, c2);
        c2 = nextcolor(c2);
        putc('\f', win3);
        fprintf(win3, "I am child window 2\n");
        pa_bcolor(win4, c3);
        c3 = nextcolor(c3);
        putc('\f', win4);
        fprintf(win4, "I am child window 3\n");
        fclose(win2);
        fclose(win3);
        fclose(win4);

    }
    et = pa_elapsed(t);
    pa_home(stdout);
    pa_bover(stdout);
    printf("Child windows should all be closed\n");
    printf("\n");
    printf("Child windows place and remove %d iterations %f seconds\n", 100,
           et*0.0001);
    printf("%f per iteration\n", et*0.0001/100);
    waitnext();

    terminate: /* terminate */

    putchar('\f');
    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_SIGN);
    pa_fontsiz(stdout, 50);
    prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Test complete");

}
