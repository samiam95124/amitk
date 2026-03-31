/********************************************************************************

Program to bounce animated balls around screen

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <graphics.h>

#define MAXBALL   10
#define REPRATE   5   /* number of moves per frame, should be low */

typedef struct balrec { /* ball data record */

    int      x, y;   /* current position */
    int      lx, ly; /* last position */
    int      xd, yd; /* deltas */
    ami_color c;      /* ami_color */
   
} balrec;
   
int    cd;              /* current display flip select */
balrec baltbl[MAXBALL]; /* ball data table */
int    i;               /* index for table */
int    nx, ny;          /* temp coordinates holders */
int    rc;              /* repetition counter */
int    ballsize;        /* size of ball onscreen */
int    halfball;        /* half size of ball */

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
   
/* Find random number between 0 and N. */

static int randn(int limit)

{

    return ((long)limit+1)*rand()/RAND_MAX;

}

int main(void)

{

    ballsize = ami_maxyg(stdout)/5; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    /* initialize ball data */
    for (i = 0; i < MAXBALL; i++) {

        baltbl[i].x = randn(ami_maxxg(stdout)-1-ballsize)+halfball+1;
        baltbl[i].y = randn(ami_maxyg(stdout)-1-ballsize)+halfball+1;
        if (randn(1)) baltbl[i].xd = +1; else baltbl[i].xd = -1;
        if (randn(1)) baltbl[i].yd = +1; else baltbl[i].yd = -1;
        baltbl[i].lx = baltbl[i].x; /* set last position to same */
        baltbl[i].ly = baltbl[i].y;
        /* set random ami_color */
        baltbl[i].c = randn(ami_magenta-ami_red)+ami_red;

    }
    ami_curvis(stdout, FALSE); /* turn off cursor */
    cd = FALSE; /* set 1st display */
    rc = 0; /* count reps */
        /* start frame timer for 60 cycle refresh */
    ami_frametimer(stdout, TRUE);
    while (TRUE) {

        /* select display and update surfaces */
        ami_select(stdout, !cd+1, cd+1);
        ami_fover(stdout); /* set overwrite */
        /* erase balls at old positions */
        for (i = 0; i < MAXBALL; i++)
            drawball(ami_white, baltbl[i].lx, baltbl[i].ly);
        ami_fxor(stdout); /* set xor mode */
        for (i = 0; i < MAXBALL; i++) { /* process balls */

            baltbl[i].lx = baltbl[i].x; /* save last position */
            baltbl[i].ly = baltbl[i].y;
            nx = baltbl[i].x+baltbl[i].xd; /* trial move ball */
            ny = baltbl[i].y+baltbl[i].yd;
            /* check out of bounds and reverse direction */
            if (nx < halfball || nx > ami_maxxg(stdout)-halfball+1)
                baltbl[i].xd = -baltbl[i].xd;
            if (ny < halfball || ny > ami_maxyg(stdout)-halfball+1)
                baltbl[i].yd = -baltbl[i].yd;
            baltbl[i].x = baltbl[i].x+baltbl[i].xd; /* move ball */
            baltbl[i].y = baltbl[i].y+baltbl[i].yd;
            /* place ball at new position */
            drawball(baltbl[i].c, baltbl[i].x, baltbl[i].y);

        }
        cd = !cd; /* flip display and update surfaces */
        rc = rc+1; /* count reps */
        if (rc >= REPRATE) {

            if (chkbrk()) goto terminate; /* check complete */
            rc = 0; /* clear rep counter */

        }

    }

    terminate:

    ami_curvis(stdout, TRUE);

}
