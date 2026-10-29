/** ****************************************************************************
 *
 * Sound library interface C++ wrapper declarations
 *
 * See cpp/sound.cpp for the rationale. The types and constants of the C
 * header appear here with the ami_/AMI_ prefixes stripped, the constants
 * in lower case as C++ constants rather than macros. The layout of
 * seqmsg is identical to ami_seqmsg in sound.h, and must stay so: the
 * wrapper casts between them.
 *
 ******************************************************************************/

#ifndef __SOUND_HPP__
#define __SOUND_HPP__

extern "C" {

#include <localdefs.h>

}

namespace sound {

/* MIDI note, channel and instrument */
typedef ami_long note;
typedef ami_long channel;
typedef ami_long instrument;

/* sequencer message types */
typedef enum {

    st_noteon, st_noteoff, st_instchange, st_attack, st_release,
    st_legato, st_portamento, st_vibrato, st_volsynthchan,
    st_porttime, st_balance, st_pan, st_timbre, st_brightness,
    st_reverb, st_tremulo, st_chorus, st_celeste, st_phaser,
    st_aftertouch, st_pressure, st_pitch, st_pitchrange, st_mono,
    st_poly, st_playsynth, st_playwave, st_volwave

} seqtyp;

/* sequencer message */
typedef struct seqmsg {

    struct seqmsg* next; /* next message in list */
    ami_long       port; /* port to which message applies */
    ami_long       time; /* time to execute message */
    seqtyp         st;   /* type of message */
    union {

        /* st_noteon st_noteoff st_aftertouch st_pressure */
        struct { channel ntc; note ntn; ami_long ntv; };
        /* st_instchange */ struct { channel icc; instrument ici; };
        /* st_attack, st_release, st_vibrato, st_volsynthchan,
           st_porttime, st_balance, st_pan, st_timbre, st_brightness,
           st_reverb, st_tremulo, st_chorus, st_celeste, st_phaser,
           st_pitch, st_pitchrange, st_mono */
        struct { channel vsc; ami_long vsv; };
        /* st_poly */ channel pc;
        /* st_legato, st_portamento */ struct { channel bsc; ami_long bsb; };
        /* st_playsynth */ ami_long sid;
        /* st_playwave */ ami_long wt;
        /* st_volwave */ ami_long wv;

    };

} seqmsg;

/* pointer to message */
typedef seqmsg* seqptr;

/* the constants of sound.h, prefixes stripped */
const ami_long chan_drum                    =  10;
const ami_long synth_out                    =   1;
const ami_long synth_in                     =   1;
const ami_long wave_in                      =   1;
const ami_long wave_out                     =   1;
const ami_long note_c                       =   1;
const ami_long note_c_sharp                 =   2;
const ami_long note_d_flat                  =   2;
const ami_long note_d                       =   3;
const ami_long note_d_sharp                 =   4;
const ami_long note_e_flat                  =   4;
const ami_long note_e                       =   5;
const ami_long note_f                       =   6;
const ami_long note_f_sharp                 =   7;
const ami_long note_g_flat                  =   7;
const ami_long note_g                       =   8;
const ami_long note_g_sharp                 =   9;
const ami_long note_a_flat                  =   9;
const ami_long note_a                       =  10;
const ami_long note_a_sharp                 =  11;
const ami_long note_b_flat                  =  11;
const ami_long note_b                       =  12;
const ami_long octave_1                     =   0;
const ami_long octave_2                     =  12;
const ami_long octave_3                     =  24;
const ami_long octave_4                     =  36;
const ami_long octave_5                     =  48;
const ami_long octave_6                     =  60;
const ami_long octave_7                     =  72;
const ami_long octave_8                     =  84;
const ami_long octave_9                     =  96;
const ami_long octave_10                    = 108;
const ami_long octave_11                    = 120;
const ami_long inst_acoustic_grand          =   1;
const ami_long inst_bright_acoustic         =   2;
const ami_long inst_electric_grand          =   3;
const ami_long inst_honky_tonk              =   4;
const ami_long inst_electric_piano_1        =   5;
const ami_long inst_electric_piano_2        =   6;
const ami_long inst_harpsichord             =   7;
const ami_long inst_clavinet                =   8;
const ami_long inst_celesta                 =   9;
const ami_long inst_glockenspiel            =  10;
const ami_long inst_music_box               =  11;
const ami_long inst_vibraphone              =  12;
const ami_long inst_marimba                 =  13;
const ami_long inst_xylophone               =  14;
const ami_long inst_tubular_bells           =  15;
const ami_long inst_dulcimer                =  16;
const ami_long inst_drawbar_organ           =  17;
const ami_long inst_percussive_organ        =  18;
const ami_long inst_rock_organ              =  19;
const ami_long inst_church_organ            =  20;
const ami_long inst_reed_organ              =  21;
const ami_long inst_accoridan               =  22;
const ami_long inst_harmonica               =  23;
const ami_long inst_tango_accordian         =  24;
const ami_long inst_nylon_string_guitar     =  25;
const ami_long inst_steel_string_guitar     =  26;
const ami_long inst_electric_jazz_guitar    =  27;
const ami_long inst_electric_clean_guitar   =  28;
const ami_long inst_electric_muted_guitar   =  29;
const ami_long inst_overdriven_guitar       =  30;
const ami_long inst_distortion_guitar       =  31;
const ami_long inst_guitar_harmonics        =  32;
const ami_long inst_acoustic_bass           =  33;
const ami_long inst_electric_bass_finger    =  34;
const ami_long inst_electric_bass_pick      =  35;
const ami_long inst_fretless_bass           =  36;
const ami_long inst_slap_bass_1             =  37;
const ami_long inst_slap_bass_2             =  38;
const ami_long inst_synth_bass_1            =  39;
const ami_long inst_synth_bass_2            =  40;
const ami_long inst_violin                  =  41;
const ami_long inst_viola                   =  42;
const ami_long inst_cello                   =  43;
const ami_long inst_contrabass              =  44;
const ami_long inst_tremolo_strings         =  45;
const ami_long inst_pizzicato_strings       =  46;
const ami_long inst_orchestral_strings      =  47;
const ami_long inst_timpani                 =  48;
const ami_long inst_string_ensemble_1       =  49;
const ami_long inst_string_ensemble_2       =  50;
const ami_long inst_synthstrings_1          =  51;
const ami_long inst_synthstrings_2          =  52;
const ami_long inst_choir_aahs              =  53;
const ami_long inst_voice_oohs              =  54;
const ami_long inst_synth_voice             =  55;
const ami_long inst_orchestra_hit           =  56;
const ami_long inst_trumpet                 =  57;
const ami_long inst_trombone                =  58;
const ami_long inst_tuba                    =  59;
const ami_long inst_muted_trumpet           =  60;
const ami_long inst_french_horn             =  61;
const ami_long inst_brass_section           =  62;
const ami_long inst_synthbrass_1            =  63;
const ami_long inst_synthbrass_2            =  64;
const ami_long inst_soprano_sax             =  65;
const ami_long inst_alto_sax                =  66;
const ami_long inst_tenor_sax               =  67;
const ami_long inst_baritone_sax            =  68;
const ami_long inst_oboe                    =  69;
const ami_long inst_english_horn            =  70;
const ami_long inst_bassoon                 =  71;
const ami_long inst_clarinet                =  72;
const ami_long inst_piccolo                 =  73;
const ami_long inst_flute                   =  74;
const ami_long inst_recorder                =  75;
const ami_long inst_pan_flute               =  76;
const ami_long inst_blown_bottle            =  77;
const ami_long inst_skakuhachi              =  78;
const ami_long inst_whistle                 =  79;
const ami_long inst_ocarina                 =  80;
const ami_long inst_lead_1_square           =  81;
const ami_long inst_lead_2_sawtooth         =  82;
const ami_long inst_lead_3_calliope         =  83;
const ami_long inst_lead_4_chiff            =  84;
const ami_long inst_lead_5_charang          =  85;
const ami_long inst_lead_6_voice            =  86;
const ami_long inst_lead_7_fifths           =  87;
const ami_long inst_lead_8_bass_lead        =  88;
const ami_long inst_pad_1_new_age           =  89;
const ami_long inst_pad_2_warm              =  90;
const ami_long inst_pad_3_polysynth         =  91;
const ami_long inst_pad_4_choir             =  92;
const ami_long inst_pad_5_bowed             =  93;
const ami_long inst_pad_6_metallic          =  94;
const ami_long inst_pad_7_halo              =  95;
const ami_long inst_pad_8_sweep             =  96;
const ami_long inst_fx_1_rain               =  97;
const ami_long inst_fx_2_soundtrack         =  98;
const ami_long inst_fx_3_crystal            =  99;
const ami_long inst_fx_4_atmosphere         = 100;
const ami_long inst_fx_5_brightness         = 101;
const ami_long inst_fx_6_goblins            = 102;
const ami_long inst_fx_7_echoes             = 103;
const ami_long inst_fx_8_sci_fi             = 104;
const ami_long inst_sitar                   = 105;
const ami_long inst_banjo                   = 106;
const ami_long inst_shamisen                = 107;
const ami_long inst_koto                    = 108;
const ami_long inst_kalimba                 = 109;
const ami_long inst_bagpipe                 = 110;
const ami_long inst_fiddle                  = 111;
const ami_long inst_shanai                  = 112;
const ami_long inst_tinkle_bell             = 113;
const ami_long inst_agogo                   = 114;
const ami_long inst_steel_drums             = 115;
const ami_long inst_woodblock               = 116;
const ami_long inst_taiko_drum              = 117;
const ami_long inst_melodic_tom             = 118;
const ami_long inst_synth_drum              = 119;
const ami_long inst_reverse_cymbal          = 120;
const ami_long inst_guitar_fret_noise       = 121;
const ami_long inst_breath_noise            = 122;
const ami_long inst_seashore                = 123;
const ami_long inst_bird_tweet              = 124;
const ami_long inst_telephone_ring          = 125;
const ami_long inst_helicopter              = 126;
const ami_long inst_applause                = 127;
const ami_long inst_gunshot                 = 128;
const ami_long note_acoustic_bass_drum      =  35;
const ami_long note_bass_drum_1             =  36;
const ami_long note_side_stick              =  37;
const ami_long note_acoustic_snare          =  38;
const ami_long note_hand_clap               =  39;
const ami_long note_electric_snare          =  40;
const ami_long note_low_floor_tom           =  41;
const ami_long note_closed_hi_hat           =  42;
const ami_long note_high_floor_tom          =  43;
const ami_long note_pedal_hi_hat            =  44;
const ami_long note_low_tom                 =  45;
const ami_long note_open_hi_hat             =  46;
const ami_long note_low_mid_tom             =  47;
const ami_long note_hi_mid_tom              =  48;
const ami_long note_crash_cymbal_1          =  49;
const ami_long note_high_tom                =  50;
const ami_long note_ride_cymbal_1           =  51;
const ami_long note_chinese_cymbal          =  52;
const ami_long note_ride_bell               =  53;
const ami_long note_tambourine              =  54;
const ami_long note_splash_cymbal           =  55;
const ami_long note_cowbell                 =  56;
const ami_long note_crash_cymbal_2          =  57;
const ami_long note_vibraslap               =  58;
const ami_long note_ride_cymbal_2           =  59;
const ami_long note_hi_bongo                =  60;
const ami_long note_low_bongo               =  61;
const ami_long note_mute_hi_conga           =  62;
const ami_long note_open_hi_conga           =  63;
const ami_long note_low_conga               =  64;
const ami_long note_high_timbale            =  65;
const ami_long note_low_timbale             =  66;
const ami_long note_high_agogo              =  67;
const ami_long note_low_agogo               =  68;
const ami_long note_cabasa                  =  69;
const ami_long note_maracas                 =  70;
const ami_long note_short_whistle           =  71;
const ami_long note_long_whistle            =  72;
const ami_long note_short_guiro             =  73;
const ami_long note_long_guiro              =  74;
const ami_long note_claves                  =  75;
const ami_long note_hi_wood_block           =  76;
const ami_long note_low_wood_block          =  77;
const ami_long note_mute_cuica              =  78;
const ami_long note_open_cuica              =  79;
const ami_long note_mute_triangle           =  80;
const ami_long note_open_triangle           =  81;

/* procedures and functions */
void starttimeout(void);
void stoptimeout(void);
ami_long curtimeout(void);
void starttimein(void);
void stoptimein(void);
ami_long curtimein(void);
ami_long synthout(void);
ami_long synthin(void);
ami_long waveout(void);
ami_long wavein(void);
void loadsynth(ami_long s, const char* sf);
void delsynth(ami_long s);
void loadwave(ami_long w, const char* fn);
void delwave(ami_long w);
void opensynthout(ami_long p);
void opensynthout(void);
void closesynthout(ami_long p);
void closesynthout(void);
void opensynthin(ami_long p);
void opensynthin(void);
void closesynthin(ami_long p);
void closesynthin(void);
void noteon(ami_long p, ami_long t, channel c, note n, ami_long v);
void noteon(ami_long t, channel c, note n, ami_long v);
void noteoff(ami_long p, ami_long t, channel c, note n, ami_long v);
void noteoff(ami_long t, channel c, note n, ami_long v);
void instchange(ami_long p, ami_long t, channel c, instrument i);
void instchange(ami_long t, channel c, instrument i);
void attack(ami_long p, ami_long t, channel c, ami_long at);
void attack(ami_long t, channel c, ami_long at);
void release(ami_long p, ami_long t, channel c, ami_long rt);
void release(ami_long t, channel c, ami_long rt);
void legato(ami_long p, ami_long t, channel c, ami_long b);
void legato(ami_long t, channel c, ami_long b);
void portamento(ami_long p, ami_long t, channel c, ami_long b);
void portamento(ami_long t, channel c, ami_long b);
void vibrato(ami_long p, ami_long t, channel c, ami_long v);
void vibrato(ami_long t, channel c, ami_long v);
void volsynthchan(ami_long p, ami_long t, channel c, ami_long v);
void volsynthchan(ami_long t, channel c, ami_long v);
void porttime(ami_long p, ami_long t, channel c, ami_long v);
void porttime(ami_long t, channel c, ami_long v);
void balance(ami_long p, ami_long t, channel c, ami_long b);
void balance(ami_long t, channel c, ami_long b);
void pan(ami_long p, ami_long t, channel c, ami_long b);
void pan(ami_long t, channel c, ami_long b);
void timbre(ami_long p, ami_long t, channel c, ami_long tb);
void timbre(ami_long t, channel c, ami_long tb);
void brightness(ami_long p, ami_long t, channel c, ami_long b);
void brightness(ami_long t, channel c, ami_long b);
void reverb(ami_long p, ami_long t, channel c, ami_long r);
void reverb(ami_long t, channel c, ami_long r);
void tremulo(ami_long p, ami_long t, channel c, ami_long tr);
void tremulo(ami_long t, channel c, ami_long tr);
void chorus(ami_long p, ami_long t, channel c, ami_long cr);
void chorus(ami_long t, channel c, ami_long cr);
void celeste(ami_long p, ami_long t, channel c, ami_long ce);
void celeste(ami_long t, channel c, ami_long ce);
void phaser(ami_long p, ami_long t, channel c, ami_long ph);
void phaser(ami_long t, channel c, ami_long ph);
void aftertouch(ami_long p, ami_long t, channel c, note n, ami_long at);
void aftertouch(ami_long t, channel c, note n, ami_long at);
void pressure(ami_long p, ami_long t, channel c, ami_long pr);
void pressure(ami_long t, channel c, ami_long pr);
void pitch(ami_long p, ami_long t, channel c, ami_long pt);
void pitch(ami_long t, channel c, ami_long pt);
void pitchrange(ami_long p, ami_long t, channel c, ami_long v);
void pitchrange(ami_long t, channel c, ami_long v);
void mono(ami_long p, ami_long t, channel c, ami_long ch);
void mono(ami_long t, channel c, ami_long ch);
void poly(ami_long p, ami_long t, channel c);
void poly(ami_long t, channel c);
void playsynth(ami_long p, ami_long t, ami_long s);
void playsynth(ami_long t, ami_long s);
void waitsynth(ami_long p);
void waitsynth(void);
void wrsynth(ami_long p, seqptr sp);
void wrsynth(seqptr sp);
void rdsynth(ami_long p, seqptr sp);
void rdsynth(seqptr sp);
void synthoutname(ami_long p, char* name, ami_long len);
void synthoutname(char* name, ami_long len);
void synthinname(ami_long p, char* name, ami_long len);
void synthinname(char* name, ami_long len);
ami_long setparamsynthout(ami_long p, const char* name, const char* value);
ami_long setparamsynthout(const char* name, const char* value);
ami_long setparamsynthin(ami_long p, const char* name, const char* value);
ami_long setparamsynthin(const char* name, const char* value);
void getparamsynthout(ami_long p, const char* name, char* value, ami_long len);
void getparamsynthout(const char* name, char* value, ami_long len);
void getparamsynthin(ami_long p, const char* name, char* value, ami_long len);
void getparamsynthin(const char* name, char* value, ami_long len);
void openwaveout(ami_long p);
void openwaveout(void);
void closewaveout(ami_long p);
void closewaveout(void);
void openwavein(ami_long p);
void openwavein(void);
void closewavein(ami_long p);
void closewavein(void);
void playwave(ami_long p, ami_long t, ami_long w);
void playwave(ami_long t, ami_long w);
void volwave(ami_long p, ami_long t, ami_long v);
void volwave(ami_long t, ami_long v);
void waitwave(ami_long p);
void waitwave(void);
void chanwaveout(ami_long p, ami_long c);
void chanwaveout(ami_long c);
void ratewaveout(ami_long p, ami_long r);
void ratewaveout(ami_long r);
void lenwaveout(ami_long p, ami_long l);
void lenwaveout(ami_long l);
void sgnwaveout(ami_long p, ami_long s);
void sgnwaveout(ami_long s);
void fltwaveout(ami_long p, ami_long f);
void fltwaveout(ami_long f);
void endwaveout(ami_long p, ami_long e);
void endwaveout(ami_long e);
void wrwave(ami_long p, unsigned char* buff, ami_long len);
void wrwave(unsigned char* buff, ami_long len);
ami_long chanwavein(ami_long p);
ami_long chanwavein(void);
ami_long ratewavein(ami_long p);
ami_long ratewavein(void);
ami_long lenwavein(ami_long p);
ami_long lenwavein(void);
ami_long sgnwavein(ami_long p);
ami_long sgnwavein(void);
ami_long endwavein(ami_long p);
ami_long endwavein(void);
ami_long fltwavein(ami_long p);
ami_long fltwavein(void);
ami_long rdwave(ami_long p, unsigned char* buff, ami_long len);
ami_long rdwave(unsigned char* buff, ami_long len);
void waveoutname(ami_long p, char* name, ami_long len);
void waveoutname(char* name, ami_long len);
void waveinname(ami_long p, char* name, ami_long len);
void waveinname(char* name, ami_long len);
ami_long setparamwaveout(ami_long p, const char* name, const char* value);
ami_long setparamwaveout(const char* name, const char* value);
ami_long setparamwavein(ami_long p, const char* name, const char* value);
ami_long setparamwavein(const char* name, const char* value);
void getparamwaveout(ami_long p, const char* name, char* value, ami_long len);
void getparamwaveout(const char* name, char* value, ami_long len);
void getparamwavein(ami_long p, const char* name, char* value, ami_long len);
void getparamwavein(const char* name, char* value, ami_long len);

/*******************************************************************************

The port objects

A synth holds a synthesizer output port and a synthesizer input port; a
wave holds the same pair of waveform ports. Opening sets the port the
object speaks for, and every call after that leaves the port off. The
destructor closes whatever the object still holds open, so a port
cannot leak by an early return.

Unlike the term and graph objects of the other wrappers, these hook
nothing: any number of them can exist at once, one per port in use.

*******************************************************************************/

class synth {

ami_long outport; /* the output port held, 0 when closed */
ami_long inport;  /* the input port held, 0 when closed */

public:

/* constructor */
synth();

/* destructor */
~synth();

/* copying is refused: two objects would free one port pair */
synth(const synth&) = delete;
synth& operator=(const synth&) = delete;

/* methods */
void opensynthout(ami_long p = synth_out);
void closesynthout(void);
void opensynthin(ami_long p = synth_in);
void closesynthin(void);
void noteon(ami_long t, channel c, note n, ami_long v);
void noteoff(ami_long t, channel c, note n, ami_long v);
void instchange(ami_long t, channel c, instrument i);
void attack(ami_long t, channel c, ami_long at);
void release(ami_long t, channel c, ami_long rt);
void legato(ami_long t, channel c, ami_long b);
void portamento(ami_long t, channel c, ami_long b);
void vibrato(ami_long t, channel c, ami_long v);
void volsynthchan(ami_long t, channel c, ami_long v);
void porttime(ami_long t, channel c, ami_long v);
void balance(ami_long t, channel c, ami_long b);
void pan(ami_long t, channel c, ami_long b);
void timbre(ami_long t, channel c, ami_long tb);
void brightness(ami_long t, channel c, ami_long b);
void reverb(ami_long t, channel c, ami_long r);
void tremulo(ami_long t, channel c, ami_long tr);
void chorus(ami_long t, channel c, ami_long cr);
void celeste(ami_long t, channel c, ami_long ce);
void phaser(ami_long t, channel c, ami_long ph);
void aftertouch(ami_long t, channel c, note n, ami_long at);
void pressure(ami_long t, channel c, ami_long pr);
void pitch(ami_long t, channel c, ami_long pt);
void pitchrange(ami_long t, channel c, ami_long v);
void mono(ami_long t, channel c, ami_long ch);
void poly(ami_long t, channel c);
void playsynth(ami_long t, ami_long s);
void waitsynth(void);
void wrsynth(seqptr sp);
void rdsynth(seqptr sp);
void synthoutname(char* name, ami_long len);
void synthinname(char* name, ami_long len);
ami_long setparamsynthout(const char* name, const char* value);
ami_long setparamsynthin(const char* name, const char* value);
void getparamsynthout(const char* name, char* value, ami_long len);
void getparamsynthin(const char* name, char* value, ami_long len);

}; /* class synth */

class wave {

ami_long outport; /* the output port held, 0 when closed */
ami_long inport;  /* the input port held, 0 when closed */

public:

/* constructor */
wave();

/* destructor */
~wave();

/* copying is refused: two objects would free one port pair */
wave(const wave&) = delete;
wave& operator=(const wave&) = delete;

/* methods */
void openwaveout(ami_long p = wave_out);
void closewaveout(void);
void openwavein(ami_long p = wave_in);
void closewavein(void);
void playwave(ami_long t, ami_long w);
void volwave(ami_long t, ami_long v);
void waitwave(void);
void chanwaveout(ami_long c);
void ratewaveout(ami_long r);
void lenwaveout(ami_long l);
void sgnwaveout(ami_long s);
void fltwaveout(ami_long f);
void endwaveout(ami_long e);
void wrwave(unsigned char* buff, ami_long len);
ami_long chanwavein(void);
ami_long ratewavein(void);
ami_long lenwavein(void);
ami_long sgnwavein(void);
ami_long endwavein(void);
ami_long fltwavein(void);
ami_long rdwave(unsigned char* buff, ami_long len);
void waveoutname(char* name, ami_long len);
void waveinname(char* name, ami_long len);
ami_long setparamwaveout(const char* name, const char* value);
ami_long setparamwavein(const char* name, const char* value);
void getparamwaveout(const char* name, char* value, ami_long len);
void getparamwavein(const char* name, char* value, ami_long len);

}; /* class wave */

} /* namespace sound */

#endif /* __SOUND_HPP__ */
