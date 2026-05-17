"""
Stub model classes and track preview draw functions.

These replicate the interfaces used by OverviewPage.cpp so the drawing code
can be ported line-for-line from the firmware.
"""

from canvas import BlendMode, Canvas, Color


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


# =============================================================================
# Note Track
# =============================================================================
class NoteSequenceStep:
    def __init__(self, gate=True, gate_probability=7, gate_offset=0, retrigger=0,
                 retrigger_probability=7, length=3, length_variation_range=0,
                 length_variation_probability=0, note=0, note_variation_range=0,
                 note_variation_probability=0, slide=False, accumulator_step_value=0,
                 pulse_count=0, gate_mode=0, harmony_role_override=0,
                 inversion_override=0, voicing_override=0, condition=0):
        self._gate = gate
        self._gate_probability = gate_probability
        self._gate_offset = gate_offset
        self._retrigger = retrigger
        self._retrigger_probability = retrigger_probability
        self._length = length
        self._length_variation_range = length_variation_range
        self._length_variation_probability = length_variation_probability
        self._note = note
        self._note_variation_range = note_variation_range
        self._note_variation_probability = note_variation_probability
        self._slide = slide
        self._accumulator_step_value = accumulator_step_value
        self._pulse_count = pulse_count
        self._gate_mode = gate_mode
        self._harmony_role_override = harmony_role_override
        self._inversion_override = inversion_override
        self._voicing_override = voicing_override
        self._condition = condition

    def gate(self): return self._gate
    def gate_probability(self): return self._gate_probability
    def gate_offset(self): return self._gate_offset
    def retrigger(self): return self._retrigger
    def retrigger_probability(self): return self._retrigger_probability
    def length(self): return self._length
    def length_variation_range(self): return self._length_variation_range
    def length_variation_probability(self): return self._length_variation_probability
    def note(self): return self._note
    def note_variation_range(self): return self._note_variation_range
    def note_variation_probability(self): return self._note_variation_probability
    def slide(self): return self._slide
    def accumulator_step_value(self): return self._accumulator_step_value
    def pulse_count(self): return self._pulse_count
    def gate_mode(self): return self._gate_mode
    def harmony_role_override(self): return self._harmony_role_override
    def inversion_override(self): return self._inversion_override
    def voicing_override(self): return self._voicing_override
    def condition(self): return self._condition


class NoteSequence:
    def __init__(self, gates=None, steps=None, first_step=0, last_step=15):
        if steps is not None:
            self._steps = steps
        elif gates is not None:
            self._steps = [NoteSequenceStep(g) for g in gates]
        else:
            self._steps = [NoteSequenceStep() for _ in range(64)]
        self._first_step = first_step
        self._last_step = last_step

    def step(self, index):
        return self._steps[index % len(self._steps)]

    def first_step(self):
        return self._first_step

    def last_step(self):
        return self._last_step


class NoteTrackEngine:
    def __init__(self, current_step=0):
        self._current_step = current_step

    def current_step(self):
        return self._current_step


