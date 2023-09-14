#include "IR/Type.h"

#include <sstream>

using namespace scatha;
using namespace ir;

void ir::privateDelete(ir::Type* type) {
    visit(*type, [](auto& derived) { delete &derived; });
}

void ir::privateDestroy(ir::Type* type) {
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

Type const* RecordType::elementAt(std::size_t index) const {
    return visit(*this, [=](auto& type) { return type.elementAtImpl(index); });
}

size_t RecordType::offsetAt(std::size_t index) const {
    return visit(*this, [=](auto& type) { return type.offsetAtImpl(index); });
}

RecordType::Member RecordType::memberAt(size_t index) const {
    return visit(*this, [=](auto& type) { return type.memberAtImpl(index); });
}

size_t RecordType::numElements() const {
    return visit(*this, [=](auto& type) { return type.numElementsImpl(); });
}

void StructType::computeSizeAndAlign() {
    setSize(0);
    setAlign(0);
    for (auto& [type, offset]: _members) {
        setAlign(std::max(align(), type->align()));
        offset = utl::round_up(size(), type->align());
        setSize(offset + type->size());
    }
    /// Empty types have a size of 1 to give objects address identity (and also
    /// because it solves many issues)
    setSize(size() == 0 ? 1 : utl::round_up(size(), align()));
}

ArrayType::ArrayType(Type const* elementType, size_t count):
    RecordType(utl::strcat("[", elementType->name(), ",", count, "]"),
               TypeCategory::ArrayType,
               count * elementType->size(),
               elementType->align()),
    _elemType(elementType),
    _count(count) {}
