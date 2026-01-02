#include "TeletypeScriptViewPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"

extern "C" {
#include "command.h"
#include "state.h"
}

namespace {
constexpr int kLineCount = 6;
constexpr int kRowStartY = 6;
constexpr int kRowStepY = 10;
constexpr int kLabelX = 2;
constexpr int kTextX = 14;
} // namespace

TeletypeScriptViewPage::TeletypeScriptViewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context) {
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
        canvas.setColor(Color::Medium);
        canvas.drawText(kLabelX, y, lineLabel);
        canvas.setColor(Color::Bright);
        canvas.drawText(kTextX, y, lineText);
    }
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
