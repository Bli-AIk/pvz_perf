// Standalone browser probe linked to the actual WebGPU backend, never the game target.
#include "graphics/webgpu/WebGPURenderer.h"
#include "platform/NativeWindow.h"
#include <array>
#include <cmath>
#include <emscripten.h>
#include <stdexcept>
#include <string>
#include <vector>
namespace Sexy
{
// The probe supplies a minimal window; no game, SDL or user profile is initialized.
struct NativeWindow::Impl
{
};
NativeWindow::NativeWindow(const NativeWindowConfig &) : impl_(new Impl)
{
    Width = 64;
    Height = 64;
}
NativeWindow::~NativeWindow() = default;
} // namespace Sexy
namespace
{
void Check(bool pass, const char *message)
{
    if (!pass)
        throw std::runtime_error(message);
}
std::array<Sexy::GpuVertex, 4> Quad(Sexy::WebGPUTexture *texture, float width, float height, float alpha = 1)
{
    const auto slot = Sexy::WebGPURenderer::SlotOf(texture);
    return {{{0, 0, 1, 1, 1, alpha, 0, 0, slot, 0},
             {width, 0, 1, 1, 1, alpha, 1, 0, slot, 0},
             {0, height, 1, 1, 1, alpha, 0, 1, slot, 0},
             {width, height, 1, 1, 1, alpha, 1, 1, slot, 0}}};
}
} // namespace
int main()
{
    try
    {
        Sexy::NativeWindow window(Sexy::NativeWindowConfig{});
        Sexy::WebGPUContext context(&window, false);
        Sexy::WebGPURenderer renderer(context, 64, 64, "");
        const unsigned int original[]{0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff};
        auto *source = renderer.CreateTexture(2, 2, original, 8);
        Check(renderer.ReadTexturePixels(source).pixels == std::vector<unsigned int>(original, original + 4),
              "BGRA upload/readback");
        auto *copy = renderer.CopyTextureRegion(source, 1, 0, 1, 2);
        Check(renderer.ReadTexturePixels(copy).pixels == std::vector<unsigned int>{0xff00ff00, 0xffffffff},
              "copy region");
        const unsigned int mask[]{0x00000010, 0x00000080, 0x000000ff, 0x00000000};
        auto *maskTexture = renderer.CreateRawTexture(2, 2, mask, 8);
        auto *composed = renderer.ComposeAlpha(source, maskTexture, 0, false);
        const auto composedPixels = renderer.ReadTexturePixels(composed).pixels;
        Check(composedPixels[0] == 0x10ff0000 && composedPixels[1] == 0x8000ff00 && composedPixels[2] == 0xff0000ff,
              "alpha composition");
        auto *target = renderer.CreateRenderImage(8, 8);
        const unsigned int red = 0xffff0000, blue = 0xff0000ff;
        auto *r = renderer.CreateTexture(1, 1, &red, 4);
        auto *b = renderer.CreateTexture(1, 1, &blue, 4);
        renderer.SetRenderImage(target);
        Check(renderer.BeginScene(), "begin generated image");
        renderer.SubmitQuads(Quad(r, 8, 8, .5f), r, false, false, Sexy::GpuBlend::Normal);
        renderer.SubmitQuads(Quad(b, 8, 8, .5f), b, false, false, Sexy::GpuBlend::Additive);
        renderer.EndSceneAndPresent();
        renderer.SetRenderImage(nullptr);
        auto values = renderer.ReadTextureFloats(target);
        Check(std::abs(values[0] - .5f) < .002f && std::abs(values[2] - .5f) < .002f &&
                  std::abs(values[3] - .75f) < .002f,
              "premultiplied normal/additive");
        auto *font = renderer.CreateRenderImage(4, 4);
        const Sexy::GpuFontGlyph glyph{0, 0, 2, 2, 0, 0, 4, 4};
        renderer.RenderFontAtlas(font, source, std::span(&glyph, 1), 0, false);
        values = renderer.ReadTextureFloats(font);
        Check(values[0] > .98f && values[1] < .002f && values[3] > .99f, "font weighted generation");
        const std::array<unsigned char, 4> coverage{0, 64, 128, 255};
        auto *coverageTexture = renderer.CreateCoverageMask(2, 2, coverage, 2);
        auto *coverageTarget = renderer.CreateRenderImage(2, 2);
        renderer.SetRenderImage(coverageTarget);
        Check(renderer.BeginScene(), "begin coverage");
        renderer.SubmitCoverageTriangles(
            std::array<Sexy::GpuVertex, 6>{Quad(coverageTexture, 2, 2)[0], Quad(coverageTexture, 2, 2)[1],
                                           Quad(coverageTexture, 2, 2)[2], Quad(coverageTexture, 2, 2)[2],
                                           Quad(coverageTexture, 2, 2)[1], Quad(coverageTexture, 2, 2)[3]},
            coverageTexture, Sexy::GpuBlend::Normal);
        renderer.EndSceneAndPresent();
        renderer.SetRenderImage(nullptr);
        values = renderer.ReadTextureFloats(coverageTarget);
        Check(values[3] == 0 && std::abs(values[7] - 64 / 255.0f) < .002f && values[15] > .99f, "coverage sampling");
        // A full 16-bit stream plus one quad must submit and reuse storage, not overflow or grow pages.
        renderer.SetRenderImage(target);
        Check(renderer.BeginScene(), "begin full stream");
        std::vector<Sexy::GpuVertex> large;
        large.reserve(65540);
        for (unsigned int i = 0; i < 16385; i++)
        {
            const auto q = Quad(r, 8, 8);
            large.insert(large.end(), q.begin(), q.end());
        }
        renderer.SubmitQuads(large, r, false, false, Sexy::GpuBlend::Opaque);
        renderer.EndSceneAndPresent();
        renderer.SetRenderImage(nullptr);
        values = renderer.ReadTextureFloats(target);
        Check(values[0] > .99f && values[2] < .001f, "16-bit stream boundary");
        Check(renderer.BeginScene(), "begin mixed main scene");
        auto black = Quad(renderer.whiteTexture, 64, 64);
        for (auto &v : black)
        {
            v.r = v.g = v.b = 0;
        }
        renderer.SubmitQuads(black, renderer.whiteTexture, false, false, Sexy::GpuBlend::Opaque);
        auto normal = Quad(renderer.whiteTexture, 64, 64, .5f);
        for (auto &v : normal)
        {
            v.g = v.b = 0;
        }
        auto additive = Quad(renderer.whiteTexture, 64, 64, .5f);
        for (auto &v : additive)
        {
            v.r = v.g = 0;
        }
        renderer.SubmitQuads(normal, renderer.whiteTexture, false, false, Sexy::GpuBlend::Normal);
        renderer.SubmitQuads(additive, renderer.whiteTexture, false, false, Sexy::GpuBlend::Additive);
        renderer.EndSceneAndPresent();
        const auto mixed = renderer.ReadScenePixels()[0];
        Check(std::abs(int((mixed >> 16) & 255) - 128) <= 1 && std::abs(int(mixed & 255) - 128) <= 1,
              "mixed normal/additive pipeline");
        context.WaitIdle();
        for (unsigned int i = 0; i < 2; i++)
        {
            Check(renderer.BeginScene(), "first two frame slots available");
            renderer.SubmitQuads(Quad(source, 64, 64), source, false, false, Sexy::GpuBlend::Opaque);
            renderer.EndSceneAndPresent();
        }
        Check(!renderer.BeginScene(), "third frame must not overwrite busy first frame");
        context.WaitIdle();
        Check(renderer.BeginScene(), "completed frame slot reusable");
        renderer.EndSceneAndPresent();
        context.WaitIdle();
        context.Check();
        EM_ASM(
            {
                Module.pvzGpuProbe = {};
                Module.pvzGpuProbe.ok = true;
                Module.pvzGpuProbe.checks = 9;
                Module.pvzGpuProbe.dualSource = !!$0;
                Module.pvzGpuProbe.framesInFlight = 2;
            },
            context.dualSource);
        return 0;
    }
    catch (const std::exception &e)
    {
        EM_ASM(
            {
                Module.pvzGpuProbe = {};
                Module.pvzGpuProbe.ok = false;
                Module.pvzGpuProbe.error = UTF8ToString($0);
            },
            e.what());
        return 1;
    }
}
