/*******************************************************************************

Play midi file

playmidi [--port=<port>|-p=<port>] <.mid file>

Plays the given midi file.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <sound.h>
#include <option.h>

int dport = AMI_SYNTH_OUT; /* set default synth out */

ami_optrec opttbl[] = {

    { "port",  NULL, &dport,  NULL, NULL },
    { "p",     NULL, &dport,  NULL, NULL },
    { NULL,    NULL, NULL,    NULL, NULL }

};

int main(int argc, char **argv)

{

    int argi = 1;

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc < 2) {

        fprintf(stderr, "Usage: playmidi [--port=<port>|-p=<port>] <.mid file>\n");
        exit(1);

    }

    ami_loadsynth(1, argv[argi]);
    ami_opensynthout(dport);
    ami_playsynth(dport, 0, 1);
    ami_waitsynth(dport);
    ami_closesynthout(dport);
    ami_delsynth(1);

}
