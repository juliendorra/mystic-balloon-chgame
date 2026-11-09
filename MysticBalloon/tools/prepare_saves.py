#!/usr/bin/env python3
"""Create Mystic Balloon's preallocated SD journal without replacing user saves."""
from pathlib import Path
import argparse
import os
import struct
import zlib
import importlib.util

NAME='MYBLSAVE.DAT'
SIZE=1536
MAGIC=int.from_bytes(b'MYBL','little')

def empty_store():
    b=bytearray(468);b[15]=1;b[17]=255
    for i in range(8):b[20+56*i+46]=63;b[20+56*i+49]=3
    return b

def seed(recover=None):
    store=empty_store()
    if recover is not None:
        raw=Path(recover).read_bytes()
        if len(raw)!=512:raise ValueError('Flash backup must contain both 256-byte save pages')
        records=[]
        for off in [0,256]:
            page=raw[off:off+256]
            magic,version,flag,seq=struct.unpack_from('<IBBH',page)
            size={1:12,2:228,3:244}.get(version)
            if magic!=MAGIC or size is None:continue
            if zlib.crc32(page[:8+size])!=struct.unpack_from('<I',page,8+size)[0]:continue
            records.append((seq,version,page[8:8+size]))
        if records:
            record=records[0]
            for candidate in records[1:]:
                if 0<(candidate[0]-record[0])%65536<32768:record=candidate
            _,version,data=record
            if version==3:store[:244]=data
            elif version==2:
                store[:20]=data[:20]
                for i in range(4):
                    store[20+56*i:72+56*i]=data[20+52*i:72+52*i]
                    struct.pack_into('<I',store,72+56*i,struct.unpack_from('<I',data,20+52*i)[0])
            else:
                score,high,level,coins,highcoins,options=struct.unpack('<IIBBBB',data)
                if not (level<39 and coins<=232 and highcoins<=232 and options<=15 and score<=999999 and high<=999999):raise ValueError('Invalid legacy save')
                store[4:16]=data;store[16]=1;store[17]=0
                struct.pack_into('<I',store,20,score)
                struct.pack_into('<I',store,72,score)
                store[64]=level;store[65]=coins;store[66]=60 if level==22 else 63
            struct.pack_into('<I',store,0,1)
    descriptor=bytearray(b'\xff'*512)
    struct.pack_into('<8sII',descriptor,0,b'MYBLSD2\0',SIZE,8)
    struct.pack_into('<I',descriptor,508,zlib.crc32(descriptor[:508]))
    generation=struct.unpack_from('<I',store)[0]
    payload=struct.pack('<IBBH',MAGIC,3,0,generation&65535)+store
    record=payload+struct.pack('<I',zlib.crc32(payload))
    record+=b'\xff'*(512-len(record))
    slots=[b'\xff'*512,b'\xff'*512];slots[generation&1]=record
    return bytes(descriptor)+b''.join(slots)

def validate(raw):
    if len(raw)!=SIZE or raw[:8]!=b'MYBLSD2\0' or struct.unpack_from('<I',raw,508)[0]!=zlib.crc32(raw[:508]):raise ValueError('Existing save file is invalid; it was left untouched')
    good=0
    for off in (512,1024):
        page=raw[off:off+512]
        size={2:436,3:468}.get(page[4])
        if page[:4]==b'MYBL' and size and zlib.crc32(page[:8+size])==struct.unpack_from('<I',page,8+size)[0]:good+=1
    if not good:raise ValueError('Both save records are damaged; existing file was left untouched')

def prepare(card,recover=None):
    root=Path(card)
    if not root.is_dir():raise ValueError('Card mount folder does not exist')
    path=root/NAME
    if path.exists():
        validate(path.read_bytes());print('Preserved existing SD saves:',path);return False
    data=seed(recover)
    with path.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
    assert path.read_bytes()==data
    print('Prepared SD saves:',path);return True

def common_record(raw):
    """Import the newest legacy SD record without changing the source file."""
    validate(raw)
    chosen=None
    for offset in (512,1024):
        page=raw[offset:offset+512];size={2:436,3:468}.get(page[4])
        if page[:4]!=b'MYBL' or not size or zlib.crc32(page[:8+size])!=struct.unpack_from('<I',page,8+size)[0]:continue
        generation=struct.unpack_from('<I',page,8)[0]
        if chosen is None or 0<(generation-chosen[0])%2**32<2**31:chosen=(generation,page)
    generation,page=chosen
    store=bytearray(page[8:476]) if page[4]==3 else empty_store()
    if page[4]==2:
        store[:20]=page[8:28]
        for i in range(8):
            start=28+i*52;target=20+i*56
            store[target:target+52]=page[start:start+52]
            struct.pack_into('<I',store,target+52,struct.unpack_from('<I',page,start)[0])
    out=bytearray(512)
    struct.pack_into('<IIHBBI',out,0,MAGIC,generation,468,3,0,0x31565343)
    out[16:484]=store;struct.pack_into('<I',out,508,zlib.crc32(out[:508]))
    return bytes(out)+b'\xff'*512

APP_ID='com.juliendorra.mysticballoon'

def shared_tools():
    public=Path(__file__).resolve().parents[2]/'chgame-sd-saves/tools'
    import sys
    sys.path.insert(0,str(public))
    import prepare as shared
    import migrate_v1
    return shared,migrate_v1

def prepare_standard(card,recover=None,migrate=False):
    root=Path(card);shared,converter=shared_tools()
    target=root/shared.filename(APP_ID)
    if target.exists():
        shared.validate(target.read_bytes(),APP_ID);return False
    common=root/'MYBL.SAV';old=root/NAME
    if migrate:
        source=common.read_bytes() if common.exists() else common_record(seed(recover) if recover is not None else old.read_bytes())
        data=converter.convert(source,'MYBL',APP_ID)
        with target.open('xb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
        if target.read_bytes()!=data:raise OSError('Mystic Balloon conversion readback failed')
        print('Converted old progress; source left untouched:',target.name);return True
    if recover is not None or common.exists() or old.exists():
        raise ValueError('Old saves need explicit --migrate-legacy before installing this version')
    shared.prepare(root,[APP_ID]);return True

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--card',required=True,type=Path);ap.add_argument('--recover',type=Path)
    ap.add_argument('--migrate-legacy',action='store_true')
    args=ap.parse_args();prepare_standard(args.card,args.recover,args.migrate_legacy)
if __name__=='__main__':main()
