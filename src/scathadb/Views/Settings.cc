#include "Views/Views.h"

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

#include "UI/Common.h"

using namespace sdb;
using namespace ftxui;

ModalView sdb::SettingsView() {
    return ModalView("Settings",
                     Renderer([] { return placeholder("Not Implemented"); }));
}
