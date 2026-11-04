#include "InertialScroll.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Sexy {
namespace { constexpr float Deceleration = 4200.0f; }
std::int64_t ScrollTimeNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void InertialScroll::Begin(unsigned long long id, int x, int y, std::int64_t nowNs) {
    if (pressed) return;
    pressed = true; dragging = false; touchId = id;
    startX = x; startY = sampleY = y; sampleTimeNs = nowNs;
    origin = position; velocity = 0;
}
void InertialScroll::Move(unsigned long long id, int x, int y, std::int64_t nowNs, float minimum, float maximum) {
    if (!pressed || id != touchId) return;
    if (!dragging && (std::abs(x-startX) > 6 || std::abs(y-startY) > 6)) dragging = true;
    if (!dragging) return;
    position = std::clamp(origin + static_cast<float>(y-startY), minimum, maximum);
    const auto elapsed = nowNs-sampleTimeNs;
    if (elapsed >= 8'000'000) {
        const float measured = (y-sampleY) * (1.0e9f/static_cast<float>(elapsed));
        velocity = std::clamp(measured, -3000.0f, 3000.0f);
        sampleY = y; sampleTimeNs = nowNs;
    }
}
bool InertialScroll::End(unsigned long long id, std::int64_t nowNs) {
    if (!pressed || id != touchId) return false;
    const bool moved = dragging;
    if (!moved || nowNs-sampleTimeNs > 100'000'000) velocity = 0;
    pressed = dragging = false;
    releaseTimeNs = nowNs; origin = position;
    return moved;
}
void InertialScroll::Cancel() { pressed = dragging = false; velocity = 0; }
void InertialScroll::Advance(std::int64_t nowNs, float minimum, float maximum) {
    if (pressed || velocity == 0) { position=std::clamp(position,minimum,maximum); return; }
    const float age = static_cast<float>(std::max<std::int64_t>(0,nowNs-releaseTimeNs))*1.0e-9f;
    const float duration = std::abs(velocity)/Deceleration;
    const float t = std::min(age,duration);
    const float sign = velocity > 0 ? 1.0f : -1.0f;
    const float next = origin + velocity*t-sign*0.5f*Deceleration*t*t;
    position = std::clamp(next,minimum,maximum);
    if (age >= duration || position != next) velocity=0;
}
}
