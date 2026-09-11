/** ****************************************************************************

\file

\brief WIDGET PACKAGE BASE

The common support under widget packages built on Petit-Ami graphics. See
widget_base.h for the story and the contract; gnome_widgets.c for the
stock widget set built on it; widget_demo.c for the smallest possible
package, one demonstration widget, which is the place to start reading.

The base hooks two process-wide vectors, once, when the first package
registers: the event chain, where it fields events for every package's
widget windows and passes the rest down; and close(), where it takes a
closing window's widgets down with the window. Packages register in
constructor order and unregister in destructor order; the last one out
puts the vectors back, verifying that what it removes is its own.

                          BSD LICENSE INFORMATION

Copyright (C) 2026 - Scott A. Franco

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the project nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <localdefs.h>
#include <graphics.h>

#include <widget_base.h>

/* system override for close() */
typedef int (*pclose_t)(int);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
#ifndef __MACH__
#define NOCANCEL
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
#endif

static wbpkgptr    pkglst;          /* registered packages, first in first */
static int         wb_hooked;       /* the vectors are hooked */

/* The tables and records are shared between the thread that makes and
   kills widgets and the thread that delivers their events; the lock
   covers exactly that data. A kill that lands while a dispatch is in
   flight cannot free the record out from under the handler: the entry
   leaves the tables at once, so nothing new finds it, and the teardown
   itself waits on the deferred list until the dispatch ends. The lock
   is recursive because closing a widget window re-enters through the
   close intercept. */
typedef struct wbdefer {

    struct wbdefer* next; /* next deferral */
    wbpkgptr        pk;   /* owning package */
    wbwigptr        wp;   /* the widget to tear down */

} wbdefer;

static pthread_mutex_t wblock;     /* recursive; made in wb_lockinit */
static pthread_once_t  wbonce = PTHREAD_ONCE_INIT;
static int             wbdispatch; /* a dispatch is in flight */
static wbdefer*        wbdeflst;   /* kills awaiting the dispatch end */

static void wb_lockinit(void)

{

    pthread_mutexattr_t a;

    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&wblock, &a);
    pthread_mutexattr_destroy(&a);

}

static void wb_lock(void)
    { pthread_once(&wbonce, wb_lockinit); pthread_mutex_lock(&wblock); }
static void wb_unlock(void)
    { pthread_mutex_unlock(&wblock); }
static ami_pevthan wb_event_old;    /* downchain event handler */
static pclose_t    ofpclose;        /* saved close() vectors */
#ifdef NOCANCEL
static pclose_t    ofpclose_nocancel;
#endif

/** ****************************************************************************

Process error

Reports through the package's reporter when it has one, so a package that
presents errors its own way (the stock set raises a dialog) keeps its
way; the base's own is stderr. Neither returns.

*******************************************************************************/

static void error(
    /** Package, or NULL */ wbpkgptr pk,
    /** Error string */     char*    es
)

{

    if (pk && pk->errrep) pk->errrep(es);
    fprintf(stderr, "Error: widget_base: %s\n", es);
    fflush(stderr);

    exit(1);

}

/** ****************************************************************************

Make file entry

If the indicated file does not have a file tracking structure in the
package, one is created. Otherwise it is a no-op.

*******************************************************************************/

static void makfil(
    /** Package */     wbpkgptr pk,
    /** Window file */ FILE*    f
)

{

    ami_long fn;
    ami_long i;

    if (!f) error(pk, "Invalid window file");
    fn = fileno(f);
    if (fn < 0 || fn > MAXFIL) error(pk, "Invalid file number");
    if (!pk->opnfil[fn]) {

        pk->opnfil[fn] = malloc(sizeof(wbfil));
        if (!pk->opnfil[fn]) error(pk, "Out of memory");
        for (i = 0; i < MAXWIG*2+1; i++) pk->opnfil[fn]->widgets[i] = NULL;

    }

}

