#include "ListPage.h"

#include "ui/LedPainter.h"
#include "ui/pages/Pages.h"
#include "ui/painters/WindowPainter.h"

#include "core/math/Math.h"

#include "model/Track.h"
#include "model/RouteFork.h"
#include "model/RouteDraft.h"

namespace {

// Inline modulation edit state. The Draft holds a full Routing::Route (large), so it
// lives at file scope — one global edit at a time, mirroring gRouteDepthQuickEditModel
// in RoutingPage.cpp — rather than as a per-instance member multiplied across ~20 list
// pages. gModEditActive gates whether the draft is live; gModEditDepth selects which
// target (value vs depth) the encoder turn stages.
enum class ModEditTarget { Value, Depth };

RouteDraft::Draft gModDraft;
bool gModEditActive = false;
ModEditTarget gModEditTarget = ModEditTarget::Value;

// Full-frame SPREAD sub-view (Shift+S5): draws 8 per-track bipolar depth bars over
// the owner-guarded draft instead of the list rows. One file-scope flag, reset in
// resetModEdit() alongside the rest of the edit state.
bool gModSpread = false;

// Owning page of the live draft. The source picker is also a ListPage; gating the
// draw-time reset and key interception on this == owner keeps the picker's own
// draw/dispatch from cancelling or mutating the underlying page's draft.
const ListPage *gModEditOwner = nullptr;

// Short source label for the inline modulated-row badge: CvIn1->"CV1",
// CvOut1->"O1", BusCv1->"B1", GateOut1->"G1", Mod1->"M1".
void printSourceAbbrev(Routing::Source source, StringBuilder &str) {
    if (Routing::isCvSource(source)) {
        str("CV%d", int(source) - int(Routing::Source::CvIn1) + 1);
    } else if (source >= Routing::Source::CvOut1 && source <= Routing::Source::CvOut8) {
        str("O%d", int(source) - int(Routing::Source::CvOut1) + 1);
    } else if (Routing::isBusSource(source)) {
        str("B%d", int(source) - int(Routing::Source::BusCv1) + 1);
    } else if (source >= Routing::Source::GateOut1 && source <= Routing::Source::GateOut8) {
        str("G%d", int(source) - int(Routing::Source::GateOut1) + 1);
    } else if (Routing::isModulatorSource(source)) {
        str("M%d", int(source) - int(Routing::Source::Mod1) + 1);
    } else {
        Routing::printSource(source, str);
    }
}

// Horizontal bipolar depth bar: dim baseline x..x+w, center tick, and a fill
// from center outward — rightward for +depth, leftward for -depth.
void drawDepthBar(Canvas &canvas, int x, int y, int w, int depthPct, bool focus = false) {
    int cx = x + w / 2;
    canvas.setColor(Color::Low);
    canvas.hline(x, y + 1, w);
    canvas.vline(cx, y - 1, 5);
    int span = std::abs(depthPct) * (w / 2) / 100;
    if (span > 0) {
        canvas.setColor(focus ? Color::Bright : Color::Medium);
        canvas.fillRect(depthPct >= 0 ? cx : cx - span, y, span, 3);
    }
}

// Vertical bipolar depth bar for the SPREAD view: an outline column x..x+w over
// y..y+h, a center tick, and a fill growing from center — upward for +depth,
// downward for -depth. Dim when the track is ineligible (not editable).
void drawSpreadBar(Canvas &canvas, int x, int y, int w, int h, int depthPct, bool eligible, bool selected) {
    canvas.setColor(eligible ? (selected ? Color::Bright : Color::Medium) : Color::Low);
    canvas.drawRect(x, y, w, h);
    int cy = y + h / 2;
    canvas.hline(x, cy, w);
    int half = h / 2;
    int span = std::abs(depthPct) * half / 100;
    if (eligible && span > 0) {
        canvas.setColor(selected ? Color::Bright : Color::Medium);
        canvas.fillRect(x + 1, depthPct >= 0 ? cy - span : cy, w - 1, span);
    }
}

// Single-letter engine abbreviation for the SPREAD per-track labels.
char trackModeLetter(Track::TrackMode mode) {
    switch (mode) {
    case Track::TrackMode::Note:        return 'N';
    case Track::TrackMode::Curve:       return 'C';
    case Track::TrackMode::MidiCv:      return 'M';
    case Track::TrackMode::Tuesday:     return 'T';
    case Track::TrackMode::DiscreteMap: return 'D';
    case Track::TrackMode::Indexed:     return 'I';
    case Track::TrackMode::Teletype:    return '9';
    case Track::TrackMode::Stochastic:  return 'S';
    case Track::TrackMode::PhaseFlux:   return 'P';
    default:                            return '?';
    }
}

} // namespace

