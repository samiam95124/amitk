/*******************************************************************************
*                                                                              *
*                          TEXT PRINT OUTPUT MODULE                            *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                            2026/08/13 S. A. Franco                           *
*                                                                              *
* Takes the call set of terminal.h and composes plain text pages from it, the  *
* terminal mode counterpart of the graphical print module. A print file is     *
* created with:                                                                *
*                                                                              *
*     openprint(&f, "name")                                                    *
*                                                                              *
* The name is either a standard filename, receiving the text, or a printer     *
* device name, recognized by the ':' in the name: "lp0:", "lp1:", etc., the    *
* printer by number as the spooler lists them, or "lp:", the system default    *
* printer. Printer output is composed the same way, then handed to the print   *
* spooler as a single job when the file is closed.                             *
*                                                                              *
* The page model is the one the manual gives for printers (4.25): a one page   *
* buffer written with the normal terminal calls, cursor movements included.    *
* The buffer is the default 80 by 25 characters; sizbuf() resizes it, and the  *
* new size is the print page size. When a form-feed ('\f') is output the page  *
* is complete: it is written out as formatted text, each line as it lays in    *
* the buffer, with positioning carried by left padding with spaces, trailing   *
* spaces and trailing blank lines dropped, and the page closed with a          *
* form-feed. A partial page left at close is ejected automatically. There is   *
* no input side: the event loop and all the input devices give errors, as      *
* does select(), since a printer has one surface and no return path.           *
*                                                                              *
* The colors and the text attributes are accepted and ignored: plain text      *
* carries neither.                                                             *
*                                                                              *
* The module interdicts the terminal vectors and the system write and close,   *
* just as network does, so that only files made by openprint() are affected;   *
* all other files pass through untouched.                                      *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* The document goes to its descriptor byte for byte: Windows would otherwise
   translate the line ends and put the .pdf's byte offsets out. The null
   device, where a printer document parks its descriptor, is spelled
   differently there too. */
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifdef _WIN32
#define NULLDEV "nul"
#else
#define NULLDEV "/dev/null"
#endif

#include <terminal.h>

/* the local library headers do not carry the process pipe calls */
extern FILE* popen(const char* command, const char* type);
extern int pclose(FILE* stream);

#define MAXPFIL 512  /* maximum file descriptors tracked */
#define MAXTAB  100  /* tab stops */
#define MAXPDEV 100  /* printer device name length */

#define DEFW    80   /* default page width */
#define DEFH    25   /* default page height */
#define MAXPAG  1000 /* sanity bound on page dimensions */

/* per print file record */
typedef struct prtrec* prtptr;
typedef struct prtrec {

    int   prtdev;         /* goes to a printer, not a file */
    char  pdev[MAXPDEV];  /* printer name, empty for system default */

    char* grid;           /* the page, w*h characters */
    ami_long  w, h;           /* page dimensions in characters */
    ami_long  curx, cury;     /* cursor, 1 based */
    int   autom;          /* auto wrap/eject mode */
    int   dirty;          /* the page holds output */
    ami_long  tabs[MAXTAB];   /* tab stops, character positions, sorted */
    int   ntabs;          /* number of tab stops */

    char* out;            /* accumulated printer output */
    ami_long  outlen;         /* occupied */
    ami_long  outcap;         /* allocated */

} prtrec;

/* print files table, by file descriptor */
static prtptr prtfil[MAXPFIL];

/* types of system vectors for override calls */
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*pclose_syst)(int);

/* system override calls */
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_close(pclose_syst nfp, pclose_syst* ofp);

/* saved system vectors */
static pwrite_t    dn_write;
static pclose_syst dn_close;

