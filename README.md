# Mystic Balloon for CHGame

I asked my daughter what Arduboy game she would want to play on the CHGame
Rev0, and she said Mystic Balloon! So here is a **128×128, 60 fps color
port** of TEAM a.r.g.'s **Mystic Balloon**.

Explore all 39 original levels with the original physics, bright
red/yellow/blue balloons and a continuous splash illustration.

**[Download version 1.3.0](https://github.com/juliendorra/mystic-balloon-chgame/releases/tag/v1.3.0)**:
[.chgame package (recommended)](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.3.0/MysticBalloon-1.3.0.chgame)
or [.bin firmware](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.3.0/MysticBalloon-1.3.0-rev0.bin).
The release includes USB and SD-card installation instructions and
[SHA-256 checksums](https://github.com/juliendorra/mystic-balloon-chgame/releases/download/v1.3.0/SHA256SUMS.txt).
No compiling needed.

Version **1.3.0** plays the ATMlib title tune, five gameplay tunes and
tracker effects by default. In **CONF**, Left/Right changes **AUDIO ATM/BEEP**
and **SOUND ON/OFF** immediately. Sound rows preview the selected audio
and show its current state, using the confirmed color scheme. B saves;
A cancels all previews. SELECT mutes either mode. Existing checkpoints
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

On physical CHGame Rev0, all **45 gameplay/performance scenarios** passed
with ATM audio enabled at approximately **60 fps with zero late frames**
and no dropped sprites. All five gameplay tunes ran. Maximum CPU work was
**15.966 ms** within the 16.667 ms frame budget, with a **1,328-byte** stack
watermark. Live palette previews, paused pixels in all four display modes,
SAVE & QUIT, CONT and both saved audio choices passed. A sampler counter
confirms approximately 31,250 PCM samples/second.

The original and native ATM interpreters produce **15,303 matching tracker
states**, with six non-clipping PCM renders. Classic mode retains **452
matching sound events** and **180 timer cases**. A survey of **17,199 viewport
positions** across all 39 levels peaks at 104 of 128 sprite commands, with
no drops. The host suite also checks 1,716 platform references and 320 player
composites. Actual RISC-V firmware passes the native emulator's mode/reboot
checks and a measured 60 fps gameplay window.

The final CONF controls pass focused physical tests for live changes,
mute, save/cancel and checkpoint retention. The gameplay benchmark above
predates that menu refinement; gameplay, audio and rendering are unchanged.
See the [build/check record](MysticBalloon/docs/soundtrack-checks.json),
[device results](MysticBalloon/docs/device-audio-1.3.0.json), and
[CONF checks](MysticBalloon/docs/device-audio-modes-1.3.0.json).
Electrical waveform and acoustic equivalence remain unmeasured.

Exact release firmware: **44,568 bytes**, CRC32 **`0x17FA0C82`**, built with
native GCC 16.2.0. Static RAM is **18,140 bytes**, with a separate 2,048-byte
stack and both save pages retained. Binary identities depend on the recorded
compiler. See the [port notes](MysticBalloon/NOTES.md) and
[original tone audit](MysticBalloon/docs/audio-fidelity.md).

## License and credits

Game and scores: [MIT](LICENSE). ATMlib: [BSD 3-Clause](MysticBalloon/ATMLIB-LICENSE).
Original Mystic Balloon (Arduboy 1.7.2), copyright © 2016
TEAM a.r.g.; credits to GAVENO, CastPixel, JO3RI and Martian220, with thanks
to MLXXXP for the Arduboy2 migration. The original sprite and level arrays
match [upstream commit 58743b6](https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon/tree/58743b6b52f1aa9f7bc39efd807034a83ebed9a6).
See [NOTICE](NOTICE) for the adaptation and asset provenance.
