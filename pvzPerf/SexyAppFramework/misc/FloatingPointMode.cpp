#include "FloatingPointMode.h"

#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

void Sexy::EnableDenormalFlushForCurrentThread() noexcept
{
#if defined(_M_X64) || defined(__x86_64__)
    // FTZ flushes tiny results; DAZ treats subnormal inputs as signed zero.
    // Preserve rounding, exception masks and status. This is thread state,
    // not a per-update operation and not a request for fast-math rewrites.
    _mm_setcsr(_mm_getcsr() | 0x8040u);
#elif defined(__aarch64__)
    // Under the default ARM floating-point behavior, FZ covers FP32/FP64
    // subnormal inputs and results. FP16 is not used by CPU game logic.
    unsigned long long control;
    __asm__ volatile("mrs %0, fpcr" : "=r"(control));
    control |= 1ULL << 24;
    __asm__ volatile("msr fpcr, %0\n\tisb" : : "r"(control) : "memory");
#else
#error Denormal flushing requires the supported x86-64 or AArch64 CPU target.
#endif
}
