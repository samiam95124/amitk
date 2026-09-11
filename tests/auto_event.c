/*******************************************************************************
*                                                                              *
*                        AUTOMATIC EVENT INPUT MODULE                          *
*                                                                              *
* Feeds a test program events from a text file, so a pattern that is driven   *
* by the user (the mouse, the keys) can be walked by the regression the same   *
* way every time. See doc/auto_events.md.                                      *
*                                                                              *
* The file holds one statement per line, after which '#' starts a comment:     *
*                                                                              *
*   <event> [parameters]   An event, named as in include/graphics.h without    *
*                          the ami_et prefix, its parameters in the order of   *
*                          the event record: "char 'h'", "moumovg 1 400 300",  *
*                          "mouba 1 1", "enter", "button 5". A character is    *
*                          written 'x' or as a number.                         *
*   sync <frame>[.<step>]  Hold the events that follow until the test is on    *
*                          that frame, and that step of it if given.           *
*   window <id>            The window the events that follow carry, 1 to      *
*                          start.                                              *
*   wait <ms>              Let the program run for that many milliseconds,     *
*                          its events its own, then a beat: a step captured    *
*                          of what time did, a progress bar or a clock.        *
*   keyboardoff, mouseoff, joystickoff                                         *
*                          Drop the events of the real keyboard, mouse or      *
*                          joystick, so a device on the desk cannot join a     *
*                          test of its events from the file; keyboardon,       *
*                          mouseon and joystickon let them through again.      *
*   mousenum <n>, joysticknum <n>                                              *
*                          The count of mice or joysticks the test is told of  *
*                          by auto_mouse() and auto_joystick(), which it uses  *
*                          in place of ami_mouse() and ami_joystick(): the     *
*                          file's count when set, the desk's otherwise.        *
*   mousebutton <n>, joybutton <n>, joyaxis <n>                                *
*                          The buttons of every mouse, and the buttons and     *
*                          axes of every joystick, from auto_mousebutton(),    *
*                          auto_joybutton() and auto_joyaxis() the same way.   *
*                                                                              *
* The device settings go at the start of the file, before any sync, so they    *
* hold for the run: the file is read to its first sync on the test's first     *
* ask, whether that is for a device or for an event. A device set aside is     *
* set aside here, in the test: its events are skipped on their way from Ami.   *
*                                                                              *
* On a display with a seat rig (the Wayland layer's PD_INPUT fifo), the mouse  *
* and key events of the file are not handed to the program but put in at the   *
* seat, as if a person made them: a move goes to the point named, in the       *
* client area of the main window, a button press or release to the pointer's  *
* place, and a key to whatever holds the focus. The display routes them the    *
* way it routes a real seat's, so a click at a widget's place presses the       *
* widget, with hover, focus and crossings as for a person, and the program     *
* gets what the widget sends. After each such line the module waits a moment   *
* for what it provokes, handing the program every event that comes, and then   *
* a beat: auto_event_step() says one when the line has run its course, which   *
* is when a test captures a step. Keys are given as keycodes, so a character   *
* is a key of the US layout, letters, digits and the unshifted punctuation.    *
* Where there is no rig (the remote client, the terminal, the framebuffer) the *
* events go to the program directly, as the widget and menu events always do.  *
* While the seat is in use the rig holds it, and the display ignores the       *
* desk's own pointer and keyboard: keyboardoff and mouseoff are moot.          *
*                                                                              *
* A number is written plain, or as max, -max, max/n or -max/n for the largest  *
* value of the type and fractions of it, which is the range of a joystick's    *
* axes. A character is 'x' or a number.                                        *
*                                                                              *
* Built with AUTO_EVENT_TERMINAL defined the module serves the terminal        *
* tests, with the terminal's event set.                                        *
*                                                                              *
* The test names the file with auto_event_name(), or auto_event_beside() for   *
* the file beside its capture file, and reports where it is with               *
* auto_event_frame() as it numbers its frames and steps. It then asks for      *
* events with auto_event() in place of ami_event(): that gives the next event  *
* from the file when one is ready for the frame the test is on, and the next   *
* event from Ami otherwise. auto_event_ready() says whether the file has one   *
* ready, so an automatic run, which has no user to fall back on, can skip a    *
* pattern's loop when the file has nothing for it.                             *
*                                                                              *
* A sync that the test passes without reaching leaves the file waiting; the    *
* run reports that at its end, on stderr, with the line it stopped at. With    *
* AUTO_EVENT_TRACE set in the environment each event given is reported as it   *
* goes, which is how the frame numbers for a new file are found.               *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifdef AUTO_EVENT_TERMINAL
#include <terminal.h>
#else
#include <graphics.h>
#include <localdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#define MAXPAR 7   /* most parameters an event takes (joymov) */
#define MAXLIN 1000 /* longest line */

