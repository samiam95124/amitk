/********************************************************************************
*                                                                              *
*                           GRAPHICAL CLOCK PROGRAM                            *
*                                                                              *
* Analog clock program in Petit-Ami windowed graphical mode.                   *
* Presents an analog clock at any scale. Clicking the mouse anywhere within    *
* clock turns the move/size frame on and off. This allows the clock to be      *
* placed at a convenient location and then have the frame removed.             *
*                                                                              *
********************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <services.h>
#include <graphics.h>

#define HOURSEC (60*60)      /* number of seconds in an hour */
#define DAYSEC  (HOURSEC*24) /* number of seconds in a day */

pa_evtrec er;  /* event record */
int       frm; /* frame on/off */

/* find rectangular coordinates from polar, relative to center of circle,
  with given diameter */

void rectcord(int a,          /* angle, 0-359 */
              int d,          /* diameter of circle */
              int* x, int* y) /* returns rectangular coordinate */

{

    float angle; /* angle in radian measure */

    angle = a*0.01745329; /* find radian measure */
    *x = round(sin(angle)*(d/2)); /* find distance x */
    *y = round(cos(angle)*(d/2)); /* find distance y */

}

/* draw polar coordinate line */

void pline(int a,           /* angle of hand */
           int o,           /* length of hand */
           int i,           /* distance from center */
           int cx, int cy,  /* center of circle in x and y */
           int w)           /* width of hand */

{

    int sx, sy, ex, ey; /* line start and end */

    rectcord(a, i, &sx, &sy); /* find startpoint of line */
    rectcord(a, o, &ex, &ey); /* find endpoint of line */
    pa_linewidth(stdout, w); /* set width */
    pa_line(stdout, cx+sx, cy-sy, cx+ex, cy-ey); /* draw second hand */

}

/* update time */

void update(int cx, int cy, /* center of clock in x and y */
            int d)          /* diameter of clock face */

{

    int t;    /* current time */
    int s;    /* seconds */
    int m;    /* minutes */
    int h;    /* hours */
    int x, y; /* coordinates */
    char ds[100]; /* storage for date */

    /* break down time to seconds, minutes, hours */
    t = pa_local(pa_time()); /* get local time */
    pa_dates(ds, 100, t); /* get the date in ASCII from that */
    t = t%DAYSEC; /* find number of seconds in day */
    /* if before 2000, find from remaining seconds */
    if (t < 0) t = DAYSEC+t;
    h = t/HOURSEC; /* find hours */
    t = t%HOURSEC; /* subtract hours */
    m = t/60; /* find minutes */
    s = t%60; /* find seconds */
    /* display time on hands */
    pline(s*(360 / 60), d, 0, cx, cy, 1); /* draw second hand */
    pline(m*(360 / 60), d, 0, cx, cy, 3); /* draw minute hand */
    pline(h*(360 / 12)+m / 2, d / 2, 0, cx, cy, 3); /* draw hour hand */
    /* place date centered, 1/4 down from the clock center (1/8 radius) */
    pa_cursorg(stdout, cx-pa_strsiz(stdout, ds)/2, cy+d/8);
    printf("%s", ds); /* write date to clock face */

}

/* draw clock */

void drawclock(void)

{

    int d;      /* circle diameter */
    int cx, cy; /* center of circle */
    int i;      /* index */
    int t;      /* length of tick mark */

    /* erase background */
    pa_fcolor(stdout, pa_white);
    pa_frect(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_fcolor(stdout, pa_black);
    /* find diameter of circle by shorter of two axies */
    if (pa_maxxg(stdout) > pa_maxyg(stdout)) d = pa_maxyg(stdout)-20;
    else d = pa_maxxg(stdout)-20;
    /* find center of clock, in center of window */
    cx = pa_maxxg(stdout)/2;
    cy = pa_maxyg(stdout)/2;
    t = d/20; /* find tick mark length */
    /* draw tick marks on clock */
    for (i = 1; i <= 12; i++)
       pline(360 / 12*i, d, d-(t+t*(i%3==0)), cx, cy, 3);
    update(cx, cy, d); /* update face time */

}

int main(void)

{

    pa_curvis(stdout, FALSE); /* turn off cursor */
    pa_buffer(stdout, FALSE); /* turn off buffering */
    pa_auto(stdout, FALSE); /* turn off wrap/scroll */
    pa_binvis(stdout); /* set no background overwrite on text */
    pa_font(stdout, PA_FONT_SIGN); /* use proportional font */
    pa_bold(stdout, TRUE); /* turn on bold */
    frm = TRUE; /* set frame on */
    pa_timer(stdout, 1, 10000, TRUE); /* set second update timer */
    /* loop for events */
    do {

        pa_event(stdin, &er); /* get next event */
        /* if either a redraw or a timer tick, draw the clock */
        if (er.etype == pa_etredraw || er.etype == pa_ettim) drawclock();
        /* if the user clicks the mouse anywhere in the clock, flip the frame
           on and off */
        if (er.etype == pa_etmouba) {

            frm = !frm; /* flip frame status */
            pa_sizable(stdout, frm);
            pa_sysbar(stdout, frm);

        }

    } while (er.etype != pa_etterm);

}
