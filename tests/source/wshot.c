/*******************************************************************************
*                                                                              *
*                       PICTURES OF THE WIDGETS                                *
*                                                                              *
*                    Copyright (C) 2026 Scott A. Franco                        *
*                                                                              *
* Takes the widget figures for the manual. Each figure stands alone in a       *
* window cut down to its size, is captured, and the next one takes its place,  *
* so what comes back is the widget and a small margin rather than a page with  *
* a widget somewhere on it.                                                    *
*                                                                              *
* Execution:                                                                   *
*                                                                              *
*     wshot <file>                                                             *
*                                                                              *
* The pictures are appended to <file> in the order below, one PNG after        *
* another in the one file, which is the form screen_capture() writes and       *
* bin/testviewer reads. Each is named on stderr as it is taken, so a figure    *
* can be matched to its frame number when the file is split.                   *
*                                                                              *
*     1  scroll bars, horizontal and vertical                                  *
*     2  scroll bars at four slider sizes                                      *
*     3  number select box                                                     *
*     4  edit box                                                              *
*     5  list box                                                              *
*     6  drop box, closed          7  drop box, open                           *
*     8  drop edit box, closed     9  drop edit box, open                      *
*    10  sliders, with and without tick marks                                  *
*    11  tab bars, the four orientations                                       *
*    12  tab bars, the four over one client area                               *
*    13  background                                                            *
*    14  group box                                                             *
*    15  progress bar                                                          *
*                                                                              *
* The two open figures need a click, because a drop box opens from the user    *
* and there is no call that opens one. The click is written to the rig's       *
* input fifo, so PD_INPUT must name it:                                        *
*                                                                              *
*     mkfifo /tmp/rig                                                          *
*     PD_INPUT=/tmp/rig bin/wshot doc/widgets.png                              *
*                                                                              *
* Without it the run still completes and those two figures come out closed.    *
*                                                                              *
* A widget is killed between figures. A drop box whose list is open is shut    *
* first: killing one with its list standing wedges the program.                *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <localdefs.h>
#include <graphics.h>

void screen_capture(void);
void screen_capture_name(const char* fn);

#define MARGIN 10   /* white left around a figure */
#define GAP    16   /* space between widgets within a figure */
#define SETTLE 3000 /* settle time before a capture, 100us units */
#define STIM   9    /* timer the settle runs on */

static ami_evtrec er;
static ami_long framex, framey; /* where the client sits within the window */
static ami_long shotno;         /* figures taken so far */

/* Widgets are windows of their own and paint from events, so a picture taken
   the moment after a widget is placed catches it half drawn. Pump events for
   a moment and let the screen come to rest first. */

static void settle(void)

{

    ami_timer(stdout, STIM, SETTLE, FALSE);
    do ami_event(stdin, &er);
    while (er.etype != ami_ettim || er.timnum != STIM);

}

/* take one figure, and say which it was */
static void shot(
    /** What the figure shows */ const char* what
)

{

    settle();
    screen_capture();
    shotno++;
    fprintf(stderr, "%3lld  %s\n", AMI_LONG_CAST(shotno), what);

}

/* Cut the window down to the figure and clear it.
   setsizg() sets the whole window, so the client the figure needs is put
   through winclientg() to find the window that holds it. */

static void page(
    /** Client size wanted */ ami_long cw,
                              ami_long ch
)

{

    ami_long ww, wh; /* window size that holds it */

    ami_winclientg(stdout, cw+MARGIN*2, ch+MARGIN*2, &ww, &wh,
                   BIT(ami_wmframe)|BIT(ami_wmsize)|BIT(ami_wmsysbar));
    ami_setsizg(stdout, ww, wh);
    framex = (ww-(cw+MARGIN*2))/2;    /* the frame is even at the sides */
    framey = wh-(ch+MARGIN*2)-framex; /* and the rest of it is the bar */
    settle();
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    ami_fcolor(stdout, ami_black);

}

static void killall(
    /** Highest widget id in use */ ami_long n
)

{

    ami_long i;

    for (i = 1; i <= n; i++) ami_killwidget(stdout, i);

}

/* a list of three strings, for the widgets that take one */
static ami_strptr mklist(
    /** The strings */ char* a,
                       char* b,
                       char* c
)

