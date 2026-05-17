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

def render_stochastic_steps_skylines(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps as city-skyline bar chart (skylines-derived). Gate probability = foreground bar height. Note probability = background dim bar."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS SKY")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    step_offset = section * 16
    current_step = track_engine.current_step()
    # From skylines.lua: move(3,58) then line_rel(5,0), then per step: line_rel(0,-5*reps), line_rel(4,0), line_rel(0,5*reps), line_rel(4,0)
    # Bar width = 4px, gap = 4px, base_y = 58 (bg) / 60 (fg), scale = 5px per rep
    base_y_bg = 50
    base_y_fg = 52
    bar_w = 4
    bar_gap = 4
    scale = 5
    canvas.set_blend_mode(BlendMode.Set)
    # Background voice (note probability) — dim, at y=58
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = 3 + 5 + i * (bar_w + bar_gap)  # 3px lead-in + 5px first segment, then each bar
        np_h = (step.note_probability() * 8) // 100  # scale to ~0..40
        if np_h > 0:
            canvas.set_color(Color.Low)
            canvas.vline(x, base_y_bg - np_h, np_h)
            canvas.vline(x + 1, base_y_bg - np_h, np_h)
    # Foreground voice (gate probability) — bright, at y=60
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = 1 + 5 + i * (bar_w + bar_gap)  # 1px lead-in + 5px first segment
        gp_h = (step.gate_probability() * 8) // 100
        canvas.set_color(Color.Bright if step_index == current_step else (Color.MediumBright if step.gate() else Color.Medium))
        canvas.vline(x, base_y_fg - gp_h, gp_h)
        canvas.vline(x + 1, base_y_fg - gp_h, gp_h)
        canvas.vline(x + 2, base_y_fg - gp_h, gp_h)
        canvas.vline(x + 3, base_y_fg - gp_h, gp_h)




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



