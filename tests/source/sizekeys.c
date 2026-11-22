/*******************************************************************************

Control-plus and control-minus make what is shown larger and smaller

The two events ami_etusize and ami_etdsize say that the user wants the
material on the screen bigger or smaller, the way control-+ and control--
work in a browser. The window is not touched -- that is the user's to set
-- so this program keeps its window exactly where it is and moves the
point size a point at a time, laying the text out again at whatever size
it has reached.

There are two of each key on a PC keyboard: the "=+" and "-_" of the main
pad, and the "+" and "-" of the numeric pad. All four are taken, and so is
control-= unshifted, since that is the one most people actually press.

Build as a graphical program:

    gcc $(CFLAGS) tests/source/sizekeys.c $(GLIBS) -o bin/sizekeys

Press control-+ and control--, and the sample text changes size while the
window stays put. Escape twice, or the close box, ends it.

*******************************************************************************/

#include <stdio.h>

#include <localdefs.h>
#include <graphics.h>

#define STARTPT 12.0 /* where the point size starts */
#define MINPT   4.0  /* and how far it goes each way */
#define MAXPT   72.0

static void layout(float pt)

{

    ami_long y;
    ami_long i;

    ami_setpoints(stdout, pt);
    fprintf(stdout, "\f");
    y = ami_chrsizy(stdout);
    ami_cursorg(stdout, 10, 10);
    fprintf(stdout, "Point size %.0f -- control-+ and control-- change it",
            pt);
    for (i = 1; i <= 6; i++) {

        ami_cursorg(stdout, 10, 10+(i+1)*y);
        fprintf(stdout, "The material is laid out again at the new size, and "
                        "the window is not touched.");

    }

}

int main(void)

{

    ami_evtrec er;
    float      pt = STARTPT;

    ami_title(stdout, "Control-plus and control-minus");
    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    ami_font(stdout, AMI_FONT_TERM);
    layout(pt);
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etusize && pt < MAXPT) layout(++pt);
        else if (er.etype == ami_etdsize && pt > MINPT) layout(--pt);
        else if (er.etype == ami_etredraw || er.etype == ami_etresize)
            layout(pt);

    } while (er.etype != ami_etterm && er.etype != ami_etcan);

    return (0);

}
