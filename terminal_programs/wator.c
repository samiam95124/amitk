/*******************************************************************************

WATOR ECOLOGICAL SIMULATOR

84 S. A. FRANCO

Based on the article in the "computer recreations" column of the December,
1984 Scientific American.

The screen is arranged as a flat projection of a torid. Upon the blank screen
are placed a number of "shark" and "fish" tokens. The tokens are processed,
in each of the time "steps", Such that breeding, attack and consumption, and
death are roughly simulated.

The result is a displayed ecology. See the article for further details.

*******************************************************************************/

#include <stdio.h>    /* standard I/O library */
#include <stdlib.h>   /* misc standard functions */
#include <terminal.h> /* terminal emulation library */

#define NFISH  200  /* initial number of fish */
#define NSHARK 20   /* initial number of shark */
#define FBREED 3    /* time before fish breed */
#define SBREED 10   /* time before shark breed */
#define STARVE 3    /* time before sharks STARVE */
#define MAXDIM 1000 /* maximum size of supported screen dims */

typedef enum {none, fish, shark} object; /* objects on screen */

/* array representing the playing board */
struct {

    object typ;    /* object at point */
    int    age;    /* age of object */
    int    hunger; /* hunger of object */
    int    moved;  /* has been moved */

} world[MAXDIM][MAXDIM];

ami_evtrec er; /* event input record */
int i;
int x;

/*******************************************************************************

Display board

Scans each of the logical objects in the world array and places the actual
display token on the screen. The tokens are:

     Shark - '@'
     Space - ' '
     Fish  - '^'

*******************************************************************************/

void display(void)

{

    int x;
    int y;

    /* scan screen */
    ami_home(stdout);
    for (y = 0; y < ami_maxy(stdout); y++) {

        ami_cursor(stdout, 1, y+1);
        for (x = 0; x < ami_maxx(stdout); x++)
            switch (world[x][y].typ) { /* object */

            case none:  putchar(' '); break;
            case fish:  putchar('^'); break;
            case shark: putchar('@'); break;

        }

    }

}

/*******************************************************************************

Find random number

Find random number between 0 and N.

*******************************************************************************/

int randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

/*******************************************************************************

Initialize board

Initializes the board to "none", then places the required fish && sharks at
random. The ages are set at random with the breeding time for the object as
the top.

*******************************************************************************/

void clear(void)

{

    int x;
    int y;
    int n;

    for (x = 0; x < ami_maxx(stdout); x++)
        for (y = 0; y < ami_maxy(stdout); y++)
            world[x][y].typ = none;
    for (n = 0; n < NFISH; n++) {

        do {

            x = randn(ami_maxx(stdout)-1);
            y = randn(ami_maxy(stdout)-1);

        } while (world[x][y].typ != none);
        world[x][y].typ = fish;
        world[x][y].age = randn(FBREED);
        world[x][y].moved = FALSE;

    }
    for (n = 0; n < NSHARK; n++) {

        do {

            x = randn(ami_maxx(stdout)-1);
            y = randn(ami_maxy(stdout)-1);

        } while (world[x][y].typ != none);
        world[x][y].typ = shark;
        world[x][y].age = randn(SBREED);
        world[x][y].hunger = 0;
        world[x][y].moved = FALSE;

    }

}

/*******************************************************************************

Find adjacent object

Finds a given object adjacent to the given coordinates. If the object is
found, the coordinates are changed to that object, otherwise, the coordinates
are left alone. If more than one of the requested object are adjacent, then
one is picked at random.

Adjacent means wrapped around the board for the edge cases.

*******************************************************************************/

void fndadj(int* x, int* y, object obj)

{

    typedef struct { int x, y; } point;

    point offset[8] = {

        {  0, -1 }, /* up */
        { +1, -1 }, /* upper right */
        { +1,  0 }, /* right */
        { +1, +1 }, /* lower right */
        {  0, +1 }, /* down */
        { -1, +1 }, /* lower left */
        { -1,  0 }, /* left */
        { -1, -1 }  /* upper left */

    };

    int top;
    point found[8];
    int i, s;
    int xn, yn; /* neighbor coordinates */

    top = 0;
    for (i = 0; i < 8; i++) { /* process points of the compass */

        xn = *x+offset[i].x; /* find neighbor locations */
        yn = *y+offset[i].y;
        /* wrap overflows */
        if (xn < 0) xn = ami_maxx(stdout)-1;
        if (xn >= ami_maxx(stdout)) xn = 0;
        if (yn < 0) yn = ami_maxy(stdout)-1;
        if (yn >= ami_maxy(stdout)) yn = 0;
        if (world[xn][yn].typ == obj) {

            found[top].x = xn;
            found[top].y = yn;
            top++;

        }

    }
    if (top) {

      s = randn(top);
      *x = found[s].x;
      *y = found[s].y;

    }

}

