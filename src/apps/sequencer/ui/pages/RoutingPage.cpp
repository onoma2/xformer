#include "RoutingPage.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"
#include "core/math/Math.h"

enum class Function {
    Prev    = 0,
    Next    = 1,
    Init    = 2,
    Learn   = 3,
    Commit  = 4,
};

RoutingPage::RoutingPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _routeListModel),
    _routeListModel(_editRoute)
{
    showRoute(0);
}

void RoutingPage::reset() {
    _engine.midiLearn().stop();
    showRoute(0);
}

void RoutingPage::enter() {
    ListPage::enter();
}

void RoutingPage::exit() {
    _engine.midiLearn().stop();

    ListPage::exit();
}

void RoutingPage::draw(Canvas &canvas) {
    if (overlayActive()) {
        drawBiasOverlay(canvas);
        return;
    }

    bool showCommit = *_route != _editRoute;
    bool showLearn = _editRoute.target() != Routing::Target::None;
    bool highlightLearn = showLearn && _engine.midiLearn().isActive();
    const char *functionNames[] = { "PREV", "NEXT", "INIT", showLearn ? "LEARN" : nullptr, showCommit ? "COMMIT" : nullptr };

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ROUTING");
    WindowPainter::drawActiveFunction(canvas, FixedStringBuilder<16>("ROUTE %d", _routeIndex + 1));
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), highlightLearn ? int(Function::Learn) : -1);

    ListPage::draw(canvas);
}

void RoutingPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (overlayActive()) {
        handleBiasOverlayKey(event);
        return;
    }

    if (key.pageModifier() && key.isStep() && key.step() == 4) { // Page + S5
        enterBiasOverlay();
        event.consume();
        return;
    }

    if (edit() && selectedRow() == int(RouteListModel::Item::Tracks) && key.isTrack()) {
        _editRoute.toggleTrack(key.track());
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Prev:
            selectRoute(_routeIndex - 1);
            break;
        case Function::Next:
            selectRoute(_routeIndex + 1);
            break;
        case Function::Init:
            _engine.midiLearn().stop();
            _editRoute.clear();
            setSelectedRow(0);
            setEdit(false);
            break;
        case Function::Learn:
            if (_engine.midiLearn().isActive()) {
                _engine.midiLearn().stop();
            } else if (_editRoute.target() != Routing::Target::None) {
                _engine.midiLearn().start([this] (const MidiLearn::Result &result) {
                    // TODO this might be unsafe as callback is called from engine thread
                    assignMidiLearn(result);
                    _engine.midiLearn().stop();
                });
            }
            break;
        case Function::Commit:
            _engine.midiLearn().stop();
            int conflict = _project.routing().checkRouteConflict(_editRoute, *_route);
            if (conflict >= 0) {
                showMessage(FixedStringBuilder<64>("ROUTE SETTINGS CONFLICT WITH ROUTE %d", conflict + 1));
            } else {
                *_route = _editRoute;
                setEdit(false);
                showMessage("ROUTE CHANGED");
            }
            break;
        }
        event.consume();
    }

    ListPage::keyPress(event);
}

void RoutingPage::encoder(EncoderEvent &event) {
    if (overlayActive()) {
        bool shift = pageKeyState()[Key::Shift];
        editBiasOverlay(event.value(), shift);
        event.consume();
        return;
    }

    if (!edit() && pageKeyState()[Key::Shift]) {
        selectRoute(_routeIndex + event.value());
        event.consume();
        return;
    }

    ListPage::encoder(event);
}

void RoutingPage::showRoute(int routeIndex, const Routing::Route *initialValue) {
    _route = &_project.routing().route(routeIndex);
    _routeIndex = routeIndex;
    _editRoute = *(initialValue ? initialValue : _route);
    _biasOverlayActive = false;

    setSelectedRow(0);
    setEdit(false);
}

void RoutingPage::drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h) {
    if (row == int(RouteListModel::Item::Tracks) &&
        column == 1 &&
        Routing::isPerTrackTarget(_editRoute.target())
    ) {
        canvas.setFont(Font::Tiny);
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(edit() && row == selectedRow() ? Color::Bright : Color::Medium);

        uint8_t tracks = _editRoute.tracks();
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            int px = x + i * 10;
            canvas.drawRect(px, y + 1, 8, 8);
            if (tracks & (1 << i)) {
                canvas.fillRect(px + 2, y + 3, 4, 4);
                if (_editRoute.hasNonDefaultShaping(i)) {
                    canvas.setColor(Color::Low);
                    canvas.fillRect(px + 1, y + 2, 3, 3);
                    canvas.setColor(edit() && row == selectedRow() ? Color::Bright : Color::Medium);
                }
            }
        }
    } else {
        ListPage::drawCell(canvas, row, column, x, y, w, h);
    }
}

void RoutingPage::selectRoute(int routeIndex) {
    routeIndex = clamp(routeIndex, 0, CONFIG_ROUTE_COUNT - 1);
    if (routeIndex != _routeIndex) {
        _engine.midiLearn().stop();
        showRoute(routeIndex);
    }
}

