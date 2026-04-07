/*******************************************************************************
*                                                                              *
*                    macOS Cocoa/Quartz graphics implementation                *
*                                                                              *
* Implements the Ami graphics API using CoreGraphics for drawing and           *
* CoreText for font rendering.  Window/event management is delegated to the   *
* Objective-C shim in graphics_cocoa.m via pa_cocoa.h.                        *
*                                                                              *
* No override vector system — widgets are handled natively by Cocoa controls. *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>

#include <localdefs.h>
#include <graphics.h>
#include "pa_cocoa.h"

/* libc/stdio.c vector override types — same ABI as the internal vectors */
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef ssize_t (*pread_t)(int, void*, size_t);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_read(pread_t  nfp, pread_t*  ofp);

/*******************************************************************************
*                                                                              *
*                              Constants                                       *
*                                                                              *
*******************************************************************************/

#define MAXFIL      100     /* maximum open file/window slots */
#define MAXPIC      50      /* maximum loadable pictures */
#define MAXTIM      10      /* maximum timers per window */
#define MAXCON      10      /* maximum screen contexts per window */
#define MAXXD       80      /* default terminal width in chars */
#define MAXYD       25      /* default terminal height in chars */
#define DEF_FONT_H  16      /* default font height in points */

/* PA angles: INT_MAX = 360 degrees */
#define ANG2RAD(a)  ((a) * (2.0 * M_PI) / (double)INT_MAX)

/*******************************************************************************
*                                                                              *
*                              Types                                           *
*                                                                              *
*******************************************************************************/

/* Drawing mode */
typedef enum { mdnorm, mdinvis, mdxor, mdand, mdor } drawmode;

/* Color as RGBA floats */
typedef struct { CGFloat r, g, b, a; } pa_rgba;

/* Font entry */
typedef struct fontrec {
    struct fontrec* next;
    char*           name;       /* display name */
    CTFontRef       ctfont;     /* CoreText font at current size */
    int             fixed;      /* is fixed-pitch */
    CGFloat         size;       /* current size in points */
} fontrec, *fontptr;

/* Screen context (PA supports multiple virtual screens per window) */
typedef struct scncon {
    int         curx,  cury;   /* text cursor (1-based char) */
    int         curxg, curyg;  /* graphics cursor (1-based pixel) */
    int         curv;          /* cursor visible */
    pa_rgba     fc, bc;        /* foreground / background color */
    drawmode    fmod, bmod;    /* draw modes */
    CGFloat     lwidth;        /* line width */
    fontptr     font;          /* current font */
    int         bold, italic, underline, strikeout;
    int         attr;          /* attribute bitmask (reverse etc.) */
    int         autof;         /* auto scroll/wrap mode */
} scncon, *scnptr;

/* Per-window record */
typedef struct winrec {
    pa_winhan   han;            /* Cocoa window handle */
    FILE*       infile;         /* associated input  stream */
    FILE*       outfile;        /* associated output stream */
    int         wid;            /* window id */
    int         parwid;         /* parent window id */

    /* dimensions */
    int         maxxg, maxyg;  /* graphical size in pixels */
    int         maxx,  maxy;   /* text size in chars */
    int         charspace;     /* pixels per char column */
    int         linespace;     /* pixels per char row */
    int         dpmx, dpmy;    /* dots per meter x/y */

    /* screens */
    scncon      screens[MAXCON];
    int         curdsp;        /* current display screen (1-based) */
    int         curupd;        /* current update screen  (1-based) */

    /* state */
    int         visible;
    int         frame, sizable, sysbar;
    int         bufmod;        /* double-buffer on */
    int         focus;
    int         fautohold;     /* auto hold on exit */

    /* font */
    fontptr     cfont;         /* current font */
    CGFloat     fontsz;        /* font size in points */

    /* event handler */
    ami_pevthan evthan;
} winrec, *winptr;

/*******************************************************************************
*                                                                              *
*                              Globals                                         *
*                                                                              *
*******************************************************************************/

static winrec   wintbl[MAXFIL];         /* window table indexed by FILE slot */
static int      opnfil[MAXFIL];         /* 1 = slot in use */
static FILE*    filwin[MAXFIL];         /* file → window mapping */
static fontptr  fntlst;                 /* global font list */
static int      fntcnt;                 /* number of fonts */
static int      inited;                 /* library initialised */
static int      fend;                   /* program ending flag */
static int      fautohold;             /* global auto hold */
static pwrite_t ofpwrite;              /* saved write vector */
static pread_t  ofpread;               /* saved read vector */
static int      maxxd;                 /* default window width in pixels */
static int      maxyd;                 /* default window height in pixels */

/* forward declarations */
static void    clear_window(winptr win);
static void    plcchr(winptr win, char c);
static ssize_t iwrite(int fd, const void* buff, size_t count);
static ssize_t iread(int fd, void* buff, size_t count);


/* standard color table: ami_color → RGBA */
static const pa_rgba colortbl[] = {
    {0,0,0,1},       /* ami_black    */
    {1,1,1,1},       /* ami_white    */
    {1,0,0,1},       /* ami_red      */
    {0,1,0,1},       /* ami_green    */
    {0,0,1,1},       /* ami_blue     */
    {0,1,1,1},       /* ami_cyan     */
    {1,1,0,1},       /* ami_yellow   */
    {1,0,1,1},       /* ami_magenta  */
    {1,1,1,1}        /* ami_backcolor (white) */
};

/*******************************************************************************
*                                                                              *
*                              Utilities                                       *
*                                                                              *
*******************************************************************************/

/* Get window record from FILE* */
static winptr f2win(FILE* f)
{
    int fd = fileno(f);
    if (fd < 0 || fd >= MAXFIL || !opnfil[fd]) return NULL;
    return &wintbl[fd];
}

/* Get current update screen */
static scnptr curscn(winptr win)
{
    return &win->screens[win->curupd - 1];
}

/* Apply foreground color to CGContext */
static void set_fg(CGContextRef ctx, pa_rgba c)
{
    CGContextSetRGBStrokeColor(ctx, c.r, c.g, c.b, c.a);
    CGContextSetRGBFillColor(ctx, c.r, c.g, c.b, c.a);
}

static void set_fg_only(CGContextRef ctx, pa_rgba c)
{
    CGContextSetRGBStrokeColor(ctx, c.r, c.g, c.b, c.a);
}

static void set_fill_only(CGContextRef ctx, pa_rgba c)
{
    CGContextSetRGBFillColor(ctx, c.r, c.g, c.b, c.a);
}

/* Convert PA 1-based coordinate to CG 0-based */
#define PX(x) ((x) - 1)
#define PY(y) ((y) - 1)

/* Convert ami_color to pa_rgba */
static pa_rgba ami2rgba(ami_color c)
{
    if (c < 0 || c > ami_backcolor) c = ami_white;
    return colortbl[c];
}

/* Allocate a new window slot, returns fd or -1 */
static int alloc_win(void)
{
    for (int i = 3; i < MAXFIL; i++) /* skip stdin/stdout/stderr */
        if (!opnfil[i]) return i;
    return -1;
}

/*******************************************************************************
*                                                                              *
*                              Font management                                 *
*                                                                              *
*******************************************************************************/

