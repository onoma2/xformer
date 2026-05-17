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


# =============================================================================
# Note Sequence Edit Page
# =============================================================================
NOTE_LAYERS = [
    "GATE", "GATEPROB", "GATEOFFSET", "RETRIG", "RETRIGPROB",
    "LENGTH", "LENGTHRNG", "LENGTHPROB", "NOTE", "NOTERNG",
    "NOTEPROB", "SLIDE", "ACCUM", "PULSES", "GATEMODE",
    "HARMONY", "INVERSION", "VOICING", "COND",
]


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

def render_ref_prob_melod(canvas: Canvas, weights=None, active_note=4, active_octave=0):
    """
    ProbabilityMelody reference screen.
    12 semitone weight bars + piano keyboard.
    weights: list of 12 ints (-1..10). -1 = excluded (X), 0..10 = ticket height.
    """
    if weights is None:
        weights = [2, 0, 5, 0, 8, 3, 0, 7, 0, 4, 0, 1]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="PROBMELO")
    WindowPainter.draw_footer(canvas, ["W1", "W2", "W3", "W4", "NEXT"])

    # 12 semitone x-positions (scaled from O_C 128px to our 256px)
    semitone_x = [int(x * 2) for x in [6, 14, 22, 30, 38, 46, 54, 62, 70, 78, 86, 94]]
    # Black key pattern: indices 1,3,6,8,10 are black keys
    black_keys = {1, 3, 6, 8, 10}

    bar_base_y = 22
    bar_max_h = 20

    canvas.set_blend_mode(BlendMode.Set)
    for i, w in enumerate(weights):
        x = semitone_x[i]
        # dotted vertical line
        canvas.set_color(Color.Medium)
        for y in range(bar_base_y, bar_base_y + bar_max_h, 2):
            canvas.point(x, y)

        if w < 0:
            # excluded: X marker
            canvas.set_color(Color.Bright)
            canvas.draw_text(x - 2, bar_base_y + bar_max_h - 6, "X")
        elif w > 0:
            # horizontal tick at weight height
            tick_y = bar_base_y + bar_max_h - (w * bar_max_h // 10)
            canvas.set_color(Color.Bright)
            canvas.hline(x - 2, tick_y, 5)
        else:
            # zero weight: no tick (hollow)
            pass

    # Keyboard at bottom
    key_y = 46
    white_key_w = 10
    black_key_w = 6
    key_h = 10
    # White keys (0,2,4,5,7,9,11)
    white_indices = [0, 2, 4, 5, 7, 9, 11]
    for idx in white_indices:
        x = semitone_x[idx] - white_key_w // 2
        canvas.set_color(Color.Medium)
        canvas.draw_rect(x, key_y, white_key_w, key_h)
    # Black keys
    for idx in black_keys:
        x = semitone_x[idx] - black_key_w // 2
        canvas.set_color(Color.Bright)
        canvas.fill_rect(x, key_y, black_key_w, key_h - 3)

    # Active note triangles (simplified as text indicator)
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    canvas.set_color(Color.Bright)
    canvas.draw_text(semitone_x[active_note] - 3, key_y + key_h + 2, note_names[active_note])
    canvas.draw_text(220, key_y + key_h + 2, f"O{active_octave}")


def render_ref_shredder(canvas: Canvas, grid_values=None, active_cell=5):
    """
    Shredder reference screen.
    4x4 Cartesian grid (5x5px frames, 8px spacing) with crosshairs.
    """
    if grid_values is None:
        grid_values = [i % 7 for i in range(16)]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="SHREDDER")
    WindowPainter.draw_footer(canvas, ["RANGE", "QUANT", "BIPOL", "RATE", "NEXT"])

    grid_x0 = 80
    grid_y0 = 20
    cell_size = 5
    cell_spacing = 8

    canvas.set_blend_mode(BlendMode.Set)
    # Draw grid frames
    for i in range(16):
        cx = grid_x0 + (i % 4) * cell_spacing
        cy = grid_y0 + (i // 4) * cell_spacing
        canvas.set_color(Color.Medium)
        canvas.draw_rect(cx, cy, cell_size, cell_size)

    # Active cell filled
    ax = grid_x0 + (active_cell % 4) * cell_spacing
    ay = grid_y0 + (active_cell // 4) * cell_spacing
    canvas.set_color(Color.Bright)
    canvas.fill_rect(ax, ay, cell_size, cell_size)

    # Crosshair lines through active row/col
    canvas.set_color(Color.Bright)
    # horizontal dotted line through active row
    row_y = ay + cell_size // 2
    for x in range(grid_x0, grid_x0 + 4 * cell_spacing, 2):
        canvas.point(x, row_y)
    # vertical dotted line through active col
    col_x = ax + cell_size // 2
    for y in range(grid_y0, grid_y0 + 4 * cell_spacing, 2):
        canvas.point(col_x, y)

    # Bar meters on right side (simplified as 4 vertical bars)
    meter_x = 140
    for i in range(4):
        val = grid_values[i]
        h = abs(val) * 2
        my = 40 - h if val >= 0 else 40
        canvas.set_color(Color.Bright if i == active_cell % 4 else Color.Medium)
        canvas.fill_rect(meter_x + i * 8, my, 5, h)
    # center zero line
    canvas.set_color(Color.Low)
    canvas.hline(meter_x, 40, 32)


def render_ref_euclid(canvas: Canvas, steps=16, hits=5, rotation=0, current_step=3):
    """
    Euclidean ring reference screen (squares-and-circles derived).
    Steps arranged on a 21px radius circle.
    Filled rect = active/deterministic, hollow circle = inactive/probabilistic.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="EUCLID")
    WindowPainter.draw_footer(canvas, ["LEN", "HITS", "ROT", "SWING", "NEXT"])

    import math
    cx = 60
    cy = 34
    radius = 21

    # Compute euclidean pattern
    def euclidean(n, k, r):
        pattern = [0] * n
        if k <= 0:
            return pattern
        idx = 0
        for _ in range(k):
            pattern[idx % n] = 1
            idx += n // k if n // k > 0 else 1
        # Rotate
        r = r % n
        return pattern[-r:] + pattern[:-r]

    pattern = euclidean(steps, hits, rotation)

    canvas.set_blend_mode(BlendMode.Set)

    # Outer guide circle
    if steps >= 7:
        canvas.set_color(Color.Medium)
        # Approximate circle with points
        for angle in range(0, 360, 5):
            rad = math.radians(angle)
            px = int(cx + radius * math.cos(rad))
            py = int(cy + radius * math.sin(rad))
            canvas.point(px, py)

    # Step dots
    for i in range(steps):
        angle = math.radians(i * 360 / steps - 90)
        px = int(cx + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))

        active = pattern[i]
        is_current = (i == current_step)
        is_rotation = (i == 0)

        if is_current:
            # Playhead: 5x5 hollow rect
            canvas.set_color(Color.Bright)
            canvas.draw_rect(px - 2, py - 2, 5, 5)
        elif is_rotation:
            # Rotation offset: 7x7 hollow rect
            canvas.set_color(Color.MediumBright)
            canvas.draw_rect(px - 3, py - 3, 7, 7)

        if active:
            # Filled 3x3 rect = deterministic
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        else:
            # Hollow 3px circle = probabilistic
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px - 1, py - 1, 3, 3)

    # Center info
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 6, cy - 2, f"{hits}")

    # Second ring (side-by-side, like EuclidRythm.cpp)
    cx2 = 180
    pattern2 = euclidean(steps, hits + 2, rotation + 1)
    if steps >= 7:
        canvas.set_color(Color.Medium)
        for angle in range(0, 360, 5):
            rad = math.radians(angle)
            px = int(cx2 + radius * math.cos(rad))
            py = int(cy + radius * math.sin(rad))
            canvas.point(px, py)
    for i in range(steps):
        angle = math.radians(i * 360 / steps - 90)
        px = int(cx2 + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        active = pattern2[i]
        is_current = (i == (current_step + 2) % steps)
        if is_current:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(px - 2, py - 2, 5, 5)
        if active:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px - 1, py - 1, 3, 3)


def render_ref_delinquencer(canvas: Canvas, probs=None, current_step=5, selected_cell=12):
    """
    delinquencer reference screen.
    8x8 binary sequencer grid (5x5px cells, 7px spacing).
    Probability shown as erased corner pixels.
    """
    if probs is None:
        probs = [100, 80, 60, 40, 20, 0, 100, 50,
                 75, 25, 100, 0, 50, 50, 100, 30,
                 100, 100, 0, 80, 60, 40, 20, 10,
                 50, 50, 50, 100, 100, 0, 75, 25,
                 100, 0, 100, 50, 25, 75, 100, 0,
                 60, 40, 80, 20, 100, 100, 50, 50,
                 0, 100, 25, 75, 50, 50, 100, 0,
                 100, 100, 100, 0, 50, 75, 25, 100]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="DELINQ")
    WindowPainter.draw_footer(canvas, ["PROB", "X", "Y", "PAT", "NEXT"])

    grid_x0 = 16
    grid_y0 = 16
    cell_size = 5
    cell_spacing = 7

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(64):
        col = i % 8
        row = i // 8
        x = grid_x0 + col * cell_spacing
        y = grid_y0 + row * cell_spacing
        prob = probs[i]

        # Base brightness by status
        if prob == 100:
            canvas.set_color(Color.Bright)
        elif prob > 50:
            canvas.set_color(Color.MediumBright)
        elif prob > 0:
            canvas.set_color(Color.Medium)
        else:
            canvas.set_color(Color.Low)

        # Draw cell frame
        canvas.draw_rect(x, y, cell_size, cell_size)
        if prob > 0:
            canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)

        # Probability < 100%: erase corner pixels
        if prob < 100 and prob > 0:
            canvas.set_blend_mode(BlendMode.Set)
            canvas.set_color(Color.None_)
            # Erase 1-4 corners based on probability
            corners = 4 - (prob // 25)
            if corners >= 1:
                canvas.point(x + 1, y + 1)  # top-left
            if corners >= 2:
                canvas.point(x + cell_size - 2, y + 1)  # top-right
            if corners >= 3:
                canvas.point(x + 1, y + cell_size - 2)  # bottom-left
            if corners >= 4:
                canvas.point(x + cell_size - 2, y + cell_size - 2)  # bottom-right
            canvas.set_blend_mode(BlendMode.Set)

        # Current step: 2px dot
        if i == current_step:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x + 2, y + 2, 2, 2)

        # Selected cell: 4px rect outline
        if i == selected_cell:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(x - 1, y - 1, cell_size + 2, cell_size + 2)

    # Page dots bottom-right
    canvas.set_color(Color.Bright)
    canvas.fill_rect(240, 50, 3, 3)
    canvas.set_color(Color.Medium)
    for i in range(1, 4):
        canvas.draw_rect(240 + i * 5, 50, 3, 3)


def render_ref_skylines(canvas: Canvas, voice1=None, voice2=None):
    """
    skylines reference screen.
    M-185 "city skyline" bar chart. Height = repetition count.
    Voice 1 foreground (bright), voice 2 background (dim).
    """
    if voice1 is None:
        voice1 = [3, 1, 4, 2, 5, 1, 3, 2]
    if voice2 is None:
        voice2 = [2, 4, 1, 3, 2, 5, 1, 3]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="SKYLINES")
    WindowPainter.draw_footer(canvas, ["V1", "V2", "MODE", "TRIG", "NEXT"])

    bar_w = 4
    bar_spacing = 2
    base_y = 50
    scale = 5  # px per rep

    canvas.set_blend_mode(BlendMode.Set)

    # Voice 2 background (dim)
    for i, reps in enumerate(voice2):
        x = 10 + i * (bar_w + bar_spacing)
        h = reps * scale
        canvas.set_color(Color.Medium)
        canvas.vline(x + 1, base_y - h, h)
        canvas.vline(x + 2, base_y - h, h)

    # Voice 1 foreground (bright)
    for i, reps in enumerate(voice1):
        x = 10 + i * (bar_w + bar_spacing)
        h = reps * scale
        canvas.set_color(Color.Bright)
        canvas.vline(x, base_y - h, h)
        canvas.vline(x + 1, base_y - h, h)
        canvas.vline(x + 2, base_y - h, h)
        canvas.vline(x + 3, base_y - h, h)

    # Base line
    canvas.set_color(Color.Low)
    canvas.hline(10, base_y, len(voice1) * (bar_w + bar_spacing))


def render_ref_prob_div(canvas: Canvas, weights=None, loop_length=16, cursor=0):
    """
    ProbabilityDivider reference screen.
    4 horizontal sliders with division labels.
    """
    if weights is None:
        weights = [8, 4, 2, 1]
    divs = [1, 2, 4, 8]
    labels = ["/1", "/2", "/4", "/8"]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="PROBDIV")
    WindowPainter.draw_footer(canvas, ["/1", "/2", "/4", "/8", "NEXT"], highlight=cursor)

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(4):
        y = 16 + i * 10
        # Division label
        canvas.set_color(Color.Bright)
        canvas.draw_text(4, y + 5, labels[i])

        # Slider track (dotted line)
        slider_x = 30
        slider_w = 80
        canvas.set_color(Color.Medium)
        for x in range(slider_x, slider_x + slider_w, 2):
            canvas.point(x, y + 3)

        # Knob (2x8 rect)
        val = weights[i]
        knob_x = slider_x + (val * slider_w // 16)
        canvas.set_color(Color.Bright if cursor == i else Color.Medium)
        canvas.fill_rect(knob_x, y, 3, 7)

    # Loop section bottom
    canvas.set_color(Color.Bright)
    canvas.draw_text(4, 56, "LOOP")
    canvas.draw_text(34, 56, str(loop_length) if loop_length > 0 else "off")
    if cursor == 4:
        canvas.set_color(Color.Bright)
        canvas.draw_text(60, 56, "<")


# =============================================================================
# Hybrid Stochastic Pages — Reference Design Crossovers (First Wave)
# =============================================================================

def render_stochastic_steps_skylines(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps as city-skyline bar chart (skylines-derived). Gate probability = foreground bar height. Note probability = background dim bar."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS SKY")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    step_offset = section * 16
    current_step = track_engine.current_step()
    base_y = 50
    bar_w = 6
    bar_gap = 2
    scale = 30
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = 8 + i * (bar_w + bar_gap)
        np_h = (step.note_probability() * scale) // 100
        canvas.set_color(Color.Low)
        canvas.vline(x + 2, base_y - np_h, np_h)
        gp_h = (step.gate_probability() * scale) // 100
        canvas.set_color(Color.Bright if step_index == current_step else (Color.MediumBright if step.gate() else Color.Medium))
        canvas.vline(x, base_y - gp_h, gp_h)
        canvas.vline(x + 1, base_y - gp_h, gp_h)
        canvas.vline(x + 2, base_y - gp_h, gp_h)
        canvas.vline(x + 3, base_y - gp_h, gp_h)
        if step.gate() and gp_h > 8:
            note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
            canvas.set_color(Color.Bright)
            canvas.draw_text(x, base_y - gp_h - 6, note_names[step.note() % 12])
    canvas.set_color(Color.Medium)
    canvas.hline(8, base_y, 16 * (bar_w + bar_gap))

def render_stochastic_steps_delinquencer(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps as 4x16 delinquencer grid with corner-pixel probability erosion."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS DEL")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    step_offset = section * 16
    current_step = track_engine.current_step()
    cell_size = 5
    cell_gap = 2
    grid_x0 = 8
    grid_y0 = 14
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(4):
        for col in range(16):
            step_index = step_offset + row * 16 + col
            if step_index >= 64: break
            step = sequence.step(step_index)
            x = grid_x0 + col * (cell_size + cell_gap)
            y = grid_y0 + row * (cell_size + cell_gap)
            prob = step.gate_probability()
            if prob == 100: canvas.set_color(Color.Bright)
            elif prob >= 50: canvas.set_color(Color.MediumBright)
            elif prob > 0: canvas.set_color(Color.Medium)
            else: canvas.set_color(Color.Low)
            canvas.draw_rect(x, y, cell_size, cell_size)
            if prob > 0:
                canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)
            if 0 < prob < 100:
                corners = 4 - (prob // 25)
                canvas.set_blend_mode(BlendMode.Set)
                canvas.set_color(Color.None_)
                if corners >= 1: canvas.point(x + 1, y + 1)
                if corners >= 2: canvas.point(x + cell_size - 2, y + 1)
                if corners >= 3: canvas.point(x + 1, y + cell_size - 2)
                if corners >= 4: canvas.point(x + cell_size - 2, y + cell_size - 2)
                canvas.set_blend_mode(BlendMode.Set)
            if step_index == current_step:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x + 2, y + 2, 2, 2)

def render_stochastic_steps_euclid(canvas: Canvas, sequence, track_engine, section: int = 0):
    """Steps arranged on a Euclidean ring (squares-and-circles derived). Square = deterministic gate on. Circle = probabilistic/off."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS EUC")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    import math
    cx = 80
    cy = 34
    radius = 20
    step_offset = section * 16
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        angle = math.radians(i * 360 / 16 - 90)
        px = int(cx + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        prob = step.gate_probability()
        if step_index == current_step:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(px - 2, py - 2, 5, 5)
        if step.gate() and prob >= 75:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        elif prob > 0:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px - 1, py - 1, 3, 3)
        else:
            canvas.set_color(Color.Low)
            canvas.point(px, py)
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 3, cy - 2, note_names[sequence.step(current_step).note() % 12])
    cx2 = 180
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx2 + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    for i in range(16):
        step = sequence.step(step_offset + i)
        angle = math.radians(i * 360 / 16 - 90)
        px = int(cx2 + radius * math.cos(angle))
        py = int(cy + radius * math.sin(angle))
        np = step.note_probability()
        if np >= 75:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(px - 1, py - 1, 3, 3)
        elif np > 0:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(px - 1, py - 1, 3, 3)
        else:
            canvas.set_color(Color.Low)
            canvas.point(px, py)

