/* BREW runtime subset used by Orcs & Elves II, over platform.h. See brew_shim.h. */
#include "brew_shim.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bar.h"
#include "moddata.h"

static IBitmap *screen;
static Bar bar;
static int barOpen;

/* ---------------------------------------------------------------- colour */

static NativeColor rgb_to_565(unsigned r, unsigned g, unsigned b)
{
	return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}

NativeColor IBITMAP_RGBToNative(IBitmap *b, RGBVAL rgb)
{
	(void)b;
	return rgb_to_565((rgb >> 8) & 0xff, (rgb >> 16) & 0xff, (rgb >> 24) & 0xff);
}

RGBVAL IBITMAP_NativeToRGB(IBitmap *b, NativeColor c)
{
	unsigned r = (c >> 11) & 0x1f, g = (c >> 5) & 0x3f, bl = c & 0x1f;
	(void)b;
	return MAKE_RGB((r << 3) | (r >> 2), (g << 2) | (g >> 4), (bl << 3) | (bl >> 2));
}

/* ---------------------------------------------------------------- bitmaps */

IBitmap *IBITMAP_Create(int w, int h)
{
	IBitmap *b = calloc(1, sizeof(*b));
	if (!b)
		return NULL;
	b->px = calloc((size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1), sizeof(uint16_t));
	if (!b->px) {
		free(b);
		return NULL;
	}
	b->width = w;
	b->height = h;
	b->pitch = w;
	b->ownsPixels = 1;
	b->transparent = 0xf81f; /* magenta, the colour key used by the game's art */
	return b;
}

void IBITMAP_Release(IBitmap *b)
{
	if (!b || b == screen)
		return;
	if (b->ownsPixels)
		free(b->px);
	free(b);
}

static int clip_rect(int *x, int *y, int *w, int *h, int bw, int bh)
{
	if (*x < 0) { *w += *x; *x = 0; }
	if (*y < 0) { *h += *y; *y = 0; }
	if (*x + *w > bw) *w = bw - *x;
	if (*y + *h > bh) *h = bh - *y;
	return *w > 0 && *h > 0;
}

int IBITMAP_FillRect(IBitmap *b, const AEERect *rc, NativeColor c, int rop)
{
	int x = rc->x, y = rc->y, w = rc->dx, h = rc->dy;
	/* FillRect with AEE_RO_TRANSPARENT is BREW's IBitmap_Invalidate: mark dirty, draw nothing. */
	if (rop == AEE_RO_TRANSPARENT)
		return SUCCESS;
	if (!b || !clip_rect(&x, &y, &w, &h, b->width, b->height))
		return SUCCESS;
	for (int j = 0; j < h; j++) {
		uint16_t *p = b->px + (y + j) * b->pitch + x;
		for (int i = 0; i < w; i++)
			p[i] = (uint16_t)c;
	}
	return SUCCESS;
}

int IBITMAP_BltIn(IBitmap *dst, int xDst, int yDst, int dx, int dy, IBitmap *src, int xSrc, int ySrc, int rop)
{
	int i, j;
	if (!dst || !src)
		return EFAILED;
	/* clip against the source */
	if (xSrc < 0) { xDst -= xSrc; dx += xSrc; xSrc = 0; }
	if (ySrc < 0) { yDst -= ySrc; dy += ySrc; ySrc = 0; }
	if (xSrc + dx > src->width) dx = src->width - xSrc;
	if (ySrc + dy > src->height) dy = src->height - ySrc;
	/* clip against the destination */
	if (xDst < 0) { xSrc -= xDst; dx += xDst; xDst = 0; }
	if (yDst < 0) { ySrc -= yDst; dy += yDst; yDst = 0; }
	if (xDst + dx > dst->width) dx = dst->width - xDst;
	if (yDst + dy > dst->height) dy = dst->height - yDst;
	if (dx <= 0 || dy <= 0)
		return SUCCESS;
	for (j = 0; j < dy; j++) {
		const uint16_t *s = src->px + (ySrc + j) * src->pitch + xSrc;
		uint16_t *d = dst->px + (yDst + j) * dst->pitch + xDst;
		if (rop == AEE_RO_TRANSPARENT) {
			uint16_t key = (uint16_t)src->transparent;
			for (i = 0; i < dx; i++)
				if (s[i] != key)
					d[i] = s[i];
		} else {
			memmove(d, s, (size_t)dx * sizeof(uint16_t));
		}
	}
	return SUCCESS;
}

