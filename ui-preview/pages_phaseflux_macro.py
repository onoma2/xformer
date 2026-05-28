"""
PhaseFlux MACRO topic mockup — full-width scope showing all 16 stages'
temporal curves stitched along X, slice widths proportional to effective
tick budgets, live response to per-sequence nudges (WarpN/RespN/PulsN)
and GWarp (cyclePhaseWarp).

Footer P0: WarpN / RespN / PulsN / GWarp / Next
Footer P1: Phase / -- / -- / -- / Next

Grid dropped — overview already visualizes all 16 stages. Header + footer
preserved.
"""

import os
import sys
import math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter

from pages_phaseflux import (
    Stage, demo_sequence,
    eval_temp,
    TEMP_CURVE_LINEAR, TEMP_CURVE_BELL, TEMP_CURVE_BOUNCE,
)


SNAKE_ORDER = [0, 1, 2, 3,  7, 6, 5, 4,  8, 9, 10, 11,  15, 14, 13, 12]

# stageDivisorTicks at PPQN-192 (matches PhaseFluxMath::kStageDivisorTicks)
STAGE_DIVISOR_TICKS = [24, 32, 48, 64, 96, 128, 192, 384]


def power_bend(x: float, warp: float) -> float:
    """Mirror of model/Curve.cpp::applyPowerBend.  warp ∈ [-64, +64] → p ∈ [-1, +1] via /64."""
    if abs(warp) < 1e-6:
        return x
    p = max(-0.984, min(0.984, warp / 64.0))
    # exponent = (1 - p) / (1 + p)
    if p >= 0:
        exp = (1.0 - p) / (1.0 + p)
    else:
        exp = (1.0 - p) / (1.0 + p)
    x = max(0.0, min(1.0, x))
    return x ** exp


def compute_cumulative(stages, sequence_divisor=12, clock_mult=100, len_nudge=0):
    """Mirror of PhaseFluxMath::computeCumulativeTicks (without measure clamp).
    LenNudge adds uniformly to every stage's stageLen, clamped to 0..127."""
    cumulative = [0] * 17
    for slot in range(16):
        cell = SNAKE_ORDER[slot]
        s = stages[cell]
        raw = 0 if s.skip else STAGE_DIVISOR_TICKS[s.stageDivisor]
        scaled_by_seq = (raw * sequence_divisor) // 12
        slen = getattr(s, "stageLen", 64)
        eff_slen = max(0, min(127, slen + len_nudge))
        scaled_by_len = (scaled_by_seq * eff_slen) // 64
        scaled = (scaled_by_len * 100) // clock_mult
        cumulative[slot + 1] = cumulative[slot] + scaled
    return cumulative


def eval_temp_with_nudge(stage, t_raw, warp_nudge=0.0, resp_nudge=0.0):
    """Apply temporal pipeline with per-sequence nudges added to per-stage warp/resp."""
    eff_warp = max(-64.0, min(64.0, stage.temporalWarp + warp_nudge))
    eff_resp = max(-64.0, min(64.0, stage.temporalResponse + resp_nudge))
    # Replicate eval_temp pipeline with effective values (Window/Repeat ignored for mockup).
    from pages_phaseflux import temp_curve_lut
    t_warped = power_bend(t_raw, eff_warp)
    t_curved = temp_curve_lut(stage.temporalCurve, t_warped)
    t_final = power_bend(t_curved, eff_resp)
    return t_final


