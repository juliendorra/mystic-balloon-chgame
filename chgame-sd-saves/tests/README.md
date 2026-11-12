# Tests

Needs Python 3 and a C++11 compiler with UBSan (GCC or Clang).
The tests use the included `extras/CHSd` library. Set `CHSAVE_CHSD` to
check another checkout of the fork. No Python packages or SDK checkout required.

```sh
python3 tests/test_saves.py
```

Set `CXX=clang++` or `CXX=g++` to choose a compiler.
The tests use the default limit of one sector per record. Set
`CHSAVE_TEST_COMPACT=0` to test a limit of 128 sectors per record.

Tests compile the actual journal, FAT lookup and CHGame adapter. They check
process restarts, FAT16/FAT32, partitions, fragmented files, unrelated data
and metadata, every interrupted-write position, generation rollover, schema
changes, corrupted records, read errors, failed verification, unchanged-write
suppression, namespaced provisioning, a real base-36 hash collision, C++/Python
filename vectors and the simulator backend. The core also runs
against a custom in-memory storage backend with a caller-owned buffer.

First-save tests start with no save file. They check automatic file creation,
fragmented free space, full cards, FAT32 root extension, mirrored FATs and
restarts after interrupted allocation, publication and the first payload write.

`vendor/fatimg.py` is CHSd's MIT-licensed FAT image builder (see `NOTICE`).
