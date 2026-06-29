#!/usr/bin/env python3
"""
Standalone actual-state renderer for the MODULATOR_MANUAL.

Composes faithful 256x64 OLED frames for the modulator bank page exactly as
ModulatorPage.cpp draws them: left rolling-scope box (x=1,y=12,119x40), a
divider at x=122, and a right PhaseFlux param list (5 rows, Tiny font, name
flush-left / value flush-right, selected row boxed). Labels and values are the
real firmware strings (FREE/TRIG/HOLD modes, per-shape F-key relabels, JustF
INTONE, Geode globals/voice, the routing membership grid).

Reuses the firmware-mirroring helpers in pages_modulator.py / painters.py —
nothing here invents a new layout. Read-only: does NOT edit generate.py or
shared modules.

  python3 ui-preview/render_modulator_manual.py            # render all -> modulator-manual/
  python3 ui-preview/render_modulator_manual.py <slug> -o out.png
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, framebuffer_to_image
from painters import WindowPainter
from pages_modulator import (
    _draw_modulator_scope,
    _draw_param_list_phaseflux,
    _draw_dest_membership_grid,
)

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "modulator-manual")


def _frame(mode, labels, values, selected, cv_label=None, page_label="",
           scope_cycles=2.2):
    """One modulator page: header + scope-left + divider + param-list-right + footer.
    Mirrors ModulatorPage::draw (the non-routing branch)."""
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=0, mode=mode)
    WindowPainter.draw_active_function(canvas, page_label)
    WindowPainter.draw_footer(canvas, [l or None for l in labels], highlight=selected)

    _draw_modulator_scope(canvas, samples_cycles=scope_cycles)
    canvas.set_color(Color.Low)
    canvas.vline(122, 13, 39)
    _draw_param_list_phaseflux(canvas, labels, values, selected)

    if cv_label:
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        canvas.draw_text(3, 51, f"-> {cv_label}")
    return fb


# --- LFO (Sine) — single page, real labels SHAPE/RATE/DEPTH/PHASE/OFFSET -----
def sine(canvas_unused=None):
    return _frame(
        "MOD 1 - MODULATOR",
        ["SHAPE", "RATE", "DEPTH", "PHASE", "OFFSET"],
        ["Sine", "1.50Hz", "64", "45°", "+0"],
        selected=1, cv_label="CV1", scope_cycles=2.2)


# --- ADSR page 1: SHAPE/ATTACK/DECAY/SUSTAIN/RELEASE -------------------------
def adsr(canvas_unused=None):
    return _frame(
        "MOD 2 - MODULATOR",
        ["SHAPE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"],
        ["ADSR", "300ms", "400ms", "80", "600ms"],
        selected=2, page_label="Pg 1/2", cv_label="CV2", scope_cycles=0.0)


# --- ADSR page 2: DEPTH(F3)/INVERT(F4)/OFFSET(F5) ----------------------------
def adsr_page2(canvas_unused=None):
    return _frame(
        "MOD 2 - MODULATOR",
        [None, None, "DEPTH", "INVERT", "OFFSET"],
        ["", "", "100", "OFF", "+1.2V"],
        selected=2, page_label="Pg 2/2", cv_label="CV2", scope_cycles=0.0)


# --- Random — Mode (FREE/TRIG/HOLD) drives behaviour; F4 = SLEW --------------
def random(canvas_unused=None):
    return _frame(
        "MOD 3 - MODULATOR",
        ["SHAPE", "RATE", "DEPTH", "SLEW", "OFFSET"],
        ["Random", "0.20Hz", "60", "250ms", "-5"],
        selected=0, cv_label="CV4", scope_cycles=1.0)


# --- Chaos (Lorenz) page 1: SHAPE/RATE/DEPTH/P1/P2 ---------------------------
def chaos(canvas_unused=None):
    return _frame(
        "MOD 4 - MODULATOR",
        ["SHAPE", "RATE", "DEPTH", "P1", "P2"],
        ["Lorenz", "1.50Hz", "80", "45", "62"],
        selected=3, page_label="Pg 1/2", cv_label="CV4", scope_cycles=3.3)


# --- Spring page 1: SHAPE/STRIKE/TENS/RING/CLANG -----------------------------
def spring(canvas_unused=None):
    return _frame(
        "MOD 5 - MODULATOR",
        ["SHAPE", "STRIKE", "TENS", "RING", "CLANG"],
        ["Spring", "1.50Hz", "4.5Hz", "80%", "50%"],
        selected=2, page_label="Pg 1/2", cv_label="CV5", scope_cycles=4.6)


# --- JustF: M2's RATE cell becomes INTONE (global spread), derived Hz below --
def justf_intone(canvas_unused=None):
    fb = _frame(
        "MOD 2 - MODULATOR",
        ["SHAPE", "INTONE", "DEPTH", "PHASE", "OFFSET"],
        ["Triangle", "+0.50", "60", "0°", "-5"],
        selected=1, cv_label="CV2", scope_cycles=2.2)
    # M2's own derived rate, tiny, under the INTONE value (firmware draws it at y=31).
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    sub = "3.0Hz"
    canvas.draw_text(256 - canvas.text_width(sub) - 2, 31, sub)
    return fb


# --- JustF follower (M5): RATE cell shows the derived rate, read-only --------
def justf_follower(canvas_unused=None):
    return _frame(
        "MOD 5 - MODULATOR",
        ["SHAPE", "RATE", "DEPTH", "PHASE", "OFFSET"],
        ["Saw Up", "6.00Hz", "40", "90°", "+0"],
        selected=0, cv_label="CV5", scope_cycles=4.4)


# --- Geode M1 page 2 globals: TIME/INTONE/RAMP/CURVE/MODE --------------------
def geode_globals(canvas_unused=None):
    return _frame(
        "MOD 1 - GEODE",
        ["TIME", "INTONE", "RAMP", "CURVE", "MODE"],
        ["158ms", "+0.50", "0.50", "LIN", "SUST"],
        selected=1, page_label="Pg 2/2", scope_cycles=2.2)


# --- Geode voice (M5 = voice 3): VOICE/DIVS/REPEAT/TUNE/TIME -----------------
def geode_voice(canvas_unused=None):
    return _frame(
        "MOD 5 - GEODE",
        ["VOICE", "DIVS", "REPEAT", "TUNE", "TIME"],
        ["V3", "8", "4", "3:1", "53ms"],
        selected=1, scope_cycles=1.5)


# --- Routing overlay: membership grid (CV/MIDI) + cursored dest + RUN/GATE ---
def routing(canvas_unused=None):
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, track=2, mode="MOD 3 - ROUTING")
    WindowPainter.draw_footer(canvas, ["RUN", "GATE", "CLEAR", "EVENT", "CC NUM"],
                              highlight=-1)
    _draw_dest_membership_grid(canvas, 1, 12, 119, 40,
                               cv_members={1, 3}, midi_members={3, 10},
                               cursor_kind='midi', cursor_index=3)
    from canvas import BlendMode
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Medium)
    canvas.draw_text(128, 22, "MIDI 3")
    canvas.set_color(Color.Bright)
    canvas.draw_text(128, 34, "CC 74")
    canvas.set_color(Color.Low)
    canvas.hline(128, 39, 124)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(128, 50, "RUN")
    canvas.set_color(Color.Bright)
    canvas.draw_text(150, 50, "CYCLE")
    canvas.set_color(Color.Medium)
    canvas.draw_text(192, 50, "GATE")
    canvas.set_color(Color.Bright)
    canvas.draw_text(222, 50, "IN2")
    return fb


SLUGS = {
    "sine": sine,
    "adsr": adsr,
    "adsr-page2": adsr_page2,
    "random": random,
    "chaos": chaos,
    "spring": spring,
    "justf-intone": justf_intone,
    "justf-follower": justf_follower,
    "geode-globals": geode_globals,
    "geode-voice": geode_voice,
    "routing": routing,
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("slug", nargs="?", help="single slug; omit to render all")
    parser.add_argument("-o", "--output", help="output path (single-slug mode)")
    parser.add_argument("--scale", type=int, default=4)
    args = parser.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)

    if args.slug:
        fb = SLUGS[args.slug]()
        out = args.output or os.path.join(OUT_DIR, f"{args.slug}.png")
        framebuffer_to_image(fb, scale=args.scale).save(os.path.abspath(out))
        print(out)
    else:
        for slug, fn in SLUGS.items():
            fb = fn()
            out = os.path.join(OUT_DIR, f"{slug}.png")
            framebuffer_to_image(fb, scale=args.scale).save(out)
            print(out)


if __name__ == "__main__":
    main()
