/*******************************************************************************
*                                                                              *
*                       BUFFERED WINDOW THEN OPEN WINDOW                       *
*                                                                              *
* Opens a child window, turns buffering on for it, then opens a second         *
* window. The second open never returns.                                       *
*                                                                              *
* Each step is announced before it is taken, so the last line printed is the    *
* call that did not come back.                                                 *
*                                                                              *
* Options, to narrow it down:                                                  *
*                                                                              *
*     nobuf     do not turn buffering on for the child                         *
*     noframe   turn the child's frame off, as a pane would                    *
*     toplevel  make the first window a top level one rather than a child      *
*     presize   size the window before buffering, so the configure is a change  *
*     samesize  ask for the size the window already has, twice                  *
*     samepos   ask for the place the window is already in, twice               *
*                                                                              *
* Build:                                                                       *
*                                                                              *
*     gcc -Iinclude -Ilibc tests/source/bufopen.c stub/keeper.o \              *
*         lib/libami_graph.a $(the graphics libraries) -o bin/bufopen          *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>

#include <localdefs.h>
#include <graphics.h>

#define WIN1 2 /* the window that gets buffered */
#define WIN2 3 /* the window that cannot be opened after it */

int main(int argc, char* argv[])

{

    FILE* w1;
    FILE* w2;
    ami_long  i;
    int   nobuf = FALSE;
    int   noframe = FALSE;
    int   toplevel = FALSE;
    int   presize = FALSE;
    int   samesize = FALSE;
    int   samepos = FALSE;

    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "nobuf")) nobuf = TRUE;
        else if (!strcmp(argv[i], "noframe")) noframe = TRUE;
        else if (!strcmp(argv[i], "toplevel")) toplevel = TRUE;
        else if (!strcmp(argv[i], "presize")) presize = TRUE;
        else if (!strcmp(argv[i], "samesize")) samesize = TRUE;
        else if (!strcmp(argv[i], "samepos")) samepos = TRUE;

    }
    ami_autohold(FALSE);
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    printf("main window up\n");

    fprintf(stderr, "open window 1\n");
    ami_openwin(&stdin, &w1, toplevel? NULL: stdout, WIN1);
    fprintf(stderr, "window 1 open\n");

    if (noframe) {

        fprintf(stderr, "frame off\n");
        ami_frame(w1, FALSE);

    }
    if (presize) {

        /* make the window a size other than the one buffering will ask
           for, so that the configure buffering does is a real change */
        fprintf(stderr, "pre-size\n");
        ami_setsizg(w1, 400, 300);

    }
    if (!nobuf) {

        fprintf(stderr, "buffer on\n");
        ami_buffer(w1, TRUE);
        fprintf(stderr, "buffer on done\n");

    }
    if (samesize) { /* the size it already has, twice */

        fprintf(stderr, "size 500x400\n");
        ami_setsizg(w1, 500, 400);
        fprintf(stderr, "size 500x400 again\n");
        ami_setsizg(w1, 500, 400);
        fprintf(stderr, "same size done\n");

    }
    if (samepos) { /* the place it is already in, twice */

        fprintf(stderr, "position 60,60\n");
        ami_setposg(w1, 60, 60);
        fprintf(stderr, "position 60,60 again\n");
        ami_setposg(w1, 60, 60);
        fprintf(stderr, "same position done\n");

    }
    ami_setsizg(w1, 300, 200);
    ami_setposg(w1, 10, 10);
    fprintf(w1, "\fwindow 1\n");
    fprintf(stderr, "window 1 drawn\n");

    /* this is the one that does not come back */
    fprintf(stderr, "open window 2\n");
    ami_openwin(&stdin, &w2, NULL, WIN2);
    fprintf(stderr, "window 2 open\n");

    ami_setsizg(w2, 300, 200);
    ami_setposg(w2, 350, 10);
    fprintf(w2, "\fwindow 2\n");
    fprintf(stderr, "done\n");

    fclose(w2);
    fclose(w1);

    return (0);

}
