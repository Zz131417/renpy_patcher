# 直接注入 测试套件说明

> 📄 **许可证约束**：本项目（含 `tests/` 下的演示游戏与汉化 mod 示例）采用
> **非商业许可证**（详见仓库根 `LICENSE`）。这些示例**仅供个人学习、研究与非商业用途**，
> **禁止任何商业使用**（销售、嵌入商业产品、付费分发等）。若需商业使用，须另行书面授权。


本目录提供一套**可直接用 Ren'Py 引擎实测**的素材，验证 `patcher build`（直接注入）产出的
`.rpa` 能被真实游戏读取、且中文 mod 生效。

---

## 一、目录结构

```
tests/
├── sample_game/game/        原始演示游戏（英文版，可直接当 game/ 运行）
│   ├── script.rpy           原始剧情（英文）
│   ├── options.rpy          配置
│   └── images/logo.png      原版蓝色 logo（由 gen_demo_assets.py 生成）
├── sample_game.rpa          原始游戏的【真实格式 RPA-3.0】（由 make_real_rpa.py 生成）
│
├── mod_hanzi/               汉化 mod 目录（直接注入的素材）
│   ├── zz_hanzi.rpy         中文剧情脚本（zz_ 前缀确保后加载，覆盖 script.rpy 的 start）
│   └── images/logo.png      汉化红色 logo（替换原 logo）
│
├── sample_game_patched/     ★ 注入产物的解包目录（可直接当 game/ 运行中文版）
├── sample_game_patched.rpa  ★ 注入产物：patcher build 生成，可被 Ren'Py 直接读取
│
├── gen_demo_assets.py       生成演示 PNG（不依赖 PIL）
├── make_real_rpa.py         生成真实格式 RPA（随机 prefix + Ren'Py 写法索引）
├── verify_rpa.py            用 Ren'Py loader 逻辑校验 .rpa 是否合规（不依赖 patcher）
└── README_test.md           本文件
```

---

## 二、用 Ren'Py 引擎实测（最直观）

1. 下载 [Ren'Py SDK](https://www.renpy.org/latest.html) 解压。
2. **测英文原版**：把 `tests/sample_game/game/` 整个目录复制为
   `<renpy>/projects/<任意名>/game/` → 双击 `renpy.exe`（或 `renpy.sh`）打开项目，
   看到英文对话 + 蓝色 logo。
3. **测中文注入版（二选一）**：
   - 方案 A（目录）：把 `tests/sample_game_patched/` 当作 `game/` 目录放进项目 →
     运行看到中文对话 + 红色 logo。
   - 方案 B（rpa）：把 `tests/sample_game_patched.rpa` 放进项目根目录（与 `game/` 同级），
     并把原 `game/` 改名/移走（避免同名冲突）→ 运行看到中文版。

> Ren'Py 同时扫描 `game/` 目录与 `*.rpa`。两者同名文件并存时优先级依版本而定，
> 故测试方案 B 时建议先移走 `game/`。

---

## 三、用 patcher 自己重跑验证（自动化）

```bash
# 0) 编译（或已用 CMake）
gcc -std=c11 -O2 -I. \
  core/plugin_loader.c util/io_buf.c util/pickle_min.c util/pickle_write.c util/dir_walk.c \
  compress/zlib_wrap.c rpa/rpa_v3.c rpa/rpa_v3_repack.c \
  mod/mod_loader.c inject/direct/direct_inject.c inject/hot/inject_api.c cli/main.c \
  -lz -o build/patcher

# 1) 生成演示素材 + 真实格式 RPA
python3 tests/gen_demo_assets.py
python3 tests/make_real_rpa.py tests/sample_game/game tests/sample_game.rpa

# 2) 直接注入：sample_game.rpa + mod_hanzi/ → my_patched.rpa
./build/patcher build tests/sample_game.rpa -m tests/mod_hanzi -o my_patched.rpa

# 3) 用 Ren'Py 读取逻辑校验产物合规（不依赖 patcher）
python3 tests/verify_rpa.py my_patched.rpa tests/sample_game/game
# 期望：索引条目=4，且与「原 game 目录 + mod 目录」合并结果逐字节一致

# 4) 解包看一下
./build/patcher unpack my_patched.rpa -o my_out
# my_out/zz_hanzi.rpy 含中文，my_out/images/logo.png 是红色汉化版
```

---

## 四、验证结论（沙箱内已自动跑过）

- `patcher unpack` 解真实格式 RPA（随机 prefix）内容**未错位** ✅
- `patcher build` 注入后：`zz_hanzi.rpy`（中文）进包、`images/logo.png` 被替换 ✅
- `verify_rpa.py`（等效 Ren'Py loader 逻辑）对拍 4 文件**逐字节一致** ✅
- 产物 `sample_game_patched.rpa` 可被 Ren'Py 引擎读取（格式合规）✅

> 因沙箱无 Ren'Py 运行时，最终「画面显示中文」需在 Windows/macOS 的 Ren'Py SDK 上目视确认；
> 文件级与格式级正确性已由上述自动化验证保证。
