"""
PhaseFlux PITCH scope redesign — faint curve trace + reachable-degree note
labels at the left edge. Dim by default, bright for the currently-playing
degree (only when the selected cell is the active/playing cell). Pulse-fire
dots removed.

Reachable degrees: sample the pitch pipeline at high resolution, quantize
each sample to an integer scale-degree offset via int(round(p_final *
rangeDegrees)) (or the bipolar variant), collect the unique set.

Labels: tiny5x5 noteName (Format::Long → "C2", "F#3"). Currently playing =
scale.noteFromVolts(cvOutput) inverse — for the preview we just take the
current pulse's quantized degree.
"""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, WindowPainter

from pages_phaseflux import (
    Stage, demo_sequence,
    SCOPE_TEMP_X, SCOPE_W, SCOPE_H, SCOPE_Y,
    eval_pitch, eval_temp, draw_grid,
    PITCH_CURVE_RAMP, PITCH_CURVE_ARC, PITCH_CURVE_MIRROR, PITCH_CURVE_FLAT,
    MASK_PATTERNS,
)


# Chromatic note naming — matches Performer's Scale::noteName(Format::Long).
_CHROMATIC = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

def note_name(absolute_semitone: int) -> str:
    octave = absolute_semitone // 12
    pc = absolute_semitone % 12
    return f"{_CHROMATIC[pc]}{octave}"


def degree_histogram(stage: Stage, range_degrees: int, direction: str,
                     samples: int = 96):
    """Sample pitch pipeline; return dict {degree_offset: count}."""
    counts = {}
    for i in range(samples):
        phi = i / float(samples)
        p_final = eval_pitch(stage, phi)
        if direction == "Up":
            off = p_final * range_degrees
        elif direction == "Down":
            off = -p_final * range_degrees
        else:  # Bipolar
            off = (p_final - 0.5) * range_degrees
        deg = int(round(off))
        counts[deg] = counts.get(deg, 0) + 1
    return counts


def current_degree(stage: Stage, stage_phase: float, range_degrees: int,
                   direction: str) -> int:
    """Quantized offset at the given phase — proxy for engine's last-fired
    degree (or for held CV after the last pulse)."""
    p_final = eval_pitch(stage, stage_phase)
    if direction == "Up":
        off = p_final * range_degrees
    elif direction == "Down":
        off = -p_final * range_degrees
    else:
        off = (p_final - 0.5) * range_degrees
    return int(round(off))


LABEL_COL_W = 22   # tiny5x5 "F#4" ≈ 14 px + breathing room each side.

def draw_pitch_scope_redesign(canvas: Canvas, stage: Stage, *,
                               base_semitone: int,
                               range_degrees: int,
                               direction: str = "Up",
                               selected_is_active: bool,
                               stage_phase: float = 0.0):
    # PTCH topic owns the same scope slot the TEMP topic uses (x=50..149).
    x = SCOPE_TEMP_X
    y = SCOPE_Y
    w = SCOPE_W
    h = SCOPE_H

    canvas.set_blend_mode(BlendMode.Set)

    # Scope outline.
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, w, h)

    # Trace area sits to the right of the label column.
    trace_x0 = x + LABEL_COL_W + 1
    trace_w  = w - LABEL_COL_W - 2

    # Vertical divider between label column and trace.
    canvas.set_color(Color.Low)
    canvas.vline(x + LABEL_COL_W, y + 1, h - 2)

    # Curve trace — medium so the shape actually reads. Now lives only in
    # the trace zone; no overlap with the label column.
    canvas.set_color(Color.Medium)
    prev_y = None
    for px in range(trace_w):
        phi = px / float(max(1, trace_w - 1))
        p_final = eval_pitch(stage, phi)
        py = y + h - 2 - int(p_final * (h - 3))
        cx = trace_x0 + px
        if prev_y is not None:
            canvas.line(cx - 1, prev_y, cx, py)
        prev_y = py

    # Pulse-fire dots on the curve — 2x2 squares at each unmuted pulse's
    # (t, pitch). Dim by default, bright as playhead reaches each one in
    # the active cell. Position uses t = i/N (engine §6.1).
    pulses = max(1, min(8, stage.pulseCount))
    mask_byte = MASK_PATTERNS[stage.mask]
    for i in range(pulses):
        t_raw = i / float(pulses)
        t_final = eval_temp(stage, t_raw)
        t_shifted = (t_final + stage.phaseShift / 8.0) % 1.0
        masked = (mask_byte >> ((i + stage.maskShift) % 8)) & 1
        if masked:
            continue
        p_final = eval_pitch(stage, t_shifted)
        dot_x = trace_x0 + int(t_shifted * (trace_w - 1))
        dot_y = y + h - 2 - int(p_final * (h - 3))
        passed = selected_is_active and stage_phase >= t_shifted
        canvas.set_color(Color.Bright if passed else Color.Medium)
        canvas.fill_rect(dot_x - 1, dot_y - 1, 2, 2)

    # Pool = the cell's SPAN: lowest, dwell-peak (home), highest reachable
    # degree, each drawn pitch-positioned at its curve-output height.
    counts = degree_histogram(stage, range_degrees, direction)
    if not counts:
        return
    lo = min(counts)
    hi = max(counts)
    dw = max(counts.items(), key=lambda kv: kv[1])[0]   # dwell-peak offset

    inner_y0 = y + 2
    label_right = x + LABEL_COL_W - 2
    canvas.set_font(Font.Tiny)

    def y_for_offset(off):
        if direction == "Down":
            p = -off / float(range_degrees)
        elif direction == "Bipolar":
            p = off / float(range_degrees) + 0.5
        else:
            p = off / float(range_degrees)
        p = max(0.0, min(1.0, p))
        return y + h - 2 - int(p * (h - 3))

    def clamp_baseline(b):
        return max(inner_y0 + 5, min(y + h - 2, b))

    def draw_note_at(off, baseline, highlight):
        note_str = note_name(base_semitone + off)
        tw = canvas.text_width(note_str)
        tx = label_right - tw
        if highlight:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(tx - 1, baseline - 5, tw + 1, 7)
            canvas.set_blend_mode(BlendMode.Sub)
            canvas.draw_text(tx, baseline, note_str)
            canvas.set_blend_mode(BlendMode.Set)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_text(tx, baseline, note_str)

    # Collect labels (span lo/dw/hi + live current note), dedup by degree,
    # then spread vertically with a 7px gap so narrow spans / the moving
    # current note never stack names on top of each other.
    labels = []  # [offset, baseline, highlight]

    def add_label(off, highlight):
        for L in labels:
            if L[0] == off:
                L[2] = L[2] or highlight
                return
        labels.append([off, y_for_offset(off) + 2, highlight])

    add_label(dw, False)
    if hi != dw:
        add_label(hi, False)
    if lo != dw:
        add_label(lo, False)
    if selected_is_active:
        cur = current_degree(stage, stage_phase, range_degrees, direction)
        if cur in counts:
            add_label(cur, True)

    labels.sort(key=lambda L: L[1])
    LABEL_GAP = 7
    bl_bot = y + h - 2
    for i, L in enumerate(labels):
        L[1] = clamp_baseline(L[1])
        if i > 0 and L[1] < labels[i - 1][1] + LABEL_GAP:
            L[1] = labels[i - 1][1] + LABEL_GAP
    if labels and labels[-1][1] > bl_bot:
        shift = labels[-1][1] - bl_bot
        for L in labels:
            L[1] -= shift

    for off, baseline, highlight in labels:
        draw_note_at(off, baseline, highlight)


