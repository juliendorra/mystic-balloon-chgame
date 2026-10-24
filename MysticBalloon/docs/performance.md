# Mystic Balloon performance on CHGame Rev0

The [Arduboy porting guide](../../docs/arduboy-porting.md) explains how to
apply these rendering and validation techniques to another game.

## Transparent platform corners, release 1.2.8

The packed terrain pass tinted each complete 16×16 tile. White padding
outside rounded corners consequently became light green in Color, with
equivalent artifacts in DMG and Amber. Offline generation now identifies
only convex exterior corners using the tile's exposed-side bits, preserving
valid white fill at joined edges. Original B&W stays pixel-identical.

The world renderer caches visible tile variants and restores their sparse
corner coordinates from the original parallax sky pattern. Its phase is
the same quarter-camera phase used by the monochrome background, including
negative coordinates. This restoration follows the player backdrop repair
and precedes foreground color commands, so neither layer reintroduces the
matte. There are only 32 corner coordinates across all 16 terrain variants;
the flash data contains 17 offsets, 32 coordinates and 32 sky bytes. Direct
atlas blits use a separate 544-byte opacity table. No framebuffer is added.

The complete local check passes, including deterministic captures, the
target build and UBSan. There are 1,648 independent full-frame platform
references (all variants, 16 scroll phases, 39 maps and four palettes) plus
68 direct atlas references. The 320 complete player composites also test
corner restoration with the player backdrop enabled. All 64 host-protocol
player/inhale canvases match their independent native references, and the
sprite diagnosis finds zero visible exterior tint in every display mode.

Release: 42,612 bytes, CRC32 `0x909C57E2`, static RAM 17,800 bytes.
Diagnostic: `MYBL 1.2.8`, 48,048 bytes, CRC32 `0x63BDB65C`, static RAM
18,072 bytes. Both retain two save pages and the separate 2 KiB stack.
The verified cartridge contains the exact release image. The player and
Amber fixes from the local 1.2.7 candidate are included in this release.

On CHGame Rev0 with sound enabled, all 39 levels plus six sustained
gameplay/palette scenarios pass: 59.66–60.56 fps, zero late frames, worst
CPU work 14.126 ms and a 1,232-byte stack watermark. Every color command
is retained. All 64 physical player/inhale canvases match the independent
native references exactly, and diagnostics restore saved preferences.
All twelve physical side-wall/ascent fixtures also match the current native
pixels and simulator camera/physics. Six additional scrolling timing
scenarios pass with zero late frames; the original checkpoint and saved
preferences are restored before installing the normal release.
The [normal 1.2.8 release upload](upload-1.2.8.txt) was accepted and marked
valid, and RUN launched the application successfully. The installed CRC32
is `0x909C57E2`; the diagnostic build has been replaced.

- [Corner before/after in all four modes](platform-corners.png)
- [Isolated and joined platforms](platform-joins.png)
- [Current validation and exact firmware identities](platform-composite-checks.json)
- [Hardware timing and checks](device-performance-platforms.json)
- [Physical player/inhale comparisons](device-player-1.2.8.json)
- [Current physical scrolling comparisons](device-scrolling-1.2.8.json)

## Clean player compositing and light Amber body, candidate 1.2.7

The player previously recolored its entire white clearing silhouette,
including 12–31 exterior pixels per normal pose and 22–32 per inhale pose.
This was most visible in DMG and Amber. Frame-specific color sprites now
contain only enclosed fill and black contours. Amber uses light index 14
for every fill row, so head, shirt and trousers share the same light shade.
Dark outlines and details remain; Original B&W keeps its original pixels.

Simply skipping exterior tint is insufficient: the original monochrome
pass has already cleared those pixels. The port now captures at most 48
source bytes beneath the player before its mask draws. After packed world
expansion, a small packed restoration reconstructs that background, then
color commands replay in their normal order. This preserves sky texture,
terrain, overlapping objects and clipping without another framebuffer.
The 836-byte color sprites are generated into flash; flood filling runs
offline. The packed inner loop remains in SRAM, while sprite selection
and the rare scaled path dispatch from flash, reducing static RAM overall.

