/*******************************************************************************

Play sounds via keyboard

Yes, its the standard annoying utility to turn your keyoard into an organ.

*******************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#include <terminal.h> /* terminal level API */
#include <sound.h>    /* sound API */
#include <option.h>

#define SECOND  10000
#define KEYDOWN 10

ami_evtrec  er;    /* event record */
ami_channel chan;  /* channel */
int        velo;  /* velocity */
int        keycnt[36];
int        ki;
int port = AMI_SYNTH_OUT; /* set default synth out */

ami_optrec opttbl[] = {

    { "port",  NULL, &port,  NULL, NULL   },
    { "p",     NULL, &port,  NULL, NULL   },
    { NULL,    NULL, NULL,    NULL, NULL }

};

void keyon(int n)

{

    if (keycnt[n] == 0)/* key not already down */
       switch (n) { /* key */

        case 1:  ami_noteon(port, 0, chan, AMI_NOTE_C+AMI_OCTAVE_4, velo);       break;
        case 2:  ami_noteon(port, 0, chan, AMI_NOTE_C_SHARP+AMI_OCTAVE_4, velo); break;
        case 3:  ami_noteon(port, 0, chan, AMI_NOTE_D+AMI_OCTAVE_4, velo);       break;
        case 4:  ami_noteon(port, 0, chan, AMI_NOTE_D_SHARP+AMI_OCTAVE_4, velo); break;
        case 5:  ami_noteon(port, 0, chan, AMI_NOTE_E+AMI_OCTAVE_4, velo);       break;
        case 6:  ami_noteon(port, 0, chan, AMI_NOTE_F+AMI_OCTAVE_4, velo);       break;
        case 7:  ami_noteon(port, 0, chan, AMI_NOTE_F_SHARP+AMI_OCTAVE_4, velo); break;
        case 8:  ami_noteon(port, 0, chan, AMI_NOTE_G+AMI_OCTAVE_4, velo);       break;
        case 9:  ami_noteon(port, 0, chan, AMI_NOTE_G_SHARP+AMI_OCTAVE_4, velo); break;
        case 10: ami_noteon(port, 0, chan, AMI_NOTE_A+AMI_OCTAVE_4, velo);       break;
        case 11: ami_noteon(port, 0, chan, AMI_NOTE_A_SHARP+AMI_OCTAVE_4, velo); break;
        case 12: ami_noteon(port, 0, chan, AMI_NOTE_B+AMI_OCTAVE_4, velo);       break;
        case 13: ami_noteon(port, 0, chan, AMI_NOTE_C+AMI_OCTAVE_5, velo);       break;
        case 14: ami_noteon(port, 0, chan, AMI_NOTE_C_SHARP+AMI_OCTAVE_5, velo); break;
        case 15: ami_noteon(port, 0, chan, AMI_NOTE_D+AMI_OCTAVE_5, velo);       break;
        case 16: ami_noteon(port, 0, chan, AMI_NOTE_D_SHARP+AMI_OCTAVE_5, velo); break;
        case 17: ami_noteon(port, 0, chan, AMI_NOTE_E+AMI_OCTAVE_5, velo);       break;
        case 18: ami_noteon(port, 0, chan, AMI_NOTE_F+AMI_OCTAVE_5, velo);       break;
        case 19: ami_noteon(port, 0, chan, AMI_NOTE_F_SHARP+AMI_OCTAVE_5, velo); break;
        case 20: ami_noteon(port, 0, chan, AMI_NOTE_G+AMI_OCTAVE_5, velo);       break;
        case 21: ami_noteon(port, 0, chan, AMI_NOTE_G_SHARP+AMI_OCTAVE_5, velo); break;
        case 22: ami_noteon(port, 0, chan, AMI_NOTE_A+AMI_OCTAVE_5, velo);       break;
        case 23: ami_noteon(port, 0, chan, AMI_NOTE_A_SHARP+AMI_OCTAVE_5, velo); break;
        case 24: ami_noteon(port, 0, chan, AMI_NOTE_B+AMI_OCTAVE_5, velo);       break;
        case 25: ami_noteon(port, 0, chan, AMI_NOTE_C+AMI_OCTAVE_6, velo);       break;
        case 26: ami_noteon(port, 0, chan, AMI_NOTE_C_SHARP+AMI_OCTAVE_6, velo); break;
        case 27: ami_noteon(port, 0, chan, AMI_NOTE_D+AMI_OCTAVE_6, velo);       break;
        case 28: ami_noteon(port, 0, chan, AMI_NOTE_D_SHARP+AMI_OCTAVE_6, velo); break;
        case 29: ami_noteon(port, 0, chan, AMI_NOTE_E+AMI_OCTAVE_6, velo);       break;
        case 30: ami_noteon(port, 0, chan, AMI_NOTE_F+AMI_OCTAVE_6, velo);       break;
        case 31: ami_noteon(port, 0, chan, AMI_NOTE_F_SHARP+AMI_OCTAVE_6, velo); break;
        case 32: ami_noteon(port, 0, chan, AMI_NOTE_G+AMI_OCTAVE_6, velo);       break;
        case 33: ami_noteon(port, 0, chan, AMI_NOTE_G_SHARP+AMI_OCTAVE_6, velo); break;
        case 34: ami_noteon(port, 0, chan, AMI_NOTE_A+AMI_OCTAVE_6, velo);       break;
        case 35: ami_noteon(port, 0, chan, AMI_NOTE_A_SHARP+AMI_OCTAVE_6, velo); break;
        case 36: ami_noteon(port, 0, chan, AMI_NOTE_B+AMI_OCTAVE_6, velo);       break;

    }
    keycnt[n] = KEYDOWN; /* start, or restart, key down count */

}