/* the event names and the count of parameters each takes */
typedef struct { const char* name; ami_evtcod code; int npar; } evtent;

static const evtent evttbl[] = {

    { "char",     ami_etchar,     1 }, { "up",       ami_etup,       0 },
    { "down",     ami_etdown,     0 }, { "left",     ami_etleft,     0 },
    { "right",    ami_etright,    0 }, { "leftw",    ami_etleftw,    0 },
    { "rightw",   ami_etrightw,   0 }, { "home",     ami_ethome,     0 },
    { "homes",    ami_ethomes,    0 }, { "homel",    ami_ethomel,    0 },
    { "end",      ami_etend,      0 }, { "ends",     ami_etends,     0 },
    { "endl",     ami_etendl,     0 }, { "scrl",     ami_etscrl,     0 },
    { "scrr",     ami_etscrr,     0 }, { "scru",     ami_etscru,     0 },
    { "scrd",     ami_etscrd,     0 }, { "pagd",     ami_etpagd,     0 },
    { "pagu",     ami_etpagu,     0 }, { "tab",      ami_ettab,      0 },
    { "enter",    ami_etenter,    0 }, { "insert",   ami_etinsert,   0 },
    { "insertl",  ami_etinsertl,  0 }, { "insertt",  ami_etinsertt,  0 },
    { "del",      ami_etdel,      0 }, { "dell",     ami_etdell,     0 },
    { "delcf",    ami_etdelcf,    0 }, { "delcb",    ami_etdelcb,    0 },
    { "copy",     ami_etcopy,     0 }, { "copyl",    ami_etcopyl,    0 },
    { "can",      ami_etcan,      0 }, { "stop",     ami_etstop,     0 },
    { "cont",     ami_etcont,     0 }, { "print",    ami_etprint,    0 },
    { "printb",   ami_etprintb,   0 }, { "prints",   ami_etprints,   0 },
    { "fun",      ami_etfun,      1 }, { "menu",     ami_etmenu,     0 },
    { "mouba",    ami_etmouba,    2 }, { "moubd",    ami_etmoubd,    2 },
    { "moumov",   ami_etmoumov,   3 }, { "tim",      ami_ettim,      1 },
    { "joyba",    ami_etjoyba,    2 }, { "joybd",    ami_etjoybd,    2 },
    { "joymov",   ami_etjoymov,   7 },
#ifdef AUTO_EVENT_TERMINAL
    { "resize",   ami_etresize,   2 }, /* the terminal's size is characters */
#else
    { "resize",   ami_etresize,   4 },
#endif
    { "focus",    ami_etfocus,    0 }, { "nofocus",  ami_etnofocus,  0 },
    { "hover",    ami_ethover,    0 }, { "nohover",  ami_etnohover,  0 },
    { "term",     ami_etterm,     0 }, { "frame",    ami_etframe,    0 },
    { "min",      ami_etmin,      0 }, { "max",      ami_etmax,      0 },
    { "norm",     ami_etnorm,     0 }, { "menus",    ami_etmenus,    1 },
#ifdef AUTO_EVENT_TERMINAL
    { "redraw",   ami_etredraw,   0 }, /* the terminal's redraw has no bounds */
#else
    /* the graphical events and the widgets: not in the terminal's set */
    { "redraw",   ami_etredraw,   4 }, { "moumovg",  ami_etmoumovg,  3 },
    { "button",   ami_etbutton,   1 }, { "chkbox",   ami_etchkbox,   1 },
    { "radbut",   ami_etradbut,   1 }, { "sclull",   ami_etsclull,   1 },
    { "scldrl",   ami_etscldrl,   1 }, { "sclulp",   ami_etsclulp,   1 },
    { "scldrp",   ami_etscldrp,   1 }, { "sclpos",   ami_etsclpos,   2 },
    { "edtbox",   ami_etedtbox,   1 }, { "numbox",   ami_etnumbox,   2 },
    { "lstbox",   ami_etlstbox,   2 }, { "drpbox",   ami_etdrpbox,   2 },
    { "drebox",   ami_etdrebox,   1 }, { "sldpos",   ami_etsldpos,   2 },
    { "tabbar",   ami_ettabbar,   2 }, { "usize",    ami_etusize,    0 },
    { "dsize",    ami_etdsize,    0 }
#endif

};
#define NEVT ((int)(sizeof(evttbl)/sizeof(evttbl[0])))

