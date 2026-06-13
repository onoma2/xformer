#!/usr/bin/env bash
# Rebuild the prebuilt firmware libraries that the Xformer VCV plugin links,
# from the CURRENT performer-phazer working tree. No git pull — dev/unpushed
# source is built as-is. Run from anywhere; the script operates on the
# performer-phazer checkout it lives in.
#
# Produces:
#   build/sim/debug/src/libcore.a
#   build/sim/debug/src/apps/sequencer/libsequencer_shared.a
#   build/xformer-vcv/libfwsim.a    (Simulator + TargetStateTracker + TargetTrace)
#   build/xformer-vcv/libcore_vcv.a (libcore.a with nanovg.c.o removed)
#
# Then rebuild the plugin (it links these). Env: JOBS=N for parallelism.
set -euo pipefail

FW="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIM="$FW/build/sim/debug"
OBJ="$SIM/src/CMakeFiles/core.dir/platform/sim/sim"
OUT="$FW/build/xformer-vcv"
JOBS="${JOBS:-4}"

# --- sanity: this is a performer-phazer checkout with a configured sim build ---
[ -f "$FW/src/apps/sequencer/SequencerApp.h" ] || {
    echo "error: $FW is not a performer-phazer checkout (no src/apps/sequencer/SequencerApp.h)"; exit 1; }
[ -f "$SIM/Makefile" ] || {
    echo "error: no CMake build at build/sim/debug. Configure it once with:"
    echo "  cmake -S \"$FW\" -B \"$SIM\" -DPLATFORM=sim -DCMAKE_BUILD_TYPE=Debug"; exit 1; }

echo "==> performer-phazer: $FW"
echo "    branch $(git -C "$FW" branch --show-current 2>/dev/null || echo '?'), HEAD $(git -C "$FW" rev-parse --short HEAD 2>/dev/null || echo '?')"
if [ -n "$(git -C "$FW" status --porcelain 2>/dev/null)" ]; then
    echo "    (uncommitted changes present — building the current working tree)"
fi

# --- 1. rebuild the CMake sim libs from current source ---
echo "==> building sim libs (core, sequencer_shared) -j$JOBS ..."
make -C "$SIM" core sequencer_shared -j"$JOBS"

CORE="$SIM/src/libcore.a"
SEQ="$SIM/src/apps/sequencer/libsequencer_shared.a"
for f in "$CORE" "$SEQ"; do
    [ -f "$f" ] || { echo "error: expected lib not produced: $f"; exit 1; }
done

# --- 2. libfwsim.a from the SDL-free sim glue objects (reuse CMake's compile) ---
echo "==> packing $OUT/libfwsim.a ..."
mkdir -p "$OUT"
rm -f "$OUT/libfwsim.a"
ar rcs "$OUT/libfwsim.a" \
    "$OBJ/Simulator.cpp.o" \
    "$OBJ/TargetStateTracker.cpp.o" \
    "$OBJ/TargetTrace.cpp.o"

# --- 3. libcore_vcv.a = libcore.a minus nanovg.c.o (plugin binds nvg* to libRack) ---
echo "==> packing $OUT/libcore_vcv.a (stripping nanovg.c.o) ..."
cp "$CORE" "$OUT/libcore_vcv.a"
ar d "$OUT/libcore_vcv.a" nanovg.c.o 2>/dev/null || echo "    (nanovg.c.o not present, ok)"

echo "==> done. firmware libs rebuilt from the current tree."
echo "    rebuild + install the plugin next, e.g.:"
echo "      make -C <VCVRack>/plugins/Onomatopoeia RACK_DIR=../.. -j$JOBS"
echo "      make -C <VCVRack>/plugins/Onomatopoeia RACK_DIR=../.. install"
