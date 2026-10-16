/* SDL2 implementation of platform.h */
#include "platform.h"

#include <SDL.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static int scrW, scrH;
static char dataDir[1024] = "gamedata";
static int virtClock;
static uint32_t virtNow;

int Plat_init(int screenW, int screenH, const char *title)
{
	int scale = 3;
	SDL_DisplayMode dm;

	scrW = screenW;
	scrH = screenH;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return -1;
	}
	if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
		scale = (dm.h * 8 / 10) / screenH;
		if (scale < 1)
			scale = 1;
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			       screenW * scale, screenH * scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (!win) {
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return -1;
	}
	/* No vsync: the game presents inside its tick, and a present that waits for the display's refresh would
	 * stretch the 40 ms game timer (the game ran slower on a 60 Hz display). The game draws only on its ticks. */
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	if (!ren)
		ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
	if (!ren) {
		fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
		return -1;
	}
	SDL_RenderSetLogicalSize(ren, screenW, screenH);
	SDL_RenderSetIntegerScale(ren, SDL_FALSE);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, screenW, screenH);
	if (!tex) {
		fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
		return -1;
	}
	return 0;
}

void Plat_shutdown(void)
{
	Audio_shutdown();
	if (tex)
		SDL_DestroyTexture(tex);
	if (ren)
		SDL_DestroyRenderer(ren);
	if (win)
		SDL_DestroyWindow(win);
	tex = NULL;
	ren = NULL;
	win = NULL;
	SDL_Quit();
}

void Plat_setVirtualClock(int enabled, uint32_t nowMs) { virtClock = enabled; virtNow = nowMs; }
uint32_t Plat_uptimeMs(void) { return virtClock ? virtNow : SDL_GetTicks(); }
void Plat_delay(uint32_t ms) { SDL_Delay(ms); }
void Plat_vibrate(int ms) { (void)ms; }
void Plat_setWindowTitle(const char *title) { if (win) SDL_SetWindowTitle(win, title); }

void Plat_present(const uint16_t *fb, int pitchPixels)
{
	if (!tex || virtClock) /* headless test runs: nothing to show; skipping the upload/present keeps them fast */
		return;
	SDL_UpdateTexture(tex, NULL, fb, pitchPixels * 2);
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	SDL_RenderCopy(ren, tex, NULL, NULL);
	SDL_RenderPresent(ren);
}

/* PC keyboard -> phone keypad. Digits stay digits (cheats, number entry). */
static int map_key(SDL_Keycode k)
{
	switch (k) {
	case SDLK_UP: case SDLK_w: return AVK_UP;
	case SDLK_DOWN: case SDLK_s: return AVK_DOWN;
	case SDLK_LEFT: return AVK_LEFT;
	case SDLK_RIGHT: return AVK_RIGHT;
	case SDLK_a: return AVK_1;          /* strafe left */
	case SDLK_d: return AVK_3;          /* strafe right */
	case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return AVK_SELECT;
	case SDLK_ESCAPE: return AVK_SOFT1; /* menu / back soft key */
	case SDLK_BACKSPACE: return AVK_CLR;
	case SDLK_TAB: return AVK_POUND;    /* automap */
	case SDLK_z: return AVK_7;          /* next weapon */
	case SDLK_x: return AVK_STAR;       /* previous weapon */
	case SDLK_c: return AVK_9;          /* pass turn */
	case SDLK_q: return AVK_SOFT1;
	case SDLK_e: return AVK_SOFT2;
	case SDLK_KP_PLUS: case SDLK_PAGEUP: return AVK_VOLUME_UP;
	case SDLK_KP_MINUS: case SDLK_PAGEDOWN: return AVK_VOLUME_DOWN;
	case SDLK_ASTERISK: case SDLK_KP_MULTIPLY: return AVK_STAR;
	case SDLK_HASH: return AVK_POUND;
	default:
		if (k >= SDLK_0 && k <= SDLK_9)
			return AVK_0 + (int)(k - SDLK_0);
		if (k >= SDLK_KP_1 && k <= SDLK_KP_9)
			return AVK_1 + (int)(k - SDLK_KP_1);
		if (k == SDLK_KP_0)
			return AVK_0;
		return 0;
	}
}

