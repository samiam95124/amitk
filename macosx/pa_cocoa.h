/*******************************************************************************
*                                                                              *
*                    Internal Cocoa/Quartz shim interface                      *
*                                                                              *
* C interface between macosx/graphics.c and macosx/graphics_cocoa.m           *
* All types here are plain C so both sides can include this header.            *
*                                                                              *
*******************************************************************************/

#ifndef __PA_COCOA_H__
#define __PA_COCOA_H__

#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>

/*
 * Opaque window handle. The .m file allocates a PAWindow Objective-C object
 * and hands back a void* to the C side.
 */
typedef void* pa_winhan;

/* Event types mirrored from ami_evtcod for use in the shim queue. */
typedef enum {
    PA_EVT_NONE = 0,
    PA_EVT_CHAR,        /* keyboard character */
    PA_EVT_KEYDOWN,     /* special key */
    PA_EVT_RESIZE,      /* window resized */
    PA_EVT_CLOSE,       /* window close button */
    PA_EVT_FOCUS,       /* gained focus */
    PA_EVT_UNFOCUS,     /* lost focus */
    PA_EVT_MOUSE_MOVE,  /* mouse moved */
    PA_EVT_MOUSE_DOWN,  /* mouse button pressed */
    PA_EVT_MOUSE_UP,    /* mouse button released */
    PA_EVT_REDRAW,      /* window needs redraw */
    PA_EVT_TIMER,       /* timer fired */
    PA_EVT_FRAME        /* frame timer */
} pa_evttype;

/* Special key codes */
typedef enum {
    PA_KEY_UP = 1,
    PA_KEY_DOWN,
    PA_KEY_LEFT,
    PA_KEY_RIGHT,
    PA_KEY_HOME,
    PA_KEY_END,
    PA_KEY_PAGEUP,
    PA_KEY_PAGEDOWN,
    PA_KEY_INSERT,
    PA_KEY_DELETE,
    PA_KEY_BACK,
    PA_KEY_ENTER,
    PA_KEY_TAB,
    PA_KEY_ESC,
    PA_KEY_F1,  PA_KEY_F2,  PA_KEY_F3,  PA_KEY_F4,
    PA_KEY_F5,  PA_KEY_F6,  PA_KEY_F7,  PA_KEY_F8,
    PA_KEY_F9,  PA_KEY_F10, PA_KEY_F11, PA_KEY_F12
} pa_keycode;

/* Raw event record passed from shim to graphics.c */
typedef struct {
    pa_evttype  type;
    pa_winhan   win;      /* which window */
    union {
        struct { uint32_t ch; }                        key;
        struct { pa_keycode code; }                    special;
        struct { int w, h; }                           resize;
        struct { int x, y; int buttons; }              mouse;
        struct { int id; }                             timer;
        struct { int x, y, w, h; }                    redraw;
    };
} pa_rawevent;

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * Initialisation / teardown
 *----------------------------------------------------------------------------*/

/* Must be called before any other pa_cocoa_* function.
   Sets up NSApplication (no-op if already done). */
void pa_cocoa_init(void);

/* Shut down — release all windows */
void pa_cocoa_deinit(void);

/*----------------------------------------------------------------------------
 * Screen queries
 *----------------------------------------------------------------------------*/

int pa_cocoa_screen_w(void);       /* screen width  in pixels  */
int pa_cocoa_screen_h(void);       /* screen height in pixels  */
int pa_cocoa_screen_wmm(void);     /* screen width  in mm      */
int pa_cocoa_screen_hmm(void);     /* screen height in mm      */

/*----------------------------------------------------------------------------
 * Window management
 *----------------------------------------------------------------------------*/

pa_winhan pa_cocoa_create_window(int x, int y, int w, int h,
                                  const char* title);
void pa_cocoa_destroy_window(pa_winhan win);
void pa_cocoa_show_window(pa_winhan win);
void pa_cocoa_hide_window(pa_winhan win);
void pa_cocoa_set_title(pa_winhan win, const char* title);
void pa_cocoa_move_window(pa_winhan win, int x, int y);
void pa_cocoa_resize_window(pa_winhan win, int w, int h);
void pa_cocoa_get_size(pa_winhan win, int* w, int* h);
void pa_cocoa_front(pa_winhan win);
void pa_cocoa_back(pa_winhan win);
void pa_cocoa_focus(pa_winhan win);
/* window chrome */
void pa_cocoa_set_frame(pa_winhan win, int on);
void pa_cocoa_set_sizable(pa_winhan win, int on);

