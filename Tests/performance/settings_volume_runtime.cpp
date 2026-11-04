// Links the production configuration reader/writer; never opens a game window.
#include "SexyAppBase.h"
#include "JsonDoc.h"
#include <filesystem>
#include <iostream>
#include <cmath>
#include <algorithm>

bool FixtureCloseRequest() { return false; }
bool (*gAppCloseRequest)() = FixtureCloseRequest;

class SettingsApp final : public Sexy::SexyAppBase {
public:
    void ButtonPress(int) override {}
    void ButtonDepress(int) override {}
    void ButtonDownTick(int) override {}
    void ButtonMouseEnter(int) override {}
    void ButtonMouseLeave(int) override {}
    void ButtonMouseMove(int, int, int) override {}
};

int main(int argc, char** argv) try {
    if (argc != 2) throw std::runtime_error("Pass an isolated settings directory");
    const auto directory = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(directory);
    const auto path = (directory / "settings.json").string();
    SettingsApp app;
    app.mChangeDirTo = directory.string();
    for (const long long input : {-999LL, -1LL, 0LL, 1LL, 37LL, 65LL, 99LL, 100LL, 101LL, 999LL}) {
        JsonDoc source;
        source.SetBool("autoDetectFrameRate", false);
        source.SetInt("targetFps", 240);
        source.SetInt("musicVolume", input);
        source.SetInt("sfxVolume", input);
        if (!source.SaveFile(path)) throw std::runtime_error("Cannot write fixture config");
        app.ReadFromRegistry();
        const auto value = std::clamp(input, 0LL, 100LL);
        if (std::abs(app.mMusicVolume - value / 100.0) > 1e-7 ||
            std::abs(app.mSfxVolume - value / 100.0 * .65) > 1e-12)
            throw std::runtime_error("Config percentages were not clamped/mapped to mixer gains");
        if(app.mLandscapeUI) throw std::runtime_error("Missing landscapeUI must default to false");
        app.WriteToRegistry();
        JsonDoc result;
        if (!result.LoadFile(path) || result.GetInt("musicVolume", -1) != value ||
            result.GetInt("sfxVolume", -1) != value)
            throw std::runtime_error("Clamped slider percentages did not round-trip");
    }
    for (int value = 0; value <= 100; ++value) {
        app.SetMusicVolume(value / 100.0f);
        app.SetSfxVolume(value / 100.0 * .65);
        app.WriteToRegistry();
        JsonDoc result;
        if (!result.LoadFile(path) || result.GetInt("musicVolume", -1) != value ||
            result.GetInt("sfxVolume", -1) != value)
            throw std::runtime_error("Slider value lost precision while saving");
    }
    for (bool enabled : {true,false}) {
        JsonDoc source;source.SetBool("landscapeUI",enabled);source.SaveFile(path);
        app.ReadFromRegistry();
        if(app.mLandscapeUI!=enabled) throw std::runtime_error("landscapeUI input lost");
        app.WriteToRegistry();JsonDoc result;result.LoadFile(path);
        if(result.GetBool("landscapeUI",!enabled)!=enabled) throw std::runtime_error("landscapeUI did not round-trip");
    }
    std::cout << "PASS production settings: clamped input, 0..100 sliders, original SFX gain, all 101 integer round-trips\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n'; return 1;
}
