#include "RoutingPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/pages/ContextMenu.h"

#include "core/utils/StringBuilder.h"
#include "core/math/Math.h"
#include "core/utils/Random.h"

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
    _creaseStaging.fill(false);
    _creaseClipboard.fill(false);
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

    // Initialize staging arrays with current route values
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        _biasStaging[i] = _editRoute.biasPct(i);
        _depthStaging[i] = _editRoute.depthPct(i);
        _creaseStaging[i] = _editRoute.creaseEnabled(i);
    }

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
        _creaseStaging[i] = _editRoute.creaseEnabled(i);
    }
    _slotState.fill(0);
    _activeSlot = 0;
}

void RoutingPage::exitBiasOverlay(bool commit) {
    if (commit) {
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            _editRoute.setBiasPct(i, _biasStaging[i]);
            _editRoute.setDepthPct(i, _depthStaging[i]);
            _editRoute.setCreaseEnabled(i, _creaseStaging[i]);
        }
        showMessage("BIAS/DEPTH/CREASE UPDATED");
    }
    _biasOverlayActive = false;
}

int RoutingPage::focusTrackIndex() const {
    int base = _activeSlot * 2;
    uint8_t state = _slotState[_activeSlot] % 6;  // 6 states: A bias, A depth, A crease, B bias, B depth, B crease
    return base + (state >= 3 ? 1 : 0);  // Tracks A (0-3) and B (3-5) correspond to track indices
}

void RoutingPage::handleBiasOverlayKey(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.pageModifier() && key.isStep() && key.step() == 4) { // Page + S5 exits
        exitBiasOverlay(false);
        event.consume();
        return;
    }
    if (key.isContextMenu()) {
        showBiasOverlayContext();
        event.consume();
        return;
    }
    if (key.isFunction()) {
        int fn = key.function();
        if (fn >= 0 && fn <= 3) {
            if (_activeSlot != fn) {
                _activeSlot = fn;
            } else {
                _slotState[fn] = (_slotState[fn] + 1) % 6;  // 6 states: A bias, A depth, A crease, B bias, B depth, B crease
            }
            event.consume();
            return;
        }
        if (fn == 4) {
            // Check for changes
            bool changed = false;
            if (*_route != _editRoute) {
                changed = true;
            } else {
                for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
                    if (_biasStaging[i] != _route->biasPct(i) ||
                        _depthStaging[i] != _route->depthPct(i) ||
                        _creaseStaging[i] != _route->creaseEnabled(i)) {
                        changed = true;
                        break;
                    }
                }
            }

            if (changed) {
                // Apply staging to editRoute
                for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
                    _editRoute.setBiasPct(i, _biasStaging[i]);
                    _editRoute.setDepthPct(i, _depthStaging[i]);
                    _editRoute.setCreaseEnabled(i, _creaseStaging[i]);
                }

                // Check conflict
                int conflict = _project.routing().checkRouteConflict(_editRoute, *_route);
                if (conflict >= 0) {
                    showMessage(FixedStringBuilder<64>("ROUTE SETTINGS CONFLICT WITH ROUTE %d", conflict + 1));
                } else {
                    *_route = _editRoute;
                    showMessage("ROUTE SAVED");
                }
            } else {
                exitBiasOverlay(false);
            }

            event.consume();
            return;
        }
    }
}

void RoutingPage::editBiasOverlay(int delta, bool shift) {
    if (delta == 0) return;
    int track = focusTrackIndex();
    uint8_t state = _slotState[_activeSlot] % 6;

    if (state == 0 || state == 3) {  // Bias adjustment (0 for A bias, 3 for B bias)
        int step = shift ? 10 : 1;
        int value = _biasStaging[track] + delta * step;
        _biasStaging[track] = clamp(value, -100, 100);
    } else if (state == 1 || state == 4) {  // Depth adjustment (1 for A depth, 4 for B depth)
        int step = shift ? 10 : 1;
        int value = _depthStaging[track] + delta * step;
        _depthStaging[track] = clamp(value, -100, 100);
    } else {  // Crease toggle (2 for A crease, 5 for B crease)
        // For crease, delta toggles the state
        if (delta > 0) {
            _creaseStaging[track] = !_creaseStaging[track]; // Toggle with positive delta
        }
    }
}