def render_stochastic_pitch_prob_melod(canvas: Canvas, sequence, track_engine, weights=None, active_note=4, active_octave=0):
    """Full ProbMeloD keyboard + weight bars for stochastic pitch page."""
    if weights is None:
        tickets = sequence.degree_tickets()
        weights = [min(10, max(-1, tickets[i])) if i < len(tickets) else 0 for i in range(12)]
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH KEY")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    semitone_x = [8 + i * 20 for i in range(12)]
    black_keys = {1, 3, 6, 8, 10}
    bar_base_y = 20
    bar_max_h = 22
    canvas.set_blend_mode(BlendMode.Set)
    for i, w in enumerate(weights):
        x = semitone_x[i]
        canvas.set_color(Color.Medium)
        for y in range(bar_base_y, bar_base_y + bar_max_h, 2):
            canvas.point(x, y)
        if w < 0:
            canvas.set_color(Color.Bright)
            canvas.draw_text(x - 2, bar_base_y + bar_max_h - 6, "X")
        elif w > 0:
            tick_y = bar_base_y + bar_max_h - (w * bar_max_h // 10)
            canvas.set_color(Color.Bright)
            canvas.hline(x - 2, tick_y, 5)
    key_y = 46
    for idx in [0, 2, 4, 5, 7, 9, 11]:
        x = semitone_x[idx] - 7
        canvas.set_color(Color.Medium)
        canvas.draw_rect(x, key_y, 14, 10)
    for idx in black_keys:
        x = semitone_x[idx] - 4
        canvas.set_color(Color.Bright)
        canvas.fill_rect(x, key_y, 8, 7)
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    canvas.set_color(Color.Bright)
    canvas.draw_text(semitone_x[active_note] - 3, key_y + 12, note_names[active_note])
    canvas.draw_text(220, key_y + 12, f"O{active_octave}")
    canvas.set_color(Color.Bright)
    if 0 <= sequence.min_degree() < 12:
        canvas.vline(semitone_x[sequence.min_degree()] - 6, bar_base_y, bar_max_h)
    if 0 <= sequence.max_degree() < 12:
        canvas.vline(semitone_x[sequence.max_degree()] + 6, bar_base_y, bar_max_h)

def render_stochastic_pitch_circle(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """Pitch class circle (Automatonnetz / OC_menus derived). Dot radius = ticket weight. Chord triangle connecting top 3."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH CIR")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    import math
    cx = 80
    cy = 34
    radius = 20
    tickets = sequence.degree_tickets()
    scale_size = min(12, sequence.scale_size())
    positions = []
    for i in range(scale_size):
        angle = math.radians(i * 360 / scale_size - 90)
        positions.append((int(cx + radius * math.cos(angle)), int(cy + radius * math.sin(angle))))
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    for angle in range(0, 360, 5):
        rad = math.radians(angle)
        canvas.point(int(cx + radius * math.cos(rad)), int(cy + radius * math.sin(rad)))
    sorted_degrees = sorted(range(scale_size), key=lambda i: tickets[i] if i < len(tickets) and tickets[i] >= 0 else -999, reverse=True)
    top3 = [d for d in sorted_degrees[:3] if d < len(tickets) and tickets[d] >= 0]
    if len(top3) >= 3:
        canvas.set_color(Color.MediumBright)
        for i in range(len(top3)):
            x1, y1 = positions[top3[i]]
            x2, y2 = positions[top3[(i + 1) % len(top3)]]
            dx, dy = x2 - x1, y2 - y1
            steps = max(abs(dx), abs(dy))
            if steps > 0:
                for s in range(steps + 1):
                    canvas.point(x1 + dx * s // steps, y1 + dy * s // steps)
    for i in range(scale_size):
        px, py = positions[i]
        selected = (i == selected_degree)
        t = tickets[i] if i < len(tickets) else 0
        if t < 0:
            canvas.set_color(Color.Low)
            canvas.draw_text(px - 2, py - 2, "X")
        elif t == 0:
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.draw_rect(px - 1, py - 1, 3, 3)
        else:
            r = min(4, 1 + t // 2)
            canvas.set_color(Color.Bright if selected else Color.MediumBright)
            canvas.fill_rect(px - r, py - r, r * 2 + 1, r * 2 + 1)
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 3, cy - 2, f"{track_engine.current_note()}")
    canvas.set_color(Color.Medium)
    canvas.draw_text(140, 20, f"ROOT:{sequence.scale_root()}")
    canvas.draw_text(140, 28, f"SIZE:{scale_size}")
    canvas.draw_text(140, 36, f"LIN:{sequence.linearity()}")

def render_stochastic_pitch_less_concepts(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """Dense less-concepts-derived pitch page. 8-bit seed pattern, large note name, parameter text, preset grid."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH DNS")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])
    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(min(8, scale_size)):
        t = tickets[i] if i < len(tickets) else 0
        canvas.set_color(Color.Bright if i == selected_degree else (Color.Medium if t > 0 else Color.Low))
        canvas.fill_rect(5 * i, 14, 4, 4)
    canvas.set_color(Color.Bright)
    canvas.hline(5 * selected_degree, 13, 4)
    current_note = track_engine.current_note()
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    nn = note_names[current_note % 12]
    canvas.set_color(Color.Bright)
    canvas.draw_text(60, 28, nn)
    canvas.set_color(Color.Medium)
    canvas.draw_text(62, 26, nn)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    canvas.draw_text(120, 20, f"deg:{selected_degree+1}")
    canvas.draw_text(120, 28, f"tix:{tickets[selected_degree] if selected_degree < len(tickets) else 0}")
    canvas.draw_text(120, 36, f"rot:{sequence.degree_rotation():+d}")
    canvas.draw_text(120, 44, f"lin:{sequence.linearity()}")
    canvas.draw_text(180, 20, f"min:{sequence.min_degree()}")
    canvas.draw_text(180, 28, f"max:{sequence.max_degree()}")
    canvas.draw_text(180, 36, f"root:{sequence.scale_root()}")
    for i in range(2):
        for j in range(8):
            idx = j + i * 8
            active = idx < scale_size and tickets[idx] >= 0
            canvas.set_color(Color.Medium if active else Color.Low)
            canvas.fill_rect(j * 5, 54 + i * 5, 4, 4)
            if idx == selected_degree:
                canvas.set_color(Color.Bright)
                canvas.draw_rect(j * 5 - 1, 53 + i * 5, 6, 6)

