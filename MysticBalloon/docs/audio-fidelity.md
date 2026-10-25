# Original sound audit — 2026-10-25

The released 1.2.8 audio adapter did not reproduce every pitch and duration
faithfully. Version **1.2.9** corrects playback and has passed local source,
event, timer and integration checks. The reconnected CHGame also passed 45 physical gameplay/performance
scenarios with audio enabled; electrical/acoustic measurements remain unverified.

## Findings and corrections

All 26 original `sound.tone` call sites are accounted for: 14 in gameplay
and 12 in menus. The gameplay frequency expressions, duration arguments
and trigger logic were retained. The main mismatch was below those calls:
`audio::blip()` packed frequencies into 20 Hz units and durations into 2 ms
units, and started/stopped through a 1 kHz sequencer.

| Sound | Original request | Old CHGame pitch |
|---|---:|---:|
| Walking/inhale animation | 150 Hz | 160 Hz |
| First bonus note | 131 Hz | 140 Hz |
| Confirmation | 425 Hz | 420 Hz |
| Fan/inhale wind | 320–339 Hz, twenty choices | 320 or 340 Hz |

`Tones.cpp` now uses TIM1 at 2 MHz and toggles PB10 through its update
interrupt, matching the original AVR timer clock and integer divider.
It preserves the original half-wave counter, final stop interrupt,
same-frame last-call-wins behavior, counter/pin phase on replacement and
mute semantics. Muting disconnects output while the timer continues;
enabling does not restart an old request. No sound samples or music were
added. The driver is private to this game's frequency/duration range.

ArduboyTones' duration argument is in 1/1024 seconds, with whole half-wave
rounding. The tested timer rules are:

```
divider = floor(1,000,000 / frequency)  # half-period in 2 MHz timer ticks
toggles = floor(duration * frequency / 512)
stop interrupt = toggles + 1
```

For example, `tone(131,150)` stops after 148.8435 ms from a zero counter;
`tone(80,30)` stops after 31.25 ms. A new request can interrupt either one.
Neither an unconditional millisecond duration nor a simple percentage
adjustment reproduces these edge counts.

The expanded CONF menu also had two inconsistent sounds: enabling audio
now uses the original 300 Hz click, and A/back restores the original 425 Hz
confirmation. Palette confirmation and the optional early intro skip use
the existing confirmation sound.

Periodic triggers now retain the original 16-bit frame-counter wrap. This
matters for the every-20-frames bonus sequence after frame 65,535. Wind still
requests a note every two frames; walking steps occur every sixteen frames.

Flash programming masks interrupts. Automatic saves now wait until the
short voice finishes; otherwise a save could drop edges or lengthen a tone.
SELECT preference changes remain pending until written. SAVE & QUIT waits
with the world frozen and then returns to the menu after a successful save.

## Validation

Run from the workspace root:

```sh
# First run: download public, commit-pinned, hash-verified references.
.venv/bin/python MysticBalloon/tools/verify_audio.py --fetch
# Subsequent runs need no network.
.venv/bin/python MysticBalloon/tools/verify_audio.py
python3 mysticballoon.py check
```

The original game is pinned to `58743b6b52f1aa9f7bc39efd807034a83ebed9a6`.
The repository does not specify exact Arduboy library versions. This audit
pins its explicit library references and per-file hashes in
[`tools/audio-upstream.json`](../tools/audio-upstream.json), rather than
claiming to reproduce an unidentified historical binary.

- **452 identical requests across 57 scenarios:** frequency, duration,
  frame number, mute flag and request order match between the actual
  original handlers and the port. Fixtures cover all menu sound sites,
  jumping, footsteps, landing, head bumps, inhaling, all fan directions,
  key/coin collection, walker absorption from both sides, hurt, falls and
  last-life loss. They include six coins collected in one frame, overlapping
  events, every coin/balloon bonus combination and counter wrap. Pause and
  game-over handlers remain silent.
- **180 timer cases:** the actual reference `ArduboyTones.cpp` and native
  `Tones.cpp` execute against register/GPIO stand-ins. The matrix covers all
  twenty wind frequencies, every fixed/bonus pitch and every used duration,
  plus rests. Divider, pin transitions, stop interrupts, mute/unmute,
  retained counter phase, replacement and indefinite tones match.
- **Full local check:** all 39 levels, UBSan, rendering/masking tests,
  palette/save controls and pause/save flows pass. Gameplay, menu and
  palette scripts produce 21 images identical across two runs. Flash
  deferral and deferred SELECT persistence have dedicated regressions.
- **Release compilation:** 42,332-byte firmware, 17,776 bytes static RAM,
  separate 2,048-byte stack and both save pages retained. No SDK/submodule
  files were changed. The diagnostic build also fits: 47,776-byte firmware
  and 18,028 bytes static RAM. The linked release uses the private TIM1
  handler and contains no shared `audio::` sequencer symbols.

Detailed results and source hashes: [audio-fidelity.json](audio-fidelity.json).
Regenerated traces, reference downloads and test executables remain in
ignored local output/cache directories.

## Physical validation of 1.2.9

The diagnostic identity was `MYBL 1.2.9`. All 39 levels and six sustained
scenarios passed with audio enabled: **59.76–60.60 fps**, **zero late
frames**, maximum CPU work **13.811 ms** and **1,232 bytes** stack usage.
All four palettes, movement, pause freezing and resume passed with no
recorded faults. The original level-entry checkpoint and preferences were
reloaded and matched before/after the run. See
[the device record](device-audio-1.2.9.json).

The physical controls check also passed live palette previews, paused-frame
references in all four modes, SAVE & QUIT and normal CONT with the original
checkpoint. [Controls record](device-controls-audio-1.2.9.json).

The normal 1.2.9 build was then accepted by the bootloader and launched.
Its SD-menu cartridge was updated to the exact same firmware; remount
readback verified all 39 other visible files and their timestamps unchanged,
including menu indexes, covers and Pinball data. The reader was safely
ejected. [Installation record](install-audio-1.2.9.json),
[upload log](upload-1.2.9.txt).

## Timing and physical limits

The port retains its existing **60 fps** pacing. Under budget, the original
Arduboy2 `setFrameRate(60)` schedules **16 ms** frames (62.5 fps). Therefore
matching frame triggers are about **4.17% farther apart in wall time** here:

| Trigger interval | Original nominal clock | CHGame clock |
|---|---:|---:|
| Wind: 2 frames | 32 ms | 33.332 ms |
| Footsteps: 16 frames | 256 ms | 266.656 ms |
| Bonus: 20 frames | 320 ms | 333.320 ms |

Changing this would retime the whole game, including physics. This correction
preserves the existing game speed and individual note timing. The expanded
128-row view also has its established viewport-relative fall threshold;
fall tests compare the equivalent threshold in each version.

The wind fixtures use the same seeded host RNG to isolate event logic.
Arduboy and CHGame have different physical random-number implementations;
the audible random pitch stream is not claimed to be bit-identical.

Register tests establish programmed timer values and ISR behavior. They do
not establish physical oscillator accuracy, interrupt jitter under load,
counter behavior during an in-flight divider change, electrical waveform,
speaker timbre or loudness. Electrical edge capture and a listening comparison would be needed to
establish physical waveform, jitter and acoustic equivalence beyond the
functional/performance checks recorded here.
