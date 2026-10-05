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
#include <limits.h>
#include <pthread.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>

#include <localdefs.h>
#include <config.h>
#include <graphics.h>
#include "pa_cocoa.h"

/* libc/stdio.c vector override types — same ABI as the internal vectors */
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef ssize_t (*pread_t)(int, void*, size_t);
typedef int     (*pclose_t)(int);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_read(pread_t  nfp, pread_t*  ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);

/*******************************************************************************
*                                                                              *
*                              Constants                                       *
*                                                                              *
*******************************************************************************/

#define MAXFIL      100     /* maximum open file/window slots */
#define MAXPIC      50      /* maximum loadable pictures */
#define MAXTIM      10      /* maximum timers per window */
#define FRMTIM      (MAXTIM-1) /* reserved slot for frame timer (0-based) */
#define MAXCON      10      /* maximum screen contexts per window */
#define MAXXD       80      /* default terminal width in chars */
#define MAXYD       25      /* default terminal height in chars */
#define DEF_FONT_H  16      /* default font height in points */

/* PA angles: LONG_MAX = 360 degrees */
#define ANG2RAD(a)  ((a) * (2.0 * M_PI) / (double)LONG_MAX)

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
    ami_long    curx,  cury;   /* text cursor (1-based char) */
    ami_long    curxg, curyg;  /* graphics cursor (1-based pixel) */
    int         curv;          /* cursor visible */
    pa_rgba     fc, bc;        /* foreground / background color */
    drawmode    fmod, bmod;    /* draw modes */
    CGFloat     lwidth;        /* line width */
    fontptr     font;          /* current font */
    int         bold, italic, underline, strikeout;
    int         superscript, subscript;
    int         condensed, extended;
    int         light, xlight, xbold;
    int         hollow, raised;
    int         reverse;
    int         autof;         /* auto scroll/wrap mode */
    ami_long    textpath;      /* text angle (PA units: LONG_MAX=360°) */
    ami_long    offx, offy;    /* viewport offset (pixels) */
    float       scalex, scaley;/* viewport scale */
} scncon, *scnptr;

/* Per-window record */
typedef struct winrec {
    pa_winhan   han;            /* Cocoa window handle */
    FILE*       infile;         /* associated input  stream */
    FILE*       outfile;        /* associated output stream */
    int         wid;            /* window id */
    int         parwid;         /* parent window id */

    /* dimensions */
    ami_long    maxxg, maxyg;  /* graphical size in pixels */
    ami_long    maxx,  maxy;   /* text size in chars */
    ami_long    charspace;     /* pixels per char column */
    ami_long    linespace;     /* pixels per char row */
    ami_long    dpmx, dpmy;    /* dots per meter x/y */

    /* screens */
    scncon      screens[MAXCON];
    int         curdsp;        /* current display screen (1-based) */
    int         curupd;        /* current update screen  (1-based) */

    /* state */
    int         visible;
    int         frame, sizable, sysbar;
    int         bufmod;        /* double-buffer on */
    int         gauto;         /* window-level auto scroll/wrap state; new or
                                  reselected screens inherit it (matches the
                                  reference: ami_auto is a window property, not
                                  a per-screen one) */
    int         focus;
    int         fautohold;     /* auto hold on exit */
    int         frmrun;        /* frame timer running */

    /* font */
    fontptr     cfont;         /* current font */
    CTFontRef   ctfont;        /* this window's font at its size and style;
                                  NULL until built, when the record's default
                                  size face serves (see winfont) */
    CGFloat     fontsz;        /* font size in pixels (CTFont logical size) */
    int         cellh;         /* the character cell height the fonts are
                                  fitted to; 0 until the first font sets it */
    float       gfpoint;       /* current font size in typographic points */

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
static pclose_t ofpclose;              /* saved close vector */
static int      maxxd;                 /* default window width in pixels */
static int      maxyd;                 /* default window height in pixels */

/* config settable runtime options */
static int      cfgmaxxd;             /* configured terminal width in chars */
static int      cfgmaxyd;             /* configured terminal height in chars */
static int      dialogerr;            /* send runtime errors to dialog */
static int      mouseenb = 1;         /* enable mouse */
static int      joyenb;               /* enable joysticks */
static int      dmpevt;               /* enable dump Petit-Ami events */
static int      dmpmsg;               /* enable dump messages (diagnostic) */
static int      prtftm;               /* print font metrics (diagnostic) */
static int      conpnt;               /* size of console font in points */

/* forward declarations */
static void    clear_window(winptr win);
static void    plcchr(winptr win, char c);
static ssize_t iwrite(int fd, const void* buff, size_t count);
static ssize_t iread(int fd, void* buff, size_t count);
static int     iclose(int fd);
static void    update_metrics(winptr win);
static int     glyphspan(CTFontRef f);
static void    fontcell(CTFontRef f, int* cs, int* ls);
static void    buildfont(winptr win, const char* name, int bold, int italic);


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

/* Copy string to critical buffer. Critical string buffers follow the package
   convention: a result that fills the entire buffer is left without a
   terminating zero, a shorter result is zero terminated, and it is an error
   if the result cannot fit in the buffer. */
static void cpycrit(char* d, ami_long dl, const char* s)
{
    ami_long l = strlen(s);
    if (l > dl) {
        fprintf(stderr, "\nError: Graphics: String too large for buffer\n");
        exit(1);
    }
    memcpy(d, s, l);
    if (l < dl) d[l] = 0;
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

/* Rebuild the current font's CTFont based on screen context effects.
 * Called when bold, italic, condensed, extended, etc. change. */
/* The window's Core Text font. Each window builds its own from the current
   font record at its own size and style; the record keeps only the face and
   a default size fallback, so one window's size never reaches another (a
   server's next session opens fresh windows, at the default). */
static CTFontRef winfont(winptr win)
{
    scnptr  sc;
    fontptr fp;
    if (win->ctfont) return win->ctfont;
    sc = curscn(win);
    fp = sc->font ? sc->font : fntlst;
    return fp ? fp->ctfont : NULL;
}

static void rebuild_font(winptr win)
{
    scnptr sc = curscn(win);
    fontptr fp = sc->font ? sc->font : fntlst;
    if (!fp || !fp->name) return;

    int b = sc->bold || sc->xbold;
    int it = sc->italic;

    buildfont(win, fp->name, b, it);

    /* apply condensed/extended via a matrix transform */
    if (win->ctfont && (sc->condensed || sc->extended)) {
        CGFloat xscale = sc->condensed ? 0.8 : 1.25;
        CGAffineTransform matrix = CGAffineTransformMakeScale(xscale, 1.0);
        CTFontRef transformed = CTFontCreateCopyWithAttributes(win->ctfont,
                                    win->fontsz, &matrix, NULL);
        if (transformed) {
            CFRelease(win->ctfont);
            win->ctfont = transformed;
        }
    }

    update_metrics(win);
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
        /* for TERM slot, try system monospace font (SF Mono) first */
        if (slot == 0) {
            CTFontRef f = pa_cocoa_system_mono_font((CGFloat)DEF_FONT_H);
            if (f) {
                CFStringRef fname = CTFontCopyFullName(f);
                char nbuf[256];
                CFStringGetCString(fname, nbuf, sizeof(nbuf),
                                   kCFStringEncodingUTF8);
                CFRelease(fname);
                fp->ctfont = f;
                fp->name   = strdup(nbuf);
                fp->size   = DEF_FONT_H;
                fp->fixed  = 1;
            }
        }
        /* fall through to name list if system mono not available */
        if (!fp->ctfont) {
            for (const char** np = lists[slot]; *np; np++) {
                CTFontRef f = make_ctfont(*np, DEF_FONT_H, 0, 0);
                if (f) {
                    fp->ctfont = f;
                    fp->name   = strdup(*np);
                    fp->size   = DEF_FONT_H;
                    fp->fixed  = (slot == 0);
                    break;
                }
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

    /* enumerate all installed system fonts after the 4 standard ones */
    CTFontCollectionRef collection = CTFontCollectionCreateFromAvailableFonts(NULL);
    if (collection) {
        CFArrayRef descs = CTFontCollectionCreateMatchingFontDescriptors(collection);
        if (descs) {
            CFIndex n = CFArrayGetCount(descs);
            /* find the tail of the font list */
            fontptr tail = fntlst;
            while (tail && tail->next) tail = tail->next;

            for (CFIndex i = 0; i < n; i++) {
                CTFontDescriptorRef desc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descs, i);
                CFStringRef cfname = CTFontDescriptorCopyAttribute(desc, kCTFontDisplayNameAttribute);
                if (!cfname) continue;

                char namebuf[256];
                if (!CFStringGetCString(cfname, namebuf, sizeof(namebuf), kCFStringEncodingUTF8)) {
                    CFRelease(cfname);
                    continue;
                }
                CFRelease(cfname);

                /* skip duplicates of the 4 standard fonts */
                int dup = 0;
                fontptr chk = fntlst;
                while (chk) {
                    if (chk->name && strcmp(chk->name, namebuf) == 0) { dup = 1; break; }
                    chk = chk->next;
                }
                if (dup) continue;

                CTFontRef f = make_ctfont(namebuf, DEF_FONT_H, 0, 0);
                if (!f) continue;

                fontptr fp = calloc(1, sizeof(fontrec));
                fp->ctfont = f;
                fp->name   = strdup(namebuf);
                fp->size   = DEF_FONT_H;
                fp->fixed  = (CTFontGetSymbolicTraits(f) & kCTFontTraitMonoSpace) != 0;
                fp->next   = NULL;

                if (tail) { tail->next = fp; tail = fp; }
                else      { fntlst = fp; tail = fp; }
                fntcnt++;
            }
            CFRelease(descs);
        }
        CFRelease(collection);
    }
}

/* Measure a string with current font; returns width in pixels */
static CGFloat measure_string(winptr win, const char* s, int len)
{
    CTFontRef cf = winfont(win);
    if (!cf || !s || len <= 0) return 0;
    CFStringRef  cfs = CFStringCreateWithBytes(NULL, (const UInt8*)s, len,
                                               kCFStringEncodingUTF8, false);
    CGFloat w = 0;
    if (cfs) {
        CFStringRef keys[1]   = { kCTFontAttributeName };
        CFTypeRef   values[1] = { cf };
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

/* Draw a string at (x, y) — top-left based, PA 1-based coords.
 * Handles text effects: bold, italic (via font), underline, strikeout,
 * superscript, subscript, reverse, hollow, raised, light. */
static void draw_string(CGContextRef ctx, winptr win, scnptr sc,
                        ami_long x, ami_long y, const char* s, int len)
{
    CTFontRef cf = winfont(win);
    if (!ctx || !cf || !s || len <= 0) return;

    /* for superscript/subscript, use a smaller font */
    CTFontRef drawFont = cf;
    int releaseDraw = 0;
    CGFloat yoff = 0;
    if (sc->superscript || sc->subscript) {
        CGFloat smallSize = win->fontsz * 0.6;
        drawFont = CTFontCreateCopyWithAttributes(cf, smallSize, NULL, NULL);
        releaseDraw = 1;
        if (sc->superscript) yoff = 0; /* top of cell */
        else yoff = win->fontsz * 0.4; /* lower in cell */
    }

    CFStringRef cfs = CFStringCreateWithBytes(NULL, (const UInt8*)s, len,
                                              kCFStringEncodingUTF8, false);
    if (!cfs) { if (releaseDraw) CFRelease(drawFont); return; }

    /* determine colors: swap fg/bg for reverse mode */
    pa_rgba fgc = sc->fc, bgc = sc->bc;
    if (sc->reverse) { pa_rgba t = fgc; fgc = bgc; bgc = t; }

    /* light/xlight: reduce alpha */
    CGFloat alpha = 1.0;
    if (sc->xlight) alpha = 0.3;
    else if (sc->light) alpha = 0.6;

    CGColorRef color = CGColorCreateGenericRGB(fgc.r, fgc.g, fgc.b, alpha);

    CFStringRef keys[2]   = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef   values[2] = { drawFont, color };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL,
                               (const void**)keys, (const void**)values,
                               2, &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
    CGColorRelease(color);

    CFAttributedStringRef as   = CFAttributedStringCreate(NULL, cfs, attrs);
    CTLineRef             line = CTLineCreateWithAttributedString(as);

    CGFloat ascent, descent;
    CGFloat width = CTLineGetTypographicBounds(line, &ascent, &descent, NULL);

    CGFloat tx = PX(x);
    CGFloat ty = PY(y) + ascent + yoff;

    /* draw text. The context has antialiasing off, for the lines and
       figures, which are drawn to the pixel; glyphs are drawn smoothed, as
       the system draws its own, else they come out ragged, each stem
       snapped to a pixel at the font's size. */
    CGContextSaveGState(ctx);
    CGContextSetAllowsAntialiasing(ctx, true);
    CGContextSetShouldAntialias(ctx, true);
    CGContextSetAllowsFontSmoothing(ctx, true);
    CGContextSetShouldSmoothFonts(ctx, true);
    CGContextSetAllowsFontSubpixelPositioning(ctx, true);
    CGContextSetShouldSubpixelPositionFonts(ctx, true);
    CGContextSetAllowsFontSubpixelQuantization(ctx, false);
    CGContextSetShouldSubpixelQuantizeFonts(ctx, false);
    CGContextTranslateCTM(ctx, tx, ty);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    if (sc->textpath != LONG_MAX / 4) {
        CGFloat rot = -(sc->textpath - LONG_MAX / 4) * (2.0 * M_PI) / (double)LONG_MAX;
        CGContextRotateCTM(ctx, rot);
    }
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetTextPosition(ctx, 0, 0);

    if (sc->hollow) {
        /* hollow: stroke the text outlines instead of filling */
        CGContextSetTextDrawingMode(ctx, kCGTextStroke);
        CGContextSetRGBStrokeColor(ctx, fgc.r, fgc.g, fgc.b, alpha);
        CGContextSetLineWidth(ctx, 0.5);
    } else if (sc->raised) {
        /* raised: draw with a slight shadow/offset for embossed look */
        CGContextSetShadow(ctx, CGSizeMake(1, -1), 1.0);
    }

    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);

    /* underline: draw a line below the baseline */
    if (sc->underline) {
        CGFloat uy = PY(y) + ascent + descent * 0.5;
        CGContextSetRGBStrokeColor(ctx, fgc.r, fgc.g, fgc.b, alpha);
        CGContextSetLineWidth(ctx, 1.0);
        CGContextMoveToPoint(ctx, tx, uy);
        CGContextAddLineToPoint(ctx, tx + width, uy);
        CGContextStrokePath(ctx);
    }

    /* strikeout: draw a line through the middle of the text */
    if (sc->strikeout) {
        CGFloat sy = PY(y) + ascent * 0.6;
        CGContextSetRGBStrokeColor(ctx, fgc.r, fgc.g, fgc.b, alpha);
        CGContextSetLineWidth(ctx, 1.0);
        CGContextMoveToPoint(ctx, tx, sy);
        CGContextAddLineToPoint(ctx, tx + width, sy);
        CGContextStrokePath(ctx);
    }

    CFRelease(line);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(cfs);
    if (releaseDraw) CFRelease(drawFont);
}

/*******************************************************************************
*                                                                              *
*                        Window init / deinit                                  *
*                                                                              *
*******************************************************************************/
/* A font's glyph span: its ascent and descent, each to the pixel below.
   The character cell is that and two for the gap, as the Linux backend
   has it from FreeType. The cell was the font size asked for, with the
   font built at that size, and a font's glyphs run taller than its size
   (Menlo at 16 spans 18.6), so rows were cut short: descenders ran into
   the row below, and a row's background fill, the size of the cell,
   covered less than the glyphs and more than the row it was meant to, as
   the last row of a buffer did a bar drawn along the buffer's bottom
   edge. */
static int glyphspan(CTFontRef f)
{
    return ((int)CTFontGetAscent(f) + (int)CTFontGetDescent(f));
}

/* The character cell of a font: the row is the glyph span and the gap,
   the column the advance of 'M', standing in for the widest. The one
   measure, taken here for the window's size at start and for each window
   as it opens: the two had their own formulas, and when they differed the
   window held fewer rows than the terminal's height. */
static void fontcell(CTFontRef f, int* cs, int* ls)
{
    CGGlyph glyph;
    UniChar ch = 'M';
    CGSize  adv;

    *ls = glyphspan(f) + 2;
    CTFontGetGlyphsForCharacters(f, &ch, &glyph, 1);
    CTFontGetAdvancesForGlyphs(f, kCTFontOrientationDefault, &glyph, &adv, 1);
    *cs = (int)(adv.width + 0.5);
    if (*cs <= 0) *cs = *ls / 2;
}

/* Build the window's font to its cell. The cell, once set, is kept over
   font changes, and each face is built at the largest size whose glyphs
   fit the cell less the gap, so that every face shares the row height,
   as on Linux. With no cell set yet, the size asked for builds the font
   and the font's span sets the cell. */
static void buildfont(winptr win, const char* name, int bold, int italic)
{
    CGFloat   em;
    int       target;
    CTFontRef t;

    if (win->ctfont) { CFRelease(win->ctfont); win->ctfont = NULL; }
    if (win->cellh <= 0) {
        win->ctfont = make_ctfont(name, win->fontsz, bold, italic);
        if (win->ctfont) win->cellh = glyphspan(win->ctfont) + 2;
        return;
    }
    target = win->cellh - 2;
    if (target < 1) target = 1;
    em = target;
    win->ctfont = make_ctfont(name, em, bold, italic);
    if (!win->ctfont) return;
    while (em > 1 && glyphspan(win->ctfont) > target) { /* too tall: smaller */
        em--;
        CFRelease(win->ctfont);
        win->ctfont = make_ctfont(name, em, bold, italic);
        if (!win->ctfont) return;
    }
    for (;;) { /* room to spare: larger, while it still fits */
        t = make_ctfont(name, em + 1, bold, italic);
        if (!t || glyphspan(t) > target) { if (t) CFRelease(t); break; }
        CFRelease(win->ctfont);
        win->ctfont = t;
        em++;
    }
    win->fontsz = em;
}


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
    if (win->ctfont) { CFRelease(win->ctfont); win->ctfont = NULL; } /* a reused slot */
    win->fontsz  = DEF_FONT_H;

    /* screen size in mm from shim */
    int smm_w  = pa_cocoa_screen_wmm();
    int smm_h  = pa_cocoa_screen_hmm();
    int spx_w  = pa_cocoa_screen_w();
    int spx_h  = pa_cocoa_screen_h();
    win->dpmx  = (smm_w > 0) ? spx_w * 1000 / smm_w : 3780; /* ~96 dpi */
    win->dpmy  = (smm_h > 0) ? spx_h * 1000 / smm_h : 3780;
    win->gfpoint = win->fontsz * 2835.0f / (float)win->dpmy;

    /* character cell size from font metrics */
    CTFontRef f = fntlst ? fntlst->ctfont : NULL;
    if (f) {
        int cs, ls;
        fontcell(f, &cs, &ls);
        win->charspace = cs;
        win->linespace = ls;
        win->cellh = ls;
    } else {
        win->linespace = DEF_FONT_H + 2;
        win->charspace = (DEF_FONT_H + 2) / 2;
    }
    win->maxx = w / win->charspace;
    win->maxy = h / win->linespace;
    if (win->maxx < 1) win->maxx = 1;
    if (win->maxy < 1) win->maxy = 1;

    win->gauto = TRUE; /* window-level auto default; screens inherit below */

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
        sc->autof   = win->gauto;
        sc->textpath = LONG_MAX / 4; /* default: east (normal reading) */
        sc->offx    = 0;
        sc->offy    = 0;
        sc->scalex  = 1.0f;
        sc->scaley  = 1.0f;
    }
}

/*******************************************************************************
*                                                                              *
*                        Library init (constructor)                            *
*                                                                              *
*******************************************************************************/

__attribute__((constructor (102)))
static void pa_graphics_init(void)
{
    if (inited) return;
    inited = TRUE;

    pa_cocoa_init();
    init_fonts();
    memset(opnfil, 0, sizeof(opnfil));

    fautohold = TRUE; /* hold window open on exit until keypress */

    /* read configuration file */
    {
        ami_valptr config_root = NULL;
        ami_valptr term_root;
        ami_valptr graph_root;
        ami_valptr diag_root;
        ami_valptr mac_root;
        ami_valptr vp;
        char*      errstr;

        ami_config(&config_root);

        /* find "terminal" block */
        term_root = ami_schlst("terminal", config_root);
        if (term_root && term_root->sublist) term_root = term_root->sublist;

        if (term_root) {

            vp = ami_schlst("maxxd", term_root);
            if (vp) {

                cfgmaxxd = strtol(vp->value, &errstr, 10);
                if (*errstr) cfgmaxxd = 0;

            }
            vp = ami_schlst("maxyd", term_root);
            if (vp) {

                cfgmaxyd = strtol(vp->value, &errstr, 10);
                if (*errstr) cfgmaxyd = 0;

            }
            vp = ami_schlst("joystick", term_root);
            if (vp) {

                joyenb = strtol(vp->value, &errstr, 10);
                if (*errstr) joyenb = 0;

            }
            vp = ami_schlst("mouse", term_root);
            if (vp) {

                mouseenb = strtol(vp->value, &errstr, 10);
                if (*errstr) mouseenb = 1;

            }
            vp = ami_schlst("dump_event", term_root);
            if (vp) {

                dmpevt = strtol(vp->value, &errstr, 10);
                if (*errstr) dmpevt = 0;

            }

        }

        /* find "graphics" block */
        graph_root = ami_schlst("graphics", config_root);
        if (graph_root) {

            vp = ami_schlst("console_points", graph_root->sublist);
            if (vp) {

                conpnt = strtol(vp->value, &errstr, 10);
                if (*errstr) conpnt = 0;

            }

            vp = ami_schlst("dialogerr", graph_root->sublist);
            if (vp) {

                dialogerr = strtol(vp->value, &errstr, 10);
                if (*errstr) dialogerr = 0;

            }

            /* find macosx subsection */
            mac_root = ami_schlst("macosx", graph_root->sublist);
            if (mac_root) {

                /* find diagnostic subsection */
                diag_root = ami_schlst("diagnostics", mac_root->sublist);
                if (diag_root) {

                    vp = ami_schlst("dump_messages", diag_root->sublist);
                    if (vp) {

                        dmpmsg = strtol(vp->value, &errstr, 10);
                        if (*errstr) dmpmsg = 0;

                    }

                    vp = ami_schlst("print_font_metrics", diag_root->sublist);
                    if (vp) {

                        prtftm = strtol(vp->value, &errstr, 10);
                        if (*errstr) prtftm = 0;

                    }

                }

            }

        }
    }

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* hook libc/stdio.c write/read vectors */
    ovr_write(iwrite, &ofpwrite);
    ovr_read(iread,  &ofpread);
    ovr_close(iclose, &ofpclose);

    /* compute default window pixel size from TERM font character grid */
    {
        fontptr fp = fntlst;
        int termw = cfgmaxxd > 0 ? cfgmaxxd : MAXXD;
        int termh = cfgmaxyd > 0 ? cfgmaxyd : MAXYD;
        int fontsz = conpnt > 0 ? conpnt : DEF_FONT_H;

        if (conpnt > 0 && fp) {
            CTFontRef newfont = CTFontCreateCopyWithAttributes(fp->ctfont,
                                    (CGFloat)fontsz, NULL, NULL);
            if (newfont) {
                CTFontRef old = fp->ctfont;
                fp->ctfont = newfont;
                fp->size = fontsz;
                CFRelease(old);
            }
        }

        /* the terminal font's cell, then the window as the terminal's
           columns and rows of it */
        int cs, ls;
        fontcell(fp->ctfont, &cs, &ls);
        maxxd = termw * cs;
        maxyd = termh * ls;
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
        win->focus = TRUE;
        opnfil[1]  = 1; /* mark slot in use */
        pa_cocoa_set_bufmod(han, win->bufmod);
        pa_cocoa_set_background(han, 1.0f, 1.0f, 1.0f);
        clear_window(win);
        pa_cocoa_show_window(han);
        pa_cocoa_flush(han);
    }

    if (joyenb) pa_cocoa_joy_init();

    /* The main-thread capture (pa_cocoa_start_event_thread) is NOT called here.

       It never returns, and a constructor that never returns stalls dyld: no

       initializer linked after this backend (pdfgraph, the widget base, any

       layered module) would run. It is called from pa_entry, the program entry

       the linker is given for every Darwin graphics program, which dyld reaches

       only after every initializer has completed. */
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
        pclose_t cppclose;
        ovr_write(ofpwrite, &cppwrite);
        ovr_read(ofpread, &cppread);
        ovr_close(ofpclose, &cppclose);
    }

    if (joyenb) pa_cocoa_joy_deinit();
    pa_cocoa_deinit();
}

/*******************************************************************************
*                                                                              *
*                        Context helpers                                       *
*                                                                              *
*******************************************************************************/

/* Map PA drawing mode to CoreGraphics blend mode */
static CGBlendMode mode2blend(drawmode m)
{
    switch (m) {
    case mdnorm:   return kCGBlendModeNormal;
    case mdinvis:  return kCGBlendModeNormal; /* caller should skip draw */
    case mdxor:    return kCGBlendModeDifference;
    case mdand:    return kCGBlendModeMultiply;
    case mdor:     return kCGBlendModeScreen;
    default:       return kCGBlendModeNormal;
    }
}

/* Get the CGContext for a window and configure it for the current screen.
 * Returns NULL if the foreground mode is invisible (caller should skip). */
static CGContextRef get_ctx(winptr win)
{
    scnptr sc = curscn(win);
    if (sc->fmod == mdinvis) return NULL; /* invisible — skip drawing */
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return NULL;
    CGContextSetLineWidth(ctx, sc->lwidth);
    CGContextSetShouldAntialias(ctx, false); /* crisp lines */
    CGContextSetBlendMode(ctx, mode2blend(sc->fmod));
    set_fg(ctx, sc->fc);
    return ctx;
}

/* Apply viewport offset/scale transform to the bitmap context.
 * Resets the CTM to retina-scale-only, then applies offset + scale. */
static void settrans(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;
    scnptr sc = curscn(win);
    CGAffineTransform cur = CGContextGetCTM(ctx);
    CGAffineTransform inv = CGAffineTransformInvert(cur);
    CGContextConcatCTM(ctx, inv);
    size_t pw = CGBitmapContextGetWidth(ctx);
    CGFloat retina = (win->maxxg > 0) ? (CGFloat)pw / win->maxxg : 1.0;
    CGContextScaleCTM(ctx, retina, retina);
    if (sc->offx || sc->offy)
        CGContextTranslateCTM(ctx, (CGFloat)sc->offx, (CGFloat)sc->offy);
    if (sc->scalex != 1.0f || sc->scaley != 1.0f)
        CGContextScaleCTM(ctx, sc->scalex, sc->scaley);
}

/* Clear the window to background color */
static void clear_window(winptr win)
{
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;
    scnptr sc = curscn(win);
    CGContextSaveGState(ctx);
    CGAffineTransform cur = CGContextGetCTM(ctx);
    CGAffineTransform inv = CGAffineTransformInvert(cur);
    CGContextConcatCTM(ctx, inv);
    size_t pw = CGBitmapContextGetWidth(ctx);
    CGFloat retina = (win->maxxg > 0) ? (CGFloat)pw / win->maxxg : 1.0;
    CGContextScaleCTM(ctx, retina, retina);
    CGContextSetBlendMode(ctx, kCGBlendModeNormal);
    CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
    CGContextFillRect(ctx, CGRectMake(0, 0, win->maxxg, win->maxyg));
    CGContextRestoreGState(ctx);
}

/*******************************************************************************
*                                                                              *
*                   Character output / terminal emulation                      *
*                                                                              *
*******************************************************************************/

/* Scroll window content up by one text line.
 * Uses raw bitmap memmove: the bitmap context is CG y-up, so row 0 of the
 * data is the BOTTOM of the screen. Shifts physical rows toward higher
 * addresses (visually upward), then redraws the freed bottom strip with
 * the background color. */
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

    /* Move the content up one text line. The bitmap is CG y-up: data row 0
       is the BOTTOM of the screen (drawing flips logical coordinates via
       PY()), so shifting visual content up means moving data rows toward
       higher addresses. */
    memmove(data + physLS * rowbytes, data, (height - physLS) * rowbytes);

    /* fill the freed rows at the bottom with the background color using CG */
    scnptr sc = curscn(win);
    CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
    ami_long clearY = (win->maxy - 1) * win->linespace; /* top of last text row, 0-based */
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
        cw = (int)(measure_string(win, &c, 1) + 0.5);
        if (cw <= 0) cw = win->charspace;
    } else {
        cw = win->charspace;
    }

    ami_long px = sc->curxg - 1; /* 1-based to 0-based */
    ami_long py = sc->curyg - 1;

    /* fill cell background */
    if (sc->bmod != mdinvis) {
        CGContextSetBlendMode(ctx, mode2blend(sc->bmod));
        CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
        if (sc->textpath != LONG_MAX / 4) {
            CGFloat ascent = winfont(win) ? CTFontGetAscent(winfont(win)) : (CGFloat)win->linespace * 0.75;
            CGContextSaveGState(ctx);
            CGContextTranslateCTM(ctx, px, py + ascent);
            CGContextScaleCTM(ctx, 1.0, -1.0);
            double a = -(sc->textpath - LONG_MAX / 4) * (2.0 * M_PI) / (double)LONG_MAX;
            CGContextRotateCTM(ctx, a);
            CGContextFillRect(ctx, CGRectMake(0, ascent - (CGFloat)win->linespace,
                                              cw, win->linespace));
            CGContextRestoreGState(ctx);
        } else {
            CGContextFillRect(ctx, CGRectMake(px, py, cw, win->linespace));
        }
    }

    /* draw glyph (use foreground blend mode) */
    CGContextSetBlendMode(ctx, mode2blend(sc->fmod));
    char s[2] = { c, 0 };
    CGContextSetRGBFillColor(ctx, sc->fc.r, sc->fc.g, sc->fc.b, 1.0);
    draw_string(ctx, win, sc, px + 1, py + 1, s, 1);

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
        if (sc->cury >= win->maxy && sc->autof) {
            scroll_up(win);
        } else {
            sc->cury++;
            sc->curyg += win->linespace;
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
        ami_long next = ((sc->curx - 1) / 8 + 1) * 8 + 1;
        if (next > win->maxx) next = win->maxx;
        sc->curx  = next;
        sc->curxg = (sc->curx - 1) * win->charspace + 1;
    } else if ((unsigned char)c >= 0x20) {
        /* printable character — draw and advance by actual glyph width */
        int cs = draw_char_at(win, c);
        if (sc->textpath != LONG_MAX / 4) {
            double a = sc->textpath * (2.0 * M_PI) / (double)LONG_MAX;
            sc->curxg += (int)round(cs * sin(a));
            sc->curyg -= (int)round(cs * cos(a));
        } else {
            sc->curx++;
            sc->curxg += cs;
            if (sc->curx > win->maxx && sc->autof) {
                sc->curx  = 1;
                sc->curxg = 1;
                if (sc->cury >= win->maxy) {
                    scroll_up(win);
                } else {
                    sc->cury++;
                    sc->curyg += win->linespace;
                }
            }
        }
    }
}

