/*******************************************************************************
*                                                                              *
*                               SOUND LIBRARY                                  *
*                                                                              *
*                              11/02 S. A. Franco                              *
*                                                                              *
* Sndlib is a combination of wave file and midi output and control functions.  *
* Implements a set of midi controls and wave controls. Also includes a "flow   *
* through sequencer" function. Each event has a timestamp, and if zero, it     *
* is performed immediately, otherwise scheduled. This allows any mix of        *
* immediate vs. sequenced events.                                              *
*                                                                              *
* Notes:                                                                       *
*                                                                              *
* 1. The parameter conversion work is being performed when a sequenced item    *
* is performed. This could be moved back to the entry of the event to save     *
* time in the timer handler.                                                   *
*                                                                              *
* 2. The validation of parameters happens both at entry time and at sequence   *
* time, need not happen on both.                                               *
*                                                                              *
* 3. The model for running MIDI and waveform files might have problems. Its    *
* supposed to be an accurate event, but its going to have file lookup time     *
* built into it, which could affect start time. A logical preload/cache        *
* model would give this package the ability to do something about that.        *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00

#include <sys/types.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#include <commctrl.h>

#include <config.h>
#include <sound.h>

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); } while (0)

//#define SHOWMIDIIN 1 /* show midi in dumps */

#define MAXMIDP 100 /* maximum midi input/output devices */
#define MAXWAVP 100 /* maximum wave input/output devices */
#define MAXMIDT 100 /* maximum number of midi tracks that can be stored */
#define MAXWAVT 100 /* maximum number of wave tracks that can be stored */

#define WAVBUF    (16*1024) /* size of output wave buffer */
#define MAXFIL    200 /* maximum size of wave table filename */
#define WAVSIZ    8192 /*4096*/ /* size of wave input buffer, about 46ms */
#define MIDMAX    1025 /* size of MIDI input message queue */
#define MIDBUFSIZ 1024 /* MIDI message buffer */

#define DEFMIDITIM 5000 /* default midi quarter note (.5 seconds) */

/*
 * Print unknown midi event codes
 */
#define MIDIUNKNOWN 1

/* midi status messages, high nybble */

#define MESS_NOTE_OFF 0x80
#define MESS_NOTE_ON  0x90
#define MESS_AFTTCH   0xa0
#define MESS_CTRL_CHG 0xb0
#define MESS_PGM_CHG  0xc0
#define MESS_CHN_PRES 0xd0
#define MESS_PTCH_WHL 0xe0

/* midi controller numbers */

#define CTLR_BANK_SELECT_COARSE              0
#define CTLR_MODULATION_WHEEL_COARSE         1
#define CTLR_BREATH_CONTROLLER_COARSE        2
#define CTLR_FOOT_PEDAL_COARSE               4
#define CTLR_PORTAMENTO_TIME_COARSE          5
#define CTLR_DATA_ENTRY_COARSE               6
#define CTLR_VOLUME_COARSE                   7
#define CTLR_BALANCE_COARSE                  8
#define CTLR_PAN_POSITION_COARSE             10
#define CTLR_EXPRESSION_COARSE               11
#define CTLR_EFFECT_CONTROL_1_COARSE         12
#define CTLR_EFFECT_CONTROL_2_COARSE         13
#define CTLR_GENERAL_PURPOSE_SLIDER_1        16
#define CTLR_GENERAL_PURPOSE_SLIDER_2        17
#define CTLR_GENERAL_PURPOSE_SLIDER_3        18
#define CTLR_GENERAL_PURPOSE_SLIDER_4        19
#define CTLR_BANK_SELECT_FINE                32
#define CTLR_MODULATION_WHEEL_FINE           33
#define CTLR_BREATH_CONTROLLER_FINE          34
#define CTLR_FOOT_PEDAL_FINE                 36
#define CTLR_PORTAMENTO_TIME_FINE            37
#define CTLR_DATA_ENTRY_FINE                 38
#define CTLR_VOLUME_FINE                     39
#define CTLR_BALANCE_FINE                    40
#define CTLR_PAN_POSITION_FINE               42
#define CTLR_EXPRESSION_FINE                 43
#define CTLR_EFFECT_CONTROL_1_FINE           44
#define CTLR_EFFECT_CONTROL_2_FINE           45
#define CTLR_HOLD_PEDAL                      64
#define CTLR_PORTAMENTO                      65
#define CTLR_SUSTENUTO_PEDAL                 66
#define CTLR_SOFT_PEDAL                      67
#define CTLR_LEGATO_PEDAL                    68
#define CTLR_HOLD_2_PEDAL                    69
#define CTLR_SOUND_VARIATION                 70
#define CTLR_SOUND_TIMBRE                    71
#define CTLR_SOUND_RELEASE_TIME              72
#define CTLR_SOUND_ATTACK_TIME               73
#define CTLR_SOUND_BRIGHTNESS                74
#define CTLR_SOUND_CONTROL_6                 75
#define CTLR_SOUND_CONTROL_7                 76
#define CTLR_SOUND_CONTROL_8                 77
#define CTLR_SOUND_CONTROL_9                 78
#define CTLR_SOUND_CONTROL_10                79
#define CTLR_GENERAL_PURPOSE_BUTTON_1        80
#define CTLR_GENERAL_PURPOSE_BUTTON_2        81
#define CTLR_GENERAL_PURPOSE_BUTTON_3        82
#define CTLR_GENERAL_PURPOSE_BUTTON_4        83
#define CTLR_EFFECTS_LEVEL                   91
#define CTLR_TREMULO_LEVEL                   92
#define CTLR_CHORUS_LEVEL                    93
#define CTLR_CELESTE_LEVEL                   94
#define CTLR_PHASER_LEVEL                    95
#define CTLR_DATA_BUTTON_INCREMENT           96
#define CTLR_DATA_BUTTON_DECREMENT           97
#define CTLR_NON_REGISTERED_PARAMETER_FINE   98
#define CTLR_NON_REGISTERED_PARAMETER_COARSE 99
#define CTLR_REGISTERED_PARAMETER_FINE       100
#define CTLR_REGISTERED_PARAMETER_COARSE     101
#define CTLR_ALL_SOUND_OFF                   120
#define CTLR_ALL_CONTROLLERS_OFF             121
#define CTLR_LOCAL_KEYBOARD                  122
#define CTLR_ALL_NOTES_OFF                   123
#define CTLR_OMNI_MODE_OFF                   124
#define CTLR_OMNI_MODE_ON                    125
#define CTLR_MONO_OPERATION                  126
#define CTLR_POLY_OPERATION                  127

/* wave device descriptor */

typedef struct snddev {

    HWAVEOUT hwo;    /* handle to wave output device */
    HWAVEIN  hwi;    /* handle to wave input device */
    int      open;   /* device is open */
    int      init;   /* device is initialized */
    WAVEHDR  hdre;   /* wave data header even */
    unsigned reme;   /* buffer remainder even */
    WAVEHDR  hdro;   /* wave data header odd */
    unsigned remo;   /* buffer remainder odd */
    int      oddevn; /* odd/even header/buffer selector */
    int      parval; /* input device parameters valid */
    int      chan;   /* number of channels */
    int      bits;   /* preferred format bit size */
    int      rate;   /* sample rate */
    int      sgn;    /* preferred format sign */
    int      big;    /* preferred format big endian */
    int      flt;    /* preferred format floating point */
    int      ssiz;   /* sample size */

} snddev;

typedef snddev* devptr; /* pointer to sound device */

/* windows message input format */

typedef struct {

    DWORD_PTR time; /* sequencer input time */
    DWORD_PTR mmsg; /* MIDI message (packed format) */

} winmidmsg;

/* midi input device descriptor */

typedef struct {

    HMIDIIN       hmi;            /* midi device handle in */
    winmidmsg     inpque[MIDMAX]; /* midi input queue */
    int           inpptr;         /* queue input pointer */
    int           outptr;         /* queue output pointer */
    int           open;           /* device is open */
    int           qovf;           /* input queue has overflowed */
    MIDIHDR       mh1, mh2;       /* buffer headers */
    unsigned char mb1[MIDBUFSIZ]; /* buffers */
    unsigned char mb2[MIDBUFSIZ];

} midinp, *midinpptr;

static HMIDIOUT         midouttab[MAXMIDP]; /* MIDI output device table */
static midinpptr        midinptab[MAXMIDP]; /* MIDI input device table */
static devptr           pcmout[MAXWAVP];    /* wave output device table */
static devptr           pcmin[MAXWAVP];     /* wave input device table */
static int              i;                  /* index for midi tables */
static ami_seqptr        seqlst;             /* active sequencer entries */
static ami_seqptr        seqfre;             /* free sequencer entries */
static int              seqrun;             /* sequencer running */
static DWORD            strtim;             /* start time for sequencer, in raw
                                               windows time */
static MMRESULT         timhan;             /* handle for running timer */
static int              sinrun;             /* input timer running */
static DWORD            sintim;             /* start time input midi, in raw
                                               windows time */
static CRITICAL_SECTION seqlock;            /* sequencer task lock */
static string           synthnam[MAXMIDT];  /* midi track file names */
static string           wavenam[MAXWAVT];   /* wave track file names */
static HANDLE           playwavecomplete;   /* wave end event handle */
static int              wavcnt;             /* count outstanding wave starts */

/*******************************************************************************

Process sound library error

Outputs an error message, then halts.

*******************************************************************************/

static void error(const string s)

{

    fprintf(stderr, "*** Sound: %s\n", s);

    exit(1);

}

/*******************************************************************************

Find path separator character

Returns the character used to separate filename path sections.
In windows/dos this is "\", in Unix/Linux it is '/'. One possible solution to
pathing is to accept both characters as a path separator. This means that
systems that use the '\' as a forcing character would need to represent the
separator as '\\'.

*******************************************************************************/

static char pthchr(void)
{

    return ('\\');

}

/********************************************************************************

Break file specification

Breaks a filespec down into its components, the path, name and extension.
Note that we don't validate file specifications here. Note that any part of
the file specification could be returned blank.

For Windows, we trim leading and trailing spaces, but leave any embedded spaces
or ".".

The path is straightforward, and consists of any number of /x sections. The
Presence of a trailing "\" without a name means the entire thing gets parsed
as a path, including any embedded spaces or "." characters.

Windows allows any number of "." characters, so we consider the extension to be
only the last such section, which could be null. Windows does not technically
consider "." to be a special character, but if the brknam and maknam procedures
are properly paired, it will effectively be treated the same as if the "."
were a normal character.

********************************************************************************/

static void brknam(
        /* file specification */ char *fn,
        /* path */               char *p, int pl,
        /* name */               char *n, int nl,
        /* extention */          char *e, int el
)

