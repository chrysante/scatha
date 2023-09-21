#ifndef SCATHA_MIR_FWD_H_
#define SCATHA_MIR_FWD_H_

#include <iosfwd>
#include <string_view>

#include "Common/Base.h"
#include "Common/Dyncast.h"
#include "IR/Fwd.h"

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
#include "MIR/Lists.def"

/// Enum listing all CFG node types in the MIR module.
enum class NodeType {
#define SC_MIR_CFGNODE_DEF(type, _) type,
#include "MIR/Lists.def"
    _count
};

/// Convert \p nodeType to string.
std::string_view toString(NodeType nodeType);

std::ostream& operator<<(std::ostream& ostream, NodeType nodeType);

} // namespace scatha::mir

namespace scatha::mir {

/// Insulated call to `delete` on the most derived base of \p value
SCTEST_API void privateDelete(mir::Value* value);

/// Insulated call to destructor on the most derived base of \p value
SCTEST_API void privateDestroy(mir::Value* value);

/// \overload
SCTEST_API void privateDelete(mir::Instruction* inst);

/// \overload
SCTEST_API void privateDestroy(mir::Instruction* inst);

} // namespace scatha::mir

/// Map enum `NodeType` to actual node types
#define SC_MIR_CFGNODE_DEF(Node, Abstractness)                                 \
    SC_DYNCAST_MAP(::scatha::mir::Node,                                        \
                   ::scatha::mir::NodeType::Node,                              \
                   Abstractness)
#include "MIR/Lists.def"

namespace scatha::mir {

/// Enum listing all instructions in the MIR module.
enum class InstCode : uint16_t {
#define SC_MIR_INSTRUCTION_DEF(type, name) type,
#include "MIR/Lists.def"
    _count
};

/// Convert \p code to string
std::string_view toString(InstCode code);

std::ostream& operator<<(std::ostream& ostream, InstCode code);

/// \Returns `true` if \p code is a terminator instruction.
bool isTerminator(InstCode code);

/// \Returns `true` if \p code is a jump or conditional jump instruction.
bool isJump(InstCode code);

using ir::ArithmeticOperation;
using ir::CompareMode;
using ir::CompareOperation;
using ir::Conversion;
using ir::UnaryArithmeticOperation;
using ir::Visibility;

/// Encapsules memory address representation of the VM.
class MemoryAddress {
public:
    /// Constant factor and term of the address calculation
    struct ConstantData {
        uint32_t offsetFactor;
        uint32_t offsetTerm;
    };

    explicit MemoryAddress(Register* addrReg,
                           Register* offsetReg,
                           uint32_t offsetFactor,
                           uint32_t offsetTerm):
        MemoryAddress(addrReg, offsetReg, { offsetFactor, offsetTerm }) {}

    explicit MemoryAddress(Register* addrReg,
                           Register* offsetReg,
                           ConstantData constData):
        addrReg(addrReg), offsetReg(offsetReg), constData(constData) {}

    explicit MemoryAddress(Register* addrReg, uint32_t offsetTerm = 0):
        MemoryAddress(addrReg, nullptr, 0, offsetTerm) {}

    /// \Returns The register that holds the base address
    Register* addressRegister() { return addrReg; }

    /// \overload
    Register const* addressRegister() const { return addrReg; }

    /// \Returns The register that holds the offset factor or `nullptr` if none
    /// is present
    Register* offsetRegister() { return offsetReg; }

    /// \overload
    Register const* offsetRegister() const { return offsetReg; }

    /// \Returns The constant data i.e. offset factor and offset term
    ConstantData constantData() const { return constData; }

    /// \Returns The constant offset factor
    uint32_t offsetFactor() { return constData.offsetFactor; }

    /// \Returns The constant offset term
    uint32_t offsetTerm() { return constData.offsetTerm; }

private:
    Register* addrReg;
    Register* offsetReg;
    ConstantData constData;
};

/// Represents the address of an external function.
struct ExtFuncAddress {
    uint32_t slot  : 11;
    uint32_t index : 21;
};

/// `InstData` for call instructions.
struct CallInstData {
    uint32_t regOffset;

    /// Only used by `callext` instructions.
    ExtFuncAddress extFuncAddress;
};

static_assert(sizeof(CallInstData) == 8,
              "Must fit into `instdata` field of Instruction class");

} // namespace scatha::mir

#endif // SCATHA_MIR_FWD_H_
