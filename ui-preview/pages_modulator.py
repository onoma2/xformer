"""
Modulator page preview renderer — proposed 'dashboard' design v2.

All parameter values visible simultaneously on the right half, arranged in
2 rows with comfortable spacing. Same Small font size as the current single-
value display. Selected value is Bright; unselected values are Medium.
Nothing else moves (waveform, header, footer, level bar stay put).
"""

import sys
import os
import math

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter


# ---------------------------------------------------------------------------
# Stub data model matching the firmware Modulator
# ---------------------------------------------------------------------------

class ModulatorShape:
    Sine = 0
    Triangle = 1
    SawUp = 2
    SawDown = 3
    Square = 4
    Random = 5
    ADSR = 6
    ChaosLorenz = 7
    ChaosLatoocarfian = 8

class ModulatorRandomMode:
    Clocked = 0
    Triggered = 1

class ModulatorMode:
    Free = 0
    Sync = 1
    Hold = 2
    Retrigger = 3


class MockModulator:
    def __init__(self, **kwargs):
        self._data = {
            'shape': ModulatorShape.Sine,
            'rate': 96,
            'depth': 25,
            'offset': 0,
            'phase': 0,
            'smooth': 100,
            'gate_track': 0,
            'random_mode': ModulatorRandomMode.Clocked,
            'mode': ModulatorMode.Free,
            'attack': 900,
            'decay': 500,
            'sustain': 100,
            'release': 200,
            'amplitude': 127,
        }
        self._data.update(kwargs)

    def shape(self): return self._data['shape']
    def rate(self): return self._data['rate']
    def depth(self): return self._data['depth']
    def offset(self): return self._data['offset']
    def phase(self): return self._data['phase']
    def smooth(self): return self._data['smooth']
    def gate_track(self): return self._data['gate_track']
    def random_mode(self): return self._data['random_mode']
    def mode(self): return self._data['mode']
    def attack(self): return self._data['attack']
    def decay(self): return self._data['decay']
    def sustain(self): return self._data['sustain']
    def release(self): return self._data['release']
    def amplitude(self): return self._data['amplitude']

    @staticmethod
    def shape_name(shape):
        names = {
            ModulatorShape.Sine: "Sine",
            ModulatorShape.Triangle: "Triangle",
            ModulatorShape.SawUp: "Saw Up",
            ModulatorShape.SawDown: "Saw Down",
            ModulatorShape.Square: "Square",
            ModulatorShape.Random: "Random",
            ModulatorShape.ADSR: "ADSR",
            ModulatorShape.ChaosLorenz: "Lorenz",
            ModulatorShape.ChaosLatoocarfian: "Latoocarfian",
        }
        return names.get(shape, "???")

    @staticmethod
    def random_mode_name(mode):
        names = {ModulatorRandomMode.Clocked: "Clocked", ModulatorRandomMode.Triggered: "Triggered"}
        return names.get(mode, "???")

    @staticmethod
    def is_chaos_shape(shape):
        return shape in (ModulatorShape.ChaosLorenz, ModulatorShape.ChaosLatoocarfian)


class MockModulatorEngine:
    def __init__(self, current_value=64, current_phase=0, adsr_state=0, adsr_timer=0):
        self._current_value = current_value
        self._current_phase = current_phase
        self._adsr_state = adsr_state
        self._adsr_timer = adsr_timer

    def current_value(self, index): return self._current_value
    def current_phase(self, index): return self._current_phase
    def adsr_state(self, index): return self._adsr_state
    def adsr_timer(self, index): return self._adsr_timer


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _format_rate(rate):
    if rate >= 96:
        div = rate / 96.0
        if abs(div - round(div)) < 0.1:
            return f"1/{int(round(div))}"
    hz = 96.0 * 2.5 / rate
    return f"{hz:.1f}Hz"


