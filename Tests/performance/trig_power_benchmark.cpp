// Standalone MSVC benchmark; link the real FastMath.cpp as a separate TU.
// See docs/validation.md for the command and interpretation.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "misc/FastMath.h"

struct Input { float angle, angle2; double ticks, positive; };
using Inputs = std::array<Input, 8192>;
constexpr unsigned Iterations = 2'000'000, Rounds = 9;

__declspec(noinline) static double Baseline(const Input& x) { return x.angle; }
__declspec(noinline) static double SinF(const Input& x) { return std::sin(x.angle); }
__declspec(noinline) static double CosF(const Input& x) { return std::cos(x.angle); }
__declspec(noinline) static double SinD(const Input& x) { return std::sin(double(x.angle)); }
__declspec(noinline) static double CosD(const Input& x) { return std::cos(double(x.angle)); }
__declspec(noinline) static double FastSinF(const Input& x) { return FastSin(x.angle); }
__declspec(noinline) static double FastCosF(const Input& x) { return FastCos(x.angle); }
__declspec(noinline) static double PowDecay(const Input& x) { return std::pow(20.0 / 21.0, x.ticks); }
__declspec(noinline) static double SqrtD(const Input& x) { return std::sqrt(x.positive); }
__declspec(noinline) static double CbrtD(const Input& x) { return std::cbrt(x.positive); }
// Four source expressions, matching the two skew axes in MatrixFromTransform.
__declspec(noinline) static double MatrixFast(const Input& x)
{
    return double(FastCos(x.angle)) + FastSin(x.angle) +
        FastSin(x.angle2) + FastCos(x.angle2);
}
__declspec(noinline) static double MatrixFastPair(const Input& x)
{
    float sx,cx,sy,cy;
    FastSinCos(x.angle,sx,cx);FastSinCos(x.angle2,sy,cy);
    return double(cx)+sx+sy+cy;
}
// Best case for sqrt reconstruction: this fixture stays within +/-60 degrees,
// so cosine is positive and no quadrant reduction/sign handling is needed.
__declspec(noinline) static double MatrixSqrtPair(const Input& x)
{
    const float sx=FastSin(x.angle),sy=FastSin(x.angle2);
    return double(std::sqrt(std::max(0.0f,1-sx*sx)))+sx+sy+
        std::sqrt(std::max(0.0f,1-sy*sy));
}
__declspec(noinline) static double MatrixCRT(const Input& x)
{
    return double(std::cos(x.angle)) + std::sin(x.angle) +
        std::sin(x.angle2) + std::cos(x.angle2);
}
// Same-angle four expressions from TodScaleRotateTransformMatrix. The compiler
// is free to share the duplicate sin/cos calls, just as in production /O2.
__declspec(noinline) static double MatrixSameAngleCRT(const Input& x)
{
    return double(std::cos(x.angle)) + std::sin(x.angle) +
        std::sin(x.angle) + std::cos(x.angle);
}

using Evaluate = double (*)(const Input&);
template<Evaluate Fn>
static double Measure(const Inputs& inputs, double& checksum, unsigned iterations)
{
    // Independent accumulators keep the checksum dependency from serializing
    // every operation. No volatile accesses or per-iteration timer reads.
    double a = 0, b = 0, c = 0, d = 0;
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < iterations; i += 4)
    {
        a += Fn(inputs[(i + 0) & 8191]);
        b += Fn(inputs[(i + 1) & 8191]);
        c += Fn(inputs[(i + 2) & 8191]);
        d += Fn(inputs[(i + 3) & 8191]);
    }
    const auto end = std::chrono::steady_clock::now();
    checksum += a + b + c + d;
    return std::chrono::duration<double, std::nano>(end - start).count() / iterations;
}
struct Case
{
    const char* name;
    double (*measure)(const Inputs&, double&, unsigned);
    std::array<double, Rounds> samples{};
};

