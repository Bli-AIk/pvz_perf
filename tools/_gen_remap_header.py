"""Reproduce the converter-only historical remap; requires an explicit output."""
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[2] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"
import argparse
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True,
                    help="Review this candidate before replacing saveConverter/src/ResourceIdRemap.h")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
final = json.loads((root / "tools/_exe_resid_final.json").read_text(encoding="utf-8"))
assert len(final) == 879, len(final)

# 现代枚举序（= modern id）
enum_text = (DEVTOOLS_ROOT / "saveConverter/src/FrozenResourceIds.h").read_text(encoding="utf-8")
i = enum_text.index('enum class ResourceId')
seg = enum_text[i:enum_text.index('RESOURCE_ID_MAX')]
names = re.findall(r'\b([A-Z0-9_]+)_ID\s*=\s*\d+\s*,', seg)
assert len(names) == 879

# Read only the frozen historical kind predicate, including the FONT_IMAGE
# atlas. Live Image* declarations no longer represent retired image slots.
image_body = enum_text.split("constexpr bool IsImageResourceId", 1)[1].split(
    "constexpr std::int32_t NormalizeImageAlias", 1)[0]
image_ranges = re.findall(r"id >= (\w+)_ID && id <= (\w+)_ID", image_body)
image_singles = re.findall(r"\bid == (\w+)_ID", image_body)
assert len(image_ranges) == 4 and image_singles == ["FONT_IMAGE_HOUSEOFTERROR28"]
image_names = set(image_singles)
for first, last in image_ranges:
    image_names.update(names[names.index(first):names.index(last) + 1])
assert image_names <= set(names), 'Frozen image kinds differ from historical enum'

exe_to_modern = [879] * 880
modern_to_exe = [879] * 880
exe_is_image = [False] * 880
for name, eid in final.items():
    mid = names.index(name)
    assert 0 <= eid < 879, (name, eid)
    assert exe_to_modern[eid] == 879 or exe_to_modern[eid] == mid, (name, eid)
    exe_to_modern[eid] = mid
    modern_to_exe[mid] = eid
    exe_is_image[eid] = name in image_names
assert all(v != 879 for v in exe_to_modern[:879]), 'exe 表有洞'
assert all(v != 879 for v in modern_to_exe[:879]), 'modern 表有洞'
# 双射校验
assert sorted(exe_to_modern[:879]) == list(range(879))
assert sorted(modern_to_exe[:879]) == list(range(879))

def fmt_ints(xs, per=12):
    lines = []
    for k in range(0, len(xs), per):
        lines.append('    ' + ', '.join(str(v) for v in xs[k:k+per]) + ',')
    return '\n'.join(lines)

def fmt_bools(xs, per=40):
    lines = []
    for k in range(0, len(xs), per):
        lines.append('    ' + ', '.join('1' if v else '0' for v in xs[k:k+per]) + ',')
    return '\n'.join(lines)

hdr = f'''#pragma once
// 由 tools/_gen_remap_header.py 生成 —— 勿手改。
// 来源：1.2.0.1073 PlantsVsZombies.exe 的 .data gResources 指针数组
// @VA 0x71da08（0x726e14 是首项 IMAGE_BLANK 变量地址）。原提取 872 项，
// 另 7 个长名项已按完整字符串 → 装载函数变量 → 表槽只读复核。
// 双向校验：6 个存档经验锚点（南瓜 232/手臂 248/梯子 307·308·310/玩偶匣 297）全中。
// 空间定义：
//   legacy(.dat)  = 1073 exe 的 ResourceId 序
//   dat64 v1/v2(规范态) = 冻结的本工程历史枚举序
// 879 = RESOURCE_ID_MAX 哨兵（两空间同值，直通不映射）。
#include <cstdint>
#include "FrozenResourceIds.h"

namespace LevelSaveRemap {{
inline constexpr int kResourceCount = 879;      // 0..878
inline constexpr int kResourceIdSentinel = 879; // RESOURCE_ID_MAX

// legacy(exe) id -> dat64(modern) id
inline constexpr std::int32_t kExeToModern[kResourceCount + 1] = {{
{fmt_ints(exe_to_modern)}
}};

// dat64(modern) id -> legacy(exe) id
inline constexpr std::int32_t kModernToExe[kResourceCount + 1] = {{
{fmt_ints(modern_to_exe)}
}};

// legacy(exe) 空间的"是图像"位图（0..878 资源 + 哨兵 0）。modern 空间沿用
// FrozenSaveResources::IsImageResourceId；不依赖运行时资源或公开图片符号。
inline constexpr bool kExeIsImage[kResourceCount + 1] = {{
{fmt_bools(exe_is_image)}
}};

constexpr bool HasBoundedResourceMappings() noexcept
{{
    for (int id = 0; id < kResourceCount; ++id)
    {{
        if (kExeToModern[id] < 0 || kExeToModern[id] >= kResourceCount ||
            kModernToExe[id] < 0 || kModernToExe[id] >= kResourceCount)
        {{
            return false;
        }}
    }}
    return true;
}}

constexpr bool HasBijectiveResourceMappings() noexcept
{{
    if (!HasBoundedResourceMappings())
    {{
        return false;
    }}
    for (int id = 0; id < kResourceCount; ++id)
    {{
        if (kModernToExe[kExeToModern[id]] != id ||
            kExeToModern[kModernToExe[id]] != id)
        {{
            return false;
        }}
    }}
    return true;
}}

constexpr bool HasConsistentImageMask() noexcept
{{
    if (!HasBoundedResourceMappings())
    {{
        return false;
    }}
    for (int id = 0; id < kResourceCount; ++id)
    {{
        if (kExeIsImage[id] !=
                FrozenSaveResources::IsImageResourceId(static_cast<FrozenSaveResources::ResourceId>(kExeToModern[id])))
        {{
            return false;
        }}
    }}
    return true;
}}

static_assert(kResourceCount == static_cast<int>(FrozenSaveResources::ResourceId::RESOURCE_ID_MAX));
static_assert(kResourceIdSentinel == kResourceCount);
static_assert(kExeToModern[kResourceIdSentinel] == kResourceIdSentinel &&
    kModernToExe[kResourceIdSentinel] == kResourceIdSentinel);
static_assert(!kExeIsImage[kResourceIdSentinel] &&
    !FrozenSaveResources::IsImageResourceId(FrozenSaveResources::ResourceId::RESOURCE_ID_MAX));
static_assert(HasBoundedResourceMappings(), "Resource mappings must stay in bounds");
static_assert(HasBijectiveResourceMappings(), "Resource mappings must be inverses");
static_assert(HasConsistentImageMask(), "Legacy image mask must match frozen historical slot kinds");
static_assert(kExeIsImage[kModernToExe[
    static_cast<int>(FrozenSaveResources::ResourceId::FONT_IMAGE_HOUSEOFTERROR28_ID)]]);
}}  // namespace LevelSaveRemap
'''
out = args.output
with out.open('x', encoding='utf-8', newline='\n') as target:
    target.write(hdr)
print('written', out, len(hdr), 'bytes')
print('一致项:', sum(1 for n in names if final[n] == names.index(n)), '/879')
