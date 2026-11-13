# CHSave

SD saves and settings for [CHGame](https://github.com/bateske/CHGame).
Each save has a stable reverse-domain ID and a hashed `.SAV` filename.
Two records, CRC checks and verified writes protect the previous save.

It makes sense for games that load artwork or levels from SD to keep their saves on the same card.
But it is also a good solution for all CHGame games, as it allows bigger save records and avoid collision by using hashed  reverse-domain IDs.

Trade-off: CHSave uses more firmware space for its code than the built-in flash save API.

Install CHSave and the included `extras/CHSd` as Arduino libraries alongside
CHGfx. Use the included CHSd in place of the stock CHSd.
CHSave selects the compact writer by default. CHSd's general writer is
available separately for games that create larger files.

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

On a FAT16/FAT32 card, the first `store()` creates the file in `/SAVES/`.
If the file is missing, `load()` returns `false` with status `Empty` and
leaves the game's initialized defaults in place. Writes are synchronous.
We recommend calling `store()` between frames and checking `status()` on failure.

The default save size is **492 bytes of game data per file**. The file takes
three 512-byte sectors on the card, one for the ID and one for each recovery record.

Use a different ID for each file, such as `com.example.game.slot-1` and
`com.example.game.slot-2`. Pinball uses `constexpr` IDs, so the compiler
hashes them ahead of time and the firmware omits runtime ID hashing.

For up to **1,004 bytes of game data in a new file**, allow two sectors per record.
Add `--build-property 'build.extra_flags=-DCHSAVE_MAX_SECTORS_PER_RECORD=2'`
to `arduino-cli compile`. Existing files keep their original capacity.

Filenames use eight base-36 characters (`0–9`, `A–Z`). CHSave checks the full
ID in the file's first sector. A mismatch returns `IdCollision` and preserves
the existing file.

## Games using CHSave

All five use the default of [one sector per record](src/Config.h#L4-L9).

- [Tetris](https://github.com/juliendorra/tetris-chgame) saves named high scores, music choice and palette. [Save code](https://github.com/juliendorra/tetris-chgame/blob/134d965a226562a3ab4c1b5fcaeae526b8516594/Tetris/Tetris.ino#L10-L27)
- [Midnight Pinball](https://github.com/juliendorra/chgame-pinball) uses two files for 48 board scores each, with mute stored in the first. [Save code](https://github.com/juliendorra/chgame-pinball/blob/a0909c6d36be9186d1c425a72eaf6a156805dcba/Pinball/Pinball.ino#L25-L85)
- [VoidBloom](https://github.com/juliendorra/VoidBloom) saves the high score and mute. [Save code](https://github.com/juliendorra/VoidBloom/blob/9a449a9ac2240c17dcde8e55a2ccd17c94951359/VoidBloom.ino#L122-L127)
- [LightDrive](https://github.com/juliendorra/light-drive) saves the best distance, palette, mute and haptics. [Save code](https://github.com/juliendorra/light-drive/blob/46b7d3f69293062d4821d680cf1f971222dddc93/LightDrive.ino#L154-L156)
- [Mystic Balloon](https://github.com/juliendorra/mystic-balloon-chgame) saves up to eight runs plus palette, sound and audio mode in 468 bytes. [Save layout](https://github.com/juliendorra/mystic-balloon-chgame/blob/cf66e12c1db8e39d500b1d670751661fe835a718/MysticBalloon/Saves.h#L5-L25)

See [the working example](examples/Settings/Settings.ino) and
[integration notes](docs/integration.md) for installation, errors and schema changes.
[Format](docs/format.md) · [Tests](tests/README.md) · [MIT license](LICENSE)