static CTFontRef make_ctfont(const char* name, CGFloat size, int bold, int italic)
{
    CTFontSymbolicTraits traits = 0;
    if (bold)   traits |= kCTFontTraitBold;
    if (italic) traits |= kCTFontTraitItalic;

    CFStringRef cfname = CFStringCreateWithCString(NULL, name,
                                                   kCFStringEncodingUTF8);
    CTFontRef base = CTFontCreateWithName(cfname, size, NULL);
    CFRelease(cfname);

    if (traits) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(base, size,
                                                              NULL, traits, traits);
        if (styled) { CFRelease(base); return styled; }
    }
    return base;
}

/* Build the standard 4 fonts (TERM, BOOK, SIGN, TECH) */
static void init_fonts(void)
{
    /* font names to try, in preference order, for each slot */
    static const char* term_names[] = {
        "Menlo", "Monaco", "Courier New", "Courier", NULL
    };
    static const char* book_names[] = {
        "Georgia", "Palatino", "Times New Roman", "Times", NULL
    };
    static const char* sign_names[] = {
        "Helvetica Neue", "Helvetica", "Arial", "Verdana", NULL
    };

    const char** lists[4] = { term_names, book_names, sign_names, sign_names };
    fntlst = NULL;
    fntcnt = 0;

    for (int slot = 0; slot < 4; slot++) {
        fontptr fp = calloc(1, sizeof(fontrec));
        /* find first available name */
        for (const char** np = lists[slot]; *np; np++) {
            CTFontRef f = make_ctfont(*np, DEF_FONT_H, 0, 0);
            if (f) {
                fp->ctfont = f;
                fp->name   = strdup(*np);
                fp->size   = DEF_FONT_H;
                fp->fixed  = (slot == 0); /* TERM is fixed */
                break;
            }
        }
        if (!fp->ctfont) {
            /* last resort: system font */
            fp->ctfont = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,
                                                       DEF_FONT_H, NULL);
            fp->name   = strdup("System");
            fp->size   = DEF_FONT_H;
        }
        fp->next = fntlst;
        fntlst   = fp;
        fntcnt++;
    }

    /* reverse list so TERM=1, BOOK=2, SIGN=3, TECH=4 */
    fontptr prev = NULL, cur = fntlst, next;
    while (cur) { next = cur->next; cur->next = prev; prev = cur; cur = next; }
    fntlst = prev;
}

/* Measure a string with current font; returns width in pixels */
static CGFloat measure_string(fontptr fp, const char* s, int len)
{
    if (!fp || !fp->ctfont || !s || len <= 0) return 0;
    CFStringRef  cfs = CFStringCreateWithBytes(NULL, (const UInt8*)s, len,
                                               kCFStringEncodingUTF8, false);
    CGFloat w = 0;
    if (cfs) {
        CFStringRef keys[1]   = { kCTFontAttributeName };
        CFTypeRef   values[1] = { fp->ctfont };
        CFDictionaryRef attrs = CFDictionaryCreate(NULL,
                                   (const void**)keys, (const void**)values,
                                   1, &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cfs, attrs);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        w = (CGFloat)CTLineGetTypographicBounds(line, NULL, NULL, NULL);
        CFRelease(line);
        CFRelease(as);
        CFRelease(attrs);
        CFRelease(cfs);
    }
    return w;
}

/* Draw a string at (x, y) — top-left based, PA 1-based coords */
static void draw_string(CGContextRef ctx, fontptr fp, scnptr sc,
                        int x, int y, const char* s, int len)
{
    if (!ctx || !fp || !fp->ctfont || !s || len <= 0) return;

    CFStringRef cfs = CFStringCreateWithBytes(NULL, (const UInt8*)s, len,
                                              kCFStringEncodingUTF8, false);
    if (!cfs) return;

    /* build attribute dictionary */
    CGFloat fgr = sc->fc.r, fgg = sc->fc.g, fgb = sc->fc.b;
    CGColorRef color = CGColorCreateGenericRGB(fgr, fgg, fgb, 1.0);

    CFStringRef keys[2]   = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef   values[2] = { fp->ctfont, color };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL,
                               (const void**)keys, (const void**)values,
                               2, &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
    CGColorRelease(color);

    CFAttributedStringRef as   = CFAttributedStringCreate(NULL, cfs, attrs);
    CTLineRef             line = CTLineCreateWithAttributedString(as);

    CGFloat ascent, descent;
    CTLineGetTypographicBounds(line, &ascent, &descent, NULL);

    /* The bitmap context has a top-left-down coordinate system (flipped).
     * CoreText draws with a bottom-left-up system. We temporarily unflip
     * the context around the line's origin so the glyphs come out right. */
    CGFloat tx = PX(x);
    CGFloat ty = PY(y) + ascent; /* baseline position in flipped coords */
    CGContextSaveGState(ctx);
    /* translate to the baseline point, flip Y, then draw at origin */
    CGContextTranslateCTM(ctx, tx, ty);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetTextPosition(ctx, 0, 0);
    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);

    CFRelease(line);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(cfs);
}

/*******************************************************************************
*                                                                              *
*                        Window init / deinit                                  *
*                                                                              *
*******************************************************************************/

static void win_init(winptr win, int wid, int parwid, int w, int h)
{
    memset(win, 0, sizeof(winrec));
    win->wid     = wid;
    win->parwid  = parwid;
    win->maxxg   = w;
    win->maxyg   = h;
    win->visible = FALSE;
    win->frame   = TRUE;
    win->sizable = TRUE;
    win->sysbar  = TRUE;
    win->bufmod  = TRUE;
    win->curdsp  = 1;
    win->curupd  = 1;
    win->cfont   = fntlst;
    win->fontsz  = DEF_FONT_H;

    /* screen size in mm from shim */
    int smm_w  = pa_cocoa_screen_wmm();
    int smm_h  = pa_cocoa_screen_hmm();
    int spx_w  = pa_cocoa_screen_w();
    int spx_h  = pa_cocoa_screen_h();
    win->dpmx  = (smm_w > 0) ? spx_w * 1000 / smm_w : 3780; /* ~96 dpi */
    win->dpmy  = (smm_h > 0) ? spx_h * 1000 / smm_h : 3780;

    /* character cell size from font metrics */
    CTFontRef f = fntlst ? fntlst->ctfont : NULL;
    if (f) {
        win->linespace  = (int)(win->fontsz + 0.5);
        {
            /* use advance of 'M' as max_advance approximation */
            CGGlyph  glyph;
            UniChar  ch = 'M';
            CTFontGetGlyphsForCharacters(f, &ch, &glyph, 1);
            CGSize   adv;
            CTFontGetAdvancesForGlyphs(f, kCTFontOrientationDefault,
                                       &glyph, &adv, 1);
            win->charspace = (int)(adv.width + 0.5);
        }
        if (win->charspace <= 0) win->charspace = win->linespace / 2;
    } else {
        win->linespace = DEF_FONT_H + 2;
        win->charspace = (DEF_FONT_H + 2) / 2;
    }
    win->maxx = w / win->charspace;
    win->maxy = h / win->linespace;
    if (win->maxx < 1) win->maxx = 1;
    if (win->maxy < 1) win->maxy = 1;

    /* init all screens */
    for (int i = 0; i < MAXCON; i++) {
        scnptr sc   = &win->screens[i];
        sc->curx    = 1;
        sc->cury    = 1;
        sc->curxg   = 1;
        sc->curyg   = 1;
        sc->curv    = TRUE;
        sc->fc      = colortbl[ami_black];
        sc->bc      = colortbl[ami_white];
        sc->fmod    = mdnorm;
        sc->bmod    = mdnorm;
        sc->lwidth  = 1.0;
        sc->font    = fntlst;
        sc->autof   = TRUE;
    }
}

