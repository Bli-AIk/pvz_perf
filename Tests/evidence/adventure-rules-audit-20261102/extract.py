"""Read-only, hash-locked 1073 adventure wave/progression evidence."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import capstone
import pefile

path = Path(sys.argv[1] if len(sys.argv) > 1 else
            'C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe')
raw = path.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == '5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251'
pe = pefile.PE(data=raw)
image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
waves = struct.unpack_from('<50i', image, 0x722CD0-base)
assert waves == (4,6,8,10,8,10,20,10,20,20,10,20,10,20,10,10,20,10,20,20,
                 10,20,20,30,20,20,30,20,30,30,10,20,10,20,20,10,20,10,20,20,
                 10,20,20,30,20,20,30,20,30,30)
print('SHA256:', digest)
print('Wave table 00722CD0:', json.dumps(waves))
for label, start, end in [
    ('Wave table lookup and replay increase, excluding miniboss; Whack override precedes this block', 0x40BC79, 0x40BCEB),
    ('New plant progression index by area/sublevel, excluding two reward gaps per ten levels', 0x456F60, 0x456F8A),
    ('Final death reward: note exceptions, replay money bag, first-play special items and silver sunflower', 0x540950, 0x540A0F),
    ('Shared replay/default money-bag choice', 0x540AA1, 0x540AA6),
]:
    print('\n'+label)
    decoded = list(decoder.disasm(image[start-base:end-base], start))
    assert decoded and decoded[-1].address+decoded[-1].size == end, label
    for instruction in decoded:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
