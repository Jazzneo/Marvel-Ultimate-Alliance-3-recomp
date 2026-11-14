#!/usr/bin/env python3
"""Where each shader's code is in the game files: runtime/nvn/shader_code_locs.txt (developer tool).

The shipped shader list (runtime/nvn/shader_variants.txt) names shaders by a one-way hash of their code; the code
itself never ships. On the player's machine the runtime finds it in their own game files - MUA3 keeps it in its shader
packages (SHADER/**/*.spkg, *.kps, *.kps3), which are read in place from the NSPs, so there is nothing to scan. This
writes, for every code the game handed to NVN in your play (cache/shader_code/<hash>.bin), the game file and the
place in it that holds the same bytes: one line each,

    <hash> <romfs path> <offset> <size> <const offset> <const size>

Facts about where things are, no game data: at startup the runtime reads those bytes from the player's own copy and
checks them against the hash before preparing anything (runtime/nvn/shader_prep.c, idx_locs).

  tools/shader_locs.py <cache/shader_code> <a folder with the romfs files to search> [out] [--main extracted/exefs/main]

--main: also the game's main module (its shaders built in): "@main <offset in the loaded module>", read by the
runtime from the module in memory.

The romfs files: tools/nsp_extract.py <base> <keys> <scratch> --update <update> --romfs none --files SHADER/
(a scratch folder outside the repo; delete it after).
"""
import os
import struct
import sys

import lz4.block


def fnv1a64(b):
    h = 0xcbf29ce484222325
    for x in b:
        h ^= x
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def nso_image(path):  # the module as loaded: each segment at its offset in memory
    d = open(path, 'rb').read()
    flags = struct.unpack_from('<I', d, 0xC)[0]
    img = bytearray()
    for i in range(3):
        foff, moff, size = struct.unpack_from('<3I', d, 0x10 + i * 0x10)
        csize = struct.unpack_from('<I', d, 0x60 + i * 4)[0]
        raw = d[foff:foff + csize]
        seg = lz4.block.decompress(raw, uncompressed_size=size) if flags & (1 << i) else raw
        if len(img) < moff + size:
            img.extend(bytes(moff + size - len(img)))
        img[moff:moff + size] = seg
    return bytes(img)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--main')]
    mains = [a.split('=', 1)[1] for a in sys.argv[1:] if a.startswith('--main=')]
    if '--main' in sys.argv:
        i = sys.argv.index('--main')
        mains.append(sys.argv[i + 1])
        args.remove(sys.argv[i + 1])
    if len(args) < 2:
        sys.exit(__doc__)
    codes_dir, root = args[0], args[1]
    out = args[2] if len(args) > 2 else 'runtime/nvn/shader_code_locs.txt'
    main_img = nso_image(mains[0]) if mains else None
    romfs = os.path.join(root, 'romfs') if os.path.isdir(os.path.join(root, 'romfs')) else root
    files = {}
    for d, _, names in os.walk(romfs):
        for n in names:
            p = os.path.join(d, n)
            files[os.path.relpath(p, romfs).replace(os.sep, '/')] = open(p, 'rb').read()
    lines, found, missing = [], 0, 0
    for n in sorted(os.listdir(codes_dir)):
        if not (len(n) == 20 and n.endswith('.bin')):
            continue
        d = open(os.path.join(codes_dir, n), 'rb').read()
        magic, size, coff, csize = struct.unpack_from('<4I', d)
        code = d[16:16 + size]
        h = int(n[:16], 16)
        if magic != 0x31434853 or len(code) != size or coff + csize != size or fnv1a64(code) != h:
            continue
        hit = None
        for rel in sorted(files):
            i = files[rel].find(code)
            if i >= 0:
                hit = (rel, i)
                break
        if not hit and main_img is not None:
            i = main_img.find(code)
            if i >= 0:
                hit = ('@main', i)
        if not hit:
            missing += 1
            continue
        found += 1
        lines.append('%016x %s %d %d %d %d' % (h, hit[0], hit[1], size, coff, csize))
    with open(out, 'w', newline='\n') as f:
        f.write('# Where each shader of runtime/nvn/shader_variants.txt is in the game files (tools/shader_locs.py):\n'
                '# hash, romfs path, offset, size, const offset, const size. No game data.\n')
        f.write(''.join(l + '\n' for l in lines))
    print('%d codes found in the game files, %d not (made by the game at run time, or in files not searched) -> %s'
          % (found, missing, out))


if __name__ == '__main__':
    main()
