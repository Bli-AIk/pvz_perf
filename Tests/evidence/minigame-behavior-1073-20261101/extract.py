"""Read-only, hash-locked excerpts from verified 1073 instruction boundaries."""
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
    ('Board normal pass, then Challenge update', 0x4188CF, 0x4188F5),
    ('Challenge speed mode 29 repeats Board::UpdateGame', 0x4273AF, 0x4273C2),
    ('Independent effects outside UpdateGame', 0x418744, 0x418755),
    ('Pool counter outside UpdateGame', 0x418835, 0x418880),
    ('Board main counter advances only while playing (or special upsell)', 0x4183B0, 0x4183F4),
    ('Loss scene set before WalkIntoHouse', 0x4160C0, 0x4160CF),
    ('Aquarium input routing', 0x424C25, 0x424C6B),
    ('Aquarium bounds, existing-brain count, cost and creation', 0x42AA70, 0x42AAF8),
]:
    print('\n' + label)
    decoded = list(decoder.disasm(data[start-base:end-base], start))
    assert decoded and decoded[-1].address + decoded[-1].size == end, label
    for instruction in decoded:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