void keyoff(int n)

{

    switch (n) { /* key */

        case 1:  ami_noteoff(port, 0, chan, AMI_NOTE_C+AMI_OCTAVE_4, velo);       break;
        case 2:  ami_noteoff(port, 0, chan, AMI_NOTE_C_SHARP+AMI_OCTAVE_4, velo); break;
        case 3:  ami_noteoff(port, 0, chan, AMI_NOTE_D+AMI_OCTAVE_4, velo);       break;
        case 4:  ami_noteoff(port, 0, chan, AMI_NOTE_D_SHARP+AMI_OCTAVE_4, velo); break;
        case 5:  ami_noteoff(port, 0, chan, AMI_NOTE_E+AMI_OCTAVE_4, velo);       break;
        case 6:  ami_noteoff(port, 0, chan, AMI_NOTE_F+AMI_OCTAVE_4, velo);       break;
        case 7:  ami_noteoff(port, 0, chan, AMI_NOTE_F_SHARP+AMI_OCTAVE_4, velo); break;
        case 8:  ami_noteoff(port, 0, chan, AMI_NOTE_G+AMI_OCTAVE_4, velo);       break;
        case 9:  ami_noteoff(port, 0, chan, AMI_NOTE_G_SHARP+AMI_OCTAVE_4, velo); break;
        case 10: ami_noteoff(port, 0, chan, AMI_NOTE_A+AMI_OCTAVE_4, velo);       break;
        case 11: ami_noteoff(port, 0, chan, AMI_NOTE_A_SHARP+AMI_OCTAVE_4, velo); break;
        case 12: ami_noteoff(port, 0, chan, AMI_NOTE_B+AMI_OCTAVE_4, velo);       break;
        case 13: ami_noteoff(port, 0, chan, AMI_NOTE_C+AMI_OCTAVE_5, velo);       break;
        case 14: ami_noteoff(port, 0, chan, AMI_NOTE_C_SHARP+AMI_OCTAVE_5, velo); break;
        case 15: ami_noteoff(port, 0, chan, AMI_NOTE_D+AMI_OCTAVE_5, velo);       break;
        case 16: ami_noteoff(port, 0, chan, AMI_NOTE_D_SHARP+AMI_OCTAVE_5, velo); break;
        case 17: ami_noteoff(port, 0, chan, AMI_NOTE_E+AMI_OCTAVE_5, velo);       break;
        case 18: ami_noteoff(port, 0, chan, AMI_NOTE_F+AMI_OCTAVE_5, velo);       break;
        case 19: ami_noteoff(port, 0, chan, AMI_NOTE_F_SHARP+AMI_OCTAVE_5, velo); break;
        case 20: ami_noteoff(port, 0, chan, AMI_NOTE_G+AMI_OCTAVE_5, velo);       break;
        case 21: ami_noteoff(port, 0, chan, AMI_NOTE_G_SHARP+AMI_OCTAVE_5, velo); break;
        case 22: ami_noteoff(port, 0, chan, AMI_NOTE_A+AMI_OCTAVE_5, velo);       break;
        case 23: ami_noteoff(port, 0, chan, AMI_NOTE_A_SHARP+AMI_OCTAVE_5, velo); break;
        case 24: ami_noteoff(port, 0, chan, AMI_NOTE_B+AMI_OCTAVE_5, velo);       break;
        case 25: ami_noteoff(port, 0, chan, AMI_NOTE_C+AMI_OCTAVE_6, velo);       break;
        case 26: ami_noteoff(port, 0, chan, AMI_NOTE_C_SHARP+AMI_OCTAVE_6, velo); break;
        case 27: ami_noteoff(port, 0, chan, AMI_NOTE_D+AMI_OCTAVE_6, velo);       break;
        case 28: ami_noteoff(port, 0, chan, AMI_NOTE_D_SHARP+AMI_OCTAVE_6, velo); break;
        case 29: ami_noteoff(port, 0, chan, AMI_NOTE_E+AMI_OCTAVE_6, velo);       break;
        case 30: ami_noteoff(port, 0, chan, AMI_NOTE_F+AMI_OCTAVE_6, velo);       break;
        case 31: ami_noteoff(port, 0, chan, AMI_NOTE_F_SHARP+AMI_OCTAVE_6, velo); break;
        case 32: ami_noteoff(port, 0, chan, AMI_NOTE_G+AMI_OCTAVE_6, velo);       break;
        case 33: ami_noteoff(port, 0, chan, AMI_NOTE_G_SHARP+AMI_OCTAVE_6, velo); break;
        case 34: ami_noteoff(port, 0, chan, AMI_NOTE_A+AMI_OCTAVE_6, velo);       break;
        case 35: ami_noteoff(port, 0, chan, AMI_NOTE_A_SHARP+AMI_OCTAVE_6, velo); break;
        case 36: ami_noteoff(port, 0, chan, AMI_NOTE_B+AMI_OCTAVE_6, velo);       break;

    }

}

