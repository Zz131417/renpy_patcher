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
- **Windows GUI**：C# / WPF（.NET 8），通过 `patcher_core.dll` 调用核心库，功能与 CLI 对齐；底部三页导航（游戏管理 / 主页 / 设置）、操作前校验输入、失败在日志区显示「错误码 + 中文含义 + 底层详情」、后台线程执行不卡界面。
- **内置更新检查**：GUI 设置页可一键检查更新（按构建号比较），并可选接收测试版通道。
- **整包导入**：直接吃整个游戏的 `.zip` 压缩包，自动解压并定位内部 `.rpa`（CLI `import` / GUI 游戏登记）。
- **导出为 exe**：把目录（游戏 / mod）打成自解压安装包，或注入启动命令生成「启动器 exe」（仅 Windows 可生成；纯解压 / 启动器两种模式）。
- **重打包 APK**：基于已有 Ren'Py 安卓包，解开 → 注入 mod → 4 字节对齐重打包为未签名 APK，便于用 `apksigner` 自签后安装（CLI + GUI）。

## 开发者文档

技术栈、项目目录、架构、构建、调试、CI、测试与许可证等开发向内容，见 [doc/DEVELOP.md](doc/DEVELOP.md)；常见问题见 [doc/FAQ.md](doc/FAQ.md)。

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

### 重打包 APK（注入 mod）

> 面向**已有 Ren'Py 安卓包**：解开 APK → 把 mod 目录按 APK 根路径合并（如 `assets/` 下的游戏资源）→
> **4 字节对齐**重打包成**未签名 APK**。Android 要求签名才能安装，因此最后请你用 Android SDK 自签：

```bash
# 基于已有 apk 重打包（注入 mod 目录），默认移除旧 META-INF 签名
patcher export-apk game.apk -m my_mod -o out.apk

# 如需保留原签名（不推荐，重打包后签名本就失效）加 --keep-sig
patcher export-apk game.apk -m my_mod -o out.apk --keep-sig

# 自签（需要 Android SDK 的 apksigner 与你自己的 keystore）
apksigner sign --ks my.keystore out.apk
```

要点：
- mod 目录路径**相对 APK 根**：想替换 `assets/game/script.rpy`，就在 mod 根放 `assets/game/script.rpy`；新文件自动追加。
- 输出做了 **4 字节对齐**（对应 `zipalign 4`），满足 APK 安装要求。
- 本项目零密码学依赖，**不内置签名**，`--keep-sig` 仅用于特殊场景；常规流程请自行 `apksigner` 签名。
- 仅处理**资源层**，不生成 `classes.dex` / `lib/` / `AndroidManifest.xml` 等运行时文件——请基于完整可运行的 Ren'Py 安卓包操作。

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

## 聊天分支导图（解析 label / menu 选择分支）

把游戏脚本里的 `label` 与 `menu` 选择分支解析成一张「分支图」，导出多种格式，方便梳理剧情走向 / 做聊天 AVG 的路线总览：

```bash
# 解析 <解包目录> 下所有 .rpy，生成分支导图
# 格式：tree（文本树）| mermaid（flowchart）| dot（graphviz）| json（结构化）
patcher branch <解包目录> -o branch.mmd --format mermaid
patcher branch <解包目录> -o branch.txt --format tree
```

- **菜单选项**以带文本的边呈现（`A -->|"去森林"| 森林`），`jump` / `call` / `return` 也作为分支边；支持前向 / 跨文件 label 引用。
- `mermaid` 可直接贴进支持 Mermaid 的 Markdown 或在线编辑器渲染；`dot` 可用 `dot -Tpng branch.dot -o branch.png` 出图。
- GUI「游戏管理 → 聊天分支导图」：选目录、选格式、点「生成导图」即弹窗展示，可再「保存到文件」。

要点：
- 中间格式为 **JSON**（`file/line/speaker/source/target`），规避 CSV 引号与换行的转义坑。
- 提取 **对话（say）+ 菜单（menu）+ 界面（ui）** 三类可翻译文本：
  - 对话：说话人+文本、裸对话、多行三引号均可；
  - 菜单：menu 选项文本（`"选项":` / `"选项" if ...:`）；
  - 界面：screen 内显示文本（`text` / `textbutton` / `button` 的 text / `hover` / `idle` / `tooltip` 等）。
- 三类统一用 Ren'Py 的 **old/new 字符串翻译**回填（`translate <lang> <block>:` 块），Ren'Py 自动匹配替换对话、菜单选项与界面显示文本。
- Ren'Py 会优先加载 `tl/<lang>/` 下的译文覆盖原文，无需重排索引。

