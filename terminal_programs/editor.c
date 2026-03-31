/*******************************************************************************
*                                                                              *
*                              TEXT EDITTOR                                    *
*                                                                              *
*                                VS. 0.1                                       *
*                                                                              *
*                           COPYRIGHT (C) 1996                                 *
*                                                                              *
*                              S. A. MOORE                                     *
*                                                                              *
* Implements a basic screen based editor with Petit-ami. The standard controls *
* are implemented, plus the following function keys:                           *
*                                                                              *
*    F1 - Search                                                               *
*    F2 - Search again                                                         *
*    F3 - Replace                                                              *
*    F4 - Replace again                                                        *
*    F5 - Record macro start/stop                                              *
*    F6 - Playback macro                                                       *
*    F7 - Save file                                                            *
*                                                                              *
********************************************************************************/

#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#include <localdefs.h>
#include <terminal.h>

#define MAXLIN 250 /* maximum entered line, must be greater than maxx */
#define MAXFIL 40  /* maximum length of filename */

typedef char*                string;  /* general string type */
typedef int   lininx;                 /* index for line buffer */
typedef char  linbuf[MAXLIN];         /* line buffer */
typedef struct line* linptr;          /* pointer to line entry */
/* The lines in the edit buffer are stored as a double linked list of
   dynamically allocated strings */
typedef struct line {                 /* line store entry */

    linptr next; /* next line in store */
    linptr last; /* last line in store */
    string str;  /* string data */

} line;
typedef int filinx; /* index for filename */
typedef char filbuf[MAXFIL]; /* filename */
typedef struct crdrec* crdptr; /* pointer to coordinate store */
typedef struct crdrec { /* cursor coordinate save */

    crdptr next; /* next entry */
    int    x, y; /* cursor coordinates */

} crdrec;

linbuf    inpbuf;        /* input line buffer */
int       buflin;        /* current line in buffer flag */
linptr    linstr;        /* edit lines storage */
linptr    paglin;        /* top of page line */
int       lincnt;        /* number of lines in buffer */
int       chrcnt;        /* number of characters in buffer */
int       linpos;        /* current line */
int       poschr;        /* current character on line */
filbuf    curfil;        /* current file to edit */
ami_evtrec er;            /* next event record */
crdptr    curstk;        /* cursor coordinate stack */
int       mpx;           /* mouse coordinates x */
int       mpy;           /* mouse coordinates y */
int   insertc;       /* insert/overwrite toggle */
linbuf    cmdlin;        /* command line */
lininx    cmdptr;        /* command line pointer */
jmp_buf   inputloop_buf; /* buffer for return to input loop */
int   redraws;       /* in error display */

void errormsg(string s);

/*******************************************************************************

Find space padded strng length

Finds the true length of a string, without right hand padding.

*******************************************************************************/

int len(string s)

{

    int i;

    i = strlen(s)-1;
    while (i > 0 && s[i] == ' ') i--;
    if (s[i] != ' ') i++;

    return i;

}

/*******************************************************************************

Push cursor coordinates

Saves the current cursor coordinates on the cursor coordinate stack.

*******************************************************************************/

void pshcur(void)

{

    crdptr p; /* coordinate entry pointer */

    p = malloc(sizeof(crdrec)); /* get a new stack entry */
    if (!p) errormsg("*** Out of memory");
    p->next = curstk; /* push onto stack */
    curstk = p;
    p->x = ami_curx(stdout); /* place save coordinates */
    p->y = ami_cury(stdout);

}

/*******************************************************************************

Pop cursor coordinates

Restores the current cursor coordinates from the cursor coordinate stack.

*******************************************************************************/

void popcur(void)

{

    crdptr p; /* coordinate entry pointer */

    if (curstk != NULL) { /* cursor stack is not empty */

        ami_cursor(stdout, curstk->x, curstk->y); /* restore old cursor position */
        p = curstk; /* remove from stack */
        curstk = curstk->next;
        free(p); /* release entry */

    }

}

/*******************************************************************************

Update status line

Draws the status line at screen bottom. The status line contains the name of
the current file, the line position, the character position, and the
insert/overwrite status.

*******************************************************************************/

void status(void)

{

    ami_curvis(stdout, FALSE); /* turn off cursor */
    pshcur(); /* save cursor position */
    ami_bcolor(stdout, ami_cyan); /* a nice (light) blue, if you please */
    ami_cursor(stdout, 1, ami_maxy(stdout)); /* position to end line on screen */
    printf("File: %-*s Line: %6d Char: %3d", MAXFIL, curfil, linpos, poschr);
    if (insertc) printf(" Ins"); else printf(" Ovr"); /* write insert status */
    while (ami_curx(stdout) < ami_maxx(stdout))
        putchar(' '); /* blank out the rest */
    putchar(' ');
    ami_bcolor(stdout, ami_white); /* back to white */
    popcur(); /* restore cursor position */
    ami_curvis(stdout, TRUE); /* turn on cursor */
    redraws = FALSE; /* set no redraw */

}

/*******************************************************************************

Update line position

Redraws just the line position in the status line.

*******************************************************************************/