/* Sync cursor state to the Cocoa view for overlay drawing */
static void update_cursor(winptr win)
{
    scnptr sc = curscn(win);
    int vis = sc->curv && win->focus &&
              sc->curxg >= 1 && sc->curyg >= 1 &&
              sc->curx <= win->maxx && sc->cury <= win->maxy;
    pa_cocoa_set_cursor(win->han, vis,
                        sc->curxg - 1, sc->curyg - 1,
                        win->charspace, win->linespace);
}

/* Write interceptor: routes writes to stdout/window fds through plcchr() */
static ssize_t iwrite(int fd, const void* buff, size_t count)
{
    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && wintbl[fd].han) {
        const char* p   = (const char*)buff;
        size_t      cnt = count;
        winptr      win = &wintbl[fd];
        while (cnt--) plcchr(win, *p++);
        update_cursor(win);
        pa_cocoa_flush(win->han);
        return (ssize_t)count;
    }
    return (*ofpwrite)(fd, buff, count);
}

/* Read interceptor: routes reads from window input fds (future: keyboard queue) */
static ssize_t iread(int fd, void* buff, size_t count)
{
    return (*ofpread)(fd, buff, count);
}

static int iclose(int fd)
{
    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && wintbl[fd].han) {
        if (wintbl[fd].ctfont) { CFRelease(wintbl[fd].ctfont); wintbl[fd].ctfont = NULL; }
        pa_cocoa_destroy_window(wintbl[fd].han);
        wintbl[fd].han = NULL;
        opnfil[fd] = 0;
    }
    return (*ofpclose)(fd);
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

    case PA_EVT_USER: {
        /* a program-sent record (ami_sendevent), returned as sent but for the
           window id, which is this window's, as on x11 */
        ami_evtrec* u = (ami_evtrec*)raw->user;
        *er = *u;
        er->winid = han2wid(raw->win);
        free(u);
        break;
    }

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
        er->rszx   = raw->resize.w;
        er->rszy   = raw->resize.h;
        {
            winptr win = NULL;
            for (int i = 0; i < MAXFIL; i++)
                if (opnfil[i] && wintbl[i].han == raw->win)
                    { win = &wintbl[i]; break; }
            if (win) {
                er->rszx = raw->resize.w / win->charspace;
                er->rszy = raw->resize.h / win->linespace;
                if (!win->bufmod) {
                    win->maxxg = raw->resize.w;
                    win->maxyg = raw->resize.h;
                    win->maxx  = er->rszx;
                    win->maxy  = er->rszy;
                    pa_cocoa_resize_bitmap(win->han,
                                           raw->resize.w, raw->resize.h);
                }
            }
        }
        break;

    case PA_EVT_CLOSE:
        er->etype = ami_etterm;
        break;

    case PA_EVT_FOCUS:
        er->etype = ami_etfocus;
        {
            winptr fw = NULL;
            for (int i = 0; i < MAXFIL; i++)
                if (opnfil[i] && wintbl[i].han == raw->win)
                    { fw = &wintbl[i]; break; }
            if (fw) {
                fw->focus = TRUE;
                update_cursor(fw);
                pa_cocoa_flush(fw->han);
            }
        }
        break;

    case PA_EVT_UNFOCUS:
        er->etype = ami_etnofocus;
        {
            winptr fw = NULL;
            for (int i = 0; i < MAXFIL; i++)
                if (opnfil[i] && wintbl[i].han == raw->win)
                    { fw = &wintbl[i]; break; }
            if (fw) {
                fw->focus = FALSE;
                update_cursor(fw);
                pa_cocoa_flush(fw->han);
            }
        }
        break;

    case PA_EVT_TIMER:
        if (raw->timer.id == FRMTIM) {
            er->etype = ami_etframe;
        } else {
            er->etype  = ami_ettim;
            er->timnum = raw->timer.id + 1; /* PA timers are 1-based */
        }
        break;

    case PA_EVT_REDRAW:
        er->etype = ami_etredraw;
        er->rsx   = raw->redraw.x;
        er->rsy   = raw->redraw.y;
        er->rex   = raw->redraw.x + raw->redraw.w - 1;
        er->rey   = raw->redraw.y + raw->redraw.h - 1;
        break;

    case PA_EVT_JOY_MOVE:
        er->etype = ami_etjoymov;
        er->winid = 0;
        er->mjoyn = raw->joymove.jn + 1;
        /* the cocoa bridge delivers axes at int full scale; the API is long
           full scale (factor is 1 where long is 32 bits) */
        er->joypx = (ami_long)raw->joymove.ax[0]*(LONG_MAX/INT_MAX);
        er->joypy = (ami_long)raw->joymove.ax[1]*(LONG_MAX/INT_MAX);
        er->joypz = (ami_long)raw->joymove.ax[2]*(LONG_MAX/INT_MAX);
        er->joyp4 = (ami_long)raw->joymove.ax[3]*(LONG_MAX/INT_MAX);
        er->joyp5 = (ami_long)raw->joymove.ax[4]*(LONG_MAX/INT_MAX);
        er->joyp6 = (ami_long)raw->joymove.ax[5]*(LONG_MAX/INT_MAX);
        break;

    case PA_EVT_JOY_DOWN:
        er->etype  = ami_etjoyba;
        er->winid  = 0;
        er->ajoyn  = raw->joybtn.jn + 1;
        er->ajoybn = raw->joybtn.btn;
        break;

    case PA_EVT_JOY_UP:
        er->etype  = ami_etjoybd;
        er->winid  = 0;
        er->djoyn  = raw->joybtn.jn + 1;
        er->djoybn = raw->joybtn.btn;
        break;

    case PA_EVT_MENU:
        er->etype  = ami_etmenus;
        er->menuid = raw->menu.id;
        break;

    case PA_EVT_MIN:
        er->etype = ami_etmin;
        break;

    case PA_EVT_MAX:
        er->etype = ami_etmax;
        break;

    case PA_EVT_NORM:
        er->etype = ami_etnorm;
        break;

    case PA_EVT_WIDGET:
        switch (raw->widget.act) {
        case PA_WIDGET_BUTTON:
            er->etype = ami_etbutton;
            er->butid = raw->widget.id;
            break;
        case PA_WIDGET_CHECKBOX:
            er->etype  = ami_etchkbox;
            er->ckbxid = raw->widget.id;
            break;
        case PA_WIDGET_RADIO:
            er->etype  = ami_etradbut;
            er->radbid = raw->widget.id;
            break;
        case PA_WIDGET_SCROLL_PAGEUP:
            er->etype  = ami_etsclulp;
            er->sclupid = raw->widget.id;
            break;
        case PA_WIDGET_SCROLL_PAGEDN:
            er->etype  = ami_etscldrp;
            er->scldpid = raw->widget.id;
            break;
        case PA_WIDGET_SCROLL_POS:
            er->etype  = ami_etsclpos;
            er->sclpid = raw->widget.id;
            er->sclpos = raw->widget.pos;
            break;
        case PA_WIDGET_SLIDER_POS:
            er->etype  = ami_etsldpos;
            er->sldpid = raw->widget.id;
            er->sldpos = raw->widget.pos;
            break;
        case PA_WIDGET_EDIT_DONE:
            er->etype  = ami_etedtbox;
            er->edtbid = raw->widget.id;
            break;
        case PA_WIDGET_NUM_DONE:
            er->etype  = ami_etnumbox;
            er->numbid = raw->widget.id;
            er->numbsl = raw->widget.pos;
            break;
        case PA_WIDGET_LIST_SEL:
            er->etype  = ami_etlstbox;
            er->lstbid = raw->widget.id;
            er->lstbsl = raw->widget.pos;
            break;
        case PA_WIDGET_DROP_SEL:
            er->etype  = ami_etdrpbox;
            er->drpbid = raw->widget.id;
            er->drpbsl = raw->widget.pos;
            break;
        case PA_WIDGET_DROPED_DONE:
            er->etype  = ami_etdrebox;
            er->drebid = raw->widget.id;
            break;
        case PA_WIDGET_TAB_SEL:
            er->etype  = ami_ettabbar;
            er->tabid  = raw->widget.id;
            er->tabsel = raw->widget.pos;
            break;
        }
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

static void cursor_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->curx   = x;
    sc->cury   = y;
    sc->curxg  = (x-1) * win->charspace + 1;
    sc->curyg  = (y-1) * win->linespace + 1;
}

static ami_long maxx_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxx : 80;
}

static ami_long maxy_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxy : 25;
}

static void home_ivf(FILE* f)    { ami_cursor(f, 1, 1); }

static void del_ivf(FILE* f)
{
    /* delete character at cursor — stub */
}

static void up_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->cury > 1) ami_cursor(f, sc->curx, sc->cury - 1);
}

static void down_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->cury < win->maxy) ami_cursor(f, sc->curx, sc->cury + 1);
}

static void left_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->curx > 1) ami_cursor(f, sc->curx - 1, sc->cury);
}

static void right_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    if (sc->curx < win->maxx) ami_cursor(f, sc->curx + 1, sc->cury);
}

static void blink_ivf(FILE* f, ami_long e)      { /* stub — CoreText doesn't blink */ }

static void reverse_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->reverse = e;
}

static void superscript_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->superscript = e;
    curscn(win)->subscript = 0; /* mutually exclusive */
}

static void subscript_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->subscript = e;
    curscn(win)->superscript = 0; /* mutually exclusive */
}

static void underline_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->underline = e;
}

static void italic_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->italic = e;
    rebuild_font(win);
}

static void bold_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bold = e;
    rebuild_font(win);
}

static void strikeout_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->strikeout = e;
}

static void standout_ivf(FILE* f, ami_long e)
{
    /* implement as reverse video */
    ami_reverse(f, e);
}

static void fcolor_ivf(FILE* f, ami_color c)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fc = ami2rgba(c);
}

static void bcolor_ivf(FILE* f, ami_color c)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba bc = ami2rgba(c);
    curscn(win)->bc = bc;
    if (win->han) pa_cocoa_set_background(win->han, bc.r, bc.g, bc.b);
}

static void auto_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->autof = e;
    win->gauto = e; /* window-level: a later screen select inherits this */
}

static void curvis_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->curv = e;
    update_cursor(win);
    pa_cocoa_flush(win->han);
}

static void scroll_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    ami_scrollg(f, x * win->charspace, y * win->linespace);
}

static ami_long curx_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curx : 1;
}

static ami_long cury_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->cury : 1;
}

static ami_long curbnd_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return FALSE;
    scnptr sc  = curscn(win);
    return sc->curx >= 1 && sc->curx <= win->maxx &&
           sc->cury >= 1 && sc->cury <= win->maxy;
}

static void select_ivf(FILE* f, ami_long u, ami_long d)
{
    winptr win = f2win(f); if (!win) return;
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON) return;
    win->curupd = u;
    win->curdsp = d;
    /* the selected update screen inherits the window's auto mode, so a
       program that set ami_auto once (on another screen) still gets it
       here -- otherwise a page-flip page keeps the default and scrolls
       when the other page does not, making the frame jump. */
    win->screens[u-1].autof = win->gauto;
    pa_cocoa_select_screens(win->han, u - 1, d - 1);
}

static void settab_ivf(FILE* f, ami_long t)  { /* stub */ }
static void restab_ivf(FILE* f, ami_long t)  { /* stub */ }
static void clrtab_ivf(FILE* f)         { /* stub */ }

static void viewoffg_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc = curscn(win);
    sc->offx = x;
    sc->offy = y;
    settrans(win);
}

static void viewscale_ivf(FILE* f, float x, float y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc = curscn(win);
    sc->scalex = x;
    sc->scaley = y;
    settrans(win);
}
static void linestyle_ivf(FILE* f, ami_lstyle style)     { /* stub */ }
static void setpoints_ivf(FILE* f, float ps)
{
    winptr win = f2win(f); if (!win) return;
    int pixsiz = (int)(ps * (float)win->dpmy / 2835.0f + 0.5f);
    if (pixsiz < 1) pixsiz = 1;
    win->fontsz  = (CGFloat)pixsiz;
    win->gfpoint = ps;
    scnptr sc = curscn(win);
    if (sc->font && sc->font->name) {
        win->cellh = 0; /* the point size builds the font; its span sets the cell */
        buildfont(win, sc->font->name, sc->bold, sc->italic);
        sc->font->size   = win->fontsz;
        update_metrics(win);
    }
}

static float points_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->gfpoint : 11.0f;
}

static ami_long  funkey_ivf(FILE* f)         { return 12; /* F1-F12 */ }

static void frametimer_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    if (e) {
        if (!win->frmrun) {
            pa_cocoa_set_timer(win->han, FRMTIM, 170, 1); /* 17ms repeating */
            win->frmrun = TRUE;
        }
    } else {
        if (win->frmrun) {
            pa_cocoa_kill_timer(win->han, FRMTIM);
            win->frmrun = FALSE;
        }
    }
}
static void autohold_ivf(ami_long e)            { fautohold = e; }

static void wrtstr_ivf(FILE* f, char* s)
{
    if (!s) return;
    /* route through fwrite so stdio layer handles it */
    fwrite(s, 1, strlen(s), f);
}

static void wrtstrn_ivf(FILE* f, char* s, ami_long n)
{
    if (!s || n <= 0) return;
    fwrite(s, 1, n, f);
}

static void sizbufg_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    if (x < 1 || y < 1) return;
    pa_cocoa_resize_bitmap(win->han, x, y);
    win->maxxg = x;
    win->maxyg = y;
    win->maxx  = x / win->charspace;
    win->maxy  = y / win->linespace;
    if (win->maxx < 1) win->maxx = 1;
    if (win->maxy < 1) win->maxy = 1;
    /* the buffer's drawing surface was recreated: drawing attributes reset
       to defaults, matching the Windows DC-recreation behavior */
    curscn(win)->lwidth = 1.0;
}

static void sizbuf_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    ami_sizbufg(f, x * win->charspace, y * win->linespace);
}

static void title_ivf(FILE* f, char* ts)
{
    winptr win = f2win(f); if (!win || !ts) return;
    pa_cocoa_set_title(win->han, ts);
}

/* The event handlers a program hooks in: one for each event code, and a
   master handler ahead of them, as the Linux backend keeps them. Each
   event goes to the master handler, then to its code's handler, and comes
   back to the caller only if neither takes it. The hooks were not kept
   here, so a program's handler was never called: terminal_test hooks the
   terminate event to end the program, and with the hook unheeded the
   window's close button and a control-c in the window did nothing. */
static ami_pevthan evthan[ami_etdsize+1];
static ami_pevthan evtshan;

static void defaultevent(ami_evtrec* ev)
{
    ev->handled = 0; /* not handled: it goes on */
}

