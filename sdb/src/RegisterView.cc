#include "Views.h"

#include <sstream>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

#include "Model.h"

using namespace sdb;
using namespace ftxui;

using TableEntry = std::vector<Element>;

namespace {

struct RegView: ftxui::ComponentBase {
    RegView(Model* model): model(model) {
        Add(Renderer([this] {
            std::vector<TableEntry> registers;
            for (size_t index = 0; index < maxReg; ++index) {
                registers.push_back(makeRegEntry(index));
            }
            auto table = Table(std::move(registers));
            table.SelectColumn(0).BorderRight();
            return table.Render();
        }));
    }

    TableEntry makeRegEntry(size_t index) const {
        return { text("%" + std::to_string(index)),
                 text(std::to_string(
                     model->virtualMachine().getRegister(index))) };
    }

    Model* model;
    size_t maxReg = 32;
};

} // namespace

ftxui::Component sdb::RegisterView(Model* model) {
    return Make<RegView>(model);
}
