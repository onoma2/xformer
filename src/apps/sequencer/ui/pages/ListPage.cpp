#include "ListPage.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"

#include "model/Track.h"
#include "model/RouteFork.h"
#include "model/RouteDraft.h"

namespace {

// Short source label for the inline modulated-row badge: CvIn1->"CV1",
// CvOut1->"O1", BusCv1->"B1", GateOut1->"G1", Mod1->"M1".
void printSourceAbbrev(Routing::Source source, StringBuilder &str) {
    if (Routing::isCvSource(source)) {
        str("CV%d", int(source) - int(Routing::Source::CvIn1) + 1);
    } else if (source >= Routing::Source::CvOut1 && source <= Routing::Source::CvOut8) {
        str("O%d", int(source) - int(Routing::Source::CvOut1) + 1);
    } else if (Routing::isBusSource(source)) {
        str("B%d", int(source) - int(Routing::Source::BusCv1) + 1);
    } else if (source >= Routing::Source::GateOut1 && source <= Routing::Source::GateOut8) {
        str("G%d", int(source) - int(Routing::Source::GateOut1) + 1);
    } else if (Routing::isModulatorSource(source)) {
        str("M%d", int(source) - int(Routing::Source::Mod1) + 1);
    } else {
        Routing::printSource(source, str);
    }
}

// Horizontal bipolar depth bar: dim baseline x..x+w, center tick, and a fill
// from center outward — rightward for +depth, leftward for -depth.
void drawDepthBar(Canvas &canvas, int x, int y, int w, int depthPct) {
    int cx = x + w / 2;
    canvas.setColor(Color::Low);
    canvas.hline(x, y + 1, w);
    canvas.vline(cx, y - 1, 5);
    int span = std::abs(depthPct) * (w / 2) / 100;
    if (span > 0) {
        canvas.setColor(Color::Medium);
        canvas.fillRect(depthPct >= 0 ? cx : cx - span, y, span, 3);
    }
}

} // namespace

ListPage::ListPage(PageManager &manager, PageContext &context, ListModel &listModel) :
    BasePage(manager, context)
{
    setListModel(listModel);
}

void ListPage::setListModel(ListModel &listModel) {
    _listModel = &listModel;
    _selectedRow = 0;
    _displayRow = 0;
    _edit = false;
}

void ListPage::show() {
    BasePage::show();
}

void ListPage::enter() {
    scrollTo(_selectedRow);
}

void ListPage::exit() {
}

void ListPage::draw(Canvas &canvas) {
    int displayRow = _displayRow;
    if (displayRow + LineCount > _listModel->rows()) {
        displayRow = std::max(0, _listModel->rows() - LineCount);
    }

    for (int i = 0; i < LineCount; ++i) {
        int row = displayRow + i;
        if (row < _listModel->rows()) {
            drawCell(canvas, row, 0, 8, 12 + i * LineHeight, 128 - 16, LineHeight);
            drawCell(canvas, row, 1, 128, 12 + i * LineHeight, 128 - 16, LineHeight);
            if (row == _selectedRow) {
                drawModulatedRow(canvas, row, 12 + i * LineHeight);
            }
        }
    }

    WindowPainter::drawScrollbar(canvas, Width - 8, 12, 4, LineCount * LineHeight, _listModel->rows(), LineCount, displayRow);
}

void ListPage::updateLeds(Leds &leds) {
}

void ListPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isLeft()) {
        if (_edit) {
            editSelectedRow(-1, globalKeyState()[Key::Shift]);
        } else {
            setSelectedRow(selectedRow() - 1);
        }
        event.consume();
    }
    else if (key.isRight()) {
        if (_edit) {
            editSelectedRow(1, globalKeyState()[Key::Shift]);
        } else {
            setSelectedRow(selectedRow() + 1);
        }
        event.consume();
    } else if (key.isEncoder()) {
        _edit = !_edit;
        event.consume();
    }
}

void ListPage::encoder(EncoderEvent &event) {
    if (_edit) {
        editSelectedRow(event.value(), event.pressed() || globalKeyState()[Key::Shift]);
    } else {
        setSelectedRow(selectedRow() + event.value());
    }
    event.consume();
}

void ListPage::keyboard(KeyboardEvent &event) {
    switch (event.keycode()) {
    case KeyboardEvent::KeyUp:
        setSelectedRow(selectedRow() - 1);
        event.consume();
        break;
    case KeyboardEvent::KeyDown:
        setSelectedRow(selectedRow() + 1);
        event.consume();
        break;
    case KeyboardEvent::KeyLeft:
        editSelectedRow(-1, event.shift());
        event.consume();
        break;
    case KeyboardEvent::KeyRight:
        editSelectedRow(1, event.shift());
        event.consume();
        break;
    case KeyboardEvent::KeyEnter:
        _edit = !_edit;
        event.consume();
        break;
    default:
        BasePage::keyboard(event);
        break;
    }
}

void ListPage::editSelectedRow(int value, bool shift) {
    const bool isTeletype = _project.selectedTrack().trackMode() == Track::TrackMode::Teletype;
    if (isTeletype) {
        _engine.lock();
    }
    _listModel->edit(_selectedRow, 1, value, shift);
    if (isTeletype) {
        _engine.unlock();
    }
}

void ListPage::setSelectedRow(int selectedRow) {
    int rows = _listModel->rows();
    if (rows <= 0) {
        _selectedRow = 0;
        _displayRow = 0;
        return;
    }

    int nextRow = selectedRow % rows;
    if (nextRow < 0) {
        nextRow += rows;
    }

    if (nextRow != _selectedRow) {
        _selectedRow = nextRow;
        scrollTo(_selectedRow);
    }
}

void ListPage::setTopRow(int row) {
    _displayRow = std::min(row, std::max(0, _listModel->rows() - LineCount));
}

void ListPage::drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h) {
    FixedStringBuilder<32> str;
    _listModel->cell(row, column, str);
    canvas.setFont(Font::Small);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(column == int(_edit) && row == _selectedRow ? Color::Bright : Color::Medium);
    canvas.drawText(x, y + 7, str);
}

void ListPage::drawModulatedRow(Canvas &canvas, int row, int y) {
    Routing::Target target = _listModel->routingTarget(row);
    if (target == Routing::Target::None) {
        return;
    }

    const auto &track = _project.selectedTrack();
    uint8_t key = 0;
    RouteParam::Range range;
    bool isMigrated = RouteFork::migrated(track.trackMode(), target, key, range) ||
                      RouteFork::migratedGlobal(target, key, range);
    if (!isMigrated) {
        return;
    }

    int trackIndex = _project.selectedTrackIndex();
    const auto &routing = _project.routing();
    if (!RouteDraft::isTrackModulated(routing, target, trackIndex)) {
        return;
    }

    int routeIndex = routing.findRoute(target, trackIndex);
    if (routeIndex < 0) {
        return;
    }
    const auto &route = routing.route(routeIndex);

    FixedStringBuilder<8> srcStr;
    printSourceAbbrev(route.source(), srcStr);
    canvas.setFont(Font::Small);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Medium);
    canvas.drawText(78, y + 7, srcStr);
    canvas.drawText(96, y + 7, ">");

    drawDepthBar(canvas, 104, y + 4, 92, route.depthPct(trackIndex));
}

void ListPage::scrollTo(int row) {
    if (row < _displayRow) {
        _displayRow = row;
    } else if (row >= _displayRow + LineCount) {
        _displayRow = row - (LineCount - 1);
    }
}
