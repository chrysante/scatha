#include "CodeGen/ISelTest.h"

#include "CodeGen/SelectionDAG.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace cg;

void cg::isel(ir::Function const& function) {
    for (auto& BB: function) {
        auto DAG = SelectionDAG::build(BB);

        generateGraphvizTmp(DAG);
    }
}
