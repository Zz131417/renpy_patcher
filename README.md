# Ren'Py Patcher

> 🌐 English documentation: [README.en.md](README.en.md)

> 📄 **许可证**：本项目采用**非商业许可证**（详见 `LICENSE`）。个人学习、研究、非商业用途可自由使用与修改；**禁止任何商业用途**，商业使用需单独书面授权。
> ✅ **当前版本 v1.0.0（正式版）**：核心引擎（解包 / 重打包 / 直接注入 / 导出）与 Windows GUI 均已稳定可用。详见 [CHANGELOG.md](CHANGELOG.md)。

面向 Ren'Py 游戏（Galgame / 视觉小说）的**汉化与 Mod 补丁工具**。

纯 C 核心引擎负责资源包（RPA）的解析与重写，配合统一模块契约，可扩展支持多种注入方式、翻译流水线与压缩导出。当前已具备「解包 → 重打包 → 直接注入 → 热注入（Windows）」完整链路，并提供与 CLI 功能对齐的 Windows GUI。

## 特性
- **RPA-3.0 双向闭环**：解包与重打包字节级兼容 Ren'Py 引擎（索引写在数据之后，头部记录索引偏移）。
- **直接注入**：把汉化/Mod 资源合并进游戏包后重打包，产出可被真实游戏直接读取的补丁。
- **热注入（Windows）**：运行时向游戏进程注入 DLL，定位 CPython、激活游戏内 Python 加载 `mod_main.py`，零改文件。
- **可扩展架构**：统一模块注册表（`core/plugin_loader`），新能力（压缩导出 / 翻译后端 / 外部插件）即插即用。
- **Windows GUI**：C++/CLI + .NET WinForms，复用同一核心库，功能与 CLI 对齐；操作前校验输入、失败在日志框显示「错误码 + 中文含义 + 底层详情」、全局 `try/catch` 防崩溃。

## 技术栈
| 层面 | 选型 |
| --- | --- |
| 核心语言 | **C11**（纯 C，无 C++ / 外部运行时依赖） |
| 压缩 | **zlib**（RPA 内嵌 deflate；自研 `zlib_wrap` 封装 inflate/deflate），源码内置 `third_party/zlib` |
| 序列化 | 自研 **pickle_min**：解析 CPython 协议 2/3/4 的 `pickle`，用于读取与重写 Ren'Py 索引 |
| 资源格式 | **RPA-3.0**：头部 `RPA-3.0` + 8 字节索引偏移 + 4 字节 XOR prefix；索引（pickle）位于数据段之后 |
| 构建系统 | **CMake ≥ 3.17**（核心静态库 `renpy_core` + CLI `patcher` 可执行 + Windows GUI `patcher_gui`）；zlib 源码内置，无 vcpkg / 系统 zlib |
| 持续集成 | **GitHub Actions**：Ubuntu(gcc) / Windows(MSVC) 双矩阵编译；发版走手动 `workflow_dispatch` 一键编译发布，`v*` 标签仅作定期备份 |
| 热注入（Windows） | DLL 注入（`CreateRemoteThread` + `LoadLibrary`）+ **CPython C-API** 桥（`py_bridge.c` 定位宿主解释器并执行 `mod_main.py`） |
| 扩展机制 | 统一模块契约（`core/plugin.h`）+ 中央注册表（`core/plugin_loader`） |

