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





def render_stochastic_lock_pit_orchisstra(canvas: Canvas, sequence, track_engine):
    """
    pit-orchisstra-derived snake trail as event history.
    Captured events drawn as a snake path. Each segment = one event.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK SNAKE")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])

    first = sequence.lock_first()
    last = sequence.lock_last()
    current_step = track_engine.current_step()

    # Grid background
    grid_x0 = 8
    grid_y0 = 14
    cell_size = 4
    canvas.set_blend_mode(BlendMode.Set)

    for row in range(8):
        for col in range(8):
            x = grid_x0 + col * cell_size
            y = grid_y0 + row * cell_size
            canvas.set_color(Color.Low)
            canvas.draw_rect(x, y, cell_size - 1, cell_size - 1)

    # Snake path through captured events
    x, y = 0, 0
    canvas.set_color(Color.Bright)
    for i in range(first, last + 1):
        evt = sequence.lock_event(i)
        gx = grid_x0 + x * cell_size + 1
        gy = grid_y0 + y * cell_size + 1
        if evt.gate():
            canvas.set_color(Color.Bright)
        else:
            canvas.set_color(Color.Medium)
        canvas.fill_rect(gx, gy, 2, 2)
        # Wiggle forward
        x = (x + 1) % 8
        if x == 0:
            y = (y + 1) % 8

    # Current step cursor
    cx = grid_x0 + (current_step % 8) * cell_size + 1
    cy = grid_y0 + (current_step // 8) * cell_size + 1
    canvas.set_color(Color.Bright)
    canvas.draw_rect(cx - 1, cy - 1, 4, 4)

# ---------------------------------------------------------------------------
# Track variants
# ---------------------------------------------------------------------------




