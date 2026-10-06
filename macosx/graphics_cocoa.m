/*******************************************************************************
*                                                                              *
*                       Cocoa/Quartz shim for Ami graphics                     *
*                                                                              *
* Objective-C side of the macOS graphics backend.  Implements window          *
* creation, event translation, and offscreen bitmap management using           *
* Cocoa (AppKit) and Quartz (CoreGraphics).                                    *
*                                                                              *
* Threading model: single-threaded poll.  User code runs on the main thread.  *
* ami_event() calls pa_cocoa_process_ns_events() which drains the NSApp       *
* event queue into our internal ring buffer, then returns one event.           *
*                                                                              *
*******************************************************************************/

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <pthread.h>
#include <dlfcn.h>
#include <crt_externs.h>
#include <IOKit/hid/IOHIDManager.h>
#include "pa_cocoa.h"

/*----------------------------------------------------------------------------
 * Threading state
 *
 * In threaded mode the Cocoa event loop runs on the main thread
 * ([NSApp run] or equivalent), and the user program runs on a worker
 * thread.  The two communicate through the event ring buffer protected
 * by evt_mutex / evt_cond.
 *----------------------------------------------------------------------------*/

static pthread_mutex_t evt_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  evt_cond  = PTHREAD_COND_INITIALIZER;
static int             threaded_mode = 0;

