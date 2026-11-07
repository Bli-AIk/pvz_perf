"""Verify the choking-death selection in the unmodified GOTY 1073 executable/pak."""
from pathlib import Path
import hashlib
import json
import re
import struct

import capstone
import pefile

root = Path('C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN')
raw = (root / 'PlantsVsZombies.exe').read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == '5f650f52d12751e08b63e590edb0d9b9f09d087cea900baa0eb39e4af8eda251'
pe = pefile.PE(data=raw)
image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
assert image[0x6F3A60-base:].startswith(b'anim_superlongdeath\0')
instructions = list(decoder.disasm(image[0x543EA9-base:0x543F92-base], 0x543EA9))
by_address = {item.address: item for item in instructions}
assert by_address[0x543EA9].op_str == '0x64'  # Rand(100)
assert by_address[0x543F09].op_str == 'ebp, 0x63'  # result == 99
assert by_address[0x543F12].op_str == '0x6f3a60'  # TrackExists
assert by_address[0x543F43].op_str == 'esi, 0x6f3a60'  # selected animation
assert struct.unpack_from('<f', image, 0x6FDF1C-base)[0] == 14.0

pak_raw = (root / 'main.pak').read_bytes()
pak = bytes(value ^ 0xF7 for value in pak_raw)
assert pak[:8] == bytes.fromhex('c04ac0ba00000000')
offset, entries = 8, []
while not pak[offset] & 0x80:
    offset += 1
    length = pak[offset]
    offset += 1
    name = pak[offset:offset+length].decode('latin1').replace('\\', '/').lower()
    offset += length
    size = struct.unpack_from('<I', pak, offset)[0]
    offset += 12
    entries.append((name, size))
offset += 1
definition = None
for name, size in entries:
    if name == 'reanim/zombie.reanim':
        definition = pak[offset:offset+size].decode('utf-8-sig')
    offset += size
assert definition is not None
track = re.search(r'<track>\s*<name>anim_superlongdeath</name>(.*?)</track>', definition, re.S).group(1)
frames = re.findall(r'<t>(.*?)</t>', track, re.S)
cel, active = -1, []
for index, frame in enumerate(frames):
    match = re.search(r'<f>(.*?)</f>', frame)
    if match:
        cel = float(match.group(1))
    if cel >= 0:
        active.append(index)
assert active == list(range(292, 429))
report = {
    'exe_sha256': digest,
    'pak_sha256': hashlib.sha256(pak_raw).hexdigest(),
    'evidence': 'Static executable instructions and original animation resource; not an in-process reproduction',
    'track': 'anim_superlongdeath', 'total_frames': len(frames),
    'active_frame_range_inclusive': [active[0], active[-1]], 'playback_rate': 14,
    'selection': 'Rand(100)==99, eligible progression, track exists, not chilled, on-screen zombie count <=5; water death takes priority',
}
output = Path(__file__).parent
(output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
(output / 'retail-disassembly.txt').write_text('SHA256: '+digest+'\n'+''.join(
    f'{item.address:08X}: {item.mnemonic} {item.op_str}\n' for item in instructions), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
