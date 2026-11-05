cmake_minimum_required(VERSION 3.25)
project(PvzDefinitionSizeProbe LANGUAGES CXX)
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Configure with emcmake; this is a definition-data probe, not the full game")
endif()
get_filename_component(PVZ_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
file(GLOB PVZ_REANIM_DATA "${PVZ_ROOT}/pvzPerf/Sexy.TodLib/generated/definitions/ReanimData*.cpp")
add_executable(pvz-definitions probe.cpp
    "${PVZ_ROOT}/pvzPerf/Sexy.TodLib/generated/GeneratedDefinitions.cpp" ${PVZ_REANIM_DATA})
target_compile_features(pvz-definitions PRIVATE cxx_std_23)
target_include_directories(pvz-definitions BEFORE PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/include" "${PVZ_ROOT}/pvzPerf"
    "${PVZ_ROOT}/pvzPerf/SexyAppFramework" "${PVZ_ROOT}/pvzPerf/Sexy.TodLib")
target_compile_options(pvz-definitions PRIVATE -Oz -flto)
target_link_options(pvz-definitions PRIVATE -Oz -flto --no-entry
    "-sMODULARIZE=1" "-sEXPORT_NAME=createPvzDefinitionProbe"
    "-sENVIRONMENT=web,node" "-sALLOW_MEMORY_GROWTH=1")