/*******************************************************************************
*                                                                              *
*                        Library init (constructor)                            *
*                                                                              *
*******************************************************************************/

__attribute__((constructor))
static void pa_graphics_init(void)
{
    if (inited) return;
    inited = TRUE;

    pa_cocoa_init();
    init_fonts();
    memset(opnfil, 0, sizeof(opnfil));

    fautohold = TRUE; /* hold window open on exit until keypress */

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* hook libc/stdio.c write/read vectors */
    ovr_write(iwrite, &ofpwrite);
    ovr_read(iread,  &ofpread);

    /* compute default window pixel size from TERM font character grid */
    {
        fontptr fp = fntlst;
        CGGlyph glyph; UniChar ch = 'M'; CGSize adv;
        CTFontGetGlyphsForCharacters(fp->ctfont, &ch, &glyph, 1);
        CTFontGetAdvancesForGlyphs(fp->ctfont, kCTFontOrientationDefault,
                                   &glyph, &adv, 1);
        int cs = (int)(adv.width + 0.5);
        int ls = (int)(fp->size + 0.5);
        if (cs <= 0) cs = ls / 2;
        maxxd = MAXXD * cs;
        maxyd = MAXYD * ls;
    }

    /* open default window for stdout (fd=1) */
    pa_winhan han = pa_cocoa_create_window(100, 100, maxxd, maxyd,
#if defined(__MACH__) || defined(__FreeBSD__)
                                           getprogname()
#else
                                           "ami"
#endif
                                           );
    if (han) {
        winptr win = &wintbl[1];
        win_init(win, 1, 0, maxxd, maxyd);
        win->han   = han;
        opnfil[1]  = 1; /* mark slot in use */
        clear_window(win);
        pa_cocoa_show_window(han);
        pa_cocoa_flush(han);
    }
}

__attribute__((destructor))
static void pa_graphics_deinit(void)
{
    winptr win;
    ami_evtrec er;

    /* check if stdout is attached to a window */
    win = NULL;
    if (opnfil[1]) win = &wintbl[1];
    if (win && win->han) {

        /* if the program exits without the user ordering an exit,
           and autohold is set, hold the window open */
        if (!fend && fautohold) {

            /* construct "Finished - <progname>" title */
            const char* fini = "Finished - ";
            const char* pname = getprogname();
            char* trmnam = malloc(strlen(fini) + strlen(pname) + 1);
            if (trmnam) {
                strcpy(trmnam, fini);
                strcat(trmnam, pname);
                pa_cocoa_set_title(win->han, trmnam);
                free(trmnam);
            }

            /* wait for a formal end (etterm from close button or Ctrl-C) */
            while (!fend) ami_event(stdin, &er);

        }

    }

    /* restore stdio vectors */
    {
        pwrite_t cppwrite;
        pread_t  cppread;
        ovr_write(ofpwrite, &cppwrite);
        ovr_read(ofpread, &cppread);
    }

    pa_cocoa_deinit();
}

/*******************************************************************************
*                                                                              *
*                        Context helpers                                       *
*                                                                              *
*******************************************************************************/

/* Get the CGContext for a window and configure it for the current screen */
static CGContextRef get_ctx(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return NULL;
    scnptr sc = curscn(win);
    CGContextSetLineWidth(ctx, sc->lwidth);
    CGContextSetShouldAntialias(ctx, false); /* crisp lines */
    set_fg(ctx, sc->fc);
    return ctx;
}

/* Clear the window to background color */
static void clear_window(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;
    scnptr sc = curscn(win);
    CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
    CGContextFillRect(ctx, CGRectMake(0, 0, win->maxxg, win->maxyg));
    CGContextSetRGBFillColor(ctx, sc->fc.r, sc->fc.g, sc->fc.b, 1.0);
}

/*******************************************************************************
*                                                                              *
*                   Character output / terminal emulation                      *
*                                                                              *
*******************************************************************************/

/* Scroll window content up by one text line.
 * Uses raw bitmap memmove: row 0 in the bitmap data is the top of the screen
 * (because CGBitmapContextCreateImage + flipped NSView compose to that).
 * Shifts physical rows toward lower addresses (upward), then redraws the
 * cleared bottom strip with the background color. */
static void scroll_up(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;

    size_t   width    = CGBitmapContextGetWidth(ctx);
    size_t   height   = CGBitmapContextGetHeight(ctx);
    size_t   rowbytes = CGBitmapContextGetBytesPerRow(ctx);
    uint8_t* data     = (uint8_t*)CGBitmapContextGetData(ctx);
    if (!data) return;

    /* scale: physical pixels per logical pixel */
    float scale   = (win->maxyg > 0) ? (float)height / win->maxyg : 1.0f;
    size_t physLS = (size_t)(win->linespace * scale + 0.5f);
    if (physLS == 0 || physLS >= height) return;
    (void)width; /* not needed for row-wise memmove */

    /* move rows up */
    memmove(data, data + physLS * rowbytes, (height - physLS) * rowbytes);

    /* fill the freed rows at the bottom with the background color using CG */
    scnptr sc = curscn(win);
    CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
    int clearY = (win->maxy - 1) * win->linespace; /* top of last text row, 0-based */
    CGContextFillRect(ctx, CGRectMake(0, PY(clearY + 1), win->maxxg, win->linespace));
    /* restore foreground */
    CGContextSetRGBFillColor(ctx, sc->fc.r, sc->fc.g, sc->fc.b, 1.0);
}

/* Draw one character cell at the current text cursor position.
 * Fills the background cell first, then draws the glyph.
 * Uses curxg/curyg (pixel coords) so arbitrary cursor placement works.
 * Returns the actual pixel width of the character drawn. */
static int draw_char_at(winptr win, char c)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return win->charspace;
    scnptr  sc  = curscn(win);
    fontptr fp  = sc->font ? sc->font : fntlst;

    /* measure actual character width */
    int cw;
    if (fp && !fp->fixed) {
        cw = (int)(measure_string(fp, &c, 1) + 0.5);
        if (cw <= 0) cw = win->charspace;
    } else {
        cw = win->charspace;
    }

    int px = sc->curxg - 1; /* 1-based to 0-based */
    int py = sc->curyg - 1;

    /* fill cell background */
    if (sc->bmod == mdnorm) {
        CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
        CGContextFillRect(ctx, CGRectMake(px, py, cw, win->linespace));
    }

    /* draw glyph */
    char s[2] = { c, 0 };
    CGContextSetRGBFillColor(ctx, sc->fc.r, sc->fc.g, sc->fc.b, 1.0);
    draw_string(ctx, fp, sc, px + 1, py + 1, s, 1);

    return cw;
}

