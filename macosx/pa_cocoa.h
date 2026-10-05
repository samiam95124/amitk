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
    PA_EVT_FRAME,       /* frame timer */
    PA_EVT_JOY_MOVE,    /* joystick axis moved */
    PA_EVT_JOY_DOWN,    /* joystick button pressed */
    PA_EVT_JOY_UP,      /* joystick button released */
    PA_EVT_MENU,        /* menu item selected */
    PA_EVT_MIN,         /* window minimized */
    PA_EVT_MAX,         /* window maximized/zoomed/fullscreen */
    PA_EVT_NORM,        /* window restored to normal */
    PA_EVT_WIDGET       /* widget activated (see pa_widgetact) */,
    PA_EVT_USER         /* program-sent event (ami_sendevent): user carries the record */
} pa_evttype;

/* widget activation kinds for PA_EVT_WIDGET */
typedef enum {
    PA_WIDGET_BUTTON = 1,    /* push button pressed */
    PA_WIDGET_CHECKBOX,      /* checkbox toggled */
    PA_WIDGET_RADIO,         /* radio button selected */
    PA_WIDGET_SCROLL_PAGEUP, /* scroll page up/left */
    PA_WIDGET_SCROLL_PAGEDN, /* scroll page down/right */
    PA_WIDGET_SCROLL_POS,    /* scroll thumb moved; pos 0..LONG_MAX */
    PA_WIDGET_SLIDER_POS,    /* slider moved; pos 0..LONG_MAX */
    PA_WIDGET_EDIT_DONE,     /* edit box completed (return pressed) */
    PA_WIDGET_NUM_DONE,      /* number select box changed; pos = value */
    PA_WIDGET_LIST_SEL,      /* list box selection; pos = 1-based index */
    PA_WIDGET_DROP_SEL,      /* drop box selection; pos = 1-based index */
    PA_WIDGET_DROPED_DONE,   /* drop edit box completed */
    PA_WIDGET_TAB_SEL        /* tab bar selection; pos = 1-based index */
} pa_widgetact;

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
        void*        user;    /* PA_EVT_USER: heap copy of the ami_evtrec, freed by the consumer */
        struct { uint32_t ch; }                        key;
        struct { pa_keycode code; }                    special;
        struct { int w, h; }                           resize;
        struct { int x, y; int buttons; }              mouse;
        struct { int id; }                             timer;
        struct { int x, y, w, h; }                    redraw;
        struct { int jn; int ax[6]; }                  joymove;
        struct { int jn; int btn; }                    joybtn;
        struct { int id; }                             menu;
        struct { int id; int act; long pos; }          widget;
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

double pa_cocoa_bitmap_scale(void); /* pixels to the point of the screen buffers */
int pa_cocoa_screen_w(void);       /* screen width  in pixels  */
int pa_cocoa_screen_h(void);       /* screen height in pixels  */
int pa_cocoa_screen_wmm(void);     /* screen width  in mm      */
int pa_cocoa_screen_hmm(void);     /* screen height in mm      */

/*----------------------------------------------------------------------------
 * Window management
 *----------------------------------------------------------------------------*/

pa_winhan pa_cocoa_create_window(int x, int y, int w, int h,
                                  const char* title);
/* Child windows are views embedded in the parent's content area:
   clipped to the parent, positioned relative to its top-left. */
pa_winhan pa_cocoa_create_child_window(pa_winhan parent,
                                        int x, int y, int w, int h);
void pa_cocoa_destroy_window(pa_winhan win);
void pa_cocoa_show_window(pa_winhan win);
void pa_cocoa_hide_window(pa_winhan win);
void pa_cocoa_set_title(pa_winhan win, const char* title);
void pa_cocoa_move_window(pa_winhan win, int x, int y);
void pa_cocoa_resize_window(pa_winhan win, int w, int h);
void pa_cocoa_get_size(pa_winhan win, int* w, int* h);
/* actual outer window size, chrome included */
void pa_cocoa_get_window_size(pa_winhan win, int* w, int* h);
/* chrome extra (window minus client) for the given frame flags */
void pa_cocoa_frame_extra(pa_winhan win, int frame, int size, int sysbar,
                          int* dw, int* dh);
void pa_cocoa_front(pa_winhan win);
void pa_cocoa_back(pa_winhan win);
void pa_cocoa_focus(pa_winhan win);
/* window chrome */
void pa_cocoa_set_frame(pa_winhan win, int on);
void pa_cocoa_set_sysbar(pa_winhan win, int on);
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
CGContextRef pa_cocoa_get_screen_context(pa_winhan win, int idx); /* any screen, 0-based */
void pa_cocoa_drag_window(pa_winhan win); /* native title-bar style drag from here */
void pa_cocoa_send_user_event(pa_winhan win, void* rec); /* ami_sendevent, any thread */
void         pa_cocoa_flush(pa_winhan win);       /* blit offscreen → screen */
void         pa_cocoa_select_screens(pa_winhan win, int upd, int dsp); /* 0-based */
void         pa_cocoa_set_cursor(pa_winhan win, int visible, int x, int y, int w, int h);
void         pa_cocoa_resize_bitmap(pa_winhan win, int w, int h);
void         pa_cocoa_set_bufmod(pa_winhan win, int on);
void         pa_cocoa_set_background(pa_winhan win, float r, float g, float b);

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

