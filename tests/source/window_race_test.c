/*******************************************************************************
*                                                                             *
*                        WINDOW RACE TEST PROGRAM                             *
*                                                                             *
*                     Copyright (C) 2026 Scott A. Franco                      *
*                                                                             *
* Makes and frees child windows in a tight cycle, each one titled, placed,    *
* sized and drawn on, to press on the place where a display's event pump and  *
* the thread making the calls meet. A window's frame and its title are drawn  *
* by the pump when the display asks for them, and by this thread when the     *
* program asks: a window being made or freed while the pump is drawing it is  *
* a use after free, and the crash lands wherever the next allocation is,      *
* which is why the fault reads as anything but what it was.                   *
*                                                                             *
* The path this presses on:                                                   *
*                                                                             *
*   evpump -> ami_event -> xwinevt -> txt2win -> fileno                       *
*                                                                             *
* Run it against a remote display (window_race_testr with graph_server up)    *
* and the two threads are the server's own, which is where the fault was      *
* first seen; run it locally and the same calls are exercised against the     *
* display library directly.                                                   *
*                                                                             *
* Execution:                                                                  *
*                                                                             *
*   window_race_test [<cycles>]                                               *
*                                                                             *
* The cycle count defaults to 50, each cycle making and freeing three child   *
* windows. There is no input and nothing to press: the test runs to the end   *
* and says what it found. It passes by completing -- a race of this kind      *
* announces itself by crashing -- and by the parent window still answering    *
* for its size at the end of every cycle, which a display left in a bad       *
* state does not.                                                             *
*                                                                             *
* Returns 0 on pass, 1 on fail.                                               *
*                                                                             *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Petit-Ami defines */
#include <localdefs.h>
#include <graphics.h>

#define OFF 0
#define ON  1

#define DEFCYC  50 /* cycles if none is given */
#define MAXCYC  100000 /* sanity bound */
#define CHILDS  3  /* child windows per cycle */

static int fails; /* count of failures */

/* Say something, to the window and to the terminal both. stdout is the
   window here, and what is written there is gone when the window goes, so
   a harness running this sees nothing at all. stderr is left alone by the
   library and is where the verdict has to go. */
static void note(const char* s)

{

    printf("%s\n", s);
    fflush(stdout);
    fprintf(stderr, "%s\n", s);
    fflush(stderr);

}

/* report a failure, naming the cycle it happened in */
static void fail(int cyc, const char* what)

{

    char b[160];

    snprintf(b, sizeof(b), "window_race_test: *** fail *** cycle %d: %s",
             cyc, what);
    note(b);
    fails++;

}

int main(int argc, char* argv[])

{

    FILE* win[CHILDS];
    ami_long  cycles, c;
    int   i, x, y;
    int   basex, basey;
    char  ts[160];

    cycles = DEFCYC;
    if (argc > 2) {

        fprintf(stderr, "Usage: window_race_test [<cycles>]\n");
        return (1);

    }
    if (argc == 2) {

        char* ep;

        cycles = strtol(argv[1], &ep, 10);
        if (*ep || cycles < 1 || cycles > MAXCYC) {

            fprintf(stderr, "Invalid cycle count\n");
            return (1);

        }

    }

    ami_title(stdout, "Window race test");
    ami_curvis(stdout, OFF);
    ami_auto(stdout, OFF);
    /* the test ends when the cycles end: there is nobody here to close the
       window, and the hold for a program that exits on its own would leave
       it standing forever */
    ami_autohold(FALSE);

    /* the parent's size is the standard the cycles are judged against: a
       display left in a bad state stops answering for it, or answers with
       something else */
    basex = ami_maxx(stdout);
    basey = ami_maxy(stdout);
    snprintf(ts, sizeof(ts), "window_race_test: %lld cycles, %d children each,"
             " parent %d by %d", AMI_LONG_CAST(cycles), CHILDS, basex, basey);
    note(ts);

    for (c = 1; c <= cycles; c++) {

        /* make them, titled, placed and sized: the title is what the pump
           draws, the size is what makes the display send the events that
           set it drawing */
        for (i = 0; i < CHILDS; i++) {

            win[i] = NULL;
            ami_openwin(&stdin, &win[i], stdout, i+2);
            if (!win[i]) { fail((int)c, "child window did not open"); break; }
            ami_curvis(win[i], OFF);
            snprintf(ts, sizeof(ts), "child %d of cycle %lld", i+1, AMI_LONG_CAST(c));
            ami_title(win[i], ts);
            ami_setpos(win[i], 1+i*21, 5);
            ami_sizbuf(win[i], 20, 8);
            ami_setsiz(win[i], 20, 8);
            ami_bcolor(win[i], i == 0? ami_cyan:
                               i == 1? ami_yellow: ami_magenta);
            putc('\f', win[i]);
            fprintf(win[i], "cycle %lld\nchild %d\n", AMI_LONG_CAST(c), i+1);
            fflush(win[i]);

        }

        /* draw on the parent as well, so this thread is inside the display
           library while the pump has frames of its own to paint */
        ami_fcolor(stdout, ami_black);
        ami_line(stdout, 1, 1, ami_maxxg(stdout), ami_maxyg(stdout));

        /* retitle them: a title is measured and drawn as one, and it is the
           font that the two threads share */
        for (i = 0; i < CHILDS; i++) if (win[i]) {

            snprintf(ts, sizeof(ts), "retitled %d/%lld", i+1, AMI_LONG_CAST(c));
            ami_title(win[i], ts);

        }

        /* and free them, in the reverse order to the making, so the list is
           unwound as well as walked */
        for (i = CHILDS-1; i >= 0; i--) if (win[i]) {

            fclose(win[i]);
            win[i] = NULL;

        }

        /* the parent is still there and still knows its size */
        x = ami_maxx(stdout);
        y = ami_maxy(stdout);
        if (x != basex || y != basey) {

            snprintf(ts, sizeof(ts), "parent size went from %d by %d to %d by %d",
                     basex, basey, x, y);
            fail((int)c, ts);

        }
        if (c % 10 == 0 || c == cycles) {

            snprintf(ts, sizeof(ts), "window_race_test: %lld of %lld cycles",
                     AMI_LONG_CAST(c), AMI_LONG_CAST(cycles));
            note(ts);

        }
        if (fails) break; /* no sense grinding on a broken display */

    }

    if (fails) {

        snprintf(ts, sizeof(ts), "window_race_test: *** fail *** (%d)", fails);
        note(ts);

    } else note("window_race_test: pass");

    return (fails? 1: 0);

}
