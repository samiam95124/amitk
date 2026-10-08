/*******************************************************************************
*                                                                              *
*                                PETIT-AMI                                     *
*                                                                              *
*                         MACOS SOUND MODULE                                   *
*                                                                              *
*                              Created 2026                                    *
*                                                                              *
*                               S. A. FRANCO                                   *
*                                                                              *
* macOS implementation of the Petit-Ami sound module using CoreMIDI,           *
* AudioToolbox (AUGraph for software MIDI synthesis, AudioQueue for wave       *
* streaming), and MusicPlayer for MIDI file playback.                          *
*                                                                              *
* Port 1 synth output is the built-in Apple DLS software synthesizer.          *
* Additional synth ports map to CoreMIDI destinations.                         *
* Wave output uses AudioQueue Services.                                        *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2026 - Scott A. Franco                                         *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/event.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <CoreMIDI/CoreMIDI.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>

#include <sound.h>

/*******************************************************************************
*                                                                              *
*                              Constants                                       *
*                                                                              *
*******************************************************************************/

#define MAXMIDP  100  /* maximum MIDI ports */
#define MAXWAVP  100  /* maximum wave ports */
#define MAXMIDT  100  /* maximum loaded MIDI file cache entries */
#define MAXWAVT  100  /* maximum loaded wave file cache entries */
#define WAVBUFS    3  /* number of AudioQueue buffers */
#define WAVBUFSZ 4096 /* AudioQueue buffer size in bytes */
#define SEQTIM_IDENT 42 /* kqueue timer ident */

/* MIDI controller numbers */
#define CTLR_MODULATION_WHEEL_COARSE   1
#define CTLR_MODULATION_WHEEL_FINE    33
#define CTLR_DATA_ENTRY_COARSE         6
#define CTLR_DATA_ENTRY_FINE          38
#define CTLR_VOLUME_COARSE             7
#define CTLR_VOLUME_FINE              39
#define CTLR_BALANCE_COARSE            8
#define CTLR_BALANCE_FINE             40
#define CTLR_PAN_POSITION_COARSE      10
#define CTLR_PAN_POSITION_FINE        42
#define CTLR_PORTAMENTO_TIME_COARSE    5
#define CTLR_PORTAMENTO_TIME_FINE     37
#define CTLR_PORTAMENTO               65
#define CTLR_LEGATO_PEDAL             68
#define CTLR_SOUND_TIMBRE             71
#define CTLR_SOUND_RELEASE_TIME       72
#define CTLR_SOUND_ATTACK_TIME        73
#define CTLR_SOUND_BRIGHTNESS         74
#define CTLR_EFFECTS_LEVEL            91
#define CTLR_TREMULO_LEVEL            92
#define CTLR_CHORUS_LEVEL             93
#define CTLR_CELESTE_LEVEL            94
#define CTLR_PHASER_LEVEL             95
#define CTLR_REGISTERED_PARAMETER_COARSE  101
#define CTLR_REGISTERED_PARAMETER_FINE    100
#define CTLR_MONO_OPERATION          126
#define CTLR_POLY_OPERATION          127

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/*******************************************************************************
*                                                                              *
*                              Types                                           *
*                                                                              *
*******************************************************************************/

/* synth output port descriptor */
typedef struct {
    int          open;     /* port is open */
    int          issoft;   /* is the software DLS synth (port 1) */
    int          isvirt;   /* is the virtual port: a CoreMIDI source of our
                              own, which other programs connect to */
    /* software synth state (port 1) */
    AUGraph      graph;
    AudioUnit    synthUnit;
    /* CoreMIDI external output (port 2+) */
    MIDIEndpointRef endpoint;
    MIDIPortRef     midiPort;
    /* plug-in overrides */
    void (*opn)(ami_long p);
    void (*cls)(ami_long p);
    void (*wrseq)(ami_long p, ami_seqptr sp);
    ami_long (*setparam)(ami_long p, string name, string value);
    void (*getparam)(ami_long p, string name, string value, ami_long len);
    char name[256];
} synthoutdev;

#define MIDIRING 4096 /* bytes of MIDI input held for the reader */

/* the names the virtual ports go by in the system */
#define VIRTOUTNAME "PetitAmi virtual out"
#define VIRTINNAME  "PetitAmi virtual in"

/* synth input port descriptor */
typedef struct {
    int          open;
    MIDIEndpointRef endpoint;
    MIDIPortRef     midiPort;
    int             isvirt;  /* is the virtual port: a CoreMIDI destination
                                of our own, which other programs send to */
    byte            ring[MIDIRING]; /* bytes received and not yet read */
    int             ringhd;  /* where the next goes */
    int             ringn;   /* bytes held */
    pthread_mutex_t rlock;   /* guards the ring */
    pthread_cond_t  rdata;   /* a byte arrived */
    int             pback;   /* byte put back, or -1 */
    int             last;    /* last status byte, for running status */
    int             sync;    /* a status byte has been seen */
    void (*opn)(ami_long p);
    void (*cls)(ami_long p);
    void (*rdseq)(ami_long p, ami_seqptr sp);
    ami_long (*setparam)(ami_long p, string name, string value);
    void (*getparam)(ami_long p, string name, string value, ami_long len);
    char name[256];
} synthindev;

/* wave output port descriptor */
typedef struct {
    int           open;
    int           chan;    /* channels */
    int           rate;    /* sample rate */
    int           bits;    /* bits per sample */
    int           sgn;     /* signed */
    int           flt;     /* floating point */
    int           big;     /* big endian */
    AudioQueueRef queue;   /* AudioQueue for streaming */
    size_t        pend;    /* bytes written and not yet played */
    int           fmtset;  /* format needs (re-)configuration */
    void (*opn)(ami_long p);
    void (*cls)(ami_long p);
    void (*chanwavout)(ami_long p, ami_long c);
    void (*ratewavout)(ami_long p, ami_long r);
    void (*lenwavout)(ami_long p, ami_long l);
    void (*sgnwavout)(ami_long p, ami_long s);
    void (*fltwavout)(ami_long p, ami_long f);
    void (*endwaveout)(ami_long p, ami_long e);
    void (*wrwav)(ami_long p, byte* buff, ami_long len);
    ami_long (*setparam)(ami_long p, string name, string value);
    void (*getparam)(ami_long p, string name, string value, ami_long len);
    char name[256];
} waveoutdev;

/* wave input port descriptor */
typedef struct {
    int           open;
    int           chan;
    int           rate;
    int           bits;
    int           sgn;
    int           flt;
    int           big;
    AudioQueueRef queue;
    byte*         ring;    /* recorded bytes not yet read */
    size_t        ringsz;  /* its size */
    size_t        ringhd;  /* where the next recorded byte goes */
    size_t        ringn;   /* bytes held */
    pthread_mutex_t rlock; /* guards the ring */
    pthread_cond_t  rdata; /* data arrived in it */
    void (*opn)(ami_long p);
    void (*cls)(ami_long p);
    ami_long (*chanwavin)(ami_long p);
    ami_long (*ratewavin)(ami_long p);
    ami_long (*lenwavin)(ami_long p);
    ami_long (*sgnwavin)(ami_long p);
    ami_long (*fltwavin)(ami_long p);
    ami_long (*endwavein)(ami_long p);
    ami_long (*rdwav)(ami_long p, byte* buff, ami_long len);
    ami_long (*setparam)(ami_long p, string name, string value);
    void (*getparam)(ami_long p, string name, string value, ami_long len);
    char name[256];
} waveindev;

/*******************************************************************************
*                                                                              *
*                           Global state                                       *
*                                                                              *
*******************************************************************************/

/* CoreMIDI client and output port (shared) */
static MIDIClientRef   midiClient;
static MIDIPortRef     midiOutPort;
static int             midiInited;

/* device tables */
static synthoutdev synthout_tab[MAXMIDP];
static int         synthout_num;
static int         synthout_plug; /* number of plug-in synth outs */

static synthindev  synthin_tab[MAXMIDP];
static int         synthin_num;
static int         synthin_plug;

static waveoutdev  waveout_tab[MAXWAVP];
static int         waveout_num;
static int         waveout_plug;

static waveindev   wavein_tab[MAXWAVP];
static int         wavein_num;
static int         wavein_plug;

/* loaded file caches */
static char* wavfil[MAXWAVT]; /* wave file name table */
static pthread_mutex_t wavlck = PTHREAD_MUTEX_INITIALIZER;

static char* synfil[MAXMIDT]; /* synth (MIDI) file name table */
static pthread_mutex_t synlck = PTHREAD_MUTEX_INITIALIZER;

/* active play counters */
static int numwav;
static pthread_mutex_t wnmlck = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wnmzer = PTHREAD_COND_INITIALIZER;

static int numseq;
static pthread_mutex_t snmlck = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  snmzer = PTHREAD_COND_INITIALIZER;

/* sequencer state */
static ami_seqptr  seqlst;    /* pending event list (time-sorted) */
static ami_seqptr  seqfre;    /* recycled-entry free list */
static int         seqrun;    /* sequencer is active */
static struct timeval strtim; /* sequencer start time */
static int         seqtimact; /* timer is armed */
static int         seqhan;    /* kqueue fd */
static pthread_mutex_t seqlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t   seqthread;
static int         seqthread_running;

/* input timing */
static int seqrunin;
static struct timeval sintim;

