/*
 * Orcs & Elves II — BREW engine reimplementation.
 * Host loop: plays the role of the BREW shell around the applet. Keys become
 * EVT_KEY events and the applet's 40 ms timer is re-armed after every tick,
 * exactly as the phone did (Applet_Tick calls ISHELL_SetTimer(40) at its end).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

#define TICK_MS 40
#define FAST_TICK_MS 16 /* --fast: about one tick per 60 Hz frame (what vsync paced it at before) */

/* Held keys repeat the way the BREW shell repeated them, not at the desktop's rate: the applet sees one
 * EVT_KEY on press, then (if still held) one every KEY_REPEAT_RATE ms after KEY_REPEAT_START ms. Only the
 * most recently pressed key repeats. The values are the commonly cited BREW shell defaults; handsets
 * could differ and they are not confirmed from an SDK header. */
#define KEY_REPEAT_START 1500
#define KEY_REPEAT_RATE  100
#define KB_AUTOREPEAT    0x40 /* dwParam flag on repeated EVT_KEYs; the applet ignores dwParam */

/* ---- headless test hooks (developer use) ----
 * OE2_TICKS=N             quit after N ticks
 * OE2_KEYS=t:k,t:k,...    press BREW key k (hex AVK or name: up,down,left,right,select,soft1,soft2,clr,
 *                         star,pound,0..9) at tick t
 * OE2_SHOT=dir            write dir/tickNNNNN.ppm at every tick listed in OE2_SHOT_AT (t,t,...) and at exit
 * OE2_VCLOCK=1            virtual clock: 40 ms of game time per tick, no sleeping (deterministic timing)
 * OE2_PIPE=1              lock-step control over stdin/stdout for test bots (implies OE2_VCLOCK), see pipe_tick
 * OE2_SEED=N              fixed GETRAND seed (brew_shim.c); OE2_KEYLOG=1 logs keys (canvas_input.c) */
typedef struct { int tick, key; } ScriptKey;
static ScriptKey scriptKeys[4096];
static int nScriptKeys;
static int shotTicks[256], nShotTicks;

static int key_by_name(const char *s)
{
	static const struct { const char *n; int k; } t[] = {
		{"up", AVK_UP}, {"down", AVK_DOWN}, {"left", AVK_LEFT}, {"right", AVK_RIGHT}, {"select", AVK_SELECT},
		{"soft1", AVK_SOFT1}, {"soft2", AVK_SOFT2}, {"clr", AVK_CLR}, {"star", AVK_STAR}, {"pound", AVK_POUND},
	};
	for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++)
		if (!strcmp(s, t[i].n))
			return t[i].k;
	if (s[0] >= '0' && s[0] <= '9' && s[1] == 0)
		return AVK_0 + (s[0] - '0');
	return (int)strtol(s, NULL, 16);
}

static void parse_hooks(void)
{
	const char *k = getenv("OE2_KEYS"), *a = getenv("OE2_SHOT_AT");
	static char buf[65536];
	char *tok, *save;
	if (k) {
		snprintf(buf, sizeof(buf), "%s", k);
		for (tok = strtok_r(buf, ",", &save); tok && nScriptKeys < 4096; tok = strtok_r(NULL, ",", &save)) {
			char *c = strchr(tok, ':');
			if (!c)
				continue;
			*c = 0;
			scriptKeys[nScriptKeys].tick = atoi(tok);
			scriptKeys[nScriptKeys].key = key_by_name(c + 1);
			nScriptKeys++;
		}
	}
	if (a) {
		snprintf(buf, sizeof(buf), "%s", a);
		for (tok = strtok_r(buf, ",", &save); tok && nShotTicks < 256; tok = strtok_r(NULL, ",", &save))
			shotTicks[nShotTicks++] = atoi(tok);
	}
}

