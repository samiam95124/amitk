/*******************************************************************************
*                                                                              *
*                        REMOTE DISPLAY CLIENT MODULE                          *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                            2026/08/14 S. A. Franco                           *
*                                                                              *
* The client side of remote display mode. Links with the program in place of  *
* the display library and exposes the complete graphics.h API: each call      *
* composes a message of the remote display protocol (graph_remote.h, and the  *
* paper doc/remote.pdf) and sends it to the server, which owns the display.   *
* Queries wait for their reply. Events are pushed by the server as they are   *
* ready; a receiver thread here moves them from the event channel to the      *
* client's queue as they arrive, so the transport is always drained, and      *
* event() takes from the queue. The queue is also where the anticipated       *
* optimization of collapsing redundant events belongs. Calls stay on the      *
* program's thread.                                                           *
*                                                                              *
* The server is named by environment: GRAPH_SERVER holds the address, a name  *
* or dotted form, 127.0.0.1 if unset; GRAPH_PORT holds the command port,      *
* GR_DEFPORT if unset. The channels are secure unless GRAPH_PLAIN is set,     *
* and the server must agree: it is secure unless started with --plain.        * The connection is made at program start and the        *
* program errors if there is no server.                                       *
*                                                                              *
* The ordinary write stream of a window travels as write messages: the        *
* module interdicts the system write for window files, exactly as network     *
* interdicts its own. Reads of standard input assemble a line from the        *
* event stream, echoing as a terminal does. eventover(), eventsover() and     *
* sendevent() act on the client's own event stream and put nothing on the     *
* wire. Files named in calls are served to the server on request by the       *
* protocol's file transfer exchange.                                          *
*                                                                              *
* openprint() is reserved by the protocol and not provided here.              *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include <graphics.h>
#include <sound.h>
#include <network.h>
#include <graph_remote.h>

#ifdef _WIN32
/* the program's name comes from the system (see pgmname); the call is
   declared here rather than pulling windows.h into the client */
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* module, char* name, unsigned long size);
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#define MAXFDS  512  /* file descriptors tracked for window files */
#define MAXHND  512  /* window handles */
#define MAXEVH  128  /* per code event handler table */
#define MAXLIN  512  /* standard input line assembly */

/* connection state */
static ami_long      cmdfn = -1;  /* command channel */
static ami_long      evtfn = -1;  /* event channel */
static ami_ulong srvaddr;     /* server address */
static ami_long      srvport;     /* command port */
static ami_long      msgmax;      /* channel message size bound */
static int           connected;   /* the link is up */
static int           gtrace;      /* diagnostic trace: GRAPH_TRACE set;
                                     every message prints to the error
                                     channel. Slow, and worth it. */
static int           gsecure;     /* secure channels: GRAPH_SECURE set;
                                     DTLS on the messages, TLS on the
                                     file connections */

/* message assembly */
static unsigned char* sbuf;       /* send buffer, msgmax bytes */
static ami_long       soff;       /* send offset */
static unsigned char* rbuf;       /* receive buffer, msgmax bytes */
static ami_long       roff;       /* receive parse offset */
static ami_long       rlen;       /* received length */
static short          sseq;       /* request serial */
/* The flow window: a command burst must not outrun the server's socket
   into datagram loss, loopback being reliable only while the consumer
   keeps up. The client counts outstanding bytes and fences with a sync
   round trip once per window; any query resets the count, being a
   fence itself. Never a wait during a stream: the fence is one round
   trip per window, amortized to nothing. */
/* The window counts messages, not bytes: the kernel accounts a socket
   buffer in per-datagram truesize, hundreds of bytes for the smallest
   message, so a stock receive buffer holds only a few hundred
   datagrams however small their payloads. Batching packs tens of
   messages into each datagram's truesize, which is what affords a
   window this wide. */
#define GRWINDOW 256
/* The window also counts bytes: a wave stream's messages fill the
   channel maximum, and a few of those cover the receiver's whole
   socket buffer, which the kernel accounts with generous overhead.
   The byte fence paces a bulk stream to the consumer, which for wave
   output is the playback rate itself. */
#define GRWINBYTES 65536
static ami_long       sentb;      /* messages since the last fence */
static ami_long       sentbytes;  /* bytes since the last fence */
static int            inquery;    /* a round trip is in progress */

/* window bookkeeping */
static ami_long fdh[MAXFDS];          /* file descriptor to window handle */
static ami_long h2lw[MAXHND];         /* window handle to logical window id */
static ami_long nexth = 2;            /* next handle; 1 is the main window */

/* The query cache: the stable metrics of a window, served locally after
   the first round trip. Remotely a query is a round trip, and programs
   ask for bounds and character metrics constantly; the API defines when
   the answers can change, so between those points the cache answers.
   Size metrics drop on a resize event or a size call; character metrics
   drop on any call that touches the font or its attributes; the dots
   per meter and the font count never change. */
#define QCMAXX    0x001
#define QCMAXY    0x002
#define QCMAXXG   0x004
#define QCMAXYG   0x008
#define QCCHRSIZX 0x010
#define QCCHRSIZY 0x020
#define QCBASELIN 0x040
#define QCDPMX    0x080
#define QCDPMY    0x100
#define QCFONTS   0x200
#define QCPOINTS  0x400
#define QCSIZES   (QCMAXX|QCMAXY|QCMAXXG|QCMAXYG)
#define QCCHARS   (QCMAXX|QCMAXY|QCCHRSIZX|QCCHRSIZY|QCBASELIN|QCPOINTS)

typedef struct qcache {

    unsigned valid; /* which entries hold */
    ami_long maxx, maxy, maxxg, maxyg;
    ami_long chrsizx, chrsizy, baselin;
    ami_long dpmx, dpmy, fonts;
    float    points;

} qcache;

static qcache qc[MAXHND];

/* the resize event, seen by the receiver, drops the window's sizes */
static void qcresize(ami_long h)

{

    if (h >= 1 && h < MAXHND) qc[h].valid &= ~QCSIZES;

}

/* event machinery. The receiver thread fills the queue; event() and
   sendevent() are the other users. The lock covers the queue only. */
static pthread_t       evthrd;     /* the receiver thread */
static volatile int    evstop;     /* the receiver is to stop, at close */
static pthread_mutex_t evlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  evcond = PTHREAD_COND_INITIALIZER;
static ami_evtrec*     evq;        /* event queue ring, grows as needed */
static ami_long        evqsiz;     /* ring size */
static ami_long        evqhead;    /* take point */
static ami_long        evqcnt;     /* events held */
static ami_pevthan     evthan[MAXEVH]; /* per code handlers */
static ami_pevthan     evtshan;    /* master handler */

/* line assembly for standard input */
static char linebuf[MAXLIN];
static ami_long linelen;
static ami_long linepos;
static int  linefull;

/* types of system vectors for override calls */
typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*pclose_t)(int);

/* system override calls */
extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);

/* saved system vectors */
static pread_t  dn_read;
static pwrite_t dn_write;
static pclose_t dn_close;

/*******************************************************************************

Report error and stop

*******************************************************************************/

static void error(const char* es)

{

    /* bypass the file layers: write directly to the error channel */
    write(2, "*** graph_client: ", 18);
    write(2, es, strlen(es));
    write(2, "\n", 1);
    exit(1);

}

/*******************************************************************************

Persistent channel

Message channels arrive with short receive timeouts, meant for transient
exchanges where a lost datagram should fail the read. These channels are
persistent: a quiet stretch is normal, a dialog can hold a reply for as
long as the user thinks, and the protocol requires a reliable carrier.
Clear the timeout.

*******************************************************************************/

static void persistent(ami_long fn)

{

    ami_tmomsg(fn, 0); /* no bound on the read */

}

/*******************************************************************************

Message assembly

The wire format of graph_remote.h: the fixed header, then the payload
values, little endian, which on the supported hosts is a plain copy.

*******************************************************************************/

static gr_msghdr* shdr(void) { return ((gr_msghdr*)sbuf); }
static gr_msghdr* rhdr(void) { return ((gr_msghdr*)rbuf); }

/* forward: the fence in send0() uses these, defined below */
static void qsend(void);
static void flushcmd(void);
static ami_long gi(void);
static void servefile(void);
static void dosync(void);

/* begin a message. The write stream flushes first: the manual's first
   condition for remote display is that all output applies in the order
   issued, and a buffered partial line, a form feed being the classic
   case, must not arrive after the calls that followed it. The flush
   lands its own message through the write interdiction before this one
   builds. */
static void begin(int mid, ami_long wid)

{

    if (connected) fflush(stdout);
    shdr()->mid = mid;
    shdr()->seq = 0;
    shdr()->wid = wid;
    soff = sizeof(gr_msghdr);

}

/* payload values */
static void pi(ami_long v)

{

    if (soff+8 > msgmax) error("Message too large for channel");
    memcpy(sbuf+soff, &v, 8);
    soff += 8;

}

static void pf(double v)

{

    if (soff+8 > msgmax) error("Message too large for channel");
    memcpy(sbuf+soff, &v, 8);
    soff += 8;

}

static void p4(int v)

{

    if (soff+4 > msgmax) error("Message too large for channel");
    memcpy(sbuf+soff, &v, 4);
    soff += 4;

}

static void psn(const char* s, ami_long n)

{

    p4((int)n);
    if (soff+n > msgmax) error("Message too large for channel");
    memcpy(sbuf+soff, s, n);
    soff += n;

}

static void ps(const char* s)

{

    psn(s, s? strlen(s): 0);

}

/* The command batch. Messages gather in one buffer and go out as one
   datagram, saving the per-datagram cost on the hot paths. Nothing ever
   waits to fill a batch: every boundary, a query, event(), the fence,
   the bye, sends at once, and the flusher thread bounds a lone trailing
   command to a couple of milliseconds. A datagram carries one or more
   messages, back to back; the header lengths delimit them. */

#define BATLIM 1400 /* batch bound: one wire MTU's worth */

static unsigned char*  batbuf;   /* the batch, NULL until the layer is up */
static ami_long        batmax;   /* its bound */
static ami_long        batlen;   /* bytes gathered */
static pthread_mutex_t batlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  batcond = PTHREAD_COND_INITIALIZER;

/* put the batch on the wire; the lock is held */
static void batflush(void)

{

    if (batlen) { ami_wrmsg(cmdfn, batbuf, batlen); batlen = 0; }

}

/* a boundary: whatever has gathered goes now */
static void flushcmd(void)

{

    if (!batbuf) return;
    pthread_mutex_lock(&batlock);
    batflush();
    pthread_mutex_unlock(&batlock);

}

/* the flusher: a lone trailing command must not sit in the batch
   waiting for a boundary that is not coming */
static void* batrun(void* arg)

