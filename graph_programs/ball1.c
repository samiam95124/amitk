/********************************************************************************

Program to bounce animated ball around screen

********************************************************************************/

#include <stdio.h>
#include <localdefs.h>
#include <graphics.h>

#define BALLACCEL 5   /* ball acceleration */

int       x, y;
int       nx, ny;
int       xd, yd;
ami_evtrec er;
int       tc;
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

int main(void)

{

    ami_curvis(stdout, FALSE); /* turn off cursor */
    ballsize = ami_maxyg(stdout)/10; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    x = halfball; /* set initial ball location */
    y = halfball;
    xd = +1; /* set movements */
    yd = +1;
    ami_frametimer(stdout, TRUE); /* start frame timer */
    while (TRUE) {

        /* place ball */
        ami_fcolor(stdout, ami_green);
        ami_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);
        if (chkbrk()) goto terminate; /* wait */
        /* erase ball */
        ami_fcolor(stdout, ami_white);
        ami_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);
        for (tc = 1; tc <= BALLACCEL; tc++) { /* move ball */

            nx = x+xd; /* trial move ball */
            ny = y+yd;
            /* check out of bounds && reverse direction */
            if (nx < halfball || nx > ami_maxxg(stdout)-halfball+1) xd = -xd;
            if (ny < halfball || ny > ami_maxyg(stdout)-halfball+1) yd = -yd;
            x = x+xd; /* move ball */
            y = y+yd;

        }

    }

    terminate: /* terminate */

    ami_curvis(stdout, TRUE); /* turn on cursor */

}