static char       fn[250];      /* the file name, empty for none */
static FILE*      ef;           /* the file, open */
static int        opened;       /* the open was tried */
static int        lineno;       /* line last read */
static int        have;         /* an event waits in pend */
static ami_evtrec pend;         /* the event read ahead */
static int        synced;       /* a sync holds the file */
static int        syncf, syncs; /* the sync's frame and step, step -1 for any */
static int        curf, curs;   /* where the test is */
static ami_long   winid = 1;    /* the window the events carry */
static int        dropkbd;      /* drop the events of the real keyboard */
static int        dropmou;      /* and mouse */
static int        dropjoy;      /* and joystick */
static ami_long   nmouse = -1;  /* the devices as the file sets them, -1 unset */
static ami_long   njoy = -1;
static ami_long   nmoubut = -1;
static ami_long   njoybut = -1;
static ami_long   njoyaxis = -1;
static int        trace;        /* report events given */
static int        draining;     /* a seat line is running its course */
static int        beat;         /* a line has run its course: a step */
static int        skipping;     /* the events of a frame passed by are
                                   skipped, to the next sync */
static int        skipped;      /* events skipped that way */

#ifndef AUTO_EVENT_TERMINAL
/* The seat rig lives in the Wayland display layer: its presence in the
   link says there is one. The remote client and the framebuffer have none */
extern void pd_evtpost(void) __attribute__((weak));

static int        seatfd = -1;  /* the rig fifo, written here */
static int        seattry;      /* the seat was tried */
static char       seatfn[80];   /* the fifo's name */
static int        cwox, cwoy;   /* the client area's offset in the window */
static int        ptrx = 1;     /* the pointer, client coordinates */
static int        ptry = 1;
#endif
#define SEATTIM 10   /* the timer the wait after a seat line, or a wait line, runs on */
#define SEATSET 2000 /* the wait after a seat line, in 100us: 200ms from the line */
static ami_long   waitms;       /* a wait line pending: its milliseconds */

/* report a fault in the file and stop: the file is part of the test */

static void fault(const char* msg, const char* s)

{

    fprintf(stderr, "auto_event: %s:%d: %s: %s\n", fn, lineno, msg, s);
    exit(1);

}

/* fill the event record from its name and parameters */

static void mkevent(const evtent* ep, ami_long* p)

