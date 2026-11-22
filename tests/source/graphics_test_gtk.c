/*******************************************************************************
*                                                                              *
* GRAPHICS TEST, GTK EDITION                                                   *
*                                                                              *
* The benchmark section of graphics_test.c, reconstructed on GTK4 the way a    *
* GTK application would build it: Cairo rendering into a backing image         *
* surface, presented through a GtkDrawingArea. That is the same architecture   *
* as the Ami Wayland backend (rasterize to a CPU buffer, hand the compositor   *
* whole frames), so the per-figure numbers compare rasterizer against          *
* rasterizer, not architecture against architecture.                           *
*                                                                              *
* Instructive differences from the Ami original, beyond the numbers:           *
*                                                                              *
* - No immediate mode. GTK draws only inside a draw callback; a program        *
*   cannot simply emit a primitive at a point in its logic. The benchmarks     *
*   therefore render to the offscreen surface and run from idle callbacks,     *
*   with the draw handler only blitting the surface. Ami programs draw         *
*   where they compute; GTK programs restructure around the toolkit's loop.    *
*                                                                              *
* - No bitwise color mix modes. Cairo's compositing is Porter-Duff; Ami's      *
*   mdxor/mdand/mdor have no expression here (the reason the Wayland           *
*   backend carries its own rasterizer). The "background invisible text"      *
*   row is text without a background fill, which is the nearest meaning.       *
*                                                                              *
* - Antialiasing is disabled for shape parity (Ami's rasterizer is exact-      *
*   pixel); text keeps Pango/FreeType rendering, as Ami keeps its own          *
*   FreeType path.                                                             *
*                                                                              *
* Usage: graphics_test_gtk [width height]                                      *
*                                                                              *
* Runs the twenty benchmarks against a width x height backing surface          *
* (default 1120 x 775, the Ami default window), showing progress in the        *
* window, then prints the benchmark table on standard output in the same       *
* format as graphics_test.                                                     *
*                                                                              *
*******************************************************************************/

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* one second in microseconds, the local timebase */
#define USEC 1000000ll

/* benchmark identities, in table order */
typedef enum {

    bnline1, bnline10, bnrect1, bnrect10, bnrrect1, bnrrect10,
    bnfrect, bnfrrect, bnellipse1, bnellipse10, bnfellipse,
    bnarc1, bnarc10, bnfarc, bnfchord, bnftriangle,
    bntext, bntextinvis, bnpict, bnpictns,

    bncount

} bench;

static const char* benchname[bncount] = {

    "line width 1",               "line width 10",
    "rectangle width 1",          "rectangle width 10",
    "rounded rectangle width 1",  "rounded rectangle width 10",
    "filled rectangle",           "filled rounded rectangle",
    "ellipse width 1",            "ellipse width 10",
    "filled ellipse",
    "arc width 1",                "arc width 10",
    "filled arc",                 "filled chord",
    "filled triangle",
    "text",                       "background invisible text",
    "Picture draw",               "No scaling picture draw"

};

/* program state: the backing surface and the benchmark walk */
typedef struct {

    cairo_surface_t* surf;    /* backing store, as the Ami backend keeps */
    cairo_t*         cr;      /* context kept open on it */
    int              w, h;    /* surface size */
    PangoLayout*     lay;     /* text layout, made once */
    GdkPixbuf*       pict;    /* test picture */
    GtkWidget*       area;    /* the on-screen view of the surface */
    bench            at;      /* next benchmark to run */
    long             iter[bncount]; /* iterations run */
    double           time[bncount]; /* seconds taken */

} state;

/* random in range, as graphics_test has it */
static int randr(int lo, int hi)

{

    return (lo+rand()%(hi-lo+1));

}

/* the six benchmark colors, matching Ami red..magenta */
static void setcolor(cairo_t* cr, int i)

{

    static const double tab[6][3] = {

        { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 },
        { 0, 1, 1 }, { 1, 1, 0 }, { 1, 0, 1 }

    };
    const double* c = tab[i%6];

    cairo_set_source_rgb(cr, c[0], c[1], c[2]);

}

