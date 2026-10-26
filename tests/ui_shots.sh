#!/usr/bin/env bash
# Renders the real UI offscreen (software backend) into PNGs for visual review.
# usage: ui_shots.sh <lumina-binary> <testdata-dir> <out-dir>
set -u
BIN=$1; DATA=$2; OUT=$3
mkdir -p "$OUT"
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software LUMINA_AUDIO_DUMMY=1 LUMINA_DATA_DIR="$OUT/appdata"
shot() { name=$1; delay=$2; shift 2; timeout 60 "$BIN" "$@" --ui-screenshot "$OUT/$name.png" --ui-screenshot-delay "$delay" >"$OUT/$name.log" 2>&1 || echo "FAILED: $name"; }
shot home 6000 --add-library-folder "$DATA"
shot movies 2500 --page movies
shot all_list 2500 --page all
for n in 0 1 2 3 4 5 6 7 8; do shot settings_$n 1500 --page settings --settings-category $n; done
shot streams 1500 --page streams
shot playlists 1500 --page playlists
for p in subtitles audio video bookmarks queue; do shot panel_$p 2500 "$DATA/sample.mp4" --panel $p; done
