#include <SDL3/SDL.h>
#include "global.h"
#include "cdata.h"
#include "audio.h"

#define AUDIO_RATE     44100
#define AUDIO_VOICES   24    // Sound effects playing at once; a new one replaces the oldest
#define AUDIO_HEARING  80.0  // Sounds farther away than this (world units) are silent
#define AUDIO_FALLOFF  25.0  // Distance at which a sound is at half volume
#define AUDIO_PAN      35.0  // Distance to the side at which a sound is fully left or right

/* One generated sound: mono samples from -1 to 1 */
typedef struct {
  float *data;
  int length;
} Sample;

typedef struct {
  int sound;
  int pos;
  float left, right;
} Voice;

static SDL_AudioStream *stream;
static int audioFailed;
static Sample sounds[SOUND_COUNT];
static Sample loops[LOOP_COUNT];
static double lastStart[SOUND_COUNT];

/* Everything below is shared with the audio thread: change it only while
   the stream is locked */
static Voice voices[AUDIO_VOICES];
static int voiceCount;
static float loopLevel[LOOP_COUNT], loopTarget[LOOP_COUNT];
static int loopPos[LOOP_COUNT];
static float soundGain = 0.8f, musicGain = 0.5f;
static int muted;
static int musicMood = MUSIC_MENU;
static long long musicPos; // Samples since the music started

/* Making the sounds */

static float randNoise() {
  return (float)(rand() / (RAND_MAX / 2.0) - 1.0);
}

static float *newSample(Sample *s, double seconds) {
  s->length = (int)(seconds * AUDIO_RATE);
  s->data = (float *)SDL_calloc(s->length, sizeof(float));
  return s->data;
}

// Scale so the loudest point is at peak
static void normalize(Sample *s, float peak) {
  float most = 0.0001f;
  int i;
  for (i=0; i < s->length; i++) if (fabsf(s->data[i]) > most) most = fabsf(s->data[i]);
  for (i=0; i < s->length; i++) s->data[i] *= peak / most;
}

// Blend the last 0.1 s into the first, so the sample loops without a click
static void makeLoop(Sample *s) {
  int fade = AUDIO_RATE / 10, i;
  for (i=0; i < fade; i++) {
    float t = (float)i / fade;
    s->data[i] = s->data[i] * t + s->data[s->length - fade + i] * (1 - t);
  }
  s->length -= fade;
}

