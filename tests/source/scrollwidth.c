/*******************************************************************************

A scroll bar on a child window is built as wide as the window

Open a framed window under windowc and put a vertical scroll bar one
column wide against its right edge, the way a reader pane does. The bar
is never seen: the widget's face is created at the full width of its
owner rather than the width asked for, so the bar's drawing lands in a
buffer of which one column shows -- and that column is empty.

The program draws a ruler of dots in every client row up to the column
the bar should start at, so what the right edge holds is plain: it
should be the bar's arrow, thumb and well characters, and it is blanks.

The same request against the root window draws correctly, which is how
this hid inside a mail program for a while: the message list's bar (on
the root) was fine while the reader's bar (on a child window) vanished,
and the child window was blamed.

Build as a character program against the manager:

    gcc $(CFLAGS) tests/source/scrollwidth.c $(CLIBSC) -o bin/scrollwidth

Run it in a terminal. Expected: a bar column on the window's right
edge, thumb at about half way. Observed: nothing.

The width the manager records for the widget can be seen by
instrumenting wigdrw: asked for x1 98, x2 98 in a client 98 wide, it
reports the face at 98 by 26 -- the width of the owner, not of the
rectangle given.

*******************************************************************************/

#include <stdio.h>
#include <limits.h>

#include <localdefs.h>
#include <terminalw.h>

#define BARID 1

int main(void)

{

    ami_evtrec er;
    FILE*      win;
    ami_long   sbw, sbh;
    ami_long   x, y;

    ami_openwin(&stdin, &win, NULL, 2);
    ami_title(win, "Scroll bar width");
    ami_auto(win, FALSE);
    ami_curvis(win, FALSE);
    ami_setsiz(win, 60, 20);
    ami_setpos(win, 3, 3);
    ami_scrollvertsiz(win, &sbw, &sbh);
    ami_scrollvert(win, ami_maxx(win)-sbw+1, 1, ami_maxx(win),
                   ami_maxy(win), BARID);
    /* a thumb of a quarter, half way down, so there is something to see */
    ami_scrollsiz(win, BARID, LONG_MAX/4);
    ami_scrollpos(win, BARID, LONG_MAX/2);
    /* the ruler: dots to the bar's column, so the empty column shows */
    for (y = 1; y <= ami_maxy(win); y++) {

        ami_cursor(win, 1, y);
        for (x = 1; x <= ami_maxx(win)-sbw; x++) fputc('.', win);

    }
    ami_cursor(win, 2, 2);
    fprintf(win, " the bar belongs on the right edge -> ");
    do { ami_event(stdin, &er); } while (er.etype != ami_etterm);

    return (0);

}
