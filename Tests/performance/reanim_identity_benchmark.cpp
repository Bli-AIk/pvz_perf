// Current integer-ID linear scan versus the actual production MPHF API.
// This isolates lookup structure, not the retired string parser or original EXE.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "LawnApp.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/generated/GeneratedDefinitions.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

bool (*gAppCloseRequest)();
namespace {
struct Query { Reanimation* animation; ReanimTrackId id; int expected; };
volatile unsigned long long sink;
__declspec(noinline) int Scan(const Query& q)
{
    const auto& tracks = gReanimatorDefArray[int(q.animation->mReanimationType)].mTracks;
    for (int i = 0; i < tracks.count; ++i)
        if (tracks.tracks[i].mTrackId == q.id) return i;
    return -1;
}
__declspec(noinline) int Production(const Query& q)
{
    return q.animation->FindTrackIndex(q.id);
}
}
int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        if (!SetProcessAffinityMask(GetCurrentProcess(), 0x4000)) throw std::runtime_error("LP14 affinity failed");
        LawnApp app;
        gLawnApp = &app;
        app.mReadFromRegistry = false;
        ReanimatorLoadDefinitions(gLawnReanimationArray, NUM_REANIMS);
        Reanimation animations[NUM_REANIMS];
        std::vector<Query> queries;
        for (int type = 0; type < NUM_REANIMS; ++type) {
            const auto& tracks = gReanimatorDefArray[type].mTracks;
            animations[type].mReanimationType = ReanimationType(type);
            for (int index = 0; index < tracks.count; ++index) {
                Query q{&animations[type], tracks.tracks[index].mTrackId, index};
                q.expected = Scan(q); // First duplicate wins, matching the production contract.
                if (Production(q) != q.expected) throw std::runtime_error("MPHF exhaustive mismatch");
                queries.push_back(q);
            }
        }
        if (queries.empty()) throw std::runtime_error("No definition corpus");
        std::printf("# exhaustive valid queries=%zu; integer scan, not a string-scan baseline\n", queries.size());
        std::puts("kernel,round,queries,ns_per_query,checksum");
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        using Function = int (*)(const Query&);
        for (int round = 0; round < 7; ++round) for (int side = 0; side < 2; ++side) {
            const int kernel = side ^ (round & 1);
            Function volatile function = kernel ? Production : Scan;
            unsigned long long checksum = 0;
            LARGE_INTEGER begin, end;
            QueryPerformanceCounter(&begin);
            for (int repeat = 0; repeat < 1024; ++repeat)
                for (const auto& q : queries) checksum += function(q);
            QueryPerformanceCounter(&end);
            sink = checksum;
            if (round) std::printf("%s,%d,%zu,%.6f,%llu\n", kernel ? "production_mphf_api" : "current_id_scan_model",
                round, queries.size() * 1024, (end.QuadPart - begin.QuadPart) * 1e9 / frequency.QuadPart / (queries.size() * 1024), checksum);
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
