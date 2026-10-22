# Make 720K (dd.st) and 1.44M (hd.st) Atari ST images with a known pattern,
# used by "make real" to check HxC-converted HFE images sector by sector.
import struct

def make(path, spt):
    tracks, sides = 80, 2
    total = tracks * sides * spt
    d = bytearray()
    for t in range(tracks):
        for s in range(sides):
            for sec in range(1, spt + 1):
                d += bytes(((t*7 + s*13 + sec*31 + i*3 + (i >> 8)) & 0xFF)
                           for i in range(512))
    boot = bytearray(d[0:512])
    boot[0:2] = b'\x60\x38'
    struct.pack_into('<HBHBHHBHHHH', boot, 11, 512, 2, 1, 2, 112, total,
                     0xF9, 5, spt, 2, 0)
    d[0:512] = boot
    with open(path, 'wb') as f:
        f.write(d)

make('dd.st', 9)
make('hd.st', 18)
