/*******************************************************************************
*                                                                              *
*                     STANDALONE macOS PNG STREAM VIEWER                        *
*                                                                              *
* Walks through a file of concatenated PNGs (as produced by the macOS          *
* screen_capture module) and displays them one at a time in a Cocoa window.    *
* Uses only Cocoa/CoreGraphics/ImageIO — no Ami dependency.                    *
*                                                                              *
* Keys:                                                                        *
*   right arrow  — next frame                                                  *
*   left arrow   — previous frame                                              *
*   q / close button — quit                                                    *
*                                                                              *
* Usage:                                                                       *
*   bin/testviewer [filename]       # default: test_images                     *
*                                                                              *
*******************************************************************************/

/*
 * This file is compiled as Objective-C via -x objective-c in the Makefile.
 * It is standalone — no Ami library dependency.
 */
#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#else
#error "testviewer.c must be compiled as Objective-C (use -x objective-c)"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ImageIO/ImageIO.h>

#define DEFAULT_FILENAME "test_images"

/* PNG signature: 8 bytes */
static const uint8_t png_sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

typedef struct {
    long     offset;    /* byte offset within file */
    long     size;      /* total PNG size in bytes */
} frame_idx_t;

static frame_idx_t *frames = NULL;
static int          nframes = 0;
static int          cur_frame = -1;
static const char  *src_filename = NULL;

/* ---------- PNG stream indexing ---------- */

/*
 * PNG files consist of the 8-byte signature followed by chunks.
 * Each chunk is: 4-byte length (big-endian) + 4-byte type + data + 4-byte CRC.
 * The last chunk is IEND. We walk chunks to find the end of each PNG.
 */

static uint32_t read_u32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static int build_index(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "testviewer: cannot open %s\n", filename);
        return -1;
    }

    /* get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int cap = 16;
    frames = (frame_idx_t *)malloc(sizeof(frame_idx_t) * cap);
    if (!frames) { fclose(f); return -1; }

    while (ftell(f) < file_size) {
        long png_start = ftell(f);

        /* read and verify PNG signature */
        uint8_t sig[8];
        if (fread(sig, 1, 8, f) != 8 || memcmp(sig, png_sig, 8) != 0)
            break;

        /* walk chunks until IEND */
        for (;;) {
            uint8_t chunk_hdr[8];
            if (fread(chunk_hdr, 1, 8, f) != 8) goto done;
            uint32_t chunk_len = read_u32_be(chunk_hdr);
            int is_iend = (memcmp(chunk_hdr + 4, "IEND", 4) == 0);

            /* skip chunk data + 4-byte CRC */
            fseek(f, (long)chunk_len + 4, SEEK_CUR);
            if (is_iend) break;
        }

        long png_end = ftell(f);

        if (nframes >= cap) {
            cap *= 2;
            frames = (frame_idx_t *)realloc(frames, sizeof(frame_idx_t) * cap);
        }
        frames[nframes].offset = png_start;
        frames[nframes].size   = png_end - png_start;
        nframes++;
    }

done:
    fclose(f);
    return 0;
}

/* ---------- load a PNG frame as CGImage ---------- */

static CGImageRef load_frame_image(int i) {
    if (i < 0 || i >= nframes) return NULL;

    FILE *f = fopen(src_filename, "rb");
    if (!f) return NULL;
    fseek(f, frames[i].offset, SEEK_SET);

    uint8_t *buf = (uint8_t *)malloc(frames[i].size);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, frames[i].size, f);
    fclose(f);

    CFDataRef data = CFDataCreate(NULL, buf, frames[i].size);
    free(buf);
    if (!data) return NULL;

    CGImageSourceRef src = CGImageSourceCreateWithData(data, NULL);
    CFRelease(data);
    if (!src) return NULL;

    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    return img;
}

/* ---------- Cocoa viewer ---------- */

@interface ViewerView : NSView {
@public
    CGImageRef currentImage;
}
@end

