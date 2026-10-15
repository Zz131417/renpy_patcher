# Mod 文档（Ren'Py Patcher）

本文说明 Ren'Py Patcher 支持的三类 mod：**文件级 mod**、**翻译 mod**、**热注入 mod（hot mod）**——
它们的目录结构、怎么被 patcher 调用（应用），以及 mod 在运行时能调用（访问）什么。

---

## 0. 三种 mod 一览

| 类型 | 作用 | 应用方式 | 平台 | 是否改文件 |
| --- | --- | --- | --- | --- |
| 文件级 mod | 把补丁文件合并进游戏包 | `patcher build -m <dir>` | 全平台 | 写进 .rpa |
| 翻译 mod | 用 `old/new` 替换游戏文本 | `translate build` 生成 → 随文件级 mod 注入 | 全平台 | 写进 .rpa |
| 热注入 mod | 运行时在游戏 Python 里执行代码 | `patcher hotinject --pid <pid> --mod <dir>` | 仅 Windows | 零改文件 |

---

## 1. 文件级 mod（file-level mod）

最通用的 mod：一个**目录**，里面放你想替换或新增到游戏 `game/` 下的文件。patcher 在「直接注入」时
把这个目录合并进游戏资源包，**同名文件覆盖、不存在则新增**。

### 目录结构示例

```
my_mod/
├── script.rpy          # 覆盖原 game/script.rpy
├── images/
│   └── logo.png        # 新增/替换图片
└── tl/
    └── zh/
        └── script.rpy  # 翻译 mod（见第 2 节），也可直接打进 mod 目录
```

### 调用（应用）方式

```bash
# 直接注入：解包 game.rpa → 合并 my_mod/ → 重打包为 patched.rpa
# （会自动写入来源声明 ZZ_RENPY_PATCHER_CREDIT.txt）
patcher build <game.rpa|别名> -m my_mod -o patched.rpa
```

合并规则：
- 遍历 `my_mod/` 下所有文件，按相对路径拷到游戏目录
- 已存在的文件被覆盖，新文件追加
- 不删除游戏原有文件

> 适合：汉化补丁、贴图替换、新增 `.rpy` 脚本、UI 调整等任何「文件级」改动。

---

## 2. 翻译 mod（translation mod）

由 patcher 的翻译流水线生成（也可用 Ren'Py 原生方式手写），放进游戏的 `tl/<lang>/` 目录，
通过 `translate <lang> <block>:` + `old "原文" / new "译文"` 字符串翻译机制替换文本。

### 由 patcher 生成（推荐）

```bash
# 1) 提取可翻译文本（对话/菜单/界面）到 JSON
patcher translate extract <解包目录> -o dialogs.json

# 2) 填充译文：用 AI 或人工编辑 dialogs.json 每行的 "target" 字段
#    （JSON 含 "type" 字段标明 say / menu / ui）

# 3) 生成翻译 mod 目录（含 tl/<lang>/）
patcher translate build dialogs.json --lang zh -o my_trans_mod
```

生成的目录结构（可直接作为文件级 mod 注入）：

```
my_trans_mod/
└── tl/
    └── zh/
        └── game_script.rpy     # translate zh <block>: / old "..." / new "..."
```

### 手写翻译 mod

也可以完全手写，遵循 Ren'Py 约定：

```renpy
# my_trans_mod/tl/zh/common.rpy
translate zh start_block:
    old "Hello, world."
    new "你好，世界。"
```

> 对话、菜单选项、界面显示文本（text / textbutton / button 等）都会被 `old/new` 匹配替换。
> 详见 `README.md` 的「聊天分支导图 / 翻译流水线」章节。

---

## 3. 热注入 mod（hot mod / runtime mod）

**运行时**向正在玩的游戏进程注入代码，在**宿主游戏的 CPython 解释器**里执行你的 Python。
零改文件、可访问游戏全部运行时状态（存档、角色对象、screen 等），适合调试、实时改文本、动态打补丁。

