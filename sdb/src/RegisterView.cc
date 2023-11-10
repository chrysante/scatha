#include "Views.h"

#include <sstream>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model.h"
#include "ScrollBase.h"

using namespace sdb;
using namespace ftxui;

using TableEntry = std::vector<Element>;

namespace {

struct RegEntry: ComponentBase {
    explicit RegEntry(Model* model, intptr_t index):
        model(model), index(index) {}

    Element Render() override {
        auto& vm = model->virtualMachine();
        auto execFrame = vm.getCurrentExecFrame();
        auto regOffset = execFrame.regPtr - execFrame.bottomReg;
        return hbox({ text(utl::strcat("%", index - regOffset, " = ")) |
                          align_right | size(WIDTH, EQUAL, 8),
                      text(std::to_string(
                          vm.getRegister(utl::narrow_cast<size_t>(index)))) });
    }

    Model* model;
    intptr_t index;
};

struct RegView: ScrollBase {
    RegView(Model* model): model(model) {
        for (intptr_t index = 0; index < maxReg; ++index) {
            Add(Make<RegEntry>(model, index));
        }
    }

    Model* model;
    intptr_t maxReg = 256;
};

} // namespace

ftxui::Component sdb::RegisterView(Model* model) {
    return Make<RegView>(model);
}