static void eventover_ivf(ami_evtcod e, ami_pevthan eh, ami_pevthan* oeh)
{
    if (e < 0 || e > ami_etdsize) { if (oeh) *oeh = NULL; return; }
    if (!evtshan) evtshan = defaultevent;
    if (!evthan[e]) evthan[e] = defaultevent;
    if (oeh) *oeh = evthan[e];
    evthan[e] = eh;
}
static void eventsover_ivf(ami_pevthan eh, ami_pevthan* oeh)
{
    if (!evtshan) evtshan = defaultevent;
    if (oeh) *oeh = evtshan;
    evtshan = eh;
}


/* Send an event to the window's input queue, as the x11 backend does: a
   copy of the record, its window id stamped to this window's, delivered
   through the Cocoa event queue so a call from another thread (graph_server's
   pump thread, waking the loop after a datagram) wakes a blocked ami_event. */
static void sendevent_ivf(FILE* f, ami_evtrec* er)
{
    winptr win = f2win(f); if (!win) return;
    ami_evtrec* copy = malloc(sizeof(*copy));
    if (!copy) return;
    *copy = *er;
    pa_cocoa_send_user_event(win->han, copy);
}

/*******************************************************************************
*                                                                              *
*                        PA API — graphical operations                         *
*                                                                              *
*******************************************************************************/

static ami_long maxxg_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxxg : maxxd;
}

static ami_long maxyg_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->maxyg : maxyd;
}

static ami_long curxg_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curxg : 1;
}

static ami_long curyg_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? curscn(win)->curyg : 1;
}

static void cursorg_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->curxg  = x;
    sc->curyg  = y;
    sc->curx   = (x - 1) / win->charspace + 1;
    sc->cury   = (y - 1) / win->linespace + 1;
}

static void line_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
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

static void linewidth_ivf(FILE* f, ami_long w)
{
    winptr win = f2win(f); if (!win) return;
    scnptr sc  = curscn(win);
    sc->lwidth = (CGFloat)(w > 0 ? w : 1);
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (ctx) CGContextSetLineWidth(ctx, sc->lwidth);
}

static void rect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGRect r = CGRectMake(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1);
    CGContextStrokeRect(ctx, r);
    pa_cocoa_flush(win->han);
}

static void frect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGContextFillRect(ctx, CGRectMake(PX(x1), PY(y1), x2-x1+1, y2-y1+1));
    pa_cocoa_flush(win->han);
}

/* Build a rounded rectangle CGPath using elliptical corner arcs.
 * xs, ys are the full width/height of the corner ellipses (PA convention).
 * Uses CGPath with per-arc transforms since CGContext transforms reset paths. */
static CGPathRef create_rrect_path(CGFloat x, CGFloat y,
                                   CGFloat w, CGFloat h,
                                   CGFloat xs, CGFloat ys)
{
    CGFloat rx = xs / 2.0;
    CGFloat ry = ys / 2.0;
    if (rx > w / 2.0) rx = w / 2.0;
    if (ry > h / 2.0) ry = h / 2.0;

    CGMutablePathRef path = CGPathCreateMutable();

    /* top edge */
    CGPathMoveToPoint(path, NULL, x + rx, y);
    CGPathAddLineToPoint(path, NULL, x + w - rx, y);

    /* top-right corner */
    CGAffineTransform t = CGAffineTransformMake(rx, 0, 0, ry, x + w - rx, y + ry);
    CGPathAddArc(path, &t, 0, 0, 1, -M_PI_2, 0, false);

    /* right edge */
    CGPathAddLineToPoint(path, NULL, x + w, y + h - ry);

    /* bottom-right corner */
    t = CGAffineTransformMake(rx, 0, 0, ry, x + w - rx, y + h - ry);
    CGPathAddArc(path, &t, 0, 0, 1, 0, M_PI_2, false);

    /* bottom edge */
    CGPathAddLineToPoint(path, NULL, x + rx, y + h);

    /* bottom-left corner */
    t = CGAffineTransformMake(rx, 0, 0, ry, x + rx, y + h - ry);
    CGPathAddArc(path, &t, 0, 0, 1, M_PI_2, M_PI, false);

    /* left edge */
    CGPathAddLineToPoint(path, NULL, x, y + ry);

    /* top-left corner */
    t = CGAffineTransformMake(rx, 0, 0, ry, x + rx, y + ry);
    CGPathAddArc(path, &t, 0, 0, 1, M_PI, M_PI + M_PI_2, false);

    CGPathCloseSubpath(path);
    return path;
}

static void rrect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGPathRef path = create_rrect_path(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1, xs, ys);
    CGContextAddPath(ctx, path);
    CGPathRelease(path);
    CGContextStrokePath(ctx);
    pa_cocoa_flush(win->han);
}

static void frrect_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    scnptr sc = curscn(win);
    set_fill_only(ctx, sc->fc);
    CGPathRef path = create_rrect_path(PX(x1), PY(y1), x2-x1+1, y2-y1+1, xs, ys);
    CGContextAddPath(ctx, path);
    CGPathRelease(path);
    CGContextFillPath(ctx);
    pa_cocoa_flush(win->han);
}

static void ellipse_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextStrokeEllipseInRect(ctx,
        CGRectMake(PX(x1)+0.5, PY(y1)+0.5, x2-x1, y2-y1));
    pa_cocoa_flush(win->han);
}

static void fellipse_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
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
 * PA: 0 = top center, clockwise, LONG_MAX = 360°.
 * CG (flipped Y-down context): 0 = right (3 o'clock), clockwise.
 * So CG = PA_radians - π/2. */
#define PA2CG(a) (ANG2RAD(a) - M_PI_2)

static void arc_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
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

static void farc_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
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

static void fchord_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea)
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

static void ftriangle_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3)
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

static void setpixel_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = get_ctx(win);
    if (!ctx) return;
    CGContextFillRect(ctx, CGRectMake(PX(x), PY(y), 1, 1));
    pa_cocoa_flush(win->han);
}

static void fover_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fmod = mdnorm;
}

static void bover_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bmod = mdnorm;
}

static void finvis_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->fmod = mdinvis;
}

static void binvis_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->bmod = mdinvis;
}

static void fxor_ivf(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->fmod = mdxor; }
static void bxor_ivf(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->bmod = mdxor; }
static void fand_ivf(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->fmod = mdand; }
static void band_ivf(FILE* f) { winptr win = f2win(f); if (win) curscn(win)->bmod = mdand; }
static void for_ivf(FILE* f)  { winptr win = f2win(f); if (win) curscn(win)->fmod = mdor;  }
static void bor_ivf(FILE* f)  { winptr win = f2win(f); if (win) curscn(win)->bmod = mdor;  }

static ami_long chrsizx_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->charspace : 8;
}

static ami_long chrsizy_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->linespace : 16;
}

static ami_long fonts_ivf(FILE* f) { return fntcnt; }

/* Update charspace/linespace/maxx/maxy from the current font */
static void update_metrics(winptr win)
{
    scnptr sc = curscn(win);
    fontptr fp = sc->font ? sc->font : fntlst;
    CTFontRef cf = winfont(win);
    if (!cf) return;
    win->linespace = win->cellh > 0 ? win->cellh : glyphspan(cf) + 2;
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

static void font_ivf(FILE* f, ami_long fc)
{
    winptr win = f2win(f); if (!win) return;
    fontptr fp = fntlst;
    int i = 1;
    while (fp && i < fc) { fp = fp->next; i++; }
    if (fp) {
        curscn(win)->font = fp;
        win->cfont        = fp;
        /* a face change always builds the window's font at its size: the
           font is the window's, so there is no record cache to consult */
        if (fp->name)
            buildfont(win, fp->name, curscn(win)->bold, curscn(win)->italic);
        update_metrics(win);
    }
}

static void fontnam_ivf(FILE* f, ami_long fc, char* fns, ami_long fnsl)
{
    fontptr fp = fntlst;
    int i = 1;
    while (fp && i < fc) { fp = fp->next; i++; }
    /* critical buffer: a full-length result is left unterminated */
    cpycrit(fns, fnsl, fp && fp->name ? fp->name : "");
}

static void fontsiz_ivf(FILE* f, ami_long s)
{
    winptr win = f2win(f); if (!win) return;
    /* the size asked for is the cell's height: the font is fitted to it */
    win->cellh = (int)(s > 0 ? s : DEF_FONT_H);
    win->fontsz = (CGFloat)win->cellh;
    scnptr sc = curscn(win);
    if (sc->font && sc->font->name) {
        buildfont(win, sc->font->name, sc->bold, sc->italic);
        update_metrics(win);
    }
    win->gfpoint = win->fontsz * 2835.0f / (float)win->dpmy;
}

static void chrspcy_ivf(FILE* f, ami_long s)  { /* stub */ }
static void chrspcx_ivf(FILE* f, ami_long s)  { /* stub */ }

static ami_long dpmx_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->dpmx : 3780;
}

static ami_long dpmy_ivf(FILE* f)
{
    winptr win = f2win(f);
    return win ? win->dpmy : 3780;
}

static ami_long strsiz_ivf(FILE* f, const char* s)
{
    winptr win = f2win(f); if (!win || !s) return 0;
    scnptr sc  = curscn(win);
    return (ami_long)measure_string(win,
                                s, (int)strlen(s));
}

static ami_long chrpos_ivf(FILE* f, const char* s, ami_long p)
{
    winptr win = f2win(f); if (!win || !s || p <= 0) return 0;
    scnptr sc  = curscn(win);
    int len = (int)strlen(s);
    if (p > len) p = len;
    return (ami_long)measure_string(win, s, (int)p);
}

#define MINJST 1 /* minimum pixels for space in justification */

/* Compute justification spacing parameters.
 * Returns: spc = pixels per space, ss = total space budget remaining. */
static void just_params(winptr win, const char* s, ami_long n,
                        ami_long* out_spc, ami_long* out_ss)
{
    fontptr fp = curscn(win)->font ? curscn(win)->font : win->cfont;
    int l = (int)strlen(s);
    ami_long sz = 0; /* critical size: chars + min space */
    ami_long ns = 0; /* number of spaces */
    ami_long cs = 0; /* total character width (no spaces) */
    for (int i = 0; i < l; i++) {
        if (s[i] == ' ') { sz += MINJST; ns++; }
        else {
            int cw = (int)(measure_string(win, &s[i], 1) + 0.5);
            sz += cw;
            cs += cw;
        }
    }
    ami_long spc = MINJST;
    ami_long ss = ns * MINJST;
    if (ns > 0 && n > sz) { spc = (n - cs) / ns; ss = n - cs; }
    *out_spc = spc;
    *out_ss  = ss;
}

static void writejust_ivf(FILE* f, const char* s, ami_long n)
{
    winptr win = f2win(f); if (!win || !s) return;
    scnptr sc  = curscn(win);
    fontptr fp = sc->font ? sc->font : win->cfont;
    int l = (int)strlen(s);

    ami_long spc, ss;
    just_params(win, s, n, &spc, &ss);

    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;

    for (int i = 0; i < l; i++) {
        if (s[i] == ' ') {
            /* advance by justified space width */
            ami_long cbs = (spc > ss) ? ss : spc;
            /* draw background for space */
            if (sc->bmod != mdinvis) {
                CGContextSetBlendMode(ctx, mode2blend(sc->bmod));
                CGContextSetRGBFillColor(ctx, sc->bc.r, sc->bc.g, sc->bc.b, 1.0);
                CGContextFillRect(ctx, CGRectMake(sc->curxg - 1, sc->curyg - 1,
                                                  cbs, win->linespace));
                CGContextSetBlendMode(ctx, mode2blend(sc->fmod));
            }
            if (spc > ss) sc->curxg += ss;
            else { sc->curxg += spc; ss -= spc; }
        } else {
            /* draw character normally */
            int cw = draw_char_at(win, s[i]);
            sc->curxg += cw;
            sc->curx = (sc->curxg - 1) / win->charspace + 1;
        }
    }
    pa_cocoa_flush(win->han);
}

static ami_long justpos_ivf(FILE* f, const char* s, ami_long p, ami_long n)
{
    winptr win = f2win(f); if (!win || !s) return 0;
    fontptr fp = curscn(win)->font ? curscn(win)->font : win->cfont;
    int l = (int)strlen(s);
    if (p < 0 || p >= l) return 0;

    ami_long spc, ss;
    just_params(win, s, n, &spc, &ss);

    ami_long cp = 0;  /* accumulated pixel position */
    ami_long crp = 0; /* result position */
    for (int i = 0; i < l; i++) {
        if (i == p) crp = cp;
        if (s[i] == ' ') {
            if (spc > ss) cp += ss;
            else { cp += spc; ss -= spc; }
        } else {
            cp += (int)(measure_string(win, &s[i], 1) + 0.5);
        }
    }
    return crp;
}

static void condensed_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->condensed = e;
    curscn(win)->extended = 0;
    rebuild_font(win);
}

static void extended_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->extended = e;
    curscn(win)->condensed = 0;
    rebuild_font(win);
}

static void xlight_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->xlight = e;
    /* xlight uses a lighter weight — approximated by alpha */
}

static void light_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->light = e;
}

static void xbold_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->xbold = e;
    rebuild_font(win);
}

static void hollow_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->hollow = e;
}

static void raised_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->raised = e;
}
static void settabg_ivf(FILE* f, ami_long t)    { /* stub */ }
static void restabg_ivf(FILE* f, ami_long t)    { /* stub */ }

static ami_long baseline_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return 0;
    scnptr sc  = curscn(win);
    fontptr fp = sc->font ? sc->font : win->cfont;
    CTFontRef cf = winfont(win);
    if (!cf) return 0;
    return (ami_long)CTFontGetAscent(cf);
}

static void fcolorg_ivf(FILE* f, ami_long r, ami_long g, ami_long b)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba c = { r/(double)LONG_MAX, g/(double)LONG_MAX, b/(double)LONG_MAX, 1.0 };
    curscn(win)->fc = c;
}

static void fcolorc_ivf(FILE* f, ami_long r, ami_long g, ami_long b) { ami_fcolorg(f, r, g, b); }

static void bcolorg_ivf(FILE* f, ami_long r, ami_long g, ami_long b)
{
    winptr win = f2win(f); if (!win) return;
    pa_rgba c = { r/(double)LONG_MAX, g/(double)LONG_MAX, b/(double)LONG_MAX, 1.0 };
    curscn(win)->bc = c;
    if (win->han) pa_cocoa_set_background(win->han, c.r, c.g, c.b);
}

static void bcolorc_ivf(FILE* f, ami_long r, ami_long g, ami_long b) { ami_bcolorg(f, r, g, b); }

/* Picture storage */
static CGImageRef pictbl[MAXPIC]; /* loaded pictures, 1-based index */

/* Set or overwrite extension on a filename */
static void setext(char* fn, const char* ext)
{
    char* p = strrchr(fn, '.');
    char* s = strrchr(fn, '/');
    if (p && (!s || p > s)) *p = 0; /* remove existing extension */
    strcat(fn, ext);
}

static void loadpict_ivf(FILE* f, ami_long p, char* fn)
{
    winptr win = f2win(f); if (!win) return;
    if (p < 1 || p > MAXPIC) return;

    /* delete any existing picture in this slot */
    if (pictbl[p-1]) { CGImageRelease(pictbl[p-1]); pictbl[p-1] = NULL; }

    /* copy filename and add .bmp extension if needed */
    char fnh[512];
    strncpy(fnh, fn, sizeof(fnh)-5);
    fnh[sizeof(fnh)-5] = 0;
    setext(fnh, ".bmp");

    /* read file into memory */
    FILE* pf = fopen(fnh, "rb");
    if (!pf) return;
    fseek(pf, 0, SEEK_END);
    ami_long sz = ftell(pf);
    fseek(pf, 0, SEEK_SET);
    uint8_t* buf = malloc(sz);
    if (!buf) { fclose(pf); return; }
    fread(buf, 1, sz, pf);
    fclose(pf);

    /* create CGImage via ImageIO (handles BMP natively) */
    CFDataRef data = CFDataCreate(NULL, buf, sz);
    free(buf);
    if (!data) return;

    CGImageSourceRef src = CGImageSourceCreateWithData(data, NULL);
    CFRelease(data);
    if (!src) return;

    pictbl[p-1] = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
}

static ami_long pictsizx_ivf(FILE* f, ami_long p)
{
    if (p < 1 || p > MAXPIC || !pictbl[p-1]) return 0;
    return (ami_long)CGImageGetWidth(pictbl[p-1]);
}

static ami_long pictsizy_ivf(FILE* f, ami_long p)
{
    if (p < 1 || p > MAXPIC || !pictbl[p-1]) return 0;
    return (ami_long)CGImageGetHeight(pictbl[p-1]);
}

static void picture_ivf(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2)
{
    winptr win = f2win(f); if (!win) return;
    if (p < 1 || p > MAXPIC || !pictbl[p-1]) return;
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;

    /* draw the image scaled to fit the bounding box */
    CGFloat dx = PX(x1), dy = PY(y1);
    CGFloat dw = x2 - x1 + 1, dh = y2 - y1 + 1;

    /* CGContextDrawImage draws in CG's Y-up coords, but our context is
     * flipped (Y-down). Save state, flip locally, draw, restore. */
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, dx, dy + dh);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextDrawImage(ctx, CGRectMake(0, 0, dw, dh), pictbl[p-1]);
    CGContextRestoreGState(ctx);

    pa_cocoa_flush(win->han);
}

static void delpict_ivf(FILE* f, ami_long p)
{
    if (p < 1 || p > MAXPIC) return;
    if (pictbl[p-1]) { CGImageRelease(pictbl[p-1]); pictbl[p-1] = NULL; }
}
static void scrollg_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    CGContextRef ctx = pa_cocoa_get_context(win->han);
    if (!ctx) return;
    scnptr sc = curscn(win);
    ami_long w = win->maxxg;
    ami_long h = win->maxyg;

    if (x == 0 && y == 0) return;

    /* if scroll exceeds screen, just clear */
    if (labs(x) >= w || labs(y) >= h) {
        clear_window(win);
        pa_cocoa_flush(win->han);
        return;
    }

    /* All operations use raw pixel data to avoid mixing memmove with CG calls
       (which is undefined per Apple docs for bitmap contexts). */
    size_t   pw       = CGBitmapContextGetWidth(ctx);
    size_t   ph       = CGBitmapContextGetHeight(ctx);
    size_t   rowbytes = CGBitmapContextGetBytesPerRow(ctx);
    uint8_t* data     = (uint8_t*)CGBitmapContextGetData(ctx);
    if (!data) return;

    float scale = (w > 0) ? (float)pw / w : 1.0f;
    int ppx = (int)(labs(x) * scale + 0.5f);
    int ppy = (int)(labs(y) * scale + 0.5f);
    int bpp = (int)(rowbytes / pw);

    /* build fill pixel in bitmap format (ARGB premultiplied, host byte order) */
    uint32_t fr = (uint32_t)(sc->bc.r * 255 + 0.5);
    uint32_t fg = (uint32_t)(sc->bc.g * 255 + 0.5);
    uint32_t fb = (uint32_t)(sc->bc.b * 255 + 0.5);
    uint32_t fillpx = (0xFFu << 24) | (fr << 16) | (fg << 8) | fb;

    /* shift rows vertically */
    if (y > 0) {
        memmove(data, data + ppy * rowbytes, (ph - ppy) * rowbytes);
    } else if (y < 0) {
        memmove(data + ppy * rowbytes, data, (ph - ppy) * rowbytes);
    }

    /* shift columns horizontally */
    if (x != 0) {
        for (size_t row = 0; row < ph; row++) {
            uint8_t* rp = data + row * rowbytes;
            if (x > 0) {
                memmove(rp, rp + ppx * bpp, (pw - ppx) * bpp);
            } else {
                memmove(rp + ppx * bpp, rp, (pw - ppx) * bpp);
            }
        }
    }

    /* fill vacated strips with background color using raw pixels */
    /* vertical vacated strip */
    if (y > 0) {
        /* bottom strip: rows ph-ppy .. ph-1 */
        for (size_t row = ph - ppy; row < ph; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = 0; col < pw; col++) rp[col] = fillpx;
        }
    } else if (y < 0) {
        /* top strip: rows 0 .. ppy-1 */
        for (size_t row = 0; row < (size_t)ppy; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = 0; col < pw; col++) rp[col] = fillpx;
        }
    }
    /* horizontal vacated strip */
    if (x > 0) {
        /* right strip: cols pw-ppx .. pw-1 */
        for (size_t row = 0; row < ph; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = pw - ppx; col < pw; col++) rp[col] = fillpx;
        }
    } else if (x < 0) {
        /* left strip: cols 0 .. ppx-1 */
        for (size_t row = 0; row < ph; row++) {
            uint32_t* rp = (uint32_t*)(data + row * rowbytes);
            for (size_t col = 0; col < (size_t)ppx; col++) rp[col] = fillpx;
        }
    }

    pa_cocoa_flush(win->han);
}
static void path_ivf(FILE* f, ami_long a)
{
    winptr win = f2win(f); if (!win) return;
    curscn(win)->textpath = a;
}

/*******************************************************************************
*                                                                              *
*                        PA API — window management                            *
*                                                                              *
*******************************************************************************/

static void openwin_ivf(FILE** infile, FILE** outfile, FILE* parent, ami_long wid)
{
    if (!inited) pa_graphics_init();

    /* The input side is the caller's: a window takes the input stream it is
     * given, typically stdin, and a window closed later leaves it be. (It
     * once opened a /dev/null input of its own for every window, and as
     * nothing closes the input of a closed window, each window left a
     * descriptor behind; a program that opens and closes windows in a loop,
     * as the window test does a hundred times over, ran out of descriptors
     * below MAXFIL after thirty or so and got no window.) No stream, or a
     * stream that is not open, or the output side of a window, is an error.
     *
     * The output side is a /dev/null stream of its own: its descriptor
     * indexes opnfil[]/wintbl[], so iwrite() can route writes to plcchr(). */
    FILE* inf = *infile;
    int   ifn = inf? fileno(inf): -1;
    if (ifn < 0 || (ifn < MAXFIL && opnfil[ifn] && wintbl[ifn].han)) {
        fprintf(stderr, "\nError: Graphics: Input side of window in wrong mode\n");
        exit(1);
    }
    FILE* outf = fopen("/dev/null", "w");
    if (!outf) return;
    setvbuf(outf, NULL, _IONBF, 0);

    int ofn = fileno(outf);
    if (ofn < 0 || ofn >= MAXFIL) {
        fprintf(stderr, "*** graphics: cannot open window: out of file slots\n");
        fclose(outf);
        return;
    }

    /* create the Cocoa window: with a parent it becomes an embedded child
       view clipped to the parent; otherwise a top-level window cascaded
       from the main window */
    int wx = 100 + (wid - 1) * 30;
    int wy = 100 + (wid - 1) * 30;
    pa_winhan han;
    winptr pwin = parent ? f2win(parent) : NULL;
    if (pwin && pwin->han)
        han = pa_cocoa_create_child_window(pwin->han, wx - 100, wy - 100,
                                           maxxd, maxyd);
    else
        han = pa_cocoa_create_window(wx, wy, maxxd, maxyd, "");
    if (!han) { fclose(outf); return; }

    winptr win = &wintbl[ofn];
    win_init(win, wid, parent ? fileno(parent) : 0, maxxd, maxyd);
    win->han     = han;
    win->infile  = inf;
    win->outfile = outf;
    opnfil[ofn]  = 1;
    pa_cocoa_set_bufmod(han, win->bufmod);
    pa_cocoa_set_background(han, 1.0f, 1.0f, 1.0f);

    pa_cocoa_show_window(han);
    clear_window(win);
    pa_cocoa_flush(han);

    *infile  = inf;
    *outfile = outf;
}

