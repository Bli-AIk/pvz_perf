cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SexyFrameworkVulkan LANGUAGES CXX)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    include(CTest)
endif()

find_package(Vulkan 1.3 REQUIRED)
find_program(PVZ_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin"
    "C:/UserProgram/VulkanSDK/1.4.350.0/Bin" REQUIRED)
find_program(PVZ_SPIRV_VAL NAMES spirv-val HINTS "$ENV{VULKAN_SDK}/Bin"
    "C:/UserProgram/VulkanSDK/1.4.350.0/Bin" REQUIRED)

option(PVZ_VULKAN_SEPARATE_BLEND_TEST "Android-only single-output premultiplied sprite blend test" OFF)
if(PVZ_VULKAN_SEPARATE_BLEND_TEST AND NOT ANDROID)
    message(FATAL_ERROR "PVZ_VULKAN_SEPARATE_BLEND_TEST requires Android")
endif()
add_library(sexy_vulkan STATIC VulkanContext.cpp VulkanRenderer.cpp ../PoolMesh.cpp)
if(PVZ_VULKAN_LEGACY)
    target_sources(sexy_vulkan PRIVATE VulkanLegacy.cpp)
    set(PVZ_NATIVE_VULKAN_ENV vulkan1.0)
    set(PVZ_NATIVE_VULKAN_DEFINES -DPVZ_VULKAN_LEGACY=1)
else()
    set(PVZ_NATIVE_VULKAN_ENV vulkan1.3)
endif()
if(PVZ_ANDROID_HDR_TEST)
    list(APPEND PVZ_NATIVE_VULKAN_DEFINES -DPVZ_ANDROID_HDR_TEST=1)
endif()
if(ANDROID AND PVZ_VULKAN_SEPARATE_BLEND_TEST)
    target_compile_definitions(sexy_vulkan PRIVATE PVZ_VULKAN_SEPARATE_BLEND_TEST=1)
endif()
if(NOT WIN32 OR NOT PVZ_WINDOWS_PLATFORM STREQUAL "WIN7")
    target_sources(sexy_vulkan PRIVATE VulkanHdrDisplay.cpp VulkanHdrObserver.cpp)
endif()
if(WIN32)
    target_sources(sexy_vulkan PRIVATE platform/Win32Surface.cpp)
