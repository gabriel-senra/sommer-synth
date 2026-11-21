#!/bin/sh
# Build a trimmed copy of AMY in build/amy_src: AMY's sources, overlaid
# with our trimmed headers. Only files whose content changed are touched,
# so make doesn't rebuild everything every time.
set -e
here=$(cd "$(dirname "$0")" && pwd)
amy="$here/../../../third_party/amy"
out="$here/../build/amy_src"
mkdir -p "$out"
put() { cmp -s "$1" "$2" 2>/dev/null || cp "$1" "$2"; }
for f in "$amy"/*.c "$amy"/*.h; do
  case "$(basename "$f")" in
    pcm_tiny.h|interp_partials.h|patches.h|pcm_samples_tiny.h) ;;  # overlaid / dropped below
    *) put "$f" "$out/$(basename "$f")" ;;
  esac
done
# Overlay: our trimmed tables replace AMY's.
put "$here/pcm_tiny.h"        "$out/pcm_tiny.h"
put "$here/interp_partials.h" "$out/interp_partials.h"
python3 "$here/make_patches.py" "$amy/patches.h" "$out/patches.h.new"
put "$out/patches.h.new" "$out/patches.h"; rm "$out/patches.h.new"