{

    int i, s, f, t; /* string indexes */
    char *s1, *s2;

    /* clear all strings */
    *p = 0;
    *n = 0;
    *e = 0;
    if (!strlen(fn)) error("File specification is empty");
    s1 = fn; /* index file spec */
    /* skip spaces */
    while (*s1 && *s1 == ' ') s1++;
    /* find last '/' that will mark the path */
    s2 = strrchr(s1, pthchr());
    if (s2) {

        /* there was a path, store that */
        while (s1 <= s2) {

            if (!pl) error("String to large for destination\n");
            *p++ = *s1++;
            pl--;

        }
        if (!nl) error("String to large for destination\n");
        *p = 0;
        /* now s1 points after path */

    }
    /* keep any leading '.' characters from fooling extension finder */
    s2 = s1; /* copy start point in name */
    while (*s2 == '.') s2++;
    /* find last '.' that will mark the extension */
    s2 = strrchr(s2, '.');
    if (s2) {

        /* there was a name, store that */
        while (s1 < s2) {

            if (!nl) error("String to large for destination\n");
            *n++ = *s1++;
            nl--;

        }
        if (!nl) error("String to large for destination\n");
        *n = 0;
        /* now s1 points to extension or nothing */

    } else {

        /* no extension */
        if (strlen(s1)+1 > nl) error("String to large for destination\n");
        strcpy(n, s1);
        while (*s1) s1++;

    }
    if (*s1 == '.') {

        /* there is an extension */
        if (strlen(s1)+1 > el) error("String to large for destination\n");
        s1++; /* skip '.' */
        strcpy(e, s1);

    }

}

/********************************************************************************

Make specification

Creates a file specification from its components, the path, name and extention.
We make sure that the path is properly terminated with ':' or '\' before
concatenating.

********************************************************************************/

static void maknam(
    /** file specification to build */ char *fn,
    /** file specification length */   int fnl,
    /** path */                        char *p,
    /** filename */                    char *n,
    /** extension */                   char *e
)
{

    int  i;    /* index for string */
    int  fsi;  /* index for output filename */
    char s[2]; /* path character buffer */

    if (strlen(p) > fnl) error("String too large for destination");
    strcpy(fn, p); /* place path */
    /* check path properly terminated */
    i = strlen(p);   /* find length */
    if (*p) /* not null */
        if (p[i-1] != pthchr()) {

        if (strlen(fn)+1 > fnl) error("String too large for destination");
        s[0] = pthchr(); /* set up path character as string */
        s[1] = 0;
        strcat(fn, s); /* add path separator */

    }
    /* terminate path */
    if (strlen(fn)+strlen(n) > fnl) error("String too large for destination");
    strcat(fn, n); /* place name */
    if (*e) {  /* there is an extension */

        if (strlen(fn)+1+strlen(e) > fnl)
            error("String too large for destination");
        strcat(fn, "."); /* place '.' */
        strcat(fn, e); /* place extension */

    }

}

/*******************************************************************************

Validate wave output table entry

If the output device table for the given wave output port is empty, allocates a
new device pointer for it and initializes it to default values.

Note that the port number is not checked.

*******************************************************************************/

static void makpcmout(int p)

{

    if (!pcmout[p-1]) {

        /* get new entry */
        pcmout[p-1] = malloc(sizeof(snddev));
        if (!pcmout[p-1]) error("No memory");
        /* set default values */
        pcmout[p-1]->hwo = 0;
        pcmout[p-1]->chan = 2;
        pcmout[p-1]->bits = 16;
        pcmout[p-1]->rate = 44100;
        pcmout[p-1]->sgn = FALSE;
        pcmout[p-1]->flt = FALSE;
        pcmout[p-1]->big = FALSE;
        pcmout[p-1]->open = FALSE;
        pcmout[p-1]->init = FALSE;
        pcmout[p-1]->oddevn = FALSE;

    }

}

/*******************************************************************************

Validate wave input table entry

If the input device table for the given wave output port is empty, allocates a
new device pointer for it and initializes it to default values.

Note that the port number is not checked.

*******************************************************************************/

static void makpcmin(int p)

{

    if (!pcmin[p-1]) {

        /* get new entry */
        pcmin[p-1] = malloc(sizeof(snddev));
        if (!pcmin[p-1]) error("No memory");
        /* set default values */
        pcmin[p-1]->hwi = 0;
        pcmin[p-1]->chan = 2;
        pcmin[p-1]->bits = 16;
        pcmin[p-1]->rate = 44100;
        pcmin[p-1]->sgn = FALSE;
        pcmin[p-1]->flt = FALSE;
        pcmin[p-1]->big = FALSE;
        pcmin[p-1]->open = FALSE;
        pcmin[p-1]->init = FALSE;
        pcmin[p-1]->oddevn = FALSE;
        pcmin[p-1]->parval = FALSE;

    }

}

/*******************************************************************************

Validate MIDI input table entry

If the output device table for the given MIDI input port is empty, allocates a
new device pointer for it and initializes it to default values.

Note that the port number is not checked.

*******************************************************************************/

static void makmidinp(int p)

{

    if (!midinptab[p-1]) {

        /* get new entry */
        midinptab[p-1] = malloc(sizeof(midinp));
        if (!midinptab[p-1]) error("No memory");
        /* set default values */
        midinptab[p-1]->inpptr = 0; /* set queue empty */
        midinptab[p-1]->outptr = 0;
        midinptab[p-1]->open = FALSE; /* set not open */
        midinptab[p-1]->qovf = FALSE; /* reset queue overflow flag */

    }

}

/*******************************************************************************

Get sequencer message entry

Gets a sequencer message entry, either from the used list, or new.

*******************************************************************************/

static void getseq(ami_seqptr* p)

{

    if (seqfre) { /* get a previous sequencer message */

        *p = seqfre; /* index top entry */
        seqfre = seqfre->next; /* gap out */

    } else {

        /* else get a new entry, with full allocation */
        *p = malloc(sizeof(ami_seqmsg));
        if (!*p) error("No memory");

    }
    (*p)->next = NULL; /* clear next */

}

/*******************************************************************************

Put sequencer message entry

Puts a sequencer message entry to the free list for reuse.

*******************************************************************************/

static void putseq(ami_seqptr p)

{

    p->next = seqfre; /* link to top of list */
    seqfre = p; /* push onto list */

}

/*******************************************************************************

Insert sequencer message

Inserts a sequencer message into the list, in ascending time order.

*******************************************************************************/

static void insseq(ami_seqptr p)

{

    ami_seqptr lp, l;

    EnterCriticalSection(&seqlock); /* start exclusive access */
    /* check sequencer list empty */
    if (!seqlst) seqlst = p; /* place as first if so */
    /* check insert to start */
    else if (p->time < seqlst->time) {

        p->next = seqlst; /* push onto list */
        seqlst = p;

    } else { /* must search list */

        lp = seqlst; /* index top of list */
        while (p->time >= lp->time && lp->next) {

            /* skip to key entry or end */
            l = lp; /* set last */
            lp = lp->next; /* index next */

        }
        if (p->time < lp->time) { /* insert here */

            l->next = p; /* link last to this */
            p->next = lp; /* link this to next */

        } else { /* insert after */

            p->next = lp->next;
            lp->next = p;

        }

    }
    LeaveCriticalSection(&seqlock); /* end exclusive access */

}

/*******************************************************************************

Execute sequencer message

Executes the call referenced by the message. Each call is performed with
sequencer bypass, which means its ok to loop back on the call.

*******************************************************************************/

static void excseq(ami_seqptr p)

