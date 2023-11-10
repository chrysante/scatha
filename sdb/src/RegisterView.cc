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

    Element Render() override;

    Model* model;
    intptr_t index;
};

struct RegView: ScrollBase {
    RegView(Model* model): model(model) {
        for (intptr_t index = 0; index < maxReg; ++index) {
            Add(Make<RegEntry>(model, index));
        }
    }

    Element Render() override {
        values = model->readRegisters(utl::narrow_cast<size_t>(maxReg));
        auto& vm = model->virtualMachine();
        auto execFrame = vm.getCurrentExecFrame();
        currentOffset = execFrame.regPtr - execFrame.bottomReg;
        return ScrollBase::Render();
    }

    Model* model;
    intptr_t maxReg = 256;
    std::vector<uint64_t> values;
    ptrdiff_t currentOffset = 0;
};

} // namespace

Element RegEntry::Render() {
    auto* parent = dynamic_cast<RegView const*>(Parent());
    return hbox({ text(utl::strcat("%", index - parent->currentOffset, " = ")) |
                      align_right | size(WIDTH, EQUAL, 8),
                  text(std::to_string(
                      parent->values[utl::narrow_cast<size_t>(index)])) });
}

ftxui::Component sdb::RegisterView(Model* model) {
    return Make<RegView>(model);
}
