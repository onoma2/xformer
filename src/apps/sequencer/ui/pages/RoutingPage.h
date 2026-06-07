#pragma once

#include "ListPage.h"

#include "model/Routing.h"
#include "model/RouteDraft.h"
#include "model/RouteBrowse.h"
#include "model/Track.h"

class RoutingPage : public ListPage {
public:
    RoutingPage(PageManager &manager, PageContext &context);

    void reset();

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;
    virtual void keyboard(KeyboardEvent &event) override;

private:
    // ROUTING opens directly into the matrix grid view
    void drawTabEditor(Canvas &canvas);
    void handleTabEditorKey(KeyPressEvent &event);
    void drawBus(Canvas &canvas);              // bus tab: bus hub display
    void handleBusKey(KeyPressEvent &event);   // bus tab: lane cursor + edit + band cycle
    void busEnterEdit();                        // begin/create the focused lane's draft
    void enterTabEditor();
    void tabRefocus();          // point _route at the cursor row's committed route (if routed)
    void matrixEnterEdit();     // encoder-press in nav: begin/create the per-row draft
    void matrixExitEdit(bool commit); // commit or cancel the draft, return to nav
    void matrixEditSource(int delta, bool group); // SOURCE view: cycle the draft source
    int routeForBandParam(uint8_t paramKey, uint8_t trackMask) const;
    int tabBandParamCount() const;

    // Static tab ring (fixed positions for muscle memory): 3 fixed bands, then 7 engine
    // pages (MidiCv excluded — its params are all shared band keys), then Bus.
    static constexpr int kBandCount = 3;
    static constexpr int kEngineCount = 7;
    static constexpr int kBusTab = kBandCount + kEngineCount;   // 10
    static constexpr int kTabCount = kBusTab + 1;               // 11
    bool tabIsBus(int t) const { return t == kBusTab; }
    bool tabIsEngine(int t) const { return t >= kBandCount && t < kBusTab; }
    Track::TrackMode tabEngineMode(int t) const;     // engine tab index -> TrackMode
    int tabParams(int t, uint8_t *keys, int maxKeys) const;  // band or engine page keys
    const char *tabName(int t) const;                // header label for tab t
    const char *tabParamName(int t, uint8_t key) const;      // row label (short pref)

    enum class MatrixView { Depth, Source };
    MatrixView _matrixView = MatrixView::Depth;

    int _tabEditorTab = 0;
    int _tabEditorRow = 0;
    int _tabScroll = 0;          // engine pages: first visible row (vertical scroll)
    int _tabCol = 0;             // cursor track column
    int _busLane = 0;            // bus tab: focused lane cursor 0..3
    bool _tabRowRouted = false;  // cursor row resolves to a committed route (_route)

    RouteDraft::Draft _matrixDraft;        // per-row staging buffer (single-instance page)
    bool _matrixEditActive = false;
    int _matrixEditRow = -1;

    Routing::Route *_route = nullptr;
    uint8_t _routeIndex = 0;
};
