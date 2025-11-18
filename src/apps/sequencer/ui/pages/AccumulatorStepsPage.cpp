#include "AccumulatorStepsPage.h"

#include "Pages.h"
#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"
#include "engine/NoteTrackEngine.h"

#include "core/utils/StringBuilder.h"

AccumulatorStepsPage::AccumulatorStepsPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{
}

void AccumulatorStepsPage::enter() {
    updateListModel();
}

void AccumulatorStepsPage::exit() {
    _listModel.setSequence(nullptr);
}

void AccumulatorStepsPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ACCST");
    WindowPainter::drawActiveFunction(canvas, "ACCU STEPS");
    WindowPainter::drawFooter(canvas);

    // Get current step for playhead rendering
    const auto &trackEngine = _engine.selectedTrackEngine().as<NoteTrackEngine>();
    const auto &sequence = _project.selectedNoteSequence();
    _currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;

    ListPage::draw(canvas);
}

void AccumulatorStepsPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void AccumulatorStepsPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void AccumulatorStepsPage::encoder(EncoderEvent &event) {
    if (!event.consumed()) {
        ListPage::encoder(event);
    }
}

void AccumulatorStepsPage::updateListModel() {
    auto &sequence = _project.selectedNoteSequence();
    _listModel.setSequence(&sequence);
}

void AccumulatorStepsPage::drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h) {
    FixedStringBuilder<32> str;
    _listModel.cell(row, column, str);
    canvas.setFont(Font::Small);
    canvas.setBlendMode(BlendMode::Set);

    // Highlight current playing step (playhead) with bright color
    // Also highlight selected row for editing
    bool isCurrentStep = (row == _currentStep);
    bool isSelected = (column == int(edit()) && row == selectedRow());

    canvas.setColor((isCurrentStep || isSelected) ? Color::Bright : Color::Medium);
    canvas.drawText(x, y + 7, str);
}