#include "Views.h"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

#include "Model.h"

using namespace sdb;
using namespace ftxui;

auto lines(std::string_view text) {
    return ranges::views::split(text, '\n') |
           ranges::views::transform(
               [](auto const& rng) { return rng | ranges::to<std::string>; });
}

namespace {

struct ConsoleViewImpl: ComponentBase {
    std::stringstream& standardout;

    ConsoleViewImpl(std::stringstream& out): standardout(out) {
        Add(Renderer([this] {
            return vbox(lines(standardout.str()) |
                        ranges::views::transform(
                            [](auto line) { return hflow(paragraph(line)); }) |
                        ranges::to<std::vector>);
        }));
    }
};

} // namespace

ftxui::Component sdb::ConsoleView(Model* model,
                                  std::stringstream& standardout) {
    return Make<ConsoleViewImpl>(standardout);
}
