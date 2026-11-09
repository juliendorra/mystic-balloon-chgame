#!/usr/bin/env python3
"""Run the library against real FAT images, with injected sector failures."""
import importlib.util
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
COMPACT = os.environ.get('CHSAVE_TEST_COMPACT') == '1'
sys.path[:0] = [str(ROOT / 'tools'), str(ROOT / 'tests/vendor')]
import prepare
import migrate_v1
from fatimg import build_image, FatVolume

MAGIC = 'com.example.tetris'
NAME = prepare.filename(MAGIC)
OTHER = 'com.example.climbing'
OTHER_NAME = prepare.filename(OTHER)
READY, EMPTY, NO_CARD, NEEDS_SETUP, INCOMPATIBLE, DAMAGED, IO_ERROR, UNSUPPORTED, INVALID = range(9)


def record(score, generation=1, magic=MAGIC, schema=1):
    page = bytearray(512)
    struct.pack_into('<IIHBBI', page, 0, prepare.RECORD_TAG, generation, 36, schema, 7, 0)
    struct.pack_into('<I', page, 16, score)
    page[20:52] = bytes([score & 255]) * 32
    struct.pack_into('<I', page, 508, zlib.crc32(page[:508]))
    return page


class SaveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='chsave-')
        cls.folder = Path(cls.tmp.name)
        cls.exe = cls.folder / 'journal'
        compiler = shlex.split(os.environ.get('CXX', 'c++'))
        common = compiler + ['-std=c++11', '-O1', '-Wall', '-Wextra', '-Werror',
                             '-fsanitize=undefined,bounds', '-fno-sanitize-recover=all',
                             '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'tests')]
        if COMPACT:
            common += ['-DCHSAVE_MAX_SLOT_SECTORS=1']
        core = ROOT / 'src/Journal.cpp'
        subprocess.run(common + [str(ROOT / 'tests/core.cpp'), str(core),
                                 '-o', str(cls.folder / 'core')], check=True)
        subprocess.run([str(cls.folder / 'core')], check=True)
        sources = [ROOT / 'tests/journal.cpp', core, ROOT / 'src/CHSave.cpp',
                   ROOT / 'src/detail/Card.cpp', ROOT / 'src/detail/Fat.cpp',
                   ROOT / 'src/detail/Block.cpp', ROOT / 'src/detail/Host.cpp']
        subprocess.run(common + ['-DCHSAVE_HOST'] + list(map(str, sources)) +
                       ['-o', str(cls.exe)], check=True)
        cls.host = cls.folder / 'simulator'
        subprocess.run(common + ['-DCHSIM'] + list(map(str, sources)) +
                       ['-o', str(cls.host)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def fixture(self, fs='fat16', mbr=True, spc=1, fragment=False, first=None):
        path = self.folder / 'card.img'
        build_image(path, {NAME: prepare.blank(MAGIC) if first is None else prepare.descriptor(MAGIC, len(first) // 1024) + first,
                           OTHER_NAME: prepare.blank(OTHER), 'OTHER.TXT': b'keep me' * 100},
                    fs=fs, mbr=mbr, spc=spc, clusters=5000 if fs == 'fat16' else 65530,
                    fragment={NAME: 3, OTHER_NAME: 3} if fragment else {})
        return path

    def run_save(self, path, command='load', magic=MAGIC, *args, exe=None, **faults):
        env = dict(os.environ, CHSAVE_CARD_IMAGE=str(path))
        env.update({key: str(value) for key, value in faults.items()})
        output = subprocess.check_output([str(exe or self.exe), command, magic,
                                          *map(str, args)], env=env, text=True)
        return tuple(map(int, output.split()))

    def test_fat_restart_fragmentation_isolation_and_unchanged_writes(self):
        for fs, mbr, spc, fragment in [('fat16', False, 1, True), ('fat16', True, 4, False),
                                      ('fat32', False, 1, True), ('fat32', True, 4, False)]:
            with self.subTest(fs=fs, mbr=mbr, spc=spc):
                path = self.fixture(fs, mbr, spc, fragment)
                before = path.read_bytes()
                self.assertEqual(self.run_save(path)[-1], EMPTY)
                self.assertEqual(self.run_save(path, 'store', MAGIC, 100)[0], 1)
                self.assertEqual(self.run_save(path, 'store', OTHER, 200)[0], 1)
                self.assertEqual(self.run_save(path)[:3], (1, 100, 7))
                self.assertEqual(self.run_save(path, 'load', OTHER)[1], 200)
                with FatVolume(path) as volume:
                    allowed = {lba + i for name in [NAME, OTHER_NAME]
                               for lba, count in volume.runs(name) for i in range(count)}
                    self.assertEqual(volume.read_file('OTHER.TXT'), b'keep me' * 100)
                after = path.read_bytes()
                changed = {i for i in range(len(after) // 512)
                           if before[i*512:(i+1)*512] != after[i*512:(i+1)*512]}
                self.assertTrue(changed and changed <= allowed)
                self.assertEqual(self.run_save(path, 'store', OTHER, 200)[0], 1)
                self.assertEqual(path.read_bytes(), after)

    def test_every_interrupted_sector_position(self):
        path = self.fixture(first=record(10, 1) + record(20, 2))
        with FatVolume(path) as volume:
            lba = volume.runs(NAME)[0][0] + 1
        original = record(10, 1)
        for cut in range(512):
            # Reset only the older slot; keep the rest of the disk unchanged.
            with path.open('r+b') as stream:
                stream.seek(lba * 512)
                stream.write(original)
            self.assertEqual(self.run_save(path, 'store', MAGIC, 30, CHSAVE_CUT=cut)[0], 0)
            self.assertEqual(self.run_save(path)[:3], (1, 20, 7), cut)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 30)[0], 1)
        self.assertEqual(self.run_save(path)[1], 30)

    def test_bad_foreign_missing_and_wrong_size_files(self):
        for data in [bytes(1024), b'bad size']:
            path = self.fixture(first=data) if len(data) % 1024 == 0 else self.fixture()
            if len(data) % 1024:
                with FatVolume(path) as volume:
                    directory = bytearray(path.read_bytes()); at = directory.index(NAME.replace('.', '').encode()); struct.pack_into('<I', directory, at + 28, len(data)); path.write_bytes(directory)
            before = path.read_bytes()
            self.assertEqual(self.run_save(path)[-1], DAMAGED)
            self.assertEqual(self.run_save(path, 'store', MAGIC, 3)[0], 0)
            self.assertEqual(path.read_bytes(), before)
        self.assertEqual(self.run_save(path, 'store', 'com.example.missing', 3)[-1], NEEDS_SETUP)
        self.assertEqual(self.run_save(self.folder / 'absent.img')[-1], NO_CARD)

    def test_generation_wrap_schema_and_corruption_recovery(self):
        path = self.fixture(first=record(10, 0xffffffff) + record(20, 0))
        self.assertEqual(self.run_save(path)[1], 20)
        self.assertEqual(self.run_save(path, 'load', MAGIC, 2)[-1], INCOMPATIBLE)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 30, 2)[0], 1)
        self.assertEqual(self.run_save(path, 'load', MAGIC, 2)[1], 30)
        self.assertEqual(self.run_save(path)[-1], INCOMPATIBLE)
        bad = record(30, 3)
        bad[20] ^= 1
        path = self.fixture(first=bad + record(20, 2))
        self.assertEqual(self.run_save(path)[1], 20)

    def test_read_failures_and_failed_readback_never_seed_or_succeed(self):
        path = self.fixture()
        before = path.read_bytes()
        for read in range(1, 8):
            self.assertEqual(self.run_save(path, 'store', MAGIC, 30, CHSAVE_FAIL_READ=read)[0], 0)
            self.assertEqual(path.read_bytes(), before)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 30, CHSAVE_CORRUPT_WRITE=1)[-1], IO_ERROR)

    def test_malformed_chain_and_readonly_attribute(self):
        path = self.fixture(spc=1)
        raw = bytearray(path.read_bytes())
        # Find the exact short directory name; change the on-disk file size.
        directory = raw.index(NAME.replace('.', '').encode())
        struct.pack_into('<I', raw, directory + 28, 1024)
        path.write_bytes(raw)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 30)[-1], DAMAGED)
        self.assertEqual(path.read_bytes(), raw)
        path = self.fixture()
        raw = bytearray(path.read_bytes())
        raw[raw.index(NAME.replace('.', '').encode()) + 11] |= 1
        path.write_bytes(raw)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 30)[-1], DAMAGED)
        self.assertEqual(path.read_bytes(), raw)

    def test_provisioning_preserves_valid_and_recoverable_saves(self):
        with tempfile.TemporaryDirectory() as folder:
            card = Path(folder)
            prepare.prepare(card, [MAGIC, OTHER])
            self.assertEqual((card / NAME).read_bytes(), prepare.blank(MAGIC))
            target = card / NAME
            target.write_bytes(prepare.descriptor(MAGIC, 1) + record(70) + bytes(512))
            before = target.read_bytes()
            prepare.prepare(card, [MAGIC, OTHER])
            self.assertEqual(target.read_bytes(), before)
            target.write_bytes(b'broken')
            with self.assertRaises(ValueError):
                prepare.prepare(card, ['com.example.new', MAGIC])
            self.assertFalse((card / prepare.filename('com.example.new')).exists())
            self.assertEqual(target.read_bytes(), b'broken')
            for invalid in ['abc!', '../x', 'UPPER.case', 'lowr', '.com.foo', 'com..foo', 'com.-foo', 'com.foo-', 'a.' + 'b' * 64, 'a.' + 'b' * 252]:
                with self.assertRaises(ValueError):
                    prepare.prepare(card, [invalid])

    def test_identity_descriptor_collision_and_corruption(self):
        path = self.fixture(first=record(20) * 2)
        with FatVolume(path) as volume:
            start = volume.runs(NAME)[0][0] * 512
        raw = bytearray(path.read_bytes())
        raw[start:start + 512] = prepare.descriptor(OTHER, 1)
        path.write_bytes(raw)
        self.assertEqual(self.run_save(path)[-1], 10)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 9)[-1], 10)
        self.assertEqual(path.read_bytes(), raw)
        raw[start + 508] ^= 1
        path.write_bytes(raw)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 9)[-1], DAMAGED)
        self.assertEqual(path.read_bytes(), raw)

    def test_cpp_python_filename_vectors_and_id_rules(self):
        vectors = {'com.example.test': '3AYOVE4K.SAV',
                   'com.example.tetris': '8DYAVD4T.SAV',
                   'com.juliendorra.pinball': '5T668NAZ.SAV',
                   'com.juliendorra.tetris': 'YKEL37VY.SAV',
                   'com.juliendorra.voidbloom': 'ZFC8ZUW4.SAV',
                   'com.juliendorra.climbingcolors': 'O1A5KSU0.SAV',
                   'com.juliendorra.climbinglab': 'Q621FBQJ.SAV',
                   'com.juliendorra.lightdrive': '4OCNF63J.SAV',
                   'com.juliendorra.mysticballoon': 'N0KYRDE5.SAV'}
        for ident, expected in vectors.items():
            self.assertEqual(prepare.filename(ident), expected)
            self.assertEqual(subprocess.check_output([str(self.exe), 'filename', ident], text=True).strip(), expected)
        path = self.fixture()
        before = path.read_bytes()
        for ident in ['BAD.id', 'four', 'com..game', 'com.game-', 'com.-game',
                      'a.' + 'b' * 64, 'a.' + 'b' * 252]:
            self.assertEqual(self.run_save(path, 'store', ident, 10)[-1], INVALID)
            self.assertEqual(path.read_bytes(), before)
        boundary = '.'.join(['a' * 63, 'b' * 63, 'c' * 63, 'd' * 61])
        self.assertEqual(len(boundary), 253)
        self.assertEqual(subprocess.check_output([str(self.exe), 'filename', boundary], text=True).strip(), prepare.filename(boundary))

    def test_real_hash_collision_cannot_claim_even_an_empty_foreign_journal(self):
        first = 'com.example.collision.5d57e5887ce381cd'
        second = 'com.example.collision.fd6323ce41531da8'
        name = prepare.filename(first)
        self.assertEqual(name, prepare.filename(second))
        for ident in (first, second):
            self.assertEqual(subprocess.check_output([str(self.exe), 'filename', ident], text=True).strip(), name)
        path = self.folder / 'collision.img'
        build_image(path, {name: prepare.blank(second)}, fs='fat16', spc=1, clusters=5000)
        before = path.read_bytes()
        self.assertEqual(self.run_save(path, 'store', first, 42)[-1], 10)
        self.assertEqual(self.run_save(path, 'load', first)[-1], 10)
        self.assertEqual(path.read_bytes(), before)
        self.assertEqual(self.run_save(path, 'store', second, 73)[0], 1)
        self.assertEqual(self.run_save(path, 'load', second)[:3], (1, 73, 7))
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(prepare.IdCollision):
                prepare.prepare(folder, [first, second])
            self.assertEqual(list(Path(folder).iterdir()), [])
            target = Path(folder) / name
            target.write_bytes(prepare.blank(second))
            with self.assertRaises(prepare.IdCollision):
                prepare.prepare(folder, [first])
            self.assertEqual(target.read_bytes(), prepare.blank(second))

    def test_explicit_host_conversion_preserves_old_and_never_overwrites_new(self):
        old = bytearray(record(70))
        old[:4] = b'TETR'
        struct.pack_into('<I', old, 12, 0x31565343)
        struct.pack_into('<I', old, 508, zlib.crc32(old[:508]))
        with tempfile.TemporaryDirectory() as folder:
            card = Path(folder)
            source = card / 'TETR.SAV'
            source.write_bytes(old + b'\xff' * 512)
            original = source.read_bytes()
            self.assertEqual(migrate_v1.migrate(card, 'TETR', MAGIC)[0], 'converted')
            new = (card / NAME).read_bytes()
            prepare.validate(new, MAGIC)
            self.assertEqual(new[528:564], old[16:52])
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual(migrate_v1.migrate(card, 'TETR', MAGIC)[0], 'kept')
            self.assertEqual((card / NAME).read_bytes(), new)
            with self.assertRaises(prepare.IdCollision):
                prepare.validate(new, OTHER)
            path = self.fixture(first=new[512:])
            self.assertEqual(self.run_save(path)[:3], (1, 70, 7))

    def test_actual_simulator_backend(self):
        path = self.fixture()
        self.assertEqual(self.run_save(path, 'store', MAGIC, 123, exe=self.host)[0], 1)
        self.assertEqual(self.run_save(path, exe=self.host)[:3], (1, 123, 7))

    @unittest.skipIf(COMPACT, 'compact firmware supports one-sector slots')
    def test_larger_variable_payloads_and_reserved_capacity(self):
        for fs, mbr, spc, fragment in [('fat16', False, 1, True), ('fat32', True, 4, True)]:
            raw = prepare.blank(MAGIC, 4096)[512:]
            path = self.fixture(fs, mbr, spc, fragment, raw)
            self.assertEqual(self.run_save(path, 'large-store', MAGIC, 4096, 11)[0], 1)
            self.assertEqual(self.run_save(path, 'large-load', MAGIC, 4096, 11)[0], 1)
            before = path.read_bytes()
            self.assertEqual(self.run_save(path, 'large-store', MAGIC, 4096, 11)[0], 1)
            self.assertEqual(path.read_bytes(), before)
            self.assertEqual(self.run_save(path, 'large-store', MAGIC, 2000, 12)[0], 1)
            self.assertEqual(self.run_save(path, 'large-load', MAGIC, 2000, 12)[0], 1)
            before = path.read_bytes()
            self.assertEqual(self.run_save(path, 'large-store', MAGIC, 5000, 1), (0, 9))
            self.assertEqual(path.read_bytes(), before)
        with tempfile.TemporaryDirectory() as folder:
            prepare.prepare(folder, ['com.example.big'], capacity=4096)
            self.assertEqual((Path(folder) / prepare.filename('com.example.big')).stat().st_size, 9728)
            prepare.prepare(folder, ['com.example.big'])
            with self.assertRaises(ValueError):
                prepare.prepare(folder, ['com.example.big'], capacity=9000)

    @unittest.skipIf(COMPACT, 'compact firmware supports one-sector slots')
    def test_maximum_payload(self):
        path = self.fixture(first=prepare.blank(MAGIC, prepare.MAX_DATA)[512:])
        self.assertEqual(self.run_save(path, 'large-store', MAGIC, 65516, 23)[0], 1)
        self.assertEqual(self.run_save(path, 'large-load', MAGIC, 65516, 23)[0], 1)
        before = path.read_bytes()
        self.assertEqual(self.run_save(path, 'large-store', MAGIC, 65517, 23)[0], 0)
        self.assertEqual(path.read_bytes(), before)

    @unittest.skipIf(COMPACT, 'compact firmware supports one-sector slots')
    def test_multisector_interrupted_writes(self):
        path = self.fixture(first=b'\xff' * 2048)
        self.assertEqual(self.run_save(path, 'large-store', MAGIC, 700, 1)[0], 1)
        self.assertEqual(self.run_save(path, 'large-store', MAGIC, 700, 2)[0], 1)
        before = path.read_bytes()
        with FatVolume(path) as volume:
            lba = volume.runs(NAME)[0][0] + 1
        original = before[lba*512:(lba+2)*512]
        for cut in range(1024):
            with path.open('r+b') as stream:
                stream.seek(lba * 512)
                stream.write(original)
            self.assertEqual(self.run_save(path, 'large-store', MAGIC, 700, 3, CHSAVE_CUT=cut)[0], 0)
            self.assertEqual(self.run_save(path, 'large-load', MAGIC, 700, 2)[0], 1, cut)


if __name__ == '__main__':
    unittest.main()
