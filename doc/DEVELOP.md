# 开发文档（Ren'Py Patcher）

面向开发者：技术栈、项目目录、架构、构建、调试、CI、测试与许可证。
普通用户（怎么用工具）请见根目录 [`README.md`](../README.md)；mod 开发见 [`doc/MOD.md`](MOD.md)。

---

## 技术栈

| 层面 | 选型 |
| --- | --- |
| 核心语言 | **C11**（纯 C，无 C++ / 外部运行时依赖） |
| 压缩 | **zlib**（RPA 内嵌 deflate；自研 `zlib_wrap` 封装 inflate/deflate），源码内置 `third_party/zlib` |
| 序列化 | 自研 **pickle_min**：解析 CPython 协议 2/3/4 的 `pickle`，用于读取与重写 Ren'Py 索引 |
| 资源格式 | **RPA-3.0**：头部 `RPA-3.0` + 8 字节索引偏移 + 4 字节 XOR prefix；索引（pickle）位于数据段之后 |
| 构建系统 | **CMake ≥ 3.17**（核心静态库 `renpy_core` + CLI `patcher` 可执行 + Windows 核心 DLL `patcher_core.dll`）；WPF GUI 在 `gui_wpf/`（.NET 8，单独 `dotnet publish`） |
| 持续集成 | **GitHub Actions**：Ubuntu(gcc) / Windows(MSVC) 双矩阵编译；推送 `main` 自动发测试版，手动 `workflow_dispatch` 发正式版 |
| 热注入（Windows） | DLL 注入（`CreateRemoteThread` + `LoadLibrary`）+ **CPython C-API** 桥（`py_bridge.c` 定位宿主解释器并执行 `mod_main.py`） |
| 扩展机制 | 统一模块契约（`core/plugin.h`）+ 中央注册表（`core/plugin_loader`） |

---

## 项目目录

```
.
├── CMakeLists.txt            # 构建定义：renpy_core 静态库 + patcher 可执行 + patcher_core.dll（Windows 核心 DLL，供 WPF P/Invoke）
├── VERSION                   # 版本号（语义化版本，单一事实来源）
├── CHANGELOG.md              # 更新日志（随发版更新）
├── LICENSE                   # 非商业许可证（禁止商业使用）
├── doc/
│   ├── MOD.md                # Mod 开发 / 调用文档（文件级 / 翻译 / 热注入）
│   ├── DEVELOP.md            # 本文：开发 / 构建 / 调试 / CI
│   └── FAQ.md                # 常见问题
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
│   ├── dir_walk.c/.h        #   目录树遍历（重打包 / mod 扫描）
│   ├── zip.c/.h             #   标准 ZIP 读写（分卷，仅依赖 zlib）
│   ├── zip_extract.c/.h     #   纯 C 解压（store/deflate/分卷，供导入整包与自解压桩复用）
│   ├── sfx.c/.h             #   sfx 拼接（目录 zip + 自解压桩 → 单文件 exe）
│   └── resolve_source.c/.h  #   输入归一化（.rpa / 别名 / 整包 zip → .rpa）
├── compress/
│   └── zlib_wrap.c/.h       # zlib inflate/deflate 封装（带安全上限防卡死）
├── rpa/                     # RPA-3.0 解包与重打包
│   ├── rpa_format.h         #   头 / 块结构定义
│   ├── rpa_v3.c/.h          #   解包 + 索引解析
│   └── rpa_v3_repack.c      #   重打包（数据 + 索引拼装）
├── mod/                     # 统一 mod 模块
│   ├── mod_loader.c         #   目录合并（覆盖 / 新增文件）
│   └── mod_manifest.h       #   mod 元数据 / 契约
├── translate/               # 翻译流水线
│   ├── rpy_dialog.c/.h      #   .rpy 对话 / 菜单 / 界面文本提取
│   ├── json_io.c/.h         #   可翻译文本 JSON 读写
│   ├── ai_translate.c/.h    #   AI 翻译（OpenAI 兼容接口）
│   └── translate_cli.c/.h   #   CLI 分发（extract / ai / human）
├── analyze/                 # 聊天 / 选择分支解析
│   └── branch.c/.h          #   label/menu 解析 → tree/mermaid/dot/json 导图
├── inject/
│   ├── direct/
│   │   └── direct_inject.c/.h  # 直接注入：解包→合并→重打包
│   └── hot/                 # 热注入（运行时进程注入，Windows）
│       ├── agent.c/.h       #   被注入的 DLL 载荷
│       ├── inject_api.c     #   热注入入口（非 Windows 为 stub）
│       └── py_bridge.c/.h   #   定位宿主 CPython 并执行 mod_main.py
├── gui_wpf/                 # Windows GUI（C# + WPF / .NET 8，P/Invoke 调 patcher_core.dll）
│   ├── MainWindow.xaml/cs   #   主窗口：底部三页导航 + 各功能页 + 日志/进度
│   ├── Native.cs           #   P/Invoke 桥接到 patcher_core.dll
│   └── App.xaml/cs         #   应用入口与界面设置加载
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

---

## 架构（统一模块契约）

- 所有能力实现 `core/plugin.h` 定义的统一接口，启动时向 `core/plugin_loader` 中央注册表登记。
- CLI（`cli/main.c`）解析子命令后，从注册表查找对应模块并调用。
- GUI（`gui/MainForm.cpp`）复用同一静态库 `renpy_core`，通过 C 接口调用，与 CLI 共享全部能力。
- 新能力（压缩导出 / 翻译后端 / 外部插件 DLL）只需实现契约并注册即可接入，无需改动核心。

模块类型一览：
- **资源操作**：`rpa/`（解包/重打包）、`mod/`（合并）、`util/zip`、`util/zip_extract`（导入整包）、`util/sfx`（导出自解压）
- **翻译**：`translate/`（提取 → AI/人工 → 生成 mod）
- **分析**：`analyze/`（分支解析）
- **注入**：`inject/direct/`（文件级）、`inject/hot/`（运行时，Windows）

---

## 构建

zlib 以源码形式内置在 `third_party/zlib`，由 CMake `add_subdirectory` 自动构建，
**无需系统 zlib / vcpkg / 联网**。

版本号由根目录 `VERSION` 文件统一管理（语义化版本，如 `1.0.0`），CMake 会据此生成
`core/version.h` 并嵌入 git 短提交信息；`patcher --version` 可查看。

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# 产物：build/patcher（Linux），build/Release/patcher.exe + patcher_core.dll（Windows）；gui_wpf 经 dotnet publish 产出 renpy_patcher_gui.exe
```

