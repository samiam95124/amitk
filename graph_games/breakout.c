/*******************************************************************************
*                                                                              *
*                                BREAKOUT GAME                                 *
*                                                                              *
*                       COPYRIGHT (C) 2002 S. A. FRANCO                        *
*                                                                              *
* Plays breakout in graphical mode.                                            *
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

/* enable sounds */
#define SOUND 1

#define   SECOND      10000                   /* one second */
#define   OSEC        (SECOND/8)              /* 1/8 second */
#define   BALMOV      50                      /* ball move timer */
#define   NEWBAL      SECOND                  /* wait for new ball time */
#define   BALLCLR     ami_blue                 /* ball ami_color */
#define   WALLCLR     ami_cyan                 /* wall ami_color */
#define   PADCLR      ami_green                /* paddle ami_color */
#define   BOUNCETIME  250                     /* time to play bounce note */
#define   WALLNOTE    (AMI_NOTE_D+AMI_OCTAVE_6) /* note to play off wall */
#define   BRICKNOTE   (AMI_NOTE_E+AMI_OCTAVE_7) /* note to play off brick */
#define   FAILTIME    1500                    /* note to play on failure */
#define   FAILNOTE    (AMI_NOTE_C+AMI_OCTAVE_4) /* note to play on fail */
#define   BRKROW      6                       /* number of brick rows */
#define   BRKCOL      10                      /* number of brick columns */

typedef struct { /* rectangle */

    int x1, y1, x2, y2;

} rectangle;

int       padx;                       /* paddle position x */
int       bdx;                        /* ball direction x */
int       bdy;                        /* ball direction y */
int       bsx;                        /* ball position save x */
int       bsy;                        /* ball position save y */
int       baltim;                     /* ball start timer */
ami_evtrec er;                         /* event record */
int       jchr;                       /* number of pixels to joystick
                                         movement */
int       score;                      /* score */
int       scrsiz;                     /* score size */
int       scrchg;                     /* score has changed */
int       bac;                        /* ball accelerator */
rectangle paddle;                     /* paddle rectangle */
rectangle ball, balsav;               /* ball rectangle */
rectangle wallt, walll, wallr, wallb; /* WALL rectangles */
rectangle bricks[BRKROW][BRKCOL];     /* brick array */
int       brki;                       /* brick was intersected */
int       fldbrk;                     /* bricks hit this field */
int       bip;                        /* middle of ball intersection with
                                         paddle */
int       wall;                       /* wall thickness */
int       brkh;                       /* brick height */
int       brkbrd;                     /* brick border */
int       balls;                      /* ball size */
int       hballs;                     /* half ball size */
int       padh;                       /* height of paddle */
int       pwdis;                      /* distance of paddle from bottom wall */
int       padw;                       /* paddle width */
int       hpadw;                      /* half paddle width */

/*******************************************************************************

Write string to screen

Writes a string to the indicated position on the screen.

********************************************************************************/

void writexy(int x, int y,   /* position to write to */
             const string s) /* char* to write */

