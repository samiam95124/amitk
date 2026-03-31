/*******************************************************************************

MINE

Mine is the classic game where a field of hidden mines is presented, and the
user tries to find the mines based on mine counts in adjacent squares.

*******************************************************************************/

#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include <localdefs.h>
#include <terminal.h>

#define MAXXS   16 /*8*/  /* size of grid */
#define MAXYS   16 /*8*/
#define MAXMINE 40 /*10*/ /* number of mines to place */

typedef char*   string;  /* general string type */
typedef struct { /* individual square */

      int mine; /* mine exists at square */
      int vis;  /* square is uncovered */
      int flag; /* square is flagged */

} square;
typedef struct { int x; int y; } point;

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

square    board[MAXXS][MAXYS]; /* playing board */
int       rndseq;              /* randomizer */
int       x, y;                /* user move coordinates */
int       done;                /* game over */
int       centerx;             /* center of screen position x */
int       centery;             /* center of screen position y */
int       cursorx;             /* cursor location x */
int       cursory;             /* cursor location y */
ami_evtrec er;                  /* event record */
int       badguess;            /* bad guess display flag */
int       mousex;              /* mouse position x */
int       mousey;              /* mouse position y */

/*******************************************************************************

Find random number

Finds a random number between the given top and 1.

*******************************************************************************/

int rand(int top)

{

#define A 16807
#define M 2147483647

    int gamma;

    gamma = A*(rndseq%(M/A))-(M%A)*(rndseq/(M/A));
    if (gamma > 0) rndseq = gamma; else rndseq = gamma+M;
    return rndseq/(M/top)+1;

}

/*******************************************************************************

Find adjacent mines

Finds the number of mines adjacent to a given square.

*******************************************************************************/

int adjacent(int x, int y)

{

    int mines;  /* number of mines */
    int xn, yn; /* neighbor coordinates */
    int i;      /* index for move array */

    mines = 0; /* clear mine count */
    for (i = 0; i < 8; i++) { /* process points of the compass */

        xn = x+offset[i].x; /* find neighbor locations */
        yn = y+offset[i].y;
        if (xn >= 0 && xn < MAXXS && yn >= 0 && yn < MAXYS)
            /* valid location */
            if (board[xn][yn].mine) mines++; /* count mines */

    }

    return mines; /* return the number of mines */

}

/*******************************************************************************

Set adjacent squares visable

Sets all of the valid adjacent squares visable. If any of those squares are
not adjacent to a mine, then the neighbors of that square are set visable, etc.
(recursively).
This is done to "rip" grids of obviously empty neighbors off the board.

*******************************************************************************/

void visadj(int x, int y)

{

    int xn, yn; /* neighbor coordinates */
    int i;      /* index for move array */

    for (i = 0; i < 8; i++) { /* process points of the compass */

        xn = x+offset[i].x; /* find neighbor locations */
        yn = y+offset[i].y;
        if (xn >= 0 && xn < MAXXS && yn >= 0 && yn < MAXYS)
            if (!board[xn][yn].vis) { /* not already visable */

            /* valid location */
            board[xn][yn].vis = TRUE; /* set visable */
            if (adjacent(xn, yn) == 0) visadj(xn, yn); /* perform recursively */

        }

    }

}

/*******************************************************************************

Display board

Displays the playing board.

*******************************************************************************/

void display(void)

{

    int x;
    int y;
    int cnt; /* count of adjacent mines */

    /* scan screen */
    ami_bcolor(stdout, ami_yellow); /* set background color */
    for (y = 0; y < MAXYS; y++)
      for (x = 0; x < MAXXS; x++) {

        ami_cursor(stdout, centerx+x, centery+y); /* set start of next line */
        if (board[x][y].vis) {

            if (board[x][y].mine) putchar('*'); else {

                cnt = adjacent(x, y); /* find adjacent mine count */
                if (cnt == 0) putchar('.'); /* no adjacent */
                else putchar(cnt+'0'); /* place the number */

            }

        } else if (board[x][y].flag) { /* display flagged location */

            if (badguess) putchar('X'); else putchar('M');

        } else putchar('=');

    }
    putchar('\n');

}

/*******************************************************************************

Initalize board

Clears all board squares to no mines, invisible and not flagged.
Then, the specified number of mines are layed on the board at random.

*******************************************************************************/

void clear(void)

