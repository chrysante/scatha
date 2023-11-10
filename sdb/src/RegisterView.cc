#include "Views.h"

#include <sstream>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

using namespace sdb;
using namespace ftxui;

using TableEntry = std::vector<Element>;

static TableEntry makeRegEntry(size_t index) {
    return { text("%" + std::to_string(index)), text(std::to_string(rand())) };
}

namespace {

struct RegView: ftxui::ComponentBase {
    RegView(Model* model): model(model) {
        Add(Renderer([this] {
            std::vector<TableEntry> registers;
            for (size_t index = 0; index < maxReg; ++index) {
                registers.push_back(makeRegEntry(index));
            }
            auto table = Table(std::move(registers));
            // table.SelectAll().Border(LIGHT);
            //  Add border around the first column.
            table.SelectColumn(0).BorderRight();
            return table.Render();
        }));
    }

    Model* model;
    size_t maxReg = 32;
};

} // namespace

ftxui::Component sdb::RegisterView(Model* model) {
    return Make<RegView>(model);
}