static void write_ppm(const char *path)
{
	IBitmap *scr = Brew_screen();
	FILE *f;
	if (!scr)
		return;
	f = fopen(path, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", scr->width, scr->height);
	for (int y = 0; y < scr->height; y++)
		for (int x = 0; x < scr->width; x++) {
			unsigned c = scr->px[y * scr->pitch + x];
			unsigned char rgb[3] = { (unsigned char)(((c >> 11) & 31) * 255 / 31),
			                         (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31) };
			fwrite(rgb, 1, 3, f);
		}
	fclose(f);
}

static void write_shot(int tick)
{
	const char *dir = getenv("OE2_SHOT");
	char path[1024];
	if (!dir)
		return;
	snprintf(path, sizeof(path), "%s/tick%05d.ppm", dir, tick);
	write_ppm(path);
}

/* OE2_PIPE: before every tick, print one status line on stdout, then read one command line from stdin and
 * run it; then the tick runs. One input line = one tick. Status line (one line):
 *   S <tick> st <Canvas.state> map <mapId> x <tileX> y <tileY> ang <destAngle 0..1023, 0=east 256=north>
 *   hp <hp>/<max> xp <xp> lvl <level> gold <gold> menu <MenuSystem.menu> sel <selectedIndex>
 *   nitems <numItems> fam <isFamiliar> lock <lockSlot> <lockCorrectHits + 10*lockDone>
 *   face '<name of the faced entity's def>' msg '<first HUD message>' w <world_hash, 8 hex digits>
 * Commands: empty line = just tick; key names as in OE2_KEYS separated by spaces (pressed in order);
 * "shot PATH" (PPM of the current screen); "god" (Player.god = 1); "noclip" (toggle Player.noclip);
 * "tp X Y" (move the player to tile X,Y keeping the angle); "quit" or EOF ends the run.
 * Developer/QA use only (re/tools/qa/drv.py); the debug commands only set existing engine fields. */
/* FNV-1a over the state the game rules act on: the random pool, every sprite's position and flags, and every
 * entity's info, parameter, tile link and health. One changed draw, step or hit anywhere changes it, which is
 * what the golden traces (re/tools/qa/golden.py) pin down. Pointers are left out (they differ per run). */
static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
	const uint8_t *b = p;
	while (n--)
		h = (h ^ *b++) * 16777619u;
	return h;
}

static uint32_t world_hash(void)
{
	Applet *a = g_app;
	Render *r = a->render;
	Game *g = a->game;
	uint32_t h = 2166136261u;
	h = fnv(h, a->random.pool, sizeof(a->random.pool));
	h = fnv(h, &a->random.index, sizeof(a->random.index));
	for (int i = 0; i < r->numSprites && i < 800; i++) {
		const Sprite *s = &r->sprites[i];
		int32_t v[4] = { s->x, s->y, s->z, (int32_t)s->info };
		h = fnv(h, v, sizeof(v));
	}
	for (int i = 0; i < g->numEntities && i < 450; i++) {
		const Entity *e = &g->entities[i];
		int32_t v[4] = { (int32_t)e->info, e->param, e->linkIndex,
		                 e->monster ? CombatEntity_getHealth(&e->monster->ce) : 0 };
		h = fnv(h, v, sizeof(v));
	}
	return h;
}

static void pipe_status(int tick)
{
	Applet *a = g_app;
	Canvas *c = a->canvas;
	Hud *h = a->hud;
	Player *p = a->player;
	Entity *f = p->facingEntity;
	printf("S %d st %d map %d x %d y %d ang %d hp %d/%d xp %d lvl %d gold %d menu %d sel %d nitems %d fam %d "
	       "lock %d %d face '%.*s' msg '%.*s' w %08x\n",
	       tick, c->state, c->mapId, c->destX >> 6, c->destY >> 6, c->destAngle & 0x3ff,
	       CombatEntity_getHealth(&p->ce), CombatEntity_getMaxHealth(&p->ce), p->currentXP, p->level, p->gold,
	       a->menuSystem->menu, a->menuSystem->selectedIndex, a->menuSystem->numItems, p->isFamiliar,
	       c->lockSlot, c->lockCorrectHits + 10 * c->lockDone,
	       f && f->def ? f->def->name.len : 0, f && f->def ? f->def->name.buf : "",
	       h->numMessages > 0 ? h->messages[0].len : 0, h->numMessages > 0 ? h->messages[0].buf : "",
	       world_hash());
	fflush(stdout);
}

static int pipe_tick(int tick)
{
	char line[4096], *tok, *save;
	pipe_status(tick);
	if (!fgets(line, sizeof(line), stdin))
		return 0;
	line[strcspn(line, "\r\n")] = 0;
	if (!strcmp(line, "quit"))
		return 0;
	if (!strcmp(line, "god")) {
		g_app->player->god = 1;
	} else if (!strcmp(line, "noclip")) {
		g_app->player->noclip = !g_app->player->noclip;
	} else if (!strncmp(line, "shot ", 5)) {
		write_ppm(line + 5);
	} else if (!strncmp(line, "tp ", 3)) {
		Canvas *cv = g_app->canvas;
		int tx, ty;
		if (sscanf(line + 3, "%d %d", &tx, &ty) == 2) {
			cv->viewX = cv->destX = tx * 64 + 32;
			cv->viewY = cv->destY = ty * 64 + 32;
			cv->viewAngle = cv->destAngle;
			Game_unlinkEntityById(g_app->player);
			Canvas_finishRotation(cv, 1);
			Game_relinkEntity(g_app->player);
		}
	} else {
		for (tok = strtok_r(line, " ", &save); tok; tok = strtok_r(NULL, " ", &save))
			Applet_HandleEvent(g_app, EVT_KEY, key_by_name(tok), 0);
	}
	return 1;
}