/* module init */
static int inited;

/*******************************************************************************
*                                                                              *
*                          Error handling                                       *
*                                                                              *
*******************************************************************************/

static void error(const char* s)
{
    fprintf(stderr, "\nError: Sound: %s\n", s);
    exit(1);
}

/* Copy string to critical output buffer. Output buffers follow the critical
   buffer convention: a result that fills the entire buffer is not zero
   terminated, a shorter result is zero terminated, and it is an error if the
   result cannot fit. */
static void cpycrit(char* d, ami_long dl, const char* s)
{
    ami_long l = (ami_long)strlen(s); /* find length of source */
    if (l > dl) error("String too large for destination");
    memcpy(d, s, l); /* copy string into place */
    if (l < dl) d[l] = 0; /* terminate if shorter than buffer */
}

/*******************************************************************************
*                                                                              *
*                          Time helpers                                         *
*                                                                              *
*******************************************************************************/

static ami_long timediff(struct timeval* rt)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ami_long ds = (ami_long)(tv.tv_sec  - rt->tv_sec);
    ami_long du = (ami_long)(tv.tv_usec - rt->tv_usec);
    return ds * 10000 + du / 100;
}

/*******************************************************************************
*                                                                              *
*                      Sequencer infrastructure                                *
*                                                                              *
*******************************************************************************/

static void getseq(ami_seqptr* p)
{
    if (seqfre) {
        *p = seqfre;
        seqfre = seqfre->next;
    } else
        *p = (ami_seqptr)malloc(sizeof(ami_seqmsg));
    (*p)->next = NULL;
}

static void putseq(ami_seqptr p)
{
    p->next = seqfre;
    seqfre = p;
}

static void insseq(ami_seqptr sp)
{
    ami_seqptr p, l;

    pthread_mutex_lock(&seqlock);
    l = NULL;
    p = seqlst;
    while (p && p->time <= sp->time) { l = p; p = p->next; }
    sp->next = p;
    if (l) l->next = sp; else seqlst = sp;
    pthread_mutex_unlock(&seqlock);
}

/* forward declarations */
static void excseq(ami_seqptr sp);
static void playwave_kickoff(ami_long port, ami_long wt);
static void playsynth_kickoff(ami_long port, ami_long sid);

static void acttim(void)
{
    if (!seqtimact && seqlst) {
        ami_long elap = timediff(&strtim);
        ami_long tl = seqlst->time - elap;
        if (tl < 0) tl = 0;
        struct kevent kev;
        /* NOTE_NSECONDS: tl is in 100us units → convert to nanoseconds */
        uint64_t ns = (uint64_t)tl * 100000ULL;
        if (ns == 0) ns = 1;
        EV_SET(&kev, SEQTIM_IDENT, EVFILT_TIMER,
               EV_ADD | EV_ONESHOT, NOTE_NSECONDS, (intptr_t)ns, NULL);
        kevent(seqhan, &kev, 1, NULL, 0, NULL);
        seqtimact = TRUE;
    }
}

static void* sequencer_thread(void* arg)
{
    (void)arg;
    struct kevent kev;

    for (;;) {
        int n = kevent(seqhan, NULL, 0, &kev, 1, NULL);
        if (n < 0) break;
        if (n == 0) continue;

        pthread_mutex_lock(&seqlock);
        ami_long elap = timediff(&strtim);
        while (seqlst && seqlst->time <= elap) {
            ami_seqptr sp = seqlst;
            seqlst = sp->next;
            sp->next = NULL;
            pthread_mutex_unlock(&seqlock);
            excseq(sp);
            putseq(sp);
            pthread_mutex_lock(&seqlock);
            elap = timediff(&strtim);
        }
        seqtimact = FALSE;
        if (seqlst)
            acttim();
        else {
            pthread_mutex_lock(&snmlck);
            numseq--;
            if (numseq <= 0) { numseq = 0; pthread_cond_broadcast(&snmzer); }
            pthread_mutex_unlock(&snmlck);
        }
        pthread_mutex_unlock(&seqlock);
    }
    return NULL;
}

static void ensure_seqthread(void)
{
    if (!seqthread_running) {
        seqhan = kqueue();
        if (seqhan < 0) error("Cannot create kqueue for sequencer");
        pthread_create(&seqthread, NULL, sequencer_thread, NULL);
        seqthread_running = TRUE;
    }
}

/* schedule or execute immediately */
static void seqorexe(ami_seqptr sp)
{
    if (sp->time == 0 || !seqrun) {
        excseq(sp);
        putseq(sp);
    } else {
        ami_long elap = timediff(&strtim);
        if (sp->time <= elap) {
            excseq(sp);
            putseq(sp);
        } else {
            insseq(sp);
            pthread_mutex_lock(&seqlock);
            acttim();
            pthread_mutex_unlock(&seqlock);
        }
    }
}

/*******************************************************************************
*                                                                              *
*                       MIDI message helpers                                   *
*                                                                              *
*******************************************************************************/

static void midisend(ami_long port, UInt32 status, UInt32 data1, UInt32 data2)
{
    synthoutdev* d = &synthout_tab[port - 1];
    if (!d->open) return;

    if (d->issoft) {
        MusicDeviceMIDIEvent(d->synthUnit, status, data1, data2, 0);
    } else if (d->endpoint) {
        Byte buf[3] = { (Byte)status, (Byte)data1, (Byte)data2 };
        MIDIPacketList pktList;
        MIDIPacket* pkt = MIDIPacketListInit(&pktList);
        int len = (status >= 0xC0 && status < 0xE0) ? 2 : 3;
        MIDIPacketListAdd(&pktList, sizeof(pktList), pkt, 0, len, buf);
        /* the virtual port is a source: what is written is what anyone
           connected to it receives */
        if (d->isvirt) MIDIReceived(d->endpoint, &pktList);
        else MIDISend(d->midiPort, d->endpoint, &pktList);
    }
}

static void ctlchg(ami_long port, ami_channel c, int cn, int v)
{
    midisend(port, 0xB0 + (c - 1), cn, v);
}

/*******************************************************************************
*                                                                              *
*                  Execute sequencer entry                                     *
*                                                                              *
*******************************************************************************/

