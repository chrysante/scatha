#ifndef SCATHA_IRGEN_MAPS_H_
#define SCATHA_IRGEN_MAPS_H_

#include <utl/hashtable.hpp>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "Sema/Fwd.h"
#include "Sema/QualType.h"

namespace scatha::irgen {

/// Maps Sema types to IR types
class TypeMap {
public:
    explicit TypeMap(ir::Context& ctx): ctx(&ctx) {}

    ///
    void insert(sema::Type const* key, ir::Type const* value);

    /// Shorthand for `get(type.get())`
    ir::Type const* get(sema::QualType type) const;

    /// Translate \p type to corresponding IR type
    ir::Type const* get(sema::Type const* type) const;

private:
    ir::Type const* getImpl(sema::Type const* type) const;

    ir::Context* ctx;
    /// Mutable to cache results in const getter functions
    mutable utl::hashmap<sema::Type const*, ir::Type const*> map;
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
