#include "Views/Views.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "Model/Model.h"
#include "Model/Options.h"
#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

static std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> result;
    enum State { Default, StringLit };
    State state = Default;
    size_t beginIdx = 0;
    size_t currIdx = 0;
    auto emitToken = [&] {
        size_t count = currIdx - beginIdx;
        if (count > 0) {
            result.push_back(std::string(text.substr(beginIdx, count)));
        }
        beginIdx = currIdx + 1;
    };
    while (true) {
        if (currIdx >= text.size()) {
            emitToken();
            break;
        }
        switch (state) {
        case Default:
            if (text[currIdx] == '\"') {
                emitToken();
                state = StringLit;
                break;
            }
            if (!std::isspace(text[currIdx])) {
                break;
            }
            emitToken();
            break;
        case StringLit:
            if (text[currIdx] == '\"') {
                emitToken();
                state = Default;
                break;
            }
            break;
        }
        ++currIdx;
    }
    return result;
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

struct SuggestionResult {
    std::string commonBegin;
    std::vector<std::filesystem::path> matches;
    std::optional<ssize_t> current = std::nullopt;
};

class AutoCompleter {
public:
    bool next(std::string& input,
              int& cursor,
              SuggestionResult& suggestion,
              int offset) {
        if (!valid) {
            buildStructure(input);
        }
        if (matches.empty()) {
            beep();
            return false;
        }
        /// Having hit before means we already inserted the single completion
        if (matches.size() == 1 && hitBefore) {
            beep();
            invalidate();
            return false;
        }
        /// On the first hit we display suggestions if there is more than one
        /// option
        if (matches.size() > 1 && !hitBefore) {
            hitBefore = true;
            fillCommon(input, cursor);
            suggestion = { baseName, matches };
            return true;
        }
        /// Then we cycle through the suggestions
        matchIndex += offset;
        matchIndex %= matches.size();
        suggestion.current = matchIndex;
        auto completed = parent / matches[size_t(matchIndex)];
        input = completed;
        cursor = static_cast<int>(input.size());
        hitBefore = true;
        return false;
    }

    void fillCommon(std::string& input, int& cursor) {
        if (matches.empty()) {
            return;
        }
        for (size_t index = baseName.size();; ++index) {
            char ref = matches[0].string()[index];
            for (auto& match: matches) {
                if (index >= match.string().size()) {
                    return;
                }
                if (ref != match.string()[index]) {
                    return;
                }
            }
            input.push_back(ref);
            baseName.push_back(ref);
            ++cursor;
        }
    }

    void invalidate() {
        valid = false;
        hitBefore = false;
    }

private:
    void buildStructure(std::string input) {
        valid = true;
        base = input;
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
            auto filename = entry.path().filename();
            if (!filename.string().starts_with(name)) {
                continue;
            }
            if (std::filesystem::is_directory(entry.path())) {
                filename /= {};
            }
            matches.push_back(filename);
        }
        matchIndex = ssize_t(matches.size()) - 1;
        ranges::sort(matches);
    }

    std::string base;
    std::string baseName;
    bool valid = false;
    bool hitBefore = false;
    ssize_t matchIndex = 0;
    std::vector<std::filesystem::path> matches;
    std::filesystem::path parent;
};

struct SuggestionView: ScrollBase {
    SuggestionView(SuggestionResult* suggestion): suggestion(suggestion) {
        for (auto [index, match]:
             suggestion->matches | ranges::views::enumerate)
        {
            Add(Renderer([=, index = index, match = match] {
                auto begin = text(suggestion->commonBegin);
                auto end = text(
                    match.string().erase(0, suggestion->commonBegin.size()));
                if (!suggestion->current ||
                    (ssize_t)index != *suggestion->current)
                {
                    end |= dim;
                }
                return hbox({ begin, end });
            }));
        }
    }

    Element Render() override {
        if (suggestion->current) {
            center(utl::narrow_cast<long>(*suggestion->current));
        }
        return ScrollBase::Render();
    }

    SuggestionResult* suggestion;
};

struct OpenFilePanelBase: ComponentBase {
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
            size_t newlinePos = utl::narrow_cast<size_t>(cursor) - 1;
            assert(content[newlinePos] == '\n');
            content.erase(newlinePos, 1);
            --cursor;
            if (std::filesystem::is_directory(content)) {
                autoComplete.invalidate();
                hideMessage();
                return;
            }
            auto args = tokenize(expandTilde(content));
            auto options = parseArguments(args);
            try {
                model->loadProgram(options.filepath);
                model->setArguments(options.arguments);
                *open = false;
            }
            catch (std::exception const& e) {
                displayError(e.what());
            }
        };
        opt.cursor_position = &cursor;
        auto input = Input(opt);
        input |= CatchEvent([this](Event event) {
            if (event == Event::Tab || event == Event::ArrowDown) {
                cycle();
                return true;
            }
            if (event == Event::TabReverse || event == Event::ArrowUp) {
                cycleBack();
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

    void cycle() {
        if (autoComplete.next(content, cursor, suggestion, 1)) {
            displaySuggestions();
        }
    }

    void cycleBack() {
        if (autoComplete.next(content, cursor, suggestion, -1)) {
            displaySuggestions();
        }
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

    void displayError(std::string msg) {
        displayMessage(
            Renderer([=]() { return text(msg) | color(Color::Red); }));
    }

    void displaySuggestions() {
        displayMessage(Make<SuggestionView>(&suggestion) | flex);
    }

    void displayMessage(Component comp) {
        hideMessage();
        Add(comp);
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
    SuggestionResult suggestion;
};

} // namespace

ModalView sdb::OpenFilePanel(Model* model) {
    auto state = ModalState::make();
    return ModalView("Open file",
                     Make<OpenFilePanelBase>(model, &state->open),
                     { .state = state, .closeButton = false });
}
