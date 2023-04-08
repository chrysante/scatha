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

std::string_view mir::toString(InstructionType instType) {
    switch (instType) {
#define SC_MIR_INSTRUCTION_DEF(Inst, Name)                                     \
    case InstructionType::Inst:                                                \
        return Name;
#include "MIR/Lists.def"
    case InstructionType::_count:
        SC_UNREACHABLE();
    }
}

std::ostream& mir::operator<<(std::ostream& ostream, InstructionType instType) {
    return ostream << toString(instType);
}

std::string_view mir::toString(UnaryArithmeticOperation operation) {
    SC_UNREACHABLE();
}

std::ostream& mir::operator<<(std::ostream& ostream,
                              UnaryArithmeticOperation operation) {
    return ostream << toString(operation);
}

std::string_view mir::toString(ArithmeticOperation operation) {
    SC_UNREACHABLE();
}

std::ostream& mir::operator<<(std::ostream& ostream,
                              ArithmeticOperation operation) {
    return ostream << toString(operation);
}