void statusl(void)

{

    if (redraws) status(); /* redraw complete */
    else {

        ami_curvis(stdout, FALSE); /* turn off cursor */
        pshcur(); /* save cursor position */
        ami_bcolor(stdout, ami_cyan); /* a nice (light) blue, if you please */
        ami_cursor(stdout, 54, ami_maxy(stdout)); /* go to line position field */
        printf("%6d", linpos); /* update cursor position */
        ami_bcolor(stdout, ami_white); /* reset color */
        popcur(); /* restore cursor position */
        ami_curvis(stdout, TRUE); /* turn on cursor */

    }

}

/*******************************************************************************

Update character position

Redraws just the character position in the status line.

*******************************************************************************/

void statusc(void)

{

    if (redraws) status(); /* redraw complete */
    else {

        ami_curvis(stdout, FALSE); /* turn off cursor */
        pshcur(); /* save cursor position */
        ami_bcolor(stdout, ami_cyan); /* a nice (light) blue, if you please */
        ami_cursor(stdout, 67, ami_maxy(stdout)); /* go to character position field */
        printf("%3d", poschr); /* update cursor position */
        ami_bcolor(stdout, ami_white); /* reset color */
        popcur(); /* restore cursor position */
        ami_curvis(stdout, TRUE); /* turn on cursor */

    }

}

/*******************************************************************************

Update insert status

Redraws just the insert status in the status line.

*******************************************************************************/

void statusi(void)

{

    if (redraws) status(); /* redraw complete */
    else {

        ami_curvis(stdout, FALSE); /* turn off cursor */
        pshcur(); /* save cursor position */
        ami_bcolor(stdout, ami_cyan); /* a nice (light) blue, if you please */
        ami_cursor(stdout, 71, ami_maxy(stdout)); /* go to character position field */
        if (insertc) printf("Ins"); else printf("Ovr"); /* write insert status */
        ami_bcolor(stdout, ami_white); /* reset color */
        popcur(); /* restore cursor position */
        ami_curvis(stdout, TRUE); /* turn on cursor */

    }

}

/*******************************************************************************

Place information line on screen

Places the information line on screen. The specified string is placed on screen
at the status line position (bottom of screen), in the alert colors.
This will be overwritten by the next status change.

*******************************************************************************/

void info(string s)

{

    ami_curvis(stdout, FALSE); /* turn off cursor */
    pshcur(); /* save cursor position */
    ami_bcolor(stdout, ami_yellow); /* place alert color */
    ami_cursor(stdout, 1, ami_maxy(stdout)); /* position to end line on screen */
    if (*s) printf("%s", s); /* output string */
    while (ami_curx(stdout) <= ami_maxx(stdout))
        putchar(' '); /* blank out the rest */
    ami_bcolor(stdout, ami_white); /* back to white */
    popcur(); /* restore cursor position */
    ami_curvis(stdout, TRUE); /* turn on cursor */
    redraws = TRUE; /* set to redraw */

}

/*******************************************************************************

Process error

Places an information line in the status area, and aborts to input mode.

*******************************************************************************/

void errormsg(string s)

{

    info(s); /* place error message */
    longjmp(inputloop_buf, 1); /* back to input */

}

/*******************************************************************************

Place line at buffer end

Places the given string at the end of the current editor buffer as a new line
entry.

*******************************************************************************/

void plclin(string s)

{

    linptr lp; /* pointer to line entry */
    lininx i;  /* index for line */

    lp = malloc(sizeof(line)); /* get a new line entry */
    lp->str = malloc(strlen(s)+1); /* get space for string */
    strcpy(lp->str, s); /* copy string into place */
    /* insert after line indexed as current */
    if (!linstr) { /* this is the first line */

        lp->next = lp; /* self link the entry */
        lp->last = lp;
        linstr = lp; /* and place root */
        paglin = linstr; /* place the page pin */

    } else { /* store not empty */

        lp->next = linstr; /* link to next */
        lp->last = linstr->last; /* link to last */
        lp->next->last = lp; /* link next to this */
        lp->last->next = lp; /* link last to this */

    }
    lincnt++; /* count lines in buffer */

}

/*******************************************************************************

Write line to display

Outputs the given line, truncated to the screen width. The line is checked for
control characters, and if found, these are replaced by "\".

*******************************************************************************/

void wrtlin(int    y, /* position to place string */
            string s) /* string to place */

{

    int i; /* string index */

    ami_cursor(stdout, 1, y); /* position to start of line */
    for (i = 0; i < ami_maxx(stdout); i++) { /* write characters */

        if (i+1 > strlen(s)) putchar(' '); /* pad end with blanks */
        else if (s[i] >= ' ') putchar(s[i]); /* output as is */
        else { /* is a control character */

            ami_fcolor(stdout, ami_red); /* place in red */
            ami_bcolor(stdout, ami_yellow);
            putchar(s[i]+'@'); /* output as control sequence */
            ami_fcolor(stdout, ami_black); /* back to normal */
            ami_bcolor(stdout, ami_white);

        }

    }

}