elseif(ANDROID)
    target_compile_definitions(sexy_vulkan PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
    target_link_libraries(sexy_vulkan PRIVATE android log)
else()
    target_sources(sexy_vulkan PRIVATE platform/LinuxSurface.cpp)
    target_link_libraries(sexy_vulkan PUBLIC sexy_native_window)
    if(PVZ_WINDOW_BACKEND STREQUAL "X11")
        find_package(X11 REQUIRED)
        set_property(SOURCE platform/LinuxSurface.cpp APPEND PROPERTY
            COMPILE_DEFINITIONS VK_USE_PLATFORM_XLIB_KHR)
        target_link_libraries(sexy_vulkan PRIVATE X11::X11)
    elseif(PVZ_WINDOW_BACKEND STREQUAL "WAYLAND")
        set_property(SOURCE platform/LinuxSurface.cpp APPEND PROPERTY
            COMPILE_DEFINITIONS VK_USE_PLATFORM_WAYLAND_KHR)
    endif()
endif()
target_include_directories(sexy_vulkan PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../../Sexy.TodLib")
target_compile_features(sexy_vulkan PUBLIC cxx_std_23)
target_link_libraries(sexy_vulkan PUBLIC Vulkan::Vulkan)
if(WIN32)
    target_link_libraries(sexy_vulkan PRIVATE user32 ole32)
    if(NOT PVZ_WINDOWS_PLATFORM STREQUAL "WIN7")
        target_link_libraries(sexy_vulkan PRIVATE runtimeobject)
    endif()
endif()
if(MSVC)
    target_compile_options(sexy_vulkan PRIVATE /W4 /utf-8 /permissive-)
elseif(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_options(sexy_vulkan PRIVATE -Wno-missing-field-initializers)
endif()

set(PVZ_NATIVE_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>")
foreach(stage IN ITEMS vs pool_vs ps sprite_ps sprite_mixed_ps hdr_linear_ps coverage_ps caustic_cs font_cs alpha_cs color_cs)
    if(stage STREQUAL "vs" OR stage STREQUAL "pool_vs")
        set(shaderStage vertex)
        set(shaderSource "${CMAKE_CURRENT_SOURCE_DIR}/shaders/game_${stage}.hlsl")
        set(shaderLanguage -x hlsl)
    elseif(stage STREQUAL "ps" OR stage STREQUAL "sprite_ps" OR stage STREQUAL "sprite_mixed_ps" OR stage STREQUAL "coverage_ps")
        # GLSL：bindless 数组采样需要 nonuniformEXT 装饰，glslc 的 HLSL 前端
        # 没有 NonUniformResourceIndex 内建
        set(shaderStage fragment)
        set(shaderSource "${CMAKE_CURRENT_SOURCE_DIR}/shaders/game_${stage}.frag")
        set(shaderLanguage "")
    elseif(stage STREQUAL "caustic_cs" OR stage STREQUAL "font_cs" OR stage STREQUAL "alpha_cs" OR stage STREQUAL "color_cs")
        # GLSL：HLSL 前端无法标注 storage image 格式（需 rgba8 匹配视图）
        set(shaderStage compute)
        set(shaderSource "${CMAKE_CURRENT_SOURCE_DIR}/shaders/game_${stage}.comp")
        set(shaderLanguage "")
    else()
        # hdr_ps 保持 HLSL：它只服务呈现单四边形（槽位跨调用动态一致，
        # 无需非一致装饰），且被 hdr_probe_cs.hlsl 以 HLSL include 复用
        set(shaderStage fragment)
        set(shaderSource "${CMAKE_CURRENT_SOURCE_DIR}/shaders/game_${stage}.hlsl")
        set(shaderLanguage -x hlsl)
    endif()
    set(shaderDependencies "${shaderSource}")
    if(shaderLanguage STREQUAL "")
        list(APPEND shaderDependencies "${CMAKE_CURRENT_SOURCE_DIR}/shaders/texture_bindings.glsl")
    endif()
    if(stage STREQUAL "hdr_linear_ps")
        list(APPEND shaderDependencies "${CMAKE_CURRENT_SOURCE_DIR}/shaders/game_hdr_ps.hlsl")
    endif()
    set(output "${PVZ_NATIVE_SHADER_DIR}/game_${stage}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PVZ_NATIVE_SHADER_DIR}"
        COMMAND "${PVZ_GLSLC}" -Werror ${shaderLanguage} "-fshader-stage=${shaderStage}"
            --target-env=${PVZ_NATIVE_VULKAN_ENV} ${PVZ_NATIVE_VULKAN_DEFINES} -O "${shaderSource}"
            -o "${output}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env ${PVZ_NATIVE_VULKAN_ENV} "${output}"
        DEPENDS ${shaderDependencies} VERBATIM)
    list(APPEND nativeShaders "${output}")
endforeach()
if(ANDROID AND PVZ_VULKAN_SEPARATE_BLEND_TEST)
    set(singleOutputShader "${PVZ_NATIVE_SHADER_DIR}/game_sprite_single_output_ps.spv")
    add_custom_command(OUTPUT "${singleOutputShader}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PVZ_NATIVE_SHADER_DIR}"
        COMMAND "${PVZ_GLSLC}" -Werror -fshader-stage=fragment --target-env=vulkan1.3 -O
            "${CMAKE_SOURCE_DIR}/Tests/performance/single_output_premult_experiment.frag" -o "${singleOutputShader}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${singleOutputShader}"
        DEPENDS "${CMAKE_SOURCE_DIR}/Tests/performance/single_output_premult_experiment.frag" VERBATIM)
    list(APPEND nativeShaders "${singleOutputShader}")
endif()
add_custom_target(sexy_vulkan_shaders DEPENDS ${nativeShaders})
add_dependencies(sexy_vulkan sexy_vulkan_shaders)

# User-authored scaling effects compile once while loading configuration.
# Android ships precompiled SPIR-V and has no DXC host dependency.
if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
find_package(directx-dxc CONFIG REQUIRED GLOBAL)
endif()
add_library(sexy_vulkan_effects STATIC
    VulkanEffectPrograms.cpp
    VulkanScaleDds.cpp
    VulkanScalePipeline.cpp
    ../../effect/ScaleEffectCompiler.cpp
    ../../effect/ScaleEffectSizing.cpp)
target_include_directories(sexy_vulkan_effects PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../effect")
target_compile_features(sexy_vulkan_effects PUBLIC cxx_std_23)
if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(sexy_vulkan_effects PUBLIC SEXY_SCALE_EFFECT_AUTHORING=1)
    target_link_libraries(sexy_vulkan_effects PUBLIC Vulkan::Vulkan PRIVATE Microsoft::DirectXShaderCompiler)
else()
    target_link_libraries(sexy_vulkan_effects PUBLIC Vulkan::Vulkan)
endif()
if(MSVC)
    target_compile_options(sexy_vulkan_effects PRIVATE /W4 /utf-8 /permissive-)
elseif(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_options(sexy_vulkan_effects PRIVATE -Wno-missing-field-initializers)
endif()

# Authoring parsers/DXC are available only in the explicitly offline asset tool.
if(NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
add_executable(compile_numeric_scale EXCLUDE_FROM_ALL
    "${CMAKE_SOURCE_DIR}/tools/compile-numeric-scale.cpp"
    ../../effect/ScaleEffectCompiler.cpp)
target_compile_definitions(compile_numeric_scale PRIVATE SEXY_SCALE_EFFECT_AUTHORING=1)
target_compile_features(compile_numeric_scale PRIVATE cxx_std_23)
target_include_directories(compile_numeric_scale PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../effect")
target_link_libraries(compile_numeric_scale PRIVATE Vulkan::Vulkan Microsoft::DirectXShaderCompiler)
add_custom_command(TARGET compile_numeric_scale POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Microsoft::DirectXShaderCompiler>" "$<TARGET_FILE_DIR:compile_numeric_scale>")
if(MSVC)
    target_compile_options(compile_numeric_scale PRIVATE /W4 /utf-8 /permissive-)
endif()

set(PVZ_EFFECTS_ROOT "${CMAKE_SOURCE_DIR}/effects")
file(GLOB_RECURSE scaleSources CONFIGURE_DEPENDS "${PVZ_EFFECTS_ROOT}/*.hlsl")
file(GLOB_RECURSE scaleIncludes CONFIGURE_DEPENDS "${PVZ_EFFECTS_ROOT}/*.hlsli")
foreach(source IN LISTS scaleSources)
    list(APPEND numericEffects "${source}.vfx")
endforeach()
if(numericEffects)
    add_custom_command(OUTPUT ${numericEffects}
        COMMAND "$<TARGET_FILE:compile_numeric_scale>" --effects "${PVZ_EFFECTS_ROOT}"
        DEPENDS compile_numeric_scale ${scaleSources} ${scaleIncludes}
        COMMENT "Baking numeric scale assets offline"
        VERBATIM)
else()
    message(STATUS "No external scaling effects found; skipping offline effect baking")
endif()
add_custom_target(bake_numeric_scale DEPENDS ${numericEffects})
endif()

target_link_libraries(sexy_vulkan PRIVATE sexy_vulkan_effects)
set_target_properties(sexy_vulkan sexy_vulkan_effects PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

if(BUILD_TESTING AND NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(PVZ_TEST_EFFECTS_DIR "${CMAKE_SOURCE_DIR}/effects" CACHE PATH
        "External scaling effect fixtures for Vulkan tests")
    add_executable(sexy_vulkan_enumeration_tests tests/enumeration_tests.cpp)
    target_compile_features(sexy_vulkan_enumeration_tests PRIVATE cxx_std_23)
    target_include_directories(sexy_vulkan_enumeration_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(sexy_vulkan_enumeration_tests PRIVATE Vulkan::Headers)
    add_test(NAME sexy_vulkan_enumeration COMMAND sexy_vulkan_enumeration_tests)
    set_tests_properties(sexy_vulkan_enumeration PROPERTIES TIMEOUT 10)
    add_executable(sexy_vulkan_scale_compile_tests tests/scale_compile_tests.cpp)
    target_link_libraries(sexy_vulkan_scale_compile_tests PRIVATE sexy_vulkan_effects)
    target_compile_definitions(sexy_vulkan_scale_compile_tests PRIVATE
        PVZ_EFFECTS_DIR="${PVZ_TEST_EFFECTS_DIR}")
    add_custom_command(TARGET sexy_vulkan_scale_compile_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Microsoft::DirectXShaderCompiler>"
            "$<TARGET_FILE_DIR:sexy_vulkan_scale_compile_tests>")
    add_test(NAME sexy_vulkan_scale_compile COMMAND sexy_vulkan_scale_compile_tests)
    set_tests_properties(sexy_vulkan_scale_compile PROPERTIES TIMEOUT 180)
endif()

if(BUILD_TESTING AND WIN32)
    add_library(sexy_scale_authoring ALIAS sexy_vulkan_effects)
    if(WIN32 AND NOT PVZ_WINDOWS_PLATFORM STREQUAL "WIN7")
        add_executable(sexy_vulkan_hdr_observer_tests tests/hdr_observer_tests.cpp)
        target_link_libraries(sexy_vulkan_hdr_observer_tests PRIVATE sexy_vulkan ole32)
        add_test(NAME sexy_vulkan_hdr_observer COMMAND sexy_vulkan_hdr_observer_tests)
        set_tests_properties(sexy_vulkan_hdr_observer PROPERTIES TIMEOUT 60 ENVIRONMENT "VK_LAYER_VALIDATE_SYNC=1")
    endif()
    add_executable(sexy_vulkan_initialization_tests tests/initialization_tests.cpp)
    target_link_libraries(sexy_vulkan_initialization_tests PRIVATE sexy_vulkan)
    add_test(NAME sexy_vulkan_initialization COMMAND sexy_vulkan_initialization_tests)
    set_tests_properties(sexy_vulkan_initialization PROPERTIES TIMEOUT 60 ENVIRONMENT "VK_LAYER_VALIDATE_SYNC=1")
    # Opt-in interception evidence only; observations/errors must not be hidden
    # inside an automatically passing test. Includes just four clear/presents.
    add_executable(sexy_vulkan_layer_probe EXCLUDE_FROM_ALL tests/layer_probe.cpp)
    target_link_libraries(sexy_vulkan_layer_probe PRIVATE sexy_vulkan)
    set(hdrProbe "${PVZ_NATIVE_SHADER_DIR}/hdr_probe.spv")
    add_custom_command(OUTPUT "${hdrProbe}"
        COMMAND "${PVZ_GLSLC}" -Werror -x hlsl -fshader-stage=compute --target-env=vulkan1.3 -O
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/hdr_probe_cs.hlsl" -o "${hdrProbe}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${hdrProbe}"
        DEPENDS tests/hdr_probe_cs.hlsl shaders/game_hdr_ps.hlsl VERBATIM)
    add_custom_target(sexy_vulkan_hdr_probe DEPENDS "${hdrProbe}")
    set(hdrLinearProbe "${PVZ_NATIVE_SHADER_DIR}/hdr_linear_probe.spv")
    add_custom_command(OUTPUT "${hdrLinearProbe}"
        COMMAND "${PVZ_GLSLC}" -Werror -x hlsl -DPVZ_HDR_LINEAR_OUTPUT -fshader-stage=compute --target-env=vulkan1.3 -O
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/hdr_probe_cs.hlsl" -o "${hdrLinearProbe}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${hdrLinearProbe}"
        DEPENDS tests/hdr_probe_cs.hlsl shaders/game_hdr_ps.hlsl VERBATIM)
    add_custom_target(sexy_vulkan_hdr_linear_probe DEPENDS "${hdrLinearProbe}")
    add_dependencies(sexy_vulkan_hdr_linear_probe sexy_vulkan_shaders)
    add_dependencies(sexy_vulkan_hdr_probe sexy_vulkan_shaders)
    if(NOT WIN32 OR NOT PVZ_WINDOWS_PLATFORM STREQUAL "WIN7")
        add_executable(sexy_vulkan_hdr_tests tests/hdr_tests.cpp tests/hdr_reference.cpp)
        target_link_libraries(sexy_vulkan_hdr_tests PRIVATE sexy_vulkan)
        target_compile_definitions(sexy_vulkan_hdr_tests PRIVATE PVZ_NATIVE_SHADER_DIR="${PVZ_NATIVE_SHADER_DIR}")
        add_dependencies(sexy_vulkan_hdr_tests sexy_vulkan_hdr_probe)
        add_dependencies(sexy_vulkan_hdr_tests sexy_vulkan_hdr_linear_probe)
        add_test(NAME sexy_vulkan_hdr COMMAND sexy_vulkan_hdr_tests)
        set_tests_properties(sexy_vulkan_hdr PROPERTIES TIMEOUT 60 ENVIRONMENT "VK_LAYER_VALIDATE_SYNC=1")
    endif()
    add_executable(sexy_vulkan_scale_pipeline_tests tests/scale_pipeline_tests.cpp)
    target_link_libraries(sexy_vulkan_scale_pipeline_tests PRIVATE sexy_vulkan_effects sexy_vulkan)
    target_compile_definitions(sexy_vulkan_scale_pipeline_tests PRIVATE
        PVZ_NATIVE_SHADER_DIR="${PVZ_NATIVE_SHADER_DIR}"
        PVZ_EFFECTS_DIR="${PVZ_TEST_EFFECTS_DIR}")
    add_custom_command(TARGET sexy_vulkan_scale_pipeline_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Microsoft::DirectXShaderCompiler>" "$<TARGET_FILE_DIR:sexy_vulkan_scale_pipeline_tests>")
    add_test(NAME sexy_vulkan_scale_pipeline COMMAND sexy_vulkan_scale_pipeline_tests)
    set_tests_properties(sexy_vulkan_scale_pipeline PROPERTIES TIMEOUT 180 ENVIRONMENT "VK_LAYER_VALIDATE_SYNC=1")
    add_executable(sexy_vulkan_scale_gpu_tests tests/scale_gpu_tests.cpp)
    target_link_libraries(sexy_vulkan_scale_gpu_tests PRIVATE sexy_vulkan_effects sexy_vulkan)
    target_compile_definitions(sexy_vulkan_scale_gpu_tests PRIVATE
        PVZ_EFFECTS_DIR="${PVZ_TEST_EFFECTS_DIR}")
    add_custom_command(TARGET sexy_vulkan_scale_gpu_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Microsoft::DirectXShaderCompiler>" "$<TARGET_FILE_DIR:sexy_vulkan_scale_gpu_tests>")
    add_test(NAME sexy_vulkan_scale_gpu COMMAND sexy_vulkan_scale_gpu_tests)
    set_tests_properties(sexy_vulkan_scale_gpu PROPERTIES TIMEOUT 180 ENVIRONMENT "VK_LAYER_VALIDATE_SYNC=1")
    if(WIN32)
        add_executable(sexy_vulkan_compare_tests EXCLUDE_FROM_ALL
            "${CMAKE_SOURCE_DIR}/Tests/native/BackendPixelCases.cpp")
        target_link_libraries(sexy_vulkan_compare_tests PRIVATE sexy_vulkan)
    endif()
    add_executable(sexy_vulkan_render_tests tests/render_tests.cpp)
    target_link_libraries(sexy_vulkan_render_tests PRIVATE sexy_vulkan)
    target_compile_definitions(sexy_vulkan_render_tests PRIVATE
        PVZ_NATIVE_SHADER_DIR="${PVZ_NATIVE_SHADER_DIR}")
    add_test(NAME sexy_vulkan_render_semantics COMMAND sexy_vulkan_render_tests)
    set_tests_properties(sexy_vulkan_render_semantics PROPERTIES
        TIMEOUT 60 ENVIRONMENT "VK_LAYER_VALIDATE_SYNC=1")
endif()

# Explicit desktop GPU experiment. Production and Android never compile these hooks.
if(BUILD_TESTING AND WIN32 AND NOT PVZ_WINDOWS_PLATFORM STREQUAL "WIN7" AND NOT PVZ_VULKAN_LEGACY)
    set(spriteExperiment "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>/generated_sprite.spv")
    add_custom_command(OUTPUT "${spriteExperiment}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>"
        COMMAND "${PVZ_GLSLC}" -Werror -fshader-stage=fragment --target-env=vulkan1.3 -O
            "${CMAKE_SOURCE_DIR}/Tests/performance/generated_sprite_experiment.frag" -o "${spriteExperiment}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${spriteExperiment}"
        DEPENDS "${CMAKE_SOURCE_DIR}/Tests/performance/generated_sprite_experiment.frag" VERBATIM)
    add_custom_target(sexy_sprite_experiment_shader DEPENDS "${spriteExperiment}")
    set(filterExperiment "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>/baked_filter.spv")
    add_custom_command(OUTPUT "${filterExperiment}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>"
        COMMAND "${PVZ_GLSLC}" -Werror -fshader-stage=fragment --target-env=vulkan1.3 -O
            "${CMAKE_SOURCE_DIR}/Tests/performance/baked_filter_experiment.frag" -o "${filterExperiment}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${filterExperiment}"
        DEPENDS "${CMAKE_SOURCE_DIR}/Tests/performance/baked_filter_experiment.frag" VERBATIM)
    add_custom_target(sexy_filter_experiment_shader DEPENDS "${filterExperiment}")
    set(instanceExperiment "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>/sprite_instance.spv")
    add_custom_command(OUTPUT "${instanceExperiment}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>"
        COMMAND "${PVZ_GLSLC}" -Werror -fshader-stage=vertex --target-env=vulkan1.3 -O
            "${CMAKE_SOURCE_DIR}/Tests/performance/sprite_instance_experiment.vert" -o "${instanceExperiment}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${instanceExperiment}"
        DEPENDS "${CMAKE_SOURCE_DIR}/Tests/performance/sprite_instance_experiment.vert" VERBATIM)
    add_custom_target(sexy_instance_experiment_shader DEPENDS "${instanceExperiment}")
    set(solidExperiment "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>/solid_fill.spv")
    add_custom_command(OUTPUT "${solidExperiment}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>"
        COMMAND "${PVZ_GLSLC}" -Werror -fshader-stage=fragment --target-env=vulkan1.3 -O
            "${CMAKE_SOURCE_DIR}/Tests/performance/solid_fill_experiment.frag" -o "${solidExperiment}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${solidExperiment}"
        DEPENDS "${CMAKE_SOURCE_DIR}/Tests/performance/solid_fill_experiment.frag" VERBATIM)
    add_custom_target(sexy_solid_experiment_shader DEPENDS "${solidExperiment}")
    set(separateBlendExperiment "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>/single_output_premult.spv")
    add_custom_command(OUTPUT "${separateBlendExperiment}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sprite-experiment/$<CONFIG>"
        COMMAND "${PVZ_GLSLC}" -Werror -fshader-stage=fragment --target-env=vulkan1.3 -O
            "${CMAKE_SOURCE_DIR}/Tests/performance/single_output_premult_experiment.frag" -o "${separateBlendExperiment}"
        COMMAND "${PVZ_SPIRV_VAL}" --target-env vulkan1.3 "${separateBlendExperiment}"
        DEPENDS "${CMAKE_SOURCE_DIR}/Tests/performance/single_output_premult_experiment.frag" VERBATIM)
    add_custom_target(sexy_separate_blend_experiment_shader DEPENDS "${separateBlendExperiment}")
    add_library(sexy_vulkan_sprite_experiment STATIC EXCLUDE_FROM_ALL
        VulkanContext.cpp VulkanRenderer.cpp VulkanHdrDisplay.cpp VulkanHdrObserver.cpp platform/Win32Surface.cpp)
    target_include_directories(sexy_vulkan_sprite_experiment PUBLIC
        "${CMAKE_SOURCE_DIR}/pvzPerf" "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../Sexy.TodLib")
    target_compile_features(sexy_vulkan_sprite_experiment PUBLIC cxx_std_23)
    target_compile_definitions(sexy_vulkan_sprite_experiment PUBLIC PVZ_VULKAN_SPRITE_EXPERIMENT=1
        PRIVATE PVZ_SEPARATE_BLEND_EXPERIMENT_SPV="${separateBlendExperiment}" PVZ_SOLID_EXPERIMENT_SPV="${solidExperiment}" PVZ_INSTANCE_EXPERIMENT_SPV="${instanceExperiment}" PVZ_SPRITE_EXPERIMENT_SPV="${spriteExperiment}" PVZ_FILTER_EXPERIMENT_SPV="${filterExperiment}")
    target_link_libraries(sexy_vulkan_sprite_experiment PUBLIC Vulkan::Vulkan
        PRIVATE sexy_vulkan_effects user32 ole32 runtimeobject)
    set_target_properties(sexy_vulkan_sprite_experiment PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    add_dependencies(sexy_vulkan_sprite_experiment sexy_vulkan_shaders sexy_sprite_experiment_shader sexy_filter_experiment_shader sexy_instance_experiment_shader sexy_solid_experiment_shader sexy_separate_blend_experiment_shader)
    add_executable(sexy_vulkan_sprite_benchmark EXCLUDE_FROM_ALL
        "${CMAKE_SOURCE_DIR}/Tests/performance/vulkan_sprite_benchmark.cpp")
    target_link_libraries(sexy_vulkan_sprite_benchmark PRIVATE sexy_vulkan_sprite_experiment)
    target_compile_definitions(sexy_vulkan_sprite_benchmark PRIVATE PVZ_NATIVE_SHADER_DIR="${PVZ_NATIVE_SHADER_DIR}")
endif()
