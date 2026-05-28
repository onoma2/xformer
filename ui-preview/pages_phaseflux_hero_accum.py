"""
PhaseFlux ACCUM hero set — 16-cell strip with up/down bars in the middle pane,
PTCH/TEMP-style 5-row param list on the right. Layout reference: Metropolix
ACCUM page (strip of stages with bars growing from a center line).

Slot map:
  F1 AMOUNT   per-cell signed step value (±15)
  F2 RANGE    per-cell wrap length (1..16)
  F3 ORDER    per-sequence (Wrap / Pend / Clip / RTZ)
  F4 TRIG     per-cell (Stage / Pulse)
  F5 SCOPE    per-sequence (Local / Track)
  Shift+F5    POLARITY (Uni / Bi)
"""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter

from pages_phaseflux import (
    Stage, demo_sequence,
    draw_grid,
    SCOPE_TEMP_X, SCOPE_W, SCOPE_H, SCOPE_Y,
)
from pages_phaseflux_hero import (
    PARAM_PANEL_X, PARAM_PANEL_W,
    DIVIDER_X_PARAM,
    _footer, _signed,
    draw_dividers,
)


# Sequence-level accumulator config (mock — real engine reads from sequence)
class AccumConfig:
    def __init__(self, scope="Local", order="Wrap", polarity="Uni",
                 pos_lim=7, neg_lim=5, reset_cycles=0):
        self.scope = scope        # "Local" | "Track"
        self.order = order        # "Wrap" | "Pend" | "Hold" | "RTZ"
        self.polarity = polarity  # "Uni" | "Bi"
        self.pos_lim = pos_lim    # 1..28 (note) or 1..8 (pulse)
        self.neg_lim = neg_lim
        self.reset_cycles = reset_cycles  # 0 = manual, N>0 = auto every N cycles


# Per-cell accumulator state (mock — extends Stage with what the engine reads)
# Pitch accumulator amounts (±15 scale degrees)
ACCUM_AMOUNTS = [+2, -1,  0, +3, +1, -2,  0, +4,
                 -3,  0, +1, +2, -1,  0, +5, -4]
ACCUM_RANGES  = [ 4,  3,  1,  6,  2,  4,  1,  8,
                  6,  1,  3,  4,  3,  1, 10,  8]
ACCUM_TRIGS   = ["Stage"] * 16
ACCUM_TRIGS[5] = "Pulse"
ACCUM_TRIGS[13] = "Pulse"

# Pulse-count accumulator amounts (±7 pulses) — different drift profile
PULSE_AMOUNTS = [+1,  0, +2, -1, +1, +3,  0, -2,
                 +2, +1,  0, -1, +3,  0, -2, +1]
PULSE_RANGES  = [ 3,  1,  4,  2,  3,  5,  1,  3,
                  4,  3,  1,  2,  6,  1,  3,  3]
PULSE_TRIGS   = ["Stage"] * 16
PULSE_TRIGS[2] = "Pulse"
PULSE_TRIGS[12] = "Pulse"


# Footer labels.
#   Plain row: F1=Amount, F2=+Lim, F3=-Lim, F4=Order, F5=Reset. Press selects,
#              encoder edits (continuous) or cycles (enum) or sets integer N.
#   Shift row: F1 → Trig (per-cell), F4 → Polar (per-sequence), F5 → Scope.
FOOTER_ACCUM       = ["Amount", "+Lim", "-Lim", "Order", "Reset"]
FOOTER_ACCUM_SHIFT = ["Trig",   None,   None,   "Polar", "Scope"]


# Asymmetric row layout matching the pitchmode renderer for consistency
# (5 rows; height could be uniform 8 since we don't stack values here).
PARAM_PANEL_Y = 12
PARAM_PANEL_H = 40
PARAM_ROW_H   = 8


def _draw_header(canvas: Canvas, scope: str):
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    label = "ACCUM" if scope == "Local" else "ACCUM.T"
    WindowPainter.draw_active_function(canvas, label)


