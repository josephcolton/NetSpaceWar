#ifndef AUDIO_H
#define AUDIO_H

#include "cdata.h"

/* Sound effects and music for the game client (the AI and server don't
   link this). Everything is generated in code when audio starts, so there
   are no sound files. The sound numbers (SOUND_*) are in cdata.h, because
   client.c notices most of them while reading the server's updates. */

#define AUDIO_VOLUME_MAX 10

enum { LOOP_THRUST, LOOP_HUM, LOOP_COUNT }; // Sounds that play while a key is held
enum { MUSIC_MENU, MUSIC_GAME };

// Start audio. Safe to call more than once; with no audio device everything stays silent.
void audioInit();
// Play a sound centered at full volume (our own ship, the menus)...
void audioPlay(int sound);
// ...or from (dx, dy) world units away from us: quieter with distance, panned left or right
void audioPlayAt(int sound, double dx, double dy);
// Turn a held sound on or off; it fades in and out
void audioLoop(int loop, int on);
// Which music plays: calm in the menus, livelier in a game
void audioMusic(int mood);
// Volumes from 0 to AUDIO_VOLUME_MAX
void audioSetVolume(int sound, int music);
void audioToggleMute();
int audioMuted();

#endif
