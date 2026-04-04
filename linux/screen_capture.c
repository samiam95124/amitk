/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* Test foundation for Petit-Ami graphics. Exposes screen_capture(), which      *
* grabs the pixel contents of the running Petit-Ami X window and appends it    *
* as a 24-bit uncompressed BMP to a file called "test_images". Multiple BMPs   *
* are concatenated back-to-back in the same file; each is self-contained       *
* (its own BITMAPFILEHEADER) so readers can walk them sequentially.            *
*                                                                              *
* The file is opened by a module constructor and closed by a destructor, so    *
* the caller only needs to call screen_capture() at each interesting moment    *
* (e.g. just before waitnext() in a test program).                             *
*                                                                              *
* Bit-for-bit comparability:                                                   *
*   - Uncompressed 24-bit BGR, no palette, no compression, no randomness.      *
*   - For a deterministic test run, `cmp` on two test_images files is a        *
*     pass/fail signal.                                                        *
*                                                                              *
* Window discovery:                                                            *
*   - Opens its own X connection via XOpenDisplay(NULL).                       *
*   - Walks EWMH _NET_CLIENT_LIST, matches window by _NET_WM_PID == getpid().  *
*   - Falls back to the largest mapped top-level window.                       *
*                                                                              *
* Viewer:                                                                      *
*   bin/testviewer    # walks through the concatenated BMPs with arrow keys    *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define CAPTURE_FILENAME "test_images"

/* ---------- module state ---------- */

static FILE     *cap_file     = NULL;
static Display  *cap_display  = NULL;
static Window    cap_window   = 0;      /* discovered lazily on first capture */
static uint32_t  cap_frame_count = 0;
static int       cap_disabled = 0;

/* ---------- little-endian writers ---------- */

static void w_u16(FILE *f, uint16_t v) {
    uint8_t b[2] = { (uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff) };
    fwrite(b, 1, 2, f);
}

static void w_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v & 0xff),
        (uint8_t)((v >> 8) & 0xff),
        (uint8_t)((v >> 16) & 0xff),
        (uint8_t)((v >> 24) & 0xff)
    };
    fwrite(b, 1, 4, f);
}

static void w_s32(FILE *f, int32_t v) { w_u32(f, (uint32_t)v); }

/* ---------- X window discovery ---------- */

/*
 * Return 1 if window w's _NET_WM_PID atom matches our pid, 0 otherwise.
 * Returns -1 if the atom is not set on the window.
 */
static int window_pid_matches(Display *d, Window w, pid_t our_pid) {
    Atom net_wm_pid = XInternAtom(d, "_NET_WM_PID", True);
    if (net_wm_pid == None) return -1;

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(d, w, net_wm_pid, 0, 1, False, XA_CARDINAL,
                           &actual_type, &actual_format, &nitems, &bytes_after,
                           &prop) != Success) {
        return -1;
    }
    if (actual_type != XA_CARDINAL || nitems != 1 || prop == NULL) {
        if (prop) XFree(prop);
        return -1;
    }
    pid_t wpid = (pid_t)(*(unsigned long *)prop);
    XFree(prop);
    return (wpid == our_pid) ? 1 : 0;
}

/*
 * Locate the Petit-Ami window on this display. Strategy:
 *   1. Walk _NET_CLIENT_LIST, return first window with _NET_WM_PID == getpid().
 *   2. If nothing matches, walk root's direct children and return the largest
 *      mapped InputOutput window.
 * Returns 0 if nothing found.
 */