ListPage::ListPage(PageManager &manager, PageContext &context, ListModel &listModel) :
    BasePage(manager, context)
{
    setListModel(listModel);
}

void ListPage::setListModel(ListModel &listModel) {
    _listModel = &listModel;
    _selectedRow = 0;
    _displayRow = 0;
    _edit = false;
}

void ListPage::show() {
    BasePage::show();
}

void ListPage::enter() {
    scrollTo(_selectedRow);
}

void ListPage::exit() {
    cancelModEditIfOwner();
}

void ListPage::draw(Canvas &canvas) {
    int gateRouteIndex;
    bool spreadOwned = gModSpread && this == gModEditOwner;
    if (!spreadOwned && !modulatedRow(_selectedRow, gateRouteIndex)) {
        cancelModEditIfOwner();
    }

    if (spreadOwned) {
        drawSpread(canvas);
        return;
    }

    int displayRow = _displayRow;
    if (displayRow + LineCount > _listModel->rows()) {
        displayRow = std::max(0, _listModel->rows() - LineCount);
    }

    for (int i = 0; i < LineCount; ++i) {
        int row = displayRow + i;
        if (row < _listModel->rows()) {
            drawCell(canvas, row, 0, 8, 12 + i * LineHeight, 128 - 16, LineHeight);
            drawCell(canvas, row, 1, 128, 12 + i * LineHeight, 128 - 16, LineHeight);
            if (row == _selectedRow) {
                drawModulatedRow(canvas, row, 12 + i * LineHeight);
            }
        }
    }

    WindowPainter::drawScrollbar(canvas, Width - 8, 12, 4, LineCount * LineHeight, _listModel->rows(), LineCount, displayRow);

    int routeIndex;
    if (_edit && modulatedRow(_selectedRow, routeIndex)) {
        const char *names[] = { nullptr, "SRC", "COMBINE", "CANCEL", "COMMIT" };
        WindowPainter::drawFooter(canvas, names, pageKeyState());
    }
}

void ListPage::updateLeds(Leds &leds) {
}

void ListPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    int routeIndex;
    bool modRow = modulatedRow(_selectedRow, routeIndex);

    // Shift+S5 (step index 4) on a migrated routing row enters the SPREAD sub-view.
    if (modRow && !gModSpread && key.shiftModifier() && key.isStep() && key.step() == 4) {
        enterSpread(routeIndex);
        event.consume();
        return;
    }

    // SPREAD owns Left/Right for eligible-track navigation; F-keys fall through to
    // the shared inline handler below (same SRC/COMBINE/CANCEL/COMMIT, teardown via
    // cancelModEditIfOwner/resetModEdit which also clear gModSpread).
    if (gModSpread && this == gModEditOwner && (key.isLeft() || key.isRight())) {
        int sel = _project.selectedTrackIndex();
        int dir = key.isRight() ? 1 : -1;
        for (int t = sel + dir; t >= 0 && t < CONFIG_TRACK_COUNT; t += dir) {
            if (trackEligible(t)) {
                _project.setSelectedTrackIndex(t);
                break;
            }
        }
        event.consume();
        return;
    }

    bool spreadOwned = gModSpread && this == gModEditOwner;
    if ((modRow || spreadOwned) && _edit && key.isFunction() && _manager.top() == this) {
        auto &routing = _project.routing();
        switch (key.function()) {
        case 1: // F2 SRC
            if (!gModEditActive) {
                gModDraft = RouteDraft::begin(routing, routeIndex);
                gModEditActive = true;
                gModEditOwner = this;
            }
            _manager.pages().routeSourceSelect.show(
                _listModel->routingTarget(_selectedRow), gModDraft.route.source(),
                [this] (bool ok, Routing::Source picked) {
                    if (ok && picked != Routing::Source::None && gModEditActive) {
                        gModDraft.route.setSource(picked);
                    }
                });
            event.consume();
            return;
        case 2: // F3 COMBINE
            if (!gModEditActive) {
                gModDraft = RouteDraft::begin(routing, routeIndex);
                gModEditActive = true;
                gModEditOwner = this;
            }
            gModDraft.route.setCombine(
                gModDraft.route.combine() == RouteApply::Combine::Modulate ?
                    RouteApply::Combine::Absolute : RouteApply::Combine::Modulate);
            event.consume();
            return;
        case 3: // F4 CANCEL
            if (gModEditActive) {
                RouteDraft::cancel(routing, gModDraft);
            }
            resetModEdit();
            _edit = false;
            event.consume();
            return;
        case 4: // F5 COMMIT
            if (gModEditActive && RouteDraft::canCommit(gModDraft)) {
                RouteDraft::commit(routing, gModDraft);
                resetModEdit();
                _edit = false;
            }
            event.consume();
            return;
        default:
            break;
        }
    }

    if (key.isLeft()) {
        if (_edit) {
            editSelectedRow(-1, globalKeyState()[Key::Shift]);
        } else {
            setSelectedRow(selectedRow() - 1);
        }
        event.consume();
    }
    else if (key.isRight()) {
        if (_edit) {
            editSelectedRow(1, globalKeyState()[Key::Shift]);
        } else {
            setSelectedRow(selectedRow() + 1);
        }
        event.consume();
    } else if (key.isEncoder()) {
        if (modRow) {
            if (!_edit) {
                _edit = true;
                gModEditTarget = ModEditTarget::Value;
            } else if (gModEditTarget == ModEditTarget::Value) {
                if (!gModEditActive) {
                    gModDraft = RouteDraft::begin(_project.routing(), routeIndex);
                    gModEditActive = true;
                    gModEditOwner = this;
                }
                gModEditTarget = ModEditTarget::Depth;
            } else {
                gModEditTarget = ModEditTarget::Value;
            }
        } else {
            _edit = !_edit;
        }
        event.consume();
    }
}

void ListPage::encoder(EncoderEvent &event) {
    if (gModSpread && this == gModEditOwner && gModEditActive) {
        int t = _project.selectedTrackIndex();
        if (trackEligible(t)) {
            int depth = clamp(gModDraft.route.depthPct(t) + event.value(), -100, 100);
            gModDraft.route.setDepthPct(t, depth);
            gModDraft.route.setTracks(gModDraft.route.tracks() | (1 << t));
        }
        event.consume();
        return;
    }
    if (_edit && gModEditActive && this == gModEditOwner && gModEditTarget == ModEditTarget::Depth) {
        int trackIndex = _project.selectedTrackIndex();
        int depth = clamp(gModDraft.route.depthPct(trackIndex) + event.value(), -100, 100);
        gModDraft.route.setDepthPct(trackIndex, depth);
    } else if (_edit) {
        editSelectedRow(event.value(), event.pressed() || globalKeyState()[Key::Shift]);
    } else {
        setSelectedRow(selectedRow() + event.value());
    }
    event.consume();
}

void ListPage::keyboard(KeyboardEvent &event) {
    switch (event.keycode()) {
    case KeyboardEvent::KeyUp:
        setSelectedRow(selectedRow() - 1);
        event.consume();
        break;
    case KeyboardEvent::KeyDown:
        setSelectedRow(selectedRow() + 1);
        event.consume();
        break;
    case KeyboardEvent::KeyLeft:
        editSelectedRow(-1, event.shift());
        event.consume();
        break;
    case KeyboardEvent::KeyRight:
        editSelectedRow(1, event.shift());
        event.consume();
        break;
    case KeyboardEvent::KeyEnter:
        _edit = !_edit;
        event.consume();
        break;
    default:
        BasePage::keyboard(event);
        break;
    }
}

