/*******************************************************************************
*                                                                              *
*                       TERMINAL SCREEN CAPTURE MODULE                         *
*                                                                              *
* Mac OS X version, for the terminal model programs. As on Linux, the screen  *
* of a terminal program belongs to the terminal emulator, not to the program, *
* and the capture is characters: the page the emulator shows, one per         *
* capture, each ended by a form feed, appended to the listing the test is     *
* judged on.                                                                   *
*                                                                              *
* The Linux harness runs the test in an xterm and has the xterm print its     *
* screen, through a command on a pipe, at the ANSI media copy request. The   *
* xterm Mac OS X has (XQuartz's) leaves a forked image of itself behind for  *
* every page it prints, and the kernel allows a user few enough processes     *
* that a long test dies part way; and nothing else on the Mac needs X at all. *
* So here the test runs in a tmux session, a terminal emulator with no        *
* display, and the page is pulled rather than pushed: at each capture the     *
* test asks tmux for the contents of its own pane (capture-pane), which the   *
* tmux client prints and exits, reaped here. Nothing is spawned by the        *
* emulator, so nothing accumulates, however many pages a test prints.        *
*                                                                              *
* The page has the format the standards have, one line per row of the pane,  *
* the form feed after the last. tmux leaves no trailing blanks on a line;     *
* the harness strips them from every listing before judging, so the pages    *
* compare the same as xterm's.                                                 *
*                                                                              *
* Calls in this module:                                                        *
*                                                                              *
* void screen_capture_name(const char* fn);                                    *
*                                                                              *
* Names the listing the pages go to. Given by the test from its command line. *
*                                                                              *
* void screen_capture(void);                                                   *
*                                                                              *
* Appends the current screen to the listing. Outside a tmux session there is  *
* no emulator to ask, and the capture is skipped.                              *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static char cap_name[1024]; /* the listing */

void screen_capture_name(const char* fn)
{
    snprintf(cap_name, sizeof(cap_name), "%s", fn);
}

void screen_capture(void)
{
    static unsigned long seq; /* the page's number, for its mark */
    const char* pane;
    char        mark[64];
    char        cmd[2048];
    int         fd;

    if (!cap_name[0]) return;
    pane = getenv("TMUX_PANE");
    if (!pane || !*pane) return; /* not under tmux: no emulator to ask */

    /* Everything the program has written must be on the screen before it is
       read, and the emulator takes it up when it gets to it: a moment after
       here, or longer when the program runs at the far end of an ssh
       connection. So the page is marked in the stream itself, as the pane's
       title, which tmux keeps; it takes up the stream in order, so when it
       shows the mark, all that came before it is on the screen. The page is
       read then, or after five seconds without it, as the screen is. */
    fflush(stdout);
    seq++;
    snprintf(mark, sizeof(mark), "pa-capture-%lu", seq);
    snprintf(cmd, sizeof(cmd), "\033]2;%s\033\\", mark);
    if (write(STDOUT_FILENO, cmd, strlen(cmd)) < 0) { /* the wait runs out */ }

    /* the pane's rows, one line each, appended to the listing; the client
       finds its server through $TMUX, as it is run from inside the session */
    snprintf(cmd, sizeof(cmd),
             "i=0; while [ \"$(tmux display -p -t '%s' '#{pane_title}')\" != '%s' ] && "
             "[ $i -lt 1000 ]; do sleep 0.005; i=$((i+1)); done; "
             "tmux capture-pane -p -t '%s' >> '%s'", pane, mark, pane, cap_name);
    if (system(cmd) != 0) return; /* no page: the test will say so */

    /* the page ends with its form feed, which is what the test waits for */
    fd = open(cap_name, O_WRONLY | O_APPEND);
    if (fd >= 0) {
        if (write(fd, "\f", 1) < 0) { /* nothing to do about it */ }
        close(fd);
    }
}
