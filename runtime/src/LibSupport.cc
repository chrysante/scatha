#include "Runtime/LibSupport.h"

#include <map>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include <scatha/Sema/SymbolTable.h>
#include <svm/VirtualMachine.h>

using namespace scatha;
using namespace ranges::views;

Library::Library(sema::SymbolTable& sym, size_t slot): sym(&sym), _slot(slot) {
    mapType(typeid(void), sym.Void());
    mapType(typeid(bool), sym.Bool());
    mapType(typeid(int8_t), sym.S8());
    mapType(typeid(int16_t), sym.S16());
    mapType(typeid(int32_t), sym.S32());
    mapType(typeid(int64_t), sym.S64());
    mapType(typeid(uint8_t), sym.U8());
    mapType(typeid(uint16_t), sym.U16());
    mapType(typeid(uint32_t), sym.U32());
    mapType(typeid(uint64_t), sym.U64());
    mapType(typeid(float), sym.F32());
    mapType(typeid(double), sym.F64());
}

sema::StructType const* Library::declareType(StructDesc desc) {
    auto* type = sym->declareStructureType(desc.name);
    assert(false && "Unimplemented");
    return type;
}

FuncDecl Library::declareFunction(std::string name,
                                  sema::FunctionSignature signature) {
    size_t slot = _slot;
    size_t index = _index++;
    auto* function = sym->declareForeignFunction(name,
                                                 slot,
                                                 index,
                                                 std::move(signature),
                                                 sema::FunctionAttribute::None);
    return { .name = name,
             .function = function,
             .address = { .slot = slot, .index = index } };
}

sema::Type const* Library::mapType(std::type_info const& key,
                                   sema::Type const* value) {
    auto [itr, success] = typemap.insert({ key, value });
    assert(success);
    return itr->second;
}

sema::Type const* Library::mapType(std::type_info const& key,
                                   StructDesc valueDesc) {
    return mapType(key, declareType(std::move(valueDesc)));
}

sema::Type const* Library::getType(std::type_info const& key) const {
    auto itr = typemap.find(key);
    if (itr != typemap.end()) {
        return itr->second;
    }
    return nullptr;
}

void scatha::defineFunction(svm::VirtualMachine& vm,
                            FuncAddress address,
                            std::string name,
                            InternalFuncPtr impl,
                            void* userptr) {
    vm.setFunction(address.slot,
                   address.index,
                   { std::move(name), impl, userptr });
}

std::vector<std::function<internal::DeclPair(Library&)>>
    internal::globalLibDecls;

std::vector<std::function<internal::DefPair()>> internal::globalLibDefines;

extern "C" void internal_declareFunctions(void* libptr) {
    using namespace internal;
    auto& lib = *static_cast<Library*>(libptr);
    auto decls = globalLibDecls | transform([&](auto& f) { return f(lib); }) |
                 ranges::to<std::vector>;
    ranges::sort(decls, ranges::less{}, &DeclPair::first);
    for (auto& [name, sig]: decls) {
        lib.declareFunction(std::move(name), std::move(sig));
    }
}

extern "C" void internal_defineFunctions(void* vmptr, size_t slot) {
    using namespace internal;
    auto defs = globalLibDefines | transform([&](auto& f) { return f(); }) |
                ranges::to<std::vector>;
    ranges::sort(defs, ranges::less{}, &DefPair::first);
    auto& vm = *static_cast<svm::VirtualMachine*>(vmptr);
    auto functions = defs | transform([](auto& p) {
                         return svm::ExternalFunction{ p.first, p.second };
                     }) |
                     ranges::to<std::vector>;
    vm.setFunctionTableSlot(slot, std::move(functions));
}
