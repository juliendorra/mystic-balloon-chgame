# CHSd with writes

A drop-in fork of CHGame's CHSd. Install it as the Arduino `CHSd` library
in place of the SDK copy, or pass `--library /path/to/chgame-chsd` to
Arduino CLI. Existing `Fat.h` and `SdSpi.h` callers keep the same API.

CHSave uses `FatWrite.h` to create files on FAT16/FAT32 and `SdWrite.h`
to write sectors. Neither is needed for asset reads; the CHGame linker
drops both write and allocation code when unused. All calls share SPI1
with the display: call them after `gfx_wait()`.

The fork uses the same mount and FAT traversal for assets and saves. The
writer rejects non-mirrored FAT32, checks FAT copies before changing them,
and verifies writes before publishing a new directory entry.

Based on [CHGame CHSd at 4d6c454](https://github.com/bateske/CHGame/tree/4d6c45452cc36e557c682a4543481e1c744c71b4/platform/board/arduino/CHGame/libraries/CHSd).
[MIT license](LICENSE) · [Attribution](NOTICE)
