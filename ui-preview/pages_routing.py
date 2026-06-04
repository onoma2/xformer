"""
Proposed routing UI — Surface A (the route-list overview).

Renders all 16 route slots occupied, targets spanning every track mode present in
the project. Validates row layout / glyph widths against the real tiny5x5 font:
slot | source | target | per-track mask | depth, with a scrollbar for the 16 routes.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

# slot, source, target, track-mask (8 bits, bit0 = T1), depth-string, dangling
ROUTES = [
    (1,  "CV1",  "Transpose",  0b00000001, "+50%",  False),  # Note
    (2,  "CV2",  "Scale",      0b00000001, "+5",    False),  # Note
    (3,  "LFO",  "First Step", 0b00000001, "+4",    False),  # Note sequence
    (4,  "CV3",  "Curve Rate", 0b00000010, "+30%",  False),  # Curve
    (5,  "M.C1", "Chaos Amt",  0b00000010, "+60%",  False),  # Curve/Chaos
    (6,  "CV4",  "Algorithm",  0b00001000, "+2",    False),  # Tuesday
    (7,  "M.C2", "Glide",      0b00001000, "+40%",  False),  # Tuesday
    (8,  "CV1",  "Mask",       0b10000000, "+50%",  False),  # Stochastic
    (9,  "CV2",  "Burst",      0b10000000, "+80%",  False),  # Stochastic
    (10, "LFO",  "DM Input",   0b00010000, "+25%",  False),  # DiscreteMap
    (11, "M.C3", "Idx Range",  0b00100000, "+50%",  False),  # Indexed
    (12, "CV3",  "Transpose",  0b01000000, "+5",    False),  # PhaseFlux
    (13, "CV4",  "Scale",      0b01000000, "+3",    False),  # PhaseFlux
    (14, "M.C4", "Tempo",      0b00000000, "+10%",  False),  # Global
    (15, "CV1",  "Mute",       0b00000100, "TRIG",  False),  # Global/PlayState
    (16, "M.PB", "Swing",      0b00000000, "+20%",  False),  # Global
]

# column x anchors
X_SLOT = 3
X_SRC = 17
X_TGT = 46
X_MASK = 150
X_DEPTH_R = 246   # right edge for right-aligned depth
SCROLL_X = 252

ROW_H = 6
TOP_Y = 18
VISIBLE = 6


def _header(canvas: Canvas, count: int):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "ROUTING")
    canvas.set_color(Color.Medium)
    label = f"{count}/16"
    canvas.draw_text(PW - canvas.text_width(label) - 2, 7, label)
    canvas.hline(0, 10, PW)


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["", "ADD", "DUP", "CLR", "EDIT"]
    for i in range(1, 5):
        x = (PW * i) // 5
        canvas.vline(x, PH - 10, 10)
    canvas.set_font(Font.Tiny)
    for i, name in enumerate(names):
        if not name:
            continue
        x0 = (PW * i) // 5
        x1 = (PW * (i + 1)) // 5
        canvas.set_color(Color.Medium)
        canvas.draw_text(x0 + (x1 - x0 - canvas.text_width(name)) // 2, PH - 3, name)


def _mask(canvas: Canvas, x: int, y: int, mask: int):
    # 8 small cells, filled = track is a member; "n/a" dashes for global (mask 0)
    if mask == 0:
        canvas.set_color(Color.MediumLow)
        canvas.draw_text(x, y + 5, "global")
        return
    for t in range(8):
        cx = x + t * 5
        if mask & (1 << t):
            canvas.set_color(Color.Bright)
            canvas.fill_rect(cx, y, 4, 4)
        else:
            canvas.set_color(Color.MediumLow)
            canvas.draw_rect(cx, y, 4, 4)


def render_routing_list(canvas: Canvas, selected: int = 0, scroll: int = 0):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()

    occupied = sum(1 for r in ROUTES if r[2])
    _header(canvas, occupied)
    _footer(canvas)

    canvas.set_font(Font.Tiny)
    for i in range(VISIBLE):
        idx = scroll + i
        if idx >= len(ROUTES):
            break
        slot, src, tgt, mask, depth, dangling = ROUTES[idx]
        y = TOP_Y + i * ROW_H
        rowsel = (idx == selected)

        if rowsel:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(0, y - 1, PW - 6, ROW_H)
            canvas.set_blend_mode(BlendMode.Sub)

        # slot
        canvas.set_color(Color.Bright if not dangling else Color.Medium)
        canvas.draw_text(X_SLOT, y + 4, f"{slot}")
        # dangling marker
        if dangling:
            canvas.draw_text(X_SLOT + 11, y + 4, "!")
        # source
        canvas.set_color(Color.Medium)
        canvas.draw_text(X_SRC, y + 4, src)
        # source -> target arrow
        canvas.draw_text(X_TGT - 7, y + 4, ">")
        # target
        canvas.set_color(Color.Bright)
        canvas.draw_text(X_TGT, y + 4, tgt)
        # per-track mask
        _mask(canvas, X_MASK, y, mask)
        # depth, right-aligned
        canvas.set_color(Color.Bright)
        canvas.draw_text(X_DEPTH_R - canvas.text_width(depth), y + 4, depth)

        if rowsel:
            canvas.set_blend_mode(BlendMode.Set)

    # scrollbar
    track_top = 12
    track_h = PH - 11 - track_top
    canvas.set_color(Color.MediumLow)
    canvas.vline(SCROLL_X, track_top, track_h)
    thumb_h = max(4, track_h * VISIBLE // len(ROUTES))
    thumb_y = track_top + track_h * scroll // len(ROUTES)
    canvas.set_color(Color.Bright)
    canvas.fill_rect(SCROLL_X - 1, thumb_y, 3, thumb_h)
