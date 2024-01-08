#ifndef SCATHA_MIR_FWD_H_
#define SCATHA_MIR_FWD_H_

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>
#include <scatha/Common/FFI.h>
#include <scatha/IR/Fwd.h> /// To borrow some enums for the `mir` namespace

namespace scatha::mir {

class Context;
class Module;
class Instruction;

/// Unlike in the IR, instructions are not values. Instructions operate i.e. use
/// and define registers. Except for that difference, the MIR is structured
/// similarly the to IR. Modules have a list of functions, functions have a list
/// of basic blocks and basic blocks have a list of instruction. Furthermore
/// functions have lists of registers that their instructions can operate on.
///
/// ```
/// Value
/// ├─ Register
/// │  ├─ SSARegister
/// │  ├─ VirtualRegister
/// │  ├─ CalleeRegister
/// │  └─ HardwareRegister
/// ├─ Constant
/// ├─ UndefValue
/// ├─ BasicBlock
/// └─ Callable
///    ├─ Function
///    └─ ForeignFunction
/// ```
///

/// Forward declarations of all CFG node types in the MIR module.
#define SC_MIR_CFGNODE_DEF(Type, ...) class Type;
#include <scatha/MIR/Lists.def>

/// Enum listing all CFG node types in the MIR module.
enum class NodeType {
#define SC_MIR_CFGNODE_DEF(Type, ...) Type,
#include <scatha/MIR/Lists.def>
    LAST = ForeignFunction
};

/// Convert \p nodeType to string.
std::string_view toString(NodeType nodeType);

///
std::ostream& operator<<(std::ostream& ostream, NodeType nodeType);

/// Insulated call to `delete` on the most derived base of \p value
SCTEST_API void privateDelete(mir::Value* value);

/// Insulated call to destructor on the most derived base of \p value
SCTEST_API void privateDestroy(mir::Value* value);

/// Forward declarations of all instructions in the MIR module.
#define SC_MIR_INSTCLASS_DEF(Type, ...) class Type;
#include <scatha/MIR/Lists.def>

/// Enum listing all instruction types in the MIR module.
enum class InstType : uint16_t {
#define SC_MIR_INSTCLASS_DEF(Type, ...) Type,
#include <scatha/MIR/Lists.def>
    LAST = SelectInst
};

/// Convert \p instType to string
std::string_view toString(InstType instType);

///
std::ostream& operator<<(std::ostream& ostream, InstType type);

/// Insulated call to `delete` on the most derived base of \p inst
SCTEST_API void privateDelete(mir::Instruction* inst);

/// Insulated call to destructor on the most derived base of \p inst
SCTEST_API void privateDestroy(mir::Instruction* inst);

/// To make the base parent case in the dyncast macro work
using VoidParent = void;

} // namespace scatha::mir

/// Map enum `NodeType` to actual node types
#define SC_MIR_CFGNODE_DEF(Node, Parent, Corporeality)                         \
    SC_DYNCAST_DEFINE(::scatha::mir::Node,                                     \
                      ::scatha::mir::NodeType::Node,                           \
                      ::scatha::mir::Parent,                                   \
                      Corporeality)
#include <scatha/MIR/Lists.def>

/// Map enum `InstType` to actual instruction types
#define SC_MIR_INSTCLASS_DEF(Inst, Parent, Corporeality)                       \
    SC_DYNCAST_DEFINE(::scatha::mir::Inst,                                     \
                      ::scatha::mir::InstType::Inst,                           \
                      ::scatha::mir::Parent,                                   \
                      Corporeality)
#include <scatha/MIR/Lists.def>

namespace scatha::mir {

/// Different register phases of the lowering process
enum class RegisterPhase {
    /// After lowering from IR the MIR is in SSA form
    SSA,

    /// After leaving SSA form the MIR is in "virtual register" form. This is
    /// somewhat of a misnomer because SSA registers are virtual as well, but
    /// we'll stick with it for now
    Virtual,

    /// Register allocation transforms the virtual register form into hardware
    /// register form. Lowering from here to assembly is pretty much a
    /// one-to-one translation
    Hardware
};

using ir::ArithmeticOperation;
using ir::CompareMode;
using ir::CompareOperation;
using ir::Conversion;
using ir::UnaryArithmeticOperation;
using ir::Visibility;

/// Constant factor and term of the address calculation
struct MemAddrConstantData {
    bool operator==(MemAddrConstantData const&) const = default;

    uint8_t offsetFactor;
    uint8_t offsetTerm;
};

/// Encapsules memory address representation of the VM.
template <typename V>
class MemoryAddressImpl {
public:
    MemoryAddressImpl(V* base,
                      V* dynOffset,
                      size_t offsetFactor,
                      size_t offsetTerm):
        MemoryAddressImpl(base,
                          dynOffset,
                          { utl::narrow_cast<uint8_t>(offsetFactor),
                            utl::narrow_cast<uint8_t>(offsetTerm) }) {}

    MemoryAddressImpl(V* base, V* dynOffset, MemAddrConstantData constData):
        base(base), _dynOffset(dynOffset), constData(constData) {}

    explicit MemoryAddressImpl(V* base, size_t offsetTerm = 0):
        MemoryAddressImpl(base, nullptr, 0, offsetTerm) {}

    bool operator==(MemoryAddressImpl const&) const = default;

    /// \Returns the register that holds the base address
    V* baseAddress() const { return base; }

    /// \Returns the register that holds the offset factor or `nullptr` if none
    /// is present
    V* dynOffset() const { return _dynOffset; }

    /// \Returns the constant data i.e. offset factor and offset term
    MemAddrConstantData constantData() const { return constData; }

    /// \Returns the constant offset factor
    size_t offsetFactor() const { return constData.offsetFactor; }

    /// \Returns the constant offset term
    size_t offsetTerm() const { return constData.offsetTerm; }

private:
    V* base;
    V* _dynOffset;
    MemAddrConstantData constData;
};

using MemoryAddress = MemoryAddressImpl<Value>;
using ConstMemoryAddress = MemoryAddressImpl<Value const>;

/// Base class for instruction and basic block
class ProgramPoint {
public:
    enum Kind { Kind_BasicBlock, Kind_Instruction };

    /// The index of this program point in the function
    /// \pre `linearize()` must have been called on the parent function
    int index() const {
        SC_EXPECT(hasIndex());
        return ppIdx;
    }

    /// \Returns `true` if `linearize()` has been called on the parent function
    bool hasIndex() const { return ppIdx != -1; }

protected:
    ProgramPoint(Kind kind): ppKind(kind) {}

    Kind progPointKind() const { return ppKind; }

private:
    friend class Function;
    int ppIdx = -1;
    Kind ppKind;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_FWD_H_
