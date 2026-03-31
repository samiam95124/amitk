/********************************************************************************

Place random balls

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <graphics.h>

int ballsize;
int halfball;

int waitframe(void)

{

    ami_evtrec er;
    int cancel;

    cancel = FALSE;
    do { ami_event(stdin, &er); }
    while (er.etype != ami_etframe && er.etype != ami_etterm);
    if (er.etype == ami_etterm) cancel = TRUE;

    return (cancel);

}

void drawball(ami_color c, int  x, int y)

{

   ami_fcolor(stdout, c); /* set ami_color */
   ami_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);

}

/* Find random number between 0 and N. */

static int randn(int limit)

{

    return ((long)limit+1)*rand()/RAND_MAX;

}

int main(void)

{

    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    ballsize = ami_maxyg(stdout)/10; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    ami_frametimer(stdout, TRUE); /* start frame timer */
    while (TRUE) {

        drawball(randn(ami_magenta+1-ami_red)+ami_red,
                 randn(ami_maxxg(stdout)-1-ballsize)+halfball+1,
                 randn(ami_maxyg(stdout)-1-ballsize)+halfball+1);
        if (waitframe()) goto terminate; /* wait for a frametime */

   }

   terminate:

   ami_auto(stdout, TRUE);
   ami_curvis(stdout, TRUE);

}
