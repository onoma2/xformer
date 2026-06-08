#include "RoutingPage.h"

#include "ui/painters/WindowPainter.h"

#include "model/RouteBrowse.h"
#include "model/RouteDraft.h"
#include "model/RouteFork.h"
#include "model/ParamTableGlobal.h"
#include "model/ParamTableNote.h"
#include "engine/RouteMidiLearn.h"

#include "ui/model/ListModel.h"

namespace {

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
    View    = 0,
    Edit    = 1,
    Combine = 2,
    Cancel  = 3,
    Commit  = 4,
};

static bool tabCellEligible(const Track &track, uint8_t paramKey);

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
    if (_matrixEditActive) {            // discard an in-flight draft (frees a new slot)
        matrixExitEdit(false);
    }
    _engine.midiLearn().stop();

    ListPage::exit();
}

void RoutingPage::draw(Canvas &canvas) {
    if (tabIsBus(_tabEditorTab)) {
        drawBus(canvas);
    } else if (tabIsMidi(_tabEditorTab)) {
        drawMidi(canvas);
    } else {
        drawTabEditor(canvas);
    }
}

void RoutingPage::keyPress(KeyPressEvent &event) {
    // Page+key belongs to TopPage (mode switch; Page+Routing again -> Modulator). Don't let
    // the matrix's T/step/function handlers consume it, or you can't leave the routing page.
    if (event.key().pageModifier()) {
        return;
    }
    if (tabIsBus(_tabEditorTab)) {
        handleBusKey(event);
    } else if (tabIsMidi(_tabEditorTab)) {
        handleMidiKey(event);
    } else {
        handleTabEditorKey(event);
    }
}

void RoutingPage::encoder(EncoderEvent &event) {
    if (tabIsBus(_tabEditorTab)) {           // bus tab
        if (_matrixEditActive) {        // editing a lane: dial the draft source/depth
            if (_matrixView == MatrixView::Source) {
                matrixEditSource(event.value(), event.pressed() || globalKeyState()[Key::Shift]);
            } else {
                int d = clamp(_matrixDraft.route.depthPct(0) + event.value(), -100, 100);
                _matrixDraft.route.setDepthPct(0, d);
            }
        } else {                        // nav: move the lane cursor 0..3
            _busLane = clamp(_busLane + event.value(), 0, Engine::BusCvCount - 1);
        }
        event.consume();
        return;
    }
    if (tabIsMidi(_tabEditorTab)) {          // MIDI tab
        bool shift = event.pressed() || globalKeyState()[Key::Shift];
        if (_matrixEditActive) {             // editing a route: dial the focused field
            auto &midi = _matrixDraft.route.midiSource();
            switch (_midiCol) {
            case 0: midi.source().editPort(event.value(), shift); break;
            case 1: midi.source().editChannel(event.value(), shift); break;
            case 2: midi.editEvent(event.value(), shift); break;
            default:
                if (midi.isControlEvent()) midi.editControlNumber(event.value(), shift);
                else midi.editNote(event.value(), shift);
                break;
            }
        } else {                             // nav: move the row cursor over learned routes
            uint8_t rows[CONFIG_ROUTE_COUNT];
            int n = RouteBrowse::midiRouteList(_project.routing(), rows, CONFIG_ROUTE_COUNT);
            _midiRow = clamp(_midiRow + event.value(), 0, n > 0 ? n - 1 : 0);
        }
        event.consume();
        return;
    }
    if (_matrixEditActive) {
        if (_matrixView == MatrixView::Source) {    // SOURCE view: dial the row's source
            matrixEditSource(event.value(), event.pressed() || globalKeyState()[Key::Shift]);
            event.consume();
            return;
        }
        if (_matrixView == MatrixView::Shaper) {    // SHAPER view: dial the row's shaper
            int idx = _matrixDraft.route.shaper(0) == Routing::Shaper::TriangleFold ? 1 : 0;
            idx = clamp(idx + (event.value() > 0 ? 1 : -1), 0, 1);
            Routing::Shaper s = idx ? Routing::Shaper::TriangleFold : Routing::Shaper::None;
            for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
                _matrixDraft.route.setShaper(t, s);
            }
            event.consume();
            return;
        }
        // DEPTH view: dial the cursor cell's draft depth
        const Track &track = _project.track(_tabCol);
        uint8_t paramKey = 0;
        {
            uint8_t keys[16];
            int n = tabParams(_tabEditorTab, keys, 16);
            paramKey = (n > 0) ? keys[clamp(_tabEditorRow, 0, n - 1)] : 0;
        }
        bool global = Routing::isProjectTarget(RouteBrowse::paramKeyToTarget(paramKey));
        if (tabCellEligible(track, paramKey)) {   // only eligible cells are editable
            int slot = global ? 0 : _tabCol;
            int newDepth = clamp(_matrixDraft.route.depthPct(slot) + event.value(), -100, 100);
            _matrixDraft.route.setDepthPct(slot, newDepth);
            if (!global) {   // membership is implicit: non-zero depth = member, 0 drops the track
                if (newDepth != 0) {
                    _matrixDraft.route.setTracks(_matrixDraft.route.tracks() | (1 << _tabCol));
                } else {
                    _matrixDraft.route.setTracks(_matrixDraft.route.tracks() & ~(1 << _tabCol));
                }
            }
        }
        event.consume();
        return;
    }
    int n = tabBandParamCount();         // nav: move the row cursor, re-point to that param's route
    _tabEditorRow = clamp(_tabEditorRow + event.value(), 0, n > 0 ? n - 1 : 0);
    const int visible = 4;               // keep the cursor inside the scroll window
    if (_tabEditorRow < _tabScroll) _tabScroll = _tabEditorRow;
    else if (_tabEditorRow >= _tabScroll + visible) _tabScroll = _tabEditorRow - visible + 1;
    tabRefocus();
    event.consume();
}

