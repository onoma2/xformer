"""
Proposed: Hermod+-style depth view for the "modulate this" front door.

Replaces the bare QuickEdit number+LED-ring depth modal with a bipolar travel bar:
- center tick = the base (the value you set on the param) = Hermod's "offset"
- bright segment = the modulation travel (base ± depth for MOD / base→base+depth for ABS)
- moving dot = the source's live position within that travel (real-time feedback)
- big signed depth readout + combine label (MOD/ABS = Hermod's polarity)

Our model maps 1:1 to Hermod's attenuverter+polarity+offset:
  depth (d)  = attenuverter amount
  combine    = polarity (Modulate centred / Absolute sweep-from-base)
  base       = offset (the value the user set)
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

BAR_Y = 47
BAR_L = 20
BAR_R = 236
CENTER = (BAR_L + BAR_R) // 2
HALF = CENTER - BAR_L


def _header(canvas: Canvas, target: str, source: str):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, target)
    canvas.set_color(Color.Medium)
    canvas.draw_text(2 + canvas.text_width(target) + 4, 7, f"< {source}")
    canvas.set_color(Color.Medium)
    canvas.hline(0, 10, PW)


def render_routing_depth(canvas: Canvas, depth: int = 45, absolute: bool = False,
                         source_pos: float = 0.6):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()

    _header(canvas, "TRANSPOSE", "CV In 1")

    # big signed depth readout (Small font)
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    val = f"{depth:+d}%"
    canvas.draw_text(BAR_L, 30, val)

    # combine label (polarity)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    label = "ABS" if absolute else "MOD"
    canvas.draw_text(BAR_R - canvas.text_width(label), 26, label)

    # faint full range line
    canvas.set_color(Color.Low)
    canvas.hline(BAR_L, BAR_Y, BAR_R - BAR_L)
    # end ticks
    canvas.vline(BAR_L, BAR_Y - 2, 5)
    canvas.vline(BAR_R, BAR_Y - 2, 5)

    # base tick (the offset / set value)
    canvas.set_color(Color.Bright)
    canvas.vline(CENTER, BAR_Y - 4, 9)

    # modulation travel (bright segment)
    span = int(abs(depth) / 100 * HALF)
    canvas.set_color(Color.MediumBright)
    if absolute:
        x0 = CENTER if depth >= 0 else CENTER - span
        canvas.fill_rect(x0, BAR_Y - 1, span if span > 0 else 1, 3)
    else:
        canvas.fill_rect(CENTER - span, BAR_Y - 1, 2 * span if span > 0 else 1, 3)

    # live source dot within the travel
    if absolute:
        pos = CENTER + int(source_pos * span * (1 if depth >= 0 else -1))
    else:
        pos = CENTER + int((source_pos * 2 - 1) * span)
    canvas.set_color(Color.Bright)
    canvas.fill_rect(pos - 2, BAR_Y - 3, 4, 7)

    # footer
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["", "MOD/ABS", "SRC", "", "DONE"]
    for i in range(1, 5):
        canvas.vline((PW * i) // 5, PH - 10, 10)
    for i, name in enumerate(names):
        if not name:
            continue
        x0 = (PW * i) // 5
        x1 = (PW * (i + 1)) // 5
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)
