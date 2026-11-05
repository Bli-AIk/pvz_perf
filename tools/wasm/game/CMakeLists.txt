cmake_minimum_required(VERSION 3.25)
project(PvzPerfWeb LANGUAGES C CXX)
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Use emcmake to configure the browser target")
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(GAME "${ROOT}/pvzPerf")
set(FRAMEWORK "${GAME}/SexyAppFramework")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PVZ_CONFIG_TARGET_SYSTEM Web)
set(PVZ_CONFIG_WINDOW_SYSTEM Browser)
set(PVZ_CONFIG_GRAPHICS_API WebGL)
set(PVZ_CONFIG_WINDOWS_API NotApplicable)
configure_file("${ROOT}/CMake/BuildConfig.h.in" "${CMAKE_BINARY_DIR}/include/pvz/BuildConfig.h" @ONLY)
set(PVZ_WEB_PAK "${ROOT}/out/x64-win32-vulkan/Release/main.pak64" CACHE FILEPATH "Prebuilt resource package")
set(PVZ_SPNG_SOURCE "C:/UserProgram/vcpkg/buildtrees/libspng/src/v0.7.4-e8b3878a48.clean/spng" CACHE PATH "libspng source directory")
set(PVZ_JSON_INCLUDE "${ROOT}/out/vcpkg_installed/x64-windows-static-md/include" CACHE PATH "nlohmann json include root")
if(NOT EXISTS "${PVZ_JSON_INCLUDE}/nlohmann/json.hpp")
    set(PVZ_JSON_INCLUDE "C:/UserProgram/vcpkg/installed/x64-windows-static-md/include")
endif()
if(NOT EXISTS "${PVZ_SPNG_SOURCE}/spng.c" OR NOT EXISTS "${PVZ_JSON_INCLUDE}/nlohmann/json.hpp")
    message(FATAL_ERROR "Set PVZ_SPNG_SOURCE and PVZ_JSON_INCLUDE to the source/header dependencies")
endif()
file(GLOB SOURCES CONFIGURE_DEPENDS
    "${GAME}/*.cpp" "${GAME}/Lawn/*.cpp" "${GAME}/Lawn/System/*.cpp" "${GAME}/Lawn/Widget/*.cpp"
    "${GAME}/Sexy.TodLib/*.cpp" "${GAME}/Sexy.TodLib/generated/*.cpp" "${GAME}/Sexy.TodLib/generated/definitions/*.cpp"
    "${FRAMEWORK}/*.cpp" "${FRAMEWORK}/graphics/*.cpp" "${FRAMEWORK}/misc/*.cpp" "${FRAMEWORK}/widget/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "/(main|TodDrawTriangle[^/]*|ModVal|Win32CrashDump|ModernVulkan[^/]*Smoke|Portable[^/]*Smoke|LoadingCloseSmoke|AttachmentLifetimeSmoke)\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "/FloatingPointMode\\.cpp$")
file(GLOB WEB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/web/*.cpp" "${FRAMEWORK}/platform/web/*.cpp" "${FRAMEWORK}/graphics/webgl/*.cpp")
add_executable(pvz-perf ${SOURCES} ${WEB_SOURCES}
    "${FRAMEWORK}/imagelib/ImageLib.cpp" "${FRAMEWORK}/paklib/PakInterface.cpp"
    "${FRAMEWORK}/effect/ScaleEffectSizing.cpp"
    "${FRAMEWORK}/platform/linux/LinuxApp.cpp" "${FRAMEWORK}/platform/linux/WindowGeometry.cpp"
    "${FRAMEWORK}/platform/linux/CalendarTime.cpp"
    "${FRAMEWORK}/graphics/gl/GLRenderer.cpp"
    "${FRAMEWORK}/graphics/gl/GLShaders.cpp"
    "${FRAMEWORK}/graphics/gl/GLImageOps.cpp"
    "${FRAMEWORK}/sound/BassMusicInterface.cpp"
    "${PVZ_SPNG_SOURCE}/spng.c")
target_include_directories(pvz-perf PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_BINARY_DIR}/include"
    "${ROOT}" "${GAME}" "${FRAMEWORK}" "${GAME}/Sexy.TodLib" "${FRAMEWORK}/graphics"
    "${PVZ_SPNG_SOURCE}")
# Do not expose all host-vcpkg headers (zlib/jpeg have a different target ABI).
file(COPY "${PVZ_JSON_INCLUDE}/nlohmann" DESTINATION "${CMAKE_BINARY_DIR}/include")
target_compile_definitions(pvz-perf PRIVATE NDEBUG PVZ_GPU_WEBGL=1 SEXY_WINDOW_BROWSER=1 SEXY_RENDER_GPU=1 SPNG_STATIC SEXY_DISABLE_MODVAL=1 AL_APIENTRY=)
target_compile_options(pvz-perf PRIVATE -Oz -flto -fexceptions -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBJPEG=1
    -Wno-inconsistent-missing-override -Wno-invalid-source-encoding -Wno-invalid-offsetof)
target_link_options(pvz-perf PRIVATE -Oz -flto -fexceptions -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBJPEG=1
    --emit-symbol-map
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728
    -sSTACK_SIZE=4194304 -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=131072 -sEXIT_RUNTIME=0
    "-sEXPORTED_RUNTIME_METHODS=['FS','IDBFS','callMain','addRunDependency','removeRunDependency']" -sINVOKE_RUN=0 -lidbfs.js
    "--preload-file=${PVZ_WEB_PAK}@/main.pak64")
set_target_properties(pvz-perf PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/site")
