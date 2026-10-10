/*******************************************************************************
*                                                                              *
*                        REMOTE DISPLAY SERVER MODULE                          *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                            2026/08/14 S. A. Franco                           *
*                                                                              *
* The server side of remote display mode: the display, and the user, are      *
* here. The server links with the standard graphics package and runs          *
* standalone. The main thread runs the command loop: wait for a message of    *
* the remote display protocol (graph_remote.h, and the paper                  *
* doc/remote.pdf), execute it against the display library, and reply if the   *
* call returns values. A second thread runs the event pump: call event()      *
* forever and send each event out on the event channel the moment it is      *
* ready; events are pushed, never polled.                                     *
*                                                                              *
* The command port comes from GRAPH_PORT, GR_DEFPORT if unset; the event      *
* channel is the next port, and the file port the one after. The server      *
* holds the file cache in its current directory, filling it by the file      *
* transfer exchange: on a miss it names its file port in the request, and    *
* the client connects and streams the file in, close delimited.               *
*                                                                              *
* The server serves one client at a time, and when the client says bye it    *
* resets and cycles back to wait for the next connection. The server has no  *
* window of its own on the display: a program's window appears when it       *
* connects and goes when it ends, holding at the end if its autohold is on,  *
* as it would on its own display. Closing the window on a running program    *
* ends it. The console interrupt shuts the server down. It must be started   *
* before its client.                                                          *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <setjmp.h>
#include <stdarg.h>
#ifndef _WIN32
#include <execinfo.h>
#endif

#include <graphics.h>
#include <network.h>
#include <graph_remote.h>
#include <widget_base.h>
#include <sound.h>
#include <services.h>

#define MAXHND 512   /* window handles */
#define MAXSTR 4096  /* string unmarshal bound */

/* channels */
static ami_long cmdfn = -1;  /* command channel */
static ami_long evtfn = -1;  /* event channel */
static ami_long srvport;     /* command port */
static ami_long msgmax;      /* channel message size bound */
static int  gtrace;      /* diagnostic trace: --trace or GRAPH_TRACE;
                            every message prints to the error channel.
                            Slow, and worth it. */
/* Secure channels are the default, and --plain is the way out of them.
   They cost nothing worth having: measured against this server over
   loopback, drawing runs at the same rate either way -- the time is the
   server's rendering, not the channel -- and a round trip costs 3.5
   microseconds more, which is under a thousandth of what a network round
   trip costs. Anyone carrying a display over a wire wants it encrypted. */
static int  gsecure = 1; /* secure channels: --plain turns them off; DTLS on the
                            messages, TLS on the file connections */

/* message assembly */
static unsigned char* sbuf; /* reply buffer */
static ami_long       soff;
static unsigned char* rbuf; /* receive buffer: points at the message
                               being executed, which may sit inside a
                               batched datagram */
static unsigned char* rbase; /* the buffer datagrams land in */
static ami_long       roff;
static ami_long       rlen;

/* the event channel is written by the pump thread and, for errors, the
   main thread; the lock keeps the messages whole */
static ami_long        evsend;     /* services lock: one sender at a time */
static int             evsendheld; /* the send holds it; a fault must free it */

/* The display library is multithreadable and carries its own locks: the
   pump's event() runs concurrently with command execution, and any
   deadlock between them is the library's bug, to be fixed there. The
   pump winds down at session end by a wake event injected from the main
   thread. */
static int byeseen;   /* the client said bye; wind down */
static int  hellopend;   /* a mid-session hello opened the next session */

/* The pump lives across sessions: it forwards events while a client is
   up, discards them while the server is idle, and treats a terminate
   from the display, a window's close button or the console interrupt,
   as the client's while one is up, as the end of the hold while a
   program's window is held, and as cancellation on an idle server. It
   never returns: the server's own exit is taken on this thread, see
   srvexit. */
static ami_long  pump;     /* the pump, by services thread id */

/* The pump stands down while a modal dialog runs. A dialog pumps its
   own events on the dispatch thread, and two threads inside event()
   split the stream between them: the pump would steal the dialog's
   paints and clicks and forward them to a client that has never heard
   of the dialog's window. The dispatch thread asks, the pump parks
   between events, and the dialog is the only consumer until it is
   dismissed -- which is also exactly the native picture, where the
   program is inside the dialog and consuming nothing else. */
/* Network channel failure recovery. A vanished secure peer surfaces
   as a network error where a clear channel just falls silent; the
   handler routes it to the recovery matching the thread it fired on:
   the dispatch thread drops the session, the pump escapes its send and
   idles, and a failure inside a deliberate close simply completes the
   close. */
/* The session's sound footprint. A session that dies leaves its
   ports open, its timers running and its stores loaded, and the next
   session's own opens would collide with them; the winddown returns
   what the session took, as it does for windows and widgets. */
#define SNDPORTS 100 /* matching the library's port maximum */
static char sesso[SNDPORTS]; /* synth out ports opened */
static char sessi[SNDPORTS]; /* synth in ports opened */
static char seswo[SNDPORTS]; /* wave out ports opened */
static char seswi[SNDPORTS]; /* wave in ports opened */
static char sessy[SNDPORTS]; /* synth stores loaded */
static char seswv[SNDPORTS]; /* waves loaded */
static char sesto;           /* output time base started */
static char sesti;           /* input time base started */

static jmp_buf   pumpjmp;    /* the pump's escape from a dead send */
static int       pumpjmpset; /* it is armed */
static int       onpump;     /* set on the pump's own thread */
static jmp_buf   closejmp;   /* escape for a failing deliberate close */
static int       inclose;    /* a deliberate close is in progress */

static int       clientup; /* a client is connected */
static int       sigasked; /* a terminate asked this session to end */

/* the wake: a user event code, injected to return the pump from event()
   and never forwarded */
#define GR_EVWAKE (ami_etuser+0x100)

/* window handle map */
static FILE* h2f[MAXHND];  /* handle to window file */
static ami_long  h2lw[MAXHND]; /* handle to logical window id */

/* The session's window. The library's main window is the server's own
   and is never written to, so it never shows: on every desktop port a
   window appears on its first output. A client's handle 1 is a window of
   its own, opened when its session opens and closed when the session
   ends, so a program's window comes and goes on the server's display as
   it would on its own. The hold at the end is the program's autohold,
   kept for the session instead of the library's: the window stays, its
   title marked, until it is closed or the next client comes. */
#define SESWID 2 /* the session window's logical id; a client's other
                    windows get their handle plus one, so none is 1, the
                    main window's */
#define MAXLW 100 /* logical ids the display library accepts, 1 to this */
static unsigned char lwused[MAXLW+1]; /* the ids the session's windows hold */
static int  sesautohold;   /* the session's autohold, on by default */
static char sestitle[256]; /* the title the session set, for the hold */
static char sespgm[128];   /* the program's name, from its hello */
static int  holding;       /* the program ended and its window is held */

/*******************************************************************************

Report error and stop

*******************************************************************************/

static void error(const char* es)

{

    write(2, "*** graph_server: ", 18);
    write(2, es, strlen(es));
    write(2, "\n", 1);
    exit(1);

}

/* A status line, to the standard error the server was started from:
   the server's life as seen from the shell, listening, a client's coming
   and going, and the shutdown. The window shows the same while idle, but
   the shell is where an operator watches a server. */

static void status(const char* fmt, ...)

