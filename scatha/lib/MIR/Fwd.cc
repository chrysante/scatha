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
#include "MIR/Lists.def.h"
    }
}

std::ostream& mir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
}

void mir::do_delete(mir::Value& value) {
    visit(value, [](auto& value) { delete &value; });
}

void mir::do_destroy(mir::Value& value) {
    visit(value, [](auto& value) { std::destroy_at(&value); });
}

std::string_view mir::toString(InstType instType) {
    switch (instType) {
#define SC_MIR_INSTCLASS_DEF(Inst, ...)                                        \
    case InstType::Inst:                                                       \
        return #Inst;
#include "MIR/Lists.def.h"
    }
}

std::ostream& mir::operator<<(std::ostream& ostream, InstType instType) {
    return ostream << toString(instType);
}

void mir::do_delete(mir::Instruction& inst) {
    visit(inst, [](auto& inst) { delete &inst; });
}

void mir::do_destroy(mir::Instruction& inst) {
    visit(inst, [](auto& inst) { std::destroy_at(&inst); });
}
