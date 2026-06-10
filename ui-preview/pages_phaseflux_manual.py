"""
PhaseFlux manual renders — firmware-accurate page set for docs/phaseflux-guide.html.

Self-contained: does NOT use the older pages_phaseflux.py mock model (whose curve
and divisor tables predate the current engine). Geometry + footers mirror
ui/pages/PhaseFluxEditPage.cpp exactly:

  Grid   GridX=6 GridY=12 CellW=9 CellH=9 CellGap=1
  Scope  ScopeTempX=50 ScopeW=100 ScopeH=38 ScopeY=12
  Divs   DividerX=46 DividerX2=152
  Param  ParamPanelX=156 Y=12 W=98 H=40 (5 rows)

Topics (Left/Right) and pages (F5 Next) follow the cpp footer label tables:
  TEMP  P0 Curve/Warp/Resp/Puls   P1 Len/FlipV/FlipH/Mask   P2 Wind/Rep/-/-
  PTCH  P0 Curve/Warp/Resp/Note   P1 Span/Dir/FlipV/FlipH   P2 Wind/Rep/Mask/Tilt
  PTCH.G P0 Curve/Warp/Resp/Rate  P1 Note/Span/FlipV/FlipH
  ACCUM.N / ACCUM.P  P0 Ac.St/M:Sta/-/Order   (1x16 row layout)
  MACRO P0 WarpN/RespN/PulsN/LenN P1 Phase/GWarp/Snap/Zero
All footers carry F5 = Next.
"""

import sys
import os
import math

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter

# ---- geometry (mirror cpp) -------------------------------------------------
GRID_X, GRID_Y, CELL_W, CELL_H, CELL_GAP = 6, 12, 9, 9, 1
DIVIDER_X, DIVIDER_X2 = 46, 152
SCOPE_X, SCOPE_W, SCOPE_H, SCOPE_Y = 50, 100, 38, 12
PARAM_X, PARAM_Y, PARAM_W, PARAM_H = 156, 12, PageWidth - 156 - 2, 40
PARAM_ROWS = 5
PARAM_ROW_H = PARAM_H // PARAM_ROWS

# ---- firmware enums / labels -----------------------------------------------
TEMP_CURVES = ["Lin", "Bell", "Bnc"]          # Linear / Bell / Bounce(ExpDown3x)
PITCH_CURVES = ["Ramp", "Bell", "Tri", "Bnc"]  # Ramp / Bell / Triangle / Bounce
MASK_NAMES = ["Off", "1in2", "1in3", "1in4", "2in3", "2in4", "3in4", "1in8"]
MASK_BITS = [0x00, 0xAA, 0x49, 0x11, 0xB6, 0x55, 0x77, 0x01]
DIR_NAMES = ["Up", "Dn", "Bip"]
RANGE_NAMES = ["0.5", "1", "2", "3"]
WINDOW_NAMES = ["Off", "F70", "F50", "P70", "P50"]
REPEAT_NAMES = ["x1", "x2", "x3", "x5"]
DIV_NAMES = ["1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1bar", "2bar"]
ORDER_NAMES = ["Wrap", "Pend", "Hold", "RTZ"]


