/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* Test foundation for Petit-Ami graphics. Exposes screen_capture(), which      *
* grabs the pixel contents of the running Petit-Ami X window and appends it    *
* as a frame to a multi-page, uncompressed TIFF file called "test_images".     *
*                                                                              *
* The file is opened by a module constructor and closed by a destructor, so    *
* the caller only needs to call screen_capture() at each interesting moment    *
* (e.g. just before waitnext() in a test program).                             *
*                                                                              *
* Bit-for-bit comparability:                                                   *
*   - Uncompressed RGB strips, no palette, no compression, no randomness.      *
*   - For a deterministic test run, `cmp` on two test_images files is a        *
*     pass/fail signal. `tiffcmp -l` gives per-frame diagnostics.              *
*                                                                              *
* Window discovery:                                                            *
*   - Opens its own X connection via XOpenDisplay(NULL).                       *
*   - Walks EWMH _NET_CLIENT_LIST, matches window by _NET_WM_PID == getpid().  *
*   - Falls back to WM_CLIENT_MACHINE match + first mapped managed window.     *
*                                                                              *
* Viewers:                                                                     *
*   gimp test_images         # each frame as a layer, arrow keys step          *
*   tiffinfo test_images     # frame count and per-frame metadata              *
*   tiffsplit test_images f_ # split into per-frame TIFFs                      *
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

/*
 * TIFF tag numbers we emit (classic TIFF 6.0, little-endian).
 */
#define TIFF_TAG_IMAGEWIDTH            256
#define TIFF_TAG_IMAGELENGTH           257
#define TIFF_TAG_BITSPERSAMPLE         258
#define TIFF_TAG_COMPRESSION           259
#define TIFF_TAG_PHOTOMETRIC           262
#define TIFF_TAG_STRIPOFFSETS          273
#define TIFF_TAG_SAMPLESPERPIXEL       277
#define TIFF_TAG_ROWSPERSTRIP          278
#define TIFF_TAG_STRIPBYTECOUNTS       279
#define TIFF_TAG_XRESOLUTION           282
#define TIFF_TAG_YRESOLUTION           283
#define TIFF_TAG_RESOLUTIONUNIT        296

#define TIFF_TYPE_SHORT     3
#define TIFF_TYPE_LONG      4
#define TIFF_TYPE_RATIONAL  5

/* number of tags we write per IFD */
#define TIFF_NUM_TAGS       12

/* ---------- module state ---------- */

static FILE     *cap_file     = NULL;
static Display  *cap_display  = NULL;
static Window    cap_window   = 0;      /* discovered lazily on first capture */
static uint32_t  cap_next_ifd_patch = 0; /* file offset of prev "next IFD" slot */
static uint32_t  cap_frame_count     = 0;
static int       cap_disabled        = 0; /* set if setup failed */

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

/* write one 12-byte IFD entry */
static void w_tag(FILE *f, uint16_t tag, uint16_t type, uint32_t count,
                  uint32_t value) {
    w_u16(f, tag);
    w_u16(f, type);
    w_u32(f, count);
    w_u32(f, value);
}

/* patch a 4-byte little-endian value at a given file offset */
static void patch_u32(FILE *f, long offset, uint32_t v) {
    long cur = ftell(f);
    fseek(f, offset, SEEK_SET);
    w_u32(f, v);
    fseek(f, cur, SEEK_SET);
}

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
 *      mapped InputOutput window (test case: only one pa window exists).
 * Returns 0 if nothing found.
 */
