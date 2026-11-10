#!/usr/bin/env python3
"""Optional host utility for developer fixtures; games create saves themselves."""
import argparse
import os
from pathlib import Path
import re
import struct
import zlib

MAX_DATA = 65516
NAME_SPACE = 36 ** 8
ALPHABET = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'
DESCRIPTOR = b'CHSAVE2\0'
RECORD_TAG = 0x33565343  # CSV3

class IdCollision(ValueError):
    pass


def check_id(game_id):
    if (not isinstance(game_id, str) or not 1 <= len(game_id) <= 253 or
            len(game_id.split('.')) < 2 or any(not re.fullmatch(
                r'[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?', part)
                for part in game_id.split('.'))):
        raise ValueError('ID must be a lowercase reverse-domain name, up to 253 characters')


def filename(game_id):
    check_id(game_id)
    value = 14695981039346656037
    for byte in game_id.encode('ascii'):
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    value %= NAME_SPACE
    name = ''
    for _ in range(8):
        value, digit = divmod(value, 36)
        name = ALPHABET[digit] + name
    return name + '.SAV'


def file_size(capacity):
    if not 1 <= capacity <= MAX_DATA:
        raise ValueError(f'capacity must be between 1 and {MAX_DATA} bytes')
    return 512 + ((capacity + 20 + 511) // 512) * 1024


def descriptor(game_id, slot_sectors):
    check_id(game_id)
    if not 1 <= slot_sectors <= 128:
        raise ValueError('slot length must be 1–128 sectors')
    block = bytearray(512)
    struct.pack_into('<8sHHI', block, 0, DESCRIPTOR, len(game_id), slot_sectors, 0)
    block[16:16 + len(game_id)] = game_id.encode('ascii')
    struct.pack_into('<I', block, 508, zlib.crc32(block[:508]))
    return bytes(block)


def blank(game_id, capacity=492):
    size = file_size(capacity)
    return descriptor(game_id, (size - 512) // 1024) + b'\xff' * (size - 512)


def validate(raw, game_id):
    check_id(game_id)
    if not 1536 <= len(raw) <= 131584 or (len(raw) - 512) % 1024:
        raise ValueError('invalid save file length')
    magic, length, sectors, reserved = struct.unpack_from('<8sHHI', raw)
    if (magic != DESCRIPTOR or not 1 <= length <= 253 or reserved or
            sectors != (len(raw) - 512) // 1024 or
            zlib.crc32(raw[:508]) != struct.unpack_from('<I', raw, 508)[0]):
        raise ValueError('damaged identity descriptor')
    if raw[16:16 + length] != game_id.encode('ascii'):
        raise IdCollision('filename collision: file belongs to another ID; left untouched')
    slot_bytes = sectors * 512
    journal = raw[512:]
    if journal == b'\xff' * len(journal):
        return
    for offset in (0, slot_bytes):
        page = journal[offset:offset + slot_bytes]
        tag, _, size, _, _, reserved = struct.unpack_from('<IIHBBI', page)
        if (tag == RECORD_TAG and not reserved and size <= slot_bytes - 20 and
                zlib.crc32(page[:-4]) == struct.unpack_from('<I', page, slot_bytes - 4)[0]):
            return
    raise ValueError('no valid record; existing file left untouched')


def prepare(card, game_ids, capacity=492):
    card = Path(card)
    if not card.is_dir():
        raise ValueError(f'card folder does not exist: {card}')
    size = file_size(capacity)
    ids = list(dict.fromkeys(game_ids))
    names = {}
    for game_id in ids:
        name = filename(game_id)
        if name in names and names[name] != game_id:
            raise IdCollision(f'filename collision between {names[name]} and {game_id}')
        names[name] = game_id
        path = card / name
        if path.is_symlink():
            raise ValueError(f'refusing symlink: {path}')
        if path.exists():
            raw = path.read_bytes()
            validate(raw, game_id)
            if len(raw) < size:
                raise ValueError('existing file is too small; back it up before resizing')
    results = []
    for name, game_id in names.items():
        path = card / name
        if path.exists():
            results.append(('kept', path))
            continue
        raw = blank(game_id, capacity)
        with path.open('xb') as stream:
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        if path.read_bytes() != raw:
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
