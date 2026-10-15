#!/usr/bin/env python3
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

W, H = 128, 64

BETUK = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "W": ["10001", "10001", "10001", "10101", "10101", "10101", "01010"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    ">": ["10000", "01000", "00100", "00010", "00100", "01000", "10000"],
    ":": ["00000", "00100", "00100", "00000", "00100", "00100", "00000"],
    ".": ["00000", "00000", "00000", "00000", "00000", "00100", "00100"],
    "_": ["00000", "00000", "00000", "00000", "00000", "00000", "11111"],
    " ": ["00000"] * 7,
}


def szoveg(d, x, y, s, nagy=1):
    for c in s:
        for sy, sor in enumerate(BETUK[c]):
            for sx, b in enumerate(sor):
                if b == "1":
                    d.rectangle([x + sx * nagy, y + sy * nagy, x + sx * nagy + nagy - 1, y + sy * nagy + nagy - 1], fill=0)
        x += 6 * nagy
    return x


def szekreny(d, rnd, mind_eg):
    x0, y0, w, h = 4, 4, 38, 58
    d.rectangle([x0, y0, x0 + w, y0 + h], outline=0)
    d.rectangle([x0 + 2, y0 + 2, x0 + w - 2, y0 + h - 2], outline=0)
    for u in range(5):
        uy = y0 + 5 + u * 10
        d.rectangle([x0 + 5, uy, x0 + w - 5, uy + 7], outline=0)
        for l in range(4):
            lx = x0 + 8 + l * 4
            if mind_eg or rnd.random() < 0.5:
                d.rectangle([lx, uy + 3, lx + 1, uy + 4], fill=0)
            else:
                d.point((lx, uy + 3), fill=0)
        for r in range(3):
            d.line([x0 + 26 + r * 2, uy + 2, x0 + 26 + r * 2, uy + 5], fill=0)
    d.line([x0 + 6, y0 + h, x0 + 6, y0 + h + 1], fill=0)
    d.line([x0 + w - 6, y0 + h, x0 + w - 6, y0 + h + 1], fill=0)


def kepkocka(i, rnd, aktiv):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    szekreny(d, rnd, aktiv and i % 2 == 0)
    szoveg(d, 50, 8, "BOTIROCKY")
    d.line([50, 18, 102, 18], fill=0)
    if aktiv:
        szoveg(d, 50, 26, "MENTES")
        pont = "." * (i % 4)
        szoveg(d, 86, 26, pont)
        d.rectangle([50, 38, 122, 44], outline=0)
        d.rectangle([52, 40, 52 + (i % 4 + 1) * 17, 42], fill=0)
    else:
        szoveg(d, 50, 26, "PARK: OK")
        x = szoveg(d, 50, 38, "> ")
        gepelt = "PARK"[: min(i, 4)]
        x = szoveg(d, x, 38, gepelt)
        if i % 2 == 0:
            d.rectangle([x, 44, x + 4, 44], fill=0)
    return img


def logo_kocka(i, rnd):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    for _ in range(14):
        x, y = rnd.randrange(W), rnd.randrange(H)
        d.point((x, y), fill=0)
        if rnd.random() < 0.3:
            d.point((x - 1, y), fill=0)
            d.point((x + 1, y), fill=0)
            d.point((x, y - 1), fill=0)
            d.point((x, y + 1), fill=0)
    cel_x = (W - 9 * 12) // 2
    x = W - (W - cel_x) * min(i, 5) // 5
    d.rectangle([x - 2, 19, x + 9 * 12, 36], fill=1)
    szoveg(d, x, 21, "BOTIROCKY", 2)
    if i >= 5:
        d.rectangle([cel_x, 39, cel_x + 9 * 12 - 3, 39], fill=0)
    if i >= 6 and i % 2 == 0:
        szoveg(d, 58, 44, "FW")
    return img


LOGO_META = """Filetype: Flipper Animation
Version: 1

Width: 128
Height: 64
Passive frames: 10
Active frames: 0
Frames order: 0 1 2 3 4 5 6 7 8 9
Active cycles: 0
Frame rate: 3
Duration: 3600
Active cooldown: 0

Bubble slots: 0
"""

MANIFEST = """Filetype: Flipper Animation Manifest
Version: 1

Name: BOTIROCKY_RACK
Min butthurt: 0
Max butthurt: 18
Min level: 1
Max level: 30
Weight: 5

Name: BOTIROCKY_LOGO
Min butthurt: 0
Max butthurt: 18
Min level: 1
Max level: 30
Weight: 3
"""


def ment(cel, kepek, meta):
    cel.mkdir(parents=True, exist_ok=True)
    for f in cel.glob("frame_*.png"):
        f.unlink()
    for i, k in enumerate(kepek):
        k.save(cel / f"frame_{i}.png")
    (cel / "meta.txt").write_text(meta)


META = """Filetype: Flipper Animation
Version: 1

Width: 128
Height: 64
Passive frames: 8
Active frames: 4
Frames order: 0 1 2 3 4 5 6 7 8 9 10 11
Active cycles: 3
Frame rate: 2
Duration: 3600
Active cooldown: 7

Bubble slots: 0
"""


def main():
    gyoker = Path(__file__).resolve().parent.parent / "assets"
    rnd = random.Random(42)
    rack = [kepkocka(i, rnd, False) for i in range(8)] + [kepkocka(i, rnd, True) for i in range(4)]
    logo = [logo_kocka(i, random.Random(i // 2)) for i in range(10)]
    ment(gyoker / "dolphin" / "external" / "L1_Botirocky_Rack_128x64", rack, META)
    csomag = gyoker / "packs" / "Botirocky" / "Anims"
    ment(csomag / "BOTIROCKY_RACK", rack, META)
    ment(csomag / "BOTIROCKY_LOGO", logo, LOGO_META)
    (csomag / "manifest.txt").write_text(MANIFEST)
    if len(sys.argv) > 1:
        nagy = [k.convert("L").resize((W * 4, H * 4), Image.NEAREST) for k in logo]
        lap = Image.new("L", (W * 4, H * 4 * 2 + 8), 128)
        lap.paste(nagy[3], (0, 0))
        lap.paste(nagy[8], (0, H * 4 + 8))
        lap.save(sys.argv[1])
    print(f"rack: {len(rack)}, logo: {len(logo)} képkocka")


if __name__ == "__main__":
    main()
