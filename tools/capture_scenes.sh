#!/usr/bin/env bash
# Captures one frame of every scene preset offscreen, for visual verification of
# rendering milestones. Each scene is warmed up by a scene-appropriate amount of
# *simulated* time so the frame shows developed orbits rather than the initial
# conditions.
#
#   tools/capture_scenes.sh [output-directory]
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXE="$ROOT/build/bin/gravitysim.exe"
OUT="${1:-$ROOT/docs/images}"
WIDTH="${WIDTH:-1100}"
HEIGHT="${HEIGHT:-660}"
UI="${UI:---no-ui}"

mkdir -p "$OUT"

# scene:warmup-seconds
SCENES=(
  "bounce:2.6"
  "attract:1400000"
  "two-body:9000000"
  "earth-moon:1400000"
  "inner:24000000"
  "solar-system:180000000"
  "binary:40000000"
  "three-body:130000000"
  "intruder:330000000"
  "compact:900000"
  "compact-vs-sun:150000000"
)

status=0
for entry in "${SCENES[@]}"; do
  scene="${entry%%:*}"
  warmup="${entry##*:}"
  target="$OUT/scene_${scene}.png"
  if "$EXE" --scene "$scene" $UI --screenshot "$target" --frame 4 \
       --warmup "$warmup" --width "$WIDTH" --height "$HEIGHT" >/dev/null 2>&1; then
    printf '  ok    %-16s -> %s\n' "$scene" "$target"
  else
    printf '  FAIL  %-16s\n' "$scene"
    status=1
  fi
done
exit "$status"
