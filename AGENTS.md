# oe2-re — guide for agents and contributors

Reimplementation of the **Orcs & Elves II** (BREW, EA/Fountainhead/id, 2007) engine in C17 + SDL2.
The user supplies their own `orcsandelves2.bar` and `orcsandelves2.mod` in `gamedata/`; we ship no game data.
Owner: George. **Commit freely; never `git push`** (George pushes after review). Commits are authored by George;
the README's AI-assistance note is the only credit line. Work in a git worktree.
Sibling project: oe-re (Orcs & Elves, this engine's ancestor; same method, same host layer).

## Where the project is
- **`classic-1.0` (tag) is done**: the faithful port. All ~700 original functions reimplemented and each one
  re-compared with the original in a final review, plus a second pass in 2026-10-04 alongside oe-re's review;
  known differences from the phone, original bugs kept on purpose and deferred items are in
  `re/notes/classic-1.0.md`.
- **`main` is now the enhanced port** (see "Direction" and "Roadmap" below). Game rules stay faithful.
- GitHub: github.com/ygorz/oe2-re, **private** since 2026-10-04 (main, the classic-1.0 tag and its release).
  George makes it public himself; never change its visibility.

## Project rules (approved by George)
- Ship only our engine source, tools and RE notes. Never commit assets, the .mod/.bar, decompiler output
  (re/out, re/ghidra_*, scratch .c dumps) or anything derived from them that reproduces game content.
- The engine is *written*, not pasted from Ghidra; the decompile is a behaviour reference.
- Game data inside the executable (stat tables, store, travel map, UI strings) is read at runtime from the
  user's `.mod` via `src/moddata.c` (size/CRC-checked), never typed into our source.
- `re/types/brew.h` (generated from BREW SDK headers) and short string quotes in `re/notes` may stay tracked.
- No machine-specific paths in tracked files (paths are relative to the repo root).

## Every change must pass
```sh
cmake --build build -j
python3 re/tools/qa/golden.py check          # all three "identical"; see below if not
python3 re/tools/qa/tests/caverns_exit.py    # exit 0
python3 re/tools/qa/tests/save_load.py       # exit 0 (needs: python3 re/tools/qa/mksave.py new, once)
```
Golden traces (`re/tools/qa/golden/`, recorded at classic-1.0) replay seeded playthroughs from boot and compare
every tick's game state plus a world hash (random pool, sprites, entities). Presentation work (resolution,
scaling, audio, input devices) must leave them identical. If a change is *meant* to alter game rules, it needs
a strong reason, George's OK, `golden.py record`, and a commit message that says so. For memory errors, run
the fuzzers under ASan/UBSan (`re/tools/qa/fuzz.py`, build with `-DOE2_SANITIZE=ON` into `build-asan`).

## Direction (George, 2026-10-01)
- Goal: ship for Omarchy Linux and other platforms (AUR / Flatpak style; user supplies .bar + .mod). It must
  clearly beat running the BREW build in an emulator: native install, high-res/scaled UI, controller,
  frame pacing, QoL options.
- One repo, one product: the enhanced port. No Classic mode in the menus; the `classic-1.0` tag is the
  reference for the original behaviour.
- Sort every change into one of three kinds:
  1. Presentation (resolution, UI scaling, audio quality, controller, frame pacing): just do it, no switch.
  2. Feel changes people could disagree on (movement, automap, menu flow): a settings toggle.
  3. Deep rewrites (e.g. the renderer at high res): one version only; the tag is the reference.
  Where enhanced code replaces original behaviour, cite it: `/* orig 0xADDR: what the game did */`.
  Never keep original code as commented-out blocks.
- Game rules (combat, AI, scripts, balance) stay faithful unless there is a strong reason.
- Audio is final: the sinc resampler and the per-sound CMF volume are all it gets. The music is the original
  recorded audio through a phone codec; further "improvement" would change what it was designed to be.

Faithfulness bugs found after the tag. Classify first:
- Port bug (our C does something the original did not): fix on main, then cherry-pick onto a `classic` branch
  made from the tag and tag `classic-1.0.x`.
- Original-game bug (the phone game itself did it): classic keeps it and re/notes/classic-1.0.md documents it;
  the enhanced port may fix it as a kind-2 change with an `orig 0xADDR` citation.
- C undefined behaviour: made safe in both, reproducing what the device actually did.

## Roadmap for the enhanced port
Architecture topics to settle before building (raised 2026-10-01):
1. Where each enhancement lives. Platform-only (controller, window scaling, frame pacing, rebinding) is safe
   and needs no engine change. Engine-touching (3D view resolution, widescreen, QoL that changes logic) needs
   a design first.
2. Original-shaped code vs. modifiable code. The port mirrors the original classes so it can be checked
   against the decompile; enhancements hook in at narrow, named points rather than spreading branches through
   original-shaped functions.
3. Game logic vs. presentation. Logic runs on the fixed 40 ms tick, armed right after Applet_Tick like
   ISHELL_SetTimer (main.c). Smooth high-refresh output (interpolated camera/sprites) must read game state only,
   never write it, so logic stays deterministic; presentation (vsync, a render thread) must never delay the tick.
4. Two render layers. High res means the 3D span renderer takes a resolution parameter while the 2D UI/HUD
   keeps phone coordinates and is scaled and composited on top. The platform seam then becomes
   "scene + overlay" instead of one RGB565 framebuffer (Plat_present).
5. Sharing with oe-re (the O&E 1 sibling). Platform layer and enhancement work (resampler, controller,
   scaling, XDG data dir) could be a shared library, copied deliberately, or kept separate. Decide before the
   second enhancement is ported to both.