/*******************************************************************************

Primitive renderers

One function per benchmark row, each drawing a single random figure, the
direct counterparts of the ami_ calls in graphics_test's speed loops.

*******************************************************************************/

static void dline(state* st, int lw)

{

    cairo_t* cr = st->cr;

    setcolor(cr, rand());
    cairo_set_line_width(cr, lw);
    cairo_move_to(cr, randr(1, st->w), randr(1, st->h));
    cairo_line_to(cr, randr(1, st->w), randr(1, st->h));
    cairo_stroke(cr);

}

/* place a random rectangle path; fill or stroke is the caller's */
static void rectpath(state* st)

{

    int x1 = randr(1, st->w), y1 = randr(1, st->h);
    int x2 = randr(1, st->w), y2 = randr(1, st->h);

    cairo_rectangle(st->cr, MIN(x1, x2), MIN(y1, y2),
                    abs(x2-x1)+1, abs(y2-y1)+1);

}

static void drect(state* st, int lw)

{

    setcolor(st->cr, rand());
    cairo_set_line_width(st->cr, lw);
    rectpath(st);
    cairo_stroke(st->cr);

}

static void dfrect(state* st)

{

    setcolor(st->cr, rand());
    rectpath(st);
    cairo_fill(st->cr);

}

/* rounded rectangle path: Cairo has no primitive; every GTK program
   carries this quarter-arc walk (GTK itself does, in its CSS renderer) */
static void rrectpath(state* st)

{

    cairo_t* cr = st->cr;
    int x1 = randr(1, st->w), y1 = randr(1, st->h);
    int x2 = randr(1, st->w), y2 = randr(1, st->h);
    double x = MIN(x1, x2), y = MIN(y1, y2);
    double w = abs(x2-x1)+1, h = abs(y2-y1)+1;
    double r = 20; /* the graphics_test benchmark corner */

    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -G_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r, 0,       G_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r, G_PI/2,  G_PI);
    cairo_arc(cr, x+r,   y+r,   r, G_PI,    3*G_PI/2);
    cairo_close_path(cr);

}

static void drrect(state* st, int lw)

{

    setcolor(st->cr, rand());
    cairo_set_line_width(st->cr, lw);
    rrectpath(st);
    cairo_stroke(st->cr);

}

static void dfrrect(state* st)

{

    setcolor(st->cr, rand());
    rrectpath(st);
    cairo_fill(st->cr);

}

/* ellipse in a random bounding box: unit circle under a scale */
static void ellipsepath(state* st)

{

    cairo_t* cr = st->cr;
    int x1 = randr(1, st->w), y1 = randr(1, st->h);
    int x2 = randr(1, st->w), y2 = randr(1, st->h);
    double w = abs(x2-x1)+1, h = abs(y2-y1)+1;

    cairo_save(cr);
    cairo_translate(cr, MIN(x1, x2)+w/2, MIN(y1, y2)+h/2);
    cairo_scale(cr, w/2, h/2);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 0, 0, 1, 0, 2*G_PI);
    cairo_restore(cr); /* line width stays in device space */

}

static void dellipse(state* st, int lw)

{

    setcolor(st->cr, rand());
    cairo_set_line_width(st->cr, lw);
    ellipsepath(st);
    cairo_stroke(st->cr);

}

static void dfellipse(state* st)

{

    setcolor(st->cr, rand());
    ellipsepath(st);
    cairo_fill(st->cr);

}

/* arcs: random box, random start and extent, as ami_arc takes them */
static void arcgeom(state* st, double* cx, double* cy, double* rx,
                    double* ry, double* a1, double* a2)

{

    int x1 = randr(1, st->w), y1 = randr(1, st->h);
    int x2 = randr(1, st->w), y2 = randr(1, st->h);

    *rx = (abs(x2-x1)+1)/2.0; *ry = (abs(y2-y1)+1)/2.0;
    *cx = MIN(x1, x2)+*rx; *cy = MIN(y1, y2)+*ry;
    *a1 = randr(0, 359)*G_PI/180;
    *a2 = *a1+randr(1, 359)*G_PI/180;

}

static void darcpath(state* st, int close, int pie)

