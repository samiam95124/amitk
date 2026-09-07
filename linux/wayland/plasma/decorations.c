/** ****************************************************************************
*                                                                              *
*                   WINDOW DECORATIONS FOR THE KDE PLASMA DESKTOP              *
*                                                                              *
* The look the Wayland backend wears on Plasma: Breeze window frames, with     *
* the title centered on a flat header and the minimize, maximize and close     *
* buttons drawn as plain glyphs at the right; and Breeze menus, the pressed    *
* entry filled with the desktop's accent color.                                *
*                                                                              *
* The colors are the desktop's own. Plasma keeps them in kdeglobals, which     *
* is read at startup and again whenever it changes, so a color scheme          *
* switch reaches the frames while the program runs. Breeze's own colors        *
* stand in when the file cannot be read.                                       *
*                                                                              *
* Only what Plasma does differently lives here. The window and menu            *
* mechanism -- tracking, opening, event routing, dragging, resizing -- is      *
* graphics.c's, and reaches this module through the vector in graphics_i.h.    *
*                                                                              *
*******************************************************************************/

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include "../graphics_i.h"

/* menu bar spacing, in addition to the text size */
#define EXTRAMENUY 10  /* extra space for menu bar y */
#define EXTRAMENUX 10  /* extra space for menu bar x */

/* child frame dimensions (Ami-drawn frames for child windows).
   Title bar height is based on the window's initial font line height so it
   scales with the configured text size; border and button sizes are
   proportional to the title bar. The metrics freeze at window creation
   (the frm* fields): the chrome must hold still while the client changes
   fonts under it. Breeze wears a slimmer title bar than Adwaita and sets
   its buttons further apart. */
#define CFRM_TITBAR_H(win)  ((win)->frmtbh)
#define CFRM_BORDER_W       4  /* resize border width in pixels */
/* input grab ring for resize: wider than the drawn border and scaled with
   the text size, so the edge is a hittable target on high densities. The
   ring rides over the client edge; the drawn frame stays CFRM_BORDER_W */
#define CFRM_GRAB_W(win)    ((win)->frmgrab)
#define CFRM_CORNER         16 /* corner resize grab length in pixels: near a
                                  corner the edge band widens to this so the
                                  diagonal resize is not a tiny CFRM_BORDER_W
                                  square that is nearly impossible to hit */
#define CFRM_BUTTON_SZ(win) ((win)->frmbsz)
#define CFRM_BUTTON_GAP     0  /* Breeze sets its button boxes adjacent */
#define CFRM_BUTTON_MG      10 /* button margin from edge */
#define CFRM_TITLE_SZ(win)  ((win)->frmtsz)
#define CFRM_MIN_W          200 /* width of a minimized child window */

/* Breeze accent, the blue everything selected wears */
#define BRZ_ACCENT 0x3daee9

/** ****************************************************************************

The Breeze palette

Plasma's colors live in kdeglobals: the header group carries the title bar
of a window, the window group the ground menus stand on. The file is read
into the palette below, and Breeze's own colors are the fallback when a
key, or the whole file, is missing. A scheme is called dark by the
luminance of its header, so a scheme nobody has named still lands right.

*******************************************************************************/

typedef struct {

    unsigned long bg;      /* frame and title bar */
    unsigned long text;    /* title text, focused */
    unsigned long textun;  /* title text, unfocused */
    unsigned long btnfg;   /* button glyphs, focused */
    unsigned long btnfgun; /* button glyphs, unfocused */
    unsigned long menubg;  /* menu ground */
    unsigned long menufg;  /* menu text */
    unsigned long menudis; /* menu text, disabled */
    unsigned long menulin; /* menu frame and separator lines */
    unsigned long selbg;   /* selected menu entry ground */
    unsigned long selfg;   /* selected menu entry text */

} frmpal;

/* Breeze light, and Breeze Dark, as the defaults */
static const frmpal brzlight = { 0xdee0e2, 0x232629, 0x707d8a,
                                 0x232629, 0x707d8a,
                                 0xeff0f1, 0x232629, 0x9b9fa2, 0xbdc3c7,
                                 BRZ_ACCENT, 0xffffff };
