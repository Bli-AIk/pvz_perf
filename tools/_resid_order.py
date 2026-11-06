"""Compare recorded retail resource order with frozen converter history."""
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[2] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"
import argparse
import json
from pathlib import Path
import re

# ---- 原版: resources.xml 顺序解析（SetDefaults 切前缀/路径）----
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--original-resources", type=Path,
                    help="Optional explicit original resources.xml; defaults to the recorded EXE mapping")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
orig = []
prefix = ''
if args.original_resources:
    xml = args.original_resources.read_text(encoding='utf-8', errors='replace')
    for m in re.finditer(r'<SetDefaults[^>]*idprefix="([^"]*)"[^>]*/>|<(Image|Font|Sound)\s+id="([^"]+)"', xml):
        if m.group(1) is not None:
            prefix = m.group(1)
        else:
            orig.append(prefix + m.group(3))
else:
    recorded = json.loads((root / "tools/_exe_resid_final.json").read_text(encoding="utf-8"))
    orig = [name for name, index in sorted(recorded.items(), key=lambda item: item[1])]
print('原版资源总数:', len(orig))

# ---- 现代: converter-only 冻结历史枚举序 ----
txt = (DEVTOOLS_ROOT / "saveConverter/src/FrozenResourceIds.h").read_text(encoding='utf-8')
i = txt.index('enum class ResourceId')
seg = txt[i:]
seg = seg[:seg.index('RESOURCE_ID_MAX')]
modern = []
for line in seg.splitlines():
    line = line.strip().rstrip(',')
    if not line or line.startswith('//') or line.startswith('enum') or line.startswith('{') or line.startswith('}'):
        continue
    name = line.split('/')[-1].split('=')[0].strip()
    if re.fullmatch(r'[A-Z0-9_]+', name):
        modern.append(name[:-3] if name.endswith('_ID') else name)
print('现代枚举总数:', len(modern))

n = min(len(orig), len(modern))
bad = 0
first = []
for k in range(n):
    if orig[k] != modern[k]:
        bad += 1
        if len(first) < 12:
            first.append((k, orig[k], modern[k]))
print(f'逐位对比: 不一致 = {bad} / {n}')
for f in first:
    print('   id %3d: 原版=%s  现代=%s' % f)
if len(orig) != len(modern):
    print('!! 总数不同: 原版 %d vs 现代 %d' % (len(orig), len(modern)))
# 关键 id 抽查（转换档里出现的）
for probe in (232, 262, 278, 281, 282, 283, 299, 318, 320, 322, 323, 327, 337, 338, 340, 879):
    if probe < len(orig):
        print('id %3d = %s (现代: %s)' % (probe, orig[probe], modern[probe] if probe < len(modern) else '?'))
