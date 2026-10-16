#ifndef OE2_BREW_SHIM_H
#define OE2_BREW_SHIM_H

/*
 * The small part of the BREW runtime that Orcs & Elves II used, re-implemented
 * on top of platform.h. Names follow the BREW SDK macros (IGRAPHICS_DrawRect,
 * IBITMAP_BltIn, ...) so ported code reads like the original calls, but these
 * are plain functions over our own structs: there are no vtables.
 *
 * Pixels are RGB565 everywhere, like the phone's 16-bit display.
 */

#include <stddef.h>
#include <stdint.h>

#include "platform.h"

typedef uint8_t boolean;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* BREW RGBVAL: 0xBBGGRR00 */
typedef uint32_t RGBVAL;
#define MAKE_RGB(r, g, b) (((uint32_t)(b) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8))
typedef uint32_t NativeColor; /* RGB565 in the low 16 bits */

typedef struct AEERect { int16_t x, y, dx, dy; } AEERect;
typedef struct AEELine { int16_t sx, sy, ex, ey; } AEELine;
typedef struct AEECircle { int16_t cx, cy, r; } AEECircle;
typedef struct AEEPoint { int16_t x, y; } AEEPoint;

enum { CLIPPING_NONE, CLIPPING_RECT };
typedef struct AEEClip {
	int8_t type;
	AEERect rect;
} AEEClip;

/* Raster ops used with IBITMAP_BltIn / IDISPLAY_BitBlt */
enum { AEE_RO_OR = 0, AEE_RO_XOR = 1, AEE_RO_COPY = 2, AEE_RO_NOT = 3, AEE_RO_MERGENOT = 4,
       AEE_RO_ANDNOT = 5, AEE_RO_TRANSPARENT = 7 };

/* ---- bitmaps (BREW IBitmap / IDIB, native RGB565) ---- */
typedef struct IBitmap {
	uint16_t *px;
	int width, height, pitch;  /* pitch in pixels */
	NativeColor transparent;   /* colour skipped by AEE_RO_TRANSPARENT */
	int ownsPixels;
} IBitmap;

typedef struct AEEBitmapInfo { uint32_t cx, cy, nDepth; } AEEBitmapInfo;

IBitmap *IBITMAP_Create(int w, int h);                 /* zeroed RGB565 bitmap */
void IBITMAP_Release(IBitmap *b);
NativeColor IBITMAP_RGBToNative(IBitmap *b, RGBVAL rgb);
RGBVAL IBITMAP_NativeToRGB(IBitmap *b, NativeColor c);
int IBITMAP_FillRect(IBitmap *b, const AEERect *rc, NativeColor c, int rop);
int IBITMAP_BltIn(IBitmap *dst, int xDst, int yDst, int dx, int dy, IBitmap *src, int xSrc, int ySrc, int rop);
int IBITMAP_SetTransparencyColor(IBitmap *b, NativeColor c);
int IBITMAP_GetInfo(IBitmap *b, AEEBitmapInfo *info);
int IBITMAP_CreateCompatibleBitmap(IBitmap *b, IBitmap **out, int w, int h);

/* The phone screen. Everything (3D view, HUD, menus) is drawn into this. */
IBitmap *Brew_screen(void);
/* Create/resize the screen; called once at startup. */
void Brew_initScreen(int w, int h);
/* Copy the screen to the window (IDISPLAY_Update). */
void IDISPLAY_Update(void);
void IDISPLAY_Backlight(int on);
/* IDISPLAY_SetColor/DrawRect only exist for the fatal-error screen. */
void IDISPLAY_ClearScreen(RGBVAL rgb);

/* ---- IGraphics: 2D primitives on the screen ---- */
typedef struct IGraphics {
	IBitmap *dst;
	NativeColor color, fillColor, background;
	uint8_t alpha, fillAlpha;
	boolean fill;
	AEERect clip;     /* always valid; defaults to the whole screen */
	int16_t tx, ty;   /* IGRAPHICS_Translate origin */
} IGraphics;