/* saved terminal vectors, one per interdicted call */
static _pa_cursor_t      dn_cursor;      static _pa_maxx_t        dn_maxx;
static _pa_maxy_t        dn_maxy;        static _pa_home_t        dn_home;
static _pa_del_t         dn_del;         static _pa_up_t          dn_up;
static _pa_down_t        dn_down;        static _pa_left_t        dn_left;
static _pa_right_t       dn_right;       static _pa_blink_t       dn_blink;
static _pa_reverse_t     dn_reverse;     static _pa_underline_t   dn_underline;
static _pa_superscript_t dn_superscript; static _pa_subscript_t   dn_subscript;
static _pa_italic_t      dn_italic;      static _pa_bold_t        dn_bold;
static _pa_strikeout_t   dn_strikeout;   static _pa_standout_t    dn_standout;
static _pa_fcolor_t      dn_fcolor;      static _pa_bcolor_t      dn_bcolor;
static _pa_curbnd_t      dn_curbnd;      static _pa_auto_t        dn_auto;
static _pa_curvis_t      dn_curvis;      static _pa_scroll_t      dn_scroll;
static _pa_curx_t        dn_curx;        static _pa_cury_t        dn_cury;
static _pa_select_t      dn_select;      static _pa_event_t       dn_event;
static _pa_timer_t       dn_timer;       static _pa_killtimer_t   dn_killtimer;
static _pa_mouse_t       dn_mouse;       static _pa_mousebutton_t dn_mousebutton;
static _pa_joystick_t    dn_joystick;    static _pa_joybutton_t   dn_joybutton;
static _pa_joyaxis_t     dn_joyaxis;     static _pa_settab_t      dn_settab;
static _pa_restab_t      dn_restab;      static _pa_clrtab_t      dn_clrtab;
static _pa_funkey_t      dn_funkey;      static _pa_frametimer_t  dn_frametimer;
static _pa_wrtstr_t      dn_wrtstr;      static _pa_wrtstrn_t     dn_wrtstrn;
static _pa_sizbuf_t      dn_sizbuf;      static _pa_title_t       dn_title;
static _pa_titlen_t      dn_titlen;      static _pa_fcolorc_t     dn_fcolorc;
static _pa_bcolorc_t     dn_bcolorc;     static _pa_sendevent_t   dn_sendevent;

/*******************************************************************************

Report error and stop

*******************************************************************************/

static void error(const char* es)

{

    /* bypass the file layers: write directly to the error channel */
    write(2, "*** txtterminal: ", 17);
    write(2, es, strlen(es));
    write(2, "\n", 1);
    exit(1);

}

/*******************************************************************************

File and state access

*******************************************************************************/

/* index print file from FILE*, NULL if not a print file */
static prtptr txt2prt(FILE* f)

{

    int fn;

    if (!f) return (NULL);
    fn = fileno(f);
    if (fn < 0 || fn >= MAXPFIL) return (NULL);

    return (prtfil[fn]);

}

/* the page grid, 1 based */
#define G(p, x, y) ((p)->grid[((y)-1)*(p)->w+((x)-1)])

/* clear the page */
static void prtclr(prtptr p)

{

    memset(p->grid, ' ', p->w*p->h);
    p->dirty = 0;

}

static void prthome(prtptr p)

{

    p->curx = 1;
    p->cury = 1;

}

/* accumulate printer output */
static void outcat(prtptr p, const char* d, ami_long n)

{

    char* ns;

    if (p->outlen+n > p->outcap) {

        p->outcap = p->outcap? p->outcap*2: 4096;
        while (p->outlen+n > p->outcap) p->outcap *= 2;
        ns = realloc(p->out, p->outcap);
        if (!ns) error("Out of memory");
        p->out = ns;

    }
    memcpy(p->out+p->outlen, d, n);
    p->outlen += n;

}

/*******************************************************************************

Page control

*******************************************************************************/

/* eject the page: write it out as formatted text. Each line lays as it does
   in the buffer, trailing spaces dropped; trailing blank lines are dropped
   and the page closes with a form-feed. A file receives its pages as they
   complete; a printer document accumulates and goes to the spooler at close
   as one job. */
static void formfeed(int fd, prtptr p)

