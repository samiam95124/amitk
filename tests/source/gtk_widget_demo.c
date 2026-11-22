/*******************************************************************************
*                                                                              *
*                     WIDGET BUSY BOX: THE GTK EDITION                         *
*                                                                              *
*                    Copyright (C) 2026 Scott A. Franco                        *
*                                                                              *
* The same page as tests/source/widgets_demo.c, built on GTK instead of on     *
* Ami, so that the two can be set beside each other: the same widgets, the     *
* same arrangement, the same dialogs, drawn by the toolkit the desktop ships.  *
*                                                                              *
* It is a comparison piece, not a port. Where GTK names a widget differently   *
* the nearest one is used and the Ami name is kept on the label, so that the   *
* pages read the same down the columns:                                        *
*                                                                              *
*     Ami                    GTK                                               *
*     button                 GtkButton                                         *
*     checkbox               GtkCheckButton                                    *
*     radio button           GtkCheckButton in a group                         *
*     edit box               GtkEntry                                          *
*     number select box      GtkSpinButton                                     *
*     drop box               GtkDropDown                                       *
*     drop edit box          GtkComboBoxText, with an entry                    *
*     list box               GtkListBox                                        *
*     tab bar                GtkNotebook                                       *
*     scroll bar             GtkScrollbar                                      *
*     slider                 GtkScale                                          *
*     group box              GtkFrame, with a label                            *
*     background             GtkFrame, without one                             *
*     progress bar           GtkProgressBar                                    *
*                                                                              *
* The dialogs are GTK's own: a message dialog for the alert, and the colour,   *
* file open, file save and font choosers. GTK has no find or find/replace      *
* dialog, so those two buttons say so rather than pretending.                  *
*                                                                              *
* Build:                                                                       *
*                                                                              *
*     make gtk_widget_demo                                                     *
*                                                                              *
*******************************************************************************/

#include <gtk/gtk.h>

#define GAP 12  /* space between widgets */

static GtkWidget* statlab;   /* the line that reports what the widgets say */
static GtkWidget* progbar;   /* stepped by a timer, as the Ami page does */
static GtkWidget* editbox;
static GtkWidget* dropedit;
static GtkWidget* toplevel;

/* say what happened, on the top line */
static void status(const char* fmt, ...)

{

    va_list ap;
    char    b[256];

    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    gtk_label_set_text(GTK_LABEL(statlab), b);

}

/* a label above a widget, left aligned as the Ami page has them */
static GtkWidget* label(const char* s)

{

    GtkWidget* l = gtk_label_new(s);

    gtk_widget_set_halign(l, GTK_ALIGN_START);

    return (l);

}

/* ------------------------------------------------------------ the controls */

static void on_button(GtkButton* b, gpointer u)
    { (void)b; (void)u; status("button pressed"); }

static void on_check(GtkCheckButton* c, gpointer u)
    { (void)u; status("checkbox %s",
        gtk_check_button_get_active(c)? "checked": "unchecked"); }

static void on_radio(GtkCheckButton* c, gpointer u)
    { if (gtk_check_button_get_active(c)) status("radio button %lld chosen",
        AMI_LONG_CAST((long)(intptr_t)u)); }

static void on_edit(GtkEntry* e, gpointer u)
    { (void)u; status("edit box says: %s",
        gtk_entry_buffer_get_text(gtk_entry_get_buffer(e))); }

static void on_spin(GtkSpinButton* s, gpointer u)
    { (void)u; status("number select box: %d", gtk_spin_button_get_value_as_int(s)); }

static void on_drop(GObject* d, GParamSpec* p, gpointer u)
    { (void)p; (void)u; status("drop box: item %u",
        gtk_drop_down_get_selected(GTK_DROP_DOWN(d))+1); }

static void on_dropedit(GtkComboBox* c, gpointer u)
    { (void)u; status("drop edit box: item %d", gtk_combo_box_get_active(c)+1); }

static void on_list(GtkListBox* l, GtkListBoxRow* r, gpointer u)
    { (void)l; (void)u; if (r) status("list box: item %d",
        gtk_list_box_row_get_index(r)+1); }

