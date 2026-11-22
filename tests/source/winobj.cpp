/*******************************************************************************

The window and widget objects, demonstrated

This is the flaw demonstration turned right side up. Two windows, each
with a button, and the buttons may even share an id: every event is
routed by the window id in its record to the window object that holds
that window, so each override below fires for its own window's events
and no other. The question the old graph object could not answer --
"which window?" -- is answered by being asked: the object called is
the window meant.

Three tiers are shown:

    1. A widget object's virtual: the second window's button is a
       subclassed button whose pressed() fires directly.

    2. A window object's virtual: the first window's button is made
       with the plain button class and left unhandled by it, so it
       arrives at the window's evbutton(id).

    3. The procedural loop: events neither claims -- the terminate
       event here -- fall through the chain and arrive at event() in
       main, as ever.

Run under a window system; the driver script clicks both buttons and
the etterm path ends it. Every line printed says which tier caught
what, and the two ids printed may be equal: the windows tell them
apart, not the ids.

Build:

    make winobj

*******************************************************************************/

extern "C" {

#include <stdio.h>

}

#include <graphics.hpp>

using namespace graphics;

static long presses; /* both handlers count here; the loop watches */

/* the first window: a plain button on it, answered by the window */
class first: public window {

    public:

    button b;

    first(void): window(), b(*this, 20, 20, 380, 280, "window 1 button", 77)

    {

        title((char*)"window 1");
        setsizg(400, 300);
        /* a slow tick, so the loop below can see how far things are:
           timer events route like any other, and unhandled ones fall
           through to it */
        timer(1, 5000, 1);

    }

    long evbutton(long id)

    {

        fprintf(stderr, "window 1's evbutton(id=%lld): my own button, no "
                        "other\n", AMI_LONG_CAST(id));
        presses++;

        return (1);

    }

};

/* the second window's button: a subclassed button that answers itself */
class mybutton: public button {

    public:

    mybutton(window& wo, long x1, long y1, long x2, long y2, const char* s,
             long id): button(wo, x1, y1, x2, y2, s, id) { }

    long pressed(void)

    {

        fprintf(stderr, "window 2's button.pressed(): the widget itself "
                        "answered\n");
        presses++;

        return (1);

    }

};

class second: public window {

    public:

    mybutton b;

    second(window* parent): window(parent),
        b(*this, 20, 20, 380, 280, "window 2 button", 77)

    {

        title((char*)"window 2");
        setsizg(400, 300);
        setposg(500, 100);

    }

};

int main(void)

{

    evtrec er;

    setvbuf(stderr, NULL, _IONBF, 0); /* say it when it happens */
    autohold(0); /* end means end */

    first  w1;           /* the main window, attached */
    second w2(NULL);     /* an independent top level */

    fprintf(stderr, "both buttons carry id 77: the windows tell them "
                    "apart.\n");
    fprintf(stderr, "click each button once; q ends it...\n");
    do {

        /* Tier three: only what the objects left unhandled arrives
           here. The button clicks never do -- the objects consumed
           them -- so what ends this loop is the terminate. */
        event(&er);
        if (er.etype == etbutton)
            fprintf(stderr, "loop: an unclaimed button event arrived "
                            "(winid=%lld)\n", AMI_LONG_CAST(er.winid));

    } while (er.etype != etterm &&
             !(er.etype == etchar && er.echar == 'q') &&
             !(er.etype == ettim && presses >= 2));

    fprintf(stderr, "done: each tier caught its own.\n");

    return (0);

}