void ListPage::editSelectedRow(int value, bool shift) {
    const bool isTeletype = _project.selectedTrack().trackMode() == Track::TrackMode::Teletype;
    if (isTeletype) {
        _engine.lock();
    }
    _listModel->edit(_selectedRow, 1, value, shift);
    if (isTeletype) {
        _engine.unlock();
    }
}

void ListPage::setSelectedRow(int selectedRow) {
    int rows = _listModel->rows();
    if (rows <= 0) {
        _selectedRow = 0;
        _displayRow = 0;
        return;
    }

    int nextRow = selectedRow % rows;
    if (nextRow < 0) {
        nextRow += rows;
    }

    if (nextRow != _selectedRow) {
        cancelModEditIfOwner();
        _selectedRow = nextRow;
        scrollTo(_selectedRow);
    }
}

void ListPage::setTopRow(int row) {
    _displayRow = std::min(row, std::max(0, _listModel->rows() - LineCount));
}

void ListPage::drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h) {
    FixedStringBuilder<32> str;
    _listModel->cell(row, column, str);
    canvas.setFont(Font::Small);
    canvas.setBlendMode(BlendMode::Set);
    bool highlight = column == int(_edit) && row == _selectedRow;
    if (highlight && gModEditActive && gModEditTarget == ModEditTarget::Depth && row == _selectedRow) {
        highlight = false;
    }
    canvas.setColor(highlight ? Color::Bright : Color::Medium);
    canvas.drawText(x, y + 7, str);
}

bool ListPage::modulatedRow(int row, int &routeIndex) const {
    routeIndex = -1;
    Routing::Target target = _listModel->routingTarget(row);
    if (target == Routing::Target::None) {
        return false;
    }

    const auto &track = _project.selectedTrack();
    uint8_t key = 0;
    RouteParam::Range range;
    bool isMigrated = RouteFork::migrated(track.trackMode(), target, key, range) ||
                      RouteFork::migratedGlobal(target, key, range);
    if (!isMigrated) {
        return false;
    }

    int trackIndex = _project.selectedTrackIndex();
    const auto &routing = _project.routing();
    if (!RouteDraft::isTrackModulated(routing, target, trackIndex)) {
        return false;
    }

    routeIndex = routing.findRoute(target, trackIndex);
    return routeIndex >= 0;
}

void ListPage::beginNewModulation(Routing::Target target, int trackIndex) {
    auto &routing = _project.routing();
    gModDraft = RouteDraft::create(routing, target, trackIndex);
    if (gModDraft.routeIndex < 0) {
        showMessage("NO EMPTY ROUTES");
        return;
    }
    gModEditActive = true;
    gModEditOwner = this;
    gModEditTarget = ModEditTarget::Depth;
    _edit = true;
    _manager.pages().routeSourceSelect.show(
        target, gModDraft.route.source(),
        [this] (bool ok, Routing::Source picked) {
            if (ok && picked != Routing::Source::None && gModEditActive) {
                gModDraft.route.setSource(picked);
            }
        });
}

void ListPage::resetModEdit() {
    gModEditActive = false;
    gModEditTarget = ModEditTarget::Value;
    gModEditOwner = nullptr;
    gModSpread = false;
}

void ListPage::cancelModEditIfOwner() {
    if (gModEditActive && this == gModEditOwner) {
        RouteDraft::cancel(_project.routing(), gModDraft);
        resetModEdit();
    }
}

