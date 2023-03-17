// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_MODULE_H_
#define SCATHA_IR_MODULE_H_

#include <span>

#include <utl/hashset.hpp>

#include <scatha/Basic/Basic.h>
#include <scatha/Basic/OpaqueRange.h>
#include <scatha/IR/Common.h>
#include <scatha/IR/List.h>
#include <scatha/IR/Type.h>

namespace scatha::ir {

class SCATHA(API) Module {
public:
    Module()                  = default;
    Module(Module const& rhs) = delete;
    Module(Module&& rhs) noexcept;
    Module& operator=(Module const& rhs) = delete;
    Module& operator=(Module&& rhs) noexcept;
    ~Module();

    auto structures() {
        return makeOpaqueRange(structs);
    }
    
    auto structures() const {
        return makeOpaqueRange(structs);
    }
    
    StructureType* findStructure(std::string_view name) {
        auto* res = static_cast<Module const*>(this)->findStructure(name);
        return const_cast<StructureType*>(res);
    }
    
    StructureType const* findStructure(std::string_view name) const {
        auto itr = structs.find(name);
        return itr != structs.end() ? *itr : nullptr;
    }
    
    auto& functions() { return funcs; }
    auto const& functions() const { return funcs; }

    void addStructure(StructureType* structure) { structs.insert(structure); }
    void addFunction(Function* function);

private:
    utl::hashset<StructureType*, Type::Hash, Type::Equals> structs;
    List<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_MODULE_H_
