#include <scatha/Runtime/Function.h>

#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

#define SYMTABLE (*static_cast<sema::SymbolTable*>(sym))

void const* scatha::VoidType(void* sym) { return SYMTABLE.qVoid(); }

void const* scatha::IntType(void* sym, size_t width) {
    return SYMTABLE.qualify(SYMTABLE.intType(width, sema::Signedness::Signed));
}

void const* scatha::UIntType(void* sym, size_t width) {
    return SYMTABLE.qualify(
        SYMTABLE.intType(width, sema::Signedness::Unsigned));
}

void const* scatha::FloatType(void* sym, size_t width) { throw; }

void const* scatha::RefType(void* sym, void const* base) {
    return SYMTABLE.setReference(static_cast<sema::QualType const*>(base),
                                 sema::RefConstExpl);
}

void const* scatha::MutRefType(void* sym, void const* base) {
    return SYMTABLE.setReference(static_cast<sema::QualType const*>(base),
                                 sema::RefMutExpl);
}
