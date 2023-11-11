#include "Views.h"

#include <ftxui/dom/elements.hpp>
#include <range/v3/view.hpp>

#include "Common.h"

using namespace sdb;
using namespace ftxui;

namespace {

struct ToolbarBase: ComponentBase {
    using ComponentBase::ComponentBase;

    Element Render() override {
        std::vector<Element> elems;
        for (size_t i = 0; i < ChildCount(); ++i) {
            if (i > 0) {
                elems.push_back(sdb::separatorEmpty()->Render());
            }
            elems.push_back(ChildAt(i)->Render());
        }
        return hbox(std::move(elems));
    }
};

} // namespace

Component sdb::Toolbar(std::vector<Component> components) {
    return Make<ToolbarBase>(std::move(components));
}
