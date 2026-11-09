/** ****************************************************************************
 *
 * Sound library interface C++ wrapper
 *
 * Wraps the calls in sound with C++ conventions, the same way
 * cpp/terminal.cpp wraps the terminal calls. This brings:
 *
 * 1. The functions, types and constants do not need an "ami_" prefix:
 * the sound namespace does the isolation. The constants of sound.h
 * appear in lower case, note_c to inst_gunshot.
 *
 * 2. Every call that takes a port has an overload without it, meaning
 * the default port.
 *
 * 3. The synth and wave objects hold ports. Opening sets the port the
 * object speaks for -- s.opensynthout() or s.opensynthout(2) -- and
 * every call after that leaves the port off: s.noteon(0, 1, note_c+
 * octave_6, v). An object holds one port of each direction, so one
 * synth can serve a keyboard in and a synthesizer out. The destructor
 * closes whatever is still open.
 *
 * Unlike term and graph, sound has no event callbacks to invert, so
 * these objects hook nothing and any number of them may exist.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>

#include <localdefs.h>
#include <sound.h>

}

#include "sound.hpp"

namespace sound {

/* procedures and functions */
void starttimeout(void) { ami_starttimeout(); }
void stoptimeout(void) { ami_stoptimeout(); }
ami_long curtimeout(void) { return ami_curtimeout(); }
void starttimein(void) { ami_starttimein(); }
void stoptimein(void) { ami_stoptimein(); }
ami_long curtimein(void) { return ami_curtimein(); }
ami_long synthout(void) { return ami_synthout(); }
ami_long synthin(void) { return ami_synthin(); }
ami_long waveout(void) { return ami_waveout(); }
ami_long wavein(void) { return ami_wavein(); }
void loadsynth(ami_long s, const char* sf) { ami_loadsynth(s, (char*)sf); }
void delsynth(ami_long s) { ami_delsynth(s); }
ami_long getwaveid(void) { return ami_getwaveid(); }
void loadwave(ami_long w, const char* fn) { ami_loadwave(w, (char*)fn); }
void delwave(ami_long w) { ami_delwave(w); }
void opensynthout(ami_long p) { ami_opensynthout(p); }
void opensynthout(void) { ami_opensynthout(1); }
void closesynthout(ami_long p) { ami_closesynthout(p); }
void closesynthout(void) { ami_closesynthout(1); }
void opensynthin(ami_long p) { ami_opensynthin(p); }
void opensynthin(void) { ami_opensynthin(1); }
void closesynthin(ami_long p) { ami_closesynthin(p); }
void closesynthin(void) { ami_closesynthin(1); }
void noteon(ami_long p, ami_long t, channel c, note n, ami_long v) { ami_noteon(p, t, c, n, v); }
void noteon(ami_long t, channel c, note n, ami_long v) { ami_noteon(1, t, c, n, v); }
void noteoff(ami_long p, ami_long t, channel c, note n, ami_long v) { ami_noteoff(p, t, c, n, v); }
void noteoff(ami_long t, channel c, note n, ami_long v) { ami_noteoff(1, t, c, n, v); }
void instchange(ami_long p, ami_long t, channel c, instrument i) { ami_instchange(p, t, c, i); }
void instchange(ami_long t, channel c, instrument i) { ami_instchange(1, t, c, i); }
void attack(ami_long p, ami_long t, channel c, ami_long at) { ami_attack(p, t, c, at); }
void attack(ami_long t, channel c, ami_long at) { ami_attack(1, t, c, at); }
void release(ami_long p, ami_long t, channel c, ami_long rt) { ami_release(p, t, c, rt); }
void release(ami_long t, channel c, ami_long rt) { ami_release(1, t, c, rt); }
void legato(ami_long p, ami_long t, channel c, ami_long b) { ami_legato(p, t, c, b); }
void legato(ami_long t, channel c, ami_long b) { ami_legato(1, t, c, b); }
void portamento(ami_long p, ami_long t, channel c, ami_long b) { ami_portamento(p, t, c, b); }
void portamento(ami_long t, channel c, ami_long b) { ami_portamento(1, t, c, b); }
void vibrato(ami_long p, ami_long t, channel c, ami_long v) { ami_vibrato(p, t, c, v); }
void vibrato(ami_long t, channel c, ami_long v) { ami_vibrato(1, t, c, v); }
void volsynthchan(ami_long p, ami_long t, channel c, ami_long v) { ami_volsynthchan(p, t, c, v); }
void volsynthchan(ami_long t, channel c, ami_long v) { ami_volsynthchan(1, t, c, v); }
void porttime(ami_long p, ami_long t, channel c, ami_long v) { ami_porttime(p, t, c, v); }
void porttime(ami_long t, channel c, ami_long v) { ami_porttime(1, t, c, v); }
void balance(ami_long p, ami_long t, channel c, ami_long b) { ami_balance(p, t, c, b); }
void balance(ami_long t, channel c, ami_long b) { ami_balance(1, t, c, b); }
void pan(ami_long p, ami_long t, channel c, ami_long b) { ami_pan(p, t, c, b); }
void pan(ami_long t, channel c, ami_long b) { ami_pan(1, t, c, b); }
void timbre(ami_long p, ami_long t, channel c, ami_long tb) { ami_timbre(p, t, c, tb); }
void timbre(ami_long t, channel c, ami_long tb) { ami_timbre(1, t, c, tb); }
void brightness(ami_long p, ami_long t, channel c, ami_long b) { ami_brightness(p, t, c, b); }
void brightness(ami_long t, channel c, ami_long b) { ami_brightness(1, t, c, b); }
void reverb(ami_long p, ami_long t, channel c, ami_long r) { ami_reverb(p, t, c, r); }
void reverb(ami_long t, channel c, ami_long r) { ami_reverb(1, t, c, r); }
void tremulo(ami_long p, ami_long t, channel c, ami_long tr) { ami_tremulo(p, t, c, tr); }
void tremulo(ami_long t, channel c, ami_long tr) { ami_tremulo(1, t, c, tr); }
void chorus(ami_long p, ami_long t, channel c, ami_long cr) { ami_chorus(p, t, c, cr); }
void chorus(ami_long t, channel c, ami_long cr) { ami_chorus(1, t, c, cr); }
void celeste(ami_long p, ami_long t, channel c, ami_long ce) { ami_celeste(p, t, c, ce); }
void celeste(ami_long t, channel c, ami_long ce) { ami_celeste(1, t, c, ce); }
void phaser(ami_long p, ami_long t, channel c, ami_long ph) { ami_phaser(p, t, c, ph); }
void phaser(ami_long t, channel c, ami_long ph) { ami_phaser(1, t, c, ph); }
void aftertouch(ami_long p, ami_long t, channel c, note n, ami_long at) { ami_aftertouch(p, t, c, n, at); }
void aftertouch(ami_long t, channel c, note n, ami_long at) { ami_aftertouch(1, t, c, n, at); }
void pressure(ami_long p, ami_long t, channel c, ami_long pr) { ami_pressure(p, t, c, pr); }
void pressure(ami_long t, channel c, ami_long pr) { ami_pressure(1, t, c, pr); }
void pitch(ami_long p, ami_long t, channel c, ami_long pt) { ami_pitch(p, t, c, pt); }
void pitch(ami_long t, channel c, ami_long pt) { ami_pitch(1, t, c, pt); }
void pitchrange(ami_long p, ami_long t, channel c, ami_long v) { ami_pitchrange(p, t, c, v); }
void pitchrange(ami_long t, channel c, ami_long v) { ami_pitchrange(1, t, c, v); }
void mono(ami_long p, ami_long t, channel c, ami_long ch) { ami_mono(p, t, c, ch); }
void mono(ami_long t, channel c, ami_long ch) { ami_mono(1, t, c, ch); }
void poly(ami_long p, ami_long t, channel c) { ami_poly(p, t, c); }
void poly(ami_long t, channel c) { ami_poly(1, t, c); }
void playsynth(ami_long p, ami_long t, ami_long s) { ami_playsynth(p, t, s); }
void playsynth(ami_long t, ami_long s) { ami_playsynth(1, t, s); }
void waitsynth(ami_long p) { ami_waitsynth(p); }
void waitsynth(void) { ami_waitsynth(1); }
void wrsynth(ami_long p, seqptr sp) { ami_wrsynth(p, (ami_seqptr)sp); }
void wrsynth(seqptr sp) { ami_wrsynth(1, (ami_seqptr)sp); }
void rdsynth(ami_long p, seqptr sp) { ami_rdsynth(p, (ami_seqptr)sp); }
void rdsynth(seqptr sp) { ami_rdsynth(1, (ami_seqptr)sp); }
void synthoutname(ami_long p, char* name, ami_long len) { ami_synthoutname(p, name, len); }
void synthoutname(char* name, ami_long len) { ami_synthoutname(1, name, len); }
void synthinname(ami_long p, char* name, ami_long len) { ami_synthinname(p, name, len); }
void synthinname(char* name, ami_long len) { ami_synthinname(1, name, len); }
ami_long setparamsynthout(ami_long p, const char* name, const char* value) { return ami_setparamsynthout(p, (char*)name, (char*)value); }
ami_long setparamsynthout(const char* name, const char* value) { return ami_setparamsynthout(1, (char*)name, (char*)value); }
ami_long setparamsynthin(ami_long p, const char* name, const char* value) { return ami_setparamsynthin(p, (char*)name, (char*)value); }
ami_long setparamsynthin(const char* name, const char* value) { return ami_setparamsynthin(1, (char*)name, (char*)value); }
void getparamsynthout(ami_long p, const char* name, char* value, ami_long len) { ami_getparamsynthout(p, (char*)name, value, len); }
void getparamsynthout(const char* name, char* value, ami_long len) { ami_getparamsynthout(1, (char*)name, value, len); }
void getparamsynthin(ami_long p, const char* name, char* value, ami_long len) { ami_getparamsynthin(p, (char*)name, value, len); }
void getparamsynthin(const char* name, char* value, ami_long len) { ami_getparamsynthin(1, (char*)name, value, len); }
void openwaveout(ami_long p) { ami_openwaveout(p); }
void openwaveout(void) { ami_openwaveout(1); }
void closewaveout(ami_long p) { ami_closewaveout(p); }
void closewaveout(void) { ami_closewaveout(1); }
void openwavein(ami_long p) { ami_openwavein(p); }
void openwavein(void) { ami_openwavein(1); }
void closewavein(ami_long p) { ami_closewavein(p); }
void closewavein(void) { ami_closewavein(1); }
void playwave(ami_long p, ami_long t, ami_long w) { ami_playwave(p, t, w); }
void playwave(ami_long t, ami_long w) { ami_playwave(1, t, w); }
void volwave(ami_long p, ami_long t, ami_long v) { ami_volwave(p, t, v); }
void volwave(ami_long t, ami_long v) { ami_volwave(1, t, v); }
void waitwave(ami_long p) { ami_waitwave(p); }
void waitwave(void) { ami_waitwave(1); }
void chanwaveout(ami_long p, ami_long c) { ami_chanwaveout(p, c); }
void chanwaveout(ami_long c) { ami_chanwaveout(1, c); }
void ratewaveout(ami_long p, ami_long r) { ami_ratewaveout(p, r); }
void ratewaveout(ami_long r) { ami_ratewaveout(1, r); }
void lenwaveout(ami_long p, ami_long l) { ami_lenwaveout(p, l); }
void lenwaveout(ami_long l) { ami_lenwaveout(1, l); }
void sgnwaveout(ami_long p, ami_long s) { ami_sgnwaveout(p, s); }
void sgnwaveout(ami_long s) { ami_sgnwaveout(1, s); }
void fltwaveout(ami_long p, ami_long f) { ami_fltwaveout(p, f); }
void fltwaveout(ami_long f) { ami_fltwaveout(1, f); }
void endwaveout(ami_long p, ami_long e) { ami_endwaveout(p, e); }
void endwaveout(ami_long e) { ami_endwaveout(1, e); }
void wrwave(ami_long p, unsigned char* buff, ami_long len) { ami_wrwave(p, (byte*)buff, len); }
void wrwave(unsigned char* buff, ami_long len) { ami_wrwave(1, (byte*)buff, len); }
ami_long chanwavein(ami_long p) { return ami_chanwavein(p); }
ami_long chanwavein(void) { return ami_chanwavein(1); }
ami_long ratewavein(ami_long p) { return ami_ratewavein(p); }
ami_long ratewavein(void) { return ami_ratewavein(1); }
ami_long lenwavein(ami_long p) { return ami_lenwavein(p); }
ami_long lenwavein(void) { return ami_lenwavein(1); }
ami_long sgnwavein(ami_long p) { return ami_sgnwavein(p); }
ami_long sgnwavein(void) { return ami_sgnwavein(1); }
ami_long endwavein(ami_long p) { return ami_endwavein(p); }
ami_long endwavein(void) { return ami_endwavein(1); }
ami_long fltwavein(ami_long p) { return ami_fltwavein(p); }
ami_long fltwavein(void) { return ami_fltwavein(1); }
ami_long rdwave(ami_long p, unsigned char* buff, ami_long len) { return ami_rdwave(p, (byte*)buff, len); }
ami_long rdwave(unsigned char* buff, ami_long len) { return ami_rdwave(1, (byte*)buff, len); }
void waveoutname(ami_long p, char* name, ami_long len) { ami_waveoutname(p, name, len); }
void waveoutname(char* name, ami_long len) { ami_waveoutname(1, name, len); }
void waveinname(ami_long p, char* name, ami_long len) { ami_waveinname(p, name, len); }
void waveinname(char* name, ami_long len) { ami_waveinname(1, name, len); }
ami_long setparamwaveout(ami_long p, const char* name, const char* value) { return ami_setparamwaveout(p, (char*)name, (char*)value); }
ami_long setparamwaveout(const char* name, const char* value) { return ami_setparamwaveout(1, (char*)name, (char*)value); }
ami_long setparamwavein(ami_long p, const char* name, const char* value) { return ami_setparamwavein(p, (char*)name, (char*)value); }
ami_long setparamwavein(const char* name, const char* value) { return ami_setparamwavein(1, (char*)name, (char*)value); }
void getparamwaveout(ami_long p, const char* name, char* value, ami_long len) { ami_getparamwaveout(p, (char*)name, value, len); }
void getparamwaveout(const char* name, char* value, ami_long len) { ami_getparamwaveout(1, (char*)name, value, len); }
void getparamwavein(ami_long p, const char* name, char* value, ami_long len) { ami_getparamwavein(p, (char*)name, value, len); }
void getparamwavein(const char* name, char* value, ami_long len) { ami_getparamwavein(1, (char*)name, value, len); }