{

    int x;
    int y;
    int n;

    for (x = 0; x < MAXXS; x++) /* clear minefield */
        for (y = 0; y < MAXYS; y++) {

        board[x][y].mine = FALSE; /* set no mine */
        board[x][y].vis = FALSE; /* set not visible */
        board[x][y].flag = FALSE; /* set not flagged */

    }
    for (n = 1; n <= MAXMINE; n++) { /* place mines */

        do {

            x = rand(MAXXS-1);
            y = rand(MAXYS-1);

        } while (board[x][y].mine); /* no mine exists at square */
        board[x][y].mine = TRUE; /* place mine */

    }

}

/*******************************************************************************

Clear line

Clears the specified line to spaces in the specified color.

*******************************************************************************/

void clrlin(int      y,   /* line to clear */
            ami_color clr) /* color to clear to */

{

    int i;

    ami_cursor(stdout, 1, y); /* position to specified line */
    ami_bcolor(stdout, clr); /* set color */
    for (i = 1; i <= ami_maxx(stdout); i++) putchar(' '); /* clear line */

}

/*******************************************************************************

Print centered string

Prints the given string centered on the given line.

*******************************************************************************/

void prtmid(int    y, /* line to print on */
            string s) /* string to print */

{

    ami_cursor(stdout, ami_maxx(stdout)/2-strlen(s)/2, y); /* position to start */
    puts(s); /* output the string */

}

/*******************************************************************************

Draw character box

Draws a box of the given color and character to the location.
The colors are not saved or restored.

*******************************************************************************/

void tbox(int sx, int sy, /* coordinates of box upper left */
         int ex, int ey, /* coordinates of box lower left */
         char c,         /* character to draw */
         ami_color  bclr, /* background color of character */
         ami_color  fclr) /* foreground color of character */

{

    int x, y; /* coordinates */

    ami_bcolor(stdout, bclr);
    ami_fcolor(stdout, fclr);
    ami_cursor(stdout, sx, sy); /* position at box top left */
    for (x = sx; x <= ex; x++) putchar(c); /* draw box top */
    ami_cursor(stdout, sx, ey); /* position at box lower left */
    for (x = sx; x <= ex; x++) putchar(c); /* draw box bottom */
    for (y = sy+1; y <= ey-1; y++) { /* draw box left side */

        ami_cursor(stdout, sx, y); /* place cursor */
        putchar(c); /* place character */

    }
    for (y = sy+1; y <= ey-1; y++) { /* draw box left side */

        ami_cursor(stderr, ex, y); /* place cursor */
        putchar(c); /* place character */

    }

}

/*******************************************************************************

Check replay

Asks the user if a replay is desired, then either cancels the game, or
sets up a new game as requested.

*******************************************************************************/

void replay(void)

{

    /* ask user for replay */
    ami_bcolor(stdout, ami_cyan);
    prtmid(ami_maxy(stdout), "PLAY AGAIN (Y/N) ?");
    do { /* wait for response */

        /* wait till a character is pressed */
        do { ami_event(stdout, &er); }
        while (er.etype != ami_etchar && er.etype != ami_etterm);
        if (er.etype == ami_etterm) /* force a quit */
            { er.etype = ami_etchar; er.echar = 'n'; }

    } while (er.echar != 'y' && er.echar != 'Y' &&
             er.echar != 'n' && er.echar != 'N');
    if (er.echar == 'n' || er.echar == 'N') done = TRUE; /* set game over */
    else {

        /* clear old messages */
        clrlin(ami_maxy(stdout)-2, ami_cyan);
        clrlin(ami_maxy(stdout), ami_cyan);
        /* start new game */
        clear(); /* set up board */
        cursorx = centerx; /* set inital cursor position */
        cursory = centery;
        badguess = FALSE; /* set bad guesses invisible */

    }

}

/*******************************************************************************

Process square "hit"

Processes a "hit" on a square, which means revealing that square, and possibly
triggering a mine.

*******************************************************************************/

void hit(int x, int y)