class Stage:
    def __init__(self, **kw):
        self.pulseCount = kw.get("pulseCount", 4)
        self.stageDivisor = kw.get("stageDivisor", 4)   # Quarter
        self.stageLen = kw.get("stageLen", 64)
        self.skip = kw.get("skip", False)
        self.temporalCurve = kw.get("temporalCurve", 0)
        self.temporalWarp = kw.get("temporalWarp", 0)
        self.temporalResponse = kw.get("temporalResponse", 0)
        self.temporalFlipV = kw.get("temporalFlipV", False)
        self.temporalFlipH = kw.get("temporalFlipH", False)
        self.pitchCurve = kw.get("pitchCurve", 0)
        self.pitchWarp = kw.get("pitchWarp", 0)
        self.pitchResponse = kw.get("pitchResponse", 0)
        self.pitchFlipV = kw.get("pitchFlipV", False)
        self.pitchFlipH = kw.get("pitchFlipH", False)
        self.basePitch = kw.get("basePitch", 0)
        self.pitchRange = kw.get("pitchRange", 1)        # index into RANGE_NAMES
        self.pitchDirection = kw.get("pitchDirection", 0)
        self.phaseShift = kw.get("phaseShift", 0)
        self.mask = kw.get("mask", 0)
        self.maskShift = kw.get("maskShift", 0)
        self.gateLength = kw.get("gateLength", 50)
        self.temporalWindow = kw.get("temporalWindow", 0)
        self.temporalRepeat = kw.get("temporalRepeat", 0)
        self.pitchWindow = kw.get("pitchWindow", 0)
        self.pitchRepeat = kw.get("pitchRepeat", 0)
        self.maskMelody = kw.get("maskMelody", 100)
        self.tiltMelody = kw.get("tiltMelody", 0)
        self.accumStep = kw.get("accumStep", 0)
        self.pulseAccumStep = kw.get("pulseAccumStep", 0)
        self.accumTrig = kw.get("accumTrig", 0)          # 0=Stage 1=Pulse
        self.pulseAccumTrig = kw.get("pulseAccumTrig", 0)


def demo_sequence():
    s = [Stage(skip=True) for _ in range(16)]
    s[0] = Stage(pulseCount=4, temporalCurve=0, pitchCurve=0,
                 accumStep=1, pulseAccumStep=0)
    s[1] = Stage(pulseCount=8, temporalCurve=1, temporalWarp=18,
                 pitchCurve=2, pitchRange=2, mask=4, phaseShift=1,
                 accumStep=2, pulseAccumStep=1)
    s[2] = Stage(pulseCount=4, temporalCurve=2, pitchCurve=1, basePitch=4,
                 accumStep=-1, pulseAccumStep=0, accumTrig=1)
    s[3] = Stage(pulseCount=6, temporalCurve=0, pitchCurve=3, pitchRange=2,
                 pitchDirection=2, accumStep=3, pulseAccumStep=2, pulseAccumTrig=1)
    return s


# ---- curve maths (mirror engine Curve::eval + PhaseFluxMath::powerBend) -----
def power_bend(z, knob):
    if knob == 0:
        return z
    z = max(0.0, min(1.0, z))
    p = knob / 64.0
    return z ** ((1.0 - p) / (1.0 + p))


def temporal_eval(idx, x):
    if idx == 0:        # Linear / RampUp
        return x
    if idx == 1:        # Bell
        return math.sin(math.pi * x)
    seg = (x * 3.0) % 1.0   # Bounce / ExpDown3x — three cascading drops
    return math.exp(-3.2 * seg)


def pitch_eval(idx, x):
    if idx == 0:        # Ramp
        return x
    if idx == 1:        # Bell
        return math.sin(math.pi * x)
    if idx == 2:        # Triangle
        return 2.0 * x if x < 0.5 else 2.0 - 2.0 * x
    seg = (x * 3.0) % 1.0   # Bounce
    return math.exp(-3.2 * seg)


def eval_temporal(stage, x):
    t = power_bend(x, stage.temporalWarp)
    if stage.temporalFlipH:
        t = 1.0 - t
    t = temporal_eval(stage.temporalCurve, t)
    if stage.temporalFlipV:
        t = 1.0 - t
    return power_bend(max(0.0, min(1.0, t)), stage.temporalResponse)


def eval_pitch(stage, phi):
    p = power_bend(phi, stage.pitchWarp)
    if stage.pitchFlipH:
        p = 1.0 - p
    p = pitch_eval(stage.pitchCurve, p)
    if stage.pitchFlipV:
        p = 1.0 - p
    return power_bend(max(0.0, min(1.0, p)), stage.pitchResponse)


