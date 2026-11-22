/*******************************************************************************
*                                                                              *
*                          WIDGET BUSY BOX                                     *
*                                                                              *
*                    Copyright (C) 2026 Scott A. Franco                        *
*                                                                              *
* Every widget Ami has, on one page, with a button for each dialog. It is a    *
* place to look at the whole widget set at once: what each one is, what it     *
* looks like beside its neighbours, and what it says when you work it.         *
*                                                                              *
* The page is laid out in four columns -- controls, lists, bars and sliders,   *
* and components -- with the dialogs a row of buttons along the foot. Every    *
* widget is placed at the size its own sizing call asks for, which is the way  *
* a portable layout is meant to be built: the program never says how many      *
* pixels a checkbox takes, it asks.                                            *
*                                                                              *
* It asks for the window, too. The page is laid out twice: the first pass      *
* creates nothing and only adds up what the widgets want, which gives each     *
* column its width and the page its height, and the second pass places for     *
* real. Between them the window is set to hold what was measured. Note that    *
* setsizg() sets the whole window, frame and all, so winclientg() turns the    *
* client the page needs into the window to ask for -- without that step the    *
* foot of the page falls outside the window and the dialog row is cut off.     *
*                                                                              *
* The top line reports what the widgets say. Working any control writes there, *
* and each dialog writes back what the user chose, so what a query returns can *
* be seen as well as the query itself.                                         *
*                                                                              *
* Two behaviours are worth watching, because they are not the same:            *
*                                                                              *
* A scroll bar does not move its own slider. It reports where the user put it  *
* and waits for the program to agree, which this one does by calling           *
* scrollpos() with the position it was given. Take that call out and the       *
* slider springs back. A slider, by contrast, moves itself and only tells the  *
* program afterwards.                                                          *
*                                                                              *
* The progress bar runs on a timer, because a component with nothing to show   *
* teaches nothing. A button, a checkbox and a radio button are disabled, to    *
* show what a widget that cannot be worked looks like beside one that can, and *
* that a disabled widget still shows the state it holds.                       *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include <localdefs.h>
#include <graphics.h>

#define GAP     12    /* space between widgets */
#define PAD     8     /* slack added to a minimum size, for the fingers */
#define COLS    4     /* columns of widgets */
#define PROGTIM 1     /* timer that walks the progress bar */
#define PROGMS  1000  /* its period, in 100us units: 100ms */

/* Widget ids. Every widget on the page has one, and they are made and killed
   as a set, so the layout can be rebuilt on a resize. */
typedef enum {

    /* controls */
    wibutton = 1, wibutdis, wicheck, wichkdis, wiradio1, wiradio2, wiraddis,
    wiedit, winumsel, widrop,
    widropedit, wilist, wiscrollv, wiscrollh, wislidev, wislideh, witabbar,
    /* components */
    wigroup, wiback, wiprog,
    /* the dialog buttons */
    widalert, widcolor, widopen, widsave, widfind, widfindrep, widfont,
    wilast

} wigid;

static ami_long made[wilast];  /* which ids are on the page now */
static ami_long progpos;       /* where the progress bar has got to */
static ami_long scrollv, scrollh; /* where the program thinks the bars are */
static ami_long lastw, lasth;  /* the size the page was laid out for */

/* a column being filled downwards */
typedef struct { ami_long x, y, w; } column;

/*******************************************************************************

Report on the top line

The line is cleared to the window width first, so a short message cannot leave
the tail of a long one behind it.

*******************************************************************************/

static void status(const char* fmt, ...)

{

    va_list ap;
    char    b[256];
    ami_long    i, n;

    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    n = ami_maxx(stdout);
    ami_cursor(stdout, 1, 1);
    for (i = 0; i < n; i++) putchar(' ');
    ami_cursor(stdout, 1, 1);
    printf("%s", b);
    fflush(stdout);

}

/* a label above a widget, in graphical position */
static void label(ami_long x, ami_long y, const char* s)

{

    ami_cursorg(stdout, x, y);
    printf("%s", s);
    fflush(stdout);

}

/*******************************************************************************

String lists

The list, drop and tab widgets take a list of strings. The strings are
literals, which outlive the widgets that show them, so nothing here is copied
or freed.

*******************************************************************************/

static ami_strptr mklist(char* a, char* b, char* c)

