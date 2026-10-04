/** ****************************************************************************
*                                                                              *
*                          SYSTEM EVENT HANDLER                                *
*                                                                              *
* System event handler for MAC OS X/BSD.                                       *
*                                                                              *
* Contains an interface to the system event handler. This is an abstraction    *
* for having the system be able to handle multiple event types and return a    *
* logical event for each one. It is used by terminal and graphical terminal    *
* Petit-Ami modules to abstract the differences between linux, which uses      *
* select() and pselect() for this purpose, and BSD/Mac OS X, which uses        *
* kqueues for this purpose.                                                    *
*                                                                              *
* It implements the following system event types:                              *
*                                                                              *
* input         An input file, has data to read.                               *
* signal        The OS has issued a signal.                                    *
* timer         A timer has fired.                                             *
*                                                                              *
* The client registers each event that will be included. In the case of        *
* timers, the API controls the timer period and repeat, and gives a call to    *
* cancel an active timer.                                                      *
*                                                                              *
* Calls in this package:                                                       *
*                                                                              *
* int system_event_addseinp(int fid);                                          *
*                                                                              *
* void system_event_deaseinp(int sid);                                         *
*                                                                              *
* Deactivates the input monitor with the given system event number. Used when  *
* the underlying device goes away, such as a joystick disconnect.              *
*                                                                              *
* Registers an input file to be monitored for input ready, or one or more      *
* bytes ready to read. The logical system event number is returned.            *
*                                                                              *
* int system_event_addsesig(int sig);                                          *
*                                                                              *
* Registers a signal to be monitored. A handler is also registered, and sends  *
* an event when the signal occurs. The logical system event number is          *
* returned.                                                                    *
*                                                                              *
* int system_event_addsetim(int sid, ami_long t, ami_long r);                  *
*                                                                              *
* Activates a timer with the given time and repeat state. An event occurs when *
* the timer fires, and may repeat. Both accepts a logical system event number  *
* and returns one. If the input event is 0, a new event is created.            *
*                                                                              *
* void system_event_deasetim(int sid);                                         *
*                                                                              *
* Deactivates the timer with the given system event number regardless of the   *
* state of the timer.                                                          *
*                                                                              *
* void system_event_getsevt(sevptr ev);                                        *
*                                                                              *
* Returns the next active system event. The event is returned in the record:   *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <sys/select.h>
#include <sys/event.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>

/* somewhat kludgy, but there should be only one include file for this */
#include "../linux/system_event.h"

#include <localdefs.h> /* ami_long and friends */
#include <diag.h>

//#define PRTSEVT /* print signals diagnostic */
#define MAXSYS 100 /* number of possible logical system events */

/* logical system event record */
typedef struct systrk* sevtptr; /* pointer to system event */
typedef struct systrk {

    sevttyp typ; /* system event type */
    int     fid; /* logical file id if used */
    int     sig; /* signal number */
    int     ei;  /* event index */
    int     rep; /* timer repeats (timer type only) */

} systrk;

/*
 * Lock for module data
 */
static pthread_mutex_t evtlock; /* lock for this module data */

/* logical system event tracking array */
static sevtptr systab[MAXSYS];
static int sysno; /* number of system event ids allocated */

static sigset_t sigmsk; /* signal mask */

static struct kevent chgevt[MAXSYS];  /* event change list */
static struct kevent chgevtc[MAXSYS]; /* event change list copy */
static struct kevent events[MAXSYS];  /* event list */
static        int    ei;              /* event index */
static        int    nev;             /* number of events available */
static        int    kerque;          /* kernel queue fid */
static        int    nchg;            /* number of change filters defined */
static        int    nchgc;           /* number of change filters defined
                                         copy */
static        int    maxnev;          /* maximum number of events returned */

static int resetsev; /* reset system event */

/* end of evtlock section */

/*******************************************************************************

Handle signal from Linux

Handle signal from linux kernel.

*******************************************************************************/

static void sig_handler(int signo)

{

    /* nothing to do: the queue reports the signal; the handler is here so
       the signal's default action, which may end the process, is not taken */
    (void)signo;

}

/** *****************************************************************************

Get system logical event

Finds a slot in the system event id table and allocates that, then returns the
resuling logical id.

*******************************************************************************/

