/*******************************************************************************

Sound library test program

Goes through various test cases on the sound library.

Notes:

1. The MIDI tests not only test sound.c, but also the synthesizer
implementation.

2. The sections after the synthesizer listening tests cover the rest of
the API: devices and their names, parameters, the sequencer proper
(timed playback), wrsynth, aftertouch/pressure/mono/poly, MIDI file
playback, wave output synthesized and from a file, wave input, and
synthesizer input. The files the file tests need are made on the spot
and removed. --nomidi starts there; --sin=<port> enables the
synthesizer input section, which wants a keyboard played.

3. The virtual MIDI loop runs by itself: the output and input sides of
ALSA's "virtual" port are opened and subscribed together with aconnect,
which puts the library's encoder on one end of a wire and its decoder
on the other. One message of each kind is sent and read back, checked
against what was sent to within a wire step. Where the port or aconnect
is missing the section says so and stands down.

*******************************************************************************/

#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <terminal.h> /* terminal level functions */
#include <services.h> /* the option character */
#include <sound.h>    /* sound library */
#include <option.h>   /* option parsing */

#define SECOND 10000 /* one second */


ami_long dport = AMI_SYNTH_OUT; /* set default synth out */
ami_long wport = AMI_WAVE_OUT;  /* wave output port */
ami_long iport = AMI_WAVE_IN;   /* wave input port */
ami_long sport = 0;             /* synth input port; 0 skips the section, since
                               reading a keyboard nobody plays blocks */
ami_long nomidi = FALSE;        /* skip the synthesizer listening tests and go
                               straight to the sections after them */

ami_long tstlo = 1;             /* first test to run */
ami_long tsthi = 1000;          /* last test to run */

/* is the test within the selected range? sound_test n skips to test n
   and runs on to the end; sound_test n x runs n through x; no arguments
   runs all */
static int tst(ami_long n)

{

    return (n >= tstlo && n <= tsthi);

}

ami_optrec opttbl[] = {

    { "port",   NULL,    &dport, NULL, NULL },
    { "p",      NULL,    &dport, NULL, NULL },
    { "wport",  NULL,    &wport, NULL, NULL },
    { "w",      NULL,    &wport, NULL, NULL },
    { "iport",  NULL,    &iport, NULL, NULL },
    { "i",      NULL,    &iport, NULL, NULL },
    { "sin",    NULL,    &sport, NULL, NULL },
    { "si",     NULL,    &sport, NULL, NULL },
    { "nomidi", &nomidi, NULL,   NULL, NULL },
    { "nm",     &nomidi, NULL,   NULL, NULL },
    { NULL,     NULL,    NULL,    NULL, NULL }

};

/* global variables */
static jmp_buf terminate_buf;

/*******************************************************************************

Wait time

wait time in 100 microseconds.

*******************************************************************************/

static void waittime(ami_long t)

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

Test files, made on the spot

The wave and MIDI file tests need a file to load, and none is committed:
a small one of each is written here, played, and removed. The wave is a
sine at the given pitch, mono, sixteen bits, the standard rate. The MIDI
file is the smallest thing that is a song: format 0, one track, three
quarter notes of a C major chord walked up, on the piano.

*******************************************************************************/

#define TESTRATE 44100

static void makewav(const char* fn, double freq, double secs)

{

    FILE* f;
    ami_long  n = (ami_long)(TESTRATE*secs);
    ami_long  dlen = n*2;
    ami_long  i;

    f = fopen(fn, "wb");
    if (!f) { printf("could not write %s\n", fn); return; }
    /* the RIFF/fmt/data framing, all little endian */
    fprintf(f, "RIFF");
    fputc((36+dlen)&0xff, f); fputc((36+dlen)>>8&0xff, f);
    fputc((36+dlen)>>16&0xff, f); fputc((36+dlen)>>24&0xff, f);
    fprintf(f, "WAVEfmt ");
    fputc(16, f); fputc(0, f); fputc(0, f); fputc(0, f); /* fmt length */
    fputc(1, f); fputc(0, f);                            /* PCM */
    fputc(1, f); fputc(0, f);                            /* mono */
    fputc(TESTRATE&0xff, f); fputc(TESTRATE>>8&0xff, f); /* rate */
    fputc(TESTRATE>>16&0xff, f); fputc(0, f);
    fputc((TESTRATE*2)&0xff, f); fputc((TESTRATE*2)>>8&0xff, f); /* byte rate */
    fputc((TESTRATE*2)>>16&0xff, f); fputc(0, f);
    fputc(2, f); fputc(0, f);                            /* block align */
    fputc(16, f); fputc(0, f);                           /* bits */
    fprintf(f, "data");
    fputc(dlen&0xff, f); fputc(dlen>>8&0xff, f);
    fputc(dlen>>16&0xff, f); fputc(dlen>>24&0xff, f);
    for (i = 0; i < n; i++) {

        /* the tone, eased in and out so it does not click */
        double a = sin(2.0*3.14159265*freq*i/TESTRATE)*20000.0;
        ami_long   e = n/50;
        short  s;

        if (i < e) a = a*i/e;
        if (n-i < e) a = a*(n-i)/e;
        s = (short)a;
        fputc(s&0xff, f); fputc(s>>8&0xff, f);

    }
    fclose(f);

}

static void makemid(const char* fn)