The complete local check passes, including UBSan and deterministic runs.
There are 320 independent full-frame composites covering all normal and
inhale frames, four modes, odd/aligned placement, all screen edges, patterned
sky and terrain. They require zero changes outside the visible player,
uniformly light Amber fill, and exact original-mode output. Another 64
full-frame comparisons validate the diagnostic protocol in the simulator;
the sprite diagnosis checks 84 sprite and four particle cases.

Release: 41,664 bytes, CRC32 `0x159EB6E5`, static RAM 17,720 bytes.
Diagnostic: `MYBL 1.2.7`, 47,104 bytes, CRC32 `0xFB40E4D6`, static RAM
17,992 bytes. Both retain two save pages and the separate 2 KiB stack.
The cartridge is verified and contains the exact release binary.

The handheld was not detected during the 1.2.7 candidate checks. Its fixes
were subsequently validated on hardware as part of release 1.2.8 above;
1.2.7 was not installed separately. The measurements below retain their
original firmware identities.

- [Amber before/after](amber-player-fix.png)
- [Current player poses in four modes](sprite-matte-comparison.png)
- [Original 1.2.6 diagnosis](sprite-matte-diagnosis-1.2.6.json)
- [Current exterior-pixel checks](sprite-matte-diagnosis.json)
- [Local validation and exact candidate identities](player-composite-checks.json)

## Visible boundaries and above-map flight, release 1.2.6

The 128×128 adaptation incorrectly clamped both camera axes to the stored
384×384 map rectangle. Upstream only caps the bottom: side walls are solid
tiles outside the stored columns, and fans can lift the player above row
zero. The clamp hid those collision walls and let the player disappear
through the top. The corrected camera follows outside the sides and top,
retaining the bottom cap at 256. Negative parallax phases now use positive
modulo so the background covers the top/left edges without seams.

A level-1 fan launch, followed by 150 frames holding B through normal
physics, reproduces the bug: before the fix, the player is completely
hidden for 134 frames, reaching screen y=-59. With the same launch, no
frames are hidden and the minimum player screen y is 50. At the side
fixtures, 58 pixels of the implicit boundary wall are visible, versus zero
before the fix. No original map, collision rule or flight physics changed.

The complete software check passes: unchanged upstream arrays, UBSan,
deterministic captures, 48 camera pixel references including negative
coordinates, visible side walls in all 39 levels, and ascent/return in all
four display modes. On CHGame Rev0, all 12 side-wall/ascent cases match
the simulator's camera and physics. Native wall, above-map background and
player pixels match the reviewed references in every mode. Comparisons
exclude the HUD and the small band of random wind particles which can
overshoot row zero by six pixels.

With runtime sound enabled, two seconds per scenario:

| Scenario | Device fps | Maximum CPU work |
|---|---:|---:|
| Color, left boundary | 60.29 | 12.326 ms |
| Color, right boundary | 60.24 | 11.023 ms |
| Color, above-map balloon flight | 60.19 | 8.085 ms |
| Original B&W, above-map flight | 60.21 | 5.142 ms |
| DMG, above-map flight | 60.20 | 8.056 ms |
| Amber, above-map flight | 60.17 | 8.067 ms |

All six scenarios have zero late frames and no dropped drawing commands.
The stack watermark is 1,232 bytes of 2,048, with no recorded device fault.
The original flash checkpoint and palette/sound preferences are restored
after diagnostics. These are targeted regression fixtures and measurements,
rather than a complete playthrough.

Release: 40,232 bytes, CRC32 `0xB53B52EF`, static RAM 17,948 bytes.
Diagnostic: `MYBL 1.2.6`, 45,476 bytes, CRC32 `0x6B9A823F`, static RAM
18,220 bytes. Both retain two save pages and the separate 2 KiB stack.