static const frmpal brzdark  = { 0x31363b, 0xeff0f1, 0x7f8c8d,
                                 0xeff0f1, 0x7f8c8d,
                                 0x31363b, 0xeff0f1, 0x7f8c8d, 0x4d4d4d,
                                 BRZ_ACCENT, 0xffffff };

static frmpal brzpal;         /* the palette in force */
static int   brzread;         /* the palette has been read */
static pthread_mutex_t brzlk = PTHREAD_MUTEX_INITIALIZER; /* the palette's
                                 lock: it is read from the desktop's file on
                                 first use, by whichever thread draws the
                                 first frame, and again on a theme change */
static int   brzdrk;          /* the scheme in force is a dark one */
static int   brzpin = -1;     /* config pinned scheme, -1 for none */

/* read one "r,g,b" key from a group of the config file, TRUE if found */

static int cfgcolor(FILE* fp, const char* group, const char* key,
                    unsigned long* c)

{

    char  line[256];
    int   ingrp = FALSE;
    int   r, g, b;
    size_t kl = strlen(key);

    rewind(fp);
    while (fgets(line, sizeof(line), fp)) {

        if (line[0] == '[') { /* a group header */

            ingrp = !strncmp(line, group, strlen(group));
            continue;

        }
        if (!ingrp) continue;
        if (strncmp(line, key, kl) || line[kl] != '=') continue;
        if (sscanf(line+kl+1, "%d,%d,%d", &r, &g, &b) != 3) return (FALSE);
        *c = ((unsigned long)r<<16)|((unsigned long)g<<8)|(unsigned long)b;

        return (TRUE);

    }

    return (FALSE);

}

/* the luminance of a color, 0-255 */

static int lumin(unsigned long c)

{

    return ((((c>>16)&0xff)*30+((c>>8)&0xff)*59+(c&0xff)*11)/100);

}

/* Read the desktop's colors. Whatever kdeglobals does not answer for is
   taken from the Breeze default of the same light or dark cast. */

static void readpal(void)

{

    FILE*         fp;
    char          path[512];
    const char*   home;
    unsigned long c;
    const frmpal* def;

    brzread = TRUE;
    fp = NULL;
    home = getenv("XDG_CONFIG_HOME");
    if (home && home[0]) snprintf(path, sizeof(path), "%s/kdeglobals", home);
    else {

        home = getenv("HOME");
        if (home) snprintf(path, sizeof(path), "%s/.config/kdeglobals", home);
        else path[0] = 0;

    }
    if (path[0]) fp = fopen(path, "r");
    /* the cast of the scheme decides which defaults fill the gaps */
    brzdrk = 0;
    if (brzpin >= 0) brzdrk = brzpin; /* the config pinned it */
    else if (fp && cfgcolor(fp, "[Colors:Header]", "BackgroundNormal", &c))
        brzdrk = lumin(c) < 128;
    else if (fp && cfgcolor(fp, "[Colors:Window]", "BackgroundNormal", &c))
        brzdrk = lumin(c) < 128;
    def = brzdrk? &brzdark: &brzlight;
    brzpal = *def;
    if (!fp) return; /* no file: Breeze's own colors stand */

    if (cfgcolor(fp, "[Colors:Header]", "BackgroundNormal", &c)) brzpal.bg = c;
    if (cfgcolor(fp, "[Colors:Header]", "ForegroundNormal", &c)) {

        brzpal.text = c;
        brzpal.btnfg = c;

    }
    if (cfgcolor(fp, "[Colors:Header]", "ForegroundInactive", &c)) {

        brzpal.textun = c;
        brzpal.btnfgun = c;

    }
    if (cfgcolor(fp, "[Colors:Window]", "BackgroundNormal", &c))
        brzpal.menubg = c;
    if (cfgcolor(fp, "[Colors:Window]", "ForegroundNormal", &c))
        brzpal.menufg = c;
    if (cfgcolor(fp, "[Colors:Window]", "ForegroundInactive", &c))
        brzpal.menudis = c;
    if (cfgcolor(fp, "[Colors:Window]", "DecorationFocus", &c))
        brzpal.selbg = c;
    if (cfgcolor(fp, "[Colors:Selection]", "ForegroundNormal", &c))
        brzpal.selfg = c;
    fclose(fp);

}

