#include "GLShaders.h"

namespace Sexy::GL3 {

std::string_view PoolVertexShader()
{
    return R"GLSL(#version 130
in vec2 inPosition;
in vec2 inColor;
uniform vec2 targetExtent, poolSize, poolTranslation;
uniform vec4 waveTime, poolClip;
uniform uint poolLayer, poolNight, poolSlot, poolSampler;
out vec4 color;
out vec2 uv;
flat out uint slot;
flat out uint samplerIndex;
void main()
{
    float x=inPosition.x, y=inPosition.y;
    float xp=inColor.x, yp=inColor.y;
    float t1=waveTime.x, t2=waveTime.y, t3=waveTime.z, t5=waveTime.w;
    vec2 base=vec2(x/15.0,y/5.0), delta=vec2(0.0);
    if (x!=0.0 && x!=15.0 && y!=0.0 && y!=5.0) {
        if (poolLayer==0u) {
            delta.x=sin(yp+t2)*0.002+sin(yp+t1)*0.005;
            delta.y=sin(xp+t5)*0.01+sin(xp+t3)*0.015+sin(xp+t1)*0.005;
        } else if (poolLayer==1u) {
            delta.x=sin(yp*0.2+t2)*0.015+sin(yp*0.2+t1)*0.012;
            delta.y=sin(xp*0.2+t5)*0.005+sin(xp*0.2+t3)*0.015+sin(xp*0.2+t1)*0.02;
        } else {
            delta.x=sin(yp+t1*1.5)*0.004+sin(yp+t2*1.5)*0.005;
            delta.y=sin(xp*4.0+t5*2.5)*0.005+sin(xp*2.0+t3*2.5)*0.04+
                sin(xp*3.0+t1*2.5)*0.02;
        }
    }
    uv=poolLayer==2u ? (base+delta)+base : delta+base;
    vec2 position=poolLayer==2u ? vec2((704.0/15.0)*x+45.0,30.0*y+288.0) :
        inPosition*(poolSize/vec2(15.0,5.0))+vec2(35.0,279.0);
    float alpha=1.0;
    if (poolLayer==2u)
        alpha=(x==0.0 || x==15.0 || y==0.0) ? 32.0/255.0 :
            poolNight!=0u ? 48.0/255.0 : x<=7.0 ? 192.0/255.0 : 128.0/255.0;
    ivec2 clippedPosition=ivec2(position);
    if (clippedPosition.x<int(poolClip.x) || clippedPosition.y<int(poolClip.y) ||
        clippedPosition.x>=int(poolClip.z) || clippedPosition.y>=int(poolClip.w)) alpha=0.0;
    color=vec4(1.0,1.0,1.0,alpha);
    slot=poolSlot; samplerIndex=poolSampler;
    gl_Position=vec4((position+poolTranslation)/targetExtent*2.0-1.0,0.0,1.0);
}
)GLSL";
}

std::string_view SceneVertexShader()
{
    return R"GLSL(#version 130
in vec2 inPosition;
in vec4 inColor;
in vec2 inUV;
in uint inSlot;
in uint inSampler;
uniform vec2 targetExtent;
out vec4 color;
out vec2 uv;
flat out uint slot;
flat out uint samplerIndex;
void main()
{
    gl_Position = vec4(inPosition / targetExtent * 2.0 - 1.0, 0.0, 1.0);
    color = inColor;
    uv = inUV;
    slot = inSlot;
    samplerIndex = inSampler;
}
)GLSL";
}

std::string_view SpriteFragmentShader()
{
    // Uploaded straight-alpha sprites targeting the main scene need only the
    // texture unit's filtering/addressing and vertex tint. Fonts, generated
    // images and explicit color/edge filters retain SceneFragmentShader.
    return R"GLSL(#version 130
uniform sampler2D image;
in vec4 color;
in vec2 uv;
flat in uint slot;
out vec4 outColor;
void main()
{
    vec4 source = texture(image, uv);
    if ((slot & 0x04000000u) != 0u) source.a = 1.0;
    outColor = source * color;
}
)GLSL";
}

