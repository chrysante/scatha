#include "Sema/Analysis/Utility.h"

#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;

QualType sema::getQualType(Type const* type, Mutability mut) {
    if (auto* ref = dyncast<ReferenceType const*>(type)) {
        return ref->base();
    }
    return { cast<ObjectType const*>(type), mut };
}

ValueCategory sema::refToLValue(Type const* type) {
    if (isa<ReferenceType>(type)) {
        return LValue;
    }
    return RValue;
}

ast::Statement* sema::parentStatement(ast::ASTNode* node) {
    while (true) {
        if (!node) {
            return nullptr;
        }
        if (auto* stmt = dyncast<ast::Statement*>(node)) {
            return stmt;
        }
        node = node->parent();
    }
}

StructType const* sema::nonTrivialLifetimeType(ObjectType const* type) {
    auto structType = dyncast<StructType const*>(type);
    if (!structType || structType->hasTrivialLifetime()) {
        return nullptr;
    }
    return structType;
}

void sema::popTopLevelDtor(ast::Expression* expr, DTorStack& dtors) {
    if (!expr || !expr->isDecorated()) {
        return;
    }
    auto* structType = dyncast<StructType const*>(expr->type().get());
    if (!structType) {
        return;
    }
    using enum SpecialLifetimeFunction;
    if (structType->specialLifetimeFunction(Destructor)) {
        dtors.pop();
    }
}
