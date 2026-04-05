/*******************************************************************************
*                                                                              *
*                               DUMP MIDI FILE                                 *
*                                                                              *
* Dumps the contents of a midi file in text.                                   *
*                                                                              *
* dumpmidi <.mid file>                                                         *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

/*
 * Print unknown midi event codes
 */
#define MIDIUNKNOWN 1

typedef char* string;  /* general string type */
typedef unsigned char byte; /* byte */
#if __STDC_VERSION__ >= 202311L
#include <stdbool.h>
typedef bool boolean;
#else
typedef enum { false, true } boolean; /* boolean */
#endif

static void error(string s)

{

    fprintf(stderr, "\nError: %s\n", s);

    exit(1);

}

static byte readbyt(FILE* fh)

{

    byte b;
    int v;

    v = getc(fh);
    if (v == EOF) error("Invalid .mid file format");
    b = v;
/* All but the header reads call this routine, so uncommenting this print will
   give a good diagnostic for data reads */
/*
printf("@%ld: byte: %2.2x\n", ftell(fh), b);
*/

    return (b);

}

static int readvar(FILE* fh, unsigned int* v)

{

    byte b;
    int cnt;

    *v = 0;
    cnt = 0;
    do {

        b = readbyt(fh); /* get next part */
        cnt++;
        *v = *v<<7|(b&0x7f); /* shift and place new value */

    } while (b >= 128);

    return (cnt);

}

static void prttxt(FILE* fh, unsigned int len)

{

    byte b;

    while (len) {

        b = readbyt(fh);
        putchar(b);
        len--;

    }

}

static int dcdmidi(FILE* fh, byte b, boolean* endtrk)