{

    memset(&pend, 0, sizeof(pend));
    pend.winid = winid;
    pend.etype = ep->code;
    switch (ep->code) {

        case ami_etchar:    pend.echar = (char)p[0]; break;
        case ami_ettim:     pend.timnum = p[0]; break;
        case ami_etmoumov:  pend.mmoun = p[0]; pend.moupx = p[1];
                            pend.moupy = p[2]; break;
        case ami_etmouba:   pend.amoun = p[0]; pend.amoubn = p[1]; break;
        case ami_etmoubd:   pend.dmoun = p[0]; pend.dmoubn = p[1]; break;
        case ami_etjoyba:   pend.ajoyn = p[0]; pend.ajoybn = p[1]; break;
        case ami_etjoybd:   pend.djoyn = p[0]; pend.djoybn = p[1]; break;
        case ami_etjoymov:  pend.mjoyn = p[0]; pend.joypx = p[1];
                            pend.joypy = p[2]; pend.joypz = p[3];
                            pend.joyp4 = p[4]; pend.joyp5 = p[5];
                            pend.joyp6 = p[6]; break;
        case ami_etfun:     pend.fkey = p[0]; break;
        case ami_etmenus:   pend.menuid = p[0]; break;
#ifdef AUTO_EVENT_TERMINAL
        case ami_etresize:  pend.rszx = p[0]; pend.rszy = p[1]; break;
#else
        case ami_etresize:  pend.rszx = p[0]; pend.rszy = p[1];
                            pend.rszxg = p[2]; pend.rszyg = p[3]; break;
        case ami_etredraw:  pend.rsx = p[0]; pend.rsy = p[1];
                            pend.rex = p[2]; pend.rey = p[3]; break;
        case ami_etmoumovg: pend.mmoung = p[0]; pend.moupxg = p[1];
                            pend.moupyg = p[2]; break;
        case ami_etbutton:  pend.butid = p[0]; break;
        case ami_etchkbox:  pend.ckbxid = p[0]; break;
        case ami_etradbut:  pend.radbid = p[0]; break;
        case ami_etsclull:  pend.sclulid = p[0]; break;
        case ami_etscldrl:  pend.scldrid = p[0]; break;
        case ami_etsclulp:  pend.sclupid = p[0]; break;
        case ami_etscldrp:  pend.scldpid = p[0]; break;
        case ami_etsclpos:  pend.sclpid = p[0]; pend.sclpos = p[1]; break;
        case ami_etedtbox:  pend.edtbid = p[0]; break;
        case ami_etnumbox:  pend.numbid = p[0]; pend.numbsl = p[1]; break;
        case ami_etlstbox:  pend.lstbid = p[0]; pend.lstbsl = p[1]; break;
        case ami_etdrpbox:  pend.drpbid = p[0]; pend.drpbsl = p[1]; break;
        case ami_etdrebox:  pend.drebid = p[0]; break;
        case ami_etsldpos:  pend.sldpid = p[0]; pend.sldpos = p[1]; break;
        case ami_ettabbar:  pend.tabid = p[0]; pend.tabsel = p[1]; break;
#endif
        default: break; /* no parameters */

    }

}

/* a parameter value: a number, 'x' for a character, or max, -max, max/n,
   -max/n for the largest value of the type and fractions of it */

static int value(char** sp, ami_long* v)

{

    char* s = *sp;
    int   neg = 0;

    if (*s == '\'' && s[1] && s[2] == '\'') { *v = s[1]; *sp = s+3; return (1); }
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (!strncmp(s, "max", 3)) {

        *v = (ami_long)(((unsigned long long)1 << (sizeof(ami_long)*8-1))-1);
        s += 3;
        if (*s == '/') {

            ami_long d = strtol(s+1, &s, 10);
            if (d < 1) return (0);
            *v /= d;

        }

    } else if (isdigit((unsigned char)*s)) *v = strtoll(s, &s, 10);
    else return (0);
    if (neg) *v = -*v;
    *sp = s;
    return (1);

}

/* parse one line: an event into pend, or a sync, window or joystick setting */

static void parse(char* ln)