static void excseq(ami_seqptr sp)
{
    ami_long b, pt;

    /* if the port has a plug-in wrseq, use it */
    if (sp->port >= 1 && sp->port <= synthout_num &&
        synthout_tab[sp->port - 1].wrseq) {
        synthout_tab[sp->port - 1].wrseq(sp->port, sp);
        return;
    }

    switch (sp->st) {
    case st_noteon:
        midisend(sp->port, 0x90 + (sp->ntc - 1),
                 sp->ntn - 1, sp->ntv / (LONG_MAX/128+1));
        break;
    case st_noteoff:
        midisend(sp->port, 0x80 + (sp->ntc - 1),
                 sp->ntn - 1, sp->ntv / (LONG_MAX/128+1));
        break;
    case st_instchange:
        midisend(sp->port, 0xC0 + (sp->icc - 1), sp->ici - 1, 0);
        break;
    case st_attack:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_ATTACK_TIME,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_release:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_RELEASE_TIME,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_legato:
        ctlchg(sp->port, sp->bsc, CTLR_LEGATO_PEDAL, !!sp->bsb * 127);
        break;
    case st_portamento:
        ctlchg(sp->port, sp->bsc, CTLR_PORTAMENTO, !!sp->bsb * 127);
        break;
    case st_vibrato:
        ctlchg(sp->port, sp->vsc, CTLR_MODULATION_WHEEL_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_MODULATION_WHEEL_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_volsynthchan:
        ctlchg(sp->port, sp->vsc, CTLR_VOLUME_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_VOLUME_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_porttime:
        ctlchg(sp->port, sp->vsc, CTLR_PORTAMENTO_TIME_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_PORTAMENTO_TIME_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_balance:
        b = sp->vsv / (LONG_MAX/8192+1) + 0x2000;
        ctlchg(sp->port, sp->vsc, CTLR_BALANCE_COARSE, b / 0x80);
        ctlchg(sp->port, sp->vsc, CTLR_BALANCE_FINE, b & 0x7f);
        break;
    case st_pan:
        b = sp->vsv / (LONG_MAX/8192+1) + 0x2000;
        ctlchg(sp->port, sp->vsc, CTLR_PAN_POSITION_COARSE, b / 0x80);
        ctlchg(sp->port, sp->vsc, CTLR_PAN_POSITION_FINE, b & 0x7f);
        break;
    case st_timbre:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_TIMBRE,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_brightness:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_BRIGHTNESS,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_reverb:
        ctlchg(sp->port, sp->vsc, CTLR_EFFECTS_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_tremulo:
        ctlchg(sp->port, sp->vsc, CTLR_TREMULO_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_chorus:
        ctlchg(sp->port, sp->vsc, CTLR_CHORUS_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_celeste:
        ctlchg(sp->port, sp->vsc, CTLR_CELESTE_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_phaser:
        ctlchg(sp->port, sp->vsc, CTLR_PHASER_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_aftertouch:
        midisend(sp->port, 0xA0 + (sp->ntc - 1),
                 sp->ntn - 1, sp->ntv / (LONG_MAX/128+1));
        break;
    case st_pressure:
        midisend(sp->port, 0xD0 + (sp->ntc - 1),
                 sp->ntv / (LONG_MAX/128+1), 0);
        break;
    case st_pitch:
        pt = sp->vsv / (LONG_MAX/8192+1) + 0x2000;
        midisend(sp->port, 0xE0 + (sp->vsc - 1), pt & 0x7f, pt / 0x80);
        break;
    case st_pitchrange:
        ctlchg(sp->port, sp->vsc, CTLR_REGISTERED_PARAMETER_COARSE, 0);
        ctlchg(sp->port, sp->vsc, CTLR_REGISTERED_PARAMETER_FINE, 0);
        ctlchg(sp->port, sp->vsc, CTLR_DATA_ENTRY_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_DATA_ENTRY_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_mono:
        ctlchg(sp->port, sp->vsc, CTLR_MONO_OPERATION, sp->vsv);
        break;
    case st_poly:
        ctlchg(sp->port, sp->pc, CTLR_POLY_OPERATION, 0);
        break;
    case st_playsynth:
        playsynth_kickoff(sp->port, sp->sid);
        break;
    case st_playwave:
        playwave_kickoff(sp->port, sp->wt);
        break;
    case st_volwave:
        /* not implemented */
        break;
    }
}

void _pa_excseq(ami_long p, ami_seqptr sp)
{
    excseq(sp);
}

/*******************************************************************************
*                                                                              *
*                    Device enumeration & init                                  *
*                                                                              *
*******************************************************************************/

static void init_coremidi(void)
{
    if (midiInited) return;
    OSStatus st = MIDIClientCreate(CFSTR("PetitAmi"), NULL, NULL, &midiClient);
    if (st != noErr) return;
    MIDIOutputPortCreate(midiClient, CFSTR("PA Output"), &midiOutPort);
    midiInited = TRUE;
}

static void enumerate_devices(void)
{
    init_coremidi();

    /* port 1 = built-in DLS software synth (always present) */
    synthout_tab[0].issoft = TRUE;
    snprintf(synthout_tab[0].name, sizeof(synthout_tab[0].name),
             "Apple DLS Music Device");
    synthout_num = 1;

    /* additional ports = CoreMIDI destinations */
    ItemCount nd = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < nd && synthout_num < MAXMIDP; i++) {
        MIDIEndpointRef ep = MIDIGetDestination(i);
        if (!ep) continue;
        synthoutdev* d = &synthout_tab[synthout_num];
        d->endpoint = ep;
        CFStringRef cfname = NULL;
        MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &cfname);
        if (cfname) {
            CFStringGetCString(cfname, d->name, sizeof(d->name),
                               kCFStringEncodingUTF8);
            CFRelease(cfname);
        } else
            snprintf(d->name, sizeof(d->name), "MIDI Out %d",
                     synthout_num + 1);
        synthout_num++;
    }

    /* synth input = CoreMIDI sources */
    ItemCount ns = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < ns && synthin_num < MAXMIDP; i++) {
        MIDIEndpointRef ep = MIDIGetSource(i);
        if (!ep) continue;
        synthindev* d = &synthin_tab[synthin_num];
        d->endpoint = ep;
        CFStringRef cfname = NULL;
        MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &cfname);
        if (cfname) {
            CFStringGetCString(cfname, d->name, sizeof(d->name),
                               kCFStringEncodingUTF8);
            CFRelease(cfname);
        } else
            snprintf(d->name, sizeof(d->name), "MIDI In %d",
                     synthin_num + 1);
        synthin_num++;
    }

    /* The last port each way is "virtual", as on Linux: an endpoint of
       our own that appears to the system when the port is opened, for
       other programs to connect to. The output is a CoreMIDI source and
       the input a destination. */
    if (midiInited && synthout_num < MAXMIDP) {
        synthout_tab[synthout_num].isvirt = TRUE;
        strcpy(synthout_tab[synthout_num].name, "virtual");
        synthout_num++;
    }
    if (midiInited && synthin_num < MAXMIDP) {
        synthin_tab[synthin_num].isvirt = TRUE;
        strcpy(synthin_tab[synthin_num].name, "virtual");
        synthin_num++;
    }

    /* wave output: always 1 default device */
    snprintf(waveout_tab[0].name, sizeof(waveout_tab[0].name),
             "Default Audio Output");
    waveout_tab[0].chan = 2;
    waveout_tab[0].rate = 44100;
    waveout_tab[0].bits = 16;
    waveout_tab[0].sgn  = TRUE;
    waveout_tab[0].flt  = FALSE;
    waveout_tab[0].big  = FALSE;
    waveout_num = 1;

    /* wave input: always 1 default device */
    snprintf(wavein_tab[0].name, sizeof(wavein_tab[0].name),
             "Default Audio Input");
    wavein_tab[0].chan = 1;
    wavein_tab[0].rate = 44100;
    wavein_tab[0].bits = 16;
    wavein_tab[0].sgn  = TRUE;
    wavein_tab[0].flt  = FALSE;
    wavein_tab[0].big  = FALSE;
    wavein_num = 1;
}

__attribute__((constructor(103)))
static void ami_init_sound(void)
{
    if (inited) return;
    inited = TRUE;
    enumerate_devices();
}

/*******************************************************************************
*                                                                              *
*                        Sequencer timing                                      *
*                                                                              *
*******************************************************************************/

void ami_starttimeout(void)
{
    ensure_seqthread();
    gettimeofday(&strtim, NULL);
    seqrun = TRUE;
}

void ami_stoptimeout(void)
{
    seqrun = FALSE;
    pthread_mutex_lock(&seqlock);
    /* drain the sequencer list */
    while (seqlst) {
        ami_seqptr sp = seqlst;
        seqlst = sp->next;
        putseq(sp);
    }
    seqtimact = FALSE;
    pthread_mutex_unlock(&seqlock);
}

ami_long ami_curtimeout(void)
{
    if (!seqrun) return 0;
    return timediff(&strtim);
}

void ami_starttimein(void)
{
    gettimeofday(&sintim, NULL);
    seqrunin = TRUE;
}

void ami_stoptimein(void)
{
    seqrunin = FALSE;
}

ami_long ami_curtimein(void)
{
    if (!seqrunin) return 0;
    return timediff(&sintim);
}

/*******************************************************************************
*                                                                              *
*                     Port enumeration                                         *
*                                                                              *
*******************************************************************************/

ami_long ami_synthout(void)  { return synthout_num; }
ami_long ami_synthin(void)   { return synthin_num; }
ami_long ami_waveout(void)   { return waveout_num; }
ami_long ami_wavein(void)    { return wavein_num; }

/* Device names are returned by the critical buffer convention: a result that
   fills the entire buffer is not zero terminated, a shorter result is zero
   terminated, and it is an error if the result cannot fit. */

void ami_synthoutname(ami_long p, string name, ami_long len)
{
    if (p < 1 || p > synthout_num) { cpycrit(name, len, ""); return; }
    cpycrit(name, len, synthout_tab[p - 1].name);
}

void ami_synthinname(ami_long p, string name, ami_long len)
{
    if (p < 1 || p > synthin_num) { cpycrit(name, len, ""); return; }
    cpycrit(name, len, synthin_tab[p - 1].name);
}

void ami_waveoutname(ami_long p, string name, ami_long len)
{
    if (p < 1 || p > waveout_num) { cpycrit(name, len, ""); return; }
    cpycrit(name, len, waveout_tab[p - 1].name);
}

void ami_waveinname(ami_long p, string name, ami_long len)
{
    if (p < 1 || p > wavein_num) { cpycrit(name, len, ""); return; }
    cpycrit(name, len, wavein_tab[p - 1].name);
}

/*******************************************************************************
*                                                                              *
*                    Synth output open/close                                   *
*                                                                              *
*******************************************************************************/

void ami_opensynthout(ami_long p)
{
    if (p < 1 || p > synthout_num) error("Invalid synth output port");
    synthoutdev* d = &synthout_tab[p - 1];
    if (d->open) return;

    if (d->opn) { d->opn(p); d->open = TRUE; return; }

    if (d->issoft) {
        /* create AUGraph: DLSSynth → DefaultOutput */
        OSStatus st;
        st = NewAUGraph(&d->graph);
        if (st != noErr) error("Cannot create AUGraph");

        AudioComponentDescription synthDesc = {
            .componentType = kAudioUnitType_MusicDevice,
            .componentSubType = kAudioUnitSubType_DLSSynth,
            .componentManufacturer = kAudioUnitManufacturer_Apple
        };
        AudioComponentDescription outDesc = {
            .componentType = kAudioUnitType_Output,
            .componentSubType = kAudioUnitSubType_DefaultOutput,
            .componentManufacturer = kAudioUnitManufacturer_Apple
        };

        AUNode synthNode, outNode;
        AUGraphAddNode(d->graph, &synthDesc, &synthNode);
        AUGraphAddNode(d->graph, &outDesc, &outNode);
        AUGraphConnectNodeInput(d->graph, synthNode, 0, outNode, 0);
        AUGraphOpen(d->graph);
        AUGraphNodeInfo(d->graph, synthNode, NULL, &d->synthUnit);
        AUGraphInitialize(d->graph);
        AUGraphStart(d->graph);
    } else if (d->isvirt) {
        if (MIDISourceCreate(midiClient, CFSTR(VIRTOUTNAME),
                             &d->endpoint) != noErr)
            error("Cannot create virtual MIDI output");
    } else {
        /* external CoreMIDI destination: port already created in enumerate */
        d->midiPort = midiOutPort;
    }
    d->open = TRUE;
}

void ami_closesynthout(ami_long p)
{
    if (p < 1 || p > synthout_num) return;
    synthoutdev* d = &synthout_tab[p - 1];
    if (!d->open) return;

    if (d->cls) { d->cls(p); d->open = FALSE; return; }

    if (d->issoft && d->graph) {
        AUGraphStop(d->graph);
        AUGraphUninitialize(d->graph);
        AUGraphClose(d->graph);
        DisposeAUGraph(d->graph);
        d->graph = NULL;
        d->synthUnit = NULL;
    }
    if (d->isvirt && d->endpoint) {
        MIDIEndpointDispose(d->endpoint);
        d->endpoint = 0;
    }
    d->open = FALSE;
}

/*******************************************************************************
*                                                                              *
*                    Synth input open/close                                    *
*                                                                              *
*******************************************************************************/

/* MIDI arrived from a source: its bytes go to the port's ring. The real
   time bytes (clock, active sensing) can fall anywhere, a message's middle
   included, and carry nothing the reader decodes: they are dropped. */
static void midi_read_cb(const MIDIPacketList* pl, void* user, void* src)
{
    synthindev* d = (synthindev*)user;
    const MIDIPacket* pk = &pl->packet[0];

    (void)src;
    pthread_mutex_lock(&d->rlock);
    for (UInt32 i = 0; i < pl->numPackets; i++) {
        for (UInt16 j = 0; j < pk->length; j++) {
            if (pk->data[j] >= 0xf8 || d->ringn >= MIDIRING) continue;
            d->ring[d->ringhd] = pk->data[j];
            d->ringhd = (d->ringhd + 1) % MIDIRING;
            d->ringn++;
        }
        pk = MIDIPacketNext(pk);
    }
    pthread_cond_broadcast(&d->rdata);
    pthread_mutex_unlock(&d->rlock);
}

/* the next byte from a synth input, waiting for it; -1 if the port closed */
static int midi_getb(synthindev* d)
{
    int b;

    if (d->pback >= 0) { b = d->pback; d->pback = -1; return b; }
    pthread_mutex_lock(&d->rlock);
    while (d->open && !d->ringn) pthread_cond_wait(&d->rdata, &d->rlock);
    if (!d->ringn) { pthread_mutex_unlock(&d->rlock); return -1; }
    b = d->ring[(d->ringhd + MIDIRING - d->ringn) % MIDIRING];
    d->ringn--;
    pthread_mutex_unlock(&d->rlock);
    return b;
}

void ami_opensynthin(ami_long p)
{
    if (p < 1 || p > synthin_num) error("Invalid synth input port");
    synthindev* d = &synthin_tab[p - 1];
    if (d->open) return;
    if (d->opn) { d->opn(p); d->open = TRUE; return; }
    /* a CoreMIDI input port on the source, whose bytes ami_rdsynth()
       decodes */
    pthread_mutex_init(&d->rlock, NULL);
    pthread_cond_init(&d->rdata, NULL);
    d->ringhd = d->ringn = 0;
    d->pback = -1;
    d->last = 0;
    d->sync = FALSE;
    if (d->isvirt) {
        if (MIDIDestinationCreate(midiClient, CFSTR(VIRTINNAME), midi_read_cb,
                                  d, &d->endpoint) != noErr)
            error("Cannot create virtual MIDI input");
    } else {
        if (MIDIInputPortCreate(midiClient, CFSTR("PA Input"), midi_read_cb,
                                d, &d->midiPort) != noErr)
            error("Cannot create MIDI input port");
        if (MIDIPortConnectSource(d->midiPort, d->endpoint, d) != noErr)
            error("Cannot connect MIDI input port");
    }
    d->open = TRUE;
}

void ami_closesynthin(ami_long p)
{
    if (p < 1 || p > synthin_num) return;
    synthindev* d = &synthin_tab[p - 1];
    if (!d->open) return;
    if (d->cls) { d->cls(p); d->open = FALSE; return; }
    if (d->isvirt) {
        if (d->endpoint) MIDIEndpointDispose(d->endpoint);
        d->endpoint = 0;
    } else if (d->midiPort) {
        MIDIPortDisconnectSource(d->midiPort, d->endpoint);
        MIDIPortDispose(d->midiPort);
        d->midiPort = 0;
    }
    pthread_mutex_lock(&d->rlock);
    d->open = FALSE;
    pthread_cond_broadcast(&d->rdata); /* release a reader */
    pthread_mutex_unlock(&d->rlock);
}

/*******************************************************************************
*                                                                              *
*                    MIDI note/control messages                                *
*                                                                              *
*******************************************************************************/

void ami_noteon(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_noteon;
    sp->ntc = c; sp->ntn = n; sp->ntv = v;
    seqorexe(sp);
}

void ami_noteoff(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_noteoff;
    sp->ntc = c; sp->ntn = n; sp->ntv = v;
    seqorexe(sp);
}

void ami_instchange(ami_long p, ami_long t, ami_channel c, ami_instrument i)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_instchange;
    sp->icc = c; sp->ici = i;
    seqorexe(sp);
}

void ami_attack(ami_long p, ami_long t, ami_channel c, ami_long at)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_attack;
    sp->vsc = c; sp->vsv = at;
    seqorexe(sp);
}

void ami_release(ami_long p, ami_long t, ami_channel c, ami_long rt)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_release;
    sp->vsc = c; sp->vsv = rt;
    seqorexe(sp);
}

void ami_legato(ami_long p, ami_long t, ami_channel c, ami_long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_legato;
    sp->bsc = c; sp->bsb = b;
    seqorexe(sp);
}

void ami_portamento(ami_long p, ami_long t, ami_channel c, ami_long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_portamento;
    sp->bsc = c; sp->bsb = b;
    seqorexe(sp);
}

void ami_vibrato(ami_long p, ami_long t, ami_channel c, ami_long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_vibrato;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_volsynthchan(ami_long p, ami_long t, ami_channel c, ami_long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_volsynthchan;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_porttime(ami_long p, ami_long t, ami_channel c, ami_long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_porttime;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_balance(ami_long p, ami_long t, ami_channel c, ami_long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_balance;
    sp->vsc = c; sp->vsv = b;
    seqorexe(sp);
}

void ami_pan(ami_long p, ami_long t, ami_channel c, ami_long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pan;
    sp->vsc = c; sp->vsv = b;
    seqorexe(sp);
}

void ami_timbre(ami_long p, ami_long t, ami_channel c, ami_long tb)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_timbre;
    sp->vsc = c; sp->vsv = tb;
    seqorexe(sp);
}

void ami_brightness(ami_long p, ami_long t, ami_channel c, ami_long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_brightness;
    sp->vsc = c; sp->vsv = b;
    seqorexe(sp);
}

void ami_reverb(ami_long p, ami_long t, ami_channel c, ami_long r)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_reverb;
    sp->vsc = c; sp->vsv = r;
    seqorexe(sp);
}

void ami_tremulo(ami_long p, ami_long t, ami_channel c, ami_long tr)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_tremulo;
    sp->vsc = c; sp->vsv = tr;
    seqorexe(sp);
}

void ami_chorus(ami_long p, ami_long t, ami_channel c, ami_long cr)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_chorus;
    sp->vsc = c; sp->vsv = cr;
    seqorexe(sp);
}

void ami_celeste(ami_long p, ami_long t, ami_channel c, ami_long ce)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_celeste;
    sp->vsc = c; sp->vsv = ce;
    seqorexe(sp);
}

