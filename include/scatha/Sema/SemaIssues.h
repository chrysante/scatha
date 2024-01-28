#ifndef SCATHA_SEMA_SEMAISSUES_H_
#define SCATHA_SEMA_SEMAISSUES_H_

#include <iosfwd>
#include <span>
#include <unordered_map>
#include <vector>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/QualType.h>

/// # Hierarchy of semantic issue classes
/// This closely reflects the hierarchy of AST nodes
///
/// ```
/// SemaIssue
/// ├─ BadStmt
/// │  ├─ GenericBadStmt
/// │  ├─ BadDecl
/// │  │  ├─ Redefinition
/// │  │  ├─ BadVarDecl
/// │  │  ├─ BadFuncDef
/// │  │  │  └─ BadSMF
/// │  │  └─ StructDefCycle
/// │  ├─ BadReturnStmt
/// │  └─ BadReturnTypeDeduction
/// ├─ BadExpr
/// │  ├─ BadSymRef
/// │  ├─ BadTypeConv
/// │  ├─ BadValueCatConv
/// │  └─ BadMutConv
/// ├─ BadPassedType
/// ├─ BadCleanup
/// └─ ORError
/// ```

namespace scatha::sema {

#define SC_SEMA_ISSUE_REASON()                                                 \
private:                                                                       \
    Reason _reason{};                                                          \
                                                                               \
public:                                                                        \
    Reason reason() const { return _reason; }

#define SC_SEMA_DERIVED_STMT(Type, Name)                                       \
    template <typename T = ast::Type>                                          \
    T const* Name() const {                                                    \
        return cast<T const*>(BadStmt::statement());                           \
    }

/// Base class of all semantic issues
class SCATHA_API SemaIssue: public Issue {
public:
    explicit SemaIssue(IssueSeverity severity = IssueSeverity::Error):
        SemaIssue(nullptr, {}, severity) {}

    SemaIssue(Scope const* scope, SourceRange sourceRange,
              IssueSeverity severity):
        Issue(sourceRange, severity), _scope(scope) {}

    /// \Returns the scope in which the issue occured
    Scope const* scope() const { return _scope; }

    void setScope(Scope const* scope) { _scope = scope; }

private:
    void format(std::ostream&) const override {}

    Scope const* _scope = nullptr;
};

/// Base class of all statement related issues
class SCATHA_API BadStmt: public SemaIssue {
public:
    /// \Returns the erroneous statement
    ast::Statement const* statement() const { return stmt; }

protected:
    explicit BadStmt(Scope const* scope, ast::Statement const* statement,
                     IssueSeverity severity);

private:
    ast::Statement const* stmt;
};

///
class SCATHA_API GenericBadStmt: public BadStmt {
public:
    enum Reason {
#define SC_SEMA_GENERICBADSTMT_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemaIssues.def.h>
    };
    SC_SEMA_ISSUE_REASON()

    GenericBadStmt(Scope const* scope, ast::Statement const* statement,
                   Reason reason);

private:
    void format(std::ostream& str) const override;
};

/// Invalid `import` statement
class SCATHA_API BadImport: public BadStmt {
public:
    enum Reason {
        LibraryNotFound,
        InvalidExpression,
        UnscopedForeignLibImport
    };
    SC_SEMA_ISSUE_REASON()

    /// Construct from AST node with reason
    BadImport(Scope const* scope, ast::ASTNode const* node, Reason reason);

    /// Construct from library name
    BadImport(Scope const* scope, std::string name);

    SC_SEMA_DERIVED_STMT(ImportStatement, importStmt)

private:
    std::string name;

    void format(std::ostream& str) const override;
};

/// Base class of all declaration related issues
class SCATHA_API BadDecl: public BadStmt {
protected:
    explicit BadDecl(Scope const* scope, ast::Declaration const* declaration,
                     IssueSeverity severity);

    SC_SEMA_DERIVED_STMT(Declaration, declaration)
};

/// Declaration of a name that is already defined in the same scope
class SCATHA_API Redefinition: public BadDecl {
public:
    Redefinition(Scope const* scope, ast::Declaration const* declaration,
                 Entity const* existing);

    /// \Returns the previous declaration of the same name
    Entity const* existing() const { return _existing; }

private:
    Entity const* _existing;
};

/// Bad declaration of variable or function parameter
class SCATHA_API BadVarDecl: public BadDecl {
public:
    enum Reason {
#define SC_SEMA_BADVARDECL_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemaIssues.def.h>
    };
    SC_SEMA_ISSUE_REASON()

