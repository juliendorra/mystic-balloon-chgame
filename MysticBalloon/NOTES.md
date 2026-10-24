# Mystic Balloon port notes

The source is TEAM a.r.g.'s Arduboy 1.7.2 at commit
`58743b6b52f1aa9f7bc39efd807034a83ebed9a6` from
<https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon>.
Only this new sketch and its workspace launcher are required; the CHGame
submodule and shared libraries are unchanged.

## Build and run

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

Local builds on 2026-10-24 (CHGame 0.3.0, native GCC 16.2.0):

| Build | Firmware image | Static RAM | Save pages |
|---|---:|---:|---:|
| Full-screen release 1.1.0 | 27,824 B / 50,944 B | 14,772 B / 18,416 B | 2 |
| Full-screen diagnostic 1.1.0 | 32,744 B / 50,944 B | 15,024 B / 18,416 B | 2 |

Both builds reserve another 2,048 B for the stack. The complete local
`check` passed, including identical captures from two gameplay runs and
the UBSan suite. The README preview is 157,063 B, under its 1 MB limit.
Real-device validation covered all 39 levels and three sustained scenarios
with no late frames; worst CPU work was 6.961 ms, with a 1,216-byte stack
watermark. See [the measurements](docs/performance.md).

## Adaptation

- `Port.h`/`Port.cpp` supply sprite, input and sound services. The original
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
- The world fills a native-scale 128×128 view. Its 2 KiB buffer is
  independent of the 8 KiB CHGfx framebuffer. `gfx_wait()` precedes both
  flash saves and writes to the display framebuffer. All 128 rows transfer
  each frame. Packed sprite/expansion SRAM sections occupy 504/324 bytes.
- The camera centers the 12×16 player at `(58, 56)` and clamps both axes
  to `0..256` within the 384×384 world. Foreground and parallax coverage,
  spawn initialization and off-screen death checks use the viewport size.
- Original 64-row menus are vertically centered. Gameplay and pause use
  the full view; pause draws the upstream badge. The added title, controls,
  status, help and pause panels are removed. The drawn state selects the
  expansion layout even when its handler transitions to another state.
- Score formatting uses `fmtInt`: the board's nano printf lacks `%l`.
- Flash records contain level, score, collected coins, best score/coin
  count, and sound. Unchanged data is never written again. Bonus counts
  finish before a level checkpoint is committed. Winning resets the
  next campaign's level, score, and coins, preserving the best result.
- START pauses without advancing game physics; SELECT toggles sound.
  The original Down+A pause remains. The introduction can be skipped with
  either action button. The original blank HELP page returns on A or B.
- Every game starts from its original physics and scoring. Each new level
  restores three balloons; the optional upstream `HARD_MODE` stays off.

## Validation

`tools/verify_sim.py` compares every original data array to upstream
fingerprints and compiles `tools/tests/port_test.cpp` against the actual
game and CHGame implementation under UBSan. It exercises sprite masking
and clipping (6,840 pixel-reference cases), full/centered expansion layouts,
18 camera placements, every level, tall hazards, all fan directions, pause, mute,
coin/key/exit collisions, scoring, checkpoint reload, continue, enemy
inhalation, death and completion. It runs 120 frames per level.

The shared simulator's pretend flash is a byte array cast to a word-aligned
record type. The test build makes an aligned copy of that host declaration
in `out/tests/`; physical device flash already has this alignment. Shared
source is unchanged. `CHMB_SANITIZERS=address,undefined` optionally enables
ASan too; the installed Apple Clang 15 ASan runtime cannot initialize on
this host, so the default uses UBSan.

`tools/scripts/gameplay.txt` records the README preview. `chgame check`
runs it twice and compares pixels for determinism and checks DMA hazards.
`tools/cart.py` makes the visual-menu picture from the original title art.

Debug identity: `MYBL 1.1.0`. `V` reports game state, entities, progress,
camera position and viewport height;
`U` reports game, expansion, chrome, wait, submit, CPU work and intervals;
`G 1` through `G 39` start a level for simulator/debug inspection. These
commands are absent from release builds. Save magic: `MYBL`; config
prefix: `CHMB_` (checked for collisions against the other games).

Release 1.1.0: 27,824 bytes, CRC32 `0x95E45A12`, SHA256
`4171331ee73004da5b618f6770c797b103e6ce0633496f08bb1f4f430e6db65c`.
The [release upload](docs/upload-1.1.0.txt) on 2026-10-24 was accepted and
marked valid; RUN launched the application and it re-enumerated on USB.
The verified cartridge contains this exact release binary.
The matching diagnostic build is 32,744 bytes, CRC32 `0xC2007B65`.
[Device evidence](docs/device-performance-128.json) records the 42 timing
scenarios plus viewport, movement, pause/resume and fault checks.
Speaker acoustics and power-cycle saving have not been independently
checked. These level visits are not a complete human playthrough.