void ami_phaser(ami_long p, ami_long t, ami_channel c, ami_long ph)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_phaser;
    sp->vsc = c; sp->vsv = ph;
    seqorexe(sp);
}

void ami_aftertouch(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long at)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_aftertouch;
    sp->ntc = c; sp->ntn = n; sp->ntv = at;
    seqorexe(sp);
}

void ami_pressure(ami_long p, ami_long t, ami_channel c, ami_long pr)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pressure;
    sp->ntc = c; sp->ntn = 0; sp->ntv = pr;
    seqorexe(sp);
}

void ami_pitch(ami_long p, ami_long t, ami_channel c, ami_long pt)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pitch;
    sp->vsc = c; sp->vsv = pt;
    seqorexe(sp);
}

void ami_pitchrange(ami_long p, ami_long t, ami_channel c, ami_long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pitchrange;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_mono(ami_long p, ami_long t, ami_channel c, ami_long ch)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_mono;
    sp->vsc = c; sp->vsv = ch;
    seqorexe(sp);
}

void ami_poly(ami_long p, ami_long t, ami_channel c)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_poly;
    sp->pc = c;
    seqorexe(sp);
}

/*******************************************************************************
*                                                                              *
*                    Synth file (MIDI) playback                                *
*                                                                              *
*******************************************************************************/

void ami_loadsynth(ami_long s, string fn)
{
    if (s < 1 || s > MAXMIDT) error("Invalid synth file slot");
    pthread_mutex_lock(&synlck);
    if (synfil[s - 1]) { free(synfil[s - 1]); synfil[s - 1] = NULL; }
    synfil[s - 1] = strdup(fn);
    pthread_mutex_unlock(&synlck);
}

