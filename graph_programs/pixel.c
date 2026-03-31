/**//***************************************************************************

                                 Pixel dazzler

Continually xors a pixel onto the screen, eventually filling it with black, then
reversing to white, and cycles forever.

*******************************************************************************/

#include <setjmp.h>
#include <stdio.h>
#include <localdefs.h>
#include <graphics.h>

#define ACCEL 1000

static int x, y, xd, yd, i;

static jmp_buf terminate_buf;

static void chkbrk(void)

{

    ami_evtrec er; /* event record */

    /* This is a dirty trick with PA. We set minimum time and check for
       user break because we want as much CPU time as possible to draw.
       A better solution would be to use another thread and set a flag for
       cancel. */
    ami_timer(stdout, 1, 1, FALSE);
    do { ami_event(stdin, &er); }
    while (er.etype != ami_ettim && er.etype != ami_etterm);
    if (er.etype == ami_etterm) { longjmp(terminate_buf, 1); }

}

int main(void)

{

    if (setjmp(terminate_buf)) goto terminate;

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    ami_fcolor(stdout, ami_white);
    ami_fxor(stdout);
    x = 1;
    y = 1;
    xd = +1;
    yd = +1;
    while (TRUE) {

        for (i = 1; i <= ACCEL; i++) {

            ami_setpixel(stdout, x, y);
            x = x+xd;
            y = y+yd;
            if (x == 1 || x == ami_maxxg(stdout)) xd = -xd;
            if (y == 1 || y == ami_maxyg(stdout)) yd = -yd;

        }
        chkbrk();

    }

terminate: /* terminate */

    ami_auto(stdout, TRUE);
    ami_curvis(stdout, TRUE);

}