{

    ami_strptr h, p;
    char*      s[3];
    int        i;

    s[0] = a; s[1] = b; s[2] = c;
    h = NULL;
    for (i = 2; i >= 0; i--) { /* built backwards: each entry heads the list */

        p = malloc(sizeof(ami_strrec));
        if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
        p->str = s[i];
        p->next = h;
        h = p;

    }

    return (h);

}


/*******************************************************************************

Laying out, and how big a window that needs

The page is laid out twice. The first pass creates nothing: it asks every
widget its size and adds them up, which gives the width of each column and how
far down the columns reach. From that the window the page needs is known, and
can be asked for. The second pass places for real, each column as wide as its
widest member.

Nothing here is a guess in pixels or in character heights. Every extent comes
from the widget that will stand in it, which is what the sizing calls are for
and what makes the page come out right in any font.

*******************************************************************************/

static ami_long colnat[COLS];  /* the natural width of each column */
static ami_long colx[COLS];    /* its left edge, once the widths are known */
static ami_long needw, needh;  /* the page the widgets add up to */
static ami_long measuring;     /* the pass that creates nothing */
static ami_long curcol, cury;  /* the column being filled, and how far down it */
static ami_long colbot;        /* the lowest any column reached */

/* start filling column n */
static void colbegin(ami_long n)

{

    curcol = n;
    cury = ami_chrsizy(stdout)*3; /* under the status line */

}

/* the width to give a widget that fills its column. While measuring the
   column has no width yet, and the widget's own minimum is what counts. */
static ami_long colwid(void)

{

    return (measuring? 0: colnat[curcol]);

}

/* take a slot w by h in the current column, and say where it went */
static void slot(ami_long w, ami_long h, ami_long* x, ami_long* y)

{

    if (w > colnat[curcol]) colnat[curcol] = w;
    *x = colx[curcol];
    *y = cury;
    cury += h+GAP;
    if (cury > colbot) colbot = cury;

}

/* a label above a widget: it takes a slot of its own */
static void slotlabel(const char* s)

{

    ami_long x, y;

    slot(ami_strsiz(stdout, (char*)s), ami_chrsizy(stdout)*5/4-GAP, &x, &y);
    if (!measuring) label(x, y, s);

}

/*******************************************************************************

Lay the page out

Every widget is placed at the size its sizing call asks for. Sizes come back as
a minimum: what it takes to hold the content and the frame, and no more. A
little is added for the fingers, as the manual advises for buttons.

*******************************************************************************/

static void place(void)

