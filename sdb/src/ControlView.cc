#include "Views.h"

#include <ftxui/dom/elements.hpp>

#include "Program.h"

using namespace sdb;
using namespace ftxui;

static Component runButton(Program* prog) {
    auto opt = ButtonOption::Border();
    opt.transform = [=](EntryState const& s) {
        Element elem = prog->running() ? text("||") : text("|>");
        return elem | border;
    };
    return Button(
        "Run",
        [=] { prog->toggleExecution(); },
        opt);
}

static Component skipButton(Program* prog) {
    auto opt = ButtonOption::Border();
    opt.transform = [=](EntryState const& s) { return text(">|") | border; };
    return Button(
        "Skip",
        [=] { prog->skipLine(); },
        opt);
}

static Component enterFunctionButton(Program* prog) {
    auto opt = ButtonOption::Border();
    opt.transform = [=](EntryState const& s) { return text("\\_") | border; };
    return Button(
        "Enter function",
        [=] { prog->enterFunction(); },
        opt);
}

static Component exitFunctionButton(Program* prog) {
    auto opt = ButtonOption::Border();
    opt.transform = [=](EntryState const& s) { return text("_/") | border; };
    return Button(
        "Exit function",
        [=] { prog->exitFunction(); },
        opt);
}

namespace {

struct CtrlView: ComponentBase {
    CtrlView(Program* prog): prog(prog) {
        Add(Container::Horizontal({
            runButton(prog),
            skipButton(prog),
            enterFunctionButton(prog),
            exitFunctionButton(prog),
        }));
    }

    Program* prog;
};

} // namespace

ftxui::Component sdb::ControlView(Program* prog) {
    return Make<CtrlView>(prog);
}
