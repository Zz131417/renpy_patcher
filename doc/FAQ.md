# 常见问题 FAQ（Ren'Py Patcher）

普通用法见根目录 [`README.md`](../README.md)；mod 开发见 [`doc/MOD.md`](MOD.md)；开发 / 构建 / 调试见 [`doc/DEVELOP.md`](DEVELOP.md)。

---

## 支持哪些游戏？

支持使用 **Ren'Py 引擎**打包的游戏（资源包为 **RPA-3.0** 格式，`.rpyc` / `.rpa`）。
非 Ren'Py 游戏（如 Unity、RPG Maker、Unreal）不在支持范围。

---

## 热注入（运行时注入）为什么只有 Windows？

热注入依赖 Windows 特有的 `CreateRemoteThread` + `LoadLibrary` DLL 注入，以及定位宿主进程内
CPython 解释器（CPython C-API 桥）。这些机制在非 Windows 平台没有等价实现，因此**热注入仅 Windows 支持**。
如果你想在非 Windows 上改游戏，请用「直接注入（文件级 mod）」或「翻译 mod」，它们是跨平台的。

---

## 导出 exe（自解压 / 启动器）为什么只有 Windows？

生成 exe 需要 Windows 原生**自解压桩** `sfx_stub.exe`（在 CI 的 Windows 上编译，随包附带）。
其他平台没有这个桩，执行 `export-exe` 会提示「仅 Windows 支持」。但**解压 / 运行**生成的 exe 本身只在 Windows 上，
普通 `export`（标准 ZIP）是全平台可用的分发方式。

---

## 翻译 mod 的 `old "原文"` 不生效 / 没被替换？

Ren'Py 的 `old/new` 字符串翻译要求 `old` 与游戏**原文逐字一致**（含标点、空格、大小写）。
排查：
- 确认提取用的就是游戏当前版本文本（重打包/更新后原文可能变了）。
- 确认 `tl/<lang>/` 目录被正确打进游戏包（用 `patcher build ... -m <翻译mod>` 注入）。
- 确认游戏启动语言匹配（`old/new` 只在对应 `lang` 下生效）。

详见 [`doc/MOD.md`](MOD.md) 的「翻译 mod」一节。

---

## AI 翻译需要什么？

需要 OpenAI 兼容接口的 API Key，设环境变量 `RP_AI_API_KEY`：

```bash
RP_AI_API_KEY=sk-xxx patcher translate ai dialogs.json -o dialogs_t.json \
    [--ai-base https://api.openai.com/v1] [--ai-model gpt-4o-mini] [--to Chinese]
```

`--ai-base` 可指向任意 OpenAI 兼容服务（如自建网关）。不填 Key 也能用「人工翻译」：
直接编辑 `dialogs.json` 里每行的 `target` 字段即可。

---

## 游戏别名（game add）存在哪里？

登记表是本地纯文本文件：
- Linux：`$RENPY_PATCHER_HOME` 或 `~/.renpy_patcher/games.txt`
- Windows：`%APPDATA%/renpy_patcher/games.txt`

无需数据库。可用 `patcher game list` 查看、`patcher game remove` 注销。

---

## 直接导入整个游戏压缩包（import）怎么找到 .rpa？

`patcher import game.zip` 会解压压缩包，在其中找到**第一个** `.rpa` 资源包并登记成别名。
如果游戏把资源拆成多个 `.rpa`（如 `game.rpa` / `audio.rpa`），import 只登记第一个；
其余的需手动 `game add` 或用 `unpack` 分别处理。

---

## 分支导图（branch）解析不到我的选择？

`branch` 解析的是 **`.rpy` 源码**里的 `label` 与 `menu`。如果游戏只发了 `.rpyc`（编译字节码），
没有源码，`branch` 无法解析（同翻译提取一样，`.rpyc` 没有可读文本）。需先拿到 `.rpy` 源码。

---

## 版本号 / 测试版 / 正式版怎么管理？

- 版本号来自根目录 `VERSION` 文件，CMake 自动生成 `core/version.h`。
- 推送 `main`（根目录无 `.release_stable`）→ 自动发**测试版**，tag 形如 `V1.4.148`（`V主版本.次版本.构建号`，构建号 = GitHub 运行序号），`version.json` 的 `version` 为 `1.4.148`、`prerelease=true`。
- 根目录放 `.release_stable` 标记并 bump `VERSION` 后推送 → 发**正式版**（`vX.Y.Z`，非 prerelease）。
- 测试改由独立的手动 `Test` 工作流跑（`.github/workflows/test.yml`，`workflow_dispatch`），不随 push 自动发版。

详细见 [`doc/DEVELOP.md`](DEVELOP.md) 的「版本与发布 / CI」。

---

## 可以商用吗？

**不可以**。本项目采用非商业许可证，个人学习 / 研究 / 非商业用途可自由使用与修改；
任何商业用途需单独书面授权。详见 [`LICENSE`](../LICENSE)。

---

## 报错看不懂怎么办？

运行加 `--debug` 看详细日志，再读 `patcher.log`。日志里错误以
「错误码 + 中文含义 + 底层详情」呈现，对照开发文档里的 `RP_ERR_*` 枚举即可定位。
GUI 版会在日志框直接显示同样信息，不用切终端。
