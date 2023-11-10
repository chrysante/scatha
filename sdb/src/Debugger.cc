#include "Debugger.h"

#include "Model.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

Debugger::Debugger(Model* model):
    model(model), screen(ScreenInteractive::Fullscreen()), regViewSize(20) {
    model->setRefreshScreenClosure(
        [this] { screen.PostEvent(Event::Special("Wakeup call")); });
    model->virtualMachine().setIOStreams(nullptr, &standardout);
    settings = SettingsView([this] { showSettings = false; });
    root = ResizableSplitRight(RegisterView(model),
                               InstructionView(model),
                               &regViewSize);
    root = Container::Vertical({
        Renderer([] { return separator(); }),
        ControlView(
            model,
            [this] {
        showSettings = true;
        settings->TakeFocus();
            }),
        Renderer([] { return separator(); }),
        root | flex,
    });
    root = ResizableSplitBottom(ConsoleView(model, standardout),
                                root,
                                &consoleViewSize);
    root |= Modal(settings, &showSettings);
    root |= CatchEvent([=](Event event) {
        if (event.is_character()) {
            for (auto& [key, command]: keyCommands) {
                if (event.character() == key) {
                    command();
                    return true;
                }
            }
        }
        return false;
    });

    addKeyCommand("q", [=] { screen.Exit(); });
    addKeyCommand("p", [=] { model->toggleExecution(); });
    addKeyCommand("s", [=] { model->skipLine(); });
    addKeyCommand("e", [=] { model->enterFunction(); });
    addKeyCommand("l", [=] { model->exitFunction(); });
}

void Debugger::run() {
    model->startExecutionThread();
    auto renderer = Renderer(root, [&] { return root->Render(); });
    screen.Loop(renderer);
}

void Debugger::addKeyCommand(std::string key, std::function<void()> command) {
    keyCommands.push_back({ std::move(key), std::move(command) });
}
