/** ****************************************************************************
*                                                                              *
*                          SYSTEM EVENT HANDLER                                *
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
* Registers an input file to be monitored for input ready, or one or more      *
* bytes ready to read. The logical system event number is returned.            *
*                                                                              *
* void system_event_deaseinp(int sid);                                         *
*                                                                              *
* Deactivates the input monitor with the given system event number. Used when  *
* the underlying device goes away, such as a joystick disconnect, since an     *
* end of file input is otherwise permanently ready and would spin the event    *
* loop.                                                                        *
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
#include <signal.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "system_event.h"

#include <diag.h>

//#define PRTSEVT /* print signals diagnostic */
#define MAXSYS 100 /* number of possible logical system events */

/* logical system event record */
typedef struct systrk* sevtptr; /* pointer to system event */
typedef struct systrk {

    sevttyp typ; /* system event type */
    int     fid; /* logical file id if used */
    int     sig; /* signal number */

} systrk;

/*
 * Lock for module data
 */
static pthread_mutex_t evtlock; /* lock for this module data */

/* logical system event tracking array */
static sevtptr systab[MAXSYS];
static int sysno; /* number of system event ids allocated */

/**
 * Set of input file ids for select
 */
static fd_set ifdseta; /* active sets */
static fd_set ifdsets; /* signaled set */
static int ifdmax;

static sigset_t sigmsk; /* signal mask */
static int      sigwr[NSIG]; /* the write end of each signal's pipe, or -1 */
static sigset_t sigact; /* signal active */

static int resetsev; /* reset system event */

/* end of evtlock section */

/** *****************************************************************************

Print file descriptor set

Given a set of file descriptors, prints out active bits. A diagnostic.

*******************************************************************************/

static void prtfds(fd_set* fdset)

