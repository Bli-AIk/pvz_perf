#include "GLRendererInternal.h"
#include "GLShaders.h"
#include "graphics/vulkan/VulkanViewport.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace Sexy {
using namespace GL3;

GLTexture::~GLTexture()
{
    if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
    if (image) glDeleteTextures(1, &image);
}
void GLRenderer::Impl::Dimensions(int w, int h) const
{
    if (w <= 0 || h <= 0 || w > maximum || h > maximum)
        throw std::invalid_argument("OpenGL texture dimensions exceed GL_MAX_TEXTURE_SIZE");
}
void GLRenderer::Impl::AllocateStorage(GLTexture& texture, int w, int h, unsigned int format)
{
    Dimensions(w, h);
    auto* result = &texture;
    result->width = w; result->height = h; result->format = format;
    glGenTextures(1, &result->image);
    glBindTexture(GL_TEXTURE_2D, result->image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0,
        format == GL_R8 ? GL_RED : GL_RGBA,
        (format == GL_RGBA16F || format == GL_RGBA32F) ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    GL3::CheckError("Allocate OpenGL image");
}
void GLRenderer::Impl::Register(GLTexture* texture)
{
    for (unsigned int slot = 1; slot < staticBase; ++slot)
        if (!slots[slot]) {
            texture->slot = slot; slots[slot] = texture;
            return;
        }
    throw std::runtime_error("OpenGL logical texture slots exhausted");
}
GLTexture* GLRenderer::Impl::Allocate(int w, int h, unsigned int format,
    bool registerSlot)
{
    Dimensions(w, h);
    auto* result = new GLTexture;
    try {
        AllocateStorage(*result, w, h, format);
        textures.insert(result);
        if (registerSlot) Register(result);
    } catch (...) {
        textures.erase(result);
        delete result;
        throw;
    }
    return result;
}
void GLRenderer::Impl::Upload(GLTexture* texture, const void* data, int pitch,
    unsigned int format)
{
    const int bytes = format == GL_RED ? 1 : 4;
    if (!data || pitch < static_cast<long long>(texture->width) * bytes)
        throw std::invalid_argument("Invalid OpenGL texture upload");
    std::vector<unsigned char> packed;
    if (pitch % bytes) {
        packed.resize(size_t(texture->width) * texture->height * bytes);
        for (int row = 0; row < texture->height; ++row)
            std::memcpy(packed.data() + size_t(row) * texture->width * bytes,
                static_cast<const unsigned char*>(data) + size_t(row) * pitch,
                size_t(texture->width) * bytes);
        data = packed.data(); pitch = texture->width * bytes;
    }
    glBindTexture(GL_TEXTURE_2D, texture->image);
    GLPlatform::UploadPixels(texture->width, texture->height, data, pitch, format);
    GL3::CheckError("Upload OpenGL image");
}
unsigned int GLRenderer::Impl::Program(std::string_view vs, std::string_view fs, bool mixed)
{
    const auto shader = [&](unsigned int type, std::string_view text) {
        std::string storage;
        GLPlatform::PrepareShaderSource(storage, text);
        const unsigned int object = glCreateShader(type);
        const char* source = text.data(); const int length = static_cast<int>(text.size());
        glShaderSource(object, 1, &source, &length);
        glCompileShader(object);
        int success = 0; glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (!success) {
            std::string log(8192, '\0'); int used = 0;
            glGetShaderInfoLog(object, static_cast<int>(log.size()), &used, log.data());
            glDeleteShader(object); log.resize(used);
            throw std::runtime_error("GLSL 1.30 compile failed: " + log);
        }
        return object;
    };
    const auto vertex = shader(GL_VERTEX_SHADER, vs);
    unsigned int fragment = 0, program = 0;
    try {
        fragment = shader(GL_FRAGMENT_SHADER, fs);
        program = glCreateProgram();
        glAttachShader(program, vertex); glAttachShader(program, fragment);
        static constexpr const char* attributes[]{"inPosition", "inColor", "inUV", "inSlot", "inSampler"};
        for (unsigned int i = 0; i < std::size(attributes); ++i)
            glBindAttribLocation(program, i, attributes[i]);
        GLPlatform::BindFragmentOutputs(program, mixed);
        glLinkProgram(program);
        int success = 0; glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            std::string log(8192, '\0'); int used = 0;
            glGetProgramInfoLog(program, static_cast<int>(log.size()), &used, log.data());
            log.resize(used); throw std::runtime_error("GLSL 1.30 link failed: " + log);
        }
    } catch (...) {
        if (program) glDeleteProgram(program);
        if (fragment) glDeleteShader(fragment);
        glDeleteShader(vertex); throw;
    }
    glDeleteShader(vertex); glDeleteShader(fragment);
    return program;
}
GLRenderer::Impl::Impl(GLContext& window, int w, int h)
    : context(window), width(w), height(h), frameSync(GLPlatform::SupportsFrameSync())
{
    context.MakeCurrent();
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_DITHER);
    GLPlatform::InitializeColorState();
    for (auto& frame : frames) {
        glGenVertexArrays(1, &frame.vao); glBindVertexArray(frame.vao);
        glGenBuffers(1, &frame.stream); glBindBuffer(GL_ARRAY_BUFFER, frame.stream);
        for(unsigned int attribute=0;attribute<5;++attribute)glEnableVertexAttribArray(attribute);
        VertexBase(0);
    }
    vao = frames[0].vao; stream = frames[0].stream;
    glBindVertexArray(vao);
    // One reusable 16-bit index pattern covers 16384 complete strip-order quads.
    // Larger streams split at complete quad boundaries; no 32-bit index requirement.
    std::vector<unsigned short> indices(65536/4*6);
    for(unsigned int quad=0;quad<65536/4;++quad) {
        const unsigned int base=quad*4;
        const size_t at=size_t(quad)*6;
        indices[at]=static_cast<unsigned short>(base);
        indices[at+1]=static_cast<unsigned short>(base+1);
        indices[at+2]=static_cast<unsigned short>(base+2);
        indices[at+3]=static_cast<unsigned short>(base+2);
        indices[at+4]=static_cast<unsigned short>(base+1);
        indices[at+5]=static_cast<unsigned short>(base+3);
    }
    glGenBuffers(1,&quadIndices);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,quadIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>(indices.size()*sizeof(unsigned short)),
        indices.data(),GL_STATIC_DRAW);
    vertices.reserve(16384); batches.reserve(1024);
}
GLRenderer::Impl::~Impl()
{
    context.MakeCurrent();
    batches.clear();
    for (auto* texture : textures) delete texture;
    for (auto* texture : retired) delete texture;
    for (const auto program : {sceneProgram, spriteProgram, mixedSpriteProgram, imageProgram, fontProgram, presentProgram})
        if (program) glDeleteProgram(program);
    if (poolProgram) glDeleteProgram(poolProgram);
    if (poolVao) glDeleteVertexArrays(1, &poolVao);
    if (poolVertices) glDeleteBuffers(1, &poolVertices);
    if (poolIndices) glDeleteBuffers(1, &poolIndices);
    if (quadIndices) glDeleteBuffers(1,&quadIndices);
    for (auto& frame : frames) {
        if (frame.fence) glDeleteSync(frame.fence);
        if (frame.stream) glDeleteBuffers(1, &frame.stream);
        if (frame.vao) glDeleteVertexArrays(1, &frame.vao);
    }
}
void GLRenderer::Impl::VertexBase(size_t first)
{
    const size_t base=first*sizeof(Vertex);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(base));
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),
        reinterpret_cast<const void*>(base+offsetof(Vertex,r)));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),
        reinterpret_cast<const void*>(base+offsetof(Vertex,u)));
    glVertexAttribIPointer(3,1,GL_UNSIGNED_INT,sizeof(Vertex),
        reinterpret_cast<const void*>(base+offsetof(Vertex,slot)));
    glVertexAttribIPointer(4,1,GL_UNSIGNED_INT,sizeof(Vertex),
        reinterpret_cast<const void*>(base+offsetof(Vertex,sampler)));
}
void GLRenderer::Impl::BindTarget()
{
    auto* target = selected ? selected : scene;
    auto& storage = *target;
    if (!storage.framebuffer) {
        glGenFramebuffers(1, &storage.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, storage.framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, storage.image, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("OpenGL required image format is not framebuffer complete");
    } else glBindFramebuffer(GL_FRAMEBUFFER, storage.framebuffer);
    glViewport(0, 0, target->width, target->height);
}
void GLRenderer::Impl::Flush()
{
    if (vertices.empty()) {
        for (auto* texture : retired) delete texture;
        retired.clear();
        return;
    }
    BindTarget();
    const auto* target = selected ? selected : scene;
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, stream);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(), GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    int sceneCoverage=-1;
    unsigned int currentProgram=0;
    unsigned int configuredPrograms=0;
    size_t vertexBase=~size_t(0);
    // These caches start unknown on every Flush: other passes may have changed
    // all GL state. The draw loop keeps its target and batch storage fixed.
    const Batch* previousBatch=nullptr;
    glEnable(GL_SCISSOR_TEST);
    for (const auto& batch : batches) {
        const unsigned int program=batch.mixed?mixedSpriteProgram:batch.sprite?spriteProgram:sceneProgram;
        if(currentProgram!=program) {
            glUseProgram(program);
            const unsigned int programBit=batch.mixed?4U:batch.sprite?1U:2U;
            if(!(configuredPrograms&programBit)) {
                const auto& uniforms=batch.mixed?mixedSpriteUniforms:batch.sprite?spriteUniforms:sceneUniforms;
                glUniform2f(uniforms[uTargetExtent],float(target->width),float(target->height));
                glUniform1i(uniforms[uImage],0);
                if(!batch.sprite)glUniform1i(uniforms[uPremultipliedOutput],target->render);
                configuredPrograms|=programBit;
            }
            currentProgram=program;
        }
        if(!previousBatch||batch.texture!=previousBatch->texture)
            glBindTexture(GL_TEXTURE_2D,batch.texture->image);
        if(batch.sprite && batch.texture->sampler!=batch.sampler) {
            const int filter=(batch.sampler&1U)?GL_LINEAR:GL_NEAREST;
            const int wrap=(batch.sampler&2U)?GL_REPEAT:GL_CLAMP_TO_EDGE;
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
            batch.texture->sampler=batch.sampler;
        }
        if(!previousBatch||batch.clip.x!=previousBatch->clip.x||batch.clip.y!=previousBatch->clip.y||
            batch.clip.width!=previousBatch->clip.width||batch.clip.height!=previousBatch->clip.height)
            glScissor(batch.clip.x,batch.clip.y,batch.clip.width,batch.clip.height);
        if(!previousBatch||batch.blend!=previousBatch->blend||batch.mixed!=previousBatch->mixed){
            if (batch.blend == GpuBlend::Opaque) glDisable(GL_BLEND);
            else {
                glEnable(GL_BLEND);
                glBlendFuncSeparate((target->render || batch.mixed) ? GL_ONE : GL_SRC_ALPHA,
                    batch.mixed ? GL_ONE_MINUS_SRC1_ALPHA :
                        batch.blend == GpuBlend::Additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA,
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            }
        }
        if(!batch.sprite && sceneCoverage!=int(batch.coverage)){
            glUniform1i(sceneUniforms[uCoverageInput],batch.coverage);
            sceneCoverage=int(batch.coverage);
        }
        if(batch.quads) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,quadIndices);
            for(size_t offset=0;offset<batch.count;) {
                const size_t first=batch.first+offset;
                // Reuse the static EBO's local origin across adjacent batches.
                // A preceding triangle batch can make the next quad unaligned;
                // only then, or at the 16-bit limit, rebase the five attributes.
                if(vertexBase==~size_t(0) || first<vertexBase ||
                    first-vertexBase>=65536 || (first-vertexBase)%4!=0) {
                    VertexBase(first);vertexBase=first;
                }
                const size_t relative=first-vertexBase;
                const size_t count=std::min<size_t>(65536-relative,batch.count-offset);
                const size_t indexOffset=relative/4*6*sizeof(unsigned short);
                glDrawElements(GL_TRIANGLES,static_cast<int>(count/4*6),GL_UNSIGNED_SHORT,
                    reinterpret_cast<const void*>(indexOffset));
                offset+=count;
            }
        } else {
            if(vertexBase==~size_t(0) || batch.first<vertexBase ||
                batch.first-vertexBase>size_t(std::numeric_limits<int>::max())) {
                VertexBase(batch.first);vertexBase=batch.first;
            }
            glDrawArrays(GL_TRIANGLES,static_cast<int>(batch.first-vertexBase),static_cast<int>(batch.count));
        }
        previousBatch=&batch;
    }
    vertices.clear(); batches.clear();
    for (auto* texture : retired) delete texture;
    retired.clear();
    Check("Draw OpenGL triangles");
}
void GLRenderer::Impl::Snapshot(const GLTexture& original, GLTexture& result)
{
    Flush();
    const auto* texture = &original;
    auto* copy = &result;
    AllocateStorage(result, texture->width, texture->height, texture->format);
    result.flags = texture->flags;
    result.render = texture->render;
    unsigned int source = 0, destination = 0;
    glGenFramebuffers(1, &source); glGenFramebuffers(1, &destination);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        texture->image, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, copy->image, 0);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0, 0, texture->width, texture->height,
        0, 0, texture->width, texture->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glDeleteFramebuffers(1, &source); glDeleteFramebuffers(1, &destination);
    Check("Snapshot OpenGL feedback source");
}
void GLRenderer::Impl::Submit(std::span<const GpuVertex> input, GLTexture* source,
    GpuBlend blend, const GpuClip* requested, bool coverage, bool quads)
{
    (void)source; // The vertex's logical slot is authoritative, as in bindless draws.
    const size_t primitiveSize=quads?4:3;
    if (!open || input.size() % primitiveSize)
        throw std::logic_error("OpenGL submission requires an open scene and complete primitive list");
    auto* target = selected ? selected : scene;
    const GpuClip bounds{0, 0, target->width, target->height};
    GpuClip clip = requested ? *requested : bounds;
    const int right = std::min(target->width, clip.x + std::max(0, clip.width));
    const int top = std::min(target->height, clip.y + std::max(0, clip.height));
    clip.x = std::max(0, clip.x); clip.y = std::max(0, clip.y);
    clip.width = std::max(0, right - clip.x); clip.height = std::max(0, top - clip.y);
    if (!clip.width || !clip.height) return;
    GLTexture* feedback = nullptr;
    for (size_t index = 0; index < input.size();) {
        auto* image = slots[input[index].slot & 4095U];
        if (!image) throw std::logic_error("OpenGL draw references an unbound logical texture");
        auto* native = image;
        if (image == target) {
            if (!feedback) {
                auto* copy = new GLTexture;
                try {
                    Snapshot(*image, *copy);
                    retired.push_back(copy);
                } catch (...) { delete copy; throw; }
                feedback = copy;
            }
            native = feedback;
        }
        constexpr unsigned int complexSampling = 0x80000000U |
            GpuVertex::kIndependentColorFilter | GpuVertex::kColorFilterMask |
            GpuVertex::kAlphaEdgeFilter;
        const bool sprite=!coverage && !target->render && !image->render &&
            (input[index].slot&complexSampling)==0;
        const bool mixed=GLPlatform::MixedSprites && sprite && blend!=GpuBlend::Opaque;
        const GpuBlend batchBlend=mixed?GpuBlend::Normal:blend;
        const unsigned int sampler=sprite?input[index].sampler:0;
        // Resolve texture/sampling and batch identity once for an adjacent run,
        // rather than repeating all bookkeeping for both triangles of every quad.
        // Only provoking metadata controls a triangle; UV and color stay per vertex.
        const auto sourceSlot=input[index].slot;
        const auto slot=sourceSlot | (mixed && blend==GpuBlend::Additive?0x02000000U:0U);
        const auto triangleSampler=coverage?0:input[index].sampler;
        size_t end=index+primitiveSize;
        while(end<input.size() && input[end].slot==sourceSlot &&
              (coverage || input[end].sampler==triangleSampler)) end+=primitiveSize;
        const size_t count=end-index;
        const size_t first=vertices.size();
        vertices.insert(vertices.end(),input.begin()+index,input.begin()+end);
        auto* output=vertices.data()+first;
        // Flat outputs are taken from the last vertex on GL 3.0. The first two
        // vertices' metadata is unobserved; preserve their position/color/UV.
        for(size_t offset=0;offset<count;offset+=primitiveSize) {
            output[offset+2].slot=slot;
            output[offset+2].sampler=triangleSampler;
            if(quads) {
                output[offset+3].slot=slot;
                output[offset+3].sampler=triangleSampler;
            }
        }
        const auto sameClip = [](const GpuClip& a, const GpuClip& b) {
            return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
        };
        if (!batches.empty() && batches.back().texture == native &&
            batches.back().blend == batchBlend && batches.back().coverage == coverage &&
            batches.back().sprite == sprite && batches.back().mixed == mixed &&
            batches.back().quads == quads && batches.back().sampler == sampler &&
            sameClip(batches.back().clip, clip))
            batches.back().count += count;
        else batches.push_back({native, first, count, batchBlend, clip, coverage, sprite, mixed, quads, sampler});
        index=end;
    }
}
GLRenderer::GLRenderer(GLContext& context, int w, int h,
    const std::filesystem::path&, const VulkanHdrParams* hdr)
    : impl_(std::make_unique<Impl>(context, w, h)), whiteTexture(impl_->white),
      causticsTexture(impl_->causticTarget), sceneOpen(impl_->open),
      sceneWidth(impl_->width), sceneHeight(impl_->height)
{
    if (context.hdr) throw std::runtime_error("OpenGL 3.0 backend supports SDR output only");
    if (hdr) impl_->hdr = *hdr;
    PrepareLoadingPipelines();
    const unsigned int white = 0xffffffffU;
    impl_->white = CreateTexture(1, 1, &white, 4);
    impl_->slots[impl_->white->slot] = nullptr;
    impl_->white->slot = 0; impl_->slots[0] = impl_->white;
    impl_->scene = impl_->Allocate(w, h, GL_RGBA8, false);
    impl_->BindTarget(); glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
}
GLRenderer::~GLRenderer() = default;
void GLRenderer::PrepareLoadingPipelines()
{
    auto& s = *impl_;
    if (s.initialized) return;
    s.sceneProgram = s.Program(SceneVertexShader(), SceneFragmentShader());
    s.spriteProgram = s.Program(SceneVertexShader(), SpriteFragmentShader());
    s.mixedSpriteProgram=s.Program(SceneVertexShader(),GLPlatform::MixedSpriteSource(),GLPlatform::MixedSprites);
    s.imageProgram = s.Program(QuadVertexShader(), ImageFragmentShader());
    s.fontProgram = s.Program(QuadVertexShader(), FontFragmentShader());
    s.presentProgram = s.Program(QuadVertexShader(), PresentFragmentShader());
    static constexpr std::array names{"targetExtent","image","premultipliedOutput","coverageInput",
        "sourceImage","alphaImage","sourceRect","alphaRect","sourceSlot",
        "operation","rectangle","tint","packedColor","causticU0","causticU1","causticV1","colorOnly",
        "samplingFlags","forceWhite","xRanges","yRanges","xTaps","yTaps",
        "xTapExtent","yTapExtent","glyphSrc","glyphDst"};
    static_assert(names.size()==Impl::uUniformCount);
    const auto cache=[&](unsigned int program,Impl::UniformLocations& locations) {
        for(size_t i=0;i<names.size();++i)
            locations[i]=glGetUniformLocation(program,names[i]);
    };
    cache(s.sceneProgram,s.sceneUniforms);cache(s.spriteProgram,s.spriteUniforms);
    cache(s.mixedSpriteProgram,s.mixedSpriteUniforms);
    cache(s.imageProgram,s.imageUniforms);
    cache(s.fontProgram,s.fontUniforms);cache(s.presentProgram,s.presentUniforms);
    s.poolProgram = s.Program(PoolVertexShader(), SceneFragmentShader());
    static constexpr std::array poolNames{"targetExtent","image","premultipliedOutput","coverageInput",
        "poolSize","poolTranslation","waveTime","poolClip","poolLayer","poolNight","poolSlot","poolSampler"};
    for (size_t i=0;i<poolNames.size();++i)
        s.poolUniforms[i]=glGetUniformLocation(s.poolProgram,poolNames[i]);
    glGenVertexArrays(1,&s.poolVao); glBindVertexArray(s.poolVao);
    glGenBuffers(1,&s.poolVertices); glBindBuffer(GL_ARRAY_BUFFER,s.poolVertices);
    glBufferData(GL_ARRAY_BUFFER,sizeof(GpuPoolVertices),GpuPoolVertices.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(GpuPoolVertex),nullptr);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(GpuPoolVertex),
        reinterpret_cast<const void*>(offsetof(GpuPoolVertex,phaseX)));
    glGenBuffers(1,&s.poolIndices); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,s.poolIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(GpuPoolIndices),GpuPoolIndices.data(),GL_STATIC_DRAW);
    glBindVertexArray(s.vao);
    s.initialized = true;
}
GLTexture* GLRenderer::CreateTexture(int w, int h, const void* pixels, int pitch, bool opaque)
{
    auto* texture = impl_->Allocate(w, h, GL_RGBA8, true);
    try { impl_->Upload(texture, pixels, pitch, GL_BGRA); }
    catch (...) { DestroyTexture(texture); throw; }
    texture->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    return texture;
}
GLTexture* GLRenderer::CreateRawTexture(int w, int h, const void* pixels, int pitch)
{
    auto* texture = impl_->Allocate(w, h, GL_RGBA8, false);
    try { impl_->Upload(texture, pixels, pitch, GL_BGRA); }
    catch (...) { DestroyTexture(texture); throw; }
    return texture;
}
void GLRenderer::ReserveStaticTextureSlots(unsigned int count)
{
    auto& s = *impl_;
    if (count >= 4095) throw std::invalid_argument("OpenGL static slots exceed logical table");
    const unsigned int base = 4096 - count - 1;
    if (s.staticBase != 4096 && s.staticBase != base)
        throw std::logic_error("OpenGL static table already reserved");
    for (unsigned int i = base; i < s.slots.size(); ++i)
        if (s.slots[i]) throw std::logic_error("OpenGL static/dynamic slot overlap");
    s.staticBase = base;
}
void GLRenderer::BindStaticTexture(GLTexture* texture, unsigned int slot)
{
    auto& s = *impl_;
    if (!texture || !s.textures.contains(texture) || !slot ||
        static_cast<unsigned long long>(s.staticBase) + slot >= 4096)
        throw std::invalid_argument("Invalid OpenGL static symbol");
    if (texture->staticSlot != ~0U && texture->staticSlot != s.staticBase + slot)
        throw std::invalid_argument("OpenGL texture already has a different static symbol");
    if (texture->slot != ~0U && s.slots[texture->slot] == texture)
        s.slots[texture->slot] = nullptr;
    texture->slot = texture->staticSlot = s.staticBase + slot;
    s.slots[texture->slot] = texture;
}
GLTexture* GLRenderer::CreateTextureStatic(int w, int h, const void* pixels,
    int pitch, bool opaque, unsigned int slot)
{
    auto* texture = impl_->Allocate(w, h, GL_RGBA8, false);
    texture->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    try { impl_->Upload(texture, pixels, pitch, GL_BGRA); BindStaticTexture(texture, slot); }
    catch (...) { DestroyTexture(texture); throw; }
    return texture;
}
GLTexture* GLRenderer::ReplaceTexture(GLTexture* old, int w, int h,
    const void* pixels, int pitch, bool opaque)
{
    if (!old) return CreateTexture(w,h,pixels,pitch,opaque);
    if (!impl_->textures.contains(old) || old == impl_->white || old->render || old->caustic)
        throw std::invalid_argument("Cannot host-replace this OpenGL image");
    // Recorded batches keep the old native name until Flush completes.
    auto* next = impl_->Allocate(w, h, GL_RGBA8, false);
    next->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    try {
        impl_->Upload(next, pixels, pitch, GL_BGRA);
        if (old->staticSlot != ~0U)
            BindStaticTexture(next,old->staticSlot-impl_->staticBase);
        else {
            next->slot=old->slot;
            impl_->slots[next->slot]=next;
        }
    }
    catch (...) { DestroyTexture(next); throw; }
    DestroyTexture(old);
    return next;
}
GLTexture* GLRenderer::CreateCoverageMask(int w, int h,
    std::span<const unsigned char> coverage, int pitch)
{
    if (pitch < w || coverage.size() < size_t(pitch) * (h - 1) + w)
        throw std::invalid_argument("Incomplete OpenGL coverage mask");
    auto* image = impl_->Allocate(w, h, GL_R8, true);
    try { impl_->Upload(image, coverage.data(), pitch, GL_RED); }
    catch (...) { DestroyTexture(image); throw; }
    return image;
}
GLTexture* GLRenderer::CreateRenderImage(int w, int h)
{
    auto& s = *impl_; s.Flush();
    auto* image = s.Allocate(w, h, GL_RGBA16F, true);
    image->render = true; image->flags = 0x80000000U;
    auto* previous = s.selected; s.selected = image;
    try {
        s.BindTarget(); glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
        s.Check("Clear RGBA16F OpenGL render image");
    } catch (...) { s.selected = previous; DestroyTexture(image); throw; }
    s.selected = previous;
    return image;
}
void GLRenderer::DestroyTexture(GLTexture* image)
{
    if (!image) return;
    auto& s = *impl_;
    if (!s.textures.contains(image) || image == s.white)
        throw std::invalid_argument("Cannot destroy the OpenGL white image or a foreign handle");
    if (image == s.selected) throw std::logic_error("Deselect OpenGL render image before destruction");
    if (!s.batches.empty()) s.retired.push_back(image);
    if (image == s.causticTarget) s.causticTarget = nullptr;
    if (image == s.causticSource) s.causticSource = nullptr;
    if (image->slot != ~0U && s.slots[image->slot] == image) s.slots[image->slot] = nullptr;
    s.textures.erase(image);
    if (s.batches.empty()) delete image;
}
unsigned int GLRenderer::SlotOf(const GLTexture* texture)
{
    if (!texture || texture->slot == ~0U)
        throw std::logic_error("OpenGL raw scratch image is not a drawable logical texture");
    return texture->slot | texture->flags;
}
void GLRenderer::SetRenderImage(GLTexture* image)
{
    auto& s = *impl_;
    if (image && (!s.textures.contains(image) || !image->render))
        throw std::invalid_argument("OpenGL target is not a live generated float image");
    if(image==s.selected)return;
    s.Flush(); s.selected = image;
    if (s.open && !s.mainFrame && !image) { glFinish(); s.open = false; }
}
bool GLRenderer::BeginScene()
{
    auto& s = *impl_;
    if (s.open) throw std::logic_error("OpenGL scene already open");
    if (!s.selected && !s.context.BeginPresent()) return false;
    if (!s.selected) {
        auto& frame = s.frames[s.frameIndex];
        if (frame.fence) {
            const auto status = glClientWaitSync(frame.fence, 0, 0);
            if (status == GL_TIMEOUT_EXPIRED) return false;
            if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
                throw std::runtime_error("OpenGL frame fence query failed");
            glDeleteSync(frame.fence); frame.fence = nullptr;
        }
        s.vao = frame.vao; s.stream = frame.stream;
    }
    // GL command ordering protects shared textures. VBO data stores are
    // replaced by BufferData at each flush; image-only jobs need no CPU wait
    // for an earlier main frame and never overwrite its submitted storage.
    s.open = true; s.mainFrame = !s.selected;
    if (s.causticsQueued && s.causticTarget && s.causticSource) {
        s.ImagePass(s.causticTarget, s.causticSource, nullptr, 3);
        s.causticsQueued = false;
    }
    s.BindTarget();
    return true;
}
void GLRenderer::SubmitTriangles(std::span<const GpuVertex> vertices,
    GLTexture* texture, bool, bool, GpuBlend blend, const GpuClip* clip)
{
    impl_->Submit(vertices, texture, blend, clip, false);
}
void GLRenderer::SubmitQuads(std::span<const GpuVertex> vertices,
    GLTexture* texture, bool, bool, GpuBlend blend, const GpuClip* clip)
{
    impl_->Submit(vertices,texture,blend,clip,false,true);
}
void GLRenderer::SubmitPool(GLTexture* texture,const GpuPoolParams& params)
{
    auto& s=*impl_;
    if (!s.open || !texture || !s.textures.contains(texture))
        throw std::logic_error("OpenGL pool requires an open scene and live source texture");
    if (params.layer>2 || params.sampler>3 ||
        static_cast<unsigned>(params.blend)>static_cast<unsigned>(GpuBlend::Opaque))
        throw std::invalid_argument("Invalid OpenGL pool layer/sampler/blend");
    s.Flush();
    auto* target=s.selected ? s.selected : s.scene;
    GLTexture feedback;
    if (texture==target) {
        s.Snapshot(*texture,feedback);
    }
    s.BindTarget();
    glBindVertexArray(s.poolVao); glUseProgram(s.poolProgram);
    const auto& u=s.poolUniforms;
    glUniform2f(u[0],float(target->width),float(target->height));
    glUniform1i(u[1],0); glUniform1i(u[2],target->render); glUniform1i(u[3],0);
    glUniform2f(u[4],params.width,params.height);
    glUniform2f(u[5],params.transX,params.transY);
    glUniform4f(u[6],params.time1,params.time2,params.time3,params.time5);
    glUniform4f(u[7],float(params.clip.x),float(params.clip.y),
        float(double(params.clip.x)+params.clip.width),float(double(params.clip.y)+params.clip.height));
    glUniform1ui(u[8],params.layer); glUniform1ui(u[9],params.night);
    glUniform1ui(u[10],texture->slot|texture->flags|params.samplingFlags);
    glUniform1ui(u[11],params.sampler);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,feedback.image ? feedback.image : texture->image);
    glDisable(GL_SCISSOR_TEST);
    if (params.blend==GpuBlend::Opaque) glDisable(GL_BLEND);
    else {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(target->render ? GL_ONE : GL_SRC_ALPHA,
            params.blend==GpuBlend::Additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    }
    glDrawElements(GL_TRIANGLES,450,GL_UNSIGNED_SHORT,nullptr);
    // Every regular flush/image pass rebinds its own VAO, program and target.
    s.Check("Draw OpenGL pool");
}
void GLRenderer::SubmitCoverageTriangles(std::span<const GpuVertex> vertices,
    GLTexture* texture, GpuBlend blend, const GpuClip* clip)
{
    impl_->Submit(vertices, texture, blend, clip, true);
}
void GLRenderer::Impl::Quad()
{
    glBindVertexArray(vao); glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
bool GLRenderer::Impl::Present()
{
    if (!context.BeginPresent()) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    const auto rect = FitSceneToSurface(width, height, context.width, context.height);
    glViewport(rect.x, context.height - rect.y - rect.height, rect.width, rect.height);
    glUseProgram(presentProgram); glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene->image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    scene->sampler=1;
    glUniform1i(presentUniforms[uImage], 0);
    Quad(); Check("Present retained OpenGL scene"); context.Present();
    return true;
}
void GLRenderer::EndSceneAndPresent()
{
    auto& s = *impl_;
    if (!s.open) throw std::logic_error("No OpenGL scene to end");
    s.Flush();
    if (s.mainFrame) {
        s.Present();
        s.sceneInitialized = true;
        if (s.frameSync) {
            auto& frame = s.frames[s.frameIndex];
            frame.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            if (!frame.fence) throw std::runtime_error("OpenGL frame fence allocation failed");
            // The fence follows presentation commands too. Submit it now,
            // rather than making the next frame's zero-timeout query flush.
            glFlush();
        }
        s.frameIndex = (s.frameIndex + 1) % Impl::FrameCount;
    }
    else {
        // The common image-only batch contract is synchronous. Core GL 3.0
        // has no sync objects; main-frame draws never take this wait.
        glFinish();
    }
    s.open = false;
}
bool GLRenderer::PresentRetainedScene()
{
    if (impl_->open || impl_->selected)
        throw std::logic_error("Retained OpenGL presentation requires the main scene and no open batch");
    if (!impl_->sceneInitialized) return false;
    return impl_->Present();
}
bool GLRenderer::ResizePresent()
{
    if (impl_->open) throw std::logic_error("Resize OpenGL presentation between scenes");
    return impl_->context.BeginPresent();
}
void GLRenderer::ResizeScene(int width, int height)
{
    auto& s = *impl_;
    if (s.open || s.selected)
        throw std::logic_error("Resize the main OpenGL canvas between scenes");
    if (s.width == width && s.height == height) return;
    auto* next = s.Allocate(width, height, GL_RGBA8, false);
    auto* previous = s.scene;
    s.scene = next;
    s.width = width;
    s.height = height;
    s.sceneInitialized = false;
    DestroyTexture(previous);
    s.BindTarget();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}
void GLRenderer::SetHdrParams(const VulkanHdrParams& params) { params.Validate(); impl_->hdr = params; }
VulkanHdrParams GLRenderer::HdrParams() const { return impl_->hdr; }
void GLRenderer::PrepareScale(bool, std::span<const GLScaleStep> steps, unsigned int, unsigned int)
{
    if (!steps.empty()) throw std::runtime_error(
        "OpenGL 3.0 cannot execute the configured HLSL compute scaling chain; GLSL 1.30 has no compute stage");
}
void GLRenderer::WarmScale(bool) {}
void GLRenderer::SetScaleMode(bool enabled, bool)
{
    if (enabled) throw std::runtime_error("OpenGL 3.0 custom compute scaling is not supported");
}
GLScaleImage GLRenderer::ScaleOutput(bool) const { return {}; }
GLRenderer::ScaleCounts GLRenderer::GetScaleCounts() const { return {}; }
GpuClip GLRenderer::PresentRect(int w, int h) const
{
    return FitSceneToSurface(impl_->width, impl_->height, w, h);
}
unsigned int GLRenderer::MaxImageDimension2D() const { return impl_->maximum; }
GLRenderer::ResourceCounts GLRenderer::GetResourceCounts() const
{
    return {impl_->textures.size(),impl_->retired.size(),0,
        Impl::FrameCount+size_t(bool(impl_->poolVertices)),impl_->batches.size()};
}
}
