#!/bin/bash
# usage: runall.sh spec1 spec2 ...   (spec names without .json)
E="$(cd "$(dirname "$0")" && pwd)"; cd "$E"
for k in "$@"; do
  src=$(python -c "import json;print(json.load(open('specs/$k.json'))['src'])"); name=$(python -c "import json;print(json.load(open('specs/$k.json'))['name'])")
  rm -rf "out/$name"; mkdir -p "out/$name"
  "/c/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P rig.py -- "specs/$k.json" "prep/$src.glb" "out/$name" > "out/$name/log.txt" 2>&1
  echo "== $k: $(grep -E 'RIG|FEET .*(raster|kmeans)|DONE|rror|feet not' "out/$name/log.txt" | tr '\n' ' ' | cut -c1-400)"
done
