# Mystic Balloon performance on CHGame Rev0

The [Arduboy porting guide](../../docs/arduboy-porting.md) explains how to
apply these rendering and validation techniques to another game.

## Native 128×128 release 1.1.0

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