{

    ami_cursorg(stdout, x, y); /* position cursor */
    puts(s); /* output string */

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

Translate color code

Translates a logical color to an RGB color. Returns the RGB color in three
variables.

*******************************************************************************/

void log2rgb(ami_color c, int* r, int* g, int* b)

{

    /* translate color number */
    switch (c) { /* color */

        case ami_black:   *r = 0;       *g= 0;        *b = 0;       break;
        case ami_white:   *r = INT_MAX; *g = INT_MAX; *b = INT_MAX; break;
        case ami_red:     *r = INT_MAX; *g = 0;       *b = 0;       break;
        case ami_green:   *r = 0;       *g = INT_MAX; *b = 0;       break;
        case ami_blue:    *r = 0;       *g = 0;       *b = INT_MAX; break;
        case ami_cyan:    *r = 0;       *g = INT_MAX; *b = INT_MAX; break;
        case ami_yellow:  *r = INT_MAX; *g = INT_MAX; *b = 0;       break;
        case ami_magenta: *r = INT_MAX; *g = 0;       *b = INT_MAX; break;
        default: ;

    }

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

Draw bordered rectangle

Draws a filled rectangle with border, in the given color.

********************************************************************************/

void dim(float dv, int* r, int* g, int* b)

{

    *r = trunc(*r*dv);
    *g = trunc(*g*dv);
    *b = trunc(*b*dv);

}

void drwbrect(rectangle* r, ami_color c)

{

    int i;
    int hr, hg, hb; /* rgb value of highlight */
    int mr, mg, mb; /* rbg value of midlight */
    int lr, lg, lb; /* rbg value of lowlight */

    log2rgb(c, &hr, &hg, &hb); /* find actual ami_color */
    mr = hr; /* copy */
    mg = hg;
    mb = hb;
    lr = hr;
    lg = hg;
    lb = hb;
    dim(0.80, &mr, &mg, &mb); /* dim midlight to %75 */
    dim(0.60, &lr, &lg, &lb); /* dim lowlight to %50 */
    ami_fcolorg(stdout, mr, mg, mb); /* set brick body to midlight */
    ami_frect(stdout, r->x1, r->y1, r->x2, r->y2); /* draw brick */
    ami_fcolorg(stdout, hr, hg, hb); /* set hilight */
    ami_frect(stdout, r->x1, r->y1, r->x1+brkbrd-1, r->y2); /* border left */
    ami_frect(stdout, r->x1, r->y1, r->x2, r->y1+brkbrd-1); /* top */
    /* set lowlight border color */
    ami_fcolorg(stdout, lr, lg, lb);
    /* border right */
    for (i = 1; i <= brkbrd; i++)
        ami_frect(stdout, r->x2-i+1, r->y1+i-1, r->x2, r->y2);
    /* border bottom */
    for (i = 1; i <= brkbrd; i++)
        ami_frect(stdout, r->x1+i-1, r->y2-i+1, r->x2, r->y2);

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

int intsec(rectangle* r1, rectangle* r2)

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
    wrtcen(ami_maxyg(stdout)-wall+1, "BREAKOUT VS. 1.0");

}

/*******************************************************************************

Draw wall

Redraws the brick wall.

********************************************************************************/

void drwwall(void)

{

    int      r, c; /* brick array indexes */
    ami_color clr;  /* brick color */

    clr = ami_red; /* set 1st pure color */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) {

        drwbrect(&bricks[r][c], clr);
        if (clr < ami_magenta) clr++;
        else clr = ami_red;

    }

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

/*******************************************************************************

Set brick WALL

Initializes the bricks in the wall coordinates.

********************************************************************************/

void setwall(void)

{

    int r, c;   /* brick array indexes */
    int brkw;   /* brick width */
    int brkr;   /* brick remainder */
    int brkoff; /* brick WALL offset */
    int co;     /* collumn offset */
    int rd;     /* remainder distributor */

    brkw = (ami_maxxg(stdout)-2*wall)/BRKCOL; /* find brick width */
    brkr = (ami_maxxg(stdout)-2*wall)%BRKCOL-1; /* find brick remainder */
    brkoff = ami_maxyg(stdout)/4; /* find brick wall offset */
    for (r = 0; r < BRKROW; r++) {

        co = 0; /* clear collumn offset */
        rd = brkr; /* set remainder distributor */
        for (c = 0; c < BRKCOL; c++) {

            setrct(&bricks[r][c], 1+co+wall, 1+(r-1)*brkh+brkoff,
                                              1+co+brkw-1+wall+(rd > 0),
                                              1+(r-1)*brkh+brkh-1+brkoff);
            co = co+brkw+(rd > 0); /* offset to next brick */
            if (brkr > 0) rd = rd-1; /* ami_reduce remainder */

        }

    }

}

/********************************************************************************

Find brick intersection

Searches for a brick that intersects with the ball, and if found, erases the
brick and returns true. Note that if more than one brick intersects, they all
disappear.

********************************************************************************/

void interbrick(void)

{

    int r, c; /* brick array indexes */

    brki = FALSE; /* set no brick intersection */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) if (intsec(&ball, &bricks[r][c])) {

        brki = TRUE; /* set intersected */
        drwrect(&bricks[r][c], ami_white); /* erase from screen */
        clrrect(&bricks[r][c]); /* clear brick data */
        score++; /* count hits */
        scrchg = TRUE; /* set changed */
        fldbrk++; /* add to bricks this field */

    }

}

int main(void)

{

#ifdef SOUND
    ami_opensynthout(AMI_SYNTH_OUT); /* open synthesizer */
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_LEAD_1_SQUARE);
    ami_starttimeout(); /* start sequencer running */
#endif
    jchr = INT_MAX/((ami_maxxg(stdout)-2)/2); /* find basic joystick increment */
    ami_curvis(stdout, FALSE); /* remove drawing cursor */
    ami_auto(stdout, FALSE); /* turn off scrolling */
    ami_font(stdout, AMI_FONT_SIGN); /* sign font */
    wall = ami_maxyg(stdout)/20; /* set wall thickness */
    brkh = ami_maxyg(stdout)/25; /* set brick thickness */
    brkbrd = brkh/5; /* set brick border */
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
    score = 0; /* clear score */
    baltim = NEWBAL/BALMOV; /* set starting ball time */
    do { /* game loop */

        setwall(); /* initialize bricks */
        drwwall(); /* redraw the wall */
        fldbrk = 0; /* clear bricks hit this field */
        do { /* fields */

            if (ball.x1 == 0 && baltim == 0) {

                /* ball not on screen, and time to wait expired, send out ball */
                setrct(&ball, wall+1, ami_maxyg(stdout)-4*wall-balls,
                               wall+1+balls, ami_maxyg(stdout)-4*wall);
                bdx = +ami_maxxg(stdout)/300; /* set direction of travel */
                bdy = -ami_maxyg(stdout)/150;
                /* draw the ball */
                ami_fcolor(stdout, BALLCLR);
                drwrect(&ball, BALLCLR);
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

                    if (ball.x1 > 0) { /* ball on screen */

                        balsav = ball; /* save ball position */
                        offrect(&ball, bdx, bdy); /* move the ball */
                        /* check off screen motions */
                        if (intsec(&ball, &walll) || intsec(&ball, &wallr)) {

                            /* hit left or right wall */
                            ball = balsav; /* restore */
                            bdx = -bdx; /* change direction */
                            offrect(&ball, bdx, bdy); /* recalculate */
#ifdef SOUND
                            /* start bounce note */
                            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, INT_MAX);
#endif

                        } else if (intsec(&ball, &wallt)) { /* hits top */

                            ball = balsav; /* restore */
                            bdy = -bdy; /* change direction */
                            offrect(&ball, bdx, bdy); /* recalculate */
#ifdef SOUND
                            /* start bounce note */
                            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, INT_MAX);
#endif

                        } else if (intsec(&ball, &paddle)) {

                            ball = balsav; /* restore */
                            /* find which 5th of the paddle was struck */
                            bip = (ball.x1+hballs-paddle.x1)/(padw/5);
                            /* clip to 5th */
                            if (bip < 0) bip = 0;
                            if (bip > 5) bip = 5;
                            switch (bip) {

                                case 0: bdx = -2; break; /* left hard */
                                case 1: bdx = -1; break; /* soft soft */
                                case 2: ;         break; /* center reflects */
                                case 3: bdx = +1; break; /* right soft */
                                case 4: bdx = +2; break; /* right hard */
                                case 5: bdx = +2; break; /* right hard */

                            }
                            bdy = -bdy; /* reflect y */
                            offrect(&ball, bdx, bdy); /* recalculate */
                            /* if the ball is still below the paddle plane, move
                               it up until it is not */
                            if (ball.y2 >= paddle.y1)
                                offrect(&ball, 0, -(ball.y2-paddle.y1+1));
#ifdef SOUND
                            /* start bounce note */
                            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, INT_MAX);
#endif

                        } else { /* check brick hits */

                            interbrick(); /* check brick intersection */
                            if (brki) { /* there was a brick hit */

                                ball = balsav; /* restore */
                                bdy = -bdy; /* change direction */
                                offrect(&ball, bdx, bdy); /* recalculate */
#ifdef SOUND
                                /* start bounce note */
                                ami_noteon(AMI_SYNTH_OUT, 0, 1, BRICKNOTE, INT_MAX);
                                ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, BRICKNOTE, INT_MAX);
#endif

                            }

                        };
                        if (intsec(&ball, &wallb)) { /* ball out of bounds */

                            drwrect(&balsav, ami_white);
                            clrrect(&ball); /* set ball not on screen */
                            /* start time on new ball wait */
                            baltim = NEWBAL/BALMOV;
#ifdef SOUND
                            /* start fail note */
                            ami_noteon(AMI_SYNTH_OUT, 0, 1, FAILNOTE, INT_MAX);
                            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+FAILTIME, 1, FAILNOTE, INT_MAX);
#endif

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
                            else /* move move ami_down */
                                ami_frect(stdout, balsav.x1, balsav.y1,
                                                 balsav.x2, ball.y1-1);
                            drwrect(&ball, BALLCLR); /* redraw the ball */

                        }

                    }
                    /* if the ball timer is running, decrement it */
                    if (baltim > 0) baltim--;

                }

            }

        } while (fldbrk != BRKROW*BRKCOL); /* until bricks are cleared */
#ifdef SOUND
        ami_noteon(AMI_SYNTH_OUT,  0,                  1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*2,  1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*3,  1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*4,  1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*5,  1, AMI_NOTE_E+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*6,  1, AMI_NOTE_E+AMI_OCTAVE_6, INT_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*7,  1, AMI_NOTE_F+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*8,  1, AMI_NOTE_F+AMI_OCTAVE_6, INT_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*9,  1, AMI_NOTE_E+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*10, 1, AMI_NOTE_E+AMI_OCTAVE_6, INT_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*11, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*13, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
#endif
        baltim = (OSEC*13+NEWBAL)/BALMOV; /* wait fanfare */
        drwrect(&ball, ami_white); /* clear ball */
        clrrect(&ball); /* set ball not on screen */

    } while (TRUE); /* forever */

    endgame:; /* exit game */

#ifdef SOUND
    ami_closesynthout(AMI_SYNTH_OUT); /* close synthesizer */
#endif

}
