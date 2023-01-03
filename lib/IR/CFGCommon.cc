#include "IR/CFGCommon.h"

#include <ostream>

using namespace scatha;
using namespace ir;

std::string_view ir::toString(NodeType nodeType) {
    switch (nodeType) {
#define SC_CGFNODE_DEF(Inst) case NodeType::Inst: return #Inst;
#include "IR/Instructions.def"
    case NodeType::_count: SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, NodeType nodeType) {
    return ostream << toString(nodeType);
}

std::string_view ir::toString(CompareOperation op) {
    switch (op) {
#define SC_COMPARE_OPERATION_DEF(Inst, name) case CompareOperation::Inst: return #name;
#include "IR/Instructions.def"
    case CompareOperation::_count: SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, CompareOperation op) {
    return ostream << toString(op);
}

std::string_view ir::toString(UnaryArithmeticOperation op) {
    switch (op) {
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(Inst, name) case UnaryArithmeticOperation::Inst: return #name;
#include "IR/Instructions.def"
    case UnaryArithmeticOperation::_count: SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, UnaryArithmeticOperation op) {
    return ostream << toString(op);
}

std::string_view ir::toString(ArithmeticOperation op) {
    switch (op) {
#define SC_ARITHMETIC_OPERATION_DEF(Inst, name) case ArithmeticOperation::Inst: return #name;
#include "IR/Instructions.def"
    case ArithmeticOperation::_count: SC_UNREACHABLE();
    };
}

std::ostream& ir::operator<<(std::ostream& ostream, ArithmeticOperation op) {
    return ostream << toString(op);
}