def render_stochastic_dice_shredder(canvas: Canvas, sequence, track_engine, axis: int = 0, active_cell: int = 5):
    """8x8 Cartesian grid (Shredder-derived) for 64-step dice view."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE SHR")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    grid_x0 = 8
    grid_y0 = 14
    cell_size = 5
    cell_spacing = 7
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(8):
        for col in range(8):
            step_index = row * 8 + col
            step = sequence.step(step_index)
            x = grid_x0 + col * cell_spacing
            y = grid_y0 + row * cell_spacing
            if axis == 0: prob = step.gate_probability()
            elif axis == 1: prob = step.note_probability()
            elif axis == 2: prob = step.length_probability()
            else: prob = step.retrigger_probability()
            is_current = (step_index == current_step)
            is_active = (step_index == active_cell)
            canvas.set_color(Color.Medium)
            canvas.draw_rect(x, y, cell_size, cell_size)
            if prob >= 75: canvas.set_color(Color.Bright)
            elif prob >= 50: canvas.set_color(Color.MediumBright)
            elif prob > 0: canvas.set_color(Color.Medium)
            else: canvas.set_color(Color.None_)
            if prob > 0 or is_current:
                canvas.fill_rect(x + 1, y + 1, cell_size - 2, cell_size - 2)
            if is_active:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x, y, cell_size, cell_size)
    ax = grid_x0 + (active_cell % 8) * cell_spacing + cell_size // 2
    ay = grid_y0 + (active_cell // 8) * cell_spacing + cell_size // 2
    canvas.set_color(Color.Bright)
    for x in range(grid_x0, grid_x0 + 8 * cell_spacing, 2):
        canvas.point(x, ay)
    for y in range(grid_y0, grid_y0 + 8 * cell_spacing, 2):
        canvas.point(ax, y)

def render_stochastic_dice_register(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """64-step register bar horizon (ShiftReg-derived). Vertical bar height = probability. 4 rows of 16."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE REG")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])
    bar_w = 2
    bar_gap = 1
    bar_max_h = 10
    base_y = 50
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(4):
        for col in range(16):
            step_index = row * 16 + col
            step = sequence.step(step_index)
            x = 8 + col * (bar_w + bar_gap) + row * 64
            if x > 240: x = 8 + col * (bar_w + bar_gap)
            if axis == 0: prob = step.gate_probability()
            elif axis == 1: prob = step.note_probability()
            elif axis == 2: prob = step.length_probability()
            else: prob = step.retrigger_probability()
            is_current = (step_index == current_step)
            h = (prob * bar_max_h) // 100
            if is_current:
                canvas.set_color(Color.Bright)
                canvas.fill_rect(x, base_y - bar_max_h, bar_w, bar_max_h)
            else:
                canvas.set_color(Color.MediumBright if prob >= 50 else Color.Medium)
                if h > 0:
                    canvas.fill_rect(x, base_y - h, bar_w, h)
                else:
                    canvas.set_color(Color.Low)
                    canvas.vline(x, base_y - 2, 2)
    canvas.set_color(Color.Medium)
    canvas.hline(8, base_y, 64)
    canvas.hline(8, base_y + 1, 64)

