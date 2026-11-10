#!/usr/bin/env python3
"""One-time, explicit conversion of a CHSave 0.1 journal. Firmware has no legacy reader."""
import argparse
import os
from pathlib import Path
import re
import struct
import zlib
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "chgame-sd-saves/tools"))
import prepare


def convert(raw, legacy_id, game_id):
    if not re.fullmatch(r'[A-Z0-9]{4}', legacy_id):
        raise ValueError('legacy ID must be four uppercase letters/digits')
    if not 1024 <= len(raw) <= 131072 or len(raw) % 1024:
        raise ValueError('invalid old journal length')
    size = len(raw) // 2
    out = bytearray(prepare.descriptor(game_id, size // 512))
    good = False
    for offset in (0, size):
        page = bytearray(raw[offset:offset + size])
        tag = 0x31565343 if size == 512 else 0x32565343
        if (page[:4] == legacy_id.encode('ascii') and
                struct.unpack_from('<I', page, 12)[0] == tag and
                struct.unpack_from('<H', page, 8)[0] <= size - 20 and
                zlib.crc32(page[:-4]) == struct.unpack_from('<I', page, size - 4)[0]):
            struct.pack_into('<I', page, 0, prepare.RECORD_TAG)
            struct.pack_into('<I', page, 12, 0)
            struct.pack_into('<I', page, size - 4, zlib.crc32(page[:-4]))
            good = True
        else:
            page[:] = b'\xff' * size
        out.extend(page)
    if not good and raw != b'\xff' * len(raw):
        raise ValueError('no valid old record; nothing converted')
    prepare.validate(out, game_id)
    return bytes(out)


def migrate(card, legacy_id, game_id):
    card = Path(card)
    target = card / prepare.filename(game_id)
    if target.is_symlink():
        raise ValueError('refusing symlink')
    if target.exists():
        prepare.validate(target.read_bytes(), game_id)
        return 'kept', target
    if not re.fullmatch(r'[A-Z0-9]{4}', legacy_id):
        raise ValueError('invalid legacy ID')
    source = card / (legacy_id + '.SAV')
    if source.is_symlink():
        raise ValueError('refusing symlink')
    data = convert(source.read_bytes(), legacy_id, game_id)
    with target.open('xb') as stream:
        stream.write(data); stream.flush(); os.fsync(stream.fileno())
    if target.read_bytes() != data:
        raise OSError('conversion readback failed')
    return 'converted', target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--card', required=True, type=Path)
    parser.add_argument('--legacy', required=True)
    parser.add_argument('game_id')
    args = parser.parse_args()
    try:
        action, path = migrate(args.card, args.legacy, args.game_id)
        print(f'{action}: {path.name}; old file left untouched')
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')

if __name__ == '__main__':
    main()
