#pragma once

#include "ListPage.h"

#include "model/Routing.h"

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
    // tab editor (matrix precursor): ROUTING opens directly into this view
    void drawTabEditor(Canvas &canvas);
    void handleTabEditorKey(KeyPressEvent &event);
    void enterTabEditor();
    void tabRefocus();          // point _route at the cursor row's committed route (if routed)
    void tabCreateRoute();      // create + focus a route for the cursor's empty param
    int routeForBandParam(uint8_t paramKey, uint8_t trackMask) const;
    int tabBandParamCount() const;
    int _tabEditorTab = 0;
    int _tabEditorRow = 0;
    bool _tabRowRouted = false;  // cursor row resolves to a committed route (_route)
    uint8_t _tabScopeMask = 0;   // scope fixed on entry (per-track mask, or 0 = global)

    Routing::Route *_route = nullptr;
    uint8_t _routeIndex = 0;
};