/*******************************************************************************

Update entire screen display

Repaints the entire screen, including body text and status line. Takes a flag
to indicate if we will clear the screen or not. Screen geometry changes don't
need a clear, it just flashes the screen.

*******************************************************************************/

void update(int clr)

{

    linptr lp; /* pointer to line entry */
    int lc;    /* line counter */
    int y;     /* y position holder */

    ami_curvis(stdout, FALSE); /* turn off cursor */
    if (clr) putchar('\f'); /* clear screen and home cursor */
    lp = paglin; /* index top of page line */
    lc = ami_maxy(stdout)-1; /* set number of lines to output */
    y = 1; /* set 1st line */
    if (lp) do { /* write lines */

        wrtlin(y, lp->str); /* output line */
        lp = lp->next; /* next line */
        y = y+1;
        lc = lc-1; /* count available lines on screen */

    /* until we wrap around, or screen full */
    } while (lp != linstr && lc != 0);
    ami_curvis(stdout, TRUE); /* turn on cursor */
    status(); /* replace status line */
    ami_home(stdout); /* place cursor at home */

}

/*******************************************************************************

Read text line

Reads a line from the given text file into a string buffer. Returns EOF if end
of file is encountered and there were no characters available.

This routine should check for overflow.

*******************************************************************************/

int getlin(FILE*  f, /* file to read */
           string s, /* string to read to */
           int l)   /* string length */

{

    int i; /* index for string */
    char c;

    for (i = 0; i < l; i++) s[i] = ' '; /* clear destination line */
    i = 0; /* set 1st character position */
    c = fgetc(f);
    while (c != '\n' && c != EOF) { /* read line characters */

        /* should check for line overflow */
        s[i] = c; /* get the next character */
        i = i+1;
        c = fgetc(f);

    }
    s[i] = 0; /* place end of string */

    return (c == EOF && !i); /* return EOF status */

}

/*******************************************************************************

Find current buffer line

Finds the current line in the buffer based on screen position, and returns
a line pointer to that entry.

*******************************************************************************/

linptr fndcur(void)

{

    linptr lp; /* pointer to line */
    int    lc; /* line count */

    lp = paglin; /* index page pin */
    lc = ami_cury(stdout); /* get current line position */
    while (lp && lc != 1) { /* walk down */

        lp = lp->next; /* next line */
        lc = lc-1; /* count */
        /* if we wrapped around to the starting line, that is the end */
        if (lp == linstr) lp = NULL;

    }

    return lp; /* return result */

}

/*******************************************************************************

Pull current line to buffer

The current line is "pulled" to the input buffer. In order to keep from
generating a lot of fractional lines, we keep the current line in a fixed
length buffer during edit on that line. Pulling a line is done before any
within-line edit void is done.

*******************************************************************************/

void getbuf(void)

{

    linptr lp; /* pointer to current line */
    int    i;  /* index for line */
    int    l;  /* length of line */

    if (!buflin) { /* line not in buffer */

        for (i = 0; i < MAXLIN; i++) inpbuf[i] = ' '; /* clear input buffer */
        lp = fndcur(); /* find current line */
        if (lp) {

            /* copy without terminating 0 */
            l = strlen(lp->str);
            for (i = 0; i < l; i++) inpbuf[i] = lp->str[i];

        }
        buflin = TRUE; /* set line in buffer */

    }

}

/*******************************************************************************

Put buffer to current line

If the current line is held in the input buffer, we put it back to the current
line position. This is done by disposing of the contents of the old string,
and allocating and filling a new string.
It is possible for the current line to be null, which means that the buffer
is in the "virtual" space below the bottom of the file. In this case, we must
allocate a series of blank lines until we reach the current line position.
Since any command that moves off the current line will run into problems with
the fiction that having the current line cached in the buffer causes, this
routine should be called before any such movement or operation.

*******************************************************************************/

void putbuf(void)

{

    linptr lp; /* pointer to current line */
    int i;     /* index for line */
    int lc;    /* line counter */
    int l;     /* length of buffered line */

    if (buflin) { /* the line is in the buffer */

        lp = fndcur(); /* find the current line */
        if (!lp) { /* beyond end, create lines */

            /* find number of new lines needed */
            lp = paglin; /* index page pin */
            lc = ami_cury(stdout); /* get current line position */
            while (lp) { /* walk down */

                lp = lp->next; /* next line */
                lc--; /* count */
                /* if we wrapped around to the starting line, that is the end */
                if (lp == linstr) lp = NULL;

            }
            /* place blank lines to fill */
            while (lc > 0) { plclin(""); lc--; }
            lp = fndcur(); /* now find that */

        }
        /* ok, there is a dirty (but workable) trick here. notice that if we have
           created blank lines below the buffer, we will be disposing of that
           newly created blank line. this does not waste storage, however, because
           zero length allocations don't actually exist. Note that this works
           regardless of standard C interpretation.  */
        free(lp->str); /* remove old line */
        l = len(inpbuf); /* find length of buffered line */
        lp->str = malloc(l+1); /* create a new string */
        strncpy(lp->str, inpbuf, l); /* copy to string */
        inpbuf[l] = 0; /* terminate string */
        buflin = FALSE; /* set line not in buffer */

    }

}

