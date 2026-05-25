"""
PhaseFlux page preview — proposed MVP layout.

Two frames rendered stacked:
  - Default view: 4x4 grid on left, two stacked curve scopes on right (temporal +
    pitch). No parameter names visible.
  - Edit-hold view: a top-row stage button is held — bottom row becomes an 8-cell
    parameter strip with names + values for that stage. Encoder turns edit the
    highlighted param.

Header/footer guards come from painters.WindowPainter (HeaderHeight=10,
FooterHeight=10 -> safe content area y=11..52, height 42).
"""

import sys
import os
import math

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import (
    BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image,
)
from painters import (
    PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter,
)


# --------------------------------------------------------------------------
# Mock model — minimal subset of PhaseFlux fields used by the preview
# --------------------------------------------------------------------------

TEMP_CURVE_LINEAR = 0
TEMP_CURVE_EXP    = 1
TEMP_CURVE_LOG    = 2
TEMP_CURVE_BELL   = 3
TEMP_CURVE_BOUNCE = 4

PITCH_CURVE_FLAT   = 0
PITCH_CURVE_RAMP   = 1
PITCH_CURVE_ARC    = 2
PITCH_CURVE_MIRROR = 3

# 8 mask patterns from spec §6.3 (bit 0 = pulse 0, bit 7 = pulse 7)
MASK_PATTERNS = [
    0b00000000,  # Off
    0b10101010,  # 1in2
    0b01001001,  # 1in3
    0b00010001,  # 1in4
    0b10110110,  # 2in3
    0b01010101,  # 2in4
    0b01110111,  # 3in4
    0b00000001,  # 1in8
]
MASK_NAMES = ["Off", "1in2", "1in3", "1in4", "2in3", "2in4", "3in4", "1in8"]

DIVISOR_NAMES = ["1/32", "1/16", "1/8", "1/4", "1/2", "1bar"]

TEMP_CURVE_NAMES = ["Lin", "Exp", "Log", "Bel", "Bnc"]
PITCH_CURVE_NAMES = ["Flt", "Rmp", "Arc", "Mir"]


class Stage:
    def __init__(self,
                 pulseCount=1,
                 stageDivisor=1,           # index into DIVISOR_NAMES
                 temporalCurve=TEMP_CURVE_LINEAR,
                 temporalWarp=0.0,
                 temporalResponse=0.0,
                 pitchCurve=PITCH_CURVE_FLAT,
                 pitchWarp=0.0,
                 pitchResponse=0.0,
                 phaseShift=0,             # 0..7 (x 45 degrees)
                 mask=0,                   # 0..7 index into MASK_PATTERNS
                 maskShift=0,
                 basePitch=0.0,
                 pitchRange=0.0,
                 gateLength=50,
                 skip=False):
        self.pulseCount       = pulseCount
        self.stageDivisor     = stageDivisor
        self.temporalCurve    = temporalCurve
        self.temporalWarp     = temporalWarp
        self.temporalResponse = temporalResponse
        self.pitchCurve       = pitchCurve
        self.pitchWarp        = pitchWarp
        self.pitchResponse    = pitchResponse
        self.phaseShift       = phaseShift
        self.mask             = mask
        self.maskShift        = maskShift
        self.basePitch        = basePitch
        self.pitchRange       = pitchRange
        self.gateLength       = gateLength
        self.skip             = skip


