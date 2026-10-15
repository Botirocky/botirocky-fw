# Botirocky FW — állapot

Ág: `botirocky` (a Momentum `dev`-ből). Build: `./fbt`.

## Kész (commitolva)
- Brand: `FIRMWARE_ORIGIN=Botirocky`, verzió `botirocky-dev`, dist-fájlnév prefix `botirocky-`,
  About „Botirocky FW:" fejléc + lábléc.
- `wifi_recon` app benne van (SKIP_EXTERNAL=False miatt fordul).
- Saját animációk (2026-10-15): `botirocky/anim_gen.py` generálja őket (saját 5x7-es betűk).
  - `assets/dolphin/external/L1_Botirocky_Rack_128x64` – a gyári delfin-készletben is (Weight 7).
  - `assets/packs/Botirocky/` asset pack: BOTIROCKY_RACK + BOTIROCKY_LOGO. A Flipperen:
    Momentum Settings → Interface → Graphics → Asset pack → Botirocky.
- Teljes build megy: `./fbt COMPACT=1 DEBUG=0 updater_package` → `dist/f7-C/*.tgz` (~12 MB).

## Letöltve, bekötésre vár (NEM commitolt, `botirocky_sd/`, git-ignore)
- `infrared/` — 8901 IR-távirányító (logickworkshop/Flipper-IRDB)
- `music_player/` — 1251 RTTTL dallam (neverfa11ing/FlipperMusicRTTTL)
- `animations/` — Kuronons/FZ_graphics
Forrás-gyűjtemény: UberGuidoZ/Flipper (csak a benign submodule-ok).

## Következő lépések
1. ~~saját animáció~~ KÉSZ. Opcionális: Kuronons (CC0) szettek a Botirocky packba, passport-kép.
2. **ITT FOLYTATNI:** A `botirocky_sd/` csomagok bekötése az SD-resource-manifestbe (firmware.scons / assets),
   vagy curated részhalmaz, hogy ne duzzadjon a flash-csomag.
3. Kigyomlálás (felesleges beépített appok).
4. Teljes build + .tgz, majd GitHub-ra push (saját repó URL kell — az origin a Next-Flip, oda nincs push).

## Tudatosan kihagyva
Egy helyi gyűjtemény autós tartalma (keeloq_mfcodes, Vehicles rögzítések) és a
UberGuidoZ BadUSB / Sub-GHz / NFC / RFID / picopass kategóriák — idegen jármű/beléptető/injektálás.