{

    va_list ap;

    fprintf(stderr, "graph_server: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);

}

/* the waiting line, before whatever blocks for the client: the secure
   wait is the handshake itself, the plain wait is the hello read */
static void waiting(void)

{

    status("waiting for %sconnection on port %lld, events on port %lld",
           gsecure? "secure ": "", AMI_LONG_CAST(srvport),
           AMI_LONG_CAST(srvport+1));

}

/* A session error: the client's fault or the wire's, not the server's.
   The error prints, the session drops, and the server recycles to wait
   for a new connection; only faults of the server itself exit. The
   jump lands in the session loop, which winds down whatever the
   session had running. */


static jmp_buf sesjmp;    /* recycle point */
static int     sesjmpset; /* the recycle point is armed: the serving loop runs */
static int     pumping;   /* the pump thread is up */
static int     cleaning;  /* winding down; a second fault is fatal */

/* The display thread's work mailbox; see ondisplay() below. The lock and
   the signal are services' own, so the handoff is the same on any system
   services runs on: waitsig releases the lock, waits, and takes it back. */
static ami_long        joblock;    /* services concurrency lock */
static ami_long        jobdone;    /* services signal: the job is finished */
static void          (*jobfn)(void*); /* the call to make, NULL for none */
static void*           jobarg;
static int             jobsts;     /* the job took a session error */
static char            jobmsg[256];/* the text of that error */
static jmp_buf         jobjmp;     /* the pump's own way out of a fault */
static int             jobjmpset;
static int             injob;      /* the pump is inside a posted job */
static int             inpumpevt;  /* the pump is forwarding an event */


static void sesserr(const char* es)

{

    /* On the display's thread inside a posted job the way out is back to
       the poster: it owns the session and the session's jump, and it is
       the one that reports and drops. */
    if (!cleaning && jobjmpset) { /* only the pump sets it, and only in a job */

        snprintf(jobmsg, sizeof(jobmsg), "%s", es);
        longjmp(jobjmp, 1);

    }
    write(2, "*** graph_server: ", 18);
    write(2, es, strlen(es));
    if (cleaning || !sesjmpset) {

        /* faulted while winding down, or before the serving loop armed its
           recycle point (at startup, the channels not yet up): there is no
           session to drop back to, and a jump through an unset recycle
           point is a crash. The fault is fatal. */
        write(2, " -- fatal\n", 10);
        exit(1);

    }
    write(2, " -- session dropped\n", 20);
    longjmp(sesjmp, 1);

}

/*******************************************************************************

Message assembly

*******************************************************************************/

static gr_msghdr* shdr(void) { return ((gr_msghdr*)sbuf); }
static gr_msghdr* rhdr(void) { return ((gr_msghdr*)rbuf); }
static void dgram(ami_long dlen);

/* unmarshal from the received message */
static ami_long gi(void)

{

    ami_long v;

    if (roff+8 > rlen) sesserr("Message truncated");
    memcpy(&v, rbuf+roff, 8);
    roff += 8;

    return (v);

}

static double gf(void)

{

    double v;

    if (roff+8 > rlen) sesserr("Message truncated");
    memcpy(&v, rbuf+roff, 8);
    roff += 8;

    return (v);

}

static int g4(void)

{

    int v;

    if (roff+4 > rlen) sesserr("Message truncated");
    memcpy(&v, rbuf+roff, 4);
    roff += 4;

    return (v);

}

/* a counted string into a bounded buffer, terminated */
static void gstr(char* dst, ami_long dl)

{

    ami_long n = g4();
    ami_long c = n;

    if (roff+n > rlen) sesserr("Message truncated");
    if (c > dl-1) c = dl-1;
    memcpy(dst, rbuf+roff, c);
    dst[c] = 0;
    roff += n;

}

/* a counted string, allocated */
static char* gsdup(void)

{

    ami_long  n = g4();
    char* s;

    if (roff+n > rlen) sesserr("Message truncated");
    s = malloc(n+1);
    if (!s) error("Out of memory");
    memcpy(s, rbuf+roff, n);
    s[n] = 0;
    roff += n;

    return (s);

}

/* a menu tree, allocated. The display library may hold the tree for the
   life of the menu, so it is not freed after the call. */
static ami_menuptr gmenu(void)

{

    ami_menuptr lst = NULL;
    ami_menuptr lp = NULL;
    ami_menuptr e;
    int         n, i;
    ami_long    flags;

    n = g4();
    for (i = 0; i < n; i++) {

        e = malloc(sizeof(ami_menurec));
        if (!e) error("Out of memory");
        flags = gi();
        e->onoff = !!(flags&1);
        e->oneof = !!(flags&2);
        e->bar = !!(flags&4);
        e->id = gi();
        e->face = gsdup();
        e->branch = gmenu();
        e->next = NULL;
        if (lp) lp->next = e; else lst = e;
        lp = e;

    }

    return (lst);

}

/* a string list, allocated; the display library may hold it */
static ami_strptr gslst(void)

{

    ami_strptr lst = NULL;
    ami_strptr lp = NULL;
    ami_strptr e;
    int        n, i;

    n = g4();
    for (i = 0; i < n; i++) {

        e = malloc(sizeof(ami_strrec));
        if (!e) error("Out of memory");
        e->str = gsdup();
        e->next = NULL;
        if (lp) lp->next = e; else lst = e;
        lp = e;

    }

    return (lst);

}

/* reply assembly */
static void rbegin(void)

{

    shdr()->mid = GR_MREPLY;
    shdr()->seq = rhdr()->seq;
    shdr()->wid = 0;
    soff = sizeof(gr_msghdr);

}

static void ri(ami_long v)

{

    if (soff+8 > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, &v, 8);
    soff += 8;

}

static void rf(double v)

{

    if (soff+8 > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, &v, 8);
    soff += 8;

}

static void r4(int v)

{

    if (soff+4 > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, &v, 4);
    soff += 4;

}

/* a string by the critical buffer rule: the length is the occupancy, which
   may be the full buffer with no terminator */
static void rstrn(const char* s, ami_long n)

{

    r4((int)n);
    if (soff+n > msgmax) sesserr("Reply too large for channel");
    memcpy(sbuf+soff, s, n);
    soff += n;

}

static void rstr(const char* s, ami_long bl)

{

    rstrn(s, strnlen(s, bl));

}

static void rmenu(ami_menuptr m)

{

    ami_menuptr p;
    int         n = 0;

    for (p = m; p; p = p->next) n++;
    r4(n);
    for (p = m; p; p = p->next) {

        ri((!!p->onoff)|((!!p->oneof)<<1)|((!!p->bar)<<2));
        ri(p->id);
        rstrn(p->face, p->face? strlen(p->face): 0);
        rmenu(p->branch);

    }

}

static void rsend(void)

{

    shdr()->len = (int)soff;
    if (gtrace)
        fprintf(stderr, "gs> %-16s w%-3lld s%-5d len%lld\n",
                gr_msgname(shdr()->mid), AMI_LONG_CAST(shdr()->wid), shdr()->seq, AMI_LONG_CAST(soff));
    ami_wrmsg(cmdfn, sbuf, soff);

}

/*******************************************************************************

Window handles

*******************************************************************************/

static FILE* wf(ami_long h)

{

    if (h < 1 || h >= MAXHND || !h2f[h]) sesserr("Invalid window handle");

    return (h2f[h]);

}

/* handle from the logical window id of an event */
static ami_long lw2h(ami_long lw)

{

    ami_long h;

    for (h = 1; h < MAXHND; h++)
        if (h2f[h] && h2lw[h] == lw) return (h);

    return (lw); /* unknown: pass as is */

}

/*******************************************************************************

File transfer

The cache is the current directory. A miss requests the file: the request
names our file port, the client connects and streams the file in, close
delimited. An empty stream is a file the client does not hold either, and
the display library's complaint follows when the call proceeds.

*******************************************************************************/

/* base name of a client supplied name: never a path on our side */
static const char* basenm(const char* fn)

{

    const char* p = strrchr(fn, '/');

    return (p? p+1: fn);

}

/* The picture extension rule of the display library: .bmp is set or
   overwritten. The names cross the wire as given, and both ends apply
   the rule where they touch a file. */
static void setfext(char* dst, ami_long dl, const char* fn, const char* ext)

{

    const char* dot = strrchr(fn, '.');
    const char* sl = strrchr(fn, '/');
    ami_long    n;

    if (dot && (!sl || dot > sl)) n = dot-fn; /* strip the extension */
    else n = strlen(fn);
    if (n > dl-(ami_long)strlen(ext)-1) n = dl-strlen(ext)-1;
    memcpy(dst, fn, n);
    strcpy(dst+n, ext);

}

/* find or fetch a named file; returns the name to use */
static const char* fndfile(const char* fn, char* cb, ami_long cbl,
                           const char* ext)

{

    FILE* tf;
    FILE* nf;
    FILE* lf;
    char  buf[4096];
    size_t r;

    char en[MAXSTR]; /* the name under the extension rule */

    /* the name as given, then the base name in the cache, both under
       the extension rule of the library: .bmp for pictures, .wav for
       waves, .mid for stored sequences */
    setfext(en, MAXSTR, fn, ext);
    tf = fopen(en, "rb");
    if (tf) { fclose(tf); snprintf(cb, cbl, "%s", en); return (cb); }
    setfext(cb, cbl, basenm(fn), ext);
    tf = fopen(cb, "rb");
    if (tf) { fclose(tf); return (cb); }
    /* Request it: the client connects to our file port and streams it.
       The request carries the name under the extension rule, and the
       client serves exactly the name asked. */
    shdr()->mid = GR_MFILEREQ;
    shdr()->seq = 0;
    shdr()->wid = 0;
    soff = sizeof(gr_msghdr);
    rstrn(en, strlen(en));
    ri(srvport+2);
    shdr()->len = (int)soff;
    ami_wrmsg(cmdfn, sbuf, soff);
    nf = ami_waitnet(srvport+2, gsecure);
    if (!nf) sesserr("Cannot open file transfer connection");
    lf = fopen(cb, "wb");
    if (!lf) sesserr("Cannot write file cache");
    while ((r = fread(buf, 1, sizeof(buf), nf)) > 0)
        if (fwrite(buf, 1, r, lf) != r) sesserr("Cannot write file cache");
    fclose(lf);
    fclose(nf);

    return (cb);

}

/*******************************************************************************

Event pump

The second thread: take each event from the display library as it comes
and push it out, translating the logical window id to the wire handle and
carrying the event union raw, as the same width hosts of the protocol
allow.

*******************************************************************************/

#define EVUNION (7*8)

/* Event thinning: the movement streams, mouse, graphical mouse and
   joystick, and window resizes, arrive faster than they matter, a
   joystick idling at full rate being the standing example. Each stream
   sends at most one event per interval per device; between sends the
   latest holds as pending. Ordering stays truthful where it counts: a
   pending movement flushes before any other event goes out, so a click
   always follows the position it happened at, and only the trailing
   rest position can lag, until the next event of any kind carries it. */

#define THINMS   10  /* the interval, milliseconds */
#define THINDEV  8   /* devices per stream */
#define THINRSZ  16  /* resize slots */

typedef struct thinslot {

    int        dirty;   /* a pending event holds */
    double     last;    /* when one last sent */
    gr_msgevt  ev;      /* the pending event */

} thinslot;

static thinslot thmou[THINDEV];  /* mouse moves, by mouse */
static thinslot thmoug[THINDEV]; /* graphical mouse moves */
static thinslot thjoy[THINDEV];  /* joystick moves, by stick */
static thinslot thrsz[THINRSZ];  /* resizes, by window handle */

static double thnow(void)

{

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec+ts.tv_nsec/1e9);

}

/* drop pending thinned events; a new session never sees stale ones */
static void thinclear(void)

{

    int i;

    for (i = 0; i < THINDEV; i++)
        thmou[i].dirty = thmoug[i].dirty = thjoy[i].dirty = 0;
    for (i = 0; i < THINRSZ; i++) thrsz[i].dirty = 0;

}

/* the thin slot for a wire event, or NULL if the type never thins */
static thinslot* thinfor(gr_msgevt* we)

{

    ami_long d;

    switch (we->etype) {

        case ami_etmoumov:  d = we->p[0]; 
            return (d >= 1 && d <= THINDEV)? &thmou[d-1]: NULL;
        case ami_etmoumovg: d = we->p[0];
            return (d >= 1 && d <= THINDEV)? &thmoug[d-1]: NULL;
        case ami_etjoymov:  d = we->p[0];
            return (d >= 1 && d <= THINDEV)? &thjoy[d-1]: NULL;
        case ami_etresize:  d = we->winid;
            return (d >= 1 && d <= THINRSZ)? &thrsz[d-1]: NULL;
        default: return (NULL);

    }

}

static void evsend1(gr_msgevt* we);

/* flush every pending movement, before any other event overtakes it */
static void thinflush(void)

{

    int i;

    for (i = 0; i < THINDEV; i++) {

        if (thmou[i].dirty)  { thmou[i].dirty = 0;  evsend1(&thmou[i].ev); }
        if (thmoug[i].dirty) { thmoug[i].dirty = 0; evsend1(&thmoug[i].ev); }
        if (thjoy[i].dirty)  { thjoy[i].dirty = 0;  evsend1(&thjoy[i].ev); }

    }
    for (i = 0; i < THINRSZ; i++)
        if (thrsz[i].dirty) { thrsz[i].dirty = 0; evsend1(&thrsz[i].ev); }

}

