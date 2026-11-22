/*******************************************************************************
*                                                                              *
*                        MENU BAR THEN RESIZE WINDOW                           *
*                                                                              *
* Builds a menu on the main window, then makes the window wider. The menu      *
* strip follows the window, but what the widening exposes is never painted:    *
* the right end of the menu bar sits there as a black box, and stays until     *
* something makes the window resize again.                                     *
*                                                                              *
* Options:                                                                     *
*                                                                              *
*     after    build the menu after the resize instead, which is the           *
*              workaround: the strip is created at the full width and is       *
*              painted whole                                                   *
*     narrow   size the window narrower rather than wider, for contrast:      *
*              shrinking exposes nothing, so there is nothing unpainted        *
*     child    after the resize, draw into a child pane before anything is     *
*              drawn on the main window, which is what a program with panes    *
*              does: the panes map the parent chain before the parent has      *
*              drawn a thing                                                   *
*     binvis   set background invisible mode on the main window first          *
*     buf      the pane is buffered, as a program keeping its panes painted    *
*              would have it                                                   *
*     widget   put a scroll bar in the pane, which brings the widget           *
*              machinery in                                                    *
*     small    grow the window by twenty pixels rather than two hundred        *
*                                                                              *
* The program draws a line of text under the menu and holds. What to look at   *
* is the right end of the menu bar: white is a pass, a black box is the        *
* issue.                                                                       *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>

#include <localdefs.h>
#include <graphics.h>

int main(int argc, char* argv[])

{

    ami_menuptr sm = NULL;
    ami_evtrec  er;
    ami_long    wx, wy;
    ami_long    i;
    int         after = FALSE;
    int         narrow = FALSE;
    int         child = FALSE;
    int         binvis = FALSE;
    int         buf = FALSE;
    int         widget = FALSE;
    ami_long    wide = 1500;
    ami_long    tall = 600;
    FILE*       pane;

    for (i = 1; i < argc; i++) {

        if (!strcmp(argv[i], "after")) after = TRUE;
        else if (!strcmp(argv[i], "narrow")) narrow = TRUE;
        else if (!strcmp(argv[i], "child")) child = TRUE;
        else if (!strcmp(argv[i], "binvis")) binvis = TRUE;
        else if (!strcmp(argv[i], "small")) { wide = 1300; tall = 920; }
        else if (!strcmp(argv[i], "buf")) { child = TRUE; buf = TRUE; }
        else if (!strcmp(argv[i], "widget")) { child = TRUE; widget = TRUE; }

    }
    ami_autohold(FALSE);
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    if (binvis) ami_binvis(stdout);

    /* a standard menu, nothing else: the strip is the subject here */
    if (!after) ami_stdmenu(BIT(AMI_SMEXIT) | BIT(AMI_SMABOUT), &sm, NULL);

    /* make the window a different width than it was born with */
    ami_winclientg(stdout, narrow? 600: wide, tall, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(stdout, wx, wy);

    if (after) ami_stdmenu(BIT(AMI_SMEXIT) | BIT(AMI_SMABOUT), &sm, NULL);

    if (child) { /* the pane draws first, as a program with panes does */

        ami_openwin(&stdin, &pane, stdout, 2);
        ami_frame(pane, FALSE);
        if (buf) ami_buffer(pane, TRUE);
        if (widget) {

            ami_long sw, sh;

            ami_scrollvertsizg(pane, &sw, &sh);
            ami_scrollvertg(pane, 1, 1, sw, 200, 1);

        }
        ami_setsizg(pane, 300, 300);
        if (buf) ami_sizbufg(pane, 300, 300);
        ami_setposg(pane, 1, 100);
        fprintf(pane, "\fthe pane\n");

    }
    fprintf(stdout, "\f");
    ami_cursorg(stdout, 10, 10);
    fprintf(stdout, "Look at the right end of the menu bar above.\n");
    fprintf(stdout, "White all the way across is a pass; a black box is "
                    "the issue.\n");

    do { ami_event(stdin, &er); } while (er.etype != ami_etterm);

    return (0);

}
