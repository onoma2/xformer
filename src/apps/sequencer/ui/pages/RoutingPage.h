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

    void showRoute(int routeIndex, const Routing::Route *initialValue = nullptr);

private:
    virtual void drawCell(Canvas &canvas, int row, int column, int x, int y, int w, int h) override;

    // bias/depth overlay
    void enterBiasOverlay();
    void exitBiasOverlay(bool commit);
    void drawBiasOverlay(Canvas &canvas);
    void handleBiasOverlayKey(KeyPressEvent &event);
    void editBiasOverlay(int delta, bool shift);
    void showBiasOverlayContext();
    void biasOverlayContextAction(int index);
    int focusTrackIndex() const;
    bool overlayActive() const { return _biasOverlayActive; }

    void selectRoute(int routeIndex);
    void assignMidiLearn(const MidiLearn::Result &result);

    RouteListModel _routeListModel;
    Routing::Route *_route;
    uint8_t _routeIndex;
    Routing::Route _editRoute;

    bool _biasOverlayActive = false;
    std::array<int8_t, CONFIG_TRACK_COUNT> _biasStaging;
    std::array<int8_t, CONFIG_TRACK_COUNT> _depthStaging;
    std::array<int8_t, CONFIG_TRACK_COUNT> _biasClipboard{};
    std::array<int8_t, CONFIG_TRACK_COUNT> _depthClipboard{};
    bool _clipboardValid = false;
    std::array<uint8_t, 4> _slotState{}; // 0..3: A bias, A depth, B bias, B depth
    int _activeSlot = 0;
};
