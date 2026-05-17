# UI Preview Tool

Fast-iteration OLED preview generator. Renders the exact same pixels the
firmware would draw on the 256×64 overview page — no C++ compilation, no
simulator startup, just a Python script.

## Usage

```bash
cd tools/ui-preview
python3 generate.py -o preview.png
```

The output is a 4× upscaled PNG (1024×256) so individual pixels are easy to
inspect. Open it in any image viewer.

### DiscreteMap flags

Override the default DiscreteMap demo data without editing code:

```bash
python3 generate.py \
    --discrete-map-stages='-80,-40,0,40,80' \
    --discrete-map-active=3 \
    --discrete-map-input=2.5 \
    --discrete-map-range='-5,5' \
    -o dm.png
```

| Flag | Description |
|------|-------------|
| `--discrete-map-stages` | Comma-separated thresholds (-100 .. 100) |
| `--discrete-map-dirs` | Comma-separated directions: `rise`, `fall`, `off`, `both` |
| `--discrete-map-active` | Active stage index (-1 = none) |
| `--discrete-map-input` | Current input CV value |
| `--discrete-map-range` | Input range as `"low,high"` |

**Tip:** Use `=` syntax for negative numbers (`--discrete-map-stages='-50,0,50'`)
to prevent the shell from interpreting the leading `-` as a flag.

## What it renders

The full OverviewPage layout:

- **Left panel** – track number (T1…T8), pattern (P1…P16), mute dimming
- **Center panel** – per-track preview strip (64 … 192, 128 px wide)
- **Right panel** – gate output indicator, CV value text
- **Separators** – vertical lines matching the firmware exactly

Reference tracks included in the default demo:

| Track | Type | Visual elements |
|-------|------|-----------------|
| 0 | Note | 16 step squares, current step bright |
| 1 | Tuesday | Gate square, step counter "8/16", algo name "CHIP2" |
| 2 | DiscreteMap | Baseline, threshold markers, input cursor |
| 3 | **Stochastic** | **← your design iteration area** |
| 4-7 | Note (empty) | Left / right info only |

## Designing the Stochastic track preview

Edit `tracks.py` → `draw_stochastic_track()`. The function signature mirrors
the firmware:

```python
def draw_stochastic_track(canvas, track_index, track_engine, sequence):
    y = track_index * 8          # row top
    # canvas is 128 px wide here: x = 64 .. 192
    # 16 columns of 8 px each fit perfectly
```

Everything in `canvas.py` is a line-for-line translation of the firmware
`Canvas` / `FrameBuffer` / `Blit` code, including:

- 4-bit grayscale color values (`Color.Low`, `Color.Medium`, `Color.Bright` …)
- `BlendMode.Set / Add / Sub`
- Exact `tiny5x5` bitmap font (parsed live from `src/core/gfx/fonts/tiny5x5.h`)
- Pixel-exact `fillRect`, `drawRect`, `hline`, `vline`, `drawText`

## Changing demo data

Edit `generate.py` → `_make_default_project()`. You can tweak gate patterns,
step positions, CV values, mute states, discrete-map stages, etc.

## File map

| File | Purpose |
|------|---------|
| `canvas.py` | FrameBuffer, Canvas primitives, font parser, image export |
| `tracks.py` | Stub model classes + `draw_*_track` functions from OverviewPage.cpp |
| `generate.py` | Overview page renderer, default demo data, CLI |

## CLI options

```
python3 generate.py -o preview.png --scale 4
```

- `-o, --output` – output image path (default: `preview.png`)
- `--scale` – nearest-neighbor upscale factor (default: 4)
