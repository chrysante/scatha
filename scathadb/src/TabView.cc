#include "Views.h"

#include <string>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "Common.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct TabViewBase: ComponentBase {
    TabViewBase(std::vector<NamedComponent> children) {
        auto names = children |
                     ranges::views::transform(&NamedComponent::name) |
                     ranges::to<std::vector>;
        auto bodies = children |
                      ranges::views::transform(&NamedComponent::component) |
                      ranges::to<std::vector>;
        auto toTabButton = [&](auto p) {
            int index = utl::narrow_cast<int>(p.first);
            auto name = p.second;
            ButtonOption opt;
            opt.transform = [=](EntryState const&) {
                auto elem = text(name) | bold;
                if (index == selector) {
                    elem |= color(Color::BlueLight);
                }
                else {
                    elem |= dim;
                }
                return elem;
            };
            opt.on_click = [=] { selector = index; };
            return Button(opt);
        };
        auto tabBar = Toolbar(names | ranges::views::enumerate |
                              ranges::views::transform(toTabButton) |
                              ranges::to<std::vector>);
        auto body = Container::Tab(std::move(bodies), &selector);
        auto main = Container::Vertical({
            tabBar,
            sdb::separator(),
            body | flex,
        });
        Add(main);
    }

    int selector = 0;
};

} // namespace

Component sdb::TabView(std::vector<NamedComponent> children) {
    return Make<TabViewBase>(std::move(children));
}
