#include "ParticleDefinitionCompiled.h"
#include "TodParticle.h"
#include "paklib/PakInterface.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace
{
constexpr FloatParameterTrack TodEmitterDefinition::* systemTracks[] = {
    &TodEmitterDefinition::mSystemDuration, &TodEmitterDefinition::mCrossFadeDuration,
    &TodEmitterDefinition::mSpawnRate, &TodEmitterDefinition::mSpawnMinActive,
    &TodEmitterDefinition::mSpawnMaxActive, &TodEmitterDefinition::mSpawnMaxLaunched,
    &TodEmitterDefinition::mEmitterRadius, &TodEmitterDefinition::mEmitterOffsetX,
    &TodEmitterDefinition::mEmitterOffsetY, &TodEmitterDefinition::mEmitterBoxX,
    &TodEmitterDefinition::mEmitterBoxY, &TodEmitterDefinition::mEmitterSkewX,
    &TodEmitterDefinition::mEmitterSkewY, &TodEmitterDefinition::mEmitterPath,
    &TodEmitterDefinition::mParticleDuration, &TodEmitterDefinition::mLaunchSpeed,
    &TodEmitterDefinition::mLaunchAngle};
constexpr unsigned RenderTrackCount = 16;

struct Reader
{
    PFILE* file;
    std::size_t remaining;
    const char* path;
    void require(bool condition)
    {
        if (!condition) throw std::runtime_error(std::string("Invalid compiled particle data: ") + path);
    }
    void bytes(void* target, std::size_t count)
    {
        require(count <= remaining && count <= 0x7fffffff);
        require(!count || p_fread(target, 1, int(count), file) == count);
        remaining -= count;
    }
    template<class T> T value()
    {
        T result{};
        bytes(&result, sizeof(result));
        return result;
    }
    unsigned word() { return value<std::uint32_t>(); }
    void track(FloatParameterTrack& result)
    {
        const auto count = word();
        require(count <= 100000 && count <= remaining / 24);
        result.mNodes = count ? new FloatParameterTrackNode[count]{} : nullptr;
        result.mCountNodes = int(count);
        for (unsigned i = 0; i < count; ++i)
        {
            auto& node = result.mNodes[i];
            node.mTime = value<double>();
            node.mLow = value<float>(); node.mHigh = value<float>();
            node.mCurveType = int(word()); node.mDistribution = int(word());
            require(std::isfinite(node.mTime) &&
                std::isfinite(node.mLow) && std::isfinite(node.mHigh) &&
                node.mCurveType >= 0 && node.mCurveType < 14 &&
                node.mDistribution >= 0 && node.mDistribution < 14);
        }
    }
    void discardTrack()
    {
        const auto count = word();
        require(count <= 100000 && count <= remaining / 24);
        for (unsigned i = 0; i < count; ++i)
        {
            const auto time = value<double>();
            const auto low = value<float>(); const auto high = value<float>();
            const auto curve = value<std::uint32_t>(); const auto distribution = value<std::uint32_t>();
            require(std::isfinite(time) && std::isfinite(low) && std::isfinite(high) &&
                curve < 14 && distribution < 14);
        }
    }
    void fields(EmitterFieldArray& result)
    {
        const auto count = word();
        require(count <= MAX_PARTICLE_FIELDS && count <= remaining / 12);
        result.Fields = count ? new ParticleField[count]{} : nullptr;
        result.count = int(count);
        for (unsigned i = 0; i < count; ++i)
        {
            auto& field = result.Fields[i];
            const auto type = word();
            require(type > FIELD_INVALID && type < PARTICLE_FIELD_COUNT);
            field.mFieldType = static_cast<ParticleFieldType>(type);
            track(field.mX); track(field.mY);
        }
    }
};
}

void TodParticleFreeCompiledDefinition(TodParticleDefinition& definition)
{
    for (unsigned v = 0; v < definition.mVariantCount && definition.mTextures; ++v)
        DefinitionUnbindAtlas(definition.mTextures[v]);
    for (int i = 0; i < definition.mEmitterDefCount && definition.mEmitterDefs; ++i)
    {
        auto& emitter = definition.mEmitterDefs[i];
        for (auto member : systemTracks) delete[] (emitter.*member).mNodes;
        for (auto member : {&TodEmitterDefinition::mParticleSpinAngle,
            &TodEmitterDefinition::mParticleSpinSpeed, &TodEmitterDefinition::mCollisionReflect,
            &TodEmitterDefinition::mCollisionSpin, &TodEmitterDefinition::mAnimationRate})
            delete[] (emitter.*member).mNodes;
        for (auto* fields : {&emitter.mParticleFields, &emitter.mSystemFields})
        {
            for (int f = 0; f < fields->count; ++f)
            {
                delete[] fields->Fields[f].mX.mNodes;
                delete[] fields->Fields[f].mY.mNodes;
            }
            delete[] fields->Fields;
        }
    }
    delete[] definition.mTextures;
    delete[] definition.mImageRects;
    delete[] definition.mEmitterDefs;
    definition = TodParticleDefinition{};
}