@implementation ViewerView

- (BOOL)acceptsFirstResponder { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    if (!currentImage) return;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];

    /* fit image in view, preserving aspect ratio */
    NSSize vs = self.bounds.size;
    size_t iw = CGImageGetWidth(currentImage);
    size_t ih = CGImageGetHeight(currentImage);
    double sx = vs.width / (double)iw;
    double sy = vs.height / (double)ih;
    double s  = sx < sy ? sx : sy;
    double dw = iw * s;
    double dh = ih * s;
    double ox = (vs.width - dw) / 2.0;
    double oy = (vs.height - dh) / 2.0;

    /* clear background */
    CGContextSetRGBFillColor(ctx, 0.2, 0.2, 0.2, 1.0);
    CGContextFillRect(ctx, NSRectToCGRect(self.bounds));

    CGContextDrawImage(ctx, CGRectMake(ox, oy, dw, dh), currentImage);
}

- (void)keyDown:(NSEvent *)event {
    unsigned short kc = event.keyCode;
    NSString *chars = event.characters;

    if (kc == 124) { /* right arrow */
        if (cur_frame + 1 < nframes) {
            cur_frame++;
            if (currentImage) CGImageRelease(currentImage);
            currentImage = load_frame_image(cur_frame);
            [self setNeedsDisplay:YES];
            [self.window setTitle:[NSString stringWithFormat:@"testviewer [%d/%d]",
                                   cur_frame + 1, nframes]];
        }
    } else if (kc == 123) { /* left arrow */
        if (cur_frame > 0) {
            cur_frame--;
            if (currentImage) CGImageRelease(currentImage);
            currentImage = load_frame_image(cur_frame);
            [self setNeedsDisplay:YES];
            [self.window setTitle:[NSString stringWithFormat:@"testviewer [%d/%d]",
                                   cur_frame + 1, nframes]];
        }
    } else if (chars.length > 0 && ([chars characterAtIndex:0] == 'q' ||
                                     [chars characterAtIndex:0] == 'Q')) {
        [self.window close];
    }
}

- (void)dealloc {
    if (currentImage) CGImageRelease(currentImage);
    [super dealloc];
}

@end

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    src_filename = (argc > 1) ? argv[1] : DEFAULT_FILENAME;

    if (build_index(src_filename) != 0) return 1;
    if (nframes == 0) {
        fprintf(stderr, "testviewer: %s contains no PNG frames\n", src_filename);
        return 1;
    }
    fprintf(stderr, "testviewer: indexed %d frame(s) from %s\n", nframes, src_filename);

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];

    /* load first frame to get dimensions */
    cur_frame = 0;
    CGImageRef firstImg = load_frame_image(0);
    if (!firstImg) {
        fprintf(stderr, "testviewer: failed to load frame 0\n");
        free(frames);
        return 1;
    }
    int ww = (int)CGImageGetWidth(firstImg);
    int wh = (int)CGImageGetHeight(firstImg);
    fprintf(stderr, "testviewer: frame 0 size %dx%d\n", ww, wh);

    NSRect winRect = NSMakeRect(100, 100, ww, wh);
    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:winRect
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];

    NSRect viewRect = NSMakeRect(0, 0, ww, wh);
    ViewerView *view = [[ViewerView alloc] initWithFrame:viewRect];
    [window setContentView:view];

    view->currentImage = firstImg; /* view takes ownership */
    [window setTitle:[NSString stringWithFormat:@"testviewer [1/%d]", nframes]];

    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    /* event loop */
    int running = 1;
    while (running) {
        NSAutoreleasePool *innerPool = [[NSAutoreleasePool alloc] init];
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantFuture]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
            [NSApp updateWindows];
            if (![window isVisible]) running = 0;
        }
        [innerPool drain];
    }

    free(frames);
    /* don't drain outer pool — Cocoa internals may have stale autoreleased objects */
    return 0;
}
