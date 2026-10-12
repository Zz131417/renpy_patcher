#!/usr/bin/env python3
"""用 Ren'Py loader 的读取逻辑验证某个 .rpa 能否被真实游戏加载。

等效于 renpy/loader.py 中 RPAFile.load()：读头部 → 跳到 index_offset →
读至文件末尾 → zlib 解压 → pickle.loads → 按 (off^prefix, len^prefix) 取数据。

用法：python3 tests/verify_rpa.py <file.rpa> [对比目录]
  若给对比目录，逐文件对拍是否一致（验证直接注入结果 = 原游戏 + mod）。
"""
import os, sys, zlib, pickle, struct

def read_rpa(path):
    raw = open(path, "rb").read()
    assert raw[:7] == b"RPA-3.0", "不是 RPA-3.0"
    prefix = int(raw[7:15], 16)
    index_off = int(raw[15:23], 16)
    idx = pickle.loads(zlib.decompress(raw[index_off:]))   # 读到文件末尾
    result = {}
    for name, blocks in idx.items():
        buf = bytearray()
        for off, length, pfx in blocks:
            real_off = off ^ (pfx << 32)
            real_len = length ^ pfx
            buf += raw[real_off:real_off + real_len]
        result[name] = bytes(buf)
    return result

if __name__ == "__main__":
    rpa = sys.argv[1]
    out = read_rpa(rpa)
    print(f"[Ren'Py 模拟读取] {rpa}")
    print(f"  索引条目: {len(out)}")
    for n in sorted(out):
        print(f"    - {n}  ({len(out[n])} bytes)")

    if len(sys.argv) > 2:
        d = sys.argv[2]
        ok = True
        for root, _, files in os.walk(d):
            for f in files:
                full = os.path.join(root, f)
                rel = os.path.relpath(full, d).replace(os.sep, "/")
                if rel not in out:
                    print(f"  [缺失] {rel}"); ok = False
                elif out[rel] != open(full, "rb").read():
                    print(f"  [不一致] {rel}"); ok = False
        print("  与目录对比:", "PASS" if ok else "FAIL")
