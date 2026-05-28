"""
PhaseFlux TEMP scope — pulse-mark density renders at counts 0/8/16 across
Repeat × Window combinations. Mirrors the sub-section + Window math the
engine and scope already use (PhaseFluxTrackEngine.cpp lines 309..354 and
PhaseFluxEditPage.cpp lines 633..699).
"""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, WindowPainter

from pages_phaseflux import (
    Stage, demo_sequence,
    SCOPE_TEMP_X, SCOPE_W, SCOPE_H, SCOPE_Y,
    eval_temp, draw_grid,
    TEMP_CURVE_LINEAR, MASK_PATTERNS,
)


WINDOW_OFF        = 0
WINDOW_FOCUS70    = 1
WINDOW_FOCUS50    = 2
WINDOW_POLARIZE70 = 3
WINDOW_POLARIZE50 = 4

WINDOW_NAMES = ["Off", "F70", "F50", "P70", "P50"]


def repeat_multiplier(repeat: int) -> int:
    return [1, 2, 3, 5][repeat]


def is_in_window(phi: float, window: int) -> bool:
    if window == WINDOW_OFF:        return True
    if window == WINDOW_FOCUS70:    return 0.15 <= phi <= 0.85
    if window == WINDOW_FOCUS50:    return 0.25 <= phi <= 0.75
    if window == WINDOW_POLARIZE70: return phi <= 0.35 or phi >= 0.65
    if window == WINDOW_POLARIZE50: return phi <= 0.25 or phi >= 0.75
    return True


def draw_temporal_scope_density(canvas: Canvas, stage: Stage, *,
                                 temporal_repeat: int = 0,
                                 temporal_window: int = WINDOW_OFF,
                                 pulse_acc_offset: int = 0):
    """Reproduces drawTemporalScope from PhaseFluxEditPage.cpp."""
    x = SCOPE_TEMP_X
    y = SCOPE_Y
    w = SCOPE_W
    h = SCOPE_H

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, w, h)
    canvas.hline(x + 1, y + h - 2, w - 2)

    if stage.skip:
        return

    temp_rep = repeat_multiplier(temporal_repeat)

    # Curve trace — Window/Repeat aware.
    canvas.set_color(Color.Bright)
    prev_py = -1
    prev_visible = False
    for px in range(w - 2):
        t_raw = px / float(w - 3)
        t_local = (t_raw * temp_rep) % 1.0
        if not is_in_window(t_local, temporal_window):
            prev_visible = False
            continue
        t_final = eval_temp(stage, t_local)
        py = y + h - 2 - int(t_final * (h - 3))
        cx = x + px
        if prev_visible:
            canvas.line(cx - 1, prev_py, cx, py)
        prev_py = py
        prev_visible = True

    # Pulse marks — 3×3 hollow ring per base, 3×2 U per accum-extra.
    mask_byte = MASK_PATTERNS[stage.mask]
    mark_center_y = y + h // 2
    inner_span = w - 5

    base_pulses = max(0, min(16, stage.pulseCount))
    effective   = max(0, min(16, stage.pulseCount + pulse_acc_offset))
    bases_drawn = min(base_pulses, effective)

    sub_base_size = effective // temp_rep if temp_rep else 0
    sub_remainder = effective %  temp_rep if temp_rep else 0
    sub_oversize_boundary = sub_remainder * (sub_base_size + 1)

    for i in range(effective):
        if i < sub_oversize_boundary:
            sub_idx = i // (sub_base_size + 1)
            local_pulse_idx = i % (sub_base_size + 1)
            pulses_in_sub = sub_base_size + 1
        else:
            j = i - sub_oversize_boundary
            sub_idx = sub_remainder + (j // sub_base_size if sub_base_size > 0 else 0)
            local_pulse_idx = (j % sub_base_size) if sub_base_size > 0 else 0
            pulses_in_sub = sub_base_size if sub_base_size > 0 else 1

        t_raw_local = float(local_pulse_idx) / float(pulses_in_sub)
        if not is_in_window(t_raw_local, temporal_window):
            continue
        t_final_local = eval_temp(stage, t_raw_local)
        t_final_global = (sub_idx + t_final_local) / float(temp_rep)
        t_shifted = (t_final_global + stage.phaseShift * 0.125) % 1.0
        muted = (mask_byte >> ((i + stage.maskShift) & 7)) & 1
        px_x = x + 2 + int(t_shifted * inner_span)

        if muted:
            canvas.set_color(Color.Low)
            canvas.point(px_x, mark_center_y)
            continue
        canvas.set_color(Color.Medium)
        if i < bases_drawn:
            canvas.draw_rect(px_x - 1, mark_center_y - 1, 3, 3)
        else:
            canvas.point(px_x - 1, mark_center_y)
            canvas.point(px_x + 1, mark_center_y)
            canvas.hline(px_x - 1, mark_center_y + 1, 3)


def render_scope_variant(canvas: Canvas, stages, selected_idx, *,
                          temporal_repeat=0, temporal_window=WINDOW_OFF,
                          pulse_count=8, label_text=""):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, "TEMP")
    WindowPainter.draw_footer(canvas,
                              names=["Curve", "Warp", "Resp", "Puls", "Next"],
                              highlight=-1)

    draw_grid(canvas, stages, active_idx=selected_idx, selected_idx=selected_idx)

    s = stages[selected_idx]
    s.pulseCount = pulse_count
    s.temporalCurve = TEMP_CURVE_LINEAR
    s.temporalWarp = 0.0
    s.temporalResponse = 0.0
    s.mask = 0
    s.maskShift = 0
    s.phaseShift = 0
    s.skip = False
    draw_temporal_scope_density(canvas, s,
                                 temporal_repeat=temporal_repeat,
                                 temporal_window=temporal_window)

    # Tiny header strip overlay above the footer separator to label the variant.
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(SCOPE_TEMP_X, SCOPE_Y - 2, label_text)


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "tempo-scope")
    os.makedirs(out_dir, exist_ok=True)

    variants = [
        # name, pulses, repeat (idx), window (idx), label
        ("tempo-scope-0-silent.png",         0, 0, WINDOW_OFF,        "0/x1/Off"),
        ("tempo-scope-8-x1-off.png",         8, 0, WINDOW_OFF,        "8/x1/Off"),
        ("tempo-scope-16-x1-off.png",       16, 0, WINDOW_OFF,       "16/x1/Off"),
        ("tempo-scope-16-x2-off.png",       16, 1, WINDOW_OFF,       "16/x2/Off"),
        ("tempo-scope-16-x3-off.png",       16, 2, WINDOW_OFF,       "16/x3/Off"),
        ("tempo-scope-16-x5-off.png",       16, 3, WINDOW_OFF,       "16/x5/Off"),
        ("tempo-scope-16-x1-focus50.png",   16, 0, WINDOW_FOCUS50,   "16/x1/F50"),
        ("tempo-scope-16-x1-polarize50.png",16, 0, WINDOW_POLARIZE50,"16/x1/P50"),
    ]
    for name, pulses, repeat, window, label in variants:
        stages = demo_sequence()
        selected = 8
        # Reset the selected stage to a clean linear shape so density reads.
        stages[selected] = Stage(pulseCount=pulses,
                                  temporalCurve=TEMP_CURVE_LINEAR,
                                  mask=0)
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        render_scope_variant(c, stages, selected,
                             temporal_repeat=repeat,
                             temporal_window=window,
                             pulse_count=pulses,
                             label_text=label)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, name)
        img.save(path)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