void ListPage::drawModulatedRow(Canvas &canvas, int row, int y) {
    int routeIndex;
    if (!modulatedRow(row, routeIndex)) {
        return;
    }

    int trackIndex = _project.selectedTrackIndex();

    Routing::Source src;
    int depthPct;
    if (gModEditActive) {
        src = gModDraft.route.source();
        depthPct = gModDraft.route.depthPct(trackIndex);
    } else {
        const auto &route = _project.routing().route(routeIndex);
        src = route.source();
        depthPct = route.depthPct(trackIndex);
    }

    bool depthFocus = gModEditActive && gModEditTarget == ModEditTarget::Depth;

    FixedStringBuilder<8> srcStr;
    printSourceAbbrev(src, srcStr);
    canvas.setFont(Font::Small);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Medium);
    canvas.drawText(78, y + 7, srcStr);
    canvas.drawText(96, y + 7, ">");

    drawDepthBar(canvas, 104, y + 4, 92, depthPct, depthFocus);
}

bool ListPage::trackEligible(int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return false;
    }
    Routing::Target target = _listModel->routingTarget(_selectedRow);
    if (target == Routing::Target::None) {
        return false;
    }
    uint8_t key = 0;
    RouteParam::Range range;
    return RouteFork::migrated(_project.track(trackIndex).trackMode(), target, key, range);
}

int ListPage::firstEligibleTrack() const {
    for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
        if (trackEligible(t)) {
            return t;
        }
    }
    return -1;
}

void ListPage::enterSpread(int routeIndex) {
    if (routeIndex < 0) {
        return;
    }
    auto &routing = _project.routing();
    if (!gModEditActive) {
        gModDraft = RouteDraft::begin(routing, routeIndex);
        gModEditActive = true;
        gModEditOwner = this;
    }
    gModSpread = true;
    _edit = true;
    int first = firstEligibleTrack();
    if (first >= 0) {
        _project.setSelectedTrackIndex(first);
    }
}

void ListPage::drawSpread(Canvas &canvas) {
    canvas.setBlendMode(BlendMode::Set);

    char header[24];
    {
        const char *name = Routing::targetName(_listModel->routingTarget(_selectedRow));
        int i = 0;
        for (; name[i] && i < int(sizeof(header)) - 1; ++i) {
            char c = name[i];
            header[i] = (c >= 'a' && c <= 'z') ? char(c - 32) : c;
        }
        header[i] = 0;
    }
    FixedStringBuilder<8> srcStr;
    printSourceAbbrev(gModDraft.route.source(), srcStr);

    canvas.setFont(Font::Small);
    canvas.setColor(Color::Bright);
    canvas.drawText(2, 8, header);
    int hx = 4 + canvas.textWidth(header) + 4;
    canvas.setColor(Color::Medium);
    canvas.drawText(hx, 8, "<");
    canvas.drawText(hx + 8, 8, srcStr);
    const char *spread = "SPREAD";
    canvas.drawText(Width - canvas.textWidth(spread) - 2, 8, spread);

    int sel = _project.selectedTrackIndex();
    int colW = Width / CONFIG_TRACK_COUNT;
    int barW = 14;
    int barTop = 12;
    int barH = 27;
    for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
        int cx = t * colW + colW / 2;
        bool eligible = trackEligible(t);
        bool selected = eligible && t == sel;
        drawSpreadBar(canvas, cx - barW / 2, barTop, barW, barH,
                      gModDraft.route.depthPct(t), eligible, selected);

        FixedStringBuilder<4> label;
        label("%d%c", t + 1, trackModeLetter(_project.track(t).trackMode()));
        int lw = canvas.textWidth(label);
        int lx = cx - lw / 2;
        canvas.setColor(eligible ? (selected ? Color::Bright : Color::Medium) : Color::Low);
        canvas.drawText(lx, barTop + barH + 9, label);
        if (selected) {
            canvas.hline(lx, barTop + barH + 11, lw);
        }
    }

    const char *names[] = { nullptr, "SRC", "COMBINE", "CANCEL", "COMMIT" };
    WindowPainter::drawFooter(canvas, names, pageKeyState());
}

void ListPage::scrollTo(int row) {
    if (row < _displayRow) {
        _displayRow = row;
    } else if (row >= _displayRow + LineCount) {
        _displayRow = row - (LineCount - 1);
    }
}