void ami_delsynth(ami_long s)
{
    if (s < 1 || s > MAXMIDT) error("Invalid synth file slot");
    pthread_mutex_lock(&synlck);
    if (synfil[s - 1]) { free(synfil[s - 1]); synfil[s - 1] = NULL; }
    pthread_mutex_unlock(&synlck);
}

/* thread function: play a MIDI file via MusicPlayer through the DLS synth */
static void* playsynth_thread(void* arg)
{
    ami_long* args = (ami_long*)arg;
    ami_long port = args[0];
    ami_long sid  = args[1];
    free(args);

    /* get the filename */
    pthread_mutex_lock(&synlck);
    if (sid < 1 || sid > MAXMIDT || !synfil[sid - 1]) {
        pthread_mutex_unlock(&synlck);
        goto done;
    }
    char* fn = strdup(synfil[sid - 1]);
    pthread_mutex_unlock(&synlck);

    /* create the MusicSequence and load the file */
    MusicSequence seq = NULL;
    MusicPlayer player = NULL;
    OSStatus st;

    st = NewMusicSequence(&seq);
    if (st != noErr) { free(fn); goto done; }

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (const UInt8*)fn, strlen(fn), false);
    free(fn);
    if (!url) { DisposeMusicSequence(seq); goto done; }

    st = MusicSequenceFileLoad(seq, url,
                               kMusicSequenceFile_MIDIType, 0);
    CFRelease(url);
    if (st != noErr) { DisposeMusicSequence(seq); goto done; }

    /* set the sequence to play through port 1's AUGraph if available */
    synthoutdev* d = &synthout_tab[port - 1];
    if (d->issoft && d->graph)
        MusicSequenceSetAUGraph(seq, d->graph);

    st = NewMusicPlayer(&player);
    if (st != noErr) { DisposeMusicSequence(seq); goto done; }

    MusicPlayerSetSequence(player, seq);
    MusicPlayerPreroll(player);
    MusicPlayerStart(player);

    /* wait for playback to finish */
    MusicTimeStamp seqLen = 0;
    UInt32 ntracks;
    MusicSequenceGetTrackCount(seq, &ntracks);
    for (UInt32 i = 0; i < ntracks; i++) {
        MusicTrack track;
        MusicSequenceGetIndTrack(seq, i, &track);
        MusicTimeStamp tlen;
        UInt32 sz = sizeof(tlen);
        MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
                              &tlen, &sz);
        if (tlen > seqLen) seqLen = tlen;
    }
    seqLen += 4; /* add a few beats for reverb tail */

    Boolean isPlaying = TRUE;
    while (isPlaying) {
        usleep(100000); /* 100ms poll */
        MusicTimeStamp now;
        MusicPlayerGetTime(player, &now);
        if (now >= seqLen) isPlaying = FALSE;
        MusicPlayerIsPlaying(player, &isPlaying);
    }

    MusicPlayerStop(player);
    DisposeMusicPlayer(player);
    DisposeMusicSequence(seq);

done:
    pthread_mutex_lock(&snmlck);
    numseq--;
    if (numseq <= 0) { numseq = 0; pthread_cond_broadcast(&snmzer); }
    pthread_mutex_unlock(&snmlck);
    return NULL;
}

static void playsynth_kickoff(ami_long port, ami_long sid)
{
    pthread_mutex_lock(&snmlck);
    numseq++;
    pthread_mutex_unlock(&snmlck);

    ami_long* args = (ami_long*)malloc(2 * sizeof(ami_long));
    args[0] = port;
    args[1] = sid;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, playsynth_thread, args);
    pthread_attr_destroy(&attr);
}

void ami_playsynth(ami_long p, ami_long t, ami_long s)
{
    if (p < 1 || p > synthout_num) error("Invalid synth port");
    if (s < 1 || s > MAXMIDT) error("Invalid synth file slot");
    if (t == 0 || !seqrun) {
        playsynth_kickoff(p, s);
    } else {
        ami_seqptr sp;
        getseq(&sp);
        sp->port = p; sp->time = t; sp->st = st_playsynth;
        sp->sid = s;
        seqorexe(sp);
    }
}

void ami_waitsynth(ami_long p)
{
    pthread_mutex_lock(&snmlck);
    while (numseq > 0)
        pthread_cond_wait(&snmzer, &snmlck);
    pthread_mutex_unlock(&snmlck);
}

void ami_wrsynth(ami_long p, ami_seqptr sp)
{
    if (p < 1 || p > synthout_num) error("Invalid synth port");
    sp->port = p;
    excseq(sp);
}

void ami_rdsynth(ami_long p, ami_seqptr sp)
{
    if (p < 1 || p > synthin_num) error("Invalid synth input port");
    synthindev* d = &synthin_tab[p - 1];
    if (d->rdseq) { d->rdseq(p, sp); return; }
    if (!d->open) error("Synth input port not open");

    /* The call waits for the next message the sequencer record can carry,
       and decodes it as the other platforms do: channels, notes and
       instruments from 1, values to the full scale of a long. It returned
       an empty record at once before, there being no input, which a
       caller took for an endless run of note 0. */
    int b, p1, p2, done = FALSE;
    ami_long pv;

    memset(sp, 0, sizeof(ami_seqmsg));
    while (!done) {
        /* skip to a status byte when the stream has just been joined */
        do { b = midi_getb(d); } while (b >= 0 && !d->sync && b < 0x80);
        if (b < 0) return; /* closed under the read */
        d->sync = TRUE;
        if (b < 0x80) { d->pback = b; b = d->last; } /* running status */
        else if (b < 0xf0) d->last = b;
        sp->port = p;
        sp->time = seqrunin? timediff(&sintim): 0;
        switch (b >> 4) {
        case 0x8: case 0x9: case 0xa: /* note off, note on, aftertouch */
            if ((p1 = midi_getb(d)) < 0 || (p2 = midi_getb(d)) < 0) return;
            sp->st = (b >> 4) == 0x8? st_noteoff:
                     (b >> 4) == 0x9? st_noteon: st_aftertouch;
            sp->ntc = (b & 15) + 1;
            sp->ntn = p1 + 1;
            sp->ntv = p2 * (LONG_MAX / 128 + 1);
            done = TRUE;
            break;
        case 0xb: /* controller change, channel mode */
            if ((p1 = midi_getb(d)) < 0 || (p2 = midi_getb(d)) < 0) return;
            if (p1 == CTLR_MONO_OPERATION) {
                sp->st = st_mono; sp->vsc = (b & 15) + 1; sp->vsv = p2;
                done = TRUE;
            } else if (p1 == CTLR_POLY_OPERATION) {
                sp->st = st_poly; sp->pc = (b & 15) + 1;
                done = TRUE;
            } /* the other controllers are not carried: read on */
            break;
        case 0xc: /* program change */
            if ((p1 = midi_getb(d)) < 0) return;
            sp->st = st_instchange;
            sp->icc = (b & 15) + 1;
            sp->ici = p1 + 1;
            done = TRUE;
            break;
        case 0xd: /* channel pressure */
            if ((p1 = midi_getb(d)) < 0) return;
            sp->st = st_pressure;
            sp->ntc = (b & 15) + 1;
            sp->ntv = p1 * (LONG_MAX / 128 + 1);
            done = TRUE;
            break;
        case 0xe: /* pitch bend: 14 bits to full scale, the bottom clamped */
            if ((p1 = midi_getb(d)) < 0 || (p2 = midi_getb(d)) < 0) return;
            sp->st = st_pitch;
            sp->vsc = (b & 15) + 1;
            pv = ((ami_long)(p2 << 7 | p1) - 0x2000) * (LONG_MAX / 8192 + 1);
            if (pv < -LONG_MAX) pv = -LONG_MAX;
            sp->vsv = pv;
            done = TRUE;
            break;
        default: /* system messages carry nothing for the record: skipped */
            if (b == 0xf0) { /* system exclusive, to its end */
                do { p1 = midi_getb(d); } while (p1 >= 0 && p1 < 0x80);
                if (p1 < 0) return;
                if (p1 != 0xf7) d->pback = p1; /* ended by another status */
            } else if (b == 0xf2) { midi_getb(d); midi_getb(d); }
            else if (b == 0xf1 || b == 0xf3) midi_getb(d);
            d->last = 0;
            d->sync = FALSE; /* a status byte starts the next */
            if (d->pback >= 0x80) d->sync = TRUE;
            break;
        }
    }
}

/*******************************************************************************
*                                                                              *
*                    Wave output open/close                                    *
*                                                                              *
*******************************************************************************/

void ami_openwaveout(ami_long p)
{
    if (p < 1 || p > waveout_num) error("Invalid wave output port");
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->open) return;
    if (d->opn) { d->opn(p); d->open = TRUE; return; }
    d->fmtset = TRUE; /* defer AudioQueue creation until first write */
    d->open = TRUE;
}

static void wowait(waveoutdev* d, size_t lim); /* forward */

void ami_closewaveout(ami_long p)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (!d->open) return;
    if (d->cls) { d->cls(p); d->open = FALSE; return; }
    if (d->queue) {
        /* what was written is played out before the queue goes */
        UInt32 run = 1, sz;
        int i;

        wowait(d, 0);
        AudioQueueStop(d->queue, false); /* stops when the last is heard */
        for (i = 0; i < 100 && run; i++) {
            sz = sizeof(run);
            if (AudioQueueGetProperty(d->queue, kAudioQueueProperty_IsRunning,
                                      &run, &sz) != noErr) break;
            if (run) usleep(10000);
        }
        AudioQueueDispose(d->queue, true);
        d->queue = NULL;
    }
    d->open = FALSE;
}

