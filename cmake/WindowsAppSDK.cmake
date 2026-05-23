# Windows App SDK (MicaController) via NuGet + C++/WinRT headers from Windows SDK cppwinrt.exe

set(MAKU_WASDK_VERSION "1.6.250108002" CACHE STRING "Microsoft.WindowsAppSDK NuGet version")

set(MAKU_NUGET_DIR "${CMAKE_BINARY_DIR}/nuget")
set(MAKU_NUGET_EXE "${CMAKE_BINARY_DIR}/nuget.exe")

if(NOT EXISTS "${MAKU_NUGET_EXE}")
    message(STATUS "Downloading nuget.exe...")
    file(DOWNLOAD
        "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe"
        "${MAKU_NUGET_EXE}"
        SHOW_PROGRESS
        TLS_VERIFY ON)
endif()

function(maku_nuget_install PACKAGE VERSION OUT_ROOT)
    set(_root "${MAKU_NUGET_DIR}/${PACKAGE}.${VERSION}")
    if(NOT EXISTS "${_root}")
        message(STATUS "NuGet install ${PACKAGE} ${VERSION}...")
        execute_process(
            COMMAND "${MAKU_NUGET_EXE}" install "${PACKAGE}" -Version "${VERSION}"
                    -OutputDirectory "${MAKU_NUGET_DIR}" -NonInteractive
            RESULT_VARIABLE _rc
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "nuget install ${PACKAGE} failed:\n${_out}\n${_err}")
        endif()
    endif()
    set("${OUT_ROOT}" "${_root}" PARENT_SCOPE)
endfunction()

maku_nuget_install(Microsoft.WindowsAppSDK "${MAKU_WASDK_VERSION}" MAKU_WASDK_ROOT)

set(MAKU_WASDK_INPUT_DIR "${MAKU_WASDK_ROOT}/lib/uap10.0.18362")
if(NOT EXISTS "${MAKU_WASDK_INPUT_DIR}/Microsoft.UI.winmd")
    message(FATAL_ERROR "Microsoft.UI.winmd not found in ${MAKU_WASDK_INPUT_DIR}")
endif()

# Match installed Windows SDK cppwinrt (avoids header version static_assert)
if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(_sdk_ver "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
else()
    set(_sdk_ver "10.0.26100.0")
endif()
set(_cppwinrt_candidates
    "$ENV{WindowsSdkDir}bin/${_sdk_ver}/x64/cppwinrt.exe"
    "C:/Program Files (x86)/Windows Kits/10/bin/${_sdk_ver}/x64/cppwinrt.exe")
set(MAKU_CPPWINRT_EXE "")
foreach(_cand IN LISTS _cppwinrt_candidates)
    if(EXISTS "${_cand}")
        set(MAKU_CPPWINRT_EXE "${_cand}")
        break()
    endif()
endforeach()
if(NOT MAKU_CPPWINRT_EXE)
    find_program(MAKU_CPPWINRT_EXE cppwinrt.exe)
endif()
if(NOT MAKU_CPPWINRT_EXE OR NOT EXISTS "${MAKU_CPPWINRT_EXE}")
    message(FATAL_ERROR "cppwinrt.exe not found (Windows SDK ${_sdk_ver})")
endif()

set(MAKU_WINRT_GEN "${CMAKE_BINARY_DIR}/winrt_gen")
file(MAKE_DIRECTORY "${MAKU_WINRT_GEN}")

set(MAKU_CPPWINRT_STAMP "${MAKU_WINRT_GEN}/.cppwinrt_stamp")
add_custom_command(
    OUTPUT "${MAKU_CPPWINRT_STAMP}"
    COMMAND "${MAKU_CPPWINRT_EXE}"
            -input "${MAKU_WASDK_INPUT_DIR}"
            -reference sdk
            -include Microsoft.UI
            -include Microsoft.UI.Composition.SystemBackdrops
            -output "${MAKU_WINRT_GEN}"
            -verbose
    COMMAND "${CMAKE_COMMAND}" -E touch "${MAKU_CPPWINRT_STAMP}"
    COMMENT "Generating C++/WinRT headers (MicaController)"
    VERBATIM)

add_custom_target(MakuCppWinRT DEPENDS "${MAKU_CPPWINRT_STAMP}")

set(MAKU_WASDK_INCLUDE "${MAKU_WASDK_ROOT}/include")

find_library(MAKU_WASDK_LIB
    NAMES Microsoft.WindowsAppRuntime
    PATHS "${MAKU_WASDK_ROOT}/lib/win10-x64"
    NO_DEFAULT_PATH)
set(MAKU_WASDK_BOOTSTRAP_DLL "${MAKU_WASDK_ROOT}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")
if(NOT EXISTS "${MAKU_WASDK_BOOTSTRAP_DLL}")
    message(FATAL_ERROR "Bootstrap DLL not found: ${MAKU_WASDK_BOOTSTRAP_DLL}")
endif()
file(TO_CMAKE_PATH "${MAKU_WASDK_BOOTSTRAP_DLL}" MAKU_WASDK_BOOTSTRAP_DLL_RC)
string(REPLACE "\\" "/" MAKU_WASDK_BOOTSTRAP_DLL_RC "${MAKU_WASDK_BOOTSTRAP_DLL_RC}")

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/embedded_runtime.rc.in"
    "${CMAKE_BINARY_DIR}/embedded_runtime.rc"
    @ONLY)

find_library(MAKU_WASDK_DISPATCHING_LIB
    NAMES Microsoft.UI.Dispatching
    PATHS "${MAKU_WASDK_ROOT}/lib/native/win10-x64"
    NO_DEFAULT_PATH)

if(NOT MAKU_WASDK_LIB)
    message(FATAL_ERROR "Windows App SDK libs not found under ${MAKU_WASDK_ROOT}")
endif()

set(MAKU_SDK_CPPWINRT_INCLUDE "C:/Program Files (x86)/Windows Kits/10/Include/${_sdk_ver}/cppwinrt")
if(DEFINED ENV{WindowsSdkDir})
    set(_sdk_inc "$ENV{WindowsSdkDir}Include/${_sdk_ver}/cppwinrt")
    if(EXISTS "${_sdk_inc}")
        set(MAKU_SDK_CPPWINRT_INCLUDE "${_sdk_inc}")
    endif()
endif()

function(maku_link_winappsdk target)
    add_dependencies(${target} MakuCppWinRT)
    target_sources(${target} PRIVATE "${CMAKE_BINARY_DIR}/embedded_runtime.rc")
    target_include_directories(${target} PRIVATE
        "${MAKU_WINRT_GEN}"
        "${MAKU_SDK_CPPWINRT_INCLUDE}"
        "${MAKU_WASDK_INCLUDE}")
    target_compile_definitions(${target} PRIVATE MAKU_WINAPPSDK=1)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus /permissive->)
    # Bootstrap.dll is embedded (RCDATA) and loaded via GetProcAddress — must NOT be in the import table.
    target_link_options(${target} PRIVATE
        "/NODEFAULTLIB:Microsoft.WindowsAppRuntime.Bootstrap.lib")
    target_link_libraries(${target} PRIVATE
        ${MAKU_WASDK_LIB}
        windowsapp
        RuntimeObject)
    if(MAKU_WASDK_DISPATCHING_LIB)
        target_link_libraries(${target} PRIVATE ${MAKU_WASDK_DISPATCHING_LIB})
    endif()
endfunction()
