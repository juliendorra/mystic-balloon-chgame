# CSV1 / CSV2 save format

A root file named `<ID>.SAV` on FAT16 or FAT32. Its size is a multiple
of 1,024 bytes, from 1 KiB to 128 KiB.
ID is four uppercase ASCII letters/digits. Fragmented files are supported.
The complete cluster chain must match the file's length (including the
unused part of its last allocated cluster). Files are created on a computer;
firmware only overwrites their data sectors.

Each of the two equal slots has these fields. A slot is 1–128 SD sectors.
Integers are little-endian. Let `N` be its length in bytes.

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 4 | Game ID as ASCII bytes |
| 4 | 4 | Generation, incremented modulo 2³² |
| 8 | 2 | Payload size, at most `N - 20` |
| 10 | 1 | Application schema |
| 11 | 1 | Application flags |
| 12 | 4 | `CSV1` (`0x31565343`) for a 512-byte slot; `CSV2` (`0x32565343`) for larger slots |
| 16 | size | Application payload |
| 16 + size | to offset `N - 5` | Zero-filled unused space |
| N - 4 | 4 | CRC-32/ISO-HDLC of bytes `0..N-5` (same as Python `zlib.crc32`) |

Two slots filled with `0xFF` mean a new journal. Otherwise at least one
slot must pass ID, tag, size and CRC validation. The newer valid generation
wins, using unsigned serial arithmetic; on a tie, slot 0 wins. Generation
differences of exactly 2³¹ are treated as a tie.

Loading checks schema and size **after** choosing the latest record. It
does not fall back to stale data with an older schema. A deliberate store
can replace the schema. The other committed slot remains until a later save.

Compatible with the 244-byte `tools/sd_saves` workspace journal. Mystic
Balloon's older `MYBLSAVE.DAT` format requires application-specific import.
