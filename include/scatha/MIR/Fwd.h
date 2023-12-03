#ifndef SCATHA_MIR_FWD_H_
#define SCATHA_MIR_FWD_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>
#include <scatha/IR/Fwd.h> // To borrow some enums for this MIR namespace

namespace scatha::mir {

class Instruction;
class Module;

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
/// └─ Function
/// ```
///

/// Forward declarations of all CFG node types in the MIR module.
#define SC_MIR_CFGNODE_DEF(type, _) class type;
#include <scatha/MIR/Lists.def>

/// Enum listing all CFG node types in the MIR module.
enum class NodeType {
#define SC_MIR_CFGNODE_DEF(type, _) type,
#include <scatha/MIR/Lists.def>
    _count
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
#define SC_MIR_INSTCLASS_DEF(type, _) class type;
#include <scatha/MIR/Lists.def>

/// Enum listing all instruction types in the MIR module.
enum class InstType : uint16_t {
#define SC_MIR_INSTCLASS_DEF(type, _) type,
#include <scatha/MIR/Lists.def>
    _count
};

/// Convert \p instType to string
std::string_view toString(InstType instType);

///
std::ostream& operator<<(std::ostream& ostream, InstType type);

/// Insulated call to `delete` on the most derived base of \p inst
SCTEST_API void privateDelete(mir::Instruction* inst);

/// Insulated call to destructor on the most derived base of \p inst
SCTEST_API void privateDestroy(mir::Instruction* inst);

} // namespace scatha::mir

/// Map enum `NodeType` to actual node types
#define SC_MIR_CFGNODE_DEF(Node, Abstractness)                                 \
    SC_DYNCAST_MAP(::scatha::mir::Node,                                        \
                   ::scatha::mir::NodeType::Node,                              \
                   Abstractness)
#include <scatha/MIR/Lists.def>

/// Map enum `InstType` to actual instruction types
#define SC_MIR_INSTCLASS_DEF(Inst, Abstractness)                               \
    SC_DYNCAST_MAP(::scatha::mir::Inst,                                        \
                   ::scatha::mir::InstType::Inst,                              \
                   Abstractness)
#include <scatha/MIR/Lists.def>

namespace scatha::mir {

/// Enum listing all instructions in the MIR module.
enum class InstCode : uint16_t {
#define SC_MIR_INSTRUCTION_DEF(type, name) type,
#include <scatha/MIR/Lists.def>
    _count
};

/// Convert \p code to string
std::string_view toString(InstCode code);

std::ostream& operator<<(std::ostream& ostream, InstCode code);

using ir::ArithmeticOperation;
using ir::CompareMode;
using ir::CompareOperation;
using ir::Conversion;
using ir::UnaryArithmeticOperation;
using ir::Visibility;

/// Encapsules memory address representation of the VM.
template <typename V>
class MemoryAddressImpl {
public:
    /// Constant factor and term of the address calculation
    struct ConstantData {
        uint8_t offsetFactor;
        uint8_t offsetTerm;
    };

    MemoryAddressImpl(V* base,
                      V* dynOffset,
                      uint32_t offsetFactor,
                      uint32_t offsetTerm):
        MemoryAddressImpl(base,
                          dynOffset,
                          { utl::narrow_cast<uint8_t>(offsetFactor),
                            utl::narrow_cast<uint8_t>(offsetTerm) }) {}

    MemoryAddressImpl(V* base, V* dynOffset, ConstantData constData):
        base(base), _dynOffset(dynOffset), constData(constData) {}

    explicit MemoryAddressImpl(V* base, uint32_t offsetTerm = 0):
        MemoryAddressImpl(base, nullptr, 0, offsetTerm) {}

    /// \Returns The register that holds the base address
    V* baseAddress() { return base; }

    /// \Returns The register that holds the offset factor or `nullptr` if none
    /// is present
    V* dynOffset() { return _dynOffset; }

    /// \Returns The constant data i.e. offset factor and offset term
    ConstantData constantData() const { return constData; }

    /// \Returns The constant offset factor
    uint32_t offsetFactor() { return constData.offsetFactor; }

    /// \Returns The constant offset term
    uint32_t offsetTerm() { return constData.offsetTerm; }

private:
    V* base;
    V* _dynOffset;
    ConstantData constData;
};

using MemoryAddress = MemoryAddressImpl<Value>;
using ConstMemoryAddress = MemoryAddressImpl<Value const>;

/// Represents the address of an external function.
struct ExtFuncAddress {
    uint32_t slot  : 11;
    uint32_t index : 21;
};

/// `InstData` for call instructions.
struct CallInstData {
    ///
    uint32_t regOffset{};

    /// Only used by `callext` instructions.
    ExtFuncAddress extFuncAddress{};

    ///
    bool isMemoryCall{};

    ///
    MemoryAddress::ConstantData addressData{};
};

static_assert(sizeof(CallInstData) <= 16,
              "Must fit into `instdata` field of Instruction class");

} // namespace scatha::mir

#endif // SCATHA_MIR_FWD_H_