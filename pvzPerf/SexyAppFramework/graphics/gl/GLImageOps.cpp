#include "SexyAppFramework/graphics/PoolMesh.h"
#include "GLRendererInternal.h"
#include "graphics/FontWeights.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Sexy {
using namespace GL3;

void GLRenderer::Impl::ImagePass(GLTexture* target, GLTexture* source,
    GLTexture* alpha, int operation, const GpuImageProcessing* processing,
    unsigned int color, bool colorOnly)
{
    Flush();
    GLTexture sourceCopy, alphaCopy;
    if (source == target) {
        Snapshot(*source, sourceCopy);
        source = &sourceCopy;
    }
    if (alpha == target) {
        Snapshot(*alpha, alphaCopy);
        alpha = &alphaCopy;
    }
    auto* previous = selected;
    selected = target;
    try {
        BindTarget(); glUseProgram(imageProgram);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, source->image);
        glUniform1i(imageUniforms[uSourceImage], 0);
        glUniform4i(imageUniforms[uSourceRect],
            0, 0, source->width, source->height);
        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, (alpha ? alpha : source)->image);
        glUniform1i(imageUniforms[uAlphaImage], 1);
        const auto* mask = alpha ? alpha : source;
        glUniform4i(imageUniforms[uAlphaRect],
            0, 0, mask->width, mask->height);
        glUniform1ui(imageUniforms[uSourceSlot], source->flags);
        glUniform1i(imageUniforms[uOperation], operation);
        glUniform4i(imageUniforms[uRectangle],
            processing ? processing->x : 0, processing ? processing->y : 0,
            target->width, target->height);
        glUniform4i(imageUniforms[uTint],
            processing ? processing->red : 255, processing ? processing->green : 255,
            processing ? processing->blue : 255, processing ? processing->alpha : 255);
        glUniform1ui(imageUniforms[uPackedColor], color);
        if (operation==3) {
            const auto offsets=EvaluatePoolCaustics(causticTimeNs);
            glUniform1ui(imageUniforms[uCausticU0],offsets.u0);
            glUniform1ui(imageUniforms[uCausticU1],offsets.u1);
            glUniform1ui(imageUniforms[uCausticV1],offsets.v1);
        }
        glUniform1i(imageUniforms[uColorOnly], colorOnly);
        Quad(); Check("OpenGL image processing pass");
    } catch (...) {
        selected = previous;
        glActiveTexture(GL_TEXTURE0);
        throw;
    }
    selected = previous;
    glActiveTexture(GL_TEXTURE0);
}

GLTexture* GLRenderer::ProcessImage(GLTexture* source, const GpuImageProcessing& processing)
{
    auto& s = *impl_;
    if (!source || !s.textures.contains(source))
        throw std::invalid_argument("OpenGL image processing requires a live source");
    if (processing.operation != GpuImageOperation::Copy &&
        processing.operation != GpuImageOperation::Colorize)
        throw std::invalid_argument("Unknown OpenGL image operation");
    auto* target = s.Allocate(processing.width, processing.height, GL_RGBA8, true);
    try {
        s.ImagePass(target, source, nullptr,
            processing.operation == GpuImageOperation::Copy ? 0 : 1, &processing);
    }
    catch (...) { DestroyTexture(target); throw; }
    return target;
}
GLTexture* GLRenderer::CopyTextureRegion(GLTexture* source, int x, int y, int w, int h)
{
    auto& s = *impl_;
    if (!source || !s.textures.contains(source))
        throw std::invalid_argument("OpenGL region copy requires a live source");
    s.Flush();
    auto* target = s.Allocate(w, h, source->format, true);
    target->flags = source->flags;
    target->render = source->render;
    auto* previous = s.selected;
    unsigned int read = 0;
    try {
        s.selected = target; s.BindTarget();
        glDisable(GL_SCISSOR_TEST); glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
        glGenFramebuffers(1, &read); glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            source->image, 0);
        const int left = static_cast<int>(std::clamp<long long>(x,0,source->width));
        const int bottom = static_cast<int>(std::clamp<long long>(y,0,source->height));
        const int right = static_cast<int>(std::clamp<long long>(static_cast<long long>(x)+w,0,source->width));
        const int top = static_cast<int>(std::clamp<long long>(static_cast<long long>(y)+h,0,source->height));
        if (right > left && top > bottom)
            glBlitFramebuffer(left,bottom,right,top,
                left-x,bottom-y,right-x,top-y,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glDeleteFramebuffers(1,&read); read = 0;
        s.Check("Lossless OpenGL region copy");
    } catch (...) {
        if (read) glDeleteFramebuffers(1,&read);
        s.selected = previous; DestroyTexture(target); throw;
    }
    s.selected = previous;
    return target;
}
GLTexture* GLRenderer::ComposeAlphaRaw(GLTexture* base, GLTexture* alpha,
    unsigned int color, bool colorOnly)
{
    auto& s = *impl_;
    if (!base || !alpha || !s.textures.contains(base) || !s.textures.contains(alpha) ||
        alpha->width != base->width || alpha->height != base->height)
        throw std::invalid_argument("Invalid OpenGL alpha composition dimensions");
    auto* target = s.Allocate(base->width, base->height, GL_RGBA8, false);
    try { s.ImagePass(target, base, alpha, 2, nullptr, color, colorOnly); }
    catch (...) { DestroyTexture(target); throw; }
    return target;
}
GLTexture* GLRenderer::AdoptComposedTexture(GLTexture* texture)
{
    auto& s = *impl_;
    if (!texture || !s.textures.contains(texture) || texture->slot != ~0U)
        throw std::invalid_argument("Only a raw OpenGL composition can be adopted");
    s.Register(texture);
    // With resident rendering enabled, scratch can also be referenced by an
    // instance or a pending CPU batch: use the normal retirement path below.
    // GL retains references for commands that have already been issued.
    for (auto it = s.textures.begin(); it != s.textures.end();) {
        auto* candidate = *it;
        if (candidate != s.scene && candidate != s.selected && candidate->slot == ~0U) {
            delete candidate;
            it = s.textures.erase(it);
        } else ++it;
    }
    return texture;
}
GLTexture* GLRenderer::ComposeAlpha(GLTexture* base, GLTexture* alpha,
    unsigned int color, bool colorOnly)
{
    return AdoptComposedTexture(ComposeAlphaRaw(base, alpha, color, colorOnly));
}

