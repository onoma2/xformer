#include "ListPage.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"

#include "model/Track.h"

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

void ListPage::scrollTo(int row) {
    if (row < _displayRow) {
        _displayRow = row;
    } else if (row >= _displayRow + LineCount) {
        _displayRow = row - (LineCount - 1);
    }
}
