/*******************************************************************************
*                                                                              *
*                       BUFFERED, AND FOLLOWING THE BUFFER                     *
*                                                                              *
* Shows what a window answers about its own size in the two modes, which is    *
* easy to get wrong: a window comes up buffered, and in buffered mode          *
* ami_maxxg reports the BUFFER, not the window on screen. Resizing the window  *
* does not change the buffer, so the answer does not change either. That is    *
* the mode working, not the window failing to notice.                          *
*                                                                              *
* The resize event is the window saying how big the display now is, so that    *
* the program can choose: leave the buffer as it is and let the window be a    *
* window onto it, or follow it with ami_sizbufg, after which ami_maxxg         *
* reports the new size. The manual calls the second one buffer follow mode.    *
*                                                                              *
* Run it and drag the window's frame:                                          *
*                                                                              *
*     bin/wmresize            buffered, buffer left alone                      *
*     bin/wmresize follow     buffered, buffer follows the window              *
*     bin/wmresize nobuf      unbuffered, where maxxg is the window            *
*                                                                              *
* Every event is printed with the size it carries beside what the window       *
* answers when asked. In the first, the two part company after a resize and    *
* stay parted. In the other two they agree.                                    *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>

#include <localdefs.h>
#include <graphics.h>

#define PANE 2 /* the child pane */

/* paint the pane so its edges can be seen */
static void paint(FILE* f, ami_long w, ami_long h)

{

    fprintf(f, "\f");
    ami_fcolor(f, ami_cyan);
    ami_frect(f, 0, 0, w-1, h-1);
    ami_fcolor(f, ami_black);
    ami_cursorg(f, 10, 10);
    fprintf(f, "pane painted for %lld by %lld\n", AMI_LONG_CAST(w), AMI_LONG_CAST(h));

}

int main(int argc, char* argv[])

{

    FILE*      pane;
    ami_evtrec er;
    ami_long   wx, wy;
    ami_long   w, h;
    ami_long   i;
    ami_long   n = 0;
    int        follow = FALSE;
    int        nobuf = FALSE;

    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "follow")) follow = TRUE;
        else if (!strcmp(argv[i], "nobuf")) nobuf = TRUE;

    }
    ami_autohold(FALSE);
    if (nobuf) ami_buffer(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_winclientg(stdout, 700, 500, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(stdout, wx, wy);
    fprintf(stdout, "\f");
    ami_cursorg(stdout, 10, 10);
    fprintf(stdout, "Drag the frame. Every event is printed on stderr.\n");

    /* a pane inside it, as a program with panes has */
    ami_openwin(&stdin, &pane, stdout, PANE);
    ami_frame(pane, FALSE);
    ami_auto(pane, FALSE);
    ami_curvis(pane, FALSE);
    w = ami_maxxg(stdout)-40;
    h = ami_maxyg(stdout)-80;
    ami_setposg(pane, 20, 60);
    ami_setsizg(pane, w, h);
    paint(pane, w, h);

    fprintf(stderr, "%s\n", nobuf? "unbuffered: maxxg is the window":
                            follow? "buffered, following the buffer":
                                    "buffered, buffer left alone");
    do {

        ami_event(stdin, &er);
        /* everything, so that nothing can be said to have been filtered */
        fprintf(stderr, "%4lld: event %2d  window %lld  size %lldx%lld  "
                "main says %lldx%lld\n", AMI_LONG_CAST(++n), (int)er.etype, AMI_LONG_CAST(er.winid),
                AMI_LONG_CAST(er.rszxg), AMI_LONG_CAST(er.rszyg), AMI_LONG_CAST(ami_maxxg(stdout)), AMI_LONG_CAST(ami_maxyg(stdout)));
        /* Only the main window's own resizes: the pane is resized here
           too, and its resize comes back as an event like any other. A
           program that acts on those resizes the pane again on the
           strength of the pane's own event, which chases itself down to
           a negative size. */
        if (er.etype == ami_etresize && er.winid == 1) {

            /* Follow the buffer, or do not. Following throws the old
               buffer away and makes one the size of the window, after
               which maxxg agrees with the event. Not following leaves
               the program with the surface it had, which the window
               shows as much of as it can. */
            if (follow) ami_sizbufg(stdout, er.rszxg, er.rszyg);
            w = er.rszxg-40;
            h = er.rszyg-80;
            ami_setsizg(pane, w, h);
            if (follow) ami_sizbufg(pane, w, h);
            paint(pane, w, h);

        }

    } while (er.etype != ami_etterm);

    return (0);

}
