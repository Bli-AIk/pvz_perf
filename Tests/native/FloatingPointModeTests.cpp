#include "../../pvzPerf/SexyAppFramework/misc/FloatingPointMode.h"
#include <bit>
#include <cstdio>
#include <thread>
#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace {
unsigned long long ReadControl() {
#if defined(_M_X64) || defined(__x86_64__)
    return _mm_getcsr();
#else
    unsigned long long value;
    __asm__ volatile("mrs %0, fpcr" : "=r"(value));
    return value;
#endif
}
void WriteControl(unsigned long long value) {
#if defined(_M_X64) || defined(__x86_64__)
    _mm_setcsr(static_cast<unsigned>(value));
#else
    __asm__ volatile("msr fpcr, %0\n\tisb" : : "r"(value) : "memory");
#endif
}
#if defined(_M_X64) || defined(__x86_64__)
constexpr auto FlushMask = 0x8040ULL;
constexpr auto RoundingMask = 0x6000ULL;
#else
constexpr auto FlushMask = 1ULL << 24;
constexpr auto RoundingMask = 3ULL << 22;
#endif
bool CheckThread() {
    const auto original = ReadControl();
    // Start with flushing off and non-default rounding. Only FZ/DAZ may change.
    const auto seed = (original & ~(FlushMask | RoundingMask)) | RoundingMask;
    WriteControl(seed);
    Sexy::EnableDenormalFlushForCurrentThread();
    bool ok = ReadControl() == (seed | FlushMask);
    Sexy::EnableDenormalFlushForCurrentThread();
    ok = ok && ReadControl() == (seed | FlushMask);
    WriteControl((original & ~RoundingMask) | FlushMask);
    volatile float normal = std::bit_cast<float>(0x00800000u);
    volatile float quarter = 0.25f;
    volatile float underflow = normal * quarter;
    volatile float subnormal = std::bit_cast<float>(0x00400000u);
    volatile float twice = 2.0f;
    volatile float inputFlushed = subnormal * twice;
    volatile float ordinary = 1.5f;
    volatile float ordinaryProduct = ordinary * twice;
    ok = ok && underflow == 0.0f && inputFlushed == 0.0f && ordinaryProduct == 3.0f;
    WriteControl(original);
    return ok;
}
}
int main() {
    const auto original = ReadControl();
    bool mainOk = CheckThread(), workerOk = false;
    std::thread worker([&] { workerOk = CheckThread(); });
    worker.join();
    const bool ok = mainOk && workerOk && ReadControl() == original;
    std::printf("%s denormal input/result flush, normal product, preserved rounding/masks, idempotence and worker initialization\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
