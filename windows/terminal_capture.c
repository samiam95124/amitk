/*******************************************************************************
*                                                                              *
*                       TERMINAL SCREEN CAPTURE MODULE                         *
*                                                                              *
* Windows version, for the terminal model programs. As on Linux, the capture  *
* is characters: the screen of a terminal program belongs to the console, and *
* what the program knows of it is characters, so the listing the test is      *
* judged on is the text of the screen, a page per capture and each ended by a *
* form feed. A pixel of difference between two consoles, two fonts or two     *
* scales does not reach it.                                                    *
*                                                                              *
* Where the Linux module asks the xterm to print its screen, the console has  *
* no printer, and its screen buffer is readable: the capture opens CONOUT$,   *
* which is whatever screen buffer is active, and reads the rows the window    *
* shows, ReadConsoleOutputCharacter giving each row's characters without      *
* their attributes. The rows are the window's rows, not the buffer's: the     *
* terminal module sizes its screen to the window, and a buffer taller than    *
* the window holds lines scrolled off the top, which are not on the screen.   *
* Trailing blanks are not on the screen either, and are dropped, as the       *
* harness drops them from the xterm's pages. The characters are written in    *
* UTF-8, which is what the xterm's printer gives.                             *
*                                                                              *
* The listing is appended with the system calls directly, which keeps it      *
* beneath the terminal module's stdio: a page written through stdout would    *
* land on the screen. Standard output is flushed first, so the page follows   *
* everything the program has drawn; the console draws as it is written, and   *
* the page is on the screen when the write returns, so the capture returns    *
* with the page in the file, and there is nothing to wait for.                *
*                                                                              *
* The capture is off until screen_capture_name() names the listing: an        *
* interactive run makes no listing.                                            *
*                                                                              *
*******************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define MAXCOL 1024 /* the widest row read */

static int  cap_enabled = 0;   /* a listing was named: the capture is on */
static char cap_name[1024];    /* the listing the pages land in */

/* append to the listing */
static void append(HANDLE f, const char* s, DWORD n)

{

    DWORD w;

    while (n) {

        if (!WriteFile(f, s, n, &w, NULL) || !w) return;
        s += w;
        n -= w;

    }

}

/* name the listing, which turns the capture on */
void screen_capture_name(const char* fn)

{

    strncpy(cap_name, fn, sizeof(cap_name)-1);
    cap_name[sizeof(cap_name)-1] = 0;
    cap_enabled = 1;

}

/* capture the screen: the rows the console window shows, as a page */
void screen_capture(void)

{

    HANDLE                     con, f;
    CONSOLE_SCREEN_BUFFER_INFO bi;
    WCHAR                      row[MAXCOL];
    char                       utf[MAXCOL*4+1];
    COORD                      at;
    DWORD                      n;
    int                        w, y, len;

    if (!cap_enabled) return;
    fflush(stdout);
    /* the active screen buffer, whichever the terminal module has up */
    con = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE,
                      FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                      0, NULL);
    if (con == INVALID_HANDLE_VALUE) return; /* no console: nothing to capture */
    if (!GetConsoleScreenBufferInfo(con, &bi)) { CloseHandle(con); return; }
    f = CreateFileA(cap_name, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { CloseHandle(con); return; }
    w = bi.srWindow.Right-bi.srWindow.Left+1;
    if (w > MAXCOL) w = MAXCOL;
    for (y = bi.srWindow.Top; y <= bi.srWindow.Bottom; y++) {

        at.X = bi.srWindow.Left;
        at.Y = (SHORT)y;
        n = 0;
        if (!ReadConsoleOutputCharacterW(con, row, w, at, &n)) n = 0;
        while (n && row[n-1] == L' ') n--; /* trailing blanks are not on the screen */
        len = 0;
        if (n) len = WideCharToMultiByte(CP_UTF8, 0, row, n, utf, sizeof(utf)-1,
                                         NULL, NULL);
        if (len < 0) len = 0;
        utf[len++] = '\n';
        append(f, utf, len);

    }
    append(f, "\f", 1); /* the page ends */
    CloseHandle(f);
    CloseHandle(con);

}