{

    ami_long   w, h, cw, ch, ox, oy, ow, oh, x, y, i;
    ami_long   rowy, roww, rowh;
    ami_strptr lp;
    static struct { ami_long id; char* face; } dlg[] = {

        { widalert,   (char*)"Alert"   },
        { widcolor,   (char*)"Color"   },
        { widopen,    (char*)"Open"    },
        { widsave,    (char*)"Save"    },
        { widfind,    (char*)"Find"    },
        { widfindrep, (char*)"Replace" },
        { widfont,    (char*)"Font"    }

    };
    #define NDLG ((ami_long)(sizeof(dlg)/sizeof(dlg[0])))

    for (i = 0; i < wilast; i++) made[i] = FALSE;
    colbot = 0;

    /* ------------------------------------------------ column 1: controls */
    colbegin(0);
    slotlabel("Controls");

    ami_buttonsizg(stdout, (char*)"Press me", &w, &h);
    slot(w+PAD*2, h+PAD, &x, &y);
    if (!measuring) {

        ami_buttong(stdout, x, y, x+w+PAD*2, y+h+PAD, (char*)"Press me",
                    wibutton);
        made[wibutton] = TRUE;

    }

    /* the same button, turned off: it draws greyed and sends nothing */
    ami_buttonsizg(stdout, (char*)"Disabled", &w, &h);
    slot(w+PAD*2, h+PAD, &x, &y);
    if (!measuring) {

        ami_buttong(stdout, x, y, x+w+PAD*2, y+h+PAD, (char*)"Disabled",
                    wibutdis);
        made[wibutdis] = TRUE;
        ami_enablewidget(stdout, wibutdis, FALSE);

    }

    ami_checkboxsizg(stdout, (char*)"Checkbox", &w, &h);
    slot(w, h, &x, &y);
    if (!measuring)
        { ami_checkboxg(stdout, x, y, x+w, y+h, (char*)"Checkbox", wicheck);
          made[wicheck] = TRUE; }

    /* turned off and set: a disabled widget still shows its state */
    ami_checkboxsizg(stdout, (char*)"Disabled", &w, &h);
    slot(w, h, &x, &y);
    if (!measuring) {

        ami_checkboxg(stdout, x, y, x+w, y+h, (char*)"Disabled", wichkdis);
        made[wichkdis] = TRUE;
        ami_selectwidget(stdout, wichkdis, TRUE);
        ami_enablewidget(stdout, wichkdis, FALSE);

    }

    ami_radiobuttonsizg(stdout, (char*)"Radio one", &w, &h);
    slot(w, h, &x, &y);
    if (!measuring)
        { ami_radiobuttong(stdout, x, y, x+w, y+h, (char*)"Radio one",
                           wiradio1); made[wiradio1] = TRUE; }
    slot(w, h, &x, &y);
    if (!measuring) {

        ami_radiobuttong(stdout, x, y, x+w, y+h, (char*)"Radio two",
                         wiradio2);
        made[wiradio2] = TRUE;
        ami_selectwidget(stdout, wiradio1, TRUE); /* one of a pair starts set */

    }

    slot(w, h, &x, &y);
    if (!measuring) {

        ami_radiobuttong(stdout, x, y, x+w, y+h, (char*)"Disabled", wiraddis);
        made[wiraddis] = TRUE;
        ami_enablewidget(stdout, wiraddis, FALSE);

    }

    slotlabel("Edit box");
    ami_editboxsizg(stdout, (char*)"Type here", &w, &h);
    slot(w, h, &x, &y);
    if (!measuring) {

        ami_editboxg(stdout, x, y, x+colwid(), y+h, wiedit);
        made[wiedit] = TRUE;
        ami_putwidgettext(stdout, wiedit, (char*)"Type here");

    }

    slotlabel("Number select");
    ami_numselboxsizg(stdout, 1, 10, &w, &h);
    slot(w, h, &x, &y);
    if (!measuring)
        { ami_numselboxg(stdout, x, y, x+w, y+h, 1, 10, winumsel);
          made[winumsel] = TRUE; }

    /* -------------------------------------------------- column 2: lists */
    colbegin(1);
    slotlabel("Lists");
    lp = mklist((char*)"Red", (char*)"Green", (char*)"Blue");

    slotlabel("Drop box");
    ami_dropboxsizg(stdout, lp, &cw, &ch, &ow, &oh);
    slot(cw, ch, &x, &y);
    if (!measuring)
        { ami_dropboxg(stdout, x, y, x+cw, y+ch, lp, widrop);
          made[widrop] = TRUE; }

    slotlabel("Drop edit box");
    ami_dropeditboxsizg(stdout, lp, &cw, &ch, &ow, &oh);
    slot(cw, ch, &x, &y);
    if (!measuring)
        { ami_dropeditboxg(stdout, x, y, x+cw, y+ch, lp, widropedit);
          made[widropedit] = TRUE; }

    slotlabel("List box");
    ami_listboxsizg(stdout, lp, &w, &h);
    slot(w, h, &x, &y);
    if (!measuring)
        { ami_listboxg(stdout, x, y, x+w, y+h, lp, wilist);
          made[wilist] = TRUE; }

    /* a tab bar is a group box with tabs: it has a client area of its own.
       The sizing call is given the tabs, so what comes back holds them. */
    slotlabel("Tab bar");
    lp = mklist((char*)"One", (char*)"Two", (char*)"Three");
    ami_tabbarsizg(stdout, lp, ami_totop, ami_chrsizy(stdout)*8,
                   ami_chrsizy(stdout)*4, &w, &h, &ox, &oy);
    slot(w, h, &x, &y);
    if (!measuring)
        { ami_tabbarg(stdout, x, y, x+w, y+h, lp, ami_totop, witabbar);
          made[witabbar] = TRUE; }

    /* ------------------------------------- column 3: the bars and sliders */
    colbegin(2);
    slotlabel("Bars and sliders");

    ami_scrollhorizsizg(stdout, &w, &h);
    slot(ami_chrsizy(stdout)*8, h, &x, &y); /* a bar is as long as it is given */
    if (!measuring) {

        ami_scrollhorizg(stdout, x, y, x+colwid(), y+h, wiscrollh);
        made[wiscrollh] = TRUE;
        ami_scrollsiz(stdout, wiscrollh, LONG_MAX/4);

    }

    ami_slidehorizsizg(stdout, &w, &h);
    slot(ami_chrsizy(stdout)*8, h, &x, &y);
    if (!measuring)
        { ami_slidehorizg(stdout, x, y, x+colwid(), y+h, 10, wislideh);
          made[wislideh] = TRUE; }

    /* the vertical pair stand side by side, as tall as they are given */
    {

        ami_long vh = ami_chrsizy(stdout)*8; /* how tall to make them */
        ami_long sw, sh;

        ami_scrollvertsizg(stdout, &sw, &sh);
        ami_slidevertsizg(stdout, &w, &h);
        slot(sw+GAP*4+w, vh, &x, &y);
        if (!measuring) {

            ami_scrollvertg(stdout, x, y, x+sw, y+vh, wiscrollv);
            made[wiscrollv] = TRUE;
            ami_scrollsiz(stdout, wiscrollv, LONG_MAX/4);
            ami_slidevertg(stdout, x+sw+GAP*4, y, x+sw+GAP*4+w, y+vh, 10,
                           wislidev);
            made[wislidev] = TRUE;

        }

    }

    /* ------------------------------------------ column 4: the components */
    colbegin(3);
    slotlabel("Components");

    /* a background is a coloured rectangle that draws itself. Nothing is
       written on it here: a widget is a window over its rectangle, and
       anything the parent draws there is behind it. */
    slotlabel("Background");
    slot(ami_chrsizy(stdout)*8, ami_chrsizy(stdout)*3, &x, &y);
    if (!measuring)
        { ami_backgroundg(stdout, x, y, x+colwid(), y+ami_chrsizy(stdout)*3,
                          wiback); made[wiback] = TRUE; }

    /* a group box is a background with a name on it */
    ami_groupsizg(stdout, (char*)"Group box", ami_chrsizy(stdout)*8,
                  ami_chrsizy(stdout)*3, &w, &h, &ox, &oy);
    slot(w, h, &x, &y);
    if (!measuring)
        { ami_groupg(stdout, x, y, x+w, y+h, (char*)"Group box", wigroup);
          made[wigroup] = TRUE; }

    slotlabel("Progress bar");
    ami_progbarsizg(stdout, &w, &h);
    slot(w, h, &x, &y);
    if (!measuring) {

        ami_progbarg(stdout, x, y, x+colwid(), y+h, wiprog);
        made[wiprog] = TRUE;
        ami_progbarpos(stdout, wiprog, progpos);

    }

    /* --------------------------------------- the dialogs, along the foot */
    ami_buttonsizg(stdout, (char*)"Replace", &w, &h);
    rowh = h+PAD;
    rowy = colbot+GAP;
    roww = GAP+ami_strsiz(stdout, (char*)"Dialogs:")+GAP;
    for (i = 0; i < NDLG; i++) {

        ami_buttonsizg(stdout, dlg[i].face, &w, &h);
        if (!measuring) {

            ami_buttong(stdout, roww, rowy, roww+w+PAD*2, rowy+rowh,
                        dlg[i].face, dlg[i].id);
            made[dlg[i].id] = TRUE;

        }
        roww += w+PAD*2+GAP;

    }
    if (!measuring) label(GAP, rowy+ami_chrsizy(stdout)/2, "Dialogs:");

    /* what the page came to */
    needh = rowy+rowh+GAP;
    needw = GAP;
    for (i = 0; i < COLS; i++) needw += colnat[i]+GAP*2;
    if (roww+GAP > needw) needw = roww+GAP;

}

