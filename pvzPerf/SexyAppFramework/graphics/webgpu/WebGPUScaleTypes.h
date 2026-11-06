#pragma once
#include "SexyAppFramework/effect/ScaleEffectSizing.h"
namespace Sexy
{
struct WebGPUScaleImage
{
    unsigned int texture{}, format{}, width{}, height{};
};
struct WebGPUScaleStep
{
    ScaleEffectRuntimeDesc *effect{};
    ScaleEffectScaling scaling{};
};
} // namespace Sexy