IGraphics *IGRAPHICS_Create(IBitmap *dst);
void IGRAPHICS_Release(IGraphics *g);
RGBVAL IGRAPHICS_SetBackground(IGraphics *g, uint8_t r, uint8_t gr, uint8_t b);
RGBVAL IGRAPHICS_SetColor(IGraphics *g, uint8_t r, uint8_t gr, uint8_t b, uint8_t a);
RGBVAL IGRAPHICS_SetFillColor(IGraphics *g, uint8_t r, uint8_t gr, uint8_t b, uint8_t a);
boolean IGRAPHICS_SetFillMode(IGraphics *g, boolean fill);
boolean IGRAPHICS_SetClip(IGraphics *g, const AEEClip *clip, uint8_t flags);
void IGRAPHICS_Translate(IGraphics *g, int16_t x, int16_t y);
int IGRAPHICS_DrawLine(IGraphics *g, const AEELine *ln);
int IGRAPHICS_DrawRect(IGraphics *g, const AEERect *rc);  /* frame in color, filled with fillColor if fill mode */
int IGRAPHICS_DrawCircle(IGraphics *g, const AEECircle *c);
int IGRAPHICS_ClearRect(IGraphics *g, const AEERect *rc); /* fill with background colour */
void IGRAPHICS_Update(IGraphics *g);

/* ---- files in gamedata/ (BREW IFileMgr / IFile) ---- */
typedef struct IFile IFile;
typedef struct IFileMgr { int lastError; } IFileMgr;
enum { _OFM_READ = 1, _OFM_READWRITE = 2, _OFM_CREATE = 4, _OFM_APPEND = 8 };
enum { SUCCESS = 0, EFAILED = 1, EFILEEXISTS = 0x100, EFILENOEXISTS = 0x101, EFSFULL = 0x106 };
IFileMgr *IFILEMGR_Get(void);
IFile *IFILEMGR_OpenFile(IFileMgr *m, const char *name, int mode);
int IFILEMGR_Remove(IFileMgr *m, const char *name);
int IFILEMGR_Test(IFileMgr *m, const char *name);          /* SUCCESS if it exists */
int IFILEMGR_GetLastError(IFileMgr *m);
uint32_t IFILEMGR_GetFreeSpace(IFileMgr *m, uint32_t *total);
int32_t IFILE_Read(IFile *f, void *buf, uint32_t n);
uint32_t IFILE_Write(IFile *f, const void *buf, uint32_t n);
int32_t IFILE_Size(IFile *f);
void IFILE_Release(IFile *f);

/* ---- resources from orcsandelves2.bar ---- */
/* Decompressed resource bytes (owned by the cache; valid until Brew_shutdown). */
const uint8_t *Brew_loadResource(int resId, size_t *size);

/* ---- misc AEEStdLib ---- */
uint32_t GETUPTIMEMS(void);
void GETRAND(uint8_t *buf, int n);
/* The applet asks the shell to close it (quit from the menu). */
void ISHELL_CloseApplet(void);
int Brew_quitRequested(void);
void ISOUND_Vibrate(int ms);
void ISOUND_StopVibrate(void);

/* BREW objects the original kept pointers to but the port does not need
 * (shell, display, heap, streams, SMS, animated images). Kept opaque so the
 * struct layouts still compile; the port leaves them NULL. */
typedef struct IShell IShell;
typedef struct IDisplay IDisplay;
typedef struct IHeap IHeap;
typedef struct ISound ISound;
typedef struct IAStream IAStream;
typedef struct IBase IBase;
typedef struct IImage IImage;
/* A playing sound (BREW IMedia); implemented by the Sound module. */
typedef struct IMedia IMedia;

/* ---- lifecycle ---- */
int Brew_init(const char *dataDir, int screenW, int screenH); /* opens bar + mod; 0 ok */
void Brew_shutdown(void);

#endif