static void run_on_main(dispatch_block_t block) {
    if (!threaded_mode || [NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

/* Associated object key for storing tag on views that lack a settable tag */
static char kPATagKey;
static void  pa_set_tag(NSView* v, int t) {
    objc_setAssociatedObject(v, &kPATagKey, @(t), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
static int   pa_get_tag(NSView* v) {
    NSNumber* n = objc_getAssociatedObject(v, &kPATagKey);
    return n ? n.intValue : (int)v.tag;
}

/*----------------------------------------------------------------------------
 * Event ring buffer
 *----------------------------------------------------------------------------*/

#define EVT_QUEUE_SIZE 256

static pa_rawevent  evt_queue[EVT_QUEUE_SIZE];
static int          evt_head = 0;   /* next write position */
static int          evt_tail = 0;   /* next read  position */

static void evt_push(const pa_rawevent* e)
{
    if (threaded_mode) pthread_mutex_lock(&evt_mutex);

    /* coalesce resize events: overwrite an existing resize for the same window */
    if (e->type == PA_EVT_RESIZE) {
        for (int i = evt_tail; i != evt_head; i = (i + 1) % EVT_QUEUE_SIZE) {
            if (evt_queue[i].type == PA_EVT_RESIZE && evt_queue[i].win == e->win) {
                evt_queue[i] = *e;
                if (threaded_mode) {
                    pthread_cond_signal(&evt_cond);
                    pthread_mutex_unlock(&evt_mutex);
                }
                return;
            }
        }
    }

    int next = (evt_head + 1) % EVT_QUEUE_SIZE;
    if (next == evt_tail) {
        if (threaded_mode) pthread_mutex_unlock(&evt_mutex);
        return;
    }
    evt_queue[evt_head] = *e;
    evt_head = next;

    if (threaded_mode) {
        pthread_cond_signal(&evt_cond);
        pthread_mutex_unlock(&evt_mutex);
    }
}

static int evt_pop(pa_rawevent* e)
{
    if (threaded_mode) pthread_mutex_lock(&evt_mutex);
    if (evt_head == evt_tail) {
        if (threaded_mode) pthread_mutex_unlock(&evt_mutex);
        return 0;
    }
    *e = evt_queue[evt_tail];
    evt_tail = (evt_tail + 1) % EVT_QUEUE_SIZE;
    if (threaded_mode) pthread_mutex_unlock(&evt_mutex);
    return 1;
}

static int evt_empty(void) {
    if (threaded_mode) {
        pthread_mutex_lock(&evt_mutex);
        int empty = (evt_head == evt_tail);
        pthread_mutex_unlock(&evt_mutex);
        return empty;
    }
    return evt_head == evt_tail;
}

/*----------------------------------------------------------------------------
 * PAView — custom NSView, owns the offscreen bitmap
 *----------------------------------------------------------------------------*/

@interface PAView : NSView {
@public
    CGContextRef  bitmap;      /* offscreen drawing surface (update screen) */
    CGContextRef  screens[10]; /* per-screen bitmaps (0-based, lazily created) */
    int           updscr;      /* current update screen (0-based) */
    int           dspscr;      /* current display screen (0-based) */
    int           bmpW;        /* bitmap width  in points */
    int           bmpH;        /* bitmap height in points */
    pa_winhan     owner;       /* back-pointer to PAWindow */
    int           curVisible;  /* cursor visible flag */
    int           curX, curY;  /* cursor position (0-based pixels) */
    int           curW, curH;  /* cursor size (pixels) */
    int           bufmod;      /* buffered mode flag */
    float         bgR, bgG, bgB; /* background color for margins */
    CGImageRef    displayImage;  /* snapshot for drawRect (threaded mode) */
    int           dispW, dispH;  /* dimensions of displayImage */
    double        lastPresent;   /* when the display was last snapshotted */
    int           presentPending; /* drawn since, not yet presented */
    pthread_mutex_t bufLock;     /* the screen buffers, between a resize
                                    and a present on another thread */
    int           retired;       /* the window is closed: no snapshot */
}
- (void)createBitmapWidth:(int)w height:(int)h;
- (void)destroyBitmap;
- (CGContextRef)ensureScreen:(int)idx;
@end

/* true when the PA window is a child (an embedded view with no NSWindow
   of its own); defined after PAWindow */
static int pa_win_is_child(pa_winhan h);

@implementation PAView

/* NSView.clipsToBounds defaults to NO since macOS 14, letting drawing
   escape the view's bounds and leave stale pixels behind when the view
   moves; restore the classic clipping behavior */
- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        if (@available(macOS 14.0, *)) self.clipsToBounds = YES;
        pthread_mutex_init(&bufLock, NULL);
    }
    return self;
}

- (BOOL)isFlipped { return YES; } /* make (0,0) top-left */
- (BOOL)acceptsFirstResponder { return YES; }

/* Child PA windows are views inside the parent's content view, so their
   focus is first-responder status, not NSWindow key status. Top-level
   windows report focus from windowDidBecomeKey/-ResignKey instead. */
- (BOOL)becomeFirstResponder
{
    if (owner && pa_win_is_child(owner)) {
        pa_rawevent e = {0};
        e.type = PA_EVT_FOCUS;
        e.win  = owner;
        evt_push(&e);
        [self.superview setNeedsDisplay:YES]; /* chrome button colors */
    }
    return YES;
}

- (BOOL)resignFirstResponder
{
    if (owner && pa_win_is_child(owner)) {
        pa_rawevent e = {0};
        e.type = PA_EVT_UNFOCUS;
        e.win  = owner;
        evt_push(&e);
        [self.superview setNeedsDisplay:YES]; /* chrome button colors */
    }
    return YES;
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    if ([self inLiveResize] && !bufmod) {
        int w = (int)newSize.width;
        int h = (int)newSize.height;
        if (w > 0 && h > 0 && (w != bmpW || h != bmpH)) {
            pa_rawevent e = {0};
            e.type     = PA_EVT_RESIZE;
            e.win      = owner;
            e.resize.w = w;
            e.resize.h = h;
            evt_push(&e);
            pa_rawevent r = {0};
            r.type     = PA_EVT_REDRAW;
            r.win      = owner;
            r.redraw.w = w;
            r.redraw.h = h;
            evt_push(&r);
        }
        [self setNeedsDisplay:YES];
    }
}

- (void)createBitmapWidth:(int)w height:(int)h
{
    /* release all existing screen bitmaps */
    for (int i = 0; i < 10; i++) {
        if (screens[i]) { CGContextRelease(screens[i]); screens[i] = NULL; }
    }
    bitmap = NULL;

    bmpW = w;
    bmpH = h;
    updscr = 0;
    dspscr = 0;

    /* create screen 0 as the default */
    screens[0] = [self createOneBitmapWidth:w height:h];
    bitmap = screens[0];
}

- (CGContextRef)createOneBitmapWidth:(int)w height:(int)h
{
    /* Every screen buffer is at the one scale, the main screen's backing
       scale, found once at the first buffer: a Retina screen's two pixels
       to the point, so that text and lines are drawn at the screen's own
       resolution rather than at one pixel to the point and doubled on the
       way to the window, which left the text soft beside the terminal's.
       The scale is taken once, as a buffer made while the window is not
       yet attached must match one made after it is: keyed to the window,
       the page flip once alternated between fitted and doubled frames. */
    CGFloat       scale = pa_cocoa_bitmap_scale();
    int           pw    = (int)(w * scale);
    int           ph    = (int)(h * scale);
    CGColorSpaceRef cs  = CGColorSpaceCreateDeviceRGB();

    CGContextRef ctx = CGBitmapContextCreate(NULL, pw, ph, 8, pw * 4, cs,
                                             kCGImageAlphaPremultipliedFirst |
                                             kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(cs);
    CGContextScaleCTM(ctx, scale, scale);
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
    return ctx;
}

- (CGContextRef)ensureScreen:(int)idx
{
    if (idx < 0 || idx >= 10) return screens[0];
    if (!screens[idx])
        screens[idx] = [self createOneBitmapWidth:bmpW height:bmpH];
    return screens[idx];
}

- (void)destroyBitmap
{
    for (int i = 0; i < 10; i++) {
        if (screens[i]) { CGContextRelease(screens[i]); screens[i] = NULL; }
    }
    bitmap = NULL;
}

- (void)drawRect:(NSRect)dirtyRect
{
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    int viewW = (int)self.bounds.size.width;
    int viewH = (int)self.bounds.size.height;

    /* In threaded mode, use the pre-rendered snapshot */
    CGImageRef img;
    int imgW, imgH;
    if (threaded_mode) {
        pthread_mutex_lock(&evt_mutex);
        img = displayImage;
        if (!img) { pthread_mutex_unlock(&evt_mutex); return; }
        CGImageRetain(img);
        imgW = dispW;
        imgH = dispH;
        pthread_mutex_unlock(&evt_mutex);
    } else {
        CGContextRef dsp = screens[dspscr];
        if (!dsp) return;
        img = CGBitmapContextCreateImage(dsp);
        imgW = bmpW;
        imgH = bmpH;
    }

    if ([self inLiveResize] && !bufmod && (imgW != viewW || imgH != viewH)) {
        /* scale last content to fill window during live resize */
        CGContextDrawImage(ctx, CGRectMake(0, 0, viewW, viewH), img);
    } else {
        /* draw bitmap at 1:1, clipped to view bounds */
        CGContextSaveGState(ctx);
        if (viewW < imgW || viewH < imgH) {
            int clipW = viewW < imgW ? viewW : imgW;
            int clipH = viewH < imgH ? viewH : imgH;
            CGContextClipToRect(ctx, CGRectMake(0, 0, clipW, clipH));
        }
        CGContextDrawImage(ctx, CGRectMake(0, 0, imgW, imgH), img);
        CGContextRestoreGState(ctx);

        if (imgW < viewW || imgH < viewH) {
            CGContextSetRGBFillColor(ctx, bgR, bgG, bgB, 1.0);
            if (imgW < viewW)
                CGContextFillRect(ctx, CGRectMake(imgW, 0,
                                                  viewW - imgW, viewH));
            if (imgH < viewH)
                CGContextFillRect(ctx, CGRectMake(0, imgH,
                                                  imgW, viewH - imgH));
        }
    }
    CGImageRelease(img);

    if (curVisible && curW > 0 && curH > 0) {
        CGContextSetBlendMode(ctx, kCGBlendModeDifference);
        CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
        CGContextFillRect(ctx, CGRectMake(curX, curY, curW, curH));
    }
}

- (void)viewDidEndLiveResize
{
    [super viewDidEndLiveResize];
    NSSize s = self.bounds.size;
    int w = (int)s.width;
    int h = (int)s.height;
    if (!threaded_mode && !bufmod && (w != bmpW || h != bmpH)) {
        [self createBitmapWidth:w height:h];
    }
    pa_rawevent e = {0};
    e.type       = PA_EVT_RESIZE;
    e.win        = owner;
    e.resize.w   = w;
    e.resize.h   = h;
    evt_push(&e);
    if (!bufmod) {
        pa_rawevent r = {0};
        r.type     = PA_EVT_REDRAW;
        r.win      = owner;
        r.redraw.w = w;
        r.redraw.h = h;
        evt_push(&r);
    }
    [self setNeedsDisplay:YES];
}

/*--- Keyboard ---*/

static uint32_t translate_key(NSEvent* event, pa_keycode* out_special)
{
    *out_special = 0;
    unsigned short kc = event.keyCode;
    /* map common special keys */
    switch (kc) {
        case 126: *out_special = PA_KEY_UP;       return 0;
        case 125: *out_special = PA_KEY_DOWN;      return 0;
        case 123: *out_special = PA_KEY_LEFT;      return 0;
        case 124: *out_special = PA_KEY_RIGHT;     return 0;
        case 115: *out_special = PA_KEY_HOME;      return 0;
        case 119: *out_special = PA_KEY_END;       return 0;
        case 116: *out_special = PA_KEY_PAGEUP;    return 0;
        case 121: *out_special = PA_KEY_PAGEDOWN;  return 0;
        case 117: *out_special = PA_KEY_DELETE;    return 0;
        case 51:  *out_special = PA_KEY_BACK;      return 0;
        case 36:  *out_special = PA_KEY_ENTER;     return 0;
        case 48:  *out_special = PA_KEY_TAB;       return 0;
        case 53:  *out_special = PA_KEY_ESC;       return 0;
        case 122: *out_special = PA_KEY_F1;        return 0;
        case 120: *out_special = PA_KEY_F2;        return 0;
        case 99:  *out_special = PA_KEY_F3;        return 0;
        case 118: *out_special = PA_KEY_F4;        return 0;
        case 96:  *out_special = PA_KEY_F5;        return 0;
        case 97:  *out_special = PA_KEY_F6;        return 0;
        case 98:  *out_special = PA_KEY_F7;        return 0;
        case 100: *out_special = PA_KEY_F8;        return 0;
        case 101: *out_special = PA_KEY_F9;        return 0;
        case 109: *out_special = PA_KEY_F10;       return 0;
        case 103: *out_special = PA_KEY_F11;       return 0;
        case 111: *out_special = PA_KEY_F12;       return 0;
    }
    /* regular character */
    NSString* chars = event.characters;
    if (chars.length > 0) return [chars characterAtIndex:0];
    return 0;
}

- (void)keyDown:(NSEvent*)event
{
    pa_keycode   special;
    uint32_t     ch = translate_key(event, &special);
    pa_rawevent  e  = {0};
    e.win = owner;
    if (special) {
        e.type         = PA_EVT_KEYDOWN;
        e.special.code = special;
    } else if (ch) {
        e.type   = PA_EVT_CHAR;
        e.key.ch = ch;
    } else return;
    evt_push(&e);
}

/*--- Mouse ---*/

- (NSPoint)flipPoint:(NSPoint)p
{
    /* isFlipped is YES so AppKit already gives us top-left coords */
    return p;
}

- (void)pushMouseEvent:(pa_evttype)type event:(NSEvent*)event
{
    NSPoint      p = [self convertPoint:event.locationInWindow fromView:nil];
    pa_rawevent  e = {0};
    e.type         = type;
    e.win          = owner;
    e.mouse.x      = (int)p.x + 1; /* PA is 1-based */
    e.mouse.y      = (int)p.y + 1;
    e.mouse.buttons = (int)event.buttonNumber + 1;
    evt_push(&e);
}

- (void)mouseMoved:(NSEvent*)e      { [self pushMouseEvent:PA_EVT_MOUSE_MOVE event:e]; }
- (void)mouseDragged:(NSEvent*)e    { [self pushMouseEvent:PA_EVT_MOUSE_MOVE event:e]; }
- (void)mouseDown:(NSEvent*)e
{
    [self.window makeFirstResponder:self]; /* clicking a child view focuses it */
    [self pushMouseEvent:PA_EVT_MOUSE_DOWN event:e];
}
- (void)mouseUp:(NSEvent*)e         { [self pushMouseEvent:PA_EVT_MOUSE_UP   event:e]; }
- (void)rightMouseDown:(NSEvent*)e  { [self pushMouseEvent:PA_EVT_MOUSE_DOWN event:e]; }
- (void)rightMouseUp:(NSEvent*)e    { [self pushMouseEvent:PA_EVT_MOUSE_UP   event:e]; }

@end

/*----------------------------------------------------------------------------
 * PAChildFrame — Ami-drawn chrome for child windows.
 *
 * Cocoa has no child windows in the Win32 sense, so a PA child window is a
 * view embedded in the parent's content view, and the frame chrome (title
 * bar, close/minimize/zoom buttons, resize borders) is drawn by us — the
 * same approach as the Linux implementation's Ami-drawn child frames.
 * The container holds the client PAView inset by the chrome metrics.
 *----------------------------------------------------------------------------*/

#define CFRM_TITBAR   24  /* title bar height */
#define CFRM_BORDER    4  /* resize border width */
#define CFRM_CORNER   16  /* corner resize grab length */
#define CFRM_BTN      12  /* button diameter */
#define CFRM_BTN_GAP   8  /* gap between buttons */
#define CFRM_BTN_MG    8  /* button margin from edge */
#define CFRM_MIN_W   120  /* minimum child window width */
#define CFRM_MIN_H   (CFRM_TITBAR + 2 * CFRM_BORDER) /* minimum height */
#define CFRM_MINWIN_W 200 /* width of a minimized child window */

/* resize edge bits */
#define CFRM_EDGE_L 1
#define CFRM_EDGE_R 2
#define CFRM_EDGE_T 4
#define CFRM_EDGE_B 8

@interface PAChildFrame : NSView {
@public
    pa_winhan ownerHan;    /* owning PAWindow */
    PAView*   client;      /* the client-area view */
    NSString* title;       /* title bar text */
    int  frameOn;          /* draw any chrome at all */
    int  sysbarOn;         /* draw the title bar */
    int  sizableOn;        /* border resize enabled */
    int  dragging;         /* moving via title bar */
    int  resizing;         /* resize edge mask, 0 = not resizing */
    NSPoint dragStart;     /* mouse position at drag start (superview coords) */
    NSRect  frameStart;    /* our frame at drag start */
    int  minimized;
    NSRect restoreMin;     /* frame to restore after minimize */
    int  maximized;
    NSRect restoreMax;     /* frame to restore after maximize */
}
- (void)layoutClient;
- (void)setClientSize:(NSSize)size;
@end

@implementation PAChildFrame

/* see PAView initWithFrame: — clip drawing to bounds (macOS 14 default NO) */
- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        if (@available(macOS 14.0, *)) self.clipsToBounds = YES;
    }
    return self;
}

- (BOOL)isFlipped { return YES; }

/* chrome insets around the client area */
- (NSEdgeInsets)chromeInsets
{
    if (!frameOn) return NSEdgeInsetsMake(0, 0, 0, 0);
    return NSEdgeInsetsMake(sysbarOn ? CFRM_TITBAR : CFRM_BORDER,
                            CFRM_BORDER, CFRM_BORDER, CFRM_BORDER);
}

- (void)layoutClient
{
    NSEdgeInsets in = [self chromeInsets];
    NSRect b = self.bounds;
    client.frame = NSMakeRect(in.left, in.top,
                              b.size.width - in.left - in.right,
                              b.size.height - in.top - in.bottom);
}

/* resize the container so the client area has the given size,
   keeping the top-left corner in place */
- (void)setClientSize:(NSSize)size
{
    NSEdgeInsets in = [self chromeInsets];
    [self setFrameSize:NSMakeSize(size.width + in.left + in.right,
                                  size.height + in.top + in.bottom)];
    [self layoutClient];
    [self.superview setNeedsDisplay:YES];
}

- (BOOL)clientFocused
{
    return self.window.firstResponder == client &&
           self.window.isKeyWindow;
}

- (NSRect)buttonRect:(int)i /* 0 = close, 1 = minimize, 2 = zoom */
{
    return NSMakeRect(CFRM_BTN_MG + i * (CFRM_BTN + CFRM_BTN_GAP),
                      (CFRM_TITBAR - CFRM_BTN) / 2, CFRM_BTN, CFRM_BTN);
}

- (void)drawRect:(NSRect)dirtyRect
{
    if (!frameOn) return;
    NSRect b = self.bounds;

    /* border + bar background */
    [[NSColor colorWithWhite:0.82 alpha:1.0] setFill];
    NSRectFill(b);

    if (!sysbarOn) return;

    /* title bar */
    [[NSColor colorWithWhite:0.85 alpha:1.0] setFill];
    NSRectFill(NSMakeRect(0, 0, b.size.width, CFRM_TITBAR));
    [[NSColor colorWithWhite:0.70 alpha:1.0] setFill];
    NSRectFill(NSMakeRect(0, CFRM_TITBAR - 1, b.size.width, 1));

    /* traffic-light buttons: close, minimize, zoom */
    int focused = [self clientFocused];
    NSColor* cols[3];
    if (focused) {
        cols[0] = [NSColor colorWithRed:1.00 green:0.37 blue:0.34 alpha:1];
        cols[1] = [NSColor colorWithRed:1.00 green:0.74 blue:0.18 alpha:1];
        cols[2] = [NSColor colorWithRed:0.16 green:0.78 blue:0.25 alpha:1];
    } else {
        cols[0] = cols[1] = cols[2] = [NSColor colorWithWhite:0.75 alpha:1];
    }
    for (int i = 0; i < 3; i++) {
        NSBezierPath* p = [NSBezierPath bezierPathWithOvalInRect:
                              [self buttonRect:i]];
        [cols[i] setFill];
        [p fill];
        [[NSColor colorWithWhite:0.55 alpha:1] setStroke];
        [p setLineWidth:0.5];
        [p stroke];
    }

    /* centered title, truncated with an ellipsis if needed */
    if (title.length) {
        NSMutableParagraphStyle* ps = [[NSMutableParagraphStyle alloc] init];
        ps.alignment = NSTextAlignmentCenter;
        ps.lineBreakMode = NSLineBreakByTruncatingTail;
        NSDictionary* at = @{
            NSFontAttributeName: [NSFont systemFontOfSize:12],
            NSForegroundColorAttributeName:
                focused ? [NSColor blackColor]
                        : [NSColor colorWithWhite:0.45 alpha:1],
            NSParagraphStyleAttributeName: ps
        };
        CGFloat lx = NSMaxX([self buttonRect:2]) + CFRM_BTN_GAP;
        NSRect tr = NSMakeRect(lx, (CFRM_TITBAR - 15) / 2,
                               b.size.width - 2 * lx, 16);
        [title drawInRect:tr withAttributes:at];
    }
}

/* which resize edges is (p) over? p in local flipped coords */
- (int)edgeMaskAt:(NSPoint)p
{
    if (!frameOn || !sizableOn) return 0;
    NSRect b = self.bounds;
    int m = 0;
    int nearL = p.x < CFRM_BORDER;
    int nearR = p.x > b.size.width - CFRM_BORDER;
    int nearT = p.y < CFRM_BORDER;
    int nearB = p.y > b.size.height - CFRM_BORDER;
    /* widen edge bands near corners so diagonals are grabbable */
    if (nearL && p.y < CFRM_CORNER) nearT = 1;
    if (nearR && p.y < CFRM_CORNER) nearT = 1;
    if (nearL && p.y > b.size.height - CFRM_CORNER) nearB = 1;
    if (nearR && p.y > b.size.height - CFRM_CORNER) nearB = 1;
    if (nearT && p.x < CFRM_CORNER) nearL = 1;
    if (nearB && p.x < CFRM_CORNER) nearL = 1;
    if (nearT && p.x > b.size.width - CFRM_CORNER) nearR = 1;
    if (nearB && p.x > b.size.width - CFRM_CORNER) nearR = 1;
    if (nearL) m |= CFRM_EDGE_L;
    if (nearR) m |= CFRM_EDGE_R;
    if (nearT) m |= CFRM_EDGE_T;
    if (nearB) m |= CFRM_EDGE_B;
    return m;
}

- (void)pushResize
{
    pa_rawevent e = {0};
    e.type     = PA_EVT_RESIZE;
    e.win      = ownerHan;
    e.resize.w = (int)client.frame.size.width;
    e.resize.h = (int)client.frame.size.height;
    evt_push(&e);
}

- (void)toggleMinimize
{
    if (minimized) {
        self.frame = restoreMin;
        client.hidden = NO;
        minimized = 0;
    } else {
        restoreMin = self.frame;
        /* arrange minimized windows along the parent's bottom edge,
           taskbar style: take the first free slot from the left */
        int slot = 0;
        for (int taken = 1; taken; slot += taken) {
            taken = 0;
            for (NSView* v in self.superview.subviews)
                if (v != self && [v isKindOfClass:[PAChildFrame class]] &&
                    ((PAChildFrame*)v)->minimized &&
                    (int)v.frame.origin.x ==
                        slot * (CFRM_MINWIN_W + CFRM_BORDER))
                    taken = 1;
        }
        NSRect pb = self.superview.bounds;
        self.frame = NSMakeRect(slot * (CFRM_MINWIN_W + CFRM_BORDER),
                                pb.size.height - CFRM_TITBAR,
                                CFRM_MINWIN_W, CFRM_TITBAR);
        client.hidden = YES;
        minimized = 1;
    }
    [self layoutClient];
    [self.window invalidateCursorRectsForView:self];
    [self.superview setNeedsDisplay:YES];
}

- (void)toggleMaximize
{
    if (minimized) [self toggleMinimize];
    if (maximized) {
        self.frame = restoreMax;
        maximized = 0;
    } else {
        restoreMax = self.frame;
        self.frame = self.superview.bounds;
        maximized = 1;
    }
    [self layoutClient];
    [self.superview setNeedsDisplay:YES];
    [self pushResize];
}

- (void)mouseDown:(NSEvent*)e
{
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];

    int m = [self edgeMaskAt:p];
    if (m && !minimized) {
        resizing   = m;
        dragStart  = [self.superview convertPoint:e.locationInWindow
                                         fromView:nil];
        frameStart = self.frame;
        return;
    }

    if (frameOn && sysbarOn && p.y < CFRM_TITBAR) {
        [self.window makeFirstResponder:client]; /* clicking chrome focuses */
        if (NSPointInRect(p, [self buttonRect:0])) {
            pa_rawevent ev = {0};
            ev.type = PA_EVT_CLOSE;
            ev.win  = ownerHan;
            evt_push(&ev);
            return;
        }
        if (NSPointInRect(p, [self buttonRect:1])) { [self toggleMinimize]; return; }
        if (NSPointInRect(p, [self buttonRect:2])) { [self toggleMaximize]; return; }
        dragging   = 1;
        dragStart  = [self.superview convertPoint:e.locationInWindow
                                         fromView:nil];
        frameStart = self.frame;
    }
}

- (void)mouseDragged:(NSEvent*)e
{
    NSPoint p = [self.superview convertPoint:e.locationInWindow fromView:nil];
    CGFloat dx = p.x - dragStart.x;
    CGFloat dy = p.y - dragStart.y;

    if (dragging) {
        [self setFrameOrigin:NSMakePoint(frameStart.origin.x + dx,
                                         frameStart.origin.y + dy)];
        [self.superview setNeedsDisplay:YES];
    } else if (resizing) {
        NSRect f = frameStart; /* flipped superview: origin = top-left */
        if (resizing & CFRM_EDGE_L) { f.origin.x += dx; f.size.width  -= dx; }
        if (resizing & CFRM_EDGE_R) { f.size.width  += dx; }
        if (resizing & CFRM_EDGE_T) { f.origin.y += dy; f.size.height -= dy; }
        if (resizing & CFRM_EDGE_B) { f.size.height += dy; }
        if (f.size.width  < CFRM_MIN_W) f.size.width  = CFRM_MIN_W;
        if (f.size.height < CFRM_MIN_H) f.size.height = CFRM_MIN_H;
        self.frame = f;
        [self layoutClient];
        [self setNeedsDisplay:YES];
        [self.superview setNeedsDisplay:YES];
    }
}

- (void)mouseUp:(NSEvent*)e
{
    if (resizing) [self pushResize];
    dragging = 0;
    resizing = 0;
}

/* diagonal resize cursor; nwse selects the top-left/bottom-right axis.
   The frame-resize cursors are macOS 15+; fall back to a crosshair. */
static NSCursor* diag_cursor(int nwse)
{
    if (@available(macOS 15.0, *)) {
        return [NSCursor frameResizeCursorFromPosition:
                    (nwse ? NSCursorFrameResizePositionTopLeft
                          : NSCursorFrameResizePositionTopRight)
                             inDirections:NSCursorFrameResizeDirectionsAll];
    }
    return [NSCursor crosshairCursor];
}

- (void)resetCursorRects
{
    if (!frameOn || !sizableOn || minimized) return;
    NSRect b = self.bounds;
    CGFloat w = b.size.width, h = b.size.height;

    /* edges */
    [self addCursorRect:NSMakeRect(0, 0, CFRM_BORDER, h)
                 cursor:[NSCursor resizeLeftRightCursor]];
    [self addCursorRect:NSMakeRect(w - CFRM_BORDER, 0, CFRM_BORDER, h)
                 cursor:[NSCursor resizeLeftRightCursor]];
    [self addCursorRect:NSMakeRect(0, h - CFRM_BORDER, w, CFRM_BORDER)
                 cursor:[NSCursor resizeUpDownCursor]];
    [self addCursorRect:NSMakeRect(0, 0, w, CFRM_BORDER)
                 cursor:[NSCursor resizeUpDownCursor]];

    /* corners: L-shaped pairs matching the widened grab bands, added last
       so they win over the straight edge bands. A full square would cover
       the title bar buttons with a resize cursor. */
    [self addCursorRect:NSMakeRect(0, 0, CFRM_BORDER, CFRM_CORNER)
                 cursor:diag_cursor(1)];
    [self addCursorRect:NSMakeRect(0, 0, CFRM_CORNER, CFRM_BORDER)
                 cursor:diag_cursor(1)];
    [self addCursorRect:NSMakeRect(w - CFRM_BORDER, h - CFRM_CORNER,
                                   CFRM_BORDER, CFRM_CORNER)
                 cursor:diag_cursor(1)];
    [self addCursorRect:NSMakeRect(w - CFRM_CORNER, h - CFRM_BORDER,
                                   CFRM_CORNER, CFRM_BORDER)
                 cursor:diag_cursor(1)];
    [self addCursorRect:NSMakeRect(w - CFRM_BORDER, 0,
                                   CFRM_BORDER, CFRM_CORNER)
                 cursor:diag_cursor(0)];
    [self addCursorRect:NSMakeRect(w - CFRM_CORNER, 0,
                                   CFRM_CORNER, CFRM_BORDER)
                 cursor:diag_cursor(0)];
    [self addCursorRect:NSMakeRect(0, h - CFRM_CORNER,
                                   CFRM_BORDER, CFRM_CORNER)
                 cursor:diag_cursor(0)];
    [self addCursorRect:NSMakeRect(0, h - CFRM_BORDER,
                                   CFRM_CORNER, CFRM_BORDER)
                 cursor:diag_cursor(0)];
}

@end

/*----------------------------------------------------------------------------
 * PAKeyWindow — NSWindow subclass that accepts key status even when borderless
 *----------------------------------------------------------------------------*/

@interface PAKeyWindow : NSWindow
@property (nonatomic) BOOL suppressResignKey;
@end

@implementation PAKeyWindow
- (BOOL)canBecomeKeyWindow  { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
- (void)resignKeyWindow {
    if (!self.suppressResignKey) [super resignKeyWindow];
}
- (void)resignMainWindow {
    if (!self.suppressResignKey) [super resignMainWindow];
}
@end

/*----------------------------------------------------------------------------
 * PAWindow — wraps NSWindow + PAView + timers
 *----------------------------------------------------------------------------*/

@interface PAWindow : NSObject <NSWindowDelegate> {
@public
    NSWindow*      window;  /* nil for child windows (embedded views) */
    PAView*        view;    /* client area */
    PAChildFrame*  cframe;  /* chrome container; child windows only */
    int            winid;   /* PA window id */
    int            wasZoomed; /* last observed isZoomed state */
    NSTimer*       timers[10]; /* up to 10 per-window timers */
}
- (instancetype)initWithX:(int)x y:(int)y width:(int)w height:(int)h
                    title:(const char*)title;
- (instancetype)initChildInParent:(PAWindow*)parent
                                x:(int)x y:(int)y width:(int)w height:(int)h;
@end

@implementation PAWindow

- (instancetype)initWithX:(int)x y:(int)y width:(int)w height:(int)h
                    title:(const char*)title
{
    self = [super init];
    if (!self) return nil;

    NSRect frame = NSMakeRect(x, y, w, h);
    NSWindowStyleMask style = NSWindowStyleMaskTitled |
                              NSWindowStyleMaskClosable |
                              NSWindowStyleMaskMiniaturizable |
                              NSWindowStyleMaskResizable;

    window = [[PAKeyWindow alloc] initWithContentRect:frame
                                            styleMask:style
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
    window.releasedWhenClosed = NO;
    if (title) window.title = [NSString stringWithUTF8String:title];
    window.delegate = self;
    window.acceptsMouseMovedEvents = YES;

    view = [[PAView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    view->owner = (__bridge pa_winhan)self;
    [view createBitmapWidth:w height:h];
    window.contentView = view;

    memset(timers, 0, sizeof(timers));
    return self;
}

/* A child PA window has no NSWindow: it is a PAView embedded in the
   parent's content view via a PAChildFrame container that draws the
   chrome. It is clipped to the parent, moves with it, and z-orders
   among its sibling views. The parent view is flipped, so (x,y) is the
   offset from the parent's top-left; (w,h) is the client size. */
- (instancetype)initChildInParent:(PAWindow*)parent
                                x:(int)x y:(int)y width:(int)w height:(int)h
{
    self = [super init];
    if (!self) return nil;

    window = nil;
    view = [[PAView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    view->owner = (__bridge pa_winhan)self;
    [view createBitmapWidth:w height:h];

    cframe = [[PAChildFrame alloc] initWithFrame:
                 NSMakeRect(x, y,
                            w + 2 * CFRM_BORDER,
                            h + CFRM_TITBAR + CFRM_BORDER)];
    cframe->ownerHan  = (__bridge pa_winhan)self;
    cframe->client    = view;
    cframe->title     = @"";
    cframe->frameOn   = 1;
    cframe->sysbarOn  = 1;
    cframe->sizableOn = 1;
    [cframe addSubview:view];
    [cframe layoutClient];
    [parent->view addSubview:cframe];

    memset(timers, 0, sizeof(timers));
    return self;
}

/*--- NSWindowDelegate ---*/

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    pa_rawevent e = {0};
    e.type = PA_EVT_CLOSE;
    e.win  = (__bridge pa_winhan)self;
    evt_push(&e);
    return NO; /* let PA handle it */
}

- (void)windowDidBecomeKey:(NSNotification*)n
{
    pa_rawevent e = {0};
    e.type = PA_EVT_FOCUS;
    e.win  = (__bridge pa_winhan)self;
    evt_push(&e);
}

- (void)windowDidResignKey:(NSNotification*)n
{
    pa_rawevent e = {0};
    e.type = PA_EVT_UNFOCUS;
    e.win  = (__bridge pa_winhan)self;
    evt_push(&e);
}

/*--- minimize / maximize / restore ---*/

- (void)pushStateEvent:(pa_evttype)t
{
    pa_rawevent e = {0};
    e.type = t;
    e.win  = (__bridge pa_winhan)self;
    evt_push(&e);
}

- (void)windowDidMiniaturize:(NSNotification*)n
{
    [self pushStateEvent:PA_EVT_MIN];
}

- (void)windowDidDeminiaturize:(NSNotification*)n
{
    [self pushStateEvent:PA_EVT_NORM];
}

/* the green button enters fullscreen on modern macOS */
- (void)windowDidEnterFullScreen:(NSNotification*)n
{
    [self pushStateEvent:PA_EVT_MAX];
}

- (void)windowDidExitFullScreen:(NSNotification*)n
{
    [self pushStateEvent:PA_EVT_NORM];
}

/* zoom (option-green or title bar double-click) has no dedicated
   notification; watch isZoomed transitions across resizes */
- (void)windowDidResize:(NSNotification*)n
{
    if (window.styleMask & NSWindowStyleMaskFullScreen)
        return; /* fullscreen is reported by its own notifications */
    int z = window.isZoomed ? 1 : 0;
    if (z != wasZoomed) {
        wasZoomed = z;
        [self pushStateEvent:z ? PA_EVT_MAX : PA_EVT_NORM];
    }
}

/*--- Timer support ---*/

- (void)timerFired:(NSTimer*)t
{
    int tid = [(NSNumber*)t.userInfo intValue];
    pa_rawevent e = {0};
    e.type    = PA_EVT_TIMER;
    e.win     = (__bridge pa_winhan)self;
    e.timer.id = tid;
    evt_push(&e);
}

@end

static int pa_win_is_child(pa_winhan h)
{
    return ((__bridge PAWindow*)h)->window == nil;
}

/*----------------------------------------------------------------------------
 * Global NSApplication setup
 *----------------------------------------------------------------------------*/

static BOOL app_inited = NO;

void pa_cocoa_init(void)
{
    if (app_inited) return;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];
    app_inited = YES;
}

void pa_cocoa_deinit(void) { /* nothing for now */ }

/*----------------------------------------------------------------------------
 * Thread transition — move user's main() to a worker thread so the
 * main thread can run the Cocoa event loop.  This mirrors the Windows
 * architecture where the display thread owns the message pump and the
 * user's thread blocks on WaitForSingleObject.
 *----------------------------------------------------------------------------*/

typedef int (*pa_main_func_t)(int, char**);

static void* pa_worker_func(void* arg)
{
    pa_main_func_t user_main = (pa_main_func_t)arg;
    int argc  = *_NSGetArgc();
    char** argv = *_NSGetArgv();
    int result = user_main(argc, argv);
    exit(result);
    return NULL;
}

/* Program entry, set by the linker for every Darwin graphics program (the
   -e _pa_entry in the Makefile GLIBS). dyld calls it only after every
   initializer in the image has run. The main-thread capture cannot live in
   a constructor: the Cocoa event loop never returns, so dyld would stall
   inside it and no constructor linked after this backend would ever run.
   From here they all have, so overriders sit above the backend as intended.
   Only the single-threaded fallback returns from the capture; main runs
   here in that case. */
int pa_entry(int argc, char** argv, char** envp, char** apple)
{
    (void)envp; (void)apple;
    pa_cocoa_start_event_thread();
    pa_main_func_t user_main = (pa_main_func_t)dlsym(RTLD_DEFAULT, "main");
    return user_main ? user_main(argc, argv) : 1;
}

void pa_cocoa_start_event_thread(void)
{
    pa_main_func_t user_main = (pa_main_func_t)dlsym(RTLD_DEFAULT, "main");
    if (!user_main) {
        fprintf(stderr, "pa_cocoa: cannot find main() — single-threaded fallback\n");
        return;
    }

    threaded_mode = 1;

    pthread_t worker;
    pthread_create(&worker, NULL, pa_worker_func, (void*)user_main);
    pthread_detach(worker);

    /* Main thread enters the Cocoa event loop — never returns.
       The worker thread calls main() which eventually calls exit(). */
    while (1) {
        @autoreleasepool {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantFuture]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
                [NSApp updateWindows];
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Screen queries
 *----------------------------------------------------------------------------*/

/* the scale of the screen buffers, pixels to the point: the main screen's
   backing scale, taken once */
double pa_cocoa_bitmap_scale(void)
{
    static CGFloat scale = 0;
    if (scale <= 0) {
        NSScreen* scr = [NSScreen mainScreen];
        scale = scr ? scr.backingScaleFactor : 1.0;
        if (scale < 1.0) scale = 1.0;
    }
    return scale;
}

int pa_cocoa_screen_w(void)
{
    return (int)[NSScreen mainScreen].frame.size.width;
}
int pa_cocoa_screen_h(void)
{
    return (int)[NSScreen mainScreen].frame.size.height;
}
int pa_cocoa_screen_wmm(void)
{
    /* use points-per-inch from screen description */
    CGSize sz = CGDisplayScreenSize(CGMainDisplayID());
    return (int)sz.width;
}
int pa_cocoa_screen_hmm(void)
{
    CGSize sz = CGDisplayScreenSize(CGMainDisplayID());
    return (int)sz.height;
}

/*----------------------------------------------------------------------------
 * Window management
 *----------------------------------------------------------------------------*/

pa_winhan pa_cocoa_create_window(int x, int y, int w, int h,
                                  const char* title)
{
    __block pa_winhan result;
    run_on_main(^{
        pa_cocoa_init();
        int screeny = pa_cocoa_screen_h() - y - h;
        PAWindow* pw = [[PAWindow alloc] initWithX:x y:screeny width:w height:h
                                             title:title];
        result = (__bridge_retained pa_winhan)pw;
    });
    return result;
}

pa_winhan pa_cocoa_create_child_window(pa_winhan parent,
                                        int x, int y, int w, int h)
{
    __block pa_winhan result;
    run_on_main(^{
        PAWindow* par = (__bridge PAWindow*)parent;
        PAWindow* pw = [[PAWindow alloc] initChildInParent:par
                                                         x:x y:y
                                                     width:w height:h];
        result = (__bridge_retained pa_winhan)pw;
    });
    return result;
}

/* Close a window. A child (embedded view) is removed from the parent's
   view and keyboard focus is handed back to the parent view; a top-level
   window is closed normally. */
static void close_and_refocus_parent(PAWindow* pw)
{
    if (!pw->window) {
        NSView* sup = pw->cframe.superview;
        [pw->cframe removeFromSuperview];
        if (sup) {
            [sup.window makeFirstResponder:sup];
            [sup setNeedsDisplay:YES];
        }
        return;
    }
    [pw->window close];
}

#define PRESENT_INTERVAL (1.0/60.0)
static PAView* pendview[32];
static int     npendview;

/* a view that is going: off the list of presents held back, and its
   screen buffers freed, under the lock a present takes */
static void retire_view(PAView* v)
{
    int i, j;
    CGImageRef old;
    /* the snapshot goes, and the mark that no other may come, in the one
       hold of the lock: a snapshot stored between the two was nobody's */
    pthread_mutex_lock(&evt_mutex);
    for (i = j = 0; i < npendview; i++) if (pendview[i] != v) pendview[j++] = pendview[i];
    npendview = j;
    v->retired = 1;
    old = v->displayImage;
    v->displayImage = NULL;
    pthread_mutex_unlock(&evt_mutex);
    if (old) CGImageRelease(old);
    pthread_mutex_lock(&v->bufLock);
    [v destroyBitmap];
    pthread_mutex_unlock(&v->bufLock);
}

void pa_cocoa_destroy_window(pa_winhan win)
{
    if (threaded_mode) {
        PAWindow* pw = (__bridge PAWindow*)win;
        /* The screen buffers went with nothing when a window closed: a
           program opening and closing child windows, as random_test does
           without end, grew by the buffers of each; at the screen's scale,
           gigabytes in a minute, and the kernel killed it for its swap. A
           present held back for the window would also have found the
           buffers still there and made it a snapshot nobody released. */
        retire_view(pw->view);
        run_on_main(^{
            PAWindow* pw2 = (__bridge PAWindow*)win;
            close_and_refocus_parent(pw2);
        });
        dispatch_async(dispatch_get_main_queue(), ^{
            CFRelease(win);
        });
    } else {
        PAWindow* pw = (__bridge PAWindow*)win;
        if (pw->view->displayImage) {
            CGImageRelease(pw->view->displayImage);
            pw->view->displayImage = NULL;
        }
        [pw->view destroyBitmap];
        close_and_refocus_parent(pw);
        CFRelease(win);
    }
}

void pa_cocoa_show_window(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            pw->cframe.hidden = NO;
            return;
        }
        [pw->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    });
}

void pa_cocoa_hide_window(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            pw->cframe.hidden = YES;
            return;
        }
        [pw->window orderOut:nil];
    });
}

void pa_cocoa_set_title(pa_winhan win, const char* title)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSString* t = [NSString stringWithUTF8String:title ? title : ""];
        if (!pw->window) {
            pw->cframe->title = t;
            [pw->cframe setNeedsDisplay:YES];
            return;
        }
        pw->window.title = t;
    });
}