// Draw a row name clipped to maxW px (drop trailing chars until it fits the gutter).
static void drawClippedName(Canvas &canvas, int x, int y, int maxW, const char *name) {
    char buf[24];
    int i = 0;
    for (; i < 23 && name[i]; ++i) {
        buf[i] = name[i];
    }
    buf[i] = 0;
    while (i > 1 && canvas.textWidth(buf) > maxW) {
        buf[--i] = 0;
    }
    canvas.drawText(x, y, buf);
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

Track::TrackMode RoutingPage::tabEngineMode(int t) const {
    static const Track::TrackMode modes[kEngineCount] = {
        Track::TrackMode::Note, Track::TrackMode::PhaseFlux, Track::TrackMode::Curve,
        Track::TrackMode::Tuesday, Track::TrackMode::DiscreteMap, Track::TrackMode::Indexed,
        Track::TrackMode::Stochastic,
    };
    return modes[clamp(t - kBandCount, 0, kEngineCount - 1)];
}

// Row keys for tab t: engine table (minus shared) for engine tabs, band keys for bands.
int RoutingPage::tabParams(int t, uint8_t *keys, int maxKeys) const {
    if (tabIsEngine(t)) {
        return RouteBrowse::enginePageParams(tabEngineMode(t), keys, maxKeys);
    }
    if (tabIsBus(t) || tabIsMidi(t)) {
        return 0;
    }
    return RouteBrowse::bandParams(RouteBrowse::Band(t), keys, maxKeys);
}

const char *RoutingPage::tabName(int t) const {
    static const char *names[kTabCount] = {
        "PITCH", "CLOCK", "GLOB",
        "NOTE", "PHASEFLUX", "CURVE", "TUESDAY", "DISCMAP", "INDEXED", "STOCH",
        "BUS", "MIDI",
    };
    return names[clamp(t, 0, kTabCount - 1)];
}

// Row label: engine tab -> engine table name; band tab -> Global/Note table name.
const char *RoutingPage::tabParamName(int t, uint8_t key) const {
    const RouteParam::Table *tbl = nullptr;
    if (tabIsEngine(t)) {
        tbl = RouteFork::tableForMode(tabEngineMode(t));
    } else if (RouteBrowse::Band(t) == RouteBrowse::Band::Global) {
        tbl = &GlobalParamTable::table();
    } else {
        tbl = &NoteParamTable::table();
    }
    const char *shortName = RouteBrowse::shortLabel(key);
    if (shortName) {
        return shortName;
    }
    const RouteParam::Row *row = tbl ? tbl->find(key) : nullptr;
    return row ? row->name : "?";
}

int RoutingPage::tabBandParamCount() const {
    uint8_t keys[16];
    return tabParams(_tabEditorTab, keys, 16);
}

// Enter edit on the cursor row. Routed row -> begin() copies the live route into the
// draft; empty row -> create() reserves an inert slot (source None, depth 0). The live
// route is untouched until COMMIT; the staged draft is what the grid renders and edits.
void RoutingPage::matrixEnterEdit() {
    uint8_t keys[16];
    int n = tabParams(_tabEditorTab, keys, 16);
    if (n == 0) return;
    uint8_t key = keys[clamp(_tabEditorRow, 0, n - 1)];
    Routing::Target target = RouteBrowse::paramKeyToTarget(key);
    if (target == Routing::Target::None) return;

    auto &routing = _project.routing();
    if (_tabRowRouted && _route) {
        _matrixDraft = RouteDraft::begin(routing, _routeIndex);
    } else {
        _matrixDraft = RouteDraft::create(routing, target, _tabCol);
        if (_matrixDraft.routeIndex < 0) {
            showMessage("NO EMPTY ROUTES");
            return;
        }
    }
    _matrixEditActive = true;
    _matrixEditRow = _tabEditorRow;
    _matrixView = MatrixView::Source;        // edit opens on SOURCE, not DEPTH
}

// Group index of a source for Shift+encoder jumps: 0 None, 1 CvIn, 2 CvOut, 3 BusCv,
// 4 GateOut, 5 Mod. Matches the {None, CvIn, CvOut, BusCv, GateOut, Mod} group order.
static int sourceGroup(Routing::Source source) {
    if (source == Routing::Source::None) return 0;
    if (source >= Routing::Source::CvIn1 && source <= Routing::Source::CvIn4) return 1;
    if (source >= Routing::Source::CvOut1 && source <= Routing::Source::CvOut8) return 2;
    if (Routing::isBusSource(source)) return 3;
    if (source >= Routing::Source::GateOut1 && source <= Routing::Source::GateOut8) return 4;
    if (Routing::isModulatorSource(source)) return 5;
    return 0;
}

// SOURCE view: step the draft's single source through sourceList() order. delta>0 next,
// <0 prev, clamped at the ends. group=true jumps to the first source of the adjacent
// group (the next/prev group head), so Shift+encoder skips whole source families.
void RoutingPage::matrixEditSource(int delta, bool group) {
    if (delta == 0) return;
    Routing::Source list[int(Routing::Source::Last)];
    Routing::Target target = _matrixDraft.route.target();
    int n = RouteBrowse::sourceList(target, list, int(Routing::Source::Last));
    if (n == 0) return;
    Routing::Source cur = _matrixDraft.route.source();
    int idx = 0;
    for (int i = 0; i < n; ++i) {
        if (list[i] == cur) { idx = i; break; }
    }
    int next = idx;
    if (group) {
        int curGroup = sourceGroup(list[idx]);
        if (delta > 0) {
            for (int i = idx + 1; i < n; ++i) {
                if (sourceGroup(list[i]) != curGroup) { next = i; break; }
            }
        } else {
            int prevGroup = -1;
            for (int i = idx - 1; i >= 0; --i) {
                int g = sourceGroup(list[i]);
                if (g != curGroup) { prevGroup = g; }
                if (prevGroup >= 0 && (i == 0 || sourceGroup(list[i - 1]) != prevGroup)) { next = i; break; }
            }
        }
    } else {
        next = clamp(idx + (delta > 0 ? 1 : -1), 0, n - 1);
    }
    _matrixDraft.route.setSource(list[next]);
}

// Leave edit. commit=true writes the draft live (caller gates on canCommit); false
// reverts (frees a freshly-allocated slot). Either way return to nav and clear state.
void RoutingPage::matrixExitEdit(bool commit) {
    auto &routing = _project.routing();
    if (commit) {
        RouteDraft::commit(routing, _matrixDraft);
    } else {
        RouteDraft::cancel(routing, _matrixDraft);
    }
    _matrixEditActive = false;
    _matrixEditRow = -1;
    _matrixDraft = RouteDraft::Draft();
    tabRefocus();
}

// Point _route at the cursor row's committed route, if that param is routed at the
// scope. The tab editor edits _route live; unrouted rows leave _tabRowRouted false
// (cursor shows "+ADD", encoder-press creates).
void RoutingPage::tabRefocus() {
    uint8_t keys[16];
    int n = tabParams(_tabEditorTab, keys, 16);
    _tabEditorRow = clamp(_tabEditorRow, 0, (n > 0 ? n - 1 : 0));
    _tabCol = clamp(_tabCol, 0, CONFIG_TRACK_COUNT - 1);
    // A matrix row = one route for the param across its tracks. Resolve it track-
    // agnostically: per-track params match any track (0xFF), globals match scope 0.
    int routeIdx = -1;
    if (n > 0) {
        uint8_t paramKey = keys[_tabEditorRow];
        Routing::Target target = RouteBrowse::paramKeyToTarget(paramKey);
        uint8_t lookupMask = Routing::isPerTrackTarget(target) ? 0xFF : 0;
        routeIdx = routeForBandParam(paramKey, lookupMask);
    }
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
    _tabScroll = 0;
    _tabCol = clamp(_project.selectedTrackIndex(), 0, CONFIG_TRACK_COUNT - 1);
    _matrixEditActive = false;
    _matrixEditRow = -1;
    _midiRow = 0;
    _midiCol = 0;
    _matrixDraft = RouteDraft::Draft();
    tabRefocus();
}

// True when track t's engine owns paramKey (so a route there is meaningful). Global
// band params have no track dimension: eligibility is the same for every column.
static bool tabCellEligible(const Track &track, uint8_t paramKey) {
    Routing::Target target = RouteBrowse::paramKeyToTarget(paramKey);
    if (target == Routing::Target::None) return false;
    // Output/transport shell targets apply to any track regardless of engine mode — they
    // aren't migrated param-table params, so the migrated() gate would wrongly reject them.
    if (target == Routing::Target::Run || target == Routing::Target::Reset ||
        target == Routing::Target::CvOutputRotate || target == Routing::Target::GateOutputRotate) {
        return true;
    }
    uint8_t key; RouteParam::Range range;
    if (Routing::isProjectTarget(target)) {
        return RouteFork::migratedGlobal(target, key, range);
    }
    return RouteFork::migrated(track.trackMode(), target, key, range);
}

void RoutingPage::drawTabEditor(Canvas &canvas) {
    uint8_t keys[16];
    int n = tabParams(_tabEditorTab, keys, 16);
    int cursorRow = clamp(_tabEditorRow, 0, (n > 0 ? n - 1 : 0));
    int cursorCol = clamp(_tabCol, 0, CONFIG_TRACK_COUNT - 1);
    uint8_t cursorKey = (n > 0) ? keys[cursorRow] : 0;

    WindowPainter::clear(canvas);
    canvas.setFont(Font::Tiny);

    // header: tab name bright, view tag dim (centered, clear of long engine names),
    // combine state (F3) far right
    canvas.setColor(Color::Bright);
    canvas.drawText(2, 7, tabName(_tabEditorTab));
    canvas.setColor(Color::Medium);
    const char *viewTag = _matrixView == MatrixView::Source ? "SOURCE"
                        : _matrixView == MatrixView::Shaper ? "SHAPER" : "DEPTH";
    canvas.drawText((CONFIG_LCD_WIDTH - canvas.textWidth(viewTag)) / 2, 7, viewTag);
    // occupied-slot counter (always) far-right; combine indicator sits to its left
    int activeRoutes = 0;
    for (int r = 0; r < CONFIG_ROUTE_COUNT; ++r) {
        if (_project.routing().route(r).active()) {
            ++activeRoutes;
        }
    }
    FixedStringBuilder<8> slots("%d/%d", activeRoutes, CONFIG_ROUTE_COUNT);
    int slotsX = CONFIG_LCD_WIDTH - canvas.textWidth(slots) - 2;
    canvas.setColor(activeRoutes >= CONFIG_ROUTE_COUNT ? Color::Bright : Color::Medium);
    canvas.drawText(slotsX, 7, slots);
    {
        RouteApply::Combine cb = RouteApply::Combine::Modulate;
        bool show = false;
        if (_matrixEditActive) { cb = _matrixDraft.route.combine(); show = true; }
        else if (_tabRowRouted && _route) { cb = _route->combine(); show = true; }
        if (show) {
            const char *cbStr = cb == RouteApply::Combine::Absolute ? "ABSOLUTE" : "MODULATE";
            canvas.setColor(_matrixEditActive ? Color::Bright : Color::Medium);
            canvas.drawText(slotsX - 6 - canvas.textWidth(cbStr), 7, cbStr);
        }
    }
    canvas.setColor(Color::Low);
    canvas.hline(0, 10, CONFIG_LCD_WIDTH);

    // grid geometry: left label gutter (wider on engine tabs for longer names) + 8
    // track columns; a 4px scrollbar column on the right when the page overflows
    const int top = 20, rowH = 8, visible = 4;
    int scroll = clamp(_tabScroll, 0, n > visible ? n - visible : 0);
    const int barW = (n > visible) ? 4 : 0;
    const int nameW = tabIsEngine(_tabEditorTab) ? 52 : 38;
    const int gridL = nameW + 2;
    const int colW = (CONFIG_LCD_WIDTH - gridL - 1 - barW) / 8;

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

    // param rows (scroll window): name in the gutter (cursor row bright), then 8 cells.
    //   '-' ineligible, '.' eligible+unrouted, depth number when routed. The active
    //   draft row reads staged depths from _matrixDraft so edits show before commit.
    for (int vi = 0; vi < visible; ++vi) {
        int r = scroll + vi;
        if (r >= n) break;
        int y = top + vi * rowH;
        uint8_t key = keys[r];
        bool cursorRowSel = r == cursorRow;
        bool draftRow = _matrixEditActive && r == _matrixEditRow;
        bool globalKey = Routing::isProjectTarget(RouteBrowse::paramKeyToTarget(key));
        canvas.setColor(cursorRowSel ? Color::Bright : Color::Medium);
        drawClippedName(canvas, 2, y + 6, nameW - 2, tabParamName(_tabEditorTab, key));
        // SOURCE view paints the row's single source on EVERY eligible cell (one source per row);
        // whether a track is actually applied is shown by DEPTH view. Resolve the row source once.
        Routing::Source rowSource = Routing::Source::None;
        RouteApply::Combine rowCombine = RouteApply::Combine::Modulate;
        Routing::Shaper rowShaper = Routing::Shaper::None;
        bool rowHasRoute = false;
        if (draftRow) {
            rowSource = _matrixDraft.route.source();
            rowCombine = _matrixDraft.route.combine();
            rowShaper = _matrixDraft.route.shaper(0);
            rowHasRoute = true;
        } else {
            int rowRouteIdx = routeForBandParam(key, globalKey ? 0 : 0xFF);
            if (rowRouteIdx >= 0) {
                const auto &rr = _project.routing().route(rowRouteIdx);
                rowSource = rr.source();
                rowCombine = rr.combine();
                rowShaper = rr.shaper(0);
                rowHasRoute = true;
            }
        }
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
            const Track &track = _project.track(t);
            int cx = gridL + t * colW + colW / 2;
            bool isCursor = cursorRowSel && t == cursorCol;
            if (!tabCellEligible(track, key)) {
                canvas.setColor(Color::Low);
                canvas.drawText(cx - 2, y + 6, "-");
                continue;
            }
            if (_matrixView == MatrixView::Source) {
                FixedStringBuilder<6> txt;
                if (rowHasRoute) {
                    Routing::printSourceAbbrev(rowSource, txt);
                } else {
                    txt(".");
                }
                canvas.setColor(isCursor ? Color::Bright : (rowHasRoute ? Color::Medium : Color::MediumLow));
                canvas.drawText(cx - canvas.textWidth(txt) / 2, y + 6, txt);
                continue;
            }
            if (_matrixView == MatrixView::Shaper) {   // row's shaper on every eligible cell
                FixedStringBuilder<6> txt;
                if (rowHasRoute) {
                    txt(rowShaper == Routing::Shaper::TriangleFold ? "FLD" : "-");
                } else {
                    txt(".");
                }
                canvas.setColor(isCursor ? Color::Bright : (rowHasRoute ? Color::Medium : Color::MediumLow));
                canvas.drawText(cx - canvas.textWidth(txt) / 2, y + 6, txt);
                continue;
            }
            // DEPTH view: per-track membership/depth.
            bool routed; int depth;
            if (draftRow) {
                int slot = globalKey ? 0 : t;
                routed = globalKey || (_matrixDraft.route.tracks() & (1 << t));
                depth = _matrixDraft.route.depthPct(slot);
            } else {
                uint8_t scopeMask = globalKey ? 0 : uint8_t(1 << t);
                int routeIdx = routeForBandParam(key, scopeMask);
                routed = routeIdx >= 0;
                depth = routed ? _project.routing().route(routeIdx).depthPct(t) : 0;
            }
            FixedStringBuilder<6> txt;
            if (routed) {
                txt("%+d", depth);
            } else {
                txt(".");
            }
            canvas.setColor(isCursor ? Color::Bright : (routed ? Color::Medium : Color::MediumLow));
            canvas.drawText(cx - canvas.textWidth(txt) / 2, y + 6, txt);
        }
        // per-row combine marker (M/A) in the right margin, routed rows only
        if (rowHasRoute) {
            canvas.setColor(cursorRowSel ? Color::Bright : Color::Medium);
            canvas.drawText(CONFIG_LCD_WIDTH - 6, y + 6, rowCombine == RouteApply::Combine::Absolute ? "A" : "M");
        }
    }

    // by-type grouping cue: under the cursor row, underline cells whose track shares
    // the cursor column's engine (the group a Shift+T on this column would toggle).
    // Per-track bands only; global rows have no per-track membership.
    bool cursorVisible = n > 0 && cursorRow >= scroll && cursorRow < scroll + visible;
    if (cursorVisible && !Routing::isProjectTarget(RouteBrowse::paramKeyToTarget(cursorKey))) {
        Track::TrackMode cursorMode = _project.track(cursorCol).trackMode();
        int uy = top + (cursorRow - scroll) * rowH + 7;
        canvas.setColor(Color::Low);
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
            if (_project.track(t).trackMode() != cursorMode) continue;
            if (!tabCellEligible(_project.track(t), cursorKey)) continue;
            int cx = gridL + t * colW + colW / 2;
            canvas.hline(cx - colW / 2 + 1, uy, colW - 2);
        }
    }

    // right-edge scrollbar when the page overflows the visible window
    if (n > visible) {
        int trackY = 19, trackH = top + visible * rowH - 19;
        canvas.setColor(Color::Low);
        canvas.vline(CONFIG_LCD_WIDTH - 2, trackY, trackH);
        int thumbH = std::max(4, trackH * visible / n);
        int thumbY = trackY + trackH * scroll / n;
        canvas.setColor(Color::Bright);
        canvas.fillRect(CONFIG_LCD_WIDTH - 3, thumbY, 3, thumbH);
    }

    bool editing = _matrixEditActive;
    bool canCommit = editing && RouteDraft::canCommit(_matrixDraft);
    const char *combineLbl = (editing && _matrixDraft.route.combine() == RouteApply::Combine::Absolute) ? "ABS" : "MOD";
    const char *fn[] = { "VIEW", "EDIT", editing ? combineLbl : nullptr, editing ? "CANCEL" : nullptr, canCommit ? "COMMIT" : nullptr };
    WindowPainter::drawFooter(canvas, fn, pageKeyState(), editing ? int(Function::Edit) : -1);
}

