/*******************************************************************************
*                                                                              *
*                           SCREEN CAPTURE MODULE                              *
*                                                                              *
* Windows/GDI version. Grabs the pixel contents of the running Petit-Ami      *
* window and appends it as a PNG to a file called "test_images". Multiple      *
* PNGs are concatenated back-to-back in the same file; each is self-           *
* delimiting (PNG signature at the start, IEND chunk at the end) so readers   *
* can walk them sequentially.                                                  *
*                                                                              *
* The file is opened by a module constructor and closed by a destructor.       *
* The caller only needs to call screen_capture() at each interesting moment    *
* (e.g. just before waitnext() in a test program).                             *
*                                                                              *
* Output format matches linux/screen_capture.c and macosx/screen_capture.c    *
* (concatenated PNGs) so test_images files are portable across platforms for   *
* cross-platform regression comparison.                                        *
*                                                                              *
* Uses Win32 file APIs (CreateFile/WriteFile) exclusively to avoid the         *
* custom libc/stdio.c intercepting and applying CRLF translation.             *
*                                                                              *
* Window discovery:                                                            *
*   - Enumerates top-level windows looking for one owned by our process.       *
*   - Picks the largest visible window belonging to GetCurrentProcessId().     *
*                                                                              *
* Viewer:                                                                      *
*   bin/testviewer    # walks through the concatenated PNGs with arrow keys    *
*                                                                              *
*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
#include <png.h>

#define CAPTURE_FILENAME "test_images"

/* ---------- module state ---------- */

static HANDLE    cap_file        = INVALID_HANDLE_VALUE;
static HWND      cap_window      = NULL;   /* discovered lazily on first capture */
static uint32_t  cap_frame_count = 0;
static int       cap_disabled    = 0;

/* ---------- window discovery ---------- */

typedef struct {

    DWORD pid;          /* our process id */
    HWND  best;         /* best candidate window */
    int   best_area;    /* area of best candidate */

} find_ctx_t;

static BOOL CALLBACK find_window_cb(HWND hwnd, LPARAM lparam)
{
    find_ctx_t *ctx = (find_ctx_t *)lparam;
    DWORD wpid;

    GetWindowThreadProcessId(hwnd, &wpid);
    if (wpid != ctx->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    RECT rc;
    if (!GetClientRect(hwnd, &rc)) return TRUE;
    int area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > ctx->best_area) {
        ctx->best_area = area;
        ctx->best = hwnd;
    }
    return TRUE;
}

static HWND find_pa_window(void)
{
    find_ctx_t ctx;

    ctx.pid = GetCurrentProcessId();
    ctx.best = NULL;
    ctx.best_area = 0;
    EnumWindows(find_window_cb, (LPARAM)&ctx);
    return ctx.best;
}

/* ---------- GDI capture -> top-down RGB ---------- */

static uint8_t *capture_window_rgb(HWND hwnd, int *w, int *h)
{
    RECT rc;

    if (!GetClientRect(hwnd, &rc)) return NULL;
    *w = rc.right - rc.left;
    *h = rc.bottom - rc.top;
    if (*w <= 0 || *h <= 0) return NULL;

    HDC hdcWin = GetDC(hwnd);
    if (!hdcWin) return NULL;

    HDC hdcMem = CreateCompatibleDC(hdcWin);
    HBITMAP hbm = CreateCompatibleBitmap(hdcWin, *w, *h);
    HGDIOBJ old = SelectObject(hdcMem, hbm);

    BitBlt(hdcMem, 0, 0, *w, *h, hdcWin, 0, 0, SRCCOPY);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = *w;
    bmi.bmiHeader.biHeight      = -(*h);  /* negative = top-down */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int row_stride = ((*w * 3) + 3) & ~3;
    uint8_t *dib = (uint8_t *)malloc(row_stride * (*h));
    if (!dib) {
        SelectObject(hdcMem, old);
        DeleteObject(hbm);
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdcWin);
        return NULL;
    }

    GetDIBits(hdcMem, hbm, 0, *h, dib, &bmi, DIB_RGB_COLORS);

    SelectObject(hdcMem, old);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWin);

    uint8_t *rgb = (uint8_t *)malloc((size_t)(*w) * (*h) * 3);
    if (!rgb) { free(dib); return NULL; }

    for (int y = 0; y < *h; y++) {
        uint8_t *src = dib + y * row_stride;
        uint8_t *dst = rgb + y * (*w) * 3;
        for (int x = 0; x < *w; x++) {
            dst[0] = src[2];  /* R <- B */
            dst[1] = src[1];  /* G <- G */
            dst[2] = src[0];  /* B <- R */
            src += 3;
            dst += 3;
        }
    }

    free(dib);
    return rgb;
}

/* ---------- PNG writing via Win32 file handle ---------- */

static void png_write_to_handle(png_structp png, png_bytep data, png_size_t len)
{
    HANDLE h = (HANDLE)png_get_io_ptr(png);
    DWORD written;
    WriteFile(h, data, (DWORD)len, &written, NULL);
}

static void png_flush_handle(png_structp png)
{
    HANDLE h = (HANDLE)png_get_io_ptr(png);
    FlushFileBuffers(h);
}

static void write_png_frame(HANDLE h, uint8_t *rgb_rows, int width, int height)
{
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    if (!png) return;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); return; }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return;
    }
    png_set_write_fn(png, (void *)h, png_write_to_handle, png_flush_handle);
    png_set_IHDR(png, info, width, height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    for (int y = 0; y < height; y++)
        png_write_row(png, rgb_rows + y * width * 3);

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    FlushFileBuffers(h);
}

/* ---------- public entry point ---------- */

void screen_capture(void)
{
    if (cap_disabled || cap_file == INVALID_HANDLE_VALUE) return;

    if (cap_window == NULL) {
        cap_window = find_pa_window();
        if (cap_window == NULL) return;
    }

    int w, h;
    uint8_t *rgb = capture_window_rgb(cap_window, &w, &h);
    if (!rgb) return;

    write_png_frame(cap_file, rgb, w, h);
    cap_frame_count++;

    free(rgb);
}

/* ---------- module lifecycle ---------- */

__attribute__((constructor(103)))
static void screen_capture_init(void)
{
    DeleteFileA(CAPTURE_FILENAME);
    cap_file = CreateFileA(CAPTURE_FILENAME, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (cap_file == INVALID_HANDLE_VALUE) {
        cap_disabled = 1;
        return;
    }
}

__attribute__((destructor(103)))
static void screen_capture_fini(void)
{
    if (cap_file != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(cap_file);
        CloseHandle(cap_file);
        cap_file = INVALID_HANDLE_VALUE;
        if (cap_frame_count == 0) {
            DeleteFileA(CAPTURE_FILENAME);
        }
    }
}