void ami_chanwaveout(ami_long p, ami_long c)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->chanwavout) { d->chanwavout(p, c); return; }
    d->chan = c;
    d->fmtset = TRUE;
}

void ami_ratewaveout(ami_long p, ami_long r)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->ratewavout) { d->ratewavout(p, r); return; }
    d->rate = r;
    d->fmtset = TRUE;
}

void ami_lenwaveout(ami_long p, ami_long l)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->lenwavout) { d->lenwavout(p, l); return; }
    d->bits = l;
    d->fmtset = TRUE;
}

void ami_sgnwaveout(ami_long p, ami_long s)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->sgnwavout) { d->sgnwavout(p, s); return; }
    d->sgn = s;
    d->fmtset = TRUE;
}

void ami_fltwaveout(ami_long p, ami_long f)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->fltwavout) { d->fltwavout(p, f); return; }
    d->flt = f;
    d->fmtset = TRUE;
}

void ami_endwaveout(ami_long p, ami_long e)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->endwaveout) { d->endwaveout(p, e); return; }
    d->big = e;
    d->fmtset = TRUE;
}

/* AudioQueue output callback: does nothing (buffer reuse managed in wrwave) */
static pthread_mutex_t wolock = PTHREAD_MUTEX_INITIALIZER; /* pending counts */
static pthread_cond_t  wocond = PTHREAD_COND_INITIALIZER;

static void aq_output_cb(void* data, AudioQueueRef q,
                          AudioQueueBufferRef buf)
{
    /* the buffer has been played: it is counted off what is pending,
       which releases a writer or a close waiting on it, and freed (each
       write allocates its own, and none was ever freed) */
    waveoutdev* d = (waveoutdev*)data;

    pthread_mutex_lock(&wolock);
    if (d->pend >= buf->mAudioDataByteSize) d->pend -= buf->mAudioDataByteSize;
    else d->pend = 0;
    pthread_cond_broadcast(&wocond);
    pthread_mutex_unlock(&wolock);
    AudioQueueFreeBuffer(q, buf);
}

/* wait until no more than lim bytes are pending on a wave output, or the
   time they take to play, and a second, has gone by */
static void wowait(waveoutdev* d, size_t lim)
{
    struct timespec ts;
    size_t bps = (size_t)d->rate * d->chan * (d->flt? sizeof(float): (d->bits + 7) / 8);

    pthread_mutex_lock(&wolock);
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (bps? d->pend / bps: 0) + 2;
    while (d->pend > lim)
        if (pthread_cond_timedwait(&wocond, &wolock, &ts)) break;
    pthread_mutex_unlock(&wolock);
}

static void ensure_aq_out(waveoutdev* d)
{
    if (!d->fmtset || !d->open) return;

    /* tear down old queue if format changed */
    if (d->queue) {
        AudioQueueStop(d->queue, true);
        AudioQueueDispose(d->queue, true);
        d->queue = NULL;
    }

    AudioStreamBasicDescription fmt = {0};
    fmt.mSampleRate = d->rate;
    fmt.mChannelsPerFrame = d->chan;
    int bytesPerSample = (d->bits + 7) / 8;

    if (d->flt) {
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kAudioFormatFlagIsFloat |
                           kAudioFormatFlagIsPacked;
        if (d->big) fmt.mFormatFlags |= kAudioFormatFlagIsBigEndian;
        bytesPerSample = sizeof(float);
        fmt.mBitsPerChannel = 32;
    } else {
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kAudioFormatFlagIsPacked;
        if (d->sgn) fmt.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
        if (d->big) fmt.mFormatFlags |= kAudioFormatFlagIsBigEndian;
        fmt.mBitsPerChannel = d->bits;
    }
    fmt.mBytesPerFrame = bytesPerSample * fmt.mChannelsPerFrame;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;

    OSStatus st = AudioQueueNewOutput(&fmt, aq_output_cb, d,
                                       NULL, NULL, 0, &d->queue);
    if (st != noErr) error("Cannot create AudioQueue output");

    d->pend = 0;
    AudioQueueStart(d->queue, NULL);
    d->fmtset = FALSE;
}

void ami_wrwave(ami_long p, byte* buff, ami_long len)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (!d->open) return;
    if (d->wrwav) { d->wrwav(p, buff, len); return; }

    ensure_aq_out(d);

    /* write data to AudioQueue buffer */
    /* the length is in samples (one frame of every channel), as on the
       other platforms; it was taken as bytes, which played a fraction of
       what was given */
    UInt32 nb = (UInt32)(len * d->chan * (d->flt? (int)sizeof(float): (d->bits + 7) / 8));
    AudioQueueBufferRef buf;
    AudioQueueAllocateBuffer(d->queue, nb, &buf);
    memcpy(buf->mAudioData, buff, nb);
    buf->mAudioDataByteSize = nb;
    pthread_mutex_lock(&wolock);
    d->pend += nb;
    pthread_mutex_unlock(&wolock);
    if (AudioQueueEnqueueBuffer(d->queue, buf, 0, NULL) != noErr) {
        pthread_mutex_lock(&wolock);
        d->pend -= nb;
        pthread_mutex_unlock(&wolock);
        AudioQueueFreeBuffer(d->queue, buf);
        return;
    }
    /* The write holds the caller to the pace of the sound, a quarter
       second ahead of it, as a blocking device does. It returned at once
       before, and a close after it cut off what had been written. */
    wowait(d, (size_t)d->rate * d->chan * (d->flt? sizeof(float): (d->bits + 7) / 8) / 4);
}

/*******************************************************************************
*                                                                              *
*                    Wave file playback                                        *
*                                                                              *
*******************************************************************************/

void ami_loadwave(ami_long w, string fn)
{
    if (w < 1 || w > MAXWAVT) error("Invalid wave file slot");
    pthread_mutex_lock(&wavlck);
    if (wavfil[w - 1]) { free(wavfil[w - 1]); wavfil[w - 1] = NULL; }
    wavfil[w - 1] = strdup(fn);
    pthread_mutex_unlock(&wavlck);
}

void ami_delwave(ami_long w)
{
    if (w < 1 || w > MAXWAVT) error("Invalid wave file slot");
    pthread_mutex_lock(&wavlck);
    if (wavfil[w - 1]) { free(wavfil[w - 1]); wavfil[w - 1] = NULL; }
    pthread_mutex_unlock(&wavlck);
}

/* AudioQueue callback for file playback */
typedef struct {
    AudioFileID       audioFile;
    AudioQueueRef     queue;
    SInt64            packetPos;
    UInt32            numPacketsToRead;
    AudioStreamPacketDescription* pktDescs;
    int               done;
} aq_playinfo;

static void aq_play_cb(void* data, AudioQueueRef q,
                        AudioQueueBufferRef buf)
{
    aq_playinfo* info = (aq_playinfo*)data;
    if (info->done) return;

    UInt32 numPackets = info->numPacketsToRead;
    UInt32 numBytes = buf->mAudioDataBytesCapacity;

    OSStatus st = AudioFileReadPacketData(info->audioFile, false,
                                           &numBytes, info->pktDescs,
                                           info->packetPos, &numPackets,
                                           buf->mAudioData);
    if (st != noErr || numPackets == 0) {
        info->done = TRUE;
        AudioQueueStop(q, false);
        return;
    }
    buf->mAudioDataByteSize = numBytes;
    info->packetPos += numPackets;
    AudioQueueEnqueueBuffer(q, buf, info->pktDescs ? numPackets : 0,
                            info->pktDescs);
}

static void* playwave_thread(void* arg)
{
    ami_long* args = (ami_long*)arg;
    ami_long port = args[0];
    ami_long wt   = args[1];
    free(args);
    (void)port;

    /* get filename */
    pthread_mutex_lock(&wavlck);
    if (wt < 1 || wt > MAXWAVT || !wavfil[wt - 1]) {
        pthread_mutex_unlock(&wavlck);
        goto done;
    }
    char* fn = strdup(wavfil[wt - 1]);
    pthread_mutex_unlock(&wavlck);

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (const UInt8*)fn, strlen(fn), false);
    free(fn);
    if (!url) goto done;

    AudioFileID audioFile = NULL;
    OSStatus st = AudioFileOpenURL(url, kAudioFileReadPermission, 0,
                                    &audioFile);
    CFRelease(url);
    if (st != noErr || !audioFile) goto done;

    AudioStreamBasicDescription fmt;
    UInt32 sz = sizeof(fmt);
    AudioFileGetProperty(audioFile, kAudioFilePropertyDataFormat, &sz, &fmt);

    aq_playinfo info = {0};
    info.audioFile = audioFile;
    info.numPacketsToRead = WAVBUFSZ / fmt.mBytesPerPacket;
    if (info.numPacketsToRead == 0) info.numPacketsToRead = 1;

    /* for VBR, allocate packet descriptions */
    if (fmt.mBytesPerPacket == 0) {
        UInt32 maxpkt;
        sz = sizeof(maxpkt);
        AudioFileGetProperty(audioFile,
                             kAudioFilePropertyPacketSizeUpperBound,
                             &sz, &maxpkt);
        info.numPacketsToRead = WAVBUFSZ / maxpkt;
        if (info.numPacketsToRead == 0) info.numPacketsToRead = 1;
        info.pktDescs = (AudioStreamPacketDescription*)
            malloc(info.numPacketsToRead * sizeof(AudioStreamPacketDescription));
    }

    st = AudioQueueNewOutput(&fmt, aq_play_cb, &info,
                              NULL, NULL, 0, &info.queue);
    if (st != noErr) {
        AudioFileClose(audioFile);
        free(info.pktDescs);
        goto done;
    }

    /* prime buffers */
    AudioQueueBufferRef bufs[WAVBUFS];
    for (int i = 0; i < WAVBUFS; i++) {
        AudioQueueAllocateBuffer(info.queue, WAVBUFSZ, &bufs[i]);
        aq_play_cb(&info, info.queue, bufs[i]);
        if (info.done) break;
    }

    AudioQueueStart(info.queue, NULL);

    /* wait until done */
    while (!info.done) usleep(50000);
    usleep(200000); /* drain tail */

    AudioQueueStop(info.queue, true);
    AudioQueueDispose(info.queue, true);
    AudioFileClose(audioFile);
    free(info.pktDescs);

