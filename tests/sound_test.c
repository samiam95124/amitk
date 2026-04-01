/*******************************************************************************

Sound library test program

Goes through various test cases on the sound library.

Notes:

1. The MIDI tests not only test sound.c, but also the synthesizer
implementation.

*******************************************************************************/

#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>

#include <terminal.h> /* terminal level functions */
#include <sound.h>    /* sound library */
#include <option.h>   /* option parsing */

#define SECOND 10000 /* one second */


int dport = AMI_SYNTH_OUT; /* set default synth out */

ami_optrec opttbl[] = {

    { "port",   NULL,    &dport, NULL, NULL },
    { "p",      NULL,    &dport, NULL, NULL },
    { NULL,     NULL,    NULL,    NULL, NULL }

};

/* global variables */
static jmp_buf terminate_buf;

/*******************************************************************************

Wait time

wait time in 100 microseconds.

*******************************************************************************/

static void waittime(int t)

{

    ami_evtrec er; /* event record */

    ami_timer(stdout, 1, t, 0);
    do { ami_event(stdin, &er);
    } while (er.etype != ami_ettim && er.etype != ami_etterm);
    if (er.etype == ami_etterm) { longjmp(terminate_buf, 1); }

}

/*******************************************************************************

Wait user interaction

Wait return to be pressed, or handle terminate.

*******************************************************************************/

static void waitnext(void)

{

    ami_evtrec er; /* event record */

    do { ami_event(stdin, &er);
    } while (er.etype != ami_etenter && er.etype != ami_etterm);
    if (er.etype == ami_etterm) { longjmp(terminate_buf, 1); }

}

/*******************************************************************************

Wait return

Prints a message and waits for return to be pressed, or handle terminate.

*******************************************************************************/

static void waitret(void)

{

    printf("Hit return to continue\n");
    waitnext();

}

/*******************************************************************************

Play random notes

Plays random notes on the current instrument, for a given number of notes, to
the given port.

*******************************************************************************/

void playrand(int port, int notes)

{

    int key;
    int i;

    srand(42);
    for( i = 0; i < notes; i++) {

        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX)-1;
        /* Play a note */
        ami_noteon(port, 0, 1, key, INT_MAX);
        /* Sleep for 1/10 second */
        waittime(SECOND/20);
        /* Stop the note */
        ami_noteoff(port, 0, 1, key, 0);
        /* Sleep for 1/10 second */
        waittime(SECOND/20);

    }

}

/*******************************************************************************

Play note

Just plays a test note, with 1/4 on and off times. Plays middle C.

*******************************************************************************/

void playnote(int port, ami_note n)

