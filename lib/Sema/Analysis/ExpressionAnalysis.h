#ifndef SCATHA_SEMA_EXPRESSIONANALYSIS_H_
#define SCATHA_SEMA_EXPRESSIONANALYSIS_H_

#include "AST/AST.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

class ExpressionAnalysisResult {
public:
    bool isLValue() const { return !!symbolID(); }
    bool isRValue() const { return !isLValue(); }

    explicit operator bool() const { return success(); }

    /// Wether the analysis was successfull.
    bool success() const { return _success; }

    /// The entity category the analyzed expression falls into.
    ast::EntityCategory category() const { return _category; }

    /// The SymbolID of the expression if it is an lvalue.  Otherwise invalid.
    SymbolID symbolID() const { return _symbolID; }

    /// TypeID of the analyzed expression if it refers to a type. Otherwise the
    /// type of the value that expression yields.
    TypeID typeID() const { return _typeID; }

private:
    bool _success;
    ast::EntityCategory _category{};
    SymbolID _symbolID;
    TypeID _typeID;

public:
    /// TODO: Make these private somehow
    static constexpr ExpressionAnalysisResult lvalue(SymbolID symbolID, TypeID typeID) {
        return { ast::EntityCategory::Value, symbolID, typeID };
    }
    static constexpr ExpressionAnalysisResult rvalue(TypeID typeID) { return { ast::EntityCategory::Value, typeID }; }
    static constexpr ExpressionAnalysisResult type(TypeID id) { return { ast::EntityCategory::Type, id }; }
    static constexpr ExpressionAnalysisResult type(SymbolID id) { return type(TypeID(id)); }
    static constexpr ExpressionAnalysisResult fail() { return { false }; }

private:
    constexpr ExpressionAnalysisResult(ast::EntityCategory category, SymbolID symbolID, TypeID typeID):
        _success(true), _category(category), _symbolID(symbolID), _typeID(typeID) {}
    constexpr ExpressionAnalysisResult(ast::EntityCategory kind, TypeID typeID):
        ExpressionAnalysisResult(kind, SymbolID{}, typeID) {}
    constexpr ExpressionAnalysisResult(bool success): _success(success) {}
};


/// \param expression The expression to be analyzed.
/// \param symbolTable The symbol table to be read from and written to.
/// \param issueHandler The issue handler to submit issues to. May be null.
/// \returns An \p ExpressionAnalysisResult.
///
ExpressionAnalysisResult analyzeExpression(ast::Expression& expression,
                                           SymbolTable& symbolTable,
                                           issue::IssueHandler* issueHandler);

} // namespace scatha::sema

#endif // SCATHA_SEMA_EXPRESSIONANALYSIS_H_