### 单文件 gcc 快速编译（开发迭代用）

调试单个模块时，可用单文件 gcc 直接编（仍需先由 CMake 生成 `core/version.h`，并链接系统 `-lz`）：

```bash
# 先生成 version.h
cmake -B build -S . >/dev/null 2>&1

# 编出完整 CLI（含所有核心源）
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -I. \
  core/plugin_loader.c core/game_mgr.c util/*.c compress/*.c rpa/*.c mod/*.c \
  translate/*.c analyze/*.c inject/direct/*.c inject/hot/inject_api.c \
  cli/main.c -lz -o /tmp/patcher_test
```

> 注意：`inject/hot/py_bridge.c` / `agent.c` 是 Windows 专用（依赖 `windows.h` / CPython），
> Linux 单文件编译只编 `inject_api.c`（热注入桩），不要编 `py_bridge.c`/`agent.c`。

---

## 调试

### 日志

运行 patcher 默认把日志追加写入 **`patcher.log`**（带时间戳、级别、模块标签），同时输出 stderr。
全局选项：

| 选项 | 作用 |
| --- | --- |
| `--no-log` | 不写日志文件，仅输出到 stderr |
| `--log <file>` | 改用指定日志文件 |
| `--debug` | 打开 DEBUG 级别（最详细） |
| `--log-level <0-3>` | 最低级别：0=DEBUG 1=INFO 2=WARN 3=ERROR |

排查问题时优先 `patcher --debug <子命令> ...`，再读 `patcher.log`（含 handler 选择、解包/注入进度与错误码）。

### 单文件 gcc 快速迭代

改了某个 `.c` 后，用上面的单文件 gcc 命令直接重编（`-Wall -Wextra` 能暴露未使用变量/函数等），
不必走完整 CMake。CI 行为可用同样命令本地复现。

### Linux 下 GDB 调试

```bash
gcc -std=c11 -g -O0 -D_POSIX_C_SOURCE=200809L -I. <源文件...> -lz -o /tmp/patcher_dbg
gdb --args /tmp/patcher_dbg unpack game.rpa -o out
```

### 常见编译 / 链接问题

1. **Windows GUI 构建报错「未声明的标识符」**
   控件在 `MainForm.cpp` 里用了，但 `MainForm.h` 的控件声明块漏了对应的 `Button^` / `TextBox^` 成员。
   多为 XAML 中引用的控件未在代码隐藏文件声明，或 P/Invoke 委托签名与原生回调不一致（尤其容易漏掉 `void* user` 参数）。核对 `Native.cs` 与 `core_api/core_api.h` 的回调签名。
