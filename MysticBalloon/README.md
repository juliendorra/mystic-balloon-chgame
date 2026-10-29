# Mystic Balloon

Find the key and float to the exit through 39 levels of fans, spikes, and walking enemies. This CHGame port preserves TEAM a.r.g.'s sprite shapes and physics, with a native 128×128 scrolling view and the Pastel Island color palette, inspired by Yoshi's Island.

![Mystic Balloon gameplay](docs/gameplay.gif)

## Controls

| Button | Action |
|---|---|
| Left / Right | Move |
| Up / Down | Look vertically; Up enters a door after finding the key |
| B | Jump; press again in the air, then hold, to float with balloons; back in menus |
| A | Inhale enemies and nearby coins; accept in menus |
| START | Pause / resume |
| SELECT | Turn sound on / off |
| Up / Down, A while paused | Select and confirm RESUME, RESTART LEVEL, or SAVE & QUIT |
| B while paused | Back to gameplay without executing the selected action |
| D-pad, A, B in menus | Choose, accept, back; B never accepts a main-menu item |

## Rules

Start each level with three balloons. Spikes and enemies pop balloons; losing all three ends the attempt. Inhaling an enemy restores a balloon, up to three. While floating, fans push you up or sideways. Find the key, then press Up at the door to finish the level.

Most levels have six coins; level 23 has four in the original map. Each coin awards 200 points, with 500 for the last coin in a level. Finishing adds 20 per collected coin and 30 per remaining balloon. Absorbing an enemy awards 50 points, plus 100 if you already have three balloons.

## How to play

Choose **PLAY** to see **NEW** at the top, an empty line, then the saved runs, named by their level and percentage, such as **LEVEL 16 0%**. The last played run is preselected. Use **Up/Down** to choose and **A** to load; A on NEW creates a separate run and keeps the others. B returns to the main menu. There are eight slots on SD, or four in flash; a full list refuses NEW instead of overwriting progress. The percentage counts the level's coins and key. Loading a mid-level run opens paused, ready for RESUME.

**HELP** shows the controls, door/key rules, loading runs, and SAVE & QUIT. A acknowledges the page; B goes back.

**CONF** still previews COLOR, ORIGINAL, DMG GREEN and AMBER with Up/Down. Sound rows restore the confirmed palette. Left/Right changes **AUDIO ATM/BEEP** and **SOUND ON/OFF**, with audible previews. A saves; B cancels all previews. SELECT mutes either mode.

Version **1.4.0** saves full gameplay state to **MYBLSAVE.DAT** on the SD card by default. Pause/SAVE & QUIT retains your actual position, coins, key, balloons and enemy state. The old level-entry checkpoint is migrated into the first run. See the [save-system behavior, setup and checks](docs/save-system.md). The installer prepares the journal without replacing existing saves.

Version **1.3.0** includes the ATMlib edition's title tune, five gameplay tunes and fourteen tracker effects. The gameplay tunes rotate by level; music continues during pause. **AUDIO BEEP** restores the faithful, single-voice 1.2.9 sound without background music. SELECT mutes either mode. Existing saves keep their progress, palette and sound setting, and default to ATM music. See the [soundtrack implementation and checks](docs/atmlib-soundtrack.md).

Press START to pause. **RESTART LEVEL** (added in **1.4.1**) returns the active run to its current level's entrance with three balloons, fresh enemies, coins and key. It rolls back that attempt's score and campaign coins, preserves the other runs, and commits the restart before changing the paused world. **SAVE & QUIT** still keeps the exact mid-level state. Press A to execute the highlighted action; B or START resumes gameplay. At a level-start prompt, A starts playing and B returns to the main menu. If A and B are pressed together, back takes priority. Death keeps the last playable save. The simulator can use a persistent SD journal with `CHMB_SAVE_CARD=/path/to/MYBLSAVE.DAT`; its flash stand-in lasts only for the current process.

Gameplay fills all 128×128 pixels at the original pixel scale, including the original HUD and pause badge. Red, yellow and blue balloons match the lives display; the character has a light peach face against a soft cyan sky and mint terrain. The splash is one continuous 128×128 illustration extending the original child, balloon and enemy composition, with the original title lettering. The camera scrolls within the 384×384 levels. Added control/status panels remain removed.

