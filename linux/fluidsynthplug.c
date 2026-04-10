/*******************************************************************************

Fluidsynth Plug-In for Petit_Ami sound module

Allows Fluidsynth to serve as a plug-in for MIDI command stream devices under
Petit_ami sound system.

*******************************************************************************/

#include <sys/types.h>
#include <unistd.h>

#include <fluidsynth.h>
#include <stdlib.h>
#include <localdefs.h>
#include <sound.h>

#define MAXINST 10 /* maximum number of fluidsynth instances */
#define INST 4 /* number of fluidsynth plug instances to create */

/* fluidsynth device record */
typedef struct {

    fluid_settings_t*     settings;
    fluid_synth_t*        synth;
    fluid_audio_driver_t* adriver;
    int                   sfont_id;

} fluiddev;

typedef fluiddev* fluidptr; /* pointer to fludsynth device */

static fluidptr devtbl[MAXINST]; /* fluidsynth instance table */

/*******************************************************************************

Flag fluidsynth error

Prints an error and stops.

*******************************************************************************/

static void error(string es)

{

    fprintf(stderr, "\nError: Fluidsynth: %s\n", es);

    exit(1);

}

/*******************************************************************************

Open Liquidsynth MIDI device

Opens a Liquidsynth MIDI port for use. Does nothing at present, since we open
one MIDI out device at init time.

*******************************************************************************/

static void openfluid(int p)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!devtbl[p-1]) error("No Fluidsynth output port at logical handle");

}

/*******************************************************************************

Close Liquidsynth MIDI device

Closes a Liquidsynth MIDI output device for use.

*******************************************************************************/

static void closefluid(int p)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!devtbl[p-1]) error("No Fluidsynth output port at logical handle");

}

/*******************************************************************************

Write liquidsynth MIDI message

Accepts a MIDI message in PA sequencer format and outputs that.

Many functions don't have equivalents in Fluidsynth. This is not serious, I
found that many of these functions do nothing on most synthesizers.

Some of the questionable codes should be compared against what happens when the
midi codes are fed directly to Fluidsynth (source code analysis?).

*******************************************************************************/

static void writefluid(int p, ami_seqptr sp)

{

    fluidptr fp; /* pointer to fluidsynth device */

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!devtbl[p-1]) error("No Fluidsynth output port at logical handle");

    fp = devtbl[p-1]; /* point to device */
    switch (sp->st) { /* sequencer message type */

        case st_noteon:
            fluid_synth_noteon(fp->synth, sp->ntc-1, sp->ntn-1, sp->ntv/0x01000000);
            break;
        case st_noteoff:
            /* Note fluidsynth has no velocity parameter */
            fluid_synth_noteoff(fp->synth, sp->ntc-1, sp->ntn-1);
            break;
        case st_instchange:
            fluid_synth_program_change(fp->synth, sp->icc-1, sp->ici-1);
            break;
        case st_attack: break; /* no equivalent function */
        case st_release: break; /* no equivalent function */
        case st_legato:
            /* this call does not appear to be in the current Fluidsynth API */
            /*
            fluid_synth_set_legato_mode(fp->synth, sp->bsc,
                sp->bsb?FLUID_CHANNEL_LEGATO_MODE_MULTI_RETRIGGER:
                        FLUID_CHANNEL_LEGATO_MODE_RETRIGGER);
            */
            break;
        case st_portamento:
            /* midi says on/off, but fluidsynth has three options, no apparent off */
            break;
        case st_vibrato:      break; /* no equivalent function */
        case st_volsynthchan: break; /* no equivalent function */
        case st_porttime:     break; /* no equivalent function */
        case st_balance:      break; /* no equivalent function */
        case st_pan:          break; /* no equivalent function */
        case st_timbre:       break; /* no equivalent function */
        case st_brightness:   break; /* no equivalent function */
        case st_reverb:       break; /* fluidsynth gives many options for this,
                                        needs research */
        case st_tremulo:      break; /* no equivalent function */
        case st_chorus:       break; /* fluidsynth gives many options for this,
                                        needs research */
        case st_celeste:      break; /* no equivalent function */
        case st_phaser:       break; /* no equivalent function */
        case st_aftertouch:   break; /* no equivalent function */
        case st_pressure:
            fluid_synth_channel_pressure(fp->synth, sp->ntc-1, sp->ntv/0x01000000);
            break;
        case st_pitch:
            fluid_synth_pitch_bend(fp->synth, sp->vsc-1, sp->vsv/0x00040000+0x2000);
            break;
        case st_pitchrange:
            /* this one is open for interpretation: what is a "semitone"? */
            fluid_synth_pitch_wheel_sens(fp->synth, sp->vsc-1, sp->vsv/0x00020000);
            break;
        case st_mono:         break; /* no equivalent function */
        case st_poly:         break; /* no equivalent function */
        case st_playsynth:
        case st_playwave:
            /* not a midi instruction, we send this back to sound.c */
            _pa_excseq(p, sp);
            break;
        case st_volwave:      break;

    }

}