int IBITMAP_SetTransparencyColor(IBitmap *b, NativeColor c)
{
	if (b)
		b->transparent = c;
	return SUCCESS;
}

int IBITMAP_GetInfo(IBitmap *b, AEEBitmapInfo *info)
{
	if (!b || !info)
		return EFAILED;
	info->cx = (uint32_t)b->width;
	info->cy = (uint32_t)b->height;
	info->nDepth = 16;
	return SUCCESS;
}

int IBITMAP_CreateCompatibleBitmap(IBitmap *b, IBitmap **out, int w, int h)
{
	(void)b;
	*out = IBITMAP_Create(w, h);
	return *out ? SUCCESS : EFAILED;
}

/* ---------------------------------------------------------------- screen */

void Brew_initScreen(int w, int h)
{
	if (screen) {
		screen->ownsPixels = 1;
		free(screen->px);
		free(screen);
	}
	screen = IBITMAP_Create(w, h);
}

IBitmap *Brew_screen(void) { return screen; }

void IDISPLAY_Update(void)
{
	if (screen)
		Plat_present(screen->px, screen->pitch);
}

void IDISPLAY_Backlight(int on) { (void)on; }

void IDISPLAY_ClearScreen(RGBVAL rgb)
{
	AEERect rc = { 0, 0, 0, 0 };
	if (!screen)
		return;
	rc.dx = (int16_t)screen->width;
	rc.dy = (int16_t)screen->height;
	IBITMAP_FillRect(screen, &rc, IBITMAP_RGBToNative(screen, rgb), AEE_RO_COPY);
}

/* ---------------------------------------------------------------- IGraphics */

IGraphics *IGRAPHICS_Create(IBitmap *dst)
{
	IGraphics *g = calloc(1, sizeof(*g));
	if (!g)
		return NULL;
	g->dst = dst;
	g->alpha = g->fillAlpha = 255;
	g->clip.dx = (int16_t)(dst ? dst->width : 0);
	g->clip.dy = (int16_t)(dst ? dst->height : 0);
	return g;
}

void IGRAPHICS_Release(IGraphics *g) { free(g); }

RGBVAL IGRAPHICS_SetBackground(IGraphics *g, uint8_t r, uint8_t gr, uint8_t b)
{
	g->background = rgb_to_565(r, gr, b);
	return MAKE_RGB(r, gr, b);
}

RGBVAL IGRAPHICS_SetColor(IGraphics *g, uint8_t r, uint8_t gr, uint8_t b, uint8_t a)
{
	g->color = rgb_to_565(r, gr, b);
	g->alpha = a;
	return MAKE_RGB(r, gr, b);
}

RGBVAL IGRAPHICS_SetFillColor(IGraphics *g, uint8_t r, uint8_t gr, uint8_t b, uint8_t a)
{
	g->fillColor = rgb_to_565(r, gr, b);
	g->fillAlpha = a;
	return MAKE_RGB(r, gr, b);
}

boolean IGRAPHICS_SetFillMode(IGraphics *g, boolean fill)
{
	boolean old = g->fill;
	g->fill = fill ? 1 : 0;
	return old;
}

boolean IGRAPHICS_SetClip(IGraphics *g, const AEEClip *clip, uint8_t flags)
{
	(void)flags;
	if (!clip || clip->type == CLIPPING_NONE || !g->dst) {
		g->clip.x = g->clip.y = 0;
		g->clip.dx = (int16_t)(g->dst ? g->dst->width : 0);
		g->clip.dy = (int16_t)(g->dst ? g->dst->height : 0);
	} else {
		g->clip = clip->rect;
	}
	return TRUE;
}

