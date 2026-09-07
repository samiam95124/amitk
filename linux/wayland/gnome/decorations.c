/** ****************************************************************************
*                                                                              *
*                    WINDOW DECORATIONS FOR THE GNOME DESKTOP                  *
*                                                                              *
* The look the Wayland backend wears on GNOME: Adwaita window frames, with     *
* the title centered and the round minimize, maximize and close buttons at     *
* the right, in the light or dark header color the desktop is set to; and      *
* the menu bar and pulldowns in the same flat white with an accent underbar    *
* on the pressed entry.                                                        *
*                                                                              *
* Only what GNOME does differently lives here. The window and menu            *
* mechanism -- tracking, opening, event routing, dragging, resizing -- is     *
* graphics.c's, and reaches this module through the vector in graphics_i.h.   *
*                                                                              *
*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "../graphics_i.h"

/* menu bar spacing, in addition to the text size */
#define EXTRAMENUY 10  /* extra space for menu bar y */
#define EXTRAMENUX 10  /* extra space for menu bar x */

/* child frame dimensions (Ami-drawn frames for child windows).
   Title bar height is based on the window's initial font line height so it
   scales with the configured text size; border and button sizes are
   proportional to the title bar. The metrics freeze at window creation
   (the frm* fields): the chrome must hold still while the client changes
   fonts under it. */
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
#define CFRM_BUTTON_GAP     6  /* gap between buttons */
#define CFRM_BUTTON_MG      8  /* button margin from edge */
#define CFRM_TITLE_SZ(win)  ((win)->frmtsz)

/* The frame palette follows the desktop's color scheme: the Adwaita
   light header on a light desktop, the dark one on a dark desktop. The
   config value frame_theme (light or dark) overrides detection */
typedef struct {

    unsigned long bg;      /* frame and title bar */
    unsigned long text;    /* title text, focused */
    unsigned long textun;  /* title text, unfocused */
    unsigned long btnbg;   /* button circles */
    unsigned long btnfg;   /* button glyphs, focused */
    unsigned long btnfgun; /* button glyphs, unfocused */

} frmpal;

static const frmpal frmlight = { 0xfafafa, 0x2e2e2e, 0x949390,
                                 0xe8e8e6, 0x2e2e2e, 0x949390 };
static const frmpal frmdark  = { 0x303030, 0xffffff, 0x808080,
                                 0x454545, 0xffffff, 0x808080 };
static int frmscheme = -1; /* -1 detect, 0 light, 1 dark */

/* pin the palette, from the config file */
static void gn_setscheme(int dark) { frmscheme = !!dark; }

/* Ask the desktop its color scheme, once. The stdio override makes
   popen a hazard, so the pipe runs on raw descriptors */
static const frmpal* framepal(void)

{

    int   pfd[2];
    pid_t pid;
    char  buf[128];
    ssize_t n;

    if (frmscheme < 0) {

        frmscheme = 0; /* the light desktop is the default */
        if (pipe(pfd) == 0) {

            pid = fork();
            if (pid == 0) {

                close(pfd[0]);
                dup2(pfd[1], 1);
                close(pfd[1]);
                execlp("gsettings", "gsettings", "get",
                       "org.gnome.desktop.interface", "color-scheme",
                       (char*)NULL);
                _exit(1);

            }
            close(pfd[1]);
            if (pid > 0) {

                n = read(pfd[0], buf, sizeof(buf)-1);
                if (n > 0) {

                    buf[n] = 0;
                    if (strstr(buf, "prefer-dark")) frmscheme = 1;

                }
                waitpid(pid, NULL, 0);

            }
            close(pfd[0]);

        }

    }
    return (frmscheme? &frmdark: &frmlight);

}
#define CFRM_MIN_W          200 /* width of a minimized child window */

/** ****************************************************************************

Frame metrics

The title bar height follows the window's opening font line height, so the
chrome scales with the configured text size, and the border and button
sizes are proportional to it. The metrics freeze in the window at creation:
the chrome must hold still while the client changes fonts under it.

*******************************************************************************/

static void gn_frmmetrics(winptr win)

{

    win->frmtbh = (int)(win->linespace*2.2);
    win->frmbsz = (int)(win->linespace*1.1);
    win->frmtsz = (int)(win->gfhigh*1.15);
    win->frmgrab = win->linespace/3 > CFRM_BORDER_W?
                   win->linespace/3: CFRM_BORDER_W;

}

/* The room the chrome takes around the client, and where the client sits
   inside it, for a given window mode set: the whole chrome with a title
   bar, the border alone with the system bar off, nothing with the frame
   off. */

