/*******************************************************************************

A progress bar set to a position outside its range stops the window
reporting resize -- for good

Open a sizable window, put a progress bar in it, and set the bar to a
position outside the range it takes. From that moment the window never
reports another ami_etresize: the window manager resizes it, the picture
is clipped to what it was, and nothing arrives. Everything else about
the window goes on working -- it draws, it reports redraw, it answers
the mouse -- so the program looks like one that has decided to ignore
being resized.

A position is a fraction of the whole range of a long, so LONG_MAX is
full and 0 is empty. LONG_MIN is not a position, and a program can
arrive at one easily: (double)pos/max*(double)LONG_MAX comes to exactly
2^63 when pos reaches max, which is one more than a long holds, and
converting it is undefined -- on this machine it is LONG_MIN. That is
how this was found.

Out of range is out of range, and the library is entitled to refuse it.
What it should not do is take the window's resize reporting away and
never give it back, silently, so that the fault shows up somewhere else
entirely.

    progresize bar follow move        positions all within range
    progresize bar follow move over   one position is LONG_MIN

Each resize seen is printed. The first prints three, the second none --
not even the resize that arrives before the bad position is ever set.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <unistd.h>

#include <localdefs.h>
#include <graphics.h>
#include <services.h>

#define PROGID 1
#define TIMMOVE 1

/* The other thread: it reads a file, over and over, and touches nothing
   the display thread has. This is what a fetch does while the window
   waits for its next event. */
static void reader(void)

{

    char  buf[65536];
    ami_long  n;

    while (TRUE) {

        FILE* f = fopen("/home/samiam/projects/amitk/graph_programs/mail.c",
                        "r");

        if (f) {

            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) ;
            fclose(f);

        }
        usleep(1000);

    }

}

int main(int argc, char* argv[])

{

    ami_evtrec er;
    ami_long   w, h;
    ami_long   moving = FALSE;  /* the bar is moved along on a timer */
    ami_long   follow = FALSE;  /* buffer follow, as a resizable program does */
    ami_long   replace = FALSE; /* and the bar is put in its place again */
    ami_long   work = FALSE;    /* a second thread reads files, as a fetch does */
    ami_long   menu = FALSE;    /* the window carries a menu bar */
    ami_long   overflow = FALSE;/* the position goes negative now and then */
    ami_long   full = FALSE;    /* and now and then it is the whole bar */
    ami_long   bar = FALSE;     /* there is a progress bar in the window */
    ami_long   i;
    ami_long   resizes = 0;
    ami_long   step = 0;

    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "move")) moving = TRUE;
        if (!strcmp(argv[i], "follow")) follow = TRUE;
        if (!strcmp(argv[i], "replace")) replace = TRUE;
        if (!strcmp(argv[i], "work")) work = TRUE;
        if (!strcmp(argv[i], "menu")) menu = TRUE;
        if (!strcmp(argv[i], "over")) overflow = TRUE;
        if (!strcmp(argv[i], "full")) full = TRUE;
        if (!strcmp(argv[i], "bar")) bar = TRUE;

    }
    ami_title(stdout, "Progress and resize");
    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    if (menu) { /* one item is enough to raise the strip */

        static ami_menurec one;

        one.next = NULL;
        one.branch = NULL;
        one.onoff = FALSE;
        one.oneof = FALSE;
        one.bar = FALSE;
        one.id = 100;
        one.face = "Something";
        ami_menu(stdout, &one);

    }
    if (work) ami_newthread(reader);
    ami_progbarsizg(stdout, &w, &h);
    /* the bar is only made if it is wanted, so that what its presence
       alone changes can be seen */
    if (bar) ami_progbarg(stdout, 20, 20, 20+w, 20+h, PROGID);
    fprintf(stderr, "moved=%lld follow=%lld replace=%lld\n", AMI_LONG_CAST(moving), AMI_LONG_CAST(follow),
            AMI_LONG_CAST(replace));
    /* stdout is the window in a graphics program, so anything meant to be
       read goes to the error stream */
    /* a tenth of a second, in hundred microsecond counts */
    ami_timer(stdout, TIMMOVE, 1000, TRUE);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etresize || er.etype == ami_etredraw) {

            /* A band drawn at the foot of the window, and the bar put at
               the same place. If drawing and widget placing measure from
               the same origin the bar sits in the band. */
            ami_fcolorc(stdout, 255, 0, 0);
            ami_frect(stdout, 1, ami_maxyg(stdout)-30, ami_maxxg(stdout),
                      ami_maxyg(stdout));
            ami_fcolor(stdout, ami_black);
            if (bar) {

                ami_poswidgetg(stdout, PROGID, 20, ami_maxyg(stdout)-28);
                ami_sizwidgetg(stdout, PROGID, w, h);

            }
            fprintf(stderr, "band and bar at y %lld, window says %lldx%lld\n",
                    AMI_LONG_CAST(ami_maxyg(stdout)-30), AMI_LONG_CAST(ami_maxxg(stdout)),
                    AMI_LONG_CAST(ami_maxyg(stdout)));

    }
    if (er.etype == ami_etresize) {

            resizes++;
            fprintf(stderr, "resize %lld: win %lld, %lldx%lld\n", AMI_LONG_CAST(resizes),
                    AMI_LONG_CAST(er.winid), AMI_LONG_CAST(er.rszxg), AMI_LONG_CAST(er.rszyg));
            /* what a program that lays itself out again does */
            if (follow) ami_sizbufg(stdout, er.rszxg, er.rszyg);
            if (replace) {

                ami_poswidgetg(stdout, PROGID, ami_maxxg(stdout)-w-8,
                               ami_maxyg(stdout)-h-8);
                ami_sizwidgetg(stdout, PROGID, w, h);

            }

        }
        if (er.etype == ami_ettim && moving && bar) {

            step = (step+1)%20;
            /* What a position worked out as (double)pos/max*(double)LONG_MAX
               comes to when pos reaches max: the product is 2^63, which
               is one more than a long can hold, and converting it is
               undefined -- on this machine it is LONG_MIN. */
            if (full && step == 0) ami_progbarpos(stdout, PROGID, LONG_MAX);
            else if (overflow && step == 0)
                ami_progbarpos(stdout, PROGID, LONG_MIN);
            else ami_progbarpos(stdout, PROGID, LONG_MAX/20*step);

        }

    } while (er.etype != ami_etterm);
    ami_killtimer(stdout, TIMMOVE);

    return (0);

}
