/** ****************************************************************************
*                                                                              *
*                        JOYSTICK VIA IOKIT HID FOR MAC OS X                   *
*                                                                              *
* The terminal backend (linux/terminal.c, built as is for Mac OS X) reads     *
* joysticks the Linux way: a device file per stick, /dev/input/jsN, that      *
* delivers struct js_event records, watched by the event loop as an input      *
* file. Mac OS X has no such device; joysticks are IOKit HID devices,          *
* delivered by callback on a run loop.                                         *
*                                                                              *
* This module presents the IOKit joysticks in the shape the terminal expects:  *
* one pipe per stick, whose read end is the "device file". A thread of its     *
* own runs the HID manager's run loop (the terminal has no Cocoa main loop     *
* to borrow) and writes each axis change or button change down the stick's    *
* pipe as a js_event, in the Linux layout: axes scaled to +-32767, buttons     *
* numbered from 0, as the Linux driver numbers them. The terminal's reader     *
* and its kqueue registration then work unchanged.                             *
*                                                                              *
* Devices present at start are found on the first pass of the run loop;       *
* a device that arrives later is added when the manager reports it, up to      *
* the table size; a device that leaves has its pipe's write end closed, so    *
* the reader sees end of file and retires it, as a Linux unplug reads.        *
*                                                                              *
* Calls in this module:                                                        *
*                                                                              *
* int  pa_hid_joy_open(int n);                                                 *
*                                                                              *
* Starts the HID manager on first call. Returns the read end of joystick       *
* n's pipe (n from 0), or -1 when there is no such joystick; the terminal      *
* calls it in sequence until -1, as it opens /dev/input/js0, js1, ... on       *
* Linux.                                                                       *
*                                                                              *
* int  pa_hid_joy_axes(int n);  int pa_hid_joy_buttons(int n);                 *
*                                                                              *
* The axis and button counts of joystick n, standing in for the JSIOCGAXES     *
* and JSIOCGBUTTONS ioctls.                                                    *
*                                                                              *
* void pa_hid_joy_close(void);                                                 *
*                                                                              *
* Stops the manager and its thread and closes the pipes.                       *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <stdint.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>

#include "joystick_hid.h"

#define MAXJOY 10   /* joysticks, as the terminal's table */
#define MAXJAX 6    /* axes carried per joystick, as the terminal reads */

/* the Linux joystick event, as linux/joystick.h lays it out */
struct js_event {
    uint32_t time;   /* event timestamp in milliseconds */
    int16_t  value;  /* value */
    uint8_t  type;   /* event type */
    uint8_t  number; /* axis/button number */
};
#define JS_EVENT_BUTTON 0x01
#define JS_EVENT_AXIS   0x02

typedef struct {
    IOHIDDeviceRef dev;          /* the device, NULL after removal */
    int            rfd, wfd;     /* the pipe: the terminal reads rfd */
    int            axes;         /* axes found */
    int            buttons;      /* buttons found */
    long           axmin[MAXJAX];
    long           axmax[MAXJAX];
} joyrec;

static joyrec          joytab[MAXJOY];
static int             numjoy;
static IOHIDManagerRef hidmgr;
static CFRunLoopRef    hidloop;       /* the manager's run loop, on hidthread */
static pthread_t       hidthread;
static int             started;       /* the manager is up */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ready = PTHREAD_COND_INITIALIZER; /* the first pass is done */
static int             firstpass;

static int joy_find(IOHIDDeviceRef dev)
{
    int i;
    for (i = 0; i < numjoy; i++) if (joytab[i].dev == dev) return i;
    return -1;
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

/* a raw axis reading to the Linux driver's +-32767 */
static int16_t scale_axis(long val, long lo, long hi)
{
    long mid, half, scaled;
    if (hi <= lo) return 0;
    mid = (lo + hi) / 2;
    half = (hi - lo) / 2;
    if (half == 0) return 0;
    scaled = (val - mid) * 32767 / half;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32767) scaled = -32767;
    return (int16_t)scaled;
}

static uint32_t now_ms(void)
{
    return (uint32_t)(CFAbsoluteTimeGetCurrent() * 1000.0);
}

