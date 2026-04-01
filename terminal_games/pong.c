/*******************************************************************************
*                                                                             *
*                                 PONG GAME                                   *
*                                                                             *
*                       COPYRIGHT (C) 1997 S. A. FRANCO                        *
*                                                                             *
* Plays pong in text mode.                                                    *
*                                                                             *
*******************************************************************************/

#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* enable or disable sound package */
#ifndef SOUND
#define SOUND 1
#endif

#include <localdefs.h>
#if SOUND
#include <sound.h>
#endif
#include <terminal.h>

#define MOVTIM 400    /* ball move time (1/25) sec */
#define NEWBAL (25*2) /* wait for new ball time, 1 sec (in ball move units) */
#define BNCENOTE 1    /* time to play bounce note */
#define WALLNOTE (AMI_NOTE_D+AMI_OCTAVE_6) /* note to play off WALL */
#define FAILTIME 4                /* note to play on failure */
#define FAILNOTE (AMI_NOTE_C+AMI_OCTAVE_4) /* note to play on fail */

int       padx;   /* paddle position x */
int       ballx;  /* ball position x */
int       bally;  /* ball position y */
int       bdx;    /* ball direction x */
int       bdy;    /* ball direction y */
int       bsx;    /* ball position save x */
int       bsy;    /* ball position save y */
int       baltim; /* ball start timer */
ami_evtrec er;     /* event record */
int       jchr;   /* number of characters to joystick movement */
int       score;  /* score */
int       nottim; /* bounce note timer */
int       failtimer; /* fail note timer */

/*******************************************************************************

Write string to screen

Writes a sgtring to the indicated position on the screen.

********************************************************************************/

void writexy(int x, int y, /* position to write to */
             char* s)      /* char* to write */

{

   ami_cursor(stdout, x, y); /* position cursor */
   puts(s); /* output string */

}

/*******************************************************************************

Write centered string

Writes a string that is centered on the line given. Returns the
starting position of the string.

********************************************************************************/

void wrtcen(int   y,   /* y position of string */
            char* s,   /* char* to write */
            int*  off) /* returns char* offset */

{

   *off = ami_maxx(stdout)/2-strlen(s)/2;
   writexy(*off, y, s); /* write out contents */

}

/*******************************************************************************

Draw screen

Draws a new screen, with borders.

********************************************************************************/

void drwscn(void)

{

    int x, y; /* screen indexes */

    putchar('\n'); /* clear screen */
    /* draw borders */
    for (x = 1; x <= ami_maxx(stdout); x++) writexy(x, 1, "*");
    for (x = 1; x <= ami_maxx(stdout); x++) writexy(x, ami_maxy(stdout), "*");
    for (y = 1; y <= ami_maxy(stdout); y++) writexy(1, y, "*");
    for (y = 1; y <= ami_maxy(stdout); y++) writexy(ami_maxx(stdout), y, "*");
    wrtcen(ami_maxy(stdout), " PONG VS. 1.0 ", &x);

}

/*******************************************************************************

Set new paddle position

Places the paddle at the given position.

********************************************************************************/

void padpos(int x)

{

    if (x < 4) x = 4; /* clip to ends */
    if (x > ami_maxx(stdout)-4) x = ami_maxx(stdout)-4;
    writexy(padx-3, ami_maxy(stdout)-1, "       "); /* blank paddle */
    padx = x; /* move right */
    writexy(padx-3, ami_maxy(stdout)-1, "======="); /* place paddle */

}

int main(void)