/** ****************************************************************************

Get widget

Gets a widget record off the package's free list, or allocates one at the
package's record size. The head is initialized here; the package's own
fields initialize through its callback.

\returns Pointer to new widget.

*******************************************************************************/

void* wb_getwig(
    /** Package */ wbpkg* pk
)

{

    wbwigptr wp;

    wb_lock();
    if (pk->wigfre) {

        wp = pk->wigfre;
        pk->wigfre = pk->wigfre->next;

    } else wp = malloc(pk->recsiz);
    wb_unlock();
    if (!wp) error(pk, "Out of memory");
    wp->next = NULL;
    wp->wf = NULL;
    wp->parent = NULL;
    wp->id = 0;
    wp->wid = 0;
    wp->enb = FALSE; /* set enabled at display */
    wp->focus = FALSE;
    wp->hover = FALSE;
    wp->px = 0;
    wp->py = 0;
    wp->live = FALSE; /* not constructed: no events until the package says */
    if (pk->wiginit) pk->wiginit(wp); /* the package's fields */

    return (wp);

}

/** ****************************************************************************

Put widget

Releases the package's resources held by the record through its callback,
and the record to the package's free list.

*******************************************************************************/

static void putwig(
    /** Package */                  wbpkgptr pk,
    /** Pointer to widget to free */ wbwigptr wp
)

{

    if (pk->wigfree) pk->wigfree(wp);
    wp->next = pk->wigfre;
    pk->wigfre = wp;

}

/** ****************************************************************************

Find widget

Given a window file and a widget id, returns the widget. Validates both.

\returns Pointer to found widget.

*******************************************************************************/

void* wb_fndwig(
    /** Package */             wbpkg* pk,
    /** Window file pointer */ FILE*  f,
    /** Logical widget id */   ami_long   id
)

{

    ami_long fn;

    void* wp;

    if (id <= -MAXWIG || id > MAXWIG || !id) error(pk, "Invalid widget id");
    fn = fileno(f);
    if (fn < 0 || fn > MAXFIL) error(pk, "Invalid file number");
    wb_lock();
    if (!pk->opnfil[fn] || !pk->opnfil[fn]->widgets[id+MAXWIG]) {

        wb_unlock();
        error(pk, "No widget by given id");

    }
    wp = pk->opnfil[fn]->widgets[id+MAXWIG];
    wb_unlock();

    return (wp);

}

/** ****************************************************************************

Create widget

The heart of the pattern: a widget is a frameless subwindow of its
parent, placed over the widget's rectangle. Buffering is off so draws
appear as made, auto is off so drawing cannot scroll it, the cursor is
off so no cursor shows in it, the frame is off so it is nothing but its
face, and background writes are off. Events addressed to the window
arrive at the package's dispatch function.

A predefined record can be passed in through wpr, which is how a package
builds compound widgets: the builder predefines the record (wb_getwig),
sets its controlling fields, and passes it in. NULL allocates fresh.

*******************************************************************************/

void wb_widget(
    /** Package */                         wbpkg* pk,
    /** Parent window file */              FILE* f,
    /** Containing rectangle for widget */ ami_long x1, ami_long y1, ami_long x2, ami_long y2,
    /** logical id for widget */           ami_long id,
    /** Widget I/O pointer, void** */      void* wpr
)