void RoutingPage::showBiasOverlayContext() {
    static const ContextMenuModel::Item items[] = {
        { "INIT" },
        { "RANDOM" },
        { "COPY" },
        { "PASTE" },
    };
    showContextMenu(ContextMenu(
        items,
        int(sizeof(items) / sizeof(items[0])),
        [this] (int index) { biasOverlayContextAction(index); }
    ));
}

void RoutingPage::biasOverlayContextAction(int index) {
    switch (index) {
    case 0: { // INIT
        _biasStaging.fill(Routing::Route::DefaultBiasPct);
        _depthStaging.fill(Routing::Route::DefaultDepthPct);
        _creaseStaging.fill(false);
        showMessage("SHAPE INIT");
        break;
    }
    case 1: { // RANDOM
        static Random rng;
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            _biasStaging[i] = clamp<int>(rng.nextRange(201) - 100, -100, 100);
            _depthStaging[i] = clamp<int>(rng.nextRange(201) - 100, -100, 100);
            _creaseStaging[i] = (rng.next() % 2) == 0;  // Random true/false
        }
        showMessage("SHAPE RANDOM");
        break;
    }
    case 2: { // COPY
        _biasClipboard = _biasStaging;
        _depthClipboard = _depthStaging;
        _creaseClipboard = _creaseStaging;
        _clipboardValid = true;
        showMessage("PARAMS COPIED");
        break;
    }
    case 3: { // PASTE
        if (_clipboardValid) {
            _biasStaging = _biasClipboard;
            _depthStaging = _depthClipboard;
            _creaseStaging = _creaseClipboard;
            showMessage("PARAMS PASTED");
        } else {
            showMessage("CLIPBOARD EMPTY");
        }
        break;
    }
    default:
        break;
    }
}

