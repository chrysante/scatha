#include "Views.h"

#include <sstream>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Common.h"
#include "Model.h"

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
        if (!model->isActive() || !model->isSleeping()) {
            return text("");
        }
        values = model->readRegisters(utl::narrow_cast<size_t>(maxReg));
        auto& vm = model->VM();
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
    std::stringstream sstr;
    uint64_t value = parent->values[utl::narrow_cast<size_t>(index)];
    auto ptr = std::bit_cast<svm::VirtualPointer>(value);
    auto derefRange = parent->model->VM().validPtrRange(ptr);
    if (derefRange >= 0) {
        sstr << "0x" << std::hex << value << " [deref=" << std::dec
             << derefRange << "]";
    }
    else {
        sstr << value;
    }
    Element elem = text(sstr.str());
    return hbox({ text(utl::strcat("%", index - parent->currentOffset, " = ")) |
                      align_right | size(WIDTH, EQUAL, 8),
                  elem });
}

static Component CompareFlagsView(Model* model) {
    return Renderer([model] {
        auto flags = model->VM().getCompareFlags();
        bool active = model->isActive() && model->isSleeping();
        auto display = [=](std::string name, bool cond) {
            return text(name) | bold |
                   color(!active ? Color::GrayDark :
                         cond    ? Color::Green :
                                   Color::Red) |
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

ftxui::Component sdb::VMStateView(Model* model) {
    auto cont = Container::Vertical({
        Make<RegView>(model),
        sdb::separator(),
        CompareFlagsView(model),
    });
    return Renderer(cont, [=] {
        if (!model->isActive()) {
            return text("No Debug Session") | bold | dim | center;
        }
        return cont->Render();
    });
}
