/*******************************************************************************
*                                                                              *
*                       TERMINAL SCREEN CAPTURE MODULE                         *
*                                                                              *
* Linux version, for the terminal model programs. The screen of a terminal    *
* program belongs to the terminal emulator, not to the program, and what the  *
* program knows of it is characters. So the capture is characters: at each    *
* capture the terminal is asked to print its screen, and the pages the        *
* terminal prints, one per capture and each ended by a form feed, are the     *
* listing the test is judged on. A pixel of difference between two terminals, *
* two fonts or two scales does not reach it.                                   *
*                                                                              *
* The request is the ANSI media copy sequence, CSI 0 i, print screen, which   *
* xterm answers by writing the text of the screen to its printer command.     *
* The first capture also sets DECPFF, CSI ? 18 h, so that the terminal        *
* follows every page with a form feed. The harness starts the xterm with the  *
* printer command appending to the listing file:                               *
*                                                                              *
*   xterm -xrm '*printerCommand: cat >> tests/results/terminal_test.lst'       *
*         -xrm '*printAttributes: 0' -xrm '*printerAutoClose: true'           *
*                                                                              *
* printAttributes 0 keeps the pages to plain text; printerAutoClose closes    *
* the printer after each page, so each page is a process of its own that      *
* exits when it has appended its page. A terminal that is not xterm ignores   *
* both sequences and prints nothing.                                           *
*                                                                              *
* The capture is off until screen_capture_name() names the listing: an        *
* interactive run in an xterm whose printer is the default, lpr, must not     *
* send its screens to a printer. The listing is the terminal's to write, at   *
* the command the harness gave it; what this module does with the name is     *
* watch the file. xterm does not wait for the printer process, so a page      *
* asked for while the last is still being appended can land ahead of it; the  *
* capture waits, after each request, until the page's form feed is in the     *
* file, and only then returns to the program. One page is ever in flight, the *
* pages arrive in order, and when the program ends every page is there.       *
*                                                                              *
* The sequences are written beneath the terminal module's write override.     *
* That module places every byte written to standard output into its screen    *
* buffer, and a control sequence sent through it would land there as text; a  *
* direct system call reaches the terminal alone. Standard output is flushed   *
* first, and the terminal module writes its own sequences as it goes, so the  *
* request follows everything the program has drawn.                           *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define OUTFIL  1      /* handle to standard output */
#define PAGEWAIT 10000 /* the most a page is waited for, in milliseconds */

static int  cap_enabled = 0;   /* a listing was named: the capture is on */
static int  cap_started = 0;   /* the form feed mode has been set */
static char cap_name[1024];    /* the listing the pages land in */

/* write to the terminal beneath the write override */
static void rawout(const char* s, size_t n)

{

    ssize_t rc;

    while (n) {

        rc = syscall(SYS_write, OUTFIL, s, n);
        if (rc <= 0) return; /* the terminal is gone: nothing to capture to */
        s += rc;
        n -= rc;

    }

}

/* the pages in the listing so far: the form feeds that end them. The
   file is read with the system calls directly, beneath the stdio override,
   which serves the terminal. */
static long pages(void)

{

    char    buf[4096];
    int     fd;
    ssize_t n;
    long    cnt = 0;

    fd = syscall(SYS_open, cap_name, O_RDONLY, 0);
    if (fd < 0) return (0); /* not there yet: no pages */
    while ((n = syscall(SYS_read, fd, buf, sizeof(buf))) > 0) {

        ssize_t i;

        for (i = 0; i < n; i++) if (buf[i] == '\f') cnt++;

    }
    syscall(SYS_close, fd);

    return (cnt);

}

/* name the listing: the pages land where the harness pointed the terminal's
   printer, and the name turns the capture on */
void screen_capture_name(const char* fn)

{

    strncpy(cap_name, fn, sizeof(cap_name)-1);
    cap_name[sizeof(cap_name)-1] = 0;
    cap_enabled = 1;

}

/* capture the screen: ask the terminal to print it, and wait for the page */
void screen_capture(void)

{

    long            before; /* pages before the request */
    int             ms;     /* milliseconds waited */
    struct timespec ts = { 0, 1000000 }; /* a millisecond */

    if (!cap_enabled) return;
    fflush(stdout);
    before = pages();
    if (!cap_started) {

        rawout("\033[?18h", 6); /* DECPFF: a form feed ends each page */
        cap_started = 1;

    }
    rawout("\033[0i", 4); /* MC: print screen */
    /* the page is in the file when its form feed is; a terminal that
       prints nothing is given up on after a while, and the run goes on */
    for (ms = 0; ms < PAGEWAIT && pages() == before; ms++)
        nanosleep(&ts, NULL);

}
