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

/// ## Forward declarations of all GFC node types
/// ```
/// Value
/// ├─ Parameter
/// ├─ BasicBlock
/// └─ User
///    ├─ Constant
///    │  ├─ IntegralConstant
///    │  └─ Function
///    └─ Instruction
///       ├─ Alloca
///       ├─ UnaryInstruction
///       │  ├─ Load
///       │  └─ UnaryArithmeticInst
///       ├─ BinaryInstruction
///       │  ├─ Store
///       │  ├─ CompareInst
///       │  └─ ArithmeticInst
///       ├─ TerminatorInst
///       │  ├─ Goto
///       │  ├─ Branch
///       │  └─ Return
///       ├─ FunctionCall
///       └─ Phi
/// ```

#define SC_CGFNODE_DEF(Node, _) class Node;
#include <scatha/IR/Lists.def>

/// List of all CFG node type.
enum class NodeType {
#define SC_CGFNODE_DEF(Node, _) Node,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert \p nodeType to string.
SCATHA(API) std::string_view toString(NodeType nodeType);

/// Insert \p nodeType into \p ostream.
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, NodeType nodeType);

/// List of all compare arithmetic operations in the IR module.
enum class CompareOperation {
#define SC_COMPARE_OPERATION_DEF(Op, _) Op,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert \p compareOp to string.
SCATHA(API) std::string_view toString(CompareOperation compareOp);

/// Insert \p compareOp into \p ostream.
SCATHA(API)
std::ostream& operator<<(std::ostream& ostream, CompareOperation compareOp);

/// List of all unary arithmetic operations in the IR module.
enum class UnaryArithmeticOperation {
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(Op, _) Op,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert \p unaryArithmeticOp to string.
SCATHA(API)
std::string_view toString(UnaryArithmeticOperation unaryArithmeticOp);

/// Insert \p unaryArithmeticOp into \p ostream.
SCATHA(API)
std::ostream& operator<<(std::ostream& ostream,
                         UnaryArithmeticOperation unaryArithmeticOp);

/// List of all binary arithmetic operations in the IR module.
enum class ArithmeticOperation {
#define SC_ARITHMETIC_OPERATION_DEF(Op, _) Op,
#include <scatha/IR/Lists.def>
    _count
};

/// Convert \p arithmeticOp to string.
SCATHA(API) std::string_view toString(ArithmeticOperation arithmeticOp);

/// Insert \p arithmeticOp into \p ostream.
SCATHA(API)
std::ostream& operator<<(std::ostream& ostream,
                         ArithmeticOperation arithmeticOp);

/// ## Forward declarations of type categories

#define SC_TYPE_CATEGORY_DEF(TypeCat) class TypeCat;
#include <scatha/IR/Lists.def>

/// List of all type categories.
enum class TypeCategory {
#define SC_TYPE_CATEGORY_DEF(TypeCat) TypeCat,
#include <scatha/IR/Lists.def>
    _count
};

} // namespace scatha::ir

/// Map enum `NodeType` to actual node types
#define SC_CGFNODE_DEF(Node, Abstractness)                                     \
    SC_DYNCAST_MAP(::scatha::ir::Node,                                         \
                   ::scatha::ir::NodeType::Node,                               \
                   Abstractness)
#include <scatha/IR/Lists.def>

/// Map enum `TypeCategory` to actual type category classes
#define SC_TYPE_CATEGORY_DEF(TypeCat)                                          \
    SC_DYNCAST_MAP(::scatha::ir::TypeCat,                                      \
                   ::scatha::ir::TypeCategory::TypeCat,                        \
                   CONCRETE);
#include <scatha/IR/Lists.def>

namespace scatha::ir::internal {

template <bool IsConst>
struct PhiMappingImpl {
    using BB = std::conditional_t<IsConst, BasicBlock const, BasicBlock>;
    using V  = std::conditional_t<IsConst, Value const, Value>;

    PhiMappingImpl() = default;

    PhiMappingImpl(BB* pred, V* value): pred(pred), value(value) {}

    PhiMappingImpl(PhiMappingImpl<false> p)
    requires IsConst
        : pred(p.pred), value(p.value) {}

    template <typename BBPtr, typename VPtr>
    PhiMappingImpl(std::pair<BBPtr, VPtr> p): pred(p.first), value(p.second) {}

    bool operator==(PhiMappingImpl const&) const = default;

    BB* pred = nullptr;
    V* value = nullptr;
};

} // namespace scatha::ir::internal

template <bool IsConst>
struct std::hash<scatha::ir::internal::PhiMappingImpl<IsConst>> {
    std::size_t operator()(
        scatha::ir::internal::PhiMappingImpl<IsConst> const& m) const {
        return utl::hash_combine(m.pred, m.value);
    }
};

namespace scatha::ir {

using PhiMapping      = internal::PhiMappingImpl<false>;
using ConstPhiMapping = internal::PhiMappingImpl<true>;

} // namespace scatha::ir

namespace scatha::internal {

/// Insulated call to `delete` on the most derived base of \p *value
SCATHA(API) void privateDelete(ir::Value* value);

/// Insulated call to destructor on the most derived base of \p *value
SCATHA(API) void privateDestroy(ir::Value* value);

/// Insulated call to `delete` on the most derived base of \p *type
SCATHA(API) void privateDelete(ir::Type* type);

/// Insulated call to destructor on the most derived base of \p *type
SCATHA(API) void privateDestroy(ir::Type* type);

} // namespace scatha::internal

#endif // SCATHA_IR_COMMON_H_
