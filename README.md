# Mystic Balloon for CHGame

I asked my daughter what Arduboy game she would want to play on the CHGame
Rev0, and she said Mystic Balloon! So here is a **128×128, 60 fps color
port** of TEAM a.r.g.'s **Mystic Balloon**.

Explore all 39 original levels with the original physics, bright
red/yellow/blue balloons and a continuous splash illustration.

**[Download version 1.4.1](https://github.com/juliendorra/mystic-balloon-chgame/releases/tag/v1.4.1)**:
[.chgame package (recommended)](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.4.1/MysticBalloon-1.4.1.chgame)
or [.bin firmware](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.4.1/MysticBalloon-1.4.1-rev0.bin).
The release includes USB and SD-card installation instructions and
[SHA-256 checksums](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.4.1/SHA256SUMS.txt).
No compiling needed.

Version **1.4.1** restores **RESTART LEVEL** to the START pause menu,
alongside RESUME and SAVE & QUIT. Restart returns the current run to its
level entrance with fresh coins, key, enemies and three balloons. It rolls
back that attempt's score and campaign coins, without changing other runs.

All menus now use **A to accept** and **B to go back or cancel**, including
CONF, saved runs, pause and level-start prompts. B wins simultaneous A+B
presses. Gameplay keeps **B for jumping/floating** and **A for inhaling**.
Button hints match the controls. Existing 1.4.0 saves migrate automatically;
the saved score becomes their initial restart baseline because the old
format did not record the level-entry score. New entry scores persist
through mid-level saves and loads.

Saved runs, introduced in **1.4.0**, retain independent progress and mid-level resume. **PLAY**
shows **NEW**, an empty line, then your runs labelled by **level and percentage**.
The last played run is selected automatically; use Up/Down and **A to load**.
NEW keeps existing progress. HELP now explains controls, loading and saving. With an SD card, eight runs live in
`MYBLSAVE.DAT`, isolated from other games. Without SD, four runs use the
shared flash fallback. Pause, then **SAVE & QUIT**, to resume at the same
position with your coins, key, balloons and enemy state restored.
[Save-system behavior and checks](MysticBalloon/docs/save-system.md).

The ATMlib soundtrack plays the title tune, five gameplay tunes and
tracker effects by default. In **CONF**, Left/Right changes **AUDIO ATM/BEEP**
and **SOUND ON/OFF** immediately. Sound rows preview the selected audio
and show its current state, using the confirmed color scheme. A saves;
B cancels all previews. SELECT mutes either mode. Existing checkpoints
and palettes are retained.
[Soundtrack implementation and checks](MysticBalloon/docs/atmlib-soundtrack.md).

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
your current run. [Controls and game rules](MysticBalloon/README.md).

## Install 1.4.1 and prepare SD saves

Download the [.chgame package](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.4.1/MysticBalloon-1.4.1.chgame)
and install it using the CHGame cart tools. The [.bin firmware](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.4.1/MysticBalloon-1.4.1-rev0.bin)
is available for direct USB flashing to CHGame Rev0. Both contain the same
normal release firmware, with both flash save pages preserved.

For SD saves, open the console's SD card reader and prepare the card once
with the standalone [prepare_saves.py helper](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.4.1/prepare_saves.py)
(Python 3; no extra dependencies):

```sh
python3 prepare_saves.py --card "/path/to/mounted/card"
```

Replace the path with your actual SD mount, such as `/Volumes/NO NAME` on
macOS. The helper creates a missing `MYBLSAVE.DAT`, preserves valid existing
runs, and refuses to replace damaged saves. Safely eject the card before
starting the game. An existing valid legacy flash checkpoint imports into
the first run; its old format only retained the level entrance.
Back up `MYBLSAVE.DAT` to keep all your runs.

From a source checkout, the launcher also prepares saves automatically:

```sh
python3.12 mysticballoon.py cart deploy MysticBalloon-1.4.1.chgame --card "/path/to/mounted/card"
```

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

The 1.4.1 deterministic host suite checks restarting after a mid-level load,
score/coin rollback without farming, failed-save recovery, unchanged other
runs, and migration of all eight SD or four flash runs from 1.4.0. It also
checks all 512 interrupted sector-write positions, four standalone
provisioning/recovery cases, A accept / B back throughout menus and
simultaneous-button safety. UI layouts are reviewed in all four palettes.
All 39 original maps and sprite fingerprints remain unchanged.

Focused physical CHGame checks cover CONF accept/cancel, NEW, level-start
back, real coin collection, exact mid-level reload, RESTART LEVEL, and
simultaneous A+B safety. Existing saved runs are backed up before tests;
only a temporary NEW run is used for gameplay. It is removed afterwards,
restoring the original runs and preferences. A restarted level-1 sample
rendered 60 frames in 1.005 seconds with zero late frames and a 1,520-byte
stack watermark. See the
[device checks](MysticBalloon/docs/device-controls-1.4.1.json) and
[installation record](MysticBalloon/docs/install-controls-1.4.1.json).

The normal release firmware is **50,100 bytes**, CRC32 **`0xA0F74C53`**,
with **18,124 bytes** static RAM, a separate **2,048-byte** stack and both
flash-save pages retained. The `.chgame` and `.bin` downloads contain the
same firmware. The bootloader checks its programmed CRC; bootloader v3
does not support independent byte readback.

The audio checks retain **15,303 matching ATM tracker states**, six PCM
renders, **452 matching classic sound events** and **180 timer cases**.
The viewport survey covers **17,199 positions**, 1,716 platform references
and 320 player composites. The older 1.3.0 45-scenario physical audio
benchmark is historical. SD-controller power-loss behavior and overnight
electrical retention are not measured by software failure injection.

## License and credits

Game and scores: [MIT](LICENSE). ATMlib: [BSD 3-Clause](MysticBalloon/ATMLIB-LICENSE).
The private CHSd-derived save driver is [MIT licensed](MysticBalloon/SAVE-CARD-LICENSE).
Original Mystic Balloon (Arduboy 1.7.2), copyright © 2016
TEAM a.r.g.; credits to GAVENO, CastPixel, JO3RI and Martian220, with thanks
to MLXXXP for the Arduboy2 migration. The original sprite and level arrays
match [upstream commit 58743b6](https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon/tree/58743b6b52f1aa9f7bc39efd807034a83ebed9a6).
See [NOTICE](NOTICE) for the adaptation and asset provenance.