void pa_cocoa_move_window(pa_winhan win, int x, int y)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            /* child: (x,y) is the offset from the parent's top-left; the
               parent view is flipped so it maps directly */
            [pw->cframe setFrameOrigin:NSMakePoint(x, y)];
            [pw->cframe.superview setNeedsDisplay:YES];
            return;
        }
        int screeny = pa_cocoa_screen_h() - y - (int)pw->window.frame.size.height;
        [pw->window setFrameOrigin:NSMakePoint(x, screeny)];
    });
}

void pa_cocoa_resize_window(pa_winhan win, int w, int h)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            [pw->cframe setClientSize:NSMakeSize(w, h)];
            return;
        }
        NSRect f = pw->window.frame;
        CGFloat oldTop = f.origin.y + f.size.height;
        NSRect content = NSMakeRect(0, 0, w, h);
        NSRect newFrame = [pw->window frameRectForContentRect:content];
        newFrame.origin.x = f.origin.x;
        newFrame.origin.y = oldTop - newFrame.size.height;
        [pw->window setFrame:newFrame display:YES];
        [pw->view createBitmapWidth:w height:h];
    });
}

/* Extra size (window minus client) a window's chrome would add for the
   given frame/size/sysbar flags: the AdjustWindowRectEx equivalent. */