static void gn_frmgeom(winptr win, ami_winmodset ms,
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

static void gn_frmminsize(winptr win, int* w, int* h)

{

    *w = CFRM_MIN_W;
    *h = CFRM_TITBAR_H(win);

}

/* What the pointer is over in the chrome. The buttons are right aligned
   in the title bar, minimize | maximize | close, laid out exactly as
   gn_frmdraw paints them; the title is what is left of them, less the
   resize borders that overlap the title row. */

static dechit gn_frmhit(winptr win, int mx, int my)

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

/* Menu metrics: an entry is a text line with room around it, a pulldown
   pads its entries out for the select and branch marks at either side,
   and its frame draws a two pixel box around them. */

static int gn_menuheight(winptr win)

{

    return (win->linespace+EXTRAMENUY);

}

static void gn_menumetrics(winptr win, int* itemextra, int* frmpad,
                           int* barextra)

{

    /* pad to sides, 2 squares, left for select, right for branch arrows,
       plus extra padding */
    *itemextra = win->menuspcy*2+win->menuspcy/2;
    *frmpad = 4;
    *barextra = EXTRAMENUX;

}

/* Determine which resize edges the pointer (mx,my) is over for a child frame of
   master size mw x mh. Edges are CFRM_GRAB_W thick, thinning to the drawn
   CFRM_BORDER_W beside the title row so the title and its buttons keep
   their clicks; within CFRM_CORNER of a corner the grab widens along both
   edges so the diagonal (corner) resize is easy to hit rather than a tiny
   square. Shared by the cursor feedback and the resize hit test so the two
   never disagree. */
static void gn_frmedges(winptr win, int mx, int my, int mw, int mh,
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


/** ****************************************************************************

Draw the window frame

Renders the Ami-drawn frame chrome on xmwhan for child windows. This includes
the title bar, close button, and resize border. Called after restore() and on
pd_etredraw events for xmwhan.

*******************************************************************************/

static void gn_frmdraw(winptr win, int mw, int mh)

{

    int tbh;          /* title bar height */
    int bsz;          /* button diameter */
    int by;           /* button y position */
    int bx_close;     /* close button x */
    int bx_max;       /* maximize button x */
    int bx_min;       /* minimize button x */
    int tlen;         /* title text pixel length */
    int title_size;   /* title font pixel size */
    int hastit;       /* title bar present in the frame geometry */

    if (!win->childfrm || !win->frmgc || !win->linespace) return;
    if (!win->frame) return; /* frame is turned off — nothing to draw */
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

    /* mw/mh are the master (xmwhan) outer dimensions, supplied by the caller.
       Every caller sets win->xmwr to match the XResize/XMoveResize it issues on
       xmwhan immediately before drawing, so the frame geometry is already known
       and there is no need to ask the server for it. The former XSync +
       XGetGeometry round trip here blocked on the compositor once per call and
       made interactive resize crawl, badly under XWayland. */
    tbh = CFRM_TITBAR_H(win);
    bsz = CFRM_BUTTON_SZ(win);
    title_size = CFRM_TITLE_SZ(win);


    /* fill the master frame background as a plain rectangle. The frame has
       square corners: rounded corners are not workable for a nested child
       window (the shape mask that would round them also clips the window's
       input region, making the corner un-grabbable, and X does not alpha-
       composite nested children against their parent), so we keep them square
       and fully grabbable. */
    win->frmgc->fg = framepal()->bg; /* the scheme's frame bg */
    pd_frect(pd_wincanvas(win->xmwhan), win->frmgc, 0, 0, mw, mh);

    /* draw title text - centered, using FreeType for native font rendering.
       If the title is too wide for the available space (between left edge and
       the leftmost button), truncate and append "..." */
    /* button row layout: minimize | maximize | close, right-aligned */
    by = (tbh - bsz) / 2;
    bx_close = mw - bsz - CFRM_BUTTON_MG;
    bx_max   = bx_close - bsz - CFRM_BUTTON_GAP;
    bx_min   = bx_max   - bsz - CFRM_BUTTON_GAP;

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
            grx_ft_draw_string(pd_wincanvas(win->xmwhan), win->frmgc, win,
                           title_size, title_size,
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
                grx_ft_draw_string(pd_wincanvas(win->xmwhan), win->frmgc, win,
                               title_size, title_size,
                               tleft, ty, win->wintitle, tl);
                grx_ft_draw_string(pd_wincanvas(win->xmwhan), win->frmgc, win,
                               title_size, title_size,
                               tleft + tw, ty, "...", 3);

            }

        }

    }

    if (hastit) {
        /* the scheme's button circles; glyphs dim without focus */
        const frmpal* pal = framepal();
        unsigned long btn_bg = pal->btnbg;
        unsigned long btn_fg = win->focus ? pal->btnfg : pal->btnfgun;
        unsigned long cls_bg = pal->btnbg;

        /* draw minimize button (circle with horizontal line) */
        win->frmgc->fg = btn_bg;
        pd_farcpie(pd_wincanvas(win->xmwhan), win->frmgc,
                 bx_min, by, bsz, bsz, 0, 360*64);
        win->frmgc->fg = btn_fg;
        win->frmgc->lw = 2; win->frmgc->lstyle = pd_linesolid;
        pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                  bx_min + bsz/4, by + bsz - bsz/3,
                  bx_min + bsz - bsz/4, by + bsz - bsz/3);

        /* draw maximize button (circle with square outline) */
        win->frmgc->fg = btn_bg;
        pd_farcpie(pd_wincanvas(win->xmwhan), win->frmgc,
                 bx_max, by, bsz, bsz, 0, 360*64);
        win->frmgc->fg = btn_fg;
        pd_rect(pd_wincanvas(win->xmwhan), win->frmgc,
                       bx_max + bsz/4, by + bsz/4,
                       bsz - bsz/2, bsz - bsz/2);

        /* draw close button (circle with X) */
        win->frmgc->fg = cls_bg;
        pd_farcpie(pd_wincanvas(win->xmwhan), win->frmgc,
                 bx_close, by, bsz, bsz, 0, 360*64);
        win->frmgc->fg = btn_fg;
        {
            int margin = bsz / 4;
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                      bx_close + margin, by + margin,
                      bx_close + bsz - margin - 1, by + bsz - margin - 1);
            pd_line(pd_wincanvas(win->xmwhan), win->frmgc,
                      bx_close + bsz - margin - 1, by + margin,
                      bx_close + margin, by + bsz - margin - 1);
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

    pd_winflush(win->xmwhan); /* this frame's toplevel, not every one */

}