void RoutingPage::drawBiasOverlay(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ROUTE SHAPE");

    // Check for changes to determine if we show COMMIT or EXIT
    bool changed = false;
    if (*_route != _editRoute) {
        changed = true;
    } else {
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            if (_biasStaging[i] != _route->biasPct(i) ||
                _depthStaging[i] != _route->depthPct(i) ||
                _creaseStaging[i] != _route->creaseEnabled(i)) {
                changed = true;
                break;
            }
        }
    }

    const char *functionNames[] = { "T1/2", "T3/4", "T5/6", "T7/8", changed ? "COMMIT" : "EXIT" };
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), _activeSlot);

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    const int colWidth = CONFIG_LCD_WIDTH / CONFIG_FUNCTION_KEY_COUNT; // ~51px, matches F-key spacing
    const int lineSpacing = 8;
    const int topY = 18; // push 4px below header

    // Draw Target Name above F5
    // F5 area is from (4 * colWidth) to Width
    int f5X = 4 * colWidth;
    int f5Width = CONFIG_LCD_WIDTH - f5X;
    int f5Center = f5X + (f5Width / 2);

    // Get target name and split into max 2 lines of 7 chars
    const char *targetName = Routing::targetName(_editRoute.target());

    char nameBuf[16]; // adequate for safety
    strncpy(nameBuf, targetName, sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';

    size_t len = strlen(nameBuf);

    // Draw Line 1
    int y1 = topY;
    int y2 = topY + lineSpacing;

    canvas.setColor(Color::Bright);

    if (len <= 7) {
        int w = canvas.textWidth(nameBuf);
        canvas.drawText(f5Center - (w / 2), y1 + 4, nameBuf); // vertically centered in the block
    } else {
        // Split into two lines.
        // Heuristic: If there's a space near the middle, split there.
        // Otherwise split at 7.

        char line1[8] = {0};
        char line2[8] = {0};

        // Find split point
        int splitIdx = 7;
        bool foundSpace = false;

        // Look for space in first 7 chars, preferring later ones
        for (int i = 6; i >= 0; --i) {
            if (nameBuf[i] == ' ') {
                splitIdx = i;
                foundSpace = true;
                break;
            }
        }

        if (foundSpace) {
            strncpy(line1, nameBuf, splitIdx);
            strncpy(line2, nameBuf + splitIdx + 1, 7); // Skip space
        } else {
            // Hard split
            strncpy(line1, nameBuf, 7);
            strncpy(line2, nameBuf + 7, 7);
        }

        int w1 = canvas.textWidth(line1);
        int w2 = canvas.textWidth(line2);

        canvas.drawText(f5Center - (w1 / 2), y1, line1);
        canvas.drawText(f5Center - (w2 / 2), y2, line2);
    }

        // Align B/D to fixed columns: use widest cases so positions don't shift with values
        FixedStringBuilder<8> maxLine2("D %+d", 100);
        const int line2Width = canvas.textWidth(maxLine2);
        const int line2XOffset = (colWidth - line2Width) / 2;

        // Calculate fixed offset for "Tn" based on widest possible Bias string
        FixedStringBuilder<8> maxBiasPart("B %+d ", -100);
        const int maxBiasWidth = canvas.textWidth(maxBiasPart);

        auto drawTrackBlock = [&] (int baseX, int baseY, int trackNumber, int bias, int depth, bool crease, bool focusBias, bool focusDepth, bool focusCrease) {            // Line 1: "B %+d ... Tn"
            // Align B with D (start at line2XOffset)
            int startX = baseX + line2XOffset;

            FixedStringBuilder<8> prefix("B %+d ", bias);
            FixedStringBuilder<4> tPart("T%d", trackNumber);

            canvas.setColor(focusBias ? Color::Bright : Color::Medium);
            canvas.drawText(startX, baseY, prefix);

            // Highlight T part if either Bias, Depth or Crease is focused
            canvas.setColor((focusBias || focusDepth || focusCrease) ? Color::Bright : Color::Medium);
            // Position T part at fixed offset relative to start, so it doesn't move with bias value length
            canvas.drawText(startX + maxBiasWidth, baseY, tPart);

            // Line 2: "D %+d L" or "D %+d C" - draw in parts to allow separate highlighting
            FixedStringBuilder<8> depthPart("D %+d ", depth);  // The "D %+d " part
            FixedStringBuilder<2> modePart("%c", crease ? 'C' : 'L');  // The "L" or "C" part

            int depthX = baseX + line2XOffset;
            int modeX = depthX + canvas.textWidth(depthPart);  // Position mode part right after depth part

            // Draw depth part
            canvas.setColor(focusDepth ? Color::Bright : Color::Medium);
            canvas.drawText(depthX, baseY + lineSpacing, depthPart);

            // Draw mode part (L/C)
            canvas.setColor(focusCrease ? Color::Bright : Color::Medium);
            canvas.drawText(modeX, baseY + lineSpacing, modePart);
        };
    for (int slot = 0; slot < 4; ++slot) {
        int x = slot * colWidth;
        int trackA = slot * 2;
        int trackB = trackA + 1;
        uint8_t state = _slotState[slot] % 6;  // 6 states: A bias, A depth, A crease, B bias, B depth, B crease
        bool focusBiasA = (slot == _activeSlot) && (state == 0);  // A bias
        bool focusDepthA = (slot == _activeSlot) && (state == 1); // A depth
        bool focusCreaseA = (slot == _activeSlot) && (state == 2); // A crease
        bool focusBiasB = (slot == _activeSlot) && (state == 3); // B bias
        bool focusDepthB = (slot == _activeSlot) && (state == 4); // B depth
        bool focusCreaseB = (slot == _activeSlot) && (state == 5); // B crease

        drawTrackBlock(x, topY, trackA + 1, _biasStaging[trackA], _depthStaging[trackA], _creaseStaging[trackA], focusBiasA, focusDepthA, focusCreaseA);
        // Place bottom block closer to footer: reduce gap to keep last line 2px above footer
        int bottomY = topY + 2 * lineSpacing + 4;
        drawTrackBlock(x, bottomY, trackB + 1, _biasStaging[trackB], _depthStaging[trackB], _creaseStaging[trackB], focusBiasB, focusDepthB, focusCreaseB);
    }
}