static void makeSounds() {
  float *d, y = 0, y2 = 0, phase = 0;
  double t, f;
  int i, k;
  Sample *s;

  // Missile: a laser "pew", a falling square-ish tone
  s = &sounds[SOUND_FIRE]; d = newSample(s, 0.16);
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    f = 1500 * pow(0.25, t / 0.16);
    phase += (float)(f / AUDIO_RATE);
    d[i] = (float)((sinf(2 * (float)M_PI * phase) > 0 ? 0.5 : -0.5) + 0.5 * sinf(2 * (float)M_PI * phase)) * (float)exp(-t * 18);
  }
  normalize(s, 0.3f);

  // Explosion: noise that darkens as it fades, over a low thump
  s = &sounds[SOUND_EXPLOSION]; d = newSample(s, 0.9);
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    y += (float)(0.02 + 0.25 * exp(-t * 5)) * (randNoise() - y);
    d[i] = (float)(y * exp(-t * 3.5) + 0.5 * sin(2 * M_PI * 55 * t) * exp(-t * 8));
  }
  normalize(s, 0.6f);

  // Hit on our own ship: a sharp crack and a low buzz
  s = &sounds[SOUND_HIT]; d = newSample(s, 0.2);
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    d[i] = (float)(0.6 * randNoise() * exp(-t * 30) + 0.4 * (sin(2 * M_PI * 160 * t) > 0 ? 1 : -1) * exp(-t * 20));
  }
  normalize(s, 0.5f);

  // Hyperjump: a rising, warbling sweep
  s = &sounds[SOUND_JUMP]; d = newSample(s, 0.5); phase = 0;
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    f = 220 * pow(10, t / 0.5) * (1 + 0.03 * sin(2 * M_PI * 18 * t));
    phase += (float)(f / AUDIO_RATE);
    d[i] = sinf(2 * (float)M_PI * phase) * (float)(fmin(t / 0.02, 1) * exp(-t * 3));
  }
  normalize(s, 0.4f);

  // Recharge orb: a rising four-note chime
  s = &sounds[SOUND_ORB]; d = newSample(s, 0.8);
  for (k=0; k < 4; k++) {
    static const int notes[4] = {72, 76, 79, 84};
    double start = k * 0.07, freq = 440 * pow(2, (notes[k] - 69) / 12.0);
    for (i=(int)(start * AUDIO_RATE); i < s->length; i++) {
      t = (double)i / AUDIO_RATE - start;
      d[i] += (float)((sin(2 * M_PI * freq * t) + 0.3 * sin(4 * M_PI * freq * t)) * exp(-t * 6));
    }
  }
  normalize(s, 0.45f);

  // Volcano: a low rumble
  s = &sounds[SOUND_ERUPT]; d = newSample(s, 0.8); y = 0;
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    y += 0.03f * (randNoise() - y);
    d[i] = (float)((y * 4 + 0.4 * sin(2 * M_PI * 45 * t)) * fmin(t / 0.05, 1) * exp(-t * 3));
  }
  normalize(s, 0.5f);

  // Asteroid breaking up: crunchy, crackling noise
  s = &sounds[SOUND_ROCK]; d = newSample(s, 0.55); y = 0;
  for (i=0; i < s->length; i++) {
    static float crackle = 1;
    t = (double)i / AUDIO_RATE;
    if (i % 250 == 0) crackle = (rand() % 3 == 0) ? 1.0f : 0.3f;
    y += 0.15f * (randNoise() - y);
    d[i] = (float)(y * crackle * exp(-t * 6) + 0.3 * sin(2 * M_PI * 70 * t) * exp(-t * 9));
  }
  normalize(s, 0.55f);

  // A volcano waking: a deep rumble swells, breaks through with a heavy
  // whoomp, then lava bubbles and settles
  s = &sounds[SOUND_VOLCANO_WAKE]; d = newSample(s, 2.2); y = y2 = 0; phase = 0;
  for (i=0; i < s->length; i++) {
    double swell, bubble = 0;
    t = (double)i / AUDIO_RATE;
    y += 0.02f * (randNoise() - y);
    swell = (t < 0.6) ? t / 0.6 : exp(-(t - 0.6) * 1.6);
    f = 35 + 30 * fmin(t / 0.6, 1);
    phase += (float)(f / AUDIO_RATE);
    d[i] = (float)((y * 5 + 0.5 * sin(2 * M_PI * phase)) * swell);
    // The whoomp
    if (t >= 0.6) {
      y2 += 0.12f * (randNoise() - y2);
      d[i] += (float)((y2 * 1.5 + 0.8 * sin(2 * M_PI * 70 * (t - 0.6))) * exp(-(t - 0.6) * 7));
    }
    // Bubbles: short low blips, fewer as it settles
    for (k=0; k < 12; k++) {
      double start = 0.75 + k * 0.11 + 0.04 * sin(k * 7.3), freq = 90 + 70 * fabs(sin(k * 3.1));
      if (t > start && t < start + 0.09) bubble += sin(2 * M_PI * freq * (t - start) * (1 + 4 * (t - start))) * exp(-(t - start) * 40) * exp(-k * 0.15);
    }
    d[i] += (float)(0.6 * bubble);
  }
  normalize(s, 0.7f);

  // A volcano dying out: the rumble sinks and fades under a long hiss of
  // steam as the lava cools
  s = &sounds[SOUND_VOLCANO_DIE]; d = newSample(s, 1.8); y = y2 = 0; phase = 0;
  for (i=0; i < s->length; i++) {
    float lowpass;
    t = (double)i / AUDIO_RATE;
    f = 75 * pow(0.4, t / 1.8);
    phase += (float)(f / AUDIO_RATE);
    y += 0.02f * (randNoise() - y);
    d[i] = (float)((y * 4 + 0.6 * sin(2 * M_PI * phase)) * exp(-t * 2.2));
    // Steam: noise with the low end taken out, rising in then fading
    lowpass = y2 += 0.3f * (randNoise() - y2);
    d[i] += (float)((randNoise() - lowpass) * 0.35 * fmin(t / 0.3, 1) * exp(-t * 1.6));
  }
  normalize(s, 0.6f);

  // Menu: a tick to move, a two-tone blip to choose
  s = &sounds[SOUND_MENU_MOVE]; d = newSample(s, 0.035);
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    d[i] = (float)((sin(2 * M_PI * 1100 * t) > 0 ? 1 : -1) * (1 - t / 0.035));
  }
  normalize(s, 0.12f);
  s = &sounds[SOUND_MENU_SELECT]; d = newSample(s, 0.1);
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    d[i] = (float)(sin(2 * M_PI * (t < 0.05 ? 700 : 1050) * t) * (1 - t / 0.1));
  }
  normalize(s, 0.2f);

  // Engine: a deep rushing noise, looped
  s = &loops[LOOP_THRUST]; d = newSample(s, 1.1); y = y2 = 0;
  for (i=0; i < s->length; i++) {
    y = y * 0.995f + randNoise() * 0.05f; // Brown noise...
    y2 += 0.08f * (y - y2);               // ...softened
    d[i] = y2;
  }
  makeLoop(s);
  normalize(s, 0.3f);

  // Power transfer: an electrical hum and buzz. Whole-number frequencies
  // over one second, so it loops cleanly.
  s = &loops[LOOP_HUM]; d = newSample(s, 1.0);
  for (i=0; i < s->length; i++) {
    t = (double)i / AUDIO_RATE;
    double saw = 2 * fmod(100 * t, 1.0) - 1;
    d[i] = (float)((0.5 * saw + 0.6 * sin(2 * M_PI * 200 * t) + 0.5 * sin(2 * M_PI * 50 * t)) * (0.8 + 0.2 * sin(2 * M_PI * 4 * t)));
  }
  normalize(s, 0.2f);
}

