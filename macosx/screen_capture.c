/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* macOS/Cocoa version. Grabs the pixel contents of the Ami offscreen bitmap    *
* and appends it as a PNG to a file called "test_images.img". Multiple PNGs are    *
* concatenated back-to-back in the same file; each is self-contained.          *
*                                                                              *
* The file is opened by a module constructor and closed by a destructor.       *
* The caller only needs to call screen_capture() at each interesting moment    *
* (e.g. just before waitnext() in a test program).                             *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

typedef void* pa_winhan;

#include <dlfcn.h>

#define CAPTURE_FILENAME "test_images.img"

/* ---------- module state ---------- */

static char      cap_name[1024] = CAPTURE_FILENAME;
static int       cap_opened     = 0;   /* the file has been made */
static FILE     *cap_file        = NULL;
static uint32_t  cap_frame_count = 0;
static int       cap_disabled    = 0;

/* Function pointers resolved at runtime via dlsym so screen_capture works
   in both graphical and terminal (non-graphical) builds. */
typedef pa_winhan (*fn_stdout_winhan)(void);
typedef CGContextRef (*fn_get_context)(pa_winhan);
static fn_stdout_winhan  p_stdout_winhan;
static fn_get_context    p_get_context;
static int               syms_resolved;

/* ---------- public entry point ---------- */

/* Name the file the pictures go to, before the first of them is taken.
   Later than that the file is already open and the name is ignored. */

/* the label of the next picture, kept in it as its PNG title */
static char cap_label[80];

/* Label the next picture, "frame N" or "frame N.S": it is kept in the picture
   as its title, and the viewer's shifted arrows step by it. One picture. */
void screen_capture_label(const char* s) {
    strncpy(cap_label, s, sizeof(cap_label)-1);
}

void screen_capture_name(const char* fn) {
    if (!fn || !*fn || cap_opened) return;
    snprintf(cap_name, sizeof(cap_name), "%s", fn);
}

/* the file is made at the first capture, so a name can be given first */
static void capopen(void) {
    cap_opened = 1;
    remove(cap_name);
    cap_file = fopen(cap_name, "wb");
    if (!cap_file) cap_disabled = 1;
}

void screen_capture(void) {
    if (cap_disabled) return;
    if (!cap_opened) capopen();
    if (!cap_file) return;

    if (!syms_resolved) {
        syms_resolved = 1;
        p_stdout_winhan = (fn_stdout_winhan)dlsym(RTLD_DEFAULT,
                                                   "pa_stdout_winhan");
        p_get_context   = (fn_get_context)dlsym(RTLD_DEFAULT,
                                                 "pa_cocoa_get_context");
    }
    if (!p_stdout_winhan || !p_get_context) return;

    pa_winhan wh = p_stdout_winhan();
    if (!wh) return;

    CGContextRef ctx = p_get_context(wh);
    if (!ctx) return;

    CGImageRef raw = CGBitmapContextCreateImage(ctx);
    if (!raw) return;

    /* CGBitmapContextCreateImage returns an image in CG's Y-up coordinate
     * system, but our bitmap was drawn in a flipped (Y-down) context.
     * Flip it so the PNG is right-side up when viewed in any standard tool. */
    size_t pw = CGImageGetWidth(raw);
    size_t ph = CGImageGetHeight(raw);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef flipCtx = CGBitmapContextCreate(NULL, pw, ph, 8, pw * 4, cs,
                               kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(cs);
    if (!flipCtx) { CGImageRelease(raw); return; }
    CGContextTranslateCTM(flipCtx, 0, ph);
    CGContextScaleCTM(flipCtx, 1.0, -1.0);
    CGContextDrawImage(flipCtx, CGRectMake(0, 0, pw, ph), raw);
    CGImageRelease(raw);
    CGImageRef img = CGBitmapContextCreateImage(flipCtx);
    CGContextRelease(flipCtx);
    if (!img) return;

    /* render PNG to an in-memory buffer via CGImageDestination */
    CFMutableDataRef pngData = CFDataCreateMutable(NULL, 0);
    if (!pngData) { CGImageRelease(img); return; }

    CGImageDestinationRef dest = CGImageDestinationCreateWithData(
        pngData, CFSTR("public.png"), 1, NULL);
    if (!dest) { CFRelease(pngData); CGImageRelease(img); return; }

    if (cap_label[0]) {
        /* the label rides as the PNG title: the viewer steps whole frames by it */
        CFStringRef title = CFStringCreateWithCString(NULL, cap_label,
                                                      kCFStringEncodingUTF8);
        const void* pk[1] = { kCGImagePropertyPNGTitle };
        const void* pv[1] = { title };
        CFDictionaryRef pngd = CFDictionaryCreate(NULL, pk, pv, 1,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        const void* dk[1] = { kCGImagePropertyPNGDictionary };
        const void* dv[1] = { pngd };
        CFDictionaryRef props = CFDictionaryCreate(NULL, dk, dv, 1,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CGImageDestinationAddImage(dest, img, props);
        CFRelease(props);
        CFRelease(pngd);
        CFRelease(title);
        cap_label[0] = 0;
    } else CGImageDestinationAddImage(dest, img, NULL);
    CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CGImageRelease(img);

    /* append PNG bytes to the concatenated file */
    const uint8_t *bytes = CFDataGetBytePtr(pngData);
    CFIndex len = CFDataGetLength(pngData);
    fwrite(bytes, 1, (size_t)len, cap_file);
    fflush(cap_file);
    CFRelease(pngData);

    cap_frame_count++;
}

/* ---------- module lifecycle ---------- */

__attribute__((destructor(103)))
static void screen_capture_fini(void) {
    if (cap_file) {
        fflush(cap_file);
        fclose(cap_file);
        cap_file = NULL;
        if (cap_frame_count == 0) {
            remove(cap_name);
        }
    }
}
