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
from fatimg import build_image, FatVolume

MAGIC = int.from_bytes(b'TETR', 'little')
OTHER = int.from_bytes(b'CCLR', 'little')
READY, EMPTY, NO_CARD, NEEDS_SETUP, INCOMPATIBLE, DAMAGED, IO_ERROR, UNSUPPORTED, INVALID = range(9)


def record(score, generation=1, magic=MAGIC, schema=1):
    page = bytearray(512)
    struct.pack_into('<IIHBBI', page, 0, magic, generation, 36, schema, 7, 0x31565343)
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
        build_image(path, {'TETR.SAV': prepare.EMPTY if first is None else first,
                           'CCLR.SAV': prepare.EMPTY, 'OTHER.TXT': b'keep me' * 100},
                    fs=fs, mbr=mbr, spc=spc, clusters=5000 if fs == 'fat16' else 65530,
                    fragment={'TETR.SAV': 2, 'CCLR.SAV': 2} if fragment else {})
        return path

    def run_save(self, path, command='load', magic=MAGIC, *args, exe=None, **faults):
        env = dict(os.environ, CHSAVE_CARD_IMAGE=str(path))
        env.update({key: str(value) for key, value in faults.items()})
        output = subprocess.check_output([str(exe or self.exe), command, f'{magic:08X}',
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
                    allowed = {lba + i for name in ['TETR.SAV', 'CCLR.SAV']
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
            lba = volume.runs('TETR.SAV')[0][0]
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
        for data in [bytes(1024), record(1, magic=OTHER) * 2, b'bad size']:
            path = self.fixture(first=data)
            before = path.read_bytes()
            self.assertEqual(self.run_save(path)[-1], DAMAGED)
            self.assertEqual(self.run_save(path, 'store', MAGIC, 3)[0], 0)
            self.assertEqual(path.read_bytes(), before)
        self.assertEqual(self.run_save(path, 'store', int.from_bytes(b'NONE', 'little'), 3)[-1], NEEDS_SETUP)
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
        directory = raw.index(b'TETR    SAV')
        struct.pack_into('<I', raw, directory + 28, 1536)
        path.write_bytes(raw)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 30)[-1], DAMAGED)
        self.assertEqual(path.read_bytes(), raw)
        path = self.fixture()
        raw = bytearray(path.read_bytes())
        raw[raw.index(b'TETR    SAV') + 11] |= 1
        path.write_bytes(raw)
        self.assertEqual(self.run_save(path, 'store', MAGIC, 30)[-1], DAMAGED)
        self.assertEqual(path.read_bytes(), raw)

    def test_provisioning_preserves_valid_and_recoverable_saves(self):
        with tempfile.TemporaryDirectory() as folder:
            card = Path(folder)
            prepare.prepare(card, ['TETR', 'CCLR'])
            self.assertEqual((card / 'TETR.SAV').read_bytes(), prepare.EMPTY)
            target = card / 'TETR.SAV'
            target.write_bytes(record(70) + bytes(512))
            before = target.read_bytes()
            prepare.prepare(card, ['TETR', 'CCLR'])
            self.assertEqual(target.read_bytes(), before)
            target.write_bytes(b'broken')
            with self.assertRaises(ValueError):
                prepare.prepare(card, ['NEWW', 'TETR'])
            self.assertFalse((card / 'NEWW.SAV').exists())
            self.assertEqual(target.read_bytes(), b'broken')
            for invalid in ['abc!', '../x', 'LONGER', 'lowr']:
                with self.assertRaises(ValueError):
                    prepare.prepare(card, [invalid])

    def test_actual_simulator_backend(self):
        path = self.fixture()
        self.assertEqual(self.run_save(path, 'store', MAGIC, 123, exe=self.host)[0], 1)
        self.assertEqual(self.run_save(path, exe=self.host)[:3], (1, 123, 7))

    @unittest.skipIf(COMPACT, 'compact firmware supports one-sector slots')
    def test_larger_variable_payloads_and_reserved_capacity(self):
        for fs, mbr, spc, fragment in [('fat16', False, 1, True), ('fat32', True, 4, True)]:
            raw = b'\xff' * prepare.file_size(4096)
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
            prepare.prepare(folder, ['BIGG'], capacity=4096)
            self.assertEqual((Path(folder) / 'BIGG.SAV').stat().st_size, 9216)
            prepare.prepare(folder, ['BIGG'])
            with self.assertRaises(ValueError):
                prepare.prepare(folder, ['BIGG'], capacity=9000)

    @unittest.skipIf(COMPACT, 'compact firmware supports one-sector slots')
    def test_maximum_payload(self):
        path = self.fixture(first=b'\xff' * prepare.file_size(prepare.MAX_DATA))
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
            lba = volume.runs('TETR.SAV')[0][0]
        original = before[lba*512:(lba+2)*512]
        for cut in range(1024):
            with path.open('r+b') as stream:
                stream.seek(lba * 512)
                stream.write(original)
            self.assertEqual(self.run_save(path, 'large-store', MAGIC, 700, 3, CHSAVE_CUT=cut)[0], 0)
            self.assertEqual(self.run_save(path, 'large-load', MAGIC, 700, 2)[0], 1, cut)


if __name__ == '__main__':
    unittest.main()
