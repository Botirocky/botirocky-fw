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
    cel = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "assets" / "dolphin" / "external" / "L1_Botirocky_Rack_128x64"
    cel.mkdir(parents=True, exist_ok=True)
    rnd = random.Random(42)
    kepek = [kepkocka(i, rnd, False) for i in range(8)] + [kepkocka(i, rnd, True) for i in range(4)]
    for i, k in enumerate(kepek):
        k.save(cel / f"frame_{i}.png")
    (cel / "meta.txt").write_text(META)
    if len(sys.argv) > 2:
        nagy = [k.convert("L").resize((W * 4, H * 4), Image.NEAREST) for k in kepek]
        nagy[0].save(sys.argv[2], save_all=True, append_images=nagy[1:], duration=500, loop=0)
        lap = Image.new("L", (W * 4, H * 4 * 2 + 8), 128)
        lap.paste(nagy[3], (0, 0))
        lap.paste(nagy[9], (0, H * 4 + 8))
        lap.save(Path(sys.argv[2]).with_suffix(".png"))
    print(f"{len(kepek)} képkocka: {cel}")


if __name__ == "__main__":
    main()
