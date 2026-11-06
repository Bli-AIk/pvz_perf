include("${CMAKE_SOURCE_DIR}/CMake/DevTools.cmake")
pvz_import_dev_tool(reanimc reanimc PVZ_RESOURCE_COMPILER_HOST)
pvz_import_dev_tool(pvz_resource_builder pvz-resource-build PVZ_RESOURCE_BUILDER_HOST)

set(PVZ_RESOURCE_CACHE "${CMAKE_BINARY_DIR}/cache/resources" CACHE PATH
    "Resource compiler cache, optionally shared with a verified host build")
set(PVZ_STATIC_DEFINITION_SOURCES
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/GeneratedDefinitions.cpp"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/GeneratedDefinitionLoading.cpp"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ZombatarAtlasData.cpp"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/SceneAtlasData.cpp")
foreach(index RANGE 0 7)
    list(APPEND PVZ_STATIC_DEFINITION_SOURCES
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/definitions/ReanimData0${index}.cpp")
endforeach()
file(GLOB_RECURSE PVZ_RESOURCE_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/definitions/*"
    "${CMAKE_SOURCE_DIR}/assets/runtime/*"
    "${CMAKE_SOURCE_DIR}/assets/source-art/ai/runtime/*"
    "${CMAKE_SOURCE_DIR}/assets/source-art/ai/runtime-overlay.json"
    "${PVZ_GAME_SOURCE_DIR}/*.cpp"
    "${PVZ_GAME_SOURCE_DIR}/*.h")
list(FILTER PVZ_RESOURCE_INPUTS EXCLUDE REGEX "/generated/")
list(FILTER PVZ_RESOURCE_INPUTS EXCLUDE REGEX "/ReanimTrack(Id|MphfGlobal)\\.h$")

# Keep generated contract headers visible as explicit compile inputs. MSVC's
# discovered includes may use a relative spelling while BYPRODUCTS use an
# absolute spelling; an order-only target dependency cannot invalidate an
# already-clean object after the resource producer changes that physical file.
set(PVZ_RESOURCE_CONTRACT_HEADERS
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/DefinitionImageBindings.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ZombatarImage.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/SceneAtlasData.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/SceneAtlasBindings.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/GeneratedDefinitions.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/GeneratedDefinitionStorage.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/NumericResourceBindings.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ResourceLoadBindings.inc"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ParticleEmitterName.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ReanimVariantId.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ParticleVariantId.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/LawnStrings.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ReanimAttacherMasks.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/ReanimTrackId.h"
    "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/ReanimTrackMphfGlobal.h")

add_custom_command(
    OUTPUT "${PVZ_RESOURCE_CACHE}/resources.stamp" "${PVZ_RESOURCE_CACHE}/main.pak64"
    BYPRODUCTS
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/DefinitionImageBindings.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ZombatarImage.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/SceneAtlasData.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/SceneAtlasBindings.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/GeneratedDefinitions.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/GeneratedDefinitionStorage.h"
        ${PVZ_STATIC_DEFINITION_SOURCES}
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/NumericResourceBindings.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ResourceLoadBindings.inc"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ParticleEmitterName.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ReanimVariantId.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ParticleVariantId.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/LawnStrings.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/LawnStrings.cpp"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/generated/ReanimAttacherMasks.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/ReanimTrackId.h"
        "${PVZ_GAME_SOURCE_DIR}/Sexy.TodLib/ReanimTrackMphfGlobal.h"
    COMMAND "$<TARGET_FILE:pvz_resource_builder>" build --root "${CMAKE_SOURCE_DIR}"
        --compiler "$<TARGET_FILE:reanimc>" --cache "${PVZ_RESOURCE_CACHE}"
        --config "${CMAKE_SOURCE_DIR}/assets/definitions/ResourceBuild.pvzbuild"
    DEPENDS reanimc pvz_resource_builder ${PVZ_RESOURCE_INPUTS} ${PVZ_DEVTOOLS_BINARY_INPUTS}
    COMMENT "Compiling definitions, generated C++ and runtime PAK64"
    VERBATIM)
add_custom_target(pvz_resources
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/$<CONFIG>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PVZ_RESOURCE_CACHE}/main.pak64" "${CMAKE_BINARY_DIR}/$<CONFIG>/main.pak64"
    DEPENDS "${PVZ_RESOURCE_CACHE}/resources.stamp" "${PVZ_RESOURCE_CACHE}/main.pak64"
    VERBATIM)
