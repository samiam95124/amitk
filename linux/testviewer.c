/*******************************************************************************
*                                                                              *
*                     STANDALONE X11 TEST OUTPUT VIEWER                        *
*                                                                              *
* Walks through the output of a test and shows it one frame at a time in an   *
* X11 window. Uses only X11 + libpng - no Ami dependency.                      *
*                                                                              *
* Two kinds of file are understood, told apart by their first bytes:           *
*                                                                              *
*   A stream of PNGs, as the screen_capture module writes to tests/<test>.img *
*   and as the picture standards tests/<platform>_compare/<test>.cmp hold:     *
*   each PNG is one frame, and is shown as it is.                              *
*                                                                              *
*   A text listing, as the terminal tests write to tests/<test>.lst and as     *
*   their standards tests/<test>.cmp hold: pages separated by form feeds,      *
*   each page one frame, drawn in a fixed font, black on white, the way the    *
*   terminal showed it. The text is UTF-8: the windowed tests draw their       *
*   frames in box-drawing characters, and the Unicode fixed font has them.     *
*                                                                              *
* Keys:                                                                        *
*   right arrow, space, page down - next frame, a step of an animation too    *
*   left arrow, backspace, page up - previous frame                            *
*   shift right / shift left       - next / previous whole frame: a page       *
*                                    labeled "frame N" with no step fraction,  *
*                                    on a text page or in a picture's title    *
*   home / end                     - first / last frame                        *
*   q / close button               - quit                                      *
*                                                                              *
* Usage:                                                                       *
*   bin/testviewer [filename]       # default: test_images.img                 *
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

#define DEFAULT_FILENAME "test_images.img"
#define TEXT_MARGIN      8     /* pixels around a text page */
#define TEXT_FONT        "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso10646-1"
#define TEXT_FONT_ALT    "fixed"

/* PNG signature: 8 bytes */
static const uint8_t png_sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

typedef struct {
    long offset;    /* byte offset within file */
    long size;      /* total PNG size in bytes */
    char label[80]; /* the picture's title, "frame N" or "frame N.S", or empty */
} frame_idx_t;

static frame_idx_t *frames = NULL;
static int          nframes = 0;

/* ---------- PNG stream indexing ---------- */

/*
 * PNG files consist of the 8-byte signature followed by chunks.
 * Each chunk is: 4-byte length (big-endian) + 4-byte type + data + 4-byte CRC.
 * The last chunk is IEND. We walk chunks to find the end of each PNG.
 */

/* A label reads "frame N" for a whole frame and "frame N.S" for a step of
   an animation within it, wherever it stands in the text: the terminal tests'
   at the top right, the window tests' in the frame title, the pictures' in
   their PNG title. 1 for a whole frame, 0 for a step or no label. */
