#include "UI/Common.h"

#include <iostream>

#include <range/v3/view.hpp>
#include <utl/utility.hpp>

using namespace sdb;
using namespace ftxui;

int sdb::xExtend(Box box) {
    int diff = box.x_max - box.x_min;
    return std::max(0, diff);
}

int sdb::yExtend(Box box) {
    int diff = box.y_max - box.y_min;
    return std::max(0, diff);
}

static Element defaultSep() { return ftxui::separator() | dim; }

Element sdb::separator() { return defaultSep(); }

Element sdb::separatorBlank() { return ftxui::separatorEmpty(); }

Element sdb::spacer() { return ftxui::filler(); }

Element sdb::placeholder(std::string message) {
    return text(message) | bold | dim | center;
}

Component sdb::Separator() {
    return Renderer([] { return separator(); });
}

Component sdb::SeparatorBlank() {
    return Renderer([] { return separatorBlank(); });
}

Component sdb::Spacer() {
    return Renderer([] { return spacer(); });
}

Component sdb::Placeholder(std::string message) {
    return Renderer([=] { return placeholder(message); });
}

Component sdb::SplitLeft(Component main, Component back, ftxui::Ref<int> size) {
    return ResizableSplit(
        { main, back, Direction::Left, size, [] { return defaultSep(); } });
}

Component sdb::SplitRight(Component main,
                          Component back,
                          ftxui::Ref<int> size) {
    return ResizableSplit(
        { main, back, Direction::Right, size, [] { return defaultSep(); } });
}

Component sdb::SplitTop(Component main, Component back, ftxui::Ref<int> size) {
    return ResizableSplit(
        { main, back, Direction::Up, size, [] { return defaultSep(); } });
}

Component sdb::SplitBottom(Component main,
                           Component back,
                           ftxui::Ref<int> size) {
    return ResizableSplit(
        { main, back, Direction::Down, size, [] { return defaultSep(); } });
}

namespace {

struct ToolbarBase: ComponentBase {
    explicit ToolbarBase(std::vector<Component> components,
                         ToolbarOptions options):
        options(options) {
        for (auto& comp: components) {
            Add(comp);
        }
    }

    Element Render() override {
        std::vector<Element> elems;
        for (size_t i = 0; i < ChildCount(); ++i) {
            if (i > 0 && options.separator) {
                elems.push_back(options.separator());
            }
            elems.push_back(ChildAt(i)->Render());
        }
        return hbox(std::move(elems));
    }

    ToolbarOptions options;
};

} // namespace

Component sdb::Toolbar(std::vector<Component> components,
                       ToolbarOptions options) {
    return Make<ToolbarBase>(std::move(components), options);
}

namespace {

struct TabViewBase: ComponentBase {
    TabViewBase(std::vector<NamedComponent> children) {
        auto names = children |
                     ranges::views::transform(&NamedComponent::name) |
                     ranges::to<std::vector>;
        auto bodies = children |
                      ranges::views::transform(&NamedComponent::component) |
                      ranges::to<std::vector>;
        auto toTabButton = [&](auto p) {
            int index = utl::narrow_cast<int>(p.first);
            auto name = p.second;
            ButtonOption opt;
            opt.transform = [=](EntryState const&) {
                auto elem = text(name) | bold;
                if (index == selector) {
                    elem |= color(Color::BlueLight);
                }
                else {
                    elem |= dim;
                }
                return elem;
            };
            opt.on_click = [=] { selector = index; };
            return Button(opt);
        };
        auto tabs = names | ranges::views::enumerate |
                    ranges::views::transform(toTabButton) |
                    ranges::to<std::vector>;
        tabs.insert(tabs.begin(), Spacer());
        tabs.push_back(Spacer());
        ToolbarOptions toolbarOpt{ .separator = [] { return spacer(); } };
        auto tabBar = Toolbar(std::move(tabs), toolbarOpt);
        auto body = Container::Tab(std::move(bodies), &selector);
        auto main = Container::Vertical({
            tabBar,
            sdb::Separator(),
            body | flex,
        });
        Add(main);
    }

    int selector = 0;
};

} // namespace

Component sdb::TabView(std::vector<NamedComponent> children) {
    return Make<TabViewBase>(std::move(children));
}

Element ScrollBase::Render() {
    if (_box != _lastBox) {
        clampScroll();
        _lastBox = _box;
    }
    std::vector<Element> elems;
    size_t begin = utl::narrow_cast<size_t>(scrollPos.load());
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

bool ScrollBase::isInView(long line) const {
    return line >= scrollPos && line <= scrollPos + yExtend(_box);
}

void ScrollBase::center(long line, double ratio) {
    ratio = std::clamp(ratio, 0.0, 1.0);
    setScroll(line - static_cast<long>(yExtend(_box) * ratio));
}

bool ScrollBase::isScrollUp(Event event, bool allowKeyScroll) const {
    if (event.is_mouse() && event.mouse().motion == Mouse::Pressed &&
        event.mouse().button == Mouse::WheelUp)
    {
        return _box.Contain(event.mouse().x, event.mouse().y);
    }
    if (allowKeyScroll && event == Event::ArrowUp) {
        return true;
    }
    return false;
}

bool ScrollBase::isScrollDown(Event event, bool allowKeyScroll) const {
    if (event.is_mouse() && event.mouse().motion == Mouse::Pressed &&
        event.mouse().button == Mouse::WheelDown)
    {
        return _box.Contain(event.mouse().x, event.mouse().y);
    }
    if (allowKeyScroll && event == Event::ArrowDown) {
        return true;
    }
    return false;
}

bool ScrollBase::handleScroll(Event event, bool allowKeyScroll) {
    if (isScrollUp(event, allowKeyScroll)) {
        setScrollOffset(-1);
        return true;
    }
    if (isScrollDown(event, allowKeyScroll)) {
        setScrollOffset(1);
        return true;
    }
    return false;
}

void ScrollBase::clampScroll() {
    scrollPos = std::clamp(scrollPos.load(), long{ 0 }, maxScrollPositition());
}

long ScrollBase::maxScrollPositition() const {
    long const overscroll = 0;
    return std::max(long{ 0 },
                    static_cast<long>(ChildCount()) - yExtend(_box) +
                        overscroll);
}

void sdb::beep() { std::cout << "\007"; }
