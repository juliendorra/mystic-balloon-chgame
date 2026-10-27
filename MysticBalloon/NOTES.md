# Mystic Balloon port notes

The source is TEAM a.r.g.'s Arduboy 1.7.2 at commit
`58743b6b52f1aa9f7bc39efd807034a83ebed9a6` from
<https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon>.
Only this new sketch and its workspace launcher are required; the CHGame
submodule and shared libraries are unchanged.

## Build and run

Version **1.3.0** adds [the ATMlib soundtrack](docs/atmlib-soundtrack.md) by
default. In CONF, Left/Right changes the audio engine and sound state,
with live audible previews. B saves; A restores all entry preferences.
Existing progress and palettes are retained. The source score, library,
generation tools and their MIT/BSD licenses are included for reproducibility.
Both the digital engine and actual RISC-V firmware have dedicated checks;
physical 1.2.9 measurements below describe the older beep-only firmware.

Version **1.2.9** includes the audio fidelity correction described in
[the audio audit](docs/audio-fidelity.md). The 1.2.8 binary identities and
hardware measurements below describe the earlier release, before this
correction.

Run these commands from the workspace root. Dependencies, board package,
and the native RISC-V compiler are shared with Ball and Pinball.

```sh
python3 mysticballoon.py setup
python3 mysticballoon.py build
.venv/bin/python MysticBalloon/tools/verify_sim.py
python3 mysticballoon.py check
python3 mysticballoon.py gif
python3 mysticballoon.py boxart
python3 mysticballoon.py export --no-build
python3 mysticballoon.py cart verify MysticBalloon/build/MysticBalloon.chgame
```

`check --no-device` runs the software checks without recompiling the device
image. Plain `check` builds locally; neither command uploads or uses CI.
`python3 mysticballoon.py upload` explicitly installs the release on a
connected handheld.

Version 1.3.0 builds at 44,568 bytes (release) and 50,104 bytes (diagnostic),
with 18,140 / 18,404 bytes static RAM and a separate 2,048-byte stack. Both
save pages are retained. All 45 physical ATM-audio scenarios passed at
approximately 60 fps with zero late frames and no dropped commands; maximum
CPU work was 15.966 ms and stack use 1,328 bytes. All five gameplay tunes,
both saved audio choices, palettes and pause/checkpoint controls passed.
The final CONF controls passed physical checks. The published normal firmware
and SD cartridge are installed and verified, preserving the current checkpoint,
all preferences, and all 40 other visible card files and timestamps. See the
[1.3.0 installation record](docs/install-audio-1.3.0.json).

Audio-fix version 1.2.9 builds locally at 42,332 bytes (release) and
47,776 bytes (diagnostic), with 17,776 / 18,028 bytes static RAM and a
separate 2,048-byte stack. Both retain the two save pages. On CHGame Rev0,
all 45 audio-enabled performance cases passed at 59.76–60.60 fps with zero
late frames, maximum work 13.811 ms, and a 1,232-byte stack watermark.
The original checkpoint and preferences were retained. See
[the 1.2.9 device record](docs/device-audio-1.2.9.json).

Local builds on 2026-10-24 (CHGame 0.3.0, native GCC 16.2.0):

| Build | Firmware image | Static RAM | Save pages |
|---|---:|---:|---:|
| Transparent platform release 1.2.8 | 42,612 B / 50,944 B | 17,800 B / 18,416 B | 2 |
| Transparent platform diagnostic 1.2.8 | 48,048 B / 50,944 B | 18,072 B / 18,416 B | 2 |