/** ****************************************************************************

Follow the desktop color scheme

GNOME announces its light/dark setting through gsettings, and will print a
line per change if asked to monitor it. The monitor is a child process
whose pipe the backend watches; each line that arrives is read here and
the palette set from it.

*******************************************************************************/

static pid_t thmpid; /* scheme monitor process */

static int gn_thememon(void)

{

    int pfd[2];

    if (pipe(pfd)) return (-1);
    thmpid = fork();
    if (thmpid == 0) {

        close(pfd[0]);
        dup2(pfd[1], 1);
        close(pfd[1]);
        execlp("gsettings", "gsettings", "monitor",
               "org.gnome.desktop.interface", "color-scheme", (char*)NULL);
        _exit(1);

    }
    close(pfd[1]);
    if (thmpid < 0) { close(pfd[0]); return (-1); }
    fcntl(pfd[0], F_SETFL, O_NONBLOCK);

    return (pfd[0]);

}

static int gn_themechg(int fd)

{

    char    buf[256];
    ssize_t n;
    int     old = frmscheme;

    n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) return (FALSE);
    buf[n] = 0;
    if (strstr(buf, "prefer-dark")) frmscheme = 1;
    else frmscheme = 0;

    return (frmscheme != old);

}

/* draw menu button title */
static void menu_draw(metptr mp)

{

    if (mp->title) { /* there is a title */

        if (mp->ena) ami_fcolor(mp->wf, ami_black);
        else ami_fcolorg(mp->wf, LONG_MAX/256*150, LONG_MAX/256*150,
                               LONG_MAX/256*150);
        if (mp->prime)
            ami_cursorg(mp->wf, 1,
                       ami_maxyg(mp->wf)/2-ami_chrsizy(mp->wf)/2);
        else ami_cursorg(mp->wf, ami_maxyg(mp->wf),
                                ami_maxyg(mp->wf)/2-ami_chrsizy(mp->wf)/2);
        fprintf(mp->wf, "%s", mp->title); /* place button title */
        /* if selected and on/off highlighted, place checkmark */
        if (mp->select && !mp->prime) {

            ami_fcolor(mp->wf, ami_black);
            ami_linewidth(mp->wf, 4);
            ami_line(mp->wf, ami_maxyg(mp->wf)/4, ami_maxyg(mp->wf)/2,
                            ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3);
            ami_line(mp->wf, ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3,
                            ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/6, ami_maxyg(mp->wf)/3);
            ami_linewidth(mp->wf, 1);

        }

    }

}