std::string_view MixedSpriteFragmentShader()
{
    return R"GLSL(#version 130
uniform sampler2D image;
in vec4 color;
in vec2 uv;
flat in uint slot;
out vec4 outColor;
out vec4 blendControl;
void main()
{
    vec4 source=texture(image,uv);
    if ((slot & 0x04000000u)!=0u) source.a=1.0;
    vec4 fragment=source*color;
    outColor=vec4(fragment.rgb*fragment.a,fragment.a);
    blendControl=vec4(0.0,0.0,0.0,(slot & 0x02000000u)!=0u?0.0:fragment.a);
}
)GLSL";
}

std::string_view SceneFragmentShader()
{
    return R"GLSL(#version 130
uniform sampler2D image;
uniform bool premultipliedOutput;
uniform bool coverageInput;
in vec4 color;
in vec2 uv;
flat in uint slot;
flat in uint samplerIndex;
out vec4 outColor;

vec3 rgbToHsl(vec3 rgb)
{
    float maxval = max(rgb.r, rgb.g);
    maxval = max(maxval, rgb.b);
    float minval = min(rgb.r, rgb.g);
    minval = min(minval, rgb.b);
    float l = (minval + maxval) / 2.0;
    if (l <= 0.0) return vec3(0.0, 0.0, l);
    float delta = maxval - minval;
    float s = delta;
    if (s <= 0.0) return vec3(0.0, s, l);
    s /= l <= 0.5 ? (minval + maxval) : (2.0 - minval - maxval);
    float r2 = (maxval - rgb.r) / delta;
    float g2 = (maxval - rgb.g) / delta;
    float b2 = (maxval - rgb.b) / delta;
    float h;
    if (maxval == rgb.r)
        h = rgb.g == minval ? (5.0 + b2) : (1.0 - g2);
    else if (maxval == rgb.g)
        h = rgb.b == minval ? (1.0 + r2) : (3.0 - b2);
    else
        h = rgb.r == minval ? (3.0 + g2) : (5.0 - r2);
    h /= 6.0;
    return vec3(h, s, l);
}

vec3 hslToRgb(vec3 hsl)
{
    float h = hsl.x;
    float sl = hsl.y;
    float l = hsl.z;
    float v = l <= 0.5 ? (l * (1.0 + sl)) : (l + sl - l * sl);
    if (v <= 0.0) return vec3(0.0);
    float y = 2.0 * l - v;
    float sv = (v - y) / v;
    h *= 6.0;
    int sextant = clamp(int(h), 0, 5);
    float vsf = v * sv * (h - float(sextant));
    float x = y + vsf;
    float z = v - vsf;
    switch (sextant) {
    case 0: return vec3(v, x, y);
    case 1: return vec3(z, v, y);
    case 2: return vec3(y, v, x);
    case 3: return vec3(y, z, v);
    case 4: return vec3(x, y, v);
    case 5: return vec3(v, y, z);
    }
    return vec3(0.0);
}

vec3 filterRgb(vec3 encodedRgb, uint colorFilterKind)
{
    if (colorFilterKind == 0u) return encodedRgb;
    if (colorFilterKind == 1u) return vec3(1.0);
    vec3 hsl = rgbToHsl(encodedRgb);
    hsl.y *= colorFilterKind == 2u ? 0.2 : 0.3;
    hsl.z *= colorFilterKind == 2u ? 1.8 : 1.2;
    vec3 rgb = hslToRgb(hsl);
    vec3 byteRgb = clamp(rgb * 255.0, vec3(0.0), vec3(255.0));
    vec3 result = vec3(ivec3(byteRgb)) / 255.0;
    return result;
}

vec4 filterSample(vec4 source, bool straightResult)
{
    bool generated = (slot & 0x80000000u) != 0u;
    uint colorFilterKind = (slot >> 28u) & 3u;
    if (colorFilterKind != 0u) {
        vec3 encodedRgb = source.rgb;
        if (generated)
            encodedRgb = source.a > 0.0 ? encodedRgb / source.a : vec3(0.0);
        vec3 filteredRgb = filterRgb(encodedRgb, colorFilterKind);
        vec3 result = straightResult ? filteredRgb : filteredRgb * source.a;
        source.rgb = result;
    } else if (generated) {
        if (straightResult)
            source.rgb = source.a > 0.0 ? source.rgb / source.a : vec3(0.0);
    } else if (!straightResult) {
        source.rgb *= source.a;
    }
    return source;
}

int repeatPixel(int pixel, int extent)
{
    // GLSL 1.30 leaves remainder with negative operands undefined.
    return pixel >= 0 ? pixel % extent : extent - 1 - ((-1 - pixel) % extent);
}

ivec2 addressPixel(ivec2 pixel)
{
    ivec2 extent = textureSize(image, 0);
    if ((samplerIndex & 2u) != 0u)
        return ivec2(repeatPixel(pixel.x, extent.x), repeatPixel(pixel.y, extent.y));
    return clamp(pixel, ivec2(0), extent - ivec2(1));
}

vec4 rawTexel(ivec2 pixel)
{
    return texelFetch(image, addressPixel(pixel), 0);
}

vec4 filterTexel(ivec2 pixel, bool straightResult)
{
    pixel = addressPixel(pixel);
    vec4 source = texelFetch(image, pixel, 0);
    if ((slot & 0x04000000u) != 0u) source.a = 1.0;
    if ((slot & 0x08000000u) != 0u &&
        (slot & 0x80000000u) == 0u && source.a == 0.0) {
        ivec3 rgbSum = ivec3(0);
        int count = 0;
        for (int dy = -1; dy <= 1; dy += 2) {
            for (int dx = -1; dx <= 1; ++dx) {
                ivec2 neighborPixel = pixel + ivec2(dx, dy);
                if (any(lessThan(neighborPixel, ivec2(0))) ||
                    any(greaterThanEqual(neighborPixel, textureSize(image, 0))))
                    continue;
                vec4 neighbor = texelFetch(image, neighborPixel, 0);
                if (neighbor.a != 0.0) {
                    vec3 byteRgb = round(clamp(neighbor.rgb, vec3(0.0), vec3(1.0)) * 255.0);
                    rgbSum += ivec3(byteRgb);
                    ++count;
                }
            }
        }
        if (count > 0) {
            vec3 correctedRgb = vec3(rgbSum / count) / 255.0;
            source.rgb = correctedRgb;
        } else {
            source.rgb = vec3(0.0);
        }
    }
    return filterSample(source, straightResult);
}

vec4 rawSource()
{
    if ((samplerIndex & 1u) == 0u)
        return rawTexel(ivec2(floor(uv * vec2(textureSize(image, 0)))));
    vec2 position = uv * vec2(textureSize(image, 0)) - vec2(0.5);
    ivec2 base = ivec2(floor(position));
    vec2 weight = fract(position);
    vec4 top = mix(rawTexel(base), rawTexel(base + ivec2(1, 0)), weight.x);
    vec4 bottom = mix(rawTexel(base + ivec2(0, 1)),
        rawTexel(base + ivec2(1, 1)), weight.x);
    return mix(top, bottom, weight.y);
}

vec4 filterSource(bool straightResult)
{
    if ((samplerIndex & 1u) == 0u)
        return filterTexel(ivec2(floor(uv * vec2(textureSize(image, 0)))), straightResult);
    vec2 position = uv * vec2(textureSize(image, 0)) - vec2(0.5);
    ivec2 base = ivec2(floor(position));
    vec2 weight = fract(position);
    vec4 top = mix(filterTexel(base, straightResult),
        filterTexel(base + ivec2(1, 0), straightResult), weight.x);
    vec4 bottom = mix(filterTexel(base + ivec2(0, 1), straightResult),
        filterTexel(base + ivec2(1, 1), straightResult), weight.x);
    return mix(top, bottom, weight.y);
}

void main()
{
    uint colorFilterKind = (slot >> 28u) & 3u;
    bool sandStraight = (slot & 0x08000000u) != 0u && (slot & 0x80000000u) == 0u;
    vec4 source = rawSource();
    if ((slot & 0x04000000u) != 0u) source.a = 1.0;
    if (coverageInput) {
        uint cover = uint(round(clamp(source.r, 0.0, 1.0) * 255.0));
        uint alpha = uint(round(clamp(color.a, 0.0, 1.0) * 255.0));
        source = vec4(color.rgb, float(((cover + 1u) * alpha) >> 8u) / 255.0);
        outColor = premultipliedOutput ? vec4(source.rgb * source.a, source.a) : source;
    } else if (premultipliedOutput) {
        bool generated = (slot & 0x80000000u) != 0u;
        bool linear = (samplerIndex & 1u) != 0u;
        bool independent = (slot & 0x40000000u) != 0u;
        if (colorFilterKind != 0u || sandStraight) {
            source = (linear || sandStraight) ? filterSource(independent) :
                filterSample(source, independent);
            if (independent) source.rgb *= source.a;
        } else if (independent) {
            if (generated)
                source = linear ? filterSource(true) :
                    vec4(source.a > 0.0 ? source.rgb / source.a : vec3(0.0), source.a);
            source.rgb *= source.a;
        } else if (!generated && linear) {
            source = filterSource(false);
        } else if (!generated) {
            source.rgb *= source.a;
        }
        outColor = vec4(source.rgb * color.rgb * color.a, source.a * color.a);
    } else {
        bool generated = (slot & 0x80000000u) != 0u;
        bool filteredPremult = !generated && colorFilterKind != 0u &&
            (slot & 0x40000000u) == 0u;
        if (colorFilterKind != 0u || sandStraight) {
            bool linear = (samplerIndex & 1u) != 0u;
            source = (linear || sandStraight) ? filterSource(!generated && !filteredPremult) :
                filterSample(source, !generated && !filteredPremult);
        }
        if (generated || filteredPremult)
            source.rgb = source.a > 0.0 ? source.rgb / source.a : vec3(0.0);
        outColor = source * color;
    }
}
)GLSL";
}

