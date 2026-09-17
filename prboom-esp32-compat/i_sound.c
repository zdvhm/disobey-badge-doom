// Sound system for ESP32-S3 DOOM (Disobey Badge 2025/2026)
// Audio disabled - ESP32-S3 has no built-in DAC and badge has no speaker

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z_zone.h"
#include "m_swap.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "d_main.h"
#include "dma.h"

bool audioStarted = false;

int snd_card = 0;
int mus_card = 0;
int snd_samplerate = 0;

#define NUM_MIX_CHANNELS 8

int lengths[NUMSFX];
unsigned char *channels[NUM_MIX_CHANNELS];
unsigned char *channelsend[NUM_MIX_CHANNELS];
int channelstart[NUM_MIX_CHANNELS];
int channelids[NUM_MIX_CHANNELS];

void *getsfx(char *sfxname, int *len)
{
    char name[20];
    int sfxlump;

    sprintf(name, "ds%s", sfxname);
    if (W_CheckNumForName(name) == -1)
        sfxlump = W_GetNumForName("dspistol");
    else
        sfxlump = W_GetNumForName(name);

    *len = W_LumpLength(sfxlump);
    return (void *)W_CacheLumpNum(sfxlump);
}

int addsfx(int sfxid, int volume, int step, int seperation)
{
    int i;
    int oldest = gametic;
    int oldestnum = 0;
    int slot;

    if (sfxid == sfx_sawup || sfxid == sfx_sawidl ||
        sfxid == sfx_sawful || sfxid == sfx_sawhit ||
        sfxid == sfx_stnmov || sfxid == sfx_pistol) {
        for (i = 0; i < NUM_MIX_CHANNELS; i++) {
            if (channels[i] && channelids[i] == sfxid) {
                channels[i] = 0;
                break;
            }
        }
    }

    for (i = 0; (i < NUM_MIX_CHANNELS) && (channels[i]); i++) {
        if (channelstart[i] < oldest) {
            oldestnum = i;
            oldest = channelstart[i];
        }
    }

    if (i == NUM_MIX_CHANNELS)
        slot = oldestnum;
    else
        slot = i;

    channels[slot] = (unsigned char *)S_sfx[sfxid].data;
    channelsend[slot] = channels[slot] + lengths[sfxid];
    channelids[slot] = sfxid;
    return -1;
}

void I_UpdateSoundParams(int handle, int volume, int seperation, int pitch)
{
    snd_SfxVolume = volume;
}

void I_SetChannels() {}

int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

int I_StartSound(int id, int channel, int vol, int sep, int pitch, int priority)
{
    return addsfx(id, vol, 0, sep);
}

void I_StopSound(int handle) {}
int I_SoundIsPlaying(int handle) { return gametic < handle; }
int I_AnySoundStillPlaying(void) { return false; }
void I_UpdateSound(void) {}
void I_ShutdownSound(void) {}

void I_InitSound(void)
{
    lprintf(LO_INFO, "I_InitSound: Audio disabled (no DAC on ESP32-S3)\n");

    for (int i = 1; i < NUMSFX; i++) {
        if (!S_sfx[i].link) {
            S_sfx[i].data = getsfx(S_sfx[i].name, &lengths[i]);
        } else {
            S_sfx[i].data = S_sfx[i].link->data;
            lengths[i] = lengths[(S_sfx[i].link - S_sfx) / sizeof(sfxinfo_t)];
        }
    }

    lprintf(LO_INFO, "I_InitSound: pre-cached all sound data\n");
}

void I_ShutdownMusic(void) {}
void I_InitMusic(void) {}
void I_PlaySong(int handle, int looping) {}

extern int mus_pause_opt;

void I_PauseSong(int handle) {}
void I_ResumeSong(int handle) {}
void I_StopSong(int handle) {}
void I_UnRegisterSong(int handle) {}

int I_RegisterSong(const void *data, size_t len)
{
    return 0;
}

int I_RegisterMusic(const char *filename, musicinfo_t *song)
{
    return 1;
}

void I_SetMusicVolume(int volume) {}
