/******************************************************************************
*                                                                             *
*                           SCREEN TEST PROGRAM                               *
*                                                                             *
*                    Copyright (C) 2021 Scott A. Franco                       *
*                                                                             *
* This program performs a reasonably complete test of common features in the  *
* terminal level standard.                                                    *
*                                                                             *
* Tests performed:                                                            *
*                                                                             *
* 1. Row id - number each row with a digit in turn. This test uncovers        *
* positioning errors.                                                         *
* 2. Column id - Same for colums.                                             *
* 3. Fill test - fills the screen with the printable ascii characters, and    *
* "elided" control characters. Tests ability to print standard ASCII set.     *
* 4. Sidewinder - Fills the screen starting from the edges in. Tests          *
* positioning.                                                                *
* 5. Bounce - A ball bounces off the walls for a while. Tests positioning.    *
* 6. Scroll - A pattern that is recognizable if shifted is written, then the  *
* display successively scrolled until blank, in each of four directions.      *
* Tests the scrolling ability.                                                *
*                                                                             *
* Notes:                                                                      *
*                                                                             *
* Should have speed tests adjust their length according to actual process     *
* time to prevent tests from taking too long on slow cpu/display.             *
*                                                                             *
* Benchmark results for 80x25 screen, AMD Ryzen 9 3950x NVIDIA GTX 3070       *
* dual:                                                                       *
*                                                                             *
* Windows console library (terminal):                                         *
*                                                                             *
* Character write speed: 0.00007291 Sec. Per character.                       *
* Scrolling speed:       0.00067421 Sec. Per scroll.                          *
* Buffer switch speed:   0.00041666 Sec. per switch.                          *
*                                                                             *
* Windows graphical library (graphics):                                       *
*                                                                             *
* Character write speed: 0.00002350 Sec. Per character.                       *
* Scrolling speed:       0.00034526 Sec. Per scroll.                          *
* Buffer switch speed:   0.00034666 Sec. per switch.                          *
*                                                                             *
* Windows graphical library (graphics), AMD 3950X 3.8ghz, Nvidia 3070 Dual,   *
* scaling off.                                                                *
*                                                                             *
* Type                   Seconds  Per fig                                     *
* --------------------------------------------                                *
* character write speed   0.06    0.000031                                    *
* Scroll speed            0.75    0.000394                                    *
* Buffer flip speed       0.18    0.000208                                    *
*                                                                             *
* Same as above with scaling on:                                              *
*                                                                             *
* Type                   Seconds  Per fig                                     *
* --------------------------------------------                                *
* character write speed   0.06    0.000031                                    *
* Scroll speed            0.57    0.000304                                    *
* Buffer flip speed       0.14    0.000155                                    *
*                                                                             *
* Linux console/xterm with glibc:                                             *
*                                                                             *
* Character write speed: 0.00000619 Sec. Per character.                       *
* Scrolling speed:       0.00135647 Sec. Per scroll.                          *
* Buffer switch speed:   0.00151633 Sec. per switch.                          *
*                                                                             *
* Linux XWindows graphical library (graphics):                                *
*                                                                             *
* Character write speed: 0.00000072 Sec. Per character.                       *
* Scrolling speed:       0.00016763 Sec. Per scroll.                          *
* Buffer switch speed:   0.00000066 Sec. per switch.                          *
*                                                                             *
*******************************************************************************/

#include <setjmp.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <stddef.h>
#include <terminal.h>
#include <services.h>

#include <diag.h>

#define SECOND 10000 /* one second */

/* unpack RGB packed values */
#define REDP(v)   (v >> 16 & 0xff)
#define GREENP(v) (v >> 8 & 0xff)
#define BLUEP(v)  (v & 0xff)

/* macros to unpack color table entries to INT_MAX ratioed numbers */
#define RED(v)   (INT_MAX/256*REDP(v))   /* red */
#define GREEN(v) (INT_MAX/256*GREENP(v)) /* green */
#define BLUE(v)  (INT_MAX/256*BLUEP(v)) /* blue */

static const int colormap[] = {

    0x330000, 0x331900, 0x333300, 0x193300, 0x003300, 0x003319, 0x003333, 
    0x001933, 0x000033, 0x190033, 0x330033, 0x330019, 0x000000, 0x660000,
    0x663300, 0x666600, 0x336600, 0x006600, 0x006633, 0x006666, 0x003366, 
    0x000066, 0x330066, 0x660066, 0x660033, 0x202020, 0x990000, 0x994c00,
    0x999900, 0x4c9900, 0x009900, 0x00994c, 0x009999, 0x004c99, 0x000099,
    0x4c0099, 0x990099, 0x99004c, 0x404040, 0xcc0000, 0xcc6600, 0xcccc00,
    0x66cc00, 0x00cc00, 0x00cc66, 0x00cccc, 0x0066cc, 0x0000cc, 0x6600cc,
    0xcc00cc, 0xcc0066, 0x606060, 0xff0000, 0xff8000, 0xffff00, 0x80ff00,
    0x00ff00, 0x00ff80, 0x00ffff, 0x0080ff, 0x0000ff, 0x7f00ff, 0xff00ff,
    0xff007f, 0x808080, 0xff3333, 0xff9933, 0xffff33, 0x99ff33, 0x33ff33,
    0x33ff99, 0x33ffff, 0x3399ff, 0x3333ff, 0x9933ff, 0xff33ff, 0xff3399,
    0xa0a0a0, 0xff6666, 0xffb266, 0xffff66, 0xb2ff66, 0x66ff66, 0x66ffb2,
    0x66ffff, 0x66b2ff, 0x6666ff, 0xb266ff, 0xff66ff, 0xff66b2, 0xc0c0c0,
    0xff9999, 0xffcc99, 0xffff99, 0xccff99, 0x99ff99, 0x99ffcc, 0x99ffff,
    0x99ccff, 0x9999ff, 0xcc99ff, 0xff99ff, 0xff99cc, 0xe0e0e0, 0xffcccc,
    0xffe5cc, 0xffffcc, 0xe5ffcc, 0xccffcc, 0xccffe5, 0xccffff, 0xcce5ff,
    0xccccff, 0xe5ccff, 0xffccff, 0xffcce5, 0xffffff

};

typedef enum {

    bncharw,     /* character write */
    bnscroll,    /* scroll */
    bnbuffer,    /* buffer flip */

} bench;

static struct { /* benchmark stats records */

    int iter; /* number of iterations performed */
    int time; /* time in 100us for test */

} benchtab[bnbuffer+1];
static bench bi;

