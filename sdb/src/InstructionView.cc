#include "Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "Program.h"

using namespace sdb;
using namespace ftxui;

static Element breakpointIndicator(bool isBreakpoint) {
    if (isBreakpoint) {
        return text("> ") | color(Color::BlueLight) | bold;
    }
    else {
        return text("  ");
    }
}

static Element lineNumber(size_t index, bool isCurrent) {
    return text(std::to_string(index + 1) + " ") | align_right |
           size(WIDTH, EQUAL, 5) |
           color(isCurrent ? Color::White : Color::GrayLight);
}

namespace {

struct InstView: ComponentBase {
    InstView(Program* prog): prog(prog) {
        for (auto [index, inst]:
             prog->instructions() | ranges::views::enumerate)
        {
            ButtonOption opt = ButtonOption::Ascii();
            opt.transform = [=, index = index](EntryState const& s) {
                bool isCurrent = index == prog->currentLine();
                bool isBreakpoint = prog->isBreakpoint(index);
                auto label = hbox({ lineNumber(index, isCurrent),
                                    text(prog->instructions()[index].text) }) |
                             flex;
                if (isCurrent) {
                    label |= bgcolor(Color::Green);
                }
                return hbox({ breakpointIndicator(isBreakpoint), label });
            };
            opt.on_click = [=, index = index] {
                prog->toggleBreakpoint(index);
            };
            Add(Button(opt));
        }
    }

    Element Render() override {
        std::vector<Element> elems;
        for (size_t index = utl::narrow_cast<size_t>(scrollPos);
             index < ChildCount();
             ++index)
        {
            elems.push_back(ChildAt(index)->Render());
        }
        return vbox(std::move(elems)) | reflect(box);
    }

    bool OnEvent(Event event) override {
        if (handleScroll(event)) {
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

    bool isScrollUp(Event event) {
        if (event.is_mouse() && event.mouse().motion == Mouse::Pressed &&
            event.mouse().button == Mouse::WheelUp)
        {
            return true;
        }
        if (event == Event::ArrowUp) {
            return true;
        }
        return false;
    }

    bool isScrollDown(Event event) {
        if (event.is_mouse() && event.mouse().motion == Mouse::Pressed &&
            event.mouse().button == Mouse::WheelDown)
        {
            return true;
        }
        if (event == Event::ArrowDown) {
            return true;
        }
        return false;
    }

    bool handleScroll(Event event) {
        if (isScrollUp(event)) {
            if (scrollPos != 0) {
                --scrollPos;
            }
            return true;
        }
        if (isScrollDown(event)) {
            if (scrollPos < static_cast<int>(ChildCount()) - box.y_max) {
                ++scrollPos;
            }
            return true;
        }
        return false;
    }

    Program* prog;
    int scrollPos = 0;
    Box box;
};

} // namespace

ftxui::Component sdb::InstructionView(Program* prog) {
    return Make<InstView>(prog);
}
