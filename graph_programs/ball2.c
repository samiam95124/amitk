/********************************************************************************

Program to bounce animated ball around screen

Same as ball1.c, but this one draws to odd/even buffers and flips them to
demonstrate smooth animation.

********************************************************************************/

#include <stdio.h>
#include <localdefs.h>
#include <graphics.h>

#define BALLACCEL 5 /* ball acceleration */

int       x, y;
int       nx, ny;
int       lx, ly;
int       xd, yd;
ami_evtrec er;
int       tc;
int       cd; /* current display flip select */
int       ballsize;
int       halfball;

int chkbrk(void)

{

    ami_evtrec er;
    int cancel;

    cancel = FALSE;
    do { ami_event(stdin, &er); }
    while (er.etype != ami_etframe && er.etype != ami_etterm);
    if (er.etype == ami_etterm) cancel = TRUE;

    return (cancel);

}

void drawball(ami_color c, int x, int y)

{

    ami_fcolor(stdout, c); /* set color */
    ami_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);

}

int main(void)

{

    ami_curvis(stdout, FALSE); /* turn off cursor */
    ballsize = ami_maxyg(stdout)/10; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    x = halfball; /* set initial ball location */
    y = halfball;
    xd = +1; /* set movements */
    yd = +1;
    lx = x; /* set last position to same */
    ly = y;
    cd = FALSE; /* set 1st display */
    ami_frametimer(stdout, TRUE); /* set frame timer */
    drawball(ami_green, x, y); /* place ball at first position */
    while (TRUE) {

        /* select display and update surfaces */
        ami_select(stdout, !cd+1, cd+1);
        drawball(ami_white, lx, ly); /* erase ball at old position */
        lx = x; /* save last position */
        ly = y;
        for (tc = 1; tc <= BALLACCEL; tc++) { /* move ball */

            nx = x+xd; /* trial move ball */
            ny = y+yd;
            /* check out of bounds and reverse direction */
            if (nx < halfball || nx > ami_maxxg(stdout)-halfball+1) xd = -xd;
            if (ny < halfball || ny > ami_maxyg(stdout)-halfball+1) yd = -yd;
            x = x+xd; /* move ball */
            y = y+yd;

        }
        drawball(ami_green, x, y); /* place ball at new position */
        cd = !cd; /* flip display and update surfaces */
        if (chkbrk()) goto terminate; /* wait */

    }

    terminate:

    ami_curvis(stdout, TRUE); /* turn on cursor */

}
