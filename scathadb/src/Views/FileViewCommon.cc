#include "Views/FileViewCommon.h"

using namespace sdb;
using namespace ftxui;

Element sdb::breakpointIndicator(LineInfo line) {
    if (!line.hasBreakpoint) {
        return text("   ");
    }
    return text("-> ") |
           color(line.state != BreakState::None ? Color::White :
                                                  Color::BlueLight) |
           bold;
}

Element sdb::lineNumber(LineInfo line) {
    return text(std::to_string(line.num + 1) + " ") | align_right |
           size(WIDTH, EQUAL, 5) |
           color(line.state != BreakState::None ? Color::White :
                                                  Color::GrayDark);
}

ElementDecorator sdb::lineMessageDecorator(std::string message) {
    return [=](Element elem) {
        return hbox(
            { elem, filler(),
              hflow(paragraphAlignRight(std::move(message))) | yflex_grow });
    };
}
