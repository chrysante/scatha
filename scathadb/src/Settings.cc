#include "Views.h"

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

using namespace sdb;
using namespace ftxui;

Component sdb::SettingsView(std::function<void()> hideSettings) {
    auto opt = ButtonOption::Simple();
    opt.transform = [](EntryState const&) { return text("X"); };
    auto closeButton = Button("X", hideSettings, opt);
    auto titlebar = Container::Horizontal(
        { closeButton, Renderer([] { return separator(); }), Renderer([] {
              return text("Settings") | bold | center | flex;
          }) });
    auto component = Container::Vertical({
        titlebar,
        Renderer([] { return separator(); }),
        Renderer([] { return text("Nothing to see here yet"); }),
    });
    component |= Renderer([&](Element inner) {
        return inner | size(WIDTH, GREATER_THAN, 30) | border;
    });
    component |= CatchEvent([=](Event event) {
        if (event == Event::Escape) {
            hideSettings();
            return true;
        }
        return false;
    });
    return component;
}
