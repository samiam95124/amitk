/*
 * Exercise for each dialog implemented in portable/gnome_widgets.c.
 * Invokes them in sequence: querycolor, queryopen, querysave, queryfind,
 * queryfindrep, queryfont. Each dialog's output is printed so the user
 * can verify round-trip behaviour.
 *
 * Build:  make testg
 * Run:    ./testg
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <graphics.h>

static void print_find_opts(const char* label, ami_qfnopts opt)
{
    printf("%s flags:", label);
    if (opt & (1 << ami_qfncase)) printf(" case");
    if (opt & (1 << ami_qfnup))   printf(" up");
    if (opt & (1 << ami_qfnre))   printf(" re");
    printf("\n");
}

static void print_replace_opts(const char* label, ami_qfropts opt)
{
    printf("%s flags:", label);
    if (opt & (1 << ami_qfrcase))    printf(" case");
    if (opt & (1 << ami_qfrup))      printf(" up");
    if (opt & (1 << ami_qfrre))      printf(" re");
    if (opt & (1 << ami_qfrfind))    printf(" find");
    if (opt & (1 << ami_qfrallfil))  printf(" all");
    if (opt & (1 << ami_qfralllin))  printf(" line");
    printf("\n");
}

static void print_font_effects(const char* label, ami_qfteffects eff)
{
    printf("%s effects:", label);
    if (eff & (1 << ami_qftestrikeout))  printf(" strikeout");
    if (eff & (1 << ami_qfteunderline))  printf(" underline");
    if (eff & (1 << ami_qftebold))       printf(" bold");
    if (eff & (1 << ami_qfteitalic))     printf(" italic");
    printf("\n");
}

int main(void)
{
    char buf[1024];
    char rep[1024];
    int  r, g, b;
    ami_qfnopts   fopt;
    ami_qfropts   fropt;
    ami_qfteffects fe;
    int  fc, fs, fr, fg, fb, br, bg, bb;

    /* --- querycolor ------------------------------------------------- */
    /* Petit-Ami color channels are ratioed against INT_MAX. */
    r = INT_MAX/2; g = INT_MAX/4; b = INT_MAX/4*3;
    printf("Calling ami_querycolor() with default rgb=(%d,%d,%d)...\n",
           r, g, b);
    fflush(stdout);
    ami_querycolor(&r, &g, &b);
    printf("Color returned: rgb=(%d,%d,%d)\n", r, g, b);
    fflush(stdout);

    /* --- queryopen -------------------------------------------------- */
    strcpy(buf, "test.c");
    printf("\nCalling ami_queryopen() with default \"%s\"...\n", buf);
    fflush(stdout);
    ami_queryopen(buf, sizeof(buf));
    printf("Open: %s\n", buf[0] ? buf : "(cancelled)");
    fflush(stdout);

    /* --- querysave -------------------------------------------------- */
    strcpy(buf, "output.txt");
    printf("\nCalling ami_querysave() with default \"%s\"...\n", buf);
    fflush(stdout);
    ami_querysave(buf, sizeof(buf));
    printf("Save: %s\n", buf[0] ? buf : "(cancelled)");
    fflush(stdout);

    /* --- queryfind -------------------------------------------------- */
    strcpy(buf, "hello");
    fopt = (1 << ami_qfncase); /* preselect "match case" */
    printf("\nCalling ami_queryfind() with default \"%s\"...\n", buf);
    print_find_opts("  input", fopt);
    fflush(stdout);
    ami_queryfind(buf, sizeof(buf), &fopt);
    printf("Find: %s\n", buf[0] ? buf : "(cancelled)");
    print_find_opts("  output", fopt);
    fflush(stdout);

    /* --- queryfindrep ----------------------------------------------- */
    strcpy(buf, "foo");
    strcpy(rep, "bar");
    fropt = 0;
    printf("\nCalling ami_queryfindrep() with find=\"%s\" replace=\"%s\"...\n",
           buf, rep);
    print_replace_opts("  input", fropt);
    fflush(stdout);
    ami_queryfindrep(buf, sizeof(buf), rep, sizeof(rep), &fropt);
    if (buf[0] == 0 && rep[0] == 0) {
        printf("FindRep: (cancelled)\n");
    } else {
        printf("FindRep: find=\"%s\" replace=\"%s\"\n", buf, rep);
        print_replace_opts("  output", fropt);
    }
    fflush(stdout);

    /* --- queryfont -------------------------------------------------- */
    fc = 1; fs = 14;
    fr = INT_MAX; fg = 0; fb = 0;          /* red */
    br = INT_MAX; bg = INT_MAX; bb = INT_MAX; /* white */
    fe = 0;
    printf("\nCalling ami_queryfont() with default font=%d size=%d...\n",
           fc, fs);
    print_font_effects("  input", fe);
    fflush(stdout);
    ami_queryfont(stdout, &fc, &fs, &fr, &fg, &fb, &br, &bg, &bb, &fe);
    printf("Font returned: font=%d size=%d\n", fc, fs);
    print_font_effects("  output", fe);
    fflush(stdout);

    return 0;
}
