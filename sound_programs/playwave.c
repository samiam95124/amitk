/*******************************************************************************

Play midi file

playmidi <.mid file>

Plays the given midi file.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <sound.h>
#include <option.h>

ami_long dport = AMI_WAVE_OUT; /* set default wave out */

ami_optrec opttbl[] = {

    { "port",  NULL, &dport,  NULL, NULL },
    { "p",     NULL, &dport,  NULL, NULL },
    { NULL,    NULL, NULL,    NULL, NULL }

};

int main(int argc, char **argv)

{

    ami_long argi = 1;
    ami_long argcl = argc;

    /* parse user options */
    ami_options(&argi, &argcl, argv, opttbl, TRUE);

    if (argcl != 2) {

        fprintf(stderr, "Usage: playwave [--port=<port>|-p <port>] <.wav file>\n");
        exit(1);

    }

    ami_loadwave(1, argv[argi]);
    ami_openwaveout(dport);
    ami_playwave(dport, 0, 1);
    ami_waitwave(dport);
    ami_closewaveout(dport);
    ami_delwave(1);

}