Both builds reserve another 2,048 B for the stack. The complete local
`check` passed, including identical captures from two gameplay runs and
the UBSan suite. The README preview is 205,727 B, under its 1 MB limit.
Release 1.2.8 removes the platform corner matte in Color, DMG and Amber.
Convex corner padding reveals the exact scrolling sky texture; connected
tile edges retain their fill. Original B&W retains its original pixels.
The 1,716 platform references cover all 16 adjacency variants, 16 scroll
phases, all 39 maps and four display modes, including native atlas blits.
Corner restoration runs after player backdrop restoration and before
foreground commands. See [corners](docs/platform-corners.png) and
[joined platforms](docs/platform-joins.png).
The 1.2.8 hardware run covers all 39 levels and six sustained scenarios
with sound enabled: 59.66–60.56 fps, zero late frames, maximum CPU work
14.126 ms, and a 1,232-byte stack watermark. All 64 physical player/inhale
canvases match the reviewed native references. See the
[timings](docs/device-performance-platforms.json) and
[pose comparisons](docs/device-player-1.2.8.json).
All twelve current physical border/ascent fixtures also match their native
references, with six additional timing scenarios and zero late frames.
Diagnostics restore the original checkpoint and saved preferences; see
[current scrolling checks](docs/device-scrolling-1.2.8.json).
Release 1.2.7 removes exterior tint from every player/inhale frame and
gives the entire Amber body the same light fill as the head. Offline masks
exclude clearing padding; 48 saved source bytes restore the underlying
world before replaying color sprites. The generic packed loop stays in
SRAM while sprite selection/scaling dispatch stays in flash. The checks
include 320 complete patterned-background/terrain composites and 64
full-frame protocol comparisons in the host simulator. These fixes are
included in 1.2.8. See [Amber before/after](docs/amber-player-fix.png).
Release 1.2.6 restores the original camera's bottom-only cap, revealing
implicit side walls and following balloon flight above the map. Repeating
backgrounds use positive modulo at negative coordinates. The old clamp
left the player completely hidden for 134/150 ascent frames; the same
fixture now has zero hidden frames. See the [before/after](docs/scrolling-review.png)
and [four-mode review](docs/scrolling-palettes.png).
All twelve physical side-wall/ascent cases match the native references
and simulator camera/physics. Six sustained device scenarios run at
60.17–60.29 fps, with zero late frames, maximum CPU work of 12.326 ms and
a 1,232-byte stack watermark. The original checkpoint and preferences are
restored after diagnostics. See [scrolling evidence](docs/device-performance-scrolling.json).
Release 1.2.5 restores the splash's large angled balloon and taut diagonal
string. CONF previews palettes immediately; B confirms and A cancels.
Pause offers RESUME and SAVE & QUIT using the existing level checkpoint.
Only the opaque panel is replayed over the frozen native world, avoiding
a redundant expansion and world redraw. All four modes match a full
redraw in pixel-reference tests. On the handheld, the title runs at
60.20 fps and pause at 59.95–60.24 fps, with zero late frames and maximum
CPU work of 5.375 ms. Save/quit and normal CONT were verified using the
existing checkpoint. See the [motion review](docs/splash-motion-review.png),
[drawing loop](docs/pixel-art-workflow.md), and [device evidence](docs/device-performance-controls.json).
The 1.2.1 device check confirms the Pastel Island default, expanded splash
and all four display modes with sound enabled: 59.94–60.24 fps, no late
frames, and at most 10.615 ms CPU work. The unchanged renderer's 1.2.0
validation covered all 39 levels and six sustained scenarios, with no late
frames; worst CPU work was 13.495 ms, with a 1,216-byte stack watermark.
See [the measurements](docs/performance.md).
The reusable workflow is in the [Arduboy porting guide](../docs/arduboy-porting.md).

## Adaptation

- `Port.h`/`Port.cpp` supply sprite and input services; `Tones.cpp` supplies
  ArduboyTones-compatible playback. The original
  Arduboy libraries, AVR timers and EEPROM library are not dependencies.
- `vec2` uses signed 16-bit components; score/checkpoint values are fixed
  32-bit values on both the host and RISC-V target. Function dispatch uses
  native-width pointers. Binary literals are standard `0b` literals.
- Negative shifts are replaced by multiplication; speed macros are
  parenthesized. Wide spike rectangles retain their original height.
- All 39 levels are in the upstream order, including its intentional
  omissions and reordered late levels. Unused source level arrays remain
  in `bitmaps.h`; the linker removes unused constants.
- Playable level 23 contains four coins in the original map. The all-coins
  badge checks the actual campaign total of 232, correcting the upstream
  constant of 234 without changing a map.
- `PortSprites` implements self-mask, overwrite, erase and interleaved
  image/mask drawing, with clipping before accessing either framebuffer.
  It stamps packed vertical bytes, with aligned byte copies or shifted
  page masks; this loop and the packed expansion run from SRAM.
- The default Pastel Island color view uses 16 semantic colors: soft cyan
  sky, mint terrain, peach skin and distinct red/yellow/blue balloons,
  including the HUD.
  CONF selects Color, Original B&W, DMG Green or Amber. Bits 1..2 of the
  existing option byte hold the palette; bit 0 remains sound. Schema-1
  saves from older releases retain progress and default to Color.
- Up to 128 color commands (8 bytes each on the device) replay after DMA.
  World colors use packed terrain spans; colored sprite pairs read each
  vertical source byte once for eight rows. Interior tint masks exclude
  the original white matte around coins, hazards and enemies. Original
  mode uses the unchanged monochrome result. Paused world commands and
  native pixels are retained; only the opaque pause panel and its labels
  are replayed, without accumulating duplicate commands or allocating RAM.
