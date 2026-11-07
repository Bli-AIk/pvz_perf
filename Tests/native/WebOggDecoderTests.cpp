#include "platform/web/WebOggDecoder.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cmath>
#include <stdexcept>
#include <string>

int main(int argc, char** argv)
{
    try {
        if (argc != 2) throw std::runtime_error("Expected a list of OGG paths");
        std::ifstream list(argv[1]);
        unsigned files = 0;
        unsigned long long samples = 0;
        for (std::string path; std::getline(list, path);) {
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Cannot read " + path);
            const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), {}};
            const auto decoded = Sexy::DecodeWebOgg(bytes);
            for (const auto sample : decoded.samples)
                if (!std::isfinite(sample)) throw std::runtime_error("Nonfinite sample: " + path);
            samples += decoded.samples.size();
            ++files;
        }
        if (!files) throw std::runtime_error("Empty corpus");
        try {
            Sexy::DecodeWebOgg({});
            throw std::runtime_error("Empty input accepted");
        } catch (const std::runtime_error& error) {
            if (std::string(error.what()) == "Empty input accepted") throw;
        }
        std::cout << "Decoded " << files << " OGG files, " << samples << " finite PCM samples\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
