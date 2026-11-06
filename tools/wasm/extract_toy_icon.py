"""Extract the actual RT_GROUP_ICON/RT_ICON resources, without resizing."""
from pathlib import Path
import argparse,struct
import pefile
from PIL import Image

def extract(exe, output):
    output.mkdir(parents=True,exist_ok=True)
    pe=pefile.PE(str(exe));resources={e.id:e for e in pe.DIRECTORY_ENTRY_RESOURCE.entries}
    group=resources[14].directory.entries[0].directory.entries[0].data.struct
    raw=pe.get_data(group.OffsetToData,group.Size)
    reserved,kind,count=struct.unpack_from('<HHH',raw)
    assert reserved==0 and kind==1
    icons={e.id:e.directory.entries[0].data.struct for e in resources[3].directory.entries}
    entries=[];payloads=[];offset=6+count*16
    for i in range(count):
        w,h,colors,res,planes,bits,size,ident=struct.unpack_from('<BBBBHHIH',raw,6+i*14)
        data=icons[ident];payload=pe.get_data(data.OffsetToData,data.Size)
        assert len(payload)==size
        entries.append(struct.pack('<BBBBHHII',w,h,colors,res,planes,bits,len(payload),offset))
        payloads.append(payload);offset+=len(payload)
    icon=output/'toy-icon.ico';icon.write_bytes(struct.pack('<HHH',0,1,count)+b''.join(entries)+b''.join(payloads))
    source=Image.open(icon);largest=max(source.info['sizes'],key=lambda s:s[0]*s[1]);png=output/f'toy-icon-{largest[0]}.png'
    source.ico.getimage(largest).save(png)
    print(f'Extracted {count} EXE icon sizes; PNG {largest}: {png}')
    pe.close()
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('exe',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args();extract(args.exe,args.output)
