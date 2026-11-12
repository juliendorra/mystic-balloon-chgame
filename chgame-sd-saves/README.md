# CHSave

SD saves and settings for [CHGame](https://github.com/bateske/CHGame).
Each save file has a reverse-domain ID and a hashed `.SAV` filename.
Two records, CRC checks and verified writes protect the previous save.

Install CHSave and the included `extras/CHSd` as Arduino libraries alongside CHGfx:

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

Insert a FAT16/FAT32 SD card. The first save creates `/SAVES/` and the game's
file automatically. Missing saves load the game's defaults.
We recommend calling `saves.store()` between frames when your game needs to save. The call waits for the write to finish. Use `saves.status()` to check failures.

The default save size is **492 bytes of game data per file**. The file takes
three 512-byte sectors on the card, one for the ID and one for each recovery record.
One `Store` can use several fixed IDs for data you save independently.
Pinball uses two IDs for 48 board scores each.

For up to **1,004 bytes of game data per file**, allow two sectors per record.
Add `--build-property 'build.extra_flags=-DCHSAVE_MAX_SECTORS_PER_RECORD=2'`
to `arduino-cli compile`.

Filenames use eight base-36 characters (`0–9`, `A–Z`). The full ID is checked
before writing, so a hash collision cannot overwrite another app's data.

## Games using CHSave

- [Tetris](https://github.com/juliendorra/tetris-chgame) saves high scores with player names, music choice, and palette. [Save code](https://github.com/juliendorra/tetris-chgame/blob/c3a2e583a922758b6022c4a59ff6aa2f8d66761e/Tetris/Tetris.ino#L10-L27) · [One sector per record](https://github.com/juliendorra/tetris-chgame/blob/c3a2e583a922758b6022c4a59ff6aa2f8d66761e/tools/sd_saves.py#L93)
- [Midnight Pinball](https://github.com/juliendorra/chgame-pinball) saves personal bests and the sound mute setting. [Save code for the 24-score build](https://github.com/juliendorra/chgame-pinball/blob/a8f0f5ba573788b3f2a63f896b46629fc1d13950/Pinball/Pinball.ino#L25-L40) · [One sector per record](https://github.com/juliendorra/chgame-pinball/blob/a8f0f5ba573788b3f2a63f896b46629fc1d13950/tools/sd_saves.py#L86)
- [VoidBloom](https://github.com/juliendorra/VoidBloom) saves the high score and sound mute setting. [Save code](https://github.com/juliendorra/VoidBloom/blob/f3a3c35cd7795ea42999b4fd1661044b0459b8f4/VoidBloom.ino#L122-L127) · [One sector per record](https://github.com/juliendorra/VoidBloom/blob/f3a3c35cd7795ea42999b4fd1661044b0459b8f4/tools/sd_saves.py#L93)
- [LightDrive](https://github.com/juliendorra/light-drive) saves the best distance, palette, sound mute, and haptics. [Save code](https://github.com/juliendorra/light-drive/blob/ddf37e0f4260f57e4902673b71da3ecd70f8e875/LightDrive.ino#L154-L156) · [One sector per record](https://github.com/juliendorra/light-drive/blob/ddf37e0f4260f57e4902673b71da3ecd70f8e875/tools/sd_saves.py#L93)
- [Mystic Balloon](https://github.com/juliendorra/mystic-balloon-chgame) saves up to eight runs plus palette, sound, and audio mode in a 468-byte payload. [Save data](https://github.com/juliendorra/mystic-balloon-chgame/blob/f9a128a10150b233b4496afc8ead3e32723f1f32/MysticBalloon/Saves.h#L5-L25) · [Preferences](https://github.com/juliendorra/mystic-balloon-chgame/blob/f9a128a10150b233b4496afc8ead3e32723f1f32/MysticBalloon/Port.h#L20-L26) · [Save code](https://github.com/juliendorra/mystic-balloon-chgame/blob/f9a128a10150b233b4496afc8ead3e32723f1f32/MysticBalloon/Saves.cpp#L67-L69) · [One sector per record](https://github.com/juliendorra/mystic-balloon-chgame/blob/f9a128a10150b233b4496afc8ead3e32723f1f32/tools/sd_saves.py#L93)

See [the working example](examples/Settings/Settings.ino) and
[integration notes](docs/integration.md) for installation, errors and schema changes.
[Format](docs/format.md) · [Tests](tests/README.md) · [MIT license](LICENSE)