/* Measure the page, then place it. The window is asked for the size the
   widgets came to, so nothing is cut off at the foot however the fonts fall. */
static void layout(void)

{

    ami_long i;

    for (i = 0; i < COLS; i++) { colnat[i] = 0; colx[i] = 0; }
    measuring = TRUE;
    place();                       /* creates nothing: adds up */
    colx[0] = GAP;                 /* now the columns can be placed */
    for (i = 1; i < COLS; i++) colx[i] = colx[i-1]+colnat[i-1]+GAP*2;
    /* needw by needh is the client the page wants. setsizg sets the whole
       window, frame and all, so winclientg turns the one into the other:
       ask for the window that contains this client, with the frame, the
       size bars and the system bar this window has. */
    {

        ami_long ww, wh;

        ami_winclientg(stdout, needw, needh, &ww, &wh,
                       BIT(ami_wmframe)|BIT(ami_wmsize)|BIT(ami_wmsysbar));
        ami_setsizg(stdout, ww, wh);

    }
    measuring = FALSE;
    place();
    lastw = ami_maxxg(stdout); lasth = ami_maxyg(stdout);

}

/* take the page down, so it can be laid out again at the new size */
static void unplace(void)

{

    ami_long i;

    for (i = 1; i < wilast; i++) if (made[i]) {

        ami_killwidget(stdout, i);
        made[i] = FALSE;

    }

}

