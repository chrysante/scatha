// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_COMMON_H_
#define SCATHA_IR_COMMON_H_

#include <iosfwd>
#include <string_view>

#include <utl/hash.hpp>

#include <scatha/Basic/Basic.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::ir {

class Context;

class Module;

/// ** Forward declarations of all GFC node types  **

// Value
// ├─ Parameter
// ├─ BasicBlock
// └─ User
//    ├─ Constant
//    │  ├─ IntegralConstant
//    │  └─ Function
//    └─ Instruction
//       ├─ Alloca
//       ├─ UnaryInstruction
//       │  ├─ Load
//       │  └─ UnaryArithmeticInst
//       ├─ BinaryInstruction
//       │  ├─ Store
//       │  ├─ CompareInst
//       │  └─ ArithmeticInst
//       ├─ TerminatorInst
//       │  ├─ Goto
//       │  ├─ Branch
//       │  └─ Return
//       ├─ FunctionCall
//       └─ Phi

#define SC_CGFNODE_DEF(Inst) class Inst;
#include <scatha/IR/Lists.def>

/// List of all CFG node type.
enum class NodeType {
#define SC_CGFNODE_DEF(Inst) Inst,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert a \p NodeType to string.
SCATHA(API) std::string_view toString(NodeType nodeType);

/// Insert a \p NodeType into ostream.
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, NodeType nodeType);

/// List of all compare arithmetic operations in the IR module.
enum class CompareOperation {
#define SC_COMPARE_OPERATION_DEF(Inst, _) Inst,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert a \p CompareOperation to string.
SCATHA(API) std::string_view toString(CompareOperation op);

/// Insert a \p CompareOperation into ostream.
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, CompareOperation op);

/// List of all unary arithmetic operations in the IR module.
enum class UnaryArithmeticOperation {
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(Inst, _) Inst,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert a \p UnaryArithmeticOperation to string.
SCATHA(API) std::string_view toString(UnaryArithmeticOperation op);

/// Insert a \p UnaryArithmeticOperation into ostream.
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, UnaryArithmeticOperation op);

/// List of all binary arithmetic operations in the IR module.
enum class ArithmeticOperation {
#define SC_ARITHMETIC_OPERATION_DEF(Inst, _) Inst,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert a \p ArithmeticOperation to string.
SCATHA(API) std::string_view toString(ArithmeticOperation op);

/// Insert a \p ArithmeticOperation into ostream.
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, ArithmeticOperation op);

/// ** Forward declarations of type categories  **

#define SC_TYPE_CATEGORY_DEF(TypeCat) class TypeCat;
#include <scatha/IR/Lists.def>

/// List of all type categories.
enum class TypeCategory {
#define SC_TYPE_CATEGORY_DEF(TypeCat) TypeCat,
#include <scatha/IR/Lists.def>
    _count
};

} // namespace scatha::ir

/// Map enum \p NodeType to actual node types
#define SC_CGFNODE_DEF(Inst) SC_DYNCAST_MAP(::scatha::ir::Inst, ::scatha::ir::NodeType::Inst);
#include <scatha/IR/Lists.def>

/// Map enum \p TypeCategory to actual type category classes
#define SC_TYPE_CATEGORY_DEF(TypeCat) SC_DYNCAST_MAP(::scatha::ir::TypeCat, ::scatha::ir::TypeCategory::TypeCat);
#include <scatha/IR/Lists.def>

namespace scatha::ir::internal {

template <bool IsConst>
struct PhiMappingImpl {
    using BB = std::conditional_t<IsConst, BasicBlock const, BasicBlock>;
    using V = std::conditional_t<IsConst, Value const, Value>;
    PhiMappingImpl(BB* pred, V* value): pred(pred), value(value) {}
    
    PhiMappingImpl(PhiMappingImpl<false> p) requires IsConst: pred(p.pred), value(p.value) {}
    
    bool operator==(PhiMappingImpl const&) const = default;
    
    BB* pred;
    V* value;
};

} // namespace scatha:ir::internal

template <bool IsConst>
struct std::hash<scatha::ir::internal::PhiMappingImpl<IsConst>> {
    std::size_t operator()(scatha::ir::internal::PhiMappingImpl<IsConst> const& m) const {
        return utl::hash_combine(m.pred, m.value);
    }
};
        
namespace scatha::ir {

using PhiMapping = internal::PhiMappingImpl<false>;
using ConstPhiMapping = internal::PhiMappingImpl<true>;

} // scatha::ir

#endif // SCATHA_IR_COMMON_H_