void pa_cocoa_frame_extra(pa_winhan win, int frame, int size, int sysbar,
                          int* dw, int* dh)
{
    __block int rw = 0, rh = 0;
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            /* Ami-drawn child chrome */
            if (frame) {
                rw = 2 * CFRM_BORDER;
                rh = (sysbar ? CFRM_TITBAR : CFRM_BORDER) + CFRM_BORDER;
            }
        } else {
            NSWindowStyleMask m = NSWindowStyleMaskBorderless;
            if (frame && sysbar)
                m |= NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                     NSWindowStyleMaskMiniaturizable;
            if (frame && size) m |= NSWindowStyleMaskResizable;
            NSRect c = NSMakeRect(0, 0, 400, 400);
            NSRect f = [NSWindow frameRectForContentRect:c styleMask:m];
            rw = (int)(f.size.width  - c.size.width);
            rh = (int)(f.size.height - c.size.height);
        }
    });
    *dw = rw;
    *dh = rh;
}

/* actual on-screen OUTER window size, chrome included (GetWindowRect) */
void pa_cocoa_get_window_size(pa_winhan win, int* w, int* h)
{
    __block int rw, rh;
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSSize s = pw->window ? pw->window.frame.size
                              : pw->cframe.frame.size;
        rw = (int)s.width;
        rh = (int)s.height;
    });
    *w = rw;
    *h = rh;
}

void pa_cocoa_get_size(pa_winhan win, int* w, int* h)
{
    __block int rw, rh;
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSSize s = pw->view.bounds.size;
        rw = (int)s.width;
        rh = (int)s.height;
    });
    *w = rw;
    *h = rh;
}

void pa_cocoa_front(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            /* child: move the view to the top of its siblings */
            NSView* sup = pw->cframe.superview;
            [pw->cframe removeFromSuperview];
            [sup addSubview:pw->cframe];
            [sup setNeedsDisplay:YES];
            return;
        }
        [pw->window orderFront:nil];
    });
}

void pa_cocoa_back(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            /* child: move the view below its siblings */
            NSView* sup = pw->cframe.superview;
            [pw->cframe removeFromSuperview];
            [sup addSubview:pw->cframe
                 positioned:NSWindowBelow
                 relativeTo:nil];
            [sup setNeedsDisplay:YES];
            return;
        }
        [pw->window orderBack:nil];
    });
}

void pa_cocoa_focus(pa_winhan win)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        if (!pw->window) {
            [pw->view.window makeFirstResponder:pw->view];
            return;
        }
        [pw->window makeKeyAndOrderFront:nil];
    });
}

static void refocus_window(NSWindow* w)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [w makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    });
}

/* Change the window style mask without losing keyboard focus. Changing
   styleMask rebuilds the window frame hierarchy, which resets the first
   responder to the window itself and issues a transient key-status resign;
   suppress the resign across the change and put the content view back as
   first responder so key events keep reaching it. */
static void set_style_mask(PAWindow* pw, NSWindowStyleMask m)
{
    PAKeyWindow* kw = (PAKeyWindow*)pw->window;
    kw.suppressResignKey = YES;
    kw.styleMask = m;
    [kw makeKeyAndOrderFront:nil];
    [kw makeFirstResponder:pw->view];
    dispatch_async(dispatch_get_main_queue(), ^{
        kw.suppressResignKey = NO;
        [kw makeKeyAndOrderFront:nil];
        [kw makeFirstResponder:pw->view];
    });
}

/* Toggle a chrome flag on a child frame. The OUTER rect is preserved
   and the client re-insets within it (Windows semantics: turning the
   frame off grows the client to fill the window rect, so adjacent
   frameless children tile edge to edge). */
static void child_chrome(PAChildFrame* cf, int* flag, int on)
{
    *flag = on;
    [cf layoutClient];
    [cf pushResize]; /* report the new client size to the program */
    [cf.window invalidateCursorRectsForView:cf];
    [cf setNeedsDisplay:YES];
    [cf.superview setNeedsDisplay:YES];
}

void pa_cocoa_set_frame(pa_winhan win, int on)
{
    run_on_main(^{
        PAWindow*       pw    = (__bridge PAWindow*)win;
        if (!pw->window) {
            child_chrome(pw->cframe, &pw->cframe->frameOn, on);
            return;
        }
        NSWindowStyleMask all = NSWindowStyleMaskTitled |
                                NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable |
                                NSWindowStyleMaskResizable;
        NSWindowStyleMask m   = pw->window.styleMask;
        if (on) m |=  all;
        else    m &= ~all;
        set_style_mask(pw, m);
    });
}

void pa_cocoa_set_sysbar(pa_winhan win, int on)
{
    run_on_main(^{
        PAWindow*       pw  = (__bridge PAWindow*)win;
        if (!pw->window) {
            child_chrome(pw->cframe, &pw->cframe->sysbarOn, on);
            return;
        }
        NSWindowStyleMask bar = NSWindowStyleMaskTitled |
                                NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable;
        NSWindowStyleMask m = pw->window.styleMask;
        if (on) m |=  bar;
        else    m &= ~bar;
        set_style_mask(pw, m);
    });
}

void pa_cocoa_set_sizable(pa_winhan win, int on)
{
    run_on_main(^{
        PAWindow*       pw  = (__bridge PAWindow*)win;
        if (!pw->window) {
            child_chrome(pw->cframe, &pw->cframe->sizableOn, on);
            return;
        }
        NSWindowStyleMask m = pw->window.styleMask;
        if (on) m |=  NSWindowStyleMaskResizable;
        else    m &= ~NSWindowStyleMaskResizable;
        set_style_mask(pw, m);
    });
}

/*----------------------------------------------------------------------------
 * Drawing context
 *----------------------------------------------------------------------------*/

CGContextRef pa_cocoa_get_context(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    return pw->view->screens[pw->view->updscr];
}

/* The bitmap of any screen (0-based), created on demand, for operations
   such as blockcopyg that move pixels between screens that need not be the
   current update or display one. */
CGContextRef pa_cocoa_get_screen_context(pa_winhan win, int idx)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    return [pw->view ensureScreen:idx];
}

/* Let the user drag the window from the current mouse-down, the way a title
   bar drag does; Cocoa tracks it natively. A child window is a view inside
   its parent and has no NSWindow of its own, so there is nothing to drag. */
void pa_cocoa_drag_window(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    run_on_main(^{
        NSEvent* e = [NSApp currentEvent];
        if (pw->window && e) [pw->window performWindowDragWithEvent:e];
    });
}

/* Queue a program-sent event (ami_sendevent). rec is a heap copy of the
   caller's ami_evtrec, opaque here (this file does not see graphics.h);
   translate_event hands it back verbatim and frees it. evt_push locks the
   queue and signals the waiter, so this is safe from any thread: it is how
   graph_server's pump thread wakes a blocked ami_event after a datagram. */
void pa_cocoa_send_user_event(pa_winhan win, void* rec)
{
    pa_rawevent e = {0};
    e.type = PA_EVT_USER;
    e.win  = win;
    e.user = rec;
    evt_push(&e);
}

/* Snapshot the display screen into displayImage and ask the view to redraw
   (threaded mode). Copies the bitmap into an independent CGImage so the
   worker can keep drawing while the main thread presents. */
static void present_display(PAView* v)
{
    /* A present held back is taken up by whichever thread next waits for
       an event, and a program with a window a thread may have another
       thread resizing this one's buffers: the copy is made under the
       buffer lock the resize takes, else it read a buffer being freed. */
    pthread_mutex_lock(&v->bufLock);
    CGContextRef ctx = v->screens[v->dspscr];
    if (!ctx) { pthread_mutex_unlock(&v->bufLock); return; }
    size_t w = CGBitmapContextGetWidth(ctx);
    size_t h = CGBitmapContextGetHeight(ctx);
    size_t bpr = CGBitmapContextGetBytesPerRow(ctx);
    void* data = CGBitmapContextGetData(ctx);
    if (!data || w == 0 || h == 0) { pthread_mutex_unlock(&v->bufLock); return; }
    CFDataRef pixelData = CFDataCreate(NULL, (const UInt8*)data, h * bpr);
    if (!pixelData) { pthread_mutex_unlock(&v->bufLock); return; }
    CGColorSpaceRef cs = CGBitmapContextGetColorSpace(ctx);
    CGDataProviderRef dp = CGDataProviderCreateWithCFData(pixelData);
    CFRelease(pixelData);
    CGImageRef snap = CGImageCreate(w, h,
        CGBitmapContextGetBitsPerComponent(ctx),
        CGBitmapContextGetBitsPerPixel(ctx),
        bpr, cs, CGBitmapContextGetBitmapInfo(ctx),
        dp, NULL, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(dp);
    pthread_mutex_unlock(&v->bufLock);
    if (!snap) return;
    pthread_mutex_lock(&evt_mutex);
    if (v->retired) { /* closed while the snapshot was made: nobody will
                         release a snapshot put in it now */
        pthread_mutex_unlock(&evt_mutex);
        CGImageRelease(snap);
        return;
    }
    CGImageRef old = v->displayImage;
    v->displayImage = snap;
    /* drawRect draws the snapshot into a POINT-sized rect (its context is
       in points), so dispW/dispH are the point dimensions, not the bitmap's
       pixel width/height. These are equal at the fixed 1x scale, but keeping
       them decoupled means a scaled buffer would still present at the
       correct window size instead of doubling. */
    v->dispW = v->bmpW;
    v->dispH = v->bmpH;
    pthread_mutex_unlock(&evt_mutex);
    if (old) CGImageRelease(old);
    dispatch_async(dispatch_get_main_queue(), ^{
        [v setNeedsDisplay:YES];
    });
}

/* The presents held back: a flush within a frame time of the last present
   marks the view instead of copying the buffer again, and the mark is
   taken up by the next flush after the frame time, or when the program
   goes to wait for an event, so that what it drew last is shown. All on
   the drawing thread, which also resizes the buffers: no copy races one. */

static double nowsec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void present_now(PAView* v)
{
    present_display(v);
    v->lastPresent = nowsec();
    v->presentPending = 0;
}

static void present_pending(void)
{
    int i, n;
    PAView* vs[32];
    if (!npendview) return;
    pthread_mutex_lock(&evt_mutex);
    n = npendview;
    for (i = 0; i < n; i++) vs[i] = pendview[i];
    npendview = 0;
    pthread_mutex_unlock(&evt_mutex);
    for (i = 0; i < n; i++) if (vs[i]->presentPending) present_now(vs[i]);
}

static void present_coalesced(PAView* v)
{
    int i;
    if (nowsec() - v->lastPresent >= PRESENT_INTERVAL) { present_now(v); return; }
    v->presentPending = 1;
    pthread_mutex_lock(&evt_mutex);
    for (i = 0; i < npendview; i++) if (pendview[i] == v) break;
    if (i == npendview && npendview < 32) pendview[npendview++] = v;
    pthread_mutex_unlock(&evt_mutex);
}

void pa_cocoa_flush(pa_winhan win)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    if (threaded_mode) {
        PAView* v = pw->view;
        /* When the update and display screens differ, the program is
           double-buffering (ami_select): drawing goes to an off-screen
           page and must not touch the display until it is flipped in.
           Presenting here would snapshot the stable on-screen page once
           per primitive -- dozens of redundant redraws per frame that
           flood the compositor and make the animation flicker. The flip
           (pa_cocoa_select_screens) presents the revealed page instead. */
        if (v->updscr != v->dspscr) return;
        /* The buffer is copied whole on each present, and at the screen's
           scale a copy is four times the size it was: once per primitive,
           as it was, a run of drawing spent its time copying. Held to a
           frame time apart. */
        present_coalesced(v);
    } else {
        [pw->view setNeedsDisplay:YES];
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0]];
    }
}

void pa_cocoa_set_cursor(pa_winhan win, int visible, int x, int y, int w, int h)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    PAView*   v  = pw->view;
    int changed = (v->curVisible != visible || v->curX != x || v->curY != y ||
                   v->curW != w || v->curH != h);
    v->curVisible = visible;
    v->curX = x;
    v->curY = y;
    v->curW = w;
    v->curH = h;
    if (changed) {
        if (threaded_mode) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [v setNeedsDisplay:YES];
            });
        } else {
            [v setNeedsDisplay:YES];
        }
    }
}

void pa_cocoa_resize_bitmap(pa_winhan win, int w, int h)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    PAView*   v  = pw->view;

    int oldW = v->bmpW;
    int oldH = v->bmpH;

    pthread_mutex_lock(&v->bufLock); /* against a present on another thread */
    /* save old display screen bitmap */
    CGContextRef oldCtx = v->screens[v->dspscr];
    CGImageRef oldImg = oldCtx ? CGBitmapContextCreateImage(oldCtx) : NULL;

    /* release all screen bitmaps except detach the display one */
    v->screens[v->dspscr] = NULL;
    for (int i = 0; i < 10; i++) {
        if (v->screens[i]) { CGContextRelease(v->screens[i]); v->screens[i] = NULL; }
    }
    if (oldCtx) CGContextRelease(oldCtx);

    /* create new bitmaps at new size */
    v->bmpW = w;
    v->bmpH = h;
    v->screens[v->dspscr] = [v createOneBitmapWidth:w height:h];
    v->bitmap = v->screens[v->dspscr];
    if (v->updscr != v->dspscr) {
        v->screens[v->updscr] = [v createOneBitmapWidth:w height:h];
    }
    pthread_mutex_unlock(&v->bufLock);

    /* copy intersection of old content into new display bitmap */
    if (oldImg) {
        int copyW = oldW < w ? oldW : w;
        int copyH = oldH < h ? oldH : h;
        if (copyW > 0 && copyH > 0) {
            CGContextRef dst = v->screens[v->dspscr];
            CGContextSaveGState(dst);
            CGContextClipToRect(dst, CGRectMake(0, 0, copyW, copyH));
            CGContextDrawImage(dst, CGRectMake(0, 0, oldW, oldH), oldImg);
            CGContextRestoreGState(dst);
        }
        CGImageRelease(oldImg);
    }

    if (!threaded_mode) [v setNeedsDisplay:YES];
}

