#include "Views.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "Common.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

static std::vector<std::string> splitWords(std::string_view text) {
    return ranges::views::split(text, ' ') |
           ranges::views::transform([](auto const& word) {
               return word | ranges::to<std::string>;
           }) |
           ranges::to<std::vector>;
}

/// https://stackoverflow.com/a/12737930/21285803
static bool isHidden(std::filesystem::path const& path) {
    auto name = path.filename().string();
    return name.front() == '.' && name != ".." && name != ".";
}

namespace {

class AutoCompleter {
public:
    void operator()(std::string& input, int& cursor) {
        if (!valid) {
            buildStructure(input);
        }
        if (matches.empty()) {
            beep();
            return;
        }
        if (matches.size() == 1 && matchIndex == 1) {
            beep();
            invalidate();
            return;
        }
        matchIndex %= matches.size();
        auto completed = parent / matches[matchIndex++];
        if (std::filesystem::is_directory(completed)) {
            completed /= {};
        }
        input = completed;
        cursor = static_cast<int>(input.size());
    }

    void invalidate() { valid = false; }

private:
    void buildStructure(std::string input) {
        valid = true;
        base = input;
        std::filesystem::path inputPath = input;
        parent = inputPath.parent_path();
        std::string name = inputPath.filename().string();
        matches.clear();
        for (auto& entry: std::filesystem::directory_iterator(
                 std::filesystem::absolute(parent)))
        {
            if (isHidden(entry.path())) {
                continue;
            }
            auto filename = entry.path().filename().string();
            if (!filename.starts_with(name)) {
                continue;
            }
            matches.push_back(filename);
        }
        ranges::sort(matches);
        matchIndex = 0;
    }

    std::string base;
    bool valid = false;
    size_t matchIndex = 0;
    std::vector<std::string> matches;
    std::filesystem::path parent;
};

struct OpenFilePanelBase: ComponentBase {
    OpenFilePanelBase(Model* model, bool* open) {
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
        opt.on_change = [=] {
            autoComplete.invalidate();
            hideErrorMessage();
        };
        opt.on_enter = [=] {
            if (content.back() == '\n') {
                content.pop_back();
            }
            auto args = splitWords(content);
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
                displayErrorMessage(e.what());
            }
        };
        opt.cursor_position = &cursor;
        auto input = Input(opt);
        input |= CatchEvent([this](Event event) {
            if (event == Event::Tab) {
                autoComplete(content, cursor);
                return true;
            }
            if (event == Event::ArrowLeft || event == Event::ArrowRight) {
                autoComplete.invalidate();
                /// We return false because the input box shall still catch this
                /// event
                return false;
            }
            return false;
        });
        Add(input);
    }

    Element Render() override {
        std::vector<Element> elems;
        for (size_t i = 0; i < ChildCount(); ++i) {
            if (i > 0) {
                elems.push_back(sdb::separator()->Render());
            }
            elems.push_back(ChildAt(i)->Render());
        }
        return vbox(std::move(elems));
    }

    Component ActiveChild() override { return ChildAt(0); }

    void displayErrorMessage(std::string msg) {
        hideErrorMessage();
        Add(Renderer([msg] { return text(msg) | color(Color::Red); }));
    }

    void hideErrorMessage() {
        assert(ChildCount() <= 2);
        if (ChildCount() == 2) {
            ChildAt(1)->Detach();
        }
    }

    int cursor = 0;
    std::string content, placeholder = "executable-path";
    AutoCompleter autoComplete;
};

} // namespace

ModalView sdb::OpenFilePanel(Model* model) {
    auto state = ModalView::makeState();
    return ModalView("Open...",
                     Make<OpenFilePanelBase>(model, &state->open),
                     state);
}