{

    struct timespec ts;

    pthread_mutex_lock(&batlock);
    for (;;) {

        int rc = 0;

        while (!batlen) pthread_cond_wait(&batcond, &batlock);
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 1500000; /* the bound */
        if (ts.tv_nsec >= 1000000000)
            { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        while (batlen && rc != ETIMEDOUT)
            rc = pthread_cond_timedwait(&batcond, &batlock, &ts);
        batflush();

    }

    return (NULL);

}

/* send the assembled message, no reply expected */
static void send0(void)

{

    shdr()->len = (int)soff;
    if (gtrace)
        fprintf(stderr, "gc> %-16s w%-3lld s%-5d len%lld\n",
                gr_msgname(shdr()->mid), AMI_LONG_CAST(shdr()->wid), shdr()->seq, AMI_LONG_CAST(soff));
    if (!batbuf) ami_wrmsg(cmdfn, sbuf, soff); /* the layer is not up */
    else {

        pthread_mutex_lock(&batlock);
        if (batlen+soff > batmax) batflush();
        if (soff >= batmax) ami_wrmsg(cmdfn, sbuf, soff); /* oversize */
        else {

            if (!batlen) pthread_cond_signal(&batcond);
            memcpy(batbuf+batlen, sbuf, soff);
            batlen += soff;

        }
        pthread_mutex_unlock(&batlock);

    }
    sentb++;
    sentbytes += soff;
    if (!inquery && (sentb >= GRWINDOW || sentbytes >= GRWINBYTES))
        dosync();

}

/* reply values */
static ami_long gi(void)

{

    ami_long v;

    if (roff+8 > rlen) error("Reply truncated");
    memcpy(&v, rbuf+roff, 8);
    roff += 8;

    return (v);

}

static double gf(void)

{

    double v;

    if (roff+8 > rlen) error("Reply truncated");
    memcpy(&v, rbuf+roff, 8);
    roff += 8;

    return (v);

}

static int g4(void)

{

    int v;

    if (roff+4 > rlen) error("Reply truncated");
    memcpy(&v, rbuf+roff, 4);
    roff += 4;

    return (v);

}

/* a counted string into a caller's buffer. The buffer rule of the API:
   a result filling the buffer exactly leaves the terminator off. */
static void gs(char* dst, ami_long dl)

{

    ami_long n = g4();
    ami_long c = n;

    if (roff+n > rlen) error("Reply truncated");
    if (c > dl) c = dl;
    memcpy(dst, rbuf+roff, c);
    if (c < dl) dst[c] = 0;
    roff += n;

}

/*******************************************************************************

File transfer service

The server, missing a file we named, asks for it inside the reply window,
naming its file port. We open the port as a stream connection and stream
the file in, close delimited. A file we do not hold streams as empty, and
the server's complaint follows. The passive form of ftp: the client
connects, because it is the side that knows the other's address.

*******************************************************************************/

static void servefile(void)

{

    char   fn[256];
    ami_long   n, port;
    FILE*  nf;
    FILE*  lf;
    char   buf[4096];
    size_t r;

    /* the request carries the name and the port */
    rlen = ((gr_msghdr*)rbuf)->len;
    roff = sizeof(gr_msghdr);
    n = g4();
    if (roff+n > rlen) error("Message truncated");
    if (n > (ami_long)sizeof(fn)-1) n = sizeof(fn)-1;
    memcpy(fn, rbuf+roff, n);
    fn[n] = 0;
    roff += n;
    port = gi();
    /* The name arrives already under the library's extension rule,
       .bmp, .wav or .mid by the call that asked; serve exactly what is
       asked. */
    /* let the server reach its accept before we knock */
    usleep(50000);
    nf = ami_opennet(srvaddr, port, gsecure);
    if (!nf) error("Cannot open file transfer connection");
    lf = fopen(fn, "rb");
    if (getenv("PA_FILEDBG")) {
        fprintf(stderr, "servefile: '%s' -> %s\n", fn, lf? "found": "MISSING");
        fflush(stderr);
    }
    if (lf) {

        while ((r = fread(buf, 1, sizeof(buf), lf)) > 0)
            if (fwrite(buf, 1, r, nf) != r)
                error("Cannot send file to server");
        fclose(lf);

    }
    /* no file: the empty stream tells the server so */
    fclose(nf);

}

/*******************************************************************************

Queries

A query sends its message and waits on the command channel for the reply.
File requests are served from inside the wait; anything else out of order
is a protocol failure.

*******************************************************************************/

static void qsend(void)

{

    inquery = 1;
    sseq++;
    shdr()->seq = sseq;
    send0();
    flushcmd(); /* the request must be on the wire before the wait */
    for (;;) {

        rlen = ami_rdmsg(cmdfn, rbuf, msgmax);
        if (rlen < (ami_long)sizeof(gr_msghdr)) error("Short message from server");
        if (gtrace)
            fprintf(stderr, "gc< %-16s w%-3lld s%-5d len%lld\n",
                    gr_msgname(rhdr()->mid), AMI_LONG_CAST(rhdr()->wid), rhdr()->seq, AMI_LONG_CAST(rlen));
        if (rhdr()->mid == GR_MFILEREQ) { servefile(); continue; }
        if (rhdr()->mid != GR_MREPLY) error("Protocol failure: not a reply");
        if (rhdr()->seq != sseq) {

            /* a stale reply, a resent fence's duplicate being the
               normal source, discards; a reply from the future is a
               real failure */
            if ((short)(sseq-rhdr()->seq) > 0) continue;
            error("Protocol failure: reply serial");

        }
        roff = sizeof(gr_msghdr);
        inquery = 0;
        sentb = 0; /* a round trip is a fence */
        sentbytes = 0;
        return;

    }

}

/* The flow fence. Unlike a query it retries: under pressure the fence
   datagram itself can be the one the kernel drops, and the sync is
   idempotent, so a bounded wait and a resend recover it. Once the
   window holds, the queues stay shallow and ordinary queries ride
   safely. */
static void dosync(void)

{

    flushcmd(); /* the fence orders after everything batched */
    inquery = 1;
    for (;;) {

        sseq++;
        shdr()->mid = GR_MSYNC;
        shdr()->seq = sseq;
        shdr()->wid = 0;
        soff = sizeof(gr_msghdr);
        shdr()->len = (int)soff;
        if (gtrace)
            fprintf(stderr, "gc> %-16s w0   s%-5d len%lld\n",
                    gr_msgname(GR_MSYNC), sseq, AMI_LONG_CAST(soff));
        ami_wrmsg(cmdfn, sbuf, soff);
        /* the bound by the ready test, the read by the message call,
           which under the secure channel is also what decrypts */
        if (!ami_rdymsg(cmdfn, 200000))
            continue; /* lost: fence again */
        rlen = ami_rdmsg(cmdfn, rbuf, msgmax);
        if (rlen < (ami_long)sizeof(gr_msghdr)) continue;
        if (rhdr()->mid == GR_MFILEREQ) { servefile(); continue; }
        if (rhdr()->mid == GR_MREPLY) break; /* any sync's reply serves */

    }
    inquery = 0;
    sentb = 0;
    sentbytes = 0;

}

/*******************************************************************************

Window handles

*******************************************************************************/

/* window handle from file */
static ami_long wh(FILE* f)

{

    int fn;

    if (!f) error("Invalid window file");
    fn = fileno(f);
    if (fn < 0 || fn >= MAXFDS || !fdh[fn]) error("Not a window file");

    return (fdh[fn]);

}

/*******************************************************************************

Menu and string list serialization

*******************************************************************************/

static void pmenu(ami_menuptr m)

{

    ami_menuptr p;
    int         n = 0;

    for (p = m; p; p = p->next) n++;
    p4(n);
    for (p = m; p; p = p->next) {

        pi((!!p->onoff)|((!!p->oneof)<<1)|((!!p->bar)<<2));
        pi(p->id);
        ps(p->face);
        pmenu(p->branch);

    }

}

static ami_menuptr gmenu(void)

{

    ami_menuptr lst = NULL;
    ami_menuptr lp = NULL;
    ami_menuptr e;
    int         n, i;
    ami_long    flags, sl;

    n = g4();
    for (i = 0; i < n; i++) {

        e = malloc(sizeof(ami_menurec));
        if (!e) error("Out of memory");
        flags = gi();
        e->onoff = !!(flags&1);
        e->oneof = !!(flags&2);
        e->bar = !!(flags&4);
        e->id = gi();
        sl = g4();
        if (roff+sl > rlen) error("Reply truncated");
        e->face = malloc(sl+1);
        if (!e->face) error("Out of memory");
        memcpy(e->face, rbuf+roff, sl);
        e->face[sl] = 0;
        roff += sl;
        e->branch = gmenu();
        e->next = NULL;
        if (lp) lp->next = e; else lst = e;
        lp = e;

    }

    return (lst);

}

static void pslst(ami_strptr sp)

{

    ami_strptr p;
    int        n = 0;

    for (p = sp; p; p = p->next) n++;
    p4(n);
    for (p = sp; p; p = p->next) ps(p->str);

}

/*******************************************************************************

Events

The server pushes events as they are ready; the receiver thread moves them
from the event channel to the queue as they arrive, so the transport is
always drained and never backs up into the channel. event() takes from the
queue; sendevent() injects into it. The override handlers are the
client's own.

The queue is the natural seat of the anticipated optimization: redundant
events, a run of mouse movements of which only the last matters, can
collapse here without any change on the wire. It is not yet done.

*******************************************************************************/

/* the size of the event record union: the largest member is the joystick
   move, seven longs */
#define EVUNION (7*8)

static void wire2evt(gr_msgevt* we, ami_evtrec* er)

{

    ami_long h = we->winid;

    memset(er, 0, sizeof(ami_evtrec));
    if (h >= 1 && h < MAXHND) er->winid = h2lw[h];
    else er->winid = h;
    er->etype = (ami_evtcod)we->etype;
    er->handled = 0;
    memcpy(&er->echar, we->p, EVUNION);

}

/* true if b supersedes a in the queue: the same movement stream from
   the same device, or a resize of the same window, of which only the
   latest matters */
static int evqsuper(ami_evtrec* a, ami_evtrec* b)

{

    if (a->etype != b->etype) return (0);
    switch (a->etype) {

        case ami_etmoumov:  return (a->mmoun == b->mmoun);
        case ami_etmoumovg: return (a->mmoung == b->mmoung);
        case ami_etjoymov:  return (a->mjoyn == b->mjoyn);
        case ami_etresize:  return (a->winid == b->winid);
        default: return (0);

    }

}

/* append to the queue, growing the ring as needed; the lock is held.
   A movement or resize replaces an unconsumed one it supersedes at the
   tail of the queue instead of stacking behind it. */
static void evqput(ami_evtrec* er)

{

    ami_evtrec* nq;
    ami_long    i;

    if (evqcnt) {

        ami_evtrec* tail = &evq[(evqhead+evqcnt-1)%evqsiz];

        if (evqsuper(tail, er)) { *tail = *er; return; }

    }
    if (evqcnt == evqsiz) {

        nq = malloc(evqsiz*2*sizeof(ami_evtrec));
        if (!nq) error("Out of memory");
        for (i = 0; i < evqcnt; i++) nq[i] = evq[(evqhead+i)%evqsiz];
        free(evq);
        evq = nq;
        evqhead = 0;
        evqsiz *= 2;

    }
    evq[(evqhead+evqcnt)%evqsiz] = *er;
    evqcnt++;
    pthread_cond_signal(&evcond);

}

/* the receiver thread: drain the event channel into the queue forever */
static void* evrun(void* arg)

{

    unsigned char* ebuf;
    gr_msghdr*     h;
    gr_msgevt*     we;
    ami_evtrec     er;
    ami_long       n;

    (void)arg;
    ebuf = malloc(msgmax);
    if (!ebuf) error("Out of memory");
    for (;;) {

        /* The wait is bounded so the stop at close is seen, and the read
           is taken only when it will not block: a read that fails is fatal
           in the library, so the channel must still be whole under it. */
        while (!evstop && !ami_rdymsg(evtfn, 50000L));
        if (evstop) break;
        n = ami_rdmsg(evtfn, ebuf, msgmax);
        if (n < (ami_long)sizeof(gr_msghdr)) error("Short message from server");
        h = (gr_msghdr*)ebuf;
        if (h->mid == GR_MERROR) {

            /* the server's error is ours */
            static char es[256];
            int  sl;
            memcpy(&sl, ebuf+sizeof(gr_msghdr), 4);
            if (sl > (int)sizeof(es)-1) sl = sizeof(es)-1;
            memcpy(es, ebuf+sizeof(gr_msghdr)+4, sl);
            es[sl] = 0;
            error(es);

        }
        if (h->mid != GR_MEVENT) error("Protocol failure: not an event");
        we = (gr_msgevt*)(ebuf+sizeof(gr_msghdr));
        if ((ami_long)we->etype == (ami_long)ami_etresize) qcresize(we->winid);
        wire2evt(we, &er);
        if (gtrace)
            fprintf(stderr, "gc<e %-15s w%lld\n",
                    gr_evtname((ami_long)er.etype), AMI_LONG_CAST(er.winid));
        pthread_mutex_lock(&evlock);
        evqput(&er);
        pthread_mutex_unlock(&evlock);

    }

    return (NULL);

}

/* The console interrupt becomes the terminate event, as it does on the
   display: the program winds down its normal way, and the bye follows.
   The signals wait on their own thread, so injecting is ordinary thread
   work; a second interrupt, the program having ignored the first, is
   force. */
#ifndef _WIN32
static void* sigrun(void* arg)

{

    sigset_t   set;
    int        sig;
    ami_evtrec er;
    int        asked = 0;

    (void)arg;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    for (;;) {

        if (sigwait(&set, &sig)) return (NULL);
        if (asked) exit(1); /* asked nicely once already */
        asked = 1;
        if (gtrace) fprintf(stderr, "gc:  interrupt -> ETTERM injected\n");
        memset(&er, 0, sizeof(er));
        er.winid = 1;
        er.etype = ami_etterm;
        pthread_mutex_lock(&evlock);
        evqput(&er);
        pthread_mutex_unlock(&evlock);

    }

    return (NULL);

}
#else
/* Windows runs the console interrupt handler on a thread of its own, so
   the injection is ordinary thread work here too, with no signal to wait
   on. The disposition resets on delivery and is put back. */
static void winsig(int sig)

{

    static int asked = 0;
    ami_evtrec er;

    signal(sig, winsig);
    if (asked) exit(1); /* asked nicely once already */
    asked = 1;
    if (gtrace) fprintf(stderr, "gc:  interrupt -> ETTERM injected\n");
    memset(&er, 0, sizeof(er));
    er.winid = 1;
    er.etype = ami_etterm;
    pthread_mutex_lock(&evlock);
    evqput(&er);
    pthread_mutex_unlock(&evlock);

}
#endif

/* dispatch an event through the override handlers; TRUE if it was
   consumed */
static int evdispatch(ami_evtrec* er)

{

    er->handled = 0;
    if (evtshan) (*evtshan)(er);
    if (!er->handled && er->etype >= 0 && er->etype < MAXEVH &&
        evthan[er->etype]) (*evthan[er->etype])(er);

    return (er->handled);

}

void ami_event(FILE* f, ami_evtrec* er)

{

    (void)f; /* events are not per file */
    /* the rule of the manual: all outstanding output completes before
       input is taken. The window files are unbuffered; the standard
       output holds partial lines, positioned text being the classic
       case, and flushes here. */
    fflush(stdout);
    flushcmd(); /* output completes before input, all the way out */
    for (;;) {

        pthread_mutex_lock(&evlock);
        while (!evqcnt) pthread_cond_wait(&evcond, &evlock);
        *er = evq[evqhead];
        evqhead = (evqhead+1)%evqsiz;
        evqcnt--;
        pthread_mutex_unlock(&evlock);
        if (!evdispatch(er)) return; /* not consumed: the program's */

    }

}

void ami_eventover(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)

{

    if (e < 0 || e >= MAXEVH) error("Invalid event code");
    *oeh = evthan[e];
    evthan[e] = eh;

}

void ami_eventsover(ami_pevthan eh, ami_pevthan* oeh)

{

    *oeh = evtshan;
    evtshan = eh;

}

void ami_sendevent(FILE* f, ami_evtrec* er)

{

    (void)f;
    pthread_mutex_lock(&evlock);
    evqput(er);
    pthread_mutex_unlock(&evlock);

}

/*******************************************************************************

System vector interdiction

Writes to window files are the write stream, chunked to the channel bound.
Reads of standard input assemble a line from the event stream with echo,
as a terminal presents.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buf, size_t n)

{

    const char* s = buf;
    size_t      i = 0;
    ami_long    chunk;

    if (!connected || fd < 0 || fd >= MAXFDS || !fdh[fd])
        return ((*dn_write)(fd, buf, n));
    chunk = msgmax-(ami_long)sizeof(gr_msghdr)-4;
    while (i < (size_t)n) {

        ami_long c = n-i > (size_t)chunk? chunk: (ami_long)(n-i);
        shdr()->mid = GR_MWRITE;
        shdr()->seq = 0;
        shdr()->wid = fdh[fd];
        soff = sizeof(gr_msghdr);
        p4((int)c);
        memcpy(sbuf+soff, s+i, c);
        soff += c;
        send0();
        i += c;

    }

    return (n);

}

/* echo through the write stream of the main window */
static void echo(const char* s, ami_long n)

{

    iwrite(1, s, n);

}

static ssize_t iread(int fd, void* buf, size_t n)

{

    ami_evtrec er;
    ami_long   c;

    if (!connected || fd != 0) return ((*dn_read)(fd, buf, n));
    /* assemble a line from the event stream, echoing */
    if (!linefull) {

        linelen = 0;
        for (;;) {

            ami_event(stdin, &er);
            if (er.etype == ami_etchar && linelen < MAXLIN-1) {

                linebuf[linelen++] = er.echar;
                echo(&er.echar, 1);

            } else if (er.etype == ami_etdelcb && linelen) {

                linelen--;
                echo("\b \b", 3);

            } else if (er.etype == ami_etenter) {

                linebuf[linelen++] = '\n';
                echo("\n", 1);
                break;

            } else if (er.etype == ami_etterm) exit(0);

        }
        linepos = 0;
        linefull = 1;

    }
    c = linelen-linepos;
    if (c > (ami_long)n) c = n;
    memcpy(buf, linebuf+linepos, c);
    linepos += c;
    if (linepos >= linelen) linefull = 0;

    return (c);

}

static int iclose(int fd)

{

    if (connected && fd >= 0 && fd < MAXFDS && fdh[fd]) {

        begin(GR_MCLOSEWIN, fdh[fd]);
        send0();
        qc[fdh[fd]].valid = 0;
        fdh[fd] = 0;

    }

    return ((*dn_close)(fd));

}

/*******************************************************************************

The API: terminal level

Each call composes its message per the catalog; queries wait for the
reply.

*******************************************************************************/

void ami_cursor(FILE* f, ami_long x, ami_long y)
    { begin(GR_MCURSOR, wh(f)); pi(x); pi(y); send0(); }
ami_long ami_maxx(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCMAXX)) {
          begin(GR_MMAXX, h); qsend();
          qc[h].maxx = gi(); qc[h].valid |= QCMAXX; }
      return (qc[h].maxx); }
