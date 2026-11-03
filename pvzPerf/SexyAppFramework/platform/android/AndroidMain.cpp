#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include "../../Sexy.TodLib/TodCommon.h"

#include "SexyAppFramework/platform/ErrorDialog.h"
#include "SexyAppFramework/Common.h"
#include "SexyAppFramework/JsonDoc.h"
#include <android/log.h>
#include <cerrno>
#include <cstdlib>
#include <cstdio>

namespace Sexy {
bool ShowNativeErrorDialog(const char* message, const char* title)
{
    __android_log_print(ANDROID_LOG_FATAL, "pvz_perf", "%s: %s",
        title ? title : "Error", message ? message : "(null)");
    std::fprintf(stderr, "%s: %s\n", title ? title : "Error",
        message ? message : "(null)");
    return true;
}
}

// main.cpp 在 Android 构建中被排除；全局函数指针在此定义（TodCommon.h 声明）。
bool (*gAppCloseRequest)();
bool (*gGetCurrentLevelName)();
bool (*gAppHasUsedCheatKeys)();
bool (*gExtractResourcesByGroup)(class ResourceManager*, int);
#include "../../Sexy.TodLib/TodDebug.h"
#include "AndroidWindow.h"
#include <android_native_app_glue.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <exception>
#include <android/asset_manager.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace Sexy;

namespace {
LawnApp* app = nullptr;
bool started = false;

void ExtractAsset(AAssetManager* manager, const char* name, const std::filesystem::path& destination) {
    if (!manager) throw std::runtime_error("Android asset manager is unavailable");
    AAsset* asset = AAssetManager_open(manager, name, AASSET_MODE_STREAMING);
    if (!asset) throw std::runtime_error(std::string("Android asset is missing: ") + name);
    const auto closeAsset = [&] { AAsset_close(asset); };
    std::filesystem::create_directories(destination.parent_path());
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) { closeAsset(); throw std::runtime_error("Cannot create extracted Android asset"); }
    std::vector<char> buffer(1024 * 1024);  // 堆分配：native_app_glue 线程栈扛不住 1MB 栈缓冲（android_main+1206 SEGV 栈溢出案）
    while (true) {
        const int count = AAsset_read(asset, buffer.data(), buffer.size());
        if (count < 0) { closeAsset(); throw std::runtime_error("Cannot read Android asset"); }
        if (count == 0) break;
        output.write(buffer.data(), count);
        if (!output) { closeAsset(); throw std::runtime_error("Cannot write extracted Android asset"); }
    }
    closeAsset();
}

// 把 APK assets 下的整目录（如 shaders/）解到运行目录。
void ExtractAssetDirectory(AAssetManager* manager, const char* directory,
    const std::filesystem::path& destination) {
    AAssetDir* dir = AAssetManager_openDir(manager, directory);
    if (!dir) throw std::runtime_error(std::string("Android asset dir is missing: ") + directory);
    const auto closeDir = [dir] { AAssetDir_close(dir); };
    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr) {
        const std::string assetPath = std::string(directory) + "/" + name;
        try {
            ExtractAsset(manager, assetPath.c_str(), destination / name);
        } catch (...) {
            closeDir();
            throw;
        }
    }
    closeDir();
}
}

