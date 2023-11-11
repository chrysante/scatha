#include "Views.h"

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

#include "Common.h"

using namespace sdb;
using namespace ftxui;

Component sdb::Toolbar(std::vector<Component> components) {
    return Container::Horizontal(std::move(components));
}
