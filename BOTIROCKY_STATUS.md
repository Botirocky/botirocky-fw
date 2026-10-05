# Botirocky FW — állapot

Ág: `botirocky` (a Momentum `dev`-ből). Build: `./fbt`.

## Kész (commitolva)
- Brand: `FIRMWARE_ORIGIN=Botirocky`, verzió `botirocky-dev`, dist-fájlnév prefix `botirocky-`,
  About „Botirocky FW:" fejléc + lábléc.
- `wifi_recon` app benne van (SKIP_EXTERNAL=False miatt fordul).

## Letöltve, bekötésre vár (NEM commitolt, `botirocky_sd/`, git-ignore)
- `infrared/` — 8901 IR-távirányító (logickworkshop/Flipper-IRDB)
- `music_player/` — 1251 RTTTL dallam (neverfa11ing/FlipperMusicRTTTL)
- `animations/` — Kuronons/FZ_graphics
Forrás-gyűjtemény: UberGuidoZ/Flipper (csak a benign submodule-ok).

## Következő lépések
1. **ITT FOLYTATNI: saját boot/dolphin animáció.** (rajzolt 128x64 pixelart, vagy user PNG-k)
2. A `botirocky_sd/` csomagok bekötése az SD-resource-manifestbe (firmware.scons / assets),
   vagy curated részhalmaz, hogy ne duzzadjon a flash-csomag.
3. Kigyomlálás (felesleges beépített appok).
4. Teljes build + .tgz, majd GitHub-ra push (saját repó URL kell — az origin a Next-Flip, oda nincs push).

## Tudatosan kihagyva
Egy helyi gyűjtemény autós tartalma (keeloq_mfcodes, Vehicles rögzítések) és a
UberGuidoZ BadUSB / Sub-GHz / NFC / RFID / picopass kategóriák — idegen jármű/beléptető/injektálás.