/* put one event on the wire */
static void evsend1(gr_msgevt* we)

{

    unsigned char ebuf[sizeof(gr_msghdr)+sizeof(gr_msgevt)];
    gr_msghdr*    h = (gr_msghdr*)ebuf;

    if (gtrace)
        fprintf(stderr, "gs>e %-15s w%lld\n",
                gr_evtname((ami_long)we->etype), AMI_LONG_CAST(we->winid));
    h->len = sizeof(ebuf);
    h->mid = GR_MEVENT;
    h->seq = 0;
    h->wid = 0;
    memcpy(ebuf+sizeof(gr_msghdr), we, sizeof(gr_msgevt));
    /* the write does not return on failure, it jumps: the flag is how the
       fault path knows to free this */
    ami_lock(evsend);
    evsendheld = 1;
    ami_wrmsg(evtfn, ebuf, sizeof(ebuf));
    evsendheld = 0;
    ami_unlock(evsend);

}

/* the network error handler: see the recovery state above */
static void neterr(const char* es)

{

    (void)es;
    /* Every way out of here is a jump, and a send that failed under the
       event channel's lock still holds it. Left held, the next event to
       go out waits on it forever, and that is the whole server: the
       thread that sends events is the thread that owns the display. */
    if (evsendheld) {

        evsendheld = 0;
        ami_unlock(evsend);

    }
    if (inclose) longjmp(closejmp, 1); /* the close completes */
    /* Inside a posted job this is a reply that could not be sent, and the
       wire thread is waiting on that job: the way out is back to it, or it
       waits for a job that will never finish. sesserr takes that route. */
    if (jobjmpset) { /* a reply that could not go: back to the poster */

        clientup = 0;
        sesserr("Network channel failed");

    }
    if (inpumpevt) { /* an event that could not go */

        /* The client is gone. The pump idles on; the wire thread, asleep
           in its channel read, is woken by shutting the command socket
           under it, and its failing read drops the session. */
        clientup = 0;
        ami_shutmsg(cmdfn);
        if (pumpjmpset) longjmp(pumpjmp, 1);

    } else sesserr("Network channel failed");

}

/*******************************************************************************

The display belongs to one thread

The pump thread owns the display: it is the only thread that calls into the
display library, whether to take an event or to make a call the client asked
for. The thread reading the wire owns nothing but the wire, and hands its work
across with ondisplay(), which posts the call and waits for it.

A window made, sized or freed under a thread that is drawing its frame is a
use after free whichever way the two threads interleave. One thread in the
library is what makes that impossible, rather than a guard around each call
that can meet it.

The post is by the event queue, which is locked and made for this, so a job
waiting cannot be missed: the wake either breaks the pump out of event() or is
already in the queue when it gets there.

*******************************************************************************/

/* A trace of the handoff, for when it does not happen: GS_JOBTRACE in the
   environment turns it on. It says which thread and what it did, so a stall
   shows as the last thing said. */

static int jobtrace = -1;

static void jt(const char* what)

{

    char b[128];
    int  n;

    if (jobtrace < 0) jobtrace = getenv("GS_JOBTRACE") != NULL;
    if (!jobtrace) return;
    n = snprintf(b, sizeof(b), "[%s] %s\n", onpump? "pump": "wire", what);
    write(2, b, n);

}

/* wake the pump, whatever it is doing */
static void wakepump(void)

{

    ami_evtrec wake;

    memset(&wake, 0, sizeof(wake));
    wake.etype = (ami_evtcod)GR_EVWAKE;
    ami_sendevent(stdout, &wake);

}

/* make this call on the display's thread, and wait for it.

   Called on the pump itself it just makes the call: the display is already
   this thread's, and a job posted to ourselves would wait forever. */

static void ondisplay(void (*fn)(void*), void* arg)

{

    if (injob) { fn(arg); return; } /* already on the display's thread */
    ami_lock(joblock);
    jobfn = fn;
    jobarg = arg;
    jobsts = 0;
    ami_unlock(joblock);
    jt("posted");
    wakepump();
    jt("woke the pump, waiting");
    ami_lock(joblock);
    while (jobfn) ami_waitsig(joblock, jobdone);
    ami_unlock(joblock);
    jt("job done");
    /* a fault in the job is taken here, on the thread that owns the session */
    if (jobsts) sesserr(jobmsg);

}

/* run a posted job, if there is one. The pump calls this each time round,
   before it goes back to the display for the next event. */

static void runjob(void)

{

    void (*fn)(void*);
    void*  arg;

    ami_lock(joblock);
    fn = jobfn;
    arg = jobarg;
    ami_unlock(joblock);
    if (!fn) return;
    jt("running a job");
    if (!setjmp(jobjmp)) {

        jobjmpset = 1;
        injob = 1;
        fn(arg);
        jobsts = 0;

    } else jobsts = 1; /* the job faulted; the poster reports it */
    jobjmpset = 0;
    injob = 0;
    ami_lock(joblock);
    jobfn = NULL;
    ami_sendsig(jobdone);
    ami_unlock(joblock);
    jt("job finished, back to the display");

}

/* the jobs the wire thread posts */
static void job_dgram(void* a) { (void)a; dgram(rlen); }
static void job_openses(void* a)

{

    FILE* inf = stdin; /* its input joins the queue the pump reads */
    FILE* outf = NULL;

    (void)a;
    ami_openwin(&inf, &outf, NULL, SESWID);
    h2f[1] = outf;
    h2lw[1] = SESWID;
    /* titled with the program's name, as the native library titles the
       window of a program that sets no title */
    if (sespgm[0]) ami_title(outf, sespgm);
    thinclear(); /* and no pending events from a prior session */

}

/* the hold: the program is gone and its window stays, its title marked
   the way the native library marks a held window */
static void job_hold(void* a)

{

    char t[sizeof(sestitle)+16];

    (void)a;
    /* the native library marks a held window with the program's name;
       the title the program set stands in for a client that sent none */
    snprintf(t, sizeof(t), "Finished - %s", sespgm[0]? sespgm: sestitle);
    ami_title(h2f[1], t);

}

/* Reset for the next client, on the thread that owns the display: the
   session's windows close, its widgets and its sound go with them. A
   timer the session left running is not recovered, the library having no
   way to ask, and would tick into the next session. */

static void job_winddown(void* a)

{

    ami_long h;

    (void)a;
#ifndef _WIN32
    if (h2f[1]) wb_purge(h2f[1]); /* the session's widgets go with it */
#endif
    /* the session's windows close, its own last: it is the parent of
       whatever the session hung on it */
    for (h = MAXHND-1; h >= 1; h--) if (h2f[h]) {

        fclose(h2f[h]);
        h2f[h] = 0;
        h2lw[h] = 0;

    }
    memset(lwused, 0, sizeof(lwused)); /* the ids are all free again */
    {

        /* and its sound footprint: ports, timers and stores */
        int sp;

        for (sp = 0; sp < SNDPORTS; sp++) {

            if (sesso[sp]) { ami_closesynthout(sp+1); sesso[sp] = 0; }
            if (sessi[sp]) { ami_closesynthin(sp+1); sessi[sp] = 0; }
            if (seswo[sp]) { ami_closewaveout(sp+1); seswo[sp] = 0; }
            if (seswi[sp]) { ami_closewavein(sp+1); seswi[sp] = 0; }
            if (sessy[sp]) { ami_delsynth(sp+1); sessy[sp] = 0; }
            if (seswv[sp]) { ami_delwave(sp+1); seswv[sp] = 0; }

        }
        if (sesto) { ami_stoptimeout(); sesto = 0; }
        if (sesti) { ami_stoptimein(); sesti = 0; }

    }

}

/* The server's own exit, from the pump. The wire thread is blocked in the
   network, in the secure listen or the hello read, and nothing short of a
   client wakes it. The destructors that exit() would run take the network
   down under it, and it faults in the freed channel: a segmentation fault
   in the DTLS listen at every shutdown under the debugger, now and then
   without it. So the process ends without them: the system reclaims the
   sockets and the windows, and nothing here is left to flush. */
static void srvexit(void)

{

    _exit(1);

}

/* End the hold, if one is on. Either thread may see its end first, the
   pump on the window's close or the wire thread on the next client's
   hello: the one that takes it under the lock ends it and the other
   finds it gone. The winddown runs on the display's thread either way,
   directly on the pump, posted from the wire. Returns whether there was
   a hold to end. */
static int endhold(int onpumpthread)

{

    int h;

    ami_lock(joblock);
    h = holding;
    holding = 0;
    ami_unlock(joblock);
    if (h) {

        cleaning = 1; /* a fault in the winddown is fatal */
        if (onpumpthread) job_winddown(NULL);
        else ondisplay(job_winddown, NULL);
        cleaning = 0;

    }

    return (h);

}

static void evpump(void)