{

    ami_long    y, x, last;
    ssize_t r;
    ami_long    i;

    /* find the last line holding anything */
    last = 0;
    for (y = 1; y <= p->h; y++)
        for (x = 1; x <= p->w; x++)
            if (G(p, x, y) != ' ') { last = y; break; }
    for (y = 1; y <= last; y++) {

        /* the line, trailing spaces dropped */
        x = p->w;
        while (x >= 1 && G(p, x, y) == ' ') x--;
        outcat(p, &G(p, 1, y), x);
        outcat(p, "\n", 1);

    }
    outcat(p, "\f", 1);
    if (!p->prtdev) {

        /* a file takes the page now */
        i = 0;
        while (i < p->outlen) {

            r = (*dn_write)(fd, p->out+i, p->outlen-i);
            if (r <= 0) error("Cannot write print file");
            i += r;

        }
        p->outlen = 0;

    }
    prtclr(p);
    prthome(p);

}

/* line feed, honoring the page bottom in auto mode */
static void linefeed(int fd, prtptr p)

{

    p->curx = 1;
    p->cury++;
    if (p->autom && p->cury > p->h) formfeed(fd, p);

}

/* place a character at the cursor */
static void plcchr(int fd, prtptr p, char c)

{

    if (p->autom && p->curx > p->w) linefeed(fd, p);
    if (p->curx >= 1 && p->curx <= p->w && p->cury >= 1 && p->cury <= p->h) {

        G(p, p->curx, p->cury) = c;
        p->dirty = 1;

    }
    p->curx++;

}

/*******************************************************************************

Document output

A printer document goes to the spooler as a single job at close; a file has
already received its pages as they ejected.

*******************************************************************************/

static void prtout(prtptr p)

{

    char  cmd[MAXPDEV+32];
    FILE* pf;

    if (!p->prtdev || !p->outlen) return;
    /* to the spooler; -s silences the job id report */
    if (p->pdev[0])
        snprintf(cmd, sizeof(cmd), "lp -s -d '%s' -", p->pdev);
    else snprintf(cmd, sizeof(cmd), "lp -s -");
    pf = popen(cmd, "w");
    if (!pf) error("Cannot start print spooler");
    if (fwrite(p->out, 1, p->outlen, pf) != (size_t)p->outlen)
        error("Cannot spool print job");
    if (pclose(pf)) error("Print spooler failed");

}

/* take down and free a print file entry */
static void prtfree(int fd)

{

    prtptr p = prtfil[fd];

    if (p->dirty) formfeed(fd, p); /* eject a partial page */
    prtout(p);
    free(p->grid);
    if (p->out) free(p->out);
    free(p);
    prtfil[fd] = NULL;

}

/*******************************************************************************

Open print file

Creates a print file. The name is either a filename for the text, or a
printer, recognized by the ':' in the name: "lp0:", "lp1:", ... select the
printer by number as the spooler lists them, and "lp:" is the system default
printer.

*******************************************************************************/

static void fndprinter(int pn, char* dst, int dstl)

{

    FILE* lp;
    char  ln[MAXPDEV];
    int   i = 0;

    dst[0] = 0;
    lp = popen("lpstat -e 2>/dev/null", "r");
    if (!lp) error("Cannot enumerate printers");
    while (fgets(ln, sizeof(ln), lp)) {

        ln[strcspn(ln, "\n")] = 0;
        if (i == pn && ln[0]) { strncpy(dst, ln, dstl-1); dst[dstl-1] = 0; }
        i++;

    }
    pclose(lp);
    if (!dst[0]) error("No such printer");
    if (strchr(dst, '\'')) error("Unusable printer name");

}

void ami_openprint(FILE** f, char* n)

