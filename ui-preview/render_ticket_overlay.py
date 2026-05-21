#!/usr/bin/env python3
"""
Preview the ticket-edit pages with an OVERLAY showing the picker's underlying
kernel shape:

- Duration tickets page: triangular kernel from noteDuration (center) + variation
  (spread). Same math as engine pickDuration.
- Pitch tickets page: Marbles-style bell from marblesBias (center) + marblesSpread
  (width). Same math as the Marbles hero page bell.

Both overlays are drawn at Color.Low so they sit behind the bright ticket bars
without competing visually. They give the user a glimpse of what the picker
would prefer if all tickets were flat.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, Color, BlendMode, framebuffer_to_image

PAGE_W = 256
PAGE_H = 64


def draw_header(canvas: Canvas, title: str):
    canvas.set_color(Color.Bright)
    canvas.set_font(Font.Small)
    canvas.draw_text(8, 8, title)
    canvas.set_color(Color.Low)
    canvas.hline(0, 12, PAGE_W)


def draw_footer(canvas: Canvas, labels):
    canvas.set_font(Font.Tiny)
    slot_w = PAGE_W // len(labels)
    for i, lbl in enumerate(labels):
        if lbl is None:
            continue
        x = i * slot_w + 4
        canvas.set_color(Color.Medium)
        canvas.draw_text(x, PAGE_H - 2, lbl)


# Engine math --------------------------------------------------------------

def duration_kernel(slot: int, center: int, variation: int) -> int:
    """Matches pickDuration() in StochasticGenerator.cpp."""
    width = 1 + (variation * 4) // 100
    leakage = variation // 10
    dist = abs(slot - center)
    return max(0, width - dist) + leakage


def marbles_bell(x: int, cx: int, spread_pix: int, falloff: float = 1.4) -> float:
    """Matches generateDegree marbles term + Marbles hero bell."""
    if spread_pix <= 0:
        return 0.0
    d = (x - cx) / float(spread_pix)
    return math.exp(-d * d * falloff)


# Page renderers -----------------------------------------------------------

def draw_duration_page(canvas: Canvas,
                       tickets,
                       note_duration: int,
                       variation: int,
                       selected_slot: int = -1,
                       active_idx: int = -1):
    """8-slot duration ticket bars + kernel overlay."""
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)

    draw_header(canvas, "DURATION TICKETS")

    num_slots = 8
    base_y = 46
    bar_max_h = 20
    bar_w = 12
    gap = bar_w
    total_w = num_slots * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    # Kernel overlay — small dim curve in a strip ABOVE the bars. Sits in the
    # "background" by being subordinate (smaller height, dim color), not by
    # Z-order behind the bars (where wide bars would hide it).
    kernels = [duration_kernel(i, note_duration, variation) for i in range(num_slots)]
    max_k = max(kernels) if max(kernels) > 0 else 1
    overlay_top = base_y - bar_max_h - 6   # strip top
    overlay_h = 5                          # max curve height in strip
    canvas.set_color(Color.MediumLow)
    prev_x, prev_y = None, None
    for i in range(num_slots):
        x = x_offset + i * (bar_w + gap) + bar_w // 2
        h = int((kernels[i] * overlay_h) / max_k)
        y = (overlay_top + overlay_h) - h
        if prev_x is not None:
            canvas.line(prev_x, prev_y, x, y)
        canvas.point(x, y)
        prev_x, prev_y = x, y

    # Ticket bars
    max_w = max(tickets) if max(tickets) > 0 else 1
    total_w_sum = sum(tickets)
    denom = max(1, max_w)
    uniform_fallback = (total_w_sum == 0)

    for i in range(num_slots):
        x = x_offset + i * (bar_w + gap)
        weight = tickets[i]
        active = (i == active_idx)
        selected = (i == selected_slot)

        if weight == 0 and not uniform_fallback:
            canvas.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            h = bar_max_h if uniform_fallback else max(1, (weight * bar_max_h) // denom)
            if active:
                canvas.set_color(Color.Bright)
            elif selected:
                canvas.set_color(Color.MediumBright)
            else:
                canvas.set_color(Color.Low if uniform_fallback else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

        # Tiny label
        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        label = duration_label_short(i)
        lw = canvas.text_width(label)
        canvas.draw_text(x + (bar_w - lw) // 2, base_y + 8, label)

    # Info line
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    info = f"DUR {note_duration}  VAR {variation}"
    canvas.draw_text(8, 18, info)

    draw_footer(canvas, ["TIX", "DUR", "VAR", None, "NEXT"])


def draw_pitch_page(canvas: Canvas,
                    degree_tickets,
                    marbles_bias: int,
                    marbles_spread: int,
                    marbles_steps: int,
                    selected_degree: int = -1,
                    active_degree: int = -1):
    """N-slot pitch ticket bars + Marbles bell overlay."""
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)

    draw_header(canvas, "SCALE TICKETS")

    scale_size = len(degree_tickets)
    base_y = 46
    bar_max_h = 20
    bar_w = max(3, (PAGE_W - 16) // scale_size - 2)
    gap = 2
    if scale_size > 20:
        gap = 1
        bar_w = max(2, (PAGE_W - 16) // scale_size - 1)
    total_w = scale_size * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    # Bell overlay — small dim curve in a strip ABOVE the bars (matches the
    # duration page overlay placement). Sub-pixel sampling per slot, connected
    # with a thin line; this stays visible regardless of bar width.
    cx_data = (marbles_bias * (scale_size - 1) + 50) // 100        # 0..scale_size-1
    spread_slots = max(0.6, (marbles_spread * scale_size) / 100.0)  # in slot units

    overlay_top = base_y - bar_max_h - 6
    overlay_h = 5
    canvas.set_color(Color.MediumLow)
    prev_x, prev_y = None, None
    for i in range(scale_size):
        d = (i - cx_data) / spread_slots
        env = math.exp(-d * d * 1.4)
        h = int(env * overlay_h)
        if h < 1:
            h = 1
        x = x_offset + i * (bar_w + gap) + bar_w // 2
        y = (overlay_top + overlay_h) - h
        if prev_x is not None:
            canvas.line(prev_x, prev_y, x, y)
        canvas.point(x, y)
        prev_x, prev_y = x, y

    # Steps sieve: degrees that don't survive top-K are dimmed (drawn in
    # Color.Low instead of Color.Medium) so the bar visibly recedes. Bell
    # overlay above shows bias/spread; the bar's brightness shows whether it
    # passes the Steps sieve. User reads the Steps value from the info row.
    survivors = compute_steps_survivors(scale_size, marbles_steps)

    # Ticket bars
    max_t = max(degree_tickets) if max(degree_tickets) > 0 else 1
    total_t = sum(degree_tickets)
    denom = max(1, max_t)
    uniform_fallback = (total_t == 0)

    for i in range(scale_size):
        x = x_offset + i * (bar_w + gap)
        tickets = degree_tickets[i]
        active = (i == active_degree)
        selected = (i == selected_degree)
        sieved_out = (i not in survivors)

        if tickets < 0:
            canvas.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
            canvas.line(x, base_y - 5, x + bar_w - 1, base_y)
            canvas.line(x, base_y, x + bar_w - 1, base_y - 5)
        elif tickets == 0 and not uniform_fallback:
            canvas.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            h = bar_max_h if uniform_fallback else max(1, (tickets * bar_max_h) // denom)
            if active:
                canvas.set_color(Color.Bright)
            elif selected:
                canvas.set_color(Color.MediumBright)
            elif sieved_out:
                canvas.set_color(Color.Low)        # sieve-cut bar dims
            else:
                canvas.set_color(Color.Low if uniform_fallback else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

    # Info line
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    info = f"BIAS {marbles_bias}  SPRD {marbles_spread}  STEPS {marbles_steps}"
    canvas.draw_text(8, 18, info)

    draw_footer(canvas, ["TIX", "BIAS", "SPRD", "STEPS", "NEXT"])


def duration_label_short(slot: int) -> str:
    # 1/16-centered LUT for preview convenience. Matches engine getDurationFraction.
    labels = ["1/2", "1/4", "3/16", "1/8", "1/8T", "1/16", "1/16T", "1/32"]
    return labels[slot]


def universal_degree_boost(deg_in_oct: int, n: int) -> int:
    """Matches universalDegreeBoost in StochasticGenerator.cpp."""
    if n <= 1:
        return 0
    half_width = max(1, n // 6)

    def kernel(target, weight):
        dist = abs(deg_in_oct - target)
        wrap = n - dist
        if wrap < dist:
            dist = wrap
        w = (half_width - dist) * weight // half_width
        return w if w > 0 else 0

    boost = 0
    boost += kernel(0, 30)
    if n >= 2: boost += kernel(n // 2, 20)
    if n >= 3: boost += kernel(n // 3, 10)
    if n >= 3: boost += kernel((2 * n) // 3, 10)
    if n >= 4: boost += kernel(n // 4, 5)
    if n >= 4: boost += kernel((3 * n) // 4, 5)
    return boost


def compute_steps_survivors(scale_size: int, steps_knob: int):
    """Top-K by universal LUT rank; matches Steps sieve in generateDegree."""
    k = max(1, (scale_size * steps_knob + 99) // 100)
    if k > scale_size:
        k = scale_size
    ranked = sorted(range(scale_size),
                    key=lambda d: -universal_degree_boost(d, scale_size))
    return set(ranked[:k])


# Driver -------------------------------------------------------------------

def draw_duration_page_kernel_bg(canvas: Canvas,
                                 tickets,
                                 note_duration: int,
                                 variation: int,
                                 selected_slot: int = -1,
                                 active_idx: int = -1):
    """Alternative duration page: kernel silhouette as background, ticket bars
    drawn on top. Bars use absolute 0..100 scale so flat-low tickets let the
    kernel dominate visually."""
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)

    draw_header(canvas, "DURATION TICKETS")

    num_slots = 8
    base_y = 50
    bar_max_h = 28
    bar_w = 12
    gap = bar_w
    total_w = num_slots * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    # Kernel silhouette as background. Sample kernel at each slot, draw a
    # filled curve interpolated between slot centers across the bar area.
    kernels = [duration_kernel(i, note_duration, variation) for i in range(num_slots)]
    max_k = max(kernels) if max(kernels) > 0 else 1

    # Compute per-pixel kernel height across the strip via linear interpolation
    slot_centers = [x_offset + i * (bar_w + gap) + bar_w // 2 for i in range(num_slots)]
    for x in range(x_offset, x_offset + total_w):
        # Find bracketing slot centers
        if x <= slot_centers[0]:
            k_norm = kernels[0] / max_k
        elif x >= slot_centers[-1]:
            k_norm = kernels[-1] / max_k
        else:
            for j in range(num_slots - 1):
                if slot_centers[j] <= x < slot_centers[j + 1]:
                    span = slot_centers[j + 1] - slot_centers[j]
                    t = (x - slot_centers[j]) / span
                    k_norm = (1 - t) * kernels[j] / max_k + t * kernels[j + 1] / max_k
                    break

        h = int(k_norm * bar_max_h)
        if h <= 0:
            continue
        top = base_y - h
        for y in range(top, base_y):
            v = (base_y - y) / max(1, h)
            col = (Color.Bright if v >= 0.85 else
                   Color.MediumBright if v >= 0.6 else
                   Color.Medium if v >= 0.35 else
                   Color.MediumLow if v >= 0.12 else
                   Color.Low)
            canvas.set_color(col)
            canvas.point(x, y)

    # Ticket bars on top — auto-normalized so they're tall enough to read.
    # Each bar column is FIRST cleared to black, masking the kernel behind it
    # so the bar fill renders against a clean background.
    max_t = max(tickets) if max(tickets) > 0 else 1
    denom = max(1, max_t)
    total_t = sum(tickets)
    uniform_fallback = (total_t == 0)
    for i in range(num_slots):
        x = x_offset + i * (bar_w + gap)
        weight = tickets[i]
        active = (i == active_idx)
        selected = (i == selected_slot)

        # Black mask
        canvas.set_color(Color.None_)
        canvas.fill_rect(x, base_y - bar_max_h, bar_w, bar_max_h)

        if weight == 0 and not uniform_fallback:
            canvas.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            h = bar_max_h if uniform_fallback else max(1, (weight * bar_max_h) // denom)
            if active:
                canvas.set_color(Color.Bright)
            elif selected:
                canvas.set_color(Color.MediumBright)
            else:
                canvas.set_color(Color.Low if uniform_fallback else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

        canvas.set_font(Font.Tiny)
        canvas.set_color(Color.Low)
        label = duration_label_short(i)
        lw = canvas.text_width(label)
        canvas.draw_text(x + (bar_w - lw) // 2, base_y + 8, label)

    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    info = f"DUR {note_duration}  VAR {variation}"
    canvas.draw_text(8, 18, info)

    draw_footer(canvas, ["TIX", "DUR", "VAR", None, "NEXT"])


def draw_pitch_page_bell_bg(canvas: Canvas,
                            degree_tickets,
                            marbles_bias: int,
                            marbles_spread: int,
                            marbles_steps: int,
                            selected_degree: int = -1,
                            active_degree: int = -1):
    """Alternative pitch page: full Marbles-style bell as background, ticket
    bars drawn on TOP so they 'cut' into the bell shape. Bars override the
    bell where they sit. Same bell math as the Marbles hero page."""
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, 0, PAGE_W, PAGE_H)

    draw_header(canvas, "SCALE TICKETS")

    scale_size = len(degree_tickets)
    base_y = 50
    bar_max_h = 18                         # trimmed from 28 — leaves headroom for bell
    # Trimmed bars + wider gaps so the bell silhouette breathes between bars.
    # Aim for ~50% gap / 50% bar of the available width.
    inner_w = PAGE_W - 16
    slot_w = inner_w // scale_size          # total per slot (bar + gap)
    bar_w = max(3, slot_w // 2)
    gap = max(2, slot_w - bar_w)
    if scale_size > 20:
        gap = max(1, slot_w - bar_w)
        bar_w = max(2, slot_w - gap)
    total_w = scale_size * (bar_w + gap) - gap
    x_offset = max(8, (PAGE_W - total_w) // 2)

    # Compute bell center in pixel space — same anchor positions as ticket bars.
    cx_data = (marbles_bias * (scale_size - 1) + 50) // 100
    cx_pix = x_offset + cx_data * (bar_w + gap) + bar_w // 2
    spread_pix = max(8, (marbles_spread * total_w) // 200)
    peak_top = base_y - bar_max_h - 1   # bell can rise to just below the info row

    # Quantization ticks (vertical guide lines) — Color.Low
    canvas.set_color(Color.Low)
    band_w = max(2, (spread_pix * 4) // max(2, marbles_steps))
    for k in range(-marbles_steps, marbles_steps + 1):
        x = cx_pix + k * band_w
        if x_offset <= x < x_offset + total_w:
            canvas.vline(x, base_y - 1, 3)

    # Bell curve fill — sample per-column from baseY upward, gradient brighter
    # toward the peak. Bars drawn later will overwrite the bell wherever they sit.
    bell_height = base_y - peak_top
    for x in range(x_offset, x_offset + total_w):
        d = (x - cx_pix) / float(spread_pix)
        env = math.exp(-d * d * 1.4)
        h = int(env * bell_height)
        if h <= 0:
            continue
        top = base_y - h
        for y in range(top, base_y):
            v = (base_y - y) / max(1, h)
            # 5-step brightness, peak at top of curve.
            col = (Color.Bright if v >= 0.85 else
                   Color.MediumBright if v >= 0.6 else
                   Color.Medium if v >= 0.35 else
                   Color.MediumLow if v >= 0.12 else
                   Color.Low)
            canvas.set_color(col)
            canvas.point(x, y)

    # Peak marker
    canvas.set_color(Color.Bright)
    canvas.point(cx_pix, peak_top)

    # Steps sieve — survivors are bars that pass; non-survivors get dimmed.
    survivors = compute_steps_survivors(scale_size, marbles_steps)

    # Ticket bars drawn ON TOP of the bell. Each bar column is FIRST cleared
    # to black (masking out the bell behind it) so the bar fill renders against
    # a clean black background — clearly discernible regardless of the bell
    # intensity at that x. Bell visibility lives in the inter-bar gaps.
    # Bars use auto-normalized scale so they're tall enough to read.
    max_t = max(degree_tickets) if max(degree_tickets) > 0 else 1
    total_t = sum(degree_tickets)
    denom = max(1, max_t)
    uniform_fallback = (total_t == 0)

    for i in range(scale_size):
        x = x_offset + i * (bar_w + gap)
        tickets = degree_tickets[i]
        active = (i == active_degree)
        selected = (i == selected_degree)
        sieved_out = (i not in survivors)

        # Black mask — full bar column cleared, hiding the bell behind it.
        canvas.set_color(Color.None_)
        canvas.fill_rect(x, base_y - bar_max_h, bar_w, bar_max_h)

        if tickets < 0:
            canvas.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
            canvas.line(x, base_y - 5, x + bar_w - 1, base_y)
            canvas.line(x, base_y, x + bar_w - 1, base_y - 5)
        elif tickets == 0 and not uniform_fallback:
            canvas.set_color(Color.Bright if active else (Color.Medium if selected else Color.Low))
            canvas.draw_rect(x, base_y - 2, bar_w - 1, 2)
        else:
            h = bar_max_h if uniform_fallback else max(1, (tickets * bar_max_h) // denom)
            if active:
                canvas.set_color(Color.Bright)
            elif selected:
                canvas.set_color(Color.MediumBright)
            elif sieved_out:
                canvas.set_color(Color.Low)
            else:
                canvas.set_color(Color.Low if uniform_fallback else Color.Medium)
            canvas.fill_rect(x, base_y - h, bar_w, h)

    # Info row
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    info = f"BIAS {marbles_bias}  SPRD {marbles_spread}  STEPS {marbles_steps}"
    canvas.draw_text(8, 18, info)

    draw_footer(canvas, ["TIX", "BIAS", "SPRD", "STEPS", "NEXT"])


def render_case(name: str, draw_fn, **kwargs):
    fb = FrameBuffer(PAGE_W, PAGE_H)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Small)
    draw_fn(canvas, **kwargs)
    return framebuffer_to_image(fb, scale=4), name


def main():
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ticket-overlay-previews')
    os.makedirs(out_dir, exist_ok=True)

    flat8 = [10] * 8
    twoslot = [0, 0, 0, 50, 0, 50, 0, 0]   # only slots 3 and 5
    flat7 = [10] * 7
    twoslot_pitch = [50, 0, 0, 0, 50, 0, 0]  # root + 5th-ish

    cases = [
        # Duration page sweeps
        ("dur-flat-var0",   draw_duration_page,
            dict(tickets=flat8, note_duration=5, variation=0,  selected_slot=5, active_idx=5)),
        ("dur-flat-var50",  draw_duration_page,
            dict(tickets=flat8, note_duration=5, variation=50, selected_slot=5, active_idx=5)),
        ("dur-flat-var100", draw_duration_page,
            dict(tickets=flat8, note_duration=5, variation=100,selected_slot=5, active_idx=5)),
        ("dur-mixed-var50-dur2", draw_duration_page,
            dict(tickets=twoslot, note_duration=2, variation=50, selected_slot=3, active_idx=3)),

        # Alternative — kernel silhouette as background
        ("dur-bg-flat-var0",   draw_duration_page_kernel_bg,
            dict(tickets=[10]*8, note_duration=5, variation=0,   selected_slot=5, active_idx=5)),
        ("dur-bg-flat-var50",  draw_duration_page_kernel_bg,
            dict(tickets=[10]*8, note_duration=5, variation=50,  selected_slot=5, active_idx=5)),
        ("dur-bg-flat-var100", draw_duration_page_kernel_bg,
            dict(tickets=[10]*8, note_duration=5, variation=100, selected_slot=5, active_idx=5)),
        ("dur-bg-mixed-var50-dur2", draw_duration_page_kernel_bg,
            dict(tickets=twoslot, note_duration=2, variation=50, selected_slot=3, active_idx=3)),

        # Pitch page sweeps (7-tone diatonic)
        ("pitch-flat-bias50-sprd50",  draw_pitch_page,
            dict(degree_tickets=flat7, marbles_bias=50, marbles_spread=50, marbles_steps=100,
                 selected_degree=0, active_degree=0)),
        ("pitch-flat-bias20-sprd20",  draw_pitch_page,
            dict(degree_tickets=flat7, marbles_bias=20, marbles_spread=20, marbles_steps=100,
                 selected_degree=0, active_degree=1)),
        ("pitch-flat-bias80-sprd90",  draw_pitch_page,
            dict(degree_tickets=flat7, marbles_bias=80, marbles_spread=90, marbles_steps=100,
                 selected_degree=5, active_degree=5)),
        ("pitch-flat-steps30",        draw_pitch_page,
            dict(degree_tickets=flat7, marbles_bias=50, marbles_spread=50, marbles_steps=30,
                 selected_degree=0, active_degree=0)),
        ("pitch-mixed-bias50-sprd50", draw_pitch_page,
            dict(degree_tickets=twoslot_pitch, marbles_bias=50, marbles_spread=50, marbles_steps=100,
                 selected_degree=0, active_degree=0)),

        # Alternative — Marbles bell as background, bars cut into it
        ("pitch-bellbg-flat-bias50-sprd50",  draw_pitch_page_bell_bg,
            dict(degree_tickets=flat7, marbles_bias=50, marbles_spread=50, marbles_steps=100,
                 selected_degree=0, active_degree=0)),
        ("pitch-bellbg-flat-bias20-sprd20",  draw_pitch_page_bell_bg,
            dict(degree_tickets=flat7, marbles_bias=20, marbles_spread=20, marbles_steps=100,
                 selected_degree=0, active_degree=1)),
        ("pitch-bellbg-flat-bias80-sprd90",  draw_pitch_page_bell_bg,
            dict(degree_tickets=flat7, marbles_bias=80, marbles_spread=90, marbles_steps=100,
                 selected_degree=5, active_degree=5)),
        ("pitch-bellbg-flat-steps30",        draw_pitch_page_bell_bg,
            dict(degree_tickets=flat7, marbles_bias=50, marbles_spread=50, marbles_steps=30,
                 selected_degree=0, active_degree=0)),
        ("pitch-bellbg-mixed-bias50-sprd50", draw_pitch_page_bell_bg,
            dict(degree_tickets=twoslot_pitch, marbles_bias=50, marbles_spread=50, marbles_steps=100,
                 selected_degree=0, active_degree=0)),
    ]

    for entry in cases:
        name, fn, kwargs = entry
        img, _ = render_case(name, fn, **kwargs)
        path = os.path.join(out_dir, f"{name}.png")
        img.save(path)
        print(f"  {name:38s} -> {path}")


if __name__ == "__main__":
    main()