def _get_param_values(modulator, current_page):
    shape = modulator.shape()
    is_random = (shape == ModulatorShape.Random)
    is_triggered = is_random and (modulator.random_mode() == ModulatorRandomMode.Triggered)
    is_adsr = (shape == ModulatorShape.ADSR)
    is_chaos = MockModulator.is_chaos_shape(shape)

    if is_adsr:
        if current_page == 0:
            labels = ["SHAPE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"]
            values = [
                "ADSR",
                f"{modulator.attack()}ms",
                f"{modulator.decay()}ms",
                f"{modulator.sustain()}",
                f"{modulator.release()}ms",
            ]
        else:
            labels = ["AMPLIT", None, None, None, None]
            values = [
                f"{modulator.amplitude()}",
                "",
                "",
                "",
                "",
            ]
    elif is_chaos:
        if current_page == 0:
            labels = ["SHAPE", "RATE", "P1", "P2", "DEPTH"]
            values = [
                MockModulator.shape_name(shape),
                _format_rate(modulator.rate()),
                f"{modulator.attack() // 20}",
                f"{modulator.decay() // 20}",
                f"{modulator.depth()}",
            ]
        else:
            labels = ["SLEW", "OFFSET", None, None, None]
            values = [
                f"{modulator.smooth()}ms",
                f"{modulator.offset():+d}",
                "",
                "",
                "",
            ]
    else:
        labels = ["SHAPE", "R.MODE" if is_random else "RATE",
                  "G.TRACK" if is_triggered else "DEPTH",
                  "OFFSET",
                  "SLEW" if is_random else "PHASE"]
        if is_random:
            values = [
                MockModulator.shape_name(shape),
                MockModulator.random_mode_name(modulator.random_mode()),
                f"T{modulator.gate_track() + 1}",
                f"{modulator.offset():+d}",
                f"{modulator.smooth()}ms",
            ]
        else:
            values = [
                MockModulator.shape_name(shape),
                _format_rate(modulator.rate()),
                f"{modulator.depth()}",
                f"{modulator.offset():+d}",
                f"{modulator.phase()}\u00b0",
            ]

    return labels, values


# ---------------------------------------------------------------------------
# Waveform / scope / ADSR drawing (mirrors firmware)
# ---------------------------------------------------------------------------

