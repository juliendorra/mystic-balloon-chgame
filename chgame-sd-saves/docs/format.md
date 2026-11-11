# CHSAVE2 / CSV3 save format

Files live in `/SAVES/` on the FAT16/FAT32 card. The filename is eight uppercase base-36
characters (`0–9`, `A–Z`) plus `.SAV`. Hash the exact lowercase ASCII reverse-domain
ID with FNV-1a-64 (offset 14695981039346656037, prime 1099511628211, wrap modulo
2^64), take the result modulo 36^8, and encode it in base 36, zero-padded to eight
characters, most-significant digit first. Example: `com.juliendorra.pinball`
becomes `5T668NAZ.SAV`. Case variations are invalid IDs, not aliases.

This gives 2,821,109,907,456 filename values (about 41.36 bits). It is a lookup
key, not proof of identity. Firmware checks the complete stored ID before reading
or writing records. A mismatch returns `IdCollision`, never claims a blank file,
and never falls back to another filename. Provisioning refuses colliding IDs too.

The file has one immutable 512-byte identity descriptor followed by two equal
journal slots of 1–128 sectors each. File length is `512 + 1024 * slotSectors`:
1,536–131,584 bytes. Fragmented files are supported. Firmware creates missing
files on first save. Normal journal updates never resize the file, change
its descriptor, or write FAT/directory metadata.

## Identity descriptor (sector 0)

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 8 | `CHSAVE2` followed by NUL |
| 8 | 2 | Full ID length, 1–253 bytes |
| 10 | 2 | Sectors per slot, 1–128; must match file length |
| 12 | 4 | Reserved, zero |
| 16 | ID length | Exact ASCII ID, no NUL required |
| remainder | to byte 507 | Zero padding |
| 508 | 4 | CRC-32/ISO-HDLC of bytes 0–507 |

IDs contain at least two dot-separated labels. Each label is 1–63 lowercase
letters/digits with optional internal hyphens. Total length is at most 253.
A missing, invalid or corrupt descriptor prevents every write.

## Each journal slot

Integers are little-endian. Let `N` be the slot length in bytes.

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 4 | `CSV3` (`0x33565343`) |
| 4 | 4 | Generation, incremented modulo 2^32 |
| 8 | 2 | Payload size, at most `N - 20` |
| 10 | 1 | Application schema |
| 11 | 1 | Application flags |
| 12 | 4 | Reserved, zero |
| 16 | size | Application payload |
| remainder | to byte `N - 5` | Zero padding |
| `N - 4` | 4 | CRC-32/ISO-HDLC of bytes 0 through `N - 5` |

Two slots filled with `0xFF` mean a new journal, after checking its descriptor.
Otherwise at least one slot must pass tag, reserved, size and CRC checks. The
newer valid generation wins using unsigned serial arithmetic; ties choose slot 0,
including a generation difference of exactly 2^31.

A valid zero-length record denotes an empty journal. Automatic creation writes
one such record before publishing the file, allowing the first payload write
to be interrupted and retried. Normal application payloads are nonempty.

Load checks schema and size after selecting the newest valid record; it never
falls back to stale application schemas. A store may deliberately change the
schema. Writes target the other slot and are read back sector by sector. The
previous committed slot remains intact until a later store.
