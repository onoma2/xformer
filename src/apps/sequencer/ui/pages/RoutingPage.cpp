#include "RoutingPage.h"

#include "ui/painters/WindowPainter.h"
#include "ui/pages/ContextMenu.h"
#include "ui/pages/Pages.h"

#include "model/RouteBrowse.h"
#include "model/RouteFork.h"
#include "model/ParamTableGlobal.h"
#include "model/ParamTableNote.h"

#include "ui/model/ListModel.h"

namespace {

// QuickEdit adapter for a route's unified depth: big centered readout + LED ring
// (mirrors GeneratorContextQuickEditModel). Edits all tracks held equal, live on the
// committed route the engine reads.
class RouteDepthQuickEditModel : public ListModel {
public:
    void configure(Routing::Route *route) { _route = route; }
    int rows() const override { return 1; }
    int columns() const override { return 2; }
    void cell(int row, int column, StringBuilder &str) const override {
        if (row != 0 || !_route) return;
        if (column == 0) str("DEPTH");
        else str("%+d%%", _route->depthPct(0));
    }
    void edit(int row, int column, int value, bool shift) override {
        if (row != 0 || column != 1 || !_route) return;
        setAll(clamp(_route->depthPct(0) + value * (shift ? 10 : 1), -100, 100));
    }
    int indexedCount(int) const override { return 16; }
    int indexed(int) const override {
        return _route ? clamp((_route->depthPct(0) + 100) * 15 / 200, 0, 15) : -1;
    }
    void setIndexed(int, int index) override {
        if (_route) setAll(clamp(index * 200 / 15 - 100, -100, 100));
    }
private:
    void setAll(int d) {
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) _route->setDepthPct(t, d);
    }
    Routing::Route *_route = nullptr;
};

RouteDepthQuickEditModel gRouteDepthQuickEditModel;

} // namespace

#include "core/utils/StringBuilder.h"
#include "core/math/Math.h"
#include "core/utils/Random.h"

#include <algorithm>
#include <cstring>

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
    _overviewActive = true;
}

void RoutingPage::enter() {
    _overviewActive = true;   // normal entry lands on the overview home
    ListPage::enter();
}

void RoutingPage::exit() {
    _engine.midiLearn().stop();

    ListPage::exit();
}

void RoutingPage::draw(Canvas &canvas) {
    if (overviewActive()) {
        drawOverview(canvas);
        return;
    }
    if (overlayActive()) {
        drawBiasOverlay(canvas);
        return;
    }
    if (tabEditorActive()) {
        drawTabEditor(canvas);
        return;
    }

    bool showCommit = *_route != _editRoute;
    bool showLearn = _editRoute.target() != Routing::Target::None;
    bool highlightLearn = showLearn && _engine.midiLearn().isActive();
    const char *functionNames[] = { "BACK", "NEXT", "INIT", showLearn ? "LEARN" : nullptr, showCommit ? "COMMIT" : nullptr };

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ROUTING");
    WindowPainter::drawActiveFunction(canvas, FixedStringBuilder<16>("ROUTE %d", _routeIndex + 1));
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), highlightLearn ? int(Function::Learn) : -1);

    ListPage::draw(canvas);
}

void RoutingPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (overviewActive()) {
        handleOverviewKey(event);
        return;
    }

    if (overlayActive()) {
        handleBiasOverlayKey(event);
        return;
    }

    if (tabEditorActive()) {
        handleTabEditorKey(event);
        return;
    }

    if (key.pageModifier() && key.isStep() && key.step() == 4) { // Page + S5
        enterBiasOverlay();
        event.consume();
        return;
    }

    if (key.pageModifier() && key.isStep() && key.step() == 5) { // Page + S6: tab editor
        enterTabEditor();
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
        case Function::Prev: // BACK to the route overview
            _engine.midiLearn().stop();
            _overviewActive = true;
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
            commitRoute();
            break;
        }
        event.consume();
    }

    ListPage::keyPress(event);
}