{

    unsigned char ebuf[sizeof(gr_msghdr)+sizeof(gr_msgevt)];
    gr_msghdr*    h = (gr_msghdr*)ebuf;
    gr_msgevt*    we = (gr_msgevt*)(ebuf+sizeof(gr_msghdr));
    ami_evtrec    er;

    onpump = 1; /* everything below this is the pump's */
    jt("pump entered");
    for (;;) {

        if (setjmp(pumpjmp)) {

            /* a send failed under this pass: the client is gone and
               the event is dropped; the session drop is the wire
               thread's to notice */

        }
        pumpjmpset = 1;
        inpumpevt = 0; /* this pass is not in the forward yet */
        /* the wire thread's work first: it is waiting on it, and the
           display is this thread's to give */
        runjob();
        ami_event(stdin, &er);
        if ((ami_long)er.etype == GR_EVWAKE) continue; /* ours: a job */
        if (gtrace && !clientup)
            fprintf(stderr, "gs.e %-15s w%lld (idle)\n",
                    gr_evtname((ami_long)er.etype), AMI_LONG_CAST(er.winid));
        if (er.etype == ami_etterm) {

            /* A terminate: a window's close button, or control-c in the
               shell that started the server, which the display library
               delivers as this same event. With a client up it is the
               client's: the terminate forwards, the program exits as it
               would on its own display, and its bye ends the session.
               With a program's window held it ends the hold, and the
               window goes. Idle, or asked again while the client is
               still here, it shuts the server down here and now. */
            if (clientup) {

                if (sigasked) {

                    status("shutting down, port %lld released, the client "
                           "on it abandoned", AMI_LONG_CAST(srvport));
                    srvexit();

                }
                sigasked = 1; /* the client was told; a second asks the server */
                status("the window was closed on the connection on port "
                       "%lld, ending its session", AMI_LONG_CAST(srvport));

            } else if (endhold(1)) {

                status("the held window was closed, the program's window "
                       "on port %lld is gone", AMI_LONG_CAST(srvport));
                continue;

            } else {

                status("shutting down, port %lld released",
                       AMI_LONG_CAST(srvport));
                srvexit();

            }

        }
        if (!clientup) continue; /* idle: nobody to forward to */
        inpumpevt = 1; /* a send that fails under here is an event's */
        (void)h;
        memset(we, 0, sizeof(gr_msgevt));
        we->winid = lw2h(er.winid);
        we->etype = (ami_long)er.etype;
        we->handled = 0;
        memcpy(we->p, &er.echar, EVUNION);
        {

            thinslot* ts = thinfor(we);

            if (ts) {

                double now = thnow();

                if (now-ts->last >= THINMS/1000.0) {

                    /* due: this one goes, and carries any pendings;
                       its own pending it supersedes outright */
                    ts->dirty = 0;
                    thinflush();
                    ts->last = now;
                    evsend1(we);

                } else {

                    /* within the interval: hold as the latest */
                    ts->ev = *we;
                    ts->dirty = 1;

                }

            } else {

                /* every other event flushes the pendings ahead of it */
                thinflush();
                evsend1(we);

            }

        }
        inpumpevt = 0;

    }

}

/*******************************************************************************

The command dispatch

Each message unpacks its payload in declaration order, into locals so the
order is the order, and executes against the display library.

*******************************************************************************/

static void dispatch(void)