/*******************************************************************************

The dialogs

Each writes back what the user chose, so the flow through model can be seen:
the value goes in, the user may change it, and it comes back changed or as it
was.

*******************************************************************************/

static void dodialog(ami_long id)

{

    char           s[200], r[200];
    ami_long       cr, cg, cb, br, bg, bb, fc, fs;
    ami_qfnopts    fnopt;
    ami_qfropts    fropt;
    ami_qfteffects eff;

    switch (id) {

        case widalert:
            ami_alert((char*)"Alert", (char*)"This is what an alert looks like.");
            status("alert: closed");
            break;

        case widcolor:
            cr = INT_MAX/2; cg = 0; cb = INT_MAX/2;
            ami_querycolor(&cr, &cg, &cb);
            status("color: red %lld green %lld blue %lld", AMI_LONG_CAST(cr), AMI_LONG_CAST(cg), AMI_LONG_CAST(cb));
            break;

        case widopen:
            strcpy(s, "myfile.txt");
            ami_queryopen(s, sizeof(s));
            status("open: %s", *s? s: "(cancelled)");
            break;

        case widsave:
            strcpy(s, "myfile.txt");
            ami_querysave(s, sizeof(s));
            status("save: %s", *s? s: "(cancelled)");
            break;

        case widfind:
            strcpy(s, "find me");
            fnopt = 0;
            ami_queryfind(s, sizeof(s), &fnopt);
            status("find: %s, options %lld", *s? s: "(cancelled)", AMI_LONG_CAST((ami_long)fnopt));
            break;

        case widfindrep:
            strcpy(s, "find me");
            strcpy(r, "replace with me");
            fropt = 0;
            ami_queryfindrep(s, sizeof(s), r, sizeof(r), &fropt);
            status("replace: %s -> %s, options %lld", *s? s: "(cancelled)", r,
                   AMI_LONG_CAST((ami_long)fropt));
            break;

        case widfont:
            fc = 1; fs = ami_chrsizy(stdout);
            cr = 0; cg = 0; cb = 0; br = INT_MAX; bg = INT_MAX; bb = INT_MAX;
            eff = 0;
            ami_queryfont(stdout, &fc, &fs, &cr, &cg, &cb, &br, &bg, &bb, &eff);
            status("font: code %lld size %lld effects %lld", AMI_LONG_CAST(fc), AMI_LONG_CAST(fs), AMI_LONG_CAST((ami_long)eff));
            break;

    }

}

int main(void)

