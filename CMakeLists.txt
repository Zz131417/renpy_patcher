cmake_minimum_required(VERSION 3.16)
project(renpy_patcher C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ===================== 版本管理 =====================
# 单一事实来源：根目录 VERSION 文件（语义化版本，如 0.1.0）。
# 打 tag 时版本号与 git tag（vX.Y.Z）保持一致。
file(READ "${CMAKE_SOURCE_DIR}/VERSION" RPATCHER_VERSION_RAW)
string(STRIP "${RPATCHER_VERSION_RAW}" RPATCHER_VERSION_STRING)
string(REGEX MATCH "^([0-9]+)\.([0-9]+)\.([0-9]+)" _ver_match "${RPATCHER_VERSION_STRING}")
if(_ver_match)
    set(RPATCHER_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(RPATCHER_VERSION_MINOR ${CMAKE_MATCH_2})
    set(RPATCHER_VERSION_PATCH ${CMAKE_MATCH_3})
else()
    message(WARNING "VERSION 文件格式应为 X.Y.Z，得到: '${RPATCHER_VERSION_STRING}'")
    set(RPATCHER_VERSION_MAJOR 0)
    set(RPATCHER_VERSION_MINOR 0)
    set(RPATCHER_VERSION_PATCH 0)
endif()

# 构建来源：git 短 commit（无 git / 失败时为 "unknown"）
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE RPATCHER_BUILD_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT RPATCHER_BUILD_COMMIT)
    set(RPATCHER_BUILD_COMMIT "unknown")
endif()
set(RPATCHER_VERSION_FULL "${RPATCHER_VERSION_STRING} (${RPATCHER_BUILD_COMMIT})")

# 生成 core/version.h（被 .gitignore 忽略；用 -I. 的手动 gcc 也能找到）
configure_file(
    "${CMAKE_SOURCE_DIR}/core/version.h.in"
    "${CMAKE_SOURCE_DIR}/core/version.h"
    @ONLY
)
# ====================================================

# 核心库：所有里程碑共享的纯 C 源码
set(CORE_SRC
    core/plugin_loader.c
    core/game_mgr.c
    util/io_buf.c
    util/pickle_min.c
    util/pickle_write.c
    util/dir_walk.c
    util/log.c
    util/credit.c
    util/zip.c
    inject/export/export_module.c
    compress/zlib_wrap.c
    rpa/rpa_v3.c
    rpa/rpa_v3_repack.c
    mod/mod_loader.c
    inject/direct/direct_inject.c
    inject/hot/inject_api.c        # 热注入入口（非 Windows 为 stub）
)

add_library(renpy_core STATIC ${CORE_SRC})

find_package(ZLIB REQUIRED)
target_include_directories(renpy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)
target_link_libraries(renpy_core PUBLIC ${ZLIB_LIBRARIES})

# 命令行工具
add_executable(patcher cli/main.c)
target_link_libraries(patcher PRIVATE renpy_core)
target_include_directories(patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 热注入 agent（仅 Windows）：被注入游戏进程的 DLL 载荷
if(WIN32)
    add_library(agent SHARED
        inject/hot/agent.c
        inject/hot/py_bridge.c
    )
    target_link_libraries(agent PRIVATE renpy_core Psapi)
    target_include_directories(agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# TODO 里程碑6：GUI（C# 壳）—— 独立项目引用本 core 静态库
# TODO 里程碑8：plugins/ 外部 DLL 自动加载（plugin_loader 已预留扫描入口）