void RoutingPage::handleTabEditorKey(KeyPressEvent &event) {
    const auto &key = event.key();

    // T1-T8 move the cursor to that track's column (nav + edit). Shift+Tn = by-type spread:
    // copy the cursor cell's depth to all eligible tracks sharing track n's engine.
    // Membership is implicit (encoder depth: non-zero = member, 0 drops). Steps are unused here.
    if (key.isTrack()) {
        int tn = key.track();
        if (key.shiftModifier() && _matrixEditActive) {
            uint8_t keys[16];
            int en = tabParams(_tabEditorTab, keys, 16);
            uint8_t editKey = (en > 0) ? keys[clamp(_matrixEditRow, 0, en - 1)] : 0;
            if (!Routing::isProjectTarget(RouteBrowse::paramKeyToTarget(editKey))) {
                int srcDepth = _matrixDraft.route.depthPct(_tabCol);
                Track::TrackMode groupMode = _project.track(tn).trackMode();
                for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
                    if (_project.track(t).trackMode() != groupMode) continue;
                    if (!tabCellEligible(_project.track(t), editKey)) continue;
                    _matrixDraft.route.setDepthPct(t, srcDepth);
                    if (srcDepth != 0) {
                        _matrixDraft.route.setTracks(_matrixDraft.route.tracks() | (1 << t));
                    } else {
                        _matrixDraft.route.setTracks(_matrixDraft.route.tracks() & ~(1 << t));
                    }
                }
            }
        } else {
            _tabCol = tn;
        }
        event.consume();
        return;
    }

    if (key.isLeft() || key.isRight()) {
        // leaving the row discards an uncommitted draft, then switches band
        if (_matrixEditActive) {
            matrixExitEdit(false);
        }
        _tabEditorTab = (_tabEditorTab + (key.isLeft() ? kTabCount - 1 : 1)) % kTabCount;
        _tabScroll = 0;
        _tabEditorRow = 0;
        tabRefocus();
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Edit: // F2 toggles nav <-> edit on the cursor row
            if (_matrixEditActive) {
                matrixExitEdit(false);
            } else {
                matrixEnterEdit();
            }
            break;
        case Function::View: // cycle the cell view: source -> depth -> shaper
            _matrixView = _matrixView == MatrixView::Source ? MatrixView::Depth
                        : _matrixView == MatrixView::Depth ? MatrixView::Shaper
                        : MatrixView::Source;
            break;
        case Function::Combine: // toggle Modulate <-> Absolute on the draft
            if (_matrixEditActive) {
                _matrixDraft.route.setCombine(
                    _matrixDraft.route.combine() == RouteApply::Combine::Absolute
                        ? RouteApply::Combine::Modulate : RouteApply::Combine::Absolute);
            }
            break;
        case Function::Cancel: // plain: discard the draft. Shift+CANCEL: REMOVE the
            // focused row's committed route entirely (only when routed).
            if (key.shiftModifier()) {
                if (_matrixEditActive) {
                    matrixExitEdit(false);
                }
                if (_tabRowRouted && _route) {
                    _project.routing().route(_routeIndex).clear();
                    tabRefocus();
                }
            } else if (_matrixEditActive) {
                matrixExitEdit(false);
            }
            break;
        case Function::Commit: // write the draft live (gated on canCommit)
            if (_matrixEditActive && RouteDraft::canCommit(_matrixDraft)) {
                matrixExitEdit(true);
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

// Bipolar -5..+5V bar: baseline + centre tick, filled span proportional to |volts|.
static void drawBusVoltBar(Canvas &canvas, int x, int y, int w, float volts, bool focus) {
    int cx = x + w / 2;
    canvas.setColor(Color::Low);
    canvas.hline(x, y + 1, w);
    canvas.vline(cx, y - 1, 5);
    int span = int(std::min(std::abs(volts), 5.f) / 5.f * (w / 2));
    canvas.setColor(focus ? Color::Bright : Color::Medium);
    if (span > 0) {
        if (volts >= 0.f) {
            canvas.fillRect(cx, y, span, 3);
        } else {
            canvas.fillRect(cx - span, y, span, 3);
        }
    }
}

void RoutingPage::drawBus(Canvas &canvas) {
    const int DIV = 104;     // level-block divider x
    const int VAL_R = 98;    // right edge of the right-aligned value column
    const int SRC_X = 110;   // group 1: routing source abbrev column
    const int DPT_X = 130;   // group 1: routing depth column
    const int SEP_X = 172;   // group separator
    const int CVR_X = 178;   // group 2: CV-router activity light
    const int TT_X = 210;    // group 2: Teletype activity light

    bool editing = _matrixEditActive;
    auto &routing = _project.routing();

    WindowPainter::clear(canvas);
    canvas.setFont(Font::Tiny);

    // header: title, column labels (or SOURCE/DEPTH view tag while editing), SAFE state
    canvas.setColor(Color::Bright);
    canvas.drawText(3, 7, "BUS");
    canvas.setColor(Color::Low);
    if (editing) {
        canvas.setColor(Color::Medium);
        canvas.drawText(40, 7, _matrixView == MatrixView::Source ? "SOURCE" : "DEPTH");
    } else {
        canvas.drawText(SRC_X, 7, "SRC");
        canvas.drawText(DPT_X, 7, "DEPTH");
        canvas.drawText(CVR_X, 7, "LIVE");
    }
    if (_project.busSafety()) {
        canvas.setColor(Color::Bright);
        canvas.drawText(VAL_R - canvas.textWidth("SAFE"), 7, "SAFE");
    }
    canvas.setColor(Color::Low);          // structural lines are always dim
    canvas.hline(0, 10, CONFIG_LCD_WIDTH);
    canvas.vline(DIV, 11, CONFIG_LCD_HEIGHT - 22);
    canvas.vline(SEP_X, 11, CONFIG_LCD_HEIGHT - 22);

    const int top = 14, rowH = 10;
    for (int lane = 0; lane < Engine::BusCvCount; ++lane) {
        int y = top + lane * rowH;
        bool focus = lane == _busLane;
        bool editThis = editing && lane == _busLane;
        if (focus) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(0, y - 1, 2, rowH - 1);
        }
        // fixed LEVEL block: lane label, bipolar bar, right-aligned value
        float volts = _engine.busCv(lane);
        canvas.setColor(focus ? Color::Bright : Color::Medium);
        FixedStringBuilder<4> label("B%d", lane + 1);
        canvas.drawText(6, y + 4, label);
        drawBusVoltBar(canvas, 22, y + 1, 40, volts, focus);
        FixedStringBuilder<8> vtxt(volts != 0.f ? "%+.1fV" : "0.0V", volts);
        canvas.drawText(VAL_R - canvas.textWidth(vtxt), y + 4, vtxt);

        // group 1: the one routing source set here + its depth (draft while editing,
        // else the committed route; "+" hint on an empty lane)
        Routing::Target laneTarget = Routing::Target(int(Routing::Target::BusCv1) + lane);
        Routing::Source src = Routing::Source::None;
        int depth = 0;
        bool routed = false;
        if (editThis) {
            src = _matrixDraft.route.source();
            depth = _matrixDraft.route.depthPct(0);
            routed = true;
        } else {
            int idx = RouteDraft::findRouteForTarget(routing, laneTarget);
            if (idx >= 0) {
                src = routing.route(idx).source();
                depth = routing.route(idx).depthPct(0);
                routed = true;
            }
        }
        if (routed && src != Routing::Source::None) {
            bool srcHot = editThis && _matrixView == MatrixView::Source;
            bool dptHot = editThis && _matrixView == MatrixView::Depth;
            FixedStringBuilder<8> tok;
            Routing::printSourceAbbrev(src, tok);
            canvas.setColor(srcHot ? Color::Bright : Color::Medium);
            canvas.drawText(SRC_X, y + 4, tok);
            FixedStringBuilder<8> dtxt("%+d%%", depth);
            canvas.setColor(dptHot ? Color::Bright : Color::Medium);
            canvas.drawText(DPT_X, y + 4, dtxt);
        } else if (editThis) {
            canvas.setColor(Color::Bright);                 // fresh draft, no source picked yet
            canvas.drawText(SRC_X, y + 4, "?");
        } else {
            canvas.setColor(Color::Low);                    // empty lane: EDIT here adds a source
            canvas.drawText(SRC_X, y + 4, "+");
        }

        // group 2: CVR / TT activity lights (set up elsewhere, lit when writing this frame)
        uint8_t writers = _engine.busWriters(lane);
        canvas.setColor((writers & Engine::BusWriterCvRouter) ? Color::Bright : Color::Low);
        canvas.drawText(CVR_X, y + 4, "CVR");
        canvas.setColor((writers & Engine::BusWriterTeletype) ? Color::Bright : Color::Low);
        canvas.drawText(TT_X, y + 4, "TT");
    }

    bool canCommit = editing && RouteDraft::canCommit(_matrixDraft);
    const char *combineLbl = (editing && _matrixDraft.route.combine() == RouteApply::Combine::Absolute) ? "ABS" : "MOD";
    const char *fn[] = { "VIEW", "EDIT", editing ? combineLbl : nullptr, editing ? "CANCEL" : nullptr, canCommit ? "COMMIT" : nullptr };
    WindowPainter::drawFooter(canvas, fn, pageKeyState(), editing ? int(Function::Edit) : -1);
}

// Begin editing the focused lane: re-open its committed routing route, or create a
// fresh inert draft (one routing source per lane; bus target is global, scope 0).
void RoutingPage::busEnterEdit() {
    auto &routing = _project.routing();
    Routing::Target target = Routing::Target(int(Routing::Target::BusCv1) + _busLane);
    int idx = RouteDraft::findRouteForTarget(routing, target);
    if (idx >= 0) {
        _matrixDraft = RouteDraft::begin(routing, idx);
    } else {
        _matrixDraft = RouteDraft::create(routing, target, 0);
        if (_matrixDraft.routeIndex < 0) {
            showMessage("NO EMPTY ROUTES");
            return;
        }
    }
    _matrixEditActive = true;
    _matrixEditRow = _busLane;
}

void RoutingPage::handleBusKey(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isLeft() || key.isRight()) {     // band cycle: discard draft, leave the bus tab
        if (_matrixEditActive) {
            matrixExitEdit(false);
        }
        _tabEditorTab = (_tabEditorTab + (key.isLeft() ? kTabCount - 1 : 1)) % kTabCount;
        _tabScroll = 0;
        _tabEditorRow = 0;
        tabRefocus();
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::View: // F1: cycle the lane view depth <-> source
            _matrixView = _matrixView == MatrixView::Depth ? MatrixView::Source : MatrixView::Depth;
            break;
        case Function::Edit: // F2: nav <-> edit on the focused lane
            if (_matrixEditActive) {
                matrixExitEdit(false);
            } else {
                busEnterEdit();
            }
            break;
        case Function::Combine: // F3: Shift = SAFE toggle; plain = MOD/ABS while editing
            if (key.shiftModifier()) {
                _project.setBusSafety(!_project.busSafety());
            } else if (_matrixEditActive) {
                _matrixDraft.route.setCombine(
                    _matrixDraft.route.combine() == RouteApply::Combine::Absolute
                        ? RouteApply::Combine::Modulate : RouteApply::Combine::Absolute);
            }
            break;
        case Function::Cancel: // F4: Shift = REMOVE the lane's route; plain = discard draft
            if (key.shiftModifier()) {
                if (_matrixEditActive) {
                    matrixExitEdit(false);
                }
                Routing::Target target = Routing::Target(int(Routing::Target::BusCv1) + _busLane);
                int idx = RouteDraft::findRouteForTarget(_project.routing(), target);
                if (idx >= 0) {
                    _project.routing().route(idx).clear();
                }
            } else if (_matrixEditActive) {
                matrixExitEdit(false);
            }
            break;
        case Function::Commit: // F5: write the draft live (gated on canCommit)
            if (_matrixEditActive && RouteDraft::canCommit(_matrixDraft)) {
                matrixExitEdit(true);
            }
            break;
        default:
            break;
        }
        event.consume();
        return;
    }
    event.consume(); // modal: swallow other keys so they don't reach TopPage
}

