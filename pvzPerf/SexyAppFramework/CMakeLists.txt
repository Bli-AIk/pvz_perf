set(PROJECT_NAME LawnProject)
################################################################################
# Source groups
################################################################################

set(Lawn__Board__Board_Include
    "../Lawn/Board.h"
    "../Lawn/Challenge.h"
    "../Lawn/Coin.h"
    "../Lawn/CursorObject.h"
    "../Lawn/Cutscene.h"
    "../Lawn/GameObject.h"
    "../Lawn/GridItem.h"
    "../Lawn/LawnMower.h"
    "../Lawn/MessageWidget.h"
    "../Lawn/Plant.h"
    "../Lawn/Projectile.h"
    "../Lawn/SeedPacket.h"
    "../Lawn/ToolTipWidget.h"
    "../Lawn/ZenGarden.h"
    "../Lawn/Zombie.h"
)
source_group("Lawn\\Board\\Board Include" FILES ${Lawn__Board__Board_Include})

set(Lawn__Board__Board_Source
    "../Lawn/Board.cpp"
    "../Lawn/BoardTouch.cpp"
    "../Lawn/LandscapeHud.cpp"
    "../Lawn/LandscapeHud.h"
    "../Lawn/Challenge.cpp"
    "../Lawn/Coin.cpp"
    "../Lawn/CursorObject.cpp"
    "../Lawn/CutScene.cpp"
    "../Lawn/GameObject.cpp"
    "../Lawn/GridItem.cpp"
    "../Lawn/LawnMower.cpp"
    "../Lawn/Motion.cpp"
    "../Lawn/MagnetMotion.cpp"
    "../Lawn/MessageWidget.cpp"
    "../Lawn/Plant.cpp"
    "../Lawn/Projectile.cpp"
    "../Lawn/SeedPacket.cpp"
    "../Lawn/SlotMachineMotion.cpp"
    "../Lawn/ToolTipWidget.cpp"
    "../Lawn/ZenGarden.cpp"
    "../Lawn/Zombie.cpp"
)
source_group("Lawn\\Board\\Board Source" FILES ${Lawn__Board__Board_Source})

set(Lawn__Common__Common_Include
    "../ConstEnums.h"
    "../GameConstants.h"
    "../Lawn/BoardInclude.h"
    "../Lawn/LawnCommon.h"
    "../Resources.h"
)
source_group("Lawn\\Common\\Common Include" FILES ${Lawn__Common__Common_Include})

set(Lawn__Common__Common_Source
    "../Lawn/LawnCommon.cpp"

    "../Resources.cpp"
)
source_group("Lawn\\Common\\Common Source" FILES ${Lawn__Common__Common_Source})

set(Lawn__System__System_Include
    "../Lawn/System/DataSync.h"
    "../Lawn/System/Music.h"
    "../Lawn/System/PlayerInfo.h"
    "../Lawn/System/PoolEffect.h"
    "../Lawn/System/PopDRMComm.h"
    "../Lawn/System/ProfileMgr.h"
    "../Lawn/System/ReanimationLawn.h"
    "../Lawn/System/SaveGame.h"
    "../Lawn/System/TypingCheck.h"
)
source_group("Lawn\\System\\System Include" FILES ${Lawn__System__System_Include})

set(Lawn__System__System_Source
    "../Lawn/System/DataSync.cpp"
    "../Lawn/System/Music.cpp"
    "../Lawn/System/PlayerInfo.cpp"
    "../Lawn/System/PoolEffect.cpp"
    "../Lawn/System/PopDRMComm.cpp"
    "../Lawn/System/ProfileMgr.cpp"
    "../Lawn/System/ReanimationLawn.cpp"
    "../Lawn/System/SaveGame.cpp"
    "../Lawn/System/SaveClockProjection.cpp"
    "../Lawn/System/TypingCheck.cpp"
)
source_group("Lawn\\System\\System Source" FILES ${Lawn__System__System_Source})

