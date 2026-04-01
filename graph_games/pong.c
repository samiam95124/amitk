/*******************************************************************************
*                                                                              *
*                                 PONG GAME                                    *
*                                                                              *
*                       COPYRIGHT (C) 1997 S. A. FRANCO                        *
*                                                                              *
* Plays pong in graphical mode.                                                *
*                                                                              *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <sound.h>
#include <graphics.h>

#define BALMOV   50                /* ball move timer */
#define NEWBAL   (100*2)           /* wait for new ball time, 1 sec (in ball units) */
#define BALLCLR  ami_blue              /* ball color */
#define WALLCLR  ami_cyan              /* WALL color */
#define PADCLR   ami_green             /* paddle color */
#define BNCENOTE 5                 /* time to play bounce note */
#define WALLNOTE (AMI_NOTE_D+AMI_OCTAVE_6) /* note to play off WALL */
#define FAILTIME 30                /* note to play on failure */
#define FAILNOTE (AMI_NOTE_C+AMI_OCTAVE_4) /* note to play on fail */

typedef struct { /* rectangle */

    int x1, y1, x2, y2;

} rectangle;

int       wall;                       /* wall thickness */
int       balls;                      /* ball size */
int       hballs;                     /* half ball size */
int       padh;                       /* height of paddle */
int       pwdis;                      /* distance of paddle from bottom wall */
int       padw;                       /* paddle width */
int       hpadw;                      /* half paddle width */
int         padx;         /* paddle position x */
int         bdx;          /* ball direction x */
int         bdy;          /* ball direction y */
int         bsx;          /* ball position save x */
int         bsy;          /* ball position save y */
int         baltim;       /* ball start timer */
ami_evtrec   er;           /* event record */
int         jchr;         /* number of pixels to joystick movement */
int         score;        /* score */
int         scrsiz;       /* score size */
int         scrchg;       /* score has changed */
int         bac;          /* ball accelerator */
int         nottim;       /* bounce note timer */
int         failtimer;    /* fail note timer */
rectangle   paddle;       /* paddle rectangle */
rectangle   ball, balsav; /* ball rectangle */
rectangle   wallt, walll, wallr, wallb; /* wall rectangles */

/*******************************************************************************

Write string to screen

Writes a string to the indicated position on the screen.

********************************************************************************/

void writexy(int x, int y,   /* position to write to */
             const string s) /* char* to write */

{

    ami_cursorg(stdout, x, y); /* position cursor */
    puts(s); /* stdout string */

}

/*******************************************************************************

Write centered string

Writes a string that is centered on the line given. Returns the
starting position of the string.

********************************************************************************/

void wrtcen(int          y, /* y position of string */
            const string s) /* string to write */

{

    int off; /* string offset */

    off = ami_maxxg(stdout)/2-ami_strsiz(stdout, s)/2;
    writexy(off, y, s); /* write out contents */

}

/*******************************************************************************

Draw rectangle

Draws a filled rectangle, in the given color.

********************************************************************************/

void drwrect(rectangle* r, ami_color c)

{

    ami_fcolor(stdout, c); /* set color */
    ami_frect(stdout, r->x1, r->y1, r->x2, r->y2);

}

/*******************************************************************************

Offset rectangle

Offsets a rectangle by an x and y difference.

********************************************************************************/

void offrect(rectangle* r, int x, int y)

{

    r->x1 = r->x1+x;
    r->y1 = r->y1+y;
    r->x2 = r->x2+x;
    r->y2 = r->y2+y;

}

/*******************************************************************************

Rationalize a rectangle

Rationalizes a rectangle, that is, arranges the points so that the 1st point
is lower in x and y than the second.

********************************************************************************/

void ratrect(rectangle* r)

{

    int t; /* swap temp */

    if (r->x1 > r->x2) { /* swap x */

        t = r->x1;
        r->x1 = r->x2;
        r->x2 = t;

    }
    if (r->y1 > r->y2) { /* swap y */

        t = r->y1;
        r->y1 = r->y2;
        r->y2 = t;

    }

}

/*******************************************************************************

Find intersection of rectangles

Checks if two rectangles intersect. Returns true if so.

********************************************************************************/

int intrect(rectangle* r1, rectangle* r2)

{

    /* rationalize the rectangles */
    ratrect(r1);
    ratrect(r2);

    return ((*r1).x2 >= (*r2).x1 && (*r1).x1 <= (*r2).x2 &&
            (*r1).y2 >= (*r2).y1 && (*r1).y1 <= (*r2).y2);

}

/*******************************************************************************

Set rectangle

Sets the rectangle to the given values.

********************************************************************************/

void setrct(rectangle* r, int x1, int y1, int x2, int y2)

{

    r->x1 = x1;
    r->y1 = y1;
    r->x2 = x2;
    r->y2 = y2;

}

