"""Read-only 1073 evidence for portal coordinates and aquarium food age."""
import hashlib
from pathlib import Path
import sys

import capstone
import pefile

path = Path(sys.argv[1] if len(sys.argv) > 1 else
            'C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe')
raw = path.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == '5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251'
pe = pefile.PE(data=raw)
image = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
print('SHA256:', digest)
for label, start, end in [
    ('Portal publishes destination integer/float X, row, integer/float Y and last portal', 0x429C01, 0x429C55),
    ('Aquarium brain starts with counter zero at +0x18', 0x42AA24, 0x42AA49),
    ('Hungry fish health limit: 150', 0x5396E8, 0x5396F8),
    ('Food filter: brain type 6 and counter at least 15', 0x53971D, 0x539731),
    ('Aquarium outer update increments counter before advancing falling Y', 0x42AF1F, 0x42AF3C),
]:
    print('\n' + label)
    decoded = list(decoder.disasm(image[start-base:end-base], start))
    assert decoded and decoded[-1].address + decoded[-1].size == end, label
    for instruction in decoded:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