static void usage(const char *argv0)
{
	printf("usage: %s [--data DIR] [--screen WxH] [--fast]\n"
	       "  --data DIR     folder holding orcsandelves2.bar and orcsandelves2.mod (default: gamedata)\n"
	       "  --screen WxH   phone screen size (default 176x220)\n"
	       "  --fast         run a tick every 16 ms (about one per frame) instead of the phone's 40 ms timer\n",
	       argv0);
}

int main(int argc, char **argv)
{
	const char *data = "gamedata";
	int w = 176, h = 220, fast = 0;
	Applet *app;
	uint32_t nextTick;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--data") && i + 1 < argc) {
			data = argv[++i];
		} else if (!strcmp(argv[i], "--screen") && i + 1 < argc) {
			if (sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w < 128 || h < 128 || w > 640 || h > 640) {
				usage(argv[0]);
				return 2;
			}
		} else if (!strcmp(argv[i], "--fast")) {
			fast = 1;
		} else {
			usage(argv[0]);
			return strcmp(argv[i], "--help") ? 2 : 0;
		}
	}

	if (Brew_init(data, w, h) != 0) {
		fprintf(stderr, "Put orcsandelves2.bar and orcsandelves2.mod in %s/ (see %s/README.txt).\n", data, data);
		return 1;
	}
	if (Plat_init(w, h, "Orcs & Elves II") != 0)
		return 1;
	Audio_init();

	app = App_create(w, h);
	if (!app) {
		fprintf(stderr, "Engine startup failed.\n");
		Plat_shutdown();
		Brew_shutdown();
		return 1;
	}
	Applet_HandleEvent(app, EVT_APP_START, 0, 0);

	parse_hooks();
	int tick = 0, maxTicks = getenv("OE2_TICKS") ? atoi(getenv("OE2_TICKS")) : -1;
	int pipeMode = getenv("OE2_PIPE") != NULL;
	int vclock = pipeMode || getenv("OE2_VCLOCK") != NULL; /* 40 ms of game time per tick, no sleeping */
	if (vclock)
		Plat_setVirtualClock(1, 1000);
	nextTick = Plat_uptimeMs();
	int heldKey = 0, suspended = 0;
	uint32_t nextRepeat = 0;
	for (;;) {
		PlatKeyEvent ev[32];
		int n;
		if (!Plat_pollEvents(ev, 32, &n))
			break;
		/* A minimised window is the PC's version of the phone interrupting the app (call, flip closed):
		 * the shell sent EVT_APP_SUSPEND, delivered no keys while suspended, then EVT_APP_RESUME. */
		if (Plat_isMinimized() != suspended) {
			suspended = !suspended;
			heldKey = 0;
			Applet_HandleEvent(app, suspended ? EVT_APP_SUSPEND : EVT_APP_RESUME, 0, 0);
		}
		for (int i = 0; i < n && !suspended; i++) {
			if (ev[i].repeat)
				continue; /* the desktop's auto-repeat; the shell's is emulated below */
			if (ev[i].down) {
				Applet_HandleEvent(app, EVT_KEY, ev[i].avk, 0);
				heldKey = ev[i].avk;
				nextRepeat = Plat_uptimeMs() + KEY_REPEAT_START;
			} else if (ev[i].avk == heldKey) {
				heldKey = 0;
			}
		}
		if (heldKey && (int32_t)(Plat_uptimeMs() - nextRepeat) >= 0) {
			/* one repeat per due time; after a stall (map load, save) the shell did not send a burst */
			Applet_HandleEvent(app, EVT_KEY, heldKey, KB_AUTOREPEAT);
			nextRepeat += KEY_REPEAT_RATE;
			if ((int32_t)(Plat_uptimeMs() - nextRepeat) >= 0)
				nextRepeat = Plat_uptimeMs() + KEY_REPEAT_RATE;
		}
		if (Brew_quitRequested())
			break;
		if (vclock || (int32_t)(Plat_uptimeMs() - nextTick) >= 0) {
			if (vclock)
				Plat_setVirtualClock(1, 1000 + (uint32_t)tick * TICK_MS);
			if (pipeMode && !pipe_tick(tick))
				break;
			for (int i = 0; i < nScriptKeys; i++)
				if (scriptKeys[i].tick == tick)
					Applet_HandleEvent(app, EVT_KEY, scriptKeys[i].key, 0);
			Applet_Tick(app);
			/* Applet_Tick re-armed the 40 ms timer at its end (ISHELL_SetTimer); the phone's deferred screen
			 * update happened inside those 40 ms, so presenting must not delay the next tick. */
			nextTick = Plat_uptimeMs() + (fast ? FAST_TICK_MS : TICK_MS);
			for (int i = 0; i < nShotTicks; i++)
				if (shotTicks[i] == tick)
					write_shot(tick);
			if (++tick == maxTicks) {
				write_shot(tick);
				break;
			}
		} else {
			Plat_delay(1);
		}
	}

	Applet_HandleEvent(app, EVT_APP_STOP, 0, 0);
	App_destroy(app);
	Plat_shutdown();
	Brew_shutdown();
	return 0;
}
