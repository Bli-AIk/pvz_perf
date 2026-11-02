"""Read-only, hash-locked 1073 grave-generation rules and Whack timeline."""
import hashlib
from pathlib import Path
import sys
import capstone
import pefile

path = Path(sys.argv[1] if len(sys.argv)>1 else 'C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe')
raw = path.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == '5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251'
pe = pefile.PE(data=raw)
image,base = pe.get_memory_mapped_image(),pe.OPTIONAL_HEADER.ImageBase
decoder = capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
print('SHA256:',digest)
for label,start,end in [
    ('Whack standalone mode 30 / Adventure level 15: initial countdown 200',0x422262,0x4222A9),
    ('Whack: decrement, at 100 remaining and wave > 0 refill max(1,5-count)',0x428B31,0x428B6B),
    ('Whack: at zero reset 2000, increment wave, reset spawn countdown',0x428B7C,0x428BCF),
    ('Whack grave: randomized delay 0..49 updates, store negative age',0x428A89,0x428ADB),
    ('Grave Danger mode 45: nonfinal flags spawn zombies; nonflags after index 5 add grave',0x4293A2,0x4293DF),
    ('Night survival only: final wave, normal limit 8 / hard-endless limit 12',0x4293DF,0x429431),
    ('Grave rise uses positive age and 100-update curve',0x450673,0x45069B),
]:
    print('\n'+label)
    instructions=list(decoder.disasm(image[start-base:end-base],start))
    assert instructions and instructions[-1].address+instructions[-1].size==end,label
    for instruction in instructions:
        print(f'{instruction.address:08X}: {instruction.mnemonic} {instruction.op_str}')