    BadVarDecl(Scope const* scope, ast::VarDeclBase const* vardecl,
               Reason reason, Type const* type = nullptr,
               ast::Expression const* initExpr = nullptr);

    SC_SEMA_DERIVED_STMT(VarDeclBase, declaration)

    ///
    Type const* type() const { return _type; }

    ///
    ast::Expression const* initExpr() const { return _initExpr; }

private:
    Type const* _type;
    ast::Expression const* _initExpr;
};

/// Invalid function signature. This includes invalid definitions of `main()`
/// and invalid special member function signatures via the base class `BadSMF`
class SCATHA_API BadFuncDef: public BadDecl {
public:
    enum Reason {
#define SC_SEMA_BADFUNCDEF_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemaIssues.def.h>
    };
    SC_SEMA_ISSUE_REASON()

    BadFuncDef(Scope const* scope, ast::FunctionDefinition const* funcdef,
               Reason reason);

    SC_SEMA_DERIVED_STMT(FunctionDefinition, definition)

    /// \Returns the name of the defined function
    std::string_view name() const;

protected:
    struct InitAsBase {};
    BadFuncDef(InitAsBase, Scope const* scope,
               ast::FunctionDefinition const* funcdef, IssueSeverity severity,
               Reason reason = {});

private:
    void format(std::ostream& str) const override;
};

/// Invalid signature of special member function
class SCATHA_API BadSMF: public BadFuncDef {
public:
    enum Reason {
#define SC_SEMA_BADSMF_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemaIssues.def.h>
    };
    SC_SEMA_ISSUE_REASON()

    BadSMF(Scope const* scope, ast::FunctionDefinition const* funcdef,
           Reason reason, StructType const* parent);

    StructType const* parent() const { return _parent; }

private:
    void format(std::ostream& str) const override;

    StructType const* _parent;
};

///
class SCATHA_API BadAccessControl: public BadDecl {
public:
    enum Reason { TooWeakForParent, TooWeakForType };
    SC_SEMA_ISSUE_REASON()

    explicit BadAccessControl(Scope const* scope, Entity const* entity,
                              Reason reason);
};

///
class SCATHA_API BadReturnStmt: public BadStmt {
public:
    enum Reason {
#define SC_SEMA_BADRETURN_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemaIssues.def.h>
    };
    SC_SEMA_ISSUE_REASON()

    SC_SEMA_DERIVED_STMT(ReturnStatement, statement)

    BadReturnStmt(Scope const* scope, ast::ReturnStatement const* statement,
                  Reason reason);

private:
    void format(std::ostream& str) const override;
};

///
class SCATHA_API BadReturnTypeDeduction: public BadStmt {
public:
    SC_SEMA_DERIVED_STMT(ReturnStatement, statement)

    BadReturnTypeDeduction(Scope const* scope,
                           ast::ReturnStatement const* statement,
                           ast::ReturnStatement const* conflictingReturn);

    /// The conflicting return statement if this error is a return type
    /// deduction error
    ast::ReturnStatement const* conflicting() const { return confl; }

private:
    ast::ReturnStatement const* confl = nullptr;
};

/// Error due to cyclic struct definition
class SCATHA_API StructDefCycle: public BadDecl {
public:
    StructDefCycle(Scope const* scope, std::vector<Entity const*> cycle);

    /// The definition cycle
    std::span<Entity const* const> cycle() const { return _cycle; }

private:
    std::vector<Entity const*> _cycle;
};

/// Base class of all statement related issues
class SCATHA_API BadExpr: public SemaIssue {
public:
    enum Reason {
#define SC_SEMA_BADEXPR_DEF(Type, Reason, Severity, Message) Reason,
#include <scatha/Sema/SemaIssues.def.h>
    };
    SC_SEMA_ISSUE_REASON()

    explicit BadExpr(Scope const* scope, ast::ASTNode const* expr,
                     Reason reason);

    /// \Returns the erroneous AST node
    /// This is the same as `expr()` except that it is not necessarily an
    /// expression
    ast::ASTNode const* astNode() const { return _node; }

    /// \Returns the erroneous expression
    ast::Expression const* expr() const;