static void buffer_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    win->bufmod = e;
    if (win->han) pa_cocoa_set_bufmod(win->han, e);
}

/* Standard character cell for desktop (parentless) windows. The desktop
   has no character grid, so window sizes/positions in character terms use
   this made-up cell, matching the Windows/Linux implementations. */
#define STDCHRX 8
#define STDCHRY 12

/* get parent window record, or NULL if window has no parent */
static winptr parwin(winptr win)
{
    if (win->parwid > 0 && win->parwid < MAXFIL && opnfil[win->parwid])
        return &wintbl[win->parwid];
    return NULL;
}

/* chrome extra size (window minus client) for the window's current state */
static void curextra(winptr win, int* dw, int* dh)
{
    pa_cocoa_frame_extra(win->han, win->frame, win->sizable, win->sysbar,
                         dw, dh);
}

/* window sizes are OUTER dimensions, frame included, per PA semantics */

static void getsizg_ivf(FILE* f, ami_long* x, ami_long* y)
{
    winptr win = f2win(f);
    /* the actual on-screen window size, not the buffer size: the drawing
       surface is independent of the window in buffered mode */
    if (win) {
        int w, h; /* shim boundary: pixel sizes pass as int */
        pa_cocoa_get_window_size(win->han, &w, &h);
        *x = w; *y = h;
    } else { *x = maxxd; *y = maxyd; }
}

static void getsiz_ivf(FILE* f, ami_long* x, ami_long* y)
{
    winptr win = f2win(f);
    if (!win) { *x = 80; *y = 25; return; }
    ami_getsizg(f, x, y);
    winptr par = parwin(win);
    if (par) { *x = (*x - 1) / par->charspace + 1;
               *y = (*y - 1) / par->linespace + 1; }
    else     { *x = (*x - 1) / STDCHRX + 1;
               *y = (*y - 1) / STDCHRY + 1; }
}

static void setsizg_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    int dw, dh;
    curextra(win, &dw, &dh);
    int cw = x - dw; if (cw < 1) cw = 1;
    int ch = y - dh; if (ch < 1) ch = 1;
    pa_cocoa_resize_window(win->han, cw, ch);
    /* the drawing surface (maxxg/maxyg) belongs to the buffer, sized by
       ami_sizbuf; only an unbuffered window's surface tracks the window */
    if (!win->bufmod) {
        win->maxxg = cw;
        win->maxyg = ch;
        win->maxx  = cw / win->charspace;
        win->maxy  = ch / win->linespace;
        pa_cocoa_resize_bitmap(win->han, cw, ch);
        scnptr sc = curscn(win);
        sc->lwidth = 1.0;
        CGContextRef ctx = pa_cocoa_get_context(win->han);
        if (ctx) CGContextSetLineWidth(ctx, 1.0);
    }
}

static void setsiz_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    winptr par = parwin(win);
    if (par) ami_setsizg(f, x * par->charspace, y * par->linespace);
    else     ami_setsizg(f, x * STDCHRX, y * STDCHRY);
}

static void setposg_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    /* pa_cocoa_move_window positions a child relative to the parent's
       top-left and a top-level window on the screen */
    pa_cocoa_move_window(win->han, x - 1, y - 1);
}

static void setpos_ivf(FILE* f, ami_long x, ami_long y)
{
    winptr win = f2win(f); if (!win) return;
    winptr par = parwin(win);
    /* character positions scale by the parent's character cell; a window
       with no parent uses the standard desktop character cell */
    if (par) ami_setposg(f, (x - 1) * par->charspace + 1,
                            (y - 1) * par->linespace + 1);
    else     ami_setposg(f, (x - 1) * STDCHRX + 1,
                            (y - 1) * STDCHRY + 1);
}

static void scnsiz_ivf(FILE* f, ami_long* x, ami_long* y)
{
    winptr win = f2win(f);
    int sw = pa_cocoa_screen_w();
    int sh = pa_cocoa_screen_h();
    *x = win ? sw / win->charspace : 80;
    *y = win ? sh / win->linespace : 25;
}

static void scnsizg_ivf(FILE* f, ami_long* x, ami_long* y)
{
    *x = pa_cocoa_screen_w();
    *y = pa_cocoa_screen_h();
}

static void scncen_ivf(FILE* f, ami_long* x, ami_long* y)
{
    winptr win = f2win(f);
    int sw = pa_cocoa_screen_w();
    int sh = pa_cocoa_screen_h();
    *x = win ? sw / win->charspace / 2 : 40;
    *y = win ? sh / win->linespace / 2 : 12;
}

static void scnceng_ivf(FILE* f, ami_long* x, ami_long* y)
{
    *x = pa_cocoa_screen_w() / 2;
    *y = pa_cocoa_screen_h() / 2;
}

static void winclientg_ivf(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms)
{
    winptr win = f2win(f);
    if (!win) { *wx = cx; *wy = cy; return; }
    int dw, dh;
    pa_cocoa_frame_extra(win->han,
                         (ms & BIT(ami_wmframe))  != 0,
                         (ms & BIT(ami_wmsize))   != 0,
                         (ms & BIT(ami_wmsysbar)) != 0,
                         &dw, &dh);
    *wx = cx + dw;
    *wy = cy + dh;
}

static void winclient_ivf(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms)
{
    winptr win = f2win(f);
    if (!win) { *wx = cx; *wy = cy; return; }
    /* client size in this window's characters -> outer size in the parent's
       characters (or the standard desktop cell for parentless windows) */
    ami_winclientg(f, cx * win->charspace, cy * win->linespace, wx, wy, ms);
    winptr par = parwin(win);
    if (par) { *wx = (*wx - 1) / par->charspace + 1;
               *wy = (*wy - 1) / par->linespace + 1; }
    else     { *wx = (*wx - 1) / STDCHRX + 1;
               *wy = (*wy - 1) / STDCHRY + 1; }
}

static void front_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_front(win->han);
}

static void back_ivf(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_back(win->han);
}

static void frame_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    win->frame = e;
    pa_cocoa_set_frame(win->han, e);
}

static void sizable_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    win->sizable = e;
    pa_cocoa_set_sizable(win->han, e);
}

static void sysbar_ivf(FILE* f, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    win->sysbar = e;
    pa_cocoa_set_sysbar(win->han, e);
}
static void menu_ivf(FILE* f, ami_menuptr m)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_menu(win->han, (void*)m);
}

static void menuena_ivf(FILE* f, ami_long id, ami_long onoff)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_menu_enable(win->han, id, onoff);
}

static void menusel_ivf(FILE* f, ami_long id, ami_long select)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_menu_check(win->han, id, select);
}

static void stdmenu_append(ami_menuptr* list, ami_menuptr m)
{
    m->next = NULL;
    if (!*list) { *list = m; return; }
    ami_menuptr lp = *list;
    while (lp->next) lp = lp->next;
    lp->next = m;
}

static void stdmenu_new(ami_menuptr* m, int id, const char* face)
{
    *m = malloc(sizeof(ami_menurec));
    (*m)->next   = NULL;
    (*m)->branch = NULL;
    (*m)->onoff  = FALSE;
    (*m)->oneof  = FALSE;
    (*m)->bar    = FALSE;
    (*m)->id     = id;
    (*m)->face   = strdup(face);
}

static void stdmenu_additem(ami_stdmenusel sms, int i, ami_menuptr* m,
                            ami_menuptr* list, const char* face, int bar)
{
    if (BIT(i) & sms) {
        stdmenu_new(m, i, face);
        stdmenu_append(list, *m);
        (*m)->bar = bar;
    }
}

static void stdmenu_ivf(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)
{
    ami_menuptr m, hm;

    *sm = NULL;

    if (sms & (BIT(AMI_SMNEW) | BIT(AMI_SMOPEN) | BIT(AMI_SMCLOSE) |
               BIT(AMI_SMSAVE) | BIT(AMI_SMSAVEAS) | BIT(AMI_SMPAGESET) |
               BIT(AMI_SMPRINT) | BIT(AMI_SMEXIT))) {
        stdmenu_new(&hm, 0, "File");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMNEW, &m, &hm->branch, "New", FALSE);
        stdmenu_additem(sms, AMI_SMOPEN, &m, &hm->branch, "Open", FALSE);
        stdmenu_additem(sms, AMI_SMCLOSE, &m, &hm->branch, "Close", FALSE);
        stdmenu_additem(sms, AMI_SMSAVE, &m, &hm->branch, "Save", FALSE);
        stdmenu_additem(sms, AMI_SMSAVEAS, &m, &hm->branch, "Save As", TRUE);
        stdmenu_additem(sms, AMI_SMPAGESET, &m, &hm->branch, "Page Setup", FALSE);
        stdmenu_additem(sms, AMI_SMPRINT, &m, &hm->branch, "Print", TRUE);
        stdmenu_additem(sms, AMI_SMEXIT, &m, &hm->branch, "Exit", FALSE);
    }

    if (sms & (BIT(AMI_SMUNDO) | BIT(AMI_SMCUT) | BIT(AMI_SMPASTE) |
               BIT(AMI_SMDELETE) | BIT(AMI_SMFIND) | BIT(AMI_SMFINDNEXT) |
               BIT(AMI_SMREPLACE) | BIT(AMI_SMGOTO) | BIT(AMI_SMSELECTALL))) {
        stdmenu_new(&hm, 0, "Edit");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMUNDO, &m, &hm->branch, "Undo", TRUE);
        stdmenu_additem(sms, AMI_SMCUT, &m, &hm->branch, "Cut", FALSE);
        stdmenu_additem(sms, AMI_SMPASTE, &m, &hm->branch, "Paste", FALSE);
        stdmenu_additem(sms, AMI_SMDELETE, &m, &hm->branch, "Delete", TRUE);
        stdmenu_additem(sms, AMI_SMFIND, &m, &hm->branch, "Find", FALSE);
        stdmenu_additem(sms, AMI_SMFINDNEXT, &m, &hm->branch, "Find Next", FALSE);
        stdmenu_additem(sms, AMI_SMREPLACE, &m, &hm->branch, "Replace", FALSE);
        stdmenu_additem(sms, AMI_SMGOTO, &m, &hm->branch, "Goto", TRUE);
        stdmenu_additem(sms, AMI_SMSELECTALL, &m, &hm->branch, "Select All", FALSE);
    }

    while (pm) {
        m = pm;
        pm = pm->next;
        stdmenu_append(sm, m);
    }

    if (sms & (BIT(AMI_SMNEWWINDOW) | BIT(AMI_SMTILEHORIZ) | BIT(AMI_SMTILEVERT) |
               BIT(AMI_SMCASCADE) | BIT(AMI_SMCLOSEALL))) {
        stdmenu_new(&hm, 0, "Window");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMNEWWINDOW, &m, &hm->branch, "New Window", TRUE);
        stdmenu_additem(sms, AMI_SMTILEHORIZ, &m, &hm->branch, "Tile Horizontally", FALSE);
        stdmenu_additem(sms, AMI_SMTILEVERT, &m, &hm->branch, "Tile Vertically", FALSE);
        stdmenu_additem(sms, AMI_SMCASCADE, &m, &hm->branch, "Cascade", TRUE);
        stdmenu_additem(sms, AMI_SMCLOSEALL, &m, &hm->branch, "Close All", FALSE);
    }

    if (sms & (BIT(AMI_SMHELPTOPIC) | BIT(AMI_SMABOUT))) {
        stdmenu_new(&hm, 0, "Help");
        stdmenu_append(sm, hm);
        stdmenu_additem(sms, AMI_SMHELPTOPIC, &m, &hm->branch, "Help Topics", TRUE);
        stdmenu_additem(sms, AMI_SMABOUT, &m, &hm->branch, "About", FALSE);
    }
}

static ami_long getwinid_ivf(void)
{
    static ami_long next = 1;
    return next++;
}

static void focus_ivf(FILE* f)
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

static void event_ivf(FILE* f, ami_evtrec* er)
{
    pa_rawevent raw;
    do {
        pa_cocoa_process_ns_events();
        pa_cocoa_wait(&raw);
        translate_event(&raw, er);
        if (er->etype == ami_etterm) fend = TRUE;
        /* the program's handlers, master first, then the code's own; an
           event they take is not returned */
        er->handled = 1;
        if (evtshan) (*evtshan)(er); else er->handled = 0;
        if (!er->handled && er->etype >= 0 && er->etype <= ami_etdsize && evthan[er->etype]) {
            er->handled = 1;
            (*evthan[er->etype])(er);
        }
    } while (er->handled);
}

static void timer_ivf(FILE* f, ami_long i, ami_long t, ami_long r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_set_timer(win->han, i-1, t, r); /* PA timers 1-based */
}

static void killtimer_ivf(FILE* f, ami_long i)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_kill_timer(win->han, i-1);
}

static ami_long mouse_ivf(FILE* f)        { return 1; }  /* one mouse */
static ami_long mousebutton_ivf(FILE* f, ami_long m) { return 3; }  /* three buttons */
static ami_long joystick_ivf(FILE* f)         { return pa_cocoa_joy_count(); }
static ami_long joybutton_ivf(FILE* f, ami_long j) { return pa_cocoa_joy_buttons(j); }
static ami_long joyaxis_ivf(FILE* f, ami_long j)   { return pa_cocoa_joy_axes(j); }

/*******************************************************************************
*                                                                              *
*                        PA API — widgets                                      *
*                                                                              *
*******************************************************************************/

static ami_long getwigid_ivf(FILE* f)
{
    static ami_long next = 1;
    return next++;
}

static void killwidget_ivf(FILE* f, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_kill_widget(win->han, id);
}

static void selectwidget_ivf(FILE* f, ami_long id, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_select(win->han, id, e);
}

static void enablewidget_ivf(FILE* f, ami_long id, ami_long e)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_widget_enable(win->han, id, e);
}

static void getwidgettext_ivf(FILE* f, ami_long id, char* s, ami_long sl)
{
    winptr win = f2win(f); if (!win) return;
    /* fetch to a terminated local, then apply the critical buffer
       convention at the caller's buffer: a result filling the entire buffer
       is left unterminated, overflow is an error */
    char* buf = malloc(sl+2);
    if (!buf) return;
    buf[0] = 0;
    pa_cocoa_widget_get_text(win->han, id, buf, (int)(sl+2));
    cpycrit(s, sl, buf);
    free(buf);
}

static void putwidgettext_ivf(FILE* f, ami_long id, char* s)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_widget_text(win->han, id, s);
}

static void sizwidget_ivf(FILE* f, ami_long id, ami_long x, ami_long y)   { /* stub */ }
static void sizwidgetg_ivf(FILE* f, ami_long id, ami_long x, ami_long y)  { /* stub */ }
static void poswidget_ivf(FILE* f, ami_long id, ami_long x, ami_long y)   { /* stub */ }
static void poswidgetg_ivf(FILE* f, ami_long id, ami_long x, ami_long y)  { /* stub */ }
static void backwidget_ivf(FILE* f, ami_long id)                { /* stub */ }
static void frontwidget_ivf(FILE* f, ami_long id)               { /* stub */ }
static void focuswidget_ivf(FILE* f, ami_long id)               { /* stub */ }

/* The plain widget calls take CHARACTER coordinates/sizes; the -g calls
   take pixels. Character rects convert to pixel rects covering the full
   character cells (Windows semantics), and pixel sizes convert to
   character sizes rounding up. */

static void chr2grrect(winptr win, ami_long* x1, ami_long* y1, ami_long* x2, ami_long* y2)
{
    *x1 = (*x1 - 1) * win->charspace + 1;
    *y1 = (*y1 - 1) * win->linespace + 1;
    *x2 = (*x2) * win->charspace;
    *y2 = (*y2) * win->linespace;
}

static void gr2chrsiz(winptr win, ami_long* w, ami_long* h)
{
    *w = (*w - 1) / win->charspace + 1;
    *h = (*h - 1) / win->linespace + 1;
}

static void buttonsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{
    *w = ami_strsiz(f, s) + 20;
    *h = ami_chrsizy(f) + 8;
}

static void buttonsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_buttonsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

static void buttong_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_button(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

static void button_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_buttong(f, x1, y1, x2, y2, s, id);
}

static void checkboxsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{
    *w = ami_strsiz(f, s) + 24;
    *h = ami_chrsizy(f) + 4;
}

static void checkboxsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_checkboxsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

static void checkboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_checkbox(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

static void checkbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_checkboxg(f, x1, y1, x2, y2, s, id);
}

static void radiobuttonsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{
    *w = ami_strsiz(f, s) + 24;
    *h = ami_chrsizy(f) + 4;
}

static void radiobuttonsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_radiobuttonsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

static void radiobuttong_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_radiobutton(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

static void radiobutton_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_radiobuttong(f, x1, y1, x2, y2, s, id);
}

static void groupsizg_ivf(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
{
    /* the group must be at least wide enough for its title as NSBox
       renders it, and tall enough for the title plus the client area */
    int tw, th;
    pa_cocoa_group_title_size(s, &tw, &th);
    *w = tw + 7 * 2;
    if (cw + 7 * 2 > *w) *w = cw + 7 * 2;
    *h  = th + ch + 5 * 2;
    *ox = 5;
    *oy = th;
}

static void groupsiz_ivf(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; *ox = *oy = 0; return; }
    ami_groupsizg(f, s, cw * win->charspace, ch * win->linespace, w, h, ox, oy);
    gr2chrsiz(win, w, h);
    gr2chrsiz(win, ox, oy);
}

static void groupg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win || !s) return;
    pa_cocoa_group(win->han, x1, y1, x2-x1+1, y2-y1+1, s, id);
}

static void group_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_groupg(f, x1, y1, x2, y2, s, id);
}

static void backgroundg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_background(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

static void background_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_backgroundg(f, x1, y1, x2, y2, id);
}

static void scrollvertsizg_ivf(FILE* f, ami_long* w, ami_long* h)  { *w = 16; *h = 100; }

static void scrollvertsiz_ivf(FILE* f, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_scrollvertsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

static void scrollvertg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollvert(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

static void scrollvert_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_scrollvertg(f, x1, y1, x2, y2, id);
}

static void scrollhorizsizg_ivf(FILE* f, ami_long* w, ami_long* h)  { *w = 100; *h = 16; }

static void scrollhorizsiz_ivf(FILE* f, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_scrollhorizsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

static void scrollhorizg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollhoriz(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

static void scrollhoriz_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_scrollhorizg(f, x1, y1, x2, y2, id);
}

static void scrollpos_ivf(FILE* f, ami_long id, ami_long r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollbar_pos(win->han, id, r);
}

static void scrollsiz_ivf(FILE* f, ami_long id, ami_long r)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_scrollbar_siz(win->han, id, r);
}

static void numselboxsizg_ivf(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h) { *w = 80; *h = 24; }

static void numselboxsiz_ivf(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_numselboxsizg(f, l, u, w, h);
    gr2chrsiz(win, w, h);
}
static void numselboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_numselbox(win->han, x1, y1, x2-x1+1, y2-y1+1, l, u, id);
}

static void numselbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_numselboxg(f, x1, y1, x2, y2, l, u, id);
}

static void editboxsizg_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{ *w = ami_strsiz(f, s) + 20; *h = ami_chrsizy(f) + 8; }

static void editboxsiz_ivf(FILE* f, char* s, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_editboxsizg(f, s, w, h);
    gr2chrsiz(win, w, h);
}

static void editboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_editbox(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

static void editbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_editboxg(f, x1, y1, x2, y2, id);
}

static void progbarsizg_ivf(FILE* f, ami_long* w, ami_long* h) { *w = 200; *h = 20; }

static void progbarsiz_ivf(FILE* f, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_progbarsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

static void progbarg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_progressbar(win->han, x1, y1, x2-x1+1, y2-y1+1, id);
}

static void progbar_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_progbarg(f, x1, y1, x2, y2, id);
}

static void progbarpos_ivf(FILE* f, ami_long id, ami_long pos)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_progressbar_pos(win->han, id, pos);
}

static void listboxsizg_ivf(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
{
    /* wide enough for the longest entry plus scroller and border, tall
       enough for all entries at the native ~18px row height */
    ami_long mw = 0;
    int  n  = 0;
    for (ami_strptr p = sp; p; p = p->next) {
        ami_long sw = ami_strsiz(f, p->str);
        if (sw > mw) mw = sw;
        n++;
    }
    *w = mw + 40;
    *h = n * 18 + 6;
}

static void listboxsiz_ivf(FILE* f, ami_strptr sp, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_listboxsizg(f, sp, w, h);
    gr2chrsiz(win, w, h);
}

static void listboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_listbox(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n, id);
    free(strs);
}

static void listbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_listboxg(f, x1, y1, x2, y2, sp, id);
}

static void dropboxsizg_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
{ *cw = 150; *ch = 24; *ow = 150; *oh = 100; }

static void dropboxsiz_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
{
    winptr win = f2win(f); if (!win) { *cw = *ch = *ow = *oh = 1; return; }
    ami_dropboxsizg(f, sp, cw, ch, ow, oh);
    gr2chrsiz(win, cw, ch);
    gr2chrsiz(win, ow, oh);
}

static void dropboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_dropbox(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n, id);
    free(strs);
}

static void dropbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_dropboxg(f, x1, y1, x2, y2, sp, id);
}

static void dropeditboxsizg_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
{ ami_dropboxsizg(f, sp, cw, ch, ow, oh); }

static void dropeditboxsiz_ivf(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh)
{
    winptr win = f2win(f); if (!win) { *cw = *ch = *ow = *oh = 1; return; }
    ami_dropeditboxsizg(f, sp, cw, ch, ow, oh);
    gr2chrsiz(win, cw, ch);
    gr2chrsiz(win, ow, oh);
}

static void dropeditboxg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_dropeditbox(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n, id);
    free(strs);
}

static void dropeditbox_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_dropeditboxg(f, x1, y1, x2, y2, sp, id);
}

static void slidehorizsizg_ivf(FILE* f, ami_long* w, ami_long* h) { *w = 150; *h = 20; }

static void slidehorizsiz_ivf(FILE* f, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_slidehorizsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

static void slidehorizg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_slider_horiz(win->han, x1, y1, x2-x1+1, y2-y1+1, mark, id);
}

static void slidehoriz_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_slidehorizg(f, x1, y1, x2, y2, mark, id);
}

static void slidevertsizg_ivf(FILE* f, ami_long* w, ami_long* h) { *w = 20; *h = 150; }

static void slidevertsiz_ivf(FILE* f, ami_long* w, ami_long* h)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; return; }
    ami_slidevertsizg(f, w, h);
    gr2chrsiz(win, w, h);
}

static void slidevertg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_slider_vert(win->han, x1, y1, x2-x1+1, y2-y1+1, mark, id);
}

static void slidevert_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    ami_slidevertg(f, x1, y1, x2, y2, mark, id);
}