def render_stochastic_marbles_rndwalk(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """RndWalk-derived inverted horizontal bar meters for Spread/Bias/Steps/Mode."""
    slots = ["SPRD", "BIAS", "STEP", "MODE"]
    modes = ["OFF", "SOFT", "HARD"]
    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()
    mode = sequence.marbles_mode()
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE RW")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)
    params = [("SPREAD", spread, 0, 100), ("BIAS", bias, -50, 50), ("STEPS", steps, 0, 16), ("MODE", mode, 0, 2)]
    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, val, minv, maxv) in enumerate(params):
        y = 18 + i * 10
        canvas.set_color(Color.Bright if i == slot else Color.Medium)
        canvas.draw_text(4, y + 5, name)
        if i == 3:
            mstr = modes[val] if val < len(modes) else "OFF"
            canvas.set_color(Color.Bright if i == slot else Color.Medium)
            canvas.draw_text(50, y + 5, mstr)
            continue
        center_x = 70
        max_w = 40
        if minv < 0:
            w = (abs(val) * max_w) // max(abs(minv), abs(maxv))
            if val >= 0:
                canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
                canvas.fill_rect(center_x, y, w, 7)
            else:
                canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
                canvas.fill_rect(center_x - w, y, w, 7)
            canvas.set_color(Color.Medium)
            canvas.vline(center_x, y, 7)
        else:
            w = (val * max_w) // maxv
            canvas.set_color(Color.Bright if i == slot else Color.MediumBright)
            canvas.fill_rect(center_x - max_w // 2, y, w, 7)
        canvas.set_color(Color.Bright)
        canvas.draw_text(120, y + 5, str(val))

def render_stochastic_marbles_constellations(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """Constellations-derived star-field distribution. Stars = random points, brightness = probability density."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE CST")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)
    import math, random
    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()
    canvas.set_blend_mode(BlendMode.Set)
    rng = random.Random(42)
    num_stars = 20 + steps * 3
    for _ in range(num_stars):
        sx = rng.randint(10, 180)
        sy = rng.randint(14, 50)
        center_x = 95 + bias
        dist = abs(sx - center_x)
        envelope = math.exp(-(dist ** 2) / (spread * 2 + 1))
        brightness = int(envelope * 15)
        if brightness >= 12: canvas.set_color(Color.Bright)
        elif brightness >= 8: canvas.set_color(Color.MediumBright)
        elif brightness >= 4: canvas.set_color(Color.Medium)
        else: canvas.set_color(Color.Low)
        size = 1 if brightness < 8 else 2
        if size == 1: canvas.point(sx, sy)
        else: canvas.fill_rect(sx - 1, sy - 1, 3, 3)
    cursor_x = 95 + bias
    cursor_y = 32
    canvas.set_color(Color.Bright)
    for x in range(cursor_x - 6, cursor_x + 7, 2):
        canvas.point(x, cursor_y)
    for y in range(cursor_y - 6, cursor_y + 7, 2):
        canvas.point(cursor_x, y)
    canvas.set_color(Color.Medium)
    canvas.draw_text(190, 20, f"S{spread}")
    canvas.draw_text(190, 28, f"B{bias:+d}")
    canvas.draw_text(190, 36, f"N{steps}")

def render_stochastic_lock_skylines(canvas: Canvas, sequence, track_engine):
    """Lock page as skyline bar chart. Captured events = bright foreground bars. Source = dim background."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK SKY")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])
    bar_w = 4
    bar_gap = 1
    base_y = 50
    scale = 5
    first = sequence.lock_first()
    last = sequence.lock_last()
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        evt = sequence.lock_event(i)
        source = sequence.step(i)
        x = 8 + i * (bar_w + bar_gap)
        src_h = (source.gate_probability() * scale) // 100
        canvas.set_color(Color.Low)
        canvas.vline(x + 2, base_y - src_h, src_h)
        if first <= i <= last and evt.gate():
            evt_h = (evt.note() % 7 + 1) * scale
            canvas.set_color(Color.Bright)
            canvas.vline(x, base_y - evt_h, evt_h)
            canvas.vline(x + 1, base_y - evt_h, evt_h)
    canvas.set_color(Color.Bright)
    fx = 8 + first * (bar_w + bar_gap)
    lx = 8 + last * (bar_w + bar_gap) + bar_w
    canvas.vline(fx, base_y - 20, 20)
    canvas.vline(lx, base_y - 20, 20)
    canvas.hline(fx, base_y - 20, lx - fx)
    canvas.hline(fx, base_y, lx - fx)
    canvas.set_color(Color.Medium)
    canvas.hline(8, base_y, 16 * (bar_w + bar_gap))

