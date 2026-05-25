"""
PhaseFlux Phase-C hero pages: TEMP and PTCH variants.

Layout (both pages share the 4x4 grid on the left + vertical divider at x=46):

  TEMP page:
    middle pane (x=50..151)  — temporal waveform scope (existing)
    right pane  (x=156..253) — 5-row param list:
      Curve | Warp | Resp | Gate | Puls
    Shift indicators appended where the slot has a binary toggle.

  PTCH page:
    middle pane (x=50..151)  — pitch waveform scope (MOVED from current right slot)
    right pane  (x=156..253) — 5-row param list:
      Curve | Warp | Resp | Base | Rng

Both pages show the currently-edited stage number in the top-right of the
header (replaces the "PHFLX" mode label in the existing preview).
"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter

# Re-use existing PhaseFlux primitives.
from pages_phaseflux import (
    Stage, demo_sequence,
    TEMP_CURVE_NAMES, PITCH_CURVE_NAMES, MASK_NAMES, DIVISOR_NAMES,
    draw_grid, draw_temporal_scope, draw_pitch_scope,
    DIVIDER_X1, SCOPE_TEMP_X, SCOPE_PITCH_X, SCOPE_W, SCOPE_H, SCOPE_Y,
)


# Right-pane param list geometry.
PARAM_PANEL_X      = 156
PARAM_PANEL_W      = PageWidth - PARAM_PANEL_X - 2   # ~98 px
PARAM_PANEL_Y      = 12
PARAM_PANEL_H      = 40
PARAM_ROW_COUNT    = 5
PARAM_ROW_H        = PARAM_PANEL_H // PARAM_ROW_COUNT  # 8 px

# Divider between scope and param list.
DIVIDER_X_PARAM    = 152


# --- Footer labels (mirror PhaseFluxEditPage.cpp) --------------------------

FOOTER_TEMP        = ["Curve", "Warp",  "Resp", "Gate", "Puls"]
FOOTER_TEMP_SHIFT  = ["FlipV", "FlipH", None,   None,   "Skip"]
FOOTER_PITCH       = ["Curve", "Warp",  "Resp", "Base", "Rng"]
FOOTER_PITCH_SHIFT = ["FlipV", "FlipH", None,   None,   None]


def _footer(labels, shift_labels, shift_held: bool):
    return [
        (shift_labels[i] if shift_held and shift_labels[i] else labels[i])
        for i in range(5)
    ]


# --- Value formatters -------------------------------------------------------

def _signed(v):
    if v == 0:
        return "0"
    return f"{v:+d}"


def _fmt_warp(v):
    return _signed(int(round(v * 64)))


def _temp_rows(stage: Stage):
    return [
        ("Curve", TEMP_CURVE_NAMES[stage.temporalCurve],   "V" if False else None),  # FlipV badge stub
        ("Warp",  _fmt_warp(stage.temporalWarp),           "H" if False else None),  # FlipH badge stub
        ("Resp",  _fmt_warp(stage.temporalResponse),       None),
        ("Gate",  f"{stage.gateLength}",                    None),
        ("Puls",  f"{stage.pulseCount}",                    "S" if stage.skip else None),
    ]


def _pitch_rows(stage: Stage):
    return [
        ("Curve", PITCH_CURVE_NAMES[stage.pitchCurve], None),
        ("Warp",  _fmt_warp(stage.pitchWarp),           None),
        ("Resp",  _fmt_warp(stage.pitchResponse),       None),
        ("Base",  _signed(int(round(stage.basePitch))), None),
        ("Rng",   f"{stage.pitchRange:.1f}",            None),
    ]


# --- Drawing -----------------------------------------------------------------

def draw_header_with_stage(canvas: Canvas, stage_idx: int, function_label: str):
    """Header layout + active function center. The stage badge is drawn INSIDE
       the waveform scope (top-left corner) by `draw_stage_badge_in_scope`."""
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, function_label)


def draw_stage_badge_in_scope(canvas: Canvas, stage_idx: int, scope_x: int):
    """Tiny stage badge "ST<n>" inside the top-left corner of the waveform
       scope (always placed at the same scope, regardless of which curve
       lives there in TEMP vs PTCH page)."""
    badge = f"ST{stage_idx + 1}"
    canvas.set_font(Font.Tiny)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.MediumBright)
    canvas.draw_text(scope_x + 2, SCOPE_Y + 6, badge)


def draw_dividers(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    h = PageHeight - HeaderHeight - FooterHeight - 4
    canvas.vline(DIVIDER_X1,         HeaderHeight + 2, h)
    canvas.vline(DIVIDER_X_PARAM,    HeaderHeight + 2, h)


def draw_param_list(canvas: Canvas, rows, selected_idx: int):
    """5 rows of [name | value | shift-badge?] in the right pane."""
    canvas.set_font(Font.Tiny)
    canvas.set_blend_mode(BlendMode.Set)

    for i, (name, value, badge) in enumerate(rows):
        ry = PARAM_PANEL_Y + i * PARAM_ROW_H
        rx = PARAM_PANEL_X
        rw = PARAM_PANEL_W
        rh = PARAM_ROW_H - 1   # 1 px gap between rows

        if i == selected_idx:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(rx, ry, rw, rh)
            name_color  = Color.Bright
            value_color = Color.Bright
            badge_color = Color.Bright
        else:
            name_color  = Color.Medium
            value_color = Color.MediumBright
            badge_color = Color.Medium

        # Name flush-left, value flush-right; shift badge appended in []
        text_y = ry + rh - 2
        canvas.set_color(name_color)
        canvas.draw_text(rx + 2, text_y, name)

        right_x = rx + rw - 2
        if badge:
            bw = canvas.text_width(f"[{badge}]")
            canvas.set_color(badge_color)
            canvas.draw_text(right_x - bw, text_y, f"[{badge}]")
            right_x -= bw + 2

        vw = canvas.text_width(value)
        canvas.set_color(value_color)
        canvas.draw_text(right_x - vw, text_y, value)


# --- Page renderers ---------------------------------------------------------

def render_temporal_hero(canvas: Canvas, stages, active_idx: int,
                          selected_idx: int, edit_slot_idx: int, shift_held: bool):
    WindowPainter.clear(canvas)
    draw_header_with_stage(canvas, selected_idx, "TEMP")

    footer = _footer(FOOTER_TEMP, FOOTER_TEMP_SHIFT, shift_held)
    WindowPainter.draw_footer(canvas, names=footer, highlight=edit_slot_idx)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_dividers(canvas)
    draw_temporal_scope(canvas, stages[selected_idx])
    draw_stage_badge_in_scope(canvas, selected_idx, SCOPE_TEMP_X)
    draw_param_list(canvas, _temp_rows(stages[selected_idx]),
                    selected_idx=edit_slot_idx)


def render_pitch_hero(canvas: Canvas, stages, active_idx: int,
                       selected_idx: int, edit_slot_idx: int, shift_held: bool):
    WindowPainter.clear(canvas)
    draw_header_with_stage(canvas, selected_idx, "PTCH")

    footer = _footer(FOOTER_PITCH, FOOTER_PITCH_SHIFT, shift_held)
    WindowPainter.draw_footer(canvas, names=footer, highlight=edit_slot_idx)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_dividers(canvas)

    # MOVE pitch scope to the temporal slot (left of the divider at x=152).
    _draw_pitch_scope_at(canvas, stages[selected_idx], x=SCOPE_TEMP_X)
    draw_stage_badge_in_scope(canvas, selected_idx, SCOPE_TEMP_X)

    draw_param_list(canvas, _pitch_rows(stages[selected_idx]),
                    selected_idx=edit_slot_idx)


def _draw_pitch_scope_at(canvas: Canvas, stage: Stage, x: int):
    """Inline copy of draw_pitch_scope but with the X position passed in."""
    import pages_phaseflux as pf

    y = SCOPE_Y
    w = SCOPE_W
    h = SCOPE_H

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, w, h)
    canvas.hline(x + 1, y + h - 2, w - 2)

    canvas.set_color(Color.Bright)
    prev_y = None
    for px in range(w - 2):
        phi = px / float(w - 3)
        p_final = pf.eval_pitch(stage, phi)
        py = y + h - 2 - int(p_final * (h - 3))
        if prev_y is not None:
            canvas.line(x + 1 + px - 1, prev_y, x + 1 + px, py)
        prev_y = py

    pulses = max(1, min(8, stage.pulseCount))
    for i in range(pulses):
        t_raw = 0.0 if pulses == 1 else i / float(pulses - 1)
        t_final = pf.eval_temp(stage, t_raw)
        t_final = (t_final + stage.phaseShift / 8.0) % 1.0
        masked = (pf.MASK_PATTERNS[stage.mask] >> ((i + stage.maskShift) % 8)) & 1
        if masked:
            continue
        p_final = pf.eval_pitch(stage, t_final)
        px = x + 1 + int(t_final * (w - 3))
        py = y + h - 2 - int(p_final * (h - 3))
        canvas.set_color(Color.Bright)
        canvas.fill_rect(px - 1, py - 1, 2, 2)


# --- Demo stage to make the screen visually rich ---------------------------

def hero_demo_stage():
    """A stage with most knobs non-default so the preview is informative."""
    s = Stage(
        pulseCount=4,
        stageDivisor=2,          # 1/8
        temporalCurve=3,         # Bell
        temporalWarp=0.40,
        temporalResponse=-0.25,
        pitchCurve=2,            # Arc
        pitchWarp=0.30,
        pitchResponse=0.0,
        basePitch=4.0,
        pitchRange=1.5,
        gateLength=60,
        phaseShift=1,
        mask=3,                  # 1in4
        skip=False,
    )
    return s


def render_both_heros(output_dir: str, scale: int = 4):
    stages = demo_sequence()
    stages[5] = hero_demo_stage()  # cell 5 — appears in row 1 of grid

    selected = 5
    active = 5

    out = []
    for label, fn in (("phaseflux-hero-temporal.png", render_temporal_hero),
                      ("phaseflux-hero-pitch.png",    render_pitch_hero)):
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        fn(c, stages, active_idx=active, selected_idx=selected,
           edit_slot_idx=1, shift_held=False)
        img = framebuffer_to_image(fb, scale=scale)
        path = os.path.join(output_dir, label)
        img.save(path)
        out.append(path)
    return out


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--out-dir",
                        default=os.path.join(os.path.dirname(__file__), "phaseflux"))
    parser.add_argument("--scale", type=int, default=4)
    args = parser.parse_args()
    paths = render_both_heros(args.out_dir, scale=args.scale)
    for p in paths:
        print(f"wrote {p}")
