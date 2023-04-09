#ifndef SCATHA_MIR_FWD_H_
#define SCATHA_MIR_FWD_H_

#include <iosfwd>
#include <string_view>

#include "IR/Common.h"
#include "Basic/Basic.h"
#include "Common/Dyncast.h"

namespace scatha::mir {

class Module;

///
/// ```
/// Value
/// ├─ Register
/// │  ├─ Parameter
/// │  └─ Instruction
/// ├─ Constant
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

namespace scatha::internal {

/// Insulated call to `delete` on the most derived base of \p *value
SCATHA_TESTAPI void privateDelete(mir::Value* value);

/// Insulated call to destructor on the most derived base of \p *value
SCATHA_TESTAPI void privateDestroy(mir::Value* value);

} // namespace scatha::internal

/// Map enum `NodeType` to actual node types
#define SC_MIR_CFGNODE_DEF(Node, Abstractness)                                 \
    SC_DYNCAST_MAP(::scatha::mir::Node,                                        \
                   ::scatha::mir::NodeType::Node,                              \
                   Abstractness)
#include "MIR/Lists.def"

namespace scatha::mir {

/// Enum listing all instructions in the MIR module.
enum class InstCode {
#define SC_MIR_INSTRUCTION_DEF(type, name) type,
#include "MIR/Lists.def"
    _count
};

std::string_view toString(InstCode code);

std::ostream& operator<<(std::ostream& ostream, InstCode code);

using ir::Conversion;

using ir::CompareMode;

using ir::CompareOperation;

using ir::UnaryArithmeticOperation;

using ir::ArithmeticOperation;

///
///
///
class MemoryAddress {
public:
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

    Register* addressRegister() { return addrReg; }

    Register const* addressRegister() const { return addrReg; }

    Register* offsetRegister() { return offsetReg; }

    Register const* offsetRegister() const { return offsetReg; }

    ConstantData constantData() const { return constData; }

    uint32_t offsetFactor() { return constData.offsetFactor; }

    uint32_t offsetTerm() { return constData.offsetTerm; }

private:
    Register* addrReg;
    Register* offsetReg;
    ConstantData constData;
};

///
///
///
struct ExtFuncAddress {
    uint32_t slot;
    uint32_t index;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_FWD_H_