void RoutingPage::encoder(EncoderEvent &event) {
    if (overviewActive()) {
        overviewEncoder(event.value());
        event.consume();
        return;
    }
    if (overlayActive()) {
        bool shift = pageKeyState()[Key::Shift];
        editBiasOverlay(event.value(), shift);
        event.consume();
        return;
    }

    if (tabEditorActive()) {             // move the row cursor, re-point to that param's route
        int n = tabBandParamCount();
        _tabEditorRow = clamp(_tabEditorRow + event.value(), 0, n - 1);
        tabRefocus();
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
    _overviewActive = false;   // showing a route means the per-route editor
    _tabEditorActive = false;
    _route = &_project.routing().route(routeIndex);
    _routeIndex = routeIndex;
    _editRoute = *(initialValue ? initialValue : _route);
    _biasOverlayActive = false;

    // Initialize staging arrays with current route values
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        _biasStaging[i] = _editRoute.biasPct(i);
        _depthStaging[i] = _editRoute.depthPct(i);
        _shaperStaging[i] = _editRoute.shaper(i);
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
        _shaperStaging[i] = _editRoute.shaper(i);
    }
    _slotState.fill(0);
    _activeSlot = 0;
}

void RoutingPage::exitBiasOverlay(bool commit) {
    if (commit) {
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            _editRoute.setBiasPct(i, _biasStaging[i]);
            _editRoute.setDepthPct(i, _depthStaging[i]);
            _editRoute.setShaper(i, _shaperStaging[i]);
        }
        showMessage("BIAS/DEPTH/SHAPER UPDATED");
    }
    _biasOverlayActive = false;
}

