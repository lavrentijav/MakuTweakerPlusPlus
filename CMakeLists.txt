cmake_minimum_required(VERSION 3.20)
project(MakuTweaker VERSION 5.8 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MAKU_ONEFILE "Build single exe with embedded loc/assets/previewimg" OFF)
option(MAKU_BUMP_BUILD "Increment BuildNumber.txt before each build (local compile counter)" OFF)

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.16
)
FetchContent_MakeAvailable(imgui json implot)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

file(GLOB_RECURSE MAKU_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(FILTER MAKU_SOURCES EXCLUDE REGEX ".*\\.h$")
list(FILTER MAKU_SOURCES EXCLUDE REGEX "EmbeddedPayloadData\\.cpp$")

add_executable(MakuTweaker WIN32
    ${MAKU_SOURCES}
    ${IMGUI_SOURCES}
    ${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c
)

target_include_directories(MakuTweaker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/sqlite
)

target_link_libraries(MakuTweaker PRIVATE
    nlohmann_json::nlohmann_json
    d3d11 dxgi dwmapi shell32 ole32 oleaut32 wbemuuid pdh psapi advapi32 bcrypt
    iphlpapi ws2_32 wtsapi32 userenv
)

target_compile_definitions(MakuTweaker PRIVATE
    UNICODE _UNICODE
    MAKUTWEAKER_VERSION="${PROJECT_VERSION}"
    NOMINMAX WIN32_LEAN_AND_MEAN
    SQLITE_THREADSAFE=1
)

if(MSVC)
    target_compile_options(MakuTweaker PRIVATE /W4 /utf-8)
    set_source_files_properties(src/core/Benchmark.cpp PROPERTIES
        COMPILE_OPTIONS "/fp:precise")

    set(MAKU_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/icons/MakuT.ico")
    if(NOT EXISTS "${MAKU_ICON_PATH}")
        set(MAKU_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/icons/MakuTweaker.ico")
    endif()
    file(TO_CMAKE_PATH "${MAKU_ICON_PATH}" MAKU_ICON_PATH_RC)
    string(REPLACE "\\" "/" MAKU_ICON_PATH_RC "${MAKU_ICON_PATH_RC}")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/AppIcon.rc.in"
                   "${CMAKE_BINARY_DIR}/AppIcon.rc" @ONLY)
    target_sources(MakuTweaker PRIVATE "${CMAKE_BINARY_DIR}/AppIcon.rc")

    set_target_properties(MakuTweaker PROPERTIES
        VS_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/app.manifest"
        OUTPUT_NAME "MakuTweaker++")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/BuildNumber.txt")
    file(READ "${CMAKE_SOURCE_DIR}/BuildNumber.txt" BUILD_NUM_RAW)
    string(STRIP "${BUILD_NUM_RAW}" BUILD_NUMBER)
else()
    set(BUILD_NUMBER "0")
endif()

set(MAKU_APP_VERSION "${PROJECT_VERSION}.${BUILD_NUMBER}")
target_compile_definitions(MakuTweaker PRIVATE
    MAKUTWEAKER_BUILD=${BUILD_NUMBER}
    MAKU_APP_VERSION="${MAKU_APP_VERSION}"
)

if(MAKU_BUMP_BUILD)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_command(
        TARGET MakuTweaker PRE_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/bump_build.py"
                --root "${CMAKE_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Bump BuildNumber.txt (compilation counter)"
    )
endif()

if(MAKU_ONEFILE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    file(GLOB_RECURSE MAKU_PAYLOAD_FILES
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/loc/*.json"
        "${CMAKE_SOURCE_DIR}/assets/**"
        "${CMAKE_SOURCE_DIR}/previewimg/**"
    )

    set(MAKU_PAYLOAD_BIN "${CMAKE_CURRENT_BINARY_DIR}/maku_payload.bin")
    set(MAKU_PAYLOAD_CPP "${CMAKE_CURRENT_BINARY_DIR}/EmbeddedPayloadData.cpp")

    add_custom_command(
        OUTPUT "${MAKU_PAYLOAD_BIN}" "${MAKU_PAYLOAD_CPP}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/pack_embedded.py"
                "${CMAKE_SOURCE_DIR}" "${MAKU_PAYLOAD_BIN}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/bin_to_cpp.py"
                "${MAKU_PAYLOAD_BIN}" "${MAKU_PAYLOAD_CPP}" EmbeddedPayload
        DEPENDS
            "${CMAKE_SOURCE_DIR}/scripts/pack_embedded.py"
            "${CMAKE_SOURCE_DIR}/scripts/bin_to_cpp.py"
            ${MAKU_PAYLOAD_FILES}
        COMMENT "Packing loc/assets/previewimg into embedded payload"
    )

    target_sources(MakuTweaker PRIVATE "${MAKU_PAYLOAD_CPP}")
    target_compile_definitions(MakuTweaker PRIVATE MAKUTWEAKER_ONEFILE=1)
    message(STATUS "MakuTweaker onefile build: embedded payload enabled")
else()
    add_custom_command(TARGET MakuTweaker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/loc" "$<TARGET_FILE_DIR:MakuTweaker>/loc"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:MakuTweaker>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/previewimg" "$<TARGET_FILE_DIR:MakuTweaker>/previewimg"
        COMMENT "Copying loc/, assets/, previewimg/ to output"
    )
endif()