static int x, y, lx, ly, tx, ty, dx, dy, maxy, maxx;
static int c;
static int top, bottom, lside, rside; /* borders */
static enum { dup, ddown, dleft, dright } direction; /* writing direction */
static int count, t1, t2, delay, minlen;   /* maximum direction, x or y */
static ami_evtrec er;   /* event record */
static int i, j, b, tc, cnt, tn;
static long clk;
static FILE *tf;   /* test file */
static char tf_NAME[10/*_FNSIZE*/] = "testfile";
static int eventflag1, eventflag2;
static ami_pevthan oeh1;
static ami_pevthan oeh2;
static char line[250];

static int          tn;       /* thread number */
static volatile int ln;       /* lock number */
static volatile int thdstp;   /* thread stop flag */
static int          sn;       /* thread stop signal */
static int          etn;      /* event thread number */
static int          esn;      /* event thread stop signal */
static volatile int ethdstp;  /* event thread stop flag */
static int          timeout1; /* first timer fires */
static int          timeout2; /* second timer fires */

/* draw box */

static void box(int sx, int sy, int ex, int ey, char c)
{

    int x, y;

    /* top */
    ami_cursor(stdout, sx, sy);
    for (x = sx; x <= ex; x++) putchar(c);
    /* bottom */
    ami_cursor(stdout, sx, ey);
    for (x = sx; x <= ex; x++) putchar(c);
    /* left */
    for (y = sy; y <= ey; y++) { ami_cursor(stdout, sx, y); putchar(c); }
    /* right */
    for (y = sy; y <= ey; y++) { ami_cursor(stdout, ex, y); putchar(c); }

}


static jmp_buf terminate_buf;
static ami_pevthan oldtermevent;

/* wait time in 100 microseconds */
static void waittime(int n, int t)
{

    ami_evtrec er; /* event record */

    ami_timer(stdout, n, t, 0);
    do { ami_event(stdin, &er);
    } while (er.etype != ami_ettim);

}


/* wait return to be pressed, or handle terminate */
static void waitnext(void)
{

    ami_evtrec er; /* event record */

    do { ami_event(stdin, &er);
    } while (er.etype != ami_etenter);

}

/** ****************************************************************************

Draw foreground color from packed 32 bit color

Takes a file and a 32 bit packed RGB color, and sets the foreground color.

*******************************************************************************/

static void fcolorp(
    /** 32 bit packed color */ unsigned long c
)

{

    ami_fcolorc(stdout, RED(c), GREEN(c), BLUE(c));

}

/** ****************************************************************************

Draw background color from packed 32 bit color

Takes a file and a 32 bit packed RGB color, and sets the background color.
table.

*******************************************************************************/

static void bcolorp(
    /** 32 bit packed color */ unsigned long c
)

{

    ami_bcolorc(stdout, RED(c), GREEN(c), BLUE(c));

}

static void timetest(void)
{

    int i, t, et, max, min;
    long total;
    ami_evtrec er;

    printf("Timer test, measuring minimum timer resolution, 100 samples\n\n");
    max = 0;
    min = INT_MAX;
    total = 0;
    for (i = 1; i <= 100; i++) {

        t = ami_clock();
        ami_timer(stdout, 1, 1, 0);
        do { putchar('*'); ami_event(stdin, &er); } while (er.etype != ami_ettim);
        et = ami_elapsed(t);
        total += ami_elapsed(t);
        if (et > max) max = et;
        if (et < min) min = et;

    }
    printf("\n");
    printf("\n");
    printf("Average time was: %ld00 Microseconds\n", total / 100);
    printf("Minimum time was: %d00 Microseconds\n", min);
    printf("Maximum time was: %d00 Microseconds\n", max);
    printf("This timer supports frame rates up to %ld", 10000 / (total / 100));
    printf(" frames per second\n");
    t = ami_clock();
    ami_timer(stdout, 1, 10000, 0);
    do { ami_event(stdin, &er); } while (er.etype != ami_ettim);
    printf("1 second time, was: %ld00 Microseconds\n", ami_elapsed(t));
    printf("\n");
    printf("30 seconds of 1 second ticks:\n");
    printf("\n");
    for (i = 1; i <= 30; i++) {

        ami_timer(stdout, 1, 10000, 0);
        do { ami_event(stdin, &er);
        } while (er.etype != ami_ettim);
        putchar('.');

    }

}

static void frametest(void)

{

    int i, t;
    long et, max, min;
    long total;
    ami_evtrec er;

    printf("Framing timer test, measuring 10 occurances of the framing timer\n\n");
    ami_frametimer(stdout, TRUE);
        max = 0;
    min = INT_MAX;
    total = 0;
    for (i = 1; i <= 10; i++) {

        t = ami_clock();
        do { putchar('.'); ami_event(stdin, &er); } while (er.etype != ami_etframe);
        et = ami_elapsed(t);
        total += ami_elapsed(t);
        if (et > max) max = et;
        if (et < min) min = et;

    }
    ami_frametimer(stdout, FALSE);
    printf("\n");
    printf("\n");
    printf("Average time was: %ld00 Microseconds\n", total / 10);
    printf("Minimum time was: %ld00 Microseconds\n", min);
    printf("Maximum time was: %ld00 Microseconds\n", max);

}

/* plot joystick on screen */

static void plotjoy(int line, int joy)

{

    int i, x;
    double r;

    ami_cursor(stdout, 1, line);
    for (i = 1; i <= ami_maxx(stdout); i++) putchar(' '); /* clear line */
    if (joy < 0) {  /* plot left */

        r = labs(joy);
        x = ami_maxx(stdout)/2-floor(r*(ami_maxx(stdout)/2)/INT_MAX+0.5);
        ami_cursor(stdout, x, line);
        while (x <= ami_maxx(stdout) / 2) {

            putchar('*');
            x++;

        }

    } else { /* plot right */

        r = joy;
        x = (int)floor(r * (ami_maxx(stdout) / 2) / INT_MAX + ami_maxx(stdout) / 2 + 0.5);
        i = ami_maxx(stdout) / 2;
        ami_cursor(stdout, i, line);
        while (i <= x) {

            putchar('*');
            i++;

        }

    }

}

/* print centered string */

static void prtcen(int y, char *s)

{

    ami_cursor(stdout, ami_maxx(stdout)/2-strlen(s)/2, y);
    fputs(s, stdout);

}

/* print center banner string */

static void prtban(char *s)

