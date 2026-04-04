/*******************************************************************************
*                                                                              *
*                       PETIT-AMI BMP STREAM VIEWER                            *
*                                                                              *
* Walks through a file of concatenated 24-bit BMPs (as produced by the         *
* screen_capture module) and displays them one at a time in a graphical        *
* window. Navigate frames with the left and right arrow keys.                  *
*                                                                              *
* Keys:                                                                        *
*   right arrow  — next frame                                                  *
*   left arrow   — previous frame                                              *
*   q / terminate — quit                                                       *
*                                                                              *
* Usage:                                                                       *
*   bin/testviewer [filename]       # default: test_images                     *
*                                                                              *
* Strategy:                                                                    *
*   1. On startup, index the file by reading each BMP's BITMAPFILEHEADER to    *
*      record its byte offset and size. Index is built once.                   *
*   2. When the user navigates to a frame, copy that BMP's bytes into a        *
*      temporary file ("testviewer_frame.bmp") and call ami_loadpict() to      *
*      have Petit-Ami load it, then ami_picture() to display it.               *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <graphics.h>

/* Petit-Ami's stdio.h does not re-export these; define them locally. */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/*
 * Petit-Ami's fseek() returns nonzero on success and zero on failure —
 * inverted from standard C. Wrap it so we can write portable-looking code:
 * pa_fseek() returns 0 on success like stdlib fseek().
 */
static int pa_fseek(FILE *f, long off, int whence) {
    return fseek(f, off, whence) ? 0 : -1;
}

#define DEFAULT_FILENAME "test_images"
#define TEMP_NAME_NOEXT  "testviewer_frame"
#define TEMP_NAME_WITH   "testviewer_frame.bmp"
#define PIC_SLOT         1

typedef struct {
    long     offset;     /* byte offset of this BMP within test_images */
    uint32_t size;       /* total BMP size from BITMAPFILEHEADER.bfSize */
} frame_idx_t;

static frame_idx_t *frames = NULL;
static int          nframes = 0;
static int          cur_frame = -1;   /* -1 = nothing loaded yet */

/* ---------- little-endian readers ---------- */

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* ---------- BMP stream indexing ---------- */

/*
 * Walk the concatenated BMP file, record each frame's offset and size.
 * Returns 0 on success, -1 on error.
 */
static int build_index(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "testviewer: cannot open %s\n", filename);
        return -1;
    }

    int cap = 16;
    frames = (frame_idx_t *)malloc(sizeof(frame_idx_t) * cap);
    if (!frames) { fclose(f); return -1; }

    for (;;) {
        long off = ftell(f);
        uint8_t hdr[14];
        size_t got = fread(hdr, 1, 14, f);
        if (got == 0) break;                      /* clean end of file */
        if (got < 14 || hdr[0] != 'B' || hdr[1] != 'M') {
            /* Trailing garbage (e.g. stale bytes from a previous larger
               run). Treat it as the end of the valid stream. */
            fprintf(stderr,
                    "testviewer: stopping at offset %ld after %d frame(s) "
                    "(trailing non-BMP data)\n", off, nframes);
            break;
        }
        uint32_t bfsize = read_u32_le(&hdr[2]);
        if (bfsize < 14) {
            fprintf(stderr, "testviewer: bogus bfSize=%u at offset %ld\n",
                    bfsize, off);
            fclose(f);
            return -1;
        }

        if (nframes >= cap) {
            cap *= 2;
            frame_idx_t *nf = (frame_idx_t *)realloc(frames,
                                              sizeof(frame_idx_t) * cap);
            if (!nf) { fclose(f); return -1; }
            frames = nf;
        }
        frames[nframes].offset = off;
        frames[nframes].size   = bfsize;
        nframes++;

        /* skip the rest of this BMP */
        if (pa_fseek(f, off + (long)bfsize, SEEK_SET) != 0) {
            fprintf(stderr, "testviewer: seek past frame %d failed\n",
                    nframes - 1);
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

/*
 * Extract frame index i into the temp BMP file.
 * Returns 0 on success, -1 on error.
 */
static int extract_frame(const char *src_name, int i) {
    if (i < 0 || i >= nframes) return -1;

    FILE *src = fopen(src_name, "rb");
    if (!src) return -1;
    if (pa_fseek(src, frames[i].offset, SEEK_SET) != 0) {
        fclose(src);
        return -1;
    }

    FILE *dst = fopen(TEMP_NAME_WITH, "wb");
    if (!dst) { fclose(src); return -1; }

    uint8_t buf[16384];
    uint32_t remaining = frames[i].size;
    while (remaining > 0) {
        size_t want = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        size_t got = fread(buf, 1, want, src);
        if (got == 0) {
            fclose(src); fclose(dst);
            return -1;
        }
        if (fwrite(buf, 1, got, dst) != got) {
            fclose(src); fclose(dst);
            return -1;
        }
        remaining -= (uint32_t)got;
    }

    fclose(src);
    fclose(dst);
    return 0;
}

/* ---------- display ---------- */

/*
 * Load frame i, fit it into the window, and paint.
 */
static void show_frame(const char *src_name, int i) {
    if (i < 0 || i >= nframes) return;

    if (extract_frame(src_name, i) != 0) {
        fprintf(stderr, "testviewer: failed to extract frame %d\n", i);
        return;
    }

    /* release any previous picture in this slot */
    if (cur_frame >= 0) ami_delpict(stdout, PIC_SLOT);

    ami_loadpict(stdout, PIC_SLOT, TEMP_NAME_NOEXT);
    int px = ami_pictsizx(stdout, PIC_SLOT);
    int py = ami_pictsizy(stdout, PIC_SLOT);

    int win_x = ami_maxxg(stdout);
    int win_y = ami_maxyg(stdout);

    /* scale to fit, preserving aspect ratio */
    double sx = (double)win_x / (double)px;
    double sy = (double)win_y / (double)py;
    double s  = sx < sy ? sx : sy;
    int dw = (int)(px * s);
    int dh = (int)(py * s);
    int ox = (win_x - dw) / 2 + 1;
    int oy = (win_y - dh) / 2 + 1;

    /* clear background */
    ami_fcolor(stdout, ami_white);
    ami_frect(stdout, 1, 1, win_x, win_y);
    ami_fcolor(stdout, ami_black);

    ami_picture(stdout, PIC_SLOT, ox, oy, ox + dw - 1, oy + dh - 1);

    cur_frame = i;
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    const char *fn = (argc > 1) ? argv[1] : DEFAULT_FILENAME;

    if (build_index(fn) != 0) return 1;
    if (nframes == 0) {
        fprintf(stderr, "testviewer: %s contains no BMP frames\n", fn);
        return 1;
    }
    printf("testviewer: indexed %d frame(s) from %s\n", nframes, fn);

    ami_auto(stdout, 0);
    ami_curvis(stdout, 0);
    ami_bcolor(stdout, ami_white);
    putchar('\f');

    show_frame(fn, 0);

    ami_evtrec er;
    for (;;) {
        ami_event(stdin, &er);
        if (er.etype == ami_etterm) break;
        if (er.etype == ami_etchar && (er.echar == 'q' || er.echar == 'Q')) break;
        if (er.etype == ami_etright) {
            if (cur_frame + 1 < nframes) show_frame(fn, cur_frame + 1);
        } else if (er.etype == ami_etleft) {
            if (cur_frame - 1 >= 0)     show_frame(fn, cur_frame - 1);
        }
    }

    if (cur_frame >= 0) ami_delpict(stdout, PIC_SLOT);
    remove(TEMP_NAME_WITH);
    free(frames);
    return 0;
}
