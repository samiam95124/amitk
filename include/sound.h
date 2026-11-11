/*******************************************************************************
*                                                                              *
*                               SOUND LIBRARY                                  *
*                                                                              *
*                              11/02 S. A. Moore                               *
*                                                                              *
* Sndlib is a combination of wave file and midi output and control functions.  *
* Implements a set of midi controls and wave controls. Also includes a "flow   *
* through sequencer" function. Each event has a timestamp, and if zero, it     *
* is performed immediately, otherwise scheduled. This allows any mix of        *
* immediate vs. sequenced events.                                              *
*                                                                              *
* Notes:                                                                       *
*                                                                              *
* 1. The parameter convertion work is being performed when a sequenced item    *
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
* String output buffers are "critical": a result may occupy the entire        *
* buffer, in which case the terminating zero is left off. Results shorter     *
* than the buffer are zero terminated. C callers should use length limited    *
* reads of such buffers (see strnlen and similar).                             *
*                                                                              *
*******************************************************************************/

#ifndef __SOUND_H__
#define __SOUND_H__

#ifdef __cplusplus
extern "C" {
#endif

/* PA local defines */
#include <localdefs.h>

#define AMI_CHAN_DRUM 10 /* the GM drum channel */

#define AMI_SYNTH_OUT 1 /* the default output synth for host */
#define AMI_SYNTH_IN  1 /* The default input from external synth */
#define AMI_WAVE_IN   1 /* the default wave input for host */
#define AMI_WAVE_OUT  1 /* the default output wave for host */

/* the notes in the lowest octave */

#define AMI_NOTE_C       1
#define AMI_NOTE_C_SHARP 2
#define AMI_NOTE_D_FLAT  2
#define AMI_NOTE_D       3
#define AMI_NOTE_D_SHARP 4
#define AMI_NOTE_E_FLAT  4
#define AMI_NOTE_E       5
#define AMI_NOTE_F       6
#define AMI_NOTE_F_SHARP 7
#define AMI_NOTE_G_FLAT  7
#define AMI_NOTE_G       8
#define AMI_NOTE_G_SHARP 9
#define AMI_NOTE_A_FLAT  9
#define AMI_NOTE_A       10
#define AMI_NOTE_A_SHARP 11
#define AMI_NOTE_B_FLAT  11
#define AMI_NOTE_B       12

/* the octaves of midi, add to note to place in that octave */

#define AMI_OCTAVE_1  0
#define AMI_OCTAVE_2  12
#define AMI_OCTAVE_3  24
#define AMI_OCTAVE_4  36
#define AMI_OCTAVE_5  48
#define AMI_OCTAVE_6  60
#define AMI_OCTAVE_7  72
#define AMI_OCTAVE_8  84
#define AMI_OCTAVE_9  96
#define AMI_OCTAVE_10 108
#define AMI_OCTAVE_11 120

/* Standard GM instruments */

/* Piano */

#define AMI_INST_ACOUSTIC_GRAND        1
#define AMI_INST_BRIGHT_ACOUSTIC       2
#define AMI_INST_ELECTRIC_GRAND        3
#define AMI_INST_HONKY_TONK            4
#define AMI_INST_ELECTRIC_PIANO_1      5
#define AMI_INST_ELECTRIC_PIANO_2      6
#define AMI_INST_HARPSICHORD           7
#define AMI_INST_CLAVINET              8

/* Chromatic percussion */

#define AMI_INST_CELESTA               9
#define AMI_INST_GLOCKENSPIEL          10
#define AMI_INST_MUSIC_BOX             11
#define AMI_INST_VIBRAPHONE            12
#define AMI_INST_MARIMBA               13
#define AMI_INST_XYLOPHONE             14
#define AMI_INST_TUBULAR_BELLS         15
#define AMI_INST_DULCIMER              16

/* Organ */

#define AMI_INST_DRAWBAR_ORGAN         17
#define AMI_INST_PERCUSSIVE_ORGAN      18
#define AMI_INST_ROCK_ORGAN            19
#define AMI_INST_CHURCH_ORGAN          20
#define AMI_INST_REED_ORGAN            21
#define AMI_INST_ACCORIDAN             22
#define AMI_INST_HARMONICA             23
#define AMI_INST_TANGO_ACCORDIAN       24

/* Guitar */

#define AMI_INST_NYLON_STRING_GUITAR   25
#define AMI_INST_STEEL_STRING_GUITAR   26
#define AMI_INST_ELECTRIC_JAZZ_GUITAR  27
#define AMI_INST_ELECTRIC_CLEAN_GUITAR 28
#define AMI_INST_ELECTRIC_MUTED_GUITAR 29
#define AMI_INST_OVERDRIVEN_GUITAR     30
#define AMI_INST_DISTORTION_GUITAR     31
#define AMI_INST_GUITAR_HARMONICS      32

/* Bass */

#define AMI_INST_ACOUSTIC_BASS         33
#define AMI_INST_ELECTRIC_BASS_FINGER  34
#define AMI_INST_ELECTRIC_BASS_PICK    35
#define AMI_INST_FRETLESS_BASS         36
#define AMI_INST_SLAP_BASS_1           37
#define AMI_INST_SLAP_BASS_2           38
#define AMI_INST_SYNTH_BASS_1          39
#define AMI_INST_SYNTH_BASS_2          40

/* Solo strings */

#define AMI_INST_VIOLIN                41
#define AMI_INST_VIOLA                 42
#define AMI_INST_CELLO                 43
#define AMI_INST_CONTRABASS            44
#define AMI_INST_TREMOLO_STRINGS       45
#define AMI_INST_PIZZICATO_STRINGS     46
#define AMI_INST_ORCHESTRAL_STRINGS    47
#define AMI_INST_TIMPANI               48

/* Ensemble */

#define AMI_INST_STRING_ENSEMBLE_1     49
#define AMI_INST_STRING_ENSEMBLE_2     50
#define AMI_INST_SYNTHSTRINGS_1        51
#define AMI_INST_SYNTHSTRINGS_2        52
#define AMI_INST_CHOIR_AAHS            53
#define AMI_INST_VOICE_OOHS            54
#define AMI_INST_SYNTH_VOICE           55
#define AMI_INST_ORCHESTRA_HIT         56

/* Brass */

#define AMI_INST_TRUMPET               57
#define AMI_INST_TROMBONE              58
#define AMI_INST_TUBA                  59
#define AMI_INST_MUTED_TRUMPET         60
#define AMI_INST_FRENCH_HORN           61
#define AMI_INST_BRASS_SECTION         62
#define AMI_INST_SYNTHBRASS_1          63
#define AMI_INST_SYNTHBRASS_2          64

/* Reed */

#define AMI_INST_SOPRANO_SAX           65
#define AMI_INST_ALTO_SAX              66
#define AMI_INST_TENOR_SAX             67
#define AMI_INST_BARITONE_SAX          68
#define AMI_INST_OBOE                  69
#define AMI_INST_ENGLISH_HORN          70
#define AMI_INST_BASSOON               71
#define AMI_INST_CLARINET              72

/* Pipe */

#define AMI_INST_PICCOLO               73
#define AMI_INST_FLUTE                 74
#define AMI_INST_RECORDER              75
#define AMI_INST_PAN_FLUTE             76
#define AMI_INST_BLOWN_BOTTLE          77
#define AMI_INST_SKAKUHACHI            78
#define AMI_INST_WHISTLE               79
#define AMI_INST_OCARINA               80

/* Synth lead */

#define AMI_INST_LEAD_1_SQUARE         81
#define AMI_INST_LEAD_2_SAWTOOTH       82
#define AMI_INST_LEAD_3_CALLIOPE       83
#define AMI_INST_LEAD_4_CHIFF          84
#define AMI_INST_LEAD_5_CHARANG        85
#define AMI_INST_LEAD_6_VOICE          86
#define AMI_INST_LEAD_7_FIFTHS         87
#define AMI_INST_LEAD_8_BASS_LEAD      88

/* Synth pad */

#define AMI_INST_PAD_1_NEW_AGE         89
#define AMI_INST_PAD_2_WARM            90
#define AMI_INST_PAD_3_POLYSYNTH       91
#define AMI_INST_PAD_4_CHOIR           92
#define AMI_INST_PAD_5_BOWED           93
#define AMI_INST_PAD_6_METALLIC        94
#define AMI_INST_PAD_7_HALO            95
#define AMI_INST_PAD_8_SWEEP           96

/* Synth effects */

#define AMI_INST_FX_1_RAIN             97
#define AMI_INST_FX_2_SOUNDTRACK       98
#define AMI_INST_FX_3_CRYSTAL          99
#define AMI_INST_FX_4_ATMOSPHERE       100
#define AMI_INST_FX_5_BRIGHTNESS       101
#define AMI_INST_FX_6_GOBLINS          102
#define AMI_INST_FX_7_ECHOES           103
#define AMI_INST_FX_8_SCI_FI           104

/* Ethnic */

#define AMI_INST_SITAR                 105
#define AMI_INST_BANJO                 106
#define AMI_INST_SHAMISEN              107
#define AMI_INST_KOTO                  108
#define AMI_INST_KALIMBA               109
#define AMI_INST_BAGPIPE               110
#define AMI_INST_FIDDLE                111
#define AMI_INST_SHANAI                112

/* Percussive */

#define AMI_INST_TINKLE_BELL           113
#define AMI_INST_AGOGO                 114
#define AMI_INST_STEEL_DRUMS           115
#define AMI_INST_WOODBLOCK             116
#define AMI_INST_TAIKO_DRUM            117
#define AMI_INST_MELODIC_TOM           118
#define AMI_INST_SYNTH_DRUM            119
#define AMI_INST_REVERSE_CYMBAL        120

/* Sound effects */

#define AMI_INST_GUITAR_FRET_NOISE     121
#define AMI_INST_BREATH_NOISE          122
#define AMI_INST_SEASHORE              123
#define AMI_INST_BIRD_TWEET            124
#define AMI_INST_TELEPHONE_RING        125
#define AMI_INST_HELICOPTER            126
#define AMI_INST_APPLAUSE              127
#define AMI_INST_GUNSHOT               128

/* Drum sounds, activated as notes to drum instruments */

#define AMI_NOTE_ACOUSTIC_BASS_DRUM 35
#define AMI_NOTE_BASS_DRUM_1        36
#define AMI_NOTE_SIDE_STICK         37
#define AMI_NOTE_ACOUSTIC_SNARE     38
#define AMI_NOTE_HAND_CLAP          39
#define AMI_NOTE_ELECTRIC_SNARE     40
#define AMI_NOTE_LOW_FLOOR_TOM      41
#define AMI_NOTE_CLOSED_HI_HAT      42
#define AMI_NOTE_HIGH_FLOOR_TOM     43
#define AMI_NOTE_PEDAL_HI_HAT       44
#define AMI_NOTE_LOW_TOM            45
#define AMI_NOTE_OPEN_HI_HAT        46
#define AMI_NOTE_LOW_MID_TOM        47
#define AMI_NOTE_HI_MID_TOM         48
#define AMI_NOTE_CRASH_CYMBAL_1     49
#define AMI_NOTE_HIGH_TOM           50
#define AMI_NOTE_RIDE_CYMBAL_1      51
#define AMI_NOTE_CHINESE_CYMBAL     52
#define AMI_NOTE_RIDE_BELL          53
#define AMI_NOTE_TAMBOURINE         54
#define AMI_NOTE_SPLASH_CYMBAL      55
#define AMI_NOTE_COWBELL            56
#define AMI_NOTE_CRASH_CYMBAL_2     57
#define AMI_NOTE_VIBRASLAP          58
#define AMI_NOTE_RIDE_CYMBAL_2      59
#define AMI_NOTE_HI_BONGO           60
#define AMI_NOTE_LOW_BONGO          61
#define AMI_NOTE_MUTE_HI_CONGA      62
#define AMI_NOTE_OPEN_HI_CONGA      63
#define AMI_NOTE_LOW_CONGA          64
#define AMI_NOTE_HIGH_TIMBALE       65
#define AMI_NOTE_LOW_TIMBALE        66
#define AMI_NOTE_HIGH_AGOGO         67
#define AMI_NOTE_LOW_AGOGO          68
#define AMI_NOTE_CABASA             69
#define AMI_NOTE_MARACAS            70
#define AMI_NOTE_SHORT_WHISTLE      71
#define AMI_NOTE_LONG_WHISTLE       72
#define AMI_NOTE_SHORT_GUIRO        73
#define AMI_NOTE_LONG_GUIRO         74
#define AMI_NOTE_CLAVES             75
#define AMI_NOTE_HI_WOOD_BLOCK      76
#define AMI_NOTE_LOW_WOOD_BLOCK     77
#define AMI_NOTE_MUTE_CUICA         78
#define AMI_NOTE_OPEN_CUICA         79
#define AMI_NOTE_MUTE_TRIANGLE      80
#define AMI_NOTE_OPEN_TRIANGLE      81

/* common types */

typedef ami_long ami_note;       /* 1..128  note number for midi */
typedef ami_long ami_channel;    /* 1..16   channel number */
typedef ami_long ami_instrument; /* 1..128  instrument number */

/* sequencer message types. each routine with a sequenced option has a
  sequencer message assocated with it */

typedef enum {
    st_noteon, st_noteoff, st_instchange, st_attack, st_release,
    st_legato, st_portamento, st_vibrato, st_volsynthchan, st_porttime,
    st_balance, st_pan, st_timbre, st_brightness, st_reverb, st_tremulo,
    st_chorus, st_celeste, st_phaser, st_aftertouch, st_pressure,
    st_pitch, st_pitchrange, st_mono, st_poly, st_playsynth,
    st_playwave, st_volwave
} ami_seqtyp;

/* sequencer message */

typedef struct ami_seqmsg {

    struct ami_seqmsg* next; /* next message in list */
    ami_long           port; /* port to which message applies */
    ami_long           time; /* time to execute message */
    ami_seqtyp         st;   /* type of message */
    union {

        /* st_noteon st_noteoff st_aftertouch st_pressure */
        struct { ami_channel ntc; ami_note ntn; ami_long ntv; };
        /* st_instchange */ struct { ami_channel icc; ami_instrument ici; };
        /* st_attack, st_release, st_vibrato, st_volsynthchan, st_porttime,
           st_balance, st_pan, st_timbre, st_brightness, st_reverb, st_tremulo,
           st_chorus, st_celeste, st_phaser, st_pitch, st_pitchrange,
           st_mono */ struct { ami_channel vsc; ami_long vsv; };
        /* st_poly */ ami_channel pc;
        /* st_legato, st_portamento */ struct { ami_channel bsc; ami_long bsb; };
        /* st_playsynth */ ami_long sid;
        /* st_playwave */ ami_long wt;
        /* st_volwave */ ami_long wv;

    };

} ami_seqmsg;

/* pointer to message */

typedef ami_seqmsg* ami_seqptr;

/* functions at this level */

void ami_starttimeout(void);
void ami_stoptimeout(void);
ami_long ami_curtimeout(void);
void ami_starttimein(void);
void ami_stoptimein(void);
ami_long ami_curtimein(void);
ami_long ami_synthout(void);
ami_long ami_synthin(void);
void ami_opensynthout(ami_long p);
void ami_closesynthout(ami_long p);
void ami_opensynthin(ami_long p);
void ami_closesynthin(ami_long p);
void ami_noteon(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long v);
void ami_noteoff(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long v);
void ami_instchange(ami_long p, ami_long t, ami_channel c, ami_instrument i);
void ami_attack(ami_long p, ami_long t, ami_channel c, ami_long at);
void ami_release(ami_long p, ami_long t, ami_channel c, ami_long rt);
void ami_legato(ami_long p, ami_long t, ami_channel c, ami_long b);
void ami_portamento(ami_long p, ami_long t, ami_channel c, ami_long b);
void ami_vibrato(ami_long p, ami_long t, ami_channel c, ami_long v);
void ami_volsynthchan(ami_long p, ami_long t, ami_channel c, ami_long v);
void ami_porttime(ami_long p, ami_long t, ami_channel c, ami_long v);
void ami_balance(ami_long p, ami_long t, ami_channel c, ami_long b);
void ami_pan(ami_long p, ami_long t, ami_channel c, ami_long b);
void ami_timbre(ami_long p, ami_long t, ami_channel c, ami_long tb);
void ami_brightness(ami_long p, ami_long t, ami_channel c, ami_long b);
void ami_reverb(ami_long p, ami_long t, ami_channel c, ami_long r);
void ami_tremulo(ami_long p, ami_long t, ami_channel c, ami_long tr);
void ami_chorus(ami_long p, ami_long t, ami_channel c, ami_long cr);
void ami_celeste(ami_long p, ami_long t, ami_channel c, ami_long ce);
void ami_phaser(ami_long p, ami_long t, ami_channel c, ami_long ph);
void ami_aftertouch(ami_long p, ami_long t, ami_channel c, ami_note n, ami_long at);
void ami_pressure(ami_long p, ami_long t, ami_channel c, ami_long pr);
void ami_pitch(ami_long p, ami_long t, ami_channel c, ami_long pt);
void ami_pitchrange(ami_long p, ami_long t, ami_channel c, ami_long v);
void ami_mono(ami_long p, ami_long t, ami_channel c, ami_long ch);
void ami_poly(ami_long p, ami_long t, ami_channel c);
void ami_loadsynth(ami_long s, string sf);
void ami_playsynth(ami_long p, ami_long t, ami_long s);
void ami_delsynth(ami_long s);
void ami_waitsynth(ami_long p);
void ami_wrsynth(ami_long p, ami_seqptr sp);
void ami_rdsynth(ami_long p, ami_seqptr sp);
ami_long ami_waveout(void);
ami_long ami_wavein(void);
void ami_openwaveout(ami_long p);
void ami_closewaveout(ami_long p);
ami_long ami_getwaveid(void);
void ami_loadwave(ami_long w, string fn);
void ami_playwave(ami_long p, ami_long t, ami_long w);
void ami_delwave(ami_long w);
void ami_volwave(ami_long p, ami_long t, ami_long v);
void ami_waitwave(ami_long p);
void ami_chanwaveout(ami_long p, ami_long c);
void ami_ratewaveout(ami_long p, ami_long r);
void ami_lenwaveout(ami_long p, ami_long l);
void ami_sgnwaveout(ami_long p, ami_long s);
void ami_fltwaveout(ami_long p, ami_long f);
void ami_endwaveout(ami_long p, ami_long e);
void ami_wrwave(ami_long p, byte* buff, ami_long len);
void ami_openwavein(ami_long p);
void ami_closewavein(ami_long p);
ami_long ami_chanwavein(ami_long p);
ami_long ami_ratewavein(ami_long p);
ami_long ami_lenwavein(ami_long p);
ami_long ami_sgnwavein(ami_long p);
ami_long ami_endwavein(ami_long p);
ami_long ami_fltwavein(ami_long p);
ami_long ami_rdwave(ami_long p, byte* buff, ami_long len);
void ami_synthoutname(ami_long p, string name, ami_long len);
void ami_synthinname(ami_long p, string name, ami_long len);
void ami_waveoutname(ami_long p, string name, ami_long len);
void ami_waveinname(ami_long p, string name, ami_long len);
ami_long ami_setparamsynthin(ami_long p, string name, string value);
ami_long ami_setparamsynthout(ami_long p, string name, string value);
ami_long ami_setparamwavein(ami_long p, string name, string value);
ami_long ami_setparamwaveout(ami_long p, string name, string value);
void ami_getparamsynthin(ami_long p, string name, string value, ami_long len);
void ami_getparamsynthout(ami_long p, string name, string value, ami_long len);
void ami_getparamwavein(ami_long p, string name, string value, ami_long len);
void ami_getparamwaveout(ami_long p, string name, string value, ami_long len);

/* non-standard local access calls */

/* register synth plug ins */
void _pa_synthoutplug(ami_long addend, string name,
                      void (*opnseq)(ami_long p), void (*clsseq)(ami_long p),
                      void (*wrseq)(ami_long p, ami_seqptr sp),
                      ami_long (*setparam)(ami_long p, string name, string value),
                      void (*getparam)(ami_long p, string name, string value, ami_long len)
                     );
void _pa_synthinplug(ami_long addend, string name,
                     void (*opnseq)(ami_long p), void (*clsseq)(ami_long p),
                     void (*rdseq)(ami_long p, ami_seqptr sp),
                     ami_long (*setparam)(ami_long p, string name, string value),
                     void (*getparam)(ami_long p, string name, string value, ami_long len)
                    );
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
                     void (*getparam)(ami_long p, string name, string value, ami_long len)
                    );
void _pa_waveinplug(ami_long addend, string name,
                    void (*open)(ami_long p), void (*close)(ami_long p),
                    ami_long (*chanwavin)(ami_long p), ami_long (*ratewavin)(ami_long p),
                    ami_long (*lenwavin)(ami_long p), ami_long (*sgnwavin)(ami_long p),
                    ami_long (*fltwavin)(ami_long p), ami_long (*endwavein)(ami_long p),
                    ami_long (*rdwav)(ami_long p, byte* buff, ami_long len),
                    ami_long (*setparam)(ami_long p, string name, string value),
                    void (*getparam)(ami_long p, string name, string value, ami_long len)
                   );

/* execute sequencer entry in main code */
void _pa_excseq(ami_long p, ami_seqptr sp);

#ifdef __cplusplus
}
#endif

#endif /* __SOUND_H__ */