{

    int i;

    ami_cursor(stdout, ami_maxx(stdout)/2-strlen(s)/2-1, ami_maxy(stdout)/2-1);
    for (i = 1; i <= strlen(s)+2; i++) putchar(' ');
    ami_cursor(stdout, ami_maxx(stdout)/2-strlen(s)/2-1, ami_maxy(stdout)/2);
    putchar(' ');
    prtcen(ami_maxy(stdout) / 2, s);
    putchar(' ');
    ami_cursor(stdout, ami_maxx(stdout)/2-strlen(s)/2-1, ami_maxy(stdout)/2+1);
    for (i = 1; i <= strlen(s)+2; i++) putchar(' ');

}

void event_vector_1(ami_evtptr er)

{

    if (er->etype != ami_etframe) er->handled = FALSE;
    eventflag1 = TRUE;

}

void event_vector_2(ami_evtptr er)

{

    if (er->etype != ami_etframe) er->handled = FALSE;
    eventflag2 = TRUE;

}

/* wait events and signal threads for timer events */
void eventthread(void)

{

    ami_evtrec er; /* event record */
    int stop;

    stop = FALSE; /* set no stop */
    do { ami_event(stdin, &er);

        ami_lock(ln);
        if (er.etype == ami_ettim) {

            if (er.timnum == 1) ami_sendsig(timeout1);
            else if (er.timnum == 2) ami_sendsig(timeout2);

        }
        stop = ethdstp;
        ami_unlock(ln);

    } while (!stop);
    ami_lock(ln);
    ami_sendsig(esn); /* signal thread complete */
    ami_unlock(ln);

}

void thread(void)

{

    int i;
    int x, y;
    int stop;

    stop = FALSE; /* set no stop */
    x = ami_maxx(stdout)/3*2;
    y = ami_maxy(stdout)/2;
    while (!stop) { /* while no stop flag */

        i = 1;
        for (i = 0; i < 10; i++) {

            ami_lock(ln);
            box(x-i, y-i, x+i, y+i, '*');
            ami_waitsig(ln, timeout2);
            box(x-i, y-i, x+i, y+i, ' ');
            ami_unlock(ln);
            i++;

        }
        ami_lock(ln);
        stop = thdstp;
        ami_unlock(ln);


    }
    ami_lock(ln);
    ami_sendsig(sn); /* signal thread complete */
    ami_unlock(ln);

}

/* terminate program on terminate event*/

void termevent(ami_evtptr er)

{

    longjmp(terminate_buf, 1);

}

