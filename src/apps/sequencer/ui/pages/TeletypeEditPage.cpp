#include "TeletypeEditPage.h"

#include "Pages.h"

#include "engine/TeletypeTrackEngine.h"
#include "model/TeletypeTrack.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"
#include "core/utils/StringBuilder.h"

namespace {
constexpr int kRowStartY = 16;
constexpr int kRowStepY = 12;
constexpr int kValueX = 28;
} // namespace

TeletypeEditPage::TeletypeEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void TeletypeEditPage::enter() {
    _selectedSlot = 0;
}

void TeletypeEditPage::exit() {
}

void TeletypeEditPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Teletype) {
        close();
        return;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "TELETYPE");

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    auto &track = _project.selectedTrack().teletypeTrack();

    for (int i = 0; i < TeletypeTrack::ScriptSlotCount; ++i) {
        int y = kRowStartY + i * kRowStepY;
        bool active = i == _selectedSlot;
        canvas.setColor(active ? Color::Bright : Color::Medium);
        FixedStringBuilder<8> slotLabel("S%d", i);
        canvas.drawText(0, y, slotLabel);

        int presetIndex = track.scriptPresetIndex(i);
        const char *presetName = TeletypeTrackEngine::presetName(presetIndex);
        canvas.drawText(kValueX, y, presetName ? presetName : "?");
    }

    const char *footerLabels[] = { "S0", "S1", "S2", "S3", nullptr };
    WindowPainter::drawFooter(canvas, footerLabels, pageKeyState(), _selectedSlot);
}

void TeletypeEditPage::updateLeds(Leds &leds) {
    LedPainter::drawSelectedSequenceSection(leds, 0);
}

void TeletypeEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (fn == 4) {
            _manager.push(&_manager.pages().teletypeScriptView);
            event.consume();
            return;
        }
        if (fn >= 0 && fn < TeletypeTrack::ScriptSlotCount) {
            setSelectedSlot(fn);
            event.consume();
            return;
        }
    }
}

void TeletypeEditPage::encoder(EncoderEvent &event) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Teletype) {
        event.consume();
        return;
    }

    auto &track = _project.selectedTrack().teletypeTrack();
    int current = track.scriptPresetIndex(_selectedSlot);
    int next = current + event.value();
    int count = TeletypeTrackEngine::PresetScriptCount;

    if (next < 0) {
        next = count - 1;
    } else if (next >= count) {
        next = 0;
    }

    track.setScriptPresetIndex(_selectedSlot, next);

    auto &trackEngine = _engine.selectedTrackEngine().as<TeletypeTrackEngine>();
    trackEngine.applyPresetToScript(_selectedSlot, next);

    event.consume();
}

void TeletypeEditPage::setSelectedSlot(int slot) {
    if (slot < 0) {
        slot = 0;
    }
    if (slot >= TeletypeTrack::ScriptSlotCount) {
        slot = TeletypeTrack::ScriptSlotCount - 1;
    }
    _selectedSlot = slot;
}
