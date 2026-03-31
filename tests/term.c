/*******************************************************************************

TERMINAL EMULATOR

This is a simple program written to the Petit Ami terminal standard that just
allows the user to scribble text on the screen. Implements the basic positioning
keys, etc.

This is used mostly for testing.

*******************************************************************************/

#include "terminal.h" /* terminal level functions */

int main(int argc, char *argv[])

{

    ami_evtrec er;   /* record for returned events */
    int autostate;  /* state of automatic wrap and scroll */
    long buf;       /* current terminal buffer */
    int fbold;      /* bold active flag */
    int fundl;      /* underline active flag */
    int fstko;      /* strikeout active flag */
    int fital;      /* italic active flag */
    int fsubs;      /* subscript active flag */
    int fsups;      /* superscript active flag */
    ami_color color; /* current color */

    buf = 1;          /* set normal buffer */
    fbold = 0;        /* set bold off */
    fundl = 0;        /* set underline off */
    fstko = 0;        /* set strikeout off */
    fital = 0;        /* set italic off */
    fsubs = 0;        /* set subscript off */
    fsups = 0;        /* set superscript off */
    autostate = 1;    /* set auto on */
    color = ami_black; /* set black foreground */
    printf("Terminal emulator test program 1.0\n");
    do { /* event loop */

        ami_event(stdin, &er); /* get the next event */
        switch (er.etype) {   /* event */

             /* pass character to output */
             case ami_etchar: putchar(er.echar); break;
             /* enter line */
             case ami_etenter: ami_cursor(stdout, 1, ami_cury(stdout)); ami_down(stdout); break;
             case ami_etup:    ami_up(stdout); break; /* up one line */
             case ami_etdown:  ami_down(stdout); break; /* down one line */
             case ami_etleft:  ami_left(stdout); break; /* left one character */
             case ami_etright: ami_right(stdout); break; /* right one character */
             case ami_ethomes: ami_home(stdout); break; /* home screen */
             /* home line */
             case ami_ethomel: ami_cursor(stdout, 1, ami_cury(stdout)); break;
             /* end screen */
             case ami_etends:  ami_cursor(stdout, ami_maxx(stdout), ami_maxy(stdout)); break;
             /* end line */
             case ami_etendl:  ami_cursor(stdout, ami_maxx(stdout), ami_cury(stdout)); break;
             case ami_etscrl:  ami_scroll(stdout, -1, 0); break; /* scroll left */
             case ami_etscrr:  ami_scroll(stdout, 1, 0); break; /* scroll right */
             case ami_etscru:  ami_scroll(stdout, 0, -1); break; /* scroll up */
             case ami_etscrd:  ami_scroll(stdout, 0, 1); break; /* scroll down */
             case ami_etdelcb: ami_del(stdout); break;  /* delete left character */
             case ami_ettab:   printf("\t"); break;/* tab */
             case ami_etinsertt: autostate = !autostate;
                 ami_auto(stdout, autostate); break;
             case ami_etfun:  /* function key */

                 if (er.fkey == 1) {  /* function 1: swap screens */

                     if (buf == 10) buf = 1;  /* wrap buffer back to zero */
                     else buf++; /* next buffer */
                     ami_select(stdout, buf, buf);

                 } else if (er.fkey == 2) {  /* function 2: bold toggle */

                     fbold = !fbold;   /* toggle */
                     ami_bold(stdout, fbold); /* apply */

                 } else if (er.fkey == 3) {  /* function 3: underline toggle */

                     fundl = !fundl; /* toggle */
                     ami_underline(stdout, fundl); /* apply */

                 } else if (er.fkey == 4) {  /* function 4: strikeout toggle */

                     fstko = !fstko; /* toggle */
                     ami_strikeout(stdout, fstko); /* apply */


                 } else if (er.fkey == 5) {  /* function 5: italic toggle */

                     fital = !fital;   /* toggle */
                     ami_italic(stdout, fital); /* apply */

                 } else if (er.fkey == 6) {  /* function 6: subscript toggle */

                     fsubs = !fsubs;   /* toggle */
                     ami_subscript(stdout, fsubs); /* apply */

                 } else if (er.fkey == 7) {  /* function 7: superscript toggle */

                     fsups = !fsups;   /* toggle */
                     ami_superscript(stdout, fsups); /* apply */

                 } else if (er.fkey == 8) {

                     color++; /* next color */
                     /* wrap color */
                     if (color > ami_magenta) color = ami_black;
                     ami_bcolor(stdout, color);

                 }
                 break;
             default: ;

        }

    } while (er.etype != ami_etterm); /* until termination signal */

    return (0); /* return no error */

}