std::string_view QuadVertexShader()
{
    return R"GLSL(#version 130
out vec2 uv;
void main()
{
    uv = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";
}

std::string_view ImageFragmentShader()
{
    return R"GLSL(#version 130
uniform sampler2D sourceImage;
uniform sampler2D alphaImage;
uniform ivec4 sourceRect;
uniform ivec4 alphaRect;
uniform ivec4 rectangle;
uniform ivec4 tint;
uniform uint sourceSlot;
uniform uint packedColor;
uniform uint causticU0;
uniform uint causticU1;
uniform uint causticV1;
uniform bool colorOnly;
uniform int operation;
out vec4 outColor;

uint BlueAt(uint index)
{
    ivec2 coord = ivec2(int(index % 256u), int(index / 256u));
    return uint(round(texelFetch(sourceImage, sourceRect.xy + coord, 0).b * 255.0));
}

uint BilinearLookup(uint u, uint v)
{
    uint timeU = u & 0xFFFF0000u;
    uint timeV = v & 0xFFFF0000u;
    uint factorU1 = ((u - timeU) & 0x0000FFFEu) + 1u;
    uint factorV1 = ((v - timeV) & 0x0000FFFEu) + 1u;
    uint factorU0 = 65536u - factorU1;
    uint factorV0 = 65536u - factorV1;
    uint indexU0 = (timeU >> 16u) % 256u;
    uint indexU1 = ((timeU >> 16u) + 1u) % 256u;
    uint indexV0 = (timeV >> 16u) % 256u;
    uint indexV1 = ((timeV >> 16u) + 1u) % 256u;
    return ((((factorU0 * factorV1) / 65536u) * BlueAt(indexV1 * 256u + indexU0)) / 65536u) +
           ((((factorU1 * factorV1) / 65536u) * BlueAt(indexV1 * 256u + indexU1)) / 65536u) +
           ((((factorU0 * factorV0) / 65536u) * BlueAt(indexV0 * 256u + indexU0)) / 65536u) +
           ((((factorU1 * factorV0) / 65536u) * BlueAt(indexV0 * 256u + indexU1)) / 65536u);
}


void main()
{
    ivec2 position = ivec2(gl_FragCoord.xy);
    if (operation == 3) {
        if (position.x >= 128 || position.y >= 64) discard;
        uint x = uint(position.x), y = uint(position.y);
        uint timeV1 = (256u - y) << 17u;
        uint timeV0 = y << 17u;
        uint timeU = x << 17u;
        uint a1 = BilinearLookup(timeU + causticU1, timeV1 + causticV1) & 0xFFu;
        uint a0 = BilinearLookup(timeU + causticU0, timeV0) & 0xFFu;
        uint a = (a0 + a1) / 2u;
        uint alpha;
        if (a >= 160u) alpha = 255u - 2u * (a - 160u);
        else if (a >= 128u) alpha = 5u * (a - 128u);
        else alpha = 0u;
        outColor = vec4(1.0, 1.0, 1.0, float(alpha / 3u) / 255.0);
        return;
    }
    if (any(greaterThanEqual(position, rectangle.zw))) discard;
    if (operation == 2) {
        vec4 value = texelFetch(sourceImage, sourceRect.xy + position, 0);
        uint alpha = uint(round(texelFetch(alphaImage,
            alphaRect.xy + position, 0).b * 255.0));
        if (colorOnly) {
            uint combinedColor = packedColor | (alpha << 24u);
            value = vec4(float((combinedColor >> 16u) & 255u), float((combinedColor >> 8u) & 255u),
                float(combinedColor & 255u), float(combinedColor >> 24u)) / 255.0;
        } else {
            value.a = float(alpha) / 255.0;
        }
        outColor = value;
        return;
    }
    ivec2 inputPosition = position + rectangle.xy;
    if (any(lessThan(inputPosition, ivec2(0))) ||
        any(greaterThanEqual(inputPosition, sourceRect.zw))) {
        outColor = vec4(0.0);
        return;
    }
    vec4 rgba = texelFetch(sourceImage, sourceRect.xy + inputPosition, 0);
    if ((sourceSlot & 0x80000000u) != 0u)
        rgba.rgb = rgba.a > 0.0 ? rgba.rgb / rgba.a : vec3(0.0);
    uvec4 value = uvec4(round(clamp(rgba, vec4(0.0), vec4(1.0)) * 255.0));
    if (operation == 0) {
        outColor = vec4(value) / 255.0;
        return;
    }
    uvec4 product = value * uvec4(tint);
    uvec4 result;
    if (all(lessThanEqual(tint, ivec4(255))))
        result = (product >> 8u) & uvec4(255u);
    else
        result = min(product / 255u, uvec4(255u));
    outColor = vec4(result) / 255.0;
}
)GLSL";
}