/* Place one character into a window, handling control characters. */
static void plcchr(winptr win, char c)
{
    scnptr sc = curscn(win);

    if (c == '\n') {
        /* newline: CR+LF — move to column 1 on next row */
        sc->curx  = 1;
        sc->curxg = 1;
        if (sc->cury >= win->maxy) {
            if (sc->autof) scroll_up(win);
            /* else: cursor stays, no scroll */
        } else {
            sc->cury++;
            sc->curyg = (sc->cury - 1) * win->linespace + 1;
        }
    } else if (c == '\r') {
        sc->curx  = 1;
        sc->curxg = 1;
    } else if (c == '\b') {
        if (sc->curx > 1) {
            sc->curx--;
            sc->curxg = (sc->curx - 1) * win->charspace + 1;
        }
    } else if (c == '\f') {
        /* form feed: clear screen and home cursor */
        clear_window(win);
        sc->curx  = 1;
        sc->cury  = 1;
        sc->curxg = 1;
        sc->curyg = 1;
    } else if (c == '\t') {
        /* advance to next 8-column tab stop */
        int next = ((sc->curx - 1) / 8 + 1) * 8 + 1;
        if (next > win->maxx) next = win->maxx;
        sc->curx  = next;
        sc->curxg = (sc->curx - 1) * win->charspace + 1;
    } else if ((unsigned char)c >= 0x20) {
        /* printable character — draw and advance by actual glyph width */
        int cs = draw_char_at(win, c);
        sc->curx++;
        sc->curxg += cs;
        if (sc->curx > win->maxx && sc->autof) {
            /* auto wrap to next line */
            sc->curx  = 1;
            sc->curxg = 1;
            if (sc->cury >= win->maxy) {
                scroll_up(win);
            } else {
                sc->cury++;
                sc->curyg = (sc->cury - 1) * win->linespace + 1;
            }
        }
    }
}

/* Write interceptor: routes writes to stdout/window fds through plcchr() */
static ssize_t iwrite(int fd, const void* buff, size_t count)
{
    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && wintbl[fd].han) {
        const char* p   = (const char*)buff;
        size_t      cnt = count;
        winptr      win = &wintbl[fd];
        while (cnt--) plcchr(win, *p++);
        pa_cocoa_flush(win->han); /* blit to screen after each write */
        return (ssize_t)count;
    }
    return (*ofpwrite)(fd, buff, count);
}

/* Read interceptor: routes reads from window input fds (future: keyboard queue) */
static ssize_t iread(int fd, void* buff, size_t count)
{
    return (*ofpread)(fd, buff, count);
}

/*******************************************************************************
*                                                                              *
*                        Event translation                                     *
*                                                                              *
*******************************************************************************/

/* Map a PA window handle back to a wid */
static int han2wid(pa_winhan han)
{
    for (int i = 0; i < MAXFIL; i++)
        if (opnfil[i] && wintbl[i].han == han) return wintbl[i].wid;
    return 0;
}

static void translate_event(const pa_rawevent* raw, ami_evtrec* er)
{
    memset(er, 0, sizeof(*er));
    er->winid = han2wid(raw->win);

    switch (raw->type) {

    case PA_EVT_CHAR:
        if (raw->key.ch == 3) {
            /* Ctrl-C: generate etterm */
            er->etype = ami_etterm;
            fend = TRUE;
        } else {
            er->etype = ami_etchar;
            er->echar = (char)raw->key.ch;
        }
        break;

    case PA_EVT_KEYDOWN:
        switch (raw->special.code) {
        case PA_KEY_UP:       er->etype = ami_etup;      break;
        case PA_KEY_DOWN:     er->etype = ami_etdown;    break;
        case PA_KEY_LEFT:     er->etype = ami_etleft;    break;
        case PA_KEY_RIGHT:    er->etype = ami_etright;   break;
        case PA_KEY_HOME:     er->etype = ami_ethome;    break;
        case PA_KEY_END:      er->etype = ami_etend;     break;
        case PA_KEY_PAGEUP:   er->etype = ami_etpagu;    break;
        case PA_KEY_PAGEDOWN: er->etype = ami_etpagd;    break;
        case PA_KEY_DELETE:   er->etype = ami_etdelcf;   break;
        case PA_KEY_BACK:     er->etype = ami_etdelcb;   break;
        case PA_KEY_ENTER:    er->etype = ami_etenter;   break;
        case PA_KEY_TAB:      er->etype = ami_ettab;     break;
        case PA_KEY_ESC:      er->etype = ami_etcan;     break;
        default:
            if (raw->special.code >= PA_KEY_F1 &&
                raw->special.code <= PA_KEY_F12) {
                er->etype = ami_etfun;
                er->fkey  = raw->special.code - PA_KEY_F1 + 1;
            }
            break;
        }
        break;

    case PA_EVT_MOUSE_MOVE:
        er->etype  = ami_etmoumovg;
        er->mmoung = 1;
        er->moupxg = raw->mouse.x;
        er->moupyg = raw->mouse.y;
        break;

    case PA_EVT_MOUSE_DOWN:
        er->etype  = ami_etmouba;
        er->amoun  = 1;
        er->amoubn = raw->mouse.buttons;
        break;

    case PA_EVT_MOUSE_UP:
        er->etype  = ami_etmoubd;
        er->dmoun  = 1;
        er->dmoubn = raw->mouse.buttons;
        break;

    case PA_EVT_RESIZE:
        er->etype  = ami_etresize;
        er->rszxg  = raw->resize.w;
        er->rszyg  = raw->resize.h;
        /* update char dimensions */
        {
            winptr win = NULL;
            for (int i = 0; i < MAXFIL; i++)
                if (opnfil[i] && wintbl[i].han == raw->win)
                    { win = &wintbl[i]; break; }
            if (win) {
                win->maxxg = raw->resize.w;
                win->maxyg = raw->resize.h;
                win->maxx  = win->maxxg / win->charspace;
                win->maxy  = win->maxyg / win->linespace;
                er->rszx   = win->maxx;
                er->rszy   = win->maxy;
            }
        }
        break;

    case PA_EVT_CLOSE:
        er->etype = ami_etterm;
        break;

    case PA_EVT_FOCUS:
        er->etype = ami_etfocus;
        break;

    case PA_EVT_UNFOCUS:
        er->etype = ami_etnofocus;
        break;

    case PA_EVT_TIMER:
        er->etype  = ami_ettim;
        er->timnum = raw->timer.id + 1; /* PA timers are 1-based */
        break;

    case PA_EVT_REDRAW:
        er->etype = ami_etredraw;
        er->rsx   = raw->redraw.x;
        er->rsy   = raw->redraw.y;
        er->rex   = raw->redraw.x + raw->redraw.w - 1;
        er->rey   = raw->redraw.y + raw->redraw.h - 1;
        break;

    default:
        er->etype = ami_etchar;
        er->echar = 0;
        break;
    }
}

/*******************************************************************************
*                                                                              *
*                           PA API — text operations                           *
*                                                                              *
*******************************************************************************/

void ami_cursor(FILE* f, int x, int y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->curx   = x;
    sc->cury   = y;
    sc->curxg  = (x-1) * win->charspace + 1;
    sc->curyg  = (y-1) * win->linespace + 1;
}

int ami_maxx(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxx : 80;
}

int ami_maxy(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxy : 25;
}

void ami_home(FILE* f)    { ami_cursor(f, 1, 1); }

void ami_del(FILE* f)
{
    /* delete character at cursor — stub */
}

void ami_up(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->cury > 1) ami_cursor(f, sc->curx, sc->cury - 1);
}

void ami_down(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->cury < win->maxy) ami_cursor(f, sc->curx, sc->cury + 1);
}

void ami_left(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->curx > 1) ami_cursor(f, sc->curx - 1, sc->cury);
}

void ami_right(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->curx < win->maxx) ami_cursor(f, sc->curx + 1, sc->cury);
}

void ami_blink(FILE* f, int e)      { /* stub — CoreText doesn't blink */ }
void ami_reverse(FILE* f, int e)    { /* stub */ }
void ami_superscript(FILE* f, int e){ /* stub */ }
void ami_subscript(FILE* f, int e)  { /* stub */ }