def _draw_waveform(canvas, modulator, engine, selected_modulator=0):
    shape = modulator.shape()
    is_random = (shape == ModulatorShape.Random)
    is_adsr = (shape == ModulatorShape.ADSR)
    is_chaos = MockModulator.is_chaos_shape(shape)

    waveform_x = 4
    waveform_y = 15
    waveform_w = 116
    waveform_h = 34

    if is_chaos:
        canvas.set_color(Color.Bright)
        canvas.draw_rect(waveform_x, waveform_y, waveform_w, waveform_h)
        scope_x = waveform_x + 2
        scope_y = waveform_y + (waveform_h // 2)
        scope_width = waveform_w - 4
        scope_height = waveform_h - 4
        canvas.set_color(Color.Low)
        canvas.hline(scope_x, scope_y, scope_width)
        canvas.set_color(Color.Bright)
        current_value = engine.current_value(selected_modulator)
        y = scope_y - ((current_value - 64) * scope_height // 2) // 127
        canvas.hline(scope_x, y, scope_width)
        # Level bar
        bar_x = waveform_x + waveform_w + 4
        bar_width = 4
        canvas.set_color(Color.Medium)
        canvas.draw_rect(bar_x, waveform_y, bar_width, waveform_h)
        canvas.set_color(Color.Bright)
        level_y = waveform_y + waveform_h - 2 - ((current_value * (waveform_h - 4)) // 127)
        canvas.hline(bar_x + 1, level_y, bar_width - 2)
        canvas.hline(bar_x + 1, level_y + 1, bar_width - 2)

    elif is_adsr:
        attack_ms = modulator.attack()
        decay_ms = modulator.decay()
        sustain_level = modulator.sustain()
        release_ms = modulator.release()
        amplitude = modulator.amplitude()

        total_time = attack_ms + decay_ms + 500 + release_ms
        if total_time == 0:
            total_time = 1
        attack_width = (attack_ms * waveform_w) // total_time
        decay_width = (decay_ms * waveform_w) // total_time
        sustain_width = (500 * waveform_w) // total_time
        release_width = (release_ms * waveform_w) // total_time

        x = waveform_x
        bottom_y = waveform_y + waveform_h - 2
        peak_y = waveform_y + 2
        scaled_peak_y = bottom_y - ((bottom_y - peak_y) * amplitude) // 127
        scaled_sustain_y = bottom_y - ((bottom_y - peak_y) * sustain_level * amplitude) // (127 * 127)

        canvas.set_color(Color.Bright)
        canvas.line(x, bottom_y, x + attack_width, scaled_peak_y)
        x += attack_width
        canvas.line(x, scaled_peak_y, x + decay_width, scaled_sustain_y)
        x += decay_width
        if sustain_width > 0:
            canvas.hline(x, scaled_sustain_y, sustain_width)
        x += sustain_width
        canvas.line(x, scaled_sustain_y, x + release_width, bottom_y)

        # Playhead
        state = engine.adsr_state(selected_modulator)
        timer = engine.adsr_timer(selected_modulator)
        playhead_x = waveform_x
        if state == 0:  # Idle
            playhead_x = waveform_x
        elif state == 1:  # Attack
            attack_ticks = max(1, (attack_ms * 192) // 2500)
            progress = min(attack_width, max(0, (timer * attack_width) // attack_ticks))
            playhead_x = waveform_x + progress
        elif state == 2:  # Decay
            decay_ticks = max(1, (decay_ms * 192) // 2500)
            progress = min(decay_width, max(0, (timer * decay_width) // decay_ticks))
            playhead_x = waveform_x + attack_width + progress
        elif state == 3:  # Sustain
            playhead_x = waveform_x + attack_width + decay_width
        elif state == 4:  # Release
            release_ticks = max(1, (release_ms * 192) // 2500)
            progress = min(release_width, max(0, (timer * release_width) // release_ticks))
            playhead_x = waveform_x + attack_width + decay_width + sustain_width + progress
        canvas.set_color(Color.Medium)
        canvas.vline(playhead_x, waveform_y + 1, waveform_h - 2)

    else:
        canvas.set_color(Color.Bright)
        canvas.draw_rect(waveform_x, waveform_y, waveform_w, waveform_h)
        scope_x = waveform_x + 2
        scope_y = waveform_y + (waveform_h // 2)
        scope_width = waveform_w - 4
        scope_height = waveform_h - 4
        canvas.set_color(Color.Low)
        canvas.hline(scope_x, scope_y, scope_width)
        canvas.set_color(Color.Bright)

        if is_random:
            current_value = engine.current_value(selected_modulator)
            y = scope_y - ((current_value - 64) * scope_height // 2) // 127
            canvas.hline(scope_x, y, scope_width)
        else:
            for xi in range(scope_width - 1):
                phase = (xi * 65536) // scope_width
                phase = (phase + (modulator.phase() * 65536 // 360)) & 0xFFFF
                rad = (phase / 65536.0) * 2 * math.pi
                val = int(math.sin(rad) * 127)
                val = (val * modulator.depth()) // 127 + modulator.offset()
                val = max(-127, min(127, val))
                y1 = scope_y - (val * scope_height // 2) // 127
                y2 = y1
                if xi < scope_width - 2:
                    next_phase = ((xi + 1) * 65536) // scope_width
                    next_phase = (next_phase + (modulator.phase() * 65536 // 360)) & 0xFFFF
                    rad2 = (next_phase / 65536.0) * 2 * math.pi
                    val2 = int(math.sin(rad2) * 127)
                    val2 = (val2 * modulator.depth()) // 127 + modulator.offset()
                    val2 = max(-127, min(127, val2))
                    y2 = scope_y - (val2 * scope_height // 2) // 127
                canvas.line(scope_x + xi, y1, scope_x + xi + 1, y2)

            current_phase = engine.current_phase(selected_modulator)
            playhead_x = (current_phase * scope_width) // 65536
            canvas.set_color(Color.Medium)
            canvas.vline(scope_x + playhead_x, waveform_y + 1, waveform_h - 2)

        # Level bar
        current_value = engine.current_value(selected_modulator)
        bar_x = waveform_x + waveform_w + 4
        bar_width = 4
        canvas.set_color(Color.Medium)
        canvas.draw_rect(bar_x, waveform_y, bar_width, waveform_h)
        canvas.set_color(Color.Bright)
        level_y = waveform_y + waveform_h - 2 - ((current_value * (waveform_h - 4)) // 127)
        canvas.hline(bar_x + 1, level_y, bar_width - 2)
        canvas.hline(bar_x + 1, level_y + 1, bar_width - 2)


# ---------------------------------------------------------------------------
# Current (baseline) renderer — mirrors firmware exactly
# ---------------------------------------------------------------------------

def render_modulator_page_current(canvas, modulator, engine,
                                  selected_modulator=0, current_page=0,
                                  selected_function=0, show_routing=False):
    is_random = (modulator.shape() == ModulatorShape.Random)
    is_triggered = is_random and (modulator.random_mode() == ModulatorRandomMode.Triggered)
    is_adsr = (modulator.shape() == ModulatorShape.ADSR)
    is_chaos = MockModulator.is_chaos_shape(modulator.shape())

    if is_adsr or is_chaos:
        total_pages = 2
    else:
        total_pages = 1
    if current_page >= total_pages:
        current_page = 0

    WindowPainter.clear(canvas)

    title = f"MOD {selected_modulator + 1} - {'ROUTING' if show_routing else 'MODULATOR'}"
    WindowPainter.draw_header(canvas, track=selected_modulator, mode=title)

    if show_routing:
        routing_names = ["MODE", "GATE", "TARGET", "EVENT", "CC NUM"]
        WindowPainter.draw_footer(canvas, routing_names, highlight=int(selected_function))
    else:
        WindowPainter.draw_active_function(canvas, "")
        if is_adsr:
            if current_page == 0:
                function_names = ["SHAPE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"]
            else:
                function_names = ["AMPLIT", None, None, None, None]
        elif is_chaos:
            if current_page == 0:
                function_names = ["SHAPE", "RATE", "P1", "P2", "DEPTH"]
            else:
                function_names = ["SLEW", "OFFSET", None, None, None]
        else:
            function_names = [
                "SHAPE",
                "R.MODE" if is_random else "RATE",
                "G.TRACK" if is_triggered else "DEPTH",
                "OFFSET",
                "SLEW" if is_random else "PHASE",
            ]
        WindowPainter.draw_footer(canvas, function_names, highlight=int(selected_function))

        if total_pages > 1:
            canvas.set_color(Color.Low)
            canvas.set_font(Font.Tiny)
            page_str = f"Pg {current_page + 1}/{total_pages}"
            canvas.draw_text(56, 12, page_str)

    labels, values = _get_param_values(modulator, current_page)
    param_name = labels[int(selected_function)] or ""
    param_value = values[int(selected_function)]

    param_x = 128
    canvas.set_font(Font.Tiny)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.draw_text(param_x, 18, param_name)

    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(param_x, 30, param_value)

    _draw_waveform(canvas, modulator, engine, selected_modulator)

    if not show_routing:
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        canvas.draw_text(param_x, 44, "-> CV1,CV3")


# ---------------------------------------------------------------------------
# PROPOSED v2 renderer — 2-row spacious layout, same Small font
# ---------------------------------------------------------------------------

def render_modulator_page_proposed_v2(canvas, modulator, engine,
                                      selected_modulator=0, current_page=0,
                                      selected_function=0, show_routing=False):
    is_random = (modulator.shape() == ModulatorShape.Random)
    is_triggered = is_random and (modulator.random_mode() == ModulatorRandomMode.Triggered)
    is_adsr = (modulator.shape() == ModulatorShape.ADSR)
    is_chaos = MockModulator.is_chaos_shape(modulator.shape())

    if is_adsr or is_chaos:
        total_pages = 2
    else:
        total_pages = 1
    if current_page >= total_pages:
        current_page = 0

    WindowPainter.clear(canvas)

    title = f"MOD {selected_modulator + 1} - {'ROUTING' if show_routing else 'MODULATOR'}"
    WindowPainter.draw_header(canvas, track=selected_modulator, mode=title)

    if show_routing:
        routing_names = ["MODE", "GATE", "TARGET", "EVENT", "CC NUM"]
        WindowPainter.draw_footer(canvas, routing_names, highlight=int(selected_function))
    else:
        WindowPainter.draw_active_function(canvas, "")
        if is_adsr:
            if current_page == 0:
                function_names = ["SHAPE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"]
            else:
                function_names = ["AMPLIT", None, None, None, None]
        elif is_chaos:
            if current_page == 0:
                function_names = ["SHAPE", "RATE", "P1", "P2", "DEPTH"]
            else:
                function_names = ["SLEW", "OFFSET", None, None, None]
        else:
            function_names = [
                "SHAPE",
                "R.MODE" if is_random else "RATE",
                "G.TRACK" if is_triggered else "DEPTH",
                "OFFSET",
                "SLEW" if is_random else "PHASE",
            ]
        WindowPainter.draw_footer(canvas, function_names, highlight=int(selected_function))

        if total_pages > 1:
            canvas.set_color(Color.Low)
            canvas.set_font(Font.Tiny)
            page_str = f"Pg {current_page + 1}/{total_pages}"
            canvas.draw_text(56, 12, page_str)

    # Waveform (left half — UNCHANGED)
    _draw_waveform(canvas, modulator, engine, selected_modulator)

    # ------------------------------------------------------------------
    # PROPOSED RIGHT PANEL: 2-row layout, Small font, no labels
    # ------------------------------------------------------------------
    labels, values = _get_param_values(modulator, current_page)

    # Collect valid (non-empty) values with their original indices
    valid = [(i, v) for i, v in enumerate(values) if v]
    if not valid:
        return

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Small)

    # ------------------------------------------------------------------
    # Clean grid layout — 2 rows by default, 3 rows when Row 1 overflows
    #
    #   x=128 : left column
    #   x=190 : middle column
    #   x=256 : right-align anchor (flush to right edge)
    #
    #   Default (2 rows):
    #     Row 1 (y=22): param 0 (left)  |  param 1 (right)
    #     Row 2 (y=42): param 2 (left)  |  param 3 (mid)  |  param 4 (right)
    #
    #   Fallback (3 rows, when param 0 + param 1 don't fit side-by-side):
    #     Row 1 (y=18): param 0 (left)
    #     Row 2 (y=34): param 1 (left)  |  param 2 (right)
    #     Row 3 (y=50): param 3 (left)  |  param 4 (right)
    # ------------------------------------------------------------------
    COL_L = 128
    COL_M = 190
    COL_R = 256

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Small)

    # Determine if we need 3-row fallback
    val0 = values[0] if len(values) > 0 and values[0] else ""
    val1 = values[1] if len(values) > 1 and values[1] else ""
    w0 = canvas.text_width(val0)
    w1 = canvas.text_width(val1)
    needs_three_rows = (w0 + w1 + 8 > 128)

    if not needs_three_rows:
        # --- 2-row layout ---
        ROW1_Y = 22
        ROW2_Y = 42

        # Row 1: param 0 + param 1
        row1 = [(i, values[i]) for i in (0, 1) if i < len(values) and values[i]]
        for idx, val in row1:
            color = Color.Bright if selected_function == idx else Color.Medium
            canvas.set_color(color)
            if idx == 0:
                canvas.draw_text(COL_L, ROW1_Y, val)
            else:
                tw = canvas.text_width(val)
                canvas.draw_text(COL_R - tw, ROW1_Y, val)

        # Row 2: param 2 + param 3 + param 4
        row2 = [(i, values[i]) for i in (2, 3, 4) if i < len(values) and values[i]]
        for idx, val in row2:
            color = Color.Bright if selected_function == idx else Color.Medium
            canvas.set_color(color)
            if idx == 2:
                canvas.draw_text(COL_L, ROW2_Y, val)
            elif idx == 3:
                canvas.draw_text(COL_M, ROW2_Y, val)
            else:  # idx == 4
                tw = canvas.text_width(val)
                canvas.draw_text(COL_R - tw, ROW2_Y, val)
    else:
        # --- 3-row fallback (only for very long param 0 + param 1) ---
        ROW1_Y = 18
        ROW2_Y = 34
        ROW3_Y = 50

        # Row 1: param 0
        if val0:
            color = Color.Bright if selected_function == 0 else Color.Medium
            canvas.set_color(color)
            canvas.draw_text(COL_L, ROW1_Y, val0)

        # Row 2: param 1 + param 2
        for idx in (1, 2):
            if idx < len(values) and values[idx]:
                color = Color.Bright if selected_function == idx else Color.Medium
                canvas.set_color(color)
                val = values[idx]
                if idx == 1:
                    canvas.draw_text(COL_L, ROW2_Y, val)
                else:
                    tw = canvas.text_width(val)
                    canvas.draw_text(COL_R - tw, ROW2_Y, val)

        # Row 3: param 3 + param 4
        for idx in (3, 4):
            if idx < len(values) and values[idx]:
                color = Color.Bright if selected_function == idx else Color.Medium
                canvas.set_color(color)
                val = values[idx]
                if idx == 3:
                    canvas.draw_text(COL_L, ROW3_Y, val)
                else:
                    tw = canvas.text_width(val)
                    canvas.draw_text(COL_R - tw, ROW3_Y, val)

    # CV routing info (kept, shifted down to avoid collision)
    if not show_routing:
        canvas.set_font(Font.Tiny)
        canvas.set_blend_mode(BlendMode.Set)
        canvas.set_color(Color.Low)
        canvas.draw_text(COL_L, 54, "-> CV1,CV3")


# ---------------------------------------------------------------------------
# Convenience wrappers for generate.py
# ---------------------------------------------------------------------------

def render_modulator_current_sine(canvas):
    mod = MockModulator(shape=ModulatorShape.Sine, rate=96, depth=50, offset=+10, phase=45)
    eng = MockModulatorEngine(current_value=90, current_phase=16384)
    render_modulator_page_current(canvas, mod, eng, selected_function=1)


def render_modulator_current_adsr(canvas):
    mod = MockModulator(shape=ModulatorShape.ADSR, attack=300, decay=400, sustain=80, release=600, amplitude=100)
    eng = MockModulatorEngine(current_value=70, adsr_state=2, adsr_timer=50)
    render_modulator_page_current(canvas, mod, eng, selected_function=2, current_page=0)


def render_modulator_current_random(canvas):
    mod = MockModulator(shape=ModulatorShape.Random, random_mode=ModulatorRandomMode.Triggered,
                        depth=60, offset=-5, smooth=250, gate_track=2)
    eng = MockModulatorEngine(current_value=110)
    render_modulator_page_current(canvas, mod, eng, selected_function=0)


def render_modulator_current_chaos(canvas):
    mod = MockModulator(shape=ModulatorShape.ChaosLorenz, rate=120, depth=80, offset=+5,
                        attack=800, decay=600)
    eng = MockModulatorEngine(current_value=55)
    render_modulator_page_current(canvas, mod, eng, selected_function=3, current_page=0)


# --- PROPOSED v2 ---

def render_modulator_proposed_sine(canvas):
    mod = MockModulator(shape=ModulatorShape.Sine, rate=96, depth=50, offset=+10, phase=45)
    eng = MockModulatorEngine(current_value=90, current_phase=16384)
    render_modulator_page_proposed_v2(canvas, mod, eng, selected_function=1)


def render_modulator_proposed_adsr(canvas):
    mod = MockModulator(shape=ModulatorShape.ADSR, attack=300, decay=400, sustain=80, release=600, amplitude=100)
    eng = MockModulatorEngine(current_value=70, adsr_state=2, adsr_timer=50)
    render_modulator_page_proposed_v2(canvas, mod, eng, selected_function=2, current_page=0)


def render_modulator_proposed_random(canvas):
    mod = MockModulator(shape=ModulatorShape.Random, random_mode=ModulatorRandomMode.Triggered,
                        depth=60, offset=-5, smooth=250, gate_track=2)
    eng = MockModulatorEngine(current_value=110)
    render_modulator_page_proposed_v2(canvas, mod, eng, selected_function=0)


def render_modulator_proposed_chaos(canvas):
    mod = MockModulator(shape=ModulatorShape.ChaosLorenz, rate=120, depth=80, offset=+5,
                        attack=800, decay=600)
    eng = MockModulatorEngine(current_value=55)
    render_modulator_page_proposed_v2(canvas, mod, eng, selected_function=3, current_page=0)


def render_modulator_proposed_adsr_page2(canvas):
    mod = MockModulator(shape=ModulatorShape.ADSR, attack=300, decay=400, sustain=80, release=600, amplitude=100)
    eng = MockModulatorEngine(current_value=70)
    render_modulator_page_proposed_v2(canvas, mod, eng, selected_function=0, current_page=1)


def render_modulator_proposed_chaos_page2(canvas):
    mod = MockModulator(shape=ModulatorShape.ChaosLorenz, rate=120, depth=80, offset=+5,
                        attack=800, decay=600, smooth=200)
    eng = MockModulatorEngine(current_value=55)
    render_modulator_page_proposed_v2(canvas, mod, eng, selected_function=1, current_page=1)
