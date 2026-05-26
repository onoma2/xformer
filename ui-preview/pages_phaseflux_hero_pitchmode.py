"""
PhaseFlux pitchMode hero renders: Cell vs Global PTCH set.

Cell mode = today's PTCH layout (badge stage#, F4=Base, F5=Rng).
Global mode = stage[0]'s curve/warp/response globalized + F4=Rate (P:T ratio),
              F5 cell shows active stage's note name with range dots BELOW.
              Range demoted to Shift+F5 (cycles enum, wraps).
              Badge inside scope shows "G" instead of stage#.
"""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter

from pages_phaseflux import (
    Stage, demo_sequence,
    PITCH_CURVE_NAMES,
    draw_grid,
    SCOPE_TEMP_X, SCOPE_W, SCOPE_H, SCOPE_Y,
)
from pages_phaseflux_hero import (
    PARAM_PANEL_X, PARAM_PANEL_W,
    DIVIDER_X_PARAM,
    _footer, _signed, _fmt_warp,
    draw_dividers, _draw_pitch_scope_at, hero_demo_stage,
)


# Note-name table mirrors NoteSequenceEditPage convention (chromatic).
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def basepitch_to_note_name(base: float, root_midi: int = 60) -> str:
    midi = root_midi + int(round(base))
    name = NOTE_NAMES[midi % 12]
    octave = (midi // 12) - 1
    return f"{name}{octave}"


def range_to_dot_count(pitch_range: float) -> int:
    if pitch_range <= 0.2: return 1
    if pitch_range <= 0.8: return 2
    if pitch_range <= 1.5: return 3
    if pitch_range <= 3.0: return 4
    return 5


# Footer labels.
FOOTER_PTCH_CELL         = ["Curve", "Warp",  "Resp", "Base",  "Span"]
FOOTER_PTCH_CELL_SHIFT   = ["FlipV", "FlipH", None,   None,    None]
FOOTER_PTCH_GLOBAL       = ["Curve", "Warp",  "Resp", "Rate",  "Note"]
FOOTER_PTCH_GLOBAL_SHIFT = ["FlipV", "FlipH", None,   None,    "Span"]


# Asymmetric row layout — last row is taller so F5 can stack note + dots.
PARAM_PANEL_Y = 12
ROW_HEIGHTS  = [7, 7, 7, 7, 13]
ROW_Y        = [PARAM_PANEL_Y + sum(ROW_HEIGHTS[:i]) for i in range(5)]


def _draw_header(canvas: Canvas, mode_marker: str):
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, mode_marker)


def _draw_stage_badge(canvas: Canvas, badge: str, scope_x: int):
    canvas.set_font(Font.Tiny)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.MediumBright)
    canvas.draw_text(scope_x + 2, SCOPE_Y + 6, badge)


def _draw_param_list(canvas: Canvas,
                     rows,
                     selected_idx: int,
                     note_row_idx: int = None,
                     note_name: str = None,
                     range_dots: int = 0):
    canvas.set_blend_mode(BlendMode.Set)

    # Match C++: highlight only the right ~1/3 (value column), not full row.
    value_col_w = PARAM_PANEL_W // 3
    value_col_x = PARAM_PANEL_X + PARAM_PANEL_W - value_col_w

    for i, (name, value, badge) in enumerate(rows):
        rx = PARAM_PANEL_X
        rw = PARAM_PANEL_W
        ry = ROW_Y[i]
        rh = ROW_HEIGHTS[i] - 1
        is_selected = (i == selected_idx)

        if is_selected:
            name_color  = Color.Medium       # label stays static even when selected
            value_color = Color.Bright
            canvas.set_color(Color.Bright)
            canvas.draw_rect(value_col_x, ry, value_col_w, rh)
        else:
            name_color  = Color.Medium
            value_color = Color.MediumBright

        # Name flush-left, Tiny font for all rows
        canvas.set_font(Font.Tiny)
        text_y = ry + 6  # baseline-ish for Tiny
        canvas.set_color(name_color)
        canvas.draw_text(rx + 2, text_y, name)

        right_x = rx + rw - 2

        if i == note_row_idx and note_name is not None:
            # Note name in Tiny (same as other rows), flush right on upper half
            canvas.set_font(Font.Tiny)
            vw = canvas.text_width(note_name)
            canvas.set_color(value_color)
            canvas.draw_text(right_x - vw, ry + 6, note_name)
            # Dots beneath the note name, flush right
            dot_y = ry + rh - 2
            dot_x = right_x
            canvas.set_color(value_color)
            for _ in range(range_dots):
                canvas.fill_rect(dot_x - 2, dot_y - 1, 2, 2)
                dot_x -= 4
        else:
            canvas.set_font(Font.Tiny)
            if badge:
                bw = canvas.text_width(f"[{badge}]")
                canvas.set_color(name_color)
                canvas.draw_text(right_x - bw, text_y, f"[{badge}]")
                right_x -= bw + 2
            vw = canvas.text_width(value)
            canvas.set_color(value_color)
            canvas.draw_text(right_x - vw, text_y, value)


