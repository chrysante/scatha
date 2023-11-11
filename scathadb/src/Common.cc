#include "Common.h"

#include <iostream>

using namespace sdb;
using namespace ftxui;

static auto defaultSep() { return ftxui::separator() | color(Color::GrayDark); }

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

void sdb::beep() { std::cout << "\007"; }