{

    ami_noteon(port, 0, 1, n, INT_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteoff(port, 0, 1, n, 0);
    waittime(SECOND/4);

}


/*******************************************************************************

Play scale

Plays a simple scale with on time.

*******************************************************************************/

void playscale(int port, int t)

{

    ami_noteon(port, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_E+AMI_OCTAVE_6, INT_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_E+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_F+AMI_OCTAVE_6, INT_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_F+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, INT_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_A+AMI_OCTAVE_6, INT_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_A+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_B+AMI_OCTAVE_6, INT_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_B+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);

}


int main(int argc, char *argv[])

{

    ami_note       n; /* note */
    int           o; /* octave */
    ami_instrument ins; /* instrument */
    int           i, x, j;
    int           argi = 1;

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 1) {

        fprintf(stderr, "Usage: sndtst [--port=<port>|--p=<port>]\n");
        exit(1);

    }

    if (setjmp(terminate_buf)) goto terminate;

    /***************************************************************************

    MIDI TESTS

    ***************************************************************************/

    ami_opensynthout(dport);

    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);

    printf("Sound library test\n\n");
    printf("Runs through various sound tests and gives you a chance to\n");
    printf("evaluate if the sound produced matches the description.\n\n");
    printf("\n");
    printf("Note that this test can also serve as a test of the output synthesizer.\n");
    printf("Not all synths implement all modes or instruments. In fact, it is common\n");
    printf("to leave many features unimplemented.\n");
    waitret();

    printf("Run through the entire scale of notes available\n");
    for (n = AMI_NOTE_C+AMI_OCTAVE_1; n <= AMI_NOTE_G+AMI_OCTAVE_11; n++) {

        printf("%d ", n);
        ami_noteon(dport, 0, 1, n, INT_MAX);
        waittime(SECOND/10);
        ami_noteoff(dport, 0, 1, n, 0);

    }
    printf("\n");
    printf("Complete\n");
    waitret();

    printf("Run through all instruments with middle C\n");
    printf("Note that not all syths implement all instruments\n");
    printf("Instruments: ");
    for (ins = AMI_INST_ACOUSTIC_GRAND; ins <= AMI_INST_GUNSHOT; ins++) {

        printf("%d ", ins);
        ami_instchange(dport, 0, 1, ins);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/10);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/10);

    }
    printf("\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    printf("Complete\n");
    waitret();

    printf("Run though all percussive instruments\n");
    printf("Note that not all syths implement all instruments\n");
    printf("Instruments: ");
    for (n = AMI_NOTE_ACOUSTIC_BASS_DRUM; n <= AMI_NOTE_OPEN_TRIANGLE; n++) {

        printf("%d ", n);
        ami_noteon(dport, 0, 10, n, INT_MAX);
        waittime(SECOND/10);
        ami_noteoff(dport, 0, 10, n, 0);
        waittime(SECOND/10);

    }
    printf("\n");
    printf("Complete\n");
    waitret();

    printf("Chop test, play note series and repeat with the envelope time\n");
    printf("limited by noteoff\n");
    printf("First piano, then organ\n");
    printf("Note that some synths appear to set a minimum on note length\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    for (i = 10; i >= 1; i--) playscale(dport, i*(SECOND/30));
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    for (i = 10; i >= 1; i--) playscale(dport, i*(SECOND/30));
    printf("Complete\n");
    waitret();

    printf("Note volume test\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    for (i = 0; i < 20; i++) {

        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, i*(INT_MAX/20));
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    printf("Complete\n");
    waitret();

    printf("Random note programming piano:\n");
    waitret();
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming harpsichord:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_HARPSICHORD);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming organ:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming soprando sax:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_SOPRANO_SAX);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming telephone:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_TELEPHONE_RING);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    /* restore piano */
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);

    /* set attack times */
    printf("Set step attack times on piano\n");
    waitret();
    for (i = 0; i <= 10; i++) {

        printf("Attack: %d\n", i*(INT_MAX/10));
        ami_attack(dport, 0, 1, i*(INT_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_attack(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set step attack times on organ\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    for (i = 0; i <= 10; i++) {

        printf("Attack: %d\n", i*(INT_MAX/10));
        ami_attack(dport, 0, 1, i*(INT_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_attack(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set release times */
    printf("Set step release times on piano\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    for (i = 0; i <= 10; i++) {

        printf("Release: %d\n", i*(INT_MAX/10));
        ami_release(dport, 0, 1, i*(INT_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_release(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set step release times on organ\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    for (i = 0; i <= 10; i++) {

        printf("Release: %d\n", i*(INT_MAX/10));
        ami_release(dport, 0, 1, i*(INT_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_release(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set legato */
    printf("Set legato on piano, first normal, then legato\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    ami_legato(dport, 0, 1, FALSE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
    /* now repeat with legato on */
    ami_legato(dport, 0, 1, TRUE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
    ami_legato(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set legato on organ, first normal, then legato\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_legato(dport, 0, 1, FALSE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
    /* now repeat with legato on */
    ami_legato(dport, 0, 1, TRUE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
    ami_legato(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set portamento */
    printf("Set portamento on piano, first normal, then portamento, through\n");
    printf("various portamento times\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    for (i = 0; i < 10; i++) {

        printf("Portamento time: %d\n", i*(INT_MAX/10));
        ami_porttime(dport, 0, 1, i*(INT_MAX/10));
        ami_portamento(dport, 0, 1, FALSE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
        /* now repeat with portamento on */
        ami_portamento(dport, 0, 1, TRUE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);

    }
    ami_portamento(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set portamento on organ, first normal, then portamento\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    for (i = 0; i < 10; i++) {

        printf("Portamento time: %d\n", i*(INT_MAX/10));
        ami_portamento(dport, 0, 1, FALSE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);
        /* now repeat with portamento on */
        ami_portamento(dport, 0, 1, TRUE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, INT_MAX);

    }
    ami_portamento(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Channel volume test. Play note continuously while advancing volume\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* advance volume sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Volume: %d\n", i*(INT_MAX/20));
        ami_volsynthchan(dport, 0, 1, i*(INT_MAX/20));
        waittime(SECOND/4);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* reset channel vol to midline */
    ami_volsynthchan(dport, 0, 1, INT_MAX/2);
    printf("Complete\n");
    waitret();

    printf("Channel balance test. Play note continuously while changing\n");
    printf("from to right\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* advance volume sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Balance: %d\n", -INT_MAX+(i*(INT_MAX/20*2)));
        ami_balance(dport, 0, 1, -INT_MAX+(i*(INT_MAX/20*2)));
        waittime(SECOND/4);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* reset channel balance to midline */
    ami_balance(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel vibrato test. Play note continuously while advancing vibrato\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* advance vibrato sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Vibrato: %d\n", i*(INT_MAX/20));
        ami_vibrato(dport, 0, 1, i*(INT_MAX/20));
        waittime(SECOND);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* reset channel vibrato to midline */
    ami_vibrato(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel pan test. Play note continuously while changing\n");
    printf("pan from to right\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* advance pan sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Pan: %d\n", -INT_MAX+(i*(INT_MAX/20*2)));
        ami_pan(dport, 0, 1, -INT_MAX+(i*(INT_MAX/20*2)));
        waittime(SECOND/4);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    /* reset channel pan to midline */
    ami_pan(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel timbre test. Play notes while advancing timbre\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance timbre sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Timbre: %d\n", i*(INT_MAX/20));
        ami_timbre(dport, 0, 1, i*(INT_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel timbre */
    ami_timbre(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel brightness test. Play notes while advancing brightness\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance brightness sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Brightness: %d\n", i*(INT_MAX/20));
        ami_brightness(dport, 0, 1, i*(INT_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel brightness */
    ami_brightness(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel reverb test. Play notes while advancing reverb\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance reverb sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Reverb: %d\n", i*(INT_MAX/20));
        ami_reverb(dport, 0, 1, i*(INT_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel reverb */
    ami_reverb(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel tremulo test. Play notes while advancing tremulo\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);

    /* advance tremulo sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Tremulo: %d\n", i*(INT_MAX/20));
        ami_tremulo(dport, 0, 1, i*(INT_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel tremulo */
    ami_tremulo(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel chorus test. Play notes while advancing chorus\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);

    /* advance chorus sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Chorus: %d\n", i*(INT_MAX/20));
        ami_chorus(dport, 0, 1, i*(INT_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel chorus */
    ami_chorus(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel celeste test. Play notes while advancing celeste\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance celeste sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Celeste: %d\n", i*(INT_MAX/20));
        ami_celeste(dport, 0, 1, i*(INT_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel celeste */
    ami_celeste(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel phaser test. Play notes while advancing phaser\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance phaser sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Phaser: %d\n", i*(INT_MAX/20));
        ami_phaser(dport, 0, 1, i*(INT_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel phaser */
    ami_phaser(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    /* don't know about this test, it seems to limit the total pitch wheel range,
       which is not right */
    printf("pitch wheel. Vary pitch wheel while playing continuously\n");
    ami_instchange(dport, 0, 1, AMI_INST_LEAD_1_SQUARE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, INT_MAX);
    for (j = 0; j < 10; j++) {

        printf("Pitchrange: %d\n", j*(INT_MAX/10));
        ami_pitchrange(dport, 0, 1, j*(INT_MAX/10));
        for (x = 0; x < 10; x++)
            for (i = 0; i < 10; i++) {

            printf("Pitch: %d\n", -INT_MAX+(i*(INT_MAX/10*2)));
            ami_pitch(dport, 0, 1, -INT_MAX+(i*(INT_MAX/10*2)));
            waittime(SECOND/100);

        }

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    ami_pitch(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

terminate: /* terminate */
    ami_closesynthout(dport);
    printf("\n");

    return (0); /* return no error */

}