/*******************************************************************************

Read file into buffer

The current buffer is cleared, and the given file is read in as a new buffer
contents.

*******************************************************************************/

void readfile(string fn) /* file to read */

{

    FILE*   f;  /* text file */
    linbuf  ln; /* input line buffer */
    int ef; /* end of file indication */

    putbuf(); /* decache any buffer */
    info("Reading file");
    /* we should dispose of existing lines before this operation */
    linstr = NULL; /* clear lines buffer */
    lincnt = 0; /* clear total lines */
    chrcnt = 0; /* clear total characters */
    linpos = 1; /* set 1st line */
    poschr = 1; /* set 1st character */
    f = fopen(fn, "r"); /* open the input file */
    if (!f) info("*** Cannot open file ***");
    ef = FALSE;
    while (!ef) { /* read lines */

        ef = getlin(f, ln, MAXLIN); /* get the next line */
        if (!ef) plclin(ln); /* place in edit buffer */

    }
    fclose(f); /* close input file */
    paglin = linstr; /* index top of buffer */
    update(TRUE); /* display that */

}

/*******************************************************************************

Write file from buffer

The current buffer is written to the given file.

*******************************************************************************/

void writefile(string fn) /* file to read */

{

    FILE*  f;  /* text file */
    linptr lp; /* line pointer */

    putbuf(); /* decache any buffer */
    info("Writing file");
    f = fopen(fn, "w"); /* open the output file */
    if (!f) info("*** Cannot open file ***");
    lp = linstr; /* index top of buffer */
    while (lp) { /* write lines */

        fprintf(f, "%s\n", lp->str); /* output line to file */
        lp = lp->next; /* go next line */
        if (lp == linstr) lp = NULL; /* line wrapped to top, signal end */

    }
    fclose(f); /* close input file */
    status(); /* refresh status line */

}

/*******************************************************************************

Move up one line

Moves the cursor position up one line. If the cursor is already at the top
of screen, then the screen is scrolled up to the next line (if it exists).

*******************************************************************************/

void movup(void)

{

    putbuf(); /* decache any buffer */
    if (linstr) { /* buffer not empty */

        if (paglin != linstr || ami_cury(stdout) > 1) {

            /* not at top of buffer, or not at top of displayed page */
            linpos--; /* adjust line count */
            /* if we aren't already at the top of screen, we can just move up */
            if (ami_cury(stdout) > 1) {

                ami_up(stdout); /* move cursor up */
                statusl(); /* update just line position field */

            } else { /* gotta scroll */

                ami_curvis(stdout, FALSE); /* turn off cursor */
                ami_scroll(stdout, 0, -1); /* scroll the screen down */
                paglin = paglin->last; /* move page pin up */
                pshcur(); /* save cursor position */
                ami_home(stdout); /* go to top line */
                wrtlin(1, paglin->str); /* output that line */
                popcur(); /* restore cursor position */
                ami_curvis(stdout, TRUE); /* turn on cursor */
                status(); /* update status line */

            }

        }

    }

}

/*******************************************************************************

Move down one line

Moves the cursor position down one line. If the cursor is already at the bottom
of screen, then the screen is scrolled down to the next line (if it exists).
Note that we allow positioning past the end of the buffer by one screen minus
one lines worth of text, which would leave the last line at the top.

*******************************************************************************/

void movdwn(void)

{

    int    lc; /* line counter */
    linptr lp; /* line pointer */

    putbuf(); /* decache any buffer */
    if (linstr) { /* buffer not empty */

        if (ami_cury(stdout) < ami_maxy(stdout)-1 ||
            paglin->next != linstr) { /* not at last line */

            /* Not last line on screen, or more lines left in buffer. We are a
               "virtual space" editor, so we fake lines below the buffer end
               as being real */
            linpos++; /* adjust line count */
            /* if we aren't already at the bottom of screen, we can just move
               down */
            if (ami_cury(stdout) < ami_maxy(stdout)-1) {

                ami_down(stdout); /* move cursor down */
                statusl(); /* update just line position field */

            } else { /* gotta scroll */

                /* clear last line */
                ami_curvis(stdout, FALSE); /* turn off cursor */
                pshcur(); /* save current position */
                ami_cursor(stdout, 1, ami_maxy(stdout));
                while (ami_curx(stdout) < ami_maxx(stdout)) putchar(' ');
                putchar(' ');
                popcur(); /* restore cursor position */
                ami_scroll(stdout, 0, +1); /* scroll the screen up */
                paglin = paglin->next; /* move page pin down */
                /* see if a line exists to fill the new slot */
                lc = 1; /* set 1st line */
                lp = paglin;
                /* while not end of buffer, and on valid screen portion */
                while (lp != linstr && lc < ami_maxy(stdout)-1) {

                    lp = lp->next; /* index next line */
                    lc++; /* count */

                }
                if (lp != linstr && lc < ami_maxy(stdout)) {

                    /* new line exists */
                    pshcur(); /* save cursor position */
                    wrtlin(ami_maxy(stdout)-1, lp->str); /* output that line */
                    popcur(); /* restore cursor position */

                }
                ami_curvis(stdout, TRUE); /* turn on cursor */
                status(); /* repaint status line */

            }

        }

    }

}

