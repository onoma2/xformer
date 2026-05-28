"""
PhaseFlux ACCUM.N redesign — minimal pass.

Goal at this step: same header, same footer, same per-cell stage visuals and
behaviour as the existing 4x4 grid. Only change: lay all 16 stage squares
out in a single row. No extra bars / amount text / pancakes yet.

Cell drawing is delegated to a row-layout variant of draw_grid_cell so the
visual rules (active fill, skip X, pulse-count bar, phase/mask dots,
selected outline) stay identical.
"""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, WindowPainter

from pages_phaseflux import (
    Stage, demo_sequence,
    GRID_CELL_W, GRID_CELL_H,
)


# Sparse row, shifted right to open a 17 px glyph strip on the left while
# keeping ≥ half-cell (5 px) of dark margin on the right.
# 16 cells * 9 px + 15 gaps * 6 px = 234 px → left margin 17, right margin 5.
ROW_GAP     = 6
ROW_X_START = 17

# Vertical: header occupies y=0..10, footer y=53..63. Center 9 px cell row
# in the 42 px content band → y = 11 + (42 - 9)/2 ≈ 27.
ROW_Y       = 28


def _draw_cell_row(canvas: Canvas, idx: int, stage: Stage,
                   is_active: bool, is_selected: bool):
    """Same drawing rules as draw_grid_cell, but the cell sits in a single
    row at (ROW_X_START + idx*step, ROW_Y) instead of a 4x4 grid."""
    x = ROW_X_START + idx * (GRID_CELL_W + ROW_GAP)
    y = ROW_Y

    canvas.set_blend_mode(BlendMode.Set)
    if is_active:
        canvas.set_color(Color.Bright)
        canvas.fill_rect(x, y, GRID_CELL_W, GRID_CELL_H)
    elif stage.skip:
        canvas.set_color(Color.Low)
        canvas.draw_rect(x, y, GRID_CELL_W, GRID_CELL_H)
        canvas.line(x + 2, y + 2, x + GRID_CELL_W - 3, y + GRID_CELL_H - 3)
        canvas.line(x + GRID_CELL_W - 3, y + 2, x + 2, y + GRID_CELL_H - 3)
    else:
        canvas.set_color(Color.MediumLow)
        canvas.draw_rect(x, y, GRID_CELL_W, GRID_CELL_H)

    if is_selected and not is_active:
        canvas.set_color(Color.Bright)
        canvas.draw_rect(x, y, GRID_CELL_W, GRID_CELL_H)

    if stage.skip:
        return

    pulses = max(1, min(8, stage.pulseCount))

    if not is_active:
        bar_color = Color.Bright if is_selected else Color.MediumBright
        canvas.set_color(bar_color)
        canvas.hline(x + 1, y + GRID_CELL_H - 2, min(GRID_CELL_W - 2, pulses))
        if stage.phaseShift != 0:
            canvas.point(x + 1, y + 1)
        if stage.mask != 0:
            canvas.point(x + GRID_CELL_W - 2, y + 1)
    else:
        canvas.set_color(Color.None_)
        canvas.hline(x + 1, y + GRID_CELL_H - 2, min(GRID_CELL_W - 2, pulses))
        if stage.phaseShift != 0:
            canvas.point(x + 1, y + 1)
        if stage.mask != 0:
            canvas.point(x + GRID_CELL_W - 2, y + 1)


def draw_grid_row(canvas: Canvas, stages, active_idx: int, selected_idx: int):
    for i in range(16):
        _draw_cell_row(canvas, i, stages[i],
                       is_active=(i == active_idx),
                       is_selected=(i == selected_idx))


# Per-cell BIPOLAR amount pancakes: 1 px lit + 1 px dark gap.
# Positive Ac.St grows UP from the top of the square; negative grows DOWN.
# Stack height = |Ac.St|, clamped to 7 (matches pulse range; note storage
# stays 5-bit ±15, UI clamps for now).
PANCAKE_W = GRID_CELL_W // 2          # 4 px wide
PANCAKE_X_OFFSET = GRID_CELL_W - PANCAKE_W   # 5 — right half of the cell
PANCAKE_MAX_MAG = 7

def draw_amount_pancakes(canvas: Canvas, amounts, stages, selected_idx: int):
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        if stages[i].skip:
            continue
        amt = amounts[i]
        if amt == 0:
            continue
        mag = min(abs(amt), PANCAKE_MAX_MAG)
        x = ROW_X_START + i * (GRID_CELL_W + ROW_GAP) + PANCAKE_X_OFFSET
        canvas.set_color(Color.Bright if i == selected_idx else Color.Medium)
        if amt > 0:
            for n in range(1, mag + 1):
                canvas.hline(x, ROW_Y - 2 * n, PANCAKE_W)
        else:
            for n in range(1, mag + 1):
                canvas.hline(x, ROW_Y + GRID_CELL_H + 2 * n - 1, PANCAKE_W)