/*----------------------------------------------------------------------------
 * Drawing — all drawing is into the offscreen bitmap context.
 * Call pa_cocoa_get_context() to obtain the CGContextRef, draw into it,
 * then call pa_cocoa_flush() to copy to the screen.
 *
 * The context origin is top-left (PA convention).  The shim applies a
 * flip transform so callers use (0,0) = top-left with Y increasing down.
 *----------------------------------------------------------------------------*/

CGContextRef pa_cocoa_get_context(pa_winhan win);
void         pa_cocoa_flush(pa_winhan win);       /* blit offscreen → screen */

/*----------------------------------------------------------------------------
 * Event polling — single-threaded poll model.
 * ami_event() calls pa_cocoa_poll() which drains the NSApplication event
 * queue into our internal queue, then returns one pa_rawevent.
 * Returns 1 if an event was dequeued, 0 if the queue is empty.
 *----------------------------------------------------------------------------*/

/* Process pending NSApp events (must be called periodically). */
void pa_cocoa_process_ns_events(void);

/* Dequeue one raw PA event; returns 1 on success, 0 if empty. */
int  pa_cocoa_dequeue(pa_rawevent* evt);

/* Block until at least one event is available, then dequeue it. */
void pa_cocoa_wait(pa_rawevent* evt);

/*----------------------------------------------------------------------------
 * Timers
 *----------------------------------------------------------------------------*/

void pa_cocoa_set_timer(pa_winhan win, int id, long ms, int repeat);
void pa_cocoa_kill_timer(pa_winhan win, int id);

/*----------------------------------------------------------------------------
 * Widgets — Cocoa native controls.
 * Each widget is identified by an integer id supplied by the caller.
 *----------------------------------------------------------------------------*/

void pa_cocoa_button(pa_winhan win, int x, int y, int w, int h,
                     const char* label, int id);
void pa_cocoa_checkbox(pa_winhan win, int x, int y, int w, int h,
                       const char* label, int id);
void pa_cocoa_radiobutton(pa_winhan win, int x, int y, int w, int h,
                          const char* label, int id);
void pa_cocoa_editbox(pa_winhan win, int x, int y, int w, int h, int id);
void pa_cocoa_listbox(pa_winhan win, int x, int y, int w, int h,
                      const char** items, int count, int id);
void pa_cocoa_scrollvert(pa_winhan win, int x, int y, int w, int h, int id);
void pa_cocoa_scrollhoriz(pa_winhan win, int x, int y, int w, int h, int id);
void pa_cocoa_slider_horiz(pa_winhan win, int x, int y, int w, int h,
                           int mark, int id);
void pa_cocoa_slider_vert(pa_winhan win, int x, int y, int w, int h,
                          int mark, int id);
void pa_cocoa_progressbar(pa_winhan win, int x, int y, int w, int h, int id);
void pa_cocoa_kill_widget(pa_winhan win, int id);
void pa_cocoa_widget_text(pa_winhan win, int id, const char* s);
void pa_cocoa_widget_get_text(pa_winhan win, int id, char* s, int sl);
void pa_cocoa_widget_enable(pa_winhan win, int id, int on);
void pa_cocoa_widget_select(pa_winhan win, int id, int on);
void pa_cocoa_scrollbar_pos(pa_winhan win, int id, int pos);
void pa_cocoa_scrollbar_siz(pa_winhan win, int id, int range);
void pa_cocoa_progressbar_pos(pa_winhan win, int id, int pos);

/*----------------------------------------------------------------------------
 * Dialogs
 *----------------------------------------------------------------------------*/

void pa_cocoa_alert(const char* title, const char* message);
void pa_cocoa_query_open(char* path, int pathlen);
void pa_cocoa_query_save(char* path, int pathlen);

/*----------------------------------------------------------------------------
 * Miscellaneous
 *----------------------------------------------------------------------------*/

/* Inject a close event into the event queue (used by signal handlers). */
void pa_cocoa_inject_close(void);

#ifdef __cplusplus
}
#endif

#endif /* __PA_COCOA_H__ */