static Window find_pa_window(Display *d) {
    Window root = DefaultRootWindow(d);
    pid_t  our_pid = getpid();

    Atom client_list = XInternAtom(d, "_NET_CLIENT_LIST", True);
    if (client_list != None) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = NULL;
        if (XGetWindowProperty(d, root, client_list, 0, 4096, False,
                               XA_WINDOW, &actual_type, &actual_format,
                               &nitems, &bytes_after, &prop) == Success
            && prop != NULL) {
            Window *wlist = (Window *)prop;
            for (unsigned long i = 0; i < nitems; i++) {
                if (window_pid_matches(d, wlist[i], our_pid) == 1) {
                    Window found = wlist[i];
                    XFree(prop);
                    return found;
                }
            }
            XFree(prop);
        }
    }

    /* fallback: scan root children for a large mapped IO window */
    Window root_ret, parent_ret;
    Window *children = NULL;
    unsigned int nchildren = 0;
    if (XQueryTree(d, root, &root_ret, &parent_ret, &children, &nchildren)
        && children) {
        Window best = 0;
        unsigned long best_area = 0;
        for (unsigned int i = 0; i < nchildren; i++) {
            XWindowAttributes a;
            if (!XGetWindowAttributes(d, children[i], &a)) continue;
            if (a.map_state != IsViewable) continue;
            if (a.class != InputOutput) continue;
            unsigned long area = (unsigned long)a.width * (unsigned long)a.height;
            if (area > best_area) { best_area = area; best = children[i]; }
        }
        XFree(children);
        if (best) return best;
    }

    return 0;
}

/* ---------- XImage -> 24-bit BGR (BMP-native) ---------- */

/*
 * Convert an XImage (TrueColor) into a packed, bottom-up BGR buffer with
 * rows padded to a 4-byte boundary (the BMP wire format). Returns a malloc'd
 * buffer of (row_stride * height) bytes on success, NULL on failure. Fills
 * *row_stride_out with the padded row size.
 *
 * Uses XGetPixel + channel masks for portability across byte/bit orders and
 * bits-per-pixel. Channel scaling is deterministic so repeated captures of
 * the same content produce bit-identical output.
 */
static uint8_t *ximage_to_bmp_pixels(XImage *img, uint32_t *row_stride_out) {
    if (!img) return NULL;
    int w = img->width;
    int h = img->height;
    uint32_t row_stride = (uint32_t)((w * 3 + 3) & ~3);
    *row_stride_out = row_stride;

    uint8_t *out = (uint8_t *)calloc((size_t)row_stride * (size_t)h, 1);
    if (!out) return NULL;

    unsigned long rmask = img->red_mask;
    unsigned long gmask = img->green_mask;
    unsigned long bmask = img->blue_mask;

    int rshift = 0, gshift = 0, bshift = 0;
    int rbits = 0, gbits = 0, bbits = 0;
    { unsigned long m;
      for (m = rmask; m && !(m & 1); m >>= 1) rshift++;
      for (; m & 1; m >>= 1) rbits++;
      for (m = gmask; m && !(m & 1); m >>= 1) gshift++;
      for (; m & 1; m >>= 1) gbits++;
      for (m = bmask; m && !(m & 1); m >>= 1) bshift++;
      for (; m & 1; m >>= 1) bbits++;
    }

    for (int y = 0; y < h; y++) {
        /* BMP is bottom-up: output row (h-1-y) for source row y */
        uint8_t *o = out + (size_t)(h - 1 - y) * row_stride;
        for (int x = 0; x < w; x++) {
            unsigned long px = XGetPixel(img, x, y);
            unsigned long r = (px & rmask) >> rshift;
            unsigned long g = (px & gmask) >> gshift;
            unsigned long b = (px & bmask) >> bshift;
            if (rbits < 8) r <<= (8 - rbits);
            else if (rbits > 8) r >>= (rbits - 8);
            if (gbits < 8) g <<= (8 - gbits);
            else if (gbits > 8) g >>= (gbits - 8);
            if (bbits < 8) b <<= (8 - bbits);
            else if (bbits > 8) b >>= (bbits - 8);
            /* BMP byte order: B, G, R */
            *o++ = (uint8_t)b;
            *o++ = (uint8_t)g;
            *o++ = (uint8_t)r;
        }
        /* padding bytes left as zero by calloc */
    }
    return out;
}

/* ---------- BMP writing ---------- */

#define BMP_FILEHDR_SIZE  14
#define BMP_INFOHDR_SIZE  40
#define BMP_HDR_TOTAL     (BMP_FILEHDR_SIZE + BMP_INFOHDR_SIZE)

/*
 * Write one complete 24-bit uncompressed BMP (headers + pixel data) to f.
 */
