#include "IR/Context.h"

#include <sstream>

#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/CFG/Constants.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"
#include "IR/VectorHash.h"

using namespace scatha;
using namespace ir;
using namespace ranges::views;

using StructKey = utl::small_vector<Type const*>;
using StructHash = VectorHash<Type const*>;
using StructEqual = VectorEqual<Type const*>;

using ArrayKey = std::pair<Type const*, size_t>;

namespace {

/// Helper class to store struct and array constants
struct RecordConstantMap {
    using RecordConstantKey = utl::small_vector<Constant*>;
    using RecordConstantHash = VectorHash<Constant*>;
    using RecordConstantEqual = VectorEqual<Constant*>;

    template <typename ConstType, typename IRType>
    ConstType* get(IRType const* type, std::span<Constant* const> elems) {
        auto itr = map.find(elems);
        if (itr == map.end()) {
            itr = map.insert({ elems | ToSmallVector<>,
                               allocate<ConstType>(elems, type) })
                      .first;
        }
        return cast<ConstType*>(itr->second.get());
    }

    utl::hashmap<RecordConstantKey, UniquePtr<RecordConstant>,
                 RecordConstantHash, RecordConstantEqual>
        map;
};

} // namespace

struct Context::Impl {
    /// ## Constants
    /// ** Bitwidth must appear before the value, because comparison of values
    /// of different widths may not be possible. **
    utl::hashmap<std::pair<size_t, APInt>, UniquePtr<IntegralConstant>>
        _integralConstants;
    utl::hashmap<std::pair<size_t, APFloat>, UniquePtr<FloatingPointConstant>>
        _floatConstants;
    utl::hashmap<Type const*, UniquePtr<UndefValue>> _undefConstants;
    utl::hashmap<RecordType const*, RecordConstantMap> recordConstants;
    UniquePtr<NullPointerConstant> nullptrConstant;

    /// ## Types
    utl::vector<UniquePtr<Type>> _types;
    VoidType const* _voidType;
    PointerType const* _ptrType;
    utl::hashmap<uint32_t, IntegralType const*> _intTypes;
    utl::hashmap<uint32_t, FloatType const*> _floatTypes;
    utl::hashmap<StructKey, StructType const*, StructHash, StructEqual>
        _anonymousStructs;
    utl::hashmap<ArrayKey, ArrayType const*> _arrayTypes;
};

Context::Context(): impl(std::make_unique<Impl>()) {
    auto vt = allocate<VoidType>();
    impl->_voidType = vt.get();
    impl->_types.push_back(std::move(vt));
    auto pt = allocate<PointerType>();
    impl->_ptrType = pt.get();
    impl->_types.push_back(std::move(pt));
    impl->nullptrConstant = allocate<NullPointerConstant>(ptrType());
    impl->nullptrConstant->setPointerInfo(
        { .align = 32, // Max align value
          .validSize = 0,
          .provenance = PointerProvenance::Static(impl->nullptrConstant.get()),
          .staticProvenanceOffset = 0 });
}

Context::Context(Context&&) noexcept = default;

Context& Context::operator=(Context&&) noexcept = default;

Context::~Context() = default;

VoidType const* Context::voidType() { return impl->_voidType; }

PointerType const* Context::ptrType() { return impl->_ptrType; }

template <typename A>
static auto* getArithmeticType(size_t bitwidth, auto& types, auto& map) {
    auto itr = map.find(bitwidth);
    if (itr != map.end()) {
        return itr->second;
    }
    auto type = allocate<A>(bitwidth);
    itr = map.insert({ bitwidth, type.get() }).first;
    types.push_back(std::move(type));
    return itr->second;
}

IntegralType const* Context::intType(size_t bitwidth) {
    return getArithmeticType<IntegralType>(bitwidth, impl->_types,
                                           impl->_intTypes);
}

IntegralType const* Context::boolType() { return intType(1); }

FloatType const* Context::floatType(size_t bitwidth) {
    SC_ASSERT(bitwidth == 32 || bitwidth == 64, "Other sizes not supported");
    return getArithmeticType<FloatType>(bitwidth, impl->_types,
                                        impl->_floatTypes);
}

