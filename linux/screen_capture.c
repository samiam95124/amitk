/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* Linux/X11 version. Grabs the pixel contents of the running Petit-Ami X      *
* window and appends it as a PNG to a file called "test_images". Multiple      *
* PNGs are concatenated back-to-back in the same file; each is self-           *
* delimiting (PNG signature at the start, IEND chunk at the end) so readers   *
* can walk them sequentially.                                                  *
*                                                                              *
* The file is opened by a module constructor and closed by a destructor.       *
* The caller only needs to call screen_capture() at each interesting moment    *
* (e.g. just before waitnext() in a test program).                             *
*                                                                              *
* Output format matches macosx/screen_capture.c (concatenated PNGs) so        *
* test_images files are portable across platforms for cross-platform           *
* regression comparison.                                                       *
*                                                                              *
* Window discovery:                                                            *
*   - Opens its own X connection via XOpenDisplay(NULL).                       *
*   - Walks EWMH _NET_CLIENT_LIST, matches window by _NET_WM_PID == getpid(). *
*   - Falls back to the largest mapped top-level window.                       *
*                                                                              *
* Viewer:                                                                      *
*   bin/testviewer    # walks through the concatenated PNGs with arrow keys    *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <png.h>
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

/* ---------- XImage -> top-down RGB ---------- */

/*
 * Convert an XImage (TrueColor) into packed top-down RGB rows (3 bytes per
 * pixel, no padding). Returns a malloc'd buffer of width*height*3 bytes on
 * success, NULL on failure.
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

/* ---------- PNG writing ---------- */

static void png_write_to_file(png_structp png, png_bytep data, png_size_t len) {
    FILE *f = (FILE *)png_get_io_ptr(png);
    fwrite(data, 1, len, f);
}

static void png_flush_file(png_structp png) {
    FILE *f = (FILE *)png_get_io_ptr(png);
    fflush(f);
}

static void write_png_frame(FILE *f, uint8_t *rgb_rows, int width, int height) {
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    if (!png) return;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); return; }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return;
    }
    png_set_write_fn(png, f, png_write_to_file, png_flush_file);
    png_set_IHDR(png, info, width, height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    for (int y = 0; y < height; y++)
        png_write_row(png, rgb_rows + y * width * 3);

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fflush(f);
}

/* ---------- public entry point ---------- */

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

    uint8_t *rgb = ximage_to_rgb(img);
    if (!rgb) {
        XDestroyImage(img);
        fprintf(stderr, "screen_capture: out of memory\n");
        return;
    }

    write_png_frame(cap_file, rgb, a.width, a.height);
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
