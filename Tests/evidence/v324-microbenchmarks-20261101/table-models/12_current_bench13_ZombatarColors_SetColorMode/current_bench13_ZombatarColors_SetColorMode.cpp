// bench13 | ZombatarColors::SetColorMode | 3 case switch -> kZombatarPaletteRange palette-range table
// FormA = ac447d19^, FormB = HEAD. Discriminant theMode uniform over ZombatarColorMode 0..4.
#include <windows.h>
#include <cstdint>
#include <cstdio>

static uint64_t g_checksum = 0;
static inline void Mix(uint64_t v) { g_checksum = g_checksum * 1315423911ULL + v + 1; }

static const int kN = 2000000;
static uint32_t g_raw[kN];

enum ZombatarColorMode {
    ZOMBATAR_COLOR_SKIN = 0, ZOMBATAR_COLOR_HAIR = 1, ZOMBATAR_COLOR_BRIGHT = 2,
    ZOMBATAR_COLOR_ITEM_NOT_CHOSEN = 3, ZOMBATAR_COLOR_NOT_APPLICABLE = 4
};

struct ZombatarColors {
    ZombatarColorMode mColorMode;
    int mHoveredColor;
    int mSelectedColor;
    int mPaletteStart;
    int mColorCount;
};
static ZombatarColors g_colors;

// ================= FormA: verbatim switch (ac447d19^) =================
static void SetColorModeA(ZombatarColorMode theMode, int theSelectedColor)
{
    g_colors.mColorMode = theMode;
    g_colors.mHoveredColor = -1;
    g_colors.mSelectedColor = theSelectedColor;

    switch (g_colors.mColorMode)
    {
    case ZOMBATAR_COLOR_SKIN:
        g_colors.mPaletteStart = 0;
        g_colors.mColorCount = 12;
        break;
    case ZOMBATAR_COLOR_HAIR:
        g_colors.mPaletteStart = 12;
        g_colors.mColorCount = 18;
        break;
    case ZOMBATAR_COLOR_BRIGHT:
        g_colors.mPaletteStart = 30;
        g_colors.mColorCount = 18;
        break;
    default:
        g_colors.mPaletteStart = -1;
        g_colors.mColorCount = 0;
        break;
    }
}

// ================= FormB: HEAD table =================
namespace {
struct ZombatarPaletteRange { int mStart; int mCount; };
	static constexpr ZombatarPaletteRange kZombatarPaletteRange[5] = {
	{0, 12},   // 0 ZOMBATAR_COLOR_SKIN
	{12, 18},  // 1 ZOMBATAR_COLOR_HAIR
	{30, 18},  // 2 ZOMBATAR_COLOR_BRIGHT
	{-1, 0},   // 3 ZOMBATAR_COLOR_ITEM_NOT_CHOSEN（原 default）
	{-1, 0},   // 4 ZOMBATAR_COLOR_NOT_APPLICABLE（原 default）
	};
static_assert(sizeof(kZombatarPaletteRange) / sizeof(kZombatarPaletteRange[0]) == 5, "slot sync");
}  // namespace

static void SetColorModeB(ZombatarColorMode theMode, int theSelectedColor)
{
    g_colors.mColorMode = theMode;
    g_colors.mHoveredColor = -1;
    g_colors.mSelectedColor = theSelectedColor;

    const ZombatarPaletteRange& aRange =
        static_cast<unsigned>(g_colors.mColorMode) < static_cast<unsigned>(5)
            ? kZombatarPaletteRange[static_cast<int>(g_colors.mColorMode)]
            : kZombatarPaletteRange[ZOMBATAR_COLOR_NOT_APPLICABLE];
    g_colors.mPaletteStart = aRange.mStart;
    g_colors.mColorCount = aRange.mCount;
}

// ---------------- harness ----------------
static double g_nsPerTick = 0;
static inline double QpcNs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart * g_nsPerTick; }

static ZombatarColorMode g_inputMode;
static int g_inputSel;

static void SetupState(int i)
{
    uint32_t v = g_raw[i];
    g_colors.mColorMode = ZOMBATAR_COLOR_NOT_APPLICABLE;
    g_colors.mHoveredColor = 7;
    g_colors.mSelectedColor = 0;
    g_colors.mPaletteStart = 99;
    g_colors.mColorCount = 99;
    g_inputMode = (ZombatarColorMode)(v % 5);
    g_inputSel = (int)(v % 40);
}

int main()
{
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_nsPerTick = 1e9 / (double)f.QuadPart;
    uint32_t x = 0x9E3779B9u;
    for (int i = 0; i < kN; i++) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; g_raw[i] = x; }

    double bestA = 1e300, bestB = 1e300;
    uint64_t csA = 0, csB = 0;
    for (int r = 0; r < 5; r++) {
        double t0 = QpcNs();
        for (int i = 0; i < kN; i++) { SetupState(i); SetColorModeA(g_inputMode, g_inputSel); Mix(g_colors.mPaletteStart); Mix(g_colors.mColorCount); Mix(g_colors.mHoveredColor); Mix(g_colors.mSelectedColor); }
        double t1 = QpcNs();
        double ns = (t1 - t0) / kN; if (ns < bestA) bestA = ns;
    }
    csA = g_checksum; g_checksum = 0;
    for (int r = 0; r < 5; r++) {
        double t0 = QpcNs();
        for (int i = 0; i < kN; i++) { SetupState(i); SetColorModeB(g_inputMode, g_inputSel); Mix(g_colors.mPaletteStart); Mix(g_colors.mColorCount); Mix(g_colors.mHoveredColor); Mix(g_colors.mSelectedColor); }
        double t1 = QpcNs();
        double ns = (t1 - t0) / kN; if (ns < bestB) bestB = ns;
    }
    csB = g_checksum;

    printf("ZombatarColors::SetColorMode | before %.2f ns | after %.2f ns | diff %+.1f%% | csA=%llx csB=%llx %s\n",
        bestA, bestB, (bestB - bestA) / bestA * 100.0,
        (unsigned long long)csA, (unsigned long long)csB, csA == csB ? "checksum=OK" : "checksum=FAIL");
    return csA == csB ? 0 : 1;
}