/* tab bar height, matching the shim's PATabBar bar strip */
#define TABBAR_H 24

/* size of a tab panel for the given client size: the bar adds its height
   on the oriented side; (ox,oy) is the client offset within the panel */
static void tabbarsizg_ivf(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
{
    switch (tor) {
    case ami_totop:
        *w = cw; *h = ch + TABBAR_H; *ox = 0; *oy = TABBAR_H; break;
    case ami_tobottom:
        *w = cw; *h = ch + TABBAR_H; *ox = 0; *oy = 0; break;
    case ami_toleft:
        *w = cw + TABBAR_H; *h = ch; *ox = TABBAR_H; *oy = 0; break;
    default: /* ami_toright */
        *w = cw + TABBAR_H; *h = ch; *ox = 0; *oy = 0; break;
    }
}

static void tabbarsiz_ivf(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy)
{
    winptr win = f2win(f); if (!win) { *w = *h = 1; *ox = *oy = 0; return; }
    ami_tabbarsizg(f, sp, tor, cw * win->charspace, ch * win->linespace,
                   w, h, ox, oy);
    gr2chrsiz(win, w, h);
    /* offsets round up to whole cells so the client clears the bar */
    *ox = (*ox + win->charspace - 1) / win->charspace;
    *oy = (*oy + win->linespace - 1) / win->linespace;
}

static void tabbarclientg_ivf(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy)
{
    switch (tor) {
    case ami_totop:
        *cw = w; *ch = h - TABBAR_H; *ox = 0; *oy = TABBAR_H; break;
    case ami_tobottom:
        *cw = w; *ch = h - TABBAR_H; *ox = 0; *oy = 0; break;
    case ami_toleft:
        *cw = w - TABBAR_H; *ch = h; *ox = TABBAR_H; *oy = 0; break;
    default: /* ami_toright */
        *cw = w - TABBAR_H; *ch = h; *ox = 0; *oy = 0; break;
    }
    if (*cw < 1) *cw = 1;
    if (*ch < 1) *ch = 1;
}

static void tabbarclient_ivf(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy)
{
    winptr win = f2win(f); if (!win) { *cw = *ch = 1; *ox = *oy = 0; return; }
    ami_tabbarclientg(f, tor, w * win->charspace, h * win->linespace,
                      cw, ch, ox, oy);
    gr2chrsiz(win, cw, ch);
    *ox = (*ox + win->charspace - 1) / win->charspace;
    *oy = (*oy + win->linespace - 1) / win->linespace;
}

static void tabbar_common(winptr win, ami_long x1, ami_long y1, ami_long x2, ami_long y2,
                          ami_strptr sp, ami_tabori tor, ami_long barh, ami_long id)
{
    int n = 0;
    for (ami_strptr p = sp; p; p = p->next) n++;
    if (!n) return;
    const char** strs = malloc(n * sizeof(char*));
    if (!strs) return;
    int i = 0;
    for (ami_strptr p = sp; p; p = p->next) strs[i++] = p->str;
    pa_cocoa_tabbar(win->han, x1, y1, x2-x1+1, y2-y1+1, strs, n,
                    (int)tor, barh, id);
    free(strs);
}

static void tabbarg_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    tabbar_common(win, x1, y1, x2, y2, sp, tor, TABBAR_H, id);
}

static void tabbar_ivf(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id)
{
    winptr win = f2win(f); if (!win) return;
    /* character metrics rounded the bar up to whole cells; use that same
       thickness so the client area lands exactly on character cells */
    ami_long barh;
    if (tor == ami_totop || tor == ami_tobottom)
        barh = (TABBAR_H + win->linespace - 1) / win->linespace
               * win->linespace;
    else
        barh = (TABBAR_H + win->charspace - 1) / win->charspace
               * win->charspace;
    chr2grrect(win, &x1, &y1, &x2, &y2);
    tabbar_common(win, x1, y1, x2, y2, sp, tor, barh, id);
}

static void tabsel_ivf(FILE* f, ami_long id, ami_long tn)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_tabbar_sel(win->han, id, tn);
}

/*******************************************************************************
*                                                                              *
*                        PA API — dialogs                                      *
*                                                                              *
* String buffers passed to these dialogs are critical buffers: a result that  *
* fills the entire buffer is left without a terminating zero, a shorter       *
* result is zero terminated, and it is an error if the result cannot fit.     *
*                                                                              *
*******************************************************************************/

static void alert_ivf(char* title, char* message)
{
    pa_cocoa_alert(title, message);
}

static void querycolor_ivf(ami_long* r, ami_long* g, ami_long* b)
{
    pa_cocoa_query_color(r, g, b);
}

static void queryopen_ivf(char* s, ami_long sl)
{
    /* the bridge fills a terminated local; the critical copy happens at the
       caller's buffer */
    char* buf = malloc(sl+2);
    if (!buf) return;
    buf[0] = 0;
    pa_cocoa_query_open(buf, (int)(sl+2));
    cpycrit(s, sl, buf);
    free(buf);
}

static void querysave_ivf(char* s, ami_long sl)
{
    char* buf = malloc(sl+2);
    if (!buf) return;
    buf[0] = 0;
    pa_cocoa_query_save(buf, (int)(sl+2));
    cpycrit(s, sl, buf);
    free(buf);
}

static void queryfind_ivf(char* s, ami_long sl, ami_qfnopts* opt)
{
    char* buf = malloc(sl+2);
    ami_long l = 0;
    if (!buf) return;
    /* the input seed may fill the caller's buffer without a terminator */
    while (l < sl && s[l]) l++;
    memcpy(buf, s, l); buf[l] = 0;
    pa_cocoa_query_find(buf, (int)(sl+2), opt);
    cpycrit(s, sl, buf);
    free(buf);
}

static void queryfindrep_ivf(char* s, ami_long sl, char* r, ami_long rl, ami_qfropts* opt)
{
    char* fbuf = malloc(sl+2);
    char* rbuf = malloc(rl+2);
    ami_long l;
    if (!fbuf || !rbuf) { free(fbuf); free(rbuf); return; }
    /* the input seeds may fill the caller's buffers without terminators */
    l = 0; while (l < sl && s[l]) l++;
    memcpy(fbuf, s, l); fbuf[l] = 0;
    l = 0; while (l < rl && r[l]) l++;
    memcpy(rbuf, r, l); rbuf[l] = 0;
    pa_cocoa_query_findrep(fbuf, (int)(sl+2), rbuf, (int)(rl+2), opt);
    cpycrit(s, sl, fbuf);
    cpycrit(r, rl, rbuf);
    free(fbuf); free(rbuf);
}

static void queryfont_ivf(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg,
                   ami_long* fb, ami_long* br, ami_long* bg, ami_long* bb,
                   ami_qfteffects* effect)
{
    char family[128] = "";
    int  n;

    /* seed the dialog with the current font code's family name */
    n = 1;
    for (fontptr fp = fntlst; fp; fp = fp->next, n++)
        if (n == *fc && fp->name) {
            strncpy(family, fp->name, sizeof(family) - 1);
            break;
        }

    pa_cocoa_query_font(family, sizeof(family), s,
                        fr, fg, fb, br, bg, bb);

    /* map the returned family name back to a PA font code; if the family
       is not in the font list, leave the code unchanged */
    n = 1;
    for (fontptr fp = fntlst; fp; fp = fp->next, n++)
        if (fp->name && !strcasecmp(fp->name, family)) {
            *fc = n;
            break;
        }
    (void)effect; /* text effects unchanged */
}


/* ---------------------------------------------------------------------------
   Override vector. Generated from include/graphics.h by gen_ovr.py: do not
   edit by hand, regenerate. Each API entry dispatches through a pointer that
   _pa_X_ovr swaps, so layered modules (pdfgraph, windowg, graph_server) can
   intercept calls. The pointer is bound at compile time to the backend
   implementation, X_ivf, so no entry can run through NULL before init.
   --------------------------------------------------------------------------- */


/* ---- backend entries supplied by ports.c ---- */
/* ---- Backend entries the Cocoa port lacked. The vector below binds to these
   by default; they are written to the same shape as the entries above. ---- */

/* Physical to logical coordinate, undoing the viewport transform that
   viewoffg and viewscale set: the x11 backend's scalex/scaley, on the
   per-screen viewport state this backend keeps. The forward transform is
   off + scale*x, so this is (x - off) / scale; a zero scale is guarded as
   x11 does. */
static ami_long scalex_ivf(FILE* f, ami_long x)
{
    winptr win = f2win(f); if (!win) return x;
    scnptr sc = curscn(win);
    if (sc->scalex == 0.0f) return x;
    return (ami_long)((float)(x - sc->offx) / sc->scalex);
}

static ami_long scaley_ivf(FILE* f, ami_long y)
{
    winptr win = f2win(f); if (!win) return y;
    scnptr sc = curscn(win);
    if (sc->scaley == 0.0f) return y;
    return (ami_long)((float)(y - sc->offy) / sc->scaley);
}

/* Copy a block of pixels from screen s to screen d (1 based), through the
   update screen's write mode. The screen bitmaps are plain y-up CG bitmaps;
   the flip that gives PA its top-left origin lives in each context's CTM.
   So the crop is taken in raw pixel space, with y counted up from the
   bottom, and the draw resets the destination CTM to identity so the block
   lands right side up. Normal mode is an exact pixel copy; the other modes
   go through the same blend mapping the primitives use. */
static void blockcopyg_ivf(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1,
                           ami_long sx2, ami_long sy2, ami_long dx1, ami_long dy1,
                           ami_long dx2, ami_long dy2)
{
    winptr   win = f2win(f); if (!win) return;
    scnptr   cs  = curscn(win);
    ami_long t;

    if (cs->fmod == mdinvis) return;
    if (s < 1 || s > MAXCON || d < 1 || d > MAXCON) return;
    if (sx1 > sx2) { t = sx1; sx1 = sx2; sx2 = t; }
    if (sy1 > sy2) { t = sy1; sy1 = sy2; sy2 = t; }
    if (dx1 > dx2) { t = dx1; dx1 = dx2; dx2 = t; }
    if (dy1 > dy2) { t = dy1; dy1 = dy2; dy2 = t; }

    CGContextRef src = pa_cocoa_get_screen_context(win->han, (int)s - 1);
    CGContextRef dst = pa_cocoa_get_screen_context(win->han, (int)d - 1);
    if (!src || !dst) return;
    CGFloat sh = (CGFloat)CGBitmapContextGetHeight(src);

    CGImageRef whole = CGBitmapContextCreateImage(src);
    if (!whole) return;
    CGImageRef blk = CGImageCreateWithImageInRect(whole,
        CGRectMake(PX(sx1), sh - sy2, sx2 - sx1 + 1, sy2 - sy1 + 1));
    CGImageRelease(whole);
    if (!blk) return;

    CGContextSaveGState(dst);
    CGContextConcatCTM(dst, CGAffineTransformInvert(CGContextGetCTM(dst)));
    CGContextSetBlendMode(dst, cs->fmod == mdnorm ? kCGBlendModeCopy
                                                  : mode2blend(cs->fmod));
    /* The destination is given in the surface's own terms, top down: the
       block drew mirrored about the middle of the surface, so that a copy
       placed below a figure landed above it (the copies of window_test's
       block copy frame that sit level with their source hid this, being
       their own mirror image). */
    CGContextDrawImage(dst,
        CGRectMake(PX(dx1), PX(dy1), dx2 - dx1 + 1, dy2 - dy1 + 1), blk);
    CGContextRestoreGState(dst);
    CGImageRelease(blk);
    pa_cocoa_flush(win->han);
}

/* Let the user drag the window from the current mouse-down, as a title bar
   drag would; Cocoa tracks it natively. graphics.h declares no
   _pa_dragwin_ovr, so this is the public entry itself, not a vector
   default. */
void ami_dragwin(FILE* f)
{
    winptr win = f2win(f); if (!win) return;
    pa_cocoa_drag_window(win->han);
}

static ami_alert_t alert_vect = alert_ivf;
void _pa_alert_ovr(ami_alert_t nfp, ami_alert_t* ofp) { *ofp = alert_vect; alert_vect = nfp; }
void ami_alert(char* title, char* message) { (*alert_vect)(title, message); }

static ami_arc_t arc_vect = arc_ivf;
void _pa_arc_ovr(ami_arc_t nfp, ami_arc_t* ofp) { *ofp = arc_vect; arc_vect = nfp; }
void ami_arc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { (*arc_vect)(f, x1, y1, x2, y2, sa, ea); }

static ami_auto_t auto_vect = auto_ivf;
void _pa_auto_ovr(ami_auto_t nfp, ami_auto_t* ofp) { *ofp = auto_vect; auto_vect = nfp; }
void ami_auto(FILE* f, ami_long e) { (*auto_vect)(f, e); }

static ami_autohold_t autohold_vect = autohold_ivf;
void _pa_autohold_ovr(ami_autohold_t nfp, ami_autohold_t* ofp) { *ofp = autohold_vect; autohold_vect = nfp; }
void ami_autohold(ami_long e) { (*autohold_vect)(e); }

static ami_back_t back_vect = back_ivf;
void _pa_back_ovr(ami_back_t nfp, ami_back_t* ofp) { *ofp = back_vect; back_vect = nfp; }
void ami_back(FILE* f) { (*back_vect)(f); }

static ami_background_t background_vect = background_ivf;
void _pa_background_ovr(ami_background_t nfp, ami_background_t* ofp) { *ofp = background_vect; background_vect = nfp; }
void ami_background(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*background_vect)(f, x1, y1, x2, y2, id); }

static ami_backgroundg_t backgroundg_vect = backgroundg_ivf;
void _pa_backgroundg_ovr(ami_backgroundg_t nfp, ami_backgroundg_t* ofp) { *ofp = backgroundg_vect; backgroundg_vect = nfp; }
void ami_backgroundg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*backgroundg_vect)(f, x1, y1, x2, y2, id); }

static ami_backwidget_t backwidget_vect = backwidget_ivf;
void _pa_backwidget_ovr(ami_backwidget_t nfp, ami_backwidget_t* ofp) { *ofp = backwidget_vect; backwidget_vect = nfp; }
void ami_backwidget(FILE* f, ami_long id) { (*backwidget_vect)(f, id); }

static ami_band_t band_vect = band_ivf;
void _pa_band_ovr(ami_band_t nfp, ami_band_t* ofp) { *ofp = band_vect; band_vect = nfp; }
void ami_band(FILE* f) { (*band_vect)(f); }

static ami_baseline_t baseline_vect = baseline_ivf;
void _pa_baseline_ovr(ami_baseline_t nfp, ami_baseline_t* ofp) { *ofp = baseline_vect; baseline_vect = nfp; }
ami_long ami_baseline(FILE* f) { return (*baseline_vect)(f); }

static ami_bcolor_t bcolor_vect = bcolor_ivf;
void _pa_bcolor_ovr(ami_bcolor_t nfp, ami_bcolor_t* ofp) { *ofp = bcolor_vect; bcolor_vect = nfp; }
void ami_bcolor(FILE* f, ami_color c) { (*bcolor_vect)(f, c); }

static ami_bcolorc_t bcolorc_vect = bcolorc_ivf;
void _pa_bcolorc_ovr(ami_bcolorc_t nfp, ami_bcolorc_t* ofp) { *ofp = bcolorc_vect; bcolorc_vect = nfp; }
void ami_bcolorc(FILE* f, ami_long r, ami_long g, ami_long b) { (*bcolorc_vect)(f, r, g, b); }

static ami_bcolorg_t bcolorg_vect = bcolorg_ivf;
void _pa_bcolorg_ovr(ami_bcolorg_t nfp, ami_bcolorg_t* ofp) { *ofp = bcolorg_vect; bcolorg_vect = nfp; }
void ami_bcolorg(FILE* f, ami_long r, ami_long g, ami_long b) { (*bcolorg_vect)(f, r, g, b); }

static ami_binvis_t binvis_vect = binvis_ivf;
void _pa_binvis_ovr(ami_binvis_t nfp, ami_binvis_t* ofp) { *ofp = binvis_vect; binvis_vect = nfp; }
void ami_binvis(FILE* f) { (*binvis_vect)(f); }

static ami_blink_t blink_vect = blink_ivf;
void _pa_blink_ovr(ami_blink_t nfp, ami_blink_t* ofp) { *ofp = blink_vect; blink_vect = nfp; }
void ami_blink(FILE* f, ami_long e) { (*blink_vect)(f, e); }

static ami_blockcopyg_t blockcopyg_vect = blockcopyg_ivf;
void _pa_blockcopyg_ovr(ami_blockcopyg_t nfp, ami_blockcopyg_t* ofp) { *ofp = blockcopyg_vect; blockcopyg_vect = nfp; }
void ami_blockcopyg(FILE* f, ami_long s, ami_long d, ami_long sx1, ami_long sy1, ami_long sx2, ami_long sy2, ami_long dx1, ami_long dy1, ami_long dx2, ami_long dy2) { (*blockcopyg_vect)(f, s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2, dy2); }

static ami_bold_t bold_vect = bold_ivf;
void _pa_bold_ovr(ami_bold_t nfp, ami_bold_t* ofp) { *ofp = bold_vect; bold_vect = nfp; }
void ami_bold(FILE* f, ami_long e) { (*bold_vect)(f, e); }

static ami_bor_t bor_vect = bor_ivf;
void _pa_bor_ovr(ami_bor_t nfp, ami_bor_t* ofp) { *ofp = bor_vect; bor_vect = nfp; }
void ami_bor(FILE* f) { (*bor_vect)(f); }

static ami_bover_t bover_vect = bover_ivf;
void _pa_bover_ovr(ami_bover_t nfp, ami_bover_t* ofp) { *ofp = bover_vect; bover_vect = nfp; }
void ami_bover(FILE* f) { (*bover_vect)(f); }

static ami_buffer_t buffer_vect = buffer_ivf;
void _pa_buffer_ovr(ami_buffer_t nfp, ami_buffer_t* ofp) { *ofp = buffer_vect; buffer_vect = nfp; }
void ami_buffer(FILE* f, ami_long e) { (*buffer_vect)(f, e); }

static ami_button_t button_vect = button_ivf;
void _pa_button_ovr(ami_button_t nfp, ami_button_t* ofp) { *ofp = button_vect; button_vect = nfp; }
void ami_button(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*button_vect)(f, x1, y1, x2, y2, s, id); }

static ami_buttong_t buttong_vect = buttong_ivf;
void _pa_buttong_ovr(ami_buttong_t nfp, ami_buttong_t* ofp) { *ofp = buttong_vect; buttong_vect = nfp; }
void ami_buttong(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*buttong_vect)(f, x1, y1, x2, y2, s, id); }

static ami_buttonsiz_t buttonsiz_vect = buttonsiz_ivf;
void _pa_buttonsiz_ovr(ami_buttonsiz_t nfp, ami_buttonsiz_t* ofp) { *ofp = buttonsiz_vect; buttonsiz_vect = nfp; }
void ami_buttonsiz(FILE* f, char* s, ami_long* w, ami_long* h) { (*buttonsiz_vect)(f, s, w, h); }

static ami_buttonsizg_t buttonsizg_vect = buttonsizg_ivf;
void _pa_buttonsizg_ovr(ami_buttonsizg_t nfp, ami_buttonsizg_t* ofp) { *ofp = buttonsizg_vect; buttonsizg_vect = nfp; }
void ami_buttonsizg(FILE* f, char* s, ami_long* w, ami_long* h) { (*buttonsizg_vect)(f, s, w, h); }

static ami_bxor_t bxor_vect = bxor_ivf;
void _pa_bxor_ovr(ami_bxor_t nfp, ami_bxor_t* ofp) { *ofp = bxor_vect; bxor_vect = nfp; }
void ami_bxor(FILE* f) { (*bxor_vect)(f); }

static ami_checkbox_t checkbox_vect = checkbox_ivf;
void _pa_checkbox_ovr(ami_checkbox_t nfp, ami_checkbox_t* ofp) { *ofp = checkbox_vect; checkbox_vect = nfp; }
void ami_checkbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*checkbox_vect)(f, x1, y1, x2, y2, s, id); }

static ami_checkboxg_t checkboxg_vect = checkboxg_ivf;
void _pa_checkboxg_ovr(ami_checkboxg_t nfp, ami_checkboxg_t* ofp) { *ofp = checkboxg_vect; checkboxg_vect = nfp; }
void ami_checkboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*checkboxg_vect)(f, x1, y1, x2, y2, s, id); }

static ami_checkboxsiz_t checkboxsiz_vect = checkboxsiz_ivf;
void _pa_checkboxsiz_ovr(ami_checkboxsiz_t nfp, ami_checkboxsiz_t* ofp) { *ofp = checkboxsiz_vect; checkboxsiz_vect = nfp; }
void ami_checkboxsiz(FILE* f, char* s, ami_long* w, ami_long* h) { (*checkboxsiz_vect)(f, s, w, h); }

static ami_checkboxsizg_t checkboxsizg_vect = checkboxsizg_ivf;
void _pa_checkboxsizg_ovr(ami_checkboxsizg_t nfp, ami_checkboxsizg_t* ofp) { *ofp = checkboxsizg_vect; checkboxsizg_vect = nfp; }
void ami_checkboxsizg(FILE* f, char* s, ami_long* w, ami_long* h) { (*checkboxsizg_vect)(f, s, w, h); }

static ami_chrpos_t chrpos_vect = chrpos_ivf;
void _pa_chrpos_ovr(ami_chrpos_t nfp, ami_chrpos_t* ofp) { *ofp = chrpos_vect; chrpos_vect = nfp; }
ami_long ami_chrpos(FILE* f, const char* s, ami_long p) { return (*chrpos_vect)(f, s, p); }

static ami_chrsizx_t chrsizx_vect = chrsizx_ivf;
void _pa_chrsizx_ovr(ami_chrsizx_t nfp, ami_chrsizx_t* ofp) { *ofp = chrsizx_vect; chrsizx_vect = nfp; }
ami_long ami_chrsizx(FILE* f) { return (*chrsizx_vect)(f); }

static ami_chrsizy_t chrsizy_vect = chrsizy_ivf;
void _pa_chrsizy_ovr(ami_chrsizy_t nfp, ami_chrsizy_t* ofp) { *ofp = chrsizy_vect; chrsizy_vect = nfp; }
ami_long ami_chrsizy(FILE* f) { return (*chrsizy_vect)(f); }

static ami_chrspcx_t chrspcx_vect = chrspcx_ivf;
void _pa_chrspcx_ovr(ami_chrspcx_t nfp, ami_chrspcx_t* ofp) { *ofp = chrspcx_vect; chrspcx_vect = nfp; }
void ami_chrspcx(FILE* f, ami_long s) { (*chrspcx_vect)(f, s); }

static ami_chrspcy_t chrspcy_vect = chrspcy_ivf;
void _pa_chrspcy_ovr(ami_chrspcy_t nfp, ami_chrspcy_t* ofp) { *ofp = chrspcy_vect; chrspcy_vect = nfp; }
void ami_chrspcy(FILE* f, ami_long s) { (*chrspcy_vect)(f, s); }

