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

def render_stochastic_lock_qfwfq(canvas: Canvas, sequence, track_engine):
    """qfwfq-derived ASCII ribbon lock page.
    Exact params from qfwfq.lua: STD_W=8, BLANK_Y=55, HACK_Y=37, STEP_Y=40, POS_Y=60.
    Scaled to 256px canvas: spacing=16, y positions compressed to clear footer.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK RIB")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])
    first = sequence.lock_first()
    last = sequence.lock_last()
    current_step = track_engine.current_step()
    # Exact qfwfq spacing scaled 2x for 256px canvas
    step_spacing = 16  # STD_W=8 * 2
    x0 = 6  # scaled from x=3
    # Y positions compressed to fit above footer (y=53)
    hack_y = 20      # HACK_Y=37 -> scaled/compressed
    step_y = 28      # STEP_Y=40 -> scaled/compressed
    blank_y = 36     # BLANK_Y=55 -> scaled/compressed
    pos_y = 44       # POS_Y=60 -> scaled/compressed
    canvas.set_blend_mode(BlendMode.Set)
    # Draw blank underlines (qfwfq draw_blanks: line from x to x+5 at BLANK_Y)
    canvas.set_color(Color.Low)
    for i in range(16):
        x = x0 + i * step_spacing
        canvas.hline(x, blank_y, 10)  # scaled 5*2
    # Draw captured gate glyphs (top row) and step dots
    for i in range(16):
        evt = sequence.lock_event(i)
        x = x0 + i * step_spacing
        in_window = first <= i <= last
        if in_window and evt.gate():
            ch = "X"
            glyph_color = Color.Bright
        elif in_window:
            ch = "."
            glyph_color = Color.Medium
        else:
            ch = "?"
            glyph_color = Color.Low
        # Step dot at step_y (like qfwfq draw_step_dot)
        if i == current_step:
            canvas.set_color(Color.Bright)
            canvas.draw_text(x, step_y, ".")
        # Glyph at hack_y (top row, like qfwfq hack_glyph row)
        canvas.set_color(glyph_color)
        canvas.draw_text(x, hack_y, ch)
    # Draw source step indices (bottom row, like qfwfq pwd glyph row at BLANK_Y-4)
    for i in range(16):
        source = sequence.step(i)
        x = x0 + i * step_spacing
        idx_str = f"{i+1:02d}"
        canvas.set_color(Color.Medium if source.gate() else Color.Low)
        canvas.draw_text(x, blank_y - 4, idx_str)
    # Cursor dot at pos_y (like qfwfq draw_cursor_dot)
    cx = x0 + current_step * step_spacing
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx, pos_y, ".")



