/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* macOS/Cocoa version. Grabs the pixel contents of the Ami offscreen bitmap    *
* and appends it as a PNG to a file called "test_images". Multiple PNGs are    *
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

#include "pa_cocoa.h"

#define CAPTURE_FILENAME "test_images"

/* ---------- module state ---------- */

static FILE     *cap_file        = NULL;
static uint32_t  cap_frame_count = 0;
static int       cap_disabled    = 0;

/* Get the pa_winhan for the stdout window (defined in macosx/graphics.c) */
extern pa_winhan pa_stdout_winhan(void);

/* ---------- public entry point ---------- */

void screen_capture(void) {
    if (cap_disabled || !cap_file) return;

    pa_winhan wh = pa_stdout_winhan();
    if (!wh) return;

    CGContextRef ctx = pa_cocoa_get_context(wh);
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

    CGImageDestinationAddImage(dest, img, NULL);
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

__attribute__((constructor(103)))
static void screen_capture_init(void) {
    remove(CAPTURE_FILENAME);
    cap_file = fopen(CAPTURE_FILENAME, "wb");
    if (!cap_file) {
        cap_disabled = 1;
        return;
    }
}

__attribute__((destructor(103)))
static void screen_capture_fini(void) {
    if (cap_file) {
        fflush(cap_file);
        fclose(cap_file);
        cap_file = NULL;
        if (cap_frame_count == 0) {
            remove(CAPTURE_FILENAME);
        }
    }
}