> ⚠️ 仅 Windows 支持（依赖 `CreateRemoteThread` + `LoadLibrary` DLL 注入 + CPython C-API 桥）。

### 目录结构（必须含 `mod_main.py`）

```
my_hot_mod/
└── mod_main.py        # 入口，定义 on_attach()
```

### 生命周期与可调用的钩子

patcher 注入后会自动执行一段 bootstrap：

```python
import sys
_mp = r'<mod_dir 绝对路径>'
if _mp not in sys.path:
    sys.path.insert(0, _mp)
try:
    import mod_main
    if hasattr(mod_main, 'on_attach'):
        mod_main.on_attach()     # ← 你的入口
except Exception as _e:
    sys.stderr.write('hot mod load error: %r\n' % _e)
```

**你能调用的：**
- `on_attach()`：注入成功后自动调用，**写你的全部逻辑在这里**
- `sys.modules` / `renpy` / 游戏自己的模块：因为运行在宿主解释器里，可以直接 import 游戏里已加载的模块、访问全局变量
- 例：实时改某角色名字、打印当前存档、hook 某个函数

### 最小示例 `mod_main.py`

```python
def on_attach():
    # 运行在游戏进程的 Python 里，可访问游戏全部状态
    print("[hot mod] 已附加到游戏进程")

    # 例：把当前界面标题改成中文（实际按游戏对象名调整）
    # import renpy
    # renpy.store.title = "我的汉化版"

    # 例：调用游戏里已加载的函数
    # import store  # 视游戏而定
```

### 调用（注入）方式

```bash
# 1) 先找到游戏进程 PID（任务管理器 / 进程列表）
# 2) 注入
patcher hotinject --pid 12345 --mod my_hot_mod
```

注入成功后，游戏的 Python 会执行 `my_hot_mod/mod_main.py` 的 `on_attach()`。
可在游戏控制台 / 日志里看到 `[hot mod] 已附加到游戏进程` 之类的输出（stdout 走宿主进程）。

> 注意：
> - 必须等游戏**启动并初始化 Python 之后**再注入（`Py_IsInitialized` 检查不通过会失败）
> - `mod_main.py` 的 import 错误会被捕获并打印 `hot mod load error: ...`，便于排查
> - 不同 Ren'Py 版本绑定不同 `python3x.dll`，py_bridge 会自动定位宿主解释器

---

## 4. 综合示例：汉化补丁（文件级 + 翻译）

```bash
# 解包
patcher unpack mygame.rpa -o game_unpacked

# 提取文本 → 翻译 → 生成翻译 mod
patcher translate extract game_unpacked -o dialogs.json
# （编辑 dialogs.json 的 target 字段）
patcher translate build dialogs.json --lang zh -o zh_mod

# 再把贴图替换等文件级改动放进同一个目录
cp -r my_assets zh_mod/

# 一步注入：合并 zh_mod → 重打包
patcher build mygame.rpa -m zh_mod -o mygame_zh.rpa
```

---

## 5. 来源声明（credit）

注入/汉化后建议给包标注来源（方便玩家追溯补丁）：

```bash
# 自动：patcher build 直接注入时已自动写入 ZZ_RENPY_PATCHER_CREDIT.txt
# 手动：给已有 .rpa 追加声明
patcher credit <game.rpa|别名> -o credited.rpa
# 仅导出声明文字（自己贴到别处）
patcher credit --text [--game <名>]
```

---

## 6. 给 mod 开发者的提示

- **文件级 mod**：路径相对于 `game/`，想替换 `game/script.rpy` 就在 mod 根放 `script.rpy`
- **翻译 mod**：`old` 字符串必须和游戏原文**逐字一致**（含标点、空格）才能匹配
- **热注入 mod**：`on_attach()` 里不要做阻塞主线程太久的操作；要访问游戏对象需知道其模块/变量名（可先用 `dir()` / 游戏源码确认）
- 调试图文见 `README.md` 的「日志」章节（`patcher.log` 含 handler 选择、解包/注入进度与错误码）