{

    FILE* f;
    /* MThd: format 0, one track, 96 per quarter; MTrk: piano, C then E
       then G, a quarter note each, end of track */
    static const unsigned char smf[] = {

        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,32,
        0x00, 0xc0, 0x00,       /* program change, piano */
        0x00, 0x90, 60, 0x60,   /* C on */
        0x60, 0x80, 60, 0x40,   /* a quarter later, off */
        0x00, 0x90, 64, 0x60,   /* E */
        0x60, 0x80, 64, 0x40,
        0x00, 0x90, 67, 0x60,   /* G */
        0x60, 0x80, 67, 0x40,
        0x00, 0xff, 0x2f, 0x00  /* end of track */

    };

    f = fopen(fn, "wb");
    if (!f) { printf("could not write %s\n", fn); return; }
    fwrite(smf, 1, sizeof(smf), f);
    fclose(f);

}

/*******************************************************************************

The virtual MIDI loop

ALSA gives a port named "virtual": opening it makes a sequencer client,
and two of them can be subscribed together with aconnect. Opening the
output side and the input side and connecting them puts the library's
own MIDI encoder on one end of a wire and its decoder on the other, so
what is sent with the API can be read back with rdsynth and compared --
the round trip through real MIDI bytes, no hardware and no hands.

The clients are found by listing before and after the opens: the ones
that appear are ours, in the order they were opened.

*******************************************************************************/

#ifdef __MACH__

/* The wire of the loop test on macOS, which has no aconnect: a CoreMIDI
   input port on the virtual output (a source), whose every packet is sent
   on to the virtual input (a destination). The pair are found by the names
   the sound module gives them. */

#include <CoreMIDI/CoreMIDI.h>

static MIDIClientRef   wirecli;
static MIDIPortRef     wirein, wireout;
static MIDIEndpointRef wiresrc, wiredst;

static void wirecb(const MIDIPacketList* pl, void* a, void* b)

{

    (void)a; (void)b;
    MIDISend(wireout, wiredst, pl);

}

static int wirenamed(MIDIEndpointRef ep, const char* name)

{

    CFStringRef cf = NULL;
    char        nm[200] = "";

    MIDIObjectGetStringProperty(ep, kMIDIPropertyName, &cf);
    if (cf) {

        CFStringGetCString(cf, nm, sizeof(nm), kCFStringEncodingUTF8);
        CFRelease(cf);

    }

    return (!strcmp(nm, name));

}

static void loopbefore(void) { }

/* connect the open virtual output to the open virtual input; returns
   null, or why not */
static const char* loopwire(void)

{

    ItemCount i;

    wiresrc = wiredst = 0;
    for (i = 0; i < MIDIGetNumberOfSources(); i++)
        if (wirenamed(MIDIGetSource(i), "PetitAmi virtual out"))
            wiresrc = MIDIGetSource(i);
    for (i = 0; i < MIDIGetNumberOfDestinations(); i++)
        if (wirenamed(MIDIGetDestination(i), "PetitAmi virtual in"))
            wiredst = MIDIGetDestination(i);
    if (!wiresrc || !wiredst)
        return ("could not find the virtual endpoints");
    if (MIDIClientCreate(CFSTR("sound_test wire"), NULL, NULL, &wirecli) ||
        MIDIInputPortCreate(wirecli, CFSTR("wire in"), wirecb, NULL, &wirein) ||
        MIDIOutputPortCreate(wirecli, CFSTR("wire out"), &wireout) ||
        MIDIPortConnectSource(wirein, wiresrc, NULL))
        return ("CoreMIDI would not connect the pair");

    return (NULL);

}

static void loopunwire(void)

{

    if (wirecli) MIDIClientDispose(wirecli);
    wirecli = 0;

}

#else

/* the library's own stdio header does not carry these */
extern FILE* popen(const char* cmd, const char* mode);
extern int   pclose(FILE* f);

static int virtclients(int* cl, int max)

{

    FILE* p;
    char  ln[300];
    int   n = 0;
    int   c = -1;

    p = popen("aconnect -l", "r");
    if (!p) return (-1);
    while (fgets(ln, sizeof(ln), p)) {

        if (!strncmp(ln, "client ", 7)) c = atoi(ln+7);
        else if (strstr(ln, "Virtual RawMIDI") && c >= 0 && n < max)
            cl[n++] = c;

    }
    pclose(p);

    return (n);

}

static int loopnb, loopbf[20];

static void loopbefore(void) { loopnb = virtclients(loopbf, 20); }

/* connect the open virtual output to the open virtual input; returns
   null, or why not */
static const char* loopwire(void)

{

    int  after[20];
    int  na;
    int  co = -1, ci = -1;
    int  j, k, found;
    char cmd[100];

    na = virtclients(after, 20);
    /* ours are the clients that were not there before, in the order they
       were opened: the output first */
    for (j = 0; j < na; j++) {

        found = FALSE;
        for (k = 0; k < loopnb; k++) if (after[j] == loopbf[k]) found = TRUE;
        if (!found) {

            if (co < 0) co = after[j];
            else if (ci < 0) ci = after[j];

        }

    }
    if (loopnb < 0 || co < 0 || ci < 0)
        return ("could not tell the virtual clients apart");
    sprintf(cmd, "aconnect %d:0 %d:0", co, ci);
    if (system(cmd)) return ("aconnect would not connect the pair");

    return (NULL);

}

