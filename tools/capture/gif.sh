#!/usr/bin/env bash
# Frames -> an itch screenshot GIF (under 3 MB): tools/capture/gif.sh <name> [first] [count] [width] [fps]
# Reads hollywoodland-ue/Saved/Capture/<name>/f_NNNN.png (30 fps), writes tools/capture/itch/<name>.gif with a palette made
# from the clip itself; steps the width down until the file fits.
set -u
name=$1; first=${2:-0}; count=${3:-150}; width=${4:-630}; fps=${5:-15}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
FF="$LOCALAPPDATA/Microsoft/WinGet/Packages/Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe/ffmpeg-9.0.2-full_build/bin/ffmpeg.exe"
SRC="$ROOT/hollywoodland-ue/Saved/Capture/$name"
# a frame the engine skipped would end the sequence: renumber what exists into a gap-free copy
SEQ="$ROOT/hollywoodland-ue/Saved/Capture/_seq_$name"; rm -rf "$SEQ"; mkdir -p "$SEQ"
i=0; for f in $(ls "$SRC" | sort); do cp "$SRC/$f" "$SEQ/f_$(printf %04d $i).png"; i=$((i + 1)); done
IN="$SEQ/f_%04d.png"
OUT="$ROOT/tools/capture/itch"; mkdir -p "$OUT"
for wf in "$width:$fps" "$width:12" "$width:10" "600:10" "600:8" "560:8" "520:8"; do
  w=${wf%%:*}; f=${wf##*:}
  "$FF" -y -loglevel error -framerate 30 -start_number "$first" -i "$IN" -frames:v "$count" \
    -vf "fps=$f,scale=$w:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle" \
    -loop 0 "$OUT/$name.gif"
  size=$(stat -c %s "$OUT/$name.gif")
  echo "$name.gif ${w}px ${f}fps $((size / 1024)) KB"
  [ "$size" -lt 3000000 ] && break
done
rm -rf "$SEQ"
