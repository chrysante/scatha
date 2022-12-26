#include "IR/CFGCommon.h"

#include <ostream>

using namespace scatha;
using namespace ir;

std::string_view ir::toString(NodeType nodeType) {
    switch (nodeType) {
        // clang-format off
#define SC_INSTRUCTION_DEF(Inst) case NodeType::Inst: return #Inst;
#include "IR/Instructions.def"
        // clang-format on
    case NodeType::_count: SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
}

std::string_view ir::toString(CompareOperation op) {
    switch (op) {
        // clang-format off
#define SC_COMPARE_OPERATION_DEF(Inst, name) case CompareOperation::Inst: return #name;
#include "IR/Instructions.def"
        // clang-format on
    case CompareOperation::_count: SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, CompareOperation op) {
    return ostream << toString(op);
}

std::string_view ir::toString(ArithmeticOperation op) {
    switch (op) {
        // clang-format off
#define SC_ARITHMETIC_OPERATION_DEF(Inst, name) case ArithmeticOperation::Inst: return #name;
#include "IR/Instructions.def"
        // clang-format on
    case ArithmeticOperation::_count: SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, ArithmeticOperation op) {
    return ostream << toString(op);
}
