#!/usr/bin/env python3
"""生成演示用 PNG（不依赖 PIL）：原版蓝色 logo + 汉化红色 logo。"""
import os, struct, zlib

def make_png(path, w, h, rgb, grid=True):
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)  # 8-bit RGB
    raw = b''
    for y in range(h):
        row = bytearray(b'\x00')
        for x in range(w):
            if grid and (x % 32 == 0 or y % 32 == 0):
                row += b'\xff\xff\xff'   # 白网格
            else:
                row += bytes(rgb)
        raw += bytes(row)
    idat = zlib.compress(raw, 9)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', idat) + chunk(b'IEND', b''))
    print("生成", path, os.path.getsize(path), "bytes")

if __name__ == "__main__":
    base = os.path.dirname(os.path.abspath(__file__))
    make_png(os.path.join(base, "sample_game/game/images/logo.png"), 320, 180, (30, 90, 200))
    make_png(os.path.join(base, "mod_hanzi/images/logo.png"), 320, 180, (200, 40, 40))