Planned work:
- Higher resolution: render the 3D view above phone resolution (like Doom RPG reimplementations' "3D detail"),
  scale the phone-sized 2D UI.
- Controller support (SDL_GameController): map pad buttons to the same BREW keys in platform_sdl.c (all input
  already funnels through map_key -> AVK codes, so it is a platform-only change).
- Packaging (before the AUR package): data location. Today the engine uses ./gamedata (or --data DIR); an
  installed /usr/bin/oe2 needs XDG. Search order: --data DIR, then ./gamedata if it holds the files, then
  $XDG_DATA_HOME/oe2-re (default ~/.local/share/oe2-re). Saves go where the data was found. On a missing
  .bar/.mod, print (and show in a window) exactly where to put them. The PKGBUILD lives in the AUR:
  `oe2-re` builds a release tag, `oe2-re-git` builds main.
- Candidates from the classic notes: fade the end-of-sound clicks in 12 sounds; GIVEMAP revealing its whole
  rectangle; range checks on indices read from save files; atomic saves (write a temp file, then rename);
  vsync on a render thread; key repeat that follows a held key's other PC binding.

Parked (far off, maybe never): an override folder (`gamedata/override/<resId>.<ext>`) for fan remasters; a
cache for expensive derived data (upscaled textures/audio) built on first launch in the XDG cache dir.

## Layout
```
CMakeLists.txt            C17; SDL2, zlib, libavcodec (QCELP) via pkg-config; -fwrapv -funsigned-char (ARM)
gamedata/README.txt       what the user drops in (bar + mod); everything else there is ignored
src/
  main.c                  host loop: keys -> EVT_KEY (BREW-style auto-repeat), 40 ms tick like ISHELL_SetTimer
  platform.h, platform_sdl.c   window/RGB565 present (no vsync), keypad mapping, files, 10-channel sinc mixer
  bar.c/.h                .bar reader (gzip resources; size/CRC-checked at startup)
  pmd.c/.h                CMF .pmd -> PCM (QCELP-13k via libavcodec)
  moddata.c/.h            reads tables from the user's .mod (CRC 1631c7f1)
  game/                   the engine: one C module per original class group
    game.h                single include; g_app replaces GETAPPINSTANCE(); arm_bit()
    types.h, protos.h     GENERATED from re/types/structs.txt and re/names.txt (don't edit)
    brew_shim.c/.h        the BREW calls the game used (IGraphics, IBitmap, IFile, resources...)
  tools/oe2tool.c         list/extract .bar, decode .pmd to WAV, check the .mod
re/
  names.txt               716 functions: addr name | prototype # note (source of protos.h)
  types/structs.txt       offset-annotated structs (source of types.h and Ghidra types)
  notes/                  BRIEFING.md, per-range analysis (r1..r8), structs_log.md,
                          classic-1.0.md (the tag; every known deviation from the phone)
  port/                   PORTING.md (rules for writing engine code), package lists, porters' notes, QA logs
  tools/                  derelocate.py, redecomp.sh, gen_*.py, merge_names.py, port_assign.py, ...
  tools/qa/               bot driver, fuzzers, golden traces, regression tests (README.md there)
  ghidra_scripts/         SetupBrew, ApplyTypes, ExportDecomp (headless)
  (ignored) ghidra_in/ ghidra_proj/ out/ refs/ assets/  — local only
```

## RE pipeline
1. `re/tools/derelocate.py` makes `re/ghidra_in/oe2_abs.mod` (PC-relative literal loads → absolute; base 0).
2. One-time import: `analyzeHeadless re/ghidra_proj oe2 -import re/ghidra_in/oe2_abs.mod -loader BinaryLoader
   -loader-baseAddr 0 -processor ARM:LE:32:v5t -scriptPath re/ghidra_scripts -preScript SetupBrew.java`.
3. `re/tools/redecomp.sh` applies `types/` + `names.txt` and exports `re/out/decomp.c` (+ fix_offsets.py).
4. After changing structs/names: `python3 re/tools/gen_engine_headers.py` regenerates `src/game/{types,protos}.h`.
Ghidra 12.1.4 (redecomp.sh looks in `$GHIDRA`, default `~/.local/opt/ghidra_12.1.4_PUBLIC`); its project path
must not contain a `.` element (hence the realpath in redecomp.sh when run from `.claude/worktrees/`).

## History
- 2026-10-01: RE complete (716 functions named, all structs typed; formats: .bar, strings, palettes,
  bitshapes/texels, .bsp maps, entities.bin, .pmd audio, saves HCFG/HPLR/HWLD). All 8 engine packages ported.
  QA: all 14 maps, 1.3M ASan/UBSan fuzz ticks, bot playthrough Caverns -> Dwarkar, every menu, shops,
  save/load (re/port/qa_gameplay.txt, qa_menus.txt).
- 2026-10-01/02: final review of every function against the original (18 verified findings, fixed or
  documented), golden traces recorded, George played level 1 and accepted it; tagged `classic-1.0`.
- 2026-10-04: second pass with oe-re's review (same method; details in re/notes/classic-1.0.md): Random_nextInt
  now follows the ARM's rotated unaligned load (dice change; golden traces re-recorded), game speed no longer
  tied to vsync, host fixes, arm_bit() everywhere, .gitignore hardened, history audited before publishing;
  `classic-1.0` moved to the reviewed commit.
- Still worth a human playthrough: quest content past Dwarkar's first story gate; the BREW key-repeat
  timing (1500/100 ms) is the commonly cited default, not confirmed from an SDK header.
