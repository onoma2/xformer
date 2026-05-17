"""
Hybrid stochastic page renderers.
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter, SequencePainter
from tracks import (
    DiscreteMapSequence, DiscreteMapTrackEngine,
    NoteSequence, NoteTrackEngine,
    TuesdaySequence, TuesdayTrackEngine,
    StochasticSequence, StochasticTrackEngine,
    _ALGO_NAMES,
)

def render_stochastic_marbles_rndwalk(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """RndWalk-derived inverted horizontal bar meters for Spread/Bias/Steps/Mode.
    Exact params from RndWalk.h: gfxInvert(31, 48+(8*ch), w, 7).
    Scaled to 256px canvas: x=128, y=20+8*i, h=7, max_w=100.
    """
    slots = ["SPRD", "BIAS", "STEP", "MODE"]
    modes = ["OFF", "SOFT", "HARD"]
    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()
    mode = sequence.marbles_mode()
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE RW")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)
    params = [("SPREAD", spread, 0, 100), ("BIAS", bias, -50, 50), ("STEPS", steps, 0, 16), ("MODE", mode, 0, 2)]
    # Exact RndWalk params scaled to 256px canvas
    bar_x = 128       # scaled from 31 (half of 64px applet -> half of 256px canvas)
    bar_h = 7         # exact from RndWalk.h
    max_w = 100       # scaled from 31
    y0 = 20           # scaled from 48, adjusted to clear header
    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, val, minv, maxv) in enumerate(params):
        y = y0 + i * 8  # exact 8px spacing from RndWalk.h
        canvas.set_color(Color.Bright if i == slot else Color.Medium)
        canvas.draw_text(4, y + 5, name)
        if i == 3:
            mstr = modes[val] if val < len(modes) else "OFF"
            canvas.set_color(Color.Bright if i == slot else Color.Medium)
            canvas.draw_text(50, y + 5, mstr)
            continue
        if minv < 0:
            w = (abs(val) * max_w) // max(abs(minv), abs(maxv))
            if val >= 0:
                canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
                canvas.fill_rect(bar_x, y, w, bar_h)
            else:
                canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
                canvas.fill_rect(bar_x - w, y, w, bar_h)
            canvas.set_color(Color.Medium)
            canvas.vline(bar_x, y, bar_h)
        else:
            w = (val * max_w) // maxv
            canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
            canvas.fill_rect(bar_x - max_w // 2, y, w, bar_h)
        canvas.set_color(Color.Bright)
        canvas.draw_text(bar_x + max_w + 8, y + 5, str(val))



def render_stochastic_marbles_constellations(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """Constellations-derived star-field distribution. Stars = random points, brightness = probability density."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE CST")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)
    import math, random
    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()
    canvas.set_blend_mode(BlendMode.Set)
    rng = random.Random(42)
    num_stars = 20 + steps * 3
    for _ in range(num_stars):
        sx = rng.randint(10, 180)
        sy = rng.randint(14, 50)
        center_x = 95 + bias
        dist = abs(sx - center_x)
        envelope = math.exp(-(dist ** 2) / (spread * 2 + 1))
        brightness = int(envelope * 15)
        if brightness >= 12: canvas.set_color(Color.Bright)
        elif brightness >= 8: canvas.set_color(Color.MediumBright)
        elif brightness >= 4: canvas.set_color(Color.Medium)
        else: canvas.set_color(Color.Low)
        size = 1 if brightness < 8 else 2
        if size == 1: canvas.point(sx, sy)
        else: canvas.fill_rect(sx - 1, sy - 1, 3, 3)
    cursor_x = 95 + bias
    cursor_y = 32
    canvas.set_color(Color.Bright)
    for x in range(cursor_x - 6, cursor_x + 7, 2):
        canvas.point(x, cursor_y)
    for y in range(cursor_y - 6, cursor_y + 7, 2):
        canvas.point(cursor_x, y)
    canvas.set_color(Color.Medium)
    canvas.draw_text(190, 20, f"S{spread}")
    canvas.draw_text(190, 28, f"B{bias:+d}")
    canvas.draw_text(190, 36, f"N{steps}")



def render_stochastic_marbles_pit_orchisstra(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """
    pit-orchisstra-derived snake path through probability field.
    Snake body = distribution samples. Food = high-probability regions.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE SNAKE")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)

    import math
    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()

    # Draw probability field as 8x8 grid
    grid_x0 = 8
    grid_y0 = 14
    cell_size = 4
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(8):
        for col in range(8):
            x = grid_x0 + col * cell_size
            y = grid_y0 + row * cell_size
            # Distance from bias center
            dist = abs(col - 4 + bias // 10)
            envelope = math.exp(-(dist ** 2) / (spread / 10 + 1))
            brightness = int(envelope * 15)
            if brightness > 10:
                canvas.set_color(Color.Bright)
            elif brightness > 5:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)
            canvas.fill_rect(x, y, cell_size - 1, cell_size - 1)

    # Snake path
    snake_len = steps + 3
    snake_x = 4 + bias // 5
    snake_y = 4
    canvas.set_color(Color.Bright)
    for i in range(snake_len):
        gx = grid_x0 + snake_x * cell_size + 1
        gy = grid_y0 + snake_y * cell_size + 1
        canvas.fill_rect(gx, gy, 2, 2)
        # Wiggle
        snake_x = (snake_x + 1) % 8
        snake_y = (snake_y + (1 if i % 2 == 0 else -1)) % 8

    # Params right
    canvas.set_color(Color.Medium)
    canvas.draw_text(180, 20, f"S{spread}")
    canvas.draw_text(180, 28, f"B{bias:+d}")
    canvas.draw_text(180, 36, f"N{steps}")




def render_stochastic_marbles_bline(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """
    bline-derived XY position grid with crosshairs.
    Spread = grid cursor size. Bias = cursor position.
    4-quadrant parameter display.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE XY")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)

    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()

    # XY grid background
    grid_x = 10
    grid_y = 16
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    for row in range(4):
        for col in range(4):
            canvas.draw_rect(grid_x + col * 10, grid_y + row * 10, 9, 9)

    # Cursor position from bias
    cursor_col = 1 + (bias + 50) // 25
    cursor_row = 1 + (spread // 25)
    cursor_col = max(0, min(3, cursor_col))
    cursor_row = max(0, min(3, cursor_row))

    # Highlight current cell
    canvas.set_color(Color.Medium)
    canvas.fill_rect(grid_x + cursor_col * 10, grid_y + cursor_row * 10, 9, 9)

    # Crosshairs
    cx = grid_x + cursor_col * 10 + 4
    cy = grid_y + cursor_row * 10 + 4
    canvas.set_color(Color.Bright)
    for x in range(grid_x, grid_x + 40, 2):
        canvas.point(x, cy)
    for y in range(grid_y, grid_y + 40, 2):
        canvas.point(cx, y)

    # 4-quadrant param display right side
    params = [("SPRD", spread), ("BIAS", bias), ("STEP", sequence.marbles_steps()), ("MODE", sequence.marbles_mode())]
    for i, (name, val) in enumerate(params):
        y = 16 + i * 10
        canvas.set_color(Color.Bright if i == slot else Color.Medium)
        canvas.draw_text(60, y + 5, name)
        canvas.draw_text(100, y + 5, str(val))

# ---------------------------------------------------------------------------
# Lock variants
# ---------------------------------------------------------------------------



