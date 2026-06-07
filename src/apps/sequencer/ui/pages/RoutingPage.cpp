#include "RoutingPage.h"

#include "ui/painters/WindowPainter.h"

#include "model/RouteBrowse.h"
#include "model/RouteDraft.h"
#include "model/RouteFork.h"
#include "model/ParamTableGlobal.h"
#include "model/ParamTableNote.h"

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
    if (_tabEditorTab == 4) {
        drawBus(canvas);
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
    if (_tabEditorTab == 4) {
        handleBusKey(event);
    } else {
        handleTabEditorKey(event);
    }
}

void RoutingPage::encoder(EncoderEvent &event) {
    if (_tabEditorTab == 4) {           // bus tab: move the lane cursor 0..3
        _busLane = clamp(_busLane + event.value(), 0, Engine::BusCvCount - 1);
        event.consume();
        return;
    }
    if (_matrixEditActive) {
        if (_matrixView == MatrixView::Source) {    // SOURCE view: dial the row's source
            matrixEditSource(event.value(), event.pressed() || globalKeyState()[Key::Shift]);
            event.consume();
            return;
        }
        // DEPTH view: dial the cursor cell's draft depth
        auto band = RouteBrowse::Band(_tabEditorTab);
        bool global = band == RouteBrowse::Band::Global;
        const Track &track = _project.track(_tabCol);
        uint8_t paramKey = 0;
        {
            uint8_t keys[8];
            int n = RouteBrowse::bandParams(band, keys, 8);
            paramKey = (n > 0) ? keys[clamp(_tabEditorRow, 0, n - 1)] : 0;
        }
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

int RoutingPage::tabBandParamCount() const {
    uint8_t keys[8];
    return RouteBrowse::bandParams(RouteBrowse::Band(_tabEditorTab), keys, 8);
}

// Enter edit on the cursor row. Routed row -> begin() copies the live route into the
// draft; empty row -> create() reserves an inert slot (source None, depth 0). The live
// route is untouched until COMMIT; the staged draft is what the grid renders and edits.
void RoutingPage::matrixEnterEdit() {
    uint8_t keys[8];
    auto band = RouteBrowse::Band(_tabEditorTab);
    int n = RouteBrowse::bandParams(band, keys, 8);
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
    uint8_t keys[8];
    int n = RouteBrowse::bandParams(RouteBrowse::Band(_tabEditorTab), keys, 8);
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
    _tabCol = clamp(_project.selectedTrackIndex(), 0, CONFIG_TRACK_COUNT - 1);
    _matrixEditActive = false;
    _matrixEditRow = -1;
    _matrixDraft = RouteDraft::Draft();
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

    // header: band name bright, view tag dim, combine state (F3) far right
    canvas.setColor(Color::Bright);
    canvas.drawText(2, 7, tabs[_tabEditorTab]);
    canvas.setColor(Color::Medium);
    canvas.drawText(40, 7, _matrixView == MatrixView::Source ? "SOURCE" : "DEPTH");
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
    //   '-' ineligible, '.' eligible+unrouted, depth number when routed. The active
    //   draft row reads staged depths from _matrixDraft so edits show before commit.
    const int top = 20, rowH = 8;
    for (int r = 0; r < n; ++r) {
        int y = top + r * rowH;
        uint8_t key = keys[r];
        bool cursorRowSel = r == cursorRow;
        bool draftRow = _matrixEditActive && r == _matrixEditRow;
        bool globalKey = Routing::isProjectTarget(RouteBrowse::paramKeyToTarget(key));
        canvas.setColor(cursorRowSel ? Color::Bright : Color::Medium);
        drawClippedName(canvas, 2, y + 6, nameW - 2, bandParamName(band, key));
        // SOURCE view paints the row's single source on EVERY eligible cell (one source per row);
        // whether a track is actually applied is shown by DEPTH view. Resolve the row source once.
        Routing::Source rowSource = Routing::Source::None;
        RouteApply::Combine rowCombine = RouteApply::Combine::Modulate;
        bool rowHasRoute = false;
        if (draftRow) {
            rowSource = _matrixDraft.route.source();
            rowCombine = _matrixDraft.route.combine();
            rowHasRoute = true;
        } else {
            int rowRouteIdx = routeForBandParam(key, globalKey ? 0 : 0xFF);
            if (rowRouteIdx >= 0) {
                const auto &rr = _project.routing().route(rowRouteIdx);
                rowSource = rr.source();
                rowCombine = rr.combine();
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
    if (n > 0 && !Routing::isProjectTarget(RouteBrowse::paramKeyToTarget(cursorKey))) {
        Track::TrackMode cursorMode = _project.track(cursorCol).trackMode();
        int uy = top + cursorRow * rowH + 7;
        canvas.setColor(Color::Low);
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
            if (_project.track(t).trackMode() != cursorMode) continue;
            if (!tabCellEligible(_project.track(t), cursorKey)) continue;
            int cx = gridL + t * colW + colW / 2;
            canvas.hline(cx - colW / 2 + 1, uy, colW - 2);
        }
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
            uint8_t keys[8];
            auto editBand = RouteBrowse::Band(_tabEditorTab);
            int en = RouteBrowse::bandParams(editBand, keys, 8);
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
        _tabEditorTab = (_tabEditorTab + (key.isLeft() ? 4 : 1)) % 5;
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
        case Function::View: // cycle the cell view: depth <-> source
            _matrixView = _matrixView == MatrixView::Depth ? MatrixView::Source : MatrixView::Depth;
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
    const int DIV = 104;     // vertical divider x
    const int VAL_R = 98;    // right edge of the right-aligned value column

    WindowPainter::clear(canvas);
    canvas.setFont(Font::Tiny);

    // header: band name + zone labels, divider between LEVEL block and CONNECTIONS zone
    canvas.setColor(Color::Bright);
    canvas.drawText(3, 7, "BUS");
    canvas.setColor(Color::Low);
    canvas.drawText(DIV + 4, 7, "<SOURCES");
    canvas.drawText(CONFIG_LCD_WIDTH - canvas.textWidth("READERS>") - 3, 7, "READERS>");
    canvas.hline(0, 10, CONFIG_LCD_WIDTH);
    canvas.vline(DIV, 11, CONFIG_LCD_HEIGHT - 22);

    const int top = 14, rowH = 10;
    for (int lane = 0; lane < Engine::BusCvCount; ++lane) {
        int y = top + lane * rowH;
        bool focus = lane == _busLane;
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

        // CONNECTIONS zone: live R/C/T writer glyphs, then enumerable source/reader tokens
        uint8_t writers = _engine.busWriters(lane);
        int cx = DIV + 6;
        struct { const char *g; uint8_t bit; } glyphs[] = {
            { "R", Engine::BusWriterRouting },
            { "C", Engine::BusWriterCvRouter },
            { "T", Engine::BusWriterTeletype },
        };
        for (auto &g : glyphs) {
            canvas.setColor((writers & g.bit) ? Color::Bright : Color::Low);
            canvas.drawText(cx, y + 4, g.g);
            cx += canvas.textWidth(g.g) + 4;
        }
        cx += 2;

        // enumerable routing routes: sources that write this lane (bright), targets that read it (dim)
        const auto &routing = _project.routing();
        Routing::Target laneTarget = Routing::Target(int(Routing::Target::BusCv1) + lane);
        Routing::Source laneSource = Routing::Source(int(Routing::Source::BusCv1) + lane);
        bool any = false;
        for (int r = 0; r < CONFIG_ROUTE_COUNT && cx < CONFIG_LCD_WIDTH - 8; ++r) {
            const auto &route = routing.route(r);
            if (!route.active()) {
                continue;
            }
            if (route.target() == laneTarget) {     // route writes the lane: show its source
                FixedStringBuilder<8> tok;
                Routing::printSourceAbbrev(route.source(), tok);
                canvas.setColor(focus ? Color::Bright : Color::Medium);
                canvas.drawText(cx, y + 4, tok);
                cx += canvas.textWidth(tok) + 5;
                any = true;
            }
        }
        for (int r = 0; r < CONFIG_ROUTE_COUNT && cx < CONFIG_LCD_WIDTH - 8; ++r) {
            const auto &route = routing.route(r);
            if (!route.active()) {
                continue;
            }
            if (route.source() == laneSource) {      // route reads the lane: show its target
                const char *nm = Routing::targetName(route.target());
                drawClippedName(canvas, cx, y + 4, CONFIG_LCD_WIDTH - 4 - cx, nm ? nm : "?");
                cx += canvas.textWidth(nm ? nm : "?") + 5;
                any = true;
            }
        }
        // enumerable CV-router lane links
        const auto &cvRoute = _project.cvRoute();
        if (cvRoute.outputDest(lane) == CvRoute::OutputDest::Bus || cvRoute.inputSource(lane) == CvRoute::InputSource::Bus) {
            any = true;
        }

        if (!any && writers == 0) {
            canvas.setColor(Color::Low);
            canvas.drawText(DIV + 6 + 3 * (canvas.textWidth("R") + 4) + 2, y + 4, "IDLE");
        }
    }

    const char *fn[] = { "VIEW", "ADD", "SAFE", "REMOVE", nullptr };
    WindowPainter::drawFooter(canvas, fn, pageKeyState(), _project.busSafety() ? int(Function::Combine) : -1);
}

void RoutingPage::handleBusKey(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isLeft() || key.isRight()) {     // band cycle: leave/enter the bus tab
        _tabEditorTab = (_tabEditorTab + (key.isLeft() ? 4 : 1)) % 5;
        _tabEditorRow = 0;
        tabRefocus();
        event.consume();
        return;
    }

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Combine: // F3 SAFE: toggle bus safety
            _project.setBusSafety(!_project.busSafety());
            break;
        default: // VIEW / ADD / REMOVE deferred to the follow-up slice
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
