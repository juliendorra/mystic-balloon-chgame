# CHSave

On the CHGame, it makes sense to switch the default of the game saved state, scores and settings storage to the SD card: the SD card will probably be always present on the CHGame console, as most game are starting to need it for artwork, levels, etc. 

Here is a small lib I use, so all my CHGame games are using a consistent way to save game state and settings.

SD saves and settings for [CHGame](https://github.com/bateske/CHGame).
Each app uses a reverse-domain ID and its own hashed `.SAV` filename.
Two records, CRC checks and verified writes protect the previous save.

Install this repo as an Arduino library alongside CHGfx. Use `CHSave.h`:

```cpp
#include <CHSave.h>

chsave::Store saves;
constexpr auto SAVE_ID = chsave::id("com.example.game"); // keep this stable across releases
struct Data { uint32_t bestScore; uint8_t sound; uint8_t reserved[3]; };
Data data{0, 1, {0, 0, 0}};

// After chgame.boot() and gfx_begin():
// saves.load(SAVE_ID, 1, data);   // false leaves data unchanged
// When the score or settings change:
// saves.store(SAVE_ID, 1, data); // check the return value
```

Prepare a mounted FAT16/FAT32 card once (Python 3, no packages):

```sh
python3 tools/prepare.py --card /path/to/card com.example.game
# For a larger save, reserve 4 KB of payload capacity:
python3 tools/prepare.py --card /path/to/card --bytes 4096 com.example.biggergame
```

Existing saves are preserved. Insert the card before starting the game.
The default reserves a 1.5 KB file for up to 492 bytes of data. Larger files
support variable payloads up to 65,516 bytes; available RAM is usually the limit.
Capacity is read from the file, with no size setting in the game.
Keep `.SAV` files out of cartridge assets so updates cannot replace them.
Save between frames; `saves.status()` explains failures.

Filenames use eight base-36 characters (`0–9`, `A–Z`). The full ID is checked
before writing, so a hash collision cannot overwrite another app's data.
Old four-character journals need an [explicit conversion](docs/integration.md#upgrading-from-01);
firmware has no legacy fallback.

## Games using CHSave

- [Tetris](https://github.com/juliendorra/tetris-chgame)
- [Midnight Pinball](https://github.com/juliendorra/chgame-pinball)
- [VoidBloom](https://github.com/juliendorra/VoidBloom)
- [LightDrive](https://github.com/juliendorra/light-drive)
- [Mystic Balloon](https://github.com/juliendorra/mystic-balloon-chgame)

See [the working example](examples/Settings/Settings.ino) and
[integration notes](docs/integration.md) for installation, errors and schema changes.
[Format](docs/format.md) · [Tests](tests/README.md) · [MIT license](LICENSE)
