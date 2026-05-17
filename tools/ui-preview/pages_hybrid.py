"""
Individual edit page renderers.
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


def render_stochastic_pitch_prob_melod(canvas: Canvas, sequence, track_engine, weights=None, active_note=4, active_octave=0):
    """Full ProbMeloD keyboard + weight bars for stochastic pitch page.
    Exact replica of ProbabilityMelody.h drawing at lines 342-418.
    Uses exact x-positions, key dimensions, and bar offsets from O_C source.
    """
    if weights is None:
        tickets = sequence.degree_tickets()
        weights = [min(10, max(-1, tickets[i])) if i < len(tickets) else 0 for i in range(12)]
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH KEY")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    # Exact x-table from ProbabilityMelody.h line 230 (128px screen coords)
    semitone_x = [2, 7, 10, 15, 18, 26, 31, 34, 39, 42, 47, 50]
    # Black-key pattern: 1=black, 0=white
    is_black = [0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0]
    # Note name chars (sharps share letter with natural)
    note_letters = ['C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B']
    canvas.set_blend_mode(BlendMode.Set)
    # --- Weight bars (DrawParams, lines 364-384) ---
    for i, w in enumerate(weights):
        # xOffset = x[i] + (p[i] ? 1 : 2)
        x_off = semitone_x[i] + (1 if is_black[i] else 2)
        # yOffset = p[i] ? 31 : 45
        y_off = 31 if is_black[i] else 45
        unmasked = (w >= 0)
        if unmasked:
            # Dotted vertical line
            canvas.set_color(Color.Medium)
            for dy in range(11):
                canvas.point(x_off, y_off + dy)
            # Weight tick: gfxLine(xOffset-1, yOffset+10-ws[i], xOffset+1, yOffset+10-ws[i])
            tick_y = y_off + 10 - w
            canvas.set_color(Color.Bright)
            canvas.hline(x_off - 1, tick_y, 3)
        else:
            # Excluded: draw X at bar position
            canvas.set_color(Color.Low)
            canvas.draw_text(x_off - 2, y_off + 4, "X")
    # --- Keyboard (DrawKeyboard, lines 342-362) ---
    # Keyboard frame: gfxFrame(0, 27, 63, 32)
    canvas.set_color(Color.Medium)
    canvas.draw_rect(0, 27, 63, 32)
    # White-key vertical lines
    for x in range(8):
        if x == 3 or x == 7:
            # Full-height line: gfxLine(x*8, 27, x*8, 58)
            canvas.vline(x * 8, 27, 32)
        else:
            # Lower line: gfxLine(x*8, 43, x*8, 58)
            canvas.vline(x * 8, 43, 16)
    # Black keys: gfxInvert((i*8)+6, 28, 5, 15) for i in [0,1,3,4,5] (skip i=2)
    black_key_indices = [0, 1, 3, 4, 5]
    for i in black_key_indices:
        bx = (i * 8) + 6
        canvas.set_color(Color.Bright)
        canvas.fill_rect(bx, 28, 5, 15)
    # --- Active note triangles (lines 385-389) ---
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    canvas.set_color(Color.Bright)
    # Draw note letter above keyboard for active note
    nx = semitone_x[active_note] + (1 if is_black[active_note] else 2) - 3
    canvas.draw_text(nx, 59, note_names[active_note])
    # Octave indicator at right
    canvas.draw_text(56, 59, f"O{active_octave}")
    # --- Range fences (parameter area, approximated) ---
    canvas.set_color(Color.Bright)
    if 0 <= sequence.min_degree() < 12:
        fx = semitone_x[sequence.min_degree()] + (1 if is_black[sequence.min_degree()] else 2)
        canvas.vline(fx, 20, 30)
    if 0 <= sequence.max_degree() < 12:
        fx = semitone_x[sequence.max_degree()] + (1 if is_black[sequence.max_degree()] else 2)
        canvas.vline(fx, 20, 30)


def render_stochastic_pitch_circle(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """Pitch class circle (Automatonnetz / OC_menus derived). Dot radius = ticket weight. Chord triangle connecting top 3."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH CIR")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    import math
    cx = 80
    cy = 34
    radius = 20
    tickets = sequence.degree_tickets()
    scale_size = min(12, sequence.scale_size())
    positions = []
    for i in range(scale_size):
        angle = math.radians(i * 360 / scale_size - 90)
        positions.append((int(cx + radius * math.cos(angle)), int(cy + radius * math.sin(angle))))
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    sorted_degrees = sorted(range(scale_size), key=lambda i: tickets[i] if i < len(tickets) and tickets[i] >= 0 else -999, reverse=True)
    top3 = [d for d in sorted_degrees[:3] if d < len(tickets) and tickets[d] >= 0]
    if len(top3) >= 3:
        canvas.set_color(Color.MediumBright)
        for i in range(len(top3)):
            x1, y1 = positions[top3[i]]
            x2, y2 = positions[top3[(i + 1) % len(top3)]]
            dx, dy = x2 - x1, y2 - y1
            steps = max(abs(dx), abs(dy))
            if steps > 0:
                for s in range(steps + 1):
                    canvas.point(x1 + dx * s // steps, y1 + dy * s // steps)
    for i in range(scale_size):
        px, py = positions[i]
        selected = (i == selected_degree)
        t = tickets[i] if i < len(tickets) else 0
        if t < 0:
            canvas.set_color(Color.Low)
            canvas.draw_text(px - 2, py - 2, "X")
        elif t == 0:
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.draw_rect(px - 1, py - 1, 3, 3)
        else:
            r = min(4, 1 + t // 2)
            canvas.set_color(Color.Bright if selected else Color.MediumBright)
            canvas.fill_rect(px - r, py - r, r * 2 + 1, r * 2 + 1)
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 3, cy - 2, f"{track_engine.current_note()}")
    canvas.set_color(Color.Medium)
    canvas.draw_text(140, 20, f"ROOT:{sequence.scale_root()}")
    canvas.draw_text(140, 28, f"SIZE:{scale_size}")
    canvas.draw_text(140, 36, f"LIN:{sequence.linearity()}")


def render_stochastic_pitch_less_concepts(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """Dense less-concepts-derived pitch page. 8-bit seed pattern, large note name, parameter text, preset grid."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH DNS")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(min(8, scale_size)):
        t = tickets[i] if i < len(tickets) else 0
        canvas.set_color(Color.Bright if i == selected_degree else (Color.Medium if t > 0 else Color.Low))
        canvas.fill_rect(5 * i, 14, 4, 4)
    canvas.set_color(Color.Bright)
    canvas.hline(5 * selected_degree, 13, 4)
    current_note = track_engine.current_note()
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    nn = note_names[current_note % 12]
    canvas.set_color(Color.Bright)
    canvas.draw_text(60, 28, nn)
    canvas.set_color(Color.Medium)
    canvas.draw_text(62, 26, nn)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(120, 20, f"deg:{selected_degree+1}")
    canvas.draw_text(120, 28, f"tix:{tickets[selected_degree] if selected_degree < len(tickets) else 0}")
    canvas.draw_text(120, 36, f"rot:{sequence.degree_rotation():+d}")
    canvas.draw_text(120, 44, f"lin:{sequence.linearity()}")
    canvas.draw_text(180, 20, f"min:{sequence.min_degree()}")
    canvas.draw_text(180, 28, f"max:{sequence.max_degree()}")
    canvas.draw_text(180, 36, f"root:{sequence.scale_root()}")
    for i in range(2):
        for j in range(8):
            idx = j + i * 8
            active = idx < scale_size and tickets[idx] >= 0
            canvas.set_color(Color.Medium if active else Color.Low)
            canvas.fill_rect(j * 5, 54 + i * 5, 4, 4)
            if idx == selected_degree:
                canvas.set_color(Color.Bright)
                canvas.draw_rect(j * 5 - 1, 53 + i * 5, 6, 6)


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


def render_stochastic_track_rndwalk(canvas: Canvas, sequence, track_engine, current_page: int = 0, selected_slot: int = 0):
    """RndWalk-derived inverted horizontal bar meter console.
    Exact params from RndWalk.h: gfxInvert(31, 48+(8*ch), w, 7).
    Scaled to 256px canvas: x=128, y=20+8*i, h=7, max_w=100.
    """
    param_pages = [
        [("OCT", "octave", -4, 4), ("TRANS", "transpose", -12, 12), ("DIV", "division", 1, 16), ("RESET", "reset_mode", 0, 4)],
        [("GBIAS", "gbias", -50, 50), ("NBIAS", "nbias", -50, 50), ("LBIAS", "lbias", -50, 50), ("RBIAS", "rbias", -50, 50)],
        [("ACC", "accent", 0, 100), ("LEG", "legato", 0, 100), ("FILL", "fill", 0, 100), ("REST", "rest_prob", 0, 100)],
    ]
    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK RW P{current_page+1}")
    # Exact RndWalk params scaled to 256px canvas
    bar_x = 128       # scaled from 31
    bar_h = 7         # exact from RndWalk.h
    max_w = 100       # scaled from 31
    y0 = 20           # scaled from 48, adjusted to clear header
    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr, minv, maxv) in enumerate(page):
        y = y0 + i * 8  # exact 8px spacing from RndWalk.h
        val = getattr(sequence, attr)()
        canvas.set_color(Color.Bright if selected_slot == i else Color.Medium)
        canvas.draw_text(4, y + 5, name)
        if minv < 0:
            w = (abs(val) * max_w) // max(abs(minv), abs(maxv))
            if val >= 0:
                canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
                canvas.fill_rect(bar_x, y, w, bar_h)
            else:
                canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
                canvas.fill_rect(bar_x - w, y, w, bar_h)
            canvas.set_color(Color.Medium)
            canvas.vline(bar_x, y, bar_h)
        else:
            w = (val * max_w) // maxv
            canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
            canvas.fill_rect(bar_x - max_w // 2, y, w, bar_h)
        canvas.set_color(Color.Bright)
        canvas.draw_text(bar_x + max_w + 8, y + 5, str(val))
    box_x, box_y = 180, 14
    canvas.set_color(Color.Medium)
    canvas.draw_rect(box_x, box_y, 72, 36)
    canvas.set_color(Color.Bright)
    canvas.draw_text(box_x + 4, box_y + 7, f"S{track_engine.current_step()+1}")
    canvas.draw_text(box_x + 4, box_y + 15, "L" if sequence.lock_enabled() else "-")
    canvas.draw_text(box_x + 4, box_y + 23, f"N{track_engine.current_note()}")
    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)


def render_stochastic_track_sequencex(canvas: Canvas, sequence, track_engine,
                                       current_page: int = 0, selected_slot: int = 0):
    """
    SequenceX-derived vertical slider columns.
    Dotted line per param + 5x3 rect indicator at value.
    """
    param_pages = [
        [("OCT", "octave", -4, 4), ("TRANS", "transpose", -12, 12),
         ("DIV", "division", 1, 16), ("RESET", "reset_mode", 0, 4)],
        [("GBIAS", "gbias", -50, 50), ("NBIAS", "nbias", -50, 50),
         ("LBIAS", "lbias", -50, 50), ("RBIAS", "rbias", -50, 50)],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK SX P{current_page+1}")

    slider_y0 = 18
    slider_h = 36
    col_width = 51
    zero_y = slider_y0 + slider_h // 2

    canvas.set_blend_mode(BlendMode.Set)

    for i, (name, attr, minv, maxv) in enumerate(page):
        x = i * col_width + col_width // 2
        val = getattr(sequence, attr)()

        # Dotted zero line
        canvas.set_color(Color.Low)
        for dx in range(x - 2, x + 3, 2):
            canvas.point(dx, zero_y)

        # Dotted slider line
        canvas.set_color(Color.Medium)
        for y in range(slider_y0, slider_y0 + slider_h, 2):
            canvas.point(x, y)

        # Value indicator rect (5x3)
        range_span = maxv - minv
        pos_y = slider_y0 + slider_h - 2 - ((val - minv) * (slider_h - 4) // range_span)
        pos_y = max(slider_y0, min(slider_y0 + slider_h - 3, pos_y))

        if i == selected_slot:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x - 2, pos_y, 5, 3)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(x - 2, pos_y, 5, 3)

        # Label
        canvas.set_color(Color.Bright if i == selected_slot else Color.Medium)
        canvas.draw_text(x - canvas.text_width(name) // 2, slider_y0 + slider_h + 2, name)

        # Value text
        canvas.set_color(Color.Bright)
        canvas.draw_text(x - canvas.text_width(str(val)) // 2, slider_y0 - 4, str(val))

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)


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


def render_stochastic_pitch_kreislauf(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """
    Kreislauf-derived concentric ring wedges.
    Scale degrees as wedges in a ring. Brightness = ticket count.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH KRS")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])

    import math
    cx = 80
    cy = 34
    r_outer = 22
    r_inner = 12
    tickets = sequence.degree_tickets()
    scale_size = min(16, sequence.scale_size())

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(scale_size):
        t = tickets[i] if i < len(tickets) else 0
        selected = (i == selected_degree)

        angle1 = math.radians(i * 360 / scale_size - 90)
        angle2 = math.radians((i + 1) * 360 / scale_size - 90)

        # Wedge brightness by ticket count
        if t < 0:
            canvas.set_color(Color.Low)
        elif t == 0:
            canvas.set_color(Color.Medium)
        elif t >= 5:
            canvas.set_color(Color.Bright if selected else Color.MediumBright)
        else:
            canvas.set_color(Color.MediumBright if selected else Color.Medium)

        # Draw wedge as filled polygon (4-point)
        p1 = (int(cx + r_outer * math.cos(angle1)), int(cy + r_outer * math.sin(angle1)))
        p2 = (int(cx + r_outer * math.cos(angle2)), int(cy + r_outer * math.sin(angle2)))
        p3 = (int(cx + r_inner * math.cos(angle2)), int(cy + r_inner * math.sin(angle2)))
        p4 = (int(cx + r_inner * math.cos(angle1)), int(cy + r_inner * math.sin(angle1)))

        # Simple fill: scanline between inner and outer radius
        for a in range(int(math.degrees(angle1)), int(math.degrees(angle2)) + 1):
            rad = math.radians(a)
            xo = int(cx + r_outer * math.cos(rad))
            yo = int(cy + r_outer * math.sin(rad))
            xi = int(cx + r_inner * math.cos(rad))
            yi = int(cy + r_inner * math.sin(rad))
            dx = xo - xi
            dy = yo - yi
            steps = max(abs(dx), abs(dy))
            if steps > 0:
                for s in range(steps + 1):
                    canvas.point(xi + dx * s // steps, yi + dy * s // steps)

        # Wedge outline
        canvas.set_color(Color.Bright if selected else Color.Medium)
        for s in range(100):
            canvas.point(p1[0] + (p2[0]-p1[0])*s//100, p1[1] + (p2[1]-p1[1])*s//100)
            canvas.point(p2[0] + (p3[0]-p2[0])*s//100, p2[1] + (p3[1]-p2[1])*s//100)
            canvas.point(p3[0] + (p4[0]-p3[0])*s//100, p3[1] + (p4[1]-p3[1])*s//100)
            canvas.point(p4[0] + (p1[0]-p4[0])*s//100, p4[1] + (p1[1]-p4[1])*s//100)

    # Center info
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 3, cy - 2, f"{scale_size}")



def render_stochastic_pitch_bline(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """
    bline-derived pattern bars for pitch degrees.
    3px wide vertical bars, height = tickets. Current degree = bright top.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH BLN")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])

    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    bar_w = 3
    bar_gap = 1
    base_y = 48
    bar_max_h = 30

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(scale_size):
        t = tickets[i] if i < len(tickets) else 0
        x = 8 + i * (bar_w + bar_gap)
        selected = (i == selected_degree)

        if t < 0:
            canvas.set_color(Color.Low)
            canvas.draw_text(x - 1, base_y - 6, "X")
        elif t == 0:
            h = 2
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.vline(x, base_y - h, h)
        else:
            h = min(bar_max_h, t * 3)
            is_current = selected
            # Bar body
            canvas.set_color(Color.MediumBright if is_current else Color.Medium)
            canvas.vline(x, base_y - h, h)
            # Bar top
            canvas.set_color(Color.Bright if is_current else Color.MediumBright)
            canvas.point(x, base_y - h - 1)

        # Degree number
        canvas.set_color(Color.Bright if selected else Color.Medium)
        canvas.draw_text(x - 1, base_y + 2, f"{i+1}")

# ---------------------------------------------------------------------------
# Dice variants
# ---------------------------------------------------------------------------


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


def render_stochastic_track_bline(canvas: Canvas, sequence, track_engine,
                                   current_page: int = 0, selected_slot: int = 0):
    """
    bline-derived 4-quadrant 2x2 parameter grid.
    Each cell = 19x19 px. Selected = bright background.
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    cell_w = 19
    cell_h = 19
    grid_x = 10
    grid_y = 14

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK BLN P{current_page+1}")

    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr) in enumerate(page):
        col = i % 2
        row = i // 2
        x = grid_x + col * (cell_w + 1)
        y = grid_y + row * (cell_h + 1)
        val = getattr(sequence, attr)()
        selected = (i == selected_slot)

        # Cell background
        if selected:
            canvas.set_color(Color.Medium)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Bright
        else:
            canvas.set_color(Color.Low)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Medium

        # Label
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 6, name)
        # Value
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 14, str(val))

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)



def render_stochastic_track_pitter_patter(canvas: Canvas, sequence, track_engine,
                                           current_page: int = 0, selected_slot: int = 0):
    """
    pitter-patter-derived grid squares + parameter text.
    4 columns with 5x5 brightness square + value below.
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    col_width = 51

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK PP P{current_page+1}")

    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr) in enumerate(page):
        x = i * col_width
        val = getattr(sequence, attr)()
        selected = (i == selected_slot)

        # 5x5 brightness square
        sq_x = x + (col_width - 5) // 2
        sq_y = 20
        if selected:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(sq_x, sq_y, 5, 5)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(sq_x, sq_y, 5, 5)

        # Label
        canvas.set_color(Color.Bright if selected else Color.Medium)
        canvas.draw_text(x + (col_width - canvas.text_width(name)) // 2, 30, name)

        # Value
        val_str = str(val)
        canvas.set_color(Color.Bright)
        canvas.draw_text(x + (col_width - canvas.text_width(val_str)) // 2, 38, val_str)

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)




