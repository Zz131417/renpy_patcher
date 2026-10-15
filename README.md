# Ren'Py Patcher

> 🌐 English documentation: [README.en.md](README.en.md)

> 📄 **许可证**：本项目采用**非商业许可证**（详见 `LICENSE`）。个人学习、研究、非商业用途可自由使用与修改；**禁止任何商业用途**，商业使用需单独书面授权。

> 📌 **版本**：最新正式版 / 测试版见 [Releases](https://github.com/Zz131417/renpy_patcher/releases)；版本号由根目录 [`VERSION`](VERSION) 文件统一管理。完整更新历史见 [CHANGELOG.md](CHANGELOG.md)（本说明文档不随版本号变化，无需每次发版修改）。

面向 Ren'Py 游戏（Galgame / 视觉小说）的**汉化与 Mod 补丁工具**。

纯 C 核心引擎负责资源包（RPA）的解析与重写，配合统一模块契约，可扩展支持多种注入方式、翻译流水线与压缩导出；当前已内置对话翻译流水线（extract → ai/human → build mod）。当前已具备「解包 → 重打包 → 直接注入 → 热注入（Windows）」完整链路，并提供与 CLI 功能对齐的 Windows GUI。

## 特性
- **RPA-3.0 双向闭环**：解包与重打包字节级兼容 Ren'Py 引擎（索引写在数据之后，头部记录索引偏移）。
- **直接注入**：把汉化/Mod 资源合并进游戏包后重打包，产出可被真实游戏直接读取的补丁。
- **热注入（Windows）**：运行时向游戏进程注入 DLL，定位 CPython、激活游戏内 Python 加载 `mod_main.py`，零改文件。
- **对话翻译流水线**：提取游戏对话 → AI / 人工翻译 → 一键生成 Ren'Py 翻译 mod，免去手工改每个脚本。
- **可扩展架构**：统一模块注册表（`core/plugin_loader`），新能力（压缩导出 / 翻译后端 / 外部插件）即插即用。
- **Windows GUI**：C++/CLI + .NET WinForms，复用同一核心库，功能与 CLI 对齐；操作前校验输入、失败在日志框显示「错误码 + 中文含义 + 底层详情」、全局 `try/catch` 防崩溃。
- **内置更新检查**：GUI 设置页可一键检查更新（按构建号比较），并可选接收测试版通道。
- **整包导入**：直接吃整个游戏的 `.zip` 压缩包，自动解压并定位内部 `.rpa`（CLI `import` / GUI 游戏登记）。
- **导出为 exe**：把目录（游戏 / mod）打成自解压安装包，或注入启动命令生成「启动器 exe」（仅 Windows 可生成；纯解压 / 启动器两种模式）。

## 技术栈
| 层面 | 选型 |
| --- | --- |
| 核心语言 | **C11**（纯 C，无 C++ / 外部运行时依赖） |
| 压缩 | **zlib**（RPA 内嵌 deflate；自研 `zlib_wrap` 封装 inflate/deflate），源码内置 `third_party/zlib` |
| 序列化 | 自研 **pickle_min**：解析 CPython 协议 2/3/4 的 `pickle`，用于读取与重写 Ren'Py 索引 |
| 资源格式 | **RPA-3.0**：头部 `RPA-3.0` + 8 字节索引偏移 + 4 字节 XOR prefix；索引（pickle）位于数据段之后 |
| 构建系统 | **CMake ≥ 3.17**（核心静态库 `renpy_core` + CLI `patcher` 可执行 + Windows GUI `patcher_gui`）；zlib 源码内置，无 vcpkg / 系统 zlib |
| 持续集成 | **GitHub Actions**：Ubuntu(gcc) / Windows(MSVC) 双矩阵编译；推送 `main` 自动发测试版，手动 `workflow_dispatch` 发正式版 |
| 热注入（Windows） | DLL 注入（`CreateRemoteThread` + `LoadLibrary`）+ **CPython C-API** 桥（`py_bridge.c` 定位宿主解释器并执行 `mod_main.py`） |
| 扩展机制 | 统一模块契约（`core/plugin.h`）+ 中央注册表（`core/plugin_loader`） |

## 项目目录
```
.
├── CMakeLists.txt            # 构建定义：renpy_core 静态库 + patcher 可执行 + patcher_gui（Windows GUI）
├── VERSION                   # 版本号（语义化版本，单一事实来源）
├── CHANGELOG.md              # 更新日志（随发版更新，本 README 不依赖它）
├── LICENSE                   # 非商业许可证（禁止商业使用）
├── DESIGN.md                 # 架构设计与里程碑说明
├── .github/workflows/
│   └── build.yml            # CI：Linux/Windows 编译；推送 main 自动发测试版 / 手动发正式版
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
│   ├── MainForm.h/.cpp      #   主窗口与事件（含 try/catch 防崩溃、报错显示、检查更新）
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

# 直接导入整个游戏压缩包（自动解压并定位内部 .rpa；可顺手登记别名）
patcher import <game.zip> -o out_dir [--name 别名]

# 把目录（游戏 / mod）导出为 exe：自解压安装包 或 启动器（仅 Windows 可生成）
patcher export-exe <dir> -o setup.exe [--launcher "<启动命令>"] [--store|--deflate]
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

如果拿到的是**整个游戏的压缩包**（而非单个 `.rpa`），可直接：

```bash
patcher import game_pack.zip -o extracted --name mygame   # 解压并自动登记内部 .rpa
```

`import` 会解压压缩包、在其中找到第一个 `.rpa` 资源包，并把它登记成别名；之后再 `unpack` / `build` 这个别名即可。

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

### 导出为 exe（自解压 / 启动器）

把目录（游戏 / mod）打包成单个 `.exe`，方便分发给玩家（双击即解压，无需额外解压工具）：

```bash
# 自解压安装包：双击解压到「同目录/同名(去.exe)」文件夹，并打开该目录
patcher export-exe game_dir -o setup.exe

# 启动器 exe：解压后自动运行 RP_AUTORUN.txt 里的命令（如启动游戏）
patcher export-exe game_dir -o launcher.exe --launcher "game.exe"
```

- 生成逻辑：本项目的 Windows 原生自解压桩 `sfx_stub.exe` + 目录 zip（`util/zip`）拼成单文件；
  运行时桩解出内置 zip，纯安装包模式弹窗并打开目录，**启动器模式**则自动执行你填的命令。
- **仅 Windows 可生成 exe**（需要 `sfx_stub.exe`，由 CI 在 Windows 上编译并随 GUI / CLI 附带）。其他平台执行会提示「仅 Windows 支持」。
- 自解压桩与解压逻辑均复用项目自研 `util/zip_extract`，零第三方依赖。

## 翻译流水线（对话翻译）

把游戏对话提取出来统一翻译，再生成 Ren'Py 翻译 mod（无需手工改每个脚本）。流程：

```bash
# 1) 解包游戏后，提取全部对话到 JSON（含说话人/原文）
patcher translate extract <解包目录> -o dialogs.json

# 2) 二选一填充译文：
#    a) AI 翻译：调用 OpenAI 兼容接口批量翻译（需 RP_AI_API_KEY），对话/菜单/界面一并发翻
#       RP_AI_API_KEY=sk-xxx patcher translate ai dialogs.json -o dialogs_t.json \
#           [--ai-base https://api.openai.com/v1] [--ai-model gpt-4o-mini] [--to Chinese]
#    b) 人工翻译：直接编辑 dialogs.json 里每行的 "target" 字段（JSON 含 type 字段标明 say/menu/ui）
#
# 3) 生成翻译 mod（tl/<lang>/*.rpy）
patcher translate human dialogs_t.json --lang zh -o translation_mod

# 4) 注入游戏：把 translation_mod 当普通 mod 直接注入
patcher build <游戏.rpa|别名> -m translation_mod -o patched.rpa
```

要点：
- 中间格式为 **JSON**（`file/line/speaker/source/target`），规避 CSV 引号与换行的转义坑。
- 提取 **对话（say）+ 菜单（menu）+ 界面（ui）** 三类可翻译文本：
  - 对话：说话人+文本、裸对话、多行三引号均可；
  - 菜单：menu 选项文本（`"选项":` / `"选项" if ...:`）；
  - 界面：screen 内显示文本（`text` / `textbutton` / `button` 的 text / `hover` / `idle` / `tooltip` 等）。
- 三类统一用 Ren'Py 的 **old/new 字符串翻译**回填（`translate <lang> <block>:` 块），Ren'Py 自动匹配替换对话、菜单选项与界面显示文本。
- Ren'Py 会优先加载 `tl/<lang>/` 下的译文覆盖原文，无需重排索引。

## GUI 版（Windows / .NET WinForms）

除命令行外，本项目另提供 **GUI 版**（仅 Windows，C++/CLI + .NET WinForms），与 CLI **功能完全对齐**，共用同一套核心 C 实现（同一个 `renpy_core` 静态库）：

- 游戏管理：登记 / 删除 / 列出别名
- 解包 / 注入（build，自动写来源声明）/ 来源声明（credit）/ 导出（export，可分卷）
- 解包 / 注入**可直接选整个游戏的 .zip 压缩包**（自动解压定位内部 .rpa）；游戏登记也支持整包导入
- 导出页新增「导出自解压 exe」「导出启动器 exe」（填启动命令），导出为单文件 exe 便于分发
- 翻译流水线：提取对话 / AI 翻译 / 生成翻译 mod
- 检查更新：设置页可一键检查，按构建号与 GitHub 最新构建比较；「接收测试版更新」开关可切换是否提示测试版

GUI、核心库与 CLI 由同一个 `CMakeLists.txt` 统一构建；zlib 以源码形式内置在 `third_party/zlib`，**无需 vcpkg / 系统 zlib / 联网**。

### 稳定性设计
- **报错显示**：操作失败时，日志框显示「错误码 + 中文含义 + 底层详情」，例如
  `[解包] 失败：错误码 2 (I/O 错误（文件打开/读取/写入失败）) | 详情: 无法打开文件 xxx`。
- **防崩溃**：所有按钮事件均包 `try/catch`（含底层异常兜底），遇到异常只写日志不弹系统报错窗。
- **输入校验**：执行前先检查文件/目录是否存在，避免无效调用。
- **日志同步**：日志框增量同步 `patcher.log`，启动即载入历史记录。
- **不卡界面**：文件操作在后台线程执行，界面在长时间任务期间仍可用。

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

### 检查更新
- GUI 在启动时自动静默检查一次，发现新版本才弹窗提示；也可在「设置」页手动点「检查更新」。
- 比较依据：拉取 GitHub `latest` 分支的 `version.json` 中的**构建号**，与本地构建号比较（`latest > 当前` 即视为有新版本）。
- 「接收测试版更新」开关（默认关）：关闭时只提示正式版；开启后测试版也会提示。开关状态持久化在 `%APPDATA%/RenPyPatcher/gui.cfg`。

## 版本与发布

> 本项目的版本号与发布信息集中在少数固定位置管理，本文档**不写死具体版本**，发版时无需改动 README。

- **版本号来源**：根目录 `VERSION` 文件（语义化版本，如 `1.1.0`）。CMake 读取后生成 `core/version.h`（被 `core/plugin.h` 包含），无需手工改版本号。
- **构建号**：由 CI 自动注入（GitHub 仓库运行序号），用于 MSI 安装包的四段版本号（供安装器做升级/降级比较）以及 GUI 检查更新时的版本比较。本地未接入 CI 时构建号默认为 0。
- **测试版命名**：自动发版采用 `v<主版本>-beta<字母>` 形式（字母从 `A` 开始依次递增，如 `betaA` / `betaB` / …）；主版本号来自 `VERSION` 文件。
- **发正式版后自动升版**：手动发布一个正式版（workflow_dispatch，tag 形如 `vX.Y.Z` 且 `prerelease=false`）后，CI 会自动把 `VERSION` 的 **minor 段 +1**（如 `1.2.0` → `1.3.0`）并推回 `main`；于是后续 `push main` 产生的测试版会自动进入新的主版本序列（`v1.3.0-betaA`…），不会回退覆盖旧 beta。无需手工维护下个开发版本号。
- **发布方式**：
  - 推送 `main` 分支 → 自动编译并发布**测试版**（预发布）。
  - 在 GitHub Actions 页面 **手动 Run workflow（`workflow_dispatch`）** 并指定 tag（如 `v1.1.0`）→ 编译并发布**正式版**（非预发布）。
- **最新发布 / 产物**：见 [Releases](https://github.com/Zz131417/renpy_patcher/releases)，产物含 Windows MSI 安装包、Windows / Linux 压缩包与 `version.json`。
- **更新历史**：见 [CHANGELOG.md](CHANGELOG.md)。

## 测试
见 [`tests/README_test.md`](tests/README_test.md)：包含可直接用 Ren'Py SDK 实测的演示游戏、汉化 mod，以及用 Ren'Py loader 逻辑校验产物的脚本。

本仓库已配置 CI（`build.yml`）：推送 `main` 自动编译并发布测试版；正式发版走手动 `workflow_dispatch`。

## 已实现能力
- RPA-3.0 解包 / 原样重打包闭环
- 直接注入（解包 → 合并 mod → 重打包，自动写来源声明）
- 热注入（Windows，运行时进程注入）
- 来源声明（自动 / 手动 / 仅导出文字）
- 分卷 ZIP 导出（自研 `util/zip`，仅依赖 zlib）
- 翻译流水线（提取对话/菜单/界面文本 → AI / 人工翻译 → 生成 Ren'Py 翻译 mod，统一 old/new 字符串翻译）
- 游戏别名管理
- Windows GUI（功能对齐 CLI，含检查更新与测试版通道）
- 跨平台 CI 编译与自动 / 手动发布

> 能力范围与最新进展以 [CHANGELOG.md](CHANGELOG.md) 为准。

## 许可证
本项目采用**非商业许可证**：个人学习、研究、非商业用途可自由使用与修改；**禁止任何商业用途**，商业使用需单独书面授权。详见 [`LICENSE`](LICENSE)。
