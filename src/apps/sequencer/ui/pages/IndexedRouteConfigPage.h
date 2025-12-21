#pragma once

#include "BasePage.h"
#include "model/IndexedSequence.h"

class IndexedRouteConfigPage : public BasePage {
public:
    IndexedRouteConfigPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;
    virtual bool isModal() const override { return true; }

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class ActiveRoute {
        RouteA,
        RouteB
    };

    enum class EditParam {
        Enabled,
        TargetGroups,
        TargetParam,
        Amount
    };

    ActiveRoute _activeRoute = ActiveRoute::RouteA;
    EditParam _editParam = EditParam::Enabled;
    IndexedSequence::RouteConfig _routeAStaged;
    IndexedSequence::RouteConfig _routeBStaged;

    IndexedSequence::RouteConfig& activeRouteConfig();
    const IndexedSequence::RouteConfig& activeRouteConfig() const;
    bool stagedChanged() const;

    void drawRouteConfig(Canvas &canvas, const IndexedSequence::RouteConfig &cfg, int y, bool active, const char *label);
    void drawGroupMask(Canvas &canvas, uint8_t groupMask, int x, int y);
};
