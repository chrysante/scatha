#ifndef SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_
#define SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_

#include <optional>

#include <utl/hashmap.hpp>

#include "Assembly/Assembly.h"
#include "Basic/Basic.h"
#include "IC/ThreeAddressCode.h"
#include "Sema/SymbolID.h"

namespace scatha::codegen {

class RegisterDescriptor {
public:
    void declareParameters(ic::FunctionLabel const&);

    assembly::RegisterIndex resolve(ic::Variable const&);
    assembly::RegisterIndex resolve(ic::Temporary const&);
    std::optional<assembly::RegisterIndex> resolve(ic::TasArgument const&);

    assembly::RegisterIndex makeTemporary();

    void markUsed(size_t count);

    void clear();

    size_t numUsedRegisters() const { return index; }

    bool empty() const;

private:
    size_t index = 0;
    utl::hashmap<sema::SymbolID, size_t> variables;
    utl::hashmap<size_t, size_t> temporaries;
};

} // namespace scatha::codegen

#endif // SCATHA_CODEGEN_REGISTERDESCRIPTOR_H_
