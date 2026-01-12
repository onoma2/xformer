#include "CvRoutePage.h"

#include "Pages.h"

#include "core/utils/StringBuilder.h"

#include "Config.h"
#include "model/CvRoute.h"
#include "ui/painters/WindowPainter.h"

#include <cstdlib>

CvRoutePage::CvRoutePage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void CvRoutePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);

    const int startX = 8;
    const int startY = 30;
    const int rowHeight = 12;
    const int colCount = 5;
    const int colWidth = (CONFIG_LCD_WIDTH - startX * 2) / colCount;

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    {
        const int logoX = startX;
        const int logoW = CONFIG_LCD_WIDTH - startX * 2;
        const int midX = logoX + logoW / 2;
        const int logoY1 = 8;
        const int logoY2 = 12;
        canvas.setColor(Color::MediumBright);
        canvas.line(logoX, logoY2, midX, logoY1);
        canvas.line(midX, logoY1, logoX + logoW, logoY2);
        canvas.setColor(Color::Medium);
        canvas.line(logoX, logoY1, midX, logoY2);
        canvas.line(midX, logoY2, logoX + logoW, logoY1);
    }

    auto drawCell = [&canvas, colWidth, startX] (int col, int y, const char *text, Color color) {
        int colX = startX + col * colWidth;
        int textWidth = canvas.textWidth(text);
        int x = colX + (colWidth - textWidth) / 2;
        canvas.setColor(color);
        canvas.drawText(x, y, text);
    };

    auto laneColor = [] (int value, int lane) {
        const int laneValue = lane == 0 ? 0 : (lane == 1 ? 33 : (lane == 2 ? 66 : 100));
        if (value == laneValue) {
            return Color::Bright;
        }
        if (value > 0 && value < 33) {
            if (lane == 0 || lane == 1) {
                Color base = Color::Medium;
                if (lane == 0 && value <= 8) {
                    return Color::MediumBright;
                }
                if (lane == 1 && value >= 25) {
                    return Color::MediumBright;
                }
                return base;
            }
        }
        if (value > 33 && value < 66) {
            if (lane == 1 || lane == 2) {
                Color base = Color::Medium;
                if (lane == 1 && value <= 41) {
                    return Color::MediumBright;
                }
                if (lane == 2 && value >= 58) {
                    return Color::MediumBright;
                }
                return base;
            }
        }
        if (value > 66 && value < 100) {
            if (lane == 2 || lane == 3) {
                Color base = Color::Medium;
                if (lane == 2 && value <= 74) {
                    return Color::MediumBright;
                }
                if (lane == 3 && value >= 92) {
                    return Color::MediumBright;
                }
                return base;
            }
        }
        return Color::Low;
    };

    const auto &cvRoute = _project.cvRoute();

    // Row 1: Inputs + Scan
    for (int lane = 0; lane < 4; ++lane) {
        Color color = laneColor(cvRoute.scan(), lane);
        if (_editRow == EditRow::Input && _activeCol == lane) {
            color = Color::Bright;
        }
        drawCell(lane, startY, inputLabel(lane, int(cvRoute.inputSource(lane))), color);
    }
    {
        FixedStringBuilder<12> scanStr;
        scanStr("%d SCAN", cvRoute.scan());
        Color color = (_editRow == EditRow::Input && _activeCol == 4) ? Color::Bright : Color::Medium;
        drawCell(4, startY, scanStr, color);
    }

    // Row 2: Outputs + Route
    int row2Y = startY + rowHeight + 8;
    for (int lane = 0; lane < 4; ++lane) {
        Color color = laneColor(cvRoute.route(), lane);
        if (_editRow == EditRow::Output && _activeCol == lane) {
            color = Color::Bright;
        }
        drawCell(lane, row2Y, outputLabel(lane, int(cvRoute.outputDest(lane))), color);
    }
    {
        FixedStringBuilder<12> routeStr;
        routeStr("%d ROUTE", cvRoute.route());
        Color color = (_editRow == EditRow::Output && _activeCol == 4) ? Color::Bright : Color::Medium;
        drawCell(4, row2Y, routeStr, color);
    }
}