/*******************************************************************************

Clear rectangle

Clear rectangle points to zero. Usually used to flag the rectangle invalid.

*******************************************************************************/

void clrrect(rectangle* r)

{

    r->x1 = 0;
    r->y1 = 0;
    r->x2 = 0;
    r->y2 = 0;

}

/*******************************************************************************

Draw screen

Draws a new screen, with borders.

********************************************************************************/

void drwscn(void)

{

    putchar('\n'); /* clear screen */
    /* draw walls */
    drwrect(&wallt, WALLCLR); /* top */
    drwrect(&walll, WALLCLR); /* left */
    drwrect(&wallr, WALLCLR); /* right */
    drwrect(&wallb, WALLCLR); /* bottom */
    ami_fcolor(stdout, ami_black);
    wrtcen(ami_maxyg(stdout)-wall+1, "PONG VS. 1.0");

}

/*******************************************************************************

Set new paddle position

Places the paddle at the given position.

********************************************************************************/

void padpos(int x)

{

    if (x-hpadw <= walll.x2) x = walll.x2+hpadw+1; /* clip to ends */
    if (x+hpadw >= wallr.x1) x = wallr.x1-hpadw-1;
    /* erase old location */
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, padx-hpadw, ami_maxyg(stdout)-wall-padh-pwdis,
                     padx+hpadw, ami_maxyg(stdout)-wall-pwdis);
    padx = x; /* set new location */
    setrct(&paddle, x-hpadw, ami_maxyg(stdout)-wall-padh-pwdis,
                    x+hpadw, ami_maxyg(stdout)-wall-pwdis);
    drwrect(&paddle, PADCLR); /* draw paddle */

}

int main(void)