def render_stochastic_lock_qfwfq(canvas: Canvas, sequence, track_engine):
    """qfwfq-derived ASCII ribbon lock page. Top row = captured gate state. Bottom row = source step index."""
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK RIB")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])
    first = sequence.lock_first()
    last = sequence.lock_last()
    current_step = track_engine.current_step()
    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        evt = sequence.lock_event(i)
        x = 8 + i * 15
        in_window = first <= i <= last
        if in_window and evt.gate():
            ch = "X"
            canvas.set_color(Color.Bright)
        elif in_window:
            ch = "."
            canvas.set_color(Color.Medium)
        else:
            ch = "?"
            canvas.set_color(Color.Low)
        if i == current_step:
            canvas.set_color(Color.Bright)
            canvas.hline(x, 38, 8)
        canvas.set_color(Color.Bright if (in_window and evt.gate()) else (Color.Medium if in_window else Color.Low))
        canvas.draw_text(x, 28, ch)
    for i in range(16):
        source = sequence.step(i)
        x = 8 + i * 15
        idx_str = f"{i+1:02d}"
        canvas.set_color(Color.Medium if source.gate() else Color.Low)
        canvas.draw_text(x, 44, idx_str)
    cx = 8 + current_step * 15 + 4
    canvas.set_color(Color.Bright)
    canvas.fill_rect(cx, 54, 2, 2)

def render_stochastic_track_rndwalk(canvas: Canvas, sequence, track_engine, current_page: int = 0, selected_slot: int = 0):
    """RndWalk-derived inverted horizontal bar meter console."""
    param_pages = [
        [("OCT", "octave", -4, 4), ("TRANS", "transpose", -12, 12), ("DIV", "division", 1, 16), ("RESET", "reset_mode", 0, 4)],
        [("GBIAS", "gbias", -50, 50), ("NBIAS", "nbias", -50, 50), ("LBIAS", "lbias", -50, 50), ("RBIAS", "rbias", -50, 50)],
        [("ACC", "accent", 0, 100), ("LEG", "legato", 0, 100), ("FILL", "fill", 0, 100), ("REST", "rest_prob", 0, 100)],
    ]
    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK RW P{current_page+1}")
    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr, minv, maxv) in enumerate(page):
        y = 16 + i * 10
        val = getattr(sequence, attr)()
        canvas.set_color(Color.Bright if selected_slot == i else Color.Medium)
        canvas.draw_text(4, y + 5, name)
        center_x = 70
        bar_max_w = 50
        if minv < 0:
            w = (abs(val) * bar_max_w) // max(abs(minv), abs(maxv))
            if val >= 0:
                canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
                canvas.fill_rect(center_x, y, w, 7)
            else:
                canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
                canvas.fill_rect(center_x - w, y, w, 7)
            canvas.set_color(Color.Medium)
            canvas.vline(center_x, y, 7)
        else:
            w = (val * bar_max_w) // maxv
            canvas.set_color(Color.Bright if selected_slot == i else Color.MediumBright)
            canvas.fill_rect(center_x - bar_max_w // 2, y, w, 7)
        canvas.set_color(Color.Bright)
        canvas.draw_text(130, y + 5, str(val))
    box_x, box_y = 180, 14
    canvas.set_color(Color.Medium)
    canvas.draw_rect(box_x, box_y, 72, 36)
    canvas.set_color(Color.Bright)
    canvas.draw_text(box_x + 4, box_y + 7, f"S{track_engine.current_step()+1}")
    canvas.draw_text(box_x + 4, box_y + 15, "L" if sequence.lock_enabled() else "-")
    canvas.draw_text(box_x + 4, box_y + 23, f"N{track_engine.current_note()}")
    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)

