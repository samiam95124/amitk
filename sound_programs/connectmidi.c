/*******************************************************************************

Connect MIDI input port to MIDI output port

Simply copies MIDI commands from the given MIDI input port to the given MIDI
output port.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <sound.h>

int main(int argc, char **argv)

{

    int i;
    int key;
    int sport;
    int dport;
    ami_seqmsg sr;

    if (argc != 1 && argc != 3) {

        fprintf(stderr, "Usage: connectmidi [<input port> <output port>]\n");
        exit(1);

    }

    /* set default ports */
    sport = 1;
    dport = 1;

    /* set ports from command line if provided */
    if (argc == 3) {

        sport = atoi(argv[1]);
        dport = atoi(argv[2]);

    }

    /* enable this for debugging dump of incoming midi */
    //ami_setparamsynthin(7, "connect", "6");

    /* open target ports */
    ami_opensynthin(sport);
    ami_opensynthout(dport);

    /* transfer data continuously */
    while (1) {

        ami_rdsynth(sport, &sr);
        ami_wrsynth(dport, &sr);

    }

}
