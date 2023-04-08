#ifndef SCATHA_CODEGEN_CODEGEN2_ADDRESSDESCRIPTOR_H_
#define SCATHA_CODEGEN_CODEGEN2_ADDRESSDESCRIPTOR_H_

#include <memory>
#include <optional>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Assembly/Value.h"
#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::cg {

class Address {
public:
    Address() = default;
    explicit Address(Asm::Value lit): lit(lit) {}
    explicit Address(Asm::RegisterIndex reg): reg(reg) {}
    explicit Address(Asm::MemoryAddress mem): mem(mem) {}

    Asm::Value get() const;

    void setRegister(Asm::RegisterIndex reg) { this->reg = reg; }

    void spill() { reg = std::nullopt; }

    void setMemory(Asm::MemoryAddress mem) { this->mem = mem; }

    bool empty() const;

private:
    std::optional<Asm::Value> lit;
    std::optional<Asm::RegisterIndex> reg;
    std::optional<Asm::MemoryAddress> mem;
};

struct ResolveResult {
    Asm::RegisterIndex dest;
    utl::small_vector<Asm::Value, 3> operands;
};

class UserMap {
public:
    void addInstruction(ir::Instruction const* inst);

    /// \Returns `true` iff \p inst is now unused.
    bool removeUser(ir::Instruction const* inst, ir::Instruction const* user);

private:
    using UserSet = utl::hashmap<ir::Instruction const*, uint16_t>;
    utl::hashmap<ir::Instruction const*, UserSet> map;
};

class SCATHA_TESTAPI AddressDescriptor {
public:
    explicit AddressDescriptor(size_t totalRegisters):
        totalRegisters(totalRegisters) {}

    /// `resolve` shall be called exactly once for every instruction during
    /// dispatch.
    ResolveResult resolve(ir::Instruction const* inst);

private:
    Address* find(ir::Value const* value);

    size_t totalRegisters = 15;
    size_t numUsedRegs    = 0;
    utl::hashmap<ir::Value const*, Address*> addressMap;
    utl::hashmap<ir::Instruction const*, utl::hashset<ir::Instruction const*>>
        userMap;
    utl::vector<std::unique_ptr<Address>> addressBag;
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_CODEGEN2_ADDRESSDESCRIPTOR_H_
