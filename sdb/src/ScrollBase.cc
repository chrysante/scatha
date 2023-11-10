#include "ScrollBase.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <utl/utility.hpp>

using namespace sdb;
using namespace ftxui;

static int yExtend(Box box) {
    int diff = box.y_max - box.y_min;
    return std::max(0, diff);
}

Element ScrollBase::Render() {
    std::vector<Element> elems;
    size_t begin = utl::narrow_cast<size_t>(scrollPos);
    size_t end = ChildCount();
    for (size_t index = begin; index < end; ++index) {
        elems.push_back(ChildAt(index)->Render());
    }
    return vbox(std::move(elems)) | flex | reflect(box);
}

bool ScrollBase::OnEvent(Event event) {
    if (handleScroll(event)) {
        return true;
    }
    return ComponentBase::OnEvent(event);
}

void ScrollBase::setScroll(long value) {
    scrollPos = value;
    clampScroll();
}

void ScrollBase::setScrollOffset(long offset) {
    scrollPos += offset;
    clampScroll();
}

bool ScrollBase::isInView(size_t index) const {
    long i = static_cast<long>(index);
    return i >= scrollPos && i < scrollPos + box.y_max - 2;
}

void ScrollBase::center(size_t index) {
    setScroll(static_cast<long>(index) - box.y_max / 2);
}

bool ScrollBase::isScrollUp(Event event) const {
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

bool ScrollBase::isScrollDown(Event event) const {
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

bool ScrollBase::handleScroll(Event event) {
    if (isScrollUp(event)) {
        setScrollOffset(-1);
        return true;
    }
    if (isScrollDown(event)) {
        setScrollOffset(1);
        return true;
    }
    return false;
}

void ScrollBase::clampScroll() { scrollPos = std::clamp(scrollPos, 0l, max()); }

long ScrollBase::max() const {
    long const overscroll = 0;
    return static_cast<long>(ChildCount()) - yExtend(box) + overscroll;
}
