#include "Views.h"

#include <functional>

#include <ftxui/dom/elements.hpp>

#include "Common.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct ButtonInfo {
    std::function<Element()> label;
    std::function<bool()> active;
    std::function<void()> action;

    ButtonInfo(Element label,
               std::function<bool()> active,
               std::function<void()> action):
        label([=] { return label; }),
        active(std::move(active)),
        action(std::move(action)) {}

    ButtonInfo(std::function<Element()> label,
               std::function<bool()> active,
               std::function<void()> action):
        label(std::move(label)),
        active(std::move(active)),
        action(std::move(action)) {}

    operator Component() const {
        auto opt = ButtonOption::Simple();
        opt.transform = [label = label, active = active](EntryState const&) {
            auto elem = label();
            if (!active()) {
                elem |= color(Color::GrayLight);
            }
            return elem | center | size(WIDTH, EQUAL, 4);
        };
        return Button(
            "Button",
            [active = active, action = action] {
            if (active()) {
                action();
            }
            else {
                beep();
            }
            },
            opt);
    }
};

} // namespace

static ButtonInfo runButton(Model* model) {
    return { [=] { return model->isRunning() ? text("||") : text("|>"); },
             [=] { return true; },
             [=] { model->toggleExecution(); } };
}

static ButtonInfo skipButton(Model* model) {
    return { text(">_"),
             [=] { return !model->isRunning(); },
             [=] { model->skipLine(); } };
}

static ButtonInfo enterFunctionButton(Model* model) {
    return { text(">\\"),
             [=] { return !model->isRunning(); },
             [=] { model->enterFunction(); } };
}

static ButtonInfo exitFunctionButton(Model* model) {
    return { text("^|"),
             [=] { return !model->isRunning(); },
             [=] { model->exitFunction(); } };
}

static Component settingsButton(std::function<void()> showSettings) {
    auto opt = ButtonOption::Simple();
    opt.transform = [](EntryState const&) { return text("Settings"); };
    return Button("", showSettings, opt);
}

static Component space() {
    return Renderer([] { return separatorEmpty(); });
}

namespace {

struct CtrlView: ComponentBase {
    CtrlView(Model* model, std::function<void()> showSettings): model(model) {
        Add(Container::Horizontal({
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
