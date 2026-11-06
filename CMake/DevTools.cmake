include_guard(GLOBAL)

if(CMAKE_HOST_WIN32)
    set(_pvz_tools_platform windows-x64)
    set(_pvz_tools_suffix .exe)
else()
    set(_pvz_tools_platform "${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
    set(_pvz_tools_suffix "")
endif()
set(PVZ_DEVTOOLS_BIN_DIR "${CMAKE_SOURCE_DIR}/tools/bin/${_pvz_tools_platform}"
    CACHE PATH "Prebuilt host tools from YuHao2006/PvzDevTools")
set(PVZ_DEVTOOLS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../PvzDevTools"
    CACHE PATH "Optional independent PvzDevTools checkout used by tool regression tests")

function(pvz_import_dev_tool target filename variable)
    set(${variable} "${PVZ_DEVTOOLS_BIN_DIR}/${filename}${_pvz_tools_suffix}"
        CACHE FILEPATH "Prebuilt ${filename} host executable from PvzDevTools")
    if(NOT EXISTS "${${variable}}")
        message(FATAL_ERROR
            "Missing host tool: ${${variable}}. Download Windows tools from "
            "https://github.com/YuHao2006/PvzDevTools/releases, or build that separate "
            "repository for this host and set PVZ_DEVTOOLS_BIN_DIR. Tool sources are not part of pvz_perf.")
    endif()
    if(NOT TARGET ${target})
        add_executable(${target} IMPORTED GLOBAL)
        set_target_properties(${target} PROPERTIES IMPORTED_LOCATION "${${variable}}")
    endif()
endfunction()

file(GLOB PVZ_DEVTOOLS_BINARY_INPUTS CONFIGURE_DEPENDS
    "${PVZ_DEVTOOLS_BIN_DIR}/*.exe" "${PVZ_DEVTOOLS_BIN_DIR}/*.dll" "${PVZ_DEVTOOLS_BIN_DIR}/manifest.json")