2. **`core/version.h: No such file`**
   必须先跑一次 `cmake -B build -S .` 生成 `version.h`，单文件 gcc 才能编。
3. **`undefined reference to deflateInit2`**
   单文件 gcc 漏了 `-lz`；或没编进 `compress/zlib_wrap.c`。
4. **分支解析（branch.c）前向引用**
   label 可能后定义（菜单选项 `jump` 到靠后的 label，或跨文件）。实现上分两遍：第一遍收集所有 label、
   记录边上 `to_name`；全部收集完后再把 `to_name` 解析为 label id。否则会出现「跳转目标解析为 -1」。
5. **GUI 热注入 `agent.dll` 未产出**
   仅当装有 Python 开发包（含 `python3x.lib` 与头文件）时一并构建；GUI 本身不依赖它，缺失不影响其他功能。

### 注入 / 解包错误码排查

错误码在日志里以「错误码 + 中文含义 + 底层详情」呈现，例如：
```
[解包] 失败：错误码 2 (I/O 错误（文件打开/读取/写入失败）) | 详情: 无法打开文件 xxx
```
对照 `core/plugin.h` 里的 `RP_ERR_*` 枚举即可定位模块（I/O / 格式 / 内存 / 参数 / 网络）。

---

## CI（GitHub Actions）

配置见 `.github/workflows/build.yml`。

- **矩阵**：Ubuntu(gcc) + Windows(MSVC) 双平台编译；Release job 负责打包与发版。
- **自动测试版**：推送 `main` → 自动编译并发布**测试版**（prerelease，tag 形如 `v1.3.0-betaA`）。
- **手动正式版**：在 Actions 页面 **Run workflow（`workflow_dispatch`）** 并填 tag（如 `v1.2.0`）→ 编译并发布**正式版**（非 prerelease）。
- **发正式版后自动升版**：正式版发布成功后，CI 自动把 `VERSION` 的 minor +1（如 `1.2.0` → `1.3.0`）并推回 `main`，
  后续测试版自然进入新主版本序列，不会回退覆盖旧 beta。
- **产物**：Windows MSI 安装包、Windows / Linux 压缩包、`version.json`（供 GUI 检查更新）。

### 查 CI 失败日志

- 在仓库 Actions 页面点对应 run → 看 job 日志（Windows 编译失败重点看 `##[error]` / `error C`）。
- 也可用 API 拉取某个 job 的原始日志（见各轮调试时的 `actions/jobs/<id>/logs`）。

### 常见 CI 坑

1. **Windows 编译挂**：基本都是 `MainForm.h` 控件声明遗漏（见上「常见编译问题 1」）。本地没法编 Windows GUI 时，
   提交后在 Actions 的 Windows job 日志里搜 `error C` 快速定位。
2. **Git 推送被拒（non-fast-forward）**：`main` 可能在你开发期间被「发版自动升 VERSION」的 commit 超前。
   解决：先 `git fetch origin main`，把你的功能 commit `cherry-pick` 到最新 `FETCH_HEAD` 上再推；
   不要直接 `git pull --rebase`（会带入 CI 自动 commit 造成重复）。

---

## 测试

见 [`tests/README_test.md`](../tests/README_test.md)：包含可直接用 Ren'Py SDK 实测的演示游戏、汉化 mod，
以及用 Ren'Py loader 逻辑校验产物的脚本（`verify_rpa.py`）。

本仓库已配置 CI（`build.yml`）：推送 `main` 自动编译并发布测试版；正式发版走手动 `workflow_dispatch`。

---

## 版本与发布

> 版本号与发布信息集中在少数固定位置管理，本文档**不写死具体版本**。

- **版本号来源**：根目录 `VERSION` 文件（语义化版本）。CMake 读取后生成 `core/version.h`，无需手工改。
- **构建号**：由 CI 自动注入（GitHub 运行序号），用于 MSI 四段版本号与 GUI 检查更新比较；本地未接 CI 时默认为 0。
- **测试版命名**：`v<主版本>-beta<字母>`（字母从 A 递增）；主版本号来自 `VERSION`。
- **发正式版后自动升版**：见上「CI」一节。
- **最新发布 / 产物**：见 [Releases](https://github.com/Zz131417/renpy_patcher/releases)。
- **更新历史**：见 [CHANGELOG.md](../CHANGELOG.md)。

---

## 许可证

本项目采用**非商业许可证**：个人学习、研究、非商业用途可自由使用与修改；**禁止任何商业用途**，
商业使用需单独书面授权。详见 [`LICENSE`](../LICENSE)。
