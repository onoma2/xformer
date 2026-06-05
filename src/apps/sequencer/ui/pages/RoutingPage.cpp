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

// Inert ListModel for the ListPage base: RoutingPage draws/edits only via the tab
// editor. routingTarget defaults to None, so the inline-modulation machinery never
// activates here.
class EmptyListModel : public ListModel {
public:
    int rows() const override { return 0; }
    int columns() const override { return 0; }
    void cell(int, int, StringBuilder &) const override {}
    void edit(int, int, int, bool) override {}
};

EmptyListModel gEmptyListModel;

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
    ListPage(manager, context, gEmptyListModel)
{
}

void RoutingPage::reset() {
    _engine.midiLearn().stop();
    enterTabEditor();
}

void RoutingPage::enter() {
    enterTabEditor();
    ListPage::enter();
}

void RoutingPage::exit() {
    _engine.midiLearn().stop();

    ListPage::exit();
}

void RoutingPage::draw(Canvas &canvas) {
    drawTabEditor(canvas);
}

void RoutingPage::keyPress(KeyPressEvent &event) {
    handleTabEditorKey(event);
}

void RoutingPage::encoder(EncoderEvent &event) {
    int n = tabBandParamCount();         // move the row cursor, re-point to that param's route
    _tabEditorRow = clamp(_tabEditorRow + event.value(), 0, n - 1);
    tabRefocus();
    event.consume();
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
        _tabRowRouted = true;
    } else {
        _tabRowRouted = false;
    }
}

void RoutingPage::enterTabEditor() {
    _tabEditorTab = 0;
    _tabEditorRow = 0;
    _tabScopeMask = 0;   // global scope by default
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
        case Function::Prev: // BACK: leave the routing page (edits are live, nothing to save)
            _manager.pop();
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
    event.consume(); // modal: swallow all other keys so they don't reach TopPage
}

void RoutingPage::keyboard(KeyboardEvent &event) {
    (void)event; // tab editor owns no keyboard editing
}
