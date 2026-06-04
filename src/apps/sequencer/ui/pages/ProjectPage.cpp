#include "ProjectPage.h"

#include "ui/LedPainter.h"
#include "ui/pages/Pages.h"
#include "ui/painters/WindowPainter.h"

#include "model/FileManager.h"
#include "model/RouteDraft.h"
#include "model/RouteFork.h"
#include "model/RouteParam.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Load,
    Save,
    SaveAs,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItemsModAdd[] = {
    { "INIT" },
    { "LOAD" },
    { "SAVE" },
    { "SAVE AS" },
    { "MOD+" }
};

static const ContextMenuModel::Item contextMenuItemsModRemove[] = {
    { "INIT" },
    { "LOAD" },
    { "SAVE" },
    { "SAVE AS" },
    { "MOD-" }
};

ProjectPage::ProjectPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel),
    _listModel(context.model.project())
{}

void ProjectPage::enter() {
}

void ProjectPage::exit() {
}

void ProjectPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "PROJECT");
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void ProjectPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void ProjectPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() && event.count() == 2) {
        contextShow(true);
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        // easter egg
        if (key.is(Key::Step15)) {
            _manager.pages().asteroids.show();
        }
        return;
    }

    if (key.is(Key::Encoder) && selectedRow() == 0) {
        _manager.pages().textInput.show("NAME:", _project.name(), Project::NameLength, [this] (bool result, const char *text) {
            if (result) {
                _project.setName(text);
            }
        });

        return;
    }

    ListPage::keyPress(event);
}

void ProjectPage::encoder(EncoderEvent &event) {
    ListPage::encoder(event);
}

void ProjectPage::keyboard(KeyboardEvent &event) {
    if (event.isPressed() && event.keycode() == KeyboardEvent::KeyEnter && selectedRow() == 0) {
        _manager.pages().textInput.show("NAME:", _project.name(), Project::NameLength,
            [this] (bool result, const char *text) {
                if (result) {
                    _project.setName(text);
                }
            });
        event.consume();
        return;
    }
    ListPage::keyboard(event);
}

void ProjectPage::contextShow(bool doubleClick) {
    auto target = _listModel.routingTarget(selectedRow());
    bool modulated = RouteDraft::isTrackModulated(_project.routing(), target, 0);
    const auto &items = modulated ? contextMenuItemsModRemove : contextMenuItemsModAdd;
    showContextMenu(ContextMenu(
        items,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); },
        doubleClick
    ));
}

void ProjectPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initProject();
        break;
    case ContextAction::Load:
        loadProject();
        break;
    case ContextAction::Save:
        saveProject();
        break;
    case ContextAction::SaveAs:
        saveAsProject();
        break;
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Last:
        break;
    }
}

bool ProjectPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Load:
    case ContextAction::Save:
    case ContextAction::SaveAs:
        return FileManager::volumeMounted();
    case ContextAction::Route: {
        auto target = _listModel.routingTarget(selectedRow());
        if (target == Routing::Target::None) return false;
        uint8_t key; RouteParam::Range range;
        return RouteFork::migratedGlobal(target, key, range);
    }
    default:
        return true;
    }
}

void ProjectPage::initProject() {
    _manager.pages().confirmation.show("ARE YOU SURE?", [this] (bool result) {
        if (result) {
            _engine.suspend();
            _project.clear();
            showMessage("PROJECT INITIALIZED");
            _engine.resume();
        }
    });
}

void ProjectPage::loadProject() {
    _manager.pages().fileSelect.show("LOAD PROJECT", FileType::Project, _project.slotAssigned() ? _project.slot() : 0, false, [this] (bool result, int slot) {
        if (result) {
            _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                if (result) {
                    loadProjectFromSlot(slot);
                }
            });
        }
    });
}

void ProjectPage::saveProject() {
    if (!_project.slotAssigned() || _project.autoLoaded()) {
        saveAsProject();
        return;
    }

    saveProjectToSlot(_project.slot());
}

void ProjectPage::saveAsProject() {
    _manager.pages().fileSelect.show("SAVE PROJECT", FileType::Project, _project.slotAssigned() ? _project.slot() : 0, true, [this] (bool result, int slot) {
        if (result) {
            if (FileManager::slotUsed(FileType::Project, slot)) {
                _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                    if (result) {
                        saveProjectToSlot(slot);
                    }
                });
            } else {
                saveProjectToSlot(slot);
            }
        }
    });
}

void ProjectPage::initRoute() {
    Routing::Target target = _listModel.routingTarget(selectedRow());
    auto &routing = _project.routing();
    if (RouteDraft::isTrackModulated(routing, target, 0)) {
        RouteDraft::removeTrack(routing, target, 0);   // MOD-
    } else {
        beginNewModulation(target, 0);                 // MOD+
    }
}

void ProjectPage::saveProjectToSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show("SAVING PROJECT ...");

    FileManager::task([this, slot] () {
        return FileManager::writeProject(_project, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage("PROJECT SAVED");
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}

void ProjectPage::loadProjectFromSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show("LOADING PROJECT ...");

    FileManager::task([this, slot] () {
        // TODO this is running in file manager thread but model notification affect ui
        return FileManager::readProject(_project, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage("PROJECT LOADED");
        } else if (result == fs::INVALID_CHECKSUM) {
            showMessage("INVALID PROJECT FILE");
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}