def _draw_bars(canvas, positions, midline_y, bar_w, half_h, amounts, max_amt,
               active_idx, selected_idx, bright):
    """Bars at explicit per-cell x positions (4-grouped layout).
    `bright=True` → active layer (bright/medium colors). `bright=False` → dim."""
    for i in range(16):
        cell_x = positions[i]
        amt = amounts[i]
        is_active   = (i == active_idx)
        is_selected = (i == selected_idx)

        bar_h = int(abs(amt) / max_amt * half_h)
        if bar_h == 0 and amt != 0:
            bar_h = 1

        if amt > 0:
            bar_y = midline_y - bar_h
            bar_dh = bar_h
        elif amt < 0:
            bar_y = midline_y + 1
            bar_dh = bar_h
        else:
            bar_y = midline_y - 1
            bar_dh = 0

        if bright:
            color = Color.Bright if is_active else (Color.MediumBright if is_selected else Color.Medium)
        else:
            color = Color.Medium if is_active else (Color.Low if is_selected else Color.Low)
        canvas.set_color(color)
        if bar_dh > 0:
            canvas.fill_rect(cell_x, bar_y, bar_w, bar_dh)


def draw_accum_strip(canvas: Canvas, amounts, active_idx: int, selected_idx: int,
                     polarity: str):
    """16-cell strip with vertical bars growing up/down from a center line.
    Positive amounts → up; negative → down. Bar width 3 px, gap 3 px.
    """
    x0 = SCOPE_TEMP_X
    y0 = SCOPE_Y
    w  = SCOPE_W
    h  = SCOPE_H

    # Background box
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    canvas.draw_rect(x0, y0, w, h)

    # Center line (mid-bar)
    center_y = y0 + h // 2
    canvas.hline(x0 + 1, center_y, w - 2)

    # Per-cell layout: 16 cells × (bar_w + gap) ≤ inner width
    inner_w = w - 4
    cell_w  = inner_w // 16     # ~6 px each
    bar_w   = max(2, cell_w - 2)
    cell_gap = (inner_w - cell_w * 16) // 2
    x_start = x0 + 2 + cell_gap

    # Bar height scale: max |amount| = 15; vertical half-height = h/2 - 2
    half_h = (h // 2) - 2
    max_amt = 15

    for i in range(16):
        cell_x = x_start + i * cell_w
        amt = amounts[i]
        is_active   = (i == active_idx)
        is_selected = (i == selected_idx)

        # Cell underline / outline — thin tick under each cell on the centerline
        canvas.set_color(Color.Low)
        canvas.point(cell_x + bar_w // 2, center_y)

        # Bar magnitude
        bar_h = int(abs(amt) / max_amt * half_h)
        if bar_h == 0 and amt != 0:
            bar_h = 1

        # Bar direction
        if amt > 0:
            bar_y = center_y - bar_h
            bar_dh = bar_h
        elif amt < 0:
            bar_y = center_y + 1
            bar_dh = bar_h
        else:
            bar_y = center_y - 1
            bar_dh = 0  # zero amount → just the dot

        # Bar fill — bright for selected/active, medium otherwise
        if is_active:
            canvas.set_color(Color.Bright)
        elif is_selected:
            canvas.set_color(Color.MediumBright)
        else:
            canvas.set_color(Color.Medium)
        if bar_dh > 0:
            canvas.fill_rect(cell_x, bar_y, bar_w, bar_dh)

        # Tiny selected indicator: bracket below the cell baseline
        if is_selected and not is_active:
            canvas.set_color(Color.Bright)
            canvas.point(cell_x + bar_w // 2, y0 + h - 1)


def draw_accum_dual_stacked(canvas: Canvas, layer_active: str,
                            active_idx: int, selected_idx: int):
    """Two stacked strips — pitch top, pulse bottom. No outer rect; dotted
    1-px gridlines every 4 cells. Active layer bright, dim other.
    layer_active: 'P' = pitch active, 'U' = pulse active.
    """
    x0 = SCOPE_TEMP_X
    y0 = SCOPE_Y
    w  = SCOPE_W
    h  = SCOPE_H

    canvas.set_blend_mode(BlendMode.Set)

    # No boundary rect — use the full area.
    # Stack: top strip + gap + bottom strip
    gap         = 2
    strip_h     = (h - gap) // 2                       # ~18
    top_midline = y0 + strip_h // 2
    bot_midline = y0 + strip_h + gap + strip_h // 2

    # Grouped layout — bars tight within each group of 4, gap-dottedline-gap
    # at group seams.
    #   [][][][] | [][][][] | [][][][] | [][][][]
    # bar_w=3, gap_within=1 → group_w=15. inter_group=5 (2 + dotted + 2).
    badge_w        = 12
    bar_w          = 3
    gap_within     = 1
    group_w        = 4 * bar_w + 3 * gap_within        # 15
    inter_group_w  = 5                                  # 2-gap | dotted | 2-gap
    total_w        = 4 * group_w + 3 * inter_group_w    # 75
    x_start        = x0 + badge_w
    # Trailing margin to plot edge: scope_w - badge_w - total_w (~13 px)

    # Per-cell x positions (16 of them) using the grouped spacing.
    positions = []
    cursor_x = x_start
    for group in range(4):
        for c in range(4):
            positions.append(cursor_x)
            cursor_x += bar_w + gap_within
        cursor_x -= gap_within     # remove trailing within-gap of this group
        if group < 3:
            cursor_x += inter_group_w  # gap-line-gap to next group

    # Dotted gridlines at the centre of each inter-group span (3 of them).
    gridline_xs = []
    for group in range(3):
        end_of_group_x = positions[group * 4 + 3] + bar_w
        gridline_xs.append(end_of_group_x + inter_group_w // 2)

    # No midlines at all — bars float around an invisible center.

    canvas.set_color(Color.Low)
    for grid_x in gridline_xs:
        for y in range(y0, y0 + h, 2):
            canvas.point(grid_x, y)

    # Note (top)
    _draw_bars(canvas, positions, top_midline, bar_w,
               half_h=strip_h // 2 - 1, amounts=ACCUM_AMOUNTS, max_amt=15,
               active_idx=active_idx, selected_idx=selected_idx,
               bright=(layer_active == "N"))
    # Pulse (bottom)
    _draw_bars(canvas, positions, bot_midline, bar_w,
               half_h=strip_h // 2 - 1, amounts=PULSE_AMOUNTS, max_amt=7,
               active_idx=active_idx, selected_idx=selected_idx,
               bright=(layer_active == "P"))


def draw_accum_dual_interleaved(canvas: Canvas, layer_active: str,
                                 active_idx: int, selected_idx: int):
    """Single strip with paired bars per cell — pitch left, pulse right.
    Both share midline at vertical center of scope.
    """
    x0 = SCOPE_TEMP_X
    y0 = SCOPE_Y
    w  = SCOPE_W
    h  = SCOPE_H

    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Low)
    canvas.draw_rect(x0, y0, w, h)

    midline_y = y0 + h // 2
    half_h    = (h // 2) - 2

    canvas.set_color(Color.Bright if layer_active != "U" else Color.Low)  # midline bright for whichever is active
    canvas.hline(x0 + 1, midline_y, w - 2)

    # Per-cell: 2 thin bars side-by-side + 1 px gap between, 1 px between cells
    # 16 cells × (2 + 1 + 2 + 1) = 16 × 6 = 96 px, fits inner 96px
    inner_w = w - 4
    cell_w  = inner_w // 16
    bar_w   = 2
    bar_gap = 1
    x_start = x0 + 2 + (inner_w - cell_w * 16) // 2

    # Pitch bars (left of each pair)
    _draw_bars(canvas, x_start, midline_y, cell_w, bar_w,
               half_h=half_h, amounts=ACCUM_AMOUNTS, max_amt=15,
               active_idx=active_idx, selected_idx=selected_idx,
               bright=(layer_active == "N"))
    # Pulse bars (right of each pair, shifted by bar_w + gap)
    _draw_bars(canvas, x_start + bar_w + bar_gap, midline_y, cell_w, bar_w,
               half_h=half_h, amounts=PULSE_AMOUNTS, max_amt=7,
               active_idx=active_idx, selected_idx=selected_idx,
               bright=(layer_active == "U"))


def _draw_param_list(canvas: Canvas, rows, selected_idx: int,
                     cycle_slots=()):
    """5-row param list with 1/3-width value column highlight (C++ parity).
    `cycle_slots` is a tuple of row indices that are press-cycle slots —
    those never get the encoder-edit highlight rect. They just show value.
    """
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)

    value_col_w = PARAM_PANEL_W // 3
    value_col_x = PARAM_PANEL_X + PARAM_PANEL_W - value_col_w

    for i, (name, value) in enumerate(rows):
        rx = PARAM_PANEL_X
        rw = PARAM_PANEL_W
        ry = PARAM_PANEL_Y + i * PARAM_ROW_H
        rh = PARAM_ROW_H - 1
        is_selected = (i == selected_idx) and (i not in cycle_slots)

        if is_selected:
            canvas.set_color(Color.Bright)
            canvas.draw_rect(value_col_x, ry, value_col_w, rh)
            name_color  = Color.Medium
            value_color = Color.Bright
        else:
            name_color  = Color.Medium
            value_color = Color.MediumBright

        text_y = ry + 6
        canvas.set_color(name_color)
        canvas.draw_text(rx + 2, text_y, name)

        right_x = rx + rw - 2
        vw = canvas.text_width(value)
        canvas.set_color(value_color)
        canvas.draw_text(right_x - vw, text_y, value)


def _accum_rows(amount: int, pos_lim: int, neg_lim: int, order: str,
                reset_cycles: int, trig: str, scope: str, shift_held: bool,
                polarity: str):
    # F1=Amount/Trig, F2=+Lim, F3=-Lim, F4=Order/Polar, F5=Reset/Scope.
    f1_name, f1_value = ("Trig",  trig)     if shift_held else ("Amount", _signed(amount))
    f4_name, f4_value = ("Polar", polarity) if shift_held else ("Order",  order)
    reset_str = "Manual" if reset_cycles == 0 else f"{reset_cycles}cyc"
    f5_name, f5_value = ("Scope", scope)    if shift_held else ("Reset",  reset_str)
    return [
        (f1_name, f1_value),
        ("+Lim",  f"{pos_lim}"),
        ("-Lim",  f"{neg_lim}"),
        (f4_name, f4_value),
        (f5_name, f5_value),
    ]


def _draw_stage_badge(canvas: Canvas, badge: str, x: int):
    canvas.set_font(Font.Tiny)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.MediumBright)
    canvas.draw_text(x + 2, SCOPE_Y + 6, badge)


def render_accum_set(canvas, stages, cfg: AccumConfig, active_idx, selected_idx,
                     edit_slot_idx, shift_held):
    WindowPainter.clear(canvas)
    _draw_header(canvas, cfg.scope)

    footer = _footer(FOOTER_ACCUM, FOOTER_ACCUM_SHIFT, shift_held)
    WindowPainter.draw_footer(canvas, names=footer, highlight=edit_slot_idx)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_dividers(canvas)

    # Middle pane: 16-cell strip with up/down bars
    draw_accum_strip(canvas, ACCUM_AMOUNTS,
                     active_idx=active_idx, selected_idx=selected_idx,
                     polarity=cfg.polarity)

    # Badge: "n" (local) or "T" (track, shared counter visible)
    badge = "T" if cfg.scope == "Track" else f"{selected_idx}"
    _draw_stage_badge(canvas, badge, SCOPE_TEMP_X)

    # Right pane: 5-row param list. F3 Amount is per-cell; the rest are
    # per-sequence (from cfg) or active-stage Trig under Shift.
    amt = ACCUM_AMOUNTS[selected_idx]
    trig = ACCUM_TRIGS[selected_idx]
    rows = _accum_rows(amt, cfg.pos_lim, cfg.neg_lim, cfg.order,
                       cfg.reset_cycles, trig, cfg.scope, shift_held,
                       cfg.polarity)
    # All five slots take encoder input. Highlight rect on selected slot.
    _draw_param_list(canvas, rows, edit_slot_idx)


def render_accum_dual(canvas, stages, cfg: AccumConfig, active_idx, selected_idx,
                      edit_slot_idx, shift_held,
                      layer: str, strip_style: str):
    """ACCUM·P or ACCUM·U set. Both accumulators visible in the strip; the
    active layer is bright. `strip_style` = 'stacked' or 'interleaved'.
    """
    WindowPainter.clear(canvas)

    # Header: ACCUM.P or ACCUM.U + scope marker after a dot if Track
    label = f"ACCUM.{layer}"
    if cfg.scope == "Track":
        label += "T"
    WindowPainter.draw_header(canvas, track=7, play_pattern=0, edit_pattern=0,
                              mode="PHFLX")
    WindowPainter.draw_active_function(canvas, label)

    footer = _footer(FOOTER_ACCUM, FOOTER_ACCUM_SHIFT, shift_held)
    WindowPainter.draw_footer(canvas, names=footer, highlight=edit_slot_idx)

    draw_grid(canvas, stages, active_idx=active_idx, selected_idx=selected_idx)
    draw_dividers(canvas)

    if strip_style == "stacked":
        draw_accum_dual_stacked(canvas, layer, active_idx, selected_idx)
    else:
        draw_accum_dual_interleaved(canvas, layer, active_idx, selected_idx)

    # Top-left badge: stage number. Layer (N/P) lives only on the header now.
    # Track scope is also signalled on the header (ACCUM.NT / ACCUM.PT).
    badge = "T" if cfg.scope == "Track" else f"{selected_idx}"
    _draw_stage_badge(canvas, badge, SCOPE_TEMP_X)

    # Right pane reads the ACTIVE LAYER's per-cell values.
    if layer == "N":
        amt = ACCUM_AMOUNTS[selected_idx]
        trig = ACCUM_TRIGS[selected_idx]
    else:
        amt = PULSE_AMOUNTS[selected_idx]
        trig = PULSE_TRIGS[selected_idx]
    rows = _accum_rows(amt, cfg.pos_lim, cfg.neg_lim, cfg.order,
                       cfg.reset_cycles, trig, cfg.scope, shift_held,
                       cfg.polarity)
    _draw_param_list(canvas, rows, edit_slot_idx)


def main():
    stages = demo_sequence()

    out_dir = os.path.join(os.path.dirname(__file__), "phaseflux-accum")
    os.makedirs(out_dir, exist_ok=True)

    variants = [
        ("accum-local-wrap.png",
         AccumConfig(scope="Local", order="Wrap", polarity="Uni"),
         dict(active_idx=5, selected_idx=5, edit_slot_idx=0, shift_held=False)),
        ("accum-local-pend.png",
         AccumConfig(scope="Local", order="Pend", polarity="Bi"),
         dict(active_idx=5, selected_idx=14, edit_slot_idx=2, shift_held=False)),
        ("accum-track.png",
         AccumConfig(scope="Track", order="RTZ",  polarity="Uni"),
         dict(active_idx=5, selected_idx=5, edit_slot_idx=4, shift_held=False)),
        ("accum-shift-polarity.png",
         AccumConfig(scope="Track", order="Pend", polarity="Bi"),
         dict(active_idx=5, selected_idx=5, edit_slot_idx=4, shift_held=True)),
    ]

    paths = []
    for name, cfg, opts in variants:
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        render_accum_set(c, stages, cfg, **opts)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, name)
        img.save(path)
        paths.append(path)
        print(f"wrote {path}")

    # Dual-accumulator variants (pitch + pulse layers both visible)
    dual_variants = [
        ("dual-stacked-N-active.png", "stacked", "N"),
        ("dual-stacked-P-active.png", "stacked", "P"),
    ]
    dual_cfg = AccumConfig(scope="Local", order="Wrap", polarity="Bi")
    dual_opts = dict(active_idx=5, selected_idx=5, edit_slot_idx=0, shift_held=False)
    for name, style, layer in dual_variants:
        fb = FrameBuffer(PageWidth, PageHeight)
        c = Canvas(fb)
        render_accum_dual(c, stages, dual_cfg, layer=layer, strip_style=style, **dual_opts)
        img = framebuffer_to_image(fb, scale=4)
        path = os.path.join(out_dir, name)
        img.save(path)
        paths.append(path)
        print(f"wrote {path}")

    return paths


if __name__ == "__main__":
    main()