/* Paint a menu entry: the bar entries and the pulldown entries are each a
   window of their own, so painting one is painting its whole surface. The
   three states differ in the ground the title sits on -- pressed lifts the
   entry out of the bar with a grey ground and the accent underbar. */

static void gn_menupaint(metptr mp, decmstate st)

{

    if (st == decmpress) { /* pressed: grey ground and accent underbar */

        ami_fcolorg(mp->wf, LONG_MAX-LONG_MAX/4, LONG_MAX-LONG_MAX/4,
                            LONG_MAX-LONG_MAX/4);
        ami_frect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
        menu_draw(mp); /* draw menu title */
        /* draw underbar */
        ami_fcolorg(mp->wf, LONG_MAX/256*233, LONG_MAX/256*84,
                            LONG_MAX/256*32);
        ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-4, ami_maxxg(mp->wf),
                            ami_maxyg(mp->wf));

    } else if (st == decmnorm) { /* at rest: white ground, divider under */

        ami_fcolor(mp->wf, ami_white);
        ami_frect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
        menu_draw(mp); /* draw menu title */
        ami_fcolorg(mp->wf,
                   LONG_MAX/256*223, LONG_MAX/256*223, LONG_MAX/256*223);
        ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-1,
                            ami_maxxg(mp->wf), ami_maxyg(mp->wf));

    } else { /* exposed: the entry in whatever state it stands */

    /* color the background */
    ami_fcolor(mp->wf, ami_white);
    ami_frect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
    menu_draw(mp); /* draw menu title */
    if (mp->pressed) {

        /* draw underbar */
        ami_fcolorg(mp->wf, LONG_MAX/256*233, LONG_MAX/256*84, LONG_MAX/256*32);
        ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-4, ami_maxxg(mp->wf), ami_maxyg(mp->wf));

    } else if (mp->prime) { /* draw divider line */

        ami_fcolorg(mp->wf,
                   LONG_MAX/256*223, LONG_MAX/256*223, LONG_MAX/256*223);
        ami_frect(mp->wf, 1, ami_maxyg(mp->wf)-1,
                            ami_maxxg(mp->wf), ami_maxyg(mp->wf));

    }
    if (mp->frm) { /* box the frame */

        ami_fcolorg(mp->wf,
                   LONG_MAX/256*150, LONG_MAX/256*150, LONG_MAX/256*150);
        ami_rect(mp->wf, 1, 1, ami_maxxg(mp->wf), ami_maxyg(mp->wf));
        ami_rect(mp->wf, 2, 2, ami_maxxg(mp->wf)-1, ami_maxyg(mp->wf)-1);

    }
    if (mp->bar && !mp->prime) { /* draw bar under */

        ami_fcolorg(mp->wf,
                   LONG_MAX/256*150, LONG_MAX/256*150, LONG_MAX/256*150);
        ami_line(mp->wf, 1, ami_maxyg(mp->wf), ami_maxxg(mp->wf),
                ami_maxyg(mp->wf));

    }
    if (!mp->prime && mp->branch && !mp->menubar) {

        ami_fcolor(mp->wf, ami_black);
        ami_ftriangle(mp->wf, ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)/3,
                             ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/4, ami_maxyg(mp->wf)/2,
                             ami_maxxg(mp->wf)-ami_maxyg(mp->wf)/2, ami_maxyg(mp->wf)-ami_maxyg(mp->wf)/3);

    }

    }

}

/** ****************************************************************************

Registration

Both decorations modules link into every program; each tests the running
desktop and the one that belongs to it registers. GNOME takes everything
that is not KDE, so a desktop nobody claims still gets frames and menus.
Registration must precede the backend's own initialization, which runs at
constructor priority 102.

*******************************************************************************/

static const decvec gnomedec = {

    .frmmetrics  = gn_frmmetrics,
    .frmgeom     = gn_frmgeom,
    .frmminsize  = gn_frmminsize,
    .frmdraw     = gn_frmdraw,
    .frmedges    = gn_frmedges,
    .frmhit      = gn_frmhit,
    .setscheme   = gn_setscheme,
    .thememon    = gn_thememon,
    .themechg    = gn_themechg,
    .menuheight  = gn_menuheight,
    .menumetrics = gn_menumetrics,
    .menupaint   = gn_menupaint

};

static int desksel(void)

{

    const char* d = getenv("XDG_CURRENT_DESKTOP");

    return (!(d && strstr(d, "KDE"))); /* anything that is not KDE */

}

static void init_decorations(void) __attribute__((constructor (101)));
static void init_decorations(void)

{

    if (!desksel()) return; /* not our desktop */
    grx_decreg(&gnomedec);

}
