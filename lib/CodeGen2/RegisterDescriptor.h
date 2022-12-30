#ifndef SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

#include <string>
#include <variant>

#include <utl/hashmap.hpp>

#include "Assembly/Assembly.h"
#include "IR/CFGCommon.h"

namespace scatha::cg2 {
	
class RegisterDescriptor {
public:
    std::variant<assembly::RegisterIndex, assembly::MemoryAddress, assembly::Value64> resolve(ir::Value const&);
    assembly::MemoryAddress resolveAddr(ir::Value const&);
    
    assembly::RegisterIndex makeTemporary();
    
    assembly::RegisterIndex allocateAutomatic(size_t numRegisters);
    
    size_t numUsedRegisters() const { return index; }
    
private:
    size_t index = 0;
    utl::hashmap<std::string, size_t> values;
};
	
} // namespace scatha::cg2

#endif // SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