def demo_sequence():
    """16 stages with a mix that exercises every transform."""
    stages = [Stage() for _ in range(16)]
    # Stage 0: default click train
    stages[0] = Stage(pulseCount=1, stageDivisor=1)
    # Stage 1: 8 pulses, exponential cluster
    stages[1] = Stage(pulseCount=8, stageDivisor=2,
                      temporalCurve=TEMP_CURVE_EXP, temporalWarp=0.4,
                      pitchCurve=PITCH_CURVE_RAMP, pitchRange=2.0)
    # Stage 2: phaseShift=2 (90 deg rotation), bell density
    stages[2] = Stage(pulseCount=6, stageDivisor=1,
                      temporalCurve=TEMP_CURVE_BELL,
                      phaseShift=2,
                      pitchCurve=PITCH_CURVE_ARC, pitchRange=1.5)
    # Stage 3: 1in2 mask, 4 pulses log
    stages[3] = Stage(pulseCount=4, stageDivisor=1,
                      temporalCurve=TEMP_CURVE_LOG, temporalWarp=-0.3,
                      mask=1,  # 1in2
                      pitchCurve=PITCH_CURVE_MIRROR, pitchRange=1.0)
    # Stage 4: skip
    stages[4] = Stage(skip=True)
    # Stage 5: long divisor, bounce
    stages[5] = Stage(pulseCount=8, stageDivisor=3,
                      temporalCurve=TEMP_CURVE_BOUNCE, temporalResponse=0.5,
                      pitchCurve=PITCH_CURVE_RAMP, pitchRange=3.0)
    # Stage 6: short, dense, mask=1in4
    stages[6] = Stage(pulseCount=8, stageDivisor=0,
                      temporalCurve=TEMP_CURVE_LINEAR,
                      mask=3, maskShift=1,
                      pitchRange=0.5)
    # Stage 7: skip
    stages[7] = Stage(skip=True)
    # Stages 8..15 stay defaults
    return stages


# --------------------------------------------------------------------------
# Curve evaluation — matches spec §6 (PowerBend axis warp + LUT shape)
# --------------------------------------------------------------------------

def power_bend(z: float, p: float) -> float:
    """PowerBend: z^((1-p)/(1+p))   for z in [0,1], p in (-1, +1)."""
    z = max(0.0, min(1.0, z))
    p = max(-0.99, min(0.99, p))
    exp = (1.0 - p) / (1.0 + p)
    return z ** exp


def temp_curve_lut(curve_id: int, x: float) -> float:
    """5 temporal shapes evaluated on x in [0,1] -> y in [0,1]."""
    if curve_id == TEMP_CURVE_LINEAR:
        return x
    if curve_id == TEMP_CURVE_EXP:
        # accelerating toward end
        return x * x
    if curve_id == TEMP_CURVE_LOG:
        # decelerating, lazy onset
        return math.sqrt(x)
    if curve_id == TEMP_CURVE_BELL:
        # peak in the middle
        return 0.5 - 0.5 * math.cos(x * math.pi)
    if curve_id == TEMP_CURVE_BOUNCE:
        # ballistic decay: density up front then quick falloff
        return 1.0 - (1.0 - x) ** 3
    return x


def pitch_curve_lut(curve_id: int, x: float) -> float:
    if curve_id == PITCH_CURVE_FLAT:
        return 0.0
    if curve_id == PITCH_CURVE_RAMP:
        return x
    if curve_id == PITCH_CURVE_ARC:
        return math.sin(x * math.pi)
    if curve_id == PITCH_CURVE_MIRROR:
        return abs(2.0 * x - 1.0)
    return 0.0


def eval_temp(stage: Stage, t_raw: float) -> float:
    t_warped = power_bend(t_raw, stage.temporalWarp)
    t_curved = temp_curve_lut(stage.temporalCurve, t_warped)
    t_final  = power_bend(t_curved, stage.temporalResponse)
    return t_final


def eval_pitch(stage: Stage, phi: float) -> float:
    phi_warped = power_bend(phi, stage.pitchWarp)
    p_curved   = pitch_curve_lut(stage.pitchCurve, phi_warped)
    p_final    = power_bend(max(0.0, min(1.0, p_curved)), stage.pitchResponse)
    return p_final


# --------------------------------------------------------------------------
# Layout constants
# --------------------------------------------------------------------------

# Left grid: 4x4 square-ish cells, x = 2..41
GRID_X      = 2
GRID_Y      = 12
GRID_CELL_W = 9
GRID_CELL_H = 9
GRID_GAP    = 1

# Vertical divider at x=46
DIVIDER_X1 = 46

# Right pane has TWO side-by-side scopes
SCOPE_W = 100
SCOPE_H = 38
SCOPE_Y = 12

SCOPE_TEMP_X = 50
SCOPE_PITCH_X = 154

DIVIDER_X2 = 152  # between the two scopes

# Edit view: 8 param boxes spanning x=50..253
PARAM_BOX_W = 24
PARAM_BOX_H = 38
PARAM_BOX_Y = 12
PARAM_BOX_X = 50
PARAM_GAP   = 1


# --------------------------------------------------------------------------
# Drawing primitives
# --------------------------------------------------------------------------

