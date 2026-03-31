/********************************************************************************

Program to draw random lines on screen

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <graphics.h>

#define FRAMETIME   156 /* time between frames, 60 cycle refresh */
#define ACCEL       5
#define COLORCHANGE 300

static int x1, y1, xd1, yd1, i, x2, y2, xd2, yd2, lx1, ly1, lx2, ly2;
static int cc; /* color counter */
static ami_color clr;

/* check user break */

static int chkbrk(void)

{

    ami_evtrec er; /* event record */
    int cancel;

    cancel = FALSE;
    ami_timer(stdout, 1, FRAMETIME, FALSE);
    do { ami_event(stdin, &er); }
    while (er.etype != ami_ettim && er.etype != ami_etterm);
    if (er.etype == ami_etterm) cancel = TRUE;

    return (cancel);

}

/* Find random number between 0 and N. */

static int randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

int main(void)

{

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    x1 = ami_maxxg(stdout)/4+10;
    y1 = 1;
    lx1 = x1;
    ly1 = y1;
    xd1 = -1;
    yd1 = +1;
    x2 = ami_maxxg(stdout)-(ami_maxxg(stdout) / 4);
    y2 = ami_maxyg(stdout);
    lx2 = x2;
    ly2 = y2;
    xd2 = -1;
    yd2 = -1;
    cc = 1;
    clr = randn(ami_magenta+1-ami_red)+ami_red;
    while (TRUE) {

        for (i = 1; i <= ACCEL; i++) {

            ami_fcolor(stdout, ami_white);
            ami_line(stdout, lx1, ly1, lx2, ly2);
            lx1 = x1;
            ly1 = y1;
            lx2 = x2;
            ly2 = y2;
            x1 = x1+xd1;
            y1 = y1+yd1;
            if (x1 == 1 || x1 == ami_maxxg(stdout)) xd1 = -xd1;
            if (y1 == 1 || y1 == ami_maxyg(stdout)) yd1 = -yd1;
            x2 = x2+xd2;
            y2 = y2+yd2;
            if (x2 == 1 || x2 == ami_maxxg(stdout)) xd2 = -xd2;
            if (y2 == 1 || y2 == ami_maxyg(stdout)) yd2 = -yd2;
            ami_fcolor(stdout, clr);
            ami_line(stdout, x1, y1, x2, y2);
            cc = cc+1;
            if (cc >= COLORCHANGE) {

                cc = 1;
                clr = randn(ami_magenta+1-ami_red)+ami_red;

            }

        }
        if (chkbrk()) goto terminate;

    }

   terminate:

   ami_auto(stdout, TRUE);
   ami_curvis(stdout, TRUE);

}
