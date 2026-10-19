cmake_minimum_required(VERSION 3.17)
project(renpy_patcher C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ===================== 版本管理 =====================
# 单一事实来源：根目录 VERSION 文件（语义化版本，如 0.1.0）。
file(READ "${CMAKE_SOURCE_DIR}/VERSION" RPATCHER_VERSION_RAW)
string(STRIP "${RPATCHER_VERSION_RAW}" RPATCHER_VERSION_STRING)
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ver_match "${RPATCHER_VERSION_STRING}")
if(_ver_match)
    set(RPATCHER_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(RPATCHER_VERSION_MINOR ${CMAKE_MATCH_2})
    set(RPATCHER_VERSION_PATCH  ${CMAKE_MATCH_3})
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
# 构建号：来自 CI（-DRP_BUILD_NUM，例如 GitHub run_number），本地默认 0。
# 用途：① MSI Product 版本第四段，供 Windows 安装器做升级/降级比较；② 构建溯源。
if(DEFINED RP_BUILD_NUM)
    set(RPATCHER_BUILD_NUM ${RP_BUILD_NUM})
else()
    set(RPATCHER_BUILD_NUM 0)
endif()

set(RPATCHER_VERSION_FULL "${RPATCHER_VERSION_STRING} (build ${RPATCHER_BUILD_NUM}, ${RPATCHER_BUILD_COMMIT})")

# 生成 core/version.h
configure_file(
    "${CMAKE_SOURCE_DIR}/core/version.h.in"
    "${CMAKE_SOURCE_DIR}/core/version.h"
    @ONLY
)
# ====================================================

# ===================== zlib（源码内置，跨平台独立构建） =====================
# third_party/zlib 为仓库内源码树，直接用其 CMake 构建静态库 zlibstatic，
# 避免对系统 zlib / vcpkg 的依赖，保证 CI 离线可构建。
# zlib 自带 example 测试会污染我们的 ctest，关闭它
option(ZLIB_BUILD_EXAMPLES "Build zlib example programs" OFF)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/zlib)
# ============================================================================

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
    util/zip_extract.c
    util/sfx.c
    util/resolve_source.c
    inject/export/export_module.c
    compress/zlib_wrap.c
    rpa/rpa_v3.c
    rpa/rpa_v3_repack.c
    mod/mod_loader.c
    translate/rpy_dialog.c
    translate/json_io.c
    translate/ai_translate.c
    translate/translate_cli.c
    analyze/branch.c
    apk/apk_tool.c
    inject/direct/direct_inject.c
    inject/hot/inject_api.c        # 热注入入口（非 Windows 为 stub，Windows 走 LoadLibrary 注入）
)

add_library(renpy_core STATIC ${CORE_SRC})
target_include_directories(renpy_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(renpy_core PUBLIC zlibstatic)

# 命令行工具
add_executable(patcher cli/main.c)
target_link_libraries(patcher PRIVATE renpy_core)
target_include_directories(patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ===================== 自解压桩（仅 Windows 原生 exe，供 export-exe 使用） =====================
# 该桩被拼接在「目录 zip」之后，运行时解出内置 zip 并可自动运行 RP_AUTORUN.txt。
# 非 Windows 跳过（自解压 exe 仅 Windows 可用）。
if(WIN32)
    add_executable(sfx_stub tools/sfx_stub.c)
    target_link_libraries(sfx_stub PRIVATE renpy_core shell32 user32)
    target_include_directories(sfx_stub PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(sfx_stub PROPERTIES WIN32_EXECUTABLE TRUE)
    # 把桩复制到 patcher / patcher_core 输出目录，便于运行时定位
    add_custom_command(TARGET sfx_stub POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:sfx_stub> $<TARGET_FILE_DIR:patcher>/sfx_stub.exe)
    if(TARGET patcher_core)
        add_custom_command(TARGET sfx_stub POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:sfx_stub> $<TARGET_FILE_DIR:patcher_core>/sfx_stub.exe)
    endif()
endif()
# ============================================================================

# ===================== 核心 DLL（供 GUI / 外部前端 P/Invoke） =====================
# 仅 Windows：把同一套核心 C 实现编成 patcher_core.dll，供 WPF（C#）通过 P/Invoke 调用。
# 导出接口见 core_api/core_api.h（所有字符串为 UTF-8）。CLI 仍链 renpy_core 静态库，互不影响。
if(WIN32)
    add_library(patcher_core SHARED core_api/core_api.c ${CORE_SRC})
    target_include_directories(patcher_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(patcher_core PUBLIC zlibstatic)
    target_compile_definitions(patcher_core PRIVATE RP_CORE_EXPORT)
    # 复制 DLL 到 CLI 输出目录，便于本地联调（WPF 运行时也需随附此 DLL）
    add_custom_command(TARGET patcher_core POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:patcher_core> $<TARGET_FILE_DIR:patcher>)
endif()
# ==========================================================================

# ==========================================================================
# ===================== 核心 API 端到端测试 =====================
# 只用 core_api.h 导出的高层 API（GUI P/Invoke 实际调用的函数）。
# Windows：链接 patcher_core.dll（GUI 真正用的内核）；Linux：链接 renpy_core 静态库。
# 跑 ctest 即可执行；CI 的 linux-cli / windows-gui job 都会调用。
enable_testing()
# Windows：链接 patcher_core.dll（GUI 真正用的内核），core_api.c 已在 DLL 内；
# Linux ：patcher_core 是 DLL 不存在，改为把 core_api.c 直接编进测试，再链 renpy_core 静态库。
add_executable(test_core_api tests/test_core_api.c)
target_include_directories(test_core_api PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(test_core_api PRIVATE patcher_core)
else()
    target_sources(test_core_api PRIVATE core_api/core_api.c)
    target_link_libraries(test_core_api PRIVATE renpy_core)
endif()
add_test(NAME core_api COMMAND test_core_api)
# ==========================================================================

# 热注入 agent（仅 Windows）：被注入游戏进程的 DLL 载荷。
# 依赖 Python 开发包（运行时定位 CPython）；若环境无 Python 开发包则跳过，
# 不影响 CLI 与 GUI 的核心功能。
if(WIN32)
    find_package(Python3 COMPONENTS Development QUIET)
    if(Python3_FOUND)
        add_library(agent SHARED
            inject/hot/agent.c
            inject/hot/py_bridge.c
        )
        target_link_libraries(agent PRIVATE renpy_core Psapi)
        target_include_directories(agent PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${Python3_INCLUDE_DIRS}
        )
    else()
        message(WARNING "Python3 开发包未找到，跳过 agent.dll 构建（不影响 CLI / GUI）")
    endif()
endif()

# TODO 里程碑6：plugins/ 外部 DLL 自动加载（plugin_loader 已预留扫描入口）