namespace {
struct WeightTexture {
    unsigned int image = 0;
    int width = 0, height = 0;
    WeightTexture(const std::vector<FontAxisTap>& values, int maximum)
    {
        width = std::min(maximum, std::max(1, static_cast<int>(values.size())));
        height = std::max(1, static_cast<int>((values.size() + width - 1) / width));
        if (height > maximum) throw std::runtime_error("OpenGL font weight table exceeds device limit");
        std::vector<FontAxisTap> padded(size_t(width) * height);
        std::copy(values.begin(), values.end(), padded.begin());
        glGenTextures(1, &image); glBindTexture(GL_TEXTURE_2D, image);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, width, height, 0,
            GL_RG_INTEGER, GL_UNSIGNED_INT, padded.data());
    }
    ~WeightTexture() { if (image) glDeleteTextures(1, &image); }
    WeightTexture(const WeightTexture&) = delete;
    WeightTexture& operator=(const WeightTexture&) = delete;
};
std::vector<FontAxisTap> Ranges(const FontAxisWeights& axis)
{
    std::vector<FontAxisTap> ranges;
    ranges.reserve(axis.offsets.size() - 1);
    for (size_t i = 1; i < axis.offsets.size(); ++i)
        ranges.push_back({axis.offsets[i - 1], axis.offsets[i] - axis.offsets[i - 1]});
    return ranges;
}
}
void GLRenderer::RenderFontAtlas(GLTexture* target, GLTexture* source,
    std::span<const GpuFontGlyph> glyphs, unsigned int samplingFlags, bool forceWhite)
{
    auto& s = *impl_;
    if (!target || !target->render || !source || !s.textures.contains(source) ||
        !s.textures.contains(target))
        throw std::invalid_argument("OpenGL font generation requires a live float render image");
    s.Flush();
    GLTexture sourceCopy;
    if (target == source) {
        s.Snapshot(*source, sourceCopy);
        source = &sourceCopy;
    }
    auto* previous = s.selected;
    s.selected = target;
    try {
        s.BindTarget(); glUseProgram(s.fontProgram);
        glUniform1i(s.fontUniforms[Impl::uSourceImage], 0);
        glUniform4i(s.fontUniforms[Impl::uSourceRect],
            0, 0, source->width, source->height);
        glUniform1ui(s.fontUniforms[Impl::uSourceSlot], source->flags);
        glUniform1ui(s.fontUniforms[Impl::uSamplingFlags], samplingFlags);
        glUniform1i(s.fontUniforms[Impl::uForceWhite], forceWhite);
        for (const auto& glyph : glyphs) {
            if (glyph.sourceWidth <= 0 || glyph.sourceHeight <= 0 ||
                glyph.sourceX < 0 || glyph.sourceY < 0 ||
                static_cast<long long>(glyph.sourceX) + glyph.sourceWidth > source->width ||
                static_cast<long long>(glyph.sourceY) + glyph.sourceHeight > source->height ||
                glyph.targetX < 0 || glyph.targetY < 0 || glyph.targetWidth <= 0 || glyph.targetHeight <= 0 ||
                static_cast<long long>(glyph.targetX) + glyph.targetWidth > target->width ||
                static_cast<long long>(glyph.targetY) + glyph.targetHeight > target->height)
                throw std::invalid_argument("OpenGL glyph rectangle exceeds its image");
            const auto x = BuildFontAxisWeights(glyph.sourceWidth, glyph.targetWidth,
                glyph.sourceX, source->width, 257);
            const auto y = BuildFontAxisWeights(glyph.sourceHeight, glyph.targetHeight,
                glyph.sourceY, source->height, 256);
            glActiveTexture(GL_TEXTURE0 + 1); WeightTexture xr(Ranges(x), s.maximum);
            glActiveTexture(GL_TEXTURE0 + 2); WeightTexture yr(Ranges(y), s.maximum);
            glActiveTexture(GL_TEXTURE0 + 3); WeightTexture xt(x.taps, s.maximum);
            glActiveTexture(GL_TEXTURE0 + 4); WeightTexture yt(y.taps, s.maximum);
            glUniform1i(s.fontUniforms[Impl::uXRanges], 1);
            glUniform1i(s.fontUniforms[Impl::uYRanges], 2);
            glUniform1i(s.fontUniforms[Impl::uXTaps], 3);
            glUniform1i(s.fontUniforms[Impl::uYTaps], 4);
            glUniform2i(s.fontUniforms[Impl::uXTapExtent], xt.width, xt.height);
            glUniform2i(s.fontUniforms[Impl::uYTapExtent], yt.width, yt.height);
            glUniform4i(s.fontUniforms[Impl::uGlyphSrc],
                glyph.sourceX, glyph.sourceY, glyph.sourceWidth, glyph.sourceHeight);
            glUniform4i(s.fontUniforms[Impl::uGlyphDst],
                glyph.targetX, glyph.targetY, glyph.targetWidth, glyph.targetHeight);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, source->image);
            glViewport(glyph.targetX, glyph.targetY, glyph.targetWidth, glyph.targetHeight);
            s.Quad(); s.Check("Generate OpenGL font glyph");
        }
    } catch (...) {
        s.selected = previous; glActiveTexture(GL_TEXTURE0); throw;
    }
    s.selected = previous; glActiveTexture(GL_TEXTURE0);
}
GLTexture* GLRenderer::CreateCausticsTexture(int w, int h, GLTexture* source)
{
    if (w != 128 || h != 64)
        throw std::invalid_argument("OpenGL caustics output must be 128 by 64");
    auto* target = impl_->Allocate(w, h, GL_RGBA8, true);
    target->caustic = true;
    auto* previous=impl_->selected;
    try {
        impl_->Flush();impl_->selected=target;impl_->BindTarget();
        glDisable(GL_SCISSOR_TEST);glClearColor(1,1,1,0);glClear(GL_COLOR_BUFFER_BIT);
        impl_->selected=previous;
        UseCausticsTexture(target, source);
    }
    catch (...) { impl_->selected=previous;DestroyTexture(target); throw; }
    return target;
}
void GLRenderer::UseCausticsTexture(GLTexture* output, GLTexture* source)
{
    auto& s = *impl_;
    if (s.open || !output || !source || !s.textures.contains(output) || !s.textures.contains(source) ||
        !output->caustic || output->width != 128 || output->height != 64 ||
        output->format != GL_RGBA8 ||
        source->width != 256 || source->height != 256)
        throw std::invalid_argument("Invalid OpenGL caustics source or output");
    s.causticTarget = output; s.causticSource = source;
    s.causticsQueued=true;
}
void GLRenderer::SetCausticsTimeNs(unsigned long long timeNs)
{
    impl_->causticTimeNs=timeNs;
    impl_->causticsQueued=impl_->causticTarget!=nullptr;
}
bool GLRenderer::HasCausticsCompute() const { return impl_->imageProgram != 0; }