void ami_underline(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->underline = e;
}

void ami_italic(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->italic = e;
}

void ami_bold(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bold = e;
}

void ami_strikeout(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->strikeout = e;
}

void ami_standout(FILE* f, int e)   { /* stub */ }

void ami_fcolor(FILE* f, ami_color c)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fc = ami2rgba(c);
}

void ami_bcolor(FILE* f, ami_color c)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bc = ami2rgba(c);
}

void ami_auto(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->autof = e;
}

void ami_curvis(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->curv = e;
}

void ami_scroll(FILE* f, int x, int y) { /* stub */ }

int ami_curx(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curx : 1;
}

int ami_cury(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->cury : 1;
}

int ami_curbnd(FILE* f)
{
    winptr win = f2win(f); if (!win) return FALSE;
    scnptr sc  = curscn(win);
    return sc->curx >= 1 && sc->curx <= win->maxx &&
           sc->cury >= 1 && sc->cury <= win->maxy;
}

void ami_select(FILE* f, int u, int d) { /* stub */ }

void ami_settab(FILE* f, int t)  { /* stub */ }
void ami_restab(FILE* f, int t)  { /* stub */ }
void ami_clrtab(FILE* f)         { /* stub */ }

int  ami_funkey(FILE* f)         { return 12; /* F1-F12 */ }

void ami_frametimer(FILE* f, int e) { /* stub */ }
void ami_autohold(int e)            { fautohold = e; }

void ami_wrtstr(FILE* f, char* s)
{
    if (!s) return;
    /* route through fwrite so stdio layer handles it */
    fwrite(s, 1, strlen(s), f);
}

void ami_wrtstrn(FILE* f, char* s, int n)
{
    if (!s || n <= 0) return;
    fwrite(s, 1, n, f);
}

void ami_sizbuf(FILE* f, int x, int y)   { /* stub */ }
void ami_sizbufg(FILE* f, int x, int y)  { /* stub */ }

void ami_title(FILE* f, char* ts)
{
    winptr win = f2win(f); if (!win || !ts) return;
    pa_cocoa_set_title(win->han, ts);
}

void ami_eventover(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)
{
    /* no override system in this implementation */
    if (oeh) *oeh = NULL;
}

void ami_eventsover(ami_pevthan eh, ami_pevthan* oeh)
{
    if (oeh) *oeh = NULL;
}

void ami_sendevent(FILE* f, ami_evtrec* er) { /* stub */ }

/*******************************************************************************
*                                                                              *
*                        PA API — graphical operations                         *
*                                                                              *
*******************************************************************************/

int ami_maxxg(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxxg : maxxd;
}

int ami_maxyg(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxyg : maxyd;
}

int ami_curxg(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curxg : 1;
}

int ami_curyg(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curyg : 1;
}

void ami_cursorg(FILE* f, int x, int y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->curxg  = x;
    sc->curyg  = y;
    sc->curx   = (x - 1) / win->charspace + 1;
    sc->cury   = (y - 1) / win->linespace + 1;
}

void ami_line(FILE* f, int x1, int y1, int x2, int y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx,    PX(x1) + 0.5, PY(y1) + 0.5);
    CGContextAddLineToPoint(ctx, PX(x2) + 0.5, PY(y2) + 0.5);
    CGContextStrokePath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_linewidth(FILE* f, int w)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->lwidth = (CGFloat)(w > 0 ? w : 1);
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (ctx) CGContextSetLineWidth(ctx, sc->lwidth);
}

void ami_rect(FILE* f, int x1, int y1, int x2, int y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGRect r = CGRectMake(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1);
    CGContextStrokeRect(ctx, r);
    pa_cocoa_flush(win->han);
}

void ami_frect(FILE* f, int x1, int y1, int x2, int y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGContextFillRect(ctx, CGRectMake(PX(x1), PY(y1), x2-x1+1, y2-y1+1));
    pa_cocoa_flush(win->han);
}

void ami_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGFloat rx = xs / 2.0, ry = ys / 2.0;
    CGFloat w  = x2 - x1, h = y2 - y1;
    CGFloat cx = PX(x1), cy = PY(y1);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, cx + rx, cy);
    CGContextAddArcToPoint(ctx, cx+w, cy,   cx+w, cy+h, rx);
    CGContextAddArcToPoint(ctx, cx+w, cy+h, cx,   cy+h, ry);
    CGContextAddArcToPoint(ctx, cx,   cy+h, cx,   cy,   rx);
    CGContextAddArcToPoint(ctx, cx,   cy,   cx+w, cy,   ry);
    CGContextClosePath(ctx);
    CGContextStrokePath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGFloat rx = xs / 2.0, ry = ys / 2.0;
    CGFloat w  = x2 - x1, h = y2 - y1;
    CGFloat cx = PX(x1), cy = PY(y1);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, cx + rx, cy);
    CGContextAddArcToPoint(ctx, cx+w, cy,   cx+w, cy+h, rx);
    CGContextAddArcToPoint(ctx, cx+w, cy+h, cx,   cy+h, ry);
    CGContextAddArcToPoint(ctx, cx,   cy+h, cx,   cy,   rx);
    CGContextAddArcToPoint(ctx, cx,   cy,   cx+w, cy,   ry);
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_ellipse(FILE* f, int x1, int y1, int x2, int y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextStrokeEllipseInRect(ctx,
        CGRectMake(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1));
    pa_cocoa_flush(win->han);
}

void ami_fellipse(FILE* f, int x1, int y1, int x2, int y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGContextFillEllipseInRect(ctx,
        CGRectMake(PX(x1), PY(y1), x2-x1+1, y2-y1+1));
    pa_cocoa_flush(win->han);
}

/* Convert PA angle to CG angle.
 * PA: 0 = top center, clockwise, INT_MAX = 360°.
 * CG (flipped Y-down context): 0 = right (3 o'clock), clockwise.
 * So CG = PA_radians - π/2. */
#define PA2CG(a) (ANG2RAD(a) - M_PI_2)

void ami_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGFloat cx = (PX(x1) + PX(x2)) / 2.0;
    CGFloat cy = (PY(y1) + PY(y2)) / 2.0;
    CGFloat rx = (x2 - x1) / 2.0;
    CGFloat ry = (y2 - y1) / 2.0;
    CGFloat start = PA2CG(sa);
    CGFloat end   = PA2CG(ea);
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, cx, cy);
    CGContextScaleCTM(ctx, rx, ry);
    CGContextBeginPath(ctx);
    CGContextAddArc(ctx, 0, 0, 1, start, end, 0);
    CGContextRestoreGState(ctx);
    CGContextStrokePath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGFloat cx = (PX(x1) + PX(x2)) / 2.0;
    CGFloat cy = (PY(y1) + PY(y2)) / 2.0;
    CGFloat rx = (x2 - x1) / 2.0;
    CGFloat ry = (y2 - y1) / 2.0;
    CGFloat start = PA2CG(sa);
    CGFloat end   = PA2CG(ea);
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, cx, cy);
    CGContextScaleCTM(ctx, rx, ry);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, 0, 0);
    CGContextAddArc(ctx, 0, 0, 1, start, end, 0);
    CGContextClosePath(ctx);
    CGContextRestoreGState(ctx);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGFloat cx = (PX(x1) + PX(x2)) / 2.0;
    CGFloat cy = (PY(y1) + PY(y2)) / 2.0;
    CGFloat rx = (x2 - x1) / 2.0;
    CGFloat ry = (y2 - y1) / 2.0;
    CGFloat start = PA2CG(sa);
    CGFloat end   = PA2CG(ea);
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, cx, cy);
    CGContextScaleCTM(ctx, rx, ry);
    CGContextBeginPath(ctx);
    CGContextAddArc(ctx, 0, 0, 1, start, end, 0);
    CGContextClosePath(ctx);
    CGContextRestoreGState(ctx);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx,    PX(x1), PY(y1));
    CGContextAddLineToPoint(ctx, PX(x2), PY(y2));
    CGContextAddLineToPoint(ctx, PX(x3), PY(y3));
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