/* methods */
synth::synth(void)

{

    outport = 0; /* nothing held yet */
    inport = 0;

}

synth::~synth(void)

{

    /* close whatever the object still holds */
    if (outport) ami_closesynthout(outport);
    if (inport) ami_closesynthin(inport);

}

void synth::opensynthout(ami_long p)

{

    /* an object holds one port a direction: opening over a held port
       lets go of the old one first */
    if (outport) ami_closesynthout(outport);
    ami_opensynthout(p);
    outport = p;

}

void synth::closesynthout(void)

{

    if (outport) { ami_closesynthout(outport); outport = 0; }

}

void synth::opensynthin(ami_long p)

{

    if (inport) ami_closesynthin(inport);
    ami_opensynthin(p);
    inport = p;

}

void synth::closesynthin(void)

{

    if (inport) { ami_closesynthin(inport); inport = 0; }

}

void synth::noteon(ami_long t, channel c, note n, ami_long v) { ami_noteon(outport, t, c, n, v); }
void synth::noteoff(ami_long t, channel c, note n, ami_long v) { ami_noteoff(outport, t, c, n, v); }
void synth::instchange(ami_long t, channel c, instrument i) { ami_instchange(outport, t, c, i); }
void synth::attack(ami_long t, channel c, ami_long at) { ami_attack(outport, t, c, at); }
void synth::release(ami_long t, channel c, ami_long rt) { ami_release(outport, t, c, rt); }
void synth::legato(ami_long t, channel c, ami_long b) { ami_legato(outport, t, c, b); }
void synth::portamento(ami_long t, channel c, ami_long b) { ami_portamento(outport, t, c, b); }
void synth::vibrato(ami_long t, channel c, ami_long v) { ami_vibrato(outport, t, c, v); }
void synth::volsynthchan(ami_long t, channel c, ami_long v) { ami_volsynthchan(outport, t, c, v); }
void synth::porttime(ami_long t, channel c, ami_long v) { ami_porttime(outport, t, c, v); }
void synth::balance(ami_long t, channel c, ami_long b) { ami_balance(outport, t, c, b); }
void synth::pan(ami_long t, channel c, ami_long b) { ami_pan(outport, t, c, b); }
void synth::timbre(ami_long t, channel c, ami_long tb) { ami_timbre(outport, t, c, tb); }
void synth::brightness(ami_long t, channel c, ami_long b) { ami_brightness(outport, t, c, b); }
void synth::reverb(ami_long t, channel c, ami_long r) { ami_reverb(outport, t, c, r); }
void synth::tremulo(ami_long t, channel c, ami_long tr) { ami_tremulo(outport, t, c, tr); }
void synth::chorus(ami_long t, channel c, ami_long cr) { ami_chorus(outport, t, c, cr); }
void synth::celeste(ami_long t, channel c, ami_long ce) { ami_celeste(outport, t, c, ce); }
void synth::phaser(ami_long t, channel c, ami_long ph) { ami_phaser(outport, t, c, ph); }
void synth::aftertouch(ami_long t, channel c, note n, ami_long at) { ami_aftertouch(outport, t, c, n, at); }
void synth::pressure(ami_long t, channel c, ami_long pr) { ami_pressure(outport, t, c, pr); }
void synth::pitch(ami_long t, channel c, ami_long pt) { ami_pitch(outport, t, c, pt); }
void synth::pitchrange(ami_long t, channel c, ami_long v) { ami_pitchrange(outport, t, c, v); }
void synth::mono(ami_long t, channel c, ami_long ch) { ami_mono(outport, t, c, ch); }
void synth::poly(ami_long t, channel c) { ami_poly(outport, t, c); }
void synth::playsynth(ami_long t, ami_long s) { ami_playsynth(outport, t, s); }
void synth::waitsynth(void) { ami_waitsynth(outport); }
void synth::wrsynth(seqptr sp) { ami_wrsynth(outport, (ami_seqptr)sp); }
void synth::rdsynth(seqptr sp) { ami_rdsynth(inport, (ami_seqptr)sp); }
void synth::synthoutname(char* name, ami_long len) { ami_synthoutname(outport, name, len); }
void synth::synthinname(char* name, ami_long len) { ami_synthinname(inport, name, len); }
ami_long synth::setparamsynthout(const char* name, const char* value) { return ami_setparamsynthout(outport, (char*)name, (char*)value); }
ami_long synth::setparamsynthin(const char* name, const char* value) { return ami_setparamsynthin(inport, (char*)name, (char*)value); }
void synth::getparamsynthout(const char* name, char* value, ami_long len) { ami_getparamsynthout(outport, (char*)name, value, len); }
void synth::getparamsynthin(const char* name, char* value, ami_long len) { ami_getparamsynthin(inport, (char*)name, value, len); }

