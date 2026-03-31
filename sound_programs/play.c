/*******************************************************************************

Player example file

Implements a Qbasic compatible "play" statement, and feeds it a sample song.
The theory here is that using an existing music notation will give us lots
of test material.

This website gives and overview of the notation. I don't think we implement
all of it.

https://www.qbasic.net/en/reference/qb11/Statement/PLAY-006.htm

*******************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#include <terminal.h> /* terminal level API */
#include <sound.h>    /* sound API */
#include <option.h> /* options parser */

#define SECOND 10000

int dport = PA_SYNTH_OUT; /* set default synth out */

ami_optrec opttbl[] = {

    { "port",  NULL, &dport,  NULL, NULL },
    { "p",     NULL, &dport,  NULL, NULL },
    { NULL,    NULL, NULL,    NULL, NULL }

};

int     ntime;  /* normal beat time in quarter notes */
ami_note octave; /* current octave */
int     deftim; /* default note time */
int     i;

static void waittime(int t)

{

    ami_evtrec er; /* event record */

    ami_timer(stdout, 1, t, FALSE);
    do { ami_event(stdin, &er); } while (er.etype != ami_ettim && er.etype != ami_etterm);
    if (er.etype == ami_etterm) exit(0);

}

static void playnote(ami_note n, int nt)

{

/*printf("Note: %d Time: %d\n", n, nt);*/
   ami_noteon(dport, 0, 1, n, INT_MAX); /* turn on the note */
   waittime(nt); /* wait time */
   ami_noteoff(dport, 0, 1, n, INT_MAX); /* turn off the note */

}

static void settim(int* t, int ln)

{

    while (ln > 4) { /* process tempo levels */

        *t = *t/2;
        ln = ln/2;

    }
    if (ln == 1) *t = *t*4; /* set whole note */
    else if (ln == 2) *t = *t*2; /* 1/2 note */

}

static void setoct(int on)

{

    if (on < 0 || on > 6) { /* bad octave */

        printf("*** Play: bad octave number\n");
        exit(1);

    }
    /* we place Plays' 6 7 octaves in the middle of midis' 11 octaves */
    switch (on) { /* octave */

        case 0: octave = PA_OCTAVE_2; break;
        case 1: octave = PA_OCTAVE_3; break;
        case 2: octave = PA_OCTAVE_4; break;
        case 3: octave = PA_OCTAVE_5; break;
        case 4: octave = PA_OCTAVE_6; break;
        case 5: octave = PA_OCTAVE_7; break;
        case 6: octave = PA_OCTAVE_8; break;

    }

}

/* Qbasic compatible "play" string command */

static void play(string ms)

{

    ami_note n;
    int     nt;
    int     x;
    int     i;

    while (*ms) { /* interpret commands*/

        /* process single notes */
        if (tolower(*ms) >= 'a' && tolower(*ms) <= 'g') {

            printf("%c ", *ms);
            /* its a note */
            switch (tolower(*ms)) { /* note */

                case 'c': n = PA_NOTE_C; break;
                case 'd': n = PA_NOTE_D; break;
                case 'e': n = PA_NOTE_E; break;
                case 'f': n = PA_NOTE_F; break;
                case 'g': n = PA_NOTE_G; break;
                case 'a': n = PA_NOTE_A; break;
                case 'b': n = PA_NOTE_B;  break;

            }
            ms++; /* next character */
            if (*ms == '+' || *ms == '#') {

                n++; /* sharpen it */
                ms++;

            } else if (*ms == '-') {

                n--; /* flatten it */
                ms++;

            }
            nt = deftim; /* set default whole note */
            /* check length follows, and set time from that */
            if (*ms >= '0' && *ms <= '9') {

                nt = ntime; /* reset to main time */
                settim(&nt, strtol(ms, &ms, 10));

            }
            if (*ms == '.') { /* dotted length */

                nt = nt+nt/2; /* extend by 1/2 */
                ms++;

            }
            /* note is fully prepared, send it */
            playnote(n+octave, nt);

        } else if (tolower(*ms) == 'o') { /* set octave */

            ms++; /* advance */
            setoct(strtol(ms, &ms, 10)); /* set octave */

        } else if (tolower(*ms) == 'l') { /* set note lengths */

            ms++; /* advance */
            deftim = ntime; /* reset default */
            settim(&deftim, strtol(ms, &ms, 10)); /* set default time */

        } else if (*ms == '>') { /* up octave */

            if (octave < PA_OCTAVE_8) octave = octave+12;
            ms++;

        } else if (*ms == '<') { /* down octave */

            if (octave > PA_OCTAVE_2) octave = octave-12;
            ms++;

        } else if (tolower(*ms) == 'n') { /* numbered note */

            ms++; /* advance */
            i = strtol(ms, &ms, 10); /* get the note */
            if (i < 0 && i > 84) {

                printf("\n*** Play: Invalid note number\n");
                exit(1);

            }
            if (i == 0) waittime(ntime); /* rest */
            else playnote(i-1+PA_OCTAVE_2, deftim); /* play note */

        } else if (tolower(*ms) == 'p') { /* pause */

            ms++; /* advance */
            settim(&x, strtol(ms, &ms, 10)); /* get time */
            waittime(x); /* wait for that time */

        } else if (tolower(*ms) == 't') { /* tempo */

            /* not implemented, just skip */
            ms++; /* advance */
            x = strtol(ms, &ms, 10); /* get time */

        } else if (tolower(*ms) == 'm') { /* various commands */

            ms++; /* advance */
            if (tolower(*ms) != 'n' && tolower(*ms) != 'l' &&
                tolower(*ms) != 's' && tolower(*ms) != 'f' &&
                tolower(*ms) != 'b') {

                printf("\n*** Play: command syntax error\n");
                exit(1);

            }
            ms++; /* advance */

        } else if (*ms == ' ') ms++; /* skip spaces */
        else {

            printf("\n*** Play: command syntax error\n");
            exit(1);

        }

    }

}

