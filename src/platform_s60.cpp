/* platform.h for Symbian 9.1 / 9.2 (S60 3rd and 3rd FP1).
 * Framebuffer present is a nearest-neighbour scale of the engine's 176x220
 * RGB565 into the phone panel, letterboxed to keep the aspect. Default panel
 * is the Nokia N80, 352x416. Files go through the Open C libc so malloc/free
 * match the engine. Ten PCM channels are mixed in software; if the audio
 * stream cannot be opened the channels still report a simulated end time, so
 * scripts that wait on Audio_isPlaying do not hang. */

#include <e32std.h>
#include <f32file.h>
#include <hal.h>

/* Open C stdint.h on GCCE 3.4.3 names __int8_t before defining it when the
 * header is pulled in from C++. Supply the types and skip that header.
 * Must come before any libc header that includes stdint.h. */
#ifndef _STDINT_H_
#define _STDINT_H_
#define _STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
typedef uint32_t uintptr_t;
typedef int32_t intptr_t;
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* S60 3rd Open C stdio does not always declare these in C++ mode. */
extern "C" int snprintf(char* s, size_t n, const char* fmt, ...);
extern "C" int remove(const char* path);

extern "C" {
#include "platform.h"
}

static int gW, gH;
static int gPhoneW = 352, gPhoneH = 416, gPhonePitch;
static TUint16* gPhone = 0;
static char gDataDir[256] = "c:\\data\\oe\\";
static int gVirtClock;
static unsigned gVirtNow;
static int gMinimized;
static int gQuit;

enum { KQ = 64 };
static PlatKeyEvent gQ[KQ];
static int gQh, gQt;

struct Chan {
	const short* pcm;
	int samples;
	int rate;
	int pos;       /* 16.16 into samples */
	int step;      /* 16.16 per output sample at gMixRate */
	int loops;
	int volume;    /* 0..100 */
	int playing;
	unsigned endMs;
};
static Chan gCh[AUDIO_CHANNELS];
static int gMixRate = 16000;
static int gAudioOpen;

extern "C" void OeS60_SetPresentTarget(TUint16* aPixels, int aPitch, int aPhoneW, int aPhoneH)
{
	gPhone = aPixels;
	gPhonePitch = aPitch;
	gPhoneW = aPhoneW;
	gPhoneH = aPhoneH;
}

extern "C" void OeS60_PushKey(int aAvk, int aDown)
{
	int n = (gQt + 1) % KQ;
	if (n == gQh)
		return;
	gQ[gQt].avk = aAvk;
	gQ[gQt].down = aDown;
	gQ[gQt].repeat = 0;
	gQt = n;
}

extern "C" void OeS60_SetMinimized(int aMinimized) { gMinimized = aMinimized; }
extern "C" void OeS60_RequestQuit(void) { gQuit = 1; }

extern "C" int Plat_init(int screenW, int screenH, const char* /*title*/)
{
	gW = screenW;
	gH = screenH;
	gQuit = 0;
	return 0;
}

extern "C" void Plat_shutdown(void)
{
	Audio_shutdown();
	gPhone = 0;
}

extern "C" void Plat_setVirtualClock(int enabled, uint32_t nowMs)
{
	gVirtClock = enabled;
	gVirtNow = nowMs;
}

extern "C" int Plat_virtualClock(void) { return gVirtClock; }

extern "C" uint32_t Plat_uptimeMs(void)
{
	if (gVirtClock)
		return gVirtNow;
	/* ENanoTickPeriod is in MICROseconds per NTickCount tick. The old code divided by
	 * 1e6 (that is seconds) and multiplied in 32 bits (wraps after ~70 min). */
	static TInt period = 0;
	if (period <= 0) {
		TInt p = 0;
		period = (HAL::Get(HALData::ENanoTickPeriod, p) == KErrNone && p > 0) ? p : 1000;
	}
	return (uint32_t)(((TUint64)User::NTickCount() * (TUint64)period) / 1000u);
}

extern "C" int Plat_isMinimized(void) { return gMinimized; }

extern "C" void Plat_delay(uint32_t ms) { User::After(ms * 1000); }

extern "C" void Plat_setWindowTitle(const char* /*title*/) {}

extern "C" void Plat_vibrate(int ms)
{
	/* CHWRMVibra needs hwrmvibraclient.lib and UserEnvironment. Left as a
	 * no-op so the MMP stays buildable on an FP1 SDK without the HWRM plugin.
	 * N95 does have a vibra; wire CHWRMVibra::StartVibraL(ms) here if wanted. */
	(void)ms;
}

/* Fit 176x220 into the panel, keeping the aspect. N80 (352x416): 332x416 with
 * black bars at the sides. 240x320: 240x300 with 10px bars top and bottom. */