static const frmpal* framepal(void)

{

    pthread_mutex_lock(&brzlk);
    if (!brzread) readpal();
    pthread_mutex_unlock(&brzlk);

    return (&brzpal);

}

/* pin the palette, from the config file */

static void pl_setscheme(int dark)

{

    brzpin = !!dark;
    brzread = FALSE; /* reread against the pinned cast */

}

/* an Ami color from an RGB triple, for the menu drawing, which goes
   through the Petit-Ami calls rather than the platform layer */

static void amicolor(FILE* f, unsigned long c)

{

    ami_fcolorg(f, LONG_MAX/256*((c>>16)&0xff), LONG_MAX/256*((c>>8)&0xff),
                   LONG_MAX/256*(c&0xff));

}

/** ****************************************************************************

Frame metrics

The title bar height follows the window's opening font line height, so the
chrome scales with the configured text size, and the border and button
sizes are proportional to it. Breeze's bar is slimmer than Adwaita's and
its buttons are glyphs rather than filled circles, so they are drawn
smaller. The metrics freeze in the window at creation: the chrome must
hold still while the client changes fonts under it.

*******************************************************************************/

static void pl_frmmetrics(winptr win)

{

    win->frmtbh = (int)(win->linespace*1.9);
    /* the button box is nearly the height of the bar, as Breeze's is; the
       glyph inside takes a little under half of it */
    win->frmbsz = (int)(win->linespace*1.6);
    win->frmtsz = (int)(win->gfhigh*1.1);
    win->frmgrab = win->linespace/3 > CFRM_BORDER_W?
                   win->linespace/3: CFRM_BORDER_W;

}

/* The room the chrome takes around the client, and where the client sits
   inside it, for a given window mode set: the whole chrome with a title
   bar, the border alone with the system bar off, nothing with the frame
   off. */

static void pl_frmgeom(winptr win, ami_winmodset ms,
                       int* pfw, int* pfh, int* cwox, int* cwoy)

{

    if (!(BIT(ami_wmframe) & ms)) { /* no frame at all */

        *pfw = 0;
        *pfh = 0;
        *cwox = 0;
        *cwoy = 0;

    } else if (!(BIT(ami_wmsysbar) & ms)) { /* border, no title bar */

        *pfw = CFRM_BORDER_W*2;
        *pfh = CFRM_BORDER_W*2;
        *cwox = CFRM_BORDER_W;
        *cwoy = CFRM_BORDER_W;

    } else { /* the whole chrome */

        *pfw = CFRM_BORDER_W*2;
        *pfh = CFRM_TITBAR_H(win)+CFRM_BORDER_W;
        *cwox = CFRM_BORDER_W;
        *cwoy = CFRM_TITBAR_H(win);

    }

}

/* the bar a minimized child collapses to: title bar height, and wide
   enough to carry a title */

static void pl_frmminsize(winptr win, int* w, int* h)

{

    *w = CFRM_MIN_W;
    *h = CFRM_TITBAR_H(win);

}

/* Determine which resize edges the pointer (mx,my) is over for a child frame of
   master size mw x mh. Edges are CFRM_GRAB_W thick, thinning to the drawn
   CFRM_BORDER_W beside the title row so the title and its buttons keep
   their clicks; within CFRM_CORNER of a corner the grab widens along both
   edges so the diagonal (corner) resize is easy to hit rather than a tiny
   square. Shared by the cursor feedback and the resize hit test so the two
   never disagree. */

