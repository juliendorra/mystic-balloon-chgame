# Mystic Balloon for CHGame

I asked my daughter what Arduboy game she would want to play on the CHGame
Rev0, and she said Mystic Balloon! So here is a **128×128, 60 fps color
port** of TEAM a.r.g.'s **Mystic Balloon**.

Explore all 39 original levels with the original physics, bright
red/yellow/blue balloons and a continuous splash illustration.

**[Download version 1.2.9](https://github.com/juliendorra/mystic-balloon-chgame/releases/tag/v1.2.9)**:
[.chgame package (recommended)](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.2.9/MysticBalloon-1.2.9.chgame)
or [.bin firmware](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.2.9/MysticBalloon-1.2.9-rev0.bin).
The release includes USB and SD-card installation instructions and
[SHA-256 checksums](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.2.9/SHA256SUMS.txt).
No compiling needed.

| Color — Pastel Island (default) | Original B&W |
|:---:|:---:|
| ![Mystic Balloon in Color](MysticBalloon/docs/gameplay.gif) | ![Mystic Balloon in Original B&W](MysticBalloon/docs/gameplay-original.gif) |

| Game Boy DMG | Amber |
|:---:|:---:|
| ![Mystic Balloon in Game Boy DMG](MysticBalloon/docs/gameplay-dmg.gif) | ![Mystic Balloon in Amber](MysticBalloon/docs/gameplay-amber.gif) |

The same title screen and gameplay sequence are shown in all four modes.

The default **Pastel Island** color palette is inspired by Yoshi's Island.
CONF also offers **Original B&W**, **Game Boy DMG** and **Amber**, with live
previews. START pauses; **SAVE & QUIT** returns to the main menu and keeps
the level-entry checkpoint. [Controls and game rules](MysticBalloon/README.md).

## Build and upload

Install Python 3.12+, Git and [Arduino CLI](https://arduino.github.io/arduino-cli/).
The simulator also needs a C++ compiler: Clang or GCC.

```sh
git clone https://github.com/juliendorra/mystic-balloon-chgame.git
cd mystic-balloon-chgame
python3.12 mysticballoon.py setup
python3.12 mysticballoon.py check
python3.12 mysticballoon.py upload
```

Setup downloads the external [CHGame SDK](https://github.com/bateske/CHGame)
at commit `e1f8d05940ddb7bbef89c13326da75c003f02904` and installs board
package **0.3.0** plus the pinned Python dependencies. The SDK and downloaded
tools are ignored by Git. This repository contains only Mystic Balloon.
On Apple Silicon, the board package's bundled compiler may require Rosetta.

For simulator checks without installing the Arduino board package:

```sh
python3.12 mysticballoon.py setup --host-only
python3.12 mysticballoon.py check --no-device
```

In Arduino IDE, install the CHGame board package following its
[installation guide](https://github.com/bateske/CHGame#-get-started), open
`MysticBalloon/MysticBalloon.ino`, and select **CHGame Rev0**, **Smallest + LTO**,
and **Upload only**.

## Package and inspect

```sh
python3.12 mysticballoon.py export
python3.12 mysticballoon.py cart verify MysticBalloon/build/MysticBalloon.chgame
python3.12 mysticballoon.py gif
```

The cartridge is `MysticBalloon/build/MysticBalloon.chgame`; the firmware
is `MysticBalloon/build/release/MysticBalloon.ino.bin`. Build products stay
local. The [art sources](MysticBalloon/tools/art/splash/README.md),
[pixel drawing workflow](MysticBalloon/docs/pixel-art-workflow.md), and
[Arduboy porting guide](docs/arduboy-porting.md) explain the port's methods.

## Validation

Release **1.2.9** corrects original audio pitches, half-wave durations,
menu clicks and long-run sound timing. It passes **452 matching sound events**
and **180 timer cases**, plus the full local game checks. See the
[audio audit](MysticBalloon/docs/audio-fidelity.md). Both downloads above
include these fixes. On physical CHGame Rev0, all 39 levels and six sustained
scenarios passed with audio enabled at approximately **60 fps with zero late
frames**; worst CPU work was **13.811 ms** within the 16.667 ms frame budget. Controls,
SAVE & QUIT and retained checkpoints also passed. The normal 1.2.9 firmware
and SD-menu copy were installed and verified. See the
[installation record](MysticBalloon/docs/install-audio-1.2.9.json).
Electrical/audio capture remains pending.

Rendering checks cover 12 border/ascent
fixtures, all 64 player/inhale canvases and saved-preference restoration.
The host suite includes 1,716 platform references and 320 player composites.

See the [validation record](MysticBalloon/docs/platform-composite-checks.json),
[performance measurements](MysticBalloon/docs/performance.md), and
[port notes](MysticBalloon/NOTES.md). Exact 1.2.9 release firmware: **42,332 bytes**,
CRC32 **`0x863402A6`**, built with native GCC 16.2.0. Binary
identities depend on the recorded compiler; a different toolchain may
produce different bytes. Native compiler development is outside this repo.

## License and credits

[MIT](LICENSE). Original Mystic Balloon (Arduboy 1.7.2), copyright © 2016
TEAM a.r.g.; credits to GAVENO, CastPixel, JO3RI and Martian220, with thanks
to MLXXXP for the Arduboy2 migration. The original sprite and level arrays
match [upstream commit 58743b6](https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon/tree/58743b6b52f1aa9f7bc39efd807034a83ebed9a6).
See [NOTICE](NOTICE) for the adaptation and asset provenance.
