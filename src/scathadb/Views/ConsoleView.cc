#include "Views/Views.h"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

#include "Model/Model.h"
#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

static std::vector<std::string> lines(std::string_view text) {
    return ranges::views::split(text, '\n') |
           ranges::views::transform([](auto const& rng) {
        return rng | ranges::to<std::string>;
    }) | ranges::to<std::vector>;
}

static size_t computeHash(std::string_view text) {
    return std::hash<std::string_view>{}(text);
}

namespace {

struct ConsoleViewImpl: ScrollBase {
    std::stringstream& out;
    std::vector<std::string> lines;
    size_t lastHash = 0;

    ConsoleViewImpl(Model* model): out(model->standardout()) {}

    Element Render() override {
        auto consoleText = out.str();
        size_t hash = computeHash(consoleText);
        if (hash != lastHash) {
            lastHash = hash;
            lines = ::lines(consoleText);
            /// Compute before detaching children
            bool isMaxScroll = scrollPosition() == maxScrollPositition();
            DetachAllChildren();
            for (auto& line: lines) {
                Add(Renderer([=] { return text(line); }));
            }
            if (isMaxScroll) {
                setScroll(999999999);
            }
        }
        return ScrollBase::Render();
    }
};

} // namespace

ftxui::Component sdb::ConsoleView(Model* model) {
    return Make<ConsoleViewImpl>(model);
}