/*******************************************************************************

Process fish moves

The board is scanned for fish, and if one is found, then we look for adjacent
empty squares. If one is found, we move the fish there. If the fish is ready
to breed, then a new fish occupy that square, and the old fish remains
unmoved. We keep track of what fish have moved, and leave the moved fish alone.

*******************************************************************************/

void dofish(void)

{

    int x, nx;
    int y, ny;

    for (x = 0; x < ami_maxx(stdout); x++)
        for (y = 0; y < ami_maxy(stdout); y++)
            if (world[x][y].typ == fish && !world[x][y].moved) {

        nx = x;
        ny = y;
        fndadj(&nx, &ny, none);
        if (world[nx][ny].typ == none) {

            world[nx][ny] = world[x][y];
            world[nx][ny].moved = TRUE;
            if (world[x][y].age == FBREED) {

                world[x][y].age = 0;
                world[nx][ny].age = 0;

            } else world[x][y].typ = none;

        }

    }

}

/*******************************************************************************

Process shark moves

The board is scanned for sharks. First, we look for fish adjacent to sharks,
and if one is found, the shark moves there (eats the fish), and has its
hunger reset.

If no fish are adjacent, then we will look for empty spaces, and move the
shark there.

In either case, if the shark is ready to breed, then a new shark occupies the
new square, and the old shark remains.

If a shark's hunger exceeds a limit, then the shark dies.

*******************************************************************************/

void doshark(void)

{

    int x, nx;
    int y, ny;

    for (x = 0; x < ami_maxx(stdout); x++)
        for (y = 0; y < ami_maxy(stdout); y++)
            if (world[x][y].typ == shark && !world[x][y].moved) {

        if (world[x][y].hunger == STARVE) world[x][y].typ = none;
        else {

            nx = x;
            ny = y;
            fndadj(&nx, &ny, fish);
            if (world[nx][ny].typ == fish) {

                world[nx][ny].typ = world[x][y].typ;
                world[nx][ny].age = world[x][y].age;
                world[nx][ny].moved = TRUE;
                if (world[x][y].age == SBREED) {

                    world[x][y].age = 0;
                    world[x][y].hunger = 0;
                    world[nx][ny].age = 0;

                } else world[x][y].typ = none;
                world[nx][ny].hunger = 0;

            } else {

                nx = x;
                ny = y;
                fndadj(&nx, &ny, none);
                if (world[nx][ny].typ == none) {

                    world[nx][ny].typ = world[x][y].typ;
                    world[nx][ny].age = world[x][y].age;
                    world[nx][ny].hunger = world[x][y].hunger;
                    world[nx][ny].moved = TRUE;
                    if (world[x][y].age == SBREED) {

                        world[x][y].age = 0;
                        world[nx][ny].age = 0;

                    } else world[x][y].typ = none;

                }

            }

        }

    }

}

/*******************************************************************************

Process clock tick

Finishes the processing for a single frame of time. The age of all objects
is increased, any moves on them are reset, && if a shark, it's hunger is
increased.

*******************************************************************************/

void clock(void)

{

    int x;
    int y;

    for (x = 0; x < ami_maxx(stdout); x++)
        for (y = 0; y < ami_maxy(stdout); y++)
            if (world[x][y].typ != none) {

       world[x][y].age++;
       world[x][y].moved = FALSE;
       if (world[x][y].typ == shark)
          world[x][y].hunger++;

   }

}

/*******************************************************************************

Main process

The scrolling is turned off, the screen cleared, the random sequencer started.
The board is set up with a random pick of fish && sharks, && the board is
displayed.
We then enter the main loop, where we process fish moves, then shark moves,
redisplay the board && update the clock.

*******************************************************************************/

int main(void)

{

    ami_select(stdout, 2, 2); /* switch screens */
    ami_auto(stdout, FALSE); /* turn off scrolling */
    ami_curvis(stdout, FALSE); /* turn off cursor */
    putchar('\f'); /* clear screen */
    clear();
    display();
    /* set cycle to rate limit at 10 times a second */
    ami_timer(stdout, 1, 1000, TRUE);
    do {

        /* get next interesting event */
        do { ami_event(stdin, &er);
        } while (er.etype != ami_ettim && er.etype != ami_etterm);
        if (er.etype == ami_ettim) { /* timer event, run cycle */

            dofish();
            doshark();
            display();
            clock();

        }

    } while (er.etype != ami_etterm);
    ami_curvis(stdout, TRUE); /* turn cursor back on */
    ami_auto(stdout, TRUE); /* turn on scrolling */
    putchar('\f'); /* clear screen */

    return (0); /* exit no error */

}