static void on_tab(GObject* n, GParamSpec* p, gpointer u)
    { (void)p; (void)u; status("tab bar: tab %d",
        gtk_notebook_get_current_page(GTK_NOTEBOOK(n))+1); }

static void on_scroll(GtkAdjustment* a, gpointer u)
    { status("scroll bar %s at %d%%", (const char*)u,
        (int)gtk_adjustment_get_value(a)); }

static void on_slide(GtkRange* r, gpointer u)
    { status("slider %s at %d%%", (const char*)u, (int)gtk_range_get_value(r)); }

/* the progress bar walks itself, as the Ami page's does */
static gboolean on_timer(gpointer u)

{

    double v = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progbar))+0.02;

    (void)u;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), v > 1.0? 0.0: v);

    return (TRUE);

}

/* -------------------------------------------------------------- the dialogs */

static void dlg_done(GObject* src, GAsyncResult* res, gpointer u)

{

    const char* what = (const char*)u;
    GError*     err = NULL;

    if (!g_strcmp0(what, "alert")) {

        gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(src), res, &err);
        status("alert: closed");

    } else if (!g_strcmp0(what, "color")) {

        GdkRGBA* c = gtk_color_dialog_choose_rgba_finish(GTK_COLOR_DIALOG(src),
                                                         res, &err);
        if (c) status("color: red %.0f green %.0f blue %.0f",
                      c->red*255, c->green*255, c->blue*255);
        else status("color: cancelled");

    } else if (!g_strcmp0(what, "font")) {

        PangoFontDescription* d =
            gtk_font_dialog_choose_font_finish(GTK_FONT_DIALOG(src), res, &err);
        if (d) status("font: %s", pango_font_description_get_family(d));
        else status("font: cancelled");

    } else { /* open and save both come back as a file */

        GFile* f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &err);

        if (!f) f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(src), res, NULL);
        if (f) { char* n = g_file_get_basename(f); status("%s: %s", what, n);
                 g_free(n); }
        else status("%s: cancelled", what);

    }
    if (err) g_error_free(err);

}

static void on_alert(GtkButton* b, gpointer u)

{

    GtkAlertDialog* d = gtk_alert_dialog_new("This is what an alert looks like.");

    (void)b; (void)u;
    gtk_alert_dialog_choose(d, GTK_WINDOW(toplevel), NULL, dlg_done, "alert");

}

static void on_color(GtkButton* b, gpointer u)

{

    GtkColorDialog* d = gtk_color_dialog_new();

    (void)b; (void)u;
    gtk_color_dialog_choose_rgba(d, GTK_WINDOW(toplevel), NULL, NULL, dlg_done,
                                 "color");

}

static void on_open(GtkButton* b, gpointer u)

{

    GtkFileDialog* d = gtk_file_dialog_new();

    (void)b; (void)u;
    gtk_file_dialog_open(d, GTK_WINDOW(toplevel), NULL, dlg_done, "open");

}

static void on_save(GtkButton* b, gpointer u)

{

    GtkFileDialog* d = gtk_file_dialog_new();

    (void)b; (void)u;
    gtk_file_dialog_set_initial_name(d, "myfile.txt");
    gtk_file_dialog_save(d, GTK_WINDOW(toplevel), NULL, dlg_done, "save");

}

static void on_font(GtkButton* b, gpointer u)

{

    GtkFontDialog* d = gtk_font_dialog_new();

    (void)b; (void)u;
    gtk_font_dialog_choose_font(d, GTK_WINDOW(toplevel), NULL, NULL, dlg_done,
                                "font");

}

/* GTK has no find or find/replace dialog: an application brings its own */
static void on_nodialog(GtkButton* b, gpointer u)
    { (void)b; status("%s: GTK has no such dialog; an application brings its own",
                      (const char*)u); }

/* ------------------------------------------------------------- the page */

static GtkWidget* column(const char* head)

{

    GtkWidget* c = gtk_box_new(GTK_ORIENTATION_VERTICAL, GAP);
    GtkWidget* h = label(head);

    gtk_widget_set_valign(c, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(c), h);

    return (c);

}

static void activate(GtkApplication* app, gpointer u)