void IGRAPHICS_Translate(IGraphics *g, int16_t x, int16_t y)
{
	g->tx = (int16_t)(g->tx + x);
	g->ty = (int16_t)(g->ty + y);
}

static void g_pixel(IGraphics *g, int x, int y, NativeColor c)
{
	if (x < g->clip.x || y < g->clip.y || x >= g->clip.x + g->clip.dx || y >= g->clip.y + g->clip.dy)
		return;
	if (x < 0 || y < 0 || x >= g->dst->width || y >= g->dst->height)
		return;
	g->dst->px[y * g->dst->pitch + x] = (uint16_t)c;
}

static void g_hspan(IGraphics *g, int x0, int x1, int y, NativeColor c)
{
	int cx0 = g->clip.x, cx1 = g->clip.x + g->clip.dx - 1;
	if (y < g->clip.y || y >= g->clip.y + g->clip.dy || y < 0 || y >= g->dst->height)
		return;
	if (cx0 < 0) cx0 = 0;
	if (cx1 > g->dst->width - 1) cx1 = g->dst->width - 1;
	if (x0 < cx0) x0 = cx0;
	if (x1 > cx1) x1 = cx1;
	for (int x = x0; x <= x1; x++)
		g->dst->px[y * g->dst->pitch + x] = (uint16_t)c;
}

