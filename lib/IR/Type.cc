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
    for (auto* member: _members) {
        setAlign(std::max(align(), member->align()));
        setSize(utl::round_up(size(), member->align()) + member->size());
    }
    setSize(utl::round_up(size(), align()));
}
