#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include "D3d12Context.h"
#include "D3d12Renderer.h"
#include "D3d12ScalePipeline.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace Sexy {
const char* GetLogFilePathC(const char* name) {
    static thread_local std::string path;
    path = (std::filesystem::temp_directory_path() / name).string();
    return path.c_str();
}
}
namespace {
using namespace Sexy;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
class TestWindow {
public:
    HWND hwnd = nullptr;
    TestWindow() {
        WNDCLASSW cls{};
        cls.lpfnWndProc = DefWindowProcW;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"PvzD3d12Gate";
        Require(RegisterClassW(&cls) != 0, "RegisterClassW failed");
        hwnd = CreateWindowExW(0, cls.lpszClassName, L"D3D12 gate",
            WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr,
            cls.hInstance, nullptr);
        Require(hwnd != nullptr, "CreateWindowExW failed");
    }
    ~TestWindow() {
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassW(L"PvzD3d12Gate", GetModuleHandleW(nullptr));
    }
};
auto Quad(float x, float y, float width, float height, uint32_t slot,
          float alpha = 1.0f) {
    return std::array<GpuVertex, 6>{{
        {x,y,1,1,1,alpha,0,0,slot,0},
        {x+width,y,1,1,1,alpha,1,0,slot,0},
        {x+width,y+height,1,1,1,alpha,1,1,slot,0},
        {x,y,1,1,1,alpha,0,0,slot,0},
        {x+width,y+height,1,1,1,alpha,1,1,slot,0},
        {x,y+height,1,1,1,alpha,0,1,slot,0}
    }};
}
void Pixel(const std::vector<uint32_t>& pixels, int x, int y, uint32_t expected) {
    const auto actual = pixels.at(size_t(y) * 800 + x);
    if (actual != expected) {
        std::cerr << "pixel=" << x << ',' << y << " actual=0x" << std::hex
                  << actual << " expected=0x" << expected << std::dec << '\n';
        throw std::runtime_error("Exact pixel comparison failed");
    }
}
void UploadAndBindless(D3d12Renderer& renderer) {
    std::array<uint32_t, 20> pitched{};
    std::array<uint32_t, 15> expected{};
    for (unsigned y = 0; y < 5; ++y)
        for (unsigned x = 0; x < 3; ++x)
            pitched[y*4+x] = expected[y*3+x] = 0xff000000u | ((y*3+x+1)*0x010305u);
    auto* npot = renderer.CreateTexture(3, 5, pitched.data(), 16);
    pitched.fill(0);
    const auto snapshot = renderer.ReadTexturePixels(npot);
    Require(snapshot.width == 3 && snapshot.height == 5, "NPOT dimensions changed");
    Require(std::equal(expected.begin(), expected.end(), snapshot.pixels.begin()),
        "Upload snapshot or row pitch changed pixels");
    uint32_t red = 0xffff0000, blue = 0xff0000ff;
    auto* redImage = renderer.CreateTexture(1, 1, &red, 4);
    auto* blueImage = renderer.CreateTexture(1, 1, &blue, 4);
    red = blue = 0;
    Require(renderer.BeginScene(), "Cannot begin bindless scene");
    auto vertices = Quad(20, 20, 10, 10, D3d12Renderer::SlotOf(redImage));
    const auto blueQuad = Quad(30, 20, 10, 10, D3d12Renderer::SlotOf(blueImage));
    std::vector<GpuVertex> batch(vertices.begin(), vertices.end());
    batch.insert(batch.end(), blueQuad.begin(), blueQuad.end());
    renderer.SubmitTriangles(batch, redImage, false, false, GpuBlend::Opaque);
    auto imageQuad = Quad(50, 20, 3, 5, D3d12Renderer::SlotOf(npot));
    renderer.SubmitTriangles(imageQuad, npot, false, false, GpuBlend::Opaque);
    batch.clear();
    renderer.DestroyTexture(redImage);
    renderer.DestroyTexture(blueImage);
    renderer.DestroyTexture(npot);
    renderer.EndSceneAndPresent();
    const auto pixels = renderer.ReadScenePixels();
    Pixel(pixels, 25, 25, 0xffff0000);
    Pixel(pixels, 35, 25, 0xff0000ff);
    for (unsigned y = 0; y < 5; ++y)
        for (unsigned x = 0; x < 3; ++x)
            Pixel(pixels, 50+x, 20+y, expected[y*3+x]);
    std::cout << "npot-pitched-upload=PASS bindless-mixed-slots=PASS "
                 "destroy-before-submit=PASS\n";
}
void Coverage(D3d12Renderer& renderer) {
    std::array<uint8_t, 256> ramp{};
    for (unsigned i = 0; i < ramp.size(); ++i) ramp[i] = uint8_t(i);
    auto* mask = renderer.CreateCoverageMask(256, 1, ramp, 256);
    ramp.fill(0);
    Require(renderer.BeginScene(), "Cannot begin coverage scene");
    auto black = Quad(0, 100, 256, 256, D3d12Renderer::SlotOf(renderer.whiteTexture));
    for (auto& vertex : black) vertex.r = vertex.g = vertex.b = 0;
    renderer.SubmitTriangles(black, renderer.whiteTexture, false, false, GpuBlend::Opaque);
    std::vector<GpuVertex> rows;
    for (unsigned alpha = 0; alpha < 256; ++alpha) {
        const auto quad = Quad(0, float(100+alpha), 256, 1,
            D3d12Renderer::SlotOf(mask), float(alpha)/255);
        rows.insert(rows.end(), quad.begin(), quad.end());
    }
    renderer.SubmitCoverageTriangles(rows, mask, GpuBlend::Normal);
    renderer.DestroyTexture(mask);
    renderer.EndSceneAndPresent();
    const auto pixels = renderer.ReadScenePixels();
    for (unsigned alpha = 0; alpha < 256; ++alpha)
        for (unsigned cover = 0; cover < 256; ++cover) {
            const auto value = ((cover+1)*alpha)>>8;
            Pixel(pixels, int(cover), 100+int(alpha), 0xff000000u | value*0x010101u);
        }
    std::cout << "coverage-exact-65536=PASS\n";
}
void MixedSprites(D3d12Renderer& renderer) {
    const uint32_t pixel = 0x8073c92b;
    auto* texture = renderer.CreateTexture(1, 1, &pixel, 4);
    Require(renderer.BeginScene(), "Cannot begin mixed sprite scene");
    auto background = Quad(0, 0, 800, 96, D3d12Renderer::SlotOf(renderer.whiteTexture));
    for (auto& vertex : background) { vertex.r = 0.13f; vertex.g = 0.27f; vertex.b = 0.41f; }
    renderer.SubmitTriangles(background, renderer.whiteTexture, false, false, GpuBlend::Opaque);
    // The independent-filter flag with no filter and a straight input selects
    // the general shader with identical sampling semantics. Paint matching
    // columns through both paths, alternating normal/additive at every draw.
    for (int pass = 0; pass < 2; ++pass) {
        for (unsigned int layer = 0; layer < 4; ++layer) {
            for (unsigned int alpha = 0; alpha < 256; ++alpha) {
                auto vertices = Quad(float(alpha + pass * 400), 0, 1, 96,
                    D3d12Renderer::SlotOf(texture) |
                    (pass ? GpuVertex::kIndependentColorFilter : 0), float(alpha) / 255);
                renderer.SubmitTriangles(vertices, texture, false, false,
                    (alpha + layer) % 2 ? GpuBlend::Additive : GpuBlend::Normal);
            }
        }
    }
    renderer.EndSceneAndPresent();
    const auto pixels = renderer.ReadScenePixels();
    for (int y = 0; y < 96; ++y) for (int x = 0; x < 256; ++x) {
        const auto fast = pixels[size_t(y) * 800 + x];
        const auto general = pixels[size_t(y) * 800 + x + 400];
        // RGBA8 blending quantizes shader output before blending. Moving the
        // source RGB multiplication into the shader permits one byte of
        // rounding difference per layer (four layers in this comparison).
        for (unsigned int shift = 0; shift < 32; shift += 8)
            if (std::abs(int((fast >> shift) & 255) - int((general >> shift) & 255)) > 4) {
                std::cerr << "mixed x=" << x << " y=" << y << " fast=" << std::hex << fast
                    << " general=" << general << std::dec << '\n';
                throw std::runtime_error("Mixed sprite blend differs from general shader");
            }
    }
    renderer.DestroyTexture(texture);
    std::cout << "mixed-sprite-normal-additive-ramp=PASS layers=4 tolerance=4-unorm-bytes\n";
}
void IndexedGeometry(D3d12Renderer& renderer) {
    Require(renderer.BeginScene(), "Cannot begin indexed geometry scene");
    const auto slot = D3d12Renderer::SlotOf(renderer.whiteTexture);
    const auto square = Quad(280, 10, 8, 8, slot);
    std::vector<GpuVertex> quads;
    quads.reserve(20000 * 4);
    for (unsigned int i = 0; i < 20000; ++i) {
        for (const auto corner : {0, 1, 5, 2}) {
            auto vertex = square[corner];
            vertex.r = i == 19999 ? 0.0f : 1.0f;
            vertex.g = 0; vertex.b = i == 19999 ? 1.0f : 0.0f;
            quads.push_back(vertex);
        }
    }
    renderer.SubmitQuads(quads, renderer.whiteTexture, false, false, GpuBlend::Opaque);
    // Mix an existing quad prefix, triangles, then another quad in one run.
    auto triangle = Quad(300, 10, 8, 8, slot);
    renderer.SubmitTriangles(triangle, renderer.whiteTexture, false, false, GpuBlend::Opaque);
    const auto last = Quad(320, 10, 8, 8, slot);
    const std::array<GpuVertex, 4> strip{last[0], last[1], last[5], last[2]};
    renderer.SubmitQuads(strip, renderer.whiteTexture, false, false, GpuBlend::Opaque);
    renderer.EndSceneAndPresent();
    const auto pixels = renderer.ReadScenePixels();
    Pixel(pixels, 284, 14, 0xff0000ff);
    Pixel(pixels, 304, 14, 0xffffffff);
    Pixel(pixels, 324, 14, 0xffffffff);
    std::cout << "indexed-quads-80000-vertices-mixed-prefix-and-split=PASS\n";
}
void ImageFactories(D3d12Renderer& renderer) {
    const uint32_t sourcePixel = 0x80402010;
    auto* source = renderer.CreateTexture(1, 1, &sourcePixel, 4);
    const GpuImageProcessing processing{0,0,1,1,255,0,255,255,GpuImageOperation::Colorize};
    auto* colored = renderer.ProcessImage(source, processing);
    Require(renderer.ReadTexturePixels(colored).pixels[0] == 0x7f3f000f,
        "Color factory changed byte semantics");
    auto* padded = renderer.CopyTextureRegion(source, 0, 0, 3, 5);
    const auto copy = renderer.ReadTexturePixels(padded);
    Require(copy.pixels[0] == sourcePixel, "GPU region copy changed source pixel");
    for (size_t i = 1; i < copy.pixels.size(); ++i)
        Require(copy.pixels[i] == 0, "GPU region copy padding was not zero");
    const uint32_t maskPixel = 0x00000033;
    auto* mask = renderer.CreateTexture(1, 1, &maskPixel, 4);
    bool mismatchRejected=false;
    try { renderer.ComposeAlphaRaw(padded,mask,0,false); }
    catch (const std::invalid_argument&) { mismatchRejected=true; }
    Require(mismatchRejected,"GPU alpha composition accepted unequal dimensions");
    auto* composed = renderer.ComposeAlpha(source, mask, 0, false);
    Require(renderer.ReadTexturePixels(composed).pixels[0] == 0x33402010,
        "GPU alpha composition changed RGB/alpha bytes");
    auto* colorOnly=renderer.ComposeAlpha(source,mask,0x80472513u,true);
    Require(renderer.ReadTexturePixels(colorOnly).pixels[0]==0xb3472513u,
        "GPU color-only composition changed packed-alpha OR semantics");
    renderer.DestroyTexture(colorOnly);
    const uint32_t bluePixel = 0xff0000ff;
    auto* blue = renderer.CreateTexture(1, 1, &bluePixel, 4);
    auto* target = renderer.CreateRenderImage(3, 5);
    renderer.SetRenderImage(target);
    Require(renderer.BeginScene(), "Cannot begin offscreen scene");
    renderer.SubmitTriangles(Quad(0,0,3,5,D3d12Renderer::SlotOf(blue)),
        blue, false, false, GpuBlend::Opaque);
    renderer.EndSceneAndPresent();
    renderer.SetRenderImage(nullptr);
    const auto offscreen = renderer.ReadTexturePixels(target);
    for (auto pixel : offscreen.pixels)
        Require(pixel == bluePixel, "Generated image drawing changed opaque pixels");
    auto* glyphTarget = renderer.CreateRenderImage(1, 1);
    const std::array<GpuFontGlyph,1> glyph{{{0,0,1,1,0,0,1,1}}};
    renderer.SetRenderImage(glyphTarget);
    Require(renderer.BeginScene(), "Cannot begin font image scene");
    renderer.RenderFontAtlas(glyphTarget, blue, glyph, 0, false);
    renderer.EndSceneAndPresent();
    renderer.SetRenderImage(nullptr);
    // Legacy SlowStretch applies (RGB * alpha) >> 8 even for alpha 255.
    Require(renderer.ReadTexturePixels(glyphTarget).pixels[0] == 0xff0000fe,
        "GPU font factory changed legacy opaque texel");
    for (auto* image : {source,colored,padded,mask,composed,blue,target,glyphTarget})
        renderer.DestroyTexture(image);
    std::cout << "color-alpha-copy-font-offscreen=PASS\n";
}
void SourceScale(D3d12Renderer& renderer, ScaleEffectRuntimeDesc& effect) {
    const auto path = std::filesystem::temp_directory_path() /
        ("pvz-d3d12-gpu-scale-" + std::to_string(GetCurrentProcessId()) + ".hlsl");
    {
        std::ofstream source(path);
        source << R"(//!MAGPIE EFFECT
//!VERSION 4
//!TEXTURE
Texture2D INPUT;
//!TEXTURE
Texture2D OUTPUT;
//!SAMPLER
//!FILTER POINT
SamplerState pointSampler;
//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT
float4 Pass1(float2 pos) { return INPUT.SampleLevel(pointSampler, pos, 0); }
)";
        Require(bool(source), "Cannot write source scale fixture");
    }
    ScaleEffectCompiler compiler;
    std::string error;
    const bool compiled = compiler.CompileD3d12(path.string(), {}, effect, error);
    std::filesystem::remove(path);
    Require(compiled, error.c_str());
    D3d12ScaleStep step{&effect, {}};
    step.scaling.mScaleX = step.scaling.mScaleY = 2;
    renderer.PrepareScale(false, std::span<const D3d12ScaleStep>(&step, 1), 1600, 1200);
    renderer.WarmScale(false);
    renderer.SetScaleMode(true, false);
    Require(renderer.PresentRetainedScene(), "Cannot present runtime-compiled scale");
    const auto original = renderer.ReadScenePixels();
    const auto scaled = renderer.ReadScalePixels(false);
    const auto dimensions = renderer.ScaleOutput(false);
    Require(dimensions.width == 1600 && dimensions.height == 1200,
        "Runtime scale output dimensions changed");
    for (unsigned y = 0; y < 1200; ++y)
        for (unsigned x = 0; x < 1600; ++x)
            Require(scaled[size_t(y)*1600+x] == original[size_t(y/2)*800+x/2],
                "Runtime-compiled GPU nearest scale changed pixels");
    renderer.SetScaleMode(false, false);
    std::cout << "user-hlsl-runtime-scale-exact-1920000=PASS\n";
}
void StaticSlots(D3d12Renderer& renderer) {
    constexpr uint32_t maxSymbol = 2287;
    constexpr uint32_t staticBase = 4096-maxSymbol-1;
    renderer.ReserveStaticTextureSlots(maxSymbol);
    const uint32_t color = 0xff2468ac;
    std::vector<D3d12Texture*> images;
    images.reserve(2000);
    for (uint32_t symbol = 1; symbol <= 2000; ++symbol) {
        auto* image = renderer.CreateTextureStatic(1,1,&color,4,false,symbol);
        Require((D3d12Renderer::SlotOf(image)&4095u) == staticBase+symbol,
            "Static symbol did not use its compiled descriptor");
        images.push_back(image);
    }
    Require(renderer.BeginScene(), "Cannot begin static slot scene");
    renderer.SubmitTriangles(Quad(700,500,1,1,D3d12Renderer::SlotOf(images.back())),
        images.back(),false,false,GpuBlend::Opaque);
    for (auto* image : images) renderer.DestroyTexture(image);
    renderer.EndSceneAndPresent();
    Pixel(renderer.ReadScenePixels(),700,500,color);
    std::cout << "static-symbols-2000-no-dynamic-duplicates=PASS\n";
}
void PoolDraw(D3d12Renderer& renderer) {
    const uint32_t white = 0xffffffffu, red = 0xffff0000u, blue = 0xff0000ffu;
    auto* source = renderer.CreateTexture(1, 1, &white, 4);
    auto* before = renderer.CreateTexture(1, 1, &red, 4);
    auto* after = renderer.CreateTexture(1, 1, &blue, 4);
    auto* output = renderer.CreateRenderImage(800, 600);
    for (const bool offscreen : {false, true}) {
        if (offscreen) renderer.SetRenderImage(output);
        for (uint32_t layer = 0; layer < 3; ++layer) {
            for (uint32_t night = 0; night < 2; ++night) {
                Require(renderer.BeginScene(), "Cannot begin pool scene");
                renderer.SubmitTriangles(Quad(2, 2, 5, 5, D3d12Renderer::SlotOf(before)),
                    before, false, false, GpuBlend::Opaque);
                const GpuPoolParams p{720, 150, -10, -20, 0.2f, 1.1f, 0.3f, 1.7f,
                    {0, 0, 800, 600}, layer, night, 0, layer == 2 ? 3u : 1u, GpuBlend::Opaque};
                renderer.SubmitPool(source, p);
                renderer.SubmitTriangles(Quad(12, 2, 5, 5, D3d12Renderer::SlotOf(after)),
                    after, false, false, GpuBlend::Opaque);
                renderer.EndSceneAndPresent();
                if (offscreen) {
                    renderer.SetRenderImage(nullptr);
                    Require(renderer.BeginScene(), "Cannot present pool image");
                    renderer.SubmitTriangles(Quad(0, 0, 800, 600, D3d12Renderer::SlotOf(output)),
                        output, false, false, GpuBlend::Opaque);
                    renderer.EndSceneAndPresent();
                    renderer.SetRenderImage(output);
                }
                const auto pixels = renderer.ReadScenePixels();
                Pixel(pixels, 3, 3, red);
                Pixel(pixels, 13, 3, blue);
                const uint32_t alpha = layer == 2 ? (night ? 48u : 192u) : 255u;
                Pixel(pixels, 200, 350, (alpha << 24) | 0xffffffu);
                if (layer == 2)
                    Pixel(pixels, 600, 350, ((night ? 48u : 128u) << 24) | 0xffffffu);
            }
        }
        renderer.SetRenderImage(nullptr);
    }
    renderer.DestroyTexture(output);
    renderer.DestroyTexture(after);
    renderer.DestroyTexture(before);
    renderer.DestroyTexture(source);
    std::cout << "pool-three-layers-day-night-translated-offscreen-state-restore=PASS\n";
}
void Validation(ID3D12Device* device) {
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> messages;
    Require(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&messages))),
        "D3D12 debug information queue missing");
    for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i) {
        SIZE_T bytes = 0;
        Require(SUCCEEDED(messages->GetMessage(i, nullptr, &bytes)), "Debug message size failed");
        std::vector<uint8_t> storage(bytes);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        Require(SUCCEEDED(messages->GetMessage(i, message, &bytes)), "Debug message read failed");
        if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
            std::cerr << message->pDescription << '\n';
            throw std::runtime_error("D3D12 validation error");
        }
    }
    std::cout << "d3d12-debug-errors=0\n";
}