static int getsys(void)

{

    int sid; /* system event id */
    int fid; /* found id */

    sid = 0; /* set 1st table entry */
    fid = -1; /* set no id */
    while (sid < MAXSYS && fid < 0) { /* search free system id */

        if (!systab[sid]) fid = sid; /* set entry found */
        sid++; /* next slot */

    }
    if (fid < 0) { /* event table full */

        fprintf(stderr, "*** System event: Event table full\n");
        fflush(stderr);
        exit(1);

    }
    systab[fid] = malloc(sizeof(systrk)); /* get new event entry */
    if (!systab[fid]) {

        fprintf(stderr, "*** System event: Out of memory\n");
        fflush(stderr);
        exit(1);

    }
    systab[fid]->typ = se_none; /* set no type */
    systab[fid]->fid = -1; /* set no file id */
    systab[fid]->sig = -1; /* set no signal id */
    systab[fid]->rep = 0; /* set no repeat */

    sysno++; /* count active entries */

    return (fid+1); /* exit with system id */

}

/** *****************************************************************************

Add file id to system event handler

Adds the given logical file id to the system event handler set. Returns a system
event logical number.

*******************************************************************************/

int system_event_addseinp(int fid)

{

    int sid; /* system logical event id */
    pid_t pid;

    pthread_mutex_lock(&evtlock); /* take the event lock */
    sid = getsys(); /* get a new system event id */
    systab[sid-1]->typ = se_inp; /* set type */
    systab[sid-1]->fid = fid; /* set fid for file */

    if (nchg >= MAXSYS) {

        pthread_mutex_unlock(&evtlock); /* release the event lock */
        fprintf(stderr, "*** System event: Too many events defined\n");
        fflush(stderr);
        exit(1);

    }

    /* construct event for fid */
    EV_SET(&chgevt[nchg], fid, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    kevent(kerque, &chgevt[nchg], 1, NULL, 0, NULL); /* register now */
    systab[sid-1]->ei = nchg; /* link to event entry */
    nchg++; /* count events registered */
    pthread_mutex_unlock(&evtlock); /* release the event lock */

    /* send reset to this process */
    pid = getpid();
    kill(pid, SIGUSR1);

    return (sid); /* exit with logical event id */

}

/** *****************************************************************************

Deactivate input event

Deactivates the input monitor with the given system event number. Used when
the underlying device goes away, such as a joystick disconnect: an end of
file input is permanently ready, so leaving it registered would spin the
event loop. The entry keeps its logical number but never fires again. The
change list entry is removed and the list compacted, since the wait loop
re-applies the full change list on every call.

*******************************************************************************/

void system_event_deaseinp(int sid)

{

    struct kevent ke; /* single change entry */
    int           ci; /* change list index */
    int           si; /* system table index */

    pthread_mutex_lock(&evtlock); /* take the event lock */
    if (sid <= 0 || !systab[sid-1] || systab[sid-1]->typ != se_inp) {

        pthread_mutex_unlock(&evtlock); /* release the event lock */
        fprintf(stderr, "*** System event: Invalid system event id\n");
        fflush(stderr);
        exit(1);

    }
    if (systab[sid-1]->fid >= 0) {

        /* remove the filter from the kqueue now */
        EV_SET(&ke, systab[sid-1]->fid, EVFILT_READ, EV_DELETE, 0, 0, 0);
        kevent(kerque, &ke, 1, NULL, 0, NULL);
        /* remove the registration from the change list and compact,
           relinking the entries that moved down */
        ci = systab[sid-1]->ei;
        while (ci < nchg-1) { chgevt[ci] = chgevt[ci+1]; ci++; }
        nchg--;
        for (si = 0; si < MAXSYS; si++)
            if (systab[si] && systab[si]->ei > systab[sid-1]->ei)
                systab[si]->ei--;
        systab[sid-1]->ei = -1; /* unlink */
        systab[sid-1]->fid = -1; /* mark input dead */

    }
    pthread_mutex_unlock(&evtlock); /* release the event lock */

}

/** *****************************************************************************

Add signal to system event handler

Adds the given signal number to the system event handler set. Note that this
routine assumes you will let this package handle the interrupt completely,
including handling the signal. If both use of this package to handle signals,
as well as allowing the client to handle signals, a chain handler using
sigaction should be used.

*******************************************************************************/

int system_event_addsesig(int sig)

{

    int   sid; /* system logical event id */
    pid_t pid;

    pthread_mutex_lock(&evtlock); /* take the event lock */
    sid = getsys(); /* get a new system event id */
    systab[sid-1]->typ = se_sig; /* set type */
    systab[sid-1]->sig = sig; /* set signal */

    /* add this signal to mask set */
    sigaddset(&sigmsk, sig);

    /* add signal handler */
    signal(sig, sig_handler);

    /* construct event for fid */
    EV_SET(&chgevt[nchg], sig, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, 0);
    kevent(kerque, &chgevt[nchg], 1, NULL, 0, NULL); /* register now */
    systab[sid-1]->ei = nchg; /* link to event entry */
    nchg++; /* count events registered */
    pthread_mutex_unlock(&evtlock); /* release the event lock */

    /* send reset to this process */
    pid = getpid();
    kill(pid, SIGUSR1);

    return (sid); /* exit with logical event id */

}

/** *****************************************************************************

Activate timer entry

Sets a timer to run with the number of 100us counts, and a repeat status.
Both takes a system event id and returns one. If the system event id is 0, a new
system id is allocated. Then either the new system id or the existing one is
returned.

*******************************************************************************/

int system_event_addsetim(int sid, ami_long t, ami_long r)
{
    struct kevent ke; /* the timer's registration */
    pthread_mutex_lock(&evtlock); /* take the event lock */
    if (!sid) { /* no previous system id */
        sid = getsys(); /* get a new system event id */
        systab[sid-1]->typ = se_tim; /* set type */
        systab[sid-1]->ei = -1; /* timers are not on the change list */
        systab[sid-1]->rep = !!r; /* save repeat flag */
    }
    /* A timer is registered here and now, once, and not through the change
       list: the wait loop re-applies that list on every call, and re-adding
       a kqueue timer re-arms it, so a timer longer than the interval between
       wake-ups could never expire while anything periodic ran. A timer that
       does not repeat is one-shot to the kqueue, which drops it as it fires;
       one that does is re-armed by the kqueue itself. A timer already set
       is re-armed with the new period. */
    EV_SET(&ke, sid, EVFILT_TIMER, EV_ADD | EV_ENABLE | (r? 0: EV_ONESHOT),
           NOTE_USECONDS, (int64_t)t*100, 0);
    kevent(kerque, &ke, 1, NULL, 0, NULL);
    pthread_mutex_unlock(&evtlock); /* release the event lock */
    return (sid);
}

/** *****************************************************************************

Deactivate timer entry

Kills a given timer, by it's id number. Only repeating timers should be killed.
Killed timers are not removed. Once a timer is set active, it is always set
in reserve.

*******************************************************************************/

void system_event_deasetim(int sid)
{
    struct kevent ke; /* the timer's deletion */
    pthread_mutex_lock(&evtlock); /* take the event lock */
    if (sid > 0 && systab[sid-1] && systab[sid-1]->typ == se_tim) {
        /* gone from the kqueue now; a one-shot that has fired is gone
           already, and the kqueue says so, which is no error here */
        EV_SET(&ke, sid, EVFILT_TIMER, EV_DELETE, 0, 0, 0);
        kevent(kerque, &ke, 1, NULL, 0, NULL);
    }
    pthread_mutex_unlock(&evtlock); /* release the event lock */
}

/** *****************************************************************************

Get system event

Get the next system event that occurs. One of an input key, a timer, a frame
timer, or a joystick event occurs, and we return this. The event that is
returned is cleared.

*******************************************************************************/

void system_event_getsevt(sevptr ev)

{

    int           rv;             /* return value */
    int           ti;             /* index for timers */
    int           ji;             /* index for joysticks */
    int           si;             /* index for system event entries */
    uint64_t      exp;            /* timer expiration time */


    pthread_mutex_lock(&evtlock); /* take the event lock */
    ev->typ = se_none; /* set no event occurred */
    do { /* find an active event */

        if (ei >= nev) { /* out of events, read the next ones */

            /* Every registration was applied to the queue as it was made (and a
               removal as it was made): nothing is passed here. The change list
               used to be passed on every read, re-adding each filter, and a
               re-added signal filter forgets a signal that came while the queue
               was not being read: a window resize during a busy moment was lost. */
            pthread_mutex_unlock(&evtlock); /* release the event lock */
            nev = kevent(kerque, NULL, 0, events, MAXSYS, NULL);
            pthread_mutex_lock(&evtlock); /* take the event lock */
            if (nev <= 0) {

                pthread_mutex_unlock(&evtlock); /* release the event lock */
                fprintf(stderr, "*** System event: Error reading next event\n");
                fflush(stderr);
                exit(1);

            }
            /* copy the event list back again */
            ei = 0; /* reset the event index */
            if (nev > maxnev) maxnev = nev; /* find new maximum */

        }
        while (ei < nev && ev->typ == se_none) { 
            
            /* traverse the event array */
            if (events[ei].filter == EVFILT_READ) { /* it's a fid ready event */

                for (si = 0; si < sysno; si++) if (systab[si])
                    if (systab[si]->fid >= 0 && systab[si]->typ == se_inp &&
                        events[ei].ident == systab[si]->fid) {

                    /* fid has flagged */
                    ev->typ = systab[si]->typ; /* set key event occurred */
                    ev->lse = si+1; /* set system logical event no */

                }

            } else if (events[ei].filter == EVFILT_SIGNAL) { /* it's a signal */
/* the event names its signal; the handler is not consulted, as it
   runs on whatever thread the kernel picks, maybe after this */

                for (si = 0; si < sysno; si++) if (systab[si])
                    if (systab[si]->typ == se_sig && systab[si]->sig > 0 &&
                        events[ei].ident == (uintptr_t)systab[si]->sig) {

                    /* signal has flagged */
                    ev->typ = systab[si]->typ; /* set key event occurred */
                    ev->lse = si+1; /* set system logical event no */

                }

            } else if (events[ei].filter == EVFILT_TIMER) {

                /* ident carries the sid */
                ev->typ = systab[events[ei].ident-1]->typ; /* set key event occurred */
                ev->lse = events[ei].ident; /* set system logical event no */

            }
            ei++; /* next event */

        }

    } while (ev->typ == se_none);
    pthread_mutex_unlock(&evtlock); /* release the event lock */

#ifdef PRTSEVT
switch (ev->typ) {
    case se_none: fprintf(stderr, "lse: %d None\n", ev->lse); break;
    case se_inp:  fprintf(stderr, "lse: %d Input file ready\n", ev->lse); break;
    case se_tim:  fprintf(stderr, "lse: %d Timer\n", ev->lse); break;
    case se_sig:  fprintf(stderr, "lse: %d Signal\n", ev->lse); break;
}
fflush(stderr);
#endif

}

/** *****************************************************************************

Initialize system event handler

Sets up the system event handler.

*******************************************************************************/

static void init_system_event (void) __attribute__((constructor (101)));
static void init_system_event()

{

    int si; /* index for system event array */

    /* initialize the events lock */
    pthread_mutex_init(&evtlock, NULL);

    /* clear the tracking array */
    for (si = 0; si < MAXSYS; si++) systab[si] = NULL;

    /* clear the set of signals we capture */
    sigemptyset(&sigmsk);

    /* clear active signals */
    sigemptyset(&sigmsk);

    sysno = 0; /* set no active system events */

    /* create kernel queue */
    kerque = kqueue();
    if (kerque ==-1) {

        fprintf(stderr, "*** System event: Could not create kernel queue\n");
        fflush(stderr);
        exit(1);

    }

    /* set no change filters registered */
    nchg = 0;

    /* enable select reset signal */
    signal(SIGUSR1, SIG_IGN);
    resetsev = system_event_addsesig(SIGUSR1);

    /* clear kevent index and count (no events active ) */
    ei = 0;
    nev = 0;
    maxnev = 0;

}

/** *****************************************************************************

Deinitialize system event handler

Tears down the system event handler.

*******************************************************************************/

static void deinit_system_event (void) __attribute__((destructor (101)));
static void deinit_system_event()

{

    /* close the event queue */
    close(kerque);

    /* release the event lock */
    pthread_mutex_destroy(&evtlock);

}