void pa_cocoa_set_bufmod(pa_winhan win, int on)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    int was = pw->view->bufmod;
    pw->view->bufmod = on;
    /* leaving buffered mode makes the application responsible for painting
       on redraw events; push an initial redraw so it paints the first
       frame without waiting for a user resize */
    if (was && !on) {
        pa_rawevent e = {0};
        e.type     = PA_EVT_REDRAW;
        e.win      = win;
        e.redraw.w = pw->view->bmpW;
        e.redraw.h = pw->view->bmpH;
        evt_push(&e);
    }
}

void pa_cocoa_set_background(pa_winhan win, float r, float g, float b)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    pw->view->bgR = r;
    pw->view->bgG = g;
    pw->view->bgB = b;
}

void pa_cocoa_select_screens(pa_winhan win, int upd, int dsp)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    PAView*   v  = pw->view;
    if (upd < 0 || upd >= 10 || dsp < 0 || dsp >= 10) return;
    [v ensureScreen:upd];
    [v ensureScreen:dsp];
    v->updscr = upd;
    int old_dsp = v->dspscr;
    v->dspscr = dsp;
    if (dsp != old_dsp) {
        /* the flip: reveal the newly-selected display page. In threaded
           mode present it (flush skips off-screen draws, so this is the
           one place the double-buffered display advances per frame). */
        if (threaded_mode) {
            present_now(v); /* a flip shows its page at once */
        } else {
            [v setNeedsDisplay:YES];
        }
    }
}

/*----------------------------------------------------------------------------
 * Event processing
 *----------------------------------------------------------------------------*/

void pa_cocoa_process_ns_events(void)
{
    if (threaded_mode) return;
    NSEvent* event;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) {
        [NSApp sendEvent:event];
        [NSApp updateWindows];
    }
}

int pa_cocoa_dequeue(pa_rawevent* evt)
{
    return evt_pop(evt);
}

void pa_cocoa_wait(pa_rawevent* evt)
{
    if (threaded_mode) {
        present_pending(); /* the program is done drawing for now: show it */
        pthread_mutex_lock(&evt_mutex);
        while (evt_tail == evt_head)
            pthread_cond_wait(&evt_cond, &evt_mutex);
        *evt = evt_queue[evt_tail];
        evt_tail = (evt_tail + 1) % EVT_QUEUE_SIZE;
        pthread_mutex_unlock(&evt_mutex);
    } else {
        while (evt_empty()) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
                [NSApp updateWindows];
            }
        }
        evt_pop(evt);
    }
}

/*----------------------------------------------------------------------------
 * Timers
 *----------------------------------------------------------------------------*/

/* us100: 100 microsecond units, PA's timer unit; see pa_cocoa.h */
void pa_cocoa_set_timer(pa_winhan win, int id, long us100, int repeat)
{
    if (id < 0 || id >= 10) return;
    run_on_main(^{
        PAWindow* pw  = (__bridge PAWindow*)win;
        if (pw->timers[id]) { [pw->timers[id] invalidate]; pw->timers[id] = nil; }
        NSTimeInterval interval = us100 * 0.0001;
        NSNumber* tid  = [NSNumber numberWithInt:id];
        pw->timers[id] = [NSTimer scheduledTimerWithTimeInterval:interval
                                                          target:pw
                                                        selector:@selector(timerFired:)
                                                        userInfo:tid
                                                         repeats:(repeat ? YES : NO)];
    });
}

void pa_cocoa_kill_timer(pa_winhan win, int id)
{
    if (id < 0 || id >= 10) return;
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        [pw->timers[id] invalidate];
        pw->timers[id] = nil;
    });
}

/*----------------------------------------------------------------------------
 * Widgets
 *----------------------------------------------------------------------------*/

/* Opaque backing for a widget. Win32 widgets are child windows that fully
   cover their rectangle; Cocoa controls can be translucent (e.g. a disabled
   button), letting window content show through. Each control sits in one
   of these, painted with the window background color. */
@interface PAWidgetBox : NSView {
@public
    float r, g, b;
}
@end

@implementation PAWidgetBox
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirtyRect
{
    [[NSColor colorWithRed:r green:g blue:b alpha:1.0] setFill];
    NSRectFill(self.bounds);
}
@end

/* wrap a control in an opaque backing box and add it to the window */
static void add_widget_boxed(PAWindow* pw, NSView* ctl, NSRect rect)
{
    PAWidgetBox* box = [[PAWidgetBox alloc] initWithFrame:rect];
    box->r = pw->view->bgR;
    box->g = pw->view->bgG;
    box->b = pw->view->bgB;
    ctl.frame = NSMakeRect(0, 0, rect.size.width, rect.size.height);
    [box addSubview:ctl];
    [pw->view addSubview:box];
}

/* Helper: find a control by tag (controls live inside PAWidgetBox) */
static NSView* find_widget(pa_winhan win, int id)
{
    PAWindow* pw = (__bridge PAWindow*)win;
    for (NSView* v in pw->view.subviews) {
        if (pa_get_tag(v) == id) return v;
        for (NSView* s in v.subviews)
            if (pa_get_tag(s) == id) return s;
    }
    return nil;
}

void pa_cocoa_button(pa_winhan win, int x, int y, int w, int h,
                     const char* label, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        b.title      = [NSString stringWithUTF8String:label];
        /* the standard rounded bezel has a fixed height; oversized buttons
           need a bezel that stretches to fill the frame */
        b.bezelStyle = (h > 32) ? NSBezelStyleRegularSquare
                                : NSBezelStyleRounded;
        b.buttonType = NSButtonTypeMomentaryPushIn;
        b.tag        = id;
        b.target     = pw;
        b.action     = @selector(buttonAction:);
        add_widget_boxed(pw, b, NSMakeRect(x-1, y-1, w, h));
    });
}

void pa_cocoa_checkbox(pa_winhan win, int x, int y, int w, int h,
                       const char* label, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        b.title      = [NSString stringWithUTF8String:label];
        b.buttonType = NSButtonTypeSwitch;
        b.tag        = id;
        b.target     = pw;
        b.action     = @selector(checkboxAction:);
        add_widget_boxed(pw, b, NSMakeRect(x-1, y-1, w, h));
    });
}

void pa_cocoa_radiobutton(pa_winhan win, int x, int y, int w, int h,
                          const char* label, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSButton* b  = [[NSButton alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        b.title      = [NSString stringWithUTF8String:label];
        b.buttonType = NSButtonTypeRadio;
        b.tag        = id;
        b.target     = pw;
        b.action     = @selector(radioAction:);
        add_widget_boxed(pw, b, NSMakeRect(x-1, y-1, w, h));
    });
}

/* size of a group box title as NSBox will render it (11pt system font) */
void pa_cocoa_group_title_size(const char* s, int* w, int* h)
{
    __block int rw, rh;
    run_on_main(^{
        NSString* str = [NSString stringWithUTF8String:s ? s : ""];
        NSDictionary* at = @{ NSFontAttributeName:
                                  [NSFont systemFontOfSize:11] };
        NSSize sz = [str sizeWithAttributes:at];
        rw = (int)(sz.width + 0.5);
        rh = (int)(sz.height + 0.5);
    });
    *w = rw;
    *h = rh;
}

void pa_cocoa_group(pa_winhan win, int x, int y, int w, int h,
                    const char* label, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSBox* box = [[NSBox alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        box.title = [NSString stringWithUTF8String:label ? label : ""];
        box.titlePosition = (label && label[0]) ? NSAtTop : NSNoTitle;
        box.titleFont = [NSFont systemFontOfSize:11];
        pa_set_tag(box, id);
        /* a group box is an outline only — no opaque backing, so widgets
           and window content inside it stay visible (Win32 semantics) */
        [pw->view addSubview:box];
    });
}

void pa_cocoa_background(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        PAWidgetBox* bg =
            [[PAWidgetBox alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        bg->r = pw->view->bgR;
        bg->g = pw->view->bgG;
        bg->b = pw->view->bgB;
        pa_set_tag(bg, id);
        [pw->view addSubview:bg];
    });
}

void pa_cocoa_editbox(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow*    pw = (__bridge PAWindow*)win;
        NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        tf.tag    = id;
        tf.target = pw;
        tf.action = @selector(editAction:);
        add_widget_boxed(pw, tf, NSMakeRect(x-1, y-1, w, h));
    });
}

/* PAScroller — Ami-drawn scrollbar. NSScroller draws only at the fixed
   system girth and refuses scaled coordinate systems, but PA scrollbars
   can be any size ("fat and skinny bars"), so we draw our own: a rounded
   track with a draggable knob, in the style of the legacy macOS scroller.
   Knob drags report positions; trough clicks report page up/down. */

/* PA full scale is 0..LONG_MAX.  (double)LONG_MAX rounds up to 2^63,
   one past LONG_MAX, so a ratio of 1.0 (or a scale value at the top of
   the range) must be clamped before the cast to avoid signed overflow. */

/* scale a 0..1 ratio to PA full scale 0..LONG_MAX, clamped */
static long pa_frac_to_long(double v)
{
    if (v <= 0.0) return 0;
    if (v >= 1.0) return LONG_MAX;
    return (long)(v * (double)LONG_MAX);
}

/* clamp a double already on the PA scale into 0..LONG_MAX */
static long pa_double_to_long(double d)
{
    if (d <= 0.0) return 0;
    if (d >= (double)LONG_MAX) return LONG_MAX;
    return (long)d;
}

@interface PAWindow (Actions)
- (void)pushWidget:(int)act wid:(int)wid pos:(long)pos;
@end

@interface PAScroller : NSView {
@public
    __weak PAWindow* owner;
    int    wid;        /* PA widget id */
    int    vertical;
    double value;      /* 0..1 */
    double knobProp;   /* knob proportion 0..1 */
    int    dragging;
    double dragOff;    /* grab offset from knob start, in pixels */
}
@end

@implementation PAScroller

- (BOOL)isFlipped { return YES; }

- (NSRect)knobRect
{
    NSRect b = self.bounds;
    if (vertical) {
        CGFloat kh = b.size.height * knobProp;
        if (kh < 20) kh = 20;
        if (kh > b.size.height) kh = b.size.height;
        return NSMakeRect(2, value * (b.size.height - kh),
                          b.size.width - 4, kh);
    } else {
        CGFloat kw = b.size.width * knobProp;
        if (kw < 20) kw = 20;
        if (kw > b.size.width) kw = b.size.width;
        return NSMakeRect(value * (b.size.width - kw), 2,
                          kw, b.size.height - 4);
    }
}

- (void)drawRect:(NSRect)dirtyRect
{
    NSRect b = self.bounds;
    /* track */
    [[NSColor colorWithWhite:0.96 alpha:1.0] setFill];
    NSRectFill(b);
    [[NSColor colorWithWhite:0.85 alpha:1.0] setStroke];
    NSBezierPath* edge = [NSBezierPath bezierPathWithRect:
                             NSInsetRect(b, 0.5, 0.5)];
    [edge setLineWidth:1.0];
    [edge stroke];
    /* knob */
    NSRect k = [self knobRect];
    CGFloat r = (vertical ? k.size.width : k.size.height) / 2;
    if (r > 8) r = 8;
    NSBezierPath* knob = [NSBezierPath bezierPathWithRoundedRect:k
                                                         xRadius:r
                                                         yRadius:r];
    [[NSColor colorWithWhite:0.55 alpha:1.0] setFill];
    [knob fill];
}

- (void)pushPos
{
    [owner pushWidget:PA_WIDGET_SCROLL_POS wid:wid
                  pos:pa_frac_to_long(value)];
}

- (void)mouseDown:(NSEvent*)e
{
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    NSRect  k = [self knobRect];
    if (NSPointInRect(p, k)) {
        dragging = 1;
        dragOff  = vertical ? p.y - k.origin.y : p.x - k.origin.x;
        return;
    }
    /* trough click: page toward the click */
    int before = vertical ? (p.y < k.origin.y) : (p.x < k.origin.x);
    [owner pushWidget:(before ? PA_WIDGET_SCROLL_PAGEUP
                              : PA_WIDGET_SCROLL_PAGEDN)
                  wid:wid pos:0];
}

- (void)mouseDragged:(NSEvent*)e
{
    if (!dragging) return;
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    NSRect  b = self.bounds;
    NSRect  k = [self knobRect];
    double range = vertical ? b.size.height - k.size.height
                            : b.size.width  - k.size.width;
    if (range <= 0) return;
    double v = ((vertical ? p.y : p.x) - dragOff) / range;
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    value = v;
    [self setNeedsDisplay:YES];
    [self pushPos];
}

- (void)mouseUp:(NSEvent*)e
{
    dragging = 0;
}

@end

static void make_scroller(PAWindow* pw, int vertical, NSRect rect, int id)
{
    PAScroller* s = [[PAScroller alloc] initWithFrame:rect];
    s->owner    = pw;
    s->wid      = id;
    s->vertical = vertical;
    s->value    = 0.0;
    s->knobProp = 0.1;
    pa_set_tag(s, id);
    add_widget_boxed(pw, s, rect);
}

/* PANumSel — number select box: an edit field paired with a stepper,
   the macOS equivalent of the Win32 up-down control. */
@interface PANumSel : NSView {
@public
    __weak PAWindow* owner;
    int          wid;
    NSTextField* field;
    NSStepper*   stepper;
}
@end

@implementation PANumSel

- (BOOL)isFlipped { return YES; }

- (void)pushValue:(long)v
{
    [owner pushWidget:PA_WIDGET_NUM_DONE wid:wid pos:v];
}

- (void)stepperAct:(id)sender
{
    /* the arrows only adjust the displayed number; the selection event
       fires when the user commits with return in the field */
    field.integerValue = stepper.integerValue;
}

- (void)fieldAct:(id)sender
{
    /* NSInteger is long on 64-bit macOS.  The stepper limits are doubles;
       a long limit near LONG_MAX rounds up to 2^63 as a double, one past
       LONG_MAX, so clamp before casting back to long. */
    double mn = stepper.minValue;
    double mx = stepper.maxValue;
    long v = (long)field.integerValue;
    if (v < mn) v = (mn <= (double)LONG_MIN) ? LONG_MIN : (long)mn;
    if (v > mx) v = (mx >= (double)LONG_MAX) ? LONG_MAX : (long)mx;
    field.integerValue   = v;
    stepper.integerValue = v;
    [self pushValue:v];
}

@end

void pa_cocoa_numselbox(pa_winhan win, int x, int y, int w, int h,
                        long l, long u, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        PANumSel* ns = [[PANumSel alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
        ns->owner = pw;
        ns->wid   = id;

        CGFloat stepw = 19;
        ns->field = [[NSTextField alloc]
                        initWithFrame:NSMakeRect(0, 0, w - stepw - 2, h)];
        ns->field.integerValue = l;
        ns->field.target   = ns;
        ns->field.action   = @selector(fieldAct:);
        [ns addSubview:ns->field];

        ns->stepper = [[NSStepper alloc]
                          initWithFrame:NSMakeRect(w - stepw, (h - 27) / 2,
                                                   stepw, 27)];
        ns->stepper.minValue  = l;
        ns->stepper.maxValue  = u;
        ns->stepper.integerValue = l;
        ns->stepper.increment = 1;
        ns->stepper.valueWraps = NO; /* stall at the ends, don't wrap */
        ns->stepper.target    = ns;
        ns->stepper.action    = @selector(stepperAct:);
        [ns addSubview:ns->stepper];

        pa_set_tag(ns, id);
        add_widget_boxed(pw, ns, NSMakeRect(x-1, y-1, w, h));
    });
}