ami_long ami_maxy(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCMAXY)) {
          begin(GR_MMAXY, h); qsend();
          qc[h].maxy = gi(); qc[h].valid |= QCMAXY; }
      return (qc[h].maxy); }
void ami_home(FILE* f)
    { begin(GR_MHOME, wh(f)); send0(); }
void ami_del(FILE* f)
    { begin(GR_MDEL, wh(f)); send0(); }
void ami_up(FILE* f)
    { begin(GR_MUP, wh(f)); send0(); }
void ami_down(FILE* f)
    { begin(GR_MDOWN, wh(f)); send0(); }
void ami_left(FILE* f)
    { begin(GR_MLEFT, wh(f)); send0(); }
void ami_right(FILE* f)
    { begin(GR_MRIGHT, wh(f)); send0(); }
void ami_blink(FILE* f, ami_long e)
    { begin(GR_MBLINK, wh(f)); pi(e); send0(); }
void ami_reverse(FILE* f, ami_long e)
    { begin(GR_MREVERSE, wh(f)); pi(e); send0(); }
void ami_underline(FILE* f, ami_long e)
    { begin(GR_MUNDERLINE, wh(f)); pi(e); send0(); }
void ami_superscript(FILE* f, ami_long e)
    { begin(GR_MSUPERSCRIPT, wh(f)); pi(e); send0(); }
void ami_subscript(FILE* f, ami_long e)
    { begin(GR_MSUBSCRIPT, wh(f)); pi(e); send0(); }
void ami_italic(FILE* f, ami_long e)
    { begin(GR_MITALIC, wh(f)); pi(e); send0(); }
void ami_bold(FILE* f, ami_long e)
    { begin(GR_MBOLD, wh(f)); pi(e); send0(); }
void ami_strikeout(FILE* f, ami_long e)
    { begin(GR_MSTRIKEOUT, wh(f)); pi(e); send0(); }
void ami_standout(FILE* f, ami_long e)
    { begin(GR_MSTANDOUT, wh(f)); pi(e); send0(); }
void ami_fcolor(FILE* f, ami_color c)
    { begin(GR_MFCOLOR, wh(f)); pi((ami_long)c); send0(); }
void ami_bcolor(FILE* f, ami_color c)
    { begin(GR_MBCOLOR, wh(f)); pi((ami_long)c); send0(); }
void ami_auto(FILE* f, ami_long e)
    { begin(GR_MAUTO, wh(f)); pi(e); send0(); }
void ami_curvis(FILE* f, ami_long e)
    { begin(GR_MCURVIS, wh(f)); pi(e); send0(); }
void ami_scroll(FILE* f, ami_long x, ami_long y)
    { begin(GR_MSCROLL, wh(f)); pi(x); pi(y); send0(); }
ami_long ami_curx(FILE* f)
    { begin(GR_MCURX, wh(f)); qsend(); return (gi()); }
ami_long ami_cury(FILE* f)
    { begin(GR_MCURY, wh(f)); qsend(); return (gi()); }
