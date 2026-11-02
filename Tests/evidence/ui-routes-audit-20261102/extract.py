"""Verified local excerpts from the hash-locked 1073 executable, never executed."""
import hashlib
from pathlib import Path
import sys

import capstone
import pefile

exe = Path(sys.argv[1] if len(sys.argv)>1 else
           'C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe')
raw = exe.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == '5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251'
pe = pefile.PE(data=raw)
base = pe.OPTIONAL_HEADER.ImageBase
data = pe.get_memory_mapped_image()
decoder = capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
print('SHA256:',digest)
for label,start,end in [
    ('Almanac Update: buttons, preview Plant::Update and Zombie::Update without a Board pause test',0x402F50,0x402FA1),
    ('Almanac Update: its own pool counter',0x40307D,0x40309C),
    ('Adventure award: Almanac and first Store introduction',0x40963A,0x40967D),
    ('Adventure award: later Store introductions and next Adventure',0x40970C,0x409765),
    ('Adventure award: Garden tutorial',0x409765,0x409796),
    ('Retail ZombieWonClick: cleanup, Adventure NewGame and unconditional NewGame tail call',0x43FB50,0x43FB88),
]:
    print('\n'+label)
    decoded=list(decoder.disasm(data[start-base:end-base],start))
    assert decoded and decoded[-1].address+decoded[-1].size == end,label
    for instruction in decoded:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