static void loopunwire(void) { }

#endif

/* one checked result of the loop test */
static ami_long loopfails;

static void loopchk(const char* what, int ok)

{

    printf("%-40s %s\n", what, ok? "pass": "*** FAIL ***");
    if (!ok) loopfails++;

}

static int loopnear(ami_long a, ami_long b, ami_long tol)

{

    return (labs(a-b) <= tol);

}

/*******************************************************************************

Play random notes

Plays random notes on the current instrument, for a given number of notes, to
the given port.

*******************************************************************************/

void playrand(ami_long port, ami_long notes)

{

    ami_long key;
    ami_long i;

    srand(42);
    for( i = 0; i < notes; i++) {

        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX)-1;
        /* Play a note */
        ami_noteon(port, 0, 1, key, LONG_MAX);
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

void playnote(ami_long port, ami_note n)

{

    ami_noteon(port, 0, 1, n, LONG_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteoff(port, 0, 1, n, 0);
    waittime(SECOND/4);

}


/*******************************************************************************

Play scale

Plays a simple scale with on time.

*******************************************************************************/

void playscale(ami_long port, ami_long t)

{

    ami_noteon(port, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_E+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_F+AMI_OCTAVE_6, LONG_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_F+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, LONG_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_A+AMI_OCTAVE_6, LONG_MAX);
    waittime(t);
    ami_noteoff(port, 0, 1, AMI_NOTE_A+AMI_OCTAVE_6, 0);
    waittime(SECOND/4);
    ami_noteon(port, 0, 1, AMI_NOTE_B+AMI_OCTAVE_6, LONG_MAX);
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
    ami_long      argi = 1;
    ami_long      argcl;

    /* parse user options */
    argcl = argc;
    if (ami_options(&argi, &argcl, argv, opttbl, TRUE)) {

        /* an option that did not parse: it was left among the positionals
           and reported as "Bad test range". The long options take two
           dashes; one dash is a run of single letter options. */
        fprintf(stderr, "Bad option: %s\n", argv[argi]);
        fprintf(stderr, "(the options are given as --name or --name=value)\n");
        exit(1);

    }

    /* the positionals select the test to skip to, and the last to run:
       "sound_test 40" runs from test 40 on, "sound_test 40 40" that test
       alone */
    if (argcl == 2 || argcl == 3) {

        char* ep;
        int   i;

        /* a positional that is not a number is an option in the wrong
           form, "--sin=1" on Windows, where options start with '/': say
           so, rather than "Bad test range" */
        for (i = 0; i < argcl-1; i++) {

            strtol(argv[argi+i], &ep, 10);
            if (ep == argv[argi+i] || *ep) {

                fprintf(stderr, "Not a test number: %s\n", argv[argi+i]);
                fprintf(stderr, "(options on this system are given as "
                                "%cname or %cname=value)\n",
                        ami_optchr(), ami_optchr());
                exit(1);

            }

        }
        tstlo = strtol(argv[argi], NULL, 10);
        if (argcl == 3) tsthi = strtol(argv[argi+1], NULL, 10);
        if (tstlo < 1 || tsthi < tstlo) {

            fprintf(stderr, "Bad test range\n");
            exit(1);

        }
        argcl = 1; /* consumed */

    }

    if (argcl != 1) {

        fprintf(stderr, "Usage: sndtst [options] [first [last]]\n");
        fprintf(stderr, "              first is the test to skip to, last the\n");
        fprintf(stderr, "              last to run (default: to the end)\n");
        fprintf(stderr, "       options: [--port=<port>|--p=<port>]\n");
        fprintf(stderr, "              [--wport=<port>] wave output port\n");
        fprintf(stderr, "              [--iport=<port>] wave input port\n");
        fprintf(stderr, "              [--sin=<port>]   synth input port; the\n");
        fprintf(stderr, "                  section runs only when given, and\n");
        fprintf(stderr, "                  wants keys played on the device\n");
        fprintf(stderr, "              [--nomidi]       skip the synthesizer\n");
        fprintf(stderr, "                  listening tests, straight to the rest\n");
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

    if (nomidi) goto newtests; /* asked to go straight to the later sections */

    if (tst(1)) {

    printf("\n===== Test 1 =====\n\n");
    printf("Run through the entire scale of notes available\n");
    for (n = AMI_NOTE_C+AMI_OCTAVE_1; n <= AMI_NOTE_G+AMI_OCTAVE_11; n++) {

        printf("%lld ", AMI_LONG_CAST(n));
        ami_noteon(dport, 0, 1, n, LONG_MAX);
        waittime(SECOND/10);
        ami_noteoff(dport, 0, 1, n, 0);

    }
    printf("\n");
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(2)) {

    printf("\n===== Test 2 =====\n\n");
    printf("Run through all instruments with middle C\n");
    printf("Note that not all syths implement all instruments\n");
    printf("Instruments: ");
    for (ins = AMI_INST_ACOUSTIC_GRAND; ins <= AMI_INST_GUNSHOT; ins++) {

        printf("%lld ", AMI_LONG_CAST(ins));
        ami_instchange(dport, 0, 1, ins);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        waittime(SECOND/10);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/10);

    }
    printf("\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(3)) {

    printf("\n===== Test 3 =====\n\n");
    printf("Run though all percussive instruments\n");
    printf("Note that not all syths implement all instruments\n");
    printf("Instruments: ");
    for (n = AMI_NOTE_ACOUSTIC_BASS_DRUM; n <= AMI_NOTE_OPEN_TRIANGLE; n++) {

        printf("%lld ", AMI_LONG_CAST(n));
        ami_noteon(dport, 0, 10, n, LONG_MAX);
        waittime(SECOND/10);
        ami_noteoff(dport, 0, 10, n, 0);
        waittime(SECOND/10);

    }
    printf("\n");
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(4)) {

    printf("\n===== Test 4 =====\n\n");
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

    } /* test range gate */

    if (tst(5)) {

    printf("\n===== Test 5 =====\n\n");
    printf("Note volume test\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    for (i = 0; i < 20; i++) {

        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, i*(LONG_MAX/20));
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(6)) {

    printf("\n===== Test 6 =====\n\n");
    printf("Random note programming piano:\n");
    waitret();
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(7)) {

    printf("\n===== Test 7 =====\n\n");
    printf("Random note programming harpsichord:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_HARPSICHORD);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(8)) {

    printf("\n===== Test 8 =====\n\n");
    printf("Random note programming organ:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(9)) {

    printf("\n===== Test 9 =====\n\n");
    printf("Random note programming soprando sax:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_SOPRANO_SAX);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(10)) {

    printf("\n===== Test 10 =====\n\n");
    printf("Random note programming telephone:\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_TELEPHONE_RING);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    /* restore piano */
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);

    /* set attack times */
    } /* test range gate */

    if (tst(11)) {

    printf("\n===== Test 11 =====\n\n");
    printf("Set step attack times on piano\n");
    waitret();
    for (i = 0; i <= 10; i++) {

        printf("Attack: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/10)));
        ami_attack(dport, 0, 1, i*(LONG_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_attack(dport, 0, 1, LONG_MAX/2); /* reset normal */
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(12)) {

    printf("\n===== Test 12 =====\n\n");
    printf("Set step attack times on organ\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    for (i = 0; i <= 10; i++) {

        printf("Attack: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/10)));
        ami_attack(dport, 0, 1, i*(LONG_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_attack(dport, 0, 1, LONG_MAX/2); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set release times */
    } /* test range gate */

    if (tst(13)) {

    printf("\n===== Test 13 =====\n\n");
    printf("Set step release times on piano\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    for (i = 0; i <= 10; i++) {

        printf("Release: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/10)));
        ami_release(dport, 0, 1, i*(LONG_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_release(dport, 0, 1, LONG_MAX/2); /* reset normal */
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(14)) {

    printf("\n===== Test 14 =====\n\n");
    printf("Set step release times on organ\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    for (i = 0; i <= 10; i++) {

        printf("Release: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/10)));
        ami_release(dport, 0, 1, i*(LONG_MAX/10));
        playnote(dport, AMI_NOTE_C+AMI_OCTAVE_6);

    }
    ami_release(dport, 0, 1, LONG_MAX/2); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set legato */
    } /* test range gate */

    if (tst(15)) {

    printf("\n===== Test 15 =====\n\n");
    printf("Set legato on piano, first normal, then legato\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    ami_legato(dport, 0, 1, FALSE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
    /* now repeat with legato on */
    ami_legato(dport, 0, 1, TRUE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
    ami_legato(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(16)) {

    printf("\n===== Test 16 =====\n\n");
    printf("Set legato on organ, first normal, then legato\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_legato(dport, 0, 1, FALSE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
    /* now repeat with legato on */
    ami_legato(dport, 0, 1, TRUE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
    waittime(SECOND/4);
    ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
    waittime(SECOND/4);
    /* turn off both */
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
    ami_legato(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set portamento */
    } /* test range gate */

    if (tst(17)) {

    printf("\n===== Test 17 =====\n\n");
    printf("Set portamento on piano, first normal, then portamento, through\n");
    printf("various portamento times\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    for (i = 0; i < 10; i++) {

        printf("Portamento time: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/10)));
        ami_porttime(dport, 0, 1, i*(LONG_MAX/10));
        ami_portamento(dport, 0, 1, FALSE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        /* now repeat with portamento on */
        ami_portamento(dport, 0, 1, TRUE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);

    }
    ami_portamento(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(18)) {

    printf("\n===== Test 18 =====\n\n");
    printf("Set portamento on organ, first normal, then portamento\n");
    waitret();
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    for (i = 0; i < 10; i++) {

        printf("Portamento time: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/10)));
        ami_portamento(dport, 0, 1, FALSE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        /* now repeat with portamento on */
        ami_portamento(dport, 0, 1, TRUE);
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX); /* play middle C */
        waittime(SECOND/4);
        ami_noteon(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX); /* play D */
        waittime(SECOND/4);
        /* turn off both */
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(dport, 0, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);

    }
    ami_portamento(dport, 0, 1, FALSE); /* reset normal */
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(19)) {

    printf("\n===== Test 19 =====\n\n");
    printf("Channel volume test. Play note continuously while advancing volume\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* advance volume sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Volume: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_volsynthchan(dport, 0, 1, i*(LONG_MAX/20));
        waittime(SECOND/4);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* reset channel vol to midline */
    ami_volsynthchan(dport, 0, 1, LONG_MAX/2);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(20)) {

    printf("\n===== Test 20 =====\n\n");
    printf("Channel balance test. Play note continuously while changing\n");
    printf("from to right\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* advance volume sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Balance: %lld\n", AMI_LONG_CAST((i-10)*(LONG_MAX/10)));
        ami_balance(dport, 0, 1, (i-10)*(LONG_MAX/10));
        waittime(SECOND/4);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* reset channel balance to midline */
    ami_balance(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(21)) {

    printf("\n===== Test 21 =====\n\n");
    printf("Channel vibrato test. Play note continuously while advancing vibrato\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* advance vibrato sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Vibrato: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_vibrato(dport, 0, 1, i*(LONG_MAX/20));
        waittime(SECOND);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* reset channel vibrato to midline */
    ami_vibrato(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(22)) {

    printf("\n===== Test 22 =====\n\n");
    printf("Channel pan test. Play note continuously while changing\n");
    printf("pan from left to right\n");
    ami_instchange(dport, 0, 1, AMI_INST_DRAWBAR_ORGAN);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* advance pan sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Pan: %lld\n", AMI_LONG_CAST((i-10)*(LONG_MAX/10)));
        ami_pan(dport, 0, 1, (i-10)*(LONG_MAX/10));
        waittime(SECOND/4);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    /* reset channel pan to midline */
    ami_pan(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(23)) {

    printf("\n===== Test 23 =====\n\n");
    printf("Channel timbre test. Play notes while advancing timbre\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance timbre sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Timbre: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_timbre(dport, 0, 1, i*(LONG_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel timbre */
    ami_timbre(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(24)) {

    printf("\n===== Test 24 =====\n\n");
    printf("Channel brightness test. Play notes while advancing brightness\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance brightness sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Brightness: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_brightness(dport, 0, 1, i*(LONG_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel brightness */
    ami_brightness(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(25)) {

    printf("\n===== Test 25 =====\n\n");
    printf("Channel reverb test. Play notes while advancing reverb\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance reverb sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Reverb: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_reverb(dport, 0, 1, i*(LONG_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel reverb */
    ami_reverb(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(26)) {

    printf("\n===== Test 26 =====\n\n");
    printf("Channel tremulo test. Play notes while advancing tremulo\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);

    /* advance tremulo sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Tremulo: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_tremulo(dport, 0, 1, i*(LONG_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel tremulo */
    ami_tremulo(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(27)) {

    printf("\n===== Test 27 =====\n\n");
    printf("Channel chorus test. Play notes while advancing chorus\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);

    /* advance chorus sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Chorus: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_chorus(dport, 0, 1, i*(LONG_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel chorus */
    ami_chorus(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(28)) {

    printf("\n===== Test 28 =====\n\n");
    printf("Channel celeste test. Play notes while advancing celeste\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance celeste sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Celeste: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_celeste(dport, 0, 1, i*(LONG_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        waittime(SECOND/4);
        ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
        waittime(SECOND/4);

    }
    /* reset channel celeste */
    ami_celeste(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(29)) {

    printf("\n===== Test 29 =====\n\n");
    printf("Channel phaser test. Play notes while advancing phaser\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    /* advance phaser sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Phaser: %lld\n", AMI_LONG_CAST(i*(LONG_MAX/20)));
        ami_phaser(dport, 0, 1, i*(LONG_MAX/20));
        ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
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
    } /* test range gate */

    if (tst(30)) {

    printf("\n===== Test 30 =====\n\n");
    printf("pitch wheel. Vary pitch wheel while playing continuously\n");
    ami_instchange(dport, 0, 1, AMI_INST_LEAD_1_SQUARE);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    for (j = 0; j < 10; j++) {

        printf("Pitchrange: %lld\n", AMI_LONG_CAST(j*(LONG_MAX/10)));
        ami_pitchrange(dport, 0, 1, j*(LONG_MAX/10));
        for (x = 0; x < 10; x++)
            for (i = 0; i < 10; i++) {

            printf("Pitch: %lld\n", AMI_LONG_CAST((i-5)*(LONG_MAX/5)));
            ami_pitch(dport, 0, 1, (i-5)*(LONG_MAX/5));
            waittime(SECOND/100);

        }

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    ami_pitch(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

newtests:

    /***************************************************************************

    DEVICES AND PARAMETERS

    ***************************************************************************/

    if (tst(31)) {

    printf("\n===== Test 31 =====\n\n");
    printf("Device inventory. Every port of every kind, by name.\n");
    {

        char nm[200];

        printf("Output synthesizers: %lld\n", AMI_LONG_CAST(ami_synthout()));
        for (i = 1; i <= ami_synthout(); i++) {

            ami_synthoutname(i, nm, sizeof(nm));
            printf("    %2lld: %s\n", AMI_LONG_CAST((ami_long)i), nm);

        }
        printf("Input synthesizers: %lld\n", AMI_LONG_CAST(ami_synthin()));
        for (i = 1; i <= ami_synthin(); i++) {

            ami_synthinname(i, nm, sizeof(nm));
            printf("    %2lld: %s\n", AMI_LONG_CAST((ami_long)i), nm);

        }
        printf("Output wave devices: %lld\n", AMI_LONG_CAST(ami_waveout()));
        for (i = 1; i <= ami_waveout(); i++) {

            ami_waveoutname(i, nm, sizeof(nm));
            printf("    %2lld: %s\n", AMI_LONG_CAST((ami_long)i), nm);

        }
        printf("Input wave devices: %lld\n", AMI_LONG_CAST(ami_wavein()));
        for (i = 1; i <= ami_wavein(); i++) {

            ami_waveinname(i, nm, sizeof(nm));
            printf("    %2lld: %s\n", AMI_LONG_CAST((ami_long)i), nm);

        }

    }
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(32)) {

    printf("\n===== Test 32 =====\n\n");
    printf("Parameters, get and set, all four directions. ALSA devices\n");
    printf("carry none, so empty values and a declined set are correct\n");
    printf("here; a plug-in with parameters would show them.\n");
    {

        char val[200];

        ami_getparamsynthout(dport, "name", val, sizeof(val));
        printf("getparamsynthout(name): \"%s\"\n", val);
        printf("setparamsynthout(name): %lld\n",
               AMI_LONG_CAST(ami_setparamsynthout(dport, "name", "value")));
        if (ami_synthin() > 0) {

            ami_getparamsynthin(1, "name", val, sizeof(val));
            printf("getparamsynthin(name):  \"%s\"\n", val);
            printf("setparamsynthin(name):  %lld\n",
                   AMI_LONG_CAST(ami_setparamsynthin(1, "name", "value")));

        }
        ami_getparamwaveout(wport, "name", val, sizeof(val));
        printf("getparamwaveout(name):  \"%s\"\n", val);
        printf("setparamwaveout(name):  %lld\n",
               AMI_LONG_CAST(ami_setparamwaveout(wport, "name", "value")));
        if (ami_wavein() > 0) {

            ami_getparamwavein(iport, "name", val, sizeof(val));
            printf("getparamwavein(name):   \"%s\"\n", val);
            printf("setparamwavein(name):   %lld\n",
                   AMI_LONG_CAST(ami_setparamwavein(iport, "name", "value")));

        }

    }
    printf("Complete\n");
    waitret();

    /***************************************************************************

    THE SEQUENCER PROPER

    ***************************************************************************/

    } /* test range gate */

    if (tst(33)) {

    printf("\n===== Test 33 =====\n\n");
    printf("Sequenced playback. Every test so far played at time zero;\n");
    printf("this queues a scale with future timestamps in one burst, and\n");
    printf("the sequencer plays it in tempo. The queueing returns at\n");
    printf("once, which the elapsed times printed show.\n");
    ami_instchange(dport, 0, 1, AMI_INST_ACOUSTIC_GRAND);
    ami_starttimeout();
    printf("time before queueing: %lld\n", AMI_LONG_CAST(ami_curtimeout()));
    {

        static const ami_note sc[8] = {

            AMI_NOTE_C+AMI_OCTAVE_6, AMI_NOTE_D+AMI_OCTAVE_6,
            AMI_NOTE_E+AMI_OCTAVE_6, AMI_NOTE_F+AMI_OCTAVE_6,
            AMI_NOTE_G+AMI_OCTAVE_6, AMI_NOTE_A+AMI_OCTAVE_6,
            AMI_NOTE_B+AMI_OCTAVE_6, AMI_NOTE_C+AMI_OCTAVE_7

        };

        for (i = 0; i < 8; i++) {

            ami_noteon(dport, (i+1)*(SECOND/3), 1, sc[i], LONG_MAX);
            ami_noteoff(dport, (i+1)*(SECOND/3)+SECOND/4, 1, sc[i], 0);

        }

    }
    printf("time after queueing:  %lld (all eight notes are queued)\n",
           AMI_LONG_CAST(ami_curtimeout()));
    waittime(SECOND*3+SECOND/2);
    printf("time after playing:   %lld\n", AMI_LONG_CAST(ami_curtimeout()));
    ami_stoptimeout();
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(34)) {

    printf("\n===== Test 34 =====\n\n");
    printf("wrsynth: the same notes as sequencer records, through the\n");
    printf("record interface the plug-ins speak. Three notes.\n");
    {

        ami_seqmsg sm;

        for (i = 0; i < 3; i++) {

            memset(&sm, 0, sizeof(sm));
            sm.port = dport;
            sm.time = 0;
            sm.st = st_noteon;
            sm.ntc = 1;
            sm.ntn = AMI_NOTE_C+AMI_OCTAVE_6+i*4;
            sm.ntv = LONG_MAX;
            ami_wrsynth(dport, &sm);
            waittime(SECOND/3);
            sm.st = st_noteoff;
            sm.ntv = 0;
            ami_wrsynth(dport, &sm);
            waittime(SECOND/6);

        }

    }
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(35)) {

    printf("\n===== Test 35 =====\n\n");
    printf("Aftertouch and channel pressure. A note is held while each\n");
    printf("sweeps up. What is heard depends on the synthesizer: the\n");
    printf("soundfont default routes channel pressure to vibrato, so on\n");
    printf("fluidsynth the note should take on a deepening vibrato in\n");
    printf("the second half; a synth may also ignore both.\n");
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    for (i = 0; i < 10; i++) {

        ami_aftertouch(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, i*(LONG_MAX/10));
        waittime(SECOND/10);

    }
    for (i = 0; i < 10; i++) {

        ami_pressure(dport, 0, 1, i*(LONG_MAX/10));
        waittime(SECOND/10);

    }
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    /* Put both back, the way every section leaves what it moved. The
       pressure is not cosmetic: the soundfont default modulator turns
       standing channel pressure into vibrato, and left at ninety
       percent it put a wobble on every note of every test after. */
    ami_aftertouch(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    ami_pressure(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(36)) {

    printf("\n===== Test 36 =====\n\n");
    printf("Mono and poly. In mono a second note silences the first; in\n");
    printf("poly they sound together. The same pair is played each way.\n");
    ami_mono(dport, 0, 1, 1);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    waittime(SECOND/2);
    ami_noteon(dport, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, LONG_MAX);
    waittime(SECOND/2);
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    ami_noteoff(dport, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, 0);
    ami_poly(dport, 0, 1);
    waittime(SECOND/2);
    ami_noteon(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
    waittime(SECOND/2);
    ami_noteon(dport, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, LONG_MAX);
    waittime(SECOND/2);
    ami_noteoff(dport, 0, 1, AMI_NOTE_C+AMI_OCTAVE_6, 0);
    ami_noteoff(dport, 0, 1, AMI_NOTE_G+AMI_OCTAVE_6, 0);
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(37)) {

    printf("\n===== Test 37 =====\n\n");
    printf("A MIDI file: made here, loaded, played, deleted. You should\n");
    printf("hear C, E, G on the piano, a quarter note each.\n");
    makemid("sound_test.mid");
    ami_loadsynth(1, "sound_test.mid");
    ami_playsynth(dport, 0, 1);
    ami_waitsynth(dport);
    ami_delsynth(1);
    remove("sound_test.mid");
    printf("Complete\n");
    waitret();

    /***************************************************************************

    WAVE OUTPUT

    ***************************************************************************/

    } /* test range gate */

    if (tst(38)) {

    printf("\n===== Test 38 =====\n\n");
    printf("Wave output, synthesized: two seconds of A 440 written\n");
    printf("straight to the device, the way genwave does.\n");
    ami_openwaveout(wport);
    ami_chanwaveout(wport, 1);
    ami_ratewaveout(wport, TESTRATE);
    ami_lenwaveout(wport, 16);
    ami_sgnwaveout(wport, TRUE);
    ami_endwaveout(wport, FALSE);
    ami_fltwaveout(wport, FALSE);
    {

        static short buf[TESTRATE/10]; /* a tenth of a second a write */
        ami_long j;

        for (i = 0; i < 20; i++) {

            for (j = 0; j < TESTRATE/10; j++)
                buf[j] = (short)(sin(2.0*3.14159265*440.0*
                                     (i*(TESTRATE/10)+j)/TESTRATE)*20000.0);
            /* the length is in samples, as genwave and connectwave pass
               it, not in bytes */
            ami_wrwave(wport, (byte*)buf, TESTRATE/10);

        }

    }
    ami_closewaveout(wport); /* each test opens and closes its own port */
    printf("Complete\n");
    waitret();

    } /* test range gate */

    if (tst(39)) {

    printf("\n===== Test 39 =====\n\n");
    printf("A wave file: made here, loaded, played, deleted. You should\n");
    printf("hear a C, a bit above the tone before, for a second and a\n");
    printf("half. volwave is exercised on the way; it is a stub in this\n");
    printf("implementation, so it changes nothing audible yet.\n");
    makewav("sound_test.wav", 523.25, 1.5);
    ami_openwaveout(wport); /* so the test runs on its own, "sound_test 39" */
    ami_loadwave(1, "sound_test.wav");
    ami_volwave(wport, 0, LONG_MAX/2);
    ami_playwave(wport, 0, 1);
    ami_waitwave(wport);
    ami_delwave(1);
    remove("sound_test.wav");
    ami_closewaveout(wport);
    printf("Complete\n");
    waitret();

    /***************************************************************************

    WAVE INPUT

    ***************************************************************************/

    } /* test range gate */

    if (tst(40)) {

    printf("\n===== Test 40 =====\n\n");
    if (ami_wavein() > 0) {

        ami_long ch, ra, lb, sg, en, fl;
        ami_long bps, total, got;
        byte* rec;

        printf("Wave input. Three seconds are recorded from input port %lld\n",
               AMI_LONG_CAST(iport));
        printf("-- make some noise -- and played back to you.\n");
        waitret();
        ami_openwavein(iport);
        ch = ami_chanwavein(iport);
        ra = ami_ratewavein(iport);
        lb = ami_lenwavein(iport);
        sg = ami_sgnwavein(iport);
        en = ami_endwavein(iport);
        fl = ami_fltwavein(iport);
        printf("The device delivers: %lld channels at %lld hertz, %lld bits,\n",
               AMI_LONG_CAST(ch), AMI_LONG_CAST(ra), AMI_LONG_CAST(lb));
        printf("%s, %s endian, %s\n", sg? "signed": "unsigned",
               en? "big": "little", fl? "float": "integer");
        /* lengths to rdwave and wrwave are in samples -- one frame of
           every channel -- as connectwave passes them, not in bytes */
        bps = ch*(lb/8);
        total = ra*3;
        rec = malloc(total*bps);
        if (rec) {

            printf("Recording...\n");
            got = 0;
            while (got < total)
                got += ami_rdwave(iport, rec+got*bps, total-got);
            ami_closewavein(iport);
            printf("Playing back...\n");
            ami_openwaveout(wport);
            ami_chanwaveout(wport, ch);
            ami_ratewaveout(wport, ra);
            ami_lenwaveout(wport, lb);
            ami_sgnwaveout(wport, sg);
            ami_endwaveout(wport, en);
            ami_fltwaveout(wport, fl);
            ami_wrwave(wport, rec, total);
            ami_closewaveout(wport);
            free(rec);

        } else {

            printf("no memory for the recording\n");
            ami_closewavein(iport);

        }
        printf("Complete\n");
        waitret();

    } else printf("No wave input device: recording not tested.\n");

    /***************************************************************************

    SYNTHESIZER INPUT, AUTOMATED: THE VIRTUAL LOOP

    ***************************************************************************/

    } /* test range gate */

    if (tst(41)) {

    printf("\n===== Test 41 =====\n\n");
    printf("The virtual MIDI loop: the encoder sent down a wire to the\n");
    printf("decoder, and every message checked against what was sent.\n");
    {

        ami_long vout = 0, vin = 0;
        char nm[200];

        for (i = 1; i <= ami_synthout() && !vout; i++) {

            ami_synthoutname(i, nm, sizeof(nm));
            if (!strcmp(nm, "virtual")) vout = i;

        }
        for (i = 1; i <= ami_synthin() && !vin; i++) {

            ami_synthinname(i, nm, sizeof(nm));
            if (!strcmp(nm, "virtual")) vin = i;

        }
        if (vout && vin) {

            const char* why;

            loopbefore();
            ami_opensynthout(vout);
            ami_opensynthin(vin);
            why = loopwire();
            if (!why) {

                {

                    ami_seqmsg sm;

                    loopfails = 0;
                    /* one of each kind down the wire ... */
                    ami_noteon(vout, 0, 1, 61, LONG_MAX);
                    ami_noteoff(vout, 0, 1, 61, 0);
                    ami_instchange(vout, 0, 2, 42);
                    ami_pressure(vout, 0, 3, LONG_MAX/2);
                    ami_pitch(vout, 0, 4, LONG_MAX/2);
                    /* ... and back, in order. The values scale to seven
                       (or for pitch, fourteen) bits on the wire, so what
                       returns is what was sent to within one wire step. */
                    ami_rdsynth(vin, &sm);
                    loopchk("loop note on", sm.st == st_noteon &&
                            sm.ntc == 1 && sm.ntn == 61 &&
                            loopnear(sm.ntv, LONG_MAX, LONG_MAX/64));
                    ami_rdsynth(vin, &sm);
                    loopchk("loop note off", sm.st == st_noteoff &&
                            sm.ntc == 1 && sm.ntn == 61 && sm.ntv == 0);
                    ami_rdsynth(vin, &sm);
                    loopchk("loop instrument change", sm.st == st_instchange &&
                            sm.icc == 2 && sm.ici == 42);
                    ami_rdsynth(vin, &sm);
                    loopchk("loop pressure", sm.st == st_pressure &&
                            sm.ntc == 3 &&
                            loopnear(sm.ntv, LONG_MAX/2, LONG_MAX/64));
                    ami_rdsynth(vin, &sm);
                    loopchk("loop pitch bend", sm.st == st_pitch &&
                            sm.vsc == 4 &&
                            loopnear(sm.vsv, LONG_MAX/2, LONG_MAX/4096));
                    printf("Loop test: %s\n", loopfails?
                           "*** FAILS ***": "all pass");

                }

            } else printf("%s: not tested\n", why);
            loopunwire();
            ami_closesynthin(vin);
            ami_closesynthout(vout);

        } else printf("no virtual MIDI port: not tested\n");

    }
    printf("Complete\n");
    waitret();

    /***************************************************************************

    SYNTHESIZER INPUT

    ***************************************************************************/

    } /* test range gate */

    if (tst(42)) {

    printf("\n===== Test 42 =====\n\n");
    if (sport > 0) {

        ami_seqmsg sm;

        printf("Synthesizer input from port %lld. Play eight notes on the\n",
               AMI_LONG_CAST(sport));
        printf("keyboard; each is echoed to the output synthesizer and\n");
        printf("printed with the time it arrived.\n");
        ami_opensynthin(sport);
        ami_starttimein();
        for (i = 0; i < 8; ) {

            ami_rdsynth(sport, &sm);
            if (sm.st == st_noteon) {

                printf("note on:  note %3lld velocity %lld time %lld\n",
                       AMI_LONG_CAST((ami_long)sm.ntn), AMI_LONG_CAST(sm.ntv), AMI_LONG_CAST(ami_curtimein()));
                ami_noteon(dport, 0, 1, sm.ntn, sm.ntv);
                i++;

            } else if (sm.st == st_noteoff) {

                printf("note off: note %3lld time %lld\n", AMI_LONG_CAST((ami_long)sm.ntn),
                       AMI_LONG_CAST(ami_curtimein()));
                ami_noteoff(dport, 0, 1, sm.ntn, sm.ntv);

            } else printf("other message: type %d time %lld\n", sm.st,
                          AMI_LONG_CAST(ami_curtimein()));

        }
        ami_stoptimein();
        ami_closesynthin(sport);
        printf("Complete\n");
        waitret();

    } else printf("Synthesizer input not tested: give --sin=<port> and play "
                  "the keys.\n");

    } /* test range gate */

terminate: /* terminate */
    ami_closesynthout(dport);
    printf("\n");

    return (0); /* return no error */

}