int RoutingPage::focusTrackIndex() const {
    int base = _activeSlot * 2;
    uint8_t state = _slotState[_activeSlot] % 6;  // 6 states: A bias, A depth, A shaper, B bias, B depth, B shaper
    return base + (state >= 3 ? 1 : 0);
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
                _slotState[fn] = (_slotState[fn] + 1) % 6;  // 6 states: A bias, A depth, A shaper, B bias, B depth, B shaper
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
                        _shaperStaging[i] != _route->shaper(i)) {
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
                    _editRoute.setShaper(i, _shaperStaging[i]);
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
    } else {  // Shaper adjustment (2 for A shaper, 5 for B shaper)
        int dir = delta > 0 ? 1 : -1;
        int value = int(_shaperStaging[track]) + dir;
        if (value < 0) value = int(Routing::Shaper::Last) - 1;
        if (value >= int(Routing::Shaper::Last)) value = 0;
        _shaperStaging[track] = Routing::Shaper(value);
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
        _shaperStaging.fill(Routing::Shaper::None);
        showMessage("SHAPE INIT");
        break;
    }
    case 1: { // RANDOM
        static Random rng;
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            _biasStaging[i] = clamp<int>(rng.nextRange(201) - 100, -100, 100);
            _depthStaging[i] = clamp<int>(rng.nextRange(201) - 100, -100, 100);
            _shaperStaging[i] = Routing::Shaper(rng.nextRange(int(Routing::Shaper::Last)));
        }
        showMessage("SHAPE RANDOM");
        break;
    }
    case 2: { // COPY
        _biasClipboard = _biasStaging;
        _depthClipboard = _depthStaging;
        _shaperClipboard = _shaperStaging;
        _clipboardValid = true;
        showMessage("PARAMS COPIED");
        break;
    }
    case 3: { // PASTE
        if (_clipboardValid) {
            _biasStaging = _biasClipboard;
            _depthStaging = _depthClipboard;
            _shaperStaging = _shaperClipboard;
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
                _shaperStaging[i] != _route->shaper(i)) {
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
            int copy1 = std::min(splitIdx, 7);
            std::memcpy(line1, nameBuf, copy1);
            line1[copy1] = '\0';

            int copy2 = std::min(7, static_cast<int>(len) - splitIdx - 1);
            if (copy2 > 0) {
                std::memcpy(line2, nameBuf + splitIdx + 1, copy2); // Skip space
            }
            line2[std::max(0, copy2)] = '\0';
        } else {
            // Hard split
            std::memcpy(line1, nameBuf, 7);
            line1[7] = '\0';
            std::memcpy(line2, nameBuf + 7, 7);
            line2[7] = '\0';
        }

        int w1 = canvas.textWidth(line1);
        int w2 = canvas.textWidth(line2);

        canvas.drawText(f5Center - (w1 / 2), y1, line1);
        canvas.drawText(f5Center - (w2 / 2), y2, line2);
    }

    // Align B/D/S to fixed columns: use widest cases so positions don't shift with values
    FixedStringBuilder<8> maxLine2("D %+d", 100);
    const int line2Width = canvas.textWidth(maxLine2);
    const int line2XOffset = (colWidth - line2Width) / 2;

    // Calculate fixed offset for "Tn" based on widest possible Bias string
    FixedStringBuilder<8> maxBiasPart("B %+d ", -100);
    const int maxBiasWidth = canvas.textWidth(maxBiasPart);

    auto shaperShort = [] (Routing::Shaper shaper) -> const char * {
        switch (shaper) {
        case Routing::Shaper::None:               return "NO";
        case Routing::Shaper::Crease:             return "CR";
        case Routing::Shaper::Location:           return "LO";
        case Routing::Shaper::Envelope:           return "EN";
        case Routing::Shaper::TriangleFold:       return "TF";
        case Routing::Shaper::FrequencyFollower:  return "FF";
        case Routing::Shaper::Activity:           return "AC";
        case Routing::Shaper::ProgressiveDivider: return "PD";
        case Routing::Shaper::VcaNext:            return "VC";
        case Routing::Shaper::Last:               break;
        }
        return "?";
    };

    auto drawTrackBlock = [&] (int baseX, int baseY, int trackNumber, int bias, int depth, Routing::Shaper shaper, bool focusBias, bool focusDepth, bool focusShaper) {
        // Line 1: "B %+d ... Tn"
        int startX = baseX + line2XOffset;

        FixedStringBuilder<8> prefix("B %+d ", bias);
        FixedStringBuilder<4> tPart("T%d", trackNumber);

        canvas.setColor(focusBias ? Color::Bright : Color::Medium);
        canvas.drawText(startX, baseY, prefix);

        canvas.setColor((focusBias || focusDepth || focusShaper) ? Color::Bright : Color::Medium);
        canvas.drawText(startX + maxBiasWidth, baseY, tPart);

        // Line 2: "D %+d Ss"
        FixedStringBuilder<8> depthPart("D %+d ", depth);
        FixedStringBuilder<5> shaperPart("%s", shaperShort(shaper));

        int depthX = baseX + line2XOffset;
        int shaperX = depthX + canvas.textWidth(depthPart);

        canvas.setColor(focusDepth ? Color::Bright : Color::Medium);
        canvas.drawText(depthX, baseY + lineSpacing, depthPart);

        canvas.setColor(focusShaper ? Color::Bright : Color::Medium);
        canvas.drawText(shaperX, baseY + lineSpacing, shaperPart);
    };
    for (int slot = 0; slot < 4; ++slot) {
        int x = slot * colWidth;
        int trackA = slot * 2;
        int trackB = trackA + 1;
        uint8_t state = _slotState[slot] % 6;  // 6 states: A bias, A depth, A shaper, B bias, B depth, B shaper
        bool focusBiasA = (slot == _activeSlot) && (state == 0);  // A bias
        bool focusDepthA = (slot == _activeSlot) && (state == 1); // A depth
        bool focusShaperA = (slot == _activeSlot) && (state == 2); // A shaper
        bool focusBiasB = (slot == _activeSlot) && (state == 3); // B bias
        bool focusDepthB = (slot == _activeSlot) && (state == 4); // B depth
        bool focusShaperB = (slot == _activeSlot) && (state == 5); // B shaper

        drawTrackBlock(x, topY, trackA + 1, _biasStaging[trackA], _depthStaging[trackA], _shaperStaging[trackA], focusBiasA, focusDepthA, focusShaperA);
        // Place bottom block closer to footer: reduce gap to keep last line 2px above footer
        int bottomY = topY + 2 * lineSpacing + 4;
        drawTrackBlock(x, bottomY, trackB + 1, _biasStaging[trackB], _depthStaging[trackB], _shaperStaging[trackB], focusBiasB, focusDepthB, focusShaperB);
    }
}