- [Before/after: both walls and ascent](scrolling-review.png)
- [All three cases in four palettes](scrolling-palettes.png)
- [Physical device log](device-scrolling.txt)
- [Checks, native pixel comparisons, timings and firmware identities](device-performance-scrolling.json)
- [Reference exporter](../tools/preview_scrolling.py) and [device checker](../tools/verify_scrolling_device.py)

## Palette preview, Save & Quit and splash motion, release 1.2.5

The splash restores a large angled balloon cropped deliberately at the
right edge, with a taut diagonal from its knot to the gripping hand.
CONF previews the highlighted palette immediately; B confirms and A
restores the previous choice. Pause adds RESUME and SAVE & QUIT.

The first pause implementation unnecessarily expanded and recolored the
frozen world after waiting for LCD DMA. On the physical device this cost
17.19 ms per frame, reducing pause to 58.23 fps. The corrected path retains
the native world and replays only the opaque action panel and its labels
after `gfx_wait()`. Full-screen DMA remains unchanged; no buffer or cache
allocation was added. In all four palettes, switching selections matches
the full-world rendering reference byte for byte and preserves every
pixel outside the panel.

With runtime sound enabled on CHGame Rev0:

| Scenario, 2 seconds each | Device fps | Maximum CPU work |
|---|---:|---:|
| Main menu and dynamic balloon | 60.20 | 5.001 ms |
| Color pause menu | 59.97 | 5.374 ms |
| Original B&W pause menu | 60.24 | 5.337 ms |
| DMG pause menu | 59.95 | 5.375 ms |
| Amber pause menu | 60.21 | 5.375 ms |

All have zero late frames and a 1,232-byte stack watermark. The physical
check verifies live previews, confirm/cancel, frozen physics/camera/pixels,
native panel references, resume, Save & Quit and normal CONT. It uses the
existing checkpoint rather than replacing it with a diagnostic level.
CONT restores the level-entry score/coins and three balloons. Host tests
also check mid-level coin collection, flash reload, and reward reset to
prevent duplicating collected coins. The complete local check passes,
including UBSan, deterministic captures and the release build.

Release: 40,280 bytes, CRC32 `0xAE8DFE5D`, static RAM 17,948 bytes.
Diagnostic: `MYBL 1.2.5`, 45,336 bytes, CRC32 `0x30FBDFA7`, static RAM
18,220 bytes. Both retain two save pages and a separate 2 KiB stack.
Gameplay timing remains documented under the earlier renderer releases;
this focused run covers the changed title and pause paths.

- [Whole-frame motion correction](splash-motion-review.png)
- [All palettes, menus and pause selections](splash-review.png)
- [Device log](device-controls.txt)
- [Checks, timings and exact firmware identities](device-performance-controls.json)
- [Repeatable device checker](../tools/verify_controls_device.py)

## Complete splash composition, release 1.2.4

The splash now uses eight intentional colors, clean foreground cuts, a
fully visible balloon, a connected outlined string and a resolved grip.
Skin colors stay on the child; the background vapor uses mint shades.
The title matte trims its six unused source rows and outer corners,
preserving 12 native rows that were previously covered by padding.
CONF moves five pixels right into the reserved dark pocket. Gameplay
rendering and logic are unchanged.

Whole-scene views at 1× and 3× were checked between drawing passes, followed
by actual menu renders in all four modes. The complete local check passes.
On CHGame Rev0, all eight main/CONF screenshots exactly match their reviewed
native pixels. The cartridge picture also matches the game artwork exactly,
without color merging. With sound enabled:

| Scenario | Device fps | Maximum CPU work |
|---|---:|---:|
| Complete splash and main menu, 3 seconds | 60.17 | 4.906 ms |
| Level 6 jumping, 5 seconds | 60.14 | 10.053 ms |