{

    prtptr p;
    int    fd, i;
    char*  cp;
    ami_long   pn;

    p = calloc(1, sizeof(prtrec));
    if (!p) error("Out of memory");
    cp = strchr(n, ':');
    if (cp) { /* a printer device */

        if (n[0] != 'l' || n[1] != 'p' || cp[1])
            error("Invalid printer device name");
        p->prtdev = 1;
        if (cp != n+2) { /* numbered: find it now, to fail early */

            pn = strtol(n+2, &cp, 10);
            if (*cp != ':') error("Invalid printer device name");
            fndprinter(pn, p->pdev, MAXPDEV);

        }
        /* the descriptor only anchors the file id */
        fd = open(NULLDEV, O_WRONLY);

    } else fd = open(n, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0666);
    if (fd < 0) error("Cannot open print file");
    if (fd >= MAXPFIL) error("Invalid file handle");

    /* the default state: the 80 by 25 page, auto on, page top */
    p->w = DEFW;
    p->h = DEFH;
    p->grid = malloc(p->w*p->h);
    if (!p->grid) error("Out of memory");
    prtclr(p);
    prthome(p);
    p->autom = 1;
    /* tabs every 8 characters, as a terminal presents */
    for (i = 1; i*8+1 <= p->w && p->ntabs < MAXTAB; i++)
        p->tabs[p->ntabs++] = i*8+1;
    prtfil[fd] = p;

    *f = fdopen(fd, "w");
    if (!*f) error("Cannot open print file");
    /* unbuffered, so that writes and terminal calls stay in order */
    setvbuf(*f, NULL, _IONBF, 0);

}

/*******************************************************************************

System vector interdiction

Writes to a print file are the character stream: printables place at the
cursor, and the controls carry their printer meanings, with form-feed
completing the page. The close completes the document.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buf, size_t n)

{

    prtptr      p;
    const char* s = buf;
    size_t      i;
    int         ti;

    if (fd < 0 || fd >= MAXPFIL || !prtfil[fd])
        return ((*dn_write)(fd, buf, n));
    p = prtfil[fd];
    for (i = 0; i < n; i++) {

        unsigned char c = s[i];
        if (c >= 32 && c != 127) plcchr(fd, p, c);
        else switch (c) {

            case '\n': linefeed(fd, p); break;
            case '\r': p->curx = 1; break;
            case '\f': formfeed(fd, p); break;
            case '\b': if (p->curx > 1) p->curx--; break;
            case '\t':
                for (ti = 0; ti < p->ntabs; ti++)
                    if (p->tabs[ti] > p->curx)
                        { p->curx = p->tabs[ti]; break; }
                break;
            default: break; /* other controls are dropped */

        }

    }

    return (n);

}

static int iclose(int fd)

{

    if (fd >= 0 && fd < MAXPFIL && prtfil[fd]) prtfree(fd);

    return ((*dn_close)(fd));

}

/*******************************************************************************

Terminal vector interdiction

Each vector checks for a print file and either executes into the page or
passes down the chain. The input side calls give errors on a print file, as
the manual specifies: a printer has no input. The colors and the attributes
are accepted and ignored: plain text carries neither.

*******************************************************************************/

static void noinput(void) { error("No input from a print file"); }

static void cursor_pvf(FILE* f, ami_long x, ami_long y)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_cursor)(f, x, y); return; }
    p->curx = x;
    p->cury = y;
}

static ami_long maxx_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_maxx)(f));
    return (p->w);
}

static ami_long maxy_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_maxy)(f));
    return (p->h);
}

static void home_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_home)(f); return; }
    prthome(p);
}

static void del_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_del)(f); return; }
    if (p->curx > 1) {

        p->curx--;
        if (p->curx <= p->w && p->cury >= 1 && p->cury <= p->h)
            G(p, p->curx, p->cury) = ' ';

    }
}

static void up_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_up)(f); return; }
    p->cury--;
}

static void down_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_down)(f); return; }
    p->cury++;
}

static void left_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_left)(f); return; }
    p->curx--;
}

static void right_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_right)(f); return; }
    p->curx++;
}

/* the attributes and colors do not exist in plain text; accepted, ignored */

static void blink_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_blink)(f, e);
}

static void reverse_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_reverse)(f, e);
}

static void underline_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_underline)(f, e);
}

static void superscript_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_superscript)(f, e);
}

static void subscript_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_subscript)(f, e);
}

static void italic_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_italic)(f, e);
}

static void bold_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_bold)(f, e);
}

static void strikeout_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_strikeout)(f, e);
}

static void standout_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_standout)(f, e);
}

static void fcolor_pvf(FILE* f, ami_color c)
{
    if (!txt2prt(f)) (*dn_fcolor)(f, c);
}

