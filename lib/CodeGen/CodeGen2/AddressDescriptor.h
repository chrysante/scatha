#ifndef SCATHA_CODEGEN_CODEGEN2_ADDRESSDESCRIPTOR_H_
#define SCATHA_CODEGEN_CODEGEN2_ADDRESSDESCRIPTOR_H_

#include <utl/hashtable.hpp>

#include "Basic/Basic.h"
#include "IR/Common.h"
#include "Assembly/Value.h"

namespace scatha::cg {

class Address {
public:
    
private:
    std::optional<Asm::Value> lit;
    std::optional<Asm::RegisterIndex> reg;
};

class SCATHA_TESTAPI AddressDescriptor {
public:
    AddressDescriptor() = default;
    
private:
    
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_CODEGEN2_ADDRESSDESCRIPTOR_H_