def draw_macro_scope(canvas, stages, *,
                     warp_nudge=0, resp_nudge=0, pulse_nudge=0,
                     len_nudge=0,
                     gwarp=0, phase=0.0, playhead_pos=None):
    """Full-width scope spanning x=2..253, y=12..50.

    GWarp: PowerBend on the cycle phase. Phase: cycle start offset 0..1.
    Slice widths reflect effective tick budgets. Curves use per-stage shape
    with WarpN/RespN added live.
    """
    scope_x = 2
    scope_y = 12
    scope_w = PageWidth - 4   # 252
    scope_h = 38

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    canvas.draw_rect(scope_x, scope_y, scope_w, scope_h)
    canvas.hline(scope_x + 1, scope_y + scope_h - 2, scope_w - 2)

    cumulative = compute_cumulative(stages, len_nudge=len_nudge)
    cycle_ticks = cumulative[16]
    if cycle_ticks == 0:
        return

    inner_x0 = scope_x + 1
    inner_w  = scope_w - 2
    inner_y0 = scope_y + 1
    inner_h  = scope_h - 3

    # Faint vertical ticks at each slice boundary (after GWarp).
    canvas.set_color(Color.Low)
    for slot in range(1, 16):
        # Slot start in unwarped cycle phase, then apply inverse-of-warp
        # so the boundary appears where the warped phase lands on that slot.
        raw_phase = cumulative[slot] / float(cycle_ticks)
        warped = power_bend(raw_phase, gwarp)
        x = inner_x0 + int(warped * (inner_w - 1))
        # Dotted column (every other pixel)
        for y in range(inner_y0, inner_y0 + inner_h, 2):
            canvas.point(x, y)

    # Curve trace — one px per X column.
    canvas.set_color(Color.Bright)
    prev_y = None
    prev_visible = False
    for px in range(inner_w):
        # Map px → phase in [0,1], then unwarp through GWarp to recover
        # which raw cycle position the engine reads. Positive GWarp →
        # early stages stretched on display.
        display_phase = px / float(inner_w - 1)
        # Inverse PowerBend: warped→raw. PowerBend with p uses exponent (1-p)/(1+p).
        # Inverse is exponent (1+p)/(1-p) i.e. negate warp.
        raw_phase = power_bend(display_phase, -gwarp)
        # Add cycle-phase offset (Phase knob, fraction [0,1]).
        raw_phase = (raw_phase + phase) % 1.0
        ticks = raw_phase * cycle_ticks

        # Find slot by linear scan.
        slot = 0
        while slot < 15 and ticks >= cumulative[slot + 1]:
            slot += 1
        slot_w = cumulative[slot + 1] - cumulative[slot]
        if slot_w <= 0:
            prev_visible = False
            continue
        t_raw = (ticks - cumulative[slot]) / float(slot_w)
        cell = SNAKE_ORDER[slot]
        s = stages[cell]
        if s.skip:
            prev_visible = False
            continue

        t_final = eval_temp_with_nudge(s, t_raw, warp_nudge, resp_nudge)
        py = inner_y0 + inner_h - 1 - int(t_final * (inner_h - 1))
        cx = inner_x0 + px
        if prev_visible:
            canvas.line(cx - 1, prev_y, cx, py)
        prev_y = py
        prev_visible = True

    # Pulse marks — small 1×1 dot per fire on each slice, centered in scope Y.
    # PulsN adds to every stage's effective pulseCount.
    mark_y = inner_y0 + inner_h // 2
    canvas.set_color(Color.Medium)
    for slot in range(16):
        cell = SNAKE_ORDER[slot]
        s = stages[cell]
        if s.skip:
            continue
        slot_w_ticks = cumulative[slot + 1] - cumulative[slot]
        if slot_w_ticks <= 0:
            continue
        eff_pulse = max(0, min(16, s.pulseCount + pulse_nudge))
        for i in range(eff_pulse):
            t_local = i / float(eff_pulse) if eff_pulse > 0 else 0.0
            t_final = eval_temp_with_nudge(s, t_local, warp_nudge, resp_nudge)
            # Slot starts at cumulative[slot] in raw cycle, fires at (slot + t_final/N) really;
            # for mark position use t_final within the slot's display X span.
            raw_phase = (cumulative[slot] + t_final * slot_w_ticks) / float(cycle_ticks)
            # Forward warp to display position
            disp = power_bend(raw_phase, gwarp)
            # Add phase offset for display
            disp = (disp - phase) % 1.0
            x = inner_x0 + int(disp * (inner_w - 1))
            canvas.point(x, mark_y)

    # Playhead — 1 px vertical line at current cycle position.
    if playhead_pos is not None and 0.0 <= playhead_pos <= 1.0:
        x = inner_x0 + int(playhead_pos * (inner_w - 1))
        canvas.set_color(Color.Bright)
        canvas.vline(x, inner_y0, inner_h)


def draw_macro_footer(canvas, *, page, selected_slot):
    """5-slot footer.
    P0 = uniform per-cell additions (nudges).
    P1 = cycle-level shape (Phase, GWarp) + press-to-fire utilities (Snap, Zero).
    """
    if page == 0:
        labels = ["WarpN", "RespN", "PulsN", "LenN", "Next"]
    else:
        labels = ["Phase", "GWarp", "Snap", "Zero", "Next"]
    WindowPainter.draw_footer(canvas, names=labels, highlight=selected_slot)


