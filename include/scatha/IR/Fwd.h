/// This file forward-declares all types in the IR folder and declares some
/// basic functions on them.
///
/// # CFG class hierarchy
/// ```
/// Value
/// ├─ Parameter
/// ├─ BasicBlock
/// └─ User
///    ├─ Constant
///    │  ├─ Global
///    │  │  ├─ GlobalVariable
///    │  │  └─ Callable
///    │  │     ├─ Function
///    │  │     └─ ForeignFunction
///    │  ├─ IntegralConstant
///    │  ├─ FloatingPointConstant
///    │  ├─ NullPointerConstant
///    │  ├─ RecordConstant
///    │  ├─ StructConstant
///    │  ├─ ArrayConstant
///    │  └─ UndefValue
///    └─ Instruction
///       ├─ Alloca
///       ├─ Store
///       ├─ Load
///       ├─ UnaryInstruction
///       │  ├─ ConversionInst
///       │  └─ UnaryArithmeticInst
///       ├─ BinaryInstruction
///       │  ├─ CompareInst
///       │  └─ ArithmeticInst
///       ├─ TerminatorInst
///       │  ├─ Goto
///       │  ├─ Branch
///       │  └─ Return
///       ├─ Call
///       ├─ Phi
///       ├─ GetElementPointer
///       ├─ ExtractValue
///       ├─ InsertValue
///       └─ Select
/// ```

#ifndef SCATHA_IR_FWD_H_
#define SCATHA_IR_FWD_H_

#include <array>
#include <iosfwd>
#include <string_view>

#include <utl/common.hpp>
#include <utl/hash.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::ir {

class Context;
class Module;

/// Forward declaration of CFG nodes
#define SC_VALUENODE_DEF(Node, ...) class Node;
#include <scatha/IR/Lists.def.h>

/// List of all CFG node type
enum class NodeType {
#define SC_VALUENODE_DEF(Node, ...) Node,
#include <scatha/IR/Lists.def.h>
    LAST = InsertValue
};

/// Convert \p nodeType to string
SCATHA_API std::string_view toString(NodeType nodeType);

/// Insert \p nodeType into \p ostream.
SCATHA_API std::ostream& operator<<(std::ostream& ostream, NodeType nodeType);

/// Forward declaration of attributes
#define SC_ATTRIBUTE_DEF(Attrib, ...) class Attrib;
#include <scatha/IR/Lists.def.h>

/// List of all attribute types
enum class AttributeType {
#define SC_ATTRIBUTE_DEF(Attrib, ...) Attrib,
#include <scatha/IR/Lists.def.h>
    LAST = ValRetAttribute
};

/// Convert \p nodeType to string
SCATHA_API std::string toString(AttributeType attrib);

/// Insert \p nodeType into \p ostream.
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    AttributeType attrib);

/// List of conversion operations
enum class Conversion {
#define SC_CONVERSION_DEF(Op, _) Op,
#include <scatha/IR/Lists.def.h>
};

/// Convert \p conv to string
SCATHA_API std::string_view toString(Conversion conv);

/// Insert \p conv into \p ostream
SCATHA_API std::ostream& operator<<(std::ostream& ostream, Conversion conv);

/// List of compare modes (signed, unsigned, float)
enum class CompareMode {
#define SC_COMPARE_MODE_DEF(Op, _) Op,
#include <scatha/IR/Lists.def.h>
};

/// Convert \p compareMode to string
SCATHA_API std::string_view toString(CompareMode compareMode);

/// Insert \p compareMode into \p ostream.
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    CompareMode compareMode);

/// List of all compare operations
enum class CompareOperation {
#define SC_COMPARE_OPERATION_DEF(Op, _) Op,
#include <scatha/IR/Lists.def.h>
};

/// Convert \p compareOp to string
SCATHA_API std::string_view toString(CompareOperation compareOp);

/// Insert \p compareOp into \p ostream.
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    CompareOperation compareOp);

/// \Returns The inverse operation of \p compareOp
/// In particular, this function maps:
/// - `Less` -> `GreaterEq`
/// - `LessEq` -> `Greater`
/// - `Greater` -> `LessEq`
/// - `GreaterEq` -> `Less`
/// - `Equal` -> `NotEqual`
/// - `NotEqual` -> `Equal`
CompareOperation inverse(CompareOperation compareOp);

/// List of all unary arithmetic operations in the IR module.
enum class UnaryArithmeticOperation {
#define SC_UNARY_ARITHMETIC_OPERATION_DEF(Op, _) Op,
#include <scatha/IR/Lists.def.h>
};

/// Convert \p unaryArithmeticOp to string
SCATHA_API std::string_view toString(
    UnaryArithmeticOperation unaryArithmeticOp);

/// Insert \p unaryArithmeticOp into \p ostream.
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    UnaryArithmeticOperation unaryArithmeticOp);

