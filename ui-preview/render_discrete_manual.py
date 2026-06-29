#!/usr/bin/env python3
"""
Discrete (DMap) track OLED renders for manuals/DISCRETE_MANUAL.html.

Mirrors DiscreteMapSequencePage::draw() (src/apps/sequencer/ui/pages/
DiscreteMapSequencePage.cpp): a 32-stage threshold bar with live input cursor,
a three-row stage-info block (direction ^/v/-/x, threshold +d, note name), and
the footer. Renders the normal footer plus the INIT submenu and the two-step
GEN (generator) flow footers, which are the page's distinct macro/generate
surfaces. Reuses canvas.py / painters.py only.

Outputs to ui-preview/discrete-manual/.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Color, Font, framebuffer_to_image
from painters import WindowPainter

W, H = 256, 64
STAGE_COUNT = 32

OFF, RISE, FALL, BOTH = "off", "rise", "fall", "both"

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def _new():
    fb = FrameBuffer(W, H)
    c = Canvas(fb, brightness=1.0)
    c.set_font(Font.Tiny)
    return fb, c


def _note_name(note_index, root_note=0):
    # Chromatic mapping per .cpp drawStageInfo: volts = noteIndex/12 + rootNote/12,
    # midi = volts*12 + 12; pitch class + octave (octave = midi//12 - 1).
    midi = note_index + root_note + 12
    pc = midi % 12
    octave = (midi // 12) - 1
    return f"{NOTE_NAMES[pc]}{octave}"


class Stage:
    def __init__(self, threshold, direction, note_index):
        self.threshold = threshold
        self.direction = direction
        self.note_index = note_index


def _draw_threshold_bar(c, stages, active_stage, selection_mask,
                        current_input, range_low, range_high):
    bar_x, bar_y, bar_w = 8, 12, 240
    base_y = bar_y + 8

    c.set_color(Color.Low)
    c.hline(bar_x, base_y, bar_w)
    c.hline(bar_x, base_y + 1, bar_w)

    for i, s in enumerate(stages):
        if s.direction == OFF:
            continue
        norm = max(0.0, min(1.0, (s.threshold + 100) / 200.0))
        x = bar_x + int(norm * bar_w)
        selected = (selection_mask >> i) & 1
        active = active_stage == i
        h = 8 if active else (6 if selected else 4)
        c.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
        c.vline(x, base_y - h, h)
        c.vline(x, base_y - h, h)

    if current_input is not None:
        inorm = max(0.0, min(1.0, (current_input - range_low) / (range_high - range_low)))
        cx = bar_x + int(inorm * bar_w)
        c.set_color(Color.Bright)
        c.vline(cx, base_y - 8, 8)


def _draw_stage_info(c, stages, section, active_stage, selection_mask,
                     edit_mode, root_note=0):
    y, spacing = 30, 30
    bracket_y = (y + 16) if edit_mode == "note" else (y + 8)
    c.set_color(Color.Bright)
    c.vline(8, bracket_y - 4, 6)
    c.vline(246, bracket_y - 4, 6)

    off = section * 8
    for i in range(8):
        idx = off + i
        if idx >= STAGE_COUNT:
            break
        s = stages[idx]
        x = 8 + i * spacing + 11
        selected = (selection_mask >> idx) & 1
        active = active_stage == idx
        col = Color.Bright if active else (Color.MediumBright if selected else Color.Medium)

        # Row 1: direction
        c.set_color(col)
        dch = {RISE: "^", FALL: "v", OFF: "-", BOTH: "x"}[s.direction]
        c.draw_text(x, y, dch)

        # Row 2: threshold
        c.set_color(col)
        c.draw_text(x - 4, y + 8, f"{s.threshold:+d}")

        # Row 3: note
        if s.direction != OFF or selected:
            c.set_color(col)
            c.draw_text(x - 4, y + 16, _note_name(s.note_index, root_note))
        else:
            c.set_color(Color.Medium)
            c.draw_text(x - 4, y + 16, "--")


def render(name, stages, *, section=0, active_stage=-1, selection_mask=1,
           current_input=None, range_low=-5.0, range_high=5.0,
           edit_mode="threshold", footer=None, active_fn_text=None,
           root_note=0):
    fb, c = _new()
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, mode="DMAP")

    if active_fn_text is None:
        active_fn_text = ("DMAP: NOTE" if edit_mode == "note" else "DMAP: THR") + f" {section + 1}/4"
    WindowPainter.draw_active_function(c, active_fn_text)

    _draw_threshold_bar(c, stages, active_stage, selection_mask,
                        current_input, range_low, range_high)
    _draw_stage_info(c, stages, section, active_stage, selection_mask,
                     edit_mode, root_note)

    if footer is None:
        footer = ["SAW", "-5/+5", "POS", "ONCE", "SYNC"]
    WindowPainter.draw_footer(c, footer)

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "discrete-manual", f"{name}.png")
    framebuffer_to_image(fb).save(out)
    print("wrote", out)


def _demo_stages():
    # Eight active stages across section 0, mixing all four directions.
    data = [
        (-80, RISE, 0),    # C4
        (-55, RISE, 2),    # D4
        (-30, BOTH, 4),    # E4
        (-5,  FALL, 7),    # G4
        (20,  RISE, 9),    # A4
        (45,  OFF,  0),
        (70,  FALL, 11),   # B4
        (95,  RISE, 12),   # C5
    ]
    stages = [Stage(t, d, n) for (t, d, n) in data]
    stages += [Stage(0, OFF, 0) for _ in range(STAGE_COUNT - len(stages))]
    return stages


def main():
    stages = _demo_stages()
    # selection: stage 3 (G4) selected, stage 4 (A4) is the active/firing stage.
    sel = (1 << 3)

    render("dmap-edit", stages, section=0, active_stage=4, selection_mask=sel,
           current_input=1.0, edit_mode="threshold")

    render("dmap-edit-note", stages, section=0, active_stage=4, selection_mask=sel,
           current_input=1.0, edit_mode="note")

    # INIT submenu footer (drawFooter InitStage::ChooseTarget).
    render("dmap-init", stages, section=0, active_stage=4, selection_mask=sel,
           current_input=1.0, footer=["ALL", "THR", "NOTE", "", "X"])

    # GEN step 1: choose generator kind (GeneratorStage::ChooseKind).
    render("dmap-gen-kind", stages, section=0, active_stage=4, selection_mask=sel,
           current_input=1.0, footer=["RAND", "LIN", "LOG", "EXP", "X"])

    # GEN step 2 (RAND): apply targets (GeneratorStage::Execute, isRand).
    render("dmap-gen-rand", stages, section=0, active_stage=4, selection_mask=sel,
           current_input=1.0, footer=["ALL", "THR", "NOTE", "TOG", "X"])

    # GEN step 2 (LIN/LOG/EXP): NOTE5/NOTE2 spread (Execute, !isRand).
    render("dmap-gen-shape", stages, section=0, active_stage=4, selection_mask=sel,
           current_input=1.0, footer=["ALL", "THR", "NOTE5", "NOTE2", "X"])


if __name__ == "__main__":
    main()
