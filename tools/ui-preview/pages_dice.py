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

def render_stochastic_dice_shredder(canvas: Canvas, sequence, track_engine, axis: int = 0, active_cell: int = 5):
    """8x8 Cartesian grid (Shredder-derived) for 64-step dice view.
    Exact replica of Shredder.h DrawGrid() and DrawMeters().
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE SHR")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    # From Shredder.h: gfxFrame(1 + (8 * (s % 4)), 26 + (8 * (s / 4)), 5, 5)
    # 4x4 grid of 5x5 frames at 8px spacing, starting at (1, 26)
    # But we need 8x8 for 64 steps, so scale: 8x8 at 4px spacing with 3x3 frames
    grid_x0 = 1
    grid_y0 = 26
    cell_spacing = 4
    cell_size = 3
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    # Draw 8x8 grid frames
    for row in range(8):
        for col in range(8):
            step_index = row * 8 + col
            step = sequence.step(step_index)
            x = grid_x0 + col * cell_spacing
            y = grid_y0 + row * cell_spacing
            if axis == 0: prob = step.gate_probability()
            elif axis == 1: prob = step.note_probability()
            elif axis == 2: prob = step.length_probability()
            else: prob = step.retrigger_probability()
            is_current = (step_index == current_step)
            is_active = (step_index == active_cell)
            # Frame
            canvas.set_color(Color.Medium)
            canvas.draw_rect(x, y, cell_size, cell_size)
            # Fill by probability
            if prob >= 75: canvas.set_color(Color.Bright)
            elif prob >= 50: canvas.set_color(Color.MediumBright)
            elif prob > 0: canvas.set_color(Color.Medium)
            else: canvas.set_color(Color.None_)
            if prob > 0 or is_current:
                canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)
            # Active cell: filled rect per Shredder.h line 305
            if is_active:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x, y, cell_size, cell_size)
    # Crosshairs: gfxDottedLine(3 + (8 * cxx), 26, 3 + (8 * cxx), 58, 2)
    # For 8x8 at 4px spacing: crosshair x = grid_x0 + 1 + (active_cell % 8) * cell_spacing
    cxx = active_cell % 8
    cxy = active_cell // 8
    ax = grid_x0 + 1 + cxx * cell_spacing
    ay = grid_y0 + 1 + cxy * cell_spacing
    canvas.set_color(Color.Bright)
    # Vertical dotted line through column
    for y in range(grid_y0, grid_y0 + 8 * cell_spacing, 2):
        canvas.point(ax, y)
    # Horizontal dotted line through row
    for x in range(grid_x0, grid_x0 + 8 * cell_spacing, 2):
        canvas.point(x, ay)



def render_stochastic_dice_register(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """64-step register bar horizon (ShiftReg-derived).
    Exact replica of ShiftReg.h register bars: gfxRect(60-(4*b), 47, 3, 14).
    Four 16-bit registers side-by-side to show 64 steps.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE REG")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    # Exact params from ShiftReg.h: bar width=3, height=14, spacing=4, y=47
    bar_w = 3
    bar_h = 14
    spacing = 4
    base_y = 24  # moved up from 47 to clear footer, keeping exact bar height
    reg_width = 64  # 16 bars * 4px spacing
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(4):
        reg_x0 = row * reg_width
        for col in range(16):
            step_index = row * 16 + col
            step = sequence.step(step_index)
            # Exact x formula from ShiftReg.h: 60 - (4 * b), scaled per register
            x = reg_x0 + (60 - (spacing * col))
            if axis == 0: prob = step.gate_probability()
            elif axis == 1: prob = step.note_probability()
            elif axis == 2: prob = step.length_probability()
            else: prob = step.retrigger_probability()
            is_current = (step_index == current_step)
            # Bar height proportional to probability
            h = (prob * bar_h) // 100
            if is_current:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x, base_y, bar_w, bar_h)
            elif h > 0:
                canvas.set_color(Color.MediumBright if prob >= 50 else Color.Medium)
                canvas.fill_rect(x, base_y + bar_h - h, bar_w, h)
            else:
                canvas.set_color(Color.Low)
                canvas.vline(x + 1, base_y + bar_h - 3, 3)
    # Separator lines between registers
    canvas.set_color(Color.Low)
    for row in range(1, 4):
        canvas.vline(row * reg_width - 1, base_y, bar_h)