# ---- body draws ------------------------------------------------------------
def draw_grid(canvas, stages, active_idx, selected_idx):
    for i, stage in enumerate(stages):
        row, col = i // 4, i % 4
        x = GRID_X + col * (CELL_W + CELL_GAP)
        y = GRID_Y + row * (CELL_H + CELL_GAP)
        is_active = (i == active_idx)
        is_sel = (i == selected_idx)
        if is_active:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x, y, CELL_W, CELL_H)
        elif stage.skip:
            canvas.set_color(Color.Low)
            canvas.draw_rect(x, y, CELL_W, CELL_H)
            canvas.line(x + 2, y + 2, x + CELL_W - 3, y + CELL_H - 3)
            canvas.line(x + CELL_W - 3, y + 2, x + 2, y + CELL_H - 3)
        else:
            canvas.set_color(Color.Bright if is_sel else Color.MediumLow)
            canvas.draw_rect(x, y, CELL_W, CELL_H)
        if stage.skip:
            continue
        pulses = max(0, min(16, stage.pulseCount))
        bar = Color.None_ if is_active else (Color.Bright if is_sel else Color.MediumBright)
        canvas.set_color(bar)
        canvas.hline(x + 1, y + CELL_H - 2, min(CELL_W - 2, pulses))
        if not is_active:
            if stage.phaseShift != 0:
                canvas.set_color(bar)
                canvas.point(x + 1, y + 1)
            if stage.mask != 0:
                canvas.set_color(bar)
                canvas.point(x + CELL_W - 2, y + 1)


def draw_dividers(canvas):
    canvas.set_color(Color.Low)
    canvas.vline(DIVIDER_X, 13, 39)
    canvas.vline(DIVIDER_X2, 13, 39)


def draw_stage_badge(canvas, idx):
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.MediumBright)
    canvas.draw_text(SCOPE_X + 2, SCOPE_Y + 6, f"ST{idx + 1}")


def draw_temporal_scope(canvas, stage):
    x, y = SCOPE_X, SCOPE_Y
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, SCOPE_W, SCOPE_H)
    canvas.hline(x + 1, y + SCOPE_H - 2, SCOPE_W - 2)
    # density trace
    canvas.set_color(Color.Bright)
    rep = REPEAT_NAMES.index(REPEAT_NAMES[stage.temporalRepeat]) and 0  # noqa (rep handled below)
    rep_mult = [1, 2, 3, 5][stage.temporalRepeat]
    prev = None
    for px in range(SCOPE_W - 2):
        traw = px / float(SCOPE_W - 3)
        tlocal = (traw * rep_mult) % 1.0
        if not _in_window(tlocal, stage.temporalWindow):
            prev = None
            continue
        v = eval_temporal(stage, tlocal)
        py = y + SCOPE_H - 2 - int(v * (SCOPE_H - 3))
        cx = x + px
        if prev is not None:
            canvas.line(cx - 1, prev, cx, py)
        prev = py
    # pulse rings
    n = max(0, min(16, stage.pulseCount))
    mark_y = y + SCOPE_H // 2
    inner = SCOPE_W - 5
    mb = MASK_BITS[stage.mask]
    for i in range(n):
        muted = (mb >> ((i + stage.maskShift) & 7)) & 1
        if n > 1:
            t = i / float(n - 1)
        else:
            t = 0.0
        if not _in_window(t, stage.temporalWindow):
            continue
        cx = x + 2 + int(t * inner)
        col = Color.Low if muted else Color.MediumBright
        canvas.set_color(col)
        canvas.draw_rect(cx - 1, mark_y - 1, 3, 3)


def draw_pitch_scope(canvas, stage):
    x, y = SCOPE_X, SCOPE_Y
    canvas.set_color(Color.Low)
    canvas.draw_rect(x, y, SCOPE_W, SCOPE_H)
    canvas.hline(x + 1, y + SCOPE_H - 2, SCOPE_W - 2)
    canvas.set_color(Color.Bright)
    rep_mult = [1, 2, 3, 5][stage.pitchRepeat]
    prev = None
    for px in range(SCOPE_W - 2):
        phi = px / float(SCOPE_W - 3)
        phi = _hold_window(phi, stage.pitchWindow)
        phi = (phi * rep_mult) % 1.0 if rep_mult > 1 else phi
        v = eval_pitch(stage, phi)
        py = y + SCOPE_H - 2 - int(v * (SCOPE_H - 3))
        cx = x + px
        if prev is not None:
            canvas.line(cx - 1, prev, cx, py)
        prev = py


