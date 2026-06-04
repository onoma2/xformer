#pragma once

#include "ListPage.h"

#include "ui/model/RouteListModel.h"

#include "model/Routing.h"

#include "engine/MidiLearn.h"

#include <array>

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

    void showRoute(int routeIndex, const Routing::Route *initialValue = nullptr);

private:
    virtual void drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h) override;

    // route overview (home): list of all routes, source>target | track-mask | depth
    bool overviewActive() const { return _overviewActive; }
    void drawOverview(Canvas &canvas);
    void handleOverviewKey(KeyPressEvent &event);
    void overviewEncoder(int delta);
    void syncOverviewScroll();
    void openRouteFromOverview();
    bool _overviewActive = true;
    int _overviewSel = 0;
    int _overviewScroll = 0;

    // tab editor (read-only display shell of the new lens editor; Page+S6)
    bool tabEditorActive() const { return _tabEditorActive; }
    void drawTabEditor(Canvas &canvas);
    void handleTabEditorKey(KeyPressEvent &event);
    void enterTabEditor();
    void tabRefocus();          // point _route at the cursor row's committed route (if routed)
    void tabCreateRoute();      // create + focus a route for the cursor's empty param
    int routeForBandParam(uint8_t paramKey, uint8_t trackMask) const;
    int tabBandParamCount() const;
    bool _tabEditorActive = false;
    int _tabEditorTab = 0;
    int _tabEditorRow = 0;
    bool _tabRowRouted = false;  // cursor row resolves to a committed route (_route)
    uint8_t _tabScopeMask = 0;   // scope fixed on entry (per-track mask, or 0 = global)

    void selectRoute(int routeIndex);
    void assignMidiLearn(const MidiLearn::Result &result);
    void commitRoute();

    RouteListModel _routeListModel;
    Routing::Route *_route;
    uint8_t _routeIndex;
    Routing::Route _editRoute;
};