def render_stochastic_dice_grd(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """
    grd-derived 8x8 brightness grid.
    5x5 px rects, brightness = probability for active axis.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE GRD")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    current_step = track_engine.current_step()
    # Exact params from grd.lua: rect(i*6, j*6, 5, 5) -> spacing=6, cell=5
    cell_size = 5
    spacing = 6
    grid_x0 = 8
    grid_y0 = 14

    canvas.set_blend_mode(BlendMode.Set)
    for row in range(8):
        for col in range(8):
            step_index = row * 8 + col
            step = sequence.step(step_index)
            x = grid_x0 + col * spacing
            y = grid_y0 + row * spacing

            if axis == 0:
                prob = step.gate_probability()
            elif axis == 1:
                prob = step.note_probability()
            elif axis == 2:
                prob = step.length_probability()
            else:
                prob = step.retrigger_probability()

            is_current = (step_index == current_step)

            # Brightness mapping
            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob >= 50:
                canvas.set_color(Color.Medium)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)

            canvas.fill_rect(x, y, cell_size - 1, cell_size - 1)




def render_stochastic_dice_kreislauf(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """
    Kreislauf-derived concentric ring probability wheel.
    Exact formula from kreislauf_app.lua: r=6, r=r+(width/num_rings)/2.5, inner=r-6.
    4 rings = 4 probability layers. 16 wedges per ring.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE KRS")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    import math
    cx = 80
    cy = 34
    # Exact kreislauf formula: start r=6, increment=(width/4)/2.5, inner=r-6
    width = 40
    increment = (width / 4) / 2.5  # = 4.0
    r = 6
    ring_radii = []
    for _ in range(4):
        r = r + increment
        ring_radii.append((r - 6, r))  # (inner, outer)
    rings = [
        ("GATE", lambda s: s.gate_probability(), ring_radii[0][1], ring_radii[0][0]),
        ("NOTE", lambda s: s.note_probability(), ring_radii[1][1], ring_radii[1][0]),
        ("LEN", lambda s: s.length_probability(), ring_radii[2][1], ring_radii[2][0]),
        ("RTRG", lambda s: s.retrigger_probability(), ring_radii[3][1], ring_radii[3][0]),
    ]
    current_step = track_engine.current_step()

    canvas.set_blend_mode(BlendMode.Set)
    for ring_idx, (label, getter, r_out, r_in) in enumerate(rings):
        for i in range(16):
            step_index = i
            step = sequence.step(step_index)
            prob = getter(step)
            is_current = (step_index == current_step)

            angle1 = math.radians(i * 360 / 16 - 90)
            angle2 = math.radians((i + 1) * 360 / 16 - 90)

            # Brightness
            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob >= 50:
                canvas.set_color(Color.Medium)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)

            # Draw wedge radial line (kreislauf stueck style)
            for a in range(int(math.degrees(angle1)), int(math.degrees(angle2)) + 1):
                rad = math.radians(a)
                xo = int(cx + r_out * math.cos(rad))
                yo = int(cy + r_out * math.sin(rad))
                xi = int(cx + r_in * math.cos(rad))
                yi = int(cy + r_in * math.sin(rad))
                dx = xo - xi
                dy = yo - yi
                steps = max(abs(dx), abs(dy))
                if steps > 0:
                    for s in range(steps + 1):
                        canvas.point(xi + dx * s // steps, yi + dy * s // steps)

    # Labels right side
    for ring_idx, (label, getter, r_out, r_in) in enumerate(rings):
        canvas.set_color(Color.Medium)
        canvas.draw_text(140, 16 + ring_idx * 8, label)

# ---------------------------------------------------------------------------
# Marbles variants
# ---------------------------------------------------------------------------