{

    byte p1;
    byte p2;
    byte p3;
    byte p4;
    byte p5;
    unsigned len;
    int cnt;

    cnt = 0;
    *endtrk = false;
    switch (b>>4) { /* command nybble */

        case 0x8: /* note off */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  printf("Note off: channel: %d key: %d velocity: %d\n", b&15, p1, p2);
                  break;
        case 0x9: /* note on */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  printf("Note on: channel: %d key: %d velocity: %d\n", b&15, p1, p2);
                  break;
        case 0xa: /* polyphonic key pressure */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  printf("Polyphonic key pressure: channel: %d key: %d pressure: %d\n", b&15, p1, p2);
                  break;
        case 0xb: /* controller change/channel mode */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  if (p1 <= 0x77)
                      printf("Controller change: channel: %d controller number: %d controller value: %d\n", b&15, p1, p2);
                  else switch (p1) { /* channel mode messages */

                      case 0x78: /* All sound off */
                                 printf("All sound off: channel: %d\n", b&15);
                                 break;
                      case 0x79: /* Reset all controllers */
                                 printf("Reset all controllers: channel: %d\n", b&15);
                                 break;
                      case 0x7a: /* Local control */
                                 printf("Local control: channel: %d ", b&15);
                                 if (p2 == 0x00) printf("disconnect keyboard");
                                 else if (p2 == 0x7f) printf("reconnect keyboard");
                                 printf("\n");
                                 break;
                      case 0x7b: /* All notes off */
                                 printf("All notes off: channel: %d\n", b&15);
                                 break;
                      case 0x7c: /* Omni mode off */
                                 printf("Omni mode off: channel: %d\n", b&15);
                                 break;
                      case 0x7d: /* omni mode on*/
                                 printf("Omni mode on: channel: %d\n", b&15);
                                 break;
                      case 0x7e: /* Mono mode on */
                                 printf("Mono mode on: channel: %d midi channel in use: %d\n", b&15, p2);
                                 break;
                      case 0x7f: /* Poly mode on */
                                 printf("Poly mode on: channel: %d\n", b&15);
                                 break;

                  }
                  break;
        case 0xc: /* program change */
                  p1 = readbyt(fh); cnt++;
                  printf("Program change: channel: %d program number: %d\n", b&15, p1);
                  break;
        case 0xd: /* channel key pressure */
                  p1 = readbyt(fh); cnt++;
                  printf("Channel key pressure: channel: %d channel pressure value: %d\n", b&15, p1);
                  break;
        case 0xe: /* pitch bend */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  printf("Pitch bend: channel: %d value: %u\n", b&15, p2<<7|p1);
                  break;
        case 0xf: /* sysex/meta */
                  switch (b) {

                      case 0xf0: /* F0 sysex event */
                                 printf("f0 sysex event\n");
                                 cnt += readvar(fh, &len); /* get length */
                                 fseek(fh, len, SEEK_CUR);
                                 break;
                      case 0xf7: /* F7 sysex event */
                                 printf("f7 sysex event\n");
                                 cnt += readvar(fh, &len); /* get length */
                                 fseek(fh, len, SEEK_CUR);
                                 break;
                      case 0xff: /* meta events */
                                 p1 = readbyt(fh);  cnt++; /* get next byte */
                                 cnt += readvar(fh, &len); /* get length */
                                 switch (p1) {

                                     case 0x00: /* Sequence number */
                                                if (len != 2) error("Meta event length does not match");
                                                p1 = readbyt(fh);  cnt++; /* get low number */
                                                p2 = readbyt(fh);  cnt++; /* get high number */
                                                printf("Sequence number: number: %d\n", p2<<8|p1);
                                                break;
                                     case 0x01: /* Text event */
                                                printf("Text event: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x02: /* Copyright notice */
                                                printf("Copyright notice: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x03: /* Sequence/track name */
                                                printf("Sequence/track name: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x04: /* Instrument name */
                                                printf("Instrument name: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x05: /* Lyric */
                                                printf("Lyric: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x06: /* Marker */
                                                printf("Marker: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x07: /* Cue point */
                                                printf("Que point: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x08: /* program name */
                                                printf("Program name: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x09: /* Device name */
                                                printf("Device name: text: ");
                                                prttxt(fh, len);
                                                printf("\n");
                                                break;
                                     case 0x20: /* channel prefix */
                                                if (len != 1) error("Meta event length does not match");
                                                p1 = readbyt(fh); cnt++; /* get channel */
                                                printf("Channel prefix: channel: %d\n", p1);
                                                break;
                                     case 0x21: /* port prefix */
                                                if (len < 1) error("Meta event length does not match");
                                                p1 = readbyt(fh); cnt++; /* get port */
                                                printf("Port prefix: port: %d\n", p1);
                                                fseek(fh, len-1, SEEK_CUR);
                                                break;
                                     case 0x2f: /* End of track */
                                                if (len != 0) error("Meta event length does not match");
                                                printf("End of track\n");
                                                *endtrk = true; /* set end of track */
                                                break;
                                     case 0x51: /* Set tempo */
                                                if (len != 3) error("Meta event length does not match");
                                                p1 = readbyt(fh); cnt++; /* get b1-b3 */
                                                p2 = readbyt(fh); cnt++;
                                                p3 = readbyt(fh); cnt++;
                                                printf("Set tempo: new tempo: %d\n", p1<<16|p2<<8|p3);
                                                fseek(fh, len-3, SEEK_CUR);
                                                break;
                                     case 0x54: /* SMTPE offset */
                                                if (len != 5) error("Meta event length does not match");
                                                p1 = readbyt(fh); cnt++; /* get b1-b5 */
                                                p2 = readbyt(fh); cnt++;
                                                p3 = readbyt(fh); cnt++;
                                                p4 = readbyt(fh); cnt++;
                                                p5 = readbyt(fh); cnt++;
                                                printf("SMTPE offset time: %2.2d:%2.2d:%2.2d frames: %d fractional frame: %d\n",
                                                       p1, p2, p3, p4, p5);
                                                break;
                                     case 0x58: /* Time signature */
                                                if (len != 4) error("Meta event length does not match");
                                                p1 = readbyt(fh); cnt++; /* get b1-b4 */
                                                p2 = readbyt(fh); cnt++;
                                                p3 = readbyt(fh); cnt++;
                                                p4 = readbyt(fh); cnt++;
                                                printf("Time signature: numerator: %d denominator: %d MIDI clocks: %d number of 1/32 notes: %d\n",
                                                       p1, p2, p3, p4);
                                                break;
                                     case 0x59: /* Key signature */
                                                if (len != 2) error("Meta event length does not match");
                                                p1 = readbyt(fh); cnt++; /* get b1-b2 */
                                                p2 = readbyt(fh); cnt++;
                                                printf("Key signature: sharps or flats: %d ", p1);
                                                if (p2 == 0) printf("major key");
                                                else if (p2 == 1) printf("minor key");
                                                printf("\n");
                                                break;
                                     case 0x7f: /* Sequencer specific */
                                                printf("Sequencer specific\n");
                                                fseek(fh, len, SEEK_CUR);
                                                break;

                                     default:   /* unknown */
                                                printf("Unknown meta event: %2.2x\n", p1);
                                                fseek(fh, len, SEEK_CUR);
                                                break;

                                 }
                                 break;

                      default:
#ifdef MIDIUNKNOWN
                               fprintf(stderr, "Unknown sysex event: %2.2x\n", b);
#endif
                               error("Invalid .mid file format");
                               break;

                  }
                  break;

        default:
#ifdef MIDIUNKNOWN
                 fprintf(stderr, "Unknown status event: %2.2x\n", b);
#endif
                 error("Invalid .mid file format");

    }

    return (cnt);

}

unsigned int read16be(FILE* fh)

{

    byte b;
    unsigned int v;

    v = readbyt(fh) << 8;
    v |= readbyt(fh);

    return (v);

}

unsigned int read32be(FILE* fh)

{

    byte b;
    unsigned int v;

    v = readbyt(fh) << 24;
    v |= readbyt(fh) << 16;
    v |= readbyt(fh) << 8;
    v |= readbyt(fh);

    return (v);

}

unsigned int str2id(string ids)

{

    return (ids[0]<<24|ids[1]<<16|ids[2]<<8|ids[3]);

}

void prthid(FILE* fh, unsigned int id)

{

    putc(id >>24 & 0xff, fh);
    putc(id >>16 & 0xff, fh);
    putc(id >>8 & 0xff, fh);
    putc(id & 0xff, fh);

}

int main(int argc, char* argv[])

{

    FILE*          fh;         /* file handle */
    unsigned int   rem;        /* remaining track length */
    unsigned int   len;        /* length read */
    unsigned int   hlen;       /* header length */
    unsigned int   delta_time; /* delta time */
    boolean        endtrk;     /* end of track flag */
    byte           last;       /* last command */
    unsigned short fmt;        /* format code */
    unsigned short tracks;     /* number of tracks */
    unsigned short division;   /* delta time */
    int            found;      /* found our header */
    unsigned int   id;         /* id */
    byte           b;
    int            trk;

    if (argc != 2) {

        fprintf(stderr, "Usage: dumpmidi <.mid file>\n");
        exit(1);

    }

    fh = fopen(argv[1], "r");
    if (!fh) error("Cannot open input .mid file");
    id = read32be(fh); /* get header id */

    /* check RIFF prefix */
    if (id == str2id("RIFF")) {

        len = read32be(fh); /* read and discard length */
        /* a RIFF file can contain a MIDI file, so we search within it */
        id = read32be(fh); /* get header id */
        if (id != str2id("RMID")) error("Invalid .mid file header");
        do {

            id = read32be(fh); /* get next id */
            len = read32be(fh); /* get length */
            found = id == str2id("data"); /* check data header */
            if (!found) fseek(fh, len, SEEK_CUR);

        } while (!found); /* not SMF header */
        id = read32be(fh); /* get next id */

    }

    /* check a midi file header */
    if (id != str2id("MThd")) error("Invalid .mid file header");

    /* read the rest of the file header */
    hlen = read32be(fh); /* get length */
    fmt = read16be(fh); /* format */
    tracks = read16be(fh); /* tracks */
    division = read16be(fh); /* delta time */

    /* check and reject SMTPE framing */
    if (division & 0x80000000) error("Cannot handle SMTPE framing");

    printf("Mid file header contents\n");

    printf("Len:      %d\n", hlen);
    printf("fmt:      %d\n", fmt);
    printf("tracks:   %d\n", tracks);
    printf("division: %d\n", division);

    for (trk = 1; trk <= tracks && !feof(fh); trk++) { /* read track chunks */

        if (!feof(fh)) { /* not eof */

            /* Read in .mid track header */
            id = read32be(fh); /* get header id */
            hlen = read32be(fh); /* get length */

            /* check a midi header */
            if (id == str2id("MTrk")) {

                printf("Track: %d\n", trk);
                rem = hlen; /* set remainder to parse */
                last = 0; /* clear last command */
                do {

                    len = readvar(fh, &delta_time); /* get delta time */
                    rem -= len; /* count */
                    b = getc(fh); /* get the command byte */
                    rem--; /* count */
                    if (b < 0x80) { /* process running status or repeat */

                        ungetc(b, fh); /* put back parameter byte */
                        rem++; /* back out count */
                        b = last; /* put back command for repeat */

                    }
                    len = dcdmidi(fh, b, &endtrk); /* decode midi instruction */
                    rem -= len; /* count */
                    /* if command is not meta, save as last command */
                    if (b < 0xf0) last = b;

                } while (rem && !endtrk); /* until run out of commands */

            } else fseek(fh, hlen, SEEK_CUR);

        }

    }

}