/* Music: a slow chord progression (Am F C G) on soft pads with an
   arpeggio over it; in a game it is quicker and adds a bass line */

static const int musicChords[4][3] = {{57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62}};
static float padPhase[6], arpPhase, bassPhase;

static float tri(float phase) {
  phase -= floorf(phase);
  return 4 * fabsf(phase - 0.5f) - 1;
}

static float noteFreq(int midi) {
  return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

static float musicSample() {
  int stepLen = (int)(AUDIO_RATE * (musicMood == MUSIC_GAME ? 0.13 : 0.21)); // A sixteenth note
  long long step = musicPos / stepLen;
  int bar = (int)((step / 16) % 4), inBar = (int)(step % 16);
  float t = (float)(musicPos % stepLen) / AUDIO_RATE; // Into this step
  float barT = (float)(musicPos % (stepLen * 16)) / AUDIO_RATE;
  float out = 0, env;
  int k, note;

  // Pads: each chord tone twice, slightly detuned, swelling in each bar
  env = fminf(barT / 0.4f, 1.0f) * 0.85f + 0.15f;
  for (k=0; k < 3; k++) {
    float f = noteFreq(musicChords[bar][k]);
    padPhase[k] += f / AUDIO_RATE;
    padPhase[k + 3] += f * 1.003f / AUDIO_RATE;
    out += (tri(padPhase[k]) + tri(padPhase[k + 3])) * 0.025f * env;
  }

  // Arpeggio up an octave: every step in a game, every other step in the menus
  if (musicMood == MUSIC_GAME || inBar % 2 == 0) {
    note = musicChords[bar][(musicMood == MUSIC_GAME ? inBar : inBar / 2) % 3] + 12 + (inBar >= 8 ? 12 : 0);
    arpPhase += noteFreq(note) / AUDIO_RATE;
    out += (musicMood == MUSIC_GAME ? tri(arpPhase) : sinf(2 * (float)M_PI * arpPhase)) * 0.035f *
           expf(-t * (musicMood == MUSIC_GAME ? 10.0f : 4.0f));
  }

  // Bass on the beat, in a game
  if (musicMood == MUSIC_GAME) {
    float beatT = (float)(musicPos % (stepLen * 4)) / AUDIO_RATE;
    bassPhase += noteFreq(musicChords[bar][0] - 12) / AUDIO_RATE;
    out += tri(bassPhase) * 0.09f * expf(-beatT * 5);
  }

  // Keep the phases small, so float precision never runs out
  for (k=0; k < 6; k++) padPhase[k] -= floorf(padPhase[k]);
  arpPhase -= floorf(arpPhase);
  bassPhase -= floorf(bassPhase);

  musicPos++;
  return out * 2.5f; // At full music volume, about as loud as the effects
}

/* Mixing, on the audio thread */

static void mix(float *out, int frames) {
  int f, v, k;
  float left, right, sample, music;

  for (f=0; f < frames; f++) {
    left = right = 0;
    for (v=0; v < voiceCount; v++) {
      Sample *s = &sounds[voices[v].sound];
      sample = s->data[voices[v].pos++];
      left += sample * voices[v].left;
      right += sample * voices[v].right;
      if (voices[v].pos >= s->length) {
        voices[v--] = voices[--voiceCount]; // Finished: drop it
      }
    }
    for (k=0; k < LOOP_COUNT; k++) {
      // Fade toward on or off over about 50 ms
      loopLevel[k] += (loopTarget[k] - loopLevel[k]) * 0.0005f;
      if (loopLevel[k] < 0.001f) continue;
      sample = loops[k].data[loopPos[k]] * loopLevel[k];
      loopPos[k] = (loopPos[k] + 1) % loops[k].length;
      left += sample;
      right += sample;
    }
    music = musicSample() * musicGain;
    left = left * soundGain + music;
    right = right * soundGain + music;
    if (muted) left = right = 0;
    out[2 * f] = fmaxf(-1.0f, fminf(1.0f, left));
    out[2 * f + 1] = fmaxf(-1.0f, fminf(1.0f, right));
  }
}

// SDL asks for more audio; the stream is locked while this runs
static void SDLCALL audioCallback(void *userdata, SDL_AudioStream *s, int additional, int total) {
  float buffer[1024];
  int frames = additional / (int)(2 * sizeof(float));
  (void)userdata;
  (void)total;
  while (frames > 0) {
    int n = frames < 512 ? frames : 512;
    mix(buffer, n);
    SDL_PutAudioStreamData(s, buffer, n * 2 * (int)sizeof(float));
    frames -= n;
  }
}

/* Called from the game */

void audioInit() {
  SDL_AudioSpec spec;
  if (stream != NULL || audioFailed) return;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    audioFailed = 1;
    return;
  }
  makeSounds();
  spec.format = SDL_AUDIO_F32;
  spec.channels = 2;
  spec.freq = AUDIO_RATE;
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, NULL);
  if (stream == NULL) {
    audioFailed = 1;
    return;
  }
  SDL_ResumeAudioStreamDevice(stream);
}