/// List of all binary arithmetic operations in the IR module
enum class ArithmeticOperation {
#define SC_ARITHMETIC_OPERATION_DEF(Op, _) Op,
#include <scatha/IR/Lists.def.h>
};

/// Convert \p arithmeticOp to string
SCATHA_API std::string_view toString(ArithmeticOperation arithmeticOp);

/// Insert \p arithmeticOp into \p ostream
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    ArithmeticOperation arithmeticOp);

/// \Returns `true` if \p op is any of the shift instructions
SCATHA_API bool isShift(ArithmeticOperation op);

/// \Returns `true` if \p op is commutative
SCATHA_API bool isCommutative(ArithmeticOperation op);

/// ## Forward declarations of type categories

#define SC_TYPE_CATEGORY_DEF(TypeCat, ...) class TypeCat;
#include <scatha/IR/Lists.def.h>

/// List of all type categories
enum class TypeCategory {
#define SC_TYPE_CATEGORY_DEF(TypeCat, ...) TypeCat,
#include <scatha/IR/Lists.def.h>
    LAST = FunctionType
};

/// List of all visibility kinds
enum class Visibility {
#define SC_VISKIND_DEF(vis) vis,
#include <scatha/IR/Lists.def.h>
};

/// Bitfield of function attributes
enum class FunctionAttribute : unsigned {
    None = 0,
    All = unsigned(-1),
    Memory_ReadNone = 1 << 0,
    Memory_WriteNone = 1 << 1,
    Memory_None = Memory_ReadNone | Memory_WriteNone
};

UTL_BITFIELD_OPERATORS(FunctionAttribute);

/// To make the base parent case in the dyncast macro work
using VoidParent = void;

} // namespace scatha::ir

/// Map enum `NodeType` to actual node types
#define SC_VALUENODE_DEF(Node, Parent, Corporeality)                           \
    SC_DYNCAST_DEFINE(::scatha::ir::Node, ::scatha::ir::NodeType::Node,        \
                      ::scatha::ir::Parent, Corporeality)
#include <scatha/IR/Lists.def.h>

/// Map enum `AttributeType` to actual attribute types
#define SC_ATTRIBUTE_DEF(Type, Parent, Corporeality)                           \
    SC_DYNCAST_DEFINE(::scatha::ir::Type, ::scatha::ir::AttributeType::Type,   \
                      ::scatha::ir::Parent, Corporeality)
#include <scatha/IR/Lists.def.h>

/// Map enum `TypeCategory` to actual type category classes
#define SC_TYPE_CATEGORY_DEF(TypeCat, Parent, Corporeality)                    \
    SC_DYNCAST_DEFINE(::scatha::ir::TypeCat,                                   \
                      ::scatha::ir::TypeCategory::TypeCat,                     \
                      ::scatha::ir::Parent, Corporeality)
#include <scatha/IR/Lists.def.h>

namespace scatha::ir::internal {

template <bool IsConst>
struct PhiMappingImpl {
    using BB = std::conditional_t<IsConst, BasicBlock const, BasicBlock>;
    using V = std::conditional_t<IsConst, Value const, Value>;

    PhiMappingImpl() = default;

    PhiMappingImpl(BB* pred, V* value): pred(pred), value(value) {}

    template <bool False = false, std::enable_if_t<!False, int> = 0>
    PhiMappingImpl(PhiMappingImpl<False> p): pred(p.pred), value(p.value) {}

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

using PhiMapping = internal::PhiMappingImpl<false>;
using ConstPhiMapping = internal::PhiMappingImpl<true>;

class PointerInfo;
struct PointerInfoDesc;

/// Insulated call to `delete` on the most derived base of \p value
SCATHA_API void do_delete(ir::Value& value);

/// Insulated call to destructor on the most derived base of \p value
SCATHA_API void do_destroy(ir::Value& value);

/// Insulated call to `delete` on the most derived base of \p attrib
SCATHA_API void do_delete(ir::Attribute& attrib);

/// Insulated call to destructor on the most derived base of \p attrib
SCATHA_API void do_destroy(ir::Attribute& attrib);

/// Insulated call to `delete` on the most derived base of \p type
SCATHA_API void do_delete(ir::Type& type);

/// Insulated call to destructor on the most derived base of \p type
SCATHA_API void do_destroy(ir::Type& type);

class ValueRef;
class DomTree;
class DominanceInfo;
class LoopNestingForest;
class LNFNode;
class LoopNestingForest;
class LoopInfo;

/// Convenience wrapper to make `std::array<Value*, N>` in a less verbose
/// way
template <size_t N>
struct ValueArray: std::array<ir::Value*, N> {};

template <typename... T>
ValueArray(T...) -> ValueArray<sizeof...(T)>;

} // namespace scatha::ir

#endif // SCATHA_IR_FWD_H_
