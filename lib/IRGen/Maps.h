#ifndef SCATHA_IRGEN_MAPS_H_
#define SCATHA_IRGEN_MAPS_H_

#include <functional>
#include <iosfwd>
#include <optional>

#include <utl/hashtable.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/MetaData.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"

namespace scatha::irgen {

/// Maps sema objects to `irgen::Value` objects
class ValueMap {
public:
    using LazyArraySize = std::function<Value()>;

    explicit ValueMap(ir::Context& ctx);

    /// Associate \p object with \p value
    void insert(sema::Object const* object, Value value);

    /// Associate \p object with \p value if \p object is not already in the map
    bool tryInsert(sema::Object const* object, Value value);

    /// Associate array object \p object with the size \p size
    void insertArraySize(sema::Object const* object, Value size);

    /// Associate array object \p object with the size \p size
    void insertArraySize(sema::Object const* object, size_t size);

    /// Associate array object \p object with the lazily evaluated size \p size
    void insertArraySize(sema::Object const* object, LazyArraySize size);

    /// Make \p newObj refer to the same array size value as \p original
    void insertArraySizeOf(sema::Object const* newObj,
                           sema::Object const* original);

    /// Retrieve value associated with \p object
    Value operator()(sema::Object const* object) const;

    /// Try to retrieve value associated with \p object
    std::optional<Value> tryGet(sema::Object const* object) const;

    /// Retrieve array size associated with \p object
    /// \pre \p object must be associated with an array size
    Value arraySize(sema::Object const* object) const;

    /// Try to retrieve array size associated with \p object
    std::optional<Value> tryGetArraySize(sema::Object const* object) const;

    /// Range accessors
    /// @{
    auto begin() const { return values.begin(); }
    auto end() const { return values.end(); }
    size_t size() const { return values.size(); }
    /// @}

private:
    ir::Context* ctx;
    utl::hashmap<sema::Object const*, Value> values;
    utl::hashmap<sema::Object const*, LazyArraySize> arraySizes;
};

/// Prints the value map \p valueMap to \p ostream
void print(ValueMap const& valueMap, std::ostream& ostream);

/// Prints the value map \p valueMap to `std::cout`
void print(ValueMap const& valueMap);

/// Maps sema functions to IR functions
class FunctionMap {
public:
    /// Associate \p semaFn with \p irFn and \p metaData
    void insert(sema::Function const* semaFn,
                ir::Callable* irFn,
                FunctionMetaData metaData);

    /// Retrieve IR function associated with \p function
    ir::Callable* operator()(sema::Function const* function) const;

    /// Try to retrieve IR function associated with \p function
    ir::Callable* tryGet(sema::Function const* function) const;

    /// \Returns the meta data associated with \p type
    FunctionMetaData const& metaData(sema::Function const* function) const;

private:
    utl::hashmap<sema::Function const*, ir::Callable*> functions;
    utl::hashmap<sema::Function const*, FunctionMetaData> functionMetaData;
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

    /// Translate \p type to back corresponding sema type
    sema::ObjectType const* operator()(ir::Type const* type) const;

    /// \Returns the meta data associated with \p type
    StructMetaData const& metaData(sema::Type const* type) const;

private:
    void insertImpl(sema::Type const* key, ir::Type const* value) const;
    ir::Type const* get(sema::Type const* type) const;

    ir::Context* ctx;
    /// Mutable to cache results in const getter functions
    mutable utl::hashmap<sema::Type const*, ir::Type const*> map;
    mutable utl::hashmap<ir::Type const*, sema::Type const*> backMap;
    utl::hashmap<sema::StructType const*, StructMetaData> meta;
};

/// # Maps of operators and other attributes

ir::UnaryArithmeticOperation mapUnaryOp(ast::UnaryOperator op);

ir::CompareOperation mapCompareOp(ast::BinaryOperator op);

ir::ArithmeticOperation mapArithmeticOp(sema::ObjectType const* type,
                                        ast::BinaryOperator op);

ir::ArithmeticOperation mapArithmeticAssignOp(sema::ObjectType const* type,
                                              ast::BinaryOperator op);

ir::CompareMode mapCompareMode(sema::ObjectType const* type);

ir::FunctionAttribute mapFuncAttrs(sema::FunctionAttribute attr);

ir::Visibility mapVisibility(sema::AccessControl accessControl);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_MAPS_H_
