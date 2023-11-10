#include "Debugger.h"

#include "Common.h"
#include "Model.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

Debugger::Debugger(Model* model):
    model(model), screen(ScreenInteractive::Fullscreen()), sidebarSize(29) {
    model->setRefreshCallback(
        [this] { screen.PostEvent(Event::Special("Wakeup call")); });
    settings = SettingsView([this] { showSettings = false; });
    auto sidebar = Container::Vertical(
        { FlagsView(model), sdb::separator(), RegisterView(model) });
    auto centralSplit =
        ResizableSplitRight(sidebar, InstructionView(model), &sidebarSize);
    auto toolbar = ToolbarView(model, [this] {
        showSettings = true;
        settings->TakeFocus();
    });
    auto top = Container::Vertical({
        sdb::separator(),
        toolbar,
        sdb::separator(),
        centralSplit | flex,
    });

    auto bottom = Container::Vertical(
        { StepControlsView(model), sdb::separator(), ConsoleView(model) });

    root = ResizableSplitBottom(bottom, top, &bottomSectionSize) |
           Modal(settings, &showSettings) | CatchEvent([=](Event event) {
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

    addKeyCommand("q", [=] { model->shutdown(); screen.Exit(); });
    addKeyCommand("p", [=] { model->toggleExecution(); });
    addKeyCommand("s", [=] { model->skipLine(); });
    addKeyCommand("e", [=] { model->enterFunction(); });
    addKeyCommand("l", [=] { model->exitFunction(); });
    addKeyCommand("r", [=] { model->restart(); });
}

void Debugger::run() {
    screen.Loop(root);
}

void Debugger::addKeyCommand(std::string key, std::function<void()> command) {
    keyCommands.push_back({ std::move(key), std::move(command) });
}