FloatType const* Context::floatType(APFloatPrec precision) {
    return floatType(precision.totalBitwidth());
}

static std::string makeAnonStructName(std::span<Type const* const> members) {
    std::stringstream sstr;
    sstr << "{ ";
    for (bool first = true; auto* type: members) {
        if (!first) {
            sstr << ", ";
        }
        first = false;
        sstr << type->name();
    }
    sstr << " }";
    return std::move(sstr).str();
}

StructType const* Context::anonymousStruct(
    std::span<Type const* const> members) {
    SC_EXPECT(ranges::all_of(members, [](auto* ty) { return !!ty; }));
    auto itr = impl->_anonymousStructs.find(members);
    if (itr != impl->_anonymousStructs.end()) {
        return itr->second;
    }
    auto type = allocate<StructType>(makeAnonStructName(members));
    type->setAnonymous();
    for (auto* member: members) {
        type->pushMember(member);
    }
    auto* result = type.get();
    impl->_types.push_back(std::move(type));
    impl->_anonymousStructs.insert({ members | ranges::to<StructKey>, result });
    return result;
}

ArrayType const* Context::arrayType(Type const* elementType, size_t count) {
    ArrayKey key = { elementType, count };
    auto itr = impl->_arrayTypes.find(key);
    if (itr != impl->_arrayTypes.end()) {
        return itr->second;
    }
    auto type = allocate<ArrayType>(elementType, count);
    itr = impl->_arrayTypes.insert({ key, type.get() }).first;
    impl->_types.push_back(std::move(type));
    return itr->second;
}

ArrayType const* Context::byteArrayType(size_t count) {
    return arrayType(intType(8), count);
}

IntegralConstant* Context::intConstant(APInt value) {
    size_t const bitwidth = value.bitwidth();
    auto itr = impl->_integralConstants.find({ bitwidth, value });
    if (itr == impl->_integralConstants.end()) {
        std::tie(itr, std::ignore) = impl->_integralConstants.insert(
            { std::pair{ bitwidth, value },
              allocate<IntegralConstant>(*this, value) });
    }
    SC_ASSERT(ucmp(itr->second->value(), value) == 0, "Value mismatch");
    return itr->second.get();
}

IntegralConstant* Context::intConstant(u64 value, size_t bitwidth) {
    return intConstant(APInt(value, bitwidth));
}

IntegralConstant* Context::boolConstant(bool value) {
    return intConstant(value, 1);
}

FloatingPointConstant* Context::floatConstant(APFloat value) {
    size_t const bitwidth = value.precision().totalBitwidth();
    auto itr = impl->_floatConstants.find({ bitwidth, value });
    if (itr == impl->_floatConstants.end()) {
        std::tie(itr, std::ignore) = impl->_floatConstants.insert(
            { std::pair{ bitwidth, value },
              allocate<FloatingPointConstant>(*this, value) });
    }
    SC_ASSERT(itr->second->value() == value, "Value mismatch");
    return itr->second.get();
}

FloatingPointConstant* Context::floatConstant(double value, size_t bitwidth) {
    switch (bitwidth) {
    case 32:
        return floatConstant(APFloat(value, APFloatPrec::Single()));
    case 64:
        return floatConstant(APFloat(value, APFloatPrec::Double()));
    default:
        SC_UNREACHABLE();
    }
}

Constant* Context::arithmeticConstant(int64_t value, Type const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [&](IntegralType const& type) {
            return intConstant(static_cast<uint64_t>(value),
                                    type.bitwidth());
        },
        [&](FloatType const& type) {
            return floatConstant(value, type.bitwidth());
        },
        [&](Type const&) -> Constant* { SC_UNREACHABLE(); }
    }; // clang-format on
}

Constant* Context::arithmeticConstant(APInt value) {
    return intConstant(value);
}

Constant* Context::arithmeticConstant(APFloat value) {
    return floatConstant(value);
}

RecordConstant* Context::recordConstant(std::span<Constant* const> elems,
                                        RecordType const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [&](StructType const& type) -> RecordConstant* {
            return structConstant(elems, &type);
        },
        [&](ArrayType const& type) -> RecordConstant* {
            return arrayConstant(elems, &type);
        },
    }; // clang-format on
}

