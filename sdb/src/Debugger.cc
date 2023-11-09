#include "Debugger.h"

#include <thread>

#include "Program.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

Debugger::Debugger(Program* prog):
    prog(prog), screen(ScreenInteractive::Fullscreen()), regViewSize(20) {
    root = ResizableSplitRight(RegisterView(prog),
                               InstructionView(prog),
                               &regViewSize);
    root = Container::Vertical({
        root | flex,
        Renderer([] { return separator(); }),
        ControlView(prog),
    });
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
    addKeyCommand("p", [=] { prog->toggleExecution(); });
    addKeyCommand("s", [=] { prog->skipLine(); });
    addKeyCommand("e", [=] { prog->enterFunction(); });
    addKeyCommand("l", [=] { prog->exitFunction(); });
}

void Debugger::run() {
    auto renderer = Renderer(root, [&] { return root->Render(); });
    std::atomic_bool running = true;
    std::thread bgThread([this, &running] {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
