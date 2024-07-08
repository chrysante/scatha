#ifndef SCATHA_IRGEN_MAPS_H_
#define SCATHA_IRGEN_MAPS_H_

#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/Metadata.h"
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

    /// Retrieve value associated with \p object
    Value operator()(sema::Object const* object) const;

    /// Try to retrieve value associated with \p object
    std::optional<Value> tryGet(sema::Object const* object) const;

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

/// Maps global sema objects to IR objects
class GlobalMap {
public:
    /// Associate \p semaFn with \p irFn and \p metadata
    void insert(sema::Function const* semaFn, FunctionMetadata metadata);

    /// Associate \p semaVar with \p metadata
    void insert(sema::Variable const* semaVar, GlobalVarMetadata metadata);

    /// Retrieve IR function metadata associated with \p function
    FunctionMetadata operator()(sema::Function const* function) const;

    /// Try to retrieve IR function metadata associated with \p function
    std::optional<FunctionMetadata> tryGet(
        sema::Function const* function) const;

    /// Retrieve global variable metadata associated with \p var
    GlobalVarMetadata operator()(sema::Variable const* var) const;

    /// Try to retrieve global variable metadata associated with \p var
    std::optional<GlobalVarMetadata> tryGet(sema::Variable const* var) const;

private:
    utl::hashmap<sema::Function const*, FunctionMetadata> functions;
    utl::hashmap<sema::Variable const*, GlobalVarMetadata> vars;
};

/// Maps sema types to IR types
class TypeMap {
public:
    explicit TypeMap(ir::Context& ctx): ctx(&ctx) {}

    /// Inserts as packed representation
    void insert(sema::RecordType const* key, ir::StructType const* value,
                RecordMetadata metadata);

    ///
    utl::small_vector<ir::Type const*, 2> map(ValueRepresentation repr,
                                              sema::Type const* type) const;

    /// Translate \p type to corresponding packed IR type
    ir::Type const* packed(sema::Type const* type) const;

    /// Translate \p type to corresponding unpacked IR types
    utl::small_vector<ir::Type const*, 2> unpacked(
        sema::Type const* type) const;

    ///
    utl::small_vector<ir::Type const*, 2> unpacked(sema::QualType type) const;

    /// \Returns the meta data associated with \p type
    RecordMetadata const& metadata(sema::Type const* type) const;

private:
    template <ValueRepresentation Repr>
    auto compute(sema::Type const* type) const;

    ir::Context* ctx;
    /// Mutable to cache results in const getter functions
    mutable utl::hashmap<sema::Type const*, ir::Type const*> packedMap;
    mutable utl::hashmap<sema::Type const*,
                         utl::small_vector<ir::Type const*, 2>>
        unpackedMap;
    utl::hashmap<sema::RecordType const*, RecordMetadata> meta;
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

ir::Visibility mapVisibility(sema::Entity const* entity);

/// \Returns an appropriate name for the result of the binary operation \p op
std::string binaryOpResultName(ast::BinaryOperator op);

///
std::optional<ir::Conversion> mapArithmeticConv(
    sema::ObjectTypeConversion conv);

///
std::string arithmeticConvName(sema::ObjectTypeConversion conv);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_MAPS_H_