static void send_event(joyrec* jp, uint8_t type, uint8_t number, int16_t value)
{
    struct js_event ev;
    if (jp->wfd < 0) return;
    ev.time = now_ms();
    ev.value = value;
    ev.type = type;
    ev.number = number;
    /* a full pipe drops the event rather than stall the HID thread */
    (void)write(jp->wfd, &ev, sizeof(ev));
}

static void hid_input_cb(void* ctx, IOReturn result, void* sender,
                         IOHIDValueRef value)
{
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    IOHIDDeviceRef  dev  = IOHIDElementGetDevice(elem);
    uint32_t page, usage;
    long raw;
    int idx;
    joyrec* jp;
    (void)ctx; (void)result; (void)sender;
    pthread_mutex_lock(&lock);
    idx = joy_find(dev);
    if (idx < 0) { pthread_mutex_unlock(&lock); return; }
    jp = &joytab[idx];
    page  = IOHIDElementGetUsagePage(elem);
    usage = IOHIDElementGetUsage(elem);
    raw   = IOHIDValueGetIntegerValue(value);
    if (page == kHIDPage_GenericDesktop) {
        int ai = axis_index_for_usage(usage);
        if (ai >= 0 && ai < jp->axes)
            send_event(jp, JS_EVENT_AXIS, (uint8_t)ai,
                       scale_axis(raw, jp->axmin[ai], jp->axmax[ai]));
    } else if (page == kHIDPage_Button) {
        /* HID numbers buttons from 1; the Linux driver from 0, and the
           terminal adds one back */
        if (usage >= 1)
            send_event(jp, JS_EVENT_BUTTON, (uint8_t)(usage - 1), raw ? 1 : 0);
    }
    pthread_mutex_unlock(&lock);
}

static void hid_match_cb(void* ctx, IOReturn result, void* sender,
                         IOHIDDeviceRef dev)
{
    joyrec* jp;
    CFArrayRef elems;
    CFIndex n, i;
    int nax = 0, nbtn = 0, fds[2];
    (void)ctx; (void)result; (void)sender;
    pthread_mutex_lock(&lock);
    if (numjoy >= MAXJOY || joy_find(dev) >= 0) { pthread_mutex_unlock(&lock); return; }
    elems = IOHIDDeviceCopyMatchingElements(dev, NULL, kIOHIDOptionsTypeNone);
    if (!elems) { pthread_mutex_unlock(&lock); return; }
    if (pipe(fds)) { CFRelease(elems); pthread_mutex_unlock(&lock); return; }
    jp = &joytab[numjoy];
    memset(jp, 0, sizeof(*jp));
    jp->dev = dev;
    jp->rfd = fds[0];
    jp->wfd = fds[1];
    n = CFArrayGetCount(elems);
    for (i = 0; i < n; i++) {
        IOHIDElementRef el = (IOHIDElementRef)CFArrayGetValueAtIndex(elems, i);
        IOHIDElementType et = IOHIDElementGetType(el);
        uint32_t pg, us;
        if (et != kIOHIDElementTypeInput_Misc && et != kIOHIDElementTypeInput_Axis &&
            et != kIOHIDElementTypeInput_Button) continue;
        pg = IOHIDElementGetUsagePage(el);
        us = IOHIDElementGetUsage(el);
        if (pg == kHIDPage_GenericDesktop) {
            int ai = axis_index_for_usage(us);
            if (ai >= 0 && ai < MAXJAX) {
                jp->axmin[ai] = IOHIDElementGetLogicalMin(el);
                jp->axmax[ai] = IOHIDElementGetLogicalMax(el);
                if (ai + 1 > nax) nax = ai + 1;
            }
        } else if (pg == kHIDPage_Button) {
            if ((int)us > nbtn) nbtn = (int)us;
        }
    }
    CFRelease(elems);
    jp->axes = nax > MAXJAX ? MAXJAX : nax;
    jp->buttons = nbtn;
    numjoy++;
    pthread_mutex_unlock(&lock);
}

