#include "Debugger.h"

#include "Model.h"
#include "Views.h"

using namespace sdb;
using namespace ftxui;

Debugger::Debugger(Model* model):
    model(model),
    screen(ScreenInteractive::Fullscreen()),
    instView(InstructionView(model)),
    fileOpenPanel(OpenFilePanel(model)),
    settingsView(SettingsView()) {
    model->setRefreshCallback(
        [this] { screen.PostEvent(Event::Special("Wakeup call")); });
    model->setReloadCallback([this] { instView->refresh(); });
    auto sidebar = Container::Vertical(
        { FlagsView(model), sdb::separator(), RegisterView(model) });
    auto centralSplit = splitRight(sidebar, instView, 30);
    auto toolbar = ToolbarView(model,
                               fileOpenPanel.openCommand(),
                               settingsView.openCommand());
    auto top = Container::Vertical({
        sdb::separator(),
        toolbar,
        sdb::separator(),
        centralSplit | flex,
    });
    auto bottom = Container::Vertical(
        { StepControlsView(model), sdb::separator(), ConsoleView(model) });
    root = splitBottom(bottom, top) | CatchEvent([=](Event event) {
               if (event.is_character()) {
                   for (auto& [key, command]: keyCommands) {
                       if (event.character() == key) {
                           command();
                           return true;
                       }
                   }
               }
               return false;
           }) |
           fileOpenPanel.overlay() | settingsView.overlay();

    addKeyCommand("q", [=] {
        model->shutdown();
        screen.Exit();
    });
    addKeyCommand("r", [=] { model->run(); });
    addKeyCommand("x", [=] { model->shutdown(); });
    addKeyCommand("o", fileOpenPanel.openCommand());
    addKeyCommand("p", [=] { model->toggleExecution(); });
    addKeyCommand("s", [=] { model->skipLine(); });
    addKeyCommand("e", [=] { model->enterFunction(); });
    addKeyCommand("l", [=] { model->exitFunction(); });
}

void Debugger::run() { screen.Loop(root); }

void Debugger::addKeyCommand(std::string key, std::function<void()> command) {
    keyCommands.push_back({ std::move(key), std::move(command) });
}