ami_long ami_curbnd(FILE* f)
    { begin(GR_MCURBND, wh(f)); qsend(); return (gi()); }
void ami_select(FILE* f, ami_long u, ami_long d)
    { begin(GR_MSELECT, wh(f)); pi(u); pi(d); send0(); }
void ami_timer(FILE* f, ami_long i, ami_long t, ami_long r)
    { begin(GR_MTIMER, wh(f)); pi(i); pi(t); pi(r); send0(); }
void ami_killtimer(FILE* f, ami_long i)
    { begin(GR_MKILLTIMER, wh(f)); pi(i); send0(); }
ami_long ami_mouse(FILE* f)
    { begin(GR_MMOUSE, wh(f)); qsend(); return (gi()); }
ami_long ami_mousebutton(FILE* f, ami_long m)
    { begin(GR_MMOUSEBUTTON, wh(f)); pi(m); qsend(); return (gi()); }
ami_long ami_joystick(FILE* f)
    { begin(GR_MJOYSTICK, wh(f)); qsend(); return (gi()); }
ami_long ami_joybutton(FILE* f, ami_long j)
    { begin(GR_MJOYBUTTON, wh(f)); pi(j); qsend(); return (gi()); }
ami_long ami_joyaxis(FILE* f, ami_long j)
    { begin(GR_MJOYAXIS, wh(f)); pi(j); qsend(); return (gi()); }
void ami_settab(FILE* f, ami_long t)
    { begin(GR_MSETTAB, wh(f)); pi(t); send0(); }
void ami_restab(FILE* f, ami_long t)
    { begin(GR_MRESTAB, wh(f)); pi(t); send0(); }
void ami_clrtab(FILE* f)
    { begin(GR_MCLRTAB, wh(f)); send0(); }
ami_long ami_funkey(FILE* f)
    { begin(GR_MFUNKEY, wh(f)); qsend(); return (gi()); }
void ami_frametimer(FILE* f, ami_long e)
    { begin(GR_MFRAMETIMER, wh(f)); pi(e); send0(); }
void ami_autohold(ami_long e)
    { begin(GR_MAUTOHOLD, 0); pi(e); send0(); }
void ami_wrtstr(FILE* f, char* s)
    { begin(GR_MWRTSTR, wh(f)); ps(s); send0(); }
void ami_wrtstrn(FILE* f, char* s, ami_long n)
    { begin(GR_MWRTSTRN, wh(f)); psn(s, n); send0(); }
void ami_sizbuf(FILE* f, ami_long x, ami_long y)
    { ami_long h = wh(f); qc[h].valid &= ~QCSIZES;
      begin(GR_MSIZBUF, h); pi(x); pi(y); send0(); }
void ami_title(FILE* f, char* ts)
    { begin(GR_MTITLE, wh(f)); ps(ts); send0(); }
void ami_fcolorc(FILE* f, ami_long r, ami_long g, ami_long b)
    { begin(GR_MFCOLORC, wh(f)); pi(r); pi(g); pi(b); send0(); }
void ami_bcolorc(FILE* f, ami_long r, ami_long g, ami_long b)
    { begin(GR_MBCOLORC, wh(f)); pi(r); pi(g); pi(b); send0(); }

/*******************************************************************************

The API: graphical level

*******************************************************************************/

ami_long ami_maxxg(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCMAXXG)) {
          begin(GR_MMAXXG, h); qsend();
          qc[h].maxxg = gi(); qc[h].valid |= QCMAXXG; }
      return (qc[h].maxxg); }
ami_long ami_maxyg(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCMAXYG)) {
          begin(GR_MMAXYG, h); qsend();
          qc[h].maxyg = gi(); qc[h].valid |= QCMAXYG; }
      return (qc[h].maxyg); }
ami_long ami_curxg(FILE* f)
    { begin(GR_MCURXG, wh(f)); qsend(); return (gi()); }
ami_long ami_curyg(FILE* f)
    { begin(GR_MCURYG, wh(f)); qsend(); return (gi()); }
void ami_line(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { begin(GR_MLINE, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); send0(); }
void ami_linewidth(FILE* f, ami_long w)
    { begin(GR_MLINEWIDTH, wh(f)); pi(w); send0(); }
void ami_linestyle(FILE* f, ami_lstyle style)
    { begin(GR_MLINESTYLE, wh(f)); pi((ami_long)style); send0(); }
void ami_rect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { begin(GR_MRECT, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); send0(); }
void ami_frect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { begin(GR_MFRECT, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); send0(); }
void ami_rrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)
    { begin(GR_MRRECT, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(xs); pi(ys);
      send0(); }
void ami_frrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)
    { begin(GR_MFRRECT, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(xs); pi(ys);
      send0(); }
void ami_ellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { begin(GR_MELLIPSE, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); send0(); }
void ami_fellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { begin(GR_MFELLIPSE, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); send0(); }
void ami_arc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
    { begin(GR_MARC, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(sa); pi(ea);
      send0(); }
void ami_farc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
    { begin(GR_MFARC, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(sa); pi(ea);
      send0(); }
void ami_fchord(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
    { begin(GR_MFCHORD, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(sa); pi(ea);
      send0(); }
void ami_ftriangle(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3,
                   ami_long y3)
    { begin(GR_MFTRIANGLE, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(x3);
      pi(y3); send0(); }
void ami_cursorg(FILE* f, ami_long x, ami_long y)
    { begin(GR_MCURSORG, wh(f)); pi(x); pi(y); send0(); }
ami_long ami_baseline(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCBASELIN)) {
          begin(GR_MBASELINE, h); qsend();
          qc[h].baselin = gi(); qc[h].valid |= QCBASELIN; }
      return (qc[h].baselin); }
void ami_setpixel(FILE* f, ami_long x, ami_long y)
    { begin(GR_MSETPIXEL, wh(f)); pi(x); pi(y); send0(); }
void ami_fover(FILE* f)
    { begin(GR_MFOVER, wh(f)); send0(); }
void ami_bover(FILE* f)
    { begin(GR_MBOVER, wh(f)); send0(); }
void ami_finvis(FILE* f)
    { begin(GR_MFINVIS, wh(f)); send0(); }
void ami_binvis(FILE* f)
    { begin(GR_MBINVIS, wh(f)); send0(); }
void ami_fxor(FILE* f)
    { begin(GR_MFXOR, wh(f)); send0(); }
void ami_bxor(FILE* f)
    { begin(GR_MBXOR, wh(f)); send0(); }
void ami_fand(FILE* f)
    { begin(GR_MFAND, wh(f)); send0(); }
void ami_band(FILE* f)
    { begin(GR_MBAND, wh(f)); send0(); }
void ami_for(FILE* f)
    { begin(GR_MFOR, wh(f)); send0(); }
void ami_bor(FILE* f)
    { begin(GR_MBOR, wh(f)); send0(); }
ami_long ami_chrsizx(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCCHRSIZX)) {
          begin(GR_MCHRSIZX, h); qsend();
          qc[h].chrsizx = gi(); qc[h].valid |= QCCHRSIZX; }
      return (qc[h].chrsizx); }
ami_long ami_chrsizy(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCCHRSIZY)) {
          begin(GR_MCHRSIZY, h); qsend();
          qc[h].chrsizy = gi(); qc[h].valid |= QCCHRSIZY; }
      return (qc[h].chrsizy); }
ami_long ami_fonts(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCFONTS)) {
          begin(GR_MFONTS, h); qsend();
          qc[h].fonts = gi(); qc[h].valid |= QCFONTS; }
      return (qc[h].fonts); }
void ami_font(FILE* f, ami_long fc)
    { ami_long h = wh(f); qc[h].valid &= ~QCCHARS;
      begin(GR_MFONT, h); pi(fc); send0(); }
void ami_fontnam(FILE* f, ami_long fc, char* fns, ami_long fnsl)
    { begin(GR_MFONTNAM, wh(f)); pi(fc); qsend(); gs(fns, fnsl); }
void ami_fontsiz(FILE* f, ami_long s)
    { ami_long h = wh(f); qc[h].valid &= ~QCCHARS;
      begin(GR_MFONTSIZ, h); pi(s); send0(); }
void ami_setpoints(FILE* f, float ps)
    { ami_long h = wh(f); qc[h].valid &= ~QCCHARS;
      begin(GR_MSETPOINTS, h); pf(ps); send0(); }
float ami_points(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCPOINTS)) {
          begin(GR_MPOINTS, h); qsend();
          qc[h].points = (float)gf(); qc[h].valid |= QCPOINTS; }
      return (qc[h].points); }
void ami_chrspcy(FILE* f, ami_long s)
    { ami_long h = wh(f); qc[h].valid &= ~QCCHARS;
      begin(GR_MCHRSPCY, h); pi(s); send0(); }
void ami_chrspcx(FILE* f, ami_long s)
    { ami_long h = wh(f); qc[h].valid &= ~QCCHARS;
      begin(GR_MCHRSPCX, h); pi(s); send0(); }
ami_long ami_dpmx(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCDPMX)) {
          begin(GR_MDPMX, h); qsend();
          qc[h].dpmx = gi(); qc[h].valid |= QCDPMX; }
      return (qc[h].dpmx); }
ami_long ami_dpmy(FILE* f)
    { ami_long h = wh(f);
      if (!(qc[h].valid&QCDPMY)) {
          begin(GR_MDPMY, h); qsend();
          qc[h].dpmy = gi(); qc[h].valid |= QCDPMY; }
      return (qc[h].dpmy); }
ami_long ami_strsiz(FILE* f, const char* s)
    { begin(GR_MSTRSIZ, wh(f)); ps(s); qsend(); return (gi()); }
ami_long ami_chrpos(FILE* f, const char* s, ami_long p)
    { begin(GR_MCHRPOS, wh(f)); ps(s); pi(p); qsend(); return (gi()); }
void ami_writejust(FILE* f, const char* s, ami_long n)
    { begin(GR_MWRITEJUST, wh(f)); ps(s); pi(n); send0(); }
ami_long ami_justpos(FILE* f, const char* s, ami_long p, ami_long n)
    { begin(GR_MJUSTPOS, wh(f)); ps(s); pi(p); pi(n); qsend(); return (gi()); }
void ami_condensed(FILE* f, ami_long e)
    { ami_long h = wh(f); qc[h].valid &= ~QCCHARS;
      begin(GR_MCONDENSED, h); pi(e); send0(); }
void ami_extended(FILE* f, ami_long e)
    { ami_long h = wh(f); qc[h].valid &= ~QCCHARS;
      begin(GR_MEXTENDED, h); pi(e); send0(); }
void ami_xlight(FILE* f, ami_long e)
    { begin(GR_MXLIGHT, wh(f)); pi(e); send0(); }
void ami_light(FILE* f, ami_long e)
    { begin(GR_MLIGHT, wh(f)); pi(e); send0(); }
void ami_xbold(FILE* f, ami_long e)
    { begin(GR_MXBOLD, wh(f)); pi(e); send0(); }
