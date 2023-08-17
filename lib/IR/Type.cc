#include "IR/Type.h"

#include <sstream>

using namespace scatha;
using namespace ir;

void scatha::internal::privateDelete(ir::Type* type) {
    visit(*type, [](auto& derived) { delete &derived; });
}

void scatha::internal::privateDestroy(ir::Type* type) {
    visit(*type, [](auto& derived) { std::destroy_at(&derived); });
}

std::string FunctionType::makeName(
    Type const* returnType, std::span<Type const* const> parameterTypes) {
    std::stringstream sstr;
    sstr << returnType->name();
    sstr << "(";
    for (bool first = true; auto* paramType: parameterTypes) {
        sstr << (first ? (first = false), "" : ", ");
        sstr << paramType->name();
    }
    sstr << ")";
    return std::move(sstr).str();
}

void StructureType::computeSizeAndAlign() {
    setSize(0);
    setAlign(0);
    _memberOffsets.clear();
    for (auto* member: _members) {
        setAlign(std::max(align(), member->align()));
        size_t const currentBaseSize = utl::round_up(size(), member->align());
        _memberOffsets.push_back(utl::narrow_cast<u16>(currentBaseSize));
        setSize(currentBaseSize + member->size());
    }
    /// Empty types have a size of 1 to give objects address identity (and also
    /// because it solves many issues)
    setSize(size() == 0 ? 1 : utl::round_up(size(), align()));
}

ArrayType::ArrayType(Type const* elementType, size_t count):
    Type(utl::strcat("[", elementType->name(), ",", count, "]"),
         TypeCategory::ArrayType,
         count * elementType->size(),
         elementType->align()),
    _elemType(elementType),
    _count(count) {}
