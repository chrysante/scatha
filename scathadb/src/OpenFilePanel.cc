#include "Views.h"

#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

#include "Common.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

namespace {

static std::vector<std::string> splitArguments(std::string_view text) {
    return ranges::views::split(text, ' ') |
           ranges::views::transform([](auto const& word) {
               return word | ranges::to<std::string>;
           }) |
           ranges::to<std::vector>;
}

struct Panel: ComponentBase {
    Panel(Model* model, bool* open) {
        InputOption opt;
        opt.content = &content;
        opt.placeholder = &placeholder;
        opt.transform = [](InputState state) {
            auto elem = state.element;
            if (state.is_placeholder) {
                elem |= color(Color::GrayDark);
            }
            return elem;
        };
        opt.on_enter = [=] {
            if (content.back() == '\n') {
                content.pop_back();
            }
            auto args = splitArguments(content);
            auto cStyleArgs = args |
                              ranges::views::transform(
                                  [](std::string& str) { return str.data(); }) |
                              ranges::to<std::vector>;
            auto options = parseArguments(cStyleArgs);
            try {
                model->loadBinary(options);
                *open = false;
            }
            catch (std::exception const& e) {
                content.clear();
                placeholder = e.what();
            }
        };
        Add(Input(opt));
    }

    std::string content, placeholder = "executable-path";
};

} // namespace

Component sdb::OpenFilePanel(Model* model, bool* open) {
    return ModalView("Open...", Make<Panel>(model, open), open);
}
