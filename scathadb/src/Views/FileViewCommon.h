#ifndef SDB_VIEWS_FILEVIEWCOMMON_H_
#define SDB_VIEWS_FILEVIEWCOMMON_H_

#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "Model/UIHandle.h" // For BreakState
#include "UI/Common.h"

namespace sdb {

struct LineInfo {
    long num;
    bool isFocused     : 1;
    bool hasBreakpoint : 1;
    BreakState state   : 3;
};

ftxui::Element breakpointIndicator(LineInfo line);

ftxui::Element lineNumber(LineInfo line);

ftxui::ElementDecorator lineMessageDecorator(std::string message);

template <typename Derived>
class FileViewBase: public ScrollBase {
public:
    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;
        if (event == Event::Special("Reload")) {
            asDerived().reload();
            return true;
        }
        if (handleScroll(event)) {
            return true;
        }
        if (event.is_mouse()) {
            return handleMouse(event);
        }
        return false;
    }

    bool handleMouse(ftxui::Event event) {
        using namespace ftxui;
        auto mouse = event.mouse();
        if (!box().Contain(mouse.x, mouse.y)) {
            return false;
        }
        if (mouse.button != Mouse::None && mouse.motion == Mouse::Pressed) {
            TakeFocus();
        }
        if (mouse.button == Mouse::Left && mouse.motion == Mouse::Pressed) {
            long line = event.mouse().y - box().y_min + scrollPosition();
            long column = event.mouse().x - box().x_min;
            auto index = asDerived().lineToIndex(line);
            if (index && column < 8) {
                asDerived().toggleBreakpoint(*index);
            }
            else {
                setFocusLine(event.mouse().y - box().y_min + scrollPosition());
            }
            return true;
        }
        return false;
    }

private:
    auto& asDerived() { return static_cast<Derived&>(*this); }
};

} // namespace sdb

#endif // SDB_VIEWS_FILEVIEWCOMMON_H_