void ami_hollow(FILE* f, ami_long e)
    { begin(GR_MHOLLOW, wh(f)); pi(e); send0(); }
void ami_raised(FILE* f, ami_long e)
    { begin(GR_MRAISED, wh(f)); pi(e); send0(); }
void ami_settabg(FILE* f, ami_long t)
    { begin(GR_MSETTABG, wh(f)); pi(t); send0(); }
void ami_restabg(FILE* f, ami_long t)
    { begin(GR_MRESTABG, wh(f)); pi(t); send0(); }
void ami_fcolorg(FILE* f, ami_long r, ami_long g, ami_long b)
    { begin(GR_MFCOLORG, wh(f)); pi(r); pi(g); pi(b); send0(); }
void ami_bcolorg(FILE* f, ami_long r, ami_long g, ami_long b)
    { begin(GR_MBCOLORG, wh(f)); pi(r); pi(g); pi(b); send0(); }
void ami_loadpict(FILE* f, ami_long p, char* fn)
    { begin(GR_MLOADPICT, wh(f)); pi(p); ps(fn); qsend(); gi(); }
ami_long ami_pictsizx(FILE* f, ami_long p)
    { begin(GR_MPICTSIZX, wh(f)); pi(p); qsend(); return (gi()); }
ami_long ami_pictsizy(FILE* f, ami_long p)
    { begin(GR_MPICTSIZY, wh(f)); pi(p); qsend(); return (gi()); }
void ami_picture(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
    { begin(GR_MPICTURE, wh(f)); pi(p); pi(x1); pi(y1); pi(x2); pi(y2);
      send0(); }
void ami_delpict(FILE* f, ami_long p)
    { begin(GR_MDELPICT, wh(f)); pi(p); send0(); }
void ami_scrollg(FILE* f, ami_long x, ami_long y)
    { begin(GR_MSCROLLG, wh(f)); pi(x); pi(y); send0(); }
void ami_path(FILE* f, ami_long a)
    { begin(GR_MPATH, wh(f)); pi(a); send0(); }
void ami_viewoffg(FILE* f, ami_long x, ami_long y)
    { begin(GR_MVIEWOFFG, wh(f)); pi(x); pi(y); send0(); }
void ami_viewscale(FILE* f, float x, float y)
    { begin(GR_MVIEWSCALE, wh(f)); pf(x); pf(y); send0(); }
ami_long ami_scalex(FILE* f, ami_long x)
    { begin(GR_MSCALEX, wh(f)); pi(x); qsend(); return (gi()); }
ami_long ami_scaley(FILE* f, ami_long y)
    { begin(GR_MSCALEY, wh(f)); pi(y); qsend(); return (gi()); }
void ami_blockcopyg(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2,
                    ami_long sy2, ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2)
    { begin(GR_MBLOCKCOPYG, wh(f)); pi(s); pi(d); pi(sx1); pi(sy1); pi(sx2);
      pi(sy2); pi(dx1); pi(dy1); pi(dx2); pi(dy2); send0(); }

/*******************************************************************************

The API: window management

*******************************************************************************/

void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)

{

    ami_long h, fd;

    h = nexth++;
    if (h >= MAXHND) error("Out of window handles");
#ifdef _WIN32
    fd = open("nul", O_WRONLY);
#else
    fd = open("/dev/null", O_WRONLY);
#endif
    if (fd < 0 || fd >= MAXFDS) error("Cannot open window file");
    fdh[fd] = h;
    h2lw[h] = wid;
    qc[h].valid = 0;
    *outfile = fdopen(fd, "w");
    if (!*outfile) error("Cannot open window file");
    setvbuf(*outfile, NULL, _IONBF, 0);
    if (infile && !*infile) *infile = stdin;
    begin(GR_MOPENWIN, 0);
    pi(parent? wh(parent): 0);
    pi(h);
    pi(wid);
    send0();

}

void ami_buffer(FILE* f, ami_long e)
    { ami_long h = wh(f); qc[h].valid &= ~QCSIZES;
      begin(GR_MBUFFER, h); pi(e); send0(); }
void ami_sizbufg(FILE* f, ami_long x, ami_long y)
    { ami_long h = wh(f); qc[h].valid &= ~QCSIZES;
      begin(GR_MSIZBUFG, h); pi(x); pi(y); send0(); }
void ami_getsiz(FILE* f, ami_long* x, ami_long* y)
    { begin(GR_MGETSIZ, wh(f)); qsend(); *x = gi(); *y = gi(); }
void ami_getsizg(FILE* f, ami_long* x, ami_long* y)
    { begin(GR_MGETSIZG, wh(f)); qsend(); *x = gi(); *y = gi(); }
void ami_setsiz(FILE* f, ami_long x, ami_long y)
    { ami_long h = wh(f); qc[h].valid &= ~QCSIZES;
      begin(GR_MSETSIZ, h); pi(x); pi(y); send0(); }
void ami_setsizg(FILE* f, ami_long x, ami_long y)
    { ami_long h = wh(f); qc[h].valid &= ~QCSIZES;
      begin(GR_MSETSIZG, h); pi(x); pi(y); send0(); }
void ami_setpos(FILE* f, ami_long x, ami_long y)
    { begin(GR_MSETPOS, wh(f)); pi(x); pi(y); send0(); }
void ami_setposg(FILE* f, ami_long x, ami_long y)
    { begin(GR_MSETPOSG, wh(f)); pi(x); pi(y); send0(); }
void ami_dragwin(FILE* f)
    { begin(GR_MDRAGWIN, wh(f)); send0(); }
void ami_scnsiz(FILE* f, ami_long* x, ami_long* y)
    { begin(GR_MSCNSIZ, wh(f)); qsend(); *x = gi(); *y = gi(); }
void ami_scnsizg(FILE* f, ami_long* x, ami_long* y)
    { begin(GR_MSCNSIZG, wh(f)); qsend(); *x = gi(); *y = gi(); }
void ami_scncen(FILE* f, ami_long* x, ami_long* y)
    { begin(GR_MSCNCEN, wh(f)); qsend(); *x = gi(); *y = gi(); }
void ami_scnceng(FILE* f, ami_long* x, ami_long* y)
    { begin(GR_MSCNCENG, wh(f)); qsend(); *x = gi(); *y = gi(); }
void ami_winclient(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy,
                   ami_winmodset ms)
    { begin(GR_MWINCLIENT, wh(f)); pi(cx); pi(cy); pi((ami_long)ms); qsend();
      *wx = gi(); *wy = gi(); }
void ami_winclientg(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy,
                    ami_winmodset ms)
    { begin(GR_MWINCLIENTG, wh(f)); pi(cx); pi(cy); pi((ami_long)ms); qsend();
      *wx = gi(); *wy = gi(); }
void ami_front(FILE* f)
    { begin(GR_MFRONT, wh(f)); send0(); }
void ami_back(FILE* f)
    { begin(GR_MBACK, wh(f)); send0(); }
void ami_frame(FILE* f, ami_long e)
    { begin(GR_MFRAME, wh(f)); pi(e); send0(); }
void ami_sizable(FILE* f, ami_long e)
    { begin(GR_MSIZABLE, wh(f)); pi(e); send0(); }
void ami_sysbar(FILE* f, ami_long e)
    { begin(GR_MSYSBAR, wh(f)); pi(e); send0(); }
void ami_menu(FILE* f, ami_menuptr m)
    { begin(GR_MMENU, wh(f)); pmenu(m); send0(); }
void ami_menuena(FILE* f, ami_long id, ami_long onoff)
    { begin(GR_MMENUENA, wh(f)); pi(id); pi(onoff); send0(); }
void ami_menusel(FILE* f, ami_long id, ami_long select)
    { begin(GR_MMENUSEL, wh(f)); pi(id); pi(select); send0(); }
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
    { begin(GR_MSTDMENU, 0); pi((ami_long)sms); pmenu(pm); qsend(); *sm = gmenu(); }
ami_long ami_getwinid(void)
    { begin(GR_MGETWINID, 0); qsend(); return (gi()); }
void ami_focus(FILE* f)
    { begin(GR_MFOCUS, wh(f)); send0(); }

/*******************************************************************************

The API: widgets

*******************************************************************************/

ami_long ami_getwigid(FILE* f)
    { begin(GR_MGETWIGID, wh(f)); qsend(); return (gi()); }
void ami_killwidget(FILE* f, ami_long id)
    { begin(GR_MKILLWIDGET, wh(f)); pi(id); send0(); }
void ami_selectwidget(FILE* f, ami_long id, ami_long e)
    { begin(GR_MSELECTWIDGET, wh(f)); pi(id); pi(e); send0(); }
void ami_enablewidget(FILE* f, ami_long id, ami_long e)
    { begin(GR_MENABLEWIDGET, wh(f)); pi(id); pi(e); send0(); }
void ami_getwidgettext(FILE* f, ami_long id, char* s, ami_long sl)
    { begin(GR_MGETWIDGETTEXT, wh(f)); pi(id); qsend(); gs(s, sl); }
void ami_putwidgettext(FILE* f, ami_long id, char* s)
    { begin(GR_MPUTWIDGETTEXT, wh(f)); pi(id); ps(s); send0(); }
void ami_sizwidget(FILE* f, ami_long id, ami_long x, ami_long y)
    { begin(GR_MSIZWIDGET, wh(f)); pi(id); pi(x); pi(y); send0(); }
void ami_sizwidgetg(FILE* f, ami_long id, ami_long x, ami_long y)
    { begin(GR_MSIZWIDGETG, wh(f)); pi(id); pi(x); pi(y); send0(); }
void ami_poswidget(FILE* f, ami_long id, ami_long x, ami_long y)
    { begin(GR_MPOSWIDGET, wh(f)); pi(id); pi(x); pi(y); send0(); }
void ami_poswidgetg(FILE* f, ami_long id, ami_long x, ami_long y)
    { begin(GR_MPOSWIDGETG, wh(f)); pi(id); pi(x); pi(y); send0(); }
void ami_backwidget(FILE* f, ami_long id)
    { begin(GR_MBACKWIDGET, wh(f)); pi(id); send0(); }
void ami_frontwidget(FILE* f, ami_long id)
    { begin(GR_MFRONTWIDGET, wh(f)); pi(id); send0(); }
void ami_focuswidget(FILE* f, ami_long id)
    { begin(GR_MFOCUSWIDGET, wh(f)); pi(id); send0(); }
