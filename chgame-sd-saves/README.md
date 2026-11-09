# CHSave

SD saves and settings for [CHGame](https://github.com/bateske/CHGame).
Each game gets its own file: `DEMO.SAV`, `TETR.SAV`, etc.
Two records, CRC checks and verified writes protect the previous save.

Install this repo as an Arduino library alongside CHGfx. Use `CHSave.h`:

```cpp
#include <CHSave.h>

chsave::Store saves;
constexpr auto SAVE_ID = chsave::id("DEMO"); // choose your own four-character ID
struct Data { uint32_t bestScore; uint8_t sound; uint8_t reserved[3]; };
Data data{0, 1, {0, 0, 0}};

// After chgame.boot() and gfx_begin():
// saves.load(SAVE_ID, 1, data);   // false leaves data unchanged
// When the score or settings change:
// saves.store(SAVE_ID, 1, data); // check the return value
```

Prepare a mounted FAT16/FAT32 card once (Python 3, no packages):

```sh
python3 tools/prepare.py --card /path/to/card DEMO
# For a larger save, reserve 4 KB of payload capacity:
python3 tools/prepare.py --card /path/to/card --bytes 4096 BIGG
```

Existing saves are preserved. Insert the card before starting the game.
The default reserves a 1 KB file for up to 492 bytes of data. Larger files
support variable payloads up to 65,516 bytes; available RAM is usually the limit.
Capacity is read from the file, with no size setting in the game.
Keep `.SAV` files out of cartridge assets so updates cannot replace them.
Save between frames; `saves.status()` explains failures.

See [the working example](examples/Settings/Settings.ino) and
[integration notes](docs/integration.md) for installation, errors and schema changes.
[Format](docs/format.md) · [Tests](tests/README.md) · [MIT license](LICENSE)
