#include "MIR/Fwd.h"

#include "MIR/CFG.h"
#include "MIR/Instructions.h"

using namespace scatha;
using namespace mir;

std::string_view mir::toString(NodeType nodeType) {
    switch (nodeType) {
#define SC_MIR_CFGNODE_DEF(Node, ...)                                          \
    case NodeType::Node:                                                       \
        return #Node;
#include "MIR/Lists.def"
    }
}

std::ostream& mir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
}

void mir::privateDelete(mir::Value* value) {
    visit(*value, [](auto& value) { delete &value; });
}

void mir::privateDestroy(mir::Value* value) {
    visit(*value, [](auto& value) { std::destroy_at(&value); });
}

std::string_view mir::toString(InstType instType) {
    switch (instType) {
#define SC_MIR_INSTCLASS_DEF(Inst, ...)                                        \
    case InstType::Inst:                                                       \
        return #Inst;
#include "MIR/Lists.def"
    }
}

std::ostream& mir::operator<<(std::ostream& ostream, InstType instType) {
    return ostream << toString(instType);
}

void mir::privateDelete(mir::Instruction* inst) {
    visit(*inst, [](auto& inst) { delete &inst; });
}

void mir::privateDestroy(mir::Instruction* inst) {
    visit(*inst, [](auto& inst) { std::destroy_at(&inst); });
}