- Player color assets contain per-frame enclosed fill and black contours;
  the original generic clearing masks are never tinted. A 48-byte backdrop
  preserves the pre-player mono pixels, and packed restoration reconstructs
  sky/terrain under the player before all color commands replay. Amber
  assigns every player fill row index 14, matching the light head. Original
  mode bypasses this restoration and uses the original source drawing.
- The world fills a native-scale 128×128 view. Its 2 KiB buffer is
  independent of the 8 KiB CHGfx framebuffer. `gfx_wait()` precedes both
  flash saves and writes to the display framebuffer. All 128 rows transfer
  each frame. Color rendering shares the same native framebuffer;
  there is no second 8 KiB color buffer.
- The camera centers the 12×16 player at `(58, 56)` and preserves the
  upstream bottom-only cap, now 256 for the taller view. It follows outside
  the sides to reveal implicit collision walls and above row zero during
  balloon flight. Foreground and parallax cover negative coordinates;
  parallax uses positive modulo to avoid top/left seams. Spawn initialization
  and off-screen death checks use the viewport size.
- The title scene is one continuous 128×128 drawing extending the original
  child, large balloon and enemy composition. The original logo is drawn
  at 2× with a tight frame matte; its unused rows and outer corners are
  transparent. Main and CONF overlays fit the central negative space. Detached
  miniature gameplay art and the old coordinate-bound eye sprites are
  removed. Other 64-row menu cards are centered.
  [Drawing sources and workflow](tools/art/splash/README.md) include the
  extracted original and imagegen provenance. `tools/art/build_art.py`
  generates both compressed splash variants, color tables, tint masks and
  menu lettering. The [palette study](docs/palette-study.md)
  includes four full-color directions and complete sheets of 145 frames.
  `CHMB_COLOR_VARIANT` in `config.h` selects the default candidate; rerun
  `.venv/bin/python MysticBalloon/tools/art/build_art.py` from the workspace
  root after changing it, then rebuild the game, GIF and cartridge art.
- Gameplay and pause use the full view; the requested pause action panel
  includes the upstream badge. Added gameplay control/status panels remain
  removed. The drawn state
  selects the layout even when its handler transitions to another state.
- Score formatting uses `fmtInt`: the board's nano printf lacks `%l`.
- Flash records contain level, score, collected coins, best score/coin
  count, sound, and palette. Unchanged data is never written again. Bonus counts
  finish before a level checkpoint is committed. Winning resets the
  next campaign's level, score, and coins, preserving the best result.
  SAVE & QUIT commits the level-entry checkpoint and best score, then
  returns to the menu only after a successful save. CONT restarts the
  level with its entry score/coins and three balloons; unbanked rewards
  are reset. Previewing a palette never writes flash.
- START pauses without advancing game physics; SELECT toggles sound.
  The original Down+A pause remains. The introduction can be skipped with
  either action button. The original blank HELP page returns on A or B.
- The private tone driver preserves the original 2 MHz divider, half-wave
  counts, monophonic interruption, timer phase and mute behavior. It replaces
  the shared blip sequencer's coarse frequency/duration units. Flash saves
  wait for the voice to finish because programming masks interrupts. SELECT
  preferences remain pending until a safe write; SAVE & QUIT keeps the world
  frozen while waiting. The 16-bit frame-counter wrap is retained for periodic
  sound triggers. The port still runs at 60 fps; the original Arduboy2 frame
  clock nominally schedules 16 ms frames. See [timing limits](docs/audio-fidelity.md).
- Every game starts from its original physics and scoring. Each new level
  restores three balloons; the optional upstream `HARD_MODE` stays off.

## Validation

`tools/verify_sim.py` compares every original data array to upstream
fingerprints and compiles `tools/tests/port_test.cpp` against the actual
game and CHGame implementation under UBSan. It exercises sprite masking
and clipping (6,840 pixel-reference cases), full/centered expansion layouts,
48 camera placements (including negative coordinates), every level, tall hazards, all fan directions, pause, mute,
coin/key/exit collisions, scoring, checkpoint reload, continue, enemy
inhalation, death and completion. It runs 120 frames per level.
Color checks compare 32 native blits to an independent clipping/mask/scale
reference, test packed terrain colors at unaligned camera positions, verify
palette/save compatibility and menu input, and require visible peach skin
and all three balloon colors. The packed color optimization produced
identical native pixels for every exported frame and review scene.
Scrolling regressions render visible side walls in all 39 levels and run
fan-driven flight above row zero and a return in all four display modes.
`tools/preview_scrolling.py` exports native fixtures for the corresponding
physical-device checker, `tools/verify_scrolling_device.py`.