def _in_window(phi, w):
    if w == 0:
        return True
    if w == 1:
        return 0.15 <= phi <= 0.85
    if w == 2:
        return 0.25 <= phi <= 0.75
    if w == 3:
        return phi <= 0.35 or phi >= 0.65
    return phi <= 0.25 or phi >= 0.75


def _hold_window(phi, w):
    if w == 1:
        return min(max(phi, 0.15), 0.85)
    if w == 2:
        return min(max(phi, 0.25), 0.75)
    if w == 3 and 0.35 < phi < 0.65:
        return 0.35 if phi < 0.5 else 0.65
    if w == 4 and 0.25 < phi < 0.75:
        return 0.25 if phi < 0.5 else 0.75
    return phi


def draw_param_list(canvas, rows, selected):
    canvas.set_font(Font.Tiny)
    for i, (name, value) in enumerate(rows):
        ry = PARAM_Y + i * PARAM_ROW_H
        rh = PARAM_ROW_H - 1
        if i == selected:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(PARAM_X, ry, PARAM_W, rh)
            nc = vc = Color.Bright
        else:
            nc, vc = Color.Medium, Color.MediumBright
        ty = ry + rh - 2
        canvas.set_color(nc)
        canvas.draw_text(PARAM_X + 2, ty, name)
        vw = canvas.text_width(value)
        canvas.set_color(vc)
        canvas.draw_text(PARAM_X + PARAM_W - 2 - vw, ty, value)


def _sgn(v):
    return "0" if v == 0 else f"{v:+d}"


# ---- page composition ------------------------------------------------------
def _chrome(canvas, active_fn, footer, highlight):
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=6, play_pattern=0, edit_pattern=0, mode="PHFLX")
    WindowPainter.draw_active_function(canvas, active_fn)
    WindowPainter.draw_footer(canvas, names=footer, highlight=highlight)


def render_temp(canvas, stages, sel, act, page, hi):
    footers = {
        0: ["Curve", "Warp", "Resp", "Puls", "Next"],
        1: ["Len", "FlipV", "FlipH", "Mask", "Next"],
        2: ["Wind", "Rep", None, None, "Next"],
    }
    s = stages[sel]
    rows = {
        0: [("Curve", TEMP_CURVES[s.temporalCurve]), ("Warp", _sgn(s.temporalWarp)),
            ("Resp", _sgn(s.temporalResponse)), ("Puls", str(s.pulseCount)), ("", "")],
        1: [("Len", str(s.stageLen)), ("FlipV", "On" if s.temporalFlipV else "Off"),
            ("FlipH", "On" if s.temporalFlipH else "Off"), ("Mask", MASK_NAMES[s.mask]), ("", "")],
        2: [("Wind", WINDOW_NAMES[s.temporalWindow]), ("Rep", REPEAT_NAMES[s.temporalRepeat]),
            ("", ""), ("", ""), ("", "")],
    }
    _chrome(canvas, "TEMP", footers[page], hi)
    draw_grid(canvas, stages, act, sel)
    draw_dividers(canvas)
    draw_temporal_scope(canvas, s)
    draw_stage_badge(canvas, sel)
    draw_param_list(canvas, rows[page], hi)