## 项目目录
```
.
├── CMakeLists.txt            # 构建定义：renpy_core 静态库 + patcher 可执行 + patcher_gui（Windows GUI）
├── VERSION                   # 版本号（语义化版本，单一事实来源）
├── CHANGELOG.md              # 更新日志
├── LICENSE                  # 非商业许可证（禁止商业使用）
├── DESIGN.md                # 架构设计与里程碑说明
├── .github/workflows/
│   └── build.yml            # CI：Linux/Windows 编译；手动 workflow_dispatch 一键编译发布
├── third_party/
│   └── zlib/                # 内置 zlib 源码（add_subdirectory 构建，无需 vcpkg / 系统 zlib / 联网）
├── core/                    # 统一模块契约与注册表
│   ├── plugin.h             #   模块接口 / 结构体定义
│   └── plugin_loader.c/.h   #   中央注册表（查找 / 注册 / 扩展入口）
├── util/                    # 通用工具
│   ├── io_buf.c/.h          #   字节缓冲（读 / 写 / 追加）
│   ├── pickle_min.c/.h      #   CPython pickle 解析器（协议 2/3/4，含 memo）
│   ├── pickle_write.c/.h    #   pickle 序列化（写入 RPA 索引，协议 2）
│   └── dir_walk.c/.h        #   目录树遍历（重打包 / mod 扫描）
├── compress/
│   └── zlib_wrap.c/.h       # zlib inflate/deflate 封装（带安全上限防卡死）
├── rpa/                     # RPA-3.0 解包与重打包
│   ├── rpa_format.h         #   头 / 块结构定义
│   ├── rpa_v3.c/.h          #   解包 + 索引解析
│   └── rpa_v3_repack.c      #   重打包（数据 + 索引拼装）
├── mod/                     # 统一 mod 模块
│   ├── mod_loader.c         #   目录合并（覆盖 / 新增文件）
│   └── mod_manifest.h       #   mod 元数据 / 契约
├── inject/
│   ├── direct/
│   │   └── direct_inject.c/.h  # 直接注入：解包→合并→重打包
│   └── hot/                 # 热注入（运行时进程注入，Windows）
│       ├── agent.c/.h       #   被注入的 DLL 载荷
│       ├── inject_api.c     #   热注入入口（非 Windows 为 stub）
│       └── py_bridge.c/.h   #   定位宿主 CPython 并执行 mod_main.py
├── gui/                     # Windows GUI（C++/CLI + .NET WinForms，复用 renpy_core）
│   ├── Program.cpp          #   托管入口 main()
│   ├── MainForm.h/.cpp      #   主窗口与事件（含 try/catch 防崩溃、报错显示）
├── cli/
│   └── main.c               # 命令行入口（子命令分发）
└── tests/                   # 演示游戏 + 汉化 mod + 验证脚本
    ├── README_test.md       #   测试说明（含许可证约束）
    ├── gen_demo_assets.py   #   生成演示游戏素材
    ├── make_real_rpa.py     #   用真实 RPA 格式打包
    ├── verify_rpa.py        #   用 Ren'Py loader 逻辑校验产物
    ├── mod_hanzi/           #   示例汉化 mod（含中文 rpy + logo 替换）
    └── sample_game/         #   示例游戏（game/ 目录）
```

## 架构（统一模块契约）
- 所有能力实现 `core/plugin.h` 定义的统一接口，启动时向 `core/plugin_loader` 中央注册表登记。
- CLI（`cli/main.c`）解析子命令后，从注册表查找对应模块并调用。
- GUI（`gui/MainForm.cpp`）复用同一静态库 `renpy_core`，通过 C 接口调用，与 CLI 共享全部能力。
- 新能力（压缩导出 / 翻译后端 / 外部插件 DLL）只需实现契约并注册即可接入，无需改动核心。

## 构建

zlib 以源码形式内置在 `third_party/zlib`，由 CMake `add_subdirectory` 自动构建，**无需系统 zlib / vcpkg / 联网**。