void RoutingPage::commitRoute() {
    _engine.midiLearn().stop();
    int conflict = _project.routing().checkRouteConflict(_editRoute, *_route);
    if (conflict >= 0) {
        showMessage(FixedStringBuilder<64>("ROUTE SETTINGS CONFLICT WITH ROUTE %d", conflict + 1));
    } else {
        bool busFeedback = Routing::isBusSource(_editRoute.source()) && Routing::isBusTarget(_editRoute.target());
        *_route = _editRoute;
        setEdit(false);
        showMessage(busFeedback ? "ROUTE CHG FB" : "ROUTE CHANGED");
    }
}

static void overviewSourceTarget(const Routing::Route &route, StringBuilder &str) {
    Routing::printSource(route.source(), str);
    str(">");
    str(Routing::targetName(route.target()));
}

void RoutingPage::drawOverview(Canvas &canvas) {
    const auto &routing = _project.routing();

    int occupied = 0;
    for (int i = 0; i < CONFIG_ROUTE_COUNT; ++i) {
        if (routing.route(i).active()) ++occupied;
    }

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ROUTING");
    WindowPainter::drawActiveFunction(canvas, FixedStringBuilder<16>("%d/%d", occupied, CONFIG_ROUTE_COUNT));
    const char *functionNames[] = { nullptr, "ADD", nullptr, nullptr, "EDIT" };
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    canvas.setFont(Font::Tiny);
    const int rowH = 6;
    const int topY = 18;
    const int visible = 6;

    for (int i = 0; i < visible; ++i) {
        int idx = _overviewScroll + i;
        if (idx >= CONFIG_ROUTE_COUNT) break;
        const auto &route = routing.route(idx);
        int y = topY + i * rowH;
        bool sel = idx == _overviewSel;

        if (sel) {
            canvas.setColor(Color::MediumBright);
            canvas.fillRect(0, y - 1, CONFIG_LCD_WIDTH - 6, rowH);
            canvas.setBlendMode(BlendMode::Sub);
        }

        canvas.setColor(route.active() ? Color::Bright : Color::Low);
        canvas.drawText(3, y + 4, FixedStringBuilder<4>("%d", idx + 1));

        if (route.active()) {
            FixedStringBuilder<32> st;
            overviewSourceTarget(route, st);
            canvas.setColor(Color::Bright);
            canvas.drawText(18, y + 4, st);
            // per-track mask (8 cells) or "global"
            if (Routing::isPerTrackTarget(route.target())) {
                uint8_t tracks = route.tracks();
                for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
                    int px = 150 + t * 5;
                    if (tracks & (1 << t)) {
                        canvas.setColor(Color::Bright);
                        canvas.fillRect(px, y, 4, 4);
                    } else {
                        canvas.setColor(Color::Low);
                        canvas.drawRect(px, y, 4, 4);
                    }
                }
            } else {
                canvas.setColor(Color::Medium);
                canvas.drawText(150, y + 4, "global");
            }
        } else {
            canvas.setColor(Color::Low);
            canvas.drawText(18, y + 4, "--");
        }

        if (sel) canvas.setBlendMode(BlendMode::Set);
    }

    // scrollbar
    int trackTop = 12, trackH = CONFIG_LCD_HEIGHT - 11 - trackTop;
    canvas.setColor(Color::Low);
    canvas.vline(CONFIG_LCD_WIDTH - 3, trackTop, trackH);
    int thumbH = std::max(4, trackH * visible / CONFIG_ROUTE_COUNT);
    int thumbY = trackTop + trackH * _overviewScroll / CONFIG_ROUTE_COUNT;
    canvas.setColor(Color::Bright);
    canvas.fillRect(CONFIG_LCD_WIDTH - 4, thumbY, 3, thumbH);
}

