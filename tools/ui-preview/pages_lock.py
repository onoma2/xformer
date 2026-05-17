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

def render_stochastic_lock_skylines(canvas: Canvas, sequence, track_engine):
    """Lock page as skyline bar chart. Captured events = bright foreground bars. Source = dim background."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK SKY")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])
    bar_w = 4
    bar_gap = 1
    base_y = 50
    scale = 5
    first = sequence.lock_first()
    last = sequence.lock_last()
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        evt = sequence.lock_event(i)
        source = sequence.step(i)
        x = 8 + i * (bar_w + bar_gap)
        src_h = (source.gate_probability() * scale) // 100
        canvas.set_color(Color.Low)
        canvas.vline(x + 2, base_y - src_h, src_h)
        if first <= i <= last and evt.gate():
            evt_h = (evt.note() % 7 + 1) * scale
            canvas.set_color(Color.Bright)
            canvas.vline(x, base_y - evt_h, evt_h)
            canvas.vline(x + 1, base_y - evt_h, evt_h)
    canvas.set_color(Color.Bright)
    fx = 8 + first * (bar_w + bar_gap)
    lx = 8 + last * (bar_w + bar_gap) + bar_w
    canvas.vline(fx, base_y - 20, 20)
    canvas.vline(lx, base_y - 20, 20)
    canvas.hline(fx, base_y - 20, lx - fx)
    canvas.hline(fx, base_y, lx - fx)
    canvas.set_color(Color.Medium)
    canvas.hline(8, base_y, 16 * (bar_w + bar_gap))



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



def render_stochastic_lock_meadowphysics(canvas: Canvas, sequence, track_engine):
    """
    meadowphysics-derived multi-voice tracker strips.
    4 rows of 2x2 dots = gate, note, length, retrigger of captured events.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK MP")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])

    current_step = track_engine.current_step()
    dot_size = 2
    padding = 6
    offset_x = 12
    offset_y = 16
    first = sequence.lock_first()
    last = sequence.lock_last()
    rows = [
        ("G", lambda e: e.gate()),
        ("N", lambda e: e.note()),
        ("L", lambda e: e.length()),
        ("R", lambda e: e.retrigger()),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter) in enumerate(rows):
        row_y = offset_y + padding * row_idx
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y + 2, label)

        for i in range(16):
            evt = sequence.lock_event(i)
            x = offset_x + padding * i
            val = getter(evt)
            is_current = (i == current_step)
            in_window = first <= i <= last

            if is_current:
                canvas.set_color(Color.Bright)
            elif in_window and val:
                canvas.set_color(Color.MediumBright)
            elif in_window:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)

            canvas.fill_rect(x, row_y, dot_size, dot_size)




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