static int label_whole(const char *s) {
    const char *lb = strstr(s, "frame ");
    while (lb) {
        const char *d = lb + 6;
        if (*d >= '0' && *d <= '9') {
            while (*d >= '0' && *d <= '9') d++;
            return !(*d == '.' && d[1] >= '0' && d[1] <= '9');
        }
        lb = strstr(lb + 6, "frame ");
    }
    return 0;
}

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

        /* walk chunks until IEND, taking the title on the way */
        char label[80];
        label[0] = 0;
        for (;;) {
            uint8_t chunk_hdr[8];
            if (fread(chunk_hdr, 1, 8, f) != 8) goto done;
            uint32_t chunk_len = read_u32_be(chunk_hdr);
            int is_iend = (memcmp(chunk_hdr + 4, "IEND", 4) == 0);

            if (memcmp(chunk_hdr + 4, "tEXt", 4) == 0 && chunk_len < 200) {
                /* keyword, a zero, the text: the title is the label */
                char txt[200];
                if (fread(txt, 1, chunk_len, f) != chunk_len) goto done;
                txt[chunk_len] = 0;
                if (!strcmp(txt, "Title") && strlen(txt) + 1 < chunk_len) {
                    strncpy(label, txt + 6, sizeof(label) - 1);
                    label[sizeof(label) - 1] = 0;
                }
                fseek(f, 4, SEEK_CUR); /* the CRC */
            } else
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
        strcpy(frames[nframes].label, label);
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

/* ---------- text pages ---------- */

/*
 * A text listing: the pages of a terminal run, each ended by a form feed.
 * The whole file is read and cut at the form feeds; a trailing empty page
 * after the last form feed is not a page. Carriage returns are dropped.
 */

static char **pages  = NULL;   /* each page, NUL terminated, lines by '\n' */
static int   *whole  = NULL;   /* the page is a whole frame: its label carries
                                  no step fraction. A page with no label is
                                  a step: every frame is stamped, and a stamp
                                  not seen landed on a screen not displayed,
                                  as the screen switching tests' do */
static int    npages = 0;
static int    pagecols = 80;   /* the widest line over all pages */
static int    pagerows = 24;   /* the tallest page */

static int is_png_file(const char *filename) {
    uint8_t sig[8];
    FILE *f = fopen(filename, "rb");
    int r;
    if (!f) return 0;
    r = fread(sig, 1, 8, f) == 8 && memcmp(sig, png_sig, 8) == 0;
    fclose(f);
    return r;
}

/* one code point of UTF-8, the pointer advanced; a bad byte is itself */
static unsigned utf8_next(const char **pp) {
    const unsigned char *p = (const unsigned char *)*pp;
    unsigned c = p[0];
    int n = 0;

    if (c >= 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 &&
        (p[3] & 0xc0) == 0x80) { c = ((c & 7) << 18) | ((p[1] & 0x3f) << 12) |
        ((p[2] & 0x3f) << 6) | (p[3] & 0x3f); n = 4; }
    else if (c >= 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80)
        { c = ((c & 15) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f); n = 3; }
    else if (c >= 0xc0 && (p[1] & 0xc0) == 0x80)
        { c = ((c & 31) << 6) | (p[1] & 0x3f); n = 2; }
    else n = 1;
    *pp += n;
    return c;
}

/* the columns a line takes: its code points, not its bytes */
static int utf8_cols(const char *s, int len) {
    const char *p = s, *e = s + len;
    int n = 0;
    while (p < e) { utf8_next(&p); n++; }
    return n;
}

static int load_text(const char *filename) {
    FILE *f = fopen(filename, "rb");
    long size;
    char *buf, *p, *q;
    int cap = 16;

    if (!f) {
        fprintf(stderr, "testviewer: cannot open %s\n", filename);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)malloc(size + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);

    /* carriage returns out, so a DOS listing pages the same */
    for (p = q = buf; *p; p++) if (*p != '\r') *q++ = *p;
    *q = 0;

    pages = (char **)malloc(sizeof(char *) * cap);
    p = buf;
    while (*p) {
        char *ff = strchr(p, '\f');
        long len = ff ? ff - p : (long)strlen(p);
        char *page = (char *)malloc(len + 1);
        memcpy(page, p, len);
        page[len] = 0;
        if (npages >= cap) {
            cap *= 2;
            pages = (char **)realloc(pages, sizeof(char *) * cap);
        }
        pages[npages++] = page;
        if (!ff) break;
        p = ff + 1;
    }
    free(buf);

    /* the whole frames are what the shifted arrows step between */
    whole = (int *)malloc(sizeof(int) * npages);
    for (int i = 0; i < npages; i++) {
        whole[i] = label_whole(pages[i]);
    }

    /* the page size: the widest line and the tallest page over them all,
       and never smaller than a terminal's 80 by 24, so the window holds
       every page without resizing between them */
    for (int i = 0; i < npages; i++) {
        int rows = 0, cols = 0, col = 0;
        for (p = pages[i]; ; ) {
            if (*p == '\n' || *p == 0) {
                if (col > cols) cols = col;
                col = 0;
                if (*p == 0) break;
                rows++;
                p++;
            } else { utf8_next((const char **)&p); col++; }
        }
        /* a page ending without a newline still has that last line */
        if (pages[i][0] && pages[i][strlen(pages[i]) - 1] != '\n') rows++;
        if (rows > pagerows) pagerows = rows;
        if (cols > pagecols) pagecols = cols;
    }
    return 0;
}

/* draw page i into the window, line by line in the fixed font */
static void draw_text_page(Display *dpy, Window win, GC gc, XFontStruct *font,
                           int i) {
    int ch = font->ascent + font->descent;
    int y = TEXT_MARGIN + font->ascent;
    char *p, *nl;

    XChar2b *cb = NULL;
    int      cbn = 0;

    XClearWindow(dpy, win);
    if (i < 0 || i >= npages) return;
    p = pages[i];
    while (*p) {
        nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > 0) {
            /* the line decoded to 16-bit characters for the Unicode font;
               a code point past the plane the font holds shows as ? */
            const char *q = p, *e = p + len;
            int n = 0;
            if (len > cbn) { cb = realloc(cb, sizeof(XChar2b) * len); cbn = len; }
            while (q < e) {
                unsigned c = utf8_next(&q);
                if (c > 0xffff) c = '?';
                cb[n].byte1 = (c >> 8) & 0xff;
                cb[n].byte2 = c & 0xff;
                n++;
            }
            XDrawString16(dpy, win, gc, TEXT_MARGIN, y, cb, n);
        }
        y += ch;
        if (!nl) break;
        p = nl + 1;
    }
    free(cb);
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

/* the frame a key asks for, from the current one; -1 to quit. Shifted,
   the arrows step between whole frames, over the steps of an animation;
   a picture stream without labels has no steps, and they step a frame there */
static int key_frame(KeySym key, int shift, int cur, int n) {
    if (key == XK_Right || key == XK_space || key == XK_Page_Down ||
        key == XK_KP_Right) {
        int i = cur + 1;
        if (shift && whole) while (i < n && !whole[i]) i++;
        return i < n ? i : cur;
    }
    if (key == XK_Left || key == XK_BackSpace || key == XK_Page_Up ||
        key == XK_KP_Left) {
        int i = cur - 1;
        if (shift && whole) while (i >= 0 && !whole[i]) i--;
        return i >= 0 ? i : cur;
    }
    if (key == XK_Home) return 0;
    if (key == XK_End) return n - 1;
    if (key == XK_q || key == XK_Q || key == XK_Escape) return -1;
    return cur;
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    const char *fn = (argc > 1) ? argv[1] : DEFAULT_FILENAME;
    int textmode;

    textmode = !is_png_file(fn);
    if (textmode) {
        if (load_text(fn) != 0) return 1;
        if (npages == 0) {
            fprintf(stderr, "testviewer: %s is empty\n", fn);
            return 1;
        }
        int nwhole = 0;
        for (int i = 0; i < npages; i++) nwhole += whole[i];
        fprintf(stderr, "testviewer: %d text page(s) of %d by %d from %s, "
                "%d whole frame(s)\n", npages, pagecols, pagerows, fn, nwhole);
    } else {
        if (build_index(fn) != 0) return 1;
        if (nframes == 0) {
            fprintf(stderr, "testviewer: %s contains no PNG frames\n", fn);
            return 1;
        }
        /* the pictures' titles say which are whole frames; a stream with no
           titles, from a test that gives none, steps a frame at a time */
        int nwhole = 0;
        for (int i = 0; i < nframes; i++) nwhole += label_whole(frames[i].label);
        if (nwhole) {
            whole = (int *)malloc(sizeof(int) * nframes);
            for (int i = 0; i < nframes; i++)
                whole[i] = label_whole(frames[i].label);
        }
        fprintf(stderr, "testviewer: indexed %d frame(s) from %s, %d whole\n",
                nframes, fn, nwhole);
    }
    int total = textmode ? npages : nframes;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open X display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);

    int fw = 0, fh = 0;
    uint8_t *rgb = NULL;
    XFontStruct *font = NULL;
    if (textmode) {
        font = XLoadQueryFont(dpy, TEXT_FONT);
        if (!font) font = XLoadQueryFont(dpy, TEXT_FONT_ALT);
        if (!font) { fprintf(stderr, "testviewer: no fixed font\n"); return 1; }
        fw = pagecols * font->max_bounds.width + 2 * TEXT_MARGIN;
        fh = pagerows * (font->ascent + font->descent) + 2 * TEXT_MARGIN;
    } else {
        /* decode first frame to get dimensions */
        rgb = load_frame(fn, 0, &fw, &fh);
        if (!rgb) { fprintf(stderr, "Failed to decode frame 0\n"); return 1; }
    }

    /* create window */
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                     0, 0, fw, fh, 0,
                                     BlackPixel(dpy, screen),
                                     WhitePixel(dpy, screen));

    char title[128];
    snprintf(title, sizeof(title), "testviewer [1/%d]", total);
    XStoreName(dpy, win, title);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);

    /* WM_DELETE_WINDOW protocol for clean close */
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    XMapWindow(dpy, win);

    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, BlackPixel(dpy, screen));
    XSetBackground(dpy, gc, WhitePixel(dpy, screen));
    if (font) XSetFont(dpy, gc, font->fid);

    /* build initial XImage, for the picture stream */
    char *xdata = NULL;
    XImage *ximg = NULL;
    if (!textmode) {
        xdata = rgb_to_ximage_data(rgb, fw, fh, visual);
        free(rgb);
        ximg = XCreateImage(dpy, visual, depth, ZPixmap, 0,
                            xdata, fw, fh, 32, 0);
    }

    int cur_frame = 0;

    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);

        switch (ev.type) {

        case Expose:
            if (ev.xexpose.count == 0) {
                if (textmode) draw_text_page(dpy, win, gc, font, cur_frame);
                else XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, fw, fh);
            }
            break;

        case KeyPress: {
            KeySym key = XLookupKeysym(&ev.xkey, 0);
            int new_frame = key_frame(key, (ev.xkey.state & ShiftMask) != 0,
                                      cur_frame, total);
            if (new_frame < 0) goto done;
            if (new_frame != cur_frame) {
                cur_frame = new_frame;
                snprintf(title, sizeof(title), "testviewer [%d/%d]",
                         cur_frame + 1, total);
                XStoreName(dpy, win, title);
                if (textmode) draw_text_page(dpy, win, gc, font, cur_frame);
                else {
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
                        XClearWindow(dpy, win);
                        XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, fw, fh);
                    }
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
    if (ximg) XDestroyImage(ximg); /* frees xdata */
    if (font) XFreeFont(dpy, font);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    free(frames);
    for (int i = 0; i < npages; i++) free(pages[i]);
    free(pages);
    free(whole);
    return 0;
}
