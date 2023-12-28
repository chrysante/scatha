#include "Runtime/Library.h"

#include <scatha/Sema/Entity.h>
#include <scatha/Sema/SymbolTable.h>

using namespace scatha;

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