void TodParticleLoadCompiledDefinition(TodParticleDefinition& definition, const char* path)
{
    if (definition.mEmitterDefs || definition.mTextures)
        throw std::logic_error("Particle definition is already loaded");
    auto* file = p_fopen(path, "rb");
    if (!file) throw std::runtime_error(std::string("Missing compiled particle data: ") + path);
    try
    {
        if (p_fseek(file, 0, SEEK_END)) throw std::runtime_error("Particle data seek failed");
        const int length = p_ftell(file);
        if (length < 24 || length > 256 * 1024 * 1024 || p_fseek(file, 0, SEEK_SET))
            throw std::runtime_error("Invalid particle data length");
        Reader reader{file, std::size_t(length), path};
        reader.require(reader.word() == 0x31435450 &&
            reader.word() == unsigned(ParticleEmitterName::top) &&
            reader.word() == kParticleEmitterNameTableHash);
        const unsigned emitters = reader.word(), variants = reader.word(), slots = reader.word();
        reader.require(emitters <= 1000 && variants > 0 && variants <= 4096 &&
            slots <= 100000 && std::uint64_t(variants) * slots <= reader.remaining / 24);
        definition.mEmitterDefs = new TodEmitterDefinition[emitters]{};
        definition.mEmitterDefCount = int(emitters);
        definition.mTextures = new Sexy::GpuTexture*[variants]{};
        definition.mVariantCount = variants;
        auto* rectangles = new Sexy::TextureRegion[std::size_t(variants) * slots]{};
        definition.mImageRects = rectangles;
        definition.mImageRectCount = slots;
        unsigned nextSlot = 0;
        for (unsigned i = 0; i < emitters; ++i)
        {
            auto& emitter = definition.mEmitterDefs[i];
            emitter.mImageRectOffset = reader.word();
            emitter.mImageRectCount = int(reader.word());
            emitter.mImageFrames = reader.value<std::int32_t>();
            emitter.mAnimated = reader.value<std::int32_t>();
            emitter.mParticleFlags = reader.value<std::int32_t>();
            const auto type = reader.word(), name = reader.word(), duration = reader.word();
            // 渲染通道掩码随 CPU 绘制路径一并恢复读取（GPU/测试 oracle 不再独占）。
            emitter.mRenderTrackMask = reader.word();
            reader.require(emitter.mImageRectOffset == nextSlot && emitter.mImageRectCount >= 0 &&
                unsigned(emitter.mImageRectCount) <= slots - nextSlot && emitter.mImageFrames > 0 &&
                (emitter.mAnimated == 0 || emitter.mAnimated == 1) &&
                emitter.mParticleFlags >= 0 && emitter.mParticleFlags < (1 << 12) &&
                type <= 4 && name < unsigned(ParticleEmitterName::top) &&
                (duration == 0xffffffff || duration < emitters));
            nextSlot += unsigned(emitter.mImageRectCount);
            emitter.mEmitterType = static_cast<EmitterType>(type);
            emitter.mNameId = static_cast<ParticleEmitterName>(name);
            emitter.mOnDurationEmitterIndex = duration == 0xffffffff ? -1 : int(duration);
            if (definition.mEmitterIndexByName[name] < 0) definition.mEmitterIndexByName[name] = int(i);
            for (auto member : systemTracks) reader.track(emitter.*member);
            reader.track(emitter.mSystemRed);
            reader.track(emitter.mSystemGreen);
            reader.track(emitter.mSystemBlue);
            reader.track(emitter.mSystemAlpha);
            reader.track(emitter.mSystemBrightness);
            reader.fields(emitter.mParticleFields); reader.fields(emitter.mSystemFields);
            reader.track(emitter.mParticleRed);
            reader.track(emitter.mParticleGreen);
            reader.track(emitter.mParticleBlue);
            reader.track(emitter.mParticleAlpha);
            reader.track(emitter.mParticleBrightness);
            reader.track(emitter.mParticleSpinAngle);
            reader.track(emitter.mParticleSpinSpeed);
            reader.track(emitter.mParticleScale);
            reader.track(emitter.mParticleStretch);
            reader.track(emitter.mCollisionReflect);
            reader.track(emitter.mCollisionSpin);
            reader.track(emitter.mClipTop);
            reader.track(emitter.mClipBottom);
            reader.track(emitter.mClipLeft);
            reader.track(emitter.mClipRight);
            reader.track(emitter.mAnimationRate);
        }
        reader.require(nextSlot == slots);
        for (unsigned v = 0; v < variants; ++v)
        {
            const auto width = reader.word(), height = reader.word(), pathLength = reader.word();
            reader.require(width <= 16384 && height <= 16384 && pathLength <= 4096);
            std::string atlasPath(pathLength, '\0');
            reader.bytes(atlasPath.data(), pathLength);
            reader.require(atlasPath.find('\0') == std::string::npos &&
                ((width && height && pathLength) || (!width && !height && !pathLength)));
            for (unsigned slot = 0; slot < slots; ++slot)
            {
                auto& rect = rectangles[std::size_t(v) * slots + slot];
                rect.mU0 = reader.value<float>(); rect.mV0 = reader.value<float>();
                rect.mU1 = reader.value<float>(); rect.mV1 = reader.value<float>();
                rect.mWidth = reader.value<std::int32_t>(); rect.mHeight = reader.value<std::int32_t>();
                reader.require(std::isfinite(rect.mU0) && std::isfinite(rect.mV0) &&
                    std::isfinite(rect.mU1) && std::isfinite(rect.mV1) &&
                    rect.mU0 >= 0 && rect.mV0 >= 0 && rect.mU1 >= rect.mU0 && rect.mV1 >= rect.mV0 &&
                    rect.mU1 <= 1 && rect.mV1 <= 1 && rect.mWidth >= 0 && rect.mHeight >= 0 &&
                    unsigned(rect.mWidth) <= width && unsigned(rect.mHeight) <= height);
            }
            if (pathLength) DefinitionBindAtlasPath(definition.mTextures[v], int(width), int(height), atlasPath.c_str());
        }
        reader.require(reader.remaining == 0);
    }
    catch (...)
    {
        p_fclose(file);
        TodParticleFreeCompiledDefinition(definition);
        throw;
    }
    p_fclose(file);
}
