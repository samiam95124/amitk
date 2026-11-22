/*******************************************************************************
*                                                                              *
*                          TEXT PRINT OUTPUT TEST                              *
*                                                                              *
* Exercises the txtterminal module: opens a print file, writes plain text,     *
* positions text with the cursor calls, runs the tabs, backspace and           *
* overwrite, resizes the page, and leaves the last page for the close to       *
* eject. The result is txttest.txt in the current directory; an argument       *
* replaces the destination, so a printer takes it with "txttest lp0:".        *
*                                                                              *
* The page is the default 80 by 25 characters until sizbuf() changes it.       *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <terminal.h>

/* place a string at a position */
static void at(FILE* p, ami_long x, ami_long y, const char* s)

{

    ami_cursor(p, x, y);
    fprintf(p, "%s", s);

}

/* center a string on a line */
static void center(FILE* p, ami_long y, const char* s)

{

    at(p, (ami_maxx(p)-(ami_long)strlen(s))/2+1, y, s);

}

int main(int argc, char* argv[])

{

    FILE* p;
    ami_long  mx, my;
    ami_long  i;

    /* the output is the print file, not this window: do not hold it on exit */
    ami_autohold(FALSE);
    ami_openprint(&p, argc > 1? argv[1]: "txttest.txt");
    mx = ami_maxx(p);
    my = ami_maxy(p);

    /* ==================== page 1: text and positioning ==================== */

    fprintf(p, "txtterminal test, page 1: text and positioning\n");
    fprintf(p, "the page is %lld x %lld characters\n", AMI_LONG_CAST(mx), AMI_LONG_CAST(my));
    fprintf(p, "\n");
    fprintf(p, "plain stream text: the quick brown fox jumps over the lazy "
               "dog 0123456789\n");
    /* positioned text: the four corners and the center */
    center(p, 8, "centered on line eight");
    at(p, 1, 10, "left");
    at(p, mx-4, 10, "right");
    at(p, 30, 12, "positioned at column thirty");
    /* a right ragged column set by cursor, out of order on purpose */
    at(p, 60, 16, "third");
    at(p, 60, 14, "first");
    at(p, 60, 15, "second");
    /* tabs */
    at(p, 1, 18, "tab stops:");
    ami_cursor(p, 1, 19);
    fprintf(p, "1\t2\t3\t4\t5\t6\n");
    fprintf(p, "one\ttwo\tthree\tfour\tfive\tsix\n");
    /* the bottom line, last column held back from the corner */
    at(p, 1, my, "bottom line");
    putc('\f', p);

    /* ================ page 2: movement, overwrite, correction ============= */

    fprintf(p, "txtterminal test, page 2: movement and overwrite\n\n");
    /* a frame drawn with the cursor calls */
    at(p, 10, 4, "+----------------------------+");
    for (i = 5; i <= 10; i++) {

        at(p, 10, i, "|");
        at(p, 39, i, "|");

    }
    at(p, 10, 11, "+----------------------------+");
    at(p, 14, 7, "framed by cursor moves");
    /* overwrite: strike a word over with dashes */
    at(p, 10, 14, "this word gets overwritten");
    at(p, 15, 14, "----");
    /* backspace correction: type, back up, retype */
    at(p, 10, 16, "correctxx");
    ami_del(p);
    ami_del(p);
    fprintf(p, "ed");
    /* relative movement: steps of a staircase */
    ami_cursor(p, 10, 19);
    for (i = 0; i < 5; i++) {

        fprintf(p, "*");
        ami_down(p);

    }
    at(p, 20, 19, "a staircase by down() calls");
    putc('\f', p);

    /* ==================== page 3: the page resized ======================== */

    /* the buffer size is the page size */
    ami_sizbuf(p, 100, 50);
    mx = ami_maxx(p);
    my = ami_maxy(p);
    fprintf(p, "txtterminal test, page 3: the page resized to %lld x %lld\n\n",
            AMI_LONG_CAST(mx), AMI_LONG_CAST(my));
    at(p, 81, 4, "past column eighty");
    center(p, 6, "centered on the hundred column page");
    at(p, 1, my, "line fifty, the new bottom");
    at(p, 1, 8, "this page was not ejected by the program; the close of the "
                "print file ejects it.");

    fclose(p);
    printf("print complete to %s\n", argc > 1? argv[1]: "txttest.txt");

    return (0);

}
