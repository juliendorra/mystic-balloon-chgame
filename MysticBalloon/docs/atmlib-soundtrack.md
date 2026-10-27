# ATMlib soundtrack — 1.3.0

The default audio is imported from [T-arg's Mystic Balloon](https://github.com/T-arg/ID-34-Mystic-Balloon/tree/87354104ee5b027e1e4b1c755de1b1be76e091b9),
commit `87354104ee5b027e1e4b1c755de1b1be76e091b9`. Its unmodified score and
configuration are preserved under `vendor/MysticBalloon/`; source hashes are
in `tools/soundtrack-upstream.json`. The original 39 levels and gameplay
remain from the port's existing source; the newer edition's other game,
menu and level changes are not imported.

## What plays

The title/menu plays **Arabian Night**. The five gameplay scores rotate by
level: **Sphinx Sands**, **Nile Caravan**, **Pharaoh's Hall**, **Oasis Dance**
and **Pyramid Chase**. The source chooses randomly at level entry; the port
rotates to make every score available and diagnostics reproducible. Music
continues through pause, as in that edition. Inter-level tally and game-over
use its silent score so effects can continue without music.

All fourteen source effects are retained: move, select, jump, step, land,
wind, hurt, key, coin, pop and four rising tally notes. Effects replace the
single SFX slot on channel zero while the other music channels continue.
Each existing game trigger names its semantic effect; its two legacy tone
arguments are unchanged. This distinguishes jump from enemy absorption and
key from coin, which shared the same beep in the old edition.

CONF's **ATM MUSIC / BEEP SFX** row switches both music and effects. **BEEP
SFX** uses the actual 1.2.9 ArduboyTones-compatible GPIO driver, with no
background music. **SOUND ON/OFF** and SELECT apply to either mode.

The 12-byte save layout and schema remain unchanged. Options bit 3 selects
classic beeps; zero selects ATM. Old records therefore default to the new
soundtrack while preserving sound enable, palette, scores and checkpoint.
Changing a palette preserves the audio bit. Confirmed choices are saved;
palette previews remain temporary.

## Reusing the 8-bit soundtrack port

ATMlib is the same pinned BSD source used by our ATM Soundtracks demo:
`T-arg/ATMlib`, commit `8fdabd2fd81082bff50e8af730cae4feff2cce99`.
The reproducible `tools/port_atmlib.py` keeps its 16-bit offsets, bytewise
unaligned reads, explicit signed effects, VLE pointer fix and volume clamps.
Its note table is calibrated for A440 at 31,250 samples/second.

This game's configuration enables the original SFX overlay and retains
the arpeggio used in its title. The independent reference test reaches a
maximum call-stack depth of two, which is reserved for all five tracker
states (four music channels and the SFX slot).

The mixer retains ATM's 25% pulse, 50% square and half-amplitude saw. The
NES demo's triangle substitution and its arranged scores are not used.
TIM1 outputs 187.5 kHz PWM on PB10; TIM3 supplies 31,250 PCM samples/second.
The short mixer and sampler ISR run from SRAM. Tracker work runs on the
lower-priority 1 kHz SysTick, publishing complete oscillator controls through
a double buffer so sampling can interrupt it safely.

Tracker timing uses the score's original integer `cia = 15625 / tempo`
divider. Scheduling on SysTick adds up to one millisecond of tick
quantization. Classic frame-triggered effects retain the established 60 fps
pacing documented in the [original tone audit](audio-fidelity.md).

Switching modes releases the outgoing timer, pending interrupt and pin
configuration before starting the other engine. No shared SDK audio engine
is linked. Saving waits for effects rather than for endlessly looping music;
PCM and tracker time are briefly suspended around the interrupt-masked flash
write, with neutral PWM output, then resumed.
Confirmed preferences and completed level checkpoints stay pending even if
the player leaves the menu or level prompt before an effect finishes. A
partial bonus tally is never written as a completed checkpoint.

## Space and rendering

The unchanged monochrome splash pixels are stored one bit per pixel before
RLE, shrinking from 4,020 to 1,764 bytes. Its production rendering is checked
against all 16,384 source pixels. Color artwork is unchanged.

The sprite queue retains its full 128-command capacity. A two-byte reference
covers application flash (`0x3000..0xFFFF`) and the SRAM constant pool
(`0x20000010..0x20002FFF`), reducing each command from eight to six bytes.
The packed addresses below `0x3000` identify SRAM; that flash range belongs
to the bootloader and contains no game sprites. This matters because the
RISC-V linker places small immutable particles and menu cursors in `.srodata`
in SRAM. Host fixtures retain full native pointers. Native queuing rejects
unsupported addresses, and a round-trip test covers both spaces. The survey checks
17,199 viewport positions across all 39 levels; the peak is 104 commands,
with no dropped commands.

The release image is **44,476 bytes**, with **18,140 bytes** static RAM.
The diagnostic image is **50,028 bytes**, with **18,404 bytes** static RAM.
Both reserve a separate 2,048-byte stack and retain both save pages.
The package contains the exact release binary plus both MIT and BSD license
texts. [Build identities and check results](soundtrack-checks.json).

## Reproduce the checks

```sh
python3 MysticBalloon/tools/import_soundtrack.py --check
python3 MysticBalloon/tools/port_atmlib.py
python3 MysticBalloon/tools/verify_soundtrack.py
python3 mysticballoon.py check
# Original beep audit uses separately cached, hash-verified references:
python3 MysticBalloon/tools/verify_audio.py --fetch
```

The soundtrack check compiles the untouched upstream interpreter against
AVR register stand-ins and compares it with the actual port. It checks
**15,303 identical tracker states**, including all six tunes, silence,
arpeggios, note/volume progression, repeated SFX replacement, recovery of the
music channel and all four tally notes. Six complete PCM WAVs under
`out/soundtrack/` check the actual mixer, sample rate and clipping. Additional
checks cover mute, flash suspension, resume and driver mode changes.

The game integration suite checks all 16 saved option combinations, migration,
CONF switches both ways, palette preview, looping menu music, pause and
SAVE & QUIT with ATM audio. The classic mode still passes **452 sound events
across 57 scenarios** and **180 actual timer-driver cases**. Undefined-behavior
sanitization is enabled. The normal `check` command includes the soundtrack
suite and the existing game/render checks.

Host traces and WAVs verify the digital engine; the physical checks below
establish timing and functionality on the handheld. Electrical waveform
capture and an acoustic comparison have not been performed.

## Physical CHGame Rev0 checks

All **45** gameplay/performance scenarios pass with the default ATM audio:
**59.95–61.27 fps** within USB measurement tolerance, **zero late frames**,
maximum CPU work **15.966 ms**, and a **1,328-byte** stack watermark. All five
gameplay tunes ran, and every color overlay was retained. A hardware-only
sampler counter confirms approximately **31,250 samples/second**. It is
compiled out of the normal release.

Live palette previews, both audio choices and their flash reloads, frozen
pause pixels in all four palettes, SAVE & QUIT and CONT also pass. The
original checkpoint, palette and sound preference are retained. Records:
[gameplay/performance](device-audio-1.3.0.json),
[controls](device-controls-audio-1.3.0.json),
[audio modes](device-audio-modes-1.3.0.json).

The normal release and SD-menu cartridge are installed and verified. The
checkpoint and preferences are retained; remount/readback verifies the
40 other visible card files and their timestamps unchanged. The card was
safely ejected. [Installation record](install-audio-1.3.0.json).

The diagnostic uses plain 32-bit `%u` fields for counters: the device's
minimal formatter did not correctly emit the initial `%lu` tick field.
Pause comparisons exclude music ticks and sampler counts because the score
continues while the world is frozen.

## Actual RISC-V firmware in the emulator

Build the diagnostic firmware, then run the optional native-core check:

```sh
python3 mysticballoon.py build --debug
python3 MysticBalloon/tools/verify_soundtrack_emulator.py --emulator /path/to/chg_headless
```

The headless emulator must route TIM3 to IRQ **54**. The earlier 8-bit demo
found and reported an incorrect IRQ-60 mapping. A nonzero PWM audio peak
alone is insufficient evidence that the sampler runs; this check reads
the real firmware's tracker counter and changing oscillator phases.

The measured gameplay window runs at **60.00 fps**, with the sampler and
tracker advancing. CONF switches from ATM to beeps and back without faults;
the saved beep choice survives an emulated reboot. Results and the exact
firmware/emulator hashes are in [soundtrack-emulator.json](soundtrack-emulator.json).
Physical timing and controls are checked separately above; electrical and
acoustic equivalence remain unmeasured.