    ///
    void setExpr(ast::ASTNode const* expr);

protected:
    explicit BadExpr(Scope const* scope, ast::ASTNode const* expr,
                     IssueSeverity severity);

private:
    void format(std::ostream&) const override;

    ast::ASTNode const* _node;
};

///
class SCATHA_API BadSymRef: public BadExpr {
public:
    explicit BadSymRef(Scope const* scope, ast::Expression const* expr,
                       EntityCategory expected);

    EntityCategory have() const;

    EntityCategory expected() const;

private:
    void format(std::ostream&) const override;

    EntityCategory _expected;
};

///
class SCATHA_API BadTypeConv: public BadExpr {
public:
    BadTypeConv(Scope const* scope, ast::ASTNode const* expr, Type const* to):
        BadExpr(scope, expr, IssueSeverity::Error), _to(to) {}

    Type const* to() const { return _to; }

private:
    void format(std::ostream&) const override;

    Type const* _to;
};

///
class SCATHA_API BadValueCatConv: public BadExpr {
public:
    BadValueCatConv(Scope const* scope, ast::ASTNode const* expr,
                    ValueCategory to):
        BadExpr(scope, expr, IssueSeverity::Error), _to(to) {}

    ValueCategory to() const { return _to; }

private:
    void format(std::ostream&) const override;

    ValueCategory _to;
};

///
class SCATHA_API BadMutConv: public BadExpr {
public:
    BadMutConv(Scope const* scope, ast::ASTNode const* expr, Mutability to):
        BadExpr(scope, expr, IssueSeverity::Error), _to(to) {}

    Mutability to() const { return _to; }

private:
    void format(std::ostream&) const override;

    Mutability _to;
};

/// Error data associated with each function in an overload set describing why
/// that function is not a match
struct ORMatchError {
    enum Reason { CountMismatch, NoArgumentConversion };

    /// The reason this overload is not callable
    Reason reason;

    /// The problematic argument index
    size_t argIndex = ~size_t{ 0 };
};

/// Error class for using invalid types as function paramaters or return types
class SCATHA_API BadPassedType: public SemaIssue {
public:
    enum Reason { Argument, Return, ReturnDeduced };

    explicit BadPassedType(Scope const* scope, ast::Expression const* expr,
                           Reason reason);

    Reason reason() const { return r; }

private:
    Reason r;
};

/// Error class issued when a cleanup of a type with deleted destructor is
/// requested
class SCATHA_API BadCleanup: public SemaIssue {
public:
    BadCleanup(ast::ASTNode const* node, Object const* object);
};

/// Overload resolution error
class SCATHA_API ORError: public SemaIssue {
public:
    enum Reason { NoMatch, Ambiguous };

    /// Static constructors @{
    static ORError makeNoMatch(
        ast::Expression const* expr, std::span<Function const* const> os,
        std::vector<std::pair<QualType, ValueCategory>> argTypes,
        std::unordered_map<Function const*, ORMatchError> matchErrors) {
        return ORError(expr, std::move(os), std::move(argTypes),
                       /* matches = */ {}, std::move(matchErrors));
    }

    static ORError makeAmbiguous(
        ast::Expression const* expr, std::span<Function const* const> os,
        std::vector<std::pair<QualType, ValueCategory>> argTypes,
        std::vector<Function const*> matches) {
        return ORError(expr, std::move(os), std::move(argTypes),
                       std::move(matches), {});
    }
    /// @}

    Reason reason() const { return matches.empty() ? NoMatch : Ambiguous; }

private:
    explicit ORError(
        ast::Expression const* expr, std::span<Function const* const> os,
        std::vector<std::pair<QualType, ValueCategory>> argTypes,
        std::vector<Function const*> matches,
        std::unordered_map<Function const*, ORMatchError> matchErrors);

    /// List of all functions in the overload set
    std::vector<Function const*> overloadSet;

    /// List of types of the call argument types
    std::vector<std::pair<QualType, ValueCategory>> argTypes;

    /// Possible call targets. Only non-empty if `reason() == Ambiguous`
    std::vector<Function const*> matches;

    /// Maps functions in the overload set the match errors describing why the
    /// function could not be called
    std::unordered_map<Function const*, ORMatchError> matchErrors;
};

} // namespace scatha::sema

#undef SC_SEMA_ISSUE_REASON
#undef SC_SEMA_DERIVED_STMT

#endif // SCATHA_SEMA_SEMAISSUES_H_