def _cell_rows(stage):
    return [
        ("Curve", PITCH_CURVE_NAMES[stage.pitchCurve], None),
        ("Warp",  _fmt_warp(stage.pitchWarp), None),
        ("Resp",  _fmt_warp(stage.pitchResponse), None),
        ("Base",  _signed(int(round(stage.basePitch))), None),
        ("Span",   f"{stage.pitchRange:.1f}", None),
    ]


def _global_rows(stage_master, rate_text, shift_held: bool):
    f5_label = "Span" if shift_held else "Note"
    return [
        ("Curve", PITCH_CURVE_NAMES[stage_master.pitchCurve], None),
        ("Warp",  _fmt_warp(stage_master.pitchWarp), None),
        ("Resp",  _fmt_warp(stage_master.pitchResponse), None),
        ("Rate",  rate_text, None),
        (f5_label, "", None),   # placeholder; rendered specially via note_row_idx
    ]


def render_pitch_cell_mode(canvas, stages, active_idx, selected_idx,
                           edit_slot_idx, shift_held):
    WindowPainter.clear(canvas)
    _draw_header(canvas, "PTCH")
    footer = _footer(FOOTER_PTCH_CELL, FOOTER_PTCH_CELL_SHIFT, shift_held)
    WindowPainter.draw_footer(canvas, names=footer, highlight=edit_slot_idx)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_dividers(canvas)
    _draw_pitch_scope_at(canvas, stages[selected_idx], x=SCOPE_TEMP_X)
    _draw_stage_badge(canvas, str(selected_idx), SCOPE_TEMP_X)

    rows = _cell_rows(stages[selected_idx])
    _draw_param_list(canvas, rows, edit_slot_idx)


def render_pitch_global_mode(canvas, stages, active_idx, selected_idx,
                             edit_slot_idx, shift_held, rate_text="1:1"):
    WindowPainter.clear(canvas)
    _draw_header(canvas, "PTCH.G")
    footer = _footer(FOOTER_PTCH_GLOBAL, FOOTER_PTCH_GLOBAL_SHIFT, shift_held)
    WindowPainter.draw_footer(canvas, names=footer, highlight=edit_slot_idx)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_dividers(canvas)
    _draw_pitch_scope_at(canvas, stages[0], x=SCOPE_TEMP_X)  # stage[0] = master
    _draw_stage_badge(canvas, "G", SCOPE_TEMP_X)

    rows = _global_rows(stages[0], rate_text, shift_held)
    active_stage = stages[selected_idx]
    note_name = basepitch_to_note_name(active_stage.basePitch)
    range_dots = range_to_dot_count(active_stage.pitchRange)
    _draw_param_list(canvas, rows, edit_slot_idx,
                     note_row_idx=4,
                     note_name=note_name,
                     range_dots=range_dots)


def main():
    stages = demo_sequence()
    s0 = hero_demo_stage()
    s0.pitchCurve = 3      # Bell
    s0.pitchWarp = 0.45
    s0.pitchResponse = -0.2
    stages[0] = s0
    s5 = hero_demo_stage()
    s5.basePitch = 7.0     # → G4
    s5.pitchRange = 1.5    # → 3 dots
    stages[5] = s5

    out_dir = os.path.join(os.path.dirname(__file__), "phaseflux-pitchmode")
    os.makedirs(out_dir, exist_ok=True)

    variants = [
        ("phaseflux-pitchmode-cell.png",
         lambda c: render_pitch_cell_mode(c, stages, active_idx=5,
                                           selected_idx=5,
                                           edit_slot_idx=3,
                                           shift_held=False)),
        ("phaseflux-pitchmode-global.png",
         lambda c: render_pitch_global_mode(c, stages, active_idx=5,
                                             selected_idx=5,
                                             edit_slot_idx=3,
                                             shift_held=False,
                                             rate_text="3:2")),
        ("phaseflux-pitchmode-global-shift.png",
         lambda c: render_pitch_global_mode(c, stages, active_idx=5,
                                             selected_idx=5,
                                             edit_slot_idx=4,
                                             shift_held=True,
                                             rate_text="3:2")),
    ]

    paths = []
    for name, fn in variants:
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        fn(c)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, name)
        img.save(path)
        paths.append(path)
        print(f"wrote {path}")
    return paths


if __name__ == "__main__":
    main()