{

    char*    s;
    char*    name;
    ami_long p[MAXPAR];
    int      np;
    const evtent* ep;
    int      i;

    s = strchr(ln, '#'); /* take off the comment */
    if (s) *s = 0;
    s = ln;
    while (isspace((unsigned char)*s)) s++;
    if (!*s) return; /* blank */
    name = s;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (*s) *s++ = 0;
    if (!strcmp(name, "sync")) {

        /* sync <frame>[.<step>] */
        syncf = (int)strtol(s, &s, 10);
        syncs = -1;
        if (*s == '.') syncs = (int)strtol(s+1, &s, 10);
        while (isspace((unsigned char)*s)) s++;
        if (*s || syncf < 1) fault("bad sync", ln);
        synced = 1;
        return;

    }
    if (!strcmp(name, "window")) {

        winid = strtol(s, &s, 10);
        while (isspace((unsigned char)*s)) s++;
        if (*s) fault("bad window", ln);
        return;

    }
    if (!strcmp(name, "wait")) {

        /* wait <ms>: the program runs, then a beat */
        waitms = strtol(s, &s, 10);
        while (isspace((unsigned char)*s)) s++;
        if (*s || waitms < 1) fault("bad wait", ln);
        have = 1; /* it takes a turn as an event does */
        return;

    }
    /* the real devices set aside or let through */
    if (!strcmp(name, "keyboardoff") || !strcmp(name, "keyboardon") ||
        !strcmp(name, "mouseoff") || !strcmp(name, "mouseon") ||
        !strcmp(name, "joystickoff") || !strcmp(name, "joystickon")) {

        int off = name[strlen(name)-1] == 'f';

        if (name[0] == 'k') dropkbd = off;
        else if (name[0] == 'm') dropmou = off;
        else dropjoy = off;
        while (isspace((unsigned char)*s)) s++;
        if (*s) fault("no parameters", name);
        return;

    }
    /* the devices as the test is told of them */
    if (!strcmp(name, "mousenum") || !strcmp(name, "joysticknum") ||
        !strcmp(name, "mousebutton") || !strcmp(name, "joybutton") ||
        !strcmp(name, "joyaxis")) {

        ami_long n;

        while (isspace((unsigned char)*s)) s++;
        if (!value(&s, &n) || n < 0) fault("count missing or bad", name);
        while (isspace((unsigned char)*s)) s++;
        if (*s) fault("too many parameters", name);
        if (!strcmp(name, "mousenum")) nmouse = n;
        else if (!strcmp(name, "joysticknum")) njoy = n;
        else if (!strcmp(name, "mousebutton")) nmoubut = n;
        else if (!strcmp(name, "joybutton")) njoybut = n;
        else njoyaxis = n;
        return;

    }
    ep = NULL;
    for (i = 0; i < NEVT && !ep; i++)
        if (!strcmp(name, evttbl[i].name)) ep = &evttbl[i];
    if (!ep) fault("unknown event", name);
    /* the parameters */
    for (np = 0; np < ep->npar; np++) {

        while (isspace((unsigned char)*s)) s++;
        if (!value(&s, &p[np])) fault("parameter missing or bad", name);

    }
    while (isspace((unsigned char)*s)) s++;
    if (*s) fault("too many parameters", name);
    mkevent(ep, p);
    have = 1;

}

/* the test is on this frame and step */

static int atsync(void)

{

    return (curf == syncf && (syncs < 0 || curs == syncs));

}

/* read ahead to the next event, unless a sync holds the file */

static void fill(void)

{

    char ln[MAXLIN];

    if (!opened) {

        opened = 1;
        if (fn[0]) ef = fopen(fn, "r");
        trace = getenv("AUTO_EVENT_TRACE") != NULL;

    }
    while (ef && !have) {

        if (synced) {

            if (!atsync()) return; /* the sync holds */
            synced = 0; /* reached: read on */

        }
        if (!fgets(ln, MAXLIN, ef)) { fclose(ef); ef = NULL; return; }
        lineno++;
        parse(ln);
        if (skipping) {

            if (synced) skipping = 0; /* the next frame's block: read on */
            else if (have) { have = 0; skipped++; }

        }

    }

}

/* an event of a device the file has set aside */

static int dropped(ami_evtrec* er)