def render_ptch(canvas, stages, sel, act, page, hi):
    footers = {
        0: ["Curve", "Warp", "Resp", "Note", "Next"],
        1: ["Span", "Dir", "FlipV", "FlipH", "Next"],
        2: ["Wind", "Rep", "Mask", "Tilt", "Next"],
    }
    s = stages[sel]
    rows = {
        0: [("Curve", PITCH_CURVES[s.pitchCurve]), ("Warp", _sgn(s.pitchWarp)),
            ("Resp", _sgn(s.pitchResponse)), ("Note", _sgn(s.basePitch)), ("", "")],
        1: [("Span", RANGE_NAMES[s.pitchRange]), ("Dir", DIR_NAMES[s.pitchDirection]),
            ("FlipV", "On" if s.pitchFlipV else "Off"), ("FlipH", "On" if s.pitchFlipH else "Off"), ("", "")],
        2: [("Wind", WINDOW_NAMES[s.pitchWindow]), ("Rep", REPEAT_NAMES[s.pitchRepeat]),
            ("Mask", str(s.maskMelody)), ("Tilt", str(s.tiltMelody)), ("", "")],
    }
    _chrome(canvas, "PTCH", footers[page], hi)
    draw_grid(canvas, stages, act, sel)
    draw_dividers(canvas)
    draw_pitch_scope(canvas, s)
    draw_stage_badge(canvas, sel)
    draw_param_list(canvas, rows[page], hi)


def render_ptch_global(canvas, stages, sel, act, page, hi):
    footers = {
        0: ["Curve", "Warp", "Resp", "Rate", "Next"],
        1: ["Note", "Span", "FlipV", "FlipH", "Next"],
    }
    master = stages[0]
    active = stages[sel]
    rows = {
        0: [("Curve", PITCH_CURVES[master.pitchCurve]), ("Warp", _sgn(master.pitchWarp)),
            ("Resp", _sgn(master.pitchResponse)), ("Rate", "5:7"), ("", "")],
        1: [("Note", _sgn(active.basePitch)), ("Span", RANGE_NAMES[active.pitchRange]),
            ("FlipV", "On" if master.pitchFlipV else "Off"), ("FlipH", "On" if master.pitchFlipH else "Off"), ("", "")],
    }
    _chrome(canvas, "PTCH.G", footers[page], hi)
    draw_grid(canvas, stages, act, sel)
    draw_dividers(canvas)
    draw_pitch_scope(canvas, master)
    draw_stage_badge(canvas, 0)
    draw_param_list(canvas, rows[page], hi)


# ---- ACCUM page (1x16 row + pancakes + glyphs, mirrors drawAccumPage) ------
ACC_X0, ACC_GAP, ACC_ROW_Y = 17, 6, 28
GLYPH_X = 4
GLYPH_ORDER_Y, GLYPH_POLAR_Y, GLYPH_RESET_BL = 18, 28, 43
G_WRAP = [0x04, 0x08, 0x1F, 0x08, 0x04]
G_PEND = [0x00, 0x0A, 0x1F, 0x0A, 0x00]
G_HOLD = [0x00, 0x00, 0x1F, 0x00, 0x00]
G_RTZ = [0x01, 0x01, 0x0F, 0x08, 0x04]
G_UNI = [0x04, 0x0E, 0x15, 0x04, 0x04]
G_BI = [0x04, 0x0E, 0x04, 0x0E, 0x04]


def _glyph(canvas, x, y, pat):
    for r in range(5):
        b = pat[r]
        for c in range(5):
            if (b >> c) & 1:
                canvas.point(x + c, y + r)


def render_accum(canvas, stages, which, sel, act):
    # which: "N" or "P"
    active_fn = "ACCUM.N" if which == "N" else "ACCUM.P"
    footer = ["Ac.St", "M:Sta", None, "Order", "Next"]
    _chrome(canvas, active_fn, footer, 0)
    # left-margin glyphs: order (Wrap), polarity (Uni), reset value
    canvas.set_color(Color.MediumBright)
    _glyph(canvas, GLYPH_X, GLYPH_ORDER_Y - 2, G_WRAP)
    _glyph(canvas, GLYPH_X, GLYPH_POLAR_Y - 2, G_UNI)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(GLYPH_X - 2, GLYPH_RESET_BL, "R4")
    # 1x16 cells with step pancakes + S/P trig glyphs
    for i, stage in enumerate(stages):
        cx = ACC_X0 + i * (CELL_W + ACC_GAP)
        cy = ACC_ROW_Y
        is_active = (i == act)
        is_sel = (i == sel)
        if is_active:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(cx, cy, CELL_W, CELL_H)
        elif stage.skip:
            canvas.set_color(Color.Low)
            canvas.draw_rect(cx, cy, CELL_W, CELL_H)
            canvas.line(cx + 2, cy + 2, cx + CELL_W - 3, cy + CELL_H - 3)
            canvas.line(cx + CELL_W - 3, cy + 2, cx + 2, cy + CELL_H - 3)
        else:
            canvas.set_color(Color.Bright if is_sel else Color.MediumLow)
            canvas.draw_rect(cx, cy, CELL_W, CELL_H)
        if stage.skip:
            continue
        # amount pancake: bar height above the cell proportional to step
        step = stage.accumStep if which == "N" else stage.pulseAccumStep
        if step:
            h = min(6, abs(step))
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(cx + 5, cy - 1 - h, 3, h)
        # trig glyph below: S (Stage) or P (Pulse)
        trig = stage.accumTrig if which == "N" else stage.pulseAccumTrig
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Medium)
        canvas.draw_text(cx + 2, cy + CELL_H + 7, "P" if trig else "S")


