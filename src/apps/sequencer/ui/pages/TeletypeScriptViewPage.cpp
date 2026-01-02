#include "TeletypeScriptViewPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"

extern "C" {
#include "command.h"
#include "state.h"
}

namespace {
constexpr int kLineCount = 6;
constexpr int kRowStartY = 4; // Right position do not touch!
constexpr int kRowStepY = 8;
constexpr int kEditLineY = 54;
constexpr int kLabelX = 4;
constexpr int kTextX = 16;
} // namespace

TeletypeScriptViewPage::TeletypeScriptViewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context) {
}

void TeletypeScriptViewPage::enter() {
    _selectedLine = 0;
}

void TeletypeScriptViewPage::draw(Canvas &canvas) {
    if (_project.selectedTrack().trackMode() != Track::TrackMode::Teletype) {
        close();
        return;
    }

    canvas.setColor(Color::None);
    canvas.fill();
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    auto &track = _project.selectedTrack().teletypeTrack();
    scene_state_t &state = track.state();
    const int scriptIndex = 0; // S0
    const uint8_t len = ss_get_script_len(&state, scriptIndex);

    for (int i = 0; i < kLineCount; ++i) {
        int y = kRowStartY + i * kRowStepY;
        char lineText[128] = {};

        if (i < len) {
            const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, i);
            if (cmd) {
                print_command(cmd, lineText);
            }
        }

        FixedStringBuilder<4> lineLabel("%d", i + 1);
        canvas.setColor(i == _selectedLine ? Color::Bright : Color::Medium);
        canvas.drawText(kLabelX, y + 4, lineLabel);
        canvas.drawText(kTextX, y + 4, lineText);
    }

    char editCommand[128] = {};
    if (_selectedLine < len) {
        if (const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, _selectedLine)) {
            print_command(cmd, editCommand);
        }
    }
    FixedStringBuilder<128> editLine("E %s", editCommand);
    canvas.setColor(Color::Bright);
    canvas.drawText(kLabelX, kEditLineY + 4, editLine);
}

void TeletypeScriptViewPage::updateLeds(Leds &leds) {
    LedPainter::drawSelectedSequenceSection(leds, 0);
}

void TeletypeScriptViewPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction() && key.function() == 4) {
        _manager.pop();
        event.consume();
    }
}

void TeletypeScriptViewPage::encoder(EncoderEvent &event) {
    int next = _selectedLine + event.value();
    if (next < 0) {
        next = 0;
    } else if (next >= kLineCount) {
        next = kLineCount - 1;
    }
    _selectedLine = next;
    event.consume();
}