/*******************************************************************************

Move left one character

If we are not already at the extreme left, moves the cursor one character to
the left.

*******************************************************************************/

void movlft(void)

{

    if (ami_curx(stdout) > 1) { /* not at extreme left */

        ami_left(stdout); /* move cursor left */
        poschr--; /* track character position */
        statusc(); /* update just character position field */

    }

}

/*******************************************************************************

Move right one character

If we are not already at the extreme right, moves the cursor one character to
the right.

*******************************************************************************/

void movrgt(void)

{

    if (ami_curx(stdout) < ami_maxx(stdout)) { /* not at extreme right */

        ami_right(stdout); /* move cursor right */
        poschr++; /* track character position */
        statusc(); /* update just character position field */

    }

}

/*******************************************************************************

Go to top of document

Moves the cursor to the top of the document.

*******************************************************************************/

void movhom(void)

{

    putbuf(); /* decache any buffer */
    if (linstr) { /* buffer not empty */

        linpos = 1; /* set 1st line */
        poschr = 1; /* set 1st character */
        if (paglin == linstr) {

            /* we are at top, just move the cursor there */
            ami_home(stdout); /* move home */
            status(); /* update status */

        } else { /* not at top, go there */

            paglin = linstr; /* set page to home */
            update(TRUE); /* redraw */

        }

    }

}

/*******************************************************************************

Go to bottom of document

Moves the cursor to the bottom of the document.

*******************************************************************************/

void movend(void)

{

    linptr lp; /* line pointer */
    int    lc; /* line count */
    int    oc; /* offset count */

    putbuf(); /* decache any buffer */
    if (linstr) { /* buffer not empty */

        lc = lincnt; /* set last line */
        lp = linstr->last;
        /* The "offset count" is the number of lines to back off from the true
           end of the file. This is choosen to be 1/2 screenfull */
        oc = (ami_maxy(stdout)-1)/2;
        /* now back up to the offset point, or the beginning of file */
        while (lp != linstr && oc) { /* back up */

            lp = lp->last; /* index last line */
            oc--; /* count */
            lc--;

        }
        linpos = lincnt; /* set new line position */
        poschr = strlen(linstr->last->str)+1; /* set new character position */
        if (lp != paglin) { /* we are not already there */

            paglin = lp; /* set new position */
            update(TRUE); /* redraw */

        }
        ami_cursor(stdout, poschr, (ami_maxy(stdout)-1)/2+1);

    }

}

/*******************************************************************************

Go to start of line

Moves the cursor to the start of the current line..

*******************************************************************************/

void movhoml(void)

{

    poschr = 1; /* update position */
    ami_cursor(stdout, 1, ami_cury(stdout)); /* move cursor */
    statusc(); /* update status */

}

/*******************************************************************************

Go to end of line

Moves the cursor to the end of the current line..

*******************************************************************************/

void movendl(void)

{

    linptr lp; /* pointer to line */

    if (buflin) /* line is in buffer */
        poschr = len(inpbuf)+1; /* set new position */
    else { /* line is in file */

        lp = fndcur(); /* find current line */
        if (lp) poschr = strlen(lp->str)+1; /* set new position */
        else poschr = 1; /* no line, position to start for empty line */

    }
    /* if the line was full, we cannot position past it */
    if (poschr > ami_maxx(stdout)) poschr = ami_maxx(stdout);
    ami_cursor(stdout, poschr, ami_cury(stdout)); /* move cursor */
    statusc(); /* update status */

}

/*******************************************************************************

Go to top of screen

Moves the cursor to the top of the current screen.

*******************************************************************************/

void movhoms(void)

{

    putbuf(); /* decache any buffer */
    linpos = linpos-ami_cury(stdout)+1; /* set new position */
    poschr = 1;
    ami_home(stdout); /* position cursor */
    status(); /* update status line */

}

/*******************************************************************************

Go to bottom of screen

Moves the cursor to the bottom of the current screen.

*******************************************************************************/

void movends(void)

{

    linptr lp; /* pointer to line */

    putbuf(); /* decache any buffer */
    linpos = linpos+ami_maxy(stdout)-ami_cury(stdout); /* set new position */
    lp = fndcur(); /* find current line */
    if (lp) poschr = strlen(lp->str)+1; /* set new position */
    else poschr = 1; /* no line, position to start for empty line */
    ami_cursor(stdout, poschr, ami_maxy(stdout)-1); /* move cursor */
    statusc(); /* update status */

}

/*******************************************************************************

Page up

Moves the position up by one screen minus one lines worth of text. One line
of overlap is allowed to give the user some context.
If there is not that much text above, we just position to the top of document.

*******************************************************************************/

void pagup(void)

