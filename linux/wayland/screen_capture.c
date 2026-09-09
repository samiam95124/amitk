/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* Wayland version. Appends the pixel contents of the running Petit-Ami        *
* window to a file called "test_images" as a PNG. Multiple PNGs are           *
* concatenated back to back in the same file; each is self delimiting (PNG    *
* signature at the start, IEND chunk at the end) so readers can walk them     *
* sequentially.                                                                *
*                                                                              *
* The file is opened at the first capture and closed by a destructor. The      *
* caller only needs to call screen_capture() at each interesting moment (e.g.  *
* just before waitnext() in a test program); screen_capture_name() ahead of    *
* the first one puts the pictures somewhere other than test_images, which is   *
* how the regression gives each test a file of its own and keeps two runs at   *
* once from writing over each other.                                           *
*                                                                              *
* Output format matches linux/screen_capture.c and macosx/screen_capture.c    *
* (concatenated PNGs) so test_images files are portable across platforms for  *
* cross-platform regression comparison. The client area is captured, as it    *
* is there: the frame around it is drawn focused or unfocused at the           *
* desktop's discretion, and a standard carrying that would fail whenever the   *
* user was looking elsewhere. What is captured is the composed picture, not    *
* the window's canvas -- the widgets, menus and child windows standing on a    *
* window are windows in their own right, and only the composition has them.    *
*                                                                              *
* There is nothing to discover and no compositor to ask. A Wayland client      *
* draws its window into a canvas of its own, and that canvas is the picture:   *
* the backend hands over the window it presents and the pixels are read        *
* straight out of it. A compositor screen grab would need a portal and the     *
* user's permission; this needs neither, and cannot catch anything on screen   *
* that is not ours.                                                            *
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
#include <setjmp.h>
#include <png.h>

#include "graphics_i.h"

#define CAPTURE_FILENAME "test_images"

/* ---------- module state ---------- */

static FILE     *cap_file        = NULL;
static char      cap_name[1024] = CAPTURE_FILENAME;
static int       cap_opened     = 0;   /* the file has been made */
static uint32_t  cap_frame_count = 0;
static int       cap_disabled    = 0;
static int       cap_nowin       = 0;   /* "no window" already reported */

/* ---------- PNG writing ---------- */

static void png_write_to_file(png_structp png, png_bytep data, png_size_t len) {
    FILE *f = (FILE *)png_get_io_ptr(png);
    fwrite(data, 1, len, f);
}

static void png_flush_file(png_structp png) {
    FILE *f = (FILE *)png_get_io_ptr(png);
    fflush(f);
}


/* the label of the next picture, kept in it as its PNG title */
static char cap_label[80];

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
    if (cap_label[0]) {
        /* the label rides as the PNG title: the viewer steps whole frames by it */
        png_text txt;
        memset(&txt, 0, sizeof(txt));
        txt.compression = PNG_TEXT_COMPRESSION_NONE;
        txt.key = (png_charp)"Title";
        txt.text = cap_label;
        txt.text_length = strlen(cap_label);
        png_set_text(png, info, &txt, 1);
    }
    png_write_info(png, info);

    for (int y = 0; y < height; y++)
        png_write_row(png, rgb_rows + y * width * 3);

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fflush(f);
}

/* ---------- public entry points ---------- */

/* Label the next picture, "frame N" or "frame N.S": it is kept in the picture
   as its title, and the viewer's shifted arrows step by it. One picture. */
void screen_capture_label(const char* s) {
    strncpy(cap_label, s, sizeof(cap_label)-1);
}

/* Name the file the pictures go to, before the first of them is taken.
   Later than that the file is already open and the name is ignored. */
void screen_capture_name(const char* fn) {
    if (!fn || !*fn || cap_opened) return;
    snprintf(cap_name, sizeof(cap_name), "%s", fn);
}

/* the file is made at the first capture, so a name can be given first */
static void capopen(void) {
    cap_opened = 1;
    remove(cap_name);
    cap_file = fopen(cap_name, "wb");
    if (!cap_file) {
        perror("screen_capture: fopen");
        cap_disabled = 1;
    }
}

void screen_capture(void) {
    uint32_t  *pix;
    uint8_t   *rgb;
    int        w, h, x, y;

    if (cap_disabled) return;
    if (!cap_opened) capopen();
    if (!cap_file) return;

    /* the composed picture: the client area with everything standing on it */
    pix = grx_capture(&w, &h);
    if (!pix) {
        if (!cap_nowin) {
            fprintf(stderr, "screen_capture: no Petit-Ami window found\n");
            cap_nowin = 1;
        }
        return;
    }

    rgb = malloc((size_t)w*h*3);
    if (!rgb) {
        free(pix);
        fprintf(stderr, "screen_capture: out of memory\n");
        return;
    }
    for (y = 0; y < h; y++) {

        uint32_t *sp = pix+(size_t)y*w;
        uint8_t  *dp = rgb+(size_t)y*w*3;

        for (x = 0; x < w; x++) {

            uint32_t p = sp[x];

            *dp++ = (p>>16)&0xff;
            *dp++ = (p>>8)&0xff;
            *dp++ = p&0xff;

        }

    }

    write_png_frame(cap_file, rgb, w, h);
    cap_label[0] = 0;
    cap_frame_count++;

    free(rgb);
    free(pix);
}

/* ---------- module lifecycle ---------- */

__attribute__((destructor))
static void screen_capture_fini(void) {
    if (cap_file) {
        fflush(cap_file);
        fclose(cap_file);
        cap_file = NULL;
        if (cap_frame_count == 0) {
            remove(cap_name);
        } else if (!isatty(fileno(stderr))) {
            /* The summary is for harness runs with stderr captured. On a
               terminal it landed on the program's final screen, printing
               over whatever the test left there. */
            fprintf(stderr, "screen_capture: wrote %u frames to %s\n",
                    cap_frame_count, cap_name);
        }
    }
}
