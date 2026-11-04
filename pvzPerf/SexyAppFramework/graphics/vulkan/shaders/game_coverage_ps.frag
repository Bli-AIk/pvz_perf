#version 450
// 覆盖掩码像素着色器（GLSL 原因同 game_ps.frag：nonuniformEXT 装饰）。
// 掩码走与普通纹理相同的全场数组——R8 视图在 texture2D 数组里采样得
// (r,0,0,1)，整数覆盖公式保持原样。采样器同 game_ps.frag 走数组
// （覆盖路径恒 nearest/clamp=0，由顶点携带）。
#extension GL_GOOGLE_include_directive : require
#include "texture_bindings.glsl"
precision mediump float;
precision highp int;

layout(set = 0, binding = 0) uniform texture2D coverageMasks[PVZ_IMAGE_COUNT];
layout(set = 1, binding = 0) uniform sampler samplers[PVZ_SAMPLER_COUNT];

layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in uint slot;
layout(location = 3) flat in uint samplerIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    // The inherited coverage API supplies bytes, not an RGBA image. Preserve
    // its integer effective-alpha formula; ordinary float multiplication has
    // different rounding, notably with a small mask and a translucent color.
    const uint cover = uint(round(clamp(texture(sampler2D(coverageMasks[nonuniformEXT(slot)],
                                                           samplers[nonuniformEXT(samplerIndex)]), uv).r, 0.0, 1.0) * 255.0));
    const uint alpha = uint(round(clamp(color.a, 0.0, 1.0) * 255.0));
    outColor = vec4(color.rgb, float(((cover + 1u) * alpha) >> 8u) / 255.0);
}