{

    int xi, yi; /* indexes for board */
    int viscnt; /* visable squares count */

    board[x][y].vis = TRUE; /* set that location visable */
    if (board[x][y].mine) { /* mine found */

        /* make all mines visable, and bad guesses too. */
        for (yi = 0; yi < MAXYS; yi++)
            for (xi = 0; xi < MAXXS; xi++)
                if (board[xi][yi].mine) board[xi][yi].vis = TRUE;
        badguess = TRUE; /* set bad guesses visable */
        display(); /* redisplay board */
        /* announce that to the player */
        ami_bcolor(stdout, ami_red);
        prtmid(ami_maxy(stdout)-2, "*** YOU HIT A MINE ! ***");
        replay(); /* process replay */

    } else { /* valid hit */

        if (adjacent(x, y) == 0) /* no adjacent mines */
            visadj(x, y); /* clean up adjacent spaces */
        /* now, the player may have won. we find this out by counting all of the
          visable squares, and seeing if the number of squares left is equal
          to the number of mines */
        viscnt = 0;
        for (yi = 0; yi < MAXYS; yi++)
            for (xi = 0; xi < MAXXS; xi++)
                if (board[xi][yi].vis) viscnt++; /* count visible */
        if (MAXXS*MAXYS-viscnt == MAXMINE) { /* player wins */

            display(); /* redisplay board */
            /* announce that to the player */
            ami_bcolor(stdout, ami_red);
            prtmid(ami_maxy(stdout)-2, "*** YOU WIN ! ***");
            replay(); /* process replay */

        }

    }
    display(); /* redisplay board */

}

/*******************************************************************************

Main process

*******************************************************************************/

int main(void)

{

    ami_select(stdout, 2, 2); /* switch screens */
    ami_auto(stdout, FALSE); /* automatic terminal off */
    ami_bcolor(stdout, ami_cyan); /* color the background */
    putchar('\f'); /* clear to that */
    ami_bcolor(stdout, ami_magenta);
    prtmid(1, "******* Mine game 1.0 ********"); /* output title */
    /* find center board position */
    centerx = ami_maxx(stdout)/2-MAXXS/2;
    centery = ami_maxy(stdout)/2-MAXYS/2;
    /* draw a border around that */
    tbox(centerx-1, centery-1, centerx+MAXXS, centery+MAXYS, ' ', ami_blue,
         ami_black);
    ami_bcolor(stdout, ami_white); /* restore the background */
    rndseq = 1;
    clear(); /* set up board */
    display(); /* display board */
    done = FALSE; /* set game in progress */
    cursorx = centerx; /* set initial cursor position */
    cursory = centery;
    badguess = FALSE; /* set bad guesses invisible */
    do { /* enter user moves */

        ami_cursor(stdout, cursorx, cursory); /* place cursor */
        x = cursorx-centerx; /* set location on board */
        y = cursory-centery;
        ami_event(stdin, &er); /* get the next event */
        switch (er.etype) { /* event */

            case ami_ettab: /* process flag */
                /* reverse flagging on location */
                board[x][y].flag = !board[x][y].flag;
                display(); /* redisplay board */
                break;
            case ami_etenter: hit(x, y); break; /* process hit */
            /* move up */
            case ami_etup: if (cursory > centery) cursory--; break;
            /* move left */
            case ami_etleft: if (cursorx > centerx) cursorx--; break;
            /* move down */
            case ami_etdown: if (cursory < centery+MAXYS-1) cursory++;
                break;
            /* move right */
            case ami_etright: if (cursorx < centerx+MAXXS-1) cursorx++;
                break;
            case ami_etmoumov: /* mouse movement */
                mousex = er.moupx; /* set new mouse position */
                mousey = er.moupy;
                break;
            case ami_etmouba: /* mouse button 1, hit */
                if (mousex >= centerx && mousex <= centerx+MAXXS-1 &&
                    mousey >= centery && mousey <= centery+MAXYS-1) {

                /* mouse postion inside valid square */
                cursorx = mousex; /* set current position to that */
                cursory = mousey;
                x = cursorx-centerx; /* set location on board */
                y = cursory-centery;
                if (er.amoubn == 1) hit(x, y); /* process hit */
                else if (er.amoubn == 2) { /* process flag */

                    /* reverse flagging on location */
                    board[x][y].flag = !board[x][y].flag;
                    display(); /* redisplay board */

                }
                break;

            }
            default: ;

        }

    } while (!done && er.etype != ami_etterm); /* game complete */
    ami_auto(stdout, TRUE); /* automatic terminal off */
    ami_select(stdout, 1, 1); /* restore screen */

    return (0); /* exit no error */

}
