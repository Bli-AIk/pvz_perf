"""Extract Toolkit-compatible geometry (never HP/timers) from an official 1073 save.
Format reference: lmintlcx/pvztoolkit src/lineup.cpp, v1.22.0.
Use the bundled converter first when the source is DAT64.
"""
import argparse,base64,json,struct,zlib
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_level_save import ARRAY_LAYOUTS,Reader,SAVE_MAGIC

def extract(path):
    reader=Reader(path.read_bytes()); header=reader.sync_bytes(12)
    if struct.unpack('<III',header)!=(SAVE_MAGIC,2,0xAC9B80AC): raise ValueError('Expected official 1073 save')
    board=reader.sync_bytes(22108); scene=struct.unpack_from('<i',board,0x53E8)[0]
    cells=[0]*54; rake=0; count=0
    for layout in ARRAY_LAYOUTS:
        reader.u32(); used=reader.u32(); alive=reader.u32(); data=reader.sync_bytes(used*layout.slot_size)
        if layout.name not in ('Plant','GridItem'): continue
        for i in range(used):
            item=data[i*layout.slot_size:(i+1)*layout.slot_size]
            if not struct.unpack_from('<I',item,layout.object_size)[0]&0xffff0000: continue
            if layout.name=='Plant':
                if item[321] or item[322]: continue
                seed,col=struct.unpack_from('<ii',item,36); row=struct.unpack_from('<i',item,28)[0]
                if not 0<=seed<48: raise ValueError('Unsupported transient plant type')
                im=struct.unpack_from('<i',item,312)[0]==48; at=row*9+col; count+=1
                if seed in (16,33): cells[at]|=((1 if seed==16 else 2)<<6)|(int(im)<<5)
                elif seed==30: cells[at]|=16|(int(im)<<3)
                elif seed==35: cells[at]|=4|(int(im)<<1)
                else:
                    if cells[at]>>10: raise ValueError('More than one main plant in a tile')
                    cells[at]|=((seed+1)<<10)|(int(im)<<9)|(int(not item[323])<<8)
            else:
                if item[32]: continue
                kind=struct.unpack_from('<i',item,8)[0]; col,row=struct.unpack_from('<ii',item,16)
                if kind==1: cells[row*9+col]=(cells[row*9+col]&~0xe0)|0xc0
                elif kind==3: cells[row*9+col]|=1
                elif kind==11: rake=row+1
    rows=6 if scene in (2,3) else 5
    raw=struct.pack('<'+'H'*(rows*9),*cells[:rows*9])
    packed=zlib.compress(raw,9)+bytes([(rake<<4)|scene])
    code=base64.b64encode(bytes(x^0x54 for x in packed)).decode()
    return {'scene':scene,'code':code,'plants':count,'healthIncluded':False}
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('save',type=Path); args=parser.parse_args()
    print(json.dumps(extract(args.save),ensure_ascii=False,indent=2))
