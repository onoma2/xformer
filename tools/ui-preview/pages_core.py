"""
Individual edit page renderers.
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter, SequencePainter
from tracks import (
    DiscreteMapSequence, DiscreteMapTrackEngine,
    NoteSequence, NoteTrackEngine,
    TuesdaySequence, TuesdayTrackEngine,
    StochasticSequence, StochasticTrackEngine,
    _ALGO_NAMES,
)

def render_note_sequence_edit_page(canvas: Canvas, sequence: NoteSequence,
                                    track_engine: NoteTrackEngine,
                                    layer_index: int = 0, section: int = 0,
                                    step_selection=None, show_detail: bool = False):
    """Replica of NoteSequenceEditPage::draw()."""
    if step_selection is None:
        step_selection = set()

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STEPS")
    layer_name = NOTE_LAYERS[layer_index] if layer_index < len(NOTE_LAYERS) else "GATE"
    WindowPainter.draw_active_function(canvas, layer_name)
    WindowPainter.draw_accumulator_value(canvas, 0, False)

    func_names = ["GATE", "RETRIG", "LENGTH", "NOTE", "COND"]
    active_fn = min(layer_index, 4)
    WindowPainter.draw_footer(canvas, func_names, highlight=active_fn)

    current_step = track_engine.current_step()
    step_offset = section * 16
    step_width = 16  # 256 / 16
    loop_y = 16

    # Loop start/end
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Bright)
    SequencePainter.draw_loop_start(canvas, (sequence.first_step() - step_offset) * step_width + 1, loop_y, step_width - 2)
    SequencePainter.draw_loop_end(canvas, (sequence.last_step() - step_offset) * step_width + 1, loop_y, step_width - 2)

    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = i * step_width
        y = 20

        # loop line
        if sequence.first_step() < step_index <= sequence.last_step():
            canvas.set_color(Color.Bright)
            canvas.point(x, loop_y)

        # step number
        canvas.set_color(Color.Bright if step_index in step_selection else Color.Medium)
        num = f"{step_index + 1}"
        canvas.draw_text(x + (step_width - canvas.text_width(num) + 1) // 2, y - 2, num)

        # gate rect
        canvas.set_color(Color.Bright if step_index == current_step else Color.Medium)
        canvas.draw_rect(x + 2, y + 2, step_width - 4, step_width - 4)
        if step.gate():
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x + 4, y + 4, step_width - 8, step_width - 8)

        # layer-specific info below gate
        if layer_index == 0:  # Gate
            pass
        elif layer_index == 1:  # GateProbability
            SequencePainter.draw_probability(canvas, x + 2, y + 18, step_width - 4, 2,
                                              step.gate_probability() + 1, 8)
        elif layer_index == 2:  # GateOffset
            SequencePainter.draw_offset(canvas, x + 2, y + 18, step_width - 4, 2,
                                         step.gate_offset(), -8, 8)
        elif layer_index == 3:  # Retrigger
            SequencePainter.draw_retrigger(canvas, x, y + 18, step_width, 2,
                                            step.retrigger() + 1, 4)
        elif layer_index == 4:  # RetriggerProbability
            SequencePainter.draw_probability(canvas, x + 2, y + 18, step_width - 4, 2,
                                              step.retrigger_probability() + 1, 8)
        elif layer_index == 5:  # Length
            SequencePainter.draw_length(canvas, x + 2, y + 18, step_width - 4, 6,
                                         step.length() + 1, 8)
        elif layer_index == 6:  # LengthVariationRange
            SequencePainter.draw_length_range(canvas, x + 2, y + 18, step_width - 4, 6,
                                               step.length() + 1, step.length_variation_range(), 8)
        elif layer_index == 7:  # LengthVariationProbability
            SequencePainter.draw_probability(canvas, x + 2, y + 18, step_width - 4, 2,
                                              step.length_variation_probability() + 1, 8)
        elif layer_index == 8:  # Note
            canvas.set_color(Color.Bright)
            note_str = f"{step.note():+d}"
            canvas.draw_text(x + (step_width - canvas.text_width(note_str) + 1) // 2, y + 20, note_str)
        elif layer_index == 9:  # NoteVariationRange
            canvas.set_color(Color.Bright)
            canvas.draw_text(x + (step_width - canvas.text_width(str(step.note_variation_range())) + 1) // 2, y + 20, str(step.note_variation_range()))
        elif layer_index == 10:  # NoteVariationProbability
            SequencePainter.draw_probability(canvas, x + 2, y + 18, step_width - 4, 2,
                                              step.note_variation_probability() + 1, 8)
        elif layer_index == 11:  # Slide
            SequencePainter.draw_slide(canvas, x + 4, y + 18, step_width - 8, 4, step.slide())
        elif layer_index == 12:  # AccumulatorTrigger
            canvas.set_font(Font.Tiny)
            canvas.set_color(Color.Bright)
            val = step.accumulator_step_value()
            if val == 0:
                s = "--"
            elif val == 1:
                s = "S"
            else:
                s = f"{val:+d}"
            canvas.draw_text(x + (step_width - canvas.text_width(s) + 1) // 2, y + 20, s)
        elif layer_index == 13:  # PulseCount
            canvas.set_color(Color.Bright)
            s = f"{step.pulse_count() + 1}"
            canvas.draw_text(x + (step_width - canvas.text_width(s) + 1) // 2, y + 20, s)
        elif layer_index == 14:  # GateMode
            canvas.set_color(Color.Bright)
            modes = ["A", "1", "H", "1L"]
            s = modes[step.gate_mode()] if step.gate_mode() < 4 else "A"
            canvas.draw_text(x + (step_width - canvas.text_width(s) + 1) // 2, y + 20, s)
        elif layer_index == 15:  # HarmonyRoleOverride
            canvas.set_font(Font.Tiny)
            canvas.set_color(Color.Bright)
            names = ["S", "R", "3", "5", "7", "-"]
            s = names[step.harmony_role_override()] if step.harmony_role_override() < 6 else "S"
            canvas.draw_text(x + (step_width - canvas.text_width(s) + 1) // 2, y + 20, s)
        elif layer_index == 16:  # InversionOverride
            canvas.set_font(Font.Tiny)
            canvas.set_color(Color.Bright)
            names = ["S", "R", "1", "2", "3"]
            s = names[step.inversion_override()] if step.inversion_override() < 5 else "S"
            canvas.draw_text(x + (step_width - canvas.text_width(s) + 1) // 2, y + 20, s)
        elif layer_index == 17:  # VoicingOverride
            canvas.set_font(Font.Tiny)
            canvas.set_color(Color.Bright)
            names = ["S", "C", "2", "3", "W"]
            s = names[step.voicing_override()] if step.voicing_override() < 5 else "S"
            canvas.draw_text(x + (step_width - canvas.text_width(s) + 1) // 2, y + 20, s)
        elif layer_index == 18:  # Condition
            canvas.set_color(Color.Bright)
            cond = step.condition()
            labels = ["--", "F1", "F2", "F3", "F4", "!F1", "!F2", "!F3", "!F4",
                      "P1", "P2", "P3", "P4", "!P1", "!P2", "!P3", "!P4"]
            s = labels[cond] if cond < len(labels) else "--"
            canvas.draw_text(x + (step_width - canvas.text_width(s) + 1) // 2, y + 20, s)
            s2 = f"{cond}"
            canvas.draw_text(x + (step_width - canvas.text_width(s2) + 1) // 2, y + 27, s2)

    # Detail box (simplified)
    if show_detail and len(step_selection) > 0:
        first_step = min(step_selection)
        step = sequence.step(first_step)
        WindowPainter.draw_frame(canvas, 64, 16, 128, 32)
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Bright)
        canvas.vline(64 + 32, 16, 32)
        canvas.set_font(Font.Small)
        detail = f"{first_step + 1}"
        if len(step_selection) > 1:
            detail += "*"
        canvas.draw_text_centered(64, 16, 32, 32, detail)
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Bright)
        canvas.draw_text_centered(64 + 32, 16, 96, 32, layer_name)


# =============================================================================
# DiscreteMap Sequence Page
# =============================================================================

def render_discrete_map_sequence_page(canvas: Canvas, sequence: DiscreteMapSequence,
                                       track_engine: DiscreteMapTrackEngine,
                                       section: int = 0, edit_mode: str = "threshold",
                                       selection_mask: int = 1, selected_stage: int = 0):
    """Replica of DiscreteMapSequencePage::draw()."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="DMAP")
    mode_str = "DMAP"
    if edit_mode == "note":
        mode_str += ": NOTE"
    else:
        mode_str += ": THR"
    mode_str += f" {section + 1}/4"
    WindowPainter.draw_active_function(canvas, mode_str)

    # Footer labels (simplified)
    fn_labels = ["SAW", "-5/+5", "POS", "ONCE", "SYNC"]
    WindowPainter.draw_footer(canvas, fn_labels)

    # Threshold bar
    bar_x = 8
    bar_y = 12
    bar_w = 240
    bar_line_y = bar_y + 8

    canvas.set_color(Color.Low)
    canvas.hline(bar_x, bar_line_y, bar_w)
    canvas.hline(bar_x, bar_line_y + 1, bar_w)

    for i in range(DiscreteMapSequence.StageCount):
        stage = sequence.stage(i)
        if stage.direction() == DiscreteMapSequence.Stage.TriggerDir.Off:
            continue
        norm = (stage.threshold() + 100.0) / 200.0
        norm = max(0.0, min(1.0, norm))
        x = bar_x + int(norm * bar_w)
        selected = (selection_mask & (1 << i)) != 0
        active = track_engine.active_stage() == i
        marker_height = 8 if active else (6 if selected else 4)
        canvas.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
        canvas.vline(x, bar_line_y - marker_height, marker_height)
        canvas.vline(x + 1, bar_line_y - marker_height, marker_height)

    # Input cursor
    range_min = sequence.range_low()
    range_max = sequence.range_high()
    input_norm = (track_engine.current_input() - range_min) / (range_max - range_min)
    input_norm = max(0.0, min(1.0, input_norm))
    cursor_x = bar_x + int(input_norm * bar_w)
    canvas.set_color(Color.Bright)
    canvas.vline(cursor_x, bar_line_y - 8, 8)

    # Stage info grid
    y = 30
    spacing = 30
    bracket_y = y + 16 if edit_mode == "note" else y + 8
    canvas.set_color(Color.Bright)
    canvas.vline(8, bracket_y - 4, 6)
    canvas.vline(246, bracket_y - 4, 6)

    step_offset = section * 8
    for i in range(8):
        stage_index = step_offset + i
        if stage_index >= DiscreteMapSequence.StageCount:
            break
        stage = sequence.stage(stage_index)
        x = 8 + i * spacing + 11
        selected = (selection_mask & (1 << stage_index)) != 0
        active = track_engine.active_stage() == stage_index

        # Direction
        canvas.set_color(Color.Bright if active else (Color.MediumBright if selected else Color.Medium))
        dir_chars = {0: '^', 1: 'v', 2: '-', 3: 'x'}
        dir_char = dir_chars.get(stage.direction(), '-')
        canvas.draw_text(x, y, dir_char)

        # Threshold
        canvas.set_color(Color.Bright if active else (Color.MediumBright if selected else Color.Medium))
        thresh = f"{stage.threshold():+d}"
        canvas.draw_text(x - 4, y + 8, thresh)

        # Note (placeholder)
        if stage.direction() != DiscreteMapSequence.Stage.TriggerDir.Off or selected:
            canvas.set_color(Color.Bright if active else (Color.MediumBright if selected else Color.Medium))
            canvas.draw_text(x - 4, y + 16, "C4")
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_text(x - 4, y + 16, "--")


