#!/usr/bin/env bash
# In-engine frame capture for the itch screenshots and the trailer (no screen grabbing: only the game's own frames).
#   tools/capture/shoot.sh <name> <start_s> <frames> <game args...>
# Runs the native game at a fixed 30 fps step (-benchmark -fps=30) and writes every frame from <start_s> (world time) to
# hollywoodland-ue/Saved/Capture/<name>/f_NNNN.png (AHLWorldActor::Tick, -HLCapture), then the game quits.
set -u
name=$1; start=$2; frames=$3; shift 3
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT/hollywoodland-ue"
rm -rf "Saved/Capture/$name"
# RES=1920x1080 (default) or 1600x900
RES=${RES:-1920x1080}
"/c/Users/theco/Projects/unreal59/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w "$ROOT/hollywoodland-ue/Hollywoodland.uproject")" -game -windowed -ResX=${RES%x*} -ResY=${RES#*x} -log '-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge'   "$@" -benchmark -fps=30 -HLCapture="$name" -HLCapStart="$start" -HLCapFrames="$frames" > /dev/null 2>&1 &
sleep 20
for i in $(seq 1 400); do tasklist | grep -qi UnrealEditor || break; sleep 3; done
echo "$name: $(ls Saved/Capture/$name 2>/dev/null | wc -l) frames"
