#include "IR/Type.h"

#include <sstream>

using namespace scatha;

std::string ir::FunctionType::makeName(Type const* returnType, std::span<Type const* const> parameterTypes) {
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

void ir::StructureType::computeSizeAndAlign() {
    setSize(0);
    setAlign(0);
    _memberOffsets.clear();
    for (auto* member: _members) {
        setAlign(std::max(align(), member->align()));
        size_t const currentBaseSize = utl::round_up(size(), member->align());
        _memberOffsets.push_back(utl::narrow_cast<u16>(currentBaseSize));
        setSize(currentBaseSize + member->size());
    }
    setSize(utl::round_up(size(), align()));
}
