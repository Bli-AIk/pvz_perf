"""Hash-locked, read-only 1073 puzzle reward cadence evidence."""
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
    ('PuzzleIsAwardStage: stage+1, signed remainder 3 for I-Zombie / 10 for Vasebreaker', 0x42C4D0, 0x42C512),
    ('PuzzlePhaseComplete repeats the same one-based reward boundary', 0x42C520, 0x42C560),
]:
    print('\n'+label)
    decoded = list(decoder.disasm(image[start-base:end-base], start))
    assert decoded and decoded[-1].address+decoded[-1].size == end
    for instruction in decoded:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