{

    nottim = 0; /* clear bounce note timer */
    failtimer = 0; /* clear fail timer */
    #if SOUND
    ami_opensynthout(AMI_SYNTH_OUT); /* open synthesizer */
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_LEAD_1_SQUARE);
    #endif
    jchr = INT_MAX/((ami_maxx(stdout)-2)/2); /* find basic joystick increment */
    ami_select(stdout, 2, 2); /* switch screens */
    putchar('\f'); /* clear screen */
    ami_curvis(stdout, FALSE); /* remove drawing cursor */
    ami_auto(stdout, FALSE); /* turn off scrolling */
    ami_timer(stdout, 1, MOVTIM, TRUE); /* set movement timer */

    start: /* start new game */

    drwscn(); /* draw game screen */
    padx = ami_maxx(stdout)/2; /* find intial paddle position */
    writexy(padx-3, ami_maxy(stdout)-1, "======="); /* place paddle */
    ballx = 0; /* set ball ! on screen */
    bally = 0;
    baltim = 0; /* set ball ready to start */
    do { /* game loop */

        if (!ballx && !baltim) {

            /* ball not on screen, and time to wait expired, send out ball */
            ballx = 2; /* place ball */
            bally = ami_maxy(stdout)-3;
            bdx = +1; /* set direction of travel */
            bdy = -1;
            writexy(ballx, bally, "*"); /* draw the ball */
            score = 0; /* clear score */

        }
        /* place updated score on screen */
        ami_cursor(stdout, ami_maxx(stdout) / 2-11 / 2, 1);
        printf("SCORE %5d\n", score);
        do { ami_event(stdin, &er); /* wait relivant events */
        } while (er.etype != ami_etterm && er.etype != ami_etleft &&
                 er.etype != ami_etright && er.etype != ami_etfun &&
                 er.etype != ami_ettim && er.etype != ami_etjoymov);
        if (er.etype == ami_etterm) goto exit; /* game exits */
        if (er.etype == ami_etfun) goto start; /* restart game */
        /* process paddle movements */
        if (er.etype == ami_etleft) padpos(padx-1); /* move left */
        else if (er.etype == ami_etright) padpos(padx+1); /* move right */
        else if (er.etype == ami_etjoymov) /* move joystick */
            padpos(ami_maxx(stdout)/2+er.joypx/jchr);
        else if (er.etype == ami_ettim) { /* move timer */

            if (er.timnum == 1) { /* ball timer */

                /* if the note timer is running, decrement it */
                if (nottim > 0) {

                    nottim--; /* derement */
                    #if SOUND
                    if (nottim == 0) /* times up, turn note off */
                        ami_noteoff(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                    #endif

                }
                /* if the fail note timer is running, decrement it */
                if (failtimer > 0) {

                    failtimer = failtimer-1; /* derement */
                    #if SOUND
                    if (!failtimer) /* times up, turn note off */
                        ami_noteoff(AMI_SYNTH_OUT, 0, 1, FAILNOTE, INT_MAX);
                    #endif

                }
                if (ballx > 0) { /* ball on screen */

                    writexy(ballx, bally, " "); /* erase the ball */
                    bsx = ballx; /* save ball position */
                    bsy = bally;
                    ballx = ballx+bdx; /* move the ball */
                    bally = bally+bdy;
                    /* check off screen motions */
                    if (ballx == 1 || ballx == ami_maxx(stdout)) {

                        ballx = bsx; /* restore */
                        bdx = -bdx; /* change direction */
                        ballx = ballx+bdx; /* recalculate */
                        /* start bounce note */
                        #if SOUND
                        ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                        #endif
                        nottim = BNCENOTE; /* set timer */

                    }
                    if (bally == 1) { /* hits top */

                        bally = bsy; /* restore */
                        bdy = -bdy; /* change direction */
                        bally = bally+bdy; /* recalculate */
                        /* start bounce note */
                        #if SOUND
                        ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                        #endif
                        nottim = BNCENOTE; /* set timer */

                    } else if (bally == ami_maxy(stdout)-1 &&
                               ballx >= padx-3 &&
                               ballx <= padx+3) {

                        /* hits paddle */
                        bally = bsy; /* restore */
                        bdy = -bdy; /* change direction */
                        bally = bally+bdy; /* recalculate */
                        score = score+1; /* count hits */
                        /* start bounce note */
                        #if SOUND
                        ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                        #endif
                        nottim = BNCENOTE; /* set timer */

                    }
                    if (bally != ami_maxy(stdout))
                        writexy(ballx, bally, "*"); /* redraw the ball */

                }
                /* if the ball timer is running, decrement it */
                if (baltim > 0) baltim--;

            }

        }
        if (bally == ami_maxy(stdout)) { /* ball out of bounds */

            ballx = 0; /* set ball not on screen */
            bally = 0;
            baltim = NEWBAL; /* start time on new ball wait */
            /* start fail note */
            #if SOUND
            ami_noteon(AMI_SYNTH_OUT, 0, 1, FAILNOTE, INT_MAX);
            #endif
            failtimer = FAILTIME; /* set timer */

        }

    } while (TRUE); /* forever */

    /* exit game */
    exit:
    ami_curvis(stdout, TRUE); /* restore drawing cursor */
    ami_auto(stdout, TRUE); /* turn scrolling back on */
    ami_select(stdout, 1, 1); /* restore screen */

    #if SOUND
    ami_closesynthout(AMI_SYNTH_OUT); /* close synthesizer */
    #endif

    return (0); /* exit no error */

}
