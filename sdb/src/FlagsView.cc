#include "Views.h"

#include <functional>

#include <ftxui/dom/elements.hpp>
#include <utl/strcat.hpp>

#include "Common.h"
#include "Model.h"

using namespace sdb;
using namespace ftxui;

Component sdb::FlagsView(Model* model) {
    return Renderer([model] {
        auto flags = model->virtualMachine().getCompareFlags();
        auto display = [](std::string name, bool cond) {
            return text(name) | color(cond ? Color::Green : Color::Red);
        };
        return vbox({
            text("Compare Flags") | underlined,
            display("Equal", flags.equal),
            display("NotEqual", !flags.equal),
            display("Less", flags.less),
            display("LessEq", flags.less || flags.equal),
            display("Greater", !flags.less && !flags.equal),
            display("GreaterEq", !flags.less),
        });
    });
}