static ami_clrtab_t clrtab_vect = clrtab_ivf;
void _pa_clrtab_ovr(ami_clrtab_t nfp, ami_clrtab_t* ofp) { *ofp = clrtab_vect; clrtab_vect = nfp; }
void ami_clrtab(FILE* f) { (*clrtab_vect)(f); }

static ami_condensed_t condensed_vect = condensed_ivf;
void _pa_condensed_ovr(ami_condensed_t nfp, ami_condensed_t* ofp) { *ofp = condensed_vect; condensed_vect = nfp; }
void ami_condensed(FILE* f, ami_long e) { (*condensed_vect)(f, e); }

static ami_curbnd_t curbnd_vect = curbnd_ivf;
void _pa_curbnd_ovr(ami_curbnd_t nfp, ami_curbnd_t* ofp) { *ofp = curbnd_vect; curbnd_vect = nfp; }
ami_long ami_curbnd(FILE* f) { return (*curbnd_vect)(f); }

static ami_cursor_t cursor_vect = cursor_ivf;
void _pa_cursor_ovr(ami_cursor_t nfp, ami_cursor_t* ofp) { *ofp = cursor_vect; cursor_vect = nfp; }
void ami_cursor(FILE* f, ami_long x, ami_long y) { (*cursor_vect)(f, x, y); }

static ami_cursorg_t cursorg_vect = cursorg_ivf;
void _pa_cursorg_ovr(ami_cursorg_t nfp, ami_cursorg_t* ofp) { *ofp = cursorg_vect; cursorg_vect = nfp; }
void ami_cursorg(FILE* f, ami_long x, ami_long y) { (*cursorg_vect)(f, x, y); }

static ami_curvis_t curvis_vect = curvis_ivf;
void _pa_curvis_ovr(ami_curvis_t nfp, ami_curvis_t* ofp) { *ofp = curvis_vect; curvis_vect = nfp; }
void ami_curvis(FILE* f, ami_long e) { (*curvis_vect)(f, e); }

static ami_curx_t curx_vect = curx_ivf;
void _pa_curx_ovr(ami_curx_t nfp, ami_curx_t* ofp) { *ofp = curx_vect; curx_vect = nfp; }
ami_long ami_curx(FILE* f) { return (*curx_vect)(f); }

static ami_curxg_t curxg_vect = curxg_ivf;
void _pa_curxg_ovr(ami_curxg_t nfp, ami_curxg_t* ofp) { *ofp = curxg_vect; curxg_vect = nfp; }
ami_long ami_curxg(FILE* f) { return (*curxg_vect)(f); }

static ami_cury_t cury_vect = cury_ivf;
void _pa_cury_ovr(ami_cury_t nfp, ami_cury_t* ofp) { *ofp = cury_vect; cury_vect = nfp; }
ami_long ami_cury(FILE* f) { return (*cury_vect)(f); }

static ami_curyg_t curyg_vect = curyg_ivf;
void _pa_curyg_ovr(ami_curyg_t nfp, ami_curyg_t* ofp) { *ofp = curyg_vect; curyg_vect = nfp; }
ami_long ami_curyg(FILE* f) { return (*curyg_vect)(f); }

static ami_del_t del_vect = del_ivf;
void _pa_del_ovr(ami_del_t nfp, ami_del_t* ofp) { *ofp = del_vect; del_vect = nfp; }
void ami_del(FILE* f) { (*del_vect)(f); }

static ami_delpict_t delpict_vect = delpict_ivf;
void _pa_delpict_ovr(ami_delpict_t nfp, ami_delpict_t* ofp) { *ofp = delpict_vect; delpict_vect = nfp; }
void ami_delpict(FILE* f, ami_long p) { (*delpict_vect)(f, p); }

static ami_down_t down_vect = down_ivf;
void _pa_down_ovr(ami_down_t nfp, ami_down_t* ofp) { *ofp = down_vect; down_vect = nfp; }
void ami_down(FILE* f) { (*down_vect)(f); }

static ami_dpmx_t dpmx_vect = dpmx_ivf;
void _pa_dpmx_ovr(ami_dpmx_t nfp, ami_dpmx_t* ofp) { *ofp = dpmx_vect; dpmx_vect = nfp; }
ami_long ami_dpmx(FILE* f) { return (*dpmx_vect)(f); }

static ami_dpmy_t dpmy_vect = dpmy_ivf;
void _pa_dpmy_ovr(ami_dpmy_t nfp, ami_dpmy_t* ofp) { *ofp = dpmy_vect; dpmy_vect = nfp; }
ami_long ami_dpmy(FILE* f) { return (*dpmy_vect)(f); }

static ami_dropbox_t dropbox_vect = dropbox_ivf;
void _pa_dropbox_ovr(ami_dropbox_t nfp, ami_dropbox_t* ofp) { *ofp = dropbox_vect; dropbox_vect = nfp; }
void ami_dropbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id) { (*dropbox_vect)(f, x1, y1, x2, y2, sp, id); }

static ami_dropboxg_t dropboxg_vect = dropboxg_ivf;
void _pa_dropboxg_ovr(ami_dropboxg_t nfp, ami_dropboxg_t* ofp) { *ofp = dropboxg_vect; dropboxg_vect = nfp; }
void ami_dropboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id) { (*dropboxg_vect)(f, x1, y1, x2, y2, sp, id); }

static ami_dropboxsiz_t dropboxsiz_vect = dropboxsiz_ivf;
void _pa_dropboxsiz_ovr(ami_dropboxsiz_t nfp, ami_dropboxsiz_t* ofp) { *ofp = dropboxsiz_vect; dropboxsiz_vect = nfp; }
void ami_dropboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { (*dropboxsiz_vect)(f, sp, cw, ch, ow, oh); }

static ami_dropboxsizg_t dropboxsizg_vect = dropboxsizg_ivf;
void _pa_dropboxsizg_ovr(ami_dropboxsizg_t nfp, ami_dropboxsizg_t* ofp) { *ofp = dropboxsizg_vect; dropboxsizg_vect = nfp; }
void ami_dropboxsizg(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { (*dropboxsizg_vect)(f, sp, cw, ch, ow, oh); }

static ami_dropeditbox_t dropeditbox_vect = dropeditbox_ivf;
void _pa_dropeditbox_ovr(ami_dropeditbox_t nfp, ami_dropeditbox_t* ofp) { *ofp = dropeditbox_vect; dropeditbox_vect = nfp; }
void ami_dropeditbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id) { (*dropeditbox_vect)(f, x1, y1, x2, y2, sp, id); }

static ami_dropeditboxg_t dropeditboxg_vect = dropeditboxg_ivf;
void _pa_dropeditboxg_ovr(ami_dropeditboxg_t nfp, ami_dropeditboxg_t* ofp) { *ofp = dropeditboxg_vect; dropeditboxg_vect = nfp; }
void ami_dropeditboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id) { (*dropeditboxg_vect)(f, x1, y1, x2, y2, sp, id); }

static ami_dropeditboxsiz_t dropeditboxsiz_vect = dropeditboxsiz_ivf;
void _pa_dropeditboxsiz_ovr(ami_dropeditboxsiz_t nfp, ami_dropeditboxsiz_t* ofp) { *ofp = dropeditboxsiz_vect; dropeditboxsiz_vect = nfp; }
void ami_dropeditboxsiz(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { (*dropeditboxsiz_vect)(f, sp, cw, ch, ow, oh); }

static ami_dropeditboxsizg_t dropeditboxsizg_vect = dropeditboxsizg_ivf;
void _pa_dropeditboxsizg_ovr(ami_dropeditboxsizg_t nfp, ami_dropeditboxsizg_t* ofp) { *ofp = dropeditboxsizg_vect; dropeditboxsizg_vect = nfp; }
void ami_dropeditboxsizg(FILE* f, ami_strptr sp, ami_long* cw, ami_long* ch, ami_long* ow, ami_long* oh) { (*dropeditboxsizg_vect)(f, sp, cw, ch, ow, oh); }

static ami_editbox_t editbox_vect = editbox_ivf;
void _pa_editbox_ovr(ami_editbox_t nfp, ami_editbox_t* ofp) { *ofp = editbox_vect; editbox_vect = nfp; }
void ami_editbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*editbox_vect)(f, x1, y1, x2, y2, id); }

static ami_editboxg_t editboxg_vect = editboxg_ivf;
void _pa_editboxg_ovr(ami_editboxg_t nfp, ami_editboxg_t* ofp) { *ofp = editboxg_vect; editboxg_vect = nfp; }
void ami_editboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*editboxg_vect)(f, x1, y1, x2, y2, id); }

static ami_editboxsiz_t editboxsiz_vect = editboxsiz_ivf;
void _pa_editboxsiz_ovr(ami_editboxsiz_t nfp, ami_editboxsiz_t* ofp) { *ofp = editboxsiz_vect; editboxsiz_vect = nfp; }
void ami_editboxsiz(FILE* f, char* s, ami_long* w, ami_long* h) { (*editboxsiz_vect)(f, s, w, h); }

static ami_editboxsizg_t editboxsizg_vect = editboxsizg_ivf;
void _pa_editboxsizg_ovr(ami_editboxsizg_t nfp, ami_editboxsizg_t* ofp) { *ofp = editboxsizg_vect; editboxsizg_vect = nfp; }
void ami_editboxsizg(FILE* f, char* s, ami_long* w, ami_long* h) { (*editboxsizg_vect)(f, s, w, h); }

static ami_ellipse_t ellipse_vect = ellipse_ivf;
void _pa_ellipse_ovr(ami_ellipse_t nfp, ami_ellipse_t* ofp) { *ofp = ellipse_vect; ellipse_vect = nfp; }
void ami_ellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { (*ellipse_vect)(f, x1, y1, x2, y2); }

static ami_enablewidget_t enablewidget_vect = enablewidget_ivf;
void _pa_enablewidget_ovr(ami_enablewidget_t nfp, ami_enablewidget_t* ofp) { *ofp = enablewidget_vect; enablewidget_vect = nfp; }
void ami_enablewidget(FILE* f, ami_long id, ami_long e) { (*enablewidget_vect)(f, id, e); }

static ami_event_t event_vect = event_ivf;
void _pa_event_ovr(ami_event_t nfp, ami_event_t* ofp) { *ofp = event_vect; event_vect = nfp; }
void ami_event(FILE* f, ami_evtrec* er) { (*event_vect)(f, er); }

static ami_eventover_t eventover_vect = eventover_ivf;
void _pa_eventover_ovr(ami_eventover_t nfp, ami_eventover_t* ofp) { *ofp = eventover_vect; eventover_vect = nfp; }
void ami_eventover(ami_evtcod e, ami_pevthan eh,  ami_pevthan* oeh) { (*eventover_vect)(e, eh, oeh); }

static ami_eventsover_t eventsover_vect = eventsover_ivf;
void _pa_eventsover_ovr(ami_eventsover_t nfp, ami_eventsover_t* ofp) { *ofp = eventsover_vect; eventsover_vect = nfp; }
void ami_eventsover(ami_pevthan eh,  ami_pevthan* oeh) { (*eventsover_vect)(eh, oeh); }

static ami_extended_t extended_vect = extended_ivf;
void _pa_extended_ovr(ami_extended_t nfp, ami_extended_t* ofp) { *ofp = extended_vect; extended_vect = nfp; }
void ami_extended(FILE* f, ami_long e) { (*extended_vect)(f, e); }

static ami_fand_t fand_vect = fand_ivf;
void _pa_fand_ovr(ami_fand_t nfp, ami_fand_t* ofp) { *ofp = fand_vect; fand_vect = nfp; }
void ami_fand(FILE* f) { (*fand_vect)(f); }

static ami_farc_t farc_vect = farc_ivf;
void _pa_farc_ovr(ami_farc_t nfp, ami_farc_t* ofp) { *ofp = farc_vect; farc_vect = nfp; }
void ami_farc(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { (*farc_vect)(f, x1, y1, x2, y2, sa, ea); }

static ami_fchord_t fchord_vect = fchord_ivf;
void _pa_fchord_ovr(ami_fchord_t nfp, ami_fchord_t* ofp) { *ofp = fchord_vect; fchord_vect = nfp; }
void ami_fchord(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long sa, ami_long ea) { (*fchord_vect)(f, x1, y1, x2, y2, sa, ea); }

static ami_fcolor_t fcolor_vect = fcolor_ivf;
void _pa_fcolor_ovr(ami_fcolor_t nfp, ami_fcolor_t* ofp) { *ofp = fcolor_vect; fcolor_vect = nfp; }
void ami_fcolor(FILE* f, ami_color c) { (*fcolor_vect)(f, c); }

static ami_fcolorc_t fcolorc_vect = fcolorc_ivf;
void _pa_fcolorc_ovr(ami_fcolorc_t nfp, ami_fcolorc_t* ofp) { *ofp = fcolorc_vect; fcolorc_vect = nfp; }
void ami_fcolorc(FILE* f, ami_long r, ami_long g, ami_long b) { (*fcolorc_vect)(f, r, g, b); }

static ami_fcolorg_t fcolorg_vect = fcolorg_ivf;
void _pa_fcolorg_ovr(ami_fcolorg_t nfp, ami_fcolorg_t* ofp) { *ofp = fcolorg_vect; fcolorg_vect = nfp; }
void ami_fcolorg(FILE* f, ami_long r, ami_long g, ami_long b) { (*fcolorg_vect)(f, r, g, b); }

static ami_fellipse_t fellipse_vect = fellipse_ivf;
void _pa_fellipse_ovr(ami_fellipse_t nfp, ami_fellipse_t* ofp) { *ofp = fellipse_vect; fellipse_vect = nfp; }
void ami_fellipse(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { (*fellipse_vect)(f, x1, y1, x2, y2); }

static ami_finvis_t finvis_vect = finvis_ivf;
void _pa_finvis_ovr(ami_finvis_t nfp, ami_finvis_t* ofp) { *ofp = finvis_vect; finvis_vect = nfp; }
void ami_finvis(FILE* f) { (*finvis_vect)(f); }

static ami_focus_t focus_vect = focus_ivf;
void _pa_focus_ovr(ami_focus_t nfp, ami_focus_t* ofp) { *ofp = focus_vect; focus_vect = nfp; }
void ami_focus(FILE* f) { (*focus_vect)(f); }

static ami_focuswidget_t focuswidget_vect = focuswidget_ivf;
void _pa_focuswidget_ovr(ami_focuswidget_t nfp, ami_focuswidget_t* ofp) { *ofp = focuswidget_vect; focuswidget_vect = nfp; }
void ami_focuswidget(FILE* f, ami_long id) { (*focuswidget_vect)(f, id); }

static ami_font_t font_vect = font_ivf;
void _pa_font_ovr(ami_font_t nfp, ami_font_t* ofp) { *ofp = font_vect; font_vect = nfp; }
void ami_font(FILE* f, ami_long fc) { (*font_vect)(f, fc); }

static ami_fontnam_t fontnam_vect = fontnam_ivf;
void _pa_fontnam_ovr(ami_fontnam_t nfp, ami_fontnam_t* ofp) { *ofp = fontnam_vect; fontnam_vect = nfp; }
void ami_fontnam(FILE* f, ami_long fc, char* fns, ami_long fnsl) { (*fontnam_vect)(f, fc, fns, fnsl); }

static ami_fonts_t fonts_vect = fonts_ivf;
void _pa_fonts_ovr(ami_fonts_t nfp, ami_fonts_t* ofp) { *ofp = fonts_vect; fonts_vect = nfp; }
ami_long ami_fonts(FILE* f) { return (*fonts_vect)(f); }

static ami_fontsiz_t fontsiz_vect = fontsiz_ivf;
void _pa_fontsiz_ovr(ami_fontsiz_t nfp, ami_fontsiz_t* ofp) { *ofp = fontsiz_vect; fontsiz_vect = nfp; }
void ami_fontsiz(FILE* f, ami_long s) { (*fontsiz_vect)(f, s); }

static ami_for_t for_vect = for_ivf;
void _pa_for_ovr(ami_for_t nfp, ami_for_t* ofp) { *ofp = for_vect; for_vect = nfp; }
void ami_for(FILE* f) { (*for_vect)(f); }

static ami_fover_t fover_vect = fover_ivf;
void _pa_fover_ovr(ami_fover_t nfp, ami_fover_t* ofp) { *ofp = fover_vect; fover_vect = nfp; }
void ami_fover(FILE* f) { (*fover_vect)(f); }

static ami_frame_t frame_vect = frame_ivf;
void _pa_frame_ovr(ami_frame_t nfp, ami_frame_t* ofp) { *ofp = frame_vect; frame_vect = nfp; }
void ami_frame(FILE* f, ami_long e) { (*frame_vect)(f, e); }

static ami_frametimer_t frametimer_vect = frametimer_ivf;
void _pa_frametimer_ovr(ami_frametimer_t nfp, ami_frametimer_t* ofp) { *ofp = frametimer_vect; frametimer_vect = nfp; }
void ami_frametimer(FILE* f, ami_long e) { (*frametimer_vect)(f, e); }

static ami_frect_t frect_vect = frect_ivf;
void _pa_frect_ovr(ami_frect_t nfp, ami_frect_t* ofp) { *ofp = frect_vect; frect_vect = nfp; }
void ami_frect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { (*frect_vect)(f, x1, y1, x2, y2); }

static ami_front_t front_vect = front_ivf;
void _pa_front_ovr(ami_front_t nfp, ami_front_t* ofp) { *ofp = front_vect; front_vect = nfp; }
void ami_front(FILE* f) { (*front_vect)(f); }

static ami_frontwidget_t frontwidget_vect = frontwidget_ivf;
void _pa_frontwidget_ovr(ami_frontwidget_t nfp, ami_frontwidget_t* ofp) { *ofp = frontwidget_vect; frontwidget_vect = nfp; }
void ami_frontwidget(FILE* f, ami_long id) { (*frontwidget_vect)(f, id); }

static ami_frrect_t frrect_vect = frrect_ivf;
void _pa_frrect_ovr(ami_frrect_t nfp, ami_frrect_t* ofp) { *ofp = frrect_vect; frrect_vect = nfp; }
void ami_frrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { (*frrect_vect)(f, x1, y1, x2, y2, xs, ys); }

static ami_ftriangle_t ftriangle_vect = ftriangle_ivf;
void _pa_ftriangle_ovr(ami_ftriangle_t nfp, ami_ftriangle_t* ofp) { *ofp = ftriangle_vect; ftriangle_vect = nfp; }
void ami_ftriangle(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long x3, ami_long y3) { (*ftriangle_vect)(f, x1, y1, x2, y2, x3, y3); }

static ami_funkey_t funkey_vect = funkey_ivf;
void _pa_funkey_ovr(ami_funkey_t nfp, ami_funkey_t* ofp) { *ofp = funkey_vect; funkey_vect = nfp; }
ami_long ami_funkey(FILE* f) { return (*funkey_vect)(f); }

static ami_fxor_t fxor_vect = fxor_ivf;
void _pa_fxor_ovr(ami_fxor_t nfp, ami_fxor_t* ofp) { *ofp = fxor_vect; fxor_vect = nfp; }
void ami_fxor(FILE* f) { (*fxor_vect)(f); }

static ami_getsiz_t getsiz_vect = getsiz_ivf;
void _pa_getsiz_ovr(ami_getsiz_t nfp, ami_getsiz_t* ofp) { *ofp = getsiz_vect; getsiz_vect = nfp; }
void ami_getsiz(FILE* f, ami_long* x, ami_long* y) { (*getsiz_vect)(f, x, y); }

static ami_getsizg_t getsizg_vect = getsizg_ivf;
void _pa_getsizg_ovr(ami_getsizg_t nfp, ami_getsizg_t* ofp) { *ofp = getsizg_vect; getsizg_vect = nfp; }
void ami_getsizg(FILE* f, ami_long* x, ami_long* y) { (*getsizg_vect)(f, x, y); }

static ami_getwidgettext_t getwidgettext_vect = getwidgettext_ivf;
void _pa_getwidgettext_ovr(ami_getwidgettext_t nfp, ami_getwidgettext_t* ofp) { *ofp = getwidgettext_vect; getwidgettext_vect = nfp; }
void ami_getwidgettext(FILE* f, ami_long id, char* s, ami_long sl) { (*getwidgettext_vect)(f, id, s, sl); }

static ami_getwigid_t getwigid_vect = getwigid_ivf;
void _pa_getwigid_ovr(ami_getwigid_t nfp, ami_getwigid_t* ofp) { *ofp = getwigid_vect; getwigid_vect = nfp; }
ami_long ami_getwigid(FILE* f) { return (*getwigid_vect)(f); }

static ami_getwinid_t getwinid_vect = getwinid_ivf;
void _pa_getwinid_ovr(ami_getwinid_t nfp, ami_getwinid_t* ofp) { *ofp = getwinid_vect; getwinid_vect = nfp; }
ami_long ami_getwinid(void) { return (*getwinid_vect)(); }

static ami_group_t group_vect = group_ivf;
void _pa_group_ovr(ami_group_t nfp, ami_group_t* ofp) { *ofp = group_vect; group_vect = nfp; }
void ami_group(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*group_vect)(f, x1, y1, x2, y2, s, id); }

static ami_groupg_t groupg_vect = groupg_ivf;
void _pa_groupg_ovr(ami_groupg_t nfp, ami_groupg_t* ofp) { *ofp = groupg_vect; groupg_vect = nfp; }
void ami_groupg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*groupg_vect)(f, x1, y1, x2, y2, s, id); }