# =============================================================================
# Tuesday Edit Page
# =============================================================================
TUESDAY_PARAMS = [
    "Algorithm", "Flow", "Ornament", "Power",
    "LoopLength", "Rotate", "Glide", "Skew",
    "GateLength", "GateOffset", "Trill", "Start",
]

TUESDAY_SHORT_NAMES = [
    "ALGO", "FLOW", "ORN", "POWER",
    "LOOP", "ROT", "GLIDE", "SKEW",
    "GATE", "GOFS", "TRILL", "START",
]

TUESDAY_MAX_VALUES = [14, 16, 16, 16, 29, 63, 100, 8, 100, 100, 100, 16]
TUESDAY_BIPOLAR = [False, False, False, False, False, True, False, True, False, False, False, False]



def render_tuesday_edit_page(canvas: Canvas, sequence: TuesdaySequence,
                              track_engine: TuesdayTrackEngine,
                              current_page: int = 0, selected_slot: int = 0):
    """Replica of TuesdayEditPage::draw()."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STEPS")

    col_width = 51
    params_per_page = 4
    page_params = [
        [0, 1, 2, 3],    # Page 0: Algorithm, Flow, Ornament, Power
        [4, 5, 6, 7],    # Page 1: LoopLength, Rotate, Glide, Skew
        [8, 9, 10, 11],  # Page 2: GateLength, GateOffset, Trill, Start
    ]

    # Draw 4 parameter columns
    for slot in range(params_per_page):
        param_idx = page_params[current_page][slot] if current_page < len(page_params) else -1
        if param_idx < 0:
            continue
        x = slot * col_width
        param_name = TUESDAY_PARAMS[param_idx]
        short_name = TUESDAY_SHORT_NAMES[param_idx]
        max_val = TUESDAY_MAX_VALUES[param_idx]
        bipolar = TUESDAY_BIPOLAR[param_idx]

        # Get value
        values = [
            sequence.algorithm(), sequence.flow(), sequence.ornament(), sequence.power(),
            sequence.loop_length(), sequence.rotate(), sequence.glide(), sequence.skew(),
            sequence.gate_length(), sequence.gate_offset(), sequence.step_trill(), sequence.start(),
        ]
        value = values[param_idx]

        canvas.set_font(Font.Tiny)
        canvas.set_blend_mode(BlendMode.Set)

        if param_idx == 0:  # Algorithm: show name
            algo_name = _ALGO_NAMES[value] if value < len(_ALGO_NAMES) else "???"
            canvas.set_color(Color.Bright if selected_slot == slot else Color.Medium)
            tw = canvas.text_width(algo_name)
            canvas.draw_text(x + (col_width - tw) // 2, 35, algo_name)
        else:
            # Numeric value
            if param_idx == 4 and value == 0:
                val_str = "INF"
            elif param_idx == 5 and sequence.loop_length() == 0:
                val_str = "N/A"
            elif bipolar:
                val_str = f"{value:+d}"
            elif param_idx in (6, 8, 9, 10):
                val_str = f"{value}%"
            else:
                val_str = f"{value}"
            canvas.set_color(Color.Bright if selected_slot == slot else Color.Medium)
            tw = canvas.text_width(val_str)
            canvas.draw_text(x + (col_width - tw) // 2, 26, val_str)

            # Bar
            bar_y = 32
            bar_h = 4
            bar_w = 40
            bar_x = x + (col_width - bar_w) // 2
            if not (param_idx == 5 and sequence.loop_length() == 0):
                canvas.set_blend_mode(BlendMode.Set)
                if bipolar:
                    center = bar_x + bar_w // 2
                    if value > 0:
                        fill_w = (value * bar_w // 2) // max_val
                        canvas.set_color(Color.Bright)
                        canvas.fill_rect(center, bar_y, fill_w, bar_h)
                    elif value < 0:
                        fill_w = (-value * bar_w // 2) // max_val
                        canvas.set_color(Color.Bright)
                        canvas.fill_rect(center - fill_w, bar_y, fill_w, bar_h)
                    canvas.set_color(Color.Medium)
                    canvas.vline(center, bar_y, bar_h)
                else:
                    fill_w = (value * bar_w) // max_val
                    if fill_w > 0:
                        canvas.set_color(Color.Bright)
                        canvas.fill_rect(bar_x, bar_y, fill_w, bar_h)

    # Status box
    box_x = 204
    box_y = 14
    box_w = 48
    box_h = 30
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.draw_rect(box_x, box_y, box_w, box_h)
    canvas.set_font(Font.Tiny)

    cv = 0.0  # stub
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    canvas.set_color(Color.Bright)
    canvas.draw_text(box_x + 4, box_y + 7, "C4")
    gate = track_engine.gate_output(0)
    gate_x = box_x + box_w - 10
    if gate:
        canvas.fill_rect(gate_x, box_y + 3, 5, 5)
    else:
        canvas.draw_rect(gate_x, box_y + 3, 5, 5)
    canvas.set_color(Color.Medium)
    canvas.draw_text(box_x + 4, box_y + 15, "0.00V")
    current_step = track_engine.current_step()
    loop_len = sequence.actual_loop_length()
    if loop_len == 0:
        step_str = f"{current_step + 1}"
    else:
        step_str = f"{(current_step % loop_len) + 1}/{loop_len}"
    canvas.draw_text(box_x + 4, box_y + 23, step_str)

    # Page indicator
    canvas.set_color(Color.Medium)
    page_str = f"[{current_page + 1}/{len(page_params)}]"
    tw = canvas.text_width(page_str)
    canvas.draw_text(204 + (52 - tw) // 2, 50, page_str)

    # Algorithm indicator
    algo_str = f"[{sequence.algorithm()}]"
    tw = canvas.text_width(algo_str)
    canvas.draw_text((51 - tw) // 2, 50, algo_str)

    # Footer
    fn_names = ["", "", "", "", "NEXT"]
    for i in range(4):
        param_idx = page_params[current_page][i] if current_page < len(page_params) else -1
        if param_idx >= 0:
            fn_names[i] = TUESDAY_SHORT_NAMES[param_idx]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)


# =============================================================================
# Stochastic Edit Page  (DESIGN ITERATION AREA)
# =============================================================================

def render_stochastic_edit_page(canvas: Canvas, sequence: StochasticSequence,
                                 track_engine: StochasticTrackEngine):
    """
    Placeholder for the Stochastic track edit page.

    The current implementation shows a simple 16-step grid similar to NoteEdit,
    but with probability shading. Use this as a starting canvas for your design.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "EDIT")
    WindowPainter.draw_footer(canvas, ["TICKS", "DEGREE", "PROB", "MODE", "NEXT"])

    step_width = 16
    y = 20
    current_step = track_engine.current_step()

    for i in range(16):
        step_index = i
        step = sequence.step(step_index)
        x = i * step_width

        # step number
        canvas.set_color(Color.Medium)
        num = f"{step_index + 1}"
        canvas.draw_text(x + (step_width - canvas.text_width(num) + 1) // 2, y - 2, num)

        # gate rect with probability shading
        prob = step.gate_probability()
        if step_index == current_step:
            canvas.set_color(Color.Bright)
        else:
            canvas.set_color(Color.Medium)
        canvas.draw_rect(x + 2, y + 2, step_width - 4, step_width - 4)

        if step.gate():
            if prob > 75:
                canvas.set_color(Color.Bright)
            elif prob > 50:
                canvas.set_color(Color.MediumBright)
            elif prob > 25:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)
            canvas.fill_rect(x + 4, y + 4, step_width - 8, step_width - 8)

        # probability text
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Bright)
        pstr = f"{prob}"
        canvas.draw_text(x + (step_width - canvas.text_width(pstr) + 1) // 2, y + 20, pstr)


# =============================================================================
# Reference Screen Renderers
# =============================================================================


def render_stochastic_steps(canvas: Canvas, sequence, track_engine,
                            layer_bank: int = 0, selected_step: int = 3,
                            step_selection=None, section: int = 0):
    """
    StochasticSequenceEditPage — primary step grid.
    16-step section grid with probability bars.
    """
    if step_selection is None:
        step_selection = {selected_step}

    layer_names = ["GATE", "NOTE", "LEN", "RTRG"]
    prob_names = ["GPROB", "NPROB", "LPROB", "RPROB"]

    if layer_bank == 0:
        active_layer = layer_names
        active_fn = "GATE"
    elif layer_bank == 1:
        active_layer = prob_names
        active_fn = "GPROB"
    else:
        active_layer = layer_names
        active_fn = "GATE"

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"STEPS {section+1}/4")
    WindowPainter.draw_footer(canvas, active_layer + ["NEXT"], highlight=0)

    step_offset = section * 16
    step_width = 16
    current_step = track_engine.current_step()
    y = 20
    loop_y = 16

    # Loop markers
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Bright)
    SequencePainter.draw_loop_start(canvas, (sequence.first_step() - step_offset) * step_width + 1, loop_y, step_width - 2)
    SequencePainter.draw_loop_end(canvas, (sequence.last_step() - step_offset) * step_width + 1, loop_y, step_width - 2)

    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = i * step_width

        # loop line
        if sequence.first_step() < step_index <= sequence.last_step():
            canvas.set_color(Color.Bright)
            canvas.point(x, loop_y)

        # step number
        sel = step_index in step_selection
        canvas.set_color(Color.Bright if sel else Color.Medium)
        num = f"{step_index + 1}"
        canvas.draw_text(x + (step_width - canvas.text_width(num) + 1) // 2, y - 2, num)

        # gate rect
        is_current = (step_index == current_step)
        canvas.set_color(Color.Bright if is_current else Color.Medium)
        canvas.draw_rect(x + 2, y + 2, step_width - 4, step_width - 4)
        if step.gate():
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x + 4, y + 4, step_width - 8, step_width - 8)

        # mini probability bars under each step
        bar_y = y + 18
        bar_h = 2
        bar_w = step_width - 4

        # Gate probability (always shown faintly)
        gp = step.gate_probability()
        canvas.set_color(Color.Low)
        canvas.fill_rect(x + 2, bar_y, bar_w, bar_h)
        if gp > 0:
            pw = (bar_w * gp) // 100
            canvas.set_color(Color.Medium if gp < 100 else Color.Bright)
            canvas.fill_rect(x + 2, bar_y, pw, bar_h)

        # Note probability bar below gate prob
        np = step.note_probability()
        if np > 0:
            pw = (bar_w * np) // 100
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(x + 2, bar_y + 3, pw, bar_h)

        # Note value text (if gate is on)
        if step.gate():
            note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
            nn = note_names[step.note() % 12]
            canvas.set_color(Color.Bright)
            canvas.draw_text(x + (step_width - canvas.text_width(nn) + 1) // 2, y + 24, nn)



def render_stochastic_dice(canvas: Canvas, sequence, track_engine,
                           axis: int = 0, selected_steps=None, section: int = 0):
    """
    StochasticDicePage — 64-step probability matrix.
    4 rows of 16 compressed cells.
    """
    if selected_steps is None:
        selected_steps = set()

    axes = ["GATE", "NOTE", "OCT", "LEN", "RTRG", "REST"]
    axis_name = axes[axis] if axis < len(axes) else "GATE"

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"DICE {axis_name}")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "OCT", "LEN", "NEXT"], highlight=axis % 5)

    cell_w = 16
    cell_h = 10
    rows = 4
    cols = 16
    current_step = track_engine.current_step()

    canvas.set_blend_mode(BlendMode.Set)
    for row in range(rows):
        for col in range(cols):
            step_index = row * 16 + col
            step = sequence.step(step_index)
            x = col * cell_w
            y = 14 + row * cell_h

            # Get probability for current axis
            if axis == 0:
                prob = step.gate_probability()
            elif axis == 1:
                prob = step.note_probability()
            elif axis == 2:
                prob = 100  # octave probability placeholder
            elif axis == 3:
                prob = step.length_probability()
            elif axis == 4:
                prob = step.retrigger_probability()
            else:
                prob = 0

            is_current = (step_index == current_step)
            is_selected = step_index in selected_steps

            # Cell brightness by probability
            if is_current:
                canvas.set_color(Color.Bright)
                canvas.draw_rect(x + 1, y + 1, cell_w - 2, cell_h - 2)
            elif is_selected:
                canvas.set_color(Color.MediumBright)
                canvas.draw_rect(x + 1, y + 1, cell_w - 2, cell_h - 2)
            else:
                # Fill level by probability
                if prob >= 75:
                    canvas.set_color(Color.MediumBright)
                elif prob >= 50:
                    canvas.set_color(Color.Medium)
                elif prob >= 25:
                    canvas.set_color(Color.Low)
                else:
                    canvas.set_color(Color.None_)
                canvas.fill_rect(x + 2, y + 2, cell_w - 4, cell_h - 4)

            # Probability text for selected/current
            if is_current or is_selected:
                canvas.set_color(Color.Bright)
                pstr = f"{prob}"
                canvas.draw_text(x + (cell_w - canvas.text_width(pstr)) // 2, y + 3, pstr)



def render_stochastic_pitch(canvas: Canvas, sequence, track_engine,
                            slot: int = 0, selected_degree: int = 2):
    """
    StochasticPitchPage — scale-degree ticket bars.
    One vertical bar per active scale degree.
    """
    slots = ["TIX", "DEG", "MASK", "LIN"]
    slot_name = slots[slot] if slot < len(slots) else "TIX"

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"PITCH {slot_name}")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "MASK", "LIN", "NEXT"], highlight=slot)

    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    bar_w = max(4, 240 // max(1, scale_size))
    bar_max_h = 30
    bar_base_y = 40
    bar_x0 = 8

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(scale_size):
        x = bar_x0 + i * bar_w
        t = tickets[i] if i < len(tickets) else 0

        selected = (i == selected_degree)

        if t < 0:
            # Excluded: X marker
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.draw_text(x + 1, bar_base_y - 8, "X")
        elif t == 0:
            # Zero tickets: hollow bar
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.draw_rect(x + 1, bar_base_y - bar_max_h, bar_w - 2, bar_max_h)
        else:
            # Filled bar proportional to tickets
            h = min(bar_max_h, t * 3)
            canvas.set_color(Color.Bright if selected else Color.MediumBright)
            canvas.fill_rect(x + 1, bar_base_y - h, bar_w - 2, h)

        # Degree number
        canvas.set_color(Color.Bright if selected else Color.Medium)
        num = f"{i+1}"
        canvas.draw_text(x + (bar_w - canvas.text_width(num)) // 2, bar_base_y + 2, num)

    # Range fences
    min_d = sequence.min_degree()
    max_d = sequence.max_degree()
    canvas.set_color(Color.Bright)
    if 0 <= min_d < scale_size:
        fx = bar_x0 + min_d * bar_w
        canvas.vline(fx, bar_base_y - bar_max_h - 2, bar_max_h + 4)
    if 0 <= max_d < scale_size:
        fx = bar_x0 + max_d * bar_w + bar_w - 1
        canvas.vline(fx, bar_base_y - bar_max_h - 2, bar_max_h + 4)

    # Current generated pitch cursor
    current_note = track_engine.current_note()
    canvas.set_color(Color.Bright)
    canvas.draw_text(220, 20, f"N{current_note}")

    # Rotation values
    canvas.set_color(Color.Medium)
    canvas.draw_text(200, 36, f"D{sequence.degree_rotation():+d}")
    canvas.draw_text(200, 44, f"M{sequence.mask_rotation():+d}")



def render_stochastic_marbles(canvas: Canvas, sequence, track_engine,
                              slot: int = 0):
    """
    StochasticMarblesPage — distribution shape.
    Histogram preview + bias/spread/steps controls.
    """
    slots = ["SPRD", "BIAS", "STEP", "MODE"]
    slot_name = slots[slot] if slot < len(slots) else "SPRD"
    modes = ["OFF", "SOFT", "HARD"]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"SHAPE {slot_name}")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)

    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()
    mode = sequence.marbles_mode()

    # Distribution histogram (left side)
    hist_x = 8
    hist_y = 40
    hist_h = 28
    hist_bins = 12
    bin_w = 14

    canvas.set_blend_mode(BlendMode.Set)

    # Generate fake distribution from spread/bias/steps
    import math
    for i in range(hist_bins):
        x = hist_x + i * bin_w
        # Gaussian-ish centered on bias
        dist = abs(i - (hist_bins // 2 + bias // 10))
        envelope = math.exp(-(dist ** 2) / (spread / 10 + 1))
        h = int(hist_h * envelope * (steps / 8))
        h = max(2, min(hist_h, h))
        canvas.set_color(Color.Medium)
        canvas.fill_rect(x, hist_y - h, bin_w - 2, h)

    # Bias marker line
    bias_x = hist_x + (hist_bins // 2 + bias // 10) * bin_w + bin_w // 2
    canvas.set_color(Color.Bright)
    canvas.vline(bias_x, hist_y - hist_h - 2, hist_h + 4)

    # Live status box (right side, Tuesday-style)
    box_x = 180
    box_y = 14
    box_w = 68
    box_h = 36
    canvas.set_color(Color.Medium)
    canvas.draw_rect(box_x, box_y, box_w, box_h)

    canvas.set_color(Color.Bright)
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    nn = note_names[track_engine.current_note() % 12]
    canvas.draw_text(box_x + 4, box_y + 7, nn)
    canvas.draw_text(box_x + 4, box_y + 15, f"S{spread}")
    canvas.draw_text(box_x + 4, box_y + 23, f"B{bias:+d}")

    # Mode text
    canvas.set_color(Color.Bright)
    canvas.draw_text(box_x + 40, box_y + 23, modes[mode] if mode < len(modes) else "OFF")

    # Parameter values as horizontal bar meters
    meter_y = 18
    params = [("SPRD", spread, 100), ("BIAS", bias + 50, 100), ("STEP", steps, 16)]
    for i, (name, val, maxv) in enumerate(params):
        y = meter_y + i * 8
        canvas.set_color(Color.Medium)
        canvas.draw_text(8, y + 5, name)
        # Bar track
        canvas.hline(40, y + 2, 60)
        fill_w = (val * 60) // maxv
        canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
        canvas.fill_rect(40, y, fill_w, 5)



def render_stochastic_lock(canvas: Canvas, sequence, track_engine,
                           slot: int = 0, selected_event: int = -1):
    """
    StochasticLockPage — captured loop surface.
    4 rows of 16 event cells.
    """
    slots = ["LOCK", "FIRST", "LAST", "ROT"]
    slot_name = slots[slot] if slot < len(slots) else "LOCK"

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"LOCK {slot_name}")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"], highlight=slot)

    cell_w = 16
    cell_h = 10
    rows = 4
    cols = 16
    current_step = track_engine.current_step()
    locked = sequence.lock_enabled()

    canvas.set_blend_mode(BlendMode.Set)
    for row in range(rows):
        for col in range(cols):
            event_index = row * 16 + col
            evt = sequence.lock_event(event_index)
            x = col * cell_w
            y = 14 + row * cell_h

            is_current = (event_index == current_step)
            is_selected = (event_index == selected_event)
            in_window = sequence.lock_first() <= event_index <= sequence.lock_last()

            # Cell frame
            if locked and in_window:
                canvas.set_color(Color.Bright if is_current else Color.MediumBright)
            else:
                canvas.set_color(Color.Medium if is_current else Color.Low)
            canvas.draw_rect(x + 1, y + 1, cell_w - 2, cell_h - 2)

            # Fill if gate on
            if evt.gate():
                if locked and in_window:
                    canvas.set_color(Color.Bright)
                else:
                    canvas.set_color(Color.Medium)
                canvas.fill_rect(x + 2, y + 2, cell_w - 4, cell_h - 4)

            # Note pitch indicator (tiny vertical bar height)
            if evt.gate():
                note_h = (evt.note() % 7) + 1
                canvas.set_color(Color.Bright if is_current else Color.MediumBright)
                canvas.vline(x + cell_w - 3, y + cell_h - 2 - note_h, note_h)

            # Retrigger marker
            if evt.retrigger() > 0:
                canvas.set_color(Color.Bright)
                canvas.point(x + 2, y + 2)

    # Window fences
    first = sequence.lock_first()
    last = sequence.lock_last()
    canvas.set_color(Color.Bright)
    fx = (first % 16) * cell_w
    fy = 14 + (first // 16) * cell_h
    canvas.draw_rect(fx, fy - 1, cell_w, cell_h + 2)
    lx = (last % 16) * cell_w
    ly = 14 + (last // 16) * cell_h
    canvas.draw_rect(lx, ly - 1, cell_w, cell_h + 2)

    # Rotation offset indicator
    rot = sequence.lock_rotate()
    if rot > 0:
        rx = (rot % 16) * cell_w + cell_w // 2
        ry = 12
        canvas.set_color(Color.Bright)
        canvas.point(rx, ry)
        canvas.point(rx - 1, ry)
        canvas.point(rx + 1, ry)



def render_stochastic_track(canvas: Canvas, sequence, track_engine,
                            current_page: int = 0, selected_slot: int = 0):
    """
    StochasticTrackPage — compact track console (Tuesday-style).
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
        [("ACC", "accent"), ("LEG", "legato"), ("CV", "cv_mode"), ("FILL", "fill")],
        [("LOWO", "low_octave"), ("HIO", "high_octave"), ("REST", "rest_prob"), ("LENM", "len_mod")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK P{current_page+1}")

    col_width = 51
    canvas.set_blend_mode(BlendMode.Set)

    for slot in range(4):
        name, attr = page[slot]
        x = slot * col_width
        value = getattr(sequence, attr)()

        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Bright if selected_slot == slot else Color.Medium)
        tw = canvas.text_width(name)
        canvas.draw_text(x + (col_width - tw) // 2, 20, name)

        # Value text
        if attr in ("division", "reset_mode", "cv_mode"):
            val_str = f"{value}"
        elif attr in ("gbias", "nbias", "lbias", "rbias", "transpose"):
            val_str = f"{value:+d}"
        elif attr in ("accent", "legato", "fill", "rest_prob"):
            val_str = f"{value}%"
        else:
            val_str = f"{value}"

        canvas.set_color(Color.Bright if selected_slot == slot else Color.Medium)
        tw = canvas.text_width(val_str)
        canvas.draw_text(x + (col_width - tw) // 2, 28, val_str)

        # Bar
        bar_y = 36
        bar_h = 4
        bar_w = 40
        bar_x = x + (col_width - bar_w) // 2
        max_val = 100 if "%" in val_str else (16 if "mode" in attr or "division" in attr else 12)
        fill_w = min(bar_w, abs(value) * bar_w // max_val) if max_val > 0 else 0
        if fill_w > 0:
            canvas.set_color(Color.Bright if selected_slot == slot else Color.MediumBright)
            canvas.fill_rect(bar_x, bar_y, fill_w, bar_h)

    # Status box
    box_x = 204
    box_y = 14
    box_w = 48
    box_h = 30
    canvas.set_color(Color.Medium)
    canvas.draw_rect(box_x, box_y, box_w, box_h)

    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    nn = note_names[track_engine.current_note() % 12]
    canvas.set_color(Color.Bright)
    canvas.draw_text(box_x + 4, box_y + 7, nn)
    gate = track_engine.gate_output()
    gate_x = box_x + box_w - 10
    if gate:
        canvas.fill_rect(gate_x, box_y + 3, 5, 5)
    else:
        canvas.draw_rect(gate_x, box_y + 3, 5, 5)
    canvas.set_color(Color.Medium)
    canvas.draw_text(box_x + 4, box_y + 15, f"S{track_engine.current_step()+1}")
    locked = "L" if sequence.lock_enabled() else "-"
    canvas.draw_text(box_x + 4, box_y + 23, locked)

    # Page indicator
    page_str = f"[{current_page+1}/{len(param_pages)}]"
    canvas.set_color(Color.Medium)
    tw = canvas.text_width(page_str)
    canvas.draw_text(204 + (52 - tw) // 2, 50, page_str)

    # Footer
    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)


# =============================================================================
# Norns-Inspired Hybrid Pages (Second Wave)
# =============================================================================

# ---------------------------------------------------------------------------
# Steps variants
# ---------------------------------------------------------------------------