{

    ami_long fn;
    wbwigptr wp;
    wbwigptr* wprp = (wbwigptr*)wpr;

    if (id <= -MAXWIG || id > MAXWIG || !id) error(pk, "Invalid widget id");
    wb_lock();
    makfil(pk, f);
    fn = fileno(f);
    wp = *wprp; /* get any predefined widget entry */
    if (!wp) wp = wb_getwig(pk); /* get widget entry if none passed in */
    if (pk->opnfil[fn]->widgets[id+MAXWIG]) {

        wb_unlock();
        error(pk, "Widget by id already in use");

    }
    pk->opnfil[fn]->widgets[id+MAXWIG] = wp;
    wb_unlock();
    wp->wid = ami_getwinid(); /* allocate a buried window id */
    ami_openwin(&stdin, &wp->wf, f, wp->wid); /* open widget window */
    wp->parent = f;
    wb_lock();
    pk->xltwig[wp->wid+MAXFIL] = wp; /* events for the window find the widget */
    wb_unlock();
    wp->id = id;
    ami_buffer(wp->wf, FALSE); /* draws appear as made */
    ami_auto(wp->wf, FALSE);   /* drawing must not scroll the face */
    ami_curvis(wp->wf, FALSE); /* a widget face never shows a cursor */
    ami_frame(wp->wf, FALSE);  /* the widget is nothing but its face */
    ami_setposg(wp->wf, x1, y1); /* place at position */
    /* The rectangle is inclusive, as every caller makes it: a widget sized
       by its own sizing call is placed at x1, y1, x1+w-1, y1+h-1, and the
       library's own menus size the same way. Made x2-x1 by y2-y1 the
       window was a pixel short each way, and a list box made for three
       rows had room to draw two. */
    ami_setsizg(wp->wf, x2-x1+1, y2-y1+1); /* set size */
    ami_binvis(wp->wf); /* no background writes */
    wp->enb = TRUE; /* set is enabled */
    wp->px = x1; /* set widget position in parent */
    wp->py = y1;

    *wprp = wp; /* copy back to caller */

}

/** ****************************************************************************

Kill widget

Takes the widget down: the package's pre-teardown callback first (stop
timers, kill subwidgets), then window id tracking cleared, so a window id
reused after the widget is freed cannot dispatch events to the freed
entry, then the widget window is closed and the record released.

*******************************************************************************/

/* the teardown proper: the package's pre-teardown callback (stop
   timers, kill subwidgets), the window, the record. The lock is held;
   the close re-enters the close intercept, which the recursive lock
   absorbs. */
static void teardown(
    /** Package */ wbpkgptr pk,
    /** Widget */  wbwigptr wp
)

{

    if (pk->wigkill) pk->wigkill(wp); /* the package's pre-teardown */
    fclose(wp->wf); /* close the window file */
    putwig(pk, wp);

}

static void intkillwidget(
    /** Package */           wbpkgptr pk,
    /** file id */           ami_long fn,
    /** Logical widget id */ ami_long id
)

{

    wbwigptr wp;
    wbdefer* dp;

    if (fn < 0 || fn > MAXFIL) error(pk, "Invalid file number");
    wb_lock();
    if (!pk->opnfil[fn]) {

        wb_unlock();
        error(pk, "File by id not open");

    }
    if (id <= -MAXWIG || id > MAXWIG || !id) {

        wb_unlock();
        error(pk, "Invalid widget id");

    }
    if (!pk->opnfil[fn]->widgets[id+MAXWIG]) {

        wb_unlock();
        error(pk, "No widget by given id");

    }
    wp = pk->opnfil[fn]->widgets[id+MAXWIG];
    /* out of the tables at once: nothing new finds the widget */
    pk->xltwig[wp->wid+MAXFIL] = NULL;
    pk->opnfil[fn]->widgets[id+MAXWIG] = NULL;
    if (wbdispatch) {

        /* a dispatch is in flight and may be standing on this record;
           the teardown waits for it */
        dp = malloc(sizeof(wbdefer));
        if (!dp) { wb_unlock(); error(pk, "Out of memory"); }
        dp->pk = pk;
        dp->wp = wp;
        dp->next = wbdeflst;
        wbdeflst = dp;

    } else teardown(pk, wp);
    wb_unlock();

}

void wb_killwidget(
    /** Package */           wbpkg* pk,
    /** Window file */       FILE*  f,
    /** Logical widget id */ ami_long   id
)

{

    intkillwidget(pk, fileno(f), id);

}

/** ****************************************************************************

Allocate anonymous widget id

Allocates and returns an "anonymous" widget id for the given window.
Client assigned ids are positive; anonymous ids are negative, starting
with -1 and proceeding downwards, so the two can never collide. The id
entry opens at widget creation and closes with the kill, so an anonymous
id, once allocated, is reserved until used and killed.

\returns The widget id.

*******************************************************************************/

