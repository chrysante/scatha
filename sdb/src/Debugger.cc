#include "Debugger.h"

#include <thread>

#include "Model.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

Debugger::Debugger(Model* model):
    screen(ScreenInteractive::Fullscreen()), regViewSize(20) {
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
    root = ResizableSplitBottom(ConsoleView(model), root, &consoleViewSize);
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
    auto renderer = Renderer(root, [&] { return root->Render(); });
    std::atomic_bool running = true;
    std::thread bgThread([this, &running] {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            screen.PostEvent(Event::Special("Wakeup call"));
        }
    });
    screen.Loop(renderer);
    running = false;
    bgThread.join();
}

void Debugger::addKeyCommand(std::string key, std::function<void()> command) {
    keyCommands.push_back({ std::move(key), std::move(command) });
}