static void bcolor_pvf(FILE* f, ami_color c)
{
    if (!txt2prt(f)) (*dn_bcolor)(f, c);
}

static void fcolorc_pvf(FILE* f, ami_long r, ami_long g, ami_long b)
{
    if (!txt2prt(f)) (*dn_fcolorc)(f, r, g, b);
}

static void bcolorc_pvf(FILE* f, ami_long r, ami_long g, ami_long b)
{
    if (!txt2prt(f)) (*dn_bcolorc)(f, r, g, b);
}

static void curvis_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) (*dn_curvis)(f, e);
    /* there is no cursor on paper */
}

static ami_long curbnd_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_curbnd)(f));
    return (p->curx >= 1 && p->curx <= p->w &&
            p->cury >= 1 && p->cury <= p->h);
}

static void auto_pvf(FILE* f, ami_long e)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_auto)(f, e); return; }
    p->autom = !!e;
}

static void scroll_pvf(FILE* f, ami_long x, ami_long y)
{
    prtptr p = txt2prt(f);
    char*  ng;
    ami_long   dx, dy, sx, sy;
    if (!p) { (*dn_scroll)(f, x, y); return; }
    /* the page is a real buffer: shift the contents, blank fill */
    ng = malloc(p->w*p->h);
    if (!ng) error("Out of memory");
    memset(ng, ' ', p->w*p->h);
    for (dy = 1; dy <= p->h; dy++)
        for (dx = 1; dx <= p->w; dx++) {

            sx = dx+x;
            sy = dy+y;
            if (sx >= 1 && sx <= p->w && sy >= 1 && sy <= p->h)
                ng[(dy-1)*p->w+(dx-1)] = G(p, sx, sy);

        }
    free(p->grid);
    p->grid = ng;

}

static ami_long curx_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_curx)(f));
    return (p->curx);
}

static ami_long cury_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) return ((*dn_cury)(f));
    return (p->cury);
}

/* the input side does not exist on a print file */

static void select_pvf(FILE* f, ami_long u, ami_long d)
{
    if (!txt2prt(f)) { (*dn_select)(f, u, d); return; }
    error("Cannot select screens on a print file");
}

static void event_pvf(FILE* f, ami_evtrec* er)
{
    if (!txt2prt(f)) { (*dn_event)(f, er); return; }
    noinput();
}

static void timer_pvf(FILE* f, ami_long i, ami_long t, ami_long r)
{
    if (!txt2prt(f)) { (*dn_timer)(f, i, t, r); return; }
    noinput();
}

static void killtimer_pvf(FILE* f, ami_long i)
{
    if (!txt2prt(f)) { (*dn_killtimer)(f, i); return; }
    noinput();
}

static ami_long mouse_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_mouse)(f));
    noinput();
    return (0);
}

static ami_long mousebutton_pvf(FILE* f, ami_long m)
{
    if (!txt2prt(f)) return ((*dn_mousebutton)(f, m));
    noinput();
    return (0);
}

static ami_long joystick_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_joystick)(f));
    noinput();
    return (0);
}

static ami_long joybutton_pvf(FILE* f, ami_long j)
{
    if (!txt2prt(f)) return ((*dn_joybutton)(f, j));
    noinput();
    return (0);
}

static ami_long joyaxis_pvf(FILE* f, ami_long j)
{
    if (!txt2prt(f)) return ((*dn_joyaxis)(f, j));
    noinput();
    return (0);
}

static ami_long funkey_pvf(FILE* f)
{
    if (!txt2prt(f)) return ((*dn_funkey)(f));
    noinput();
    return (0);
}

static void frametimer_pvf(FILE* f, ami_long e)
{
    if (!txt2prt(f)) { (*dn_frametimer)(f, e); return; }
    noinput();
}

static void sendevent_pvf(FILE* f, ami_evtrec* er)
{
    if (!txt2prt(f)) { (*dn_sendevent)(f, er); return; }
    noinput();
}

/* tabs */

