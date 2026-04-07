/*******************************************************************************
*                                                                              *
*                     STANDALONE WIN32 PNG STREAM VIEWER                        *
*                                                                              *
* Walks through a file of concatenated PNGs (as produced by the screen_capture *
* module) and displays them one at a time in a Win32 window.                   *
* Uses only Win32 GDI + libpng - no Ami dependency.                            *
*                                                                              *
* Keys:                                                                        *
*   right arrow  - next frame                                                  *
*   left arrow   - previous frame                                              *
*   q / close button - quit                                                    *
*                                                                              *
* Usage:                                                                       *
*   bin\testviewer [filename]       # default: test_images                     *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <png.h>

#define DEFAULT_FILENAME "test_images"

/* PNG signature: 8 bytes */
static const uint8_t png_sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

typedef struct {
    long offset;    /* byte offset within file */
    long size;      /* total PNG size in bytes */
} frame_idx_t;

static frame_idx_t *frames     = NULL;
static int          nframes    = 0;
static const char  *g_filename = NULL;
static int          cur_frame  = 0;
static int          cur_w      = 0;
static int          cur_h      = 0;
static uint8_t     *cur_rgb    = NULL;    /* current frame as packed RGB */

/* ---------- PNG stream indexing ---------- */

static uint32_t read_u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static int build_index(const char *filename)
{
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

static void png_read_from_mem(png_structp png, png_bytep out, png_size_t len)
{
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
                           int *w, int *h)
{
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

static uint8_t *load_frame(const char *filename, int i, int *w, int *h)
{
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

/* ---------- Win32 display ---------- */

/*
 * Paint the current RGB frame to the window using StretchDIBits.
 * RGB is converted to BGR on-the-fly via the DIB format.
 */
static void paint_frame(HWND hwnd)
{
    if (!cur_rgb) return;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    /* build a top-down BGR DIB from the RGB buffer */
    int row_stride = ((cur_w * 3) + 3) & ~3;
    uint8_t *dib = (uint8_t *)malloc(row_stride * cur_h);
    if (dib) {
        for (int y = 0; y < cur_h; y++) {
            uint8_t *src = cur_rgb + y * cur_w * 3;
            uint8_t *dst = dib + y * row_stride;
            for (int x = 0; x < cur_w; x++) {
                dst[0] = src[2];  /* B */
                dst[1] = src[1];  /* G */
                dst[2] = src[0];  /* R */
                src += 3;
                dst += 3;
            }
        }

        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = cur_w;
        bmi.bmiHeader.biHeight      = -cur_h;  /* negative = top-down */
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 24;
        bmi.bmiHeader.biCompression = BI_RGB;

        SetDIBitsToDevice(hdc, 0, 0, cur_w, cur_h,
                          0, 0, 0, cur_h,
                          dib, &bmi, DIB_RGB_COLORS);
        free(dib);
    }

    EndPaint(hwnd, &ps);
}

static void update_title(HWND hwnd)
{
    char title[128];
    snprintf(title, sizeof(title), "testviewer [%d/%d]",
             cur_frame + 1, nframes);
    SetWindowText(hwnd, title);
}

static void switch_frame(HWND hwnd, int new_frame)
{
    if (new_frame < 0 || new_frame >= nframes) return;
    if (new_frame == cur_frame) return;

    int nw, nh;
    uint8_t *rgb = load_frame(g_filename, new_frame, &nw, &nh);
    if (!rgb) return;

    if (cur_rgb) free(cur_rgb);
    cur_rgb = rgb;
    cur_w = nw;
    cur_h = nh;
    cur_frame = new_frame;

    /* resize window to fit new frame (adjust for frame/title) */
    RECT rc = { 0, 0, cur_w, cur_h };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(hwnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER);

    update_title(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {

    case WM_PAINT:
        paint_frame(hwnd);
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_RIGHT)
            switch_frame(hwnd, cur_frame + 1);
        else if (wp == VK_LEFT)
            switch_frame(hwnd, cur_frame - 1);
        else if (wp == 'Q')
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

int main(int argc, char *argv[])
{
    g_filename = (argc > 1) ? argv[1] : DEFAULT_FILENAME;

    if (build_index(g_filename) != 0) return 1;
    if (nframes == 0) {
        fprintf(stderr, "testviewer: %s contains no PNG frames\n", g_filename);
        return 1;
    }
    fprintf(stderr, "testviewer: indexed %d frame(s) from %s\n",
            nframes, g_filename);

    /* decode first frame */
    cur_rgb = load_frame(g_filename, 0, &cur_w, &cur_h);
    if (!cur_rgb) {
        fprintf(stderr, "testviewer: failed to decode frame 0\n");
        return 1;
    }

    /* register window class */
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = wndproc;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName  = "TestViewerClass";
    RegisterClass(&wc);

    /* create window sized to first frame */
    RECT rc = { 0, 0, cur_w, cur_h };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindow("TestViewerClass", "testviewer [1/1]",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top,
                              NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) {
        fprintf(stderr, "testviewer: CreateWindow failed\n");
        return 1;
    }

    update_title(hwnd);

    /* message loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (cur_rgb) free(cur_rgb);
    free(frames);
    return 0;
}
