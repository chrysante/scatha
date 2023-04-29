#ifndef SCATHA_SEMA_EXPRESSIONANALYSIS_H_
#define SCATHA_SEMA_EXPRESSIONANALYSIS_H_

#include "AST/AST.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

class ExpressionAnalysisResult;

/// \param expression The expression to be analyzed.
/// \param symbolTable The symbol table to be read from and written to.
/// \param issueHandler The issue handler to submit issues to. May be null.
/// \returns An `ExpressionAnalysisResult`.
///
ExpressionAnalysisResult analyzeExpression(ast::Expression& expression,
                                           SymbolTable& symbolTable,
                                           IssueHandler& issueHandler);

class ExpressionAnalysisResult {
public:
    /// # Construction

    static ExpressionAnalysisResult lvalue(Entity* entity,
                                           QualType const* type) {
        return { EntityCategory::Value, entity, type };
    }

    static ExpressionAnalysisResult rvalue(QualType const* type) {
        return { EntityCategory::Value, nullptr, type };
    }

    static ExpressionAnalysisResult indeterminate() {
        return { EntityCategory::Indeterminate, nullptr, nullptr };
    }

    template <typename E = Entity>
    static ExpressionAnalysisResult type(QualType const* type) {
        return { EntityCategory::Type,
                 static_cast<E*>(const_cast<QualType*>(type)),
                 nullptr };
    }

    static ExpressionAnalysisResult fail() { return { false }; }

    /// # Non static members

    bool isLValue() const { return entity() != nullptr; }

    bool isRValue() const { return !isLValue(); }

    explicit operator bool() const { return success(); }

    /// Wether the analysis was successfull.
    bool success() const { return _success; }

    /// The entity category the analyzed expression falls into.
    EntityCategory category() const { return _category; }

    /// The entity of the expression if it is an lvalue.  Otherwise `nullptr`.
    Entity* entity() const { return _entity; }

    /// Type of the analyzed expression if it refers to a type. Otherwise the
    /// type of the value that expression yields.
    QualType const* type() const { return _type; }

private:
    bool _success = false;
    EntityCategory _category{};
    Entity* _entity       = nullptr;
    QualType const* _type = nullptr;

private:
    ExpressionAnalysisResult(EntityCategory category,
                             Entity* entity,
                             QualType const* type):
        _success(true), _category(category), _entity(entity), _type(type) {}

    ExpressionAnalysisResult(bool success): _success(success) {}
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_EXPRESSIONANALYSIS_H_
