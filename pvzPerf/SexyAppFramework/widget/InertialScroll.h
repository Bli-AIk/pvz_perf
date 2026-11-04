#pragma once
#include <cstdint>

namespace Sexy {
// UI-only displacement. Owners supply absolute pointer coordinates and a clock.
struct InertialScroll {
    float position = 0;
    float velocity = 0;
    float origin = 0;
    unsigned long long touchId = 0;
    std::int64_t sampleTimeNs = 0;
    std::int64_t releaseTimeNs = 0;
    int startX = 0, startY = 0, sampleY = 0;
    bool pressed = false, dragging = false;

    void Begin(unsigned long long id, int x, int y, std::int64_t nowNs);
    void Move(unsigned long long id, int x, int y, std::int64_t nowNs, float minimum, float maximum);
    bool End(unsigned long long id, std::int64_t nowNs);
    void Cancel();
    void Advance(std::int64_t nowNs, float minimum, float maximum);
};
std::int64_t ScrollTimeNs();
}
