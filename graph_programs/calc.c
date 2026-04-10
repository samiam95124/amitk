/*******************************************************************************
*                                                                              *
*                              DESK CALCULATOR                                 *
*                                                                              *
*                       COPYRIGHT (C) 2026 S. A. FRANCO                        *
*                                                                              *
* A graphical desk calculator with resizable window. Supports basic            *
* arithmetic (+, -, *, /), percentage, sign toggle, and clear functions.        *
* Uses the Petit-Ami widget system for buttons and display.                    *
*                                                                              *
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <localdefs.h>
#include <graphics.h>

/*******************************************************************************

Widget IDs

*******************************************************************************/

/* display */
#define ID_DISPLAY  1
#define ID_BACK     2

/* digit buttons */
#define ID_0        10
#define ID_1        11
#define ID_2        12
#define ID_3        13
#define ID_4        14
#define ID_5        15
#define ID_6        16
#define ID_7        17
#define ID_8        18
#define ID_9        19
#define ID_DOT      20

/* operator buttons */
#define ID_PLUS     30
#define ID_MINUS    31
#define ID_MULT     32
#define ID_DIV      33
#define ID_EQUALS   34

/* function buttons */
#define ID_CLEAR    40
#define ID_CE       41
#define ID_SIGN     42
#define ID_PERCENT  43
#define ID_BKSP     44

/* menu IDs */
#define MENU_EXIT   100
#define MENU_ABOUT  101

/* total widget count for layout */
#define NCOLS       4
#define NROWS       6  /* 1 display row + 5 button rows */

/*******************************************************************************

Calculator state

*******************************************************************************/

double accumulator;    /* running result */
double operand;        /* current displayed number */
int    op;             /* pending operator: 0=none, '+', '-', '*', '/' */
int    newentry;       /* next digit starts a new number */
int    hasdot;         /* decimal point entered */
int    dotplace;       /* decimal place counter */
char   display[64];    /* display string */

/*******************************************************************************

Menu helpers (from chess.c pattern)

*******************************************************************************/

static ami_menuptr newmenuitem(int onoff, int oneof, int bar, int id,
                               const char *face)
{

    ami_menuptr mp;

    mp = malloc(sizeof(ami_menurec));
    mp->next = NULL;
    mp->branch = NULL;
    mp->onoff = onoff;
    mp->oneof = oneof;
    mp->bar = bar;
    mp->id = id;
    mp->face = malloc(strlen(face) + 1);
    strcpy(mp->face, face);

    return mp;

}

static void appendmenu(ami_menuptr *list, ami_menuptr item)
{

    ami_menuptr p;

    if (*list == NULL) *list = item;
    else {

        p = *list;
        while (p->next) p = p->next;
        p->next = item;

    }

}

static void setup_menu(void)
{

    ami_menuptr menu = NULL;
    ami_menuptr file_menu, file_items;
    ami_menuptr help_menu, help_items;

    file_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "File");
    appendmenu(&menu, file_menu);
    file_items = NULL;
    appendmenu(&file_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_EXIT, "Exit"));
    file_menu->branch = file_items;

    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);
    help_items = NULL;
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Calculator"));
    help_menu->branch = help_items;

    ami_menu(stdout, menu);

}

/*******************************************************************************

Display formatting

*******************************************************************************/

static void format_display(double val)
{

    /* format number, strip trailing zeros */
    if (val == (long long)val && fabs(val) < 1e15)
        sprintf(display, "%.0f", val);
    else
        sprintf(display, "%.10g", val);

}

static void update_display(void)
{

    ami_putwidgettext(stdout, ID_DISPLAY, display);

}

/*******************************************************************************

Calculator logic

*******************************************************************************/

static void calc_clear(void)
{

    accumulator = 0;
    operand = 0;
    op = 0;
    newentry = TRUE;
    hasdot = FALSE;
    dotplace = 0;
    strcpy(display, "0");

}

static double do_op(double a, int oper, double b)
{

    switch (oper) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return b != 0 ? a / b : 0;
    }

    return b;

}

static void press_digit(int d)
{

    char ds[2];

    if (newentry) {

        strcpy(display, "");
        hasdot = FALSE;
        dotplace = 0;
        newentry = FALSE;

    }

    if (strlen(display) >= 15) return; /* limit display length */

    ds[0] = '0' + d;
    ds[1] = 0;

    /* don't allow leading zeros */
    if (strcmp(display, "0") == 0 && d == 0 && !hasdot) return;
    if (strcmp(display, "0") == 0 && d != 0 && !hasdot) strcpy(display, "");

    strcat(display, ds);
    operand = atof(display);

}

static void press_dot(void)
{

    if (newentry) {

        strcpy(display, "0");
        hasdot = FALSE;
        dotplace = 0;
        newentry = FALSE;

    }

    if (hasdot) return;
    hasdot = TRUE;
    strcat(display, ".");

}

static void press_operator(int newop)
{

    if (op && !newentry) {

        accumulator = do_op(accumulator, op, operand);
        format_display(accumulator);
        operand = accumulator;

    } else if (!op) {

        accumulator = operand;

    }

    op = newop;
    newentry = TRUE;

}

static void press_equals(void)
{

    if (op) {

        accumulator = do_op(accumulator, op, operand);
        format_display(accumulator);
        operand = accumulator;
        op = 0;
        newentry = TRUE;

    }

}

static void press_sign(void)
{

    operand = -operand;
    format_display(operand);

}

static void press_percent(void)
{

    if (op) {

        operand = accumulator * operand / 100.0;
        format_display(operand);

    } else {

        operand = operand / 100.0;
        format_display(operand);

    }

}

static void press_ce(void)
{

    operand = 0;
    strcpy(display, "0");
    newentry = TRUE;
    hasdot = FALSE;

}