Both have zero late frames, no dropped drawing commands and no recorded
device fault. Diagnostic: `MYBL 1.2.4`, 44,076 bytes, CRC32 `0x9943340E`.
Release: 39,068 bytes, CRC32 `0xE98AB87C`; both retain two save pages.

- [Full-scene before/after and native-size review](splash-global-review.png)
- [Art-only composite review](splash-art-review.png)
- [All display modes and menu selections](splash-review.png)
- [Device log](device-composite.txt)
- [Checks, measurements and exact firmware identities](device-performance-composite.json)

## Direct eye refinement, release 1.2.3

This release changes 42 splash pixels using hand-drawn palette-indexed
stencils. The draw/check/redraw loop ran locally twice, then the real menu
renderer was reviewed in Color, Original B&W, DMG Green and Amber. The full
local check passes, including unchanged upstream arrays, UBSan tests,
deterministic captures and a native release build. The pixel patch is
reproducible and preserves every pixel outside its two eye regions, the
canvas dimensions and the palette.

Release: 40,344 bytes, CRC32 `0x936ACAA5`, static RAM 17,936 bytes, with two
save pages. Gameplay rendering and logic are unchanged. No additional
hardware timing run was needed for this small artwork correction; the
recorded 1.2.2 measurements remain below with their original identities.

- [Drawing workflow and research](pixel-art-workflow.md)
- [Exact edit and release identity](eyes-edit.json)
- [Before/after and pixel difference](eyes-review.png)

## Continuous splash release 1.2.2

Release 1.2.2 replaces the assembled title art with one continuous drawing
and places the original main menu and CONF controls in its central space.
The gameplay renderer is unchanged. The complete local check passes,
including all 81 unchanged upstream arrays, UBSan game tests and identical
simulator captures from two runs.

On the physical CHGame Rev0, all eight main/CONF captures in Color,
Original B&W, DMG Green and Amber exactly match the reviewed native pixels.
CONF is reached through normal button controls. With sound enabled:

| Scenario | Device fps | Average CPU work | Maximum CPU work |
|---|---:|---:|---:|
| Continuous splash and main menu, 3 seconds | 59.97 | 5.336 ms | 5.340 ms |
| Level 6 jumping, 5 seconds | 60.11 | 9.812 ms | 10.640 ms |

Both scenarios have zero late frames, no dropped color commands and no
device fault. Diagnostic: `MYBL 1.2.2`, 45,324 bytes, CRC32 `0x3609110E`.
Release: 40,316 bytes, CRC32 `0x45041057`. Both retain two save pages.

- [Device log](device-splash.txt)
- [Native pixel checks, timing and exact firmware identities](device-performance-splash.json)
- [Visual review of the artwork and menu selections](splash-review.png)
- [Drawing source and reproducible art workflow](../tools/art/splash/README.md)

## Pastel Island release 1.2.1

Pastel Island is the default full-color palette in 1.2.1. The game, expanded
splash, GIF and cartridge art use the same selected color table. Rendering
and game logic are unchanged from 1.2.0; the earlier 45-scenario renderer
validation is retained below.

Focused checks on the physical CHGame Rev0 on 2026-10-24 confirmed the
boot/default palette, expanded splash and all four menu display modes.
Level 6 was exercised with sound enabled for five seconds in Pastel Island
and two seconds in each other mode, after a warm-up. All had zero late
frames and no dropped color commands; the stack watermark was 1,216 bytes.

| Mode | Device fps | Average CPU work | Maximum CPU work |
|---|---:|---:|---:|
| Pastel Island | 60.13 | 9.830 ms | 10.615 ms |
| Original B&W | 60.24 | 5.502 ms | 6.419 ms |
| DMG | 59.94 | 9.464 ms | 10.555 ms |
| Amber | 60.16 | 9.455 ms | 10.574 ms |

Diagnostic: `MYBL 1.2.1`, 43,688 bytes, CRC32 `0xCD3039FA`.
Release: 38,688 bytes, CRC32 `0x051A08E4`.

