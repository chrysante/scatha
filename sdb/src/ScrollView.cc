#include "Views.h"

#include <vector>

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>
#include <utl/utility.hpp>

using namespace sdb;
using namespace ftxui;

namespace {

struct Impl: ComponentBase {
    Impl(Component child) { Add(child); }

    Element Render() override {
        auto child = ChildAt(0);
        std::vector<Element> elems;
        for (size_t index = utl::narrow_cast<size_t>(scrollPos);
             index < child->ChildCount();
             ++index)
        {
            elems.push_back(child->ChildAt(index)->Render());
        }
        return vbox(std::move(elems)) | reflect(box);
    }

    bool OnEvent(Event event) override {
        if (handleScroll(event)) {
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

    bool isScrollUp(Event event) const {
        if (event.is_mouse() && event.mouse().motion == Mouse::Pressed &&
            event.mouse().button == Mouse::WheelUp)
        {
            return box.Contain(event.mouse().x, event.mouse().y);
        }
        if (event == Event::ArrowUp) {
            return true;
        }
        return false;
    }

    bool isScrollDown(Event event) const {
        if (event.is_mouse() && event.mouse().motion == Mouse::Pressed &&
            event.mouse().button == Mouse::WheelDown)
        {
            return box.Contain(event.mouse().x, event.mouse().y);
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
            int const overscroll = 20;
            if (scrollPos < static_cast<int>(ChildAt(0)->ChildCount()) -
                                box.y_max + overscroll)
            {
                ++scrollPos;
            }
            return true;
        }
        return false;
    }

    int scrollPos = 0;
    Box box;
};

} // namespace

Component sdb::ScrollView(ftxui::Component child) { return Make<Impl>(child); }
