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
    base_y_bg = 58
    base_y_fg = 60
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



def render_stochastic_steps_delinquencer(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps as 4x16 delinquencer grid with corner-pixel probability erosion."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS DEL")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    step_offset = section * 16
    current_step = track_engine.current_step()
    cell_size = 5
    cell_gap = 2
    grid_x0 = 8
    grid_y0 = 14
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(4):
        for col in range(16):
            step_index = step_offset + row * 16 + col
            if step_index >= 64: break
            step = sequence.step(step_index)
            x = grid_x0 + col * (cell_size + cell_gap)
            y = grid_y0 + row * (cell_size + cell_gap)
            prob = step.gate_probability()
            if prob == 100: canvas.set_color(Color.Bright)
            elif prob >= 50: canvas.set_color(Color.MediumBright)
            elif prob > 0: canvas.set_color(Color.Medium)
            else: canvas.set_color(Color.Low)
            canvas.draw_rect(x, y, cell_size, cell_size)
            if prob > 0:
                canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)
            if 0 < prob < 100:
                corners = 4 - (prob // 25)
                canvas.set_blend_mode(BlendMode.Set)
                canvas.set_color(Color.None_)
                if corners >= 1: canvas.point(x + 1, y + 1)
                if corners >= 2: canvas.point(x + cell_size - 2, y + 1)
                if corners >= 3: canvas.point(x + 1, y + cell_size - 2)
                if corners >= 4: canvas.point(x + cell_size - 2, y + cell_size - 2)
                canvas.set_blend_mode(BlendMode.Set)
            if step_index == current_step:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x + 2, y + 2, 2, 2)



def render_stochastic_steps_euclid(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps arranged on a Euclidean ring (squares-and-circles derived). Square = deterministic gate on. Circle = probabilistic/off."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS EUC")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    import math
    # From EuclidRythm.cpp: draw_eclid_cyrcle(x, y, ...) with circle at (x+3, 31+3), radius=21
    # Two rings side by side at x=0 and x=64 on 128px screen
    cx = 3 + 32   # left ring center x (scaled to fit 256px canvas: 3*2=6, but use 35 for centering)
    cy = 34       # circle center y = 31+3
    radius = 21   # exact radius from EuclidRythm.cpp
    step_offset = section * 16
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    # Guide circle
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        angle = math.radians(i * 360 / 16 - 90)
        px = int(cx + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        prob = step.gate_probability()
        # Cursor: 5x5 hollow rect at (x+1, y+1) per EuclidRythm.cpp
        if step_index == current_step:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(px + 1, py + 1, 5, 5)
        # Active/deterministic: 3x3 fillRect at (x+2, y+2)
        if step.gate() and prob >= 75:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px + 2, py + 2, 3, 3)
        elif prob > 0:
            # Probabilistic: drawCircle(x+3, y+3, 3) -> hollow circle radius 3
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px + 1, py + 1, 5, 5)
        else:
            canvas.set_color(Color.Low)
            canvas.point(px + 3, py + 3)
    # Second ring (note probability) at x=64 on 128px screen -> 128 on 256px
    cx2 = 128 + 3
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx2 + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    for i in range(16):
        step = sequence.step(step_offset + i)
        angle = math.radians(i * 360 / 16 - 90)
        px = int(cx2 + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        np = step.note_probability()
        if np >= 75:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px + 2, py + 2, 3, 3)
        elif np > 0:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px + 1, py + 1, 5, 5)
        else:
            canvas.set_color(Color.Low)
            canvas.point(px + 3, py + 3)



def render_stochastic_steps_bline(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    bline-derived stacked pattern bar charts.
    5 rows: gate, note, length, retrigger, condition.
    Each row = 16 vertical bars (3px wide). Bar top highlighted at current step.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS BLN")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    bar_w = 3
    bar_gap = 1
    base_y = 48
    rows = [
        ("G", lambda s: s.gate_probability(), 6),
        ("N", lambda s: s.note(), 12),
        ("L", lambda s: s.length() * 3, 6),
        ("R", lambda s: s.retrigger() * 2, 4),
        ("C", lambda s: s.condition(), 4),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter, scale) in enumerate(rows):
        row_y = base_y - row_idx * 9
        # Row label
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y - 1, label)

        for i in range(16):
            step_index = step_offset + i
            step = sequence.step(step_index)
            x = 12 + i * (bar_w + bar_gap)
            val = getter(step)
            h = min(8, val // scale) if scale > 0 else 0
            is_current = (step_index == current_step)

            # Bar body
            canvas.set_color(Color.MediumBright if is_current else Color.Low)
            canvas.vline(x, row_y - h, h)
            # Bar top
            canvas.set_color(Color.Bright if is_current else Color.Medium)
            canvas.point(x, row_y - h - 1)




def render_stochastic_steps_hiswing(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    Hiswing-derived vertical line chart.
    Exact params from Hiswing.lua: width=120/16, x=4+(i-1)*width, base_y=64.
    Scaled 2x for 256px canvas: width=15, x=8+i*15, base_y=52.
    Swing offset for even steps: +3px (scaled from +1..5 range).
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS HSW")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    # Exact Hiswing params scaled 2x for 256px canvas
    base_y = 52       # scaled from 64, clearing footer at 53
    step_w = 15       # 120/16 = 7.5, scaled 2x = 15
    x0 = 8            # scaled from 4

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        # Exact formula: x = 4 + (i-1)*width, scaled: x = 8 + i*15
        x = x0 + i * step_w

        # Swing offset for even steps (Hiswing get_visual_swing_offset)
        if i % 2 == 1:
            x += 3  # scaled swing offset

        # Height from note value (Hiswing: util.linlin(24, 84, 4, 40, note))
        note_h = min(30, (step.note() * 4) // 3 + 4)
        prob = step.gate_probability()
        is_current = (step_index == current_step)
        is_selected = (i == 3)  # demo selected

        if is_current:
            # Current step: line 4px higher (Hiswing selected_step behavior)
            canvas.set_color(Color.Bright)
            canvas.vline(x, base_y - note_h - 4, note_h + 4)
        elif is_selected:
            canvas.set_color(Color.MediumBright)
            canvas.vline(x, base_y - note_h, note_h)
        else:
            # Probability = brightness
            if prob >= 75:
                canvas.set_color(Color.Medium)
            elif prob >= 50:
                canvas.set_color(Color.MediumBright)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)
            canvas.vline(x, base_y - note_h, note_h)




def render_stochastic_steps_meadowphysics(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    meadowphysics-derived horizontal tracker strips.
    4 rows of 2x2 px dots across 16 steps. Dot brightness = probability.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS MP")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    dot_size = 2
    padding = 6
    offset_x = 12
    offset_y = 16
    rows = [
        ("GATE", lambda s: s.gate_probability()),
        ("NOTE", lambda s: s.note_probability()),
        ("LEN", lambda s: s.length_probability()),
        ("RTRG", lambda s: s.retrigger_probability()),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter) in enumerate(rows):
        row_y = offset_y + padding * row_idx
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y + 2, label[:1])

        for i in range(16):
            step_index = step_offset + i
            step = sequence.step(step_index)
            x = offset_x + padding * i
            prob = getter(step)
            is_current = (step_index == current_step)

            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob > 0:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)

            canvas.fill_rect(x, row_y, dot_size, dot_size)

# ---------------------------------------------------------------------------
# Pitch variants
# ---------------------------------------------------------------------------



