/** ****************************************************************************

\file

\brief WIDGET DEMONSTRATOR: HOW TO MAKE YOUR OWN WIDGET

Copyright (C) 2026 Scott A. Franco

This is a demonstrator for creating your own widgets on top of Petit-Ami
graphics. It is the smallest possible widget package: exactly one widget,
built on widget_base.c, the common support shared with the stock widget
set in gnome_widgets.c, so the shape of the thing can be seen whole.

The widget here is a "kick button": a button whose face is a still photo
of a soccer player. Press it and the player kicks the ball, as a frame
animation with a kick sound at the moment of contact. That is all it
does. It is deliberately not useful; it is deliberately a widget no stock
widget set would ever give you, which is the point of rolling your own.

                            THE RECIPE

A widget is a small frameless subwindow of the window that owns it. That
one idea buys everything else: the graphics module clips it, buffers it,
occludes it and delivers events addressed to it, exactly as for any other
window. The base (widget_base.h) owns that machinery; what remains for
the widget author is exactly what makes the widget itself:

1. THE RECORD: declare your widget record with the base head first
   (a wbwig named head) and your own state after it -- here, the
   animation frame and whether it is running. Register the package once at startup
   (wb_init), passing your callbacks.

2. DRAW: paint the face on the widget window when a redraw event arrives,
   and whenever your own state changes. The face here is a picture
   (ami_loadpict/ami_picture); the stock set draws with lines, rectangles
   and text. Anything the drawing calls can do can be a widget face.

3. BEHAVE: your dispatch callback receives every event addressed to your
   widgets' windows -- mouse, keyboard, timers, redraw -- and reports to
   the parent window's event queue with ami_sendevent. A widget standing
   in for a stock kind would report the stock code; a widget with no
   stock analogue defines its own code from the user space, ami_etuser
   and up, with the event record's union fields its own to assign. This
   widget defines ETKICKED (widget_demo.h), fired not at the press but
   at the frame where the foot meets the ball, the widget id riding in
   the record.

4. ENTRY CALLS: a widget package defines its own. The stock set overrides
   the ami_ widget vectors because it implements the standard set; a user
   widget has no vector to override, and none is needed: kickbutton() is
   just a call, and wb_widget/wb_killwidget under it do the mechanics.

The base holds what every package needs alike: the tracking tables, the
subwindow creation, the event chain intercept that routes each package
its own windows and passes the rest down, teardown, and the close()
override that takes a window's widgets down with it. Read this file for
the shape, then gnome_widgets.c for what a full set looks like on the
same base.

                              THE ASSETS

The animation frames (tests/widget_demo/kick01.bmp ...) are derived from
"Boykickingball-slowmotion-tokyo-20160423.webm" by Nesnad, from Wikimedia
Commons, licensed CC BY 4.0 (creativecommons.org/licenses/by/4.0). The
kick sound (tests/widget_demo/kick.wav) is synthesized for this demo and
carries no license restrictions. Pictures load only as 24 bit uncompressed
.bmp, which is why the frames are stored that way.

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

#include <stdio.h>

#include <localdefs.h>
#include <graphics.h>
#include <sound.h>

#include <widget_base.h>
#include <widget_demo.h>

/* the animation */
#define FRAMES    24  /* frames in the kick animation */
#define FRATE     700 /* frame time, timer units of 100us: 70ms a frame */
#define KICKFRM   12  /* the frame where the foot meets the ball */
#define FRMTIMER  1   /* the widget window timer used for the animation */
/* where the assets live, relative to the run directory */
#define FRMFILE   "tests/widget_demo/kick%02d" /* .bmp is implied */
#define WAVFILE   "tests/widget_demo/kick"     /* .wav is implied */

/* the widget's few colors, packed RGB. The stock set draws from a theme
   table read from configuration; a one widget demo hardcodes them */
#define RGB(r, g, b) ((r) << 16|(g) << 8|(b))
#define COL_OUTLINE  RGB(0x8f, 0x8f, 0x8f) /* resting outline */
#define COL_HOVER    RGB(0x4a, 0x90, 0xd9) /* outline with the mouse over */
#define COL_FOCUS    RGB(0x1a, 0x5f, 0xb4) /* outline with the focus */