void android_main(android_app* state) {
    state->onAppCmd = AndroidAppCommand;
    state->onInputEvent = AndroidAppInput;
    BindAndroidApp(state);
    // 诊断文件与可执行目录语义都依赖它；窗口命令在等待循环里就可能触发。
    SetAndroidRuntimeDirectory(state->activity->internalDataPath);
    std::string settingsPath;
    int requestedTargetFps=0;
    int resetEndless=0;
    // Prefer the Activity's Android/media directory. Keep the former external
    // files directory as a migration source and a fallback when media is absent.
    {
        std::string appDataDir = state->activity->internalDataPath;
        std::string legacyExternalDir;
        JNIEnv* env = state->activity->env;
        if (env && state->activity->vm) {
            state->activity->vm->AttachCurrentThread(&env, nullptr);
            const jclass cls = env->GetObjectClass(state->activity->clazz);
            // Android equivalent of a launch option; persists through the normal settings file.
            const jmethodID getIntent=env->GetMethodID(cls,"getIntent","()Landroid/content/Intent;");
            const jobject intent=getIntent ? env->CallObjectMethod(state->activity->clazz,getIntent) : nullptr;
            if (!env->ExceptionCheck() && intent) {
                const jclass intentClass=env->GetObjectClass(intent);
                const jmethodID getIntExtra=env->GetMethodID(intentClass,"getIntExtra","(Ljava/lang/String;I)I");
                const jstring key=env->NewStringUTF("targetFps");
                if (getIntExtra) requestedTargetFps=env->CallIntMethod(intent,getIntExtra,key,0);
                const jstring resetKey=env->NewStringUTF("resetEndless");
                if (getIntExtra) resetEndless=env->CallIntMethod(intent,getIntExtra,resetKey,0);
                const jmethodID removeExtra=env->GetMethodID(intentClass,"removeExtra","(Ljava/lang/String;)V");
                if (removeExtra) env->CallVoidMethod(intent,removeExtra,resetKey);
                env->DeleteLocalRef(resetKey);
                env->DeleteLocalRef(key);env->DeleteLocalRef(intentClass);env->DeleteLocalRef(intent);
            }
            if (env->ExceptionCheck()) {env->ExceptionClear();requestedTargetFps=0;}
            const jmethodID mid = env->GetMethodID(
                cls, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
            if (mid) {
                const auto file = static_cast<jobject>(
                    env->CallObjectMethod(state->activity->clazz, mid, nullptr));
                if (!env->ExceptionCheck() && file) {
                    const jclass fileCls = env->GetObjectClass(file);
                    const jmethodID getAbs = env->GetMethodID(
                        fileCls, "getAbsolutePath", "()Ljava/lang/String;");
                    const auto jstr = static_cast<jstring>(
                        env->CallObjectMethod(file, getAbs));
                    const char* chars = env->GetStringUTFChars(jstr, nullptr);
                    const std::string external = chars;
                    legacyExternalDir = external;
                    env->ReleaseStringUTFChars(jstr, chars);
                    // 探测可写：真实 app 进程视角（run-as 域测试会失真）
                    const std::filesystem::path probe =
                        std::filesystem::path(external) / "pvz_probe";
                    if (FILE* f = std::fopen(probe.c_str(), "w")) {
                        std::fputc('x', f);
                        std::fclose(f);
                        std::remove(probe.c_str());
                        appDataDir = external;
                    }
                } else if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                }
            }
            const jmethodID getMedia = env->GetMethodID(cls, "getExternalMediaDirs", "()[Ljava/io/File;");
            const auto mediaDirs = getMedia ? static_cast<jobjectArray>(
                env->CallObjectMethod(state->activity->clazz, getMedia)) : nullptr;
            if (!env->ExceptionCheck() && mediaDirs) {
                for (jsize i = 0; i < env->GetArrayLength(mediaDirs); ++i) {
                    jobject directory = env->GetObjectArrayElement(mediaDirs, i);
                    if (!directory) continue;
                    jclass fileClass = env->GetObjectClass(directory);
                    jmethodID getPath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
                    auto path = getPath ? static_cast<jstring>(env->CallObjectMethod(directory, getPath)) : nullptr;
                    std::string media;
                    if (!env->ExceptionCheck() && path) {
                        if (const char* chars = env->GetStringUTFChars(path, nullptr)) {
                            media = chars;
                            env->ReleaseStringUTFChars(path, chars);
                        }
                    }
                    if (path) env->DeleteLocalRef(path);
                    env->DeleteLocalRef(fileClass);
                    env->DeleteLocalRef(directory);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (media.empty()) continue;
                    const auto probe = std::filesystem::path(media) / ".pvz-write-probe";
                    if (FILE* file = std::fopen(probe.string().c_str(), "w")) {
                        const bool writable = std::fputc('x', file) != EOF;
                        const bool closed = std::fclose(file) == 0;
                        std::remove(probe.string().c_str());
                        if (writable && closed) { appDataDir = media; break; }
                    }
                }
                env->DeleteLocalRef(mediaDirs);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(cls);
        }
        // Settings belong to the normal external data directory even when test saves are isolated.
        const std::filesystem::path settingsDirectory(appDataDir);
        settingsPath=(settingsDirectory/"settings.json").string();
        setenv("PVZ_SETTINGS_FILE",settingsPath.c_str(),1);
        const std::filesystem::path internal(state->activity->internalDataPath);
        if (std::filesystem::exists(internal / "gpu-benchmark.enable")) {
            // Keep the original GPU fixture intact while comparing an extra
            // dense case. Both are isolated from normal player data.
            const auto fixture = internal / (std::filesystem::exists(internal / "gpu-benchmark-dense.enable")
                ? "gpu-dense-fixture-20261028" : "gpu-only-fixture-20261027");
            const auto session = internal / "gpu-benchmark-session";
            if (!std::filesystem::is_regular_file(fixture / "game1_58.dat64"))
                throw std::runtime_error("GPU benchmark fixture is missing");
            std::filesystem::create_directories(session);
            for (const char* name : {"game1_58.dat64", "user1.dat64", "users.dat"})
                std::filesystem::copy_file(fixture / name, session / name,
                    std::filesystem::copy_options::overwrite_existing);
            appDataDir = session.string();
            setenv("PVZ_ANDROID_GPU_FIXTURE", "1", 1);
        }
        setenv("PVZ_APPDATA_DIR", appDataDir.c_str(), 1);
        // Import once, preferring the latest external files over old internal
        // copies. Without a marker, consumed level saves would resurrect on
        // the next launch. Never overwrite files already in the destination.
        const auto migrationMarker = settingsDirectory / ".user-data-migrated";
        if (!std::filesystem::exists(migrationMarker)) {
            namespace fs2 = std::filesystem;
            for (const fs2::path& source : {fs2::path(legacyExternalDir), internal}) {
                if (source.empty() || source == settingsDirectory || !fs2::exists(source)) continue;
                for (const auto& entry : fs2::directory_iterator(source)) {
                    if (!entry.is_regular_file()) continue;
                    const auto name = entry.path().filename().string();
                    if (entry.path().extension() != ".dat64" ||
                        (name.rfind("user", 0) != 0 && name.rfind("game", 0) != 0)) continue;
                    const auto to = settingsDirectory / name;
                    if (!fs2::exists(to)) fs2::copy_file(entry.path(), to);
                }
                // Publish the profile index only after its records are present.
                for (const char* name : {"settings.json", "scale.json", "users.dat"}) {
                    const auto from = source / name, to = settingsDirectory / name;
                    if (!fs2::exists(to) && fs2::is_regular_file(from)) fs2::copy_file(from, to);
                }
            }
            std::ofstream completed(migrationMarker);
            completed << "1\n";
            completed.close();
            if (!completed) throw std::runtime_error("Cannot commit Android user-data migration");
        }
    }
    while (!state->window) {
        int events = 0;
        android_poll_source* source = nullptr;
        if (ALooper_pollOnce(-1, nullptr, &events,
                reinterpret_cast<void**>(&source)) >= 0 && source)
            source->process(state, source);
    }
    try {
        gTodStringFormats = gLawnStringFormats;
        gTodStringFormatCount = gLawnStringFormatCount;
        gAppCloseRequest = LawnGetCloseRequest;
        // Test APKs include the perfect profile and canonical endless start.
        // Publish users.dat last; an interrupted first launch can retry without
        // replacing an established profile or a consumed level save.
        const char* dataPath = std::getenv("PVZ_APPDATA_DIR");
        const std::filesystem::path userDirectory(dataPath ? dataPath : state->activity->internalDataPath);
        const auto scalePath = userDirectory / "scale.json";
        if (!std::filesystem::exists(scalePath)) {
            const auto temporary = userDirectory / "scale.json.tmp";
            ExtractAsset(state->activity->assetManager, "scale.json", temporary);
            std::filesystem::rename(temporary, scalePath);
        }
        setenv("PVZ_SCALE_CONFIG_FILE", scalePath.string().c_str(), 1);
        if (!std::filesystem::exists(userDirectory / "users.dat")) {
            for (const char* name : {"user1.dat64", "game1_58.dat64", "users.dat"}) {
                const std::string asset = std::string("bundled-saves/") + name;
                ExtractAsset(state->activity->assetManager, asset.c_str(), userDirectory / name);
            }
        }
        const std::filesystem::path internalDirectory(state->activity->internalDataPath);
        int previousReset=0;
        if (std::ifstream marker(internalDirectory / "endless-reset-token"); marker)
            marker >> previousReset;
        if (resetEndless > 0 && resetEndless != previousReset) {
            const auto level=userDirectory / "game1_58.dat64";
            const std::filesystem::path internal(state->activity->internalDataPath);
            // Explicit benchmark reset replaces only this slot. Do not retain
            // superseded gameplay saves; the bundled baseline is authoritative.
            ExtractAsset(state->activity->assetManager,"bundled-saves/game1_58.dat64",level);
            // Export a copy from the actual application storage namespace for
            // external verification; run-as cannot write that path on all ROMs.
            std::filesystem::copy_file(level,internal / "endless-restored.dat64",
                std::filesystem::copy_options::overwrite_existing);
            // Do not let the old internal migration seed resurrect a consumed
            // level on a later launch after restoring the external slot.
            if (userDirectory != internal)
                std::filesystem::remove(internal / "game1_58.dat64");
            std::ofstream marker(internal / "endless-reset-token", std::ios::trunc);
            if (!(marker << resetEndless)) throw std::runtime_error("Cannot record completed endless reset");
        }
        app = new LawnApp();
        // main.cpp 在安卓构建被排除；gLawnApp（PoolEffect/关卡逻辑依赖）
        // 必须在此补赋值，否则资源加载在 LoadingThreadCompleted 处终止。
        gLawnApp = app;
        const std::filesystem::path runtimeRoot(state->activity->internalDataPath);
        ExtractAsset(state->activity->assetManager, "main.pak64", runtimeRoot / "main.pak64");
        ExtractAssetDirectory(state->activity->assetManager, "shaders", runtimeRoot / "shaders");
        // libbass.so 不走 assets：数据目录的 so 在 targetSdk 29+ 无法 dlopen，
        // 它随 APK 装进 nativeLibraryDir，由 BassLoader 按 soname 加载。
        SetAndroidRuntimeDirectory(runtimeRoot.string());
        app->mChangeDirTo = runtimeRoot.string();
        if (requestedTargetFps!=0) {
            const auto timing=FrameTiming::FromSetting(requestedTargetFps);
            JsonDoc settings;
            if (std::filesystem::exists(settingsPath) && !settings.LoadFile(settingsPath))
                throw std::runtime_error("Cannot read existing settings: "+settings.mError);
            settings.SetInt("targetFps",timing.mTargetFps);
            if (!settings.SaveFile(settingsPath)) throw std::runtime_error("Cannot save settings: "+settingsPath);
        }
        app->Init();
        started = true;
        app->Start();
        app->Shutdown();
        app->RethrowLoadingThreadError();
        delete app;
        app = nullptr;
        gLawnApp = nullptr;
    } catch (const std::exception& error) {
        __android_log_print(ANDROID_LOG_ERROR, "pvz_perf", "%s", error.what());
        if (app) { app->Shutdown(); delete app; app = nullptr; }
        gLawnApp = nullptr;
    }
    // android_main 返回即 glue 线程终结；finish 的 DESTROY 命令必须先在本
    // 线程泵完，否则 Activity 悬挂、系统报 destroy timeout、进程变僵尸。
    if (state->activity)
        ANativeActivity_finish(state->activity);
    while (!state->destroyRequested) {
        int events = 0;
        android_poll_source* source = nullptr;
        if (ALooper_pollOnce(-1, nullptr, &events,
                reinterpret_cast<void**>(&source)) >= 0 && source)
            source->process(state, source);
    }
}