wave::wave(void)

{

    outport = 0; /* nothing held yet */
    inport = 0;

}

wave::~wave(void)

{

    /* close whatever the object still holds */
    if (outport) ami_closewaveout(outport);
    if (inport) ami_closewavein(inport);

}

void wave::openwaveout(ami_long p)

{

    /* an object holds one port a direction: opening over a held port
       lets go of the old one first */
    if (outport) ami_closewaveout(outport);
    ami_openwaveout(p);
    outport = p;

}

void wave::closewaveout(void)

{

    if (outport) { ami_closewaveout(outport); outport = 0; }

}

void wave::openwavein(ami_long p)

{

    if (inport) ami_closewavein(inport);
    ami_openwavein(p);
    inport = p;

}

void wave::closewavein(void)

{

    if (inport) { ami_closewavein(inport); inport = 0; }

}

void wave::playwave(ami_long t, ami_long w) { ami_playwave(outport, t, w); }
void wave::volwave(ami_long t, ami_long v) { ami_volwave(outport, t, v); }
void wave::waitwave(void) { ami_waitwave(outport); }
void wave::chanwaveout(ami_long c) { ami_chanwaveout(outport, c); }
void wave::ratewaveout(ami_long r) { ami_ratewaveout(outport, r); }
void wave::lenwaveout(ami_long l) { ami_lenwaveout(outport, l); }
void wave::sgnwaveout(ami_long s) { ami_sgnwaveout(outport, s); }
void wave::fltwaveout(ami_long f) { ami_fltwaveout(outport, f); }
void wave::endwaveout(ami_long e) { ami_endwaveout(outport, e); }
void wave::wrwave(unsigned char* buff, ami_long len) { ami_wrwave(outport, (byte*)buff, len); }
ami_long wave::chanwavein(void) { return ami_chanwavein(inport); }
ami_long wave::ratewavein(void) { return ami_ratewavein(inport); }
ami_long wave::lenwavein(void) { return ami_lenwavein(inport); }
ami_long wave::sgnwavein(void) { return ami_sgnwavein(inport); }
ami_long wave::endwavein(void) { return ami_endwavein(inport); }
ami_long wave::fltwavein(void) { return ami_fltwavein(inport); }
ami_long wave::rdwave(unsigned char* buff, ami_long len) { return ami_rdwave(inport, (byte*)buff, len); }
void wave::waveoutname(char* name, ami_long len) { ami_waveoutname(outport, name, len); }
void wave::waveinname(char* name, ami_long len) { ami_waveinname(inport, name, len); }
ami_long wave::setparamwaveout(const char* name, const char* value) { return ami_setparamwaveout(outport, (char*)name, (char*)value); }
ami_long wave::setparamwavein(const char* name, const char* value) { return ami_setparamwavein(inport, (char*)name, (char*)value); }
void wave::getparamwaveout(const char* name, char* value, ami_long len) { ami_getparamwaveout(outport, (char*)name, value, len); }
void wave::getparamwavein(const char* name, char* value, ami_long len) { ami_getparamwavein(inport, (char*)name, value, len); }

} /* namespace sound */