static void press_backspace(void)
{

    int len;

    if (newentry) return;
    len = strlen(display);
    if (len <= 1 || (len == 2 && display[0] == '-')) {

        strcpy(display, "0");
        operand = 0;
        newentry = TRUE;
        return;

    }
    if (display[len-1] == '.') hasdot = FALSE;
    display[len-1] = 0;
    operand = atof(display);

}

/*******************************************************************************

Widget layout

Calculates positions based on current window size. Called on resize.
Kills all existing widgets and recreates them.

*******************************************************************************/

static int widgets_created = FALSE;

static void layout(void)
{

    int w, h;         /* window size */
    int bw, bh;       /* button size */
    int pad;          /* padding between buttons */
    int x0, y0;       /* origin for button grid */
    int dh;           /* display height */
    int r, c;         /* row, col */
    int x1, y1, x2, y2;
    int i;

    w = ami_maxxg(stdout);
    h = ami_maxyg(stdout);

    /* kill all existing widgets on resize */
    if (widgets_created) {

        ami_killwidget(stdout, ID_DISPLAY);
        ami_killwidget(stdout, ID_BACK);
        for (i = ID_0; i <= ID_DOT; i++) ami_killwidget(stdout, i);
        ami_killwidget(stdout, ID_PLUS);
        ami_killwidget(stdout, ID_MINUS);
        ami_killwidget(stdout, ID_MULT);
        ami_killwidget(stdout, ID_DIV);
        ami_killwidget(stdout, ID_EQUALS);
        ami_killwidget(stdout, ID_CLEAR);
        ami_killwidget(stdout, ID_CE);
        ami_killwidget(stdout, ID_SIGN);
        ami_killwidget(stdout, ID_PERCENT);
        ami_killwidget(stdout, ID_BKSP);

    }

    pad = w / 60;
    if (pad < 2) pad = 2;

    /* display takes top area */
    dh = h / NROWS - pad;
    if (dh < 20) dh = 20;

    /* button area below display */
    bw = (w - pad * (NCOLS + 1)) / NCOLS;
    bh = (h - dh - pad * (NROWS)) / (NROWS - 1);

    x0 = pad;
    y0 = dh + pad * 2;

    /* background for the whole window */
    ami_backgroundg(stdout, 1, 1, w, h, ID_BACK);

    /* display (edit box, read-only feel) */
    ami_editboxg(stdout, pad, pad, w - pad, dh, ID_DISPLAY);
    ami_putwidgettext(stdout, ID_DISPLAY, display);

    /* button grid layout:
       Row 0: C  CE  Bksp  /
       Row 1: 7  8   9     *
       Row 2: 4  5   6     -
       Row 3: 1  2   3     +
       Row 4: +/- 0  .     =
    */

    struct { int id; const char *label; } buttons[5][4] = {
        { {ID_CLEAR, "C"},   {ID_CE, "CE"},   {ID_BKSP, "<-"},  {ID_DIV, "/"} },
        { {ID_7, "7"},       {ID_8, "8"},      {ID_9, "9"},       {ID_MULT, "*"} },
        { {ID_4, "4"},       {ID_5, "5"},      {ID_6, "6"},       {ID_MINUS, "-"} },
        { {ID_1, "1"},       {ID_2, "2"},      {ID_3, "3"},       {ID_PLUS, "+"} },
        { {ID_SIGN, "+/-"},  {ID_0, "0"},      {ID_DOT, "."},     {ID_EQUALS, "="} }
    };

    for (r = 0; r < 5; r++) {
        for (c = 0; c < NCOLS; c++) {

            x1 = x0 + c * (bw + pad);
            y1 = y0 + r * (bh + pad);
            x2 = x1 + bw;
            y2 = y1 + bh;
            ami_buttong(stdout, x1, y1, x2, y2,
                        (char*)buttons[r][c].label, buttons[r][c].id);

        }
    }

    widgets_created = TRUE;

}

/*******************************************************************************

Handle button press

*******************************************************************************/

static void handle_button(int id)
{

    switch (id) {

        case ID_0: case ID_1: case ID_2: case ID_3: case ID_4:
        case ID_5: case ID_6: case ID_7: case ID_8: case ID_9:
            press_digit(id - ID_0);
            break;
        case ID_DOT:     press_dot();       break;
        case ID_PLUS:    press_operator('+'); break;
        case ID_MINUS:   press_operator('-'); break;
        case ID_MULT:    press_operator('*'); break;
        case ID_DIV:     press_operator('/'); break;
        case ID_EQUALS:  press_equals();     break;
        case ID_CLEAR:   calc_clear();       break;
        case ID_CE:      press_ce();         break;
        case ID_SIGN:    press_sign();       break;
        case ID_PERCENT: press_percent();    break;
        case ID_BKSP:    press_backspace();  break;

    }

    update_display();

}

/*******************************************************************************

Main

*******************************************************************************/

int main(void)
{

    ami_evtrec er;

    ami_title(stdout, "Calculator");
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_font(stdout, AMI_FONT_SIGN);

    setup_menu();

    calc_clear();
    layout();
    update_display();

    do {

        ami_event(stdin, &er);

        if (er.etype == ami_etredraw || er.etype == ami_etresize)
            layout();

        else if (er.etype == ami_etbutton)
            handle_button(er.butid);

        else if (er.etype == ami_etmenus) {

            switch (er.menuid) {

                case MENU_EXIT:
                    er.etype = ami_etterm;
                    break;

                case MENU_ABOUT:
                    ami_alert("About Calculator",
                              "Desk Calculator for Amitk\n"
                              "Copyright (C) 2026 S. A. Franco");
                    layout();
                    break;

            }

        }

    } while (er.etype != ami_etterm);

    return 0;

}
