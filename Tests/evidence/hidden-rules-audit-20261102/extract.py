"""Read-only 1073 hidden Shovel, paused storm and Ice timing evidence."""
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
image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
print('SHA256:', digest)
for label, start, end in [
    ('IsShovelLevel: live Board and original mode 46', 0x456CF0, 0x456D07),
    ('Conveyor common pick register starts at zero', 0x4258EB, 0x4258F0),
    ('Shovel pick: seed zero (Peashooter), weight 100, single choice', 0x425AFF, 0x425B25),
    ('Paused storm: hold at 1; threshold 150 in FLASH_1 is checked before DEC', 0x426180, 0x4261B5),
    ('Ice demonstration: original mode 42 and exact main counter 3000', 0x427424, 0x42743E),
]:
    print('\n'+label)
    decoded = list(decoder.disasm(image[start-base:end-base], start))
    assert decoded and decoded[-1].address+decoded[-1].size == end, label
    for instruction in decoded:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
