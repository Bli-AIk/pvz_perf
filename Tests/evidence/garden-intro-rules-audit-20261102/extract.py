"""Read only the hash-locked 1073 Intro pre-update and dispatch excerpts."""
import hashlib
from pathlib import Path
import sys

import capstone
import pefile

exe = Path(sys.argv[1] if len(sys.argv) > 1 else
           'C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe')
raw = exe.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == '5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251'
pe = pefile.PE(data=raw)
base = pe.OPTIONAL_HEADER.ImageBase
data = pe.get_memory_mapped_image()
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
print('SHA256:', digest)
for label, start, end in [
    ('Movie dispatcher: mode 72, add 10 and call UpdateIntro', 0x43F540, 0x43F558),
    ('Intro time 10 calls LoadIntroBoard', 0x4445D6, 0x4445E1),
    ('LoadIntroBoard: pre-update flag, 100 virtual Board updates, restore flag', 0x441A46, 0x441A68),
    ('Intro end dispatches the first Adventure game', 0x444696, 0x4446B0),
]:
    print('\n'+label)
    decoded = list(decoder.disasm(data[start-base:end-base], start))
    assert decoded and decoded[-1].address+decoded[-1].size == end, label
    for instruction in decoded:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