void ami_setpixel(FILE* f, int x, int y)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextFillRect(ctx, CGRectMake(PX(x), PY(y), 1, 1));
    pa_cocoa_flush(win->han);
}

void ami_fover(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fmod = mdnorm;
}

void ami_bover(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bmod = mdnorm;
}

void ami_finvis(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fmod = mdinvis;
}

void ami_binvis(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bmod = mdinvis;
}

void ami_fxor(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->fmod = mdxor; }
void ami_bxor(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->bmod = mdxor; }
void ami_fand(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->fmod = mdand; }
void ami_band(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->bmod = mdand; }
void ami_for(FILE* f)  { winptr win = f2win(f); if (win) curscn(win)->fmod = mdor;  }
void ami_bor(FILE* f)  { winptr win = f2win(f); if (win) curscn(win)->bmod = mdor;  }

int ami_chrsizx(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->charspace : 8;
}

int ami_chrsizy(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->linespace : 16;
}

int ami_fonts(FILE* f) { return fntcnt; }

/* Update charspace/linespace/maxx/maxy from the current font */
static void update_metrics(winptr win)
{
    scnptr sc = curscn(win);
    fontptr fp = sc->font ? sc->font : fntlst;
    if (!fp || !fp->ctfont) return;
    CTFontRef cf = fp->ctfont;
    /* linespace = requested font height (fontsz), matching Linux behavior
     * where linespace = gfhigh. This ensures chrsizy/fontsiz round-trip. */
    win->linespace = (int)(win->fontsz + 0.5);
    CGGlyph  glyph;
    UniChar  ch = 'M';
    CTFontGetGlyphsForCharacters(cf, &ch, &glyph, 1);
    CGSize   adv;
    CTFontGetAdvancesForGlyphs(cf, kCTFontOrientationDefault,
                               &glyph, &adv, 1);
    win->charspace = (int)(adv.width + 0.5);
    if (win->charspace <= 0) win->charspace = win->linespace / 2;
    win->maxx = win->maxxg / win->charspace;
    win->maxy = win->maxyg / win->linespace;
    if (win->maxx < 1) win->maxx = 1;
    if (win->maxy < 1) win->maxy = 1;
}

void ami_font(FILE* f, int fc)
{
    winptr win = f2win(f); if (!win) return;
    fontptr fp = fntlst;
    int i = 1;
    while (fp && i < fc) { fp = fp->next; i++; }
    if (fp) {
        curscn(win)->font = fp;
        win->cfont        = fp;
        /* rebuild at current window font size */
        if (fp->name && fp->size != win->fontsz) {
            if (fp->ctfont) CFRelease(fp->ctfont);
            fp->ctfont = make_ctfont(fp->name, win->fontsz,
                                     curscn(win)->bold, curscn(win)->italic);
            fp->size = win->fontsz;
        }
        update_metrics(win);
    }
}

void ami_fontnam(FILE* f, int fc, char* fns, int fnsl)
{
    fontptr fp = fntlst;
    int i = 1;
    while (fp && i < fc) { fp = fp->next; i++; }
    if (fp && fp->name) {
        strncpy(fns, fp->name, fnsl-1);
        fns[fnsl-1] = 0;
    } else if (fnsl > 0) fns[0] = 0;
}

void ami_fontsiz(FILE* f, int s)
{
    winptr win = f2win(f); if (!win) return;
    win->fontsz = (CGFloat)(s > 0 ? s : DEF_FONT_H);
    /* rebuild current font at new size */
    scnptr sc = curscn(win);
    if (sc->font && sc->font->name) {
        if (sc->font->ctfont) CFRelease(sc->font->ctfont);
        sc->font->ctfont = make_ctfont(sc->font->name, win->fontsz,
                                       sc->bold, sc->italic);
        sc->font->size   = win->fontsz;
        update_metrics(win);
    }
}

void ami_chrspcy(FILE* f, int s)  { /* stub */ }
void ami_chrspcx(FILE* f, int s)  { /* stub */ }

int ami_dpmx(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->dpmx : 3780;
}

int ami_dpmy(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->dpmy : 3780;
}

int ami_strsiz(FILE* f, const char* s)
{
    winptr win = f2win(f); if (!win || !s) return 0;
    scnptr sc  = curscn(win);
    return (int)measure_string(sc->font ? sc->font : win->cfont,
                               s, (int)strlen(s));
}

int ami_chrpos(FILE* f, const char* s, int p)
{
    winptr win = f2win(f); if (!win || !s || p <= 0) return 0;
    scnptr sc  = curscn(win);
    int len = (int)strlen(s);
    if (p > len) p = len;
    return (int)measure_string(sc->font ? sc->font : win->cfont, s, p);
}

void ami_writejust(FILE* f, const char* s, int n)  { /* stub */ }
int  ami_justpos(FILE* f, const char* s, int p, int n) { return ami_chrpos(f, s, p); }

void ami_condensed(FILE* f, int e)  { /* stub */ }
void ami_extended(FILE* f, int e)   { /* stub */ }
void ami_xlight(FILE* f, int e)     { /* stub */ }
void ami_light(FILE* f, int e)      { /* stub */ }
void ami_xbold(FILE* f, int e)      { /* stub */ }
void ami_hollow(FILE* f, int e)     { /* stub */ }
void ami_raised(FILE* f, int e)     { /* stub */ }
void ami_settabg(FILE* f, int t)    { /* stub */ }
void ami_restabg(FILE* f, int t)    { /* stub */ }

int ami_baseline(FILE* f)
{
    winptr win = f2win(f); if (!win) return 0;
    scnptr sc  = curscn(win);
    fontptr fp = sc->font ? sc->font : win->cfont;
    if (!fp || !fp->ctfont) return 0;
    return (int)CTFontGetAscent(fp->ctfont);
}

void ami_fcolorg(FILE* f, int r, int g, int b)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba c = { r/255.0, g/255.0, b/255.0, 1.0 };
    curscn(win)->fc = c;
}

void ami_fcolorc(FILE* f, int r, int g, int b) { ami_fcolorg(f, r, g, b); }

void ami_bcolorg(FILE* f, int r, int g, int b)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba c = { r/255.0, g/255.0, b/255.0, 1.0 };
    curscn(win)->bc = c;
}

void ami_bcolorc(FILE* f, int r, int g, int b) { ami_bcolorg(f, r, g, b); }

void ami_loadpict(FILE* f, int p, char* fn)      { /* stub */ }
int  ami_pictsizx(FILE* f, int p)                { return 0; }
int  ami_pictsizy(FILE* f, int p)                { return 0; }
void ami_picture(FILE* f, int p, int x1, int y1, int x2, int y2) { /* stub */ }
void ami_delpict(FILE* f, int p)                 { /* stub */ }
void ami_scrollg(FILE* f, int x, int y)          { /* stub */ }
void ami_path(FILE* f, int a)                    { /* stub */ }

