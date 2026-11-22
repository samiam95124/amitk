/*******************************************************************************
*                                                                              *
*                       PICTURES OF THE DIALOGS                                *
*                                                                              *
*                    Copyright (C) 2026 Scott A. Franco                        *
*                                                                              *
* Takes the dialog figures for the manual. A dialog stops the thread that      *
* calls it until the user answers, so it cannot capture itself and cannot go   *
* on to the next one: this program opens one dialog per run and stands there   *
* holding it open while the rig's frame dump takes the picture from outside.   *
*                                                                              *
* Execution:                                                                   *
*                                                                              *
*     PD_DUMP=<prefix> dshot <which>                                           *
*                                                                              *
* where <which> is one of alert, color, open, save, find, findrep or font.     *
* The rig writes <prefix>-<window>-<frame>.ppm for every frame committed, and  *
* the dialog is its own window, so the last frame of the highest numbered      *
* window is the picture wanted:                                                *
*                                                                              *
*     PD_DUMP=/tmp/d timeout 12 bin/dshot alert                                *
*     ls /tmp/d-*.ppm | tail -1                                                *
*                                                                              *
* The run is ended from outside, by the timeout, because nothing answers the   *
* dialog. Run the file dialogs from a directory whose listing is worth         *
* printing: they show it, and whatever is in it goes in the manual.            *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <localdefs.h>
#include <graphics.h>

int main(int argc, char* argv[])

{

    char           s[250], r[250]; /* find and replace strings */
    ami_long       cr, cg, cb;     /* color */
    ami_qfnopts    optf;           /* find options */
    ami_qfropts    optfr;          /* find/replace options */
    ami_long       fc, fs;         /* font code and size */
    ami_long       fr, fg, fb;     /* foreground color */
    ami_long       br, bg, bb;     /* background color */
    ami_qfteffects fe;             /* font effects */
    char*          which;          /* which dialog to open */

    /* the window is made before main() is reached, and is held open at the
       end unless this is said, so it is said before anything can return */
    ami_autohold(FALSE);
    if (argc != 2) {

        fprintf(stderr,
                "usage: dshot alert|color|open|save|find|findrep|font\n");

        return (1);

    }
    which = argv[1];
    ami_title(stdout, "dialog pictures");

    if (!strcmp(which, "alert"))
        ami_alert((char*)"Attention", (char*)"This is an alert box.");
    else if (!strcmp(which, "color")) {

        cr = 0xc000; cg = 0x2000; cb = 0x8000;
        ami_querycolor(&cr, &cg, &cb);

    } else if (!strcmp(which, "open")) {

        strcpy(s, "myfile.txt");
        ami_queryopen(s, sizeof(s));

    } else if (!strcmp(which, "save")) {

        strcpy(s, "myfile.txt");
        ami_querysave(s, sizeof(s));

    } else if (!strcmp(which, "find")) {

        strcpy(s, "hello");
        optf = 0;
        ami_queryfind(s, sizeof(s), &optf);

    } else if (!strcmp(which, "findrep")) {

        strcpy(s, "hello");
        strcpy(r, "goodbye");
        optfr = 0;
        ami_queryfindrep(s, sizeof(s), r, sizeof(r), &optfr);

    } else if (!strcmp(which, "font")) {

        /* a size the sample can be read at */
        fc = 1; fs = 40;
        fr = fg = fb = 0;
        br = bg = bb = INT_MAX;
        fe = 0;
        ami_queryfont(stdout, &fc, &fs, &fr, &fg, &fb, &br, &bg, &bb, &fe);

    } else {

        fprintf(stderr, "dshot: no dialog called \"%s\"\n", which);

        return (1);

    }

    return (0);

}