void RoutingPage::syncOverviewScroll() {
    const int visible = 6;
    _overviewSel = clamp(_overviewSel, 0, CONFIG_ROUTE_COUNT - 1);
    if (_overviewSel < _overviewScroll) _overviewScroll = _overviewSel;
    if (_overviewSel >= _overviewScroll + visible) _overviewScroll = _overviewSel - visible + 1;
}

void RoutingPage::overviewEncoder(int delta) {
    _overviewSel += delta;
    syncOverviewScroll();
}

void RoutingPage::openRouteFromOverview() {
    showRoute(_overviewSel);   // sets _overviewActive = false
}

void RoutingPage::handleOverviewKey(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Next: { // ADD: open the first empty route
            int empty = _project.routing().findEmptyRoute();
            if (empty < 0) {
                showMessage("NO EMPTY ROUTES");
                break;
            }
            _overviewSel = empty;
            syncOverviewScroll();
            openRouteFromOverview();
            break;
        }
        case Function::Commit: // EDIT: open selected
            openRouteFromOverview();
            break;
        default:
            break;
        }
        event.consume();
    }
}

static char trackModeLetter(Track::TrackMode mode) {
    switch (mode) {
    case Track::TrackMode::Note:        return 'N';
    case Track::TrackMode::Curve:       return 'C';
    case Track::TrackMode::MidiCv:      return 'M';
    case Track::TrackMode::Tuesday:     return 'A'; // Algo
    case Track::TrackMode::DiscreteMap: return 'D';
    case Track::TrackMode::Indexed:     return 'I';
    case Track::TrackMode::Teletype:    return 'T';
    case Track::TrackMode::Stochastic:  return 'S';
    case Track::TrackMode::PhaseFlux:   return 'P';
    default:                            return '?';
    }
}

// Name for a band ParamKey: Global keys from the global table, the rest from Note
// (which backs every per-track band param). Reuses the staged row names, no new strings.
static const char *bandParamName(RouteBrowse::Band band, uint8_t key) {
    const RouteParam::Table &tbl = (band == RouteBrowse::Band::Global)
        ? GlobalParamTable::table() : NoteParamTable::table();
    const RouteParam::Row *row = tbl.find(key);
    return row ? row->name : "?";
}

// Find the active route targeting paramKey within the scope; -1 if none.
int RoutingPage::routeForBandParam(uint8_t paramKey, uint8_t trackMask) const {
    const auto &routing = _project.routing();
    for (int r = 0; r < CONFIG_ROUTE_COUNT; ++r) {
        if (RouteBrowse::matches(routing.route(r), paramKey, trackMask)) {
            return r;
        }
    }
    return -1;
}

int RoutingPage::tabBandParamCount() const {
    uint8_t keys[8];
    return RouteBrowse::bandParams(RouteBrowse::Band(_tabEditorTab), keys, 8);
}