void ami_buttonsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MBUTTONSIZ, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_buttonsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MBUTTONSIZG, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_button(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { begin(GR_MBUTTON, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s); pi(id);
      send0(); }
void ami_buttong(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { begin(GR_MBUTTONG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s); pi(id);
      send0(); }
void ami_checkboxsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MCHECKBOXSIZ, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_checkboxsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MCHECKBOXSIZG, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_checkbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { begin(GR_MCHECKBOX, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s); pi(id);
      send0(); }
void ami_checkboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s,
                   ami_long id)
    { begin(GR_MCHECKBOXG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s);
      pi(id); send0(); }
void ami_radiobuttonsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MRADIOBUTTONSIZ, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_radiobuttonsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MRADIOBUTTONSIZG, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_radiobutton(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s,
                     ami_long id)
    { begin(GR_MRADIOBUTTON, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s);
      pi(id); send0(); }
void ami_radiobuttong(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s,
                      ami_long id)
    { begin(GR_MRADIOBUTTONG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s);
      pi(id); send0(); }
void ami_groupsizg(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h,
                   ami_long* ox, ami_long* oy)
    { begin(GR_MGROUPSIZG, wh(f)); ps(s); pi(cw); pi(ch); qsend();
      *w = gi(); *h = gi(); *ox = gi(); *oy = gi(); }
void ami_groupsiz(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h,
                  ami_long* ox, ami_long* oy)
    { begin(GR_MGROUPSIZ, wh(f)); ps(s); pi(cw); pi(ch); qsend();
      *w = gi(); *h = gi(); *ox = gi(); *oy = gi(); }
void ami_group(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { begin(GR_MGROUP, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s); pi(id);
      send0(); }
void ami_groupg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
    { begin(GR_MGROUPG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); ps(s); pi(id);
      send0(); }
void ami_background(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MBACKGROUND, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_backgroundg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MBACKGROUNDG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_scrollvertsizg(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSCROLLVERTSIZG, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_scrollvertsiz(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSCROLLVERTSIZ, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_scrollvert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MSCROLLVERT, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_scrollvertg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MSCROLLVERTG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_scrollhorizsizg(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSCROLLHORIZSIZG, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_scrollhorizsiz(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSCROLLHORIZSIZ, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_scrollhoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MSCROLLHORIZ, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_scrollhorizg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MSCROLLHORIZG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_scrollpos(FILE* f, ami_long id, ami_long r)
    { begin(GR_MSCROLLPOS, wh(f)); pi(id); pi(r); send0(); }
void ami_scrollsiz(FILE* f, ami_long id, ami_long r)
    { begin(GR_MSCROLLSIZ, wh(f)); pi(id); pi(r); send0(); }
void ami_numselboxsizg(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)
    { begin(GR_MNUMSELBOXSIZG, wh(f)); pi(l); pi(u); qsend();
      *w = gi(); *h = gi(); }
void ami_numselboxsiz(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)
    { begin(GR_MNUMSELBOXSIZ, wh(f)); pi(l); pi(u); qsend();
      *w = gi(); *h = gi(); }
void ami_numselbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u,
                   ami_long id)
    { begin(GR_MNUMSELBOX, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(l);
      pi(u); pi(id); send0(); }
void ami_numselboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u,
                    ami_long id)
    { begin(GR_MNUMSELBOXG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(l);
      pi(u); pi(id); send0(); }
void ami_editboxsizg(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MEDITBOXSIZG, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_editboxsiz(FILE* f, char* s, ami_long* w, ami_long* h)
    { begin(GR_MEDITBOXSIZ, wh(f)); ps(s); qsend(); *w = gi(); *h = gi(); }
void ami_editbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MEDITBOX, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_editboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MEDITBOXG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_progbarsizg(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MPROGBARSIZG, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_progbarsiz(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MPROGBARSIZ, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_progbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MPROGBAR, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_progbarg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
    { begin(GR_MPROGBARG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(id);
      send0(); }
void ami_progbarpos(FILE* f, ami_long id, ami_long pos)
    { begin(GR_MPROGBARPOS, wh(f)); pi(id); pi(pos); send0(); }
void ami_listboxsizg(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
    { begin(GR_MLISTBOXSIZG, wh(f)); pslst(sp); qsend(); *w = gi(); *h = gi(); }
void ami_listboxsiz(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
    { begin(GR_MLISTBOXSIZ, wh(f)); pslst(sp); qsend(); *w = gi(); *h = gi(); }
void ami_listbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                 ami_long id)
    { begin(GR_MLISTBOX, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pslst(sp);
      pi(id); send0(); }
void ami_listboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                  ami_long id)
    { begin(GR_MLISTBOXG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pslst(sp);
      pi(id); send0(); }
void ami_dropboxsizg(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow,
                     ami_long* oh)
    { begin(GR_MDROPBOXSIZG, wh(f)); pslst(sp); qsend();
      *cw = gi(); *ch = gi(); *ow = gi(); *oh = gi(); }
void ami_dropboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow,
                    ami_long* oh)
    { begin(GR_MDROPBOXSIZ, wh(f)); pslst(sp); qsend();
      *cw = gi(); *ch = gi(); *ow = gi(); *oh = gi(); }
void ami_dropbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                 ami_long id)
    { begin(GR_MDROPBOX, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pslst(sp);
      pi(id); send0(); }
void ami_dropboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                  ami_long id)
    { begin(GR_MDROPBOXG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pslst(sp);
      pi(id); send0(); }
void ami_dropeditboxsizg(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow,
                         ami_long* oh)
    { begin(GR_MDROPEDITBOXSIZG, wh(f)); pslst(sp); qsend();
      *cw = gi(); *ch = gi(); *ow = gi(); *oh = gi(); }
void ami_dropeditboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow,
                        ami_long* oh)
    { begin(GR_MDROPEDITBOXSIZ, wh(f)); pslst(sp); qsend();
      *cw = gi(); *ch = gi(); *ow = gi(); *oh = gi(); }
void ami_dropeditbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2,
                     ami_strptr sp, ami_long id)
    { begin(GR_MDROPEDITBOX, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pslst(sp);
      pi(id); send0(); }
void ami_dropeditboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2,
                      ami_strptr sp, ami_long id)
    { begin(GR_MDROPEDITBOXG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2);
      pslst(sp); pi(id); send0(); }
void ami_slidehorizsizg(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSLIDEHORIZSIZG, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_slidehorizsiz(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSLIDEHORIZSIZ, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_slidehoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
                    ami_long id)
    { begin(GR_MSLIDEHORIZ, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(mark);
      pi(id); send0(); }
void ami_slidehorizg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
                     ami_long id)
    { begin(GR_MSLIDEHORIZG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(mark);
      pi(id); send0(); }
void ami_slidevertsizg(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSLIDEVERTSIZG, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_slidevertsiz(FILE* f, ami_long* w, ami_long* h)
    { begin(GR_MSLIDEVERTSIZ, wh(f)); qsend(); *w = gi(); *h = gi(); }
void ami_slidevert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
                   ami_long id)
    { begin(GR_MSLIDEVERT, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(mark);
      pi(id); send0(); }
void ami_slidevertg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark,
                    ami_long id)
    { begin(GR_MSLIDEVERTG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pi(mark);
      pi(id); send0(); }
void ami_tabbarsizg(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w,
                    ami_long* h, ami_long* ox, ami_long* oy)
    { begin(GR_MTABBARSIZG, wh(f)); pslst(sp); pi((ami_long)tor); pi(cw); pi(ch); qsend();
      *w = gi(); *h = gi(); *ox = gi(); *oy = gi(); }
void ami_tabbarsiz(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h,
                   ami_long* ox, ami_long* oy)
    { begin(GR_MTABBARSIZ, wh(f)); pslst(sp); pi((ami_long)tor); pi(cw); pi(ch); qsend();
      *w = gi(); *h = gi(); *ox = gi(); *oy = gi(); }
void ami_tabbarclientg(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw,
                       ami_long* ch, ami_long* ox, ami_long* oy)
    { begin(GR_MTABBARCLIENTG, wh(f)); pi((ami_long)tor); pi(w); pi(h); qsend();
      *cw = gi(); *ch = gi(); *ox = gi(); *oy = gi(); }
void ami_tabbarclient(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw,
                      ami_long* ch, ami_long* ox, ami_long* oy)
    { begin(GR_MTABBARCLIENT, wh(f)); pi((ami_long)tor); pi(w); pi(h); qsend();
      *cw = gi(); *ch = gi(); *ox = gi(); *oy = gi(); }
void ami_tabbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                ami_tabori tor, ami_long id)
    { begin(GR_MTABBAR, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pslst(sp);
      pi((ami_long)tor); pi(id); send0(); }
void ami_tabbarg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp,
                 ami_tabori tor, ami_long id)
    { begin(GR_MTABBARG, wh(f)); pi(x1); pi(y1); pi(x2); pi(y2); pslst(sp);
      pi((ami_long)tor); pi(id); send0(); }
void ami_tabsel(FILE* f, ami_long id, ami_long tn)
    { begin(GR_MTABSEL, wh(f)); pi(id); pi(tn); send0(); }

/*******************************************************************************

The API: dialogs

*******************************************************************************/

void ami_alert(char* title, char* message)
    { begin(GR_MALERT, 0); ps(title); ps(message); qsend(); }
      /* the reply comes on dismissal: alert is modal by contract */
void ami_querycolor(ami_long* r, ami_long* g, ami_long* b)
    { begin(GR_MQUERYCOLOR, 0); pi(*r); pi(*g); pi(*b); qsend();
      *r = gi(); *g = gi(); *b = gi(); }
void ami_queryopen(char* s, ami_long sl)
    { begin(GR_MQUERYOPEN, 0); ps(s); qsend(); gs(s, sl); }
void ami_querysave(char* s, ami_long sl)
    { begin(GR_MQUERYSAVE, 0); ps(s); qsend(); gs(s, sl); }
void ami_queryfind(char* s, ami_long sl, ami_qfnopts* opt)
    { begin(GR_MQUERYFIND, 0); ps(s); pi((ami_long)*opt); qsend(); gs(s, sl);
      *opt = (ami_qfnopts)gi(); }
void ami_queryfindrep(char* s, ami_long sl, char* r, ami_long rl, ami_qfropts* opt)
    { begin(GR_MQUERYFINDREP, 0); ps(s); ps(r); pi((ami_long)*opt); qsend();
      gs(s, sl); gs(r, rl); *opt = (ami_qfropts)gi(); }
void ami_queryfont(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb,
                   ami_long* br, ami_long* bg, ami_long* bb, ami_qfteffects* effect)
    { begin(GR_MQUERYFONT, wh(f)); pi(*fc); pi(*s); pi(*fr); pi(*fg); pi(*fb);
      pi(*br); pi(*bg); pi(*bb); pi((ami_long)*effect); qsend();
      *fc = gi(); *s = gi(); *fr = gi(); *fg = gi(); *fb = gi();
      *br = gi(); *bg = gi(); *bb = gi(); *effect = (ami_qfteffects)gi(); }

/*******************************************************************************

The sound API

The full sound call set of sound.h, carried in the sound partition of
the message space. There is no window: every message goes with wid 0.
The devices, synthesizers, wave channels, their inputs, live at the
server, which is where the speakers and the microphones are. Sequencer
times are interpreted against the server's time bases, and the time
queries return them, so client-computed schedules land where they
should. The input calls block the caller until the far device delivers,
exactly as the native calls do. The files of loadsynth() and loadwave()
live where the program lives and travel by the file transfer mechanism,
which is why those calls are queries.

*******************************************************************************/

/* the channel voice pattern: port, time, channel, and one value */
static void sndc1(int mid, ami_long p, ami_long t, ami_long c, ami_long v)

{

    begin(mid, 0);
    pi(p); pi(t); pi(c); pi(v);
    send0();

}

void ami_starttimeout(void) { begin(GR_MSTARTTIMEOUT, 0); send0(); }
void ami_stoptimeout(void) { begin(GR_MSTOPTIMEOUT, 0); send0(); }
ami_long ami_curtimeout(void)
    { begin(GR_MCURTIMEOUT, 0); qsend(); return (gi()); }
void ami_starttimein(void) { begin(GR_MSTARTTIMEIN, 0); send0(); }
void ami_stoptimein(void) { begin(GR_MSTOPTIMEIN, 0); send0(); }
ami_long ami_curtimein(void)
    { begin(GR_MCURTIMEIN, 0); qsend(); return (gi()); }
ami_long ami_synthout(void)
    { begin(GR_MSYNTHOUT, 0); qsend(); return (gi()); }
ami_long ami_synthin(void)
    { begin(GR_MSYNTHIN, 0); qsend(); return (gi()); }
void ami_opensynthout(ami_long p)
    { begin(GR_MOPENSYNTHOUT, 0); pi(p); send0(); }
void ami_closesynthout(ami_long p)
    { begin(GR_MCLOSESYNTHOUT, 0); pi(p); send0(); }
void ami_opensynthin(ami_long p)
    { begin(GR_MOPENSYNTHIN, 0); pi(p); send0(); }
void ami_closesynthin(ami_long p)
    { begin(GR_MCLOSESYNTHIN, 0); pi(p); send0(); }
void ami_noteon(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long v)
    { begin(GR_MNOTEON, 0); pi(p); pi(t); pi(c); pi(n); pi(v); send0(); }
void ami_noteoff(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long v)
    { begin(GR_MNOTEOFF, 0); pi(p); pi(t); pi(c); pi(n); pi(v); send0(); }
void ami_instchange(ami_long p, ami_long t, ami_channel c, ami_instrument i)
    { sndc1(GR_MINSTCHANGE, p, t, c, i); }
void ami_attack(ami_long p, ami_long t, ami_channel c, ami_long at)
    { sndc1(GR_MATTACK, p, t, c, at); }
void ami_release(ami_long p, ami_long t, ami_channel c, ami_long rt)
    { sndc1(GR_MRELEASE, p, t, c, rt); }
void ami_legato(ami_long p, ami_long t, ami_channel c, ami_long b)
    { sndc1(GR_MLEGATO, p, t, c, b); }
void ami_portamento(ami_long p, ami_long t, ami_channel c, ami_long b)
    { sndc1(GR_MPORTAMENTO, p, t, c, b); }
void ami_vibrato(ami_long p, ami_long t, ami_channel c, ami_long v)
    { sndc1(GR_MVIBRATO, p, t, c, v); }
void ami_volsynthchan(ami_long p, ami_long t, ami_channel c, ami_long v)
    { sndc1(GR_MVOLSYNTHCHAN, p, t, c, v); }
void ami_porttime(ami_long p, ami_long t, ami_channel c, ami_long v)
    { sndc1(GR_MPORTTIME, p, t, c, v); }
void ami_balance(ami_long p, ami_long t, ami_channel c, ami_long b)
    { sndc1(GR_MBALANCE, p, t, c, b); }
void ami_pan(ami_long p, ami_long t, ami_channel c, ami_long b)
    { sndc1(GR_MPAN, p, t, c, b); }
void ami_timbre(ami_long p, ami_long t, ami_channel c, ami_long tb)
    { sndc1(GR_MTIMBRE, p, t, c, tb); }
void ami_brightness(ami_long p, ami_long t, ami_channel c, ami_long b)
    { sndc1(GR_MBRIGHTNESS, p, t, c, b); }
void ami_reverb(ami_long p, ami_long t, ami_channel c, ami_long r)
    { sndc1(GR_MREVERB, p, t, c, r); }
void ami_tremulo(ami_long p, ami_long t, ami_channel c, ami_long tr)
    { sndc1(GR_MTREMULO, p, t, c, tr); }
void ami_chorus(ami_long p, ami_long t, ami_channel c, ami_long cr)
    { sndc1(GR_MCHORUS, p, t, c, cr); }
void ami_celeste(ami_long p, ami_long t, ami_channel c, ami_long ce)
    { sndc1(GR_MCELESTE, p, t, c, ce); }
void ami_phaser(ami_long p, ami_long t, ami_channel c, ami_long ph)
    { sndc1(GR_MPHASER, p, t, c, ph); }
void ami_aftertouch(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long at)
    { begin(GR_MAFTERTOUCH, 0); pi(p); pi(t); pi(c); pi(n); pi(at); send0(); }
void ami_pressure(ami_long p, ami_long t, ami_channel c, ami_long pr)
    { sndc1(GR_MPRESSURE, p, t, c, pr); }
void ami_pitch(ami_long p, ami_long t, ami_channel c, ami_long pt)
    { sndc1(GR_MPITCH, p, t, c, pt); }
void ami_pitchrange(ami_long p, ami_long t, ami_channel c, ami_long v)
    { sndc1(GR_MPITCHRANGE, p, t, c, v); }
void ami_mono(ami_long p, ami_long t, ami_channel c, ami_long ch)
    { sndc1(GR_MMONO, p, t, c, ch); }
void ami_poly(ami_long p, ami_long t, ami_channel c)
    { begin(GR_MPOLY, 0); pi(p); pi(t); pi(c); send0(); }
void ami_loadsynth(ami_long s, string sf)
    { begin(GR_MLOADSYNTH, 0); pi(s); ps(sf); qsend(); gi(); }
void ami_playsynth(ami_long p, ami_long t, ami_long s)
    { begin(GR_MPLAYSYNTH, 0); pi(p); pi(t); pi(s); send0(); }
void ami_delsynth(ami_long s) { begin(GR_MDELSYNTH, 0); pi(s); send0(); }
void ami_waitsynth(ami_long p)
    { begin(GR_MWAITSYNTH, 0); pi(p); qsend(); gi(); }

void ami_wrsynth(ami_long p, ami_seqptr sp)

{

    ami_long u[3];

    begin(GR_MWRSYNTH, 0);
    pi(p);
    pi(sp->port);
    pi(sp->time);
    pi((ami_long)sp->st);
    /* the union rides as its three widest words */
    memcpy(u, &sp->ntc, sizeof(u));
    pi(u[0]); pi(u[1]); pi(u[2]);
    send0();

}

void ami_rdsynth(ami_long p, ami_seqptr sp)

{

    ami_long u[3];

    begin(GR_MRDSYNTH, 0);
    pi(p);
    qsend(); /* blocks until the far device delivers */
    sp->next = NULL;
    sp->port = gi();
    sp->time = gi();
    sp->st = (ami_seqtyp)gi();
    u[0] = gi(); u[1] = gi(); u[2] = gi();
    memcpy(&sp->ntc, u, sizeof(u));

}

ami_long ami_waveout(void)
    { begin(GR_MWAVEOUT, 0); qsend(); return (gi()); }
ami_long ami_wavein(void)
    { begin(GR_MWAVEIN, 0); qsend(); return (gi()); }
void ami_openwaveout(ami_long p)
    { begin(GR_MOPENWAVEOUT, 0); pi(p); send0(); }
void ami_closewaveout(ami_long p)
    { begin(GR_MCLOSEWAVEOUT, 0); pi(p); send0(); }
void ami_loadwave(ami_long w, string fn)
    { begin(GR_MLOADWAVE, 0); pi(w); ps(fn); qsend(); gi(); }
void ami_playwave(ami_long p, ami_long t, ami_long w)
    { begin(GR_MPLAYWAVE, 0); pi(p); pi(t); pi(w); send0(); }
void ami_delwave(ami_long w) { begin(GR_MDELWAVE, 0); pi(w); send0(); }
void ami_volwave(ami_long p, ami_long t, ami_long v)
    { begin(GR_MVOLWAVE, 0); pi(p); pi(t); pi(v); send0(); }
void ami_waitwave(ami_long p)
    { begin(GR_MWAITWAVE, 0); pi(p); qsend(); gi(); }
/* The frame size of an output port, channels times sample bytes, is
   tracked from the parameter calls so the raw stream can chunk on
   frame boundaries. A raw stream requires the parameters set first,
   which is also what the device needs to interpret the samples. */
#define MAXWAVP 100 /* maximum wave ports, matching the library */
static ami_long wochan[MAXWAVP]; /* channels set per output port */
static ami_long wobits[MAXWAVP]; /* sample bits set per output port */

void ami_chanwaveout(ami_long p, ami_long c)
    { if (p >= 1 && p <= MAXWAVP) wochan[p-1] = c;
      begin(GR_MCHANWAVEOUT, 0); pi(p); pi(c); send0(); }
void ami_ratewaveout(ami_long p, ami_long r)
    { begin(GR_MRATEWAVEOUT, 0); pi(p); pi(r); send0(); }
void ami_lenwaveout(ami_long p, ami_long l)
    { if (p >= 1 && p <= MAXWAVP) wobits[p-1] = l;
      begin(GR_MLENWAVEOUT, 0); pi(p); pi(l); send0(); }
void ami_sgnwaveout(ami_long p, ami_long s)
    { begin(GR_MSGNWAVEOUT, 0); pi(p); pi(s); send0(); }
void ami_fltwaveout(ami_long p, ami_long f)
    { begin(GR_MFLTWAVEOUT, 0); pi(p); pi(f); send0(); }
void ami_endwaveout(ami_long p, ami_long e)
    { begin(GR_MENDWAVEOUT, 0); pi(p); pi(e); send0(); }

void ami_wrwave(ami_long p, byte* buff, ami_long len)

{

    ami_long fsiz;  /* frame size in bytes */
    ami_long chunk; /* frames per message */
    ami_long n;

    /* Lengths are in samples, one frame of every channel; the byte
       stream chunks to the channel bound on frame boundaries. The
       frame size comes from the parameters the program set, which a
       raw stream must set for the device to make sense of it anyway. */
    if (p < 1 || p > MAXWAVP) error("Invalid wave port");
    if (!wochan[p-1] || !wobits[p-1])
        error("Remote wrwave requires channels and bits set first");
    fsiz = wochan[p-1]*((wobits[p-1]+7)/8);
    chunk = (msgmax-96)/fsiz;
    while (len > 0) {

        n = len > chunk? chunk: len;
        begin(GR_MWRWAVE, 0);
        pi(p);
        pi(n);
        psn((char*)buff, n*fsiz);
        qsend(); /* the ack paces the stream and keeps it whole */
        gi();
        buff += n*fsiz;
        len -= n;

    }

}

void ami_openwavein(ami_long p)
    { begin(GR_MOPENWAVEIN, 0); pi(p); send0(); }
void ami_closewavein(ami_long p)
    { begin(GR_MCLOSEWAVEIN, 0); pi(p); send0(); }
ami_long ami_chanwavein(ami_long p)
    { begin(GR_MCHANWAVEIN, 0); pi(p); qsend(); return (gi()); }
ami_long ami_ratewavein(ami_long p)
    { begin(GR_MRATEWAVEIN, 0); pi(p); qsend(); return (gi()); }
ami_long ami_lenwavein(ami_long p)
    { begin(GR_MLENWAVEIN, 0); pi(p); qsend(); return (gi()); }
ami_long ami_sgnwavein(ami_long p)
    { begin(GR_MSGNWAVEIN, 0); pi(p); qsend(); return (gi()); }
ami_long ami_endwavein(ami_long p)
    { begin(GR_MENDWAVEIN, 0); pi(p); qsend(); return (gi()); }
ami_long ami_fltwavein(ami_long p)
    { begin(GR_MFLTWAVEIN, 0); pi(p); qsend(); return (gi()); }

ami_long ami_rdwave(ami_long p, byte* buff, ami_long len)

{

    ami_long total = 0;
    ami_long got, dl;

    /* Lengths are in samples, one frame of every channel. The server
       clamps each answer to the channel bound and this loops until the
       count is satisfied; each round blocks until the far device
       delivers, exactly as the native call does. */
    while (len > 0) {

        begin(GR_MRDWAVE, 0);
        pi(p);
        pi(len);
        qsend();
        got = gi(); /* frames delivered */
        dl = g4(); /* the block length in bytes */
        memcpy(buff, rbuf+roff, dl);
        roff += dl;
        buff += dl;
        total += got;
        len -= got;

    }

    return (total);

}

void ami_synthoutname(ami_long p, string name, ami_long len)
    { begin(GR_MSYNTHOUTNAME, 0); pi(p); qsend(); gs(name, len); }
void ami_synthinname(ami_long p, string name, ami_long len)
    { begin(GR_MSYNTHINNAME, 0); pi(p); qsend(); gs(name, len); }
void ami_waveoutname(ami_long p, string name, ami_long len)
    { begin(GR_MWAVEOUTNAME, 0); pi(p); qsend(); gs(name, len); }
void ami_waveinname(ami_long p, string name, ami_long len)
    { begin(GR_MWAVEINNAME, 0); pi(p); qsend(); gs(name, len); }
ami_long ami_setparamsynthin(ami_long p, string name, string value)
    { begin(GR_MSETPARAMSYNTHIN, 0); pi(p); ps(name); ps(value); qsend();
      return (gi()); }
ami_long ami_setparamsynthout(ami_long p, string name, string value)
    { begin(GR_MSETPARAMSYNTHOUT, 0); pi(p); ps(name); ps(value); qsend();
      return (gi()); }
ami_long ami_setparamwavein(ami_long p, string name, string value)
    { begin(GR_MSETPARAMWAVEIN, 0); pi(p); ps(name); ps(value); qsend();
      return (gi()); }
ami_long ami_setparamwaveout(ami_long p, string name, string value)
    { begin(GR_MSETPARAMWAVEOUT, 0); pi(p); ps(name); ps(value); qsend();
      return (gi()); }
void ami_getparamsynthin(ami_long p, string name, string value, ami_long len)
    { begin(GR_MGETPARAMSYNTHIN, 0); pi(p); ps(name); qsend();
      gs(value, len); }
void ami_getparamsynthout(ami_long p, string name, string value, ami_long len)
    { begin(GR_MGETPARAMSYNTHOUT, 0); pi(p); ps(name); qsend();
      gs(value, len); }
void ami_getparamwavein(ami_long p, string name, string value, ami_long len)
    { begin(GR_MGETPARAMWAVEIN, 0); pi(p); ps(name); qsend();
      gs(value, len); }
void ami_getparamwaveout(ami_long p, string name, string value, ami_long len)
    { begin(GR_MGETPARAMWAVEOUT, 0); pi(p); ps(name); qsend();
      gs(value, len); }

/*******************************************************************************

Initialize and deinitialize

The connection is made at program start: resolve the server, open the two
channels, exchange the hello. The main window is handle 1 from the start.

*******************************************************************************/

/* The program's name as the native library would title its window: the
   executable's base name, without the extension on Windows. The system
   knows which file is running, whatever the command line spelled. */
static void pgmname(char* dst, ami_long dl)

{

    char  path[1024];
    char* s;
    char* b;

    path[0] = 0;
#ifdef _WIN32
    if (!GetModuleFileNameA(NULL, path, sizeof(path)-1)) path[0] = 0;
    path[sizeof(path)-1] = 0;
#elif defined(__APPLE__)
    {

        uint32_t l = sizeof(path);

        if (_NSGetExecutablePath(path, &l)) path[0] = 0;

    }
#else
    {

        ssize_t l = readlink("/proc/self/exe", path, sizeof(path)-1);

        if (l > 0) path[l] = 0; else path[0] = 0;

    }
#endif
    b = path;
    for (s = path; *s; s++) if (*s == '/' || *s == '\\') b = s+1;
    snprintf(dst, dl, "%s", b);
#ifdef _WIN32
    s = strrchr(dst, '.');
    if (s && s != dst) *s = 0;
#endif

}

static void ami_init_graph_client(void) __attribute__((constructor (105)));
static void ami_init_graph_client(void)

{

    const char* sa;
    const char* sp;
    ami_long    v;

    sa = getenv("GRAPH_SERVER");
    if (!sa || !sa[0]) sa = "127.0.0.1";
    sp = getenv("GRAPH_PORT");
    srvport = sp && sp[0]? atol(sp): GR_DEFPORT;
    gtrace = getenv("GRAPH_TRACE") != NULL;
    /* Secure channels are the default, as they are in the server, and
       GRAPH_PLAIN is the way out of them. GRAPH_SECURE is still honoured
       and now says only what is already so. */
    gsecure = getenv("GRAPH_PLAIN") == NULL;
    ami_addrnet((char*)sa, &srvaddr);
    msgmax = ami_maxmsg(srvaddr, gsecure);
    if (msgmax < 1024) error("Message channel too small");
    sbuf = malloc(msgmax);
    rbuf = malloc(msgmax);
    if (!sbuf || !rbuf) error("Out of memory");

    ovr_read(iread, &dn_read);
    ovr_write(iwrite, &dn_write);
    ovr_close(iclose, &dn_close);

    cmdfn = ami_openmsg(srvaddr, srvport, gsecure);
    persistent(cmdfn);
    /* The event channel opens before the hello: under the secure
       channel the server accepts each channel's handshake in turn
       before it can answer anything, so both handshakes must complete
       before the first exchange. In the clear the order is
       indifferent. */
    evtfn = ami_openmsg(srvaddr, srvport+1, gsecure);
    persistent(evtfn);
    /* the main window exists from the hello */
    fdh[1] = 1;
    h2lw[1] = 1;
    connected = 1;
    /* The hello, with a bounded wait of our own: a missing server should
       say so, not hang or die in the transport. The channel descriptor
       takes a timeout for just this exchange. */
    {

        ssize_t r = -1;
        int     try;
        char    pgmnam[128];

        /* The hello carries the program's name: the server titles the
           program's window with it, as the native library titles its own
           window, and marks the held window with it at the end. */
        pgmname(pgmnam, sizeof(pgmnam));
        /* A datagram handshake retries: the hello is idempotent. The
           bound is by the ready test and the read by the message call, which
           under the secure channel is also what decrypts: a raw recv
           on a DTLS channel sees only ciphertext. */
        for (try = 0; try < 3 && r < (ami_long)sizeof(gr_msghdr); try++) {

            begin(GR_MHELLO, 0);
            pi(GR_VERSION);
            ps(pgmnam);
            sseq++;
            shdr()->seq = sseq;
            send0();
            if (ami_rdymsg(cmdfn, 2*1000000L))
                r = ami_rdmsg(cmdfn, rbuf, msgmax);

        }
        if (r < (ami_long)sizeof(gr_msghdr)) {

            static char es[128];

            snprintf(es, sizeof(es),
                     "No display server at %s port %lld", sa, AMI_LONG_CAST(srvport));
            error(es);

        }
        persistent(cmdfn);
        rlen = r;
        if (rhdr()->mid != GR_MREPLY || rhdr()->seq != sseq)
            error("Protocol failure: bad hello reply");
        roff = sizeof(gr_msghdr);

    }
    v = gi();
    if (v != GR_VERSION) error("Server protocol version mismatch");
    /* the event channel, and its hello so the server knows where events
       go */
    shdr()->mid = GR_MEVOPEN;
    shdr()->seq = 0;
    shdr()->wid = 0;
    soff = sizeof(gr_msghdr);
    pi(GR_VERSION);
    shdr()->len = (int)soff;
    ami_wrmsg(evtfn, sbuf, soff);
    /* the receiver drains the event channel into the queue from here on.
       The console interrupt signals block everywhere and wait on their
       own thread, becoming the terminate event. */
    evqsiz = 256;
    evq = malloc(evqsiz*sizeof(ami_evtrec));
    if (!evq) error("Out of memory");
#ifndef _WIN32
    {

        sigset_t  set;
        pthread_t st;

        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &set, NULL);
        /* a background launch inherits ignore, which discards even
           blocked signals; the default disposition keeps them pending
           for the sigwait thread */
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        pthread_create(&st, NULL, sigrun, NULL);

    }
#else
    signal(SIGINT, winsig);
    signal(SIGTERM, winsig);
#endif
    if (pthread_create(&evthrd, NULL, evrun, NULL))
        error("Cannot start event receiver");
    /* the command batch comes up last: until then send0 is direct */
    batmax = msgmax < BATLIM? msgmax: BATLIM;
    batbuf = malloc(batmax);
    if (!batbuf) error("Out of memory");
    {

        pthread_t bt;

        if (pthread_create(&bt, NULL, batrun, NULL))
            error("Cannot start batch flusher");

    }

}

static void ami_deinit_graph_client(void) __attribute__((destructor (105)));
static void ami_deinit_graph_client(void)

{

    pread_t  cpread;
    pwrite_t cpwrite;
    pclose_t cpclose;

    if (connected) {

        begin(GR_MBYE, 0);
        send0();
        flushcmd(); /* the bye is a boundary */
        connected = 0;
        /* stop the receiver before its channel goes away. The session may
           be dying half made, an error before the event side came up, so
           each piece takes down only if it exists. */
        if (evq) {

            /* The receiver sees the stop at its next wait, within the poll
               bound, and is joined. Ending it outright, as before, left
               whatever lock it held, the heap's at times, held forever,
               and the exit hung on it. */
            evstop = 1;
            /* unless this is the receiver itself, exiting on an error of
               its own: it cannot wait for itself */
            if (!pthread_equal(pthread_self(), evthrd))
                pthread_join(evthrd, NULL);

        }
        ami_clsmsg(cmdfn);
        if (evtfn >= 0) ami_clsmsg(evtfn);

    }
    /* swap the system vectors back; if we don't come off the top of the
       chain the stacking is corrupt */
    ovr_read(dn_read, &cpread);
    ovr_write(dn_write, &cpwrite);
    ovr_close(dn_close, &cpclose);
    if (cpread != iread || cpwrite != iwrite || cpclose != iclose)
        error("System consistency check");

}
