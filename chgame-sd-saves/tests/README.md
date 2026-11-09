# Tests

Needs Python 3 and a C++11 compiler with UBSan (GCC or Clang).
No Python packages or CHGame SDK checkout required.

```sh
python3 tests/test_saves.py
```

Set `CXX=clang++` or `CXX=g++` to choose a compiler.
Set `CHSAVE_TEST_COMPACT=1` to test the optional compact firmware profile.

Tests compile the actual journal, FAT lookup and CHGame adapter. They check
process restarts, FAT16/FAT32, partitions, fragmented files, unrelated data
and metadata, every interrupted-write position, generation rollover, schema
changes, corrupted records, read errors, failed verification, unchanged-write
suppression, provisioning and the simulator backend. The core also runs
against a custom in-memory storage backend with a caller-owned buffer.

`vendor/fatimg.py` is CHSd's MIT-licensed FAT image builder (see `NOTICE`).
