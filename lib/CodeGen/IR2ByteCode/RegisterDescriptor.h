#ifndef SCATHA_CODEGEN_IR2BYTECODE_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN_IR2BYTECODE_REGISTERDESCRIPTOR_H_

#include <memory>
#include <string>

#include <utl/hashmap.hpp>

#include "Assembly/Value.h"
#include "IR/Common.h"

namespace scatha::cg {

class RegisterDescriptor {
public:
    Asm::Value resolve(ir::Value const&);

    Asm::MemoryAddress resolveAddr(ir::Value const&);

    Asm::RegisterIndex makeTemporary();

    Asm::RegisterIndex allocateAutomatic(size_t numRegisters);

    size_t numUsedRegisters() const { return index; }

private:
    size_t index = 0;
    utl::hashmap<std::string, size_t> values;
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_IR2BYTECODE_REGISTERDESCRIPTOR_H_