{

    int cnt;

    putbuf();; /* decache any buffer */
    if (paglin) { /* buffer not empty */

        if (paglin == linstr) { /* already at top, just home cursor */

            linpos = 1; /* set new position */
            ami_cursor(stdout, poschr, 1); /* set to top of screen */
            status(); /* update status line */

        } else {

            /* find number of lines on a page, minus status and slop line */
            cnt = ami_maxy(stdout)-2;
            /* move up to appropriate line */
            while (cnt > 0 && paglin->last && paglin != linstr) {

                paglin = paglin->last; /* move up one line */
                cnt--; /* count lines */
                linpos--;

            }
            pshcur(); /* push cursor coordinates */
            update(TRUE); /* redraw */
            popcur(); /* restore cursor coordinates */

        }

    }

}

/*******************************************************************************

Page down

Moves the position down by one screen minus one lines worth of text. One line
of overlap is allowed to give the user some context.
We allow positioning beyond the end of document by one screen minus one line
of text. If there is not that many lines to the "virtual end point", we just
position to the virtual end point.

*******************************************************************************/

void pagdwn(void)

{

    int cnt;

    putbuf(); /* decache any buffer */
    if (paglin) { /* buffer not empty */

        if (paglin->next != linstr) { /* not at end of buffer */

            /* find number of lines on a page, minus status and slop line */
            cnt = ami_maxy(stdout)-2;
            /* move down to appropriate line */
            while (cnt > 0 && paglin->next && paglin->next != linstr) {

                paglin = paglin->next; /* move down one line */
                cnt--; /* count lines */
                linpos++;

            }
            pshcur(); /* push cursor coordinates */
            update(TRUE); /* redraw */
            popcur(); /* restore cursor coordinates */

        }

    }

}

/*******************************************************************************

Scroll up one line

The screen is scrolled up by one line, revealing at new line at bottom.

*******************************************************************************/

void scrup(void)

{

    putbuf(); /* decache any buffer */
    if (paglin != linstr) { /* not empty and not at buffer top */

        /* not at top of buffer, or not at top of displayed page */
        linpos--; /* adjust line count */
        ami_curvis(stdout, FALSE); /* turn off cursor */
        ami_scroll(stdout, 0, -1); /* scroll the screen down */
        paglin = paglin->last; /* move page pin up */
        pshcur(); /* save cursor position */
        ami_home(stdout); /* go to top line */
        if (strlen(paglin->str))
            printf("%s", paglin->str); /* write revealed line over blanks */
        popcur(); /* restore cursor position */
        ami_curvis(stdout, TRUE); /* turn on cursor */
        status(); /* update status line */

    }

}

/*******************************************************************************

Scroll down one line

The screen is scrolled down by one line, revealing at new line at top.

*******************************************************************************/

void scrdwn(void)

{

    int    lc; /* line counter */
    linptr lp; /* line pointer */

    putbuf(); /* decache any buffer */
    if (linstr) { /* buffer not empty */

        if (paglin->next != linstr) { /* not at last line */

            linpos++; /* adjust line count */
            /* clear last line */
            ami_curvis(stdout, FALSE); /* turn off cursor */
            pshcur(); /* save current position */
            ami_cursor(stdout, 1, ami_maxy(stdout));
            while (ami_curx(stdout) < ami_maxx(stdout)) putchar(' ');
            putchar(' ');
            popcur(); /* restore cursor position */
            ami_scroll(stdout, 0, +1); /* scroll the screen up */
            paglin = paglin->next; /* move page pin down */
            /* see if a line exists to fill the new slot */
            lc = 1; /* set 1st line */
            lp = paglin;
            /* while not end of buffer, and on valid screen portion */
            while (lp != linstr && lc < ami_maxy(stdout)-1) {

                lp = lp->next; /* index next line */
                lc++; /* count */

            }
            if (lp != linstr && lc < ami_maxy(stdout)) {

                /* new line exists */
                pshcur(); /* save cursor position */
                ami_cursor(stdout, 1, ami_maxy(stdout)-1); /* go to last line */
                if (strlen(lp->str))
                    printf("%s", lp->str); /* output that line */
                popcur(); /* restore cursor position */

            }
            ami_curvis(stdout, TRUE); /* turn on cursor */
            status(); /* repaint status line */

        }

    }

}

/*******************************************************************************

Track mouse movements

Updates the mouse location when it moves.

*******************************************************************************/

void moumov(void)

{

   mpx = er.moupx; /* save current mouse coordinates */
   mpy = er.moupy;

}

/*******************************************************************************

Handle mouse button assert

Performs the action for a mouse button assert. If the mouse position points
to the valid screen area (in the text pane and not the status line), then we
change the cursor location to equal that.

*******************************************************************************/

void mouass(void)

{

    if (mpy < ami_maxy(stdout)) {

        /* not on status line */
        linpos = linpos+(mpy-ami_cury(stdout)); /* set new position */
        poschr = mpx;
        ami_cursor(stdout, mpx, mpy); /* place cursor at new position */
        status(); /* update status line */

    }

}

