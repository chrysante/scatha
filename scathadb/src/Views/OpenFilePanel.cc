#include "Views/Views.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "Model/Model.h"
#include "Model/Options.h"
#include "UI/Common.h"

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

static std::string expandTilde(std::string input) {
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] != '~') {
            continue;
        }
        char const* home = std::getenv("HOME");
        if (!home) {
            break;
        }
        std::string Home(home);
        input.erase(input.begin() + static_cast<long>(i));
        input.insert(i, Home);
        i += Home.size() - 1; // -1 to account for ++i;
    }
    return input;
}

namespace {

class AutoCompleter {
public:
    std::vector<std::string> operator()(std::string& input, int& cursor) {
        if (!valid) {
            buildStructure(input);
        }
        if (matches.empty()) {
            beep();
            return {};
        }
        /// `matchIndex == 1` here means we already inserted the completion
        if (matches.size() == 1 && matchIndex == 1) {
            beep();
            invalidate();
            return {};
        }
        /// On the first hit we display suggestions if there is more than one
        /// option
        if (matches.size() > 1 && !hitBefore) {
            hitBefore = true;
            return matches;
        }
        /// Then we cycle through the suggestions
        matchIndex %= matches.size();
        auto completed = parent / matches[matchIndex++];
        if (std::filesystem::is_directory(completed)) {
            completed /= {};
        }
        input = completed;
        cursor = static_cast<int>(input.size());
        return {};
    }

    void invalidate() {
        valid = false;
        hitBefore = false;
    }

private:
    void buildStructure(std::string input) {
        valid = true;
        base = input;
        matchIndex = 0;
        matches.clear();
        auto [relParent, absParent, name] = [&] {
            auto rel = std::filesystem::path(input);
            auto abs = std::filesystem::absolute(expandTilde(input));
            if (std::filesystem::exists(abs) &&
                std::filesystem::is_directory(abs))
            {
                return std::tuple{ rel, abs, std::string{} };
            }
            return std::tuple{ rel.parent_path(),
                               abs.parent_path(),
                               abs.filename().string() };
        }();
        baseName = name;
        parent = relParent;
        for (auto& entry: std::filesystem::directory_iterator(absParent)) {
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
    }

    std::string base;
    std::string baseName;
    bool valid = false;
    bool hitBefore = false;
    size_t matchIndex = 0;
    std::vector<std::string> matches;
    std::filesystem::path parent;
};

struct OpenFilePanelBase: ComponentBase {
    enum MessageKind { DEFAULT, ERROR };

    OpenFilePanelBase(Model* model, bool* open) {
        InputOption opt;
        opt.content = &content;
        opt.placeholder = &placeholder;
        opt.transform = [](InputState state) {
            auto elem = state.element;
            if (state.is_placeholder) {
                elem |= dim;
            }
            return elem;
        };
        opt.on_change = [=] {
            autoComplete.invalidate();
            hideMessage();
        };
        opt.on_enter = [=] {
            if (content.back() == '\n') {
                content.pop_back();
            }
            auto args = splitWords(expandTilde(content));
            auto options = parseArguments(args);
            try {
                model->loadBinary(options);
                *open = false;
            }
            catch (std::exception const& e) {
                displayMessage({ e.what() }, ERROR);
            }
        };
        opt.cursor_position = &cursor;
        auto input = Input(opt);
        input |= CatchEvent([this](Event event) {
            if (event == Event::Tab) {
                std::vector<std::string> suggestions =
                    autoComplete(content, cursor);
                if (!suggestions.empty()) {
                    displayMessage(std::move(suggestions));
                }
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
                elems.push_back(sdb::separator());
            }
            elems.push_back(ChildAt(i)->Render());
        }
        return vbox(std::move(elems));
    }

    Component ActiveChild() override { return ChildAt(0); }

    void displayMessage(std::vector<std::string> msg,
                        MessageKind kind = DEFAULT) {
        hideMessage();
        Add(Renderer([=] {
            auto t = vbox(msg | ranges::views::transform([](auto const& str) {
                              return text(str);
                          }) |
                          ranges::to<std::vector>);
            switch (kind) {
            case DEFAULT:
                t |= dim;
                break;
            case ERROR:
                t |= color(Color::Red);
                break;
            }
            return t;
        }));
    }

    void hideMessage() {
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
    auto state = ModalState::make();
    return ModalView("Open file",
                     Make<OpenFilePanelBase>(model, &state->open),
                     { .state = state, .closeButton = false });
}