{

#ifndef AUTO_EVENT_TERMINAL
    if (seatfd >= 0 && er->etype != ami_etjoyba && er->etype != ami_etjoybd &&
        er->etype != ami_etjoymov) return (0); /* the seat's are the file's */
#endif
    if (er->etype >= ami_etchar && er->etype <= ami_etmenu) return (dropkbd);
    if (er->etype == ami_etmouba || er->etype == ami_etmoubd ||
#ifndef AUTO_EVENT_TERMINAL
        er->etype == ami_etmoumovg ||
#endif
        er->etype == ami_etmoumov) return (dropmou);
    if (er->etype == ami_etjoyba || er->etype == ami_etjoybd ||
        er->etype == ami_etjoymov) return (dropjoy);
    return (0);

}

/* The devices as the test is to know them: the file's counts where it sets
   them, Ami's otherwise. The test uses these in place of ami_mouse() and the
   rest. Each reads the file first, so the settings at its start hold from
   the test's first ask. */

ami_long auto_mouse(FILE* f)
    { fill(); return (nmouse >= 0 ? nmouse : ami_mouse(f)); }
ami_long auto_mousebutton(FILE* f, ami_long m)
    { fill(); return (nmoubut >= 0 ? nmoubut : ami_mousebutton(f, m)); }
ami_long auto_joystick(FILE* f)
    { fill(); return (njoy >= 0 ? njoy : ami_joystick(f)); }
ami_long auto_joybutton(FILE* f, ami_long j)
    { fill(); return (njoybut >= 0 ? njoybut : ami_joybutton(f, j)); }
ami_long auto_joyaxis(FILE* f, ami_long j)
    { fill(); return (njoyaxis >= 0 ? njoyaxis : ami_joyaxis(f, j)); }

/* The file the events are read from. It is opened at the first ask, so a
   missing file is no file. */

void auto_event_name(const char* name)

{

    strncpy(fn, name, sizeof(fn)-1);

}

/* The file of that name beside a capture file, in the capture file's
   place, so a run whose captures go to some/where/x.lst takes
   some/where/name.evt from wherever it runs. */

void auto_event_beside(const char* capfile, const char* name)

{

    const char* sl = strrchr(capfile, '/');
    size_t      dl = sl ? (size_t)(sl-capfile+1) : 0;

    char nm[250];

    if (dl+strlen(name) >= sizeof(nm)) dl = 0; /* too long: the name alone */
    memcpy(nm, capfile, dl);
    strcpy(nm+dl, name);
    auto_event_name(nm);

}

/* Where the test is: called as the test numbers its frames and steps. */

void auto_event_frame(int frame, int step)

{

    if (frame != curf) {

        /* a new frame: a line of the last one still running its course is
           let go, its beat with it, and the rest of the last frame's events,
           read ahead or still in the file, are skipped to the next sync:
           a test running a selected range asks after them without running
           the pattern they were for */
        if (draining) ami_killtimer(stdout, SEATTIM);
        draining = 0;
        beat = 0;
        if (ef && !synced) {

            skipping = 1;
            if (have) { have = 0; skipped++; }

        }

    }
    curf = frame;
    curs = step;

}

/* An event is ready in the file for where the test is. */

int auto_event_ready(void)

{

    if (draining) return (1);
    fill();
    return (have);

}

#ifndef AUTO_EVENT_TERMINAL

/* The seat: opened at the first mouse or key event, when the display layer
   carries a rig. A fifo of this process's own is made and named to the
   layer in PD_INPUT, which the layer opens at its next look for input; the
   client area's place in the window is taken from the frame's extents, the
   border being half the width and the rest the title. */

static int seatopen(void)