# ---- MACRO page (full-width scope) -----------------------------------------
def render_macro(canvas, stages, page, hi):
    footers = {
        0: ["WarpN", "RespN", "PulsN", "LenN", "Next"],
        1: ["Phase", "GWarp", "Snap", "Zero", "Next"],
    }
    _chrome(canvas, "MACRO", footers[page], hi)
    # full-width macro scope: x=2..253, show a composite curve made of the
    # per-cell temporal curves laid end to end (the live macro feedback).
    x0, x1, top, bot = 2, 253, 14, 50
    canvas.set_color(Color.Low)
    canvas.draw_rect(x0, top, x1 - x0, bot - top)
    active = [s for s in stages if not s.skip]
    n = max(1, len(active))
    canvas.set_color(Color.Bright)
    prev = None
    span = (x1 - x0 - 2)
    for px in range(span):
        f = px / float(span)
        idx = min(n - 1, int(f * n))
        local = (f * n) - int(f * n)
        v = eval_temporal(active[idx], local)
        py = bot - 1 - int(v * (bot - top - 3))
        cx = x0 + 1 + px
        if prev is not None:
            canvas.line(cx - 1, prev, cx, py)
        prev = py


# ---- driver ----------------------------------------------------------------
def _emit(out_dir, name, render):
    fb = FrameBuffer(PageWidth, PageHeight)
    c = Canvas(fb)
    render(c)
    path = os.path.join(out_dir, name)
    framebuffer_to_image(fb, scale=4).save(path)
    print(f"wrote {path}")


def main():
    out = os.path.join(os.path.dirname(__file__), "phaseflux-manual")
    os.makedirs(out, exist_ok=True)
    st = demo_sequence()
    sel, act = 1, 2

    _emit(out, "phflx-temp-p0.png", lambda c: render_temp(c, st, sel, act, 0, 1))
    _emit(out, "phflx-temp-p1.png", lambda c: render_temp(c, st, sel, act, 1, 3))
    _emit(out, "phflx-temp-p2.png", lambda c: render_temp(c, st, sel, act, 2, 0))
    _emit(out, "phflx-ptch-p0.png", lambda c: render_ptch(c, st, sel, act, 0, 0))
    _emit(out, "phflx-ptch-p1.png", lambda c: render_ptch(c, st, sel, act, 1, 1))
    _emit(out, "phflx-ptch-p2.png", lambda c: render_ptch(c, st, sel, act, 2, 2))
    _emit(out, "phflx-ptchg-p0.png", lambda c: render_ptch_global(c, st, sel, act, 0, 3))
    _emit(out, "phflx-ptchg-p1.png", lambda c: render_ptch_global(c, st, sel, act, 1, 0))
    _emit(out, "phflx-accum-n.png", lambda c: render_accum(c, st, "N", 1, act))
    _emit(out, "phflx-accum-p.png", lambda c: render_accum(c, st, "P", 1, act))
    _emit(out, "phflx-macro-p0.png", lambda c: render_macro(c, st, 0, 0))
    _emit(out, "phflx-macro-p1.png", lambda c: render_macro(c, st, 1, 0))


if __name__ == "__main__":
    main()