void CvRoutePage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (fn >= 0 && fn <= 4) {
            _activeCol = fn;
            _editRow = (_editRow == EditRow::Input) ? EditRow::Output : EditRow::Input;
        }
        event.consume();
    }
}

void CvRoutePage::encoder(EncoderEvent &event) {
    auto &cvRoute = _project.cvRoute();
    int delta = event.value();
    bool shift = event.pressed() || globalKeyState()[Key::Shift];
    int step = shift ? 10 : 1;

    if (_activeCol == 4) {
        if (_editRow == EditRow::Input) {
            cvRoute.setScan(cvRoute.scan() + delta * step);
        } else {
            cvRoute.setRoute(cvRoute.route() + delta * step);
        }
    } else {
        if (_editRow == EditRow::Input) {
            cycleInput(_activeCol, delta);
        } else {
            cycleOutput(_activeCol, delta);
        }
    }
    event.consume();
}

void CvRoutePage::cycleInput(int lane, int delta) {
    if (delta == 0) {
        return;
    }
    auto &cvRoute = _project.cvRoute();
    int current = int(cvRoute.inputSource(lane));
    int dir = delta > 0 ? 1 : -1;
    int steps = std::abs(delta);

    for (int step = 0; step < steps; ++step) {
        for (int i = 0; i < int(CvRoute::InputSource::Last); ++i) {
            int next = (current + dir + int(CvRoute::InputSource::Last)) % int(CvRoute::InputSource::Last);
            auto candidate = static_cast<CvRoute::InputSource>(next);
            current = next;
            if (candidate == CvRoute::InputSource::Bus &&
                cvRoute.outputDest(lane) == CvRoute::OutputDest::Bus) {
                continue;
            }
            cvRoute.setInputSource(lane, candidate);
            break;
        }
    }
}

void CvRoutePage::cycleOutput(int lane, int delta) {
    if (delta == 0) {
        return;
    }
    auto &cvRoute = _project.cvRoute();
    int current = int(cvRoute.outputDest(lane));
    int dir = delta > 0 ? 1 : -1;
    int steps = std::abs(delta);

    for (int step = 0; step < steps; ++step) {
        for (int i = 0; i < int(CvRoute::OutputDest::Last); ++i) {
            int next = (current + dir + int(CvRoute::OutputDest::Last)) % int(CvRoute::OutputDest::Last);
            auto candidate = static_cast<CvRoute::OutputDest>(next);
            current = next;
            if (candidate == CvRoute::OutputDest::Bus &&
                cvRoute.inputSource(lane) == CvRoute::InputSource::Bus) {
                continue;
            }
            cvRoute.setOutputDest(lane, candidate);
            break;
        }
    }
}

const char *CvRoutePage::inputLabel(int lane, int value) {
    switch (static_cast<CvRoute::InputSource>(value)) {
    case CvRoute::InputSource::CvIn:
        switch (lane) {
        case 0: return "CV in 1";
        case 1: return "CV in 2";
        case 2: return "CV in 3";
        case 3: return "CV in 4";
        default: return "CV";
        }
    case CvRoute::InputSource::Bus:
        switch (lane) {
        case 0: return "BUS 1";
        case 1: return "BUS 2";
        case 2: return "BUS 3";
        case 3: return "BUS 4";
        default: return "BUS";
        }
    case CvRoute::InputSource::Off:
        return "0V";
    case CvRoute::InputSource::Last:
        break;
    }
    return "-";
}

const char *CvRoutePage::outputLabel(int lane, int value) {
    switch (static_cast<CvRoute::OutputDest>(value)) {
    case CvRoute::OutputDest::CvOut:
        switch (lane) {
        case 0: return "CV R 1";
        case 1: return "CV R 2";
        case 2: return "CV R 3";
        case 3: return "CV R 4";
        default: return "CVR";
        }
    case CvRoute::OutputDest::Bus:
        switch (lane) {
        case 0: return "BUS 1";
        case 1: return "BUS 2";
        case 2: return "BUS 3";
        case 3: return "BUS 4";
        default: return "BUS";
        }
    case CvRoute::OutputDest::None:
        return "NONE";
    case CvRoute::OutputDest::Last:
        break;
    }
    return "-";
}