# Snake walk order: row 0 L->R, row 1 R->L, row 2 L->R, row 3 R->L
def snake_order():
    order = []
    for r in range(4):
        cols = range(4) if r % 2 == 0 else range(3, -1, -1)
        for c in cols:
            order.append(r * 4 + c)
    return order


def draw_grid_cell(canvas: Canvas, idx: int, stage: Stage,
                   is_active: bool, is_selected: bool):
    """idx is the raw cell number 0..15 — row = idx // 4, col = idx % 4."""
    row = idx // 4
    col = idx % 4
    x = GRID_X + col * (GRID_CELL_W + GRID_GAP)
    y = GRID_Y + row * (GRID_CELL_H + GRID_GAP)

    # Background fill — active cell inverted, selected stays outlined, idle dim
    canvas.set_blend_mode(BlendMode.Set)
    if is_active:
        canvas.set_color(Color.Bright)
        canvas.fill_rect(x, y, GRID_CELL_W, GRID_CELL_H)
        text_color = Color.None_
    elif stage.skip:
        canvas.set_color(Color.Low)
        canvas.draw_rect(x, y, GRID_CELL_W, GRID_CELL_H)
        # Hollow X
        canvas.line(x + 2, y + 2, x + GRID_CELL_W - 3, y + GRID_CELL_H - 3)
        canvas.line(x + GRID_CELL_W - 3, y + 2, x + 2, y + GRID_CELL_H - 3)
        text_color = None
    else:
        canvas.set_color(Color.MediumLow)
        canvas.draw_rect(x, y, GRID_CELL_W, GRID_CELL_H)
        text_color = Color.Medium

    if is_selected and not is_active:
        canvas.set_color(Color.Bright)
        canvas.draw_rect(x, y, GRID_CELL_W, GRID_CELL_H)
        text_color = Color.Bright

    if stage.skip:
        return

    # 9x9 square cells — no room for both number AND dots. Use a single visual:
    # cell brightness = pulseCount density (more pulses = brighter fill bar).
    pulses = max(1, min(8, stage.pulseCount))

    # Tiny horizontal bar inside the cell showing pulseCount (1..8 → 1..7 px)
    if not is_active:
        bar_color = Color.MediumBright if not is_selected else Color.Bright
        canvas.set_color(bar_color)
        bar_w = pulses  # 1..8 px
        canvas.hline(x + 1, y + GRID_CELL_H - 2, min(GRID_CELL_W - 2, bar_w))

        # Transform indicators: top-left dot = phaseShift, top-right dot = mask
        if stage.phaseShift != 0:
            canvas.set_color(bar_color)
            canvas.point(x + 1, y + 1)
        if stage.mask != 0:
            canvas.set_color(bar_color)
            canvas.point(x + GRID_CELL_W - 2, y + 1)
    else:
        # Active cell — invert: use None_ for the markings
        canvas.set_color(Color.None_)
        bar_w = pulses
        canvas.hline(x + 1, y + GRID_CELL_H - 2, min(GRID_CELL_W - 2, bar_w))
        if stage.phaseShift != 0:
            canvas.point(x + 1, y + 1)
        if stage.mask != 0:
            canvas.point(x + GRID_CELL_W - 2, y + 1)


def draw_grid(canvas: Canvas, stages, active_idx: int, selected_idx: int):
    for i in range(16):
        draw_grid_cell(canvas, i, stages[i],
                       is_active=(i == active_idx),
                       is_selected=(i == selected_idx))


def draw_bank_indicator(canvas: Canvas, bank: int):
    """Bank 0 = stages 1-8, Bank 1 = stages 9-16. Drawn in header gutter,
       not the content area, to stay clear of the footer separator at y=53."""
    canvas.set_font(Font.Tiny)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    label = "<1-8>" if bank == 0 else "<9-16>"
    canvas.draw_text(140, 6, label)


def draw_dividers(canvas: Canvas, edit_mode: bool = False):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    h = PageHeight - HeaderHeight - FooterHeight - 4
    canvas.vline(DIVIDER_X1, HeaderHeight + 2, h)
    if not edit_mode:
        canvas.vline(DIVIDER_X2, HeaderHeight + 2, h)