{

    switch (p->st) { /* sequencer message type */

        case st_noteon:       ami_noteon(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_noteoff:      ami_noteoff(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_instchange:   ami_instchange(p->port, 0, p->icc, p->ici); break;
        case st_attack:       ami_attack(p->port, 0, p->vsc, p->vsv); break;
        case st_release:      ami_release(p->port, 0, p->vsc, p->vsv); break;
        case st_legato:       ami_legato(p->port, 0, p->bsc, p->bsb); break;
        case st_portamento:   ami_portamento(p->port, 0, p->bsc, p->bsb); break;
        case st_vibrato:      ami_vibrato(p->port, 0, p->vsc, p->vsv); break;
        case st_volsynthchan: ami_volsynthchan(p->port, 0, p->vsc, p->vsv); break;
        case st_porttime:     ami_porttime(p->port, 0, p->vsc, p->vsv); break;
        case st_balance:      ami_balance(p->port, 0, p->vsc, p->vsv); break;
        case st_pan:          ami_pan(p->port, 0, p->vsc, p->vsv); break;
        case st_timbre:       ami_timbre(p->port, 0, p->vsc, p->vsv); break;
        case st_brightness:   ami_brightness(p->port, 0, p->vsc, p->vsv); break;
        case st_reverb:       ami_reverb(p->port, 0, p->vsc, p->vsv); break;
        case st_tremulo:      ami_tremulo(p->port, 0, p->vsc, p->vsv); break;
        case st_chorus:       ami_chorus(p->port, 0, p->vsc, p->vsv); break;
        case st_celeste:      ami_celeste(p->port, 0, p->vsc, p->vsv); break;
        case st_phaser:       ami_phaser(p->port, 0, p->vsc, p->vsv); break;
        case st_aftertouch:   ami_aftertouch(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_pressure:     ami_pressure(p->port, 0, p->ntc, p->ntv);
                              break;
        case st_pitch:        ami_pitch(p->port, 0, p->vsc, p->vsv); break;
        case st_pitchrange:   ami_pitchrange(p->port, 0, p->vsc, p->vsv); break;
        case st_mono:         ami_mono(p->port, 0, p->vsc, p->vsv); break;
        case st_poly:         ami_poly(p->port, 0, p->pc); break;
        case st_playsynth:    ami_playsynth(p->port, 0, p->sid); break;
        case st_playwave:     ami_playwave(p->port, 0, p->wt); break;
        case st_volwave:      ami_volwave(p->port, 0, p->wv); break;

    }

}

/*******************************************************************************

Dump sequencer list

A diagnostic, dumps the given sequencer list in ASCII.

*******************************************************************************/

static void dmpseq(ami_seqptr p)

{

    switch (p->st) { /* sequencer message type */

        case st_noteon:       printf("noteon: Time: %d Port: %d Channel: %d "
                                     "Note: %d Velocity: %d\n",
                                     p->time, p->port, p->ntc, p->ntn, p->ntv);
                              break;
        case st_noteoff:      printf("noteoff: Time: %d Port: %d Channel: %d "
                                     "Note: %d Velocity: %d\n", p->time,
                                     p->port, p->ntc, p->ntn, p->ntv);
                              break;
        case st_instchange:   printf("instchange: Time: %d Port: %d p->port "
                                     "Channel: %d Instrument: %d\n", p->time,
                                     p->port, p->icc, p->ici);
                              break;
        case st_attack:       printf("attack: Time: %d Port: %d Channel: %d "
                                     "attack time: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_release:      printf("release: Time: %d Port: %d Channel: %d "
                                     "release time: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_legato:       printf("legato: Time: %d Port: %d Channel: %d "
                                     "legato on/off: %d\n", p->time, p->port,
                                     p->bsc, p->bsb);
                              break;
        case st_portamento:   printf("portamento: Time: %d Port: %d Channel: %d "
                                     "portamento on/off: %d\n", p->time, p->port,
                                     p->bsc, p->bsb);
                              break;
        case st_vibrato:      printf("vibrato: Time: %d Port: %d Channel: %d "
                                     "Vibrato: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_volsynthchan: printf("volsynthchan: Time: %d Port: %d Channel: %d "
                                     "Volume: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_porttime:     printf("porttime: Time: %d Port: %d Channel: %d "
                                     "Portamento time: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_balance:      printf("attack: Time: %d Port: %d Channel: %d "
                                     "Ballance: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_pan:          printf("pan: Time: %d Port: %d Channel: %d "
                                     "Pan: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_timbre:       printf("timbre: Time: %d Port: %d Channel: %d "
                                     "Timbre: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_brightness:   printf("brightness: Time: %d Port: %d Channel: %d "
                                     "Brightness: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_reverb:       printf("reverb: Time: %d Port: %d Channel: %d "
                                     "Reverb: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_tremulo:      printf("tremulo: Time: %d Port: %d Channel: %d "
                                     "Tremulo: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_chorus:       printf("chorus: Time: %d Port: %d Channel: %d "
                                     "Chorus: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_celeste:      printf("celeste: Time: %d Port: %d Channel: %d "
                                     "Celeste: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_phaser:       printf("Phaser: Time: %d Port: %d Channel: %d "
                                     "Phaser: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_aftertouch:   printf("aftertouch: Time: %d Port: %d Channel: %d "
                                     "Note: %d Aftertouch: %d\n", p->time,
                                     p->port, p->ntc, p->ntn, p->ntv);
                              break;
        case st_pressure:     printf("pressure: Time: %d Port: %d Channel: %d "
                                     "Pressure: %d\n", p->time, p->port, p->ntc,
                                     p->ntv);
                              break;
        case st_pitch:        printf("pitch: Time: %d Port: %d Channel: %d "
                                     "Pitch: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_pitchrange:   printf("pitchrange: Time: %d Port: %d Channel: %d "
                                     "Pitch range: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_mono:         printf("mono: Time: %d Port: %d Channel: %d "
                                     "Mono notes: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_poly:         printf("poly: Time: %d Port: %d Channel: %d\n",
                                     p->time, p->port, p->pc);
                              break;
        case st_playsynth:    printf("playsynth: Time: %d Port: %d "
                                     ".mid file id: %d\n", p->time, p->port,
                                     p->sid);
                              break;
        case st_playwave:     printf("playwave: Time: %d Port: %d "
                                     ".wav file logical number: %d\n", p->time,
                                     p->port, p->wt);
                              break;
        case st_volwave:      printf("volwave: Time: %d Port: %d Volume: %d\n",
                                     p->time, p->port, p->wv);
                              break;

    }

}

/*******************************************************************************

Dump sequencer list

A diagnostic, dumps the given sequencer list in ASCII.

*******************************************************************************/

static void dmpseqlst(ami_seqptr p)

{

    while (p) {

        dmpseq(p);
        p = p->next;

    }

}

/*******************************************************************************

Find elapsed millisecond time corrected

Finds the elapsed time on the Windows millisecond time, then corrects that
for 100us time.

Windows time is kept as a wrapping unsigned timer. Because add and subtract
are the same regardless of signed/unsigned, we do trial subtracts and examine
the results, instead of using comparisons, and -1 becomes the top value.

*******************************************************************************/

static DWORD difftime(DWORD rt)

{

    DWORD ct, et;

    ct = timeGetTime(); /* get current time */
    et = ct-rt; /* find difference assuming rt > ct */
    if (et < 0) et = -1-rt+ct;

    return (et*10); /* correct 1 milliseconds to 100us */

}

/*******************************************************************************

Find windows wave formats

Converts wave format word into highest format specification. Takes the bit set
for Windows possible formats, and returns the parameters from that, which are
choosen to be the most capable format.

*******************************************************************************/

void fndwavfmt(
    /* Windows format bit set */ DWORD format,
    /* sample rate (returning) */  int* rate,
    /* bit size (returning) */     int* bits,
    /* channels (returning) */     int* chan,
    /* signed (returning) */       int* sgn,
    /* float (returning) */        int* flt,
    /* big endian (returning) */   int* big
)

{

#if 0
// The 96 khz formats don't exist in mingw
    if (format & WAVE_FORMAT_96S16)
        { *rate = 96000; *chan = 2; *bits = 16; *sgn = TRUE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_96S08)
        { *rate = 96000; *chan = 2; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_96M16)
        { *rate = 96000; *chan = 2; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_96M08)
        { *rate = 96000; *chan = 2; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }
    else
#endif
    if (format & WAVE_FORMAT_4S16)
        { *rate = 44100; *chan = 2; *bits = 16; *sgn = TRUE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_4S08)
        { *rate = 44100; *chan = 2; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_4M16)
        { *rate = 44100; *chan = 1; *bits = 16; *sgn = TRUE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_4M08)
        { *rate = 44100; *chan = 1; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_2S16)
        { *rate = 22050; *chan = 2; *bits = 16; *sgn = TRUE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_2S08)
        { *rate = 22050; *chan = 2; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_2M16)
        { *rate = 22050; *chan = 1; *bits = 16; *sgn = TRUE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_2M08)
        { *rate = 22050; *chan = 1; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_1S16)
        { *rate = 11025; *chan = 2; *bits = 16; *sgn = TRUE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_1S08)
        { *rate = 11025; *chan = 2; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE;}
    else if (format & WAVE_FORMAT_1M16)
        { *rate = 11025; *chan = 1; *bits = 16; *sgn = TRUE; *flt = FALSE;
          *big = FALSE; }
    else if (format & WAVE_FORMAT_1M08)
        { *rate = 11025; *chan = 1; *bits = 8; *sgn = FALSE; *flt = FALSE;
          *big = FALSE; }

}

/*******************************************************************************

Load wave input device parameters

Checks if the input device parameters are defined, and loads them if not. We
don't require that the input device is opened to read its parameters. Because
such reads are not bracketed by open/close, we have to load them automatically.

The device entry must already be allocated.

*******************************************************************************/

static void loadinpar(int p)

{

    MMRESULT   r;
    WAVEINCAPS pwic;
    devptr     dp;

    dp = pcmin[p-1]; /* get the device */
    if (!dp->parval) {

        /* now get the dev characteristics */
        r = waveInGetDevCaps(p-1, &pwic, sizeof(WAVEINCAPS));
        if (r != MMSYSERR_NOERROR)
            error("Unable to get Midi device capabilities");
        fndwavfmt(pwic.dwFormats, &dp->rate, &dp->bits, &dp->chan, &dp->sgn,
                  &dp->flt, &dp->big);
        pcmin[p-1]->parval = TRUE; /* set values defined */

    }

}

/*******************************************************************************

Timer handler procedure

Called when the windows event timer expires, we first check if the sequencer
is still running. If not, we do nothing, because we may have been called
while the sequencer is being shut down. If it is running, we then take all
messages off the top of the queue that have become due. Timer overruns are
handled by executing all past due events, on the idea that things like volume
changes, etc, need to be performed to stay in sync. If notes are past due,
this will cause "note scramble" for a short time, and we might have to improve
this.

After all due messages are cleared, if the queue still has active messages,
then another timer is set for that new top message. This keeps the queue moving
until clear.

*******************************************************************************/

static void CALLBACK nextseq(UINT id, UINT msg, DWORD_PTR usr, DWORD_PTR dw1,
                             DWORD_PTR dw2)

{

    ami_seqptr p;    /* message entry pointer */
    DWORD     elap; /* elapsed time */

    if (seqrun) { /* sequencer is still running */

        EnterCriticalSection(&seqlock); /* start exclusive access */
        p = seqlst; /* index top of list */
        while (p) { /* process all past due messages */

            elap = difftime(strtim); /* find elapsed time */
            if (p->time <= elap) { /* execute this message */

                seqlst = seqlst->next; /* gap out */
                excseq(p); /* execute it */
                putseq(p); /* release entry */
                p = seqlst; /* index top of list again */

            } else p = NULL; /* stop search */

        }
        if (seqlst) /* start another timer */
            timhan = timeSetEvent((seqlst->time-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);
        LeaveCriticalSection(&seqlock); /* end exclusive access */

    }

}

/********************************************************************************

Find number of output midi ports

Returns the total number of output midi ports.

********************************************************************************/

int ami_synthout(void)

{

    return (midiOutGetNumDevs());

}

/*******************************************************************************

Find number of input midi ports

Returns the total number of input midi ports.

*******************************************************************************/

int ami_synthin(void)

{

    return (midiInGetNumDevs());

}

/********************************************************************************

Open synthesizer output port

Opens a syth output port. The output ports have their own separate logical
numbers separate from input numbers, and by convention, port 1 will be the
main synthesizer for the computer, and port 2 will be an output port to any
midi chained devices outside the computer.

********************************************************************************/

void ami_opensynthout(int p)

{

    /* open midi output device */
    midiOutOpen(&midouttab[p], p-1, 0, 0, CALLBACK_NULL);

}

/********************************************************************************

Close midi sythesiser output port

Closes a previously opened midi output port.

********************************************************************************/

void ami_closesynthout(int p)

{

    midiOutClose(midouttab[p]); /* close port */
    midouttab[p] = (HMIDIOUT)-1; /* set closed */

}

/********************************************************************************

Start time output

Starts the sequencer function. The sequencer is cleared, and upcount {s
after this call. Before a sequencer start, any notes marked as "sequenced" by
having a non-zero time value would cause an error. After sequencer start,
they are either:

1. Discarded if the time has already passed.

2. Send immediately if the time is now (or very close to now).

3. Buffered and scheduled to be set out at the correct time.

We mark sequencer start by recording the start base time, which is the
windows free running time that all sequencer times will be measured from.
That counter is 32 bits unsigned millisecond, which gives 49.71 days. We
take half that, because we store as signed, so we are 24.855 days full
sequencer time.

Note that there will be no sequencer events, because we don't allow them to
be set without the sequencer running. The first event will be kicked off by
a sequenced event.

********************************************************************************/

void ami_starttimeout(void)

{

    strtim = timeGetTime(); /* place current system time */
    seqrun = TRUE; /* set sequencer running */

}

/********************************************************************************

Stop time output

Stops midi sequencer function. Any timers and buffers in use by the sequencer
are cleared, and all pending events dropped.

********************************************************************************/

void ami_stoptimeout(void)

{

    ami_seqptr p; /* message pointer */

    strtim = 0; /* clear start time */
    seqrun = FALSE; /* set sequencer ! running */
    /* if there is a pending sequencer timer, kill it */
    if (timhan) timeKillEvent(timhan);
    /* now clear all pending events */
    while (seqlst) { /* clear */

        p = seqlst; /* index top of list */
        seqlst = seqlst->next; /* gap out */
        putseq(p); /* release entry */

    }

}

/********************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

********************************************************************************/

int ami_curtimeout(void)

{

    if (!seqrun) error("Sequencer not running");

    return (difftime(strtim)); /* return difference time */

}

/*******************************************************************************

Start time input

Marks the time basis for input midi streams. Normally, MIDI input streams are
marked with 0 time, which means unsequenced. However, starting the input time
will mark MIDI inputs with their arrival time, which can be used to sequence
the MIDI commands. Stopping the time will return to marking 0 time.

*******************************************************************************/

void ami_starttimein(void)

{


    sintim = timeGetTime(); /* place current system time */
    sinrun = TRUE; /* set sequencer running */

}

/*******************************************************************************

Stop time input

Simply sets that we are not marking input time anymore.

*******************************************************************************/

void ami_stoptimein(void)

{

    sintim = 0; /* clear start time */
    sinrun = FALSE; /* set sequencer ! running */

}

/*******************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************/

int ami_curtimein(void)

{

    if (!sinrun) error("Sequencer not running");

    return (difftime(sintim)); /* return difference time */

}

/********************************************************************************

Note on

Turns on a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to INT_MAX.

********************************************************************************/

void ami_noteon(int p, int t, ami_channel c, ami_note n, int v)

{

    DWORD     msg;  /* message holder */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    ami_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (v / 0x01000000)*65536+(n-1)*256+MESS_NOTE_ON+(c-1);
        midiOutShortMsg(midouttab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_noteon; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = v; /* set velocity */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Note off

Turns off a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to INT_MAX.

********************************************************************************/

void ami_noteoff(int p, int t, ami_channel c, ami_note n, int v)

{

    DWORD     msg;
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    ami_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (v / 0x01000000)*65536+(n-1)*256+MESS_NOTE_OFF+(c-1);
        midiOutShortMsg(midouttab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_noteoff; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = v; /* set velocity */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Instrument change

Selects a new instrument for the given channel. Tbe new instrument is specified
by Midi GM encoding, 1 to 128. Takes a time for sequencing.

********************************************************************************/

void ami_instchange(int p, int t, ami_channel c, ami_instrument i)

{

    DWORD     msg;
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    ami_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (i < 1 || i > 128) error("Bad instrument number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        msg = (i-1)*256+MESS_PGM_CHG+(c-1); /* construct midi message */
        midiOutShortMsg(midouttab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_instchange; /* set type */
        sp->icc = c; /* set channel */
        sp->ici = i; /* set instrument */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Controller change

Processes a controller value set, from 0 to 127.

********************************************************************************/

static void ctlchg(int p, int t, ami_channel c, int cn, int v)

{

    DWORD msg;

    /* construct midi message */
    msg = v*65536+cn*256+MESS_CTRL_CHG+(c-1);
    midiOutShortMsg(midouttab[p], msg);

}

/********************************************************************************

Set attack time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

********************************************************************************/

void ami_attack(int p, int t, ami_channel c, int at)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_ATTACK_TIME, at/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_attack; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = at; /* set attack */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/ 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set release time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

********************************************************************************/

void ami_release(int p, int t, ami_channel c, int rt)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    DWORD     tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_RELEASE_TIME, rt/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if p}ing timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_release; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = rt; /* set release */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Legato pedal on/off

Sets the legato mode on/off.

********************************************************************************/

void ami_legato(int p, int t, ami_channel c, int b)

{

    DWORD     msg;
    DWORD     elap;    /* current elapsed time */
    int       tact;    /* timer active */
    ami_seqptr sp;      /* message pointer */


    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_LEGATO_PEDAL, !!b*127);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_legato; /* set type */
        sp->bsc = c; /* set channel */
        sp->bsb = b; /* set on/off */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

   }

}

/********************************************************************************

Portamento pedal on/off

Sets the portamento mode on/off.

********************************************************************************/

void ami_portamento(int p, int t, ami_channel c, int b)

{

    DWORD     msg;
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    ami_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_PORTAMENTO, !!b*127);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_portamento; /* set type */
        sp->bsc = c; /* set channel */
        sp->bsb = b; /* set on/off */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set volume

Sets synthesizer volume, 0 to INT_MAX.

********************************************************************************/

void ami_volsynthchan(int p, int t, ami_channel c, int v)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_VOLUME_COARSE, v/0x01000000); /* set high */
        ctlchg(p, t, c, CTLR_VOLUME_FINE, v/0x00020000 && 0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_volsynthchan; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set volume */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

};

/********************************************************************************

Set left right channel balance

Sets sets the left right channel balance. -INT_MAX is all left, 0 is center,
INT_MAX is all right.

********************************************************************************/

void ami_balance(int p, int t, ami_channel c, int b)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */


    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        b = b/0x00040000+0x2000; /* reduce to 14 bits, positive only */
        ctlchg(p, t, c, CTLR_BALANCE_COARSE, b / 0x80); /* set high */
        ctlchg(p, t, c, CTLR_BALANCE_FINE, b && 0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_balance; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set balance */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set portamento time

Sets portamento time, 0 to INT_MAX.

********************************************************************************/

void ami_porttime(int p, int t, ami_channel c, int v)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_PORTAMENTO_TIME_COARSE, v/0x01000000); /* set high */
        /* set low */
        ctlchg(p, t, c, CTLR_PORTAMENTO_TIME_FINE, v/0x00020000 && 0x7f);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_porttime; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set time */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

   }

}

/********************************************************************************

Set vibrato

Sets modulaton value, 0 to INT_MAX.

********************************************************************************/

void ami_vibrato(int p, int t, ami_channel c, int v)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_MODULATION_WHEEL_COARSE, v/0x01000000); /* set high */
        /* set low */
        ctlchg(p, t, c, CTLR_MODULATION_WHEEL_FINE, v/0x00020000 && 0x7f);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_vibrato; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set vibrato */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set left/right pan position

Sets sets the left/right pan position. -INT_MAX is hard left, 0 is center,
INT_MAX is hard right.

********************************************************************************/

void ami_pan(int p, int t, ami_channel c, int b)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        b = b / 0x00040000+0x2000; /* reduce to 14 bits, positive only */
        ctlchg(p, t, c, CTLR_PAN_POSITION_COARSE, b/0x80); /* set high */
        ctlchg(p, t, c, CTLR_PAN_POSITION_FINE, b&0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pan; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set pan */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound timbre

Sets the sound timbre, 0 to INT_MAX.

********************************************************************************/

void ami_timbre(int p, int t, ami_channel c, int tb)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_TIMBRE, tb/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_timbre; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = tb; /* set timbre */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                    TIME_CALLBACK_FUNCTION |
                                    TIME_KILL_SYNCHRONOUS |
                                    TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound brightness

Sets the sound brightness, 0 to INT_MAX.

********************************************************************************/

void ami_brightness(int p, int t, ami_channel c, int b)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_BRIGHTNESS, b/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_brightness; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set brightness */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound reverb

Sets the sound reverb, 0 to INT_MAX.

********************************************************************************/

void ami_reverb(int p, int t, ami_channel c, int r)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_EFFECTS_LEVEL, r/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_reverb; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = r; /* set reverb */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound tremulo

Sets the sound tremulo, 0 to INT_MAX.

********************************************************************************/

void ami_tremulo(int p, int t, ami_channel c, int tr)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_TREMULO_LEVEL, tr/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_tremulo; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = tr; /* set tremulo */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound chorus

Sets the sound chorus, 0 to INT_MAX.

********************************************************************************/

void ami_chorus(int p, int t, ami_channel c, int cr)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_CHORUS_LEVEL, cr/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_chorus; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = cr; /* set chorus */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound celeste

Sets the sound celeste, 0 to INT_MAX.

********************************************************************************/

void ami_celeste(int p, int t, ami_channel c, int ce)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_CELESTE_LEVEL, ce/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_celeste; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ce; /* set celeste */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound phaser

Sets the sound phaser, 0 to INT_MAX.

********************************************************************************/

void ami_phaser(int p, int t, ami_channel c, int ph)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_PHASER_LEVEL, ph/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_phaser; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ph; /* set phaser */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set pitch range

Sets the range of pitch that can be reached by the pitch adjustment. The range
is from 0 to INT_MAX, && represents from from 0 to 127 semitones. This means
that a setting of INT_MAX means that every note in the midi scale can be reached,
with an accuracy of 128 of the space from C to C#. At this setting, any note
could be reached with a slide, for example.

********************************************************************************/

void ami_pitchrange(int p, int t, ami_channel c, int v)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* set up data entry */
        ctlchg(p, t, c, CTLR_REGISTERED_PARAMETER_COARSE, 0); /* set high */
        ctlchg(p, t, c, CTLR_REGISTERED_PARAMETER_FINE, 0); /* set low */
        ctlchg(p, t, c, CTLR_DATA_ENTRY_COARSE, v/0x01000000); /* set high */
        ctlchg(p, t, c, CTLR_DATA_ENTRY_FINE, v/0x00020000 & 0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pitchrange; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set pitchrange */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set monophonic mode

If ommni is off, this sets how many channels to respond to. If omni is on,
then only once note at a time will be played. The select is from 0 to 16,
with 0 being "allways select single note mode".

********************************************************************************/

void ami_mono(int p, int t, ami_channel c, int ch)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    if (ch < 0 || c > 16) error("Bad mono mode number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_MONO_OPERATION, ch);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_mono; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ch; /* set mono mode */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set polyphonic mode

Reenables polyphonic mode after a monophonic operation.

********************************************************************************/

void ami_poly(int p, int t, ami_channel c)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_POLY_OPERATION, 0); /* value dosen't matter */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_poly; /* set type */
        sp->pc = c; /* set channel */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Aftertouch

Controls aftertouch, 0 to INT_MAX, on a note.

********************************************************************************/

void ami_aftertouch(int p, int t, ami_channel c, ami_note n, int at)

{

    DWORD msg;
    ami_seqptr sp; /* message pointer */
    DWORD elap;   /* current elapsed time */
    int tact;     /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (at/0x01000000)*65536+(n-1)*256+MESS_AFTTCH+(c-1);
        midiOutShortMsg(midouttab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_aftertouch; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = at; /* set aftertouch */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Channel pressure

Controls channel pressure, 0 to INT_MAX, on a note.

********************************************************************************/

void ami_pressure(int p, int t, ami_channel c, int pr)

{

    DWORD     msg;
    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (pr/0x01000000)*256+MESS_CHN_PRES+(c-1);
        midiOutShortMsg(midouttab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pressure; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntv = pr; /* set pressure */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set pitch wheel

Sets the pitch wheel value, from 0 to INT_MAX. This is the amount off the note
in the channel. The GM standard is to adjust for a whole step up && down, which
is 4 half steps total. A "half step" is the difference between, say, C && C#.

********************************************************************************/

void ami_pitch(int p, int t, ami_channel c, int pt)

{

    DWORD     msg;
    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        pt = pt/0x00040000+0x2000; /* reduce to 14 bits, positive only */
        /* construct midi message */
        msg = (pt/0x80)*65536+(pt && 0x7f)*256+MESS_PTCH_WHL+(c-1);
        midiOutShortMsg(midouttab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pitch; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = pt; /* set pitch */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/*******************************************************************************

Load synthesizer file

Loads a synthesizer control file, usually midi format, into a logical cache,
from 1 to N. These are loaded up into memory for minimum latency. The file is
specified by file name, and the file type is system dependent.

Note that we support 100 synth files loaded, but the Petit-ami "rule of thumb"
is no more than 10 synth files at a time.

Windows does not need to preload files (although we could find later that a
delay to play might require it). This implementation just saves the name for
the subsequent play operation.

*******************************************************************************/

void ami_loadsynth(int s, string fn)

{

    if (s < 1 || s > MAXMIDT) error("Invalid logical synthesizer file number");
    if (synthnam[s-1])
        error("Synthesizer file already defined for logical number");
    /* simply store the filename for later */
    synthnam[s-1] = malloc(strlen(fn)+1); /* get a name entry */
    if (!synthnam[s-1]) error("No memory");
    strcpy(synthnam[s-1], fn);

}

/*******************************************************************************

Delete synthesizer file

Removes a synthesizer file from the caching table. This frees up the entry to be
redefined.

Attempting to delete a synthesizer entry that is actively being played will
result in this routine blocking until it is complete.

*******************************************************************************/

void ami_delsynth(int s)

{

    if (s < 1 || s > MAXMIDT) error("Invalid logical synthesizer file number");
    if (!synthnam[s-1])
        error("No synthesizer file loaded for logical number");
    free(synthnam[s-1]);

}

/********************************************************************************

Play synthesizer file

Plays the waveform file to the indicated midi device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows midi files to be sequenced against other wave files and midi files.
The file is specified by file name, end the file type is system dependent.
This version uses the string send MCI command, one of the simplest ways to do
that. There does not seem to be an obvious way to relate the MCI devices to the
midi devices.

Windows cannot play more than one midi file at a time (although it can layer
one wave with one midi). Also, a midi open/close sequence like we use here will
fail if the default synth is open. We handle this by closing the default if
it is open, then reopening it afterwards.

********************************************************************************/

void ami_playsynth(int p, int t, int s)

{

    char      sfb[100], b[100];
    int       x;
    MCIERROR  r; /* return */
    char      fp[100], fn[100], fe[100]; /* filename components */
    ami_seqptr sp;  /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (p != 1) error("Must execute play on default output channel");
    if (midouttab[p] < 0) error("Synth output channel not open");
    if (s < 1 || s > MAXMIDT) error("Invalid logical synthesizer file number");
    if (!synthnam[s-1])
        error("No synthesizer file loaded for logical wave number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ami_closesynthout(1); /* close default output */
        /* break filename components */
        brknam(synthnam[s-1], fp, 100, fn, 100, fe, 100);
        if (fe[0] == ' ') strcpy(fe, "mid");
        maknam(sfb, 100, fp, fn, fe);
        mciSendString("close midi", NULL, 0, NULL);
        strcpy(b, "open ");
        strcat(b, sfb);
        strcat(b, " alias midi");
        mciSendString(b, NULL, 0, NULL);
        mciSendString("play midi", NULL, 0, NULL);
        ami_opensynthout(1); /* reopen default midi */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_playsynth; /* set type */
        sp->sid = s; /* place synth file id */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/*******************************************************************************

Wait synthesizers complete

Waits for all running sequencers to complete before returning.
The synthesizers all play on a separate thread. Normally, if the parent program
exits before the threads all complete, the synth plays stop, and this is usually
the correct behavior. However, in some cases we want the synth sequencers to
complete. for example a "play mymidi.mid" command should wait until the synth
sequencers finish.

This routine waits until ALL synth operations complete. There is no way to
determine indivudual sequencer completions, since even the same track could have
mutiple plays active at the same time. So we keep a counter of active plays, and
wait until they all stop.

The active sequencer count includes all sequencers, including syth (midi) file
plays and individual notes/events sent to the midi "manual" interface. Thus this
call only makes sense if the calling program has stopped sending events to the
sequencer(s), including background tasks.

*******************************************************************************/

void ami_waitsynth(int p)

{

    TCHAR status[100];

    /* This is a real waste of time but it works. Looking for a better
       solution */
    do { mciSendString("status midi mode", status, 100, NULL);
    } while (!strcmp(status, "playing"));

}

/********************************************************************************

Find number of wave devices.

Returns the number of wave output devices available.

********************************************************************************/

int ami_waveout(void)

{

    return (waveOutGetNumDevs());

}

/*******************************************************************************

Find number of input wave devices.

Returns the number of wave output devices available.

*******************************************************************************/

int ami_wavein(void)

{

    return (waveInGetNumDevs());

}

/********************************************************************************

Open wave output device

Opens a wave output device by number. By convention, wave out 1 is the default
output device. This is presently a no-op for windows.

********************************************************************************/

void ami_openwaveout(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    makpcmout(p); /* ensure device exists */
    if (pcmout[p-1]->open) error("Wave output device is already open");
    pcmout[p-1]->open = TRUE; /* set device open */
    pcmout[p-1]->init = FALSE; /* set device not initialized */

}

/********************************************************************************

CLose wave output device

Closes a wave output device by number. This is presently a no-op for windows.

********************************************************************************/

void ami_closewaveout(int p)

{

    MMRESULT r;

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    if (!pcmout[p-1] || !pcmout[p-1]->open)
        error("Wave output device is not open");
    /* close the device */
    r = waveOutClose(pcmout[p-1]->hwo);
    if (r != MMSYSERR_NOERROR) error("Couldn't close wave output device");
    pcmout[p-1]->open = FALSE; /* set not open */


}

/*******************************************************************************

Load waveform file

Loads a waveform file to a logical cache, from 1 to N. These are loaded up into
memory for minimum latency. The file is specified by file name, and the file
type is system dependent.

Note that we support 100 wave files loaded, but the Petit-ami "rule of thumb"
is no more than 10 wave files at a time.

Note that at present, we don't implement wave caching. This is mainly because on
the test system, the latency to play is acceptable.

*******************************************************************************/

void ami_loadwave(int w, string fn)

{

    if (w < 1 || w > MAXWAVT) error("Invalid logical wave file number");
    if (synthnam[w-1])
        error("Wave file already defined for logical number");
    /* simply store the filename for later */
    wavenam[w-1] = malloc(strlen(fn)+1); /* get a name entry */
    if (!wavenam[w-1]) error("No memory");
    strcpy(wavenam[w-1], fn);

}

/*******************************************************************************

Delete waveform file

Removes a waveform file from the caching table. This frees up the entry to be
redefined.

*******************************************************************************/

void ami_delwave(int w)

{

    if (w < 1 || w > MAXWAVT) error("Invalid logical wave file number");
    if (!wavenam[w-1])
        error("No wave file loaded for logical number");
    free(synthnam[w-1]);

}

/********************************************************************************

Play waveform file

Plays the waveform file to the indicated wave device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows wave files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.

********************************************************************************/

static DWORD WINAPI waveplaythread(LPVOID lpParameter)

{

    int w = (int)lpParameter; /* get wave number */

    /* play sound without error checking, wait complete */
    PlaySound(wavenam[w-1], 0, SND_FILENAME | SND_NODEFAULT | SND_SYNC);
    wavcnt--; /* set wave complete */
    SetEvent(playwavecomplete); /* flag play has finished */

}

void ami_playwave(int p, int t, int w)

{

    ami_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    BOOL      b;

    if (w < 1 || w > MAXWAVT) error("Invalid logical wave file number");
    if (!wavenam[w-1])
        error("No wave file loaded for logical number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        CreateThread(NULL, 0, waveplaythread, (void*)w, 0, NULL);
        wavcnt++; /* count active wave starts */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_playwave; /* set type */
        sp->wt = w; /* set logical track */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Adjust waveform volume

Adjusts the volume on waveform playback. The volume value is from 0 to INT_MAX.

********************************************************************************/

void ami_volwave(int p, int t, int v)

{

    error("ami_wolwave: Is not implemented");


}

/*******************************************************************************

Wait waves complete

Waits for all pending wave out operations to complete before returning.
Wavefiles all play on a separate thread. Normally, if the parent program exits
before the threads all complete, the wave plays stop, and this is usually the
correct behavior. However, in some cases we want the wave to complete. for
example a "play mywave.wav" command should wait until the wave finishes.

This routine waits until ALL wave operations complete. There is no way to
determine indivudual wave completions, since even the same track could have
mutiple plays active at the same time. So we keep a counter of active plays, and
wait until they all stop.

*******************************************************************************/

void ami_waitwave(int p)

{

    DWORD r;

    /* this is MT questionable */
    while (wavcnt) {

        WaitForSingleObject(playwavecomplete, -1); /* wait for thread to start */
        if (r == -1) error("Could not wait for wave complete");

    }
    ResetEvent(playwavecomplete); /* clear event */

}

/*******************************************************************************

Set the number of channels for a wave output device

The given port will have its channel number set from the provided number. It
must be a wave output port, and it must be open. Wave samples are always output
in 24 bit samples, and the channels are interleaved. This means that the
presentation will have channel 1 first, followed by channel 2, etc, then repeat
for the next sample.

*******************************************************************************/

void ami_chanwaveout(int p, int c)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    if (!pcmout[p-1] || !pcmout[p-1]->open)
        error("Wave output device is not open");
    pcmout[p-1]->chan = c; /* set channel count */

}

/*******************************************************************************

Set the rate for a wave output device

The given port will have its rate set from the provided number, which is the
number of samples per second that will be output. It must be a wave output port,
and it must be open. Output samples are retimed for the rate. No matter how much
data is written, each sample is timed to output at the given rate, buffering as
required.

*******************************************************************************/

void ami_ratewaveout(int p, int r)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    if (!pcmout[p-1] || !pcmout[p-1]->open)
        error("Wave output device is not open");
    pcmout[p-1]->rate = r; /* set sample rate */

}

/*******************************************************************************

Set bit length for output wave device

The given port has its bit length for samples set. Bit lengths are rounded up
to the nearest byte. At the present time, bit numbers that are not divisible by
8 are not supported, but the most likely would be to zero or 1 pad the msbs
depending on signed status, effectively extending the samples. Thus the bit
cound would mainly indicate precision only.

*******************************************************************************/

void ami_lenwaveout(int p, int l)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    if (!pcmout[p-1] || !pcmout[p-1]->open)
        error("Wave output device is not open");
    pcmout[p-1]->bits = l; /* set bit length */

}

/*******************************************************************************

Set sign of wave output device samples

The given port has its signed/no signed status changed. Note that all floating
point formats are inherently signed.

*******************************************************************************/

void ami_sgnwaveout(int p, int s)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    if (!pcmout[p-1] || !pcmout[p-1]->open)
        error("Wave output device is not open");
    pcmout[p-1]->sgn = !!s; /* set signed status */

}

/*******************************************************************************

Set floating/non-floating point format

Sets the floating point/integer format for output sound samples.

*******************************************************************************/

void ami_fltwaveout(int p, int f)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    if (!pcmout[p-1] || !pcmout[p-1]->open)
        error("Wave output device is not open");
    pcmout[p-1]->flt = !!f; /* set floating point status */

}

/*******************************************************************************

Set big/little endian format

Sets the big or little endian format for an output wave device. It is possible
that an installation is fixed to the endian format of the host machine, in which
case it is an error to set a format that is different.

*******************************************************************************/

void ami_endwaveout(int p, int e)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    if (!pcmout[p-1] || !pcmout[p-1]->open)
        error("Wave output device is not open");
    pcmout[p-1]->big = !!e; /* set endian status */

}

/*******************************************************************************

Write wave data output

Writes a buffer of wave data to the given output wave port. The data must be
formatted according to the number of channels. It must be a wave output port,
and it must be open. Wave samples are always output in 24 bit samples, and the
channels are interleaved. This means that the presentation will have channel
1 first, followed by channel 2, etc, then repeat for the next sample. The
samples are converted as required from the 24 bit, big endian samples as
required. The can be up or down converted according to size, and may be
converted to floating point.

This package does not control buffering dept. As much buffering a needed to keep
the data is used without regard for realtime concerns. The necessary buffering
for real time is implemented by the user of this package, which is generally
recommended to be 1ms or less (64 samples at a 44100 sample rate).

Windows note: using a demo program, it was determined that 26ms is the minimum
for the waveOut interface API we are using.

*******************************************************************************/

void ami_wrwave(int p, byte* buff, int len)

{

    MMRESULT     r;
    WAVEFORMATEX wf;
    int          bytes;
    LPWAVEHDR    wp;
    devptr       dp;
    unsigned*    rem;

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port number");
    if (p > waveOutGetNumDevs()) error("No system wave output device exists");
    dp = pcmout[p-1]; /* index the device */
    if (!dp || !dp->open) error("Wave output device not open");
    if (!dp->init) {

        /* first operation on device, set parameters and open */
        wf.wFormatTag = WAVE_FORMAT_PCM; /* set one or two channel format */
        wf.nChannels = dp->chan; /* set number of channels */
        wf.nSamplesPerSec = dp->rate; /* set sample rate */
        /* set block alignment */
        bytes = dp->bits/8; /* find bytes per sample */
        if (dp->bits&8) bytes++; /* round  up */
        bytes *= dp->chan; /* find total channels size */
        dp->ssiz = bytes; /* save sample size */
        wf.nBlockAlign = bytes; /* set block align */
        wf.nAvgBytesPerSec = bytes*dp->rate; /* find average bytes/sec */
        wf.wBitsPerSample = dp->bits; /* set bits per sample */
        wf.cbSize = 0; /* no added information */
#if 0
dbg_printf(dlinfo, "Channels: %d\n", wf.nChannels);
dbg_printf(dlinfo, "nSamplesPerSec: %d\n", wf.nSamplesPerSec);
dbg_printf(dlinfo, "nAvgBytesPerSec: %d\n", wf.nAvgBytesPerSec);
dbg_printf(dlinfo, "nBlockAlign: %d\n", wf.nBlockAlign);
dbg_printf(dlinfo, "wBitsPerSample: %d\n", wf.wBitsPerSample);
#endif
        /* open wave out channel */
        r = waveOutOpen(&dp->hwo, p-1, &wf, 0, 0, CALLBACK_NULL);
        if (r != MMSYSERR_NOERROR) error("Couldn't open wave output device");
        dp->init = TRUE; /* set initialized */
        dp->oddevn = FALSE; /* set even buffer active */

        /* initialize 2 headers */
        wp = &dp->hdre; /* index even header */
        wp->lpData = NULL; /* clear buffer */
        wp->dwBufferLength = 0; /* clear length */
        wp->dwBytesRecorded = 0;
        wp->dwUser = 0;
        wp->dwFlags = 0; /* clear flags */
        wp->dwLoops = 1;
        wp->lpNext = NULL;
        wp->reserved = 0;
        wp->lpData = malloc(WAVSIZ); /* get new buffer */
        if (!wp->lpData) error("No memory");
        wp->dwBufferLength = WAVSIZ; /* place length */
        r = waveOutPrepareHeader(dp->hwo, wp, sizeof(WAVEHDR));
        if (r != MMSYSERR_NOERROR)
            error("Couldn't prepare wave output device header");
        dp->reme = 0; /* arm the buffer */
        wp->dwFlags |= WHDR_DONE; /* set buffer has finished */

        wp = &dp->hdro; /* index odd header */
        wp->lpData = NULL; /* clear buffer */
        wp->dwBufferLength = 0; /* clear length */
        wp->dwBytesRecorded = 0;
        wp->dwUser = 0;
        wp->dwFlags = WHDR_DONE; /* clear flags, but set done */
        wp->dwLoops = 1;
        wp->lpNext = NULL;
        wp->reserved = 0;
        wp->lpData = malloc(WAVSIZ); /* get new buffer */
        if (!wp->lpData) error("No memory");
        wp->dwBufferLength = WAVSIZ; /* place length */
        r = waveOutPrepareHeader(dp->hwo, wp, sizeof(WAVEHDR));
        if (r != MMSYSERR_NOERROR)
            error("Couldn't prepare wave output device header");
        dp->remo = 0; /* arm the buffer */
        wp->dwFlags |= WHDR_DONE; /* set buffer has finished */

    }
    len *= dp->ssiz; /* scale length by sample size to find bytes */
    while (len) { /* until read is satisfied */

        if (dp->oddevn) { wp = &dp->hdro; rem = &dp->remo; } /* index odd header */
        else { wp = &dp->hdre; rem = &dp->reme; } /* index even header */
        /* if buffer is busy, wait until finished. This should be a signaled wait */
        while (!(wp->dwFlags & WHDR_DONE));
        /* divide into partial buffer writes vs. whole buffer writes */
        if (wp->dwBufferLength-*rem >= len) {

           /* copy outgoing data from buffer */
            memcpy(wp->lpData+*rem, buff, len);
            *rem += len;
            len = 0;
            if (wp->dwBufferLength == *rem)
                /* buffer filled, move on to next buffer */
                dp->oddevn = !dp->oddevn; /* flip the buffers */

        } else {

            /* move whole buffer from caller */
            memcpy(wp->lpData+*rem, buff, wp->dwBufferLength-*rem);
            len -= wp->dwBufferLength-*rem;
            /* move on to next buffer */
            dp->oddevn = !dp->oddevn; /* flip the buffers */

        }
        /* if the buffer is full up, write out */
        if (*rem >= wp->dwBufferLength) {

            r = waveOutWrite(dp->hwo, wp, sizeof(WAVEHDR));
            if (r != MMSYSERR_NOERROR)
                error("Couldn't write wave output device");
            *rem = 0; /* reset remainder */

        }

    }

}

/*******************************************************************************

Open wave input device

Opens a wave output device by number. By convention, wave in 1 is the default
input device. The wave stream parameters are determined during enumeration,
but we assert them here on open.

*******************************************************************************/

void ami_openwavein(int p)

{

    devptr     dp;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    makpcmin(p); /* ensure device exists */
    dp = pcmin[p-1]; /* get device */
    if (pcmin[p-1]->open) error("Wave input device is already open");
    pcmin[p-1]->open = TRUE; /* set device open */
    pcmin[p-1]->init = FALSE; /* set device not initialized */
    loadinpar(p); /* validate parameters */

}

/*******************************************************************************

CLose wave input device

Closes a wave input device by number. This is presently a no-op for linux.

*******************************************************************************/

void ami_closewavein(int p)

{

    MMRESULT r;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    if (!pcmin[p-1] || !pcmin[p-1]->open)
        error("Wave input device is not open");
    /* close the device */
    if (pcmin[p-1]->hwi) {

        r = waveInClose(pcmin[p-1]->hwi);
        if (r != MMSYSERR_NOERROR) error("Couldn't close wave input device");

    }
    pcmin[p-1]->open = FALSE; /* set not open */

}

/*******************************************************************************

Get the number of channels for a wave input device

The given port will have its channel number read and returned. It must be a wave
input port, and it must be open. Wave samples are always input in 24 bit
samples, and the channels are interleaved. This means that the presentation will
have channel 1 first, followed by channel 2, etc, then repeat for the next
sample.

*******************************************************************************/

int ami_chanwavein(int p)

{

    devptr dp;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    makpcmin(p); /* ensure device exists */
    loadinpar(p); /* validate parameters */
    dp = pcmin[p-1]; /* get device */

    return (dp->chan);

}

/*******************************************************************************

Get the rate for a wave input device

The given port will have its rate read and returned, which is the
number of samples per second that will be input. It must be a wave output port,
and it must be open. Input samples are timed at the rate.

Windows does not tell us the native format of devices, so we choose an "ideal".

*******************************************************************************/

int ami_ratewavein(int p)

{

    devptr dp;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    makpcmin(p); /* ensure device exists */
    loadinpar(p); /* validate parameters */
    dp = pcmin[p-1]; /* get device */

    return (dp->rate);

}

/*******************************************************************************

Get the bit length for a wave input device

Returns the number of bits for a given input wave device. To find the number of
bytes required for the format is:

bytes = bits/8;
if (bits%8) bytes++;

This is then multipled by the number of channels to determine the size of a
"sample", or unit of sound data transfer.

At present, only whole byte format samples are supported, IE., 8, 16, 24, 32,
etc. However, the caller should assume that partial bytes can be used and thus
round up bit lengths as shown above.

Windows does not tell us the native format of devices, so we choose an "ideal".

*******************************************************************************/

int ami_lenwavein(int p)

{

    devptr dp;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    makpcmin(p); /* ensure device exists */
    loadinpar(p); /* validate parameters */
    dp = pcmin[p-1]; /* get device */

    return (dp->bits);

}

/*******************************************************************************

Get signed status of wave input device

Returns true if the given wave input device has signed sampling. Note that
signed sampling is always true if the samples are floating point.

*******************************************************************************/

int ami_sgnwavein(int p)

{

    devptr dp;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    makpcmin(p); /* ensure device exists */
    loadinpar(p); /* validate parameters */
    dp = pcmin[p-1]; /* get device */

    return (dp->sgn);

}

/*******************************************************************************

Get big endian status of wave input device

Returns true if the given wave input device has big endian sampling.

*******************************************************************************/

int ami_endwavein(int p)

{

    devptr dp;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    makpcmin(p); /* ensure device exists */
    loadinpar(p); /* validate parameters */
    dp = pcmin[p-1]; /* get device */

    return (dp->big);

}

/*******************************************************************************

Get floating point status of wave input device

Returns true if the given wave input device has floating point sampling.

*******************************************************************************/

int ami_fltwavein(int p)

{

    devptr dp;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    makpcmin(p); /* ensure device exists */
    loadinpar(p); /* validate parameters */
    dp = pcmin[p-1]; /* get device */

    return (dp->flt);

}

/*******************************************************************************

Read wave data input

Reads a buffer of wave data from the given input wave port. The data is
formatted according to the number of channels. It must be a wave input port,
and it must be open. Wave samples are always input in 24 bit samples, and the
channels are interleaved. This means that the presentation will have channel
1 first, followed by channel 2, etc, then repeat for the next sample. The
samples are converted as required from the 24 bit, big endian samples as
required. The can be up or down converted according to size, and may be
converted to floating point.

The input device will be buffered according to the parameters of the input
device and/or the device software. The hardware will have a given buffering
amount, and the driver many change that to be higher or lower. Generally the
buffering will be designed to keep the buffer latency below 1ms (64 samples at a
44100 sample rate). Because the exact sample rate is unknown, the caller is
recommended to provide a buffer that is greater than any possible buffer amount.
Thus (for example) 1024*channels would be an appropriate buffer size. Not
providing enough buffering will not cause an error, but will cause the read
rate to fall behind the data rate.

ami_rdwave() will return the actual number of bytes read, which will contain
3*ami_chanwavein() bytes of samples. This will then be the actual buffer content.

*******************************************************************************/

int ami_rdwave(int p, byte* buff, int len)

{

    MMRESULT     r;
    WAVEFORMATEX wf;
    int          bytes;
    LPWAVEHDR    wp;
    devptr       dp;
    unsigned*    rem;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port number");
    if (p > waveInGetNumDevs()) error("No system wave input device exists");
    dp = pcmin[p-1]; /* index the device */
    if (!dp || !dp->open) error("Wave input device not open");
    if (!dp->init) {

        /* first operation on device, set parameters and open */
        wf.wFormatTag = WAVE_FORMAT_PCM; /* set one or two channel format */
        wf.nChannels = dp->chan; /* set number of channels */
        wf.nSamplesPerSec = dp->rate; /* set sample rate */
        /* set block alignment */
        bytes = dp->bits/8; /* find bytes per sample */
        if (dp->bits&8) bytes++; /* round  up */
        bytes *= dp->chan; /* find total channels size */
        dp->ssiz = bytes; /* save sample size */
        wf.nBlockAlign = bytes; /* set block align */
        wf.nAvgBytesPerSec = bytes*dp->rate; /* find average bytes/sec */
        wf.wBitsPerSample = dp->bits; /* set bits per sample */
        wf.cbSize = 0; /* no added information */
#if 0
dbg_printf(dlinfo, "Channels: %d\n", wf.nChannels);
dbg_printf(dlinfo, "nSamplesPerSec: %d\n", wf.nSamplesPerSec);
dbg_printf(dlinfo, "nAvgBytesPerSec: %d\n", wf.nAvgBytesPerSec);
dbg_printf(dlinfo, "nBlockAlign: %d\n", wf.nBlockAlign);
dbg_printf(dlinfo, "wBitsPerSample: %d\n", wf.wBitsPerSample);
#endif
        /* open wave out channel */
        r = waveInOpen(&dp->hwi, p-1, &wf, 0, 0, CALLBACK_NULL);
        if (r != MMSYSERR_NOERROR) error("Couldn't open wave input device");
        dp->init = TRUE; /* set initialized */
        dp->oddevn = FALSE; /* set even buffer active */

        /* initialize 2 headers */
        wp = &dp->hdre; /* index even header */
        wp->lpData = NULL; /* clear buffer */
        wp->dwBufferLength = 0; /* clear length */
        wp->dwBytesRecorded = 0;
        wp->dwUser = 0;
        wp->dwFlags = 0; /* clear flags */
        wp->dwLoops = 1;
        wp->lpNext = NULL;
        wp->reserved = 0;
        wp->lpData = malloc(WAVSIZ); /* get new buffer */
        if (!wp->lpData) error("No memory");
        wp->dwBufferLength = WAVSIZ; /* place length */
        r = waveInPrepareHeader(dp->hwi, wp, sizeof(WAVEHDR));
        if (r != MMSYSERR_NOERROR)
            error("Couldn't prepare wave input device header");
        /* note we preload the buffers into the input stream, in even then odd
           order  */
        r = waveInAddBuffer(dp->hwi, wp, sizeof(WAVEHDR));
        if (r != MMSYSERR_NOERROR) error("Couldn't add buffer to wave input device");
        dp->reme = 0; /* arm the buffer */

        wp = &dp->hdro; /* index odd header */
        wp->lpData = NULL; /* clear buffer */
        wp->dwBufferLength = 0; /* clear length */
        wp->dwBytesRecorded = 0;
        wp->dwUser = 0;
        wp->dwFlags = 0; /* clear flags */
        wp->dwLoops = 1;
        wp->lpNext = NULL;
        wp->reserved = 0;
        wp->lpData = malloc(WAVSIZ); /* get new buffer */
        if (!wp->lpData) error("No memory");
        wp->dwBufferLength = WAVSIZ; /* place length */
        r = waveInPrepareHeader(dp->hwi, wp, sizeof(WAVEHDR));
        if (r != MMSYSERR_NOERROR)
            error("Couldn't prepare wave input device header");
        r = waveInAddBuffer(dp->hwi, wp, sizeof(WAVEHDR));
        if (r != MMSYSERR_NOERROR) error("Couldn't add buffer to wave output device");
        dp->remo = 0; /* arm the buffer */

        /* start input channel */
        r = waveInStart(dp->hwi);
        if (r != MMSYSERR_NOERROR) error("Couldn't add buffer to wave input device");

    }
    len *= dp->ssiz; /* scale length by sample size to find bytes */
    while (len) { /* until read is satisfied */

        if (dp->oddevn) { wp = &dp->hdro; rem = &dp->remo; } /* index odd header */
        else { wp = &dp->hdre; rem = &dp->reme; } /* index even header */
        /* if buffer is empty, wait until full. This should be a signaled wait */
        while (!(wp->dwFlags & WHDR_DONE));
        /* divide into partial buffer reads vs. whole buffer reads */
        if (wp->dwBufferLength-*rem >= len) {

            memcpy(buff, wp->lpData+*rem, len); /* copy incoming data to buffer */
            *rem += len;
            len = 0;
            if (wp->dwBufferLength == *rem)
                /* data used, move on to next buffer */
                dp->oddevn = !dp->oddevn; /* flip the buffers */

        } else {

            /* move whole buffer to caller */
            memcpy(buff, wp->lpData+*rem, wp->dwBufferLength-*rem);
            len -= wp->dwBufferLength-*rem;
            /* move on to next buffer */
            dp->oddevn = !dp->oddevn; /* flip the buffers */

        }
        /* if the buffer is used up, rearm */
        if (*rem >= wp->dwBufferLength) {

            r = waveInAddBuffer(dp->hwi, wp, sizeof(WAVEHDR));
            if (r != MMSYSERR_NOERROR)
                error("Couldn't add buffer to wave input device");
            *rem = 0; /* reset remainder */

        }

    }

}

/*******************************************************************************

Find device name of synthesizer output port

Returns the ALSA device name of the given synthsizer output port.

*******************************************************************************/

void ami_synthoutname(int p, string name, int len)

{

    MMRESULT r;
    MIDIOUTCAPS pmoc;

    r = midiOutGetDevCaps(p-1, &pmoc, sizeof(MIDIOUTCAPS));
    if (r != MMSYSERR_NOERROR) error("Unable to get Midi device capabilities");
    if (strlen(pmoc.szPname)+1 > len) error("String to large for destination");
    strcpy(name, pmoc.szPname);

}

/*******************************************************************************

Find device name of synthesizer input port

Returns the ALSA device name of the given synthsizer input port.

*******************************************************************************/

void ami_synthinname(int p, string name, int len)

{

    MMRESULT r;
    MIDIINCAPS pmic;

    r = midiInGetDevCaps(p-1, &pmic, sizeof(MIDIINCAPS));
    if (r != MMSYSERR_NOERROR) error("Unable to get Midi device capabilities");
    if (strlen(pmic.szPname)+1 > len) error("String to large for destination");
    strcpy(name, pmic.szPname);

}

/*******************************************************************************

Find device name of wave output port

Returns the ALSA device name of the given wave output port.

*******************************************************************************/

void ami_waveoutname(int p, string name, int len)

{

    MMRESULT r;
    WAVEOUTCAPS pwoc;

    r = waveOutGetDevCaps(p-1, &pwoc, sizeof(WAVEOUTCAPS));
    if (r != MMSYSERR_NOERROR) error("Unable to get wave device capabilities");
    if (strlen(pwoc.szPname)+1 > len) error("String to large for destination");
    strcpy(name, pwoc.szPname);

}

/*******************************************************************************

Find device name of wave input port

Returns the ALSA device name of the given wave input port.

*******************************************************************************/

void ami_waveinname(int p, string name, int len)

{

    MMRESULT r;
    WAVEINCAPS pwic;

    r = waveInGetDevCaps(p-1, &pwic, sizeof(WAVEINCAPS));
    if (r != MMSYSERR_NOERROR) error("Unable to get Midi device capabilities");
    if (strlen(pwic.szPname)+1 > len) error("String to large for destination");
    strcpy(name, pwic.szPname);

}

/*******************************************************************************

MIDI input callback

The callback messages important events on the MIDI device. We use it to enter
MIDI data messages into a queue.
Open a synthesizer input port.

The given synthesizer port is opened and ready for reading.

*******************************************************************************/

void CALLBACK MidiInProc(
   HMIDIIN   hMidiIn,
   UINT      wMsg,
   DWORD_PTR dwInstance,
   DWORD_PTR dwParam1,
   DWORD_PTR dwParam2
)

{

    midinpptr dp;
    int       nxtinp;

    if (wMsg == MIM_DATA) {

//        dbg_printf("Port: %d time: %ld msg: %02x:%02x:%02x\n", dwInstance,
//                   dwParam2, dwParam1 & 0xff, dwParam1 >> 8 & 0xff,
//                   dwParam1 >> 16 & 0xff);
        dp = midinptab[dwInstance-1]; /* get device pointer */
        nxtinp = dp->inpptr+1; /* get next input pointer */
        if (nxtinp >= MIDMAX) nxtinp = 0; /* wrap input pointer */
        if (nxtinp != dp->outptr) {

            /* no overflow */
            dp->inpque[dp->inpptr].time = dwParam2; /* place time */
            dp->inpque[dp->inpptr].mmsg = dwParam1; /* place message */
            dp->inpptr = nxtinp; /* advance input pointer */

        } else dp->qovf = TRUE; /* flag queue overflow */

    }

}

/*******************************************************************************

Open a synthesizer input port.

The given synthesizer port is opened and ready for reading.

*******************************************************************************/

void ami_opensynthin(int p)

{

    midinpptr dp;
    MMRESULT  r;

    if (p < 1 || p > MAXMIDP) error("Invalid MIDI input port number");
    if (p > midiInGetNumDevs()) error("No system wave input device exists");
    makmidinp(p); /* ensure device exists */
    dp = midinptab[p-1]; /* get device pointer */
    if (dp->open) error("Wave input device is already open");
    dp->open = TRUE; /* set device open */

    r = midiInOpen(&dp->hmi, 0, (DWORD_PTR)MidiInProc, p, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) error("Cannot open midi device");

    dp->mh1.lpData = dp->mb1;
    dp->mh1.dwBufferLength = MIDBUFSIZ;
    dp->mh1.dwBytesRecorded = 0;
    dp->mh1.dwUser = 0;
    dp->mh1.dwFlags = 0;
    dp->mh1.dwOffset = 0;

    r = midiInPrepareHeader(dp->hmi, &dp->mh1, sizeof(MIDIHDR));
    if (r != MMSYSERR_NOERROR) error("Cannot prepare header");

    r = midiInAddBuffer(dp->hmi, &dp->mh1, sizeof(MIDIHDR));
    if (r != MMSYSERR_NOERROR) error("Cannot add buffer");

    dp->mh2.lpData = dp->mb2;
    dp->mh1.dwBufferLength = MIDBUFSIZ;
    dp->mh1.dwBytesRecorded = 0;
    dp->mh1.dwUser = 0;
    dp->mh1.dwFlags = 0;
    dp->mh1.dwOffset = 0;

    r = midiInPrepareHeader(dp->hmi, &dp->mh1, sizeof(MIDIHDR));
    if (r != MMSYSERR_NOERROR) error("Cannot prepare header");

    r = midiInAddBuffer(dp->hmi, &dp->mh1, sizeof(MIDIHDR));
    if (r != MMSYSERR_NOERROR) error("Cannot add buffer");

    r = midiInStart(dp->hmi);
    if (r != MMSYSERR_NOERROR) error("Cannot start input");

}

/*******************************************************************************

Close a synthesizer input port

Closes the given synthesizer port for reading.

*******************************************************************************/

void ami_closesynthin(int p)

{

    error("ami_closesynthin: Is not implemented");

}

/*******************************************************************************

Write synthesizer port

Writes a given sequencer instruction entry the given output port. This is the
same effect as the "constructor" entries, but means the user won't have to
decode streaming MIDI devices.

Note that if the entries are sequenced (time not 0) they will be entered into
the sequencer list.

The port numbers in the input are ignored and replaced with the port given as
a parameter.

*******************************************************************************/

void ami_wrsynth(int p, ami_seqptr sp)

{

    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    ami_seqptr spp;  /* message pointer */

    elap = difftime(strtim); /* find elapsed time */
    if (sp->time || (sp->time <= elap && seqrun)) {

        /* entry to be sequenced */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&spp); /* get a sequencer message */
        /* make a copy of the command record */
        memcpy(spp, sp, sizeof(ami_seqmsg));
        spp->port = p; /* override the port number */
        insseq(spp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((sp->time-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    } else  {

        if (sp->port != p) {

            /* the ports don't match, we need to override it. We won't modify
               record given, but make a copy */
            getseq(&spp); /* get a sequencer message */
            /* make a copy of the command record */
            memcpy(spp, sp, sizeof(ami_seqmsg));
            spp->port = p; /* override the port number */
            excseq(spp); /* output */

        } else excseq(sp); /* execute the synth note */

    }

}

/*******************************************************************************

Parse sequencer entry

Parses a MIDI sequencer entry from the 1 to 3 bytes given in a Windows packed
message. Windows is assumed to handle syncing the data stream, and also handling
running status or repeats. Windows creates several whole MIDI messages in these
cases.

If system exclusive messages are processed, they go through a separate input
system.

Time stamping is presently done by stamping on entry. Windows documentation
states that this can be wrong, and includes timestamps in MIDI messages that
originates from the driver. This can be used as a further improvement on the
Algorithm used here.

*******************************************************************************/

static void parseq(int p, ami_seqptr sp, byte b1, byte b2, byte b3)

{

    int t;
    byte p1;
    byte p2;
    byte p3;
    byte p4;
    byte p5;
    unsigned len;
    int i;

    t = 0; /* set no time */
    if (sinrun) t = difftime(sintim); /* mark with current time */
    switch (b1>>4) { /* command nybble */

        case 0x8: /* note off */
                  p1 = b2;
                  p2 = b3;
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_noteoff; /* set type */
                  sp->ntc = (b1&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set velocity */
                  break;
        case 0x9: /* note on */
                  p1 = b2;
                  p2 = b3;
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_noteon; /* set type */
                  sp->ntc = (b1&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set velocity */
                  break;
        case 0xa: /* polyphonic key pressure */
                  p1 = b2;
                  p2 = b3;
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_aftertouch; /* set type */
                  sp->ntc = (b1&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set aftertouch */
                  break;
        case 0xb: /* controller change/channel mode */
                  p1 = b2;
                  p2 = b3;
                  switch (p1) { /* channel mode messages */

                      /* note we don't implement all controller messages */
                      case CTLR_SOUND_ATTACK_TIME:
                      case CTLR_SOUND_RELEASE_TIME:
                      case CTLR_LEGATO_PEDAL:
                      case CTLR_PORTAMENTO:
                      case CTLR_VOLUME_COARSE:
                      case CTLR_VOLUME_FINE:
                      case CTLR_BALANCE_COARSE:
                      case CTLR_BALANCE_FINE:
                      case CTLR_PORTAMENTO_TIME_COARSE:
                      case CTLR_PORTAMENTO_TIME_FINE:
                      case CTLR_MODULATION_WHEEL_COARSE:
                      case CTLR_MODULATION_WHEEL_FINE:
                      case CTLR_PAN_POSITION_COARSE:
                      case CTLR_PAN_POSITION_FINE:
                      case CTLR_SOUND_TIMBRE:
                      case CTLR_SOUND_BRIGHTNESS:
                      case CTLR_EFFECTS_LEVEL:
                      case CTLR_TREMULO_LEVEL:
                      case CTLR_CHORUS_LEVEL:
                      case CTLR_CELESTE_LEVEL:
                      case CTLR_PHASER_LEVEL:
                      case CTLR_REGISTERED_PARAMETER_COARSE:
                      case CTLR_REGISTERED_PARAMETER_FINE:
                      case CTLR_DATA_ENTRY_COARSE:
                      case CTLR_DATA_ENTRY_FINE:
                                 break;

                      case CTLR_MONO_OPERATION: /* Mono mode on */
                                 sp->port = p; /* set port */
                                 sp->time = t; /* set time */
                                 sp->st = st_mono; /* set type */
                                 sp->vsc = (b1&15)+1; /* set channel */
                                 sp->vsv = p2; /* set mono mode */
                                 break;
                      case CTLR_POLY_OPERATION: /* Poly mode on */
                                 sp->port = p; /* set port */
                                 sp->time = t; /* set time */
                                 sp->st = st_poly; /* set type */
                                 sp->pc = (b1&15)+1; /* set channel */
                                 break;

                  }
                  break;
        case 0xc: /* program change */
                  p1 = b2;
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_instchange; /* set type */
                  sp->icc = (b1&15)+1; /* set channel */
                  sp->ici = p1+1; /* set instrument */
                  break;
        case 0xd: /* channel key pressure */
                  p1 = b2;
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_pressure; /* set type */
                  sp->ntc = (b1&15)+1; /* set channel */
                  sp->ntv = p1*0x01000000; /* set pressure */
                  break;
        case 0xe: /* pitch bend */
                  p1 = b2;
                  p2 = b3;
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_pitch; /* set type */
                  sp->vsc = (b1&15)+1; /* set channel */
                  sp->vsv = p2<<7|p1; /* set pitch */
                  break;
        case 0xf: /* sysex/meta */
                  /* windows sends system exclusive messages via the buffer
                     so they should not show up here */
                  error("System exclusive message encountered in common data");
                  break;

        default: error("Invalid MIDI format");

    }
#ifdef SHOWMIDIIN
    dmpseq(sp); /* dump sequencer instruction */
#endif

}

/*******************************************************************************

Read synthesizer port

Reads and parses a midi instruction record from the given input port. ALSA midi
input ports are assumed to be "time delimited", that is, input terminates if the
input device takes to long to deliver the next byte. This makes the input self
syncronising. This property will exist even if the traffic is burstly, IE.,
since we parse a MIDI instruction and stop, we will keep sync as long as we
can properly decode MIDI instructions.

We return the next midi instruction as a sequencer record. The port is as the
input, which is not useful except to document where the instruction came from.
If a start time is set for the input, then the input is timestamped, otherwise
the time will be set to 0 or "unsequenced".

Note that this routine will read meta-instructions, but skip them. It is not
a full MIDI decoder.

*******************************************************************************/

void ami_rdsynth(int p, ami_seqptr sp)

{

    midinpptr dp;
    DWORD_PTR midins;
    int       nxtout;
    byte      b1, b2, b3;

    if (p < 1 || p > MAXMIDP) error("Invalid MIDI input port number");
    if (p > midiInGetNumDevs()) error("No system wave input device exists");
    makmidinp(p); /* ensure device exists */
    dp = midinptab[p-1]; /* get device pointer */
    if (!dp->open) error("Wave input device is not open");
    /* wait for data in queue */
    while (dp->outptr == dp->inpptr);
    midins = dp->inpque[dp->outptr].mmsg; /* get next MIDI message */
    nxtout = dp->outptr+1; /* get next output pointer */
    if (nxtout >= MIDMAX) nxtout = 0; /* wrap the output pointer */
    dp->outptr = nxtout; /* update */
    /* put MIDI instruction into bytes */
    b1 = midins & 0xff;
    b2 = midins >> 8 & 0xff;
    b3 = midins >> 16 & 0xff;
    /* now parse the sequencer entry */
    parseq(p, sp, b1, b2, b3);

}

/*******************************************************************************

Get device parameter synth out

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void ami_getparamsynthout(int p, string name, string value, int len)

{

    error("ami_getparamsynthout: Is not implemented");

}

/*******************************************************************************

Get device parameter synth in

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void ami_getparamsynthin(int p, string name, string value, int len)

{

    error("ami_getparamsynthin: Is not implemented");

}

/*******************************************************************************

Get device parameter wave out

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void ami_getparamwaveout(int p, string name, string value, int len)

{

    error("ami_getparamwaveout: Is not implemented");

}

/*******************************************************************************

Get device parameter wave in

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void ami_getparamwavein(int p, string name, string value, int len)

{

    error("ami_getparamwavein: Is not implemented");

}

/*******************************************************************************

Set device parameter synth out

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int ami_setparamsynthout(int p, string name, string value)

{

    error("ami_setparamsynthout: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Set device parameter synth in

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int ami_setparamsynthin(int p, string name, string value)

{

    error("ami_setparamsynthin: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Set device parameter wave out

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int ami_setparamwaveout(int p, string name, string value)

{

    error("ami_setparamwaveout: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Set device parameter wave in

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int ami_setparamwavein(int p, string name, string value)

{

    error("ami_setparamwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Initialize sound module

Clears sequencer lists, flags no timer active, clears the midi output port
table, and initializes the sequencer task mutex.

*******************************************************************************/

static void ami_init_sound (void) __attribute__((constructor (102)));
static void ami_init_sound()

{

    int i;

    seqlst = NULL; /* clear active sequencer list */
    seqfre = NULL; /* clear free sequencer messages */
    seqrun = FALSE; /* set sequencer ! running */
    strtim = 0; /* clear start time */
    timhan = 0; /* set no timer active */
    wavcnt = 0; /* clear wave start count */
    for (i = 0; i < MAXMIDP; i++) midouttab[i] = (HMIDIOUT)-1; /* set no midi output ports open */
    for (i = 0; i < MAXMIDP; i++) midinptab[i] = NULL; /* set no midi output ports open */
    for (i = 0; i < MAXWAVP; i++) pcmout[i] = NULL; /* set no wave output ports open */
    for (i = 0; i < MAXMIDT; i++) synthnam[i] = NULL; /* clear synth track list */
    for (i = 0; i < MAXWAVT; i++) wavenam[i] = NULL; /* clear wave track list */
    InitializeCriticalSection(&seqlock); /* initialize the sequencer lock */
    /* initialize wave play complete signal */
    playwavecomplete = CreateEvent(NULL, TRUE, FALSE, NULL);

}
