#include "MIR/Fwd.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

std::string_view mir::toString(NodeType nodeType) {
    switch (nodeType) {
#define SC_MIR_CFGNODE_DEF(Node, _)                                            \
    case NodeType::Node:                                                       \
        return #Node;
#include "MIR/Lists.def"
    case NodeType::_count:
        SC_UNREACHABLE();
    }
}

std::ostream& mir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
}

void internal::privateDelete(mir::Value* value) {
    visit(*value, [](auto& value) { delete &value; });
}

void internal::privateDestroy(mir::Value* value) {
    visit(*value, [](auto& value) { std::destroy_at(&value); });
}

std::string_view mir::toString(InstCode code) {
    switch (code) {
#define SC_MIR_INSTRUCTION_DEF(Inst, Name)                                     \
    case InstCode::Inst:                                                       \
        return Name;
#include "MIR/Lists.def"
    case InstCode::_count:
        SC_UNREACHABLE();
    }
}

std::ostream& mir::operator<<(std::ostream& ostream, InstCode code) {
    return ostream << toString(code);
}

bool mir::isTerminator(InstCode code) {
    return code == InstCode::Jump || code == InstCode::CondJump ||
           code == InstCode::Return;
}