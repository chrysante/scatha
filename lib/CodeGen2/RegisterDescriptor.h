#ifndef SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

#include <string>

#include <utl/hashmap.hpp>

#include "Assembly/Assembly.h"
#include "IR/CFGCommon.h"

namespace scatha::cg2 {
	
class RegisterDescriptor {
public:
    assembly::RegisterIndex resolve(ir::Value const&);
    
    assembly::RegisterIndex makeTemporary();

    size_t numUsedRegisters() const { return index; }
    
private:
    size_t index = 0;
    utl::hashmap<std::string, size_t> values;
};
	
} // namespace scatha::cg2

#endif // SCATHA_CODEGEN2_REGISTERDESCRIPTOR_H_

