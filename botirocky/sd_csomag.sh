#!/usr/bin/env bash
set -euo pipefail

GYOKER="$(cd "$(dirname "$0")/.." && pwd)"
FORRAS="${GYOKER}/botirocky_sd"
MUNKA="${GYOKER}/build/botirocky_sd"
KIMENET="${GYOKER}/dist/botirocky-sd-$(date +%Y%m%d).tgz"

klonoz() {
    local nev="$1" url="$2"
    if [[ -d "${FORRAS}/${nev}/.git" ]]; then
        git -C "${FORRAS}/${nev}" pull -q --ff-only || true
    else
        git clone -q --depth 1 "$url" "${FORRAS}/${nev}"
    fi
}

mkdir -p "$FORRAS"
klonoz infrared https://github.com/logickworkshop/Flipper-IRDB
klonoz music_player https://github.com/neverfa11ing/FlipperMusicRTTTL

rm -rf "$MUNKA"
mkdir -p "${MUNKA}/infrared/IRDB" "${MUNKA}/apps_data/music_player"

rsync -a --exclude .git --include "*/" --include "*.ir" --include "LICENSE" --include "README.md" --exclude "*" \
    "${FORRAS}/infrared/" "${MUNKA}/infrared/IRDB/"
rsync -a --exclude .git --include "*/" --include "*.txt" --include "*.fmf" --include "README.md" --exclude "*" \
    "${FORRAS}/music_player/" "${MUNKA}/apps_data/music_player/"
find "$MUNKA" -type d -empty -delete

mkdir -p "$(dirname "$KIMENET")"
tar -C "$MUNKA" -czf "$KIMENET" .
ir=$(find "${MUNKA}/infrared" -name "*.ir" | wc -l)
zene=$(find "${MUNKA}/apps_data/music_player" \( -name "*.txt" -o -name "*.fmf" \) | wc -l)
echo "kész: ${KIMENET} ($(du -h "$KIMENET" | cut -f1)) – ${ir} IR-fájl, ${zene} dallam"
echo "Az SD-kártya gyökerébe kell kicsomagolni (qFlipper vagy kártyaolvasó)."
