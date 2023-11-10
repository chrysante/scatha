#include "Views.h"

#include <functional>

#include <ftxui/dom/elements.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Common.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

Component sdb::FlagsView(Model* model) {
    return Renderer([model] {
        auto flags = model->virtualMachine().getCompareFlags();
        auto display = [](std::string name, bool cond) {
            return text(name) | bold | color(cond ? Color::Green : Color::Red) |
                   center |
                   size(WIDTH, EQUAL, utl::narrow_cast<int>(name.size() + 2));
        };
        return hbox({
                   display("==", flags.equal),
                   display("!=", !flags.equal),
                   display("<", flags.less),
                   display("<=", flags.less || flags.equal),
                   display(">", !flags.less && !flags.equal),
                   display(">=", !flags.less),
               }) |
               center;
    });
}