- [Pastel Island device log](device-pastel.txt)
- [Focused checks and exact firmware identities](device-performance-pastel.json)

## Color renderer validation, release 1.2.0

The final color renderer was measured on CHGame Rev0 on 2026-10-24 with
sound enabled. All 45 scenarios passed with zero late frames: 39 level
visits, three longer color scenarios, and one sustained jumping scenario
in each other display mode. Worst CPU work was 13.495 ms against the
16.667 ms budget; the stack watermark was 1,216 bytes. No color commands
were dropped; the largest observed command count was 69 of 128.

| Scenario | Device fps | Average CPU work | Maximum CPU work |
|---|---:|---:|---:|
| Color, level 1 walking | 60.13 | 8.168 ms | 8.290 ms |
| Color, level 6 jumping | 60.16 | 9.833 ms | 10.627 ms |
| Color, level 39 inhaling | 60.14 | 10.474 ms | 10.592 ms |
| Original B&W, level 6 jumping | 60.12 | 5.562 ms | 6.420 ms |
| DMG, level 6 jumping | 60.16 | 9.663 ms | 10.596 ms |
| Amber, level 6 jumping | 60.12 | 9.670 ms | 10.631 ms |

The monochrome buffer still overlaps the preceding LCD transfer. After
`gfx_wait()`, packed expansion produces background indices 0/2, and adding
three over packed terrain spans produces 3/5. This avoids a tile lookup for
every pixel. Compact commands then replay sprite colors: pairs of packed
source columns are loaded once and expanded across eight rows. Interior
masking removes the original white matte around objects. These loops run
from SRAM, and full-screen transfer remains asynchronous.

Changing palettes does not change game logic. Original B&W bypasses the
color replay; DMG and Amber map the same colored scene to four shades.
The four full-color review candidates share semantic indices and differ
only in palette values. The [study](palette-study.md) renders all source
animation frames through the actual game code; all exported native pixels
were identical before and after the packed color-blit optimization.

`U` now reports separate `expand` and `color` stages; the old `chrome` field
belonged to the removed UI. During hardware checks, `O` enables audio only
at runtime, preserving the saved preference. The tone adapter now passes a
valid effect table to the native audio service, which otherwise refuses
to enable sound.

Diagnostic: `MYBL 1.2.0`, 43,680 bytes, CRC32 `0xB457A70D`.
Release: 38,684 bytes, CRC32 `0xCCEF73DB`. The final release also colors
the decorative splash balloons yellow/blue to match the gameplay trio;
that artwork-only change followed timing validation.

- [Color device log](device-color.txt)
- [All 45 scenarios and functional checks](device-performance-color.json)

These are targeted level visits and tests, not a complete playthrough.

## Historical native 128×128 release 1.1.0

Measured on the physical handheld on 2026-10-24. Gameplay now occupies all
128×128 pixels with the original HUD and pause artwork; added UI is removed.
The monochrome buffer is 2 KiB, expansion covers all 128 rows, and every
frame submits the full LCD framebuffer. Packed sprite/expansion loops
still run from SRAM. Camera, foreground and parallax coverage use the new
viewport dimensions.

| Five-second scenario | Device fps | Average CPU work | Maximum CPU work |
|---|---:|---:|---:|
| Level 1, walking | 60.15 | 4.738 ms | 4.793 ms |
| Level 6, jumping | 60.14 | 5.236 ms | 6.112 ms |
| Level 39, inhaling | 60.13 | 5.922 ms | 5.999 ms |

All 39 levels were also visited while moving and floating. Across all 42
scenarios: zero late frames, worst CPU work 6.961 ms, maximum stack
watermark 1,216 bytes of 2,048. Short one-second measurements ranged from
59.67 to 60.80 fps; serial timing and boundary frames explain this spread.
The stage timer records 16.665–16.666 ms average intervals in the sustained
tests. Expansion takes approximately 1.417 ms per full-screen frame.

