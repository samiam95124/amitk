/********************************************************************************

Program to draw random lines on screen

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <localdefs.h>
#include <graphics.h>

#define FRAMETIME 156 /* time between frames, 60 cycle refresh */

static int waitframe(void)

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

/*******************************************************************************

Find random number

Find random number between 0 and N.

*******************************************************************************/

static int randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

int main(void)

{

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    while (TRUE) {

        ami_fcolor(stdout, randn(ami_magenta+1-ami_red)+ami_red);
        ami_line(stdout, randn(ami_maxxg(stdout)-1)+1, randn(ami_maxyg(stdout)-1)+1,
                        randn(ami_maxxg(stdout)-1)+1, randn(ami_maxyg(stdout)-1)+1);
        if (waitframe()) goto terminate;

    }

    terminate:

    ami_auto(stdout, TRUE);
    ami_curvis(stdout, TRUE);

}