{

    GtkWidget*  win, * page, * cols, * c1, * c2, * c3, * c4, * row, * w, * bars;
    GtkAdjustment* adj;
    const char* items[] = { "Red", "Green", "Blue", NULL };
    int         i;

    (void)u;
    win = gtk_application_window_new(app);
    toplevel = win;
    gtk_window_set_title(GTK_WINDOW(win), "GTK widget busy box");

    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, GAP);
    gtk_widget_set_margin_start(page, GAP); gtk_widget_set_margin_end(page, GAP);
    gtk_widget_set_margin_top(page, GAP); gtk_widget_set_margin_bottom(page, GAP);

    statlab = label("every widget GTK has, beside the Ami page. Work one, or "
                    "press a dialog button.");
    gtk_box_append(GTK_BOX(page), statlab);

    cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, GAP*2);
    gtk_box_append(GTK_BOX(page), cols);

    /* ------------------------------------------------ column 1: controls */
    c1 = column("Controls");
    w = gtk_button_new_with_label("Press me");
    g_signal_connect(w, "clicked", G_CALLBACK(on_button), NULL);
    gtk_box_append(GTK_BOX(c1), w);

    w = gtk_check_button_new_with_label("Checkbox");
    g_signal_connect(w, "toggled", G_CALLBACK(on_check), NULL);
    gtk_box_append(GTK_BOX(c1), w);

    {

        GtkWidget* r1 = gtk_check_button_new_with_label("Radio one");
        GtkWidget* r2 = gtk_check_button_new_with_label("Radio two");

        gtk_check_button_set_group(GTK_CHECK_BUTTON(r2), GTK_CHECK_BUTTON(r1));
        gtk_check_button_set_active(GTK_CHECK_BUTTON(r1), TRUE);
        g_signal_connect(r1, "toggled", G_CALLBACK(on_radio), (gpointer)1);
        g_signal_connect(r2, "toggled", G_CALLBACK(on_radio), (gpointer)2);
        gtk_box_append(GTK_BOX(c1), r1);
        gtk_box_append(GTK_BOX(c1), r2);

    }

    gtk_box_append(GTK_BOX(c1), label("Edit box"));
    editbox = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(editbox), "Type here");
    g_signal_connect(editbox, "activate", G_CALLBACK(on_edit), NULL);
    gtk_box_append(GTK_BOX(c1), editbox);

    gtk_box_append(GTK_BOX(c1), label("Number select"));
    w = gtk_spin_button_new_with_range(1, 10, 1);
    g_signal_connect(w, "value-changed", G_CALLBACK(on_spin), NULL);
    gtk_box_append(GTK_BOX(c1), w);

    /* --------------------------------------------------- column 2: lists */
    c2 = column("Lists");
    gtk_box_append(GTK_BOX(c2), label("Drop box"));
    w = gtk_drop_down_new_from_strings(items);
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_drop), NULL);
    gtk_box_append(GTK_BOX(c2), w);

    gtk_box_append(GTK_BOX(c2), label("Drop edit box"));
    dropedit = gtk_combo_box_text_new_with_entry();
    for (i = 0; items[i]; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dropedit), items[i]);
    g_signal_connect(dropedit, "changed", G_CALLBACK(on_dropedit), NULL);
    gtk_box_append(GTK_BOX(c2), dropedit);

    gtk_box_append(GTK_BOX(c2), label("List box"));
    {

        GtkWidget* lb = gtk_list_box_new();

        for (i = 0; items[i]; i++)
            gtk_list_box_append(GTK_LIST_BOX(lb), label(items[i]));
        g_signal_connect(lb, "row-selected", G_CALLBACK(on_list), NULL);
        gtk_box_append(GTK_BOX(c2), lb);

    }

    gtk_box_append(GTK_BOX(c2), label("Tab bar"));
    {

        GtkWidget* nb = gtk_notebook_new();
        const char* tabs[] = { "One", "Two", "Three" };

        for (i = 0; i < 3; i++) {

            GtkWidget* pg = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

            gtk_widget_set_size_request(pg, 160, 80);
            gtk_notebook_append_page(GTK_NOTEBOOK(nb), pg, label(tabs[i]));

        }
        g_signal_connect(nb, "notify::page", G_CALLBACK(on_tab), NULL);
        gtk_box_append(GTK_BOX(c2), nb);

    }

    /* ---------------------------------------- column 3: bars and sliders */
    c3 = column("Bars and sliders");
    adj = gtk_adjustment_new(0, 0, 110, 1, 10, 10);
    w = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, adj);
    gtk_widget_set_size_request(w, 200, -1);
    g_signal_connect(adj, "value-changed", G_CALLBACK(on_scroll),
                     (gpointer)"horizontal");
    gtk_box_append(GTK_BOX(c3), w);

    w = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(w), FALSE);
    g_signal_connect(w, "value-changed", G_CALLBACK(on_slide),
                     (gpointer)"horizontal");
    gtk_box_append(GTK_BOX(c3), w);

    bars = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, GAP*2);
    adj = gtk_adjustment_new(0, 0, 110, 1, 10, 10);
    w = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, adj);
    gtk_widget_set_size_request(w, -1, 160);
    g_signal_connect(adj, "value-changed", G_CALLBACK(on_scroll),
                     (gpointer)"vertical");
    gtk_box_append(GTK_BOX(bars), w);

    w = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(w), FALSE);
    gtk_widget_set_size_request(w, -1, 160);
    g_signal_connect(w, "value-changed", G_CALLBACK(on_slide),
                     (gpointer)"vertical");
    gtk_box_append(GTK_BOX(bars), w);
    gtk_box_append(GTK_BOX(c3), bars);

    /* ----------------------------------------- column 4: the components */
    c4 = column("Components");
    gtk_box_append(GTK_BOX(c4), label("Background"));
    w = gtk_frame_new(NULL);
    /* an Adwaita frame draws nothing of itself: the view class gives it the
       filled panel the Ami background component has */
    gtk_widget_add_css_class(w, "view");
    gtk_widget_add_css_class(w, "frame");
    gtk_widget_set_size_request(w, 200, 60);
    gtk_box_append(GTK_BOX(c4), w);

    w = gtk_frame_new("Group box");
    gtk_widget_add_css_class(w, "frame");
    {

        GtkWidget* in = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        gtk_widget_set_size_request(in, 200, 60);
        gtk_frame_set_child(GTK_FRAME(w), in);

    }
    gtk_box_append(GTK_BOX(c4), w);

    gtk_box_append(GTK_BOX(c4), label("Progress bar"));
    progbar = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar), 0.2);
    gtk_box_append(GTK_BOX(c4), progbar);

    gtk_box_append(GTK_BOX(cols), c1);
    gtk_box_append(GTK_BOX(cols), c2);
    gtk_box_append(GTK_BOX(cols), c3);
    gtk_box_append(GTK_BOX(cols), c4);

    /* -------------------------------------- the dialogs, along the foot */
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, GAP);
    gtk_box_append(GTK_BOX(row), label("Dialogs:"));
    {

        struct { const char* face; GCallback fn; const char* arg; } dlg[] = {

            { "Alert",   G_CALLBACK(on_alert),    NULL      },
            { "Color",   G_CALLBACK(on_color),    NULL      },
            { "Open",    G_CALLBACK(on_open),     NULL      },
            { "Save",    G_CALLBACK(on_save),     NULL      },
            { "Find",    G_CALLBACK(on_nodialog), "find"    },
            { "Replace", G_CALLBACK(on_nodialog), "replace" },
            { "Font",    G_CALLBACK(on_font),     NULL      }

        };

        for (i = 0; i < (int)(sizeof(dlg)/sizeof(dlg[0])); i++) {

            w = gtk_button_new_with_label(dlg[i].face);
            g_signal_connect(w, "clicked", dlg[i].fn, (gpointer)dlg[i].arg);
            gtk_box_append(GTK_BOX(row), w);

        }

    }
    gtk_box_append(GTK_BOX(page), row);

    /* GTK sizes the window from what the widgets ask for, which is the same
       thing the Ami page works out for itself in its measuring pass */
    gtk_window_set_child(GTK_WINDOW(win), page);
    g_timeout_add(100, on_timer, NULL);
    gtk_window_present(GTK_WINDOW(win));

}

int main(int argc, char* argv[])

{

    GtkApplication* app;
    int             r;

    app = gtk_application_new("org.amitk.gtkwidgetdemo", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    r = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return (r);

}
