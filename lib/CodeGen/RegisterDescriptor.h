#ifndef SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

#include <string>
#include <memory>

#include <utl/hashmap.hpp>

#include "Assembly/Value.h"
#include "IR/CFGCommon.h"

namespace scatha::cg2 {
	
class RegisterDescriptor {
public:
    asm2::Value resolve(ir::Value const&);
    
    asm2::MemoryAddress resolveAddr(ir::Value const&);
    
    asm2::RegisterIndex makeTemporary();
    
    asm2::RegisterIndex allocateAutomatic(size_t numRegisters);
    
    size_t numUsedRegisters() const { return index; }
    
private:
    size_t index = 0;
    utl::hashmap<std::string, size_t> values;
};
	
} // namespace scatha::cg2

#endif // SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