static void pl_frmedges(winptr win, int mx, int my, int mw, int mh,
                        int* left, int* right, int* top, int* bottom)
{
    int bw     = CFRM_GRAB_W(win);
    int hastit = win->cwoy > win->cwox;
    int titrow = hastit && my < win->cwoy;
    int topw   = hastit? CFRM_BORDER_W: bw;
    int sidew  = titrow? CFRM_BORDER_W: bw;
    int on_left   = mx < sidew;
    int on_right  = mx >= mw - sidew;
    int on_top    = my < topw;
    int on_bottom = my >= mh - bw;
    int near_left   = mx < CFRM_CORNER;
    int near_right  = mx >= mw - CFRM_CORNER;
    int near_top    = my < CFRM_CORNER;
    int near_bottom = my >= mh - CFRM_CORNER;
    /* on an edge -- plus, when also on a perpendicular edge and within the
       corner length of it, promote the hit to that corner (L-shaped grab) */
    *left   = on_left   || ((on_top || on_bottom) && near_left);
    *right  = on_right  || ((on_top || on_bottom) && near_right);
    *top    = on_top    || ((on_left || on_right) && near_top);
    *bottom = on_bottom || ((on_left || on_right) && near_bottom);
}

/* What the pointer is over in the chrome. The buttons are right aligned
   in the title bar, minimize | maximize | close, laid out exactly as
   pl_frmdraw paints them; the title is what is left of them, less the
   resize borders that overlap the title row. */

static dechit pl_frmhit(winptr win, int mx, int my)

{

    int mw  = win->xmwr.w;
    int tbh = CFRM_TITBAR_H(win);
    int bsz = CFRM_BUTTON_SZ(win);
    int hastit = win->cwoy > win->cwox; /* the geometry reserves a title */
    int cbx, cby, mabx, mibx;

    if (!hastit) return (dechnone); /* border only: no buttons, no title */
    cbx  = mw-bsz-CFRM_BUTTON_MG;
    cby  = (tbh-bsz)/2;
    mabx = cbx-bsz-CFRM_BUTTON_GAP;
    mibx = mabx-bsz-CFRM_BUTTON_GAP;
    if (my >= cby && my < cby+bsz) { /* on the button row */

        if (mx >= cbx && mx < cbx+bsz) return (dechclose);
        if (mx >= mabx && mx < mabx+bsz) return (dechmax);
        if (mx >= mibx && mx < mibx+bsz) return (dechmin);

    }
    if (my >= CFRM_BORDER_W && my < tbh &&
        mx >= CFRM_BORDER_W && mx < mw-CFRM_BORDER_W) return (dechtitle);

    return (dechnone);

}

/** ****************************************************************************

Draw the window frame

Renders the Breeze chrome on the master window: the flat header in the
desktop's header color, the title centered on it, and the three button
glyphs at the right. Breeze draws no circle behind a button, just the
glyph, and dims the whole bar when the window is not focused.

*******************************************************************************/

static void pl_frmdraw(winptr win, int mw, int mh)