std::vector<float> GLRenderer::ReadTextureFloats(GLTexture* texture)
{
    auto& s = *impl_;
    if (s.open) throw std::logic_error("OpenGL diagnostic readback requires a closed scene");
    if (!texture || !s.textures.contains(texture))
        throw std::invalid_argument("Cannot read a destroyed OpenGL image");
    std::vector<float> output(size_t(texture->width) * texture->height * 4);
    unsigned int framebuffer = 0;
    glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        texture->image, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &framebuffer);
        throw std::runtime_error("OpenGL diagnostic framebuffer is incomplete");
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    GLPlatform::ReadTextureFloats(texture->width, texture->height, texture->format, output.data());
    glDeleteFramebuffers(1, &framebuffer);
    s.Check("Read OpenGL float image");
    return output;
}
ImagePixels GLRenderer::ReadTexturePixels(GLTexture* texture)
{
    const auto values = ReadTextureFloats(texture);
    ImagePixels output(texture->width, texture->height);
    for (size_t i = 0; i < output.pixels.size(); ++i) {
        const float alpha = texture->flags & GpuVertex::kOpaqueInput ? 1 : values[i * 4 + 3];
        const auto byte = [](float value) {
            return static_cast<unsigned int>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        };
        const float factor = texture->render ? (alpha > 0 ? 1 / alpha : 0) : 1;
        output.pixels[i] = (byte(alpha) << 24) | (byte(values[i * 4] * factor) << 16) |
            (byte(values[i * 4 + 1] * factor) << 8) | byte(values[i * 4 + 2] * factor);
    }
    return output;
}
std::vector<unsigned int> GLRenderer::ReadScenePixels()
{
    if (!impl_->sceneInitialized)
        throw std::logic_error("OpenGL scene readback requires a completed main scene");
    return ReadTexturePixels(impl_->scene).pixels;
}
std::vector<unsigned int> GLRenderer::ReadScalePixels(bool)
{
    throw std::logic_error("OpenGL 3.0 has no configured custom scaling output");
}
}
