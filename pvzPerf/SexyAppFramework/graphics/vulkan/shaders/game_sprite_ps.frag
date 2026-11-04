#version 450
#extension GL_GOOGLE_include_directive : require
#include "texture_bindings.glsl"
// Adapted from PvZ-Portable GLInterface.cpp SHADER_CODE, commit
// 396f1c7fcf0507dcd1b79dc1ffad139eea2fb6ca (LGPL-3.0; see PvZ-Portable.LICENSE.txt).
// https://github.com/wszqkzqk/PvZ-Portable
// Match its GLES precision and textured, non-mixed fragment computation.
// Vulkan retains bindless descriptors and separate fixed-function blend modes.
// Font masks, generated images and filter aliases do not enter this pipeline.
precision mediump float;
precision highp int;
layout(set = 0, binding = 0) uniform texture2D images[PVZ_IMAGE_COUNT];
layout(set = 1, binding = 0) uniform sampler samplers[PVZ_SAMPLER_COUNT];
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 2) flat in uint slot;
layout(location = 3) flat in uint samplerIndex;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 fragColor = v_color;
    vec2 uv = v_uv;
    fragColor = texture(sampler2D(images[nonuniformEXT(slot & 0xfffu)],
                                  samplers[nonuniformEXT(samplerIndex)]), uv) * v_color;
    // Preserve the engine's opaque-input contract without affecting RGB.
    if ((slot & 0x04000000u) != 0u)
        fragColor.a = v_color.a;
    outColor = fragColor;
}