def draw_temporal_scope(canvas: Canvas, stage: Stage):
    """Left-of-two side-by-side scope: temporal curve + pulse fire ticks."""
    x = SCOPE_TEMP_X
    y = SCOPE_Y
    w = SCOPE_W
    h = SCOPE_H

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, w, h)

    # Baseline
    canvas.set_color(Color.Low)
    canvas.hline(x + 1, y + h - 2, w - 2)

    # Density curve y = t_final scaled: visualise temporal mapping as a thin line
    canvas.set_color(Color.Bright)
    prev_y = None
    for px in range(w - 2):
        t_raw = px / float(w - 3)
        t_final = eval_temp(stage, t_raw)
        py = y + h - 2 - int(t_final * (h - 3))
        if prev_y is not None:
            canvas.line(x + 1 + px - 1, prev_y, x + 1 + px, py)
        prev_y = py

    # Pulse tick marks along the bottom — actual fire times
    pulses = max(1, min(8, stage.pulseCount))
    canvas.set_color(Color.Bright)
    for i in range(pulses):
        t_raw = 0.0 if pulses == 1 else i / float(pulses - 1)
        t_final = eval_temp(stage, t_raw)
        # phaseShift in 1/8 increments
        t_final = (t_final + stage.phaseShift / 8.0) % 1.0
        # mask gating
        masked = (MASK_PATTERNS[stage.mask] >> ((i + stage.maskShift) % 8)) & 1
        px = x + 1 + int(t_final * (w - 3))
        if masked:
            canvas.set_color(Color.Low)
            canvas.point(px, y + h - 1)
        else:
            canvas.set_color(Color.Bright)
            canvas.vline(px, y + h - 4, 3)


def draw_pitch_scope(canvas: Canvas, stage: Stage):
    """Right-of-two side-by-side scope: pitch CV shape over phase."""
    x = SCOPE_PITCH_X
    y = SCOPE_Y
    w = SCOPE_W
    h = SCOPE_H

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, w, h)

    # Zero line
    canvas.set_color(Color.Low)
    canvas.hline(x + 1, y + h - 2, w - 2)

    # Pitch curve trace
    canvas.set_color(Color.Bright)
    prev_y = None
    for px in range(w - 2):
        phi = px / float(w - 3)
        p_final = eval_pitch(stage, phi)
        py = y + h - 2 - int(p_final * (h - 3))
        if prev_y is not None:
            canvas.line(x + 1 + px - 1, prev_y, x + 1 + px, py)
        prev_y = py

    # Pulse-fire dots at actual pulse times — show pitch sampled at each
    pulses = max(1, min(8, stage.pulseCount))
    for i in range(pulses):
        t_raw = 0.0 if pulses == 1 else i / float(pulses - 1)
        t_final = eval_temp(stage, t_raw)
        t_final = (t_final + stage.phaseShift / 8.0) % 1.0
        masked = (MASK_PATTERNS[stage.mask] >> ((i + stage.maskShift) % 8)) & 1
        if masked:
            continue
        phi = t_final
        p_final = eval_pitch(stage, phi)
        px = x + 1 + int(phi * (w - 3))
        py = y + h - 2 - int(p_final * (h - 3))
        canvas.set_color(Color.Bright)
        canvas.fill_rect(px - 1, py - 1, 2, 2)


# --------------------------------------------------------------------------
# Edit-mode parameter strip (replaces curve scopes when stage button held)
# --------------------------------------------------------------------------

# 8 MVP-guard params surfaced on the bottom-row hardware keys.
def _fmt_warp(v: float) -> str:
    if abs(v) < 0.005:
        return "0"
    return f"{v:+.1f}"


EDIT_PARAMS = [
    ("PUL",  lambda s: f"{s.pulseCount}"),
    ("DIV",  lambda s: DIVISOR_NAMES[s.stageDivisor]),
    ("TC",   lambda s: TEMP_CURVE_NAMES[s.temporalCurve]),
    ("TWP",  lambda s: _fmt_warp(s.temporalWarp)),
    ("PC",   lambda s: PITCH_CURVE_NAMES[s.pitchCurve]),
    ("PWP",  lambda s: _fmt_warp(s.pitchWarp)),
    ("SHF",  lambda s: f"{s.phaseShift * 45}"),
    ("MSK",  lambda s: MASK_NAMES[s.mask]),
]