static Window find_pa_window(Display *d) {
    Window root = DefaultRootWindow(d);
    pid_t  our_pid = getpid();

    /* --- try _NET_CLIENT_LIST --- */
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

    /* --- fallback: scan root children for a large mapped IO window --- */
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

/* ---------- TIFF writing ---------- */

/*
 * Write the TIFF file header (8 bytes). The first IFD offset is left as a
 * placeholder and back-patched when the first frame is written.
 * Returns the file offset of the "first IFD" slot for later patching.
 */
static long write_tiff_header(FILE *f) {
    fputc('I', f); fputc('I', f);   /* little-endian */
    w_u16(f, 42);                   /* magic */
    long patch_off = ftell(f);
    w_u32(f, 0);                    /* first IFD offset - back-patched */
    return patch_off;
}

/*
 * Append one frame as an IFD+strip to the currently-open TIFF file.
 * img must be a 24-bit packed RGB buffer of exactly width*height*3 bytes.
 * Updates cap_next_ifd_patch to point at this IFD's "next IFD" slot.
 */
static void write_tiff_frame(FILE *f, const uint8_t *img,
                             uint32_t width, uint32_t height) {
    uint32_t strip_bytes = width * height * 3u;

    /* 1. pixel strip */
    uint32_t strip_off = (uint32_t)ftell(f);
    fwrite(img, 1, strip_bytes, f);

    /* 2. BitsPerSample array (3 shorts = 6 bytes, too big for inline value) */
    uint32_t bps_off = (uint32_t)ftell(f);
    w_u16(f, 8); w_u16(f, 8); w_u16(f, 8);

    /* 3. two RATIONAL values for X/Y resolution (72/1) */
    uint32_t xres_off = (uint32_t)ftell(f);
    w_u32(f, 72); w_u32(f, 1);
    uint32_t yres_off = (uint32_t)ftell(f);
    w_u32(f, 72); w_u32(f, 1);

    /* 4. IFD: 2-byte count, 12 bytes per tag, 4-byte next-IFD pointer.
     *    IFD must start on a word boundary. */
    long cur = ftell(f);
    if (cur & 1) { fputc(0, f); cur++; }
    uint32_t ifd_off = (uint32_t)cur;

    /* patch previous frame's (or header's) "next IFD" slot to point here */
    patch_u32(f, (long)cap_next_ifd_patch, ifd_off);

    w_u16(f, TIFF_NUM_TAGS);
    /* tags MUST be written in ascending tag-number order */
    w_tag(f, TIFF_TAG_IMAGEWIDTH,       TIFF_TYPE_LONG,     1, width);
    w_tag(f, TIFF_TAG_IMAGELENGTH,      TIFF_TYPE_LONG,     1, height);
    w_tag(f, TIFF_TAG_BITSPERSAMPLE,    TIFF_TYPE_SHORT,    3, bps_off);
    w_tag(f, TIFF_TAG_COMPRESSION,      TIFF_TYPE_SHORT,    1, 1);      /* none */
    w_tag(f, TIFF_TAG_PHOTOMETRIC,      TIFF_TYPE_SHORT,    1, 2);      /* RGB */
    w_tag(f, TIFF_TAG_STRIPOFFSETS,     TIFF_TYPE_LONG,     1, strip_off);
    w_tag(f, TIFF_TAG_SAMPLESPERPIXEL,  TIFF_TYPE_SHORT,    1, 3);
    w_tag(f, TIFF_TAG_ROWSPERSTRIP,     TIFF_TYPE_LONG,     1, height);
    w_tag(f, TIFF_TAG_STRIPBYTECOUNTS,  TIFF_TYPE_LONG,     1, strip_bytes);
    w_tag(f, TIFF_TAG_XRESOLUTION,      TIFF_TYPE_RATIONAL, 1, xres_off);
    w_tag(f, TIFF_TAG_YRESOLUTION,      TIFF_TYPE_RATIONAL, 1, yres_off);
    w_tag(f, TIFF_TAG_RESOLUTIONUNIT,   TIFF_TYPE_SHORT,    1, 2);      /* inch */

    /* remember where the next-IFD slot lives, write 0 for now */
    cap_next_ifd_patch = (uint32_t)ftell(f);
    w_u32(f, 0);
}

/* ---------- XImage -> packed RGB conversion ---------- */

/*
 * Convert an XImage (TrueColor, 24/32-bit) into packed 24-bit RGB.
 * Returns a malloc'd buffer of width*height*3 bytes, or NULL on failure.
 *
 * We use XGetPixel+masks to extract channels portably — this handles any
 * byte order, bit order, or bits-per-pixel the X server gives us.
 */
static uint8_t *ximage_to_rgb(XImage *img) {
    if (!img) return NULL;
    int w = img->width;
    int h = img->height;
    uint8_t *out = (uint8_t *)malloc((size_t)w * (size_t)h * 3u);
    if (!out) return NULL;

    unsigned long rmask = img->red_mask;
    unsigned long gmask = img->green_mask;
    unsigned long bmask = img->blue_mask;

    /* compute right-shift + scale factor for each channel */
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

    uint8_t *o = out;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long px = XGetPixel(img, x, y);
            unsigned long r = (px & rmask) >> rshift;
            unsigned long g = (px & gmask) >> gshift;
            unsigned long b = (px & bmask) >> bshift;
            /* scale each channel up to 8 bits (deterministic) */
            if (rbits < 8) r <<= (8 - rbits);
            else if (rbits > 8) r >>= (rbits - 8);
            if (gbits < 8) g <<= (8 - gbits);
            else if (gbits > 8) g >>= (gbits - 8);
            if (bbits < 8) b <<= (8 - bbits);
            else if (bbits > 8) b >>= (bbits - 8);
            *o++ = (uint8_t)r;
            *o++ = (uint8_t)g;
            *o++ = (uint8_t)b;
        }
    }
    return out;
}

/* ---------- public entry point ---------- */

/*
 * Grab the current Petit-Ami window contents and append as a TIFF frame.
 * Safe to call repeatedly; silently does nothing if setup has failed.
 */
void screen_capture(void) {
    if (cap_disabled || !cap_file || !cap_display) return;

    /* lazy window discovery: pa window may not exist at constructor time */
    if (cap_window == 0) {
        cap_window = find_pa_window(cap_display);
        if (cap_window == 0) {
            fprintf(stderr, "screen_capture: no Petit-Ami window found\n");
            return;
        }
    }

    /* make sure pa's drawing is flushed to the server before we read */
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

    uint8_t *rgb = ximage_to_rgb(img);
    if (!rgb) {
        XDestroyImage(img);
        fprintf(stderr, "screen_capture: out of memory\n");
        return;
    }

    write_tiff_frame(cap_file, rgb, (uint32_t)a.width, (uint32_t)a.height);
    cap_frame_count++;

    free(rgb);
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
    cap_file = fopen(CAPTURE_FILENAME, "wb");
    if (!cap_file) {
        perror("screen_capture: fopen " CAPTURE_FILENAME);
        XCloseDisplay(cap_display);
        cap_display = NULL;
        cap_disabled = 1;
        return;
    }
    cap_next_ifd_patch = (uint32_t)write_tiff_header(cap_file);
}

__attribute__((destructor))
static void screen_capture_fini(void) {
    if (cap_file) {
        /* terminate the IFD chain: the current "next IFD" slot should be 0.
         * write_tiff_frame already leaves it as 0, so no action needed unless
         * zero frames were written. */
        fflush(cap_file);
        fclose(cap_file);
        cap_file = NULL;
        if (cap_frame_count == 0) {
            /* no frames written: delete the stub file so cmp isn't confused */
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