{

    ami_long wx, wy;

    if (seattry) return (seatfd >= 0);
    seattry = 1;
    if (!pd_evtpost || getenv("PD_INPUT") || getenv("AMI_WL_INPUT"))
        return (0); /* no rig, or one in other hands */
    sprintf(seatfn, "/tmp/ami_seat.%d", (int)getpid());
    unlink(seatfn);
    if (mkfifo(seatfn, 0600)) return (0);
    seatfd = open(seatfn, O_RDWR|O_NONBLOCK);
    if (seatfd < 0) { unlink(seatfn); return (0); }
    setenv("PD_INPUT", seatfn, 1);
    ami_winclientg(stdout, 0, 0, &wx, &wy,
                   BIT(ami_wmframe)|BIT(ami_wmsize)|BIT(ami_wmsysbar));
    cwox = (int)wx/2;
    cwoy = (int)wy-(int)wx/2;
    if (trace) fprintf(stderr, "auto_event: seat %s, client at %d,%d\n",
                       seatfn, cwox, cwoy);

    return (1);

}

static void seatline(const char* ln)

{

    if (trace) fprintf(stderr, "auto_event: seat: %s", ln);
    if (write(seatfd, ln, strlen(ln)) < 0) fault("seat write failed", ln);

}

/* the X keycode of a character on the US layout, and whether it wants a
   shift; 0 for none */

static int keycode(int c, int* shift)

{

    static const char* rows[] = {
        "1234567890-=", "qwertyuiop[]", "asdfghjkl;'`", "\\zxcvbnm,./" };
    static const int  base[] = { 10, 24, 38, 51 };
    static const char* upper = "!@#$%^&*()_+QWERTYUIOP{}ASDFGHJKL:\"~|ZXCVBNM<>?";
    static const char* lower = "1234567890-=qwertyuiop[]asdfghjkl;'`\\zxcvbnm,./";
    const char* u;
    int i;

    *shift = 0;
    if (c == ' ') return (65);
    u = strchr(upper, c);
    if (u && c) { *shift = 1; c = lower[u-upper]; }
    for (i = 0; i < 4; i++) {

        const char* r = strchr(rows[i], c);

        if (r && c) return (base[i]+(int)(r-rows[i]));

    }

    return (0);

}

/* the keycode of a key event, 0 for none */

static int keyof(ami_evtcod e)

{

    switch (e) {

        case ami_etenter:   return (36);
        case ami_etdelcb:   return (22);
        case ami_etdelcf:   return (119);
        case ami_ettab:     return (23);
        case ami_etup:      return (111);
        case ami_etdown:    return (116);
        case ami_etleft:    return (113);
        case ami_etright:   return (114);
        case ami_ethomel:   return (110);
        case ami_etendl:    return (115);
        case ami_etpagu:    return (112);
        case ami_etpagd:    return (117);
        case ami_etinsertt: return (118);
        default:            return (0);

    }

}

/* Put the pending event in at the seat if it is one the seat can make:
   1 when it went that way, 0 when it is the program's directly. */

static int seatput(void)

{

    char ln[80];
    int  k, shift;

    switch (pend.etype) {

        case ami_etmoumovg:
        case ami_etmoumov:
        case ami_etmouba:
        case ami_etmoubd:
        case ami_etchar:
            break;
        default:
            if (!keyof(pend.etype)) return (0);

    }
    if (!seatopen()) return (0);
    ami_timer(stdout, SEATTIM, SEATSET, FALSE); /* the moment for the line */
    switch (pend.etype) {

        case ami_etmoumovg:
            ptrx = (int)pend.moupxg;
            ptry = (int)pend.moupyg;
            sprintf(ln, "move %d %d\n", cwox+ptrx-1, cwoy+ptry-1);
            seatline(ln);
            break;
        case ami_etmoumov: /* character cells: their centers */
            ptrx = (int)((pend.moupx-1)*ami_chrsizx(stdout)+ami_chrsizx(stdout)/2+1);
            ptry = (int)((pend.moupy-1)*ami_chrsizy(stdout)+ami_chrsizy(stdout)/2+1);
            sprintf(ln, "move %d %d\n", cwox+ptrx-1, cwoy+ptry-1);
            seatline(ln);
            break;
        case ami_etmouba:
            sprintf(ln, "btndown %d %d %d\n", (int)pend.amoubn, cwox+ptrx-1, cwoy+ptry-1);
            seatline(ln);
            break;
        case ami_etmoubd:
            sprintf(ln, "btnup %d %d %d\n", (int)pend.dmoubn, cwox+ptrx-1, cwoy+ptry-1);
            seatline(ln);
            break;
        case ami_etchar:
            k = keycode((unsigned char)pend.echar, &shift);
            if (!k) fault("no key for the character", "char");
            if (shift) seatline("keydown 50\n");
            sprintf(ln, "key %d\n", k);
            seatline(ln);
            if (shift) seatline("keyup 50\n");
            break;
        default:
            sprintf(ln, "key %d\n", keyof(pend.etype));
            seatline(ln);

    }

    return (1);

}