# Single-page footer — limits live on the sequence list page now.
FOOTER_ACCUM = ["Ac.St", "Order", "Polar", "Reset", "Trig"]


# --- hand-drawn 5x5 glyphs --------------------------------------------------
# "X" = lit, "." = transparent.

_GLYPH_ORDER = {
    "Wrap": [
        "..X..",
        "...X.",
        "XXXXX",
        "...X.",
        "..X..",
    ],
    "Pend": [
        ".....",
        ".X.X.",
        "XXXXX",
        ".X.X.",
        ".....",
    ],
    "Hold": [
        ".....",
        ".....",
        "XXXXX",
        ".....",
        ".....",
    ],
    "RTZ": [
        "X....",
        "X....",
        "XXXX.",
        "...X.",
        "..X..",
    ],
    "Rand": [
        "X.X.X",
        ".X.X.",
        "X.X.X",
        ".X.X.",
        "X.X.X",
    ],
}

_GLYPH_POLAR = {
    "Uni": [
        "..X..",
        ".XXX.",
        "X.X.X",
        "..X..",
        "..X..",
    ],
    "Bi": [
        "..X..",
        ".XXX.",
        "..X..",
        ".XXX.",
        "..X..",
    ],
}


def _paint_glyph(canvas: Canvas, x: int, y: int, pattern):
    canvas.set_blend_mode(BlendMode.Set)
    for row, line in enumerate(pattern):
        for col, ch in enumerate(line):
            if ch == "X":
                canvas.point(x + col, y + row)


# Left-margin glyph strip: 3 glyphs stacked vertically.
GLYPH_X       = 4       # left edge of glyph column (5 px wide → x=4..8)
GLYPH_ORDER_Y = 18
GLYPH_POLAR_Y = 28
GLYPH_RESET_Y = 38      # tiny5x5 baseline for text (M / digit / +)


def draw_sequence_glyphs(canvas: Canvas, *, order: str, polar: str,
                          reset_cycles: int):
    canvas.set_color(Color.MediumBright)
    _paint_glyph(canvas, GLYPH_X, GLYPH_ORDER_Y, _GLYPH_ORDER[order])
    _paint_glyph(canvas, GLYPH_X, GLYPH_POLAR_Y, _GLYPH_POLAR[polar])

    # Reset uses an actual font glyph — M / digit / +.
    canvas.set_font(Font.Tiny)
    if reset_cycles <= 0:
        txt = "M"
    elif reset_cycles <= 9:
        txt = str(reset_cycles)
    else:
        txt = "+"
    tw = canvas.text_width(txt)
    canvas.draw_text(GLYPH_X + (5 - tw) // 2, GLYPH_RESET_Y + 5, txt)


# --- per-cell T/S chip ------------------------------------------------------

DEMO_TRIGS = ["S"] * 16
DEMO_TRIGS[8]  = "P"
DEMO_TRIGS[12] = "P"


def draw_trig_chips(canvas: Canvas, stages, trigs):
    """Per-cell single-char trig glyph (T = Stage, P = Pulse), sitting below
    the square in the lim- pancake band, on the LEFT half of the cell."""
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    for i in range(16):
        if stages[i].skip:
            continue
        x_col = ROW_X_START + i * (GRID_CELL_W + ROW_GAP)
        canvas.draw_text(x_col - 2, ROW_Y + GRID_CELL_H + 6, trigs[i])


DEMO_AMOUNTS = [+2, 0, +3, 0,  0, 0, -1, 0,  +2, 0, 0, 0,  -3, 0, 0, 0]


def render_accum_n_redesign(canvas: Canvas, stages, *,
                             selected_cell: int = 4,
                             current_cell: int = 8,
                             amounts=None,
                             trigs=None,
                             order: str = "Wrap",
                             polar: str = "Uni",
                             reset_cycles: int = 4):
    if amounts is None:
        amounts = DEMO_AMOUNTS
    if trigs is None:
        trigs = DEMO_TRIGS
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, "ACCUM")
    WindowPainter.draw_footer(canvas, names=FOOTER_ACCUM, highlight=-1)
    draw_sequence_glyphs(canvas, order=order, polar=polar,
                          reset_cycles=reset_cycles)
    draw_amount_pancakes(canvas, amounts, stages, selected_idx=selected_cell)
    draw_trig_chips(canvas, stages, trigs)
    draw_grid_row(canvas, stages, active_idx=current_cell, selected_idx=selected_cell)


def main():
    stages = demo_sequence()
    out_dir = os.path.join(os.path.dirname(__file__), "accum-n")
    os.makedirs(out_dir, exist_ok=True)

    fb = FrameBuffer(PageWidth, PageHeight)
    c = Canvas(fb)
    render_accum_n_redesign(c, stages, selected_cell=4, current_cell=8)
    img = framebuffer_to_image(fb, scale=4)
    path = os.path.join(out_dir, "accum-n-row.png")
    img.save(path)
    print(f"wrote {path}")


if __name__ == "__main__":
    main()