// Compact event token for the narrow EVENT column (full eventName() won't fit).
static const char *midiEventAbbrev(Routing::MidiSource::Event e) {
    using Event = Routing::MidiSource::Event;
    switch (e) {
    case Event::ControlAbsolute: return "CCa";
    case Event::ControlRelative: return "CCr";
    case Event::PitchBend:       return "PB";
    case Event::NoteMomentary:   return "NtM";
    case Event::NoteToggle:      return "NtT";
    case Event::NoteVelocity:    return "NtV";
    case Event::NoteRange:       return "NtR";
    default:                     return "?";
    }
}

// MIDI tab: every route whose source is MIDI, one row each (legacy detail fields:
// target it drives | Port | Channel | Event | CC/Note). EDIT opens the focused route's
// draft; VIEW walks the columns; encoder dials the focused field; COMMIT persists.
void RoutingPage::drawMidi(Canvas &canvas) {
    const int DIV = 76, TGT_X = 4, PORT_X = 82, CH_X = 110, EVT_X = 132, NUM_X = 170;
    auto &routing = _project.routing();
    uint8_t rows[CONFIG_ROUTE_COUNT];
    int n = RouteBrowse::midiRouteList(routing, rows, CONFIG_ROUTE_COUNT);
    bool editing = _matrixEditActive;
    _midiRow = clamp(_midiRow, 0, n > 0 ? n - 1 : 0);

    WindowPainter::clear(canvas);
    canvas.setFont(Font::Tiny);

    // header: title + column labels, or the active column tag while editing
    canvas.setColor(Color::Bright);
    canvas.drawText(3, 7, "MIDI");
    if (editing) {
        static const char *colTag[4] = { "PORT", "CHANNEL", "EVENT", "CC/NOTE" };
        canvas.setColor(Color::Medium);
        canvas.drawText(PORT_X, 7, colTag[clamp(_midiCol, 0, 3)]);
    } else {
        canvas.setColor(Color::Low);
        canvas.drawText(PORT_X, 7, "PORT");
        canvas.drawText(CH_X, 7, "CH");
        canvas.drawText(EVT_X, 7, "EVENT");
        canvas.drawText(NUM_X, 7, "CC/NOTE");
    }
    canvas.setColor(Color::Low);
    canvas.hline(0, 10, CONFIG_LCD_WIDTH);
    canvas.vline(DIV, 11, CONFIG_LCD_HEIGHT - 22);

    if (n == 0) {
        canvas.setColor(Color::Low);
        const char *msg = "no MIDI assignments - learn from the matrix";
        canvas.drawText((CONFIG_LCD_WIDTH - canvas.textWidth(msg)) / 2, 34, msg);
    }

    const int top = 14, rowH = 10, visible = 4;
    int firstRow = (n > visible) ? clamp(_midiRow - visible + 1, 0, n - visible) : 0;
    for (int vi = 0; vi < visible; ++vi) {
        int ri = firstRow + vi;
        if (ri >= n) break;
        int y = top + vi * rowH;
        bool editThis = editing && ri == _matrixEditRow;
        bool focus = editThis || (!editing && ri == _midiRow);
        const Routing::Route &route = routing.route(rows[ri]);
        const Routing::MidiSource &ms = editThis ? _matrixDraft.route.midiSource() : route.midiSource();
        Routing::Target tgt = editThis ? _matrixDraft.route.target() : route.target();
        uint8_t tracks = editThis ? _matrixDraft.route.tracks() : route.tracks();

        if (focus) {
            canvas.setColor(Color::Bright);
            canvas.fillRect(0, y - 1, 2, rowH - 1);
        }
        // target it drives: name + first track for per-track targets, clipped to gutter
        FixedStringBuilder<20> tlabel("%s", Routing::targetName(tgt));
        if (Routing::isPerTrackTarget(tgt)) {
            for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
                if (tracks & (1 << t)) { tlabel(" %d", t + 1); break; }
            }
        }
        canvas.setColor(focus ? Color::Bright : Color::Medium);
        drawClippedName(canvas, TGT_X, y + 4, DIV - TGT_X - 2, tlabel);

        FixedStringBuilder<8> portTxt; ms.source().printPort(portTxt);
        FixedStringBuilder<8> chTxt; ms.source().printChannel(chTxt);
        const char *evtTxt = midiEventAbbrev(ms.event());
        FixedStringBuilder<8> numTxt;
        bool hasNum = ms.event() != Routing::MidiSource::Event::PitchBend;
        if (hasNum) {
            if (ms.isControlEvent()) ms.printControlNumber(numTxt); else ms.printNote(numTxt);
        }
        auto fieldColor = [&](int col) {
            return (editThis && col == _midiCol) ? Color::Bright : Color::Medium;
        };
        canvas.setColor(fieldColor(0)); canvas.drawText(PORT_X, y + 4, portTxt);
        canvas.setColor(fieldColor(1)); canvas.drawText(CH_X, y + 4, chTxt);
        canvas.setColor(fieldColor(2)); canvas.drawText(EVT_X, y + 4, evtTxt);
        if (hasNum) {
            canvas.setColor(fieldColor(3)); canvas.drawText(NUM_X, y + 4, numTxt);
        } else {
            canvas.setColor(Color::Low); canvas.drawText(NUM_X, y + 4, "-");
        }
    }

    if (n > visible) {                       // right-edge scrollbar
        int trackH = CONFIG_LCD_HEIGHT - 22;
        int thumbH = std::max(4, trackH * visible / n);
        int thumbY = 11 + (trackH - thumbH) * firstRow / std::max(1, n - visible);
        canvas.setColor(Color::Low);
        canvas.vline(CONFIG_LCD_WIDTH - 2, 11, trackH);
        canvas.setColor(Color::Medium);
        canvas.fillRect(CONFIG_LCD_WIDTH - 3, thumbY, 2, thumbH);
    }

    bool learning = _engine.midiLearn().isActive();
    if (learning) {                          // armed: capture popup over the page
        const int bw = 120, bh = 16;
        int bx = (CONFIG_LCD_WIDTH - bw) / 2;
        int by = (CONFIG_LCD_HEIGHT - 11 - bh) / 2;
        canvas.setColor(Color::None);
        canvas.fillRect(bx, by, bw, bh);
        canvas.setColor(Color::Bright);
        canvas.drawRect(bx, by, bw, bh);
        const char *m = "waiting for MIDI...";
        canvas.drawText(bx + (bw - canvas.textWidth(m)) / 2, by + 11, m);
    }

    bool canCommit = editing && RouteDraft::canCommit(_matrixDraft);
    const char *fn[] = { "VIEW", "EDIT", editing ? "LEARN" : nullptr, editing ? "CANCEL" : nullptr, canCommit ? "COMMIT" : nullptr };
    WindowPainter::drawFooter(canvas, fn, pageKeyState(), learning ? int(Function::Combine) : (editing ? int(Function::Edit) : -1));
}

