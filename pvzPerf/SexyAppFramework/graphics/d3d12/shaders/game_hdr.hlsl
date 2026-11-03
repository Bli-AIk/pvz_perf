// Native port of vulkan/shaders/game_hdr_ps.hlsl, including presenter VS.
// b0, space0: 8 root DWORDs:
// DWORD 0,1 (bytes 0,4): float2 viewport (width, height).
// DWORD 2,3 (bytes 8,12): reserved padding.
// DWORD 4..7 (bytes 16..28): float4 hdr
// (paperWhite, maxNits, reinhardPaper, strength), matching Vulkan PresentParams.
// t0: SDR UNORM Texture2D<float4> presentation source; s0: linear/clamp.
// Output is Rec.2020 PQ for the HDR swapchain. Decode SDR and encode PQ once.
cbuffer PresentConstants : register(b0, space0)
{
    float2 viewport : packoffset(c0.x);
    float2 padding : packoffset(c0.z);
    float4 hdr : packoffset(c1);
};

Texture2D<float4> sourceImage : register(t0, space0);
SamplerState sourceSampler : register(s0, space0);

// Same 40-byte VulkanVertex input as game.hlsl; descriptor fields are unused
// by presentation because the source and sampler are bound directly.
struct VertexIn
{
    float2 position : POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    uint slot : TEXCOORD1;
    uint samplerIndex : TEXCOORD2;
};

struct VertexOut
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VertexOut VSMain(VertexIn input)
{
    VertexOut output;
    const float2 ndc = input.position / viewport * 2.0 - 1.0;
    output.position = float4(ndc.x, -ndc.y, 0.0, 1.0);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

float3 SRGBToLinear(float3 c)
{
    float3 lo = c / 12.92;
    float3 hi = pow((c + 0.055) / 1.055, 2.4);
    return lerp(lo, hi, step(0.04045, c));
}



static const float3x3 M709to2020 = {
    0.6274040, 0.3292860, 0.0433101,
    0.0690970, 0.9195400, 0.0113612,
    0.0163916, 0.0880132, 0.8955953 };
static const float3x3 M709toEXP = {
    0.753845, 0.198593, 0.047562,
    0.045746, 0.941777, 0.012477,
    -0.001211, 0.017604, 0.983607 };

#define PAPER_WHITE hdr.x
#define MAX_NITS hdr.y
#define REINHARD_PAPER hdr.z
#define STRENGTH hdr.w

static const float MSPx[5] = { 0.0, 0.3, 0.6, 0.9, 1.0 };
static const float MSPy[5] = { 0.0, 0.470306, 1.56789, 6.24642, 10.0 };
static const float MSEd[5] = { 1.56768, 2.19488, 5.92676, 23.6637, 37.5354 };
static const float MSSlope[4] = { 1.56768, 3.65859, 15.5951, 37.5354 };
static const float3 MLUMA_LERP = { 1.2, 1.1, 1.0 };
static const float COLOR_SATURATION = 1.0;
static const float3 COLOR_SAT_RGB = { 0.2, 0.2, 0.2 };
static const float GAMUT_EXPANSION = 1.0;
static const float3 GAMUT_START = { 1.0, 1.0, 1.0 };
static const float3 GAMUT_STOP = { 3.5, 5.0, 5.0 };
static const float MAX_REINHARD_NITS = 1000.0;
static const float REINHARD_ML_LERP = 0.5;

float SplineITM(float x)
{
    x = saturate(x);
    int seg = (x >= 0.9) ? 3 : (x >= 0.6) ? 2 : (x >= 0.3) ? 1 : 0;
    float dx = MSPx[seg + 1] - MSPx[seg] + 0.000001;
    float t = x - MSPx[seg];
    float c3 = (3.0 * MSSlope[seg] - 2.0 * MSEd[seg] - MSEd[seg + 1]) / dx;
    float c4 = (MSEd[seg] + MSEd[seg + 1] - 2.0 * MSSlope[seg]) / (dx * dx);
    return MSPy[seg] + MSEd[seg] * t + c3 * t * t + c4 * t * t * t;
}

float ReinhardITM(float x, float A, float K, float B)
{
    float lo = x / (1.0001 - x);
    float hi = B + x * K / (K - x);
    return x > 0.5 ? hi : lo;
}

float4 ConvertHdr(float3 signal)
{
    signal = saturate(signal);
    float3 c = SRGBToLinear(signal);
    float lum = dot(c, float3(0.212500, 0.715400, 0.072100));
    float Cb = dot(c, float3(-0.099910, -0.336090, 0.436000));
    float Cr = dot(c, float3(0.615000, -0.558610, -0.056390));
    float A = MAX_REINHARD_NITS / REINHARD_PAPER;
    float K = (A + 0.0001) / (A - 0.9999);
    float B = 1.0 / (A + 1.0001);
    float4 x4 = float4(c, lum);
    float4 yRein = select(x4 > 0.5, B + x4 * K / (K - x4), x4 / (1.0001 - x4));
    float4 ySpl = float4(SplineITM(c.r), SplineITM(c.g), SplineITM(c.b), SplineITM(lum));
    ySpl *= 100.0 / REINHARD_PAPER;
    float4 curve = lerp(ySpl, yRein, REINHARD_ML_LERP);
    float3 pres = curve.w * (c / max(lum, 0.0001));
    float3 rec = float3(curve.w + 1.280330 * Cr,
        curve.w - 0.214820 * Cb - 0.380590 * Cr,
        curve.w + 2.127980 * Cb);
    float3 outCol = MLUMA_LERP * pres + (1.0 - MLUMA_LERP) * rec;
    float3 satW = COLOR_SATURATION * COLOR_SAT_RGB;
    outCol = (1.0 - satW) * outCol + satW * curve.rgb;
    outCol = 0.9 * outCol + 0.1 * (c * A);
    float tGate = saturate((MAX_NITS - REINHARD_PAPER) / (MAX_REINHARD_NITS - REINHARD_PAPER));
    outCol = lerp(c, outCol, tGate);
    outCol *= REINHARD_PAPER / PAPER_WHITE;
    float3 s2020 = mul(M709to2020, outCol);
    float mx = max(outCol.x, max(outCol.y, outCol.z));
    float3 gate = saturate((mx - GAMUT_START) / (GAMUT_STOP - GAMUT_START));
    float3 sGam = lerp(s2020, mul(M709toEXP, outCol), gate);
    float3 s = GAMUT_EXPANSION > 0 ? sGam : s2020;
    float3 sdrRef = mul(M709to2020, c) * (REINHARD_PAPER / PAPER_WHITE);
    s = lerp(sdrRef, s, STRENGTH);
    const float3x3 M2020to709 = {
        1.6604910172, -0.5876472172, -0.0728450165,
        -0.1245500243, 1.1329005300, -0.0083484533,
        -0.0181511207, -0.1005786015, 1.1187294311 };
    return float4(mul(M2020to709, s) * (PAPER_WHITE / 80.0), 1.0);
}

float4 PSMain(VertexOut input) : SV_Target0
{
    return ConvertHdr(sourceImage.Sample(sourceSampler, input.uv).rgb * input.color.rgb);
}
