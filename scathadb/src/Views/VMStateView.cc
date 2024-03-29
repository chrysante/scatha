#include "Views/Views.h"

#include <sstream>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model/Model.h"
#include "UI/Common.h"

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
        if (!model->isPaused()) {
            return text("");
        }
        auto lock = model->lockVM();
        values = model->readRegisters(utl::narrow_cast<size_t>(maxReg));
        auto& vm = model->VM();
        auto execFrame = vm.getCurrentExecFrame();
        currentOffset = execFrame.regPtr - execFrame.bottomReg;
        lock.unlock();
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
    auto& values = parent->values;
    if (index >= (ssize_t)values.size()) {
        return text("");
    }
    uint64_t value = values[utl::narrow_cast<size_t>(index)];
    auto ptr = std::bit_cast<svm::VirtualPointer>(value);
    auto derefRange = parent->model->VM().validPtrRange(ptr);
    std::stringstream sstr;
    if (derefRange >= 0) {
        sstr << "[" << ptr.slotIndex << ":" << ptr.offset << ":" << derefRange
             << "]";
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
        bool active = model->isPaused();
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
        sdb::Separator(),
        CompareFlagsView(model),
    });
    return Renderer(cont, [=] {
        if (model->isStopped()) {
            return placeholder("No Debug Session");
        }
        return cont->Render();
    });
}