/* Start (or restart) timer id. The time is in 100 microsecond units, PA's
   timer unit (10000 = one second), as the implementation converts it; the
   callers pass PA timer values through unchanged. Not milliseconds. */
void pa_cocoa_set_timer(pa_winhan win, int id, long us100, int repeat);
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
void pa_cocoa_group_title_size(const char* s, int* w, int* h);
void pa_cocoa_group(pa_winhan win, int x, int y, int w, int h,
                    const char* label, int id);
void pa_cocoa_background(pa_winhan win, int x, int y, int w, int h, int id);
void pa_cocoa_editbox(pa_winhan win, int x, int y, int w, int h, int id);
void pa_cocoa_numselbox(pa_winhan win, int x, int y, int w, int h,
                        long l, long u, int id);
void pa_cocoa_listbox(pa_winhan win, int x, int y, int w, int h,
                      const char** items, int count, int id);
void pa_cocoa_dropbox(pa_winhan win, int x, int y, int w, int h,
                      const char** items, int count, int id);
void pa_cocoa_dropeditbox(pa_winhan win, int x, int y, int w, int h,
                          const char** items, int count, int id);
/* ori: 0 = top, 1 = right, 2 = bottom, 3 = left (matches ami_tabori);
   barh is the bar strip thickness in pixels (client = rect minus strip) */
void pa_cocoa_tabbar(pa_winhan win, int x, int y, int w, int h,
                     const char** items, int count, int ori, int barh,
                     int id);
void pa_cocoa_tabbar_sel(pa_winhan win, int id, int tn);
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
/* pos/range/ratio values are PA full scale, 0..LONG_MAX */
void pa_cocoa_scrollbar_pos(pa_winhan win, int id, long pos);
void pa_cocoa_scrollbar_siz(pa_winhan win, int id, long range);
void pa_cocoa_progressbar_pos(pa_winhan win, int id, long pos);

/*----------------------------------------------------------------------------
 * Menus — builds macOS menu bar from PA menu tree.
 * The menu_list parameter is an ami_menurec* cast to void*.
 *----------------------------------------------------------------------------*/

void pa_cocoa_menu(pa_winhan win, void* menu_list);
void pa_cocoa_menu_enable(pa_winhan win, int id, int on);
void pa_cocoa_menu_check(pa_winhan win, int id, int on);

/*----------------------------------------------------------------------------
 * Dialogs
 *----------------------------------------------------------------------------*/

void pa_cocoa_alert(const char* title, const char* message);
void pa_cocoa_query_open(char* path, int pathlen);
void pa_cocoa_query_save(char* path, int pathlen);
/* find/find-replace option bits, matching BIT(ami_qfnopt)/BIT(ami_qfropt) */
#define PA_QF_CASE   (1 << 0) /* case sensitive */
#define PA_QF_UP     (1 << 1) /* search up */
#define PA_QF_RE     (1 << 2) /* regular expression */
#define PA_QF_FIND   (1 << 3) /* find only (find/replace dialog) */
#define PA_QF_ALLFIL (1 << 4) /* replace all in file */
#define PA_QF_ALLLIN (1 << 5) /* replace all on line(s) */

/* color components are 0..LONG_MAX (PA scale); in/out */
void pa_cocoa_query_color(long* r, long* g, long* b);
/* find/find-replace dialogs; opt bits per ami_qfnopt/ami_qfropt */
void pa_cocoa_query_find(char* s, int sl, long* opt);
void pa_cocoa_query_findrep(char* s, int sl, char* r, int rl, long* opt);
/* font dialog: family name in/out, size in points, fg/bg 0..LONG_MAX */
void pa_cocoa_query_font(char* family, int famlen, long* size,
                         long* fr, long* fg, long* fb,
                         long* br, long* bg, long* bb);

/*----------------------------------------------------------------------------
 * Miscellaneous
 *----------------------------------------------------------------------------*/

/* Start threaded event delivery: moves user's main() to worker thread,
   keeps Cocoa event loop on the main thread. Never returns. */
void pa_cocoa_start_event_thread(void);
int  pa_entry(int argc, char** argv, char** envp, char** apple); /* linker -e */

/* Inject a close event into the event queue (used by signal handlers). */
void pa_cocoa_inject_close(void);

/* Joystick support via GameController framework */
void pa_cocoa_joy_init(void);
void pa_cocoa_joy_deinit(void);
int  pa_cocoa_joy_count(void);
int  pa_cocoa_joy_buttons(int j);
int  pa_cocoa_joy_axes(int j);

#ifdef __CORETEXT__
/* Get system monospace font (SF Mono) as CTFontRef. Caller must CFRelease. */
CTFontRef pa_cocoa_system_mono_font(CGFloat size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __PA_COCOA_H__ */
