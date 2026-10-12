#!/usr/bin/env python3
"""生成【真实格式】的 RPA-3.0 资源包，用于测试 patcher 对真实 Ren'Py 游戏的兼容。

格式（与 Ren'Py 引擎完全一致）：
    布局: [24字节头][文件数据…][zlib 压缩的 pickle 索引]
    头:   "RPA-3.0" + prefix(8hex小写) + index_offset(8hex) + "\n"
    索引: { name: [(real_off ^ (prefix<<32), len ^ prefix, prefix), …] }
    索引写在数据之后，头部第三字段 = 索引在文件中的真实偏移。

用法：python3 tests/make_real_rpa.py <game_dir> <out.rpa> [prefix]
"""
import os, sys, zlib, pickle

HEADER_LEN = 24
GAME_DIR = sys.argv[1] if len(sys.argv) > 1 else "tests/sample_game/game"
OUT      = sys.argv[2] if len(sys.argv) > 2 else "tests/sample_game.rpa"
PREFIX   = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x1A2B3C4D

def collect(d):
    out = {}
    for root, _, files in os.walk(d):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, d).replace(os.sep, "/")
            out[rel] = open(full, "rb").read()
    return out

files = collect(GAME_DIR)
print(f"收集到 {len(files)} 个文件: {sorted(files)}")

total = sum(len(b) for b in files.values())
index_off = HEADER_LEN + total

index = {}
off = HEADER_LEN
for name in sorted(files):
    body = files[name]
    index[name] = [(off ^ (PREFIX << 32), len(body) ^ PREFIX, PREFIX)]
    off += len(body)
assert off == index_off, (off, index_off)

comp = zlib.compress(pickle.dumps(index, protocol=2))
with open(OUT, "wb") as f:
    f.write(("RPA-3.0%08x%08x\n" % (PREFIX, index_off)).encode())
    for name in sorted(files):
        f.write(files[name])
    f.write(comp)

print(f"已生成真实格式 RPA: {OUT}")
print(f"  prefix=0x{PREFIX:08x}  index_offset={index_off}  total={os.path.getsize(OUT)} bytes")
