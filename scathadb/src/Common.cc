#include "Common.h"

#include <iostream>

#include <range/v3/view.hpp>
#include <utl/utility.hpp>

using namespace sdb;
using namespace ftxui;

static Element defaultSep() {
    return ftxui::separator() | color(Color::GrayDark);
}

Component sdb::separator() {
    return Renderer([] { return defaultSep(); });
}

Component sdb::separatorEmpty() {
    return Renderer([] { return ftxui::separatorEmpty(); });
}

Component sdb::spacer() {
    return Renderer([] { return ftxui::filler(); });
}

Component sdb::splitLeft(Component main, Component back, int size) {
    return ResizableSplit(
        { main, back, Direction::Left, size, [] { return defaultSep(); } });
}

Component sdb::splitRight(Component main, Component back, int size) {
    return ResizableSplit(
        { main, back, Direction::Right, size, [] { return defaultSep(); } });
}

Component sdb::splitTop(Component main, Component back, int size) {
    return ResizableSplit(
        { main, back, Direction::Up, size, [] { return defaultSep(); } });
}

Component sdb::splitBottom(Component main, Component back, int size) {
    return ResizableSplit(
        { main, back, Direction::Down, size, [] { return defaultSep(); } });
}

static int yExtend(Box box) {
    int diff = box.y_max - box.y_min;
    return std::max(0, diff);
}

Element ScrollBase::Render() {
    if (_box != _lastBox) {
        clampScroll();
        _lastBox = _box;
    }
    std::vector<Element> elems;
    size_t begin = utl::narrow_cast<size_t>(scrollPos);
    size_t end = ChildCount();
    for (size_t index = 0; index < begin; ++index) {
        ChildAt(index)->Render();
    }
    for (size_t index = begin; index < end; ++index) {
        elems.push_back(ChildAt(index)->Render());
    }
    return vbox(std::move(elems)) | flex | reflect(_box);
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
    return i >= scrollPos && i < scrollPos + _box.y_max - 2;
}

void ScrollBase::center(size_t index) {
    setScroll(static_cast<long>(index) - _box.y_max / 2);
}

bool ScrollBase::isScrollUp(Event event) const {
    if (event.is_mouse() && event.mouse().motion == Mouse::Pressed &&
        event.mouse().button == Mouse::WheelUp)
    {
        return _box.Contain(event.mouse().x, event.mouse().y);
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
        return _box.Contain(event.mouse().x, event.mouse().y);
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

void ScrollBase::clampScroll() {
    scrollPos = std::clamp(scrollPos, long{ 0 }, maxScrollPositition());
}

long ScrollBase::maxScrollPositition() const {
    long const overscroll = 0;
    return std::max(long{ 0 },
                    static_cast<long>(ChildCount()) - yExtend(_box) +
                        overscroll);
}

void sdb::beep() { std::cout << "\007"; }

Options sdb::parseArguments(std::span<char*> args) {
    if (args.empty()) {
        return {};
    }
    Options options{};
    options.filepath = args.front();
    for (auto* arg: args | ranges::views::drop(1)) {
        options.arguments.push_back(std::string(arg));
    }
    return options;
}
