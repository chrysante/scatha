#ifndef SCATHA_SEMA_SEMANTICISSUES_H_
#define SCATHA_SEMA_SEMANTICISSUES_H_

#include <iosfwd>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>
#include <scatha/Sema/Fwd.h>

/// # Hierarchy of semantic issue classes
/// These closely reflect the hierarchy of AST nodes
///
/// ```
/// SemanticIssue
/// ├─ BadStatement
/// │  ├─ BadDeclaration
/// │  │  ├─ Redefinition
/// │  │  ├─ DeclInvalidInScope
/// │  │  ├─ BadVarDecl
/// │  │  ├─ BadParamDecl
/// │  │  ├─ BadFuncDef
/// │  │  ├─ BadStructDef
/// │  ├─ BadCompoundStatement ??
/// │  ├─ BadExpressionStatement ??
/// │  └─ BadControlFlowStatement
/// │     ├─ BadReturnStatement
/// │     ├─ BadIfStatement
/// │     ├─ BadLoopStatement
/// │     └─ BadJumpStatement
/// └─ BadExpression
///    ├─ BadIdentifier
///    ├─ BadLiteral ??
///    ├─ BadUnaryExpression
///    ├─ BadBinaryExpression
///    ├─ BadMemberAccess
///    ├─ BadConditional
///    ├─ BadFunctionCall
///    ├─ BadConstructorCall
///    ├─ BadSubscript
///    ├─ BadPointerReference
///    ├─ BadPointerDereference
///    └─ BadTypeConversion
/// ```

namespace scatha::sema {

/// Base class of all semantic issues
class SCATHA_API SemaIssue: public Issue {
public:
    SemaIssue(Scope const* scope,
              SourceRange sourceRange,
              IssueSeverity severity):
        Issue(sourceRange, severity), _scope(scope) {}

    /// \Returns the scope in which the issue occured
    Scope const* scope() const { return _scope; }

private:
    Scope const* _scope;
};

/// Base class of all statement related issues
class SCATHA_API BadStatementNEW: public SemaIssue {
public:
    /// \Returns the erroneous statement
    ast::Statement const* statement() const { return stmt; }

protected:
    explicit BadStatementNEW(ast::Statement const* statement,
                             Scope const* scope,
                             IssueSeverity severity);

private:
    ast::Statement const* stmt;
};

/// Base class of all declaration related issues
class SCATHA_API BadDecl: public BadStatementNEW {
protected:
    explicit BadDecl(ast::Declaration const* declaration,
                     Scope const* scope,
                     IssueSeverity severity);

    /// \Returns the erroneous declaration
    ast::Declaration const* declaration() const;
};

/// Declaration of a name that is already defined in the same scope
class SCATHA_API Redefinition: public BadDecl {
public:
    Redefinition(ast::Declaration const* declaration,
                 ast::Declaration const* prevDeclaration,
                 Scope const* scope):
        BadDecl(declaration, scope, IssueSeverity::Error),
        prev(prevDeclaration) {}

    /// \Returns the previous declaration of the same name
    ast::Declaration const* previousDeclaration() const { return prev; }

private:
    void format(std::ostream& str) const override;

    ast::Declaration const* prev;
};

///
class SCATHA_API DeclInvalidInScope: public BadDecl {
public:
    DeclInvalidInScope(ast::Declaration const* declaration, Scope const* scope):
        BadDecl(declaration, scope, IssueSeverity::Error) {}

private:
    void format(std::ostream& str) const override;
};

///
class SCATHA_API BadVarDecl: public BadDecl {
public:
    enum Reason {
#define SC_SEMA_BADVARDECL_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemanticIssuesNEW.def>
    };

    BadVarDecl(ast::VariableDeclaration const* vardecl,
               Scope const* scope,
               Reason reason,
               Type const* type = nullptr,
               ast::Expression const* initExpr = nullptr);

    ///
    Reason reason() const { return _reason; }

    ///
    Type const* type() const { return _type; }

    ///
    ast::Expression const* initExpr() const { return _initExpr; }

private:
    void format(std::ostream& str) const override;

    Reason _reason;
    Type const* _type;
    ast::Expression const* _initExpr;
};

///
class SCATHA_API BadParamDecl: public BadDecl {
public:
    BadParamDecl();

private:
};

///
class SCATHA_API BadFuncDef: public BadDecl {
public:
    enum Reason {
#define SC_SEMA_BADFUNCDEF_DEF(reason, _0, _1) reason,
#include <scatha/Sema/SemanticIssuesNEW.def>
    };

    BadFuncDef(ast::FunctionDefinition const* funcdef,
               Scope const* scope,
               Reason reason);

    ///
    Reason reason() const { return _reason; }

private:
    void format(std::ostream& str) const override;

    Reason _reason;
};

///
class SCATHA_API BadStructDefinition: public BadDecl {
public:
    BadStructDefinition();

private:
};

/// Base class of all statement related issues
class SCATHA_API BadExpr: public SemaIssue {
public:
    /// \Returns the erroneous expression
    ast::Expression const* expression() const { return expr; }

protected:
    explicit BadExpr(ast::Expression const* expression,
                     Scope const* scope,
                     IssueSeverity severity);

private:
    ast::Expression const* expr;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SEMANTICISSUES_H_