static void instab(prtptr p, ami_long t)
{
    int i, j;
    for (i = 0; i < p->ntabs; i++) {

        if (p->tabs[i] == t) return;
        if (p->tabs[i] > t) break;

    }
    if (p->ntabs >= MAXTAB) error("Too many tabs");
    for (j = p->ntabs; j > i; j--) p->tabs[j] = p->tabs[j-1];
    p->tabs[i] = t;
    p->ntabs++;
}

static void settab_pvf(FILE* f, ami_long t)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_settab)(f, t); return; }
    instab(p, t);
}

static void restab_pvf(FILE* f, ami_long t)
{
    prtptr p = txt2prt(f);
    int i, j;
    if (!p) { (*dn_restab)(f, t); return; }
    for (i = 0; i < p->ntabs; i++) if (p->tabs[i] == t) {

        for (j = i; j < p->ntabs-1; j++) p->tabs[j] = p->tabs[j+1];
        p->ntabs--;
        return;

    }
}

static void clrtab_pvf(FILE* f)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_clrtab)(f); return; }
    p->ntabs = 0;
}

/* direct strings */

static void wrtstr_pvf(FILE* f, char* s)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_wrtstr)(f, s); return; }
    while (*s) {

        if ((unsigned char)*s >= 32 && (unsigned char)*s != 127)
            plcchr(fileno(f), p, *s);
        s++;

    }
}

static void wrtstrn_pvf(FILE* f, char* s, ami_long n)
{
    prtptr p = txt2prt(f);
    if (!p) { (*dn_wrtstrn)(f, s, n); return; }
    while (n--) {

        if ((unsigned char)*s >= 32 && (unsigned char)*s != 127)
            plcchr(fileno(f), p, *s);
        s++;

    }
}

/* the buffer size is the page size */

static void sizbuf_pvf(FILE* f, ami_long x, ami_long y)
{
    prtptr p = txt2prt(f);
    char*  ng;
    int    i;
    if (!p) { (*dn_sizbuf)(f, x, y); return; }
    if (x < 1 || y < 1 || x > MAXPAG || y > MAXPAG)
        error("Invalid page dimensions");
    ng = malloc(x*y);
    if (!ng) error("Out of memory");
    free(p->grid);
    p->grid = ng;
    p->w = x;
    p->h = y;
    /* as the terminal buffer: the contents are cleared */
    prtclr(p);
    prthome(p);
    /* the default tabs refit the new width */
    p->ntabs = 0;
    for (i = 1; i*8+1 <= p->w && p->ntabs < MAXTAB; i++)
        p->tabs[p->ntabs++] = i*8+1;
}

/* a text page has no title */

static void title_pvf(FILE* f, char* ts)
{
    if (!txt2prt(f)) (*dn_title)(f, ts);
}

static void titlen_pvf(FILE* f, char* ts, ami_long l)
{
    if (!txt2prt(f)) (*dn_titlen)(f, ts, l);
}

/*******************************************************************************

Initialize and deinitialize

The module hooks in above the terminal library and the window manager, so
that a print file is taken before either sees it. Print files still open at
exit are completed, so a program that neglects the close still prints.

*******************************************************************************/

static void ami_init_txtterminal(void) __attribute__((constructor (108)));
static void ami_init_txtterminal(void)