{

    ami_strptr sp;  /* list being built */
    ami_strptr e;   /* entry */
    char*      v[3];
    int        i;

    v[0] = a; v[1] = b; v[2] = c;
    sp = NULL;
    for (i = 2; i >= 0; i--) {

        e = malloc(sizeof(ami_strrec));
        e->str = v[i];
        e->next = sp;
        sp = e;

    }

    return (sp);

}

/* Click a place in the figure. The rig takes window coordinates, so the frame
   is added to the figure's own. Nothing happens if no fifo is named. */

static void click(
    /** Where, in figure coordinates */ ami_long x,
                                        ami_long y
)

{

    const char* fn; /* the fifo's name */
    FILE*       f;

    fn = getenv("PD_INPUT");
    if (!fn) return;
    f = fopen(fn, "w");
    if (!f) return;
    fprintf(f, "btn 1 %lld %lld\n", AMI_LONG_CAST(x+framex), AMI_LONG_CAST(y+framey));
    fclose(f);

}

int main(int argc, char* argv[])

{

    ami_long   w, h;         /* widget size */
    ami_long   cw, ch;       /* closed size of a drop box */
    ami_long   ow, oh;       /* open size of a drop box */
    ami_long   sw, sh;       /* scroll bar thickness */
    ami_long   vw, vh;       /* vertical slider size */
    ami_long   bar;          /* how long a bar or slider is drawn */
    ami_long   i;
    ami_strptr lp;           /* string list */

    /* the window is made before main() is reached, and is held open at the
       end unless this is said, so it is said before anything can return */
    ami_autohold(FALSE);
    if (argc > 1) screen_capture_name(argv[1]);
    else {

        fprintf(stderr, "usage: wshot <file>\n");

        return (1);

    }
    if (!getenv("PD_INPUT"))
        fprintf(stderr,
                "wshot: PD_INPUT names no fifo: the two open drop box "
                "figures will come out closed\n");
    ami_title(stdout, "widget pictures");
    bar = ami_chrsizy(stdout)*8;

    /* ---- scroll bars, standing as they do around a client area */
    ami_scrollhorizsizg(stdout, &w, &sh);
    ami_scrollvertsizg(stdout, &sw, &h);
    page(sw+GAP+bar, bar+GAP+sh);
    ami_scrollvertg(stdout, MARGIN, MARGIN, MARGIN+sw, MARGIN+bar, 2);
    ami_scrollhorizg(stdout, MARGIN+sw+GAP, MARGIN+bar+GAP,
                     MARGIN+sw+GAP+bar, MARGIN+bar+GAP+sh, 1);
    ami_scrollsiz(stdout, 1, LONG_MAX/4);
    ami_scrollsiz(stdout, 2, LONG_MAX/4);
    ami_scrollpos(stdout, 2, LONG_MAX/3);
    shot("scroll bars, horizontal and vertical");
    killall(2);

    /* ---- the same bar at four slider sizes */
    page(sw*4+GAP*3, bar);
    for (i = 0; i < 4; i++) {

        ami_scrollvertg(stdout, MARGIN+(sw+GAP)*i, MARGIN,
                        MARGIN+(sw+GAP)*i+sw, MARGIN+bar, i+1);
        ami_scrollsiz(stdout, i+1, LONG_MAX/8*(i+1)*2);
        ami_scrollpos(stdout, i+1, LONG_MAX/5*(3-i));

    }
    shot("scroll bars at four slider sizes");
    killall(4);

    /* ---- a number select box */
    ami_numselboxsizg(stdout, 1, 10, &w, &h);
    page(w, h);
    ami_numselboxg(stdout, MARGIN, MARGIN, MARGIN+w, MARGIN+h, 1, 10, 1);
    ami_putwidgettext(stdout, 1, (char*)"3");
    shot("number select box");
    killall(1);

    /* ---- an edit box */
    ami_editboxsizg(stdout, (char*)"Hi there, george", &w, &h);
    page(w, h);
    ami_editboxg(stdout, MARGIN, MARGIN, MARGIN+w, MARGIN+h, 1);
    ami_putwidgettext(stdout, 1, (char*)"Hi there, george");
    shot("edit box");
    killall(1);

    /* ---- a list box */
    lp = mklist((char*)"Green", (char*)"Red", (char*)"Blue");
    ami_listboxsizg(stdout, lp, &w, &h);
    page(w, h);
    ami_listboxg(stdout, MARGIN, MARGIN, MARGIN+w, MARGIN+h, lp, 1);
    shot("list box");
    killall(1);

    /* ---- a drop box, shut and then standing open */
    lp = mklist((char*)"bird", (char*)"cat", (char*)"dog");
    ami_dropboxsizg(stdout, lp, &cw, &ch, &ow, &oh);
    page(cw, ch);
    ami_dropboxg(stdout, MARGIN, MARGIN, MARGIN+cw, MARGIN+ch, lp, 1);
    shot("drop box, closed");
    killall(1);
    /* the open figure is sized for the open box, the closed one is not */
    page(ow, oh);
    ami_dropboxg(stdout, MARGIN, MARGIN, MARGIN+cw, MARGIN+ch, lp, 1);
    settle();
    click(MARGIN+cw-ch/2, MARGIN+ch/2); /* the chevron: the face does not open it */
    shot("drop box, open");
    click(MARGIN+cw-ch/2, MARGIN+ch/2); /* shut it again before it is killed */
    settle();
    killall(1);

    /* ---- a drop edit box, the same two ways */
    lp = mklist((char*)"Tortillas", (char*)"flower", (char*)"corn");
    ami_dropeditboxsizg(stdout, lp, &cw, &ch, &ow, &oh);
    page(cw, ch);
    ami_dropeditboxg(stdout, MARGIN, MARGIN, MARGIN+cw, MARGIN+ch, lp, 1);
    ami_putwidgettext(stdout, 1, (char*)"Tortillas");
    shot("drop edit box, closed");
    killall(1);
    page(ow, oh);
    ami_dropeditboxg(stdout, MARGIN, MARGIN, MARGIN+cw, MARGIN+ch, lp, 1);
    ami_putwidgettext(stdout, 1, (char*)"Tortillas");
    settle();
    click(MARGIN+cw-ch/2, MARGIN+ch/2);
    shot("drop edit box, open");
    click(MARGIN+cw-ch/2, MARGIN+ch/2);
    settle();
    killall(1);

    /* ---- sliders, with tick marks and without */
    ami_slidehorizsizg(stdout, &w, &h);
    ami_slidevertsizg(stdout, &vw, &vh);
    page(bar+GAP*2+vw*2+GAP, bar);
    {

        /* the flat pair sit level with the middle of the upright pair */
        ami_long ty = MARGIN+(bar-h*2-GAP)/2;

        ami_slidehorizg(stdout, MARGIN, ty, MARGIN+bar, ty+h, 10, 1);
        ami_slidehorizg(stdout, MARGIN, ty+h+GAP, MARGIN+bar, ty+h*2+GAP, 0, 2);

    }
    ami_slidevertg(stdout, MARGIN+bar+GAP*2, MARGIN, MARGIN+bar+GAP*2+vw,
                   MARGIN+bar, 10, 3);
    ami_slidevertg(stdout, MARGIN+bar+GAP*3+vw, MARGIN,
                   MARGIN+bar+GAP*3+vw*2, MARGIN+bar, 0, 4);
    shot("sliders, with and without tick marks");
    killall(4);

    /* ---- the tab bars: first the four orientations, each on its own */
    {

        ami_strptr hp, vp;    /* tabs for the flat and upright bars */
        ami_long cwid, chgt;      /* client area asked of each bar */
        ami_long tw[4], th[4];    /* what each bar came back as */
        ami_long tox[4], toy[4];  /* and where its client sits within it */
        ami_long mid, colx;       /* where the middle column and row fall */
        ami_long pw, ph;          /* the figure's size */
        static const ami_tabori ori[4] =
            { ami_totop, ami_toright, ami_tobottom, ami_toleft };

        cwid = ami_chrsizy(stdout)*9;
        chgt = ami_chrsizy(stdout)*5;
        hp = mklist((char*)"Left", (char*)"Center", (char*)"Right");
        vp = mklist((char*)"Top", (char*)"Center", (char*)"Bottom");
        for (i = 0; i < 4; i++)
            ami_tabbarsizg(stdout, i&1? vp: hp, ori[i], cwid, chgt,
                           &tw[i], &th[i], &tox[i], &toy[i]);
        /* the flat pair stand above and below, the upright pair beside */
        pw = tw[3]+GAP+(tw[0] > tw[2]? tw[0]: tw[2])+GAP+tw[1];
        ph = th[0]+GAP+(th[3] > th[1]? th[3]: th[1])+GAP+th[2];
        page(pw, ph);
        colx = MARGIN+tw[3]+GAP;
        mid = MARGIN+th[0]+GAP;
        ami_tabbarg(stdout, colx, MARGIN, colx+tw[0], MARGIN+th[0], hp,
                    ami_totop, 1);
        ami_tabbarg(stdout, MARGIN, mid, MARGIN+tw[3], mid+th[3], vp,
                    ami_toleft, 2);
        ami_tabbarg(stdout, MARGIN+pw-tw[1], mid, MARGIN+pw, mid+th[1], vp,
                    ami_toright, 3);
        ami_tabbarg(stdout, colx, MARGIN+ph-th[2], colx+tw[2], MARGIN+ph, hp,
                    ami_tobottom, 4);
        shot("tab bars, the four orientations");
        killall(4);

        /* ---- and the same four laid over one client area */
        {

            ami_long ox[4], oy[4];             /* each bar's origin from the client */
            ami_long minx, miny, maxx, maxy;   /* what the four of them span */
            ami_long cx, cy;                   /* where the shared client falls */

            /* A bar is never smaller than its own tabs need, so the four of
               them want different clients from the same request. The largest
               of those is asked for again, and then all four agree. */
            for (i = 0; i < 4; i++) {

                if (!(i&1) && tw[i] > cwid) cwid = tw[i];
                if (i&1 && th[i] > chgt) chgt = th[i];

            }
            for (i = 0; i < 4; i++)
                ami_tabbarsizg(stdout, i&1? vp: hp, ori[i], cwid, chgt,
                               &tw[i], &th[i], &tox[i], &toy[i]);
            for (i = 0; i < 4; i++) { ox[i] = -tox[i]; oy[i] = -toy[i]; }
            minx = miny = 0;
            maxx = maxy = 0;
            for (i = 0; i < 4; i++) {

                if (ox[i] < minx) minx = ox[i];
                if (oy[i] < miny) miny = oy[i];
                if (ox[i]+tw[i] > maxx) maxx = ox[i]+tw[i];
                if (oy[i]+th[i] > maxy) maxy = oy[i]+th[i];

            }
            page(maxx-minx, maxy-miny);
            cx = MARGIN-minx;
            cy = MARGIN-miny;
            for (i = 0; i < 4; i++)
                ami_tabbarg(stdout, cx+ox[i], cy+oy[i], cx+ox[i]+tw[i],
                            cy+oy[i]+th[i], i&1? vp: hp, ori[i], i+1);
            shot("tab bars, the four over one client area");
            killall(4);

        }

    }

    /* ---- a background */
    page(ami_chrsizy(stdout)*10, ami_chrsizy(stdout)*6);
    ami_backgroundg(stdout, MARGIN, MARGIN, MARGIN+ami_chrsizy(stdout)*10,
                    MARGIN+ami_chrsizy(stdout)*6, 1);
    shot("background");
    killall(1);

    /* ---- a group box */
    ami_groupsizg(stdout, (char*)"Hello there", ami_chrsizy(stdout)*8,
                  ami_chrsizy(stdout)*4, &w, &h, &ow, &oh);
    page(w, h);
    ami_groupg(stdout, MARGIN, MARGIN, MARGIN+w, MARGIN+h,
               (char*)"Hello there", 1);
    shot("group box");
    killall(1);

    /* ---- a progress bar */
    ami_progbarsizg(stdout, &w, &h);
    page(bar, h);
    ami_progbarg(stdout, MARGIN, MARGIN, MARGIN+bar, MARGIN+h, 1);
    ami_progbarpos(stdout, 1, LONG_MAX/3);
    shot("progress bar");
    killall(1);

    return (0);

}
