#pragma once

#include "BasePage.h"

#include "ui/model/ListModel.h"

class ListPage : public BasePage {
public:
    ListPage(PageManager &manager, PageContext &context, ListModel &listModel);

    void setListModel(ListModel &listModel);

    virtual void show() override;

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;
    virtual void keyboard(KeyboardEvent &event) override;

    int selectedRow() const { return _selectedRow; }
    void setSelectedRow(int selectedRow);

    bool edit() const { return _edit; }
    void setEdit(bool edit) { _edit = edit; }

    void setTopRow(int row);

    // MOD+ entry: reserve an inert slot, enter inline edit with the new draft
    // active, and auto-open the source picker. Draft commits on F5, frees on F4.
    void beginNewModulation(Routing::Target target, int trackIndex);

protected:
    virtual void drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h);

private:
    void drawModulatedRow(Canvas &canvas, int row, int y);

    // SPREAD sub-view (Shift+S5 on a migrated row): full-frame per-track depth
    // editor over the owner-guarded draft. enterSpread ensures a draft is active,
    // drawSpread renders the 8 bars, trackEligible gates which tracks are editable.
    void enterSpread(int routeIndex);
    void drawSpread(Canvas &canvas);
    bool trackEligible(int trackIndex) const;
    int firstEligibleTrack() const;

    // True when `row` is a migrated routing target currently modulated for the
    // selected track; on success returns its routeIndex (>=0). Same gate as
    // drawModulatedRow — guards all inline modulation edit handling.
    bool modulatedRow(int row, int &routeIndex) const;

    void resetModEdit();

    // Cancels the live draft and clears edit state, but only when this page owns
    // the draft. Every navigate-away/leave site routes through here so the source
    // picker (also a ListPage) can't wipe the owner's draft mid round-trip.
    void cancelModEditIfOwner();

protected:

private:
    void scrollTo(int row);
    void editSelectedRow(int value, bool shift);

    static constexpr int LineHeight = 10;
    static constexpr int LineCount = 4;

    ListModel *_listModel;
    int _selectedRow = 0;
    int _displayRow = 0;
    bool _edit = false;
};