set(Lawn__Widget__Widget_Include
    "../Lawn/Widget/AchievementsWidget.h"
    "../Lawn/Widget/Almanac.h"
    "../Lawn/Widget/AwardScreen.h"
    "../Lawn/Widget/ChallengeScreen.h"
    "../Lawn/Widget/CheatDialog.h"
    "../Lawn/Widget/ContinueDialog.h"
    "../Lawn/Widget/CreditScreen.h"
    "../Lawn/Widget/GameButton.h"
    "../Lawn/Widget/GameSelector.h"
    "../Lawn/Widget/ImitaterDialog.h"
    "../Lawn/Widget/LawnDialog.h"
    "../Lawn/Widget/NewOptionsDialog.h"
    "../Lawn/Widget/NewUserDialog.h"
    "../Lawn/Widget/SeedChooserScreen.h"
    "../Lawn/Widget/StoreScreen.h"
    "../Lawn/Widget/TitleScreen.h"
    "../Lawn/Widget/UserDialog.h"
    "../Lawn/Widget/ZombatarColors.h"
    "../Lawn/Widget/ZombatarItems.h"
    "../Lawn/Widget/ZombatarScreen.h"
)
source_group("Lawn\\Widget\\Widget Include" FILES ${Lawn__Widget__Widget_Include})

set(Lawn__Widget__Widget_Source
    "../Lawn/Widget/AchievementsWidget.cpp"
    "../Lawn/Widget/Almanac.cpp"
    "../Lawn/Widget/AwardScreen.cpp"
    "../Lawn/Widget/ChallengeScreen.cpp"
    "../Lawn/Widget/CheatDialog.cpp"
    "../Lawn/Widget/ContinueDialog.cpp"
    "../Lawn/Widget/CreditScreen.cpp"
    "../Lawn/Widget/GameButton.cpp"
    "../Lawn/Widget/GameSelector.cpp"
    "../Lawn/Widget/ImitaterDialog.cpp"
    "../Lawn/Widget/LawnDialog.cpp"
    "../Lawn/Widget/NewOptionsDialog.cpp"
    "../Lawn/Widget/NewUserDialog.cpp"
    "../Lawn/Widget/SeedChooserScreen.cpp"
    "../Lawn/Widget/StoreScreen.cpp"
    "../Lawn/Widget/TitleScreen.cpp"
    "../Lawn/Widget/UserDialog.cpp"
    "../Lawn/Widget/ZombatarColors.cpp"
    "../Lawn/Widget/ZombatarItems.cpp"
    "../Lawn/Widget/ZombatarScreen.cpp"
    "../Lawn/Widget/ZombatarResources.cpp"
)
source_group("Lawn\\Widget\\Widget Source" FILES ${Lawn__Widget__Widget_Source})

set(TodLib__Common__TodCommon_Include
    "../Sexy.TodLib/DataArray.h"
    "../Sexy.TodLib/Definition.h"
    "../Sexy.TodLib/TodCommon.h"
    "../Sexy.TodLib/TodDebug.h"
    "../Sexy.TodLib/TodFoley.h"
    "../Sexy.TodLib/TodAllocator.h"
    "../Sexy.TodLib/TodStringFile.h"
    "../Sexy.TodLib/generated/LawnStrings.h"
    "../Sexy.TodLib/generated/EntityStrings.h"
)
source_group("TodLib\\Common\\TodCommon Include" FILES ${TodLib__Common__TodCommon_Include})

set(TodLib__Common__TodCommon_Source
    "../Sexy.TodLib/Definition.cpp"
    "../Sexy.TodLib/TodCommon.cpp"
    "misc/FastMath.cpp"
    "misc/FrameTiming.cpp"
    "../Sexy.TodLib/TodDebug.cpp"
    "../Sexy.TodLib/TodFoley.cpp"
    "../Sexy.TodLib/TodAllocator.cpp"
    "../Sexy.TodLib/TodStringFile.cpp"
    "../Sexy.TodLib/generated/LawnStrings.cpp"
    ${PVZ_STATIC_DEFINITION_SOURCES}
    "../Sexy.TodLib/generated/EntityStrings.cpp"
    "../Sexy.TodLib/generated/DaveDialogueMetadata.cpp"
)
source_group("TodLib\\Common\\TodCommon Source" FILES ${TodLib__Common__TodCommon_Source})

set(TodLib__Effect__Effect_Include
    "../Sexy.TodLib/Attachment.h"
    "../Sexy.TodLib/EffectSystem.h"
    "../Sexy.TodLib/FilterEffect.h"
    "../Sexy.TodLib/Reanimator.h"
    "../Sexy.TodLib/TodParticle.h"
)
source_group("TodLib\\Effect\\Effect Include" FILES ${TodLib__Effect__Effect_Include})

