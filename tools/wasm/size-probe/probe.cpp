#include "Sexy.TodLib/generated/GeneratedDefinitions.h"
#include <emscripten/emscripten.h>
#include <cstdint>
extern "C" {
EMSCRIPTEN_KEEPALIVE int pvz_reanim_count() { return GeneratedDefinitions::ReanimationCount; }
EMSCRIPTEN_KEEPALIVE int pvz_particle_count() { return GeneratedDefinitions::ParticleCount; }
EMSCRIPTEN_KEEPALIVE std::uintptr_t pvz_reanim_definitions() {
    return reinterpret_cast<std::uintptr_t>(GeneratedDefinitions::Reanimations);
}
EMSCRIPTEN_KEEPALIVE std::uintptr_t pvz_particle_definitions() {
    return reinterpret_cast<std::uintptr_t>(GeneratedDefinitions::Particles);
}
}