static ami_groupsiz_t groupsiz_vect = groupsiz_ivf;
void _pa_groupsiz_ovr(ami_groupsiz_t nfp, ami_groupsiz_t* ofp) { *ofp = groupsiz_vect; groupsiz_vect = nfp; }
void ami_groupsiz(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { (*groupsiz_vect)(f, s, cw, ch, w, h, ox, oy); }

static ami_groupsizg_t groupsizg_vect = groupsizg_ivf;
void _pa_groupsizg_ovr(ami_groupsizg_t nfp, ami_groupsizg_t* ofp) { *ofp = groupsizg_vect; groupsizg_vect = nfp; }
void ami_groupsizg(FILE* f, char* s, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { (*groupsizg_vect)(f, s, cw, ch, w, h, ox, oy); }

static ami_hollow_t hollow_vect = hollow_ivf;
void _pa_hollow_ovr(ami_hollow_t nfp, ami_hollow_t* ofp) { *ofp = hollow_vect; hollow_vect = nfp; }
void ami_hollow(FILE* f, ami_long e) { (*hollow_vect)(f, e); }

static ami_home_t home_vect = home_ivf;
void _pa_home_ovr(ami_home_t nfp, ami_home_t* ofp) { *ofp = home_vect; home_vect = nfp; }
void ami_home(FILE* f) { (*home_vect)(f); }

static ami_italic_t italic_vect = italic_ivf;
void _pa_italic_ovr(ami_italic_t nfp, ami_italic_t* ofp) { *ofp = italic_vect; italic_vect = nfp; }
void ami_italic(FILE* f, ami_long e) { (*italic_vect)(f, e); }

static ami_joyaxis_t joyaxis_vect = joyaxis_ivf;
void _pa_joyaxis_ovr(ami_joyaxis_t nfp, ami_joyaxis_t* ofp) { *ofp = joyaxis_vect; joyaxis_vect = nfp; }
ami_long ami_joyaxis(FILE* f, ami_long j) { return (*joyaxis_vect)(f, j); }

static ami_joybutton_t joybutton_vect = joybutton_ivf;
void _pa_joybutton_ovr(ami_joybutton_t nfp, ami_joybutton_t* ofp) { *ofp = joybutton_vect; joybutton_vect = nfp; }
ami_long ami_joybutton(FILE* f, ami_long j) { return (*joybutton_vect)(f, j); }

static ami_joystick_t joystick_vect = joystick_ivf;
void _pa_joystick_ovr(ami_joystick_t nfp, ami_joystick_t* ofp) { *ofp = joystick_vect; joystick_vect = nfp; }
ami_long ami_joystick(FILE* f) { return (*joystick_vect)(f); }

static ami_justpos_t justpos_vect = justpos_ivf;
void _pa_justpos_ovr(ami_justpos_t nfp, ami_justpos_t* ofp) { *ofp = justpos_vect; justpos_vect = nfp; }
ami_long ami_justpos(FILE* f, const char* s, ami_long p, ami_long n) { return (*justpos_vect)(f, s, p, n); }

static ami_killtimer_t killtimer_vect = killtimer_ivf;
void _pa_killtimer_ovr(ami_killtimer_t nfp, ami_killtimer_t* ofp) { *ofp = killtimer_vect; killtimer_vect = nfp; }
void ami_killtimer(FILE* f, ami_long i) { (*killtimer_vect)(f, i); }

static ami_killwidget_t killwidget_vect = killwidget_ivf;
void _pa_killwidget_ovr(ami_killwidget_t nfp, ami_killwidget_t* ofp) { *ofp = killwidget_vect; killwidget_vect = nfp; }
void ami_killwidget(FILE* f, ami_long id) { (*killwidget_vect)(f, id); }

static ami_left_t left_vect = left_ivf;
void _pa_left_ovr(ami_left_t nfp, ami_left_t* ofp) { *ofp = left_vect; left_vect = nfp; }
void ami_left(FILE* f) { (*left_vect)(f); }

static ami_light_t light_vect = light_ivf;
void _pa_light_ovr(ami_light_t nfp, ami_light_t* ofp) { *ofp = light_vect; light_vect = nfp; }
void ami_light(FILE* f, ami_long e) { (*light_vect)(f, e); }

static ami_line_t line_vect = line_ivf;
void _pa_line_ovr(ami_line_t nfp, ami_line_t* ofp) { *ofp = line_vect; line_vect = nfp; }
void ami_line(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { (*line_vect)(f, x1, y1, x2, y2); }

static ami_linestyle_t linestyle_vect = linestyle_ivf;
void _pa_linestyle_ovr(ami_linestyle_t nfp, ami_linestyle_t* ofp) { *ofp = linestyle_vect; linestyle_vect = nfp; }
void ami_linestyle(FILE* f, ami_lstyle style) { (*linestyle_vect)(f, style); }

static ami_linewidth_t linewidth_vect = linewidth_ivf;
void _pa_linewidth_ovr(ami_linewidth_t nfp, ami_linewidth_t* ofp) { *ofp = linewidth_vect; linewidth_vect = nfp; }
void ami_linewidth(FILE* f, ami_long w) { (*linewidth_vect)(f, w); }

static ami_listbox_t listbox_vect = listbox_ivf;
void _pa_listbox_ovr(ami_listbox_t nfp, ami_listbox_t* ofp) { *ofp = listbox_vect; listbox_vect = nfp; }
void ami_listbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id) { (*listbox_vect)(f, x1, y1, x2, y2, sp, id); }

static ami_listboxg_t listboxg_vect = listboxg_ivf;
void _pa_listboxg_ovr(ami_listboxg_t nfp, ami_listboxg_t* ofp) { *ofp = listboxg_vect; listboxg_vect = nfp; }
void ami_listboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_long id) { (*listboxg_vect)(f, x1, y1, x2, y2, sp, id); }

static ami_listboxsiz_t listboxsiz_vect = listboxsiz_ivf;
void _pa_listboxsiz_ovr(ami_listboxsiz_t nfp, ami_listboxsiz_t* ofp) { *ofp = listboxsiz_vect; listboxsiz_vect = nfp; }
void ami_listboxsiz(FILE* f, ami_strptr sp, ami_long* w, ami_long* h) { (*listboxsiz_vect)(f, sp, w, h); }

static ami_listboxsizg_t listboxsizg_vect = listboxsizg_ivf;
void _pa_listboxsizg_ovr(ami_listboxsizg_t nfp, ami_listboxsizg_t* ofp) { *ofp = listboxsizg_vect; listboxsizg_vect = nfp; }
void ami_listboxsizg(FILE* f, ami_strptr sp, ami_long* w, ami_long* h) { (*listboxsizg_vect)(f, sp, w, h); }

static ami_loadpict_t loadpict_vect = loadpict_ivf;
void _pa_loadpict_ovr(ami_loadpict_t nfp, ami_loadpict_t* ofp) { *ofp = loadpict_vect; loadpict_vect = nfp; }
void ami_loadpict(FILE* f, ami_long p, char* fn) { (*loadpict_vect)(f, p, fn); }

static ami_maxx_t maxx_vect = maxx_ivf;
void _pa_maxx_ovr(ami_maxx_t nfp, ami_maxx_t* ofp) { *ofp = maxx_vect; maxx_vect = nfp; }
ami_long ami_maxx(FILE* f) { return (*maxx_vect)(f); }

static ami_maxxg_t maxxg_vect = maxxg_ivf;
void _pa_maxxg_ovr(ami_maxxg_t nfp, ami_maxxg_t* ofp) { *ofp = maxxg_vect; maxxg_vect = nfp; }
ami_long ami_maxxg(FILE* f) { return (*maxxg_vect)(f); }

static ami_maxy_t maxy_vect = maxy_ivf;
void _pa_maxy_ovr(ami_maxy_t nfp, ami_maxy_t* ofp) { *ofp = maxy_vect; maxy_vect = nfp; }
ami_long ami_maxy(FILE* f) { return (*maxy_vect)(f); }

static ami_maxyg_t maxyg_vect = maxyg_ivf;
void _pa_maxyg_ovr(ami_maxyg_t nfp, ami_maxyg_t* ofp) { *ofp = maxyg_vect; maxyg_vect = nfp; }
ami_long ami_maxyg(FILE* f) { return (*maxyg_vect)(f); }

static ami_menu_t menu_vect = menu_ivf;
void _pa_menu_ovr(ami_menu_t nfp, ami_menu_t* ofp) { *ofp = menu_vect; menu_vect = nfp; }
void ami_menu(FILE* f, ami_menuptr m) { (*menu_vect)(f, m); }

static ami_menuena_t menuena_vect = menuena_ivf;
void _pa_menuena_ovr(ami_menuena_t nfp, ami_menuena_t* ofp) { *ofp = menuena_vect; menuena_vect = nfp; }
void ami_menuena(FILE* f, ami_long id, ami_long onoff) { (*menuena_vect)(f, id, onoff); }

static ami_menusel_t menusel_vect = menusel_ivf;
void _pa_menusel_ovr(ami_menusel_t nfp, ami_menusel_t* ofp) { *ofp = menusel_vect; menusel_vect = nfp; }
void ami_menusel(FILE* f, ami_long id, ami_long select) { (*menusel_vect)(f, id, select); }

static ami_mouse_t mouse_vect = mouse_ivf;
void _pa_mouse_ovr(ami_mouse_t nfp, ami_mouse_t* ofp) { *ofp = mouse_vect; mouse_vect = nfp; }
ami_long ami_mouse(FILE* f) { return (*mouse_vect)(f); }

static ami_mousebutton_t mousebutton_vect = mousebutton_ivf;
void _pa_mousebutton_ovr(ami_mousebutton_t nfp, ami_mousebutton_t* ofp) { *ofp = mousebutton_vect; mousebutton_vect = nfp; }
ami_long ami_mousebutton(FILE* f, ami_long m) { return (*mousebutton_vect)(f, m); }

static ami_numselbox_t numselbox_vect = numselbox_ivf;
void _pa_numselbox_ovr(ami_numselbox_t nfp, ami_numselbox_t* ofp) { *ofp = numselbox_vect; numselbox_vect = nfp; }
void ami_numselbox(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id) { (*numselbox_vect)(f, x1, y1, x2, y2, l, u, id); }

static ami_numselboxg_t numselboxg_vect = numselboxg_ivf;
void _pa_numselboxg_ovr(ami_numselboxg_t nfp, ami_numselboxg_t* ofp) { *ofp = numselboxg_vect; numselboxg_vect = nfp; }
void ami_numselboxg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long l, ami_long u, ami_long id) { (*numselboxg_vect)(f, x1, y1, x2, y2, l, u, id); }

static ami_numselboxsiz_t numselboxsiz_vect = numselboxsiz_ivf;
void _pa_numselboxsiz_ovr(ami_numselboxsiz_t nfp, ami_numselboxsiz_t* ofp) { *ofp = numselboxsiz_vect; numselboxsiz_vect = nfp; }
void ami_numselboxsiz(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h) { (*numselboxsiz_vect)(f, l, u, w, h); }

static ami_numselboxsizg_t numselboxsizg_vect = numselboxsizg_ivf;
void _pa_numselboxsizg_ovr(ami_numselboxsizg_t nfp, ami_numselboxsizg_t* ofp) { *ofp = numselboxsizg_vect; numselboxsizg_vect = nfp; }
void ami_numselboxsizg(FILE* f, ami_long l, ami_long u, ami_long* w, ami_long* h) { (*numselboxsizg_vect)(f, l, u, w, h); }

static ami_openwin_t openwin_vect = openwin_ivf;
void _pa_openwin_ovr(ami_openwin_t nfp, ami_openwin_t* ofp) { *ofp = openwin_vect; openwin_vect = nfp; }
void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, ami_long wid) { (*openwin_vect)(infile, outfile, parent, wid); }

static ami_path_t path_vect = path_ivf;
void _pa_path_ovr(ami_path_t nfp, ami_path_t* ofp) { *ofp = path_vect; path_vect = nfp; }
void ami_path(FILE* f, ami_long a) { (*path_vect)(f, a); }

static ami_pictsizx_t pictsizx_vect = pictsizx_ivf;
void _pa_pictsizx_ovr(ami_pictsizx_t nfp, ami_pictsizx_t* ofp) { *ofp = pictsizx_vect; pictsizx_vect = nfp; }
ami_long ami_pictsizx(FILE* f, ami_long p) { return (*pictsizx_vect)(f, p); }

static ami_pictsizy_t pictsizy_vect = pictsizy_ivf;
void _pa_pictsizy_ovr(ami_pictsizy_t nfp, ami_pictsizy_t* ofp) { *ofp = pictsizy_vect; pictsizy_vect = nfp; }
ami_long ami_pictsizy(FILE* f, ami_long p) { return (*pictsizy_vect)(f, p); }

static ami_picture_t picture_vect = picture_ivf;
void _pa_picture_ovr(ami_picture_t nfp, ami_picture_t* ofp) { *ofp = picture_vect; picture_vect = nfp; }
void ami_picture(FILE* f, ami_long p, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { (*picture_vect)(f, p, x1, y1, x2, y2); }

static ami_points_t points_vect = points_ivf;
void _pa_points_ovr(ami_points_t nfp, ami_points_t* ofp) { *ofp = points_vect; points_vect = nfp; }
float ami_points(FILE* f) { return (*points_vect)(f); }

static ami_poswidget_t poswidget_vect = poswidget_ivf;
void _pa_poswidget_ovr(ami_poswidget_t nfp, ami_poswidget_t* ofp) { *ofp = poswidget_vect; poswidget_vect = nfp; }
void ami_poswidget(FILE* f, ami_long id, ami_long x, ami_long y) { (*poswidget_vect)(f, id, x, y); }

static ami_poswidgetg_t poswidgetg_vect = poswidgetg_ivf;
void _pa_poswidgetg_ovr(ami_poswidgetg_t nfp, ami_poswidgetg_t* ofp) { *ofp = poswidgetg_vect; poswidgetg_vect = nfp; }
void ami_poswidgetg(FILE* f, ami_long id, ami_long x, ami_long y) { (*poswidgetg_vect)(f, id, x, y); }

static ami_progbar_t progbar_vect = progbar_ivf;
void _pa_progbar_ovr(ami_progbar_t nfp, ami_progbar_t* ofp) { *ofp = progbar_vect; progbar_vect = nfp; }
void ami_progbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*progbar_vect)(f, x1, y1, x2, y2, id); }

static ami_progbarg_t progbarg_vect = progbarg_ivf;
void _pa_progbarg_ovr(ami_progbarg_t nfp, ami_progbarg_t* ofp) { *ofp = progbarg_vect; progbarg_vect = nfp; }
void ami_progbarg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*progbarg_vect)(f, x1, y1, x2, y2, id); }

static ami_progbarpos_t progbarpos_vect = progbarpos_ivf;
void _pa_progbarpos_ovr(ami_progbarpos_t nfp, ami_progbarpos_t* ofp) { *ofp = progbarpos_vect; progbarpos_vect = nfp; }
void ami_progbarpos(FILE* f, ami_long id, ami_long pos) { (*progbarpos_vect)(f, id, pos); }

static ami_progbarsiz_t progbarsiz_vect = progbarsiz_ivf;
void _pa_progbarsiz_ovr(ami_progbarsiz_t nfp, ami_progbarsiz_t* ofp) { *ofp = progbarsiz_vect; progbarsiz_vect = nfp; }
void ami_progbarsiz(FILE* f, ami_long* w, ami_long* h) { (*progbarsiz_vect)(f, w, h); }

static ami_progbarsizg_t progbarsizg_vect = progbarsizg_ivf;
void _pa_progbarsizg_ovr(ami_progbarsizg_t nfp, ami_progbarsizg_t* ofp) { *ofp = progbarsizg_vect; progbarsizg_vect = nfp; }
void ami_progbarsizg(FILE* f, ami_long* w, ami_long* h) { (*progbarsizg_vect)(f, w, h); }

static ami_putwidgettext_t putwidgettext_vect = putwidgettext_ivf;
void _pa_putwidgettext_ovr(ami_putwidgettext_t nfp, ami_putwidgettext_t* ofp) { *ofp = putwidgettext_vect; putwidgettext_vect = nfp; }
void ami_putwidgettext(FILE* f, ami_long id, char* s) { (*putwidgettext_vect)(f, id, s); }

static ami_querycolor_t querycolor_vect = querycolor_ivf;
void _pa_querycolor_ovr(ami_querycolor_t nfp, ami_querycolor_t* ofp) { *ofp = querycolor_vect; querycolor_vect = nfp; }
void ami_querycolor(ami_long* r, ami_long* g, ami_long* b) { (*querycolor_vect)(r, g, b); }

static ami_queryfind_t queryfind_vect = queryfind_ivf;
void _pa_queryfind_ovr(ami_queryfind_t nfp, ami_queryfind_t* ofp) { *ofp = queryfind_vect; queryfind_vect = nfp; }
void ami_queryfind(char* s, ami_long sl, ami_qfnopts* opt) { (*queryfind_vect)(s, sl, opt); }

static ami_queryfindrep_t queryfindrep_vect = queryfindrep_ivf;
void _pa_queryfindrep_ovr(ami_queryfindrep_t nfp, ami_queryfindrep_t* ofp) { *ofp = queryfindrep_vect; queryfindrep_vect = nfp; }
void ami_queryfindrep(char* s, ami_long sl, char* r, ami_long rl, ami_qfropts* opt) { (*queryfindrep_vect)(s, sl, r, rl, opt); }

static ami_queryfont_t queryfont_vect = queryfont_ivf;
void _pa_queryfont_ovr(ami_queryfont_t nfp, ami_queryfont_t* ofp) { *ofp = queryfont_vect; queryfont_vect = nfp; }
void ami_queryfont(FILE* f, ami_long* fc, ami_long* s, ami_long* fr, ami_long* fg, ami_long* fb, ami_long* br, ami_long* bg, ami_long* bb, ami_qfteffects* effect) { (*queryfont_vect)(f, fc, s, fr, fg, fb, br, bg, bb, effect); }

static ami_queryopen_t queryopen_vect = queryopen_ivf;
void _pa_queryopen_ovr(ami_queryopen_t nfp, ami_queryopen_t* ofp) { *ofp = queryopen_vect; queryopen_vect = nfp; }
void ami_queryopen(char* s, ami_long sl) { (*queryopen_vect)(s, sl); }

static ami_querysave_t querysave_vect = querysave_ivf;
void _pa_querysave_ovr(ami_querysave_t nfp, ami_querysave_t* ofp) { *ofp = querysave_vect; querysave_vect = nfp; }
void ami_querysave(char* s, ami_long sl) { (*querysave_vect)(s, sl); }

static ami_radiobutton_t radiobutton_vect = radiobutton_ivf;
void _pa_radiobutton_ovr(ami_radiobutton_t nfp, ami_radiobutton_t* ofp) { *ofp = radiobutton_vect; radiobutton_vect = nfp; }
void ami_radiobutton(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*radiobutton_vect)(f, x1, y1, x2, y2, s, id); }

static ami_radiobuttong_t radiobuttong_vect = radiobuttong_ivf;
void _pa_radiobuttong_ovr(ami_radiobuttong_t nfp, ami_radiobuttong_t* ofp) { *ofp = radiobuttong_vect; radiobuttong_vect = nfp; }
void ami_radiobuttong(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, char* s, ami_long id) { (*radiobuttong_vect)(f, x1, y1, x2, y2, s, id); }

static ami_radiobuttonsiz_t radiobuttonsiz_vect = radiobuttonsiz_ivf;
void _pa_radiobuttonsiz_ovr(ami_radiobuttonsiz_t nfp, ami_radiobuttonsiz_t* ofp) { *ofp = radiobuttonsiz_vect; radiobuttonsiz_vect = nfp; }
void ami_radiobuttonsiz(FILE* f, char* s, ami_long* w, ami_long* h) { (*radiobuttonsiz_vect)(f, s, w, h); }

static ami_radiobuttonsizg_t radiobuttonsizg_vect = radiobuttonsizg_ivf;
void _pa_radiobuttonsizg_ovr(ami_radiobuttonsizg_t nfp, ami_radiobuttonsizg_t* ofp) { *ofp = radiobuttonsizg_vect; radiobuttonsizg_vect = nfp; }
void ami_radiobuttonsizg(FILE* f, char* s, ami_long* w, ami_long* h) { (*radiobuttonsizg_vect)(f, s, w, h); }

static ami_raised_t raised_vect = raised_ivf;
void _pa_raised_ovr(ami_raised_t nfp, ami_raised_t* ofp) { *ofp = raised_vect; raised_vect = nfp; }
void ami_raised(FILE* f, ami_long e) { (*raised_vect)(f, e); }

static ami_rect_t rect_vect = rect_ivf;
void _pa_rect_ovr(ami_rect_t nfp, ami_rect_t* ofp) { *ofp = rect_vect; rect_vect = nfp; }
void ami_rect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2) { (*rect_vect)(f, x1, y1, x2, y2); }

static ami_restab_t restab_vect = restab_ivf;
void _pa_restab_ovr(ami_restab_t nfp, ami_restab_t* ofp) { *ofp = restab_vect; restab_vect = nfp; }
void ami_restab(FILE* f, ami_long t) { (*restab_vect)(f, t); }

static ami_restabg_t restabg_vect = restabg_ivf;
void _pa_restabg_ovr(ami_restabg_t nfp, ami_restabg_t* ofp) { *ofp = restabg_vect; restabg_vect = nfp; }
void ami_restabg(FILE* f, ami_long t) { (*restabg_vect)(f, t); }

static ami_reverse_t reverse_vect = reverse_ivf;
void _pa_reverse_ovr(ami_reverse_t nfp, ami_reverse_t* ofp) { *ofp = reverse_vect; reverse_vect = nfp; }
void ami_reverse(FILE* f, ami_long e) { (*reverse_vect)(f, e); }

static ami_right_t right_vect = right_ivf;
void _pa_right_ovr(ami_right_t nfp, ami_right_t* ofp) { *ofp = right_vect; right_vect = nfp; }
void ami_right(FILE* f) { (*right_vect)(f); }

static ami_rrect_t rrect_vect = rrect_ivf;
void _pa_rrect_ovr(ami_rrect_t nfp, ami_rrect_t* ofp) { *ofp = rrect_vect; rrect_vect = nfp; }
void ami_rrect(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long xs, ami_long ys) { (*rrect_vect)(f, x1, y1, x2, y2, xs, ys); }

static ami_scalex_t scalex_vect = scalex_ivf;
void _pa_scalex_ovr(ami_scalex_t nfp, ami_scalex_t* ofp) { *ofp = scalex_vect; scalex_vect = nfp; }
ami_long ami_scalex(FILE* f, ami_long x) { return (*scalex_vect)(f, x); }

static ami_scaley_t scaley_vect = scaley_ivf;
void _pa_scaley_ovr(ami_scaley_t nfp, ami_scaley_t* ofp) { *ofp = scaley_vect; scaley_vect = nfp; }
ami_long ami_scaley(FILE* f, ami_long y) { return (*scaley_vect)(f, y); }

static ami_scncen_t scncen_vect = scncen_ivf;
void _pa_scncen_ovr(ami_scncen_t nfp, ami_scncen_t* ofp) { *ofp = scncen_vect; scncen_vect = nfp; }
void ami_scncen(FILE* f, ami_long* x, ami_long* y) { (*scncen_vect)(f, x, y); }

static ami_scnceng_t scnceng_vect = scnceng_ivf;
void _pa_scnceng_ovr(ami_scnceng_t nfp, ami_scnceng_t* ofp) { *ofp = scnceng_vect; scnceng_vect = nfp; }
void ami_scnceng(FILE* f, ami_long* x, ami_long* y) { (*scnceng_vect)(f, x, y); }

static ami_scnsiz_t scnsiz_vect = scnsiz_ivf;
void _pa_scnsiz_ovr(ami_scnsiz_t nfp, ami_scnsiz_t* ofp) { *ofp = scnsiz_vect; scnsiz_vect = nfp; }
void ami_scnsiz(FILE* f, ami_long* x, ami_long* y) { (*scnsiz_vect)(f, x, y); }

static ami_scnsizg_t scnsizg_vect = scnsizg_ivf;
void _pa_scnsizg_ovr(ami_scnsizg_t nfp, ami_scnsizg_t* ofp) { *ofp = scnsizg_vect; scnsizg_vect = nfp; }
void ami_scnsizg(FILE* f, ami_long* x, ami_long*y) { (*scnsizg_vect)(f, x, y); }

static ami_scroll_t scroll_vect = scroll_ivf;
void _pa_scroll_ovr(ami_scroll_t nfp, ami_scroll_t* ofp) { *ofp = scroll_vect; scroll_vect = nfp; }
void ami_scroll(FILE* f, ami_long x, ami_long y) { (*scroll_vect)(f, x, y); }