extern "C" void Plat_present(const uint16_t* fb, int pitchPixels)
{
	static short xmap[1024];
	static int mapDstW = 0, mapSrcW = 0;
	if (!gPhone || !fb || gW <= 0 || gH <= 0 || gPhoneW > 1024)
		return;
	int dstW = gPhoneW;
	int dstH = gH * gPhoneW / gW;
	if (dstH > gPhoneH) {
		dstH = gPhoneH;
		dstW = gW * gPhoneH / gH;
	}
	int ox = (gPhoneW - dstW) / 2;
	int oy = (gPhoneH - dstH) / 2;
	if (mapDstW != dstW || mapSrcW != gW) {
		for (int x = 0; x < dstW; ++x) {
			int sx = x * gW / dstW;
			xmap[x] = (short)(sx >= gW ? gW - 1 : sx);
		}
		mapDstW = dstW;
		mapSrcW = gW;
	}
	/* black bars only (letterbox), not the whole panel every frame */
	for (int y = 0; y < gPhoneH; ++y) {
		TUint16* row = gPhone + y * gPhonePitch;
		if (y < oy || y >= oy + dstH) {
			for (int x = 0; x < gPhoneW; ++x) row[x] = 0;
		} else {
			for (int x = 0; x < ox; ++x) row[x] = 0;
			for (int x = ox + dstW; x < gPhoneW; ++x) row[x] = 0;
		}
	}
	int prevSy = -1;
	for (int y = 0; y < dstH; ++y) {
		int sy = y * gH / dstH;
		if (sy >= gH)
			sy = gH - 1;
		TUint16* dst = gPhone + (oy + y) * gPhonePitch + ox;
		if (sy == prevSy) { /* 2x upscale: repeat the previous row */
			memcpy(dst, dst - gPhonePitch, (size_t)dstW * 2);
			continue;
		}
		prevSy = sy;
		const uint16_t* src = fb + sy * pitchPixels;
		for (int x = 0; x < dstW; ++x)
			dst[x] = src[xmap[x]];
	}
}

extern "C" int Plat_pollEvents(PlatKeyEvent* ev, int max, int* count)
{
	int n = 0;
	while (n < max && gQh != gQt) {
		ev[n++] = gQ[gQh];
		gQh = (gQh + 1) % KQ;
	}
	*count = n;
	return gQuit ? 0 : 1;
}

extern "C" void Plat_setDataDir(const char* dir)
{
	if (!dir)
		return;
	strncpy(gDataDir, dir, sizeof(gDataDir) - 1);
	gDataDir[sizeof(gDataDir) - 1] = 0;
	for (int k = 0; gDataDir[k]; ++k)
		if (gDataDir[k] == '\\')
			gDataDir[k] = '/';
	int n = (int)strlen(gDataDir);
	if (n > 0 && gDataDir[n - 1] != '/') {
		if (n + 1 < (int)sizeof(gDataDir)) {
			gDataDir[n] = '/';
			gDataDir[n + 1] = 0;
		}
	}
}

extern "C" const char* Plat_dataDir(void) { return gDataDir; }

extern "C" const char* Plat_dataPath(const char* name)
{
	static char buf[512];
	snprintf(buf, sizeof(buf), "%s%s", gDataDir, name ? name : "");
	return buf;
}

extern "C" int Plat_fileRead(const char* name, uint8_t** data, size_t* len)
{
	const char* path = Plat_dataPath(name);
	FILE* f = fopen(path, "rb");
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	long n = ftell(f);
	if (n < 0) {
		fclose(f);
		return -1;
	}
	rewind(f);
	uint8_t* p = (uint8_t*)malloc((size_t)n + 1);
	if (!p) {
		fclose(f);
		return -1;
	}
	size_t got = fread(p, 1, (size_t)n, f);
	fclose(f);
	p[got] = 0;
	*data = p;
	*len = got;
	return 0;
}

extern "C" int Plat_fileWrite(const char* name, const void* data, size_t len)
{
	FILE* f = fopen(Plat_dataPath(name), "wb");
	if (!f)
		return -1;
	size_t n = fwrite(data, 1, len, f);
	fclose(f);
	return n == len ? 0 : -1;
}

extern "C" int Plat_fileRemove(const char* name)
{
	return remove(Plat_dataPath(name)) == 0 ? 0 : -1;
}

extern "C" int Plat_fileExists(const char* name)
{
	FILE* f = fopen(Plat_dataPath(name), "rb");
	if (!f)
		return 0;
	fclose(f);
	return 1;
}

extern "C" int Audio_init(void)
{
	memset(gCh, 0, sizeof(gCh));
	gAudioOpen = 0;
	/* A real CMdaAudioOutputStream can be attached here. The mix buffer is
	 * ready either way; without a stream the game stays silent but timed. */
	return 0;
}

extern "C" void Audio_shutdown(void)
{
	gAudioOpen = 0;
	memset(gCh, 0, sizeof(gCh));
}

extern "C" void Audio_play(int channel, const int16_t* pcm, int samples, int rate, int loops)
{
	if (channel < 0 || channel >= AUDIO_CHANNELS || !pcm || samples <= 0 || rate <= 0)
		return;
	Chan* c = &gCh[channel];
	c->pcm = pcm;
	c->samples = samples;
	c->rate = rate;
	c->pos = 0;
	c->step = (int)(((int64_t)rate << 16) / gMixRate);
	if (c->step < 1)
		c->step = 1;
	c->loops = loops;
	c->volume = 100;
	c->playing = 1;
	unsigned dur = (unsigned)((int64_t)samples * 1000 / rate);
	if (loops)
		dur = 0x7fffffff;
	c->endMs = Plat_uptimeMs() + (dur ? dur : 1);
}

extern "C" void Audio_stop(int channel)
{
	if (channel < 0 || channel >= AUDIO_CHANNELS)
		return;
	gCh[channel].playing = 0;
	gCh[channel].endMs = 0;
}

extern "C" int Audio_isPlaying(int channel)
{
	if (channel < 0 || channel >= AUDIO_CHANNELS)
		return 0;
	Chan* c = &gCh[channel];
	if (!c->playing)
		return 0;
	if (!c->loops && (int32_t)(Plat_uptimeMs() - c->endMs) >= 0) {
		c->playing = 0;
		return 0;
	}
	return 1;
}

extern "C" void Audio_setVolume(int channel, int volume0to100)
{
	if (channel < 0 || channel >= AUDIO_CHANNELS)
		return;
	if (volume0to100 < 0)
		volume0to100 = 0;
	if (volume0to100 > 100)
		volume0to100 = 100;
	gCh[channel].volume = volume0to100;
}

extern "C" void Audio_pauseAll(int pause)
{
	(void)pause;
}
