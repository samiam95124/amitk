/*******************************************************************************

Dump sequencer plug-in for Petit_Ami sound module

Dumps the input sequencer records for MIDI and reroutes them to the next device
in line (numeric order). This can be useful to see the MIDI stream, or simply
to test MIDI in plugins.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <localdefs.h>
#include <sound.h>

#define MAXINST 100 /* maximum allowed device instance (unused) */

int sport; /* input port */

/*******************************************************************************

Flag dumpmidi error

Prints an error and stops.

*******************************************************************************/

static void error(string es)

{

    fprintf(stderr, "\nError: Dumpmidi: %s\n", es);

    exit(1);

}

/*******************************************************************************

Open Liquidsynth MIDI device

Opens a Liquidsynth MIDI port for use. Does nothing at present, since we open
one MIDI out device at init time.

*******************************************************************************/

static void opendump(int p)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_opensynthin(sport); /* open the monitored device */

}

/*******************************************************************************

Close Liquidsynth MIDI device

Closes a Liquidsynth MIDI output device for use.

*******************************************************************************/

static void closedump(int p)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_closesynthin(sport); /* close the monitored device */

}

/*******************************************************************************

Read and dump MIDI message

Reads a sequencer message. The sequencer message is read from the next input
device in the table, then that is dumped and returned to the caller.

*******************************************************************************/

static void readdump(int p, ami_seqptr sp)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_rdsynth(sport, sp); /* get seq record */
    /* now just dump the message */
    switch (sp->st) { /* sequencer message type */

        case st_noteon:       printf("noteon: Time: %d Port: %d Channel: %d "
                                     "Note: %d Velocity: %d\n",
                                     sp->time, sp->port, sp->ntc, sp->ntn, sp->ntv);
                              break;
        case st_noteoff:      printf("noteoff: Time: %d Port: %d Channel: %d "
                                     "Note: %d Velocity: %d\n", sp->time,
                                     sp->port, sp->ntc, sp->ntn, sp->ntv);
                              break;
        case st_instchange:   printf("instchange: Time: %d Port: %d sp->port "
                                     "Channel: %d Instrument: %d\n", sp->time,
                                     sp->port, sp->icc, sp->ici);
                              break;
        case st_attack:       printf("attack: Time: %d Port: %d Channel: %d "
                                     "attack time: %d\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_release:      printf("release: Time: %d Port: %d Channel: %d "
                                     "release time: %d\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_legato:       printf("legato: Time: %d Port: %d Channel: %d "
                                     "legato on/off: %d\n", sp->time, sp->port,
                                     sp->bsc, sp->bsb);
                              break;
        case st_portamento:   printf("portamento: Time: %d Port: %d Channel: %d "
                                     "portamento on/off: %d\n", sp->time, sp->port,
                                     sp->bsc, sp->bsb);
                              break;
        case st_vibrato:      printf("vibrato: Time: %d Port: %d Channel: %d "
                                     "Vibrato: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_volsynthchan: printf("volsynthchan: Time: %d Port: %d Channel: %d "
                                     "Volume: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_porttime:     printf("porttime: Time: %d Port: %d Channel: %d "
                                     "Portamento time: %d\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_balance:      printf("attack: Time: %d Port: %d Channel: %d "
                                     "Ballance: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_pan:          printf("pan: Time: %d Port: %d Channel: %d "
                                     "Pan: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_timbre:       printf("timbre: Time: %d Port: %d Channel: %d "
                                     "Timbre: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_brightness:   printf("brightness: Time: %d Port: %d Channel: %d "
                                     "Brightness: %d\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_reverb:       printf("reverb: Time: %d Port: %d Channel: %d "
                                     "Reverb: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_tremulo:      printf("tremulo: Time: %d Port: %d Channel: %d "
                                     "Tremulo: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_chorus:       printf("chorus: Time: %d Port: %d Channel: %d "
                                     "Chorus: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_celeste:      printf("celeste: Time: %d Port: %d Channel: %d "
                                     "Celeste: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_phaser:       printf("Phaser: Time: %d Port: %d Channel: %d "
                                     "Phaser: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_aftertouch:   printf("aftertouch: Time: %d Port: %d Channel: %d "
                                     "Note: %d Aftertouch: %d\n", sp->time,
                                     sp->port, sp->ntc, sp->ntn, sp->ntv);
                              break;
        case st_pressure:     printf("pressure: Time: %d Port: %d Channel: %d "
                                     "Pressure: %d\n", sp->time, sp->port, sp->ntc,
                                     sp->ntv);
                              break;
        case st_pitch:        printf("pitch: Time: %d Port: %d Channel: %d "
                                     "Pitch: %d\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_pitchrange:   printf("pitchrange: Time: %d Port: %d Channel: %d "
                                     "Pitch range: %d\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_mono:         printf("mono: Time: %d Port: %d Channel: %d "
                                     "Mono notes: %d\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_poly:         printf("poly: Time: %d Port: %d Channel: %d\n",
                                     sp->time, sp->port, sp->pc);
                              break;
        case st_playsynth:    printf("playsynth: Time: %d Port: %d "
                                     ".mid file id: %d\n", sp->time, sp->port,
                                     sp->sid);
                              break;
        case st_playwave:     printf("playwave: Time: %d Port: %d "
                                     ".wav file logical number: %d\n", sp->time,
                                     sp->port, sp->wt);
                              break;
        case st_volwave:      printf("volwave: Time: %d Port: %d Volume: %d\n",
                                     sp->time, sp->port, sp->wv);
                              break;

    }

}

/*******************************************************************************

Set parameter

Set plug in parameter from the given name and value. Not implemented at present.
Always returns error.

*******************************************************************************/

int setparamdump(int p, string name, string value)

{

    int r;
    string ep;

    r = 1; /* set error by default */
    if (!strcmp(name, "connect")) {

        /* set connection for output */
        sport = strtol(value, &ep, 0);
        r = !*ep; /* set good if entire string read */

    }
    return (r); /* exit with error */

}

/*******************************************************************************

Get parameter

Get plug in parameter from the given name and value. Not implemented at present.
Always returns empty string.

*******************************************************************************/

void getparamdump(int p, string name, string value, int len)

{

    *value = 0; /* clear output string */

}

/*******************************************************************************

Initialize dumpmidi plug-in.

Registers dumpmidi as a plug-in device with PA sound module.

*******************************************************************************/

static void dumpmidi_plug_init (void) __attribute__((constructor (103)));
static void dumpmidi_plug_init()

{

    /* now install us as PA device at end */
    _pa_synthinplug(TRUE, "Dump MIDI", opendump, closedump, readdump,
                    setparamdump, getparamdump);
    sport = 0; /* set input port invalid until set */

}

/*******************************************************************************

Deinitialize dumpmidi plug-in.

Clean up dumpmidi instance.

*******************************************************************************/

static void dumpmidi_plug_deinit (void) __attribute__((destructor (103)));
static void dumpmidi_plug_deinit()

{

}