// Begin editing the focused MIDI route: copy the live route into the draft (the route
// already exists — created in the grid with source MIDI). Live route untouched until COMMIT.
void RoutingPage::midiEnterEdit() {
    uint8_t rows[CONFIG_ROUTE_COUNT];
    int n = RouteBrowse::midiRouteList(_project.routing(), rows, CONFIG_ROUTE_COUNT);
    if (n == 0) return;
    int ri = clamp(_midiRow, 0, n - 1);
    _matrixDraft = RouteDraft::begin(_project.routing(), rows[ri]);
    _matrixEditActive = true;
    _matrixEditRow = ri;
    _midiCol = 0;
}

void RoutingPage::handleMidiKey(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isLeft() || key.isRight()) {     // tab cycle: discard draft, leave the MIDI tab
        _engine.midiLearn().stop();
        if (_matrixEditActive) {
            matrixExitEdit(false);
        }
        _tabEditorTab = (_tabEditorTab + (key.isLeft() ? kTabCount - 1 : 1)) % kTabCount;
        _tabScroll = 0;
        _tabEditorRow = 0;
        tabRefocus();
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::View: // F1: advance the edited column (PitchBend skips CC/Note)
            if (_matrixEditActive) {
                int cols = _matrixDraft.route.midiSource().event() == Routing::MidiSource::Event::PitchBend ? 3 : 4;
                _midiCol = (_midiCol + 1) % cols;
            }
            break;
        case Function::Combine: // F3: arm/disarm LEARN, capturing into the draft
            if (_matrixEditActive) {
                if (_engine.midiLearn().isActive()) {
                    _engine.midiLearn().stop();
                } else {
                    _engine.midiLearn().start([this] (const MidiLearn::Result &result) {
                        assignMidiLearn(result, _matrixDraft.route.midiSource());
                        _engine.midiLearn().stop();
                    });
                }
            }
            break;
        case Function::Edit: // F2: nav <-> edit on the focused route
            _engine.midiLearn().stop();
            if (_matrixEditActive) {
                matrixExitEdit(false);
            } else {
                midiEnterEdit();
            }
            break;
        case Function::Cancel: // F4: Shift = REMOVE the route; plain = discard draft
            _engine.midiLearn().stop();
            if (key.shiftModifier()) {
                if (_matrixEditActive) {
                    matrixExitEdit(false);
                }
                uint8_t rows[CONFIG_ROUTE_COUNT];
                int n = RouteBrowse::midiRouteList(_project.routing(), rows, CONFIG_ROUTE_COUNT);
                if (n > 0) {
                    _project.routing().route(rows[clamp(_midiRow, 0, n - 1)]).clear();
                }
            } else if (_matrixEditActive) {
                matrixExitEdit(false);
            }
            break;
        case Function::Commit: // F5: write the draft live (gated on canCommit)
            _engine.midiLearn().stop();
            if (_matrixEditActive && RouteDraft::canCommit(_matrixDraft)) {
                matrixExitEdit(true);
            }
            break;
        default:
            break;
        }
        event.consume();
        return;
    }
    event.consume(); // modal: swallow other keys so they don't reach TopPage
}

void RoutingPage::keyboard(KeyboardEvent &event) {
    (void)event; // tab editor owns no keyboard editing
}