done:
    pthread_mutex_lock(&wnmlck);
    numwav--;
    if (numwav <= 0) { numwav = 0; pthread_cond_broadcast(&wnmzer); }
    pthread_mutex_unlock(&wnmlck);
    return NULL;
}

static void playwave_kickoff(ami_long port, ami_long wt)
{
    pthread_mutex_lock(&wnmlck);
    numwav++;
    pthread_mutex_unlock(&wnmlck);

    ami_long* args = (ami_long*)malloc(2 * sizeof(ami_long));
    args[0] = port;
    args[1] = wt;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, playwave_thread, args);
    pthread_attr_destroy(&attr);
}

void ami_playwave(ami_long p, ami_long t, ami_long w)
{
    if (p < 1 || p > waveout_num) error("Invalid wave output port");
    if (w < 1 || w > MAXWAVT) error("Invalid wave file slot");
    if (t == 0 || !seqrun) {
        playwave_kickoff(p, w);
    } else {
        ami_seqptr sp;
        getseq(&sp);
        sp->port = p; sp->time = t; sp->st = st_playwave;
        sp->wt = w;
        seqorexe(sp);
    }
}

void ami_volwave(ami_long p, ami_long t, ami_long v)
{
    /* not implemented at present */
    (void)p; (void)t; (void)v;
}

void ami_waitwave(ami_long p)
{
    (void)p;
    pthread_mutex_lock(&wnmlck);
    while (numwav > 0)
        pthread_cond_wait(&wnmzer, &wnmlck);
    pthread_mutex_unlock(&wnmlck);
}

/*******************************************************************************
*                                                                              *
*                    Wave input                                                *
*                                                                              *
*******************************************************************************/

/* the input queue filled a buffer: its bytes go to the ring, the oldest
   giving way when the reader has fallen behind, and the buffer goes back */