ami_long wb_getwigid(
    /** Package */     wbpkg* pk,
    /** Window file */ FILE*  f
)

{

    ami_long fn;
    ami_long wid;

    wb_lock();
    makfil(pk, f);
    fn = fileno(f);
    wid = -1; /* start at -1 */
    while (wid > -MAXWIG && pk->opnfil[fn]->widgets[wid+MAXWIG]) wid--;
    wb_unlock();
    if (wid == -MAXWIG) error(pk, "No more anonymous widget IDs");

    return (wid);

}

/** ****************************************************************************

Purge window

Kills every widget on the window, for every registered package, without
closing the window itself. The display server uses it between sessions:
a session's widgets on the main window would otherwise greet the next
session.

*******************************************************************************/

void wb_purge(
    /** Window file */ FILE* f
)

{

    wbpkgptr pk;
    ami_long fd;
    ami_long i;

    fd = fileno(f);
    if (fd < 0 || fd >= MAXFIL) return;
    wb_lock();
    for (pk = pkglst; pk; pk = pk->next)
        if (pk->opnfil[fd])
            for (i = 0; i < MAXWIG*2+1; i++)
                if (pk->opnfil[fd]->widgets[i])
                    intkillwidget(pk, fd, i-MAXWIG);
    wb_unlock();

}

/** ****************************************************************************

Widget event intercept

Hooked into the event chain when the first package registers. Events
whose window id belongs to a package's widget window dispatch to that
package; all others pass down the chain to whoever hooked before -- and
finally the program. This is what lets any number of widget packages
coexist: each recognizes only its own windows.

*******************************************************************************/

static void wb_event(
    /** Event record pointer */ ami_evtrec* ev
)

{

    wbpkgptr pk;
    wbwigptr wg = NULL;
    wbpkgptr fpk = NULL;
    wbdefer* dp;

    wb_lock();
    if (ev->winid > -MAXFIL && ev->winid <= MAXFIL)
        for (pk = pkglst; pk && !fpk; pk = pk->next) {

            wg = pk->xltwig[ev->winid+MAXFIL];
            if (wg) fpk = pk; /* one of this package's widgets */

        }
    /* A widget under construction takes no events: the record goes into
       the tables before the package finishes building it, and a stale
       event dispatched into that gap runs a handler on half-set state.
       The completing redraw repaints whatever was missed. */
    if (fpk && !wg->live) { wb_unlock(); return; }
    if (fpk) wbdispatch++; /* kills defer from here */
    wb_unlock();
    if (fpk) {

        fpk->dispatch(ev, wg);
        wb_lock();
        wbdispatch--;
        if (!wbdispatch) while (wbdeflst) {

            /* the kills that waited on this dispatch */
            dp = wbdeflst;
            wbdeflst = dp->next;
            teardown(dp->pk, dp->wp);
            free(dp);

        }
        wb_unlock();

        return;

    }
    wb_event_old(ev); /* no package's: down the chain */

}

/** ****************************************************************************

Close intercepts

A window file closed with widgets still in it takes the widgets down
with it, for every registered package; without this, closing the parent
stranded the widget windows and their records.

*******************************************************************************/

static int ivclose(
    /** Base call vector */ pclose_t closedc,
    /** File logical id */  int fd
)

{

    wbpkgptr pk;
    ami_long i;

    if (fd >= 0 && fd < MAXFIL) {

        wb_lock();
        for (pk = pkglst; pk; pk = pk->next)
            if (pk->opnfil[fd]) {

                for (i = 0; i < MAXWIG*2+1; i++)
                    if (pk->opnfil[fd]->widgets[i])
                        intkillwidget(pk, fd, i-MAXWIG);
                free(pk->opnfil[fd]);
                pk->opnfil[fd] = NULL;

            }
        wb_unlock();

    }

    return (*closedc)(fd);

}