{

    cairo_t* cr = st->cr;
    double cx, cy, rx, ry, a1, a2;

    arcgeom(st, &cx, &cy, &rx, &ry, &a1, &a2);
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx, ry);
    cairo_new_sub_path(cr);
    if (pie) cairo_move_to(cr, 0, 0);
    cairo_arc(cr, 0, 0, 1, a1, a2);
    if (close) cairo_close_path(cr);
    cairo_restore(cr);

}

static void darc(state* st, int lw)

{

    setcolor(st->cr, rand());
    cairo_set_line_width(st->cr, lw);
    darcpath(st, FALSE, FALSE);
    cairo_stroke(st->cr);

}

static void dfarc(state* st)

{

    /* Ami's filled arc is the pie section */
    setcolor(st->cr, rand());
    darcpath(st, TRUE, TRUE);
    cairo_fill(st->cr);

}

static void dfchord(state* st)

{

    /* the chord closes rim to rim, no center point */
    setcolor(st->cr, rand());
    darcpath(st, TRUE, FALSE);
    cairo_fill(st->cr);

}

static void dftriangle(state* st)

{

    cairo_t* cr = st->cr;

    setcolor(cr, rand());
    cairo_move_to(cr, randr(1, st->w), randr(1, st->h));
    cairo_line_to(cr, randr(1, st->w), randr(1, st->h));
    cairo_line_to(cr, randr(1, st->w), randr(1, st->h));
    cairo_close_path(cr);
    cairo_fill(cr);

}

/* text through Pango, as GTK text always goes. The plain row fills the
   background rectangle first, as Ami's overwrite mode paints; the
   "invisible background" row lays glyphs alone */
static void dtext(state* st, int bgfill)

{

    cairo_t* cr = st->cr;
    int x = randr(1, st->w), y = randr(1, st->h);
    int tw, th;

    if (bgfill) {

        pango_layout_get_pixel_size(st->lay, &tw, &th);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_rectangle(cr, x, y, tw, th);
        cairo_fill(cr);

    }
    setcolor(cr, rand());
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, st->lay);

}

/* picture draws: scaled to a random box, and at natural size */
static void dpict(state* st, int scale)

{

    cairo_t* cr = st->cr;
    double pw = gdk_pixbuf_get_width(st->pict);
    double ph = gdk_pixbuf_get_height(st->pict);

    cairo_save(cr);
    if (scale) {

        int x1 = randr(1, st->w), y1 = randr(1, st->h);
        int x2 = randr(1, st->w), y2 = randr(1, st->h);

        cairo_translate(cr, MIN(x1, x2), MIN(y1, y2));
        cairo_scale(cr, (abs(x2-x1)+1)/pw, (abs(y2-y1)+1)/ph);
        gdk_cairo_set_source_pixbuf(cr, st->pict, 0, 0);

    } else
        gdk_cairo_set_source_pixbuf(cr, st->pict,
                                    randr(1, st->w), randr(1, st->h));
    cairo_paint(cr);
    cairo_restore(cr);

}

/*******************************************************************************

Benchmark driver

The calibration is graphics_test's: double the count until a pass takes a
measurable second, scale to a five second run, run it for the record.

*******************************************************************************/

/* one figure of the given benchmark */
static void figure(state* st, bench bn)

{

    switch (bn) {

        case bnline1:      dline(st, 1); break;
        case bnline10:     dline(st, 10); break;
        case bnrect1:      drect(st, 1); break;
        case bnrect10:     drect(st, 10); break;
        case bnrrect1:     drrect(st, 1); break;
        case bnrrect10:    drrect(st, 10); break;
        case bnfrect:      dfrect(st); break;
        case bnfrrect:     dfrrect(st); break;
        case bnellipse1:   dellipse(st, 1); break;
        case bnellipse10:  dellipse(st, 10); break;
        case bnfellipse:   dfellipse(st); break;
        case bnarc1:       darc(st, 1); break;
        case bnarc10:      darc(st, 10); break;
        case bnfarc:       dfarc(st); break;
        case bnfchord:     dfchord(st); break;
        case bnftriangle:  dftriangle(st); break;
        case bntext:       dtext(st, TRUE); break;
        case bntextinvis:  dtext(st, FALSE); break;
        case bnpict:       dpict(st, TRUE); break;
        case bnpictns:     dpict(st, FALSE); break;
        default: break;

    }

}