// Create a route for the cursor's band param at the editor scope. Allocates an empty
// slot, sets target (from the band key) + tracks (scope, or the selected track if the
// scope is global but the band is per-track) + a default CV1 source, then focuses it
// in edit mode so the encoder dials depth immediately.
void RoutingPage::tabCreateRoute() {
    uint8_t keys[8];
    int n = RouteBrowse::bandParams(RouteBrowse::Band(_tabEditorTab), keys, 8);
    if (n == 0) return;
    uint8_t key = keys[clamp(_tabEditorRow, 0, n - 1)];
    Routing::Target target = RouteBrowse::paramKeyToTarget(key);
    if (target == Routing::Target::None) return;

    int slot = _project.routing().findEmptyRoute();
    if (slot < 0) { showMessage("NO EMPTY ROUTES"); return; }

    Routing::Route route;
    route.clear();
    route.setTarget(target);
    if (Routing::isPerTrackTarget(target)) {
        uint8_t mask = _tabScopeMask ? _tabScopeMask : uint8_t(1 << _project.selectedTrackIndex());
        route.setTracks(mask);
    }
    route.setSource(Routing::Source::CvIn1);
    for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
        route.setDepthPct(t, 0);   // silent until the user dials depth (edit mode opens here)
    }

    // reject if the new route collides with an existing one (same guard as commit)
    int conflict = _project.routing().checkRouteConflict(route, route);
    if (conflict >= 0) {
        showMessage(FixedStringBuilder<64>("CONFLICT WITH ROUTE %d", conflict + 1));
        return;
    }

    _project.routing().route(slot) = route;
    _route = &_project.routing().route(slot);
    _routeIndex = slot;
    _editRoute = *_route;
    _tabRowRouted = true;
    // open the depth modal so the encoder dials the new route's depth up from 0
    gRouteDepthQuickEditModel.configure(_route);
    _manager.pages().quickEdit.show(gRouteDepthQuickEditModel, 0);
}

// Point _route at the cursor row's committed route, if that param is routed at the
// scope. The tab editor edits _route live; unrouted rows leave _tabRowRouted false
// (cursor shows "+ADD", encoder-press creates).
void RoutingPage::tabRefocus() {
    uint8_t keys[8];
    int n = RouteBrowse::bandParams(RouteBrowse::Band(_tabEditorTab), keys, 8);
    _tabEditorRow = clamp(_tabEditorRow, 0, (n > 0 ? n - 1 : 0));
    int routeIdx = (n > 0) ? routeForBandParam(keys[_tabEditorRow], _tabScopeMask) : -1;
    if (routeIdx >= 0) {
        _route = &_project.routing().route(routeIdx);
        _routeIndex = routeIdx;
        _editRoute = *_route;
        _tabRowRouted = true;
    } else {
        _tabRowRouted = false;
    }
}

// Which of the 4 tab bands contains paramKey; -1 if none.
static int bandOfParamKey(uint8_t key) {
    uint8_t keys[8];
    for (int b = 0; b < 4; ++b) {
        int n = RouteBrowse::bandParams(RouteBrowse::Band(b), keys, 8);
        for (int i = 0; i < n; ++i) {
            if (keys[i] == key) return b;
        }
    }
    return -1;
}

void RoutingPage::enterTabEditor() {
    _tabEditorActive = true;
    // scope = the entered route's scope; land the cursor on the entered route's param
    bool perTrack = Routing::isPerTrackTarget(_editRoute.target());
    _tabScopeMask = perTrack ? uint8_t(_editRoute.tracks()) : 0;
    uint8_t key = RouteFork::targetToParamKey(_editRoute.target());
    int band = bandOfParamKey(key);
    if (band >= 0) {
        _tabEditorTab = band;
        uint8_t keys[8];
        int n = RouteBrowse::bandParams(RouteBrowse::Band(band), keys, 8);
        _tabEditorRow = 0;
        for (int i = 0; i < n; ++i) {
            if (keys[i] == key) { _tabEditorRow = i; break; }
        }
    } else {
        _tabEditorTab = 0;
        _tabEditorRow = 0;
    }
    tabRefocus();
}

