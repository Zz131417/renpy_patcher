# 更新日志

> 版本号格式 `X.Y.Z`。**正式版**从 `v1.0.0` 起标记发布；此前 `v0.1.x` 系列为预发布（非正式）版本，仅供先行体验与反馈。

## v1.3.0（开发中）

### 文档
- 新增 [MOD.md](doc/MOD.md)：mod 开发 / 调用文档，覆盖**文件级 mod**（build 合并）、**翻译 mod**（translate build 生成 old/new）、**热注入 mod**（hotinject 注入，`mod_main.py` 的 `on_attach()` 生命周期与宿主 Python 访问）三类，含目录结构、调用命令、最小示例与综合汉化示例。
- 文档细分：把 `README.md` 中偏开发向的内容（技术栈 / 项目目录 / 架构 / 构建 / 调试 / CI / 测试 / 许可证）拆分到 [doc/DEVELOP.md](doc/DEVELOP.md)，新增常见问题 [doc/FAQ.md](doc/FAQ.md)；`README.md` 精简为用户向用法总览，顶部加「开发者文档」入口链接。

### 新增 / 变更
- **聊天 / 选择分支解析与导图生成**：新增 `analyze/branch`，解析游戏脚本（.rpy）中的 `label` 与 `menu` 选择分支，构建分支图并导出 **四种格式**：
  - `tree`：文本树（从 `start` 深度优先展开菜单选项与跳转链，最直观）
  - `mermaid`：Mermaid `flowchart`，可直接贴进支持 Mermaid 的 Markdown / 在线渲染
  - `dot`：Graphviz DOT，可用 `dot -Tpng` 等生成图片
  - `json`：结构化数据，供程序/界面二次处理
  - 菜单选项以带文本的边（`|"选项"|`）呈现；`jump`/`call`/`return` 也作为分支边；支持前向 / 跨文件 label 引用。
- **CLI**：`patcher branch <解包目录> -o out [--format mermaid|json|dot|tree]`。
- **GUI**：游戏管理页新增「聊天分支导图」分组框（选目录 + 选格式 + 生成，自动弹窗展示；可另存为文件）。

## v1.2.0

> 发版策略调整：发布一个正式版（如 `v1.2.0`）后，CI 会自动把 `VERSION` 的 minor 段 +1（→`1.3.0`）并推回 `main`，后续自动测试版自然进入新主版本序列（`v1.3.0-betaA`…），不再回退覆盖旧 beta。

### 新增 / 变更
- **导入整个游戏压缩包**：CLI `patcher import <game.zip> -o out_dir [--name 别名]` + GUI 游戏登记，可直接吃整个游戏的 `.zip`，自动解压并定位内部首个 `.rpa`（可选顺手登记别名）。底层新增纯 C 的 `util/zip_extract`（store / deflate，支持分卷）。

- **翻译流水线补齐菜单 / 界面文本**：`translate extract` 现在除对话（say）外，还提取 **菜单选项（menu）** 与 **界面显示文本（ui，含 `text` / `textbutton` / `button` 的 text / `hover` / `idle` / `tooltip` 等）**，JSON 增加 `type` 字段区分（`say` / `menu` / `ui`）。生成翻译 mod 统一用 Ren'Py 的 `old "原文" / new "译文"` 字符串翻译（修复了原本 `translate <lang>:` 缺块名的语法错误），对话、菜单、界面三种文本均可被 Ren'Py 自动匹配替换。
- **导出为 exe**：把目录（游戏 / mod）打成单个 `.exe` 便于分发：
  - 自解压安装包 exe：双击解压到「同目录/同名(去 `.exe`)」文件夹并打开
  - 启动器 exe：解压后自动运行所填启动命令（如 `game.exe`）
  - 底层：`tools/sfx_stub.c`（Windows 原生自解压桩）与目录 zip 拼成单文件；GUI「导出」页新增「导出自解压 exe」「导出启动器 exe」两个按钮，CLI 新增 `export-exe`。生成 exe 仅 Windows 支持（需 `sfx_stub.exe` 桩，随包附带）。
- **GUI 美化**（随本版进入测试序列）：深色侧边导航栏、强调色按钮、圆角分组框、底部状态栏、后台线程不卡界面、项目主页链接、终端风日志框；解包 / 注入可直接选整包 `.zip`。

### 说明
- 生成 exe 仅 Windows 可生成，其他平台执行会提示「仅 Windows 支持」。
- 检查更新、测试版通道、MSI 安装包等沿用 v1.1.0 既定机制。

## v1.1.0

新增 **翻译流水线（M8）**：提取游戏对话 → 批量翻译 → 回填生成 Ren'Py 翻译 mod，再用 `build` 注入游戏。

### 新增
- **CLI `translate` 子命令**（中间交换格式为 JSON，规避 CSV 引号/换行转义坑）：
  - `translate extract <解包目录> -o dialogs.json`：递归提取所有 `.rpy` 的 **对话**（说话人+文本 / 裸对话 / 多行三引号），跳过菜单选项、界面显示语句、赋值与结构行
  - `translate ai <dialogs.json> -o out.json`：调用 **OpenAI 兼容 Chat Completions** 批量翻译（默认目标中文）；需 `RP_AI_API_KEY`，支持 `--ai-base/--ai-key/--ai-model` 或环境变量指定任意兼容服务（DeepSeek / 通义 / 本地 llama.cpp 等）；原文写 `source`、译文写 `target`
  - `translate human <dialogs.json> --lang zh -o mod_dir` / `translate build ...`：把有译文的条目生成 Ren'Py 翻译 mod（`tl/<lang>/*.rpy`，`old "原文"` / `new "译文"`）