void ResizeCanvas(D3d12Renderer& renderer, ScaleEffectRuntimeDesc& effect) {
    const unsigned int red = 0xffff0000;
    auto* source = renderer.CreateTextureStatic(1, 1, &red, 4, true, 2001);
    const auto slot = D3d12Renderer::SlotOf(source);
    auto* generated = renderer.CreateRenderImage(3, 5);
    const auto generatedSlot = D3d12Renderer::SlotOf(generated);
    for (const int width : {801, 960, 999, 1065, 1066, 1399, 1400, 800}) {
        renderer.ResizeScene(width, 600);
        Require(renderer.sceneWidth == width && renderer.sceneHeight == 600,
            "Canvas dimensions did not change");
        Require(!renderer.PresentRetainedScene(), "Old-size canvas was retained after resize");
        Require(D3d12Renderer::SlotOf(source) == slot &&
            D3d12Renderer::SlotOf(generated) == generatedSlot,
            "Canvas resize changed a resident image slot");
        Require(renderer.ReadTexturePixels(source).pixels[0] == red,
            "Canvas resize changed resident image contents");
        Require(renderer.BeginScene(), "Cannot draw a resized canvas");
        const auto quad = Quad(0, 0, float(width), 600, slot);
        renderer.SubmitTriangles(quad, source, false, false, GpuBlend::Opaque);
        renderer.EndSceneAndPresent();
        const auto pixels = renderer.ReadScenePixels();
        Require(pixels.size() == size_t(width) * 600 &&
            std::all_of(pixels.begin(), pixels.end(), [=](auto pixel) { return pixel == red; }),
            "Resized canvas has incorrect pixels or uncovered edges");
        D3d12ScaleStep step{&effect, {}};
        step.scaling.mScaleX = step.scaling.mScaleY = 1;
        renderer.PrepareScale(false, {&step, 1}, unsigned(width), 600);
        renderer.WarmScale(false);
        renderer.SetScaleMode(true, false);
        Require(renderer.ScaleOutput(false).width == unsigned(width),
            "Scale chain still uses the old canvas");
        Require(renderer.ReadScalePixels(false) == pixels,
            "Rebuilt scale chain changed resized canvas pixels");
        const auto scaleOutput = renderer.ScaleOutput(false);
        renderer.ResizeScene(width, 600);
        Require(renderer.PresentRetainedScene(),
            "Same-size resize discarded the retained scene");
        const auto unchangedScaleOutput = renderer.ScaleOutput(false);
        Require(unchangedScaleOutput.resource == scaleOutput.resource &&
            unchangedScaleOutput.width == scaleOutput.width &&
            unchangedScaleOutput.height == scaleOutput.height,
            "Same-size resize rebuilt the scale output");
        Require(D3d12Renderer::SlotOf(source) == slot &&
            D3d12Renderer::SlotOf(generated) == generatedSlot &&
            renderer.ReadScenePixels() == pixels &&
            renderer.ReadScalePixels(false) == pixels,
            "Same-size resize changed resident slots or retained pixels");
        renderer.SetScaleMode(false, false);
    }
    renderer.DestroyTexture(source);
    renderer.DestroyTexture(generated);
    std::cout << "canvas-resize-800-through-1400-resident-textures-and-scale=PASS\n";
}
}
void FrameSlots(Sexy::D3d12Context& context) {
    context.WaitIdle();
    Microsoft::WRL::ComPtr<ID3D12Fence> gate;
    Require(SUCCEEDED(context.device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(gate.GetAddressOf()))), "Cannot create test queue gate");
    Require(SUCCEEDED(context.queue->Wait(gate.Get(), 1)), "Cannot gate the test queue");
    try {
        for (unsigned int frame = 0; frame < Sexy::D3d12Context::FrameCount; ++frame) {
            Require(context.BeginCommands(false) != nullptr, "Unused allocator slot stalled");
            // No WSI image: tests allocator ownership without a blocked DXGI Present.
            context.Present();
        }
        Require(context.BeginCommands(false) == nullptr, "Busy allocator was reset before completion");
    } catch (...) { gate->Signal(1); throw; }
    Require(SUCCEEDED(gate->Signal(1)), "Cannot release the test queue gate");
    context.WaitIdle();
    Require(context.BeginCommands(false) != nullptr, "Completed allocator could not be retried");
    context.SubmitAndWait();
    std::cout << "two-frame-allocator-fence-nonblocking-retry=PASS\n";
}
int main(int argc, char** argv) {
    try {
        bool arenaStress = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--arena-stress") arenaStress = true;
            else throw std::invalid_argument("Unknown D3D12 render-test argument");
        }
        if (arenaStress) throw std::invalid_argument("--arena-stress requires resident reanimation");
        TestWindow window;
        Sexy::D3d12Context context(window.hwnd, true);
        std::cout << "adapter=" << context.adapterDescription << '\n';
        FrameSlots(context);
        Sexy::ScaleEffectRuntimeDesc scaleEffect;
        try {
            Sexy::D3d12Renderer renderer(context, 800, 600, PVZ_D3D12_SHADER_DIRECTORY);
            renderer.PrepareLoadingPipelines();
            UploadAndBindless(renderer);
            MixedSprites(renderer);
            IndexedGeometry(renderer);
            Coverage(renderer);
            ImageFactories(renderer);
            PoolDraw(renderer);
            SourceScale(renderer, scaleEffect);
            StaticSlots(renderer);
            ResizeCanvas(renderer, scaleEffect);
        } catch (...) {
            Validation(context.device);
            throw;
        }
        context.WaitIdle();
        Validation(context.device);
        std::cout << "backend=D3D12 result=PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "D3D12 gate failed: " << error.what() << '\n';
        return 1;
    }
}
