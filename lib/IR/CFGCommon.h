#ifndef SCATHA_IR_COMMON_H_
#define SCATHA_IR_COMMON_H_

#include "Common/Dyncast.h"

namespace scatha::ir {

///
/// ** Forward declarations of types in the GFC **
///

// Value
// ├─ Parameter
// ├─ Constant
// │  └─ Function
// ├─ BasicBlock
// └─ Instruction
//    ├─ UnaryInstruction
//    │  └─ Alloca
//    ├─ CompareInst
//    └─ TerminatorInst
//       ├─ Goto
//       ├─ Branch
//       └─ Return

class Value;
class Parameter;
class Constant;
class Function;
class BasicBlock;
class Instruction;
class UnaryInstruction;
class Alloca;
class CompareInst;
class TerminatorInst;
class Goto;
class Branch;
class Return;

enum class NodeType {
    Value,
    Parameter,
    Constant,
    Function,
    BasicBlock,
    Instruction,
    UnaryInstruction,
    Alloca,
    CompareInst,
    TerminatorInst,
    Goto,
    Branch,
    Return,

    _count
};

} // namespace scatha::ir

#define SC_IR_ENABLE_DYNCAST(type) SC_DYNCAST_MAP(scatha::ir::type, scatha::ir::NodeType::type)

SC_IR_ENABLE_DYNCAST(Value);
SC_IR_ENABLE_DYNCAST(Parameter);
SC_IR_ENABLE_DYNCAST(Constant);
SC_IR_ENABLE_DYNCAST(Function);
SC_IR_ENABLE_DYNCAST(BasicBlock);
SC_IR_ENABLE_DYNCAST(Instruction);
SC_IR_ENABLE_DYNCAST(UnaryInstruction);
SC_IR_ENABLE_DYNCAST(Alloca);
SC_IR_ENABLE_DYNCAST(CompareInst);
SC_IR_ENABLE_DYNCAST(TerminatorInst);
SC_IR_ENABLE_DYNCAST(Goto);
SC_IR_ENABLE_DYNCAST(Branch);
SC_IR_ENABLE_DYNCAST(Return);

#undef SC_IR_ENABLE_DYNCAST

namespace scatha::ir {} // namespace scatha::ir

#endif // SCATHA_IR_COMMON_H_