int Plat_pollEvents(PlatKeyEvent *ev, int max, int *count)
{
	SDL_Event e;
	int n = 0, keepGoing = 1;
	while (n < max && SDL_PollEvent(&e)) { /* a full buffer leaves the rest for the next poll */
		switch (e.type) {
		case SDL_QUIT:
			keepGoing = 0;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP: {
			int avk;
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11) {
				Uint32 fl = SDL_GetWindowFlags(win);
				SDL_SetWindowFullscreen(win, (fl & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
				break;
			}
			avk = map_key(e.key.keysym.sym);
			if (avk) {
				ev[n].avk = avk;
				ev[n].down = e.type == SDL_KEYDOWN;
				ev[n].repeat = e.key.repeat;
				n++;
			}
			break;
		}
		default:
			break;
		}
	}
	*count = n;
	return keepGoing;
}

int Plat_isMinimized(void)
{
	return win && (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED) != 0;
}

/* ---- files ---- */

void Plat_setDataDir(const char *dir)
{
	snprintf(dataDir, sizeof(dataDir), "%s", dir);
}

const char *Plat_dataDir(void) { return dataDir; }

const char *Plat_dataPath(const char *name)
{
	static char path[2048];
	snprintf(path, sizeof(path), "%s/%s", dataDir, name);
	return path;
}

int Plat_fileRead(const char *name, uint8_t **data, size_t *len)
{
	FILE *f = fopen(Plat_dataPath(name), "rb");
	long n;
	*data = NULL;
	*len = 0;
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	*data = malloc((size_t)n + 1);
	if (!*data || fread(*data, 1, (size_t)n, f) != (size_t)n) {
		free(*data);
		*data = NULL;
		fclose(f);
		return -1;
	}
	fclose(f);
	*len = (size_t)n;
	return 0;
}

int Plat_fileWrite(const char *name, const void *data, size_t len)
{
	char tmp[2100];
	FILE *f;
	snprintf(tmp, sizeof(tmp), "%s.tmp", Plat_dataPath(name));
	f = fopen(tmp, "wb");
	if (!f)
		return -1;
	if (fwrite(data, 1, len, f) != len) {
		fclose(f);
		remove(tmp);
		return -1;
	}
	if (fclose(f) != 0) {
		remove(tmp);
		return -1;
	}
	return rename(tmp, Plat_dataPath(name)) == 0 ? 0 : -1;
}

int Plat_fileRemove(const char *name) { return remove(Plat_dataPath(name)) == 0 ? 0 : -1; }

int Plat_fileExists(const char *name)
{
	struct stat st;
	return stat(Plat_dataPath(name), &st) == 0;
}

/* ---- audio: tiny mixer, windowed-sinc resampling ----
 * The game's sounds are 8 kHz PCM; the device runs at 48 kHz or so. Linear interpolation leaves strong images
 * of the source spectrum above 4 kHz (a harsh, metallic edge), so each output sample is instead a 32-tap
 * Kaiser-windowed sinc over the source, low-passed just under the source Nyquist. The kernel is tabulated at
 * RS_PHASES fractional positions and linearly interpolated between them. It assumes upsampling (device rate
 * >= source rate), which is always the case here. */

#define RS_HALF   16                 /* taps each side of the output position */
#define RS_TAPS   (2 * RS_HALF)
#define RS_PHASES 256
#define RS_CUTOFF 0.45               /* pass band edge, as a fraction of the source rate */
#define RS_BETA   8.0                /* Kaiser window shape */

static float rsKernel[RS_PHASES + 1][RS_TAPS];

static double bessel_i0(double x)
{
	double sum = 1.0, term = 1.0;
	for (int k = 1; k < 32; k++) {
		term *= (x / (2.0 * k)) * (x / (2.0 * k));
		sum += term;
	}
	return sum;
}

static void rs_build_kernel(void)
{
	for (int ph = 0; ph <= RS_PHASES; ph++) {
		double frac = (double)ph / RS_PHASES, sum = 0.0, row[RS_TAPS];
		for (int t = 0; t < RS_TAPS; t++) {
			double x = (double)(t - (RS_HALF - 1)) - frac; /* source offset from the output position */
			double w = x / RS_HALF, sinc;
			double arg = 2.0 * M_PI * RS_CUTOFF * x;
			sinc = fabs(x) < 1e-9 ? 1.0 : sin(arg) / arg;
			row[t] = fabs(w) >= 1.0 ? 0.0 : sinc * bessel_i0(RS_BETA * sqrt(1.0 - w * w)) / bessel_i0(RS_BETA);
			sum += row[t];
		}
		for (int t = 0; t < RS_TAPS; t++)
			rsKernel[ph][t] = (float)(row[t] / sum); /* unity gain at DC for every phase */
	}
}

typedef struct {
	const int16_t *pcm;
	int samples;
	uint32_t step;   /* 16.16 source samples per output sample */
	uint64_t pos;    /* 16.16 */
	int loops;       /* remaining extra plays; -1 forever */
	int volume;      /* 0..100 */
	int playing;
} Chan;

static SDL_AudioDeviceID dev;
static SDL_AudioSpec have;
static Chan chans[AUDIO_CHANNELS];

/* Source sample at the 16.16 position `pos`, band-limited. Samples outside the clip count as silence. */
static float rs_sample(const Chan *ch, uint64_t pos)
{
	int ip = (int)(pos >> 16), first = ip - (RS_HALF - 1);
	uint32_t fr = (uint32_t)(pos & 0xffff);
	int ph = (int)(fr >> 8);                     /* 65536 / RS_PHASES */
	float mix = (float)(fr & 0xff) / 256.0f, acc = 0.0f;
	const float *k0 = rsKernel[ph], *k1 = rsKernel[ph + 1];
	int t0 = first < 0 ? -first : 0;
	int t1 = first + RS_TAPS > ch->samples ? ch->samples - first : RS_TAPS;
	for (int t = t0; t < t1; t++)
		acc += (float)ch->pcm[first + t] * (k0[t] + (k1[t] - k0[t]) * mix);
	return acc;
}

static void mix_cb(void *ud, Uint8 *stream, int len)
{
	int16_t *out = (int16_t *)stream;
	int frames = len / (int)(sizeof(int16_t) * have.channels);
	(void)ud;
	for (int i = 0; i < frames; i++) {
		float acc = 0.0f;
		int s;
		for (int c = 0; c < AUDIO_CHANNELS; c++) {
			Chan *ch = &chans[c];
			if (!ch->playing)
				continue;
			if ((uint32_t)(ch->pos >> 16) >= (uint32_t)ch->samples) {
				if (ch->loops != 0) {
					if (ch->loops > 0)
						ch->loops--;
					ch->pos = 0;
				} else if ((uint32_t)(ch->pos >> 16) >= (uint32_t)ch->samples + RS_HALF) {
					ch->playing = 0;
					continue;
				}
				/* else: let the filter ring out over silence for RS_HALF source samples (no click) */
			}
			acc += rs_sample(ch, ch->pos) * (float)ch->volume / 100.0f;
			ch->pos += ch->step;
		}
		s = (int)lrintf(acc);
		if (s > 32767)
			s = 32767;
		if (s < -32768)
			s = -32768;
		for (int k = 0; k < have.channels; k++)
			*out++ = (int16_t)s;
	}
}

int Audio_init(void)
{
	SDL_AudioSpec want;
	SDL_zero(want);
	want.freq = 48000;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = mix_cb;
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
	if (!dev || have.format != AUDIO_S16SYS) {
		fprintf(stderr, "Audio disabled: %s\n", SDL_GetError());
		if (dev)
			SDL_CloseAudioDevice(dev);
		dev = 0;
		return -1;
	}
	rs_build_kernel();
	for (int c = 0; c < AUDIO_CHANNELS; c++)
		chans[c].volume = 100;
	SDL_PauseAudioDevice(dev, 0);
	return 0;
}

void Audio_shutdown(void)
{
	if (dev)
		SDL_CloseAudioDevice(dev);
	dev = 0;
}

void Audio_play(int channel, const int16_t *pcm, int samples, int rate, int loops)
{
	Chan *ch;
	if (!dev || channel < 0 || channel >= AUDIO_CHANNELS || !pcm || samples <= 0)
		return;
	SDL_LockAudioDevice(dev);
	ch = &chans[channel];
	ch->pcm = pcm;
	ch->samples = samples;
	ch->step = (uint32_t)(((uint64_t)rate << 16) / (uint64_t)have.freq);
	ch->pos = 0;
	ch->loops = loops;
	ch->playing = 1;
	SDL_UnlockAudioDevice(dev);
}

void Audio_stop(int channel)
{
	if (!dev || channel < 0 || channel >= AUDIO_CHANNELS)
		return;
	SDL_LockAudioDevice(dev);
	chans[channel].playing = 0;
	SDL_UnlockAudioDevice(dev);
}

int Audio_isPlaying(int channel)
{
	int p;
	if (!dev || channel < 0 || channel >= AUDIO_CHANNELS)
		return 0;
	SDL_LockAudioDevice(dev);
	/* a sound counts as finished at its last sample, even while the resampler's tail still rings out */
	p = chans[channel].playing && (uint32_t)(chans[channel].pos >> 16) < (uint32_t)chans[channel].samples;
	SDL_UnlockAudioDevice(dev);
	return p;
}

void Audio_setVolume(int channel, int volume0to100)
{
	if (channel < 0 || channel >= AUDIO_CHANNELS)
		return;
	if (volume0to100 < 0)
		volume0to100 = 0;
	if (volume0to100 > 100)
		volume0to100 = 100;
	if (dev)
		SDL_LockAudioDevice(dev);
	chans[channel].volume = volume0to100;
	if (dev)
		SDL_UnlockAudioDevice(dev);
}

void Audio_pauseAll(int pause)
{
	if (dev)
		SDL_PauseAudioDevice(dev, pause);
}