std::string_view FontFragmentShader()
{
    return R"GLSL(#version 130
uniform usampler2D xRanges;
uniform usampler2D yRanges;
uniform usampler2D xTaps;
uniform usampler2D yTaps;
uniform ivec2 xTapExtent;
uniform ivec2 yTapExtent;
uniform ivec4 glyphSrc;
uniform ivec4 glyphDst;
uniform ivec4 sourceRect;
uniform sampler2D sourceImage;
uniform uint sourceSlot;
uniform uint samplingFlags;
uniform bool forceWhite;
out vec4 outColor;

// Decode the CPU's modulo-2^32 index using only in-range int conversions.
int signedIndex(uint bits)
{
    return bits <= 0x7fffffffu ? int(bits) : -1 - int(~bits);
}

uvec4 rawPixel(ivec2 position)
{
    position = clamp(position, ivec2(0), sourceRect.zw - ivec2(1));
    vec4 rgba = texelFetch(sourceImage, sourceRect.xy + position, 0);
    if ((sourceSlot & 0x04000000u) != 0u) rgba.a = 1.0;
    if ((sourceSlot & 0x80000000u) != 0u)
        rgba.rgb = rgba.a > 0.0 ? rgba.rgb / rgba.a : vec3(0.0);
    return uvec4(round(clamp(rgba, vec4(0.0), vec4(1.0)) * 255.0));
}

uvec4 pixel(ivec2 position)
{
    uvec4 rgba = rawPixel(position);
    if ((samplingFlags & 0x08000000u) != 0u && rgba.a == 0u) {
        uvec3 rgb = uvec3(0);
        uint count = 0u;
        for (int dy = -1; dy <= 1; dy += 2) for (int dx = -1; dx <= 1; ++dx) {
            ivec2 neighbor = position + ivec2(dx, dy);
            if (any(lessThan(neighbor, ivec2(0))) ||
                any(greaterThanEqual(neighbor, sourceRect.zw)))
                continue;
            uvec4 value = rawPixel(neighbor);
            if (value.a != 0u) { rgb += value.rgb; ++count; }
        }
        rgba.rgb = count != 0u ? rgb / count : uvec3(0);
    }
    return rgba;
}

ivec2 tapPosition(uint offset, ivec2 extent)
{
    return ivec2(int(offset % uint(extent.x)), int(offset / uint(extent.x)));
}

void main()
{
    ivec2 outputPosition = ivec2(gl_FragCoord.xy) - glyphDst.xy;
    if (any(lessThan(outputPosition, ivec2(0))) ||
        any(greaterThanEqual(outputPosition, glyphDst.zw))) discard;
    uvec2 xr = texelFetch(xRanges, ivec2(outputPosition.x, 0), 0).rg;
    uvec2 yr = texelFetch(yRanges, ivec2(outputPosition.y, 0), 0).rg;
    uvec4 total = uvec4(0);
    for (uint y = 0u; y < yr.y; ++y) {
        uvec2 yt = texelFetch(yTaps, tapPosition(yr.x + y, yTapExtent), 0).rg;
        uvec4 horizontal = uvec4(0);
        for (uint x = 0u; x < xr.y; ++x) {
            uvec2 xt = texelFetch(xTaps, tapPosition(xr.x + x, xTapExtent), 0).rg;
            horizontal += pixel(glyphSrc.xy +
                ivec2(signedIndex(xt.x), signedIndex(yt.x))) * xt.y;
        }
        total += horizontal * yt.y;
    }
    uvec4 result = total >> 16u;
    result.rgb = (result.rgb * result.a) >> 8u;
    if (result.a == 0u) result.rgb = uvec3(0);
    float alpha = float(result.a) / 255.0;
    vec3 rgb = forceWhite ? vec3(1.0) : vec3(result.rgb) / 255.0;
    outColor = vec4(rgb * alpha, alpha);
}
)GLSL";
}

std::string_view PresentFragmentShader()
{
    return R"GLSL(#version 130
uniform sampler2D image;
uniform ivec2 sourceExtent;
uniform bool linearFilter;
in vec2 uv;
out vec4 outColor;
void main()
{
    vec2 flippedUV = vec2(uv.x, 1.0 - uv.y);
    outColor = texture(image, flippedUV);
}
)GLSL";
}

}