def render_macro(canvas, stages, *,
                  warp_nudge=0, resp_nudge=0, pulse_nudge=0,
                  len_nudge=0,
                  gwarp=0, phase=0.0,
                  playhead_pos=None,
                  footer_page=0, selected_slot=-1):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                               mode="PHFLX")
    WindowPainter.draw_active_function(canvas, "MACRO")

    draw_macro_scope(canvas, stages,
                      warp_nudge=warp_nudge,
                      resp_nudge=resp_nudge,
                      pulse_nudge=pulse_nudge,
                      len_nudge=len_nudge,
                      gwarp=gwarp,
                      phase=phase,
                      playhead_pos=playhead_pos)

    draw_macro_footer(canvas, page=footer_page, selected_slot=selected_slot)


def make_demo_stages():
    """16 stages — varied curves, no skips, default stageDivisor, default pulseCount=4."""
    stages = [Stage() for _ in range(16)]
    curves = [
        TEMP_CURVE_LINEAR, TEMP_CURVE_BELL,    TEMP_CURVE_LINEAR, TEMP_CURVE_BOUNCE,
        TEMP_CURVE_BELL,   TEMP_CURVE_LINEAR,  TEMP_CURVE_BELL,   TEMP_CURVE_BOUNCE,
        TEMP_CURVE_LINEAR, TEMP_CURVE_BOUNCE,  TEMP_CURVE_BELL,   TEMP_CURVE_LINEAR,
        TEMP_CURVE_BOUNCE, TEMP_CURVE_BELL,    TEMP_CURVE_LINEAR, TEMP_CURVE_LINEAR,
    ]
    for i in range(16):
        stages[i] = Stage(pulseCount=4, temporalCurve=curves[i], stageDivisor=2)
        stages[i].stageLen = 64  # transparent default
    return stages


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "macro")
    os.makedirs(out_dir, exist_ok=True)

    variants = [
        # name, warp_nudge, resp_nudge, pulse_nudge, len_nudge, gwarp, phase, playhead, skip_set, page
        ("macro-baseline.png",       0,   0, 0,  0,   0, 0.0,  0.30, None, 0),
        ("macro-warpn-pos30.png",   30,   0, 0,  0,   0, 0.0,  0.30, None, 0),
        ("macro-respn-pos30.png",    0,  30, 0,  0,   0, 0.0,  0.30, None, 0),
        ("macro-pulsn-pos2.png",     0,   0, 2,  0,   0, 0.0,  0.30, None, 0),
        ("macro-lenn-pos30.png",     0,   0, 0, 30,   0, 0.0,  0.30, None, 0),
        ("macro-lenn-neg30.png",     0,   0, 0,-30,   0, 0.0,  0.30, None, 0),
        ("macro-gwarp-pos40.png",    0,   0, 0,  0,  40, 0.0,  0.30, None, 1),
        ("macro-phase-quarter.png",  0,   0, 0,  0,   0, 0.25, 0.50, None, 1),
        ("macro-combo.png",         20,  10, 1, 10,  25, 0.0,  0.30, None, 0),
        ("macro-skips-sparse.png",   0,   0, 0,  0,   0, 0.0,  0.50, {2, 5, 9, 12}, 0),
        ("macro-skips-two-active.png", 0, 0, 0,  0,   0, 0.0,  0.50, set(range(16)) - {0, 8}, 0),
    ]
    for name, wn, rn, pn, ln, gw, ph, playhead, skip_set, page in variants:
        stages = make_demo_stages()
        if skip_set:
            for i in skip_set:
                stages[i].skip = True
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        render_macro(c, stages,
                      warp_nudge=wn,
                      resp_nudge=rn,
                      pulse_nudge=pn,
                      len_nudge=ln,
                      gwarp=gw,
                      phase=ph,
                      playhead_pos=playhead,
                      footer_page=page,
                      selected_slot=-1)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, name)
        img.save(path)
        print(f"wrote {path}")

    # P1 with each slot selected for highlight (Snap and Zero are press-to-fire).
    for sel, name in [(0, "macro-p1-phase-selected.png"),
                      (1, "macro-p1-gwarp-selected.png"),
                      (2, "macro-p1-snap-selected.png"),
                      (3, "macro-p1-zero-selected.png")]:
        stages = make_demo_stages()
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        render_macro(c, stages,
                      phase=0.2 if sel == 0 else 0.0,
                      gwarp=20 if sel == 1 else 0,
                      playhead_pos=0.30,
                      footer_page=1,
                      selected_slot=sel)
        framebuffer_to_image(fb, scale=4).save(os.path.join(out_dir, name))
        print(f"wrote {os.path.join(out_dir, name)}")


if __name__ == "__main__":
    main()
