# Saved runs — 1.4.0

The previous port kept one 12-byte level-entry checkpoint in the CHGame
SDK's two shared flash pages. NEW could replace it. Another game's first
save could also replace it. Power-off duration is not part of that save
format; flash is nonvolatile. Those overwrite paths explain possible lost
progress, but do not establish a hardware retention fault.

## Player behavior

PLAY opens the saved-run list directly: NEW at the top, an empty spacer
line, then the existing runs, named `LEVEL 16 0%`, for example. The last
played run is preselected. UP/DOWN selects a run and B loads it; on NEW,
B creates a run. NEW stays visible while longer lists of runs scroll.
NEW creates another run. It never replaces an existing one. There are eight
runs on SD, or four in the flash fallback. A full list refuses NEW and
reports SAVES FULL; it does not silently delete a run.

The percentage counts collected coins plus the key, divided by that map's
coins plus its key. Level 23 has four coins; the other maps have six.
The label changes as that run advances. A finished campaign shows 100%.

The game saves level transitions, objective/score/life changes when effects
have finished, and pause/SAVE & QUIT. A mid-level save retains fixed-point
player position and velocity, balloons, control/immunity/jump state,
collected coins and their moved horizontal positions, key possession,
enemy positions/health/direction, score, campaign coins and update phase.
Static map geometry and cosmetic particles are regenerated. A mid-level
load opens paused, ready for RESUME. Death retains the last playable save.

Existing schema-1 saves migrate into the first run, keeping their score,
level, coins, high score, palette and sound settings. Their original format
never stored a partial level or player position, so those migrated entries
start at the saved level's entrance. New saves support mid-level resume.

## SD is the primary store

With a usable SD card, the game reads and writes `MYBLSAVE.DAT` in the root
of the card. It does not update the shared flash save on every SD save.
Switching games therefore cannot replace the SD journal.

Prepare the file once on a mounted card:

```sh
python3 MysticBalloon/tools/prepare_saves.py --card /path/to/card
```

The workspace launcher also does this before `cart deploy --card ...`.
The preparation tool creates a missing journal, preserves a valid existing
one, and refuses to overwrite a damaged file. It is deliberately not bundled
as a replaceable data asset: installing/upgrading the cartridge must not
copy an empty save over the user's existing progress.

The 1,536-byte file is preallocated by the host. Sector zero is a checked
format descriptor; the other two sectors alternate full records with a
32-bit generation and CRC-32. Each record holds all eight 52-byte worlds
and shared preferences. All fields have fixed widths and compile-time size
checks. Readback verifies a commit before SAVE & QUIT exits. A torn newest
record falls back to the previous complete record. If both are invalid,
the game reports an error and leaves the file untouched.

During play, only the existing file's two data sectors are written. The
firmware never allocates clusters, changes FAT entries, or updates directory
metadata. The private CHSd-derived FAT reader supports all three sectors
being fragmented. SPI is shared with the LCD, so every read/write occurs
after `gfx_wait()` and before the next display flush, borrowing the idle
1 KiB graphics scratch rather than adding a large RAM buffer.

A present but unprepared/invalid card reports SET UP SD SAVES or SAVE ERROR;
it does not silently replace that card's data. Insert the card before
starting the game. After a card error, restart with the card connected.
Copy `MYBLSAVE.DAT` to back up all runs.

## Flash coexistence and precedent

CHGame's own [SD-menu documentation](https://github.com/bateske/CHGame/blob/main/docs/sd-menu.md)
explicitly describes the current shared save area: another game's save can
replace it. Its `chgame/Save.h`/`Save.cpp` provide CRC-checked alternating
256-byte pages at `0xF500` and `0xF600`; this port preserves both pages.
The CHSd reader establishes the board's SPI hand-off and scratch-buffer
rules. CHSDtoUSB provides the on-board precedent for SD block writes.
Our private adaptation adds a polled CMD24 writer, waits for card-ready,
and verifies the record readback. Provenance and the MIT license are in
`src/savecard/` and `SAVE-CARD-LICENSE`; shared SDK sources are unchanged.

Without an SD card, four worlds fit in the existing 244-byte payload limit.
Before writing, Mystic Balloon checks ownership of both flash pages and
refuses to erase a foreign game's record. A magic number detects ownership;
it does not create extra storage or make unmodified games share the pages
cooperatively. Other games can still replace the fallback. Use SD for
progress that must coexist across games.

[Arduboy2's EEPROM guidance](https://github.com/MLXXXp/Arduboy2#using-eeprom-in-a-sketch---this-is-important)
reserves the first 16 bytes for system settings. The remaining EEPROM is a
shared pool; choosing a game ID and fixed offsets identifies data but does
not allocate an isolated region automatically. Original Mystic Balloon
uses such fixed offsets and ID markers. These should not be mistaken for
a general multi-game persistence service.

[Arduboy FX](https://github.com/MrBlinky/ArduboyFX/blob/main/src/ArduboyFX.h)
instead provides a program-exclusive 4 KiB save block. Its save implementation
appends states until the block needs erasing. The useful principles here
are per-game ownership, bounded/versioned state and avoiding needless
writes. The CHGame SD journal supplies isolation; alternating CRC records
supply recovery. Unchanged preferences/checkpoints are not continuously
written to flash.

## Validation and limits

The host suite checks all 16 legacy option combinations, independent NEW
runs, mid-level world round trips, full-capacity refusal, foreign flash
protection, corrupt-record recovery, and all 512 interrupted positions of
an SD sector write. Damaged journals are preserved. Provisioning is checked
for non-overwrite and newest-record migration.

On CHGame Rev0, a real coin collision, pause, SAVE & QUIT, reload and another
NEW retain both the old and new runs. Mid-level position, coins, score and
balloons reload correctly. A one-second resumed gameplay window has zero
late frames and a 1,600-byte stack watermark. The original level-16 save is
backed up before installation; test runs are temporary.

The host also retains all 39 maps, 17,199 viewport references, 320 player
composites, 452 original beep events, 180 timer cases and the 15,303-state
ATM comparison. The splash uses a framebuffer-backed LZ decoder to make
room for storage; its production pixels are compared with the drawing
source. The small on-device save diagnostic omits splash bitmaps and the
older rendering-stage profiler to fit Rev0; normal firmware retains them.

Failure injection models interrupted software writes. It does not measure
an SD controller's internal power-loss behavior or overnight electrical
retention. The save record and card installation evidence document what
was actually checked.
