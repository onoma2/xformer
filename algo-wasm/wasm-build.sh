#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

OUT_DIR="web"
SRC="tuesday_wasm.cpp"
OUT_JS="$OUT_DIR/tuesday.js"

mkdir -p "$OUT_DIR"

emcc "$SRC" -std=c++17 -O2 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s EXPORTED_FUNCTIONS='[_malloc,_free,_wasm_init,_wasm_set_param,_wasm_run_steps,_wasm_get_steps_len,_wasm_get_steps_ptr,_wasm_reset]' \
  -s EXPORTED_RUNTIME_METHODS='[ccall,cwrap,HEAPU8,HEAPF32]' \
  -o "$OUT_JS"

echo "Built $OUT_JS"