/*******************************************************************************
*                                                                              *
*                        PA API — window management                            *
*                                                                              *
*******************************************************************************/

void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)
{
    if (!inited) pa_graphics_init();

    /* Open /dev/null to obtain real file descriptors.  The fd numbers index
     * into opnfil[]/wintbl[], so iwrite() can route writes to plcchr(). */
    FILE* inf  = fopen("/dev/null", "r");
    FILE* outf = fopen("/dev/null", "w");
    if (!inf || !outf) { if (inf) fclose(inf); if (outf) fclose(outf); return; }
    setvbuf(inf,  NULL, _IONBF, 0);
    setvbuf(outf, NULL, _IONBF, 0);

    int ifn = fileno(inf);
    int ofn = fileno(outf);
    if (ifn < 0 || ofn < 0 || ofn >= MAXFIL) {
        fclose(inf); fclose(outf); return;
    }

    /* create the Cocoa window */
    pa_winhan han = pa_cocoa_create_window(100, 100, maxxd, maxyd, "");
    if (!han) { fclose(inf); fclose(outf); return; }

    winptr win = &wintbl[ofn];
    win_init(win, wid, parent ? fileno(parent) : 0, maxxd, maxyd);
    win->han     = han;
    win->infile  = inf;
    win->outfile = outf;
    opnfil[ofn]  = 1;

    pa_cocoa_show_window(han);
    clear_window(win);
    pa_cocoa_flush(han);

    *infile  = inf;
    *outfile = outf;
}

void ami_buffer(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    win->bufmod = e;
}

void ami_getsiz(FILE* f, int* x, int* y)
{
    winptr win = f2win(f);
    if (win) { *x = win->maxx; *y = win->maxy; }
    else     { *x = 80;        *y = 25; }
}

void ami_getsizg(FILE* f, int* x, int* y)
{
    winptr win = f2win(f);
    if (win) { *x = win->maxxg; *y = win->maxyg; }
    else     { *x = maxxd;    *y = maxyd; }
}

void ami_setsiz(FILE* f, int x, int y)
{
    ami_setsizg(f, x * (f2win(f) ? f2win(f)->charspace : 8),
                   y * (f2win(f) ? f2win(f)->linespace  : 16));
}

void ami_setsizg(FILE* f, int x, int y)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_resize_window(win->han, x, y);
    win->maxxg = x;
    win->maxyg = y;
    win->maxx  = x / win->charspace;
    win->maxy  = y / win->linespace;
}

void ami_setpos(FILE* f, int x, int y)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_move_window(win->han, x, y);
}

void ami_setposg(FILE* f, int x, int y) { ami_setpos(f, x, y); }

void ami_scnsiz(FILE* f, int* x, int* y)
{
    winptr win = f2win(f);
    int sw = pa_cocoa_screen_w();
    int sh = pa_cocoa_screen_h();
    *x = win ? sw / win->charspace : 80;
    *y = win ? sh / win->linespace : 25;
}

void ami_scnsizg(FILE* f, int* x, int* y)
{
    *x = pa_cocoa_screen_w();
    *y = pa_cocoa_screen_h();
}

void ami_scncen(FILE* f, int* x, int* y)
{
    winptr win = f2win(f);
    int sw = pa_cocoa_screen_w();
    int sh = pa_cocoa_screen_h();
    *x = win ? sw / win->charspace / 2 : 40;
    *y = win ? sh / win->linespace / 2 : 12;
}

void ami_scnceng(FILE* f, int* x, int* y)
{
    *x = pa_cocoa_screen_w() / 2;
    *y = pa_cocoa_screen_h() / 2;
}

void ami_winclient(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)
{
    *wx = cx; *wy = cy; /* stub — no chrome adjustment yet */
}

void ami_winclientg(FILE* f, int cx, int cy, int* wx, int* wy, ami_winmodset ms)
{
    *wx = cx; *wy = cy;
}

void ami_front(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_front(win->han);
}

void ami_back(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_back(win->han);
}

void ami_frame(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    win->frame = e;
    pa_cocoa_set_frame(win->han, e);
}

void ami_sizable(FILE* f, int e)
{
    winptr win = f2win(f); if (!win) return;
    win->sizable = e;
    pa_cocoa_set_sizable(win->han, e);
}

void ami_sysbar(FILE* f, int e)       { /* stub */ }
void ami_menu(FILE* f, ami_menuptr m) { /* stub */ }
void ami_menuena(FILE* f, int id, int onoff) { /* stub */ }
void ami_menusel(FILE* f, int id, int select) { /* stub */ }
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm) { /* stub */ }

int ami_getwinid(void)
{
    static int next = 1;
    return next++;
}

void ami_focus(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_focus(win->han);
}

/* Return the window handle for stdout (used by screen_capture) */
pa_winhan pa_stdout_winhan(void)
{
    if (opnfil[1] && wintbl[1].han) return wintbl[1].han;
    return NULL;
}

/*******************************************************************************
*                                                                              *
*                        PA API — event handling                               *
*                                                                              *
*******************************************************************************/

void ami_event(FILE* f, ami_evtrec* er)
{
    pa_rawevent raw;
    pa_cocoa_process_ns_events();
    pa_cocoa_wait(&raw);
    translate_event(&raw, er);
    if (er->etype == ami_etterm) fend = TRUE;
}

void ami_timer(FILE* f, int i, long t, int r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_set_timer(win->han, i-1, t, r); /* PA timers 1-based */
}

void ami_killtimer(FILE* f, int i)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_kill_timer(win->han, i-1);
}

int ami_mouse(FILE* f)        { return 1; }  /* one mouse */
int ami_mousebutton(FILE* f, int m) { return 3; }  /* three buttons */
int ami_joystick(FILE* f)     { return 0; }
int ami_joybutton(FILE* f, int j) { return 0; }
int ami_joyaxis(FILE* f, int j)   { return 0; }

/*******************************************************************************
*                                                                              *
*                        PA API — widgets                                      *
*                                                                              *
*******************************************************************************/

int ami_getwigid(FILE* f)
{
    static int next = 1;
    return next++;
}

void ami_killwidget(FILE* f, int id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_kill_widget(win->han, id);
}

void ami_selectwidget(FILE* f, int id, int e)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_select(win->han, id, e);
}

void ami_enablewidget(FILE* f, int id, int e)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_enable(win->han, id, e);
}

void ami_getwidgettext(FILE* f, int id, char* s, int sl)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_get_text(win->han, id, s, sl);
}

void ami_putwidgettext(FILE* f, int id, char* s)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_widget_text(win->han, id, s);
}

void ami_sizwidget(FILE* f, int id, int x, int y)   { /* stub */ }
void ami_sizwidgetg(FILE* f, int id, int x, int y)  { /* stub */ }
void ami_poswidget(FILE* f, int id, int x, int y)   { /* stub */ }
void ami_poswidgetg(FILE* f, int id, int x, int y)  { /* stub */ }
void ami_backwidget(FILE* f, int id)                { /* stub */ }
void ami_frontwidget(FILE* f, int id)               { /* stub */ }
void ami_focuswidget(FILE* f, int id)               { /* stub */ }

void ami_buttonsiz(FILE* f, char* s, int* w, int* h)
{
    *w = ami_strsiz(f, s) + 20;
    *h = ami_chrsizy(f) + 8;
}