def render_pitch_scope_redesign(canvas: Canvas, stages, *,
                                 selected_idx: int,
                                 active_idx: int,
                                 base_semitone: int,
                                 range_degrees: int,
                                 direction: str,
                                 stage_phase: float):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, "PTCH")
    WindowPainter.draw_footer(canvas,
                              names=["Curve", "Warp", "Resp", "Base", "Span"],
                              highlight=-1)

    # Keep the 4x4 grid on the left; the PTCH topic only renders the pitch scope.
    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    stage = stages[selected_idx]

    draw_pitch_scope_redesign(canvas, stage,
                              base_semitone=base_semitone,
                              range_degrees=range_degrees,
                              direction=direction,
                              selected_is_active=(selected_idx == active_idx),
                              stage_phase=stage_phase)


def _custom_sequence(selected_idx: int, curve: int, pulse_count: int = 8,
                     warp: float = 0.0, response: float = 0.0):
    """Demo sequence — replace the selected stage with a fresh one carrying
    the requested pitch shape so the scope has something to chew on."""
    stages = demo_sequence()
    s = Stage(pulseCount=pulse_count,
              pitchCurve=curve,
              pitchWarp=warp,
              pitchResponse=response)
    stages[selected_idx] = s
    return stages


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "pitch-scope")
    os.makedirs(out_dir, exist_ok=True)

    variants = [
        # name, curve, pulses, warp, response, base, range_degrees, direction, phase, sel_is_active
        ("pitch-scope-ramp-up-octave.png",        PITCH_CURVE_RAMP,   8,  0.0,  0.0, 36, 12, "Up",      0.40, True),
        ("pitch-scope-arc-bipolar-octave.png",    PITCH_CURVE_ARC,    8,  0.0,  0.0, 48, 12, "Bipolar", 0.50, True),
        ("pitch-scope-mirror-up-narrow.png",      PITCH_CURVE_MIRROR, 6,  0.0,  0.0, 60,  4, "Up",      0.25, True),
        ("pitch-scope-ramp-flat-via-response.png",PITCH_CURVE_RAMP,   8,  0.0, -0.99,36, 12, "Up",      0.30, True),
        ("pitch-scope-ramp-up-wide-not-active.png",PITCH_CURVE_RAMP,  8,  0.0,  0.0, 36, 24, "Up",      0.00, False),
    ]
    for entry in variants:
        name, curve, pulses, warp, resp, base, rng, direction, phase, sel_active = entry
        selected = 8
        active = 8 if sel_active else 14
        stages = _custom_sequence(selected, curve, pulse_count=pulses,
                                   warp=warp, response=resp)
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        render_pitch_scope_redesign(c, stages,
                                    selected_idx=selected,
                                    active_idx=active,
                                    base_semitone=base,
                                    range_degrees=rng,
                                    direction=direction,
                                    stage_phase=phase)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, name)
        img.save(path)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