def draw_note_track(canvas: Canvas, track_index: int, track_engine: NoteTrackEngine, sequence: NoteSequence):
    canvas.set_blend_mode(BlendMode.Set)
    step_offset = (max(0, track_engine.current_step()) // 16) * 16
    y = track_index * 8
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = 64 + i * 8
        if track_engine.current_step() == step_index:
            canvas.set_color(Color.Bright if step.gate() else Color.MediumBright)
            canvas.fill_rect(x + 1, y + 1, 6, 6)
        else:
            canvas.set_color(Color.Medium if step.gate() else Color.Low)
            canvas.fill_rect(x + 1, y + 1, 6, 6)


# =============================================================================
# Tuesday Track
# =============================================================================
class TuesdaySequence:
    def __init__(self, algorithm=0, loop_length=16, flow=8, ornament=8, power=8,
                 rotate=0, glide=0, skew=0, gate_length=50, gate_offset=0,
                 step_trill=0, start=0):
        self._algorithm = algorithm
        self._loop_length = loop_length
        self._flow = flow
        self._ornament = ornament
        self._power = power
        self._rotate = rotate
        self._glide = glide
        self._skew = skew
        self._gate_length = gate_length
        self._gate_offset = gate_offset
        self._step_trill = step_trill
        self._start = start

    def algorithm(self): return self._algorithm
    def actual_loop_length(self): return self._loop_length
    def loop_length(self): return self._loop_length
    def flow(self): return self._flow
    def ornament(self): return self._ornament
    def power(self): return self._power
    def rotate(self): return self._rotate
    def glide(self): return self._glide
    def skew(self): return self._skew
    def gate_length(self): return self._gate_length
    def gate_offset(self): return self._gate_offset
    def step_trill(self): return self._step_trill
    def start(self): return self._start


class TuesdayTrackEngine:
    def __init__(self, current_step=0, gate_output=False):
        self._current_step = current_step
        self._gate_output = gate_output

    def current_step(self):
        return self._current_step

    def gate_output(self, channel):
        return self._gate_output


_ALGO_NAMES = [
    "Test", "TriTr", "Stomp", "Marko", "Chip1", "Chip2",
    "Wobbl", "SclWk", "Wndow", "Minml", "Ganz", "Blake",
    "Aphex", "Autec", "StpWv",
]


def draw_tuesday_track(canvas: Canvas, track_index: int, track_engine: TuesdayTrackEngine, tuesday_sequence: TuesdaySequence):
    canvas.set_blend_mode(BlendMode.Set)
    y = track_index * 8
    current_step = track_engine.current_step()
    loop_length = tuesday_sequence.actual_loop_length()
    current_step_for_display = current_step
    if loop_length > 0:
        current_step_for_display = current_step % loop_length

    gate_active = track_engine.gate_output(0)
    canvas.set_color(Color.Bright if gate_active else Color.Low)
    canvas.fill_rect(64 + 1, y + 1, 6, 6)

    canvas.set_color(Color.Medium)
    if current_step >= 0:
        if loop_length > 0:
            canvas.draw_text(64 + 12, y + 5, f"{current_step_for_display + 1}/{loop_length}")
        else:
            canvas.draw_text(64 + 12, y + 5, f"{current_step + 1}")

    algo = tuesday_sequence.algorithm()
    algo_name = _ALGO_NAMES[algo] if 0 <= algo < len(_ALGO_NAMES) else ""
    if algo_name:
        canvas.set_color(Color.Medium)
        tw = canvas.text_width(algo_name)
        canvas.draw_text(192 - tw, y + 5, algo_name)


# =============================================================================
# DiscreteMap Track
# =============================================================================
class DiscreteMapSequence:
    StageCount = 32

    class Stage:
        class TriggerDir:
            Rise = 0
            Fall = 1
            Off = 2
            Both = 3

        def __init__(self, threshold=0, direction=TriggerDir.Off, note_index=0):
            self._threshold = threshold
            self._direction = direction
            self._note_index = note_index

        def threshold(self):
            return self._threshold

        def direction(self):
            return self._direction

        def note_index(self):
            return self._note_index

    def __init__(self, stages=None, range_low=-5.0, range_high=5.0):
        if stages is None:
            stages = []
        self._stages = stages + [self.Stage() for _ in range(self.StageCount - len(stages))]
        self._range_low = range_low
        self._range_high = range_high

    def stage(self, index):
        return self._stages[index]

    def range_low(self):
        return self._range_low

    def range_high(self):
        return self._range_high


class DiscreteMapTrackEngine:
    def __init__(self, active_stage=-1, current_input=0.0):
        self._active_stage = active_stage
        self._current_input = current_input

    def active_stage(self):
        return self._active_stage

    def current_input(self):
        return self._current_input


def draw_discrete_map_track(canvas: Canvas, track_index: int, track_engine: DiscreteMapTrackEngine, sequence: DiscreteMapSequence):
    canvas.set_blend_mode(BlendMode.Set)
    y = track_index * 8
    bar_x = 64
    bar_w = 128
    bar_line_y = y + 6

    range_min = sequence.range_low()
    range_max = sequence.range_high()
    if abs(range_max - range_min) < 0.001:
        range_min = -5.0
        range_max = 5.0

    canvas.set_color(Color.Low)
    canvas.hline(bar_x, bar_line_y, bar_w)

    for i in range(DiscreteMapSequence.StageCount):
        stage = sequence.stage(i)
        if stage.direction() == DiscreteMapSequence.Stage.TriggerDir.Off:
            continue

        norm = (stage.threshold() + 100.0) / 200.0
        norm = clamp(norm, 0.0, 1.0)
        x = bar_x + int(norm * bar_w)

        active = track_engine.active_stage() == i
        marker_height = 5 if active else 3

        canvas.set_color(Color.Bright if active else Color.Medium)
        canvas.vline(x, bar_line_y - marker_height, marker_height)

    input_norm = (track_engine.current_input() - range_min) / (range_max - range_min)
    input_norm = clamp(input_norm, 0.0, 1.0)
    cursor_x = bar_x + int(input_norm * bar_w)

    if bar_x <= cursor_x < bar_x + bar_w:
        canvas.set_color(Color.Bright)
        canvas.vline(cursor_x, y, 7)


# =============================================================================
# Stochastic Track  (DESIGN ITERATION AREA)
# =============================================================================
class StochasticSequenceStep:
    def __init__(self, gate=True, gate_probability=100,
                 note=0, note_probability=100,
                 length=4, length_probability=100,
                 retrigger=0, retrigger_probability=100,
                 condition=0, slide=False):
        self._gate = gate
        self._gate_probability = gate_probability
        self._note = note
        self._note_probability = note_probability
        self._length = length
        self._length_probability = length_probability
        self._retrigger = retrigger
        self._retrigger_probability = retrigger_probability
        self._condition = condition
        self._slide = slide

    def gate(self): return self._gate
    def gate_probability(self): return self._gate_probability
    def note(self): return self._note
    def note_probability(self): return self._note_probability
    def length(self): return self._length
    def length_probability(self): return self._length_probability
    def retrigger(self): return self._retrigger
    def retrigger_probability(self): return self._retrigger_probability
    def condition(self): return self._condition
    def slide(self): return self._slide


class StochasticSequence:
    def __init__(self, steps=None,
                 # Pitch page
                 degree_tickets=None, scale_size=7, scale_root=0,
                 degree_rotation=0, mask_rotation=0,
                 linearity=50, min_degree=0, max_degree=6,
                 # Marbles page
                 marbles_spread=50, marbles_bias=0, marbles_steps=4, marbles_mode=1,
                 # Lock page
                 lock_enabled=False, lock_first=0, lock_last=15, lock_rotate=0,
                 lock_events=None,
                 # Track page
                 octave=0, transpose=0, division=1, reset_mode=0,
                 gbias=0, nbias=0, lbias=0, rbias=0,
                 accent=0, legato=0, cv_mode=0, fill=0,
                 low_octave=-1, high_octave=1, rest_prob=0, len_mod=0,
                 first_step=0, last_step=15):
        if steps is None:
            steps = [StochasticSequenceStep() for _ in range(64)]
        self._steps = steps
        self._first_step = first_step
        self._last_step = last_step

        # Pitch
        if degree_tickets is None:
            degree_tickets = [1] * scale_size
        self._degree_tickets = degree_tickets
        self._scale_size = scale_size
        self._scale_root = scale_root
        self._degree_rotation = degree_rotation
        self._mask_rotation = mask_rotation
        self._linearity = linearity
        self._min_degree = min_degree
        self._max_degree = max_degree

        # Marbles
        self._marbles_spread = marbles_spread
        self._marbles_bias = marbles_bias
        self._marbles_steps = marbles_steps
        self._marbles_mode = marbles_mode  # 0=OFF, 1=SOFT, 2=HARD

        # Lock
        self._lock_enabled = lock_enabled
        self._lock_first = lock_first
        self._lock_last = lock_last
        self._lock_rotate = lock_rotate
        if lock_events is None:
            lock_events = [StochasticSequenceStep(gate=(i % 3 == 0),
                                                   note=i % 7,
                                                   length=3 + (i % 4),
                                                   retrigger=i % 3)
                           for i in range(64)]
        self._lock_events = lock_events

        # Track
        self._octave = octave
        self._transpose = transpose
        self._division = division
        self._reset_mode = reset_mode
        self._gbias = gbias
        self._nbias = nbias
        self._lbias = lbias
        self._rbias = rbias
        self._accent = accent
        self._legato = legato
        self._cv_mode = cv_mode
        self._fill = fill
        self._low_octave = low_octave
        self._high_octave = high_octave
        self._rest_prob = rest_prob
        self._len_mod = len_mod

    def step(self, index):
        return self._steps[index % len(self._steps)]

    def first_step(self): return self._first_step
    def last_step(self): return self._last_step

    # Pitch
    def degree_tickets(self): return self._degree_tickets
    def scale_size(self): return self._scale_size
    def scale_root(self): return self._scale_root
    def degree_rotation(self): return self._degree_rotation
    def mask_rotation(self): return self._mask_rotation
    def linearity(self): return self._linearity
    def min_degree(self): return self._min_degree
    def max_degree(self): return self._max_degree

    # Marbles
    def marbles_spread(self): return self._marbles_spread
    def marbles_bias(self): return self._marbles_bias
    def marbles_steps(self): return self._marbles_steps
    def marbles_mode(self): return self._marbles_mode

    # Lock
    def lock_enabled(self): return self._lock_enabled
    def lock_first(self): return self._lock_first
    def lock_last(self): return self._lock_last
    def lock_rotate(self): return self._lock_rotate
    def lock_event(self, index): return self._lock_events[index % len(self._lock_events)]

    # Track
    def octave(self): return self._octave
    def transpose(self): return self._transpose
    def division(self): return self._division
    def reset_mode(self): return self._reset_mode
    def gbias(self): return self._gbias
    def nbias(self): return self._nbias
    def lbias(self): return self._lbias
    def rbias(self): return self._rbias
    def accent(self): return self._accent
    def legato(self): return self._legato
    def cv_mode(self): return self._cv_mode
    def fill(self): return self._fill
    def low_octave(self): return self._low_octave
    def high_octave(self): return self._high_octave
    def rest_prob(self): return self._rest_prob
    def len_mod(self): return self._len_mod


class StochasticTrackEngine:
    def __init__(self, current_step=0, current_note=0, gate_output=False):
        self._current_step = current_step
        self._current_note = current_note
        self._gate_output = gate_output

    def current_step(self): return self._current_step
    def current_note(self): return self._current_note
    def gate_output(self, channel=0): return self._gate_output


def draw_stochastic_track(canvas: Canvas, track_index: int, track_engine: StochasticTrackEngine, sequence: StochasticSequence):
    """
    Stochastic track preview for the Overview page row.

    This is the iteration area. The current default shows 16 step squares
    with brightness indicating gate probability, plus a 'STO' label.
    Modify this function to experiment with new layouts.
    """
    canvas.set_blend_mode(BlendMode.Set)
    y = track_index * 8
    step_offset = (max(0, track_engine.current_step()) // 16) * 16

    # Design budget: 128 px wide (x=64 .. 192). 16 x 8px columns = 128 px.
    for i in range(16):
        step_index = step_offset + i
        step = sequence.step(step_index)
        x = 64 + i * 8

        prob = step.gate_probability()
        if track_engine.current_step() == step_index:
            canvas.set_color(Color.Bright if prob > 50 else Color.MediumBright)
        else:
            canvas.set_color(Color.Medium if prob > 50 else Color.Low)
        canvas.fill_rect(x + 1, y + 1, 6, 6)

    # TODO: add right-side label or indicators here if desired.
    # Example: canvas.draw_text(192 - canvas.text_width("STO"), y + 5, "STO")
    # NOTE: label x must be < 184 to avoid overlapping the last 8px column.


# =============================================================================
# Other tracks (minimal or empty)
# =============================================================================
def draw_curve_track(canvas: Canvas, track_index: int, track_engine, curve_track, sequence):
    """Placeholder — curve track preview not implemented in this tool yet."""
    pass


def draw_indexed_track(canvas: Canvas, track_index: int, track_engine, sequence):
    """Placeholder — indexed track preview not implemented in this tool yet."""
    pass


def draw_teletype_track(canvas: Canvas, track_index: int, track_engine, track):
    """Placeholder — teletype track preview not implemented in this tool yet."""
    pass