int main(int argc, char** argv)
{
    DWORD_PTR allowed = 0, system = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &allowed, &system) || !allowed)
        return 2;
    const unsigned bit = argc > 1 ? unsigned(std::strtoul(argv[1], nullptr, 10)) : 0;
    if (bit >= sizeof(DWORD_PTR) * 8) return 2;
    const DWORD_PTR affinity = argc > 1 ? DWORD_PTR(1) << bit : allowed & (~allowed + 1);
    if (!(allowed & affinity) || !SetThreadAffinityMask(GetCurrentThread(), affinity))
        return 2;
    std::printf("MSVC %d, affinity 0x%llx, iterations %u, rounds %u\n",
        _MSC_FULL_VER, static_cast<unsigned long long>(affinity), Iterations, Rounds);
    Inputs inputs{};
    std::uint64_t seed = 0x60191981;
    auto random = [&]() {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
        return seed;
    };
    for (auto& x : inputs)
    {
        x.angle = float(double(random() % 2'000'001) / 1'000'000 - 1) * 1.04719755f;
        x.angle2 = float(double(random() % 2'000'001) / 1'000'000 - 1) * 1.04719755f;
        x.ticks = double(random() % 1'200'000'001) / 10'000'000.0;
        x.positive = 0.001 + double(random() % 1'000'000) / 1000;
    }
    double maxSinError = 0, maxCosError = 0;
    for (const auto& x : inputs)
    {
        maxSinError = std::max(maxSinError, std::abs(FastSinF(x) - SinF(x)));
        maxCosError = std::max(maxCosError, std::abs(FastCosF(x) - CosF(x)));
    }
    double pairSinError=0,pairCosError=0,sqrtCosError=0;
    for(unsigned i=0;i<=100000;++i) {
        const float angle=(float(i)/100000*2-1)*50.2654825f;
        float sine,cosine;FastSinCos(angle,sine,cosine);
        if(!std::isfinite(sine) || !std::isfinite(cosine)) return 3;
        pairSinError=std::max(pairSinError,std::abs(double(sine)-std::sin(double(angle))));
        pairCosError=std::max(pairCosError,std::abs(double(cosine)-std::cos(double(angle))));
        const float rawSine=FastSin(angle);
        const double referenceCos=std::cos(double(angle));
        const float reconstructed=std::copysign(std::sqrt(std::max(0.0f,1-rawSine*rawSine)),float(referenceCos));
        sqrtCosError=std::max(sqrtCosError,std::abs(double(reconstructed)-referenceCos));
    }
    std::printf("Shared reduction max error [-16pi,16pi]: sin=%.9g cos=%.9g\n",pairSinError,pairCosError);
    std::printf("Sqrt reconstruction max cosine error (oracle sign, not timed): %.9g\n",sqrtCosError);
    if(pairSinError>0.0001 || pairCosError>0.0001) return 3;
    Case cases[] = {
        {"baseline", Measure<Baseline>}, {"sin(float)", Measure<SinF>},
        {"cos(float)", Measure<CosF>}, {"sin(double)", Measure<SinD>},
        {"cos(double)", Measure<CosD>}, {"FastSin(real TU)", Measure<FastSinF>},
        {"FastCos(real TU)", Measure<FastCosF>}, {"pow(20/21,t)", Measure<PowDecay>},
        {"sqrt(double)", Measure<SqrtD>}, {"cbrt(double)", Measure<CbrtD>},
        {"matrix fast (4)", Measure<MatrixFast>},
        {"matrix shared reduction", Measure<MatrixFastPair>},
        {"matrix sqrt best case", Measure<MatrixSqrtPair>}, {"matrix CRT 2 axes (4)", Measure<MatrixCRT>},
        {"matrix CRT same axis", Measure<MatrixSameAngleCRT>}
    };
    double checksum = 0;
    for (auto& c : cases) c.measure(inputs, checksum, 100'000);
    constexpr unsigned count = sizeof(cases) / sizeof(cases[0]);
    for (unsigned round = 0; round < Rounds; ++round)
        for (unsigned j = 0; j < count; ++j)
        {
            // Relatively-prime stride rotates and permutes the measurement order.
            auto& c = cases[(round * 5 + j * 7) % count];
            c.samples[round] = c.measure(inputs, checksum, Iterations);
        }
    for (auto& c : cases)
    {
        std::sort(c.samples.begin(), c.samples.end());
        std::printf("%-26s median %8.3f ns/eval  min %8.3f max %8.3f\n",
            c.name, c.samples[Rounds / 2], c.samples.front(), c.samples.back());
    }
    std::printf("max abs error in +/-60deg: sin %.9g cos %.9g; checksum %.17g\n",
        maxSinError, maxCosError, checksum);
}