`tools/verify_audio.py --fetch` downloads hash-pinned original game and
Arduboy library sources once; subsequent runs are offline. It executes the
actual original sound-trigger handlers alongside the port and compares
452 tone requests across 57 scenarios. A separate register harness executes
the original ArduboyTones driver and native timer ISR across 180 cases,
checking divider, pin transitions, stop counts, mute and replacement. Source
hashes and detailed results are in [the audio audit](docs/audio-fidelity.json).

The shared simulator's pretend flash is a byte array cast to a word-aligned
record type. The test build makes an aligned copy of that host declaration
in `out/tests/`; physical device flash already has this alignment. Shared
source is unchanged. `CHMB_SANITIZERS=address,undefined` optionally enables
ASan too; the installed Apple Clang 15 ASan runtime cannot initialize on
this host, so the default uses UBSan.

`tools/scripts/gameplay.txt` records the README preview. `chgame check`
runs it twice and compares pixels for determinism and checks DMA hazards.
`tools/cart.py` makes the visual-menu picture from the original title art.

Debug identity: `MYBL 1.2.9`. `V` reports game state, entities, progress,
camera position, viewport height, palette and command capacity;
`U` reports game, expansion, color, wait, submit, CPU work and intervals;
`G 1` through `G 39` start a level for simulator/debug inspection. These
commands and `C 0..3` (display mode), `T 0..3` (color study), `O 0/1`
(runtime audio override), `J x y vy` (edge/fan launch in the current level),
and `R` (discard diagnostic changes and reload the saved main menu) are
absent from release builds. Save magic: `MYBL`; config
prefix: `CHMB_` (checked for collisions against the other games).

`D 0..15` renders static native reference canvases for all twelve player
poses and four inhale frames. `tools/verify_player_device.py --port PORT`
checks all 64 mode/pose combinations; `--sim EXECUTABLE --out DIRECTORY`
validates the same protocol in the host. `tools/diagnose_mattes.py` builds
the references and checks that original exterior clearing pixels receive
no tint in any mode. The historical [1.2.7 local validation record](docs/player-composite-checks.json)
retains that candidate's binary identities; its player fixes ship with
1.2.8. The [1.2.8 validation record](docs/platform-composite-checks.json)
includes the platform references, current firmware identities and device
results. The release CRC32 is `0x909C57E2`; its verified cartridge contains
the exact 42,612-byte image.

Installed 1.2.9 release build: 42,332 bytes, CRC32 `0x863402A6`, SHA256
`625a802492eb39924ed7cfdb1f00b21c9532116fc7bb9b5e2f02a51ce4a475d8`.
The [upload](docs/upload-1.2.9.txt) was accepted and the normal application
re-enumerated on 2026-10-25. Its existing SD-menu cartridge now contains the
same firmware. All 39 other visible files and timestamps were retained,
and the original checkpoint/preferences were verified after the SD update.
See [installation evidence](docs/install-audio-1.2.9.json).

Release 1.2.8 SHA256:
`cb9c6a2030be8527759a026a245a67c6c9b453e19ff4b3e50fd396b70c9dc6cf`.
The [release upload](docs/upload-1.2.8.txt) was accepted and marked valid on
2026-10-24; RUN launched it and the application re-enumerated on USB.
This normal release replaces the diagnostic build, with both save pages
retained. The bootloader verifies the image CRC in flash; independent
readback is unavailable on bootloader v3.

Release 1.2.6: 40,232 bytes, CRC32 `0xB53B52EF`, SHA256
`132ccaec9418a151cd9b08507e7b306c36f40cff862a4dba8604ec502eea0255`.
The [release upload](docs/upload-1.2.6.txt) was accepted and marked valid on
2026-10-24; RUN launched it and the application re-enumerated on USB.
Its cartridge was verified to contain that exact release binary.
The 1.2.6 diagnostic build is 45,476 bytes, CRC32 `0x6B9A823F`.
[Scrolling evidence](docs/device-performance-scrolling.json) records
camera/physics comparisons, native wall/sky/player pixel checks and timing.
[Full composite evidence](docs/device-performance-composite.json) records
native pixel comparisons, drawing invariants, menu controls and timing.
[Earlier 1.2.2 evidence](docs/device-performance-splash.json) retains its
own native comparisons and title/gameplay measurements.
[Pastel Island 1.2.1 evidence](docs/device-performance-pastel.json) retains
the default/splash and four-mode checks. The retained [1.2.0 device evidence](docs/device-performance-color.json)
records the 45 timing scenarios plus viewport, palette, movement,
pause/resume and fault checks for the same renderer.
Speaker acoustics and power-cycle saving have not been independently
checked. These level visits are not a complete human playthrough.