void audioPlayAt(int sound, double dx, double dy) {
  double dist = sqrt(dx * dx + dy * dy), now = platformNow(), volume, pan;
  Voice *voice;
  int v;

  if (stream == NULL || sound < 0 || sound >= SOUND_COUNT) return;
  // A volcano waking or dying is heard across most of the map
  if (sound == SOUND_VOLCANO_WAKE || sound == SOUND_VOLCANO_DIE) dist /= 2;
  if (dist > AUDIO_HEARING) return;
  // Many explosions at once (a ship makes five) sound like one
  if (now - lastStart[sound] < (sound == SOUND_EXPLOSION ? 0.06 : 0.02)) return;
  lastStart[sound] = now;

  volume = 1.0 / (1.0 + (dist / AUDIO_FALLOFF) * (dist / AUDIO_FALLOFF));
  pan = fmax(-1.0, fmin(1.0, dx / AUDIO_PAN));
  SDL_LockAudioStream(stream);
  if (voiceCount < AUDIO_VOICES) {
    voice = &voices[voiceCount++];
  } else {
    // Replace the one that has played longest
    voice = &voices[0];
    for (v=1; v < AUDIO_VOICES; v++) if (voices[v].pos > voice->pos) voice = &voices[v];
  }
  voice->sound = sound;
  voice->pos = 0;
  // Equal-power pan, scaled so a centered sound is at full volume
  voice->left = (float)(volume * cos((pan + 1) * M_PI / 4) * M_SQRT2);
  voice->right = (float)(volume * sin((pan + 1) * M_PI / 4) * M_SQRT2);
  SDL_UnlockAudioStream(stream);
}

void audioPlay(int sound) {
  audioPlayAt(sound, 0.0, 0.0);
}

void audioLoop(int loop, int on) {
  if (stream == NULL) return;
  SDL_LockAudioStream(stream);
  loopTarget[loop] = on ? 1.0f : 0.0f;
  SDL_UnlockAudioStream(stream);
}

void audioMusic(int mood) {
  if (stream == NULL) return;
  SDL_LockAudioStream(stream);
  if (mood != musicMood) musicPos = 0;
  musicMood = mood;
  SDL_UnlockAudioStream(stream);
}

void audioSetVolume(int sound, int music) {
  if (stream == NULL) return;
  SDL_LockAudioStream(stream);
  soundGain = (float)sound / AUDIO_VOLUME_MAX;
  musicGain = (float)music / AUDIO_VOLUME_MAX;
  SDL_UnlockAudioStream(stream);
}

void audioToggleMute() {
  if (stream == NULL) return;
  SDL_LockAudioStream(stream);
  muted = !muted;
  SDL_UnlockAudioStream(stream);
}

int audioMuted() {
  return muted;
}
