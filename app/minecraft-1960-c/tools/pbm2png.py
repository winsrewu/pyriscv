#!/usr/bin/env python3
"""Convert a P4 PBM into a scaled grayscale PNG (stdlib only)."""
import struct, sys, zlib

def read_pbm(path):
    with open(path, 'rb') as f:
        data = f.read()
    # header: P4 <w> <h> then packed bits
    parts = data.split(None, 3)
    assert parts[0] == b'P4'
    w, h = int(parts[1]), int(parts[2])
    bits = parts[3]
    px = []
    stride = (w + 7) // 8
    for y in range(h):
        row = []
        for x in range(w):
            byte = bits[y * stride + x // 8]
            row.append(1 if (byte >> (7 - (x % 8))) & 1 else 0)
        px.append(row)
    return w, h, px

def write_png(path, w, h, px, scale=2):
    W, H = w * scale, h * scale
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        for x in range(W):
            raw.append(0 if px[y // scale][x // scale] else 255)
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        c += struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
        return c
    ihdr = struct.pack('>IIBBBBB', W, H, 8, 0, 0, 0, 0)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', ihdr)
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    png += chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)

if __name__ == '__main__':
    src = sys.argv[1]
    dst = sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    w, h, px = read_pbm(src)
    write_png(dst, w, h, px, scale)
    print('wrote', dst, w, 'x', h)
