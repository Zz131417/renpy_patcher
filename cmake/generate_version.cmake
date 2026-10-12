# cmake/generate_version.cmake
# 由 GUI 工程的 PreBuild 事件调用，根据根目录 VERSION 与 git 提交生成 core/version.h。
#
# 用法：cmake -D VERSION_FILE=<路径> -P cmake/generate_version.cmake
# 注意：脚本用 CMAKE_CURRENT_LIST_DIR（= cmake/ 目录）定位仓库根，
#       因此无论从哪个工作目录调用都能正确找到 core/version.h.in 与 VERSION。
# 单一事实来源：VERSION 文件（如 0.1.7）。

cmake_minimum_required(VERSION 3.10)

# 仓库根目录（cmake/ 的上级）
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED VERSION_FILE)
    set(VERSION_FILE "${REPO_ROOT}/VERSION")
endif()

# 读取 VERSION
file(READ "${VERSION_FILE}" VER)
string(STRIP "${VER}" VER)

# 解析 major.minor.patch
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _match "${VER}")
if(NOT _match)
    message(FATAL_ERROR "无法解析 VERSION: '${VER}' (文件: ${VERSION_FILE})")
endif()
set(RPATCHER_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(RPATCHER_VERSION_MINOR "${CMAKE_MATCH_2}")
set(RPATCHER_VERSION_PATCH  "${CMAKE_MATCH_3}")
set(RPATCHER_VERSION_STRING "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

# 尝试获取 git commit（在项目根执行）
set(RPATCHER_BUILD_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${REPO_ROOT}"
        OUTPUT_VARIABLE _commit
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_commit)
        set(RPATCHER_BUILD_COMMIT "${_commit}")
    endif()
endif()

set(RPATCHER_VERSION_FULL "${RPATCHER_VERSION_STRING} (${RPATCHER_BUILD_COMMIT})")

configure_file(
    "${REPO_ROOT}/core/version.h.in"
    "${REPO_ROOT}/core/version.h"
    @ONLY)

message(STATUS "已生成 core/version.h: ${RPATCHER_VERSION_STRING} (${RPATCHER_BUILD_COMMIT})")
