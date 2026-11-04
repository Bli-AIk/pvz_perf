#version 450
#extension GL_GOOGLE_include_directive : require
#include "texture_bindings.glsl"
// Textured fragment math follows PvZ-Portable SHADER_CODE (LGPL-3.0).
// See PvZ-Portable.LICENSE.txt. Dual-source blending additionally preserves
// the engine's original alpha accumulation for both normal and additive draws.
precision mediump float;
precision highp int;
layout(set = 0, binding = 0) uniform texture2D images[PVZ_IMAGE_COUNT];
layout(set = 1, binding = 0) uniform sampler samplers[PVZ_SAMPLER_COUNT];
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in uint slot;
layout(location = 3) flat in uint samplerIndex;
layout(location = 0, index = 0) out vec4 outColor;
layout(location = 0, index = 1) out vec4 blendControl;
void main()
{
    vec4 source = texture(sampler2D(images[nonuniformEXT(slot & 0xfffu)],
                                   samplers[nonuniformEXT(samplerIndex)]), uv);
    if ((slot & 0x04000000u) != 0u) source.a = 1.0;
    vec4 fragment = source * color;
    outColor = vec4(fragment.rgb * fragment.a, fragment.a);
    // Bit 25 is internal to Vulkan; callers still pass ordinary texture slots.
    blendControl = vec4(0.0, 0.0, 0.0, (slot & 0x02000000u) != 0u ? 0.0 : fragment.a);
}
