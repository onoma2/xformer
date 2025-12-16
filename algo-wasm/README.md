# algo-wasm

Lightweight Tuesday algorithm sandbox compiled to WebAssembly. This trims the firmware dependencies down to a single C++ translation unit and approximates the gate/microgate scheduler so you can iterate on algorithm parameters from a browser.

## Assumptions (simplifications)
- Scale: C major (7-note); root = C; transpose/octave applied but no per-project scale switching.
- Clock: 100 BPM, 192 PPQN; divisor 24 (1/16th), loop length 32.
- Track state: always playing, not muted; start/skew/reset handled minimally.
- Gate/microgate timing: approximated (per-step tick offsets), not the full FreeRTOS/timer path.

## Layout
- `tuesday_wasm.cpp` – self-contained simulator + C exports for wasm.
- `web/` – minimal UI (ES module) that loads wasm, exposes controls, and renders a 32-step grid.
- `wasm-build.sh` – emscripten build helper (expects `emcc` on PATH).

## Build
```bash
cd algo-wasm
./wasm-build.sh
```
Outputs `web/tuesday.js` and `web/tuesday.wasm`.

## Serve locally
```bash
cd web
python -m http.server 8000
# open http://localhost:8000
```

## Extending
- Add parameters/fields to `SequenceParams` in `tuesday_wasm.cpp`.
- Update `exportedStep` struct if you need more data in JS.
- The algorithms are lifted from `TuesdayTrackEngine` with minor adaptations to remove firmware dependencies; keep behavior changes annotated if you tweak them.
