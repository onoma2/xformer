"""
Routing tab-editor SOURCE picker overlay (slice 6).

Modal list opened by F3 SRC on a routed row. Mirrors the firmware ListPage +
WindowPainter header/footer: header "SOURCE", scrollable source list (the CV-domain
sources RouteBrowse::sourceList offers — MIDI and the self-route bus excluded),
selected row highlighted, scrollbar, footer CANCEL/OK. Validates that source-name
glyph widths (worst case "Gate Out 8") sit clear inside the safe area.
"""

from canvas import BlendMode, Canvas, Color, Font

PW = 256
PH = 64

# RouteBrowse::sourceList order for a non-bus target: None + CvIn1-4 + CvOut1-8
# + BusCv1-4 + GateOut1-8 + Mod1-8 (MIDI excluded), rendered via Routing::printSource.
SOURCES = (
    ["None"]
    + [f"CV In {i}" for i in range(1, 5)]
    + [f"CV Out {i}" for i in range(1, 9)]
    + [f"BUS {i}" for i in range(1, 5)]
    + [f"Gate Out {i}" for i in range(1, 9)]
    + [f"Mod {i}" for i in range(1, 9)]
)

ROW_H = 7
TOP_Y = 14
VISIBLE = 5
X_NAME = 4
SCROLL_X = 252


def _header(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "SOURCE")
    canvas.hline(0, 10, PW)


def _footer(canvas: Canvas):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.Medium)
    canvas.hline(0, PH - 11, PW)
    names = ["", "", "", "CANCEL", "OK"]
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


def render_routing_source(canvas: Canvas, selected: int = 1, scroll: int = 0):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill()

    _header(canvas)
    _footer(canvas)

    canvas.set_font(Font.Tiny)
    for i in range(VISIBLE):
        idx = scroll + i
        if idx >= len(SOURCES):
            break
        y = TOP_Y + i * ROW_H
        rowsel = (idx == selected)

        if rowsel:
            canvas.set_color(Color.MediumBright)
            canvas.fill_rect(0, y, PW - 6, ROW_H)
            canvas.set_blend_mode(BlendMode.Sub)

        canvas.set_color(Color.Bright)
        canvas.draw_text(X_NAME, y + 5, SOURCES[idx])

        if rowsel:
            canvas.set_blend_mode(BlendMode.Set)

    # scrollbar
    track_top = 12
    track_h = PH - 11 - track_top
    canvas.set_color(Color.MediumLow)
    canvas.vline(SCROLL_X, track_top, track_h)
    thumb_h = max(4, track_h * VISIBLE // len(SOURCES))
    thumb_y = track_top + track_h * scroll // len(SOURCES)
    canvas.set_color(Color.Bright)
    canvas.fill_rect(SCROLL_X - 1, thumb_y, 3, thumb_h)
