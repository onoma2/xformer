#pragma once

#include "ListPage.h"

#include "model/Routing.h"
#include "model/RouteDraft.h"

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
    void enterTabEditor();
    void tabRefocus();          // point _route at the cursor row's committed route (if routed)
    void matrixEnterEdit();     // encoder-press in nav: begin/create the per-row draft
    void matrixExitEdit(bool commit); // commit or cancel the draft, return to nav
    int routeForBandParam(uint8_t paramKey, uint8_t trackMask) const;
    int tabBandParamCount() const;

    enum class MatrixView { Depth, Source };
    MatrixView _matrixView = MatrixView::Depth;

    int _tabEditorTab = 0;
    int _tabEditorRow = 0;
    int _tabCol = 0;             // cursor track column
    bool _tabRowRouted = false;  // cursor row resolves to a committed route (_route)

    RouteDraft::Draft _matrixDraft;        // per-row staging buffer (single-instance page)
    bool _matrixEditActive = false;
    int _matrixEditRow = -1;

    Routing::Route *_route = nullptr;
    uint8_t _routeIndex = 0;
};