/* Widget record: the base head first, then everything this widget needs
   to know about one instance -- which for the kick button is only the
   animation state. */
typedef struct wigrec* wigptr;
typedef struct wigrec {

    wbwig head; /* the base head, first */
    /** the kick is running */              ami_long playing;
    /** current animation frame, 1 based */ ami_long frame;

} wigrec;

static wbpkg pkg;    /* this package's base instance */
static int   sndopn; /* kick sound is loaded */
static ami_long kickwav; /* the kick sound's wave id, from ami_getwaveid():
                            an anonymous id, clear of any the program names */

/** ****************************************************************************

Kick button draw

Draws the face: the current animation frame scaled over the whole widget
(frame 1 is the resting still photo), and an outline over it whose color
reports the widget's state: focused, hovered, or resting. The face is a
picture, but any drawing the graphics calls can make can be a face.

*******************************************************************************/

static void kickbutton_draw(
    /** Widget data pointer */ wigptr wg
)

{

    ami_long fr = wg->frame;

    if (fr < 1 || fr > FRAMES) fr = 1;
    ami_picture(wg->head.wf, fr, 1, 1, ami_maxxg(wg->head.wf), ami_maxyg(wg->head.wf));
    ami_linewidth(wg->head.wf, 3);
    if (wg->head.focus && wg->head.enb)
        ami_fcolorg(wg->head.wf, COL_FOCUS >> 16&0xff, COL_FOCUS >> 8&0xff,
                            COL_FOCUS&0xff);
    else if (wg->head.hover && wg->head.enb)
        ami_fcolorg(wg->head.wf, COL_HOVER >> 16&0xff, COL_HOVER >> 8&0xff,
                            COL_HOVER&0xff);
    else
        ami_fcolorg(wg->head.wf, COL_OUTLINE >> 16&0xff, COL_OUTLINE >> 8&0xff,
                            COL_OUTLINE&0xff);
    ami_rrect(wg->head.wf, 2, 2, ami_maxxg(wg->head.wf)-1, ami_maxyg(wg->head.wf)-1, 20, 20);

}

/** ****************************************************************************

Kick button event handler

The widget's behavior, fed by the events the base routes to this package
for its widget windows:

- redraw paints the face.
- a button 1 press starts the kick: a repeating timer on the widget
  window paces the frames.
- each timer tick advances a frame; at the contact frame the kick sound
  fires and the kick reports to the parent as the user defined ETKICKED
  event -- the widget's meaning is "he kicked the ball", and that is
  when the ball is kicked; after the last frame the timer is killed and
  the face returns to the still.
- focus, hover and their ends just repaint, showing the state color.

*******************************************************************************/

static void kickbutton_event(
    /** Event record pointer */ ami_evtrec* ev,
    /** Widget data pointer */  void*       vwg
)

{

    wigptr     wg = (wigptr)vwg;
    ami_evtrec er;

    if (ev->etype == ami_etredraw) kickbutton_draw(wg);
    else if (ev->etype == ami_etmouba && ev->amoubn == 1) {

        if (wg->head.enb && !wg->playing) {

            /* roll the animation off the press */
            wg->playing = TRUE;
            wg->frame = 1;
            ami_timer(wg->head.wf, FRMTIMER, FRATE, TRUE);

        }

    } else if (ev->etype == ami_ettim && ev->timnum == FRMTIMER) {

        if (wg->playing && wg->frame < FRAMES) {

            wg->frame++;
            if (wg->frame == KICKFRM) {

                /* The foot meets the ball: the sound, and the report.
                   The kick goes to the parent as the widget's own user
                   defined event, at the moment it means. */
                if (sndopn) ami_playwave(AMI_WAVE_OUT, 0, kickwav);
                er.etype = ETKICKED;
                er.butid = wg->head.id; /* ours to assign for our own code */
                ami_sendevent(wg->head.parent, &er);

            }
            kickbutton_draw(wg);

        } else { /* done: back to the still */

            ami_killtimer(wg->head.wf, FRMTIMER);
            wg->playing = FALSE;
            wg->frame = 1;
            kickbutton_draw(wg);

        }

    } else if (ev->etype == ami_etfocus) {

        wg->head.focus = TRUE;
        kickbutton_draw(wg);

    } else if (ev->etype == ami_etnofocus) {

        wg->head.focus = FALSE;
        kickbutton_draw(wg);

    } else if (ev->etype == ami_ethover) {

        wg->head.hover = TRUE;
        kickbutton_draw(wg);

    } else if (ev->etype == ami_etnohover) {

        wg->head.hover = FALSE;
        kickbutton_draw(wg);

    }

}

