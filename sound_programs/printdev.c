/*******************************************************************************

Print device tables

Prints the available device tables.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sound.h>
#include <localdefs.h>
#include <option.h>

#define BUFLEN 200

int is = FALSE; /* print input synth ports */
int os = FALSE;
int iw = FALSE;
int ow = FALSE;

ami_optrec opttbl[] = {

    { "is", &is,  NULL,  NULL, NULL },
    { "os", &os,  NULL,  NULL, NULL },
    { "iw", &iw,  NULL,  NULL, NULL },
    { "ow", &ow,  NULL,  NULL, NULL },
    { NULL, NULL, NULL,  NULL, NULL }

};

int main(int argc, char **argv)

{

    int i;
    char buff[BUFLEN];
    int max;
    int argi = 1;

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 1) {

        fprintf(stderr, "Usage: printdev [--is|--os|--iw|--ow\n");
        exit(1);

    }

    /* the default is print all */
    if (!is && !os && !iw && !ow) { is = os = iw = ow = TRUE; }

    printf("\n");

    if (is) {

        printf("Input synthesizer devices:\n\n");
        for (i = 1; i <= ami_synthin(); i++) {

            ami_synthinname(i, buff, BUFLEN);
            printf("%2d: %s\n", i, buff);

        }
        printf("\n");

    }

    if (os) {

        printf("Output synthesizer devices:\n\n");
        for (i = 1; i <= ami_synthout(); i++) {

            ami_synthoutname(i, buff, BUFLEN);
            printf("%2d: %s\n", i, buff);

        }
        printf("\n");

    }

    if (iw) {

        /* sweep for max len */
        max = 0;
        for (i = 1; i <= ami_wavein(); i++) {

            ami_waveinname(i, buff, BUFLEN);
            if (strlen(buff) > max) max = strlen(buff);

        }
        printf("Input wave devices:\n\n");
        for (i = 1; i <= ami_wavein(); i++) {

            ami_waveinname(i, buff, BUFLEN);
            printf("%2d: %-*.*s channels: %d rate: %5d len: %2d sign: %d "
                   "big endian: %d float: %d\n", i, max, max, buff,
                   ami_chanwavein(i), ami_ratewavein(i), ami_lenwavein(i),
                   ami_sgnwavein(i), ami_endwavein(i), ami_fltwavein(i));

        }
        printf("\n");

    }

    if (ow) {

        printf("Output wave devices:\n\n");
        for (i = 1; i <= ami_waveout(); i++) {

            ami_waveoutname(i, buff, BUFLEN);
            printf("%2d: %s\n", i, buff);

        }
        printf("\n");

    }

}
