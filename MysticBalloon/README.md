# Mystic Balloon

Find the key and float to the exit through 39 levels of fans, spikes, and walking enemies. This CHGame port preserves TEAM a.r.g.'s Arduboy artwork and physics, with a native 128×128 scrolling view that reveals more of each level.

![Mystic Balloon gameplay](docs/gameplay.gif)

## Controls

| Button | Action |
|---|---|
| Left / Right | Move |
| Up / Down | Look vertically; Up enters a door after finding the key |
| B | Jump; press again in the air, then hold, to float with balloons |
| A | Inhale enemies and nearby coins |
| START | Pause / resume |
| SELECT | Turn sound on / off |
| D-pad, B, A in menus | Choose, confirm, back; either action button selects on the main menu |
| Down + A | Original pause shortcut; release, then A or B resumes |

## Rules

Start each level with three balloons. Spikes and enemies pop balloons; losing all three ends the attempt. Inhaling an enemy restores a balloon, up to three. While floating, fans push you up or sideways. Find the key, then press Up at the door to finish the level.

Most levels have six coins; level 23 has four in the original map. Each coin awards 200 points, with 500 for the last coin in a level. Finishing adds 20 per collected coin and 30 per remaining balloon. Absorbing an enemy awards 50 points, plus 100 if you already have three balloons.

## How to play

Choose **PLAY**, then **NEW** to start or **CONT** to resume the saved level. **INFO** shows the high score, and **CONF** sets sound. Progress is saved after the end-of-level bonus tally; continuing restarts that level with three balloons. High scores and the sound setting also survive power cycles. The simulator keeps saves only for the current process.

Gameplay fills all 128×128 pixels at the original pixel scale, including the original HUD and pause badge. The camera scrolls within the 384×384 levels. Original 128×64 menu artwork is centered vertically. The added title, control, status and help panels have been removed.

From this workspace, build and upload with `python3 mysticballoon.py upload`. The shareable `build/MysticBalloon.chgame` can also be deployed to the handheld's SD card using the CHGame cart tools. In Arduino, open `MysticBalloon.ino`, select **CHGame Rev0**, **Smallest + LTO**, and **Upload only**. See [CHGame installation](https://github.com/bateske/CHGame#-get-started).

## Developer notes

- `Port.cpp` maps the original sprite modes to a 2 KiB monochrome buffer, then expands it into CHGfx's framebuffer after the previous DMA transfer finishes.
- The packed sprite and expansion loops run from SRAM. Full-screen play transfers all 128 LCD rows. Real-device performance measurements are in [docs/performance.md](docs/performance.md).
- `MysticBalloon.ino` uses native CHGame input, frame pacing, debug protocol, and pause controls. The game runs at 60 logic frames per second.
- `Port.cpp` saves with the unique `MYBL` flash magic, batches writes, and retains both save pages.
- The original game code lives in the adjacent headers. `bitmaps.h` retains the upstream artwork and level bytes; `tools/upstream-assets.json` records their fingerprints.
- See [NOTES.md](NOTES.md) for build, tests, packaging, and validation limits.

## Credits

Mystic Balloon (Arduboy 1.7.2), copyright (c) 2016 TEAM a.r.g., is licensed under the [MIT License](LICENSE). Original credits: GAVENO, CastPixel, JO3RI, Martian220 (2016–2018); thanks to MLXXXP for the original migration to Arduboy2. See [NOTICE](NOTICE) and the [original source](https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon).