The camera follows beyond the map's sides to show its boundary walls and scrolls above the top during balloon flight, matching the original game. Only its bottom position is capped. See the [scrolling correction](docs/scrolling-review.png).

Player and inhale sprites use clean frame-specific color masks, without the original clearing padding being tinted. In Amber, the whole body uses the same light fill as the head, with dark outlines and details. [Amber before/after](docs/amber-player-fix.png).

See the [four full-color studies](docs/palette-study.md), including levels 1, 6, 16 and 39 and complete sprite sheets in each palette.

From this workspace, build and upload with `python3 mysticballoon.py upload`. The shareable `build/MysticBalloon.chgame` can also be deployed to the handheld's SD card using the CHGame cart tools. In Arduino, open `MysticBalloon.ino`, select **CHGame Rev0**, **Smallest + LTO**, and **Upload only**. See [CHGame installation](https://github.com/bateske/CHGame#-get-started).

## Developer notes

- `Port.cpp` preserves the 2 KiB monochrome buffer and records compact sprite color commands. After the previous DMA finishes, packed expansion and terrain spans fill CHGfx's framebuffer, then `Color.cpp` replays color masks in vertical-byte groups.
- Packed drawing, expansion and color loops run from SRAM. Full-screen play transfers all 128 LCD rows. Real-device performance measurements are in [docs/performance.md](docs/performance.md).
- Clean player color masks are generated into flash. A 48-byte source backdrop preserves the sky/terrain beneath the original clearing mask; Original B&W keeps the original monochrome result.
- Platform corner masks reveal the actual scrolling background in Color, DMG and Amber while preserving joined edges. See the [corner comparison](docs/platform-corners.png) and [joined platforms](docs/platform-joins.png).
- Pause retains the frozen native world and replays only its opaque action panel; this avoids rebuilding the world while the LCD transfer is in flight.
- `MysticBalloon.ino` uses native CHGame input, frame pacing, debug protocol, and pause controls. The game runs at 60 logic frames per second.
- `Port.cpp` saves with the unique `MYBL` flash magic, batches writes, and retains both save pages.
- **AUDIO BEEP** retains the [1.2.9 audio correction](docs/audio-fidelity.md): ArduboyTones pitches, half-wave durations and interruption rules. Flash saves wait for effects, then briefly suspend PCM while writing so a looping soundtrack cannot block saves.
- The original game code lives in the adjacent headers. `bitmaps.h` retains the upstream artwork and level bytes; `tools/upstream-assets.json` records their fingerprints.
- See [NOTES.md](NOTES.md) for build, tests, packaging, and validation limits.
- The [splash drawing sources and workflow](tools/art/splash/README.md) include the extracted original, completed drawing and native menu review in all four display modes.
- The [global splash review](docs/splash-global-review.png) checks the complete composition at native size and 3×, including tight title masking and clear foreground/menu boundaries.
- The [motion correction](docs/splash-motion-review.png) restores a large angled balloon and a taut string from knot to grip, checked across the whole frame.
- Eye corrections use the [direct pixel draw/check/redraw loop](docs/pixel-art-workflow.md): compact text stencils, indexed PNGs and exact difference previews.

## Credits

Mystic Balloon (Arduboy 1.7.2), copyright (c) 2016 TEAM a.r.g., is licensed under the [MIT License](LICENSE). Original credits: GAVENO, CastPixel, JO3RI, Martian220 (2016–2018); thanks to MLXXXP for the original migration to Arduboy2. See [NOTICE](NOTICE) and the [original source](https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon).

Music and tracker effects come from [T-arg's ATMlib edition](https://github.com/T-arg/ID-34-Mystic-Balloon), also MIT licensed. ATMlib / Squawk by Davey Taylor (STG), Joeri Gantois (JO3RI) and TEAM a.r.g. is [BSD 3-Clause](ATMLIB-LICENSE). Its RISC-V backend reuses the work from our 8-bit soundtrack demo, with the original ATM pulse/square/saw waveforms.
