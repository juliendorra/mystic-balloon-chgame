# Using CHSave

## Install

Copy this repo to your Arduino sketchbook's `libraries/CHSave` folder, or
install its ZIP through **Sketch → Include Library → Add .ZIP Library**.
Install the CHGame board package and CHGfx from the CHGame SDK.

With Arduino CLI, point at this repo and your SDK libraries:

```sh
arduino-cli compile -b CHGame:ch32v:rev0:opt=oslto,rtlib=nano,periph=game,usb=uploadonly \
  --library /path/to/chgame-sd-saves \
  --library /path/to/CHGame/platform/board/arduino/CHGame/libraries/CHGame \
  --library /path/to/CHGame/platform/board/arduino/CHGame/libraries/CHGfx \
  examples/Settings
```

CHSave has its own namespaced SD driver and FAT lookup. It can coexist with
CHSd for game assets. It does not replace SDK files or SDK `save::` calls;
use `chsave::Store` explicitly.

## Data and IDs

Use a stable reverse-domain ID such as `com.example.game`. IDs are lowercase
ASCII labels (letters, digits and internal hyphens), separated by dots; labels
are 1–63 characters and the complete ID is at most 253 characters. A game's settings and
progress can share one struct. Use fixed-width fields, initialize padding,
and avoid pointers, strings with heap storage, and C++ containers. The struct
must fit the file's capacity. Typed calls store its exact memory layout.

The first `store()` creates the file and chooses its capacity from the payload,
rounded to SD sectors. Loading a missing file returns `Empty` without writing.
The minimum file is 1.5 KB (492-byte payload); the largest is 128 KiB + 512 bytes (65,516-byte payload).
The file holds an immutable 512-byte identity descriptor and two equal slots
rounded to SD sectors. Each record can use
any payload length up to capacity. Existing files are never silently resized.

Increment the schema byte when the layout changes. A mismatched schema or
size returns `Incompatible`. For migration, load the old struct with its old
schema, convert it, then store the new struct with the new schema. Don't
automatically save defaults on every failed load.

The `load(id, schema, void*, size, flags)` and
`store(id, schema, const void*, size, flags)` overloads let you encode bytes
yourself for a layout that also works on other architectures.

## Failures

Both operations return `bool`; `status()` gives the last result.
Typed loads preserve the destination on failure. They use a temporary copy
of the struct on the stack, so leave enough RAM for both copies. Raw byte
loads can partially change the destination on an I/O error; discard it on
failure. Flags are changed only after a successful load.

| Status | Meaning |
| --- | --- |
| `Ready` | Loaded or saved successfully. |
| `Empty` | No save yet; keep defaults. The first store creates the file. |
| `NoCard` | Card initialization failed. |
| `NotFound` | Storage-backend result for a missing file; `Journal` reports `Empty` to callers. |
| `Incompatible` | Latest record uses a different schema or size. |
| `Damaged` | Invalid file/chain, or neither record is usable. Back up before repair. |
| `IoError` | A read, write or verification failed. |
| `UnsupportedFilesystem` | Cannot mount FAT16/FAT32; exFAT is unsupported. |
| `InvalidArgument` | Invalid ID, empty/oversized payload, or invalid scratch buffer. |
| `TooLarge` | Payload exceeds this file's reserved capacity. |
| `IdCollision` | The hashed filename belongs to another full ID; no writes occur. |
| `NoSpace` | No free clusters or directory entry for the new save. |

Do not report a successful save unless `store()` returns true. Restart
with the card inserted after removal or card errors. Back up the `.SAV`
files by copying them from the card.

## Memory and other platforms

`Store` waits for CHGfx and borrows 1,024 bytes of its idle scratch buffer.
Use it after display initialization, between frames. Calls are synchronous;
do not call from interrupts, drawing callbacks, SD streams or concurrent
tasks. Returned data belongs to your struct, not the scratch buffer.

The portable core is `Journal.h` + `Journal.cpp`. Implement `chsave::Storage`
with `open`, `create`, `slotSectors`, `read` and `write`, then pass that backend and a caller-owned,
4-byte-aligned 1,024-byte buffer to `chsave::Journal`. The payload must not
overlap that buffer. Backends must confine writes to the file's data sectors.
Sector 0 holds the immutable identity descriptor. Sector indices 1 onward run
through both slots; `slotSectors()` is the length of one
slot, from 1 to 128. The default is one sector for simple custom backends.
A fixed-memory backend can keep the default `create()` implementation if
its storage always exists.
The supplied hardware adapter targets CHGame Rev0 (CH32X035, SPI1, PB11 CS).

For firmware close to Rev0's flash limit, build the whole library and sketch
with `-DCHSAVE_MAX_SLOT_SECTORS=1`. This optional compact profile supports
1.5 KB files and payloads up to 492 bytes. The default supports larger files
and discovers their capacity at runtime. Both profiles use the same format
for minimum-size files.

For CHSIM builds, set `CHSAVE_CARD_IMAGE` to a writable FAT disk image.
The simulator backend modifies that image; use a copy for experiments.

## Recovery limits

Writes target the older slot and read it back. Identical saves skip writing.
One valid slot survives a torn write to the other; two invalid slots are
left untouched. Newly created files include a valid empty record so an
interrupted first payload write can be retried.

File creation updates FAT copies, invalidates FAT32 free-space hints and
initializes the journal before publishing its directory entry. Later record
writes leave FAT/directory metadata alone. An interrupted allocation can leave
unlinked clusters; a filesystem check can reclaim them. These checks cannot
guarantee an SD controller's behavior during physical power
loss. The tests simulate interrupted sector writes, not electrical failures.
