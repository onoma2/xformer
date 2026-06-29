#!/usr/bin/env python3
"""
Actual-state OLED renders for the Fractal track edit/hero pages, faithful to
FractalSequenceEditPage.cpp draw calls. Hero ring F5=NEXT: Trunk -> Branch ->
Ornament -> Source. Outputs to ui-preview/fractal-manual/.

Reuses render_fractal_hero.py's canvas setup + helper idiom (_new/_tt/_tr/_bar);
the per-page geometry/labels are re-derived line-for-line from the firmware
.cpp rather than the older proposal layout that hero.py still carries.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, framebuffer_to_image
from painters import WindowPainter
from render_fractal_hero import _new  # reuse hero canvas setup helper

W, H = 256, 64

# Firmware tables (FractalSequenceEditPage.cpp / FractalTrack.h / engine).
POOL = ["Tra", "Rev", "Inv", "RtI", "Rot", "Cmp", "Exp", "Thn"]
GATE_LOGIC = ["A", "B", "And", "Or", "Xor", "Nand"]
CV_LOGIC = ["A", "B", "Min", "Max", "Sum", "Avg", "Gat"]
ORNAMENTS = [
    "Anticipation", "Suspension", "Syncopation", "Octave-Up", "Fifth-Up",
    "Half-Turn Toward", "Half-Turn Away", "Run Toward", "Run Away", "Turn",
    "Arp Toward", "Arp Away", "Mordent Up", "Mordent Down", "Trill",
]


def _header_footer(c, active_fn, footer, fn_label):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=5, mode="FRACTAL")
    WindowPainter.draw_active_function(c, fn_label)
    WindowPainter.draw_footer(c, footer, highlight=active_fn)


def _draw_bar(c, x, y, w, h, pct, highlight):
    """Mirror of drawBar(): Low outline, filled portion bright/medium-bright."""
    c.set_color(Color.Low)
    c.draw_rect(x, y, w, h)
    fw = max(1, (w - 2) * max(0, min(100, pct)) // 100)
    c.set_color(Color.Bright if highlight else Color.MediumBright)
    c.fill_rect(x + 1, y + 1, fw, h - 2)


def _draw_seg_row(c, x, y, w, items, sel, focus):
    """Mirror of drawSegRow(): single-select cells, on=Bright/MediumBright."""
    n = len(items)
    cw = w // n
    for i, it in enumerate(items):
        bx = x + i * cw
        on = i == sel
        c.set_color(Color.Bright if on else Color.Low)
        c.draw_rect(bx, y, cw - 1, 9)
        c.set_color((Color.Bright if focus else Color.MediumBright) if on else Color.MediumLow)
        c.draw_text(bx + (cw - 1 - c.text_width(it)) // 2, y + 7, it)


# ---------------------------------------------------------------------------
# Trunk — captured-loop tape, nested rec/loop/orn zone lines, playhead, keypad.
# drawTrunk(): tapeTop=25, tapeBot=44; zone lines at tapeTop-7/-5/-3.
# ---------------------------------------------------------------------------
def render_trunk(c, bracket="Loop"):
    foc = {"Record": 0, "Loop": 1, "Ornament": 2}[bracket]
    _header_footer(c, foc, ["REC", "LOOP", "ORN", "R.Skip", "NEXT"], "LOOP")

    N = 16
    rec_first, rec_last = 0, 15
    loop_first, loop_last = 0, 11
    orn_first, orn_last = 3, 10
    cur = 5
    record_skip = 0

    tape_top, tape_bot = 25, 44
    tape_h = tape_bot - tape_top + 1
    margin = 4
    step_w = max(1, (W - 2 * margin) // N)
    used = step_w * N
    x0 = (W - used) // 2

    c.set_color(Color.Low)
    c.draw_rect(x0, tape_top, used, tape_h)

    # captured cells: height=pitch ((semis+60)/120), width=gate length (0..15).
    semis_demo = [0, -7, 5, 12, -3, 7, 0, None, 2, 9, -5, 4, None, 14, -12, 3]
    gate_demo = [12, 9, 15, 10, 6, 15, 8, 0, 11, 15, 5, 9, 0, 15, 4, 7]
    for i in range(N):
        semis = semis_demo[i]
        gate_len = gate_demo[i]
        if semis is None or gate_len == 0:
            continue
        pitch = (semis + 60.0) / 120.0
        pitch = max(0.05, min(1.0, pitch))
        bar_h = max(1, int((tape_h - 3) * pitch))
        bw = max(1, step_w * gate_len // 15)
        x = x0 + i * step_w
        in_loop = loop_first <= i <= loop_last
        is_cur = i == cur
        c.set_color(Color.Bright if is_cur else (Color.MediumBright if in_loop else Color.Low))
        c.fill_rect(x, tape_bot - bar_h, bw, bar_h)

    # nested zone lines (rec ⊇ loop ⊇ orn), focused bracket bright.
    c.set_color(Color.Bright if bracket == "Record" else Color.Medium)
    c.hline(x0 + rec_first * step_w, tape_top - 7, (rec_last - rec_first + 1) * step_w)
    c.set_color(Color.Bright if bracket == "Loop" else Color.MediumBright)
    c.hline(x0 + loop_first * step_w, tape_top - 5, (loop_last - loop_first + 1) * step_w)
    c.set_color(Color.Bright if bracket == "Ornament" else Color.MediumBright)
    c.hline(x0 + orn_first * step_w, tape_top - 3, (orn_last - orn_first + 1) * step_w)

    c.set_color(Color.Bright)
    c.vline(x0 + cur * step_w + step_w // 2, tape_top, tape_h)

    c.set_font(Font.Tiny)
    c.set_color(Color.Bright if bracket == "Record" else Color.Medium)
    c.draw_text(2, 16, f"rec {rec_first + 1}-{rec_last + 1}")
    c.set_color(Color.Medium)
    s = f"orn {orn_first + 1}-{orn_last + 1}"
    c.draw_text(W - 2 - c.text_width(s), 16, s)
    c.set_color(Color.Medium)
    c.draw_text(90, 51, f"loop {loop_first + 1}-{loop_last + 1}")
    c.set_color(Color.Medium)
    c.draw_text(2, 51, f"R.Skip {record_skip}")
    c.set_color(Color.Medium)
    s = f"{N}c"
    c.draw_text(W - 2 - c.text_width(s), 51, s)


# ---------------------------------------------------------------------------
# Branch — route readout, T+B1..BN chain blocks, transform-pool toggles.
# drawBranch(): blocks y=26 h=10, pool y=45 6x6.
# ---------------------------------------------------------------------------
def render_branch(c, focus="Count"):
    foc = {"Count": 0, "Path": 1, "Pool": 2, "Seed": 3}[focus]
    _header_footer(c, foc, ["CNT", "PATH", "POOL", "SEED", "NEXT"], "BRANCH")

    N = 3
    route = [1, 3, 2]  # T>B1>B3>B2
    block_labels = ["T", "Tra+3", "Rev", "Rot5"]
    playing = 1
    queued = 2
    pool = 0b00111111  # Tra..Cmp on, Exp/Thn off
    pool_index = 0

    c.set_font(Font.Tiny)
    c.set_color(Color.Medium)
    c.draw_text(2, 17, "Route")
    c.set_color(Color.Bright)
    rstr = "T" + "".join(f">B{r}" for r in route)
    c.draw_text(40, 17, rstr)
    c.set_color(Color.MediumBright)
    s = f"N={N}"
    c.draw_text(W - 2 - c.text_width(s), 17, s)

    cols = N + 1
    bw = W // cols
    y = 26
    for j in range(cols):
        x = j * bw
        is_playing = j == playing
        is_queued = queued >= 0 and j == queued
        c.set_color(Color.Bright if is_playing else Color.Medium)
        c.draw_rect(x + 1, y, bw - 2, 10)
        label = block_labels[j]
        c.set_color(Color.Bright if is_playing else Color.MediumBright)
        c.draw_text(x + 1 + (bw - 2 - c.text_width(label)) // 2, y + 6, label)
        if is_queued:
            c.set_color(Color.MediumBright)
            c.draw_text(x + 2, y - 1, "Q")

    pw = W // 8
    py = 45
    for k in range(8):
        x = k * pw
        en = (pool >> k) & 1
        sel = (focus == "Pool") and (k == pool_index)
        c.set_color(Color.Bright if en else Color.Low)
        c.draw_rect(x + 1, py, 6, 6)
        if en:
            c.fill_rect(x + 2, py + 1, 4, 4)
        if sel:
            c.set_color(Color.Bright)
            c.draw_rect(x - 1, py - 2, 10, 10)
        c.set_color(Color.MediumBright if en else Color.Low)
        c.draw_text(x + 10, py + 5, POOL[k])


# ---------------------------------------------------------------------------
# Ornament — Rate/Intens bars (tier ticks), Scale+zone, Last + queued.
# drawOrnament(): bars y=13/24 w=96; tier ticks 40%/75%.
# ---------------------------------------------------------------------------
def render_ornament(c, focus="Rate", lock=False):
    if lock:
        foc = 3
    else:
        foc = {"Rate": 0, "Intensity": 1, "Scale": 2}[focus]
    _header_footer(c, foc, ["RATE", "INT", "SCALE", "LOCK", "NEXT"], "ORN")

    rate = 35
    intensity = 60
    scale_name = "Major"
    orn_first, orn_last = 3, 10
    last_orn = 12  # Mordent Up
    queued_orn = -1

    c.set_font(Font.Tiny)
    c.set_color(Color.Medium)
    c.draw_text(2, 17, "Rate")
    _draw_bar(c, 40, 13, 96, 6, rate, focus == "Rate")
    c.set_color(Color.MediumBright)
    s = f"{rate}%"
    c.draw_text(W - 2 - c.text_width(s), 17, s)

    c.set_color(Color.Medium)
    c.draw_text(2, 28, "Intens")
    _draw_bar(c, 40, 24, 96, 6, intensity, focus == "Intensity")
    c.set_color(Color.MediumLow)
    c.vline(40 + 1 + (96 - 2) * 40 // 100, 24, 6)
    c.vline(40 + 1 + (96 - 2) * 75 // 100, 24, 6)
    tier = "off" if intensity == 0 else ("8-trill" if intensity >= 75 else ("4-step" if intensity >= 40 else "2-step"))
    c.set_color(Color.MediumBright)
    s = f"{intensity}% {tier}"
    c.draw_text(W - 2 - c.text_width(s), 28, s)

    c.set_color(Color.Medium)
    c.draw_text(2, 39, "Scale")
    c.set_color(Color.Bright if focus == "Scale" else Color.MediumBright)
    c.draw_text(40, 39, scale_name)
    c.set_color(Color.MediumBright)
    s = f"zone {orn_first + 1}-{orn_last + 1}"
    c.draw_text(W - 2 - c.text_width(s), 39, s)

    c.set_color(Color.Medium)
    c.draw_text(2, 50, "Last")
    c.set_color(Color.MediumBright)
    c.draw_text(40, 50, ORNAMENTS[last_orn])
    if queued_orn >= 0:
        c.set_color(Color.Bright)
        s = f"Q {ORNAMENTS[queued_orn]}"
        c.draw_text(W - 2 - c.text_width(s), 50, s)


# ---------------------------------------------------------------------------
# Source — SrcA/SrcB refs, Gate/CV seg rows, "<gate> + <cv>" result readout.
# drawSource(): gate row y=22, cv row y=33, result y=51.
# ---------------------------------------------------------------------------
def render_source(c, focus="Gate"):
    foc = {"SourceA": 0, "SourceB": 1, "Gate": 2, "Cv": 3}[focus]
    _header_footer(c, foc, ["SrcA", "SrcB", "GATE", "CV", "NEXT"], "MIX")

    src_a = "N T1"     # Note track 1 (track kind -> plain "A")
    src_b = "S T3"     # Stochastic track 3 (plain "B")
    a_is_channel = False
    b_is_channel = False
    gate_sel = 2       # And
    cv_sel = 4         # Sum

    c.set_font(Font.Tiny)
    c.set_color(Color.Medium)
    c.draw_text(2, 16, "A-CV" if a_is_channel else "A")
    c.set_color(Color.Bright if focus == "SourceA" else Color.MediumBright)
    c.draw_text(28, 16, src_a)

    c.set_color(Color.Medium)
    c.draw_text(196, 16, "B-Gt" if b_is_channel else "B")
    c.set_color(Color.Bright if focus == "SourceB" else Color.MediumBright)
    c.draw_text(W - 2 - c.text_width(src_b), 16, src_b)

    c.set_color(Color.Medium)
    c.draw_text(2, 29, "Gate")
    _draw_seg_row(c, 40, 22, 214, GATE_LOGIC, gate_sel, focus == "Gate")

    c.set_color(Color.Medium)
    c.draw_text(2, 40, "CV")
    _draw_seg_row(c, 40, 33, 214, CV_LOGIC, cv_sel, focus == "Cv")

    c.set_color(Color.Medium)
    c.draw_text(2, 51, f"{GATE_LOGIC[gate_sel]} + {CV_LOGIC[cv_sel]}")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "fractal-manual")
    os.makedirs(outdir, exist_ok=True)
    pages = {
        "trunk": lambda c: render_trunk(c, "Loop"),
        "branch": lambda c: render_branch(c, "Pool"),
        "ornament": lambda c: render_ornament(c, "Rate"),
        "source": lambda c: render_source(c, "Gate"),
    }
    for slug, fn in pages.items():
        fb, c = _new()
        fn(c)
        framebuffer_to_image(fb, scale=4).save(os.path.join(outdir, f"{slug}.png"))
        print(f"saved {slug}")


if __name__ == "__main__":
    main()