def draw_param_strip(canvas: Canvas, stage: Stage, edit_idx: int):
    """8 boxes spanning the full right pane (no scope divider here).
       edit_idx is which bottom-row key is currently held / cursor focus."""
    x0 = PARAM_BOX_X
    canvas.set_font(Font.Tiny)

    box_w = PARAM_BOX_W
    gap   = PARAM_GAP

    for i, (name, value_fn) in enumerate(EDIT_PARAMS):
        bx = x0 + i * (box_w + gap)
        by = PARAM_BOX_Y
        bh = PARAM_BOX_H

        canvas.set_blend_mode(BlendMode.Set)
        is_selected = (i == edit_idx)

        if is_selected:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(bx, by, box_w, bh)
            label_color = Color.Bright
            value_color = Color.Bright
        else:
            canvas.set_color(Color.Low)
            canvas.draw_rect(bx, by, box_w, bh)
            label_color = Color.Medium
            value_color = Color.MediumBright

        # Name in upper portion
        canvas.set_color(label_color)
        nw = canvas.text_width(name)
        canvas.draw_text(bx + (box_w - nw) // 2, by + 9, name)

        # Separator inside the box
        canvas.set_color(Color.Low)
        canvas.hline(bx + 2, by + 14, box_w - 4)

        # Value in lower portion
        canvas.set_color(value_color)
        v = value_fn(stage)
        vw = canvas.text_width(v)
        canvas.draw_text(bx + (box_w - vw) // 2, by + 28, v)


# --------------------------------------------------------------------------
# Frame renderers
# --------------------------------------------------------------------------

def render_default_frame(canvas: Canvas, stages, active_idx: int,
                          selected_idx: int, bank: int):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=0, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, "GRID")
    WindowPainter.draw_footer(canvas,
                              names=["STAGE", "TIME", "PITCH", "DIV", "TRK"],
                              highlight=-1)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_bank_indicator(canvas, bank)
    draw_dividers(canvas, edit_mode=False)

    # Show curves for SELECTED stage (what the user is about to edit / inspecting)
    stage = stages[selected_idx]
    draw_temporal_scope(canvas, stage)
    draw_pitch_scope(canvas, stage)


def render_edit_frame(canvas: Canvas, stages, active_idx: int,
                       selected_idx: int, edit_param_idx: int, bank: int):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=0, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, f"ST{selected_idx + 1}")
    WindowPainter.draw_footer(canvas,
                              names=["TIME", "CURVE", "XFRM", "ACCM", "TRK"],
                              highlight=0)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_bank_indicator(canvas, bank)
    draw_dividers(canvas, edit_mode=True)

    stage = stages[selected_idx]
    draw_param_strip(canvas, stage, edit_idx=edit_param_idx)


# --------------------------------------------------------------------------
# Top-level: render both frames into one tall PNG
# --------------------------------------------------------------------------

def render_both(output_path: str, scale: int = 4):
    from PIL import Image

    stages = demo_sequence()

    # Frame 1: default — stage 1 is playing (active), stage 2 is selected for
    # inspection (the curves on the right reflect stage 2's params).
    fb1 = FrameBuffer(PageWidth, PageHeight)
    c1 = Canvas(fb1)
    render_default_frame(c1, stages,
                          active_idx=1,    # cell index, 0..15
                          selected_idx=2,
                          bank=0)
    img1 = framebuffer_to_image(fb1, scale=1)

    # Frame 2: edit-hold on stage 3, bottom-row cursor on TC (param idx 2 in
    # EDIT_PARAMS).
    fb2 = FrameBuffer(PageWidth, PageHeight)
    c2 = Canvas(fb2)
    render_edit_frame(c2, stages,
                       active_idx=1,
                       selected_idx=2,
                       edit_param_idx=2,
                       bank=0)
    img2 = framebuffer_to_image(fb2, scale=1)

    # Side-by-side with a 4-px dark gap between the two frames
    gap = 4
    combined = Image.new('L', (PageWidth * 2 + gap, PageHeight), color=8)
    combined.paste(img1, (0, 0))
    combined.paste(img2, (PageWidth + gap, 0))

    if scale > 1:
        combined = combined.resize(
            (combined.width * scale, combined.height * scale), Image.NEAREST)

    combined.save(output_path)
    return output_path


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output",
                        default="phaseflux-preview.png",
                        help="output PNG path")
    parser.add_argument("--scale", type=int, default=4)
    args = parser.parse_args()
    path = render_both(args.output, scale=args.scale)
    print(f"wrote {path}")