The checker also verifies the 128-row viewport, camera bounds, original
monochrome palette indices, movement, pause/resume and absence of device
faults. This is a set of targeted scenarios, not a complete playthrough.
Software checks pass 6,840 sprite reference cases, both expansion layouts,
18 camera placements, game flow under UBSan, and deterministic captures.

Diagnostic identity: `MYBL 1.1.0`, CRC32 `0xC2007B65`. Release 1.1.0 uses the
same gameplay renderer and is 27,824 bytes, CRC32 `0x95E45A12`.

- [Full-screen device log](device-128.txt)
- [All 42 full-screen scenarios and functional checks](device-performance-128.json)

## Historical 128×64 optimization (1.0.1)

Measured on the physical handheld on 2026-10-24. The original port ran at
approximately 26–31 fps: each sprite was drawn pixel by pixel from flash,
the playfield was expanded pixel by pixel, and all labels were redrawn on
every frame. The simulator checked correctness but did not model this CPU
cost; its deterministic game clock was insufficient evidence of device fps.

The optimized renderer stamps each Arduboy byte as eight vertical pixels,
uses byte copies for aligned opaque sprites, and shifts bytes across two
pages for other alignments. Both sprite drawing and framebuffer expansion
run from SRAM. The expansion reads each source column pair once and writes
eight packed output rows. Unchanged labels stay in the framebuffer, and
ordinary play transfers only the middle 64 LCD rows. A changed status band
causes a full transfer.

| Device scenario | Original CPU work / frame | Optimized CPU work / frame | Original frames in 4 s | Optimized frames in 4 s |
|---|---:|---:|---:|---:|
| Level 1, moving | 35.260 ms | 3.369 ms | 114 | 241 |
| Level 6, jumping | 31.964 ms | 3.405 ms | 126 | 241 |
| Level 39, inhaling | 38.165 ms | 3.611 ms | 106 | 241 |

CPU work includes game logic, sprite rendering, framebuffer expansion,
labels, waiting for the previous transfer, and submitting the next
transfer. LCD DMA continues asynchronously after submission; the fps and
late-frame measurements cover the actual running game, including transfers.
The 60 fps frame budget is 16.667 ms.

The separate device validation covered all 39 levels while moving and
floating, followed by five-second sustained walking, jumping, and inhaling
tests. Those longer tests measured 59.79, 60.16, and 60.44 fps. All 42
scenarios had zero late frames; the largest observed CPU-work time was
5.185 ms. Short windows can include a few catch-up frames when leaving
serial lockstep. The reusable checker now warms up before measuring.
Directional input, pause, resume, and the absence of recorded hardware
faults were also checked.

The diagnostic firmware used for these measurements identified as
`MYBL 1.0.0`, CRC32 `0xF3BBEEFF`; it contains the optimized renderer.
Release 1.0.1 includes that same renderer, the score formatter correction,
and updated diagnostics. The SDK's small printf does not implement `%l`,
so scores now use CHGame's integer formatter.

Evidence:

- [Original stage timings](device-baseline.txt)
- [Optimized stage timings](device-optimized.txt)
- [All 42 device scenarios and functional checks](device-performance.json)

Reproduce after installing a debug build:

```sh
python3 mysticballoon.py build --debug
python3 mysticballoon.py uploader flash MysticBalloon/build/debug/MysticBalloon.ino.bin --device rev0 --run --quiet
.venv/bin/python MysticBalloon/tools/verify_device.py --port /dev/cu.usbmodem101
```

The port's `U` debug command reports the complete CPU stages; the library's
`P` command covers only the expansion/chrome render section and is not a
measure of the whole frame. After diagnostics, restore the normal release
with `python3 mysticballoon.py upload`.

The 128×64 regression checks included 4,200 sprite cases against an independent pixel
reference, every vertical bit alignment, partial pages, all four masking
modes, all 16,384 expansion/output pixels, the full game-flow UBSan suite,
and identical simulator gameplay captures before and after optimization.