{

    ovr_write(iwrite, &dn_write);
    ovr_close(iclose, &dn_close);

    _pa_cursor_ovr(cursor_pvf, &dn_cursor);
    _pa_maxx_ovr(maxx_pvf, &dn_maxx);
    _pa_maxy_ovr(maxy_pvf, &dn_maxy);
    _pa_home_ovr(home_pvf, &dn_home);
    _pa_del_ovr(del_pvf, &dn_del);
    _pa_up_ovr(up_pvf, &dn_up);
    _pa_down_ovr(down_pvf, &dn_down);
    _pa_left_ovr(left_pvf, &dn_left);
    _pa_right_ovr(right_pvf, &dn_right);
    _pa_blink_ovr(blink_pvf, &dn_blink);
    _pa_reverse_ovr(reverse_pvf, &dn_reverse);
    _pa_underline_ovr(underline_pvf, &dn_underline);
    _pa_superscript_ovr(superscript_pvf, &dn_superscript);
    _pa_subscript_ovr(subscript_pvf, &dn_subscript);
    _pa_italic_ovr(italic_pvf, &dn_italic);
    _pa_bold_ovr(bold_pvf, &dn_bold);
    _pa_strikeout_ovr(strikeout_pvf, &dn_strikeout);
    _pa_standout_ovr(standout_pvf, &dn_standout);
    _pa_fcolor_ovr(fcolor_pvf, &dn_fcolor);
    _pa_bcolor_ovr(bcolor_pvf, &dn_bcolor);
    _pa_curbnd_ovr(curbnd_pvf, &dn_curbnd);
    _pa_auto_ovr(auto_pvf, &dn_auto);
    _pa_curvis_ovr(curvis_pvf, &dn_curvis);
    _pa_scroll_ovr(scroll_pvf, &dn_scroll);
    _pa_curx_ovr(curx_pvf, &dn_curx);
    _pa_cury_ovr(cury_pvf, &dn_cury);
    _pa_select_ovr(select_pvf, &dn_select);
    _pa_event_ovr(event_pvf, &dn_event);
    _pa_timer_ovr(timer_pvf, &dn_timer);
    _pa_killtimer_ovr(killtimer_pvf, &dn_killtimer);
    _pa_mouse_ovr(mouse_pvf, &dn_mouse);
    _pa_mousebutton_ovr(mousebutton_pvf, &dn_mousebutton);
    _pa_joystick_ovr(joystick_pvf, &dn_joystick);
    _pa_joybutton_ovr(joybutton_pvf, &dn_joybutton);
    _pa_joyaxis_ovr(joyaxis_pvf, &dn_joyaxis);
    _pa_settab_ovr(settab_pvf, &dn_settab);
    _pa_restab_ovr(restab_pvf, &dn_restab);
    _pa_clrtab_ovr(clrtab_pvf, &dn_clrtab);
    _pa_funkey_ovr(funkey_pvf, &dn_funkey);
    _pa_frametimer_ovr(frametimer_pvf, &dn_frametimer);
    _pa_wrtstr_ovr(wrtstr_pvf, &dn_wrtstr);
    _pa_wrtstrn_ovr(wrtstrn_pvf, &dn_wrtstrn);
    _pa_sizbuf_ovr(sizbuf_pvf, &dn_sizbuf);
    _pa_title_ovr(title_pvf, &dn_title);
    _pa_titlen_ovr(titlen_pvf, &dn_titlen);
    _pa_fcolorc_ovr(fcolorc_pvf, &dn_fcolorc);
    _pa_bcolorc_ovr(bcolorc_pvf, &dn_bcolorc);
    _pa_sendevent_ovr(sendevent_pvf, &dn_sendevent);

}

static void ami_deinit_txtterminal(void) __attribute__((destructor (108)));
static void ami_deinit_txtterminal(void)

