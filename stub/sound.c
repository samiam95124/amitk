/*******************************************************************************
*                                                                              *
*                                PETIT-AMI                                     *
*                                                                              *
*                               SOUND MODULE                                   *
*                                                                              *
*                              Created 2020                                    *
*                                                                              *
*                               S. A. FRANCO                                   *
*                                                                              *
* This is a stubbed version. It exists so that Petit-Ami can be compiled and   *
* linked without all of the modules being complete.                            *
*                                                                              *
* All of the calls in this library print an error and exit.                    *
*                                                                              *
* One use of the stubs module is to serve as a prototype for a new module      *
* implementation.                                                              *
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

#include <stdlib.h>
#include <stdio.h>
#include <sound.h>

/*******************************************************************************

Process sound library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

static void error(string s)

{

    fprintf(stderr, "\nError: Sound: %s\n", s);

    exit(1);

}

/*******************************************************************************

Find number of output midi ports

Returns the total number of output midi ports.

*******************************************************************************/

int ami_synthout(void)

{

    error("ami_synthout: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find number of input midi ports

Returns the total number of input midi ports.

*******************************************************************************/

int ami_synthin(void)

{

    error("ami_synthin: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Open synthesiser output port

Opens a syth output port. The output ports have their own separate logical
numbers separate from input numbers, and by convention, port 1 will be the
main synthesizer for the computer, and port 2 will be an output port to any
midi chained devices outside the computer.

*******************************************************************************/

void ami_opensynthout(int p)

{

    error("ami_opensynthout: Is not implemented");

}

/*******************************************************************************

Close midi sythesiser output port

Closes a previously opened midi output port.

*******************************************************************************/

void ami_closesynthout(int p)

{

    error("ami_closesynthout: Is not implemented");

}

/*******************************************************************************

Start time output

Starts the sequencer function. The sequencer is cleared, and upcount begins
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

*******************************************************************************/

void ami_starttimeout(void)

{

    error("ami_starttimeout: Is not implemented");

}

/*******************************************************************************

Stop time output

Stops midi sequencer function. Any timers and buffers in use by the sequencer
are cleared, and all pending events dropped.

Note that this does not stop any midi files from being sequenced.

*******************************************************************************/

void ami_stoptimeout(void)

{

    error("ami_stoptimeout: Is not implemented");

}

/*******************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************/

int ami_curtimeout(void)

{

    error("ami_curtimeout: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("ami_starttimein: Is not implemented");

}

/*******************************************************************************

Stop time input

Simply sets that we are not marking input time anymore.

*******************************************************************************/

void ami_stoptimein(void)

{

    error("ami_stoptimein: Is not implemented");

}

/*******************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************/

int ami_curtimein(void)

{

    error("ami_curtimein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Note on

Turns on a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************/

void ami_noteon(int p, int t, ami_channel c, ami_note n, int v)

{

    error("ami_noteon: Is not implemented");

}

/*******************************************************************************

Note off

Turns off a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************/

void ami_noteoff(int p, int t, ami_channel c, ami_note n, int v)

{

    error("ami_noteoff: Is not implemented");

}

/*******************************************************************************

Instrument change

Selects a new instrument for the given channel. Tbe new instrument is specified
by Midi GM encoding, 1 to 128. Takes a time for sequencing.

*******************************************************************************/

void ami_instchange(int p, int t, ami_channel c, ami_instrument i)

{

    error("ami_instchange: Is not implemented");

}

/*******************************************************************************

Set attack time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

*******************************************************************************/

void ami_attack(int p, int t, ami_channel c, int at)

{

    error("ami_attack: Is not implemented");

}

/*******************************************************************************

Set release time

Sets the time of release on a note, ie., how long it takes for the note to go
full off.

*******************************************************************************/

void ami_release(int p, int t, ami_channel c, int rt)

{

    error("ami_release: Is not implemented");

}

/*******************************************************************************

Legato pedal on/off

Sets the legato mode on/off.

*******************************************************************************/

void ami_legato(int p, int t, ami_channel c, int b)

{

    error("ami_legato: Is not implemented");

}

/*******************************************************************************

Portamento pedal on/off

Sets the portamento mode on/off.

*******************************************************************************/

void ami_portamento(int p, int t, ami_channel c, int b)

{

    error("ami_portamento: Is not implemented");

}

/*******************************************************************************

Set volume

Sets synthesizer volume, 0 to maxint.

*******************************************************************************/

void ami_volsynthchan(int p, int t, ami_channel c, int v)

{

    error("ami_volsynthchan: Is not implemented");

}

/*******************************************************************************

Set left right channel balance

Sets sets the left right channel balance. -maxint is all left, 0 is center,
maxint is all right.

*******************************************************************************/

void ami_balance(int p, int t, ami_channel c, int b)

{

    error("ami_balance: Is not implemented");

}

/*******************************************************************************

Set portamento time

Sets portamento time, 0 to maxint.

*******************************************************************************/

void ami_porttime(int p, int t, ami_channel c, int v)

{

    error("ami_porttime: Is not implemented");

}

/*******************************************************************************

Set vibrato

Sets modulaton value, 0 to maxint.

*******************************************************************************/

void ami_vibrato(int p, int t, ami_channel c, int v)

{

    error("ami_vibrato: Is not implemented");

}

/*******************************************************************************

Set left/right pan position

Sets sets the left/right pan position. -maxint is hard left, 0 is center,
maxint is hard right.

*******************************************************************************/

void ami_pan(int p, int t, ami_channel c, int b)

{

    error("ami_pan: Is not implemented");

}

/*******************************************************************************

Set sound timbre

Sets the sound timbre, 0 to maxint.

*******************************************************************************/

void ami_timbre(int p, int t, ami_channel c, int tb)

{

    error("ami_timbre: Is not implemented");

}

/*******************************************************************************

Set sound brightness

Sets the sound brightness, 0 to maxint.

*******************************************************************************/

void ami_brightness(int p, int t, ami_channel c, int b)

{

    error("ami_brightness: Is not implemented");

}

/*******************************************************************************

Set sound reverb

Sets the sound reverb, 0 to maxint.

*******************************************************************************/

void ami_reverb(int p, int t, ami_channel c, int r)

{

    error("ami_reverb: Is not implemented");

}

/*******************************************************************************

Set sound tremulo

Sets the sound tremulo, 0 to maxint.

*******************************************************************************/

void ami_tremulo(int p, int t, ami_channel c, int tr)

{

    error("ami_tremulo: Is not implemented");

}

/*******************************************************************************

Set sound chorus

Sets the sound chorus, 0 to maxint.

*******************************************************************************/

void ami_chorus(int p, int t, ami_channel c, int cr)

{

    error("ami_chorus: Is not implemented");

}

/*******************************************************************************

Set sound celeste

Sets the sound celeste, 0 to maxint.

*******************************************************************************/

void ami_celeste(int p, int t, ami_channel c, int ce)

{

    error("ami_celeste: Is not implemented");

}

/*******************************************************************************

Set sound phaser

Sets the sound phaser, 0 to maxint.

*******************************************************************************/

void ami_phaser(int p, int t, ami_channel c, int ph)

{

    error("ami_phaser: Is not implemented");

}

/*******************************************************************************

Set pitch range

Sets the range of pitch that can be reached by the pitch adjustment. The range
is from 0 to maxint, and represents from from 0 to 127 semitones. This means
that a setting of maxint means that every note in the midi scale can be reached,
with an accuracy of 128 of the space from C to C#. At this setting, any note
could be reached with a slide, for example.

*******************************************************************************/

void ami_pitchrange(int p, int t, ami_channel c, int v)

{

    error("ami_pitchrange: Is not implemented");

}

/*******************************************************************************

Set monophonic mode

If ommni is off, this sets how many channels to respond to. If omni is on,
then only once note at a time will be played. The select is from 0 to 16,
with 0 being "allways select single note mode".

*******************************************************************************/

void ami_mono(int p, int t, ami_channel c, int ch)

{

    error("ami_mono: Is not implemented");

}

/*******************************************************************************

Set polyphonic mode

Reenables polyphonic mode after a monophonic operation.

*******************************************************************************/

void ami_poly(int p, int t, ami_channel c)

{

    error("ami_poly: Is not implemented");

}

/*******************************************************************************

Aftertouch

Controls aftertouch, 0 to maxint, on a note.

*******************************************************************************/

void ami_aftertouch(int p, int t, ami_channel c, ami_note n, int at)

{

    error("ami_aftertouch: Is not implemented");

}

/*******************************************************************************

Channel pressure

Controls channel pressure, 0 to maxint.

*******************************************************************************/

void ami_pressure(int p, int t, ami_channel c, int pr)

{

    error("ami_pressure: Is not implemented");

}

/*******************************************************************************

Set pitch wheel

Sets the pitch wheel value, from -maxint to maxint. This is the amount off the
note in the channel. The GM standard is to adjust for a whole step up and down,
which is 4 half steps total. A "half step" is the difference between, say, C and
C#.

*******************************************************************************/

void ami_pitch(int p, int t, ami_channel c, int pt)

{

    error("ami_pitch: Is not implemented");

}

/*******************************************************************************

Load synthesizer file

Loads a synthesizer control file, usually midi format, into a logical cache,
from 1 to N. These are loaded up into memory for minimum latency. The file is
specified by file name, and the file type is system dependent.

Note that we support 100 synth files loaded, but the Petit-ami "rule of thumb"
is no more than 10 synth files at a time.

*******************************************************************************/

void ami_loadsynth(int s, string fn)

{

    error("ami_loadsynth: Is not implemented");

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

    error("ami_delsynth: Is not implemented");

}

/*******************************************************************************

Play synthesiser file

Plays the waveform file to the indicated midi device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows midi files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.
This version uses the string send MCI command, one of the simpliest ways to do
that. There does not seem to be an obvious way to relate the MCI devices to the
midi devices.

Windows cannot play more than one midi file at a time (although it can layer
one wave with one midi). Also, a midi open/close sequence like we use here will
fail if the default synth is open. We handle this by closing the default if
it is open, then reopening it afterwards.

*******************************************************************************/

void ami_playsynth(int p, int t, int s)

{

    error("ami_playsynth: Is not implemented");

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

    error("ami_waitsynth: Is not implemented");

}

/*******************************************************************************

Find number of output wave devices.

Returns the number of wave output devices available.

*******************************************************************************/

int ami_waveout(void)

{

    error("ami_waveout: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find number of input wave devices.

Returns the number of wave output devices available.

*******************************************************************************/

int ami_wavein(void)

{

    error("ami_wavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Open wave output device

Opens a wave output device by number. By convention, wave out 1 is the default
output device. This is presently a no-op for linux.

*******************************************************************************/

void ami_openwaveout(int p)

{

    error("ami_openwaveout: Is not implemented");

}

/*******************************************************************************

CLose wave output device

Closes a wave output device by number. This is presently a no-op for linux.

*******************************************************************************/

void ami_closewaveout(int p)

{

    error("ami_closewaveout: Is not implemented");

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

    error("ami_loadwave: Is not implemented");

}

/*******************************************************************************

Delete waveform file

Removes a waveform file from the caching table. This frees up the entry to be
redefined.

*******************************************************************************/

void ami_delwave(int w)

{

    error("ami_delwave: Is not implemented");

}

/*******************************************************************************

Play waveform file

Plays the waveform file to the indicated wave device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows wave files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.

*******************************************************************************/

void ami_playwave(int p, int t, int w)

{

    error("ami_playwave: Is not implemented");

}

/*******************************************************************************

Adjust waveform volume

Adjusts the volume on waveform playback. The volume value is from 0 to maxint.

Not implemented at present.

*******************************************************************************/

void ami_volwave(int p, int t, int v)

{

    error("ami_volwave: Is not implemented");

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

    error("ami_waitwave: Is not implemented");

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

    error("ami_chanwaveout: Is not implemented");

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

    error("ami_ratewaveout: Is not implemented");

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

    error("ami_lenwaveout: Is not implemented");

}

/*******************************************************************************

Set sign of wave output device samples

The given port has its signed/no signed status changed. Note that all floating
point formats are inherently signed.

*******************************************************************************/

void ami_sgnwaveout(int p, int s)

{

    error("ami_sgnwaveout: Is not implemented");

}

/*******************************************************************************

Set floating/non-floating point format

Sets the floating point/integer format for output sound samples.

*******************************************************************************/

void ami_fltwaveout(int p, int f)

{

    error("ami_fltwaveout: Is not implemented");

}

/*******************************************************************************

Set big/little endian format

Sets the big or little endian format for an output wave device. It is possible
that an installation is fixed to the endian format of the host machine, in which
case it is an error to set a format that is different.

*******************************************************************************/

void ami_endwaveout(int p, int e)

{

    error("ami_endwaveout: Is not implemented");

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
the data is used without regard for realtime concerns. The nececessary buffering
for real time is implemented by the user of this package, which is generally
recommended to be 1ms or less (64 samples at a 44100 sample rate).

*******************************************************************************/

void ami_wrwave(int p, byte* buff, int len)

{

    error("ami_wrwave: Is not implemented");

}

/*******************************************************************************

Open wave input device

Opens a wave output device by number. By convention, wave in 1 is the default
input device. The wave stream parameters are determined during enumeration,
but we assert them here on open.

*******************************************************************************/

void ami_openwavein(int p)

{

    error("ami_openwavein: Is not implemented");

}

/*******************************************************************************

CLose wave input device

Closes a wave input device by number. This is presently a no-op for linux.

*******************************************************************************/

void ami_closewavein(int p)

{

    error("ami_closewavein: Is not implemented");

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

    error("ami_chanwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get the rate for a wave input device

The given port will have its rate read and returned, which is the
number of samples per second that will be input. It must be a wave output port,
and it must be open. Input samples are timed at the rate.

*******************************************************************************/

int ami_ratewavein(int p)

{

    error("ami_ratewavein: Is not implemented");

    return (1); /* this just shuts up compiler */

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

*******************************************************************************/

int ami_lenwavein(int p)

{

    error("ami_lenwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get signed status of wave input device

Returns true if the given wave input device has signed sampling. Note that
signed sampling is always true if the samples are floating point.

*******************************************************************************/

int ami_sgnwavein(int p)

{

    error("ami_sgnwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get big endian status of wave input device

Returns true if the given wave input device has big endian sampling.

*******************************************************************************/

int ami_endwavein(int p)

{

    error("ami_endwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get floating point status of wave input device

Returns true if the given wave input device has floating point sampling.

*******************************************************************************/

int ami_fltwavein(int p)

{

    error("ami_fltwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("ami_rdwave: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find device name of synthesizer output port

Returns the ALSA device name of the given synthsizer output port.

*******************************************************************************/

void ami_synthoutname(int p, string name, int len)

{

    error("ami_synthoutname: Is not implemented");

}

/*******************************************************************************

Find device name of synthesizer input port

Returns the ALSA device name of the given synthsizer input port.

*******************************************************************************/

void ami_synthinname(int p, string name, int len)

{

    error("ami_synthinname: Is not implemented");

}

/*******************************************************************************

Find device name of wave output port

Returns the ALSA device name of the given wave output port.

*******************************************************************************/

void ami_waveoutname(int p, string name, int len)

{

    error("ami_waveoutname: Is not implemented");

}

/*******************************************************************************

Find device name of wave input port

Returns the ALSA device name of the given wave input port.

*******************************************************************************/

void ami_waveinname(int p, string name, int len)

{

    error("ami_waveinname: Is not implemented");

}

/*******************************************************************************

Open a synthesizer input port.

The given synthesizer port is opened and ready for reading.

*******************************************************************************/

void ami_opensynthin(int p)

{

    error("ami_opensynthin: Is not implemented");

}

/*******************************************************************************

Close a synthesizer input port

Closes the given synthsizer port for reading.

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

    error("ami_wrsynth: Is not implemented");

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

    error("ami_rdsynth: Is not implemented");

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
