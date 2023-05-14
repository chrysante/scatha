#include <scatha/Runtime/Common.h>

#include <sstream>

#include "CommonImpl.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

static sema::ObjectType const* toObjType(sema::SymbolTable& sym,
                                         BaseType type) {
    switch (type) {
    case BaseType::Void:
        return sym.rawVoid();
    case BaseType::Bool:
        return sym.rawBool();
    case BaseType::Byte:
        return sym.rawByte();
    case BaseType::S8:
        return sym.rawS8();
    case BaseType::S16:
        return sym.rawS16();
    case BaseType::S32:
        return sym.rawS32();
    case BaseType::S64:
        return sym.rawS64();
    case BaseType::U8:
        return sym.rawU8();
    case BaseType::U16:
        return sym.rawU16();
    case BaseType::U32:
        return sym.rawU32();
    case BaseType::U64:
        return sym.rawU64();
    case BaseType::F32:
        return sym.rawF32();
    case BaseType::F64:
        return sym.rawF64();
    }
}

sema::QualType const* scatha::toSemaType(sema::SymbolTable& sym,
                                         QualType type) {
    sema::ObjectType const* base = toObjType(sym, type.base);
    switch (type.qual) {
    case Qualifier::None:
        return sym.qualify(base);
    case Qualifier::Ref:
        return sym.qualify(base, sema::RefConstExpl);
    case Qualifier::MutRef:
        return sym.qualify(base, sema::RefMutExpl);
    case Qualifier::ArrayRef: {
        auto* array = sym.arrayType(base, sema::ArrayType::DynamicCount);
        return sym.qualify(array, sema::RefConstExpl);
    }
    case Qualifier::MutArrayRef: {
        auto* array = sym.arrayType(base, sema::ArrayType::DynamicCount);
        return sym.qualify(array, sema::RefMutExpl);
    }
    }
}

auto mapType(sema::SymbolTable& sym) {
    return [&](QualType type) { return toSemaType(sym, type); };
}

sema::FunctionSignature scatha::toSemaSig(sema::SymbolTable& sym,
                                          QualType returnType,
                                          std::span<QualType const> argTypes) {
    auto map  = mapType(sym);
    auto args = argTypes | ranges::views::transform(map) |
                ranges::to<utl::small_vector<sema::QualType const*>>;
    return sema::FunctionSignature(std::move(args), map(returnType));
}

static void mangleImpl(std::ostream& str, QualType type) {
    switch (type.qual) {
    case Qualifier::None:
        break;
    case Qualifier::Ref:
        str << "&";
        break;
    case Qualifier::MutRef:
        str << "&mut-";
        break;
    case Qualifier::ArrayRef: {
        str << "&[";
        break;
    }
    case Qualifier::MutArrayRef:
        str << "&mut-[";
        break;
    }
    switch (type.base) {
    case BaseType::Void:
        str << "void";
        break;
    case BaseType::Bool:
        str << "bool";
        break;
    case BaseType::Byte:
        str << "byte";
        break;
    case BaseType::S8:
        str << "s8";
        break;
    case BaseType::S16:
        str << "s16";
        break;
    case BaseType::S32:
        str << "s32";
        break;
    case BaseType::S64:
        str << "s64";
        break;
    case BaseType::U8:
        str << "u8";
        break;
    case BaseType::U16:
        str << "u16";
        break;
    case BaseType::U32:
        str << "u32";
        break;
    case BaseType::U64:
        str << "u64";
        break;
    case BaseType::F32:
        str << "f32";
        break;
    case BaseType::F64:
        str << "f64";
        break;
    }
    if (type.qual == Qualifier::ArrayRef || type.qual == Qualifier::MutArrayRef)
    {
        str << "]";
    }
}

std::string scatha::mangleFunctionName(std::string_view name,
                                       std::span<QualType const> args) {
    std::stringstream sstr;
    sstr << name;
    for (auto arg: args) {
        sstr << "-";
        mangleImpl(sstr, arg);
    }
    return std::move(sstr).str();
}
