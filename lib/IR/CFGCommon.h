// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_COMMON_H_
#define SCATHA_IR_COMMON_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Basic/Basic.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::ir {

///
/// ** Forward declarations of all types in the GFC **
///

// Value
// ├─ Parameter
// ├─ Constant
// │  ├─ IntegralConstant
// │  └─ Function
// ├─ BasicBlock
// └─ Instruction
//    ├─ Alloca
//    ├─ UnaryInstruction
//    │  ├─ Load
//    │  └─ UnaryArithmeticInst
//    ├─ BinaryInstruction
//    │  ├─ Store
//    │  ├─ CompareInst
//    │  └─ ArithmeticInst
//    ├─ TerminatorInst
//    │  ├─ Goto
//    │  ├─ Branch
//    │  └─ Return
//    ├─ FunctionCall
//    └─ Phi

#define SC_CGFNODE_DEF(Inst) class Inst;
#include <scatha/IR/Lists.def>

enum class NodeType {
#define SC_CGFNODE_DEF(Inst) Inst,
#include <scatha/IR/Lists.def>
    _count
};

SCATHA(API) std::string_view toString(NodeType nodeType);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, NodeType nodeType);

enum class CompareOperation {
#define SC_COMPARE_OPERATION_DEF(Inst, _) Inst,
#include <scatha/IR/Lists.def>
    _count
};

SCATHA(API) std::string_view toString(CompareOperation op);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, CompareOperation op);

enum class UnaryArithmeticOperation {
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(Inst, _) Inst,
#include <scatha/IR/Lists.def>
    _count
};

SCATHA(API) std::string_view toString(UnaryArithmeticOperation op);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, UnaryArithmeticOperation op);

enum class ArithmeticOperation {
#define SC_ARITHMETIC_OPERATION_DEF(Inst, _) Inst,
#include <scatha/IR/Lists.def>
    _count
};

SCATHA(API) std::string_view toString(ArithmeticOperation op);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, ArithmeticOperation op);

} // namespace scatha::ir

#define SC_CGFNODE_DEF(Inst) SC_DYNCAST_MAP(::scatha::ir::Inst, ::scatha::ir::NodeType::Inst);
#include <scatha/IR/Lists.def>

#endif // SCATHA_IR_COMMON_H_