static void hid_remove_cb(void* ctx, IOReturn result, void* sender,
                          IOHIDDeviceRef dev)
{
    int idx;
    (void)ctx; (void)result; (void)sender;
    pthread_mutex_lock(&lock);
    idx = joy_find(dev);
    if (idx >= 0) {
        joytab[idx].dev = NULL;
        /* the reader sees end of file and retires the stick, as a Linux
           unplug reads */
        if (joytab[idx].wfd >= 0) { close(joytab[idx].wfd); joytab[idx].wfd = -1; }
    }
    pthread_mutex_unlock(&lock);
}

/* The manager's thread: set up on its own run loop, make the first pass
   (which reports the devices already present), signal that, then serve. */
static void* hid_thread(void* arg)
{
    CFMutableDictionaryRef m[3];
    CFArrayRef matches;
    int usages[3] = { kHIDUsage_GD_Joystick, kHIDUsage_GD_GamePad,
                      kHIDUsage_GD_MultiAxisController };
    int i;
    (void)arg;
    hidloop = CFRunLoopGetCurrent();
    hidmgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    for (i = 0; i < 3; i++) {
        int page = kHIDPage_GenericDesktop;
        CFNumberRef p = CFNumberCreate(NULL, kCFNumberIntType, &page);
        CFNumberRef u = CFNumberCreate(NULL, kCFNumberIntType, &usages[i]);
        m[i] = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(m[i], CFSTR(kIOHIDDeviceUsagePageKey), p);
        CFDictionarySetValue(m[i], CFSTR(kIOHIDDeviceUsageKey), u);
        CFRelease(p); CFRelease(u);
    }
    matches = CFArrayCreate(NULL, (const void**)m, 3, &kCFTypeArrayCallBacks);
    IOHIDManagerSetDeviceMatchingMultiple(hidmgr, matches);
    CFRelease(matches);
    for (i = 0; i < 3; i++) CFRelease(m[i]);
    IOHIDManagerRegisterDeviceMatchingCallback(hidmgr, hid_match_cb, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(hidmgr, hid_remove_cb, NULL);
    IOHIDManagerRegisterInputValueCallback(hidmgr, hid_input_cb, NULL);
    IOHIDManagerScheduleWithRunLoop(hidmgr, hidloop, kCFRunLoopDefaultMode);
    IOHIDManagerOpen(hidmgr, kIOHIDOptionsTypeNone);
    /* the first pass reports the devices already connected */
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    pthread_mutex_lock(&lock);
    firstpass = 1;
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
    CFRunLoopRun(); /* until pa_hid_joy_close stops it */
    return NULL;
}

static void start(void)
{
    if (started) return;
    started = 1;
    numjoy = 0;
    if (pthread_create(&hidthread, NULL, hid_thread, NULL)) { started = 0; return; }
    pthread_mutex_lock(&lock);
    while (!firstpass) pthread_cond_wait(&ready, &lock);
    pthread_mutex_unlock(&lock);
}

int pa_hid_joy_open(int n)
{
    int fd = -1;
    start();
    pthread_mutex_lock(&lock);
    if (n >= 0 && n < numjoy) fd = joytab[n].rfd;
    pthread_mutex_unlock(&lock);
    return fd;
}

int pa_hid_joy_axes(int n)
{
    int r = 0;
    pthread_mutex_lock(&lock);
    if (n >= 0 && n < numjoy) r = joytab[n].axes;
    pthread_mutex_unlock(&lock);
    return r;
}

int pa_hid_joy_buttons(int n)
{
    int r = 0;
    pthread_mutex_lock(&lock);
    if (n >= 0 && n < numjoy) r = joytab[n].buttons;
    pthread_mutex_unlock(&lock);
    return r;
}

void pa_hid_joy_close(void)
{
    int i;
    if (!started) return;
    if (hidloop) CFRunLoopStop(hidloop);
    pthread_join(hidthread, NULL);
    if (hidmgr) {
        IOHIDManagerClose(hidmgr, kIOHIDOptionsTypeNone);
        CFRelease(hidmgr);
        hidmgr = NULL;
    }
    for (i = 0; i < numjoy; i++) {
        if (joytab[i].wfd >= 0) close(joytab[i].wfd);
        /* rfd is the terminal's to close, as its joystick file */
        joytab[i].wfd = -1;
        joytab[i].dev = NULL;
    }
    numjoy = 0;
    started = 0;
    firstpass = 0;
}