/* PAListBox — list box as a single-column table in a scroll view */
@interface PAListBox : NSScrollView <NSTableViewDataSource, NSTableViewDelegate> {
@public
    __weak PAWindow*    owner;
    int                 wid;
    NSArray<NSString*>* items;
    NSTableView*        table;
}
@end

@implementation PAListBox

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv
{
    return items.count;
}

- (NSView*)tableView:(NSTableView*)tv
  viewForTableColumn:(NSTableColumn*)col
                 row:(NSInteger)row
{
    NSTextField* tf = [tv makeViewWithIdentifier:@"cell" owner:self];
    if (!tf) {
        tf = [[NSTextField alloc] init];
        tf.identifier      = @"cell";
        tf.bordered        = NO;
        tf.editable        = NO;
        tf.drawsBackground = NO;
    }
    tf.stringValue = items[row];
    return tf;
}

- (void)tableViewSelectionDidChange:(NSNotification*)n
{
    NSInteger r = table.selectedRow;
    if (r >= 0)
        [owner pushWidget:PA_WIDGET_LIST_SEL wid:wid pos:(long)r + 1];
}

@end

void pa_cocoa_listbox(pa_winhan win, int x, int y, int w, int h,
                      const char** strs, int count, int id)
{
    /* copy the strings before crossing to the main thread */
    NSMutableArray<NSString*>* arr = [NSMutableArray arrayWithCapacity:count];
    for (int i = 0; i < count; i++)
        [arr addObject:[NSString stringWithUTF8String:strs[i] ? strs[i] : ""]];

    run_on_main(^{
        PAWindow*  pw = (__bridge PAWindow*)win;
        PAListBox* lb = [[PAListBox alloc]
                            initWithFrame:NSMakeRect(0, 0, w, h)];
        lb->owner = pw;
        lb->wid   = id;
        lb->items = arr;

        NSTableView* t = [[NSTableView alloc]
                             initWithFrame:NSMakeRect(0, 0, w, h)];
        NSTableColumn* c = [[NSTableColumn alloc] initWithIdentifier:@"c"];
        c.width = w;
        [t addTableColumn:c];
        t.headerView = nil;
        /* fixed row metrics so ami_listboxsiz's 18px-per-entry holds;
           plain style avoids the macOS 11 automatic inset padding */
        if (@available(macOS 11.0, *)) t.style = NSTableViewStylePlain;
        t.rowHeight        = 15;
        t.intercellSpacing = NSMakeSize(3, 3);
        t.dataSource = lb;
        t.delegate   = lb;
        lb->table    = t;

        lb.documentView          = t;
        lb.hasVerticalScroller   = YES;
        lb.autohidesScrollers    = YES; /* only appears if entries overflow */
        lb.borderType            = NSBezelBorder;

        pa_set_tag(lb, id);
        add_widget_boxed(pw, lb, NSMakeRect(x-1, y-1, w, h));
        [t reloadData];
    });
}

void pa_cocoa_dropbox(pa_winhan win, int x, int y, int w, int h,
                      const char** strs, int count, int id)
{
    NSMutableArray<NSString*>* arr = [NSMutableArray arrayWithCapacity:count];
    for (int i = 0; i < count; i++)
        [arr addObject:[NSString stringWithUTF8String:strs[i] ? strs[i] : ""]];

    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        /* the PA rect reserves room for the dropped-down list; the control
           itself occupies only the closed height at the top */
        int ch = h < 25 ? h : 25;
        NSPopUpButton* b = [[NSPopUpButton alloc]
                               initWithFrame:NSMakeRect(0, 0, w, ch)
                                   pullsDown:NO];
        for (NSString* t in arr) [b addItemWithTitle:t];
        b.tag    = id;
        b.target = pw;
        b.action = @selector(dropboxAction:);
        add_widget_boxed(pw, b, NSMakeRect(x-1, y-1, w, ch));
    });
}

void pa_cocoa_dropeditbox(pa_winhan win, int x, int y, int w, int h,
                          const char** strs, int count, int id)
{
    NSMutableArray<NSString*>* arr = [NSMutableArray arrayWithCapacity:count];
    for (int i = 0; i < count; i++)
        [arr addObject:[NSString stringWithUTF8String:strs[i] ? strs[i] : ""]];

    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        /* as with the drop box, only the closed height is the control */
        int ch = h < 24 ? h : 24;
        NSComboBox* b = [[NSComboBox alloc]
                            initWithFrame:NSMakeRect(0, 0, w, ch)];
        [b addItemsWithObjectValues:arr];
        b.completes = YES;
        b.tag    = id;
        b.target = pw;
        b.action = @selector(dropeditAction:);
        add_widget_boxed(pw, b, NSMakeRect(x-1, y-1, w, ch));
    });
}

/* PATabBar — a tabbed panel: segment bar on the oriented edge plus an
   opaque, outlined client area filling the rest of the PA rect, matching
   the Win32 tab control's appearance. */
@interface PATabBar : NSView {
@public
    int   ori;   /* 0 top, 1 right, 2 bottom, 3 left */
    int   barH;
    float r, g, b; /* client background */
}
@end

@implementation PATabBar

- (BOOL)isFlipped { return YES; }

- (NSRect)clientRect
{
    NSRect bo = self.bounds;
    switch (ori) {
    case 0:  return NSMakeRect(0, barH, bo.size.width, bo.size.height - barH);
    case 2:  return NSMakeRect(0, 0, bo.size.width, bo.size.height - barH);
    case 3:  return NSMakeRect(barH, 0, bo.size.width - barH, bo.size.height);
    default: return NSMakeRect(0, 0, bo.size.width - barH, bo.size.height);
    }
}

- (void)drawRect:(NSRect)dirtyRect
{
    /* the whole panel is opaque, bar strip included, like a Win32 tab
       control; the client area gets an outline */
    [[NSColor colorWithRed:r green:g blue:b alpha:1.0] setFill];
    NSRectFill(self.bounds);
    [[NSColor colorWithWhite:0.65 alpha:1.0] setStroke];
    NSBezierPath* edge = [NSBezierPath bezierPathWithRect:
                             NSInsetRect([self clientRect], 0.5, 0.5)];
    [edge setLineWidth:1.0];
    [edge stroke];
}

@end

void pa_cocoa_tabbar(pa_winhan win, int x, int y, int w, int h,
                     const char** strs, int count, int ori, int barh,
                     int id)
{
    NSMutableArray<NSString*>* arr = [NSMutableArray arrayWithCapacity:count];
    for (int i = 0; i < count; i++)
        [arr addObject:[NSString stringWithUTF8String:strs[i] ? strs[i] : ""]];

    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        /* the PA rect is the whole tabbed panel: bar strip of the given
           thickness on the oriented edge, framed client area filling the
           remainder */
        const int ctlH = 24; /* natural segmented control height */
        int vertical = (ori == 1 || ori == 3); /* right/left */

        PATabBar* tb = [[PATabBar alloc]
                           initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        tb->ori  = ori;
        tb->barH = barh;
        tb->r = pw->view->bgR;
        tb->g = pw->view->bgG;
        tb->b = pw->view->bgB;

        NSSegmentedControl* sc = [[NSSegmentedControl alloc]
                                     initWithFrame:NSMakeRect(0, 0,
                                        vertical ? h : w, ctlH)];
        sc.segmentCount = arr.count;
        for (NSInteger i = 0; i < (NSInteger)arr.count; i++)
            [sc setLabel:arr[i] forSegment:i];
        sc.selectedSegment = 0;
        sc.tag    = id;
        sc.target = pw;
        sc.action = @selector(tabbarAction:);

        /* the control sits in the strip against the client edge */
        if (vertical) {
            /* a segmented control is horizontal-only: place it in the side
               strip and rotate it upright; hit testing follows */
            CGFloat sx = (ori == 3) ? barh - ctlH : w - barh;
            sc.frame = NSMakeRect(sx + (ctlH - h) / 2.0,
                                  (h - ctlH) / 2.0, h, ctlH);
            sc.frameCenterRotation = 90; /* first item at top on both sides */
        } else {
            CGFloat sy = (ori == 0) ? barh - ctlH : h - barh;
            sc.frame = NSMakeRect(0, sy, w, ctlH);
        }
        [tb addSubview:sc];
        pa_set_tag(tb, id);
        [pw->view addSubview:tb];
    });
}

void pa_cocoa_tabbar_sel(pa_winhan win, int id, int tn)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSSegmentedControl class]])
            [(NSSegmentedControl*)v setSelectedSegment:tn - 1];
    });
}

void pa_cocoa_scrollvert(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        make_scroller(pw, 1, NSMakeRect(x-1, y-1, w, h), id);
    });
}

void pa_cocoa_scrollhoriz(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        make_scroller(pw, 0, NSMakeRect(x-1, y-1, w, h), id);
    });
}

void pa_cocoa_slider_horiz(pa_winhan win, int x, int y, int w, int h,
                           int mark, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSSlider* s  = [[NSSlider alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        s.minValue   = 0;
        s.maxValue   = (double)LONG_MAX;
        s.doubleValue = 0;
        /* mark is the tick frequency on the Windows 0..100 trackbar scale;
           0 disables tick marks */
        s.numberOfTickMarks = (mark > 0) ? 100 / mark + 1 : 0;
        s.tag        = id;
        s.target     = pw;
        s.action     = @selector(sliderAction:);
        add_widget_boxed(pw, s, NSMakeRect(x-1, y-1, w, h));
    });
}

void pa_cocoa_slider_vert(pa_winhan win, int x, int y, int w, int h,
                          int mark, int id)
{
    run_on_main(^{
        PAWindow* pw = (__bridge PAWindow*)win;
        NSSlider* s  = [[NSSlider alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        s.vertical   = YES;
        s.minValue   = 0;
        s.maxValue   = (double)LONG_MAX;
        s.doubleValue = 0;
        s.numberOfTickMarks = (mark > 0) ? 100 / mark + 1 : 0;
        s.tag        = id;
        s.target     = pw;
        s.action     = @selector(sliderAction:);
        add_widget_boxed(pw, s, NSMakeRect(x-1, y-1, w, h));
    });
}

void pa_cocoa_progressbar(pa_winhan win, int x, int y, int w, int h, int id)
{
    run_on_main(^{
        PAWindow*        pw = (__bridge PAWindow*)win;
        NSProgressIndicator* p =
            [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(x-1, y-1, w, h)];
        p.style          = NSProgressIndicatorStyleBar;
        p.indeterminate  = NO;
        p.minValue       = 0;
        p.maxValue       = (double)LONG_MAX;
        pa_set_tag(p, id);
        add_widget_boxed(pw, p, NSMakeRect(x-1, y-1, w, h));
    });
}

void pa_cocoa_kill_widget(pa_winhan win, int id)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (!v) return;
        /* remove the opaque backing box along with the control */
        if ([v.superview isKindOfClass:[PAWidgetBox class]]) v = v.superview;
        [v removeFromSuperview];
    });
}

void pa_cocoa_widget_text(pa_winhan win, int id, const char* s)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (!v) return;
        NSString* ns = [NSString stringWithUTF8String:s];
        if ([v isKindOfClass:[NSControl class]])
            [(NSControl*)v setStringValue:ns];
    });
}

void pa_cocoa_widget_get_text(pa_winhan win, int id, char* s, int sl)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (!v) { if (sl > 0) s[0] = 0; return; }
        if ([v isKindOfClass:[NSControl class]]) {
            const char* cs = [[(NSControl*)v stringValue] UTF8String];
            strncpy(s, cs, sl-1);
            s[sl-1] = 0;
        }
    });
}

void pa_cocoa_widget_enable(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSControl class]])
            [(NSControl*)v setEnabled:(on ? YES : NO)];
    });
}

void pa_cocoa_widget_select(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSButton class]]) {
            NSButton* b = (NSButton*)v;
            /* A momentary push button does not render its state; convert it
               to push-on/push-off the first time a select is applied so the
               pressed-in look persists. Checkboxes and radios already show
               state (nonzero showsStateBy) and are left alone. */
            if ([[b cell] showsStateBy] == 0)
                [b setButtonType:NSButtonTypePushOnPushOff];
            b.state = on ? NSControlStateValueOn : NSControlStateValueOff;
        }
    });
}

void pa_cocoa_scrollbar_pos(pa_winhan win, int id, long pos)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[PAScroller class]]) {
            ((PAScroller*)v)->value = (double)pos / (double)LONG_MAX;
            [v setNeedsDisplay:YES];
        }
    });
}

void pa_cocoa_scrollbar_siz(pa_winhan win, int id, long range)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[PAScroller class]]) {
            ((PAScroller*)v)->knobProp = (double)range / (double)LONG_MAX;
            [v setNeedsDisplay:YES];
        }
    });
}

void pa_cocoa_progressbar_pos(pa_winhan win, int id, long pos)
{
    run_on_main(^{
        NSView* v = find_widget(win, id);
        if (v && [v isKindOfClass:[NSProgressIndicator class]])
            [(NSProgressIndicator*)v setDoubleValue:(double)pos];
    });
}

/*----------------------------------------------------------------------------
 * Widget action callbacks (target/action from controls)
 *----------------------------------------------------------------------------*/

@implementation PAWindow (Actions)

- (void)pushWidget:(int)act wid:(int)wid pos:(long)pos
{
    pa_rawevent e = {0};
    e.type       = PA_EVT_WIDGET;
    e.win        = (__bridge pa_winhan)self;
    e.widget.id  = wid;
    e.widget.act = act;
    e.widget.pos = pos;
    evt_push(&e);
}

- (void)buttonAction:(id)sender
{
    [self pushWidget:PA_WIDGET_BUTTON wid:(int)[(NSControl*)sender tag] pos:0];
}

- (void)checkboxAction:(id)sender
{
    [self pushWidget:PA_WIDGET_CHECKBOX wid:(int)[(NSControl*)sender tag] pos:0];
}

- (void)radioAction:(id)sender
{
    [self pushWidget:PA_WIDGET_RADIO wid:(int)[(NSControl*)sender tag] pos:0];
}

- (void)editAction:(id)sender
{
    [self pushWidget:PA_WIDGET_EDIT_DONE wid:(int)[(NSControl*)sender tag] pos:0];
}

- (void)scrollAction:(id)sender
{
    NSScroller* sc = (NSScroller*)sender;
    int wid = (int)sc.tag;
    switch (sc.hitPart) {
    case NSScrollerDecrementPage:
        [self pushWidget:PA_WIDGET_SCROLL_PAGEUP wid:wid pos:0];
        break;
    case NSScrollerIncrementPage:
        [self pushWidget:PA_WIDGET_SCROLL_PAGEDN wid:wid pos:0];
        break;
    default: /* knob / knob slot: report the new position */
        [self pushWidget:PA_WIDGET_SCROLL_POS wid:wid
                     pos:pa_frac_to_long(sc.doubleValue)];
        break;
    }
}