set(TodLib__Effect__Effect_Source
    "../Sexy.TodLib/Attachment.cpp"
    "../Sexy.TodLib/EffectSystem.cpp"
    "../Sexy.TodLib/FilterEffect.cpp"
    "../Sexy.TodLib/Reanimator.cpp"
    "../Sexy.TodLib/TodParticle.cpp"
    "../Sexy.TodLib/ParticleLifetime.cpp"
    "../Sexy.TodLib/ParticleDefinitionCompiled.cpp"
)
source_group("TodLib\\Effect\\Effect Source" FILES ${TodLib__Effect__Effect_Source})

set(ALL_FILES
    ${no_group_source_files}
    ${Lawn__Board__Board_Include}
    ${Lawn__Board__Board_Source}
    ${Lawn__Common__Common_Include}
    ${Lawn__Common__Common_Source}
    ${Lawn__System__System_Include}
    ${Lawn__System__System_Source}
    ${Lawn__Widget__Widget_Include}
    ${Lawn__Widget__Widget_Source}
    ${TodLib__Common__TodCommon_Include}
    ${TodLib__Common__TodCommon_Source}
    ${TodLib__Effect__Effect_Include}
    ${TodLib__Effect__Effect_Source}
)

################################################################################
# Target
################################################################################
# LTO（MSVC：/GL + /LTCG，vcxproj WholeProgramOptimization），仅 Release。
# 目录级变量须在 add_executable 之前设置才能覆盖本目录及全部子目标；
# Debug 不受影响。
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    # NDK Clang/LLD supports full LTO. CMake's generic IPO probe can select
    # host gold without the Android toolchain flags and falsely reject it.
    # Keep Release IPO mandatory for the actual Android targets instead.
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LAWN_IPO_SUPPORTED OUTPUT LAWN_IPO_ERROR)
    if(LAWN_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()
endif()

# P4：reanim attacher 掩码编译期生成（reanimc 重跑更新 MPHF 头 → 联动重生成，
# 仅 Reanimator.cpp 包含 → 改版只重编一个 obj）。恒跑目标 + 生成器内容比对：
# 无变化零写盘，ninja 不重编任何 obj
set_source_files_properties(${PVZ_STATIC_DEFINITION_SOURCES} PROPERTIES GENERATED TRUE)
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
elseif(MSVC)
    add_executable(${PROJECT_NAME} WIN32 ${ALL_FILES})
    target_compile_options(${PROJECT_NAME} PRIVATE /we4834)
elseif(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${gameShaderDirectory}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        VERBATIM)
else()
add_executable(${PROJECT_NAME} ${ALL_FILES})
target_compile_options(${PROJECT_NAME} PRIVATE -Werror=unused-result)
endif()
add_dependencies(${PROJECT_NAME} pvz_resources)


# GOTY 1073 officially excludes the cob-cannon projectile from roof-slope
# correction (the community-named "Wind from the Upper Boundary" fix).
# Projectile.cpp contains the only DO_FIX_BUGS block in this translation unit,
# so keep the definition source-local instead of enabling Patoke's unrelated
# experimental fixes throughout the game.
set_property(SOURCE "../Lawn/Projectile.cpp" APPEND PROPERTY
    COMPILE_DEFINITIONS DO_FIX_BUGS)

add_subdirectory(graphics)
add_subdirectory(sound)
add_subdirectory(widget)
add_subdirectory(misc)
add_subdirectory(paklib)
add_subdirectory(imagelib)

target_sources(${PROJECT_NAME} PRIVATE
    "Common.h"
    "Common.cpp"
    "graphics/GameViewport.cpp"
    "widget/InertialScroll.cpp"
    "widget/InertialScroll.h"
    "misc/FloatingPointMode.cpp"
    "misc/FloatingPointMode.h"
    "SexyApp.cpp"
    "SexyApp.h"
    "SexyAppBase.cpp"
    "SexyAppBase.h"
    "JsonDoc.cpp"
    "JsonDoc.h"



    # No group sources
    "${CMAKE_SOURCE_DIR}/assets/branding/Cursor13.cur"
    "../LawnApp.cpp"
    "../LawnApp.h"
    "../main.cpp"
    ".gitattributes"
    ".gitignore"
    "resource.h"
    "LawnProject.rc"
)

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    get_target_property(_PVZ_GAME_SOURCES ${PROJECT_NAME} SOURCES)
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX "(^|/)main\\.cpp$")
    set_property(TARGET ${PROJECT_NAME} PROPERTY SOURCES "${_PVZ_GAME_SOURCES}")
    target_sources(${PROJECT_NAME} PRIVATE
        "platform/android/AndroidMain.cpp"
        "platform/android/AndroidWindow.h")
    set(_PVZ_ANDROID_NDK "${CMAKE_ANDROID_NDK}")
    if(NOT _PVZ_ANDROID_NDK)
        set(_PVZ_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
    endif()
    set(_PVZ_ANDROID_GLUE "${_PVZ_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
    if(NOT EXISTS "${_PVZ_ANDROID_GLUE}")
        message(FATAL_ERROR "Android NDK native_app_glue.c not found: ${_PVZ_ANDROID_GLUE}")
    endif()
    target_sources(${PROJECT_NAME} PRIVATE "${_PVZ_ANDROID_GLUE}")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${_PVZ_ANDROID_NDK}/sources/android/native_app_glue"
        # OpenAL-soft 头（AL/al.h 等）随源码树供给；Android 运行库用 BASS，
        # 这些头仅为编译 platform/Audio.h 的引用闭包。
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/audio/openal")
    target_link_libraries(${PROJECT_NAME} PRIVATE android log)
endif()

if(APPLE)
    get_target_property(_PVZ_GAME_SOURCES ${PROJECT_NAME} SOURCES)
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX "Smoke\\.cpp$")
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX "\\.(rc|cur)$")
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX "/platform/(windows|linux)/")
    set_property(TARGET ${PROJECT_NAME} PROPERTY SOURCES "${_PVZ_GAME_SOURCES}")
    file(GLOB macAppSources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/*.mm")
    if(NOT macAppSources)
        message(FATAL_ERROR "The Cocoa platform sources must be integrated before building LawnProject")
    endif()
    target_sources(${PROJECT_NAME} PRIVATE ${macAppSources})
    target_compile_options(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        SEXY_WINDOW_COCOA=1 SEXY_RENDER_GPU=1 PVZ_GPU_METAL=1
        "$<$<NOT:$<CONFIG:Release>>:SEXY_DISABLE_MODVAL=1>")
    foreach(framework IN ITEMS Cocoa CoreGraphics Metal QuartzCore AudioUnit AudioToolbox CoreAudio)
        find_library(PVZ_MAC_${framework} NAMES ${framework} REQUIRED)
        target_link_libraries(${PROJECT_NAME} PRIVATE "${PVZ_MAC_${framework}}")
    endforeach()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path")
    message(STATUS "Experimental macOS: native MSL rendering and loading-time HLSL scale conversion")
endif()

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    get_target_property(_PVZ_GAME_SOURCES ${PROJECT_NAME} SOURCES)
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX
        "/(ModernVulkan[^/]*Smoke|Portable[^/]*Smoke|LoadingCloseSmoke|AttachmentLifetimeSmoke)\\.cpp$")
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX "\\.(rc|cur)$")
    set_property(TARGET ${PROJECT_NAME} PROPERTY SOURCES "${_PVZ_GAME_SOURCES}")
    target_sources(${PROJECT_NAME} PRIVATE
        "platform/linux/LinuxApp.cpp"
        "platform/linux/WindowGeometry.cpp"
        "platform/linux/CalendarTime.cpp"
        "platform/linux/UserDirectories.cpp"
        "platform/linux/TabletCaret.cpp")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SEXY_RENDER_GPU=1 SEXY_WINDOW_ANDROID=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_native_window android log)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    get_target_property(_PVZ_GAME_SOURCES ${PROJECT_NAME} SOURCES)
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX
        "/(ModernVulkan[^/]*Smoke|Portable[^/]*Smoke|LoadingCloseSmoke|AttachmentLifetimeSmoke)\\.cpp$")
    list(FILTER _PVZ_GAME_SOURCES EXCLUDE REGEX "\\.(rc|cur)$")
    set_property(TARGET ${PROJECT_NAME} PROPERTY SOURCES "${_PVZ_GAME_SOURCES}")
    target_sources(${PROJECT_NAME} PRIVATE
        "platform/linux/LinuxApp.cpp"
        "platform/linux/WindowGeometry.cpp"
        "platform/linux/CalendarTime.cpp"
        "platform/linux/UserDirectories.cpp"
        "platform/linux/TabletCaret.cpp")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        SEXY_RENDER_GPU=1
        "$<$<NOT:$<CONFIG:Release>>:SEXY_DISABLE_MODVAL=1>")
    if(PVZ_WINDOW_BACKEND STREQUAL "WAYLAND")
        target_compile_definitions(${PROJECT_NAME} PRIVATE SEXY_WINDOW_WAYLAND=1)
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
        BUILD_RPATH_USE_ORIGIN TRUE)
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_native_window ${CMAKE_DL_LIBS})
endif()

if(PVZ_WINDOW_BACKEND STREQUAL "WIN32")
    target_sources(${PROJECT_NAME} PRIVATE
        "platform/windows/Win32App.cpp"
        "platform/windows/WindowGeometry.cpp"
        "platform/windows/CalendarTime.cpp"
        "platform/windows/UserDirectories.cpp"
        "platform/windows/TabletCaret.cpp")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SEXY_WINDOW_WIN32=1 SEXY_RENDER_GPU=1)
    if(BUILD_TESTING)
        add_executable(modern_message_readiness_tests "${CMAKE_SOURCE_DIR}/tools/native-tests/win32_message_readiness_tests.cpp")
        target_compile_features(modern_message_readiness_tests PRIVATE cxx_std_20)
        target_link_libraries(modern_message_readiness_tests PRIVATE user32)
        add_test(NAME modern_message_readiness COMMAND modern_message_readiness_tests)
        set_tests_properties(modern_message_readiness PROPERTIES TIMEOUT 15)
        if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
            get_filename_component(_PVZ_MESSAGE_MSVC_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
            if(EXISTS "${_PVZ_MESSAGE_MSVC_BIN}/clang_rt.asan_dynamic-x86_64.dll")
                add_executable(modern_message_readiness_asan_tests "${CMAKE_SOURCE_DIR}/tools/native-tests/win32_message_readiness_tests.cpp")
                target_compile_features(modern_message_readiness_asan_tests PRIVATE cxx_std_20)
                target_compile_options(modern_message_readiness_asan_tests PRIVATE /fsanitize=address /Zi)
                target_link_options(modern_message_readiness_asan_tests PRIVATE /INCREMENTAL:NO /DEBUG)
                target_link_libraries(modern_message_readiness_asan_tests PRIVATE user32)
                set_target_properties(modern_message_readiness_asan_tests PROPERTIES
                    INTERPROCEDURAL_OPTIMIZATION FALSE INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
                add_custom_command(TARGET modern_message_readiness_asan_tests POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${_PVZ_MESSAGE_MSVC_BIN}/clang_rt.asan_dynamic-x86_64.dll"
                        "$<TARGET_FILE_DIR:modern_message_readiness_asan_tests>" VERBATIM)
                add_test(NAME modern_message_readiness_asan COMMAND modern_message_readiness_asan_tests)
                set_tests_properties(modern_message_readiness_asan PROPERTIES TIMEOUT 15)
            endif()
        endif()
    endif()
endif()

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE SexyAppBase)

################################################################################
# Target name
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG            "pvz_perf"
    TARGET_NAME_RELEASE          "pvz_perf"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_DEBUG            "${CMAKE_BINARY_DIR}/Debug"
    OUTPUT_DIRECTORY_RELEASE          "${CMAKE_BINARY_DIR}/Release"
)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/tools/start-lp14.cmd"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/start-lp14.cmd"
        VERBATIM)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file(GENERATE OUTPUT "$<TARGET_FILE_DIR:${PROJECT_NAME}>/start.cmd"
        INPUT "${CMAKE_SOURCE_DIR}/CMake/StartWslGame.cmd"
        NEWLINE_STYLE CRLF)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/assets/branding/pvz_perf.desktop"
            "${CMAKE_SOURCE_DIR}/assets/branding/pvz_perf.png"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        VERBATIM)
    include(GNUInstallDirs)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/branding/pvz_perf.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT branding)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/branding/pvz_perf.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        COMPONENT branding)
endif()

################################################################################
# Compile definitions
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:"
        "_DEBUG;"
        #"_WINDOWS;"
        #"_VISUALC_;"
        #"_JPEGLIB_;"
        #"_CRT_SECURE_NO_WARNINGS"
    ">"
    "$<$<CONFIG:Release>:"
        "NDEBUG;"
        "RELEASEFINAL;"
        #"_WINDOWS;"
        #"_VISUALC_;"
        #"_JPEGLIB_"
    ">"
    "_LIB;"
    "_MBCS"
)

################################################################################
# Dependencies
################################################################################
# settings.json / scale.json 解析（JsonDoc 是 nlohmann 唯一展开 TU，见 JsonDoc.h 头注）
find_package(nlohmann_json CONFIG REQUIRED)

# Native backend source is part of this framework. The game call-site migration is
# tracked in MODERN_STATUS.md; linking this target alone does NOT switch renderers.
if(PVZ_RENDER_BACKEND STREQUAL "VULKAN")
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_vulkan)
    add_dependencies(${PROJECT_NAME} sexy_vulkan_shaders)
    set(gameShaderDirectory "${CMAKE_BINARY_DIR}/SexyAppFramework/graphics/vulkan/shaders/$<CONFIG>")
elseif(PVZ_RENDER_BACKEND STREQUAL "D3D12")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PVZ_GPU_D3D12=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_d3d12)
    add_dependencies(${PROJECT_NAME} sexy_d3d12_shaders)
    set(gameShaderDirectory "${CMAKE_BINARY_DIR}/SexyAppFramework/graphics/d3d12/shaders")
elseif(PVZ_RENDER_BACKEND STREQUAL "OPENGL")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PVZ_GPU_OPENGL=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_opengl)
elseif(PVZ_RENDER_BACKEND STREQUAL "D3D9")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PVZ_GPU_D3D9=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_d3d9)
    add_dependencies(${PROJECT_NAME} sexy_d3d9_shaders)
    set(gameShaderDirectory "${CMAKE_BINARY_DIR}/SexyAppFramework/graphics/d3d9/shaders")
elseif(PVZ_RENDER_BACKEND STREQUAL "METAL")
    if(NOT TARGET sexy_metal)
        message(FATAL_ERROR "Native Metal requires the real sexy_metal backend target")
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_metal)
    set(gameShaderDirectory "${CMAKE_BINARY_DIR}/SexyAppFramework/graphics/metal/shaders")
endif()
if(APPLE)
    pvz_stage_macos_shader_compiler(${PROJECT_NAME})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${gameShaderDirectory}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/default-scale.json"
            "-DDESTINATION=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            -P "${CMAKE_SOURCE_DIR}/CMake/StageScaleConfig.cmake"
        VERBATIM)
elseif(PVZ_RENDER_BACKEND STREQUAL "OPENGL")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/default-scale.json"
            "-DDESTINATION=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            -P "${CMAKE_SOURCE_DIR}/CMake/StageScaleConfig.cmake"
        VERBATIM)
elseif(PVZ_RENDER_BACKEND STREQUAL "D3D9")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${gameShaderDirectory}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/graphics/opengl/default-scale.json"
            "-DDESTINATION=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            -P "${CMAKE_SOURCE_DIR}/CMake/StageScaleConfig.cmake"
        VERBATIM)
elseif(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${gameShaderDirectory}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        VERBATIM)
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${gameShaderDirectory}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE=${CMAKE_SOURCE_DIR}/effects"
            "-DDESTINATION=$<TARGET_FILE_DIR:${PROJECT_NAME}>/effects"
            -P "${CMAKE_SOURCE_DIR}/CMake/StageNumericEffects.cmake"
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE=${CMAKE_SOURCE_DIR}/scale.json"
            "-DDESTINATION=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            -P "${CMAKE_SOURCE_DIR}/CMake/StageScaleConfig.cmake"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Microsoft::DirectXShaderCompiler>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        VERBATIM)
if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE_DIR:Microsoft::DirectXShaderCompiler>/${CMAKE_SHARED_LIBRARY_PREFIX}dxil${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>" VERBATIM)
endif()
endif()

set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "$<$<CONFIG:Debug>:"
        "winmm;"
        #"ddraw;"
    ">"
    "$<$<CONFIG:Release>:"
        "winmm;"
	    #"ddraw;"
	#"ucrt;"
    ">"
)

target_include_directories(${PROJECT_NAME} PRIVATE "." "../Sexy.TodLib")
get_property(pvzFrameworkTargets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(pvzTarget IN LISTS pvzFrameworkTargets)
    if(pvzTarget MATCHES "^modern_.*tests$")
        target_link_libraries(${pvzTarget} PRIVATE pvz_source_headers)
    endif()
endforeach()
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Brepro)
    target_link_options(${PROJECT_NAME} PRIVATE /Brepro)
    # Release builds carry full PDB symbols so crash addresses stay
    # resolvable. /Brepro + /DEBUG keeps builds reproducible while still
    # emitting the symbol file.
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG>)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")
    # The game owns its Win32 window and borrowed-HWND Vulkan context. Historical external-host
    # research targets must not be linked merely because they are configured.
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE -static -static-libgcc -static-libstdc++ "${ADDITIONAL_LIBRARY_DEPENDENCIES}")
elseif(ANDROID)
    # 16KB 内存页设备（Android 15+ 旗舰）要求 LOAD 段 16KB 对齐才能加载。
    target_link_options(${PROJECT_NAME} PRIVATE
        "-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")
endif()

if(WIN32)
    set(nativeBassLibrary "${PVZ_BASS_DLL}")
else()
    set(nativeBassLibrary "${PVZ_BASS_LIBRARY}")
endif()
if(nativeBassLibrary)
    get_filename_component(nativeBassLibraryAbsolute "${nativeBassLibrary}" ABSOLUTE)
    if(NOT EXISTS "${nativeBassLibraryAbsolute}")
        message(FATAL_ERROR "Native BASS library does not exist: ${nativeBassLibraryAbsolute}")
    endif()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${nativeBassLibraryAbsolute}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${CMAKE_SHARED_LIBRARY_PREFIX}bass${CMAKE_SHARED_LIBRARY_SUFFIX}"
        COMMENT "Staging the configured native BASS library"
        VERBATIM
    )
endif()

if(TARGET pvz_dat64_converter)
    add_custom_target(pvz_save_converter_deploy
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_BINARY_DIR}/$<CONFIG>/data"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:pvz_dat64_converter>"
            "${CMAKE_BINARY_DIR}/$<CONFIG>/data/$<TARGET_FILE_NAME:pvz_dat64_converter>"
        COMMAND "${CMAKE_COMMAND}" -E rm -f
            "${CMAKE_BINARY_DIR}/$<CONFIG>/data/msvcp140.dll"
            "${CMAKE_BINARY_DIR}/$<CONFIG>/data/vcruntime140.dll"
            "${CMAKE_BINARY_DIR}/$<CONFIG>/data/vcruntime140_1.dll"
        DEPENDS pvz_dat64_converter
        COMMENT "Deploying the native dat/dat64 converter to data/"
        VERBATIM
    )
    add_dependencies(${PROJECT_NAME} pvz_save_converter_deploy)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "pvz_perf"
    OUTPUT_NAME_RELEASE "pvz_perf"
    OUTPUT_NAME_DEBUG "pvz_perf"
    RUNTIME_OUTPUT_NAME_RELEASE "pvz_perf"
    RUNTIME_OUTPUT_NAME_DEBUG "pvz_perf")
# Scaling source is compiled during configuration loading, never during drawing.
if(PVZ_RENDER_BACKEND STREQUAL "D3D12")
    target_link_libraries(${PROJECT_NAME} PRIVATE sexy_d3d12_effects)
elseif(PVZ_RENDER_BACKEND STREQUAL "METAL")
    if(TARGET sexy_metal_effects)
        target_link_libraries(${PROJECT_NAME} PRIVATE sexy_metal_effects)
    endif()
elseif(NOT PVZ_RENDER_BACKEND MATCHES "^(OPENGL|D3D9)$")
    if(TARGET sexy_vulkan_effects)
        target_link_libraries(${PROJECT_NAME} PRIVATE sexy_vulkan_effects)
    endif()
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# A regenerated resource contract must invalidate consumers in the same build,
# before a new PAK64 is paired with the executable. Use the producer's exact
# path spelling rather than relying solely on compiler-discovered aliases.
get_target_property(_PVZ_CONTRACT_CONSUMERS ${PROJECT_NAME} SOURCES)
foreach(_PVZ_CONSUMER IN LISTS _PVZ_CONTRACT_CONSUMERS)
    if(_PVZ_CONSUMER MATCHES "\\.cpp$")
        get_filename_component(_PVZ_CONSUMER_ABSOLUTE "${_PVZ_CONSUMER}"
            ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        set_property(SOURCE "${_PVZ_CONSUMER_ABSOLUTE}" APPEND PROPERTY
            OBJECT_DEPENDS "${PVZ_RESOURCE_CONTRACT_HEADERS}")
    endif()
endforeach()
