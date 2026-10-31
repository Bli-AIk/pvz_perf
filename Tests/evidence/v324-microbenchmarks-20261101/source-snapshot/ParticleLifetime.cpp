#include "TodParticle.h"
#include "ReanimClock.h"
#include <algorithm>

std::int64_t ParticleLifeAgeAt(std::int64_t nowNs, std::int64_t birthNs, std::int64_t durationNs)
{
    return std::min(ReanimClock::Subtract(nowNs, birthNs), durationNs);
}

namespace {
double NormalizedLife(std::int64_t nowNs, std::int64_t birthNs, std::int64_t durationNs)
{
    const auto age = ParticleLifeAgeAt(nowNs, birthNs, durationNs);
    return age < 0 ? -1.0 : double(age) / std::max<std::int64_t>(1, durationNs);
}
}

void RefreshParticleLifeFraction(TodParticle& particle, std::int64_t nowNs)
{
    auto& holder = *particle.mParticleEmitter->mParticleSystem->mParticleHolder;
    holder.mData->mParticleLifeFractions[holder.mParticles.DataArrayIndexOf(&particle)] = NormalizedLife(nowNs, particle.mBirthTimeNs, particle.mParticleDurationNs);
}

void RefreshParticleLifeFraction(TodParticleEmitter& emitter, std::int64_t nowNs)
{
    auto& holder = *emitter.mParticleSystem->mParticleHolder;
    holder.mData->mEmitterLifeFractions[holder.mEmitters.DataArrayIndexOf(&emitter)] = NormalizedLife(nowNs, emitter.mBirthTimeNs, emitter.mSystemDurationNs);
}

std::int64_t TodParticleAnimationPhase(const TodParticle& particle, const FloatParameterTrack& track)
{
    if (!TodParticleConstantAnimationRate(track)) return particle.mAnimationSampleIntegral;
    const auto nowNs = particle.mParticleEmitter && particle.mParticleEmitter->mParticleSystem
        ? particle.mParticleEmitter->mParticleSystem->mLastUpdateTimeNs : 0;
    const auto phase = ParticleClock::ConstantRatePhase(
        ReanimClock::Subtract(nowNs, particle.mAnimationBirthTimeNs), track.mNodes[0].mLow);
    const auto anchored = (phase + particle.mAnimationSampleIntegral % ParticleClock::Cycle) % ParticleClock::Cycle;
    return anchored < 0 ? anchored + ParticleClock::Cycle : anchored;
}

bool TodParticleConstantAnimationRate(const FloatParameterTrack& track)
{
    return track.mCountNodes == 1 && track.mNodes[0].mLow == track.mNodes[0].mHigh;
}