int IGRAPHICS_DrawLine(IGraphics *g, const AEELine *ln)
{
	int x0 = ln->sx + g->tx, y0 = ln->sy + g->ty, x1 = ln->ex + g->tx, y1 = ln->ey + g->ty;
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	if (!g->dst)
		return EFAILED;
	for (;;) {
		g_pixel(g, x0, y0, g->color);
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
	return SUCCESS;
}

int IGRAPHICS_DrawRect(IGraphics *g, const AEERect *rc)
{
	int x = rc->x + g->tx, y = rc->y + g->ty, w = rc->dx, h = rc->dy;
	if (!g->dst || w <= 0 || h <= 0)
		return SUCCESS;
	if (g->fill)
		for (int j = y + 1; j < y + h - 1; j++)
			g_hspan(g, x + 1, x + w - 2, j, g->fillColor);
	g_hspan(g, x, x + w - 1, y, g->color);
	g_hspan(g, x, x + w - 1, y + h - 1, g->color);
	for (int j = y; j < y + h; j++) {
		g_pixel(g, x, j, g->color);
		g_pixel(g, x + w - 1, j, g->color);
	}
	return SUCCESS;
}

int IGRAPHICS_DrawCircle(IGraphics *g, const AEECircle *c)
{
	int cx = c->cx + g->tx, cy = c->cy + g->ty, r = c->r;
	int x = r, y = 0, err = 1 - r;
	if (!g->dst || r < 0)
		return SUCCESS;
	while (x >= y) {
		if (g->fill) {
			g_hspan(g, cx - x, cx + x, cy + y, g->fillColor);
			g_hspan(g, cx - x, cx + x, cy - y, g->fillColor);
			g_hspan(g, cx - y, cx + y, cy + x, g->fillColor);
			g_hspan(g, cx - y, cx + y, cy - x, g->fillColor);
		}
		g_pixel(g, cx + x, cy + y, g->color); g_pixel(g, cx - x, cy + y, g->color);
		g_pixel(g, cx + x, cy - y, g->color); g_pixel(g, cx - x, cy - y, g->color);
		g_pixel(g, cx + y, cy + x, g->color); g_pixel(g, cx - y, cy + x, g->color);
		g_pixel(g, cx + y, cy - x, g->color); g_pixel(g, cx - y, cy - x, g->color);
		y++;
		if (err < 0) {
			err += 2 * y + 1;
		} else {
			x--;
			err += 2 * (y - x) + 1;
		}
	}
	return SUCCESS;
}

int IGRAPHICS_ClearRect(IGraphics *g, const AEERect *rc)
{
	int x = rc->x + g->tx, y = rc->y + g->ty;
	if (!g->dst)
		return EFAILED;
	for (int j = y; j < y + rc->dy; j++)
		g_hspan(g, x, x + rc->dx - 1, j, g->background);
	return SUCCESS;
}

void IGRAPHICS_Update(IGraphics *g) { (void)g; IDISPLAY_Update(); }

/* ---------------------------------------------------------------- files */

struct IFile {
	FILE *f;
};

static IFileMgr fileMgr;

IFileMgr *IFILEMGR_Get(void) { return &fileMgr; }

IFile *IFILEMGR_OpenFile(IFileMgr *m, const char *name, int mode)
{
	const char *fm = "rb";
	IFile *file;
	FILE *f;
	if (mode & _OFM_CREATE) {
		if (Plat_fileExists(name)) { /* BREW refuses to create over an existing file */
			m->lastError = EFILEEXISTS;
			return NULL;
		}
		fm = "w+b";
	} else if (mode & (_OFM_READWRITE | _OFM_APPEND))
		fm = "r+b";
	f = fopen(Plat_dataPath(name), fm);
	if (!f) {
		m->lastError = (mode & _OFM_CREATE) ? EFAILED : EFILENOEXISTS;
		return NULL;
	}
	if (mode & _OFM_APPEND)
		fseek(f, 0, SEEK_END);
	file = calloc(1, sizeof(*file));
	if (!file) {
		fclose(f);
		m->lastError = EFAILED;
		return NULL;
	}
	file->f = f;
	m->lastError = SUCCESS;
	return file;
}

int IFILEMGR_Remove(IFileMgr *m, const char *name)
{
	m->lastError = Plat_fileRemove(name) == 0 ? SUCCESS : EFILENOEXISTS;
	return m->lastError;
}

int IFILEMGR_Test(IFileMgr *m, const char *name)
{
	(void)m;
	return Plat_fileExists(name) ? SUCCESS : EFILENOEXISTS;
}

int IFILEMGR_GetLastError(IFileMgr *m) { return m->lastError; }

uint32_t IFILEMGR_GetFreeSpace(IFileMgr *m, uint32_t *total)
{
	(void)m;
	if (total)
		*total = 64u << 20;
	return 32u << 20;
}

int32_t IFILE_Read(IFile *f, void *buf, uint32_t n) { return f ? (int32_t)fread(buf, 1, n, f->f) : 0; }
/* BREW wrote to flash synchronously and a failed write set the IFileMgr's last error, which OutputStream_flush
 * stores in os->error (the save code then stops). stdio buffers, so flush here to see a full disk at once. */
uint32_t IFILE_Write(IFile *f, const void *buf, uint32_t n)
{
	size_t w;
	if (!f)
		return 0;
	errno = 0;
	w = fwrite(buf, 1, n, f->f);
	if (w == n && fflush(f->f) != 0)
		w = 0;
	if (w < n) {
		fileMgr.lastError = errno == ENOSPC ? EFSFULL : EFAILED;
		w = 0; /* the game only treats 0 as a failed write: a partial one must stop the save too */
	}
	return (uint32_t)w;
}

int32_t IFILE_Size(IFile *f)
{
	long cur, end;
	if (!f)
		return 0;
	cur = ftell(f->f);
	fseek(f->f, 0, SEEK_END);
	end = ftell(f->f);
	fseek(f->f, cur, SEEK_SET);
	return (int32_t)end;
}

void IFILE_Release(IFile *f)
{
	if (!f)
		return;
	fclose(f->f);
	free(f);
}

/* ---------------------------------------------------------------- resources */

typedef struct ResCache {
	int id;
	uint8_t *data;
	size_t size;
} ResCache;

static ResCache *cache;
static int cacheCount, cacheCap;

const uint8_t *Brew_loadResource(int resId, size_t *size)
{
	uint8_t *d;
	size_t n = 0;
	for (int i = 0; i < cacheCount; i++)
		if (cache[i].id == resId) {
			if (size)
				*size = cache[i].size;
			return cache[i].data;
		}
	if (!barOpen)
		return NULL;
	d = Bar_load(&bar, resId, &n);
	if (!d)
		return NULL;
	if (cacheCount == cacheCap) {
		int cap = cacheCap ? cacheCap * 2 : 64;
		ResCache *nc = realloc(cache, (size_t)cap * sizeof(*nc));
		if (!nc) {
			free(d);
			return NULL;
		}
		cache = nc;
		cacheCap = cap;
	}
	cache[cacheCount].id = resId;
	cache[cacheCount].data = d;
	cache[cacheCount].size = n;
	cacheCount++;
	if (size)
		*size = n;
	return d;
}

/* ---------------------------------------------------------------- misc */

uint32_t GETUPTIMEMS(void) { return Plat_uptimeMs(); }

void GETRAND(uint8_t *buf, int n)
{
	/* BREW's GETRAND is a hardware/OS source; any good PRNG is equivalent here.
	 * OE2_SEED=N (developer use) seeds it with a fixed value so headless runs are reproducible. */
	static uint64_t s = 0;
	if (!s) {
		const char *seed = getenv("OE2_SEED");
		if (seed)
			s = 0x9E3779B97F4A7C15ull ^ strtoull(seed, NULL, 0);
		else
			s = 0x9E3779B97F4A7C15ull ^ ((uint64_t)Plat_uptimeMs() << 17) ^ (uint64_t)(uintptr_t)&s;
		if (!s)
			s = 0x9E3779B97F4A7C15ull;
	}
	for (int i = 0; i < n; i++) {
		s ^= s << 13;
		s ^= s >> 7;
		s ^= s << 17;
		buf[i] = (uint8_t)(s >> 24);
	}
}

static int quitRequested;
void ISHELL_CloseApplet(void) { quitRequested = 1; }
int Brew_quitRequested(void) { return quitRequested; }

void ISOUND_Vibrate(int ms) { Plat_vibrate(ms); }
void ISOUND_StopVibrate(void) { Plat_vibrate(0); }

/* ---------------------------------------------------------------- lifecycle */

int Brew_init(const char *dataDir, int screenW, int screenH)
{
	char path[2048];
	int r;
	Plat_setDataDir(dataDir);
	snprintf(path, sizeof(path), "%s/orcsandelves2.bar", dataDir);
	if (Bar_open(&bar, path) != 0) {
		fprintf(stderr, "Cannot open %s\n", path);
		return -1;
	}
	barOpen = 1;
	/* The engine assumes every resource exists (the phone's copy was always intact); a damaged or different
	 * .bar would crash or hang later, so refuse it here like the .mod. */
	if (!Bar_isSupported(&bar)) {
		fprintf(stderr, "%s is damaged or not the supported Orcs & Elves II BREW build "
			"(expected %u bytes, CRC32 %08x)\n", path, BAR_EXPECTED_SIZE, BAR_EXPECTED_CRC32);
		return -1;
	}
	snprintf(path, sizeof(path), "%s/orcsandelves2.mod", dataDir);
	r = ModData_open(&g_mod, path);
	if (r != 0) {
		fprintf(stderr, r == -2 ? "%s is not the supported Orcs & Elves II BREW build\n" : "Cannot open %s\n", path);
		return -1;
	}
	Brew_initScreen(screenW, screenH);
	return screen ? 0 : -1;
}

void Brew_shutdown(void)
{
	for (int i = 0; i < cacheCount; i++)
		free(cache[i].data);
	free(cache);
	cache = NULL;
	cacheCount = cacheCap = 0;
	if (barOpen)
		Bar_close(&bar);
	barOpen = 0;
	ModData_close(&g_mod);
	if (screen) {
		free(screen->px);
		free(screen);
		screen = NULL;
	}
}
