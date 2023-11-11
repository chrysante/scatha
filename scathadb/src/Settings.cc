#include "Views.h"

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

#include "Common.h"

using namespace sdb;
using namespace ftxui;

Component sdb::SettingsView(bool* open) {
    return ModalView("Settings",
                     Renderer([] { return text("Nothing to see here yet"); }),
                     open);
}
