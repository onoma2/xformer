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



