#ifndef SCATHA_IRGEN_MAPS_H_
#define SCATHA_IRGEN_MAPS_H_

#include <utl/hashtable.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/MetaData.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"

namespace scatha::irgen {

/// Maps Sema types to IR types
class TypeMap {
public:
    explicit TypeMap(ir::Context& ctx): ctx(&ctx) {}

    ///
    void insert(sema::StructureType const* key,
                ir::StructureType const* value,
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
    utl::hashmap<sema::StructureType const*, StructMetaData> meta;
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