void RoutingPage::assignMidiLearn(const MidiLearn::Result &result) {
    auto &midiSource = _editRoute.midiSource();

    _editRoute.setSource(Routing::Source::Midi);

    midiSource.source().setPort(Types::MidiPort(result.port));
    midiSource.source().setChannel(result.channel);

    switch (result.event) {
    case MidiLearn::Event::ControlAbsolute:
        midiSource.setEvent(Routing::MidiSource::Event::ControlAbsolute);
        midiSource.setControlNumber(result.controlNumber);
        break;
    case MidiLearn::Event::ControlRelative:
        midiSource.setEvent(Routing::MidiSource::Event::ControlRelative);
        midiSource.setControlNumber(result.controlNumber);
        break;
    case MidiLearn::Event::PitchBend:
        midiSource.setEvent(Routing::MidiSource::Event::PitchBend);
        break;
    case MidiLearn::Event::Note:
        midiSource.setEvent(Routing::MidiSource::Event::NoteMomentary);
        midiSource.setNote(result.note);
        break;
    case MidiLearn::Event::Last:
        break;
    }

    setSelectedRow(int(RouteListModel::MidiSource));
    setTopRow(int(RouteListModel::MidiSource));
    setEdit(false);
}

void RoutingPage::enterBiasOverlay() {
    if (!Routing::isPerTrackTarget(_editRoute.target())) {
        showMessage("TARGET NOT PER TRACK");
        return;
    }
    _biasOverlayActive = true;
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        _biasStaging[i] = _editRoute.biasPct(i);
        _depthStaging[i] = _editRoute.depthPct(i);
    }
    _slotState.fill(0);
    _activeSlot = 0;
}

void RoutingPage::exitBiasOverlay(bool commit) {
    if (commit) {
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            _editRoute.setBiasPct(i, _biasStaging[i]);
            _editRoute.setDepthPct(i, _depthStaging[i]);
        }
        showMessage("BIAS/DEPTH UPDATED");
    }
    _biasOverlayActive = false;
}

int RoutingPage::focusTrackIndex() const {
    int base = _activeSlot * 2;
    uint8_t state = _slotState[_activeSlot] % 4;
    return base + (state >= 2 ? 1 : 0);
}

void RoutingPage::handleBiasOverlayKey(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.pageModifier() && key.isStep() && key.step() == 4) { // Page + S5 exits
        exitBiasOverlay(false);
        event.consume();
        return;
    }
    if (key.isFunction()) {
        int fn = key.function();
        if (fn >= 0 && fn <= 3) {
            if (_activeSlot != fn) {
                _activeSlot = fn;
            } else {
                _slotState[fn] = (_slotState[fn] + 1) % 4;
            }
            event.consume();
            return;
        }
        if (fn == 4) {
            exitBiasOverlay(true);
            event.consume();
            return;
        }
    }
}

void RoutingPage::editBiasOverlay(int delta, bool shift) {
    if (delta == 0) return;
    int step = shift ? 10 : 1;
    int track = focusTrackIndex();
    bool editBias = (_slotState[_activeSlot] % 2) == 0;
    auto &values = editBias ? _biasStaging : _depthStaging;
    int value = values[track] + delta * step;
    values[track] = clamp(value, -100, 100);
}

void RoutingPage::drawBiasOverlay(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ROUTE SHAPE");
    const char *functionNames[] = { "T1/2", "T3/4", "T5/6", "T7/8", "COMMIT" };
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), _activeSlot);

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    const int colWidth = CONFIG_LCD_WIDTH / CONFIG_FUNCTION_KEY_COUNT; // ~51px, matches F-key spacing
    const int lineSpacing = 10;
    // Align B/D to the same left edge using widest case "D +100"
    FixedStringBuilder<8> maxLine2("D %+d", 100);
    const int line2Width = canvas.textWidth(maxLine2);
    const int line2XOffset = (colWidth - line2Width) / 2;
    // Track prefix width for widest track number (T8 B )
    FixedStringBuilder<4> trackPrefix("T8 B ");
    const int trackPrefixWidth = canvas.textWidth(trackPrefix);
    const int topY = 16; // push 4px below header

    auto drawTrackBlock = [&] (int baseX, int baseY, int trackNumber, int bias, int depth, bool focusBias, bool focusDepth) {
        // Line 1: "Tn B %+d" aligned so the 'B' shares left edge with line2's 'D'
        FixedStringBuilder<12> line1("T%d B %+d", trackNumber, bias);
        // Compute width of "Tn " prefix to back up from the B start
        FixedStringBuilder<4> trackPrefixCurrent("T%d ", trackNumber);
        int currentPrefixWidth = canvas.textWidth(trackPrefixCurrent);
        int line1X = baseX + line2XOffset - trackPrefixWidth + currentPrefixWidth;
        canvas.setColor(focusBias ? Color::Bright : Color::Medium);
        canvas.drawText(line1X, baseY, line1);

        // Line 2: "D %+d"
        FixedStringBuilder<8> depthStr("D %+d", depth);
        int depthX = baseX + line2XOffset;
        canvas.setColor(focusDepth ? Color::Bright : Color::Medium);
        canvas.drawText(depthX, baseY + lineSpacing, depthStr);
    };

    for (int slot = 0; slot < 4; ++slot) {
        int x = slot * colWidth;
        int trackA = slot * 2;
        int trackB = trackA + 1;
        uint8_t state = _slotState[slot] % 4;
        bool focusBiasA = slot == _activeSlot && state == 0;
        bool focusDepthA = slot == _activeSlot && state == 1;
        bool focusBiasB = slot == _activeSlot && state == 2;
        bool focusDepthB = slot == _activeSlot && state == 3;

        drawTrackBlock(x, topY, trackA + 1, _biasStaging[trackA], _depthStaging[trackA], focusBiasA, focusDepthA);
        // Place bottom block closer to footer: reduce gap to keep last line 2px above footer
        int bottomY = topY + 2 * lineSpacing + 4;
        drawTrackBlock(x, bottomY, trackB + 1, _biasStaging[trackB], _depthStaging[trackB], focusBiasB, focusDepthB);
    }
}