int main(int argc, char *argv[])
{

    if (setjmp(terminate_buf)) goto terminate; /* set up long jump to end */
    /* override terminate handler */
    ami_eventover(ami_etterm, termevent, &oldtermevent); 

    ami_select(stdout, 2, 2);   /* move off the display buffer */
    /* set black on white text */
    ami_fcolor(stdout, ami_black);
    ami_bcolor(stdout, ami_white);
    printf("\f");
    ami_curvis(stdout, FALSE);
    prtban("Terminal mode screen test vs. 1.0");
    prtcen(ami_maxy(stdout), "Press return to continue");
    waitnext();

    /* *********************** Title set test ********************* */

    printf("\f");
    ami_title(stdout, "Terminal test");
    printf("Terminal window title set test.\n");
    printf("\n");
    printf("See of the title of the terminal window above has changed.\n");
    printf("\n");
    printf("Note that this will do nothing if the terminal is not windowed.\n");
    printf("Note also that changing the terminal title may not be\n");
    printf("implemented.\n");
    prtcen(ami_maxy(stdout), "Press return to continue");
    waitnext();

    /* *********************** Display screen parameters ********************* */

    printf("\f");   /* clear screen */
    printf("Screen size: x -> %d y -> %d\n\n", ami_maxx(stdout), ami_maxy(stdout));
    printf("Number of joysticks: %d\n", ami_joystick(stdout));
    for (i = 1; i <= ami_joystick(stdout); i++) {

        printf("\n");
        printf("Number of axes on joystick: %d is: %d\n", i,
            ami_joyaxis(stdout, i));
        printf("Number of buttons on joystick: %d is: %d\n", i,
            ami_joybutton(stdout, i));

    }
    printf("\n");
    printf("Number of mice: %d\n", ami_mouse(stdout));
    for (i = 1; i <= ami_mouse(stdout); i++) {

        printf("\n");
        printf("Number of buttons on mouse: %d is: %d\n", i,
            ami_mousebutton(stdout, i));

    }
    prtcen(ami_maxy(stdout), "Press return to continue");
    waitnext();

    /* ***************************** Timers test **************************** */

    printf("\f");
    timetest();
    printf("\n");
    frametest();
    prtcen(ami_maxy(stdout), "Press return to continue");
    waitnext();

    /* ********************* Cursor visible/invisible test ****************** */

    printf("\f");
    ami_curvis(stdout, 1);
    printf("Cursor should be [on ], press return ->");
    waitnext();
    ami_curvis(stdout, 0);
    printf("\rCursor should be [off], press return ->");
    waitnext();
    ami_curvis(stdout, 1);
    printf("\rCursor should be [on ], press return ->");
    waitnext();
    printf("\r                                       ");
    ami_curvis(stdout, 0);
    printf("\n");
    printf("\n");
    prtcen(ami_maxy(stdout), "Press return to continue");
    waitnext();

    /* *********************** Console standard text entry ********************* */

   printf("\f");
   ami_curvis(stdout, 1);
   printf("Standard input line enter test\n");
   printf("\n");
   printf("Enter text below. The line editor may have common line edit features\n");
   printf("installed, such as back up cursor, delete backwards/forwards, start\n");
   printf("and end of line, etc. Read the local system manual and try them.\n");
   printf("\n");
   line[0] = 0;
   i = 0;
   do {

        c = getchar();
        if (c != EOF && c != '\n') line[i++] = c;

    } while (c != EOF && c != '\n');
    line[i] = 0;
    printf("\n");
    printf("You typed:\n");
    printf("\n");
    printf("%s", line);
    prtcen(ami_maxy(stdout), "Press return to continue");
    waitnext();

    /* **************** Console standard text entry with offset ************* */

   printf("\f");
   ami_curvis(stdout, 1);
   printf("Standard input line enter with offset test\n");
   printf("\n");
   printf("Enter text below. The line editor may have common line edit features\n");
   printf("installed, such as back up cursor, delete backwards/forwards, start\n");
   printf("and end of line, etc. Read the local system manual and try them.\n");
   printf("\n");
   printf("===========>");
   line[0] = 0;
   i = 0;
   do {

        c = getchar();
        if (c != EOF && c != '\n') line[i++] = c;

    } while (c != EOF && c != '\n');
    line[i] = 0;
    printf("\n");
    printf("You typed:\n");
    printf("\n");
    printf("%s", line);
    prtcen(ami_maxy(stdout), "Press return to continue");
    waitnext();

    /* ************************* Test last line problem ************************ */

    printf("\f");
    ami_curvis(stdout, 0); /* remove cursor */
    ami_auto(stdout, FALSE); /* turn off auto scroll */
    prtcen(1, "Last line blank out test");
    ami_cursor(stdout, 1, 3);
    printf("If this terminal is not capable of showing the last character on\n");
    printf("the last line, the \"*\" character pointed to by the arrow below\n");
    printf("will not appear (probally blank). This should be noted for each\n");
    printf("of the following test patterns.\n");
    ami_cursor(stdout, 1, ami_maxy(stdout));
    for (i = 1; i <= ami_maxx(stdout)-2; i++) putchar('-');
    printf(">*");
    waitnext();

    /* ************************** Cursor movements test ************************ */

    /* First, do it with automatic scrolling on. The pattern will rely on scroll
       up, down, left wrap and right wrap working correctly. */
    printf("\f");
    ami_auto(stdout, TRUE);   /* set auto on */
    ami_curvis(stdout, 0);   /* remove cursor */
    /* top of left lower */
    ami_cursor(stdout, 1, ami_maxy(stdout));
    printf("\\/");
    /* top of right lower, bottom of left lower, and move it all up */
    ami_cursor(stdout, ami_maxx(stdout) - 1, ami_maxy(stdout));
    printf("\\//\\");
    /* finish right lower */
    ami_up(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_down(stdout);
    ami_down(stdout);
    printf("/\\");
    /* now move it back down */
    ami_home(stdout);
    ami_left(stdout);
    /* upper left hand cross */
    ami_cursor(stdout, 1, 1);
    printf("\\/");
    ami_cursor(stdout, ami_maxx(stdout), 1);
    ami_right(stdout);
    printf("/\\");
    /* upper right hand cross */
    ami_cursor(stdout, ami_maxx(stdout) - 1, 2);
    printf("/\\");
    ami_cursor(stdout, 1, 2);
    ami_left(stdout);
    ami_left(stdout);
    printf("\\/");
    /* test delete works */
    prtcen(1, "BARK!");
    ami_del(stdout);
    ami_del(stdout);
    ami_del(stdout);
    ami_del(stdout);
    ami_del(stdout);
    prtcen(ami_maxy(stdout)/2-1, "Cursor movements test, automatic scroll ON");
    prtcen(ami_maxy(stdout)/2+1, "Should be a double line X in each corner");
    waitnext();

    /* Now do it with automatic scrolling off. The pattern will rely on the
       ability of the cursor to go into "negative" space. */

    printf("\f");
    ami_auto(stdout, FALSE);   /* disable automatic screen scroll/wrap */
    /* upper left */
    ami_home(stdout);
    printf("\\/");
    ami_up(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_down(stdout);
    ami_down(stdout);
    ami_right(stdout);
    ami_right(stdout);
    printf("/\\");
    /* upper right */
    ami_cursor(stdout, ami_maxx(stdout)-1, 1);
    printf("\\/");
    ami_down(stdout);
    ami_del(stdout);
    ami_del(stdout);
    printf("/\\");
    /* lower left */
    ami_cursor(stdout, 1, ami_maxy(stdout));
    printf("/\\");
    ami_down(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_left(stdout);
    ami_up(stdout);
    ami_up(stdout);
    ami_right(stdout);
    printf("\\/");
    /* lower right */
    ami_cursor(stdout, ami_maxx(stdout), ami_maxy(stdout)-1);
    putchar('/');
    ami_left(stdout);
    ami_left(stdout);
    printf("\\");
    ami_down(stdout);
    ami_del(stdout);
    printf("/\\");
    prtcen(ami_maxy(stdout)/2-1,
           "Cursor movements test, automatic scroll OFF");
    prtcen(ami_maxy(stdout)/2+1, "Should be a double line X in each corner");
    waitnext();

    /* **************************** Scroll cursor test ************************* */

    printf("\f");
    ami_curvis(stdout, 1);
    prtcen(ami_maxy(stdout)/2, "Scroll cursor test, cursor should be here ->");
    ami_up(stdout);
    ami_scroll(stdout, 0, 1);
    waitnext();
    ami_curvis(stdout, 0);

    /* ******************************* Row ID test ***************************** */

    printf("\f");
    /* perform row id test */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++)   /* output characters */
        putchar(c);
        if (c != '9') c++;   /* next character */
        else c = '0';   /* start over */

    }
    prtban("Row ID test, all rows should be numbered");
    waitnext();

    /* *************************** Column ID test ***************************** */

    printf("\f");
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y); /* index start of line */
        c = '1';
        for (x = 1; x <= ami_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    prtban("Column ID test, all columns should be numbered");
    waitnext();

    /* ****************************** Fill test ******************************** */

    printf("\f");
    c = '\0';   /* initalize character value */
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) {

            if (c >= ' ' && c != '\177') putchar(c);
            else printf("\\");
            if (c != '\177') c++;   /* next character */
            else c = '\0';   /* start over */

        }

    }
    prtban("Fill test, all printable characters should appear");
    waitnext();

    /* **************************** Sidewinder test **************************** */

    printf("\f");
    /* perform sidewinder */
    x = 1;   /* set origin */
    y = 1;
    top = 1;   /* set borders */
    bottom = ami_maxy(stdout);
    lside = 2;
    rside = ami_maxx(stdout);
    direction = ddown;   /* start down */
    t1 = ami_maxx(stdout);
    t2 = ami_maxy(stdout);
    tc = 0;
    for (count = 1; count <= t1*t2; count++) {

        /* for all screen characters */
        ami_cursor(stdout, x, y); /* place character */
        putchar('*');
        tc++;
        if (tc >= 10) {

            waittime(1, 50); /* 5 milliseconds */
            tc = 0;

        }
        switch (direction) {

            case ddown:
                y++;   /* next */
                if (y == bottom) {  /* change */

                    direction = dright;
                    bottom--;

                }
                break;

            case dright:
                x++;   /* next */
                if (x == rside) {

                    direction = dup;
                    rside--;

                }
                break;


            case dup:
                y--;
                if (y == top) {

                    direction = dleft;
                    top++;

                }
                break;


            case dleft:
                x--;
                if (x == lside) {

                    direction = ddown;
                    lside++;

                }
                break;


        }

    }
    prtcen(ami_maxy(stdout) - 1, "                 ");
    prtcen(ami_maxy(stdout), " Sidewinder test ");
    waitnext();

    /* *************************** Bouncing ball test ************************** */

    printf("\f");
    x = 10;   /* set origin */
    y = 20;
    lx = 10;   /* set last */
    ly = 20;
    dx = -1;   /* set initial directions */
    dy = -1;
    for (count = 1; count <= 1000; count++) {

        ami_cursor(stdout, x, y);   /* place character */
        putchar('*');
        waittime(1, 100); /* wait for display, otherwise cannot see */
        ami_cursor(stdout, lx, ly);   /* place character */
        putchar(' ');
        lx = x;   /* set last */
        ly = y;
        x += dx;   /* find next x */
        y += dy;   /* find next y */
        tx = x;
        ty = y;
        if (x == 1 || tx == ami_maxx(stdout))   /* find new dir x */
        dx = -dx;
        if (y == 1 || ty == ami_maxy(stdout))   /* find new dir y */
        dy = -dy;
        /* slow this down */
        waittime(1, 100);

    }
    prtcen(ami_maxy(stdout)-1, "                    ");
    prtcen(ami_maxy(stdout), " Bouncing ball test ");
    waitnext();

    /* ************************ Attributes and colors test ******************** */

    printf("\f");
    if (ami_maxy(stdout) < 20)
    printf("Not enough lines for attributes test");
    else {

        ami_blink(stdout, 1);
        printf("Blinking text\n");
        ami_blink(stdout, 0);
        ami_reverse(stdout, 1);
        printf("Reversed text\n");
        ami_reverse(stdout, 0);
        ami_underline(stdout, 1);
        printf("Underlined text\n");
        ami_underline(stdout, 0);
        printf("Superscript ");
        ami_superscript(stdout, 1);
        printf("text\n");
        ami_superscript(stdout, 0);
        printf("Subscript ");
        ami_subscript(stdout, 1);
        printf("text\n");
        ami_subscript(stdout, 0);
        ami_italic(stdout, 1);
        printf("Italic text\n");
        ami_italic(stdout, 0);
        ami_bold(stdout, 1);
        printf("Bold text\n");
        ami_bold(stdout, 0);
        ami_strikeout(stdout, 1);
        printf("Strikeout text\n");
        ami_strikeout(stdout, 0);
        ami_standout(stdout, 1);
        printf("Standout text\n");
        ami_standout(stdout, 0);
        ami_fcolor(stdout, ami_red);
        printf("Red text\n");
        ami_fcolor(stdout, ami_green);
        printf("Green text\n");
        ami_fcolor(stdout, ami_blue);
        printf("Blue text\n");
        ami_fcolor(stdout, ami_cyan);
        printf("Cyan text\n");
        ami_fcolor(stdout, ami_yellow);
        printf("Yellow text\n");
        ami_fcolor(stdout, ami_magenta);
        printf("Magenta text\n");
        ami_fcolor(stdout, ami_black);
        ami_bcolor(stdout, ami_red);
        printf("Red background text\n");
        ami_bcolor(stdout, ami_green);
        printf("Green background text\n");
        ami_bcolor(stdout, ami_blue);
        printf("Blue background text\n");
        ami_bcolor(stdout, ami_cyan);
        printf("Cyan background text\n");
        ami_bcolor(stdout, ami_yellow);
        printf("Yellow background text\n");
        ami_bcolor(stdout, ami_magenta);
        printf("Magenta background text\n");
        ami_bcolor(stdout, ami_black);
        ami_fcolor(stdout, ami_white);
        printf("White on black text\n");
        ami_bcolor(stdout, ami_white);
        ami_fcolor(stdout, ami_black);        
        printf("Black on white text\n");
        ami_bcolor(stdout, ami_white);
        ami_fcolor(stdout, ami_black);
        prtcen(ami_maxy(stdout), "Attributes and colors test");

    }
    waitnext();

    /* **************************** RGB colors test ************************* */

    printf("\f");
    ami_auto(stdout, TRUE);
    prtcen(ami_maxy(stdout), "RGB colors test");
    ami_home(stdout);
    printf("The terminal may not implement 24 bit RGB colors for characters.\n");
    printf("\n");
    printf("In this case the colors will be the nearest primaries to the RGB\n");
    printf("Colors.\n");
    printf("\n");
    printf("Foreground      Background\n");
    j = 0;
    for (i = 0; i < sizeof(colormap)/sizeof(int); i++) {

        ami_bcolor(stdout, ami_white);
        fcolorp(colormap[i]);
        putchar('*');
        j++;
        if (j >= 13) {

            printf("   ");
            j = 0;
            while (j < 13) {

                ami_fcolor(stdout, ami_white);
                bcolorp(colormap[i-13+j+1]);
                putchar('*');
                j++;

            }
            j = 0;
            printf("\n");

        }

    }
    waitnext();
    ami_bcolor(stdout, ami_white);
    ami_fcolor(stdout, ami_black);

    /* ***************************** Scrolling test **************************** */

    printf("\f");
    /* fill screen with row order data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) putchar(c); /* output characters */
        if (c != '9') c++; /* next character */
        else c = '0'; /* start over */

    }
    for (y = 1; y <= ami_maxy(stdout); y++) { waittime(1, 200); ami_scroll(stdout, 0, 1); }
    prtcen(ami_maxy(stdout), "Scroll up");
    waitnext();
    printf("\f");
    /* fill screen with row order data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) putchar(c); /* output characters */
        if (c != '9') c++; /* next character */
        else c = '0';   /* start over */

    }
    for (y = 1; y <= ami_maxy(stdout); y++) { waittime(1, 200); ami_scroll(stdout, 0, -1); }
    prtcen(ami_maxy(stdout), "Scroll down");
    waitnext();
    printf("\f");
    /* fill screen with collumn order data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (x = 1; x <= ami_maxx(stdout); x++) { waittime(1, 200); ami_scroll(stdout, 1, 0); }
    prtcen(ami_maxy(stdout), "Scroll left");
    waitnext();
    printf("\f");
    /* fill screen with collumn order data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++;   /* next character */
            else c = '0';   /* start over */

        }

    }
    for (x = 1; x <= ami_maxx(stdout); x++) { waittime(1, 200); ami_scroll(stdout, -1, 0); }
    /* find minimum direction, x or y */
    if (x < y) minlen = x; else minlen = y;
    prtcen(ami_maxy(stdout), "Scroll right");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) {

            putchar(c);   /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (i = 1; i <= minlen; i++) { waittime(1, 200); ami_scroll(stdout, 1, 1); }
    prtcen(ami_maxy(stdout), "Scroll up/left");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (i = 1; i <= minlen; i++) { waittime(1, 200); ami_scroll(stdout, 1, -1); }
    prtcen(ami_maxy(stdout), "Scroll down/left");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (i = 1; i <= minlen; i++) { waittime(1, 200); ami_scroll(stdout, -1, 1); }
    prtcen(ami_maxy(stdout), "Scroll up/right");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++) {

            putchar(c);   /* output characters */
            if (c != '9') c++;   /* next character */
            else c = '0';   /* start over */

         }

    }
    for (i = 1; i <= minlen; i++) { waittime(1, 200); ami_scroll(stdout, -1, -1); }
    prtcen(ami_maxy(stdout), "Scroll down/right");
    waitnext();

    /* ******************************** Tab test ******************************* */

    /* Note tab test, besides testing tabbing, also tests offscreen draws
       (clipping). */

    printf("\f");
    ami_auto(stdout, FALSE); /* turn off auto */
    /* fill top with column order data */
    c = '1';
    for (x = 1; x <= ami_maxx(stdout); x++) {

        putchar(c); /* output characters */
        if (c != '9') c++; /* next character */
        else c = '0'; /* start over */

    }
    /* run tabbing */
    for (y = 1; y <= ami_maxy(stdout); y++) {

        for (i = 1; i <= y-1; i++) printf("\t");
        printf(">Tab %3d\n", y-1);

    }
    prtcen(ami_maxy(stdout), "Tabbing test");
    waitnext();

    /* ************************** Offscreen write test ************************* */

    putchar('\f');
    ami_auto(stdout, FALSE);
    /* right */
    x = ami_maxx(stdout)/2; /* find center screen */
    y = ami_maxy(stdout)/2;
    for (i = 0; i < ami_maxx(stdout)/2+200; i++) {

        ami_cursor(stdout, x+i, y);
        putchar('*');

    }
    /* down */
    for (i = 0; i < ami_maxy(stdout)/2+200; i++) {

        ami_cursor(stdout, x, y+i);
        putchar('*');

    }
    /* left */
    for (i = 0; i < ami_maxx(stdout)/2+200; i++) {

        ami_cursor(stdout, x-i, y);
        putchar('*');

    }
    /* up */
    for (i = 0; i < ami_maxy(stdout)/2+200; i++) {

        ami_cursor(stdout, x, y-i);
        putchar('*');

    }
    ami_home(stdout);
    printf("Offscreen write test\n");
    printf("\n");
    printf("There should be a cross centered onscreen.\n");
    printf("The display should not scroll.\n");
    waitnext();

    /* ************************** Offscreen scroll test ********************* */

    putchar('\f');
    ami_auto(stdout, FALSE);
    printf("Offscreen scroll test\n");
    printf("\n");
    printf("The line numbers will count screen lines.\n");
    printf("The display should not scroll.\n");
    printf("\n");
    for (y = 6; y < ami_maxy(stdout)+200; y++) printf("Line %d\n", y);
    waitnext();

    /* ************************** Buffer switching test ************************ */

    printf("\f");
    ami_curvis(stdout, FALSE);
    for (b = 2; b <= 10; b++) {  /* prepare buffers */

        ami_select(stdout, b, 2);   /* select buffer */
        /* write a shinking box pattern */
        box(b - 1, b-1, ami_maxx(stdout)-(b- 2), ami_maxy(stdout)-(b-2), '*');
        prtcen(ami_maxy(stdout), "Buffer switching test");

    }
    for (i = 1; i <= 30; i++) /* flip buffers */
        for (b = 2; b <= 10; b++) { waittime(1, 300); ami_select(stdout, 2, b); }
    ami_select(stdout, 2, 2);   /* restore buffer select */

    /* **************************** Writethrough test ************************** */

    printf("\f");
    prtcen(ami_maxy(stdout), "File writethrough test");
    ami_home(stdout);
    tf = fopen(tf_NAME, "w");
    if (tf == NULL) {

        fprintf(stderr, "*** Cannot open file: %s\n", tf_NAME);
        exit(1);

    }
    fprintf(tf, "This is a test file\n");
    fclose(tf);
    tf = fopen(tf_NAME, "r");
    if (tf == NULL) {

        fprintf(stderr, "*** File not found: %s\n", tf_NAME);
        exit(1);

    }
