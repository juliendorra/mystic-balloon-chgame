# Saved runs

PLAY opens NEW, a spacer, then the saved runs. Up/Down chooses a run; A loads
it, B returns. The last played run is selected first. Eight runs fit on SD;
a full list refuses NEW and never deletes another run.

Saves include position, speed, balloons, coins, key, moved enemies, health,
score, level-entry score and palette/audio preferences. Static maps and
cosmetic particles are regenerated. Mid-level loads open paused. Death keeps
the last playable checkpoint.

START offers RESUME, RESTART LEVEL and SAVE & QUIT. RESTART LEVEL resets only
the active run's current level and entry score, with fresh enemies, coins and
key. It commits successfully before changing the paused world. SAVE & QUIT
keeps the exact mid-level state. B or START resumes without executing a menu
action. A accepts and B cancels on other menus; back wins simultaneous inputs.

## SD storage

ID: `com.juliendorra.mysticballoon`. File: `/SAVES/N0KYRDE5.SAV` on the card.
The CHSave file is 1,536 bytes: an immutable full-ID descriptor and two
512-byte records. The 468-byte payload schema remains 3. Records alternate,
are CRC checked and verified after writing. A failed write leaves the other
record usable; unchanged checkpoints skip writing.

The first save creates the directory and file. A missing card shows INSERT SD
CARD. Invalid identity, corrupt records and failed writes report SAVE ERROR.
Firmware has no flash fallback or legacy migration reader.

## Upgrade existing saves

Copy `N0KYRDE5.SAV` from the card root into `SAVES/` before playing.

## Validation

The host game suite checks all 16 option combinations, independent NEW runs,
exact mid-level resume, restart failure handling, capacity, preferences across
reboots and all 512 interrupted-write positions. It confirms that legacy flash
is ignored and foreign-game flash remains untouched. Provisioning tests check
explicit host conversion and preservation of every saved run.

Native Rev0 builds and host checks validate this source change. No new physical
console installation is implied by those checks; earlier hardware measurements
belong to their recorded versions. See [the shared SD-save policy](../../docs/sd-saves.md).