{

    int         fd;
    pwrite_t    cpwrite;
    pclose_syst cpclose;

    /* complete any print file the program left open */
    for (fd = 0; fd < MAXPFIL; fd++)
        if (prtfil[fd]) { prtfree(fd); (*dn_close)(fd); }

    /* pop off the terminal vectors; the modules below deinstall after us
       and check that they come off the top of the chain */
    { _pa_cursor_t t;      _pa_cursor_ovr(dn_cursor, &t); }
    { _pa_maxx_t t;        _pa_maxx_ovr(dn_maxx, &t); }
    { _pa_maxy_t t;        _pa_maxy_ovr(dn_maxy, &t); }
    { _pa_home_t t;        _pa_home_ovr(dn_home, &t); }
    { _pa_del_t t;         _pa_del_ovr(dn_del, &t); }
    { _pa_up_t t;          _pa_up_ovr(dn_up, &t); }
    { _pa_down_t t;        _pa_down_ovr(dn_down, &t); }
    { _pa_left_t t;        _pa_left_ovr(dn_left, &t); }
    { _pa_right_t t;       _pa_right_ovr(dn_right, &t); }
    { _pa_blink_t t;       _pa_blink_ovr(dn_blink, &t); }
    { _pa_reverse_t t;     _pa_reverse_ovr(dn_reverse, &t); }
    { _pa_underline_t t;   _pa_underline_ovr(dn_underline, &t); }
    { _pa_superscript_t t; _pa_superscript_ovr(dn_superscript, &t); }
    { _pa_subscript_t t;   _pa_subscript_ovr(dn_subscript, &t); }
    { _pa_italic_t t;      _pa_italic_ovr(dn_italic, &t); }
    { _pa_bold_t t;        _pa_bold_ovr(dn_bold, &t); }
    { _pa_strikeout_t t;   _pa_strikeout_ovr(dn_strikeout, &t); }
    { _pa_standout_t t;    _pa_standout_ovr(dn_standout, &t); }
    { _pa_fcolor_t t;      _pa_fcolor_ovr(dn_fcolor, &t); }
    { _pa_bcolor_t t;      _pa_bcolor_ovr(dn_bcolor, &t); }
    { _pa_curbnd_t t;      _pa_curbnd_ovr(dn_curbnd, &t); }
    { _pa_auto_t t;        _pa_auto_ovr(dn_auto, &t); }
    { _pa_curvis_t t;      _pa_curvis_ovr(dn_curvis, &t); }
    { _pa_scroll_t t;      _pa_scroll_ovr(dn_scroll, &t); }
    { _pa_curx_t t;        _pa_curx_ovr(dn_curx, &t); }
    { _pa_cury_t t;        _pa_cury_ovr(dn_cury, &t); }
    { _pa_select_t t;      _pa_select_ovr(dn_select, &t); }
    { _pa_event_t t;       _pa_event_ovr(dn_event, &t); }
    { _pa_timer_t t;       _pa_timer_ovr(dn_timer, &t); }
    { _pa_killtimer_t t;   _pa_killtimer_ovr(dn_killtimer, &t); }
    { _pa_mouse_t t;       _pa_mouse_ovr(dn_mouse, &t); }
    { _pa_mousebutton_t t; _pa_mousebutton_ovr(dn_mousebutton, &t); }
    { _pa_joystick_t t;    _pa_joystick_ovr(dn_joystick, &t); }
    { _pa_joybutton_t t;   _pa_joybutton_ovr(dn_joybutton, &t); }
    { _pa_joyaxis_t t;     _pa_joyaxis_ovr(dn_joyaxis, &t); }
    { _pa_settab_t t;      _pa_settab_ovr(dn_settab, &t); }
    { _pa_restab_t t;      _pa_restab_ovr(dn_restab, &t); }
    { _pa_clrtab_t t;      _pa_clrtab_ovr(dn_clrtab, &t); }
    { _pa_funkey_t t;      _pa_funkey_ovr(dn_funkey, &t); }
    { _pa_frametimer_t t;  _pa_frametimer_ovr(dn_frametimer, &t); }
    { _pa_wrtstr_t t;      _pa_wrtstr_ovr(dn_wrtstr, &t); }
    { _pa_wrtstrn_t t;     _pa_wrtstrn_ovr(dn_wrtstrn, &t); }
    { _pa_sizbuf_t t;      _pa_sizbuf_ovr(dn_sizbuf, &t); }
    { _pa_title_t t;       _pa_title_ovr(dn_title, &t); }
    { _pa_titlen_t t;      _pa_titlen_ovr(dn_titlen, &t); }
    { _pa_fcolorc_t t;     _pa_fcolorc_ovr(dn_fcolorc, &t); }
    { _pa_bcolorc_t t;     _pa_bcolorc_ovr(dn_bcolorc, &t); }
    { _pa_sendevent_t t;   _pa_sendevent_ovr(dn_sendevent, &t); }

    /* swap the system vectors back; if we don't come off the top of the
       chain the stacking is corrupt */
    ovr_write(dn_write, &cpwrite);
    ovr_close(dn_close, &cpclose);
    if (cpwrite != iwrite || cpclose != iclose)
        error("System consistency check");

}