{

    FILE* f = NULL;
    ami_long  a, b, c, d, e, g, i, j;
    ami_long  o1, o2, o3, o4;
    double fa, fb;
    char  s1[MAXSTR];
    char  s2[MAXSTR];

    if (gtrace)
        fprintf(stderr, "gs< %-16s w%-3lld s%-5d len%lld\n",
                gr_msgname(rhdr()->mid), AMI_LONG_CAST(rhdr()->wid), rhdr()->seq, AMI_LONG_CAST(rlen));
    if (rhdr()->wid) f = wf(rhdr()->wid);
    switch (rhdr()->mid) {

        /* ------------------------------------------------------ session */

        case GR_MHELLO:
            a = gi();
            /* the program's name follows, from a client that sends one */
            if (roff < rlen) gstr(sespgm, sizeof(sespgm));
            else sespgm[0] = 0;
            rbegin(); ri(GR_VERSION); rsend();
            if (a != GR_VERSION) sesserr("Client protocol version mismatch");
            if (clientup) {

                /* A hello mid-session: the old client went without its
                   bye, a kill or a dead network, and a new one is
                   knocking. The old session winds down and this hello,
                   already answered, opens the new one. */
                hellopend = 1;
                byeseen = 1;

            }
            break;
        case GR_MSYNC:
            /* the flow fence: the reply is the client's permission to
               send another window */
            rbegin(); ri(0); rsend();
            break;
        case GR_MBYE:
            /* wind down: the main loop stops the pump first, so nothing
               is inside the display library when the exit takes it down */
            byeseen = 1;
            break;

        /* -------------------------------------------------- byte stream */

        case GR_MWRITE:
            a = g4();
            if (roff+a > rlen) sesserr("Message truncated");
            fwrite(rbuf+roff, 1, a, f);
            fflush(f);
            break;
        case GR_MCLOSEWIN:
            fclose(f);
            h2f[rhdr()->wid] = NULL;
            /* its id goes back to the pool */
            a = h2lw[rhdr()->wid];
            if (a > SESWID && a <= MAXLW) lwused[a] = 0;
            h2lw[rhdr()->wid] = 0;
            break;

        /* ----------------------------------------------- terminal level */

        case GR_MCURSOR: a = gi(); b = gi(); ami_cursor(f, a, b); break;
        case GR_MMAXX: rbegin(); ri(ami_maxx(f)); rsend(); break;
        case GR_MMAXY: rbegin(); ri(ami_maxy(f)); rsend(); break;
        case GR_MHOME: ami_home(f); break;
        case GR_MDEL: ami_del(f); break;
        case GR_MUP: ami_up(f); break;
        case GR_MDOWN: ami_down(f); break;
        case GR_MLEFT: ami_left(f); break;
        case GR_MRIGHT: ami_right(f); break;
        case GR_MBLINK: a = gi(); ami_blink(f, a); break;
        case GR_MREVERSE: a = gi(); ami_reverse(f, a); break;
        case GR_MUNDERLINE: a = gi(); ami_underline(f, a); break;
        case GR_MSUPERSCRIPT: a = gi(); ami_superscript(f, a); break;
        case GR_MSUBSCRIPT: a = gi(); ami_subscript(f, a); break;
        case GR_MITALIC: a = gi(); ami_italic(f, a); break;
        case GR_MBOLD: a = gi(); ami_bold(f, a); break;
        case GR_MSTRIKEOUT: a = gi(); ami_strikeout(f, a); break;
        case GR_MSTANDOUT: a = gi(); ami_standout(f, a); break;
        case GR_MFCOLOR: a = gi(); ami_fcolor(f, (ami_color)a); break;
        case GR_MBCOLOR: a = gi(); ami_bcolor(f, (ami_color)a); break;
        case GR_MAUTO: a = gi(); ami_auto(f, a); break;
        case GR_MCURVIS: a = gi(); ami_curvis(f, a); break;
        case GR_MSCROLL: a = gi(); b = gi(); ami_scroll(f, a, b); break;
        case GR_MCURX: rbegin(); ri(ami_curx(f)); rsend(); break;
        case GR_MCURY: rbegin(); ri(ami_cury(f)); rsend(); break;
        case GR_MCURBND: rbegin(); ri(ami_curbnd(f)); rsend(); break;
        case GR_MSELECT: a = gi(); b = gi(); ami_select(f, a, b); break;
        case GR_MTIMER:
            a = gi(); b = gi(); c = gi(); ami_timer(f, a, b, c); break;
        case GR_MKILLTIMER: a = gi(); ami_killtimer(f, a); break;
        case GR_MMOUSE: rbegin(); ri(ami_mouse(f)); rsend(); break;
        case GR_MMOUSEBUTTON:
            a = gi(); rbegin(); ri(ami_mousebutton(f, a)); rsend(); break;
        case GR_MJOYSTICK: rbegin(); ri(ami_joystick(f)); rsend(); break;
        case GR_MJOYBUTTON:
            a = gi(); rbegin(); ri(ami_joybutton(f, a)); rsend(); break;
        case GR_MJOYAXIS:
            a = gi(); rbegin(); ri(ami_joyaxis(f, a)); rsend(); break;
        case GR_MSETTAB: a = gi(); ami_settab(f, a); break;
        case GR_MRESTAB: a = gi(); ami_restab(f, a); break;
        case GR_MCLRTAB: ami_clrtab(f); break;
        case GR_MFUNKEY: rbegin(); ri(ami_funkey(f)); rsend(); break;
        case GR_MFRAMETIMER: a = gi(); ami_frametimer(f, a); break;
        case GR_MAUTOHOLD:
            /* the session's: the hold acts when the program ends, on
               its window here, never on the server's own exit */
            a = gi();
            sesautohold = a != 0;
            break;
        case GR_MWRTSTR: gstr(s1, MAXSTR); ami_wrtstr(f, s1); break;
        case GR_MWRTSTRN:
            a = g4();
            if (roff+a > rlen) sesserr("Message truncated");
            if (a > MAXSTR) a = MAXSTR;
            memcpy(s1, rbuf+roff, a);
            ami_wrtstrn(f, s1, a);
            break;
        case GR_MSIZBUF: a = gi(); b = gi(); ami_sizbuf(f, a, b); break;
        case GR_MTITLE:
            gstr(s1, MAXSTR);
            if (f == h2f[1]) /* the program's window: its title names the hold */
                snprintf(sestitle, sizeof(sestitle), "%s", s1);
            ami_title(f, s1);
            break;
        case GR_MFCOLORC:
            a = gi(); b = gi(); c = gi(); ami_fcolorc(f, a, b, c); break;
        case GR_MBCOLORC:
            a = gi(); b = gi(); c = gi(); ami_bcolorc(f, a, b, c); break;

        /* ---------------------------------------------- graphical level */

        case GR_MMAXXG: rbegin(); ri(ami_maxxg(f)); rsend(); break;
        case GR_MMAXYG: rbegin(); ri(ami_maxyg(f)); rsend(); break;
        case GR_MCURXG: rbegin(); ri(ami_curxg(f)); rsend(); break;
        case GR_MCURYG: rbegin(); ri(ami_curyg(f)); rsend(); break;
        case GR_MLINE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_line(f, a, b, c, d); break;
        case GR_MLINEWIDTH: a = gi(); ami_linewidth(f, a); break;
        case GR_MLINESTYLE: a = gi(); ami_linestyle(f, (ami_lstyle)a); break;
        case GR_MRECT:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_rect(f, a, b, c, d); break;
        case GR_MFRECT:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_frect(f, a, b, c, d); break;
        case GR_MRRECT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_rrect(f, a, b, c, d, e, g); break;
        case GR_MFRRECT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_frrect(f, a, b, c, d, e, g); break;
        case GR_MELLIPSE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_ellipse(f, a, b, c, d); break;
        case GR_MFELLIPSE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_fellipse(f, a, b, c, d); break;
        case GR_MARC:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_arc(f, a, b, c, d, e, g); break;
        case GR_MFARC:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_farc(f, a, b, c, d, e, g); break;
        case GR_MFCHORD:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_fchord(f, a, b, c, d, e, g); break;
        case GR_MFTRIANGLE:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_ftriangle(f, a, b, c, d, e, g); break;
        case GR_MCURSORG: a = gi(); b = gi(); ami_cursorg(f, a, b); break;
        case GR_MBASELINE: rbegin(); ri(ami_baseline(f)); rsend(); break;
        case GR_MSETPIXEL: a = gi(); b = gi(); ami_setpixel(f, a, b); break;
        case GR_MFOVER: ami_fover(f); break;
        case GR_MBOVER: ami_bover(f); break;
        case GR_MFINVIS: ami_finvis(f); break;
        case GR_MBINVIS: ami_binvis(f); break;
        case GR_MFXOR: ami_fxor(f); break;
        case GR_MBXOR: ami_bxor(f); break;
        case GR_MFAND: ami_fand(f); break;
        case GR_MBAND: ami_band(f); break;
        case GR_MFOR: ami_for(f); break;
        case GR_MBOR: ami_bor(f); break;
        case GR_MCHRSIZX: rbegin(); ri(ami_chrsizx(f)); rsend(); break;
        case GR_MCHRSIZY: rbegin(); ri(ami_chrsizy(f)); rsend(); break;
        case GR_MFONTS: rbegin(); ri(ami_fonts(f)); rsend(); break;
        case GR_MFONT: a = gi(); ami_font(f, a); break;
        case GR_MFONTNAM:
            a = gi();
            ami_fontnam(f, a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend();
            break;
        case GR_MFONTSIZ: a = gi(); ami_fontsiz(f, a); break;
        case GR_MSETPOINTS: fa = gf(); ami_setpoints(f, (float)fa); break;
        case GR_MPOINTS: rbegin(); rf(ami_points(f)); rsend(); break;
        case GR_MCHRSPCY: a = gi(); ami_chrspcy(f, a); break;
        case GR_MCHRSPCX: a = gi(); ami_chrspcx(f, a); break;
        case GR_MDPMX: rbegin(); ri(ami_dpmx(f)); rsend(); break;
        case GR_MDPMY: rbegin(); ri(ami_dpmy(f)); rsend(); break;
        case GR_MSTRSIZ:
            gstr(s1, MAXSTR);
            rbegin(); ri(ami_strsiz(f, s1)); rsend();
            break;
        case GR_MCHRPOS:
            gstr(s1, MAXSTR); a = gi();
            rbegin(); ri(ami_chrpos(f, s1, a)); rsend();
            break;
        case GR_MWRITEJUST:
            gstr(s1, MAXSTR); a = gi(); ami_writejust(f, s1, a); break;
        case GR_MJUSTPOS:
            gstr(s1, MAXSTR); a = gi(); b = gi();
            rbegin(); ri(ami_justpos(f, s1, a, b)); rsend();
            break;
        case GR_MCONDENSED: a = gi(); ami_condensed(f, a); break;
        case GR_MEXTENDED: a = gi(); ami_extended(f, a); break;
        case GR_MXLIGHT: a = gi(); ami_xlight(f, a); break;
        case GR_MLIGHT: a = gi(); ami_light(f, a); break;
        case GR_MXBOLD: a = gi(); ami_xbold(f, a); break;
        case GR_MHOLLOW: a = gi(); ami_hollow(f, a); break;
        case GR_MRAISED: a = gi(); ami_raised(f, a); break;
        case GR_MSETTABG: a = gi(); ami_settabg(f, a); break;
        case GR_MRESTABG: a = gi(); ami_restabg(f, a); break;
        case GR_MFCOLORG:
            a = gi(); b = gi(); c = gi(); ami_fcolorg(f, a, b, c); break;
        case GR_MBCOLORG:
            a = gi(); b = gi(); c = gi(); ami_bcolorg(f, a, b, c); break;
        case GR_MLOADPICT:
            a = gi(); gstr(s1, MAXSTR);
            ami_loadpict(f, a, (char*)fndfile(s1, s2, MAXSTR, ".bmp"));
            rbegin(); ri(0); rsend();
            break;
        case GR_MPICTSIZX:
            a = gi(); rbegin(); ri(ami_pictsizx(f, a)); rsend(); break;
        case GR_MPICTSIZY:
            a = gi(); rbegin(); ri(ami_pictsizy(f, a)); rsend(); break;
        case GR_MPICTURE:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_picture(f, a, b, c, d, e); break;
        case GR_MDELPICT: a = gi(); ami_delpict(f, a); break;
        case GR_MSCROLLG: a = gi(); b = gi(); ami_scrollg(f, a, b); break;
        case GR_MPATH: a = gi(); ami_path(f, a); break;
        case GR_MVIEWOFFG: a = gi(); b = gi(); ami_viewoffg(f, a, b); break;
        case GR_MVIEWSCALE:
            fa = gf(); fb = gf();
            ami_viewscale(f, (float)fa, (float)fb); break;
        case GR_MSCALEX:
            a = gi(); rbegin(); ri(ami_scalex(f, a)); rsend(); break;
        case GR_MSCALEY:
            a = gi(); rbegin(); ri(ami_scaley(f, a)); rsend(); break;
        case GR_MBLOCKCOPYG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            i = gi(); j = gi(); o1 = gi(); o2 = gi();
            ami_blockcopyg(f, a, b, c, d, e, g, i, j, o1, o2); break;

        /* --------------------------------------------- window management */

        case GR_MOPENWIN: {

            FILE* inf = stdin;
            FILE* outf = NULL;
            ami_long par, h;

            par = gi(); h = gi(); a = gi();
            if (h < 1 || h >= MAXHND || h2f[h])
                sesserr("Invalid window handle");
            /* The logical id is the server's to choose: the client maps
               events by handle, never by id, so the program's own (a) is
               not needed, and could not be used, the session window
               holding one of the program's possible ids. The library
               takes ids up to MAXLW only, and the program's handles climb
               past that as it opens and closes windows, so the ids come
               from a pool and go back at close. */
            for (a = SESWID+1; a <= MAXLW && lwused[a]; a++);
            if (a > MAXLW) sesserr("Too many windows open");
            lwused[a] = 1;
            ami_openwin(&inf, &outf, par? wf(par): NULL, a);
            /* the library titles a new window with this program's name,
               the server's: the window is the client program's, and takes
               its name, as the session window did */
            if (sespgm[0]) ami_title(outf, sespgm);
            h2f[h] = outf;
            h2lw[h] = a;
            break;

        }
        case GR_MBUFFER: a = gi(); ami_buffer(f, a); break;
        case GR_MSIZBUFG: a = gi(); b = gi(); ami_sizbufg(f, a, b); break;
        case GR_MGETSIZ:
            ami_getsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MGETSIZG:
            ami_getsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSETSIZ: a = gi(); b = gi(); ami_setsiz(f, a, b); break;
        case GR_MSETSIZG: a = gi(); b = gi(); ami_setsizg(f, a, b); break;
        case GR_MSETPOS: a = gi(); b = gi(); ami_setpos(f, a, b); break;
        case GR_MSETPOSG: a = gi(); b = gi(); ami_setposg(f, a, b); break;
        case GR_MDRAGWIN: ami_dragwin(f); break;
        case GR_MSCNSIZ:
            ami_scnsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCNSIZG:
            ami_scnsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCNCEN:
            ami_scncen(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCNCENG:
            ami_scnceng(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MWINCLIENT:
            a = gi(); b = gi(); c = gi();
            ami_winclient(f, a, b, &o1, &o2, (ami_winmodset)c);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MWINCLIENTG:
            a = gi(); b = gi(); c = gi();
            ami_winclientg(f, a, b, &o1, &o2, (ami_winmodset)c);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MFRONT: ami_front(f); break;
        case GR_MBACK: ami_back(f); break;
        case GR_MFRAME: a = gi(); ami_frame(f, a); break;
        case GR_MSIZABLE: a = gi(); ami_sizable(f, a); break;
        case GR_MSYSBAR: a = gi(); ami_sysbar(f, a); break;
        case GR_MMENU: ami_menu(f, gmenu()); break;
        case GR_MMENUENA: a = gi(); b = gi(); ami_menuena(f, a, b); break;
        case GR_MMENUSEL: a = gi(); b = gi(); ami_menusel(f, a, b); break;
        case GR_MSTDMENU: {

            ami_menuptr pm, sm = NULL;

            a = gi();
            pm = gmenu();
            ami_stdmenu((ami_stdmenusel)a, &sm, pm);
            rbegin(); rmenu(sm); rsend();
            break;

        }
        case GR_MGETWINID: rbegin(); ri(ami_getwinid()); rsend(); break;
        case GR_MFOCUS: ami_focus(f); break;

        /* --------------------------------------------------- widgets */

        case GR_MGETWIGID: rbegin(); ri(ami_getwigid(f)); rsend(); break;
        case GR_MKILLWIDGET: a = gi(); ami_killwidget(f, a); break;
        case GR_MSELECTWIDGET:
            a = gi(); b = gi(); ami_selectwidget(f, a, b); break;
        case GR_MENABLEWIDGET:
            a = gi(); b = gi(); ami_enablewidget(f, a, b); break;
        case GR_MGETWIDGETTEXT:
            a = gi();
            ami_getwidgettext(f, a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend();
            break;
        case GR_MPUTWIDGETTEXT:
            a = gi(); gstr(s1, MAXSTR); ami_putwidgettext(f, a, s1); break;
        case GR_MSIZWIDGET:
            a = gi(); b = gi(); c = gi(); ami_sizwidget(f, a, b, c); break;
        case GR_MSIZWIDGETG:
            a = gi(); b = gi(); c = gi(); ami_sizwidgetg(f, a, b, c); break;
        case GR_MPOSWIDGET:
            a = gi(); b = gi(); c = gi(); ami_poswidget(f, a, b, c); break;
        case GR_MPOSWIDGETG:
            a = gi(); b = gi(); c = gi(); ami_poswidgetg(f, a, b, c); break;
        case GR_MBACKWIDGET: a = gi(); ami_backwidget(f, a); break;
        case GR_MFRONTWIDGET: a = gi(); ami_frontwidget(f, a); break;
        case GR_MFOCUSWIDGET: a = gi(); ami_focuswidget(f, a); break;
        case GR_MBUTTONSIZ:
            gstr(s1, MAXSTR);
            ami_buttonsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MBUTTONSIZG:
            gstr(s1, MAXSTR);
            ami_buttonsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MBUTTON:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_button(f, a, b, c, d, s1, e); break;
        case GR_MBUTTONG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_buttong(f, a, b, c, d, s1, e); break;
        case GR_MCHECKBOXSIZ:
            gstr(s1, MAXSTR);
            ami_checkboxsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MCHECKBOXSIZG:
            gstr(s1, MAXSTR);
            ami_checkboxsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MCHECKBOX:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_checkbox(f, a, b, c, d, s1, e); break;
        case GR_MCHECKBOXG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_checkboxg(f, a, b, c, d, s1, e); break;
        case GR_MRADIOBUTTONSIZ:
            gstr(s1, MAXSTR);
            ami_radiobuttonsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MRADIOBUTTONSIZG:
            gstr(s1, MAXSTR);
            ami_radiobuttonsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MRADIOBUTTON:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_radiobutton(f, a, b, c, d, s1, e); break;
        case GR_MRADIOBUTTONG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_radiobuttong(f, a, b, c, d, s1, e); break;
        case GR_MGROUPSIZG:
            gstr(s1, MAXSTR); a = gi(); b = gi();
            ami_groupsizg(f, s1, a, b, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MGROUPSIZ:
            gstr(s1, MAXSTR); a = gi(); b = gi();
            ami_groupsiz(f, s1, a, b, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MGROUP:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_group(f, a, b, c, d, s1, e); break;
        case GR_MGROUPG:
            a = gi(); b = gi(); c = gi(); d = gi(); gstr(s1, MAXSTR);
            e = gi(); ami_groupg(f, a, b, c, d, s1, e); break;
        case GR_MBACKGROUND:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_background(f, a, b, c, d, e); break;
        case GR_MBACKGROUNDG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_backgroundg(f, a, b, c, d, e); break;
        case GR_MSCROLLVERTSIZG:
            ami_scrollvertsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLVERTSIZ:
            ami_scrollvertsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLVERT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollvert(f, a, b, c, d, e); break;
        case GR_MSCROLLVERTG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollvertg(f, a, b, c, d, e); break;
        case GR_MSCROLLHORIZSIZG:
            ami_scrollhorizsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLHORIZSIZ:
            ami_scrollhorizsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSCROLLHORIZ:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollhoriz(f, a, b, c, d, e); break;
        case GR_MSCROLLHORIZG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_scrollhorizg(f, a, b, c, d, e); break;
        case GR_MSCROLLPOS: a = gi(); b = gi(); ami_scrollpos(f, a, b); break;
        case GR_MSCROLLSIZ: a = gi(); b = gi(); ami_scrollsiz(f, a, b); break;
        case GR_MNUMSELBOXSIZG:
            a = gi(); b = gi();
            ami_numselboxsizg(f, a, b, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MNUMSELBOXSIZ:
            a = gi(); b = gi();
            ami_numselboxsiz(f, a, b, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MNUMSELBOX:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            i = gi(); ami_numselbox(f, a, b, c, d, e, g, i); break;
        case GR_MNUMSELBOXG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            i = gi(); ami_numselboxg(f, a, b, c, d, e, g, i); break;
        case GR_MEDITBOXSIZG:
            gstr(s1, MAXSTR);
            ami_editboxsizg(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MEDITBOXSIZ:
            gstr(s1, MAXSTR);
            ami_editboxsiz(f, s1, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MEDITBOX:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_editbox(f, a, b, c, d, e); break;
        case GR_MEDITBOXG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_editboxg(f, a, b, c, d, e); break;
        case GR_MPROGBARSIZG:
            ami_progbarsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MPROGBARSIZ:
            ami_progbarsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MPROGBAR:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_progbar(f, a, b, c, d, e); break;
        case GR_MPROGBARG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_progbarg(f, a, b, c, d, e); break;
        case GR_MPROGBARPOS: a = gi(); b = gi(); ami_progbarpos(f, a, b); break;
        case GR_MLISTBOXSIZG:
            ami_listboxsizg(f, gslst(), &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MLISTBOXSIZ:
            ami_listboxsiz(f, gslst(), &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MLISTBOX: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_listbox(f, a, b, c, d, sp, e); break;

        }
        case GR_MLISTBOXG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_listboxg(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPBOXSIZG:
            ami_dropboxsizg(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPBOXSIZ:
            ami_dropboxsiz(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPBOX: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropbox(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPBOXG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropboxg(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPEDITBOXSIZG:
            ami_dropeditboxsizg(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPEDITBOXSIZ:
            ami_dropeditboxsiz(f, gslst(), &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MDROPEDITBOX: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropeditbox(f, a, b, c, d, sp, e); break;

        }
        case GR_MDROPEDITBOXG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            ami_dropeditboxg(f, a, b, c, d, sp, e); break;

        }
        case GR_MSLIDEHORIZSIZG:
            ami_slidehorizsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEHORIZSIZ:
            ami_slidehorizsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEHORIZ:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidehoriz(f, a, b, c, d, e, g); break;
        case GR_MSLIDEHORIZG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidehorizg(f, a, b, c, d, e, g); break;
        case GR_MSLIDEVERTSIZG:
            ami_slidevertsizg(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEVERTSIZ:
            ami_slidevertsiz(f, &o1, &o2);
            rbegin(); ri(o1); ri(o2); rsend(); break;
        case GR_MSLIDEVERT:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidevert(f, a, b, c, d, e, g); break;
        case GR_MSLIDEVERTG:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi(); g = gi();
            ami_slidevertg(f, a, b, c, d, e, g); break;
        case GR_MTABBARSIZG: {
            ami_strptr sp = gslst();
            a = gi(); b = gi(); c = gi();
            ami_tabbarsizg(f, sp, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;

        }
        case GR_MTABBARSIZ: {
            ami_strptr sp = gslst();
            a = gi(); b = gi(); c = gi();
            ami_tabbarsiz(f, sp, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;

        }
        case GR_MTABBARCLIENTG:
            a = gi(); b = gi(); c = gi();
            ami_tabbarclientg(f, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MTABBARCLIENT:
            a = gi(); b = gi(); c = gi();
            ami_tabbarclient(f, (ami_tabori)a, b, c, &o1, &o2, &o3, &o4);
            rbegin(); ri(o1); ri(o2); ri(o3); ri(o4); rsend(); break;
        case GR_MTABBAR: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            g = gi(); ami_tabbar(f, a, b, c, d, sp, (ami_tabori)e, g); break;

        }
        case GR_MTABBARG: {

            ami_strptr sp;
            a = gi(); b = gi(); c = gi(); d = gi(); sp = gslst(); e = gi();
            g = gi(); ami_tabbarg(f, a, b, c, d, sp, (ami_tabori)e, g); break;

        }
        case GR_MTABSEL: a = gi(); b = gi(); ami_tabsel(f, a, b); break;

        /* ----------------------------------------------------- sound */

        case GR_MSTARTTIMEOUT: ami_starttimeout(); sesto = 1; break;
        case GR_MSTOPTIMEOUT: ami_stoptimeout(); sesto = 0; break;
        case GR_MCURTIMEOUT:
            rbegin(); ri(ami_curtimeout()); rsend(); break;
        case GR_MSTARTTIMEIN: ami_starttimein(); sesti = 1; break;
        case GR_MSTOPTIMEIN: ami_stoptimein(); sesti = 0; break;
        case GR_MCURTIMEIN:
            rbegin(); ri(ami_curtimein()); rsend(); break;
        case GR_MSYNTHOUT: rbegin(); ri(ami_synthout()); rsend(); break;
        case GR_MSYNTHIN: rbegin(); ri(ami_synthin()); rsend(); break;
        case GR_MOPENSYNTHOUT: a = gi(); ami_opensynthout(a);
            if (a >= 1 && a <= SNDPORTS) sesso[a-1] = 1; break;
        case GR_MCLOSESYNTHOUT: a = gi(); ami_closesynthout(a);
            if (a >= 1 && a <= SNDPORTS) sesso[a-1] = 0; break;
        case GR_MOPENSYNTHIN: a = gi(); ami_opensynthin(a);
            if (a >= 1 && a <= SNDPORTS) sessi[a-1] = 1; break;
        case GR_MCLOSESYNTHIN: a = gi(); ami_closesynthin(a);
            if (a >= 1 && a <= SNDPORTS) sessi[a-1] = 0; break;
        case GR_MNOTEON:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_noteon(a, b, c, d, e); break;
        case GR_MNOTEOFF:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_noteoff(a, b, c, d, e); break;
        case GR_MINSTCHANGE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_instchange(a, b, c, d); break;
        case GR_MATTACK:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_attack(a, b, c, d); break;
        case GR_MRELEASE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_release(a, b, c, d); break;
        case GR_MLEGATO:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_legato(a, b, c, d); break;
        case GR_MPORTAMENTO:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_portamento(a, b, c, d); break;
        case GR_MVIBRATO:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_vibrato(a, b, c, d); break;
        case GR_MVOLSYNTHCHAN:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_volsynthchan(a, b, c, d); break;
        case GR_MPORTTIME:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_porttime(a, b, c, d); break;
        case GR_MBALANCE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_balance(a, b, c, d); break;
        case GR_MPAN:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_pan(a, b, c, d); break;
        case GR_MTIMBRE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_timbre(a, b, c, d); break;
        case GR_MBRIGHTNESS:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_brightness(a, b, c, d); break;
        case GR_MREVERB:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_reverb(a, b, c, d); break;
        case GR_MTREMULO:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_tremulo(a, b, c, d); break;
        case GR_MCHORUS:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_chorus(a, b, c, d); break;
        case GR_MCELESTE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_celeste(a, b, c, d); break;
        case GR_MPHASER:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_phaser(a, b, c, d); break;
        case GR_MAFTERTOUCH:
            a = gi(); b = gi(); c = gi(); d = gi(); e = gi();
            ami_aftertouch(a, b, c, d, e); break;
        case GR_MPRESSURE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_pressure(a, b, c, d); break;
        case GR_MPITCH:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_pitch(a, b, c, d); break;
        case GR_MPITCHRANGE:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_pitchrange(a, b, c, d); break;
        case GR_MMONO:
            a = gi(); b = gi(); c = gi(); d = gi();
            ami_mono(a, b, c, d); break;
        case GR_MPOLY:
            a = gi(); b = gi(); c = gi(); ami_poly(a, b, c); break;
        case GR_MLOADSYNTH:
            a = gi(); gstr(s1, MAXSTR);
            ami_loadsynth(a, (char*)fndfile(s1, s2, MAXSTR, ".mid"));
            if (a >= 1 && a <= SNDPORTS) sessy[a-1] = 1;
            rbegin(); ri(0); rsend();
            break;
        case GR_MPLAYSYNTH:
            a = gi(); b = gi(); c = gi(); ami_playsynth(a, b, c); break;
        case GR_MDELSYNTH: a = gi(); ami_delsynth(a);
            if (a >= 1 && a <= SNDPORTS) sessy[a-1] = 0; break;
        case GR_MWAITSYNTH:
            a = gi(); ami_waitsynth(a);
            rbegin(); ri(0); rsend(); break;
        case GR_MWRSYNTH: {

            ami_seqmsg sm;
            ami_long   u[3];

            a = gi();
            memset(&sm, 0, sizeof(sm));
            sm.port = gi();
            sm.time = gi();
            sm.st = (ami_seqtyp)gi();
            u[0] = gi(); u[1] = gi(); u[2] = gi();
            memcpy(&sm.ntc, u, sizeof(u));
            ami_wrsynth(a, &sm);
            break;

        }
        case GR_MRDSYNTH: {

            ami_seqmsg sm;
            ami_long   u[3];

            a = gi();
            memset(&sm, 0, sizeof(sm));
            ami_rdsynth(a, &sm); /* blocks until the device delivers */
            memcpy(u, &sm.ntc, sizeof(u));
            rbegin();
            ri(sm.port); ri(sm.time); ri((ami_long)sm.st);
            ri(u[0]); ri(u[1]); ri(u[2]);
            rsend();
            break;

        }
        case GR_MWAVEOUT: rbegin(); ri(ami_waveout()); rsend(); break;
        case GR_MWAVEIN: rbegin(); ri(ami_wavein()); rsend(); break;
        case GR_MOPENWAVEOUT: a = gi(); ami_openwaveout(a);
            if (a >= 1 && a <= SNDPORTS) seswo[a-1] = 1; break;
        case GR_MCLOSEWAVEOUT: a = gi(); ami_closewaveout(a);
            if (a >= 1 && a <= SNDPORTS) seswo[a-1] = 0; break;
        case GR_MLOADWAVE:
            a = gi(); gstr(s1, MAXSTR);
            ami_loadwave(a, (char*)fndfile(s1, s2, MAXSTR, ".wav"));
            if (a >= 1 && a <= SNDPORTS) seswv[a-1] = 1;
            rbegin(); ri(0); rsend();
            break;
        case GR_MPLAYWAVE:
            a = gi(); b = gi(); c = gi(); ami_playwave(a, b, c); break;
        case GR_MDELWAVE: a = gi(); ami_delwave(a);
            if (a >= 1 && a <= SNDPORTS) seswv[a-1] = 0; break;
        case GR_MVOLWAVE:
            a = gi(); b = gi(); c = gi(); ami_volwave(a, b, c); break;
        case GR_MWAITWAVE:
            a = gi(); ami_waitwave(a);
            rbegin(); ri(0); rsend(); break;
        case GR_MCHANWAVEOUT: a = gi(); b = gi(); ami_chanwaveout(a, b); break;
        case GR_MRATEWAVEOUT: a = gi(); b = gi(); ami_ratewaveout(a, b); break;
        case GR_MLENWAVEOUT: a = gi(); b = gi(); ami_lenwaveout(a, b); break;
        case GR_MSGNWAVEOUT: a = gi(); b = gi(); ami_sgnwaveout(a, b); break;
        case GR_MFLTWAVEOUT: a = gi(); b = gi(); ami_fltwaveout(a, b); break;
        case GR_MENDWAVEOUT: a = gi(); b = gi(); ami_endwaveout(a, b); break;
        case GR_MWRWAVE:
            /* i64 port, i64 frames, then the samples as a block; the
               ack paces the stream to the playback rate */
            a = gi(); b = gi(); c = g4();
            if (roff+c > rlen) sesserr("Message truncated");
            ami_wrwave(a, (byte*)(rbuf+roff), b);
            rbegin(); ri(0); rsend();
            break;
        case GR_MOPENWAVEIN: a = gi(); ami_openwavein(a);
            if (a >= 1 && a <= SNDPORTS) seswi[a-1] = 1; break;
        case GR_MCLOSEWAVEIN: a = gi(); ami_closewavein(a);
            if (a >= 1 && a <= SNDPORTS) seswi[a-1] = 0; break;
        case GR_MCHANWAVEIN:
            a = gi(); rbegin(); ri(ami_chanwavein(a)); rsend(); break;
        case GR_MRATEWAVEIN:
            a = gi(); rbegin(); ri(ami_ratewavein(a)); rsend(); break;
        case GR_MLENWAVEIN:
            a = gi(); rbegin(); ri(ami_lenwavein(a)); rsend(); break;
        case GR_MSGNWAVEIN:
            a = gi(); rbegin(); ri(ami_sgnwavein(a)); rsend(); break;
        case GR_MENDWAVEIN:
            a = gi(); rbegin(); ri(ami_endwavein(a)); rsend(); break;
        case GR_MFLTWAVEIN:
            a = gi(); rbegin(); ri(ami_fltwavein(a)); rsend(); break;
        case GR_MRDWAVE: {

            static byte* wvbuf;
            static ami_long  wvblen;
            ami_long     got, fsiz, fit;

            /* Lengths are in samples, one frame of every channel. The
               answer clamps to what one message carries, at the frame
               size this device delivers, and the client loops. */
            a = gi(); b = gi();
            fsiz = ami_chanwavein(a)*((ami_lenwavein(a)+7)/8);
            if (fsiz < 1) fsiz = 1;
            fit = (msgmax-96)/fsiz;
            if (b > fit) b = fit;
            if (b*fsiz > wvblen) {

                free(wvbuf);
                wvbuf = malloc(b*fsiz);
                if (!wvbuf) sesserr("Out of memory");
                wvblen = b*fsiz;

            }
            got = ami_rdwave(a, wvbuf, b); /* blocks until data */
            rbegin();
            ri(got);
            rstrn((char*)wvbuf, got > 0? got*fsiz: 0);
            rsend();
            break;

        }
        case GR_MSYNTHOUTNAME:
            a = gi(); ami_synthoutname(a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MSYNTHINNAME:
            a = gi(); ami_synthinname(a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MWAVEOUTNAME:
            a = gi(); ami_waveoutname(a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MWAVEINNAME:
            a = gi(); ami_waveinname(a, s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MSETPARAMSYNTHIN:
            a = gi(); gstr(s1, MAXSTR); gstr(s2, MAXSTR);
            rbegin(); ri(ami_setparamsynthin(a, s1, s2)); rsend(); break;
        case GR_MSETPARAMSYNTHOUT:
            a = gi(); gstr(s1, MAXSTR); gstr(s2, MAXSTR);
            rbegin(); ri(ami_setparamsynthout(a, s1, s2)); rsend(); break;
        case GR_MSETPARAMWAVEIN:
            a = gi(); gstr(s1, MAXSTR); gstr(s2, MAXSTR);
            rbegin(); ri(ami_setparamwavein(a, s1, s2)); rsend(); break;
        case GR_MSETPARAMWAVEOUT:
            a = gi(); gstr(s1, MAXSTR); gstr(s2, MAXSTR);
            rbegin(); ri(ami_setparamwaveout(a, s1, s2)); rsend(); break;
        case GR_MGETPARAMSYNTHIN:
            a = gi(); gstr(s1, MAXSTR);
            ami_getparamsynthin(a, s1, s2, MAXSTR);
            rbegin(); rstr(s2, MAXSTR); rsend(); break;
        case GR_MGETPARAMSYNTHOUT:
            a = gi(); gstr(s1, MAXSTR);
            ami_getparamsynthout(a, s1, s2, MAXSTR);
            rbegin(); rstr(s2, MAXSTR); rsend(); break;
        case GR_MGETPARAMWAVEIN:
            a = gi(); gstr(s1, MAXSTR);
            ami_getparamwavein(a, s1, s2, MAXSTR);
            rbegin(); rstr(s2, MAXSTR); rsend(); break;
        case GR_MGETPARAMWAVEOUT:
            a = gi(); gstr(s1, MAXSTR);
            ami_getparamwaveout(a, s1, s2, MAXSTR);
            rbegin(); rstr(s2, MAXSTR); rsend(); break;

        /* --------------------------------------------------- dialogs */

        case GR_MALERT:
            gstr(s1, MAXSTR); gstr(s2, MAXSTR);
            ami_alert(s1, s2);
            rbegin(); rsend(); /* the reply is the dismissal */
            break;
        case GR_MQUERYCOLOR:
            o1 = gi(); o2 = gi(); o3 = gi();
            ami_querycolor(&o1, &o2, &o3);
            rbegin(); ri(o1); ri(o2); ri(o3); rsend(); break;
        case GR_MQUERYOPEN:
            gstr(s1, MAXSTR);
            ami_queryopen(s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MQUERYSAVE:
            gstr(s1, MAXSTR);
            ami_querysave(s1, MAXSTR);
            rbegin(); rstr(s1, MAXSTR); rsend(); break;
        case GR_MQUERYFIND: {

            ami_qfnopts opt;
            gstr(s1, MAXSTR); opt = (ami_qfnopts)gi();
            ami_queryfind(s1, MAXSTR, &opt);
            rbegin(); rstr(s1, MAXSTR); ri((ami_long)opt); rsend(); break;

        }
        case GR_MQUERYFINDREP: {

            ami_qfropts opt;
            gstr(s1, MAXSTR); gstr(s2, MAXSTR); opt = (ami_qfropts)gi();
            ami_queryfindrep(s1, MAXSTR, s2, MAXSTR, &opt);
            rbegin(); rstr(s1, MAXSTR); rstr(s2, MAXSTR); ri((ami_long)opt);
            rsend(); break;

        }
        case GR_MQUERYFONT: {

            ami_long fc, s, fr, fg, fb, br, bg, bb;
            ami_qfteffects eff;

            fc = gi(); s = gi(); fr = gi(); fg = gi(); fb = gi();
            br = gi(); bg = gi(); bb = gi(); eff = (ami_qfteffects)gi();
            ami_queryfont(f, &fc, &s, &fr, &fg, &fb, &br, &bg, &bb, &eff);
            rbegin(); ri(fc); ri(s); ri(fr); ri(fg); ri(fb);
            ri(br); ri(bg); ri(bb); ri((ami_long)eff); rsend(); break;

        }

        default: sesserr("Unknown message");

    }

}

/* Execute every message in the datagram at rbase, dlen bytes; a
   datagram carries one or more messages back to back, the header
   lengths delimiting them. rbuf walks the batch; a session error inside
   longjmps out, and the fault path restores rbuf to rbase. */
static void dgram(ami_long dlen)

{

    ami_long off = 0;
    ami_long ml;

    while (off < dlen) {

        if (dlen-off < (ami_long)sizeof(gr_msghdr)) {

            rbuf = rbase;
            sesserr("Short message from client");

        }
        rbuf = rbase+off;
        ml = rhdr()->len;
        if (ml < (ami_long)sizeof(gr_msghdr) || off+ml > dlen) {

            rbuf = rbase;
            sesserr("Message framing error");

        }
        rlen = ml;
        roff = sizeof(gr_msghdr);
        dispatch();
        off += ml;

    }
    rbuf = rbase;

}

/* A crash in the field must tell its story: the fatal signals print
   the backtrace of the thread that died, resolvable with addr2line,
   before going down. gdb slows the process enough to hide the races
   this exists to catch. */
static void crashbt(int sig)

{

#ifndef _WIN32
    void* frames[32];
    int   n;

    fprintf(stderr, "*** graph_server: fatal signal %d, backtrace:\n", sig);
    n = backtrace(frames, 32);
    backtrace_symbols_fd(frames, n, 2);
#else
    /* Windows has no backtrace call: the signal number is the story */
    fprintf(stderr, "*** graph_server: fatal signal %d\n", sig);
#endif
    _exit(128+sig);

}

/*******************************************************************************

Main: bind, greet, pump, serve

Both channels bind before the hello is answered, so nothing the client
sends afterward can land on an unbound port. The event pump starts once
the client's event channel hello has arrived and named the peer.

*******************************************************************************/

int main(int argc, char* argv[])

{

    signal(SIGSEGV, crashbt);
#ifdef SIGBUS
    signal(SIGBUS, crashbt);
#endif
    signal(SIGFPE, crashbt);

    const char* sp;
    ami_ulong la;

    {

        int i;

        for (i = 1; i < argc; i++) {

            if (!strcmp(argv[i], "--trace")) gtrace = 1;
            /* -s is what secure used to want and is kept, meaning
               nothing now that it is the default */
            if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--secure"))
                gsecure = 1;
            if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--plain"))
                gsecure = 0;

        }

    }
    if (getenv("GRAPH_TRACE")) gtrace = 1;
    /* the locks and signals the handoff runs on, before any thread */
    joblock = ami_initlock();
    evsend = ami_initlock();
    jobdone = ami_initsig();
    /* The server never holds its final screen: it is infrastructure,
       and its exit must release the display and the ports at once. The
       hold belongs to programs, and a session's program never exits
       this process. */
    ami_autohold(0);
    ami_neterror(neterr); /* channel failures recycle, not abort */
    sp = getenv("GRAPH_PORT");
    srvport = sp && sp[0]? atol(sp): GR_DEFPORT;
    ami_addrnet("127.0.0.1", &la);
    msgmax = ami_maxmsg(la, gsecure);
    if (msgmax < 1024) error("Message channel too small");
    sbuf = malloc(msgmax);
    rbuf = rbase = malloc(msgmax);
    if (!sbuf || !rbuf) error("Out of memory");

    /* both channels up before anyone is answered. The channels arrive
       with short receive timeouts, meant for transient exchanges where a
       lost datagram should fail the read; these channels are persistent,
       a server without a client yet and a quiet command channel both
       being normal, so the timeouts clear. */
    /* The pump owns the display, and it is up before anything waits: the
       wait for a client is a long one, and through it the pump is what
       repaints the window and hears a terminate -- the close button, or
       control-c in the shell, which the library gives as an event. */
    pump = ami_newthread(evpump);
    if (pump < 1) error("Cannot start event pump");
    /* the secure wait is the client's handshake, and returns only when
       one has come: the waiting is announced before it */
    if (gsecure) waiting();
    cmdfn = ami_waitmsg(srvport, gsecure);
    evtfn = ami_waitmsg(srvport+1, gsecure);
    /* deep receive buffers: a full flow window must fit with room,
       and events should never drop for a slow moment */
    ami_bufmsg(cmdfn, 2*1024*1024);
    ami_bufmsg(evtfn, 2*1024*1024);
    /* the channels are kept: no bound on their reads */
    ami_tmomsg(cmdfn, 0);
    ami_tmomsg(evtfn, 0);

    /* nothing is ever written to the library's main window, so it never
       shows: the shell has the server's status, and the display shows
       the programs' windows only */

    /* the session loop: serve a client, reset, wait for the next; a
       session error jumps back here, winds down, and recycles, as does
       a client sent off by its window's close. The console interrupt
       of an idle server is the way out. */
    for (;;) {

        ami_long h;
        int  faulted;

        faulted = setjmp(sesjmp);
        sesjmpset = 1; /* from here a fault recycles the session */
        rbuf = rbase; /* a fault mid-batch leaves the walk pointer inside */
        if (faulted) goto winddown; /* the pump lives on, idle */

        /* fresh session */
        byeseen = 0;
        sigasked = 0;
        cleaning = 0;

        /* the hello; a mid-session hello was already read and answered,
           and its client is waiting on the event channel exchange */
        if (!hellopend) {

            /* a plain channel waits here, on the hello; a secure one
               waited on the handshake, and announced itself there */
            if (!gsecure) waiting();
            rlen = ami_rdmsg(cmdfn, rbase, msgmax);
            if (rlen < (ami_long)sizeof(gr_msghdr))
                sesserr("Short message from client");
            if (((gr_msghdr*)rbase)->mid != GR_MHELLO)
                sesserr(gsecure? "Protocol failure: no hello":
                        "Protocol failure: no hello (a client wanting a "
                        "secure channel needs the server started with -s)");
            ondisplay(job_dgram, NULL);

        }
        hellopend = 0;

        /* The event channel hello names the peer for events. The wait is
           bounded: a client that died between hellos must not wedge the
           server, and its successor's hello waits on the command
           channel. */
        {

            /* the bound by the ready test, the read by the message call,
               which is what records the peer the events go back to */
            if (!ami_rdymsg(evtfn, 10*1000000L))
                sesserr("Protocol failure: no event channel hello");
            rlen = ami_rdmsg(evtfn, rbase, msgmax);
            if (rlen < (ami_long)sizeof(gr_msghdr) ||
                ((gr_msghdr*)rbuf)->mid != GR_MEVOPEN)
                sesserr("Protocol failure: no event channel hello");

        }
        /* a window held from the last program goes first: the display
           is the new program's now */
        endhold(0);
        /* the program's window, with the library's defaults until the
           program says otherwise */
        sesautohold = 1;
        sestitle[0] = 0;
        ondisplay(job_openses, NULL);
        clientup = 1; /* the pump forwards from here on */
        status("connection arrived on port %lld, its event channel on port "
               "%lld", AMI_LONG_CAST(srvport), AMI_LONG_CAST(srvport+1));

        /* The command loop. A burst of commands executes under one hold
           of the display lock: after the blocking read, the socket
           drains without blocking, so drawing throughput is bounded by
           the wire and not by the heartbeat. */
        while (!byeseen) {

            rlen = ami_rdmsg(cmdfn, rbase, msgmax);
            for (;;) {

                ondisplay(job_dgram, NULL);
                if (byeseen) break;
                /* the drain probes by the ready test and reads by the
                   message call, which under the secure channel is also
                   what decrypts: a raw recv on DTLS sees only ciphertext */
                if (!ami_rdymsg(cmdfn, 0))
                    break; /* the burst is drained */
                rlen = ami_rdmsg(cmdfn, rbase, msgmax);

            }

        }
        if (sesautohold && !sigasked && !hellopend) {

            /* The program ended on its own with its autohold on: its
               window holds, title marked, until it is closed or the next
               client comes, as a native program's would. Its channels
               are done regardless; the display part of the session ends
               with the hold. */
            ami_lock(joblock);
            holding = 1;
            ami_unlock(joblock);
            ondisplay(job_hold, NULL);
            status("connection on port %lld closed: the program ended, its "
                   "window held until closed", AMI_LONG_CAST(srvport));
            clientup = 0;
            cleaning = 1; /* a fault in the channel work is fatal */

        } else {

            /* no hold: the session's windows go now. A terminate that
               asked this session to end ended just the session, as any
               other bye; the server waits for the next client. */
winddown:
            status("connection on port %lld closed: %s",
                   AMI_LONG_CAST(srvport),
                   faulted?   "session error":
                   hellopend? "a new client's hello replaced it":
                   sigasked?  "the window was closed on it":
                              "the program ended");
            clientup = 0;
            cleaning = 1; /* a fault in the winddown itself is fatal */
            ondisplay(job_winddown, NULL);

        }

        if (gsecure) {

            /* A secure channel belongs to its client: the DTLS
               association is peer-bound, so the next session needs
               fresh channels, accepted from the next client's
               handshakes. The reopen waits inside for the client. */
            inclose = 1;
            if (!setjmp(closejmp)) ami_clsmsg(cmdfn);
            if (!setjmp(closejmp)) ami_clsmsg(evtfn);
            inclose = 0;
            waiting();
            cmdfn = ami_waitmsg(srvport, gsecure);
            evtfn = ami_waitmsg(srvport+1, gsecure);
            ami_tmomsg(cmdfn, 0);
            ami_tmomsg(evtfn, 0);

        } else if (faulted) {

            /* drop whatever the dead session left on the channels, so
               the next hello read is not stale traffic */
            {

                ami_long dfd;
                int  di;

                for (di = 0; di < 2; di++) {

                    dfd = di? evtfn: cmdfn;
                    for (;;) {

                        if (!ami_rdymsg(dfd, 0)) break;
                        ami_rdmsg(dfd, rbase, msgmax);

                    }

                }

            }

        }
        cleaning = 0;

    }

}