int main(int argc, char **argv)

{

    int song;
    int argi = 1;
    char buff[20];

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 1) {

        fprintf(stderr, "Usage: play [--port=<port>|-p=<port>]\n");
        exit(1);

    }

    /* set up "play" parameters */
    ntime = SECOND/2; /* set default tempo for quarter notes */
    octave = PA_OCTAVE_5; /* start in middle octave */
    deftim = ntime; /* set whole notes default */

    printf("Synthesisers: %d\n", ami_synthout());
    ami_opensynthout(dport); /* open main synthesiser */

    ami_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);

    printf("1: Mozart's Sonata in C\n");
    printf("2: Stars And Stripes Forever\n");
    printf("3: Da Da Dida Dida\n");
    printf("4: Revilie\n");
    printf("5: Taps\n");
    printf("6: Beethoven's Fifth\n");
    printf("7: Football song\n");
    printf("8: Anchors Away\n");
    printf("9: America The Beautiful\n");
    printf("10: Battle Hymn of the Republic\n");
    printf("11: The Cassions Go Rolling Along\n");
    printf("12: Star Spangled Banner\n");
    printf("13: Dixie\n");
    printf("14: The Odd Couple\n");
    printf("15: Star Wars\n");
    printf("16: Alex's Theme (Beverly Hills Cop)\n");
    printf("17: Bagpipes\n");
    printf("18: Theme from Star Trek\n");

    printf("\n");
    printf("Enter song to play: ");
    fgets(buff, 20, stdin);
    sscanf(buff, "%d", &song);

    if (--song == 0) {

    printf("Mozart's Sonata in C\n");
    play("c2 l4 e g < b. > l16 c d l2 c");
    play("> a l4 g > c < g l16 g f e f l2 e");
    play("< a8 l16 b > c d e f g a g f e d c < b a");
    play("g8 a b > c d e f g f e d c < b a g f8 g a b > c d e");
    play("f e d c < b a g f e8 f g a b > c d e d c < b a g f e");
    play("d8 e f g a b > c# d < a b > c# d e f g");
    play("a b > c < b a g f e f g a g f e d c");
    play("< l8 b ms > g e c ml d g ms e c");
    play("d4 g4 < g2 g2 > c4 e4 g2");
    play("l16 a g f e f e d c e d e d e d e d e d e d e d c d");
    play("c4 c < g > c e g e c e f d < b > d");
    play("c4 < c < g > c e g e c e f d < b > d c4 > c4 c2");
    printf("\n");

    } else if (--song == 0) {

    printf("Stars And Stripes Forever\n");
    play("MN T240");
    play("O1 C2 O0 B4. O1 C8 O0 A4 O1 C2 D4 E-4 E4 F4 F#4 G4 P4 O0G2");
    play("O3 E4 P8 E8 E4 E4 E4 P8 E8 F4 F4 D8 C#8 MS D8 E8 D4 D4");
    play("O3 ML C32 O2 B16. MS B8 A4 B4 G4 MNO3G4 P8 G8 G4 G4");
    play("O3 G4 P8 G8 G#4 G#4 ML A8 MN G#8 MS A8 O4C8O3 MNB4 A4 G#2.");
    play("O3 G#4 G4 P8 G8 MS O4 C4 P8 O3 A8 G4 F#4 G4 E4 MN D4");
    play("O2 G8 F#8 G4 O3 D4 O2 G8 F#8 G4 O3");
    play("O3 G4 P8 G8 MS O4 C4 P8 O3 A8 G4 F#4 G4 E-4 MN D4");
    play("O2 F#8 E8 F#8 E8 F#4 G4 P4 G4 .F8 E2 A4.G8 O1 B2 A2");
    play("O1 G2 O2 F2 E2 D4.E8 F4 A2");
    play("O3 C4 D2 C2");
    play("O2 E1 D2");
    play("O3 G4.F8 E2 A4. G8 O2 B2 A2 G2 O3F2 E2 D4.E8 F4 A4 O4 D4.C8");
    play("O3 E4 G4 C4.E8 D1 C1");

    } else if (--song == 0) {

    printf("Da Da Dida Dida\n");
    play("MN T200");
    play("O2 C8 F8 A8 O3 C4 O2A8 O3C4.");
    play("O2 C#8 F#8 A#8 O3 C#4 O2A#8 O3C#4.");
    play("O2 D8 G8 B8 O3 D4 O2B8 O3D4.");

    } else if (--song == 0) {

    printf("Revilie\n");
    play("MB MN T180");
    play("O2 C8.C16 F8 C8 F8 A8 F4 F8.F16 A8 F8 A8 O3C8 O2 A4 F8.A16 O3 C4");
    play("O2 A8.F16 C4 C8.C16 F4 F8.F16 F4 MF");

    } else if (--song == 0) {

    printf("Taps\n");
    play("T120 MN MB");
    play("O3L8C.L16C L2F.L8C.L16F");
    play("L2A.L8C.L16F L4A L8C L16F L4A L8C L16F L2A.");
    play("O3 L8F.L16A ML O4L2C MN O3L4A L4F L2C.");
    play("O3L8C.L16C ML L1F MN L4F");

    } else if (--song == 0) {

    printf("Beethoven's Fifth\n");
    play("T180 o2 P2 P8 L8 GGG L2 E-");
    play("P24 P8 L8 FFF L2 D");

    } else if (--song == 0) {

    printf("Football song\n");
    play("MB MN T220 O3");
    play("O3 E2 D#4 E4 F8 F4 E8 F2");
    play("O3 F8 F4 F8 E4 F4 G8 G4 F#8 G2");
    play("O3 A4 O4 C4 O3 B4 A4 G4 E4 C4 D4");
    play("O3 E8 G4 F8 E4 D4 C2. MF");

    } else if (--song == 0) {

    printf("Anchors Away\n");
    play("MB T200O3L4");
    play("MB MLCCMBMNEGMLAL8AMNL8EL4L2AO4CL4DO3GO4L1C");

    } else if (--song == 0) {

    printf("America The Beautiful\n");
    play("T120 O3");
    play("G4 G4. E8 E4 G4 G4. D8 D4 E4 F4 G4 A4 B4 G2.");
    play("G4 G4. E8 E4 G4 G4. D8 D4 >D4 C+4 D4 E4 D2.");
    play("E4. E8 D4 C4 C4. < B8 B4 >C4 D4 < B4 A4 G4 >C2.");
    play("C4 C4. C4 C4. C4 D4 C2.");

    } else if (--song == 0) {

    printf("Battle Hymn of the Republic\n");
    play("T120 O3 L8");
    play("F. F. F16 F. E16 D. F16 B-. >C16 D. D16 D. C16 < B-4");
    play("B-. A16 G. G16 G. A16 B-. A16 B-. G16 F. G16 F. D16 F4");
    play("F. F16 F. F16 F. E16 D. F16 B-. >C16 D. D16 D. C16 < B-4");
    play("B-4 >C4 C4 < B-4 A4 B-2 P2");
    play("F4. E D. F16 B-. >C16 D2 < B-4 P4 G4. A B-. A16 B-. G16 F2 D4 P4");
    play("F4. E D. F16 B-. >C16 D2 < B-4 B-4 >C4 C4 < B-4 A4 B-2");

    } else if (--song == 0) {

    printf("The Cassions Go Rolling Along\n");
    play("T145 O2 L8");
    play("G E G4 G E G4 G E G A G E G4");
    play("E F G F4 D G F4 D C2. P8");
    play("G E G4 G E G4 G E G A G E G4");
    play("E F G F4 D G F4 D C2. P8");
    play("G G >C P8 C P");
    play(">C C C4 < < B A B >C D2. P8");
    play("C4 C4 < B2 A B >C");
    play("E F G F4 D G F4 D C2");

    } else if (--song == 0) {

    printf("Star Spangled Banner\n");
    play("T120 O2 L4");
    play("F8 D8 < B- >D F B-2 >D8 C8 < B- D E F2 F8 F8 >D. C8 < B-");
    play("A2 G8 A8 B- B- F D < B- >F8 D8 < B- >D F B-2 >D8 C8");
    play("< B- D E F2 F8 F8 >D. C8 < B- A2 G8 A8 B- B- F D < B-");
    play(">>D8 D8 D D E- F2. E-8 D8 C C D E-2 E- D2 C8 < B-8");
    play("A2 G8 A8 B- D E F2 F B- B- B-8 A8 G G G");
    play(">C E-8 D8 C8 < B-8 B- A2 P4");
    play("F8 F8 B-. >C8 D8 E-8 F2 < B-8 >C8 D. E-8 C < B-2");

    } else if (--song == 0) {

    printf("Dixie\n");
    play("MBT140O3E8C8C8C16D16E16F16G8G8G8E8A8A8A8.G16A8.G16");
    play("A16B18O4C16D16E4.C16O3G16O4C4.O3G16E16G4.D16E16C4.");
    play("MBT140O3E8C8C8C16D16E16F16G8G8G8E8A8A8A8.G16A8.G16");
    play("A16B18O4C16D16E4.C16O3G16O4C4.O3G16E16G4.D16E16C4.");
    play("MBT1408O4C8E8D8C8O3A8O4C4O3A8O4D4.O3A8O4D4.O3G8O4C8");
    play("E8D8C8O3A8B8O4C8.O3A16G8E8O4C8O3E8E8D4E8C4.");
    play("MBT140E8D4.F8E8G8O4E8.C16D8C4O3E8C4.E8D4.F8E8G8O4E8.C16D8C4.");

    } else if (--song == 0) {

    printf("The Odd Couple\n");
    play("mn t120 l16");
    play("o2 a o3 d8. aa8. g ml a2 a4 mn g8. fg4 f8. ef2 ml d2 d2. mn");
    play("p8. d g8. o4  dd8. c ml d2 d4 mn c8. o3 b- o4 c4 o3 b-8. a ml b-2 g2 g1");
    play("mn p4 ml a4 a8. mn fg8. d ml f2g2");
    play("mn p4 ml a4 a8. mn fg8. a o4 c4 c8. o3 a f4 d4 p4 ml a4 a8. ");
    play("ml f l12 ga o4 c l16 mn d4 d8. c o3 a8. gf4 ml g1 g2. p8.");
    play("mn t120 l16");
    play("o2 a o3  d8. aa8. g ml a2 a4 mn g8. fg4 f8. ef2 ml d2 d2. mn");
    play("p8. d g8.  o4  dd8. c ml d2 d4 mn c8. o3 b- o4 c4 o3 b-8. a ml b-2 g2 g1");
    play("mn p4 ml a4 a8. mn fg8. d ml f2g2");
    play("mn p4 ml a4 a8. f l12 ga o4 c l16 mn d4 c4 o3  a8. gf8. d");
    play("ml f2 f8. mn df8. d ml f2 f8. mn df8. d ml f1 f1 ");
    play("mn d8. aa8. g ml a2 a2 p8 l8 ml dfdf1");

    } else if (--song == 0) {

    printf("Star Wars\n");
    play("t136 mn o3 l8");
    play("ddgfe-dc o2 b-ag o3 d2. l12 ddd l8 g4 p4 p2 p2 ");
    play("t236 l6 o2 ddd l2 ml g o3 dd mn l6 c o2 ba l2 o3 ml gdd");
    play("mn l6 c o2 ba ml l2 o3 gdd mn l6 c o2 b o3 c l2 ml o2 a1a4 p4 mn");
    play("t236 l6 o2 ddd l2 ml g o3 dd mn l6 c o2 ba l2 o3 ml gdd");
    play("mn l6 c o2 ba ml l2 o3 gdd mn l6 c o2 b o3 c l2 ml o2 a1a4 p4 mn");
    play("t136 mn o3 l8");
    play("p4 mn o2 l8 d4 e4.e o3c o2 bag l12 gab l8 a8. e16f+4d8. d");
    play("e4.e o3 c o2 bag o3 d8.o2  a16 ml a4a4 mn d4 e4.e O3 c o2 bag ");
    play("l12 gaba8. e16 f+4 o3 d8. d16 l16  g8. fe-8. d c8. o2 b-a8. g");
    play("o3 d2");
    play("t236 l6 o2 ddd l2 ml g o3 dd mn l6 c o2 ba l2 o3 ml gdd");
    play("mn l6 c o2 ba ml l2 o3 gdd mn l6 co2 b o3c l2 ml o2a1a4 p4 mn");
    play("t236 l6 o2 ddd l2 ml g o3 dd mn l6 c o2 ba l2 o3 ml gdd");
    play("mn l6 c o2ba ml l2 o3gdd mn l6 co2bo3c l2 ml o2a1a4 p4 mn");
    play("l6 o3 mn ddd ml l1 gggg4 p4 p4 mn l12 dddg2");

    } else if (--song == 0) {

    printf("Alex's Theme (Beverly Hills Cop)\n");
    play("mbo3l8f#p8a8.mlf#16mnf#16f#16b8f#ef#p8o4c#8.o3mlf#16mnf#16f#16");
    play("o4dc#o3af#o4c#F#o3f#16mle16mne16e16c#g#mlf#f#4p4p3");
    play("mbo3l8f#p8a8.mlf#16mnf#16f#16b8f#ef#p8o4c#8.o3mlf#16mnf#16f#16");
    play("o4dc#o3af#o4c#F#o3f#16mle16mne16e16c#g#mlf#f#4p4p3");
    play("l8o2f#p8o2f#.o2mle16mne16o2e16o1c#o2c#o1ef#p8o2f#p8p16o1c#1");
    play("6o2c#ef#o1do2p8d.o1mle16mne16o2e16o1c#ef#o2f#p4p16e16c#o1ba");

    } else if (--song == 0) {

    printf("Bagpipes\n");
    play("T200MN");
    play("O2L8AO3L4DL8DDEFL4EL8DL4DL8O2AO3DEFGAB-L2AP8");
    play("O3L8AO4L4DL8DDCO3B-L4AL8AL4AL8FL4AL8AGFEL4FL8DL4D");
    play("O3L8EL4FL8FGAB-L4AL8AO4L4DP8O3L8DEFGFEDCL2DP8L8DL2EP8L8EFGFEDCL2D");
    play("T200MN");
    play("O2L8AO3L4DL8DDEFL4EL8DL4DL8O2AO3DEFGAB-L2AP8");
    play("O3L8AO4L4DL8DDCO3B-L4AL8AL4AL8FL4AL8AGFEL4FL8DL4D");
    play("O3L8EL4FL8FGAB-L4AL8AO4L4DP8O3L8DEFGFEDCL2DP8L8DL2EP8L8EFGFEDCL2D");
    play("L1D");

    } else if (--song == 0) {

    printf("Theme from Star Trek\n");
    play("T240MLO4B1.D1.F1.O3A1.P8");
    play("T240O3MLG2G8MNO4C8.MLF2MNF2MLE4.E8C4O3A4O4D4G2P8G4B1P4.");
    play("T240O4MLC2C8MNF8.MLB-2MNB-2MLA4.A8F4D4G4O5C2P8C4E1P1");
    play("T160O3G2O4F2.E4T240D4C4O3B4");
    play("MLT160O3B-2B-1G2O4G2.F4T240E4D4C4T180O3B2B1");
    play("MLT160O3B-4A2.B4O4C#D4T240E4F#G");
    play("MLT160O4A2B-1.O3B-2.O4C4");
    play("MLT160O4D4E-4T240F4G4A-4T160B-2B1");
    play("MLT160O3G2O4F2.E4T240D4C4O3B4");
    play("MLT160O3B-2B-1A-4");
    play("MLT160O3G2O4G2.F4T240E4D4C4");
    play("MLT160O3B2B1B-4A2.B4");
    play("MLT160O4C4D4T240E4F4E4T160G2.G4");
    play("MLT160O4B-2.A4G2C1MLD4.F4.A4.");
    play("MLT160O5C1");

    }

   return 0;

}