{

    nottim = 0; /* clear bounce note timer */
    failtimer = 0; /* clear fail timer */
    ami_opensynthout(AMI_SYNTH_OUT); /* open synthesizer */
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_LEAD_1_SQUARE);
    jchr = INT_MAX/((ami_maxxg(stdout)-2)/2); /* find basic joystick increment */
    ami_curvis(stdout, FALSE); /* remove drawing cursor */
    ami_auto(stdout, FALSE); /* turn off scrolling */
    ami_font(stdout, AMI_FONT_SIGN); /* sign font */
    wall = ami_maxyg(stdout)/20; /* set wall thickness */
    balls = ami_maxyg(stdout)/20; /* set ball size */
    hballs = balls/2; /* set half ball size */
    padh = ami_maxyg(stdout)/22; /* set paddle thickness */
    pwdis = padh/4; /* set distance of paddle to wall */
    padw = ami_maxxg(stdout)/8; /* set paddle width */
    hpadw = padw/2; /* half paddle width */
    ami_bold(stdout, TRUE);
    ami_fontsiz(stdout, wall-2); /* font fits in the wall */
    ami_binvis(stdout); /* no background writes */
    ami_timer(stdout, 1, BALMOV, TRUE); /* enable timer */

    newgame: /* start new game */

    padx = ami_maxxg(stdout)/2; /* find initial paddle position */
    padpos(padx); /* display paddle */
    clrrect(&ball); /* set ball not on screen */
    baltim = 0; /* set ball ready to start */
    /* set up wall rectangles */
    setrct(&wallt, 1, 1, ami_maxxg(stdout), wall); /* top */
    setrct(&walll, 1, 1, wall, ami_maxyg(stdout)); /* left */
    /* right */
    setrct(&wallr, ami_maxxg(stdout)-wall, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    /* bottom */
    setrct(&wallb, 1, ami_maxyg(stdout)-wall, ami_maxxg(stdout), ami_maxyg(stdout));
    scrsiz = ami_strsiz(stdout, "SCORE 0000"); /* set nominal size of score string */
    scrchg = TRUE; /* set score changed */
    drwscn(); /* draw game screen */
    do { /* game loop */

        if (ball.x1 == 0 && baltim == 0) {

            /* ball not on screen, and time to wait expired, send out ball */
            setrct(&ball, wall+1, ami_maxyg(stdout)-4*wall-balls,
                          wall+1+balls, ami_maxyg(stdout)-4*wall);
            bdx = +ami_maxxg(stdout)/300; /* set direction of travel */
            bdy = -ami_maxyg(stdout)/150;
            /* draw the ball */
            ami_fcolor(stdout, BALLCLR);
            drwrect(&ball, BALLCLR);
            score = 0; /* clear score */
            scrchg = TRUE; /* set changed */

        }
        if (scrchg) { /* process score change */

            /* erase score */
            ami_fcolor(stdout, WALLCLR);
            ami_frect(stdout, ami_maxxg(stdout)/2-scrsiz/2, 1,
                          ami_maxxg(stdout)/2+scrsiz/2, wall);
            /* place updated score on screen */
            ami_fcolor(stdout, ami_black);
            ami_cursorg(stdout, ami_maxxg(stdout)/2-scrsiz/2, 2);
            printf("SCORE %5d\n", score);
            scrchg = FALSE; /* reset score change flag */

        }
        do { ami_event(stdin, &er); /* wait relevant events */
        } while (er.etype != ami_etterm && er.etype != ami_etleft &&
                 er.etype != ami_etright && er.etype != ami_etfun &&
                 er.etype != ami_ettim && er.etype != ami_etjoymov);
        if (er.etype == ami_etterm) goto endgame; /* game exits */
        if (er.etype == ami_etfun) goto newgame; /* restart game */
        /* process paddle movements */
        if (er.etype == ami_etleft) padpos(padx-5); /* move left */
	    else if (er.etype == ami_etright) padpos(padx+5); /* move right */
        else if (er.etype == ami_etjoymov) /* move joystick */
            padpos(ami_maxxg(stdout)/2+er.joypx/jchr);
        else if (er.etype == ami_ettim) { /* move timer */

            if (er.timnum == 1) { /* ball timer */

              	/* if the note timer is running, decrement it */
                if (nottim > 0) {

                  	nottim--; /* derement */
                   	if (nottim == 0) /* times up, turn note off */
                      	ami_noteoff(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);

                }
                /* if the fail note timer is running, decrement it */
                if (failtimer > 0) {

                   	failtimer = failtimer-1; /* derement */
                   	if (!failtimer) /* times up, turn note off */
                      	ami_noteoff(AMI_SYNTH_OUT, 0, 1, FAILNOTE, INT_MAX);

                }
                if (ball.x1 > 0) { /* ball on screen */

                   	balsav = ball; /* save ball position */
                   	offrect(&ball, bdx, bdy); /* move the ball */
                   	/* check off screen motions */
                   	if (intrect(&ball, &walll) || intrect(&ball, &wallr)) {

                     	/* hit left or right wall */
                     	ball = balsav; /* restore */
                      	bdx = -bdx; /* change direction */
                      	offrect(&ball, bdx, bdy); /* recalculate */
                      	/* start bounce note */
                      	ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                      	nottim = BNCENOTE; /* set timer */

                   } else if (intrect(&ball, &wallt)) { /* hits top */

                      	ball = balsav; /* restore */
                      	bdy = -bdy; /* change direction */
                     	offrect(&ball, bdx, bdy); /* recalculate */
                      	/* start bounce note */
                      	ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                      	nottim = BNCENOTE; /* set timer */

                   } else if (intrect(&ball, &paddle)) {

                      	/* hits paddle. now the ball can hit left, center or right.
                           left goes left, right goes right, and center reflects */
                      	ball = balsav; /* restore */
                      	if (ball.x1+hballs < padx-padh/2) bdx = -1; /* left */
                      	else if (ball.x1+hballs > padx+padh/2) bdx = +1; /* right */
                      	else if (bdx < 0) bdx = -1; else bdx = +1; /* center */
                      	bdy = -bdy; /* reflect y */
                      	offrect(&ball, bdx, bdy); /* recalculate */
                      	score = score+1; /* count hits */
                      	scrchg = TRUE; /* set changed */
                      	/* start bounce note */
                      	ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                      	nottim = BNCENOTE; /* set timer */

                   }
                   if (intrect(&ball, &wallb)) { /* ball out of bounds */

                        drwrect(&balsav, ami_white);
                        clrrect(&ball); /* set ball not on screen */
                        baltim = NEWBAL; /* start time on new ball wait */
                        /* start fail note */
                        ami_noteon(AMI_SYNTH_OUT, 0, 1, FAILNOTE, INT_MAX);
                        failtimer = FAILTIME; /* set timer */

                   } else { /* ball in play */

                        /* erase only the leftover part of the old ball */
                        ami_fcolor(stdout, ami_white);
                        if (bdx < 0) /* ball move left */
                            ami_frect(stdout, ball.x2+1, balsav.y1,
                                             balsav.x2, balsav.y2);
                        else /* move move right */
                            ami_frect(stdout, balsav.x1, balsav.y1,
                                             ball.x1-1, balsav.y2);
                        if (bdy < 0) /* ball move up */
                            ami_frect(stdout, balsav.x1, ball.y2+1,
                                             balsav.x2, balsav.y2);
                        else /* move move down */
                            ami_frect(stdout, balsav.x1, balsav.y1,
                                             balsav.x2, ball.y1-1);
                        drwrect(&ball, BALLCLR); /* redraw the ball */

                    }

                }
                /* if the ball timer is running, decrement it */
                if (baltim > 0) baltim = baltim-1;

            }

        }

    } while (TRUE); /* forever */

    endgame: /* exit game */

    ami_closesynthout(AMI_SYNTH_OUT); /* close synthesizer */

}


