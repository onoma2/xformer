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
    _tabCol = clamp(_tabCol, 0, CONFIG_TRACK_COUNT - 1);
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
    _tabCol = clamp(_project.selectedTrackIndex(), 0, CONFIG_TRACK_COUNT - 1);
    _tabScopeMask = 0;   // global scope by default
    tabRefocus();
}

// True when track t's engine owns paramKey (so a route there is meaningful). Global
// band params have no track dimension: eligibility is the same for every column.
static bool tabCellEligible(const Track &track, uint8_t paramKey) {
    Routing::Target target = RouteBrowse::paramKeyToTarget(paramKey);
    if (target == Routing::Target::None) return false;
    uint8_t key; RouteParam::Range range;
    if (Routing::isProjectTarget(target)) {
        return RouteFork::migratedGlobal(target, key, range);
    }
    return RouteFork::migrated(track.trackMode(), target, key, range);
}

void RoutingPage::drawTabEditor(Canvas &canvas) {
    const char *tabs[] = { "PITCH", "CLOCK", "PROB", "GLOB" };
    auto band = RouteBrowse::Band(_tabEditorTab);

    uint8_t keys[8];
    int n = RouteBrowse::bandParams(band, keys, 8);
    int cursorRow = clamp(_tabEditorRow, 0, (n > 0 ? n - 1 : 0));
    int cursorCol = clamp(_tabCol, 0, CONFIG_TRACK_COUNT - 1);
    uint8_t cursorKey = (n > 0) ? keys[cursorRow] : 0;

    WindowPainter::clear(canvas);
    canvas.setFont(Font::Tiny);

    // header: band name bright, view tag dim, MODULATE dim far right
    canvas.setColor(Color::Bright);
    canvas.drawText(2, 7, tabs[_tabEditorTab]);
    canvas.setColor(Color::Medium);
    canvas.drawText(40, 7, "DEPTH");
    canvas.drawText(CONFIG_LCD_WIDTH - canvas.textWidth("MODULATE") - 2, 7, "MODULATE");
    canvas.setColor(Color::Low);
    canvas.hline(0, 10, CONFIG_LCD_WIDTH);

    // grid geometry: left label gutter + 8 evenly-spaced track columns
    const int nameW = 38;
    const int gridL = nameW + 2;
    const int colW = (CONFIG_LCD_WIDTH - gridL - 1) / 8;

    // column headers: track number + engine letter, cursor column bright,
    // else medium when the cursor row's param is eligible for that engine, dim if not
    const int hdrY = 17;
    for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
        const Track &track = _project.track(t);
        int cx = gridL + t * colW + colW / 2;
        FixedStringBuilder<4> lbl("%d%c", t + 1, Track::trackModeLetter(track.trackMode()));
        if (t == cursorCol) {
            canvas.setColor(Color::Bright);
        } else {
            canvas.setColor(tabCellEligible(track, cursorKey) ? Color::Medium : Color::Low);
        }
        canvas.drawText(cx - canvas.textWidth(lbl) / 2, hdrY, lbl);
    }

    // param rows: name in the gutter (cursor row bright), then 8 cells. Per cell:
    //   '-' ineligible, '.' eligible+unrouted, depth number when routed.
    const int top = 20, rowH = 8;
    for (int r = 0; r < n; ++r) {
        int y = top + r * rowH;
        uint8_t key = keys[r];
        bool cursorRowSel = r == cursorRow;
        canvas.setColor(cursorRowSel ? Color::Bright : Color::Medium);
        canvas.drawText(2, y + 6, bandParamName(band, key));
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
            const Track &track = _project.track(t);
            int cx = gridL + t * colW + colW / 2;
            bool isCursor = cursorRowSel && t == cursorCol;
            if (!tabCellEligible(track, key)) {
                canvas.setColor(Color::Low);
                canvas.drawText(cx - 2, y + 6, "-");
                continue;
            }
            uint8_t scopeMask = Routing::isProjectTarget(RouteBrowse::paramKeyToTarget(key))
                ? 0 : uint8_t(1 << t);
            int routeIdx = routeForBandParam(key, scopeMask);
            if (routeIdx >= 0) {
                FixedStringBuilder<6> txt("%+d", _project.routing().route(routeIdx).depthPct(t));
                canvas.setColor(isCursor ? Color::Bright : Color::Medium);
                canvas.drawText(cx - canvas.textWidth(txt) / 2, y + 6, txt);
            } else {
                canvas.setColor(isCursor ? Color::Bright : Color::MediumLow);
                canvas.drawText(cx - canvas.textWidth(".") / 2, y + 6, ".");
            }
        }
    }

    const char *fn[] = { "BACK", _tabRowRouted ? "COMBINE" : nullptr, _tabRowRouted ? "SRC" : nullptr, nullptr, nullptr };
    WindowPainter::drawFooter(canvas, fn, pageKeyState());
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
