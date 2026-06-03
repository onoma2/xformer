"""Mirror of RoutingPage::drawTabEditor (read-only display shell) for glyph-fit check."""
from canvas import BlendMode, Canvas, Color, Font

PW, PH = 256, 64
TABS = ["PITCH", "CLOCK", "PROB", "GLOB"]


def render_routing_tabeditor(canvas: Canvas, active_tab=0, target="Transpose",
                             source="CV1", depth=50, mask=0b00010101,
                             layout="NCNANDPS", combine="Modulate"):
    canvas.set_blend_mode(BlendMode.Set); canvas.set_color(Color.None_); canvas.fill()
    # header
    canvas.set_font(Font.Tiny); canvas.set_color(Color.Bright)
    canvas.draw_text(2, 7, "A 120.0"); canvas.draw_text(54, 7, "ROUTE")
    canvas.draw_text(100, 7, target)
    canvas.set_color(Color.Medium); canvas.hline(0, 10, PW)
    # footer
    canvas.set_color(Color.Medium); canvas.hline(0, PH - 11, PW)
    canvas.draw_text(2 + (PW // 5 - canvas.text_width("BACK")) // 2, PH - 3, "BACK")
    for i in range(1, 5):
        canvas.vline((PW * i) // 5, PH - 10, 10)
    # vertical tabs
    tabX, tabW, tabTop, tabH = 2, 40, 14, 9
    for i, t in enumerate(TABS):
        y = tabTop + i * tabH
        if i == active_tab:
            canvas.set_color(Color.MediumBright); canvas.fill_rect(tabX, y, tabW, tabH - 1)
            canvas.set_blend_mode(BlendMode.Sub); canvas.draw_text(tabX + 3, y + 6, t)
            canvas.set_blend_mode(BlendMode.Set)
        else:
            canvas.set_color(Color.MediumLow); canvas.draw_text(tabX + 3, y + 6, t)
    canvas.set_color(Color.MediumLow); canvas.vline(tabW + 2, 12, PH - 23)
    # param row
    cx, rowY = tabW + 6, 16
    mapX = PW - 8*11 - 1
    canvas.set_color(Color.Bright); canvas.draw_text(cx, rowY + 4, target.upper())
    canvas.set_color(Color.Medium); canvas.draw_text(cx, rowY + 12, source)
    d=f"{depth:+d}%"; canvas.set_color(Color.Bright); canvas.draw_text(mapX - 6 - canvas.text_width(d), rowY + 12, d)
    # mini-map
    mapX, mapY = PW - 8 * 11 - 1, rowY + 8
    for t in range(8):
        px = mapX + t * 11
        ch = layout[t]
        if mask & (1 << t):
            canvas.set_color(Color.Bright); canvas.fill_rect(px, mapY, 10, 7)
            canvas.set_blend_mode(BlendMode.Sub); canvas.draw_text(px + 3, mapY + 5, ch)
            canvas.set_blend_mode(BlendMode.Set)
        else:
            canvas.set_color(Color.MediumLow); canvas.draw_text(px + 3, mapY + 5, ch)
    # combine
    canvas.set_color(Color.Medium); canvas.draw_text(cx, rowY + 28, f"Combine: {combine}")
