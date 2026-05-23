cmake_minimum_required(VERSION 3.20)
project(MakuTweaker VERSION 5.6 LANGUAGES CXX)

if(WIN32 AND MSVC)
    include("${CMAKE_SOURCE_DIR}/cmake/WindowsAppSDK.cmake")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Single-config generators (Ninja): default to Release unless overridden.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo)" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(imgui json)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

file(GLOB_RECURSE MAKU_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(MakuTweaker WIN32
    ${MAKU_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(MakuTweaker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(MakuTweaker PRIVATE
    nlohmann_json::nlohmann_json
    d3d11 dxgi dwmapi shell32 ole32 oleaut32 wbemuuid pdh psapi advapi32 iphlpapi ws2_32 crypt32
)

target_compile_definitions(MakuTweaker PRIVATE
    UNICODE _UNICODE
    MAKUTWEAKER_VERSION="${PROJECT_VERSION}"
    NOMINMAX WIN32_LEAN_AND_MEAN
)

if(WIN32 AND MSVC)
    maku_link_winappsdk(MakuTweaker)
endif()

if(MSVC)
    # Release / RelWithDebInfo: size and speed (VS multi-config + Ninja).
    target_compile_options(MakuTweaker PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:/O2 /Ob2 /Oi /Gy>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Ob1 /Zi>
    )
    target_link_options(MakuTweaker PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:/OPT:REF /OPT:ICF>
    )

    set(MAKU_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/icons/MakuTweaker.ico")
    if(NOT EXISTS "${MAKU_ICON_PATH}")
        set(MAKU_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/icons/makut.ico")
    endif()
    file(TO_CMAKE_PATH "${MAKU_ICON_PATH}" MAKU_ICON_PATH_RC)
    string(REPLACE "\\" "/" MAKU_ICON_PATH_RC "${MAKU_ICON_PATH_RC}")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/AppIcon.rc.in"
                   "${CMAKE_BINARY_DIR}/AppIcon.rc" @ONLY)
    target_sources(MakuTweaker PRIVATE "${CMAKE_BINARY_DIR}/AppIcon.rc")

    target_compile_options(MakuTweaker PRIVATE /W4 /utf-8)
    set_source_files_properties(src/core/Benchmark.cpp PROPERTIES
        COMPILE_OPTIONS "/fp:precise")
    set_target_properties(MakuTweaker PROPERTIES
        VS_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/app.manifest"
        OUTPUT_NAME "MakuTweaker++")
endif()

# Embed build number
if(EXISTS "${CMAKE_SOURCE_DIR}/BuildNumber.txt")
    file(READ "${CMAKE_SOURCE_DIR}/BuildNumber.txt" BUILD_NUM_RAW)
    string(STRIP "${BUILD_NUM_RAW}" BUILD_NUMBER)
else()
    set(BUILD_NUMBER "0")
endif()
target_compile_definitions(MakuTweaker PRIVATE MAKUTWEAKER_BUILD=${BUILD_NUMBER})

add_custom_command(TARGET MakuTweaker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/loc" "$<TARGET_FILE_DIR:MakuTweaker>/loc"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:MakuTweaker>/assets"
    COMMENT "Copying loc/ and assets/ to output"
)