{

    ami_evtrec er;

    ami_title(stdout, "Ami widget busy box");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE); /* the page is placed, not scrolled */
    /* The labels are the program's own text, and the window's default is the
       terminal font. The widgets draw their faces in the sign font, so the
       page reads as one thing only if its labels are in the same. */
    ami_font(stdout, AMI_FONT_SIGN);
    progpos = 0;
    scrollv = scrollh = 0;
    layout(); /* measures the page, sizes the window to it, then places */
    status("every widget Ami has. Work one, or press a dialog button.");
    ami_timer(stdout, PROGTIM, PROGMS, TRUE);

    do {

        ami_event(stdin, &er);
        switch (er.etype) {

            /* ------------------------------------------------- controls */
            case ami_etbutton:
                if (er.butid >= widalert) dodialog(er.butid);
                else status("button %lld pressed", AMI_LONG_CAST(er.butid));
                break;

            case ami_etchkbox:
                /* the program keeps the state: the widget only reports the
                   click. Flip it and say so. */
                { static ami_long ck = FALSE;
                  ck = !ck;
                  ami_selectwidget(stdout, wicheck, ck);
                  status("checkbox %s", ck? "checked": "unchecked"); }
                break;

            case ami_etradbut:
                /* a radio pair is made mutually exclusive by the program */
                ami_selectwidget(stdout, wiradio1, er.radbid == wiradio1);
                ami_selectwidget(stdout, wiradio2, er.radbid == wiradio2);
                status("radio button %lld chosen",
                       AMI_LONG_CAST(er.radbid == wiradio1? 1L: 2L));
                break;

            case ami_etedtbox:
                { char s[200];
                  ami_getwidgettext(stdout, wiedit, s, sizeof(s));
                  status("edit box says: %s", s); }
                break;

            case ami_etnumbox:
                status("number select box: %lld", AMI_LONG_CAST(er.numbsl));
                break;

            case ami_etlstbox:
                status("list box: item %lld", AMI_LONG_CAST(er.lstbsl));
                break;

            case ami_etdrpbox:
                status("drop box: item %lld", AMI_LONG_CAST(er.drpbsl));
                break;

            case ami_etdrebox:
                { char s[200];
                  ami_getwidgettext(stdout, widropedit, s, sizeof(s));
                  status("drop edit box says: %s", s); }
                break;

            case ami_ettabbar:
                status("tab bar: tab %lld", AMI_LONG_CAST(er.tabsel));
                break;

            /* --------------------------------------- scroll bars: we move them */
            case ami_etsclpos:
                if (er.sclpid == wiscrollv) scrollv = er.sclpos;
                else scrollh = er.sclpos;
                ami_scrollpos(stdout, er.sclpid, er.sclpos);
                status("scroll bar %s dragged to %lld%%",
                       er.sclpid == wiscrollv? "vertical": "horizontal",
                       AMI_LONG_CAST(er.sclpos/(LONG_MAX/100)));
                break;

            case ami_etsclull: case ami_etsclulp:
                { ami_long id = er.etype == ami_etsclull? er.sclulid: er.sclupid;
                  ami_long* p = id == wiscrollv? &scrollv: &scrollh;
                  ami_long step = er.etype == ami_etsclull? LONG_MAX/50:
                                                        LONG_MAX/8;
                  *p = *p > step? *p-step: 0;
                  ami_scrollpos(stdout, id, *p);
                  status("scroll %s up/left, now %lld%%",
                         er.etype == ami_etsclull? "line": "page",
                         AMI_LONG_CAST(*p/(LONG_MAX/100))); }
                break;

            case ami_etscldrl: case ami_etscldrp:
                { ami_long id = er.etype == ami_etscldrl? er.scldrid: er.scldpid;
                  ami_long* p = id == wiscrollv? &scrollv: &scrollh;
                  ami_long step = er.etype == ami_etscldrl? LONG_MAX/50:
                                                        LONG_MAX/8;
                  *p = *p < LONG_MAX-step? *p+step: LONG_MAX;
                  ami_scrollpos(stdout, id, *p);
                  status("scroll %s down/right, now %lld%%",
                         er.etype == ami_etscldrl? "line": "page",
                         AMI_LONG_CAST(*p/(LONG_MAX/100))); }
                break;

            /* ------------------------- sliders: they have moved themselves */
            case ami_etsldpos:
                status("slider %s at %lld%%",
                       er.sldpid == wislidev? "vertical": "horizontal",
                       AMI_LONG_CAST(er.sldpos/(LONG_MAX/100)));
                break;

            /* ------------------------------ the progress bar walks itself */
            case ami_ettim:
                if (er.timnum == PROGTIM) {

                    progpos += LONG_MAX/50;
                    if (progpos < 0 || progpos > LONG_MAX-LONG_MAX/50)
                        progpos = 0;
                    ami_progbarpos(stdout, wiprog, progpos);

                }
                break;

            case ami_etresize:
                /* The layout is a function of the window size: take it down
                   and lay it out again. A resize that changes nothing --
                   the one that follows setting the size at the start is
                   one -- is left alone, so it cannot wipe the page or the
                   message on the top line. */
                if (ami_maxxg(stdout) == lastw && ami_maxyg(stdout) == lasth)
                    break;
                unplace();
                putchar('\f');
                lastw = ami_maxxg(stdout); lasth = ami_maxyg(stdout);
                measuring = FALSE;
                place(); /* the columns keep the widths they measured to */
                status("resized: the page is laid out again");
                break;

            default: ;

        }

    } while (er.etype != ami_etterm);
    ami_killtimer(stdout, PROGTIM);
    unplace();

    return (0);

}
