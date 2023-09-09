#ifndef SCATHA_IRGEN_MAPS_H_
#define SCATHA_IRGEN_MAPS_H_

#include <functional>

#include <utl/hashtable.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/MetaData.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"

namespace scatha::irgen {

/// Maps sema entities to `irgen::Value` objects
class ValueMap {
public:
    using LazyArraySize = std::function<Value(ir::BasicBlock*)>;

    /// Associate \p object with \p value
    void insert(sema::Object const* object, Value value);

    /// Associate \p semaFn with \p irFn and \p metaData
    void insert(sema::Function const* semaFn,
                ir::Callable* irFn,
                FunctionMetaData metaData);

    /// Associate array object \p object with the size \p size
    void insertArraySize(sema::Object const* object, Value size);

    /// Associate array object \p object with the lazily evaluated size \p size
    void insertArraySize(sema::Object const* object, LazyArraySize size);

    /// Make \p newObj refer to the same array size value as \p original
    void insertArraySizeOf(sema::Object const* newObj,
                           sema::Object const* original);

    /// Retrieve value associated with \p object
    Value operator()(sema::Object const* object) const;

    /// Retrieve IR function associated with \p function
    ir::Callable* operator()(sema::Function const* function) const;

    /// Retrieve array size associated with \p object
    /// \pre \p object must be associated with an array size
    Value arraySize(sema::Object const* object, ir::BasicBlock* BB) const;

    /// Try to retrieve array size associated with \p object
    std::optional<Value> tryGetArraySize(sema::Object const* object,
                                         ir::BasicBlock* BB) const;

    /// \Returns the meta data associated with \p type
    FunctionMetaData const& metaData(sema::Function const* function) const;

private:
    utl::hashmap<sema::Entity const*, Value> values;
    utl::hashmap<sema::Function const*, ir::Callable*> functions;
    utl::hashmap<sema::Function const*, FunctionMetaData> functionMetaData;
    utl::hashmap<sema::Object const*, LazyArraySize> arraySizes;
};

/// Maps sema types to IR types
class TypeMap {
public:
    explicit TypeMap(ir::Context& ctx): ctx(&ctx) {}

    ///
    void insert(sema::StructType const* key,
                ir::StructType const* value,
                StructMetaData metaData);

    /// Shorthand for `(*this*)(type.get())`
    ir::Type const* operator()(sema::QualType type) const;

    /// Translate \p type to corresponding IR type
    ir::Type const* operator()(sema::Type const* type) const;

    /// \Returns the meta data associated with \p type
    StructMetaData const& metaData(sema::Type const* type) const;

private:
    void insertImpl(sema::Type const* key, ir::Type const* value) const;
    ir::Type const* get(sema::Type const* type) const;

    ir::Context* ctx;
    /// Mutable to cache results in const getter functions
    mutable utl::hashmap<sema::Type const*, ir::Type const*> map;
    utl::hashmap<sema::StructType const*, StructMetaData> meta;
};

ir::UnaryArithmeticOperation mapUnaryOp(ast::UnaryOperator op);

ir::CompareOperation mapCompareOp(ast::BinaryOperator op);

ir::ArithmeticOperation mapArithmeticOp(sema::BuiltinType const* type,
                                        ast::BinaryOperator op);

ir::ArithmeticOperation mapArithmeticAssignOp(sema::BuiltinType const* type,
                                              ast::BinaryOperator op);

ir::CompareMode mapCompareMode(sema::BuiltinType const* type);

ir::FunctionAttribute mapFuncAttrs(sema::FunctionAttribute attr);

ir::Visibility accessSpecToVisibility(sema::AccessSpecifier spec);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_MAPS_H_