void RoutingPage::drawTabEditor(Canvas &canvas) {
    const char *tabs[] = { "PITCH", "CLOCK", "PROB", "GLOB" };
    const int tabCount = 4;

    // cursor param name in the header. Tab editor edits the committed route live, so
    // there is no draft/commit — depth (modal) and combine (F2) apply immediately.
    uint8_t curKeys[8];
    int curN = RouteBrowse::bandParams(RouteBrowse::Band(_tabEditorTab), curKeys, 8);
    uint8_t cursorKey = (curN > 0) ? curKeys[clamp(_tabEditorRow, 0, curN - 1)] : 0;
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "ROUTE");
    WindowPainter::drawActiveFunction(canvas, bandParamName(RouteBrowse::Band(_tabEditorTab), cursorKey));
    const char *fn[] = { "BACK", _tabRowRouted ? "COMBINE" : nullptr, _tabRowRouted ? "SRC" : nullptr, nullptr, nullptr };
    WindowPainter::drawFooter(canvas, fn, pageKeyState());

    canvas.setFont(Font::Tiny);

    // left vertical tab column
    const int tabX = 2, tabW = 40, tabTop = 14, tabH = 9;
    for (int i = 0; i < tabCount; ++i) {
        int y = tabTop + i * tabH;
        bool active = i == _tabEditorTab;
        if (active) {
            canvas.setColor(Color::MediumBright);
            canvas.fillRect(tabX, y, tabW, tabH - 1);
            canvas.setBlendMode(BlendMode::Sub);
            canvas.drawText(tabX + 3, y + 6, tabs[i]);
            canvas.setBlendMode(BlendMode::Set);
        } else {
            canvas.setColor(Color::Low);
            canvas.drawText(tabX + 3, y + 6, tabs[i]);
        }
    }
    canvas.setColor(Color::Low);
    canvas.vline(tabW + 2, 12, CONFIG_LCD_HEIGHT - 23);

    auto band = RouteBrowse::Band(_tabEditorTab);

    // band param list: each param resolved to its routed state at the fixed scope.
    // The cursor row (_tabEditorRow) drives the highlight; its route is _route (the
    // committed slot), edited live.
    uint8_t keys[8];
    int n = RouteBrowse::bandParams(band, keys, 8);
    const int cx = tabW + 6, listTop = 15, rowH = 6;
    const int depthR = CONFIG_LCD_WIDTH - 4;
    for (int i = 0; i < n; ++i) {
        int y = listTop + i * rowH;
        uint8_t key = keys[i];
        bool cursor = i == _tabEditorRow;
        int depth = 0;
        bool routed;
        if (cursor) {
            routed = _tabRowRouted;
            if (routed && _route) depth = _route->depthPct(0);
        } else {
            int routeIdx = routeForBandParam(key, _tabScopeMask);
            routed = routeIdx >= 0;
            if (routed) depth = _project.routing().route(routeIdx).depthPct(0);
        }

        if (cursor) {
            canvas.setColor(Color::MediumBright);
            canvas.fillRect(cx - 2, y - 1, CONFIG_LCD_WIDTH - cx - 2, rowH);
            canvas.setBlendMode(BlendMode::Sub);
        }
        canvas.setColor(routed ? Color::Bright : Color::Low);
        canvas.drawText(cx, y + 4, bandParamName(band, key));
        if (routed) {
            FixedStringBuilder<10> dep("%+d%%", depth);
            canvas.setColor(Color::Medium);
            canvas.drawText(depthR - canvas.textWidth(dep), y + 4, dep);
        } else {
            const char *hint = cursor ? "+ADD" : "--"; // cursor on empty row: press to add
            canvas.setColor(cursor ? Color::Medium : Color::Low);
            canvas.drawText(depthR - canvas.textWidth(hint), y + 4, hint);
        }
        if (cursor) canvas.setBlendMode(BlendMode::Set);
    }

    // scope mini-map + combine for the focused route, compact at the bottom
    // show the focused route's actual tracks when routed; else the entry scope
    uint8_t mapMask = (_tabRowRouted && _route && Routing::isPerTrackTarget(_route->target()))
        ? uint8_t(_route->tracks()) : _tabScopeMask;
    const int mapX = CONFIG_LCD_WIDTH - 8 * 11 - 1, mapY = CONFIG_LCD_HEIGHT - 19;
    if (mapMask != 0) {
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
            int px = mapX + t * 11;
            char letter[2] = { trackModeLetter(_project.track(t).trackMode()), 0 };
            if (mapMask & (1 << t)) {
                canvas.setColor(Color::Bright);
                canvas.fillRect(px, mapY, 10, 7);
                canvas.setBlendMode(BlendMode::Sub);
                canvas.drawText(px + 3, mapY + 5, letter);
                canvas.setBlendMode(BlendMode::Set);
            } else {
                canvas.setColor(Color::Low);
                canvas.drawText(px + 3, mapY + 5, letter);
            }
        }
    } else {
        canvas.setColor(Color::Medium);
        canvas.drawText(mapX, mapY + 5, "global");
    }
    if (_tabRowRouted && _route) {
        canvas.setColor(Color::Medium);
        canvas.drawText(cx, mapY + 5,
            _route->combine() == RouteApply::Combine::Absolute ? "ABS" : "MOD");
    }
}

