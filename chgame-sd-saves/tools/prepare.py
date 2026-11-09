#!/usr/bin/env python3
"""Create or check CHSave files on a mounted FAT16/FAT32 card."""
import argparse
import os
from pathlib import Path
import re
import struct
import zlib

EMPTY = b'\xff' * 1024
MAX_DATA = 65516


def file_size(capacity):
    if not 1 <= capacity <= MAX_DATA:
        raise ValueError(f'capacity must be between 1 and {MAX_DATA} bytes')
    return ((capacity + 20 + 511) // 512) * 1024


def validate(raw, game_id):
    if not 1024 <= len(raw) <= 131072 or len(raw) % 1024:
        raise ValueError('save file must be 1–128 KiB, in multiples of 1024 bytes')
    slot_bytes = len(raw) // 2
    expected_tag = 0x31565343 if slot_bytes == 512 else 0x32565343
    if raw == b'\xff' * len(raw):
        return
    magic = int.from_bytes(game_id.encode('ascii'), 'little')
    for offset in (0, slot_bytes):
        page = raw[offset:offset + slot_bytes]
        ident, _, size, _, _, tag = struct.unpack_from('<IIHBBI', page)
        if (ident == magic and size <= slot_bytes - 20 and tag == expected_tag and
                zlib.crc32(page[:-4]) == struct.unpack_from('<I', page, slot_bytes - 4)[0]):
            return
    raise ValueError('no valid record; existing file left untouched')


def prepare(card, game_ids, capacity=492):
    card = Path(card)
    if not card.is_dir():
        raise ValueError(f'card folder does not exist: {card}')
    ids = list(dict.fromkeys(game_ids))
    size = file_size(capacity)
    for game_id in ids:
        if not re.fullmatch(r'[A-Z0-9]{4}', game_id):
            raise ValueError(f'ID must be four uppercase letters/digits: {game_id!r}')
    # Check the whole batch before creating any missing files.
    for game_id in ids:
        path = card / (game_id + '.SAV')
        if path.is_symlink():
            raise ValueError(f'refusing symlink: {path}')
        if path.exists():
            try:
                raw = path.read_bytes()
                validate(raw, game_id)
                if len(raw) < size:
                    raise ValueError('existing file is too small; back it up before resizing')
            except ValueError as error:
                raise ValueError(f'{path}: {error}') from error
    results = []
    for game_id in ids:
        path = card / (game_id + '.SAV')
        if path.exists():
            results.append(('kept', path))
            continue
        # Exclusive creation also protects against an existing-file race.
        with path.open('xb') as stream:
            stream.write(b'\xff' * size)
            stream.flush()
            os.fsync(stream.fileno())
        if path.read_bytes() != b'\xff' * size:
            raise OSError(f'provisioning readback failed: {path}')
        results.append(('created', path))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--card', required=True, type=Path)
    parser.add_argument('--bytes', dest='capacity', type=int, default=492,
                        help='payload capacity to reserve per game (default: 492)')
    parser.add_argument('game_ids', metavar='ID', nargs='+')
    args = parser.parse_args()
    try:
        for action, path in prepare(args.card, args.game_ids, args.capacity):
            print(f'{action}: {path.name}')
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
