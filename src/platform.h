#ifndef OE2_PLATFORM_H
#define OE2_PLATFORM_H

/* Host services that replace the BREW runtime: window + framebuffer, keys,
 * time, files in gamedata/, and PCM sound channels. */

#include <stddef.h>
#include <stdint.h>

/* BREW virtual key codes (AEEVCodes.h) */
enum {
	AVK_0 = 0xE021, AVK_1, AVK_2, AVK_3, AVK_4, AVK_5, AVK_6, AVK_7, AVK_8, AVK_9,
	AVK_STAR = 0xE02B, AVK_POUND = 0xE02C, AVK_POWER, AVK_END, AVK_SEND,
	AVK_CLR = 0xE030, AVK_UP, AVK_DOWN, AVK_LEFT, AVK_RIGHT, AVK_SELECT,
	AVK_SOFT1 = 0xE036, AVK_SOFT2, AVK_SOFT3, AVK_SOFT4,
	AVK_VOLUME_UP = 0xE047, AVK_VOLUME_DOWN = 0xE048,
};

/* BREW event codes used by the applet */
enum {
	EVT_APP_START = 0, EVT_APP_STOP = 1, EVT_APP_SUSPEND = 2, EVT_APP_RESUME = 3,
	EVT_KEY = 0x100, EVT_KEY_PRESS = 0x101, EVT_KEY_RELEASE = 0x102,
};

typedef struct PlatKeyEvent {
	int avk;   /* BREW key code */
	int down;  /* 1 press (or auto-repeat), 0 release */
	int repeat;
} PlatKeyEvent;

int Plat_init(int screenW, int screenH, const char *title);
void Plat_shutdown(void);
uint32_t Plat_uptimeMs(void);
/* Deterministic test clock: when enabled, Plat_uptimeMs returns the value set here. */
void Plat_setVirtualClock(int enabled, uint32_t nowMs);
void Plat_delay(uint32_t ms);
/* Copy a screenW x screenH RGB565 framebuffer to the window. */
void Plat_present(const uint16_t *fb, int pitchPixels);
/* Returns 0 when the user asked to quit. Fills up to max events. */
int Plat_pollEvents(PlatKeyEvent *ev, int max, int *count);
/* 1 while the window is minimised (the host's equivalent of the phone suspending the app). */
int Plat_isMinimized(void);
void Plat_vibrate(int ms);
void Plat_setWindowTitle(const char *title);

/* gamedata directory (bar, saves). Set once at startup. */
void Plat_setDataDir(const char *dir);
const char *Plat_dataDir(void);
/* Path inside the data dir; returns a static buffer. */
const char *Plat_dataPath(const char *name);
int Plat_fileRead(const char *name, uint8_t **data, size_t *len);  /* 0 ok; *data malloc'd */
int Plat_fileWrite(const char *name, const void *data, size_t len); /* 0 ok */
int Plat_fileRemove(const char *name);
int Plat_fileExists(const char *name);

/* Sound: the game has 10 media channels. PCM is mono 16-bit at `rate` Hz. */
#define AUDIO_CHANNELS 10
int Audio_init(void);
void Audio_shutdown(void);
void Audio_play(int channel, const int16_t *pcm, int samples, int rate, int loops);
void Audio_stop(int channel);
int Audio_isPlaying(int channel);
void Audio_setVolume(int channel, int volume0to100);
void Audio_pauseAll(int pause);

#endif
