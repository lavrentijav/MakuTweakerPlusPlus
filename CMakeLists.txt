cmake_minimum_required(VERSION 3.20)
project(MakuTweaker VERSION 5.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

file(GLOB_RECURSE MAKU_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(FILTER MAKU_SOURCES EXCLUDE REGEX ".*\\.h$")

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
    d3d11 dxgi dwmapi shell32 ole32 oleaut32 wbemuuid pdh psapi advapi32
)

target_compile_definitions(MakuTweaker PRIVATE
    UNICODE _UNICODE
    MAKUTWEAKER_VERSION="${PROJECT_VERSION}"
    NOMINMAX WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(MakuTweaker PRIVATE /W4 /utf-8)
    set_source_files_properties(src/core/Benchmark.cpp PROPERTIES
        COMPILE_OPTIONS "/fp:precise")
    set_target_properties(MakuTweaker PROPERTIES
        VS_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/app.manifest")
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
