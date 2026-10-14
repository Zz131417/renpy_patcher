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
    # 把桩复制到 patcher / patcher_gui 输出目录，便于运行时定位
    add_custom_command(TARGET sfx_stub POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:sfx_stub> $<TARGET_FILE_DIR:patcher>/sfx_stub.exe)
    if(TARGET patcher_gui)
        add_custom_command(TARGET sfx_stub POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:sfx_stub> $<TARGET_FILE_DIR:patcher_gui>/sfx_stub.exe)
    endif()
endif()
# ============================================================================

# ===================== Windows GUI（C++/CLI WinForms） =====================
# 仅 Windows：复用同一套核心 C 实现（renpy_core 静态库），
# GUI 用 C++/CLI（/clr）调用，功能与 CLI 完全对齐。
if(WIN32)
    # 注意：不要加 WIN32。WIN32 会让链接器要求 native WinMain，
    # 但 C++/CLI 程序的入口是托管 main()（见 gui/Program.cpp），
    # 故保持默认 CONSOLE 子系统以 main 为入口，链接即可通过。
    add_executable(patcher_gui
        gui/Program.cpp
        gui/MainForm.cpp
        gui/MainForm.h
    )
    # 显式开启 C++/CLI（/clr）——比 VS_CLR_SUPPORT 属性更可靠，确保 /clr 被注入编译命令。
    # /EHa：/clr 要求的异常处理模型；/permissive：关闭一致性模式（/clr 需要）。
    target_compile_options(patcher_gui PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/clr /EHa /permissive>)
    # 绑定 .NET Framework 4.8 并引用 WinForms 等程序集
    set_target_properties(patcher_gui PROPERTIES
        VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8"
        VS_DOTNET_REFERENCES "System;System.Core;System.Drawing;System.Windows.Forms;System.Xml"
    )
    target_link_libraries(patcher_gui PRIVATE renpy_core)
    target_include_directories(patcher_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
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