/*******************************************************************************

Toggle insert mode

Changes the insert/overwrite status to the opposite mode. Updates the display.

*******************************************************************************/

void togins(void)

{

    insertc = !insertc; /* insert toggle */
    statusi(); /* update display */

}

/*******************************************************************************

Enter character

Enters a single character to the current edit position. First, the line is
"pulled" to a buffer. Then, the character is inserted at the current character
position, and the line and status redrawn.

*******************************************************************************/

void entchr(char c)

{

    int i; /* index for line */
    int y; /* cursor y save */
    int l; /* length of current line */

    if (insertc) { /* process using insert mode */

        getbuf(); /* pull line to buffer */
        l = len(inpbuf); /* find current length of line */
        if (l < ami_maxx(stdout)) { /* we have room to place */

            /* move up buffer to make room */
            for (i = l; i >= poschr; i--) inpbuf[i] = inpbuf[i-1];
            inpbuf[poschr-1] = c; /* place character */
            y = ami_cury(stdout); /* save location y */
            ami_curvis(stdout, FALSE); /* turn off cursor */
            l = len(inpbuf); /* find new length of line */
            for (i = poschr-1; i < l; i++)
                putchar(inpbuf[i]); /* output the line */
            if (poschr < ami_maxx(stdout))
                poschr++; /* advance character position */
            ami_cursor(stdout, poschr, y); /* restore cursor to new position */
            ami_curvis(stdout, TRUE); /* turn one cursor */
            statusc(); /* update character position field */

        }

    } else /* process using overwrite mode */
        if (poschr <= ami_maxx(stdout)) { /* we have room to place */

        getbuf(); /* pull line to buffer */
        y = ami_cury(stdout); /* save location y */
        inpbuf[poschr] = c; /* place character */
        putchar(c); /* place character on screen */
        if (poschr < ami_maxx(stdout)) /* not at extreme right */
            poschr++; /* advance character position */
        ami_cursor(stdout, poschr, y); /* restore cursor to new position */
        statusc(); /* update character position field */

    }

}

/*******************************************************************************

Delete back

The character to the left of the cursor is removed, and all the characters
to the right are moved left one character.

*******************************************************************************/

void delbwd(void)

{

    int i; /* index for line */
    int l; /* length of line */
    int y; /* y position save */

    if (poschr > 1) { /* not already at extreme left */

        getbuf(); /* pull line to buffer */
        y = ami_cury(stdout); /* save location y */
        /* gap character */
        for (i = poschr; i <= MAXLIN; i++) inpbuf[i-1] = inpbuf[i];
        inpbuf[MAXLIN-1] = ' '; /* fill last position */
        poschr--; /* set new character position */
        ami_left(stdout); /* move cursor left */
        l = strlen(inpbuf); /* find length of input buffer */
        ami_curvis(stdout, FALSE); /* turn off cursor */
        for (i = poschr; i <= l; i++) putchar(inpbuf[i]); /* replace line */
        if (l < ami_maxx(stdout)) putchar(' '); /* blank out last position */
        ami_cursor(stdout, poschr, y); /* restore position */
        ami_curvis(stdout, TRUE); /* turn on cursor */
        statusc(); /* update character position field */

   }

}

/*******************************************************************************

Delete forward

The character at the cursor is removed,and all the characters to the right of
the cursor are moved left one character.

*******************************************************************************/

void delfwd(void)

{

    int i; /* index for line */
    int l; /* length of line */
    int y; /* y position save */

    if (poschr < ami_maxx(stdout)) { /* not already at extreme right */

        getbuf(); /* pull line to buffer */
        y = ami_cury(stdout); /* save location y */
        /* gap character */
        for (i = poschr; i <= MAXLIN-1; i++) inpbuf[i] = inpbuf[i+1];
        inpbuf[MAXLIN-1] = ' '; /* fill last position */
        l = strlen(inpbuf); /* find length of input buffer */
        ami_curvis(stdout, FALSE); /* turn off cursor */
        for (i = poschr; i <= l; i++) putchar(inpbuf[i]); /* replace line */
        if (l < ami_maxx(stdout)) putchar(' '); /* blank out last position */
        ami_cursor(stdout, poschr, y); /* restore position */
        ami_curvis(stdout, TRUE); /* turn on cursor */
        statusc(); /* update character position field */

   }

}

/*******************************************************************************

Line enter

Moves to beginning of the the next line. Enter does not really do anything
special in edit, its just the combination of two motions.

*******************************************************************************/

void enter(void)

{

    movdwn(); /* move down a line */
    ami_cursor(stdout, 1, ami_cury(stdout)); /* move to extreme left */
    poschr = 1;

}

/*******************************************************************************

Tab

In overwrite mode, we simply position to the next tab. In insert mode, we
insert enough spaces to reach the next tab.

*******************************************************************************/

void tab(void)

{

    if (poschr < ami_maxx(stdout)) /* not at extreme right */
        do { /* output spaces */

            entchr(' '); /* place a single space */

    } while (poschr < ami_maxx(stdout) && !((poschr-1)%8==0));

}