static int iclose(int fd)

{

    return ivclose(ofpclose, fd);

}

#ifdef NOCANCEL
static int iclose_nocancel(int fd)

{

    return ivclose(ofpclose_nocancel, fd);

}
#endif

/** ****************************************************************************

Register package

Clears the package's tables and puts it on the package list; the first
registration hooks the event chain and close(). Called from the
package's constructor: registration order is constructor order, and a
package's widgets are recognized from that point on.

*******************************************************************************/

void wb_init(
    /** Package */                    wbpkg*        pk,
    /** widget record size */         ami_long      recsiz,
    /** event handler for widgets */  wb_dispatch_t dispatch,
    /** record field initializer */   wb_wiginit_t  wiginit,
    /** pre-teardown callback */      wb_wigkill_t  wigkill,
    /** resource release callback */  wb_wigfree_t  wigfree,
    /** error reporter, or NULL */    wb_error_t    errrep
)

{

    ami_long fn;
    wbpkgptr* lp;

    if (recsiz < (ami_long)sizeof(wbwig)) error(pk, "Record too small for head");
    if (!dispatch) error(pk, "Package must dispatch events");
    pk->recsiz = recsiz;
    pk->dispatch = dispatch;
    pk->wiginit = wiginit;
    pk->wigkill = wigkill;
    pk->wigfree = wigfree;
    pk->errrep = errrep;
    pk->wigfre = NULL;
    for (fn = 0; fn < MAXFIL; fn++) pk->opnfil[fn] = NULL;
    for (fn = 0; fn < MAXFIL*2+1; fn++) pk->xltwig[fn] = NULL;
    pk->next = NULL;
    /* onto the end of the package list: dispatch checks packages in
       registration order */
    lp = &pkglst;
    while (*lp) lp = &(*lp)->next;
    *lp = pk;
    if (pkglst == pk) { /* first in: hook the vectors */

        ami_eventsover(wb_event, &wb_event_old);
        ovr_close(iclose, &ofpclose);
#ifdef NOCANCEL
        ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);
#endif
        wb_hooked = TRUE;

    }

}

/** ****************************************************************************

Unregister package

Takes down any widgets the package still holds, and takes it off the
package list. The last package out restores the hooked vectors,
verifying that what it removes is its own. Called from the package's
destructor: destructors run in reverse constructor order, so the first
package registered restores.

*******************************************************************************/

void wb_deinit(
    /** Package */ wbpkg* pk
)

{

    ami_long    fn;
    ami_long    i;
    wbpkgptr*   lp;
    ami_pevthan cppevt;
    pclose_t    cppclose;
#ifdef NOCANCEL
    pclose_t    cppclose_nocancel;
#endif

    /* the package's remaining widgets go down with it */
    for (fn = 0; fn < MAXFIL; fn++)
        if (pk->opnfil[fn]) {

            for (i = 0; i < MAXWIG*2+1; i++)
                if (pk->opnfil[fn]->widgets[i])
                    intkillwidget(pk, fn, i-MAXWIG);
            free(pk->opnfil[fn]);
            pk->opnfil[fn] = NULL;

        }
    /* off the package list */
    lp = &pkglst;
    while (*lp && *lp != pk) lp = &(*lp)->next;
    if (*lp) *lp = pk->next;
    /* An error exit during early startup runs destructors for packages
       that never registered; with nothing hooked there is nothing to
       put back. */
    if (!wb_hooked) return;
    if (!pkglst) { /* last out: put back the vectors we took */

        ami_eventsover(wb_event_old, &cppevt);
        if (cppevt != wb_event) error(pk, "Event vector mismatch");
        ovr_close(ofpclose, &cppclose);
        if (cppclose != iclose) error(pk, "System override vector mismatch");
#ifdef NOCANCEL
        ovr_close_nocancel(ofpclose_nocancel, &cppclose_nocancel);
        if (cppclose_nocancel != iclose_nocancel)
            error(pk, "System override vector mismatch");
#endif

    }

}