/* timed pass of n figures; cairo surfaces buffer nothing, but flush
   anyway so the account is closed, as XSync would close it */
static double timedpass(state* st, bench bn, long n)

{

    gint64 t = g_get_monotonic_time();
    long   i;

    for (i = 0; i < n; i++) figure(st, bn);
    cairo_surface_flush(st->surf);
    return ((g_get_monotonic_time()-t)/(double)USEC);

}

/* run one benchmark to the record */
static void runbench(state* st, bench bn)

{

    long   i = 10;
    double et;

    do {

        et = timedpass(st, bn, i);
        i *= 2;

    } while (et < 1.0);
    i /= 2;
    i = (long)(5.0/(et/i));
    if (i < 1) i = 1;
    st->time[bn] = timedpass(st, bn, i);
    st->iter[bn] = i;

}

/* the idle walk: one benchmark per callback, the window repainting
   between them -- the closest GTK comes to drawing while computing */
static gboolean stepbench(gpointer ud)

{

    state* st = ud;

    if (st->at >= bncount) {

        printf("\nBenchmark table\n\n");
        printf("Type                        Seconds   Per fig\n");
        printf("--------------------------------------------------\n");
        for (bench b = 0; b < bncount; b++)
            printf("%-27s%8.2f%12.6f\n", benchname[b], st->time[b],
                   st->iter[b]? st->time[b]/st->iter[b]: 0.0);
        fflush(stdout);
        return (G_SOURCE_REMOVE); /* window holds until closed */

    }
    runbench(st, st->at);
    st->at++;
    gtk_widget_queue_draw(st->area);
    return (G_SOURCE_CONTINUE);

}

/*******************************************************************************

GTK scaffolding

*******************************************************************************/

/* the draw handler blits the backing surface, nothing more: rendering
   happened elsewhere, exactly as the Ami backend presents its buffer */
static void draw(GtkDrawingArea* a, cairo_t* cr, int w, int h, gpointer ud)

{

    state* st = ud;

    cairo_set_source_surface(cr, st->surf, 0, 0);
    cairo_paint(cr);

}

static void activate(GtkApplication* app, gpointer ud)

{

    state*     st = ud;
    GtkWidget* win;
    cairo_t*   cr;

    /* the backing store and its persistent context */
    st->surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, st->w, st->h);
    cr = cairo_create(st->surf);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE); /* Ami parity */
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    st->cr = cr;

    /* text fixture: the layout is built once, holding the benchmark
       string, as Ami's glyph cache holds its faces */
    st->lay = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(st->lay,
        pango_font_description_from_string("Monospace 11"));
    pango_layout_set_text(st->lay, "Test text", -1);

    st->pict = gdk_pixbuf_new_from_file("tests/mypic.bmp", NULL);
    if (!st->pict) {

        fprintf(stderr, "cannot load tests/mypic.bmp (run from repo root)\n");
        exit(1);

    }

    win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "graphics_test_gtk");
    gtk_window_set_default_size(GTK_WINDOW(win), st->w, st->h);
    st->area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(st->area), draw, st, NULL);
    gtk_window_set_child(GTK_WINDOW(win), st->area);
    gtk_window_present(GTK_WINDOW(win));

    g_idle_add(stepbench, st);

}

int main(int argc, char* argv[])

{

    static state    st; /* zeroed */
    GtkApplication* app;
    int             rc;

    st.w = 1120; st.h = 775; /* the Ami default window */
    if (argc >= 3) {

        st.w = atoi(argv[1]); st.h = atoi(argv[2]);
        if (st.w < 100 || st.h < 100) {

            fprintf(stderr, "Usage: graphics_test_gtk [width height]\n");
            return (1);

        }
        /* the size arguments are ours, not GTK's */
        argc = 1;

    }
    app = gtk_application_new("org.petit_ami.graphics_test_gtk",
                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &st);
    rc = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return (rc);

}
