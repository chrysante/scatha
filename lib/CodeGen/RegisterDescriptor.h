#ifndef SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

#include <string>
#include <memory>

#include <utl/hashmap.hpp>

#include "Assembly/Value.h"
#include "IR/CFGCommon.h"

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

#endif // SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