{

    int tbh;          /* title bar height */
    int bsz;          /* button box */
    int by;           /* button y position */
    int bx_close;     /* close button x */
    int bx_max;       /* maximize button x */
    int bx_min;       /* minimize button x */
    int tlen;         /* title text pixel length */
    int title_size;   /* title font pixel size */
    int hastit;       /* title bar present in the frame geometry */

    if (!win->childfrm || !win->frmgc || !win->linespace) return;
    if (!win->frame) return; /* frame is turned off -- nothing to draw */
    /* the geometry carries the chrome: a title bar reserves more height at
       the top than the border width (sysbar off leaves border only) */
    hastit = win->cwoy > win->cwox;
    /* Paint against the layer's live master size, not the caller's tracked
       one: a configure can land between the caller reading its tracking
       and this paint, and chrome laid out for the stale width leaves the
       bar half-painted at one size and titled for another */
    {
        int gx, gy, gm;

        pd_wingeom(win->xmwhan, &gx, &gy, &mw, &mh, &gm);
    }

    tbh = CFRM_TITBAR_H(win);
    bsz = CFRM_BUTTON_SZ(win);
    title_size = CFRM_TITLE_SZ(win);

    /* fill the master frame background as a plain rectangle. The frame has
       square corners: rounded corners are not workable for a nested child
       window (the shape mask that would round them also clips the window's
       input region, making the corner un-grabbable), so we keep them square
       and fully grabbable -- and Breeze's own corners are nearly square */
    win->frmgc->fg = framepal()->bg; /* the desktop's header color */
    pd_frect(pd_wincanvas(win->xmwhan), win->frmgc, 0, 0, mw, mh);

    /* button row layout: minimize | maximize | close, right-aligned */
    by = (tbh - bsz) / 2;
    bx_close = mw - bsz - CFRM_BUTTON_MG;
    bx_max   = bx_close - bsz - CFRM_BUTTON_GAP;
    bx_min   = bx_max   - bsz - CFRM_BUTTON_GAP;

    /* draw title text, centered in what the buttons leave. If the title is
       too wide for it, truncate and append "..." */
    if (hastit && win->wintitle && win->wintitle[0] && win->ftface) {

        int len = strlen(win->wintitle);
        int tleft = CFRM_BUTTON_MG; /* title left margin */
        int avail = bx_min - CFRM_BUTTON_GAP - tleft; /* space for title */

        /* The face carries whatever size the client last drew with;
           measurement must run at the chrome's own title size or the
           layout truncates and justifies against phantom widths. The
           lock is held from here through the drawing: the face is shared,
           and a program drawing from another thread -- the remote display
           server does -- would otherwise render through it in between and
           leave it at its own size. */
        FT_Set_Pixel_Sizes(win->ftface, title_size, title_size);
        tlen = grx_ft_text_width(win, win->wintitle, len);
        int ty = (tbh + title_size) / 2 - 2;

        win->frmgc->fg = win->focus? framepal()->text: framepal()->textun;
        if (tlen <= avail) {

            /* title fits: center it in the available space */
            int tx = tleft + (avail - tlen) / 2;
            grx_ft_draw_string(pd_wincanvas(win->xmwhan), win->frmgc,
                               win, title_size, title_size,
                               tx, ty, win->wintitle, len);

        } else {

            /* truncate with "..." */
            int dotw = grx_ft_text_width(win, "...", 3);
            if (avail > dotw) {

                int tw = 0;
                int tl;
                for (tl = 0; tl < len; tl++) {

                    int cw = 0;
                    if (FT_Load_Char(win->ftface,
                                     (unsigned char)win->wintitle[tl],
                                     FT_LOAD_DEFAULT) == 0)
                        cw = (int)(win->ftface->glyph->advance.x >> 6);
                    if (tw + cw + dotw > avail) break;
                    tw += cw;

                }
                grx_ft_draw_string(pd_wincanvas(win->xmwhan), win->frmgc,
                                   win, title_size, title_size,
                                   tleft, ty, win->wintitle, tl);
                grx_ft_draw_string(pd_wincanvas(win->xmwhan), win->frmgc,
                                   win, title_size, title_size,
                                   tleft + tw, ty, "...", 3);

            }

        }

    }

    if (hastit) {

        /* Breeze buttons are glyphs on the bar itself, no circle behind
           them: a chevron down for minimize, a chevron up for maximize --
           a diamond when the window is already maximized, which is
           Breeze's restore -- and a cross for close. They dim with the
           title when the window loses focus */
        const frmpal* pal = framepal();
        int margin = bsz*5/16;      /* glyph inset in the button box */
        int gxc = bsz/2;            /* glyph center */
        int gyc = bsz/2;
        /* the glyphs are all built on one half span: the chevrons rise it
           over a span of twice it, so their arms lie at 45 degrees, the
           cross squares off the same box, and the diamond takes it as
           the half diagonal -- which is how Breeze draws all three */
        int hs  = (bsz-margin*2)/2;

        win->frmgc->fg = win->focus? pal->btnfg: pal->btnfgun;
        win->frmgc->lw = 2; win->frmgc->lstyle = pd_linesolid;

        /* minimize: a chevron pointing down */
        pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                bx_min+gxc-hs, by+gyc-hs/2, bx_min+gxc, by+gyc+hs/2);
        pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                bx_min+gxc, by+gyc+hs/2, bx_min+gxc+hs, by+gyc-hs/2);

        if (win->winstate == 1) {

            /* restore: a diamond, the maximized window's own glyph */
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_max+gxc, by+gyc-hs, bx_max+gxc+hs, by+gyc);
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_max+gxc+hs, by+gyc, bx_max+gxc, by+gyc+hs);
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_max+gxc, by+gyc+hs, bx_max+gxc-hs, by+gyc);
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_max+gxc-hs, by+gyc, bx_max+gxc, by+gyc-hs);

        } else {

            /* maximize: a chevron pointing up */
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_max+gxc-hs, by+gyc+hs/2, bx_max+gxc, by+gyc-hs/2);
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_max+gxc, by+gyc-hs/2, bx_max+gxc+hs, by+gyc+hs/2);

        }

        /* close: a cross, square and a shade narrower than the chevrons,
           as Breeze draws it */
        {
            int xh = hs*5/6; /* half the cross, on both axes */

            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_close+gxc-xh, by+gyc-xh, bx_close+gxc+xh, by+gyc+xh);
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                    bx_close+gxc+xh, by+gyc-xh, bx_close+gxc-xh, by+gyc+xh);

        }

        win->frmgc->lw = 1; win->frmgc->lstyle = pd_linesolid;

    }

    /* A toplevel's frame is compositor-interactive: declare the regions so
       a title press becomes an interactive move and a border press an
       interactive resize. The declared title stops short of the buttons,
       whose presses must reach us. */
    if (!win->parwin) {

        if (hastit)
            pd_winframe(win->xmwhan,
                        CFRM_BORDER_W, CFRM_BORDER_W,
                        bx_min - CFRM_BUTTON_GAP - CFRM_BORDER_W,
                        tbh - CFRM_BORDER_W, CFRM_GRAB_W(win));
        else
            /* border only: no title region to drag by */
            pd_winframe(win->xmwhan, 0, 0, 0, 0, CFRM_GRAB_W(win));

    }

    /* no flush here: the frame is drawn under the window's lock, and the
       damage it marked is composed by the live beat or the event loop's
       next pump, with no lock of ours held */

}

