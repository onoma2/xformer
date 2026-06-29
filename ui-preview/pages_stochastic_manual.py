"""
Stochastic manual renders — firmware-accurate page set for docs/stochastic-guide.html.

Four edit pages (F5 NEXT cycles), mirroring StochasticSequenceEditPage.cpp:
  LIVE             footer LiveR / LiveM / NewR / NewM / NEXT
  LOOP             footer LoopR / LoopM / NewR / NewM / NEXT
  SCALE TICKETS    footer LiveM / NewM / DROT / MROT / NEXT
  DURATION TICKETS footer LiveR / NewR / RST / - / NEXT

LIVE / LOOP reuse the existing constellation + loop-tape art; their footers are
overstamped to match the page's domain mode (a fresh track is Live/Live, so the
LIVE page reads LiveR/LiveM, not the old LoopR/LoopM the art hardcoded).
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font, FrameBuffer, framebuffer_to_image
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter
from pages_stochastic_live import render_stochastic_live_constellation
from pages_stochastic_loop import render_stochastic_loop_proposed

# Duration LUT labels at Divisor = 1/16 (StochasticTypes.h kStochasticDurationLut)
DUR_LABELS = ["1/2", "1/4", "3/16", "1/8", "1/8T", "1/16", "1/16T", "1/32"]


def _restamp_footer(canvas, labels, highlight=-1):
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_color(Color.None_)
    canvas.fill_rect(0, PageHeight - FooterHeight - 1, PageWidth, FooterHeight + 1)
    WindowPainter.draw_footer(canvas, names=labels, highlight=highlight)


def render_live(canvas):
    render_stochastic_live_constellation(canvas)
    _restamp_footer(canvas, ["LiveR", "LiveM", "NewR", "NewM", "NEXT"])


def render_loop(canvas):
    render_stochastic_loop_proposed(canvas)
    _restamp_footer(canvas, ["LoopR", "LoopM", "NewR", "NewM", "NEXT"])


def _ticket_bars(canvas, items, *, selected, active, x0, y_base, col_w, max_h):
    """Generic ticket bar chart. items: list of (label, ticket). ticket -1 =
    excluded (dash), 0 = flat baseline, 1..100 = weighted height."""
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Tiny)
    for i, (label, ticket) in enumerate(items):
        cx = x0 + i * col_w
        bw = col_w - 3
        if ticket < 0:
            canvas.set_color(Color.Low)
            canvas.line(cx, y_base - 5, cx + bw - 1, y_base)
            canvas.line(cx, y_base, cx + bw - 1, y_base - 5)
        else:
            h = 2 + (ticket * (max_h - 2)) // 100 if ticket > 0 else 2
            is_act = (i == active)
            is_sel = (i == selected)
            canvas.set_color(Color.Bright if (is_act or is_sel) else Color.MediumBright)
            canvas.fill_rect(cx, y_base - h, bw, h)
        if is_sel := (i == selected):
            canvas.set_color(Color.Bright)
            canvas.draw_rect(cx - 1, y_base - max_h - 1, bw + 2, max_h + 2)
        canvas.set_color(Color.Bright if i == active else Color.Medium)
        lw = canvas.text_width(label)
        canvas.draw_text(cx + (bw - lw) // 2, y_base + 8, label)


def render_pitch(canvas):
    # Mirrors drawPitchPage: header mode "SCALE TICKETS", per-degree bars with
    # baseline, selected/active cursor underline, "DEG n:" readout (left) and
    # DROT/MROT (right). 7-degree major scale, 4th excluded, degree 5 active.
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="SCALE TICKETS")
    tickets = [80, 20, 50, -1, 70, 15, 30]
    items = [(str(i + 1), t) for i, t in enumerate(tickets)]
    _ticket_bars(canvas, items, selected=2, active=4,
                 x0=20, y_base=46, col_w=30, max_h=22)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(8, 18, "DEG 3: 50")
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Medium)
    rot = "DROT:+0 MROT:+0"
    canvas.draw_text(PageWidth - canvas.text_width(rot) - 8, 18, rot)
    _restamp_footer(canvas, ["LiveM", "NewM", "DROT", "MROT", "NEXT"])


def render_duration(canvas):
    # Mirrors drawDurationPage: header mode "DURATION TICKETS", 8 LUT slots with
    # baseline, active slot 1/16 (idx 5), selected slot 1/8 (idx 3), "1/8: n"
    # readout (left) and "REST n%" tiny (right).
    WindowPainter.clear(canvas)
    WindowPainter.draw_header(canvas, mode="DURATION TICKETS")
    items = [(DUR_LABELS[i], t) for i, t in
             enumerate([0, 0, 0, 40, 0, 70, 0, 0])]
    _ticket_bars(canvas, items, selected=3, active=5,
                 x0=14, y_base=46, col_w=29, max_h=22)
    canvas.set_blend_mode(BlendMode.Set)
    canvas.set_font(Font.Small)
    canvas.set_color(Color.Bright)
    canvas.draw_text(8, 18, "1/8: 40")
    rest = "REST 15%"
    canvas.set_font(Font.Tiny)
    canvas.set_color(Color.Bright)
    canvas.draw_text(PageWidth - canvas.text_width(rest) - 8, 18, rest)
    _restamp_footer(canvas, ["LiveR", "NewR", "RST", None, "NEXT"])


def _emit(out_dir, name, render):
    fb = FrameBuffer(PageWidth, PageHeight)
    c = Canvas(fb)
    render(c)
    path = os.path.join(out_dir, name)
    framebuffer_to_image(fb, scale=4).save(path)
    print(f"wrote {path}")


def main():
    out = os.path.join(os.path.dirname(__file__), "stochastic-manual")
    os.makedirs(out, exist_ok=True)
    _emit(out, "stoch-live.png", render_live)
    _emit(out, "stoch-loop.png", render_loop)
    _emit(out, "stoch-pitch.png", render_pitch)
    _emit(out, "stoch-duration.png", render_duration)


if __name__ == "__main__":
    main()