/*******************************************************************************

Set parameter

Set plug in parameter from the given name and value. Not implemented at present.
Always returns error.

*******************************************************************************/

int setparamfluid(int p, string name, string value)

{

    return (1); /* always return error */

}

/*******************************************************************************

Get parameter

Get plug in parameter from the given name and value. Not implemented at present.
Always returns empty string.

*******************************************************************************/

void getparamfluid(int p, string name, string value, int len)

{

    *value = 0; /* clear output string */

}

/*******************************************************************************

Initialize Fluidsynth plug-in.

Registers Fluidsynth as a plug-in device with PA sound module.

*******************************************************************************/

static int stderr_file;

static void quiet(void)

{

    /* run alsa open in quiet */
    fflush(stderr);
    stderr_file = dup(fileno(stderr));
    freopen("/dev/null", "w", stderr);

}

static void unquiet(void)

{

    fflush(stderr);
    dup2(stderr_file, fileno(stderr));
    close(stderr_file);
    clearerr(stderr);

}

static void fluidsynth_plug_init (void) __attribute__((constructor (103)));
static void fluidsynth_plug_init()

{

    char buff[200];
    int i;
    int r;

    /* clear instance table */
    for (i = 0; i < MAXINST; i++) devtbl[i] = NULL;

    /* turn off liquidsynth error messages */
    quiet();

    /* create number of desired instances */
    for (i = 0; i < INST; i++) {

        /* create table entry */
        devtbl[i] = malloc(sizeof(fluiddev));
        if (!devtbl[i]) error("Cannot allocate device");
        /* create the settings */
        devtbl[i]->settings = new_fluid_settings();
        /* create the synthesizer */
        devtbl[i]->synth = new_fluid_synth(devtbl[i]->settings);
        /* create the audio driver as alsa type */
        fluid_settings_setstr(devtbl[i]->settings, "audio.driver", "alsa");
        /* disable realtime priority to suppress warning when not running as root */
        fluid_settings_setint(devtbl[i]->settings, "audio.realtime-prio", 0);
        devtbl[i]->adriver = new_fluid_audio_driver(devtbl[i]->settings, devtbl[i]->synth);
        /* load a SoundFont and reset presets */
        devtbl[i]->sfont_id = fluid_synth_sfload(devtbl[i]->synth, "/usr/share/sounds/sf2/FluidR3_GM.sf2", 1);
        /* fluidsynth default volume is very low. Turn up to reasonable value */
        fluid_settings_setnum(devtbl[i]->settings, "synth.gain", 1.0);

        /* show the device fluidsynth connects to (usually "default") */
        /*
        r = fluid_settings_copystr(settings, "audio.alsa.device", buff, 200);
        printf("The alsa PCM device for Fluidsynth is: %s\n", buff);
        */

        /* now install us as PA device */
        sprintf(buff, "Fluidsynth%d", i+1);
        _pa_synthoutplug(FALSE, buff, openfluid, closefluid, writefluid,
                         setparamfluid, getparamfluid);

    }
    /* re-enable error messages */
    unquiet();



}

/*******************************************************************************

Deinitialize Fluidsynth plug-in.

Clean up Fluidsynth instance.

*******************************************************************************/

static void fluidsynth_plug_deinit (void) __attribute__((destructor (103)));
static void fluidsynth_plug_deinit()

{

    int i;

    /* Clean up */
    for (i = 0; i < MAXINST; i++) if (devtbl[i]) {

        delete_fluid_audio_driver(devtbl[i]->adriver);
        delete_fluid_synth(devtbl[i]->synth);
        delete_fluid_settings(devtbl[i]->settings);

    }

}
