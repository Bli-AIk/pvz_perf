#include "WebOggDecoder.h"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#include <limits>
#include <stdexcept>
#include <string>

namespace Sexy {
WebAudioPcm DecodeWebOgg(std::span<const unsigned char> bytes)
{
    if (bytes.size() > std::numeric_limits<int>::max())
        throw std::runtime_error("OGG file is too large");
    int error = 0;
    auto* decoder = stb_vorbis_open_memory(bytes.data(), static_cast<int>(bytes.size()), &error, nullptr);
    if (!decoder) throw std::runtime_error("OGG header decode failed (" + std::to_string(error) + ")");
    try {
        const auto info = stb_vorbis_get_info(decoder);
        const auto length = stb_vorbis_stream_length_in_samples(decoder);
        if (info.channels < 1 || info.channels > 2 || !info.sample_rate || !length ||
            length > std::numeric_limits<int>::max() / info.channels)
            throw std::runtime_error("Invalid OGG channel count, rate or sample length");
        WebAudioPcm result;
        result.channels = info.channels;
        result.sampleRate = static_cast<int>(info.sample_rate);
        result.samples.resize(static_cast<size_t>(length) * info.channels);
        size_t written = 0;
        while (written < result.samples.size()) {
            const auto frames = stb_vorbis_get_samples_float_interleaved(decoder, info.channels,
                result.samples.data() + written, static_cast<int>(result.samples.size() - written));
            if (!frames) break;
            written += static_cast<size_t>(frames) * info.channels;
        }
        if (written != result.samples.size()) throw std::runtime_error("Incomplete OGG sample data");
        stb_vorbis_close(decoder);
        return result;
    } catch (...) { stb_vorbis_close(decoder); throw; }
}
}
