/** ****************************************************************************

\file

\brief WIDGET PACKAGE BASE DEFINITIONS

The common support under widget packages built on Petit-Ami graphics: the
machinery that is the same no matter what the widgets are. A widget
package -- the stock set in gnome_widgets.c, the demonstrator in
widget_demo.c, or one of your own -- supplies its widgets: the record
fields they keep, the draw and event code that gives them their look and
behavior, and the entry calls that create them. The base supplies the
rest:

- the tracking model: a per window table of widgets by client id, the
  window id to widget map that routes events, and the record free list;
- creation: a widget is a frameless subwindow of its owner, and the base
  opens, places and registers it;
- the event intercept: the base hooks the event chain once and fields
  events for all packages' widget windows, handing each to its package's
  dispatch function and passing everything else down the chain;
- teardown: killing a widget, and taking a window's widgets down with it
  when the window file closes under them;
- anonymous (negative) widget id allocation.

A package declares one wbpkg, statically, and initializes it with
wb_init from its constructor, passing its callbacks. Packages coexist:
each catches only its own windows.

The package's widget record must begin with the base head, laid down by
the WB_WIGHEAD macro with the package's own record pointer type, so the
head fields read naturally (wg->wf, wg->id) and the base can work on any
package's records:

    typedef struct wigrec* wigptr;
    typedef struct wigrec {

        WB_WIGHEAD(wigptr)          -- the base head, first
        (the package's own fields)

    } wigrec;

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

#ifndef __WIDGET_BASE_H__
#define __WIDGET_BASE_H__

#include <stdio.h>

#include <graphics.h>

#define MAXFIL 100 /* maximum open files */
#define MAXWIG 100 /* maximum widgets per window */

/* The common head of every widget record, laid down first in the
   package's record with the package's own record pointer type. The base
   works on records through this head; the package's fields follow it. */
#define WB_WIGHEAD(recptr) \
    /** next entry in free list */            recptr next; \
    /** output file for the widget window */  FILE*  wf; \
    /** parent window */                      FILE*  parent; \
    /** id number, client assigned */         ami_long   id; \
    /** widget window id */                   ami_long   wid; \
    /** widget is enabled */                  ami_long   enb; \
    /** focused */                            ami_long   focus; \
    /** hovered */                            ami_long   hover; \
    /** position of widget in parent */       ami_long   px, py; \
    /** creation order, for the tab walk */   ami_long   tabseq; \
    /** Tab stops here: the widget takes the keyboard focus */ \
                                              ami_long   tabstop; \
    /** construction complete: events dispatch only from here */ \
                                              ami_long   live;

/* the base's view of a widget record: the head alone */
typedef struct wbwig* wbwigptr;
typedef struct wbwig { WB_WIGHEAD(struct wbwig*) } wbwig;

/* package callbacks. The records pass as void* and the package casts to
   its own record type. */

/* handle an event for one of the package's widgets */
typedef void (*wb_dispatch_t)(ami_evtrec* ev, void* wg);
/* initialize the package's fields of a fresh record (the base has
   initialized the head) */
typedef void (*wb_wiginit_t)(void* wg);
/* about to take the widget down: stop timers, kill subwidgets */
typedef void (*wb_wigkill_t)(void* wg);
/* release the package's resources held by the record */
typedef void (*wb_wigfree_t)(void* wg);
/* report an error for the package, and do not return */
typedef void (*wb_error_t)(char* es);

/* per window file record: the widgets living in that window, indexed by
   client id, which may be negative; 0 is never used */
typedef struct wbfil* wbfilptr;
typedef struct wbfil {

    wbwigptr widgets[MAXWIG*2+1];

} wbfil;

/* One widget package. Declared statically by the package; the fields
   belong to the base. */
typedef struct wbpkg* wbpkgptr;
typedef struct wbpkg {

    /** package list link */                  wbpkgptr      next;
    /** widget record size, package's own */  ami_long      recsiz;
    /** event handler for its widgets */      wb_dispatch_t dispatch;
    /** record field initializer, or NULL */  wb_wiginit_t  wiginit;
    /** pre-teardown, or NULL */              wb_wigkill_t  wigkill;
    /** resource release, or NULL */          wb_wigfree_t  wigfree;
    /** error reporter, or NULL for stderr */ wb_error_t    errrep;
    /** record free list */                   wbwigptr      wigfre;
    /** open files table */                   wbfilptr      opnfil[MAXFIL];
    /** widget window id to widget */         wbwigptr      xltwig[MAXFIL*2+1];

} wbpkg;

/* register the package: hooks the event chain and close() on first use */
void wb_init(wbpkg* pk, ami_long recsiz, wb_dispatch_t dispatch,
             wb_wiginit_t wiginit, wb_wigkill_t wigkill,
             wb_wigfree_t wigfree, wb_error_t errrep);
/* unregister the package, taking down any widgets it still holds; the
   last package out restores the hooks */
void wb_deinit(wbpkg* pk);
/* get a fresh widget record, head initialized, package fields via the
   wiginit callback; for predefining an entry to pass into wb_widget */
void* wb_getwig(wbpkg* pk);
/* Create a widget: a frameless subwindow of the parent over the given
   rectangle, graphical coordinates, registered under the id. *wpr NULL
   allocates a record; a predefined record passes in through it (how a
   package builds compound widgets). The record returns through it. */
void wb_widget(wbpkg* pk, FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2,
               ami_long id, void* wpr);
/* find a widget by parent window and id; errors if there is none */
void* wb_fndwig(wbpkg* pk, FILE* f, ami_long id);
/* take a widget down: pre-teardown callback, window closed, record
   released */
void wb_killwidget(wbpkg* pk, FILE* f, ami_long id);
/* allocate an anonymous (negative) widget id in the window */
ami_long wb_getwigid(wbpkg* pk, FILE* f);
/* kill every widget on the window, every package, without closing it;
   what a session leaves behind must not greet the next one */
void wb_purge(FILE* f);

#endif /* __WIDGET_BASE_H__ */
