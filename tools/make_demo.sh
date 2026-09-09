#!/usr/bin/env bash
# Renders the demo clips shown in the README.
#
# Every shot is deterministic: simulated time advances by a fixed amount per
# output frame rather than by the wall clock, so re-running this produces
# identical footage. Requires ffmpeg on PATH.
#
#   tools/make_demo.sh [output-directory]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXE="$ROOT/build/bin/gravitysim.exe"
OUT="${1:-$ROOT/docs/media}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

WIDTH="${WIDTH:-1024}"
HEIGHT="${HEIGHT:-576}"
FPS="${FPS:-30}"
CRF="${CRF:-27}"

mkdir -p "$OUT"

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg not found on PATH" >&2
  exit 1
fi

# shot <name> <frames> <extra gravitysim args...>  -> echoes the frame directory
shot() {
  local name="$1"; shift
  local frames="$1"; shift
  local dir="$WORK/$name"
  mkdir -p "$dir"
  echo "  rendering $name ($frames frames)" >&2
  "$EXE" --no-ui --width "$WIDTH" --height "$HEIGHT" \
         --sequence "$dir" --sequence-frames "$frames" "$@" >/dev/null 2>&1
  echo "$dir"
}

encode() {
  local dir="$1" target="$2"
  # yuv420p and even dimensions, so the result plays in browsers and on GitHub.
  ffmpeg -y -loglevel error -framerate "$FPS" -i "$dir/frame_%05d.png" \
    -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" \
    -c:v libx264 -profile:v high -pix_fmt yuv420p -crf "$CRF" -preset slow \
    -movflags +faststart "$target"
}

echo "Rendering demo clips into $OUT"

# Chaotic scenes track the BARYCENTRE, not a body. Following a single body is
# useless there: the three-body preset ejects a star and the camera then leaves
# the rest of the system behind, which is exactly what the first attempt did.

# 1. The solar system turning, camera sweeping a full circle around it.
d=$(shot solar 240 --scene solar-system --sequence-step 1200000 --sequence-orbit 360)
encode "$d" "$OUT/solar-system.mp4"

# 2. Three-body chaos: the "this is not scripted" shot. Kept short enough to
#    show the dance rather than the aftermath of the ejection.
d=$(shot three 240 --scene three-body --focus barycentre \
      --sequence-step 250000 --sequence-orbit 90)
encode "$d" "$OUT/three-body.mp4"

# 3. A star inserted at runtime tearing the inner solar system apart.
d=$(shot intruder 240 --scene inner --focus barycentre --distance 95 \
      --spawn Sun --spawn-distance 14 --warmup 20000000 --sequence-step 220000)
encode "$d" "$OUT/spawned-star.mp4"

# A short looping GIF for the top of the README. Two-pass palette generation,
# because GIF's default 256-colour dithering wrecks the gradients in the grid.
echo "  building README gif"
d=$(shot loop 96 --scene inner --sequence-step 900000 --sequence-orbit 360)
ffmpeg -y -loglevel error -framerate 24 -i "$d/frame_%05d.png" \
  -vf "fps=16,scale=520:-1:flags=lanczos,palettegen=stats_mode=diff:max_colors=128" \
  "$WORK/palette.png"
ffmpeg -y -loglevel error -framerate 24 -i "$d/frame_%05d.png" -i "$WORK/palette.png" \
  -lavfi "fps=16,scale=520:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5" \
  "$OUT/demo.gif"

echo
ls -lh "$OUT"