def render_stochastic_track_sequencex(canvas: Canvas, sequence, track_engine, current_page: int = 0, selected_slot: int = 0):
    """SequenceX-derived vertical slider columns. Dotted line per param + 5x3 rect indicator at value."""
    param_pages = [
        [("OCT", "octave", -4, 4), ("TRANS", "transpose", -12, 12), ("DIV", "division", 1, 16), ("RESET", "reset_mode", 0, 4)],
        [("GBIAS", "gbias", -50, 50), ("NBIAS", "nbias", -50, 50), ("LBIAS", "lbias", -50, 50), ("RBIAS", "rbias", -50, 50)],
    ]
    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK SX P{current_page+1}")
    slider_y0 = 18
    slider_h = 36
    col_width = 51
    zero_y = slider_y0 + slider_h // 2
    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr, minv, maxv) in enumerate(page):
        x = i * col_width + col_width // 2
        val = getattr(sequence, attr)()
        canvas.set_color(Color.Low)
        for dx in range(x - 2, x + 3, 2):
            canvas.point(dx, zero_y)
        canvas.set_color(Color.Medium)
        for y in range(slider_y0, slider_y0 + slider_h, 2):
            canvas.point(x, y)
        range_span = maxv - minv
        pos_y = slider_y0 + slider_h - 2 - ((val - minv) * (slider_h - 4) // range_span)
        pos_y = max(slider_y0, min(slider_y0 + slider_h - 3, pos_y))
        if i == selected_slot:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x - 2, pos_y, 5, 3)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(x - 2, pos_y, 5, 3)
        canvas.set_color(Color.Bright if i == selected_slot else Color.Medium)
        canvas.draw_text(x - canvas.text_width(name) // 2, slider_y0 + slider_h + 2, name)
        canvas.set_color(Color.Bright)
        canvas.draw_text(x - canvas.text_width(str(val)) // 2, slider_y0 - 4, str(val))
    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)


# =============================================================================
# Stochastic Track Page Renderers
# =============================================================================

STOCH_LAYERS = [
    # Bank A
    ("GATE", "GPROB"), ("NOTE", "NPROB"), ("LEN", "LPROB"), ("RTRG", "RPROB"),
    # Bank B
    ("OCT", "OFFSET"), ("SLIDE", "COND"), ("REPEAT", "MODE"), ("REST", "LOOP"),
]


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

def render_stochastic_steps_bline(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    bline-derived stacked pattern bar charts.
    5 rows: gate, note, length, retrigger, condition.
    Each row = 16 vertical bars (3px wide). Bar top highlighted at current step.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS BLN")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    bar_w = 3
    bar_gap = 1
    base_y = 48
    rows = [
        ("G", lambda s: s.gate_probability(), 6),
        ("N", lambda s: s.note(), 12),
        ("L", lambda s: s.length() * 3, 6),
        ("R", lambda s: s.retrigger() * 2, 4),
        ("C", lambda s: s.condition(), 4),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter, scale) in enumerate(rows):
        row_y = base_y - row_idx * 9
        # Row label
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y - 1, label)

        for i in range(16):
            step_index = step_offset + i
            step = sequence.step(step_index)
            x = 12 + i * (bar_w + bar_gap)
            val = getter(step)
            h = min(8, val // scale) if scale > 0 else 0
            is_current = (step_index == current_step)

            # Bar body
            canvas.set_color(Color.MediumBright if is_current else Color.Low)
            canvas.vline(x, row_y - h, h)
            # Bar top
            canvas.set_color(Color.Bright if is_current else Color.Medium)
            canvas.point(x, row_y - h - 1)


def render_stochastic_steps_hiswing(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    Hiswing-derived vertical line chart.
    Each step = vertical line from bottom. Height = note value.
    Swing offset displaces even steps horizontally. Brightness = probability.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS HSW")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    base_y = 50
    step_w = 120 // 16

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = 4 + i * step_w

        # Swing offset for even steps
        if i % 2 == 1:
            x += 2

        note_h = min(40, step.note() * 3 + 4)
        prob = step.gate_probability()
        is_current = (step_index == current_step)
        is_selected = (i == 3)  # demo selected

        if is_current:
            # Current step: line 4px higher
            canvas.set_color(Color.Bright)
            canvas.vline(x, base_y - note_h - 4, note_h + 4)
        elif is_selected:
            canvas.set_color(Color.MediumBright)
            canvas.vline(x, base_y - note_h, note_h)
        else:
            # Probability = brightness
            if prob >= 75:
                canvas.set_color(Color.Medium)
            elif prob >= 50:
                canvas.set_color(Color.MediumBright)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)
            canvas.vline(x, base_y - note_h, note_h)


def render_stochastic_steps_meadowphysics(canvas: Canvas, sequence, track_engine, section: int = 0):
    """
    meadowphysics-derived horizontal tracker strips.
    4 rows of 2x2 px dots across 16 steps. Dot brightness = probability.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "STEPS MP")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    step_offset = section * 16
    current_step = track_engine.current_step()
    dot_size = 2
    padding = 6
    offset_x = 12
    offset_y = 16
    rows = [
        ("GATE", lambda s: s.gate_probability()),
        ("NOTE", lambda s: s.note_probability()),
        ("LEN", lambda s: s.length_probability()),
        ("RTRG", lambda s: s.retrigger_probability()),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter) in enumerate(rows):
        row_y = offset_y + padding * row_idx
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y + 2, label[:1])

        for i in range(16):
            step_index = step_offset + i
            step = sequence.step(step_index)
            x = offset_x + padding * i
            prob = getter(step)
            is_current = (step_index == current_step)

            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob > 0:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)

            canvas.fill_rect(x, row_y, dot_size, dot_size)

# ---------------------------------------------------------------------------
# Pitch variants
# ---------------------------------------------------------------------------

def render_stochastic_pitch_kreislauf(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """
    Kreislauf-derived concentric ring wedges.
    Scale degrees as wedges in a ring. Brightness = ticket count.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH KRS")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])

    import math
    cx = 80
    cy = 34
    r_outer = 22
    r_inner = 12
    tickets = sequence.degree_tickets()
    scale_size = min(16, sequence.scale_size())

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(scale_size):
        t = tickets[i] if i < len(tickets) else 0
        selected = (i == selected_degree)

        angle1 = math.radians(i * 360 / scale_size - 90)
        angle2 = math.radians((i + 1) * 360 / scale_size - 90)

        # Wedge brightness by ticket count
        if t < 0:
            canvas.set_color(Color.Low)
        elif t == 0:
            canvas.set_color(Color.Medium)
        elif t >= 5:
            canvas.set_color(Color.Bright if selected else Color.MediumBright)
        else:
            canvas.set_color(Color.MediumBright if selected else Color.Medium)

        # Draw wedge as filled polygon (4-point)
        p1 = (int(cx + r_outer * math.cos(angle1)), int(cy + r_outer * math.sin(angle1)))
        p2 = (int(cx + r_outer * math.cos(angle2)), int(cy + r_outer * math.sin(angle2)))
        p3 = (int(cx + r_inner * math.cos(angle2)), int(cy + r_inner * math.sin(angle2)))
        p4 = (int(cx + r_inner * math.cos(angle1)), int(cy + r_inner * math.sin(angle1)))

        # Simple fill: scanline between inner and outer radius
        for a in range(int(math.degrees(angle1)), int(math.degrees(angle2)) + 1):
            rad = math.radians(a)
            xo = int(cx + r_outer * math.cos(rad))
            yo = int(cy + r_outer * math.sin(rad))
            xi = int(cx + r_inner * math.cos(rad))
            yi = int(cy + r_inner * math.sin(rad))
            dx = xo - xi
            dy = yo - yi
            steps = max(abs(dx), abs(dy))
            if steps > 0:
                for s in range(steps + 1):
                    canvas.point(xi + dx * s // steps, yi + dy * s // steps)

        # Wedge outline
        canvas.set_color(Color.Bright if selected else Color.Medium)
        for s in range(100):
            canvas.point(p1[0] + (p2[0]-p1[0])*s//100, p1[1] + (p2[1]-p1[1])*s//100)
            canvas.point(p2[0] + (p3[0]-p2[0])*s//100, p2[1] + (p3[1]-p2[1])*s//100)
            canvas.point(p3[0] + (p4[0]-p3[0])*s//100, p3[1] + (p4[1]-p3[1])*s//100)
            canvas.point(p4[0] + (p1[0]-p4[0])*s//100, p4[1] + (p1[1]-p4[1])*s//100)

    # Center info
    canvas.set_color(Color.Bright)
    canvas.draw_text(cx - 3, cy - 2, f"{scale_size}")


def render_stochastic_pitch_bline(canvas: Canvas, sequence, track_engine, selected_degree: int = 2):
    """
    bline-derived pattern bars for pitch degrees.
    3px wide vertical bars, height = tickets. Current degree = bright top.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "PITCH BLN")
    WindowPainter.draw_footer(canvas, ["TIX", "DEG", "RANGE", "LIN", "NEXT"])

    tickets = sequence.degree_tickets()
    scale_size = sequence.scale_size()
    bar_w = 3
    bar_gap = 1
    base_y = 48
    bar_max_h = 30

    canvas.set_blend_mode(BlendMode.Set)
    for i in range(scale_size):
        t = tickets[i] if i < len(tickets) else 0
        x = 8 + i * (bar_w + bar_gap)
        selected = (i == selected_degree)

        if t < 0:
            canvas.set_color(Color.Low)
            canvas.draw_text(x - 1, base_y - 6, "X")
        elif t == 0:
            h = 2
            canvas.set_color(Color.Medium if selected else Color.Low)
            canvas.vline(x, base_y - h, h)
        else:
            h = min(bar_max_h, t * 3)
            is_current = selected
            # Bar body
            canvas.set_color(Color.MediumBright if is_current else Color.Medium)
            canvas.vline(x, base_y - h, h)
            # Bar top
            canvas.set_color(Color.Bright if is_current else Color.MediumBright)
            canvas.point(x, base_y - h - 1)

        # Degree number
        canvas.set_color(Color.Bright if selected else Color.Medium)
        canvas.draw_text(x - 1, base_y + 2, f"{i+1}")

# ---------------------------------------------------------------------------
# Dice variants
# ---------------------------------------------------------------------------

def render_stochastic_dice_grd(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """
    grd-derived 8x8 brightness grid.
    5x5 px rects, brightness = probability for active axis.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE GRD")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    current_step = track_engine.current_step()
    cell_size = 5
    grid_x0 = 8
    grid_y0 = 14

    canvas.set_blend_mode(BlendMode.Set)
    for row in range(8):
        for col in range(8):
            step_index = row * 8 + col
            step = sequence.step(step_index)
            x = grid_x0 + col * cell_size
            y = grid_y0 + row * cell_size

            if axis == 0:
                prob = step.gate_probability()
            elif axis == 1:
                prob = step.note_probability()
            elif axis == 2:
                prob = step.length_probability()
            else:
                prob = step.retrigger_probability()

            is_current = (step_index == current_step)

            # Brightness mapping
            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob >= 50:
                canvas.set_color(Color.Medium)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)

            canvas.fill_rect(x, y, cell_size - 1, cell_size - 1)


def render_stochastic_dice_kreislauf(canvas: Canvas, sequence, track_engine, axis: int = 0):
    """
    Kreislauf-derived concentric ring probability wheel.
    4 rings = 4 probability layers. 16 wedges per ring.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "DICE KRS")
    WindowPainter.draw_footer(canvas, ["GATE", "NOTE", "LEN", "RTRG", "NEXT"])

    import math
    cx = 80
    cy = 34
    rings = [
        ("GATE", lambda s: s.gate_probability(), 20, 16),
        ("NOTE", lambda s: s.note_probability(), 15, 11),
        ("LEN", lambda s: s.length_probability(), 10, 6),
        ("RTRG", lambda s: s.retrigger_probability(), 5, 2),
    ]
    current_step = track_engine.current_step()

    canvas.set_blend_mode(BlendMode.Set)
    for ring_idx, (label, getter, r_out, r_in) in enumerate(rings):
        for i in range(16):
            step_index = i
            step = sequence.step(step_index)
            prob = getter(step)
            is_current = (step_index == current_step)

            angle1 = math.radians(i * 360 / 16 - 90)
            angle2 = math.radians((i + 1) * 360 / 16 - 90)

            # Brightness
            if is_current:
                canvas.set_color(Color.Bright)
            elif prob >= 75:
                canvas.set_color(Color.MediumBright)
            elif prob >= 50:
                canvas.set_color(Color.Medium)
            elif prob > 0:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.None_)

            # Draw wedge radial line
            for a in range(int(math.degrees(angle1)), int(math.degrees(angle2)) + 1):
                rad = math.radians(a)
                xo = int(cx + r_out * math.cos(rad))
                yo = int(cy + r_out * math.sin(rad))
                xi = int(cx + r_in * math.cos(rad))
                yi = int(cy + r_in * math.sin(rad))
                dx = xo - xi
                dy = yo - yi
                steps = max(abs(dx), abs(dy))
                if steps > 0:
                    for s in range(steps + 1):
                        canvas.point(xi + dx * s // steps, yi + dy * s // steps)

    # Labels right side
    for ring_idx, (label, getter, r_out, r_in) in enumerate(rings):
        canvas.set_color(Color.Medium)
        canvas.draw_text(140, 16 + ring_idx * 8, label)

# ---------------------------------------------------------------------------
# Marbles variants
# ---------------------------------------------------------------------------

def render_stochastic_marbles_pit_orchisstra(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """
    pit-orchisstra-derived snake path through probability field.
    Snake body = distribution samples. Food = high-probability regions.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE SNAKE")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)

    import math
    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()
    steps = sequence.marbles_steps()

    # Draw probability field as 8x8 grid
    grid_x0 = 8
    grid_y0 = 14
    cell_size = 4
    canvas.set_blend_mode(BlendMode.Set)
    for row in range(8):
        for col in range(8):
            x = grid_x0 + col * cell_size
            y = grid_y0 + row * cell_size
            # Distance from bias center
            dist = abs(col - 4 + bias // 10)
            envelope = math.exp(-(dist ** 2) / (spread / 10 + 1))
            brightness = int(envelope * 15)
            if brightness > 10:
                canvas.set_color(Color.Bright)
            elif brightness > 5:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)
            canvas.fill_rect(x, y, cell_size - 1, cell_size - 1)

    # Snake path
    snake_len = steps + 3
    snake_x = 4 + bias // 5
    snake_y = 4
    canvas.set_color(Color.Bright)
    for i in range(snake_len):
        gx = grid_x0 + snake_x * cell_size + 1
        gy = grid_y0 + snake_y * cell_size + 1
        canvas.fill_rect(gx, gy, 2, 2)
        # Wiggle
        snake_x = (snake_x + 1) % 8
        snake_y = (snake_y + (1 if i % 2 == 0 else -1)) % 8

    # Params right
    canvas.set_color(Color.Medium)
    canvas.draw_text(180, 20, f"S{spread}")
    canvas.draw_text(180, 28, f"B{bias:+d}")
    canvas.draw_text(180, 36, f"N{steps}")


def render_stochastic_marbles_bline(canvas: Canvas, sequence, track_engine, slot: int = 0):
    """
    bline-derived XY position grid with crosshairs.
    Spread = grid cursor size. Bias = cursor position.
    4-quadrant parameter display.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "MARBLE XY")
    WindowPainter.draw_footer(canvas, ["SPRD", "BIAS", "STEP", "MODE", "PITCH"], highlight=slot)

    spread = sequence.marbles_spread()
    bias = sequence.marbles_bias()

    # XY grid background
    grid_x = 10
    grid_y = 16
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    for row in range(4):
        for col in range(4):
            canvas.draw_rect(grid_x + col * 10, grid_y + row * 10, 9, 9)

    # Cursor position from bias
    cursor_col = 1 + (bias + 50) // 25
    cursor_row = 1 + (spread // 25)
    cursor_col = max(0, min(3, cursor_col))
    cursor_row = max(0, min(3, cursor_row))

    # Highlight current cell
    canvas.set_color(Color.Medium)
    canvas.fill_rect(grid_x + cursor_col * 10, grid_y + cursor_row * 10, 9, 9)

    # Crosshairs
    cx = grid_x + cursor_col * 10 + 4
    cy = grid_y + cursor_row * 10 + 4
    canvas.set_color(Color.Bright)
    for x in range(grid_x, grid_x + 40, 2):
        canvas.point(x, cy)
    for y in range(grid_y, grid_y + 40, 2):
        canvas.point(cx, y)

    # 4-quadrant param display right side
    params = [("SPRD", spread), ("BIAS", bias), ("STEP", sequence.marbles_steps()), ("MODE", sequence.marbles_mode())]
    for i, (name, val) in enumerate(params):
        y = 16 + i * 10
        canvas.set_color(Color.Bright if i == slot else Color.Medium)
        canvas.draw_text(60, y + 5, name)
        canvas.draw_text(100, y + 5, str(val))

# ---------------------------------------------------------------------------
# Lock variants
# ---------------------------------------------------------------------------

def render_stochastic_lock_meadowphysics(canvas: Canvas, sequence, track_engine):
    """
    meadowphysics-derived multi-voice tracker strips.
    4 rows of 2x2 dots = gate, note, length, retrigger of captured events.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK MP")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])

    current_step = track_engine.current_step()
    dot_size = 2
    padding = 6
    offset_x = 12
    offset_y = 16
    first = sequence.lock_first()
    last = sequence.lock_last()
    rows = [
        ("G", lambda e: e.gate()),
        ("N", lambda e: e.note()),
        ("L", lambda e: e.length()),
        ("R", lambda e: e.retrigger()),
    ]

    canvas.set_blend_mode(BlendMode.Set)
    for row_idx, (label, getter) in enumerate(rows):
        row_y = offset_y + padding * row_idx
        canvas.set_color(Color.Medium)
        canvas.draw_text(2, row_y + 2, label)

        for i in range(16):
            evt = sequence.lock_event(i)
            x = offset_x + padding * i
            val = getter(evt)
            is_current = (i == current_step)
            in_window = first <= i <= last

            if is_current:
                canvas.set_color(Color.Bright)
            elif in_window and val:
                canvas.set_color(Color.MediumBright)
            elif in_window:
                canvas.set_color(Color.Medium)
            else:
                canvas.set_color(Color.Low)

            canvas.fill_rect(x, row_y, dot_size, dot_size)


def render_stochastic_lock_pit_orchisstra(canvas: Canvas, sequence, track_engine):
    """
    pit-orchisstra-derived snake trail as event history.
    Captured events drawn as a snake path. Each segment = one event.
    """
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, "LOCK SNAKE")
    WindowPainter.draw_footer(canvas, ["LOCK", "FIRST", "LAST", "ROT", "CLEAR"])

    first = sequence.lock_first()
    last = sequence.lock_last()
    current_step = track_engine.current_step()

    # Grid background
    grid_x0 = 8
    grid_y0 = 14
    cell_size = 4
    canvas.set_blend_mode(BlendMode.Set)

    for row in range(8):
        for col in range(8):
            x = grid_x0 + col * cell_size
            y = grid_y0 + row * cell_size
            canvas.set_color(Color.Low)
            canvas.draw_rect(x, y, cell_size - 1, cell_size - 1)

    # Snake path through captured events
    x, y = 0, 0
    canvas.set_color(Color.Bright)
    for i in range(first, last + 1):
        evt = sequence.lock_event(i)
        gx = grid_x0 + x * cell_size + 1
        gy = grid_y0 + y * cell_size + 1
        if evt.gate():
            canvas.set_color(Color.Bright)
        else:
            canvas.set_color(Color.Medium)
        canvas.fill_rect(gx, gy, 2, 2)
        # Wiggle forward
        x = (x + 1) % 8
        if x == 0:
            y = (y + 1) % 8

    # Current step cursor
    cx = grid_x0 + (current_step % 8) * cell_size + 1
    cy = grid_y0 + (current_step // 8) * cell_size + 1
    canvas.set_color(Color.Bright)
    canvas.draw_rect(cx - 1, cy - 1, 4, 4)

# ---------------------------------------------------------------------------
# Track variants
# ---------------------------------------------------------------------------

def render_stochastic_track_bline(canvas: Canvas, sequence, track_engine,
                                   current_page: int = 0, selected_slot: int = 0):
    """
    bline-derived 4-quadrant 2x2 parameter grid.
    Each cell = 19x19 px. Selected = bright background.
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    cell_w = 19
    cell_h = 19
    grid_x = 10
    grid_y = 14

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK BLN P{current_page+1}")

    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr) in enumerate(page):
        col = i % 2
        row = i // 2
        x = grid_x + col * (cell_w + 1)
        y = grid_y + row * (cell_h + 1)
        val = getattr(sequence, attr)()
        selected = (i == selected_slot)

        # Cell background
        if selected:
            canvas.set_color(Color.Medium)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Bright
        else:
            canvas.set_color(Color.Low)
            canvas.fill_rect(x, y, cell_w, cell_h)
            text_color = Color.Medium

        # Label
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 6, name)
        # Value
        canvas.set_color(text_color)
        canvas.draw_text(x + 2, y + 14, str(val))

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)


def render_stochastic_track_pitter_patter(canvas: Canvas, sequence, track_engine,
                                           current_page: int = 0, selected_slot: int = 0):
    """
    pitter-patter-derived grid squares + parameter text.
    4 columns with 5x5 brightness square + value below.
    """
    param_pages = [
        [("OCT", "octave"), ("TRANS", "transpose"), ("DIV", "division"), ("RESET", "reset_mode")],
        [("GBIAS", "gbias"), ("NBIAS", "nbias"), ("LBIAS", "lbias"), ("RBIAS", "rbias")],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]
    col_width = 51

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK PP P{current_page+1}")

    canvas.set_blend_mode(BlendMode.Set)
    for i, (name, attr) in enumerate(page):
        x = i * col_width
        val = getattr(sequence, attr)()
        selected = (i == selected_slot)

        # 5x5 brightness square
        sq_x = x + (col_width - 5) // 2
        sq_y = 20
        if selected:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(sq_x, sq_y, 5, 5)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(sq_x, sq_y, 5, 5)

        # Label
        canvas.set_color(Color.Bright if selected else Color.Medium)
        canvas.draw_text(x + (col_width - canvas.text_width(name)) // 2, 30, name)

        # Value
        val_str = str(val)
        canvas.set_color(Color.Bright)
        canvas.draw_text(x + (col_width - canvas.text_width(val_str)) // 2, 38, val_str)

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)



def render_stochastic_track_sequencex(canvas: Canvas, sequence, track_engine,
                                       current_page: int = 0, selected_slot: int = 0):
    """
    SequenceX-derived vertical slider columns.
    Dotted line per param + 5x3 rect indicator at value.
    """
    param_pages = [
        [("OCT", "octave", -4, 4), ("TRANS", "transpose", -12, 12),
         ("DIV", "division", 1, 16), ("RESET", "reset_mode", 0, 4)],
        [("GBIAS", "gbias", -50, 50), ("NBIAS", "nbias", -50, 50),
         ("LBIAS", "lbias", -50, 50), ("RBIAS", "rbias", -50, 50)],
    ]

    page = param_pages[current_page] if current_page < len(param_pages) else param_pages[0]

    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="STOCH")
    WindowPainter.draw_active_function(canvas, f"TRACK SX P{current_page+1}")

    slider_y0 = 18
    slider_h = 36
    col_width = 51
    zero_y = slider_y0 + slider_h // 2

    canvas.set_blend_mode(BlendMode.Set)

    for i, (name, attr, minv, maxv) in enumerate(page):
        x = i * col_width + col_width // 2
        val = getattr(sequence, attr)()

        # Dotted zero line
        canvas.set_color(Color.Low)
        for dx in range(x - 2, x + 3, 2):
            canvas.point(dx, zero_y)

        # Dotted slider line
        canvas.set_color(Color.Medium)
        for y in range(slider_y0, slider_y0 + slider_h, 2):
            canvas.point(x, y)

        # Value indicator rect (5x3)
        range_span = maxv - minv
        pos_y = slider_y0 + slider_h - 2 - ((val - minv) * (slider_h - 4) // range_span)
        pos_y = max(slider_y0, min(slider_y0 + slider_h - 3, pos_y))

        if i == selected_slot:
            canvas.set_color(Color.Bright)
            canvas.fill_rect(x - 2, pos_y, 5, 3)
        else:
            canvas.set_color(Color.Medium)
            canvas.draw_rect(x - 2, pos_y, 5, 3)

        # Label
        canvas.set_color(Color.Bright if i == selected_slot else Color.Medium)
        canvas.draw_text(x - canvas.text_width(name) // 2, slider_y0 + slider_h + 2, name)

        # Value text
        canvas.set_color(Color.Bright)
        canvas.draw_text(x - canvas.text_width(str(val)) // 2, slider_y0 - 4, str(val))

    fn_names = [p[0] for p in page] + ["NEXT"]
    WindowPainter.draw_footer(canvas, fn_names, highlight=selected_slot)

