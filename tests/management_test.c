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
static ami_evtrec  er;
static ami_menuptr mp;           /* menu pointer */
static ami_menuptr ml;           /* menu list */
static ami_menuptr sm;           /* submenu list */
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

/* wait return to be pressed, or handle terminate */

static void waitnext(void)

{

    ami_evtrec er; /* event record */

    do { ami_event(stdin, &er); }
    while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

}

/* wait return to be pressed, or handle terminate, while printing characters */

static void waitnextprint(void)

{

    ami_evtrec er; /* event record */

    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etchar)
            printf("Window: %d char: %c\n", er.winid, er.echar);

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

}

/* print centered string */

static void prtcen(int y, const char* s)

{

   ami_cursor(stdout, (ami_maxx(stdout)/2)-(strlen(s)/2), y);
   printf("%s", s);

}

/* print centered string graphical */

static void prtceng(int y, const char* s)

{

   ami_cursorg(stdout, (ami_maxxg(stdout)/2)-(ami_strsiz(stdout, s)/2), y);
   printf("%s", s);

}

/* wait time in 100 microseconds */

static void waittime(int t)

{

    ami_evtrec er;

    ami_timer(stdout, 1, t, FALSE);
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

/* display frame test */

static void frameinside(const string s, int x, int y)

{

    putchar('\f');
    ami_fcolor(stdout, ami_cyan);
    ami_rect(stdout, 1, 1, x, y);
    ami_line(stdout, 1, 1, x, y);
    ami_line(stdout, 1, y, x, 1);
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    puts(s);
    ami_bover(stdout);

}

static void frametest(const string s)

{

    ami_evtrec er;
    int       x, y;

    x = ami_maxxg(stdout); /* set size */
    y = ami_maxyg(stdout);
    frameinside(s, x, y);
    do {

        ami_event(stdin, &er); /* get next event */
        if (er.etype == ami_etredraw) frameinside(s, x, y);
        if (er.etype == ami_etresize) {

            /* Save the new dimensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = ami_maxxg(stdout);
            y = ami_maxyg(stdout);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);

}

/* Finds the largest square that fits into the screen, then applies a ratio to
   that. Used to determine a relative size that fits the screen. */
static void sqrrat(int* xs, int* ys, float rat)

{

    /* ratio by screen smallest x-y, then square it up */
    ami_getsizg(stdout, xs, ys);
    if (*xs > *ys) { *ys /= rat; *xs = *ys; } /* square */
    else { *xs /= rat; *ys = *xs; }

}

static ami_color nextcolor(ami_color c)

{

    c++;
    if (c > ami_magenta) c = ami_red;

    return (c);

}

int main(void)

{

    int xr;

    if (setjmp(terminate_buf)) goto terminate;

    ami_auto(stdout, OFF);
    ami_curvis(stdout, OFF);
    printf("Managed screen test vs. 0.1\n");
    printf("\n");
    ami_scnsiz(stdout, &x, &y);
    printf("Screen size character: x: %d y: %d\n", x, y);
    ami_scnsizg(stdout, &x, &y);
    printf("Screen size pixel: x: %d y: %d\n", x, y);
    printf("\n");
    ami_getsiz(stdout, &x, &y);
    printf("Window size character: x: %d y: %d\n", x, y);
    ami_getsizg(stdout, &ox, &oy);
    printf("Window size graphical: x: %d y: %d\n", ox, oy);
    printf("\n");
    printf("Client size character: x: %d y: %d\n", ami_maxx(stdout), ami_maxy(stdout));
    printf("Client size graphical: x: %d y: %d\n", ami_maxxg(stdout), ami_maxyg(stdout));
    printf("\n");
    printf("Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Window titling test ************************** */

    ami_title(stdout, "This is a mangement test window");
    printf("The title bar of this window should read: This is a mangement test window\n");
    prtceng(ami_maxyg(stdout)-ami_chrsizy(stdout), "Window title test");
    waitnext();

    /* ************************** Multiple windows ************************** */

    putchar('\f');
    ami_curvis(stdout, ON);
    prtceng(ami_maxyg(stdout)-ami_chrsizy(stdout), "Multiple window test");
    ami_home(stdout);
    ami_auto(stdout, ON);
    printf("This is the main window");
    printf("\n");
    printf("Select back and forth between each window, and make sure the\n");
    printf("cursor follows\n");
    printf("\n");
    printf("Here is the cursor->");
    ami_openwin(&stdin, &win2, NULL, 2);
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
    ami_curvis(stdout, OFF);
    ami_auto(stdout, OFF);

    /* ********************* Resize buffer window character ******************** */

    ox = ami_maxx(stdout);
    oy = ami_maxy(stdout);
    ami_bcolor(stdout, ami_white);
    ami_sizbuf(stdout, 50, 50);
    ami_bcolor(stdout, ami_cyan);
    putchar('\f');
    for (x = 1; x <= ami_maxx(stdout); x++) printf("*");
    ami_cursor(stdout, 1, ami_maxy(stdout));
    for (x = 1; x <= ami_maxx(stdout); x++) printf("*");
    for (y = 1; y <= ami_maxy(stdout); y++) { ami_cursor(stdout, 1, y); printf("*"); }
    for (y = 1; y <= ami_maxy(stdout); y++) { ami_cursor(stdout, ami_maxx(stdout), y); printf("*"); }
    ami_home(stdout);
    printf("Buffer should now be 50 by 50 characters, and\n");
    printf("painted blue\n");
    printf("maxx: %d maxy: %d\n", ami_maxx(stdout), ami_maxy(stdout));
    printf("Open up window to verify this\n");
    prtcen(ami_maxy(stdout), "Buffer resize character test\n");
    ami_bcolor(stdout, ami_white);
    waitnext();
    ami_sizbuf(stdout, ox, oy);

    /* *********************** Resize buffer window pixel ********************** */

    ox = ami_maxxg(stdout);
    oy = ami_maxyg(stdout);
    sqrrat(&xs, &ys, 1.3); /* find square ratio */
    ami_bcolor(stdout, ami_white);
    ami_sizbufg(stdout, xs, ys);
    ami_bcolor(stdout, ami_cyan);
    putchar('\f');
    ami_linewidth(stdout, 20);
    ami_line(stdout, 1, 1, ami_maxxg(stdout), 1);
    ami_line(stdout, 1, 1, 1, ami_maxyg(stdout));
    ami_line(stdout, 1, ami_maxyg(stdout), ami_maxxg(stdout), ami_maxyg(stdout));
    ami_line(stdout, ami_maxxg(stdout), 1, ami_maxxg(stdout), ami_maxyg(stdout));
    printf("Buffer should now be %d by %d pixels, and\n", xs, ys);
    printf("painted blue\n");
    printf("maxxg: %d maxyg: %d\n", ami_maxxg(stdout), ami_maxyg(stdout));
    printf("Open up window to verify this\n");
    prtcen(ami_maxy(stdout), "Buffer resize graphical test");
    ami_bcolor(stdout, ami_white);
    waitnext();
    ami_sizbufg(stdout, ox, oy);

    /* ****************** Resize screen with buffer on character *************** */

    ox = ami_maxxg(stdout);
    oy = ami_maxyg(stdout);
    for (x = 20; x <= 80; x++) {

        ami_setsiz(stdout, x, 25);
        ami_getsiz(stdout, &x2, &y2);
        if (x2 != x || y2 != 25) {

            ami_setsiz(stdout, 80, 25);
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

        ami_setsiz(stdout, 80, y);
        ami_getsiz(stdout, &x2, &y2);
        if (x2 != 80 || y2 != y) {

            ami_setsiz(stdout, 80, 25);
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
    ami_winclientg(stdout, ox, oy, &ox, &oy, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(stdout, ox, oy);

    /* ******************** Resize screen with buffer on pixel ***************** */

    ox = ami_maxxg(stdout);
    oy = ami_maxyg(stdout);
    sqrrat(&xs, &ys, 1.5); /* find square ratio */
    for (x = xs; x <= xs*4; x += xs/64) {

        ami_setsizg(stdout, x, ys);
        ami_getsizg(stdout, &x2, &y2);
        if (x2 != x || y2 != ys) {

            ami_setsiz(stdout, 80, 25);
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

        ami_setsizg(stdout, xs, y);
        ami_getsizg(stdout, &x2, &y2);
        if (x2 != xs || y2 != y) {

            ami_setsiz(stdout, 80, 25);
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
    ami_winclientg(stdout, ox, oy, &ox, &oy, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(stdout, ox, oy);

    /* ********************************* Front/back test *********************** */

    sqrrat(&xs, &ys, 8); /* find square ratio */
    cs = ami_chrsizy(stdout); /* save the character size */
    putchar('\f');
    ami_auto(stdout, OFF);
    printf("Position window for font/back test\n");
    printf("Then hit space to flip font/back status, or return to stop\n");
    fb = FALSE; /* clear front/back status */
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, ys);

    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etchar) if (er.echar == ' ') { /* flip front/back */

            fb = !fb;
            if (fb) {

                ami_front(stdout);
                ami_fcolor(stdout, ami_white);
                prtceng(ami_maxyg(stdout)/2-ami_chrsizy(stdout)/2, "Back");
                ami_fcolor(stdout, ami_black);
                prtceng(ami_maxyg(stdout)/2-ami_chrsizy(stdout)/2, "Front");

            } else {

                ami_back(stdout);
                ami_fcolor(stdout, ami_white);
                prtceng(ami_maxyg(stdout)/2-ami_chrsizy(stdout)/2, "Front");
                ami_fcolor(stdout, ami_black);
                prtceng(ami_maxyg(stdout)/2-ami_chrsizy(stdout)/2, "Back");

            }

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_home(stdout);
    ami_fontsiz(stdout, cs);
    ami_font(stdout, AMI_FONT_TERM);
    ami_auto(stdout, ON);

    /* ************************* Frame controls test buffered ****************** */

    putchar('\f');
    ami_fcolor(stdout, ami_cyan);
    ami_rect(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    ami_line(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    ami_line(stdout, 1, ami_maxyg(stdout), ami_maxxg(stdout), 1);
    ami_fcolor(stdout, ami_black);
    ami_binvis(stdout);
    printf("Ready for frame controls buffered\n");
    printf("(Note system may not implement all -- or any frame controls)\n");
    waitnext();
    ami_frame(stdout, OFF);
    printf("Entire frame off\n");
    waitnext();
    ami_frame(stdout, ON);
    printf("Entire frame on\n");
    waitnext();
    ami_sysbar(stdout, OFF);
    printf("System bar off\n");
    waitnext();
    ami_sysbar(stdout, ON);
    printf("System bar on\n");
    waitnext();
    ami_sizable(stdout, OFF);
    printf("Size bars off\n");
    waitnext();
    ami_sizable(stdout, ON);
    printf("Size bars on\n");
    waitnext();
    ami_bover(stdout);

    /* ************************* Frame controls test unbuffered ****************** */

    ami_buffer(stdout, OFF);
    frametest("Ready for frame controls unbuffered - Resize me!");
    printf("(Note system may not implement all -- or any frame controls)\n");
    ami_frame(stdout, OFF);
    frametest("Entire frame off");
    ami_frame(stdout, ON);
    frametest("Entire frame on");
    ami_sysbar(stdout, OFF);
    frametest("System bar off");
    ami_sysbar(stdout, ON);
    frametest("System bar on");
    ami_sizable(stdout, OFF);
    frametest("Size bars off");
    ami_sizable(stdout, ON);
    frametest("Size bars on");
    ami_buffer(stdout, ON);

    /* ********************************* Menu test ***************************** */

    ami_auto(stdout, ON);
    putchar('\f');
    ami_fcolor(stdout, ami_cyan);
    ami_rect(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    ami_line(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    ami_line(stdout, 1, ami_maxyg(stdout), ami_maxxg(stdout), 1);
    ami_fcolor(stdout, ami_black);
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
    ami_menu(stdout, ml);
    ami_menuena(stdout, 3, OFF); /* disable "Walk" */
    ami_menusel(stdout, 5, ON); /* turn on "slow" */
    ami_menusel(stdout, 8, ON); /* turn on "red" */

    ami_home(stdout);
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

        ami_event(stdin, &er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (er.etype == ami_etmenus) {

            printf("Menu select: ");
            switch (er.menuid) {

                case 1:  printf("Say hello\n"); break;
                case 2:  printf("Bark\n"); break;
                case 3:  printf("Walk\n"); break;
                case 4:  printf("Sublist\n"); break;
                case 5:  printf("slow\n"); ami_menusel(stdout, 5, ON); break;
                case 6:  printf("medium\n"); ami_menusel(stdout, 6, ON); break;
                case 7:  printf("fast\n"); ami_menusel(stdout, 7, ON); break;
                case 8:  printf("red\n"); sred = !sred;
                         ami_menusel(stdout, 8, sred); break;
                case 9:  printf("green\n"); sgreen = !sgreen;
                         ami_menusel(stdout, 9, sgreen); break;
                case 10: printf("blue\n"); sblue = !sblue;
                         ami_menusel(stdout, 10, sblue); break;

            }

        }

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    ami_menu(stdout, NULL);

    /* ****************************** Standard menu test ******************** */

    putchar('\f');
    ami_auto(stdout, ON);
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
    ami_menu(stdout, mp);
    printf("Standard menu appears above\n");
    printf("Check our 'one', 'two', 'three' buttons are in the program\n");
    printf("defined position\n");
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);
        if (er.etype == ami_etmenus) {

            printf("Menu select: ");
            switch (er.menuid) {

                case AMI_SMNEW:       printf("new\n"); break;
                case AMI_SMOPEN:      printf("open\n"); break;
                case AMI_SMCLOSE:     printf("close\n"); break;
                case AMI_SMSAVE:      printf("save\n"); break;
                case AMI_SMSAVEAS:    printf("saveas\n"); break;
                case AMI_SMPAGESET:   printf("pageset\n"); break;
                case AMI_SMPRINT:     printf("print\n"); break;
                case AMI_SMEXIT:      printf("exit\n"); break;
                case AMI_SMUNDO:      printf("undo\n"); break;
                case AMI_SMCUT:       printf("cut\n"); break;
                case AMI_SMPASTE:     printf("paste\n"); break;
                case AMI_SMDELETE:    printf("delete\n"); break;
                case AMI_SMFIND:      printf("find\n"); break;
                case AMI_SMFINDNEXT:  printf("findnext\n"); break;
                case AMI_SMREPLACE:   printf("replace\n"); break;
                case AMI_SMGOTO:      printf("goto\n"); break;
                case AMI_SMSELECTALL: printf("selectall\n"); break;
                case AMI_SMNEWWINDOW: printf("newwindow\n"); break;
                case AMI_SMTILEHORIZ: printf("tilehoriz\n"); break;
                case AMI_SMTILEVERT:  printf("tilevert\n"); break;
                case AMI_SMCASCADE:   printf("cascade\n"); break;
                case AMI_SMCLOSEALL:  printf("closeall\n"); break;
                case AMI_SMHELPTOPIC: printf("helptopic\n"); break;
                case AMI_SMABOUT:     printf("about\n"); break;
                case AMI_SMMAX+1:     printf("one\n"); break;
                case AMI_SMMAX+2:     printf("two\n"); break;
                case AMI_SMMAX+3:     printf("three\n"); break;

            }

        }

    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    ami_menu(stdout, NULL);

    /* ************************* Child windows test character ****************** */

    putchar('\f');
    chrgrid();
    prtcen(ami_maxy(stdout), "Child windows test character");
    ami_openwin(&stdin, &win2, stdout, 2);
    ami_curvis(win2, OFF);
    ami_setpos(win2, 1, 10);
    ami_sizbuf(win2, 20, 10);
    ami_setsiz(win2, 20, 10);
    ami_openwin(&stdin, &win3, stdout, 3);
    ami_curvis(win3, OFF);
    ami_setpos(win3, 21, 10);
    ami_sizbuf(win3, 20, 10);
    ami_setsiz(win3, 20, 10);
    ami_openwin(&stdin, &win4, stdout, 4);
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
    ami_home(stdout);
    printf("There should be 3 labeled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)      \n");
    waitnext();
    ami_frame(win2, OFF);
    ami_frame(win3, OFF);
    ami_frame(win4, OFF);
    ami_home(stdout);
    printf("There should be 3 labeled child windows below, without frames\n");
    printf("                                                            \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_home(stdout);
    printf("Child windows should all be closed                           \n");
    waitnext();

    /* *************************** Child windows test pixel ******************** */

    putchar('\f');
    sqrrat(&xs, &ys, 2.5); /* find square ratio */
    prtcen(ami_maxy(stdout), "Child windows test pixel");
    ami_openwin(&stdin, &win2, stdout, 2);
    ami_curvis(win2, OFF);
    ami_setposg(win2, xs*0+1, ys/2.5);
    ami_sizbufg(win2, xs, ys);
    ami_setsizg(win2, xs, ys);
    ami_openwin(&stdin, &win3, stdout, 3);
    ami_curvis(win3, OFF);
    ami_setposg(win3, xs*1+1, ys/2.5);
    ami_sizbufg(win3, xs, ys);
    ami_setsizg(win3, xs, ys);
    ami_openwin(&stdin, &win4, stdout, 4);
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
    ami_home(stdout);
    printf("There should be 3 labled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)      \n");
    waitnext();
    ami_frame(win2, OFF);
    ami_frame(win3, OFF);
    ami_frame(win4, OFF);
    ami_home(stdout);
    printf("There should be 3 labled child windows below, without frames\n");
    printf("                                                            \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_home(stdout);
    printf("Child windows should all be closed                          \n");
    printf("                                                            \n");
    waitnext();

    /* *************** Child windows independent test character ************ */

    ami_curvis(stdout, ON);
    putchar('\f');
    chrgrid();
    prtcen(ami_maxy(stdout), "Child windows independent test character");
    ami_openwin(&stdin, &win2, stdout, 2);
    ami_setpos(win2, 11, 10);
    ami_sizbuf(win2, 30, 10);
    ami_setsiz(win2, 30, 10);
    ami_openwin(&stdin, &win3, stdout, 3);
    ami_setpos(win3, 41, 10);
    ami_sizbuf(win3, 30, 10);
    ami_setsiz(win3, 30, 10);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_home(stdout);
    printf("There should be 2 labeled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)       \n");
    printf("Test focus can be moved between windows, including the main  \n");
    printf("window. Test windows can be minimized and maximized          \n");
    printf("(if framed), test entering characters to windows.            \n");
    do {

        ami_event(stdin, &er); /* get next event */
        if (er.etype == ami_etchar) {

            if (er.winid == 2) fputc(er.echar, win2);
            else if (er.winid == 3) fputc(er.echar, win3);

        } else if (er.etype == ami_etenter) {

            /* translate the crs so we can test scrolling */
            if (er.winid == 2) fputc('\n', win2);
            else if (er.winid == 3) fputc('\n', win3);

        } else if (er.etype == ami_etterm && er.winid == 1)
            /* only take terminations from main window */
            longjmp(terminate_buf, 1);

    /* terminate on cr to the main window only */
    } while (er.etype != ami_etenter || er.winid != 1);
    fclose(win2);
    fclose(win3);
    ami_home(stdout);
    printf("Child windows should all be closed                           \n");
    printf("                                                             \n");
    printf("                                                             \n");
    printf("                                                             \n");
    printf("                                                             \n");
    ami_curvis(stdout, OFF);
    waitnext();

    /* ******************** Child windows independent test pixel ************** */

    putchar('\f');
    sqrrat(&xs, &ys, 2); /* find square ratio */
    prtcen(ami_maxy(stdout), "Child windows test pixel");
    ami_openwin(&stdin, &win2, stdout, 2);
    ami_setposg(win2, xs*0+xs/5, ys/2);
    ami_sizbufg(win2, xs, ys);
    ami_setsizg(win2, xs, ys);
    ami_openwin(&stdin, &win3, stdout, 3);
    ami_setposg(win3, xs*1+xs/5, ys/2);
    ami_sizbufg(win3, xs, ys);
    ami_setsizg(win3, xs, ys);
    ami_bcolor(win2, ami_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    ami_bcolor(win3, ami_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    ami_home(stdout);
    printf("There should be 2 labeled child windows below, with frames   \n");
    printf("(the system may not implement frames on child windows)      \n");
    printf("Test focus can be moved between windows, test windows can be \n");
    printf("minimized and maximized (if framed), test entering           \n");
    printf("characters to windows.                                       \n");
    do {

        ami_event(stdin, &er); /* get next event */
        if (er.etype == ami_etchar) {

            if (er.winid == 2) fputc(er.echar, win2);
            else if (er.winid == 3) fputc(er.echar, win3);

        } else if (er.etype == ami_etenter) {

            /* translate the crs so we can test scrolling */
            if (er.winid == 2) fputc('\n', win2);
            else if (er.winid == 3) fputc('\n', win3);

        } else if (er.etype == ami_etterm && er.winid == 1)
            /* only take terminations from main window */
            longjmp(terminate_buf, 1);

    /* terminate on cr to the main window only */
    } while (er.etype != ami_etenter || er.winid != 1);
    fclose(win2);
    fclose(win3);
    ami_home(stdout);
    printf("Child windows should all be closed                          \n");
    printf("                                                            \n");
    printf("                                                            \n");
    printf("                                                            \n");
    printf("                                                            \n");
    waitnext();

    /* ******************* Child windows stacking test pixel ******************* */

    putchar('\f');
    sqrrat(&xs, &ys, 2.5); /* find square ratio */
    prtcen(ami_maxy(stdout), "Child windows stacking test pixel");
    ami_openwin(&stdin, &win2, stdout, 2);
    ami_curvis(win2, OFF);
    ami_setposg(win2, xs/2*0+xs/5, ys/2.5+ys*0/4);
    ami_sizbufg(win2, xs, ys);
    ami_setsizg(win2, xs, ys);
    ami_openwin(&stdin, &win3, stdout, 3);
    ami_curvis(win3, OFF);
    ami_setposg(win3, xs/2*1+xs/5, ys/2.5+ys*1/4);
    ami_sizbufg(win3, xs, ys);
    ami_setsizg(win3, xs, ys);
    ami_openwin(&stdin, &win4, stdout, 4);
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
    ami_home(stdout);
    printf("There should be 3 labled child windows below, overlapped,   \n");
    printf("with child 1 on the bottom, child 2 middle, and child 3 top.\n");
    waitnext();
    ami_back(win2);
    ami_back(win3);
    ami_back(win4);
    ami_home(stdout);
    printf("Now the windows are reordered, with child 1 on top, child 2 \n");
    printf("below that, and child 3 on the bottom.                      \n");
    waitnext();
    ami_front(win2);
    ami_front(win3);
    ami_front(win4);
    ami_home(stdout);
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
    ami_buffer(stdout, OFF);
    ami_auto(stdout, OFF);
    ami_openwin(&stdin, &win2, stdout, 2);
    ami_setposg(win2, xs/2*1, ys/2*1);
    ami_sizbufg(win2, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
    ami_setsizg(win2, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
    ami_openwin(&stdin, &win3, stdout, 3);
    ami_setposg(win3, xs/2*2, ys/2*2);
    ami_sizbufg(win3, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
    ami_setsizg(win3, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
    ami_openwin(&stdin, &win4, stdout, 4);
    ami_setposg(win4, xs/2*3, ys/2*3);
    ami_sizbufg(win4, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
    ami_setsizg(win4, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
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

        ami_event(stdin, &er);
        if (er.etype == ami_etredraw || er.etype == ami_etresize) {

            putchar('\f');
            prtceng(ami_maxyg(stdout)-ami_chrsizy(stdout),
                    "Child windows stacking resize test pixel 1");
            prtceng(1, "move and resize");
            ami_setsizg(win3, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
            ami_setsizg(win4, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);
            ami_setsizg(win2, ami_maxxg(stdout)-xs*2, ami_maxyg(stdout)-ys*2);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_buffer(stdout, ON);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ************** Child windows stacking resize test pixel 2 *************** */

    sqrrat(&xs, &ys, 20); /* find square ratio */
    ami_buffer(stdout, OFF);
    ami_openwin(&stdin, &win2, stdout, 2);
    ami_auto(win2, OFF);
    ami_curvis(win2, OFF);
    ami_setposg(win2, xs*1, ys*1);
    ami_sizbufg(win2, ami_strsiz(win2, "I am child window 1"), ami_chrsizy(win2));
    ami_setsizg(win2, ami_maxxg(stdout)-xs*1*2, ami_maxyg(stdout)-ys*1*2);
    ami_openwin(&stdin, &win3, stdout, 3);
    ami_auto(win3, OFF);
    ami_curvis(win3, OFF);
    ami_setposg(win3, xs*2, ys*2);
    ami_sizbufg(win2, ami_strsiz(win3, "I am child window 2"), ami_chrsizy(win3));
    ami_setsizg(win3, ami_maxxg(stdout)-xs*2*2, ami_maxyg(stdout)-ys*2*2);
    ami_openwin(&stdin, &win4, stdout, 4);
    ami_auto(win4, OFF);
    ami_curvis(win4, OFF);
    ami_setposg(win4, xs*3, ys*3);
    ami_sizbufg(win2, ami_strsiz(win4, "I am child window 3"), ami_chrsizy(win4));
    ami_setsizg(win4, ami_maxxg(stdout)-xs*3*2, ami_maxyg(stdout)-ys*3*2);
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

        ami_event(stdin, &er);
        if (er.etype == ami_etredraw  || er.etype == ami_etresize) {

            putchar('\f');
            prtceng(ami_maxyg(stdout)-ami_chrsizy(stdout),
                    "Child windows stacking resize test pixel 2");
            prtceng(1, "move and resize");
            ami_setsizg(win2, ami_maxxg(stdout)-xs*1*2, ami_maxyg(stdout)-ys*1*2);
            ami_setsizg(win3, ami_maxxg(stdout)-xs*2*2, ami_maxyg(stdout)-ys*2*2);
            ami_setsizg(win4, ami_maxxg(stdout)-xs*3*2, ami_maxyg(stdout)-ys*3*2);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    ami_buffer(stdout, ON);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ******************************* Buffer off test *********************** */

    putchar('\f');
    cs = ami_chrsizy(stdout); /* save the character size */
    ami_auto(stdout, OFF);
    ami_buffer(stdout, OFF);
    /* initialize prime size information */
    x = ami_maxxg(stdout);
    y = ami_maxyg(stdout);
    ami_linewidth(stdout, 5); /* set large lines */
    ami_font(stdout, AMI_FONT_SIGN);
    ami_binvis(stdout);
    do {

        ami_event(stdin, &er); /* get next event */
        if (er.etype == ami_etredraw || er.etype == ami_etresize) {

            /* clear screen without overwriting frame */
            ami_fcolor(stdout, ami_white);
            ami_frect(stdout, 1+5, 1+5, x-5, y-5);
            ami_fcolor(stdout, ami_black);
            ami_fontsiz(stdout, y / 10);
            prtceng(ami_maxyg(stdout)/2-ami_chrsizy(stdout)/2,
                    "SIZE AND COVER ME !");
            ami_rect(stdout, 1+2, 1+2, x-2, y-2); /* frame the window */

        }
        if (er.etype == ami_etresize) {

            /* Save the new demensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = ami_maxxg(stdout);
            y = ami_maxyg(stdout);

        }
        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_buffer(stdout, ON);
    ami_fontsiz(stdout, cs);
    ami_font(stdout, AMI_FONT_TERM);
    ami_home(stdout);
    ami_auto(stdout, ON);

    /* ****************************** min/max/norm test ********************* */

    putchar('\f');
    ami_auto(stdout, OFF);
    ami_buffer(stdout, OFF);
    ami_font(stdout, AMI_FONT_TERM);
    mincnt = 0; /* clear minimize counter */
    maxcnt = 0; /* clear maximize counter */
    nrmcnt = 0; /* clear normalize counter */
    do {

        ami_event(stdin, &er); /* get next event */
        /* count minimize, maximize, normalize */
        if (er.etype == ami_etmax) maxcnt = maxcnt+1;
        if (er.etype == ami_etmin) mincnt = mincnt+1;
        if (er.etype == ami_etnorm) nrmcnt = nrmcnt+1;
        if (er.etype == ami_etredraw || er.etype == ami_etmax ||
            er.etype == ami_etmin || er.etype == ami_etnorm) {

            putchar('\f');
            printf("Minimize, maximize and restore this window\n");
            printf("\n");
            printf("Minimize count:  %d\n", mincnt);
            printf("Maximize count:  %d\n", maxcnt);
            printf("Normalize count: %d\n", nrmcnt);

        }

        if (er.etype == ami_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != ami_etenter);
    ami_buffer(stdout, ON);

    /* ******************** Window size calculate character ***************** */

    putchar('\f');
    prtceng(ami_maxyg(stdout)-ami_chrsizy(stdout), "Window size calculate character");
    ami_home(stdout);
    ami_openwin(&stdin, &win2, NULL, 2);
    ami_linewidth(stdout, 1);

    ami_winclient(stdout, 20, 10, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    printf("For (20, 10) client, full frame, window size is: %d,%d\n", x, y);
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
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("System bar off\n");
    ami_sysbar(win2, OFF);
    ami_winclient(stdout, 20, 10, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize));
    printf("For (20, 10) client, no system bar, window size is: %d,%d\n", x, y);
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
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("Sizing bars off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, OFF);
    ami_winclient(stdout, 20, 10, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsysbar));
    printf("For (20, 10) client, no size bars, window size is: %d,%d\n", x, y);
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
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("frame off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, ON);
    ami_frame(win2, OFF);
    ami_winclient(stdout, 20, 10, &x, &y, BIT(ami_wmsize) | BIT(ami_wmsysbar));
    printf("For (20, 10) client, no frame, window size is: %d,%d\n", x, y);
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
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    fclose(win2);

    /* ************************ Window size calculate pixel ******************** */

    putchar('\f');
    xr = ami_maxxg(stdout)/3; /* ratio window but parent */
    prtceng(ami_maxyg(stdout)-ami_chrsizy(stdout), "Window size calculate pixel");
    ami_home(stdout);
    ami_openwin(&stdin, &win2, NULL, 2);
    ami_linewidth(stdout, 1);
    ami_fcolor(win2, ami_cyan);
    ami_winclientg(stdout, xr, xr, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    printf("For (%d, %d) client, full frame, window size is: %d,%d\n", xr, xr, x, y);
    ami_setsizg(win2, x, y);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    ami_curvis(win2, OFF);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    printf("System bar off\n");
    ami_sysbar(win2, OFF);
    ami_winclientg(stdout, xr, xr, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize));
    printf("For (%d, %d) client, no system bar, window size is: %d,%d\n", xr, xr, x, y);
    ami_setsizg(win2, x, y);
    putc('\f', win2);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    printf("Sizing bars off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, OFF);
    ami_winclientg(stdout, xr, xr, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsysbar));
    printf("For (%d, %d) client, no sizing, window size is: %d,%d\n", xr, xr, x, y);
    ami_setsizg(win2, x, y);
    putc('\f', win2);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    printf("frame off\n");
    ami_sysbar(win2, ON);
    ami_sizable(win2, ON);
    ami_frame(win2, OFF);
    ami_winclientg(stdout, xr, xr, &x, &y, BIT(ami_wmsize) | BIT(ami_wmsysbar));
    printf("For (%d, %d) client, no frame, window size is: %d,%d\n", xr, xr, x, y);
    ami_setsizg(win2, x, y);
    putc('\f', win2);
    ami_rect(win2, 1, 1, xr, xr);
    ami_line(win2, 1, 1, xr, xr);
    ami_line(win2, 1, xr, xr, 1);
    printf("Check client window has (%d, %d) surface\n", xr, xr);
    waitnext();

    fclose(win2);

    /* ******************* Window size calculate minimums pixel *************** */

    /* this test does not work, winclient needs to return the minimums */

#if 0
    putchar('\f');
    prtceng(ami_maxyg(stdout)-ami_chrsizy(stdout), "Window size calculate minimum pixel");
    ami_home(stdout);
    ami_openwin(&stdin, &win2, NULL, 2);
    ami_linewidth(stdout, 1);
    ami_fcolor(win2, ami_cyan);
    ami_winclientg(stdout, 1, 1, &x, &y, BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    printf("For (200, 200) client, full frame, window size minimum is: %d,%d\n", x, y);
    ami_setsizg(win2, 1, 1);
    ami_getsizg(win2, &x2, &y2);
    waitnext();

    fclose(win2);
#endif

    /* ********************** Child windows torture test pixel ***************** */

    ami_getsizg(stdout, &xs, &ys); /* get window size */
    if (xs > ys) { xs /= 3.5; ys = xs; }
    else { ys /= 3.5; xs = ys; }
    c1 = ami_red;
    c2 = ami_green;
    c3 = ami_blue;
    putchar('\f');
    printf("Child windows torture test pixel\n");
    t = ami_clock(); /* get base time */
    for (i = 1; i <= 100; i++) {

        ami_openwin(&stdin, &win2, stdout, 2);
        ami_setposg(win2, xs/10, ys/5);
        ami_sizbufg(win2, xs, ys);
        ami_setsizg(win2, xs, ys);
        ami_openwin(&stdin, &win3, stdout, 3);
        ami_setposg(win3, xs/10+xs, ys/5);
        ami_sizbufg(win3, xs, ys);
        ami_setsizg(win3, xs, ys);
        ami_openwin(&stdin, &win4, stdout, 4);
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
    ami_home(stdout);
    ami_bover(stdout);
    printf("Child windows should all be closed\n");
    printf("\n");
    printf("Child windows place and remove %d iterations %f seconds\n", 100,
           et*0.0001);
    printf("%f per iteration\n", et*0.0001/100);
    waitnext();

    terminate: /* terminate */

    putchar('\f');
    ami_auto(stdout, OFF);
    ami_font(stdout, AMI_FONT_SIGN);
    ami_fontsiz(stdout, 50);
    prtceng(ami_maxyg(stdout)/2-ami_chrsizy(stdout)/2, "Test complete");

}