## GUI 版（Windows / .NET 8 WPF）

除命令行外，本项目另提供 **GUI 版**（仅 Windows，C# + WPF / .NET 8），与 CLI **功能完全对齐**：核心 C 库被编成 `patcher_core.dll`，GUI 通过 P/Invoke 调用，**CLI 与 GUI 共用同一份核心代码**，逻辑完全一致。

- 游戏管理：登记 / 删除 / 列出别名
- 解包 / 注入（build，自动写来源声明）/ 来源声明（credit）/ 导出（export，可分卷）
- 解包 / 注入**可直接选整个游戏的 .zip 压缩包**（自动解压定位内部 .rpa）；游戏登记也支持整包导入
- 导出页新增「导出自解压 exe」「导出启动器 exe」（填启动命令），导出为单文件 exe 便于分发
- 翻译流水线：提取对话 / AI 翻译 / 生成翻译 mod
- 聊天分支导图：解析 label / menu 选择分支，导出 tree / mermaid / dot / json（CLI + GUI）
- 导出 APK：基于已有 Ren'Py 安卓包重打包注入 mod，生成未签名 APK（需自行 apksigner 签名）
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
1. 安装 [.NET 8 SDK](https://dotnet.microsoft.com/download) 与带「C++ 桌面开发」的 Visual Studio（编译核心 DLL / CLI 需要 MSVC + CMake + zlib）。
2. 用 CMake 构建核心库与 CLI（产出 `patcher_core.dll`、`patcher.exe`，zlib 经 vcpkg 接入）：
   ```bash
   cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DVCPKG_TARGET_TRIPLET=x64-windows-static
   cmake --build build --config Release --target patcher patcher_core
   ```
3. 用 .NET 发布 WPF（自包含，免装运行时）：
   ```bash
   dotnet publish gui_wpf/gui_wpf.csproj -c Release -r win-x64 --self-contained true -o gui_publish
   copy build\bin\Release\patcher_core.dll gui_publish\
   ```
4. 运行 `gui_publish\renpy_patcher_gui.exe`（需同目录有 `patcher_core.dll` 与 `patcher.exe`）。

> 说明：GUI 通过 P/Invoke 调 `patcher_core.dll`；热注入 `agent.dll` 仅在装有 Python 开发包时一并构建，GUI 本身不依赖 Python。

### 检查更新
- GUI 在启动时自动静默检查一次，发现新版本才弹窗提示；也可在「设置」页手动点「检查更新」。
- 比较依据：拉取 GitHub `latest` 分支的 `version.json` 中的**构建号**，与本地构建号比较（`latest > 当前` 即视为有新版本）。
- 「接收测试版更新」开关（默认关）：关闭时只提示正式版；开启后测试版也会提示。开关状态持久化在 `%APPDATA%/RenPyPatcher/gui.cfg`。

## 测试

见 [`tests/README_test.md`](tests/README_test.md)：包含可直接用 Ren'Py SDK 实测的演示游戏、汉化 mod，以及用 Ren'Py loader 逻辑校验产物的脚本。

本仓库已配置 CI（`build.yml`）：推送 `main` 自动编译并发布测试版；正式发版走手动 `workflow_dispatch`。

## 已实现能力
- RPA-3.0 解包 / 原样重打包闭环
- 直接注入（解包 → 合并 mod → 重打包，自动写来源声明）
- 热注入（Windows，运行时进程注入）
- 来源声明（自动 / 手动 / 仅导出文字）
- 分卷 ZIP 导出（自研 `util/zip`，仅依赖 zlib）
- 重打包 APK：基于已有安卓包注入 mod，4 字节对齐重打包（CLI + GUI，需自行签名）
- 翻译流水线（提取对话/菜单/界面文本 → AI / 人工翻译 → 生成 Ren'Py 翻译 mod，统一 old/new 字符串翻译）
- 聊天 / 选择分支解析与导图生成（tree / mermaid / dot / json 四种格式，CLI + GUI）
- Mod 文档（[MOD.md](doc/MOD.md)）：文件级 mod / 翻译 mod / 热注入 mod 的目录结构、调用命令与生命周期
- 游戏别名管理
- Windows GUI（功能对齐 CLI，含检查更新与测试版通道）
- 跨平台 CI 编译与自动 / 手动发布

> 能力范围与最新进展以 [CHANGELOG.md](CHANGELOG.md) 为准。
>
> 开发 / 构建 / 调试 / CI 见 [doc/DEVELOP.md](doc/DEVELOP.md)；常见问题见 [doc/FAQ.md](doc/FAQ.md)。
