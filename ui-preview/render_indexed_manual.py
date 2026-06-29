#!/usr/bin/env python3
"""
Indexed track OLED renders for INDEXED_MANUAL.html — actual-state, ground-truth.

Each page mirrors the firmware draw routines line-for-line:
  1) edit   — IndexedSequenceEditPage::draw: timeline bar (width=duration,
              inner fill=gate ticks) + info/edit rows + DUR/GATE/NOTE/SEQ/MATH footer.
  2) edit-groups — same page in Groups mode (A B C D footer, member counts).
  3) math   — IndexedMathPage::draw: slots A/B, columns TARGET/OP/VALUE/GROUPS/N=.
  4) routes — IndexedRouteConfigPage::draw: Route A/B + Mix row, columns
              SOURCE/GROUPS/TARGET/AMOUNT/N=.

Standalone — does not touch generate.py. Outputs into ui-preview/indexed-manual/.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import WindowPainter

W, H = 256, 64
COL_W = W // 5  # CONFIG_LCD_WIDTH / CONFIG_FUNCTION_KEY_COUNT = 51


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def _new():
    fb = FrameBuffer(W, H)
    c = Canvas(fb, brightness=1.0)
    c.set_font(Font.Tiny)
    return fb, c


# ---------------------------------------------------------------------------
# Shared: group-mask drawing (firmware drawGroupMask in both config pages)
# ---------------------------------------------------------------------------
def draw_group_mask(c, group_mask, x, y, width, on_color):
    # 0 = ALL, 0x10 = UNGR, 0x20 = SEL (Math only)
    if group_mask == 0x10:
        label = "UNGR"
        c.set_color(on_color)
        c.draw_text(x + (width - c.text_width(label)) // 2, y, label)
        return
    if group_mask == 0x20:
        label = "SEL"
        c.set_color(on_color)
        c.draw_text(x + (width - c.text_width(label)) // 2, y, label)
        return
    if group_mask == 0:
        label = "ALL"
        c.set_color(on_color)
        c.draw_text(x + (width - c.text_width(label)) // 2, y, label)
        return
    labels = ["A", "B", "C", "D"]
    group_width = 4 * 8
    start_x = x + (width - group_width) // 2
    for i in range(4):
        in_group = (group_mask & (1 << i)) != 0
        c.set_color(on_color if in_group else Color.Low)
        c.draw_text(start_x + i * 8, y, labels[i])


def draw_centered(c, col, y, text, color):
    col_x = col * COL_W
    x = col_x + (COL_W - c.text_width(text)) // 2
    c.set_color(color)
    c.draw_text(x, y, text)


# ---------------------------------------------------------------------------
# Gate-tick math (mirrors IndexedSequence::gateTicks). gateValue stored 0..1023
# as permille of duration; 0 = OFF.
# ---------------------------------------------------------------------------
def gate_ticks(gate_value, duration_ticks):
    if gate_value == 0:
        return 0
    gv = clamp(gate_value, 1, 1023)
    ticks = int((duration_ticks * gv) / 1023)
    return clamp(ticks, 1, duration_ticks) if duration_ticks > 0 else 0


# ---------------------------------------------------------------------------
# 1) Edit page — IndexedSequenceEditPage::draw (timeline + info/edit rows)
# ---------------------------------------------------------------------------
# step = (duration_ticks, gate_value_permille, note_index)
def render_edit(c, steps, current_step, selection, section=0, section_count=3,
                divisor=12, sel_dur=None, sel_gate=None, sel_note="", info_note=None):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=0, mode=f"INDEXED EDIT {section + 1}/{section_count}")
    c.set_blend_mode(BlendMode.Set)

    active_length = len(steps)

    # timeline totals (no current-step modulation in static render)
    total_ticks = 0
    nonzero = 0
    for i, (dur, gv, ni) in enumerate(steps):
        d = dur if i == current_step else dur * divisor
        total_ticks += d
        if d > 0:
            nonzero += 1

    if total_ticks > 0 and nonzero > 0:
        bar_x, bar_y, bar_w, bar_h = 8, 14, 240, 16
        min_step_w = 7
        two_rows = (min_step_w * nonzero) > bar_w
        row_h = bar_h // 2 if two_rows else bar_h
        total_w = bar_w * 2 if two_rows else bar_w

        current_x = bar_x
        row = 0
        extra_pixels = max(0, total_w - min_step_w * nonzero)
        error = 0

        for i, (dur, gv, ni) in enumerate(steps):
            active = (i == current_step)
            d = dur if active else dur * divisor

            step_w = 0
            if d > 0:
                scaled = extra_pixels * d + error
                extra_w = scaled // total_ticks
                error = scaled % total_ticks
                step_w = min_step_w + extra_w

            selected = i in selection

            gate_w = 0
            if d > 0:
                gt = min(gate_ticks(gv, dur) * divisor, d) if not active else gate_ticks(gv, dur)
                gate_w = int((step_w * gt) / d)
            gate_w = clamp(gate_w, 0, max(0, step_w - 2))

            def draw_segment(seg_x, seg_y, seg_w, gate_remaining, x_offset):
                if seg_w <= 0:
                    return gate_remaining
                draw_x = seg_x + x_offset
                draw_w = seg_w - x_offset
                if draw_x < bar_x:
                    shift = bar_x - draw_x
                    draw_x = bar_x
                    draw_w = max(0, draw_w - shift)
                c.set_color(Color.Bright if selected else (Color.MediumBright if active else Color.Medium))
                c.draw_rect(draw_x, seg_y, draw_w, row_h)
                if gate_remaining > 0 and draw_w > 2 and row_h > 2:
                    gate_seg = min(gate_remaining, draw_w - 2)
                    c.set_color(Color.Bright if selected else (Color.MediumBright if active else Color.Low))
                    c.fill_rect(draw_x + 1, seg_y + 1, gate_seg, row_h - 2)
                    gate_remaining -= gate_seg
                return gate_remaining

            seg_gate = gate_w
            seg_row = row
            seg_x = current_x
            remaining = step_w
            x_offset = -1 if i > 0 else 0
            while remaining > 0:
                row_y = bar_y + seg_row * row_h + (-1 if seg_row == 1 else 0)
                row_remaining = (bar_x + bar_w) - seg_x
                seg_w = min(remaining, row_remaining)
                seg_gate = draw_segment(seg_x, row_y, seg_w, seg_gate, x_offset)
                remaining -= seg_w
                if remaining > 0 and two_rows and seg_row == 0:
                    seg_row = 1
                    seg_x = bar_x
                    continue
                break

            current_x += step_w
            if two_rows and current_x >= bar_x + bar_w:
                current_x = bar_x + (current_x - (bar_x + bar_w))
                row = 1

    # bottom info/edit rows
    if selection:
        # info line (bars + deltas) — render an example string
        if info_note is None:
            info_note = "BARS 2.0  DT +0 / +0"
        c.set_font(Font.Tiny)
        c.set_color(Color.Medium)
        c.draw_text_centered(0, 32, 256, 8, info_note)

        y = 40
        c.set_font(Font.Small)
        c.set_color(Color.Bright)
        c.draw_text_centered(0, y, 51, 16, str(sel_dur))
        c.draw_text_centered(51, y, 51, 16, str(sel_gate))
        c.draw_text_centered(102, y, 51, 16, sel_note)
    else:
        if info_note is None:
            info_note = "STEP 1/16  48  24  3.00 C3"
        c.set_font(Font.Tiny)
        c.set_color(Color.MediumBright)
        c.draw_text_centered(0, 32, 256, 8, info_note)

    footer = ["DUR", "GATE", "NOTE", "SEQ", "MATH"]
    WindowPainter.draw_footer(c, footer, highlight=0)  # DUR mode active


# ---------------------------------------------------------------------------
# Edit page — Groups mode (A B C D footer, per-group member counts)
# ---------------------------------------------------------------------------
def render_edit_groups(c, steps, focus_mask, group_counts, section=0, section_count=3,
                       current_step=0, selection=(), divisor=12):
    # reuse timeline render then overdraw the groups bottom rows
    render_edit(c, steps, current_step, set(selection), section, section_count, divisor,
                sel_dur=0, sel_gate=0, sel_note="")
    # blank the bottom info area + footer band, then draw groups bottom + footer
    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.None_)
    c.fill_rect(0, 31, 256, 33)

    y = 40
    c.set_font(Font.Small)
    c.set_blend_mode(BlendMode.Set)
    labels = ["A", "B", "C", "D"]
    for i in range(4):
        in_group = (focus_mask & (1 << i)) != 0
        c.set_color(Color.Medium)
        c.draw_text_centered(i * 51, y - 8, 51, 8, str(group_counts[i]))
        gtext = f"[{labels[i]}]" if in_group else "[-]"
        c.draw_text_centered(i * 51, y, 51, 16, gtext)

    footer = ["A", "B", "C", "D", "BACK"]
    WindowPainter.draw_footer(c, footer, highlight=-1)


# ---------------------------------------------------------------------------
# 3) Math page — IndexedMathPage::draw
# ---------------------------------------------------------------------------
# cfg = dict(target, op, value_str, group_mask, n)
def _draw_math_config(c, cfg, y, active, label, edit_param):
    c.set_font(Font.Tiny)
    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.Bright if active else Color.Medium)
    c.draw_text(2, y, label)

    draw_centered(c, 0, y, cfg["target"],
                  Color.Bright if (active and edit_param == 0) else Color.Medium)
    draw_centered(c, 1, y, cfg["op"],
                  Color.Bright if (active and edit_param == 1) else Color.Medium)
    draw_centered(c, 2, y, cfg["value_str"],
                  Color.Bright if (active and edit_param == 2) else Color.Medium)
    group_color = Color.Bright if (active and edit_param == 3) else Color.Medium
    draw_group_mask(c, cfg["group_mask"], COL_W * 3, y, COL_W, group_color)
    draw_centered(c, 4, y, f"N={cfg['n']}", Color.Bright if active else Color.Medium)


def render_math(c, op_a, op_b, active_op=0, edit_param=0, commit=True):
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=0, mode="MATH")
    _draw_math_config(c, op_a, 16, active_op == 0, "A", edit_param)
    _draw_math_config(c, op_b, 36, active_op == 1, "B", edit_param)
    footer = ["TARGET", "OP", "VALUE", "GROUPS", "COMMIT" if commit else "BACK"]
    WindowPainter.draw_footer(c, footer, highlight=edit_param)


# ---------------------------------------------------------------------------
# 4) Route Config page — IndexedRouteConfigPage::draw
# ---------------------------------------------------------------------------
# cfg = dict(source, group_mask, target, amount_str, n)
def _draw_route_config(c, cfg, y, active, label, edit_param):
    c.set_font(Font.Tiny)
    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.Bright if active else Color.Medium)
    c.draw_text(2, y, label)

    # col0 SOURCE (edit_param Enabled == 0)
    draw_centered(c, 0, y, cfg["source"],
                  Color.Bright if (edit_param == 0 and active) else Color.Medium)
    # col4 N=
    draw_centered(c, 4, y, f"N={cfg['n']}", Color.Bright if active else Color.Medium)
    # col1 group mask (always Bright/Low in firmware route drawGroupMask)
    draw_group_mask(c, cfg["group_mask"], COL_W, y, COL_W, Color.Bright)
    # col2 TARGET (edit_param TargetParam == 2)
    draw_centered(c, 2, y, cfg["target"],
                  Color.Bright if (edit_param == 2 and active) else Color.Medium)
    # col3 AMOUNT (edit_param Amount == 3)
    draw_centered(c, 3, y, cfg["amount_str"],
                  Color.Bright if (edit_param == 3 and active) else Color.Medium)


def _draw_mix_config(c, mode_name, y, active):
    c.set_font(Font.Tiny)
    c.set_blend_mode(BlendMode.Set)
    c.set_color(Color.Bright if active else Color.Medium)
    c.draw_text(2, y, "MIX")
    draw_centered(c, 2, y, mode_name, Color.Bright if active else Color.Medium)


def render_routes(c, route_a, route_b, mix_name, active=0, edit_param=0, commit=True):
    # active: 0 RouteA, 1 RouteB, 2 Mix
    WindowPainter.clear(c)
    WindowPainter.draw_header(c, track=0, mode="ROUTE CONFIG")
    _draw_route_config(c, route_a, 16, active == 0, "A", edit_param)
    _draw_route_config(c, route_b, 36, active == 1, "B", edit_param)
    _draw_mix_config(c, mix_name, 46, active == 2)
    footer = ["SOURCE", "GROUPS", "TARGET", "AMOUNT", "COMMIT" if commit else "BACK"]
    highlight = -1 if active == 2 else edit_param
    WindowPainter.draw_footer(c, footer, highlight=highlight)


# ---------------------------------------------------------------------------
def main():
    here = os.path.dirname(os.path.abspath(__file__))
    outdir = os.path.join(here, "indexed-manual")
    os.makedirs(outdir, exist_ok=True)

    # Example 8-step sequence: varied durations (ticks), gate permille, note index.
    # divisor 12 -> programmed durations get ×12 on the timeline.
    seq = [
        (48, 1023, 0),   # whole-ish, full gate
        (24, 512, 2),
        (24, 250, 4),
        (12, 1023, 5),
        (36, 700, 7),
        (24, 0, 9),      # gate OFF
        (12, 1023, 11),
        (48, 400, 12),
    ]

    def edit_default(c):
        render_edit(c, seq, current_step=3, selection=set(), section=0,
                    info_note="STEP 4/8  12  12  3.42 F3")

    def edit_selected(c):
        render_edit(c, seq, current_step=3, selection={1}, section=0,
                    sel_dur=24, sel_gate=12, sel_note="3.17 D3",
                    info_note="BARS 2.5  DT +6 / -2")

    def edit_groups(c):
        render_edit_groups(c, seq, focus_mask=0b0101, group_counts=[3, 2, 1, 0],
                           section=0, current_step=3, selection={1})

    def math(c):
        op_a = {"target": "DUR", "op": "MUL", "value_str": "2", "group_mask": 0, "n": 8}
        op_b = {"target": "NOTE", "op": "RAMP", "value_str": "12", "group_mask": 0b0001, "n": 3}
        render_math(c, op_a, op_b, active_op=0, edit_param=1, commit=True)

    def math_groups(c):
        op_a = {"target": "GATE", "op": "JIT", "value_str": "5", "group_mask": 0x20, "n": 2}
        op_b = {"target": "DUR", "op": "QNT", "value_str": "24", "group_mask": 0x10, "n": 5}
        render_math(c, op_a, op_b, active_op=1, edit_param=3, commit=True)

    def routes(c):
        ra = {"source": "A", "group_mask": 0, "target": "DUR", "amount_str": "+100%", "n": 8}
        rb = {"source": "B", "group_mask": 0b0010, "target": "NOTE", "amount_str": "+50%", "n": 2}
        render_routes(c, ra, rb, "AtoB", active=0, edit_param=0, commit=True)

    def routes_mix(c):
        ra = {"source": "A", "group_mask": 0, "target": "DUR", "amount_str": "+100%", "n": 8}
        rb = {"source": "B", "group_mask": 0, "target": "DUR", "amount_str": "-75%", "n": 8}
        render_routes(c, ra, rb, "MAX", active=2, edit_param=0, commit=True)

    pages = {
        "indexed-edit": edit_default,
        "indexed-edit-selected": edit_selected,
        "indexed-edit-groups": edit_groups,
        "indexed-math": math,
        "indexed-math-groups": math_groups,
        "indexed-routes": routes,
        "indexed-routes-mix": routes_mix,
    }
    for slug, fn in pages.items():
        fb, c = _new()
        fn(c)
        framebuffer_to_image(fb, scale=4).save(os.path.join(outdir, f"{slug}.png"))
        print(f"saved {slug}")


if __name__ == "__main__":
    main()