static ami_scrollg_t scrollg_vect = scrollg_ivf;
void _pa_scrollg_ovr(ami_scrollg_t nfp, ami_scrollg_t* ofp) { *ofp = scrollg_vect; scrollg_vect = nfp; }
void ami_scrollg(FILE* f, ami_long x, ami_long y) { (*scrollg_vect)(f, x, y); }

static ami_scrollhoriz_t scrollhoriz_vect = scrollhoriz_ivf;
void _pa_scrollhoriz_ovr(ami_scrollhoriz_t nfp, ami_scrollhoriz_t* ofp) { *ofp = scrollhoriz_vect; scrollhoriz_vect = nfp; }
void ami_scrollhoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*scrollhoriz_vect)(f, x1, y1, x2, y2, id); }

static ami_scrollhorizg_t scrollhorizg_vect = scrollhorizg_ivf;
void _pa_scrollhorizg_ovr(ami_scrollhorizg_t nfp, ami_scrollhorizg_t* ofp) { *ofp = scrollhorizg_vect; scrollhorizg_vect = nfp; }
void ami_scrollhorizg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*scrollhorizg_vect)(f, x1, y1, x2, y2, id); }

static ami_scrollhorizsiz_t scrollhorizsiz_vect = scrollhorizsiz_ivf;
void _pa_scrollhorizsiz_ovr(ami_scrollhorizsiz_t nfp, ami_scrollhorizsiz_t* ofp) { *ofp = scrollhorizsiz_vect; scrollhorizsiz_vect = nfp; }
void ami_scrollhorizsiz(FILE* f, ami_long* w, ami_long* h) { (*scrollhorizsiz_vect)(f, w, h); }

static ami_scrollhorizsizg_t scrollhorizsizg_vect = scrollhorizsizg_ivf;
void _pa_scrollhorizsizg_ovr(ami_scrollhorizsizg_t nfp, ami_scrollhorizsizg_t* ofp) { *ofp = scrollhorizsizg_vect; scrollhorizsizg_vect = nfp; }
void ami_scrollhorizsizg(FILE* f, ami_long* w, ami_long* h) { (*scrollhorizsizg_vect)(f, w, h); }

static ami_scrollpos_t scrollpos_vect = scrollpos_ivf;
void _pa_scrollpos_ovr(ami_scrollpos_t nfp, ami_scrollpos_t* ofp) { *ofp = scrollpos_vect; scrollpos_vect = nfp; }
void ami_scrollpos(FILE* f, ami_long id, ami_long r) { (*scrollpos_vect)(f, id, r); }

static ami_scrollsiz_t scrollsiz_vect = scrollsiz_ivf;
void _pa_scrollsiz_ovr(ami_scrollsiz_t nfp, ami_scrollsiz_t* ofp) { *ofp = scrollsiz_vect; scrollsiz_vect = nfp; }
void ami_scrollsiz(FILE* f, ami_long id, ami_long r) { (*scrollsiz_vect)(f, id, r); }

static ami_scrollvert_t scrollvert_vect = scrollvert_ivf;
void _pa_scrollvert_ovr(ami_scrollvert_t nfp, ami_scrollvert_t* ofp) { *ofp = scrollvert_vect; scrollvert_vect = nfp; }
void ami_scrollvert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*scrollvert_vect)(f, x1, y1, x2, y2, id); }

static ami_scrollvertg_t scrollvertg_vect = scrollvertg_ivf;
void _pa_scrollvertg_ovr(ami_scrollvertg_t nfp, ami_scrollvertg_t* ofp) { *ofp = scrollvertg_vect; scrollvertg_vect = nfp; }
void ami_scrollvertg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long id) { (*scrollvertg_vect)(f, x1, y1, x2, y2, id); }

static ami_scrollvertsiz_t scrollvertsiz_vect = scrollvertsiz_ivf;
void _pa_scrollvertsiz_ovr(ami_scrollvertsiz_t nfp, ami_scrollvertsiz_t* ofp) { *ofp = scrollvertsiz_vect; scrollvertsiz_vect = nfp; }
void ami_scrollvertsiz(FILE* f, ami_long* w, ami_long* h) { (*scrollvertsiz_vect)(f, w, h); }

static ami_scrollvertsizg_t scrollvertsizg_vect = scrollvertsizg_ivf;
void _pa_scrollvertsizg_ovr(ami_scrollvertsizg_t nfp, ami_scrollvertsizg_t* ofp) { *ofp = scrollvertsizg_vect; scrollvertsizg_vect = nfp; }
void ami_scrollvertsizg(FILE* f, ami_long* w, ami_long* h) { (*scrollvertsizg_vect)(f, w, h); }

static ami_select_t select_vect = select_ivf;
void _pa_select_ovr(ami_select_t nfp, ami_select_t* ofp) { *ofp = select_vect; select_vect = nfp; }
void ami_select(FILE* f, ami_long u, ami_long d) { (*select_vect)(f, u, d); }

static ami_selectwidget_t selectwidget_vect = selectwidget_ivf;
void _pa_selectwidget_ovr(ami_selectwidget_t nfp, ami_selectwidget_t* ofp) { *ofp = selectwidget_vect; selectwidget_vect = nfp; }
void ami_selectwidget(FILE* f, ami_long id, ami_long e) { (*selectwidget_vect)(f, id, e); }

static ami_sendevent_t sendevent_vect = sendevent_ivf;
void _pa_sendevent_ovr(ami_sendevent_t nfp, ami_sendevent_t* ofp) { *ofp = sendevent_vect; sendevent_vect = nfp; }
void ami_sendevent(FILE* f, ami_evtrec* er) { (*sendevent_vect)(f, er); }

static ami_setpixel_t setpixel_vect = setpixel_ivf;
void _pa_setpixel_ovr(ami_setpixel_t nfp, ami_setpixel_t* ofp) { *ofp = setpixel_vect; setpixel_vect = nfp; }
void ami_setpixel(FILE* f, ami_long x, ami_long y) { (*setpixel_vect)(f, x, y); }

static ami_setpoints_t setpoints_vect = setpoints_ivf;
void _pa_setpoints_ovr(ami_setpoints_t nfp, ami_setpoints_t* ofp) { *ofp = setpoints_vect; setpoints_vect = nfp; }
void ami_setpoints(FILE* f, float ps) { (*setpoints_vect)(f, ps); }

static ami_setpos_t setpos_vect = setpos_ivf;
void _pa_setpos_ovr(ami_setpos_t nfp, ami_setpos_t* ofp) { *ofp = setpos_vect; setpos_vect = nfp; }
void ami_setpos(FILE* f, ami_long x, ami_long y) { (*setpos_vect)(f, x, y); }

static ami_setposg_t setposg_vect = setposg_ivf;
void _pa_setposg_ovr(ami_setposg_t nfp, ami_setposg_t* ofp) { *ofp = setposg_vect; setposg_vect = nfp; }
void ami_setposg(FILE* f, ami_long x, ami_long y) { (*setposg_vect)(f, x, y); }

static ami_setsiz_t setsiz_vect = setsiz_ivf;
void _pa_setsiz_ovr(ami_setsiz_t nfp, ami_setsiz_t* ofp) { *ofp = setsiz_vect; setsiz_vect = nfp; }
void ami_setsiz(FILE* f, ami_long x, ami_long y) { (*setsiz_vect)(f, x, y); }

static ami_setsizg_t setsizg_vect = setsizg_ivf;
void _pa_setsizg_ovr(ami_setsizg_t nfp, ami_setsizg_t* ofp) { *ofp = setsizg_vect; setsizg_vect = nfp; }
void ami_setsizg(FILE* f, ami_long x, ami_long y) { (*setsizg_vect)(f, x, y); }

static ami_settab_t settab_vect = settab_ivf;
void _pa_settab_ovr(ami_settab_t nfp, ami_settab_t* ofp) { *ofp = settab_vect; settab_vect = nfp; }
void ami_settab(FILE* f, ami_long t) { (*settab_vect)(f, t); }

static ami_settabg_t settabg_vect = settabg_ivf;
void _pa_settabg_ovr(ami_settabg_t nfp, ami_settabg_t* ofp) { *ofp = settabg_vect; settabg_vect = nfp; }
void ami_settabg(FILE* f, ami_long t) { (*settabg_vect)(f, t); }

static ami_sizable_t sizable_vect = sizable_ivf;
void _pa_sizable_ovr(ami_sizable_t nfp, ami_sizable_t* ofp) { *ofp = sizable_vect; sizable_vect = nfp; }
void ami_sizable(FILE* f, ami_long e) { (*sizable_vect)(f, e); }

static ami_sizbuf_t sizbuf_vect = sizbuf_ivf;
void _pa_sizbuf_ovr(ami_sizbuf_t nfp, ami_sizbuf_t* ofp) { *ofp = sizbuf_vect; sizbuf_vect = nfp; }
void ami_sizbuf(FILE* f, ami_long x, ami_long y) { (*sizbuf_vect)(f, x, y); }

static ami_sizbufg_t sizbufg_vect = sizbufg_ivf;
void _pa_sizbufg_ovr(ami_sizbufg_t nfp, ami_sizbufg_t* ofp) { *ofp = sizbufg_vect; sizbufg_vect = nfp; }
void ami_sizbufg(FILE* f, ami_long x, ami_long y) { (*sizbufg_vect)(f, x, y); }

static ami_sizwidget_t sizwidget_vect = sizwidget_ivf;
void _pa_sizwidget_ovr(ami_sizwidget_t nfp, ami_sizwidget_t* ofp) { *ofp = sizwidget_vect; sizwidget_vect = nfp; }
void ami_sizwidget(FILE* f, ami_long id, ami_long x, ami_long y) { (*sizwidget_vect)(f, id, x, y); }

static ami_sizwidgetg_t sizwidgetg_vect = sizwidgetg_ivf;
void _pa_sizwidgetg_ovr(ami_sizwidgetg_t nfp, ami_sizwidgetg_t* ofp) { *ofp = sizwidgetg_vect; sizwidgetg_vect = nfp; }
void ami_sizwidgetg(FILE* f, ami_long id, ami_long x, ami_long y) { (*sizwidgetg_vect)(f, id, x, y); }

static ami_slidehoriz_t slidehoriz_vect = slidehoriz_ivf;
void _pa_slidehoriz_ovr(ami_slidehoriz_t nfp, ami_slidehoriz_t* ofp) { *ofp = slidehoriz_vect; slidehoriz_vect = nfp; }
void ami_slidehoriz(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { (*slidehoriz_vect)(f, x1, y1, x2, y2, mark, id); }

static ami_slidehorizg_t slidehorizg_vect = slidehorizg_ivf;
void _pa_slidehorizg_ovr(ami_slidehorizg_t nfp, ami_slidehorizg_t* ofp) { *ofp = slidehorizg_vect; slidehorizg_vect = nfp; }
void ami_slidehorizg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { (*slidehorizg_vect)(f, x1, y1, x2, y2, mark, id); }

static ami_slidehorizsiz_t slidehorizsiz_vect = slidehorizsiz_ivf;
void _pa_slidehorizsiz_ovr(ami_slidehorizsiz_t nfp, ami_slidehorizsiz_t* ofp) { *ofp = slidehorizsiz_vect; slidehorizsiz_vect = nfp; }
void ami_slidehorizsiz(FILE* f, ami_long* w, ami_long* h) { (*slidehorizsiz_vect)(f, w, h); }

static ami_slidehorizsizg_t slidehorizsizg_vect = slidehorizsizg_ivf;
void _pa_slidehorizsizg_ovr(ami_slidehorizsizg_t nfp, ami_slidehorizsizg_t* ofp) { *ofp = slidehorizsizg_vect; slidehorizsizg_vect = nfp; }
void ami_slidehorizsizg(FILE* f, ami_long* w, ami_long* h) { (*slidehorizsizg_vect)(f, w, h); }

static ami_slidevert_t slidevert_vect = slidevert_ivf;
void _pa_slidevert_ovr(ami_slidevert_t nfp, ami_slidevert_t* ofp) { *ofp = slidevert_vect; slidevert_vect = nfp; }
void ami_slidevert(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { (*slidevert_vect)(f, x1, y1, x2, y2, mark, id); }

static ami_slidevertg_t slidevertg_vect = slidevertg_ivf;
void _pa_slidevertg_ovr(ami_slidevertg_t nfp, ami_slidevertg_t* ofp) { *ofp = slidevertg_vect; slidevertg_vect = nfp; }
void ami_slidevertg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_long mark, ami_long id) { (*slidevertg_vect)(f, x1, y1, x2, y2, mark, id); }

static ami_slidevertsiz_t slidevertsiz_vect = slidevertsiz_ivf;
void _pa_slidevertsiz_ovr(ami_slidevertsiz_t nfp, ami_slidevertsiz_t* ofp) { *ofp = slidevertsiz_vect; slidevertsiz_vect = nfp; }
void ami_slidevertsiz(FILE* f, ami_long* w, ami_long* h) { (*slidevertsiz_vect)(f, w, h); }

static ami_slidevertsizg_t slidevertsizg_vect = slidevertsizg_ivf;
void _pa_slidevertsizg_ovr(ami_slidevertsizg_t nfp, ami_slidevertsizg_t* ofp) { *ofp = slidevertsizg_vect; slidevertsizg_vect = nfp; }
void ami_slidevertsizg(FILE* f, ami_long* w, ami_long* h) { (*slidevertsizg_vect)(f, w, h); }

static ami_standout_t standout_vect = standout_ivf;
void _pa_standout_ovr(ami_standout_t nfp, ami_standout_t* ofp) { *ofp = standout_vect; standout_vect = nfp; }
void ami_standout(FILE* f, ami_long e) { (*standout_vect)(f, e); }

static ami_stdmenu_t stdmenu_vect = stdmenu_ivf;
void _pa_stdmenu_ovr(ami_stdmenu_t nfp, ami_stdmenu_t* ofp) { *ofp = stdmenu_vect; stdmenu_vect = nfp; }
void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm) { (*stdmenu_vect)(sms, sm, pm); }

static ami_strikeout_t strikeout_vect = strikeout_ivf;
void _pa_strikeout_ovr(ami_strikeout_t nfp, ami_strikeout_t* ofp) { *ofp = strikeout_vect; strikeout_vect = nfp; }
void ami_strikeout(FILE* f, ami_long e) { (*strikeout_vect)(f, e); }

static ami_strsiz_t strsiz_vect = strsiz_ivf;
void _pa_strsiz_ovr(ami_strsiz_t nfp, ami_strsiz_t* ofp) { *ofp = strsiz_vect; strsiz_vect = nfp; }
ami_long ami_strsiz(FILE* f, const char* s) { return (*strsiz_vect)(f, s); }

static ami_subscript_t subscript_vect = subscript_ivf;
void _pa_subscript_ovr(ami_subscript_t nfp, ami_subscript_t* ofp) { *ofp = subscript_vect; subscript_vect = nfp; }
void ami_subscript(FILE* f, ami_long e) { (*subscript_vect)(f, e); }

static ami_superscript_t superscript_vect = superscript_ivf;
void _pa_superscript_ovr(ami_superscript_t nfp, ami_superscript_t* ofp) { *ofp = superscript_vect; superscript_vect = nfp; }
void ami_superscript(FILE* f, ami_long e) { (*superscript_vect)(f, e); }

static ami_sysbar_t sysbar_vect = sysbar_ivf;
void _pa_sysbar_ovr(ami_sysbar_t nfp, ami_sysbar_t* ofp) { *ofp = sysbar_vect; sysbar_vect = nfp; }
void ami_sysbar(FILE* f, ami_long e) { (*sysbar_vect)(f, e); }

static ami_tabbar_t tabbar_vect = tabbar_ivf;
void _pa_tabbar_ovr(ami_tabbar_t nfp, ami_tabbar_t* ofp) { *ofp = tabbar_vect; tabbar_vect = nfp; }
void ami_tabbar(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id) { (*tabbar_vect)(f, x1, y1, x2, y2, sp, tor, id); }

static ami_tabbarclient_t tabbarclient_vect = tabbarclient_ivf;
void _pa_tabbarclient_ovr(ami_tabbarclient_t nfp, ami_tabbarclient_t* ofp) { *ofp = tabbarclient_vect; tabbarclient_vect = nfp; }
void ami_tabbarclient(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { (*tabbarclient_vect)(f, tor, w, h, cw, ch, ox, oy); }

static ami_tabbarclientg_t tabbarclientg_vect = tabbarclientg_ivf;
void _pa_tabbarclientg_ovr(ami_tabbarclientg_t nfp, ami_tabbarclientg_t* ofp) { *ofp = tabbarclientg_vect; tabbarclientg_vect = nfp; }
void ami_tabbarclientg(FILE* f, ami_tabori tor, ami_long w, ami_long h, ami_long* cw, ami_long* ch, ami_long* ox, ami_long* oy) { (*tabbarclientg_vect)(f, tor, w, h, cw, ch, ox, oy); }

static ami_tabbarg_t tabbarg_vect = tabbarg_ivf;
void _pa_tabbarg_ovr(ami_tabbarg_t nfp, ami_tabbarg_t* ofp) { *ofp = tabbarg_vect; tabbarg_vect = nfp; }
void ami_tabbarg(FILE* f, ami_long x1, ami_long y1, ami_long x2, ami_long y2, ami_strptr sp, ami_tabori tor, ami_long id) { (*tabbarg_vect)(f, x1, y1, x2, y2, sp, tor, id); }

static ami_tabbarsiz_t tabbarsiz_vect = tabbarsiz_ivf;
void _pa_tabbarsiz_ovr(ami_tabbarsiz_t nfp, ami_tabbarsiz_t* ofp) { *ofp = tabbarsiz_vect; tabbarsiz_vect = nfp; }
void ami_tabbarsiz(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { (*tabbarsiz_vect)(f, sp, tor, cw, ch, w, h, ox, oy); }

static ami_tabbarsizg_t tabbarsizg_vect = tabbarsizg_ivf;
void _pa_tabbarsizg_ovr(ami_tabbarsizg_t nfp, ami_tabbarsizg_t* ofp) { *ofp = tabbarsizg_vect; tabbarsizg_vect = nfp; }
void ami_tabbarsizg(FILE* f, ami_strptr sp, ami_tabori tor, ami_long cw, ami_long ch, ami_long* w, ami_long* h, ami_long* ox, ami_long* oy) { (*tabbarsizg_vect)(f, sp, tor, cw, ch, w, h, ox, oy); }

static ami_tabsel_t tabsel_vect = tabsel_ivf;
void _pa_tabsel_ovr(ami_tabsel_t nfp, ami_tabsel_t* ofp) { *ofp = tabsel_vect; tabsel_vect = nfp; }
void ami_tabsel(FILE* f, ami_long id, ami_long tn) { (*tabsel_vect)(f, id, tn); }

static ami_timer_t timer_vect = timer_ivf;
void _pa_timer_ovr(ami_timer_t nfp, ami_timer_t* ofp) { *ofp = timer_vect; timer_vect = nfp; }
void ami_timer(FILE* f, ami_long i, ami_long t, ami_long r) { (*timer_vect)(f, i, t, r); }

static ami_title_t title_vect = title_ivf;
void _pa_title_ovr(ami_title_t nfp, ami_title_t* ofp) { *ofp = title_vect; title_vect = nfp; }
void ami_title(FILE* f, char* ts) { (*title_vect)(f, ts); }

static ami_underline_t underline_vect = underline_ivf;
void _pa_underline_ovr(ami_underline_t nfp, ami_underline_t* ofp) { *ofp = underline_vect; underline_vect = nfp; }
void ami_underline(FILE* f, ami_long e) { (*underline_vect)(f, e); }

static ami_up_t up_vect = up_ivf;
void _pa_up_ovr(ami_up_t nfp, ami_up_t* ofp) { *ofp = up_vect; up_vect = nfp; }
void ami_up(FILE* f) { (*up_vect)(f); }

static ami_viewoffg_t viewoffg_vect = viewoffg_ivf;
void _pa_viewoffg_ovr(ami_viewoffg_t nfp, ami_viewoffg_t* ofp) { *ofp = viewoffg_vect; viewoffg_vect = nfp; }
void ami_viewoffg(FILE* f, ami_long x, ami_long y) { (*viewoffg_vect)(f, x, y); }

static ami_viewscale_t viewscale_vect = viewscale_ivf;
void _pa_viewscale_ovr(ami_viewscale_t nfp, ami_viewscale_t* ofp) { *ofp = viewscale_vect; viewscale_vect = nfp; }
void ami_viewscale(FILE* f, float x, float y) { (*viewscale_vect)(f, x, y); }

static ami_winclient_t winclient_vect = winclient_ivf;
void _pa_winclient_ovr(ami_winclient_t nfp, ami_winclient_t* ofp) { *ofp = winclient_vect; winclient_vect = nfp; }
void ami_winclient(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms) { (*winclient_vect)(f, cx, cy, wx, wy, ms); }

static ami_winclientg_t winclientg_vect = winclientg_ivf;
void _pa_winclientg_ovr(ami_winclientg_t nfp, ami_winclientg_t* ofp) { *ofp = winclientg_vect; winclientg_vect = nfp; }
void ami_winclientg(FILE* f, ami_long cx, ami_long cy, ami_long* wx, ami_long* wy, ami_winmodset ms) { (*winclientg_vect)(f, cx, cy, wx, wy, ms); }

static ami_writejust_t writejust_vect = writejust_ivf;
void _pa_writejust_ovr(ami_writejust_t nfp, ami_writejust_t* ofp) { *ofp = writejust_vect; writejust_vect = nfp; }
void ami_writejust(FILE* f, const char* s, ami_long n) { (*writejust_vect)(f, s, n); }

static ami_wrtstr_t wrtstr_vect = wrtstr_ivf;
void _pa_wrtstr_ovr(ami_wrtstr_t nfp, ami_wrtstr_t* ofp) { *ofp = wrtstr_vect; wrtstr_vect = nfp; }
void ami_wrtstr(FILE* f, char* s) { (*wrtstr_vect)(f, s); }

static ami_wrtstrn_t wrtstrn_vect = wrtstrn_ivf;
void _pa_wrtstrn_ovr(ami_wrtstrn_t nfp, ami_wrtstrn_t* ofp) { *ofp = wrtstrn_vect; wrtstrn_vect = nfp; }
void ami_wrtstrn(FILE* f, char* s, ami_long n) { (*wrtstrn_vect)(f, s, n); }

static ami_xbold_t xbold_vect = xbold_ivf;
void _pa_xbold_ovr(ami_xbold_t nfp, ami_xbold_t* ofp) { *ofp = xbold_vect; xbold_vect = nfp; }
void ami_xbold(FILE* f, ami_long e) { (*xbold_vect)(f, e); }

static ami_xlight_t xlight_vect = xlight_ivf;
void _pa_xlight_ovr(ami_xlight_t nfp, ami_xlight_t* ofp) { *ofp = xlight_vect; xlight_vect = nfp; }
void ami_xlight(FILE* f, ami_long e) { (*xlight_vect)(f, e); }