#else

static int seatput(void) { return (0); }

#endif

/* the wait after a seat line, or of a wait line: a fixed moment, armed as
   the line goes in, during which the program's events come to it one by
   one; the moment's end is the beat. Fixed, not quiet, since a program
   with the frame timer on is never quiet. */

static void seatdrain(FILE* f, ami_evtrec* er)

{

    do { ami_event(f, er); } while (dropped(er));
    if (trace) fprintf(stderr, "auto_event: drain: event %d window %d\n",
                       (int)er->etype, (int)er->winid);
    if (er->etype == ami_ettim && er->timnum == SEATTIM) {

        draining = 0;
        beat = 1;

    }

}

/* The next event: from the file if one is ready, else from Ami. A mouse or
   key event of the file goes in at the seat where there is one, and what
   it provokes comes back through Ami. */

void auto_event(FILE* f, ami_evtrec* er)

{

    if (draining) { seatdrain(f, er); return; }
    fill();
    if (have) {

        if (trace) fprintf(stderr, "auto_event: line %d: event %d at frame %d.%d\n",
                           lineno, (int)pend.etype, curf, curs);
        have = 0;
        if (waitms) {

            /* a wait line: the program runs its course for the moment */
            ami_timer(stdout, SEATTIM, waitms*10, FALSE);
            waitms = 0;
            draining = 1;
            seatdrain(f, er);

        } else if (seatput()) {

            draining = 1;
            seatdrain(f, er);

        } else {

            *er = pend;
            beat = 1; /* the program has it: a step */

        }

    } else do { ami_event(f, er); } /* the devices set aside are skipped */
    while (dropped(er));

}

/* A line of the file has run its course since the last ask: the program has
   been given what it provoked, and the screen shows the result. Once. */

int auto_event_step(void)

{

    int b = beat;

    beat = 0;

    return (b);

}

/* At the end, a file not used up is reported: a sync the test never reached,
   or events it never asked for. A file read to its end says nothing. */

__attribute__((destructor))
static void auto_event_fini(void)

{

#ifndef AUTO_EVENT_TERMINAL
    if (seatfd >= 0) { close(seatfd); unlink(seatfn); }
#endif
    /* skipped events are reported where the seat is in use: without one a
       key that would have gone to a widget ends a pattern's loop early,
       and its leftovers are skipped as a matter of course */
#ifndef AUTO_EVENT_TERMINAL
    if (skipped && (seatfd >= 0 || trace))
#else
    if (skipped && trace)
#endif
        fprintf(stderr, "auto_event: %s: %d events of frames passed by were skipped\n",
                fn, skipped);
    if (ef && !synced && !have) fill(); /* see if anything is left */
    if (!ef && !have) return; /* used up */
    if (synced && syncs < 0)
        fprintf(stderr, "auto_event: %s: left waiting at line %d for frame %d\n",
                fn, lineno, syncf);
    else if (synced)
        fprintf(stderr, "auto_event: %s: left waiting at line %d for frame %d.%d\n",
                fn, lineno, syncf, syncs);
    else
        fprintf(stderr, "auto_event: %s: events from line %d were not used\n",
                fn, lineno);
    if (ef) fclose(ef);

}
