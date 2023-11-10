#include "Common.h"

#include <iostream>

using namespace sdb;
using namespace ftxui;

Component sdb::separator() {
    return Renderer([] { return ftxui::separator(); });
}

Component sdb::separatorEmpty() {
    return Renderer([] { return ftxui::separatorEmpty(); });
}

Component sdb::spacer() {
    return Renderer([] { return ftxui::filler(); });
}

void sdb::beep() { std::cout << "\007"; }