- (void)sliderAction:(id)sender
{
    NSSlider* sl = (NSSlider*)sender;
    [self pushWidget:PA_WIDGET_SLIDER_POS wid:(int)sl.tag
                 pos:pa_double_to_long(sl.doubleValue)];
}

- (void)dropboxAction:(id)sender
{
    NSPopUpButton* b = (NSPopUpButton*)sender;
    [self pushWidget:PA_WIDGET_DROP_SEL wid:(int)b.tag
                 pos:(long)b.indexOfSelectedItem + 1];
}

- (void)dropeditAction:(id)sender
{
    [self pushWidget:PA_WIDGET_DROPED_DONE wid:(int)[(NSControl*)sender tag]
                 pos:0];
}

- (void)tabbarAction:(id)sender
{
    NSSegmentedControl* sc = (NSSegmentedControl*)sender;
    [self pushWidget:PA_WIDGET_TAB_SEL wid:(int)sc.tag
                 pos:(long)sc.selectedSegment + 1];
}

@end

/*----------------------------------------------------------------------------
 * Menus — native macOS menu bar
 *----------------------------------------------------------------------------*/

/* Layout-compatible mirror of ami_menurec so we can walk the tree */
/* Layout-compatible mirror of ami_menurec (include/graphics.h). The
   integer fields are 'long' there, not 'int': on LP64 the difference is
   16 bytes, which would shift 'face' and read a garbage/NULL string. */
typedef struct pa_menu_node {
    struct pa_menu_node* next;
    struct pa_menu_node* branch;
    long onoff;
    long oneof;
    long bar;
    long id;
    char* face;
} pa_menu_node;

/* Menu action target — fires PA_EVT_MENU events */
@interface PAMenuTarget : NSObject
@property (assign) pa_winhan owner;
- (void)menuItemAction:(NSMenuItem*)sender;
@end

@implementation PAMenuTarget
- (void)menuItemAction:(NSMenuItem*)sender
{
    pa_rawevent e = {0};
    e.type    = PA_EVT_MENU;
    e.win     = self.owner;
    e.menu.id = (int)sender.tag;
    evt_push(&e);
}
@end

static PAMenuTarget* menuTarget = nil;
static NSMenu* savedAppMenu = nil;

static NSMenuItem* findMenuItemByTag(NSMenu* menu, int tag)
{
    for (NSInteger i = 0; i < menu.numberOfItems; i++) {
        NSMenuItem* item = [menu itemAtIndex:i];
        if (item.tag == tag && !item.hasSubmenu) return item;
        if (item.hasSubmenu) {
            NSMenuItem* found = findMenuItemByTag(item.submenu, tag);
            if (found) return found;
        }
    }
    return nil;
}

static void buildMenu(NSMenu* menu, pa_menu_node* list, PAMenuTarget* target)
{
    menu.autoenablesItems = NO;
    for (pa_menu_node* m = list; m; m = m->next) {
        NSString* title = m->face ? [NSString stringWithUTF8String:m->face]
                                  : @"";
        if (m->branch) {
            NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                          action:nil
                                                   keyEquivalent:@""];
            item.tag = m->id;
            NSMenu* sub = [[NSMenu alloc] initWithTitle:title];
            buildMenu(sub, m->branch, target);
            item.submenu = sub;
            [menu addItem:item];
        } else {
            NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                          action:@selector(menuItemAction:)
                                                   keyEquivalent:@""];
            item.target = target;
            item.tag = m->id;
            [menu addItem:item];
        }
        if (m->bar) {
            [menu addItem:[NSMenuItem separatorItem]];
        }
    }
}

void pa_cocoa_menu(pa_winhan win, void* menu_list)
{
    run_on_main(^{
        pa_menu_node* list = (pa_menu_node*)menu_list;

        if (!menuTarget) {
            menuTarget = [[PAMenuTarget alloc] init];
        }
        menuTarget.owner = win;

        NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@""];
        mainMenu.autoenablesItems = NO;

        /* the application menu stays first, as it is */
        if (!savedAppMenu) {
            NSMenu* cur = [NSApp mainMenu];
            if (cur && cur.numberOfItems > 0) {
                savedAppMenu = [[cur itemAtIndex:0] copy];
            }
        }
        if (savedAppMenu) {
            [mainMenu addItem:[savedAppMenu copy]];
        } else {
            NSString* progName = [NSProcessInfo processInfo].processName;
            NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:progName
                                                              action:nil
                                                       keyEquivalent:@""];
            appItem.submenu = [[NSMenu alloc] initWithTitle:progName];
            [mainMenu addItem:appItem];
        }

        /* The program's top level items are the titles across the bar, as
           on the other platforms. A title with a branch drops the branch.
           A title with none, a one shot item at the top level, which the
           bar has no way to fire, drops a menu of that one item, which
           fires it: the bar then reads the same on every platform. (The
           manual tells programs that a one shot at the top level may not
           be obeyed; this is what becomes of one here.) */
        for (pa_menu_node* m = list; m; m = m->next) {
            NSString* title = m->face ? [NSString stringWithUTF8String:m->face]
                                      : @"";
            NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:title
                                                         action:nil
                                                  keyEquivalent:@""];
            NSMenu* sub = [[NSMenu alloc] initWithTitle:title];
            top.tag = m->id;
            sub.autoenablesItems = NO;
            if (m->branch) buildMenu(sub, m->branch, menuTarget);
            else {
                NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                              action:@selector(menuItemAction:)
                                                       keyEquivalent:@""];
                item.target = menuTarget;
                item.tag = m->id;
                [sub addItem:item];
            }
            top.submenu = sub;
            [mainMenu addItem:top];
        }

        [NSApp setMainMenu:mainMenu];
    });
}

void pa_cocoa_menu_enable(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSMenu* mainMenu = [NSApp mainMenu];
        if (!mainMenu) return;
        NSMenuItem* item = findMenuItemByTag(mainMenu, id);
        if (item) item.enabled = on ? YES : NO;
        /* a top level item is a title across the bar: the title goes grey
           with it, as the item does on the other platforms */
        for (NSInteger i = 1; i < mainMenu.numberOfItems; i++) {
            NSMenuItem* top = [mainMenu itemAtIndex:i];
            if (top.tag == id && top.hasSubmenu) top.enabled = on ? YES : NO;
        }
    });
}

void pa_cocoa_menu_check(pa_winhan win, int id, int on)
{
    run_on_main(^{
        NSMenu* mainMenu = [NSApp mainMenu];
        if (!mainMenu) return;
        NSMenuItem* item = findMenuItemByTag(mainMenu, id);
        if (item) item.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    });
}

/*----------------------------------------------------------------------------
 * Dialogs
 *----------------------------------------------------------------------------*/

void pa_cocoa_alert(const char* title, const char* message)
{
    run_on_main(^{
        NSAlert* a    = [[NSAlert alloc] init];
        a.messageText = [NSString stringWithUTF8String:title   ? title   : ""];
        a.informativeText = [NSString stringWithUTF8String:message ? message : ""];
        /* unbundled programs have no app icon, which renders as a generic
           folder; use the standard caution badge instead */
        a.icon = [NSImage imageNamed:NSImageNameCaution];
        [a addButtonWithTitle:@"OK"];
        [a runModal];
    });
}

/*--- modal query panels ---*/

/* ends a modal session from OK/Cancel buttons */
@interface PAModalEnd : NSObject
@end

@implementation PAModalEnd
- (void)ok:(id)s     { [NSApp stopModalWithCode:NSModalResponseOK]; }
- (void)cancel:(id)s { [NSApp stopModalWithCode:NSModalResponseCancel]; }
@end

static PAModalEnd* modal_end(void)
{
    static PAModalEnd* me = nil;
    if (!me) me = [[PAModalEnd alloc] init];
    return me;
}

/* create an empty titled modal panel of the given content size */
static NSWindow* modal_panel(NSString* title, int w, int h)
{
    NSWindow* p = [[NSWindow alloc]
                      initWithContentRect:NSMakeRect(0, 0, w, h)
                                styleMask:NSWindowStyleMaskTitled
                                  backing:NSBackingStoreBuffered
                                    defer:NO];
    p.releasedWhenClosed = NO;
    p.title = title;
    [p center];
    return p;
}

/* add OK and Cancel buttons along the bottom of a panel */
static void modal_buttons(NSWindow* p)
{
    NSRect  b  = [p.contentView bounds];
    NSButton* ok = [[NSButton alloc]
                       initWithFrame:NSMakeRect(b.size.width - 90, 10, 80, 28)];
    ok.title = @"OK";
    ok.bezelStyle = NSBezelStyleRounded;
    ok.keyEquivalent = @"\r";
    ok.target = modal_end();
    ok.action = @selector(ok:);
    [p.contentView addSubview:ok];

    NSButton* ca = [[NSButton alloc]
                       initWithFrame:NSMakeRect(b.size.width - 180, 10, 80, 28)];
    ca.title = @"Cancel";
    ca.bezelStyle = NSBezelStyleRounded;
    ca.keyEquivalent = @"\033";
    ca.target = modal_end();
    ca.action = @selector(cancel:);
    [p.contentView addSubview:ca];
}

static NSTextField* modal_label(NSWindow* p, NSString* text, NSRect r)
{
    NSTextField* l = [[NSTextField alloc] initWithFrame:r];
    l.stringValue     = text;
    l.bordered        = NO;
    l.editable        = NO;
    l.drawsBackground = NO;
    [p.contentView addSubview:l];
    return l;
}

static NSButton* modal_check(NSWindow* p, NSString* text, NSRect r, int on)
{
    NSButton* c = [[NSButton alloc] initWithFrame:r];
    c.buttonType = NSButtonTypeSwitch;
    c.title      = text;
    c.state      = on ? NSControlStateValueOn : NSControlStateValueOff;
    [p.contentView addSubview:c];
    return c;
}

void pa_cocoa_query_color(long* r, long* g, long* b)
{
    __block long rr = *r, rg = *g, rb = *b;
    run_on_main(^{
        NSWindow* p = modal_panel(@"Select Color", 260, 130);
        NSColorWell* well = [[NSColorWell alloc]
                                initWithFrame:NSMakeRect(80, 55, 100, 60)];
        well.color = [NSColor colorWithRed:(CGFloat)((double)rr / (double)LONG_MAX)
                                     green:(CGFloat)((double)rg / (double)LONG_MAX)
                                      blue:(CGFloat)((double)rb / (double)LONG_MAX)
                                     alpha:1.0];
        [p.contentView addSubview:well];
        modal_buttons(p);
        /* the well/color-panel link is unreliable inside a modal session;
           track the shared color panel directly and mirror it in the well */
        NSColorPanel* cp = NSColorPanel.sharedColorPanel;
        id obs = [[NSNotificationCenter defaultCenter]
                     addObserverForName:NSColorPanelColorDidChangeNotification
                                 object:cp
                                  queue:nil
                             usingBlock:^(NSNotification* n) {
                                 well.color = cp.color;
                             }];
        NSModalResponse rc = [NSApp runModalForWindow:p];
        [[NSNotificationCenter defaultCenter] removeObserver:obs];
        [cp orderOut:nil];
        [p orderOut:nil];
        if (rc == NSModalResponseOK) {
            NSColor* c = [well.color
                colorUsingColorSpace:NSColorSpace.genericRGBColorSpace];
            rr = pa_frac_to_long(c.redComponent);
            rg = pa_frac_to_long(c.greenComponent);
            rb = pa_frac_to_long(c.blueComponent);
        }
    });
    *r = rr;
    *g = rg;
    *b = rb;
}

void pa_cocoa_query_find(char* s, int sl, long* opt)
{
    __block long ropt = *opt;
    __block NSString* rstr = nil;
    NSString* init = [NSString stringWithUTF8String:s ? s : ""];
    run_on_main(^{
        NSWindow* p = modal_panel(@"Find", 340, 165);
        modal_label(p, @"Find:", NSMakeRect(15, 130, 60, 20));
        NSTextField* tf = [[NSTextField alloc]
                              initWithFrame:NSMakeRect(75, 127, 250, 24)];
        tf.stringValue = init;
        [p.contentView addSubview:tf];
        NSButton* ccase = modal_check(p, @"Case sensitive",
                                      NSMakeRect(15, 100, 200, 20),
                                      ropt & PA_QF_CASE);
        NSButton* cup   = modal_check(p, @"Search up",
                                      NSMakeRect(15, 78, 200, 20),
                                      ropt & PA_QF_UP);
        NSButton* cre   = modal_check(p, @"Regular expression",
                                      NSMakeRect(15, 56, 200, 20),
                                      ropt & PA_QF_RE);
        modal_buttons(p);
        [p makeFirstResponder:tf];
        NSModalResponse rc = [NSApp runModalForWindow:p];
        [p orderOut:nil];
        if (rc == NSModalResponseOK) {
            rstr = tf.stringValue;
            ropt = 0;
            if (ccase.state == NSControlStateValueOn) ropt |= PA_QF_CASE;
            if (cup.state   == NSControlStateValueOn) ropt |= PA_QF_UP;
            if (cre.state   == NSControlStateValueOn) ropt |= PA_QF_RE;
        } else
            rstr = @"";
    });
    if (sl > 0) {
        strncpy(s, rstr.UTF8String, sl - 1);
        s[sl - 1] = 0;
    }
    *opt = ropt;
}