/** ****************************************************************************

Package callbacks

The base initializes the record head; wiginit initializes this widget's
own fields. wigkill runs before the base takes a widget down: the
animation timer must not outlive its window.

*******************************************************************************/

static void kickbutton_init(
    /** Widget data pointer */ void* vwg
)

{

    wigptr wg = (wigptr)vwg;

    wg->playing = FALSE;
    wg->frame = 1; /* resting on the still */

}

static void kickbutton_kill(
    /** Widget data pointer */ void* vwg
)

{

    wigptr wg = (wigptr)vwg;

    if (wg->playing) ami_killtimer(wg->head.wf, FRMTIMER);

}

/** ****************************************************************************

Create kick button

The public call: creates a kick button in the given window over the given
rectangle, in graphical coordinates, with the given client id. The base
makes the subwindow and the record; the face photo and the animation
frames load onto the widget's own window (each window carries its own
picture table), and the kick sound loads once for the program.

*******************************************************************************/

void kickbutton(
    /** Parent window file */   FILE* f,
    /** Containing rectangle */ ami_long x1, ami_long y1, ami_long x2, ami_long y2,
    /** logical id for widget */ ami_long id
)

{

    wigptr wp = NULL;
    char   fn[100];
    ami_long   i;

    wb_widget(&pkg, f, x1, y1, x2, y2, id, &wp);
    /* the still and the animation frames, pictures 1..FRAMES on the
       widget window */
    for (i = 1; i <= FRAMES; i++) {

        sprintf(fn, FRMFILE, (int)i);
        ami_loadpict(wp->head.wf, i, fn);

    }
    if (!sndopn) { /* the kick sound loads once */

        ami_openwaveout(AMI_WAVE_OUT);
        kickwav = ami_getwaveid();
        ami_loadwave(kickwav, WAVFILE);
        sndopn = TRUE;

    }
    wp->head.live = TRUE; /* whole from here: events may dispatch */
    kickbutton_draw(wp); /* show the resting face */

}

/** ****************************************************************************

Kill kick button

Removes the kick button by id from the window. The package that creates a
widget kind removes it: the stock killwidget call knows nothing of this
widget, and need not.

*******************************************************************************/

void kickbuttonkill(
    /** Parent window file */    FILE* f,
    /** logical id for widget */ ami_long  id
)

{

    wb_killwidget(&pkg, f, id); /* wigkill stops the animation */

}

/** ****************************************************************************

Enable/disable kick button

A disabled button shows no state colors and does not kick.

*******************************************************************************/

void kickbuttonenable(
    /** Parent window file */    FILE* f,
    /** logical id for widget */ ami_long  id,
    /** enable/disable */        ami_long  e
)

{

    wigptr wp = wb_fndwig(&pkg, f, id);

    wp->head.enb = !!e;
    kickbutton_draw(wp);

}

/** ****************************************************************************

Widget demo startup/shutdown

Registers the package with the base. Runs after graphics (102), sound
(103) and the stock widget set (105): registration order is constructor
order, and each package's widgets are recognized from its own windows
regardless of order.

*******************************************************************************/

static void init_widget_demo(void) __attribute__((constructor (110)));
static void init_widget_demo()

{

    sndopn = FALSE;
    wb_init(&pkg, sizeof(wigrec), kickbutton_event, kickbutton_init,
            kickbutton_kill, NULL, NULL);

}

static void deinit_widget_demo(void) __attribute__((destructor (110)));
static void deinit_widget_demo()

{

    wb_deinit(&pkg);

}