{

    int fid;

    for (fid = 0; fid < ifdmax; fid++) 
        fprintf(stderr, "%d", FD_ISSET(fid, fdset));

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

    int   sid; /* system logical event id */
    pid_t pid;

    pthread_mutex_lock(&evtlock); /* take the event lock */
    sid = getsys(); /* get a new system event id */
    systab[sid-1]->typ = se_inp; /* set type */
    systab[sid-1]->fid = fid; /* set fid for file */
    FD_SET(fid, &ifdseta); /* add to active set */
    if (fid+1 > ifdmax) ifdmax = fid+1; /* set maximum fid for pselect() */
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
event loop. The entry keeps its logical number but never fires again.

*******************************************************************************/

void system_event_deaseinp(int sid)

{

    pthread_mutex_lock(&evtlock); /* take the event lock */
    if (sid <= 0 || !systab[sid-1] || systab[sid-1]->typ != se_inp) {

        pthread_mutex_unlock(&evtlock); /* release the event lock */
        fprintf(stderr, "*** System event: Invalid system event id\n");
        fflush(stderr);
        exit(1);

    }
    if (systab[sid-1]->fid >= 0) {

        FD_CLR(systab[sid-1]->fid, &ifdseta); /* remove from active set */
        FD_CLR(systab[sid-1]->fid, &ifdsets); /* and any pending set */
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

The signal is caught by a handler that writes a byte down a pipe of its own,
and the read end of the pipe is the file id pselect() waits on. A handler
runs in whatever thread the kernel delivers the signal to, which is what
makes this work: the earlier way, blocking the signal and reading it from a
signalfd, blocked it in the calling thread only, and a process that carries
other threads -- every program linking the sound library carries dozens,
the synthesizer's -- had the signal taken by one of those with its default
action, so that a window resize never reached the terminal.

*******************************************************************************/

/* the handler: a byte down the pipe, and nothing else */
static void sighan(int sig)

{

    int  e = errno;
    char c = sig;

    if (sig > 0 && sig < NSIG && sigwr[sig] >= 0) write(sigwr[sig], &c, 1);
    errno = e;

}


int system_event_addsesig(int sig)

{

    int              sid;    /* system logical event id */
    int              fid;    /* file id */
    int              pfd[2]; /* the signal's pipe */
    struct sigaction sa;
    pid_t            pid;

    pthread_mutex_lock(&evtlock); /* take the event lock */
    if (sig <= 0 || sig >= NSIG || pipe(pfd)) {

        pthread_mutex_unlock(&evtlock);
        return (0);

    }
    fcntl(pfd[0], F_SETFL, O_NONBLOCK); fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pfd[1], F_SETFL, O_NONBLOCK); fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
    fid = pfd[0];
    sigwr[sig] = pfd[1];
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighan;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(sig, &sa, NULL);

    sid = getsys(); /* get a new system event id */
    systab[sid-1]->typ = se_sig; /* set type */
    systab[sid-1]->sig = sig; /* set signal */
    systab[sid-1]->fid = fid; /* set file id */
    FD_SET(fid, &ifdseta); /* add to active set */
    if (fid+1 > ifdmax) ifdmax = fid+1; /* set maximum fid for pselect() */
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

    struct itimerspec ts;
    int    rv;
    ami_long   tl;
    int    fid;
    pid_t  pid;

    pthread_mutex_lock(&evtlock); /* take the event lock */
    if (!sid) { /* no previous system id */

        sid = getsys(); /* get a new system event id */
        systab[sid-1]->typ = se_tim; /* set type */
        systab[sid-1]->fid = timerfd_create(CLOCK_REALTIME, 0);
        if (systab[sid-1]->fid == -1) {

            pthread_mutex_unlock(&evtlock); /* release the event lock */
            fprintf(stderr, "*** System event: Cannot create timer\n");
            fflush(stderr);
            exit(1);

        }
        FD_SET(systab[sid-1]->fid, &ifdseta); /* add to active set */
        if (systab[sid-1]->fid+1 > ifdmax)
            ifdmax = systab[sid-1]->fid+1; /* set maximum fid for pselect() */

    }
    fid = systab[sid-1]->fid; /* get the fid for the timer */
    pthread_mutex_unlock(&evtlock); /* release the event lock */

    /* set timer run time */
    tl = t;
    ts.it_value.tv_sec = tl/10000; /* set number of seconds to run */
    ts.it_value.tv_nsec = tl%10000*100000; /* set number of nanoseconds to run */

    /* set if timer does not rerun */
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    if (r) { /* timer reruns */

        ts.it_interval.tv_sec = ts.it_value.tv_sec;
        ts.it_interval.tv_nsec = ts.it_value.tv_nsec;

    }

    rv = timerfd_settime(fid, 0, &ts, NULL);
    if (rv < 0) {

        fprintf(stderr, "*** System event: Unable to set time\n");
        fflush(stderr);
        exit(1);

    }

    /* send reset to this process */
    pid = getpid();
    kill(pid, SIGUSR1);

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

    struct itimerspec ts;
    int rv;
    int fid;

    pthread_mutex_lock(&evtlock); /* take the event lock */
    if (sid <= 0 || !systab[sid-1]) {

        pthread_mutex_unlock(&evtlock); /* release the event lock */
        fprintf(stderr, "*** System event: Invalid system event id\n");
        fflush(stderr);
        exit(1);

    }
    fid = systab[sid-1]->fid; /* get the timer fid */
    pthread_mutex_unlock(&evtlock); /* release the event lock */

    /* set timer run time to zero to kill it */
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    rv = timerfd_settime(fid, 0, &ts, NULL);
    if (rv < 0) {

        fprintf(stderr, "*** System event: Unable to set time\n");
        fflush(stderr);
        exit(1);

    }


}

/** *****************************************************************************

Get system event

Get the next system event that occurs. One of an input key, a timer, a frame
timer, or a joystick event occurs, and we return this. The event that is
returned is cleared.

*******************************************************************************/

void system_event_getsevt(sevptr ev)

{

    int                     rv;  /* return value */
    int                     ti;  /* index for timers */
    int                     ji;  /* index for joysticks */
    int                     si;  /* index for system event entries */
    uint64_t                exp; /* timer expiration time */
    char                    drain[64]; /* signal pipe contents */
    int                     k;   /* scan count */
    static int              lastsi = -1; /* the entry returned last, for a fair scan */

    pthread_mutex_lock(&evtlock); /* take the event lock */
    ev->typ = se_none; /* set no event occurred */
    do { /* find an active event */

        /* search for fid and sig entries */
        /* the scan resumes after the entry returned last time: a scan from
           the start returns the first ready entry every time, and one that
           is always ready, a display eight threads keep busy, starves the
           entries after it. The event loop's own timer never came. */
        for (k = 0; k < sysno && ev->typ == se_none; k++)
            if (systab[si = (lastsi+1+k)%sysno]) {

            if (systab[si]->fid >= 0 && FD_ISSET(systab[si]->fid, &ifdsets)) {

                /* fid has flagged */
                ev->typ = systab[si]->typ; /* set key event occurred */
                ev->lse = si+1; /* set system logical event no */
                lastsi = si; /* the next scan starts after this one */
                FD_CLR(systab[si]->fid, &ifdsets); /* remove from input signals */
                if (systab[si]->typ == se_tim) /* is a timer */
                    /* clear the timer by reading it's expiration time */
                    read(systab[si]->fid, &exp, sizeof(uint64_t));
                else if (systab[si]->typ == se_sig) /* it's a signal */
                    /* clear the signal by draining its pipe */
                    while (read(systab[si]->fid, drain, sizeof(drain)) > 0);

            }

        }
        if (ev->typ == se_none) {

            /* no input is active, load a new signaler set */
            ifdsets = ifdseta; /* set up request set */
            pthread_mutex_unlock(&evtlock); /* release the event lock */
            rv = pselect(ifdmax, &ifdsets, NULL, NULL, NULL, &sigmsk);
            pthread_mutex_lock(&evtlock); /* take the event lock */
            /* if error, the input set won't be modified and thus will appear as
               if they were active. We clear them in this case */
            if (rv < 0) FD_ZERO(&ifdsets);

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

    /* clear input select set */
    FD_ZERO(&ifdseta);

    /* clear current max input fd */
    ifdmax = 0;

    /* clear the signaling set */
    FD_ZERO(&ifdsets);

    /* clear the set of signals we capture */
    sigemptyset(&sigmsk);

    /* no signal pipes yet */
    for (si = 0; si < NSIG; si++) sigwr[si] = -1;

    sysno = 0; /* set no active system events */

    /* enable select reset signal */
    signal(SIGUSR1, SIG_IGN);
    resetsev = system_event_addsesig(SIGUSR1);

}

/** *****************************************************************************

Deinitialize system event handler

Tears down the system event handler.

*******************************************************************************/

static void deinit_system_event (void) __attribute__((destructor (101)));
static void deinit_system_event()

{

    int si; /* index for system events */

    /* close any open timers, and the signal pipes with their handlers */
    for (si = 0; si < MAXSYS ; si++)
        if (systab[si] && systab[si]->typ == se_tim) close(systab[si]->fid);
        else if (systab[si] && systab[si]->typ == se_sig) {

            signal(systab[si]->sig, SIG_DFL);
            close(sigwr[systab[si]->sig]); sigwr[systab[si]->sig] = -1;
            close(systab[si]->fid);

        }

    /* release the event lock */
    pthread_mutex_destroy(&evtlock);

}
