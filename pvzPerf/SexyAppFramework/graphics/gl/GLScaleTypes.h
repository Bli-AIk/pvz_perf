#pragma once
#include "SexyAppFramework/effect/ScaleEffectSizing.h"

namespace Sexy {
struct GLScaleImage {
    unsigned int texture{};
    unsigned int format{}, width{}, height{};
};
struct GLScaleStep {
    ScaleEffectRuntimeDesc* effect{};
    ScaleEffectScaling scaling{};
};
}
