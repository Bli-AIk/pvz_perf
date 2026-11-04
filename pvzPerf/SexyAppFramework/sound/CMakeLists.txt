target_sources(${PROJECT_NAME} PRIVATE
    "SoundDecode.h"
    "SoundDecode.cpp"
    "BassMusicInterface.h"
    "DummyMusicInterface.h"
    "MusicInterface.h"
    "SoundInstance.h"
    "SoundManager.h"
    "BassLoader.cpp"
    "BassLoader.h"
    "BassMusicInterface.cpp"
    #"SoundBuild.cpp"
)
if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        "../platform/windows/audio/DSoundManager.cpp"
        "../platform/windows/audio/DSoundInstance.cpp")
elseif(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        "../platform/macos/audio/AudioUnitDevice.mm"
        "../platform/macos/audio/AudioUnitSoundManager.mm"
        "../platform/macos/audio/AudioUnitMusicPlayback.mm")
elseif(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Android：音效走 openal-soft（vcpkg 供给，AAudio/OpenSL 后端），
    # libopenal.so 随 APK 打包；音乐仍用 BASS Android 二进制。
    target_link_libraries(${PROJECT_NAME} PRIVATE android log)
    target_sources(${PROJECT_NAME} PRIVATE
        "../platform/audio/openal/OpenALDevice.cpp"
        "../platform/audio/openal/OpenALSoundManager.cpp")
    # CMAKE_ANDROID_ABI 在部分配置时序下不可靠；目标架构以处理器为准
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_PVZ_OPENAL_TRIPLET arm64-android)
    else()
        set(_PVZ_OPENAL_TRIPLET x64-android)
    endif()
    unset(PVZ_OPENAL_LIBRARY CACHE)  # 清除失败查找的负缓存
    set(_PVZ_OPENAL_CANDIDATES
        "${CMAKE_SOURCE_DIR}/out/vcpkg_installed/${_PVZ_OPENAL_TRIPLET}/lib/libopenal.a"
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/${_PVZ_OPENAL_TRIPLET}/lib/libopenal.a")
    set(PVZ_OPENAL_LIBRARY "")
    foreach(_candidate IN LISTS _PVZ_OPENAL_CANDIDATES)
        if(EXISTS "${_candidate}")
            set(PVZ_OPENAL_LIBRARY "${_candidate}")
            break()
        endif()
    endforeach()
    if(NOT PVZ_OPENAL_LIBRARY)
        message(FATAL_ERROR "libopenal.a not found: abi=${CMAKE_ANDROID_ABI} triplet=${_PVZ_OPENAL_TRIPLET} src=${CMAKE_SOURCE_DIR}")
    endif()
    # openal-soft 1.25 依赖 {fmt}（vcpkg 同目录供给）
    string(REGEX REPLACE "libopenal\.a$" "libfmt.a" PVZ_FMT_LIBRARY "${PVZ_OPENAL_LIBRARY}")
    if(NOT EXISTS "${PVZ_FMT_LIBRARY}")
        message(FATAL_ERROR "libfmt.a not found next to libopenal.a")
    endif()
    # 静态 openal-soft：安卓后端（AAudio/OpenSL ES）需要系统库
    target_link_libraries(${PROJECT_NAME} PRIVATE "${PVZ_OPENAL_LIBRARY}" "${PVZ_FMT_LIBRARY}" OpenSLES)
    # AAudio entered Android in API 26. OpenAL's Android backends load it
    # dynamically when available; API 24/25 must retain the OpenSL ES path.
    if(CMAKE_SYSTEM_VERSION GREATER_EQUAL 26)
        target_link_libraries(${PROJECT_NAME} PRIVATE aaudio)
    endif()
else()
    find_package(OpenAL REQUIRED)
    target_sources(${PROJECT_NAME} PRIVATE
        "../platform/audio/openal/OpenALDevice.cpp"
        "../platform/audio/openal/OpenALSoundManager.cpp"
        "../platform/audio/openal/OpenALMusicPlayback.cpp")
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL::OpenAL)
endif()