/*******************************************************************************

Execute function key by number

Accepts a function key number, 1-n, and executes the associated function.

*******************************************************************************/

void func(int fn)

{

    switch (fn) {

        case 7:

            if (!*curfil) errormsg("*** No filename set ***");
            writefile(curfil); /* write output file */
            break;

    }

}

/*******************************************************************************

Main procedure

Initializes the program, loads the target source file, and enters the event
loop. Note during init we select screen 2, which on most PA implementations
causes the original screen to be saved, so that we can restore it again when
the program exits.

All of the command keys in the editor appear in the event loop. We leave it
mostly up to PA to assign which keys do what in the editor, the exception being
the function keys.

*******************************************************************************/

int main(int argc, char *argv[])

{

    if (setjmp(inputloop_buf)) goto inputloop;

    linstr = NULL; /* clear lines buffer */
    paglin = NULL; /* clear top of page line */
    curstk = NULL; /* clear coordinate stack */
    lincnt = 0; /* clear total lines */
    chrcnt = 0; /* clear total characters */
    linpos = 1; /* set 1st line */
    poschr = 1; /* set 1st character */
    mpx = 0; /* set mouse is nowhere */
    mpy = 0;
    buflin = FALSE; /* set no line in buffer */
    insertc = TRUE; /* set insert mode on */
    redraws = FALSE; /* set no redraw required */
    /* check screen size is less than our minimum */
    if (ami_maxx(stdout) < 70 || ami_maxy(stdout) < 2) {

        /* we take a special short exit because the display is not workable.
           This only works for in-line display, separate windows just exit
           because it happens too fast */
        printf("*** Window too small\n");
        goto stopprog;

    }
    ami_select(stdout, 2, 2); /* flip to private screen */
    ami_auto(stdout, FALSE); /* turn off scrolling/wrapping */
    update(TRUE); /* present blank screen */
    strcpy(curfil, ""); /* clear current filename */
    if (argc == 2) { /* input file exists */

        strncpy(curfil, argv[1], MAXFIL);
        readfile(curfil); /* read the file in */

    }
    inputloop: /* return to input level */

    /* The screen is initalized with the specified file. Now we enter the event
       loop */
    do { /* event loop */

        ami_event(stdin, &er); /* get the next event */
        switch (er.etype) { /* event */

            case ami_etchar:    entchr(er.echar); /* ASCII character returned */
                               break;
            case ami_etup:      movup(); break; /* cursor up one line */
            case ami_etdown:    movdwn(); break; /* down one line */
            case ami_etleft:    movlft(); break; /* left one character */
            case ami_etright:   movrgt(); break; /* right one character */
            case ami_etleftw:   break; /* left one word */
            case ami_etrightw:  break; /* right one word */
            case ami_ethome:    movhom(); break; /* home of document */
            case ami_ethomes:   movhoms(); break; /* home of screen */
            case ami_ethomel:   movhoml(); break; /* home of line */
            case ami_etend:     movend(); break; /* end of document */
            case ami_etends:    movends(); break; /* end of screen */
            case ami_etendl:    movendl(); break; /* end of line */
            case ami_etscrl:    break; /* scroll left one character */
            case ami_etscrr:    break; /* scroll right one character */
            case ami_etscru:    scrup(); break; /* scroll up one line */
            case ami_etscrd:    scrdwn(); break; /* scroll down one line */
            case ami_etpagu:    pagup(); break; /* page up */
            case ami_etpagd:    pagdwn(); break; /* page down */
            case ami_ettab:     tab(); break; /* tab */
            case ami_etenter:   enter(); break; /* enter line */
            case ami_etinsert:  break; /* insert block */
            case ami_etinsertl: break; /* insert line */
            case ami_etinsertt: togins(); break; /* insert toggle */
            case ami_etdel:     break; /* delete block */
            case ami_etdell:    break; /* delete line */
            case ami_etdelcf:   delfwd(); break; /* delete character forward */
            case ami_etdelcb:   delbwd(); break; /* delete character backward */
            case ami_etcopy:    break; /* copy block */
            case ami_etcopyl:   break; /* copy line */
            case ami_etcan:     break; /* cancel current operation */
            case ami_etstop:    break; /* stop current operation */
            case ami_etcont:    break; /* continue current operation */
            case ami_etprint:   break; /* print document */
            case ami_etprintb:  break; /* print block */
            case ami_etprints:  break; /* print screen */
            case ami_etfun:     func(er.fkey); break; /* functions */
            case ami_etmouba:   mouass(); break; /* mouse button 1 assertion */
            case ami_etmoumov:  moumov(); break; /* mouse move */
            case ami_etresize:  
                ami_sizbuf(stdout, er.rszx, er.rszy); /* resize buffer to fit screen */
                update(FALSE); /* update window */
                break;
            case ami_etterm:    break; /* terminate program */
            default:;

        }

   } while (er.etype != ami_etterm); /* until terminal event */
   ami_auto(stdout, TRUE); /* turn on scrolling */
   ami_select(stdout, 1, 1); /* return to normal screen */

   stopprog: ; /* exit program */

   return (0); /* exit no error */

}
