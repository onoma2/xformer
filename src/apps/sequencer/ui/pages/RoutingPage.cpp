#include "RoutingPage.h"

#include "ui/painters/WindowPainter.h"
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

    if (tabEditorActive()) {
        handleTabEditorKey(event);
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