- **Windows GUI**：新增「翻译流水线」分组，三个按钮一一对应 CLI 的 extract / ai / build（AI Key/Base/Model 可填）
- **Windows GUI 模块化重构**：改为左侧导航栏 + 三个页面（游戏管理 / 首页 / 设置）。AI 翻译配置移到「设置」页；「首页」提供快速上手指引与版本信息；「游戏管理」承载解包 / 注入 / 来源声明 / 导出 / 翻译 / 日志等全部操作
- **Windows GUI 安装包**：新增 MSI 安装包（`patcher-gui-windows-x64.msi`）。用 WiX Toolset 生成，CI 自动打包 `patcher_gui.exe` + VC++ 运行库，带开始菜单快捷方式与 .NET Framework 4.8 启动校验；安装包随 Release 一并发布
- **人工翻译**也支持：直接编辑 `dialogs.json` 的 `target` 字段即可，不强制走 AI

### 说明
- 第一版只做 **对话翻译**（按需求"对话成功后再做界面/菜单"）；菜单、界面文本翻译后续版本补齐
- **版本与构建策略**：测试版以 `v主版本-beta<字母>` 命名（构建号递增，A/B/C…/Z/AA…）；MSI 安装包版本号采用 `主版本.构建号` 四段格式（如 `1.1.0.123`），由 CI 构建号驱动，供 Windows 安装器做升级/降级比较，无需手工维护第四段
- **检查更新**：GUI 在「设置」页新增「检查更新」按钮，并在启动时自动静默检查。通过对比 GitHub 上 `latest` 分支的 `version.json` 中的**构建号**与本地构建号（`RPATCHER_BUILD_NUM`）判断是否有新版本；发现更新则弹窗提示并可直接跳转下载页
- **更新通道开关**：「设置」页新增「接收测试版更新」勾选框（默认不勾选）。不勾选时，检查更新只提示**正式版**（远端 `prerelease=true` 的测试版不会打扰）；勾选后测试版也会提示。开关状态持久化到 `%APPDATA%/RenPyPatcher/gui.cfg`
- 热注入仍为代码完成待 Windows 实测


## v1.0.0（正式版）

首个稳定发布版。在预发布系列已完成全部核心能力并验证 GUI 稳定性后，定为正式版。

### 包含的能力
- **命令行（CLI）**：游戏管理 / 解包 / 注入（自动写来源声明）/ 来源声明 / 导出（可分卷）
- **Windows GUI**：C++/CLI + .NET WinForms，功能与 CLI 对齐
  - 游戏管理（别名登记表）
  - 解包 / 注入 / 来源声明 / 导出
  - 日志框实时显示操作结果与底层错误原因
- **稳定性**：GUI 全操作 `try/catch` 防崩溃；失败显示「错误码 + 中文含义 + 底层详情」
- **构建**：单一 CMake 工程统一构建核心 C 库 + CLI + GUI；zlib 源码内置 `third_party/zlib`，CI 离线可构建

### 已知范围
- 热注入 `agent.dll` 仍需 Python 开发包才会构建，GUI 不依赖 Python
- 建议先在测试游戏上验证解包 / 注入流程再用于正式工程

---

## v0.1.8（预发布 / 非正式版）

本版聚焦 **GUI 稳定性**：之前 GUI 遇到底层错误会直接弹系统报错窗甚至闪退，现已解决。

### 新增 / 改进
- **报错显示到日志**：操作失败时，日志框会显示「错误码 + 中文含义 + 底层具体原因」，例如：
  `[解包] 失败：错误码 2 (I/O 错误（文件打开/读取/写入失败）) | 详情: 无法打开文件 xxx`
  - 底层 `log_error` 会自动记录最后一条错误详情，GUI 通过 `rp_last_error()` 取回
  - 新增 `rp_strerror()` 把错误码翻译为中文
- **防崩溃**：GUI 全部操作按钮改为 `try/catch` 包裹（含底层异常兜底），遇到异常不再弹出系统错误窗，只在日志框写一行「❌ 异常（已拦截，程序未崩溃）」
- **输入校验**：执行前先检查文件/目录是否存在（`CheckFile` / `CheckDir`），不存在直接提示，避免无效调用
- **日志实时同步**：日志框增量同步 `patcher.log`，启动即载入历史记录（含以往错误）

### 基线
- 沿用 v0.1.7 的 CMake 统一构建（核心 C 库 + CLI + Windows C++/CLI GUI）与 zlib 源码内置，CI 离线可构建

### 已知范围
- 热注入 `agent.dll` 仍需 Python 开发包才会构建，本版 GUI 不依赖 Python
- 为预发布版本，建议先在测试游戏上验证解包/注入流程

---

## v0.1.7（预发布 / 非正式版）

- 首次将 CLI 与 Windows GUI 统一到单个 CMake 工程构建
- zlib 以源码形式内置 `third_party/zlib`，不再依赖 vcpkg / 系统 zlib / 联网
- GUI 功能与 CLI 对齐：游戏管理 / 解包 / 注入（自动写来源声明）/ 来源声明 / 导出