RecordConstant* Context::addRecordConstant(UniquePtr<RecordConstant> constant) {
    if (!constant->type()) {
        return nullptr;
    }
    auto& map = impl->recordConstants[constant->type()].map;
    auto key = constant->elements() | ToSmallVector<>;
    auto itr = map.find(key);
    if (itr != map.end()) {
        auto* repl = itr->second.get();
        constant->replaceAllUsesWith(repl);
        return repl;
    }
    else {
        auto* result = constant.get();
        map.insert({ std::move(key), std::move(constant) });
        return result;
    }
}

template <typename ConstType, typename IRType>
static ConstType* recordConstantImpl(auto& constantMap, IRType const* type,
                                     std::span<Constant* const> elems) {
    return constantMap[type].template get<ConstType>(type, elems);
}

StructConstant* Context::structConstant(std::span<Constant* const> elems,
                                        StructType const* type) {
    return recordConstantImpl<StructConstant>(impl->recordConstants, type,
                                              elems);
}

StructConstant* Context::anonymousStructConstant(
    std::span<Constant* const> elems) {
    auto* type =
        anonymousStruct(elems | transform(&Value::type) | ToSmallVector<>);
    return structConstant(elems, type);
}

ArrayConstant* Context::arrayConstant(std::span<Constant* const> elems,
                                      ArrayType const* type) {
    return recordConstantImpl<ArrayConstant>(impl->recordConstants, type,
                                             elems);
}

ArrayConstant* Context::stringLiteral(std::string_view text) {
    auto* type = arrayType(intType(8), text.size());
    auto elems = text | ranges::views::transform([&](char c) -> Constant* {
        return intConstant(static_cast<unsigned>(c), 8);
    }) | ToSmallVector<>;
    return arrayConstant(elems, type);
}

NullPointerConstant* Context::nullpointer() {
    return impl->nullptrConstant.get();
}

Constant* Context::nullConstant(Type const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [&](ArithmeticType const& type) {
            return arithmeticConstant(0, &type);
        },
        [&](PointerType const&) {
            return nullpointer();
        },
        [&](RecordType const& type) {
            auto elems = type.elements() |
                         ranges::views::transform([&](auto* type) {
                             return nullConstant(type);
                         }) | ToSmallVector<>;
            return recordConstant(elems, &type);
        },
        [&](Type const&) -> Constant* {
            SC_UNREACHABLE();
        },
    }; // clang-format on
}

UndefValue* Context::undef(Type const* type) {
    auto itr = impl->_undefConstants.find(type);
    if (itr == impl->_undefConstants.end()) {
        bool success = false;
        std::tie(itr, success) =
            impl->_undefConstants.insert({ type, allocate<UndefValue>(type) });
        SC_ASSERT(success, "");
    }
    return itr->second.get();
}

Value* Context::voidValue() { return undef(voidType()); }

bool Context::isCommutative(ArithmeticOperation op) const {
    return ir::isCommutative(op);
}

bool Context::isAssociative(ArithmeticOperation op) const {
    switch (op) {
    case ArithmeticOperation::Add:
    case ArithmeticOperation::Mul:
    case ArithmeticOperation::And:
    case ArithmeticOperation::Or:
    case ArithmeticOperation::XOr:
        return true;

    case ArithmeticOperation::FAdd:
    case ArithmeticOperation::FMul:
        return associativeFloatArithmetic();

    default:
        return false;
    }
}

bool Context::cleanConstants() {
    auto recordConstants = impl->recordConstants | values |
                           transform(&RecordConstantMap::map) | join | values |
                           ToAddress | ToSmallVector<>;
    utl::topsort(recordConstants.begin(), recordConstants.end(),
                 [](auto* value) {
        return value->operands() | Filter<RecordConstant>;
    });
    for (auto* value: recordConstants | reverse | filter(&Value::unused)) {
        auto itr = impl->recordConstants.find(value->type());
        SC_ASSERT(itr != impl->recordConstants.end(), "");
        auto& map = itr->second.map;
        auto key = value->operands() | transform(cast<Constant*>) |
                   ToSmallVector<>;
        bool success = map.erase(key) == 1;
        SC_ASSERT(success, "");
    }
    return false;
}