版本号由根目录 `VERSION` 文件统一管理（语义化版本，如 `1.0.0`），CMake 会据此生成
`core/version.h` 并嵌入 git 短提交信息；`patcher --version` 可查看。

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# 产物：build/patcher（Linux），build/Release/patcher.exe + patcher_gui.exe（Windows）
```

> 注：CLI 也可用单文件 gcc 直接编（仍需先由 CMake 生成 `core/version.h`，且链接系统 `-lz`）：
> ```bash
> gcc -std=c11 -O2 -I. core/plugin_loader.c util/*.c compress/*.c rpa/*.c mod/*.c \
>     inject/direct/*.c inject/hot/inject_api.c cli/main.c -lz -o build/patcher
> ```

## 用法
```bash
patcher --version        # 查看版本与构建信息（commit / 许可）
patcher [--log f.log | --no-log | --debug | --log-level N] <子命令> ...

# 游戏管理（把常用游戏登记成别名，之后用别名代替路径）
patcher game add   <别名> <game.rpa路径> [备注]      # 登记游戏
patcher game list                                      # 列出已登记游戏
patcher game show  <别名>                             # 查看详情（含包内文件数）
patcher game remove <别名>                            # 注销游戏
patcher game path   <别名>                            # 仅打印路径（便于脚本）

# 资源操作（输入可以是 .rpa 路径，也可以是已登记游戏的别名）
patcher unpack <game.rpa|别名> -o out_dir             # 解包
patcher repack dir -o out.rpa                        # 原样重打包
patcher build  <game.rpa|别名> -m mod_dir -o patched.rpa  # 直接注入（自动写入来源声明）
patcher hotinject --pid <pid> --mod mod_dir          # 热注入（Windows）

# 来源声明：给（已）注入/汉化的包标注「由 Ren'Py Patcher 开源项目注入/汉化」
patcher credit <game.rpa|别名> -o out.rpa            # 给已有包追加声明
patcher credit --text [--game <名>]                  # 仅导出声明文字

# 导出：把目录（解包产物 / mod）打包成标准 zip（可分卷），便于汉化补丁分发
patcher export <dir> -o <out.zip> [-split 100M] [-store|-deflate]
```

## 日志

默认每次运行会把日志追加写入 **`patcher.log`**（带时间戳、级别、模块标签），同时也输出到 stderr。可用全局选项控制：

| 全局选项 | 作用 |
| --- | --- |
| `--no-log` | 不写日志文件，仅输出到 stderr |
| `--log <file>` | 改用指定日志文件 |
| `--debug` | 打开 DEBUG 级别（最详细） |
| `--log-level <0-3>` | 设置最低级别：0=DEBUG 1=INFO 2=WARN 3=ERROR |

级别越高越简略，`INFO` 及以上为默认。日志内容含 handler 选择、解包/注入进度与错误码，便于排查。

## 游戏管理

把常用游戏的 `.rpa` 路径登记成**别名**，之后 `unpack` / `build` 等命令既可以吃路径，也可以直接吃别名：

```bash
patcher game add   mygame /path/to/game.rpa "备注"
patcher unpack mygame -o out        # 等价于 unpack /path/to/game.rpa
patcher build  mygame -m mod/ -o p.rpa
```

登记表保存在本地（`$RENPY_PATCHER_HOME` 或 `~/.renpy_patcher/games.txt`，Windows 在 `%APPDATA%/renpy_patcher/games.txt`），为纯文本，无需数据库。


## 来源声明

当你用本工具注入/汉化游戏后，可以给包里写入一段「由 Ren'Py Patcher 开源项目注入/汉化」的声明文字，方便玩家追溯补丁来源：

- **自动**：`patcher build` 直接注入时，会**自动**把声明文件 `ZZ_RENPY_PATCHER_CREDIT.txt` 烘焙进输出包。
- **手动**：对已注入/已有的 `.rpa`，用 `patcher credit <包> -o out.rpa` 追加声明（解包→加文件→重打包）。
- **仅导出文字**：`patcher credit --text [--game <名>]` 把声明文字打印到终端，便于复制到其他说明里。

声明内容含项目名、仓库地址、版本、非商业许可证与生成时间（中英文双语）。


## 导出（分发补丁）

把解包产物、mod 目录等打包成**标准 `.zip`**，方便分发给玩家（无需 .rpa 工具即可解压）：

```bash
patcher export <dir> -o patch.zip            # 默认 deflate 压缩
patcher export <dir> -o patch.zip -store      # 仅存储不压缩
patcher export <dir> -o patch.zip -split 100M  # 分卷：生成 .z01 .z02 ... .zip
```

要点：
- 产出标准 ZIP，可被系统解压工具 / Python `zipfile` 正常读取。
- `-split <size>` 按固定字节分卷（支持 K/M/G），按 `cat *.z* out.zip` 顺序拼接即可还原。
- 底层为自研 `util/zip`（仅依赖 zlib，无第三方库），与项目零运行时依赖理念一致。


## GUI 版（Windows / .NET WinForms）

除命令行外，本项目另提供 **GUI 版**（仅 Windows，C++/CLI + .NET WinForms），与 CLI **功能完全对齐**，共用同一套核心 C 实现（同一个 `renpy_core` 静态库）：

- 游戏管理：登记 / 删除 / 列出别名
- 解包 / 注入（build，自动写来源声明）/ 来源声明（credit）/ 导出（export，可分卷）

GUI、核心库与 CLI 由同一个 `CMakeLists.txt` 统一构建；zlib 以源码形式内置在 `third_party/zlib`，**无需 vcpkg / 系统 zlib / 联网**。

### 稳定性（v1.0.0 重点）
- **报错显示**：操作失败时，日志框显示「错误码 + 中文含义 + 底层详情」，例如
  `[解包] 失败：错误码 2 (I/O 错误（文件打开/读取/写入失败）) | 详情: 无法打开文件 xxx`。
- **防崩溃**：所有按钮事件均包 `try/catch`（含底层异常兜底），遇到异常只写日志不弹系统报错窗。
- **输入校验**：执行前先检查文件/目录是否存在，避免无效调用。
- **日志同步**：日志框增量同步 `patcher.log`，启动即载入历史记录。

### 构建 GUI（Windows）
1. 安装 Visual Studio 2019/2022（「使用 C++ 的桌面开发」+「C++/CLI 支持」+「.NET Framework 4.8 开发工具」）。
2. 用 CMake 配置并构建（自动产出 `patcher_gui.exe`，同时构建 CLI 与内置 zlib）：
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
   也可在 VS 中「打开 CMake 项目」。
3. 输出：`build/Release/patcher_gui.exe`（同目录另有 `patcher.exe`）。

> 说明：GUI 复用同一核心库；热注入 `agent.dll` 仅在装有 Python 开发包时一并构建，GUI 本身不依赖 Python。


## 版本管理

- **单一事实来源**：根目录 `VERSION` 文件（语义化版本，如 `1.0.0`）。
- **自动生成**：CMake 读取 `VERSION` 并捕获 git 短提交，生成 `core/version.h`（被 `core/plugin.h` 包含），无需手工改版本号。
- **查看版本**：`patcher --version`（或 `-v`）打印版本号、构建来源 commit 与许可证。
- **发版方式**：在 GitHub Actions 页面 **手动 Run workflow（workflow_dispatch）** 一键编译并发布 Release；发版 tag 默认自动用 `build-<时间>`，也可手动指定。
  - `v*` 标签**仅作定期备份**用途：推 `v*` 标签只跑构建验证，不会自动发版。
- `core/version.h` 为生成文件，已被 `.gitignore` 忽略，请勿手工提交。


## 测试
见 [`tests/README_test.md`](tests/README_test.md)：包含可直接用 Ren'Py SDK 实测的演示游戏、汉化 mod，以及用 Ren'Py loader 逻辑校验产物的脚本。

本仓库已配置 CI（`build.yml`）：推送 `main` 自动编译验证；正式发版走手动 `workflow_dispatch` 一键编译发布并附各平台产物。

## 进度 / 路线图
| 里程碑 | 内容 | 状态 |
| --- | --- | --- |
| M1 | RPA-3.0 解包 | ✅ 完成 |
| M2 | 原样重打包闭环 | ✅ 完成 |
| M3 | 直接注入（合并 mod → 重打包） | ✅ 完成 |
| M4 | 热注入代码（Windows 进程注入 + CPython 桥） | ✅ 代码完成（待 Windows 实机验证） |
| M5 | 导出 / 分卷（标准 zip，自研 util/zip） | ✅ 完成 |
| GUI | Windows GUI 版（C++/CLI WinForms，与 CLI 功能对齐，CMake 统一构建，防崩溃 + 报错显示） | ✅ 完成（v1.0.0） |
| M7 | 外部插件 DLL 自动加载（注册表入口已预留） | 🔲 未实现 |
| M8 | 翻译流水线 | 🔲 未实现 |
| M9 | APK 支持 | 🔲 暂搁置 |

## 许可证
本项目采用**非商业许可证**（详见 [`LICENSE`](LICENSE)）。个人学习、研究、非商业用途可自由使用与修改；
**禁止任何商业用途**（销售、嵌入商业产品、付费分发等），商业使用须另行书面授权。
`tests/` 下的演示游戏与汉化 mod 示例同样受此约束。
