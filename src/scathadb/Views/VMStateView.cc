#include "Views/Views.h"

#include <sstream>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Model/Events.h"
#include "Model/Model.h"
#include "UI/Common.h"
#include "Util/Messenger.h"

using namespace sdb;

using TableEntry = std::vector<ftxui::Element>;

namespace {

struct RegEntry: ftxui::ComponentBase {
    explicit RegEntry(Model* model, intptr_t index):
        model(model), index(index) {}

    ftxui::Element OnRender() override;

    Model* model;
    intptr_t index;
};

struct RegView: ScrollBase, Transceiver {
    RegView(Model* model):
        Transceiver(model->messenger()->shared_from_this()), model(model) {
        for (intptr_t index = 0; index < maxReg; ++index)
            Add(ftxui::Make<RegEntry>(model, index));
    }

    ftxui::Element OnRender() override {
        if (!model->isPaused()) return ftxui::text("") | ftxui::flex;
        svm::ExecutionFrame execFrame{};
        send_now(DoOnVMThread{ [&](svm::VirtualMachine const& vm) {
            auto regs = vm.registerData();
            values.assign(regs.begin(),
                          std::max(regs.end(), regs.begin() + maxReg));
            execFrame = vm.getCurrentExecFrame();
        } });
        currentOffset = execFrame.regPtr - execFrame.bottomReg;
        return ScrollBase::OnRender();
    }

    Model* model;
    intptr_t maxReg = 256;
    std::vector<uint64_t> values;
    ptrdiff_t currentOffset = 0;
};

} // namespace

ftxui::Element RegEntry::OnRender() {
    auto* parent = dynamic_cast<RegView const*>(Parent());
    auto& values = parent->values;
    if (index >= (ssize_t)values.size()) {
        return ftxui::text("");
    }
    uint64_t value = values[utl::narrow_cast<size_t>(index)];
    auto ptr = std::bit_cast<svm::VirtualPointer>(value);
    auto derefRange = [&] {
        ptrdiff_t derefRange = -1;
        parent->send_now(DoOnVMThread{ [&](svm::VirtualMachine const& vm) {
            derefRange = vm.validPtrRange(ptr);
        } });
        return derefRange;
    }();
    std::stringstream sstr;
    if (derefRange >= 0) {
        sstr << "[" << ptr.slotIndex << ":" << ptr.offset << ":" << derefRange
             << "]";
    }
    else {
        sstr << value;
    }
    ftxui::Element elem = ftxui::text(sstr.str());
    return ftxui::hbox(
        { ftxui::text(utl::strcat("%", index - parent->currentOffset, " = ")) |
              ftxui::align_right | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
          elem });
}

static ftxui::Component CompareFlagsView(Model* model) {
    return ftxui::Renderer([model] {
        bool active = model->isPaused();
        auto flags = [&] {
            svm::CompareFlags flags{};
            if (!active) return flags;
            model->messenger()->send_now(
                DoOnVMThread{ [&](svm::VirtualMachine const& vm) {
                flags = vm.getCompareFlags();
            } });
            return flags;
        }();
        auto display = [=](std::string name, bool cond) {
            return ftxui::text(name) | ftxui::bold |
                   ftxui::color(!active ? ftxui::Color::GrayDark :
                                cond    ? ftxui::Color::Green :
                                          ftxui::Color::Red) |
                   ftxui::center |
                   ftxui::size(ftxui::WIDTH, ftxui::EQUAL,
                               utl::narrow_cast<int>(name.size() + 2));
        };
        return ftxui::hbox({
                   display("==", flags.equal),
                   display("!=", !flags.equal),
                   display("<", flags.less),
                   display("<=", flags.less || flags.equal),
                   display(">", !flags.less && !flags.equal),
                   display(">=", !flags.less),
               }) |
               ftxui::center;
    });
}

ftxui::Component sdb::VMStateView(Model* model) {
    auto cont = ftxui::Container::Vertical({
        ftxui::Make<RegView>(model),
        sdb::Separator(),
        CompareFlagsView(model),
    });
    return ftxui::Renderer(cont, [=] {
        if (model->isIdle()) return placeholder("No Debug Session");
        return cont->Render();
    });
}
