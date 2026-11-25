/*******************************************************************************

Play text MIDI file

playtextmidi [--port=<port>|-p <port>] <text midi file>

Reads a text MIDI file in the mf2t/t2mf format and plays it through the Ami
sound API. The text format is line-oriented:

    MFile <format> <ntracks> <division>
    MTrk
    <time> <event> [args...]
    ...
    TrkEnd

Events understood:

    NoteOn ch=<c> n=<n> v=<v>
    NoteOff ch=<c> n=<n> v=<v>
    PrCh ch=<c> p=<p>              (program/instrument change)
    Par ch=<c> c=<cc> v=<v>        (control change / parameter)
    PitchBend ch=<c> v=<v>
    ChPr ch=<c> v=<v>              (channel pressure)
    KeyPr ch=<c> n=<n> v=<v>       (polyphonic aftertouch)
    Tempo <us-per-quarter>
    Meta TrkEnd

Times are in MIDI ticks. Division from the MFile header sets ticks-per-quarter.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>

#include <sound.h>
#include <option.h>

/*******************************************************************************

Constants

*******************************************************************************/

#define MAXTRK   64
#define MAXEVT   50000
#define SECOND   10000   /* Ami time unit: 100us ticks per second */

/*******************************************************************************

Types

*******************************************************************************/

typedef enum {
    EVT_NOTEON, EVT_NOTEOFF, EVT_PRCH, EVT_PAR, EVT_PITCHBEND,
    EVT_CHPR, EVT_KEYPR, EVT_TEMPO, EVT_END
} evttype;

typedef struct {
    int      tick;    /* absolute tick */
    int      abstime; /* computed absolute time in 100us Ami units */
    evttype  type;
    int      ch;      /* channel 1-16 */
    int      n;       /* note or controller number */
    int      v;       /* velocity / value */
    int      tempo;   /* microseconds per quarter (for tempo events) */
} midievt;

/*******************************************************************************

Globals

*******************************************************************************/

ami_long dport = AMI_SYNTH_OUT;

ami_optrec opttbl[] = {

    { "port",  NULL, &dport,  NULL, NULL },
    { "p",     NULL, &dport,  NULL, NULL },
    { NULL,    NULL, NULL,    NULL, NULL }

};

static midievt events[MAXEVT];
static int     nevents;
static int     division = 480; /* ticks per quarter note */

/*******************************************************************************

Parse helpers

*******************************************************************************/

static int getparam(const char* s, const char* key)
{
    char pat[32];
    snprintf(pat, sizeof(pat), "%s=", key);
    const char* p = strstr(s, pat);
    if (!p) return -1;
    return atoi(p + strlen(pat));
}

/*******************************************************************************

Parse one track from the file

*******************************************************************************/

static void parse_track(FILE* f)
{
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        /* strip newline */
        char* nl = strchr(line, '\n');
        if (nl) *nl = 0;
        nl = strchr(line, '\r');
        if (nl) *nl = 0;

        /* skip blank lines */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) continue;

        /* end of track */
        if (strncmp(p, "TrkEnd", 6) == 0) return;

        /* parse tick */
        if (!isdigit((unsigned char)*p) && *p != '-') continue;
        int tick = (int)strtol(p, &p, 10);
        while (*p == ' ' || *p == '\t') p++;

        if (nevents >= MAXEVT) continue;
        midievt* e = &events[nevents];
        e->tick = tick;

        if (strncmp(p, "On", 2) == 0 || strncmp(p, "NoteOn", 6) == 0) {
            e->type = EVT_NOTEON;
            e->ch = getparam(p, "ch");
            e->n  = getparam(p, "n");
            e->v  = getparam(p, "v");
            if (e->v == 0) e->type = EVT_NOTEOFF;
            nevents++;
        } else if (strncmp(p, "Off", 3) == 0 ||
                   strncmp(p, "NoteOff", 7) == 0) {
            e->type = EVT_NOTEOFF;
            e->ch = getparam(p, "ch");
            e->n  = getparam(p, "n");
            e->v  = getparam(p, "v");
            nevents++;
        } else if (strncmp(p, "PrCh", 4) == 0) {
            e->type = EVT_PRCH;
            e->ch = getparam(p, "ch");
            e->v  = getparam(p, "p");
            nevents++;
        } else if (strncmp(p, "Par", 3) == 0) {
            e->type = EVT_PAR;
            e->ch = getparam(p, "ch");
            e->n  = getparam(p, "c");
            e->v  = getparam(p, "v");
            nevents++;
        } else if (strncmp(p, "PitchBend", 9) == 0 ||
                   strncmp(p, "Pb", 2) == 0) {
            e->type = EVT_PITCHBEND;
            e->ch = getparam(p, "ch");
            e->v  = getparam(p, "v");
            nevents++;
        } else if (strncmp(p, "ChPr", 4) == 0) {
            e->type = EVT_CHPR;
            e->ch = getparam(p, "ch");
            e->v  = getparam(p, "v");
            nevents++;
        } else if (strncmp(p, "KeyPr", 5) == 0) {
            e->type = EVT_KEYPR;
            e->ch = getparam(p, "ch");
            e->n  = getparam(p, "n");
            e->v  = getparam(p, "v");
            nevents++;
        } else if (strncmp(p, "Tempo", 5) == 0) {
            e->type = EVT_TEMPO;
            p += 5;
            while (*p == ' ' || *p == '\t') p++;
            e->tempo = atoi(p);
            nevents++;
        } else if (strncmp(p, "Meta", 4) == 0 &&
                   strstr(p, "TrkEnd")) {
            return;
        }
    }
}

