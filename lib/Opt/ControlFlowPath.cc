#include "Opt/ControlFlowPath.h"

using namespace scatha;
using namespace opt;

bool ControlFlowPath::valid() const {
    return validImpl([](ir::BasicBlock const* bb) { return bb->successors(); });
}

bool ControlFlowPath::validBackwards() const {
    return validImpl([](ir::BasicBlock const* bb) -> auto const& { return bb->predecessors; });
}

bool ControlFlowPath::validImpl(auto getListOfSuccessors) const {
    switch (_bbs.size()) {
    case 0:
        return false;
    case 1:
        return _beginInst->parent() == _backInst->parent() && _beginInst->parent() == _bbs.front();
        
    default:
        auto itr = _bbs.begin();
        if (_beginInst->parent() != *itr) {
            return false;
        }
        for (auto backItr = _bbs.end() - 1; itr != backItr; ) {
            auto next = std::next(itr);
            auto&& succs = getListOfSuccessors(*itr);
            if (std::find(succs.begin(), succs.end(), *next) == succs.end()) {
                return false;
            }
            itr = next;
        }
        if (_backInst->parent() != *itr) {
            return false;
        }
    }
    return true;
}
