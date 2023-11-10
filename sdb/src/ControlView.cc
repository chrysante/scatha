#include "Views.h"

#include <functional>

#include <ftxui/dom/elements.hpp>
#include <utl/utility.hpp>

#include "Common.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct ButtonInfo {
    std::function<std::string()> label;
    std::function<bool()> active;
    std::function<void()> action;

    ButtonInfo(std::string label,
               std::function<bool()> active,
               std::function<void()> action):
        label([=] { return label; }),
        active(std::move(active)),
        action(std::move(action)) {}

    ButtonInfo(std::function<std::string()> label,
               std::function<bool()> active,
               std::function<void()> action):
        label(std::move(label)),
        active(std::move(active)),
        action(std::move(action)) {}

    operator Component() const {
        auto opt = ButtonOption::Simple();
        opt.transform = [active = active, label = label](EntryState const&) {
            auto str = label();
            auto elem = text(str) | bold;
            if (!active()) {
                elem |= color(Color::GrayDark);
            }
            return elem | center |
                   size(WIDTH, EQUAL, utl::narrow_cast<int>(str.size() + 2));
        };
        auto callback = [active = active, action = action] {
            if (active()) {
                action();
            }
            else {
                beep();
            }
        };
        return Button("Button", callback, opt);
    }
};

} // namespace

static ButtonInfo runButton(Model* model) {
    return { [=] { return model->isSleeping() ? ">_" : "||"; },
             [=] { return model->isActive(); },
             [=] { model->toggleExecution(); } };
}

static ButtonInfo skipButton(Model* model) {
    return { "⋀_",
             [=] { return model->isActive() && model->isSleeping(); },
             [=] { model->skipLine(); } };
}

static ButtonInfo enterFunctionButton(Model* model) {
    return { ">\\",
             [=] { return model->isActive() && model->isSleeping(); },
             [=] { model->enterFunction(); } };
}

static ButtonInfo exitFunctionButton(Model* model) {
    return { "^|",
             [=] { return model->isActive() && model->isSleeping(); },
             [=] { model->exitFunction(); } };
}

static ButtonInfo restartButton(Model* model) {
    return { ">R", [=] { return true; }, [=] { model->restart(); } };
}

static ButtonInfo settingsButton(std::function<void()> showSettings) {
    return { "Settings", [=] { return true; }, showSettings };
}

static Component space() {
    return Renderer([] { return separatorEmpty(); });
}

namespace {

struct CtrlView: ComponentBase {
    CtrlView(Model* model, std::function<void()> showSettings): model(model) {
        Add(Container::Horizontal({
            restartButton(model),
            space(),
            runButton(model),
            space(),
            skipButton(model),
            space(),
            enterFunctionButton(model),
            space(),
            exitFunctionButton(model),
            Renderer([] { return filler(); }),
            settingsButton(showSettings),
            space(),
        }));
    }

    Model* model;
};

} // namespace

ftxui::Component sdb::ControlView(Model* model,
                                  std::function<void()> showSettings) {
    return Make<CtrlView>(model, showSettings);
}
