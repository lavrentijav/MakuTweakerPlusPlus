# Skip copying loc/assets next to exe when embedded (Release).
if(CONFIG STREQUAL "Release" OR CONFIG STREQUAL "MinSizeRel")
    message(STATUS "Post-build: embedded data in exe — skip loc/assets copy")
    if(EXISTS "${DST}/loc")
        file(REMOVE_RECURSE "${DST}/loc")
    endif()
    if(EXISTS "${DST}/assets")
        file(REMOVE_RECURSE "${DST}/assets")
    endif()
    return()
endif()

if(NOT EXISTS "${SRC}/loc")
    message(FATAL_ERROR "PostBuildCopy: loc/ not found at ${SRC}/loc")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SRC}/loc" "${DST}/loc"
    COMMAND_ERROR_IS_FATAL ANY)
if(EXISTS "${SRC}/assets")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SRC}/assets" "${DST}/assets"
        COMMAND_ERROR_IS_FATAL ANY)
endif()
