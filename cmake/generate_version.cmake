# cmake/generate_version.cmake
# 由 GUI 工程的 PreBuild 事件调用，根据根目录 VERSION 与 git 提交生成 core/version.h。
# 用法：cmake -D VERSION_FILE=../VERSION -P cmake/generate_version.cmake
# 单一事实来源：VERSION 文件（如 0.1.6）。

cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED VERSION_FILE)
    set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
endif()

# 读取 VERSION
file(READ "${VERSION_FILE}" VER)
string(STRIP "${VER}" VER)

# 解析 major.minor.patch
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _match "${VER}")
if(NOT _match)
    message(FATAL_ERROR "无法解析 VERSION: '${VER}'")
endif()
set(RPATCHER_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(RPATCHER_VERSION_MINOR "${CMAKE_MATCH_2}")
set(RPATCHER_VERSION_PATCH  "${CMAKE_MATCH_3}")
set(RPATCHER_VERSION_STRING "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

# 尝试获取 git commit
set(RPATCHER_BUILD_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _commit
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_commit)
        set(RPATCHER_BUILD_COMMIT "${_commit}")
    endif()
endif()

set(RPATCHER_VERSION_FULL "${RPATCHER_VERSION_STRING} (${RPATCHER_BUILD_COMMIT})")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/core/version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/version.h"
    @ONLY)

message(STATUS "已生成 core/version.h: ${RPATCHER_VERSION_STRING} (${RPATCHER_BUILD_COMMIT})")