// find out why getc() does not work, believe limitation in our stdio.
    c = fgetc(tf);
    while (c != '\n' && c != EOF) {

        putchar(c);
        c = fgetc(tf);

    }
    printf("\n");
    printf("\n");
    printf("s/b\n");
    printf("\n");
    printf("This is a test file\n");
    waitnext();

    /* **************************** buffer follow test ************************* */

    printf("\f");
    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    box(1, 1, ami_maxx(stdout), ami_maxy(stdout), '*');
    prtcen(ami_maxy(stdout), " Buffer follow test ");
    ami_cursor(stdout, 3, 3);
    printf("Resize the window, the frame should stay at the original size\n");
    waitnext();
    printf("\f");
    box(1, 1, ami_maxx(stdout), ami_maxy(stdout), '*');
    prtcen(ami_maxy(stdout), " Buffer follow test ");
    ami_cursor(stdout, 3, 3);
    printf("Resize the window, the frame should follow the window\n");
    x = ami_maxx(stdout);
    y = ami_maxy(stdout);
    do { 

        ami_event(stdin, &er);
        if (er.etype == ami_etresize) {

            box(1, 1, x, y, ' ');
            ami_sizbuf(stdout, er.rszx, er.rszy);
            box(1, 1, ami_maxx(stdout), ami_maxy(stdout), '*');
            x = ami_maxx(stdout);
            y = ami_maxy(stdout);
            prtcen(ami_maxy(stdout), " Buffer follow test ");
            ami_cursor(stdout, 3, 3);
            printf("Resize the window, the frame should follow the window\n");

        }

    } while (er.etype != ami_etenter);
    ami_auto(stdout, TRUE);
    ami_curvis(stdout, FALSE);

    /* **************************** Focus and hover test *********************** */

    printf("\f");
    ami_curvis(stdout, FALSE);
    prtcen(ami_maxy(stdout), "Focus and hover test");
    ami_home(stdout);
    printf("Click the window, then other windows and watch the focus box.\n");
    printf("\n");
    printf("Roll over the window, then outside the window, and watch the hover box.\n");
    printf("\n");
    printf("If focus is not supported, it is always on");
    printf("\n");
    printf("Note with simulated hover, assert is immedate, but deassert is\n");
    printf("after about 5 seconds.\n");
    box(10, 10, 30, 14, '#');
    ami_cursor(stdout, 17, 12);
    printf("Focus");
    box(40, 10, 60, 14, '#');
    ami_cursor(stdout, 47, 12);
    printf("hover");
    do { 

        ami_event(stdin, &er);
        if (er.etype == ami_etfocus) box(10, 10, 30, 14, '#');
        else if (er.etype == ami_etnofocus) box(10, 10, 30, 14, '*');
        if (er.etype == ami_ethover) box(40, 10, 60, 14, '#');
        else if (er.etype == ami_etnohover) box(40, 10, 60, 14, '*');

    } while (er.etype != ami_etenter);
    ami_curvis(stdout, TRUE);

    /* ******************************* Threading test ************************** */

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    printf("\f");
    printf("The left and right figures are run on different threads\n");
    prtcen(ami_maxy(stdout), "Threading test");
    thdstp = FALSE;
    ethdstp = FALSE;
    ln = ami_initlock();
    sn = ami_initsig();
    esn = ami_initsig();
    timeout1 = ami_initsig();
    timeout2 = ami_initsig();
    tn = ami_newthread(thread);
    etn = ami_newthread(eventthread);
    ami_timer(stdout, 1, SECOND/10, TRUE);
    ami_timer(stdout, 2, SECOND/10, TRUE);
    x = ami_maxx(stdout)/3;
    y = ami_maxy(stdout)/2;
    for (j = 0; j < 30; j++) {

        i = 1;
        for (i = 0; i < 10; i++) {

            ami_lock(ln);
            box(x-i, y-i, x+i, y+i, '*');
            ami_waitsig(ln, timeout1);
            box(x-i, y-i, x+i, y+i, ' ');
            ami_unlock(ln);
            i++;

        }

    }
    /* stop subthread */
    ami_lock(ln);
    thdstp = TRUE;
    ami_waitsig(ln, sn);
    ami_unlock(ln);
    /* stop event thread */
    ami_lock(ln);
    ethdstp = TRUE;
    ami_waitsig(ln, esn);
    ami_unlock(ln);
    ami_killtimer(stdout, 1);
    ami_killtimer(stdout, 2);
    ami_cursor(stdout, 1, 3);
    ami_deinitlock(ln);
    printf("Test complete!\n");
    waitnext();
    ami_auto(stdout, TRUE);
    ami_curvis(stdout, TRUE);

    /* ****************************** Joystick test **************************** */

    if (ami_joystick(stdout) > 0) {  /* joystick test */

        printf("\f");
        ami_curvis(stdout, FALSE);
        prtcen(1, "Move the joystick(s) X, Y and Z, and hit buttons");
        prtcen(ami_maxy(stdout), "Joystick test");
        do {   /* gather joystick events */

            /* we do up to 4 joysticks */
            ami_event(stdin, &er);
            if (er.etype == ami_etjoymov) {  /* joystick movement */

                ami_cursor(stdout, 1, 3);
                printf("joystick: %3d x: %11d y: %11d z: %11d\n",
                       er.mjoyn, er.joypx, er.joypy, er.joypz);
                printf("              4: %11d 5: %11d 6: %11d\n",
                       er.joyp4, er.joyp5, er.joyp6);
                plotjoy(5, er.joypx);
                plotjoy(6, er.joypy);
                plotjoy(7, er.joypz);
                plotjoy(8, er.joyp4);
                plotjoy(9, er.joyp5);
                plotjoy(10, er.joyp6);

            } else if (er.etype == ami_etjoyba) {  /* joystick button assert */

                if (er.ajoyn == 1) {  /* joystick 1 */

                    ami_cursor(stdout, 1, 18);
                    printf("joystick: %d button assert:   %2d",
                           er.ajoyn, er.ajoybn);

                } else if (er.ajoyn == 2) {  /* joystick 2 */

                    ami_cursor(stdout, 1, 19);
                    printf("joystick: %d button assert:   %2d",
                           er.ajoyn, er.ajoybn);

                } else if (er.ajoyn == 3) {  /* joystick 3 */

                    ami_cursor(stdout, 1, 20);
                    printf("joystick: %d button assert:   %2d",
                           er.ajoyn, er.ajoybn);

                } else if (er.ajoyn == 4) {  /* joystick 4 */

                    ami_cursor(stdout, 1, 21);
                    printf("joystick: %d button assert:   %2d",
                           er.ajoyn, er.ajoybn);

                }

            } else if (er.etype == ami_etjoybd) {  /* joystick button deassert */

                if (er.djoyn == 1) {  /* joystick 1 */

                    ami_cursor(stdout, 1, 18);
                    printf("joystick: %d button deassert: %2d",
                           er.djoyn, er.djoybn);

                } else if (er.djoyn == 2) {  /* joystick 2 */

                    ami_cursor(stdout, 1, 19);
                    printf("joystick: %d button deassert: %2d",
                           er.djoyn, er.djoybn);

                } else if (er.djoyn == 3) {  /* joystick 3 */

                    ami_cursor(stdout, 1, 20);
                    printf("joystick: %d button deassert: %2d",
                           er.djoyn, er.djoybn);

                } else if (er.djoyn == 4) {  /* joystick 4 */

                    ami_cursor(stdout, 1, 21);
                    printf("joystick: %d button deassert: %2d",
                           er.djoyn, er.djoybn);

                }

            }

        } while (er.etype != ami_etenter);
        ami_curvis(stdout, TRUE);

    }

    /* **************************** Mouse test ********************************* */

    if (ami_mouse(stdin) > 0) {  /* mouse test */

        printf("\f");
        ami_auto(stdout, FALSE);
        ami_curvis(stdout, FALSE);
        prtcen(1, "Move the mouse, and hit buttons");
        prtcen(ami_maxy(stdout), "Mouse test");
        do { /* gather mouse events */

            /* we only one mouse, all mice equate to that (multiple controls) */
            ami_event(stdin, &er);
            if (er.etype == ami_etmoumov) {

                ami_cursor(stdout, x, y);
                printf("          \n");
                ami_cursor(stdout, er.moupx, er.moupy);
                x = ami_curx(stdout);
                y = ami_cury(stdout);
                printf("<- Mouse %d\n", er.mmoun);
                prtcen(1, "Move the mouse, and hit buttons");
                prtcen(ami_maxy(stdout), "Mouse test");

            }
            /* blank out button status line */
            ami_cursor(stdout, 1, ami_maxy(stdout)-2);
            for (i = 1; i <= ami_maxx(stdout); i++) putchar(' ');
            if (er.etype == ami_etmouba) {  /* mouse button assert */

                ami_cursor(stdout, 1, ami_maxy(stdout)-2);
                printf("Mouse button assert, mouse: %d button: %d\n",
                       er.amoun, er.amoubn);
                prtcen(1, "Move the mouse, and hit buttons");
                prtcen(ami_maxy(stdout), "Mouse test");

            }
            if (er.etype == ami_etmoubd) {  /* mouse button assert */

                ami_cursor(stdout, 1, ami_maxy(stdout) - 2);
                printf("Mouse button deassert, mouse: %d button: %d\n",
                       er.dmoun, er.dmoubn);
                prtcen(1, "Move the mouse, and hit buttons");
                prtcen(ami_maxy(stdout), "Mouse test");

            }

        } while (er.etype != ami_etenter);
        ami_auto(stdout, TRUE);
        ami_curvis(stdout, TRUE);

    }

    /* ************************* Event vector test  **************************** */

    printf("\f");
    prtcen(ami_maxy(stdout), "Event vector test");
    ami_home(stdout);
    /* since there is no facility to remove vectors, these tests have to be done
       in order. */

    eventflag1 = FALSE;
    ami_eventover(ami_etframe, event_vector_1, &oeh1);
    ami_frametimer(stdout, TRUE);
    printf("Waiting for frame event, hit return to continue\n");
    do { ami_event(stdin, &er); }
    while (er.etype != ami_etframe && er.etype != ami_etenter);
    if (er.etype == ami_etframe) printf("*** Event bled through! ***\n");
    if (eventflag1) printf("Fanout event passes\n");
    else printf("*** Fanout event fails! ***\n");
    eventflag2 = FALSE;
    ami_eventsover(event_vector_2, &oeh1);
    printf("Waiting for frame event, hit return to continue\n");
    do { ami_event(stdin, &er); }
    while (er.etype != ami_etframe && er.etype != ami_etenter);
    if (er.etype == ami_etframe) printf("*** Event bled through! ***\n");
    if (eventflag2) printf("Master event passes\n");
    else printf("*** Master event fails! ***\n");

    ami_frametimer(stdout, FALSE);
    waitnext();

    /* ********************** Character write speed test *********************** */

    printf("\f");
    ami_curvis(stdout, FALSE);
    clk = ami_clock();   /* get reference time */
    c = '\0';   /* initalize character value */
    cnt = 0;   /* clear character count */
    maxx = ami_maxx(stdout);
    maxy = ami_maxy(stdout);
    for (y = 1; y <= maxy; y++) {

        ami_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= maxx; x++) {

            if (c >= ' ' && c != '\177') putchar(c); else putchar('\\');
            if (c != '\177') c++; /* next character */
            else c = '\0'; /* start over */
            cnt++;   /* count characters */

        }

    }
    clk = ami_elapsed(clk);   /* find elapsed time */
    benchtab[bncharw].iter = cnt;
    benchtab[bncharw].time = clk;
    printf("\f");
    printf("Character write speed %f seconds, per character %f\n",
           (float)clk*0.0001, (float)clk/cnt*0.0001);
    waitnext();

    /* ************************** Scrolling speed test ************************* */

    printf("\f");
    /* fill screen so we aren't moving blanks (could be optimized) */
    c = '1';
    for (y = 1; y <= ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= ami_maxx(stdout); x++)   /* output characters */
        putchar(c);
        if (c != '9') c++; /* next character */
        else c = '0'; /* start over */

    }
    prtban("Scrolling speed test");
    clk = ami_clock(); /* get reference time */
    cnt = 0; /* clear count */
    for (i = 1; i <= 100; i++) { /* scroll various directions */

        ami_scroll(stdout, 0, -1); /* up */
        ami_scroll(stdout, -1, 0); /* left */
        ami_scroll(stdout, 0, 1); /* down */
        ami_scroll(stdout, 0, 1); /* down */
        ami_scroll(stdout, 1, 0); /* right */
        ami_scroll(stdout, 1, 0); /* right */
        ami_scroll(stdout, 0, -1); /* up */
        ami_scroll(stdout, 0, -1); /* up */
        ami_scroll(stdout, -1, 0); /* left */
        ami_scroll(stdout, 0, 1); /* down */
        ami_scroll(stdout, -1, -1); /* up/left */
        ami_scroll(stdout, 1, 1); /* down/right */
        ami_scroll(stdout, 1, 1); /* down/right */
        ami_scroll(stdout, -1, -1); /* up/left */
        ami_scroll(stdout, 1, -1); /* up/right */
        ami_scroll(stdout, -1, 1); /* down/left */
        ami_scroll(stdout, -1, 1); /* down/left */
        ami_scroll(stdout, 1, -1); /* up/right */
        cnt += 19; /* count all scrolls */

    }
    clk = ami_elapsed(clk);   /* find elapsed time */
    benchtab[bnscroll].iter = cnt;
    benchtab[bnscroll].time = clk;
    printf("\f");
    printf("Scrolling speed: %f seconds, per scroll %f\n",
           (float)clk*0.0001, (float)clk/cnt*0.0001);
    waitnext();

    /* ************************** Buffer flip speed test ************************* */

    printf("\f");
    cnt = 0;   /* clear count */

    for (b = 2; b <= 10; b++) {  /* prepare buffers */

        ami_select(stdout, b, 2);   /* select buffer */
        /* write a shinking box pattern */
        box(b - 1, b - 1, ami_maxx(stdout) - b + 2, ami_maxy(stdout) - b + 2, '*');

    }

    clk = ami_clock();   /* get reference time */
    for (i = 1; i <= 100; i++) /* flip buffers */
    for (b = 2; b <= 10; b++) {

        ami_select(stdout, 2, b);
        cnt++;

    }
    clk = ami_elapsed(clk);   /* find elapsed time */
    benchtab[bnbuffer].iter = cnt;
    benchtab[bnbuffer].time = clk;
    ami_select(stdout, 2, 2);   /* restore buffer select */
    printf("\f");
    printf("Buffer switch speed: %f average seconds per switch %f\n",
           (float)clk*0.0001, (float)clk/cnt*0.0001);
    waitnext();

terminate: /* terminate */

    /* test complete */
    ami_select(stdout, 1, 1); /* back to display buffer */
    ami_curvis(stdout, 1);     /* restore cursor */
    ami_auto(stdout, 1);   /* enable automatic screen wrap */
    if (tf != NULL) fclose(tf);
    printf("\n");
    printf("Test complete\n");
    printf("\n");

    /* output table */

    printf("\n");
    printf("Benchmark table\n");
    printf("\n");
    printf("Type                   Seconds  Per fig\n");
    printf("--------------------------------------------\n");
    for (bi = bncharw; bi <= bnbuffer; bi++) {

        switch (bi) { /* benchmark type */

            case bncharw:  printf("character write speed "); break;
            case bnscroll: printf("Scroll speed          "); break;
            case bnbuffer: printf("Buffer flip speed     "); break;

        };
        printf("%6.2f", benchtab[bi].time*0.0001);
        printf("    ");
        printf("%f", benchtab[bi].time*0.0001/benchtab[bi].iter);
        printf("\n");

    }
    printf("\n");

}