static void write_bmp(FILE *f, const uint8_t *pixels, uint32_t row_stride,
                      int32_t width, int32_t height) {
    uint32_t image_bytes = row_stride * (uint32_t)height;
    uint32_t file_size   = BMP_HDR_TOTAL + image_bytes;

    /* BITMAPFILEHEADER */
    fputc('B', f); fputc('M', f);
    w_u32(f, file_size);
    w_u16(f, 0);
    w_u16(f, 0);
    w_u32(f, BMP_HDR_TOTAL);            /* bfOffBits */

    /* BITMAPINFOHEADER */
    w_u32(f, BMP_INFOHDR_SIZE);
    w_s32(f, width);
    w_s32(f, height);                   /* positive = bottom-up */
    w_u16(f, 1);                        /* planes */
    w_u16(f, 24);                       /* bits per pixel */
    w_u32(f, 0);                        /* BI_RGB, no compression */
    w_u32(f, image_bytes);
    w_s32(f, 2835);                     /* 72 DPI */
    w_s32(f, 2835);
    w_u32(f, 0);
    w_u32(f, 0);

    fwrite(pixels, 1, image_bytes, f);
}

/* ---------- public entry point ---------- */

/*
 * Grab the current Petit-Ami window contents and append as a BMP frame.
 * Safe to call repeatedly; silently does nothing if setup failed.
 */
void screen_capture(void) {
    if (cap_disabled || !cap_file || !cap_display) return;

    if (cap_window == 0) {
        cap_window = find_pa_window(cap_display);
        if (cap_window == 0) {
            fprintf(stderr, "screen_capture: no Petit-Ami window found\n");
            return;
        }
    }

    /* Flush Petit-Ami's Xlib output buffer FIRST (its display is a separate
       connection from ours, so our XSync can't do it). Then XSync ours to
       make sure the grab doesn't race the server. */
    extern void pa_xflush(void);
    pa_xflush();
    XSync(cap_display, False);

    XWindowAttributes a;
    if (!XGetWindowAttributes(cap_display, cap_window, &a)) {
        fprintf(stderr, "screen_capture: XGetWindowAttributes failed\n");
        return;
    }

    XImage *img = XGetImage(cap_display, cap_window, 0, 0,
                            a.width, a.height, AllPlanes, ZPixmap);
    if (!img) {
        fprintf(stderr, "screen_capture: XGetImage failed (%dx%d)\n",
                a.width, a.height);
        return;
    }

    uint32_t row_stride = 0;
    uint8_t *pixels = ximage_to_bmp_pixels(img, &row_stride);
    if (!pixels) {
        XDestroyImage(img);
        fprintf(stderr, "screen_capture: out of memory\n");
        return;
    }

    write_bmp(cap_file, pixels, row_stride, a.width, a.height);
    cap_frame_count++;

    free(pixels);
    XDestroyImage(img);
}

/* ---------- module lifecycle ---------- */

__attribute__((constructor))
static void screen_capture_init(void) {
    cap_display = XOpenDisplay(NULL);
    if (!cap_display) {
        fprintf(stderr, "screen_capture: XOpenDisplay failed, disabled\n");
        cap_disabled = 1;
        return;
    }
    /* Ensure a fresh file: Petit-Ami's fopen("wb") does not pass O_TRUNC,
       so stale bytes from a prior (possibly larger) run would remain past
       the new content and break BMP-stream walkers. */
    remove(CAPTURE_FILENAME);
    cap_file = fopen(CAPTURE_FILENAME, "wb");
    if (!cap_file) {
        perror("screen_capture: fopen " CAPTURE_FILENAME);
        XCloseDisplay(cap_display);
        cap_display = NULL;
        cap_disabled = 1;
        return;
    }
}

__attribute__((destructor))
static void screen_capture_fini(void) {
    if (cap_file) {
        fflush(cap_file);
        fclose(cap_file);
        cap_file = NULL;
        if (cap_frame_count == 0) {
            remove(CAPTURE_FILENAME);
        } else {
            fprintf(stderr, "screen_capture: wrote %u frames to %s\n",
                    cap_frame_count, CAPTURE_FILENAME);
        }
    }
    if (cap_display) {
        XCloseDisplay(cap_display);
        cap_display = NULL;
    }
}