void ami_buttonsizg(FILE* f, char* s, int* w, int* h) { ami_buttonsiz(f, s, w, h); }

void ami_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_button(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

void ami_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
{
    ami_button(f, x1, y1, x2, y2, s, id);
}

void ami_checkboxsiz(FILE* f, char* s, int* w, int* h)
{
    *w = ami_strsiz(f, s) + 24;
    *h = ami_chrsizy(f) + 4;
}

void ami_checkboxsizg(FILE* f, char* s, int* w, int* h) { ami_checkboxsiz(f, s, w, h); }

void ami_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_checkbox(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

void ami_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
{
    ami_checkbox(f, x1, y1, x2, y2, s, id);
}

void ami_radiobuttonsiz(FILE* f, char* s, int* w, int* h)
{
    *w = ami_strsiz(f, s) + 24;
    *h = ami_chrsizy(f) + 4;
}

void ami_radiobuttonsizg(FILE* f, char* s, int* w, int* h) { ami_radiobuttonsiz(f, s, w, h); }

void ami_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_radiobutton(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

void ami_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)
{
    ami_radiobutton(f, x1, y1, x2, y2, s, id);
}

void ami_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy)
{ *w = cw + 10; *h = ch + 20; *ox = 5; *oy = 15; }

void ami_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h, int* ox, int* oy)
{ ami_groupsizg(f, s, cw, ch, w, h, ox, oy); }

void ami_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)   { /* stub */ }
void ami_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)  { /* stub */ }
void ami_background(FILE* f, int x1, int y1, int x2, int y2, int id)       { /* stub */ }
void ami_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)      { /* stub */ }

void ami_scrollvertsizg(FILE* f, int* w, int* h)  { *w = 16; *h = 100; }
void ami_scrollvertsiz(FILE* f, int* w, int* h)   { ami_scrollvertsizg(f, w, h); }

void ami_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollvert(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)
{ ami_scrollvert(f, x1, y1, x2, y2, id); }

void ami_scrollhorizsizg(FILE* f, int* w, int* h)  { *w = 100; *h = 16; }
void ami_scrollhorizsiz(FILE* f, int* w, int* h)   { ami_scrollhorizsizg(f, w, h); }

void ami_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollhoriz(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)
{ ami_scrollhoriz(f, x1, y1, x2, y2, id); }

void ami_scrollpos(FILE* f, int id, int r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollbar_pos(win->han, id, r);
}

void ami_scrollsiz(FILE* f, int id, int r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollbar_siz(win->han, id, r);
}

void ami_numselboxsizg(FILE* f, int l, int u, int* w, int* h) { *w = 80; *h = 24; }
void ami_numselboxsiz(FILE* f, int l, int u, int* w, int* h)  { ami_numselboxsizg(f, l, u, w, h); }
void ami_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id) { /* stub */ }
void ami_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id) { /* stub */ }

void ami_editboxsizg(FILE* f, char* s, int* w, int* h)
{ *w = ami_strsiz(f, s) + 20; *h = ami_chrsizy(f) + 8; }

void ami_editboxsiz(FILE* f, char* s, int* w, int* h) { ami_editboxsizg(f, s, w, h); }

void ami_editbox(FILE* f, int x1, int y1, int x2, int y2, int id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_editbox(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id)
{ ami_editbox(f, x1, y1, x2, y2, id); }

void ami_progbarsizg(FILE* f, int* w, int* h) { *w = 200; *h = 20; }
void ami_progbarsiz(FILE* f, int* w, int* h)  { ami_progbarsizg(f, w, h); }

void ami_progbar(FILE* f, int x1, int y1, int x2, int y2, int id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_progressbar(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

void ami_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)
{ ami_progbar(f, x1, y1, x2, y2, id); }

void ami_progbarpos(FILE* f, int id, int pos)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_progressbar_pos(win->han, id, pos);
}

void ami_listboxsizg(FILE* f, ami_strptr sp, int* w, int* h) { *w = 150; *h = 100; }
void ami_listboxsiz(FILE* f, ami_strptr sp, int* w, int* h)  { ami_listboxsizg(f, sp, w, h); }
void ami_listbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)   { /* stub */ }
void ami_listboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)  { /* stub */ }

void ami_dropboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
{ *cw = 150; *ch = 24; *ow = 150; *oh = 100; }

void ami_dropboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
{ ami_dropboxsizg(f, sp, cw, ch, ow, oh); }

void ami_dropbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)   { /* stub */ }
void ami_dropboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)  { /* stub */ }

void ami_dropeditboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
{ ami_dropboxsizg(f, sp, cw, ch, ow, oh); }

void ami_dropeditboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)
{ ami_dropeditboxsizg(f, sp, cw, ch, ow, oh); }

void ami_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id) { /* stub */ }
void ami_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id) { /* stub */ }

void ami_slidehorizsizg(FILE* f, int* w, int* h) { *w = 150; *h = 20; }
void ami_slidehorizsiz(FILE* f, int* w, int* h)  { ami_slidehorizsizg(f, w, h); }

void ami_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_slider_horiz(win->han, x1, y1, x2-x1+1, y2-y1+1, mark, id);
}

void ami_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
{ ami_slidehoriz(f, x1, y1, x2, y2, mark, id); }

void ami_slidevertsizg(FILE* f, int* w, int* h) { *w = 20; *h = 150; }
void ami_slidevertsiz(FILE* f, int* w, int* h)  { ami_slidevertsizg(f, w, h); }

void ami_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_slider_vert(win->han, x1, y1, x2-x1+1, y2-y1+1, mark, id);
}

void ami_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)
{ ami_slidevert(f, x1, y1, x2, y2, mark, id); }

void ami_tabbarsizg(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy)
{ *w = cw + 10; *h = ch + 30; *ox = 5; *oy = 25; }

void ami_tabbarsiz(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h, int* ox, int* oy)
{ ami_tabbarsizg(f, tor, cw, ch, w, h, ox, oy); }

void ami_tabbarclientg(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy)
{ *cw = w - 10; *ch = h - 30; *ox = 5; *oy = 25; }

void ami_tabbarclient(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch, int* ox, int* oy)
{ ami_tabbarclientg(f, tor, w, h, cw, ch, ox, oy); }

void ami_tabbar(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, ami_tabori tor, int id)  { /* stub */ }
void ami_tabbarg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, ami_tabori tor, int id) { /* stub */ }
void ami_tabsel(FILE* f, int id, int tn) { /* stub */ }

/*******************************************************************************
*                                                                              *
*                        PA API — dialogs                                      *
*                                                                              *
*******************************************************************************/

void ami_alert(char* title, char* message)
{
    pa_cocoa_alert(title, message);
}

void ami_querycolor(int* r, int* g, int* b)
{
    /* stub — NSColorPanel could be added */
    *r = *g = *b = 0;
}

void ami_queryopen(char* s, int sl)  { pa_cocoa_query_open(s, sl); }
void ami_querysave(char* s, int sl)  { pa_cocoa_query_save(s, sl); }

void ami_queryfind(char* s, int sl, ami_qfnopts* opt)         { if (sl>0) s[0]=0; }
void ami_queryfindrep(char* s, int sl, char* r, int rl,
                      ami_qfropts* opt)                       { if (sl>0) s[0]=0; if (rl>0) r[0]=0; }
void ami_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg,
                   int* fb, int* br, int* bg, int* bb,
                   ami_qfteffects* effect)                    { /* stub */ }