void pa_cocoa_query_findrep(char* s, int sl, char* r, int rl, long* opt)
{
    __block long ropt = *opt;
    __block NSString* fstr = nil;
    __block NSString* rstr = nil;
    NSString* finit = [NSString stringWithUTF8String:s ? s : ""];
    NSString* rinit = [NSString stringWithUTF8String:r ? r : ""];
    run_on_main(^{
        NSWindow* p = modal_panel(@"Find and Replace", 360, 235);
        modal_label(p, @"Find:", NSMakeRect(15, 200, 70, 20));
        NSTextField* ff = [[NSTextField alloc]
                              initWithFrame:NSMakeRect(90, 197, 250, 24)];
        ff.stringValue = finit;
        [p.contentView addSubview:ff];
        modal_label(p, @"Replace:", NSMakeRect(15, 172, 70, 20));
        NSTextField* rf = [[NSTextField alloc]
                              initWithFrame:NSMakeRect(90, 169, 250, 24)];
        rf.stringValue = rinit;
        [p.contentView addSubview:rf];
        NSButton* ccase = modal_check(p, @"Case sensitive",
                                      NSMakeRect(15, 142, 160, 20),
                                      ropt & PA_QF_CASE);
        NSButton* cup   = modal_check(p, @"Search up",
                                      NSMakeRect(15, 120, 160, 20),
                                      ropt & PA_QF_UP);
        NSButton* cre   = modal_check(p, @"Regular expression",
                                      NSMakeRect(15, 98, 160, 20),
                                      ropt & PA_QF_RE);
        NSButton* cfnd  = modal_check(p, @"Find only",
                                      NSMakeRect(185, 142, 160, 20),
                                      ropt & PA_QF_FIND);
        NSButton* cfil  = modal_check(p, @"All in file",
                                      NSMakeRect(185, 120, 160, 20),
                                      ropt & PA_QF_ALLFIL);
        NSButton* clin  = modal_check(p, @"All on line(s)",
                                      NSMakeRect(185, 98, 160, 20),
                                      ropt & PA_QF_ALLLIN);
        modal_buttons(p);
        [p makeFirstResponder:ff];
        NSModalResponse rc = [NSApp runModalForWindow:p];
        [p orderOut:nil];
        if (rc == NSModalResponseOK) {
            fstr = ff.stringValue;
            rstr = rf.stringValue;
            ropt = 0;
            if (ccase.state == NSControlStateValueOn) ropt |= PA_QF_CASE;
            if (cup.state   == NSControlStateValueOn) ropt |= PA_QF_UP;
            if (cre.state   == NSControlStateValueOn) ropt |= PA_QF_RE;
            if (cfnd.state  == NSControlStateValueOn) ropt |= PA_QF_FIND;
            if (cfil.state  == NSControlStateValueOn) ropt |= PA_QF_ALLFIL;
            if (clin.state  == NSControlStateValueOn) ropt |= PA_QF_ALLLIN;
        } else {
            fstr = @"";
            rstr = @"";
        }
    });
    if (sl > 0) { strncpy(s, fstr.UTF8String, sl - 1); s[sl - 1] = 0; }
    if (rl > 0) { strncpy(r, rstr.UTF8String, rl - 1); r[rl - 1] = 0; }
    *opt = ropt;
}

void pa_cocoa_query_font(char* family, int famlen, long* size,
                         long* fr, long* fg, long* fb,
                         long* br, long* bg, long* bb)
{
    __block long rsize = *size;
    __block long rfr = *fr, rfg = *fg, rfb = *fb;
    __block long rbr = *br, rbg = *bg, rbb = *bb;
    __block NSString* rfam = nil;
    NSString* finit = [NSString stringWithUTF8String:family ? family : ""];
    run_on_main(^{
        NSWindow* p = modal_panel(@"Select Font", 360, 215);
        modal_label(p, @"Font:", NSMakeRect(15, 180, 60, 20));
        NSPopUpButton* fpop = [[NSPopUpButton alloc]
                                  initWithFrame:NSMakeRect(75, 176, 265, 26)
                                      pullsDown:NO];
        NSArray<NSString*>* fams =
            [[NSFontManager sharedFontManager] availableFontFamilies];
        fams = [fams sortedArrayUsingSelector:
                         @selector(caseInsensitiveCompare:)];
        for (NSString* f in fams) [fpop addItemWithTitle:f];
        if (finit.length) [fpop selectItemWithTitle:finit];
        [p.contentView addSubview:fpop];

        modal_label(p, @"Size:", NSMakeRect(15, 148, 60, 20));
        NSTextField* sf = [[NSTextField alloc]
                              initWithFrame:NSMakeRect(75, 145, 60, 24)];
        sf.integerValue = rsize;
        [p.contentView addSubview:sf];

        modal_label(p, @"Foreground:", NSMakeRect(15, 110, 100, 20));
        NSColorWell* fwell = [[NSColorWell alloc]
                                 initWithFrame:NSMakeRect(115, 100, 60, 32)];
        fwell.color = [NSColor colorWithRed:(CGFloat)((double)rfr / (double)LONG_MAX)
                                      green:(CGFloat)((double)rfg / (double)LONG_MAX)
                                       blue:(CGFloat)((double)rfb / (double)LONG_MAX)
                                      alpha:1.0];
        [p.contentView addSubview:fwell];

        modal_label(p, @"Background:", NSMakeRect(185, 110, 100, 20));
        NSColorWell* bwell = [[NSColorWell alloc]
                                 initWithFrame:NSMakeRect(285, 100, 60, 32)];
        bwell.color = [NSColor colorWithRed:(CGFloat)((double)rbr / (double)LONG_MAX)
                                      green:(CGFloat)((double)rbg / (double)LONG_MAX)
                                       blue:(CGFloat)((double)rbb / (double)LONG_MAX)
                                      alpha:1.0];
        [p.contentView addSubview:bwell];

        modal_buttons(p);
        NSColorPanel* cp = NSColorPanel.sharedColorPanel;
        id obs = [[NSNotificationCenter defaultCenter]
                     addObserverForName:NSColorPanelColorDidChangeNotification
                                 object:cp
                                  queue:nil
                             usingBlock:^(NSNotification* n) {
                                 if (fwell.isActive) fwell.color = cp.color;
                                 if (bwell.isActive) bwell.color = cp.color;
                             }];
        NSModalResponse rc = [NSApp runModalForWindow:p];
        [[NSNotificationCenter defaultCenter] removeObserver:obs];
        [cp orderOut:nil];
        [p orderOut:nil];
        if (rc == NSModalResponseOK) {
            rfam = fpop.titleOfSelectedItem;
            rsize = sf.integerValue > 0 ? (long)sf.integerValue : rsize;
            NSColor* c = [fwell.color
                colorUsingColorSpace:NSColorSpace.genericRGBColorSpace];
            rfr = pa_frac_to_long(c.redComponent);
            rfg = pa_frac_to_long(c.greenComponent);
            rfb = pa_frac_to_long(c.blueComponent);
            c = [bwell.color
                colorUsingColorSpace:NSColorSpace.genericRGBColorSpace];
            rbr = pa_frac_to_long(c.redComponent);
            rbg = pa_frac_to_long(c.greenComponent);
            rbb = pa_frac_to_long(c.blueComponent);
        }
    });
    if (rfam && famlen > 0) {
        strncpy(family, rfam.UTF8String, famlen - 1);
        family[famlen - 1] = 0;
    }
    *size = rsize;
    *fr = rfr; *fg = rfg; *fb = rfb;
    *br = rbr; *bg = rbg; *bb = rbb;
}

void pa_cocoa_query_open(char* path, int pathlen)
{
    run_on_main(^{
        NSOpenPanel* p = [NSOpenPanel openPanel];
        p.canChooseFiles    = YES;
        p.canChooseDirectories = NO;
        p.allowsMultipleSelection = NO;
        if ([p runModal] == NSModalResponseOK && p.URLs.count > 0) {
            const char* u = [p.URLs[0].path UTF8String];
            strncpy(path, u, pathlen-1);
            path[pathlen-1] = 0;
        } else {
            path[0] = 0;
        }
    });
}

void pa_cocoa_query_save(char* path, int pathlen)
{
    run_on_main(^{
        NSSavePanel* p = [NSSavePanel savePanel];
        if ([p runModal] == NSModalResponseOK && p.URL) {
            const char* u = [p.URL.path UTF8String];
            strncpy(path, u, pathlen-1);
            path[pathlen-1] = 0;
        } else {
            path[0] = 0;
        }
    });
}

void pa_cocoa_inject_close(void)
{
    pa_rawevent e = {0};
    e.type = PA_EVT_CLOSE;
    e.win  = NULL;
    evt_push(&e);
}

/*----------------------------------------------------------------------------
 * Joystick support via IOKit HID
 *----------------------------------------------------------------------------*/

#define MAXJOY 10
#define MAXJAX 6

typedef struct {
    IOHIDDeviceRef dev;
    int            axes;
    int            buttons;
    int            axsav[MAXJAX];
    IOHIDElementRef axelem[MAXJAX];
    long           axmin[MAXJAX];
    long           axmax[MAXJAX];
} joyrec;

static joyrec          joytab[MAXJOY];
static int             numjoy;
static IOHIDManagerRef hidmgr;

static int joy_find(IOHIDDeviceRef dev)
{
    for (int i = 0; i < numjoy; i++)
        if (joytab[i].dev == dev) return i;
    return -1;
}

static int scale_hid(long val, long lo, long hi)
{
    if (hi <= lo) return 0;
    long mid = (lo + hi) / 2;
    long half = (hi - lo) / 2;
    if (half == 0) return 0;
    long long scaled = (long long)(val - mid) * INT_MAX / half;
    if (scaled > INT_MAX) return INT_MAX;
    if (scaled < -INT_MAX) return -INT_MAX;
    return (int)scaled;
}

static int axis_index_for_usage(uint32_t usage)
{
    switch (usage) {
    case kHIDUsage_GD_X:  return 0;
    case kHIDUsage_GD_Y:  return 1;
    case kHIDUsage_GD_Z:  return 2;
    case kHIDUsage_GD_Rx: return 3;
    case kHIDUsage_GD_Ry: return 4;
    case kHIDUsage_GD_Rz: return 5;
    default: return -1;
    }
}

static void hid_input_cb(void* ctx, IOReturn result, void* sender,
                          IOHIDValueRef value)
{
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    IOHIDDeviceRef  dev  = IOHIDElementGetDevice(elem);
    int idx = joy_find(dev);
    if (idx < 0) return;
    joyrec* jp = &joytab[idx];

    uint32_t page  = IOHIDElementGetUsagePage(elem);
    uint32_t usage = IOHIDElementGetUsage(elem);
    long     raw   = IOHIDValueGetIntegerValue(value);

    if (page == kHIDPage_GenericDesktop) {
        int ai = axis_index_for_usage(usage);
        if (ai >= 0 && ai < jp->axes) {
            jp->axsav[ai] = scale_hid(raw, jp->axmin[ai], jp->axmax[ai]);
            pa_rawevent e = {0};
            e.type = PA_EVT_JOY_MOVE;
            e.win  = NULL;
            e.joymove.jn = idx;
            for (int i = 0; i < MAXJAX; i++)
                e.joymove.ax[i] = jp->axsav[i];
            evt_push(&e);
        }
        if (usage == kHIDUsage_GD_Hatswitch) {
            /* hat switch: map to axes 4,5 if not already used */
            /* for now just skip hat */
        }
    } else if (page == kHIDPage_Button) {
        int btn = (int)usage; /* 1-based */
        pa_rawevent e = {0};
        e.type = raw ? PA_EVT_JOY_DOWN : PA_EVT_JOY_UP;
        e.win  = NULL;
        e.joybtn.jn  = idx;
        e.joybtn.btn = btn;
        evt_push(&e);
    }
}

static void hid_match_cb(void* ctx, IOReturn result, void* sender,
                          IOHIDDeviceRef dev)
{
    if (numjoy >= MAXJOY) return;
    if (joy_find(dev) >= 0) return;

    joyrec* jp = &joytab[numjoy];
    memset(jp, 0, sizeof(*jp));
    jp->dev = dev;

    /* enumerate elements to count axes and buttons */
    CFArrayRef elems = IOHIDDeviceCopyMatchingElements(dev, NULL,
                           kIOHIDOptionsTypeNone);
    if (!elems) { jp->dev = NULL; return; }

    int nax = 0, nbtn = 0;
    CFIndex n = CFArrayGetCount(elems);
    for (CFIndex i = 0; i < n; i++) {
        IOHIDElementRef el = (IOHIDElementRef)CFArrayGetValueAtIndex(elems, i);
        IOHIDElementType etype = IOHIDElementGetType(el);
        if (etype != kIOHIDElementTypeInput_Misc &&
            etype != kIOHIDElementTypeInput_Axis &&
            etype != kIOHIDElementTypeInput_Button)
            continue;
        uint32_t pg = IOHIDElementGetUsagePage(el);
        uint32_t us = IOHIDElementGetUsage(el);

        if (pg == kHIDPage_GenericDesktop) {
            int ai = axis_index_for_usage(us);
            if (ai >= 0 && ai < MAXJAX) {
                jp->axelem[ai] = el;
                jp->axmin[ai]  = IOHIDElementGetLogicalMin(el);
                jp->axmax[ai]  = IOHIDElementGetLogicalMax(el);
                if (ai + 1 > nax) nax = ai + 1;
            }
        } else if (pg == kHIDPage_Button) {
            int bn = (int)us;
            if (bn > nbtn) nbtn = bn;
        }
    }
    CFRelease(elems);

    jp->axes    = nax > MAXJAX ? MAXJAX : nax;
    jp->buttons = nbtn;
    numjoy++;
}

static void hid_remove_cb(void* ctx, IOReturn result, void* sender,
                           IOHIDDeviceRef dev)
{
    int idx = joy_find(dev);
    if (idx < 0) return;
    joytab[idx].dev = NULL;
    joytab[idx].axes = 0;
    joytab[idx].buttons = 0;
}

void pa_cocoa_joy_init(void)
{
    numjoy = 0;
    hidmgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!hidmgr) return;

    /* match joysticks and gamepads */
    NSDictionary* joy = @{
        @(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
        @(kIOHIDDeviceUsageKey):     @(kHIDUsage_GD_Joystick)
    };
    NSDictionary* pad = @{
        @(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
        @(kIOHIDDeviceUsageKey):     @(kHIDUsage_GD_GamePad)
    };
    NSDictionary* multi = @{
        @(kIOHIDDeviceUsagePageKey): @(kHIDPage_GenericDesktop),
        @(kIOHIDDeviceUsageKey):     @(kHIDUsage_GD_MultiAxisController)
    };
    IOHIDManagerSetDeviceMatchingMultiple(hidmgr,
        (__bridge CFArrayRef)@[joy, pad, multi]);

    IOHIDManagerRegisterDeviceMatchingCallback(hidmgr, hid_match_cb, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(hidmgr, hid_remove_cb, NULL);
    IOHIDManagerRegisterInputValueCallback(hidmgr, hid_input_cb, NULL);
    IOHIDManagerScheduleWithRunLoop(hidmgr, CFRunLoopGetMain(),
                                    kCFRunLoopDefaultMode);
    IOHIDManagerOpen(hidmgr, kIOHIDOptionsTypeNone);

    /* pump the run loop once to pick up already-connected devices */
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
}

void pa_cocoa_joy_deinit(void)
{
    if (hidmgr) {
        IOHIDManagerClose(hidmgr, kIOHIDOptionsTypeNone);
        IOHIDManagerUnscheduleFromRunLoop(hidmgr, CFRunLoopGetMain(),
                                          kCFRunLoopDefaultMode);
        CFRelease(hidmgr);
        hidmgr = NULL;
    }
    for (int i = 0; i < numjoy; i++) {
        joytab[i].dev = NULL;
        joytab[i].axes = 0;
        joytab[i].buttons = 0;
    }
    numjoy = 0;
}

int pa_cocoa_joy_count(void)   { return numjoy; }

int pa_cocoa_joy_buttons(int j)
{
    if (j < 1 || j > numjoy) return 0;
    return joytab[j-1].buttons;
}

int pa_cocoa_joy_axes(int j)
{
    if (j < 1 || j > numjoy) return 0;
    int a = joytab[j-1].axes;
    return a > MAXJAX ? MAXJAX : a;
}

CTFontRef pa_cocoa_system_mono_font(CGFloat size)
{
    NSFont* f = [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular];
    if (f) return CTFontCreateWithName((__bridge CFStringRef)[f fontName], size, NULL);
    return NULL;
}