/** ****************************************************************************

Follow the desktop color scheme

Plasma has no monitor command of its own: it writes the colors into
kdeglobals and the toolkits notice. We do the same, watching the config
directory for writes to that file. The watch descriptor rides the system
event loop, and each event rereads the palette.

*******************************************************************************/

static int pl_thememon(void)

{

    int         fd;
    char        path[512];
    const char* home;

    fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
    if (fd < 0) return (-1);
    home = getenv("XDG_CONFIG_HOME");
    if (home && home[0]) snprintf(path, sizeof(path), "%s", home);
    else {

        home = getenv("HOME");
        if (!home) { close(fd); return (-1); }
        snprintf(path, sizeof(path), "%s/.config", home);

    }
    /* the settings are rewritten by replacement, so the directory carries
       the news: kdeglobals arrives as a new file rather than a write */
    if (inotify_add_watch(fd, path,
                          IN_CLOSE_WRITE|IN_MOVED_TO|IN_CREATE) < 0) {

        close(fd);

        return (-1);

    }

    return (fd);

}

static int pl_themechg(int fd)

{

    char   buf[4096];
    char*  p;
    int    n;
    int    ours = FALSE;
    frmpal old;

    n = read(fd, buf, sizeof(buf));
    if (n <= 0) return (FALSE);
    /* walk the events, looking for the settings file */
    for (p = buf; p < buf+n; ) {

        struct inotify_event* ev = (struct inotify_event*)p;

        if (ev->len && !strncmp(ev->name, "kdeglobals", 10)) ours = TRUE;
        p += sizeof(struct inotify_event)+ev->len;

    }
    if (!ours) return (FALSE);
    pthread_mutex_lock(&brzlk);
    old = brzpal;
    readpal(); /* take the colors again */
    pthread_mutex_unlock(&brzlk);

    return (memcmp(&old, &brzpal, sizeof(frmpal)) != 0);

}

/** ****************************************************************************

Menus

Breeze menus are flat: the bar entries sit on the window ground and the
one pressed is filled with the accent color and titled in white. A
pulldown draws a single line border around its entries.

*******************************************************************************/

/* Menu metrics: an entry is a text line with room around it, a pulldown
   pads its entries out for the select and branch marks at either side,
   and its frame draws a single line box around them. */

static int pl_menuheight(winptr win)

{

    return (win->linespace+EXTRAMENUY);

}

static void pl_menumetrics(winptr win, int* itemextra, int* frmpad,
                           int* barextra)