void tick(void)

{

    int i;

    for (i = 0; i < 36; i++) /* scan keys */
        if (keycnt[i] > 0) { /* there is an active key */

        keycnt[i]--; /* count */
        if (!keycnt[i]) keyoff(i); /* process key off */

    }

}

int main(int argc, char **argv)

{

    int argi = 1;

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 1) {

        fprintf(stderr, "Usage: keyboard [--port=<port>|-p=<port>]\n");

        exit(1);

    }

    ami_opensynthout(port); /* open main synthesiser */
    chan = 1; /* set channel 1 */
    velo = INT_MAX; /* set velocity */
    for (ki = 0; ki < 36; ki++) keycnt[ki] = 0; /* clear key down counts */
    ami_timer(stdout, 1, SECOND/4/10, TRUE); /* set basic timer */
    do { /* events */

        ami_event(stdin, &er); /* get next event */
        if (er.etype == ami_etchar) /* its a standard key */
            switch (tolower(er.echar)) {

            case '1':  keyon(1); break;
            case '2':  keyon(2); break;
            case '3':  keyon(3); break;
            case '4':  keyon(4); break;
            case '5':  keyon(5); break;
            case '6':  keyon(6); break;
            case '7':  keyon(7); break;
            case '8':  keyon(8); break;
            case '9':  keyon(9); break;
            case '0':  keyon(10); break;
            case '-':  keyon(11); break;
            case '=':  keyon(12); break;
            case 'q':  keyon(13); break;
            case 'w':  keyon(14); break;
            case 'e':  keyon(15); break;
            case 'r':  keyon(16); break;
            case 't':  keyon(17); break;
            case 'y':  keyon(18); break;
            case 'u':  keyon(19); break;
            case 'i':  keyon(20); break;
            case 'o':  keyon(21); break;
            case 'p':  keyon(22); break;
            case '[':  keyon(23); break;
            case ']':  keyon(24); break;
            case 'a':  keyon(25); break;
            case 's':  keyon(26); break;
            case 'd':  keyon(27); break;
            case 'f':  keyon(28); break;
            case 'g':  keyon(29); break;
            case 'h':  keyon(30); break;
            case 'j':  keyon(31); break;
            case 'k':  keyon(32); break;
            case 'l':  keyon(33); break;
            case ';':  keyon(34); break;
            case '\'': keyon(35); break;

        } else if (er.etype == ami_etenter) keyon(36);
        else if (er.etype == ami_etfun) {

            if (er.fkey == 1) ami_instchange(port, 0, 1, AMI_INST_ACOUSTIC_GRAND);
            else if (er.fkey == 2) ami_instchange(port, 0, 1, AMI_INST_ELECTRIC_GRAND);
            else if (er.fkey == 3) ami_instchange(port, 0, 1, AMI_INST_ROCK_ORGAN);
            else if (er.fkey == 4) ami_instchange(port, 0, 1, AMI_INST_CHURCH_ORGAN);
            else if (er.fkey == 5) ami_instchange(port, 0, 1, AMI_INST_NYLON_STRING_GUITAR);
            else if (er.fkey == 6) ami_instchange(port, 0, 1, AMI_INST_OVERDRIVEN_GUITAR);
            else if (er.fkey == 7) ami_instchange(port, 0, 1, AMI_INST_TRUMPET);
            else if (er.fkey == 8) ami_instchange(port, 0, 1, AMI_INST_LEAD_1_SQUARE);
            else if (er.fkey == 9) ami_instchange(port, 0, 1, AMI_INST_LEAD_2_SAWTOOTH);
            else if (er.fkey == 10) ami_instchange(port, 0, 1, AMI_INST_PAD_1_NEW_AGE);
            else if (er.fkey == 11) ami_instchange(port, 0, 1, AMI_INST_PAD_3_POLYSYNTH);
            else if (er.fkey == 12) ami_instchange(port, 0, 1, AMI_INST_WOODBLOCK);

        } else if (er.etype == ami_ettim) tick(); /* process key timers */

    } while (er.etype != ami_etterm);

}
