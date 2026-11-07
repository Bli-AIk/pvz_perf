#pragma once
#include <span>
#include <vector>

namespace Sexy {
struct WebAudioPcm {
    int channels = 0, sampleRate = 0;
    std::vector<float> samples;
};
WebAudioPcm DecodeWebOgg(std::span<const unsigned char> bytes);
}
