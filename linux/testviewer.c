/*******************************************************************************
*                                                                              *
*                     STANDALONE X11 PNG STREAM VIEWER                         *
*                                                                              *
* Walks through a file of concatenated PNGs (as produced by the screen_capture *
* module) and displays them one at a time in an X11 window.                    *
* Uses only X11 + libpng - no Ami dependency.                                  *
*                                                                              *
* Keys:                                                                        *
*   right arrow  - next frame                                                  *
*   left arrow   - previous frame                                              *
*   q / close button - quit                                                    *
*                                                                              *
* Usage:                                                                       *
*   bin/testviewer [filename]       # default: test_images                     *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <png.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define DEFAULT_FILENAME "test_images"

/* PNG signature: 8 bytes */
static const uint8_t png_sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

typedef struct {
    long offset;    /* byte offset within file */
    long size;      /* total PNG size in bytes */
} frame_idx_t;

static frame_idx_t *frames = NULL;
static int          nframes = 0;

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

/* ---------- PNG decoding ---------- */

typedef struct {
    const uint8_t *data;
    size_t         size;
    size_t         pos;
} mem_reader_t;

static void png_read_from_mem(png_structp png, png_bytep out, png_size_t len) {
    mem_reader_t *r = (mem_reader_t *)png_get_io_ptr(png);
    if (r->pos + len > r->size) {
        png_error(png, "read past end of PNG data");
        return;
    }
    memcpy(out, r->data + r->pos, len);
    r->pos += len;
}

/*
 * Decode a PNG from a memory buffer into an RGB pixel array.
 * Returns malloc'd buffer (width*height*3), sets *w and *h.
 * Returns NULL on failure.
 */
static uint8_t *decode_png(const uint8_t *data, size_t data_size,
                           int *w, int *h) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             NULL, NULL, NULL);
    if (!png) return NULL;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); return NULL; }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    mem_reader_t reader = { data, data_size, 0 };
    png_set_read_fn(png, &reader, png_read_from_mem);
    png_read_info(png, info);

    *w = png_get_image_width(png, info);
    *h = png_get_image_height(png, info);
    int color_type = png_get_color_type(png, info);
    int bit_depth = png_get_bit_depth(png, info);

    /* normalize to 8-bit RGB */
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGBA ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_strip_alpha(png);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);

    size_t rowbytes = png_get_rowbytes(png, info);
    uint8_t *pixels = (uint8_t *)malloc(rowbytes * (*h));
    if (!pixels) {
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    png_bytep *row_ptrs = (png_bytep *)malloc(sizeof(png_bytep) * (*h));
    for (int y = 0; y < *h; y++)
        row_ptrs[y] = pixels + y * rowbytes;
    png_read_image(png, row_ptrs);
    free(row_ptrs);

    png_destroy_read_struct(&png, &info, NULL);
    return pixels;
}

/* ---------- frame loading ---------- */

/*
 * Load frame i from the data file, decode it, return RGB pixels.
 * Sets *w and *h. Returns NULL on failure.
 */
static uint8_t *load_frame(const char *filename, int i, int *w, int *h) {
    if (i < 0 || i >= nframes) return NULL;

    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, frames[i].offset, SEEK_SET);

    uint8_t *buf = (uint8_t *)malloc(frames[i].size);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, frames[i].size, f);
    fclose(f);

    uint8_t *rgb = decode_png(buf, frames[i].size, w, h);
    free(buf);
    return rgb;
}

/* ---------- X11 display ---------- */

/*
 * Convert RGB pixels to the format X11 wants (typically 32-bit with masks
 * from the visual). Returns a malloc'd buffer for XCreateImage.
 */
static char *rgb_to_ximage_data(const uint8_t *rgb, int w, int h,
                                Visual *visual) {
    char *buf = (char *)malloc((size_t)w * h * 4);
    if (!buf) return NULL;

    unsigned long rmask = visual->red_mask;
    unsigned long gmask = visual->green_mask;
    unsigned long bmask = visual->blue_mask;

    int rshift = 0, gshift = 0, bshift = 0;
    { unsigned long m;
      for (m = rmask; m && !(m & 1); m >>= 1) rshift++;
      for (m = gmask; m && !(m & 1); m >>= 1) gshift++;
      for (m = bmask; m && !(m & 1); m >>= 1) bshift++;
    }

    uint32_t *out = (uint32_t *)buf;
    const uint8_t *in = rgb;
    for (int i = 0; i < w * h; i++) {
        uint8_t r = *in++;
        uint8_t g = *in++;
        uint8_t b = *in++;
        *out++ = ((uint32_t)r << rshift)
               | ((uint32_t)g << gshift)
               | ((uint32_t)b << bshift);
    }
    return buf;
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    const char *fn = (argc > 1) ? argv[1] : DEFAULT_FILENAME;

    if (build_index(fn) != 0) return 1;
    if (nframes == 0) {
        fprintf(stderr, "testviewer: %s contains no PNG frames\n", fn);
        return 1;
    }
    fprintf(stderr, "testviewer: indexed %d frame(s) from %s\n", nframes, fn);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open X display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);

    /* decode first frame to get dimensions */
    int fw, fh;
    uint8_t *rgb = load_frame(fn, 0, &fw, &fh);
    if (!rgb) { fprintf(stderr, "Failed to decode frame 0\n"); return 1; }

    /* create window */
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                     0, 0, fw, fh, 0,
                                     BlackPixel(dpy, screen),
                                     WhitePixel(dpy, screen));

    char title[128];
    snprintf(title, sizeof(title), "testviewer [1/%d]", nframes);
    XStoreName(dpy, win, title);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);

    /* WM_DELETE_WINDOW protocol for clean close */
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    XMapWindow(dpy, win);

    /* build initial XImage */
    char *xdata = rgb_to_ximage_data(rgb, fw, fh, visual);
    free(rgb);
    XImage *ximg = XCreateImage(dpy, visual, depth, ZPixmap, 0,
                                xdata, fw, fh, 32, 0);

    int cur_frame = 0;
    GC gc = DefaultGC(dpy, screen);

    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);

        switch (ev.type) {

        case Expose:
            if (ev.xexpose.count == 0)
                XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, fw, fh);
            break;

        case KeyPress: {
            KeySym key = XLookupKeysym(&ev.xkey, 0);
            int new_frame = cur_frame;
            if (key == XK_Right && cur_frame + 1 < nframes)
                new_frame = cur_frame + 1;
            else if (key == XK_Left && cur_frame > 0)
                new_frame = cur_frame - 1;
            else if (key == XK_q || key == XK_Q)
                goto done;

            if (new_frame != cur_frame) {
                cur_frame = new_frame;
                int nw, nh;
                rgb = load_frame(fn, cur_frame, &nw, &nh);
                if (rgb) {
                    XDestroyImage(ximg); /* frees xdata too */
                    fw = nw; fh = nh;
                    xdata = rgb_to_ximage_data(rgb, fw, fh, visual);
                    free(rgb);
                    ximg = XCreateImage(dpy, visual, depth, ZPixmap, 0,
                                        xdata, fw, fh, 32, 0);
                    XResizeWindow(dpy, win, fw, fh);
                    snprintf(title, sizeof(title), "testviewer [%d/%d]",
                             cur_frame + 1, nframes);
                    XStoreName(dpy, win, title);
                    XClearWindow(dpy, win);
                    XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, fw, fh);
                }
            }
            break;
        }

        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete)
                goto done;
            break;

        default:
            break;
        }
    }

done:
    XDestroyImage(ximg); /* frees xdata */
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    free(frames);
    return 0;
}
