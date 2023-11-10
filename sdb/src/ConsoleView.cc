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
    std::stringstream sstr;

    ConsoleViewImpl() {
        sstr << "Hello World!\n";
        sstr << 23 << "\n";
        sstr << 123 << std::endl;
        sstr << 64 << "\n";
        sstr << "Goodbye.\n";

        Add(Renderer([this] {
            return vbox(lines(sstr.str()) |
                        ranges::views::transform(
                            [](auto line) { return hflow(paragraph(line)); }) |
                        ranges::to<std::vector>);
        }));
    }
};

} // namespace

ftxui::Component sdb::ConsoleView(Model* model) {
    return Make<ConsoleViewImpl>();
}
