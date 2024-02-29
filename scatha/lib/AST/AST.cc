#include "AST/AST.h"

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

void ast::privateDelete(ASTNode* node) {
    visit(*node, [](auto& derived) { delete &derived; });
}

SourceRange ASTNode::sourceRange() const {
    SourceRange result = directSourceRange();
    for (auto* child: children()) {
        if (child && !isa<CompoundStatement>(child)) {
            result = merge(result, child->sourceRange());
        }
    }
    return result;
}

UniquePtr<ASTNode> ASTNode::extractFromParent() {
    return parent()->extractChild(indexInParent());
}

ASTNode* ASTNode::replaceChild(ASTNode const* old, UniquePtr<ASTNode> repl) {
    SC_ASSERT(old->parent() == this, "old is not a child of this");
    SC_ASSERT(!repl->parent(), "repl already has a parent");
    size_t index = old->indexInParent();
    repl->_parent = this;
    auto* result = repl.get();
    _children[index] = std::move(repl);
    return result;
}

ASTNode* ASTNode::replace(UniquePtr<ASTNode> repl) {
    return parent()->replaceChild(this, std::move(repl));
}

size_t ASTNode::indexOf(ASTNode const* child) const {
    auto itr = ranges::find_if(_children, [&](auto& otherChild) {
        return otherChild.get() == child;
    });
    return utl::narrow_cast<size_t>(itr - _children.begin());
}

sema::Object const* Expression::object() const {
    return cast<sema::Object const*>(entity());
}

sema::EntityCategory Expression::entityCategory() const {
    expectDecorated();
    if (!entity()) {
        return sema::EntityCategory::Indeterminate;
    }
    return entity()->category();
}

void Expression::decorateValue(sema::Entity* entity,
                               sema::ValueCategory valueCategory,
                               sema::QualType type) {
    SC_ASSERT(entity, "Must not be null");
    _entity = entity;
    _valueCat = valueCategory;
    _type = type;
    auto* object = dyncast<sema::Object*>(entity);
    if (!type && object) {
        _type = object->getQualType();
    }
    markDecorated();
}

void Expression::decorateType(sema::Entity* type) {
    _entity = type;
    markDecorated();
}

void Expression::decorateNamespace(sema::Entity* entity) {
    _entity = entity;
    markDecorated();
}

void FunctionCall::decorateCall(sema::Object* object,
                                sema::ValueCategory valueCategory,
                                sema::QualType type,
                                sema::Function* calledFunction) {
    decorateValue(object, valueCategory, type);
    _function = calledFunction;
}

NontrivConstructExpr::NontrivConstructExpr(
    utl::small_vector<UniquePtr<Expression>> arguments, SourceRange sourceRange,
    sema::StructType const* constructedType):
    ConstructBase(NodeType::NontrivConstructExpr, nullptr, std::move(arguments),
                  sourceRange, constructedType) {}

sema::StructType const* NontrivConstructExpr::constructedType() const {
    return cast<sema::StructType const*>(ConstructBase::constructedType());
}

void NontrivConstructExpr::decorateConstruct(
    sema::Object* obj, sema::Function const* constructor) {
    ctor = constructor;
    decorateValue(obj, sema::ValueCategory::RValue);
}

NontrivAggrConstructExpr::NontrivAggrConstructExpr(
    utl::small_vector<UniquePtr<Expression>> arguments, SourceRange sourceRange,
    sema::StructType const* constructedType):
    ConstructBase(NodeType::NontrivAggrConstructExpr, nullptr,
                  std::move(arguments), sourceRange, constructedType) {}

sema::StructType const* NontrivAggrConstructExpr::constructedType() const {
    return cast<sema::StructType const*>(ConstructBase::constructedType());
}

DynArrayConstructExpr::DynArrayConstructExpr(
    utl::small_vector<UniquePtr<Expression>> arguments, SourceRange sourceRange,
    sema::ArrayType const* constructedType):
    ConstructBase(NodeType::DynArrayConstructExpr, nullptr,
                  std::move(arguments), sourceRange, constructedType) {}

sema::ArrayType const* DynArrayConstructExpr::constructedType() const {
    return cast<sema::ArrayType const*>(ConstructBase::constructedType());
}

void VarDeclBase::decorateVarDecl(sema::Entity* entity) {
    if (auto* object = dyncast<sema::Object*>(entity)) {
        _type = object->type();
    }
    Declaration::decorateDecl(entity);
}

sema::Variable const* VarDeclBase::variable() const {
    return cast<sema::Variable const*>(entity());
}

sema::Object const* VarDeclBase::object() const {
    return cast<sema::Object const*>(entity());
}

sema::Function const* FunctionDefinition::function() const {
    return cast<sema::Function const*>(entity());
}

sema::StructType const* StructDefinition::structType() const {
    return cast<sema::StructType const*>(entity());
}

sema::Library const* ImportStatement::library() const {
    return cast<sema::Library const*>(entity());
}

void CompoundStatement::decorateScope(sema::Scope* scope) {
    decorateStmt(scope);
}

sema::Scope const* CompoundStatement::scope() const {
    return cast<sema::Scope const*>(entity());
}