void RoutingPage::handleTabEditorKey(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isLeft() || key.isRight()) {
        _tabEditorTab = (_tabEditorTab + (key.isLeft() ? 3 : 1)) % 4;
        _tabEditorRow = 0;
        tabRefocus();
        event.consume();
        return;
    }
    if (key.isEncoder()) {
        if (_tabRowRouted) {            // routed row: open the depth quick-edit modal (live)
            gRouteDepthQuickEditModel.configure(_route);
            _manager.pages().quickEdit.show(gRouteDepthQuickEditModel, 0);
        } else {
            tabCreateRoute();           // empty row: create a route here
        }
        event.consume();
        return;
    }
    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Prev: // BACK to overview (edits are live, nothing to save)
            _tabEditorActive = false;
            _overviewActive = true;
            break;
        case Function::Next: // COMBINE: toggle Modulate <-> Absolute on the focused route (live)
            if (_tabRowRouted && _route) {
                _route->setCombine(_route->combine() == RouteApply::Combine::Absolute
                                   ? RouteApply::Combine::Modulate : RouteApply::Combine::Absolute);
            }
            break;
        case Function::Init: // SRC: pick the focused route's source (live on confirm)
            if (_tabRowRouted && _route) {
                Routing::Route *route = _route;
                _manager.pages().routeSourceSelect.show(route->target(), route->source(),
                    [route] (bool ok, Routing::Source source) {
                        if (ok) {
                            route->setSource(source);
                        }
                    });
            }
            break;
        default:
            break;
        }
        event.consume();
        return;
    }
    if (key.pageModifier() && key.isStep() && key.step() == 5) { // Page+S6 exits to old editor
        if (_route) _editRoute = *_route;   // resync so the old editor sees the live edits
        _tabEditorActive = false;
        event.consume();
        return;
    }
    event.consume(); // modal: swallow all other keys so they don't reach TopPage
}

void RoutingPage::keyboard(KeyboardEvent &event) {
    if (tabEditorActive() || overviewActive()) { // read-only / list views own no keyboard editing
        return;
    }
    if (event.isPressed()) {
        if (event.keycode() == KeyboardEvent::KeyEnter) {
            commitRoute();
            event.consume();
            return;
        }
        if (event.keycode() == KeyboardEvent::KeyEscape) {
            _manager.pop();
            event.consume();
            return;
        }
    }
    if (!handleFunctionKeys(event)) {
        ListPage::keyboard(event);
    }
}