static void aq_input_cb(void* user, AudioQueueRef q, AudioQueueBufferRef buf,
                        const AudioTimeStamp* ts, UInt32 npkt,
                        const AudioStreamPacketDescription* pd)
{
    waveindev* d = (waveindev*)user;
    const byte* src = (const byte*)buf->mAudioData;
    size_t n = buf->mAudioDataByteSize;

    (void)ts; (void)npkt; (void)pd;
    pthread_mutex_lock(&d->rlock);
    if (d->ring) {
        if (n > d->ringsz) { src += n - d->ringsz; n = d->ringsz; }
        for (size_t i = 0; i < n; i++) {
            d->ring[d->ringhd] = src[i];
            d->ringhd = (d->ringhd + 1) % d->ringsz;
        }
        d->ringn += n;
        if (d->ringn > d->ringsz) d->ringn = d->ringsz;
        pthread_cond_broadcast(&d->rdata);
    }
    pthread_mutex_unlock(&d->rlock);
    if (d->open) AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

void ami_openwavein(ami_long p)
{
    if (p < 1 || p > wavein_num) error("Invalid wave input port");
    waveindev* d = &wavein_tab[p - 1];
    if (d->open) return;
    if (d->opn) { d->opn(p); d->open = TRUE; return; }

    /* An AudioQueue records in the port's format into a ring that
       ami_rdwave() empties. The ring holds two seconds: a reader slower
       than that loses the oldest sound. */
    AudioStreamBasicDescription fmt = {0};
    int bytesPerSample = (d->bits + 7) / 8;
    fmt.mSampleRate = d->rate;
    fmt.mChannelsPerFrame = d->chan;
    fmt.mFormatID = kAudioFormatLinearPCM;
    if (d->flt) {
        fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        bytesPerSample = sizeof(float);
        fmt.mBitsPerChannel = 32;
    } else {
        fmt.mFormatFlags = kAudioFormatFlagIsPacked;
        if (d->sgn) fmt.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
        fmt.mBitsPerChannel = d->bits;
    }
    if (d->big) fmt.mFormatFlags |= kAudioFormatFlagIsBigEndian;
    fmt.mBytesPerFrame = bytesPerSample * fmt.mChannelsPerFrame;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;

    pthread_mutex_init(&d->rlock, NULL);
    pthread_cond_init(&d->rdata, NULL);
    d->ringsz = (size_t)d->rate * 2 * fmt.mBytesPerFrame;
    d->ring = (byte*)malloc(d->ringsz);
    if (!d->ring) error("No memory for wave input");
    d->ringhd = d->ringn = 0;

    if (AudioQueueNewInput(&fmt, aq_input_cb, d, NULL, NULL, 0,
                           &d->queue) != noErr)
        error("Cannot create AudioQueue input");
    for (int i = 0; i < WAVBUFS; i++) {
        AudioQueueBufferRef buf;
        AudioQueueAllocateBuffer(d->queue, WAVBUFSZ, &buf);
        AudioQueueEnqueueBuffer(d->queue, buf, 0, NULL);
    }
    if (AudioQueueStart(d->queue, NULL) != noErr)
        error("Cannot start wave input");
    d->open = TRUE;
}

void ami_closewavein(ami_long p)
{
    if (p < 1 || p > wavein_num) return;
    waveindev* d = &wavein_tab[p - 1];
    if (!d->open) return;
    if (d->cls) { d->cls(p); d->open = FALSE; return; }
    d->open = FALSE; /* the callback gives no more buffers back */
    if (d->queue) {
        AudioQueueStop(d->queue, true);
        AudioQueueDispose(d->queue, true);
        d->queue = NULL;
    }
    pthread_mutex_lock(&d->rlock);
    free(d->ring);
    d->ring = NULL;
    d->ringn = 0;
    pthread_cond_broadcast(&d->rdata); /* release a reader */
    pthread_mutex_unlock(&d->rlock);
}

ami_long ami_chanwavein(ami_long p)
{
    if (p < 1 || p > wavein_num) return 1;
    waveindev* d = &wavein_tab[p - 1];
    if (d->chanwavin) return d->chanwavin(p);
    return d->chan;
}

ami_long ami_ratewavein(ami_long p)
{
    if (p < 1 || p > wavein_num) return 44100;
    waveindev* d = &wavein_tab[p - 1];
    if (d->ratewavin) return d->ratewavin(p);
    return d->rate;
}

ami_long ami_lenwavein(ami_long p)
{
    if (p < 1 || p > wavein_num) return 16;
    waveindev* d = &wavein_tab[p - 1];
    if (d->lenwavin) return d->lenwavin(p);
    return d->bits;
}

ami_long ami_sgnwavein(ami_long p)
{
    if (p < 1 || p > wavein_num) return TRUE;
    waveindev* d = &wavein_tab[p - 1];
    if (d->sgnwavin) return d->sgnwavin(p);
    return d->sgn;
}

ami_long ami_endwavein(ami_long p)
{
    if (p < 1 || p > wavein_num) return FALSE;
    waveindev* d = &wavein_tab[p - 1];
    if (d->endwavein) return d->endwavein(p);
    return d->big;
}

ami_long ami_fltwavein(ami_long p)
{
    if (p < 1 || p > wavein_num) return FALSE;
    waveindev* d = &wavein_tab[p - 1];
    if (d->fltwavin) return d->fltwavin(p);
    return d->flt;
}

ami_long ami_rdwave(ami_long p, byte* buff, ami_long len)
{
    if (p < 1 || p > wavein_num) return 0;
    waveindev* d = &wavein_tab[p - 1];
    if (d->rdwav) return d->rdwav(p, buff, len);
    if (!d->open || len <= 0) return 0;

    /* The length is in samples (one frame of every channel), as on the
       other platforms. The call waits for sound, and returns what has
       arrived, at least one sample and no more than asked. It returned
       nothing at all before, there being no input, and a caller reading
       until it had its count never came back. */
    size_t frm = (size_t)d->chan * (d->flt? sizeof(float): (d->bits + 7) / 8);
    size_t n, tl, i;

    pthread_mutex_lock(&d->rlock);
    while (d->ring && d->ringn < frm) pthread_cond_wait(&d->rdata, &d->rlock);
    if (!d->ring) { pthread_mutex_unlock(&d->rlock); return 0; }
    n = d->ringn / frm;
    if (n > (size_t)len) n = (size_t)len;
    tl = (d->ringhd + d->ringsz - d->ringn) % d->ringsz; /* the oldest byte */
    for (i = 0; i < n * frm; i++) buff[i] = d->ring[(tl + i) % d->ringsz];
    d->ringn -= n * frm;
    pthread_mutex_unlock(&d->rlock);
    return (ami_long)n;
}

/*******************************************************************************
*                                                                              *
*                    Device parameters                                         *
*                                                                              *
*******************************************************************************/

/* Parameter values are returned by the critical buffer convention: a result
   that fills the entire buffer is not zero terminated, a shorter result is
   zero terminated, and it is an error if the result cannot fit. */

void ami_getparamsynthout(ami_long p, string name, string value, ami_long len)
{
    if (p < 1 || p > synthout_num) { cpycrit(value, len, ""); return; }
    synthoutdev* d = &synthout_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    cpycrit(value, len, "");
}

void ami_getparamsynthin(ami_long p, string name, string value, ami_long len)
{
    if (p < 1 || p > synthin_num) { cpycrit(value, len, ""); return; }
    synthindev* d = &synthin_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    cpycrit(value, len, "");
}

void ami_getparamwaveout(ami_long p, string name, string value, ami_long len)
{
    if (p < 1 || p > waveout_num) { cpycrit(value, len, ""); return; }
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    cpycrit(value, len, "");
}

void ami_getparamwavein(ami_long p, string name, string value, ami_long len)
{
    if (p < 1 || p > wavein_num) { cpycrit(value, len, ""); return; }
    waveindev* d = &wavein_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    cpycrit(value, len, "");
}

ami_long ami_setparamsynthout(ami_long p, string name, string value)
{
    if (p < 1 || p > synthout_num) return 1;
    synthoutdev* d = &synthout_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

ami_long ami_setparamsynthin(ami_long p, string name, string value)
{
    if (p < 1 || p > synthin_num) return 1;
    synthindev* d = &synthin_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

ami_long ami_setparamwaveout(ami_long p, string name, string value)
{
    if (p < 1 || p > waveout_num) return 1;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

ami_long ami_setparamwavein(ami_long p, string name, string value)
{
    if (p < 1 || p > wavein_num) return 1;
    waveindev* d = &wavein_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

/*******************************************************************************
*                                                                              *
*                    Plug-in registration                                      *
*                                                                              *
*******************************************************************************/

void _pa_synthoutplug(ami_long addend, string name,
                      void (*opnseq)(ami_long p), void (*clsseq)(ami_long p),
                      void (*wrseq)(ami_long p, ami_seqptr sp),
                      ami_long (*setparam)(ami_long p, string name, string value),
                      void (*getparam)(ami_long p, string name, string value,
                                       ami_long len))
{
    if (addend) {
        if (synthout_num >= MAXMIDP) return;
        synthoutdev* d = &synthout_tab[synthout_num];
        memset(d, 0, sizeof(*d));
        d->opn = opnseq; d->cls = clsseq; d->wrseq = wrseq;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        synthout_num++;
        synthout_plug++;
    } else {
        /* insert at front */
        if (synthout_num >= MAXMIDP) return;
        memmove(&synthout_tab[1], &synthout_tab[0],
                synthout_num * sizeof(synthoutdev));
        memset(&synthout_tab[0], 0, sizeof(synthoutdev));
        synthout_tab[0].opn = opnseq;
        synthout_tab[0].cls = clsseq;
        synthout_tab[0].wrseq = wrseq;
        synthout_tab[0].setparam = setparam;
        synthout_tab[0].getparam = getparam;
        strncpy(synthout_tab[0].name, name,
                sizeof(synthout_tab[0].name) - 1);
        synthout_num++;
        synthout_plug++;
    }
}

void _pa_synthinplug(ami_long addend, string name,
                     void (*opnseq)(ami_long p), void (*clsseq)(ami_long p),
                     void (*rdseq)(ami_long p, ami_seqptr sp),
                     ami_long (*setparam)(ami_long p, string name, string value),
                     void (*getparam)(ami_long p, string name, string value,
                                      ami_long len))
{
    if (addend) {
        if (synthin_num >= MAXMIDP) return;
        synthindev* d = &synthin_tab[synthin_num];
        memset(d, 0, sizeof(*d));
        d->opn = opnseq; d->cls = clsseq; d->rdseq = rdseq;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        synthin_num++;
        synthin_plug++;
    } else {
        if (synthin_num >= MAXMIDP) return;
        memmove(&synthin_tab[1], &synthin_tab[0],
                synthin_num * sizeof(synthindev));
        memset(&synthin_tab[0], 0, sizeof(synthindev));
        synthin_tab[0].opn = opnseq;
        synthin_tab[0].cls = clsseq;
        synthin_tab[0].rdseq = rdseq;
        synthin_tab[0].setparam = setparam;
        synthin_tab[0].getparam = getparam;
        strncpy(synthin_tab[0].name, name,
                sizeof(synthin_tab[0].name) - 1);
        synthin_num++;
        synthin_plug++;
    }
}

void _pa_waveoutplug(ami_long addend, string name,
                     void (*open)(ami_long p), void (*close)(ami_long p),
                     void (*chanwavout)(ami_long p, ami_long c),
                     void (*ratewavout)(ami_long p, ami_long r),
                     void (*lenwavout)(ami_long p, ami_long l),
                     void (*sgnwavout)(ami_long p, ami_long s),
                     void (*fltwavout)(ami_long p, ami_long f),
                     void (*endwaveout)(ami_long p, ami_long e),
                     void (*wrwav)(ami_long p, byte* buff, ami_long len),
                     ami_long (*setparam)(ami_long p, string name, string value),
                     void (*getparam)(ami_long p, string name, string value,
                                      ami_long len))
{
    if (addend) {
        if (waveout_num >= MAXWAVP) return;
        waveoutdev* d = &waveout_tab[waveout_num];
        memset(d, 0, sizeof(*d));
        d->opn = open; d->cls = close;
        d->chanwavout = chanwavout; d->ratewavout = ratewavout;
        d->lenwavout = lenwavout; d->sgnwavout = sgnwavout;
        d->fltwavout = fltwavout; d->endwaveout = endwaveout;
        d->wrwav = wrwav;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        waveout_num++;
        waveout_plug++;
    } else {
        if (waveout_num >= MAXWAVP) return;
        memmove(&waveout_tab[1], &waveout_tab[0],
                waveout_num * sizeof(waveoutdev));
        memset(&waveout_tab[0], 0, sizeof(waveoutdev));
        waveout_tab[0].opn = open; waveout_tab[0].cls = close;
        waveout_tab[0].chanwavout = chanwavout;
        waveout_tab[0].ratewavout = ratewavout;
        waveout_tab[0].lenwavout = lenwavout;
        waveout_tab[0].sgnwavout = sgnwavout;
        waveout_tab[0].fltwavout = fltwavout;
        waveout_tab[0].endwaveout = endwaveout;
        waveout_tab[0].wrwav = wrwav;
        waveout_tab[0].setparam = setparam;
        waveout_tab[0].getparam = getparam;
        strncpy(waveout_tab[0].name, name,
                sizeof(waveout_tab[0].name) - 1);
        waveout_num++;
        waveout_plug++;
    }
}

void _pa_waveinplug(ami_long addend, string name,
                    void (*open)(ami_long p), void (*close)(ami_long p),
                    ami_long (*chanwavin)(ami_long p), ami_long (*ratewavin)(ami_long p),
                    ami_long (*lenwavin)(ami_long p), ami_long (*sgnwavin)(ami_long p),
                    ami_long (*fltwavin)(ami_long p), ami_long (*endwavein)(ami_long p),
                    ami_long (*rdwav)(ami_long p, byte* buff, ami_long len),
                    ami_long (*setparam)(ami_long p, string name, string value),
                    void (*getparam)(ami_long p, string name, string value,
                                     ami_long len))
{
    if (addend) {
        if (wavein_num >= MAXWAVP) return;
        waveindev* d = &wavein_tab[wavein_num];
        memset(d, 0, sizeof(*d));
        d->opn = open; d->cls = close;
        d->chanwavin = chanwavin; d->ratewavin = ratewavin;
        d->lenwavin = lenwavin; d->sgnwavin = sgnwavin;
        d->fltwavin = fltwavin; d->endwavein = endwavein;
        d->rdwav = rdwav;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        wavein_num++;
        wavein_plug++;
    } else {
        if (wavein_num >= MAXWAVP) return;
        memmove(&wavein_tab[1], &wavein_tab[0],
                wavein_num * sizeof(waveindev));
        memset(&wavein_tab[0], 0, sizeof(waveindev));
        wavein_tab[0].opn = open; wavein_tab[0].cls = close;
        wavein_tab[0].chanwavin = chanwavin;
        wavein_tab[0].ratewavin = ratewavin;
        wavein_tab[0].lenwavin = lenwavin;
        wavein_tab[0].sgnwavin = sgnwavin;
        wavein_tab[0].fltwavin = fltwavin;
        wavein_tab[0].endwavein = endwavein;
        wavein_tab[0].rdwav = rdwav;
        wavein_tab[0].setparam = setparam;
        wavein_tab[0].getparam = getparam;
        strncpy(wavein_tab[0].name, name,
                sizeof(wavein_tab[0].name) - 1);
        wavein_num++;
        wavein_plug++;
    }
}

#pragma clang diagnostic pop
