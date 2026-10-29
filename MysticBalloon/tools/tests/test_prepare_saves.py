#!/usr/bin/env python3
"""Check SD provisioning against real versioned save-record byte layouts."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import prepare_saves as saves


def page(version,data,sequence=1):
    raw=struct.pack('<IBBH',saves.MAGIC,version,0,sequence)+data
    return (raw+struct.pack('<I',zlib.crc32(raw))).ljust(256,b'\xff')


class Provisioning(unittest.TestCase):
    def test_schema3_seed_and_non_overwrite(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertTrue(saves.prepare(d));p=Path(d)/saves.NAME;before=p.read_bytes()
            saves.validate(before);self.assertEqual(before[512+4],3)
            self.assertFalse(saves.prepare(d));self.assertEqual(p.read_bytes(),before)
            broken=bytearray(before);broken[512+476]^=1;broken[1024+476]^=1;p.write_bytes(broken)
            with self.assertRaises(ValueError):saves.prepare(d)
            self.assertEqual(p.read_bytes(),broken)

    def test_legacy_flash_recovery(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'flash.bin'
            old=struct.pack('<IIBBBB',16090,17090,15,57,62,1)
            p.write_bytes(page(1,old)+page(1,old,2))
            raw=saves.seed(p);saves.validate(raw);s=raw[1032:1500]
            self.assertEqual(struct.unpack_from('<I',s,20)[0],16090)
            self.assertEqual(struct.unpack_from('<I',s,72)[0],16090)
            self.assertEqual((s[16],s[17],s[64],s[65]),(1,0,15,57))

    def test_all_four_runs_recovered_from_v2_and_v3_flash(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'flash.bin'
            for version,stride,size in ((2,52,228),(3,56,244)):
                old=bytearray(size);old[15]=1;old[16]=4;old[17]=2
                for i in range(4):
                    off=20+i*stride;struct.pack_into('<I',old,off,5000+i*100)
                    old[off+44]=i+3;old[off+45]=20+i;old[off+46]=63;old[off+49]=3
                    if version==3:struct.pack_into('<I',old,off+52,4000+i*100)
                p.write_bytes(page(version,old)+page(version,old,2))
                raw=saves.seed(p);saves.validate(raw);s=raw[1032:1500]
                self.assertEqual((s[16],s[17]),(4,2))
                for i in range(4):
                    self.assertEqual(struct.unpack_from('<I',s,20+i*56)[0],5000+i*100)
                    self.assertEqual(struct.unpack_from('<I',s,72+i*56)[0],(5000 if version==2 else 4000)+i*100)

    def test_existing_v2_journal_is_left_for_firmware_migration(self):
        raw=bytearray(saves.seed());raw[512:]=b'\xff'*1024
        store=bytearray(436);store[15]=1;store[17]=255
        for i in range(8):store[20+52*i+46]=63;store[20+52*i+49]=3
        record=struct.pack('<IBBH',saves.MAGIC,2,0,0)+store
        raw[512:1024]=(record+struct.pack('<I',zlib.crc32(record))).ljust(512,b'\xff')
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/saves.NAME;p.write_bytes(raw)
            self.assertFalse(saves.prepare(d));self.assertEqual(p.read_bytes(),raw)


if __name__=='__main__':unittest.main()