{

    /* pad to sides, 2 squares, left for select, right for branch arrows,
       plus extra padding */
    *itemextra = win->menuspcy*2+win->menuspcy/2;
    *frmpad = 4;
    *barextra = EXTRAMENUX;

}

/* draw menu entry title, in the color its state calls for */

static void menu_draw(metptr mp, int selected)

{

    const frmpal* pal = framepal();

    if (mp->title) { /* there is a title */

        if (selected) amicolor(mp->wf, pal->selfg);
        else if (mp->ena) amicolor(mp->wf, pal->menufg);
        else amicolor(mp->wf, pal->menudis);
        if (mp->prime)
            ami_cursorg(mp->wf, 1,
                        ami_maxyg(mp->wf)/2-ami_chrsizy(mp->wf)/2);
        else ami_cursorg(mp->wf, ami_maxyg(mp->wf),
                         ami_maxyg(mp->wf)/2-ami_chrsizy(mp->wf)/2);
        fprintf(mp->wf, "%s", mp->title); /* place button title */
        /* if selected and on/off highlighted, place checkmark */
        if (mp->select && !mp->prime) {

            ami_linewidth(mp->wf, 3);
            ami_line(mp->wf, ami_maxyg(mp->wf)/4, ami_maxyg(mp->wf)/2,
                     ami_maxyg(mp->wf)/2,
                     ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3);
            ami_line(mp->wf, ami_maxyg(mp->wf)/2,
                     ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3,
                     ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/6,
                     ami_maxyg(mp->wf)/3);
            ami_linewidth(mp->wf, 1);

        }

    }

}

/* Paint a menu entry: the bar entries and the pulldown entries are each a
   window of their own, so painting one is painting its whole surface. A
   pressed entry is filled with the accent color, which is how Breeze
   marks the open menu and the entry under the pointer alike. */

static void pl_menupaint(metptr mp, decmstate st)

{

    const frmpal* pal = framepal();
    int           pressed = (st == decmpress) ||
                            (st == decmexpose && mp->pressed);

    /* the ground: accent when pressed, the window color otherwise */
    if (pressed) amicolor(mp->wf, pal->selbg);
    else amicolor(mp->wf, pal->menubg);
    ami_frect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
    menu_draw(mp, pressed); /* draw menu title */

    if (mp->frm) { /* box the frame */

        amicolor(mp->wf, pal->menulin);
        ami_rect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));

    }
    if (mp->bar && !mp->prime) { /* draw separator under */

        amicolor(mp->wf, pal->menulin);
        ami_line(mp->wf, 1, ami_maxyg(mp->wf), ami_maxxg(mp->wf),
                 ami_maxyg(mp->wf));

    }
    if (!mp->prime && mp->branch && !mp->menubar) { /* branch arrow */

        if (pressed) amicolor(mp->wf, pal->selfg);
        else amicolor(mp->wf, pal->menufg);
        ami_ftriangle(mp->wf,
                      ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/2,
                      ami_maxyg(mp->wf)/3,
                      ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/4,
                      ami_maxyg(mp->wf)/2,
                      ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/2,
                      ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3);

    }

}

/** ****************************************************************************

Registration

Both decorations modules link into every program; each tests the running
desktop and the one that belongs to it registers. This one takes KDE.
Registration must precede the backend's own initialization, which runs at
constructor priority 102.

*******************************************************************************/

static const decvec plasmadec = {

    .frmmetrics  = pl_frmmetrics,
    .frmgeom     = pl_frmgeom,
    .frmminsize  = pl_frmminsize,
    .frmdraw     = pl_frmdraw,
    .frmedges    = pl_frmedges,
    .frmhit      = pl_frmhit,
    .setscheme   = pl_setscheme,
    .thememon    = pl_thememon,
    .themechg    = pl_themechg,
    .menuheight  = pl_menuheight,
    .menumetrics = pl_menumetrics,
    .menupaint   = pl_menupaint

};

static int desksel(void)

{

    const char* d = getenv("XDG_CURRENT_DESKTOP");

    return (d && strstr(d, "KDE"));

}

static void init_decorations(void) __attribute__((constructor (101)));
static void init_decorations(void)

{

    if (!desksel()) return; /* not our desktop */
    grx_decreg(&plasmadec);

}