/*******************************************************************************

Compare events by tick for qsort (stable by original order)

*******************************************************************************/

static int evt_cmp(const void* a, const void* b)
{
    const midievt* ea = (const midievt*)a;
    const midievt* eb = (const midievt*)b;
    if (ea->abstime != eb->abstime) return ea->abstime - eb->abstime;
    return (int)(ea - events) - (int)(eb - events);
}

/*******************************************************************************

Convert ticks to absolute time

Walks through the event list applying tempo changes. Time unit is 100us.

*******************************************************************************/

static void compute_times(void)
{
    int tempo = 500000; /* default 120 BPM = 500000 us/quarter */
    int i;

    /* first pass: assign absolute time based on tempo map */
    for (i = 0; i < nevents; i++) {
        /* us_per_tick = tempo / division */
        /* ami_time = tick * us_per_tick / 100  (100us units) */
        long long us = (long long)events[i].tick * tempo / division;
        events[i].abstime = (int)(us / 100);
    }

    /* second pass: apply tempo changes — recompute all events after each
       tempo change using the new tempo for ticks after the change point */
    int last_tempo_tick = 0;
    int last_tempo_time = 0;
    tempo = 500000;

    for (i = 0; i < nevents; i++) {
        long long us = (long long)(events[i].tick - last_tempo_tick) * tempo
                       / division;
        events[i].abstime = last_tempo_time + (int)(us / 100);
        if (events[i].type == EVT_TEMPO) {
            last_tempo_tick = events[i].tick;
            last_tempo_time = events[i].abstime;
            tempo = events[i].tempo;
        }
    }
}

/*******************************************************************************

Scale velocity from MIDI 0-127 to Ami 0-LONG_MAX

*******************************************************************************/

static ami_long scalevel(int v)
{
    if (v <= 0) return 0;
    if (v >= 127) return LONG_MAX;
    /* divide full scale first to avoid 64 bit overflow */
    return v * (LONG_MAX / 127);
}

/*******************************************************************************

Main

*******************************************************************************/

int main(int argc, char **argv)
{
    ami_long argi = 1;
    ami_long argcl = argc;
    char line[1024];

    ami_options(&argi, &argcl, argv, opttbl, TRUE);

    if (argi >= argcl) {
        fprintf(stderr,
                "Usage: playtextmidi [--port=<port>|-p <port>] <file>\n");
        exit(1);
    }

    FILE* f = fopen(argv[argi], "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", argv[argi]);
        exit(1);
    }

    /* parse MFile header and tracks */
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strncmp(p, "MFile", 5) == 0) {
            int fmt, ntracks;
            sscanf(p + 5, "%d %d %d", &fmt, &ntracks, &division);
            if (division <= 0) division = 480;
        } else if (strncmp(p, "MTrk", 4) == 0) {
            parse_track(f);
        }
    }
    fclose(f);

    if (nevents == 0) {
        fprintf(stderr, "No events found in %s\n", argv[argi]);
        exit(1);
    }

    /* compute absolute times and sort */
    compute_times();
    qsort(events, nevents, sizeof(midievt), evt_cmp);

    printf("Loaded %d events, division=%d\n", nevents, division);

    /* open synth and start sequencer */
    ami_opensynthout(dport);
    ami_starttimeout();

    /* find last event time for waiting */
    int lasttime = 0;

    /* send all events as sequenced */
    for (int i = 0; i < nevents; i++) {
        midievt* e = &events[i];
        int t = e->abstime;
        int ch = e->ch;

        if (t > lasttime) lasttime = t;

        switch (e->type) {
        case EVT_NOTEON:
            ami_noteon(dport, t, ch, e->n + 1, scalevel(e->v));
            break;
        case EVT_NOTEOFF:
            ami_noteoff(dport, t, ch, e->n + 1, scalevel(e->v));
            break;
        case EVT_PRCH:
            ami_instchange(dport, t, ch, e->v + 1);
            break;
        case EVT_PAR:
            /* map MIDI CC to ami calls */
            switch (e->n) {
            case 1:
                ami_vibrato(dport, t, ch, scalevel(e->v));
                break;
            case 7:
                ami_volsynthchan(dport, t, ch, scalevel(e->v));
                break;
            case 10:
                /* divide full scale first to avoid 64 bit overflow */
                ami_pan(dport, t, ch, (e->v - 64) * (LONG_MAX / 64));
                break;
            case 64: /* sustain — not directly mapped, skip */
                break;
            case 91:
                ami_reverb(dport, t, ch, scalevel(e->v));
                break;
            case 93:
                ami_chorus(dport, t, ch, scalevel(e->v));
                break;
            default:
                break;
            }
            break;
        case EVT_PITCHBEND:
            /* divide full scale first to avoid 64 bit overflow */
            ami_pitch(dport, t, ch, (e->v - 8192) * (LONG_MAX / 8192));
            break;
        case EVT_CHPR:
            ami_pressure(dport, t, ch, scalevel(e->v));
            break;
        case EVT_KEYPR:
            ami_aftertouch(dport, t, ch, e->n + 1, scalevel(e->v));
            break;
        case EVT_TEMPO:
        case EVT_END:
            break;
        }
    }

    /* wait for all sequenced events to fire, then a 2s tail for release */
    while (ami_curtimeout() < lasttime)
        usleep(50000);
    usleep(2000000);

    ami_stoptimeout();
    ami_closesynthout(dport);

    return 0;
}
